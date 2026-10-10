// AUTO's finite-difference Jacobians (W257): the one central-difference loop
// the extended systems of autlib3.cpp and autlib5.cpp all use.
#pragma once

#include "auto_c.h"
#include "auto_f2c.h"

namespace xpp::auto_jacobian {

// d(eval)/dx by central differences about x[0..n), step HMACH*(max|x| + 1)
// (HMACH: AUTO's own relative step), one evaluation either side of x_i in turn.
// eval(const double *x, double *f) fills f[0..nout); column i of the
// nout-by-n result goes to out[j + i*ld]. uu1, uu2 (n values) and ff1, ff2
// (nout values) are the caller's scratch. Returns the step, which the
// forward differences for the parameters then reuse.
template <class Eval>
doublereal central(integer n, integer nout, const doublereal *x, doublereal *uu1, doublereal *uu2,
                   doublereal *ff1, doublereal *ff2, Eval &&eval, doublereal *out, integer ld) {
  doublereal umx = 0.;
  for (integer i = 0; i < n; ++i) {
    if (f2c::abs(x[i]) > umx) {
      umx = f2c::abs(x[i]);
    }
  }
  const doublereal ep = HMACH * (umx + 1);
  for (integer i = 0; i < n; ++i) {
    for (integer j = 0; j < n; ++j) {
      uu1[j] = x[j];
      uu2[j] = x[j];
    }
    uu1[i] -= ep;
    uu2[i] += ep;
    eval(uu1, ff1);
    eval(uu2, ff2);
    for (integer j = 0; j < nout; ++j) {
      out[j + i * ld] = (ff2[j] - ff1[j]) / (ep * 2);
    }
  }
  return ep;
}

}  // namespace xpp::auto_jacobian
