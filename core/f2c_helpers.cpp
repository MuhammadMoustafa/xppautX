/* f2c runtime helpers used by the autoX and EISPACK code (originally f2c's
   libF77 support files cabs.c and z_abs.c; d_sign.c is xpp::transfer_sign); the one-use ones
   went into their callers (W491). */

#include <cmath>
#include "autox_f2c.h"

namespace {

/* cabs.c: |real + i imag| without overflow in the squares */
/* f__cabs: |re + i im| without overflow in the squares (f2c cabs.c) */
double complex_magnitude(double real, double imag) {
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

/* z_abs: f2c's libF77 z_abs, the magnitude of a double complex */
double complex_magnitude_of(const doublecomplex *z) { return (complex_magnitude(z->r, z->i)); }
