#pragma once

#include "equilibria.h"
#include "xpp_error.h"
#include "xpp_math.h" /* xpp::ComplexPair */

namespace xpp {

struct Session; /* session.h */

/* del_stab.cpp */
ComplexPair complex_subtract(ComplexPair z, ComplexPair w);
ComplexPair complex_multiply(ComplexPair z, ComplexPair w);
ComplexPair complex_divide(ComplexPair z, ComplexPair w);
ComplexPair complex_exponential(ComplexPair z);
void switch_rows(ComplexPair *z, int i1, int i2, int n);
ComplexPair make_complex(double x, double y);
double complex_modulus(ComplexPair z);
ComplexPair complex_determinant(ComplexPair *z, int n);
void fill_characteristic_matrix(ComplexPair *z, double *delay, int n, int m, double *coef, ComplexPair lambda);
void process_root(double real, double im);
double get_arg(double *delay, double *coef, int m, int n, ComplexPair lambda);
int test_sign(double old, double newval);
int plot_args(double *coef, double *delay, int n, int m, int npts, double almax, double wmax);

/* Newton's method in the complex plane from the dominant root so far for a root of the characteristic
   equation: how it ended (NewtonRun, equilibria.h) */
NewtonRun find_positive_root(Session &s, double *coef, double *delay, int n, int m, double rad, double err, double eps, double big, int maxit, double *rr);

/* Sing pts for a delay equation: the equilibrium Newton finds from x
   (or Could not converge, an error) and its stability (stabinfo) */
Result<> do_delay_sing(Session &s, double *x, double eps, double err, double big, int maxit, int n, int *ierr, double *stabinfo);

} // namespace xpp
