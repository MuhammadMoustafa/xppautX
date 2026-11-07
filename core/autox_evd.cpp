#include "autox_unknowns.h"
#include <algorithm>
#include <vector> /* first: autox_f2c.h's min/max macros (pulled in below) break libstdc++ headers if not included first */
#include "autox_evd.h"
#include "session.h"
#include <cstdlib>
#include <math.h>
#include <stdio.h>

#include "diagram.h"
#include "autox_c.h"
#include "xpp_session_autox.h"

#include "autox_stability.h"
#include "model_files.h"
#include "xpp_ui.h"
#include "xpp_math.h"
#include "xpp_job.h"
#include "xpp_log.h"
#include "equilibria.h"
#include "numerics_settings.h"

namespace xpp {

/* The most points a session's diagram holds: the bound where autoX's memory
   grows (NMX bounds one branch's loop only, and models set 10^7 to mean "no
   limit"); it is also what a saved session accepts back.
   XPP_DIAGRAM_POINTS_LIMIT lowers it, so servercheck reaches it without a
   million points (the way XPP_STORAGE_FAIL_GROW reaches a full store). */
std::size_t autox_diagram_points_limit()
{
  const char *lowered=std::getenv("XPP_DIAGRAM_POINTS_LIMIT");
  return lowered ? static_cast<std::size_t>(std::strtoull(lowered,nullptr,10)) : snapx::autox_members::saved_points_limit;
}

/* The most jobs a run of branch jobs makes, which a recording's jobs list may not exceed (ui_json.cpp). Jobs come from two
   places only (autox_jobs.cpp): the two roots, and expand, which makes the starts a job's points give, at most two per
   point (a branch point's two directions, a cycle branch's two equilibria: autox_found_starts). The points the jobs hold
   come to the diagram's limit, and the point that reaches it is held before the run stops (point_stored counts each point
   the jobs hold against the limit). A threaded run's jobs still running at that point may store a few more before they
   see the stop, but no job is expanded once the run is fatal (autox_jobs.cpp expand), so those make no jobs. A key has one element per generation, a job of the chain, so it is no
   longer than the jobs. */
std::size_t autox_recorded_jobs_limit()
{
  constexpr std::size_t roots=2; /* both directions of the run's start (autox_run_jobs) */
  constexpr std::size_t starts_per_point=2; /* see above */
  constexpr std::size_t reaching_point=1; /* held before the run stops */
  return roots+starts_per_point*(autox_diagram_points_limit()+reaching_point);
}

std::string autox_diagram_limit_message()
{
  return xpp::format("the diagram holds {} points, the most a session keeps; the points so far are kept (clear the diagram or start a new one)", autox_diagram_points_limit());
}

std::optional<std::string> autox_diagram_full(std::size_t points)
{
  if(points<autox_diagram_points_limit())return std::nullopt;
  return autox_diagram_limit_message();
}

[[noreturn]] void autox_nonfinite(const iap_type &iap, integer point)
{
  autox_fail(xpp::format("Branch {} point {}: nonfinite autoX solution", iap.ibr, point));
}

void autox_check_start_parameters(const iap_type &iap, const rap_type &rap, const double *par, const integer *icp)
{
  const Session &s=*iap.run->session;
  const int k=s.autox_state.parameter_layout.model_index(static_cast<int>(icp[0]));
  if(k<0)return; // autoX's period is not a model parameter; autoX bounds only the principal parameter.
  const double value=par[icp[0]];
  /* outside or on a bound, a start is refused only when Ds leads away from
     the interval: extending a branch that ended just past a bound goes back in */
  if(autox_start_points_away(value,rap.rl0,rap.rl1,rap.ds))
    autox_fail(Error{"autox",xpp::format("Start parameter \"{}\" = {} is at or outside [parameter_min = {}, parameter_max = {}] with step_size = {} pointing away; change the start, the bounds or the sign of step_size",
        s.model().upar_names[k],value,rap.rl0,rap.rl1,rap.ds),command_place()});
}

void autox_check_start_norm(const iap_type &iap, const rap_type &rap, double norm)
{
  if(!std::isfinite(norm) || norm<rap.a0 || norm>rap.a1)
    autox_fail(Error{"autox",xpp::format("Start norm = {} is outside [norm_min = {}, norm_max = {}]; change the start or widen the norm bounds",norm,rap.a0,rap.a1),command_place()});
}

/* A steady-state start from initial conditions (irs 0) is refined by Newton
   from the run's own parameters, as Sing pts refines an equilibrium, when its
   residual is above EPSU; u is the refined start, or the start as given when
   Newton does not converge (then the run is refused). */
void autox_check_start_equilibrium(const iap_type &iap, const rap_type &rap, double *u, const double *par, const integer *icp)
{
  if(iap.irs!=0 || iap.ips!=AutoxProblem::SteadyState)return;
  Session &s=*iap.run->session;
  const int n=s.model().node;
  if(n<=0)return;
  std::vector<double> residual(static_cast<std::size_t>(n));
  if(iap.run->refined_start){ /* the jobs' start, refined once before they began (autox_jobs.cpp) */
    std::copy(iap.run->refined_start->begin(),iap.run->refined_start->end(),u);
    iap.run->stop.start_refined=true;
    return;
  }
  func(*iap.run,iap.ndm,u,icp,iap.nfpr,par,0,residual.data(),nullptr,nullptr); // the residual of the start, at the run's parameters
  const int worst=xpp::worst_residual(residual.data(),n);
  const double start_value=std::abs(residual[worst]);
  if(std::isfinite(start_value) && start_value<=rap.epsu)return;

  const auto &eq=s.numerics.equilibria;
  std::vector<double> start(u,u+n);
  std::vector<double> work(static_cast<std::size_t>(n*n+2*n));
  xpp::NewtonRun newton;
  if(auto r=xpp::rooter(s,u,eq.newton_tolerance,eq.jacobian_step,eq.search_limit,work.data(),newton,eq.max_iterations,n);!r)
    autox_fail(r.error());
  if(newton.end!=xpp::NewtonEnd::converged){
    std::copy(start.begin(),start.end(),u);
    const std::string &name=s.model().uvar_names[worst];
    const std::string why=xpp::newton_cause(s,newton,static_cast<std::size_t>(n)).why;
    autox_fail(Error{"autox",xpp::format("The initial conditions are not a steady state, and Newton's method from them did not converge ({}); equation \"{}\" has the largest residual |f(x,p)| = {}, above state_tolerance = {}. Newton settings: {}. Use Sing pts or integrate to a steady state before starting autoX",
        why,name,xpp::number(start_value),xpp::number(rap.epsu),xpp::named_settings(s,{"newton_tolerance","jacobian_step","search_limit","max_iterations"})),model_place(s.model(),name)});
  }

  func(*iap.run,iap.ndm,u,icp,iap.nfpr,par,0,residual.data(),nullptr,nullptr);
  const double refined=std::abs(residual[xpp::worst_residual(residual.data(),n)]);
  iap.run->stop.start_refined=true;
  xpp::log(XPP_LOG_INFO,"autoX start is not a steady state: equation \"{}\" had the largest residual |f(x,p)| = {}; Newton's method refined it to {} before the run\n",s.model().uvar_names[worst],xpp::number(start_value),xpp::number(refined));
}

void init_autox(const AutoxState &st, XAUTOX &run, int ndim, bool exact)
{
  const xpp::BIFUR &b=st.bifur;
  /* here are the constants that we do not allow the user to change */
  int nnbc;
  run.iad=st.advanced.iad;   
  run.iplt=0;
  run.mxbf=st.advanced.switch_one_direction ? -st.advanced.max_branch_switches : st.advanced.max_branch_switches;
  run.iid=st.advanced.iid;
  run.itmx=st.advanced.itmx;
  run.itnw=st.advanced.itnw;
  run.nwtn=st.advanced.nwtn;
  run.jac=exact?autox_jac_supplied:autox_jac_differences;
  run.iads=st.advanced.iads;
  run.weights={{xpp::autox_unknown::period,0.0}};
  run.nunstab=st.homo_equilibria.nunstab;
  run.nstab=st.homo_equilibria.nstab;
  run.iequib=st.homo_equilibria.iequib;
  run.nint=0;    
  
  if(b.ips==AutoxProblem::BoundaryValue)
    nnbc=ndim;
  else
    nnbc=0;
  run.ndim=ndim;
  run.nbc=nnbc; 
  run.ips=b.ips;
  run.irs=b.irs;
  run.ilp=b.ilp;
  run.nicp=b.nfpar;
  run.icp.resize(std::max(5, b.nfpar));
  run.icp[0]=st.parameter_layout.solver_index(st.axes().icp1);
  run.icp[1]=st.axes().icp2<st.parameter_layout.model_parameters ? st.parameter_layout.solver_index(st.axes().icp2) : xpp::autox_unknown::period;
  run.icp[2]=st.parameter_layout.solver_index(b.icp3);
  run.icp[3]=st.parameter_layout.solver_index(b.icp4);
  run.icp[4]=st.parameter_layout.solver_index(b.icp5);
  run.ntst=b.ntst;
  run.ncol=b.ncol;
  run.isp=b.isp;
  run.isw=b.isw;
  run.nmx=b.nmx;
  run.rl0=b.rl0;
  run.rl1=b.rl1;
  run.a0=b.a0;
  run.a1=b.a1;
  run.npr=b.npr;

  run.epsl=b.epsl;
  run.epss=b.epss;
  run.epsu=b.epsu;
  run.ds=b.ds;
  run.dsmax=b.dsmax;
  run.dsmin=b.dsmin;

  run.marks.clear();
  for(const auto &mark:b.marks)
    run.marks.push_back({mark.period ? xpp::autox_unknown::period : st.parameter_layout.solver_index(mark.parameter),
                            mark.value});
 
}

/* Only unit 8,3 or q.prb is important; all others are unnecesary */

DiagramPointType get_bif_type(int ibr, int ntot)
{
  if(ibr<0&&ntot!=0)return ntot<0?STABLE_PERIODIC:UNSTABLE_PERIODIC;
  if(ibr>0&&ntot>0)return UNSTABLE_EQ;
  return STABLE_EQ;
}
void autox_store_point(Session &s, const DIAGRAM &p, bool new_segment)
{
  if(!s.autox_state.diag_flag){
    edit_start(s,p);
    s.autox_state.diag_flag=true;
  } else {
    add_diagram(s,p);
  }
  if(p.from)last_diagram(s.diagram)->from=p.from;

  /* plotted, and its stability shown, from the stored point, as a redraw
     or a grab shows it */
  const DIAGRAM *d=last_diagram(s.diagram);
  add_point(s,*d,get_bif_type(p.ibr,p.ntot),new_segment?0:1);
}

void autox_show_live_point(Session &s, const DIAGRAM &p, int live, bool new_segment)
{
  add_live_point(s,p,live,get_bif_type(p.ibr,p.ntot),new_segment?0:1);
}

bool autox_start_points_away(double value, double rl0, double rl1, double ds)
{
  return (value<=rl0 && ds<0) || (value>=rl1 && ds>0);
}

void addbif(iap_type *iap, rap_type *rap, integer ntots, integer ibrs, double *par,integer *icp,int lab, double *a, std::span<const double> uhigh, std::span<const double> ulow, std::span<const double> u0, std::span<const double> ubar)
{
  xpp::Session &s=*iap->run->session;
  int icp1=icp[0],icp2=icp[1],icp3=icp[2],icp4=icp[3];
  double per=par[xpp::autox_unknown::period];
  int n=iap->ndim;
  /* xppautX: the point's stability, or zeros: not computed (autox_stability.h) */
  xpp::DIAGRAM p{};
  p.size_variables(static_cast<size_t>(n));
  autox_stability_for(iap->run->stability,static_cast<int>(ibrs),static_cast<int>(ntots),p.evr,p.evi);

  // Guard the final stored quantities too, including derived norms and spectra.
  if (!xpp::all_finite({par, static_cast<size_t>(iap->run->parameter_count)}) ||
      !xpp::all_finite({a, 1}) || !xpp::all_finite({&per, 1}) ||
      !xpp::all_finite(uhigh) || !xpp::all_finite(ulow) || !xpp::all_finite(u0) || !xpp::all_finite(ubar) ||
      !xpp::all_finite(p.evr) || !xpp::all_finite(p.evi))
    autox_nonfinite(*iap, std::abs(ntots));

  AutoxRun &run=*iap->run;
  if(const auto full=autox_diagram_full(s.diagram.points.size()+run.points.size())){
    run.run_wide_error=true; /* the whole run is over: the queue stops every job */
    autox_fail(*full);
  }
  int from=autox_run_from_take(*iap->run);/* the run's first point says which label it started from */
  p.ibr=static_cast<int>(ibrs);
  p.ntot=static_cast<int>(ntots);
  p.itp=iap->itp;
  p.lab=lab;
  p.nfpar=static_cast<int>(iap->nfpr);
  p.norm=*a;
  p.uhi.assign(uhigh.begin(),uhigh.end());
  p.ulo.assign(ulow.begin(),ulow.end());
  p.u0.assign(u0.begin(),u0.end());
  p.ubar.assign(ubar.begin(),ubar.end());
  for(int k=0;k<iap->nfpr;k++){
    const int parameter=s.autox_state.parameter_layout.model_index(static_cast<int>(icp[k]));
    if(parameter>=0){
      p.parameter_indices.push_back(parameter);
      p.par.push_back(par[icp[k]]);
    }
  }
  p.per=per;
  p.icp1=s.autox_state.parameter_layout.axis(icp1);
  p.icp2=s.autox_state.parameter_layout.axis(icp2);
  p.icp3=s.autox_state.parameter_layout.axis(icp3);
  p.icp4=s.autox_state.parameter_layout.axis(icp4);
  p.flag2=s.autox_state.two_param;
  if (iap->itpst == AutoxPointKind::Hopf && iap->ndim == 2 * iap->ndm + 2)
    p.flag2=autox_hopf_curve_kind(u0[2 * iap->ndm]);
  p.jacobian=s.numerics.jacobian;
  if(run.job){
    /* a job keeps its points apart until the jobs are joined (autox_jobs.cpp), which draws them as they come in the meantime */
    p.from=from;
    diagram_record_branch(s.model(),autox_constants(run),run.points.empty()?nullptr:&run.points.back().point,p);
    run.points.push_back({std::move(p),iap->ntot==1});
    if(run.fail_after_points&&run.points.size()==*run.fail_after_points)
      autox_fail(xpp::format("a job failed for a check (XPP_AUTOX_FAIL_CHILD_AT={})",*run.fail_after_points));
    if(run.stop_after_points&&run.points.size()+1==*run.stop_after_points)run.stopped_alone=true; /* the next solve ends the branch */
    if(run.point_stored)run.point_stored(run.points.size());
    return;
  }
  diagram_record_branch(s.model(),s.parser.constants,last_diagram(s.diagram),p);
  p.from=from;
  autox_store_point(s,p,iap->ntot==1);
  xpp::job::report_point(static_cast<int>(std::abs(ibrs)),static_cast<int>(std::abs(ntots))); /* xppautX: where it got to (xpp_job.h) */
}


} // namespace xpp
