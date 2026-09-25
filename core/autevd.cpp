#include "autevd.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>


#include "auto_nox.h"
#include "diagram.h"
#include "gear.h"

#include "auto_c.h"
#include "auto_def2.h"
#include "autlim.h"
#include "auto_stability.h"
#include "xAuto.h"
#include "xpp_job.h"
#include "xpp_mem.h"

#define SPECIAL 5
#define SPER 3
#define UPER 4
#define SEQ 1
#define UEQ 2

#define ESCAPE 27
#define MAXDIMHET 12
#define MAXDIMHOM 24


extern double outperiod[20];
extern integer UzrPar[20];
extern int NAutoUzr;

extern ADVAUTO aauto;

XAUTO xAuto;

extern int AutoTwoParam;
int DiagFlag=0;
extern int NBifs; /* diagram.c: the index the next add_diagram() gives */
void init_auto(int ndim, int nicp, int nbc, int ips, int irs, int ilp, int ntst, int isp, int isw, int nmx,
               int npr, double ds, double dsmin, double dsmax, double rl0, double rl1, double a0, double a1,
               int ip1, int ip2, int ip3, int ip4, int ip5, int nuzr, double epsl, double epsu, double epss,
               int ncol)
{
  (void)nbc;
  (void)nuzr;

  /* here are the constants that we do not allow the user to change */
  int nnbc;
  int i;
  xAuto.iad=aauto.iad;   
  xAuto.iplt=0;
  xAuto.mxbf=aauto.mxbf;
  xAuto.iid=aauto.iid;
  xAuto.itmx=aauto.itmx;
  xAuto.itnw=aauto.itnw;
  xAuto.nwtn=aauto.nwtn;
  xAuto.jac=0;
  xAuto.iads=aauto.iads;
  xAuto.nthl=1;
  xAuto.ithl[0]=10;
  xAuto.thl[0]=0.0;
  xAuto.nint=0;    
  
  if(ips==4)
    nnbc=ndim;
  else
    nnbc=0;
  xAuto.ndim=ndim;
  xAuto.nbc=nnbc; 
  xAuto.ips=ips;
  xAuto.irs=irs;
  xAuto.ilp=ilp;
  xAuto.nicp=nicp;
  xAuto.icp[0]=ip1;
  xAuto.icp[1]=ip2;
  xAuto.icp[2]=ip3;
  xAuto.icp[3]=ip4;
  xAuto.icp[4]=ip5;
  xAuto.ntst=ntst;
  xAuto.ncol=ncol;
  xAuto.isp=isp;
  xAuto.isw=isw;
  xAuto.nmx=nmx;
  xAuto.rl0=rl0;
  xAuto.rl1=rl1;
  xAuto.a0=a0;
  xAuto.a1=a1;
  xAuto.npr=npr;
  


  xAuto.epsl=epsl;
  xAuto.epss=epss;
  xAuto.epsu=epsu;
  xAuto.ds=ds;
  xAuto.dsmax=dsmax;
  xAuto.dsmin=dsmin;

  xAuto.nuzr=NAutoUzr;
  for(i=0;i<NAutoUzr;i++){
    xAuto.iuz[i]=UzrPar[i];
     xAuto.vuz[i]=outperiod[i];
  }
 
}




/* Only unit 8,3 or q.prb is important; all others are unnecesary */


int get_bif_type(int ibr, int ntot, int lab)
{
  (void)lab;
  int type=SEQ;

    if(ibr<0&&ntot<0)type=SPER;
  if(ibr<0&&ntot>0)type=UPER;
  if(ibr>0&&ntot>0)type=UEQ;
  if(ibr>0&&ntot<0)type=SEQ;
  /* if(lab>0)type=SPECIAL; */
  return(type);
}
void addbif(iap_type *iap, rap_type *rap, integer ntots, integer ibrs, double *par,integer *icp,int lab, double *a, double *uhigh, double *ulow, double *u0, double *ubar)
{
  (void)rap;
  int icp1=icp[0],icp2=icp[1],icp3=icp[2],icp4=icp[3];
  double per=par[10];
  int n=iap->ndim;
  int from=auto_run_from_take(); /* the run's first point says which label it started from */
  int type=get_bif_type(ibrs,ntots,lab);
  /* its entry in the diagram list: the first one after start_diagram, else a new one */
  int node=DiagFlag==0?0:NBifs;
  /* xppautX: the point's stability, or zeros: not computed (auto_stability.h) */
  double *ev=(double *)xpp_malloc(2*(size_t)n*sizeof(double));
  auto_stability_for((int)ibrs,(int)ntots,n,ev,ev+n);

  if(DiagFlag==0){
    edit_start(ibrs,ntots,iap->itp,lab,iap->nfpr,*a,uhigh,ulow,u0,ubar,
	       par,per,n,icp1,icp2,icp3,icp4,ev,ev+n);
    DiagFlag=1;
  } else {
    add_diagram(ibrs,ntots,iap->itp,lab,iap->nfpr,*a,uhigh,ulow,u0,ubar,
	        par,per,n,icp1,icp2,icp3,icp4,AutoTwoParam,ev,ev+n);
  }
  xpp_free(ev);
  if(from)set_last_diagram_from(from);

  /* plotted, and its stability shown, from the stored point, as a redraw
     or a grab shows it */
  const DIAGRAM *d=last_diagram();
  auto_point_id(ibrs,ntots,iap->itp,node,from);
  add_point(par,per,uhigh,ulow,ubar,*a,type,iap->ntot==1?0:1,lab,
	    iap->nfpr,icp1,icp2,icp3,icp4,AutoTwoParam,d->evr,d->evi);
  xpp_job_point_stored((int)labs(ibrs),(int)labs(ntots)); /* xppautX: where it got to (xpp_job.h) */
}




