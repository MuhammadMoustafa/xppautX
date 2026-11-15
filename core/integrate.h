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

struct Session; /* session.h */

/* What a run reports to the command that started it, each as data, once its result exists: the caller shows it (freezes the curve, takes
   the movie's frame, draws; the integrators know no window). A member may be empty (a run its caller shows nothing of). `label` names
   the run ("p=0.3  i=4": a sweep's, empty for a plain run). An error from `stored` stops a sweep and is its failure, but a Range
   Equilibria step's, which goes on and tells it once the sweep has ended. */
struct RunReport {
  /* the run's rows are stored (a Range Equilibria step's equilibrium found), before the data table follows */
  std::function<Result<>(std::string_view label)> stored;
  /* the run is complete, the data table follows it */
  std::function<void(std::string_view label)> done;
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
Result<> validate_steady_state(SteadyStateSettings settings, double dt, double start);
Result<> run_to_steady_state(Session &s, SteadyStateSettings settings, const RunReport &report);

/* a global flag's event stops the integration (flags.cpp) */
void send_halt(Session &s, int flag);

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
DenseEvent dense_event(Session &s, double from, double until, double step_from, double step_until,
                      const std::function<void(double,std::span<double>)> &state,
                      std::span<double> scratch, std::span<double> derivative, bool sections=true);

/* the equilibrium Newton finds from the initial data, with its
   eigenvalues (x, re, im per variable), written to name (nothing when
   Newton does not converge); shoot: also the invariant manifolds of a
   saddle, integrated into UMk.dat/SMk.dat (--silent's --equil 0/1, the
   protocol's `equilibrium` `write`) */
Result<> write_equilibrium(Session &s, const char *name, int shoot, bool confirm, const RunReport &report);
void init_range(Session &s);
/* Continue (W213), the one way to extend the trajectory: to the time `until`, or for `extra` more,
   either rounded up to the output grid (Dt times Store every N steps) and appending rows at that
   stride; the end reached is the state's time */
Result<bool> continue_to(Session &s, double until, const RunReport &report);
Result<bool> continue_for(Session &s, double extra, const RunReport &report);
/* ... for another Total (Run duration): the key c, a user button's `c`, a `continue` with no value */
Result<bool> continue_total(Session &s, const RunReport &report);
int range_item(Session &s);
int range_item2(Session &s);
/* the type and index of what s.integrator->range names to vary (and the second item when `two`), or the error of one that is
   neither a parameter nor a variable */
Result<> run_allowed(const Session &s, std::string_view command);
Result<> resolve_range_items(Session &s, bool two);
Result<> resolve_range_items(const Session &s, RangeVars &range, bool two);
int set_up_range(Session &s);
int set_up_range2(Session &s);
void init_monte_carlo(Session &s);

void swap_color(Session &s, Color *col, int rorw);
/* the color of the window's first curve is newcolor, the one it had given back in *old (the equilibria's
   manifolds are shot in their own colors with the curve's pen) */
void change_current_color(Session &s, const Color &newcolor, Color *old);
void set_cycle(Session &s, int flag, int *icol);
int do_range(Session &s, double *x, int flag, const RunReport &report);
Result<int> equilibrium_range(Session &s, const RunReport &report);
int write_this_run(Session &s, const char *file, int i);
void run_now(Session &s, const RunReport &report);

/* the runs, the one core each front end's command calls (docs/protocol.md "run"). Every one returns whether
   the run ended before its end without failing (an event's arret, a section that stops it, '/'; the
   reason is shown once), or the error it failed with, which its command shows. */
/* where a run starts from (XPPAUT's Initialconds items): the initial data (Go), the stored initial data again
   (Old), the last state (Last), the last state with the clock going on (Shift), the initial data backward in
   time (Backward) */
enum class RunFrom { initial, old, last, shift, backward };
/* reset_integrals: a run from the last state of an integral-equation method starts the integrals afresh (else they go on) */
Result<bool> run_from(Session &s, RunFrom from, const RunReport &report, bool reset_integrals = false);
/* the states a graph plots against each other (0-based); the pair a click sets */
struct Plane {
  int horizontal, vertical;
};
/* the plane of graph window `graph` (its index), or why it has none */
Result<Plane> window_plane(const Session &s, int graph);
/* a run from the initial data with the plane's two states set to a click's plot coordinates, no data kept of the
   run before */
Result<bool> run_from_click(Session &s, Plane plane, double horizontal, double vertical, const RunReport &report);
/* one run from each click in turn, the first emptying the store, the rest added to it (suppress_bounds: Mice's
   runs ignore the state limit); the runs made (fewer when cancelled) */
Result<int> run_from_clicks(Session &s, Plane plane, std::span<const std::pair<double, double>> clicks, bool suppress_bounds, const RunReport &report);
/* a run from manifold `index` (1-based) of the equilibria's shooting */
Result<bool> run_from_manifold(Session &s, int index, const RunReport &report);
/* what a range sweep did: the integrations it completed and the steps (rows) they stored */
struct RangeProgress {
  int runs = 0;
  long steps = 0;
};
/* the sweep over the settings in s.integrator->range (flag 0: one parameter, 1: two), what it completed in `done`
   when given; the sweep's end code (-1 when a run stopped early) or the error it failed with */
Result<int> run_range(Session &s, int flag, RangeProgress *done, const RunReport &report);
/* the integration of x as the Session's settings say, after a run's start made: the one every run ends in */
Result<bool> integrate_from(Session &s, double *x, const RunReport &report);
void do_start_flags(Session &s, double *x, double *t);
int usual_integrate_stuff(Session &s, double *x, const RunReport &report); /* integrate_from, its error shown: 1 when it failed */
void evaluate_ar_ic(Session &s, const char *v, const char *f, int j1, int j2);
void arr_ic_start(Session &s);
/* the initial conditions by formula (initial_conditions.set_by_formula): `variable` a state, or an array range name[j1..j2] whose
   formula reads [j] as the element's number (an array's formula is kept like the model's own); the states set (indices),
   or why none was, nothing changed */
Result<std::vector<int>> set_ic_formula(Session &s, std::string_view variable, std::string_view formula);
/* `size` states from `name` on set to the formula, its t the element's offset from the first (initial_conditions.set_by_array_formula) */
Result<std::vector<int>> set_ic_array(Session &s, std::string_view name, int size, std::string_view formula);
/* Node-only states preserve Markov initial data; full states replace both.
   Any other length is a programming error, rejected before writing. */
void set_initial_data(Session &s, std::span<const double> state);
void get_ic(Session &s, int it, double *x);
void send_output(Session &s, double *y, double t);
Result<> stor_full(Session &s);
int do_autox_range_go(Session &s, const RunReport &report);

/* The most steps one axis of a sweep takes (Range, Range Equilibria and Range Shoot alike): steps+1 runs fit an int. */
inline constexpr int MAX_RANGE_STEPS = std::numeric_limits<int>::max() - 1;
/* The most runs a Range sweep makes: it holds one int seed per run before the first run, since runs on worker threads
   read them out of order (W683; 2^24 runs are 64 MB of seeds, where the int range's 2^31 were 8 GB). Past the int range
   a run's number and indices would overflow too (W682). */
inline constexpr std::int64_t MAX_RANGE_RUNS = std::int64_t{1} << 24;
/* A sweep's run count for steps and steps2 (steps2 0 for a single range), refused before the sweep starts when it is past
   MAX_RANGE_RUNS, naming the steps and the setting they came from (W682). */
Result<std::int64_t> sweep_run_count(const Session &s, int steps, int steps2);





/* integrates x from *t over tend in steps of dt, storing the points
   (count 0: none) and plotting every store_every-th: 1 when it stopped early
   ('/', the step size under Hmin, a range's quiet failure of the
   method), else 0; or why it failed (a variable NaN or out of
   bounds, the method's own failure, a delay or DAE step), for the
   command that ran it to show (W63b) */
Result<int> integrate(Session &s, double *t, double *x, double tend, double dt, int count, int store_every, int *start);
/* shooting's integration over the whole interval (drawn as it runs when
   ishow, where a failure only ends the curve), or the method's failure */
Result<> ode_int(Session &s, double *y, double *t, int *istart, int ishow);
/* the recorded failure, cleared, so the next run starts clean (and the
   DAE solve afresh) */
std::unexpected<Error> take_step_error(Session &s);
/* the curve from x (shoot: from xg along evec, the side sgn), bounds
   unchecked, or the error it failed with */
Result<> shoot(Session &s, double *x, double *xg, double *evec, int sgn);
Result<> shoot_easy(Session &s, double *x);
/* An invariant set's branch starts at the saddle, at this time, whether it is
   drawn or exported (so the two are the same branch), and is stored whole from
   it whatever store_from says (W533). */
inline constexpr double BRANCH_START_TIME = 0.0;
/* BranchStoreFrom sets store_from to BRANCH_START_TIME for the branch's run and
   puts the run's value back when it ends, on error too (W533). */
class BranchStoreFrom {
public:
  explicit BranchStoreFrom(Session &s);
  ~BranchStoreFrom();
  BranchStoreFrom(const BranchStoreFrom &) = delete;
  BranchStoreFrom &operator=(const BranchStoreFrom &) = delete;
private:
  Session &s_;
  double before_;
};
/* Monte Carlo's search for equilibria from random guesses (with their
   manifolds when ishoot), or the first manifold that failed */
Result<> do_monte_carlo_search(Session &s, int append, int stuffbrowse,int ishoot);

} // namespace xpp
