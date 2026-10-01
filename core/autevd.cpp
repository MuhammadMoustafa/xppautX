#include <algorithm>
#include <vector> /* first: auto_f2c.h's min/max macros (pulled in below) break libstdc++ headers if not included first */
#include "autevd.h"
#include "session.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#include "diagram.h"

#include "auto_stability.h"
#include "xpp_job.h"

namespace xpp {

void init_auto(AutoState &st, int ndim)
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
  st.run.jac=0;
  st.run.iads=st.advanced.iads;
  st.run.nthl=1;
  st.run.ithl[0]=10;
  st.run.thl[0]=0.0;
  st.run.nint=0;    
  
  if(b.ips==4)
    nnbc=ndim;
  else
    nnbc=0;
  st.run.ndim=ndim;
  st.run.nbc=nnbc; 
  st.run.ips=b.ips;
  st.run.irs=b.irs;
  st.run.ilp=b.ilp;
  st.run.nicp=b.nfpar;
  st.run.icp[0]=st.axes().icp1;
  st.run.icp[1]=st.axes().icp2;
  st.run.icp[2]=b.icp3;
  st.run.icp[3]=b.icp4;
  st.run.icp[4]=b.icp5;
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

  st.run.nuzr=st.nuzr;
  for(i=0;i<st.nuzr;i++){
    st.run.iuz[i]=st.uzr_par[i];
     st.run.vuz[i]=st.uzr_period[i];
  }
 
}

/* Only unit 8,3 or q.prb is important; all others are unnecesary */

int get_bif_type(int ibr, int ntot, int lab)
{
  int type=STABLE_EQ;

    if(ibr<0&&ntot<0)type=STABLE_PERIODIC;
  if(ibr<0&&ntot>0)type=UNSTABLE_PERIODIC;
  if(ibr>0&&ntot>0)type=UNSTABLE_EQ;
  if(ibr>0&&ntot<0)type=STABLE_EQ;
  return(type);
}
void addbif(iap_type *iap, rap_type *rap, integer ntots, integer ibrs, double *par,integer *icp,int lab, double *a, double *uhigh, double *ulow, double *u0, double *ubar)
{
  xpp::Session &s=*iap->lib->session;
  int icp1=icp[0],icp2=icp[1],icp3=icp[2],icp4=icp[3];
  double per=par[10];
  int n=iap->ndim;
  int from=auto_run_from_take(s); /* the run's first point says which label it started from */
  int type=get_bif_type(ibrs,ntots,lab);
  /* xppautX: the point's stability, or zeros: not computed (auto_stability.h) */
  std::vector<double> ev(2*static_cast<size_t>(n));
  auto_stability_for(static_cast<int>(ibrs),static_cast<int>(ntots),n,ev.data(),ev.data()+n);

  xpp::DIAGRAM p{};
  p.ibr=static_cast<int>(ibrs);
  p.ntot=static_cast<int>(ntots);
  p.itp=static_cast<int>(iap->itp);
  p.lab=lab;
  p.nfpar=static_cast<int>(iap->nfpr);
  p.norm=*a;
  p.uhi=uhigh;
  p.ulo=ulow;
  p.u0=u0;
  p.ubar=ubar;
  p.evr=ev.data();
  p.evi=ev.data()+n;
  std::copy_n(par,8,p.par);
  p.per=per;
  p.icp1=icp1;
  p.icp2=icp2;
  p.icp3=icp3;
  p.icp4=icp4;
  p.flag2=s.auto_state.two_param;
  if(s.auto_state.diag_flag==0){
    edit_start(s,p,n);
    s.auto_state.diag_flag=1;
  } else {
    add_diagram(s,p,n);
  }
  if(from)last_diagram(s.diagram)->from=from;

  /* plotted, and its stability shown, from the stored point, as a redraw
     or a grab shows it */
  const DIAGRAM *d=last_diagram(s.diagram);
  add_point(s,*d,par,type,iap->ntot==1?0:1);
  xpp::job::report_point(static_cast<int>(labs(ibrs)),static_cast<int>(labs(ntots))); /* xppautX: where it got to (xpp_job.h) */
}


} // namespace xpp
