#pragma once
/* The integrator's state a Session holds (integrate.cpp, flags.cpp, gear.cpp, ...): plain data the integrators work on (W744). What
   the option table sets of it (the Range sweeps' settings) is model_integrator_settings.h's, its base (W757). The methods of
   SteadyStateMonitor are integrate.cpp's. */
#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include "model_integrator_settings.h"
#include "model_parts.h"
#include "xpp_math.h"

namespace xpp {

class Solver; /* solver.h */
struct RunParts; /* run_parts.h */

struct SteadyStateSettings {
  int decimals;
  double hold, maximum;
};
struct SteadyStateResult {
  std::string status;
  int decimals;
  double time = 0;
};
/* A state value's digits at the requested decimals, text in a fixed buffer
   (no allocation per comparison). 309 integer digits of the largest double,
   the point, MAX_STEADY_DECIMALS decimals and a sign fit in 330. */
struct SteadyDigits {
  std::array<char, 330> text;
  unsigned short size = 0;
  bool operator==(const SteadyDigits &o) const { return std::equal(text.begin(), text.begin() + size, o.text.begin(), o.text.begin() + o.size); }
};
/* Compares full core states formatted to the requested fixed decimal
   places at every configured Dt interval, never the float plot samples. */
class SteadyStateMonitor {
 public:
  explicit SteadyStateMonitor(SteadyStateSettings settings): settings_(settings) {}
  void begin(std::span<const double> state, double time);
  bool observe(std::span<const double> state, double time);
 private:
  SteadyStateSettings settings_;
  std::vector<SteadyDigits> previous_, next_; /* next_ is kept to reuse its storage */
  double previous_time_ = 0, unchanged_ = 0;
};

/* the right-hand side the solvers step, rhs(t, y, ydot, neq): a function
   of the parts it is bound to (the load binds my_rhs to its Session's), so
   that a solver reaches the Model and the run's state through the IntegratorState
   that holds it, never through a global (W47d4) */
struct RightHandSide {
  int (*function)(RunParts &s, double t, double *y, double *ydot, int neq) = nullptr;
  RunParts *parts = nullptr;
  int operator()(double t, double *y, double *ydot, int neq) const
  {
    return function(*parts,t,y,ydot,neq);
  }
};

/* the fixed points the Monte Carlo search found (at most MAXFP): each
   one's values and its eigenvalues' real and imaginary parts, NODE of
   each; flag 1 once their storage is made */
constexpr int MAXFP = 400;
struct FixedPointList {
  int n = 0, flag = 0;
  std::vector<std::vector<double>> x, er, em; /* MAXFP each, once flag is 1 */
};

/* the Monte Carlo search's settings: the number of guesses, the tolerance
   two points are one within, and each variable's range */
struct FixedPointGuess {
  int n = 0;
  double tol = 0;
  std::vector<double> xlo, xhi; /* NODE each (init_monte_carlo) */
};

/* each global flag's state during a run (xpp::Model has its definition):
   the condition's value at the step before and this one, where in the
   step it crossed (tstar, 0..1), whether it did (hit, the pass it did in)
   and the events' values (flags.cpp) */
struct FlagState {
  double f0 = 0.0, f1 = 0.0;
  double tstar = 0.0;
  std::array<double, max_events> vrhs{};
  int hit = 0;
};

/* the integrator's state, a Session's (session.h), beside the settings the option table sets (Range, Sing pts' Range) */
struct IntegratorState : IntegratorSettings {
  /* Present only during an explicitly requested unchanged-digits run;
     its final result is session-local, cleared by the next ordinary run. */
  std::optional<SteadyStateMonitor> steady;
  std::optional<SteadyStateResult> steady_result;
  /* the method's solver, with its work memory (xpp::start_solver) */
  std::unique_ptr<Solver> solver;
  /* the right-hand side the solvers step: my_rhs of this Session */
  RightHandSide rhs;
  /* a range integration is running (pp_shoot's shooting reads it) */
  int range_flag = 0;
  /* this Session is a sweep's worker (sweep_runs.h): its integration runs on a thread of its own and calls the
     front end for nothing (no progress, no plot, no poll), the main thread does */
  bool on_worker = false;
  /* the integration starts afresh (the solvers' istart) */
  int my_start = 0;
  /* --noout: a batch run writes no output file */
  int suppress_out = 0;
  /* the bounds check is off */
  int suppress_bounds = 0;
  /* where the last integration stopped */
  double last_time = 0;
  /* the adjoint is computed over a range (adj2.cpp) */
  int adj_range = 0;
  /* a global flag's event asked the integration to stop (send_halt): the
     flag that did, or -1 when the stop came from elsewhere (a cancel) */
  int stop_flag = 0;
  int halt_flag = -1;
  /* why the last integrate() ended before its end without failing (a state
     at state_limit inside a sweep or a drawn flow, an event's arret, a
     section crossing that stops the run); empty when it ran to its end */
  std::string stop_note;
  /* the step Rosenbrock's last advance ended with, its next one's first
     (odesol2.cpp rosenbrock_steps) */
  double rosen_htry = 0;
  /* the global flags' states (flags.cpp) */
  std::vector<FlagState> flags; /* the model's flags (compile_flags) */
  /* the LU of the Jacobian Gear factored last, by independent subsystem (gear.cpp gear_step) */
  xpp::BlockLu gear_lu;
  /* the array initial values in use */
  ArrayIcs array_ics;
  /* Sing pts' Monte Carlo search and Range */
  FixedPointList fixptlist;
  FixedPointGuess fixptguess;
};

} // namespace xpp
