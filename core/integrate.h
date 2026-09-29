#ifndef _integrate_h_
#define _integrate_h_

#include <stdio.h>
#include "xpplim.h"
#ifdef __cplusplus
extern "C" {
#endif

/* the equilibrium Newton finds from the initial data, with its
   eigenvalues (x, re, im per variable), written to name (nothing when
   Newton does not converge); shoot: also the invariant manifolds of a
   saddle, integrated into UMk.dat/SMk.dat (-silent's -equil 0/1, the
   protocol's `equilibrium` `write`) */
void write_equilibrium(const char *name, int shoot);
void init_ar_ic(void);
void dump_range(FILE *fp, int f);
void init_range(void);
int set_up_eq_range(void);
void cont_integ(void);
int range_item(void);
int range_item2(void);
int set_up_range(void);
int set_up_range2(void);
void init_monte_carlo(void);
void monte_carlo(void);
void do_monte_carlo_search(int append, int stuffbrowse,int ishoot);
void do_eq_range(double *x);
void swap_color(int *col, int rorw);
void set_cycle(int flag, int *icol);
int do_range(double *x, int flag);
void find_equilib_com(int com);
int write_this_run(const char *file, int i);
void do_init_data(int com);
void run_now(void);
void do_start_flags(double *x, double *t);
int usual_integrate_stuff(double *x); /* integrate()'s result */
void do_new_array_ic(const char *newic, int j1, int j2);
void store_new_array_ic(const char *newic, int j1, int j2, const char *formula);
void evaluate_ar_ic(const char *v, const char *f, int j1, int j2);
int extract_ic_data(char *big);
void arr_ic_start(void);
int set_array_ic(void);
int form_ic(void);
void get_ic(int it, double *x);
int ode_int(double *y, double *t, int *istart, int ishow);
int integrate(double *t, double *x, double tend, double dt, int count, int nout, int *start);
void send_halt(double *y, double t);
void send_output(double *y, double t);
void do_plot(float *oldxpl, float *oldypl, float *oldzpl, float *xpl, float *ypl, float *zpl);
void plot_the_graphs(float *xv, float *xvold, int node, int neq, double ddt, int *tc,int flag);
void plot_one_graph(float *xv, float *xvold, int node, int neq, double ddt, int *tc);
void restore(int i1, int i2);
void comp_color(float *v1, float *v2, int n, float dt);
void shoot(double *x, double *xg, double *evec, int sgn);
void shoot_easy(double *x);
void stop_integration(void);
int stor_full(void);
int do_auto_range_go();

#ifdef __cplusplus
}

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "solver.h"

/* the model's array initial values (x[j1..j2](0)=formula, applied by
   arr_ic_start when set_all_vals sets the model up), element by element:
   each variable's name and formula with the index worked out, the index
   j, and which of the model's array initial values it is (group, from 1) */
struct ArrayInitialValue {
  std::string var, formula;
  int j = 0, group = 0;
};
std::vector<ArrayInitialValue> array_initial_values();

/* Initialconds/Range's settings (integrate.cpp's, read by load_eqn.cpp's
   options): the one range over item (and item2 for the double range) */
struct RangeVars {
  std::string item, item2;
  int steps, steps2, reset, oldic, index, index2, cycle, type, type2, movie;
  double plow, phigh, plow2, phigh2;
  int rtype;
};

/* the integrator's state, a Session's (session.h) */
struct IntegratorState {
  /* the method's solver, with its work memory (xpp::start_solver) */
  std::unique_ptr<xpp::Solver> solver;
  /* the right-hand side the solvers step (my_rhs, or AUTO's and the
     adjoint's own while they run) */
  int (*rhs)(double t, double *y, double *ydot, int neq) = nullptr;
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
  /* a DAE's algebraic solve failed during the step */
  int delay_err = 0;
  /* -makeplot: a batch run writes its plot too */
  int make_plot_flag = 0;
  /* where the last integration stopped */
  double last_time = 0;
  /* the adjoint is computed over a range (adj2.cpp) */
  int adj_range = 0;
};
#endif
#endif
