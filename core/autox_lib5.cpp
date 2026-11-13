#include "xpp_mem.h"
#include "autox_unknowns.h"
/* translated from autoX's autox_lib5.f by f2c */

#include <vector>
#include "xpp_io.h"
#include "session.h"
#include "autox_evd.h" /* xAutox (its own extern) */
#include "autox_jacobian.h"
#include "xpp_math.h"

/* Finite-difference scratch and HomCont state belong to iap->run. W247
   gives each collocation worker its own scratch and a copy of the HomCont
   settings; boundary-condition updates stay on the calling thread. */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*        Subroutines for Homoclinic Bifurcation Analysis */
/*       (A. R. Champneys, Yu. A. Kuznetsov, B. Sandstede) */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/* fnho: AUTO's equations of the problem kind homoclinic (homcont.f90: HomCont's homoclinic problem (IPS 9)) */
int 
homoclinic_equations(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp)
{
  return xpp::autox_jacobian::problem(iap, rap, ndim, u, uold, icp, par, ijac, f, dfdu, dfdp, iap->nfpr, homoclinic_function);
} /* fnho_ */

/* ffho: AUTO's function evaluation of the problem kind homoclinic (homcont.f90: HomCont's homoclinic problem (IPS 9)) */
int 
homoclinic_function(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp)
{
  integer dfdu_dim1;

  integer i, j;
  doublereal dum1;

  dfdu_dim1 = ndm;
    
  ndm = iap->ndm;

  if (iap->run->homcont.homoclinic_constants.itwist == 0) {
    /*        *Evaluate the R.-H. sides */
    problem_function(iap, rap, ndm, u, uold, icp, par, 0,
	 f, dfdu, dfdp);
  } else {
    /*        *Adjoint variational equations for normal vector */
    problem_function(iap, rap, ndm, u, uold, icp, par, 1,
	 f, dfdu, dfdp);
    /*        *Set F = - (Df)^T u */
    for (j = 0; j < ndm; ++j) {
      dum1 = 0.;
      for (i = 0; i < ndm; ++i) {
	dum1 += ARRAY2D(dfdu, i, j) * u[ndm + i];
      }
      f[ndm + j] = -dum1;
    }
    /*        *Set F =  F + PAR(10) * f */
    for (j = 0; j < ndm; ++j) {
      f[ndm + j] += par[xpp::autox_unknown::homoclinic_adjoint] * f[j];
    }
  }

  /* Scale by truncation interval T=PAR(11) */

  for (i = 0; i < ndim; ++i) {
    f[i] = par[xpp::autox_unknown::period] * f[i];
  }

  return 0;
} /* ffho_ */

/* bcho: AUTO's boundary conditions of the problem kind homoclinic (homcont.f90: HomCont's homoclinic problem (IPS 9)) */
int 
homoclinic_boundary_conditions(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nbc, const doublereal *u0, const doublereal *u1, doublereal *f, integer ijac, doublereal *dbc)
{
  integer dbc_dim1;

  integer nfpr;

  doublereal ep;
  integer nbc0;

  std::vector<doublereal> ff1(iap->nbc);
  std::vector<doublereal> ff2(iap->nbc);
  std::vector<doublereal> uu1(iap->ndim);
  std::vector<doublereal> uu2(iap->ndim);
  std::vector<doublereal> dfu(xpp::size_product(iap->nbc, xpp::size_sum(xpp::size_product(2, iap->ndim), iap->run->parameter_count)));

/* Generates the boundary conditions for homoclinic bifurcation analysis 
*/

  dbc_dim1 = nbc;
  
  nbc0 = iap->nbc0;
  nfpr = iap->nfpr;

/* Generate the function. */

  homoclinic_boundary_function(iap, rap, ndim, par, icp, nbc, nbc0, &u0[0], &u1[0], &
       f[0], dfu.data(), true);

  if (ijac == 0) {
    return 0;
  }

  /* Derivatives with respect to U0. */

  ep = xpp::autox_jacobian::central(
      ndim, nbc, u0, uu1.data(), uu2.data(), ff1.data(), ff2.data(),
      [&](const doublereal *uu, doublereal *ff) { homoclinic_boundary_function(iap, rap, ndim, par, icp, nbc, nbc0, uu, u1, ff, dfu.data(), false); },
      dbc, dbc_dim1);

  /* Derivatives with respect to U1. */

  ep = xpp::autox_jacobian::central(
      ndim, nbc, u1, uu1.data(), uu2.data(), ff1.data(), ff2.data(),
      [&](const doublereal *uu, doublereal *ff) { homoclinic_boundary_function(iap, rap, ndim, par, icp, nbc, nbc0, u0, uu, ff, dfu.data(), false); },
      dbc + ndim * dbc_dim1, dbc_dim1);

  xpp::autox_jacobian::forward_parameters(
      nfpr, nbc, icp, ep, par,
      [&](doublereal *ff) { homoclinic_boundary_function(iap, rap, ndim, par, icp, nbc, nbc0, u0, u1 , ff, dfu.data(), false); },
      f, ff2.data(), dbc, dbc_dim1, ndim * 2);

  return 0;
} /* bcho_ */

/* fbho: AUTO's boundary function evaluation of the problem kind homoclinic (homcont.f90: HomCont's homoclinic problem (IPS 9)) */
int 
homoclinic_boundary_function(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nbc, integer nbc0, const doublereal *u0, const doublereal *u1, doublereal *fb, doublereal *dbc, bool save_basis)
{

  integer ieig;

  integer i, j, k;

  integer ineig;

  integer jb;
  integer ip;
  integer kp;

  integer ijc = 0, ndm;
  doublereal dum, dum1, dum2;

    /* I am not 100% sure if this is supposed to be iap->ndm or iap->ndim,
       but it appears from looking at the code that it should be iap->ndm.
       Also, note that I have replaced the occurances of N X in the algorithms
       with (iap->ndm), so if you change it to iap->ndim here you will
       need to make the similiar changes in the algorithms.
       Finally, the routines called from here homoclinic_projection and homoclinic_eigen
       also depend on these arrays, and more importantly the algorithm,
       having N X.  So, they all need to be changed at once.
    */
  std::vector<doublereal> f(iap->ndm);
  std::vector<doublereal> bound(xpp::size_product(iap->ndm, iap->ndm));
  std::vector<doublereal> fj(iap->ndm);
  std::vector<doublereal> ri(iap->ndm);
  std::vector<doublereal> rr(iap->ndm);
  std::vector<doublereal> vr(xpp::size_product(iap->ndm, iap->ndm));
  std::vector<doublereal> vt(xpp::size_product(iap->ndm, iap->ndm));
  std::vector<doublereal> xequib1(iap->ndm);
  std::vector<doublereal> xequib2(iap->ndm);

  /* Generates the boundary conditions for homoclinic orbits. */

  ndm = iap->ndm;

  /*     *Initialization */
  for (i = 1; i <= nbc; ++i) {
    fb[-1 + i] = 0.;
  }
  jb = 1;

/*     *Update pu0,pu1 */
  for (i = 0; i < ndim; ++i) {
    iap->run->homcont.homoclinic_endpoints.pu0[i] = u0[i];
    iap->run->homcont.homoclinic_endpoints.pu1[i] = u1[i];
  }

  if (iap->run->homcont.homoclinic_constants.iequib == 0 || iap->run->homcont.homoclinic_constants.iequib == -1) {
    user_output_parameters(ndm, u0, par);
  }
  /*              write(9,*) 'Xequib:' */
  for (i = 0; i < ndm; ++i) {
    xequib1[i] = par[i + xpp::autox_unknown::homoclinic_equilibrium_start];
    /*              write(9,*) I,XEQUIB1(I) */
  }
  if (iap->run->homcont.homoclinic_constants.iequib >= 0) {
    for (i = 0; i < ndm; ++i) {
      xequib2[i] = par[i + xpp::autox_unknown::homoclinic_equilibrium_start];
    }
  } else {
    for (i = 0; i < ndm; ++i) {
      xequib2[i] = par[ndm + xpp::autox_unknown::homoclinic_equilibrium_start + i];
    }
  }

  /*     **Regular Continuation** */
  if (iap->run->homcont.homoclinic_constants.istart != 3) {
    /*        *Projection boundary conditions for the homoclinic orbit */
    /*        *NSTAB boundary conditions at t=0 */
    homoclinic_projection(iap, bound.data(), xequib1.data(), icp, par, xpp::HomcontManifold::stable, xpp::HomcontEndpoint::left, true, &ndm, save_basis);
    for (i = 0; i < iap->run->homcont.homoclinic_constants.nstab; ++i) {
      for (k = 0; k < ndm; ++k) {
	fb[-1 + jb] += (u0[k] - xequib1[k]) * bound[i + k * (iap->ndm)];
      }
      /*         write(9,*) 'fb',jb,fb(jb) */
      ++jb;
    }
    /*        *NUNSTAB boundary conditions at t=1 */
    if (iap->run->homcont.homoclinic_constants.nrev == 0) {
      homoclinic_projection(iap, bound.data(), xequib2.data(), icp, par, xpp::HomcontManifold::unstable, xpp::HomcontEndpoint::right, true, &ndm, save_basis);
      for (i = ndm - iap->run->homcont.homoclinic_constants.nunstab; i < ndm; ++i) {
	for (k = 0; k < ndm; ++k) {
	  fb[-1 + jb] += (u1[k] - xequib2[k]) * bound[i + k * (iap->ndm)];
	}
	++jb;
      }
    } else {
      /*         *NUNSTAB symmetric boundary conditions at t=1 if NREV=1
       */

      for (i = 0; i < ndim; ++i) {
	if (iap->run->homcont.homoclinic_indices.irev[i] > 0) {
	  if (iap->run->homcont.homoclinic_indices.irev[i] == 1) {
	    /* *****NOTE MODIFICATION FROM GENERAL CASE */
	    fb[-1 + jb] = xpp::math::sin(u1[i]);
	    /*                        FB(JB)=U1(I) */
	  } else {
	    fb[-1 + jb] = u1[i];
	  }
	  ++jb;
	}
      }
    }
    ieig = 0;
    ineig = 0;
    /*        *NFIXED extra boundary conditions for the fixed conditions 
     */
    if (iap->run->homcont.homoclinic_constants.nfixed > 0) {
      if (ieig == 0) {
	homoclinic_eigen(iap, 1, 2, rr.data(), ri.data(), vr.data(), xequib1.data(), icp, par, &ndm);
	ieig = 1;
      }
      for (i = 0; i < iap->run->homcont.homoclinic_constants.nfixed; ++i) {
	if (iap->run->homcont.homoclinic_indices.ifixed[i] > 10 && ineig == 0) {
	  homoclinic_eigen(iap, 1, 1, rr.data(), ri.data(), vt.data(), xequib1.data(), icp, par, &ndm);
	  ineig = 1;
	}
	fb[-1 + jb] = homoclinic_degeneracy_condition(iap, iap->run->homcont.homoclinic_indices.ifixed[i], rr.data(), ri.data(), vr.data(),vt.data(), icp, par);
	++jb;
      }
    }
    /*        *NDM initial conditions for the equilibrium if IEQUIB=1,2,-2
     */
    if (iap->run->homcont.homoclinic_constants.iequib != 0 && iap->run->homcont.homoclinic_constants.iequib != -1) {
      user_right_hand_side(*iap->run, ndm, xequib1.data(), icp, iap->nfpr, par, 0, f.data(), &dum1, &dum2);
      for (i = 0; i < ndm; ++i) {
	fb[-1 + jb] = f[i];
	++jb;
      }
      /*        *NDM extra initial conditions for the equilibrium if IEQ
		UIB=-2 */
      if (iap->run->homcont.homoclinic_constants.iequib == -2) {
	user_right_hand_side(*iap->run, ndm, xequib2.data(), icp, iap->nfpr, par, 0, f.data(), &dum1, &dum2);
	for (i = 0; i < ndm; ++i) {
	  fb[-1 + jb] = f[i];
	  ++jb;
	}
      }
    }
    /*       *extra boundary condition in the case of a saddle-node homocl
	     inic*/
    if (iap->run->homcont.homoclinic_constants.iequib == 2) {
      if (ineig == 0) {
	homoclinic_eigen(iap, 1, 1, rr.data(), ri.data(), vt.data(), xequib1.data(), icp, par, &ndm);
	ineig = 1;
      }
      fb[-1 + jb] = rr[iap->run->homcont.homoclinic_constants.nstab];
      ++jb;
    }
    /*        *boundary conditions for normal vector */
    if (iap->run->homcont.homoclinic_constants.itwist == 1) {
      /*           *-orthogonal to the unstable directions of A  at t=0 
       */
      homoclinic_projection(iap, bound.data(), xequib1.data(), icp, par, xpp::HomcontManifold::unstable, xpp::HomcontEndpoint::left, false, &ndm, save_basis);
      for (i = ndm - iap->run->homcont.homoclinic_constants.nunstab; i < ndm; ++i) {
	dum = 0.;
	for (k = 0; k < ndm; ++k) {
	  dum += u0[ndm + k] * bound[i + k * (iap->ndm)];
	}
	fb[-1 + jb] = dum;
	++jb;
      }
      /*           *-orthogonal to the stable directions of A  at t=1 */
      homoclinic_projection(iap, bound.data(), xequib2.data(), icp, par, xpp::HomcontManifold::stable, xpp::HomcontEndpoint::right, false, &ndm, save_basis);
      for (i = 0; i < iap->run->homcont.homoclinic_constants.nstab; ++i) {
	dum = 0.;
	for (k = 0; k < ndm; ++k) {
	  dum += u1[ndm + k] * bound[i + k * (iap->ndm)];
	}
	fb[-1 + jb] = dum;
	++jb;
      }
      return 0;
    }
  } else {
    /*     **Starting Solutions using Homotopy** */
    jb = 0;
    for (i = 1; i <= nbc; ++i) {
      fb[-1 + i] = 0.;
    }
    ineig = 0;
    ip = xpp::autox_unknown::homoclinic_start;
    if (iap->run->homcont.homoclinic_constants.iequib >= 0) {
      ip += ndm;
    } else {
      ip += ndm << 1;
    }
    kp = ip;
    /*        *Explicit boundary conditions for homoclinic orbit at t=0 */
    homoclinic_eigen(iap, 1, 2, rr.data(), ri.data(), vr.data(), xequib1.data(), icp, par, &ndm);
    ieig = 1;
    if (iap->run->homcont.homoclinic_constants.nunstab > 1) {
      dum = 0.;
      kp = ip + iap->run->homcont.homoclinic_constants.nunstab;
      jb = ndm + 1;
      for (j = 0; j < iap->run->homcont.homoclinic_constants.nunstab; ++j) {
	for (i = 0; i < ndm; ++i) {
	  fb[i] = u0[i] - xequib1[i] - par[ip + j] * vr[iap->run->homcont.homoclinic_constants.nstab + j + i * (iap->ndm)];
	}
	/* Computing 2nd power */
	dum += par[ip + j] * par[ip + j];
      }
      jb = ndm + 1;
      fb[-1 + jb] = dum - par[-1 + ip];
      ++jb;
    } else {
      kp = ip + 1;
      jb = ndm;
      for (i = 0; i < ndm; ++i) {
	fb[i] = u0[i] - xequib1[i] - par[-1 + ip] * par[ip] *
	  vr[iap->run->homcont.homoclinic_constants.nstab + i * (iap->ndm)];
      }
      jb = ndm + 1;
    }
    /*        *Projection boundary conditions for the homoclinic orbit at 
t=1 */
    if (ineig == 0) {
      homoclinic_eigen(iap, 1, 1, rr.data(), ri.data(), vt.data(), xequib2.data(), icp, par, &ndm);
      ineig = 1;
    }
    for (i = 0; i < iap->run->homcont.homoclinic_constants.nunstab; ++i) {
      k = i + iap->run->homcont.homoclinic_constants.nstab;
      dum = 0.;
      for (j = 0; j < ndm; ++j) {
	dum += (u1[j] - xequib2[j]) * vt[k + j * (iap->ndm)];
      }
      ++kp;
      fb[-1 + jb] = dum - par[-1 + kp];
      ++jb;
    }
    /*        *NDM initial conditions for the equilibrium if IEQUIB=1,2,-2
 */
    if (iap->run->homcont.homoclinic_constants.iequib != 0 && iap->run->homcont.homoclinic_constants.iequib != -1) {
      user_right_hand_side(*iap->run, ndm, xequib1.data(), icp, iap->nfpr, par, 0, f.data(), &dum1, &dum2);
      for (i = 0; i < ndm; ++i) {
	fb[-1 + jb] = f[i];
	++jb;
      }
      /*        *NDM extra initial conditions for the equilibrium if IEQ
		UIB=-2 */
      if (iap->run->homcont.homoclinic_constants.iequib == -2) {
	user_right_hand_side(*iap->run, ndm, xequib2.data(), icp, iap->nfpr, par, 0, f.data(), &dum1, &dum2)
	  ;
	for (i = 0; i < ndm; ++i) {
	  fb[-1 + jb] = f[i];
	  ++jb;
	}
      }
    }
  }

  /*      write(9,*) NBCN,NBC */
  /* *user defined extra boundary conditions */
  if (iap->run->homcont.boundary_condition_count.nbcn > 0) {
    user_boundary_conditions(*iap->run, ndim, par, icp, iap->run->homcont.boundary_condition_count.nbcn, u0, u1, ijc, fj.data(), dbc);
    for (k = 0; k < iap->run->homcont.boundary_condition_count.nbcn; ++k) {
      fb[-1 + jb] = fj[k];
      /*            write(9,*),fb(jb),par(30) */
      ++jb;
    }
  }

  return 0;
} /* fbho_ */

/* icho: AUTO's integral conditions of the problem kind homoclinic (homcont.f90: HomCont's homoclinic problem (IPS 9)) */
int 
homoclinic_integral_conditions(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nint, const doublereal *u, const doublereal *uold, const doublereal *udot, const doublereal *upold, doublereal *f, integer ijac, doublereal *dint)
{
  homoclinic_integral_function(iap, rap, ndim, par, icp, nint, iap->nnt0, u, uold, udot, upold, f, dint);
  if (ijac == 0) return 0;
  /* The phase conditions are linear: their derivatives are exact (AUTO-07p ICHO). */
  const integer ndm = iap->ndm;
  for (integer entry = 0; entry < nint * (ndim + iap->run->parameter_count); ++entry) dint[entry] = 0.;
  integer row = 0;
  if (iap->run->homcont.homoclinic_constants.nrev == 0) {
    for (integer i = 0; i < ndm; ++i) dint[row + i*nint] = upold[i];
    ++row;
  }
  if (iap->run->homcont.homoclinic_constants.itwist == 1) {
    for (integer i = 0; i < ndm; ++i) dint[row + (ndm + i)*nint] = uold[ndm + i];
  }
  return 0;
} /* icho_ */

/* fiho: AUTO's integral function evaluation of the problem kind homoclinic (homcont.f90: HomCont's homoclinic problem (IPS 9)) */
int 
homoclinic_integral_function(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nint, integer nnt0, const doublereal *u, const doublereal *uold, const doublereal *udot, const doublereal *upold, doublereal *fi, doublereal *dint)
{

  integer ijac = 0;

  integer i, jb;
  integer ndm;
  doublereal dum;

  std::vector<doublereal> fj(iap->ndim);
  /* Generates the integral conditions for homoclinic orbits. */

  ndm = iap->ndm;
  jb = 0;

/* Integral phase condition for homoclinic orbit */

  if (iap->run->homcont.homoclinic_constants.nrev == 0) {
    dum = 0.;
    for (i = 0; i < ndm; ++i) {
      dum += upold[i] * (u[i] - uold[i]);
    }
    ++jb;
    fi[-1 + jb] = dum;
  }

  /* Integral phase condition for adjoint equation */

  if (iap->run->homcont.homoclinic_constants.itwist == 1) {
    dum = 0.;
    for (i = 0; i < ndm; ++i) {
      dum += uold[ndm + i] * (u[ndm + i] - uold[ndm + i]);
    }
    ++jb;
    fi[1] = dum;
  }

  /* User-defined integral constraints */

  if (jb < nint) {
    user_integral_conditions(ndm, par, icp, nint, u, uold, udot, 
	 upold, ijac, fj.data(), dint);
    for (i = 0; i < nint - jb; ++i) {
      fi[i + jb] = fj[i];
    }
  }
  return 0;
} /* fiho_ */

/* inho: XPPAUT 8.0 autlib5.c:833: reads HomCont's constants from fort.11, sets IAP's re-defined constants */
int 
homoclinic_read_constants(iap_type *iap, integer *icp, doublereal *par)
{

  integer ndim, nint, nuzr, i, nfree, icorr, nbc, ndm, isw;

  /* Allocate memory for global structures. */

  iap->run->homcont.homoclinic_indices.ipsi.assign(iap->run->parameter_count, 0);
  iap->run->homcont.homoclinic_indices.ifixed.assign(iap->run->parameter_count, 0);
  iap->run->homcont.homoclinic_indices.irev.assign((iap->ndim), 0);

  iap->run->homcont.homoclinic_eigen_check.ieigc.assign(2, 0);

  iap->run->homcont.beyn_workspace.iflag.assign(4, 0);

  /* the prjctn_ function uses this array to test if this is
       the first time the prjctn_ function has been called.
       Accordingly, I initialize it to zero here after I
       have created it. */
  for(i=0;i<4;i++)
    iap->run->homcont.beyn_workspace.iflag[i]=0;

/* Reads from fort.11 specific constants for homoclinic continuation. */
/* Sets up re-defined constants in IAP. */
/* Sets other constants in the following common blocks. */

/* set various constants */

  ndim = iap->ndim;
  isw = iap->isw;
  nbc = iap->nbc;
  nint = iap->nint;
  nuzr = iap->nuzr;
  ndm = ndim;
  iap->run->homcont.homoclinic_zero_threshold.compzero = autox_hmachho;
  iap->run->homcont.homoclinic_constants.nunstab=iap->run->params.nunstab;
  iap->run->homcont.homoclinic_constants.nstab=iap->run->params.nstab;
  iap->run->homcont.homoclinic_constants.iequib=iap->run->params.iequib;
  iap->run->homcont.homoclinic_constants.itwist=0;
  iap->run->homcont.homoclinic_constants.istart=2;
  iap->run->homcont.homoclinic_constants.nrev=0;
  iap->run->homcont.homoclinic_constants.nfixed=0;
  iap->run->homcont.homoclinic_constants.npsi=0;
  /* updated reading in of constants for reversible equations */
  /* replaces location in datafile of compzero */

  ndim = ndm * (iap->run->homcont.homoclinic_constants.itwist + 1);
  /* Allocate memory for global structures.  We didn't know the
     size for these until ndim was computed. */

  iap->run->homcont.homoclinic_endpoints.pu0.assign((ndim), 0.);
  iap->run->homcont.homoclinic_endpoints.pu1.assign((ndim), 0.);

  iap->run->homcont.homoclinic_eigen_workspace.rr.assign((ndim), 0.);
  iap->run->homcont.homoclinic_eigen_workspace.ri.assign((ndim), 0.);
  iap->run->homcont.homoclinic_eigen_workspace.v.assign(xpp::size_product(ndim, ndim), 0.);
  iap->run->homcont.homoclinic_eigen_workspace.vt.assign(xpp::size_product(ndim, ndim), 0.);
  iap->run->homcont.homoclinic_eigen_workspace.xequib.assign((ndim), 0.);

  iap->run->homcont.homoclinic_eigen_check.vrprev.assign(xpp::size_product(2, ndim, ndim), 0.);

  iap->run->homcont.beyn_workspace.cprev.assign(xpp::size_product(2, 2, ndim, ndim), 0.);

  nfree = iap->run->homcont.homoclinic_constants.nfixed + 2 - iap->run->homcont.homoclinic_constants.nrev + nint + nbc;
  iap->run->homcont.boundary_condition_count.nbcn = nbc;

/* Free parameter (artificial parameter for psi) */
/* nondegeneracy parameter of the adjoint */

  if (iap->run->homcont.homoclinic_constants.itwist == 1) {
    ++nfree;
    icp[-1 + nfree] = xpp::autox_unknown::homoclinic_adjoint;
    par[xpp::autox_unknown::homoclinic_adjoint] = 0.;
  }

  /* Extra free parameters for equilibrium if iequib=1,2,-2 */

  if (iap->run->homcont.homoclinic_constants.iequib != 0 && iap->run->homcont.homoclinic_constants.iequib != -1) {
    for (i = 0; i < ndm; ++i) {
      icp[nfree + i] = i + xpp::autox_unknown::homoclinic_equilibrium_start;
    }
  }

  if (iap->run->homcont.homoclinic_constants.iequib == -2) {
    for (i = 0; i < ndm; ++i) {
      icp[nfree + ndm + i] = ndm + xpp::autox_unknown::homoclinic_equilibrium_start + i;
    }
  }

  if (iap->run->homcont.homoclinic_constants.istart != 3) {
    /*     *regular continuation */

    nint = nint + iap->run->homcont.homoclinic_constants.itwist + 1 - iap->run->homcont.homoclinic_constants.nrev;

    if (isw == 2) {
      icorr = 2;
    } else {
      icorr = 1;
    }
    nbc = iap->run->homcont.homoclinic_constants.nstab + iap->run->homcont.homoclinic_constants.nunstab + (iap->run->homcont.homoclinic_constants.itwist + iap->run->homcont.homoclinic_constants.iequib) * ndm + nfree - nint - icorr;
    if (iap->run->homcont.homoclinic_constants.iequib == 2) {
      nbc = nbc - ndm + 1;
    }
    if (iap->run->homcont.homoclinic_constants.iequib < 0) {
      nbc -= (iap->run->homcont.homoclinic_constants.iequib * 3 + 2) * ndm;
    }
  } else {
    /*     *starting solutions using homotopy */
    if (iap->run->homcont.homoclinic_constants.nunstab == 1) {
      nbc = ndm * (iap->run->homcont.homoclinic_constants.iequib + 1) + 1;
    } else {
      nbc = ndm * (iap->run->homcont.homoclinic_constants.iequib + 1) + iap->run->homcont.homoclinic_constants.nunstab + 1;
    }
    if (iap->run->homcont.homoclinic_constants.iequib == 2) {
      xpp::autox_fail("autoX: HomCont's IEQUIB 2 (a generic heteroclinic orbit) is not allowed with ISTART 3 (a start by homotopy)");
    }
    if (iap->run->homcont.homoclinic_constants.iequib < 0) {
      nbc -= ndm * (iap->run->homcont.homoclinic_constants.iequib * 3 + 2);
    }
    nint = 0;
  }

  /* write new constants into IAP */

  iap->ndim = ndim;
  iap->nbc = nbc;
  iap->nint = nint;
  iap->nuzr = nuzr;
  iap->ndm = ndm;

  return 0;
} /* inho_ */

/* preho: AUTO homcont.f90:1320: special homoclinic orbit preprocessing */
int 
homoclinic_preprocess(AutoxRun &run, integer *ndx, integer *ntsr, integer *nar, integer *ndim, integer *ncolrs, doublereal *ups, doublereal *udotps, doublereal *tm, doublereal *par)
{
  integer ups_dim1, udotps_dim1;

  integer jmin;
  doublereal upsi;
  integer i, j, k;
  doublereal tmmin;
  integer k1, k2, ii;
  doublereal upsmin;
  integer ist;

/* Preprocesses (perturbs) restart data to enable */
/* initial computation of the adjoint variable */

  udotps_dim1 = *ndx;
  ups_dim1 = *ndx;
    
  if (*nar < *ndim) {
    for (j = 0; j < *ntsr; ++j) {
      for (i = 0; i < *ncolrs; ++i) {
	k1 = i * *ndim;
	k2 = (i + 1) * *ndim - 1;
	for (k = k1 + *nar; k <= k2; ++k) {
	  ARRAY2D(ups, j, k) = .1;
	}
      }
    }
    for (k = *nar; k < *ndim; ++k) {
      ARRAY2D(ups, *ntsr, k) = .1;
    }
  }

  /* Shift phase if necessary if continu(e)ing from */
  /* a periodic orbit into a homoclinic one */

  if (run.homcont.homoclinic_constants.istart == 1) {

    /* First find smallest value in norm */

    upsmin = autox_homoclinic_norm_start;
    jmin = 1;
    for (j = 0; j < *ntsr + 1; ++j) {
      upsi = 0.;
      for (i = 0; i < *nar; ++i) {
	upsi += (ARRAY2D(ups, j, i) - par[i + xpp::autox_unknown::homoclinic_equilibrium_start]) * (ARRAY2D(ups, j, i) - par[i + xpp::autox_unknown::homoclinic_equilibrium_start]);
      }
      if (upsi <= upsmin) {
	upsmin = upsi;
	jmin = j + 1;
      }
    }
    tmmin = tm[-1 + jmin];

    /* And then do the actual shift */

    if (jmin != 1) {
      ist = 0;
      j = *ntsr + 1;
      for (ii = 0; ii < *ntsr; ++ii) {
	if (j == *ntsr + 1) {
	  ++ist;
	  tm[-1 + j] = tm[-1 + ist];
	  for (k = 0; k < *ncolrs * *ndim; ++k) {
	    ARRAY2D(ups, (j - 1), k) = ARRAY2D(ups, (ist - 1), k);
	    ARRAY2D(udotps, (j - 1), k) = ARRAY2D(udotps, (ist - 1), k);
	  }
	  j = ist;
	}
	i = j;
	j = j + jmin - 1;
	if (j > *ntsr) {
	  j -= *ntsr;
	}
	if (j == ist) {
	  j = *ntsr + 1;
	}
	tm[-1 + i] = tm[-1 + j] - tmmin;
	if (tm[-1 + i] < 0.) {
	  tm[-1 + i] += 1.;
	}
	for (k = 0; k < *ncolrs * *ndim; ++k) {
	  ARRAY2D(ups, (i - 1), k) = ARRAY2D(ups, (j - 1), k);
	  ARRAY2D(udotps, (i - 1), k) = ARRAY2D(udotps, (j - 1), k);
	}
      }

      /* Last equal to first */

      tm[*ntsr] = 1.;
      for (k = 0; k < *ncolrs * *ndim; ++k) {
	ARRAY2D(ups, *ntsr, k) = ARRAY2D(ups, 0, k);
	ARRAY2D(udotps, *ntsr, k) = ARRAY2D(udotps, 0, k);
      }

    }
  }

  return 0;
} /* preho_ */

/* stpnho: AUTO's starting data of the problem kind homoclinic (homcont.f90: HomCont's homoclinic problem (IPS 9)) */
int 
homoclinic_starting_data(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *ntsr, integer *ncolrs, doublereal *rlcur, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *upoldp, doublereal *tm, doublereal *dtm, integer *nodir, doublereal *thl, doublereal *thu)
{
  return autox_uniform_start(iap, rap, par, icp, ntsr, ncolrs, rlcur, *ndxloc, ups, tm, nodir,
      [&](doublereal *state, doublereal &time) { homoclinic_start_point(iap, icp, state, par, &time); });
} /* stpnho_ */

/* stpho: XPPAUT 8.0 autlib5.c:1191: a starting point for homoclinic continuation (ISTART 2 calls STPNHO, 3 sets up the homotopy) */
int 
homoclinic_start_point(iap_type *iap, integer *icp, doublereal *u, doublereal *par, doublereal *t)
{

  integer i, j;

  integer ip;
  integer kp;
  integer ndm;

  std::vector<doublereal> ri(iap->ndm);
  std::vector<doublereal> rr(iap->ndm);
  std::vector<doublereal> vr(xpp::size_product(iap->ndm, iap->ndm));
  std::vector<doublereal> vt(xpp::size_product(iap->ndm, iap->ndm));
  std::vector<doublereal> xequib(iap->ndm);

  /* Generates a starting point for homoclinic continuation */
  /* If ISTART=2 it calls STPNHO. */
  /* If ISTART=3 it sets up the homotopy method. */

  ndm = iap->ndm;

  /* Initialize parameters */

  user_start(*iap->run, ndm, *t, u, par);

  /* Initialize solution and additional parameters */

  switch (static_cast<int>(iap->run->homcont.homoclinic_constants.istart)) {
  case 1:  goto L1;
  case 2:  goto L2;
  case 3:  goto L3;
  }

  /* -----------------------------------------------------------------------
 */
 L1:
  /* Obsolete option */

  return 0;

/* -----------------------------------------------------------------------
 */
 L2:
  /*     *Regular continuation (explicit solution in STHO) */

  return 0;

/* -----------------------------------------------------------------------
 */
 L3:
  /*     *Starting solutions using homotopy */

  user_output_parameters(ndm, u, par);
  for (i = 0; i < ndm; ++i) {
    xequib[i] = par[i + xpp::autox_unknown::homoclinic_equilibrium_start];
  }
  homoclinic_eigen(iap, 1, 1, rr.data(), ri.data(), vt.data(), xequib.data(), icp, par, &ndm);
  homoclinic_eigen(iap, 1, 2, rr.data(), ri.data(), vr.data(), xequib.data(), icp, par, &ndm);

  /* Set up artificial parameters at the left-hand end point of orbit */

  ip = xpp::autox_unknown::homoclinic_start;
  if (iap->run->homcont.homoclinic_constants.iequib >= 0) {
    ip += ndm;
  } else {
    ip += ndm * 2;
  }
  kp = ip;

/* Parameters xi 1=1, xi i=0, i=2,NSTAB */

  par[ip] = 1.;
  if (iap->run->homcont.homoclinic_constants.nunstab > 1) {
    for (i = 1; i < iap->run->homcont.homoclinic_constants.nunstab; ++i) {
      par[ip + i] = 0.;
    }
  }
  ip += iap->run->homcont.homoclinic_constants.nunstab;

/*Starting guess for homoclinic orbit in real principal unstable direction
*/

  for (i = 0; i < ndm; ++i) {
    u[i] = xequib[i] + vr[iap->run->homcont.homoclinic_constants.nstab + i * (iap->ndm)]
      * par[-1 + kp] * par[kp] * xpp::math::exp(rr[iap->run->homcont.homoclinic_constants.nstab] * *t * par[xpp::autox_unknown::period]);
  }
  for (i = 0; i < ndm; ++i) {
    xpp::print(iap->run->fp9,"stpho {:20.10f}\n",u[i]);	
  }
  xpp::print(iap->run->fp9,"\n");	

/* Artificial parameters at the right-hand end point of the orbit */
/* omega_i=<x(1)-x_o,w_i^*> */

  for (i = 0; i < iap->run->homcont.homoclinic_constants.nunstab; ++i) {
    par[ip + i] = 0.;
    for (j = 0; j < ndm; ++j) {
      par[ip + i] += vr[iap->run->homcont.homoclinic_constants.nstab + j * (iap->ndm)] * par[-1 + kp] * par[kp] * xpp::math::exp(rr[iap->run->homcont.homoclinic_constants.nstab] * par[xpp::autox_unknown::period]) *
	vt[iap->run->homcont.homoclinic_constants.nstab + i + j * (iap->ndm)];
    }
  }
  ip += iap->run->homcont.homoclinic_constants.nunstab;
  return 0;
  /* -----------------------------------------------------------------------
   */
} /* stpho_ */

/* pvlsho: HomCont's PVLS: eigenvalues and degeneracy parameters (autlib5.c:1331) */
int 
homoclinic_output_parameters(iap_type *iap, rap_type *rap, integer *icp, doublereal *dtm, integer *ndxloc, doublereal *ups, integer *ndim, doublereal *p0, doublereal *p1, doublereal *par)
{

  integer i, j;

  doublereal orient;

  integer iid, ndm;

  iid = iap->iid;
  ndm = iap->ndm;

  output_parameters_boundary(iap, rap, icp, dtm, ndxloc, ups, ndim, 
	 p0, p1, par);

  /*      *Compute eigenvalues */
  iap->run->homcont.homoclinic_eigen_workspace.ineig = 0;
  for (i = 0; i < ndm; ++i) {
    iap->run->homcont.homoclinic_eigen_workspace.xequib[i] = par[i + xpp::autox_unknown::homoclinic_equilibrium_start];
  }
  homoclinic_eigen(iap, 1, 2, iap->run->homcont.homoclinic_eigen_workspace.rr.data(), iap->run->homcont.homoclinic_eigen_workspace.ri.data(), iap->run->homcont.homoclinic_eigen_workspace.v.data(), iap->run->homcont.homoclinic_eigen_workspace.xequib.data(), 
	icp, par, &ndm);
  if (iid >= 3) {
    xpp::print(iap->run->fp9,"EIGENVALUES\n");	
    for (j = 0; j < ndm; ++j) {
      xpp::print(iap->run->fp9," ({:12.7f} {:12.7f})\n",iap->run->homcont.homoclinic_eigen_workspace.rr[j],iap->run->homcont.homoclinic_eigen_workspace.ri[j]);	
    }
  }
  if (iap->run->homcont.homoclinic_constants.itwist == 1) {
    homoclinic_eigen(iap, 1, 1, iap->run->homcont.homoclinic_eigen_workspace.rr.data(), iap->run->homcont.homoclinic_eigen_workspace.ri.data(), iap->run->homcont.homoclinic_eigen_workspace.vt.data(), 
	  iap->run->homcont.homoclinic_eigen_workspace.xequib.data(), icp, par, &ndm);
    iap->run->homcont.homoclinic_eigen_workspace.ineig = 1;
    orient = homoclinic_degeneracy_condition(iap, 0, iap->run->homcont.homoclinic_eigen_workspace.rr.data(), iap->run->homcont.homoclinic_eigen_workspace.ri.data(), iap->run->homcont.homoclinic_eigen_workspace.v.data(), 
		   iap->run->homcont.homoclinic_eigen_workspace.vt.data(), icp, par);
    if (iid >= 3) {
      if (orient < 0.) {
	xpp::print(iap->run->fp9," Non-orientable, ({:20.10f})\n",orient);	
      } else {
	xpp::print(iap->run->fp9," Orientable ({:20.10f})\n",orient);	
      }
    }
  }

  for (i = 0; i < iap->run->homcont.homoclinic_constants.npsi; ++i) {
    if (iap->run->homcont.homoclinic_indices.ipsi[i] > 10 && iap->run->homcont.homoclinic_eigen_workspace.ineig == 0) {
      homoclinic_eigen(iap, 1, 1, iap->run->homcont.homoclinic_eigen_workspace.rr.data(), iap->run->homcont.homoclinic_eigen_workspace.ri.data(), iap->run->homcont.homoclinic_eigen_workspace.vt.data(), 
	    iap->run->homcont.homoclinic_eigen_workspace.xequib.data(), icp, par, &ndm);
      iap->run->homcont.homoclinic_eigen_workspace.ineig = 1;
    }
    par[iap->run->homcont.homoclinic_indices.ipsi[i] + xpp::autox_unknown::test_function_start - 1] = homoclinic_degeneracy_condition(iap, iap->run->homcont.homoclinic_indices.ipsi[i], iap->run->homcont.homoclinic_eigen_workspace.rr.data(), iap->run->homcont.homoclinic_eigen_workspace.ri.data(), iap->run->homcont.homoclinic_eigen_workspace.v.data(), iap->run->homcont.homoclinic_eigen_workspace.vt.data(), icp, par);
    if (iid >= 3) {
      xpp::print(iap->run->fp9," PSI({:2})={:20.10f}\n",iap->run->homcont.homoclinic_indices.ipsi[i],par[iap->run->homcont.homoclinic_indices.ipsi[i] + xpp::autox_unknown::test_function_start - 1]);

    }
  }

  return 0;

} /* pvlsho_ */

/* psiho: AUTO homcont.f90:1795: the degenerate homoclinic conditions PSI(IS)=0 */
doublereal 
homoclinic_degeneracy_condition(const iap_type *iap, integer is, doublereal *rr, doublereal *ri, doublereal *v, doublereal *vt, const integer *icp, doublereal *par)
{
  doublereal ret_val;

  integer i, j;
  doublereal droot, s1, s2, f0norm, f1norm, u0norm, u1norm;
  integer ndm;
  doublereal dum1, dum2;

  std::vector<doublereal> f0(iap->ndm);
  std::vector<doublereal> f1(iap->ndm);

/* The conditions for degenerate homoclinic orbits are given by PSI(IS)=0.
 */

/* RR and RI contain the real and imaginary parts of eigenvalues which are
 */
/* ordered with respect to their real parts (smallest first). */
/* The (generalised) real eigenvectors are stored as the ROWS of V. */
/* The (generalised) real left eigenvectors are in the ROWS of VT. */
/* In the block ENDPTS are stored the co-ordinates of the left (PU0) */
/*and right (PU1) endpoints of the solution (+  vector if that is computed
)*/

  vt -= ((iap->ndm)+1);
  v -= ((iap->ndm)+1);

  ndm = iap->ndm;

  user_right_hand_side(*iap->run, ndm, iap->run->homcont.homoclinic_endpoints.pu0.data(), icp, iap->nfpr, par, 0, f0.data(), &dum1, &dum2);
  user_right_hand_side(*iap->run, ndm, iap->run->homcont.homoclinic_endpoints.pu1.data(), icp, iap->nfpr, par, 0, f1.data(), &dum1, &dum2);

  ret_val = 0.;

/*  Compute orientation */

  if (is == 0) {
    s1 = 0.;
    s2 = 0.;
    f0norm = 0.;
    f1norm = 0.;
    u0norm = 0.;
    u1norm = 0.;
    for (j = 0; j < ndm; ++j) {
      s1 += f1[j] * iap->run->homcont.homoclinic_endpoints.pu0[ndm + j];
      s2 += f0[j] * iap->run->homcont.homoclinic_endpoints.pu1[ndm + j];
      /* Computing 2nd power */
      f0norm += f0[j] * f0[j];
      /* Computing 2nd power */
      f1norm += f1[j] * f1[j];
      /* Computing 2nd power */
      u0norm += iap->run->homcont.homoclinic_endpoints.pu0[j + ndm] * iap->run->homcont.homoclinic_endpoints.pu0[j + ndm];
      /* Computing 2nd power */
      u1norm += iap->run->homcont.homoclinic_endpoints.pu1[j + ndm] * iap->run->homcont.homoclinic_endpoints.pu1[j + ndm];
    }
    droot = sqrt(f0norm * f1norm * u0norm * u1norm);
    if (droot != 0.) {
      ret_val = -s1 * s2 / droot;
    } else {
      ret_val = 0.;
    }
    return ret_val;
  } else if (is == 11) {
    /* L11 below still reads f1[]; free it there instead. */
  } else if (is == 12) {
    /* L12 below still reads f0[]; free it there instead. */
  } else {
  }

  switch (static_cast<int>(is)) {
  case 1:  goto L1;
  case 2:  goto L2;
  case 3:  goto L3;
  case 4:  goto L4;
  case 5:  goto L5;
  case 6:  goto L6;
  case 7:  goto L7;
  case 8:  goto L8;
  case 9:  goto L9;
  case 10:  goto L10;
  case 11:  goto L11;
  case 12:  goto L12;
  case 13:  goto L13;
  case 14:  goto L14;
  case 15:  goto L15;
  case 16:  goto L16;
  }

  /* Resonant eigenvalues (neutral saddle) */

 L1:
  ret_val = rr[-1 + iap->run->homcont.homoclinic_constants.nstab] + rr[iap->run->homcont.homoclinic_constants.nstab] + ri[-1 + iap->run->homcont.homoclinic_constants.nstab] + 
    ri[iap->run->homcont.homoclinic_constants.nstab];
  return ret_val;

/* Double real leading eigenvalues (stable) */
/*   (saddle, saddle-focus transition) */

 L2:
  if (f2c::abs(ri[-1 + iap->run->homcont.homoclinic_constants.nstab]) > iap->run->homcont.homoclinic_zero_threshold.compzero) {
    /* Computing 2nd power */
    doublereal tmp= ri[-1 + iap->run->homcont.homoclinic_constants.nstab] - ri[-1 + iap->run->homcont.homoclinic_constants.nstab - 1];
    ret_val = -(tmp * tmp);
  } else {
    /* Computing 2nd power */
    doublereal tmp = rr[-1 + iap->run->homcont.homoclinic_constants.nstab] - rr[-1 + iap->run->homcont.homoclinic_constants.nstab - 1];
    ret_val = tmp * tmp;
  }
  return ret_val;

/* Double real positive eigenvalues (unstable) */
/*   (saddle, saddle-focus transition) */

 L3:
  if (f2c::abs(ri[iap->run->homcont.homoclinic_constants.nstab]) > iap->run->homcont.homoclinic_zero_threshold.compzero) {
    /* Computing 2nd power */
    doublereal tmp = ri[iap->run->homcont.homoclinic_constants.nstab] - ri[iap->run->homcont.homoclinic_constants.nstab + 1];
    ret_val = -(tmp * tmp);
  } else {
    /* Computing 2nd power */
    doublereal tmp = rr[iap->run->homcont.homoclinic_constants.nstab] - rr[iap->run->homcont.homoclinic_constants.nstab + 1];
    ret_val = tmp * tmp;
  }
  return ret_val;

/* Neutral saddle, saddle-focus or bi-focus (includes 1, above, also) */

 L4:
  ret_val = rr[-1 + iap->run->homcont.homoclinic_constants.nstab] + rr[iap->run->homcont.homoclinic_constants.nstab];
  return ret_val;

  /* Neutrally-divergent saddle-focus (stable eigenvalues complex) */

 L5:
  ret_val = rr[-1 + iap->run->homcont.homoclinic_constants.nstab] + rr[iap->run->homcont.homoclinic_constants.nstab] + rr[iap->run->homcont.homoclinic_constants.nstab - 2];
  return ret_val;

/* Neutrally-divergent saddle-focus (unstable eigenvalues complex) */

 L6:
  ret_val = rr[-1 + iap->run->homcont.homoclinic_constants.nstab] + rr[iap->run->homcont.homoclinic_constants.nstab] + rr[iap->run->homcont.homoclinic_constants.nstab + 1];
  return ret_val;

/* Three leading eigenvalues (stable) */

 L7:
  ret_val = rr[-1 + iap->run->homcont.homoclinic_constants.nstab] - rr[iap->run->homcont.homoclinic_constants.nstab - 3];
  return ret_val;

  /* Three leading eigenvalues (ustable) */

 L8:
  ret_val = rr[iap->run->homcont.homoclinic_constants.nstab] - rr[iap->run->homcont.homoclinic_constants.nunstab + 2];
  return ret_val;

  /* Local bifurcation (zero eigenvalue or Hopf): NSTAB decreases */
  /*  (nb. the problem becomes ill-posed after a zero of 9 or 10) */

 L9:
  ret_val = rr[-1 + iap->run->homcont.homoclinic_constants.nstab];
  return ret_val;

/* Local bifurcation (zero eigenvalue or Hopf): NSTAB increases */

 L10:
  ret_val = rr[iap->run->homcont.homoclinic_constants.nstab];
  return ret_val;

  /* Orbit flip (with respect to leading stable direction) */
  /*     e.g. 1D unstable manifold */

 L11:
  for (j = 0; j < ndm; ++j) {
    ret_val += f1[j] * vt[iap->run->homcont.homoclinic_constants.nstab + (j + 1) * (iap->ndm)];
  }
  ret_val *= xpp::math::exp(-par[xpp::autox_unknown::period] * rr[-1 + iap->run->homcont.homoclinic_constants.nstab] / 2.);
  return ret_val;

  /* Orbit flip (with respect to leading unstable direction) */
  /*     e.g. 1D stable manifold */

 L12:
  for (j = 0; j < ndm; ++j) {
    ret_val += f0[j] * vt[iap->run->homcont.homoclinic_constants.nstab + 1 + (j + 1) * (iap->ndm)];
  }
  ret_val *= xpp::math::exp(par[xpp::autox_unknown::period] * rr[iap->run->homcont.homoclinic_constants.nstab] / 2.);
  return ret_val;

  /* Inclination flip (critically twisted) with respect to stable manifold 
*/
/*   e.g. 1D unstable manifold */

 L13:
  for (i = 0; i < ndm; ++i) {
    ret_val += iap->run->homcont.homoclinic_endpoints.pu0[ndm + i] * v[iap->run->homcont.homoclinic_constants.nstab + (i + 1) * (iap->ndm)]
      ;
  }
  ret_val *= xpp::math::exp(-par[xpp::autox_unknown::period] * rr[-1 + iap->run->homcont.homoclinic_constants.nstab] / 2.);
  return ret_val;

  /* Inclination flip (critically twisted) with respect to unstable manifold
 */
/*   e.g. 1D stable manifold */

 L14:
  for (i = 0; i < ndm; ++i) {
    ret_val += iap->run->homcont.homoclinic_endpoints.pu1[ndm + i] * v[iap->run->homcont.homoclinic_constants.nstab + 1 + (i + 1) * (iap->ndm)];
  }
  ret_val *= xpp::math::exp(par[xpp::autox_unknown::period] * rr[iap->run->homcont.homoclinic_constants.nstab] / 2.);
  return ret_val;

  /* Non-central homoclinic to saddle-node (in stable manifold) */

 L15:
  for (i = 0; i < ndm; ++i) {
    ret_val += (par[i + xpp::autox_unknown::homoclinic_equilibrium_start] - iap->run->homcont.homoclinic_endpoints.pu1[i]) * v[iap->run->homcont.homoclinic_constants.nstab + 1 + (i + 1) * (iap->ndm)];
  }
  return ret_val;

/* Non-central homoclinic to saddle-node (in unstable manifold) */

 L16:
  for (i = 0; i < ndm; ++i) {
    ret_val += (par[i + xpp::autox_unknown::homoclinic_equilibrium_start] - iap->run->homcont.homoclinic_endpoints.pu0[i]) * v[iap->run->homcont.homoclinic_constants.nstab + 1 + (i + 1) * (iap->ndm)];
  }
  return ret_val;

} /* psiho_ */

/* eighi: interface to eigho with its workspace (autlib5.c:1655) */
int 
homoclinic_eigen(const iap_type *iap, integer isign, integer itrans, doublereal *rr, doublereal *ri, doublereal *vret, doublereal *xequib, const integer *icp, doublereal *par, integer *ndm)
{

  std::vector<doublereal> dfdp(xpp::size_product(*ndm, iap->run->parameter_count));
  std::vector<doublereal> dfdu(xpp::size_product(*ndm, *ndm));
  std::vector<doublereal> zz(xpp::size_product(*ndm, *ndm));

  xpp::homoclinic_eigen_ordered(iap, &isign, &itrans, rr, ri, vret, xequib, icp, par, ndm, dfdu.data(), dfdp.data(), zz.data());

  return 0;
} /* homoclinic_eigen */

/* prjcti: interface to prjctn with its workspace (autlib5.c:443) */
int 
homoclinic_projection(const iap_type *iap, doublereal *bound, doublereal *xequib, const integer *icp, doublereal *par, xpp::HomcontManifold manifold, xpp::HomcontEndpoint endpoint, bool transpose, integer *ndm, bool save_basis)
{
  
  std::vector<doublereal> dfdp(xpp::size_product(*ndm, iap->run->parameter_count));
  std::vector<doublereal> dfdu(xpp::size_product(*ndm, *ndm));
  
  xpp::homoclinic_projection_conditions(iap, bound, xequib, icp, par, manifold, endpoint, transpose, *ndm, dfdu.data(), dfdp.data(), save_basis);
  
  return 0;
} /* homoclinic_projection */
