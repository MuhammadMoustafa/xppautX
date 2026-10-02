/* f2c runtime helpers used by the AUTO code (originally f2c's libI77/libF77
   support files cabs.c, d_imag.c, d_lg10.c, d_sign.c, i_dnnt.c, i_nint.c,
   pow_dd.c, pow_di.c, pow_ii.c, r_lg10.c, z_abs.c, z_exp.c, z_log.c),
   merged into one small translation unit since nothing depends on their
   individual file names. */

#include <cmath>
#include "auto_f2c.h"
#include "xpp_math.h"

namespace {
constexpr double log10e = 0.43429448190325182765;

/* cabs.c: |real + i imag| without overflow in the squares */
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
} // namespace

double d_imag(const doublecomplex *z) { return (z->i); }

double d_lg10(const doublereal *x) { return (log10e * xpp::math::log(*x)); }

double d_sign(doublereal a, doublereal b) {
  double x;
  x = (a >= 0 ? a : -a);
  return (b >= 0 ? x : -x);
}

integer i_dnnt(const doublereal *x) {
  return static_cast<integer>(*x >= 0. ? floor(*x + .5) : -floor(.5 - *x));
}

integer i_nint(const real *x) {
  return static_cast<integer>(*x >= 0 ? floor(*x + .5) : -floor(.5 - *x));
}

double pow_dd(const doublereal *ap, const doublereal *bp) { return (xpp::math::pow(*ap, *bp)); }

double pow_di(const doublereal *ap, const integer *bp) {
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

double r_lg10(real x) { return (log10e * xpp::math::log(x)); }

double z_abs(const doublecomplex *z) { return (f__cabs(z->r, z->i)); }

void z_exp(doublecomplex *r, const doublecomplex *z) {
  double expx, zi = z->i;

  expx = xpp::math::exp(z->r);
  r->r = expx * xpp::math::cos(zi);
  r->i = expx * xpp::math::sin(zi);
}

void z_log(doublecomplex *r, const doublecomplex *z) {
  double zi = z->i, zr = z->r;
  r->i = xpp::math::atan2(zi, zr);
  r->r = xpp::math::log(f__cabs(zr, zi));
}
