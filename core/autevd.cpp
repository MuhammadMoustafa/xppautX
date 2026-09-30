#include <vector> /* first: auto_f2c.h's min/max macros (pulled in below) break libstdc++ headers if not included first */
#include "autevd.h"
#include "session.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#include "diagram.h"

#include "auto_stability.h"
#include "xpp_job.h"

#define SPER 3
#define UPER 4
#define SEQ 1
#define UEQ 2

void init_auto(AutoState &st, int ndim, int nicp, int nbc, int ips, int irs, int ilp, int ntst, int isp, int isw, int nmx,
               int npr, double ds, double dsmin, double dsmax, double rl0, double rl1, double a0, double a1,
               int ip1, int ip2, int ip3, int ip4, int ip5, int nuzr, double epsl, double epsu, double epss,
               int ncol)
{
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
  
  if(ips==4)
    nnbc=ndim;
  else
    nnbc=0;
  st.run.ndim=ndim;
  st.run.nbc=nnbc; 
  st.run.ips=ips;
  st.run.irs=irs;
  st.run.ilp=ilp;
  st.run.nicp=nicp;
  st.run.icp[0]=ip1;
  st.run.icp[1]=ip2;
  st.run.icp[2]=ip3;
  st.run.icp[3]=ip4;
  st.run.icp[4]=ip5;
  st.run.ntst=ntst;
  st.run.ncol=ncol;
  st.run.isp=isp;
  st.run.isw=isw;
  st.run.nmx=nmx;
  st.run.rl0=rl0;
  st.run.rl1=rl1;
  st.run.a0=a0;
  st.run.a1=a1;
  st.run.npr=npr;

  st.run.epsl=epsl;
  st.run.epss=epss;
  st.run.epsu=epsu;
  st.run.ds=ds;
  st.run.dsmax=dsmax;
  st.run.dsmin=dsmin;

  st.run.nuzr=st.nuzr;
  for(i=0;i<st.nuzr;i++){
    st.run.iuz[i]=st.uzr_par[i];
     st.run.vuz[i]=st.uzr_period[i];
  }
 
}

/* Only unit 8,3 or q.prb is important; all others are unnecesary */

int get_bif_type(int ibr, int ntot, int lab)
{
  int type=SEQ;

    if(ibr<0&&ntot<0)type=SPER;
  if(ibr<0&&ntot>0)type=UPER;
  if(ibr>0&&ntot>0)type=UEQ;
  if(ibr>0&&ntot<0)type=SEQ;
  return(type);
}
void addbif(iap_type *iap, rap_type *rap, integer ntots, integer ibrs, double *par,integer *icp,int lab, double *a, double *uhigh, double *ulow, double *u0, double *ubar)
{
  xpp::Session &s=*iap->lib->session;
  int icp1=icp[0],icp2=icp[1],icp3=icp[2],icp4=icp[3];
  double per=par[10];
  int n=iap->ndim;
  int from=auto_run_from_take(); /* the run's first point says which label it started from */
  int type=get_bif_type(ibrs,ntots,lab);
  /* its entry in the diagram list: the first one after start_diagram, else a new one */
  int node=s.auto_state.diag_flag==0?0:diagram_count(s.diagram);
  /* xppautX: the point's stability, or zeros: not computed (auto_stability.h) */
  std::vector<double> ev(2*static_cast<size_t>(n));
  auto_stability_for(static_cast<int>(ibrs),static_cast<int>(ntots),n,ev.data(),ev.data()+n);

  if(s.auto_state.diag_flag==0){
    edit_start(s,ibrs,ntots,iap->itp,lab,iap->nfpr,*a,uhigh,ulow,u0,ubar,
	       par,per,n,icp1,icp2,icp3,icp4,ev.data(),ev.data()+n);
    s.auto_state.diag_flag=1;
  } else {
    add_diagram(s,ibrs,ntots,iap->itp,lab,iap->nfpr,*a,uhigh,ulow,u0,ubar,
	        par,per,n,icp1,icp2,icp3,icp4,s.auto_state.two_param,ev.data(),ev.data()+n);
  }
  if(from)last_diagram(s.diagram)->from=from;

  /* plotted, and its stability shown, from the stored point, as a redraw
     or a grab shows it */
  const DIAGRAM *d=last_diagram(s.diagram);
  auto_point_id(ibrs,ntots,iap->itp,node,from);
  add_point(s,par,per,uhigh,ulow,ubar,*a,type,iap->ntot==1?0:1,lab,
	    iap->nfpr,icp1,icp2,icp3,icp4,s.auto_state.two_param,d->evr,d->evi);
  xpp_job_point_stored(static_cast<int>(labs(ibrs)),static_cast<int>(labs(ntots))); /* xppautX: where it got to (xpp_job.h) */
}

