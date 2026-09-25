#ifndef _do_fit_h_
#define _do_fit_h_
#ifdef __cplusplus
extern "C" {
#endif

#include "xpplim.h"


typedef struct {
  char file[MAX_LEN_SBOX];
  char varlist[MAX_LEN_SBOX],collist[MAX_LEN_SBOX];
  char parlist1[MAX_LEN_SBOX],parlist2[MAX_LEN_SBOX];
  int dim,npars,nvars,npts,maxiter;
  int icols[50],ipar[50],ivar[50];
  double tol,eps;
} FITINFO;


void init_fit_info(void);
void get_fit_info(double *y, double *a, double *t0, int *flag, double eps, double *yfit, double **yderv, int npts, int npars, int nvars, int *ivar, int *ipar);
int one_step_int(double *y, double t0, double t1, int *istart);
void print_fit_info(void);
void test_fit(void);
int run_fit(char *filename, int npts, int npars, int nvars, int maxiter, int ndim, double eps, double tol, int *ipar, int *ivar, int *icols, double *y0, double *a, double *yfit);
int marlevstep(double *t0, double *y0, double *y, double *sig, double *a, int npts, int nvars, int npars, int *ivar, int *ipar, double *covar, double *alpha, double *chisq, double *alambda, double *work, double **yderv, double *yfit, double *ochisq, int ictrl, double eps);
int mrqcof(double *t0, double *y0, double *y, double *sig, double *a, int npts, int nvars, int npars, int *ivar, int *ipar, double *alpha, double *chisq, double *beta, double **yderv, double *yfit, double eps);
int get_fit_params(void);
void parse_collist(char *collist, int *icols, int *n);
void parse_varlist(char *varlist, int *ivars, int *n);
void parse_parlist(char *parlist, int *ipars, int *n);

#ifdef __cplusplus
}
#endif
#endif

