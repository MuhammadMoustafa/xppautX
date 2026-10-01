#ifndef _markov_h_
#define _markov_h_

#include <cstdio>
#include "xpp_math.h" /* Random, the generator stocHast seeds */
#include <array>
#include <span>
#include <string>
#include <vector>
#include "xpplim.h"

namespace xpp {

struct Session; /* session.h */

void  make_gill_nu(Session &s, double *nu, int n, int m, double *v);
void  one_gill_step(Session &s, int meth, int nrxn, int *rxn, double *v);

/* the Markov variables, the Wiener parameters and stocHast (markov.cpp),
   in the Session s: a load's, or the run's */
void add_markov(Session &s, int nstate, const char *name);
void add_wiener(Session &s, int index);
void set_wieners(Session &s, double dt, double *x, double t);
int old_build_markov(Session &s, FILE *fptr, const char *name);
void create_markov(Session &s, int nstates, double *st, int type, const char *name);
void add_markov_entry(Session &s, int index, int j, int k, const char *expr);
void compile_all_markov(Session &s);
int compile_markov(Session &s, int index, int j, int k);
void update_markov(Session &s, double *x, double t, double dt);
double new_state(Session &s, double old, int index, double dt);
void  do_stochast_com(Session &s, int i);
void  mean_back(Session &s);
void  variance_back(Session &s);
void  compute_em(Session &s);
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

} // namespace xpp
#endif
