#pragma once

#include <cstdio>
#include "xpp_math.h" /* Random, the generator stocHast seeds */
#include <array>
#include <span>
#include <string>
#include <vector>
#include "storage.h"
#include "xpp_error.h"
#include "model.h" /* Model::OuProcess */

namespace xpp {

struct Session; /* session.h */

void  make_gill_nu(Session &s, double *nu, int n, int m, double *v);
void  one_gill_step(Session &s, int meth, int nrxn, int *rxn, double *v, double *r);

/* the Markov variables, the Wiener parameters and stocHast (markov.cpp),
   in the Session s: a load's, or the run's */
void add_markov(Session &s, int nstate, const char *name);
void add_wiener(Session &s, int index);
/* every Ornstein-Uhlenbeck process to its start (a run's) */
void start_ou(Session &s);
/* the noise of the step of dt about to be taken: Markov chains, Wiener parameters and Ornstein-Uhlenbeck processes, once for all its stages */
void set_wieners(Session &s, double dt, double *x, double t);
int old_build_markov(Session &s, FILE *fptr, const char *name);
void create_markov(Session &s, int nstates, double *st, int type, const char *name);
void add_markov_entry(Session &s, int index, int j, int k, const char *expr);
void compile_all_markov(Session &s);
int compile_markov(Session &s, int index, int j, int k);
void update_markov(Session &s, double *x, double t, double dt);
double new_state(Session &s, double old, int index, double dt);
/* the Stochastic menu's items (an imported button sequence's), asking for what the analysis commands take as arguments */
void  do_stochast_com(Session &s, int i);
/* the seed the next run starts from (the Session's random numbers seeded with it at once), or why it is not one */
Result<> set_random_seed(Session &s, int seed);
/* the mean or the variance of the stochastic runs in the browser, or why there are none */
Result<> mean_back(Session &s);
Result<> variance_back(Session &s);
/* the Range settings' sweep as many independent runs whose mean and variance are kept: the runs completed */
Result<int> stochastic_integrate(Session &s);
void  free_stoch(Session &s);
void  init_stoch(Session &s, int len);
void  append_stoch(Session &s, int first, int length);
void  do_stats(Session &s, int ierr);

/* an .ode transition row's next cell from *start on: the text between
   its { and }, *start past the } */
std::string markov_cell(const char *row, int *start);
/* the Markov variable name's transitions, cells its nstates x nstates
   formulas row by row (text, or numbers for a fixed chain): its index */
int build_markov(Session &s, std::span<const std::string> cells, const char *name);
/* the mean and variance of the runs converted to the store's precision (storage_precision_changed) */
void retype_stochastic(Session &s);
/* stocHast's many-runs state (markov.cpp), a Session's (session.h):
   whether a Compute is running (flag), whether its mean and variance
   exist (here), the number of trials and the length of each run, and the
   mean and variance columns. The browser (new_browse_dat, browse.h) takes
   the statistics as a table of columns, so mean/variance are arrays of
   column views, each into the mean_rows/variance_rows column that owns it
   (at the store's precision, the arithmetic as the store keeps a value). */
struct StochasticState {
  /* a Compute sweep is running: its runs add to the mean and the variance (markov.cpp stochastic_integrate sets it, only for the sweep) */
  bool flag = false;
  int here = 0, n_trials = 0, len = 0;
  std::vector<Column> mean, variance;
  std::vector<OwnedColumn> mean_rows, variance_rows;
};

} // namespace xpp
