/* autlib5.f -- translated by f2c (version 19970805).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include <vector>
#include "xpp_io.h"
#include "session.h"
#include "autevd.h" /* xAuto (its own extern) */
#include "auto_jacobian.h"
#include "xpp_math.h"

/* The memory for these are taken care of in main, and setubv for the
   mpi parallel case.  These are global since the they are used many times
   in the wrapper functions in autlib3.c (and autlib5.c) and the cost
   of allocating and deallocating them is prohibitive. */
/* global_scratch: auto_c.h */

/* All of these global structures correspond to common
   blocks in the original code.  They are ONLY used within
   the Homcont code.
*/

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*        Subroutines for Homoclinic Bifurcation Analysis */
/*       (A. R. Champneys, Yu. A. Kuznetsov, B. Sandstede) */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/*     ---------- ---- */
/* Subroutine */ int 
fnho(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp)
{
  xpp::Session &s=*iap->lib->session;
  /* System generated locals */
  integer dfdu_dim1, dfdp_dim1;

  /* Local variables */

  integer nfpr;
  integer i, j;
  doublereal ep;
  integer ndm;

/* Generates the equations for homoclinic bifurcation analysis */

/* Local */

    /* Parameter adjustments */
    /*--u;*/
    /*--icp;*/
    /*--par;*/
    /*--f;*/
  dfdp_dim1 = ndim;
  dfdu_dim1 = ndim;
    
  ndm = iap->ndm;
  nfpr = iap->nfpr;

/* Generate the function. */

  ffho(iap, rap, ndim, u, uold, icp, par, f, ndm, 
       s.auto_lib.scratch.dfu, s.auto_lib.scratch.dfp);

  if (ijac == 0) {
    return 0;
  }

  /* Generate the Jacobian. */

  ep = xpp::auto_jacobian::central(
      ndim, ndim, u, s.auto_lib.scratch.uu1, s.auto_lib.scratch.uu2, s.auto_lib.scratch.ff1, s.auto_lib.scratch.ff2,
      [&](const doublereal *uu, doublereal *ff) { ffho(iap, rap, ndim, uu, uold, icp, par, ff, ndm, s.auto_lib.scratch.dfu, s.auto_lib.scratch.dfp); },
      dfdu, dfdu_dim1);

  for (i = 0; i < nfpr; ++i) {
    par[icp[i]] += ep;
    ffho(iap, rap, ndim, u, uold, icp, par, s.auto_lib.scratch.ff1, 
	 ndm, s.auto_lib.scratch.dfu, s.auto_lib.scratch.dfp);
    for (j = 0; j < ndim; ++j) {
      ARRAY2D(dfdp, j, icp[i]) = (s.auto_lib.scratch.ff1[j] - f[j]) / ep;
    }
    par[icp[i]] -= ep;
  }

  return 0;
} /* fnho_ */

/*     ---------- ---- */
/* Subroutine */ int 
ffho(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp)
{
  /* System generated locals */
  integer dfdu_dim1;

    /* Local variables */

  integer i, j;
  doublereal dum1;

    /* Parameter adjustments */
    /*--u;*/
    /*--uold;*/
    /*--icp;*/
    /*--par;*/
    /*--f;*/
  dfdu_dim1 = ndm;
    
  ndm = iap->ndm;

  if (iap->lib->homcont.blhom.itwist == 0) {
    /*        *Evaluate the R.-H. sides */
    funi(iap, rap, ndm, u, uold, icp, par, 0,
	 f, dfdu, dfdp);
  } else {
    /*        *Adjoint variational equations for normal vector */
    funi(iap, rap, ndm, u, uold, icp, par, 1,
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
      f[ndm + j] += par[9] * f[j];
    }
  }

  /* Scale by truncation interval T=PAR(11) */

  for (i = 0; i < ndim; ++i) {
    f[i] = par[10] * f[i];
  }

  return 0;
} /* ffho_ */

/*     ---------- ---- */
/* Subroutine */ int 
bcho(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nbc, const doublereal *u0, const doublereal *u1, doublereal *f, integer ijac, doublereal *dbc)
{
  /* System generated locals */
  integer dbc_dim1;

  /* Local variables */

  integer nfpr;
  integer i, j;
  doublereal ep;
  integer nbc0;

  std::vector<doublereal> ff1(iap->nbc);
  std::vector<doublereal> ff2(iap->nbc);
  std::vector<doublereal> uu1(iap->ndim);
  std::vector<doublereal> uu2(iap->ndim);
  std::vector<doublereal> dfu((iap->nbc)*(2*iap->ndim+NPARX));

/* Generates the boundary conditions for homoclinic bifurcation analysis 
*/

/* Local */

    /* Parameter adjustments */
    /*--par;*/
    /*--icp;*/
    /*--f;*/
    /*--u0;*/
    /*--u1;*/
  dbc_dim1 = nbc;
  
  nbc0 = iap->nbc0;
  nfpr = iap->nfpr;

/* Generate the function. */

  fbho(iap, rap, ndim, par, icp, nbc, nbc0, &u0[0], &u1[0], &
       f[0], dfu.data());

  if (ijac == 0) {
    return 0;
  }

  /* Derivatives with respect to U0. */

  ep = xpp::auto_jacobian::central(
      ndim, nbc, u0, uu1.data(), uu2.data(), ff1.data(), ff2.data(),
      [&](const doublereal *uu, doublereal *ff) { fbho(iap, rap, ndim, par, icp, nbc, nbc0, uu, u1, ff, dfu.data()); },
      dbc, dbc_dim1);

  /* Derivatives with respect to U1. */

  ep = xpp::auto_jacobian::central(
      ndim, nbc, u1, uu1.data(), uu2.data(), ff1.data(), ff2.data(),
      [&](const doublereal *uu, doublereal *ff) { fbho(iap, rap, ndim, par, icp, nbc, nbc0, u0, uu, ff, dfu.data()); },
      dbc + ndim * dbc_dim1, dbc_dim1);

  for (i = 0; i < nfpr; ++i) {
    par[icp[i]] += ep;
    fbho(iap, rap, ndim, par, icp, nbc, nbc0, u0, u1
	 , ff2.data(), dfu.data());
    for (j = 0; j < nbc; ++j) {
      ARRAY2D(dbc, j, (ndim * 2) + icp[i]) = (ff2[j] - f[j]) / ep;
    }
    par[icp[i]] -= ep;
  }

  return 0;
} /* bcho_ */

/*     ---------- ---- */
/* Subroutine */ int 
fbho(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nbc, integer nbc0, const doublereal *u0, const doublereal *u1, doublereal *fb, doublereal *dbc)
{

    /* Local variables */

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
       Finally, the routines called from here prjcti and eighi
       also depend on these arrays, and more importantly the algorithm,
       having N X.  So, they all need to be changed at once.
    */
  std::vector<doublereal> f(iap->ndm);
  std::vector<doublereal> bound((iap->ndm)*(iap->ndm));
  std::vector<doublereal> fj(iap->ndm);
  std::vector<doublereal> ri(iap->ndm);
  std::vector<doublereal> rr(iap->ndm);
  std::vector<doublereal> vr((iap->ndm)*(iap->ndm));
  std::vector<doublereal> vt((iap->ndm)*(iap->ndm));
  std::vector<doublereal> xequib1(iap->ndm);
  std::vector<doublereal> xequib2(iap->ndm);

  /* Generates the boundary conditions for homoclinic orbits. */

/* Local */

    /* Parameter adjustments */
    /*--par;*/
    /*--icp;*/
    /*--u0;*/
    /*--u1;*/
    /*--fb;*/

  ndm = iap->ndm;

  /*     *Initialization */
  for (i = 1; i <= nbc; ++i) {
    fb[-1 + i] = 0.;
  }
  jb = 1;

/*     *Update pu0,pu1 */
  for (i = 0; i < ndim; ++i) {
    iap->lib->homcont.blhmu.pu0[i] = u0[i];
    iap->lib->homcont.blhmu.pu1[i] = u1[i];
  }

  if (iap->lib->homcont.blhom.iequib == 0 || iap->lib->homcont.blhom.iequib == -1) {
    pvls(ndm, u0, par);
  }
  /*              write(9,*) 'Xequib:' */
  for (i = 0; i < ndm; ++i) {
    xequib1[i] = par[i + 11];
    /*              write(9,*) I,XEQUIB1(I) */
  }
  if (iap->lib->homcont.blhom.iequib >= 0) {
    for (i = 0; i < ndm; ++i) {
      xequib2[i] = par[i + 11];
    }
  } else {
    for (i = 0; i < ndm; ++i) {
      xequib2[i] = par[ndm + 11 + i];
    }
  }

  /*     **Regular Continuation** */
  if (iap->lib->homcont.blhom.istart != 3) {
    /*        *Projection boundary conditions for the homoclinic orbit */
    /*        *NSTAB boundary conditions at t=0 */
    prjcti(iap, bound.data(), xequib1.data(), icp, par, -1, 1, 1, &ndm);
    for (i = 0; i < iap->lib->homcont.blhom.nstab; ++i) {
      for (k = 0; k < ndm; ++k) {
	fb[-1 + jb] += (u0[k] - xequib1[k]) * bound[i + k * (iap->ndm)];
      }
      /*         write(9,*) 'fb',jb,fb(jb) */
      ++jb;
    }
    /*        *NUNSTAB boundary conditions at t=1 */
    if (iap->lib->homcont.blhom.nrev == 0) {
      prjcti(iap, bound.data(), xequib2.data(), icp, par, 1, 2, 1, &
	     ndm);
      for (i = ndm - iap->lib->homcont.blhom.nunstab; i < ndm; ++i) {
	for (k = 0; k < ndm; ++k) {
	  fb[-1 + jb] += (u1[k] - xequib2[k]) * bound[i + k * (iap->ndm)];
	}
	++jb;
      }
    } else {
      /*         *NUNSTAB symmetric boundary conditions at t=1 if NREV=1
       */

      for (i = 0; i < ndim; ++i) {
	if (iap->lib->homcont.blhmp.irev[i] > 0) {
	  if (iap->lib->homcont.blhmp.irev[i] == 1) {
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
    if (iap->lib->homcont.blhom.nfixed > 0) {
      if (ieig == 0) {
	eighi(iap, 1, 2, rr.data(), ri.data(), vr.data(), xequib1.data(), icp, par, &
	      ndm);
	ieig = 1;
      }
      for (i = 0; i < iap->lib->homcont.blhom.nfixed; ++i) {
	if (iap->lib->homcont.blhmp.ifixed[i] > 10 && ineig == 0) {
	  eighi(iap, 1, 1, rr.data(), ri.data(), vt.data(), xequib1.data(), icp, par, &ndm);
	  ineig = 1;
	}
	fb[-1 + jb] = psiho(iap, iap->lib->homcont.blhmp.ifixed[i], rr.data(), ri.data(), vr.data(),vt.data(), icp, par);
	++jb;
      }
    }
    /*        *NDM initial conditions for the equilibrium if IEQUIB=1,2,-2
     */
    if (iap->lib->homcont.blhom.iequib != 0 && iap->lib->homcont.blhom.iequib != -1) {
      func(*iap->lib->session, ndm, xequib1.data(), icp, par, 0, f.data(), &dum1, &dum2);
      for (i = 0; i < ndm; ++i) {
	fb[-1 + jb] = f[i];
	++jb;
      }
      /*        *NDM extra initial conditions for the equilibrium if IEQ
		UIB=-2 */
      if (iap->lib->homcont.blhom.iequib == -2) {
	func(*iap->lib->session, ndm, xequib2.data(), icp, par, 0, f.data(), &dum1, &dum2);
	for (i = 0; i < ndm; ++i) {
	  fb[-1 + jb] = f[i];
	  ++jb;
	}
      }
    }
    /*       *extra boundary condition in the case of a saddle-node homocl
	     inic*/
    if (iap->lib->homcont.blhom.iequib == 2) {
      if (ineig == 0) {
	eighi(iap, 1, 1, rr.data(), ri.data(), vt.data(), xequib1.data(), icp, par, &ndm);
	ineig = 1;
      }
      fb[-1 + jb] = rr[iap->lib->homcont.blhom.nstab];
      ++jb;
    }
    /*        *boundary conditions for normal vector */
    if (iap->lib->homcont.blhom.itwist == 1) {
      /*           *-orthogonal to the unstable directions of A  at t=0 
       */
      prjcti(iap, bound.data(), xequib1.data(), icp, par, 1, 1, 2, &ndm);
      for (i = ndm - iap->lib->homcont.blhom.nunstab; i < ndm; ++i) {
	dum = 0.;
	for (k = 0; k < ndm; ++k) {
	  dum += u0[ndm + k] * bound[i + k * (iap->ndm)];
	}
	fb[-1 + jb] = dum;
	++jb;
      }
      /*           *-orthogonal to the stable directions of A  at t=1 */
      prjcti(iap, bound.data(), xequib2.data(), icp, par, -1, 2, 2, &ndm);
      for (i = 0; i < iap->lib->homcont.blhom.nstab; ++i) {
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
    ip = 12;
    if (iap->lib->homcont.blhom.iequib >= 0) {
      ip += ndm;
    } else {
      ip += ndm << 1;
    }
    kp = ip;
    /*        *Explicit boundary conditions for homoclinic orbit at t=0 */
    eighi(iap, 1, 2, rr.data(), ri.data(), vr.data(), xequib1.data(), icp, par, &ndm);
    ieig = 1;
    if (iap->lib->homcont.blhom.nunstab > 1) {
      dum = 0.;
      kp = ip + iap->lib->homcont.blhom.nunstab;
      jb = ndm + 1;
      for (j = 0; j < iap->lib->homcont.blhom.nunstab; ++j) {
	for (i = 0; i < ndm; ++i) {
	  fb[i] = u0[i] - xequib1[i] - par[ip + j] * vr[iap->lib->homcont.blhom.nstab + j + i * (iap->ndm)];
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
	  vr[iap->lib->homcont.blhom.nstab + i * (iap->ndm)];
      }
      jb = ndm + 1;
    }
    /*        *Projection boundary conditions for the homoclinic orbit at 
t=1 */
    if (ineig == 0) {
      eighi(iap, 1, 1, rr.data(), ri.data(), vt.data(), xequib2.data(), icp, par, &ndm);
      ineig = 1;
    }
    for (i = 0; i < iap->lib->homcont.blhom.nunstab; ++i) {
      k = i + iap->lib->homcont.blhom.nstab;
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
    if (iap->lib->homcont.blhom.iequib != 0 && iap->lib->homcont.blhom.iequib != -1) {
      func(*iap->lib->session, ndm, xequib1.data(), icp, par, 0, f.data(), &dum1, &dum2);
      for (i = 0; i < ndm; ++i) {
	fb[-1 + jb] = f[i];
	++jb;
      }
      /*        *NDM extra initial conditions for the equilibrium if IEQ
		UIB=-2 */
      if (iap->lib->homcont.blhom.iequib == -2) {
	func(*iap->lib->session, ndm, xequib2.data(), icp, par, 0, f.data(), &dum1, &dum2)
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
  if (iap->lib->homcont.bcnn.nbcn > 0) {
    bcnd(*iap->lib->session, ndim, par, icp, iap->lib->homcont.bcnn.nbcn, u0, u1, ijc, fj.data(), dbc);
    for (k = 0; k < iap->lib->homcont.bcnn.nbcn; ++k) {
      fb[-1 + jb] = fj[k];
      /*            write(9,*),fb(jb),par(30) */
      ++jb;
    }
  }

  return 0;
} /* fbho_ */

/*     ---------- ---- */
/* Subroutine */ int 
icho(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nint, const doublereal *u, const doublereal *uold, const doublereal *udot, const doublereal *upold, doublereal *f, integer ijac, doublereal *dint)
{
  /* System generated locals */
  integer dint_dim1;

  /* Local variables */

  integer nfpr;
  integer i, j;
  doublereal ep;
  integer nnt0;

  std::vector<doublereal> ff1(iap->nint);
  std::vector<doublereal> ff2(iap->nint);
  std::vector<doublereal> uu1(iap->ndim);
  std::vector<doublereal> uu2(iap->ndim);
  std::vector<doublereal> dfu((iap->ndim)*(iap->ndim + NPARX));

/* Generates integral conditions for homoclinic bifurcation analysis */

/* Local */

    /* Parameter adjustments */
    /*--par;*/
    /*--icp;*/
    /*--u;*/
    /*--uold;*/
    /*--udot;*/
    /*--upold;*/
    /*--f;*/
  dint_dim1 = nint;
  
  nnt0 = iap->nnt0;
  nfpr = iap->nfpr;

/* Generate the function. */

  fiho(iap, rap, ndim, par, icp, nint, nnt0, u, uold, 
       udot, upold, f, dfu.data());

  if (ijac == 0) {
    return 0;
  }

  /* Generate the Jacobian. */

  ep = xpp::auto_jacobian::central(
      ndim, nint, u, uu1.data(), uu2.data(), ff1.data(), ff2.data(),
      [&](const doublereal *uu, doublereal *ff) { fiho(iap, rap, ndim, par, icp, nint, nnt0, uu, uold , udot, upold, ff, dfu.data()); },
      dint, dint_dim1);

  for (i = 0; i < nfpr; ++i) {
    par[icp[i]] += ep;
    fiho(iap, rap, ndim, par, icp, nint, nnt0, u, 
	 uold, udot, upold, ff1.data(), dfu.data());
    for (j = 0; j < nint; ++j) {
      ARRAY2D(dint, j, ndim + icp[i]) = (ff1[j] - f[j]) / ep;
    }
    par[icp[i]] -= ep;
  }

  return 0;
} /* icho_ */

/*     ---------- ---- */
/* Subroutine */ int 
fiho(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nint, integer nnt0, const doublereal *u, const doublereal *uold, const doublereal *udot, const doublereal *upold, doublereal *fi, doublereal *dint)
{

    /* Local variables */
  integer ijac = 0;

  integer i, jb;
  integer ndm;
  doublereal dum;

  std::vector<doublereal> fj(iap->ndim);
  /* Generates the integral conditions for homoclinic orbits. */

  /* Parameter adjustments */
  /*--par;*/
  /*--icp;*/
  /*--u;*/
  /*--uold;*/
  /*--udot;*/
  /*--upold;*/
  /*--fi;*/

  ndm = iap->ndm;
  jb = 0;

/* Integral phase condition for homoclinic orbit */

  if (iap->lib->homcont.blhom.nrev == 0) {
    dum = 0.;
    for (i = 0; i < ndm; ++i) {
      dum += upold[i] * (u[i] - uold[i]);
    }
    ++jb;
    fi[-1 + jb] = dum;
  }

  /* Integral phase condition for adjoint equation */

  if (iap->lib->homcont.blhom.itwist == 1) {
    dum = 0.;
    for (i = 0; i < ndm; ++i) {
      dum += uold[ndm + i] * (u[ndm + i] - uold[ndm + i]);
    }
    ++jb;
    fi[1] = dum;
  }

  /* User-defined integral constraints */

  if (jb < nint) {
    icnd(ndm, par, icp, nint, u, uold, udot, 
	 upold, ijac, fj.data(), dint);
    for (i = 0; i < nint - jb; ++i) {
      fi[i + jb] = fj[i];
    }
  }
  return 0;
} /* fiho_ */

/*     ---------- ---- */
/* Subroutine */ int 
inho(iap_type *iap, integer *icp, doublereal *par)
{

    /* Local variables */
  integer ndim, nint, nuzr, i, nfree, icorr, nbc, ndm, isw;

  /* Allocate memory for global structures. */

  iap->lib->homcont.blhmp.ipsi.assign(NPARX, 0);
  iap->lib->homcont.blhmp.ifixed.assign(NPARX, 0);
  iap->lib->homcont.blhmp.irev.assign((iap->ndim), 0);

  iap->lib->homcont.blhme.ieigc.assign(2, 0);

  iap->lib->homcont.beyn.iflag.assign(4, 0);

  /* the prjctn_ function uses this array to test if this is
       the first time the prjctn_ function has been called.
       Accordingly, I initialize it to zero here after I
       have created it. */
  for(i=0;i<4;i++)
    iap->lib->homcont.beyn.iflag[i]=0;

/* Reads from fort.11 specific constants for homoclinic continuation. */
/* Sets up re-defined constants in IAP. */
/* Sets other constants in the following common blocks. */

/* set various constants */

    /* Parameter adjustments */
    /*--par;*/
    /*--icp;*/

  ndim = iap->ndim;
  isw = iap->isw;
  nbc = iap->nbc;
  nint = iap->nint;
  nuzr = iap->nuzr;
  ndm = ndim;
  iap->lib->homcont.blhma.compzero = HMACHHO;
  iap->lib->homcont.blhom.nunstab=iap->lib->session->auto_state.run.nunstab;
  iap->lib->homcont.blhom.nstab=iap->lib->session->auto_state.run.nstab;
  iap->lib->homcont.blhom.iequib=iap->lib->session->auto_state.run.iequib;
  iap->lib->homcont.blhom.itwist=0;
  iap->lib->homcont.blhom.istart=2;
  iap->lib->homcont.blhom.nrev=0;
  iap->lib->homcont.blhom.nfixed=0;
  iap->lib->homcont.blhom.npsi=0;
  /* updated reading in of constants for reversible equations */
  /* replaces location in datafile of compzero */

  ndim = ndm * (iap->lib->homcont.blhom.itwist + 1);
  /* Allocate memory for global structures.  We didn't know the
     size for these until ndim was computed. */

  iap->lib->homcont.blhmu.pu0.assign((ndim), 0.);
  iap->lib->homcont.blhmu.pu1.assign((ndim), 0.);

  iap->lib->homcont.bleig.rr.assign((ndim), 0.);
  iap->lib->homcont.bleig.ri.assign((ndim), 0.);
  iap->lib->homcont.bleig.v.assign((ndim)*(ndim), 0.);
  iap->lib->homcont.bleig.vt.assign((ndim)*(ndim), 0.);
  iap->lib->homcont.bleig.xequib.assign((ndim), 0.);

  iap->lib->homcont.blhme.vrprev.assign(2*(ndim)*(ndim), 0.);

  iap->lib->homcont.beyn.cprev.assign(2*2*(ndim)*(ndim), 0.);

  nfree = iap->lib->homcont.blhom.nfixed + 2 - iap->lib->homcont.blhom.nrev + nint + nbc;
  iap->lib->homcont.bcnn.nbcn = nbc;

/* Free parameter (artificial parameter for psi) */
/* nondegeneracy parameter of the adjoint */

  if (iap->lib->homcont.blhom.itwist == 1) {
    ++nfree;
    icp[-1 + nfree] = 9;
    par[9] = 0.;
  }

  /* Extra free parameters for equilibrium if iequib=1,2,-2 */

  if (iap->lib->homcont.blhom.iequib != 0 && iap->lib->homcont.blhom.iequib != -1) {
    for (i = 0; i < ndm; ++i) {
      icp[nfree + i] = i + 11;
    }
  }

  if (iap->lib->homcont.blhom.iequib == -2) {
    for (i = 0; i < ndm; ++i) {
      icp[nfree + ndm + i] = ndm + 11 + i;
    }
  }

  if (iap->lib->homcont.blhom.istart != 3) {
    /*     *regular continuation */

    nint = nint + iap->lib->homcont.blhom.itwist + 1 - iap->lib->homcont.blhom.nrev;

    if (isw == 2) {
      icorr = 2;
    } else {
      icorr = 1;
    }
    nbc = iap->lib->homcont.blhom.nstab + iap->lib->homcont.blhom.nunstab + (iap->lib->homcont.blhom.itwist + iap->lib->homcont.blhom.iequib) * ndm + nfree - nint - icorr;
    if (iap->lib->homcont.blhom.iequib == 2) {
      nbc = nbc - ndm + 1;
    }
    if (iap->lib->homcont.blhom.iequib < 0) {
      nbc -= (iap->lib->homcont.blhom.iequib * 3 + 2) * ndm;
    }
  } else {
    /*     *starting solutions using homotopy */
    if (iap->lib->homcont.blhom.nunstab == 1) {
      nbc = ndm * (iap->lib->homcont.blhom.iequib + 1) + 1;
    } else {
      nbc = ndm * (iap->lib->homcont.blhom.iequib + 1) + iap->lib->homcont.blhom.nunstab + 1;
    }
    if (iap->lib->homcont.blhom.iequib == 2) {
      xpp::print(iap->lib->fp9,"WARNING: IEQUIB=2 NOT ALLOWED WITH ISTART=3\n");	
    }
    if (iap->lib->homcont.blhom.iequib < 0) {
      nbc -= ndm * (iap->lib->homcont.blhom.iequib * 3 + 2);
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

/*     ---------- ----- */
/* Subroutine */ int 
preho(AutoLib &lib, integer *ndx, integer *ntsr, integer *nar, integer *ndim, integer *ncolrs, doublereal *ups, doublereal *udotps, doublereal *tm, doublereal *par)
{
  /* System generated locals */
  integer ups_dim1, udotps_dim1;

  /* Local variables */
  integer jmin;
  doublereal upsi;
  integer i, j, k;
  doublereal tmmin;
  integer k1, k2, ii;
  doublereal upsmin;
  integer ist;

/* Preprocesses (perturbs) restart data to enable */
/* initial computation of the adjoint variable */

    /* Parameter adjustments */
    /*--tm;*/
    /*--par;*/
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

  if (lib.homcont.blhom.istart == 1) {

    /* First find smallest value in norm */

    upsmin = 1e20;
    jmin = 1;
    for (j = 0; j < *ntsr + 1; ++j) {
      upsi = 0.;
      for (i = 0; i < *nar; ++i) {
	upsi += (ARRAY2D(ups, j, i) - par[i + 11]) * (ARRAY2D(ups, j, i) - par[i + 11]);
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

/*     ---------- ------ */
/* Subroutine */ int 
stpnho(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *ntsr, integer *ncolrs, doublereal *rlcur, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *upoldp, doublereal *tm, doublereal *dtm, integer *nodir, doublereal *thl, doublereal *thu)
{
  /* System generated locals */
  integer ups_dim1;

  /* Local variables */
  integer ndim, ncol, nfpr, ntst, ncol1, i, j, k;
  doublereal t;
  integer k1, k2;

  doublereal dt;
  integer lab, ibr;

  std::vector<doublereal> u(iap->ndim);
  /* Generates a starting point for the continuation of a branch of */
  /* of solutions to general boundary value problems by calling the user */
  /* supplied subroutine STPNT where an analytical solution is given. */

  /* Local */

    /* Parameter adjustments */
    /*--par;*/
    /*--icp;*/
    /*--rlcur;*/
    /*--rldot;*/
    /*--tm;*/
    /*--dtm;*/
  ups_dim1 = *ndxloc;

  ndim = iap->ndim;
  ntst = iap->ntst;
  ncol = iap->ncol;
  nfpr = iap->nfpr;

/* Generate the (initially uniform) mesh. */

  msh(iap, rap, tm);
  dt = 1. / (ntst * ncol);

  for (j = 0; j < ntst + 1; ++j) {
    if (j == ntst) {
      ncol1 = 1;
    } else {
      ncol1 = ncol;
    }
    for (i = 0; i < ncol1; ++i) {
      t = tm[j] + i * dt;
      k1 = i * ndim;
      k2 = (i + 1) * ndim - 1;
      stpho(iap, icp, u.data(), par, &t);
      for (k = k1; k <= k2; ++k) {
	ARRAY2D(ups, j, k) = u[k - k1];
      }
    }
  }

  *ntsr = ntst;
  *ncolrs = ncol;
  ibr = 1;
  iap->ibr = ibr;
  lab = 0;
  iap->lab = lab;

  for (i = 0; i < nfpr; ++i) {
    rlcur[i] = par[icp[i]];
  }

  *nodir = 1;
  return 0;
} /* stpnho_ */

/*     ---------- ----- */
/* Subroutine */ int 
stpho(iap_type *iap, integer *icp, doublereal *u, doublereal *par, doublereal *t)
{
    /* Local variables */

  integer i, j;

  integer ip;
  integer kp;
  integer ndm;

  std::vector<doublereal> ri(iap->ndm);
  std::vector<doublereal> rr(iap->ndm);
  std::vector<doublereal> vr((iap->ndm)*(iap->ndm));
  std::vector<doublereal> vt((iap->ndm)*(iap->ndm));
  std::vector<doublereal> xequib(iap->ndm);

  /* Generates a starting point for homoclinic continuation */
  /* If ISTART=2 it calls STPNHO. */
  /* If ISTART=3 it sets up the homotopy method. */

/* Local */

    /* Parameter adjustments */
    /*--par;*/
    /*--u;*/
    /*--icp;*/

  ndm = iap->ndm;

  /* Initialize parameters */

  stpnt(*iap->lib->session, ndm, *t, u, par);

  /* Initialize solution and additional parameters */

  switch (static_cast<int>(iap->lib->homcont.blhom.istart)) {
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

  pvls(ndm, u, par);
  for (i = 0; i < ndm; ++i) {
    xequib[i] = par[i + 11];
  }
  eighi(iap, 1, 1, rr.data(), ri.data(), vt.data(), xequib.data(), icp, par, &ndm);
  eighi(iap, 1, 2, rr.data(), ri.data(), vr.data(), xequib.data(), icp, par, &ndm);

  /* Set up artificial parameters at the left-hand end point of orbit */

  ip = 12;
  if (iap->lib->homcont.blhom.iequib >= 0) {
    ip += ndm;
  } else {
    ip += ndm * 2;
  }
  kp = ip;

/* Parameters xi 1=1, xi i=0, i=2,NSTAB */

  par[ip] = 1.;
  if (iap->lib->homcont.blhom.nunstab > 1) {
    for (i = 1; i < iap->lib->homcont.blhom.nunstab; ++i) {
      par[ip + i] = 0.;
    }
  }
  ip += iap->lib->homcont.blhom.nunstab;

/*Starting guess for homoclinic orbit in real principal unstable direction
*/

  for (i = 0; i < ndm; ++i) {
    u[i] = xequib[i] + vr[iap->lib->homcont.blhom.nstab + i * (iap->ndm)]
      * par[-1 + kp] * par[kp] * xpp::math::exp(rr[iap->lib->homcont.blhom.nstab] * *t * par[10]);
  }
  for (i = 0; i < ndm; ++i) {
    xpp::print(iap->lib->fp9,"stpho {:20.10f}\n",u[i]);	
  }
  xpp::print(iap->lib->fp9,"\n");	

/* Artificial parameters at the right-hand end point of the orbit */
/* omega_i=<x(1)-x_o,w_i^*> */

  for (i = 0; i < iap->lib->homcont.blhom.nunstab; ++i) {
    par[ip + i] = 0.;
    for (j = 0; j < ndm; ++j) {
      par[ip + i] += vr[iap->lib->homcont.blhom.nstab + j * (iap->ndm)] * par[-1 + kp] * par[kp] * xpp::math::exp(rr[iap->lib->homcont.blhom.nstab] * par[10]) * 
	vt[iap->lib->homcont.blhom.nstab + i + j * (iap->ndm)];
    }
  }
  ip += iap->lib->homcont.blhom.nunstab;
  return 0;
  /* -----------------------------------------------------------------------
   */
} /* stpho_ */

/*     ---------- ------ */
/* Subroutine */ int 
pvlsho(iap_type *iap, rap_type *rap, integer *icp, doublereal *dtm, integer *ndxloc, doublereal *ups, integer *ndim, doublereal *p0, doublereal *p1, doublereal *par)
{

  /* Local variables */
  integer i, j;

  doublereal orient;

  integer iid, ndm;

    /* Parameter adjustments */
    /*--icp;*/
    /*--dtm;*/
    /*--par;*/

  iid = iap->iid;
  ndm = iap->ndm;

  pvlsbv(iap, rap, icp, dtm, ndxloc, ups, ndim, 
	 p0, p1, par);

  /*      *Compute eigenvalues */
  iap->lib->homcont.bleig.ineig = 0;
  for (i = 0; i < ndm; ++i) {
    iap->lib->homcont.bleig.xequib[i] = par[i + 11];
  }
  eighi(iap, 1, 2, iap->lib->homcont.bleig.rr.data(), iap->lib->homcont.bleig.ri.data(), iap->lib->homcont.bleig.v.data(), iap->lib->homcont.bleig.xequib.data(), 
	icp, par, &ndm);
  if (iid >= 3) {
    xpp::print(iap->lib->fp9,"EIGENVALUES\n");	
    for (j = 0; j < ndm; ++j) {
      xpp::print(iap->lib->fp9," ({:12.7f} {:12.7f})\n",iap->lib->homcont.bleig.rr[j],iap->lib->homcont.bleig.ri[j]);	
    }
  }
  if (iap->lib->homcont.blhom.itwist == 1) {
    eighi(iap, 1, 1, iap->lib->homcont.bleig.rr.data(), iap->lib->homcont.bleig.ri.data(), iap->lib->homcont.bleig.vt.data(), 
	  iap->lib->homcont.bleig.xequib.data(), icp, par, &ndm);
    iap->lib->homcont.bleig.ineig = 1;
    orient = psiho(iap, 0, iap->lib->homcont.bleig.rr.data(), iap->lib->homcont.bleig.ri.data(), iap->lib->homcont.bleig.v.data(), 
		   iap->lib->homcont.bleig.vt.data(), icp, par);
    if (iid >= 3) {
      if (orient < 0.) {
	xpp::print(iap->lib->fp9," Non-orientable, ({:20.10f})\n",orient);	
      } else {
	xpp::print(iap->lib->fp9," Orientable ({:20.10f})\n",orient);	
      }
    }
  }

  for (i = 0; i < iap->lib->homcont.blhom.npsi; ++i) {
    if (iap->lib->homcont.blhmp.ipsi[i] > 10 && iap->lib->homcont.bleig.ineig == 0) {
      eighi(iap, 1, 1, iap->lib->homcont.bleig.rr.data(), iap->lib->homcont.bleig.ri.data(), iap->lib->homcont.bleig.vt.data(), 
	    iap->lib->homcont.bleig.xequib.data(), icp, par, &ndm);
      iap->lib->homcont.bleig.ineig = 1;
    }
    par[iap->lib->homcont.blhmp.ipsi[i] + 19] = psiho(iap, iap->lib->homcont.blhmp.ipsi[i], iap->lib->homcont.bleig.rr.data(), iap->lib->homcont.bleig.ri.data(), iap->lib->homcont.bleig.v.data(), iap->lib->homcont.bleig.vt.data(), icp, par);
    if (iid >= 3) {
      xpp::print(iap->lib->fp9," PSI({:2})={:20.10f}\n",iap->lib->homcont.blhmp.ipsi[i],par[iap->lib->homcont.blhmp.ipsi[i] + 19]);	

    }
  }

  return 0;

} /* pvlsho_ */

/*     -------- ------- -------- ----- */
doublereal 
psiho(const iap_type *iap, integer is, doublereal *rr, doublereal *ri, doublereal *v, doublereal *vt, const integer *icp, doublereal *par)
{
  /* System generated locals */
  doublereal ret_val;

    /* Local variables */

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

/* Local */

    /* Parameter adjustments */
    /*--par;*/
    /*--icp;*/
    /*--ri;*/
    /*--rr;*/
  vt -= ((iap->ndm)+1);
  v -= ((iap->ndm)+1);

  ndm = iap->ndm;

  func(*iap->lib->session, ndm, iap->lib->homcont.blhmu.pu0.data(), icp, par, 0, f0.data(), &dum1, &dum2);
  func(*iap->lib->session, ndm, iap->lib->homcont.blhmu.pu1.data(), icp, par, 0, f1.data(), &dum1, &dum2);

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
      s1 += f1[j] * iap->lib->homcont.blhmu.pu0[ndm + j];
      s2 += f0[j] * iap->lib->homcont.blhmu.pu1[ndm + j];
      /* Computing 2nd power */
      f0norm += f0[j] * f0[j];
      /* Computing 2nd power */
      f1norm += f1[j] * f1[j];
      /* Computing 2nd power */
      u0norm += iap->lib->homcont.blhmu.pu0[j + ndm] * iap->lib->homcont.blhmu.pu0[j + ndm];
      /* Computing 2nd power */
      u1norm += iap->lib->homcont.blhmu.pu1[j + ndm] * iap->lib->homcont.blhmu.pu1[j + ndm];
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
  ret_val = rr[-1 + iap->lib->homcont.blhom.nstab] + rr[iap->lib->homcont.blhom.nstab] + ri[-1 + iap->lib->homcont.blhom.nstab] + 
    ri[iap->lib->homcont.blhom.nstab];
  return ret_val;

/* Double real leading eigenvalues (stable) */
/*   (saddle, saddle-focus transition) */

 L2:
  if (f2c::abs(ri[-1 + iap->lib->homcont.blhom.nstab]) > iap->lib->homcont.blhma.compzero) {
    /* Computing 2nd power */
    doublereal tmp= ri[-1 + iap->lib->homcont.blhom.nstab] - ri[-1 + iap->lib->homcont.blhom.nstab - 1];
    ret_val = -(tmp * tmp);
  } else {
    /* Computing 2nd power */
    doublereal tmp = rr[-1 + iap->lib->homcont.blhom.nstab] - rr[-1 + iap->lib->homcont.blhom.nstab - 1];
    ret_val = tmp * tmp;
  }
  return ret_val;

/* Double real positive eigenvalues (unstable) */
/*   (saddle, saddle-focus transition) */

 L3:
  if (f2c::abs(ri[iap->lib->homcont.blhom.nstab]) > iap->lib->homcont.blhma.compzero) {
    /* Computing 2nd power */
    doublereal tmp = ri[iap->lib->homcont.blhom.nstab] - ri[iap->lib->homcont.blhom.nstab + 1];
    ret_val = -(tmp * tmp);
  } else {
    /* Computing 2nd power */
    doublereal tmp = rr[iap->lib->homcont.blhom.nstab] - rr[iap->lib->homcont.blhom.nstab + 1];
    ret_val = tmp * tmp;
  }
  return ret_val;

/* Neutral saddle, saddle-focus or bi-focus (includes 1, above, also) */

 L4:
  ret_val = rr[-1 + iap->lib->homcont.blhom.nstab] + rr[iap->lib->homcont.blhom.nstab];
  return ret_val;

  /* Neutrally-divergent saddle-focus (stable eigenvalues complex) */

 L5:
  ret_val = rr[-1 + iap->lib->homcont.blhom.nstab] + rr[iap->lib->homcont.blhom.nstab] + rr[iap->lib->homcont.blhom.nstab - 2];
  return ret_val;

/* Neutrally-divergent saddle-focus (unstable eigenvalues complex) */

 L6:
  ret_val = rr[-1 + iap->lib->homcont.blhom.nstab] + rr[iap->lib->homcont.blhom.nstab] + rr[iap->lib->homcont.blhom.nstab + 1];
  return ret_val;

/* Three leading eigenvalues (stable) */

 L7:
  ret_val = rr[-1 + iap->lib->homcont.blhom.nstab] - rr[iap->lib->homcont.blhom.nstab - 3];
  return ret_val;

  /* Three leading eigenvalues (ustable) */

 L8:
  ret_val = rr[iap->lib->homcont.blhom.nstab] - rr[iap->lib->homcont.blhom.nunstab + 2];
  return ret_val;

  /* Local bifurcation (zero eigenvalue or Hopf): NSTAB decreases */
  /*  (nb. the problem becomes ill-posed after a zero of 9 or 10) */

 L9:
  ret_val = rr[-1 + iap->lib->homcont.blhom.nstab];
  return ret_val;

/* Local bifurcation (zero eigenvalue or Hopf): NSTAB increases */

 L10:
  ret_val = rr[iap->lib->homcont.blhom.nstab];
  return ret_val;

  /* Orbit flip (with respect to leading stable direction) */
  /*     e.g. 1D unstable manifold */

 L11:
  for (j = 0; j < ndm; ++j) {
    ret_val += f1[j] * vt[iap->lib->homcont.blhom.nstab + (j + 1) * (iap->ndm)];
  }
  ret_val *= xpp::math::exp(-par[10] * rr[-1 + iap->lib->homcont.blhom.nstab] / 2.);
  return ret_val;

  /* Orbit flip (with respect to leading unstable direction) */
  /*     e.g. 1D stable manifold */

 L12:
  for (j = 0; j < ndm; ++j) {
    ret_val += f0[j] * vt[iap->lib->homcont.blhom.nstab + 1 + (j + 1) * (iap->ndm)];
  }
  ret_val *= xpp::math::exp(par[10] * rr[iap->lib->homcont.blhom.nstab] / 2.);
  return ret_val;

  /* Inclination flip (critically twisted) with respect to stable manifold 
*/
/*   e.g. 1D unstable manifold */

 L13:
  for (i = 0; i < ndm; ++i) {
    ret_val += iap->lib->homcont.blhmu.pu0[ndm + i] * v[iap->lib->homcont.blhom.nstab + (i + 1) * (iap->ndm)]
      ;
  }
  ret_val *= xpp::math::exp(-par[10] * rr[-1 + iap->lib->homcont.blhom.nstab] / 2.);
  return ret_val;

  /* Inclination flip (critically twisted) with respect to unstable manifold
 */
/*   e.g. 1D stable manifold */

 L14:
  for (i = 0; i < ndm; ++i) {
    ret_val += iap->lib->homcont.blhmu.pu1[ndm + i] * v[iap->lib->homcont.blhom.nstab + 1 + (i + 1) * (iap->ndm)];
  }
  ret_val *= xpp::math::exp(par[10] * rr[iap->lib->homcont.blhom.nstab] / 2.);
  return ret_val;

  /* Non-central homoclinic to saddle-node (in stable manifold) */

 L15:
  for (i = 0; i < ndm; ++i) {
    ret_val += (par[i + 11] - iap->lib->homcont.blhmu.pu1[i]) * v[iap->lib->homcont.blhom.nstab + 1 + (i + 1) * (iap->ndm)];
  }
  return ret_val;

/* Non-central homoclinic to saddle-node (in unstable manifold) */

 L16:
  for (i = 0; i < ndm; ++i) {
    ret_val += (par[i + 11] - iap->lib->homcont.blhmu.pu0[i]) * v[iap->lib->homcont.blhom.nstab + 1 + (i + 1) * (iap->ndm)];
  }
  return ret_val;

} /* psiho_ */

/*     ---------- ----- */
/* Subroutine */ int 
eighi(const iap_type *iap, integer isign, integer itrans, doublereal *rr, doublereal *ri, doublereal *vret, doublereal *xequib, const integer *icp, doublereal *par, integer *ndm)
{

  std::vector<doublereal> dfdp((*ndm)*NPARX);
  std::vector<doublereal> dfdu((*ndm)*(*ndm));
  std::vector<doublereal> zz((*ndm)*(*ndm));

  eigho(iap, &isign, &itrans, rr, ri, vret, xequib, icp, par, ndm, dfdu.data(), dfdp.data(), zz.data());

  return 0;
} /* eighi */

/*     ---------- ----- */
/* Subroutine */ int 
eigho(const iap_type *iap, integer *isign, integer *itrans, doublereal *rr, doublereal *ri, doublereal *vret, doublereal *xequib, const integer *icp, doublereal *par, integer *ndm, doublereal *dfdu, doublereal *dfdp, doublereal *zz)
{
  /* System generated locals */
  integer dfdu_dim1, zz_dim1;

  /* Local variables */

  integer i, j, k, ifail;
  doublereal vdot;

  std::vector<doublereal> f(*ndm);
  std::vector<doublereal> ridum(*ndm);
  std::vector<doublereal> vidum((*ndm)*(*ndm));
  std::vector<doublereal> rrdum(*ndm);
  std::vector<doublereal> vrdum((*ndm)*(*ndm));
  std::vector<doublereal> vi((*ndm)*(*ndm));
  std::vector<doublereal> vr((*ndm)*(*ndm));
  std::vector<doublereal> fv1(*ndm);
  std::vector<integer> iv1(*ndm);

  /* Uses EISPACK routine RG to calculate the eigenvalues/eigenvectors */
  /* of the linearization matrix a (obtained from DFHO) and orders them */
  /* according to their real parts. Simple continuity with respect */
  /* previous call with same value of ITRANS. */

  /* 	input variables */
  /* 		ISIGN  = 1 => left-hand endpoint */
  /*       	       = 2 => right-hand endpoint */
  /*               ITRANS = 1 use transpose of A */
  /*                      = 2 otherwise */

/*       output variables */
/* 		RR,RI real and imaginary parts of eigenvalues, ordered w.r.t */
/* 	           real parts (largest first) */
/* 	        VRET the rows of which are real parts of corresponding */
/*                  eigenvectors */

/* Local */

    /* Parameter adjustments */
    /*--rr;*/
    /*--ri;*/
    /*--xequib;*/
    /*--icp;*/
    /*--par;*/
  vret -= ((*ndm)+1);
  zz_dim1 = *ndm;
  dfdu_dim1 = *ndm;
    
  ifail = 0;

  func(*iap->lib->session, *ndm, xequib, icp, par, 1, f.data(), dfdu, 
       dfdp);

  if (*itrans == 1) {
    for (i = 0; i < *ndm; ++i) {
      for (j = 0; j < *ndm; ++j) {
	vrdum[i + j * (*ndm)] = ARRAY2D(dfdu, j, i);
      }
    }
    for (i = 0; i < *ndm; ++i) {
      for (j = 0; j < *ndm; ++j) {
	ARRAY2D(dfdu, i, j) = vrdum[i + j * (*ndm)];
      }
    }
  }

  /* EISPACK call for eigenvalues and eigenvectors */
  rg(*ndm, *ndm, dfdu, rr, ri, 1, zz, 
     iv1.data(), fv1.data(), &ifail);

  if (ifail != 0) {
    xpp::print(iap->lib->fp9,"EISPACK EIGENVALUE ROUTINE FAILED !\n");	
  }

  for (j = 0; j < *ndm; ++j) {
    if (ri[j] > 0.) {
      for (i = 0; i < *ndm; ++i) {
	vr[i + j * (*ndm)] = ARRAY2D(zz, i, j);
	vi[i + j * (*ndm)] = ARRAY2D(zz, i, (j + 1));
      }
    } else if (ri[j] < 0.) {
      for (i = 0; i < *ndm; ++i) {
	vr[i + j * (*ndm)] = ARRAY2D(zz, i, (j - 1));
	vi[i + j * (*ndm)] = -ARRAY2D(zz, i, j);
      }
    } else {
      for (i = 0; i < *ndm; ++i) {
	vr[i + j * (*ndm)] = ARRAY2D(zz, i, j);
	vi[i + j * (*ndm)] = 0.;
      }
    }
  }
  /*Order the eigenvectors/values according size of real part of eigenvalue.
*/
/*     (smallest first) */

  for (i = 0; i < *ndm - 1; ++i) {
    for (j = i + 1; j < *ndm; ++j) {
      if (rr[i] > rr[j]) {
	rrdum[i] = rr[i];
	ridum[i] = ri[i];
	rr[i] = rr[j];
	rr[j] = rrdum[i];
	ri[i] = ri[j];
	ri[j] = ridum[i];
	for (k = 0; k < *ndm; ++k) {
	  vrdum[k + i * (*ndm)] = vr[k + i * (*ndm)];
	  vr[k + i * (*ndm)] = vr[k + j * (*ndm)];
	  vr[k + j * (*ndm)] = vrdum[k + i * (*ndm)];
	  vidum[k + i * (*ndm)] = vi[k + i * (*ndm)];
	  vi[k + i * (*ndm)] = vi[k + j * (*ndm)];
	  vi[k + j * (*ndm)] = vidum[k + i * (*ndm)];
	}
      }
    }
  }

  /* Choose sign of real part of eigenvectors to be */
  /* commensurate with that of the corresponding eigenvector */
  /* from the previous call with the same value of ISIGN */

  if (iap->lib->homcont.blhme.ieigc[*itrans - 1] == 0) {
    for (j = 0; j < *ndm; ++j) {
      for (i = 0; i < *ndm; ++i) {
	iap->lib->homcont.blhme.vrprev[*itrans + (i * 2 + j * (*ndm) * 2) - 1] = vr[i + j * (*ndm)];
      }
    }
    iap->lib->homcont.blhme.ieigc[*itrans - 1] = 1;
  }
  for (i = 0; i < *ndm; ++i) {
    vdot = 0.;
    {
      integer tmp;
      tmp = *ndm;
      for (j = 0; j < tmp; ++j) {
	vdot += vr[j + i * tmp] * iap->lib->homcont.blhme.vrprev[*itrans + (j * 2 + i * tmp * 2) - 1];
      }
    }
    if (vdot < 0.) {
      for (j = 0; j < *ndm; ++j) {
	vr[j + i * (*ndm)] = -vr[j + i * (*ndm)];
	/*               VI(J,I)=-VI(J,I) */
      }
    }
    for (j = 0; j < *ndm; ++j) {
      iap->lib->homcont.blhme.vrprev[*itrans + (j * 2 + i * (*ndm) * 2) - 1] = vr[j + i * (*ndm)];
    }
  }

  /* Send back the transpose of the matrix of real parts of eigenvectors */
  for (i = 0; i < *ndm; ++i) {
    for (j = 0; j < *ndm; ++j) {
      vret[(i + 1) + (j + 1) * (*ndm)] = vr[j + i * (*ndm)];
    }
  }

  return 0;
} /* eigho_ */

/*     ---------- ------ */
/* Subroutine */ int 
prjcti(const iap_type *iap, doublereal *bound, doublereal *xequib, const integer *icp, doublereal *par, integer imfd, integer is, integer itrans, integer *ndm)
{
  
  std::vector<doublereal> dfdp((*ndm)*NPARX);
  std::vector<doublereal> dfdu((*ndm)*(*ndm));
  
  prjctn(iap, bound, xequib, icp, par, &imfd, &is, &itrans, ndm, dfdu.data(), dfdp.data());
  
  return 0;
} /* prjcti */

/*     ---------- ------ */
/* Subroutine */ int 
prjctn(const iap_type *iap, doublereal *bound, doublereal *xequib, const integer *icp, doublereal *par, integer *imfd, integer *is, integer *itrans, integer *ndm, doublereal *dfdu, doublereal *dfdp)
{
  /* System generated locals */
  integer dfdu_dim1;

    /* Local variables */
  integer i, j, k;
  integer mcond, k1, k2, m0;

  doublereal det, eps;

  std::vector<doublereal> fdum(*ndm);
  std::vector<doublereal> cnow((*ndm)*(*ndm));
  std::vector<integer> type__(*ndm);
  std::vector<doublereal> a((*ndm)*(*ndm));
  std::vector<doublereal> d((*ndm)*(*ndm));
  std::vector<doublereal> v((*ndm)*(*ndm));
  std::vector<doublereal> ei(*ndm);
  std::vector<doublereal> er(*ndm);
  std::vector<doublereal> ort(*ndm);
  std::vector<doublereal> dum1((*ndm)*(*ndm));
  std::vector<doublereal> dum2((*ndm)*(*ndm));

  /* Compute NUNSTAB (or NSTAB) projection boundary condition functions */
  /*onto to the UNSTABLE (or STABLE) manifold of the appropriate equilibrium
   */

/*    IMFD   = -1 stable eigenspace */
/*           =  1 unstable eigenspace */
/*    ITRANS =  1 use transpose of A */
/*           =  2 otherwise */
/*    IS     =  I (1 or 2) implies use the ith equilibrium in XEQUIB */

/* Use the normalization in Beyn 1990 (4.4) to ensure continuity */
/* w.r.t parameters. */
/* For the purposes of this routine the "previous point on the */
/* branch" is at the values of PAR at which the routine was last */
/* called with the same values of IS and ITRANS. */

/* Local */

    /* Parameter adjustments */
    /*--xequib;*/
    /*--icp;*/
    /*--par;*/
  bound -= ((*ndm)+1);
  dfdu_dim1 = *ndm;
  
  func(*iap->lib->session, *ndm, xequib, icp, par, 1, fdum.data(), dfdu, dfdp);

  /* Compute transpose of A if ITRANS=1 */
  if (*itrans == 1) {
    for (i = 0; i < *ndm; ++i) {
      for (j = 0; j < *ndm; ++j) {
	a[i + j * (*ndm)] = ARRAY2D(dfdu, j, i);
      }
    }
  } else {
    for (i = 0; i < *ndm; ++i) {
      for (j = 0; j < *ndm; ++j) {
	a[i + j * (*ndm)] = ARRAY2D(dfdu, i, j);
      }
    }
  }

  /* Compute basis V to put A in upper Hessenberg form */
  {
    /* This is here since I don't want to change the calling sequence of the
       BLAS routines. */
    integer tmp = 1;
    orthes((ndm), ndm, &tmp, ndm, a.data(), ort.data());
    ortran((ndm), ndm, &tmp, ndm, a.data(), ort.data(), v.data());
  }

  /* Force A to be upper Hessenberg */
  if (*ndm > 2) {
    for (i = 2; i < *ndm; ++i) {
      for (j = 0; j < i - 1; ++j) {
	a[i + j * (*ndm)] = 0.;
      }
    }
  }

  /* Computes basis to put A in "Quasi Upper-Triangular form" */
  /* with the positive (negative) eigenvalues first if IMFD =-1 (=1) */
  eps = iap->lib->homcont.blhma.compzero;
  {
    /* This is here since I don't want to change the calling sequence of the
       BLAS routines. */
    integer tmp = 1;
    hqr3lc(a.data(), v.data(), ndm, &tmp, ndm, &eps, er.data(), ei.data(), type__.data(), (ndm), (ndm),
	   imfd);
  }
  /* Put the basis in the appropriate part of the matrix CNOW */
  if (*imfd == 1) {
    k1 = *ndm - iap->lib->homcont.blhom.nunstab + 1;
    k2 = *ndm;
  } else {
    k1 = 1;
    k2 = iap->lib->homcont.blhom.nstab;
  }
  mcond = k2 - k1 + 1;
  m0 = k1 - 1;

  for (i = k1 - 1; i < k2; ++i) {
    for (j = 0; j < *ndm; ++j) {
      cnow[i + j * (*ndm)] = v[j + (i - k1 + 1) * (*ndm)];
    }
  }

  /* Set previous matrix to be the present one if this is the first call */

    /* Note by Randy Paffenroth:  There is a slight problem here
       in that this array is used before it is assigned to,
       hence its value is, in general, undefined.  It has
       worked because the just happened to be filled
       with zeros, even though this is not guaranteed.*/
  if (iap->lib->homcont.beyn.iflag[*is + (*itrans *2 ) - 3] == 0) {
    for (i = k1 - 1; i < k2; ++i) {
      for (j = 0; j < *ndm; ++j) {
	iap->lib->homcont.beyn.cprev[i + (j + ((*is - 1) + ((*itrans - 1) * 2)) * (*ndm)) * (*ndm)] = cnow[i + j * (*ndm)];
	bound[(i + 1) + (j + 1) * (*ndm)] = cnow[i + j * (*ndm)];
      }
    }
    iap->lib->homcont.beyn.iflag[*is + (*itrans * 2) - 3] = 1;
    return 0;
  }

  /* Calculate the (transpose of the) BEYN matrix D and hence BOUND */
  for (i = 0; i < mcond; ++i) {
    for (j = 0; j < mcond; ++j) {
      dum1[i + j * (*ndm)] = 0.;
      dum2[i + j * (*ndm)] = 0.;
      {
	integer tmp;
	tmp = *ndm;
	for (k = 0; k < tmp; ++k) {
	  dum1[i + j * (tmp)] += iap->lib->homcont.beyn.cprev[i + m0 + (k + ((*is - 1) + ((*itrans - 1) * 2)) * (tmp)) * (tmp)] 
	    * cnow[j + m0 + k * (tmp)];
	  dum2[i + j * (tmp)] += iap->lib->homcont.beyn.cprev[i + m0 + (k + ((*is - 1) + ((*itrans - 1) * 2)) * (tmp)) * (tmp)] 
	    * iap->lib->homcont.beyn.cprev[j + m0 + (k + ((*is - 1) + ((*itrans - 1) * 2)) * (tmp)) * (tmp)];
	}
      }
    }
  }

  if (mcond > 0) {
    ge(iap->lib->fp9, mcond, *ndm, dum1.data(), mcond, *ndm, d.data(), *ndm,
       dum2.data(), &det);
  }

  for (i = 0; i < mcond; ++i) {
    for (j = 0; j < *ndm; ++j) {
      bound[(i + 1) + m0 + (j + 1) * (*ndm)] = 0.;
      for (k = 0; k < mcond; ++k) {
	bound[(i + 1) + m0 + (j + 1) * (*ndm)] += d[k + i * (*ndm)] * 
	  cnow[k + m0 + j * (*ndm)];
      }
    }
  }

  for (i = k1 - 1; i < k2; ++i) {
    for (j = 0; j < *ndm; ++j) {
      iap->lib->homcont.beyn.cprev[i + (j + ((*is - 1) + ((*itrans - 1) * 2)) * (*ndm)) * (*ndm)] = bound[(i + 1) + (j + 1) * (*ndm)];
    }
  }

  return 0;
} /* prjctn_ */

