#include "xpp_mem.h"

#include <array>
#include <cstdlib>
#include <memory>
#include <optional>
#include <vector>
#include "xpp_log.h"
#include "session.h"
#include "autevd.h" /* xAuto (its own extern) */
#include "auto_nox.h" /* xpp::auto_fort_path() */
#include "diagram.h" /* diagram_of_label */


namespace {
/* AUTO's fort.3 (its restart data: read, or written from scratch), fort.7
   and fort.9 for one run, opened where open_auto put them, and the
   Session whose run it is: AUTO reads and writes them through its
   AutoLib (auto_state.h), which the run's iap_type points at. Closed on
   every way out of the run (Windows cannot rename or delete an open
   file, which close_auto does next), the pointers cleared first. */
struct RunUnits {
  AutoLib &lib;
  xpp::UniqueFile u3,u7,u9;
  RunUnits(xpp::Session &s, bool restart)
    : lib(s.auto_lib)
  {
    const auto open = [&s](int unit, const char *mode) {
      const std::string path=xpp::auto_fort_path(s,unit);
      xpp::UniqueFile file(xpp::files::open_stream(path,mode));
      if(!file)xpp::auto_fail((mode[0]=='r' ? xpp::files::open_error("AUTO",path)
                                         : xpp::files::write_error("AUTO",path)));
      return file;
    };
    u3=open(3,restart?"r":"w+");
    u7=open(7,"w");
    u9=open(9,"w");
    lib.session=&s;
    lib.fp3=u3.get();
    lib.fp7=u7.get();
    lib.fp9=u9.get();
  }
  ~RunUnits() { lib.fp3=lib.fp7=lib.fp9=nullptr; lib.session=nullptr; }
  RunUnits(const RunUnits &)=delete;
  RunUnits &operator=(const RunUnits &)=delete;
};

using AeStart = STPNT_TYPE_AE((*));
using BvStart = STPNT_TYPE_BVP((*));
using BvValues = PVLI_TYPE_BVP((*));

function_list algebraic(FUNI_TYPE((*funi)), AeStart start)
{
  return {AutoSolver::Algebraic, {}, {funi, start, pvlsae}};
}

function_list boundary(FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), BvStart start, BvValues values = pvlsbv)
{
  return {AutoSolver::BoundaryValue, {funi, bcni, icni, start, values}, {}};
}

/* a one-parameter continuation's functions (abs(ISW) != 2, and the
   problems that have no other): the start from scratch (IRS 0) or from a
   label of the restart file */
std::optional<function_list> one_parameter(const iap_type &iap)
{
  using xpp::AutoProblem;
  using xpp::AutoPointKind;
  const bool restart = iap.irs > 0;
  const bool two = std::abs(iap.isw) == 2;
  const auto ae = [&](FUNI_TYPE((*funi))) { return algebraic(funi, restart ? stpnae : stpnus); };
  const auto bv = [&](FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni))) {
    return boundary(funi, bcni, icni, restart ? stpnbv : stpnub);
  };
  switch (iap.ips) {
  case AutoProblem::Algebraic:
  case AutoProblem::SteadyState:
    if (!two) return ae(funi);
    break;
  case AutoProblem::WaveSteady: /* spatially homogeneous solutions */
    if (!two) return ae(fnws);
    break;
  case AutoProblem::DiscreteMap: /* fixed points */
    if (!two) return ae(fnds);
    break;
  case AutoProblem::TimeIntegration:
    return ae(fnti);
  case AutoProblem::Periodic:
    if (two) break;
    if (iap.itp.is(AutoPointKind::Hopf) || iap.itp.started_from(AutoPointKind::Hopf))
      return boundary(fnps, bcps, icps, stpnps);
    return bv(fnps, bcps, icps);
  case AutoProblem::WavePeriodic: /* wave trains of parabolic systems */
    if (two) break;
    if (iap.itp.is(AutoPointKind::Hopf)) return boundary(fnwp, bcps, icps, stpnwp);
    return bv(fnwp, bcps, icps);
  case AutoProblem::BoundaryValue:
  case AutoProblem::FloquetBoundaryValue:
    if (!two) return bv(funi, bcni, icni);
    break;
  case AutoProblem::Homoclinic:
    if (!two) return boundary(fnho, bcho, icho, restart ? stpnbv : stpnho, pvlsho);
    break;
  case AutoProblem::ParabolicPeriodicEvolution:
    return bv(fnpe, bcps, icpe);
  case AutoProblem::PeriodicOptimization:
    if (std::abs(iap.isw) == 1) return boundary(fnpo, bcpo, icpo, iap.nfpr < 6 ? stpnpo : stpnbv);
    break;
  case AutoProblem::ParabolicUserEvolution:
    return bv(fnpe, bcni, icpe);
  case AutoProblem::ParabolicUserSteady:
    return bv(fnsp, bcni, icpe);
  case AutoProblem::AlgebraicOptimization: {
    const bool from_fold = iap.itp.kind == AutoPointKind::Fold;
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
  bool (*problem)(xpp::AutoProblem);
  std::array<xpp::AutoPointKind, 2> kinds; /* the second Regular for one kind */
  function_list (*start)();
};

constexpr std::array<TwoParameterRow, 9> two_parameter_rows = {{
  /* folds of algebraic problems */
  {xpp::problem_at_most_steady, {xpp::AutoPointKind::BranchPoint, xpp::AutoPointKind::Fold},
   [] { return algebraic(fnlp, stpnlp); }},
  {[](xpp::AutoProblem p) { return p == xpp::AutoProblem::DiscreteMap; },
   {xpp::AutoPointKind::PeriodDoubling, xpp::AutoPointKind::Regular}, [] { return algebraic(fnlp, stpnlp); }},
  /* Hopf bifurcations of ODEs, waves and maps (a map's is its torus) */
  {[](xpp::AutoProblem p) { return p == xpp::AutoProblem::Algebraic || p == xpp::AutoProblem::SteadyState; },
   {xpp::AutoPointKind::Hopf, xpp::AutoPointKind::Regular}, [] { return algebraic(fnhb, stpnhb); }},
  {[](xpp::AutoProblem p) { return p == xpp::AutoProblem::WaveSteady; },
   {xpp::AutoPointKind::Hopf, xpp::AutoPointKind::Regular}, [] { return algebraic(fnhw, stpnhw); }},
  {[](xpp::AutoProblem p) { return p == xpp::AutoProblem::DiscreteMap; },
   {xpp::AutoPointKind::Torus, xpp::AutoPointKind::Regular}, [] { return algebraic(fnhb, stpnhb); }},
  /* folds, period doublings and tori of periodic orbits */
  {[](xpp::AutoProblem p) { return p == xpp::AutoProblem::Periodic; },
   {xpp::AutoPointKind::PeriodicFold, xpp::AutoPointKind::PeriodicBranch}, [] { return boundary(fnpl, bcpl, icpl, stpnpl); }},
  {[](xpp::AutoProblem p) { return p == xpp::AutoProblem::Periodic; },
   {xpp::AutoPointKind::PeriodDoubling, xpp::AutoPointKind::Regular}, [] { return boundary(fnpd, bcpd, icpd, stpnpd); }},
  {[](xpp::AutoProblem p) { return p == xpp::AutoProblem::Periodic; },
   {xpp::AutoPointKind::Torus, xpp::AutoPointKind::Regular}, [] { return boundary(fntr, bctr, ictr, stpntr); }},
  /* folds of boundary value problems */
  {[](xpp::AutoProblem p) { return p == xpp::AutoProblem::BoundaryValue; },
   {xpp::AutoPointKind::PeriodicFold, xpp::AutoPointKind::PeriodicBranch}, [] { return boundary(fnbl, bcbl, icbl, stpnbl); }},
}};

std::optional<function_list> two_parameter(const iap_type &iap)
{
  if (std::abs(iap.isw) != 2) return std::nullopt;
  for (const TwoParameterRow &row : two_parameter_rows) {
    if (!row.problem(iap.ips)) continue;
    const auto any = [&](auto test) {
      for (const xpp::AutoPointKind k : row.kinds)
        if (k != xpp::AutoPointKind::Regular && test(k)) return true;
      return false;
    };
    if (any([&](xpp::AutoPointKind k) { return iap.itp.is(k); })) return row.start();
    if (any([&](xpp::AutoPointKind k) { return iap.itp.started_from(k); })) {
      function_list list = row.start();
      if (list.solver == AutoSolver::Algebraic) list.aelist.stpnt = stpnae;
      else list.bvlist.stpnt = stpnbv;
      return list;
    }
  }
  return std::nullopt;
}

/* the run's problem functions; a combination AUTO has none for is an error */
function_list problem_functions(const iap_type &iap)
{
  const auto named = [](xpp::AutoPointCode c) {
    const std::string symbol = xpp::auto_point_symbol(c);
    return symbol == "  " ? std::string("a regular point") : "a point of type " + symbol;
  };
  const auto &s=*iap.lib->session;
  if(std::abs(iap.isw)==2 && s.auto_state.run.icp[0]==s.auto_state.run.icp[1])
    xpp::auto_fail(xpp::format("Two-parameter start has the same parameter \"{}\" on both axes; select two different parameters",xpp::auto_par_name(s,s.auto_state.axes().icp1)));
  /* the restart label's stored point decides whether it is a periodic orbit (its branch's sign, get_bif_type):
     a periodic branch's end point restarts a periodic run; a steady state needs a Hopf point */
  const xpp::DIAGRAM *label=iap.irs>0?diagram_of_label(s,iap.irs):nullptr;
  if(label && iap.ips==xpp::AutoProblem::Periodic && std::abs(iap.isw)!=2 &&
     !xpp::point_is_periodic(xpp::get_bif_type(label->ibr,label->ntot)) &&
     !iap.itp.is(xpp::AutoPointKind::Hopf) && !iap.itp.started_from(xpp::AutoPointKind::Hopf))
    xpp::auto_fail(xpp::format("Restart label {} has kind {}; a periodic run needs a Hopf point (HB) or a periodic orbit; select one of those points",iap.irs,named(iap.itp)));
  if (auto list = one_parameter(iap)) return *list;
  if (auto list = two_parameter(iap)) return *list;
  const char *continuation = std::abs(iap.isw) == 2 ? "a two-parameter continuation (ISW 2)"
                             : iap.isw < 0          ? "a branch switch (ISW -1)"
                                                    : "a one-parameter continuation (ISW 1)";
  xpp::auto_fail(xpp::format("Restart label {}: AUTO has no {} of {} from {} (ITP {}){}; select a point of a kind supported by the run (two-parameter starts need a fold, Hopf, period doubling, torus or branch point)", iap.irs, continuation,
                             xpp::auto_problem_description(iap.ips), named(iap.itp), xpp::auto_point_encode(iap.itp),
                             iap.itp.start == xpp::AutoPointKind::Regular ? ""
                                 : ", continued from " + named({iap.itp.start, xpp::AutoPointKind::Regular})));
}
} // namespace

/* one AUTO run: the problem's functions, then the continuation; a failure
   is thrown as xpp::AutoFailed (auto_state.h), which do_auto shows */
void go_go_auto(xpp::Session &s)
{
  s.auto_lib.parameter_count = s.auto_state.parameter_layout.size();
  std::vector<integer> icp(xpp::size_product(2, s.auto_lib.parameter_count));
  std::vector<doublereal> par(xpp::size_product(2, s.auto_lib.parameter_count));
  std::vector<doublereal> thl(s.auto_lib.parameter_count);
  std::vector<doublereal> thu;
  std::vector<AutoMark> marks(s.auto_state.run.marks.size());
  iap_type iap;
  rap_type rap;
  RunUnits units(s, s.auto_state.run.irs > 0);
  iap.lib = &s.auto_lib;

  init(&iap, &rap, par.data(), icp.data(), thl.data(), thu, marks);

  /* the restart label, and the type of its point */
  if (iap.irs > 0) {
    bool found = false;
    findlb(&iap, &rap, iap.irs, &(iap.nfpr), &found);
    if (!found)
      xpp::auto_fail(xpp::format("AUTO: restart label {} is not in {}", iap.irs, xpp::auto_fort_path(s, 3)));
  }
  const function_list list = problem_functions(iap);
  init1(&iap, &rap, icp.data(), par.data());
  chdim(&iap);
  if (list.solver == AutoSolver::BoundaryValue)
    xpp::AutoParallel::select_run(s.auto_lib, iap.ndim, iap.ntst, iap.ncol);
  else s.auto_lib.collocation.reset();

  /* the evaluator scratch (s.auto_lib.scratch) the run reuses in
     autlib3.cpp and autlib5.cpp, sized once */
  size_run_scratch(iap);

  if (list.solver == AutoSolver::Algebraic)
    autoae(&iap, &rap, par.data(), icp.data(), list.aelist.funi, list.aelist.stpnt, list.aelist.pvli, thl.data(), thu.data(),
           marks);
  else
    autobv(&iap, &rap, par.data(), icp.data(), list.bvlist.funi, list.bvlist.bcni,
           list.bvlist.icni, list.bvlist.stpnt, list.bvlist.pvli, thl.data(), thu.data(), marks);
}
