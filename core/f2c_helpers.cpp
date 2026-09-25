/* f2c runtime helpers used by the AUTO code (originally f2c's libI77/libF77
   support files cabs.c, d_imag.c, d_lg10.c, d_sign.c, i_dnnt.c, i_nint.c,
   pow_dd.c, pow_di.c, pow_ii.c, r_lg10.c, z_abs.c, z_exp.c, z_log.c),
   merged into one small translation unit since nothing depends on their
   individual file names. */

#include <cmath> /* first: C++ headers before auto_f2c.h's min/max macros */
#include "auto_f2c.h"

namespace {
constexpr double log10e = 0.43429448190325182765;
}

double f__cabs(double real, double imag) {
  double temp;

  if (real < 0)
    real = -real;
  if (imag < 0)
    imag = -imag;
  if (imag > real) {
    temp = real;
    real = imag;
    imag = temp;
  }
  if ((real + imag) == real)
    return (real);

  temp = imag / real;
  temp = real * sqrt(1.0 + temp * temp); /*overflow!!*/
  return (temp);
}

double d_imag(doublecomplex *z) { return (z->i); }

double d_lg10(doublereal *x) { return (log10e * log(*x)); }

double d_sign(doublereal a, doublereal b) {
  double x;
  x = (a >= 0 ? a : -a);
  return (b >= 0 ? x : -x);
}

integer i_dnnt(doublereal *x) {
  return (integer)(*x >= 0. ? floor(*x + .5) : -floor(.5 - *x));
}

integer i_nint(real *x) {
  return (integer)(*x >= 0 ? floor(*x + .5) : -floor(.5 - *x));
}

double pow_dd(doublereal *ap, doublereal *bp) { return (pow(*ap, *bp)); }

double pow_di(doublereal *ap, integer *bp) {
  double pow_, x;
  integer n;
  unsigned long u;

  pow_ = 1;
  x = *ap;
  n = *bp;

  if (n != 0) {
    if (n < 0) {
      n = -n;
      x = 1 / x;
    }
    for (u = n;;) {
      if (u & 01)
        pow_ *= x;
      if (u >>= 1)
        x *= x;
      else
        break;
    }
  }
  return (pow_);
}

integer pow_ii(integer ap, integer bp) {
  integer pow_, x, n;
  unsigned long u;

  x = ap;
  n = bp;

  if (n <= 0) {
    if (n == 0 || x == 1)
      return 1;
    if (x != -1)
      return x == 0 ? 1 / x : 0;
    n = -n;
  }
  u = n;
  for (pow_ = 1;;) {
    if (u & 01)
      pow_ *= x;
    if (u >>= 1)
      x *= x;
    else
      break;
  }
  return (pow_);
}

double r_lg10(real x) { return (log10e * log(x)); }

double z_abs(doublecomplex *z) { return (f__cabs(z->r, z->i)); }

void z_exp(doublecomplex *r, doublecomplex *z) {
  double expx, zi = z->i;

  expx = exp(z->r);
  r->r = expx * cos(zi);
  r->i = expx * sin(zi);
}

void z_log(doublecomplex *r, doublecomplex *z) {
  double zi = z->i, zr = z->r;
  r->i = atan2(zi, zr);
  r->r = log(f__cabs(zr, zi));
}
