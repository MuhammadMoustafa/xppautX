#include "xpp_mem.h"
#include <vector>
#include <cstring>
#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include "autox_c.h"
#include "autox_state.h"
#include "xpp_job.h" /* xppautX: cancel */
#include "autox_codes.h"
#include "autox_f2c.h"
#include "autox_parallel.h"
#include "xpp_math.h"

namespace {
/* This structure contains all of the input data for the setup_boundary_system routine
   Those values which are arrays and those
   which are input and output are markered as such*/
struct setubv_parallel_arglist {
  std::ptrdiff_t ndim, ncol, nbc, nint, ncb, nrc, nra, nca, na; /*scalar input */
  ProblemFunction problem_function; /*scalar input*/
  IntegralFunction integral_function; /*scalar input*/
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
  BoundaryFunction boundary_function;
  bool jacobian = true;
  /* Set when the integral conditions are pure (integral_conditions_are_pure): each interval's workers evaluate them once, as the
     Jacobian is built, into cc and into these terms of fc and dd (the product added to each, by interval, point, condition:
     fc's, then dd's ncb), which setubv_make_fc_dd adds in that order, the serial loop's. Null: both loops call them serially. */
  double *integral_terms = nullptr;
};

/* setup_boundary_system's steps (the original autoX also ran them on pthreads or MPI
   workers). */
void setubv_make_fc_dd(const setubv_parallel_arglist &larg, double *dups, double *rlcur,
		       double *rlold, double rds);
void setubv_parallel_arglist_constructor(std::ptrdiff_t ndim, std::ptrdiff_t na, std::ptrdiff_t ncol,
					 std::ptrdiff_t nbc, std::ptrdiff_t nint, std::ptrdiff_t ncb, std::ptrdiff_t nrc, std::ptrdiff_t nra, std::ptrdiff_t nca,
					 ProblemFunction problem_function, IntegralFunction integral_function, std::ptrdiff_t ndxloc, iap_type *iap, rap_type *rap, double *par,
					 std::ptrdiff_t *icp, xpp::Matrix3<double> aa, xpp::Matrix3<double> bb,
					 xpp::Matrix3<double> cc, xpp::Matrix<double> dd, double *fa, double *fc, double *ups,
					 double *uoldps, double *udotps,
					 double *upoldp, double *dtm,
					 double *wp, double *wt, double *wi,
					 double *thu, double *thl, double *rldot, BoundaryFunction boundary_function,
					 setubv_parallel_arglist *data);

/* Calls the integral conditions at each of interval jj's ncol + 1 collocation points, on larg's own run's arrays, and
   hands visit(k) their values (that run's ficd and dicd). */
template <class Visit>
void for_integral_points(const setubv_parallel_arglist &larg, std::ptrdiff_t jj, double *par, Visit &&visit)
{
  const std::ptrdiff_t ups_dim1 = larg.ndxloc, uoldps_dim1 = larg.ndxloc, udotps_dim1 = larg.ndxloc, upoldp_dim1 = larg.ndxloc;
  const double *ups = larg.ups, *uoldps = larg.uoldps, *udotps = larg.udotps, *upoldp = larg.upoldp;
  auto &a = larg.iap->run->assembly;
  for (std::ptrdiff_t k = 0; k <= larg.ncol; ++k) {
    const bool last = k == larg.ncol;
    const std::ptrdiff_t j1 = (last ? jj + 1 : jj) + larg.loop_offset;
    for (std::ptrdiff_t i = 0; i < larg.ndim; ++i) {
      const std::ptrdiff_t i1 = last ? i : k * larg.ndim + i;
      a.uic[i] = ARRAY2D(ups, j1, i1);
      a.uio[i] = ARRAY2D(uoldps, j1, i1);
      a.uid[i] = ARRAY2D(udotps, j1, i1);
      a.uip[i] = ARRAY2D(upoldp, j1, i1);
    }
    (*larg.integral_function)(larg.iap, larg.rap, larg.ndim, par, larg.icp, larg.nint,
        a.uic.data(), a.uio.data(), a.uid.data(), a.uip.data(), a.ficd.data(), 2, a.dicd.data());
    visit(k);
  }
}

/* The integral conditions' rows of cc at interval jj's collocation point k, from the point's dicd. */
void set_integral_rows(const setubv_parallel_arglist &larg, std::ptrdiff_t jj, std::ptrdiff_t k)
{
  const double *dicd = larg.iap->run->assembly.dicd.data();
  const std::ptrdiff_t dicd_dim1 = larg.nint;
  for (std::ptrdiff_t m = 0; m < larg.nint; ++m)
    for (std::ptrdiff_t i = 0; i < larg.ndim; ++i)
      larg.cc(k * larg.ndim + i, larg.nbc + m, jj) = larg.dtm[jj] * larg.wi[k] * ARRAY2D(dicd, m, i);
}

/* Interval jj's integral conditions on a worker: cc's rows and the terms of fc and dd (see setubv_parallel_arglist). */
void set_integral_terms(const setubv_parallel_arglist &larg, std::ptrdiff_t jj)
{
  auto &a = larg.iap->run->assembly;
  const double *dicd = a.dicd.data();
  const std::ptrdiff_t dicd_dim1 = larg.nint;
  double *terms = larg.integral_terms + jj * (larg.ncol + 1) * larg.nint * (1 + larg.ncb);
  std::copy_n(larg.par, larg.iap->run->parameter_count, a.prm.begin());
  for_integral_points(larg, jj, a.prm.data(), [&](std::ptrdiff_t k) {
    set_integral_rows(larg, jj, k);
    for (std::ptrdiff_t m = 0; m < larg.nint; ++m) {
      *terms++ = larg.dtm[jj] * larg.wi[k] * a.ficd[m];
      for (std::ptrdiff_t i = 0; i < larg.ncb; ++i)
        *terms++ = larg.dtm[jj] * larg.wi[k] * ARRAY2D(dicd, m, larg.ndim + larg.icp[i]);
    }
  });
}

/* The pseudo-arclength equation's row of cc, interval jj's part. */
void set_arclength_row(const setubv_parallel_arglist &larg, std::ptrdiff_t jj)
{
  const std::ptrdiff_t udotps_dim1 = larg.ndxloc;
  const double *udotps = larg.udotps;
  for (std::ptrdiff_t i = 0; i < larg.ndim; ++i) {
    for (std::ptrdiff_t k = 0; k < larg.ncol; ++k) {
      const std::ptrdiff_t k1 = k * larg.ndim + i;
      larg.cc(k1, larg.nrc - 1, jj) = larg.dtm[jj] * larg.thu[i] * larg.wi[k] * ARRAY2D(udotps, jj + larg.loop_offset, k1);
    }
    larg.cc(larg.nra + i, larg.nrc - 1, jj) = larg.dtm[jj] * larg.thu[i] * larg.wi[larg.ncol] * ARRAY2D(udotps, jj + 1 + larg.loop_offset, i);
  }
}

/* The integral conditions that read nothing but their arguments, the run's rotation counts and the parameters, which they leave
   as they are: the periodic family's (with their boundary conditions, analytic too), so a worker may evaluate them, once, in any order. */
bool integral_conditions_are_pure(IntegralFunction f)
{
  return f == periodic_integral_conditions || f == periodic_fold_integral_conditions
      || f == period_doubling_integral_conditions || f == torus_integral_conditions;
}

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
    if (!larg->iap->run->owner && larg->iap->run->setubv_stop && autox_cancelled(*larg->iap->run)) break; /* xppautX: cancel (W752: the rule of autox_cancelled) */
    j = jj;
    jp1 = j + 1;
    if (larg->jacobian) {
      // this interval's blocks start at zero (the later stages fill what they set)
      std::ranges::fill(larg->aa.plane(jj), 0.);
      std::ranges::fill(larg->bb.plane(jj), 0.);
      std::ranges::fill(larg->cc.plane(jj), 0.);
    }
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
    if (larg->jacobian) {
      set_arclength_row(*larg, jj);
      if (larg->integral_terms) set_integral_terms(*larg, jj);
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
  std::ptrdiff_t dbc_dim1;
  std::ptrdiff_t i, k;
  std::ptrdiff_t jj;

  double *ups = larg->ups;
  const std::ptrdiff_t ups_dim1 = larg->ndxloc;

  const xpp::Matrix3<double> cc = larg->cc;

  auto &dbc = larg->iap->run->assembly.dbc;
  auto &fbc = larg->iap->run->assembly.fbc;
  auto &ubc0 = larg->iap->run->assembly.ubc0;
  auto &ubc1 = larg->iap->run->assembly.ubc1;

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
  
  /*     Integral constraints (the pseudo-arclength equation's row is set by each interval's collocation) : */
  if (larg->nint > 0 && !larg->integral_terms) {
    for (jj = larg->loop_start; jj < larg->loop_end; ++jj)
      for_integral_points(*larg, jj, larg->par, [&](std::ptrdiff_t k) { set_integral_rows(*larg, jj, k); });
  }
}

} // namespace

/* setubv: XPPAUT's AUTO's SETUBV (solvebv.f90:293): the collocation system's blocks */
int 
setup_boundary_system(std::ptrdiff_t ndim, xpp::AutoxProblem ips, std::ptrdiff_t na, std::ptrdiff_t ncol, std::ptrdiff_t nbc, std::ptrdiff_t nint, std::ptrdiff_t ncb, std::ptrdiff_t nrc, std::ptrdiff_t nra, std::ptrdiff_t nca, 
       ProblemFunction problem_function, BoundaryFunction boundary_function, IntegralFunction integral_function, std::ptrdiff_t ndxloc, iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, 
       double rds, xpp::Matrix3<double> aa, xpp::Matrix3<double> bb, xpp::Matrix3<double> cc, xpp::Matrix<double> dd, double *fa, double *fc, double *rlcur, 
       double *rlold, double *rldot, double *ups, double *uoldps, double *udotps, double *upoldp, double *dups, 
       double *dtm, double *thl, double *thu, double *p0, double *p1)
{
  iap->run->assembly.resize(ndim, ncol, nbc, nint, iap->run->parameter_count);
  std::ptrdiff_t i, k;

  auto &wi = iap->run->assembly.wi;
  auto &wp = iap->run->assembly.wp;
  auto &wt = iap->run->assembly.wt;

  /* Initialize to zero (the blocks of aa, bb and cc are each interval's own, zeroed where it is set up). */
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
  
    if (nint > 0 && integral_conditions_are_pure(integral_function)) {
      iap->run->assembly.integral_terms.resize(xpp::size_product(na, ncol + 1, nint, 1 + ncb));
      arglist.integral_terms = iap->run->assembly.integral_terms.data();
    }
    auto &residual_parameters = iap->run->assembly.residual_parameters;
    std::copy_n(par, iap->run->parameter_count, residual_parameters.begin());
    if (iap->run->setubv_stop && autox_cancelled(*iap->run)) return 0;
    setubv_make_aa_bb_cc(&arglist);
    if (iap->run->setubv_stop && autox_cancelled(*iap->run)) return 0;
    // Boundary differencing can change PAR's last bits (notably HomCont).
    // Only then is the residual from the Jacobian visit no longer reusable.
    if (std::memcmp(par, residual_parameters.data(), xpp::size_product(iap->run->parameter_count, sizeof(double))) != 0) {
      arglist.jacobian = false;
      dispatch_mesh(&arglist, setubv_make_fa_aa_bb);
      if (iap->run->setubv_stop && autox_cancelled(*iap->run)) return 0;
    }
    setubv_make_fc_dd(arglist,dups,rlcur,rlold,rds);
  }

  return 0;
}

namespace {

void setubv_make_fc_dd(const setubv_parallel_arglist &larg, double *dups, double *rlcur, 
	     double *rlold, double rds) {
  std::ptrdiff_t i,j,jj,k,m;
  double rlsum;

  std::ptrdiff_t dups_dim1 = larg.ndxloc;
  
  const xpp::Matrix<double> dd = larg.dd;

  double *ups = larg.ups;
  std::ptrdiff_t ups_dim1 = larg.ndxloc;

  double *uoldps = larg.uoldps;
  std::ptrdiff_t uoldps_dim1 = larg.ndxloc;
  
  std::ptrdiff_t dbc_dim1 = larg.nbc;
  auto &dbc = larg.iap->run->assembly.dbc;
  auto &fbc = larg.iap->run->assembly.fbc;
  auto &ubc0 = larg.iap->run->assembly.ubc0;
  auto &ubc1 = larg.iap->run->assembly.ubc1;

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
  if (larg.nint > 0 && larg.integral_terms) {
    const double *term = larg.integral_terms;
    for (jj = larg.loop_start; jj < larg.loop_end; ++jj)
      for (k = 0; k <= larg.ncol; ++k)
        for (m = 0; m < larg.nint; ++m) {
          larg.fc[larg.nbc + m] -= *term++;
          for (i = 0; i < larg.ncb; ++i) dd(i, larg.nbc + m) += *term++;
        }
  } else if (larg.nint > 0) {
    for (jj = larg.loop_start; jj < larg.loop_end; ++jj)
      for_integral_points(larg, jj, larg.par, [&](std::ptrdiff_t k) {
        const auto &a = larg.iap->run->assembly;
        const double *dicd = a.dicd.data();
        const std::ptrdiff_t dicd_dim1 = larg.nint;
        for (std::ptrdiff_t m = 0; m < larg.nint; ++m) {
          larg.fc[larg.nbc + m] -= larg.dtm[jj] * larg.wi[k] * a.ficd[m];
          for (std::ptrdiff_t i = 0; i < larg.ncb; ++i)
            dd(i, larg.nbc + m) += larg.dtm[jj] * larg.wi[k] * ARRAY2D(dicd, m, larg.ndim + larg.icp[i]);
        }
      });
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
					 ProblemFunction problem_function, IntegralFunction integral_function, std::ptrdiff_t ndxloc, iap_type *iap, rap_type *rap, double *par, 
					 std::ptrdiff_t *icp, xpp::Matrix3<double> aa, xpp::Matrix3<double> bb, 
					 xpp::Matrix3<double> cc, xpp::Matrix<double> dd, double *fa, double *fc, double *ups, 
					 double *uoldps, double *udotps, 
					 double *upoldp, double *dtm, 
					 double *wp, double *wt, double *wi,
					 double *thu, double *thl,
					 double *rldot, BoundaryFunction boundary_function, setubv_parallel_arglist *data) {
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
