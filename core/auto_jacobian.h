// AUTO's finite-difference Jacobians: common loops (W257) and orchestration
// (W333) for autlib3.cpp and autlib5.cpp, preserving evaluator arithmetic.
#pragma once

#include <vector>
#include "auto_c.h"
#include "auto_state.h"
#include "auto_f2c.h"

namespace xpp::auto_jacobian {

// Algebraic fold/Hopf augmentations differentiate only the principal parameter.
constexpr integer principal_parameter_count = 1;

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

// The caller supplies AUTO's step and evaluator; columns follow icp, with
// col0 accounting for state columns in boundary and integral Jacobians.
template <class Eval>
void forward_parameters(integer count, integer nout, const integer *icp, doublereal ep,
                        doublereal *par, Eval &&eval, const doublereal *f,
                        doublereal *ff, doublereal *out, integer ld, integer col0) {
  for (integer i = 0; i < count; ++i) {
    par[icp[i]] += ep;
    eval(ff);
    for (integer j = 0; j < nout; ++j) {
      out[j + (col0 + icp[i]) * ld] = (ff[j] - f[j]) / ep;
    }
    par[icp[i]] -= ep;
  }
}

// Evaluate, then differentiate in AUTO's original state/parameter order.
// col0 keeps integral conditions' state columns before their parameter columns.
template <class Eval>
int differentiate(integer ndim, integer nout, const doublereal *u,
                  const integer *icp, doublereal *par, integer ijac,
                  doublereal *f, doublereal *dfdu, doublereal *dfdp,
                  integer count, integer col0, doublereal *uu1, doublereal *uu2,
                  doublereal *ff1, doublereal *ff2, Eval evaluate) {
  evaluate(u, f);
  if (ijac == 0) return 0;
  const doublereal ep = central(ndim, nout, u, uu1, uu2, ff1, ff2,
                               evaluate, dfdu, nout);
  forward_parameters(count, nout, icp, ep, par,
                     [&](doublereal *value) { evaluate(u, value); },
                     f, ff1, dfdp, nout, col0);
  return 0;
}

// Common augmented-system shell; evaluator equations and parameter range stay
// with the formulation. Scratch is the calling AUTO context's, never global.
template <class Eval>
int problem(const iap_type *iap, const rap_type *rap, integer ndim,
            const doublereal *u, const doublereal *uold, const integer *icp,
            doublereal *par, integer ijac, doublereal *f, doublereal *dfdu,
            doublereal *dfdp, integer count, Eval eval) {
  const auto evaluate = [&](const doublereal *state, doublereal *value) {
    eval(iap, rap, ndim, state, uold, icp, par, value, iap->ndm,
         iap->lib->scratch.dfu, iap->lib->scratch.dfp);
  };
  return differentiate(ndim, ndim, u, icp, par, ijac, f, dfdu, dfdp,
                       count, 0, iap->lib->scratch.uu1, iap->lib->scratch.uu2,
                       iap->lib->scratch.ff1, iap->lib->scratch.ff2, evaluate);
}

// These formulations already supply their Jacobian.
template <class Eval>
int supplied(const iap_type *iap, const rap_type *rap, integer ndim,
             const doublereal *u, const doublereal *uold, const integer *icp,
             doublereal *par, integer ijac, doublereal *f, doublereal *dfdu,
             doublereal *dfdp, Eval eval) {
  eval(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp,
       iap->ndm, iap->lib->scratch.dfu, iap->lib->scratch.dfp);
  return 0;
}

// Integral evaluators need their own local derivative storage, unlike the
// augmented-system scratch reused by problem(). Dimensions follow the iap.
template <class Eval>
int integral(const iap_type *iap, const rap_type *rap, integer ndim,
             doublereal *par, const integer *icp, integer nint,
             const doublereal *u, const doublereal *uold, const doublereal *udot,
             const doublereal *upold, doublereal *f, integer ijac,
             doublereal *dint, Eval eval) {
  std::vector<doublereal> ff1(iap->nint), ff2(iap->nint);
  std::vector<doublereal> uu1(iap->ndim), uu2(iap->ndim);
  std::vector<doublereal> dfu(iap->ndim * (iap->ndim + iap->lib->parameter_count));
  const auto evaluate = [&](const doublereal *state, doublereal *value) {
    eval(iap, rap, ndim, par, icp, nint, iap->nnt0, state, uold, udot,
         upold, value, dfu.data());
  };
  return differentiate(ndim, nint, u, icp, par, ijac, f, dint, dint,
                       iap->nfpr, ndim, uu1.data(), uu2.data(),
                       ff1.data(), ff2.data(), evaluate);
}

}  // namespace xpp::auto_jacobian
