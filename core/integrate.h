#pragma once

#include <algorithm>
#include <functional>
#include <array>
#include <cstdio>
#include <memory>
#include <limits>
#include <span>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "solver.h"
#include "model.h"

namespace xpp {

struct Session; /* session.h */
struct RangeVars;

/* Decimal places, not significant digits. Double has digits10 reliable
   decimal digits; asking for more would imply unsupported precision. */
inline constexpr int MAX_STEADY_DECIMALS = std::numeric_limits<double>::digits10;
inline constexpr int DEFAULT_STEADY_DECIMALS = 9; /* User's requested initial precision. */
inline constexpr double DEFAULT_STEADY_HOLD = 1; /* One model-time unit of unchanged digits. */
/* The integrator's iteration counters are int; a run of more steps than
   this (less one for its rounding) cannot be counted. */
inline constexpr double MAX_RUN_STEPS = std::numeric_limits<int>::max() - 1;
/* How far above a whole number of Dt a direct `continue until` may lie, as
   a fraction of Dt, and still count as on the grid (the rounding noise of
   subtracting two times); any more rounds up to the next grid point. */
inline constexpr double CONTINUE_GRID_TOLERANCE = 1e-6;
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
Result<> validate_steady_state(SteadyStateSettings settings, double dt, double start);
Result<> run_to_steady_state(Session &s, SteadyStateSettings settings);

/* a line of initial data x[j1..j2](0)=formula (the parser's, which
   search_array hands it) kept in m's array initial values */
int extract_ic_data(Model &m, char *big);
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
Result<> write_equilibrium(Session &s, const char *name, int shoot, bool confirm);
void init_range(Session &s);
int set_up_eq_range(Session &s);
/* Continue (W213), the one way to extend the trajectory: to the time `until`, or for `extra` more,
   either rounded up to the output grid (Dt times Store every N steps) and appending rows at that
   stride; the end reached is the state's time */
Result<bool> continue_to(Session &s, double until);
Result<bool> continue_for(Session &s, double extra);
/* ... for another Total (Run duration): the key c, a user button's `c`, a `continue` with no value */
Result<bool> continue_total(Session &s);
int range_item(Session &s);
int range_item2(Session &s);
/* the type and index of what s.integrator.range names to vary (and the second item when `two`), or the error of one that is
   neither a parameter nor a variable */
Result<> run_allowed(const Session &s, std::string_view command);
Result<> resolve_range_items(Session &s, bool two);
Result<> resolve_range_items(const Session &s, RangeVars &range, bool two);
int set_up_range(Session &s);
int set_up_range2(Session &s);
void init_monte_carlo(Session &s);
void monte_carlo(Session &s);

void swap_color(Session &s, Color *col, int rorw);
void set_cycle(Session &s, int flag, int *icol);
int do_range(Session &s, double *x, int flag);
void find_equilib_com(Session &s, int com);
Result<int> equilibrium_range(Session &s);
int write_this_run(Session &s, const char *file, int i);
/* XPPAUT's Initialconds items as an imported button sequence runs them (kbs.h), asking what the registry's
   run.*, ic.* commands take as arguments */
void do_init_data(Session &s, int com);
void run_now(Session &s);

/* the runs, the one core each front end's command calls (docs/protocol.md "run"). Every one returns whether
   the run ended before its end without failing (an event's arret, a section that stops it, '/'; the
   reason is shown once), or the error it failed with, which its command shows. */
/* where a run starts from (XPPAUT's Initialconds items): the initial data (Go), the stored initial data again
   (Old), the last state (Last), the last state with the clock going on (Shift), the initial data backward in
   time (Backward) */
enum class RunFrom { initial, old, last, shift, backward };
/* reset_integrals: a run from the last state of an integral-equation method starts the integrals afresh (else they go on) */
Result<bool> run_from(Session &s, RunFrom from, bool reset_integrals = false);
/* the states a graph plots against each other (0-based); the pair a click sets */
struct Plane {
  int horizontal, vertical;
};
/* the plane of graph window `graph` (its index), or why it has none */
Result<Plane> window_plane(const Session &s, int graph);
/* a run from the initial data with the plane's two states set to a click's plot coordinates, no data kept of the
   run before */
Result<bool> run_from_click(Session &s, Plane plane, double horizontal, double vertical);
/* one run from each click in turn, the first emptying the store, the rest added to it (suppress_bounds: Mice's
   runs ignore the state limit); the runs made (fewer when cancelled) */
Result<int> run_from_clicks(Session &s, Plane plane, std::span<const std::pair<double, double>> clicks, bool suppress_bounds);
/* a run from manifold `index` (1-based) of the equilibria's shooting */
Result<bool> run_from_manifold(Session &s, int index);
/* what a range sweep did: the integrations it completed and the steps (rows) they stored */
struct RangeProgress {
  int runs = 0;
  long steps = 0;
};
/* the sweep over the settings in s.integrator.range (flag 0: one parameter, 1: two), what it completed in `done`
   when given; the sweep's end code (-1 when a run stopped early) or the error it failed with */
Result<int> run_range(Session &s, int flag, RangeProgress *done);
/* the integration of x as the Session's settings say, after a run's start made: the one every run ends in */
Result<bool> integrate_from(Session &s, double *x);
void do_start_flags(Session &s, double *x, double *t);
int usual_integrate_stuff(Session &s, double *x); /* integrate_from, its error shown: 1 when it failed */
void evaluate_ar_ic(Session &s, const char *v, const char *f, int j1, int j2);
void arr_ic_start(Session &s);
/* the initial conditions by formula (ic.formula): `variable` a state, or an array range name[j1..j2] whose
   formula reads [j] as the element's number (an array's formula is kept like the model's own); the states set (indices),
   or why none was, nothing changed */
Result<std::vector<int>> set_ic_formula(Session &s, std::string_view variable, std::string_view formula);
/* `size` states from `name` on set to the formula, its t the element's offset from the first (ic.array) */
Result<std::vector<int>> set_ic_array(Session &s, std::string_view name, int size, std::string_view formula);
int set_array_ic(Session &s);
int form_ic(Session &s);
/* Node-only states preserve Markov initial data; full states replace both.
   Any other length is a programming error, rejected before writing. */
void set_initial_data(Session &s, std::span<const double> state);
void get_ic(Session &s, int it, double *x);
void send_output(Session &s, double *y, double t);
void do_plot(Session &s, float *oldxpl, float *oldypl, float *oldzpl, float *xpl, float *ypl, float *zpl);
void plot_the_graphs(Session &s, float *xv, float *xvold, int node, int neq, double ddt, int *tc,int flag);
void plot_one_graph(Session &s, float *xv, float *xvold, int node, int neq, double ddt, int *tc);
void restore(Session &s, int i1, int i2);
void comp_color(Session &s, float *v1, float *v2, int n, float dt);
Result<> stor_full(Session &s);
int do_auto_range_go(Session &s);

/* the model's array initial values (x[j1..j2](0)=formula, applied by
   arr_ic_start when set_all_vals sets the model up), element by element:
   each variable's name and formula with the index worked out, the index
   j, and which of the model's array initial values it is (group, from 1) */
struct ArrayInitialValue {
  std::string var, formula;
  int j = 0, group = 0;
};
std::vector<ArrayInitialValue> array_initial_values(const Model &m);

/* Initialconds/Range's settings (integrate.cpp's, read by load_eqn.cpp's
   options): the one range over item (and item2 for the double range) */
/* how many values a range varies (RangeVars::rtype): one, two together or two independently */
inline constexpr int RANGE_SINGLE = 0, RANGE_TOGETHER = 1, RANGE_INDEPENDENT = 2;
struct RangeVars {
  std::string item, item2;
  bool write_files = false; // Opt in: reset runs otherwise keep no separate files.
  int steps = 20, steps2 = 10; /* the option's default; a double range's second steps, 10 as XPPAUT's dialog fell back to */
  bool reset, oldic; /* each run starts from the initial data / from the same initial data */
  int index, index2, cycle, type, type2, movie;
  double plow, phigh, plow2, phigh2;
  int rtype;
};

/* the right-hand side the solvers step, rhs(t, y, ydot, neq): a function
   of the Session it is bound to (the load binds my_rhs to its Session), so
   that a solver reaches the Model and Session through the IntegratorState
   that holds it, never through a global (W47d4) */
struct RightHandSide {
  int (*function)(Session &s, double t, double *y, double *ydot, int neq) = nullptr;
  Session *session = nullptr;
  int operator()(double t, double *y, double *ydot, int neq) const
  {
    return function(*session,t,y,ydot,neq);
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
/* Sing pts' Range settings */
struct EquilibriumRange {
  std::string item;
  int steps = 10, shoot = 0, col = 0, movie = 0, mc = 0; /* Steps starts at 10, what the dialog's empty value fell back to */
  double plow = 0, phigh = 0;
};

/* each global flag's state during a run (xpp::Model has its definition):
   the condition's value at the step before and this one, where in the
   step it crossed (tstar, 0..1), whether it did (hit, the pass it did in)
   and the events' values (flags.cpp) */
struct FlagState {
  double f0 = 0.0, f1 = 0.0;
  double tstar = 0.0;
  std::array<double, Model::max_events> vrhs{};
  int hit = 0;
};

/* the integrator's state, a Session's (session.h) */
struct IntegratorState {
  /* Present only during an explicitly requested unchanged-digits run;
     its final result is session-local, cleared by the next ordinary run. */
  std::optional<SteadyStateMonitor> steady;
  std::optional<SteadyStateResult> steady_result;
  /* the method's solver, with its work memory (xpp::start_solver) */
  std::unique_ptr<Solver> solver;
  /* the right-hand side the solvers step: my_rhs of this Session */
  RightHandSide rhs;
  /* Initialconds/Range's settings */
  RangeVars range{};
  /* a range integration is running (pp_shoot's shooting reads it) */
  int range_flag = 0;
  /* the integration starts afresh (the solvers' istart) */
  int my_start = 0;
  /* --noout: a batch run writes no output file */
  int suppress_out = 0;
  /* the bounds check is off */
  int suppress_bounds = 0;
  /* a step's delay (stop_integration) or DAE solve (do_daes) failed:
     why; integrate() ends and returns it */
  std::optional<Error> step_error;
  /* -makeplot: a batch run writes its plot too */
  int make_plot_flag = 0;
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
     (odesol2.cpp rosen) */
  double rosen_htry = 0;
  /* the global flags' states (flags.cpp) */
  std::vector<FlagState> flags; /* the model's flags (compile_flags) */
  /* Gear's pivots of the Jacobian it factored last (gear.cpp ggear) */
  std::vector<int> gear_pivot;
  /* the array initial values in use */
  ArrayIcs array_ics;
  /* Sing pts' Monte Carlo search and Range */
  FixedPointList fixptlist;
  FixedPointGuess fixptguess;
  EquilibriumRange eq_range;
};

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
/* a step or an evaluation that cannot go on (a delay out of range, a DAE
   solve, user functions nested past MAX_UFUN_DEPTH): records why (the
   first); the integration ends after the step and returns it, and what
   no caller takes the command shows when it ends (ui_json.cpp) */
void stop_integration(Session &s, Error why);
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
