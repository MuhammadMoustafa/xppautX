#include "xpp_mem.h"
#include <vector>
#include "auto_c.h"
#include "session.h"
#include "xpp_job.h" /* xppautX: cancel */


namespace {
/* This structure contains all of the input data for the setubv routine
   Those values which are arrays and those
   which are input and output are markered as such*/
struct setubv_parallel_arglist {
  integer ndim, ncol, nbc, nint, ncb, nrc, nra, nca, na; /*scalar input */
  FUNI_TYPE((*funi)); /*scalar input*/
  ICNI_TYPE((*icni)); /*scalar input*/
  integer ndxloc; /*scalar input*/
  iap_type *iap; /*array input size: NIAP*/
  rap_type *rap; /*array input size: NRAP*/
  doublereal  *par; /* two parameter_count halves */
  integer *icp; /* two parameter_count halves */
  xpp::Matrix3<doublereal> aa; /*output (but must be initialized to 0) nca X nra X na */
  xpp::Matrix3<doublereal> bb; /*output (but must be initialized to 0) ncb X nra X na */
  xpp::Matrix3<doublereal> cc; /*output (but must be initialized to 0) nca X nrc X na */
  xpp::Matrix<doublereal> dd; /*output (but must be initialized to 0) ncb X nrc */
  doublereal *fa; /*array output (but must be initialized to 0) size: *nra X *na */
  doublereal *fc; /*array output (but must be initialized to 0) size: *nrc */
  doublereal *ups; /*array input size: *ndxloc X (*ndim X *ncol) */
  doublereal *uoldps; /*array input size: *ndxloc X (*ndim X *ncol) */
  doublereal *udotps; /*array input size: *ndxloc X (*ndim X *ncol) */
  doublereal *upoldp; /*array input size: *ndxloc X (*ndim X *ncol) */
  doublereal *dtm; /*array input size: *na */
  integer loop_start; /*scalar input*/
  integer loop_end; /*scalar input*/  
  integer loop_offset; /*scalar input*/
  doublereal *wp; /*array input size: MCL2*MCL1 */
  doublereal *wt; /*array input size: MCL2*MCL1 */
  doublereal *wi; /*array input size: MCL2*MCL1??? Not sure of this one yet */
  doublereal *thu; /*array input size: ndim * 8 */
  doublereal *thl; /* parameter_count entries */
  doublereal *rldot; /* parameter_count entries */
  BCNI_TYPE((*bcni));
};

/* setubv's steps (the original AUTO also ran them on pthreads or MPI
   workers). */
void setubv_make_fa(const setubv_parallel_arglist *larg);
void setubv_make_fc_dd(const setubv_parallel_arglist &larg, doublereal *dups, doublereal *rlcur,
		       doublereal *rlold, doublereal rds);
void setubv_parallel_arglist_constructor(integer ndim, integer na, integer ncol,
					 integer nbc, integer nint, integer ncb, integer nrc, integer nra, integer nca,
					 FUNI_TYPE((*funi)), ICNI_TYPE((*icni)), integer ndxloc, iap_type *iap, rap_type *rap, doublereal *par,
					 integer *icp, xpp::Matrix3<doublereal> aa, xpp::Matrix3<doublereal> bb,
					 xpp::Matrix3<doublereal> cc, xpp::Matrix<doublereal> dd, doublereal *fa, doublereal *fc, doublereal *ups,
					 doublereal *uoldps, doublereal *udotps,
					 doublereal *upoldp, doublereal *dtm,
					 doublereal *wp, doublereal *wt, doublereal *wi,
					 doublereal *thu, doublereal *thl, doublereal *rldot, BCNI_TYPE((*bcni)),
					 setubv_parallel_arglist *data);

void setubv_make_aa_bb(const setubv_parallel_arglist *larg)
{
  integer i,j,k,l,l1,ib,ic,jj,jp1,ic1,ib1;
  doublereal dt,ddt;
  doublereal *ups=larg->ups,*uoldps=larg->uoldps,*wp=larg->wp,*wt=larg->wt;
  const xpp::Matrix3<doublereal> aa=larg->aa,bb=larg->bb;
  const integer ups_dim1=larg->ndxloc,uoldps_dim1=larg->ndxloc;
  const integer dfdu_dim1=larg->ndim,dfdp_dim1=larg->ndim;
  const integer wploc_dim1=larg->ncol+1,wp_dim1=larg->ncol+1,wt_dim1=larg->ncol+1;
  std::vector<doublereal> dfdp(xpp::size_product(larg->ndim, larg->iap->lib->parameter_count)),dfdu(xpp::size_product(larg->ndim,larg->ndim));
  std::vector<doublereal> uold(larg->ndim),f(larg->ndim),u(larg->ndim);
  std::vector<doublereal> wploc(xpp::size_product(larg->ncol, xpp::size_sum(larg->ncol, 1))),prm(larg->iap->lib->parameter_count);
  /* Generate AA and BB: */
  
  /*      Partition the mesh intervals */
  /*jj will be replaced with loop_start and loop_end*/
  for (jj = larg->loop_start; jj < larg->loop_end; ++jj) {
    if (!larg->iap->lib->pure_rhs && larg->iap->lib->setubv_stop && xpp::job::cancelled()) break; /* xppautX: cancel */
    j = jj;
    jp1 = j + 1;
    dt = larg->dtm[j];
    ddt = 1. / dt;
    for (ic = 0; ic < larg->ncol; ++ic) {
      for (ib = 0; ib < larg->ncol + 1; ++ib) {
	ARRAY2D(wploc, ib, ic) = ddt * ARRAY2D(wp,ib, ic);
      }
    }
    /*this loop uses the loop_offset variable since up and uoldps
      and sent by the MPI version in their entirety, but
      loop_start and loop_end have been shifted.  The loop_offset
      variable contains the original value of loop_start and removes
      the shift*/
    for (ic = 0; ic < larg->ncol; ++ic) {
      for (k = 0; k < larg->ndim; ++k) {
	u[k] = ARRAY2D(wt, larg->ncol, ic) * ARRAY2D(ups, jp1 + larg->loop_offset, k);
	uold[k] = ARRAY2D(wt, larg->ncol, ic) * ARRAY2D(uoldps, jp1 + larg->loop_offset, k);
	for (l = 0; l < larg->ncol; ++l) {
	  l1 = l * larg->ndim + k;
	  u[k] += ARRAY2D(wt, l, ic) * ARRAY2D(ups, j + larg->loop_offset, l1);
	  uold[k] += ARRAY2D(wt, l, ic) * ARRAY2D(uoldps, j + larg->loop_offset, l1);
	}
      }

      for (i = 0; i < larg->iap->lib->parameter_count; ++i) {
	prm[i] = larg->par[i];
      }
      (*(larg->funi))(larg->iap, larg->rap, larg->ndim, u.data(), uold.data(), larg->icp, prm.data(), 2, f.data(), dfdu.data(), dfdp.data());


      ic1 = ic * (larg->ndim);
      for (ib = 0; ib < larg->ncol + 1; ++ib) {
	double wt_tmp=ARRAY2D(wt, ib, ic);
	double wploc_tmp=ARRAY2D(wploc, ib, ic);
	ib1 = ib * larg->ndim;
	for (i = 0; i < larg->ndim; ++i) {
	  aa(ib1 + i, ic1 + i, jj) = wploc_tmp;
	  for (k = 0; k < larg->ndim; ++k) {
	    aa(ib1 + k, ic1 + i, jj) -= wt_tmp * ARRAY2D(dfdu, i, k);
	  }
	}
      }
      for (i = 0; i < larg->ndim; ++i) {
	for (k = 0; k < larg->ncb; ++k) {
	  bb(k, ic1 + i,  jj) = -ARRAY2D(dfdp, i, larg->icp[k]);
	}
      }
    }
  
  }

}

// The caller alone polls cancellation before/after each parallel section.
void dispatch_mesh(const setubv_parallel_arglist *larg,
                   void (*work)(const setubv_parallel_arglist *))
{
  AutoLib &lib=*larg->iap->lib;
  if (lib.collocation) {
    lib.collocation->prepare(lib,larg->iap->ndim,larg->iap->nbc);
    lib.collocation->intervals(larg->loop_end-larg->loop_start,[&](std::ptrdiff_t begin,std::ptrdiff_t end,AutoLib &worker) {
      iap_type local=*larg->iap;
      local.lib=&worker;
      setubv_parallel_arglist mine=*larg;
      mine.iap=&local;
      mine.loop_start=larg->loop_start+begin;
      mine.loop_end=larg->loop_start+end;
      work(&mine);
    });
    lib.collocation->publish_rhs(*lib.session);
  } else work(larg);

}

void setubv_make_aa_bb_cc(const setubv_parallel_arglist *larg)
{
  /* System generated locals */
  integer ups_dim1,
    uoldps_dim1, udotps_dim1, upoldp_dim1,
    dbc_dim1, dicd_dim1;

  /* Local variables */
  integer i, j, k, m;
  integer k1;
  integer i1,j1;

  integer jj;
  integer jp1;

  doublereal *ups = larg->ups;
  doublereal *upoldp = larg->upoldp;
  doublereal *udotps = larg->udotps;
  doublereal *uoldps = larg->uoldps;

  const xpp::Matrix3<doublereal> cc = larg->cc;

  std::vector<doublereal> dicd(xpp::size_product(larg->nint, xpp::size_sum(larg->ndim, larg->iap->lib->parameter_count)));
  std::vector<doublereal> ficd(larg->nint);
  std::vector<doublereal> dbc(xpp::size_product(larg->nbc, xpp::size_sum(xpp::size_product(2, larg->ndim), larg->iap->lib->parameter_count)));
  std::vector<doublereal> fbc(larg->nbc);
  std::vector<doublereal> uic(larg->ndim);
  std::vector<doublereal> uio(larg->ndim);
  std::vector<doublereal> uid(larg->ndim);
  std::vector<doublereal> uip(larg->ndim);
  std::vector<doublereal> ubc0(larg->ndim);
  std::vector<doublereal> ubc1(larg->ndim);

  upoldp_dim1 = larg->ndxloc;
  udotps_dim1 = larg->ndxloc;
  uoldps_dim1 = larg->ndxloc;
  ups_dim1 = larg->ndxloc;
  dicd_dim1 = larg->nint;
  dbc_dim1 = larg->nbc;

  dispatch_mesh(larg,setubv_make_aa_bb);

  /*     Generate CC : */
  
  /*     Boundary conditions : */
  if (larg->nbc > 0) {
    for (i = 0; i < larg->ndim; ++i) {
      ubc0[i] = ARRAY2D(ups, 0, i);
      ubc1[i] = ARRAY2D(ups, larg->na, i);
    }
    

    (*(larg->bcni))(larg->iap, larg->rap, larg->ndim, larg->par, 
	    larg->icp, larg->nbc, ubc0.data(), ubc1.data(), fbc.data(), 2, dbc.data());
    for (i = 0; i < larg->nbc; ++i) {
      for (k = 0; k < larg->ndim; ++k) {
	/*NOTE!!
	  This needs to split up.  Only the first processor does the first part
	  and only the last processors does the last part.*/
	if(larg->loop_offset + larg->loop_start == 0) {
	  cc(k, i, 0) = ARRAY2D(dbc, i, k);
	}
	if(larg->loop_offset + larg->loop_end == larg->na) {
	  cc(larg->nra + k, i, larg->na-1 - larg->loop_offset) = 
	    ARRAY2D(dbc ,i , larg->ndim + k);
	}
      }
    }
  }
  
  /*     Integral constraints : */
  if (larg->nint > 0) {
    for (jj = larg->loop_start; jj < larg->loop_end; ++jj) {
      j = jj;
      jp1 = j + 1;
      for (k = 0; k < (larg->ncol + 1); ++k) {
	for (i = 0; i < larg->ndim; ++i) {
	  i1 = k * larg->ndim + i;
	  j1 = j;
	  if (k+1 == (larg->ncol + 1)) {
	    i1 = i;
	  }
	  if (k+1 == (larg->ncol + 1)) {
	    j1 = jp1;
	  }
	  uic[i] = ARRAY2D(ups, j1 + larg->loop_offset, i1);
	  uio[i] = ARRAY2D(uoldps, j1 + larg->loop_offset, i1);
	  uid[i] = ARRAY2D(udotps, j1 + larg->loop_offset, i1);
	  uip[i] = ARRAY2D(upoldp, j1 + larg->loop_offset, i1);
	}
	

	(*(larg->icni))(larg->iap, larg->rap, larg->ndim, larg->par, 
		larg->icp, larg->nint, 
		uic.data(), uio.data(), uid.data(), uip.data(), ficd.data(), 2, dicd.data());

	
	for (m = 0; m < larg->nint; ++m) {
	  for (i = 0; i < larg->ndim; ++i) {
	    k1 = k * larg->ndim + i;
	    cc(k1, larg->nbc + m, jj) = 
	      larg->dtm[j] * larg->wi[k ] * ARRAY2D(dicd, m, i);
	  }
	}
      }
    }
  }
  /*     Pseudo-arclength equation : */
  for (jj = larg->loop_start; jj < larg->loop_end; ++jj) {
    for (i = 0; i < larg->ndim; ++i) {
      for (k = 0; k < larg->ncol; ++k) {
	k1 = k * larg->ndim + i;
	cc(k1 , larg->nrc - 1, jj) = 
	  larg->dtm[jj] * larg->thu[i] * larg->wi[k] * 
	  ARRAY2D(udotps, jj + larg->loop_offset, k1);
      }
      cc(larg->nra + i, larg->nrc -1, jj) = 
	larg->dtm[jj] * larg->thu[i] * larg->wi[larg->ncol] * 
	ARRAY2D(udotps, jj + 1 + larg->loop_offset, i);
    }
  }


}

} // namespace

int 
setubv(integer ndim, xpp::AutoProblem ips, integer na, integer ncol, integer nbc, integer nint, integer ncb, integer nrc, integer nra, integer nca, 
       FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), integer ndxloc, iap_type *iap, rap_type *rap, doublereal *par, integer *icp, 
       doublereal rds, xpp::Matrix3<doublereal> aa, xpp::Matrix3<doublereal> bb, xpp::Matrix3<doublereal> cc, xpp::Matrix<doublereal> dd, doublereal *fa, doublereal *fc, doublereal *rlcur, 
       doublereal *rlold, doublereal *rldot, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *dups, 
       doublereal *dtm, doublereal *thl, doublereal *thu, doublereal *p0, doublereal *p1)
{
  /* System generated locals */
  /* Local variables */
  integer i, j, k;

  
  std::vector<doublereal> wi(xpp::size_sum(ncol, 1));
  std::vector<doublereal> wp(xpp::size_product(ncol, xpp::size_sum(ncol, 1)));
  std::vector<doublereal> wt(xpp::size_product(ncol, xpp::size_sum(ncol, 1)));

  wint(ncol + 1, wi.data());
  genwts(ncol, ncol + 1, wt.data(), wp.data());
  
  /* Initialize to zero. */
  for (i = 0; i < nrc; ++i) {
    fc[i] = 0.;
    for (k = 0; k < ncb; ++k) {
      dd(k, i) = 0.;
    }
  }

  /* Set constants. */
  for (i = 0; i < ncb; ++i) {
    par[icp[i]] = rlcur[i];
  }
  
  /*  NA is the local node's mesh interval number. */
  
  for (i = 0; i < na; ++i) {
    for (j = 0; j < nra; ++j) {
      for (k = 0; k < nca; ++k) {
	aa(k, j, i) = 0.;
      }
    }
    for (j = 0; j < nra; ++j) {
      for (k = 0; k < ncb; ++k) {
	bb(k, j, i) = 0.;
      }
    }
    for (j = 0; j < nca; ++j) {
      for (k = 0; k < nrc; ++k) {
	cc(j, k, i) = 0.;
      }
    }
  }

  /*     ** Time evolution computations (parabolic systems) */
  if (xpp::problem_is_parabolic_evolution(ips)) {
    rap->tivp = rlold[0];
  } 
 
  {
    setubv_parallel_arglist arglist;
    setubv_parallel_arglist_constructor(ndim, na, ncol, nbc, nint, ncb, 
					nrc, nra, nca, funi, icni, ndxloc, iap, rap, 
					par, icp, aa, bb, cc, dd, fa, fc, ups, 
					uoldps, udotps, upoldp, dtm, wp.data(), wt.data(), wi.data(), 
					thu, thl, rldot, bcni, &arglist);
  
    if (iap->lib->setubv_stop && xpp::job::cancelled()) return 0;
    setubv_make_aa_bb_cc(&arglist);
    if (iap->lib->setubv_stop && xpp::job::cancelled()) return 0;
    dispatch_mesh(&arglist,setubv_make_fa);
    if (iap->lib->setubv_stop && xpp::job::cancelled()) return 0;
    setubv_make_fc_dd(arglist,dups,rlcur,rlold,rds);
  }

  return 0;
}

namespace {
void setubv_make_fa(const setubv_parallel_arglist *larg) {
  integer i,j,k,l;
  integer ic,k1,ib;
  integer jj,jp1,l1,ic1;
  doublereal dt,ddt;

  doublereal *ups = larg->ups;
  integer ups_dim1 = larg->ndxloc;

  doublereal *uoldps = larg->uoldps;
  integer uoldps_dim1 = larg->ndxloc;

  doublereal *wp = larg->wp;
  integer wp_dim1 = larg->ncol + 1;

  doublereal *wt = larg->wt;
  integer wt_dim1 = larg->ncol + 1;
  
  doublereal *fa = larg->fa;
  integer fa_dim1 = larg->nra;
  
  std::vector<doublereal> wploc(xpp::size_product(larg->ncol, xpp::size_sum(larg->ncol, 1)));
  integer wploc_dim1 = larg->ncol + 1;
  
  std::vector<doublereal> dfdp(xpp::size_product(larg->ndim, larg->iap->lib->parameter_count));
  std::vector<doublereal> dfdu(xpp::size_product(larg->ndim, larg->ndim));
  std::vector<doublereal> u(larg->ndim);
  std::vector<doublereal> uold(larg->ndim);
  std::vector<doublereal> f(larg->ndim);
  std::vector<doublereal> prm(larg->iap->lib->parameter_count);

  for (jj = larg->loop_start; jj < larg->loop_end; ++jj) {
    if (!larg->iap->lib->pure_rhs && larg->iap->lib->setubv_stop && xpp::job::cancelled()) break; /* xppautX: cancel */
    j = jj;
    jp1 = j + 1;
    dt = larg->dtm[j];
    ddt = 1. / dt;
    for (ic = 0; ic < larg->ncol; ++ic) {
      for (ib = 0; ib < larg->ncol + 1; ++ib) {
	ARRAY2D(wploc, ib, ic) = ddt * ARRAY2D(wp,ib, ic);
      }
    }
    for (ic = 0; ic < larg->ncol; ++ic) {
      for (k = 0; k < larg->ndim; ++k) {
	u[k] = ARRAY2D(wt, larg->ncol, ic) * ARRAY2D(ups, jp1, k);
	uold[k] = ARRAY2D(wt, larg->ncol, ic) * ARRAY2D(uoldps, jp1, k);
	for (l = 0; l < larg->ncol; ++l) {
	  l1 = l * larg->ndim + k;
	  u[k] += ARRAY2D(wt, l, ic) * ARRAY2D(ups, j + larg->loop_offset, l1);
	  uold[k] += ARRAY2D(wt, l, ic) * ARRAY2D(uoldps, j + larg->loop_offset, l1);
	}
      }

      for (i = 0; i < larg->iap->lib->parameter_count; ++i) {
	prm[i] = larg->par[i];
      }
      /* The residual reads only f: no Jacobian (docs/xppaut-findings.md #39). */
      (*(larg->funi))(larg->iap, larg->rap, larg->ndim, u.data(), uold.data(), larg->icp, prm.data(), 0, f.data(), dfdu.data(), dfdp.data());

      ic1 = ic * (larg->ndim);
      for (i = 0; i < larg->ndim; ++i) {
	ARRAY2D(fa,ic1 + i, jj) = f[i] - ARRAY2D(wploc, larg->ncol, ic) * ARRAY2D(ups, jp1 + larg->loop_offset, i);
	for (k = 0; k < larg->ncol; ++k) {
	  k1 = k * larg->ndim + i;
	  ARRAY2D(fa, ic1 + i, jj) -= ARRAY2D(wploc, k, ic) * ARRAY2D(ups, j + larg->loop_offset, k1);
	}
      }
    }
  
  }
  
}


void setubv_make_fc_dd(const setubv_parallel_arglist &larg, doublereal *dups, doublereal *rlcur, 
	     doublereal *rlold, doublereal rds) {
  integer i,j,jj,jp1,k,i1,m,j1;
  doublereal rlsum;

  integer dups_dim1 = larg.ndxloc;
  
  const xpp::Matrix<doublereal> dd = larg.dd;

  doublereal *ups = larg.ups;
  integer ups_dim1 = larg.ndxloc;

  doublereal *uoldps = larg.uoldps;
  integer uoldps_dim1 = larg.ndxloc;
  
  doublereal *udotps = larg.udotps;
  integer udotps_dim1 = larg.ndxloc;
  
  doublereal *upoldp = larg.upoldp;
  integer upoldp_dim1 = larg.ndxloc;
  
  integer dbc_dim1 = larg.nbc;
  std::vector<doublereal> dbc(xpp::size_product(larg.nbc, xpp::size_sum(xpp::size_product(2, larg.ndim), larg.iap->lib->parameter_count)));
  std::vector<doublereal> fbc(larg.nbc);
  std::vector<doublereal> ubc0(larg.ndim);
  std::vector<doublereal> ubc1(larg.ndim);
  integer dicd_dim1 = larg.nint;
  std::vector<doublereal> dicd(xpp::size_product(larg.nint, xpp::size_sum(larg.ndim, larg.iap->lib->parameter_count)));
  std::vector<doublereal> ficd(larg.nint);
  std::vector<doublereal> uic(larg.ndim);
  std::vector<doublereal> uio(larg.ndim);
  std::vector<doublereal> uid(larg.ndim);
  std::vector<doublereal> uip(larg.ndim);

  /* Boundary condition part of FC */
  if (larg.nbc > 0) {
    for (i = 0; i < larg.ndim; ++i) {
      ubc0[i] = ARRAY2D(ups, 0, i);
      ubc1[i] = ARRAY2D(ups, larg.na, i);
    }
    
    (*(larg.bcni))(larg.iap, larg.rap, larg.ndim, larg.par, 
		   larg.icp, larg.nbc, ubc0.data(), ubc1.data(), fbc.data(), 2, dbc.data());
    for (i = 0; i < larg.nbc; ++i) {
      larg.fc[i] = -fbc[i];
      for (k = 0; k < larg.ncb; ++k) {
	dd(k, i) = 
	  ARRAY2D(dbc, i, (larg.ndim *2) + larg.icp[k]);
      }
    }
    /*       Save difference : */
    for (j = 0; j < larg.na + 1; ++j) {
      for (i = 0; i < larg.nra; ++i) {
	ARRAY2D(dups, j, i) = ARRAY2D(ups, j, i) - ARRAY2D(uoldps, j, i);
      }
    }
  }

  /* Integral constraint part of FC */
  if (larg.nint > 0) {
    for (jj = larg.loop_start; jj < larg.loop_end; ++jj) {
      j = jj;
      jp1 = j + 1;
      for (k = 0; k < (larg.ncol + 1); ++k) {
	for (i = 0; i < larg.ndim; ++i) {
	  i1 = k * larg.ndim + i;
	  j1 = j;
	  if (k+1 == (larg.ncol + 1)) {
	    i1 = i;
	  }
	  if (k+1 == (larg.ncol + 1)) {
	    j1 = jp1;
	  }
	  uic[i] = ARRAY2D(ups, j1, i1);
	  uio[i] = ARRAY2D(uoldps, j1, i1);
	  uid[i] = ARRAY2D(udotps, j1, i1);
	  uip[i] = ARRAY2D(upoldp, j1, i1);
	}
	
	(*(larg.icni))(larg.iap, larg.rap, larg.ndim, larg.par, 
		larg.icp, larg.nint, 
		uic.data(), uio.data(), uid.data(), uip.data(), ficd.data(), 2, dicd.data());
	
	for (m = 0; m < larg.nint; ++m) {
	  larg.fc[larg.nbc + m] -= larg.dtm[j] * larg.wi[k] * ficd[m];
	  for (i = 0; i < larg.ncb; ++i) {
	    dd(i, larg.nbc + m) += 
	      larg.dtm[j] * larg.wi[k] * ARRAY2D(dicd, m, larg.ndim + larg.icp[i]);
	  }
	}
      }
    }
  }

  for (i = 0; i < larg.ncb; ++i) {
    dd(i, (larg.nrc-1)) = larg.thl[larg.icp[i]] * larg.rldot[i];
  }

  rlsum = 0.;
  for (i = 0; i < larg.ncb; ++i) {
    rlsum += larg.thl[larg.icp[i]] * (rlcur[i] - rlold[i]) * larg.rldot[i];
  }

  larg.fc[larg.nrc-1] = rds - rinpr(larg.iap, &(larg.ndim), &(larg.ndxloc), larg.udotps, dups, larg.dtm, larg.thu) - rlsum;


}


/* Fill in a setubv_parallel_arglist for the individual variables */
void setubv_parallel_arglist_constructor(integer ndim, integer na, integer ncol, 
					 integer nbc, integer nint, integer ncb, integer nrc, integer nra, integer nca, 
					 FUNI_TYPE((*funi)), ICNI_TYPE((*icni)), integer ndxloc, iap_type *iap, rap_type *rap, doublereal *par, 
					 integer *icp, xpp::Matrix3<doublereal> aa, xpp::Matrix3<doublereal> bb, 
					 xpp::Matrix3<doublereal> cc, xpp::Matrix<doublereal> dd, doublereal *fa, doublereal *fc, doublereal *ups, 
					 doublereal *uoldps, doublereal *udotps, 
					 doublereal *upoldp, doublereal *dtm, 
					 doublereal *wp, doublereal *wt, doublereal *wi,
					 doublereal *thu, doublereal *thl,
					 doublereal *rldot, BCNI_TYPE((*bcni)), setubv_parallel_arglist *data) {
  data->ndim   = ndim;
  data->ncol   = ncol;
  data->nbc    = nbc;
  data->nint   = nint;
  data->ncb    = ncb;
  data->nrc    = nrc;
  data->nra    = nra;
  data->nca    = nca;
  data->na     = na;
  data->funi   = funi;
  data->icni   = icni;
  data->ndxloc = ndxloc;
  data->iap    = iap;
  data->rap    = rap;
  data->par    = par;
  data->icp    = icp;
  data->aa     = aa;
  data->bb     = bb;
  data->cc     = cc;
  data->dd     = dd;
  data->fa     = fa;
  data->fc     = fc;
  data->ups    = ups;
  data->uoldps = uoldps;
  data->udotps = udotps;
  data->upoldp = upoldp;
  data->dtm    = dtm;
  data->loop_start = 0;
  data->loop_end   = na;
  data->loop_offset = 0;
  data->wp     = wp;
  data->wt     = wt;
  data->wi     = wi;
  data->thu    = thu;
  data->thl    = thl;
  data->rldot  = rldot;
  data->bcni   = bcni;
}  
} // namespace
