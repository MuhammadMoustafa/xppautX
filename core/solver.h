#ifndef XPP_SOLVER_H
#define XPP_SOLVER_H
/* The integration methods (W51): one xpp::Solver per method, found in an
   explicit registry table (solver.cpp) by the number the model's METHOD
   option, the numerics menu and .set files use. A Solver owns its work
   memory (made when it starts, freed with it), and every method's advance
   returns one Result<> (an xpp::Result, xpp_error.h) in place of its
   own kflag dialect. C++ only.
   AUTO's own integrators are not here. */

#include <memory>
#include <span>
#include <string>
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

/* what a method is: how Integrate drives it, what the numerics menu asks */
struct SolverTraits {
  /* the time counts iterations of a map (dt is +-1) */
  bool discrete = false;
  /* advances a number of steps of dt; otherwise it chooses its own steps
     on to an output time (its NOUT is 1) */
  bool fixed_step = false;
  /* asks Tolerance, minimum and maximum step */
  bool step_tolerance = false;
  /* asks the relative and absolute tolerances */
  bool rel_abs_tolerance = false;
  /* asks the Newton tolerance and iterations of its implicit step */
  bool newton = false;
  /* asks whether the system is banded */
  bool banded = false;
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
  virtual Result<> advance(const SolverStep &step) = 0;
  /* an integration that went through has ended */
  virtual void finish() {}

protected:
  /* the Session it steps: the run takes it once, where it starts (W47d4) */
  Session &session_;

  /* a failed advance of this method: at the command that ran it (in a
     script, its step: xpp_ui.h command_place), or at place (a model line) */
  Result<> failed(std::string what) const;
  Result<> failed(std::string what, Place place) const { return fail(info_.name, std::move(what), std::move(place)); }

private:
  const SolverInfo &info_;
};

/* the registry, in method order */
std::span<const SolverInfo> solvers();
/* a method's row (Runge-Kutta's for a number that names none) */
const SolverInfo &solver_info(int method);


/* starts the solver of s's numerics.method: a fresh one, with fresh work
   memory, the Session's (integrate.h's IntegratorState) */
void start_solver(Session &s);

} // namespace xpp

#endif
