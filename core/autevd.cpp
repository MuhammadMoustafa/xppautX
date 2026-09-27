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

void init_auto(int ndim, int nicp, int nbc, int ips, int irs, int ilp, int ntst, int isp, int isw, int nmx,
               int npr, double ds, double dsmin, double dsmax, double rl0, double rl1, double a0, double a1,
               int ip1, int ip2, int ip3, int ip4, int ip5, int nuzr, double epsl, double epsu, double epss,
               int ncol)
{
  xpp::Session &s=xpp::session();
  /* here are the constants that we do not allow the user to change */
  int nnbc;
  int i;
  s.auto_state.run.iad=s.auto_state.advanced.iad;   
  s.auto_state.run.iplt=0;
  s.auto_state.run.mxbf=s.auto_state.advanced.mxbf;
  s.auto_state.run.iid=s.auto_state.advanced.iid;
  s.auto_state.run.itmx=s.auto_state.advanced.itmx;
  s.auto_state.run.itnw=s.auto_state.advanced.itnw;
  s.auto_state.run.nwtn=s.auto_state.advanced.nwtn;
  s.auto_state.run.jac=0;
  s.auto_state.run.iads=s.auto_state.advanced.iads;
  s.auto_state.run.nthl=1;
  s.auto_state.run.ithl[0]=10;
  s.auto_state.run.thl[0]=0.0;
  s.auto_state.run.nint=0;    
  
  if(ips==4)
    nnbc=ndim;
  else
    nnbc=0;
  s.auto_state.run.ndim=ndim;
  s.auto_state.run.nbc=nnbc; 
  s.auto_state.run.ips=ips;
  s.auto_state.run.irs=irs;
  s.auto_state.run.ilp=ilp;
  s.auto_state.run.nicp=nicp;
  s.auto_state.run.icp[0]=ip1;
  s.auto_state.run.icp[1]=ip2;
  s.auto_state.run.icp[2]=ip3;
  s.auto_state.run.icp[3]=ip4;
  s.auto_state.run.icp[4]=ip5;
  s.auto_state.run.ntst=ntst;
  s.auto_state.run.ncol=ncol;
  s.auto_state.run.isp=isp;
  s.auto_state.run.isw=isw;
  s.auto_state.run.nmx=nmx;
  s.auto_state.run.rl0=rl0;
  s.auto_state.run.rl1=rl1;
  s.auto_state.run.a0=a0;
  s.auto_state.run.a1=a1;
  s.auto_state.run.npr=npr;

  s.auto_state.run.epsl=epsl;
  s.auto_state.run.epss=epss;
  s.auto_state.run.epsu=epsu;
  s.auto_state.run.ds=ds;
  s.auto_state.run.dsmax=dsmax;
  s.auto_state.run.dsmin=dsmin;

  s.auto_state.run.nuzr=s.auto_state.nuzr;
  for(i=0;i<s.auto_state.nuzr;i++){
    s.auto_state.run.iuz[i]=s.auto_state.uzr_par[i];
     s.auto_state.run.vuz[i]=s.auto_state.uzr_period[i];
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
  int icp1=icp[0],icp2=icp[1],icp3=icp[2],icp4=icp[3];
  double per=par[10];
  int n=iap->ndim;
  int from=auto_run_from_take(); /* the run's first point says which label it started from */
  int type=get_bif_type(ibrs,ntots,lab);
  /* its entry in the diagram list: the first one after start_diagram, else a new one */
  int node=xpp::session().auto_state.diag_flag==0?0:diagram_count();
  /* xppautX: the point's stability, or zeros: not computed (auto_stability.h) */
  std::vector<double> ev(2*static_cast<size_t>(n));
  auto_stability_for(static_cast<int>(ibrs),static_cast<int>(ntots),n,ev.data(),ev.data()+n);

  if(xpp::session().auto_state.diag_flag==0){
    edit_start(ibrs,ntots,iap->itp,lab,iap->nfpr,*a,uhigh,ulow,u0,ubar,
	       par,per,n,icp1,icp2,icp3,icp4,ev.data(),ev.data()+n);
    xpp::session().auto_state.diag_flag=1;
  } else {
    add_diagram(ibrs,ntots,iap->itp,lab,iap->nfpr,*a,uhigh,ulow,u0,ubar,
	        par,per,n,icp1,icp2,icp3,icp4,xpp::session().auto_state.two_param,ev.data(),ev.data()+n);
  }
  if(from)set_last_diagram_from(from);

  /* plotted, and its stability shown, from the stored point, as a redraw
     or a grab shows it */
  const DIAGRAM *d=last_diagram();
  auto_point_id(ibrs,ntots,iap->itp,node,from);
  add_point(par,per,uhigh,ulow,ubar,*a,type,iap->ntot==1?0:1,lab,
	    iap->nfpr,icp1,icp2,icp3,icp4,xpp::session().auto_state.two_param,d->evr,d->evi);
  xpp_job_point_stored(static_cast<int>(labs(ibrs)),static_cast<int>(labs(ntots))); /* xppautX: where it got to (xpp_job.h) */
}

