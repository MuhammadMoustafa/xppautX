#ifndef _do_fit_h_
#define _do_fit_h_

#include <string_view>
#include "xpp_error.h"
#include "xpplim.h"

/* FITINFO is internal to do_fit.cpp (the only file that uses it); it is
   not declared here. */

namespace xpp {

struct Session; /* session.h */

void init_fit_info();
void print_fit_info();
int get_fit_params();

void test_fit(Session &s);

/* a fit that ran: it converged, or stopped at its iteration limit */
enum class FitEnd { Converged, MaxIterations };

/* the fit's pieces return why they failed (a data file, a singular
   matrix, an integration), for the command, test_fit, to show */
Result<> get_fit_info(Session &s, double *y, double *a, double *t0, double eps, double *yfit, double **yderv, int npts, int npars, int nvars, int *ivar, int *ipar);
Result<> one_step_int(Session &s, double *y, double t0, double t1, int *istart);
Result<FitEnd> run_fit(Session &s, const char *filename, int npts, int npars, int nvars, int maxiter, int ndim, double eps, double tol, int *ipar, int *ivar, int *icols, double *y0, double *a, double *yfit);
Result<> marlevstep(Session &s, double *t0, double *y0, double *y, double *sig, double *a, int npts, int nvars, int npars, int *ivar, int *ipar, double *covar, double *alpha, double *chisq, double *alambda, double *work, double **yderv, double *yfit, double *ochisq, int ictrl, double eps);
Result<> mrqcof(Session &s, double *t0, double *y0, double *y, double *sig, double *a, int npts, int nvars, int npars, int *ivar, int *ipar, double *alpha, double *chisq, double *beta, double **yderv, double *yfit, double eps);

/* the fit's lists, blank- or comma-separated: the data columns, the
   fitted variables and (appended at ipars[*n]) the parameters */
void parse_collist(std::string_view collist, int *icols, int *n);
void parse_varlist(const Session &s, std::string_view varlist, int *ivars, int *n);
void parse_parlist(const Session &s, std::string_view parlist, int *ipars, int *n);

} // namespace xpp
#endif
