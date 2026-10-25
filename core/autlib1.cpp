#include "auto_unknowns.h"
#include "xpp_files.h"
#include <array>
#include <span>
#include <initializer_list>
#include <string>
#include <vector>
#include "xpp_mem.h"
#include "session.h"
#include "xpp_log.h"
#include "autevd.h"
#include "auto_stability.h"
#include "auto_stability_auto.h"
#include "xpp_ui.h" /* err_msg(), byeauto_() */
#include "auto_nox.h" /* auto_screen_col(), RestartLabel */
#include "xpp_job.h" /* xppautX: cancel */
#include "auto_stop.h" /* xppautX: why a branch ended (T23) */
#include "form_ode.h"
#include "model.h"
#include "xpp_math.h"

namespace {
struct AutoPointOutputContext {
  integer ndim;
  xpp::AutoProblem ips;
  integer isw, iplt, nmx, npr, ndm;
  xpp::AutoPointCode itp;
  xpp::AutoPointKind itpst;
  integer ibr;
  doublereal rl0, rl1, a0, a1;
};
AutoPointOutputContext point_output_context(const iap_type &iap, const rap_type &rap) {
  return {iap.ndim, iap.ips, iap.isw, iap.iplt, iap.nmx, iap.npr,
          iap.ndm, iap.itp, iap.itpst, iap.ibr, rap.rl0, rap.rl1, rap.a0, rap.a1};
}

struct AutoIntegralContext {
  integer ndim, ntst, ncol;
  std::span<const doublereal> weights;
};
AutoIntegralContext integral_context(const iap_type &iap) {
  auto &weights = iap.lib->scratch.integral_weights;
  if (weights.size() != xpp::size_sum(iap.ncol, 1)) {
    weights.resize(xpp::size_sum(iap.ncol, 1));
    wint(iap.ncol + 1, weights.data());
  }
  return {iap.ndim, iap.ntst, iap.ncol, weights};
}
} // namespace

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*                    Initialization */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/* xppautX: cancel: 1 while lcspae/lcspbv locate a special point. Their
   solves run to the end, as stdrbv's does (xpp_job.h): contae/contbv have
   already made the new, unstored point the one a cancelled solve returns
   to. The cancel then reaches the next ordinary solve, which returns to the
   last stored point, so a cancelled run always ends the same way (the EP
   repeating that point) and a script can replay it exactly. */

namespace {
/* the exact Jacobian failed in this run (autpp.cpp): the derivatives of
   the solve in progress are not the model's, so its step is not taken */
bool jacobian_failed(const iap_type *iap) { return iap->lib->session->auto_lib.jacobian_failure.has_value(); }

/* the user cancelled, or the exact Jacobian failed: what ends a run where
   anything else (a solve that does not converge) only gives a point up */
bool stop_asked(const iap_type *iap) { return xpp::job::cancelled() || jacobian_failed(iap); }

/* auto_locating = 1 for a scope: cleared however it ends, a failed solve
   (xpp::AutoFailed) included, so the next run can be cancelled again */
struct Locating {
  AutoLib &lib;
  explicit Locating(AutoLib &l) : lib(l) { lib.locating = 1; }
  ~Locating() { lib.locating = 0; }
  Locating(const Locating &) = delete;
  Locating &operator=(const Locating &) = delete;
};

/* A failed location must leave the converged point and the data its next
   test/step consumes intact. Scratch factorizations matter too: fnlpbv uses
   solvbv with ifst=0. Only allocate these owned copies after a sign change. */
struct LocationState {
  struct Array {
    std::span<double> target;
    std::vector<double> values;
    explicit Array(std::span<double> a) : target(a), values(a.begin(), a.end()) {}
    void restore() const { std::copy(values.begin(), values.end(), target.begin()); }
  };
  std::vector<Array> arrays;
  double dsold, det, tivp;
  integer nit, nins, isp;
  xpp::AutoStability::Values stability;
  std::vector<double> codim_left;
  decltype(AutoLib::solvbv) solvbv;
  decltype(AutoLib::homcont) homcont;

  LocationState(const iap_type &iap, const rap_type &rap,
                std::initializer_list<std::span<double>> data)
    : dsold(rap.dsold), det(rap.det), tivp(rap.tivp),
      nit(iap.nit), nins(iap.nins), isp(iap.isp),
      stability(iap.lib->session->auto_state.stability.computed),
      codim_left(iap.lib->codim.previous_left),
      solvbv(iap.lib->solvbv), homcont(iap.lib->homcont)
  {
    arrays.reserve(data.size());
    for (auto a : data) arrays.emplace_back(a);
  }
  void restore(iap_type &iap, rap_type &rap) const {
    for (const auto &a : arrays) a.restore();
    rap.dsold = dsold;
    rap.det = det;
    rap.tivp = tivp;
    iap.nit = nit;
    iap.nins = nins;
    iap.isp = isp;
    iap.lib->session->auto_state.stability.computed = stability;
    iap.lib->codim.previous_left = codim_left;
    iap.lib->solvbv = solvbv;
    iap.lib->homcont = homcont;
  }
};

// W356 (#418): AUTO-07p CNRLAE ae.f90:199-214 / CNRLBV
// bvp.f90:287-305. A later location invalidates earlier tests' samples.
template<class Function> struct LocationTest {
  Function function;
  double &value;
  xpp::AutoPointKind kind;
  std::optional<integer> user_index;
};

template<class Function, class Evaluate, class Locate>
void locate_tests(iap_type &iap, rap_type &rap,
                  std::vector<LocationTest<Function>> &tests, std::span<const AutoMark> marks,
                  Evaluate evaluate, Locate locate) {
  double dstest = rap.dsold;
  std::size_t last_stepped = 0;
  std::optional<std::size_t> found;
  for (std::size_t i = 0; i < tests.size(); ++i) {
    auto &test = tests[i];
    if (test.user_index) iap.iuzr = *test.user_index;
    bool stepped = false;
    locate(test, dstest, stepped);
    if (stepped) last_stepped = i;
    if (iap.istop == 1) break;
    if (iap.itp.kind == xpp::AutoPointKind::Pending) {
      iap.itp = {test.kind, iap.itpst};
      found = i;
    }
  }
  for (std::size_t i = 0; i < last_stepped; ++i) {
    auto &test = tests[i];
    if (test.user_index) iap.iuzr = *test.user_index;
    test.value = evaluate(test);
  }
  if (iap.istop != 1 && found && tests[*found].user_index &&
      marks[*tests[*found].user_index].parameter < 0) iap.istop = -1;
}

// The arclength within which a location takes a zero as found: EPSS
// relative to the geometric mean of DS and DSMAX (AUTO-07p LCSPAE/LCSPBV).
double location_tolerance(const rap_type &rap) {
  return rap.epss * (1 + std::sqrt(std::abs(rap.ds * rap.dsmax)));
}

// AUTO-07p LCSPAE ae.f90:679-698 / LCSPBV bvp.f90:1309-1329:
// coincident bifurcations keep the first label, except that they override UZ.
// A test that does not apply at either point brackets nothing (W419).
bool location_sign_change(iap_type &iap, const rap_type &rap,
                          double q0, double &q1, double dstest,
                          bool enabled, bool user_zero) {
  if (!auto_test_applies(q0) || !auto_test_applies(q1)) return false;
  if (iap.itp.kind != xpp::AutoPointKind::Regular &&
      std::abs(auto_hmach1 * q1 * dstest) < location_tolerance(rap) * std::abs(q0 - q1)) {
    if (iap.itp.kind == xpp::AutoPointKind::UserZero && enabled && !user_zero)
      iap.itp = {xpp::AutoPointKind::Pending};
    q1 = 0;
  }
  return !((q0 >= 0 && q1 >= 0) || (q0 <= 0 && q1 <= 0) || !enabled);
}

// W405 (#467): a torus curve's own equations are singular at its 1:1
// resonance (the pair meets the trivial multiplier of a cycle, or the fold
// branch of a map), and at its 1:2 (W419): where sin(theta) = 0 the
// equations of the pair's real and imaginary parts decouple. The solution
// there is not isolated and Newton does not converge to it. A location of a
// zero there aims half its tolerance short of it, so it ends on a converged
// point within that tolerance.
double location_step(const rap_type &rap, double rds, bool singular) {
  return singular ? rds - std::copysign(location_tolerance(rap) / 2, rds) : rds;
}

/* AutoLib::setubv_stop set for one solvbv, cleared the same way */
struct SetubvStop {
  AutoLib &lib;
  SetubvStop(AutoLib &l, int on) : lib(l) { lib.setubv_stop = on; }
  ~SetubvStop() { lib.setubv_stop = 0; }
  SetubvStop(const SetubvStop &) = delete;
  SetubvStop &operator=(const SetubvStop &) = delete;
};

/* fort.8, opened when the run writes its first label (wrtsp8, wrtbv8) and
   kept open for the rest of it (close_auto closes it). One that cannot be
   opened fails the run instead of exit(1), a server must outlive a bad
   HOME; a later run tries again. */
FILE *fort8(AutoLib &lib)
{
  if(!lib.fp8){
    lib.fp8.reset(xpp::files::open_stream(xpp::auto_fort_path(*lib.session,8),"w"));
    if(!lib.fp8)xpp::auto_fail(xpp::files::write_error("AUTO", xpp::auto_fort_path(*lib.session,8)));
  }
  return lib.fp8.get();
}
} // namespace

int init(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *thl, std::vector<doublereal> &thu_vec, std::span<AutoMark> marks)
{
  xpp::Session &s=*iap->lib->session;
  /* Local variables */
  integer nbif;
  integer iads, ndim, nicp, ncol, mxbf;
  doublereal epsl;
  integer nfpr, nins, iplt, nint, jtmp;
  doublereal epsu, epss;
  integer itmx, itnw, ntot, ipos, nwtn;
  doublereal tivp;
  integer ntst, iuzr;
  integer nuzr, i;
  doublereal dsold, dsmin, dsmax, a0, a1;
  integer istop;
  xpp::AutoPointKind itpst;
  doublereal ds, rl0, rl1;
  integer iad, jac, lab, nbc, iid, ibr, ndm;
  doublereal det;
  integer ilp, nit, isp, irs, npr, isw, nmx, nbc0, nnt0;
  xpp::AutoProblem ips;
  xpp::AutoPointCode itp;

  for (i = 0; i < iap->lib->parameter_count; ++i) {
    icp[i] = i;
    jtmp = iap->lib->parameter_count;
    icp[jtmp + i] = 0;
    par[i] = 0.;
    par[jtmp + i] = 0.;
    thl[icp[i]] = 1.;
  }

 ndim=s.auto_state.run.ndim;
  ips=s.auto_state.run.ips;
  irs=s.auto_state.run.irs;
  ilp=s.auto_state.run.ilp;

  thu_vec.assign(xpp::size_product(static_cast<size_t>(ndim), AutoState::state_copies), 1.);

  jtmp = iap->lib->parameter_count;
  nicp=s.auto_state.run.nicp;

  for (i = 0; i < nicp; ++i) {
    icp[jtmp+i]=s.auto_state.run.icp[i];

  }
  
  if (nicp > 0) {
    for (i = 0; i < nicp; ++i) {
      jtmp = iap->lib->parameter_count;
      icp[i] = icp[jtmp + i];
    }
  } else {
    nicp = 1;
    jtmp = iap->lib->parameter_count;
    icp[jtmp] = icp[0];
  }
      
  ntst=s.auto_state.run.ntst;
  ncol=s.auto_state.run.ncol;
  iad=s.auto_state.run.iad;
  isp=s.auto_state.run.isp;
  isw=s.auto_state.run.isw;
  iplt=s.auto_state.run.iplt;
  nbc=s.auto_state.run.nbc;
  nint=s.auto_state.run.nint;
  nmx=s.auto_state.run.nmx;
  rl0=s.auto_state.run.rl0;
  rl1=s.auto_state.run.rl1;
  a0=s.auto_state.run.a0;
  a1=s.auto_state.run.a1;  
  npr=s.auto_state.run.npr;
  mxbf=s.auto_state.run.mxbf;
  iid=s.auto_state.run.iid;
  itmx=s.auto_state.run.itmx;
  itnw=s.auto_state.run.itnw;
  nwtn=s.auto_state.run.nwtn;
  jac=s.auto_state.run.jac;
  epsl=s.auto_state.run.epsl;
  epss=s.auto_state.run.epss;
  epsu=s.auto_state.run.epsu;
  ds=s.auto_state.run.ds;
  dsmin=s.auto_state.run.dsmin;
  dsmax=s.auto_state.run.dsmax;
  iads=s.auto_state.run.iads;

  if(dsmin < 0.0) {
    xpp::log_auto("Warning : DSMIN less then 0.0, will use absolute value instead.");
    dsmin = f2c::abs(dsmin);
  }

  if(dsmax < 0.0) {
    xpp::log_auto("Warning : DSMAX less then 0.0, will use absolute value instead.");
    dsmax = f2c::abs(dsmax);
  }
  for (const AutoRunWeight &weight : s.auto_state.run.weights)
    thl[weight.index]=weight.value;
    
  nuzr=static_cast<integer>(s.auto_state.run.marks.size());
  std::copy(s.auto_state.run.marks.begin(),s.auto_state.run.marks.end(),marks.begin());

  iap->ndim = ndim;

  iap->ips = ips;

  iap->irs = irs;

  iap->ilp = ilp;

  iap->ntst = ntst;

  iap->ncol = ncol;

  iap->iad = iad;

  iap->iads = iads;

  iap->isp = isp;

  iap->isw = isw;

  iap->iplt = iplt;

  iap->nbc = nbc;

  iap->nint = nint;

  iap->nmx = nmx;

  iap->nuzr = nuzr;

  iap->npr = npr;

  iap->mxbf = mxbf;

  iap->iid = iid;

  iap->itmx = itmx;

  iap->itnw = itnw;

  iap->nwtn = nwtn;

  iap->jac = jac;

  ndm = ndim;

  if (nbc != 0) {

    nbc0 = nbc;

  } else {

    nbc0 = 1;

  }

  if (nint != 0) {

    nnt0 = nint;

  } else {

    nnt0 = 1;

  }

  iuzr = 1;

  itp = xpp::AutoPointCode{};

  itpst = xpp::AutoPointKind::Regular;

  nfpr = 1;

  ibr = 1;

  nit = 0;

  ntot = 0;

  nins = 0;

  istop = 0;

  nbif = 0;

  ipos = 1;

  lab = 0;

  iap->ndm = ndm;

  iap->nbc0 = nbc0;

  iap->nnt0 = nnt0;

  iap->iuzr = iuzr;

  iap->itp = itp;

  iap->itpst = itpst;

  iap->nfpr = nfpr;

  iap->ibr = ibr;

  iap->nit = nit;

  iap->ntot = ntot;

  iap->nins = nins;

  iap->istop = istop;

  iap->nbif = nbif;

  iap->ipos = ipos;

  iap->lab = lab;

  iap->nicp = nicp;

  rap->ds = ds;

  rap->dsmin = dsmin;

  rap->dsmax = dsmax;

  dsold = ds;

  rap->dsold = dsold;

  rap->rl0 = rl0;

  rap->rl1 = rl1;

  rap->a0 = a0;

  rap->a1 = a1;

  det = 0.;

  tivp = 0.;

  rap->epsl = epsl;

  rap->epsu = epsu;

  rap->epss = epss;

  rap->det = det;

  rap->tivp = tivp;

  return 0;

} 

/*     ---------- ----- */
/* Subroutine */ int 
chdim(iap_type *iap)
{
  /* Local variables */
  integer npar;

/* Check dimensions. */

  npar = iap->nfpr;

  if (npar > iap->lib->parameter_count) {
    xpp::auto_fail(xpp::format("dimension exceeded: {} free parameters, at most {}", npar, iap->lib->parameter_count));
  }

  return 0;
} /* chdim_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*               The leading subroutines of AUTO */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

int autoae(iap_type *iap, rap_type *rap, doublereal *par, 
	   integer *icp, 
	   FUNI_TYPE((*funi)),
	   STPNT_TYPE_AE((*stpnt)),
	   PVLI_TYPE_AE((*pvli)),
	   doublereal *thl, doublereal *thu, std::span<const AutoMark> marks)

{
  /* This is the entry subroutine for algebraic systems. */

  cnrlae(iap, rap, par, icp, funi, stpnt, pvli, thl, thu, marks);
  return 0;
} 

int 
autobv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), STPNT_TYPE_BVP((*stpnt)), PVLI_TYPE_BVP((*pvli)), doublereal *thl, doublereal *thu, std::span<const AutoMark> marks)
{

  /* THIS IS THE ENTRY ROUTINE FOR GENERAL BOUNDARY VALUE PROBLEMS. */

  cnrlbv(iap, rap, par, icp, funi, bcni, icni, stpnt, pvli, thl, thu, marks);
  return 0;
} 

/*     ---------- ----- */
/* Subroutine */ int 
init1(iap_type *iap, rap_type *rap, integer *icp, doublereal *par)
{

    /* Local variables */
  integer ndim, ncol, nicp;

  integer nneg, nfpr, nint, jtmp, nuzr, i;
  doublereal dsmin, dsmax, fc;
  integer ic, jc;
  doublereal ds;
  integer nxploc, jac, nbc, ndm, ilp, isp, irs,
    isw, nmx;
  xpp::AutoProblem ips;
  xpp::AutoPointCode itp;

  std::vector<integer> ict(iap->lib->parameter_count);

  /* General initialization. Redefinition of constants. */

  ndim = iap->ndim;
  ips = iap->ips;
  irs = iap->irs;
  ilp = iap->ilp;
  ncol = iap->ncol;
  isp = iap->isp;
  isw = iap->isw;
  nbc = iap->nbc;
  nint = iap->nint;
  nmx = iap->nmx;
  nuzr = iap->nuzr;
  jac = iap->jac;
  itp = iap->itp;
  nfpr = iap->nfpr;
  nicp = iap->nicp;

  ds = rap->ds;
  dsmin = rap->dsmin;
  dsmax = rap->dsmax;

  if (isw == 0) {
    isw = 1;
  }

  /* Check and perturb pseudo arclength stepsize and steplimits. */
  /* (Perturbed to avoid exact computation of certain singular points). */

  if (ds == 0.) {
    ds = .1;
  }
  if (dsmin == 0.) {
    dsmin = f2c::abs(ds) * 1e-4;
  }
  fc = auto_hmach1;
  ds = fc * ds;
  dsmin /= fc;
  dsmax = fc * dsmax;

  const bool reduced_hopf = ips == xpp::AutoProblem::Algebraic || ips == xpp::AutoProblem::SteadyState;

  /* Redefinition for waves */
  if (ips == xpp::AutoProblem::WaveSteady) {
    ips = xpp::AutoProblem::SteadyState;
    iap->ips = ips;
    ndim <<= 1;
    ndm = ndim;
    iap->ndm = ndm;
  } else if (ips == xpp::AutoProblem::WavePeriodic) {
    ips = xpp::AutoProblem::Periodic;
    iap->ips = ips;
    ndim <<= 1;
    ndm = ndim;
    iap->ndm = ndm;
  }

  /* General Redefinition. */

  if (xpp::problem_is_algebraic_family(ips) && isw == 1) {
    /*        ** Algebraic Systems */
    nfpr = 1;

  } else if (ips == xpp::AutoProblem::TimeIntegration) {
    /*        ** Time integration */
    nfpr = 1;
    isp = 0;
    ilp = 0;
    icp[0] = xpp::auto_unknown::evolution_time;

  } else if (ips == xpp::AutoProblem::Periodic && f2c::abs(isw) == 1) {
    /*        ** Periodic Solutions */
    nbc = ndim;
    nint = 1;
    nfpr = nbc + nint - ndim + 1;
    /*        **ISW=1 when starting from a HB */
    if (itp.is(xpp::AutoPointKind::Hopf) || itp.started_from(xpp::AutoPointKind::Hopf)) {
      isw = 1;
    }
    if (nicp == 1) {
      /*          **Variable period */
      icp[1] = xpp::auto_unknown::period;
    }

  } else if (ips == xpp::AutoProblem::BoundaryValue && f2c::abs(isw) == 1) {
    /*        ** Boundary value problems */
    nfpr = nbc + nint - ndim + 1;

  } else if (ips == xpp::AutoProblem::FloquetBoundaryValue && f2c::abs(isw) == 1) {
    /*        ** Boundary value problems */
    nfpr = nbc + nint - ndim + 1;

  } else if (ips == xpp::AutoProblem::Homoclinic && f2c::abs(isw) == 1) {
    /*        ** Homoclinic bifurcation analysis */
    /*        Redefine AUTO constants for homoclinic orbits */
    inho(iap, icp, par);
    ndim = iap->ndim;
    nbc = iap->nbc;
    nint = iap->nint;
    nuzr = iap->nuzr;
    nfpr = nbc + nint - ndim + 1;
  } else if (xpp::problem_is_parabolic_evolution(ips)) {
    /*        **Evolution calculations for Parabolic Systems */
    ndim <<= 1;
    nbc = ndim;
    nint = 0;
    nfpr = 1;
    ilp = 0;
    isp = 0;
    icp[0] = xpp::auto_unknown::evolution_time;

  } else if (ips == xpp::AutoProblem::ParabolicUserSteady) {
    /*        **Stationary calculations for Parabolic Systems */
    ndim <<= 1;
    nbc = ndim;
    nint = 0;
    nfpr = 1;

  } else if (ips == xpp::AutoProblem::PeriodicOptimization) {
    /*          ** Optimization of periodic solutions */
    for (i = 0; i < nicp; ++i) {
      ict[i] = icp[i];
    }
    nfpr = 0;
    for (i = 0; i < nicp; ++i) {
      if (ict[i] >= 0) {
	icp[nfpr] = ict[i];
	++nfpr;
      }
    }
    icp[nfpr] = xpp::auto_unknown::objective;
    icp[nfpr + 1] = xpp::auto_unknown::optimization_kappa;
    icp[nfpr + 2] = xpp::auto_unknown::optimization_gamma;
    nfpr += 3;
    ndim <<= 1;
    nbc = ndim;
    nint = nfpr - 1;
    /* overload to define optimality integrals */
    nneg = 0;
    for (i = 0; i < nicp; ++i) {
      ic = ict[i];
      jc = f2c::abs(ic) - xpp::auto_unknown::test_function_start;
      if (ic < 0 && jc > 0 && jc <= 11) {
	++nneg;
	icp[nfpr + nneg - 1] = jc;
      }
    }
    /* Set indices of output parameters */
    nicp = nfpr - 3;
    for (i = 0; i < nicp; ++i) {
      jtmp = iap->lib->parameter_count;
      icp[jtmp + i] = icp[i];
    }

  } else if (ips == xpp::AutoProblem::AlgebraicOptimization) {
    /*        ** Algebraic optimization Problems */
    if (iap->itp.kind == xpp::AutoPointKind::Fold || iap->irs == 0) {
      iap->nfpr++;
    }
    nfpr = iap->nfpr;
    if (nfpr == 2) {
      ++ndim;
      icp[0] = xpp::auto_unknown::objective;
    } else {
      ndim = (ndim << 1) + nfpr;
      icp[0] = xpp::auto_unknown::objective;
    }

  } else if (irs > 0 && f2c::abs(isw) == 2) {
    /*        ** Continuation of singular points */

    if ((itp.is(xpp::AutoPointKind::BranchPoint) || itp.started_from(xpp::AutoPointKind::BranchPoint) || itp.is(xpp::AutoPointKind::Fold) || itp.started_from(xpp::AutoPointKind::Fold) || (ips==xpp::AutoProblem::DiscreteMap && (itp.is(xpp::AutoPointKind::PeriodDoubling) || itp.started_from(xpp::AutoPointKind::PeriodDoubling))))
	&& xpp::problem_is_algebraic_family(ips)) {
      /*          ** Fold continuation (Algebraic Problems) */
      ndim = (ndim << 1) + 1;
      nfpr = 2;

    } else if ((itp.is(xpp::AutoPointKind::Hopf) || itp.started_from(xpp::AutoPointKind::Hopf) || (ips==xpp::AutoProblem::DiscreteMap && (itp.is(xpp::AutoPointKind::Torus) || itp.started_from(xpp::AutoPointKind::Torus)))) && xpp::problem_is_algebraic_family(ips)) {
      /*          ** Hopf bifurcation continuation (Maps, ODE, Waves) */
      // Equilibria and maps use the reduced real-vector system; waves keep their own formulation.
      ndim = reduced_hopf || ips==xpp::AutoProblem::DiscreteMap ? ndim * 2 + 2 : ndim * 3 + 2;
      nfpr = 2;

    } else if ((itp.is(xpp::AutoPointKind::PeriodicFold) || itp.is(xpp::AutoPointKind::PeriodicBranch)) && ips == xpp::AutoProblem::Periodic) {
      /*          ** Fold continuation (Periodic solutions); start */
      ndim <<= 1;
      nbc = ndim;
      nint = 3;
      nfpr = nbc + nint - ndim + 1;
      if (icp[2] == xpp::auto_unknown::period || nicp == 2) {
	/*            ** Variable period */
	icp[1] = xpp::auto_unknown::fold_norm;
	icp[2] = xpp::auto_unknown::period;
	icp[3] = xpp::auto_unknown::fold_beta;
      } else {
	/*            ** Fixed period */
	icp[2] = xpp::auto_unknown::fold_norm;
	icp[3] = xpp::auto_unknown::fold_beta;
      }
      ilp = 0;
      isw = -2;
      isp = 0;
      nmx = 5;
      xpp::log_auto("\nGenerating starting data :\n Restart at EP label below :\n");
      iap->lib->restart_flag=1;

    } else if ((itp.started_from(xpp::AutoPointKind::PeriodicFold) || itp.started_from(xpp::AutoPointKind::PeriodicBranch)) && ips == xpp::AutoProblem::Periodic) {
      /*          ** Fold continuation (Periodic solutions); restart */
      ndim <<= 1;
      nbc = ndim;
      nint = 3;
      nfpr = nbc + nint - ndim + 1;
      if (nicp == 2) {
	/*            ** Variable period */
	icp[2] = xpp::auto_unknown::period;
      }
      icp[3] = xpp::auto_unknown::fold_beta;

    } else if (itp.is(xpp::AutoPointKind::PeriodDoubling) && ips == xpp::AutoProblem::Periodic) {
      /*          ** Continuation of period doubling bifurcations; start
 */
      ndim <<= 1;
      nbc = ndim;
      nint = 2;
      nfpr = nbc + nint - ndim + 1;
      if (icp[2] == xpp::auto_unknown::period || nicp == 2) {
	/*            ** Variable period */
	icp[1] = xpp::auto_unknown::period;
	icp[2] = xpp::auto_unknown::doubling_norm;
      } else {
	/*            ** Fixed period */
	icp[2] = xpp::auto_unknown::doubling_norm;
      }
      ilp = 0;
      isw = -2;
      isp = 0;
      nmx = 5;
      xpp::log_auto("\nGenerating starting data :\n Restart at EP label below :\n");
      iap->lib->restart_flag=1;

    } else if (itp.started_from(xpp::AutoPointKind::PeriodDoubling) && ips == xpp::AutoProblem::Periodic) {
      /*          ** Continuation of period doubling bifurcations; resta
rt */
      ndim <<= 1;
      nbc = ndim;
      nint = 2;
      nfpr = nbc + nint - ndim + 1;
      if (icp[2] == xpp::auto_unknown::period || nicp == 2) {
	/*            ** Variable period */
	icp[2] = xpp::auto_unknown::period;
      }

    } else if (itp.is(xpp::AutoPointKind::Torus) && ips == xpp::AutoProblem::Periodic) {
      /*          ** Continuation of torus bifurcations; start */
      ndim *= 3;
      nbc = ndim;
      nint = 3;
      nfpr = nbc + nint - ndim + 1;
      icp[1] = xpp::auto_unknown::period;
      icp[2] = xpp::auto_unknown::torus_angle;
      icp[3] = xpp::auto_unknown::torus_norm;
      ilp = 0;
      isp = 0;
      isw = -2;
      nmx = 5;
      xpp::log_auto("\nGenerating starting data :\n Restart at EP label below :\n");
      iap->lib->restart_flag=1;

    } else if (itp.started_from(xpp::AutoPointKind::Torus) && ips == xpp::AutoProblem::Periodic) {
      /*          ** Continuation of torus bifurcations; restart */
      ndim *= 3;
      nbc = ndim;
      nint = 3;
      nfpr = nbc + nint - ndim + 1;
      icp[2] = xpp::auto_unknown::period;
      icp[3] = xpp::auto_unknown::torus_angle;

    } else if ((itp.is(xpp::AutoPointKind::PeriodicFold) || itp.is(xpp::AutoPointKind::PeriodicBranch)) && ips == xpp::AutoProblem::BoundaryValue) {
      /*          ** Continuation of folds (BVP; start) */
      ndim <<= 1;
      nbc <<= 1;
      nint = (nint << 1) + 1;
      nfpr = nbc + nint - ndim + 1;
      nxploc = nfpr / 2 - 1;
      if (nxploc > 0) {
	for (i = 0; i < nxploc; ++i) {
	  icp[nfpr / 2 + i] = i + xpp::auto_unknown::bvp_fold_start;
	}
      }
      icp[nfpr / 2] = nfpr / 2 + xpp::auto_unknown::bvp_fold_start;
      ilp = 0;
      isw = -2;
      isp = 0;
      nmx = 5; 
      iap->lib->restart_flag=1;
      xpp::log_auto("\nGenerating starting data :\n Restart at EP label below :\n");

    } else if (itp.started_from(xpp::AutoPointKind::PeriodicFold) && ips == xpp::AutoProblem::BoundaryValue) {
      /* AUTO's own test names the fold twice, never the branch point: kept, so the selection is the same */
      /*          ** Continuation of folds (BVP; restart) */
      ndim <<= 1;
      nbc <<= 1;
      nint = (nint << 1) + 1;
      nfpr = nbc + nint - ndim + 1;
      nxploc = nfpr / 2 - 1;
      if (nxploc > 0) {
	for (i = 0; i < nxploc; ++i) {
	  icp[nfpr / 2 + i] = i + xpp::auto_unknown::bvp_fold_start;
	}
      }

    }

  }

  iap->ndim = ndim;
  iap->ips = ips;
  iap->irs = irs;
  iap->ilp = ilp;
  iap->ncol = ncol;
  iap->isp = isp;
  iap->isw = isw;
  iap->nbc = nbc;
  iap->nint = nint;
  iap->nmx = nmx;
  iap->nuzr = nuzr;
  iap->jac = jac;
  iap->nfpr = nfpr;
  iap->nicp = nicp;

  rap->ds = ds;
  rap->dsmin = dsmin;
  rap->dsmax = dsmax;
  return 0;
} /* init1 */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*                    Algebraic Problems */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/*     ---------- ------ */
/* Subroutine */ int 
cnrlae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), STPNT_TYPE_AE((*stpnt)), PVLI_TYPE_AE((*pvli)), doublereal *thl, doublereal *thu, std::span<const AutoMark> marks)
{

    /* Local variables */
  integer nbfc;
  integer nbif, iads;
  integer mxbf;

  std::vector<doublereal> stla;
  std::vector<doublereal> stld;
  integer nins;
  integer ipos;
  integer ntot, nuzr;
  integer i;
  doublereal dsold;

  std::vector<doublereal> rlold(iap->lib->parameter_count);
  std::vector<doublereal> rldot(iap->lib->parameter_count);
  std::vector<doublereal> rlcur(iap->lib->parameter_count);
  integer istop;
  xpp::AutoPointKind itpst;

  doublereal ds;

  integer lab, ibr;
  doublereal rbp, rds;
  xpp::AutoProblem ips;
  integer irs, isp;
  doublereal rev;
  doublereal rlp;
  integer nit;
  xpp::AutoPointCode itp;

  integer aa_first_dimension = iap->ndim + 1;

  std::vector<doublereal> dfdp(xpp::size_product(iap->ndim, iap->lib->parameter_count));
  std::vector<doublereal> dfdu(xpp::size_product(iap->ndim, iap->ndim));
  std::vector<doublereal> uold(iap->ndim);
  std::vector<doublereal> udot(iap->ndim);
  std::vector<doublereal> stud;
  std::vector<doublereal> f(iap->ndim);
  std::vector<doublereal> u(iap->ndim);
  std::vector<doublereal> aa(xpp::size_product(xpp::size_sum(iap->ndim, 1), xpp::size_sum(iap->ndim, 1)));
  std::vector<doublereal> du(xpp::size_sum(iap->ndim, 1));
  std::vector<doublereal> rhs(xpp::size_sum(iap->ndim, 1));
  std::vector<doublereal> stu;
  std::vector<doublereal> uzr(iap->nuzr);
    
  /* Controls the bifurcation analysis of algebraic problems */

/* Local */

  ips = iap->ips;
  irs = iap->irs;
  iads = iap->iads;
  isp = iap->isp;
  nuzr = iap->nuzr;
  mxbf = iap->mxbf;
  itpst = iap->itpst;
  ibr = iap->ibr;

  ds = rap->ds;

  nins = 0;
  iap->nins = nins;
  rbp = 0.;
  rev = 0.;
  rlp = 0.;
  if (nuzr > 0) {
    for (i = 0; i < nuzr; ++i) {
      uzr[i] = 0.;
    }
  }
  rds = ds;
  dsold = ds;
  rap->dsold = dsold;
  nit = 0;
  iap->nit = nit;
  nbif = 0;
  iap->nbif = nbif;
  nbfc = 0;
  ipos = 1;
  iap->ipos = ipos;
  ntot = 0;
  iap->ntot = ntot;
  lab = 0;
  iap->lab = lab;

  for (i = 0; i < iap->ndim; ++i) {
    u[i] = 0.;
    du[i] = 0.;
    udot[i] = 0.;
    uold[i] = 0.;
    f[i] = 0.;
  }

  /* Generate the starting point */

  iap->lib->codim.prepare(iap->ndm);
  double rbt=0, rcp=0, rgh=0;
  const auto special_points=iap->lib->session->auto_state.special_points;
  (*stpnt)(iap, rap, par, icp, u.data());
  (*pvli)(iap, rap, u.data(), par);

/* Determine a suitable starting label and branch number */

  newlab(iap, rap);

  /* Write constants */

  sthd(iap, rap, par, icp, thl, thu);

  /* Write plotting data for the starting point */

  istop = 0;
  iap->istop = istop;
  if (irs == 0) {
    itp = xpp::AutoPointCode{xpp::AutoPointKind::End, itpst};
  } else {
    itp = xpp::AutoPointCode{};
  }
  iap->itp = itp;
  rlcur[0] = par[icp[0]];
  stplae(iap, rap, par, icp, rlcur.data(), u.data());
  istop = iap->istop;
  if (istop == 1) {
    goto L6;
  }

  /* Starting procedure  (to get second point on first branch) : */

  stprae(iap, rap, par, icp, funi, &rds, &aa_first_dimension, aa.data(), 
	 rhs.data(), rlcur.data(), rlold.data(), rldot.data(), u.data(), du.data(), uold.data(), udot.data(), f.data(), dfdu.data(), dfdp.data(), thl, 
	 thu);
  istop = iap->istop;
  if (istop == 1) {
    goto L5;
  }
  itp = xpp::AutoPointCode{};
  iap->itp = itp;
  goto L3;

/* Initialize computation of the next bifurcating branch. */

 L2:
  swpnt(iap, rap, par, icp, &rds, stud.data(), stu.data(),
	stla.data(), stld.data(), rlcur.data(), rlold.data(), rldot.data(), u.data(), udot.data());

  ipos = iap->ipos;
  if (ipos == 1) {
    --nbif;
    iap->nbif = nbif;
    ++nbfc;
  }

  rbp = 0.;
  rev = 0.;
  rlp = 0.;
  if (nuzr > 0) {
    for (i = 0; i < nuzr; ++i) {
      uzr[i] = 0.;
    }
  }
  if (ipos == 0 || mxbf < 0) {
    ++ibr;
  }
  iap->ibr = ibr;

  ntot = 0;
  iap->ntot = ntot;
  istop = 0;
  iap->istop = istop;
  itp = xpp::AutoPointCode{};
  iap->itp = itp;
  nit = 0;
  iap->nit = nit;
  dsold = rds;
  rap->dsold = dsold;

/* Store plotting data for first point on the bifurcating branch */

  stplae(iap, rap, par, icp, rlcur.data(), u.data());
  istop = iap->istop;
  if (istop == 1) {
    goto L6;
  }

  /* Determine the second point on the bifurcating branch */

  swprc(iap, rap, par, icp, funi, &aa_first_dimension, aa.data(), rhs.data(), 
	rlcur.data(), rlold.data(), rldot.data(), u.data(), du.data(), uold.data(), udot.data(), f.data(), dfdu.data(), dfdp.data(), &rds, thl, 
	thu);
  istop = iap->istop;
  if (istop == 1) {
    goto L5;
  }

  /* Store plotting data for second point : */

  stplae(iap, rap, par, icp, rlcur.data(), u.data());
  istop = iap->istop;
  if (istop == 1) {
    goto L6;
  }
  rbp = 0.;
  rev = 0.;
  rlp = 0.;

/* Provide initial approximation to the next point on the branch */

 L3:
  contae(iap, rap, &rds, rlcur.data(), rlold.data(), rldot.data(), u.data(), uold.data(), udot.data());

/* Find the next solution point on the branch */

  solvae(iap, rap, par, icp, funi, &rds, &aa_first_dimension, aa.data(), 
	 rhs.data(), rlcur.data(), rlold.data(), rldot.data(), u.data(), du.data(), uold.data(), udot.data(), f.data(), dfdu.data(), dfdp.data(), thl, 
	 thu);
  istop = iap->istop;
  if (istop == 1) {
    goto L5;
  }

  {
    std::vector<LocationTest<decltype(&fnuzae)>> tests;
    for (integer index = 0; index < nuzr; ++index)
      tests.push_back({fnuzae, uzr[index], xpp::AutoPointKind::UserZero, index});
    if (iap->ilp == 1) tests.push_back({fnlpae, rlp, xpp::AutoPointKind::Fold, {}});
    if (isp != 0 && (iap->isw!=2 || ips==xpp::AutoProblem::DiscreteMap)) tests.push_back({fnbpae, rbp, xpp::AutoPointKind::BranchPoint, {}});
    const bool fold_curve=iap->isw==2 && iap->itpst==xpp::AutoPointKind::Fold;
    const bool hopf_curve=iap->isw==2 && iap->itpst==xpp::AutoPointKind::Hopf && iap->ndim==2*iap->ndm+2;
    const bool codim=ips==xpp::AutoProblem::SteadyState && (fold_curve || hopf_curve);
    if (codim && isp!=0 && isp!=3 && xpp::auto_codim_enabled(special_points,xpp::AutoPointKind::BogdanovTakens))
      tests.push_back({fnbtae,rbt,xpp::AutoPointKind::BogdanovTakens,{}});
    if (codim && isp!=0 && isp!=3 && fold_curve && xpp::auto_codim_enabled(special_points,xpp::AutoPointKind::Cusp))
      tests.push_back({fncpae,rcp,xpp::AutoPointKind::Cusp,{}});
    if (ips==xpp::AutoProblem::DiscreteMap && iap->isw==2 && isp!=0)
      tests.push_back({fnrnae,rbt,xpp::AutoPointKind::Pending,{}});
    if (xpp::problem_is_steady_or_map(ips))
      tests.push_back({fnhbae, rev, codim ? xpp::AutoPointKind::ZeroHopf : xpp::AutoPointKind::Hopf, {}});
    if (codim && isp!=0 && isp!=3 && hopf_curve && xpp::auto_codim_enabled(special_points,xpp::AutoPointKind::GeneralizedHopf))
      tests.push_back({fnghae,rgh,xpp::AutoPointKind::GeneralizedHopf,{}});
    locate_tests(*iap, *rap, tests, marks,
      [&](const auto &test) {
        logical changed;
        return test.function(iap, rap, par, icp, &changed, funi, &aa_first_dimension,
            aa.data(), rlcur.data(), rlold.data(), rldot.data(), u.data(), uold.data(),
            udot.data(), rhs.data(), dfdu.data(), dfdp.data(), marks);
      },
      [&](auto &test, double &dstest, bool &stepped) {
        lcspae(iap, rap, par, icp, test.function, funi, &aa_first_dimension,
            aa.data(), rhs.data(), rlcur.data(), rlold.data(), rldot.data(), u.data(),
            du.data(), uold.data(), udot.data(), f.data(), dfdu.data(), dfdp.data(),
            &test.value, thl, thu, marks, dstest, stepped);
        if (ips==xpp::AutoProblem::DiscreteMap && (test.function==fnhbae || test.function==fnrnae))
          test.kind=iap->lib->codim.map_kind;
      });
    if (iap->istop != 1 && iap->itp.kind == xpp::AutoPointKind::BranchPoint) {
      ++nbif;
      iap->nbif = nbif;
      stla.resize(nbif);
      stld.resize(nbif);
      stud.resize(xpp::size_product(iap->ndim, nbif));
      stu.resize(xpp::size_product(iap->ndim, nbif));
      stbif(iap, rap, par, icp, &aa_first_dimension, aa.data(),
	    stud.data(), stu.data(), stla.data(), stld.data(), rlcur.data(), rlold.data(), rldot.data(), u.data(),
	    du.data(), udot.data(), dfdu.data(), dfdp.data(), thl, thu);
    }
  }

  /* Store plotting data on unit 7 : */

 L5:
  stplae(iap, rap, par, icp, rlcur.data(), u.data());

/* Adapt the stepsize along the branch */

  itp = iap->itp;
  ntot = iap->ntot;
  if (iads != 0 && ntot % iads == 0 && (itp.kind == xpp::AutoPointKind::Regular || itp.kind == xpp::AutoPointKind::Output)) {
    adptds(iap, rap, &rds);
  }

 L6:
  itp = xpp::AutoPointCode{};
  iap->itp = itp;
  istop = iap->istop;
  if (istop == 0) {
    goto L3;
  }

  nbif = iap->nbif;
  if (nbif != 0 && nbfc < f2c::abs(mxbf)) {
    goto L2;
  }

  return 0;
} /* cnrlae_ */

/*     ---------- ------ */
/* Subroutine */ int 
stpnus(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *u)
{
  integer ndim;

  /* Gets the starting data from user supplied STPNT */

  ndim = iap->ndim;

  stpnt(*iap->lib->session, ndim, 0.0, u, par);

  return 0;
} /* stpnus_ */

/*     ---------- ------ */
/* Subroutine */ int 
stpnae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *u)
{
  logical found;
  integer nfprs;

  integer irs;

  /* Gets the starting data from unit 3 */

  irs = iap->irs;
  findlb(iap, rap, irs, &nfprs, &found);
  readlb(iap, rap, u, par);
  return 0;
} /* stpnae_ */

/*     ---------- ------ */
/* Subroutine */ int 
stprae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), doublereal *rds, integer *m1aaloc, doublereal *aa, doublereal *rhs, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *du, doublereal *uold, doublereal *udot, doublereal *f, doublereal *dfdu, doublereal *dfdp, doublereal *thl, doublereal *thu)
{
  xpp::auto_stability_step_start(iap->lib->session->auto_state.stability,
      iap->ibr, iap->ntot + (iap->lib->locating ? 1 : 0));

  /* System generated locals */
  integer aa_dim1;

  /* Local variables */
  integer ndim;

  doublereal sign;
  integer i, k;

  doublereal sc;
  doublereal ss;

  integer iid;

/* Finds the second point on the initial solution branch. */

  aa_dim1 = *m1aaloc;

  ndim = iap->ndim;
  iid = iap->iid;

  rlold[0] = par[icp[0]];
  for (i = 0; i < ndim; ++i) {
    uold[i] = u[i];
  }

  /* Determine the direction of the branch at the starting point */

  (*funi)(iap, rap, ndim, u, uold, icp, par, 2, f, dfdu, dfdp);
  for (i = 0; i < ndim; ++i) {
    rhs[i] = f[i];
    ARRAY2D(aa, i, ndim) = dfdp[(icp[0]) * ndim + i];
    ARRAY2D(aa, ndim, i) = 0.;
    for (k = 0; k < ndim; ++k) {
      ARRAY2D(aa, i, k) = dfdu[k * ndim + i];
    }
  }
  rhs[ndim] = 0.;
  ARRAY2D(aa, ndim, ndim) = 0.;

  if (iid >= 3) {
    integer tmp = ndim + 1;
    wrjac(iap, &tmp, m1aaloc, aa, rhs);
  }
  xpp::nlvc(iap->lib->fp9, ndim + 1, *m1aaloc, 1, aa, du);

  /* Scale and make sure that the PAR(ICP(1))-dot is positive. */

  ss = 0.;
  for (i = 0; i < ndim; ++i) {
    /* Computing 2nd power */
    ss += thu[i] * (du[i] * du[i]);
  }
  /* Computing 2nd power */
  ss += thl[icp[0]] * (du[ndim] * du[ndim]);

  sign = 1.;
  if (du[ndim] < 0.) {
    sign = -1.;
  }
  sc = sign / sqrt(ss);
  for (i = 0; i < ndim + 1; ++i) {
    du[i] = sc * du[i];
  }

  for (i = 0; i < ndim; ++i) {
    udot[i] = du[i];
  }
  rldot[0] = du[ndim];

/* Set initial approximations to the second point on the branch */

  for (i = 0; i < ndim; ++i) {
    u[i] = uold[i] + *rds * udot[i];
  }
  rlcur[0] = rlold[0] + *rds * rldot[0];

  solvae(iap, rap, par, icp, funi, rds, m1aaloc, aa,
	 rhs, rlcur, rlold, rldot, u, du, uold, 
	 udot, f, dfdu, dfdp, thl, thu);

  return 0;
} /* stprae_ */

/*     ---------- ------ */
/* Subroutine */ int 
contae(iap_type *iap, rap_type *rap, doublereal *rds, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *uold, doublereal *udot)
{
  xpp::auto_stability_step_start(iap->lib->session->auto_state.stability,
      iap->ibr, iap->ntot + (iap->lib->locating ? 1 : 0));

  /* Local variables */
  integer ndim, i;
  doublereal dsold;
  xpp::AutoProblem ips;

/* This subroutine determines an initial approximation to the next */
/* solution on a branch by extrapolating from the two preceding points. */
/* The step used in the preceding step has been stored in DSOLD. */

  ndim = iap->ndim;
  ips = iap->ips;

  dsold = rap->dsold;

  rldot[0] = (rlcur[0] - rlold[0]) / dsold;
  for (i = 0; i < ndim; ++i) {
    udot[i] = (u[i] - uold[i]) / dsold;
  }

  rlold[0] = rlcur[0];
  rlcur[0] += *rds * rldot[0];
  for (i = 0; i < ndim; ++i) {
    uold[i] = u[i];
    u[i] += udot[i] * *rds;
  }
  /*      Save old time for time integration */
  if (ips == xpp::AutoProblem::TimeIntegration) {
    rap->tivp = rlold[0];
  }

  return 0;
} /* contae_ */

/*     ---------- ----- */
/* Subroutine */ int 
solvae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), doublereal *rds, integer *m1aaloc, doublereal *aa, doublereal *rhs, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *du, doublereal *uold, doublereal *udot, doublereal *f, doublereal *dfdu, doublereal *dfdp, doublereal *thl, doublereal *thu)
{
  xpp::Session &s=*iap->lib->session;
  /* System generated locals */
  integer aa_dim1;

  /* Local variables */
  integer iads, ndim;
  doublereal drlm;
  integer ndmr;
  doublereal epsl, epsu, dumx;
  integer ntop, itnw, ntot, i, k;
  doublereal dsold, dsmin;

  doublereal rdrlm, rdumx;
  integer istop;
  bool nonfinite = false;

  doublereal au;
  doublereal delref=0.0, ss, delmax;

  integer iid;
  doublereal dds;
  integer ibr;
  doublereal det, adu;
  integer nit, mxt;
  doublereal umx;
  integer nit1;
  integer &last_ntop=iap->lib->solvae_ntop;

/* This is the subroutine for computing solution branches. It solves */
/* the equations for finding the next point on the branch at distance DS 
*/
/* from the current point. An initial approximation to the new point */
/* ( i.e. to PAR(ICP(1)) and U ) has been supplied by CONT. */

/* Local */

  aa_dim1 = *m1aaloc;

  ndim = iap->ndim;
  iads = iap->iads;
  iid = iap->iid;
  itnw = iap->itnw;
  ibr = iap->ibr;

  dsmin = rap->dsmin;
  epsl = rap->epsl;
  epsu = rap->epsu;

 L1:
  dsold = *rds;
  rap->dsold = dsold;
  dds = 1. / *rds;
  nit = 0;
  iap->nit = nit;
  ntot = iap->ntot;
  ntop = ntot + 1;
  ndmr = ndim;
  if (ndmr > 6) {
    ndmr = 6;
  }
  if (iid >= 2) {
    if(last_ntop != ntop) {
      xpp::print(s.auto_lib.fp9,"========================================");
      xpp::print(s.auto_lib.fp9,"========================================\n");
      last_ntop = ntop;
    }
    if (nit == 0) {
      xpp::print(s.auto_lib.fp9,"  BR    PT  IT\n");
    }

    xpp::print(s.auto_lib.fp9,"{:4} {:5}{:4}    {:14.6E}              ",ibr,ntop,
	    nit,rlcur[0]);
    for (i = 0; i < ndmr; ++i) {
      xpp::print(s.auto_lib.fp9,"{:14.6E}",u[i]);
    }
    xpp::print(s.auto_lib.fp9,"\n");
  }

  /* Call user-supplied FUNC to evaluate the right hand side of the */
  /* differential equation and its derivatives : */

  for (nit1 = 1; nit1 <= itnw; ++nit1) {

    if (!iap->lib->locating && xpp::job::cancelled()) goto L5; /* xppautX: cancel */
    nit = nit1;
    iap->nit = nit;
    par[icp[0]] = rlcur[0];
    (*funi)(iap, rap, ndim, u, uold, icp, par, 2, f, dfdu, dfdp);
    if (jacobian_failed(iap)) goto L5; /* W275: the derivatives are not the model's: back to the last point */

    /* Set up the Jacobian matrix and the right hand side : */

    for (i = 0; i < ndim; ++i) {
      ARRAY2D(aa, i, ndim) = dfdp[(icp[0]) * ndim + i];
      rhs[i] = -f[i];
      for (k = 0; k < ndim; ++k) {
	ARRAY2D(aa, i, k) = dfdu[k * ndim + i];
      }
    }
    for (k = 0; k < ndim; ++k) {
      ARRAY2D(aa, ndim, k) = thu[k] * 2. * (u[k] - uold[k]) * dds;
    }
    ARRAY2D(aa, ndim, ndim) = thl[icp[0]] * 2. * (rlcur[0] - 
						      rlold[0]) * dds;
    ss = 0.;
    for (i = 0; i < ndim; ++i) {
      /* Computing 2nd power */
      ss += thu[i] * ((u[i] - uold[i]) * (u[i] - uold[i]));
    }
    /* Computing 2nd power */
    rhs[ndim] = *rds - dds * ss - thl[icp[0]] * dds * ((rlcur[0] - rlold[0]) * (rlcur[0] - rlold[0]));

    /* Use Gauss elimination with pivoting to solve the linearized system 
       : */

    if (iid >= 5) {
      integer tmp = ndim + 1;
      wrjac(iap, &tmp, m1aaloc, aa, rhs);
    }

    xpp::ge(iap->lib->fp9, iap->iid, ndim + 1, *m1aaloc, aa, 1, ndim + 1, du,
       ndim + 1, rhs, &det);
    rap->det = det;
    drlm = du[ndim];

    /* Add the Newton increments : */

    for (i = 0; i < ndim; ++i) {
      u[i] += du[i];
    }
    rlcur[0] += drlm;
    /* du's last entry is drlm, already in rlcur */
    if (!xpp::all_finite({u, static_cast<size_t>(ndim)}) || !std::isfinite(rlcur[0])) {
      nonfinite = true;
      goto L3;
    }
    dumx = 0.;
    umx = 0.;
    for (i = 0; i < ndim; ++i) {
      adu = f2c::abs(du[i]);
      au = f2c::abs(u[i]);
      if (au > umx) {
	umx = au;
      }
      if (adu > dumx) {
	dumx = adu;
      }
    }

    if (iid >= 2) {
      xpp::print(s.auto_lib.fp9,"{:4} {:5}{:4}    {:14.6E}              ",ibr,ntop,
	      nit,rlcur[0]);
      for (i = 0; i < ndmr; ++i) {
	xpp::print(s.auto_lib.fp9,"{:14.6E}",u[i]);
      }
      xpp::print(s.auto_lib.fp9,"\n");
    }

    rdrlm = f2c::abs(drlm) / (f2c::abs(rlcur[0]) + 1.);
    rdumx = dumx / (umx + 1.);
    if (rdrlm <= epsl && rdumx <= epsu) {
      /* W421: the test functions read the Jacobian of the converged point,
         as AUTO-07p's SOLVAE recomputes it (ae.f90), not the last iterate's */
      par[icp[0]] = rlcur[0];
      (*funi)(iap, rap, ndim, u, uold, icp, par, 2, f, dfdu, dfdp);
      if (jacobian_failed(iap)) goto L5;
      pvlsae(iap, rap, u, par);
      if (!xpp::all_finite({par, static_cast<size_t>(iap->lib->parameter_count)})) {
        nonfinite = true;
        goto L3;
      }
      if (iid >= 2) {
	xpp::print(s.auto_lib.fp9,"\n");
      }
      return 0;
    }

    /* Check whether relative error has reached user-supplied tolerance : 
     */

    if (nit == 1) {
      delref = f2c::max(rdrlm,rdumx) * 20;
    } else {
      delmax = f2c::max(rdrlm,rdumx);
      if (delmax > delref) {
	goto L3;
      }
    }

    /* L2: */
  }

  /* Maximum number of iterations has been reached */

 L3:
  if (iads == 0) {
    xpp::print(s.auto_lib.fp9,"{:4} {:5} NOTE:No convergence with fixed step size\n",ibr,ntop);
  }
  if (iads == 0) {
    xpp::auto_stop_noconv(iap->lib->session->auto_state.stop, xpp::AUTO_STOP_NOCONV_FIXED, *rds, dsmin); /* xppautX: T23 */
    goto L5;
  }

  /* Reduce stepsize and try again */

  mxt = itnw;
  iap->nit = mxt;
  adptds(iap, rap, rds);
  if (f2c::abs(*rds) < dsmin) {
    goto L4;
  }
  rlcur[0] = rlold[0] + *rds * rldot[0];
  for (i = 0; i < ndim; ++i) {
    u[i] = uold[i] + *rds * udot[i];
  }
  if (iid >= 2) {
    xpp::print(s.auto_lib.fp9," NOTE:Retrying step\n");
  }
  goto L1;

/* Minimum stepsize reached */

 L4:
  xpp::print(s.auto_lib.fp9,"{:4} {:5} NOTE:No convergence using minimum step size\n",ibr,ntop);
  xpp::auto_stop_noconv(iap->lib->session->auto_state.stop, xpp::AUTO_STOP_NOCONV_MIN, *rds, dsmin); /* xppautX: T23 */
 L5:
  rlcur[0] = rlold[0];
  par[icp[0]] = rlcur[0];
  for (i = 0; i < ndim; ++i) {
    u[i] = uold[i];
  }
  const bool retained = xpp::auto_stability_step_failed(iap->lib->session->auto_state.stability,
      iap->ibr, iap->ntot + 1);
  istop = 1;
  iap->istop = istop;
  if (nonfinite && !stop_asked(iap))
    xpp::auto_nonfinite(*iap, iap->ntot + 1);
  if (!retained && xpp::problem_is_steady_or_map(iap->ips) &&
      !stop_asked(iap))
    xpp::auto_stability_equilibrium_restored(*iap, *rap, par, icp, funi, u, uold, f, dfdu, dfdp);
  return 0;
} /* solvae_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*               Detection of Singular Points */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/*     ---------- ------ */
/* Subroutine */ int 
lcspae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FNCS_TYPE_AE((*fncs)), FUNI_TYPE((*funi)), integer *m1aaloc, doublereal *aa, doublereal *rhs, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *du, doublereal *uold, doublereal *udot, doublereal *f, doublereal *dfdu, doublereal *dfdp, doublereal *q, doublereal *thl, doublereal *thu, std::span<const AutoMark> marks, double &dstest, bool &stepped)
{
  stepped = false;

  /* Local variables */
  logical chng;
  doublereal epss, rrds;
  integer itmx;
  doublereal rtmp;
  integer ntot;
  doublereal s, dsmax;
  integer istop;
  doublereal q0, q1, s0, s1, dq, ds;

  integer itlcsp, iid, ibr;
  doublereal rds;
  xpp::AutoPointCode itp;

  /* This subroutine uses the secant method to accurately locate special */
  /* points (branch points, folds, Hopf bifurcations, user zeroes). */
  /* These are characterized as zeroes of the function FNCS supplied in the 
*/
/* call. */
/* This subroutine calls CONT and SOLVAE with varying stepsize RDS. */
/* The special point is assumed to have been found with sufficient */
/* accuracy if the ratio between RDS and the user supplied value of */
/* DS is less than the user-supplied toler du. */

  iid = iap->iid;
  itmx = iap->itmx;
  ibr = iap->ibr;

  ds = rap->ds;
  dsmax = rap->dsmax;
  epss = rap->epss;

  /* Check whether FNCS has changed sign (FNCS is EXTERNAL). */

  q0 = *q;
  q1 = (*fncs)(iap, rap, par, icp, &chng, funi, m1aaloc, 
	       aa, rlcur, rlold, rldot, u, uold, udot, 
	       rhs, dfdu, dfdp, marks);
  ntot = iap->ntot;
  if (!location_sign_change(*iap, *rap, q0, q1, dstest, chng, fncs == fnuzae)) {
    *q = q1;
    return 0;
  }

  const auto n = static_cast<std::size_t>(iap->ndim);
  const auto np = static_cast<std::size_t>(iap->lib->parameter_count);
  const auto na = static_cast<std::size_t>(*m1aaloc) * (n + 1);
  const double saved_q = q1;
  const double saved_dstest = dstest;
  LocationState saved(*iap, *rap, {{par, np}, {rlcur, 1}, {rlold, 1}, {rldot, 1},
      {u, n}, {uold, n}, {udot, n}, {aa, na}, {rhs, n + 1}, {du, n + 1},
      {f, n}, {dfdu, n * n}, {dfdp, n * np}});

  const bool singular = iap->itpst == xpp::AutoPointKind::Torus &&
      xpp::auto_torus_singular(u[2 * iap->ndm]);

  /* Use the secant method for the first step: */

  s0 = 0.;
  s1 = dstest;
  itlcsp = 0;
  dq = q0 - q1;
  rds = q1 / dq * (s1 - s0);
  rtmp = auto_hmach1;
 L1:
  rds = rtmp * rds;

  /* Return if relative tolerance has been met : */

  rrds = f2c::abs(rds) / (sqrt(f2c::abs(ds * dsmax)) + 1);
  if (rrds < epss) {
    itp = xpp::AutoPointCode{xpp::AutoPointKind::Pending};
    iap->itp = itp;
    *q = 0.;
    xpp::print(iap->lib->fp9," ==> Location of special point :  Convergence.    Stepsize ={:11.3E}\n",rds);
    return 0;
  }

  rds = location_step(*rap, rds, singular);

  /* If requested write additional output on unit 9 : */

  if (iid >= 2) {
    xpp::print(iap->lib->fp9," ==> Location of special point :  Iteration {:3}   Stepsize ={:11.3E}\n",itlcsp,rds);	
  }

  {
    Locating locating(*iap->lib); /* predictor and solve belong to the unstored point */
    contae(iap, rap, &rds, rlcur, rlold, rldot, u, uold,
	 udot);
    solvae(iap, rap, par, icp, funi, &rds, m1aaloc, aa,
	   rhs, rlcur, rlold, rldot, u, du, uold, udot, f,
	   dfdu, dfdp, thl, thu);
  }
  istop = iap->istop;
  if (istop != 0) {
    // AUTO-07p LCSPAE ae.f90:738: a SOLVAE that does not converge leaves the
    // search: the point is given up (NOTE: Possible special point) and the branch goes on.
    if (stop_asked(iap)) {
      *q = 0.;
      return 0;
    }
    iap->istop = 0;
    goto L2;
  }

  *q = (*fncs)(iap, rap, par, icp, &chng, funi, m1aaloc, 
	       aa, rlcur, rlold, rldot, u, uold, udot,
	       rhs, dfdu, dfdp, marks);
  // W419: an iterate where the test does not apply is no zero: give the point up.
  if (!auto_test_applies(*q)) goto L2;
  ++itlcsp;
  if (itlcsp <= itmx) {
    // AUTO-07p LCSPAE ae.f90:743 / LCSPBV bvp.f90:1414:
    // the corrector may have retried with a shorter step.
    stepped = true;
    s = s1 + rds;
    dstest = s;
    /*        Use Mueller's method with bracketing for subsequent steps */
    mueller(&q0, &q1, q, &s0, &s1, &s, &rds);
    goto L1;
  }

 L2:
  saved.restore(*iap, *rap);
  dstest = saved_dstest;
  stepped = false;
  *q = saved_q;

  xpp::print(iap->lib->fp9,"{:4} {:5} NOTE:Possible special point\n",ibr,ntot + 1);

  return 0;

} /* lcspae_ */

/*     ---------- ------- */
/* Subroutine */ int 
mueller(doublereal *q0, doublereal *q1, doublereal *q, doublereal *s0, doublereal *s1, doublereal *s, doublereal *rds)
{
  // W356 (#418), reimplementation of AUTO-07p MUELLER,
  // support.f90:31-61: use zero for a nonpositive discriminant and
  // the root formula whose denominator adds like-signed quantities.
  const double h0 = *s0 - *s;
  const double h1 = *s1 - *s;
  const double d = h0 * h1 * (h1 - h0);
  const double a = (h1 * h1 * (*q0 - *q) - h0 * h0 * (*q1 - *q)) / d;
  const double b = (-h1 * (*q0 - *q) + h0 * (*q1 - *q)) / d;
  double root = 0;
  if (a * a > 4 * b * *q) {
    root = std::sqrt(a * a - 4 * b * *q);
    if (a < 0) root = -root;
  }
  *rds = -2 * *q / (a + root);

  const double dq = *q1 * *q;
  if (dq < 0.) {
    *q0 = *q1;
    *s0 = *s1;
  }
  *q1 = *q;
  *s1 = *s;

  return 0;
} /* mueller_ */

/*     ------ --------- -------- ------ */
doublereal 
fnbpae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, logical *chng, FUNI_TYPE((*funi)), integer *m1aaloc, doublereal *aa, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *uold, doublereal *udot, doublereal *rhs, doublereal *dfdu, doublereal *dfdp, std::span<const AutoMark> marks)
{
  /* System generated locals */
  doublereal ret_val;

  /* Local variables */
  integer ntop, ntot, iid, ibr;
  doublereal det;

  iid = iap->iid;
  ibr = iap->ibr;
  ntot = iap->ntot;
  ntop = ntot + 1;

  det = rap->det;
  ret_val = det;
  *chng = TRUE_;

  /* If requested write additional output on unit 9 : */

  if (iid >= 2) {
    xpp::print(iap->lib->fp9,"{:4} {:5}        BP   Function {:14.6E}\n",ibr,ntop,ret_val);
  }

  return ret_val;
} /* fnbpae_ */

/*     ------ --------- -------- ------ */
doublereal 
fnlpae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, logical *chng, FUNI_TYPE((*funi)), integer *m1aaloc, doublereal *aa, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *uold, doublereal *udot, doublereal *rhs, doublereal *dfdu, doublereal *dfdp, std::span<const AutoMark> marks)
{
  /* System generated locals */
  integer aa_dim1;
  doublereal ret_val;

    /* Local variables */
  integer ndim, ntop, ntot, i, k;

  integer iid, ibr;
  doublereal det;

  std::vector<doublereal> ud(xpp::size_sum(iap->ndim, 1));

  /* Local */

  /* Parameter adjustments */
  /*--par;*/
  /*--icp;*/
  /*--rlcur;*/
  /*--rlold;*/
  /*--rldot;*/
  /*--u;*/
  /*--uold;*/
  /*--udot;*/
  /*--rhs;*/
  /*--dfdu;*/
  /*--dfdp;*/
  aa_dim1 = *m1aaloc;

  ndim = iap->ndim;
  iid = iap->iid;
  ibr = iap->ibr;
  ntot = iap->ntot;
  ntop = ntot + 1;

  /* dfdu and dfdp are the converged point's: solvae computed them (W421) */
  for (i = 0; i < ndim; ++i) {
    ARRAY2D(aa, i, ndim) = dfdp[(icp[0]) * ndim + i];
    for (k = 0; k < ndim; ++k) {
      ARRAY2D(aa, i, k) = dfdu[k * ndim + i];
    }
  }
  for (k = 0; k < ndim; ++k) {
    ARRAY2D(aa, ndim, k) = udot[k];
    rhs[k] = 0.;
  }
  ARRAY2D(aa, ndim, ndim) = rldot[0];
  rhs[ndim] = 1.;

  /* det stays this matrix's: the branch point test (fnbpae) reads the Newton
     matrix's, whose last row is the arclength condition, as AUTO-07p's FNLPAE
     leaves AP%DET alone; the tangent row here changes sign when a step turns
     past a fold (W423, docs/xppaut-findings.md 72) */
  xpp::ge(iap->lib->fp9, iap->iid, ndim + 1, *m1aaloc, aa, 1, ndim + 1, ud.data(), ndim + 1, rhs, &det);
  {
    integer tmp = ndim + 1;
    nrmlz(&tmp, ud.data());
  }
  ret_val = ud[ndim];
  *chng = TRUE_;

  /* If requested write additional output on unit 9 : */

  if (iid >= 2) {
    xpp::print(iap->lib->fp9,"{:4} {:5}        Fold Function {:14.6E}\n",f2c::abs(ibr),ntop,ret_val);
  }
  return ret_val;
} /* fnlpae_ */

/*     ------ --------- -------- ------ */
doublereal 
fnhbae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, logical *chng, FUNI_TYPE((*funi)), integer *m1aaloc, doublereal *aa, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *uold, doublereal *udot, doublereal *rhs, doublereal *dfdu, doublereal *dfdp, std::span<const AutoMark> marks)
{
  /* System generated locals */
  doublereal ret_val;

  /* Local variables */
  integer ndim;
  doublereal rmax;
  integer nins, ntop, ntot;
  doublecomplex ztmp;
  integer nins1, i, j;
  integer ntotp1;

  doublereal rp;

  integer iid, ibr, ndm, ier, loc=0;
  xpp::AutoProblem ips;
  doublereal rev;
  integer isw;

  std::vector<doublecomplex> ev(iap->ndim);

  /* Local */

  /* Parameter adjustments */
  /*--par;*/
  /*--icp;*/
  /*--u;*/
  /*--uold;*/
  /*--udot;*/
  /*--rhs;*/

  ndim = iap->ndim;
  ndm = iap->ndm;
  ips = iap->ips;
  isw = iap->isw;
  iid = iap->iid;
  ibr = iap->ibr;
  ntot = iap->ntot;
  ntop = ntot + 1;

/* INITIALIZE */

  *chng = FALSE_;

  /* Compute the eigenvalues of the Jacobian */

  const bool codim_curve=ips==xpp::AutoProblem::SteadyState && isw==2 &&
      (iap->itpst==xpp::AutoPointKind::Fold ||
       (iap->itpst==xpp::AutoPointKind::Hopf && ndim==2*ndm+2));
  const double axis_tolerance = xpp::auto_hopf_axis_tolerance(
      xpp::Matrix<const double>({dfdu,static_cast<size_t>(ndim*ndm)},ndim,ndm),ndm);
  // EISPACK overwrites its matrix: dfdu stays the converged point's Jacobian
  // for the tests after this one and stbif (W421; AUTO-07p PVLSAE's copy).
  std::vector<doublereal> jacobian(dfdu, dfdu + static_cast<size_t>(ndim*ndm));
  xpp::eig(iap, &ndm, &ndim, jacobian.data(), ev.data(), &ier);
  if (ips == xpp::AutoProblem::DiscreteMap) {
    for (i = 0; i < ndm; ++i) {
      if (ev[i].r != -1. || d_imag(&ev[i]) != 0.) {
	doublecomplex in,out;
	in.r = ev[i].r + 1., in.i = ev[i].i;
	z_log(&out, &in);
	ev[i].r = out.r, ev[i].i = out.i;
      } else {
	ev[i].r = -auto_rlarge, ev[i].i = 0.;
      }
    }
  }
  /* xppautX: the point's eigenvalues (auto_stability.h; eig computed ndm) */
  xpp::auto_stability_computed(iap->lib->session->auto_state.stability,ibr,ntot+1,{&ev[0].r,2*static_cast<size_t>(ndm)},xpp::AUTO_STABILITY_STEADY);
  /* Order the eigenvalues by real part. */

  for (i = 0; i < ndm - 1; ++i) {
    rmax = -auto_rlarge;
    for (j = i; j < ndm; ++j) {
      rp = ev[j].r;
      if (rp >= rmax) {
	rmax = rp;
	loc = j;
      }
    }
    if (loc != i) {
      ztmp.r = ev[loc].r, ztmp.i = ev[loc].i;
      ev[loc].r = ev[i].r, ev[loc].i = ev[i].i;
      ev[i].r = ztmp.r, ev[i].i = ztmp.i;
    }
  }

  // One order-independent choice supplies both the Hopf test and period.
  rev = auto_test_not_applicable;
  const auto pair = xpp::auto_hopf_pair({ev.data(),static_cast<size_t>(ndm)},axis_tolerance);
  if (pair) {
    rev = pair->r;
    if (!codim_curve && ips!=xpp::AutoProblem::DiscreteMap) par[xpp::auto_unknown::period] = pi(2.0) / std::abs(pair->i);
  }

  /* Compute the number of eigenvalues with negative real part. */

  nins1 = 0;
  if (isw != 2) {
    for (i = 0; i < ndm; ++i) {
      if (ev[i].r <= 0.) {
	++nins1;
      }
    }
  } else {
    for (i = 0; i < ndm; ++i) {
      if (ev[i].r <= auto_hmach) {
	++nins1;
      }
    }
  }

  const bool map=ips==xpp::AutoProblem::DiscreteMap;
  if (map || codim_curve) {
    // AUTO-07p STABEQ: omit the imposed zero/+1/-1, or the imposed pair.
    const integer forced=isw!=2 ? 0 :
        (iap->itpst==xpp::AutoPointKind::Hopf || iap->itpst==xpp::AutoPointKind::Torus ? 2 : 1);
    std::stable_sort(ev.begin(),ev.begin()+ndm,[](const auto &a,const auto &b) { return std::abs(a.r)<std::abs(b.r); });
    nins1=forced;
    for (i=forced;i<ndm;++i) if (ev[i].r<=axis_tolerance) ++nins1;
    ret_val=ndm>forced ? ev[forced].r : auto_test_not_applicable;
    if (map) {
      std::optional<doublecomplex> selected;
      for (i=forced;i<ndm;++i)
        if ((forced || ev[i].i!=0.) && (!selected || std::abs(ev[i].r)<std::abs(selected->r))) selected=ev[i];
      auto &kind=iap->lib->codim.map_kind;
      kind=selected ? xpp::auto_map_secondary_kind(isw==2 ? iap->itpst : xpp::AutoPointKind::Regular,std::abs(selected->i),rap->epss) : xpp::AutoPointKind::Regular;
      ret_val=selected ? selected->r : auto_test_not_applicable;
      *chng=nins1!=iap->nins && kind!=xpp::AutoPointKind::Regular && iap->isp!=0 &&
          (isw!=2 || xpp::auto_codim_enabled(iap->lib->session->auto_state.special_points,kind));
      if (isw!=2 && selected && kind==xpp::AutoPointKind::Torus)
        par[xpp::auto_unknown::period]=pi(2.)/std::abs(selected->i);
    }
  } else {
    ret_val = isw==2 ? auto_test_not_applicable : rev;
  }
  if (!map) {
    nins = iap->nins;
    // AUTO-07p FNHBEQ equilibrium.f90:624-629: a plain HB changes a pair.
    constexpr integer hopf_stability_change = 2; // one complex conjugate pair
    const integer required_change=isw==2 && iap->itpst==xpp::AutoPointKind::Hopf ? 1 : hopf_stability_change;
    if (f2c::abs(nins1 - nins) >= required_change) *chng = TRUE_;
    if (isw==2 && (iap->isp==0 || iap->isp==3 ||
        !xpp::auto_codim_enabled(iap->lib->session->auto_state.special_points,xpp::AutoPointKind::ZeroHopf) ||
        iap->itp.kind==xpp::AutoPointKind::BogdanovTakens)) { *chng=FALSE_; ret_val=auto_test_not_applicable; }
  }
  nins = nins1;
  iap->nins = nins;

  ntot = iap->ntot;
  ntotp1 = ntot + 1;
  if (iid >= 2) {
    xpp::print(iap->lib->fp9,"{:4} {:5}        Hopf Function {:14.6E}\n",f2c::abs(ibr),ntop,ret_val);
  }
  if (nins1 == ndm) {
    ntotp1 = -ntotp1;
  }

  xpp::print(iap->lib->fp9,"{:4} {:5}        Eigenvalues:                                 Stable:{:3}\n",f2c::abs(ibr),ntop,nins);
  if (ips == xpp::AutoProblem::DiscreteMap) {
    for (i = 0; i < ndm; ++i) {
      doublecomplex tmp;
      z_exp(&tmp, &ev[i]);
      xpp::print(iap->lib->fp9,"{:4} {:5}        Eigenvalue{:3} {:14.6E}{:14.6E}\n",f2c::abs(ibr),ntop,i+1,tmp.r,tmp.i);
    }
  } else {
    for (i = 0; i < ndm; ++i) {
      xpp::print(iap->lib->fp9,"{:4} {:5}        Eigenvalue{:3} {:14.6E}{:14.6E}\n",f2c::abs(ibr),ntop,i+1,ev[i].r,ev[i].i);
    }
  }

  return ret_val;
} /* fnhbae_ */

/*     ------ --------- -------- ------ */
namespace {
// IID 3 starts detailed scalar-test diagnostics in AUTO.
constexpr integer user_zero_diagnostic_level = 3;
// Preserve the two original diagnostic column widths.
constexpr int algebraic_user_zero_width = 16;
constexpr int boundary_user_zero_width = 14;

doublereal user_zero(const iap_type &iap, const doublereal *par,
                     const integer *icp, logical *chng,
                     std::span<const AutoMark> marks, int width) {
  const integer iuzr = iap.iuzr;
  const doublereal value = auto_parameter_value(
      *iap.lib->session, f2c::abs(marks[iuzr].parameter),
      {icp, static_cast<std::size_t>(iap.nfpr)},
      {par, static_cast<std::size_t>(iap.lib->parameter_count)}) - marks[iuzr].value;
  *chng = TRUE_;
  if (iap.iid >= user_zero_diagnostic_level) {
    xpp::print(iap.lib->fp9, "{:4} {:5}        User Func. {:3} {:{}.6E}\n",
               f2c::abs(iap.ibr), iap.ntot + 1,
               iuzr, value, width);
  }
  return value;
}
} // namespace

doublereal
fnuzae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, logical *chng, FUNI_TYPE((*funi)), integer *m1aaloc, doublereal *aa, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *uold, doublereal *udot, doublereal *rhs, doublereal *dfdu, doublereal *dfdp, std::span<const AutoMark> marks)
{
  return user_zero(*iap, par, icp, chng, marks, algebraic_user_zero_width);
} /* fnuzae_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*                   Branch Switching for Algebraic Problems */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/*     ---------- ----- */
/* Subroutine */ int 
stbif(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *m1aaloc, doublereal *aa, doublereal *stud, doublereal *stu, doublereal *stla, doublereal *stld, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *du, doublereal *udot, doublereal *dfdu, doublereal *dfdp, doublereal *thl, doublereal *thu)
{
  /* System generated locals */
  integer aa_dim1;

  /* Local variables */
  integer nbif, ndim;

  integer i, j;
  doublereal sc;
  doublereal ss;

  /* Stores branching data in the following arrays : */
  /*        STU    ( the solution vector U ) */
  /*        STUD   ( U-dot ) */
  /*        STLA   ( PAR(ICP(1)) ) */
  /*        STLD  ( PAR(ICP(1))-dot ) */
  /* Here the vector ( PAR(ICP(1))-dot , U-dot ) lies in the 2-d nullspace 
   */
  /* at branch point and is perpendicular to the direction vector of */
  /* known branch at this point. */

/* Local */

  /* Parameter adjustments */
  /*--par;*/
  /*--icp;*/
  /*--stla;*/
  /*--stld;*/
  /*--rlcur;*/
  /*--rlold;*/
  /*--rldot;*/
  /*--u;*/
  /*--du;*/
  /*--udot;*/
  /*--dfdu;*/
  /*--dfdp;*/
  /*--thl;*/
  /*--thu;*/
  aa_dim1 = *m1aaloc;

  ndim = iap->ndim;
  nbif = iap->nbif;

  /* Keep track of the number of branch points stored. */


  for (i = 0; i < ndim; ++i) {
    for (j = 0; j < ndim; ++j) {
      ARRAY2D(aa, i, j) = dfdu[j * ndim + i];
    }
  }

  for (i = 0; i < ndim; ++i) {
    ARRAY2D(aa, i, ndim) = dfdp[(icp[0]) * ndim + i];
    ARRAY2D(aa, ndim, i) = udot[i];
  }
  ARRAY2D(aa, ndim, ndim) = rldot[0];

  xpp::nlvc(iap->lib->fp9, ndim + 1, *m1aaloc, 1, aa, du);

  ss = 0.;
  for (i = 0; i < ndim; ++i) {
    /* Computing 2nd power */
    ss += thu[i] * (du[i] * du[i]);
  }
  /* Computing 2nd power */
  ss += thl[icp[0]] * (du[ndim] * du[ndim]);
  sc = 1. / sqrt(ss);

  for (i = 0; i < ndim + 1; ++i) {
    du[i] = sc * du[i];
  }

  nbif = iap->nbif;
  stld[-1 + nbif] = du[ndim];
  for (i = 0; i < ndim; ++i) {
    stu[(nbif - 1) * ndim + i] = u[i];
    stud[(nbif - 1) * ndim + i] = du[i];
  }
  stla[-1 + nbif] = rlcur[0];

  return 0;
} /* stbif_ */

/*     ---------- ----- */
/* Subroutine */ int 
swpnt(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *rds, doublereal *stud, doublereal *stu, doublereal *stla, doublereal *stld, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *udot)
{
  /* System generated locals */

    /* Local variables */
  integer nbif, ndim, mxbf, ipos, i, i1;
  doublereal ds;
  integer isw;

  /* This subroutine retrieves the branching data U, U-dot, PAR(ICP(1)), */
  /* PAR(ICP(1))-dot. If this initialization corresponds to the computation */
  /* of the bifurcating branch in opposite direction, then only the sign of */
  /*  the stepsize ( DS ) along the branch is reversed. */

  /* Parameter adjustments */
  /*--par;*/
  /*--icp;*/
  /*--stla;*/
  /*--stld;*/
  /*--rlcur;*/
  /*--rlold;*/
  /*--rldot;*/
  /*--u;*/
  /*--udot;*/
  
  ndim = iap->ndim;
  isw = iap->isw;
  mxbf = iap->mxbf;
  nbif = iap->nbif;
  ipos = iap->ipos;

  ds = rap->ds;

  *rds = ds;
  if (ipos == 0) {
    *rds = -ds;
  }
  rlcur[0] = stla[0];
  par[icp[0]] = rlcur[0];
  rldot[0] = stld[0];
  for (i = 0; i < ndim; ++i) {
    u[i] = stu[i];
    udot[i] = stud[i];
  }
  if (f2c::abs(isw) == 2) {
    par[icp[1]] = u[-1 + ndim];
  }

  if (mxbf >= 0) {
    ipos = 1 - ipos;
    iap->ipos = ipos;
  }
  if (ipos == 0) {
    return 0;
  }

  for (i = 0; i + 1 < nbif; ++i) {
    stla[i] = stla[i + 1];
    stld[i] = stld[i + 1];
    for (i1 = 0; i1 < ndim; ++i1) {
      stu[i * ndim + i1] = stu[(i + 1) * ndim + i1];
      stud[i * ndim + i1] = stud[(i + 1) * ndim + i1];
    }
  }

  return 0;
} /* swpnt_ */

/*     ---------- ----- */
/* Subroutine */ int 
swprc(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), integer *m1aaloc, doublereal *aa, doublereal *rhs, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *du, doublereal *uold, doublereal *udot, doublereal *f, doublereal *dfdu, doublereal *dfdp, doublereal *rds, doublereal *thl, doublereal *thu)
{
  // Internal switches can reuse BR/PT for a different branch point. Point
  // zero means no proven record; on failure refresh the restored Jacobian.
  xpp::auto_stability_step_start(iap->lib->session->auto_state.stability, iap->ibr, 0);

  /* System generated locals */
  integer aa_dim1;

    /* Local variables */
  integer iads, ndim;
  doublereal drlm;
  integer ndmr;
  doublereal epsl, epsu, dumx;
  integer ntop, itnw, ntot, i, k;
  doublereal dsold, dsmin;

  doublereal rdrlm, rdumx;
  integer istop;
  bool nonfinite = false;

  doublereal au;
  doublereal ss;

  integer iid;
  doublereal adu;
  integer ibr;
  doublereal det;
  integer nit, mxt;
  doublereal umx, rlm1;

  std::vector<doublereal> u1(xpp::size_sum(iap->ndim, 1));

  /* Controls the computation of the second point on a bifurcating branch. */
/* This point is required to lie in a hyper-plane at distance DS from the */
/* branch point. This hyper-plane is parallel to the tangent of the */
/* known branch at the branch point. */

/* Local */

  /* Parameter adjustments */
  /*--par;*/
  /*--icp;*/
  /*--rhs;*/
  /*--rlcur;*/
  /*--rlold;*/
  /*--rldot;*/
  /*--u;*/
  /*--du;*/
  /*--uold;*/
  /*--udot;*/
  /*--f;*/
  /*--dfdu;*/
  /*--dfdp;*/
  /*--thl;*/
  /*--thu;*/
  aa_dim1 = *m1aaloc;
  
  ndim = iap->ndim;
  iads = iap->iads;
  iid = iap->iid;
  itnw = iap->itnw;
  ibr = iap->ibr;
  ntot = iap->ntot;
  ntop = ntot + 1;

  dsmin = rap->dsmin;
  epsl = rap->epsl;
  epsu = rap->epsu;

  /* Initialize and provide initial guess : */

  rlold[0] = rlcur[0];
  rlcur[0] = rlold[0] + *rds * rldot[0];
  for (i = 0; i < ndim; ++i) {
    uold[i] = u[i];
    u[i] = uold[i] + *rds * udot[i];
  }

 L2:
  dsold = *rds;
  rap->dsold = dsold;
  nit = 0;
  iap->nit = nit;

/* Write additional output on unit 9 if requested : */

  ndmr = ndim;
  if (ndmr > 6) {
    ndmr = 6;
  }
  if (iid >= 2) {
    xpp::print(iap->lib->fp9," Branch {:2} N={:5} IT={:2} PAR({:2})={:11.3E} U=",ibr,ntop,
	    nit,icp[0],rlcur[0]);	
    for (i = 0; i < ndmr; ++i) {
      xpp::print(iap->lib->fp9,"{:11.3E}",u[i]);	
    }
    xpp::print(iap->lib->fp9,"\n");	

  }

  rlm1 = rlcur[0];
  for (i = 0; i < ndim; ++i) {
    u1[i] = u[i];
  }

  for (nit = 0; nit < itnw; ++nit) {

    iap->nit = nit + 1;
    par[icp[0]] = rlcur[0];
    (*funi)(iap, rap, ndim, u, uold, icp, par, 2, f, dfdu, dfdp);
    for (i = 0; i < ndim; ++i) {
      ARRAY2D(aa, i, ndim) = dfdp[(icp[0]) * ndim + i];
      rhs[i] = -f[i];
      for (k = 0; k < ndim; ++k) {
	ARRAY2D(aa, i, k) = dfdu[k * ndim + i];
      }
    }
    for (k = 0; k < ndim; ++k) {
      ARRAY2D(aa, ndim, k) = thu[k] * udot[k];
    }
    ARRAY2D(aa, ndim, ndim) = thl[icp[0]] * rldot[0];
    ss = 0.;
    for (i = 0; i < ndim; ++i) {
      ss += thu[i] * (u[i] - u1[i]) * udot[i];
    }
    rhs[ndim] = -ss - thl[icp[0]] * (rlcur[0] - rlm1) * rldot[0];

    /* Use Gauss elimination with pivoting to solve the linearized system 
       : */

    if (iid >= 5) {
      integer tmp = ndim + 1;
      wrjac(iap, &tmp, m1aaloc, aa, rhs);
    }
    xpp::ge(iap->lib->fp9, iap->iid, ndim + 1, *m1aaloc, aa, 1, ndim + 1, du,
       ndim + 1, rhs, &det);
    rap->det = det;
    drlm = du[ndim];

    /* Add the Newton increments : */

    for (i = 0; i < ndim; ++i) {
      u[i] += du[i];
    }
    rlcur[0] += drlm;
    /* du's last entry is drlm, already in rlcur */
    if (!xpp::all_finite({u, static_cast<size_t>(ndim)}) || !std::isfinite(rlcur[0])) {
      nonfinite = true;
      goto failed_step;
    }
    dumx = 0.;
    umx = 0.;
    for (i = 0; i < ndim; ++i) {
      adu = f2c::abs(du[i]);
      if (adu > dumx) {
	dumx = adu;
      }
      au = f2c::abs(u[i]);
      if (au > umx) {
	umx = au;
      }
    }

    if (iid >= 2) {
      xpp::print(iap->lib->fp9," Branch {:2} N={:5} IT={:2} PAR({:2})={:11.3E} U=",ibr,ntop,
	      nit+1,icp[0],rlcur[0]);	
      for (i = 0; i < ndmr; ++i) {
	xpp::print(iap->lib->fp9,"{:11.3E}",u[i]);	
      }
      xpp::print(iap->lib->fp9,"\n");	
    }

    /* Check whether relative error has reached user-supplied tolerance : 
     */

    rdrlm = f2c::abs(drlm) / (f2c::abs(rlcur[0]) + 1.);
    rdumx = dumx / (umx + 1.);
    if (rdrlm < epsl && rdumx < epsu) {
      return 0;
    }
    /* L3: */
  }

failed_step:
  /* Maximum number of iterations reached. Reduce stepsize and try again. */

  if (iads == 0) {
    xpp::print(iap->lib->fp9,"{:4} {:5} NOTE:No convergence when switching branches with fixed step size\n",ibr,ntop);
  }
  if (iads == 0) {
    xpp::auto_stop_noconv(iap->lib->session->auto_state.stop, xpp::AUTO_STOP_NOCONV_SWITCH_FIXED, *rds, dsmin); /* xppautX: T23 */
    goto L5;
  }

  mxt = itnw;
  iap->nit = mxt;
  adptds(iap, rap, rds);
  if (f2c::abs(*rds) < dsmin) {
    goto L4;
  }
  rlcur[0] = rlold[0] + *rds * rldot[0];
  for (i = 0; i < ndim; ++i) {
    u[i] = uold[i] + *rds * udot[i];
  }
  if (iid >= 2) {
    xpp::print(iap->lib->fp9,"{:4} {:5} NOTE:Retrying step\n",ibr,ntop);
  }
  goto L2;

/* Minimum stepsize reached. */

 L4:
  xpp::print(iap->lib->fp9,"{:4} {:5} NOTE:No convergence when switching branches with minimum step size\n",ibr,ntop);
  xpp::auto_stop_noconv(iap->lib->session->auto_state.stop, xpp::AUTO_STOP_NOCONV_SWITCH_MIN, *rds, dsmin); /* xppautX: T23 */
 L5:
  rlcur[0] = rlold[0];
  par[icp[0]] = rlcur[0];
  for (i = 0; i < ndim; ++i) {
    u[i] = uold[i];
  }
  const bool retained = xpp::auto_stability_step_failed(iap->lib->session->auto_state.stability,
      iap->ibr, iap->ntot + 1);
  istop = 1;
  iap->istop = istop;
  if (nonfinite && !stop_asked(iap))
    xpp::auto_nonfinite(*iap, iap->ntot + 1);

  // swpnt can return to an earlier branch point whose spectrum is no
  // longer the owner's last record. Recompute at the restored solution.
  if (!retained && xpp::problem_is_steady_or_map(iap->ips) && !stop_asked(iap)) {
    xpp::auto_stability_equilibrium_restored(*iap, *rap, par, icp, funi, u, uold, f, dfdu, dfdp);
  }
  return 0;
	
} /* swprc_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*                    Output (Algebraic Problems) */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/*     ---------- ---- */
/* Subroutine */ int 
sthd(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *thl, doublereal *thu)
{
  xpp::Session &s=*iap->lib->session;
  /* Local variables */
  integer ndim, ncol, mxbf, nicp;
  doublereal epsl;
  integer nfpr, iplt, nint;
  doublereal epsu, epss;
  integer jtmp, itmx, itnw, nwtn, ntst, nuzr, i;
  doublereal dsmin, dsmax, a0, a1, ds, rl0, rl1;
  integer iad, jac, nbc, iid, ilp, isp, irs, npr, isw, nmx;
  xpp::AutoProblem ips;

  /* Write the values of the user defined parameters on unit 7. */
  /* This identifying information is preceded by a '   0' on each line. */
  /* The first line in the file contains the (generally) user-supplied */
  /* limits of the bifurcation diagram, viz. RL0,RL1,A0 and A1. */
  /* These are often convenient for an initial plot of the diagram. */
        
  /* Parameter adjustments */
  /*--thu;*/
  /*--thl;*/
  /*--icp;*/
  /*--par;*/
  
  ndim = iap->ndim;
  ips = iap->ips;
  irs = iap->irs;
  ilp = iap->ilp;
  ntst = iap->ntst;
  ncol = iap->ncol;
  iad = iap->iad;
  isp = iap->isp;
  isw = iap->isw;
  iplt = iap->iplt;
  nbc = iap->nbc;
  nint = iap->nint;
  nmx = iap->nmx;
  nuzr = iap->nuzr;
  npr = iap->npr;
  mxbf = iap->mxbf;
  iid = iap->iid;
  itmx = iap->itmx;
  itnw = iap->itnw;
  nwtn = iap->nwtn;
  jac = iap->jac;
  nfpr = iap->nfpr;
  nicp = iap->nicp;
  
  ds = rap->ds;
  dsmin = rap->dsmin;
  dsmax = rap->dsmax;
  rl0 = rap->rl0;
  rl1 = rap->rl1;
  a0 = rap->a0;
  a1 = rap->a1;
  epsl = rap->epsl;
  epsu = rap->epsu;
  epss = rap->epss;

  xpp::print(s.auto_lib.fp7,"   0 {:12.4E}{:12.4E}{:12.4E}{:12.4E}\n",rl0,rl1,a0,a1);
  xpp::print(s.auto_lib.fp7,"   0   EPSL={:11.4E}  EPSU ={:11.4E}  EPSS ={:11.4E}\n",epsl,epsu,epss);
  xpp::print(s.auto_lib.fp7,"   0   DS  ={:11.4E}  DSMIN={:11.4E}  DSMAX={:11.4E}\n",ds,dsmin,dsmax);
  xpp::print(s.auto_lib.fp7,"   0   NDIM={:4}   IPS ={:4}   IRS ={:4}   ILP ={:4}\n",ndim,xpp::auto_problem_code(ips),irs,ilp); 
  xpp::print(s.auto_lib.fp7,"   0   NTST={:4}   NCOL={:4}   IAD ={:4}   ISP ={:4}\n",ntst,ncol,iad,isp);
  xpp::print(s.auto_lib.fp7,"   0   ISW ={:4}   IPLT={:4}   NBC ={:4}   NINT={:4}\n",isw,iplt,nbc,nint);
  xpp::print(s.auto_lib.fp7,"   0   NMX={:5}   NPR ={:4}   MXBF={:4}   IID ={:4}\n",nmx,npr,mxbf,iid);
  xpp::print(s.auto_lib.fp7,"   0   ITMX={:4}   ITNW={:4}   NWTN={:4}   JAC={:4}  NUZR={:4}\n",itmx,itnw,nwtn,jac,nuzr);

  if (nicp == 1) {
    jtmp = iap->lib->parameter_count;
    xpp::print(s.auto_lib.fp7,"   0   User-specified parameter:       ");
    for (i = 0; i < nicp; ++i) {
      xpp::print(s.auto_lib.fp7,"{:4}",icp[jtmp + i]);
    }
    xpp::print(s.auto_lib.fp7,"\n");
  } else {
    jtmp = iap->lib->parameter_count;
    xpp::print(s.auto_lib.fp7,"   0   User-specified parameters:      ");
    for (i = 0; i < nicp; ++i) {
      xpp::print(s.auto_lib.fp7,"{:4}",icp[jtmp + i]);
    }
    xpp::print(s.auto_lib.fp7,"\n");
  }

  if (nfpr == 1) {
    xpp::print(s.auto_lib.fp7,"   0   Active continuation parameter:  ");
	
    for (i = 0; i < nfpr; ++i) {
      xpp::print(s.auto_lib.fp7,"{:4}",icp[i]);
    }
    xpp::print(s.auto_lib.fp7,"\n");
  } else {
    xpp::print(s.auto_lib.fp7,"   0   Active continuation parameters:  ");
    for (i = 0; i < nfpr; ++i) {
      xpp::print(s.auto_lib.fp7,"{:4}",icp[i]);
    }
    xpp::print(s.auto_lib.fp7,"\n");
  }

  return 0;
} /* sthd_ */

/*     ---------- ------ */
/* Subroutine */ int 
headng(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer iunit, integer *n1, integer *n2)
{
  /* Local variables */
  integer iplt, i, j;
  /* The fixed-width column headings printed below. */
  std::array<std::string, auto_heading_columns> col;
  integer ndm;
  xpp::AutoProblem ips;

  /* Prints headings above columns on unit 6 and 7. */

  /* Parameter adjustments */
  /*--icp;*/
  /*--par;*/

  ips = iap->ips;
  iplt = iap->iplt;
  ndm = iap->ndm;

  /*initialize strings*/
  for (i = 0; i < auto_heading_columns; ++i) {
    col[i].assign(auto_column_width, ' ');
  }

  if (iunit == 6) {
    xpp::log_auto(" \n");
  }
  if (iunit == 7) {
    xpp::print(iap->lib->fp7,"   0\n");
  }
  if (iunit == 9) {
    xpp::print(iap->lib->fp9," \n");	
  }

  j = 0;
  for (i = 0; i < *n1; ++i) {
    ++j;
    if (static_cast<double>(j) == 2.) {
      j = j + 1 + *n2;
    }   
    if (icp[i] >= xpp::auto_unknown::period) {
      col[j-1] = xpp::format("   PAR({})    ",icp[i]);
    } else {
      col[j-1] = xpp::format("   PAR({})     ",icp[i]);
    }
	
  }

  if (iplt > ndm && iplt <= ndm << 1) {
    col[1] = xpp::format(" INTEGRAL U({})",iplt-ndm);
  } else if (iplt > ndm << 1 && iplt <= ndm * 3) {
    col[1] = xpp::format(" L2-NORM U({}) ",iplt - (ndm * 2));
  } else if (iplt > 0 && iplt <= ndm) {
    if (xpp::problem_is_algebraic_family(ips) || ips == xpp::AutoProblem::AlgebraicOptimization) {
      col[1] = xpp::format("     U({})     ",-iplt);
    } else {
      col[1] = xpp::format("   MAX U({})   ",iplt);
    }
  } else if (iplt < 0 && iplt >= -ndm) {
    if (xpp::problem_is_algebraic_family(ips) || ips == xpp::AutoProblem::AlgebraicOptimization) {
      col[1] = xpp::format("     U({})     ",-iplt);
    } else {
      col[1] = xpp::format("   MIN U({})   ",-iplt);
    }
  } else {
    col[1] = "   L2-NORM    ";
  }

  if (*n2 > 0) {
    for (i = 0; i < *n2; ++i) {
      col[i+2] = xpp::format("     U({})     ",i + 1);
    }
    if (xpp::problem_has_solution_profile(ips)) {
      for (i = 3; i <= *n2 + 2; ++i) {
	col[i-1][3]='M';
	col[i-1][4]='A';
	col[i-1][5]='X';
      }
    }
  }

  for (i = 0; i < *n1 + *n2 + 1; ++i) {
    if (col[i] == xpp::format("   PAR({})    ",xpp::auto_unknown::period) && xpp::problem_heads_period(ips)) {
      col[i] = "    PERIOD    ";
    } else if (col[i] == xpp::format("   PAR({})    ",xpp::auto_unknown::objective) && xpp::problem_is_optimization(ips)) {
      col[i] = "     FOPT     ";
    } else if (col[i] == xpp::format("   PAR({})    ",xpp::auto_unknown::evolution_time) && xpp::problem_is_parabolic_evolution(ips)) {
      col[i] = "     TIME     ";
    }
  }

  if (iunit == 6) {
    xpp::log_auto("  BR    PT  TY LAB ");
    for (i = 0; i < *n1 + *n2 + 1; ++i) {
      /* PAR(n)/U(n) as the user named them */
      xpp::log_auto("{}",auto_screen_col(*iap->lib->session, col[i]).c_str());
    }
    xpp::log_auto("\n");
  } else if (iunit == 7) {
    xpp::print(iap->lib->fp7,"   0    PT  TY LAB ");
    for (i = 0; i < *n1 + *n2 + 1; ++i) {
      xpp::print(iap->lib->fp7,"{}",col[i]);
    }
    xpp::print(iap->lib->fp7,"\n");
  } else if (iunit == 9) {
    xpp::print(iap->lib->fp9,"  BR    PT  TY LAB ");	
    for (i = 0; i < *n1 + *n2 + 1; ++i) {
      xpp::print(iap->lib->fp9,"{}",col[i]);
    }
    xpp::print(iap->lib->fp9,"\n");	
  }

  return 0;
} /* headng_ */

/*     ---------- ------ */
/* Subroutine */ int 
stplae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *rlcur, doublereal *u)
{

    /* Local variables */
  integer labw, nins, ntot, i;

  integer istop, ntots;
  doublereal ss;

  integer iab, lab;
  doublereal amp;

  int iflag=0;
  xpp::AutoStopAt stop_at = {0}; /* xppautX: T23 */

/* Stores the bifurcation diagram on unit 7 (Algebraic Problems). */
/* Every line written contains, in order, the following: */

/*  IBR    : The label of the branch. */
/*  NTOT   : The index of the point on the branch. */
/*           (Points are numbered consecutively along a branch). */
/*           If IPS=1 or -1, then the sign of NTOT indicates stability : 
*/
/*            - = stable , + = unstable, unknown, or not relevant. */
/*  ITP    : An integer indicating the type of point : */

/*             1  (BP)  :   Branch point. */
/*             2  (LP)  :   Fold. */
/*             3  (HB)  :   Hopf bifurcation point. */
/*             4  (  )  :   Output point (Every NPR steps along branch). 
*/
/*            -4  (UZ)  :   Output point (Zero of user function). */
/*             9  (EP)  :   End point of branch, normal termination. */
/*            -9  (MX)  :   End point of branch, abnormal termination. */

/*  LAB        : The label of a special point. */
/*  PAR(ICP(1)): The principal parameter. */
/*  A          : The L2-norm of the solution vector, or other measure of 
*/
/*               the solution (see the user-supplied parameter IPLT). */
/*  U          : The first few components of the solution vector. */
/*  PAR(ICP(*)): Further free parameters (if any). */

  /* Parameter adjustments */
  /*--u;*/
  /*--rlcur;*/
  /*--icp;*/

  auto context = point_output_context(*iap, *rap);

  ntot = iap->ntot;
  ++ntot;
  iap->ntot = ntot;

  pvlsae(iap, rap, u, par);

/* ITP is set to 4 every NPR steps along a branch, and the entire */
/* solution is written on unit 8. */

  if (context.npr != 0) {
    if (ntot % context.npr == 0 && context.itp.kind == xpp::AutoPointKind::Regular) {
      context.itp = xpp::AutoPointCode{xpp::AutoPointKind::Output, context.itpst};
    }
    iap->itp = context.itp;
  }

  /* CHECK WHETHER LIMITS OF THE BIFURCATION DIAGRAM HAVE BEEN REACHED : */

  iab = f2c::abs(context.iplt);

  if (iab <= context.ndim && iab > 0) {
    amp = u[-1 + iab];
  } else if (context.iplt > context.ndim && context.iplt <= (context.ndim*2)) {
    amp = u[-1 + context.iplt - context.ndim];
  } else if (context.iplt > (context.ndim*2) && context.iplt <= context.ndim * 3) {
    amp = u[-1 + context.iplt - (context.ndim*2)];
  } else {
    ss = 0.;
    for (i = 0; i < context.ndm; ++i) {
      ss += u[i] * u[i];
    }
    amp = sqrt(ss);
  }
  xpp::byeauto_(&iflag);
  istop = iap->istop;
  /* xppautX: cancel: a point the solve reached is stored as it is; the next
     solve sees the cancel and ends the branch with an EP repeating it, so a
     cancel always ends the same way, wherever it came (xpp_job.h, replay) */
  if (istop == 0 && xpp::job::cancelled()) iflag = 0;
  stop_at.user = iflag == 1 || istop == 1; /* xppautX: T23, unless MX or UZR below */
  if (istop == 1 && !xpp::job::cancelled()) { /* xppautX: cancel: EP, not MX */
    /*        Maximum number of iterations reached somewhere. */
    context.itp = xpp::AutoPointCode{xpp::AutoPointKind::MaxSteps, context.itpst};
    iap->itp = context.itp;
    stop_at.noconv = 1, stop_at.user = 0;
  } else if (istop == -1) {
    /*        ** UZR endpoint */
    context.itp = xpp::AutoPointCode{xpp::AutoPointKind::End, context.itpst};
    iap->itp = context.itp;
  } else {
    if (rlcur[0] < context.rl0 || rlcur[0] > context.rl1 || amp < context.a0 || amp > context.a1 || ntot
	== context.nmx|| iflag==1) {
      istop = 1;
      iap->istop = istop;
      context.itp = xpp::AutoPointCode{xpp::AutoPointKind::End, context.itpst};
      iap->itp = context.itp;
    }
  }

  labw = 0;
  if (context.itp.kind != xpp::AutoPointKind::Regular) {
    lab = iap->lab;
    ++lab;
    iap->lab = lab;
    labw = lab;
  }

  /* Determine stability and print output on units 6 and 7. */

  ntots = ntot;
  nins = iap->nins;
  if (xpp::problem_is_steady_or_map(context.ips) && f2c::abs(context.isw) != 2 && ntot > 1) {
    if (nins == context.ndim) {
      ntots = -ntot;
    }
  }
  xpp::addbif(iap,rap,ntots,iap->ibr,par,icp,labw,&amp, {u,static_cast<size_t>(iap->ndim)}, {u,static_cast<size_t>(iap->ndim)}, {u,static_cast<size_t>(iap->ndim)}, {u,static_cast<size_t>(iap->ndim)});
  wrline(iap, rap, par, icp, &icp[iap->lib->parameter_count], &context.ibr, &ntots,
	 &labw, &amp, u);
  if (iap->istop != 0) { /* xppautX: why the branch ended (auto_stop.h, T23) */
    stop_at.br = context.ibr, stop_at.pt = ntot, stop_at.ipar = icp[0], stop_at.par = rlcur[0], stop_at.norm = amp;
    stop_at.rl0 = context.rl0, stop_at.rl1 = context.rl1, stop_at.a0 = context.a0, stop_at.a1 = context.a1, stop_at.nmx = context.nmx;
    stop_at.mark = iap->istop == -1;
    if (stop_at.mark) stop_at.user = 0;
    xpp::auto_stop_branch_end(*iap->lib->session, &stop_at);
  }

  /* Write restart information for multi-parameter analysis : */

  if (labw != 0) {
    wrtsp8(iap, rap, par, icp, &labw, rlcur, u);
  }

  return 0;
} /* stplae_ */

/*     ---------- ------ */
/* Subroutine */ int 
wrline(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *icu, integer *ibr, integer *ntot, integer *lab, doublereal *vaxis, doublereal *u)
{
  xpp::Session &s=*iap->lib->session;

  /* Local variables */
  integer nicp, i;
  const char *atype = "  ";
  integer n1, n2;

  integer nt, ndm, lb;
  xpp::AutoPointCode itp;
  lb=*lab;
  if((s.auto_lib.restart_flag==1)&&(lb!=0)){
    
    s.auto_lib.restart_flag=0;
    s.auto_state.restart_label=*lab;
  }

/* Write one line of output on unit 6 and 7. */

  /* Parameter adjustments */
  /*--u;*/
  /*--icu;*/
  /*--icp;*/
  /*--par;*/

  ndm = iap->ndm;
  itp = iap->itp;
  nicp = iap->nicp;

  n1 = nicp;
  n2 = ndm;
  nt = n1 + n2;

  if (n1 > auto_diagram_columns) {
    n1 = auto_diagram_columns;
    n2 = 0;
  } else if (nt > auto_diagram_columns) {
    n2 = auto_diagram_columns - n1;
  }

  /* Write a heading above the first line. */

  if (f2c::abs(*ntot) == 1) {
    headng(iap, rap, par, icu, 6, &n1, &n2);
  }
  if (f2c::abs(*ntot) == 1) {
    headng(iap, rap, par, icu, 7, &n1, &n2);
  }
  headng(iap, rap, par, icu, 9, &n1, &n2);

  if (itp.kind == xpp::AutoPointKind::BranchPoint) {
    atype = "BP";
  } else if (itp.kind == xpp::AutoPointKind::Fold) {
    atype = "LP";
  } else if (itp.kind == xpp::AutoPointKind::Hopf) {
    atype = "HB";
  } else if (itp.kind == xpp::AutoPointKind::Output) {
    atype = "  ";
  } else if (itp.kind == xpp::AutoPointKind::UserZero) {
    atype = "UZ";
  } else if (itp.kind == xpp::AutoPointKind::PeriodicFold) {
    atype = "LP";
  } else if (itp.kind == xpp::AutoPointKind::PeriodicBranch) {
    atype = "BP";
  } else if (itp.kind == xpp::AutoPointKind::PeriodDoubling) {
    atype = "PD";
  } else if (itp.kind == xpp::AutoPointKind::Torus) {
    atype = "TR";
  } else if (itp.kind == xpp::AutoPointKind::End) {
    atype = "EP";
  } else if (itp.kind == xpp::AutoPointKind::MaxSteps) {
    atype = "MX";
  } else {
    atype = "  ";
  }

  if (n2 == 0) {
    if (itp.kind != xpp::AutoPointKind::Regular) {
      xpp::log_auto("{:4d} {:5d}  {:c}{:c}{:4d}",(*ibr),*ntot,atype[0],atype[1],*(lab));
      xpp::log_auto("{:14.6E}",par[icu[0]]);
      xpp::log_auto("{:14.6E}",(*vaxis));
      for (i = 1; i < n1; ++i) {
	xpp::log_auto("{:14.6E}",par[icu[i]]);
      }
      xpp::log_auto("\n");
    }
    xpp::print(s.auto_lib.fp7,"{:4} {:5} {:3} {:3}",(*ibr),*ntot,xpp::auto_point_encode(itp),(*lab));
    xpp::print(s.auto_lib.fp7,"{:14.5E}",par[icu[0]]);
    xpp::print(s.auto_lib.fp7,"{:14.5E}",(*vaxis));
    for (i = 1; i < n1; ++i) {
      xpp::print(s.auto_lib.fp7,"{:14.5E}",par[icu[i]]);
    }
    xpp::print(s.auto_lib.fp7,"\n");
    xpp::print(s.auto_lib.fp9,"{:4} {:5}  {:c}{:c}{:4}",(*ibr),*ntot,atype[0],atype[1],*(lab));
    xpp::print(s.auto_lib.fp9,"{:14.6E}",par[icu[0]]);
    xpp::print(s.auto_lib.fp9,"{:14.6E}",(*vaxis));
    for (i = 1; i < n1; ++i) {
      xpp::print(s.auto_lib.fp9,"{:14.6E}",par[icu[i]]);
    }
    xpp::print(s.auto_lib.fp9,"\n");
  } else {
    if (n1 == 1) {
      if (itp.kind != xpp::AutoPointKind::Regular) {
	xpp::log_auto("{:4d} {:5d}  {:c}{:c}{:4d}",f2c::abs(*ibr),f2c::abs(*ntot),atype[0],atype[1],*(lab));
	xpp::log_auto("{:14.6E}",par[icu[0]]);
	xpp::log_auto("{:14.6E}",(*vaxis));
	for (i = 0; i < n2; ++i) {
	  xpp::log_auto("{:14.6E}",u[i]);
	}
	xpp::log_auto("\n");
      }
      xpp::print(s.auto_lib.fp7,"{:4} {:5} {:3} {:3}",(*ibr),*ntot,xpp::auto_point_encode(itp),(*lab));
      xpp::print(s.auto_lib.fp7,"{:14.5E}",par[icu[0]]);
      xpp::print(s.auto_lib.fp7,"{:14.5E}",(*vaxis));
      for (i = 0; i < n2; ++i) {
	xpp::print(s.auto_lib.fp7,"{:14.5E}",u[i]);
      }
      xpp::print(s.auto_lib.fp7,"\n");
      xpp::print(s.auto_lib.fp9,"{:4} {:5}  {:c}{:c}{:4}",(*ibr),*ntot,atype[0],atype[1],*(lab));
      xpp::print(s.auto_lib.fp9,"{:14.6E}",par[icu[0]]);
      xpp::print(s.auto_lib.fp9,"{:14.6E}",(*vaxis));
      for (i = 0; i < n2; ++i) {
	xpp::print(s.auto_lib.fp9,"{:14.6E}",u[i]);
      }
      xpp::print(s.auto_lib.fp9,"\n");	

    } else {
      if (itp.kind != xpp::AutoPointKind::Regular) {
	xpp::log_auto("{:4d} {:5d}  {:c}{:c}{:4d}",f2c::abs(*ibr),f2c::abs(*ntot),atype[0],atype[1],*(lab));
	xpp::log_auto("{:14.6E}",par[icu[0]]);
	xpp::log_auto("{:14.6E}",(*vaxis));
	for (i = 0; i < n2; ++i) {
	  xpp::log_auto("{:14.6E}",u[i]);
	}
	for (i = 1; i < n1; ++i) {
	  xpp::log_auto("{:14.6E}",par[icu[i]]);
	}
	xpp::log_auto("\n");
      }
      xpp::print(s.auto_lib.fp7,"{:4} {:5} {:3} {:3}",(*ibr),*ntot,xpp::auto_point_encode(itp),(*lab));
      xpp::print(s.auto_lib.fp7,"{:14.5E}",par[icu[0]]);
      xpp::print(s.auto_lib.fp7,"{:14.5E}",(*vaxis));
      for (i = 0; i < n2; ++i) {
	xpp::print(s.auto_lib.fp7,"{:14.5E}",u[i]);
      }
      for (i = 1; i < n1; ++i) {
	xpp::print(s.auto_lib.fp7,"{:14.5E}",par[icu[i]]);
      }
      xpp::print(s.auto_lib.fp7,"\n");
      xpp::print(s.auto_lib.fp9,"{:4} {:5}  {:c}{:c}{:4}",(*ibr),*ntot,atype[0],atype[1],*(lab));
      xpp::print(s.auto_lib.fp9,"{:14.6E}",par[icu[0]]);
      xpp::print(s.auto_lib.fp9,"{:14.6E}",(*vaxis));
      for (i = 0; i < n2; ++i) {
	xpp::print(s.auto_lib.fp9,"{:14.6E}",u[i]);
      }
      for (i = 1; i < n1; ++i) {
	xpp::print(s.auto_lib.fp9,"{:14.6E}",par[icu[i]]);
      }
      xpp::print(s.auto_lib.fp9,"\n");	
    }
  }

  return 0;
} /* wrline_ */

/*     ---------- ------ */
/* Subroutine */ int 
wrtsp8(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *lab, doublereal *rlcur, doublereal *u)
{
  xpp::Session &s=*iap->lib->session;
  /* Local variables */
  integer ndim, nfpr, ntpl, jtmp, ntot, i;
  doublereal t;
  integer nrowpr, ibr;
  integer nar, isw;
  xpp::AutoPointCode itp;

  FILE *const fp8=fort8(s.auto_lib);

  /* Write restart information on singular points, plotting points, etc., */
  /* on unit 8. */

  /* Parameter adjustments */
  /*--u;*/
  /*--rlcur;*/
  /*--icp;*/
  /*--par;*/

  ndim = iap->ndim;
  isw = iap->isw;
  itp = iap->itp;
  ibr = iap->ibr;
  nfpr = iap->nfpr;
  ntot = iap->ntot;

  ntpl = 1;
  nar = ndim + 1;
  jtmp = iap->lib->parameter_count;
  nrowpr = ndim / auto_solution_columns + 1 + (jtmp - 1) / auto_solution_columns + 1;
  par[icp[0]] = rlcur[0];
  t = 0.;

  xpp::print(fp8,"{:5}",ibr);
  xpp::print(fp8," {:4}",ntot);
  xpp::print(fp8," {:4}",xpp::auto_point_encode(itp));
  xpp::print(fp8," {:4}",(*lab));
  xpp::print(fp8," {:4}",nfpr);
  xpp::print(fp8," {:4}",isw);
  xpp::print(fp8," {:4}",ntpl);
  xpp::print(fp8," {:4}",nar);
  xpp::print(fp8," {:4}",nrowpr);
  xpp::print(fp8," {:4}",0);
  xpp::print(fp8," {:4}",0);
  xpp::print(fp8," {:4}\n",iap->lib->parameter_count);
  xpp::print(fp8,"    {:19.10E}",t);
  for (i = 0; i < ndim; ++i) {
    if((i>0)&&((i+1)%auto_solution_columns==0))
      xpp::print(fp8,"\n    ");
    xpp::print(fp8,"{:19.10E}",u[i]);
  }
  xpp::print(fp8,"\n");
  for (i = 0; i < iap->lib->parameter_count; ++i) {
    if(i==0)
      xpp::print(fp8,"    ");
    if((i>0)&&(i%auto_solution_columns==0))
      xpp::print(fp8,"\n    ");
    xpp::print(fp8,"{:19.10E}",par[i]);
  }
  xpp::print(fp8,"\n");    
  fflush(fp8);

  return 0;
} /* wrtsp8_ */

/*     ---------- ------ */
/* Subroutine */ int 
wrjac(iap_type *iap, integer *n, integer *m1aaloc, doublereal *aa, doublereal *rhs)
{
  /* System generated locals */
  integer aa_dim1;

  /* Local variables */
  integer i, j;

  /* Parameter adjustments */
  /*--rhs;*/
  aa_dim1 = *m1aaloc;
    
  xpp::print(iap->lib->fp9," Residual vector :\n");	

  for (i = 0; i < *n; ++i) {
    xpp::print(iap->lib->fp9," {:10.3E}",rhs[i]);	
  }
  xpp::print(iap->lib->fp9,"\n");	
  xpp::print(iap->lib->fp9," Jacobian matrix :\n");	
  for (i = 0; i < *n; ++i) {
    for (j = 0; j < *n; ++j) {
      xpp::print(iap->lib->fp9," {:10.3E}",ARRAY2D(aa, i, j));	
    }
    xpp::print(iap->lib->fp9,"\n");	

  }

  return 0;
} /* wrjac_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*                    Mesh and Weight Generation */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/*     ---------- --- */
/* Subroutine */ int 
msh(const iap_type *iap, const rap_type *rap, doublereal *tm)
{

    /* Local variables */
  integer ntst, j;
  doublereal dt;

/* Generates a uniform mesh on [0,1]. */

  /* Parameter adjustments */
  /*--tm;*/

  ntst = iap->ntst;

  tm[0] = 0.;
  dt = 1. / ntst;
  for (j = 0; j < ntst; ++j) {
    tm[j + 1] = (j + 1) * dt;
  }

  return 0;
} /* msh_ */

/*     ---------- ------ */
/* Subroutine */ int 
genwts(const integer ncol, const integer n1, doublereal *wt, doublereal *wp)
{
  /* System generated locals */
  integer wt_dim1, wp_dim1;

    /* Local variables */
  doublereal d;
  integer i, k, l;
  doublereal p, denom;

  integer ib, ic;
  doublereal sum;
  integer ncp1;

  std::vector<doublereal> xm(xpp::size_sum(ncol, 1));
  std::vector<doublereal> zm(ncol);

/* Generates weights of the collocation method. The user selected */
/* number of collocation points (ncol) must be one of { 2,...,7 }. */

/* The following weights are generated : */

/*         WT : for the function value, */
/*         WP : for the first derivative, */

/* Local */

/* Generate the collocation points : */
  /* Parameter adjustments */
  wp_dim1 = n1;
  wt_dim1 = n1;
    
  cpnts(ncol, zm.data());

  ncp1 = ncol + 1;
  d = 1. / ncol;
  for (i = 0; i < ncp1; ++i) {
    xm[i] = i * d;
  }

  /* Generate weights : */

  for (ib = 0; ib < ncp1; ++ib) {
    denom = 1.;
    for (k = 0; k < ncp1; ++k) {
      if (k != ib) {
	denom *= xm[ib] - xm[k];
      }
    }
    for (ic = 0; ic < ncol; ++ic) {
      /* Weights for the function values : */
      p = 1.;
      for (k = 0; k < ncp1; ++k) {
	if (k != ib) {
	  p *= zm[ic] - xm[k];
	}
      }
      ARRAY2D(wt, ib, ic) = p / denom;
      /* Weights for derivatives : */
      sum = 0.;
      for (l = 0; l < ncp1; ++l) {
	if (l != ib) {
	  p = 1.;
	  for (k = 0; k < ncp1; ++k) {
	    if (k != ib && k != l) {
	      p *= zm[ic] - xm[k];
	    }
	  }
	  sum += p;
	}
      }
      ARRAY2D(wp, ib, ic) = sum / denom;
    }
  }

  return 0;
} /* genwts_ */

/*     ---------- ----- */
/* Subroutine */ int 
cpnts(const integer ncol, doublereal *zm)
{
    /* Local variables */
  doublereal c, r, c1, c2, c3;

  /* Generates the collocation points with respect to [0,1]. */
  if (ncol > 7) {
    xpp::auto_fail(xpp::format("Ncol is {}: AUTO has collocation weights for at most 7; set Ncol (Numerics) to 7 or smaller", ncol));
  }

  switch (static_cast<int>(ncol - 1)) {
  case 1:  goto L2;
  case 2:  goto L3;
  case 3:  goto L4;
  case 4:  goto L5;
  case 5:  goto L6;
  case 6:  goto L7;
  }

 L2:
  c = .5 / sqrt(3.);
  zm[0] = .5 - c;
  zm[1] = c + .5;
  return 0;

 L3:
  c = sqrt(.6) * .5;
  zm[0] = .5 - c;
  zm[1] = .5;
  zm[2] = c + .5;
  return 0;

 L4:
  r = .8571428571428571;
  /* Computing 2nd power */
  c = sqrt(r * r - .34285714285714286) * .5;
  c1 = sqrt(c + .42857142857142855) * .5;
  c2 = sqrt(.42857142857142855 - c) * .5;
  zm[0] = .5 - c1;
  zm[1] = .5 - c2;
  zm[2] = c2 + .5;
  zm[3] = c1 + .5;
  return 0;

 L5:
  c1 = .45308992296933198;
  c2 = .26923465505284155;
  zm[0] = .5 - c1;
  zm[1] = .5 - c2;
  zm[2] = .5;
  zm[3] = c2 + .5;
  zm[4] = c1 + .5;
  return 0;

 L6:
  c1 = .46623475710157603;
  c2 = .33060469323313224;
  c3 = .11930959304159845;
  zm[0] = .5 - c1;
  zm[1] = .5 - c2;
  zm[2] = .5 - c3;
  zm[3] = c3 + .5;
  zm[4] = c2 + .5;
  zm[5] = c1 + .5;
  return 0;

 L7:
  c1 = .4745539956171379;
  c2 = .37076559279969723;
  c3 = .20292257568869859;
  zm[0] = .5 - c1;
  zm[1] = .5 - c2;
  zm[2] = .5 - c3;
  zm[3] = .5;
  zm[4] = c3 + .5;
  zm[5] = c2 + .5;
  zm[6] = c1 + .5;
  return 0;
} /* cpnts_ */

/*     ---------- ------ */
/* Subroutine */ int 
cntdif(integer *n, doublereal *d)
{

    /* Local variables */
  integer i, k, k1;
  doublereal sc;

  /* Generates the coefficients of the central difference formula for */
  /* Nth derivative at uniformly spaced points */
  /*              0 = x  < x  < ... < x  = 1. */
  /*                   0    1          N */

  /* Parameter adjustments */
  /*--d;*/

  d[0] = 1.;
  if (*n == 0) {
    return 0;
  }
 
  for (i = 0; i < *n; ++i) {
    d[i + 1] = 0.;
    for (k = 0; k < i + 1; ++k) {
      k1 = i + 1 - k;
      d[k1] = d[k1 - 1] - d[k1];
    }
    d[0] = -d[0];
  }

  /* Scale to [0,1]  : */

  sc = static_cast<doublereal>(pow_ii(*n, *n));
  for (i = 0; i < *n + 1; ++i) {
    d[i] = sc * d[i];
  }

  return 0;
} /* cntdif_ */

/*     ---------- ---- */
/* Subroutine */ int 
wint(const integer n, doublereal *wi)
{
  doublereal c;

  /* Generates the weights for the integration formula based on polynomial */
  /* interpolation at N equally spaced points in [0,1]. */
  
  switch (static_cast<int>(n - 2)) {
  case 1:  goto L3;
  case 2:  goto L4;
  case 3:  goto L5;
  case 4:  goto L6;
  case 5:  goto L7;
  case 6:  goto L8;
  }

 L3:
  c = .16666666666666666;
  wi[0] = c;
  wi[1] = c * 4.;
  wi[2] = c;
  return 0;

 L4:
  c = .125;
  wi[0] = c;
  wi[1] = c * 3.;
  wi[2] = wi[1];
  wi[3] = c;
  return 0;

 L5:
  c = .011111111111111112;
  wi[0] = c * 7.;
  wi[1] = c * 32.;
  wi[2] = c * 12.;
  wi[3] = wi[1];
  wi[4] = wi[0];
  return 0;

 L6:
  wi[0] = .065972222222222224;
  wi[1] = .26041666666666669;
  wi[2] = .1736111111111111;
  wi[3] = wi[2];
  wi[4] = wi[1];
  wi[5] = wi[0];
  return 0;

 L7:
  wi[0] = .04880952380952381;
  wi[1] = .25714285714285712;
  wi[2] = .03214285714285714;
  wi[3] = .32380952380952382;
  wi[4] = wi[2];
  wi[5] = wi[1];
  wi[6] = wi[0];
  return 0;

 L8:
  wi[0] = .043460648148148151;
  wi[1] = .20700231481481482;
  wi[2] = .076562500000000006;
  wi[3] = .17297453703703702;
  wi[4] = wi[3];
  wi[5] = wi[2];
  wi[6] = wi[1];
  wi[7] = wi[0];

  return 0;
} /* wint_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*          Stepsize and Mesh Adaption */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/*     ---------- ------ */
/* Subroutine */ int 
adptds(iap_type *iap, rap_type *rap, doublereal *rds)
{
  /* Local variables */
  doublereal ards;
  integer ntop, itnw, ntot;
  doublereal dsmax;
  integer n1, ibr, nit;

/* The stepsize along the branch of solutions is adapted depending on the 
*/
/* number of Newton iterations in the previous step (called if IADS > 0). 
*/

  dsmax = rap->dsmax;
  itnw = iap->itnw;
  ibr = iap->ibr;
  nit = iap->nit;
  ntot = iap->ntot;
  ntop = ntot + 1;

  if (itnw <= 3) {
    itnw = 3;
    n1 = 2;
  } else {
    n1 = itnw / 2;
  }

  if (nit <= 1) {
    *rds *= 2.;
  } else if (nit == 2) {
    *rds *= 1.5;
  } else if (nit > 2 && nit <= n1) {
    *rds *= 1.1;
  } else if (nit >= itnw) {
    *rds *= .5;
  }

  ards = f2c::abs(*rds);
  if (ards > dsmax) {
    *rds = *rds * dsmax / ards;
  }

  xpp::print(iap->lib->fp9,"{:4} {:5}        Iterations     {:3}\n",f2c::abs(ibr),ntop - 1,nit);
  xpp::print(iap->lib->fp9,"{:4} {:5}        Stepsize      {:14.6E}\n",f2c::abs(ibr),ntop,(*rds));

  return 0;
} /* adptds_ */

/*     ---------- ----- */
/* Subroutine */ int 
adapt(iap_type *iap, rap_type *rap, integer *nold, integer *ncold, integer *nnew, integer *ncnew, doublereal *tm, doublereal *dtm, integer *ndxloc, doublereal *ups, doublereal *vps)
{
  /* System generated locals */
  integer ups_dim1, vps_dim1;

    /* Local variables */
  integer ndim, iper;
  integer i, j, noldp1, nnewp1;

  integer nrwnew;
  integer isw;
  xpp::AutoProblem ips;

  std::vector<doublereal> uint(xpp::size_product(*ndxloc, xpp::size_product(iap->ndim, iap->ncol)));
  std::vector<doublereal> tint(*ndxloc);
  std::vector<doublereal> tm2(*ndxloc);
  std::vector<integer> itm(*ndxloc);

  /* Adapts the distribution of the mesh points so that the increase of the */
  /* monotone function EQDF becomes approximately equidistributed over the */
  /* intervals. The functions UPS and VPS are interpolated on new mesh. */

/* Local */

  /* Parameter adjustments */
  /*--tm;*/
  /*--dtm;*/
  vps_dim1 = *ndxloc;
  ups_dim1 = *ndxloc;
    
  ndim = iap->ndim;
  ips = iap->ips;
  isw = iap->isw;

  noldp1 = *nold + 1;
  nnewp1 = *nnew + 1;
  nrwnew = ndim * *ncnew;

  for (j = 0; j < (*ndxloc); ++j) {
    for (i = 0; i < (iap->ndim * iap->ncol); ++i) {
      uint[j + i * (*ndxloc)] = 0.;
    }
  }

  /* For periodic boundary conditions extrapolate by periodicity. */

  if ((ips == xpp::AutoProblem::Periodic || ips == xpp::AutoProblem::WavePeriodic)
      && f2c::abs(isw) != 2) {
    iper = 1;
  } else {
    iper = 0;
  }

  /* Generate the new mesh : */

  newmsh(iap, rap, ndxloc, ups, nold, 
	 ncold, tm, dtm, nnew, tint.data(), &iper);

  /* Replace UPS by its interpolant on the new mesh : */

  interp(iap, rap, &ndim, &noldp1, ncold, tm, ndxloc, ups, &nnewp1, ncnew, tint.data(), uint.data(), tm2.data(), itm.data());
  for (j = 0; j < nnewp1; ++j) {
    for (i = 0; i < nrwnew; ++i) {
      ARRAY2D(ups, j, i) = uint[j + i * (*ndxloc)];
    }
  }

  /* Replace VPS by its interpolant on the new mesh : */

  interp(iap, rap, &ndim, &noldp1, ncold, tm, ndxloc, 
	 vps, &nnewp1, ncnew, tint.data(), uint.data(), tm2.data(), itm.data());
  for (j = 0; j < nnewp1; ++j) {
    for (i = 0; i < nrwnew; ++i) {
      ARRAY2D(vps, j, i) = uint[j + i * (*ndxloc)];
    }
  }

  /* Replace old mesh : */

  tm[0] = 0.;
  for (j = 0; j < *nnew; ++j) {
    dtm[j] = tint[j+1] - tint[j];
    tm[j + 1] = tint[j+1];
  }

  return 0;
} /* adapt_ */

/*     ---------- ------ */
/* Subroutine */ int 
interp(iap_type *iap, rap_type *rap, integer *ndim, integer *n, integer *nc, doublereal *tm, integer *ndxloc, doublereal *ups, integer *n1, integer *nc1, doublereal *tm1, doublereal *ups1, doublereal *tm2, integer *itm1)
{
  /* System generated locals */
  integer ups_dim1, ups1_dim1;

  /* Local variables */

  doublereal d;
  integer i, j, k, l;
  doublereal z__;
  integer j1, k1, l1;
  doublereal ri;

  integer n1m1, ncp1;

  std::vector<doublereal> w(xpp::size_sum(*nc, 1));
  std::vector<doublereal> x(xpp::size_sum(*nc, 1));

/* Finds interpolant (TM(.) , UPS(.) ) on new mesh TM1. */

/* Local */

  /* Parameter adjustments */
  /*--tm;*/
  /*--tm1;*/
  /*--tm2;*/
  /*--itm1;*/
  ups1_dim1 = *ndxloc;
  ups_dim1 = *ndxloc;
    
  ncp1 = *nc + 1;
  n1m1 = *n1 - 1;

  for (i = 0; i < *nc1; ++i) {
    ri = static_cast<doublereal>(i);
    d = ri / *nc1;
    for (j1 = 0; j1 < n1m1; ++j1) {
      tm2[j1] = tm1[j1] + d * (tm1[j1 + 1] - tm1[j1]);
    }
    ordr(iap, rap, n, tm, &n1m1, tm2, itm1);
    for (j1 = 0; j1 <n1m1; ++j1) {
      j = itm1[j1];
      z__ = tm2[j1];
      d = (tm[j] - tm[-1 + j]) / *nc;
      for (l = 0; l < ncp1; ++l) {
	x[l] = tm[-1 + j] + l * d;
      }
      intwts(iap, rap, &ncp1, &z__, x.data(), w.data());
      for (k = 0; k < *ndim; ++k) {
	k1 = i * *ndim + k;
	ARRAY2D(ups1, j1, k1) = w[ncp1 - 1] * ARRAY2D(ups, j, k);
	for (l = 0; l < *nc; ++l) {
	  l1 = k + l * *ndim;
	  ARRAY2D(ups1, j1, k1) += w[l] * ARRAY2D(ups, (j - 1), l1);
	}
      }
    }
  }

  for (i = 0; i < *ndim; ++i) {
    ARRAY2D(ups1, (*n1 - 1), i) = ARRAY2D(ups, (*n - 1), i);
  }

  return 0;
} /* interp_ */

/*     ---------- ------ */
/* Subroutine */ int 
newmsh(iap_type *iap, rap_type *rap, integer *ndxloc, doublereal *ups, integer *nold, integer *ncold, doublereal *tmold, doublereal *dtmold, integer *nnew, doublereal *tmnew, integer *iper)
{
    /* Local variables */

  integer ndim;

  integer j;
  integer j1, noldp1, nnewp1;
  doublereal dal;

  std::vector<doublereal> uneq(xpp::size_sum(*nnew, 1));
  std::vector<doublereal> eqf(xpp::size_sum(*nold, 1));
  std::vector<doublereal> density(*nold);
  std::vector<integer> ial(xpp::size_sum(*nnew, 1));

  /* Redistributes the mesh according to the function EQDF. */

/* Local */

  /* Parameter adjustments */
  /*--tmnew;*/
  /*--dtmold;*/
  /*--tmold;*/

  ndim = iap->ndim;

  /* Put the values of the monotonely increasing function EQDF in EQF. */

  eqdf(iap, rap, nold, &ndim, ncold, dtmold, ndxloc, ups, eqf.data(), density,
       iper);

  /* Uniformly divide the range of EQDF : */

  noldp1 = *nold + 1;
  nnewp1 = *nnew + 1;
  dal = eqf[noldp1 - 1] / *nnew;
  for (j = 0; j < nnewp1; ++j) {
    uneq[j] = (j) * dal;
  }

  ordr(iap, rap, &noldp1, eqf.data(), &nnewp1, uneq.data(), ial.data());

/* Generate the new mesh in TMNEW : */

  // Keep the endpoints exact; recover distance from the nearer endpoint
  // using EQDF's density, without subtracting accumulated monitor values.
  tmnew[0] = tmold[0];
  for (j1 = 1; j1 < *nnew; ++j1) {
    j = ial[j1];
    const doublereal left = uneq[j1] - eqf[j - 1];
    const doublereal right = eqf[j] - uneq[j1];
    tmnew[j1] = left <= right ? tmold[j - 1] + left / density[j - 1]
                             : tmold[j] - right / density[j - 1];
  }
  tmnew[*nnew] = tmold[*nold];

  return 0;
} /* newmsh_ */

/*     ---------- ---- */
/* Subroutine */ int 
ordr(iap_type *iap, rap_type *rap, integer *n, doublereal *tm, integer *n1, doublereal *tm1, integer *itm1)
{

    /* Local variables */
  integer j, k0, j1, k1=0;

  /* TM and TM1 are two ascending arrays with values in [0,1]. On exit the 
*/
/* value of ITM1( i ) specifies the index of the TM-interval in which */
/* TM1(i) lies. */

  /* Parameter adjustments */
  /*--tm;*/
  /*--itm1;*/
  /*--tm1;*/

  k0 = 2;
  for (j1 = 0; j1 < *n1; ++j1) {
    for (j = k0; j <= *n; ++j) {
      k1 = j;
      if (tm1[j1] < tm[-1 + j]) {
	goto L1;
      }
    }
  L1:
    itm1[j1] = k1 - 1;
    k0 = k1;
  }

  return 0;
} /* ordr_ */

/*     ---------- ------ */
/* Subroutine */ int 
intwts(iap_type *iap, rap_type *rap, integer *n, doublereal *z__, doublereal *x, doublereal *wts)
{

    /* Local variables */
  integer k;
  doublereal p, denom;
  integer ib;

  /* Generates weights for Lagrange interpolation. */

  /* Parameter adjustments */
  /*--wts;*/
  /*--x;*/

  for (ib = 0; ib < *n; ++ib) {
    p = 1.;
    denom = 1.;
    for (k = 0; k < *n; ++k) {
      if (k != ib) {
	p *= *z__ - x[k];
	denom *= x[ib] - x[k];
      }
    }
    wts[ib] = p / denom;
  }

  return 0;
} /* intwts_ */

/*     ---------- ---- */
/* Subroutine */ int 
eqdf(iap_type *iap, rap_type *rap, integer *ntst, integer *ndim, integer *ncol, doublereal *dtm, integer *ndxloc, doublereal *ups, doublereal *eqf, std::span<doublereal> density, integer *iper)
{
  /* System generated locals */
  integer ups_dim1;

  /* Local variables */
  doublereal dtav, e;
  integer i, j, k;
  logical small;
  integer k1;
  doublereal sc;

  integer jp1;
  doublereal pwr;

  std::vector<doublereal> hd(xpp::size_product(xpp::size_sum(*ntst, 1), xpp::size_product(*ndim, *ncol)));
  std::vector<doublereal> wh(xpp::size_sum(*ncol, 1));

  /* Compute approximation to NCOL-th derivative : */
  /* Parameter adjustments */
  /*--dtm;*/
  /*--eqf;*/
  ups_dim1 = *ndxloc;

  cntdif(ncol, wh.data());

  small = TRUE_;
  for (j = 0; j < *ntst; ++j) {
    jp1 = j + 1;
    sc = 1. / pow_di(&dtm[j], ncol);
    for (i = 0; i < *ndim; ++i) {
      hd[j + i * (*ntst + 1)] = wh[*ncol] * ARRAY2D(ups, jp1, i);
      for (k = 0; k < *ncol; ++k) {
	k1 = i + k * *ndim;
	hd[j + i * (*ntst + 1)] += wh[k] * ARRAY2D(ups, j, k1);
      }
      hd[j + i * (*ntst + 1)] = sc * hd[j + i * (*ntst + 1)];
      if (f2c::abs(hd[j + i * (*ntst + 1)]) > auto_hmach) {
	small = FALSE_;
      }
    }
  }

  /* Take care of "small derivative" case. */

  if (small) {
    for (i = 0; i < *ntst + 1; ++i) {
      eqf[i] = static_cast<doublereal>(i);
    }
    for (j = 0; j < *ntst; ++j) density[j] = 1. / dtm[j];
    return 0;
  }

  if (*iper == 1) {
    /*        *Extend by periodicity : */
    for (i = 0; i < *ndim; ++i) {
      hd[*ntst + i * (*ntst + 1)] = hd[i * (*ntst + 1)];
    }
    dtm[*ntst] = dtm[0];
  } else {
    /*        *Extend by extrapolation : */
    for (i = 0; i < *ndim; ++i) {
      hd[*ntst + i * (*ntst + 1)] = hd[(*ntst -1) + i * (*ntst + 1)] * 2 - hd[(*ntst - 2) + i * (*ntst + 1)];
    }
    dtm[*ntst] = dtm[-1 + *ntst];
  }

  /* Compute approximation to (NCOL+1)-st derivative : */

  for (j = 0; j < *ntst; ++j) {
    jp1 = j + 1;
    dtav = (dtm[j] + dtm[j + 1]) * .5;
    sc = 1. / dtav;
    for (i = 0; i < *ndim; ++i) {
      hd[j + i * (*ntst + 1)] = sc * (hd[j + 1 + i * (*ntst + 1)] - hd[j + i * (*ntst + 1)]);
    }
  }

  /* Define the equidistribution function : */

  pwr = 1. / (*ncol + 1.);
  eqf[0] = 0.;
  for (j = 0; j < *ntst; ++j) {
    e = 0.;
    for (i = 0; i < *ndim; ++i) {
      doublereal tmp = f2c::abs(hd[j + i * (*ntst + 1)]);
      e += pow_dd(&tmp, &pwr);
    }
    density[j] = e;
    eqf[j + 1] = eqf[j] + dtm[j] * e;
  }
  return 0;
} /* eqdf_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*                    General Support Routines */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/*     ---------- --- */


/*     ---------- ---- */


/*     ---------- ----- */
/* Subroutine */ int 
nrmlz(integer *ndim, doublereal *v)
{
    /* Local variables */
  doublereal c;
  integer i;
  doublereal ss;

  /* Scale the vector V so that its discrete L2-norm becomes 1. */

  /* Parameter adjustments */
  /*--v;*/

  ss = 0.;
  for (i = 0; i < *ndim; ++i) {
    ss += v[i] * v[i];
  }
  c = 1. / sqrt(ss);
  for (i = 0; i < *ndim; ++i) {
    v[i] *= c;
  }

  return 0;
} /* nrmlz_ */

/*     ------ --------- -------- */
doublereal 
pi(doublereal r)
{
  /* System generated locals */
  doublereal ret_val;

  ret_val = r * 4. * xpp::math::atan(1.);

  return ret_val;
} /* pi */

/*     ---------- -- */


/*     ---------- ------ */
/* Subroutine */ int 
newlab(iap_type *iap, rap_type *rap)
{

  /* Local variables */
  integer mlab, ibrs;

  integer labrs, nskip, lab, ibr, mbr, irs, isw;
  xpp::AutoProblem ips;
  xpp::AutoPointCode itp;
  logical eof3;

  /* Determine a suitable label when restarting. */

  ips = iap->ips;
  irs = iap->irs;
  isw = iap->isw;
  itp = iap->itp;

  mbr = 0;
  mlab = 0;
  if (auto r=xpp::rewind_stream(iap->lib->fp3, xpp::auto_fort_path(*iap->lib->session,3)); !r)
    xpp::auto_fail(r.error());
  xpp::TokenReader tr = xpp::TokenReader::attach(iap->lib->fp3);

 L1:
  {
    xpp::AutoSolutionHeader header;
    if (!xpp::read_auto_solution_header(tr, header, iap->lib->parameter_count)) goto L2;
    ibrs = header.ibr;
    labrs = header.lab;
    nskip = header.nskip;
  }
  /*go to the end of the line*/
  tr.skip_line();

  if (ibrs > mbr) {
    mbr = ibrs;
  }
  if (labrs > mlab) {
    mlab = labrs;
  }
  skip3(iap->lib->fp3, &nskip, &eof3);
  if (! eof3) {
    goto L1;
  }

 L2:
  /* A fresh run has an empty fort.3, but the diagram may retain older
     branches. Their labels must still identify their own restart solution. */
  for(const auto &point:iap->lib->session->diagram.points){
    mbr=std::max(mbr,static_cast<integer>(f2c::abs(point.ibr)));
    mlab=std::max(mlab,static_cast<integer>(point.lab));
  }
  lab = mlab;
  iap->lab = lab;
  if (isw < 0 || irs == 0) {
    ibr = mbr + 1;
    iap->ibr = ibr;
  } else if ((itp.start == xpp::AutoPointKind::Regular && f2c::abs(isw) == 2) || (ips == xpp::AutoProblem::Periodic && itp.is(xpp::AutoPointKind::Hopf)) || 
	     (ips == xpp::AutoProblem::BoundaryValue && isw == 2 && itp.start == xpp::AutoPointKind::Regular) || (ips == xpp::AutoProblem::AlgebraicOptimization && itp.kind == xpp::AutoPointKind::Fold)) {
    ibr = irs;
    iap->ibr = ibr;
  }

  return 0;
} /* newlab_ */

bool read_auto_restart_header(iap_type *iap, const rap_type *rap,
                              xpp::TokenReader &reader, xpp::AutoSolutionHeader &header,
                              integer *ntst, integer *ncol) {
  integer free;
  logical found;
  findlb(iap, rap, iap->irs, &free, &found);
  reader = xpp::TokenReader::attach(iap->lib->fp3);
  if (!xpp::read_auto_solution_header(reader, header, iap->lib->parameter_count)) return false;
  *ntst = header.ntst;
  *ncol = header.ncol;
  iap->ibr = header.ibr;
  return true;
}

int 
findlb(iap_type *iap, const rap_type *rap, 
       integer irs, integer *nfpr, logical *found)
{
  /* Local variables */
  integer labrs, nskip, ibr, isw;
  xpp::AutoPointKind itpst;
  xpp::AutoPointCode itp;
  logical eof3;
  long line_start;

  /* Locates restart point with label IRS and determines type. */
  /* If the label can not be located on unit 3 then FOUND will be .FALSE. */

  *found = FALSE_;
  if (auto r=xpp::rewind_stream(iap->lib->fp3, xpp::auto_fort_path(*iap->lib->session,3)); !r)
    xpp::auto_fail(r.error());
  xpp::TokenReader tr = xpp::TokenReader::attach(iap->lib->fp3);
  isw = iap->isw;

  while(1) {
    /* where this label line starts: readlb() reads it again from here */
    const auto position=xpp::tell_stream(iap->lib->fp3, xpp::auto_fort_path(*iap->lib->session,3));
    if (!position) xpp::auto_fail(position.error());
    line_start = *position;
    xpp::AutoSolutionHeader header;
    if (!xpp::read_auto_solution_header(tr, header, iap->lib->parameter_count)) break;
    ibr = header.ibr;
    itp = header.itp;
    labrs = header.lab;
    *nfpr = header.nfpr;
    nskip = header.nskip;
    /*go to the end of the line*/
    tr.skip_line();
    iap->itp = itp;
    iap->ibr = ibr;
    if (labrs == irs) {
      *found = TRUE_;
      if (f2c::abs(isw) == 2) {
	itpst = xpp::auto_point_start_of(itp);
	iap->itpst = itpst;
      } else {
        itpst = xpp::AutoPointKind::Regular;
	iap->itpst = itpst;
      }
      /* Back to the start of the line with an absolute seek. The old
	 backward walk with fseek(fp3,-2,SEEK_CUR) is undefined on a text
	 stream in the Windows C runtime (CRLF translation): it left the
	 stream mid-line, so readlb() parsed garbage for nar and wrote
	 that many values past the end of u, corrupting the heap. */
      if (auto r=xpp::seek_stream(iap->lib->fp3, line_start, xpp::auto_fort_path(*iap->lib->session,3)); !r)
        xpp::auto_fail(r.error());
      return 0;
    } else {
      skip3(iap->lib->fp3, &nskip, &eof3);
      if (eof3) {
	break;
      }
    }
  }
  return 0;
} 

/*     ---------- ------ */
/* Subroutine */ int 
readlb(const iap_type *iap, const rap_type *rap, doublereal *u, doublereal *par)
{
  /* Local variables */
  integer ndim;

  integer nar;

  /* Reads the restart data for algebraic problems. findlb() has
     already validated the label line this is reading (it's the same
     line, re-read from its start), so these reads are not expected to
     fail; still, bail out before ndim (read from nar) or nparr can be
     used as a garbage loop bound or array size if one ever does. */

  xpp::TokenReader tr = xpp::TokenReader::attach(iap->lib->fp3);
  xpp::AutoSolutionHeader header;
  if (!xpp::read_auto_solution_header(tr, header, iap->lib->parameter_count)) return 1;
  nar = header.nar;
  ndim = nar - 1;
  if (ndim > iap->ndim) return 1;
  doublereal time;
  if (!xpp::read_auto_solution_rows(tr, ndim, 0, 0, 1, 0, ndim,
                                    u, &time, false, 0, {})) return 1;
  if (!xpp::read_auto_solution_parameters(tr, header, *iap->lib->session, par)) return 1;

  return 0;
} /* readlb_ */

/*     ---------- ----- */
/* Subroutine */ int 
skip3(FILE *fp3, integer *nskip, logical *eof3)
{

  /* Local variables */
  integer i;

  /* Skips the specified number of lines on unit 3. */

  *eof3 = FALSE_;
  xpp::TokenReader tr = xpp::TokenReader::attach(fp3);
  for (i = 0; i < *nskip; ++i) {
    /* NOTE from Randy:  I am not 100% happy with this.  I am
       not sure if this properly simulates the Fortran behavior */
    if (!tr.skip_line()) { /* end of file before the line's \n */
      *eof3 = TRUE_;
      return 0;
    }
  }
  return 0;
} /* skip3_ */

/*     ------ --------- -------- ----- */
doublereal 
rinpr(iap_type *iap, const integer *ndim1, const integer *ndxloc, doublereal *ups, doublereal *vps, doublereal *dtm, doublereal *thu)
{
  /* System generated locals */
  integer ups_dim1, vps_dim1;
  doublereal ret_val;

  /* Local variables */

  integer i, j, k;
  doublereal s;
  integer k1;
  doublereal sj;
  integer jp1;

  /* Computes the L2 inner product of UPS and VPS. */
  /* (Using the first NDIM1 components only.) */

  /* Local */

  /* Parameter adjustments */
  /*--dtm;*/
  /*--thu;*/
  vps_dim1 = *ndxloc;
  ups_dim1 = *ndxloc;
    
  auto [ndim, ntst, ncol, wi] = integral_context(*iap);

  s = 0.;
  for (j = 0; j < ntst; ++j) {
    jp1 = j + 1;
    sj = 0.;
    for (i = 0; i < *ndim1; ++i) {
      for (k = 0; k < ncol; ++k) {
	k1 = k * ndim + i;
	sj += wi[k] * thu[i] * ARRAY2D(ups, j, k1) * ARRAY2D(vps, j, k1);
      }
      sj += wi[ncol] * thu[i] * ARRAY2D(ups, jp1, i) * ARRAY2D(vps, jp1, i);
    }
    s += dtm[j] * sj;
  }

  ret_val = s;

  return ret_val;
} /* rinpr_ */

/*     ------ --------- -------- ------ */
doublereal 
rnrmsq(iap_type *iap, integer *ndim1, integer *ndxloc, doublereal *ups, doublereal *dtm, doublereal *thu)
{
  /* System generated locals */
  doublereal ret_val;

    /* Local variables */

/* Finds the norm of UPS (first NDIM1 components are included only). */

  /* Parameter adjustments */
  /*--thu;*/

  ret_val = rinpr(iap, ndim1, ndxloc, ups, ups, dtm, thu);

  return ret_val;
} /* rnrmsq_ */

/*     ------ --------- -------- ----- */
doublereal 
rintg(iap_type *iap, integer *ndxloc, integer ic, doublereal *ups, doublereal *dtm)
{
  /* System generated locals */
  integer ups_dim1;
  doublereal ret_val;

  /* Local variables */

  integer j, k;
  doublereal s;
  integer k1;
  doublereal sj;
  integer jp1;

  /* Computes the integral of the IC'th component of UPS. */

/* Local */

  /* Parameter adjustments */
  /*--dtm;*/
  ups_dim1 = *ndxloc;
  
  auto [ndim, ntst, ncol, wi] = integral_context(*iap);
  s = 0.;
  for (j = 0; j < ntst; ++j) {
    jp1 = j + 1;
    sj = 0.;
    for (k = 0; k < ncol; ++k) {
      k1 = k * ndim + ic - 1;
      sj += wi[k] * ARRAY2D(ups, j, k1);
    }
    sj += wi[ncol] * ARRAY2D(ups, jp1, (ic - 1));
    s += dtm[j] * sj;
  }

  ret_val = s;

  return ret_val;
} /* rintg_ */

/*     ------ --------- -------- ----- */
doublereal 
rnrm2(iap_type *iap, integer *ndxloc, integer *ic, doublereal *ups, doublereal *dtm)
{
  /* System generated locals */
  integer ups_dim1;
  doublereal ret_val;

  /* Local variables */

  integer j, k;
  doublereal s;
  integer k1;
  doublereal sj;
  integer jp1;

  /* Computes the L2-norm of the IC'th component of UPS. */

/* Local */

  /* Parameter adjustments */
  /*--dtm;*/
  ups_dim1 = *ndxloc;
  
  auto [ndim, ntst, ncol, wi] = integral_context(*iap);
  s = 0.;
  for (j = 0; j < ntst; ++j) {
    jp1 = j + 1;
    sj = 0.;
    for (k = 0; k < ncol; ++k) {
      k1 = k * ndim + *ic - 1;
      /* Computing 2nd power */
      sj += wi[k] * (ARRAY2D(ups, j, k1) * ARRAY2D(ups, j, k1));
    }
    /* Computing 2nd power */
    sj += wi[ncol] * (ARRAY2D(ups, jp1, (*ic - 1)) * ARRAY2D(ups, jp1, (*ic - 1)));
    s += dtm[j] * sj;
  }

  ret_val = sqrt(s);

  return ret_val;
} /* rnrm2_ */

/*     ------ --------- -------- ------ */
doublereal 
rmxups(iap_type *iap, integer *ndxloc, integer *i, doublereal *ups)
{
  /* System generated locals */
  integer ups_dim1;
  doublereal ret_val;

  /* Local variables */
  integer ndim, ncol, ntst, j, k, k1;

  /* Computes the maximum of the I'th component of UPS. */

  /* Parameter adjustments */
  ups_dim1 = *ndxloc;
  
  ndim = iap->ndim;
  ntst = iap->ntst;
  ncol = iap->ncol;

  ret_val = ARRAY2D(ups, 0, (*i - 1));

  for (j = 0; j < ntst; ++j) {
    for (k = 0; k < ncol; ++k) {
      k1 = k * ndim + *i - 1;
      if (ARRAY2D(ups, j, k1) > ret_val) {
	ret_val = ARRAY2D(ups, j, k1);
      }
    }
  }
  if (ARRAY2D(ups, ntst, (*i - 1)) > ret_val) {
    ret_val = ARRAY2D(ups, ntst, (*i - 1));
  }

  return ret_val;
} /* rmxups_ */

/*     ------ --------- -------- ------ */
doublereal 
rmnups(iap_type *iap, integer *ndxloc, integer *i, doublereal *ups)
{
  /* System generated locals */
  integer ups_dim1;
  doublereal ret_val;

  /* Local variables */
  integer ndim, ncol, ntst, j, k, k1;

  /* Computes the minimum of the I'th component of UPS. */

  /* Parameter adjustments */
  ups_dim1 = *ndxloc;
  
  ndim = iap->ndim;
  ntst = iap->ntst;
  ncol = iap->ncol;

  ret_val = ARRAY2D(ups, 0, (*i - 1));

  for (j = 0; j < ntst; ++j) {
    for (k = 0; k < ncol; ++k) {
      k1 = k * ndim + (*i - 1);
      if (ARRAY2D(ups, j, k1) < ret_val) {
	ret_val = ARRAY2D(ups, j, k1);
      }
    }
  }
  if (ARRAY2D(ups, ntst, (*i - 1)) < ret_val) {
    ret_val = ARRAY2D(ups, ntst, (*i - 1 ));
  }

  return ret_val;
} /* rmnups_ */

/*     ---------- ------ */
/* Subroutine */ int 
scaleb(iap_type *iap, integer *icp, integer *ndxloc, doublereal *dvps, doublereal *rld, doublereal *dtm, doublereal *thl, doublereal *thu)
{
  /* System generated locals */
  integer dvps_dim1;

  /* Local variables */
  integer ndim, ncol, nfpr, nrow, ntst, i, j;
  doublereal sc, ss;

/* Scales the vector (DVPS,RLD) so its norm becomes 1. */

  /* Parameter adjustments */
  /*--icp;*/
  /*--rld;*/
  /*--dtm;*/
  /*--thl;*/
  /*--thu;*/
  dvps_dim1 = *ndxloc;

  ndim = iap->ndim;
  ntst = iap->ntst;
  ncol = iap->ncol;
  nfpr = iap->nfpr;

  ss = rnrmsq(iap, &ndim, ndxloc, dvps, dtm, thu);
  for (i = 0; i < nfpr; ++i) {
    /* Computing 2nd power */
    ss += thl[icp[i]] * (rld[i] * rld[i]);
  }

  sc = 1. / sqrt(ss);

  nrow = ndim * ncol;
  for (j = 0; j < ntst; ++j) {
    for (i = 0; i < nrow; ++i) {
      ARRAY2D(dvps, j, i) *= sc;
    }
  }

  for (i = 0; i < ndim; ++i) {
    ARRAY2D(dvps, ntst, i) *= sc;
  }

  for (i = 0; i < nfpr; ++i) {
    rld[i] = sc * rld[i];
  }

  return 0;
} /* scaleb_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*                    General Boundary Value Problems */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/*     ---------- ------ */
/* Subroutine */ int 
cnrlbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), STPNT_TYPE_BVP((*stpnt)), PVLI_TYPE_BVP((*pvli)), doublereal *thl, doublereal *thu, std::span<const AutoMark> marks)
{
    /* Local variables */
  integer iads, ndim, ncol;
  integer ntot, ntst, nuzr, i, j;
  doublereal dsold;
  integer nodir = 0;
  std::vector<doublereal> rlold(iap->lib->parameter_count);
  std::vector<doublereal> rldot(iap->lib->parameter_count);
  std::vector<doublereal> rlcur(iap->lib->parameter_count);
  integer nitps, istop;
  xpp::AutoPointKind itpst;
  doublereal ds;
  doublereal bp1;
  doublereal sp1;
  integer iad;
  integer ilp, isp, irs;
  xpp::AutoProblem ips;
  doublereal rds, rlp;
  integer isw;
  xpp::AutoPointCode itp;
    
  integer ntst_plus_one=iap->ntst + 1;

  std::vector<doublereal> ups(xpp::size_product(xpp::size_sum(iap->ntst, 1), xpp::size_product(iap->ndim, iap->ncol)));
  std::vector<doublereal> upoldp(xpp::size_product(xpp::size_sum(iap->ntst, 1), xpp::size_product(iap->ndim, iap->ncol)));
  std::vector<doublereal> uoldps(xpp::size_product(xpp::size_sum(iap->ntst, 1), xpp::size_product(iap->ndim, iap->ncol)));
  std::vector<doublereal> udotps(xpp::size_product(xpp::size_sum(iap->ntst, 1), xpp::size_product(iap->ndim, iap->ncol)));
  std::vector<doublereal> dups(xpp::size_product(xpp::size_sum(iap->ntst, 1), xpp::size_product(iap->ndim, iap->ncol)));
  std::vector<doublereal> fa(xpp::size_product(xpp::size_sum(iap->ntst, 1), xpp::size_product(iap->ndim, iap->ncol)));
  std::vector<doublereal> dtm(xpp::size_sum(iap->ntst, 1));
  std::vector<doublereal> tm(xpp::size_sum(iap->ntst, 1));
  std::vector<doublereal> fc(xpp::size_sum(iap->nbc, iap->nint, 1));
  std::vector<doublereal> p0(xpp::size_product(iap->ndim, iap->ndim));
  std::vector<doublereal> p1(xpp::size_product(iap->ndim, iap->ndim));
  std::vector<doublecomplex> ev(iap->ndim);
  if (iap->isw==2 && xpp::problem_has_orbit_multipliers(iap->ips)) iap->lib->codim.prepare_cycle(iap->ndim,iap->ndm);
  std::vector<doublereal> uzr(iap->nuzr);

  /* INITIALIZE COMPUTATION OF BRANCH */

  /* Parameter adjustments */
  /*--icp;*/
  /*--par;*/

  ndim = iap->ndim;
  ips = iap->ips;
  irs = iap->irs;
  ilp = iap->ilp;
  ntst = iap->ntst;
  ncol = iap->ncol;
  iad = iap->iad;
  iads = iap->iads;
  isp = iap->isp;
  isw = iap->isw;
  nuzr = iap->nuzr;
  itpst = iap->itpst;

  ds = rap->ds;

  rds = ds;
  dsold = rds;
  rap->dsold = dsold;
  if (isp < 0) {
    isp = -isp;
    iap->isp = isp;
  }
  sp1 = 0.;
  bp1 = 0.;
  rlp = 0.;
  if (nuzr > 0) {
    for (i = 0; i < nuzr; ++i) {
      uzr[i] = 0.;
    }
  }
  nitps = 0;
  iap->nit = nitps;
  ntot = 0;
  iap->ntot = ntot;
  istop = 0;
  iap->istop = istop;

  for (j = 0; j < (iap->ntst + 1); ++j) {
    for (i = 0; i < (iap->ndim * iap -> ncol); ++i) {
      ups[j + i * (iap->ntst + 1)] = 0.;
      uoldps[j + i * (iap->ntst + 1)] = 0.;
      upoldp[j + i * (iap->ntst + 1)] = 0.;
      dups[j + i * (iap->ntst + 1)] = 0.;
      udotps[j + i * (iap->ntst + 1)] = 0.;
      fa[j + i * (iap->ntst + 1)] = 0.;
    }
  }
  
  for(j = 0; j < iap->lib->parameter_count; ++j) {
    rldot[j] = 0.0;
    rlcur[j] = 0.0;
    rlold[j] = 0.0;
  }

  rsptbv(iap, rap, par, icp, funi, stpnt, &rds, rlcur.data(), 
	 rlold.data(), rldot.data(), &ntst_plus_one, ups.data(), uoldps.data(), udotps.data(), upoldp.data(), dups.data(), tm.data(), 
	 dtm.data(), ev.data(), &nodir, thl, thu);
  (*pvli)(iap, rap, icp, dtm.data(), &ntst_plus_one, ups.data(), &ndim, p0.data(), p1.data(), par);

  setrtn(iap, &ntst, &ntst_plus_one, ups.data(), par);

  if (nodir == 1 && isw > 0) {
    stdrbv(iap, rap, par, icp, funi, bcni, icni, rlcur.data(), 
	   rlold.data(), rldot.data(), iap->ntst+1, ups.data(), dups.data(), uoldps.data(), udotps.data(), upoldp.data(), 
	   fa.data(), fc.data(), dtm.data(), 0, p0.data(), p1.data(), thl, thu);
  } else if (irs != 0 && isw < 0) {
    stdrbv(iap, rap, par, icp, funi, bcni, icni, rlcur.data(), 
	   rlold.data(), rldot.data(), iap->ntst+1, ups.data(), dups.data(), uoldps.data(), udotps.data(), upoldp.data(), 
	   fa.data(), fc.data(), dtm.data(), 1, p0.data(), p1.data(), thl, thu);
  }

  /* Store plotting data for restart point : */

  sthd(iap, rap, par, icp, thl, thu);
  if (irs == 0) {
    itp = xpp::AutoPointCode{xpp::AutoPointKind::End, itpst};
  } else {
    itp = xpp::AutoPointCode{};
  }
  iap->itp = itp;
  istop = 0;
  iap->istop = istop;
  (*pvli)(iap, rap, icp, dtm.data(), &ntst_plus_one, ups.data(), &ndim, p0.data(), p1.data(), par);
  stplbv(iap, rap, par, icp, rldot.data(), &ntst_plus_one, ups.data(), udotps.data(),
	 tm.data(), dtm.data(), thl, thu);
  istop = iap->istop;
  if (istop == 1) {
    return 0;
  }

  extrbv(iap, rap, funi, &rds, rlcur.data(), rlold.data(), rldot.data(), &ntst_plus_one, ups.data(),
	 uoldps.data(), udotps.data());

  itp = xpp::AutoPointCode{};
  iap->itp = itp;
  goto L2;

 L1:
  itp = xpp::AutoPointCode{};
  iap->itp = itp;
  ntot = iap->ntot;

/* Adapt the mesh to the solution. */

  if (iad != 0) {
    if (ntot % iad == 0) {
      adapt(iap, rap, &ntst, &ncol, &ntst, &ncol, tm.data(), dtm.data(), &
	    ntst_plus_one, ups.data(), uoldps.data());
    }
  }

  /* Adapt the stepsize along the branch. */

  if (iads != 0) {
    if (ntot % iads == 0) {
      adptds(iap, rap, &rds);
    }
  }

  /* Provide initial approximation and determine next point. */

  contbv(iap, rap, par, icp, funi, &rds, rlcur.data(), rlold.data(), 
	 rldot.data(), &ntst_plus_one, ups.data(), uoldps.data(), udotps.data(), upoldp.data(), dtm.data(), thl, thu);
  
 L2:
  stepbv(iap, rap, par, icp, funi, bcni, icni, pvli, &rds, 
	 rlcur.data(), rlold.data(), rldot.data(), &ntst_plus_one, ups.data(), dups.data(), uoldps.data(), udotps.data(), upoldp.data(),
	 fa.data(), fc.data(), tm.data(), dtm.data(), p0.data(), p1.data(), thl, thu);
  istop = iap->istop;
  if (istop == 1) {
    goto L3;
  }

  {
    std::vector<LocationTest<decltype(&fnuzbv)>> tests;
    for (integer index = 0; index < nuzr; ++index)
      tests.push_back({fnuzbv, uzr[index], xpp::AutoPointKind::UserZero, index});
    if (ilp == 1) tests.push_back({fnlpbv, rlp, xpp::AutoPointKind::PeriodicFold, {}});
    if (isp >= 2) tests.push_back({fnbpbv, bp1, xpp::AutoPointKind::PeriodicBranch, {}});
    if ((isp == 1 || isp == 2) && xpp::problem_has_orbit_multipliers(ips))
      tests.push_back({fnspbv, sp1, xpp::AutoPointKind::Pending, {}});
    locate_tests(*iap, *rap, tests, marks,
      [&](const auto &test) {
        logical changed;
        return test.function(iap, rap, par, icp, &changed, funi, bcni, icni,
            p0.data(), p1.data(), ev.data(), rlcur.data(), rlold.data(), rldot.data(),
            &ntst_plus_one, ups.data(), uoldps.data(), udotps.data(), upoldp.data(),
            fa.data(), fc.data(), dups.data(), tm.data(), dtm.data(), thl, thu, marks);
      },
      [&](auto &test, double &dstest, bool &stepped) {
        lcspbv(iap, rap, par, icp, test.function, funi, bcni, icni, pvli,
            &test.value, rlcur.data(), rlold.data(), rldot.data(), &ntst_plus_one,
            ups.data(), dups.data(), uoldps.data(), udotps.data(), upoldp.data(),
            fa.data(), fc.data(), tm.data(), dtm.data(), p0.data(), p1.data(),
            ev.data(), thl, thu, marks, dstest, stepped);
      });
    if (iap->istop != 1 && iap->itp.kind == xpp::AutoPointKind::Pending)
      tpspbv(iap, rap, par, icp, ev.data());
  }

  /* Store plotting data. */

 L3:
  (*pvli)(iap, rap, icp, dtm.data(), &ntst_plus_one, ups.data(), &ndim, p0.data(), p1.data(), par);
  stplbv(iap, rap, par, icp, rldot.data(), &ntst_plus_one, ups.data(), udotps.data(),
	 tm.data(), dtm.data(), thl, thu);

  istop = iap->istop;
  if (istop == 0) {
    goto L1;
  } else {
    return 0;
  }

} /* cnrlbv_ */

/*     ---------- ------ */
/* Subroutine */ int 
contbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), doublereal *rds, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *dtm, doublereal *thl, doublereal *thu)
{
  /* System generated locals */
  integer ups_dim1, udotps_dim1, uoldps_dim1;

  /* Local variables */
  integer ndim, ncol, nfpr, nrow, ntst, i, j;
  doublereal dsold;

  doublereal dds;

  /* Determines an initial approximation to the next solution point, */
  /* by a computation of the null space of the Jacobian. */
  /* The stepsize used in the preceding step has been stored in DSOLD. */

  /* Parameter adjustments */
  /*--dtm;*/
  /*--rlcur;*/
  /*--rlold;*/
  /*--rldot;*/
  udotps_dim1 = *ndxloc;
  uoldps_dim1 = *ndxloc;
  ups_dim1 = *ndxloc;
  
  ndim = iap->ndim;
  ntst = iap->ntst;
  ncol = iap->ncol;
  nfpr = iap->nfpr;

  dsold = rap->dsold;

  /* Compute rate of change (along branch) of PAR(ICP(1)) and U : */

  dds = 1. / dsold;
  nrow = ndim * ncol;
  for (j = 0; j < ntst + 1; ++j) {
    for (i = 0; i < nrow; ++i) {
      ARRAY2D(udotps, j, i) = (ARRAY2D(ups, j, i) - ARRAY2D(uoldps, j, i)) * dds;
    }
  }
  for (i = 0; i < nfpr; ++i) {
    rldot[i] = (rlcur[i] - rlold[i]) * dds;
  }
  /*        Rescale, to set the norm of (UDOTPS,RLDOT) equal to 1. */
  scaleb(iap, icp, ndxloc, udotps, rldot, dtm, 
	 thl, thu);

  /* Extrapolate to get initial approximation to next solution point. */

  extrbv(iap, rap, funi, rds, rlcur, rlold, rldot, 
	 ndxloc, ups, uoldps, 
	 udotps);

  /* Store time-derivative. */

  stupbv(iap, rap, par, icp, funi, rlcur, rlold, rldot,
	 ndxloc, ups, uoldps, upoldp);

  return 0;
} /* contbv_ */

/*     ---------- ------ */
/* Subroutine */ int 
extrbv(iap_type *iap, rap_type *rap, FUNI_TYPE((*funi)), doublereal *rds, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *udotps)
{
  xpp::auto_stability_step_start(iap->lib->session->auto_state.stability,
      iap->ibr, iap->ntot + (iap->lib->locating ? 1 : 0));

  /* System generated locals */
  integer ups_dim1, udotps_dim1, uoldps_dim1;

  /* Local variables */
  integer ndim, ncol, nfpr, nrow, ntst, i, j;

  /* Determines an initial approximation to the next solution by */
  /* a computation of the null space of the Jacobian. */
  /* The stepsize used in the preceding step has been stored in DSOLD. */

  /* Parameter adjustments */
  /*--rlcur;*/
  /*--rlold;*/
  /*--rldot;*/
  udotps_dim1 = *ndxloc;
  uoldps_dim1 = *ndxloc;
  ups_dim1 = *ndxloc;
  
  ndim = iap->ndim;
  ntst = iap->ntst;
  ncol = iap->ncol;
  nfpr = iap->nfpr;

  nrow = ndim * ncol;
  for (i = 0; i < nfpr; ++i) {
    rlold[i] = rlcur[i];
    rlcur[i] += *rds * rldot[i];
  }
  for (j = 0; j < ntst + 1; ++j) {
    for (i = 0; i < nrow; ++i) {
      ARRAY2D(uoldps, j, i) = ARRAY2D(ups, j, i);
      ARRAY2D(ups, j, i) += *rds * ARRAY2D(udotps, j, i);
    }
  }

  return 0;
} /* extrbv_ */

/*     ---------- ------ */
/* Subroutine */ int 
stupbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *upoldp)
{
  /* System generated locals */
  integer ups_dim1, uoldps_dim1, upoldp_dim1;

  /* Local variables */
  integer ndim, ncol;
  integer nfpr, ntst;
  integer i, j, k;
  integer n1;
  xpp::AutoProblem ips;

  std::vector<doublereal> dfdp(xpp::size_product(iap->ndim, iap->lib->parameter_count));
  std::vector<doublereal> dfdu(xpp::size_product(iap->ndim, iap->ndim));
  std::vector<doublereal> uold(iap->ndim);
  std::vector<doublereal> f(iap->ndim);
  std::vector<doublereal> u(iap->ndim);

/* Stores U-prime (derivative with respect to T) in UPOLDP. */

/* Local */

  /* Parameter adjustments */
  /*--par;*/
  /*--icp;*/
  /*--rlcur;*/
  /*--rlold;*/
  /*--rldot;*/
  upoldp_dim1 = *ndxloc;
  uoldps_dim1 = *ndxloc;
  ups_dim1 = *ndxloc;
  
  ndim = iap->ndim;
  ips = iap->ips;
  ntst = iap->ntst;
  ncol = iap->ncol;
  nfpr = iap->nfpr;

  for (i = 0; i < nfpr; ++i) {
    par[icp[i]] = rlold[i];
  }

  for (j = 0; j < ntst + 1; ++j) {
    for (i = 0; i < ndim; ++i) {
      u[i] = ARRAY2D(uoldps, j, i);
      if (xpp::problem_is_parabolic_evolution(ips)) {
	uold[i] = ARRAY2D(uoldps, j, i) * 2 - ARRAY2D(ups, j, i);
      } else {
	uold[i] = ARRAY2D(uoldps, j, i);
      }
    }
    (*funi)(iap, rap, ndim, u.data(), uold.data(), icp, par, 0, f.data(), 
	    dfdu.data(), dfdp.data());
    for (i = 0; i < ndim; ++i) {
      ARRAY2D(upoldp, j, i) = f[i];
    }
  }

  for (k = 1; k <= ncol - 1; ++k) {
    n1 = k * ndim;
    for (j = 0; j < ntst; ++j) {
      for (i = 0; i < ndim; ++i) {
	u[i] = ARRAY2D(uoldps, j, (n1 + i));
	if (xpp::problem_is_parabolic_evolution(ips)) {
	  uold[i] = ARRAY2D(uoldps, j, (n1 + i)) * 2 
	    - ARRAY2D(ups, j, (n1 + i));
	} else {
	  uold[i] = ARRAY2D(uoldps, j, (n1 + i));
	}
      }
      (*funi)(iap, rap, ndim, u.data(), uold.data(), icp, par, 0, f.data(), dfdu.data(), dfdp.data());
      for (i = 0; i < ndim; ++i) {
	ARRAY2D(upoldp, j, (n1 + i)) = f[i];
      }
    }
  }

  for (i = 0; i < nfpr; ++i) {
    par[icp[i]] = rlcur[i];
  }

  return 0;
} /* stupbv_ */

/*     ---------- ------ */
/* Subroutine */ int 
stepbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), PVLI_TYPE_BVP((*pvli)), doublereal *rds, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *dups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *fa, doublereal *fc, doublereal *tm, doublereal *dtm, doublereal *p0, doublereal *p1, doublereal *thl, doublereal *thu)
{
  /* System generated locals */
  integer ups_dim1, uoldps_dim1, udotps_dim1, fa_dim1;

  /* Local variables */
  integer iads;
  doublereal adrl;
  logical done;
  integer ndim, ncol;
  doublereal epsl, rdrl;
  integer nfpr, ifst;
  doublereal epsu;
  integer ntop, itnw, nllv;
  doublereal dumx;
  integer ntot, nrow=0, nwtn, ntst, i, j;
  doublereal dsold, dsmin;
  integer nitps;
  doublereal rdumx;
  integer istop;
  bool nonfinite = false;
  doublereal au;

  doublereal delref=0.0, delmax;

  integer iid;
  doublereal adu;
  integer ibr, mxt;
  doublereal umx;
  integer nit1;

  /* Controls the solution of the nonlinear equations (by Newton's method) 
*/
/* for the next solution (PAR(ICP(*)) , U) on a branch of solutions. */

  /* Parameter adjustments */
  /*--par;*/
  /*--icp;*/
  /*--rlcur;*/
  /*--rlold;*/
  /*--rldot;*/
  /*--fc;*/
  /*--tm;*/
  /*--dtm;*/
  fa_dim1 = *ndxloc;
  udotps_dim1 = *ndxloc;
  uoldps_dim1 = *ndxloc;
  ups_dim1 = *ndxloc;
    
  ndim = iap->ndim;
  ntst = iap->ntst;
  ncol = iap->ncol;
  iads = iap->iads;
  iid = iap->iid;
  itnw = iap->itnw;
  nwtn = iap->nwtn;
  nfpr = iap->nfpr;
  ibr = iap->ibr;
  ntot = iap->ntot;
  ntop = ntot + 1;

  dsmin = rap->dsmin;
  epsl = rap->epsl;
  epsu = rap->epsu;

 L1:
  dsold = *rds;
  rap->dsold = dsold;
  nitps = 0;
  iap->nit = nitps;

  /* Write additional output on unit 9 if requested. */

  wrtbv9(iap, rap, par, icp, rlcur, ndxloc, 
	 ups, tm, dtm, thl, thu);

  /* Generate the Jacobian matrix and the right hand side. */

  for (nit1 = 1; nit1 <= itnw; ++nit1) {

    { int iflag; xpp::byeauto_(&iflag); } /* xppautX: cancel */
    if (!iap->lib->locating && xpp::job::cancelled()) { nrow = ndim * ncol; goto L13; } /* xppautX: cancel */
    nitps = nit1;
    iap->nit = nitps;
    nllv = 0;

    ifst = 0;
    if (nitps <= nwtn) {
      ifst = 1;
    }

    {
      SetubvStop stop(*iap->lib, !iap->lib->locating); /* xppautX: cancel */
      solvbv(&ifst, iap, rap, par, icp, funi, bcni, icni, 
	     rds, &nllv, rlcur, rlold, rldot, ndxloc, 
	     ups, dups, uoldps, 
	     udotps, upoldp, dtm, fa, fc, p0, 
	     p1, thl, thu);
    }
    if ((!iap->lib->locating && xpp::job::cancelled()) || jacobian_failed(iap)) { nrow = ndim * ncol; goto L13; } /* xppautX: cancel, W275 */
    /* Add Newton increments. */

    for (i = 0; i < ndim; ++i) {
      ARRAY2D(ups, ntst, i) += fc[i];
    }
    for (i = 0; i < nfpr; ++i) {
      rlcur[i] += fc[ndim + i];
      par[icp[i]] = rlcur[i];
    }

    dumx = 0.;
    umx = 0.;
    nrow = ndim * ncol;
    for (j = 0; j < ntst; ++j) {
      for (i = 0; i < nrow; ++i) {
	adu = f2c::abs(ARRAY2D(fa, j, i));
	if (adu > dumx) {
	  dumx = adu;
	}
	au = f2c::abs(ARRAY2D(ups, j, i));
	if (au > umx) {
	  umx = au;
	}
	ARRAY2D(ups, j, i) += ARRAY2D(fa, j, i);
      }
    }
    // Check the active mesh entries; endpoint padding is not solution data.
    for (i = 0; i < nrow; ++i) {
      const size_t count = static_cast<size_t>(ntst + (i < ndim ? 1 : 0));
      if (!xpp::all_finite({ups + i * ups_dim1, count})) {
        nonfinite = true;
        goto L3;
      }
    }
    if (!xpp::all_finite({par, static_cast<size_t>(iap->lib->parameter_count)})) {
      nonfinite = true;
      goto L3;
    }
    /* I am not sure why this is here, and it requires a wierd
       special case for the parsing of points which are "special"
       (i.e. bifurcations, folds, etc).  I may want to get rid
       of it at some time.
    */
    wrtbv9(iap, rap, par, icp, rlcur, ndxloc, ups, tm, dtm, thl, thu);

    /* Check whether user-supplied error tolerances have been met : */

    done = TRUE_;
    rdrl = 0.;
    for (i = 0; i < nfpr; ++i) {
      adrl = f2c::abs(fc[ndim + i]) / (f2c::abs(rlcur[i]) + 1.);
      if (adrl > epsl) {
	done = FALSE_;
      }
      if (adrl > rdrl) {
	rdrl = adrl;
      }
    }
    rdumx = dumx / (umx + 1.);
    if (done && rdumx < epsu) {
      (*pvli)(iap, rap, icp, dtm, ndxloc, ups, &ndim, p0, p1, par);
      if (!xpp::all_finite({par, static_cast<size_t>(iap->lib->parameter_count)})) {
        nonfinite = true;
        goto L3;
      }
      if (iid >= 2) {
	xpp::print(iap->lib->fp9," \n");	
      }
      return 0;
    }

    if (nitps == 1) {
      delref = f2c::max(rdrl,rdumx) * 20;
    } else {
      delmax = f2c::max(rdrl,rdumx);
      if (delmax > delref) {
	goto L3;
      }
    }

    /* L2: */
  }

  /* Maximum number of iterations reached. */

 L3:
  if (iads == 0) {
    xpp::print(iap->lib->fp9,"{:4} {:5} NOTE:No convergence with fixed step size\n",ibr,ntop);
    xpp::auto_stop_noconv(iap->lib->session->auto_state.stop, xpp::AUTO_STOP_NOCONV_FIXED, *rds, dsmin); /* xppautX: T23 */
    goto L13;
  }

  /* Reduce stepsize and try again. */

  mxt = itnw;
  iap->nit = mxt;
  adptds(iap, rap, rds);
  if (f2c::abs(*rds) < dsmin) {
    goto L12;
  }
  for (i = 0; i < nfpr; ++i) {
    rlcur[i] = rlold[i] + *rds * rldot[i];
  }
  for (j = 0; j < ntst + 1; ++j) {
    for (i = 0; i < nrow; ++i) {
      ARRAY2D(ups, j, i) = ARRAY2D(uoldps, j, i) + *rds * 
	ARRAY2D(udotps, j, i);
    }
  }
  if (iid >= 2) {
    xpp::print(iap->lib->fp9,"{:4} {:5} NOTE:Retrying step\n",ibr,ntop);

  }
  goto L1;

/* Minimum stepsize reached. */

 L12:
  xpp::print(iap->lib->fp9,"{:4} {:5}, NOTE:No convergence using minimum step size\n",ibr,ntop);
  xpp::auto_stop_noconv(iap->lib->session->auto_state.stop, xpp::AUTO_STOP_NOCONV_MIN, *rds, dsmin); /* xppautX: T23 */
 L13:
  for (i = 0; i < nfpr; ++i) {
    rlcur[i] = rlold[i];
    par[icp[i]] = rlcur[i];
  }
  for (j = 0; j < ntst + 1; ++j) {
    for (i = 0; i < nrow; ++i) {
      ARRAY2D(ups, j, i) = ARRAY2D(uoldps, j, i);
    }
  }
  const bool retained = xpp::auto_stability_step_failed(iap->lib->session->auto_state.stability,
      iap->ibr, iap->ntot + 1);
  istop = 1;
  iap->istop = istop;
  if (nonfinite && !stop_asked(iap))
    xpp::auto_nonfinite(*iap, iap->ntot + 1);

  if (!retained && xpp::problem_has_orbit_multipliers(iap->ips) &&
      iap->isp != 0 && !stop_asked(iap)) {
    // A failed location can restore an unstored solution. Rebuild its
    // circuit pencil at zero step, without applying Newton increments;
    // the trial's pencil (and flowkm's in-place transforms) is not its own.
    integer fresh = 1, nullity = 0;
    double zero_step = 0;
    solvbv(&fresh, iap, rap, par, icp, funi, bcni, icni, &zero_step, &nullity,
           rlcur, rlold, rldot, ndxloc, ups, dups, uoldps, udotps, upoldp,
           dtm, fa, fc, p0, p1, thl, thu);
    xpp::auto_stability_multipliers_restored(iap->lib->session->auto_state.stability, *iap, p0, p1);
  }
  return 0;
} /* stepbv_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*      Restart of Solution Branches ( Differential Equations ) */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/*     ---------- ------ */
/* Subroutine */ int 
rsptbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), STPNT_TYPE_BVP((*stpnt)), doublereal *rds, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *dups, doublereal *tm, doublereal *dtm, doublecomplex *ev, integer *nodir, doublereal *thl, doublereal *thu)
{
  /* System generated locals */
  integer ups_dim1, uoldps_dim1;

    /* Local variables */
  integer ndim, ncol, nfpr, ntst, i, j;

  integer ntsrs;

  integer ncolrs;

  integer ntst_fort8,ncol_fort8,junk;

  /* Restarts computation of a branch of solutions at point labelled IRS. */
  /* The output written on unit 8 by a previous run is now expected as */
  /* input on unit 3. The label IRS, where computation is to resume, must */
  /* be specified in the user-supplied subroutine INIT. */
  /* If IRS=0 then the starting point must be provided analytically in the 
*/
/* user-supplied subroutine STPNT. */

  /* Parameter adjustments */
  /*--par;*/
  /*--icp;*/
  /*--rlcur;*/
  /*--rlold;*/
  /*--rldot;*/
  /*--tm;*/
  /*--dtm;*/
  /*--ev;*/
  uoldps_dim1 = *ndxloc;
  ups_dim1 = *ndxloc;
    
  ndim = iap->ndim;
  ntst = iap->ntst;
  ncol = iap->ncol;
  nfpr = iap->nfpr;

  /* Get restart data : */

    /* First take a peak at the file to see if ntst and
       ncol are different then the values found in
       the parameter file fort.2.
    */
  if(iap->irs > 0) {
    findlb(iap, rap, iap->irs, &junk, &junk);
    xpp::TokenReader tr = xpp::TokenReader::attach(iap->lib->fp3);
    xpp::AutoSolutionHeader header;
    if (!xpp::read_auto_solution_header(tr, header, iap->lib->parameter_count)) return 1;
    ntst_fort8 = header.ntst;
    ncol_fort8 = header.ncol;
  } else {
    ntst_fort8 = iap->ntst;
    ncol_fort8 = iap->ncol;
  }

  {
    integer ntst_used,ncol_used;
    integer ndxloc_orig=*ndxloc;

    /* use the bigger of the size defined in fort.2 and the one defined in fort.8 */
    if(ntst_fort8 > ntst)
      ntst_used=ntst_fort8;
    else
      ntst_used=ntst;

    if(ncol_fort8 > ncol)
      ncol_used=ncol_fort8;
    else
      ncol_used=ncol;
      
    *ndxloc=(ntst_used + 1)*4;
    std::vector<doublereal> ups_new(xpp::size_product(*ndxloc, xpp::size_product(iap->ndim, ncol_used)));
    std::vector<doublereal> upoldp_new(xpp::size_product(*ndxloc, xpp::size_product(iap->ndim, ncol_used)));
    std::vector<doublereal> udotps_new(xpp::size_product(*ndxloc, xpp::size_product(iap->ndim, ncol_used)));
    std::vector<doublereal> tm_new(*ndxloc);
    std::vector<doublereal> dtm_new(*ndxloc);

    /*initialize arrays*/
    for(i=0;i<*ndxloc;i++) {
      dtm_new[i]=0.0;
      tm_new[i]=0.0;
      for(j=0;j<ndim*ncol_used;j++) {
	ups_new[i+j*(*ndxloc)] = 0.0;
	upoldp_new[i+j*(*ndxloc)] = 0.0;
	udotps_new[i+j*(*ndxloc)] = 0.0;
      }
    }
    (*stpnt)(iap, rap, par, icp, &ntsrs, &ncolrs, rlcur, 
	     rldot, ndxloc, ups_new.data(), udotps_new.data(), upoldp_new.data(), tm_new.data(), 
	     dtm_new.data(), nodir, thl, thu);
    /* Determine a suitable starting label and branch number. */
      
    newlab(iap, rap);
      
    for (j = 0; j < ntsrs; ++j) {
      dtm_new[j] = tm_new[j + 1] - tm_new[j];
    }
      
    /* Adapt mesh if necessary : */
      
    if (ntst != ntsrs || ncol != ncolrs) {
      adapt(iap, rap, &ntsrs, &ncolrs, &ntst, &ncol, tm_new.data(), dtm_new.data(), 
	    ndxloc, ups_new.data(), udotps_new.data());
    }
    /* Copy from the temporary large arrays into the normal arrays. */
    for(i=0;i<ntst+1;i++) {
      dtm[-1 + i+1] = dtm_new[i];
      tm[-1 + i+1] = tm_new[i];
      for(j=0;j<ndim*ncol;j++) {
	ups[i+j*(ntst+1)] = ups_new[i+j*(*ndxloc)];
	upoldp[i+j*(ntst+1)] = upoldp_new[i+j*(*ndxloc)];
	udotps[i+j*(ntst+1)] = udotps_new[i+j*(*ndxloc)];
      }
    }
    *ndxloc=ndxloc_orig;
  }

  /* Set UOLDPS, RLOLD. */

  for (i = 0; i < nfpr; ++i) {
    rlcur[i] = par[icp[i]];
    rlold[i] = rlcur[i];
  }

  for (i = 0; i < ndim * ncol; ++i) {
    for (j = 0; j < ntst + 1; ++j) {
      ARRAY2D(uoldps, j, i) = ARRAY2D(ups, j, i);
    }
  }

  /* Store U-prime (derivative with respect to time or space variable). */

  if (*nodir == -1) {
    /*        ** Restart from a Hopf bifurcation. */
    *nodir = 0;
  } else {
    /*        ** Restart from orbit. */
    stupbv(iap, rap, par, icp, funi, rlcur, rlold, rldot, ndxloc, ups, uoldps, 
	   upoldp);
  }

  return 0;
} /* rsptbv_ */

/*     ---------- ------ */
/* Subroutine */ int 
stpnbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *ntsrs, integer *ncolrs, doublereal *rlcur, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *upoldp, doublereal *tm, doublereal *dtm, integer *nodir, doublereal *thl, doublereal *thu)
{
  /* System generated locals */

    /* Local variables */
  integer ndim, nars;
  integer nfpr;

  integer i;

  std::vector<integer> icprs(iap->lib->parameter_count);

  integer nfprs, nskip1, nskip2;
  xpp::AutoPointCode itprs;

  integer ndimrd, ndimrs, isw;
  xpp::AutoProblem ips;

  /* This subroutine locates and retrieves the information required to */
  /* restart computation at the point with label IRS. */
  /* This information is expected on unit 3. */

/* Local */

  /* Parameter adjustments */
  /*--tm;*/
  /*--dtm;*/
  /*--par;*/
  /*--icp;*/
  /*--rlcur;*/
  /*--rldot;*/
    
  ndim = iap->ndim;
  ips = iap->ips;
  isw = iap->isw;
  nfpr = iap->nfpr;

  xpp::TokenReader tr;
  xpp::AutoSolutionHeader header;
  if (!read_auto_restart_header(iap, rap, tr, header, ntsrs, ncolrs)) return 1;
  itprs = header.itp;
  nfprs = header.nfpr;
  nars = header.nar;
  iap->lab = header.lab;

  ndimrs = nars - 1;
  nskip1 = (ndimrs + 1) / (auto_solution_columns + 1) - ndim / auto_solution_columns;
  nskip2 = (ndimrs + 1) / (auto_solution_columns + 2) - ndim / (auto_solution_columns + 1);
  if (ndim <= ndimrs) {
    ndimrd = ndim;
  } else {
    ndimrd = ndimrs;
  }

  if (!xpp::read_auto_solution_rows(tr, ndim, *ntsrs, *ncolrs, *ndxloc,
                                    0, ndimrd, ups, tm, true, nskip1, {})) return 1;

  for (i = 0; i < nfprs; ++i) {
    if (!tr.read(icprs[i])) return 1;
  }
  for (i = 0; i < nfprs; ++i) {
    if (!tr.read(rldot[i])) return 1;
  }

  /* Read U-dot (deriv. with respect to arclength along solution branch). */

  if (!xpp::read_auto_solution_rows(tr, ndim, *ntsrs, *ncolrs, *ndxloc,
                                    0, ndimrd, udotps, nullptr, true, nskip2, {})) return 1;

  /* Read the parameter values. */

  if (!xpp::read_auto_solution_parameters(tr, header, *iap->lib->session, par)) return 1;
  for (i = 0; i < nfpr; ++i) {
    rlcur[i] = par[icp[i]];
  }

  /* Special case : Preprocess restart data in case of homoclinic */
  /* continuation */

  if (ips == xpp::AutoProblem::Homoclinic) {
    preho(*iap->lib, ndxloc, ntsrs, &ndimrd, &ndim, ncolrs, ups, udotps, tm, par);

    /* Special case : Preprocess restart data in case of branch switching 
     */
    /* at a period doubling bifurcation. */

  } else if (ips == xpp::AutoProblem::Periodic && isw == -1 && itprs.is(xpp::AutoPointKind::PeriodDoubling)) {
    pdble(iap, rap, &ndim, ntsrs, ncolrs, ndxloc, ups, udotps, tm, par);
    return 0;
  }

  /* Take care of the case where the free parameters have been changed at */
  /* the restart point. */

  if (nfprs != nfpr) {
    *nodir = 1;
    return 0;
  }
  for (i = 0; i < nfpr; ++i) {
    if (icprs[i] != icp[i]) {
      *nodir = 1;
      return 0;
    }
  }

  return 0;
} /* stpnbv_ */

int auto_uniform_start(iap_type *iap, rap_type *rap, doublereal *par,
                       integer *icp, integer *ntsr, integer *ncolrs,
                       doublereal *rlcur, integer ndxloc, doublereal *ups,
                       doublereal *tm, integer *nodir,
                       const std::function<void(doublereal *, doublereal &)> &evaluate) {
  const integer ndim = iap->ndim, ntst = iap->ntst, ncol = iap->ncol, nfpr = iap->nfpr;
  std::vector<doublereal> u(ndim);
  msh(iap, rap, tm);
  const doublereal dt = 1. / (ntst * ncol);
  for (integer j = 0; j < ntst + 1; ++j) {
    const integer ncol1 = j == ntst ? 1 : ncol;
    for (integer i = 0; i < ncol1; ++i) {
      doublereal time = tm[j] + i * dt;
      const integer k1 = i * ndim, k2 = (i + 1) * ndim;
      evaluate(u.data(), time);
      for (integer k = k1; k < k2; ++k) ups[j + k * ndxloc] = u[k - k1];
    }
  }
  *ntsr = ntst;
  *ncolrs = ncol;
  iap->ibr = 1;
  iap->lab = 0;
  for (integer i = 0; i < nfpr; ++i) rlcur[i] = par[icp[i]];
  *nodir = 1;
  return 0;
}

void auto_hopf_mesh(const iap_type *iap, const rap_type *rap, integer ndxloc,
                    doublereal *ups, doublereal *udotps, doublereal *upoldp,
                    doublereal *tm, doublereal *dtm, doublereal *rldot,
                    const doublereal *u, const doublereal *rnllv, doublereal tpi) {
  const integer ndim = iap->ndim, ntst = iap->ntst, ncol = iap->ncol;
  msh(iap, rap, tm);
  const doublereal dt = 1. / ntst;
  for (integer j = 0; j < ntst + 1; ++j) {
    const doublereal time = tm[j];
    const doublereal sine = xpp::math::sin(tpi * time);
    const doublereal cosine = xpp::math::cos(tpi * time);
    for (integer k = 0; k < ndim; ++k) {
      udotps[j + k * ndxloc] = sine * rnllv[k] + cosine * rnllv[ndim + k];
      upoldp[j + k * ndxloc] = cosine * rnllv[k] - sine * rnllv[ndim + k];
      ups[j + k * ndxloc] = u[k];
    }
  }
  for (integer i = 0; i < ncol - 1; ++i) {
    for (integer j = 0; j < ntst; ++j) {
      const doublereal time = tm[j] + (i + 1) * (tm[j + 1] - tm[j]) / ncol;
      const doublereal sine = xpp::math::sin(tpi * time);
      const doublereal cosine = xpp::math::cos(tpi * time);
      for (integer k = 0; k < ndim; ++k) {
        const integer k1 = (i + 1) * ndim + k;
        udotps[j + k1 * ndxloc] = sine * rnllv[k] + cosine * rnllv[ndim + k];
        upoldp[j + k1 * ndxloc] = cosine * rnllv[k] - sine * rnllv[ndim + k];
        ups[j + k1 * ndxloc] = u[k];
      }
    }
  }
  rldot[0] = 0.;
  rldot[1] = 0.;
  for (integer i = 0; i < ntst; ++i) dtm[i] = dt;
}

/*     ---------- ------ */
/* Subroutine */ int
stpnub(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *ntsrs, integer *ncolrs, doublereal *rlcur, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *upoldp, doublereal *tm, doublereal *dtm, integer *nodir, doublereal *thl, doublereal *thu)
{
  return auto_uniform_start(iap, rap, par, icp, ntsrs, ncolrs, rlcur, *ndxloc, ups, tm, nodir,
      [&](doublereal *state, doublereal &time) { stpnt(*iap->lib->session, iap->ndim, time, state, par); });
} /* stpnub_ */

/*     ---------- ------ */
/* Subroutine */ int 
setrtn(iap_type *iap, integer *ntst, integer *ndxloc, doublereal *ups, doublereal *par)
{
  /* System generated locals */
  integer ups_dim1;

  /* Local variables */
  integer i;

  integer nbc;

  /* Initialization for rotations */

  /* Parameter adjustments */
  /*--par;*/
  ups_dim1 = *ndxloc;
  
  par[xpp::auto_unknown::rotation_period] = pi(2.0);
  nbc = iap->nbc;

  iap->lib->rotations.irtn = 0;
  for (i = 0; i < nbc; ++i) {
    doublereal tmp = (ARRAY2D(ups, *ntst, i) - ARRAY2D(ups, 0, i)) / pi(2.0);
    iap->lib->rotations.nrtn[i] = i_dnnt(&tmp);

    if (iap->lib->rotations.nrtn[i] != 0) {
      iap->lib->rotations.irtn = 1;
    }
  }

  return 0;
} /* setrtn_ */

/*     ---------- ------ */
/* Subroutine */ int 
stdrbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer ndxloc, doublereal *ups, doublereal *dups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *fa, doublereal *fc, doublereal *dtm, integer iperp, doublereal *p0, doublereal *p1, doublereal *thl, doublereal *thu)
{
  /* System generated locals */
  integer udotps_dim1, fa_dim1;

  /* Builtin Local */

    /* variables functions */
  integer ndim, ncol, nfpr, ifst, nllv;
  doublereal rdsz;
  integer nrow, ntst, i, j;

  integer iid;

/* Generates a direction vector (UDOTPS,RLDOT) that is needed to start */
/* the computation of a branch when no direction vector is given. */

/* Generate the Jacobian matrix with zero direction vector. */
/* (Then the last row of the Jacobian will be zero) */
/* in case the starting direction is to be determined. */

  /* Parameter adjustments */
  /*--par;*/
  /*--icp;*/
  /*--rlcur;*/
  /*--rlold;*/
  /*--rldot;*/
  /*--fc;*/
  /*--dtm;*/
  fa_dim1 = ndxloc;
  udotps_dim1 = ndxloc;
  
  ndim = iap->ndim;
  ntst = iap->ntst;
  ncol = iap->ncol;
  iid = iap->iid;
  nfpr = iap->nfpr;

  nrow = ndim * ncol;
  if (iperp == 0) {
    for (j = 0; j < ntst + 1; ++j) {
      for (i = 0; i < nrow; ++i) {
	ARRAY2D(udotps, j, i) = 0.;
      }
    }
    for (i = 0; i < nfpr; ++i) {
      rldot[i] = 0.;
    }
  }

  rdsz = 0.;
  nllv = 1;
  ifst = 1;
  solvbv(&ifst, iap, rap, par, icp, funi, bcni, icni, &rdsz, 
	 &nllv, rlcur, rlold, rldot, &ndxloc, ups, dups, uoldps, 
	 udotps, upoldp, dtm, fa, fc, 
	 p0, p1, thl, thu);

  /* Compute the starting direction. */

  for (i = 0; i < ndim; ++i) {
    ARRAY2D(udotps, ntst, i) = fc[i];
  }
  for (i = 0; i < nfpr; ++i) {
    rldot[i] = fc[ndim + i];
    par[icp[i]] = rlcur[i];
  }

  for (j = 0; j < ntst; ++j) {
    for (i = 0; i < nrow; ++i) {
      ARRAY2D(udotps, j, i) = ARRAY2D(fa, j, i);
    }
  }

  /* Scale the starting direction. */

  scaleb(iap, icp, &ndxloc, udotps, rldot, dtm, thl, thu);

  // AUTO-07p STDRBV: a constant first parameter carries only roundoff.
  // On cycle curves orient by the first significant free parameter instead.
  integer direction=0;
  if (iap->isw==2 && xpp::problem_has_orbit_multipliers(iap->ips)) {
    for (integer k=0;k<nfpr;++k)
      if (std::abs(rldot[k])/(1+std::abs(rlcur[k]))>rap->epsl) {
        direction=k;
        break;
      }
  }
  if (rldot[direction] < 0.) {
    for (i = 0; i < nfpr; ++i) {
      rldot[i] = -rldot[i];
    }
    for (j = 0; j < ntst; ++j) {
      for (i = 0; i < nrow; ++i) {
	ARRAY2D(udotps, j, i) = -ARRAY2D(udotps, j, i);
      }
    }
    for (i = 0; i < ndim; ++i) {
      ARRAY2D(udotps, ntst, i) = -ARRAY2D(udotps, ntst, i); 
    }
  }

  if (iid >= 2) {
    xpp::print(iap->lib->fp9,"Starting direction of the free parameter(s) :\n");	

    for (i = 0; i < nfpr; ++i) {
      xpp::print(iap->lib->fp9," PAR({:3}) :{:11.3E}\n",icp[i],rldot[i]);	
    }
  }

  return 0;
} /* stdrbv_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*  Detection and Location of Branch Points in Boundary Value Problems */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/*     ---------- ------ */
/* Subroutine */ int 
lcspbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FNCS_TYPE_BVP((*fncs)), FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), PVLI_TYPE_BVP((*pvli)), doublereal *q, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *dups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *fa, doublereal *fc, doublereal *tm, doublereal *dtm, doublereal *p0, doublereal *p1, doublecomplex *ev, doublereal *thl, doublereal *thu, std::span<const AutoMark> marks, double &dstest, bool &stepped)
{
  stepped = false;
  /* Local variables */
  logical chng;
  doublereal epss;
  integer ntop, itmx;
  doublereal rtmp, rrds;
  integer ntot;
  doublereal s, dsmax;
  integer istop;
  doublereal q0, q1, s0, s1;
  integer nitsp1;
  doublereal dq, ds;

  integer iid, ibr;
  doublereal rds;
  xpp::AutoPointCode itp;

  /* This subroutine uses the Secant method to accurately locate folds */
  /* branch points, and zero(es) of user parameter values. */
  /* Such points are located as points on a solution branch where the */
  /* EXTERNAL function FNCS changes sign. */
  /* It involves calling the basic solution subroutines CONTBV and STEP */
  /* with decreasing values of RDS (stepsize along branch). */
  /* The point is assumed to have been found with sufficient accuracy if */
  /* the ratio between RDS and the user supplied value of DS is less than */
  /* the user-supplied tolerance EPSS. */
  /* This subroutine is called from CNRLB, which controls the computation */
  /* of branches of solutions to general boundary value problems. */

  /* Parameter adjustments */
  /*--ev;*/
  /*--dtm;*/
  /*--tm;*/
  /*--rldot;*/
  /*--rlold;*/
  /*--rlcur;*/

  iid = iap->iid;
  itmx = iap->itmx;
  ibr = iap->ibr;
  ntot = iap->ntot;
  ntop = ntot + 1;

  ds = rap->ds;
  dsmax = rap->dsmax;
  epss = rap->epss;

  /* Check for zero. */

  q0 = *q;
  q1 = (*fncs)(iap, rap, par, icp, &chng, funi, bcni, icni, p0, p1, 
	       ev, rlcur, rlold, rldot, ndxloc, ups, uoldps, 
	       udotps, upoldp, fa, fc, dups, tm, dtm, thl, thu, marks)
    ;

  if (!location_sign_change(*iap, *rap, q0, q1, dstest, chng, fncs == fnuzbv)) {
    *q = q1;
    return 0;
  }

  const auto n = static_cast<std::size_t>(iap->ndim);
  const auto np = static_cast<std::size_t>(iap->lib->parameter_count);
  const auto nfpr = static_cast<std::size_t>(iap->nfpr);
  const auto nu = static_cast<std::size_t>(*ndxloc) * n * iap->ncol;
  const auto nc = static_cast<std::size_t>(iap->nbc + iap->nint + 1);
  const double saved_q = q1;
  const double saved_dstest = dstest;
  LocationState saved(*iap, *rap, {{par, np}, {rlcur, nfpr}, {rlold, nfpr},
      {rldot, nfpr}, {ups, nu}, {uoldps, nu}, {udotps, nu}, {upoldp, nu},
      {p0, n * n}, {p1, n * n}, {dups, nu}, {fa, nu}, {fc, nc}});
  const std::vector<doublecomplex> saved_ev(ev, ev + n);

  const bool singular = iap->isw == 2 && iap->itpst == xpp::AutoPointKind::Torus &&
      xpp::auto_torus_singular(par[xpp::auto_unknown::torus_angle]);

  /* Use the secant method for the first step: */

  s0 = 0.;
  s1 = dstest;
  nitsp1 = 0;
  dq = q0 - q1;
  rds = q1 / dq * (s1 - s0);
  rtmp = auto_hmach1;
 L1:
  rds = rtmp * rds;

  /* Return if tolerance has been met : */

  rrds = f2c::abs(rds) / (sqrt(f2c::abs(ds * dsmax)) + 1);
  if (rrds < epss) {
    itp = xpp::AutoPointCode{xpp::AutoPointKind::Pending};
    iap->itp = itp;
    *q = 0.;
    xpp::print(iap->lib->fp9,"==> Location of special point : Convergence.    Stepsize ={:11.3E}\n",rds);	

    return 0;
  }

  rds = location_step(*rap, rds, singular);

  /* If requested write additional output on unit 9 : */

  if (iid >= 2) {
    xpp::print(iap->lib->fp9," ==> Location of special point :  Iteration {:3}   Stepsize ={:11.3E}\n",nitsp1,rds);	
  }

  {
    Locating locating(*iap->lib); /* predictor and solve belong to the unstored point */
    contbv(iap, rap, par, icp, funi, &rds, rlcur, rlold, &
	 rldot[0], ndxloc, ups, uoldps, udotps, upoldp, dtm, thl, thu);
    stepbv(iap, rap, par, icp, funi, bcni, icni, pvli, &rds, &rlcur[-1 +
								   1], rlold, rldot, ndxloc, ups, dups, uoldps, udotps,
	   upoldp, fa, fc, tm, dtm, p0, p1, thl, thu);
  }
  istop = iap->istop;
  if (istop != 0) {
    // AUTO-07p LCSPBV bvp.f90:1393: a STEPBV that does not converge leaves the
    // search: the point is given up (NOTE: Possible special point) and the branch goes on.
    if (stop_asked(iap)) {
      *q = 0.;
      return 0;
    }
    iap->istop = 0;
    goto L2;
  }

  /* Check for zero. */

  *q = (*fncs)(iap, rap, par, icp, &chng, funi, bcni, icni, p0, p1, 
	       ev, rlcur, rlold, rldot, ndxloc, ups, uoldps, 
	       udotps, upoldp, fa, fc, dups, tm, dtm, thl, thu, marks)
    ;
  // W419: an iterate where the test does not apply is no zero: give the point up.
  if (!auto_test_applies(*q)) goto L2;

  ++nitsp1;
  if (nitsp1 <= itmx) {
    // AUTO-07p LCSPAE ae.f90:743 / LCSPBV bvp.f90:1414:
    // the corrector may have retried with a shorter step.
    stepped = true;
    s = s1 + rds;
    dstest = s;
    /*        Use Mueller's method with bracketing for subsequent steps */
    mueller(&q0, &q1, q, &s0, &s1, &s, &rds);
    goto L1;
  }

 L2:
  saved.restore(*iap, *rap);
  dstest = saved_dstest;
  stepped = false;
  std::copy(saved_ev.begin(), saved_ev.end(), ev);
  *q = saved_q;

  xpp::print(iap->lib->fp9,"{:4} {:5} NOTE:Possible special point\n",ibr,ntop);

  return 0;
} /* lcspbv_ */

/*     ------ --------- -------- ------ */
doublereal 
fnlpbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, logical *chng, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), doublereal *p0, doublereal *p1, doublecomplex *ev, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *fa, doublereal *fc, doublereal *dups, doublereal *tm, doublereal *dtm, doublereal *thl, doublereal *thu, std::span<const AutoMark> marks)
{
  /* System generated locals */
  integer udotps_dim1, fa_dim1;
  doublereal ret_val;

    /* Local variables */
  integer ndim, ncol, nfpr, ifst, nllv, ntop;
  doublereal rdsz;
  integer ntot, ntst, i, j;

  integer iid, ibr;

/* RETURNS A QUANTITY THAT CHANGES SIGN AT A LIMIT POINT (BVP) */

  fa_dim1 = *ndxloc;
  udotps_dim1 = *ndxloc;

  ndim = iap->ndim;
  ntst = iap->ntst;
  ncol = iap->ncol;
  iid = iap->iid;
  nfpr = iap->nfpr;
  ibr = iap->ibr;
  ntot = iap->ntot;
  ntop = ntot + 1;

  /* Find the direction vector. */

  nllv = -1;
  ifst = 0;
  rdsz = 0.;

  solvbv(&ifst, iap, rap, par, icp, funi, bcni, icni, &rdsz, &nllv,
	 rlcur, rlold, rldot, ndxloc, ups, dups, uoldps, udotps, 
	 upoldp, dtm, fa, fc, 
	 p0, p1, thl, thu);

  for (i = 0; i < ndim; ++i) {
    ARRAY2D(udotps, ntst, i) = fc[i];
  }

  for (i = 0; i < nfpr; ++i) {
    rldot[i] = fc[ndim + i];
  }

  for (j = 0; j < ntst; ++j) {
    for (i = 0; i < ndim*ncol; ++i) {
      ARRAY2D(udotps, j, i) = ARRAY2D(fa, j, i);
    }
  }

  /* Scale the direction vector. */

  scaleb(iap, icp, ndxloc, udotps, rldot, dtm, 
	 thl, thu);
  if (iid >= 2) {
    xpp::print(iap->lib->fp9,"{:4} {:5}        Fold Function {:14.6E}\n",f2c::abs(ibr),ntop,rldot[0]);
  }

  /* Set the quantity to be returned. */

  ret_val = rldot[0];
  *chng = TRUE_;
  return ret_val;
} /* fnlpbv_ */

/*     ------ --------- -------- ------ */
doublereal 
fnbpbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, logical *chng, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), doublereal *p0, doublereal *p1, doublecomplex *ev, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *fa, doublereal *fc, doublereal *dups, doublereal *tm, doublereal *dtm, doublereal *thl, doublereal *thu, std::span<const AutoMark> marks)
{
  /* System generated locals */
  doublereal ret_val;

  /* Local variables */
  integer ndim, ntop, ntot;
  doublereal f;
  integer i;
  doublereal u;

  integer iid;
  doublereal det;
  integer ibr;
  doublereal det0;

  std::vector<doublereal> pp(xpp::size_product(iap->ndim, iap->ndim));

  ndim = iap->ndim;
  iid = iap->iid;

/* Save the determinant of the reduced system. */

  det = rap->det;
  det0 = det;
  ibr = iap->ibr;
  ntot = iap->ntot;
  ntop = ntot + 1;

  /* Compute the determinant of P1. */

/* Computing 2nd power */
  for (i = 0; i < ndim * ndim; ++i) {
    pp[i] = p1[i];
  }
  xpp::ge(iap->lib->fp9, iap->iid, ndim, ndim, pp.data(), 0, 1, &u, 1, &f, &det);
  rap->det = det;

  /* Set the determinant of the normalized reduced system. */

  if (det != 0.) {
    ret_val = det0 / det;
    *chng = TRUE_;
    const double angle = par[xpp::auto_unknown::torus_angle];
    if (iap->isw == 2 && iap->itpst == xpp::AutoPointKind::Torus && xpp::auto_torus_near_one_to_one(angle)) {
      // W405 (#467): a cycle's torus equations hold the trivial multiplier's
      // block, rotation minus identity, whose determinant 4 sin^2(theta/2)
      // vanishes at the 1:1 resonance: divided out there, a branch point at
      // the resonance is a simple zero of the test and not a triple one.
      const double half = xpp::math::sin(angle / 2);
      if (half != 0) ret_val /= 4 * half * half;
    }
  } else {
    ret_val = auto_test_not_applicable;
    *chng = FALSE_;
  }

  if (iid >= 2) {
    xpp::print(iap->lib->fp9,"{:4} {:5}        BP   Function {:14.6E}\n",f2c::abs(ibr),ntop,ret_val);

  }
  return ret_val;
} /* fnbpbv_ */

/*     ------ --------- -------- ------ */
doublereal 
fnspbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, logical *chng, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), doublereal *p0, doublereal *p1, doublecomplex *ev, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *fa, doublereal *fc, doublereal *dups, doublereal *tm, doublereal *dtm, doublereal *thl, doublereal *thu, std::span<const AutoMark> marks)
{
  /* System generated locals */
  doublereal ret_val;

  /* Local variables */
  doublereal amin;
  integer ndim, nins, ntop, ntot;
  integer nins1=0;
  doublereal d;
  integer i, j;

  integer iid,ibr,loc=0,isp,isw;
  doublereal azm1;

  std::vector<doublereal> wrk(xpp::size_product(iap->ndim, iap->ndim));

  /* This function returns a quantity that changes sign when a complex */
  /* pair of eigenvalues of the linearized Poincare map moves in or out */
  /* of the unit circle or when a real eigenvalues passes through -1. */
  /* Local */

  /* Parameter adjustments */
  /*--ev;*/
  /*--p1;*/
  /*--p0;*/

  ndim = iap->ndim;
  isp = iap->isp;
  isw = iap->isw;
  iid = iap->iid;
  ibr = iap->ibr;
  ntot = iap->ntot;
  ntop = ntot + 1;

  /* Initialize: until a test below applies, none does. */

  ret_val = auto_test_not_applicable;
  d = ret_val;
  *chng = FALSE_;

/*  Compute the Floquet multipliers */

  if (isw==2) {
    xpp::auto_cycle_multipliers(*iap,p0,p1,ev,true);
    ndim=iap->ndm;
  } else xpp::flowkm(iap->lib->fp9, &ndim, p0, p1, &iid, wrk.data(), ev, true);
  /* Find the multiplier closest to z=1. */
  /* xppautX: the orbit's Floquet multipliers (auto_stability.h) */
  xpp::auto_stability_computed(iap->lib->session->auto_state.stability,ibr,ntot+1,{&ev[0].r,2*static_cast<size_t>(ndim)},xpp::AUTO_STABILITY_PERIODIC);
  amin = auto_rlarge;
  for (j = 0; j < ndim; ++j) {
    doublecomplex tmp;
    tmp.r = ev[j].r - 1., tmp.i = ev[j].i;
    azm1 = z_abs(&tmp);
    if (azm1 <= amin) {
      amin = azm1;
      loc = j;
    }
  }
  // Preserve conjugate adjacency when moving the neutral multiplier.
  if (loc != 0) std::rotate(ev, ev + loc, ev + loc + 1);

  integer first=1;
  if (isw==2 && (iap->itpst==xpp::AutoPointKind::PeriodicFold ||
                 iap->itpst==xpp::AutoPointKind::PeriodDoubling))
    first=static_cast<integer>(auto_cycle_place_imposed(iap->itpst,{ev,static_cast<std::size_t>(ndim)}));
  // W356 (#418): AUTO-07p periodic.f90:1960-1983, insertion by
  // conjugate blocks. Keep both computed values, including their order.
  for (i = first+1; i < ndim; ++i) {
    const auto value = ev[i];
    if (value.i < 0) continue;
    const integer width = value.i > 0 && i + 1 < ndim ? 2 : 1;
    const auto partner = width == 2 ? ev[i + 1] : value;
    const double distance = std::abs(z_abs(&ev[i]) - 1);
    integer position = i;
    for (;;) {
      if (position <= first) break;
      integer previous = position - 1;
      if (ev[previous].i < 0 && previous > first) --previous;
      if (std::abs(z_abs(&ev[previous]) - 1) <= distance) break;
      std::move_backward(ev + previous, ev + position, ev + position + width);
      position = previous;
    }
    ev[position] = value;
    if (width == 2) ev[position + 1] = partner;
  }

  /* Print error message if the Floquet multiplier at z=1 is inaccurate. */
  /* (ISP is set to negative and detection of bifurations is discontinued) 
   */

  {
    doublecomplex tmp;
    tmp.r = ev[0].r - 1., tmp.i = ev[0].i;
    amin = z_abs(&tmp);
  }
  if (amin > .05 && isp == 2) {
    if (iid >= 2) {
      xpp::print(iap->lib->fp9,"{:4} {:5} NOTE:Multiplier inaccurate\n",f2c::abs(ibr),ntop);

    }
    for (i = 0; i < ndim; ++i) {
      xpp::print(iap->lib->fp9,"{:4} {:5}        Multiplier {:3} {:14.6E} {:14.6E}\n",f2c::abs(ibr),ntop,i,ev[i].r,ev[i].i);
    }
    nins = 0;
    iap->nins = nins;
    xpp::print(iap->lib->fp9,"{:4} {:5}        Multipliers:   Stable: {:3}\n",f2c::abs(ibr),ntop,nins);
    isp = -isp;
    iap->isp = isp;
    return ret_val;
  }

  /* Restart automatic detection if the Floquet multiplier at z=1 is */
  /* sufficiently accurate again. */

  if (isp < 0) {
    if (amin < .01) {
      xpp::print(iap->lib->fp9,"{:4} {:5} NOTE:Multiplier accurate again\n",f2c::abs(ibr),ntop);
      isp = -isp;
      iap->isp = isp;
    } else {
      for (i = 0; i < ndim; ++i) {
	xpp::print(iap->lib->fp9,"{:4} {:5}        Multiplier {:3} {:14.6E} {:14.6E}\n",f2c::abs(ibr),ntop,i,ev[i].r,ev[i].i);
      }
      return ret_val;
    }
  }

  /* Count the number of Floquet multipliers inside the unit circle. */

  // AUTO-07p periodic.f90:2031: neutral multipliers have finite numerical error.
  constexpr double multiplier_stability_tolerance = 1e-5;
  nins1 = 1;
  for (i = 1; i < ndim; ++i)
    if (z_abs(&ev[i]) <= 1 + multiplier_stability_tolerance) ++nins1;

  // TPSPBV periodic.f90:2136-2140: no PD/TR in planar flows.
  if (iap->ndm <= 2) {
    d = auto_test_not_applicable;
    ret_val = d;
  } else {
    if (isp == 2) {
      if (d_imag(&ev[1]) == 0. && ev[1].r > 0.) {
	/*            *Ignore if second multiplier is real positive */
	d = auto_test_not_applicable;
      } else {
	d = z_abs(&ev[1]) - 1.;
      }
      if (isw == 2) {
        d=auto_cycle_test(*iap,*rap,par,{ev,static_cast<std::size_t>(ndim)},chng);
        ret_val=d;
      } else {
	ret_val = d;
      }
      nins = iap->nins;
      if (isw!=2 && nins1 != nins) {
	*chng = TRUE_;
      }
    }
  }

  nins = nins1;
  iap->nins = nins;
  if (iid >= 2 && (isp == 1 || isp == 2)) {
    xpp::print(iap->lib->fp9,"{:4} {:5}        SPB  Function {:14.6E}\n",f2c::abs(ibr),ntop,d);
  }

  /* Print the Floquet multipliers. */

  nins = iap->nins;
  xpp::print(iap->lib->fp9,"{:4} {:5}        Multipliers:   Stable: {:3}\n",f2c::abs(ibr),ntop,nins);

  for (i = 0; i < ndim; ++i) {
    xpp::print(iap->lib->fp9,"{:4} {:5}        Multiplier {:3} {:14.6E} {:14.6E}\n",f2c::abs(ibr),ntop,i,ev[i].r,ev[i].i);

  }

  return ret_val;
} /* fnspbv_ */

/*     ------ --------- -------- ------ */
doublereal 
fnuzbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, logical *chng, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), doublereal *p0, doublereal *p1, doublecomplex *ev, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *fa, doublereal *fc, doublereal *dups, doublereal *tm, doublereal *dtm, doublereal *thl, doublereal *thu, std::span<const AutoMark> marks)
{
  return user_zero(*iap, par, icp, chng, marks, boundary_user_zero_width);
} /* fnuzbv_ */

/*     ---------- ------ */
/* Subroutine */ int 
tpspbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublecomplex *ev)
{

    /* Local variables */
  doublereal amin;
  integer ndim;
  doublereal epss, d;
  integer i;
  xpp::AutoPointKind itpst;
  doublereal ad;
  integer loc, loc1;
  xpp::AutoPointCode itp;
  doublereal azm1;

  /* Determines type of secondary periodic bifurcation. */

  ndim = iap->ndim;
  if (iap->isw==2) {
    iap->itp={xpp::auto_cycle_resonance_kind(iap->itpst,par[xpp::auto_unknown::torus_angle]),iap->itpst};
    return 0;
  }

  epss = rap->epss;
  itpst = iap->itpst;

  /* Find the eigenvalue closest to z=1. */

  loc = 1;
  amin = auto_rlarge;
  for (i = 0; i < ndim; ++i) {
    doublecomplex tmp;
    tmp.r = ev[i].r - 1., tmp.i = ev[i].i;
    azm1 = z_abs(&tmp);
    if (azm1 <= amin) {
      amin = azm1;
      loc = i;
    }
  }

  /* Find the eigenvalue closest to the unit circle */
  /* (excluding the eigenvalue at z=1). */

  loc1 = 1;
  amin = auto_rlarge;
  for (i = 0; i < ndim; ++i) {
    if (i != loc) {
      d = z_abs(&ev[i]) - 1.;
      ad = f2c::abs(d);
      if (ad <= amin) {
	amin = ad;
	loc1 = i;
      }
    }
  }

  if (f2c::abs(d_imag(&ev[loc1])) > sqrt(epss)) {
    /*       ** torus bifurcation */
    itp = xpp::AutoPointCode{xpp::AutoPointKind::Torus, itpst};
    iap->itp = itp;
    par[xpp::auto_unknown::torus_angle] =f2c::abs(xpp::math::atan2(d_imag(&ev[loc1]),ev[loc1].r));

  } else { /* a real multiplier */
    if (ev[loc1].r < -.5) {
      /*       ** period doubling */
      itp = xpp::AutoPointCode{xpp::AutoPointKind::PeriodDoubling, itpst};
      iap->itp = itp;
    } else {
      /*       ** something else... */
      itp = xpp::AutoPointCode{};
      iap->itp = itp;
    }
  }

  return 0;
} /* tpspbv_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*                    Output (Boundary Value Problems) */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

/*     ---------- ------ */
/* Subroutine */ int 
stplbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *tm, doublereal *dtm, doublereal *thl, doublereal *thu)
{
    /* Local variables */
  integer labw, ibrs, nins, itmp, jtmp, ntot;
  int iflag=0;
  xpp::AutoStopAt stop_at = {0}; /* xppautX: T23 */
  integer i;
  std::vector<double> u_high(iap->ndim);
  std::vector<double> u_low(iap->ndim);
  std::vector<double> u_0(iap->ndim);
  std::vector<double> u_bar(iap->ndim);

  integer istop, n2, ntots;

  integer iab, lab;
  doublereal amp;

  std::array<doublereal, auto_diagram_columns> umx;

  /* Writes the bifurcation diagram on unit 7 (Differential Equations) */
  /* (Also controls the writing of complete solutions on unit 8). */
  /* Every line written contains, in order, the following: */

/*  IBR    : The label of the branch. */
/*  NTOT   : The index of the point on the branch. */
/*           (Points are numbered consecutively along a branch). */
/*           If IPS=2 or 3, then the sign of NTOT indicates stability : */
/*            - = stable , + = unstable, or unknown. */
/*  ITP    : An integer indicating the type of point : */

/*             4  (  )  :   Output point (Every NPR steps along branch). 
*/
/*            -4  (UZ)  :   Output point (Zero of user function). */
/*             5  (LP)  :   Fold (fold). */
/*             6  (BP)  :   Branch point. */
/*             7  (PD)  :   Period doubling bifurcation. */
/*             8  (TR)  :   Bifurcation to an invariant torus. */
/*             9  (EP)  :   End point of branch, normal termination. */
/*            -9  (MX)  :   End point of branch, abnormal termination. */

/*  LAB        : The label of a special point. */
/*  PAR(ICP(1)): The principal parameter. */
/*  A          : The L2-norm of the solution vector, or other measure of 
*/
/*               the solution (see the user-supplied parameter IPLT). */
/*  MAX U(*)   : The maxima of the first few solution components. */
/*  PAR(ICP(*)): Further free parameters (if any). */

/* Local */

  auto context = point_output_context(*iap, *rap);

  ntot = iap->ntot;
  ++ntot;
  iap->ntot = ntot;

/* ITP is set to 4 every NPR steps along a branch of solns and the entire 
*/
/* solution is written on unit 8. */

  if (context.npr != 0) {
    if (ntot % context.npr == 0 && context.itp.kind == xpp::AutoPointKind::Regular) {
      context.itp = xpp::AutoPointCode{xpp::AutoPointKind::Output, context.itpst};
    }
    iap->itp = context.itp;
  }

  /* Check whether limits of the bifurcation diagram have been reached : */

  iab = f2c::abs(context.iplt);
  if (iab == 0 || iab > context.ndm * 3) {
    amp = sqrt(rnrmsq(iap, &context.ndm, ndxloc, ups, dtm, thu));
  }
  if (context.iplt > 0 && iab <= context.ndm) {
    amp = rmxups(iap, ndxloc, &iab, ups);
  }
  if (context.iplt > context.ndm && iab <= (context.ndm * 2)) {
    amp = rintg(iap, ndxloc, iab - context.ndm, ups, dtm);
  }
  if (context.iplt > (context.ndm * 2) && iab <= context.ndm * 3) {
    integer tmp = iab - (context.ndm * 2);
    amp = rnrm2(iap, ndxloc, &tmp, ups, dtm);
  }
  if (context.iplt < 0 && iab <= context.ndm) {
    amp = rmnups(iap, ndxloc, &iab, ups);
  }

  /* here is another place for byeauto
   call with iflag  */
  xpp::byeauto_(&iflag);
  istop = iap->istop;
  if (istop == 0 && xpp::job::cancelled()) iflag = 0; /* xppautX: cancel: as in stplae */
  stop_at.user = iflag == 1 || istop == 1; /* xppautX: T23, unless MX or UZR below */
  if (istop == 1 && !xpp::job::cancelled()) { /* xppautX: cancel: EP, not MX */
    /*        ** Maximum number of iterations reached somewhere. */
    context.itp = xpp::AutoPointCode{xpp::AutoPointKind::MaxSteps, context.itpst};
    iap->itp = context.itp;
    stop_at.noconv = 1, stop_at.user = 0;
  } else if (istop == -1) {
    /*        ** UZR endpoint */
    context.itp = xpp::AutoPointCode{xpp::AutoPointKind::End, context.itpst};
    iap->itp = context.itp;
  } else {
    if (par[icp[0]] < context.rl0 || par[icp[0]] > context.rl1 || amp < context.a0 || amp > context.a1 ||
	ntot >= context.nmx || iflag==1 ) { /* more bye auto */
      istop = 1;
      iap->istop = istop;
      context.itp = xpp::AutoPointCode{xpp::AutoPointKind::End, context.itpst};
      iap->itp = context.itp;
    }
  }

  /* All special points receive label: */

  labw = 0;
  if (context.itp.kind != xpp::AutoPointKind::Regular) {
    lab = iap->lab;
    ++lab;
    iap->lab = lab;
    labw = lab;
  }

  /* Compute maxima of solution components. */

  n2 = context.ndm;
  if (n2 > auto_diagram_columns) {
    n2 = auto_diagram_columns;
  }
  for (i = 0; i < n2; ++i) {
    itmp = i + 1;
    umx[i] = rmxups(iap, ndxloc, &itmp, ups);
  }

  /* compute max, min, etc of solutions   This is correct!! */
  for (i=0;i<iap->lib->session->model().node;i++){
    itmp=i+1;
    u_high[i]=rmxups(iap,ndxloc,&itmp,ups);
    u_low[i]=rmnups(iap,ndxloc,&itmp,ups);
    u_0[i]=ups[i*(*ndxloc)];
    u_bar[i]=rintg(iap, ndxloc, itmp, ups, dtm);
  }
    
  /* Determine stability, and write output on units 7 and 8. */

  ibrs = context.ibr;
  ntots = ntot;
  if (context.ips == xpp::AutoProblem::Periodic && f2c::abs(context.isw) != 2) {
    ibrs = -context.ibr;
    nins = iap->nins;
    if (nins == context.ndim) {
      ntots = -ntot;
    }
  }
  jtmp = iap->lib->parameter_count;
  /* addbif max min  of variables & initial data 

  */
  xpp::addbif(iap,rap, ntots, ibrs, par,icp,labw,&amp, u_high, u_low, u_0, u_bar);  

  wrline(iap, rap, par, icp, &icp[jtmp], &ibrs, &ntots,
	 &labw, &amp, umx.data());
  if (iap->istop != 0) { /* xppautX: why the branch ended (auto_stop.h, T23) */
    stop_at.br = context.ibr, stop_at.pt = ntot, stop_at.ipar = icp[0], stop_at.par = par[icp[0]], stop_at.norm = amp;
    stop_at.rl0 = context.rl0, stop_at.rl1 = context.rl1, stop_at.a0 = context.a0, stop_at.a1 = context.a1, stop_at.nmx = context.nmx;
    stop_at.mark = iap->istop == -1;
    if (stop_at.mark) stop_at.user = 0;
    xpp::auto_stop_branch_end(*iap->lib->session, &stop_at);
  }

  /* Write plotting and restart data on unit 8. */

  if (context.itp.kind != xpp::AutoPointKind::Regular) {
    wrtbv8(iap, rap, par, icp, rldot, ndxloc, ups, 
	   udotps, tm, dtm);
  }

  return 0;
} /* stplbv_ */

/*     ---------- ------ */
/* Subroutine */ int 
wrtbv8(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *tm, doublereal *dtm)
{
  xpp::Session &s=*iap->lib->session;
  /* System generated locals */
  integer ups_dim1, udotps_dim1;

  /* Local variables */
  integer ndim, ncol, nfpr, ntpl, jtmp, ntot, ntst, i, j, k;
  doublereal t;
  integer k1, k2;
  doublereal rn;
  integer nrowpr, lab, ibr, nar, nrd, isw;
  xpp::AutoPointCode itp;

  FILE *const fp8=fort8(s.auto_lib);

  /* Writes plotting and restart data on unit 8, viz.: */
  /* (1) data identifying the corresponding point on unit 7, */
  /* (2) the complete solution, */
  /* (3) the direction of the branch. */

  /* Specifically the following is written: */

/*  IBR   : The index of the branch. */
/*  NTOT  : The index of the point. */
/*  ITP   : The type of point (see STPLBV above). */
/*  LAB   : The label of the point. */
/*  NFPR : The number of free parameters used in the computation. */
/*  ISW   : The value of ISW used in the computation. */
/*  NTPL  : The number of points in the time interval [0,1] for which */
/*          solution values are wriiten. */
/*  NAR   : The number of values written per point. */
/*          (NAR=NDIM+1, since T and U(i), i=1,..,NDIM are written). */
/*  NROWPR: The number of lines printed following the identifying line */
/*          and before the next data set or the end of the file. */
/*          (Used for quickly skipping a data set when searching). */
/*  NTST  : The number of time intervals used in the discretization. */
/*  NCOL  : The number of collocation points used. */
/*  iap->lib->parameter_count : The dimension of the par array (and the number of */
/*          number of values in the parameter block).*/

/*  Following the above described identifying line there are NTPL lines */
/* containing : */
/*     T , U-1(T) , U-2(T) , ... , U-NDIM(T), */
/* where NDIM is the dimension of the system of differential equations. */

/*  Following this is a line which lists the active parameters (from ICP) */

/* Following this is a line containing */
/*    RL-dot(i) , i=1,NFPR, */

/* and following this are NTPL lines each containing */
/*    U-dot-1(T), U-dot-2(T), ... , U-dot-NDIM(T). */

/* Finally the parameter values PAR(i) , i=1,iap->lib->parameter_count, are written. */

/*  Above, RL-dot(.) and U-dot(.) specify the direction of the branch. */

  udotps_dim1 = *ndxloc;
  ups_dim1 = *ndxloc;

  ndim = iap->ndim;
  ntst = iap->ntst;
  ncol = iap->ncol;
  isw = iap->isw;
  itp = iap->itp;
  nfpr = iap->nfpr;
  ibr = iap->ibr;
  ntot = iap->ntot;
  lab = iap->lab;

  /* Write information identifying the solution : */

  ntpl = ncol * ntst + 1;
  nar = ndim + 1;
  nrd = ndim / auto_solution_columns + 2 + (ndim - 1) / auto_solution_columns;
  jtmp = iap->lib->parameter_count;
  nrowpr = nrd * (ncol * ntst + 1) + (nfpr - 1) / auto_solution_columns + 1 + (jtmp - 1) / auto_solution_columns + 
    1 + (nfpr - 1) / auto_solution_parameter_indices_per_line + 1;

  xpp::print(fp8,"{:5}",ibr);
  xpp::print(fp8," {:4}",ntot);
  xpp::print(fp8," {:4}",xpp::auto_point_encode(itp));
  xpp::print(fp8," {:4}",lab);
  xpp::print(fp8," {:4}",nfpr);
  xpp::print(fp8," {:4}",isw);
  xpp::print(fp8," {:4}",ntpl);
  xpp::print(fp8," {:4}",nar);
  xpp::print(fp8," {:4}",nrowpr);
  xpp::print(fp8," {:4}",ntst);
  xpp::print(fp8," {:4}",ncol);
  xpp::print(fp8," {:4}\n",iap->lib->parameter_count);

/* Write the entire solution on unit 8 : */

  for (j = 0; j < ntst; ++j) {
    rn = 1. / ncol;
    for (i = 0; i < ncol; ++i) {
      k1 = i * ndim;
      k2 = (i + 1) * ndim;
      t = tm[j] + i * rn * dtm[j];
      xpp::print(fp8,"    {:19.10E}",t);
      for (k = k1; k < k2; ++k) {
	if((k+1-k1)%auto_solution_columns==0)
	  xpp::print(fp8,"\n    ");
	xpp::print(fp8,"{:19.10E}",ARRAY2D(ups, j, k));
      }
      xpp::print(fp8,"\n");	 

    }
  }
  xpp::print(fp8,"    {:19.10E}",tm[ntst]);
  for (i = 0; i < ndim; ++i) {
    if((i+1)%auto_solution_columns==0)
      xpp::print(fp8,"\n    ");
    xpp::print(fp8,"{:19.10E}",ARRAY2D(ups, ntst, i));
  }
  xpp::print(fp8,"\n");  

/* Write the free parameter indices: */
  for (i = 0; i < nfpr; ++i) {
    xpp::print(fp8,"{:5}",icp[i]);
  }
  xpp::print(fp8,"\n");  

/* Write the direction of the branch: */
  xpp::print(fp8,"    ");
  for (i = 0; i < nfpr; ++i) {
    if((i>0)&&((i)%auto_solution_columns==0))
      xpp::print(fp8,"\n    ");
    xpp::print(fp8,"{:19.10E}",rldot[i]);
  }
  xpp::print(fp8,"\n");  

  for (j = 0; j < ntst; ++j) {
    for (i = 0; i < ncol; ++i) {
      k1 = i * ndim;
      k2 = (i + 1)* ndim;

      xpp::print(fp8,"    ");
      for (k = k1; k < k2; ++k) {
	if((k!=k1)&&((k-k1)%auto_solution_columns==0))
	  xpp::print(fp8,"\n    ");
	xpp::print(fp8,"{:19.10E}",ARRAY2D(udotps, j, k));
      }
      xpp::print(fp8,"\n");
    }
  }
  xpp::print(fp8,"    ");

  for (k = 0; k < ndim; ++k) {
    if((k!=0)&&(k%auto_solution_columns==0))
      xpp::print(fp8,"\n    ");
    xpp::print(fp8,"{:19.10E}",ARRAY2D(udotps, ntst, k));
  }
  xpp::print(fp8,"\n");

/* Write the parameter values. */

  xpp::print(fp8,"    ");
  for (i = 0; i < iap->lib->parameter_count; ++i) {
    if((i>0)&&(i%auto_solution_columns==0))
      xpp::print(fp8,"\n    ");
    xpp::print(fp8,"{:19.10E}",par[i]);
  }
  xpp::print(fp8,"\n");
  fflush(fp8);
  return 0;
} /* wrtbv8_ */

/*     ---------- ------ */
/* Subroutine */ int 
wrtbv9(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *rlcur, integer *ndxloc, doublereal *ups, doublereal *tm, doublereal *dtm, doublereal *thl, doublereal *thu)
{
  xpp::Session &s=*iap->lib->session;
  
  /* System generated locals */
  integer ups_dim1;

  /* Local variables */
  integer ndim, ncol, iplt, mtot, ntot, ntst, i, j, k;
  doublereal t;
  integer k1, nitps, k2;
  doublereal rn;
  
  integer iab, iid, ibr, ndm;
  doublereal amp=0.0;
  
  /* Writes additional output on unit 9. */

  ups_dim1 = *ndxloc;

  ndim = iap->ndim;
  ntst = iap->ntst;
  ncol = iap->ncol;
  iplt = iap->iplt;
  iid = iap->iid;
  ndm = iap->ndm;
  ibr = iap->ibr;
  nitps = iap->nit;
  ntot = iap->ntot;
  
  iab = f2c::abs(iplt);
  if (iab == 0 || iab > ndim) {
    amp = sqrt(rnrmsq(iap, &ndm, ndxloc, ups, dtm, thu));
  }
  if (iplt > 0 && iab <= ndim) {
    amp = rmxups(iap, ndxloc, &iab, ups);
  }
  if (iplt < 0 && iab <= ndim) {
    amp = rmnups(iap, ndxloc, &iab, ups);
  }
  if (iid >= 2) {
    if (nitps == 0 || iid >= 3) {
      xpp::print(s.auto_lib.fp9,"========================================");
      xpp::print(s.auto_lib.fp9,"========================================\n");
      xpp::print(s.auto_lib.fp9,"  BR    PT  IT\n");	
      
    }
    mtot = ntot + 1;
    xpp::print(s.auto_lib.fp9,"{:4} {:5}{:4}    {:14.6E}{:14.6E}\n",ibr,mtot,nitps,rlcur[0],amp);
  }
  
  if (iid >= 5) {
    xpp::print(s.auto_lib.fp9," UPS :\n");	
    for (j = 0; j < ntst; ++j) {
      rn = 1. / ncol;
      for (i = 0; i < ncol; ++i) {
	t = tm[j] + i * rn * dtm[j];
	k1 = i * ndim;
	k2 = (i + 1) * ndim;
	xpp::print(s.auto_lib.fp9," {:14.6E}",t);	
	for (k = k1; k < k2; ++k) {
	  if((k+1-k1)%7==0)
	    xpp::print(s.auto_lib.fp9,"\n ");
	  xpp::print(s.auto_lib.fp9," {:14.6E}",ARRAY2D(ups, j, k));	
	}
	xpp::print(s.auto_lib.fp9,"\n");	
	
      }
    }
    xpp::print(s.auto_lib.fp9," {:14.6E}",tm[ntst]);	
    for (i = 0; i < ndim; ++i) {
      if((i+1)%7==0)
	xpp::print(s.auto_lib.fp9,"\n ");
      xpp::print(s.auto_lib.fp9," {:14.6E}",ARRAY2D(ups, ntst, i));	
    }
    xpp::print(s.auto_lib.fp9,"\n");	
  }
  return 0;
} /* wrtbv9_ */

/*     ---------- ------ */
/* Subroutine */ int 
pvlsae(iap_type *iap, rap_type *rap, doublereal *u, doublereal *par)
{

  integer ndm;

  ndm = iap->ndm;
  if (iap->itpst == xpp::AutoPointKind::Hopf && iap->ndim == 2 * ndm + 2) {
    // AUTO-07p equilibrium.f90:365-367 retains PAR(11) for kappa <= 0.
    const double kappa = u[2 * ndm];
    if (kappa > 0.) par[xpp::auto_unknown::period] = pi(2.) / sqrt(kappa);
  }
  if (iap->ips==xpp::AutoProblem::DiscreteMap && iap->itpst==xpp::AutoPointKind::Torus)
    par[xpp::auto_unknown::torus_angle]=u[2*ndm];
  pvls(ndm, u, par);

  return 0;
} /* pvlsae_ */

/*     ---------- ------ */
/* Subroutine */ int 
pvlsbv(iap_type *iap, rap_type *rap, integer *icp, doublereal *dtm, integer *ndxloc, doublereal *ups, integer *ndim, doublereal *p0, doublereal *p1, doublereal *par)
{
  integer ndm;

  ndm = iap->jac;
  pvls(ndm, ups, par);

  return 0;
} /* pvlsbv_ */


void allocate_global_memory(const iap_type iap) {
    /* assign: zeroed */
    iap.lib->scratch.dfu.assign(xpp::size_product(iap.ndim, iap.ndim), 0.);
    iap.lib->scratch.dfp.assign(xpp::size_product(iap.ndim, iap.lib->parameter_count), 0.);
    iap.lib->scratch.uu1.assign(iap.ndim, 0.);
    iap.lib->scratch.uu2.assign(iap.ndim, 0.);
    iap.lib->scratch.ff1.assign(iap.ndim, 0.);
    iap.lib->scratch.ff2.assign(iap.ndim, 0.);
    iap.lib->scratch.funi_u1.resize(iap.ndim);
    iap.lib->scratch.funi_u2.resize(iap.ndim);
    iap.lib->scratch.funi_f1.resize(iap.ndim);
    iap.lib->scratch.funi_f2.resize(iap.ndim);
    iap.lib->rotations.nrtn.assign(iap.nbc, 0);
}
