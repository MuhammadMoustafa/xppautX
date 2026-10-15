#include "auto_unknowns.h"
#include <algorithm>
#include <vector> /* first: auto_f2c.h's min/max macros (pulled in below) break libstdc++ headers if not included first */
#include "autevd.h"
#include "session.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#include "diagram.h"

#include "auto_stability.h"
#include "xpp_math.h"
#include "xpp_job.h"

namespace xpp {

[[noreturn]] void auto_nonfinite(const iap_type &iap, integer point)
{
  auto_fail(xpp::format("Branch {} point {}: nonfinite AUTO solution", iap.ibr, point));
}

void init_auto(AutoState &st, int ndim, bool exact)
{
  const xpp::BIFUR &b=st.bifur;
  /* here are the constants that we do not allow the user to change */
  int nnbc;
  int i;
  st.run.iad=st.advanced.iad;   
  st.run.iplt=0;
  st.run.mxbf=st.advanced.mxbf;
  st.run.iid=st.advanced.iid;
  st.run.itmx=st.advanced.itmx;
  st.run.itnw=st.advanced.itnw;
  st.run.nwtn=st.advanced.nwtn;
  st.run.jac=exact?1:0;
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
  for(i=0;i<st.nuzr;i++)
    st.run.marks.push_back({b.period_mark[i] ? xpp::auto_unknown::period : st.parameter_layout.solver_index(static_cast<int>(st.uzr_par[i])),
                            st.uzr_period[i]});
 
}

/* Only unit 8,3 or q.prb is important; all others are unnecesary */

DiagramPointType get_bif_type(int ibr, int ntot, int lab)
{
  DiagramPointType type=STABLE_EQ;

    if(ibr<0&&ntot<0)type=STABLE_PERIODIC;
  if(ibr<0&&ntot>0)type=UNSTABLE_PERIODIC;
  if(ibr>0&&ntot>0)type=UNSTABLE_EQ;
  if(ibr>0&&ntot<0)type=STABLE_EQ;
  return(type);
}
void addbif(iap_type *iap, rap_type *rap, integer ntots, integer ibrs, double *par,integer *icp,int lab, double *a, std::span<const double> uhigh, std::span<const double> ulow, std::span<const double> u0, std::span<const double> ubar)
{
  xpp::Session &s=*iap->lib->session;
  int icp1=icp[0],icp2=icp[1],icp3=icp[2],icp4=icp[3];
  double per=par[xpp::auto_unknown::period];
  int n=iap->ndim;
  const DiagramPointType type=get_bif_type(ibrs,ntots,lab);
  /* xppautX: the point's stability, or zeros: not computed (auto_stability.h) */
  xpp::DIAGRAM p{};
  p.size_variables(static_cast<size_t>(n));
  auto_stability_for(s.auto_state.stability,static_cast<int>(ibrs),static_cast<int>(ntots),p.evr,p.evi);

  // Guard the final stored quantities too, including derived norms and spectra.
  if (!xpp::all_finite({par, static_cast<size_t>(iap->lib->parameter_count)}) ||
      !xpp::all_finite({a, 1}) || !xpp::all_finite({&per, 1}) ||
      !xpp::all_finite(uhigh) || !xpp::all_finite(ulow) || !xpp::all_finite(u0) || !xpp::all_finite(ubar) ||
      !xpp::all_finite(p.evr) || !xpp::all_finite(p.evi))
    auto_nonfinite(*iap, labs(ntots));

  int from=auto_run_from_take(s); /* the run's first point says which label it started from */
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
    if(parameter>=0 && parameter<s.model().nupar){
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
  p.jacobian=s.numerics.jacobian;
  diagram_record_branch(s,p);
  if(s.auto_state.diag_flag==0){
    edit_start(s,p);
    s.auto_state.diag_flag=1;
  } else {
    add_diagram(s,p);
  }
  if(from)last_diagram(s.diagram)->from=from;

  /* plotted, and its stability shown, from the stored point, as a redraw
     or a grab shows it */
  const DIAGRAM *d=last_diagram(s.diagram);
  add_point(s,*d,type,iap->ntot==1?0:1);
  xpp::job::report_point(static_cast<int>(labs(ibrs)),static_cast<int>(labs(ntots))); /* xppautX: where it got to (xpp_job.h) */
}


} // namespace xpp
