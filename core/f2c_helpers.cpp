/* f2c runtime helpers used by the AUTO and EISPACK code (originally f2c's
   libF77 support files cabs.c, d_sign.c and z_abs.c); the one-use ones
   went into their callers (W491). */

#include <cmath>
#include "auto_f2c.h"

namespace {

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

double d_sign(doublereal a, doublereal b) {
  double x;
  x = (a >= 0 ? a : -a);
  return (b >= 0 ? x : -x);
}

double z_abs(const doublecomplex *z) { return (f__cabs(z->r, z->i)); }
