#pragma once
/* stocHast's many runs (stochastic.cpp): Stochastic Compute's sweep and the mean and variance of its runs, which the
   data table can show. The Markov variables and the noise of one step are markov.h's. */
#include <vector>
#include "storage.h"
#include "xpp_error.h"

namespace xpp {

struct Session; /* session.h */
struct RunReport; /* integrate.h */

/* the mean or the variance of the stochastic runs in the browser, or why there are none */
Result<> mean_back(Session &s);
Result<> variance_back(Session &s);
/* the Range settings' sweep as many independent runs whose mean and variance are kept: the runs completed */
Result<int> stochastic_integrate(Session &s, const RunReport &report);
void  free_stoch(Session &s);
void  init_stoch(Session &s, int len);
void  append_stoch(Session &s, int first, int length);
void  do_stats(Session &s, int ierr);

/* the mean and variance of the runs converted to the store's precision (storage_precision_changed) */
void retype_stochastic(Session &s);
/* stocHast's many-runs state (stochastic.cpp), a Session's (session.h):
   whether a Compute is running (flag), whether its mean and variance
   exist (here), the number of trials and the length of each run, and the
   mean and variance columns. The browser (new_browse_dat, browse.h) takes
   the statistics as a table of columns, so mean/variance are arrays of
   column views, each into the mean_rows/variance_rows column that owns it
   (at the store's precision, the arithmetic as the store keeps a value). */
struct StochasticState {
  /* a Compute sweep is running: its runs add to the mean and the variance (stochastic.cpp stochastic_integrate sets it, only for the sweep) */
  bool flag = false;
  int here = 0, n_trials = 0, len = 0;
  std::vector<Column> mean, variance;
  std::vector<OwnedColumn> mean_rows, variance_rows;
};

} // namespace xpp
