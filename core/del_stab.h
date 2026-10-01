#ifndef _del_stab_h_
#define _del_stab_h_

#include "xpp_error.h"

namespace xpp {

struct Session; /* session.h */

struct COMPLEX {
  double r,i;
};

/* del_stab.cpp */
COMPLEX cdif(COMPLEX z, COMPLEX w);
COMPLEX cmlt(COMPLEX z, COMPLEX w);
COMPLEX cdivv(COMPLEX z, COMPLEX w);
COMPLEX cexp2(COMPLEX z);
void switch_rows(COMPLEX *z, int i1, int i2, int n);
COMPLEX rtoc(double x, double y);
double c_abs(COMPLEX z);
COMPLEX cdeterm(COMPLEX *z, int n);
void make_z(COMPLEX *z, double *delay, int n, int m, double *coef, COMPLEX lambda);
void process_root(double real, double im);
double get_arg(double *delay, double *coef, int m, int n, COMPLEX lambda);
int test_sign(double old, double newval);
int plot_args(double *coef, double *delay, int n, int m, int npts, double almax, double wmax);

int find_positive_root(Session &s, double *coef, double *delay, int n, int m, double rad, double err, double eps, double big, int maxit, double *rr);

/* Sing pts for a delay equation: the equilibrium Newton finds from x
   (or Could not converge, an error) and its stability (stabinfo) */
Result<> do_delay_sing(Session &s, double *x, double eps, double err, double big, int maxit, int n, int *ierr, float *stabinfo);

} // namespace xpp
#endif
