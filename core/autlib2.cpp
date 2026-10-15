#include "xpp_mem.h"
/* Autlib2.f -- translated by f2c (version 19970805).
   You must link the resulting object file with the libraries:
	-lf2c -lm   (in that order)
*/

#include <vector>
#include "xpp_log.h"
#include "session.h"
#include "auto_c.h"
#include "xpp_job.h" /* xppautX: cancel */

/* The sizes follow the Fortran's macros: nrow = ndim*ncol (rows of A),
   nclm = nrow + ndim (its columns), nrc = nbc + nint + 1 (rows of C and D),
   and each array holds ntst + 1 mesh intervals (the last is a spare). */
void SolvbvWork::resize(std::ptrdiff_t ndim, std::ptrdiff_t ncol, std::ptrdiff_t ntst, std::ptrdiff_t nbc, std::ptrdiff_t nint, std::ptrdiff_t parameters)
{
  const std::size_t nrow = xpp::size_product(ndim,ncol);
  const std::size_t nclm = xpp::size_sum(nrow,ndim), nrc = xpp::size_sum(nbc,nint,1), nodes = xpp::size_sum(ntst,1);
  a.assign(xpp::size_product(nclm, nrow, nodes), 0.);
  b.assign(xpp::size_product(parameters, nrow, nodes), 0.);
  c.assign(xpp::size_product(nclm, nrc, nodes), 0.);
  d.assign(xpp::size_product(nrc, parameters), 0.);
  a1.assign(xpp::size_product(ndim, ndim, nodes), 0.);
  a2.assign(xpp::size_product(ndim, ndim, nodes), 0.);
  s1.assign(xpp::size_product(ndim, ndim, nodes), 0.);
  s2.assign(xpp::size_product(ndim, ndim, nodes), 0.);
  bb.assign(xpp::size_product(ndim, parameters, nodes), 0.);
  cc.assign(xpp::size_sum(xpp::size_product(nrc, ndim, nodes), 1), 0.);
  faa.assign(xpp::size_product(ndim, nodes), 0.);
  icf.assign(xpp::size_product(nclm, nodes), 0);
  irf.assign(xpp::size_product(nrow, nodes), 0);
  ipr.assign(xpp::size_product(ndim, nodes), 0);
  icf1.assign(xpp::size_product(ndim, nodes), 0);
  icf2.assign(xpp::size_product(ndim, nodes), 0);
}


/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*           Setting up of the Jacobian and right hand side */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/*     ---------- ------ */
/* Subroutine */ int 
solvbv(integer *ifst, iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), doublereal *rds, integer *nllv, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *dups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *dtm, doublereal *fa, doublereal *fc, doublereal *p0, doublereal *p1, doublereal *thl, doublereal *thu)
{

  /* Local variables */
  
  integer ndim;
  integer ncol, nclm, nfpr, nint, nrow, ntst;

  integer nbc, iid;
  doublereal det;
  xpp::AutoProblem ips;
  integer nrc;

  /* Sets up and solves the linear equations for one Newton/Chord iteration 
   */

  /* Most of the required memory is allocated below */
  /* This is an interesting section of code.  The main point
     is that setubv and conpar only get called when ifst
     is 1.  This is a optimization since you can solve
     the system using the previously factored jacobian.
     One thing to watch out for is that two seperate calls
     of solvbv_ talk to each other through these arrays,
     so it is only safe to get rid of them when ifst is
     1 (since their entries will then be recreated in conpar
     and setubv).
  */

  std::vector<doublereal> ft(xpp::size_product(iap->ndim, iap->ncol, xpp::size_sum(iap->ntst, 1)));

   if (*ifst==1){
    /* the arrays are sized (and zeroed) in one place, SolvbvWork::resize */
    iap->lib->solvbv.resize(iap->ndim, iap->ncol, iap->ntst, iap->nbc, iap->nint, iap->lib->parameter_count);
  }

  ndim = iap->ndim;
  ips = iap->ips;
  ntst = iap->ntst;
  ncol = iap->ncol;
  nbc = iap->nbc;
  nint = iap->nint;
  iid = iap->iid;
  nfpr = iap->nfpr;
  nrc = nbc + nint + 1;
  nrow = ndim * ncol;
  nclm = nrow + ndim;
  
  if (ntst < 1) {
    xpp::auto_fail(xpp::format("Ntst is {}, less than 1; set Ntst (Numerics) to 1 or more", ntst));
  }
  
  if (*ifst == 1) {
    setubv(ndim, ips, ntst, ncol, nbc, nint, nfpr, nrc, nrow, nclm,
	   funi, bcni, icni, *ndxloc, iap, rap, par, icp, 
	   *rds, iap->lib->solvbv.a_matrix(nclm, nrow, ntst), iap->lib->solvbv.b_matrix(nfpr, nrow, ntst), iap->lib->solvbv.c_matrix(nclm, nrc, ntst), iap->lib->solvbv.d_matrix(nfpr, nrc), ft.data(), fc, rlcur, 
	   rlold, rldot, ups, uoldps, udotps, upoldp, dups, 
	   dtm, thl, thu, p0, p1);
  } else {
    setrhs(&ndim, &ips, &ntst, &ncol, &nbc, &nint, &
	   nfpr, &nrc, &nrow, &nclm, funi, bcni, icni,
	   ndxloc, iap, rap, par, icp, rds, ft.data(), fc, rlcur, 
	   rlold, rldot, ups, uoldps, udotps, upoldp, dups, dtm, thl, 
	   thu, p0, p1);
  }

  if (iap->lib->setubv_stop && xpp::job::cancelled()) return 0; /* xppautX: cancel */

  brbd(*iap->lib, iap->lib->fp9, ft.data(), fc, p0, p1,
       ifst, &iid, nllv, &det, &ndim, &ntst, &nbc, &nrow, &nclm, &nfpr, &nrc);
  
  faft(ft.data(), fa, &ntst, &nrow, ndxloc);

  rap->det = det;
  return 0;
} /* solvbv_ */



/*     ---------- ---- */
/* Subroutine */ int 
faft(doublereal *ff, doublereal *fa, integer *ntst, integer *nrow, integer *ndxloc)
{
  /* System generated locals */
  integer fa_dim1, ff_dim1;

    /* Local variables */
  integer i, j;

  /* Parameter adjustments */
  ff_dim1 = *nrow;
  fa_dim1 = *ndxloc;
    
  for (i = 0; i < *ntst; ++i) {
    for (j = 0; j < *nrow; ++j) {
      ARRAY2D(fa, i, j) = ARRAY2D(ff, j, i);
    }
  }

  return 0;
} /* faft_ */



/*     ---------- ------ */
/* Subroutine */ int 
setrhs(integer *ndim, xpp::AutoProblem *ips, integer *na, integer *ncol, integer *nbc, integer *nint, integer *ncb, integer *nrc, integer *nra, integer *nca, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), integer *ndxloc, iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *rds, doublereal *fa, doublereal *fc, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *dups, doublereal *dtm, doublereal *thl, doublereal *thu, doublereal *p0, doublereal *p1)
{
  /* System generated locals */
  integer ups_dim1, dups_dim1, uoldps_dim1, 
    udotps_dim1, upoldp_dim1, 
    fa_dim1, wt_dim1, wp_dim1, wploc_dim1;

  integer i, j, k, l, m;
  integer i1, j1, k1, l1;

  doublereal rlsum;
  integer ib, ic, jj;
  integer ic1;

  integer jp1;
  integer ncp1;
  doublereal dt,ddt;

  std::vector<doublereal> dicd(xpp::size_product(iap->nint, xpp::size_sum(iap->ndim, iap->lib->parameter_count)));
  std::vector<doublereal> ficd(iap->nint);
  std::vector<doublereal> dfdp(xpp::size_product(iap->ndim, iap->lib->parameter_count));
  std::vector<doublereal> dfdu(xpp::size_product(iap->ndim, iap->ndim));
  std::vector<doublereal> uold(iap->ndim);
  std::vector<doublereal> f(iap->ndim);
  std::vector<doublereal> u(iap->ndim);
  std::vector<doublereal> wploc(xpp::size_product(iap->ncol, xpp::size_sum(iap->ncol, 1)));
  std::vector<doublereal> wi(xpp::size_sum(iap->ncol, 1));
  std::vector<doublereal> wp(xpp::size_product(iap->ncol, xpp::size_sum(iap->ncol, 1)));
  std::vector<doublereal> wt(xpp::size_product(iap->ncol, xpp::size_sum(iap->ncol, 1)));
  std::vector<doublereal> dbc(xpp::size_product(iap->nbc, xpp::size_sum(xpp::size_product(2, iap->ndim), iap->lib->parameter_count)));
  std::vector<doublereal> fbc(iap->nbc);
  std::vector<doublereal> uic(iap->ndim);
  std::vector<doublereal> uio(iap->ndim);
  std::vector<doublereal> prm(iap->lib->parameter_count);
  std::vector<doublereal> uid(iap->ndim);
  std::vector<doublereal> uip(iap->ndim);
  std::vector<doublereal> ubc0(iap->ndim);
  std::vector<doublereal> ubc1(iap->ndim);

  /* Parameter adjustments */
  /*--par;*/
  /*--icp;*/
  /*--fc;*/
  /*--rlcur;*/
  /*--rlold;*/
  /*--rldot;*/
  /*--dtm;*/
  /*--thl;*/
  /*--thu;*/
  fa_dim1 = *nra;
  dups_dim1 = *ndxloc;
  upoldp_dim1 = *ndxloc;
  udotps_dim1 = *ndxloc;
  uoldps_dim1 = *ndxloc;
  ups_dim1 = *ndxloc;
  wt_dim1 = iap->ncol+1;
  wp_dim1 = iap->ncol+1;
  wploc_dim1 = iap->ncol+1;

  wint(*ncol + 1, wi.data());
  genwts(*ncol, iap->ncol + 1, wt.data(), wp.data());
  /* Initialize to zero. */
  for (i = 0; i < *nrc; ++i) {
    fc[i] = 0.;
  }

  /* Set constants. */
  ncp1 = *ncol + 1;
  for (i = 0; i < *ncb; ++i) {
    par[icp[i]] = rlcur[i];
  }

  /* Generate FA : */

  for (jj = 0; jj < *na; ++jj) {
    j = jj;
    jp1 = j + 1;
    dt = dtm[j];
    ddt = 1. / dt;
    for (ic = 0; ic < *ncol; ++ic) {
      for (ib = 0; ib < ncp1; ++ib) {
	ARRAY2D(wploc, ib, ic) = ddt * ARRAY2D(wp, ib, ic);
      }
    }
    for (ic = 0; ic < *ncol; ++ic) {
      for (k = 0; k < *ndim; ++k) {
	u[k] = ARRAY2D(wt, *ncol, ic) * ARRAY2D(ups, jp1, k);
	uold[k] = ARRAY2D(wt, *ncol, ic) * ARRAY2D(uoldps, jp1, k);
	for (l = 0; l < *ncol; ++l) {
	  l1 = l * *ndim + k;
	  u[k] += ARRAY2D(wt, l, ic) * ARRAY2D(ups, j, l1);
	  uold[k] += ARRAY2D(wt, l, ic) * ARRAY2D(uoldps, j, l1);
	}
      }
      /*     ** Time evolution computations (parabolic systems) */
      if (xpp::problem_is_parabolic_evolution(*ips)) {
	rap->tivp = rlold[0];
      }
      for (i = 0; i < iap->lib->parameter_count; ++i) {
	prm[i] = par[i];
      }
      /* The residual reads only f: no Jacobian (docs/xppaut-findings.md #39). */
      (*funi)(iap, rap, *ndim, u.data(), uold.data(), icp, prm.data(), 0, f.data(), 
	      dfdu.data(), dfdp.data());
      ic1 = ic * *ndim;
      for (i = 0; i < *ndim; ++i) {
	ARRAY2D(fa, ic1 + i, jj) = f[i] - ARRAY2D(wploc, *ncol, ic) * ARRAY2D(ups, jp1, i);
	for (k = 0; k < *ncol; ++k) {
	  k1 = k * *ndim + i;
	  ARRAY2D(fa, ic1 + i, jj) -= ARRAY2D(wploc, k, ic) * ARRAY2D(ups, j, k1);
	}
      }
      /* L1: */
    }
    /* L2: */
  }

  /*     Generate FC : */

/*     Boundary conditions : */

  if (*nbc > 0) {
    for (i = 0; i < *ndim; ++i) {
      ubc0[i] = ARRAY2D(ups, 0, i);
      ubc1[i] = ARRAY2D(ups, *na, i);
    }
    (*bcni)(iap, rap, *ndim, par, icp, *nbc, ubc0.data(), ubc1.data(), 
	    fbc.data(), 2, dbc.data());
    for (i = 0; i < *nbc; ++i) {
      fc[i] = -fbc[i];
    }
    /*       Save difference : */
    for (j = 0; j < *na + 1; ++j) {
      for (i = 0; i < *nra; ++i) {
	ARRAY2D(dups, j, i) = ARRAY2D(ups, j, i) - ARRAY2D(uoldps, j, i);
      }
    }
  }

  /*     Integral constraints : */
  if (*nint > 0) {
    for (jj = 0; jj < *na; ++jj) {
      j = jj;
      jp1 = j + 1;
      for (k = 0; k < ncp1; ++k) {
	for (i = 0; i < *ndim; ++i) {
	  i1 = k * *ndim + i;
	  j1 = j;
	  if (k + 1 == ncp1) {
	    i1 = i;
	  }
	  if (k + 1 == ncp1) {
	    j1 = jp1;
	  }
	  uic[i] = ARRAY2D(ups, j1, i1);
	  uio[i] = ARRAY2D(uoldps, j1, i1);
	  uid[i] = ARRAY2D(udotps, j1, i1);
	  uip[i] = ARRAY2D(upoldp, j1, i1);
	}
	(*icni)(iap, rap, *ndim, par, icp, *nint, uic.data(), 
		uio.data(), uid.data(), uip.data(), ficd.data(), 2, dicd.data());
	for (m = 0; m < *nint; ++m) {
	  fc[*nbc + m] -= dtm[j] * wi[k] * ficd[m];
	}
      }
    }
  }

  /*     Pseudo-arclength equation : */
  rlsum = 0.;
  for (i = 0; i < *ncb; ++i) {
    rlsum += thl[icp[i]] * (rlcur[i] - rlold[i]) * rldot[i];
  }

  fc[-1 + *nrc] = *rds - rinpr(iap, ndim, ndxloc, udotps, 
			       dups, dtm, thu) - rlsum;

  return 0;
} /* setrhs_ */

/*     ---------- ---- */
/* Subroutine */ int 
brbd(AutoLib &lib, FILE *fp9, doublereal *fa, doublereal *fc, doublereal *p0, doublereal *p1, integer *ifst, integer *idb, integer *nllv, doublereal *det, integer *nov, integer *na, integer *nbc, integer *nra, integer *nca, integer *ncb, integer *nrc)
{
  /* the matrices and index arrays solvbv sized and kept in lib.solvbv */
  SolvbvWork &work = lib.solvbv;
  doublereal *a = work.a.data(), *b = work.b.data(), *c = work.c.data(), *d = work.d.data();
  doublereal *a1 = work.a1.data(), *a2 = work.a2.data(), *bb = work.bb.data(), *cc = work.cc.data();
  doublereal *faa = work.faa.data(), *s1 = work.s1.data(), *s2 = work.s2.data();
  integer *ipr = work.ipr.data(), *icf1 = work.icf1.data(), *icf2 = work.icf2.data();
  integer *irf = work.irf.data(), *icf = work.icf.data();

  std::vector<doublereal> e(xpp::size_product(xpp::size_sum(*nov, *nrc), xpp::size_sum(*nov, *nrc)));
  std::vector<doublereal> fcc(xpp::size_sum(xpp::size_sum(*nov, *nrc), xpp::size_product(2, *nov, *nov), 1));

  std::vector<doublereal> sol1(xpp::size_product(*nov, xpp::size_sum(*na, 1)));
  std::vector<doublereal> sol2(xpp::size_product(*nov, xpp::size_sum(*na, 1)));
  std::vector<doublereal> sol3(xpp::size_product(*nov, xpp::size_sum(*na, 1)));
  
  /* Local */

  /* Parameter adjustments */
  /*--icf;*/
  /*--irf;*/
  /*--icf2;*/
  /*--icf1;*/
  /*--ipr;*/
  /*--s2;*/
  /*--s1;*/
  /*--faa;*/
  /*--cc;*/
  /*--bb;*/
  /*--a2;*/
  /*--a1;*/
  /*--p1;*/
  /*--p0;*/
  /*--fc;*/
  /*--fa;*/
  /*--d;*/
  /*--c;*/
  /*--b;*/
  /*--a;*/

  if (*idb > 4) {
    print1(fp9, nov, na, nra, nca, ncb, nrc, a, b, c, d, &
    	   fa[0], fc);
  }
  if (*ifst == 1) {
    conpar(lib, nov, na, nra, nca, a, ncb, b, nbc, nrc, c, d, irf, icf);
    copycp(na, nov, nra, nca, a, ncb, b, nrc, c, 
	   a1, a2, bb, cc, irf);
  }

  if (*nllv == 0) {
    conrhs(nov, na, nra, nca, a, nbc, nrc, c, fa, fc, 
	   irf, icf);
    cpyrhs(na, nov, nra, faa, fa, irf);
  } else {
    setzero(fa, fc, na, nra, nrc);
    cpyrhs(na, nov, nra, faa, fa, irf);
  }

  if (*ifst == 1) {
    reduce(a1, a2, bb, cc, d, na, 
	   nov, ncb, nrc, s1, s2, icf1, icf2, 
	   ipr, nbc);
  }

  if (*nllv == 0) {
    redrhs(a1, a2, cc, faa, fc, na, 
	   nov, ncb, nrc, icf1, icf2, ipr, nbc);
  }

  dimrge(fp9, e.data(), cc, d, fc, ifst, na, 
	 nrc, nov, ncb, idb, nllv, fcc.data(), p0, p1, det, s1, a2,
	 faa, bb);

  bcksub(s1, s2, a2, bb, faa, fc, 
	 fcc.data(), sol1.data(), sol2.data(), sol3.data(), na, nov, ncb, icf2);

  infpar(a, b, fa, sol1.data(), sol2.data(), fc, na, nov, nra, 
	 nca, ncb, irf, icf);

  return 0;
} /* brbd_ */

/*     ---------- ------- */
/* Subroutine */ int 
setzero(doublereal *fa, doublereal *fc, integer *na, integer *nra, integer *nrc)
{
  /* System generated locals */
  integer fa_dim1;

    /* Local variables */
  integer i, j;

    /* Parameter adjustments */
    /*--fc;*/
  fa_dim1 = *nra;
    
  for (i = 0; i < *na; ++i) {
    for (j = 0; j < *nra; ++j) {
      ARRAY2D(fa, j, i) = 0.;
    }
  }

  for (i = 0; i < *nrc; ++i) {
    fc[i] = 0.;
  }

  return 0;
} /* setzero_ */

/*     ---------- ------ */
/* Subroutine */ int 
conrhs(integer *nov, integer *na, integer *nra, integer *nca, doublereal *a, integer *nbc, integer *nrc, doublereal *c, doublereal *fa, doublereal *fc, integer *irf, integer *icf)
{
  /* System generated locals */
  integer icf_dim1, irf_dim1, a_dim1, a_dim2, c_dim1, c_dim2, fa_dim1;

    /* Local variables */
  integer i, icfic, irfir, m2, ic, ir, irfirp, ir1, nex,
    irp;

    /* Parameter adjustments */
    /*--fc;*/
  irf_dim1 = *nra;
  fa_dim1 = *nra;
  icf_dim1 = *nca;
  a_dim1 = *nca;
  a_dim2 = *nra;
  c_dim1 = *nca;
  c_dim2 = *nrc;
    
  nex = *nca - (*nov * 2);
  if (nex == 0) {
    return 0;
  }

  /* Condensation of right hand side. */

  m2 = *nov + nex;

  for (i = 0; i < *na; ++i) {
    for (ic = *nov; ic < m2; ++ic) {
      ir1 = ic - *nov + 1;
      irp = ir1 - 1;
      irfirp = ARRAY2D(irf, irp, i);
      icfic = ARRAY2D(icf, ic, i);
      for (ir = ir1; ir < *nra; ++ir) {
	irfir = ARRAY2D(irf, ir, i);
	if (ARRAY3D(a, (icfic - 1), (irfir - 1), i) != 0.) {
	  ARRAY2D(fa, (irfir - 1), i) -= ARRAY3D(a, (icfic - 1), (irfir - 1), i) * ARRAY2D(fa, (irfirp - 1), i);
	}
      }
      for (ir = *nbc; ir < *nrc; ++ir) {
	if (ARRAY3D(c, (icfic - 1), ir, i) != 0.) {
	  fc[ir] -= ARRAY3D(c, (icfic - 1), ir, i) * ARRAY2D(fa, (irfirp - 1), i);
	}
      }
    }
  }

  return 0;
} /* conrhs_ */

/*     ---------- ------ */
/* Subroutine */ int 
copycp(integer *na, integer *nov, integer *nra, integer *nca, doublereal *a, integer *ncb, doublereal *b, integer *nrc, doublereal *c, doublereal *a1, doublereal *a2, doublereal *bb, doublereal *cc, integer *irf)
{
  /* System generated locals */
  integer irf_dim1, a_dim1, a_dim2, b_dim1, b_dim2, c_dim1, c_dim2, a1_dim1, a1_dim2,
    a2_dim1, a2_dim2, bb_dim1, bb_dim2, cc_dim1,cc_dim2;

  /* Local variables */
  integer i, irfir, ic, ir, ic1, nap1;

/* Local */

/* Copies the condensed sytem generated by CONPAR into workspace. */

    /* Parameter adjustments */
  a2_dim1 = *nov;
  a2_dim2 = *nov;
  a1_dim1 = *nov;
  a1_dim2 = *nov;
  irf_dim1 = *nra;
  a_dim1 = *nca;
  a_dim2 = *nra;
  bb_dim1 = *nov;
  bb_dim2 = *ncb;
  b_dim1 = *ncb;
  b_dim2 = *nra;
  cc_dim1 = *nov;
  cc_dim2 = *nrc;
  c_dim1 = *nca;
  c_dim2 = *nrc;
    
  nap1 = *na + 1;
  for (i = 0; i < *na; ++i) {
    for (ir = 0; ir < *nov; ++ir) {
      irfir = ARRAY2D(irf, *nra - *nov + ir, i);
      for (ic = 0; ic < *nov; ++ic) {
	ic1 = *nca - *nov + ic;
	ARRAY3D(a1,  ir, ic, i) = ARRAY3D(a, ic, (irfir - 1), i);
	ARRAY3D(a2, ir, ic, i) = ARRAY3D(a, ic1, (irfir - 1), i);
      }
      for (ic = 0; ic < *ncb; ++ic) {
	ARRAY3D(bb, ir, ic, i) = ARRAY3D(b, ic, (irfir - 1), i);
      }
    }
  }

  for (i = 0; i < nap1; ++i) {
    for (ir = 0; ir < *nrc; ++ir) {
      for (ic = 0; ic < *nov; ++ic) {
	if (i + 1 == 1) {
	  ARRAY3D(cc, ic, ir, i) = ARRAY3D(c, ic, ir, i);
	} else if (i + 1 == nap1) {
	  ARRAY3D(cc, ic, ir, i) = ARRAY3D(c, *nra + ic, ir, (i - 1));
	} else {
	  ARRAY3D(cc, ic, ir, i) = ARRAY3D(c, ic, ir, i) + ARRAY3D(c, *nra + ic, ir, (i - 1));
	}
      }
    }
  }

  return 0;
} /* copycp_ */

/*     ---------- ------ */
/* Subroutine */ int 
cpyrhs(integer *na, integer *nov, integer *nra, doublereal *faa, doublereal *fa, integer *irf)
{
  /* System generated locals */
  integer irf_dim1, fa_dim1, faa_dim1;

  /* Local variables */
  integer i, irfir, ir;

/*     **Copy the RHS */
    /* Parameter adjustments */
  faa_dim1 = *nov;
  irf_dim1 = *nra;
  fa_dim1 = *nra;

  for (i = 0; i < *na; ++i) {
    for (ir = 0; ir < *nov; ++ir) {
      irfir = ARRAY2D(irf, *nra - *nov + ir, i);
      ARRAY2D(faa, ir, i) = ARRAY2D(fa, (irfir - 1), i);
    }
  }

  return 0;
} /* cpyrhs_ */

/*     ---------- ------ */
/* Subroutine */ int 
reduce(doublereal *a1, doublereal *a2, doublereal *bb, doublereal *cc, doublereal *dd, integer *na, integer *nov, integer *ncb, integer *nrc, doublereal *s1, doublereal *s2, integer *icf1, integer *icf2, integer *ipr, integer *nbc)
{
  /* System generated locals */
  integer icf1_dim1, icf2_dim1, a1_dim1, a1_dim2, a2_dim1, a2_dim2, 
    s1_dim1, s1_dim2, s2_dim1, s2_dim2, 
    bb_dim1, bb_dim2, cc_dim1, cc_dim2, 
    dd_dim1, ipr_dim1;

  /* Local variables */
  doublereal zero, tpiv;
  integer nbcp1, ipiv1, jpiv1, ipiv2, jpiv2, i, l;
  integer i1, i2, i3, k1, k2, ic, ir;
  doublereal rm;
  doublereal tmp;
  integer nam1, icp1;
  doublereal piv1, piv2;
  int itmp;

  /* Parameter adjustments */
  ipr_dim1 = *nov;
  icf2_dim1 = *nov;
  icf1_dim1 = *nov;
  s2_dim1 = *nov;
  s2_dim2 = *nov;
  s1_dim1 = *nov;
  s1_dim2 = *nov;
  a2_dim1 = *nov;
  a2_dim2 = *nov;
  a1_dim1 = *nov;
  a1_dim2 = *nov;
  dd_dim1 = *ncb;
  bb_dim1 = *nov;
  bb_dim2 = *ncb;
  cc_dim1 = *nov;
  cc_dim2 = *nrc;
    
  zero = 0.;
  nbcp1 = *nbc + 1;
  nam1 = *na - 1;

  /* Initialization */

  for (i = 0; i < *na; ++i) {
    for (k1 = 0; k1 < *nov; ++k1) {
      ARRAY2D(icf1, k1, i) = k1 + 1;
      ARRAY2D(icf2, k1, i) = k1 + 1;
      ARRAY2D(ipr, k1, i) = k1 + 1;
      for (k2 = 0; k2 < *nov; ++k2) {
	ARRAY3D(s2, k1, k2, i) = 0.;
	ARRAY3D(s1, k1, k2, i) = 0.;
      }
    }
  }

  for (ir = 0; ir < *nov; ++ir) {
    for (ic = 0; ic < *nov; ++ic) {
      ARRAY2D(s1, ir, ic) = ARRAY2D(a1, ir, ic);
    }
  }

  /* The reduction process is done concurrently */
  for (i1 = 0; i1 < nam1; ++i1) {

    i2 = i1 + 1;
    i3 = i2 + 1;

    for (ic = 0; ic < *nov; ++ic) {
      icp1 = ic + 1;

      /* Complete pivoting; rows are swapped physically, columns swap in
	 dices */
      piv1 = zero;
      ipiv1 = ic + 1;
      jpiv1 = ic + 1;
      for (k1 = ic; k1 < *nov; ++k1) {
	for (k2 = ic; k2 < *nov; ++k2) {
	  tpiv = ARRAY3D(a2, k1, ARRAY2D(icf2, k2, i1) - 1, i1);
	  if (tpiv < zero) {
	    tpiv = -tpiv;
	  }
	  if (piv1 < tpiv) {
	    piv1 = tpiv;
	    ipiv1 = k1 + 1;
	    jpiv1 = k2 + 1;
	  }
	}
      }

      piv2 = zero;
      ipiv2 = 1;
      jpiv2 = ic + 1;
      for (k1 = 0; k1 < *nov; ++k1) {
	for (k2 = ic; k2 < *nov; ++k2) {
	  tpiv = ARRAY3D(a1, k1, ARRAY2D(icf1, k2, i2) - 1, i2);
	  if (tpiv < zero) {
	    tpiv = -tpiv;
	  }
	  if (piv2 < tpiv) {
	    piv2 = tpiv;
	    ipiv2 = k1 + 1;
	    jpiv2 = k2 + 1;
	  }
	}
      }

      if (piv1 >= piv2) {
	ARRAY2D(ipr, ic, i1) = ipiv1;
	itmp = ARRAY2D(icf2, ic, i1);
	ARRAY2D(icf2, ic, i1) = ARRAY2D(icf2, (jpiv1 - 1), i1);
	ARRAY2D(icf2, (jpiv1 - 1), i1) = itmp;
	itmp = ARRAY2D(icf1, ic, i2);
	ARRAY2D(icf1, ic, i2) = ARRAY2D(icf1, (jpiv1 - 1), i2);
	ARRAY2D(icf1, (jpiv1 - 1), i2) = itmp;
	/* Swapping */
	for (l = 0; l < *nov; ++l) {
	  tmp = ARRAY3D(s1, ic, l, i1);
	  ARRAY3D(s1, ic, l, i1) = ARRAY3D(s1, (ipiv1 - 1), l, i1);
	  ARRAY3D(s1, (ipiv1 - 1), l, i1) = tmp;
	  if (l >= ic) {
	    tmp = ARRAY3D(a2, ic, ARRAY2D(icf2, l, i1) - 1, i1);
	    ARRAY3D(a2, ic, ARRAY2D(icf2, l, i1) - 1, i1) = 
	      ARRAY3D(a2, (ipiv1 - 1), ARRAY2D(icf2, l, i1) - 1, i1);
	    ARRAY3D(a2, (ipiv1 - 1), ARRAY2D(icf2, l, i1) - 1, i1) = tmp;
	  }
	  tmp = ARRAY3D(s2, ic, l, i1);
	  ARRAY3D(s2, ic, l, i1) = ARRAY3D(s2, (ipiv1 - 1), l, i1);
	  ARRAY3D(s2, (ipiv1 - 1), l, i1) = tmp;
	}

	for (l = 0; l < *ncb; ++l) {
	  tmp = ARRAY3D(bb, ic, l, i1);
	  ARRAY3D(bb, ic, l, i1) = ARRAY3D(bb, (ipiv1 - 1), l, i1);
	  ARRAY3D(bb, (ipiv1 - 1), l, i1) = tmp;
	}
      } else {
	ARRAY2D(ipr, ic, i1) = *nov + ipiv2;
	itmp = ARRAY2D(icf2, ic, i1);
	ARRAY2D(icf2, ic, i1) = ARRAY2D(icf2, (jpiv2 - 1), i1);
	ARRAY2D(icf2, (jpiv2 - 1), i1) = itmp;
	itmp = ARRAY2D(icf1, ic, i2);
	ARRAY2D(icf1, ic, i2) = ARRAY2D(icf1, (jpiv2 - 1), i2);
	ARRAY2D(icf1, (jpiv2 - 1), i2) = itmp;
	/* Swapping */
	for (l = 0; l < *nov; ++l) {
	  if (l >= ic) {
	    tmp = ARRAY3D(a2, ic , ARRAY2D(icf2, l, i1) - 1, i1);
	    ARRAY3D(a2, ic , ARRAY2D(icf2, l, i1) - 1, i1) = 
	      ARRAY3D(a1, (ipiv2 - 1), ARRAY2D(icf2, l, i1) - 1, i2);
	    ARRAY3D(a1, (ipiv2 - 1), ARRAY2D(icf2, l, i1) - 1, i2) = tmp;
	  }
	  tmp = ARRAY3D(s2, ic, l, i1);
	  ARRAY3D(s2, ic, l, i1) = ARRAY3D(a2, (ipiv2 - 1), l, i2);
	  ARRAY3D(a2, (ipiv2 - 1), l, i2) = tmp;
	  tmp = ARRAY3D(s1, ic, l, i1);
	  ARRAY3D(s1, ic, l, i1) = ARRAY3D(s1, (ipiv2 - 1), l, i2);
	  ARRAY3D(s1, (ipiv2 - 1), l, i2) = tmp;
	}
	for (l = 0; l < *ncb; ++l) {
	  tmp = ARRAY3D(bb, ic, l, i1);
	  ARRAY3D(bb, ic, l, i1) = ARRAY3D(bb, (ipiv2 - 1), l, i2);
	  ARRAY3D(bb, (ipiv2 - 1), l, i2) = tmp;
	}
      }

      /* End of pivoting; Elimination starts here */

      for (ir = icp1; ir < *nov; ++ir) {
	rm = ARRAY3D(a2, ir, ARRAY2D(icf2, ic, i1) - 1, i1) / 
	  ARRAY3D(a2, ic, ARRAY2D(icf2, ic, i1) - 1, i1);
	ARRAY3D(a2, ir, ARRAY2D(icf2, ic, i1) - 1, i1) = rm;

	if (rm != 0.) {
	  for (l = icp1; l < *nov; ++l) {
	    ARRAY3D(a2, ir, ARRAY2D(icf2, l, i1) - 1, i1) -= 
	      rm * ARRAY3D(a2, ic, ARRAY2D(icf2, l, i1) - 1, i1);
	  }

	  for (l = 0; l < *nov; ++l) {
	    ARRAY3D(s1, ir, l, i1) -= rm * ARRAY3D(s1, ic, l, i1);
	    ARRAY3D(s2, ir, l, i1) -= rm * ARRAY3D(s2, ic, l, i1);
	  }

	  for (l = 0; l < *ncb; ++l) {
	    ARRAY3D(bb, ir, l, i1) -= rm * ARRAY3D(bb, ic, l, i1);
	  }
	}
      }

      for (ir = 0; ir < *nov; ++ir) {
	rm = ARRAY3D(a1, ir, ARRAY2D(icf1, ic, i2) - 1, i2) / 
	  ARRAY3D(a2, ic, ARRAY2D(icf2, ic, i1) - 1, i1);
	ARRAY3D(a1, ir, ARRAY2D(icf1, ic, i2) - 1, i2) = rm;

	if (rm != 0.) {
	  for (l = icp1; l < *nov; ++l) {
	    ARRAY3D(a1, ir, ARRAY2D(icf1, l, i2) - 1, i2) -= 
	      rm * ARRAY3D(a2, ic, ARRAY2D(icf2, l, i1) - 1, i1);
	  }
	  for (l = 0; l < *nov; ++l) {
	    ARRAY3D(s1, ir, l, i2) -= rm * ARRAY3D(s1, ic, l, i1);
	    ARRAY3D(a2, ir, l, i2) -= rm * ARRAY3D(s2, ic, l, i1);
	  }
	  for (l = 0; l < *ncb; ++l) {
	    ARRAY3D(bb, ir, l, i2) -= rm * ARRAY3D(bb, ic, l, i1);
	  }
	}
      }

      for (ir = nbcp1 - 1; ir < *nrc; ++ir) {
	rm = ARRAY3D(cc, ARRAY2D(icf2, ic, i1) - 1, ir, i2) / 
	  ARRAY3D(a2, ic, ARRAY2D(icf2, ic, i1) - 1, i1);
	ARRAY3D(cc, ARRAY2D(icf2, ic, i1) - 1, ir, i2) = rm;

	if (rm != 0.) {
	  for (l = icp1; l < *nov; ++l) {
	    ARRAY3D(cc, ARRAY2D(icf2, l, i1) - 1, ir, i2) -= 
	      rm * ARRAY3D(a2, ic, ARRAY2D(icf2, l, i1) - 1, i1);
	  }
	  for (l = 0; l < *nov; ++l) {
	    ARRAY3D(cc, l, ir, 0) -= rm * ARRAY3D(s1, ic, l, i1);
	    ARRAY3D(cc, l, ir, i3) -= rm * ARRAY3D(s2, ic, l, i1);
	  }
	  for (l = 0; l < *ncb; ++l) {
	    ARRAY2D(dd, l, ir) -= rm * ARRAY3D(bb, ic, l, i1);
	  }
	}
      }

      /* L2: */
    }
    /* L3: */
  }

  /* Initialization */
  for (i = 0; i < *nov; ++i) {
    ARRAY2D(icf2, i, (*na - 1)) = i + 1;
  }

  return 0;
} /* reduce_ */

/*     ---------- ------ */
/* Subroutine */ int 
redrhs(doublereal *a1, doublereal *a2, doublereal *cc, doublereal *faa, doublereal *fc, integer *na, integer *nov, integer *ncb, integer *nrc, integer *icf1, integer *icf2, integer *ipr, integer *nbc)
{
  /* System generated locals */
  integer icf1_dim1, icf2_dim1, 
    a1_dim1, a1_dim2, a2_dim1, a2_dim2, 
    cc_dim1, cc_dim2, faa_dim1, 
    ipr_dim1;

  /* Local variables */
  integer nbcp1, ipiv1;

  integer i1, i2, l1, ic, ir;
  doublereal rm;
  doublereal tmp;
  integer nam1, icp1;

  /* Parameter adjustments */
  /*--fc;*/
  ipr_dim1 = *nov;
  icf2_dim1 = *nov;
  icf1_dim1 = *nov;
  faa_dim1 = *nov;
  a2_dim1 = *nov;
  a2_dim2 = *nov;
  a1_dim1 = *nov;
  a1_dim2 = *nov;
  cc_dim1 = *nov;
  cc_dim2 = *nrc;

  nbcp1 = *nbc + 1;
  nam1 = *na - 1;

  /* Reduce concurrently in each node */
  for (i1 = 0; i1 < nam1; ++i1) {
    i2 = i1 + 1;
    for (ic = 0; ic < *nov; ++ic) {
      icp1 = ic + 1;
      ipiv1 = ARRAY2D(ipr, ic, i1);
      if (ipiv1 <= *nov) {
	tmp = ARRAY2D(faa, ic, i1);
	ARRAY2D(faa, ic, i1) = ARRAY2D(faa, (ipiv1 - 1), i1);
	ARRAY2D(faa, (ipiv1 - 1), i1) = tmp;
      } else {
	l1 = (ipiv1 - *nov) - 1;
	tmp = ARRAY2D(faa, ic, i1);
	ARRAY2D(faa, ic, i1) = ARRAY2D(faa, l1, i2);
	ARRAY2D(faa, l1, i2) = tmp;
      }
      for (ir = icp1; ir < *nov; ++ir) {
	l1 = ARRAY2D(icf2, ic, i1) - 1;
	rm = ARRAY3D(a2, ir, l1, i1);
	ARRAY2D(faa, ir, i1) -= rm * ARRAY2D(faa, ic, i1);
      }
      for (ir = 0; ir < *nov; ++ir) {
	l1 = ARRAY2D(icf1, ic, i2) - 1;
	rm = ARRAY3D(a1, ir, l1, i2);
	ARRAY2D(faa, ir, i2) -= rm * ARRAY2D(faa, ic, i1);
      }
      for (ir = nbcp1 - 1; ir < *nrc; ++ir) {
	l1 = ARRAY2D(icf2, ic, i1) - 1;
	rm = ARRAY3D(cc, l1, ir, i2);
	fc[ir] -= rm * ARRAY2D(faa, ic, i1);
      }
    }
  }

  return 0;
} /* redrhs_ */

/*     ---------- ------ */
/* Subroutine */ int 
dimrge(FILE *fp9, doublereal *e, doublereal *cc, doublereal *d, doublereal *fc, integer *ifst, integer *na, integer *nrc, integer *nov, integer *ncb, integer *idb, integer *nllv, doublereal *fcc, doublereal *p0, doublereal *p1, doublereal *det, doublereal *s, doublereal *a2, doublereal *faa, doublereal *bb)
{

  /* System generated locals */
  integer e_dim1, cc_dim1, cc_dim2, d_dim1, 
    p0_dim1, p1_dim1, s_dim1, s_dim2, 
    faa_dim1, a2_dim1, a2_dim2, bb_dim1, 
    bb_dim2;

  /* Local variables */

  integer i, j, k;

  integer novpi, novpj, k1, k2;

  integer novpj2, kc, kr, ncrloc, nap1;

  std::vector<double> xe(xpp::size_sum(*nov, *nrc));

  /* Parameter adjustments */
  /*--fc;*/
  /*--xe;*/
  /*--fcc;*/
  faa_dim1 = *nov;
  a2_dim1 = *nov;
  a2_dim2 = *nov;
  s_dim1 = *nov;
  s_dim2 = *nov;
  p1_dim1 = *nov;
  p0_dim1 = *nov;
  cc_dim1 = *nov;
  cc_dim2 = *nrc;
  e_dim1 = *nov + *nrc;
  bb_dim1 = *nov;
  bb_dim2 = *ncb;
  d_dim1 = *ncb;
    
  nap1 = *na + 1;
  ncrloc = *nrc + *nov;

  /* Copy */
  for (i = 0; i < *nov; ++i) {
    for (j = 0; j < *nov; ++j) {
      novpj = *nov + j;
      ARRAY2D(e, i, j) = ARRAY3D(s, i, j, (*na - 1));
      ARRAY2D(p0, i, j) = ARRAY3D(s, i, j, (*na - 1));
      ARRAY2D(e, i, novpj) = ARRAY3D(a2, i, j, (*na - 1));
      ARRAY2D(p1, i, j) = ARRAY3D(a2, i, j, (*na - 1));
    }
    for (j = 0; j < *ncb; ++j) {
      novpj2 = (*nov * 2) + j;
      ARRAY2D(e, i, novpj2) = ARRAY3D(bb, i, j, (*na - 1));
    }
  }

  for (i = 0; i < *nrc; ++i) {
    novpi = *nov + i;
    for (j = 0; j < *nov; ++j) {
      novpj = *nov + j;
      ARRAY2D(e, novpi, j) = ARRAY3D(cc, j, i, 0);
      ARRAY2D(e, novpi, novpj) = ARRAY3D(cc, j, i, (nap1 - 1));
    }
    for (j = 0; j < *ncb; ++j) {
      novpj2 = (*nov * 2) + j;
      ARRAY2D(e, novpi, novpj2) = ARRAY2D(d, j, i);
    }
  }

  for (i = 0; i < *nov; ++i) {
    xe[i] = ARRAY2D(faa, i, (*na - 1));
  }

  for (i = 0; i < *nrc; ++i) {
    novpi = *nov + i;
    xe[novpi] = fc[i];
  }

  if (*idb >= 3) {
    xpp::print(fp9," Residuals of reduced system:\n");	
	  
    xpp::print(fp9," ");
    for (i = 0; i < ncrloc; ++i) {
      xpp::print(fp9,"{:11.3E}",xe[i]);	
      if((i+ 1)%10==0)
	xpp::print(fp9,"\n ");
	    
    }
    xpp::print(fp9,"\n");	
  }

  if (*idb >= 4) {

    xpp::print(fp9," Reduced Jacobian matrix:\n");	
	      
    for (i = 0; i < ncrloc; ++i) {
      int total_printed = 0;
      for (j = 0; j < ncrloc; ++j) {
	if((total_printed != 0)&&(total_printed % 10 == 0))
	  xpp::print(fp9,"\n");	
	xpp::print(fp9," {:11.3E}",ARRAY2D(e, i, j));	
	total_printed++;
      }
      xpp::print(fp9,"\n");	
    }
  }

  /* Solve for FCC */
  if (*nllv == 0) {
    xpp::ge(fp9, ncrloc, ncrloc, e, 1, ncrloc, fcc,
       ncrloc, xe.data(), det);
  } else if (*nllv > 0) {
    xpp::nlvc(fp9, ncrloc, ncrloc, *nllv, e, fcc);
  } else {
    for (i = 0; i < ncrloc - 1; ++i) {
      xe[i] = 0.;
    }
    xe[-1 + ncrloc] = 1.;
    xpp::ge(fp9, ncrloc, ncrloc, e, 1, ncrloc, fcc,
       ncrloc, xe.data(), det);
  }
  if (*idb >= 4) {
    xpp::print(fp9," Solution vector:\n");	
	  
    for (i = 0; i < ncrloc; ++i) {
      if((i!=0)&&(i%auto_solution_columns==0))
	xpp::print(fp9,"\n");	
      xpp::print(fp9," {:11.3E}",fcc[i]);	
    }
    xpp::print(fp9,"\n");	
  }

  k1 = ncrloc;
  /* Computing 2nd power */
  k2 = k1 + (*nov) * (*nov);
  for (kr = 0; kr < *nov; ++kr) {
    for (kc = 0; kc < *nov; ++kc) {
      k = kr * *nov + kc;
      fcc[k1 + k] = ARRAY2D(p0, kr, kc);
      fcc[k2 + k] = ARRAY2D(p1, kr, kc);
    }
  }
  /* Computing 2nd power */
  fcc[ncrloc + ((*nov) * (*nov) * 2)] = *det;

  for (i = 0; i < *nrc; ++i) {
    fc[i] = fcc[*nov + i];
  }

  return 0;
} /* dimrge_ */

/*     ---------- ------ */
/* Subroutine */ int 
bcksub(doublereal *s1, doublereal *s2, doublereal *a2, doublereal *bb, doublereal *faa, doublereal *fc, doublereal *fcc, doublereal *sol1, doublereal *sol2, doublereal *sol3, integer *na, integer *nov, integer *ncb, integer *icf2)
{
  /* System generated locals */
  integer icf2_dim1, s1_dim1, s1_dim2, s2_dim1, 
    s2_dim2, a2_dim1, a2_dim2, bb_dim1, bb_dim2,
    sol1_dim1, sol2_dim1, 
    sol3_dim1, faa_dim1;

  /* Local variables */
  integer i, k, l;
  integer l1, l2;
  doublereal sm;
  integer nam1;

  /* Parameter adjustments */
  /*--fc;*/
  /*--fcc;*/
  icf2_dim1 = *nov;
  sol3_dim1 = *nov;
  sol2_dim1 = *nov;
  sol1_dim1 = *nov;
  faa_dim1 = *nov;
  a2_dim1 = *nov;
  a2_dim2 = *nov;
  s2_dim1 = *nov;
  s2_dim2 = *nov;
  s1_dim1 = *nov;
  s1_dim2 = *nov;
  bb_dim1 = *nov;
  bb_dim2 = *ncb;

  for (l = 0; l < *nov; ++l) {
    ARRAY2D(sol1, l, (*na - 1)) = fcc[l];
    ARRAY2D(sol2, l, (*na - 1)) = fc[l];
  }

  if (*na > 1) {
    for (l = 0; l < *nov; ++l) {
      ARRAY2D(sol1, l, (*na - 2)) = ARRAY2D(sol1, l, (*na - 1));
      ARRAY2D(sol3, l, (*na - 2)) = ARRAY2D(sol2, l, (*na - 1));
    }
  }

  /* Backsubstitution process; concurrently in each node. */
  nam1 = *na - 1;
  for (i = nam1 - 1; i >= 0; --i) {
    for (k = *nov - 1; k >= 0; --k) {
      sm = 0.;
      for (l = 0; l < *nov; ++l) {
	sm += ARRAY2D(sol1, l, i) * ARRAY3D(s1, k, l, i);
	sm += ARRAY2D(sol3, l, i) * ARRAY3D(s2, k, l, i);
      }
      for (l = 0; l < *ncb; ++l) {
	sm += fc[*nov + l] * ARRAY3D(bb, k, l, i);
      }
      for (l = k + 1; l < *nov; ++l) {
	l1 = ARRAY2D(icf2, l, i) - 1;
	sm += ARRAY2D(sol2, l1, i) * ARRAY3D(a2, k, l1, i);
      }
      l2 = ARRAY2D(icf2, k, i) - 1;
      ARRAY2D(sol2, l2, i) = (ARRAY2D(faa, k, i) - sm) / ARRAY3D(a2, k, l2, i);
    }
    for (l = 0; l < *nov; ++l) {
      ARRAY2D(sol1, l, (i + 1)) = ARRAY2D(sol2, l, i);
      if (i + 1 > 1) {
	ARRAY2D(sol3, l, (i - 1)) = ARRAY2D(sol2, l, i);
	ARRAY2D(sol1, l, (i - 1)) = ARRAY2D(sol1, l, i);
      }
    }
  }

  return 0;
} /* bcksub_ */

/*     ---------- ------ */
/* Subroutine */ int 
infpar(doublereal *a, doublereal *b, doublereal *fa, doublereal *sol1, doublereal *sol2, doublereal *fc, integer *na, integer *nov, integer *nra, integer *nca, integer *ncb, integer *irf, integer *icf)
{
  /* System generated locals */
  integer irf_dim1, icf_dim1, a_dim1, a_dim2, 
    b_dim1, b_dim2, fa_dim1, sol1_dim1,
    sol2_dim1;

    /* Local variables */
  integer nram, icfj1, i, j;
  integer nrapj, irfir, j1, novpj, icfnovpir, ir;
  doublereal sm;
  integer novpir, irp1;

  std::vector<doublereal> x(*nra);

/* Determine the local varables by backsubstitition. */

    /* Parameter adjustments */
    /*--fc;*/
  sol2_dim1 = *nov;
  sol1_dim1 = *nov;
  irf_dim1 = *nra;
  fa_dim1 = *nra;
  icf_dim1 = *nca;
  a_dim1 = *nca;
  a_dim2 = *nra;
  b_dim1 = *ncb;
  b_dim2 = *nra;
    
  nram = *nra - *nov;

/* Backsubstitution in the condensation of parameters; no communication. 
*/
  for (i = 0; i < *na; ++i) {
    for (ir = nram - 1; ir >= 0; --ir) {
      irp1 = ir + 1;
      sm = 0.;
      irfir = ARRAY2D(irf, ir, i) - 1;
      for (j = 0; j < *nov; ++j) {
	nrapj = *nra + j;
	sm += ARRAY3D(a, j, irfir, i) * ARRAY2D(sol1, j, i);
	sm += ARRAY3D(a, nrapj, irfir, i) * ARRAY2D(sol2, j, i);
      }
      for (j = 0; j < *ncb; ++j) {
	novpj = *nov + j;
	sm += ARRAY3D(b, j, irfir , i) * fc[novpj];
      }
      for (j = irp1; j < nram; ++j) {
	j1 = j + *nov;
	icfj1 = ARRAY2D(icf, j1, i) - 1;
	sm += ARRAY3D(a, icfj1, irfir, i) * x[icfj1];
      }
      novpir = *nov + ir;
      icfnovpir = ARRAY2D(icf, novpir, i) - 1;
      x[icfnovpir] = (ARRAY2D(fa, irfir, i) - sm) / ARRAY3D(a, icfnovpir, irfir, i);
    }
    /*        **Copy SOL1 and X into FA */
    for (j = 0; j < *nov; ++j) {
      ARRAY2D(fa, j, i) = ARRAY2D(sol1, j, i);
    }
    for (j = *nov; j < *nra; ++j) {
      ARRAY2D(fa, j, i) = x[j];
    }
  }

  return 0;
} /* infpar_ */

/*     ---------- ------ */
/* Subroutine */ int 
print1(FILE *fp9, integer *nov, integer *na, integer *nra, integer *nca, integer *ncb, integer *nrc, doublereal *a, doublereal *b, doublereal *c, doublereal *d, doublereal *fa, doublereal *fc)
{

  /* System generated locals */
  integer a_dim1, a_dim2, b_dim1, b_dim2, c_dim1, 
    c_dim2, d_dim1, fa_dim1;

  /* Local variables */
  integer i, ic, ir;

  /* Parameter adjustments */
  /*--fc;*/
  fa_dim1 = *nra;
  a_dim1 = *nca;
  a_dim2 = *nra;
  d_dim1 = *ncb;
  b_dim1 = *ncb;
  b_dim2 = *nra;
  c_dim1 = *nca;
  c_dim2 = *nrc;
    
  xpp::print(fp9,"AA , BB , FA (Full dimension) :\n");	
  /* should be 10.3f*/
  for (i = 0; i < *na; ++i) {
    xpp::print(fp9,"I={:3}\n",i + 1);
    for (ir = 0; ir < *nra; ++ir) {
      int total_written = 0;
      for (ic = 0; ic < *nca; ++ic) {
	if((total_written != 0) && (total_written%12 == 0))
	  xpp::print(fp9,"\n");
	xpp::print(fp9," {:10.3E}",ARRAY3D(a, ic, ir, i));
	total_written++;
      }
      for (ic = 0; ic < *ncb; ++ic) {
	if((total_written != 0) && (total_written%12 == 0))
	  xpp::print(fp9,"\n");
	xpp::print(fp9," {:10.3E}",ARRAY3D(b, ic, ir, i));	
	total_written++;
      }
      if((total_written != 0) && (total_written%12 == 0))
	xpp::print(fp9,"\n");
      xpp::print(fp9," {:10.3E}",ARRAY2D(fa, ir, i));	
      xpp::print(fp9,"\n");	
    }
  }

  xpp::print(fp9,"CC (Full dimension) :\n");	

  for (i = 0; i < *na; ++i) {
    xpp::print(fp9,"I={:3}\n",i + 1);	
    for (ir = 0; ir < *nrc; ++ir) {
      int total_written = 0;
      for (ic = 0; ic < *nca; ++ic) {
	if((total_written != 0) && (total_written%12 == 0))
	  xpp::print(fp9,"\n");
	xpp::print(fp9," {:10.3E}",ARRAY3D(c, ic, ir, i));	
	total_written++;
      }
      xpp::print(fp9,"\n");	
    }
  }

  xpp::print(fp9,"DD , FC\n");	

  for (ir = 0; ir < *nrc; ++ir) {
    int total_written = 0;
    for (ic = 0; ic < *ncb; ++ic) {
      if((total_written != 0) && (total_written%12 == 0))
	xpp::print(fp9,"\n");
      xpp::print(fp9," {:10.3E}",ARRAY2D(d, ic, ir));	
      total_written++;
    }
    xpp::print(fp9," {:10.3E}\n",fc[ir]);	
  }

  return 0;
} /* print1_ */

