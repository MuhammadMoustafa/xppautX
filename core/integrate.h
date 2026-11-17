#pragma once

#include <algorithm>
#include "xpp_math.h"
#include <functional>
#include <array>
#include <cstdint>
#include <memory>
#include <limits>
#include <span>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <expected>
#include <string_view>

#include "solver.h"
#include "integrator_state.h"
#include "model_parts.h"
#include "xpp_error.h"
namespace xpp { struct Color; struct Model; }

namespace xpp {

struct RunParts; /* run_parts.h */

/* What a run reports to the command that started it, each as data, once its result exists: the caller shows it (freezes the curve, takes
   the movie's frame, draws; the integrators know no window). A member may be empty (a run its caller shows nothing of). `label` names
   the run ("p=0.3  i=4": a sweep's, empty for a plain run). An error from `stored` stops a sweep and is its failure, but a Range
   Equilibria step's, which goes on and tells it once the sweep has ended. */
struct RunReport {
  /* the run's rows are stored (a Range Equilibria step's equilibrium found), before the data table follows */
  std::function<Result<>(std::string_view label)> stored;
  /* the run is complete, the data table follows it */
  std::function<void(std::string_view label)> done;
  /* a run from the last state is about to overwrite the initial conditions: the caller keeps an undo point of them */
  std::function<void()> overwriting_initial_data;
  /* a sweep's run wrote its data file (the run's number, from 0): its picture goes with it */
  std::function<void(int run)> written;
  /* a sweep: the report of the single run it makes inside a run (the period's orbit of Range's adjoint mode) */
  std::shared_ptr<const RunReport> orbit;
};

/* Decimal places, not significant digits. Double has digits10 reliable
   decimal digits; asking for more would imply unsupported precision. */
inline constexpr int MAX_STEADY_DECIMALS = std::numeric_limits<double>::digits10;
inline constexpr int DEFAULT_STEADY_DECIMALS = 9; /* User's requested initial precision. */
inline constexpr double DEFAULT_STEADY_HOLD = 1; /* One model-time unit of unchanged digits. */
/* How far above a whole number of Dt a direct `continue until` may lie, as
   a fraction of Dt, and still count as on the grid (the rounding noise of
   subtracting two times); any more rounds up to the next grid point. */
inline constexpr double CONTINUE_GRID_TOLERANCE = 1e-6;
Result<> validate_steady_state(SteadyStateSettings options, double dt, double start);

/* a global flag's event stops the integration (flags.cpp) */
void send_halt(RunParts &s, int flag);

/* The integrator's driver works on the Session s the command (or the run)
   that started it passes down (W47d4). */

/* A dense accepted-step event, consumed before the solver returns a sample.
   Bracketing uses 32 subintervals of each inspected interval; sign crossings
   are refined to 8*epsilon*max(1,abs(time)). Tangencies without a sampled
   zero and crossings faster than this subdivision are not guaranteed. */
inline constexpr int DENSE_EVENT_SUBDIVISIONS=32;
struct DenseEvent {
  double time=0;
  bool found=false, section=false;
  int bound_variable=-1, flag_count=0;
  std::vector<int> flags; /* the global flags that fire, flag_count of them */
};
DenseEvent dense_event(RunParts &s, double from, double until, double step_from, double step_until,
                      const std::function<void(double,std::span<double>)> &state,
                      std::span<double> scratch, std::span<double> derivative, bool sections=true);

void init_range(RunParts &s);
int range_item(RunParts &s);
int range_item2(RunParts &s);
/* the type and index of what s.integrator->range names to vary (and the second item when `two`), or the error of one that is
   neither a parameter nor a variable */
Result<> resolve_range_items(RunParts &s, bool two);
Result<> resolve_range_items(const RunParts &s, RangeVars &range, bool two);
int set_up_range(RunParts &s);
int set_up_range2(RunParts &s);
void init_monte_carlo(RunParts &s);

void swap_color(RunParts &s, Color *col, int rorw);
/* the color of the window's first curve is newcolor, the one it had given back in *old (the equilibria's
   manifolds are shot in their own colors with the curve's pen) */
void change_current_color(RunParts &s, const Color &newcolor, Color *old);
void set_cycle(RunParts &s, int flag, int *icol);

/* Node-only states preserve Markov initial data; full states replace both.
   Any other length is a programming error, rejected before writing. */
void set_initial_data(RunParts &s, std::span<const double> state);
void get_ic(RunParts &s, int it, double *x);
/* the global flags' first step: the state before a run's first row */
void do_start_flags(RunParts &s, double *x, double *t);
/* a row was just stored: a replayed script may stop the job here, and a front end may show the run growing */
void row_stored(RunParts &s);
void send_output(RunParts &s, double *y, double t);
Result<> stor_full(RunParts &s);

/* The most steps one axis of a sweep takes (Range, Range Equilibria and Range Shoot alike): steps+1 runs fit an int. */
inline constexpr int MAX_RANGE_STEPS = std::numeric_limits<int>::max() - 1;
/* The most runs a Range sweep makes: it holds one int seed per run before the first run, since runs on worker threads
   read them out of order (W683; 2^24 runs are 64 MB of seeds, where the int range's 2^31 were 8 GB). Past the int range
   a run's number and indices would overflow too (W682). */
inline constexpr std::int64_t MAX_RANGE_RUNS = std::int64_t{1} << 24;
/* A sweep's run count for steps and steps2 (steps2 0 for a single range), refused before the sweep starts when it is past
   MAX_RANGE_RUNS, naming the steps and the option they came from (W682). */
Result<std::int64_t> sweep_run_count(const RunParts &s, int steps, int steps2);





/* integrates x from *t over tend in steps of dt, storing the points
   (count 0: none) and plotting every store_every-th: 1 when it stopped early
   ('/', the step size under Hmin, a range's quiet failure of the
   method), else 0; or why it failed (a variable NaN or out of
   bounds, the method's own failure, a delay or DAE step), for the
   command that ran it to show (W63b) */
Result<int> integrate(RunParts &s, double *t, double *x, double tend, double dt, int count, int store_every, int *start);
/* shooting's integration over the whole interval (drawn as it runs when
   ishow, where a failure only ends the curve), or the method's failure */
Result<> ode_int(RunParts &s, double *y, double *t, int *istart, int ishow);
/* the recorded failure, cleared, so the next run starts clean (and the
   DAE solve afresh) */
std::unexpected<Error> take_step_error(RunParts &s);
/* the curve from x (shoot: from xg along evec, the side sgn), bounds
   unchecked, or the error it failed with */
Result<> shoot(RunParts &s, double *x, double *xg, double *evec, int sgn);
Result<> shoot_easy(RunParts &s, double *x);
/* An invariant set's branch starts at the saddle, at this time, whether it is
   drawn or exported (so the two are the same branch), and is stored whole from
   it whatever store_from says (W533). */
inline constexpr double BRANCH_START_TIME = 0.0;
/* BranchStoreFrom sets store_from to BRANCH_START_TIME for the branch's run and
   puts the run's value back when it ends, on error too (W533). */
class BranchStoreFrom {
public:
  explicit BranchStoreFrom(RunParts &s);
  ~BranchStoreFrom();
  BranchStoreFrom(const BranchStoreFrom &) = delete;
  BranchStoreFrom &operator=(const BranchStoreFrom &) = delete;
private:
  RunParts &s_;
  double before_;
};

} // namespace xpp
