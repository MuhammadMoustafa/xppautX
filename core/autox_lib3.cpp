#include "xpp_mem.h"
#include "autox_unknowns.h"
/* translated from autoX's autox_lib3.f by f2c */

#include <array>
#include <vector>
#include "xpp_log.h"
#include "session.h"
#include "autox_c.h"
#include "autox_jacobian.h"
#include "xpp_math.h"
#include "solver.h"

namespace {
// Original amplitude of the torus restart's added eigenvector states.
constexpr double torus_initial_amplitude = 1e-4;
} // namespace

/* Reused finite-difference scratch belongs to iap->run: the calling
   run's AutoxRun, or a collocation worker's private AutoxRun (W247).
   Rotation counts are computed once per run and only read by workers. */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*  Subroutines for the Continuation of Folds (Algebraic Problems) */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/* fnlp: AUTO's equations of the problem kind fold (toolboxae.f90:304 STPNLP: continuation of folds) */
int 
fold_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp)
{
  return xpp::autox_jacobian::curve(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp, false, fold_function);
} /* fnlp_ */

/* fflp: AUTO's function evaluation of the problem kind fold (toolboxae.f90:304 STPNLP: continuation of folds) */
int 
fold_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, std::ptrdiff_t ndm, double *dfdu, double *dfdp)
{
  std::ptrdiff_t dfdu_dim1;
  
  std::ptrdiff_t i, j;
  xpp::AutoxProblem ips;
  
  dfdu_dim1 = ndm;

  ips = iap->ips;

  par[icp[1]] = u[-1 + ndim];
  if (ips == xpp::AutoxProblem::DiscreteMap) {
    discrete_map_equations(iap, rap, ndm, u, uold, icp, par, ijac, f, dfdu, dfdp);
  } else {
    problem_function(iap, rap, ndm, u, uold, icp, par, ijac, f, dfdu, dfdp);
  }

  for (i = 0; i < ndm; ++i) {
    f[ndm + i] = 0.;
    for (j = 0; j < ndm; ++j) {
      f[ndm + i] += (ARRAY2D(dfdu, i, j) + (i==j && iap->itpst==xpp::AutoxPointKind::PeriodDoubling ? 2. : 0.)) * u[ndm + j];
    }
  }

  f[-1 + ndim] = -1.;

  for (i = 0; i < ndm; ++i) {
    f[-1 + ndim] += u[ndm + i] * u[ndm + i];
  }

  return 0;
} /* fflp_ */

/* stpnlp: AUTO's starting data of the problem kind fold (toolboxae.f90:304 STPNLP: continuation of folds) */
int 
fold_starting_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, double *u)
{
  std::ptrdiff_t ndim;

  double uold = 0.; /* problem_function/discrete_map_equations take it, unread */
  std::ptrdiff_t nfpr1;
  std::ptrdiff_t i;
  bool found;

  std::ptrdiff_t ndm, irs;
  xpp::AutoxProblem ips;

  std::vector<double> f(iap->ndim);
  std::vector<double> v(iap->ndim);
  /* Generates starting data for the continuation of folds. */

  ndim = iap->ndim;
  ips = iap->ips;
  irs = iap->irs;
  ndm = iap->ndm;

  find_restart_label(iap, rap, irs, &nfpr1, &found);
  read_restart_label(iap, rap, u, par);

  if (ips == xpp::AutoxProblem::DiscreteMap) {
    discrete_map_equations(iap, rap, ndm, u, &uold, icp, par, 1, f.data(), 
	 iap->run->scratch.dfu.data(), iap->run->scratch.dfp.data());
  } else {
    problem_function(iap, rap, ndm, u, &uold, icp, par, 1, f.data(), 
	 iap->run->scratch.dfu.data(), iap->run->scratch.dfp.data());
  }
  if (ips==xpp::AutoxProblem::DiscreteMap && iap->itp.is(xpp::AutoxPointKind::PeriodDoubling))
    for (std::ptrdiff_t k=0;k<ndm;++k) iap->run->scratch.dfu[k+ndm*k]+=2.;
  xpp::matrix_null_vector(iap->run->fp9, ndm, ndm, 1, iap->run->scratch.dfu.data(), v.data());
  normalize_vector(&ndm, v.data());
  for (i = 0; i < ndm; ++i) {
    u[ndm + i] = v[i];
  }
  u[-1 + ndim] = par[icp[1]];

  return 0;
} /* stpnlp_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*     Subroutines for the Optimization of Algebraic Systems */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/* fnc1: AUTO's equations of the problem kind optimization_single (optimization.f90:114 FNC1: optimization of algebraic systems (one parameter)) */
int 
optimization_single_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp)
{
  std::ptrdiff_t dfdu_dim1, dfdp_dim1;

  std::ptrdiff_t i, j;
  std::vector<double> ddp(iap->run->parameter_count);
  std::ptrdiff_t ndm;

  std::vector<double> ddu(iap->ndim);
  /* Generate the equations for the continuation scheme used for */
  /* the optimization of algebraic systems (one parameter). */

  dfdp_dim1 = ndim;
  dfdu_dim1 = ndim;
    
  ndm = iap->ndm;

  par[icp[1]] = u[-1 + ndim];
  problem_function(iap, rap, ndm, u, uold, icp, par, ijac, f, dfdu, dfdp);

  /* Rearrange (Since dimensions in FNC1 and FUNI differ). */

  if (ijac != 0) {
    for (j = ndm - 1; j >= 0; --j) {
      for (i = ndm - 1; i >= 0; --i) {
	ARRAY2D(dfdu, i, j) = dfdu[j * ndm + i];
      }
    }

    for (j = iap->run->parameter_count - 1; j >= 0; --j) {
      for (i = ndm - 1; i >= 0; --i) {
	ARRAY2D(dfdp, i, j) = dfdp[j * ndm + i];
      }
    }
  }

  objective_interface(iap, rap, ndm, u, icp, par, ijac, &f[-1 + ndim], ddu.data(), 
       ddp.data());
  f[-1 + ndim] = par[icp[0]] - f[-1 + ndim];

  if (ijac != 0) {
    for (i = 0; i < ndm; ++i) {
      ARRAY2D(dfdu, (ndim - 1), i) = -ddu[i];
      ARRAY2D(dfdu, i, (ndim - 1)) = ARRAY2D(dfdp, i, (icp[1]));
      ARRAY2D(dfdp, i, (icp[0])) = 0.;
    }
    ARRAY2D(dfdu, (ndim - 1), (ndim - 1)) = -ddp[icp[1]];
    ARRAY2D(dfdp, (ndim - 1), (icp[0])) = 1.;
  }
  return 0;
} /* fnc1_ */

/* stpnc1: AUTO's starting data of the problem kind optimization_single (optimization.f90:114 FNC1: optimization of algebraic systems (one parameter)) */
int 
optimization_single_starting_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, double *u)
{
  std::ptrdiff_t ndim;

  std::ptrdiff_t nfpr;

  std::ptrdiff_t ndm;
  double fop, dum;

  /* Generate starting data for optimization problems (one parameter). */

  ndim = iap->ndim;
  ndm = iap->ndm;

  user_start(*iap->run, ndim, 0.0, u, par);
  nfpr = 2;
  iap->nfpr = nfpr;
  objective_interface(iap, rap, ndm, u, icp, par, 0, &fop, &dum, &
       dum);
  par[icp[0]] = fop;
  u[-1 + ndim] = par[icp[1]];

  return 0;
} /* stpnc1_ */

/* fnc2: AUTO's equations of the problem kind optimization_multi (optimization.f90:191 FNC2: optimization of algebraic systems (more than one parameter)) */
int 
optimization_multi_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp)
{
  std::ptrdiff_t dfdu_dim1, dfdp_dim1;

  std::ptrdiff_t i;
  std::ptrdiff_t ndm;

  /* Generate the equations for the continuation scheme used for the */
  /* optimization of algebraic systems (more than one parameter). */

  dfdp_dim1 = ndim;
  dfdu_dim1 = ndim;
    
  ndm = iap->ndm;

  /* Generate the function. */

  optimization_multi_function(iap, rap, ndim, u, uold, icp, par, f, ndm, 
       iap->run->scratch.dfu.data(), iap->run->scratch.dfp.data());

  if (ijac == 0) {
    return 0;
  }

  /* Generate the Jacobian. */

  xpp::autox_jacobian::central(
      ndim, ndim, u, iap->run->scratch.uu1.data(), iap->run->scratch.uu2.data(), iap->run->scratch.ff1.data(), iap->run->scratch.ff2.data(),
      [&](const double *uu, double *ff) { optimization_multi_function(iap, rap, ndim, uu, uold, icp, par, ff, ndm, iap->run->scratch.dfu.data(), iap->run->scratch.dfp.data()); },
      dfdu, dfdu_dim1);

  for (i = 0; i < ndim; ++i) {
    ARRAY2D(dfdp, i, (icp[0])) = 0.;
  }
  ARRAY2D(dfdp, (ndim - 1), (icp[0])) = 1.;

  return 0;
} /* fnc2_ */

/* ffc2: AUTO's function evaluation of the problem kind optimization_multi (optimization.f90:191 FNC2: optimization of algebraic systems (more than one parameter)) */
int 
optimization_multi_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, double *f, std::ptrdiff_t ndm, double *dfdu, double *dfdp)
{
  std::ptrdiff_t dfdu_dim1, dfdp_dim1;

  std::ptrdiff_t icpm;

  std::ptrdiff_t nfpr, i, j;
  double fop;
  std::vector<double> ddp(iap->run->parameter_count);
  std::ptrdiff_t ndm2;

  std::vector<double> ddu(iap->ndim);

  dfdp_dim1 = ndm;
  dfdu_dim1 = ndm;
    
  nfpr = iap->nfpr;

  for (i = 1; i < nfpr; ++i) {
    par[icp[i]] = u[(ndm * 2) + i];
  }
  problem_function(iap, rap, ndm, u, uold, icp, par, 2, f, dfdu, dfdp);
  objective_interface(iap, rap, ndm, u, icp, par, 2, &fop, ddu.data(), ddp.data());

  for (i = 0; i < ndm; ++i) {
    f[ndm + i] = ddu[i] * u[(ndm * 2)];
    for (j = 0; j < ndm; ++j) {
      f[ndm + i] += ARRAY2D(dfdu, j, i) * u[ndm + j];
    }
  }

  ndm2 = ndm * 2;
  icpm = nfpr - 2;
  for (i = 0; i < icpm; ++i) {
    f[ndm2 + i] = ddp[icp[i + 1]] * u[ndm2];
  }

  for (i = 0; i < icpm; ++i) {
    for (j = 0; j < ndm; ++j) {
      f[ndm2 + i] += u[ndm + j] * ARRAY2D(dfdp, j, (icp[i + 1]));
    }
  }

  f[ndim - 2] = u[ndm2] * u[ndm2] - 1;
  for (j = 0; j < ndm; ++j) {
    f[ndim - 2] += u[ndm + j] * u[ndm + j];
  }
  f[-1 + ndim] = par[icp[0]] - fop;

  return 0;
} /* ffc2_ */

/* stpnc2: AUTO's starting data of the problem kind optimization_multi (optimization.f90:191 FNC2: optimization of algebraic systems (more than one parameter)) */
int 
optimization_multi_starting_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, double *u)
{

  std::ptrdiff_t ndim;

  double uold = 0.; /* problem_function/discrete_map_equations take it, unread */
  std::ptrdiff_t nfpr;
  std::ptrdiff_t i, j;
  bool found;

  std::vector<double> dp(iap->run->parameter_count);

  std::ptrdiff_t ndm;
  double fop;
  std::ptrdiff_t irs;

  std::vector<double> f(iap->ndim);
  std::vector<double> v(iap->ndim);
  std::vector<double> dd(xpp::size_product(iap->ndim, iap->ndim));
  std::vector<double> du(iap->ndim);
  /* Generates starting data for the continuation equations for */
  /* optimization of algebraic systems (More than one parameter). */

  ndim = iap->ndim;
  irs = iap->irs;
  ndm = iap->ndm;

  find_restart_label(iap, rap, irs, &nfpr, &found);
  ++nfpr;
  iap->nfpr = nfpr;
  read_restart_label(iap, rap, u, par);

  if (nfpr == 3) {
    problem_function(iap, rap, ndm, u, &uold, icp, par, 2, f.data(), 
	 iap->run->scratch.dfu.data(), iap->run->scratch.dfp.data());
    objective_interface(iap, rap, ndm, u, icp, par, 2, &fop, du.data(), 
	 dp.data());
    /*       TRANSPOSE */
    for (i = 0; i < ndm; ++i) {
      for (j = 0; j < ndm; ++j) {
	dd[i + j * ndim] = iap->run->scratch.dfu[i * ndm + j];
      }
    }
    for (i = 0; i < ndm; ++i) {
      dd[i + ndm * ndim] = du[i];
      dd[ndm + i * ndim] = iap->run->scratch.dfp[(icp[1]) * ndm + i];
    }
    dd[ndm + ndm * ndim] = dp[icp[1]];
    xpp::matrix_null_vector(iap->run->fp9, ndm + 1, ndim, 1, dd.data(), v.data());
    {
      std::ptrdiff_t tmp = ndm + 1;
      normalize_vector(&tmp, v.data());
    }
    for (i = 0; i < ndm + 1; ++i) {
      u[ndm + i] = v[i];
    }
    par[icp[0]] = fop;
  }

  for (i = 0; i < nfpr - 1; ++i) {
    u[ndim - nfpr + 1 + i] = par[icp[i + 1]];
  }

  return 0;
} /* stpnc2_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*        Subroutines for Discrete Dynamical Systems */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/* fnds: AUTO's equations of the problem kind discrete_map (maps.f90:62 FNDS: continuing fixed points of a map) */
int 
discrete_map_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp)
{
  std::ptrdiff_t dfdu_dim1;

  std::ptrdiff_t i;

  /* Generate the equations for continuing fixed points. */

  dfdu_dim1 = ndim;
    
  problem_function(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp);

  for (i = 0; i < ndim; ++i) {
    f[i] -= u[i];
  }

  if (ijac == 0) {
    return 0;
  }

  for (i = 0; i < ndim; ++i) {
    --ARRAY2D(dfdu, i, i);
  }

  return 0;
} /* fnds_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*        Subroutines for Time Integration of ODEs */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/* fnti: AUTO's equations of the problem kind time_integration (timeint.f90:37 FNTI: time integration) */
int 
time_integration_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp)
{
  std::ptrdiff_t dfdu_dim1, dfdp_dim1;

  double told;
  std::ptrdiff_t i, j;
  double dt;

  /* Generate the equations for continuing fixed points. */

  dfdp_dim1 = ndim;
  dfdu_dim1 = ndim;
    
  problem_function(iap, rap, ndim, u, uold, icp, par, ijac, f, 
       dfdu, dfdp);

  told = rap->tivp;
  dt = par[icp[0]] - told;

  for (i = 0; i < ndim; ++i) {
    ARRAY2D(dfdp, i, (icp[0])) = f[i];
    f[i] = dt * f[i] - u[i] + uold[i];
  }

  if (ijac == 0) {
    return 0;
  }

  for (i = 0; i < ndim; ++i) {
    for (j = 0; j < ndim; ++j) {
      ARRAY2D(dfdu, i, j) = dt * ARRAY2D(dfdu, i, j);
    }
    ARRAY2D(dfdu, i, i) += -1.;
  }

  return 0;
} /* time_integration_equations */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*     Subroutines for Hopf (ODE) and Torus (Map) Curves */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/* fnhb: AUTO's equations of the problem kind hopf (equilibrium.f90:67 FNHB: 2-parameter continuation of Hopf points in ODEs and maps) */
int 
hopf_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp)
{
  return xpp::autox_jacobian::curve(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp, true, hopf_function);
} /* fnhb_ */

/* ffhb: AUTO's function evaluation of the problem kind hopf (equilibrium.f90:67 FNHB: 2-parameter continuation of Hopf points in ODEs and maps) */
int 
hopf_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, std::ptrdiff_t ndm, double *dfdu, double *dfdp)
{
  // AUTO-07p equilibrium.f90:190-258, Kuznetsov (10.80), reimplemented:
  // State = [equilibrium, real vector, omega^2, second parameter].
  // For maps use theta and B=A-I: B^2+2(1-cos(theta))*(B+I),
  // reimplemented from maps.f90 FFNSX (Kuznetsov 10.83).
  par[icp[1]] = u[ndim - 1];
  const bool map=iap->ips==xpp::AutoxProblem::DiscreteMap;
  if (map) discrete_map_equations(iap, rap, ndm, u, uold, icp, par, ijac, f, dfdu, dfdp);
  else problem_function(iap, rap, ndm, u, uold, icp, par, ijac, f, dfdu, dfdp);
  const xpp::Matrix<double> blocks({dfdu, static_cast<size_t>(ndm * ndim)}, ndm, ndim);
  const double kappa = u[2 * ndm];
  const double shift=map ? 2*(1-xpp::math::cos(kappa)) : kappa;
  // The square goes into columns ndm..2*ndm-1, which share nothing with the Jacobian's;
  // each of those starts as a sum with 0., so none is -0 (product_accumulate's precondition).
  for (std::ptrdiff_t i = 0; i < ndm; ++i)
    for (std::ptrdiff_t j = 0; j < ndm; ++j)
      blocks(i, ndm + j) = (i == j ? shift : 0.) + (map ? shift*blocks(i,j) : 0.);
  xpp::product_accumulate(&blocks(0, ndm), ndm, &blocks(0, 0), ndm, &blocks(0, 0), ndm, ndm, ndm, ndm);
  for (std::ptrdiff_t i = 0; i < ndm; ++i) {
    blocks(i, 2 * ndm) = map ? 2*xpp::math::sin(kappa)*u[ndm+i] : u[ndm+i];
    if (map) for (std::ptrdiff_t j=0;j<ndm;++j) blocks(i,2*ndm)+=2*xpp::math::sin(kappa)*blocks(i,j)*u[ndm+j];
    blocks(i, ndim - 1) = 0.;
    for (std::ptrdiff_t j = 0; j < ndm; ++j)
      blocks(i, ndim - 1) += (blocks(j, i) - blocks(i, j)) * uold[ndm + j];
  }
  f[2 * ndm] = -1.;
  f[ndim - 1] = 0.;
  for (std::ptrdiff_t i = 0; i < ndm; ++i) {
    f[ndm + i] = 0.;
    for (std::ptrdiff_t j = 0; j < ndm; ++j) f[ndm + i] += blocks(i, ndm + j) * u[ndm + j];
    f[2 * ndm] += u[ndm + i] * u[ndm + i];
    f[ndim - 1] += blocks(i, ndim - 1) * u[ndm + i];
  }
  return 0;
} /* ffhb_ */

/* stpnhb: AUTO's starting data of the problem kind hopf (equilibrium.f90:67 FNHB: 2-parameter continuation of Hopf points in ODEs and maps) */
int 
hopf_starting_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, double *u)
{
  // AUTO-07p equilibrium.f90:278-323: a unit null vector of A^2+kappa I.
  std::ptrdiff_t free = 0;
  bool found = false;
  find_restart_label(iap, rap, iap->irs, &free, &found);
  if (!found) autox_restart_label_missing(iap);
  read_restart_label(iap, rap, u, par);
  const std::ptrdiff_t n = iap->ndm;
  const double omega = multiple_of_pi(2.) / par[xpp::autox_unknown::period];
  u[2 * n] = iap->ips==xpp::AutoxProblem::DiscreteMap ? omega : omega * omega;
  u[iap->ndim - 1] = par[icp[1]];
  std::vector<double> old(iap->ndim), residual(iap->ndim);
  std::vector<double> blocks(xpp::size_product(n, iap->ndim)), parameters(xpp::size_product(n, iap->run->parameter_count));
  hopf_function(iap, rap, iap->ndim, u, old.data(), icp, par, 2, residual.data(), n, blocks.data(), parameters.data());
  std::ptrdiff_t size = n;
  // The Hopf pair gives a two-dimensional kernel. NLVC chooses its phase;
  // subsequent correctors use the previous vector's symmetric condition.
  xpp::matrix_null_vector(iap->run->fp9, n, n, 2, blocks.data() + n * n, u + n);
  normalize_vector(&size, u + n);
  return 0;
} /* stpnhb_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*   Subroutines for the Continuation of Hopf Bifurcation Points (Waves) */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/* fnhw: AUTO's equations of the problem kind hopf_wave (parabolic.f90:234 FNHW: 2-parameter continuation of Hopf points in waves) */
int 
hopf_wave_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp)
{
  return xpp::autox_jacobian::problem(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp, xpp::autox_jacobian::principal_parameter_count, hopf_wave_function);
} /* fnhw_ */

/* ffhw: AUTO's function evaluation of the problem kind hopf_wave (parabolic.f90:234 FNHW: 2-parameter continuation of Hopf points in waves) */
int 
hopf_wave_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, double *f, std::ptrdiff_t ndm, double *dfdu, double *dfdp)
{
  std::ptrdiff_t dfdu_dim1;

  std::ptrdiff_t ijac;

  std::ptrdiff_t i, j;
  double rom;
  std::ptrdiff_t ndm2;

  dfdu_dim1 = ndm;
    
  ndm2 = ndm * 2;

  rom = u[-1 + ndim - 1];
  par[icp[1]] = u[-1 + ndim];
  ijac = 1;
  wave_steady_equations(iap, rap, ndm, u, uold, icp, par, ijac, f, 
       dfdu, dfdp);

  for (i = 0; i < ndm; ++i) {
    f[ndm + i] = u[ndm2 + i];
    f[ndm2 + i] = -u[ndm + i];
    for (j = 0; j < ndm; ++j) {
      f[ndm + i] += rom * ARRAY2D(dfdu, i, j) * u[ndm + j];
      f[ndm2 + i] += rom * ARRAY2D(dfdu, i, j) * u[ndm2 + j];
    }
  }

  f[ndim - 2] = -1.;

  for (i = 0; i < ndm; ++i) {
    f[ndim - 2] = f[ndim - 2] + u[ndm + i] * u[ndm + i] + u[ndm2 + i] * u[ndm2 + i];
  }

  f[-1 + ndim] = 0.;

  for (i = 0; i < ndm; ++i) {
    f[-1 + ndim] = f[-1 + ndim] + uold[ndm2 + i] * (u[ndm + i] - uold[ndm + i]) - uold[ndm + i] * (u[ndm2 + i] - uold[ndm2 + i]);
  }

  return 0;
} /* ffhw_ */

/* stpnhw: AUTO's starting data of the problem kind hopf_wave (parabolic.f90:234 FNHW: 2-parameter continuation of Hopf points in waves) */
int 
hopf_wave_starting_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, double *u)
{
  std::ptrdiff_t ijac, ndim;

  double uold = 0.;

  std::ptrdiff_t nfpr1;
  std::ptrdiff_t i, j;
  bool found;

  double period;
  std::ptrdiff_t ndm, irs;
  double rom;
  std::ptrdiff_t ndm2;

  std::vector<double> smat(xpp::size_product(xpp::size_product(2, iap->ndim), xpp::size_product(2, iap->ndim)));
  std::vector<double> f(iap->ndim);
  std::vector<double> v(iap->ndim);
  std::vector<double> dfp(xpp::size_product(iap->ndim, iap->run->parameter_count));
  std::vector<double> dfu(xpp::size_product(iap->ndim, iap->ndim));

  /* Generates starting data for the continuation of a bifurcation to a */
  /* traveling wave. */

  /* Local (Can't use BLLOC here.) */

  ndim = iap->ndim;
  irs = iap->irs;
  ndm = iap->ndm;

  find_restart_label(iap, rap, irs, &nfpr1, &found);
  read_restart_label(iap, rap, u, par);

  ijac = 1;
  period = par[xpp::autox_unknown::period];
  rom = period / multiple_of_pi(2.0);
  wave_steady_equations(iap, rap, ndm, u, &uold, icp, par, ijac, f.data(), dfu.data(), 
       dfp.data());

  ndm2 = ndm * 2;
  for (i = 0; i < ndm2; ++i) {
    for (j = 0; j < ndm2; ++j) {
      smat[i + j * (ndim*2)] = 0.;
    }
  }

  for (i = 0; i < ndm; ++i) {
    smat[i + (ndm + i) * (ndim*2)] = 1.;
  }

  for (i = 0; i < ndm; ++i) {
    smat[ndm + i + i * (ndim*2)] = -1.;
  }

  for (i = 0; i < ndm; ++i) {
    for (j = 0; j < ndm; ++j) {
      smat[i + j * (ndim*2)] = rom * iap->run->scratch.dfu[j * ndm + i];
      smat[ndm + i + (ndm + j) * (ndim*2)] = rom * iap->run->scratch.dfu[j * ndm + i];
    }
  }
  {
    std::ptrdiff_t tmp=(ndim*2);
    xpp::matrix_null_vector(iap->run->fp9, ndm2, tmp, 2, smat.data(), v.data());
  }
  normalize_vector(&ndm2, v.data());

  for (i = 0; i < ndm2; ++i) {
    u[ndm + i] = v[i];
  }

  u[ndim - 2] = rom;
  u[-1 + ndim] = par[icp[1]];

  return 0;
} /* stpnhw_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*          Periodic Solutions and Fixed Period Orbits */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/* fnps: AUTO's equations of the problem kind periodic (periodic.f90:352 STPNPS: continuation of periodic solutions) */
int 
periodic_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp)
{
  std::ptrdiff_t dfdu_dim1, dfdp_dim1;

  std::ptrdiff_t i, j;
  double period;

/* Generates the equations for the continuation of periodic orbits. */

/* Generate the function. */

  dfdp_dim1 = ndim;
  dfdu_dim1 = ndim;
    
  if (icp[1] == xpp::autox_unknown::period) {
    /*          **Variable period continuation */
    problem_function(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp);
    period = par[xpp::autox_unknown::period];
    for (i = 0; i < ndim; ++i) {
      ARRAY2D(dfdp, i, xpp::autox_unknown::period) = f[i];
      f[i] = period * ARRAY2D(dfdp, i, xpp::autox_unknown::period);
    }
    if (ijac == 0) {
      return 0;
    }
    /*          **Generate the Jacobian. */
    for (i = 0; i < ndim; ++i) {
      for (j = 0; j < ndim; ++j) {
	ARRAY2D(dfdu, i, j) = period * ARRAY2D(dfdu, i, j);
      }
      ARRAY2D(dfdp, i, (icp[0])) = period * ARRAY2D(dfdp, i, (icp[0]));
    }
  } else {
    /*          **Fixed period continuation */
    period = par[xpp::autox_unknown::period];
    problem_function(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp);
    for (i = 0; i < ndim; ++i) {
      f[i] = period * f[i];
    }
    if (ijac == 0) {
      return 0;
    }
    /*          **Generate the Jacobian. */
    for (i = 0; i < ndim; ++i) {
      for (j = 0; j < ndim; ++j) {
	ARRAY2D(dfdu, i, j) = period * ARRAY2D(dfdu, i, j);
      }
      for (j = 0; j < 2; ++j) {
	ARRAY2D(dfdp, i, icp[j]) = period * ARRAY2D(dfdp, i, icp[j]);
      }
    }
  }

  return 0;
} /* fnps_ */

/* bcps: AUTO's boundary conditions of the problem kind periodic (periodic.f90:352 STPNPS: continuation of periodic solutions) */
int 
periodic_boundary_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nbc, const double *u0, const double *u1, double *f, std::ptrdiff_t ijac, double *dbc)
{
  std::ptrdiff_t dbc_dim1;

  std::ptrdiff_t jtmp, i, j, nn;

  dbc_dim1 = nbc;
  
  for (i = 0; i < ndim; ++i) {
    f[i] = u0[i] - u1[i];
  }

  /* Rotations */
  if (iap->run->rotations.irtn != 0) {
    for (i = 0; i < ndim; ++i) {
      if (iap->run->rotations.nrtn[i] != 0) {
	f[i] += par[xpp::autox_unknown::rotation_period] * iap->run->rotations.nrtn[i];
      }
    }
  }

  if (ijac == 0) {
    return 0;
  }

  jtmp = iap->run->parameter_count;
  nn = (ndim * 2) + jtmp;
  for (i = 0; i < nbc; ++i) {
    for (j = 0; j < nn; ++j) {
      ARRAY2D(dbc, i, j) = 0.;
    }
  }

  for (i = 0; i < ndim; ++i) {
    ARRAY2D(dbc, i, i) = 1.;
    ARRAY2D(dbc, i, (ndim + i)) = -1.;
  }

  return 0;
} /* bcps_ */

/* icps: AUTO's integral conditions of the problem kind periodic (periodic.f90:352 STPNPS: continuation of periodic solutions) */
int 
periodic_integral_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nint, const double *u, const double *uold, const double *udot, const double *upold, double *f, std::ptrdiff_t ijac, double *dint)
{
  std::ptrdiff_t dint_dim1;

  std::ptrdiff_t jtmp, i, nn;

  dint_dim1 = nint;
  
  // Center on the computed orbit: discrete quadrature need not make uold dot upold zero.
  f[0] = 0.;
  for (i = 0; i < ndim; ++i) {
    if (iap->run->rotations.nrtn[i] == 0) {
      f[0] += (u[i] - uold[i]) * upold[i];
    }
  }

  if (ijac == 0) {
    return 0;
  }

  jtmp = iap->run->parameter_count;
  nn = ndim + jtmp;
  for (i = 0; i < nn; ++i) {
    ARRAY2D(dint, 0, i) = 0.;
  }

  for (i = 0; i < ndim; ++i) {
    if (iap->run->rotations.nrtn[i] == 0) {
      ARRAY2D(dint, 0, i) = upold[i];
    } else {
      ARRAY2D(dint, 0, i) = 0.;
    }
  }

  return 0;
} /* icps_ */

/* pdble: AUTO periodic.f90:321: preprocesses restart data for switching branches at a period doubling */
int 
period_doubling_restart(const iap_type *iap, const rap_type *rap, std::ptrdiff_t *ndim, std::ptrdiff_t *ntst, std::ptrdiff_t *ncol, std::ptrdiff_t *ndxloc, double *ups, double *udotps, double *tm, double *par)
{
  std::ptrdiff_t ups_dim1, udotps_dim1;

  std::ptrdiff_t i, j, i1, i2;

  /* Preprocesses restart data for switching branches at a period doubling */

  udotps_dim1 = *ndxloc;
  ups_dim1 = *ndxloc;
    
  par[xpp::autox_unknown::period] *= 2.;
  if (iap->run->rotations.irtn != 0) {
    par[xpp::autox_unknown::rotation_period] *= 2.;
  }

  for (i = 0; i < *ntst; ++i) {
    tm[i] *= .5;
    tm[*ntst + i] = tm[i] + .5;
  }

  tm[(*ntst * 2)] = 1.;

  for (j = 0; j < *ntst + 1; ++j) {
    for (i1 = 0; i1 < *ndim; ++i1) {
      for (i2 = 0; i2 < *ncol; ++i2) {
	i = i2 * *ndim + i1;
	ARRAY2D(ups, *ntst + j, i) = 
	  ARRAY2D(ups, *ntst, i1) + 
	  ARRAY2D(ups, j, i) - 
	  ARRAY2D(ups, 0, i1);
	ARRAY2D(udotps, *ntst + j, i) = 
	  ARRAY2D(udotps, *ntst, i1) + 
	  ARRAY2D(udotps, j, i) - 
	  ARRAY2D(udotps, 0, i);
      }
    }
  }

  *ntst *= 2;

  return 0;
} /* pdble_ */

/* stpnps: AUTO's starting data of the problem kind periodic (periodic.f90:352 STPNPS: continuation of periodic solutions) */
int 
periodic_starting_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, std::ptrdiff_t *ntsr, std::ptrdiff_t *ncolrs, double *rlcur, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *udotps, double *upoldp, double *tm, double *dtm, std::ptrdiff_t *nodir, double *thl, double *thu)
{

  std::ptrdiff_t ndim;

  double uold = 0.;
  std::ptrdiff_t nfpr, ndim2, nfpr1;

  std::ptrdiff_t i, j;
  double rimhb;
  bool found;

  double period;

  double tpi;
  std::ptrdiff_t irs;

  std::vector<double> smat(xpp::size_product(xpp::size_product(iap->ndim, 2), xpp::size_product(iap->ndim, 2)));
  std::vector<double> rnllv(xpp::size_product(xpp::size_product(iap->ndim, 2), xpp::size_product(iap->ndim, 2)));
  std::vector<double> f(iap->ndim);
  std::vector<double> u(iap->ndim);
  /* Generates starting data for the continuation of a branch of periodic */
  /* solutions from a Hopf bifurcation point. */

  ndim = iap->ndim;
  irs = iap->irs;
  nfpr = iap->nfpr;

  find_restart_label(iap, rap, irs, &nfpr1, &found);
  read_restart_label(iap, rap, u.data(), par);

  for (i = 0; i < nfpr; ++i) {
    rlcur[i] = par[icp[i]];
  }

  period = par[xpp::autox_unknown::period];
  tpi = multiple_of_pi(2.0);
  rimhb = tpi / period;
  *ntsr = iap->ntst;
  *ncolrs = iap->ncol;

  ndim2 = ndim * 2;
  for (i = 0; i < ndim2; ++i) {
    for (j = 0; j < ndim2; ++j) {
      smat[i + j * (ndim*2)] = 0.;
    }
  }

  for (i = 0; i < ndim; ++i) {
    smat[i + i * (ndim*2)] = -rimhb;
    smat[ndim + i + (ndim + i) * (ndim*2)] = rimhb;
  }

  problem_function(iap, rap, ndim, u.data(), &uold, icp, par, 1, f.data(), 
       iap->run->scratch.dfu.data(), iap->run->scratch.dfp.data());

  for (i = 0; i < ndim; ++i) {
    for (j = 0; j < ndim; ++j) {
      smat[i + (ndim + j) * (ndim*2)] = iap->run->scratch.dfu[j * ndim + i];
      smat[ndim + i + j * (ndim*2)] = iap->run->scratch.dfu[j * ndim + i];
    }
  }

  {
    std::ptrdiff_t tmp=(ndim*2);
    xpp::matrix_null_vector(iap->run->fp9, ndim2, tmp, 2, smat.data(), rnllv.data());
  }
  normalize_vector(&ndim2, rnllv.data());

/* Generate the (initially uniform) mesh. */

  autox_hopf_mesh(iap, rap, *ndxloc, ups, udotps, upoldp, tm, dtm,
                 rldot, u.data(), rnllv.data(), tpi);

  scale_direction(iap, icp, ndxloc, udotps, rldot, dtm, thl, thu);

  *nodir = -1;

  return 0;
} /* stpnps_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*          Travelling Wave Solutions to Parabolic PDEs */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/* fnws: AUTO's equations of the problem kind wave_steady (parabolic.f90:110 FNWS: spatially homogeneous solutions of parabolic systems, to find travelling-wave bifurcations (IPS 11)) */
int 
wave_steady_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp)
{

  std::ptrdiff_t ndm, ndm2;

  /* Sets up equations for the continuation of spatially homogeneous */
  /* solutions to parabolic systems, for the purpose of finding */
  /* bifurcations to travelling wave solutions. */

  ndm = iap->ndm;

  /* Generate the function. */

  ndm2 = ndm / 2;
  wave_steady_function(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp, ndm2, iap->run->scratch.dfu.data(), 
       iap->run->scratch.dfp.data());

  return 0;
} /* fnws_ */

/* ffws: AUTO's function evaluation of the problem kind wave_steady (parabolic.f90:110 FNWS: spatially homogeneous solutions of parabolic systems, to find travelling-wave bifurcations (IPS 11)) */
int 
wave_steady_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp, std::ptrdiff_t ndm, double *dfu, double *dfp)
{
  std::ptrdiff_t dfdu_dim1, dfdp_dim1, dfu_dim1, dfp_dim1;

  std::ptrdiff_t nfpr;
  double c;
  std::ptrdiff_t i, j;

  dfdp_dim1 = ndim;
  dfdu_dim1 = ndim;
  dfp_dim1 = ndm;
  dfu_dim1 = ndm;
    
  nfpr = iap->nfpr;

  c = par[xpp::autox_unknown::wave_speed];
  problem_function(iap, rap, ndm, u, uold, icp, par, ijac, f, dfu, dfp);

  for (i = 0; i < ndm; ++i) {
    f[ndm + i] = -(c * u[ndm + i] + f[i]) / par[i + xpp::autox_unknown::diffusion_start];
    f[i] = u[ndm + i];
  }

  if (ijac == 0) {
    return 0;
  }

  for (i = 0; i < ndm; ++i) {
    for (j = 0; j < ndm; ++j) {
      ARRAY2D(dfdu, i, j) = 0.;
      ARRAY2D(dfdu, i, (j + ndm)) = 0.;
      ARRAY2D(dfdu, i + ndm, j) = -ARRAY2D(dfu, i, j) / par[i + xpp::autox_unknown::diffusion_start];
      ARRAY2D(dfdu, i + ndm, (j + ndm)) = 0.;
    }
    ARRAY2D(dfdu, i, (i + ndm)) = 1.;
    ARRAY2D(dfdu, i + ndm, (i + ndm)) = -c / par[i + xpp::autox_unknown::diffusion_start];
    if (autox_model_parameter(*iap->run->session,icp[0])) {
      ARRAY2D(dfdp, i, (icp[0])) = 0.;
      ARRAY2D(dfdp, i + ndm, icp[0]) = -ARRAY2D(dfp, i, icp[0]) / par[i + xpp::autox_unknown::diffusion_start];
    }
    if (nfpr > 1 && autox_model_parameter(*iap->run->session,icp[1])) {
      ARRAY2D(dfdp, i, (icp[1])) = 0.;
      ARRAY2D(dfdp, i + ndm, icp[1]) = -ARRAY2D(dfp, i, icp[1]) / par[i + xpp::autox_unknown::diffusion_start];
    }
  }

  /* Derivative with respect to the wave speed. */

  for (i = 0; i < ndm; ++i) {
    ARRAY2D(dfdp, i, xpp::autox_unknown::wave_speed) = 0.;
    ARRAY2D(dfdp, i + ndm, xpp::autox_unknown::wave_speed) = -u[ndm + i] / par[i + xpp::autox_unknown::diffusion_start];
  }

  /* Derivatives with respect to the diffusion coefficients. */

  for (j = 0; j < ndm; ++j) {
    for (i = 0; i < ndm; ++i) {
      ARRAY2D(dfdp, i, (j + xpp::autox_unknown::diffusion_start)) = 0.;
      ARRAY2D(dfdp, i + ndm, (j + xpp::autox_unknown::diffusion_start)) = 0.;
    }
    ARRAY2D(dfdp, j + ndm, (j + xpp::autox_unknown::diffusion_start)) = -f[j + ndm] / par[j + xpp::autox_unknown::diffusion_start];
  }

  return 0;
} /* ffws_ */

/* fnwp: AUTO's equations of the problem kind wave_train (XPPAUT 8.0 autlib3.c:1018: continuation of travelling waves (IPS 12)) */
int 
wave_train_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp)
{
  std::ptrdiff_t dfdu_dim1, dfdp_dim1;

  std::ptrdiff_t i, j;
  double period;

/* Equations for the continuation of traveling waves. */

/* Generate the function and Jacobian. */

  dfdp_dim1 = ndim;
  dfdu_dim1 = ndim;
    
  if (icp[1] == xpp::autox_unknown::period) {
    /*          **Variable wave length */
    wave_steady_equations(iap, rap, ndim, u, uold, icp, par, ijac, f, 
	 dfdu, dfdp);
    period = par[xpp::autox_unknown::period];
    for (i = 0; i < ndim; ++i) {
      ARRAY2D(dfdp, i, xpp::autox_unknown::period) = f[i];
      f[i] = period * f[i];
    }
    if (ijac == 0) {
      return 0;
    }
    for (i = 0; i < ndim; ++i) {
      for (j = 0; j < ndim; ++j) {
	ARRAY2D(dfdu, i, j) = period * ARRAY2D(dfdu, i, j);
      }
    }
    for (i = 0; i < ndim; ++i) {
      ARRAY2D(dfdp, i, (icp[0])) = period * ARRAY2D(dfdp, i, (icp[0]));
    }
  } else {
    /*          **Fixed wave length */
    wave_steady_equations(iap, rap, ndim, u, uold, icp, par, ijac, f, 
	 dfdu, dfdp);
    period = par[xpp::autox_unknown::period];
    for (i = 0; i < ndim; ++i) {
      f[i] = period * f[i];
    }
    if (ijac == 0) {
      return 0;
    }
    for (i = 0; i < ndim; ++i) {
      for (j = 0; j < ndim; ++j) {
	ARRAY2D(dfdu, i, j) = period * ARRAY2D(dfdu, i, j);
      }
    }
    for (i = 0; i < ndim; ++i) {
      for (j = 0; j < 2; ++j) {
	ARRAY2D(dfdp, i, icp[j]) = period * ARRAY2D(dfdp, i, icp[j]);
      }
    }
  }

  return 0;
} /* fnwp_ */

/* stpnwp: AUTO's starting data of the problem kind wave_train (XPPAUT 8.0 autlib3.c:1018: continuation of travelling waves (IPS 12)) */
int 
wave_train_starting_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, std::ptrdiff_t *ntsr, std::ptrdiff_t *ncolrs, double *rlcur, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *udotps, double *upoldp, double *tm, double *dtm, std::ptrdiff_t *nodir, double *thl, double *thu)
{

  std::ptrdiff_t ndim;

  double uold = 0.;
  std::ptrdiff_t nfpr;

  std::ptrdiff_t ndim2, nfpr1;

  std::ptrdiff_t i, j;
  double rimhb;
  bool found;

  double period;

  double tpi;
  std::ptrdiff_t irs;

  std::vector<double> smat(xpp::size_product(xpp::size_product(2, iap->ndim), xpp::size_product(2, iap->ndim)));
  std::vector<double> f(iap->ndim);
  std::vector<double> u(iap->ndim);
  std::vector<double> rnllv(xpp::size_product(2, iap->ndim));
  std::vector<double> dfp(xpp::size_product(iap->ndim, iap->run->parameter_count));
  std::vector<double> dfu(xpp::size_product(iap->ndim, iap->ndim));

/* Generates starting data for the continuation of a branch of periodic */
/* solutions starting from a Hopf bifurcation point (Waves). */

/* Local (Can't use BLLOC here.) */

  ndim = iap->ndim;
  irs = iap->irs;
  nfpr = iap->nfpr;

  find_restart_label(iap, rap, irs, &nfpr1, &found);
  read_restart_label(iap, rap, u.data(), par);

  for (i = 0; i < nfpr; ++i) {
    rlcur[i] = par[icp[i]];
  }

  period = par[xpp::autox_unknown::period];
  tpi = multiple_of_pi(2.0);
  rimhb = tpi / period;
  *ntsr = iap->ntst;
  *ncolrs = iap->ncol;

  ndim2 = ndim * 2;
  for (i = 0; i < ndim2; ++i) {
    for (j = 0; j < ndim2; ++j) {
      smat[i + j * (ndim*2)] = 0.;
    }
  }

  for (i = 0; i < ndim; ++i) {
    smat[i + i * (ndim*2)] = -rimhb;
    smat[ndim + i + (ndim + i) * (ndim*2)] = rimhb;
  }

  wave_steady_equations(iap, rap, ndim, u.data(), &uold, icp, par, 1, f.data(), dfu.data(), dfp.data());

  for (i = 0; i < ndim; ++i) {
    for (j = 0; j < ndim; ++j) {
      smat[i + (ndim + j) * (ndim*2)] = dfu[j * ndim + i];
      smat[ndim + i + j * (ndim*2)] = dfu[j * ndim + i];
    }
  }

  xpp::matrix_null_vector(iap->run->fp9, ndim2, ndim*2, 2, smat.data(), rnllv.data());
  normalize_vector(&ndim2, rnllv.data());

  /* Generate the (initially uniform) mesh. */

  autox_hopf_mesh(iap, rap, *ndxloc, ups, udotps, upoldp, tm, dtm,
                 rldot, u.data(), rnllv.data(), tpi);

  scale_direction(iap, icp, ndxloc, udotps, rldot, dtm, thl, thu);

  *nodir = -1;

  return 0;
} /* stpnwp_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*             Parabolic PDEs : Stationary States */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/* fnsp: AUTO's equations of the problem kind parabolic_stationary (autox_lib3.cpp:1157: parabolic PDEs, stationary states) */
int 
parabolic_stationary_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp)
{
  return xpp::autox_jacobian::supplied(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp, parabolic_stationary_function);
} /* fnsp_ */

/* ffsp: AUTO's function evaluation of the problem kind parabolic_stationary (autox_lib3.cpp:1157: parabolic PDEs, stationary states) */
int 
parabolic_stationary_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp, std::ptrdiff_t ndm, double *dfu, double *dfp)
{
  std::ptrdiff_t dfdu_dim1, dfdp_dim1, dfu_dim1, dfp_dim1;

  std::ptrdiff_t i, j;
  double period;

  dfdp_dim1 = ndim;
  dfdu_dim1 = ndim;
  dfp_dim1 = ndm;
  dfu_dim1 = ndm;
    
  problem_function(iap, rap, ndm, u, uold, icp, par, ijac, &f[ndm], dfu, dfp);

  period = par[xpp::autox_unknown::period];
  for (i = 0; i < ndm; ++i) {
    f[i] = period * u[ndm + i];
    f[ndm + i] = -period * f[ndm + i] / par[i + xpp::autox_unknown::diffusion_start];
  }

  if (ijac == 0) {
    return 0;
  }

  for (i = 0; i < ndm; ++i) {
    for (j = 0; j < ndm; ++j) {
      ARRAY2D(dfdu, i, j) = 0.;
      ARRAY2D(dfdu, i, (j + ndm)) = 0.;
      ARRAY2D(dfdu, i + ndm, j) = -period * ARRAY2D(dfu, i, j) / par[i + xpp::autox_unknown::diffusion_start];
      ARRAY2D(dfdu, i + ndm, (j + ndm)) = 0.;
    }
    ARRAY2D(dfdu, i, (i + ndm)) = period;
    if (icp[0] == xpp::autox_unknown::period) {
      ARRAY2D(dfdp, i, (icp[0])) = f[i] / period;
      ARRAY2D(dfdp, ndm + i, icp[0]) = f[ndm + i] / period;
    } else if (icp[0] == i + xpp::autox_unknown::diffusion_start - 1) {
      ARRAY2D(dfdp, i, (icp[0])) = 0.;
      ARRAY2D(dfdp, ndm + i, icp[0]) = -f[ndm + i] / par[i + xpp::autox_unknown::diffusion_start];
    } else if (icp[0] != xpp::autox_unknown::period && ! (icp[0] > xpp::autox_unknown::diffusion_start - 1 && icp[0] <= ndm + xpp::autox_unknown::diffusion_start - 1)) {
      ARRAY2D(dfdp, i, (icp[0])) = 0.;
      ARRAY2D(dfdp, i + ndm, icp[0]) = -period * ARRAY2D(dfp, i, icp[0]) / par[i + xpp::autox_unknown::diffusion_start];
    }
  }

  return 0;
} /* ffsp_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*            Time Evolution of Parabolic PDEs */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/* fnpe: AUTO's equations of the problem kind parabolic_evolution (parabolic.f90:389 FNPE: one implicit Euler time step; autox_lib3.cpp:1217: time evolution of parabolic PDEs) */
int 
parabolic_evolution_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp)
{
  return xpp::autox_jacobian::supplied(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp, parabolic_evolution_function);
} /* fnpe_ */

/* ffpe: AUTO's function evaluation of the problem kind parabolic_evolution (parabolic.f90:389 FNPE: one implicit Euler time step; autox_lib3.cpp:1217: time evolution of parabolic PDEs) */
int 
parabolic_evolution_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp, std::ptrdiff_t ndm, double *dfu, double *dfp)
{
  std::ptrdiff_t dfdu_dim1, dfdp_dim1, dfu_dim1;

  std::ptrdiff_t i, j;
  double t, dsmin, rlold, ds, dt, period;

  dfdp_dim1 = ndim;
  dfdu_dim1 = ndim;
  dfu_dim1 = ndm;
    
  ds = rap->ds;
  dsmin = rap->dsmin;

  period = par[xpp::autox_unknown::period];
  t = par[icp[0]];
  rlold = rap->tivp;
  dt = t - rlold;
  if (f2c::abs(dt) < dsmin) {
    dt = ds;
  }

  problem_function(iap, rap, ndm, u, uold, icp, par, ijac, &f[ndm], dfu, dfp);

  for (i = 0; i < ndm; ++i) {
    f[i] = period * u[ndm + i];
    f[ndm + i] = period * ((u[i] - uold[i]) / dt - f[ndm + i]) /par[i + xpp::autox_unknown::diffusion_start];
  }

  if (ijac == 0) {
    return 0;
  }

  for (i = 0; i < ndm; ++i) {
    for (j = 0; j < ndm; ++j) {
      ARRAY2D(dfdu, i, j) = 0.;
      ARRAY2D(dfdu, i, (j + ndm)) = 0.;
      ARRAY2D(dfdu, i + ndm, j) = -period * ARRAY2D(dfu, i, j) / par[i + xpp::autox_unknown::diffusion_start];
      ARRAY2D(dfdu, i + ndm, (j + ndm)) = 0.;
    }
    ARRAY2D(dfdu, i, (i + ndm)) = period;
    ARRAY2D(dfdu, i + ndm, i) += period / (dt * par[i + xpp::autox_unknown::diffusion_start]);
    ARRAY2D(dfdp, i, (icp[0])) = 0.;
    /* Computing 2nd power */
    ARRAY2D(dfdp, i + ndm, icp[0]) = -period * (u[i] - uold[i])/ (dt * dt * par[i + xpp::autox_unknown::diffusion_start]);
  }

  return 0;
} /* ffpe_ */

/* icpe: AUTO's integral conditions of the problem kind parabolic_evolution (parabolic.f90:389 FNPE: one implicit Euler time step; autox_lib3.cpp:1217: time evolution of parabolic PDEs) */
int 
parabolic_evolution_integral_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nint, const double *u, const double *uold, const double *udot, const double *upold, double *f, std::ptrdiff_t ijac, double *dint)
{

  /* Dummy integral condition subroutine for parabolic systems. */

  return 0;
} /* icpe_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*    Subroutines for the Continuation of Folds for Periodic Solution */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/* fnpl: AUTO's equations of the problem kind periodic_fold (periodic.f90:631 STPNPL: 2-parameter continuation of folds of periodic solutions) */
int 
periodic_fold_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp)
{
  // FNPLF's block structure (AUTO-07p periodic.f90:427-507): only
  // the derivatives of the variational equations need second differences.
  const std::ptrdiff_t n = iap->ndm;
  auto &scratch = iap->run->scratch;
  const auto evaluate = [&](const double *state, double *value, std::ptrdiff_t derivatives) {
    periodic_fold_function(iap, rap, ndim, state, uold, icp, par, derivatives, value, n,
         scratch.dfu.data(), scratch.dfp.data());
  };
  evaluate(u, f, ijac == 0 ? 1 : 2);
  if (ijac == 0) return 0;
  const double period = par[xpp::autox_unknown::period];
  const bool variable_period = icp[2] == xpp::autox_unknown::period;
  const xpp::Matrix<const double> a(scratch.dfu, n, n);
  const xpp::Matrix<const double> p(scratch.dfp, n, iap->run->parameter_count);
  const xpp::Matrix<double> jac({dfdu, xpp::size_product(ndim, ndim)}, ndim, ndim);
  const xpp::Matrix<double> dp({dfdp, xpp::size_product(ndim, iap->run->parameter_count)}, ndim, iap->run->parameter_count);
  std::fill_n(dfdu, ndim * ndim, 0.);
  std::fill_n(dfdp, ndim * iap->run->parameter_count, 0.);
  for (std::ptrdiff_t i = 0; i < n; ++i) {
    for (std::ptrdiff_t j = 0; j < n; ++j) {
      jac(i, j) = period * a(i, j);
      jac(n + i, n + j) = jac(i, j);
    }
    for (std::ptrdiff_t k = 0; k < iap->nfpr; ++k)
      dp(i, icp[k]) = period * p(i, icp[k]);
    if (variable_period) {
      dp(i, xpp::autox_unknown::period) = f[i] / period;
      dp(n + i, xpp::autox_unknown::period) =
          (f[n + i] - par[xpp::autox_unknown::fold_beta] * dp(i, xpp::autox_unknown::period)) / period;
    }
    dp(n + i, xpp::autox_unknown::fold_beta) =
        variable_period ? f[i] / period : p(i, icp[1]);
  }
  // central() copies the physical states; keep the unperturbed eigenvector
  // in the tail of both preallocated scratch states.
  std::copy_n(u, ndim, scratch.uu1.data());
  std::copy_n(u, ndim, scratch.uu2.data());
  const double step = xpp::autox_jacobian::central(
      n, n, u, scratch.uu1.data(), scratch.uu2.data(),
      scratch.ff1.data(), scratch.ff2.data(),
      [&](const double *state, double *value) {
        evaluate(state, value, 1);
        std::copy_n(value + n, n, value);
      },
      dfdu + n, ndim);
  if (ijac == 1) return 0;
  for (std::ptrdiff_t k = 0; k < iap->nfpr; ++k) {
    const std::ptrdiff_t parameter = icp[k];
    if (parameter == xpp::autox_unknown::period ||
        parameter == xpp::autox_unknown::fold_beta ||
        parameter == xpp::autox_unknown::fold_norm) continue;
    const double saved = par[parameter];
    xpp::autox_jacobian::forward_parameters(
        1, n, icp + k, step, par,
        [&](double *value) {
          evaluate(u, scratch.ff2.data(), 1);
          std::copy_n(scratch.ff2.data() + n, n, value);
        }, f + n, scratch.ff1.data(), dfdp + n, ndim, 0);
    par[parameter] = saved;
  }
  return 0;
} /* fnpl_ */

/* ffpl: AUTO's function evaluation of the problem kind periodic_fold (periodic.f90:631 STPNPL: 2-parameter continuation of folds of periodic solutions) */
int
periodic_fold_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, std::ptrdiff_t ndm, double *dfdu, double *dfdp)
{
  std::ptrdiff_t dfdu_dim1, dfdp_dim1;

  double beta;

  std::ptrdiff_t i, j;
  double period;

  dfdp_dim1 = ndm;
  dfdu_dim1 = ndm;
    
  period = par[xpp::autox_unknown::period];
  beta = par[xpp::autox_unknown::fold_beta];
  problem_function(iap, rap, ndm, u, uold, icp, par, icp[2] == xpp::autox_unknown::period ? ijac : 2, f,
       dfdu, dfdp);

  for (i = 0; i < ndm; ++i) {
    f[ndm + i] = 0.;
    for (j = 0; j < ndm; ++j) {
      f[ndm + i] += ARRAY2D(dfdu, i, j) * u[ndm + j];
    }
    if (icp[2] == xpp::autox_unknown::period) {
      /*            ** Variable period */
      f[ndm + i] = period * f[ndm + i] + beta * f[i];
    } else {
      /*            ** Fixed period */
      f[ndm + i] = period * f[ndm + i] + beta * ARRAY2D(dfdp, i, (icp[1]));
    }
    f[i] = period * f[i];
  }

  return 0;
} /* ffpl_ */

/* bcpl: AUTO's boundary conditions of the problem kind periodic_fold (periodic.f90:631 STPNPL: 2-parameter continuation of folds of periodic solutions) */
int 
periodic_fold_boundary_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nbc, const double *u0, const double *u1, double *f, std::ptrdiff_t ijac, double *dbc)
{
  std::ptrdiff_t dbc_dim1;
  std::ptrdiff_t jtmp, i, j, nn, ndm;

  /* Boundary conditions for continuing folds (Periodic solutions) */

  dbc_dim1 = nbc;
  
  for (i = 0; i < ndim; ++i) {
    f[i] = u0[i] - u1[i];
  }

  /* Rotations */
  if (iap->run->rotations.irtn != 0) {
    ndm = iap->ndm;
    for (i = 0; i < ndm; ++i) {
      if (iap->run->rotations.nrtn[i] != 0) {
	f[i] += par[xpp::autox_unknown::rotation_period] * iap->run->rotations.nrtn[i];
      }
    }
  }

  if (ijac == 0) {
    return 0;
  }

  jtmp = iap->run->parameter_count;
  nn = (ndim * 2) + jtmp;
  for (i = 0; i < nbc; ++i) {
    for (j = 0; j < nn; ++j) {
      ARRAY2D(dbc, i, j) = 0.;
    }
  }

  for (i = 0; i < ndim; ++i) {
    ARRAY2D(dbc, i, i) = 1.;
    ARRAY2D(dbc, i, (ndim + i)) = -1.;
  }

  return 0;
} /* bcpl_ */

/* icpl: AUTO's integral conditions of the problem kind periodic_fold (periodic.f90:631 STPNPL: 2-parameter continuation of folds of periodic solutions) */
int 
periodic_fold_integral_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nint, const double *u, const double *uold, const double *udot, const double *upold, double *f, std::ptrdiff_t ijac, double *dint)
{
  std::ptrdiff_t dint_dim1;

  std::ptrdiff_t jtmp, i, j, nn, ndm;

  /* Integral conditions for continuing folds (Periodic solutions) */

  dint_dim1 = nint;
  
  ndm = iap->ndm;

  periodic_integral_conditions(iap, rap, ndm, par, icp, nint, u, uold, udot, upold, f, 0, nullptr);
  f[1] = 0.;
  /* Computing 2nd power */
  f[2] = par[xpp::autox_unknown::fold_beta] * par[xpp::autox_unknown::fold_beta] - par[xpp::autox_unknown::fold_norm];

  for (i = 0; i < ndm; ++i) {
    if (iap->run->rotations.nrtn[i] == 0) {
      f[1] += u[ndm + i] * upold[i];
    }
    f[2] += u[ndm + i] * u[ndm + i];
  }

  if (ijac == 0) {
    return 0;
  }

  jtmp = iap->run->parameter_count;
  nn = ndim + jtmp;
  for (i = 0; i < nint; ++i) {
    for (j = 0; j < nn; ++j) {
      ARRAY2D(dint, i, j) = 0.;
    }
  }

  for (i = 0; i < ndm; ++i) {
    if (iap->run->rotations.nrtn[i] == 0) {
      ARRAY2D(dint, 0, i) = upold[i];
      ARRAY2D(dint, 1, ndm + i) = upold[i];
    } else {
      ARRAY2D(dint, 0, i) = 0.;
      ARRAY2D(dint, 1, ndm + i) = 0.;
    }
    ARRAY2D(dint, 2, ndm + i) = u[ndm + i] * 2.;
  }

  ARRAY2D(dint, 2, ndim + xpp::autox_unknown::fold_beta) = par[xpp::autox_unknown::fold_beta] * 2.;
  ARRAY2D(dint, 2, ndim + xpp::autox_unknown::fold_norm) = -1.;

  return 0;
} /* icpl_ */

/* stpnpl: AUTO's starting data of the problem kind periodic_fold (periodic.f90:631 STPNPL: 2-parameter continuation of folds of periodic solutions) */
int 
periodic_fold_starting_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, std::ptrdiff_t *ntsr, std::ptrdiff_t *ncolrs, double *rlcur, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *udotps, double *upoldp, double *tm, double *dtm, std::ptrdiff_t *nodir, double *thl, double *thu)
{

  std::ptrdiff_t ups_dim1, udotps_dim1;

  std::ptrdiff_t ndim;
  std::ptrdiff_t nfpr, nrsp1, i, j, k;

  std::ptrdiff_t k1, k2;
  std::vector<std::ptrdiff_t> icprs(iap->run->parameter_count);

  double rd1, rd2;
  std::ptrdiff_t ndm;

  /* Generates starting data for the 2-parameter continuation of folds */
  /* on a branch of periodic solutions. */

  udotps_dim1 = *ndxloc;
  ups_dim1 = *ndxloc;

  ndim = iap->ndim;
  ndm = iap->ndm;
  nfpr = iap->nfpr;

  xpp::TokenReader tr;
  xpp::AutoxSolutionHeader header;
  read_autox_restart_header(iap, rap, tr, header, ntsr, ncolrs);
  nrsp1 = *ntsr + 1;

  autox_restart_read(iap, xpp::read_autox_solution_rows(tr, ndim, *ntsr, *ncolrs, *ndxloc,
                                    0, ndm, ups, tm, false, 0, {}),
                    "the solution rows");
  autox_restart_read(iap, tr.read(icprs[0]), "the free parameter indices");
  autox_restart_read(iap, tr.read(icprs[1]), "the free parameter indices");
  autox_restart_read(iap, tr.read(rd1), "the parameter derivatives");
  autox_restart_read(iap, tr.read(rd2), "the parameter derivatives");

  /* Read U-dot (derivative with respect to arclength). */

  autox_restart_read(iap, xpp::read_autox_solution_rows(tr, ndim, *ntsr, *ncolrs, *ndxloc,
                                    0, ndm, udotps, nullptr, false, 0, {}),
                    "the arclength derivatives");
  /* Read the parameter values. */

  autox_restart_read(iap, xpp::read_autox_solution_parameters(tr, header, *iap->run, par), "the parameter values");

  /* Complement starting data */
  par[xpp::autox_unknown::fold_beta] = 0.;
  par[xpp::autox_unknown::fold_norm] = 0.;
  if (icp[2] == xpp::autox_unknown::period) {
    /*          Variable period */
    rldot[0] = rd1;
    rldot[1] = 0.;
    rldot[2] = rd2;
    rldot[3] = 0.;
    /*          Variable period */
  } else {
    /*          Fixed period */
    rldot[0] = rd1;
    rldot[1] = rd2;
    rldot[2] = 0.;
    rldot[3] = 0.;
  }

  for (j = 0; j < *ntsr; ++j) {
    for (i = 0; i < *ncolrs; ++i) {
      k1 = i * ndim + ndm;
      k2 = (i + 1) * ndim - 1;
      for (k = k1; k <= k2; ++k) {
	ARRAY2D(ups, j, k) = 0.;
	ARRAY2D(udotps, j, k) = 0.;
      }
    }
  }

  for (k = ndm; k < ndim; ++k) {
    ARRAY2D(ups, nrsp1 - 1, k) = 0.;
    ARRAY2D(udotps, nrsp1 - 1, k) = 0.;
  }

  for (i = 0; i < nfpr; ++i) {
    rlcur[i] = par[icp[i]];
  }

  *nodir = 0;

  return 0;
} /* stpnpl_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*   Subroutines for the Continuation of Period Doubling Bifurcations */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/* fnpd: AUTO's equations of the problem kind period_doubling (periodic.f90:1459 STPNPD: 2-parameter continuation of period doublings) */
int 
period_doubling_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp)
{
  return xpp::autox_jacobian::problem(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp, iap->nfpr, period_doubling_function);
} /* fnpd_ */

/* ffpd: AUTO's function evaluation of the problem kind period_doubling (periodic.f90:1459 STPNPD: 2-parameter continuation of period doublings) */
int 
period_doubling_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, double *f, std::ptrdiff_t ndm, double *dfdu, double *dfdp)
{
  std::ptrdiff_t dfdu_dim1;

  std::ptrdiff_t i, j;
  double period;

  dfdu_dim1 = ndm;
    
  period = par[xpp::autox_unknown::period];
  problem_function(iap, rap, ndm, u, uold, icp, par, 1, f, dfdu, dfdp);

  for (i = 0; i < ndm; ++i) {
    f[ndm + i] = 0.;
    for (j = 0; j < ndm; ++j) {
      f[ndm + i] += ARRAY2D(dfdu, i, j) * u[ndm + j];
    }
    f[i] = period * f[i];
    f[ndm + i] = period * f[ndm + i];
  }

  return 0;
} /* ffpd_ */

/* bcpd: AUTO's boundary conditions of the problem kind period_doubling (periodic.f90:1459 STPNPD: 2-parameter continuation of period doublings) */
int 
period_doubling_boundary_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nbc, const double *u0, const double *u1, double *f, std::ptrdiff_t ijac, double *dbc)
{
  std::ptrdiff_t dbc_dim1;

  std::ptrdiff_t jtmp, i, j, nn, ndm;

  /* Generate boundary conditions for the 2-parameter continuation */
  /* of period doubling bifurcations. */

  dbc_dim1 = nbc;
  
  ndm = iap->ndm;

  for (i = 0; i < ndm; ++i) {
    f[i] = u0[i] - u1[i];
    f[ndm + i] = u0[ndm + i] + u1[ndm + i];
  }

  /* Rotations */
  if (iap->run->rotations.irtn != 0) {
    for (i = 0; i < ndm; ++i) {
      if (iap->run->rotations.nrtn[i] != 0) {
	f[i] += par[xpp::autox_unknown::rotation_period] * iap->run->rotations.nrtn[i];
      }
    }
  }

  if (ijac == 0) {
    return 0;
  }

  jtmp = iap->run->parameter_count;
  nn = (ndim * 2) + jtmp;
  for (i = 0; i < nbc; ++i) {
    for (j = 0; j < nn; ++j) {
      ARRAY2D(dbc, i, j) = 0.;
    }
  }

  for (i = 0; i < ndim; ++i) {
    ARRAY2D(dbc, i, i) = 1.;
    if ((i + 1) <= ndm) {
      ARRAY2D(dbc, i, (ndim + i)) = -1.;
    } else {
      ARRAY2D(dbc, i, (ndim + i)) = 1.;
    }
  }

  return 0;
} /* bcpd_ */

/* icpd: AUTO's integral conditions of the problem kind period_doubling (periodic.f90:1459 STPNPD: 2-parameter continuation of period doublings) */
int 
period_doubling_integral_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nint, const double *u, const double *uold, const double *udot, const double *upold, double *f, std::ptrdiff_t ijac, double *dint)
{
  std::ptrdiff_t dint_dim1;

  std::ptrdiff_t jtmp, i, j, nn, ndm;

  dint_dim1 = nint;
  
  ndm = iap->ndm;

  periodic_integral_conditions(iap, rap, ndm, par, icp, nint, u, uold, udot, upold, f, 0, nullptr);
  f[1] = -par[xpp::autox_unknown::doubling_norm];

  for (i = 0; i < ndm; ++i) {
    f[1] += u[ndm + i] * u[ndm + i];
  }

  if (ijac == 0) {
    return 0;
  }

  jtmp = iap->run->parameter_count;
  nn = ndim + jtmp;
  for (i = 0; i < nint; ++i) {
    for (j = 0; j < nn; ++j) {
      ARRAY2D(dint, i, j) = 0.;
    }
  }

  for (i = 0; i < ndm; ++i) {
    if (iap->run->rotations.nrtn[i] == 0) {
      ARRAY2D(dint, 0, i) = upold[i];
    } else {
      ARRAY2D(dint, 0, i) = 0.;
    }
    ARRAY2D(dint, 1, ndm + i) = u[ndm + i] * 2.;
  }

  ARRAY2D(dint, 1, ndim + xpp::autox_unknown::doubling_norm) = -1.;

  return 0;
} /* icpd_ */

/* stpnpd: AUTO's starting data of the problem kind period_doubling (periodic.f90:1459 STPNPD: 2-parameter continuation of period doublings) */
int 
period_doubling_starting_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, std::ptrdiff_t *ntsr, std::ptrdiff_t *ncolrs, double *rlcur, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *udotps, double *upoldp, double *tm, double *dtm, std::ptrdiff_t *nodir, double *thl, double *thu)
{

  std::ptrdiff_t ups_dim1, udotps_dim1;

  std::ptrdiff_t ndim;
  std::ptrdiff_t nfpr, i, j, k;

  std::ptrdiff_t k1, k2;
  std::vector<std::ptrdiff_t> icprs(iap->run->parameter_count);

  std::ptrdiff_t ndm;

  /* Generates starting data for the 2-parameter continuation of */
  /* period-doubling bifurcations on a branch of periodic solutions. */

  udotps_dim1 = *ndxloc;
  ups_dim1 = *ndxloc;

  ndim = iap->ndim;
  ndm = iap->ndm;
  nfpr = iap->nfpr;

  xpp::TokenReader tr;
  xpp::AutoxSolutionHeader header;
  read_autox_restart_header(iap, rap, tr, header, ntsr, ncolrs);

  autox_restart_read(iap, xpp::read_autox_solution_rows(tr, ndim, *ntsr, *ncolrs, *ndxloc,
                                    0, ndm, ups, tm, false, 0, {}),
                    "the solution rows");
  autox_restart_read(iap, tr.read(icprs[0]), "the free parameter indices");
  autox_restart_read(iap, tr.read(icprs[1]), "the free parameter indices");
  autox_restart_read(iap, tr.read(rldot[0]), "the parameter derivatives");
  autox_restart_read(iap, tr.read(rldot[1]), "the parameter derivatives");

  /* Read U-dot (derivative with respect to arclength). */

  autox_restart_read(iap, xpp::read_autox_solution_rows(tr, ndim, *ntsr, *ncolrs, *ndxloc,
                                    0, ndm, udotps, nullptr, false, 0, {}),
                    "the arclength derivatives");
  /* Read the parameter values. */

  autox_restart_read(iap, xpp::read_autox_solution_parameters(tr, header, *iap->run, par), "the parameter values");

  /* Complement starting data */
  par[xpp::autox_unknown::doubling_norm] = 0.;
  rldot[2] = 0.;
  for (j = 0; j < *ntsr; ++j) {
    for (i = 0; i < *ncolrs; ++i) {
      k1 = i* ndim + ndm ;
      k2 = (i + 1) * ndim - 1;
      for (k = k1; k <= k2; ++k) {
	ARRAY2D(ups, j, k) = 0.;
	ARRAY2D(udotps, j, k) = 0.;
      }
    }
  }
  for (k = ndm; k < ndim; ++k) {
    ARRAY2D(ups, *ntsr, k) = 0.;
    ARRAY2D(udotps, *ntsr, k) = 0.;
  }

  for (i = 0; i < nfpr; ++i) {
    rlcur[i] = par[icp[i]];
  }

  *nodir = 0;

  return 0;
} /* stpnpd_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*       Subroutines for the Continuation of Torus Bifurcations */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/* fntr: AUTO's equations of the problem kind torus (periodic.f90:1728 STPNTR: 2-parameter continuation of torus bifurcations) */
int 
torus_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp)
{
  return xpp::autox_jacobian::problem(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp, iap->nfpr, torus_function);
} /* fntr_ */

/* fftr: AUTO's function evaluation of the problem kind torus (periodic.f90:1728 STPNTR: 2-parameter continuation of torus bifurcations) */
int 
torus_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, double *f, std::ptrdiff_t ndm, double *dfdu, double *dfdp)
{
  std::ptrdiff_t dfdu_dim1;

  std::ptrdiff_t i, j;
  double period;
  std::ptrdiff_t ndm2;

  dfdu_dim1 = ndm;
    
  period = par[xpp::autox_unknown::period];
  problem_function(iap, rap, ndm, u, uold, icp, par, 1, f, dfdu, dfdp);

  ndm2 = ndm * 2;
  for (i = 0; i < ndm; ++i) {
    f[ndm + i] = 0.;
    f[ndm2 + i] = 0.;
    for (j = 0; j < ndm; ++j) {
      f[ndm + i] += ARRAY2D(dfdu, i, j) * u[ndm + j];
      f[ndm2 + i] += ARRAY2D(dfdu, i, j) * u[ndm2 + j];
    }
    f[ndm + i] = period * f[ndm + i];
    f[ndm2 + i] = period * f[ndm2 + i];
    f[i] = period * f[i];
  }

  return 0;
} /* fftr_ */

/* bctr: AUTO's boundary conditions of the problem kind torus (periodic.f90:1728 STPNTR: 2-parameter continuation of torus bifurcations) */
int 
torus_boundary_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nbc, const double *u0, const double *u1, double *f, std::ptrdiff_t ijac, double *dbc)
{
  std::ptrdiff_t dbc_dim1;

  std::ptrdiff_t jtmp, i, j;
  double theta, cs;
  std::ptrdiff_t nn;
  double ss;
  std::ptrdiff_t ndm, ndm2;

  dbc_dim1 = nbc;
  
  ndm = iap->ndm;

  ndm2 = ndm << 1;
  theta = par[xpp::autox_unknown::torus_angle];

  ss = xpp::math::sin(theta);
  cs = xpp::math::cos(theta);

  for (i = 0; i < ndm; ++i) {
    f[i] = u0[i] - u1[i];
    f[ndm + i] = u1[ndm + i] - cs * u0[ndm + i] + ss * u0[ndm2 + i];
    f[ndm2 + i] = u1[ndm2 + i] - cs * u0[ndm2 + i] - ss * u0[ndm + i];
  }

  /* Rotations */
  if (iap->run->rotations.irtn != 0) {
    for (i = 0; i < ndm; ++i) {
      if (iap->run->rotations.nrtn[i] != 0) {
	f[i] += par[xpp::autox_unknown::rotation_period] * iap->run->rotations.nrtn[i];
      }
    }
  }

  if (ijac == 0) {
    return 0;
  }

  jtmp = iap->run->parameter_count;
  nn = (ndim * 2) + jtmp;
  for (i = 0; i < nbc; ++i) {
    for (j = 0; j < nn; ++j) {
      ARRAY2D(dbc, i, j) = 0.;
    }
  }

  for (i = 0; i < ndm; ++i) {
    ARRAY2D(dbc, i, i) = 1.;
    ARRAY2D(dbc, i, (ndim + i)) = -1.;
    ARRAY2D(dbc, ndm + i, (ndm + i)) = -cs;
    ARRAY2D(dbc, ndm + i, (ndm2 + i)) = ss;
    ARRAY2D(dbc, ndm + i, (ndim + ndm + i)) = 1.;
    ARRAY2D(dbc, ndm + i, ((ndim * 2) + xpp::autox_unknown::torus_angle)) = cs * u0[ndm2 + i] + ss * u0[ndm + i];
    ARRAY2D(dbc, ndm2 + i, (ndm + i)) = -ss;
    ARRAY2D(dbc, ndm2 + i, (ndm2 + i)) = -cs;
    ARRAY2D(dbc, ndm2 + i, (ndim + ndm2 + i)) = 1.;
    ARRAY2D(dbc, ndm2 + i, ((ndim * 2) + xpp::autox_unknown::torus_angle)) = ss * u0[ndm2 + i] - cs * u0[ndm + i];
  }

  return 0;
} /* bctr_ */

/* ictr: AUTO's integral conditions of the problem kind torus (periodic.f90:1728 STPNTR: 2-parameter continuation of torus bifurcations) */
int 
torus_integral_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nint, const double *u, const double *uold, const double *udot, const double *upold, double *f, std::ptrdiff_t ijac, double *dint)
{
  std::ptrdiff_t dint_dim1;

  std::ptrdiff_t jtmp, i, j, nn, ndm, ndm2;

  dint_dim1 = nint;
  
  ndm = iap->ndm;
  ndm2 = ndm * 2;

  periodic_integral_conditions(iap, rap, ndm, par, icp, nint, u, uold, udot, upold, f, 0, nullptr);
  f[1] = 0.;
  f[2] = -par[xpp::autox_unknown::torus_norm];

  for (i = 0; i < ndm; ++i) {
    f[1] = f[1] + u[ndm + i] * uold[ndm2 + i] - u[ndm2 + i] * uold[ndm + i];
    f[2] = f[2] + u[ndm + i] * u[ndm + i] + u[ndm2 + i] * u[ndm2 + i];
  }

  if (ijac == 0) {
    return 0;
  }

  jtmp = iap->run->parameter_count;
  nn = ndim + jtmp;
  for (i = 0; i < nint; ++i) {
    for (j = 0; j < nn; ++j) {
      ARRAY2D(dint, i, j) = 0.;
    }
  }

  for (i = 0; i < ndm; ++i) {
    if (iap->run->rotations.nrtn[i] == 0) {
      ARRAY2D(dint, 0, i) = upold[i];
    } else {
      ARRAY2D(dint, 0, i) = 0.;
    }
    ARRAY2D(dint, 1, ndm + i) = uold[ndm2 + i];
    ARRAY2D(dint, 1, ndm2 + i) = -uold[ndm + i];
    ARRAY2D(dint, 2, ndm + i) = u[ndm + i] * 2;
    ARRAY2D(dint, 2, ndm2 + i) = u[ndm2 + i] * 2;
  }

  ARRAY2D(dint, 2, ndim + xpp::autox_unknown::torus_norm) = -1.;

  return 0;
} /* ictr_ */

/* stpntr: AUTO's starting data of the problem kind torus (periodic.f90:1728 STPNTR: 2-parameter continuation of torus bifurcations) */
int 
torus_starting_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, std::ptrdiff_t *ntsr, std::ptrdiff_t *ncolrs, double *rlcur, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *udotps, double *upoldp, double *tm, double *dtm, std::ptrdiff_t *nodir, double *thl, double *thu)
{

  std::ptrdiff_t ups_dim1, udotps_dim1;

  std::ptrdiff_t ndim;
  std::ptrdiff_t nfpr, i;

  std::vector<std::ptrdiff_t> icprs(iap->run->parameter_count);

  std::ptrdiff_t ndm;

  /* Generates starting data for the 2-parameter continuation of torus */
  /* bifurcations. */

  udotps_dim1 = *ndxloc;
  ups_dim1 = *ndxloc;
    
  ndim = iap->ndim;
  ndm = iap->ndm;
  nfpr = iap->nfpr;

  xpp::TokenReader tr;
  xpp::AutoxSolutionHeader header;
  read_autox_restart_header(iap, rap, tr, header, ntsr, ncolrs);

  autox_restart_read(iap, xpp::read_autox_solution_rows(tr, ndim, *ntsr, *ncolrs, *ndxloc,
                                    0, ndm, ups, tm, false, 0, [&](std::ptrdiff_t j, std::ptrdiff_t i, double time) {
        const std::ptrdiff_t k1 = i * ndim + ndm;
        for (std::ptrdiff_t k = k1; k < k1 + ndm; ++k) {
          ARRAY2D(ups, j, k) = xpp::math::sin(time) * torus_initial_amplitude;
          ARRAY2D(ups, j, (k + ndm)) = xpp::math::cos(time) * torus_initial_amplitude;
        }
      }), "the solution rows");
  for (i = 0; i < ndm; ++i) {
    ARRAY2D(ups, *ntsr, (ndm + i)) = 0.;
    ARRAY2D(ups, *ntsr, ((ndm * 2) + i)) = 0.;
  }

  autox_restart_read(iap, tr.read(icprs[0]), "the free parameter indices");
  autox_restart_read(iap, tr.read(icprs[1]), "the free parameter indices");
  autox_restart_read(iap, tr.read(rldot[0]), "the parameter derivatives");
  autox_restart_read(iap, tr.read(rldot[1]), "the parameter derivatives");
  rldot[2] = 0.;
  rldot[3] = 0.;

  /* Read the direction vector and initialize the starting direction. */

  autox_restart_read(iap, xpp::read_autox_solution_rows(tr, ndim, *ntsr, *ncolrs, *ndxloc,
                                    0, ndm, udotps, nullptr, false, 0, {}),
                    "the arclength derivatives");
  // Preserve the original interior direction columns; only the endpoint
  // initialized the added torus columns (the old interior loop was empty).
  for (i = 0; i < ndm; ++i) {
    ARRAY2D(udotps, *ntsr, (ndm + i)) = 0.;
    ARRAY2D(udotps, *ntsr, ((ndm * 2) + i)) = 0.;
  }

  /* Read the parameter values. */

  autox_restart_read(iap, xpp::read_autox_solution_parameters(tr, header, *iap->run, par), "the parameter values");

  par[xpp::autox_unknown::torus_norm] = 0.;

  for (i = 0; i < nfpr; ++i) {
    rlcur[i] = par[icp[i]];
  }

  *nodir = 0;

  return 0;
} /* stpntr_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*        Subroutines for Optimization of Periodic Solutions */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/* fnpo: AUTO's equations of the problem kind periodic_optimization (optimization.f90:370,612: optimization of periodic solutions) */
int 
periodic_optimization_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp)
{
  std::ptrdiff_t dfdu_dim1, dfdp_dim1;

  std::ptrdiff_t nfpr;
  std::ptrdiff_t i;
  double ep, period;
  std::ptrdiff_t ndm;

  std::vector<double> upold(iap->ndim);

  /* Generates the equations for periodic optimization problems. */

  dfdp_dim1 = ndim;
  dfdu_dim1 = ndim;
    
  ndm = iap->ndm;
  nfpr = iap->nfpr;

/* Generate F(UOLD) */

  problem_function(iap, rap, ndm, uold, uold, icp, par, 0, 
       upold.data(), dfdu, dfdp);
  period = par[xpp::autox_unknown::period];
  for (i = 0; i < ndm; ++i) {
    upold[i] = period * upold[i];
  }

  /* Generate the function. */

  periodic_optimization_function(iap, rap, ndim, u, uold, upold.data(), icp, par, f,
       ndm, iap->run->scratch.dfu.data(), iap->run->scratch.dfp.data());

  if (ijac == 0) {
    return 0;
  }

  /* Generate the Jacobian. */

  ep = xpp::autox_jacobian::central(
      ndim, ndim, u, iap->run->scratch.uu1.data(), iap->run->scratch.uu2.data(), iap->run->scratch.ff1.data(), iap->run->scratch.ff2.data(),
      [&](const double *uu, double *ff) { periodic_optimization_function(iap, rap, ndim, uu, uold, upold.data(), icp, par, ff, ndm, iap->run->scratch.dfu.data(), iap->run->scratch.dfp.data()); },
      dfdu, dfdu_dim1);

  xpp::autox_jacobian::forward_parameters(
      nfpr, ndim, icp, ep, par,
      [&](double *ff) { periodic_optimization_function(iap, rap, ndim, u, uold, upold.data(), icp, par, ff, ndm, iap->run->scratch.dfu.data(), iap->run->scratch.dfp.data()); },
      f, iap->run->scratch.ff1.data(), dfdp, dfdp_dim1, 0);

  return 0;
} /* fnpo_ */

/* ffpo: AUTO's function evaluation of the problem kind periodic_optimization (optimization.f90:370,612: optimization of periodic solutions) */
int 
periodic_optimization_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const double *upold, const std::ptrdiff_t *icp, double *par, double *f, std::ptrdiff_t ndm, double *dfdu, double *dfdp)
{
  std::ptrdiff_t dfdu_dim1, dfdp_dim1;

  std::ptrdiff_t i, j;
  double gamma, rkappa, period, fop;
  std::vector<double> dfp(iap->run->parameter_count);

  std::vector<double> dfu(iap->ndim);

  dfdp_dim1 = ndm;
  dfdu_dim1 = ndm;
    
  period = par[xpp::autox_unknown::period];
  rkappa = par[xpp::autox_unknown::optimization_kappa];
  gamma = par[xpp::autox_unknown::optimization_gamma];

  for (i = 0; i < ndm; ++i) {
    for (j = 0; j < iap->run->parameter_count; ++j) {
      ARRAY2D(dfdp, i, j) = 0.;
    }
  }
  problem_function(iap, rap, ndm, u, uold, icp, par, 1, f, 
       dfdu, dfdp);
  for (i = 0; i < iap->run->parameter_count; ++i) {
    dfp[i] = 0.;
  }
  objective_interface(iap, rap, ndm, u, icp, par, 1, &fop, dfu.data(), dfp.data());

  for (i = 0; i < ndm; ++i) {
    f[ndm + i] = 0.;
    for (j = 0; j < ndm; ++j) {
      f[ndm + i] -= ARRAY2D(dfdu, j, i) * u[ndm + j];
    }
    f[i] = period * f[i];
    f[ndm + i] = period * f[ndm + i] + rkappa * upold[i] + gamma *dfu[i];
  }

  return 0;
} /* ffpo_ */

/* bcpo: AUTO's boundary conditions of the problem kind periodic_optimization (optimization.f90:370,612: optimization of periodic solutions) */
int 
periodic_optimization_boundary_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nbc, const double *u0, const double *u1, double *f, std::ptrdiff_t ijac, double *dbc)
{
  std::ptrdiff_t dbc_dim1;

  std::ptrdiff_t nfpr, i, j, nbc0;

  /* Generates the boundary conditions for periodic optimization problems. 
*/

  dbc_dim1 = nbc;
  
  nfpr = iap->nfpr;

  for (i = 0; i < nbc; ++i) {
    f[i] = u0[i] - u1[i];
  }

  /* Rotations */
  if (iap->run->rotations.irtn != 0) {
    nbc0 = iap->nbc0;
    for (i = 0; i < nbc0; ++i) {
      if (iap->run->rotations.nrtn[i] != 0) {
	f[i] += par[xpp::autox_unknown::rotation_period] * iap->run->rotations.nrtn[i];
      }
    }
  }

  if (ijac == 0) {
    return 0;
  }

  for (i = 0; i < nbc; ++i) {
    for (j = 0; j <= (ndim * 2); ++j) {
      ARRAY2D(dbc, i, j) = 0.;
    }
    ARRAY2D(dbc, i, i) = 1.;
    ARRAY2D(dbc, i, (ndim + i)) = -1.;
    for (j = 0; j < nfpr; ++j) {
      ARRAY2D(dbc, i, (ndim * 2) + icp[j]) = 0.;
    }
  }
  return 0;
} /* bcpo_ */

/* icpo: AUTO's integral conditions of the problem kind periodic_optimization (optimization.f90:370,612: optimization of periodic solutions) */
int 
periodic_optimization_integral_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nint, const double *u, const double *uold, const double *udot, const double *upold, double *f, std::ptrdiff_t ijac, double *dint)
{
  std::ptrdiff_t dint_dim1;

  std::ptrdiff_t nfpr;

  double ep;
  std::ptrdiff_t ndm;
  std::ptrdiff_t nnt0;

  std::vector<double> f1(iap->nint);
  std::vector<double> f2(iap->nint);
  std::vector<double> dnt(xpp::size_product(iap->nint, xpp::size_sum(iap->ndim, iap->run->parameter_count)));

/* Generates integral conditions for periodic optimization problems. */

  dint_dim1 = nint;
  
  ndm = iap->ndm;
  nnt0 = iap->nnt0;
  nfpr = iap->nfpr;

  /* Generate the function. */

  periodic_optimization_integral_function(iap, rap, ndim, par, icp, nint, nnt0, u, uold, 
       udot, upold, f, dnt.data(), ndm, iap->run->scratch.dfu.data(), iap->run->scratch.dfp.data());

  if (ijac == 0) {
    return 0;
  }

  /* Generate the Jacobian. */

  ep = xpp::autox_jacobian::central(
      ndim, nint, u, iap->run->scratch.uu1.data(), iap->run->scratch.uu2.data(), f1.data(), f2.data(),
      [&](const double *uu, double *ff) { periodic_optimization_integral_function(iap, rap, ndim, par, icp, nint, nnt0, uu, uold, udot, upold, ff, dnt.data(), ndm, iap->run->scratch.dfu.data(), iap->run->scratch.dfp.data()); },
      dint, dint_dim1);

  xpp::autox_jacobian::forward_parameters(
      nfpr, nint, icp, ep, par,
      [&](double *ff) { periodic_optimization_integral_function(iap, rap, ndim, par, icp, nint, nnt0, u, uold, udot, upold, ff, dnt.data(), ndm, iap->run->scratch.dfu.data(), iap->run->scratch.dfp.data()); },
      f, f1.data(), dint, dint_dim1, ndim);

  return 0;
} /* icpo_ */

/* fipo: AUTO's integral function evaluation of the problem kind periodic_optimization (optimization.f90:370,612: optimization of periodic solutions) */
int 
periodic_optimization_integral_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nint, std::ptrdiff_t nnt0, const double *u, const double *uold, const double *udot, const double *upold, double *fi, double *dint, std::ptrdiff_t ndmt, double *dfdu, double *dfdp)
{
  std::ptrdiff_t dfdp_dim1;

  std::ptrdiff_t nfpr, indx;
  std::ptrdiff_t i, j, l;
  std::vector<double> dfp(iap->run->parameter_count);
  std::ptrdiff_t ndm;
  double fop;

  std::vector<double> f(iap->ndim);
  std::vector<double> dfu(iap->ndim);

  dfdp_dim1 = ndmt;

  ndm = iap->ndm;
  nfpr = iap->nfpr;

  periodic_integral_conditions(iap, rap, ndm, par, icp, nint, u, uold, udot, upold, fi, 0, nullptr);

  for (i = 0; i < iap->run->parameter_count; ++i) {
    dfp[i] = 0.;
  }
  objective_interface(iap, rap, ndm, u, icp, par, 2, &fop, dfu.data(), dfp.data());
  fi[1] = par[xpp::autox_unknown::objective] - fop;

  /* Computing 2nd power */
  fi[2] = par[xpp::autox_unknown::optimization_kappa] * par[xpp::autox_unknown::optimization_kappa] + par[xpp::autox_unknown::optimization_gamma] * par[xpp::autox_unknown::optimization_gamma] - par[xpp::autox_unknown::optimization_norm];
  for (i = 0; i < ndm; ++i) {
    /* Computing 2nd power */
    fi[2] +=  u[ndm + i] * u[ndm + i];
  }

  for (i = 0; i < ndm; ++i) {
    for (j = 0; j < iap->run->parameter_count; ++j) {
      ARRAY2D(dfdp, i, j) = 0.;
    }
  }
  problem_function(iap, rap, ndm, u, uold, icp, par, 2, f.data(), dfdu, dfdp);

  for (l = 3; l < nint; ++l) {
    indx = icp[nfpr + l - 3];
    if (indx == xpp::autox_unknown::period) {
      fi[l] = -par[xpp::autox_unknown::optimization_gamma] * dfp[indx] - par[indx + xpp::autox_unknown::test_function_start];
      for (i = 0; i < ndm; ++i) {
	fi[l] += f[i] * u[ndm + i];
      }
    } else {
      fi[l] = -par[xpp::autox_unknown::optimization_gamma] * dfp[indx] - par[indx + xpp::autox_unknown::test_function_start];
      for (i = 0; i < ndm; ++i) {
	fi[l] += par[xpp::autox_unknown::period] * ARRAY2D(dfdp, i, (indx)) * u[ndm + i]
          ;
      }
    }
  }

  return 0;
} /* fipo_ */

/* stpnpo: AUTO's starting data of the problem kind periodic_optimization (optimization.f90:370,612: optimization of periodic solutions) */
int 
periodic_optimization_starting_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, std::ptrdiff_t *ntsr, std::ptrdiff_t *ncolrs, double *rlcur, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *udotps, double *upoldp, double *tm, double *dtm, std::ptrdiff_t *nodir, double *thl, double *thu)
{

  std::ptrdiff_t ups_dim1;

  std::ptrdiff_t ndim;
  std::ptrdiff_t nfpr;
  double dump;

  double dumu;
  std::ptrdiff_t i, j, k;

  std::vector<std::ptrdiff_t> icprs(iap->run->parameter_count);

  std::ptrdiff_t k1, k2;
  double fs;

  std::ptrdiff_t ndm;
  double rld1, rld2;

  std::ptrdiff_t temporary_storage_dim1;
  // Objective samples use the target collocation layout and ndxloc stride.
  std::vector<double> temporary_storage(xpp::size_product(*ndxloc, xpp::size_product(iap->ndim, iap->ncol)));
  std::vector<double> u(iap->ndim);

  /* Generates starting data for optimization of periodic solutions. */

  ups_dim1 = *ndxloc;
  temporary_storage_dim1 = *ndxloc;
  
  ndim = iap->ndim;
  ndm = iap->ndm;
  nfpr = iap->nfpr;

  xpp::TokenReader tr;
  xpp::AutoxSolutionHeader header;
  read_autox_restart_header(iap, rap, tr, header, ntsr, ncolrs);

  autox_restart_read(iap, xpp::read_autox_solution_rows(tr, ndim, *ntsr, *ncolrs, *ndxloc,
                                    0, ndm, ups, tm, false, 0, {}),
                    "the solution rows");
  for (j = 0; j < *ntsr; ++j) dtm[j] = tm[j + 1] - tm[j];

  autox_restart_read(iap, tr.read(icprs[0]), "the free parameter indices");
  autox_restart_read(iap, tr.read(icprs[1]), "the free parameter indices");
  autox_restart_read(iap, tr.read(rld1), "the parameter derivatives");
  autox_restart_read(iap, tr.read(rld2), "the parameter derivatives");

  /* Read U-dot (derivative with respect to arclength). */
  autox_restart_read(iap, xpp::read_autox_solution_rows(tr, ndim, *ntsr, *ncolrs, *ndxloc,
                                    0, ndm, udotps, nullptr, false, 0, {}),
                    "the arclength derivatives");
  /* Read the parameter values. */
  autox_restart_read(iap, xpp::read_autox_solution_parameters(tr, header, *iap->run, par), "the parameter values");

  for (j = 0; j < *ntsr; ++j) {
    for (i = 0; i < *ncolrs; ++i) {
      k1 = i * ndim;
      k2 = k1 + ndm - 1;
      for (k = k1; k <= k2; ++k) {
	u[k - k1] = ARRAY2D(ups, j, k);
      }
      user_objective(ndm, u.data(), icp, par, 0, &fs, &dumu, &dump);
      ARRAY2D(temporary_storage, j, k1) = fs;
    }
  }
  for (k = 0; k < ndm; ++k) {
    u[k] = ARRAY2D(ups, *ntsr, k);
  }
  user_objective(ndm, u.data(), icp, par, 0, &fs, &dumu, &dump);
  temporary_storage[*ntsr] = fs;
  par[xpp::autox_unknown::objective] = integral_of_component(iap, ndxloc, 1, temporary_storage.data(), dtm);

  /* Complement starting data */

  for (i = xpp::autox_unknown::optimization_norm; i < iap->run->parameter_count; ++i) {
    par[i] = 0.;
  }

  for (j = 0; j < *ntsr; ++j) {
    for (i = 0; i < *ncolrs; ++i) {
      k1 = i * ndim + ndm;
      k2 = (i + 1) * ndim - 1;
      for (k = k1; k <= k2; ++k) {
	ARRAY2D(ups, j, k) = 0.;
      }
    }
  }
  for (k = ndm; k < ndim; ++k) {
    ARRAY2D(ups, *ntsr, k) = 0.;
  }

  for (i = 0; i < nfpr; ++i) {
    rlcur[i] = par[icp[i]];
  }

  *nodir = 1;

  return 0;
} /* stpnpo_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*        Subroutines for the Continuation of Folds for BVP. */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/* fnbl: AUTO's equations of the problem kind boundary_fold (toolboxbv.f90:839 STPNBL: 2-parameter continuation of folds of a BVP) */
int 
boundary_fold_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp)
{
  return xpp::autox_jacobian::problem(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp, iap->nfpr, boundary_fold_function);
} /* fnbl_ */

/* ffbl: AUTO's function evaluation of the problem kind boundary_fold (toolboxbv.f90:839 STPNBL: 2-parameter continuation of folds of a BVP) */
int 
boundary_fold_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, double *f, std::ptrdiff_t ndm, double *dfdu, double *dfdp)
{
  std::ptrdiff_t dfdu_dim1, dfdp_dim1;

  std::ptrdiff_t nfpr, nfpx, i, j;

  dfdp_dim1 = ndm;
  dfdu_dim1 = ndm;
    
  nfpr = iap->nfpr;

  problem_function(iap, rap, ndm, u, uold, icp, par, 2, f, 
       dfdu, dfdp);

  nfpx = nfpr / 2 - 1;
  for (i = 0; i < ndm; ++i) {
    f[ndm + i] = 0.;
    for (j = 0; j < ndm; ++j) {
      f[ndm + i] += ARRAY2D(dfdu, i, j) * u[ndm + j];
    }
    if (nfpx > 0) {
      for (j = 0; j < nfpx; ++j) {
	f[ndm + i] += ARRAY2D(dfdp, i, icp[j + 1]) * par[icp[nfpr - nfpx + j]];
      }
    }
  }

  return 0;
} /* ffbl_ */

/* bcbl: AUTO's boundary conditions of the problem kind boundary_fold (toolboxbv.f90:839 STPNBL: 2-parameter continuation of folds of a BVP) */
int 
boundary_fold_boundary_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nbc, const double *u0, const double *u1, double *f, std::ptrdiff_t ijac, double *dbc)
{
  std::ptrdiff_t dbc_dim1;

  std::ptrdiff_t nfpr;

  double ep;
  std::ptrdiff_t nbc0;

  std::vector<double> ff1(iap->nbc);
  std::vector<double> ff2(iap->nbc);
  std::vector<double> uu1(iap->ndim);
  std::vector<double> uu2(iap->ndim);
  std::vector<double> dfu(xpp::size_product(iap->nbc, xpp::size_sum(xpp::size_product(2, iap->ndim), iap->run->parameter_count)));
		     
/* Generates the boundary conditions for the 2-parameter continuation */
/* of folds (BVP). */

  dbc_dim1 = nbc;
  
  nbc0 = iap->nbc0;
  nfpr = iap->nfpr;

  /* Generate the function. */

  boundary_fold_boundary_function(iap, rap, ndim, par, icp, nbc, nbc0, u0, u1, f, dfu.data());

  if (ijac == 0) {
    return 0;
  }

  /* Derivatives with respect to U0. */

  ep = xpp::autox_jacobian::central(
      ndim, nbc, u0, uu1.data(), uu2.data(), ff1.data(), ff2.data(),
      [&](const double *uu, double *ff) { boundary_fold_boundary_function(iap, rap, ndim, par, icp, nbc, nbc0, uu, u1, ff, dfu.data()); },
      dbc, dbc_dim1);

  /* Derivatives with respect to U1. */

  ep = xpp::autox_jacobian::central(
      ndim, nbc, u1, uu1.data(), uu2.data(), ff1.data(), ff2.data(),
      [&](const double *uu, double *ff) { boundary_fold_boundary_function(iap, rap, ndim, par, icp, nbc, nbc0, u0, uu, ff, dfu.data()); },
      dbc + ndim * dbc_dim1, dbc_dim1);

  xpp::autox_jacobian::forward_parameters(
      nfpr, nbc, icp, ep, par,
      [&](double *ff) { boundary_fold_boundary_function(iap, rap, ndim, par, icp, nbc, nbc0, u0, u1, ff, dfu.data()); },
      f, ff2.data(), dbc, dbc_dim1, ndim * 2);

  return 0;
} /* bcbl_ */

/* fbbl: AUTO's boundary function evaluation of the problem kind boundary_fold (toolboxbv.f90:839 STPNBL: 2-parameter continuation of folds of a BVP) */
int 
boundary_fold_boundary_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nbc, std::ptrdiff_t nbc0, const double *u0, const double *u1, double *f, double *dbc)
{
  std::ptrdiff_t dbc_dim1;

  std::ptrdiff_t nfpr, nfpx, i, j, ndm;

  dbc_dim1 = nbc0;
  
  ndm = iap->ndm;
  nfpr = iap->nfpr;

  nfpx = nfpr / 2 - 1;
  boundary_function(iap, rap, ndm, par, icp, nbc0, u0, u1, f, 2, dbc);
  for (i = 0; i < nbc0; ++i) {
    f[nbc0 + i] = 0.;
    for (j = 0; j < ndm; ++j) {
      f[nbc0 + i] += ARRAY2D(dbc, i, j) * u0[ndm + j];
      f[nbc0 + i] += ARRAY2D(dbc, i, (ndm + j)) * u1[ndm + j];
    }
    if (nfpx != 0) {
      for (j = 0; j < nfpx; ++j) {
	f[nbc0 + i] += ARRAY2D(dbc, i, ndim + icp[j + 1]) *
	  par[icp[nfpr - nfpx + j]];
      }
    }
  }

  return 0;
} /* fbbl_ */

/* icbl: AUTO's integral conditions of the problem kind boundary_fold (toolboxbv.f90:839 STPNBL: 2-parameter continuation of folds of a BVP) */
int 
boundary_fold_integral_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nint, const double *u, const double *uold, const double *udot, const double *upold, double *f, std::ptrdiff_t ijac, double *dint)
{
  return xpp::autox_jacobian::integral(iap, rap, ndim, par, icp, nint, u, uold, udot, upold, f, ijac, dint, boundary_fold_integral_function);
} /* icbl_ */

/* fibl: AUTO's integral function evaluation of the problem kind boundary_fold (toolboxbv.f90:839 STPNBL: 2-parameter continuation of folds of a BVP) */
int 
boundary_fold_integral_function(const iap_type *iap, const rap_type *rap, const std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nint, std::ptrdiff_t nnt0, const double *u, const double *uold, const double *udot, const double *upold, double *f, double *dint)
{
  std::ptrdiff_t dint_dim1;

  std::ptrdiff_t nfpr, nfpx=0, i, j, ndm;

  dint_dim1 = nnt0;
  
  ndm = iap->ndm;
  nfpr = iap->nfpr;

  if (nnt0 > 0) {
    nfpx = nfpr / 2 - 1;
    integral_function(iap, rap, ndm, par, icp, nnt0, u, uold, udot, upold, f, 2, dint);
    for (i = 0; i < nnt0; ++i) {
      f[nnt0 + i] = 0.;
      for (j = 0; j < ndm; ++j) {
	f[nnt0 + i] += ARRAY2D(dint, i, j) * u[ndm + j];
      }
      if (nfpx != 0) {
	for (j = 0; j < nfpx; ++j) {
	  f[nnt0 + i] += ARRAY2D(dint, i, ndm + icp[j + 1]) * par[icp[nfpr - nfpx + j]];
	}
      }
    }
  }

  /* Note that PAR(11+NFPR/2) is used to keep the norm of the null vector */
  f[-1 + nint] = -par[-1 + nfpr / 2 + xpp::autox_unknown::bvp_fold_start + 1];
  for (i = 0; i < ndm; ++i) {
    f[-1 + nint] += u[ndm + i] * u[ndm + i];
  }
  if (nfpx != 0) {
    for (i = 0; i < nfpx; ++i) {
      /* Computing 2nd power */
      f[-1 + nint] += par[icp[nfpr - nfpx + i]] * par[icp[nfpr - nfpx + i]];
    }
  }

  return 0;
} /* fibl_ */

/* stpnbl: AUTO's starting data of the problem kind boundary_fold (toolboxbv.f90:839 STPNBL: 2-parameter continuation of folds of a BVP) */
int 
boundary_fold_starting_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, std::ptrdiff_t *ntsr, std::ptrdiff_t *ncolrs, double *rlcur, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *udotps, double *upoldp, double *tm, double *dtm, std::ptrdiff_t *nodir, double *thl, double *thu)
{

  std::ptrdiff_t ndim;
  std::ptrdiff_t nfpr, nfpx, nfpr0, i;

  std::vector<std::ptrdiff_t> icprs(iap->run->parameter_count);

  std::ptrdiff_t ndm;

  /* Generates starting data for the 2-parameter continuation of folds. */
  /* (BVP). */

  ndim = iap->ndim;
  ndm = iap->ndm;
  nfpr = iap->nfpr;

  xpp::TokenReader tr;
  xpp::AutoxSolutionHeader header;
  read_autox_restart_header(iap, rap, tr, header, ntsr, ncolrs);

  autox_restart_read(iap, xpp::read_autox_solution_rows(tr, ndim, *ntsr, *ncolrs, *ndxloc,
                                    0, ndm, ups, tm, false, 0, {}),
                    "the solution rows");
  nfpr0 = nfpr / 2;
  autox_restart_read(iap, tr.read(icprs[0]), "the free parameter indices");
  for (i = 0; i < nfpr0; ++i) {
    autox_restart_read(iap, tr.read(rldot[i]), "the parameter derivatives");
  }

  /* Read U-dot (Derivative with respect to arclength). */

  autox_restart_read(iap, xpp::read_autox_solution_rows(tr, ndim, *ntsr, *ncolrs, *ndxloc,
                                    ndm, ndim - ndm, ups, nullptr, false, 0, {}),
                    "the arclength derivatives");
  /* Read the parameter values. */

  autox_restart_read(iap, xpp::read_autox_solution_parameters(tr, header, *iap->run, par), "the parameter values");

  nfpx = nfpr / 2 - 1;
  if (nfpx > 0) {
    for (i = 0; i < nfpx; ++i) {
      par[icp[nfpr0 + 1 + i]] = rldot[i + 1];
    }
  }
  /* Initialize the norm of the null vector */
  par[-1 + nfpr / 2 + xpp::autox_unknown::bvp_fold_start + 1] = 0.;

  for (i = 0; i < nfpr; ++i) {
    rlcur[i] = par[icp[i]];
  }

  *nodir = 1;

  return 0;
} /* stpnbl_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*          Routines for Interface with User Supplied Routines */
/*  (To generate Jacobian by differencing, if not supplied analytically) */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/* funi: AUTO's FUNI (interfaces.f90:60): the interface to the user's right-hand side */
int 
problem_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp)
{
  std::ptrdiff_t dfdu_dim1, dfdp_dim1;

  std::ptrdiff_t nfpr;
  double rtmp;
  std::ptrdiff_t i;
  double ep;
  std::ptrdiff_t jac, ijc;

  auto &u1zz = iap->run->scratch.funi_u1;
  auto &u2zz = iap->run->scratch.funi_u2;
  auto &f1zz = iap->run->scratch.funi_f1;
  auto &f2zz = iap->run->scratch.funi_f2;

  /* Interface subroutine to user supplied FUNC. */

  dfdp_dim1 = ndim;
  dfdu_dim1 = ndim;
    
  jac = iap->jac;
  nfpr = iap->nfpr;

/* Generate the function. */

  ijc = jac == autox_jac_differences ? 0 : ijac;
  user_right_hand_side(*iap->run, ndim, u, icp, nfpr, par, ijc, f, dfdu,
       dfdp);

  if (jac == autox_jac_supplied || ijac == 0) {
    return 0;
  }

  /* Generate the Jacobian by differencing. The run preallocates these;
     direct evaluator calls also establish their own required extent. */
  u1zz.resize(ndim);
  u2zz.resize(ndim);
  f1zz.resize(ndim);
  f2zz.resize(ndim);

  const auto evaluate = [&](const double *uu, double *ff) { func_again(*iap->run, ndim, uu, icp, nfpr, par, ff); };
  const xpp::DifferenceGroups *groups = xpp::autox_jacobian::state_groups(iap, ndim);
  ep = groups ? xpp::autox_jacobian::central_grouped(ndim, u, f, *groups, u1zz.data(), u2zz.data(), f1zz.data(), f2zz.data(), evaluate, dfdu, dfdu_dim1)
              : xpp::autox_jacobian::central(ndim, ndim, u, u1zz.data(), u2zz.data(), f1zz.data(), f2zz.data(), evaluate, dfdu, dfdu_dim1);

  if (ijac == 1) {
    return 0;
  }

  for (i = 0; i < nfpr; ++i) {
    rtmp = autox_hmach;
    ep = rtmp * (f2c::abs(par[icp[i]]) + 1);
    xpp::autox_jacobian::forward_parameters(
        1, ndim, icp + i, ep, par,
        [&](double *ff) { user_right_hand_side(*iap->run, ndim, u, icp, nfpr, par, 0, ff, dfdu, dfdp); },
        f, f1zz.data(), dfdp, dfdp_dim1, 0);
  }

  return 0;
} /* problem_function */

/* bcni: AUTO's BCNI (interfaces.f90:141): the interface to the user's boundary conditions */
int 
boundary_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nbc, const double *u0, const double *u1, double *f, std::ptrdiff_t ijac, double *dbc)
{
  std::ptrdiff_t dbc_dim1;

  std::ptrdiff_t nfpr;
  double rtmp;
  std::ptrdiff_t i;
  double ep;

  std::vector<double> u1zz(iap->ndim);
  std::vector<double> u2zz(iap->ndim);
  std::vector<double> f1zz(iap->nbc);
  std::vector<double> f2zz(iap->nbc);

  /* Interface subroutine to the user supplied BCND. */

  dbc_dim1 = nbc;

  nfpr = iap->nfpr;

  /* Generate the function. */

  /* xppautX's user_boundary_conditions supplies no derivatives: they are differenced whatever
     JAC says (JAC=1 is the model's exact Jacobian in user_right_hand_side, W260) */
  user_boundary_conditions(*iap->run, ndim, par, icp, nbc, u0, u1, 0, f, dbc);

  if (ijac == 0) {
    return 0;
  }

  /* Generate the Jacobian by differencing. */

  ep = xpp::autox_jacobian::central(
      ndim, nbc, u0, u1zz.data(), u2zz.data(), f1zz.data(), f2zz.data(),
      [&](const double *uu, double *ff) { user_boundary_conditions(*iap->run, ndim, par, icp, nbc, uu, u1, 0, ff, dbc); },
      dbc, dbc_dim1);

  ep = xpp::autox_jacobian::central(
      ndim, nbc, u1, u1zz.data(), u2zz.data(), f1zz.data(), f2zz.data(),
      [&](const double *uu, double *ff) { user_boundary_conditions(*iap->run, ndim, par, icp, nbc, u0, uu, 0, ff, dbc); },
      dbc + ndim * dbc_dim1, dbc_dim1);

  if (ijac == 1) {
    return 0;
  }

  for (i = 0; i < nfpr; ++i) {
    rtmp = autox_hmach;
    ep = rtmp * (f2c::abs(par[icp[i]]) + 1);
    xpp::autox_jacobian::forward_parameters(
        1, nbc, icp + i, ep, par,
        [&](double *ff) { user_boundary_conditions(*iap->run, ndim, par, icp, nbc, u0, u1, 0, ff, dbc); },
        f, f1zz.data(), dbc, dbc_dim1, ndim * 2);
  }

  return 0;
} /* boundary_function */

/* icni: AUTO's ICNI (interfaces.f90:236): the interface to the user's integral conditions */
int 
integral_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nint, const double *u, const double *uold, const double *udot, const double *upold, double *f, std::ptrdiff_t ijac, double *dint)
{
  std::ptrdiff_t dint_dim1;

  std::ptrdiff_t nfpr;
  double rtmp;
  std::ptrdiff_t i;
  double ep;

  std::vector<double> f1zz(iap->nint);
  std::vector<double> f2zz(iap->nint);
  std::vector<double> u1zz(iap->ndim);
  std::vector<double> u2zz(iap->ndim);
  /* Interface subroutine to user supplied ICND. */

  dint_dim1 = nint;

  nfpr = iap->nfpr;

  /* Generate the integrand. */

  /* xppautX's user_integral_conditions supplies no derivatives: they are differenced whatever
     JAC says (JAC=1 is the model's exact Jacobian in user_right_hand_side, W260) */
  user_integral_conditions(ndim, par, icp, nint, u, uold, udot, upold,
       0, f, dint);

  if (ijac == 0) {
    return 0;
  }

  /* Generate the Jacobian by differencing. */

  ep = xpp::autox_jacobian::central(
      ndim, nint, u, u1zz.data(), u2zz.data(), f1zz.data(), f2zz.data(),
      [&](const double *uu, double *ff) { user_integral_conditions(ndim, par, icp, nint, uu, uold, udot, upold, 0, ff, dint); },
      dint, dint_dim1);

  if (ijac == 1) {
    return 0;
  }

  for (i = 0; i < nfpr; ++i) {
    rtmp = autox_hmach;
    ep = rtmp * (f2c::abs(par[icp[i]]) + 1);
    xpp::autox_jacobian::forward_parameters(
        1, nint, icp + i, ep, par,
        [&](double *ff) { user_integral_conditions(ndim, par, icp, nint, u, uold, udot, upold, 0, ff, dint); },
        f, f1zz.data(), dint, dint_dim1, ndim);
  }

  return 0;
} /* integral_function */

/* fopi: interface to the user's FOPT, objective and Jacobian by differencing (XPPAUT 8.0 autlib3.c:4890) */
int 
objective_interface(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp)
{

  std::ptrdiff_t nfpr;

  double rtmp;
  std::ptrdiff_t i;
  double f1, f2, ep;

  std::vector<double> u1zz(iap->ndim);
  std::vector<double> u2zz(iap->ndim);

  /* Interface subroutine to user supplied FOPT. */

  nfpr = iap->nfpr;

  /* Generate the objective function. */

  /* xppautX's user_objective supplies no derivatives: they are differenced whatever
     JAC says (JAC=1 is the model's exact Jacobian in user_right_hand_side, W260) */
  user_objective(ndim, u, icp, par, 0, f, dfdu, dfdp);

  if (ijac == 0) {
    return 0;
  }

  /* Generate the Jacobian by differencing. */

  ep = xpp::autox_jacobian::central(
      ndim, 1, u, u1zz.data(), u2zz.data(), &f1, &f2,
      [&](const double *uu, double *ff) { user_objective(ndim, uu, icp, par, 0, ff, dfdu, dfdp); },
      dfdu, 1);

  if (ijac == 1) {
    return 0;
  }

  for (i = 0; i < nfpr; ++i) {
    rtmp = autox_hmach;
    ep = rtmp * (f2c::abs(par[icp[i]]) + 1);
    xpp::autox_jacobian::forward_parameters(
        1, 1, icp + i, ep, par,
        [&](double *ff) { user_objective(ndim, u, icp, par, 0, ff, dfdu, dfdp); },
        f, &f1, dfdp, 1, 0);
  }

  return 0;
} /* objective_interface */
