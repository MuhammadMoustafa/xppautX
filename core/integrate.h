#ifndef _integrate_h_
#define _integrate_h_

#include <stdio.h>
#include "xpplim.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Initialconds/Range's settings (integrate.cpp's, read by load_eqn.cpp's
   options): one type for both, a C++ requirement since W27 */
typedef struct RangeVars {
  char item[MAX_LEN_SBOX], item2[MAX_LEN_SBOX];
  int steps, steps2, reset, oldic, index, index2, cycle, type, type2, movie;
  double plow, phigh, plow2, phigh2;
  int rtype;
} RangeVars;
extern RangeVars range;

/* the fixed-step integrator Integrate uses (numerics.c picks it) */
extern int (*solver)(double *y, double *tim, double dt, int nt, int neq, int *istart, double *work);
void silent_equilibria(void);
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
void batch_integrate(void);
void do_batch_dry_run(void);
void batch_integrate_once(void);
int write_this_run(const char *file, int i);
void do_init_data(int com);
void run_now(void);
void do_start_flags(double *x, double *t);
void usual_integrate_stuff(double *x);
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
void export_data(FILE *fp);
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
#endif
#endif
