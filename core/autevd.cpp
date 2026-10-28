#include "auto_unknowns.h"
#include <algorithm>
#include <vector> /* first: auto_f2c.h's min/max macros (pulled in below) break libstdc++ headers if not included first */
#include "autevd.h"
#include "session.h"
#include <cstdlib>
#include <math.h>
#include <stdio.h>

#include "diagram.h"
#include "xpp_session_auto.h"

#include "auto_stability.h"
#include "model_files.h"
#include "xpp_ui.h"
#include "xpp_math.h"
#include "xpp_job.h"
#include "xpp_log.h"
#include "equilibria.h"
#include "numerics_settings.h"

namespace xpp {

/* The most points a session's diagram holds: the bound where AUTO's memory
   grows (NMX bounds one branch's loop only, and models set 10^7 to mean "no
   limit"); it is also what a saved session accepts back.
   XPP_DIAGRAM_POINTS_LIMIT lowers it, so servercheck reaches it without a
   million points (the way XPP_STORAGE_FAIL_GROW reaches a full store). */
static std::size_t diagram_points_limit()
{
  const char *lowered=std::getenv("XPP_DIAGRAM_POINTS_LIMIT");
  return lowered ? static_cast<std::size_t>(std::strtoull(lowered,nullptr,10)) : snapx::auto_members::saved_points_limit;
}

[[noreturn]] void auto_nonfinite(const iap_type &iap, integer point)
{
  auto_fail(xpp::format("Branch {} point {}: nonfinite AUTO solution", iap.ibr, point));
}

void auto_check_start_parameters(const iap_type &iap, const rap_type &rap, const double *par, const integer *icp)
{
  const Session &s=*iap.lib->session;
  const int k=s.auto_state.parameter_layout.model_index(static_cast<int>(icp[0]));
  if(k<0)return; // AUTO's period is not a model parameter; AUTO bounds only the principal parameter.
  const double value=par[icp[0]];
  /* outside or on a bound, a start is refused only when Ds leads away from
     the interval: extending a branch that ended just past a bound goes back in */
  if((value<=rap.rl0 && rap.ds<0) || (value>=rap.rl1 && rap.ds>0))
    auto_fail(Error{"auto",xpp::format("Start parameter \"{}\" = {} is at or outside [parameter_min = {}, parameter_max = {}] with step_size = {} pointing away; change the start, the bounds or the sign of step_size",
        s.model().upar_names[k],value,rap.rl0,rap.rl1,rap.ds),command_place()});
}

void auto_check_start_norm(const iap_type &iap, const rap_type &rap, double norm)
{
  if(!std::isfinite(norm) || norm<rap.a0 || norm>rap.a1)
    auto_fail(Error{"auto",xpp::format("Start norm = {} is outside [norm_min = {}, norm_max = {}]; change the start or widen the norm bounds",norm,rap.a0,rap.a1),command_place()});
}

/* A steady-state start from initial conditions (irs 0) is refined by Newton
   from the run's own parameters, as Sing pts refines an equilibrium, when its
   residual is above EPSU; u is the refined start, or the start as given when
   Newton does not converge (then the run is refused). */
void auto_check_start_equilibrium(const iap_type &iap, const rap_type &rap, double *u, const double *par, const integer *icp)
{
  if(iap.irs!=0 || iap.ips!=AutoProblem::SteadyState)return;
  Session &s=*iap.lib->session;
  const int n=s.model().node;
  if(n<=0)return;
  std::vector<double> residual(static_cast<std::size_t>(n));
  func(s,iap.ndm,u,icp,iap.nfpr,par,0,residual.data(),nullptr,nullptr); // also sets the constants to par
  const int worst=xpp::worst_residual(residual.data(),n);
  const double start_value=std::abs(residual[worst]);
  if(std::isfinite(start_value) && start_value<=rap.epsu)return;

  const auto &eq=s.numerics.equilibria;
  std::vector<double> start(u,u+n);
  std::vector<double> work(static_cast<std::size_t>(n*n+2*n));
  xpp::NewtonRun run;
  if(auto r=xpp::rooter(s,u,eq.newton_tolerance,eq.jacobian_step,eq.search_limit,work.data(),run,eq.max_iterations,n);!r)
    auto_fail(r.error());
  if(run.end!=xpp::NewtonEnd::converged){
    std::copy(start.begin(),start.end(),u);
    const std::string &name=s.model().uvar_names[worst];
    const std::string why=xpp::newton_cause(s,run,static_cast<std::size_t>(n)).why;
    auto_fail(Error{"auto",xpp::format("The initial conditions are not a steady state, and Newton's method from them did not converge ({}); equation \"{}\" has the largest residual |f(x,p)| = {}, above state_tolerance = {}. Newton settings: {}. Use Sing pts or integrate to a steady state before starting AUTO",
        why,name,xpp::number(start_value),xpp::number(rap.epsu),xpp::named_settings(s,{"newton_tolerance","jacobian_step","search_limit","max_iterations"})),model_place(s.model(),name)});
  }

  func(s,iap.ndm,u,icp,iap.nfpr,par,0,residual.data(),nullptr,nullptr);
  const double refined=std::abs(residual[xpp::worst_residual(residual.data(),n)]);
  s.auto_state.stop.start_refined=true;
  xpp::log(XPP_LOG_INFO,"AUTO start is not a steady state: equation \"{}\" had the largest residual |f(x,p)| = {}; Newton's method refined it to {} before the run\n",s.model().uvar_names[worst],xpp::number(start_value),xpp::number(refined));
}

void init_auto(AutoState &st, int ndim, bool exact)
{
  const xpp::BIFUR &b=st.bifur;
  /* here are the constants that we do not allow the user to change */
  int nnbc;
  st.run.iad=st.advanced.iad;   
  st.run.iplt=0;
  st.run.mxbf=st.advanced.switch_one_direction ? -st.advanced.max_branch_switches : st.advanced.max_branch_switches;
  st.run.iid=st.advanced.iid;
  st.run.itmx=st.advanced.itmx;
  st.run.itnw=st.advanced.itnw;
  st.run.nwtn=st.advanced.nwtn;
  st.run.jac=exact?auto_jac_supplied:auto_jac_differences;
  st.run.iads=st.advanced.iads;
  st.run.weights={{xpp::auto_unknown::period,0.0}};
  st.run.nint=0;    
  
  if(b.ips==AutoProblem::BoundaryValue)
    nnbc=ndim;
  else
    nnbc=0;
  st.run.ndim=ndim;
  st.run.nbc=nnbc; 
  st.run.ips=b.ips;
  st.run.irs=b.irs;
  st.run.ilp=b.ilp;
  st.run.nicp=b.nfpar;
  st.run.icp.resize(std::max(5, b.nfpar));
  st.run.icp[0]=st.parameter_layout.solver_index(st.axes().icp1);
  st.run.icp[1]=st.axes().icp2<st.parameter_layout.model_parameters ? st.parameter_layout.solver_index(st.axes().icp2) : xpp::auto_unknown::period;
  st.run.icp[2]=st.parameter_layout.solver_index(b.icp3);
  st.run.icp[3]=st.parameter_layout.solver_index(b.icp4);
  st.run.icp[4]=st.parameter_layout.solver_index(b.icp5);
  st.run.ntst=b.ntst;
  st.run.ncol=b.ncol;
  st.run.isp=b.isp;
  st.run.isw=b.isw;
  st.run.nmx=b.nmx;
  st.run.rl0=b.rl0;
  st.run.rl1=b.rl1;
  st.run.a0=b.a0;
  st.run.a1=b.a1;
  st.run.npr=b.npr;

  st.run.epsl=b.epsl;
  st.run.epss=b.epss;
  st.run.epsu=b.epsu;
  st.run.ds=b.ds;
  st.run.dsmax=b.dsmax;
  st.run.dsmin=b.dsmin;

  st.run.marks.clear();
  for(const auto &mark:b.marks)
    st.run.marks.push_back({mark.period ? xpp::auto_unknown::period : st.parameter_layout.solver_index(mark.parameter),
                            mark.value});
 
}

/* Only unit 8,3 or q.prb is important; all others are unnecesary */

DiagramPointType get_bif_type(int ibr, int ntot)
{
  if(ibr<0&&ntot!=0)return ntot<0?STABLE_PERIODIC:UNSTABLE_PERIODIC;
  if(ibr>0&&ntot>0)return UNSTABLE_EQ;
  return STABLE_EQ;
}
void addbif(iap_type *iap, rap_type *rap, integer ntots, integer ibrs, double *par,integer *icp,int lab, double *a, std::span<const double> uhigh, std::span<const double> ulow, std::span<const double> u0, std::span<const double> ubar)
{
  xpp::Session &s=*iap->lib->session;
  int icp1=icp[0],icp2=icp[1],icp3=icp[2],icp4=icp[3];
  double per=par[xpp::auto_unknown::period];
  int n=iap->ndim;
  const DiagramPointType type=get_bif_type(ibrs,ntots);
  /* xppautX: the point's stability, or zeros: not computed (auto_stability.h) */
  xpp::DIAGRAM p{};
  p.size_variables(static_cast<size_t>(n));
  auto_stability_for(s.auto_state.stability,static_cast<int>(ibrs),static_cast<int>(ntots),p.evr,p.evi);

  // Guard the final stored quantities too, including derived norms and spectra.
  if (!xpp::all_finite({par, static_cast<size_t>(iap->lib->parameter_count)}) ||
      !xpp::all_finite({a, 1}) || !xpp::all_finite({&per, 1}) ||
      !xpp::all_finite(uhigh) || !xpp::all_finite(ulow) || !xpp::all_finite(u0) || !xpp::all_finite(ubar) ||
      !xpp::all_finite(p.evr) || !xpp::all_finite(p.evi))
    auto_nonfinite(*iap, std::abs(ntots));

  if(const std::size_t limit=diagram_points_limit();s.diagram.points.size()>=limit)
    auto_fail(xpp::format("the diagram holds {} points, the most a session keeps; the points so far are kept (clear the diagram or start a new one)", limit));
  int from=auto_run_from_take(s);/* the run's first point says which label it started from */
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
    const int parameter=s.auto_state.parameter_layout.model_index(static_cast<int>(icp[k]));
    if(parameter>=0){
      p.parameter_indices.push_back(parameter);
      p.par.push_back(par[icp[k]]);
    }
  }
  p.per=per;
  p.icp1=s.auto_state.parameter_layout.axis(icp1);
  p.icp2=s.auto_state.parameter_layout.axis(icp2);
  p.icp3=s.auto_state.parameter_layout.axis(icp3);
  p.icp4=s.auto_state.parameter_layout.axis(icp4);
  p.flag2=s.auto_state.two_param;
  if (iap->itpst == AutoPointKind::Hopf && iap->ndim == 2 * iap->ndm + 2)
    p.flag2=auto_hopf_curve_kind(u0[2 * iap->ndm]);
  p.jacobian=s.numerics.jacobian;
  diagram_record_branch(s,p);
  if(!s.auto_state.diag_flag){
    edit_start(s,p);
    s.auto_state.diag_flag=true;
  } else {
    add_diagram(s,p);
  }
  if(from)last_diagram(s.diagram)->from=from;

  /* plotted, and its stability shown, from the stored point, as a redraw
     or a grab shows it */
  const DIAGRAM *d=last_diagram(s.diagram);
  add_point(s,*d,type,iap->ntot==1?0:1);
  xpp::job::report_point(static_cast<int>(std::abs(ibrs)),static_cast<int>(std::abs(ntots))); /* xppautX: where it got to (xpp_job.h) */
}


} // namespace xpp
