#include "xpp_mem.h"
// autoX's finite-difference Jacobians: common loops (W257) and orchestration
// (W333) for autox_lib3.cpp and autox_lib5.cpp, preserving evaluator arithmetic.
#pragma once

#include <algorithm>
#include <vector>
#include "xpp_math.h"
#include "jacobian.h"
#include "autox_c.h"
#include "autox_state.h"
#include "autox_f2c.h"
#include "session.h"
#include "solver.h"

namespace xpp::autox_jacobian {

// Algebraic fold/Hopf augmentations differentiate only the principal parameter.
constexpr integer principal_parameter_count = 1;

// The largest |x[i]|, i < n.
inline doublereal largest_magnitude(integer n, const doublereal *x) {
  doublereal umx = 0.;
  for (integer i = 0; i < n; ++i) {
    if (f2c::abs(x[i]) > umx) {
      umx = f2c::abs(x[i]);
    }
  }
  return umx;
}

// autoX's difference step about x[0..n): HMACH*(max|x| + 1).
inline doublereal central_step(integer n, const doublereal *x) {
  return autox_hmach * (largest_magnitude(n, x) + 1);
}

// The difference groups of the model's states (W488, jacobian.h) for autoX's
// differences of the right-hand side, or null when it does not group: its reads
// are not known, or a map iterated store_every times reads more states than
// one step does.
inline const DifferenceGroups *state_groups(const iap_type *iap, integer n) {
  const auto &numerics = iap->run->session->numerics;
  const bool chained = xpp::solver_info(numerics.method).traits.discrete && numerics.store_every != 1;
  return chained ? nullptr : difference_groups(iap->run->session->model(), iap->run->jacobian_scratch, n);
}

// d(eval)/dx by central differences about x[0..n), step HMACH*(max|x| + 1)
// (HMACH: autoX's own relative step), one evaluation either side of x_i in turn.
// eval(const double *x, double *f) fills f[0..nout); column i of the
// nout-by-n result goes to out[j + i*ld]. uu1, uu2 (n values) and ff1, ff2
// (nout values) are the caller's scratch. Returns the step, which the
// forward differences for the parameters then reuse.
template <class Eval>
doublereal central(integer n, integer nout, const doublereal *x, doublereal *uu1, doublereal *uu2,
                   doublereal *ff1, doublereal *ff2, Eval &&eval, doublereal *out, integer ld,
                   std::span<const integer> columns = {}) {
  const doublereal ep = central_step(n, x);
  const integer count = columns.empty() ? n : static_cast<integer>(columns.size());
  for (integer column = 0; column < count; ++column) {
    const integer i = columns.empty() ? column : columns[column];
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

// central() with the columns in difference groups (jacobian.h): one pair of
// evaluations per group instead of per column, the same bits. Equation j
// reads none of the group's states but one, i, so with the whole group
// perturbed it gives j's value for i alone perturbed (an equation reading
// none gives its value at x itself, fx, as it would with i perturbed
// alone); those entries are (fx-fx)/(2 ep), as central() computes them.
// fx is the evaluator at x.
template <class Eval>
doublereal central_grouped(integer n, const doublereal *x, const doublereal *fx, const DifferenceGroups &groups,
                           doublereal *uu1, doublereal *uu2, doublereal *ff1, doublereal *ff2,
                           Eval &&eval, doublereal *out, integer ld) {
  const doublereal ep = central_step(n, x);
  for (const std::vector<int> &group : groups.groups) {
    for (integer j = 0; j < n; ++j) {
      uu1[j] = x[j];
      uu2[j] = x[j];
    }
    for (const int i : group) {
      uu1[i] -= ep;
      uu2[i] += ep;
    }
    eval(uu1, ff1);
    eval(uu2, ff2);
    for (const int i : group) {
      for (integer j = 0; j < n; ++j) {
        out[j + i * ld] = (fx[j] - fx[j]) / (ep * 2);
      }
      for (const int j : groups.rows[static_cast<size_t>(i)]) {
        out[j + i * ld] = (ff2[j] - ff1[j]) / (ep * 2);
      }
    }
  }
  return ep;
}

// The fold system's difference columns in groups: the n states in the groups
// of `states` (the model's), then the second parameter at ndim-1, alone, since
// every equation reads it. Row n+j of the system, (A v)_j, reads exactly the
// states equation j reads, so `states` serves for those rows, but A is itself
// differenced with autoX's step from the largest |x| of the states it is given,
// and a group's perturbation must leave that step as each state's alone does:
// a state that moves the largest |x| (the largest itself, or one within ep of
// it) is a group of its own, with every row taken from its evaluations.
inline DifferenceGroups fold_columns(const DifferenceGroups &states, integer n, integer ndim,
                                     const doublereal *x, doublereal ep) {
  const doublereal umx = largest_magnitude(n, x);
  DifferenceGroups columns;
  columns.rows.resize(static_cast<size_t>(ndim));
  const auto alone = [&](integer i) {
    for (integer row = n; row < ndim; ++row) columns.rows[static_cast<size_t>(i)].push_back(static_cast<int>(row));
    columns.groups.push_back({static_cast<int>(i)});
  };
  for (const std::vector<int> &group : states.groups) {
    std::vector<int> together;
    for (const int i : group) {
      if (f2c::abs(x[i] - ep) <= umx && f2c::abs(x[i] + ep) <= umx) {
        together.push_back(i);
        for (const int j : states.rows[static_cast<size_t>(i)]) columns.rows[static_cast<size_t>(i)].push_back(static_cast<int>(n) + j);
      } else {
        alone(i);
      }
    }
    if (!together.empty()) columns.groups.push_back(std::move(together));
  }
  alone(ndim - 1);
  return columns;
}

// The caller supplies autoX's step and evaluator; columns follow icp, with
// col0 accounting for state columns in boundary and integral Jacobians.
template <class Eval>
void forward_parameters(integer count, integer nout, const integer *icp, doublereal ep,
                        doublereal *par, Eval &&eval, const doublereal *f,
                        doublereal *ff, doublereal *out, integer ld, integer col0) {
  for (integer i = 0; i < count; ++i) {
    // (p + ep) - ep is not always p: restore the value itself, as AUTO-07p does
    const doublereal saved = par[icp[i]];
    par[icp[i]] = saved + ep;
    eval(ff);
    for (integer j = 0; j < nout; ++j) {
      out[j + (col0 + icp[i]) * ld] = (ff[j] - f[j]) / ep;
    }
    par[icp[i]] = saved;
  }
}

// Evaluate, then differentiate in autoX's original state/parameter order.
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
// with the formulation. Scratch is the calling autoX context's, never global.
template <class Eval>
int problem(const iap_type *iap, const rap_type *rap, integer ndim,
            const doublereal *u, const doublereal *uold, const integer *icp,
            doublereal *par, integer ijac, doublereal *f, doublereal *dfdu,
            doublereal *dfdp, integer count, Eval eval) {
  const auto evaluate = [&](const doublereal *state, doublereal *value) {
    eval(iap, rap, ndim, state, uold, icp, par, value, iap->ndm,
         iap->run->scratch.dfu.data(), iap->run->scratch.dfp.data());
  };
  return differentiate(ndim, ndim, u, icp, par, ijac, f, dfdu, dfdp,
                       count, 0, iap->run->scratch.uu1.data(), iap->run->scratch.uu2.data(),
                       iap->run->scratch.ff1.data(), iap->run->scratch.ff2.data(), evaluate);
}

// Structured equilibrium fold/Hopf Jacobian, reimplemented from AUTO-07p
// toolboxae.f90:171-265 and equilibrium.f90:84-187. Only derivatives of
// A*v (or (A*A+kappa*I)*v and phase) need numerical second derivatives.
// eval supplies A and, for Hopf, the extra vector/phase blocks after A.
template <class Eval>
int curve(const iap_type *iap, const rap_type *rap, integer ndim,
          const doublereal *u, const doublereal *uold, const integer *icp,
          doublereal *par, integer ijac, doublereal *f, doublereal *dfdu,
          doublereal *dfdp, bool hopf, Eval eval) {
  const integer n = iap->ndm;
  std::vector<double> base(xpp::size_product(n, ndim)), parameters(xpp::size_product(n, iap->run->parameter_count));
  // ijac 1 for the evaluations whose derivatives with respect to the parameters nobody reads
  const auto evaluate = [&](const double *state, double *value, integer jac = 1) {
    eval(iap, rap, ndim, state, uold, icp, par, jac, value, n,
         base.data(), parameters.data());
  };
  evaluate(u, f, 2);
  if (ijac == 0) return 0;
  const Matrix<const double> a(base, n, ndim);
  const Matrix<const double> p(parameters, n, iap->run->parameter_count);
  const Matrix<double> jac({dfdu, static_cast<size_t>(ndim * ndim)}, ndim, ndim);
  const Matrix<double> dp({dfdp, static_cast<size_t>(ndim * iap->run->parameter_count)}, ndim, iap->run->parameter_count);
  std::fill_n(dfdu, ndim * ndim, 0.);
  for (integer i = 0; i < n; ++i) {
    for (integer j = 0; j < n; ++j) {
      jac(i, j) = a(i, j);
      jac(n + i, n + j) = hopf ? a(i, n + j) : a(i, j) + (i==j && iap->ips==AutoxProblem::DiscreteMap && iap->itpst==AutoxPointKind::PeriodDoubling ? 2. : 0.);
    }
    jac(i, ndim - 1) = p(i, icp[1]);
    jac(2 * n, n + i) = 2 * u[n + i];
    if (hopf) {
      jac(n + i, 2 * n) = a(i, 2 * n);
      jac(ndim - 1, n + i) = a(i, ndim - 1);
    }
    dp(i, icp[0]) = p(i, icp[0]);
  }
  std::vector<integer> columns(xpp::size_sum(n, 1));
  for (integer i = 0; i < n; ++i) columns[i] = i;
  columns[n] = ndim - 1; // p2 is the last augmented state
  std::vector<double> low(ndim), high(ndim), minus(ndim), plus(ndim), numeric(xpp::size_product(ndim, ndim));
  // The fold system's columns in groups where the model's reads allow; the Hopf
  // system's phase row (the last) reads every state, so its columns go one by one.
  const DifferenceGroups *states = hopf || iap->ips == AutoxProblem::DiscreteMap || iap->jac != autox_jac_differences
                                       ? nullptr : state_groups(iap, n);
  const double step = states
      ? central_grouped(ndim, u, f, fold_columns(*states, n, ndim, u, central_step(ndim, u)), low.data(), high.data(),
                        minus.data(), plus.data(), evaluate, numeric.data(), ndim)
      : central(ndim, ndim, u, low.data(), high.data(), minus.data(), plus.data(),
                evaluate, numeric.data(), ndim, columns);
  const Matrix<const double> difference(numeric, ndim, ndim);
  for (const integer column : columns)
    for (integer row = n; row < ndim; ++row)
      if (row != 2 * n) jac(row, column) = difference(row, column);
  // AUTO-07p uses a forward difference for the principal parameter's
  // variational equations; the model's own parameter block above wins in rows
  // 0..n-1 (the difference also visited F, whose supplied derivatives are exact).
  std::vector<double> principal(n);
  for (integer i = 0; i < n; ++i) principal[i] = dp(i, icp[0]);
  forward_parameters(principal_parameter_count, ndim, icp, step, par,
                     [&](double *value) { evaluate(u, value); },
                     f, plus.data(), dfdp, ndim, 0);
  par[icp[1]] = u[ndim - 1];
  for (integer i = 0; i < n; ++i) dp(i, icp[0]) = principal[i];
  dp(2 * n, icp[0]) = 0.;
  return 0;
}

// These formulations already supply their Jacobian.
template <class Eval>
int supplied(const iap_type *iap, const rap_type *rap, integer ndim,
             const doublereal *u, const doublereal *uold, const integer *icp,
             doublereal *par, integer ijac, doublereal *f, doublereal *dfdu,
             doublereal *dfdp, Eval eval) {
  eval(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp,
       iap->ndm, iap->run->scratch.dfu.data(), iap->run->scratch.dfp.data());
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
  std::vector<doublereal> dfu(xpp::size_product(iap->ndim, xpp::size_sum(iap->ndim, iap->run->parameter_count)));
  const auto evaluate = [&](const doublereal *state, doublereal *value) {
    eval(iap, rap, ndim, par, icp, nint, iap->nnt0, state, uold, udot,
         upold, value, dfu.data());
  };
  return differentiate(ndim, nint, u, icp, par, ijac, f, dint, dint,
                       iap->nfpr, ndim, uu1.data(), uu2.data(),
                       ff1.data(), ff2.data(), evaluate);
}

}  // namespace xpp::autox_jacobian
