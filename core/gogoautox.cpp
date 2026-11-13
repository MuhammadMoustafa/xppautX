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


problem_function_table algebraic(ProblemFunction problem_function, UserStartAlgebraic start)
{
  return {AutoxSolver::Algebraic, {}, {problem_function, start, output_parameters_algebraic}};
}

problem_function_table boundary(ProblemFunction problem_function, BoundaryFunction boundary_function, IntegralFunction integral_function, UserStartBoundary start, OutputParametersBoundary values = output_parameters_boundary)
{
  return {AutoxSolver::BoundaryValue, {problem_function, boundary_function, integral_function, start, values}, {}};
}

/* a one-parameter continuation's functions (abs(ISW) != 2, and the
   problems that have no other): the start from scratch (IRS 0) or from a
   label of the restart file */
std::optional<problem_function_table> one_parameter(const iap_type &iap)
{
  using xpp::AutoxProblem;
  using xpp::AutoxPointKind;
  const bool restart = iap.irs > 0;
  const bool two = std::abs(iap.isw) == 2;
  const auto ae = [&](ProblemFunction problem_function) { return algebraic(problem_function, restart ? algebraic_start : user_start_algebraic); };
  const auto bv = [&](ProblemFunction problem_function, BoundaryFunction boundary_function, IntegralFunction integral_function) {
    return boundary(problem_function, boundary_function, integral_function, restart ? boundary_start : user_start_boundary);
  };
  switch (iap.ips) {
  case AutoxProblem::Algebraic:
  case AutoxProblem::SteadyState:
    if (!two) return ae(problem_function);
    break;
  case AutoxProblem::WaveSteady: /* spatially homogeneous solutions */
    if (!two) return ae(wave_steady_equations);
    break;
  case AutoxProblem::DiscreteMap: /* fixed points */
    if (!two) return ae(discrete_map_equations);
    break;
  case AutoxProblem::TimeIntegration:
    return ae(time_integration_equations);
  case AutoxProblem::Periodic:
    if (two) break;
    if (iap.itp.is(AutoxPointKind::Hopf) || iap.itp.started_from(AutoxPointKind::Hopf))
      return boundary(periodic_equations, periodic_boundary_conditions, periodic_integral_conditions, periodic_starting_data);
    return bv(periodic_equations, periodic_boundary_conditions, periodic_integral_conditions);
  case AutoxProblem::WavePeriodic: /* wave trains of parabolic systems */
    if (two) break;
    if (iap.itp.is(AutoxPointKind::Hopf)) return boundary(wave_train_equations, periodic_boundary_conditions, periodic_integral_conditions, wave_train_starting_data);
    return bv(wave_train_equations, periodic_boundary_conditions, periodic_integral_conditions);
  case AutoxProblem::BoundaryValue:
  case AutoxProblem::FloquetBoundaryValue:
    if (!two) return bv(problem_function, boundary_function, integral_function);
    break;
  case AutoxProblem::Homoclinic:
    if (!two) return boundary(homoclinic_equations, homoclinic_boundary_conditions, homoclinic_integral_conditions, restart ? boundary_start : homoclinic_starting_data, homoclinic_output_parameters);
    break;
  case AutoxProblem::ParabolicPeriodicEvolution:
    return bv(parabolic_evolution_equations, periodic_boundary_conditions, parabolic_evolution_integral_conditions);
  case AutoxProblem::PeriodicOptimization:
    if (std::abs(iap.isw) == 1) return boundary(periodic_optimization_equations, periodic_optimization_boundary_conditions, periodic_optimization_integral_conditions, iap.nfpr < 6 ? periodic_optimization_starting_data : boundary_start);
    break;
  case AutoxProblem::ParabolicUserEvolution:
    return bv(parabolic_evolution_equations, boundary_function, parabolic_evolution_integral_conditions);
  case AutoxProblem::ParabolicUserSteady:
    return bv(parabolic_stationary_equations, boundary_function, parabolic_evolution_integral_conditions);
  case AutoxProblem::AlgebraicOptimization: {
    const bool from_fold = iap.itp.kind == AutoxPointKind::Fold;
    if (iap.nfpr + (from_fold || !restart ? 1 : 0) == 2) return algebraic(optimization_single_equations, restart ? algebraic_start : optimization_single_starting_data);
    return algebraic(optimization_multi_equations, from_fold ? optimization_multi_starting_data : algebraic_start);
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
  problem_function_table (*start)();
};

constexpr std::array<TwoParameterRow, 9> two_parameter_rows = {{
  /* folds of algebraic problems */
  {xpp::problem_at_most_steady, {xpp::AutoxPointKind::BranchPoint, xpp::AutoxPointKind::Fold},
   [] { return algebraic(fold_equations, fold_starting_data); }},
  {[](xpp::AutoxProblem p) { return p == xpp::AutoxProblem::DiscreteMap; },
   {xpp::AutoxPointKind::PeriodDoubling, xpp::AutoxPointKind::Regular}, [] { return algebraic(fold_equations, fold_starting_data); }},
  /* Hopf bifurcations of ODEs, waves and maps (a map's is its torus) */
  {[](xpp::AutoxProblem p) { return p == xpp::AutoxProblem::Algebraic || p == xpp::AutoxProblem::SteadyState; },
   {xpp::AutoxPointKind::Hopf, xpp::AutoxPointKind::Regular}, [] { return algebraic(hopf_equations, hopf_starting_data); }},
  {[](xpp::AutoxProblem p) { return p == xpp::AutoxProblem::WaveSteady; },
   {xpp::AutoxPointKind::Hopf, xpp::AutoxPointKind::Regular}, [] { return algebraic(hopf_wave_equations, hopf_wave_starting_data); }},
  {[](xpp::AutoxProblem p) { return p == xpp::AutoxProblem::DiscreteMap; },
   {xpp::AutoxPointKind::NeimarkSacker, xpp::AutoxPointKind::Regular}, [] { return algebraic(hopf_equations, hopf_starting_data); }},
  /* folds, period doublings and tori of periodic orbits */
  {[](xpp::AutoxProblem p) { return p == xpp::AutoxProblem::Periodic; },
   {xpp::AutoxPointKind::CycleFold, xpp::AutoxPointKind::CycleBranch}, [] { return boundary(periodic_fold_equations, periodic_fold_boundary_conditions, periodic_fold_integral_conditions, periodic_fold_starting_data); }},
  {[](xpp::AutoxProblem p) { return p == xpp::AutoxProblem::Periodic; },
   {xpp::AutoxPointKind::PeriodDoubling, xpp::AutoxPointKind::Regular}, [] { return boundary(period_doubling_equations, period_doubling_boundary_conditions, period_doubling_integral_conditions, period_doubling_starting_data); }},
  {[](xpp::AutoxProblem p) { return p == xpp::AutoxProblem::Periodic; },
   {xpp::AutoxPointKind::NeimarkSacker, xpp::AutoxPointKind::Regular}, [] { return boundary(torus_equations, torus_boundary_conditions, torus_integral_conditions, torus_starting_data); }},
  /* folds of boundary value problems */
  {[](xpp::AutoxProblem p) { return p == xpp::AutoxProblem::BoundaryValue; },
   {xpp::AutoxPointKind::CycleFold, xpp::AutoxPointKind::CycleBranch}, [] { return boundary(boundary_fold_equations, boundary_fold_boundary_conditions, boundary_fold_integral_conditions, boundary_fold_starting_data); }},
}};

std::optional<problem_function_table> two_parameter(const iap_type &iap)
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
      problem_function_table list = row.start();
      if (list.solver == AutoxSolver::Algebraic) list.aelist.user_start = algebraic_start;
      else list.bvlist.user_start = boundary_start;
      return list;
    }
  }
  return std::nullopt;
}

/* the run's problem functions; a combination autoX has none for is an error */
problem_function_table problem_functions(const iap_type &iap)
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
/* go_go_autox: gogoautox.cpp: builds the problem's function table and runs it */
void run_autox_problem(AutoxRun &run)
{
  xpp::Session &s=*run.session;
  run.parameter_count = s.autox_state.parameter_layout.size();
  std::vector<std::ptrdiff_t> icp(xpp::size_product(2, run.parameter_count));
  std::vector<double> par(xpp::size_product(2, run.parameter_count));
  std::vector<double> thl(run.parameter_count);
  std::vector<double> thu;
  std::vector<AutoxMark> marks(run.params.marks.size());
  iap_type iap;
  rap_type rap;
  RunUnits units(run, run.params.irs > 0);
  iap.run = &run;

  initialize_run(&iap, &rap, par.data(), icp.data(), thl.data(), thu, marks);

  /* the restart label, and the type of its point */
  if (iap.irs > 0) {
    bool found = false;
    find_restart_label(&iap, &rap, iap.irs, &(iap.nfpr), &found);
    if (!found)
      xpp::autox_fail(xpp::format("autoX: restart label {} is not in {}", iap.irs, xpp::autox_fort_path(run, 3)));
  }
  const problem_function_table list = problem_functions(iap);
  initialize_constants(&iap, &rap, icp.data(), par.data());
  check_dimensions(&iap);
  if (list.solver == AutoxSolver::BoundaryValue)
    xpp::AutoxParallel::select_run(run, iap.ndim, iap.ntst, iap.ncol);
  else run.collocation.reset();

  /* the evaluator scratch (run.scratch) the run reuses in
     autox_lib3.cpp and autox_lib5.cpp, sized once */
  size_run_scratch(iap);

  if (list.solver == AutoxSolver::Algebraic)
    continue_algebraic_problem(&iap, &rap, par.data(), icp.data(), list.aelist.problem_function, list.aelist.user_start, list.aelist.pvli, thl.data(), thu.data(),
           marks);
  else
    continue_boundary_problem(&iap, &rap, par.data(), icp.data(), list.bvlist.problem_function, list.bvlist.boundary_function,
           list.bvlist.integral_function, list.bvlist.user_start, list.bvlist.pvli, thl.data(), thu.data(), marks);
}
