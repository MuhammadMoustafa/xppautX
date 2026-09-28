#include "model.h"
#include "session.h"
#include "flags.h"
#include "form_ode.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "xpp_log.h"

#include "cv2.h"
#include "stiff.h"
#include "derived.h"
#include "dormpri.h"
#include "gear.h"
#include "integrate.h"
#include "expr.h"

#include <stdlib.h> 
#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "getvar.h"
#include <array>
#include <string>
#include <vector>
#define MY_DBL_EPS 5e-16

#include "odesol2.h"

/*  this is a new (Summer 1995) addition to XPP that allows one to
    do things like delta functions and other discontinuous
    stuff.

    The conditions are set up as part of the "ODE" file:

global sign condition {event1;....;eventn}
global sign {condition} {event1;....;eventn}

the {} and ;  are required for the events

condition is anything that when it evaluates to 0 means the flag should be 
set.  The sign is like in Poincare maps, thus let C(t1) and C(t2) be
the value of the condition at t1  and t2.  
sign = 0 ==>  just find when C(t)=0
sign = 1 ==>  C(t1)<0 C(t2)>0
sign = -1==>  C(t1)>0 C(t2)<0

To get the time of the event, we use linear interpolation:

 t* = t1 + (t2-t1)
           -------   (0-C(t1))
          C(t2)-C(t1)
This yields  the variables, etc at that time 

Now what are the events:

They are of the form:
   name = expression  the variable  <name> is replaced by the value of 
  <expression> 

Note that there may be several "conditions" defined and that
these must also be checked to see if they have been switched
and in what order.  This is particularly true for "delta" function
type things.

Here is a simple example -- the kicked cycle:
dx/dt = y 
dy/dy = -x -c y

if y=0 and y goes from pos to neg then x=x+b
here is how it would work:

global -1 y {x=x+b}

Here is Tysons model:

global -1 u-.2 {m=.5*m}

*/

/*

type =0 variable
type =1 parameter
type =2 output
type =3 halt
*/

#define IC 2
#define PARAM 1

/* the most events a flag has */
constexpr int MAX_EVENTS=xpp::Model::max_events;

namespace {
/* each flag's state during a run (xpp::Model has its definition): the
   condition's value at the step before and this one, where in the step
   it crossed (tstar, 0..1), whether it did (hit, the pass it did in) and
   the events' values */
struct FlagState {
  double f0=0.0,f1=0.0;
  double tstar=0.0;
  std::array<double,MAX_EVENTS> vrhs{};
  int hit=0;
};
std::array<FlagState,MAXFLAG> fstate;
}


/* rest is "{name=formula;name=formula;...}" (spaces ignored): the flag's
   events, in order */
int add_global(const char *cond, int sign, const char *rest)
{
  std::array<xpp::Model::GlobalFlag,MAXFLAG> &flags=xpp::model().flags;
  int nevents,j=xpp::model().nflags;
  std::string temp;
  if(xpp::model().nflags>=MAXFLAG){
    xpp_log(XPP_LOG_WARN, "Too many global conditions\n");
    return(1);
  }
  flags[j].cond=cond;
  nevents=0;
  flags[j].lhsname[0].clear();
  for(const char *p=rest;*p;p++){
    char ch=*p;
    if(ch=='{'||ch==' ')continue;
    if(ch=='}'||ch==';'){
      if(nevents==MAX_EVENTS){
	xpp_log(XPP_LOG_WARN, " Too many events per flag \n");
	return(1);
      }
      if(flags[j].lhsname[nevents].empty()){
	xpp::log(XPP_LOG_WARN, " No event variable named for {} \n",temp);
	return(1);
      }
      flags[j].rhs[nevents]=temp;
      nevents++;
      temp.clear();
      if(ch=='}')break;
      continue;
    }
    if(ch=='='){
      flags[j].lhsname[nevents]=temp;
      temp.clear();
      if(nevents<MAX_EVENTS-1)
	flags[j].lhsname[nevents+1].clear();
      continue;
    }
    temp+=ch;
  }
  if(nevents==0){
    xpp_log(XPP_LOG_WARN, " No events for condition %s \n",cond);
    return(1);
  }
 /*  we now have the condition, the names, and the formulae */
  flags[j].sign=sign;
  flags[j].nevents=nevents;
  xpp::model().nflags++;
  return(0);
}

/* expr compiled (add_expr), its ENDEXP included; false if it does not
   parse */
static bool compile(const std::string &expr, std::vector<int> &out)
{
  int command[256];
  int nc;
  if(add_expr(expr.c_str(),command,&nc))return false;
  out.assign(command,command+nc+1);
  return true;
}

int compile_flags()
{
  std::array<xpp::Model::GlobalFlag,MAXFLAG> &flags=xpp::model().flags;
  int j;
  int i,index;
  if(xpp::model().nflags==0)return(0);
  for(j=0;j<xpp::model().nflags;j++){
    if(!compile(flags[j].cond,flags[j].comcond)){
      xpp::log(XPP_LOG_WARN, "Illegal global condition:  {}\n",flags[j].cond);
      return(1);
    }
    flags[j].anypars=0;
    flags[j].nointerp=0;
    for(i=0;i<flags[j].nevents;i++){
      const char *name=flags[j].lhsname[i].c_str();
      index=find_user_name(IC,name);
      if(index<0){
	index=find_user_name(PARAM,name);
	if(index<0){
	  if(strcasecmp(name,"out_put")==0)
	    {
	      flags[j].type[i]=2;
	      flags[j].lhs[i]=0;
	    }
	  else {
	    if(strcasecmp(name,"arret")==0)
	      {
		flags[j].type[i]=3;
		flags[j].lhs[i]=0;

	      }
	    else {
	      if(strcasecmp(name,"no_interp")==0)
		{
		  flags[j].nointerp=1;
                  flags[j].type[i]=0;
		  flags[j].lhs[i]=0;
		}

	    else {
	      xpp::log(XPP_LOG_WARN, " <{}> is not a valid variable/parameter name \n",
		     name);
	      return(1);
	    }
	    }
	  }
	}
	else{
	  flags[j].lhs[i]=index;
	  flags[j].type[i]=1;
          flags[j].anypars=1;
	}
      }
      else {
	flags[j].lhs[i]=index;
	flags[j].type[i]=0;
      }
      if(!compile(flags[j].rhs[i],flags[j].comrhs[i])){
	xpp::log(XPP_LOG_WARN, "Illegal event {} for global {}\n",
	       flags[j].rhs[i],flags[j].cond);
      return(1);
      }
    }
  }
  return(0);
}

/*  here is the shell code for a loop around  integration step  */

int one_flag_step(double *yold, double *ynew, int *istart, double told, double *tnew, int neq, double *s)
{
  std::array<xpp::Model::GlobalFlag,MAXFLAG> &flags=xpp::model().flags;
  double dt=*tnew-told;
  double f0,f1,tol,tolmin=1e-10;
  double smin=2;
  int sign,i,j,in,ncycle=0,newhit,nevents;

  if(xpp::model().nflags==0)return(0);
  for(i=0;i<xpp::model().nflags;i++){
    fstate[i].tstar=2.0;
    fstate[i].hit=0;
  }
  /* If this is the first call, then need f1  */
  if(*istart==1){  
    for(i=0;i<neq;i++)
      SETVAR(i+1,yold[i]);
    SETVAR(0,told);
    for(i=0;i<xpp::model().nflags;i++)
    *istart=0;
  
  }
  for(i=0;i<xpp::model().nflags;i++){
    sign=flags[i].sign;
    fstate[i].f0=fstate[i].f1;
    f0=fstate[i].f0;
    for(j=0;j<neq;j++)
      SETVAR(j+1,ynew[j]);
    SETVAR(0,*tnew);
    f1=evaluate(flags[i].comcond.data());
    fstate[i].f1=f1;
    tol=fabs(f1-f0);
    switch(sign){
    case 1: 
      if((((f0<0.0)&&(f1>0.0))||((f0<0.0)&&(f1>0.0)))&&tol>tolmin){
	fstate[i].hit=ncycle+1;
	fstate[i].tstar=f0/(f0-f1);
      }
      break;
    case -1:
      if(f0>0.0&&f1<=0.0&&tol>tolmin){
	fstate[i].hit=ncycle+1;
	fstate[i].tstar=f0/(f0-f1);
      }
      break;
    case 0:
      if(fabs(f1)<MY_DBL_EPS){
	fstate[i].hit=ncycle+1;
	fstate[i].tstar=told;
      }
      break;
    }
    if(flags[i].nointerp==1)
      {
	fstate[i].tstar=1.0;
      }
    
      if(smin>fstate[i].tstar)smin=fstate[i].tstar;

  } /* run through flags */
 
   if(smin<xpp::session().numerics.stol)smin=xpp::session().numerics.stol;
  else smin=(1+xpp::session().numerics.stol)*smin;  
  if(smin>1.0)return(0);

  *tnew=told+dt*smin;
  SETVAR(0,*tnew);
  for(i=0;i<neq;i++){
    ynew[i]=yold[i]+smin*(ynew[i]-yold[i]);
    SETVAR(i+1,ynew[i]);
  }
  for(i=0;i<xpp::model().nflags;i++)
    fstate[i].f0=evaluate(flags[i].comcond.data());
  while(1){ /* run through all possible events  */
    ncycle++;
    newhit=0;
    for(i=0;i<xpp::model().nflags;i++){
      nevents=flags[i].nevents;
      if(fstate[i].hit==ncycle&&fstate[i].tstar<=smin){
	for(j=0;j<nevents;j++){
	  fstate[i].vrhs[j]=evaluate(flags[i].comrhs[j].data());
	  in=flags[i].lhs[j];
	  if(flags[i].type[j]==0)
	        SETVAR(in+1,fstate[i].vrhs[j]);
	 
	}
      }
    }
    for(i=0;i<xpp::model().nflags;i++){
      nevents=flags[i].nevents;
      if(fstate[i].hit==ncycle&&fstate[i].tstar<=smin){
	for(j=0;j<nevents;j++){
	  
	  in=flags[i].lhs[j];
	  if(flags[i].type[j]==0){
	     ynew[in]=fstate[i].vrhs[j];
	     /* SETVAR(in+1,ynew[in]); if this screws up */
	  }
	  else {
	    if(flags[i].type[j]==1)
	      set_val(xpp::model().upar_names[in],fstate[i].vrhs[j]);
	    else{

	      if((flags[i].type[j]==2)&&(fstate[i].vrhs[j]>0))send_output(ynew,*tnew);
	      if((flags[i].type[j]==3)&&(fstate[i].vrhs[j]>0))send_halt(ynew,*tnew);
	    }
	  }

	}
	if(flags[i].anypars){
	  evaluate_derived();
	  redraw_params();
	}
      }
    }

    for(i=0;i<neq;i++){
      ynew[i]=GETVAR(i+1); /* if this screws up */
    }
    for(i=0;i<xpp::model().nflags;i++){
      fstate[i].f1=evaluate(flags[i].comcond.data());
      if(fstate[i].hit>0)continue; /* already hit so dont do anything */
      f1=fstate[i].f1;
      sign=flags[i].sign;
      f0=fstate[i].f0;
      tol=fabs(f1-f0);
      switch(sign){
      case 1:
	if(f0<=0.0&&f1>=0.0&&tol>tolmin){
	  fstate[i].tstar=smin;
	  fstate[i].hit=ncycle+1;
	  newhit=1;
	}
	break;
      case -1:
	if(f0>=0.0&&f1<=0.0&&tol>tolmin){
	  fstate[i].tstar=smin;
	  fstate[i].hit=ncycle+1;
	  newhit=1;
	}
	break; 
      case 0:
	if(f0*f1<=0&&(f1!=0||f0!=0)&&tol>tolmin){
	  fstate[i].tstar=smin;
	  fstate[i].hit=ncycle+1;
	  newhit=1;
	}
      }
    }
    if(newhit==0)break;
  }
 
  *s=smin;
  return(1);
}

/*  here are the ODE drivers */

int one_flag_step_symp(double *y, double dt, double *work, int neq, double *tim, int *istart)
{
  double yold[MAXODE],told;
  int i,hit;
  double s,dtt=dt;
  int nstep=0; 
  while(1){
    for(i=0;i<neq;i++)
      yold[i]=y[i];
    told=*tim;
    one_step_symp(y,dtt,work,neq,tim);
    if((hit=one_flag_step(yold,y,istart,told,tim,neq,&s ))==0)
      break;
    /* Its a hit !! */
    nstep++;
    dtt=(1-s)*dt;
    if(nstep>(xpp::model().nflags+2)){
      xpp_log(XPP_LOG_WARN, " Working too hard?? ");
      xpp_log(XPP_LOG_WARN, "smin=%g\n",s);
      break;
    }
  }
  
  return(1);
}
 
int one_flag_step_euler(double *y, double dt, double *work, int neq, double *tim, int *istart)
{
  double yold[MAXODE],told;
  int i,hit;
  double s,dtt=dt;
  int nstep=0; 
  while(1){
    for(i=0;i<neq;i++)
      yold[i]=y[i];
    told=*tim;
    one_step_euler(y,dtt,work,neq,tim);
    if((hit=one_flag_step(yold,y,istart,told,tim,neq,&s ))==0)
      break;
    /* Its a hit !! */
    nstep++;
    dtt=(1-s)*dt;
    if(nstep>(xpp::model().nflags+2)){
      xpp_log(XPP_LOG_WARN, " Working too hard?? ");
      xpp_log(XPP_LOG_WARN, "smin=%g\n",s);
      break;
    }
  }
  
  return(1);
}
    
int one_flag_step_discrete(double *y, double dt, double *work, int neq, double *tim, int *istart)
{
  double yold[MAXODE],told;
  int i,hit;
  double s,dtt=dt;
  int nstep=0; 
  while(1){
    for(i=0;i<neq;i++)
      yold[i]=y[i];
    told=*tim;
    one_step_discrete(y,dtt,work,neq,tim);
    if((hit=one_flag_step(yold,y,istart,told,tim,neq,&s ))==0)
      break;
    /* Its a hit !! */
    nstep++;
    dtt=(1-s)*dt;
    if(nstep>(xpp::model().nflags+2)){
      xpp_log(XPP_LOG_WARN, " Working too hard?? ");
      xpp_log(XPP_LOG_WARN, "smin=%g\n",s);
      break;
    }
  }
  return(1);
}
     
int one_flag_step_heun(double *y, double dt, double *yval[2], int neq, double *tim, int *istart)
{
  double yold[MAXODE],told;
  int i,hit;
  double s,dtt=dt;
  int nstep=0; 
  while(1){
    for(i=0;i<neq;i++)
      yold[i]=y[i];
    told=*tim;
    one_step_heun(y,dtt,yval,neq,tim);
    if((hit=one_flag_step(yold,y,istart,told,tim,neq,&s ))==0)
      break;
    /* Its a hit !! */
    nstep++;
    dtt=(1-s)*dt;
    if(nstep>(xpp::model().nflags+2)){
      xpp_log(XPP_LOG_WARN, " Working too hard? ");
      xpp_log(XPP_LOG_WARN, " smin=%g\n",s);
      break;
    }
  }
  return(1);
}
    
int one_flag_step_rk4(double *y, double dt, double *yval[3], int neq, double *tim, int *istart)
{
  double yold[MAXODE],told;
  int i,hit;
  double s,dtt=dt;
  int nstep=0; 
  while(1){
    for(i=0;i<neq;i++)
      yold[i]=y[i];
    told=*tim;
    one_step_rk4(y,dtt,yval,neq,tim);
    if((hit=one_flag_step(yold,y,istart,told,tim,neq,&s ))==0)
      break;
    /* Its a hit !! */
    nstep++;
    dtt=(1-s)*dt;
    if(nstep>(xpp::model().nflags+2)){
      xpp_log(XPP_LOG_WARN, " Working too hard?");
            xpp_log(XPP_LOG_WARN, "smin=%g\n",s);
      break;
    }
  }
  return(1);
}

int one_flag_step_gear(int neq, double *t, double tout, double *y, double hmin, double hmax, double eps, int mf, double *error, int *kflag, int *jstart, double *work, int *iwork)
{
    double yold[MAXODE],told;
  int i,hit;
  double s;
  int nstep=0; 
  while(1){
    for(i=0;i<neq;i++)
      yold[i]=y[i];
    told=*t;
    ggear(neq,t,tout,y, hmin,hmax,eps,mf,error,kflag,jstart,work,iwork);
    if(*kflag<0) break;
    if((hit=one_flag_step(yold,y,jstart,told,t,neq,&s ))==0)
      break;
    /* Its a hit !! */
    nstep++;
    *jstart=0; /* for gear always reset  */
    if(*t==tout)break;
    if(nstep>(xpp::model().nflags+2)){
      xpp_log(XPP_LOG_WARN, " Working too hard? ");
            xpp_log(XPP_LOG_WARN, "smin=%g\n",s);
      break;
    }
  }
  return 0;

}
int one_flag_step_rosen(double *y,double *tstart,double tfinal,
int *istart,int n,double *work,int *ierr)
{
   double yold[MAXODE],told;
  int i,ok,hit;
  double s;
  int nstep=0; 
  while(1){
    for(i=0;i<n;i++)
      yold[i]=y[i];
    told=*tstart;
    ok=rosen(y,tstart,tfinal,istart,n,work,ierr);
    if(ok==-1) break;
    if((hit=one_flag_step(yold,y,istart,told,tstart,n,&s ))==0)
      break;
    /* Its a hit !! */
    nstep++;
    
    if(*tstart==tfinal)break;
    if(nstep>(xpp::model().nflags+2)){
      xpp_log(XPP_LOG_WARN, " Working too hard? ");
            xpp_log(XPP_LOG_WARN, "smin=%g\n",s);
      *ierr=-2;
      return 1;
      break;
    }
  }
  return 0;

}

int one_flag_step_dp(int *istart, double *y, double *t, int n, double tout, double *tol, double *atol, int flag, int *kflag)
{
   double yold[MAXODE],told;
  int i,hit;
  double s;
  int nstep=0; 
  while(1){
    for(i=0;i<n;i++)
      yold[i]=y[i];
    told=*t;
    dormprin(istart,y,t,n,tout,tol,atol,flag,kflag);
    if(*kflag!=1) break;
    if((hit=one_flag_step(yold,y,istart,told,t,n,&s ))==0)
      break;
    /* Its a hit !! */
    nstep++;
    
    if(*t==tout)break;
    if(nstep>(xpp::model().nflags+2)){
      xpp_log(XPP_LOG_WARN, " Working too hard? ");
            xpp_log(XPP_LOG_WARN, "smin=%g\n",s);
      return 1;
      break;
    }
  }
  return 0;

}

#ifdef CVODE_YES
int one_flag_step_cvode(int *command, double *y, double *t, int n, double tout, int *kflag, double *atol, double *rtol)  /* command =0 continue, 1 is start 2 finish */
{
    double yold[MAXODE],told;
  int i,hit,neq=n;
  double s;
  int nstep=0; 
  while(1){
    for(i=0;i<neq;i++)
      yold[i]=y[i];
    told=*t;
    ccvode(command,y,t,n,tout,kflag,atol,rtol);
    if(*kflag<0) break;
    if((hit=one_flag_step(yold,y,command,told,t,neq,&s ))==0)
      break;
    /* Its a hit !! */
    nstep++;
   end_cv();
    *command=1; /* for cvode always reset  */
    if(*t==tout)break;
    if(nstep>(xpp::model().nflags+2)){
      xpp_log(XPP_LOG_WARN, " Working too hard? ");
            xpp_log(XPP_LOG_WARN, "smin=%g\n",s);
      return 1;
    }
  }
  return 0;

}

#endif
int one_flag_step_adap(double *y, int neq, double *t, double tout, double eps, double *hguess, double hmin, double *work, int *ier, double epjac, int iflag, int *jstart)
{
    double yold[MAXODE],told;
  int i,hit;
  double s;
  int nstep=0; 
  while(1){
    for(i=0;i<neq;i++)
      yold[i]=y[i];
    told=*t;
    gadaptive(y,neq,t,tout,eps,
		     hguess,hmin,work,ier,epjac,iflag,jstart);
    if(*ier) break;
    if((hit=one_flag_step(yold,y,jstart,told,t,neq,&s ))==0)
      break;
    /* Its a hit !! */
    nstep++;
    
    if(*t==tout)break;
    if(nstep>(xpp::model().nflags+2)){
      xpp_log(XPP_LOG_WARN, " Working too hard? ");
            xpp_log(XPP_LOG_WARN, "smin=%g\n",s);
      break;
    }
  }
  return 0;

}

int one_flag_step_backeul(double *y, double *t, double dt, int neq, double *yg, double *yp, double *yp2, double *ytemp, double *errvec, double *jac, int *istart)
{
  double yold[MAXODE],told;
  int i,hit,j;
  double s;
  double dtt=dt; 
  int nstep=0; 
  while(1){
    for(i=0;i<neq;i++)
      yold[i]=y[i];
    told=*t;
    if((j=one_bak_step(y,t,dtt,neq,yg,yp,yp2,ytemp,errvec,jac,istart))!=0)
      return(j);
    if((hit=one_flag_step(yold,y,istart,told,t,neq,&s ))==0)
      break;
    /* Its a hit !! */
    nstep++;
    dtt=(1-s)*dt;  
    if(nstep>(xpp::model().nflags+2)){
      xpp_log(XPP_LOG_WARN, " Working too hard?");
            xpp_log(XPP_LOG_WARN, "smin=%g\n",s);
      break;
    }
  }
  return 0;
}

