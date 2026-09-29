#ifndef _markov_h_
#define _markov_h_

#include <stdio.h>
#include "xpp_math.h" /* nsrand48/ndrand48, the generator stocHast seeds */
#ifdef __cplusplus
extern "C" {
#endif

/* markov.c */
void add_wiener(int index);
void set_wieners(double dt, double *x, double t);
void add_markov(int nstate, const char *name);
int old_build_markov(FILE *fptr, const char *name);
void create_markov(int nstates, double *st, int type, const char *name);
void add_markov_entry(int index, int j, int k, const char *expr);
void compile_all_markov(void);
int compile_markov(int index, int j, int k);
void update_markov(double *x, double t, double dt);
double new_state(double old, int index, double dt);
void  make_gill_nu(double *nu, int n, int m, double *v);
void  one_gill_step(int meth, int nrxn, int *rxn, double *v);
void  do_stochast_com(int i);
void  mean_back(void);
void  variance_back(void);
void  compute_em(void);
void  free_stoch(void);
void  init_stoch(int len);
void  append_stoch(int first, int length);
void  do_stats(int ierr);

#ifdef __cplusplus
}

#include <array>
#include <span>
#include <string>
#include <vector>
#include "xpplim.h"
/* an .ode transition row's next cell from *start on: the text between
   its { and }, *start past the } */
std::string markov_cell(const char *row, int *start);
/* the Markov variable name's transitions, cells its nstates x nstates
   formulas row by row (text, or numbers for a fixed chain): its index */
int build_markov(std::span<const std::string> cells, const char *name);
/* stocHast's many-runs state (markov.cpp), a Session's (session.h):
   whether a Compute is running (flag), whether its mean and variance
   exist (here), the number of trials and the length of each run, and the
   mean and variance columns. The browser (new_browse_dat, browse.h) takes
   the statistics as a plain float ** of MAXODE rows, so mean/variance
   are arrays of row pointers, each into the vector of mean_rows/
   variance_rows that owns it. */
struct StochasticState {
  int flag = 0, here = 0, n_trials = 0, len = 0;
  std::array<float *, MAXODE> mean{}, variance{};
  std::array<std::vector<float>, MAXODE> mean_rows, variance_rows;
};
#endif
#endif
