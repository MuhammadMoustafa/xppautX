#pragma once
/* The runs a command makes of the integrator, and what is made of each (the data table, the sweeps, the equilibria
   of Monte Carlo): one run from the initial data, from the last state or from a click, Continue, Run to steady
   state, Range's sweeps, Range Equilibria and Monte Carlo. integrate.h's integrate() integrates one run and reports
   through RunReport; these start it, take its rows into the data table and say what came of it (runs.cpp). */
#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "integrate.h"
#include "xpp_error.h"

namespace xpp {

struct Session; /* session.h */

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
/* the sweep over the settings in s.integrator->range (one parameter or two), what it completed in `done`
   when given; the sweep's end code (-1 when a run stopped early) or the error it failed with */
Result<int> run_range(Session &s, RangeProgress *done, const RunReport &report);

/* the equilibrium Newton finds from the initial data, with its
   eigenvalues (x, re, im per variable), written to name (nothing when
   Newton does not converge); shoot: also the invariant manifolds of a
   saddle, integrated into UMk.dat/SMk.dat (--silent's --equil 0/1, the
   protocol's `equilibrium` `write`) */
Result<> write_equilibrium(Session &s, const char *name, int shoot, bool confirm, const RunReport &report);

/* Continue (W213), the one way to extend the trajectory: to the time `until`, or for `extra` more,
   either rounded up to the output grid (Dt times Store every N steps) and appending rows at that
   stride; the end reached is the state's time */
Result<bool> continue_to(Session &s, double until, const RunReport &report);
Result<bool> continue_for(Session &s, double extra, const RunReport &report);
/* ... for another Total (Run duration): the key c, a user button's `c`, a `continue` with no value */
Result<bool> continue_total(Session &s, const RunReport &report);

Result<> run_allowed(const Session &s, std::string_view command);

/* the integration of x as the Session's settings say, after a run's start made: the one every run ends in */
Result<bool> integrate_from(Session &s, double *x, const RunReport &report);
int usual_integrate_stuff(Session &s, double *x, const RunReport &report); /* integrate_from, its error shown: 1 when it failed */

/* Monte Carlo's search for equilibria from random guesses (with their
   manifolds when ishoot), or the first manifold that failed */
Result<> do_monte_carlo_search(Session &s, int append, int stuffbrowse,int ishoot);

Result<> run_to_steady_state(Session &s, SteadyStateSettings settings, const RunReport &report);

/* what a sweep over autoX's diagram gives in place of the Range settings: the parameter it varies, how many points
   the diagram has (the sweep makes one run at each, the first after the parameter's value at point 0) and what to do
   as each run begins (autoX marks the point). autoX makes it; the sweep, which is below autoX, only calls it. */
struct DiagramRange {
  std::string parameter;
  int points;
  std::function<void(int point)> mark;
};
/* the sweep of Range's settings (`two`: the second parameter too) as the command's Initialconds > Range does, or over
   autoX's diagram when `diagram` is given: the sweep's end code (-1 when a run stopped early) */
int do_range(Session &s, double *x, bool two, const RunReport &report, const DiagramRange *diagram = nullptr);
Result<int> equilibrium_range(Session &s, const RunReport &report);
int write_this_run(Session &s, const char *file, int i);
void run_now(Session &s, const RunReport &report);

/* the adjoint of one period of the orbit through x, saved and shown as Range's adjoint mode asks */
void compute_one_period(Session &s, double period, double *x, const char *name, const RunReport &report);

} // namespace xpp
