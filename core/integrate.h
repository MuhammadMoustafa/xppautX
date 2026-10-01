#ifndef _integrate_h_
#define _integrate_h_

#include <array>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "solver.h"
#include "xpplim.h"
#include "model.h"

namespace xpp {

struct Session; /* session.h */

/* a line of initial data x[j1..j2](0)=formula (the parser's, which
   search_array hands it) kept in m's array initial values */
int extract_ic_data(Model &m, char *big);
/* a global flag's event stops the integration (flags.cpp) */
void send_halt(Session &s);

/* The integrator's driver works on the Session s the command (or the run)
   that started it passes down (W47d4). */

/* the equilibrium Newton finds from the initial data, with its
   eigenvalues (x, re, im per variable), written to name (nothing when
   Newton does not converge); shoot: also the invariant manifolds of a
   saddle, integrated into UMk.dat/SMk.dat (-silent's -equil 0/1, the
   protocol's `equilibrium` `write`) */
void write_equilibrium(Session &s, const char *name, int shoot);
void dump_range(Session &s, FILE *fp, int f);
void init_range(Session &s);
int set_up_eq_range(Session &s);
void cont_integ(Session &s);
int range_item(Session &s);
int range_item2(Session &s);
int set_up_range(Session &s);
int set_up_range2(Session &s);
void init_monte_carlo(Session &s);
void monte_carlo(Session &s);
void do_eq_range(Session &s, double *x);
void swap_color(Session &s, int *col, int rorw);
void set_cycle(Session &s, int flag, int *icol);
int do_range(Session &s, double *x, int flag);
void find_equilib_com(Session &s, int com);
int write_this_run(Session &s, const char *file, int i);
void do_init_data(Session &s, int com);
void run_now(Session &s);
void do_start_flags(Session &s, double *x, double *t);
int usual_integrate_stuff(Session &s, double *x); /* the run: integrate()'s result, 1 when it failed (the error shown) */
void do_new_array_ic(Session &s, const char *newic, int j1, int j2);
void evaluate_ar_ic(Session &s, const char *v, const char *f, int j1, int j2);
void arr_ic_start(Session &s);
int set_array_ic(Session &s);
int form_ic(Session &s);
void get_ic(Session &s, int it, double *x);
void send_output(Session &s, double *y, double t);
void do_plot(Session &s, float *oldxpl, float *oldypl, float *oldzpl, float *xpl, float *ypl, float *zpl);
void plot_the_graphs(Session &s, float *xv, float *xvold, int node, int neq, double ddt, int *tc,int flag);
void plot_one_graph(Session &s, float *xv, float *xvold, int node, int neq, double ddt, int *tc);
void restore(Session &s, int i1, int i2);
void comp_color(Session &s, float *v1, float *v2, int n, float dt);
int stor_full(Session &s);
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
struct RangeVars {
  std::string item, item2;
  int steps, steps2, reset, oldic, index, index2, cycle, type, type2, movie;
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
  int steps = 0, shoot = 0, col = 0, movie = 0, mc = 0;
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
  /* -noout: a batch run writes no output file */
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
  /* a global flag's event asked the integration to stop (send_halt) */
  int stop_flag = 0;
  /* the step Rosenbrock's last advance ended with, its next one's first
     (odesol2.cpp rosen) */
  double rosen_htry = 0;
  /* the step Dormand-Prince's last advance ended with, its next one's
     first guess (dormpri.cpp dormprin) */
  double dp_hout = 0;
  /* the global flags' states (flags.cpp) */
  std::vector<FlagState> flags; /* the model's nflags (compile_flags) */
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
   (count 0: none) and plotting every nout-th: 1 when it stopped early
   ('/', the step size under Hmin, a range's quiet failure of the
   method), else 0; or why it failed (a variable NaN or out of
   bounds, the method's own failure, a delay or DAE step), for the
   command that ran it to show (W63b) */
Result<int> integrate(Session &s, double *t, double *x, double tend, double dt, int count, int nout, int *start);
/* shooting's integration over the whole interval (drawn as it runs when
   ishow, where a failure only ends the curve), or the method's failure */
Result<> ode_int(Session &s, double *y, double *t, int *istart, int ishow);
/* a delay out of range in a step: records why (the first), and the
   integration ends after the step and returns it */
void stop_integration(Session &s, Error why);
/* the curve from x (shoot: from xg along evec, the side sgn), bounds
   unchecked, or the error it failed with */
Result<> shoot(Session &s, double *x, double *xg, double *evec, int sgn);
Result<> shoot_easy(Session &s, double *x);
/* Monte Carlo's search for equilibria from random guesses (with their
   manifolds when ishoot), or the first manifold that failed */
Result<> do_monte_carlo_search(Session &s, int append, int stuffbrowse,int ishoot);

} // namespace xpp
#endif
