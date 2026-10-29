#pragma once

#include <string_view>
#include "xpp_error.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace xpp {

struct Session; /* session.h */

/* Data's Fit settings (do_fit.cpp), a Session's (session.h): the data
   file, the variables and their columns, the parameters to fit, the
   dimension, points, iterations and tolerances */
struct FitInfo {
  std::string file;
  std::string varlist, collist;
  std::string parlist1, parlist2;
  int dim = 0, npars = 0, nvars = 0, npts = 0, maxiter = 20;
  std::vector<int> icols, ipar, ivar;
  double tol = .001, eps = 1e-5;
};

void print_fit_info(const Session &s);
int get_fit_params(Session &s);


/* a fit that ran: it converged, stopped at its iteration limit, or was cancelled */
enum class FitEnd { Converged, MaxIterations, Stopped };
/* how a fit ended and where it stood: the iterations taken, chi-squared, and the last relative
   improvement of chi-squared (what tol is compared with) */
struct FitResult {
  FitEnd end = FitEnd::Stopped;
  int iterations = 0;
  double chisq = 0, change = 0;
};

/* a fit that ran: how it ended and the fitted quantities (the parameters, and initial conditions, by name) now */
struct FitOutcome {
  FitResult result;
  std::vector<std::pair<std::string, double>> parameters;
};
/* the fit of the data file s.fit.file as s.fit's settings say, the fitted quantities set to the last iterate
   (the settings' lists are get_fit_params's questions, or the command's arguments), or why it failed */
Result<FitOutcome> test_fit(Session &s);

/* the fit's pieces return why they failed (a data file, a singular
   matrix, an integration), for the command, test_fit, to show */
Result<> get_fit_info(Session &s, double *y, double *a, double *t0, double eps, double *yfit, double **yderv, int npts, int npars, int nvars, int *ivar, int *ipar);
Result<> one_step_int(Session &s, double *y, double t0, double t1, int *istart);
Result<FitResult> run_fit(Session &s, const char *filename, int npts, int npars, int nvars, int maxiter, int ndim, double eps, double tol, int *ipar, int *ivar, int *icols, double *y0, double *a);
Result<> marlevstep(Session &s, double *t0, double *y0, double *y, double *sig, double *a, int npts, int nvars, int npars, int *ivar, int *ipar, double *covar, double *alpha, double *chisq, double *alambda, double *work, double **yderv, double *yfit, double *ochisq, int ictrl, double eps);
Result<> mrqcof(Session &s, double *t0, double *y0, double *y, double *sig, double *a, int npts, int nvars, int npars, int *ivar, int *ipar, double *alpha, double *chisq, double *beta, double **yderv, double *yfit, double eps);

/* the fit's lists, blank- or comma-separated: the data columns, the
   fitted variables and (appended at ipars) the parameters */
void parse_collist(std::string_view collist, std::vector<int> &icols);
void parse_varlist(const Session &s, std::string_view varlist, std::vector<int> &ivars);
void parse_parlist(const Session &s, std::string_view parlist, std::vector<int> &ipars);

} // namespace xpp
