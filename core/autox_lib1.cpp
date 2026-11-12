#include "autox_unknowns.h"
#include "xpp_files.h"
#include <array>
#include <span>
#include <initializer_list>
#include <string>
#include <vector>
#include "xpp_mem.h"
#include "session.h"
#include "xpp_log.h"
#include "autox_evd.h"
#include "autox_stability.h"
#include "autox_stability_autox.h"
#include "xpp_ui.h" /* err_msg(), byeautox_() */
#include "autox_nox.h" /* autox_screen_col(), RestartLabel */
#include "xpp_job.h" /* xppautX: cancel */
#include "autox_stop.h" /* xppautX: why a branch ended (T23) */
#include "form_ode.h"
#include "model.h"
#include "xpp_math.h"

namespace {
struct AutoxPointOutputContext {
  integer ndim;
  xpp::AutoxProblem ips;
  integer isw, iplt, nmx, npr, ndm;
  xpp::AutoxPointCode itp;
  xpp::AutoxPointKind itpst;
  integer ibr;
  doublereal rl0, rl1, a0, a1;
};
AutoxPointOutputContext point_output_context(const iap_type &iap, const rap_type &rap) {
  return {iap.ndim, iap.ips, iap.isw, iap.iplt, iap.nmx, iap.npr,
          iap.ndm, iap.itp, iap.itpst, iap.ibr, rap.rl0, rap.rl1, rap.a0, rap.a1};
}

struct AutoxIntegralContext {
  integer ndim, ntst, ncol;
  std::span<const doublereal> weights;
};
AutoxIntegralContext integral_context(const iap_type &iap) {
  auto &weights = iap.run->scratch.integral_weights;
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
/* the exact Jacobian failed in this run (autox_pp.cpp): the derivatives of
   the solve in progress are not the model's, so its step is not taken */
bool jacobian_failed(const iap_type *iap) { return iap->run->jacobian_failure.has_value(); }

/* the user cancelled, or the exact Jacobian failed: what ends a run where
   anything else (a solve that does not converge) only gives a point up */
bool stop_asked(const iap_type *iap) { return autox_cancelled(*iap->run) || jacobian_failed(iap); }

/* x^n (n >= 1) by repeated squaring, in f2c's pow_di order (its bits) */
double power_by_squaring(double x, integer n)
{
  double power = 1;
  for (auto u = static_cast<std::size_t>(n);;) {
    if (u & 1) power *= x;
    if (u >>= 1) x *= x;
    else break;
  }
  return power;
}

/* the special point a test function locates, for a note */
const char *located_kind(FNCS_TYPE_AE((*fncs)))
{
  if (fncs == fnbpae) return "a branch point (BP)";
  if (fncs == fnlpae) return "a fold (LP)";
  if (fncs == fnhbae) return "a Hopf bifurcation (HB)";
  if (fncs == fnbtae) return "a Bogdanov-Takens point (BT)";
  if (fncs == fncpae) return "a cusp (CP)";
  if (fncs == fnghae) return "a generalized Hopf point (GH)";
  if (fncs == fnrnae) return "a resonance";
  if (fncs == fnuzae) return "a user zero (UZ)";
  return "a special point";
}
const char *located_kind(FNCS_TYPE_BVP((*fncs)), const iap_type &iap)
{
  if (fncs == fnlpbv) return "a fold (LP)";
  if (fncs == fnbpbv) return "a branch point (BP)";
  if (fncs == fnspbv) {
    // on a two-parameter curve of cycles the test is the resonance of the curve's own multiplier (W565)
    if (iap.isw == 2) switch (iap.itpst) {
      case xpp::AutoxPointKind::CycleFold: return "a 1:1 resonance (R1)";
      case xpp::AutoxPointKind::PeriodDoubling: return "a 1:2 resonance (R2)";
      case xpp::AutoxPointKind::NeimarkSacker: return "a strong resonance (R1, R2, R3, R4)";
      default: break;
    }
    return "a period doubling or torus bifurcation (PD, TR)";
  }
  if (fncs == fnuzbv) return "a user zero (UZ)";
  return "a special point";
}

/* lcspae/lcspbv gave a special point up: the branch goes on without it
   (AUTO-07p's NOTE: Possible special point); why is null when ITMX
   iterations did not reach it */
void special_point_lost(const iap_type &iap, integer ibr, integer ntot, const char *kind, const char *why)
{
  xpp::autox_stop_note(*iap.run,
                      xpp::format("Br {} Pt {}: {} between points {} and {} was not located ({}); it is missing from the diagram",
                                  f2c::abs(ibr), ntot, kind, ntot, ntot + 1,
                                  why ? std::string(why) : xpp::format("not in max_location_iterations = {} iterations", iap.itmx)));
}

/* autox_locating = 1 for a scope: cleared however it ends, a failed solve
   (xpp::AutoxFailed) included, so the next run can be cancelled again */
struct Locating {
  AutoxRun &run;
  explicit Locating(AutoxRun &l) : run(l) { run.locating = true; }
  ~Locating() { run.locating = false; }
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
  xpp::AutoxStability::Values stability;
  std::vector<double> codim_left;
  decltype(AutoxRun::solvbv) solvbv;
  decltype(AutoxRun::homcont) homcont;

  LocationState(const iap_type &iap, const rap_type &rap,
                std::initializer_list<std::span<double>> data)
    : dsold(rap.dsold), det(rap.det), tivp(rap.tivp),
      nit(iap.nit), nins(iap.nins), isp(iap.isp),
      stability(iap.run->stability.computed),
      codim_left(iap.run->codim.previous_left),
      solvbv(iap.run->solvbv), homcont(iap.run->homcont)
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
    iap.run->stability.computed = stability;
    iap.run->codim.previous_left = codim_left;
    iap.run->solvbv = solvbv;
    iap.run->homcont = homcont;
  }
};

// W356 (#418): AUTO-07p CNRLAE ae.f90:199-214 / CNRLBV
// bvp.f90:287-305. A later location invalidates earlier tests' samples.
template<class Function> struct LocationTest {
  Function function;
  AutoxTestSample &sample;
  xpp::AutoxPointKind kind;
  std::optional<integer> user_index;
  /* W604: set for the hidden test of a branch's return to its start (autox_ends.h): a crossing it locates is kept
     only when this says the states there are the start's */
  const std::function<bool()> *accept = nullptr;
};

/* The first `count` tests' values at the current point, kept as their samples for the next step. */
template<class Function, class Evaluate>
void sample_tests(iap_type &iap, std::vector<LocationTest<Function>> &tests, std::size_t count, Evaluate evaluate) {
  for (std::size_t i = 0; i < count; ++i) {
    auto &test = tests[i];
    if (test.user_index) iap.iuzr = *test.user_index;
    test.sample = {evaluate(test), std::nullopt};
  }
}

bool is_branch_point(xpp::AutoxPointKind kind) {
  return kind == xpp::AutoxPointKind::BranchPoint || kind == xpp::AutoxPointKind::CycleBranch;
}

/* W572: whether a branch's starting point has tests that mean something: not a special point, where they are
   zero (a sign change from noise would label it again), and not a two-parameter curve's, whose tests are the
   codimension-two ones. The label the run restarts from says what the point is (irs 0: initial data). */
bool start_is_tested(const iap_type &iap, xpp::AutoxPointKind label) {
  return iap.isw != 2 &&
         (iap.irs == 0 || label == xpp::AutoxPointKind::Regular || label == xpp::AutoxPointKind::Output ||
          label == xpp::AutoxPointKind::End || label == xpp::AutoxPointKind::MaxSteps);
}

/* W604: an orbit as the same-point rule sees it, stplbv's u_high then its u_low: each of the model's variables'
   maximum, then each one's minimum over the orbit. Phase-free, so a family that closes on itself in another phase
   matches its start */
std::vector<double> orbit_extremes(iap_type &iap, integer ndxloc, doublereal *ups) {
  const std::size_t node = static_cast<std::size_t>(iap.run->session->model().node);
  std::vector<double> extremes(2 * node);
  for (std::size_t k = 0; k < node; ++k) {
    integer variable = static_cast<integer>(k) + 1;
    extremes[k] = rmxups(&iap, &ndxloc, &variable, ups);
    extremes[node + k] = rmnups(&iap, &ndxloc, &variable, ups);
  }
  return extremes;
}

/* W604: a branch that came back to its start ends there (autox_ends.h). The point is labelled branch_closed, and the
   stop says closed whichever limit it also reached. A point the same step located there is the start itself, not a
   new one (W611: a cycle branch from a Hopf point returns to it with the fold test's zero at the turn of its
   parameter there, where a cycle and its phase shifts are one family: no fold of cycles, docs/auto-references.md),
   so the label gives way to the closure's; only a point the user asked for (UZ) keeps it */
void end_closed_branch(iap_type &iap, xpp::AutoxPointCode &itp, xpp::AutoxStopAt &stop_at) {
  if (!iap.run->stop.closure_gap) return;
  iap.istop = 1;
  if (itp.kind != xpp::AutoxPointKind::UserZero)
    itp = xpp::AutoxPointCode{xpp::AutoxPointKind::BranchClosed, xpp::AutoxPointKind::Regular};
  iap.itp = itp;
  stop_at.closed = true, stop_at.user = 0;
}

template<class Function, class Evaluate, class Locate>
void locate_tests(iap_type &iap, rap_type &rap,
                  std::vector<LocationTest<Function>> &tests, std::span<const AutoxMark> marks,
                  Evaluate evaluate, Locate locate) {
  double dstest = rap.dsold;
  std::size_t last_stepped = 0;
  bool resonance_after_branch_point = false;
  std::optional<std::size_t> found;
  struct Bracket { AutoxTestSample start; double end; }; // a test's sample at the step's start, its value after its location
  std::vector<Bracket> brackets;
  for (std::size_t i = 0; i < tests.size(); ++i) {
    auto &test = tests[i];
    if (test.user_index) iap.iuzr = *test.user_index;
    if (test.accept && iap.itp.kind != xpp::AutoxPointKind::Regular) {
      /* another test located a point in this step: the branch closes there if that point is the start (the loop
         closes where a special point is, and the parameter touches the start's value there without a sign change);
         the hidden test never moves the point (W604) */
      test.sample = {evaluate(test), std::nullopt};
      (*test.accept)();
      continue;
    }
    bool stepped = false;
    const bool deferred = test.sample.after_branch_point;
    const AutoxTestSample start = test.sample;
    locate(test, dstest, stepped);
    brackets.push_back({start, test.sample.value});
    if (stepped) {
      last_stepped = i;
      resonance_after_branch_point = deferred;
    }
    if (iap.istop == 1) break;
    if (iap.itp.kind == xpp::AutoxPointKind::Pending) {
      iap.itp = {test.kind, iap.itpst};
      found = i;
    }
  }
  sample_tests(iap, tests, last_stepped, evaluate);
  // W585, W586 (#650, #651): on a torus curve the branch point and the 1:1 or 1:2 resonance are one point (the
  // curve's equations are singular there, W419), labelled once as each, whichever is located first and whichever
  // side of the zero it lands. A resonance located after its branch point (deferred_resonance): the branch-point
  // test's crossing near it is that branch point, already labelled. A resonance located in the step where the
  // branch point was given up, landing past it (henon_rot_th's converged run, 2e-8 past th = pi): the branch point's
  // zero is within the tolerance of this point, so its sample is the test here with the sign of the step's start,
  // and the next step takes it here (locate_special_point's at_start); a search from the step's start, as
  // deferred_resonance's for a resonance behind, does not converge on the test's cubic zero in ITMX steps.
  if (resonance_after_branch_point)
    for (std::size_t i = 0; i < last_stepped; ++i)
      if (is_branch_point(tests[i].kind)) tests[i].sample = {};
  if (!resonance_after_branch_point && iap.istop != 1 && found && iap.itpst == xpp::AutoxPointKind::NeimarkSacker &&
      (iap.itp.kind == xpp::AutoxPointKind::Resonance1 || iap.itp.kind == xpp::AutoxPointKind::Resonance2))
    for (std::size_t i = 0; i < *found; ++i) {
      const auto &[start, end] = brackets[i];
      auto &sample = tests[i].sample;
      if (is_branch_point(tests[i].kind) && start.value * end < 0 && sample.value * start.value <= 0)
        sample = {std::copysign(std::max(std::abs(sample.value), std::numeric_limits<double>::denorm_min()), start.value), std::nullopt};
    }
  if (iap.istop != 1 && found && tests[*found].user_index &&
      marks[*tests[*found].user_index].parameter < 0) iap.istop = -1;
}

// W458 (#521): the steps of a branch started at a Hopf point whose points'
// test functions and multipliers read the converged point's Jacobian, as an
// equilibrium's do (W421), and not the last Newton iterate's, autoX's choice
// for cycles. The start is where the constant solutions cross the cycles'
// equations, and the tangent, the fold's test, is ill-conditioned near it;
// the second point's corrector starts from the eigen-cycle, and its one
// correction moves the parameter further than the branch has moved
// (motoneuron6c: 1e-6 against 1.5e-7), so its fold test had the wrong sign
// and an LP that is not there was labelled. A test's first comparison is
// the second step (each starts at 0, AUTO-07p's TEST): its two points and
// every point of a location between them read their own Jacobian, so a
// fold there is located where it is (the Bautin normal form's, to 1e-9).
constexpr integer hopf_start_steps = 2;

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
  if (!autox_test_applies(q0) || !autox_test_applies(q1)) return false;
  if (iap.itp.kind != xpp::AutoxPointKind::Regular &&
      std::abs(autox_hmach1 * q1 * dstest) < location_tolerance(rap) * std::abs(q0 - q1)) {
    if (iap.itp.kind == xpp::AutoxPointKind::UserZero && enabled && !user_zero)
      iap.itp = {xpp::AutoxPointKind::Pending};
    q1 = 0;
  }
  return !((q0 >= 0 && q1 >= 0) || (q0 <= 0 && q1 <= 0) || !enabled);
}

// W585 (#650): a torus curve's own equations have a branch point where its
// resonance test is zero at 1:1 and 1:2 (W405, W419: the fold or flip curve of
// cycles crosses there), so a step that locates that BP first meets the
// resonance's zero within the tolerance of it, and AUTO-07p's rule above keeps
// the BP's label alone (hill_cycles_torus: the R2 at d = 0.2770059707517 was
// lost). The point is both: the resonance is the curve's codimension-two point
// and the BP its formulation's. The zero is not dropped but located on the
// next step as a point of its own: where location_sign_change took it as
// coincident with the BP (it set the value measured at the BP to 0), the test
// keeps a value on the side of the zero it was measured on.
bool deferred_resonance(const iap_type &iap, double q1_measured, double q1) {
  return is_branch_point(iap.itp.kind) && q1 == 0 && q1_measured != 0 && autox_test_applies(q1_measured);
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

/* the multipliers of a cycle, restored with the rest of a failed location's state */
struct LocationStateWithMultipliers {
  LocationState state;
  doublecomplex *target;
  std::vector<doublecomplex> ev;
  LocationStateWithMultipliers(LocationState s, doublecomplex *e, std::size_t n)
    : state(std::move(s)), target(e), ev(e, e + n) {}
  void restore(iap_type &iap, rap_type &rap) const {
    state.restore(iap, rap);
    std::copy(ev.begin(), ev.end(), target);
  }
};

/* lcspae/lcspbv met a bracket of a test (a sign change between two samples)
   that a smooth crossing does not explain (W545): the branch goes on without
   the point, and says so once. */
void special_point_rejected(const iap_type &iap, integer ibr, integer ntot, const char *kind, double parameter,
                            const std::string &why)
{
  xpp::autox_stop_note(*iap.run,
                      xpp::format("Br {} Pt {}: {} between points {} and {} was not accepted at parameter {:.10g}: {}; it is missing from the diagram",
                                  f2c::abs(ibr), ntot, kind, ntot, ntot + 1, parameter, why));
}

/* W545 (#610): the search LCSPAE (equilibria) and LCSPBV (cycles) share.
   AUTO-07p's secant and Mueller iteration, with its tolerance, stored values
   and early exits, as W356 translated it, and two additions, each for a
   bracket that is new to it:
   - a located zero leaves the test at the discarded end of its step as the
     test's value instead of 0, so the next step's sign change reports a second
     zero ahead of it (AUTO-07p's LCSPAE ae.f90:759 and LCSPBV bvp.f90:1425
     store 0 and miss it: docs/auto07p-findings.md 11);
   - a search that ends at ITMX bisects the bracket the iteration holds, down
     to the tolerance, where AUTO-07p gives the point up (finding 11).
   Both are accepted only as a crossing of a smooth test (autox_crossing_smooth,
   after the search). A cycle's branch-point test, a ratio of consecutive
   determinants, brackets a zero only while the values at the ends of the step
   are of a size a crossing within it gives (autox_ratio_in_scale, before the
   search), for every bracket, AUTO-07p's own included (W547). A zero refused
   is said once and the state restored, exactly as for a point given up.
   A torus curve's resonance test whose zero coincides with a branch point the step
   located is deferred to the next step, not dropped (W585, deferred_resonance).
   evaluate(chng) is the test at the current point, advance(rds) the predictor
   and corrector for a step of rds (rds may come back shorter, a corrector's
   retry), snapshot() the state a failed search restores, torus_singular()
   whether a torus curve's equations are singular there (location_step).
   accept (W604, the hidden test of a branch's return to its start, whose crossings are no special points): called
   at the located zero, it says whether to keep it; one it refuses, and one the search fails to find, leave the
   state as it was and no note, as though the step had no crossing. */
template<class Evaluate, class Advance, class Snapshot, class TorusSingular>
void locate_special_point(iap_type &iap, rap_type &rap, AutoxTestSample &sample, const char *kind,
                          bool user_zero, bool ratio_test, bool torus_resonance, const double &parameter,
                          double &dstest, bool &stepped, const std::function<bool()> *accept,
                          Evaluate evaluate, Advance advance, Snapshot snapshot, TorusSingular torus_singular)
{
  stepped = false;
  const integer iid = iap.iid;
  const integer itmx = iap.itmx;
  const integer ibr = iap.ibr;
  const double ds = rap.ds;
  const double dsmax = rap.dsmax;
  const double epss = rap.epss;

  /* Check whether the test has changed sign. */

  double q0 = sample.value;
  const std::optional<double> discarded = std::exchange(sample.offset, std::nullopt);
  const bool coincident = std::exchange(sample.after_branch_point, false);
  double &q = sample.value;
  bool chng;
  double q1 = evaluate(chng);
  const integer ntot = iap.ntot;
  const double q1_measured = q1;
  if (!location_sign_change(iap, rap, q0, q1, dstest, chng, user_zero)) {
    q = q1;
    if (torus_resonance && deferred_resonance(iap, q1_measured, q1)) {
      sample.after_branch_point = true;
      if ((q0 < 0) == (q1_measured < 0)) {
        q = q1_measured; // the zero is ahead: the next step brackets it from here
      } else {
        // the zero is behind: the next step brackets it from where q0 was measured, behind this point
        q = q0;
        sample.offset = discarded.value_or(0.) - dstest;
      }
    }
    return;
  }

  const double q_start = q0;
  const double q_end = q1;
  const double saved_dstest = dstest;
  const double s_start = discarded.value_or(0.);
  const double step_length = std::abs(saved_dstest - s_start);
  if (ratio_test && !autox_ratio_in_scale(q_start, q_end, step_length, location_tolerance(rap))) {
    q = q_end;
    special_point_rejected(iap, ibr, ntot, kind, parameter,
                           xpp::format("its test is a ratio of consecutive determinants, and its values {:.3g} and {:.3g} at the ends of the step are beyond what a crossing within it gives",
                                       q_start, q_end));
    return;
  }

  auto saved = snapshot();
  const bool singular = torus_singular();
  const char *why = nullptr; /* why the point was given up; null: ITMX */
  integer iterations = 0;
  double rds, s;

  /* One trial point: false when the search must go on without the point (why says so), and
     cancelled when a stop was asked for. */
  bool cancelled = false;
  const auto trial = [&] {
    if (iid >= 2 && !accept) {
      xpp::print(iap.run->fp9," ==> Location of special point :  Iteration {:3}   Stepsize ={:11.3E}\n",iterations,rds);
    }
    {
      Locating locating(*iap.run); /* predictor and solve belong to the unstored point */
      advance(rds);
    }
    if (iap.istop != 0) {
      // AUTO-07p LCSPAE ae.f90:738 / LCSPBV bvp.f90:1393: a solve that does not converge leaves the
      // search: the point is given up (NOTE: Possible special point) and the branch goes on.
      if (stop_asked(&iap)) {
        cancelled = true;
        return false;
      }
      iap.istop = 0;
      why = "the corrector did not converge";
      return false;
    }
    q = evaluate(chng);
    // W419: an iterate where the test does not apply is no zero: give the point up.
    if (!autox_test_applies(q)) {
      why = "its test function does not apply there";
      return false;
    }
    return true;
  };

  /* Use the secant method for the first step: */

  double s0 = s_start, s1 = saved_dstest;
  double dq = q0 - q1;
  rds = q1 / dq * (s1 - s0);
  const double rtmp = autox_hmach1;
  bool converged = false;
  for (;;) {
    rds = rtmp * rds;

    /* Return if relative tolerance has been met : */

    const double rrds = f2c::abs(rds) / (sqrt(f2c::abs(ds * dsmax)) + 1);
    if (rrds < epss) {
      converged = true;
      break;
    }

    // W580: a singular zero within half a tolerance of the step's start, a point already solved, is taken
    // there, not half a tolerance past it (henon_rot_th's BP next to the R2 located at th = pi: 3.7e-5 off)
    const bool at_start = singular && !discarded && std::abs(s1 + rds - s_start) < location_tolerance(rap) / 2;
    rds = at_start ? s_start - s1 : location_step(rap, rds, singular);
    if (!trial()) break;
    ++iterations;
    // AUTO-07p LCSPAE ae.f90:743 / LCSPBV bvp.f90:1414:
    // the corrector may have retried with a shorter step.
    s = s1 + rds;
    const bool more = iterations <= itmx;
    if (more) {
      stepped = true;
      dstest = s;
    }
    if (at_start && s == s_start) {
      s1 = s;
      converged = true;
      break;
    }
    /*        Use Mueller's method with bracketing for subsequent steps */
    mueller(&q0, &q1, &q, &s0, &s1, &s, &rds);
    if (!more) break;
  }
  std::string refused;
  bool bisected = false;
  const auto lo = std::min(s_start, saved_dstest), hi = std::max(s_start, saved_dstest);
  // W580: not where a torus curve's equations are singular: the halving has no aim short of the zero
  // (location_step's), and a trial on it leaves the next step no tangent (henon_rot_th's converged run
  // located its branch point 4.4e-8 past th = pi, and the curve went back the way it came)
  if (!cancelled && !converged && !why && !singular && std::min(s0, s1) >= lo && std::max(s0, s1) <= hi) {
    // AUTO-07p gives the point up here. The bracket (s0, s1) holds opposite signs and the state
    // is at s1: halve it down to the tolerance. Whether it ends on a zero is the rule's to say.
    bisected = true;
    stepped = true; // the state moves from here
    const double tolerance = location_tolerance(rap);
    while (std::abs(s1 - s0) >= tolerance) {
      s = (s0 + s1) / 2;
      if (s == s0 || s == s1) break;
      rds = s - s1;
      if (!trial()) break;
      if ((q < 0.) != (q1 < 0.)) { // a value of exactly 0 is on the non-negative side
        q0 = q1;
        s0 = s1;
      }
      q1 = q;
      s1 = s;
      dstest = s;
    }
    converged = !why && !cancelled;
  }
  // W588 (#653): a zero the coincidence rule put at the stored point (deferred_resonance), which the search cannot
  // reach where the torus equations are singular, is taken at that point, as at_start takes a singular zero at a
  // solved point (W580): forced_pp_ns' converged run, whose corrector failed 1.5e-8 short of the R2 beside its BP
  bool at_stored_point = false;
  if (coincident && singular && why && !cancelled) {
    saved.restore(iap, rap);
    s1 = saved_dstest;
    rds = -s1;
    why = nullptr;
    if (trial()) {
      s = s0 = s1 = s1 + rds;
      dstest = s;
      stepped = converged = at_stored_point = true;
    }
  }
  if (cancelled) {
    q = 0.;
    return;
  }

  // a zero new to AUTO-07p is accepted as a crossing of a smooth test, or of one whose zero is of odd order
  // (W580), or not at all (W545); a first secant step that lands within the tolerance of the end measures no
  // slope, so it shows no crossing
  if (converged && (discarded || bisected) && !at_stored_point) {
    const double width = std::abs(s1 - s0);
    const double ratio = autox_crossing_slope_ratio(q_end - q_start, step_length, q1 - q0, width);
    if (iterations == 0) {
      converged = false;
      refused = "its test is within the tolerance of zero at the end of the step, with no search to show it crosses there";
    } else if (!(ratio <= autox_crossing_slope_ratio_max)) {
      converged = false;
      refused = xpp::format("its test is not a smooth crossing: across the last {:.3g} of the search its slope is {:.3g} times its slope across the step, more than {} either way",
                            width, ratio, autox_crossing_slope_ratio_max);
    } else if (ratio * autox_crossing_slope_ratio_max < 1) {
      const double order = autox_zero_order(q_start, s1 - s_start, q_end, saved_dstest - s1);
      if (!autox_zero_crosses(order)) {
        converged = false;
        refused = xpp::format("its test does not cross zero there: across the last {:.3g} of the search its slope is {:.3g} times its slope across the step, and its values {:.3g} and {:.3g} at the ends of the step make it a zero of order {}, not odd: a double zero the samples straddle",
                              width, ratio, q_start, q_end, order);
      }
    }
  }

  if (converged && accept && !(*accept)()) converged = false;

  if (converged) {
    iap.itp = xpp::AutoxPointCode{xpp::AutoxPointKind::Pending};
    // the discarded end is ahead of the located point: it, not 0, is the test's value there (W545)
    const double ahead = saved_dstest - s1;
    if (iterations > 0 && ahead * saved_dstest > 0) {
      q = q_end;
      sample.offset = ahead;
    } else {
      q = 0.;
    }
    if (!accept) xpp::print(iap.run->fp9," ==> Location of special point :  Convergence.    Stepsize ={:11.3E}\n",rds);
    return;
  }

  const double refused_parameter = parameter;
  saved.restore(iap, rap);
  dstest = saved_dstest;
  stepped = false;
  q = q_end;
  if (accept) return;
  xpp::print(iap.run->fp9,"{:4} {:5} NOTE:Possible special point\n",ibr,ntot + 1);
  if (refused.empty()) special_point_lost(iap, ibr, ntot, kind, why);
  else special_point_rejected(iap, ibr, ntot, kind, refused_parameter, refused);
}

/* AutoxRun::setubv_stop set for one solvbv, cleared the same way */
struct SetubvStop {
  AutoxRun &run;
  SetubvStop(AutoxRun &l, bool on) : run(l) { run.setubv_stop = on; }
  ~SetubvStop() { run.setubv_stop = false; }
  SetubvStop(const SetubvStop &) = delete;
  SetubvStop &operator=(const SetubvStop &) = delete;
};

/* fort.8, opened when the run writes its first label (wrtsp8, wrtbv8) and
   kept open for the rest of it (close_autox closes it). One that cannot be
   opened fails the run instead of exit(1), a server must outlive a bad
   HOME; a later run tries again. */
FILE *fort8(AutoxRun &run)
{
  if(!run.fp8){
    run.fp8.reset(xpp::files::open_stream(xpp::autox_fort_path(run,8),"w"));
    if(!run.fp8)xpp::autox_fail(xpp::files::write_error("autoX", xpp::autox_fort_path(run,8)));
  }
  return run.fp8.get();
}
} // namespace

int init(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *thl, std::vector<doublereal> &thu_vec, std::span<AutoxMark> marks)
{
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
  xpp::AutoxPointKind itpst;
  doublereal ds, rl0, rl1;
  integer iad, jac, lab, nbc, iid, ibr, ndm;
  doublereal det;
  integer ilp, nit, isp, irs, npr, isw, nmx, nbc0, nnt0;
  xpp::AutoxProblem ips;
  xpp::AutoxPointCode itp;

  for (i = 0; i < iap->run->parameter_count; ++i) {
    icp[i] = i;
    jtmp = iap->run->parameter_count;
    icp[jtmp + i] = 0;
    par[i] = 0.;
    par[jtmp + i] = 0.;
    thl[icp[i]] = 1.;
  }

 ndim=iap->run->params.ndim;
  ips=iap->run->params.ips;
  irs=iap->run->params.irs;
  ilp=iap->run->params.ilp;

  thu_vec.assign(xpp::size_product(static_cast<size_t>(ndim), AutoxState::state_copies), 1.);

  jtmp = iap->run->parameter_count;
  nicp=iap->run->params.nicp;

  for (i = 0; i < nicp; ++i) {
    icp[jtmp+i]=iap->run->params.icp[i];

  }
  
  if (nicp > 0) {
    for (i = 0; i < nicp; ++i) {
      jtmp = iap->run->parameter_count;
      icp[i] = icp[jtmp + i];
    }
  } else {
    nicp = 1;
    jtmp = iap->run->parameter_count;
    icp[jtmp] = icp[0];
  }
      
  ntst=iap->run->params.ntst;
  ncol=iap->run->params.ncol;
  iad=iap->run->params.iad;
  isp=iap->run->params.isp;
  isw=iap->run->params.isw;
  iplt=iap->run->params.iplt;
  nbc=iap->run->params.nbc;
  nint=iap->run->params.nint;
  nmx=iap->run->params.nmx;
  rl0=iap->run->params.rl0;
  rl1=iap->run->params.rl1;
  a0=iap->run->params.a0;
  a1=iap->run->params.a1;  
  npr=iap->run->params.npr;
  mxbf=iap->run->params.mxbf;
  iid=iap->run->params.iid;
  itmx=iap->run->params.itmx;
  itnw=iap->run->params.itnw;
  nwtn=iap->run->params.nwtn;
  jac=iap->run->params.jac;
  epsl=iap->run->params.epsl;
  epss=iap->run->params.epss;
  epsu=iap->run->params.epsu;
  ds=iap->run->params.ds;
  dsmin=iap->run->params.dsmin;
  dsmax=iap->run->params.dsmax;
  iads=iap->run->params.iads;

  for (const AutoxRunWeight &weight : iap->run->params.weights)
    thl[weight.index]=weight.value;
    
  nuzr=static_cast<integer>(iap->run->params.marks.size());
  std::copy(iap->run->params.marks.begin(),iap->run->params.marks.end(),marks.begin());

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

  itp = xpp::AutoxPointCode{};

  itpst = xpp::AutoxPointKind::Regular;

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

int 
chdim(iap_type *iap)
{
  integer npar;

/* Check dimensions. */

  npar = iap->nfpr;

  if (npar > iap->run->parameter_count) {
    xpp::autox_fail(xpp::format("dimension exceeded: {} free parameters, at most {}", npar, iap->run->parameter_count));
  }

  return 0;
} /* chdim_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*               The leading subroutines of autoX */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

int autoae(iap_type *iap, rap_type *rap, doublereal *par, 
	   integer *icp, 
	   FUNI_TYPE((*funi)),
	   STPNT_TYPE_AE((*stpnt)),
	   PVLI_TYPE_AE((*pvli)),
	   doublereal *thl, doublereal *thu, std::span<const AutoxMark> marks)

{
  /* This is the entry subroutine for algebraic systems. */

  cnrlae(iap, rap, par, icp, funi, stpnt, pvli, thl, thu, marks);
  return 0;
} 

int 
autobv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), STPNT_TYPE_BVP((*stpnt)), PVLI_TYPE_BVP((*pvli)), doublereal *thl, doublereal *thu, std::span<const AutoxMark> marks)
{

  /* THIS IS THE ENTRY ROUTINE FOR GENERAL BOUNDARY VALUE PROBLEMS. */

  cnrlbv(iap, rap, par, icp, funi, bcni, icni, stpnt, pvli, thl, thu, marks);
  return 0;
} 

int 
init1(iap_type *iap, rap_type *rap, integer *icp, doublereal *par)
{

  integer ndim, ncol, nicp;

  integer nneg, nfpr, nint, jtmp, nuzr, i;
  doublereal dsmin, dsmax, fc;
  integer ic, jc;
  doublereal ds;
  integer nxploc, jac, nbc, ndm, ilp, isp, irs,
    isw, nmx;
  xpp::AutoxProblem ips;
  xpp::AutoxPointCode itp;

  std::vector<integer> ict(iap->run->parameter_count);

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

  fc = autox_hmach1;
  ds = fc * ds;
  dsmin /= fc;
  dsmax = fc * dsmax;

  const bool reduced_hopf = ips == xpp::AutoxProblem::Algebraic || ips == xpp::AutoxProblem::SteadyState;

  /* Redefinition for waves */
  if (ips == xpp::AutoxProblem::WaveSteady) {
    ips = xpp::AutoxProblem::SteadyState;
    iap->ips = ips;
    ndim <<= 1;
    ndm = ndim;
    iap->ndm = ndm;
  } else if (ips == xpp::AutoxProblem::WavePeriodic) {
    ips = xpp::AutoxProblem::Periodic;
    iap->ips = ips;
    ndim <<= 1;
    ndm = ndim;
    iap->ndm = ndm;
  }

  /* General Redefinition. */

  if (xpp::problem_is_algebraic_family(ips) && isw == 1) {
    /*        ** Algebraic Systems */
    nfpr = 1;

  } else if (ips == xpp::AutoxProblem::TimeIntegration) {
    /*        ** Time integration */
    nfpr = 1;
    isp = 0;
    ilp = 0;
    icp[0] = xpp::autox_unknown::evolution_time;

  } else if (ips == xpp::AutoxProblem::Periodic && f2c::abs(isw) == 1) {
    /*        ** Periodic Solutions */
    nbc = ndim;
    nint = 1;
    nfpr = nbc + nint - ndim + 1;
    /*        **ISW=1 when starting from a HB */
    if (itp.is(xpp::AutoxPointKind::Hopf) || itp.started_from(xpp::AutoxPointKind::Hopf)) {
      isw = 1;
    }
    if (nicp == 1) {
      /*          **Variable period */
      icp[1] = xpp::autox_unknown::period;
    }

  } else if (ips == xpp::AutoxProblem::BoundaryValue && f2c::abs(isw) == 1) {
    /*        ** Boundary value problems */
    nfpr = nbc + nint - ndim + 1;

  } else if (ips == xpp::AutoxProblem::FloquetBoundaryValue && f2c::abs(isw) == 1) {
    /*        ** Boundary value problems */
    nfpr = nbc + nint - ndim + 1;

  } else if (ips == xpp::AutoxProblem::Homoclinic && f2c::abs(isw) == 1) {
    /*        ** Homoclinic bifurcation analysis */
    /*        Redefine autoX constants for homoclinic orbits */
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
    icp[0] = xpp::autox_unknown::evolution_time;

  } else if (ips == xpp::AutoxProblem::ParabolicUserSteady) {
    /*        **Stationary calculations for Parabolic Systems */
    ndim <<= 1;
    nbc = ndim;
    nint = 0;
    nfpr = 1;

  } else if (ips == xpp::AutoxProblem::PeriodicOptimization) {
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
    icp[nfpr] = xpp::autox_unknown::objective;
    icp[nfpr + 1] = xpp::autox_unknown::optimization_kappa;
    icp[nfpr + 2] = xpp::autox_unknown::optimization_gamma;
    nfpr += 3;
    ndim <<= 1;
    nbc = ndim;
    nint = nfpr - 1;
    /* overload to define optimality integrals */
    nneg = 0;
    for (i = 0; i < nicp; ++i) {
      ic = ict[i];
      jc = f2c::abs(ic) - xpp::autox_unknown::test_function_start;
      if (ic < 0 && jc > 0 && jc <= 11) {
	++nneg;
	icp[nfpr + nneg - 1] = jc;
      }
    }
    /* Set indices of output parameters */
    nicp = nfpr - 3;
    for (i = 0; i < nicp; ++i) {
      jtmp = iap->run->parameter_count;
      icp[jtmp + i] = icp[i];
    }

  } else if (ips == xpp::AutoxProblem::AlgebraicOptimization) {
    /*        ** Algebraic optimization Problems */
    if (iap->itp.kind == xpp::AutoxPointKind::Fold || iap->irs == 0) {
      iap->nfpr++;
    }
    nfpr = iap->nfpr;
    if (nfpr == 2) {
      ++ndim;
      icp[0] = xpp::autox_unknown::objective;
    } else {
      ndim = (ndim << 1) + nfpr;
      icp[0] = xpp::autox_unknown::objective;
    }

  } else if (irs > 0 && f2c::abs(isw) == 2) {
    /*        ** Continuation of singular points */

    if ((itp.is(xpp::AutoxPointKind::BranchPoint) || itp.started_from(xpp::AutoxPointKind::BranchPoint) || itp.is(xpp::AutoxPointKind::Fold) || itp.started_from(xpp::AutoxPointKind::Fold) || (ips==xpp::AutoxProblem::DiscreteMap && (itp.is(xpp::AutoxPointKind::PeriodDoubling) || itp.started_from(xpp::AutoxPointKind::PeriodDoubling))))
	&& xpp::problem_is_algebraic_family(ips)) {
      /*          ** Fold continuation (Algebraic Problems) */
      ndim = (ndim << 1) + 1;
      nfpr = 2;

    } else if ((itp.is(xpp::AutoxPointKind::Hopf) || itp.started_from(xpp::AutoxPointKind::Hopf) || (ips==xpp::AutoxProblem::DiscreteMap && (itp.is(xpp::AutoxPointKind::NeimarkSacker) || itp.started_from(xpp::AutoxPointKind::NeimarkSacker)))) && xpp::problem_is_algebraic_family(ips)) {
      /*          ** Hopf bifurcation continuation (Maps, ODE, Waves) */
      // Equilibria and maps use the reduced real-vector system; waves keep their own formulation.
      ndim = reduced_hopf || ips==xpp::AutoxProblem::DiscreteMap ? ndim * 2 + 2 : ndim * 3 + 2;
      nfpr = 2;

    } else if ((itp.is(xpp::AutoxPointKind::CycleFold) || itp.is(xpp::AutoxPointKind::CycleBranch)) && ips == xpp::AutoxProblem::Periodic) {
      /*          ** Fold continuation (Periodic solutions); start */
      ndim <<= 1;
      nbc = ndim;
      nint = 3;
      nfpr = nbc + nint - ndim + 1;
      if (icp[2] == xpp::autox_unknown::period || nicp == 2) {
	/*            ** Variable period */
	icp[1] = xpp::autox_unknown::fold_norm;
	icp[2] = xpp::autox_unknown::period;
	icp[3] = xpp::autox_unknown::fold_beta;
      } else {
	/*            ** Fixed period */
	icp[2] = xpp::autox_unknown::fold_norm;
	icp[3] = xpp::autox_unknown::fold_beta;
      }
      ilp = 0;
      isw = -2;
      isp = 0;
      nmx = 5;
      xpp::log_autox("\nGenerating starting data :\n Restart at EP label below :\n");
      iap->run->restart_flag=true;

    } else if ((itp.started_from(xpp::AutoxPointKind::CycleFold) || itp.started_from(xpp::AutoxPointKind::CycleBranch)) && ips == xpp::AutoxProblem::Periodic) {
      /*          ** Fold continuation (Periodic solutions); restart */
      ndim <<= 1;
      nbc = ndim;
      nint = 3;
      nfpr = nbc + nint - ndim + 1;
      if (nicp == 2) {
	/*            ** Variable period */
	icp[2] = xpp::autox_unknown::period;
      }
      icp[3] = xpp::autox_unknown::fold_beta;

    } else if (itp.is(xpp::AutoxPointKind::PeriodDoubling) && ips == xpp::AutoxProblem::Periodic) {
      /*          ** Continuation of period doubling bifurcations; start
 */
      ndim <<= 1;
      nbc = ndim;
      nint = 2;
      nfpr = nbc + nint - ndim + 1;
      if (icp[2] == xpp::autox_unknown::period || nicp == 2) {
	/*            ** Variable period */
	icp[1] = xpp::autox_unknown::period;
	icp[2] = xpp::autox_unknown::doubling_norm;
      } else {
	/*            ** Fixed period */
	icp[2] = xpp::autox_unknown::doubling_norm;
      }
      ilp = 0;
      isw = -2;
      isp = 0;
      nmx = 5;
      xpp::log_autox("\nGenerating starting data :\n Restart at EP label below :\n");
      iap->run->restart_flag=true;

    } else if (itp.started_from(xpp::AutoxPointKind::PeriodDoubling) && ips == xpp::AutoxProblem::Periodic) {
      /*          ** Continuation of period doubling bifurcations; resta
rt */
      ndim <<= 1;
      nbc = ndim;
      nint = 2;
      nfpr = nbc + nint - ndim + 1;
      if (icp[2] == xpp::autox_unknown::period || nicp == 2) {
	/*            ** Variable period */
	icp[2] = xpp::autox_unknown::period;
      }

    } else if (itp.is(xpp::AutoxPointKind::NeimarkSacker) && ips == xpp::AutoxProblem::Periodic) {
      /*          ** Continuation of torus bifurcations; start */
      ndim *= 3;
      nbc = ndim;
      nint = 3;
      nfpr = nbc + nint - ndim + 1;
      icp[1] = xpp::autox_unknown::period;
      icp[2] = xpp::autox_unknown::torus_angle;
      icp[3] = xpp::autox_unknown::torus_norm;
      ilp = 0;
      isp = 0;
      isw = -2;
      nmx = 5;
      xpp::log_autox("\nGenerating starting data :\n Restart at EP label below :\n");
      iap->run->restart_flag=true;

    } else if (itp.started_from(xpp::AutoxPointKind::NeimarkSacker) && ips == xpp::AutoxProblem::Periodic) {
      /*          ** Continuation of torus bifurcations; restart */
      ndim *= 3;
      nbc = ndim;
      nint = 3;
      nfpr = nbc + nint - ndim + 1;
      icp[2] = xpp::autox_unknown::period;
      icp[3] = xpp::autox_unknown::torus_angle;

    } else if ((itp.is(xpp::AutoxPointKind::CycleFold) || itp.is(xpp::AutoxPointKind::CycleBranch)) && ips == xpp::AutoxProblem::BoundaryValue) {
      /*          ** Continuation of folds (BVP; start) */
      ndim <<= 1;
      nbc <<= 1;
      nint = (nint << 1) + 1;
      nfpr = nbc + nint - ndim + 1;
      nxploc = nfpr / 2 - 1;
      if (nxploc > 0) {
	for (i = 0; i < nxploc; ++i) {
	  icp[nfpr / 2 + i] = i + xpp::autox_unknown::bvp_fold_start;
	}
      }
      icp[nfpr / 2] = nfpr / 2 + xpp::autox_unknown::bvp_fold_start;
      ilp = 0;
      isw = -2;
      isp = 0;
      nmx = 5; 
      iap->run->restart_flag=true;
      xpp::log_autox("\nGenerating starting data :\n Restart at EP label below :\n");

    } else if (itp.started_from(xpp::AutoxPointKind::CycleFold) && ips == xpp::AutoxProblem::BoundaryValue) {
      /* autoX's own test names the fold twice, never the branch point: kept, so the selection is the same */
      /*          ** Continuation of folds (BVP; restart) */
      ndim <<= 1;
      nbc <<= 1;
      nint = (nint << 1) + 1;
      nfpr = nbc + nint - ndim + 1;
      nxploc = nfpr / 2 - 1;
      if (nxploc > 0) {
	for (i = 0; i < nxploc; ++i) {
	  icp[nfpr / 2 + i] = i + xpp::autox_unknown::bvp_fold_start;
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

/* The direction (PAR(ICP(1))-dot, U-dot) of the other branch at a branch point: the unit vector
   in the 2-d nullspace of the Jacobian perpendicular to the known branch's direction (udot, rldot).
   It is written to du (ndim + 1 values); dfdu and dfdp are the Jacobian at the point. */
static void perpendicular_null_direction(iap_type *iap, integer *icp, integer *m1aaloc, doublereal *aa, doublereal *du, const doublereal *rldot, const doublereal *udot, const doublereal *dfdu, const doublereal *dfdp, const doublereal *thl, const doublereal *thu)
{
  const integer ndim = iap->ndim;
  integer i, j;
  doublereal sc;
  doublereal ss;

  integer aa_dim1 = *m1aaloc;

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

  xpp::nlvc(iap->run->fp9, ndim + 1, *m1aaloc, 1, aa, du);

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
}

/* The start of a restart that switches branches at a branch point of equilibria (ISW=-1; AUTO-07p's
   STPRAE with IPERP=1): the direction of the other branch is the null vector of the Jacobian
   perpendicular to the direction the point keeps with it (wrtsp8). Scaled and signed as AUTO-07p
   does: PAR(ICP(1))-dot positive, or, when it is zero, the first state that moves positive. A
   stored direction missing or made for another parameter ends the run saying so. */
static void
stpswae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), integer *m1aaloc, doublereal *aa, doublereal *rlcur, doublereal *u, doublereal *du, doublereal *udot, doublereal *rldot, doublereal *f, doublereal *dfdu, doublereal *dfdp, doublereal *thl, doublereal *thu)
{
  const auto &stored = iap->run->restart_direction;
  const std::string where = xpp::format("restart label {} in {}", iap->irs, xpp::autox_fort_path(*iap->run, 3));
  const integer ndim = iap->ndim;
  if (!stored)
    xpp::autox_fail(xpp::format("autoX: {} keeps no direction of its branch, so the branch cannot be switched there: only a branch point of an equilibrium run stores it", where));
  if (stored->parameter != icp[0] || stored->udot.size() != static_cast<std::size_t>(ndim)) {
    const xpp::Session &session = *iap->run->session;
    const auto named = [&session](integer solver_index) {
      const int k = session.autox_state.parameter_layout.model_index(static_cast<int>(solver_index));
      return k < 0 ? xpp::format("autoX's parameter {}", solver_index) : xpp::format("\"{}\"", session.model().upar_names[static_cast<std::size_t>(k)]);
    };
    xpp::autox_fail(xpp::Error{"autox", xpp::format("autoX: the direction {} keeps was made for {} with {} variables; the run continues {} with {}: switching continues the same parameter", where,
                                                  named(stored->parameter), stored->udot.size(), named(icp[0]), ndim), xpp::command_place()});
  }

  std::vector<doublereal> uold(u, u + ndim);
  (*funi)(iap, rap, ndim, u, uold.data(), icp, par, 2, f, dfdu, dfdp);
  std::copy(stored->udot.begin(), stored->udot.end(), udot);
  rldot[0] = stored->rldot;
  perpendicular_null_direction(iap, icp, m1aaloc, aa, du, rldot, udot, dfdu, dfdp, thl, thu);

  /* the sign: PAR(ICP(1))-dot positive, or the first state that moves when that is zero */
  doublereal lead = du[ndim];
  if (f2c::abs(lead) / (1. + f2c::abs(rlcur[0])) <= rap->epsl) {
    for (integer i = 0; i < ndim; ++i) {
      if (f2c::abs(du[i]) / (1. + f2c::abs(u[i])) > rap->epsu) {
        lead = du[i];
        break;
      }
    }
  }
  const doublereal sign = lead < 0. ? -1. : 1.;
  for (integer i = 0; i < ndim; ++i)
    udot[i] = sign * du[i];
  rldot[0] = sign * du[ndim];
}

int 
cnrlae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), STPNT_TYPE_AE((*stpnt)), PVLI_TYPE_AE((*pvli)), doublereal *thl, doublereal *thu, std::span<const AutoxMark> marks)
{

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

  std::vector<doublereal> rlold(iap->run->parameter_count);
  std::vector<doublereal> rldot(iap->run->parameter_count);
  std::vector<doublereal> rlcur(iap->run->parameter_count);
  integer istop;
  xpp::AutoxPointKind itpst;

  doublereal ds;

  integer lab, ibr;
  doublereal rds;
  AutoxTestSample rbp, rev, rlp, closing;
  xpp::AutoxProblem ips;
  integer irs, isp;
  integer nit;
  xpp::AutoxPointCode itp;

  integer aa_first_dimension = iap->ndim + 1;

  std::vector<doublereal> dfdp(xpp::size_product(iap->ndim, iap->run->parameter_count));
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
  std::vector<AutoxTestSample> uzr(iap->nuzr);
    
  /* Controls the bifurcation analysis of algebraic problems */

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
  rbp = rev = rlp = closing = {};
  if (nuzr > 0) {
    for (i = 0; i < nuzr; ++i) {
      uzr[i] = {};
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

  iap->run->codim.prepare(iap->ndm);
  AutoxTestSample rbt, rcp, rgh;
  const auto special_points=iap->run->session->autox_state.special_points;

  /* The tests of the point just solved. A switched branch's second point leaves the branch point test out: the
     determinant there is of the switch's Newton matrix, whose last row is not the arclength condition. */
  using Tests = std::vector<LocationTest<decltype(&fnuzae)>>;
  /* W604: whether a crossing of the start's parameter value is the start (autox_ends.h) */
  const std::function<bool()> closes = [&] {
    return xpp::autox_branch_closes(*iap, rap->epss, par, icp, std::span<const doublereal>(u.data(), static_cast<std::size_t>(iap->ndim)));
  };
  const auto tests_at_point = [&](bool with_branch_point) {
    Tests tests;
    for (integer index = 0; index < nuzr; ++index)
      tests.push_back({fnuzae, uzr[index], xpp::AutoxPointKind::UserZero, index});
    if (iap->ilp == 1) tests.push_back({fnlpae, rlp, xpp::AutoxPointKind::Fold, {}});
    if (with_branch_point && isp != 0 && (iap->isw!=2 || ips==xpp::AutoxProblem::DiscreteMap)) tests.push_back({fnbpae, rbp, xpp::AutoxPointKind::BranchPoint, {}});
    const bool fold_curve=iap->isw==2 && iap->itpst==xpp::AutoxPointKind::Fold;
    const bool hopf_curve=iap->isw==2 && iap->itpst==xpp::AutoxPointKind::Hopf && iap->ndim==2*iap->ndm+2;
    const bool codim=ips==xpp::AutoxProblem::SteadyState && (fold_curve || hopf_curve);
    if (codim && isp!=0 && isp!=3 && xpp::autox_codim_enabled(special_points,xpp::AutoxPointKind::BogdanovTakens))
      tests.push_back({fnbtae,rbt,xpp::AutoxPointKind::BogdanovTakens,{}});
    if (codim && isp!=0 && isp!=3 && fold_curve && xpp::autox_codim_enabled(special_points,xpp::AutoxPointKind::Cusp))
      tests.push_back({fncpae,rcp,xpp::AutoxPointKind::Cusp,{}});
    if (ips==xpp::AutoxProblem::DiscreteMap && iap->isw==2 && isp!=0)
      tests.push_back({fnrnae,rbt,xpp::AutoxPointKind::Pending,{}});
    if (xpp::problem_is_steady_or_map(ips))
      tests.push_back({fnhbae, rev, codim ? xpp::AutoxPointKind::ZeroHopf : xpp::AutoxPointKind::Hopf, {}});
    if (codim && isp!=0 && isp!=3 && hopf_curve && xpp::autox_codim_enabled(special_points,xpp::AutoxPointKind::GeneralizedHopf))
      tests.push_back({fnghae,rgh,xpp::AutoxPointKind::GeneralizedHopf,{}});
    tests.push_back({fnclosedae, closing, xpp::AutoxPointKind::BranchClosed, {}, &closes});
    return tests;
  };
  const auto evaluate_test = [&](const auto &test) {
    bool changed;
    return test.function(iap, rap, par, icp, &changed, funi, &aa_first_dimension,
        aa.data(), rlcur.data(), rlold.data(), rldot.data(), u.data(), uold.data(),
        udot.data(), rhs.data(), dfdu.data(), dfdp.data(), marks);
  };

  /* The tests at the point just solved, as the loop makes them at each point it computes: a sign change from the
     tests' last values is located, a branch point's data kept. */
  const auto scan_point = [&] {
    auto tests = tests_at_point(true);
    locate_tests(*iap, *rap, tests, marks, evaluate_test,
      [&](auto &test, double &dstest, bool &stepped) {
        lcspae(iap, rap, par, icp, test.function, funi, &aa_first_dimension,
            aa.data(), rhs.data(), rlcur.data(), rlold.data(), rldot.data(), u.data(),
            du.data(), uold.data(), udot.data(), f.data(), dfdu.data(), dfdp.data(),
            test.sample, thl, thu, marks, dstest, stepped, test.accept);
        if (ips==xpp::AutoxProblem::DiscreteMap && (test.function==fnhbae || test.function==fnrnae))
          test.kind=iap->run->codim.map_kind;
      });
    if (iap->istop != 1 && iap->itp.kind == xpp::AutoxPointKind::BranchPoint) {
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
  };

  /* The determinant of the Newton matrix the step from the start would have (solvae's: its last row,
     2 thu (u - uold) / rds, is 2 thu udot for a step along udot), the branch point test's value at the start. */
  const auto start_determinant = [&] {
    const integer ndim = iap->ndim, ld = aa_first_dimension;
    std::vector<doublereal> matrix(aa.size()), rhs_unused(ndim + 1), solution(ndim + 1);
    for (integer i = 0; i < ndim; ++i) {
      matrix[i + ndim * ld] = dfdp[icp[0] * ndim + i];
      for (integer k = 0; k < ndim; ++k) matrix[i + k * ld] = dfdu[k * ndim + i];
    }
    for (integer k = 0; k < ndim; ++k) matrix[ndim + k * ld] = thu[k] * 2. * udot[k];
    matrix[ndim + ndim * ld] = thl[icp[0]] * 2. * rldot[0];
    doublereal det;
    xpp::ge(iap->run->fp9, iap->iid, ndim + 1, ld, matrix.data(), 1, ndim + 1, solution.data(), ndim + 1, rhs_unused.data(), &det);
    return det;
  };

  /* W572: the tests' values at a point the loop does not test (a branch's first point, or the second of a
     switched branch), so the step to the next compares two values and a zero between them is located. The
     point's stored stability and period stay what they were: the Hopf test's evaluation writes both. A
     switched branch's second point has no branch point test (its determinant is the switch's own). */
  const auto seed_tests = [&](bool at_start) {
    const auto stability = iap->run->stability.computed;
    const double period = par[xpp::autox_unknown::period];
    const doublereal det = rap->det;
    if (at_start) rap->det = start_determinant();
    auto tests = tests_at_point(at_start);
    sample_tests(*iap, tests, tests.size(), evaluate_test);
    iap->run->stability.computed = stability;
    par[xpp::autox_unknown::period] = period;
    rap->det = det;
  };

  (*stpnt)(iap, rap, par, icp, u.data());
  const xpp::AutoxPointKind start_kind = iap->itp.kind; // what the restart label was (stpnt read it)
  xpp::autox_check_start_parameters(*iap,*rap,par,icp);
  xpp::autox_check_start_equilibrium(*iap,*rap,u.data(),par,icp);
  constexpr integer stride=1; // The algebraic state is contiguous.
  xpp::autox_check_start_norm(*iap,*rap,xpp::dnrm2(&iap->ndm,u.data(),&stride));
  (*pvli)(iap, rap, u.data(), par);

/* Determine a suitable starting label and branch number */

  newlab(iap, rap);

  /* Write constants */

  sthd(iap, rap, par, icp, thl, thu);

  /* Write plotting data for the starting point */

  istop = 0;
  iap->istop = istop;
  if (irs == 0) {
    itp = xpp::AutoxPointCode{xpp::AutoxPointKind::End, itpst};
  } else {
    itp = xpp::AutoxPointCode{};
  }
  iap->itp = itp;
  rlcur[0] = par[icp[0]];
  stplae(iap, rap, par, icp, rlcur.data(), u.data(), udot.data(), rldot.data());
  istop = iap->istop;
  if (istop == 1) {
    goto L6;
  }

  /* A restart that switches branches (ISW=-1) is at a branch point: the direction of the other
     branch, then its second point in the hyper-plane through the point perpendicular to it. */

  if (iap->isw < 0) {
    stpswae(iap, rap, par, icp, funi, &aa_first_dimension, aa.data(), rlcur.data(), u.data(), du.data(), udot.data(), rldot.data(), f.data(), dfdu.data(), dfdp.data(), thl, thu);
    goto L4;
  }

  /* Starting procedure  (to get second point on first branch) : */

  stdirae(iap, rap, par, icp, funi, &aa_first_dimension, aa.data(), rhs.data(), rlold.data(), rldot.data(), u.data(),
	  du.data(), uold.data(), udot.data(), f.data(), dfdu.data(), dfdp.data(), thl, thu);

  /* W572: the start's tests, from its Jacobian and direction */

  if (start_is_tested(*iap, start_kind)) {
    seed_tests(true);
  }
  stprae(iap, rap, par, icp, funi, &rds, &aa_first_dimension, aa.data(), 
	 rhs.data(), rlcur.data(), rlold.data(), rldot.data(), u.data(), du.data(), uold.data(), udot.data(), f.data(), dfdu.data(), dfdp.data(), thl, 
	 thu);
  istop = iap->istop;
  if (istop == 1) {
    goto L5;
  }

  /* W572: the second point, which is not stored, is tested like each point the loop computes, so a zero between
     the start and it, or between it and the next point, is located and stored; no zero: the loop goes on. */

  itp = xpp::AutoxPointCode{};
  iap->itp = itp;
  scan_point();
  if (iap->istop != 0 || iap->itp.kind != xpp::AutoxPointKind::Regular) {
    goto L5;
  }
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

  rbp = rev = rlp = closing = {};
  if (nuzr > 0) {
    for (i = 0; i < nuzr; ++i) {
      uzr[i] = {};
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
  itp = xpp::AutoxPointCode{};
  iap->itp = itp;
  nit = 0;
  iap->nit = nit;
  dsold = rds;
  rap->dsold = dsold;

/* Store plotting data for first point on the bifurcating branch */

  stplae(iap, rap, par, icp, rlcur.data(), u.data(), udot.data(), rldot.data());
  istop = iap->istop;
  if (istop == 1) {
    goto L6;
  }

 L4:
  /* Determine the second point on the bifurcating branch */

  swprc(iap, rap, par, icp, funi, &aa_first_dimension, aa.data(), rhs.data(), 
	rlcur.data(), rlold.data(), rldot.data(), u.data(), du.data(), uold.data(), udot.data(), f.data(), dfdu.data(), dfdp.data(), &rds, thl, 
	thu);
  istop = iap->istop;
  if (istop == 1) {
    goto L5;
  }

  /* Store plotting data for second point : */

  stplae(iap, rap, par, icp, rlcur.data(), u.data(), udot.data(), rldot.data());
  istop = iap->istop;
  if (istop == 1) {
    goto L6;
  }
  rbp = rev = rlp = closing = {};

  /* W569, W572: the new branch's tests start at its second point, not its third (AUTO-07p's tests begin there:
     a fold between the second and third points is never labelled, as pp2's after the switch at its branch point).
     The branch point test is not among them: its determinant is the switch's own. The fold's tangent is the branch
     point's direction, which has the sign of the point's own until a fold turns it. */

  seed_tests(false);

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

  scan_point();

  /* Store plotting data on unit 7 : */

 L5:
  stplae(iap, rap, par, icp, rlcur.data(), u.data(), udot.data(), rldot.data());

/* Adapt the stepsize along the branch */

  itp = iap->itp;
  ntot = iap->ntot;
  if (iads != 0 && ntot % iads == 0 && (itp.kind == xpp::AutoxPointKind::Regular || itp.kind == xpp::AutoxPointKind::Output)) {
    adptds(iap, rap, &rds);
  }

 L6:
  itp = xpp::AutoxPointCode{};
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

int 
stpnus(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *u)
{
  integer ndim;

  /* Gets the starting data from user supplied STPNT */

  ndim = iap->ndim;

  stpnt(*iap->run, ndim, 0.0, u, par);

  return 0;
} /* stpnus_ */

int 
stpnae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *u)
{
  bool found;
  integer nfprs;

  integer irs;

  /* Gets the starting data from unit 3 */

  irs = iap->irs;
  findlb(iap, rap, irs, &nfprs, &found);
  readlb(iap, rap, u, par);
  return 0;
} /* stpnae_ */

/* The direction of the branch at its starting point (W572 splits it from stprae: cnrlae tests the start
   with its Jacobian and this direction before the step to the second point). */
int 
stdirae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), integer *m1aaloc, doublereal *aa, doublereal *rhs, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *du, doublereal *uold, doublereal *udot, doublereal *f, doublereal *dfdu, doublereal *dfdp, doublereal *thl, doublereal *thu)
{
  xpp::autox_stability_step_start(iap->run->stability,
      iap->ibr, iap->ntot + (iap->run->locating ? 1 : 0));

  integer aa_dim1;

  integer ndim;

  doublereal sign;
  integer i, k;

  doublereal sc;
  doublereal ss;

  integer iid;

/* Finds the direction at the first point of the initial solution branch. */

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
  xpp::nlvc(iap->run->fp9, ndim + 1, *m1aaloc, 1, aa, du);

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

  return 0;
} /* stdirae */

/* Finds the second point on the initial solution branch, from the direction stdirae found. */

int 
stprae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), doublereal *rds, integer *m1aaloc, doublereal *aa, doublereal *rhs, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *du, doublereal *uold, doublereal *udot, doublereal *f, doublereal *dfdu, doublereal *dfdp, doublereal *thl, doublereal *thu)
{
  const integer ndim = iap->ndim;

/* Set initial approximations to the second point on the branch */

  for (integer i = 0; i < ndim; ++i) {
    u[i] = uold[i] + *rds * udot[i];
  }
  rlcur[0] = rlold[0] + *rds * rldot[0];

  solvae(iap, rap, par, icp, funi, rds, m1aaloc, aa,
	 rhs, rlcur, rlold, rldot, u, du, uold, 
	 udot, f, dfdu, dfdp, thl, thu);

  return 0;
} /* stprae_ */

int 
contae(iap_type *iap, rap_type *rap, doublereal *rds, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *uold, doublereal *udot)
{
  xpp::autox_stability_step_start(iap->run->stability,
      iap->ibr, iap->ntot + (iap->run->locating ? 1 : 0));

  integer ndim, i;
  doublereal dsold;
  xpp::AutoxProblem ips;

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
  if (ips == xpp::AutoxProblem::TimeIntegration) {
    rap->tivp = rlold[0];
  }

  return 0;
} /* contae_ */

int 
solvae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), doublereal *rds, integer *m1aaloc, doublereal *aa, doublereal *rhs, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *du, doublereal *uold, doublereal *udot, doublereal *f, doublereal *dfdu, doublereal *dfdp, doublereal *thl, doublereal *thu)
{
  double pivot = NAN; /* the last linear solve's smallest pivot (xpp::ge) */
  integer aa_dim1;

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
  integer &last_ntop=iap->run->solvae_ntop;

/* This is the subroutine for computing solution branches. It solves */
/* the equations for finding the next point on the branch at distance DS 
*/
/* from the current point. An initial approximation to the new point */
/* ( i.e. to PAR(ICP(1)) and U ) has been supplied by CONT. */

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
      xpp::print(iap->run->fp9,"========================================");
      xpp::print(iap->run->fp9,"========================================\n");
      last_ntop = ntop;
    }
    if (nit == 0) {
      xpp::print(iap->run->fp9,"  BR    PT  IT\n");
    }

    xpp::print(iap->run->fp9,"{:4} {:5}{:4}    {:14.6E}              ",ibr,ntop,
	    nit,rlcur[0]);
    for (i = 0; i < ndmr; ++i) {
      xpp::print(iap->run->fp9,"{:14.6E}",u[i]);
    }
    xpp::print(iap->run->fp9,"\n");
  }

  /* Call user-supplied FUNC to evaluate the right hand side of the */
  /* differential equation and its derivatives : */

  for (nit1 = 1; nit1 <= itnw; ++nit1) {

    if (!iap->run->locating && autox_cancelled(*iap->run)) goto L5; /* xppautX: cancel */
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

    pivot = xpp::ge(iap->run->fp9, iap->iid, ndim + 1, *m1aaloc, aa, 1, ndim + 1, du,
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
      xpp::print(iap->run->fp9,"{:4} {:5}{:4}    {:14.6E}              ",ibr,ntop,
	      nit,rlcur[0]);
      for (i = 0; i < ndmr; ++i) {
	xpp::print(iap->run->fp9,"{:14.6E}",u[i]);
      }
      xpp::print(iap->run->fp9,"\n");
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
      if (!xpp::all_finite({par, static_cast<size_t>(iap->run->parameter_count)})) {
        nonfinite = true;
        goto L3;
      }
      if (iid >= 2) {
	xpp::print(iap->run->fp9,"\n");
      }
      return 0;
    }

    /* Check whether relative error has reached user-supplied tolerance : 
     */

    if (nit == 1) {
      delref = f2c::max(rdrlm,rdumx) * autox_divergence_ratio;
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
    xpp::print(iap->run->fp9,"{:4} {:5} NOTE:No convergence with fixed step size\n",ibr,ntop);
  }
  if (iads == 0) {
    xpp::autox_stop_noconv(iap->run->stop, {xpp::AUTOX_STOP_NOCONV_FIXED, *rds, dsmin, itnw, epsl, epsu, pivot}); /* xppautX: T23 */
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
    xpp::print(iap->run->fp9," NOTE:Retrying step\n");
  }
  goto L1;

/* Minimum stepsize reached */

 L4:
  xpp::print(iap->run->fp9,"{:4} {:5} NOTE:No convergence using minimum step size\n",ibr,ntop);
  xpp::autox_stop_noconv(iap->run->stop, {xpp::AUTOX_STOP_NOCONV_MIN, *rds, dsmin, itnw, epsl, epsu, pivot}); /* xppautX: T23 */
 L5:
  rlcur[0] = rlold[0];
  par[icp[0]] = rlcur[0];
  for (i = 0; i < ndim; ++i) {
    u[i] = uold[i];
  }
  const bool retained = xpp::autox_stability_step_failed(iap->run->stability,
      iap->ibr, iap->ntot + 1);
  istop = 1;
  iap->istop = istop;
  if (nonfinite && !stop_asked(iap))
    xpp::autox_nonfinite(*iap, iap->ntot + 1);
  if (!retained && xpp::problem_is_steady_or_map(iap->ips) &&
      !stop_asked(iap))
    xpp::autox_stability_equilibrium_restored(*iap, *rap, par, icp, funi, u, uold, f, dfdu, dfdp);
  return 0;
} /* solvae_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*               Detection of Singular Points */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

int 
lcspae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FNCS_TYPE_AE((*fncs)), FUNI_TYPE((*funi)), integer *m1aaloc, doublereal *aa, doublereal *rhs, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *du, doublereal *uold, doublereal *udot, doublereal *f, doublereal *dfdu, doublereal *dfdp, AutoxTestSample &q, doublereal *thl, doublereal *thu, std::span<const AutoxMark> marks, double &dstest, bool &stepped, const std::function<bool()> *accept)
{
  /* This subroutine uses the secant method to accurately locate special */
  /* points (branch points, folds, Hopf bifurcations, user zeroes). */
  /* These are characterized as zeroes of the function FNCS supplied in the 
*/
/* call. */
/* This subroutine calls CONT and SOLVAE with varying stepsize RDS. */
/* The special point is assumed to have been found with sufficient */
/* accuracy if the ratio between RDS and the user supplied value of */
/* DS is less than the user-supplied toler du. */
  const auto n = static_cast<std::size_t>(iap->ndim);
  const auto np = static_cast<std::size_t>(iap->run->parameter_count);
  const auto na = static_cast<std::size_t>(*m1aaloc) * (n + 1);
  locate_special_point(*iap, *rap, q, located_kind(fncs), fncs == fnuzae, false,
      fncs == fnrnae && iap->itpst == xpp::AutoxPointKind::NeimarkSacker, par[icp[0]], dstest, stepped, accept,
      [&](bool &chng) {
        return (*fncs)(iap, rap, par, icp, &chng, funi, m1aaloc, aa, rlcur, rlold, rldot,
                       u, uold, udot, rhs, dfdu, dfdp, marks);
      },
      [&](double &rds) {
        contae(iap, rap, &rds, rlcur, rlold, rldot, u, uold, udot);
        solvae(iap, rap, par, icp, funi, &rds, m1aaloc, aa, rhs, rlcur, rlold, rldot, u, du,
               uold, udot, f, dfdu, dfdp, thl, thu);
      },
      [&] {
        return LocationState(*iap, *rap, {{par, np}, {rlcur, 1}, {rlold, 1}, {rldot, 1},
            {u, n}, {uold, n}, {udot, n}, {aa, na}, {rhs, n + 1}, {du, n + 1},
            {f, n}, {dfdu, n * n}, {dfdp, n * np}});
      },
      [&] {
        return iap->itpst == xpp::AutoxPointKind::NeimarkSacker &&
            xpp::autox_torus_singular(u[2 * iap->ndm]);
      });
  return 0;
} /* lcspae_ */

int 
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

doublereal 
fnbpae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, bool *chng, FUNI_TYPE((*funi)), integer *m1aaloc, doublereal *aa, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *uold, doublereal *udot, doublereal *rhs, doublereal *dfdu, doublereal *dfdp, std::span<const AutoxMark> marks)
{
  doublereal ret_val;

  integer ntop, ntot, iid, ibr;
  doublereal det;

  iid = iap->iid;
  ibr = iap->ibr;
  ntot = iap->ntot;
  ntop = ntot + 1;

  det = rap->det;
  ret_val = det;
  *chng = true;

  /* If requested write additional output on unit 9 : */

  if (iid >= 2) {
    xpp::print(iap->run->fp9,"{:4} {:5}        BP   Function {:14.6E}\n",ibr,ntop,ret_val);
  }

  return ret_val;
} /* fnbpae_ */

doublereal 
fnlpae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, bool *chng, FUNI_TYPE((*funi)), integer *m1aaloc, doublereal *aa, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *uold, doublereal *udot, doublereal *rhs, doublereal *dfdu, doublereal *dfdp, std::span<const AutoxMark> marks)
{
  integer aa_dim1;
  doublereal ret_val;

  integer ndim, ntop, ntot, i, k;

  integer iid, ibr;
  doublereal det;

  std::vector<doublereal> ud(xpp::size_sum(iap->ndim, 1));

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
  xpp::ge(iap->run->fp9, iap->iid, ndim + 1, *m1aaloc, aa, 1, ndim + 1, ud.data(), ndim + 1, rhs, &det);
  {
    integer tmp = ndim + 1;
    nrmlz(&tmp, ud.data());
  }
  /* W609 (#676): the null vector points along the step just taken (u - uold over its signed length), not along the
     tangent the step started with. AUTO-07p's FNLPAE (toolboxae.f90:703-747) orients it by the old tangent; a step
     that turns past 90 degrees in one go (a coarse step across a fold's tight turn) then leaves the test's sign
     unchanged where the fold is, and the zero is found a step late, where the stale sample meets a correctly oriented
     one (enz_isola at step 0.05: the fold 27.0747 labelled at 27.0470). No step taken yet (a branch's start) leaves the
     old tangent's orientation. */
  double along = ud[ndim] * (rlcur[0] - rlold[0]);
  for (k = 0; k < ndim; ++k) along += ud[k] * (u[k] - uold[k]);
  ret_val = along * rap->dsold < 0 ? -ud[ndim] : ud[ndim];
  *chng = true;

  /* If requested write additional output on unit 9 : */

  if (iid >= 2) {
    xpp::print(iap->run->fp9,"{:4} {:5}        Fold Function {:14.6E}\n",f2c::abs(ibr),ntop,ret_val);
  }
  return ret_val;
} /* fnlpae_ */

doublereal 
fnhbae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, bool *chng, FUNI_TYPE((*funi)), integer *m1aaloc, doublereal *aa, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *uold, doublereal *udot, doublereal *rhs, doublereal *dfdu, doublereal *dfdp, std::span<const AutoxMark> marks)
{
  doublereal ret_val;

  integer ndim;
  doublereal rmax;
  integer nins, ntop, ntot;
  doublecomplex ztmp;
  integer nins1, i, j;
  integer ntotp1;

  doublereal rp;

  integer iid, ibr, ndm, ier, loc=0;
  xpp::AutoxProblem ips;
  doublereal rev;
  integer isw;

  std::vector<doublecomplex> ev(iap->ndim);

  ndim = iap->ndim;
  ndm = iap->ndm;
  ips = iap->ips;
  isw = iap->isw;
  iid = iap->iid;
  ibr = iap->ibr;
  ntot = iap->ntot;
  ntop = ntot + 1;

/* INITIALIZE */

  *chng = false;

  /* Compute the eigenvalues of the Jacobian */

  const bool codim_curve=ips==xpp::AutoxProblem::SteadyState && isw==2 &&
      (iap->itpst==xpp::AutoxPointKind::Fold ||
       (iap->itpst==xpp::AutoxPointKind::Hopf && ndim==2*ndm+2));
  const double axis_tolerance = xpp::autox_hopf_axis_tolerance(
      xpp::Matrix<const double>({dfdu,static_cast<size_t>(ndim*ndm)},ndim,ndm),ndm);
  // EISPACK overwrites its matrix: dfdu stays the converged point's Jacobian
  // for the tests after this one and stbif (W421; AUTO-07p PVLSAE's copy).
  std::vector<doublereal> jacobian(dfdu, dfdu + static_cast<size_t>(ndim*ndm));
  xpp::eig(iap, &ndm, &ndim, jacobian.data(), ev.data(), &ier);
  if (ips == xpp::AutoxProblem::DiscreteMap) {
    for (i = 0; i < ndm; ++i) {
      if (ev[i].r != -1. || ev[i].i != 0.) {
	/* log(1 + ev) */
	doublecomplex in;
	in.r = ev[i].r + 1., in.i = ev[i].i;
	ev[i].i = xpp::math::atan2(in.i, in.r);
	ev[i].r = xpp::math::log(z_abs(&in));
      } else {
	ev[i].r = -autox_rlarge, ev[i].i = 0.;
      }
    }
  }
  /* xppautX: the point's eigenvalues (autox_stability.h; eig computed ndm) */
  xpp::autox_stability_computed(iap->run->stability,ibr,ntot+1,{&ev[0].r,2*static_cast<size_t>(ndm)},xpp::AUTOX_STABILITY_STEADY);
  /* Order the eigenvalues by real part. */

  for (i = 0; i < ndm - 1; ++i) {
    rmax = -autox_rlarge;
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
  rev = autox_test_not_applicable;
  const auto pair = xpp::autox_hopf_pair({ev.data(),static_cast<size_t>(ndm)},axis_tolerance);
  if (pair) {
    rev = pair->r;
    if (!codim_curve && ips!=xpp::AutoxProblem::DiscreteMap) par[xpp::autox_unknown::period] = pi(2.0) / std::abs(pair->i);
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
      if (ev[i].r <= autox_hmach) {
	++nins1;
      }
    }
  }

  const bool map=ips==xpp::AutoxProblem::DiscreteMap;
  if (map || codim_curve) {
    // AUTO-07p STABEQ: omit the imposed zero/+1/-1, or the imposed pair.
    const integer forced=isw!=2 ? 0 :
        (iap->itpst==xpp::AutoxPointKind::Hopf || iap->itpst==xpp::AutoxPointKind::NeimarkSacker ? 2 : 1);
    std::stable_sort(ev.begin(),ev.begin()+ndm,[](const auto &a,const auto &b) { return std::abs(a.r)<std::abs(b.r); });
    nins1=forced;
    for (i=forced;i<ndm;++i) if (ev[i].r<=axis_tolerance) ++nins1;
    ret_val=ndm>forced ? ev[forced].r : autox_test_not_applicable;
    if (map) {
      std::optional<doublecomplex> selected;
      for (i=forced;i<ndm;++i)
        if ((forced || ev[i].i!=0.) && (!selected || std::abs(ev[i].r)<std::abs(selected->r))) selected=ev[i];
      auto &kind=iap->run->codim.map_kind;
      kind=selected ? xpp::autox_map_secondary_kind(isw==2 ? iap->itpst : xpp::AutoxPointKind::Regular,std::abs(selected->i),rap->epss) : xpp::AutoxPointKind::Regular;
      ret_val=selected ? selected->r : autox_test_not_applicable;
      *chng=nins1!=iap->nins && kind!=xpp::AutoxPointKind::Regular && iap->isp!=0 &&
          (isw!=2 || xpp::autox_codim_enabled(iap->run->session->autox_state.special_points,kind));
      if (isw!=2 && selected && kind==xpp::AutoxPointKind::NeimarkSacker)
        par[xpp::autox_unknown::period]=pi(2.)/std::abs(selected->i);
    }
  } else {
    ret_val = isw==2 ? autox_test_not_applicable : rev;
  }
  if (!map) {
    nins = iap->nins;
    // AUTO-07p FNHBEQ equilibrium.f90:624-629: a plain HB changes a pair.
    constexpr integer hopf_stability_change = 2; // one complex conjugate pair
    const integer required_change=isw==2 && iap->itpst==xpp::AutoxPointKind::Hopf ? 1 : hopf_stability_change;
    if (f2c::abs(nins1 - nins) >= required_change) *chng = true;
    if (isw==2 && (iap->isp==0 || iap->isp==3 ||
        !xpp::autox_codim_enabled(iap->run->session->autox_state.special_points,xpp::AutoxPointKind::ZeroHopf) ||
        iap->itp.kind==xpp::AutoxPointKind::BogdanovTakens)) { *chng=false; ret_val=autox_test_not_applicable; }
  }
  nins = nins1;
  iap->nins = nins;

  ntot = iap->ntot;
  ntotp1 = ntot + 1;
  if (iid >= 2) {
    xpp::print(iap->run->fp9,"{:4} {:5}        Hopf Function {:14.6E}\n",f2c::abs(ibr),ntop,ret_val);
  }
  if (nins1 == ndm) {
    ntotp1 = -ntotp1;
  }

  xpp::print(iap->run->fp9,"{:4} {:5}        Eigenvalues:                                 Stable:{:3}\n",f2c::abs(ibr),ntop,nins);
  if (ips == xpp::AutoxProblem::DiscreteMap) {
    for (i = 0; i < ndm; ++i) {
      const double expx = xpp::math::exp(ev[i].r); /* exp(ev) */
      xpp::print(iap->run->fp9,"{:4} {:5}        Eigenvalue{:3} {:14.6E}{:14.6E}\n",f2c::abs(ibr),ntop,i+1,
                 expx * xpp::math::cos(ev[i].i),expx * xpp::math::sin(ev[i].i));
    }
  } else {
    for (i = 0; i < ndm; ++i) {
      xpp::print(iap->run->fp9,"{:4} {:5}        Eigenvalue{:3} {:14.6E}{:14.6E}\n",f2c::abs(ibr),ntop,i+1,ev[i].r,ev[i].i);
    }
  }

  return ret_val;
} /* fnhbae_ */

namespace {
// IID 3 starts detailed scalar-test diagnostics in autoX.
constexpr integer user_zero_diagnostic_level = 3;
// Preserve the two original diagnostic column widths.
constexpr int algebraic_user_zero_width = 16;
constexpr int boundary_user_zero_width = 14;

/* W604: the hidden test of a branch's return to its start: the continuation parameter less the start's, which
   applies once the start is stored (autox_ends.h) */
doublereal closed_test(const iap_type &iap, const doublereal *par, const integer *icp, bool *chng) {
  *chng = iap.run->branch_start.has_value();
  return *chng ? par[icp[0]] - iap.run->branch_start->point.parameters[0] : autox_test_not_applicable;
}

doublereal user_zero(const iap_type &iap, const doublereal *par,
                     const integer *icp, bool *chng,
                     std::span<const AutoxMark> marks, int width) {
  const integer iuzr = iap.iuzr;
  const doublereal value = autox_parameter_value(
      *iap.run, f2c::abs(marks[iuzr].parameter),
      {icp, static_cast<std::size_t>(iap.nfpr)},
      {par, static_cast<std::size_t>(iap.run->parameter_count)}) - marks[iuzr].value;
  *chng = true;
  if (iap.iid >= user_zero_diagnostic_level) {
    xpp::print(iap.run->fp9, "{:4} {:5}        User Func. {:3} {:{}.6E}\n",
               f2c::abs(iap.ibr), iap.ntot + 1,
               iuzr, value, width);
  }
  return value;
}
} // namespace

doublereal
fnuzae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, bool *chng, FUNI_TYPE((*funi)), integer *m1aaloc, doublereal *aa, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *uold, doublereal *udot, doublereal *rhs, doublereal *dfdu, doublereal *dfdp, std::span<const AutoxMark> marks)
{
  return user_zero(*iap, par, icp, chng, marks, algebraic_user_zero_width);
} /* fnuzae_ */

doublereal
fnclosedae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, bool *chng, FUNI_TYPE((*funi)), integer *m1aaloc, doublereal *aa, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *uold, doublereal *udot, doublereal *rhs, doublereal *dfdu, doublereal *dfdp, std::span<const AutoxMark> marks)
{
  return closed_test(*iap, par, icp, chng);
} /* fnclosedae_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*                   Branch Switching for Algebraic Problems */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

int 
stbif(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *m1aaloc, doublereal *aa, doublereal *stud, doublereal *stu, doublereal *stla, doublereal *stld, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *du, doublereal *udot, doublereal *dfdu, doublereal *dfdp, doublereal *thl, doublereal *thu)
{
  integer nbif, ndim;

  integer i;

  /* Stores branching data in the following arrays : */
  /*        STU    ( the solution vector U ) */
  /*        STUD   ( U-dot ) */
  /*        STLA   ( PAR(ICP(1)) ) */
  /*        STLD  ( PAR(ICP(1))-dot ) */
  /* Here the vector ( PAR(ICP(1))-dot , U-dot ) lies in the 2-d nullspace 
   */
  /* at branch point and is perpendicular to the direction vector of */
  /* known branch at this point. */

  ndim = iap->ndim;
  perpendicular_null_direction(iap, icp, m1aaloc, aa, du, rldot, udot, dfdu, dfdp, thl, thu);

  nbif = iap->nbif;
  stld[-1 + nbif] = du[ndim];
  for (i = 0; i < ndim; ++i) {
    stu[(nbif - 1) * ndim + i] = u[i];
    stud[(nbif - 1) * ndim + i] = du[i];
  }
  stla[-1 + nbif] = rlcur[0];

  return 0;
} /* stbif_ */

int 
swpnt(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *rds, doublereal *stud, doublereal *stu, doublereal *stla, doublereal *stld, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *udot)
{

  integer nbif, ndim, mxbf, ipos, i, i1;
  doublereal ds;
  integer isw;

  /* This subroutine retrieves the branching data U, U-dot, PAR(ICP(1)), */
  /* PAR(ICP(1))-dot. If this initialization corresponds to the computation */
  /* of the bifurcating branch in opposite direction, then only the sign of */
  /*  the stepsize ( DS ) along the branch is reversed. */

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

int 
swprc(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), integer *m1aaloc, doublereal *aa, doublereal *rhs, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *du, doublereal *uold, doublereal *udot, doublereal *f, doublereal *dfdu, doublereal *dfdp, doublereal *rds, doublereal *thl, doublereal *thu)
{
  double pivot = NAN; /* the last linear solve's smallest pivot (xpp::ge) */
  // Internal switches can reuse BR/PT for a different branch point. Point
  // zero means no proven record; on failure refresh the restored Jacobian.
  xpp::autox_stability_step_start(iap->run->stability, iap->ibr, 0);

  integer aa_dim1;

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
    xpp::print(iap->run->fp9," Branch {:2} N={:5} IT={:2} PAR({:2})={:11.3E} U=",ibr,ntop,
	    nit,icp[0],rlcur[0]);	
    for (i = 0; i < ndmr; ++i) {
      xpp::print(iap->run->fp9,"{:11.3E}",u[i]);	
    }
    xpp::print(iap->run->fp9,"\n");	

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
    pivot = xpp::ge(iap->run->fp9, iap->iid, ndim + 1, *m1aaloc, aa, 1, ndim + 1, du,
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
      xpp::print(iap->run->fp9," Branch {:2} N={:5} IT={:2} PAR({:2})={:11.3E} U=",ibr,ntop,
	      nit+1,icp[0],rlcur[0]);	
      for (i = 0; i < ndmr; ++i) {
	xpp::print(iap->run->fp9,"{:11.3E}",u[i]);	
      }
      xpp::print(iap->run->fp9,"\n");	
    }

    /* Check whether relative error has reached user-supplied tolerance : 
     */

    rdrlm = f2c::abs(drlm) / (f2c::abs(rlcur[0]) + 1.);
    rdumx = dumx / (umx + 1.);
    if (rdrlm < epsl && rdumx < epsu) {
      /* the point's parameter is the last Newton update's, as solvae stores it (the evaluation above used the one before);
         W569: its Jacobian is the converged point's too, for the fold test cnrlae takes here (as solvae, W421) */
      par[icp[0]] = rlcur[0];
      (*funi)(iap, rap, ndim, u, uold, icp, par, 2, f, dfdu, dfdp);
      if (jacobian_failed(iap)) goto L5;
      return 0;
    }
    /* L3: */
  }

failed_step:
  /* Maximum number of iterations reached. Reduce stepsize and try again. */

  if (iads == 0) {
    xpp::print(iap->run->fp9,"{:4} {:5} NOTE:No convergence when switching branches with fixed step size\n",ibr,ntop);
  }
  if (iads == 0) {
    xpp::autox_stop_noconv(iap->run->stop, {xpp::AUTOX_STOP_NOCONV_SWITCH_FIXED, *rds, dsmin, itnw, epsl, epsu, pivot}); /* xppautX: T23 */
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
    xpp::print(iap->run->fp9,"{:4} {:5} NOTE:Retrying step\n",ibr,ntop);
  }
  goto L2;

/* Minimum stepsize reached. */

 L4:
  xpp::print(iap->run->fp9,"{:4} {:5} NOTE:No convergence when switching branches with minimum step size\n",ibr,ntop);
  xpp::autox_stop_noconv(iap->run->stop, {xpp::AUTOX_STOP_NOCONV_SWITCH_MIN, *rds, dsmin, itnw, epsl, epsu, pivot}); /* xppautX: T23 */
 L5:
  rlcur[0] = rlold[0];
  par[icp[0]] = rlcur[0];
  for (i = 0; i < ndim; ++i) {
    u[i] = uold[i];
  }
  const bool retained = xpp::autox_stability_step_failed(iap->run->stability,
      iap->ibr, iap->ntot + 1);
  istop = 1;
  iap->istop = istop;
  if (nonfinite && !stop_asked(iap))
    xpp::autox_nonfinite(*iap, iap->ntot + 1);

  // swpnt can return to an earlier branch point whose spectrum is no
  // longer the owner's last record. Recompute at the restored solution.
  if (!retained && xpp::problem_is_steady_or_map(iap->ips) && !stop_asked(iap)) {
    xpp::autox_stability_equilibrium_restored(*iap, *rap, par, icp, funi, u, uold, f, dfdu, dfdp);
  }
  return 0;
	
} /* swprc_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*                    Output (Algebraic Problems) */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

int 
sthd(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *thl, doublereal *thu)
{
  integer ndim, ncol, mxbf, nicp;
  doublereal epsl;
  integer nfpr, iplt, nint;
  doublereal epsu, epss;
  integer jtmp, itmx, itnw, nwtn, ntst, nuzr, i;
  doublereal dsmin, dsmax, a0, a1, ds, rl0, rl1;
  integer iad, jac, nbc, iid, ilp, isp, irs, npr, isw, nmx;
  xpp::AutoxProblem ips;

  /* Write the values of the user defined parameters on unit 7. */
  /* This identifying information is preceded by a '   0' on each line. */
  /* The first line in the file contains the (generally) user-supplied */
  /* limits of the bifurcation diagram, viz. RL0,RL1,A0 and A1. */
  /* These are often convenient for an initial plot of the diagram. */
        
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

  xpp::print(iap->run->fp7,"   0 {:12.4E}{:12.4E}{:12.4E}{:12.4E}\n",rl0,rl1,a0,a1);
  xpp::print(iap->run->fp7,"   0   EPSL={:11.4E}  EPSU ={:11.4E}  EPSS ={:11.4E}\n",epsl,epsu,epss);
  xpp::print(iap->run->fp7,"   0   DS  ={:11.4E}  DSMIN={:11.4E}  DSMAX={:11.4E}\n",ds,dsmin,dsmax);
  xpp::print(iap->run->fp7,"   0   NDIM={:4}   IPS ={:4}   IRS ={:4}   ILP ={:4}\n",ndim,xpp::autox_problem_code(ips),irs,ilp); 
  xpp::print(iap->run->fp7,"   0   NTST={:4}   NCOL={:4}   IAD ={:4}   ISP ={:4}\n",ntst,ncol,iad,isp);
  xpp::print(iap->run->fp7,"   0   ISW ={:4}   IPLT={:4}   NBC ={:4}   NINT={:4}\n",isw,iplt,nbc,nint);
  xpp::print(iap->run->fp7,"   0   NMX={:5}   NPR ={:4}   MXBF={:4}   IID ={:4}\n",nmx,npr,mxbf,iid);
  xpp::print(iap->run->fp7,"   0   ITMX={:4}   ITNW={:4}   NWTN={:4}   JAC={:4}  NUZR={:4}\n",itmx,itnw,nwtn,jac,nuzr);

  if (nicp == 1) {
    jtmp = iap->run->parameter_count;
    xpp::print(iap->run->fp7,"   0   User-specified parameter:       ");
    for (i = 0; i < nicp; ++i) {
      xpp::print(iap->run->fp7,"{:4}",icp[jtmp + i]);
    }
    xpp::print(iap->run->fp7,"\n");
  } else {
    jtmp = iap->run->parameter_count;
    xpp::print(iap->run->fp7,"   0   User-specified parameters:      ");
    for (i = 0; i < nicp; ++i) {
      xpp::print(iap->run->fp7,"{:4}",icp[jtmp + i]);
    }
    xpp::print(iap->run->fp7,"\n");
  }

  if (nfpr == 1) {
    xpp::print(iap->run->fp7,"   0   Active continuation parameter:  ");
	
    for (i = 0; i < nfpr; ++i) {
      xpp::print(iap->run->fp7,"{:4}",icp[i]);
    }
    xpp::print(iap->run->fp7,"\n");
  } else {
    xpp::print(iap->run->fp7,"   0   Active continuation parameters:  ");
    for (i = 0; i < nfpr; ++i) {
      xpp::print(iap->run->fp7,"{:4}",icp[i]);
    }
    xpp::print(iap->run->fp7,"\n");
  }

  return 0;
} /* sthd_ */

int 
headng(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer iunit, integer *n1, integer *n2)
{
  integer iplt, i, j;
  /* The fixed-width column headings printed below. */
  std::array<std::string, autox_heading_columns> col;
  integer ndm;
  xpp::AutoxProblem ips;

  /* Prints headings above columns on unit 6 and 7. */

  ips = iap->ips;
  iplt = iap->iplt;
  ndm = iap->ndm;

  /*initialize strings*/
  for (i = 0; i < autox_heading_columns; ++i) {
    col[i].assign(autox_column_width, ' ');
  }

  if (iunit == 6) {
    xpp::log_autox(" \n");
  }
  if (iunit == 7) {
    xpp::print(iap->run->fp7,"   0\n");
  }
  if (iunit == 9) {
    xpp::print(iap->run->fp9," \n");	
  }

  j = 0;
  for (i = 0; i < *n1; ++i) {
    ++j;
    if (static_cast<double>(j) == 2.) {
      j = j + 1 + *n2;
    }   
    if (icp[i] >= xpp::autox_unknown::period) {
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
    if (xpp::problem_is_algebraic_family(ips) || ips == xpp::AutoxProblem::AlgebraicOptimization) {
      col[1] = xpp::format("     U({})     ",-iplt);
    } else {
      col[1] = xpp::format("   MAX U({})   ",iplt);
    }
  } else if (iplt < 0 && iplt >= -ndm) {
    if (xpp::problem_is_algebraic_family(ips) || ips == xpp::AutoxProblem::AlgebraicOptimization) {
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
    if (col[i] == xpp::format("   PAR({})    ",xpp::autox_unknown::period) && xpp::problem_heads_period(ips)) {
      col[i] = "    PERIOD    ";
    } else if (col[i] == xpp::format("   PAR({})    ",xpp::autox_unknown::objective) && xpp::problem_is_optimization(ips)) {
      col[i] = "     FOPT     ";
    } else if (col[i] == xpp::format("   PAR({})    ",xpp::autox_unknown::evolution_time) && xpp::problem_is_parabolic_evolution(ips)) {
      col[i] = "     TIME     ";
    }
  }

  if (iunit == 6) {
    xpp::log_autox("  BR    PT  TY LAB ");
    for (i = 0; i < *n1 + *n2 + 1; ++i) {
      /* PAR(n)/U(n) as the user named them */
      xpp::log_autox("{}",autox_screen_col(*iap->run->session, col[i]).c_str());
    }
    xpp::log_autox("\n");
  } else if (iunit == 7) {
    xpp::print(iap->run->fp7,"   0    PT  TY LAB ");
    for (i = 0; i < *n1 + *n2 + 1; ++i) {
      xpp::print(iap->run->fp7,"{}",col[i]);
    }
    xpp::print(iap->run->fp7,"\n");
  } else if (iunit == 9) {
    xpp::print(iap->run->fp9,"  BR    PT  TY LAB ");	
    for (i = 0; i < *n1 + *n2 + 1; ++i) {
      xpp::print(iap->run->fp9,"{}",col[i]);
    }
    xpp::print(iap->run->fp9,"\n");	
  }

  return 0;
} /* headng_ */

int 
stplae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *rlcur, doublereal *u, const doublereal *udot, const doublereal *rldot)
{

  integer labw, nins, ntot, i;

  integer istop, ntots;
  doublereal ss;

  integer iab, lab;
  doublereal amp;

  int iflag=0;
  xpp::AutoxStopAt stop_at = {0}; /* xppautX: T23 */

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

  auto context = point_output_context(*iap, *rap);

  ntot = iap->ntot;
  ++ntot;
  iap->ntot = ntot;

  pvlsae(iap, rap, u, par);
  xpp::autox_branch_stored(*iap, ntot, rap->epss, par, icp, std::span<const doublereal>(u, static_cast<std::size_t>(iap->ndim)), false);

/* ITP is set to 4 every NPR steps along a branch, and the entire */
/* solution is written on unit 8. */

  if (context.npr != 0) {
    if (ntot % context.npr == 0 && context.itp.kind == xpp::AutoxPointKind::Regular) {
      context.itp = xpp::AutoxPointCode{xpp::AutoxPointKind::Output, context.itpst};
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
  autox_checkpoint(*iap->run,&iflag);
  istop = iap->istop;
  /* xppautX: cancel: a point the solve reached is stored as it is; the next
     solve sees the cancel and ends the branch with an EP repeating it, so a
     cancel always ends the same way, wherever it came (xpp_job.h, replay) */
  if (istop == 0 && autox_cancelled(*iap->run)) iflag = 0;
  stop_at.user = iflag == 1 || istop == 1; /* xppautX: T23, unless MX or UZR below */
  if (istop == 1 && !autox_cancelled(*iap->run)) { /* xppautX: cancel: EP, not MX */
    /*        Maximum number of iterations reached somewhere. */
    context.itp = xpp::AutoxPointCode{xpp::AutoxPointKind::MaxSteps, context.itpst};
    iap->itp = context.itp;
    stop_at.noconv = 1, stop_at.user = 0;
  } else if (istop == -1) {
    /*        ** UZR endpoint */
    context.itp = xpp::AutoxPointCode{xpp::AutoxPointKind::End, context.itpst};
    iap->itp = context.itp;
  } else {
    if (rlcur[0] < context.rl0 || rlcur[0] > context.rl1 || amp < context.a0 || amp > context.a1 || ntot
	== context.nmx|| iflag==1) {
      istop = 1;
      iap->istop = istop;
      /* xppautX (W495): a special point the last step located keeps its
         label; the branch still ends there, its stop note says which bound
         (AUTO-07p labels it EP: auto07p-findings 9) */
      if (context.itp.kind == xpp::AutoxPointKind::Regular || context.itp.kind == xpp::AutoxPointKind::Output)
        context.itp = xpp::AutoxPointCode{xpp::AutoxPointKind::End, context.itpst};
      iap->itp = context.itp;
    }
  }

  /* xppautX (W604): a branch that came back to its start ends there (autox_ends.h) */
  end_closed_branch(*iap, context.itp, stop_at);
  istop = iap->istop;

  labw = 0;
  if (context.itp.kind != xpp::AutoxPointKind::Regular) {
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
  wrline(iap, rap, par, icp, &icp[iap->run->parameter_count], &context.ibr, &ntots,
	 &labw, &amp, u);
  if (iap->istop != 0) { /* xppautX: why the branch ended (autox_stop.h, T23) */
    stop_at.br = context.ibr, stop_at.pt = ntot, stop_at.ipar = icp[0], stop_at.par = rlcur[0], stop_at.norm = amp;
    stop_at.rl0 = context.rl0, stop_at.rl1 = context.rl1, stop_at.a0 = context.a0, stop_at.a1 = context.a1, stop_at.nmx = context.nmx;
    stop_at.mark = iap->istop == -1;
    if (stop_at.mark) stop_at.user = 0;
    xpp::autox_stop_branch_end(*iap->run, &stop_at);
    iap->run->stop.closure_gap.reset();
  }

  /* Write restart information for multi-parameter analysis : */

  if (labw != 0) {
    wrtsp8(iap, rap, par, icp, &labw, rlcur, u, udot, rldot);
  }

  return 0;
} /* stplae_ */

int 
wrline(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *icu, integer *ibr, integer *ntot, integer *lab, doublereal *vaxis, doublereal *u)
{

  integer nicp, i;

  integer n1, n2;

  integer nt, ndm, lb;
  xpp::AutoxPointCode itp;
  lb=*lab;
  if(iap->run->restart_flag&&(lb!=0)){
    
    iap->run->restart_flag=false;
    iap->run->restart_label=*lab;
  }

/* Write one line of output on unit 6 and 7. */

  ndm = iap->ndm;
  itp = iap->itp;
  nicp = iap->nicp;

  n1 = nicp;
  n2 = ndm;
  nt = n1 + n2;

  if (n1 > autox_diagram_columns) {
    n1 = autox_diagram_columns;
    n2 = 0;
  } else if (nt > autox_diagram_columns) {
    n2 = autox_diagram_columns - n1;
  }

  /* Write a heading above the first line. */

  if (f2c::abs(*ntot) == 1) {
    headng(iap, rap, par, icu, 6, &n1, &n2);
  }
  if (f2c::abs(*ntot) == 1) {
    headng(iap, rap, par, icu, 7, &n1, &n2);
  }
  headng(iap, rap, par, icu, 9, &n1, &n2);

  const std::string_view atype = xpp::autox_point_kind_mark(itp.kind); /* the type's mark: autoX's table and fort.9 */

  if (n2 == 0) {
    if (itp.kind != xpp::AutoxPointKind::Regular) {
      xpp::log_autox("{}",xpp::autox_point_row_key(*ibr,*ntot,atype,*lab));
      xpp::log_autox("{:14.6E}",par[icu[0]]);
      xpp::log_autox("{:14.6E}",(*vaxis));
      for (i = 1; i < n1; ++i) {
	xpp::log_autox("{:14.6E}",par[icu[i]]);
      }
      xpp::log_autox("\n");
    }
    xpp::print(iap->run->fp7,"{}",xpp::autox_branch_row_key(*ibr,*ntot,xpp::autox_point_encode(itp),*lab));
    xpp::print(iap->run->fp7,"{:14.5E}",par[icu[0]]);
    xpp::print(iap->run->fp7,"{:14.5E}",(*vaxis));
    for (i = 1; i < n1; ++i) {
      xpp::print(iap->run->fp7,"{:14.5E}",par[icu[i]]);
    }
    xpp::print(iap->run->fp7,"\n");
    xpp::print(iap->run->fp9,"{}",xpp::autox_point_row_key(*ibr,*ntot,atype,*lab));
    xpp::print(iap->run->fp9,"{:14.6E}",par[icu[0]]);
    xpp::print(iap->run->fp9,"{:14.6E}",(*vaxis));
    for (i = 1; i < n1; ++i) {
      xpp::print(iap->run->fp9,"{:14.6E}",par[icu[i]]);
    }
    xpp::print(iap->run->fp9,"\n");
  } else {
    if (n1 == 1) {
      if (itp.kind != xpp::AutoxPointKind::Regular) {
	xpp::log_autox("{}",xpp::autox_point_row_key(f2c::abs(*ibr),f2c::abs(*ntot),atype,*lab));
	xpp::log_autox("{:14.6E}",par[icu[0]]);
	xpp::log_autox("{:14.6E}",(*vaxis));
	for (i = 0; i < n2; ++i) {
	  xpp::log_autox("{:14.6E}",u[i]);
	}
	xpp::log_autox("\n");
      }
      xpp::print(iap->run->fp7,"{}",xpp::autox_branch_row_key(*ibr,*ntot,xpp::autox_point_encode(itp),*lab));
      xpp::print(iap->run->fp7,"{:14.5E}",par[icu[0]]);
      xpp::print(iap->run->fp7,"{:14.5E}",(*vaxis));
      for (i = 0; i < n2; ++i) {
	xpp::print(iap->run->fp7,"{:14.5E}",u[i]);
      }
      xpp::print(iap->run->fp7,"\n");
      xpp::print(iap->run->fp9,"{}",xpp::autox_point_row_key(*ibr,*ntot,atype,*lab));
      xpp::print(iap->run->fp9,"{:14.6E}",par[icu[0]]);
      xpp::print(iap->run->fp9,"{:14.6E}",(*vaxis));
      for (i = 0; i < n2; ++i) {
	xpp::print(iap->run->fp9,"{:14.6E}",u[i]);
      }
      xpp::print(iap->run->fp9,"\n");	

    } else {
      if (itp.kind != xpp::AutoxPointKind::Regular) {
	xpp::log_autox("{}",xpp::autox_point_row_key(f2c::abs(*ibr),f2c::abs(*ntot),atype,*lab));
	xpp::log_autox("{:14.6E}",par[icu[0]]);
	xpp::log_autox("{:14.6E}",(*vaxis));
	for (i = 0; i < n2; ++i) {
	  xpp::log_autox("{:14.6E}",u[i]);
	}
	for (i = 1; i < n1; ++i) {
	  xpp::log_autox("{:14.6E}",par[icu[i]]);
	}
	xpp::log_autox("\n");
      }
      xpp::print(iap->run->fp7,"{}",xpp::autox_branch_row_key(*ibr,*ntot,xpp::autox_point_encode(itp),*lab));
      xpp::print(iap->run->fp7,"{:14.5E}",par[icu[0]]);
      xpp::print(iap->run->fp7,"{:14.5E}",(*vaxis));
      for (i = 0; i < n2; ++i) {
	xpp::print(iap->run->fp7,"{:14.5E}",u[i]);
      }
      for (i = 1; i < n1; ++i) {
	xpp::print(iap->run->fp7,"{:14.5E}",par[icu[i]]);
      }
      xpp::print(iap->run->fp7,"\n");
      xpp::print(iap->run->fp9,"{}",xpp::autox_point_row_key(*ibr,*ntot,atype,*lab));
      xpp::print(iap->run->fp9,"{:14.6E}",par[icu[0]]);
      xpp::print(iap->run->fp9,"{:14.6E}",(*vaxis));
      for (i = 0; i < n2; ++i) {
	xpp::print(iap->run->fp9,"{:14.6E}",u[i]);
      }
      for (i = 1; i < n1; ++i) {
	xpp::print(iap->run->fp9,"{:14.6E}",par[icu[i]]);
      }
      xpp::print(iap->run->fp9,"\n");	
    }
  }

  return 0;
} /* wrline_ */

/* Values in autoX's solution-file rows: autox_solution_columns to a row, each row indented. */
static void print_solution_row_values(FILE *fp, std::span<const doublereal> values)
{
  xpp::print(fp,"    ");
  for (std::size_t i = 0; i < values.size(); ++i) {
    if ((i > 0) && (i % autox_solution_columns == 0))
      xpp::print(fp,"\n    ");
    xpp::print(fp,"{:19.10E}",values[i]);
  }
  xpp::print(fp,"\n");
}

static integer solution_row_count(integer values)
{
  return (values - 1) / autox_solution_columns + 1;
}

int 
wrtsp8(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *lab, doublereal *rlcur, doublereal *u, const doublereal *udot, const doublereal *rldot)
{
  integer ndim, nfpr, ntpl, jtmp, ntot, i;
  doublereal t;
  integer nrowpr, ibr;
  integer nar, isw;
  xpp::AutoxPointCode itp;

  FILE *const fp8=fort8(*iap->run);

  /* Write restart information on singular points, plotting points, etc., */
  /* on unit 8. */

  ndim = iap->ndim;
  isw = iap->isw;
  itp = iap->itp;
  ibr = iap->ibr;
  nfpr = iap->nfpr;
  ntot = iap->ntot;

  ntpl = 1;
  nar = ndim + 1;
  jtmp = iap->run->parameter_count;
  nrowpr = ndim / autox_solution_columns + 1 + (jtmp - 1) / autox_solution_columns + 1;
  /* A branch point keeps the direction of its branch (AUTO-07p's NTST=1 record: the
     free parameter indices, PAR-dot, U-dot): a restart that switches branches needs it. */
  const bool direction = itp.kind == xpp::AutoxPointKind::BranchPoint && f2c::abs(isw) == 1;
  if (direction)
    nrowpr += (nfpr - 1) / autox_solution_parameter_indices_per_line + 1 + solution_row_count(nfpr) + solution_row_count(ndim);
  par[icp[0]] = rlcur[0];
  t = 0.;

  {
    xpp::AutoxSolutionHeader header;
    header.ibr=ibr; header.ntot=ntot; header.itp=itp; header.lab=*lab; header.nfpr=nfpr; header.isw=isw;
    header.ntpl=ntpl; header.nar=nar; header.nskip=nrowpr; header.ntst=direction ? 1 : 0; header.ncol=0;
    header.npar=iap->run->parameter_count;
    xpp::print(fp8,"{}",xpp::autox_solution_header_text(header));
  }
  xpp::print(fp8,"    {:19.10E}",t);
  for (i = 0; i < ndim; ++i) {
    if((i>0)&&((i+1)%autox_solution_columns==0))
      xpp::print(fp8,"\n    ");
    xpp::print(fp8,"{:19.10E}",u[i]);
  }
  xpp::print(fp8,"\n");
  if (direction) {
    for (i = 0; i < nfpr; ++i)
      xpp::print(fp8,"{:5}",icp[i]);
    xpp::print(fp8,"\n");
    print_solution_row_values(fp8, {rldot, static_cast<std::size_t>(nfpr)});
    print_solution_row_values(fp8, {udot, static_cast<std::size_t>(ndim)});
  }
  print_solution_row_values(fp8, {par, static_cast<std::size_t>(jtmp)});
  fflush(fp8);

  return 0;
} /* wrtsp8_ */

int 
wrjac(iap_type *iap, integer *n, integer *m1aaloc, doublereal *aa, doublereal *rhs)
{
  integer aa_dim1;

  integer i, j;

  aa_dim1 = *m1aaloc;
    
  xpp::print(iap->run->fp9," Residual vector :\n");	

  for (i = 0; i < *n; ++i) {
    xpp::print(iap->run->fp9," {:10.3E}",rhs[i]);	
  }
  xpp::print(iap->run->fp9,"\n");	
  xpp::print(iap->run->fp9," Jacobian matrix :\n");	
  for (i = 0; i < *n; ++i) {
    for (j = 0; j < *n; ++j) {
      xpp::print(iap->run->fp9," {:10.3E}",ARRAY2D(aa, i, j));	
    }
    xpp::print(iap->run->fp9,"\n");	

  }

  return 0;
} /* wrjac_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*                    Mesh and Weight Generation */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

int 
msh(const iap_type *iap, const rap_type *rap, doublereal *tm)
{

  integer ntst, j;
  doublereal dt;

/* Generates a uniform mesh on [0,1]. */

  ntst = iap->ntst;

  tm[0] = 0.;
  dt = 1. / ntst;
  for (j = 0; j < ntst; ++j) {
    tm[j + 1] = (j + 1) * dt;
  }

  return 0;
} /* msh_ */

int 
genwts(const integer ncol, const integer n1, doublereal *wt, doublereal *wp)
{
  integer wt_dim1, wp_dim1;

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

/* Generate the collocation points : */
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

int 
cpnts(const integer ncol, doublereal *zm)
{
  doublereal c, r, c1, c2, c3;

  /* Generates the collocation points with respect to [0,1]. */
  if (ncol > 7) {
    xpp::autox_fail(xpp::format("collocation_points is {}: autoX has collocation weights for at most 7; set collocation_points (Numerics) to 7 or smaller", ncol));
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

int 
cntdif(integer *n, doublereal *d)
{

  integer i, k, k1;
  doublereal sc;

  /* Generates the coefficients of the central difference formula for */
  /* Nth derivative at uniformly spaced points */
  /*              0 = x  < x  < ... < x  = 1. */
  /*                   0    1          N */

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

  integer n_to_n = 1; /* n^n, exact */
  for (i = 0; i < *n; ++i) n_to_n *= *n;
  sc = static_cast<doublereal>(n_to_n);
  for (i = 0; i < *n + 1; ++i) {
    d[i] = sc * d[i];
  }

  return 0;
} /* cntdif_ */

int 
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

int 
adptds(iap_type *iap, rap_type *rap, doublereal *rds)
{
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

  xpp::print(iap->run->fp9,"{:4} {:5}        Iterations     {:3}\n",f2c::abs(ibr),ntop - 1,nit);
  xpp::print(iap->run->fp9,"{:4} {:5}        Stepsize      {:14.6E}\n",f2c::abs(ibr),ntop,(*rds));

  return 0;
} /* adptds_ */

int 
adapt(iap_type *iap, rap_type *rap, integer *nold, integer *ncold, integer *nnew, integer *ncnew, doublereal *tm, doublereal *dtm, integer *ndxloc, doublereal *ups, doublereal *vps)
{
  integer ups_dim1, vps_dim1;

  integer ndim, iper;
  integer i, j, noldp1, nnewp1;

  integer nrwnew;
  integer isw;
  xpp::AutoxProblem ips;

  std::vector<doublereal> uint(xpp::size_product(*ndxloc, xpp::size_product(iap->ndim, iap->ncol)));
  std::vector<doublereal> tint(*ndxloc);
  std::vector<doublereal> tm2(*ndxloc);
  std::vector<integer> itm(*ndxloc);

  /* Adapts the distribution of the mesh points so that the increase of the */
  /* monotone function EQDF becomes approximately equidistributed over the */
  /* intervals. The functions UPS and VPS are interpolated on new mesh. */

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

  if ((ips == xpp::AutoxProblem::Periodic || ips == xpp::AutoxProblem::WavePeriodic)
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

int 
interp(iap_type *iap, rap_type *rap, integer *ndim, integer *n, integer *nc, doublereal *tm, integer *ndxloc, doublereal *ups, integer *n1, integer *nc1, doublereal *tm1, doublereal *ups1, doublereal *tm2, integer *itm1)
{
  integer ups_dim1, ups1_dim1;

  doublereal d;
  integer i, j, k, l;
  doublereal z__;
  integer j1, k1, l1;
  doublereal ri;

  integer n1m1, ncp1;

  std::vector<doublereal> w(xpp::size_sum(*nc, 1));
  std::vector<doublereal> x(xpp::size_sum(*nc, 1));

/* Finds interpolant (TM(.) , UPS(.) ) on new mesh TM1. */

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

int 
newmsh(iap_type *iap, rap_type *rap, integer *ndxloc, doublereal *ups, integer *nold, integer *ncold, doublereal *tmold, doublereal *dtmold, integer *nnew, doublereal *tmnew, integer *iper)
{

  integer ndim;

  integer j;
  integer j1, noldp1, nnewp1;
  doublereal dal;

  std::vector<doublereal> uneq(xpp::size_sum(*nnew, 1));
  std::vector<doublereal> eqf(xpp::size_sum(*nold, 1));
  std::vector<doublereal> density(*nold);
  std::vector<integer> ial(xpp::size_sum(*nnew, 1));

  /* Redistributes the mesh according to the function EQDF. */

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

int 
ordr(iap_type *iap, rap_type *rap, integer *n, doublereal *tm, integer *n1, doublereal *tm1, integer *itm1)
{

  integer j, k0, j1, k1=0;

  /* TM and TM1 are two ascending arrays with values in [0,1]. On exit the 
*/
/* value of ITM1( i ) specifies the index of the TM-interval in which */
/* TM1(i) lies. */

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

int 
intwts(iap_type *iap, rap_type *rap, integer *n, doublereal *z__, doublereal *x, doublereal *wts)
{

  integer k;
  doublereal p, denom;
  integer ib;

  /* Generates weights for Lagrange interpolation. */

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

int 
eqdf(iap_type *iap, rap_type *rap, integer *ntst, integer *ndim, integer *ncol, doublereal *dtm, integer *ndxloc, doublereal *ups, doublereal *eqf, std::span<doublereal> density, integer *iper)
{
  integer ups_dim1;

  doublereal dtav, e;
  integer i, j, k;
  bool small;
  integer k1;
  doublereal sc;

  integer jp1;
  doublereal pwr;

  std::vector<doublereal> hd(xpp::size_product(xpp::size_sum(*ntst, 1), xpp::size_product(*ndim, *ncol)));
  std::vector<doublereal> wh(xpp::size_sum(*ncol, 1));

  /* Compute approximation to NCOL-th derivative : */
  ups_dim1 = *ndxloc;

  cntdif(ncol, wh.data());

  small = true;
  for (j = 0; j < *ntst; ++j) {
    jp1 = j + 1;
    sc = 1. / power_by_squaring(dtm[j], *ncol);
    for (i = 0; i < *ndim; ++i) {
      hd[j + i * (*ntst + 1)] = wh[*ncol] * ARRAY2D(ups, jp1, i);
      for (k = 0; k < *ncol; ++k) {
	k1 = i + k * *ndim;
	hd[j + i * (*ntst + 1)] += wh[k] * ARRAY2D(ups, j, k1);
      }
      hd[j + i * (*ntst + 1)] = sc * hd[j + i * (*ntst + 1)];
      if (f2c::abs(hd[j + i * (*ntst + 1)]) > autox_hmach) {
	small = false;
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
      e += xpp::math::pow(tmp, pwr);
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

int 
nrmlz(integer *ndim, doublereal *v)
{
  doublereal c;
  integer i;
  doublereal ss;

  /* Scale the vector V so that its discrete L2-norm becomes 1. */

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

doublereal 
pi(doublereal r)
{
  doublereal ret_val;

  ret_val = r * 4. * xpp::math::atan(1.);

  return ret_val;
} /* pi */

int 
newlab(iap_type *iap, rap_type *rap)
{

  integer mlab;

  integer lab, ibr, mbr, irs, isw;
  xpp::AutoxProblem ips;
  xpp::AutoxPointCode itp;

  /* Determine a suitable label when restarting. */

  ips = iap->ips;
  irs = iap->irs;
  isw = iap->isw;
  itp = iap->itp;

  mbr = 0;
  mlab = 0;
  /* a job's fort.3 holds only the point it restarts from, numbered as the job that found it numbers (autox_jobs.cpp):
     a job numbers from the diagram's, as every job does */
  if (!iap->run->job) {
    const xpp::AutoxSolutionIndex &solutions = xpp::autox_run_solutions(*iap->run);
    mbr = static_cast<integer>(solutions.max_branch);
    mlab = static_cast<integer>(solutions.max_label);
  }

  /* A fresh run has an empty fort.3, but the diagram may retain older
     branches. Their labels must still identify their own restart solution. */
  for(const auto &point:iap->run->session->diagram.points){
    mbr=std::max(mbr,static_cast<integer>(f2c::abs(point.ibr)));
    mlab=std::max(mlab,static_cast<integer>(point.lab));
  }
  lab = mlab;
  iap->lab = lab;
  iap->run->base_branch = mbr;
  iap->run->base_label = mlab;
  if (isw < 0 || irs == 0 || iap->run->job) { /* a job's branch is the next number in its own numbering, never a label of the job it restarts from */
    ibr = mbr + 1;
    iap->ibr = ibr;
  } else if ((itp.start == xpp::AutoxPointKind::Regular && f2c::abs(isw) == 2) || (ips == xpp::AutoxProblem::Periodic && itp.is(xpp::AutoxPointKind::Hopf)) || 
	     (ips == xpp::AutoxProblem::BoundaryValue && isw == 2 && itp.start == xpp::AutoxPointKind::Regular) || (ips == xpp::AutoxProblem::AlgebraicOptimization && itp.kind == xpp::AutoxPointKind::Fold)) {
    ibr = irs;
    iap->ibr = ibr;
  }

  return 0;
} /* newlab_ */

void autox_restart_label_missing(const iap_type *iap)
{
  xpp::autox_fail(xpp::format("autoX: restart label {} is not in {}", iap->irs, xpp::autox_fort_path(*iap->run, 3)));
}

void autox_restart_read(const iap_type *iap, bool ok, std::string_view what)
{
  if (!ok)
    xpp::autox_fail(xpp::format("autoX: the restart data of label {} in {} ends before {}", iap->irs,
                               xpp::autox_fort_path(*iap->run, 3), what));
}

void read_autox_restart_header(iap_type *iap, const rap_type *rap,
                              xpp::TokenReader &reader, xpp::AutoxSolutionHeader &header,
                              integer *ntst, integer *ncol) {
  integer free;
  bool found;
  findlb(iap, rap, iap->irs, &free, &found);
  if (!found) autox_restart_label_missing(iap);
  reader = xpp::TokenReader::attach(iap->run->fp3);
  autox_restart_read(iap, xpp::read_autox_solution_header(reader, header, iap->run->parameter_count), "its header");
  if (header.equilibrium_direction())
    xpp::autox_fail(xpp::format("autoX: restart label {} in {} is an equilibrium's branch point, not a mesh of a periodic solution or a boundary value problem", iap->irs,
                               xpp::autox_fort_path(*iap->run, 3)));
  *ntst = header.ntst;
  *ncol = header.ncol;
  iap->ibr = header.ibr;
}

int 
findlb(iap_type *iap, const rap_type *rap, 
       integer irs, integer *nfpr, bool *found)
{
  /* Locates restart point with label IRS and determines type. */
  /* If the label can not be located on unit 3 then FOUND will be .FALSE. */

  *found = false;
  const xpp::AutoxSolutionIndex &solutions = xpp::autox_run_solutions(*iap->run);
  const std::string path = xpp::autox_fort_path(*iap->run, 3);
  /* the records before the label set what a search that reads them sets: the last one's, when the label is not there */
  for (const xpp::AutoxSolutionRecord &record : solutions.records) {
    *nfpr = static_cast<integer>(record.nfpr);
    iap->itp = record.itp;
    iap->ibr = static_cast<integer>(record.ibr);
    if (record.lab != irs) continue;
    *found = true;
    iap->itpst = f2c::abs(iap->isw) == 2 ? xpp::autox_point_start_of(record.itp) : xpp::AutoxPointKind::Regular;
    /* readlb() reads the label line again from its start */
    if (auto r=xpp::seek_stream(iap->run->fp3, record.position, path); !r)
      xpp::autox_fail(r.error());
    return 0;
  }
  if (auto r=xpp::seek_stream(iap->run->fp3, solutions.bytes, path); !r) /* readlb() finds the end of the file, as after the search */
    xpp::autox_fail(r.error());
  return 0;
} 

/* Reads the restart data of an algebraic problem from the label line
   findlb() found (read again from its start); a short read ends the run. */
void
readlb(const iap_type *iap, const rap_type *rap, doublereal *u, doublereal *par)
{
  xpp::TokenReader tr = xpp::TokenReader::attach(iap->run->fp3);
  xpp::AutoxSolutionHeader header;
  autox_restart_read(iap, xpp::read_autox_solution_header(tr, header, iap->run->parameter_count), "its header");
  const integer ndim = header.nar - 1;
  if (ndim > iap->ndim)
    xpp::autox_fail(xpp::format("autoX: the restart data of label {} in {} has {} variables, the model {}", iap->irs,
                               xpp::autox_fort_path(*iap->run, 3), ndim, iap->ndim));
  doublereal time;
  autox_restart_read(iap, xpp::read_autox_solution_rows(tr, ndim, 0, 0, 1, 0, ndim, u, &time, false, 0, {}),
                    "the solution");
  iap->run->restart_direction.reset();
  if (header.equilibrium_direction()) {
    /* the branch's direction, which wrtsp8 keeps at a branch point: the free parameter indices, PAR-dot, U-dot */
    AutoxRestartDirection direction;
    for (long i = 0; i < header.nfpr; ++i) {
      int index;
      autox_restart_read(iap, tr.read(index), "the free parameter indices");
      if (i == 0) direction.parameter = index;
    }
    for (long i = 0; i < header.nfpr; ++i) {
      double rldot;
      autox_restart_read(iap, tr.read(rldot), "the parameter derivatives");
      if (i == 0) direction.rldot = rldot;
    }
    direction.udot.resize(static_cast<std::size_t>(ndim));
    for (double &value : direction.udot)
      autox_restart_read(iap, tr.read(value), "the arclength derivatives");
    iap->run->restart_direction = std::move(direction);
  }
  autox_restart_read(iap, xpp::read_autox_solution_parameters(tr, header, *iap->run, par), "the parameter values");
} /* readlb_ */

doublereal 
rinpr(iap_type *iap, const integer *ndim1, const integer *ndxloc, doublereal *ups, doublereal *vps, doublereal *dtm, doublereal *thu)
{
  integer ups_dim1, vps_dim1;
  doublereal ret_val;

  integer i, j, k;
  doublereal s;
  integer k1;
  doublereal sj;
  integer jp1;

  /* Computes the L2 inner product of UPS and VPS. */
  /* (Using the first NDIM1 components only.) */

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

doublereal 
rnrmsq(iap_type *iap, integer *ndim1, integer *ndxloc, doublereal *ups, doublereal *dtm, doublereal *thu)
{
  doublereal ret_val;

/* Finds the norm of UPS (first NDIM1 components are included only). */

  ret_val = rinpr(iap, ndim1, ndxloc, ups, ups, dtm, thu);

  return ret_val;
} /* rnrmsq_ */

doublereal 
rintg(iap_type *iap, integer *ndxloc, integer ic, doublereal *ups, doublereal *dtm)
{
  integer ups_dim1;
  doublereal ret_val;

  integer j, k;
  doublereal s;
  integer k1;
  doublereal sj;
  integer jp1;

  /* Computes the integral of the IC'th component of UPS. */

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

doublereal 
rnrm2(iap_type *iap, integer *ndxloc, integer *ic, doublereal *ups, doublereal *dtm)
{
  integer ups_dim1;
  doublereal ret_val;

  integer j, k;
  doublereal s;
  integer k1;
  doublereal sj;
  integer jp1;

  /* Computes the L2-norm of the IC'th component of UPS. */

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

doublereal 
rmxups(iap_type *iap, integer *ndxloc, integer *i, doublereal *ups)
{
  integer ups_dim1;
  doublereal ret_val;

  integer ndim, ncol, ntst, j, k, k1;

  /* Computes the maximum of the I'th component of UPS. */

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

doublereal 
rmnups(iap_type *iap, integer *ndxloc, integer *i, doublereal *ups)
{
  integer ups_dim1;
  doublereal ret_val;

  integer ndim, ncol, ntst, j, k, k1;

  /* Computes the minimum of the I'th component of UPS. */

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

int 
scaleb(iap_type *iap, integer *icp, integer *ndxloc, doublereal *dvps, doublereal *rld, doublereal *dtm, doublereal *thl, doublereal *thu)
{
  integer dvps_dim1;

  integer ndim, ncol, nfpr, nrow, ntst, i, j;
  doublereal sc, ss;

/* Scales the vector (DVPS,RLD) so its norm becomes 1. */

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

int 
cnrlbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), STPNT_TYPE_BVP((*stpnt)), PVLI_TYPE_BVP((*pvli)), doublereal *thl, doublereal *thu, std::span<const AutoxMark> marks)
{
  integer iads, ndim, ncol;
  integer ntot, ntst, nuzr, i, j;
  doublereal dsold;
  integer nodir = 0;
  iap->run->hopf_start = stpnt == stpnps || stpnt == stpnwp;
  std::vector<doublereal> rlold(iap->run->parameter_count);
  std::vector<doublereal> rldot(iap->run->parameter_count);
  std::vector<doublereal> rlcur(iap->run->parameter_count);
  integer nitps, istop;
  xpp::AutoxPointKind itpst;
  doublereal ds;
  AutoxTestSample bp1;
  AutoxTestSample sp1;
  integer iad;
  integer ilp, isp, irs;
  xpp::AutoxProblem ips;
  doublereal rds;
  AutoxTestSample rlp;
  AutoxTestSample closing;
  integer isw;
  xpp::AutoxPointCode itp;
    
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
  if (iap->isw==2 && xpp::problem_has_orbit_multipliers(iap->ips)) iap->run->codim.prepare_cycle(iap->ndim,iap->ndm);
  std::vector<AutoxTestSample> uzr(iap->nuzr);

  /* INITIALIZE COMPUTATION OF BRANCH */

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
  sp1 = bp1 = rlp = closing = {};
  if (nuzr > 0) {
    for (i = 0; i < nuzr; ++i) {
      uzr[i] = {};
    }
  }
  nitps = 0;
  iap->nit = nitps;
  ntot = 0;
  iap->ntot = ntot;
  istop = 0;
  iap->istop = istop;

  using Tests = std::vector<LocationTest<decltype(&fnuzbv)>>;
  /* W604: whether a crossing of the start's parameter value is the start (autox_ends.h) */
  const std::function<bool()> closes = [&] {
    return xpp::autox_branch_closes(*iap, rap->epss, par, icp, orbit_extremes(*iap, ntst_plus_one, ups.data()));
  };
  const auto tests_at_point = [&] {
    Tests tests;
    for (integer index = 0; index < nuzr; ++index)
      tests.push_back({fnuzbv, uzr[index], xpp::AutoxPointKind::UserZero, index});
    if (ilp == 1) tests.push_back({fnlpbv, rlp, xpp::AutoxPointKind::CycleFold, {}});
    if (isp >= 2) tests.push_back({fnbpbv, bp1, xpp::AutoxPointKind::CycleBranch, {}});
    if ((isp == 1 || isp == 2) && xpp::problem_has_orbit_multipliers(ips))
      tests.push_back({fnspbv, sp1, xpp::AutoxPointKind::Pending, {}});
    tests.push_back({fnclosedbv, closing, xpp::AutoxPointKind::BranchClosed, {}, &closes});
    return tests;
  };
  const auto evaluate_test = [&](const auto &test) {
    bool changed;
    return test.function(iap, rap, par, icp, &changed, funi, bcni, icni,
        p0.data(), p1.data(), ev.data(), rlcur.data(), rlold.data(), rldot.data(),
        &ntst_plus_one, ups.data(), uoldps.data(), udotps.data(), upoldp.data(),
        fa.data(), fc.data(), dups.data(), tm.data(), dtm.data(), thl, thu, marks);
  };

  /* W572: the tests' values at the starting point, which the loop does not test: the step to the second point
     then compares two values and a zero between them is located. The point's Newton matrix is made and factored
     as the tests read it (as a Hopf start's is, in stepbv), with the direction stdrbv found; the direction, the
     multipliers, the stored stability and the determinant stay what they were, so no result changes where no
     zero lies. */
  const auto seed_start_tests = [&] {
    const auto direction = rldot;
    const auto tangent_ps = udotps;
    const auto multipliers = ev;
    const auto stability = iap->run->stability.computed;
    const integer saved_isp = iap->isp, saved_nins = iap->nins;
    const doublereal saved_det = rap->det;
    const auto homcont = iap->run->homcont; // a homoclinic run's boundary evaluations advance its projection state
    integer refactor = 1, tangent = -1;
    doublereal no_step = 0.;
    solvbv(&refactor, iap, rap, par, icp, funi, bcni, icni, &no_step, &tangent,
           rlcur.data(), rlold.data(), rldot.data(), &ntst_plus_one, ups.data(), dups.data(), uoldps.data(),
           udotps.data(), upoldp.data(), dtm.data(), fa.data(), fc.data(), p0.data(), p1.data(), thl, thu);
    if (jacobian_failed(iap)) return;
    auto tests = tests_at_point();
    sample_tests(*iap, tests, tests.size(), evaluate_test);
    rldot = direction;
    udotps = tangent_ps;
    ev = multipliers;
    iap->run->stability.computed = stability;
    iap->isp = saved_isp;
    iap->nins = saved_nins;
    rap->det = saved_det;
    iap->run->homcont = homcont;
  };

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
  
  for(j = 0; j < iap->run->parameter_count; ++j) {
    rldot[j] = 0.0;
    rlcur[j] = 0.0;
    rlold[j] = 0.0;
  }

  rsptbv(iap, rap, par, icp, funi, stpnt, &rds, rlcur.data(), 
	 rlold.data(), rldot.data(), &ntst_plus_one, ups.data(), uoldps.data(), udotps.data(), upoldp.data(), dups.data(), tm.data(), 
	 dtm.data(), ev.data(), &nodir, thl, thu);
  const xpp::AutoxPointKind start_kind = iap->itp.kind; // what the restart label was (stpnt read it)
  xpp::autox_check_start_parameters(*iap,*rap,par,icp);
  xpp::autox_check_start_norm(*iap,*rap,sqrt(rnrmsq(iap,&iap->ndm,&ntst_plus_one,ups.data(),dtm.data(),thu)));
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
    itp = xpp::AutoxPointCode{xpp::AutoxPointKind::End, itpst};
  } else {
    itp = xpp::AutoxPointCode{};
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

  /* W572: a Hopf start is no solution of the cycle's equations: its first step is not tested */

  if (!iap->run->hopf_start && start_is_tested(*iap, start_kind)) {
    seed_start_tests();
  }

  extrbv(iap, rap, funi, &rds, rlcur.data(), rlold.data(), rldot.data(), &ntst_plus_one, ups.data(),
	 uoldps.data(), udotps.data());

  itp = xpp::AutoxPointCode{};
  iap->itp = itp;
  goto L2;

 L1:
  itp = xpp::AutoxPointCode{};
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
    auto tests = tests_at_point();
    locate_tests(*iap, *rap, tests, marks, evaluate_test,
      [&](auto &test, double &dstest, bool &stepped) {
        lcspbv(iap, rap, par, icp, test.function, funi, bcni, icni, pvli,
            test.sample, rlcur.data(), rlold.data(), rldot.data(), &ntst_plus_one,
            ups.data(), dups.data(), uoldps.data(), udotps.data(), upoldp.data(),
            fa.data(), fc.data(), tm.data(), dtm.data(), p0.data(), p1.data(),
            ev.data(), thl, thu, marks, dstest, stepped, test.accept);
      });
    if (iap->istop != 1 && iap->itp.kind == xpp::AutoxPointKind::Pending)
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

int 
contbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), doublereal *rds, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *dtm, doublereal *thl, doublereal *thu)
{
  integer ups_dim1, udotps_dim1, uoldps_dim1;

  integer ndim, ncol, nfpr, nrow, ntst, i, j;
  doublereal dsold;

  doublereal dds;

  /* Determines an initial approximation to the next solution point, */
  /* by a computation of the null space of the Jacobian. */
  /* The stepsize used in the preceding step has been stored in DSOLD. */

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

int 
extrbv(iap_type *iap, rap_type *rap, FUNI_TYPE((*funi)), doublereal *rds, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *udotps)
{
  xpp::autox_stability_step_start(iap->run->stability,
      iap->ibr, iap->ntot + (iap->run->locating ? 1 : 0));

  integer ups_dim1, udotps_dim1, uoldps_dim1;

  integer ndim, ncol, nfpr, nrow, ntst, i, j;

  /* Determines an initial approximation to the next solution by */
  /* a computation of the null space of the Jacobian. */
  /* The stepsize used in the preceding step has been stored in DSOLD. */

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

int 
stupbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *upoldp)
{
  integer ups_dim1, uoldps_dim1, upoldp_dim1;

  integer ndim, ncol;
  integer nfpr, ntst;
  integer i, j, k;
  integer n1;
  xpp::AutoxProblem ips;

  std::vector<doublereal> dfdp(xpp::size_product(iap->ndim, iap->run->parameter_count));
  std::vector<doublereal> dfdu(xpp::size_product(iap->ndim, iap->ndim));
  std::vector<doublereal> uold(iap->ndim);
  std::vector<doublereal> f(iap->ndim);
  std::vector<doublereal> u(iap->ndim);

/* Stores U-prime (derivative with respect to T) in UPOLDP. */

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

int 
stepbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), PVLI_TYPE_BVP((*pvli)), doublereal *rds, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *dups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *fa, doublereal *fc, doublereal *tm, doublereal *dtm, doublereal *p0, doublereal *p1, doublereal *thl, doublereal *thu)
{
  integer ups_dim1, uoldps_dim1, udotps_dim1, fa_dim1;

  integer iads;
  doublereal adrl;
  bool done;
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

    { int iflag; autox_checkpoint(*iap->run,&iflag); } /* xppautX: cancel */
    if (!iap->run->locating && autox_cancelled(*iap->run)) { nrow = ndim * ncol; goto L13; } /* xppautX: cancel */
    nitps = nit1;
    iap->nit = nitps;
    nllv = 0;

    ifst = 0;
    if (nitps <= nwtn) {
      ifst = 1;
    }

    {
      SetubvStop stop(*iap->run, !iap->run->locating); /* xppautX: cancel */
      solvbv(&ifst, iap, rap, par, icp, funi, bcni, icni, 
	     rds, &nllv, rlcur, rlold, rldot, ndxloc, 
	     ups, dups, uoldps, 
	     udotps, upoldp, dtm, fa, fc, p0, 
	     p1, thl, thu);
    }
    if ((!iap->run->locating && autox_cancelled(*iap->run)) || jacobian_failed(iap)) { nrow = ndim * ncol; goto L13; } /* xppautX: cancel, W275 */
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
    if (!xpp::all_finite({par, static_cast<size_t>(iap->run->parameter_count)})) {
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

    done = true;
    rdrl = 0.;
    for (i = 0; i < nfpr; ++i) {
      adrl = f2c::abs(fc[ndim + i]) / (f2c::abs(rlcur[i]) + 1.);
      if (adrl > epsl) {
	done = false;
      }
      if (adrl > rdrl) {
	rdrl = adrl;
      }
    }
    rdumx = dumx / (umx + 1.);
    if (done && rdumx < epsu) {
      if (iap->run->hopf_start && iap->ntot <= hopf_start_steps && (iap->ilp == 1 || iap->isp != 0)) {
        integer refactor = 1, tangent = -1;
        doublereal no_step = 0.;
        solvbv(&refactor, iap, rap, par, icp, funi, bcni, icni, &no_step, &tangent,
               rlcur, rlold, rldot, ndxloc, ups, dups, uoldps, udotps, upoldp, dtm,
               fa, fc, p0, p1, thl, thu);
        if ((!iap->run->locating && autox_cancelled(*iap->run)) || jacobian_failed(iap)) { nrow = ndim * ncol; goto L13; }
      }
      (*pvli)(iap, rap, icp, dtm, ndxloc, ups, &ndim, p0, p1, par);
      if (!xpp::all_finite({par, static_cast<size_t>(iap->run->parameter_count)})) {
        nonfinite = true;
        goto L3;
      }
      if (iid >= 2) {
	xpp::print(iap->run->fp9," \n");	
      }
      return 0;
    }

    if (nitps == 1) {
      delref = f2c::max(rdrl,rdumx) * autox_divergence_ratio;
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
    xpp::print(iap->run->fp9,"{:4} {:5} NOTE:No convergence with fixed step size\n",ibr,ntop);
    xpp::autox_stop_noconv(iap->run->stop, {xpp::AUTOX_STOP_NOCONV_FIXED, *rds, dsmin, itnw, epsl, epsu, iap->run->solvbv.pivot}); /* xppautX: T23 */
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
    xpp::print(iap->run->fp9,"{:4} {:5} NOTE:Retrying step\n",ibr,ntop);

  }
  goto L1;

/* Minimum stepsize reached. */

 L12:
  xpp::print(iap->run->fp9,"{:4} {:5}, NOTE:No convergence using minimum step size\n",ibr,ntop);
  xpp::autox_stop_noconv(iap->run->stop, {xpp::AUTOX_STOP_NOCONV_MIN, *rds, dsmin, itnw, epsl, epsu, iap->run->solvbv.pivot}); /* xppautX: T23 */
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
  const bool retained = xpp::autox_stability_step_failed(iap->run->stability,
      iap->ibr, iap->ntot + 1);
  istop = 1;
  iap->istop = istop;
  if (nonfinite && !stop_asked(iap))
    xpp::autox_nonfinite(*iap, iap->ntot + 1);

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
    xpp::autox_stability_multipliers_restored(iap->run->stability, *iap, p0, p1);
  }
  return 0;
} /* stepbv_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*      Restart of Solution Branches ( Differential Equations ) */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

int 
rsptbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), STPNT_TYPE_BVP((*stpnt)), doublereal *rds, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *dups, doublereal *tm, doublereal *dtm, doublecomplex *ev, integer *nodir, doublereal *thl, doublereal *thu)
{
  integer ups_dim1, uoldps_dim1;

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
    bool found = false;
    findlb(iap, rap, iap->irs, &junk, &found);
    xpp::TokenReader tr = xpp::TokenReader::attach(iap->run->fp3);
    xpp::AutoxSolutionHeader header;
    autox_restart_read(iap, xpp::read_autox_solution_header(tr, header, iap->run->parameter_count), "its header");
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

int 
stpnbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *ntsrs, integer *ncolrs, doublereal *rlcur, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *upoldp, doublereal *tm, doublereal *dtm, integer *nodir, doublereal *thl, doublereal *thu)
{

  integer ndim, nars;
  integer nfpr;

  integer i;

  std::vector<integer> icprs(iap->run->parameter_count);

  integer nfprs, nskip1, nskip2;
  xpp::AutoxPointCode itprs;

  integer ndimrd, ndimrs, isw;
  xpp::AutoxProblem ips;

  /* This subroutine locates and retrieves the information required to */
  /* restart computation at the point with label IRS. */
  /* This information is expected on unit 3. */

  ndim = iap->ndim;
  ips = iap->ips;
  isw = iap->isw;
  nfpr = iap->nfpr;

  xpp::TokenReader tr;
  xpp::AutoxSolutionHeader header;
  read_autox_restart_header(iap, rap, tr, header, ntsrs, ncolrs);
  itprs = header.itp;
  nfprs = header.nfpr;
  nars = header.nar;
  iap->lab = header.lab;

  ndimrs = nars - 1;
  nskip1 = (ndimrs + 1) / (autox_solution_columns + 1) - ndim / autox_solution_columns;
  nskip2 = (ndimrs + 1) / (autox_solution_columns + 2) - ndim / (autox_solution_columns + 1);
  if (ndim <= ndimrs) {
    ndimrd = ndim;
  } else {
    ndimrd = ndimrs;
  }

  autox_restart_read(iap, xpp::read_autox_solution_rows(tr, ndim, *ntsrs, *ncolrs, *ndxloc,
                                    0, ndimrd, ups, tm, true, nskip1, {}),
                    "the solution rows");

  for (i = 0; i < nfprs; ++i) {
    autox_restart_read(iap, tr.read(icprs[i]), "the free parameter indices");
  }
  for (i = 0; i < nfprs; ++i) {
    autox_restart_read(iap, tr.read(rldot[i]), "the parameter derivatives");
  }

  /* Read U-dot (deriv. with respect to arclength along solution branch). */

  autox_restart_read(iap, xpp::read_autox_solution_rows(tr, ndim, *ntsrs, *ncolrs, *ndxloc,
                                    0, ndimrd, udotps, nullptr, true, nskip2, {}),
                    "the arclength derivatives");

  /* Read the parameter values. */

  autox_restart_read(iap, xpp::read_autox_solution_parameters(tr, header, *iap->run, par), "the parameter values");
  for (i = 0; i < nfpr; ++i) {
    rlcur[i] = par[icp[i]];
  }

  /* Special case : Preprocess restart data in case of homoclinic */
  /* continuation */

  if (ips == xpp::AutoxProblem::Homoclinic) {
    preho(*iap->run, ndxloc, ntsrs, &ndimrd, &ndim, ncolrs, ups, udotps, tm, par);

    /* Special case : Preprocess restart data in case of branch switching 
     */
    /* at a period doubling bifurcation. */

  } else if (ips == xpp::AutoxProblem::Periodic && isw == -1 && itprs.is(xpp::AutoxPointKind::PeriodDoubling)) {
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

int autox_uniform_start(iap_type *iap, rap_type *rap, doublereal *par,
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

void autox_hopf_mesh(const iap_type *iap, const rap_type *rap, integer ndxloc,
                    doublereal *ups, doublereal *udotps, doublereal *upoldp,
                    doublereal *tm, doublereal *dtm, doublereal *rldot,
                    const doublereal *u, const doublereal *rnllv, doublereal tpi) {
  const integer ndim = iap->ndim, ntst = iap->ntst, ncol = iap->ncol;
  // W458 (#521): the first cycle is the equilibrium plus DS times the
  // eigen-cycle udotps, so its time derivative, the next point's phase
  // condition, points along upoldp times the sign of DS. Oriented so, the
  // phase condition (and the determinant in BP's test) keeps its sign
  // from the first point to the second; with DS < 0 it flipped, a BP between
  // the second and third point that is not there (motoneuron6c's cycles).
  const doublereal orientation = std::copysign(1., rap->ds);
  msh(iap, rap, tm);
  const doublereal dt = 1. / ntst;
  for (integer j = 0; j < ntst + 1; ++j) {
    const doublereal time = tm[j];
    const doublereal sine = xpp::math::sin(tpi * time);
    const doublereal cosine = xpp::math::cos(tpi * time);
    for (integer k = 0; k < ndim; ++k) {
      udotps[j + k * ndxloc] = sine * rnllv[k] + cosine * rnllv[ndim + k];
      upoldp[j + k * ndxloc] = orientation * (cosine * rnllv[k] - sine * rnllv[ndim + k]);
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
        upoldp[j + k1 * ndxloc] = orientation * (cosine * rnllv[k] - sine * rnllv[ndim + k]);
        ups[j + k1 * ndxloc] = u[k];
      }
    }
  }
  rldot[0] = 0.;
  rldot[1] = 0.;
  for (integer i = 0; i < ntst; ++i) dtm[i] = dt;
}

int
stpnub(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *ntsrs, integer *ncolrs, doublereal *rlcur, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *upoldp, doublereal *tm, doublereal *dtm, integer *nodir, doublereal *thl, doublereal *thu)
{
  return autox_uniform_start(iap, rap, par, icp, ntsrs, ncolrs, rlcur, *ndxloc, ups, tm, nodir,
      [&](doublereal *state, doublereal &time) { stpnt(*iap->run, iap->ndim, time, state, par); });
} /* stpnub_ */

int 
setrtn(iap_type *iap, integer *ntst, integer *ndxloc, doublereal *ups, doublereal *par)
{
  integer ups_dim1;

  integer i;

  integer nbc;

  /* Initialization for rotations */

  ups_dim1 = *ndxloc;
  
  par[xpp::autox_unknown::rotation_period] = pi(2.0);
  nbc = iap->nbc;

  iap->run->rotations.irtn = 0;
  for (i = 0; i < nbc; ++i) {
    doublereal tmp = (ARRAY2D(ups, *ntst, i) - ARRAY2D(ups, 0, i)) / pi(2.0);
    iap->run->rotations.nrtn[i] = static_cast<integer>(tmp >= 0. ? floor(tmp + .5) : -floor(.5 - tmp)); /* the nearest whole number */

    if (iap->run->rotations.nrtn[i] != 0) {
      iap->run->rotations.irtn = 1;
    }
  }

  return 0;
} /* setrtn_ */

int 
stdrbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer ndxloc, doublereal *ups, doublereal *dups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *fa, doublereal *fc, doublereal *dtm, integer iperp, doublereal *p0, doublereal *p1, doublereal *thl, doublereal *thu)
{
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
    xpp::print(iap->run->fp9,"Starting direction of the free parameter(s) :\n");	

    for (i = 0; i < nfpr; ++i) {
      xpp::print(iap->run->fp9," PAR({:3}) :{:11.3E}\n",icp[i],rldot[i]);	
    }
  }

  return 0;
} /* stdrbv_ */

/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */
/*  Detection and Location of Branch Points in Boundary Value Problems */
/* ----------------------------------------------------------------------- */
/* ----------------------------------------------------------------------- */

int 
lcspbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FNCS_TYPE_BVP((*fncs)), FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), PVLI_TYPE_BVP((*pvli)), AutoxTestSample &q, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *dups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *fa, doublereal *fc, doublereal *tm, doublereal *dtm, doublereal *p0, doublereal *p1, doublecomplex *ev, doublereal *thl, doublereal *thu, std::span<const AutoxMark> marks, double &dstest, bool &stepped, const std::function<bool()> *accept)
{
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
  const auto n = static_cast<std::size_t>(iap->ndim);
  const auto np = static_cast<std::size_t>(iap->run->parameter_count);
  const auto nfpr = static_cast<std::size_t>(iap->nfpr);
  const auto nu = static_cast<std::size_t>(*ndxloc) * n * iap->ncol;
  const auto nc = static_cast<std::size_t>(iap->nbc + iap->nint + 1);
  locate_special_point(*iap, *rap, q, located_kind(fncs, *iap), fncs == fnuzbv, fncs == fnbpbv,
      fncs == fnspbv && iap->isw == 2 && iap->itpst == xpp::AutoxPointKind::NeimarkSacker, par[icp[0]], dstest, stepped, accept,
      [&](bool &chng) {
        return (*fncs)(iap, rap, par, icp, &chng, funi, bcni, icni, p0, p1, ev, rlcur, rlold,
                       rldot, ndxloc, ups, uoldps, udotps, upoldp, fa, fc, dups, tm, dtm,
                       thl, thu, marks);
      },
      [&](double &rds) {
        contbv(iap, rap, par, icp, funi, &rds, rlcur, rlold, &rldot[0], ndxloc, ups, uoldps,
               udotps, upoldp, dtm, thl, thu);
        stepbv(iap, rap, par, icp, funi, bcni, icni, pvli, &rds, &rlcur[-1 + 1], rlold, rldot,
               ndxloc, ups, dups, uoldps, udotps, upoldp, fa, fc, tm, dtm, p0, p1, thl, thu);
      },
      [&] {
        return LocationStateWithMultipliers(LocationState(*iap, *rap, {{par, np}, {rlcur, nfpr},
            {rlold, nfpr}, {rldot, nfpr}, {ups, nu}, {uoldps, nu}, {udotps, nu}, {upoldp, nu},
            {p0, n * n}, {p1, n * n}, {dups, nu}, {fa, nu}, {fc, nc}}), ev, n);
      },
      [&] {
        return iap->isw == 2 && iap->itpst == xpp::AutoxPointKind::NeimarkSacker &&
            xpp::autox_torus_singular(par[xpp::autox_unknown::torus_angle]);
      });
  return 0;
} /* lcspbv_ */

doublereal 
fnlpbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, bool *chng, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), doublereal *p0, doublereal *p1, doublecomplex *ev, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *fa, doublereal *fc, doublereal *dups, doublereal *tm, doublereal *dtm, doublereal *thl, doublereal *thu, std::span<const AutoxMark> marks)
{
  integer udotps_dim1, fa_dim1;
  doublereal ret_val;

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
    xpp::print(iap->run->fp9,"{:4} {:5}        Fold Function {:14.6E}\n",f2c::abs(ibr),ntop,rldot[0]);
  }

  /* Set the quantity to be returned. */

  ret_val = rldot[0];
  *chng = true;
  return ret_val;
} /* fnlpbv_ */

doublereal 
fnbpbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, bool *chng, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), doublereal *p0, doublereal *p1, doublecomplex *ev, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *fa, doublereal *fc, doublereal *dups, doublereal *tm, doublereal *dtm, doublereal *thl, doublereal *thu, std::span<const AutoxMark> marks)
{
  doublereal ret_val;

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
  xpp::ge(iap->run->fp9, iap->iid, ndim, ndim, pp.data(), 0, 1, &u, 1, &f, &det);
  rap->det = det;

  /* Set the determinant of the normalized reduced system. */

  if (det != 0.) {
    ret_val = det0 / det;
    *chng = true;
    const double angle = par[xpp::autox_unknown::torus_angle];
    if (iap->isw == 2 && iap->itpst == xpp::AutoxPointKind::NeimarkSacker && xpp::autox_torus_near_one_to_one(angle)) {
      // W405 (#467): a cycle's torus equations hold the trivial multiplier's
      // block, rotation minus identity, whose determinant 4 sin^2(theta/2)
      // vanishes at the 1:1 resonance: divided out there, a branch point at
      // the resonance is a simple zero of the test and not a triple one.
      const double half = xpp::math::sin(angle / 2);
      if (half != 0) ret_val /= 4 * half * half;
    }
  } else {
    ret_val = autox_test_not_applicable;
    *chng = false;
  }

  if (iid >= 2) {
    xpp::print(iap->run->fp9,"{:4} {:5}        BP   Function {:14.6E}\n",f2c::abs(ibr),ntop,ret_val);

  }
  return ret_val;
} /* fnbpbv_ */

doublereal 
fnspbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, bool *chng, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), doublereal *p0, doublereal *p1, doublecomplex *ev, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *fa, doublereal *fc, doublereal *dups, doublereal *tm, doublereal *dtm, doublereal *thl, doublereal *thu, std::span<const AutoxMark> marks)
{
  doublereal ret_val;

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

  ndim = iap->ndim;
  isp = iap->isp;
  isw = iap->isw;
  iid = iap->iid;
  ibr = iap->ibr;
  ntot = iap->ntot;
  ntop = ntot + 1;

  /* Initialize: until a test below applies, none does. */

  ret_val = autox_test_not_applicable;
  d = ret_val;
  *chng = false;

/*  Compute the Floquet multipliers */

  if (isw==2) {
    xpp::autox_cycle_multipliers(*iap,p0,p1,ev,true);
    ndim=iap->ndm;
  } else xpp::flowkm(iap->run->fp9, &ndim, p0, p1, &iid, wrk.data(), ev, true);
  /* Find the multiplier closest to z=1. */
  /* xppautX: the orbit's Floquet multipliers (autox_stability.h) */
  xpp::autox_stability_computed(iap->run->stability,ibr,ntot+1,{&ev[0].r,2*static_cast<size_t>(ndim)},xpp::AUTOX_STABILITY_PERIODIC);
  amin = autox_rlarge;
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
  if (isw==2 && (iap->itpst==xpp::AutoxPointKind::CycleFold ||
                 iap->itpst==xpp::AutoxPointKind::PeriodDoubling))
    first=static_cast<integer>(autox_cycle_place_imposed(iap->itpst,{ev,static_cast<std::size_t>(ndim)}));
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
      xpp::print(iap->run->fp9,"{:4} {:5} NOTE:Multiplier inaccurate\n",f2c::abs(ibr),ntop);

    }
    xpp::autox_stop_note(*iap->run,
                        xpp::format("Br {} Pt {}: the Floquet multipliers are inaccurate (the one at 1 is off by {:.3g}); "
                                    "period doubling and torus detection are off until they are accurate again (try a larger mesh_intervals or collocation_points)",
                                    f2c::abs(ibr), ntop, amin));
    for (i = 0; i < ndim; ++i) {
      xpp::print(iap->run->fp9,"{:4} {:5}        Multiplier {:3} {:14.6E} {:14.6E}\n",f2c::abs(ibr),ntop,i,ev[i].r,ev[i].i);
    }
    nins = 0;
    iap->nins = nins;
    xpp::print(iap->run->fp9,"{:4} {:5}        Multipliers:   Stable: {:3}\n",f2c::abs(ibr),ntop,nins);
    isp = -isp;
    iap->isp = isp;
    return ret_val;
  }

  /* Restart automatic detection if the Floquet multiplier at z=1 is */
  /* sufficiently accurate again. */

  if (isp < 0) {
    if (amin < .01) {
      xpp::print(iap->run->fp9,"{:4} {:5} NOTE:Multiplier accurate again\n",f2c::abs(ibr),ntop);
      isp = -isp;
      iap->isp = isp;
    } else {
      for (i = 0; i < ndim; ++i) {
	xpp::print(iap->run->fp9,"{:4} {:5}        Multiplier {:3} {:14.6E} {:14.6E}\n",f2c::abs(ibr),ntop,i,ev[i].r,ev[i].i);
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
    d = autox_test_not_applicable;
    ret_val = d;
  } else {
    if (isp == 2) {
      if (ev[1].i == 0. && ev[1].r > 0.) {
	/*            *Ignore if second multiplier is real positive */
	d = autox_test_not_applicable;
      } else {
	d = z_abs(&ev[1]) - 1.;
      }
      if (isw == 2) {
        d=autox_cycle_test(*iap,*rap,par,{ev,static_cast<std::size_t>(ndim)},chng);
        ret_val=d;
      } else {
	ret_val = d;
      }
      nins = iap->nins;
      if (isw!=2 && nins1 != nins) {
	*chng = true;
      }
    }
  }

  nins = nins1;
  iap->nins = nins;
  if (iid >= 2 && (isp == 1 || isp == 2)) {
    xpp::print(iap->run->fp9,"{:4} {:5}        SPB  Function {:14.6E}\n",f2c::abs(ibr),ntop,d);
  }

  /* Print the Floquet multipliers. */

  nins = iap->nins;
  xpp::print(iap->run->fp9,"{:4} {:5}        Multipliers:   Stable: {:3}\n",f2c::abs(ibr),ntop,nins);

  for (i = 0; i < ndim; ++i) {
    xpp::print(iap->run->fp9,"{:4} {:5}        Multiplier {:3} {:14.6E} {:14.6E}\n",f2c::abs(ibr),ntop,i,ev[i].r,ev[i].i);

  }

  return ret_val;
} /* fnspbv_ */

doublereal 
fnuzbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, bool *chng, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), doublereal *p0, doublereal *p1, doublecomplex *ev, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *fa, doublereal *fc, doublereal *dups, doublereal *tm, doublereal *dtm, doublereal *thl, doublereal *thu, std::span<const AutoxMark> marks)
{
  return user_zero(*iap, par, icp, chng, marks, boundary_user_zero_width);
} /* fnuzbv_ */

doublereal
fnclosedbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, bool *chng, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), doublereal *p0, doublereal *p1, doublecomplex *ev, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *fa, doublereal *fc, doublereal *dups, doublereal *tm, doublereal *dtm, doublereal *thl, doublereal *thu, std::span<const AutoxMark> marks)
{
  return closed_test(*iap, par, icp, chng);
} /* fnclosedbv_ */

int 
tpspbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublecomplex *ev)
{

  doublereal amin;
  integer ndim;
  doublereal epss, d;
  integer i;
  xpp::AutoxPointKind itpst;
  doublereal ad;
  integer loc, loc1;
  xpp::AutoxPointCode itp;
  doublereal azm1;

  /* Determines type of secondary periodic bifurcation. */

  ndim = iap->ndim;
  if (iap->isw==2) {
    iap->itp={xpp::autox_cycle_resonance_kind(iap->itpst,par[xpp::autox_unknown::torus_angle]),iap->itpst};
    return 0;
  }

  epss = rap->epss;
  itpst = iap->itpst;

  /* Find the eigenvalue closest to z=1. */

  loc = 1;
  amin = autox_rlarge;
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
  amin = autox_rlarge;
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

  if (f2c::abs(ev[loc1].i) > sqrt(epss)) {
    /*       ** torus bifurcation */
    itp = xpp::AutoxPointCode{xpp::AutoxPointKind::NeimarkSacker, itpst};
    iap->itp = itp;
    par[xpp::autox_unknown::torus_angle] =f2c::abs(xpp::math::atan2(ev[loc1].i,ev[loc1].r));

  } else { /* a real multiplier */
    if (ev[loc1].r < -.5) {
      /*       ** period doubling */
      itp = xpp::AutoxPointCode{xpp::AutoxPointKind::PeriodDoubling, itpst};
      iap->itp = itp;
    } else {
      /*       ** something else... */
      itp = xpp::AutoxPointCode{};
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

int 
stplbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *tm, doublereal *dtm, doublereal *thl, doublereal *thu)
{
  integer labw, ibrs, nins, itmp, jtmp, ntot;
  int iflag=0;
  xpp::AutoxStopAt stop_at = {0}; /* xppautX: T23 */
  integer i;
  std::vector<double> u_high(iap->ndim);
  std::vector<double> u_low(iap->ndim);
  std::vector<double> u_0(iap->ndim);
  std::vector<double> u_bar(iap->ndim);

  integer istop, n2, ntots;

  integer lab;
  doublereal amp;

  std::array<doublereal, autox_diagram_columns> umx;

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

  auto context = point_output_context(*iap, *rap);

  ntot = iap->ntot;
  ++ntot;
  iap->ntot = ntot;

/* ITP is set to 4 every NPR steps along a branch of solns and the entire 
*/
/* solution is written on unit 8. */

  if (context.npr != 0) {
    if (ntot % context.npr == 0 && context.itp.kind == xpp::AutoxPointKind::Regular) {
      context.itp = xpp::AutoxPointCode{xpp::AutoxPointKind::Output, context.itpst};
    }
    iap->itp = context.itp;
  }

  /* Check whether limits of the bifurcation diagram have been reached : */

  const auto orbit_norm = [&] {
    doublereal norm = 0;
    integer iab = f2c::abs(context.iplt);
    if (iab == 0 || iab > context.ndm * 3) {
      norm = sqrt(rnrmsq(iap, &context.ndm, ndxloc, ups, dtm, thu));
    }
    if (context.iplt > 0 && iab <= context.ndm) {
      norm = rmxups(iap, ndxloc, &iab, ups);
    }
    if (context.iplt > context.ndm && iab <= (context.ndm * 2)) {
      norm = rintg(iap, ndxloc, iab - context.ndm, ups, dtm);
    }
    if (context.iplt > (context.ndm * 2) && iab <= context.ndm * 3) {
      integer tmp = iab - (context.ndm * 2);
      norm = rnrm2(iap, ndxloc, &tmp, ups, dtm);
    }
    if (context.iplt < 0 && iab <= context.ndm) {
      norm = rmnups(iap, ndxloc, &iab, ups);
    }
    return norm;
  };
  amp = orbit_norm();

  /* here is another place for byeautox
   call with iflag  */
  autox_checkpoint(*iap->run,&iflag);
  istop = iap->istop;
  if (istop == 0 && autox_cancelled(*iap->run)) iflag = 0; /* xppautX: cancel: as in stplae */
  stop_at.user = iflag == 1 || istop == 1; /* xppautX: T23, unless MX or UZR below */
  if (istop == 1 && !autox_cancelled(*iap->run)) { /* xppautX: cancel: EP, not MX */
    /*        ** Maximum number of iterations reached somewhere. */
    context.itp = xpp::AutoxPointCode{xpp::AutoxPointKind::MaxSteps, context.itpst};
    iap->itp = context.itp;
    stop_at.noconv = 1, stop_at.user = 0;
  } else if (istop == -1) {
    /*        ** UZR endpoint */
    context.itp = xpp::AutoxPointCode{xpp::AutoxPointKind::End, context.itpst};
    iap->itp = context.itp;
  } else {
    if (par[icp[0]] < context.rl0 || par[icp[0]] > context.rl1 || amp < context.a0 || amp > context.a1 ||
	ntot >= context.nmx || iflag==1 ) { /* more bye auto */
      istop = 1;
      iap->istop = istop;
      /* xppautX (W495): a special point the last step located keeps its
         label; the branch still ends there, its stop note says which bound
         (AUTO-07p labels it EP: auto07p-findings 9) */
      if (context.itp.kind == xpp::AutoxPointKind::Regular || context.itp.kind == xpp::AutoxPointKind::Output)
        context.itp = xpp::AutoxPointCode{xpp::AutoxPointKind::End, context.itpst};
      iap->itp = context.itp;
    }
  }

  /* Compute maxima of solution components. */

  n2 = context.ndm;
  if (n2 > autox_diagram_columns) {
    n2 = autox_diagram_columns;
  }
  const std::vector<double> extremes = orbit_extremes(*iap, *ndxloc, ups);
  const std::size_t node = static_cast<std::size_t>(iap->run->session->model().node);
  for (i = 0; i < n2; ++i) {
    itmp = i + 1;
    umx[i] = static_cast<std::size_t>(i) < node ? extremes[static_cast<std::size_t>(i)] : rmxups(iap, ndxloc, &itmp, ups);
  }

  /* compute max, min, etc of solutions   This is correct!! */
  for (std::size_t k=0;k<node;k++){
    itmp=static_cast<integer>(k)+1;
    u_high[k]=extremes[k];
    u_low[k]=extremes[node+k];
    u_0[k]=ups[static_cast<integer>(k)*(*ndxloc)];
    u_bar[k]=rintg(iap, ndxloc, itmp, ups, dtm);
  }
  xpp::autox_branch_stored(*iap, ntot, rap->epss, par, icp, extremes, true); /* W604: what the branch closes on */

  /* xppautX (W518): a cycle branch whose period grows without bound while
     its orbit lingers at an equilibrium ends at a SNIC, a homoclinic orbit
     or a heteroclinic cycle (autox_global.h); a special point the last step
     located keeps its label and the next point decides */
  if (context.ips == xpp::AutoxProblem::Periodic && f2c::abs(context.isw) != 2) {
    xpp::AutoxStop &stop = iap->run->stop;
    const bool growing = xpp::autox_period_watch_add(stop.period_watch, par[xpp::autox_unknown::period], ntot == 1);
    if (growing && iap->istop == 0 && !autox_cancelled(*iap->run) &&
        (context.itp.kind == xpp::AutoxPointKind::Regular || context.itp.kind == xpp::AutoxPointKind::Output)) {
      if (auto end = xpp::autox_global_end(*iap, par, icp, ups, *ndxloc, par[xpp::autox_unknown::period], stop.period_watch.shortest)) {
        stop.global = std::move(*end);
        istop = 1;
        iap->istop = istop;
        context.itp = xpp::AutoxPointCode{stop.global.kind, xpp::AutoxPointKind::Regular};
        iap->itp = context.itp;
        stop_at.user = 0, stop_at.global = 1;
      }
    }
  }

  /* xppautX (W604): a branch of cycles whose amplitude falls back to about zero at an equilibrium with a purely
     imaginary pair ends at that Hopf point (autox_ends.h), stored as one coherent point: its parameter where the
     squared amplitude reaches zero, the equilibrium there as its orbit, extremes, mean and norm, and the period
     2 pi over the pair's frequency; a special point the last step located keeps its label */
  if (context.ips == xpp::AutoxProblem::Periodic && f2c::abs(context.isw) != 2 && context.ndm == static_cast<integer>(node)) {
    xpp::AutoxStop &stop = iap->run->stop;
    const double amplitude = xpp::autox_orbit_amplitude({u_high.data(), node}, {u_low.data(), node}, {u_bar.data(), node});
    const bool shrunk = xpp::autox_amplitude_watch_add(stop.amplitude_watch, amplitude, par[icp[0]], ntot == 1);
    if (shrunk && iap->istop == 0 && !autox_cancelled(*iap->run) &&
        (context.itp.kind == xpp::AutoxPointKind::Regular || context.itp.kind == xpp::AutoxPointKind::Output)) {
      if (auto end = xpp::autox_hopf_end(*iap, par, icp, {u_bar.data(), node}, stop.amplitude_watch)) {
        stop.hopf_end = std::move(*end);
        xpp::autox_hopf_end_apply(*iap, stop.hopf_end, par, icp, *ndxloc, ups);
        for (std::size_t k = 0; k < node; ++k) u_high[k] = u_low[k] = u_0[k] = u_bar[k] = stop.hopf_end.equilibrium[k];
        for (i = 0; i < n2 && static_cast<std::size_t>(i) < node; ++i) umx[i] = stop.hopf_end.equilibrium[static_cast<std::size_t>(i)];
        amp = orbit_norm();
        istop = 1;
        iap->istop = istop;
        context.itp = xpp::AutoxPointCode{xpp::AutoxPointKind::HopfEnd, xpp::AutoxPointKind::Regular};
        iap->itp = context.itp;
        stop_at.user = 0, stop_at.hopf_end = true;
      }
    }
  }

  /* xppautX (W604): a branch that came back to its start ends there (autox_ends.h), unless it is the end at a
     Hopf point */
  if (!stop_at.hopf_end) end_closed_branch(*iap, context.itp, stop_at);
  istop = iap->istop;

  /* All special points receive label: */

  labw = 0;
  if (context.itp.kind != xpp::AutoxPointKind::Regular) {
    lab = iap->lab;
    ++lab;
    iap->lab = lab;
    labw = lab;
  }

  /* Determine stability, and write output on units 7 and 8. */

  ibrs = context.ibr;
  ntots = ntot;
  if (context.ips == xpp::AutoxProblem::Periodic && f2c::abs(context.isw) != 2) {
    ibrs = -context.ibr;
    nins = iap->nins;
    if (nins == context.ndim) {
      ntots = -ntot;
    }
  }
  jtmp = iap->run->parameter_count;
  /* addbif max min  of variables & initial data 

  */
  xpp::addbif(iap,rap, ntots, ibrs, par,icp,labw,&amp, u_high, u_low, u_0, u_bar);  

  wrline(iap, rap, par, icp, &icp[jtmp], &ibrs, &ntots,
	 &labw, &amp, umx.data());
  if (iap->istop != 0) { /* xppautX: why the branch ended (autox_stop.h, T23) */
    stop_at.br = context.ibr, stop_at.pt = ntot, stop_at.ipar = icp[0], stop_at.par = par[icp[0]], stop_at.norm = amp;
    stop_at.rl0 = context.rl0, stop_at.rl1 = context.rl1, stop_at.a0 = context.a0, stop_at.a1 = context.a1, stop_at.nmx = context.nmx;
    stop_at.mark = iap->istop == -1;
    if (stop_at.mark) stop_at.user = 0;
    xpp::autox_stop_branch_end(*iap->run, &stop_at);
    iap->run->stop.closure_gap.reset();
  }

  /* Write plotting and restart data on unit 8. */

  if (context.itp.kind != xpp::AutoxPointKind::Regular) {
    wrtbv8(iap, rap, par, icp, rldot, ndxloc, ups, 
	   udotps, tm, dtm);
  }

  return 0;
} /* stplbv_ */

int 
wrtbv8(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *tm, doublereal *dtm)
{
  integer ups_dim1, udotps_dim1;

  integer ndim, ncol, nfpr, ntpl, jtmp, ntot, ntst, i, j, k;
  doublereal t;
  integer k1, k2;
  doublereal rn;
  integer nrowpr, lab, ibr, nar, nrd, isw;
  xpp::AutoxPointCode itp;

  FILE *const fp8=fort8(*iap->run);

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
/*  iap->run->parameter_count : The dimension of the par array (and the number of */
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

/* Finally the parameter values PAR(i) , i=1,iap->run->parameter_count, are written. */

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
  nrd = ndim / autox_solution_columns + 2 + (ndim - 1) / autox_solution_columns;
  jtmp = iap->run->parameter_count;
  nrowpr = nrd * (ncol * ntst + 1) + (nfpr - 1) / autox_solution_columns + 1 + (jtmp - 1) / autox_solution_columns + 
    1 + (nfpr - 1) / autox_solution_parameter_indices_per_line + 1;

  {
    xpp::AutoxSolutionHeader header;
    header.ibr=ibr; header.ntot=ntot; header.itp=itp; header.lab=lab; header.nfpr=nfpr; header.isw=isw;
    header.ntpl=ntpl; header.nar=nar; header.nskip=nrowpr; header.ntst=ntst; header.ncol=ncol;
    header.npar=iap->run->parameter_count;
    xpp::print(fp8,"{}",xpp::autox_solution_header_text(header));
  }

/* Write the entire solution on unit 8 : */

  for (j = 0; j < ntst; ++j) {
    rn = 1. / ncol;
    for (i = 0; i < ncol; ++i) {
      k1 = i * ndim;
      k2 = (i + 1) * ndim;
      t = tm[j] + i * rn * dtm[j];
      xpp::print(fp8,"    {:19.10E}",t);
      for (k = k1; k < k2; ++k) {
	if((k+1-k1)%autox_solution_columns==0)
	  xpp::print(fp8,"\n    ");
	xpp::print(fp8,"{:19.10E}",ARRAY2D(ups, j, k));
      }
      xpp::print(fp8,"\n");	 

    }
  }
  xpp::print(fp8,"    {:19.10E}",tm[ntst]);
  for (i = 0; i < ndim; ++i) {
    if((i+1)%autox_solution_columns==0)
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
  print_solution_row_values(fp8, {rldot, static_cast<std::size_t>(nfpr)});

  for (j = 0; j < ntst; ++j) {
    for (i = 0; i < ncol; ++i) {
      k1 = i * ndim;
      k2 = (i + 1)* ndim;

      xpp::print(fp8,"    ");
      for (k = k1; k < k2; ++k) {
	if((k!=k1)&&((k-k1)%autox_solution_columns==0))
	  xpp::print(fp8,"\n    ");
	xpp::print(fp8,"{:19.10E}",ARRAY2D(udotps, j, k));
      }
      xpp::print(fp8,"\n");
    }
  }
  xpp::print(fp8,"    ");

  for (k = 0; k < ndim; ++k) {
    if((k!=0)&&(k%autox_solution_columns==0))
      xpp::print(fp8,"\n    ");
    xpp::print(fp8,"{:19.10E}",ARRAY2D(udotps, ntst, k));
  }
  xpp::print(fp8,"\n");

/* Write the parameter values. */

  print_solution_row_values(fp8, {par, static_cast<std::size_t>(iap->run->parameter_count)});
  fflush(fp8);
  return 0;
} /* wrtbv8_ */

int 
wrtbv9(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *rlcur, integer *ndxloc, doublereal *ups, doublereal *tm, doublereal *dtm, doublereal *thl, doublereal *thu)
{
  
  integer ups_dim1;

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
      xpp::print(iap->run->fp9,"========================================");
      xpp::print(iap->run->fp9,"========================================\n");
      xpp::print(iap->run->fp9,"  BR    PT  IT\n");	
      
    }
    mtot = ntot + 1;
    xpp::print(iap->run->fp9,"{:4} {:5}{:4}    {:14.6E}{:14.6E}\n",ibr,mtot,nitps,rlcur[0],amp);
  }
  
  if (iid >= 5) {
    xpp::print(iap->run->fp9," UPS :\n");	
    for (j = 0; j < ntst; ++j) {
      rn = 1. / ncol;
      for (i = 0; i < ncol; ++i) {
	t = tm[j] + i * rn * dtm[j];
	k1 = i * ndim;
	k2 = (i + 1) * ndim;
	xpp::print(iap->run->fp9," {:14.6E}",t);	
	for (k = k1; k < k2; ++k) {
	  if((k+1-k1)%7==0)
	    xpp::print(iap->run->fp9,"\n ");
	  xpp::print(iap->run->fp9," {:14.6E}",ARRAY2D(ups, j, k));	
	}
	xpp::print(iap->run->fp9,"\n");	
	
      }
    }
    xpp::print(iap->run->fp9," {:14.6E}",tm[ntst]);	
    for (i = 0; i < ndim; ++i) {
      if((i+1)%7==0)
	xpp::print(iap->run->fp9,"\n ");
      xpp::print(iap->run->fp9," {:14.6E}",ARRAY2D(ups, ntst, i));	
    }
    xpp::print(iap->run->fp9,"\n");	
  }
  return 0;
} /* wrtbv9_ */

int 
pvlsae(iap_type *iap, rap_type *rap, doublereal *u, doublereal *par)
{

  integer ndm;

  ndm = iap->ndm;
  if (iap->itpst == xpp::AutoxPointKind::Hopf && iap->ndim == 2 * ndm + 2) {
    // AUTO-07p equilibrium.f90:365-367 retains PAR(11) for kappa <= 0.
    const double kappa = u[2 * ndm];
    if (kappa > 0.) par[xpp::autox_unknown::period] = pi(2.) / sqrt(kappa);
  }
  if (iap->ips==xpp::AutoxProblem::DiscreteMap && iap->itpst==xpp::AutoxPointKind::NeimarkSacker)
    par[xpp::autox_unknown::torus_angle]=u[2*ndm];
  pvls(ndm, u, par);

  return 0;
} /* pvlsae_ */

int 
pvlsbv(iap_type *iap, rap_type *rap, integer *icp, doublereal *dtm, integer *ndxloc, doublereal *ups, integer *ndim, doublereal *p0, doublereal *p1, doublereal *par)
{
  integer ndm;

  ndm = iap->jac;
  pvls(ndm, ups, par);

  return 0;
} /* pvlsbv_ */

void size_run_scratch(const iap_type &iap) {
    /* assign: zeroed */
    iap.run->scratch.dfu.assign(xpp::size_product(iap.ndim, iap.ndim), 0.);
    iap.run->scratch.dfp.assign(xpp::size_product(iap.ndim, iap.run->parameter_count), 0.);
    iap.run->scratch.uu1.assign(iap.ndim, 0.);
    iap.run->scratch.uu2.assign(iap.ndim, 0.);
    iap.run->scratch.ff1.assign(iap.ndim, 0.);
    iap.run->scratch.ff2.assign(iap.ndim, 0.);
    iap.run->scratch.funi_u1.resize(iap.ndim);
    iap.run->scratch.funi_u2.resize(iap.ndim);
    iap.run->scratch.funi_f1.resize(iap.ndim);
    iap.run->scratch.funi_f2.resize(iap.ndim);
    iap.run->rotations.nrtn.assign(iap.nbc, 0);
}
