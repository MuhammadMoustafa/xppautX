#include "xpp_mem.h"
#include <vector>
#include <cstring>
#include "autox_c.h"
#include "session.h"
#include "xpp_job.h" /* xppautX: cancel */

namespace {
/* This structure contains all of the input data for the setup_boundary_system routine
   Those values which are arrays and those
   which are input and output are markered as such*/
struct setubv_parallel_arglist {
  std::ptrdiff_t ndim, ncol, nbc, nint, ncb, nrc, nra, nca, na; /*scalar input */
  FUNI_TYPE((*problem_function)); /*scalar input*/
  ICNI_TYPE((*integral_function)); /*scalar input*/
  std::ptrdiff_t ndxloc; /*scalar input*/
  iap_type *iap; /*array input size: NIAP*/
  rap_type *rap; /*array input size: NRAP*/
  double  *par; /* two parameter_count halves */
  std::ptrdiff_t *icp; /* two parameter_count halves */
  xpp::Matrix3<double> aa; /*output (but must be initialized to 0) nca X nra X na */
  xpp::Matrix3<double> bb; /*output (but must be initialized to 0) ncb X nra X na */
  xpp::Matrix3<double> cc; /*output (but must be initialized to 0) nca X nrc X na */
  xpp::Matrix<double> dd; /*output (but must be initialized to 0) ncb X nrc */
  double *fa; /*array output (but must be initialized to 0) size: *nra X *na */
  double *fc; /*array output (but must be initialized to 0) size: *nrc */
  double *ups; /*array input size: *ndxloc X (*ndim X *ncol) */
  double *uoldps; /*array input size: *ndxloc X (*ndim X *ncol) */
  double *udotps; /*array input size: *ndxloc X (*ndim X *ncol) */
  double *upoldp; /*array input size: *ndxloc X (*ndim X *ncol) */
  double *dtm; /*array input size: *na */
  std::ptrdiff_t loop_start; /*scalar input*/
  std::ptrdiff_t loop_end; /*scalar input*/  
  std::ptrdiff_t loop_offset; /*scalar input*/
  double *wp; /*array input size: MCL2*MCL1 */
  double *wt; /*array input size: MCL2*MCL1 */
  double *wi; /*array input size: MCL2*MCL1??? Not sure of this one yet */
  double *thu; /*array input size: ndim * 8 */
  double *thl; /* parameter_count entries */
  double *rldot; /* parameter_count entries */
  BCNI_TYPE((*boundary_function));
  bool jacobian = true;
};

/* setup_boundary_system's steps (the original autoX also ran them on pthreads or MPI
   workers). */
void setubv_make_fc_dd(const setubv_parallel_arglist &larg, double *dups, double *rlcur,
		       double *rlold, double rds);
void setubv_parallel_arglist_constructor(std::ptrdiff_t ndim, std::ptrdiff_t na, std::ptrdiff_t ncol,
					 std::ptrdiff_t nbc, std::ptrdiff_t nint, std::ptrdiff_t ncb, std::ptrdiff_t nrc, std::ptrdiff_t nra, std::ptrdiff_t nca,
					 FUNI_TYPE((*problem_function)), ICNI_TYPE((*integral_function)), std::ptrdiff_t ndxloc, iap_type *iap, rap_type *rap, double *par,
					 std::ptrdiff_t *icp, xpp::Matrix3<double> aa, xpp::Matrix3<double> bb,
					 xpp::Matrix3<double> cc, xpp::Matrix<double> dd, double *fa, double *fc, double *ups,
					 double *uoldps, double *udotps,
					 double *upoldp, double *dtm,
					 double *wp, double *wt, double *wi,
					 double *thu, double *thl, double *rldot, BCNI_TYPE((*boundary_function)),
					 setubv_parallel_arglist *data);

void setubv_make_fa_aa_bb(const setubv_parallel_arglist *larg)
{
  std::ptrdiff_t i,j,k,l,l1,ib,ic,jj,jp1,ic1,ib1;
  double dt,ddt;
  double *ups=larg->ups,*uoldps=larg->uoldps,*wp=larg->wp,*wt=larg->wt;
  const xpp::Matrix3<double> aa=larg->aa,bb=larg->bb;
  const std::ptrdiff_t ups_dim1=larg->ndxloc,uoldps_dim1=larg->ndxloc;
  const std::ptrdiff_t dfdu_dim1=larg->ndim,dfdp_dim1=larg->ndim;
  const std::ptrdiff_t wploc_dim1=larg->ncol+1,wp_dim1=larg->ncol+1,wt_dim1=larg->ncol+1;
  auto &dfdp = larg->iap->run->assembly.dfdp;
  auto &dfdu = larg->iap->run->assembly.dfdu;
  auto &uold = larg->iap->run->assembly.uold;
  auto &f = larg->iap->run->assembly.f;
  auto &u = larg->iap->run->assembly.u;
  auto &wploc = larg->iap->run->assembly.wploc;
  auto &prm = larg->iap->run->assembly.prm;
  /* Generate AA and BB: */
  
  /*      Partition the mesh intervals */
  /*jj will be replaced with loop_start and loop_end*/
  for (jj = larg->loop_start; jj < larg->loop_end; ++jj) {
    if (!larg->iap->run->owner && larg->iap->run->setubv_stop && xpp::job::cancelled()) break; /* xppautX: cancel */
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

      for (i = 0; i < larg->iap->run->parameter_count; ++i) {
	prm[i] = larg->par[i];
      }
      (*(larg->problem_function))(larg->iap, larg->rap, larg->ndim, u.data(), uold.data(), larg->icp, prm.data(), larg->jacobian ? 2 : 0, f.data(), dfdu.data(), dfdp.data());

      ic1 = ic * (larg->ndim);
      // FUNI already supplied the residual before differentiating.
      const std::ptrdiff_t fa_dim1 = larg->nra;
      double *fa = larg->fa;
      for (i = 0; i < larg->ndim; ++i) {
        ARRAY2D(fa, ic1 + i, jj) = f[i] - ARRAY2D(wploc, larg->ncol, ic) * ARRAY2D(ups, jp1 + larg->loop_offset, i);
        for (k = 0; k < larg->ncol; ++k) {
          const std::ptrdiff_t k1 = k * larg->ndim + i;
          ARRAY2D(fa, ic1 + i, jj) -= ARRAY2D(wploc, k, ic) * ARRAY2D(ups, j + larg->loop_offset, k1);
        }
      }

      if (larg->jacobian) {
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
            bb(k, ic1 + i, jj) = -ARRAY2D(dfdp, i, larg->icp[k]);
          }
        }
      }
    }
  
  }

}

// The caller alone polls cancellation before/after each parallel section.
void dispatch_mesh(const setubv_parallel_arglist *larg,
                   void (*work)(const setubv_parallel_arglist *))
{
  AutoxRun &run=*larg->iap->run;
  if (run.collocation) {
    run.collocation->prepare(run,larg->iap->ndim,larg->iap->nbc);
    run.collocation->intervals(larg->loop_end-larg->loop_start,[&](std::ptrdiff_t begin,std::ptrdiff_t end,AutoxRun &worker) {
      iap_type local=*larg->iap;
      local.run=&worker;
      setubv_parallel_arglist mine=*larg;
      mine.iap=&local;
      worker.assembly.resize(larg->ndim, larg->ncol, larg->nbc, larg->nint, worker.parameter_count);
      mine.loop_start=larg->loop_start+begin;
      mine.loop_end=larg->loop_start+end;
      work(&mine);
    });
    run.collocation->publish_rhs(run);
  } else work(larg);

}

void setubv_make_aa_bb_cc(const setubv_parallel_arglist *larg)
{
  std::ptrdiff_t ups_dim1,
    uoldps_dim1, udotps_dim1, upoldp_dim1,
    dbc_dim1, dicd_dim1;

  std::ptrdiff_t i, j, k, m;
  std::ptrdiff_t k1;
  std::ptrdiff_t i1,j1;

  std::ptrdiff_t jj;
  std::ptrdiff_t jp1;

  double *ups = larg->ups;
  double *upoldp = larg->upoldp;
  double *udotps = larg->udotps;
  double *uoldps = larg->uoldps;

  const xpp::Matrix3<double> cc = larg->cc;

  auto &dicd = larg->iap->run->assembly.dicd;
  auto &ficd = larg->iap->run->assembly.ficd;
  auto &dbc = larg->iap->run->assembly.dbc;
  auto &fbc = larg->iap->run->assembly.fbc;
  auto &uic = larg->iap->run->assembly.uic;
  auto &uio = larg->iap->run->assembly.uio;
  auto &uid = larg->iap->run->assembly.uid;
  auto &uip = larg->iap->run->assembly.uip;
  auto &ubc0 = larg->iap->run->assembly.ubc0;
  auto &ubc1 = larg->iap->run->assembly.ubc1;

  upoldp_dim1 = larg->ndxloc;
  udotps_dim1 = larg->ndxloc;
  uoldps_dim1 = larg->ndxloc;
  ups_dim1 = larg->ndxloc;
  dicd_dim1 = larg->nint;
  dbc_dim1 = larg->nbc;

  dispatch_mesh(larg,setubv_make_fa_aa_bb);

  /*     Generate CC : */
  
  /*     Boundary conditions : */
  if (larg->nbc > 0) {
    for (i = 0; i < larg->ndim; ++i) {
      ubc0[i] = ARRAY2D(ups, 0, i);
      ubc1[i] = ARRAY2D(ups, larg->na, i);
    }
    
    (*(larg->boundary_function))(larg->iap, larg->rap, larg->ndim, larg->par, 
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
	
	(*(larg->integral_function))(larg->iap, larg->rap, larg->ndim, larg->par, 
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

/* setubv: XPPAUT's AUTO's SETUBV (solvebv.f90:293): the collocation system's blocks */
int 
setup_boundary_system(std::ptrdiff_t ndim, xpp::AutoxProblem ips, std::ptrdiff_t na, std::ptrdiff_t ncol, std::ptrdiff_t nbc, std::ptrdiff_t nint, std::ptrdiff_t ncb, std::ptrdiff_t nrc, std::ptrdiff_t nra, std::ptrdiff_t nca, 
       FUNI_TYPE((*problem_function)), BCNI_TYPE((*boundary_function)), ICNI_TYPE((*integral_function)), std::ptrdiff_t ndxloc, iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, 
       double rds, xpp::Matrix3<double> aa, xpp::Matrix3<double> bb, xpp::Matrix3<double> cc, xpp::Matrix<double> dd, double *fa, double *fc, double *rlcur, 
       double *rlold, double *rldot, double *ups, double *uoldps, double *udotps, double *upoldp, double *dups, 
       double *dtm, double *thl, double *thu, double *p0, double *p1)
{
  iap->run->assembly.resize(ndim, ncol, nbc, nint, iap->run->parameter_count);
  std::ptrdiff_t i, j, k;

  auto &wi = iap->run->assembly.wi;
  auto &wp = iap->run->assembly.wp;
  auto &wt = iap->run->assembly.wt;

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
					nrc, nra, nca, problem_function, integral_function, ndxloc, iap, rap, 
					par, icp, aa, bb, cc, dd, fa, fc, ups, 
					uoldps, udotps, upoldp, dtm, wp.data(), wt.data(), wi.data(), 
					thu, thl, rldot, boundary_function, &arglist);
  
    auto &residual_parameters = iap->run->assembly.residual_parameters;
    std::copy_n(par, iap->run->parameter_count, residual_parameters.begin());
    if (iap->run->setubv_stop && xpp::job::cancelled()) return 0;
    setubv_make_aa_bb_cc(&arglist);
    if (iap->run->setubv_stop && xpp::job::cancelled()) return 0;
    // Boundary differencing can change PAR's last bits (notably HomCont).
    // Only then is the residual from the Jacobian visit no longer reusable.
    if (std::memcmp(par, residual_parameters.data(), xpp::size_product(iap->run->parameter_count, sizeof(double))) != 0) {
      arglist.jacobian = false;
      dispatch_mesh(&arglist, setubv_make_fa_aa_bb);
      if (iap->run->setubv_stop && xpp::job::cancelled()) return 0;
    }
    setubv_make_fc_dd(arglist,dups,rlcur,rlold,rds);
  }

  return 0;
}

namespace {

void setubv_make_fc_dd(const setubv_parallel_arglist &larg, double *dups, double *rlcur, 
	     double *rlold, double rds) {
  std::ptrdiff_t i,j,jj,jp1,k,i1,m,j1;
  double rlsum;

  std::ptrdiff_t dups_dim1 = larg.ndxloc;
  
  const xpp::Matrix<double> dd = larg.dd;

  double *ups = larg.ups;
  std::ptrdiff_t ups_dim1 = larg.ndxloc;

  double *uoldps = larg.uoldps;
  std::ptrdiff_t uoldps_dim1 = larg.ndxloc;
  
  double *udotps = larg.udotps;
  std::ptrdiff_t udotps_dim1 = larg.ndxloc;
  
  double *upoldp = larg.upoldp;
  std::ptrdiff_t upoldp_dim1 = larg.ndxloc;
  
  std::ptrdiff_t dbc_dim1 = larg.nbc;
  auto &dbc = larg.iap->run->assembly.dbc;
  auto &fbc = larg.iap->run->assembly.fbc;
  auto &ubc0 = larg.iap->run->assembly.ubc0;
  auto &ubc1 = larg.iap->run->assembly.ubc1;
  std::ptrdiff_t dicd_dim1 = larg.nint;
  auto &dicd = larg.iap->run->assembly.dicd;
  auto &ficd = larg.iap->run->assembly.ficd;
  auto &uic = larg.iap->run->assembly.uic;
  auto &uio = larg.iap->run->assembly.uio;
  auto &uid = larg.iap->run->assembly.uid;
  auto &uip = larg.iap->run->assembly.uip;

  /* Boundary condition part of FC */
  if (larg.nbc > 0) {
    for (i = 0; i < larg.ndim; ++i) {
      ubc0[i] = ARRAY2D(ups, 0, i);
      ubc1[i] = ARRAY2D(ups, larg.na, i);
    }
    
    (*(larg.boundary_function))(larg.iap, larg.rap, larg.ndim, larg.par, 
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
	
	(*(larg.integral_function))(larg.iap, larg.rap, larg.ndim, larg.par, 
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

  larg.fc[larg.nrc-1] = rds - mesh_inner_product(larg.iap, &(larg.ndim), &(larg.ndxloc), larg.udotps, dups, larg.dtm, larg.thu) - rlsum;

}

/* Fill in a setubv_parallel_arglist for the individual variables */
void setubv_parallel_arglist_constructor(std::ptrdiff_t ndim, std::ptrdiff_t na, std::ptrdiff_t ncol, 
					 std::ptrdiff_t nbc, std::ptrdiff_t nint, std::ptrdiff_t ncb, std::ptrdiff_t nrc, std::ptrdiff_t nra, std::ptrdiff_t nca, 
					 FUNI_TYPE((*problem_function)), ICNI_TYPE((*integral_function)), std::ptrdiff_t ndxloc, iap_type *iap, rap_type *rap, double *par, 
					 std::ptrdiff_t *icp, xpp::Matrix3<double> aa, xpp::Matrix3<double> bb, 
					 xpp::Matrix3<double> cc, xpp::Matrix<double> dd, double *fa, double *fc, double *ups, 
					 double *uoldps, double *udotps, 
					 double *upoldp, double *dtm, 
					 double *wp, double *wt, double *wi,
					 double *thu, double *thl,
					 double *rldot, BCNI_TYPE((*boundary_function)), setubv_parallel_arglist *data) {
  data->ndim   = ndim;
  data->ncol   = ncol;
  data->nbc    = nbc;
  data->nint   = nint;
  data->ncb    = ncb;
  data->nrc    = nrc;
  data->nra    = nra;
  data->nca    = nca;
  data->na     = na;
  data->problem_function   = problem_function;
  data->integral_function   = integral_function;
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
  data->boundary_function   = boundary_function;
}  
} // namespace
