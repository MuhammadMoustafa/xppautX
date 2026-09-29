#ifndef XPP_SOLVER_H
#define XPP_SOLVER_H
/* The integration methods (W51): one xpp::Solver per method, found in an
   explicit registry table (solver.cpp) by the number the model's METHOD
   option, the numerics menu and .set files use. A Solver owns its work
   memory (made when it starts, freed with it), and every method's advance
   returns one SolverResult in place of its own kflag dialect. C++ only.
   AUTO's own integrators are not here. */

#include <memory>
#include <span>
#include <string>
#include <vector>

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

/* how an advance went */
struct SolverResult {
  bool ok = true;
  std::string error; /* why it failed, for the user (empty: nothing to say) */
};

class Solver;

/* a row of the registry */
struct SolverInfo {
  method::Id id;
  const char *name;      /* do_info's and the front end's */
  const char *set_label; /* the label .set files write beside it */
  SolverTraits traits;
  /* the solver, with its work memory for n equations */
  std::unique_ptr<Solver> (*start)(const SolverInfo &info, int n);
};

class Solver {
public:
  explicit Solver(const SolverInfo &info) : info_(info) {}
  virtual ~Solver() = default;
  Solver(const Solver &) = delete;
  Solver &operator=(const Solver &) = delete;

  const SolverInfo &info() const { return info_; }
  const SolverTraits &traits() const { return info_.traits; }

  /* an integration begins from *start (1: afresh); a method whose own
     convention differs translates it */
  virtual void begin([[maybe_unused]] int *start) {}
  virtual SolverResult advance(const SolverStep &step) = 0;
  /* an integration that went through has ended */
  virtual void finish() {}

private:
  const SolverInfo &info_;
};

/* the registry, in method order */
std::span<const SolverInfo> solvers();
/* a method's row (Runge-Kutta's for a number that names none) */
const SolverInfo &solver_info(int method);

/* a failed advance: the bell, then its error unless quiet */
void report_solver_failure(const SolverResult &r, bool quiet);

/* starts the solver of numerics.method: a fresh one, with fresh work
   memory, the Session's (integrate.h's IntegratorState) */
void start_solver();

} // namespace xpp

#endif
