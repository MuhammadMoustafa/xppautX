#include "xpp_mem.h"
#include "auto_unknowns.h"
/* translated from AUTO's autlib3.f by f2c */

#include <array>
#include <vector>
#include "xpp_log.h"
#include "session.h"
#include "auto_c.h"
#include "auto_jacobian.h"
#include "xpp_math.h"

namespace {
// Original amplitude of the torus restart's added eigenvector states.
constexpr doublereal torus_initial_amplitude = 1e-4;
} // namespace

/* Reused finite-difference scratch belongs to iap->lib: the calling
   Session's AutoLib, or a collocation worker's private AutoLib (W247).
   Rotation counts are computed once per run and only read by workers. */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*  Subroutines for the Continuation of Folds (Algebraic Problems) */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

int 
fnlp(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp)
{
  return xpp::auto_jacobian::curve(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp, false, fflp);
} /* fnlp_ */

int 
fflp(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp)
{
  integer dfdu_dim1;
  
  integer i, j;
  xpp::AutoProblem ips;
  
  dfdu_dim1 = ndm;

  ips = iap->ips;

  par[icp[1]] = u[-1 + ndim];
  if (ips == xpp::AutoProblem::DiscreteMap) {
    fnds(iap, rap, ndm, u, uold, icp, par, 2, f, dfdu, dfdp);
  } else {
    funi(iap, rap, ndm, u, uold, icp, par, 2, f, dfdu, dfdp);
  }

  for (i = 0; i < ndm; ++i) {
    f[ndm + i] = 0.;
    for (j = 0; j < ndm; ++j) {
      f[ndm + i] += (ARRAY2D(dfdu, i, j) + (i==j && iap->itpst==xpp::AutoPointKind::PeriodDoubling ? 2. : 0.)) * u[ndm + j];
    }
  }

  f[-1 + ndim] = -1.;

  for (i = 0; i < ndm; ++i) {
    f[-1 + ndim] += u[ndm + i] * u[ndm + i];
  }

  return 0;
} /* fflp_ */

int 
stpnlp(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *u)
{
  integer ndim;

  doublereal uold = 0.; /* funi/fnds take it, unread */
  integer nfpr1;
  integer i;
  logical found;

  integer ndm, irs;
  xpp::AutoProblem ips;

  std::vector<doublereal> f(iap->ndim);
  std::vector<doublereal> v(iap->ndim);
  /* Generates starting data for the continuation of folds. */

  ndim = iap->ndim;
  ips = iap->ips;
  irs = iap->irs;
  ndm = iap->ndm;

  findlb(iap, rap, irs, &nfpr1, &found);
  readlb(iap, rap, u, par);

  if (ips == xpp::AutoProblem::DiscreteMap) {
    fnds(iap, rap, ndm, u, &uold, icp, par, 1, f.data(), 
	 iap->lib->scratch.dfu.data(), iap->lib->scratch.dfp.data());
  } else {
    funi(iap, rap, ndm, u, &uold, icp, par, 1, f.data(), 
	 iap->lib->scratch.dfu.data(), iap->lib->scratch.dfp.data());
  }
  if (ips==xpp::AutoProblem::DiscreteMap && iap->itp.is(xpp::AutoPointKind::PeriodDoubling))
    for (integer k=0;k<ndm;++k) iap->lib->scratch.dfu[k+ndm*k]+=2.;
  xpp::nlvc(iap->lib->fp9, ndm, ndm, 1, iap->lib->scratch.dfu.data(), v.data());
  nrmlz(&ndm, v.data());
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

int 
fnc1(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp)
{
  integer dfdu_dim1, dfdp_dim1;

  integer i, j;
  std::vector<doublereal> ddp(iap->lib->parameter_count);
  integer ndm;

  std::vector<doublereal> ddu(iap->ndim);
  /* Generate the equations for the continuation scheme used for */
  /* the optimization of algebraic systems (one parameter). */

  dfdp_dim1 = ndim;
  dfdu_dim1 = ndim;
    
  ndm = iap->ndm;

  par[icp[1]] = u[-1 + ndim];
  funi(iap, rap, ndm, u, uold, icp, par, ijac, f, dfdu, dfdp);

  /* Rearrange (Since dimensions in FNC1 and FUNI differ). */

  if (ijac != 0) {
    for (j = ndm - 1; j >= 0; --j) {
      for (i = ndm - 1; i >= 0; --i) {
	ARRAY2D(dfdu, i, j) = dfdu[j * ndm + i];
      }
    }

    for (j = iap->lib->parameter_count - 1; j >= 0; --j) {
      for (i = ndm - 1; i >= 0; --i) {
	ARRAY2D(dfdp, i, j) = dfdp[j * ndm + i];
      }
    }
  }

  fopi(iap, rap, ndm, u, icp, par, ijac, &f[-1 + ndim], ddu.data(), 
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

int 
stpnc1(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *u)
{
  integer ndim;

  integer nfpr;

  integer ndm;
  doublereal fop, dum;

  /* Generate starting data for optimization problems (one parameter). */

  ndim = iap->ndim;
  ndm = iap->ndm;

  stpnt(*iap->lib->session, ndim, 0.0, u, par);
  nfpr = 2;
  iap->nfpr = nfpr;
  fopi(iap, rap, ndm, u, icp, par, 0, &fop, &dum, &
       dum);
  par[icp[0]] = fop;
  u[-1 + ndim] = par[icp[1]];

  return 0;
} /* stpnc1_ */

int 
fnc2(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp)
{
  integer dfdu_dim1, dfdp_dim1;

  integer i;
  integer ndm;

  /* Generate the equations for the continuation scheme used for the */
  /* optimization of algebraic systems (more than one parameter). */

  dfdp_dim1 = ndim;
  dfdu_dim1 = ndim;
    
  ndm = iap->ndm;

  /* Generate the function. */

  ffc2(iap, rap, ndim, u, uold, icp, par, f, ndm, 
       iap->lib->scratch.dfu.data(), iap->lib->scratch.dfp.data());

  if (ijac == 0) {
    return 0;
  }

  /* Generate the Jacobian. */

  xpp::auto_jacobian::central(
      ndim, ndim, u, iap->lib->scratch.uu1.data(), iap->lib->scratch.uu2.data(), iap->lib->scratch.ff1.data(), iap->lib->scratch.ff2.data(),
      [&](const doublereal *uu, doublereal *ff) { ffc2(iap, rap, ndim, uu, uold, icp, par, ff, ndm, iap->lib->scratch.dfu.data(), iap->lib->scratch.dfp.data()); },
      dfdu, dfdu_dim1);

  for (i = 0; i < ndim; ++i) {
    ARRAY2D(dfdp, i, (icp[0])) = 0.;
  }
  ARRAY2D(dfdp, (ndim - 1), (icp[0])) = 1.;

  return 0;
} /* fnc2_ */

int 
ffc2(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp)
{
  integer dfdu_dim1, dfdp_dim1;

  integer icpm;

  integer nfpr, i, j;
  doublereal fop;
  std::vector<doublereal> ddp(iap->lib->parameter_count);
  integer ndm2;

  std::vector<doublereal> ddu(iap->ndim);

  dfdp_dim1 = ndm;
  dfdu_dim1 = ndm;
    
  nfpr = iap->nfpr;

  for (i = 1; i < nfpr; ++i) {
    par[icp[i]] = u[(ndm * 2) + i];
  }
  funi(iap, rap, ndm, u, uold, icp, par, 2, f, dfdu, dfdp);
  fopi(iap, rap, ndm, u, icp, par, 2, &fop, ddu.data(), ddp.data());

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

int 
stpnc2(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *u)
{

  integer ndim;

  doublereal uold = 0.; /* funi/fnds take it, unread */
  integer nfpr;
  integer i, j;
  logical found;

  std::vector<doublereal> dp(iap->lib->parameter_count);

  integer ndm;
  doublereal fop;
  integer irs;

  std::vector<doublereal> f(iap->ndim);
  std::vector<doublereal> v(iap->ndim);
  std::vector<doublereal> dd(xpp::size_product(iap->ndim, iap->ndim));
  std::vector<doublereal> du(iap->ndim);
  /* Generates starting data for the continuation equations for */
  /* optimization of algebraic systems (More than one parameter). */

  ndim = iap->ndim;
  irs = iap->irs;
  ndm = iap->ndm;

  findlb(iap, rap, irs, &nfpr, &found);
  ++nfpr;
  iap->nfpr = nfpr;
  readlb(iap, rap, u, par);

  if (nfpr == 3) {
    funi(iap, rap, ndm, u, &uold, icp, par, 2, f.data(), 
	 iap->lib->scratch.dfu.data(), iap->lib->scratch.dfp.data());
    fopi(iap, rap, ndm, u, icp, par, 2, &fop, du.data(), 
	 dp.data());
    /*       TRANSPOSE */
    for (i = 0; i < ndm; ++i) {
      for (j = 0; j < ndm; ++j) {
	dd[i + j * ndim] = iap->lib->scratch.dfu[i * ndm + j];
      }
    }
    for (i = 0; i < ndm; ++i) {
      dd[i + ndm * ndim] = du[i];
      dd[ndm + i * ndim] = iap->lib->scratch.dfp[(icp[1]) * ndm + i];
    }
    dd[ndm + ndm * ndim] = dp[icp[1]];
    xpp::nlvc(iap->lib->fp9, ndm + 1, ndim, 1, dd.data(), v.data());
    {
      integer tmp = ndm + 1;
      nrmlz(&tmp, v.data());
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

int 
fnds(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp)
{
  integer dfdu_dim1;

  integer i;

  /* Generate the equations for continuing fixed points. */

  dfdu_dim1 = ndim;
    
  funi(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp);

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

int 
fnti(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp)
{
  integer dfdu_dim1, dfdp_dim1;

  doublereal told;
  integer i, j;
  doublereal dt;

  /* Generate the equations for continuing fixed points. */

  dfdp_dim1 = ndim;
  dfdu_dim1 = ndim;
    
  funi(iap, rap, ndim, u, uold, icp, par, ijac, f, 
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
} /* fnti */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*     Subroutines for Hopf (ODE) and Torus (Map) Curves */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

int 
fnhb(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp)
{
  return xpp::auto_jacobian::curve(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp, true, ffhb);
} /* fnhb_ */

int 
ffhb(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp)
{
  // AUTO-07p equilibrium.f90:190-258, Kuznetsov (10.80), reimplemented:
  // State = [equilibrium, real vector, omega^2, second parameter].
  // For maps use theta and B=A-I: B^2+2(1-cos(theta))*(B+I),
  // reimplemented from maps.f90 FFNSX (Kuznetsov 10.83).
  par[icp[1]] = u[ndim - 1];
  const bool map=iap->ips==xpp::AutoProblem::DiscreteMap;
  if (map) fnds(iap, rap, ndm, u, uold, icp, par, 2, f, dfdu, dfdp);
  else funi(iap, rap, ndm, u, uold, icp, par, 2, f, dfdu, dfdp);
  const xpp::Matrix<double> blocks({dfdu, static_cast<size_t>(ndm * ndim)}, ndm, ndim);
  const double kappa = u[2 * ndm];
  const double shift=map ? 2*(1-xpp::math::cos(kappa)) : kappa;
  // The square goes into columns ndm..2*ndm-1, which share nothing with the Jacobian's.
  for (integer i = 0; i < ndm; ++i)
    for (integer j = 0; j < ndm; ++j)
      blocks(i, ndm + j) = (i == j ? shift : 0.) + (map ? shift*blocks(i,j) : 0.);
  xpp::product_accumulate(&blocks(0, ndm), ndm, &blocks(0, 0), ndm, &blocks(0, 0), ndm, ndm, ndm, ndm);
  for (integer i = 0; i < ndm; ++i) {
    blocks(i, 2 * ndm) = map ? 2*xpp::math::sin(kappa)*u[ndm+i] : u[ndm+i];
    if (map) for (integer j=0;j<ndm;++j) blocks(i,2*ndm)+=2*xpp::math::sin(kappa)*blocks(i,j)*u[ndm+j];
    blocks(i, ndim - 1) = 0.;
    for (integer j = 0; j < ndm; ++j)
      blocks(i, ndim - 1) += (blocks(j, i) - blocks(i, j)) * uold[ndm + j];
  }
  f[2 * ndm] = -1.;
  f[ndim - 1] = 0.;
  for (integer i = 0; i < ndm; ++i) {
    f[ndm + i] = 0.;
    for (integer j = 0; j < ndm; ++j) f[ndm + i] += blocks(i, ndm + j) * u[ndm + j];
    f[2 * ndm] += u[ndm + i] * u[ndm + i];
    f[ndim - 1] += blocks(i, ndim - 1) * u[ndm + i];
  }
  return 0;
} /* ffhb_ */

int 
stpnhb(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *u)
{
  // AUTO-07p equilibrium.f90:278-323: a unit null vector of A^2+kappa I.
  integer free = 0;
  logical found = false;
  findlb(iap, rap, iap->irs, &free, &found);
  if (!found) auto_restart_label_missing(iap);
  readlb(iap, rap, u, par);
  const integer n = iap->ndm;
  const double omega = pi(2.) / par[xpp::auto_unknown::period];
  u[2 * n] = iap->ips==xpp::AutoProblem::DiscreteMap ? omega : omega * omega;
  u[iap->ndim - 1] = par[icp[1]];
  std::vector<double> old(iap->ndim), residual(iap->ndim);
  std::vector<double> blocks(xpp::size_product(n, iap->ndim)), parameters(xpp::size_product(n, iap->lib->parameter_count));
  ffhb(iap, rap, iap->ndim, u, old.data(), icp, par, residual.data(), n, blocks.data(), parameters.data());
  integer size = n;
  // The Hopf pair gives a two-dimensional kernel. NLVC chooses its phase;
  // subsequent correctors use the previous vector's symmetric condition.
  xpp::nlvc(iap->lib->fp9, n, n, 2, blocks.data() + n * n, u + n);
  nrmlz(&size, u + n);
  return 0;
} /* stpnhb_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*   Subroutines for the Continuation of Hopf Bifurcation Points (Waves) */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

int 
fnhw(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp)
{
  return xpp::auto_jacobian::problem(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp, xpp::auto_jacobian::principal_parameter_count, ffhw);
} /* fnhw_ */

int 
ffhw(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp)
{
  integer dfdu_dim1;

  integer ijac;

  integer i, j;
  doublereal rom;
  integer ndm2;

  dfdu_dim1 = ndm;
    
  ndm2 = ndm * 2;

  rom = u[-1 + ndim - 1];
  par[icp[1]] = u[-1 + ndim];
  ijac = 1;
  fnws(iap, rap, ndm, u, uold, icp, par, ijac, f, 
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

int 
stpnhw(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *u)
{
  integer ijac, ndim;

  doublereal uold = 0.;

  integer nfpr1;
  integer i, j;
  logical found;

  doublereal period;
  integer ndm, irs;
  doublereal rom;
  integer ndm2;

  std::vector<doublereal> smat(xpp::size_product(xpp::size_product(2, iap->ndim), xpp::size_product(2, iap->ndim)));
  std::vector<doublereal> f(iap->ndim);
  std::vector<doublereal> v(iap->ndim);
  std::vector<doublereal> dfp(xpp::size_product(iap->ndim, iap->lib->parameter_count));
  std::vector<doublereal> dfu(xpp::size_product(iap->ndim, iap->ndim));

  /* Generates starting data for the continuation of a bifurcation to a */
  /* traveling wave. */

  /* Local (Can't use BLLOC here.) */

  ndim = iap->ndim;
  irs = iap->irs;
  ndm = iap->ndm;

  findlb(iap, rap, irs, &nfpr1, &found);
  readlb(iap, rap, u, par);

  ijac = 1;
  period = par[xpp::auto_unknown::period];
  rom = period / pi(2.0);
  fnws(iap, rap, ndm, u, &uold, icp, par, ijac, f.data(), dfu.data(), 
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
      smat[i + j * (ndim*2)] = rom * iap->lib->scratch.dfu[j * ndm + i];
      smat[ndm + i + (ndm + j) * (ndim*2)] = rom * iap->lib->scratch.dfu[j * ndm + i];
    }
  }
  {
    integer tmp=(ndim*2);
    xpp::nlvc(iap->lib->fp9, ndm2, tmp, 2, smat.data(), v.data());
  }
  nrmlz(&ndm2, v.data());

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

int 
fnps(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp)
{
  integer dfdu_dim1, dfdp_dim1;

  integer i, j;
  doublereal period;

/* Generates the equations for the continuation of periodic orbits. */

/* Generate the function. */

  dfdp_dim1 = ndim;
  dfdu_dim1 = ndim;
    
  if (icp[1] == xpp::auto_unknown::period) {
    /*          **Variable period continuation */
    funi(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp);
    period = par[xpp::auto_unknown::period];
    for (i = 0; i < ndim; ++i) {
      ARRAY2D(dfdp, i, xpp::auto_unknown::period) = f[i];
      f[i] = period * ARRAY2D(dfdp, i, xpp::auto_unknown::period);
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
    period = par[xpp::auto_unknown::period];
    funi(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp);
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

int 
bcps(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nbc, const doublereal *u0, const doublereal *u1, doublereal *f, integer ijac, doublereal *dbc)
{
  integer dbc_dim1;

  integer jtmp, i, j, nn;

  dbc_dim1 = nbc;
  
  for (i = 0; i < ndim; ++i) {
    f[i] = u0[i] - u1[i];
  }

  /* Rotations */
  if (iap->lib->rotations.irtn != 0) {
    for (i = 0; i < ndim; ++i) {
      if (iap->lib->rotations.nrtn[i] != 0) {
	f[i] += par[xpp::auto_unknown::rotation_period] * iap->lib->rotations.nrtn[i];
      }
    }
  }

  if (ijac == 0) {
    return 0;
  }

  jtmp = iap->lib->parameter_count;
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

int 
icps(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nint, const doublereal *u, const doublereal *uold, const doublereal *udot, const doublereal *upold, doublereal *f, integer ijac, doublereal *dint)
{
  integer dint_dim1;

  integer jtmp, i, nn;

  dint_dim1 = nint;
  
  // Center on the computed orbit: discrete quadrature need not make uold dot upold zero.
  f[0] = 0.;
  for (i = 0; i < ndim; ++i) {
    if (iap->lib->rotations.nrtn[i] == 0) {
      f[0] += (u[i] - uold[i]) * upold[i];
    }
  }

  if (ijac == 0) {
    return 0;
  }

  jtmp = iap->lib->parameter_count;
  nn = ndim + jtmp;
  for (i = 0; i < nn; ++i) {
    ARRAY2D(dint, 0, i) = 0.;
  }

  for (i = 0; i < ndim; ++i) {
    if (iap->lib->rotations.nrtn[i] == 0) {
      ARRAY2D(dint, 0, i) = upold[i];
    } else {
      ARRAY2D(dint, 0, i) = 0.;
    }
  }

  return 0;
} /* icps_ */

int 
pdble(const iap_type *iap, const rap_type *rap, integer *ndim, integer *ntst, integer *ncol, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *tm, doublereal *par)
{
  integer ups_dim1, udotps_dim1;

  integer i, j, i1, i2;

  /* Preprocesses restart data for switching branches at a period doubling */

  udotps_dim1 = *ndxloc;
  ups_dim1 = *ndxloc;
    
  par[xpp::auto_unknown::period] *= 2.;
  if (iap->lib->rotations.irtn != 0) {
    par[xpp::auto_unknown::rotation_period] *= 2.;
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

int 
stpnps(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *ntsr, integer *ncolrs, doublereal *rlcur, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *upoldp, doublereal *tm, doublereal *dtm, integer *nodir, doublereal *thl, doublereal *thu)
{

  integer ndim;

  doublereal uold = 0.;
  integer nfpr, ndim2, nfpr1;

  integer i, j;
  doublereal rimhb;
  logical found;

  doublereal period;

  doublereal tpi;
  integer irs;

  std::vector<doublereal> smat(xpp::size_product(xpp::size_product(iap->ndim, 2), xpp::size_product(iap->ndim, 2)));
  std::vector<doublereal> rnllv(xpp::size_product(xpp::size_product(iap->ndim, 2), xpp::size_product(iap->ndim, 2)));
  std::vector<doublereal> f(iap->ndim);
  std::vector<doublereal> u(iap->ndim);
  /* Generates starting data for the continuation of a branch of periodic */
  /* solutions from a Hopf bifurcation point. */

  ndim = iap->ndim;
  irs = iap->irs;
  nfpr = iap->nfpr;

  findlb(iap, rap, irs, &nfpr1, &found);
  readlb(iap, rap, u.data(), par);

  for (i = 0; i < nfpr; ++i) {
    rlcur[i] = par[icp[i]];
  }

  period = par[xpp::auto_unknown::period];
  tpi = pi(2.0);
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

  funi(iap, rap, ndim, u.data(), &uold, icp, par, 1, f.data(), 
       iap->lib->scratch.dfu.data(), iap->lib->scratch.dfp.data());

  for (i = 0; i < ndim; ++i) {
    for (j = 0; j < ndim; ++j) {
      smat[i + (ndim + j) * (ndim*2)] = iap->lib->scratch.dfu[j * ndim + i];
      smat[ndim + i + j * (ndim*2)] = iap->lib->scratch.dfu[j * ndim + i];
    }
  }

  {
    integer tmp=(ndim*2);
    xpp::nlvc(iap->lib->fp9, ndim2, tmp, 2, smat.data(), rnllv.data());
  }
  nrmlz(&ndim2, rnllv.data());

/* Generate the (initially uniform) mesh. */

  auto_hopf_mesh(iap, rap, *ndxloc, ups, udotps, upoldp, tm, dtm,
                 rldot, u.data(), rnllv.data(), tpi);

  scaleb(iap, icp, ndxloc, udotps, rldot, dtm, thl, thu);

  *nodir = -1;

  return 0;
} /* stpnps_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*          Travelling Wave Solutions to Parabolic PDEs */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

int 
fnws(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp)
{

  integer ndm, ndm2;

  /* Sets up equations for the continuation of spatially homogeneous */
  /* solutions to parabolic systems, for the purpose of finding */
  /* bifurcations to travelling wave solutions. */

  ndm = iap->ndm;

  /* Generate the function. */

  ndm2 = ndm / 2;
  ffws(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp, ndm2, iap->lib->scratch.dfu.data(), 
       iap->lib->scratch.dfp.data());

  return 0;
} /* fnws_ */

int 
ffws(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp, integer ndm, doublereal *dfu, doublereal *dfp)
{
  integer dfdu_dim1, dfdp_dim1, dfu_dim1, dfp_dim1;

  integer nfpr;
  doublereal c;
  integer i, j;

  dfdp_dim1 = ndim;
  dfdu_dim1 = ndim;
  dfp_dim1 = ndm;
  dfu_dim1 = ndm;
    
  nfpr = iap->nfpr;

  c = par[xpp::auto_unknown::wave_speed];
  funi(iap, rap, ndm, u, uold, icp, par, ijac, f, dfu, dfp);

  for (i = 0; i < ndm; ++i) {
    f[ndm + i] = -(c * u[ndm + i] + f[i]) / par[i + xpp::auto_unknown::diffusion_start];
    f[i] = u[ndm + i];
  }

  if (ijac == 0) {
    return 0;
  }

  for (i = 0; i < ndm; ++i) {
    for (j = 0; j < ndm; ++j) {
      ARRAY2D(dfdu, i, j) = 0.;
      ARRAY2D(dfdu, i, (j + ndm)) = 0.;
      ARRAY2D(dfdu, i + ndm, j) = -ARRAY2D(dfu, i, j) / par[i + xpp::auto_unknown::diffusion_start];
      ARRAY2D(dfdu, i + ndm, (j + ndm)) = 0.;
    }
    ARRAY2D(dfdu, i, (i + ndm)) = 1.;
    ARRAY2D(dfdu, i + ndm, (i + ndm)) = -c / par[i + xpp::auto_unknown::diffusion_start];
    if (auto_model_parameter(*iap->lib->session,icp[0])) {
      ARRAY2D(dfdp, i, (icp[0])) = 0.;
      ARRAY2D(dfdp, i + ndm, icp[0]) = -ARRAY2D(dfp, i, icp[0]) / par[i + xpp::auto_unknown::diffusion_start];
    }
    if (nfpr > 1 && auto_model_parameter(*iap->lib->session,icp[1])) {
      ARRAY2D(dfdp, i, (icp[1])) = 0.;
      ARRAY2D(dfdp, i + ndm, icp[1]) = -ARRAY2D(dfp, i, icp[1]) / par[i + xpp::auto_unknown::diffusion_start];
    }
  }

  /* Derivative with respect to the wave speed. */

  for (i = 0; i < ndm; ++i) {
    ARRAY2D(dfdp, i, xpp::auto_unknown::wave_speed) = 0.;
    ARRAY2D(dfdp, i + ndm, xpp::auto_unknown::wave_speed) = -u[ndm + i] / par[i + xpp::auto_unknown::diffusion_start];
  }

  /* Derivatives with respect to the diffusion coefficients. */

  for (j = 0; j < ndm; ++j) {
    for (i = 0; i < ndm; ++i) {
      ARRAY2D(dfdp, i, (j + xpp::auto_unknown::diffusion_start)) = 0.;
      ARRAY2D(dfdp, i + ndm, (j + xpp::auto_unknown::diffusion_start)) = 0.;
    }
    ARRAY2D(dfdp, j + ndm, (j + xpp::auto_unknown::diffusion_start)) = -f[j + ndm] / par[j + xpp::auto_unknown::diffusion_start];
  }

  return 0;
} /* ffws_ */

int 
fnwp(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp)
{
  integer dfdu_dim1, dfdp_dim1;

  integer i, j;
  doublereal period;

/* Equations for the continuation of traveling waves. */

/* Generate the function and Jacobian. */

  dfdp_dim1 = ndim;
  dfdu_dim1 = ndim;
    
  if (icp[1] == xpp::auto_unknown::period) {
    /*          **Variable wave length */
    fnws(iap, rap, ndim, u, uold, icp, par, ijac, f, 
	 dfdu, dfdp);
    period = par[xpp::auto_unknown::period];
    for (i = 0; i < ndim; ++i) {
      ARRAY2D(dfdp, i, xpp::auto_unknown::period) = f[i];
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
    fnws(iap, rap, ndim, u, uold, icp, par, ijac, f, 
	 dfdu, dfdp);
    period = par[xpp::auto_unknown::period];
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

int 
stpnwp(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *ntsr, integer *ncolrs, doublereal *rlcur, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *upoldp, doublereal *tm, doublereal *dtm, integer *nodir, doublereal *thl, doublereal *thu)
{

  integer ndim;

  doublereal uold = 0.;
  integer nfpr;

  integer ndim2, nfpr1;

  integer i, j;
  doublereal rimhb;
  logical found;

  doublereal period;

  doublereal tpi;
  integer irs;

  std::vector<doublereal> smat(xpp::size_product(xpp::size_product(2, iap->ndim), xpp::size_product(2, iap->ndim)));
  std::vector<doublereal> f(iap->ndim);
  std::vector<doublereal> u(iap->ndim);
  std::vector<doublereal> rnllv(xpp::size_product(2, iap->ndim));
  std::vector<doublereal> dfp(xpp::size_product(iap->ndim, iap->lib->parameter_count));
  std::vector<doublereal> dfu(xpp::size_product(iap->ndim, iap->ndim));

/* Generates starting data for the continuation of a branch of periodic */
/* solutions starting from a Hopf bifurcation point (Waves). */

/* Local (Can't use BLLOC here.) */

  ndim = iap->ndim;
  irs = iap->irs;
  nfpr = iap->nfpr;

  findlb(iap, rap, irs, &nfpr1, &found);
  readlb(iap, rap, u.data(), par);

  for (i = 0; i < nfpr; ++i) {
    rlcur[i] = par[icp[i]];
  }

  period = par[xpp::auto_unknown::period];
  tpi = pi(2.0);
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

  fnws(iap, rap, ndim, u.data(), &uold, icp, par, 1, f.data(), dfu.data(), dfp.data());

  for (i = 0; i < ndim; ++i) {
    for (j = 0; j < ndim; ++j) {
      smat[i + (ndim + j) * (ndim*2)] = dfu[j * ndim + i];
      smat[ndim + i + j * (ndim*2)] = dfu[j * ndim + i];
    }
  }

  xpp::nlvc(iap->lib->fp9, ndim2, ndim*2, 2, smat.data(), rnllv.data());
  nrmlz(&ndim2, rnllv.data());

  /* Generate the (initially uniform) mesh. */

  auto_hopf_mesh(iap, rap, *ndxloc, ups, udotps, upoldp, tm, dtm,
                 rldot, u.data(), rnllv.data(), tpi);

  scaleb(iap, icp, ndxloc, udotps, rldot, dtm, thl, thu);

  *nodir = -1;

  return 0;
} /* stpnwp_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*             Parabolic PDEs : Stationary States */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

int 
fnsp(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp)
{
  return xpp::auto_jacobian::supplied(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp, ffsp);
} /* fnsp_ */

int 
ffsp(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp, integer ndm, doublereal *dfu, doublereal *dfp)
{
  integer dfdu_dim1, dfdp_dim1, dfu_dim1, dfp_dim1;

  integer i, j;
  doublereal period;

  dfdp_dim1 = ndim;
  dfdu_dim1 = ndim;
  dfp_dim1 = ndm;
  dfu_dim1 = ndm;
    
  funi(iap, rap, ndm, u, uold, icp, par, ijac, &f[ndm], dfu, dfp);

  period = par[xpp::auto_unknown::period];
  for (i = 0; i < ndm; ++i) {
    f[i] = period * u[ndm + i];
    f[ndm + i] = -period * f[ndm + i] / par[i + xpp::auto_unknown::diffusion_start];
  }

  if (ijac == 0) {
    return 0;
  }

  for (i = 0; i < ndm; ++i) {
    for (j = 0; j < ndm; ++j) {
      ARRAY2D(dfdu, i, j) = 0.;
      ARRAY2D(dfdu, i, (j + ndm)) = 0.;
      ARRAY2D(dfdu, i + ndm, j) = -period * ARRAY2D(dfu, i, j) / par[i + xpp::auto_unknown::diffusion_start];
      ARRAY2D(dfdu, i + ndm, (j + ndm)) = 0.;
    }
    ARRAY2D(dfdu, i, (i + ndm)) = period;
    if (icp[0] == xpp::auto_unknown::period) {
      ARRAY2D(dfdp, i, (icp[0])) = f[i] / period;
      ARRAY2D(dfdp, ndm + i, icp[0]) = f[ndm + i] / period;
    } else if (icp[0] == i + xpp::auto_unknown::diffusion_start - 1) {
      ARRAY2D(dfdp, i, (icp[0])) = 0.;
      ARRAY2D(dfdp, ndm + i, icp[0]) = -f[ndm + i] / par[i + xpp::auto_unknown::diffusion_start];
    } else if (icp[0] != xpp::auto_unknown::period && ! (icp[0] > xpp::auto_unknown::diffusion_start - 1 && icp[0] <= ndm + xpp::auto_unknown::diffusion_start - 1)) {
      ARRAY2D(dfdp, i, (icp[0])) = 0.;
      ARRAY2D(dfdp, i + ndm, icp[0]) = -period * ARRAY2D(dfp, i, icp[0]) / par[i + xpp::auto_unknown::diffusion_start];
    }
  }

  return 0;
} /* ffsp_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*            Time Evolution of Parabolic PDEs */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

int 
fnpe(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp)
{
  return xpp::auto_jacobian::supplied(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp, ffpe);
} /* fnpe_ */

int 
ffpe(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp, integer ndm, doublereal *dfu, doublereal *dfp)
{
  integer dfdu_dim1, dfdp_dim1, dfu_dim1;

  integer i, j;
  doublereal t, dsmin, rlold, ds, dt, period;

  dfdp_dim1 = ndim;
  dfdu_dim1 = ndim;
  dfu_dim1 = ndm;
    
  ds = rap->ds;
  dsmin = rap->dsmin;

  period = par[xpp::auto_unknown::period];
  t = par[icp[0]];
  rlold = rap->tivp;
  dt = t - rlold;
  if (f2c::abs(dt) < dsmin) {
    dt = ds;
  }

  funi(iap, rap, ndm, u, uold, icp, par, ijac, &f[ndm], dfu, dfp);

  for (i = 0; i < ndm; ++i) {
    f[i] = period * u[ndm + i];
    f[ndm + i] = period * ((u[i] - uold[i]) / dt - f[ndm + i]) /par[i + xpp::auto_unknown::diffusion_start];
  }

  if (ijac == 0) {
    return 0;
  }

  for (i = 0; i < ndm; ++i) {
    for (j = 0; j < ndm; ++j) {
      ARRAY2D(dfdu, i, j) = 0.;
      ARRAY2D(dfdu, i, (j + ndm)) = 0.;
      ARRAY2D(dfdu, i + ndm, j) = -period * ARRAY2D(dfu, i, j) / par[i + xpp::auto_unknown::diffusion_start];
      ARRAY2D(dfdu, i + ndm, (j + ndm)) = 0.;
    }
    ARRAY2D(dfdu, i, (i + ndm)) = period;
    ARRAY2D(dfdu, i + ndm, i) += period / (dt * par[i + xpp::auto_unknown::diffusion_start]);
    ARRAY2D(dfdp, i, (icp[0])) = 0.;
    /* Computing 2nd power */
    ARRAY2D(dfdp, i + ndm, icp[0]) = -period * (u[i] - uold[i])/ (dt * dt * par[i + xpp::auto_unknown::diffusion_start]);
  }

  return 0;
} /* ffpe_ */

int 
icpe(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nint, const doublereal *u, const doublereal *uold, const doublereal *udot, const doublereal *upold, doublereal *f, integer ijac, doublereal *dint)
{

  /* Dummy integral condition subroutine for parabolic systems. */

  return 0;
} /* icpe_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*    Subroutines for the Continuation of Folds for Periodic Solution */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

int 
fnpl(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp)
{
  // FNPLF's block structure (AUTO-07p periodic.f90:427-507): only
  // the derivatives of the variational equations need second differences.
  const integer n = iap->ndm;
  auto &scratch = iap->lib->scratch;
  const auto evaluate = [&](const double *state, double *value, integer derivatives) {
    ffpl(iap, rap, ndim, state, uold, icp, par, derivatives, value, n,
         scratch.dfu.data(), scratch.dfp.data());
  };
  evaluate(u, f, ijac == 0 ? 1 : 2);
  if (ijac == 0) return 0;
  const double period = par[xpp::auto_unknown::period];
  const bool variable_period = icp[2] == xpp::auto_unknown::period;
  const xpp::Matrix<const double> a(scratch.dfu, n, n);
  const xpp::Matrix<const double> p(scratch.dfp, n, iap->lib->parameter_count);
  const xpp::Matrix<double> jac({dfdu, xpp::size_product(ndim, ndim)}, ndim, ndim);
  const xpp::Matrix<double> dp({dfdp, xpp::size_product(ndim, iap->lib->parameter_count)}, ndim, iap->lib->parameter_count);
  std::fill_n(dfdu, ndim * ndim, 0.);
  std::fill_n(dfdp, ndim * iap->lib->parameter_count, 0.);
  for (integer i = 0; i < n; ++i) {
    for (integer j = 0; j < n; ++j) {
      jac(i, j) = period * a(i, j);
      jac(n + i, n + j) = jac(i, j);
    }
    for (integer k = 0; k < iap->nfpr; ++k)
      dp(i, icp[k]) = period * p(i, icp[k]);
    if (variable_period) {
      dp(i, xpp::auto_unknown::period) = f[i] / period;
      dp(n + i, xpp::auto_unknown::period) =
          (f[n + i] - par[xpp::auto_unknown::fold_beta] * dp(i, xpp::auto_unknown::period)) / period;
    }
    dp(n + i, xpp::auto_unknown::fold_beta) =
        variable_period ? f[i] / period : p(i, icp[1]);
  }
  // central() copies the physical states; keep the unperturbed eigenvector
  // in the tail of both preallocated scratch states.
  std::copy_n(u, ndim, scratch.uu1.data());
  std::copy_n(u, ndim, scratch.uu2.data());
  const double step = xpp::auto_jacobian::central(
      n, n, u, scratch.uu1.data(), scratch.uu2.data(),
      scratch.ff1.data(), scratch.ff2.data(),
      [&](const double *state, double *value) {
        evaluate(state, value, 1);
        std::copy_n(value + n, n, value);
      },
      dfdu + n, ndim);
  if (ijac == 1) return 0;
  for (integer k = 0; k < iap->nfpr; ++k) {
    const integer parameter = icp[k];
    if (parameter == xpp::auto_unknown::period ||
        parameter == xpp::auto_unknown::fold_beta ||
        parameter == xpp::auto_unknown::fold_norm) continue;
    const double saved = par[parameter];
    xpp::auto_jacobian::forward_parameters(
        1, n, icp + k, step, par,
        [&](double *value) {
          evaluate(u, scratch.ff2.data(), 1);
          std::copy_n(scratch.ff2.data() + n, n, value);
        }, f + n, scratch.ff1.data(), dfdp + n, ndim, 0);
    par[parameter] = saved;
  }
  return 0;
} /* fnpl_ */

int
ffpl(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp)
{
  integer dfdu_dim1, dfdp_dim1;

  doublereal beta;

  integer i, j;
  doublereal period;

  dfdp_dim1 = ndm;
  dfdu_dim1 = ndm;
    
  period = par[xpp::auto_unknown::period];
  beta = par[xpp::auto_unknown::fold_beta];
  funi(iap, rap, ndm, u, uold, icp, par, icp[2] == xpp::auto_unknown::period ? ijac : 2, f,
       dfdu, dfdp);

  for (i = 0; i < ndm; ++i) {
    f[ndm + i] = 0.;
    for (j = 0; j < ndm; ++j) {
      f[ndm + i] += ARRAY2D(dfdu, i, j) * u[ndm + j];
    }
    if (icp[2] == xpp::auto_unknown::period) {
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

int 
bcpl(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nbc, const doublereal *u0, const doublereal *u1, doublereal *f, integer ijac, doublereal *dbc)
{
  integer dbc_dim1;
  integer jtmp, i, j, nn, ndm;

  /* Boundary conditions for continuing folds (Periodic solutions) */

  dbc_dim1 = nbc;
  
  for (i = 0; i < ndim; ++i) {
    f[i] = u0[i] - u1[i];
  }

  /* Rotations */
  if (iap->lib->rotations.irtn != 0) {
    ndm = iap->ndm;
    for (i = 0; i < ndm; ++i) {
      if (iap->lib->rotations.nrtn[i] != 0) {
	f[i] += par[xpp::auto_unknown::rotation_period] * iap->lib->rotations.nrtn[i];
      }
    }
  }

  if (ijac == 0) {
    return 0;
  }

  jtmp = iap->lib->parameter_count;
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

int 
icpl(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nint, const doublereal *u, const doublereal *uold, const doublereal *udot, const doublereal *upold, doublereal *f, integer ijac, doublereal *dint)
{
  integer dint_dim1;

  integer jtmp, i, j, nn, ndm;

  /* Integral conditions for continuing folds (Periodic solutions) */

  dint_dim1 = nint;
  
  ndm = iap->ndm;

  icps(iap, rap, ndm, par, icp, nint, u, uold, udot, upold, f, 0, nullptr);
  f[1] = 0.;
  /* Computing 2nd power */
  f[2] = par[xpp::auto_unknown::fold_beta] * par[xpp::auto_unknown::fold_beta] - par[xpp::auto_unknown::fold_norm];

  for (i = 0; i < ndm; ++i) {
    if (iap->lib->rotations.nrtn[i] == 0) {
      f[1] += u[ndm + i] * upold[i];
    }
    f[2] += u[ndm + i] * u[ndm + i];
  }

  if (ijac == 0) {
    return 0;
  }

  jtmp = iap->lib->parameter_count;
  nn = ndim + jtmp;
  for (i = 0; i < nint; ++i) {
    for (j = 0; j < nn; ++j) {
      ARRAY2D(dint, i, j) = 0.;
    }
  }

  for (i = 0; i < ndm; ++i) {
    if (iap->lib->rotations.nrtn[i] == 0) {
      ARRAY2D(dint, 0, i) = upold[i];
      ARRAY2D(dint, 1, ndm + i) = upold[i];
    } else {
      ARRAY2D(dint, 0, i) = 0.;
      ARRAY2D(dint, 1, ndm + i) = 0.;
    }
    ARRAY2D(dint, 2, ndm + i) = u[ndm + i] * 2.;
  }

  ARRAY2D(dint, 2, ndim + xpp::auto_unknown::fold_beta) = par[xpp::auto_unknown::fold_beta] * 2.;
  ARRAY2D(dint, 2, ndim + xpp::auto_unknown::fold_norm) = -1.;

  return 0;
} /* icpl_ */

int 
stpnpl(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *ntsr, integer *ncolrs, doublereal *rlcur, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *upoldp, doublereal *tm, doublereal *dtm, integer *nodir, doublereal *thl, doublereal *thu)
{

  integer ups_dim1, udotps_dim1;

  integer ndim;
  integer nfpr, nrsp1, i, j, k;

  integer k1, k2;
  std::vector<integer> icprs(iap->lib->parameter_count);

  doublereal rd1, rd2;
  integer ndm;

  /* Generates starting data for the 2-parameter continuation of folds */
  /* on a branch of periodic solutions. */

  udotps_dim1 = *ndxloc;
  ups_dim1 = *ndxloc;

  ndim = iap->ndim;
  ndm = iap->ndm;
  nfpr = iap->nfpr;

  xpp::TokenReader tr;
  xpp::AutoSolutionHeader header;
  read_auto_restart_header(iap, rap, tr, header, ntsr, ncolrs);
  nrsp1 = *ntsr + 1;

  auto_restart_read(iap, xpp::read_auto_solution_rows(tr, ndim, *ntsr, *ncolrs, *ndxloc,
                                    0, ndm, ups, tm, false, 0, {}),
                    "the solution rows");
  auto_restart_read(iap, tr.read(icprs[0]), "the free parameter indices");
  auto_restart_read(iap, tr.read(icprs[1]), "the free parameter indices");
  auto_restart_read(iap, tr.read(rd1), "the parameter derivatives");
  auto_restart_read(iap, tr.read(rd2), "the parameter derivatives");

  /* Read U-dot (derivative with respect to arclength). */

  auto_restart_read(iap, xpp::read_auto_solution_rows(tr, ndim, *ntsr, *ncolrs, *ndxloc,
                                    0, ndm, udotps, nullptr, false, 0, {}),
                    "the arclength derivatives");
  /* Read the parameter values. */

  auto_restart_read(iap, xpp::read_auto_solution_parameters(tr, header, *iap->lib->session, par), "the parameter values");

  /* Complement starting data */
  par[xpp::auto_unknown::fold_beta] = 0.;
  par[xpp::auto_unknown::fold_norm] = 0.;
  if (icp[2] == xpp::auto_unknown::period) {
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

int 
fnpd(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp)
{
  return xpp::auto_jacobian::problem(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp, iap->nfpr, ffpd);
} /* fnpd_ */

int 
ffpd(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp)
{
  integer dfdu_dim1;

  integer i, j;
  doublereal period;

  dfdu_dim1 = ndm;
    
  period = par[xpp::auto_unknown::period];
  funi(iap, rap, ndm, u, uold, icp, par, 1, f, dfdu, dfdp);

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

int 
bcpd(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nbc, const doublereal *u0, const doublereal *u1, doublereal *f, integer ijac, doublereal *dbc)
{
  integer dbc_dim1;

  integer jtmp, i, j, nn, ndm;

  /* Generate boundary conditions for the 2-parameter continuation */
  /* of period doubling bifurcations. */

  dbc_dim1 = nbc;
  
  ndm = iap->ndm;

  for (i = 0; i < ndm; ++i) {
    f[i] = u0[i] - u1[i];
    f[ndm + i] = u0[ndm + i] + u1[ndm + i];
  }

  /* Rotations */
  if (iap->lib->rotations.irtn != 0) {
    for (i = 0; i < ndm; ++i) {
      if (iap->lib->rotations.nrtn[i] != 0) {
	f[i] += par[xpp::auto_unknown::rotation_period] * iap->lib->rotations.nrtn[i];
      }
    }
  }

  if (ijac == 0) {
    return 0;
  }

  jtmp = iap->lib->parameter_count;
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

int 
icpd(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nint, const doublereal *u, const doublereal *uold, const doublereal *udot, const doublereal *upold, doublereal *f, integer ijac, doublereal *dint)
{
  integer dint_dim1;

  integer jtmp, i, j, nn, ndm;

  dint_dim1 = nint;
  
  ndm = iap->ndm;

  icps(iap, rap, ndm, par, icp, nint, u, uold, udot, upold, f, 0, nullptr);
  f[1] = -par[xpp::auto_unknown::doubling_norm];

  for (i = 0; i < ndm; ++i) {
    f[1] += u[ndm + i] * u[ndm + i];
  }

  if (ijac == 0) {
    return 0;
  }

  jtmp = iap->lib->parameter_count;
  nn = ndim + jtmp;
  for (i = 0; i < nint; ++i) {
    for (j = 0; j < nn; ++j) {
      ARRAY2D(dint, i, j) = 0.;
    }
  }

  for (i = 0; i < ndm; ++i) {
    if (iap->lib->rotations.nrtn[i] == 0) {
      ARRAY2D(dint, 0, i) = upold[i];
    } else {
      ARRAY2D(dint, 0, i) = 0.;
    }
    ARRAY2D(dint, 1, ndm + i) = u[ndm + i] * 2.;
  }

  ARRAY2D(dint, 1, ndim + xpp::auto_unknown::doubling_norm) = -1.;

  return 0;
} /* icpd_ */

int 
stpnpd(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *ntsr, integer *ncolrs, doublereal *rlcur, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *upoldp, doublereal *tm, doublereal *dtm, integer *nodir, doublereal *thl, doublereal *thu)
{

  integer ups_dim1, udotps_dim1;

  integer ndim;
  integer nfpr, i, j, k;

  integer k1, k2;
  std::vector<integer> icprs(iap->lib->parameter_count);

  integer ndm;

  /* Generates starting data for the 2-parameter continuation of */
  /* period-doubling bifurcations on a branch of periodic solutions. */

  udotps_dim1 = *ndxloc;
  ups_dim1 = *ndxloc;

  ndim = iap->ndim;
  ndm = iap->ndm;
  nfpr = iap->nfpr;

  xpp::TokenReader tr;
  xpp::AutoSolutionHeader header;
  read_auto_restart_header(iap, rap, tr, header, ntsr, ncolrs);

  auto_restart_read(iap, xpp::read_auto_solution_rows(tr, ndim, *ntsr, *ncolrs, *ndxloc,
                                    0, ndm, ups, tm, false, 0, {}),
                    "the solution rows");
  auto_restart_read(iap, tr.read(icprs[0]), "the free parameter indices");
  auto_restart_read(iap, tr.read(icprs[1]), "the free parameter indices");
  auto_restart_read(iap, tr.read(rldot[0]), "the parameter derivatives");
  auto_restart_read(iap, tr.read(rldot[1]), "the parameter derivatives");

  /* Read U-dot (derivative with respect to arclength). */

  auto_restart_read(iap, xpp::read_auto_solution_rows(tr, ndim, *ntsr, *ncolrs, *ndxloc,
                                    0, ndm, udotps, nullptr, false, 0, {}),
                    "the arclength derivatives");
  /* Read the parameter values. */

  auto_restart_read(iap, xpp::read_auto_solution_parameters(tr, header, *iap->lib->session, par), "the parameter values");

  /* Complement starting data */
  par[xpp::auto_unknown::doubling_norm] = 0.;
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

int 
fntr(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp)
{
  return xpp::auto_jacobian::problem(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp, iap->nfpr, fftr);
} /* fntr_ */

int 
fftr(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp)
{
  integer dfdu_dim1;

  integer i, j;
  doublereal period;
  integer ndm2;

  dfdu_dim1 = ndm;
    
  period = par[xpp::auto_unknown::period];
  funi(iap, rap, ndm, u, uold, icp, par, 1, f, dfdu, dfdp);

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

int 
bctr(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nbc, const doublereal *u0, const doublereal *u1, doublereal *f, integer ijac, doublereal *dbc)
{
  integer dbc_dim1;

  integer jtmp, i, j;
  doublereal theta, cs;
  integer nn;
  doublereal ss;
  integer ndm, ndm2;

  dbc_dim1 = nbc;
  
  ndm = iap->ndm;

  ndm2 = ndm << 1;
  theta = par[xpp::auto_unknown::torus_angle];

  ss = xpp::math::sin(theta);
  cs = xpp::math::cos(theta);

  for (i = 0; i < ndm; ++i) {
    f[i] = u0[i] - u1[i];
    f[ndm + i] = u1[ndm + i] - cs * u0[ndm + i] + ss * u0[ndm2 + i];
    f[ndm2 + i] = u1[ndm2 + i] - cs * u0[ndm2 + i] - ss * u0[ndm + i];
  }

  /* Rotations */
  if (iap->lib->rotations.irtn != 0) {
    for (i = 0; i < ndm; ++i) {
      if (iap->lib->rotations.nrtn[i] != 0) {
	f[i] += par[xpp::auto_unknown::rotation_period] * iap->lib->rotations.nrtn[i];
      }
    }
  }

  if (ijac == 0) {
    return 0;
  }

  jtmp = iap->lib->parameter_count;
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
    ARRAY2D(dbc, ndm + i, ((ndim * 2) + xpp::auto_unknown::torus_angle)) = cs * u0[ndm2 + i] + ss * u0[ndm + i];
    ARRAY2D(dbc, ndm2 + i, (ndm + i)) = -ss;
    ARRAY2D(dbc, ndm2 + i, (ndm2 + i)) = -cs;
    ARRAY2D(dbc, ndm2 + i, (ndim + ndm2 + i)) = 1.;
    ARRAY2D(dbc, ndm2 + i, ((ndim * 2) + xpp::auto_unknown::torus_angle)) = ss * u0[ndm2 + i] - cs * u0[ndm + i];
  }

  return 0;
} /* bctr_ */

int 
ictr(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nint, const doublereal *u, const doublereal *uold, const doublereal *udot, const doublereal *upold, doublereal *f, integer ijac, doublereal *dint)
{
  integer dint_dim1;

  integer jtmp, i, j, nn, ndm, ndm2;

  dint_dim1 = nint;
  
  ndm = iap->ndm;
  ndm2 = ndm * 2;

  icps(iap, rap, ndm, par, icp, nint, u, uold, udot, upold, f, 0, nullptr);
  f[1] = 0.;
  f[2] = -par[xpp::auto_unknown::torus_norm];

  for (i = 0; i < ndm; ++i) {
    f[1] = f[1] + u[ndm + i] * uold[ndm2 + i] - u[ndm2 + i] * uold[ndm + i];
    f[2] = f[2] + u[ndm + i] * u[ndm + i] + u[ndm2 + i] * u[ndm2 + i];
  }

  if (ijac == 0) {
    return 0;
  }

  jtmp = iap->lib->parameter_count;
  nn = ndim + jtmp;
  for (i = 0; i < nint; ++i) {
    for (j = 0; j < nn; ++j) {
      ARRAY2D(dint, i, j) = 0.;
    }
  }

  for (i = 0; i < ndm; ++i) {
    if (iap->lib->rotations.nrtn[i] == 0) {
      ARRAY2D(dint, 0, i) = upold[i];
    } else {
      ARRAY2D(dint, 0, i) = 0.;
    }
    ARRAY2D(dint, 1, ndm + i) = uold[ndm2 + i];
    ARRAY2D(dint, 1, ndm2 + i) = -uold[ndm + i];
    ARRAY2D(dint, 2, ndm + i) = u[ndm + i] * 2;
    ARRAY2D(dint, 2, ndm2 + i) = u[ndm2 + i] * 2;
  }

  ARRAY2D(dint, 2, ndim + xpp::auto_unknown::torus_norm) = -1.;

  return 0;
} /* ictr_ */

int 
stpntr(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *ntsr, integer *ncolrs, doublereal *rlcur, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *upoldp, doublereal *tm, doublereal *dtm, integer *nodir, doublereal *thl, doublereal *thu)
{

  integer ups_dim1, udotps_dim1;

  integer ndim;
  integer nfpr, i;

  std::vector<integer> icprs(iap->lib->parameter_count);

  integer ndm;

  /* Generates starting data for the 2-parameter continuation of torus */
  /* bifurcations. */

  udotps_dim1 = *ndxloc;
  ups_dim1 = *ndxloc;
    
  ndim = iap->ndim;
  ndm = iap->ndm;
  nfpr = iap->nfpr;

  xpp::TokenReader tr;
  xpp::AutoSolutionHeader header;
  read_auto_restart_header(iap, rap, tr, header, ntsr, ncolrs);

  auto_restart_read(iap, xpp::read_auto_solution_rows(tr, ndim, *ntsr, *ncolrs, *ndxloc,
                                    0, ndm, ups, tm, false, 0, [&](integer j, integer i, doublereal time) {
        const integer k1 = i * ndim + ndm;
        for (integer k = k1; k < k1 + ndm; ++k) {
          ARRAY2D(ups, j, k) = xpp::math::sin(time) * torus_initial_amplitude;
          ARRAY2D(ups, j, (k + ndm)) = xpp::math::cos(time) * torus_initial_amplitude;
        }
      }), "the solution rows");
  for (i = 0; i < ndm; ++i) {
    ARRAY2D(ups, *ntsr, (ndm + i)) = 0.;
    ARRAY2D(ups, *ntsr, ((ndm * 2) + i)) = 0.;
  }

  auto_restart_read(iap, tr.read(icprs[0]), "the free parameter indices");
  auto_restart_read(iap, tr.read(icprs[1]), "the free parameter indices");
  auto_restart_read(iap, tr.read(rldot[0]), "the parameter derivatives");
  auto_restart_read(iap, tr.read(rldot[1]), "the parameter derivatives");
  rldot[2] = 0.;
  rldot[3] = 0.;

  /* Read the direction vector and initialize the starting direction. */

  auto_restart_read(iap, xpp::read_auto_solution_rows(tr, ndim, *ntsr, *ncolrs, *ndxloc,
                                    0, ndm, udotps, nullptr, false, 0, {}),
                    "the arclength derivatives");
  // Preserve the original interior direction columns; only the endpoint
  // initialized the added torus columns (the old interior loop was empty).
  for (i = 0; i < ndm; ++i) {
    ARRAY2D(udotps, *ntsr, (ndm + i)) = 0.;
    ARRAY2D(udotps, *ntsr, ((ndm * 2) + i)) = 0.;
  }

  /* Read the parameter values. */

  auto_restart_read(iap, xpp::read_auto_solution_parameters(tr, header, *iap->lib->session, par), "the parameter values");

  par[xpp::auto_unknown::torus_norm] = 0.;

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

int 
fnpo(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp)
{
  integer dfdu_dim1, dfdp_dim1;

  integer nfpr;
  integer i;
  doublereal ep, period;
  integer ndm;

  std::vector<doublereal> upold(iap->ndim);

  /* Generates the equations for periodic optimization problems. */

  dfdp_dim1 = ndim;
  dfdu_dim1 = ndim;
    
  ndm = iap->ndm;
  nfpr = iap->nfpr;

/* Generate F(UOLD) */

  funi(iap, rap, ndm, uold, uold, icp, par, 0, 
       upold.data(), dfdu, dfdp);
  period = par[xpp::auto_unknown::period];
  for (i = 0; i < ndm; ++i) {
    upold[i] = period * upold[i];
  }

  /* Generate the function. */

  ffpo(iap, rap, ndim, u, uold, upold.data(), icp, par, f,
       ndm, iap->lib->scratch.dfu.data(), iap->lib->scratch.dfp.data());

  if (ijac == 0) {
    return 0;
  }

  /* Generate the Jacobian. */

  ep = xpp::auto_jacobian::central(
      ndim, ndim, u, iap->lib->scratch.uu1.data(), iap->lib->scratch.uu2.data(), iap->lib->scratch.ff1.data(), iap->lib->scratch.ff2.data(),
      [&](const doublereal *uu, doublereal *ff) { ffpo(iap, rap, ndim, uu, uold, upold.data(), icp, par, ff, ndm, iap->lib->scratch.dfu.data(), iap->lib->scratch.dfp.data()); },
      dfdu, dfdu_dim1);

  xpp::auto_jacobian::forward_parameters(
      nfpr, ndim, icp, ep, par,
      [&](doublereal *ff) { ffpo(iap, rap, ndim, u, uold, upold.data(), icp, par, ff, ndm, iap->lib->scratch.dfu.data(), iap->lib->scratch.dfp.data()); },
      f, iap->lib->scratch.ff1.data(), dfdp, dfdp_dim1, 0);

  return 0;
} /* fnpo_ */

int 
ffpo(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const doublereal *upold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp)
{
  integer dfdu_dim1, dfdp_dim1;

  integer i, j;
  doublereal gamma, rkappa, period, fop;
  std::vector<doublereal> dfp(iap->lib->parameter_count);

  std::vector<doublereal> dfu(iap->ndim);

  dfdp_dim1 = ndm;
  dfdu_dim1 = ndm;
    
  period = par[xpp::auto_unknown::period];
  rkappa = par[xpp::auto_unknown::optimization_kappa];
  gamma = par[xpp::auto_unknown::optimization_gamma];

  for (i = 0; i < ndm; ++i) {
    for (j = 0; j < iap->lib->parameter_count; ++j) {
      ARRAY2D(dfdp, i, j) = 0.;
    }
  }
  funi(iap, rap, ndm, u, uold, icp, par, 1, f, 
       dfdu, dfdp);
  for (i = 0; i < iap->lib->parameter_count; ++i) {
    dfp[i] = 0.;
  }
  fopi(iap, rap, ndm, u, icp, par, 1, &fop, dfu.data(), dfp.data());

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

int 
bcpo(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nbc, const doublereal *u0, const doublereal *u1, doublereal *f, integer ijac, doublereal *dbc)
{
  integer dbc_dim1;

  integer nfpr, i, j, nbc0;

  /* Generates the boundary conditions for periodic optimization problems. 
*/

  dbc_dim1 = nbc;
  
  nfpr = iap->nfpr;

  for (i = 0; i < nbc; ++i) {
    f[i] = u0[i] - u1[i];
  }

  /* Rotations */
  if (iap->lib->rotations.irtn != 0) {
    nbc0 = iap->nbc0;
    for (i = 0; i < nbc0; ++i) {
      if (iap->lib->rotations.nrtn[i] != 0) {
	f[i] += par[xpp::auto_unknown::rotation_period] * iap->lib->rotations.nrtn[i];
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

int 
icpo(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nint, const doublereal *u, const doublereal *uold, const doublereal *udot, const doublereal *upold, doublereal *f, integer ijac, doublereal *dint)
{
  integer dint_dim1;

  integer nfpr;

  doublereal ep;
  integer ndm;
  integer nnt0;

  std::vector<doublereal> f1(iap->nint);
  std::vector<doublereal> f2(iap->nint);
  std::vector<doublereal> dnt(xpp::size_product(iap->nint, xpp::size_sum(iap->ndim, iap->lib->parameter_count)));

/* Generates integral conditions for periodic optimization problems. */

  dint_dim1 = nint;
  
  ndm = iap->ndm;
  nnt0 = iap->nnt0;
  nfpr = iap->nfpr;

  /* Generate the function. */

  fipo(iap, rap, ndim, par, icp, nint, nnt0, u, uold, 
       udot, upold, f, dnt.data(), ndm, iap->lib->scratch.dfu.data(), iap->lib->scratch.dfp.data());

  if (ijac == 0) {
    return 0;
  }

  /* Generate the Jacobian. */

  ep = xpp::auto_jacobian::central(
      ndim, nint, u, iap->lib->scratch.uu1.data(), iap->lib->scratch.uu2.data(), f1.data(), f2.data(),
      [&](const doublereal *uu, doublereal *ff) { fipo(iap, rap, ndim, par, icp, nint, nnt0, uu, uold, udot, upold, ff, dnt.data(), ndm, iap->lib->scratch.dfu.data(), iap->lib->scratch.dfp.data()); },
      dint, dint_dim1);

  xpp::auto_jacobian::forward_parameters(
      nfpr, nint, icp, ep, par,
      [&](doublereal *ff) { fipo(iap, rap, ndim, par, icp, nint, nnt0, u, uold, udot, upold, ff, dnt.data(), ndm, iap->lib->scratch.dfu.data(), iap->lib->scratch.dfp.data()); },
      f, f1.data(), dint, dint_dim1, ndim);

  return 0;
} /* icpo_ */

int 
fipo(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nint, integer nnt0, const doublereal *u, const doublereal *uold, const doublereal *udot, const doublereal *upold, doublereal *fi, doublereal *dint, integer ndmt, doublereal *dfdu, doublereal *dfdp)
{
  integer dfdp_dim1;

  integer nfpr, indx;
  integer i, j, l;
  std::vector<doublereal> dfp(iap->lib->parameter_count);
  integer ndm;
  doublereal fop;

  std::vector<doublereal> f(iap->ndim);
  std::vector<doublereal> dfu(iap->ndim);

  dfdp_dim1 = ndmt;

  ndm = iap->ndm;
  nfpr = iap->nfpr;

  icps(iap, rap, ndm, par, icp, nint, u, uold, udot, upold, fi, 0, nullptr);

  for (i = 0; i < iap->lib->parameter_count; ++i) {
    dfp[i] = 0.;
  }
  fopi(iap, rap, ndm, u, icp, par, 2, &fop, dfu.data(), dfp.data());
  fi[1] = par[xpp::auto_unknown::objective] - fop;

  /* Computing 2nd power */
  fi[2] = par[xpp::auto_unknown::optimization_kappa] * par[xpp::auto_unknown::optimization_kappa] + par[xpp::auto_unknown::optimization_gamma] * par[xpp::auto_unknown::optimization_gamma] - par[xpp::auto_unknown::optimization_norm];
  for (i = 0; i < ndm; ++i) {
    /* Computing 2nd power */
    fi[2] +=  u[ndm + i] * u[ndm + i];
  }

  for (i = 0; i < ndm; ++i) {
    for (j = 0; j < iap->lib->parameter_count; ++j) {
      ARRAY2D(dfdp, i, j) = 0.;
    }
  }
  funi(iap, rap, ndm, u, uold, icp, par, 2, f.data(), dfdu, dfdp);

  for (l = 3; l < nint; ++l) {
    indx = icp[nfpr + l - 3];
    if (indx == xpp::auto_unknown::period) {
      fi[l] = -par[xpp::auto_unknown::optimization_gamma] * dfp[indx] - par[indx + xpp::auto_unknown::test_function_start];
      for (i = 0; i < ndm; ++i) {
	fi[l] += f[i] * u[ndm + i];
      }
    } else {
      fi[l] = -par[xpp::auto_unknown::optimization_gamma] * dfp[indx] - par[indx + xpp::auto_unknown::test_function_start];
      for (i = 0; i < ndm; ++i) {
	fi[l] += par[xpp::auto_unknown::period] * ARRAY2D(dfdp, i, (indx)) * u[ndm + i]
          ;
      }
    }
  }

  return 0;
} /* fipo_ */

int 
stpnpo(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *ntsr, integer *ncolrs, doublereal *rlcur, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *upoldp, doublereal *tm, doublereal *dtm, integer *nodir, doublereal *thl, doublereal *thu)
{

  integer ups_dim1;

  integer ndim;
  integer nfpr;
  doublereal dump;

  doublereal dumu;
  integer i, j, k;

  std::vector<integer> icprs(iap->lib->parameter_count);

  integer k1, k2;
  doublereal fs;

  integer ndm;
  doublereal rld1, rld2;

  integer temporary_storage_dim1;
  // Objective samples use the target collocation layout and ndxloc stride.
  std::vector<doublereal> temporary_storage(xpp::size_product(*ndxloc, xpp::size_product(iap->ndim, iap->ncol)));
  std::vector<doublereal> u(iap->ndim);

  /* Generates starting data for optimization of periodic solutions. */

  ups_dim1 = *ndxloc;
  temporary_storage_dim1 = *ndxloc;
  
  ndim = iap->ndim;
  ndm = iap->ndm;
  nfpr = iap->nfpr;

  xpp::TokenReader tr;
  xpp::AutoSolutionHeader header;
  read_auto_restart_header(iap, rap, tr, header, ntsr, ncolrs);

  auto_restart_read(iap, xpp::read_auto_solution_rows(tr, ndim, *ntsr, *ncolrs, *ndxloc,
                                    0, ndm, ups, tm, false, 0, {}),
                    "the solution rows");
  for (j = 0; j < *ntsr; ++j) dtm[j] = tm[j + 1] - tm[j];

  auto_restart_read(iap, tr.read(icprs[0]), "the free parameter indices");
  auto_restart_read(iap, tr.read(icprs[1]), "the free parameter indices");
  auto_restart_read(iap, tr.read(rld1), "the parameter derivatives");
  auto_restart_read(iap, tr.read(rld2), "the parameter derivatives");

  /* Read U-dot (derivative with respect to arclength). */
  auto_restart_read(iap, xpp::read_auto_solution_rows(tr, ndim, *ntsr, *ncolrs, *ndxloc,
                                    0, ndm, udotps, nullptr, false, 0, {}),
                    "the arclength derivatives");
  /* Read the parameter values. */
  auto_restart_read(iap, xpp::read_auto_solution_parameters(tr, header, *iap->lib->session, par), "the parameter values");

  for (j = 0; j < *ntsr; ++j) {
    for (i = 0; i < *ncolrs; ++i) {
      k1 = i * ndim;
      k2 = k1 + ndm - 1;
      for (k = k1; k <= k2; ++k) {
	u[k - k1] = ARRAY2D(ups, j, k);
      }
      fopt(ndm, u.data(), icp, par, 0, &fs, &dumu, &dump);
      ARRAY2D(temporary_storage, j, k1) = fs;
    }
  }
  for (k = 0; k < ndm; ++k) {
    u[k] = ARRAY2D(ups, *ntsr, k);
  }
  fopt(ndm, u.data(), icp, par, 0, &fs, &dumu, &dump);
  temporary_storage[*ntsr] = fs;
  par[xpp::auto_unknown::objective] = rintg(iap, ndxloc, 1, temporary_storage.data(), dtm);

  /* Complement starting data */

  for (i = xpp::auto_unknown::optimization_norm; i < iap->lib->parameter_count; ++i) {
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

int 
fnbl(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp)
{
  return xpp::auto_jacobian::problem(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp, iap->nfpr, ffbl);
} /* fnbl_ */

int 
ffbl(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp)
{
  integer dfdu_dim1, dfdp_dim1;

  integer nfpr, nfpx, i, j;

  dfdp_dim1 = ndm;
  dfdu_dim1 = ndm;
    
  nfpr = iap->nfpr;

  funi(iap, rap, ndm, u, uold, icp, par, 2, f, 
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

int 
bcbl(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nbc, const doublereal *u0, const doublereal *u1, doublereal *f, integer ijac, doublereal *dbc)
{
  integer dbc_dim1;

  integer nfpr;

  doublereal ep;
  integer nbc0;

  std::vector<doublereal> ff1(iap->nbc);
  std::vector<doublereal> ff2(iap->nbc);
  std::vector<doublereal> uu1(iap->ndim);
  std::vector<doublereal> uu2(iap->ndim);
  std::vector<doublereal> dfu(xpp::size_product(iap->nbc, xpp::size_sum(xpp::size_product(2, iap->ndim), iap->lib->parameter_count)));
		     
/* Generates the boundary conditions for the 2-parameter continuation */
/* of folds (BVP). */

  dbc_dim1 = nbc;
  
  nbc0 = iap->nbc0;
  nfpr = iap->nfpr;

  /* Generate the function. */

  fbbl(iap, rap, ndim, par, icp, nbc, nbc0, u0, u1, f, dfu.data());

  if (ijac == 0) {
    return 0;
  }

  /* Derivatives with respect to U0. */

  ep = xpp::auto_jacobian::central(
      ndim, nbc, u0, uu1.data(), uu2.data(), ff1.data(), ff2.data(),
      [&](const doublereal *uu, doublereal *ff) { fbbl(iap, rap, ndim, par, icp, nbc, nbc0, uu, u1, ff, dfu.data()); },
      dbc, dbc_dim1);

  /* Derivatives with respect to U1. */

  ep = xpp::auto_jacobian::central(
      ndim, nbc, u1, uu1.data(), uu2.data(), ff1.data(), ff2.data(),
      [&](const doublereal *uu, doublereal *ff) { fbbl(iap, rap, ndim, par, icp, nbc, nbc0, u0, uu, ff, dfu.data()); },
      dbc + ndim * dbc_dim1, dbc_dim1);

  xpp::auto_jacobian::forward_parameters(
      nfpr, nbc, icp, ep, par,
      [&](doublereal *ff) { fbbl(iap, rap, ndim, par, icp, nbc, nbc0, u0, u1, ff, dfu.data()); },
      f, ff2.data(), dbc, dbc_dim1, ndim * 2);

  return 0;
} /* bcbl_ */

int 
fbbl(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nbc, integer nbc0, const doublereal *u0, const doublereal *u1, doublereal *f, doublereal *dbc)
{
  integer dbc_dim1;

  integer nfpr, nfpx, i, j, ndm;

  dbc_dim1 = nbc0;
  
  ndm = iap->ndm;
  nfpr = iap->nfpr;

  nfpx = nfpr / 2 - 1;
  bcni(iap, rap, ndm, par, icp, nbc0, u0, u1, f, 2, dbc);
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

int 
icbl(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nint, const doublereal *u, const doublereal *uold, const doublereal *udot, const doublereal *upold, doublereal *f, integer ijac, doublereal *dint)
{
  return xpp::auto_jacobian::integral(iap, rap, ndim, par, icp, nint, u, uold, udot, upold, f, ijac, dint, fibl);
} /* icbl_ */

int 
fibl(const iap_type *iap, const rap_type *rap, const integer ndim, doublereal *par, const integer *icp, integer nint, integer nnt0, const doublereal *u, const doublereal *uold, const doublereal *udot, const doublereal *upold, doublereal *f, doublereal *dint)
{
  integer dint_dim1;

  integer nfpr, nfpx=0, i, j, ndm;

  dint_dim1 = nnt0;
  
  ndm = iap->ndm;
  nfpr = iap->nfpr;

  if (nnt0 > 0) {
    nfpx = nfpr / 2 - 1;
    icni(iap, rap, ndm, par, icp, nnt0, u, uold, udot, upold, f, 2, dint);
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
  f[-1 + nint] = -par[-1 + nfpr / 2 + xpp::auto_unknown::bvp_fold_start + 1];
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

int 
stpnbl(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *ntsr, integer *ncolrs, doublereal *rlcur, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *upoldp, doublereal *tm, doublereal *dtm, integer *nodir, doublereal *thl, doublereal *thu)
{

  integer ndim;
  integer nfpr, nfpx, nfpr0, i;

  std::vector<integer> icprs(iap->lib->parameter_count);

  integer ndm;

  /* Generates starting data for the 2-parameter continuation of folds. */
  /* (BVP). */

  ndim = iap->ndim;
  ndm = iap->ndm;
  nfpr = iap->nfpr;

  xpp::TokenReader tr;
  xpp::AutoSolutionHeader header;
  read_auto_restart_header(iap, rap, tr, header, ntsr, ncolrs);

  auto_restart_read(iap, xpp::read_auto_solution_rows(tr, ndim, *ntsr, *ncolrs, *ndxloc,
                                    0, ndm, ups, tm, false, 0, {}),
                    "the solution rows");
  nfpr0 = nfpr / 2;
  auto_restart_read(iap, tr.read(icprs[0]), "the free parameter indices");
  for (i = 0; i < nfpr0; ++i) {
    auto_restart_read(iap, tr.read(rldot[i]), "the parameter derivatives");
  }

  /* Read U-dot (Derivative with respect to arclength). */

  auto_restart_read(iap, xpp::read_auto_solution_rows(tr, ndim, *ntsr, *ncolrs, *ndxloc,
                                    ndm, ndim - ndm, ups, nullptr, false, 0, {}),
                    "the arclength derivatives");
  /* Read the parameter values. */

  auto_restart_read(iap, xpp::read_auto_solution_parameters(tr, header, *iap->lib->session, par), "the parameter values");

  nfpx = nfpr / 2 - 1;
  if (nfpx > 0) {
    for (i = 0; i < nfpx; ++i) {
      par[icp[nfpr0 + 1 + i]] = rldot[i + 1];
    }
  }
  /* Initialize the norm of the null vector */
  par[-1 + nfpr / 2 + xpp::auto_unknown::bvp_fold_start + 1] = 0.;

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

int 
funi(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp)
{
  integer dfdu_dim1, dfdp_dim1;

  integer nfpr;
  doublereal rtmp;
  integer i;
  doublereal ep;
  integer jac, ijc;

  auto &u1zz = iap->lib->scratch.funi_u1;
  auto &u2zz = iap->lib->scratch.funi_u2;
  auto &f1zz = iap->lib->scratch.funi_f1;
  auto &f2zz = iap->lib->scratch.funi_f2;

  /* Interface subroutine to user supplied FUNC. */

  dfdp_dim1 = ndim;
  dfdu_dim1 = ndim;
    
  jac = iap->jac;
  nfpr = iap->nfpr;

/* Generate the function. */

  ijc = jac == auto_jac_differences ? 0 : ijac;
  func(*iap->lib->session, ndim, u, icp, nfpr, par, ijc, f, dfdu,
       dfdp, iap->lib);

  if (jac == auto_jac_supplied || ijac == 0) {
    return 0;
  }

  /* Generate the Jacobian by differencing. The run preallocates these;
     direct evaluator calls also establish their own required extent. */
  u1zz.resize(ndim);
  u2zz.resize(ndim);
  f1zz.resize(ndim);
  f2zz.resize(ndim);

  ep = xpp::auto_jacobian::central(
      ndim, ndim, u, u1zz.data(), u2zz.data(), f1zz.data(), f2zz.data(),
      [&](const doublereal *uu, doublereal *ff) { func(*iap->lib->session, ndim, uu, icp, nfpr, par, 0, ff, dfdu, dfdp, iap->lib); },
      dfdu, dfdu_dim1);

  if (ijac == 1) {
    return 0;
  }

  for (i = 0; i < nfpr; ++i) {
    rtmp = auto_hmach;
    ep = rtmp * (f2c::abs(par[icp[i]]) + 1);
    xpp::auto_jacobian::forward_parameters(
        1, ndim, icp + i, ep, par,
        [&](doublereal *ff) { func(*iap->lib->session, ndim, u, icp, nfpr, par, 0, ff, dfdu, dfdp, iap->lib); },
        f, f1zz.data(), dfdp, dfdp_dim1, 0);
  }

  return 0;
} /* funi */

int 
bcni(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nbc, const doublereal *u0, const doublereal *u1, doublereal *f, integer ijac, doublereal *dbc)
{
  integer dbc_dim1;

  integer nfpr;
  doublereal rtmp;
  integer i;
  doublereal ep;

  std::vector<doublereal> u1zz(iap->ndim);
  std::vector<doublereal> u2zz(iap->ndim);
  std::vector<doublereal> f1zz(iap->nbc);
  std::vector<doublereal> f2zz(iap->nbc);

  /* Interface subroutine to the user supplied BCND. */

  dbc_dim1 = nbc;

  nfpr = iap->nfpr;

  /* Generate the function. */

  /* xppautX's bcnd supplies no derivatives: they are differenced whatever
     JAC says (JAC=1 is the model's exact Jacobian in func, W260) */
  bcnd(*iap->lib->session, ndim, par, icp, nbc, u0, u1, 0, f, dbc);

  if (ijac == 0) {
    return 0;
  }

  /* Generate the Jacobian by differencing. */

  ep = xpp::auto_jacobian::central(
      ndim, nbc, u0, u1zz.data(), u2zz.data(), f1zz.data(), f2zz.data(),
      [&](const doublereal *uu, doublereal *ff) { bcnd(*iap->lib->session, ndim, par, icp, nbc, uu, u1, 0, ff, dbc); },
      dbc, dbc_dim1);

  ep = xpp::auto_jacobian::central(
      ndim, nbc, u1, u1zz.data(), u2zz.data(), f1zz.data(), f2zz.data(),
      [&](const doublereal *uu, doublereal *ff) { bcnd(*iap->lib->session, ndim, par, icp, nbc, u0, uu, 0, ff, dbc); },
      dbc + ndim * dbc_dim1, dbc_dim1);

  if (ijac == 1) {
    return 0;
  }

  for (i = 0; i < nfpr; ++i) {
    rtmp = auto_hmach;
    ep = rtmp * (f2c::abs(par[icp[i]]) + 1);
    xpp::auto_jacobian::forward_parameters(
        1, nbc, icp + i, ep, par,
        [&](doublereal *ff) { bcnd(*iap->lib->session, ndim, par, icp, nbc, u0, u1, 0, ff, dbc); },
        f, f1zz.data(), dbc, dbc_dim1, ndim * 2);
  }

  return 0;
} /* bcni */

int 
icni(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nint, const doublereal *u, const doublereal *uold, const doublereal *udot, const doublereal *upold, doublereal *f, integer ijac, doublereal *dint)
{
  integer dint_dim1;

  integer nfpr;
  doublereal rtmp;
  integer i;
  doublereal ep;

  std::vector<doublereal> f1zz(iap->nint);
  std::vector<doublereal> f2zz(iap->nint);
  std::vector<doublereal> u1zz(iap->ndim);
  std::vector<doublereal> u2zz(iap->ndim);
  /* Interface subroutine to user supplied ICND. */

  dint_dim1 = nint;

  nfpr = iap->nfpr;

  /* Generate the integrand. */

  /* xppautX's icnd supplies no derivatives: they are differenced whatever
     JAC says (JAC=1 is the model's exact Jacobian in func, W260) */
  icnd(ndim, par, icp, nint, u, uold, udot, upold,
       0, f, dint);

  if (ijac == 0) {
    return 0;
  }

  /* Generate the Jacobian by differencing. */

  ep = xpp::auto_jacobian::central(
      ndim, nint, u, u1zz.data(), u2zz.data(), f1zz.data(), f2zz.data(),
      [&](const doublereal *uu, doublereal *ff) { icnd(ndim, par, icp, nint, uu, uold, udot, upold, 0, ff, dint); },
      dint, dint_dim1);

  if (ijac == 1) {
    return 0;
  }

  for (i = 0; i < nfpr; ++i) {
    rtmp = auto_hmach;
    ep = rtmp * (f2c::abs(par[icp[i]]) + 1);
    xpp::auto_jacobian::forward_parameters(
        1, nint, icp + i, ep, par,
        [&](doublereal *ff) { icnd(ndim, par, icp, nint, u, uold, udot, upold, 0, ff, dint); },
        f, f1zz.data(), dint, dint_dim1, ndim);
  }

  return 0;
} /* icni */

int 
fopi(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp)
{

  integer nfpr;

  doublereal rtmp;
  integer i;
  doublereal f1, f2, ep;

  std::vector<doublereal> u1zz(iap->ndim);
  std::vector<doublereal> u2zz(iap->ndim);

  /* Interface subroutine to user supplied FOPT. */

  nfpr = iap->nfpr;

  /* Generate the objective function. */

  /* xppautX's fopt supplies no derivatives: they are differenced whatever
     JAC says (JAC=1 is the model's exact Jacobian in func, W260) */
  fopt(ndim, u, icp, par, 0, f, dfdu, dfdp);

  if (ijac == 0) {
    return 0;
  }

  /* Generate the Jacobian by differencing. */

  ep = xpp::auto_jacobian::central(
      ndim, 1, u, u1zz.data(), u2zz.data(), &f1, &f2,
      [&](const doublereal *uu, doublereal *ff) { fopt(ndim, uu, icp, par, 0, ff, dfdu, dfdp); },
      dfdu, 1);

  if (ijac == 1) {
    return 0;
  }

  for (i = 0; i < nfpr; ++i) {
    rtmp = auto_hmach;
    ep = rtmp * (f2c::abs(par[icp[i]]) + 1);
    xpp::auto_jacobian::forward_parameters(
        1, 1, icp + i, ep, par,
        [&](doublereal *ff) { fopt(ndim, u, icp, par, 0, ff, dfdu, dfdp); },
        f, &f1, dfdp, 1, 0);
  }

  return 0;
} /* fopi */
