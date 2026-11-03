#include "xpp_mem.h"

#include <array>
#include <cstdlib>
#include <memory>
#include <optional>
#include <vector>
#include "xpp_log.h"
#include "session.h"
#include "autox_evd.h" /* xAutox (its own extern) */
#include "autox_nox.h" /* xpp::autox_fort_path() */
#include "diagram.h" /* diagram_of_label */


namespace {
/* autoX's fort.3 (its restart data: read, or written from scratch), fort.7
   and fort.9 for one run, opened where open_autox put them: autoX reads and
   writes them through its AutoxRun (autox_state.h), which the run's iap_type
   points at. Closed on every way out of the run (Windows cannot rename or
   delete an open file, which close_autox does next), the pointers cleared
   first. */
struct RunUnits {
  AutoxRun &run;
  xpp::UniqueFile u3,u7,u9;
  RunUnits(AutoxRun &run_, bool restart)
    : run(run_)
  {
    const auto open = [this](int unit, const char *mode) {
      const std::string path=xpp::autox_fort_path(run,unit);
      xpp::UniqueFile file(xpp::files::open_stream(path,mode));
      if(!file)xpp::autox_fail((mode[0]=='r' ? xpp::files::open_error("autoX",path)
                                         : xpp::files::write_error("autoX",path)));
      return file;
    };
    u3=open(3,restart?"r":"w+");
    u7=open(7,"w");
    u9=open(9,"w");
    run.fp3=u3.get();
    run.fp7=u7.get();
    run.fp9=u9.get();
  }
  ~RunUnits() { run.fp3=run.fp7=run.fp9=nullptr; }
  RunUnits(const RunUnits &)=delete;
  RunUnits &operator=(const RunUnits &)=delete;
};

using AeStart = STPNT_TYPE_AE((*));
using BvStart = STPNT_TYPE_BVP((*));
using BvValues = PVLI_TYPE_BVP((*));

function_list algebraic(FUNI_TYPE((*funi)), AeStart start)
{
  return {AutoxSolver::Algebraic, {}, {funi, start, pvlsae}};
}

function_list boundary(FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), BvStart start, BvValues values = pvlsbv)
{
  return {AutoxSolver::BoundaryValue, {funi, bcni, icni, start, values}, {}};
}

/* a one-parameter continuation's functions (abs(ISW) != 2, and the
   problems that have no other): the start from scratch (IRS 0) or from a
   label of the restart file */
std::optional<function_list> one_parameter(const iap_type &iap)
{
  using xpp::AutoxProblem;
  using xpp::AutoxPointKind;
  const bool restart = iap.irs > 0;
  const bool two = std::abs(iap.isw) == 2;
  const auto ae = [&](FUNI_TYPE((*funi))) { return algebraic(funi, restart ? stpnae : stpnus); };
  const auto bv = [&](FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni))) {
    return boundary(funi, bcni, icni, restart ? stpnbv : stpnub);
  };
  switch (iap.ips) {
  case AutoxProblem::Algebraic:
  case AutoxProblem::SteadyState:
    if (!two) return ae(funi);
    break;
  case AutoxProblem::WaveSteady: /* spatially homogeneous solutions */
    if (!two) return ae(fnws);
    break;
  case AutoxProblem::DiscreteMap: /* fixed points */
    if (!two) return ae(fnds);
    break;
  case AutoxProblem::TimeIntegration:
    return ae(fnti);
  case AutoxProblem::Periodic:
    if (two) break;
    if (iap.itp.is(AutoxPointKind::Hopf) || iap.itp.started_from(AutoxPointKind::Hopf))
      return boundary(fnps, bcps, icps, stpnps);
    return bv(fnps, bcps, icps);
  case AutoxProblem::WavePeriodic: /* wave trains of parabolic systems */
    if (two) break;
    if (iap.itp.is(AutoxPointKind::Hopf)) return boundary(fnwp, bcps, icps, stpnwp);
    return bv(fnwp, bcps, icps);
  case AutoxProblem::BoundaryValue:
  case AutoxProblem::FloquetBoundaryValue:
    if (!two) return bv(funi, bcni, icni);
    break;
  case AutoxProblem::Homoclinic:
    if (!two) return boundary(fnho, bcho, icho, restart ? stpnbv : stpnho, pvlsho);
    break;
  case AutoxProblem::ParabolicPeriodicEvolution:
    return bv(fnpe, bcps, icpe);
  case AutoxProblem::PeriodicOptimization:
    if (std::abs(iap.isw) == 1) return boundary(fnpo, bcpo, icpo, iap.nfpr < 6 ? stpnpo : stpnbv);
    break;
  case AutoxProblem::ParabolicUserEvolution:
    return bv(fnpe, bcni, icpe);
  case AutoxProblem::ParabolicUserSteady:
    return bv(fnsp, bcni, icpe);
  case AutoxProblem::AlgebraicOptimization: {
    const bool from_fold = iap.itp.kind == AutoxPointKind::Fold;
    if (iap.nfpr + (from_fold || !restart ? 1 : 0) == 2) return algebraic(fnc1, restart ? stpnae : stpnc1);
    return algebraic(fnc2, from_fold ? stpnc2 : stpnae);
  }
  }
  return std::nullopt;
}

/* the two-parameter continuations (abs(ISW) == 2) of a special point:
   started at the point (ITP its kind) or restarted from a point of such a
   continuation (ITP's start its kind), whose start function is the plain
   restart's */
struct TwoParameterRow {
  bool (*problem)(xpp::AutoxProblem);
  std::array<xpp::AutoxPointKind, 2> kinds; /* the second Regular for one kind */
  function_list (*start)();
};

constexpr std::array<TwoParameterRow, 9> two_parameter_rows = {{
  /* folds of algebraic problems */
  {xpp::problem_at_most_steady, {xpp::AutoxPointKind::BranchPoint, xpp::AutoxPointKind::Fold},
   [] { return algebraic(fnlp, stpnlp); }},
  {[](xpp::AutoxProblem p) { return p == xpp::AutoxProblem::DiscreteMap; },
   {xpp::AutoxPointKind::PeriodDoubling, xpp::AutoxPointKind::Regular}, [] { return algebraic(fnlp, stpnlp); }},
  /* Hopf bifurcations of ODEs, waves and maps (a map's is its torus) */
  {[](xpp::AutoxProblem p) { return p == xpp::AutoxProblem::Algebraic || p == xpp::AutoxProblem::SteadyState; },
   {xpp::AutoxPointKind::Hopf, xpp::AutoxPointKind::Regular}, [] { return algebraic(fnhb, stpnhb); }},
  {[](xpp::AutoxProblem p) { return p == xpp::AutoxProblem::WaveSteady; },
   {xpp::AutoxPointKind::Hopf, xpp::AutoxPointKind::Regular}, [] { return algebraic(fnhw, stpnhw); }},
  {[](xpp::AutoxProblem p) { return p == xpp::AutoxProblem::DiscreteMap; },
   {xpp::AutoxPointKind::NeimarkSacker, xpp::AutoxPointKind::Regular}, [] { return algebraic(fnhb, stpnhb); }},
  /* folds, period doublings and tori of periodic orbits */
  {[](xpp::AutoxProblem p) { return p == xpp::AutoxProblem::Periodic; },
   {xpp::AutoxPointKind::CycleFold, xpp::AutoxPointKind::CycleBranch}, [] { return boundary(fnpl, bcpl, icpl, stpnpl); }},
  {[](xpp::AutoxProblem p) { return p == xpp::AutoxProblem::Periodic; },
   {xpp::AutoxPointKind::PeriodDoubling, xpp::AutoxPointKind::Regular}, [] { return boundary(fnpd, bcpd, icpd, stpnpd); }},
  {[](xpp::AutoxProblem p) { return p == xpp::AutoxProblem::Periodic; },
   {xpp::AutoxPointKind::NeimarkSacker, xpp::AutoxPointKind::Regular}, [] { return boundary(fntr, bctr, ictr, stpntr); }},
  /* folds of boundary value problems */
  {[](xpp::AutoxProblem p) { return p == xpp::AutoxProblem::BoundaryValue; },
   {xpp::AutoxPointKind::CycleFold, xpp::AutoxPointKind::CycleBranch}, [] { return boundary(fnbl, bcbl, icbl, stpnbl); }},
}};

std::optional<function_list> two_parameter(const iap_type &iap)
{
  if (std::abs(iap.isw) != 2) return std::nullopt;
  for (const TwoParameterRow &row : two_parameter_rows) {
    if (!row.problem(iap.ips)) continue;
    const auto any = [&](auto test) {
      for (const xpp::AutoxPointKind k : row.kinds)
        if (k != xpp::AutoxPointKind::Regular && test(k)) return true;
      return false;
    };
    if (any([&](xpp::AutoxPointKind k) { return iap.itp.is(k); })) return row.start();
    if (any([&](xpp::AutoxPointKind k) { return iap.itp.started_from(k); })) {
      function_list list = row.start();
      if (list.solver == AutoxSolver::Algebraic) list.aelist.stpnt = stpnae;
      else list.bvlist.stpnt = stpnbv;
      return list;
    }
  }
  return std::nullopt;
}

/* the run's problem functions; a combination autoX has none for is an error */
function_list problem_functions(const iap_type &iap)
{
  const auto named = [](xpp::AutoxPointCode c) {
    const std::string_view name = xpp::autox_point_kind_name(c.kind);
    return name == "regular" ? std::string("a regular point") : "a point of type " + std::string(name);
  };
  const auto &s=*iap.run->session;
  if(std::abs(iap.isw)==2 && iap.run->params.icp[0]==iap.run->params.icp[1])
    xpp::autox_fail(xpp::format("Two-parameter start has the same parameter \"{}\" on both axes; select two different parameters",xpp::autox_par_name(s,s.autox_state.axes().icp1)));
  /* the restart label's stored point decides whether it is a periodic orbit (its branch's sign, get_bif_type):
     a periodic branch's end point restarts a periodic run; a steady state needs a Hopf point */
  const xpp::DIAGRAM *label=iap.irs>0?diagram_of_label(s,iap.irs):nullptr;
  if(label && iap.ips==xpp::AutoxProblem::Periodic && std::abs(iap.isw)!=2 &&
     !xpp::point_is_periodic(xpp::get_bif_type(label->ibr,label->ntot)) &&
     !iap.itp.is(xpp::AutoxPointKind::Hopf) && !iap.itp.started_from(xpp::AutoxPointKind::Hopf))
    xpp::autox_fail(xpp::format("Restart label {} has kind {}; a periodic run needs a hopf point or a periodic orbit; select one of those points",iap.irs,named(iap.itp)));
  if (auto list = one_parameter(iap)) return *list;
  if (auto list = two_parameter(iap)) return *list;
  const char *continuation = std::abs(iap.isw) == 2 ? "a two-parameter continuation (ISW 2)"
                             : iap.isw < 0          ? "a branch switch (ISW -1)"
                                                    : "a one-parameter continuation (ISW 1)";
  xpp::autox_fail(xpp::format("Restart label {}: autoX has no {} of {} from {}{}; select a point of a kind supported by the run (two-parameter starts need a fold, Hopf, period doubling, torus or branch point)", iap.irs, continuation,
                             xpp::autox_problem_description(iap.ips), named(iap.itp),
                             iap.itp.start == xpp::AutoxPointKind::Regular ? ""
                                 : ", continued from " + named({iap.itp.start, xpp::AutoxPointKind::Regular})));
}
} // namespace

/* one autoX run: the problem's functions, then the continuation; a failure
   is thrown as xpp::AutoxFailed (autox_state.h), which do_autox shows */
void go_go_autox(AutoxRun &run)
{
  xpp::Session &s=*run.session;
  run.parameter_count = s.autox_state.parameter_layout.size();
  std::vector<integer> icp(xpp::size_product(2, run.parameter_count));
  std::vector<doublereal> par(xpp::size_product(2, run.parameter_count));
  std::vector<doublereal> thl(run.parameter_count);
  std::vector<doublereal> thu;
  std::vector<AutoxMark> marks(run.params.marks.size());
  iap_type iap;
  rap_type rap;
  RunUnits units(run, run.params.irs > 0);
  iap.run = &run;

  init(&iap, &rap, par.data(), icp.data(), thl.data(), thu, marks);

  /* the restart label, and the type of its point */
  if (iap.irs > 0) {
    bool found = false;
    findlb(&iap, &rap, iap.irs, &(iap.nfpr), &found);
    if (!found)
      xpp::autox_fail(xpp::format("autoX: restart label {} is not in {}", iap.irs, xpp::autox_fort_path(run, 3)));
  }
  const function_list list = problem_functions(iap);
  init1(&iap, &rap, icp.data(), par.data());
  chdim(&iap);
  if (list.solver == AutoxSolver::BoundaryValue)
    xpp::AutoxParallel::select_run(run, iap.ndim, iap.ntst, iap.ncol);
  else run.collocation.reset();

  /* the evaluator scratch (run.scratch) the run reuses in
     autox_lib3.cpp and autox_lib5.cpp, sized once */
  size_run_scratch(iap);

  if (list.solver == AutoxSolver::Algebraic)
    autoae(&iap, &rap, par.data(), icp.data(), list.aelist.funi, list.aelist.stpnt, list.aelist.pvli, thl.data(), thu.data(),
           marks);
  else
    autobv(&iap, &rap, par.data(), icp.data(), list.bvlist.funi, list.bvlist.bcni,
           list.bvlist.icni, list.bvlist.stpnt, list.bvlist.pvli, thl.data(), thu.data(), marks);
}
