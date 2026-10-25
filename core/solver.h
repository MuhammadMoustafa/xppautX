#pragma once
/* The integration methods (W51): one xpp::Solver per method, found in an
   explicit registry table (solver.cpp) by the number the model's METHOD
   option, the numerics menu and .set files use. A Solver owns its work
   memory (made when it starts, freed with it), and every method's advance
   returns one Result<> (an xpp::Result, xpp_error.h) in place of its
   own kflag dialect. C++ only.
   AUTO's own integrators are not here. */

#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "xpp_error.h"

namespace xpp {

/* the methods by number (numerics.method) */
namespace method {
enum Id : int {
  DISCRETE, EULER, MOD_EULER, RK4, ADAMS, GEAR, VOLTERRA, BACKEUL,
  RKQS, STIFF, CVODE, DP5, DP83, RB23, SYMPLECT,
  COUNT
};
}

/* what an implicit fixed step (backward Euler's, Volterra's) returns when
   its Newton iteration fails: the matrix is singular, or the iterations
   ran out (SolverWork::Stop says where) */
inline constexpr int NEWTON_SINGULAR = -1, NEWTON_NO_CONVERGENCE = -2;

/* what a method is: how Integrate drives it, what the numerics menu asks */
struct SolverTraits {
  /* the time counts iterations of a map (dt is +-1) */
  bool discrete = false;
  /* advances a number of steps of dt; otherwise it chooses its own steps
     on to an output time (its store_every is 1) */
  bool fixed_step = false;
  /* the settings its steps read, which the numerics menu asks and the
     Numerics form shows (W381 questions 5 and 8): relative_tolerance,
     absolute_tolerance, min_step, max_step */
  bool relative_tolerance = false;
  bool absolute_tolerance = false;
  bool min_step = false;
  bool max_step = false;
  /* asks the Newton tolerance and iterations of its implicit step */
  bool newton = false;
  /* asks whether the system is banded */
  bool banded = false;
  /* keeps integral-equation history and asks its maximum stored points */
  bool integral_history = false;
  /* pairs coordinates and momenta, requiring an even dimension */
  bool paired_dimension = false;
  /* uses the eighth-order Dormand-Prince step rather than the fifth */
  bool eighth_order = false;
  /* retains an accepted-step interpolant across requested output times */
  bool dense_output = false;
  /* uses the stiff adaptive step rather than adaptive Runge-Kutta */
  bool stiff_step = false;
  /* draws Wiener noise once per step */
  bool wiener = false;
};

/* one advance: from *t, either `steps` steps of `dt` (a fixed-step method)
   or on to `tout` from the step size guess *hguess, which it updates */
struct SolverStep {
  double *y = nullptr;
  double *t = nullptr;
  int neq = 0;
  int *start = nullptr; /* 1 afresh, else continuing (the solver updates it) */
  double dt = 0;
  int steps = 0;
  double tout = 0;
  double *hguess = nullptr;
  /* The trajectory output loop consumes sections; fitting and shooting need
     the requested endpoint. Flags and halts apply in either context. */
  bool sections = false;
};

/* how an advance went: on a failure, the method's name and why it
   failed, for the user (an empty `what`: nothing to say, the routine
   said it already) */

class Solver;
struct Session; /* session.h */

/* a row of the registry */
struct SolverInfo {
  method::Id id;
  const char *name;      /* do_info's and the front end's */
  const char *set_label; /* the label .set files write beside it */
  const char *key;       /* the one word a .odex writes (docs/odex.md "Options") */
  SolverTraits traits;
  /* the solver of the Session s, with its work memory for n equations */
  std::unique_ptr<Solver> (*start)(const SolverInfo &info, Session &s, int n);
};

class Solver {
public:
  Solver(const SolverInfo &info, Session &s) : session_(s), info_(info) {}
  virtual ~Solver() = default;
  Solver(const Solver &) = delete;
  Solver &operator=(const Solver &) = delete;

  const SolverInfo &info() const { return info_; }
  const SolverTraits &traits() const { return info_.traits; }

  /* an integration begins from *start (1: afresh); a method whose own
     convention differs translates it */
  virtual void begin([[maybe_unused]] int *start) {}
  /* one advance of the method; an exact Jacobian that failed on the way
     (jacobian.h JacobianFailed) is its error */
  Result<> advance(const SolverStep &step);
  /* an integration that went through has ended */
  virtual void finish() {}
  virtual bool section_found() const { return false; }

protected:
  /* the Session it steps: the run takes it once, where it starts (W47d4) */
  Session &session_;

  /* a failed advance of this method: at the command that ran it (in a
     script, its step: xpp_ui.h command_place), or at place (a model line) */
  Result<> failed(std::string what) const;
  Result<> failed(std::string what, Place place) const { return fail(info_.name, std::move(what), std::move(place)); }
  /* An advance that stopped at time t (W480, "every stop says why"):
     "<method>: <what> at t = <t>", then the variable it stopped in
     (", in X"; from 0, -1 when none is known), the settings that caused it
     with their values (" (min_step = 1e-12)", the option table's keys) and
     detail, on a line of its own. It is at the variable's model line, else
     at the line the model sets the first of the settings on, else at the
     command that ran the method. */
  Result<> stopped(std::string_view what, double t, int variable, std::initializer_list<std::string_view> settings,
                   std::string_view detail = {}) const;

private:
  /* the method's own advance */
  virtual Result<> step_by(const SolverStep &step) = 0;
  const SolverInfo &info_;
};

/* the registry, in method order */
std::span<const SolverInfo> solvers();
struct Model;
/* The one place a method is chosen (W132): pick_method takes a name (a
   method's name or set label, XPPAUT's one-letter @ meth key, a spelling
   shipped models use), check_method a method's number (a menu, a .set);
   both refuse what the model cannot run (Symplectic an odd dimension,
   Volterra no kernels, kernels anything but Volterra) at place. */
Result<method::Id> pick_method(const Model &model, std::string_view text, Place place);
Result<method::Id> check_method(const Model &model, int id, Place place);
/* the method whose key (the word our files and the protocol use: runge_kutta,
   cvode, ...) is exactly key, refused as check_method refuses */
Result<method::Id> method_by_key(const Model &model, std::string_view key, Place place);
/* a method's row: callers must supply a checked id */
const SolverInfo &solver_info(int method);


/* A run by method info with output interval dt over length, refused before
   it starts when its step limits leave no step to take (W381 question 8,
   W480 I1): for a method that reads min_step, min_step above max_step (when
   it reads that too), above |dt| or above the run's |length|. */
Result<> check_step_limits(const Session &s, const SolverInfo &info, double dt, double length);

/* A run from start over length (both in the direction of dt) that stores
   rows, refused before it starts when store_from lies beyond its end in
   that direction: nothing would be stored (W381 decision 4), unless an event
   writes rows (out_put). A store_from
   before the start is fine (every row is stored). */
Result<> check_store_from(const Session &s, double start, double dt, double length);

/* whether a row at time t is stored: t has passed store_from in the run's
   direction (forward t >= store_from, backward t <= store_from) */
inline bool stored_from(double store_from, double t, double dt) { return dt<0 ? t<=store_from : t>=store_from; }

/* starts the solver of s's numerics.method: a fresh one, with fresh work
   memory, the Session's (integrate.h's IntegratorState) */
void start_solver(Session &s);

} // namespace xpp

