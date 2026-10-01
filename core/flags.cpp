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


int split_events(const char *cond, const char *rest, std::vector<FlagEvent> &events)
{
  std::string temp,name;
  events.clear();
  for(const char *p=rest;*p;p++){
    char ch=*p;
    if(ch=='{'||ch==' ')continue;
    if(ch=='}'||ch==';'){
      if(static_cast<int>(events.size())==MAX_EVENTS){
	xpp::log_printf(XPP_LOG_WARN, " Too many events per flag \n");
	return(1);
      }
      if(name.empty()){
	xpp::log(XPP_LOG_WARN, " No event variable named for {} \n",temp);
	return(1);
      }
      events.push_back({name,temp});
      name.clear();
      temp.clear();
      if(ch=='}')break;
      continue;
    }
    if(ch=='='){
      name=temp;
      temp.clear();
      continue;
    }
    temp+=ch;
  }
  if(events.empty()){
    xpp::log_printf(XPP_LOG_WARN, " No events for condition %s \n",cond);
    return(1);
  }
  return(0);
}

int add_global(xpp::Session &s, const char *cond, int sign, const std::vector<FlagEvent> &events)
{
  xpp::Model &m=s.model();
  if(m.nflags>=MAXFLAG){
    xpp::log_printf(XPP_LOG_WARN, "Too many global conditions\n");
    return(1);
  }
  if(static_cast<int>(events.size())>MAX_EVENTS){
    xpp::log_printf(XPP_LOG_WARN, " Too many events per flag \n");
    return(1);
  }
  xpp::Model::GlobalFlag &f=m.flags[m.nflags];
  f.cond=cond;
  for(size_t e=0;e<events.size();e++){
    f.lhsname[e]=events[e].name;
    f.rhs[e]=events[e].formula;
  }
  f.sign=sign;
  f.nevents=static_cast<int>(events.size());
  m.nflags++;
  return(0);
}

/* expr compiled (add_expr), its ENDEXP included; false if it does not
   parse */
static bool compile(xpp::Session &s, const std::string &expr, std::vector<int> &out)
{
  int command[256];
  int nc;
  if(xpp::add_expr(s,expr,command,&nc))return false;
  out.assign(command,command+nc+1);
  return true;
}

int compile_flags(xpp::Session &s)
{
  std::array<xpp::Model::GlobalFlag,MAXFLAG> &flags=s.model().flags;
  int j;
  int i,index;
  if(s.model().nflags==0)return(0);
  for(j=0;j<s.model().nflags;j++){
    if(!compile(s,flags[j].cond,flags[j].comcond)){
      xpp::log(XPP_LOG_WARN, "Illegal global condition:  {}\n",flags[j].cond);
      return(1);
    }
    flags[j].anypars=0;
    flags[j].nointerp=0;
    for(i=0;i<flags[j].nevents;i++){
      const char *name=flags[j].lhsname[i].c_str();
      index=xpp::find_user_name(s.model(),IC,name);
      if(index<0){
	index=xpp::find_user_name(s.model(),PARAM,name);
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
      if(!compile(s,flags[j].rhs[i],flags[j].comrhs[i])){
	xpp::log(XPP_LOG_WARN, "Illegal event {} for global {}\n",
	       flags[j].rhs[i],flags[j].cond);
      return(1);
      }
    }
  }
  return(0);
}

/*  here is the shell code for a loop around  integration step  */

int one_flag_step(xpp::Session &s, double *yold, double *ynew, int *istart, double told, double *tnew, int neq, double *frac)
{
  std::array<xpp::Model::GlobalFlag,MAXFLAG> &flags=s.model().flags;
  double dt=*tnew-told;
  double f0,f1,tol,tolmin=1e-10;
  double smin=2;
  int sign,i,j,in,ncycle=0,newhit,nevents;

  if(s.model().nflags==0)return(0);
  for(i=0;i<s.model().nflags;i++){
    fstate[i].tstar=2.0;
    fstate[i].hit=0;
  }
  /* If this is the first call, then need f1  */
  if(*istart==1){  
    for(i=0;i<neq;i++)
      setvar(s,i+1,yold[i]);
    setvar(s,0,told);
    for(i=0;i<s.model().nflags;i++)
    *istart=0;
  
  }
  for(i=0;i<s.model().nflags;i++){
    sign=flags[i].sign;
    fstate[i].f0=fstate[i].f1;
    f0=fstate[i].f0;
    for(j=0;j<neq;j++)
      setvar(s,j+1,ynew[j]);
    setvar(s,0,*tnew);
    f1=xpp::evaluate(s,flags[i].comcond.data());
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
 
   if(smin<s.numerics.stol)smin=s.numerics.stol;
  else smin=(1+s.numerics.stol)*smin;  
  if(smin>1.0)return(0);

  *tnew=told+dt*smin;
  setvar(s,0,*tnew);
  for(i=0;i<neq;i++){
    ynew[i]=yold[i]+smin*(ynew[i]-yold[i]);
    setvar(s,i+1,ynew[i]);
  }
  for(i=0;i<s.model().nflags;i++)
    fstate[i].f0=xpp::evaluate(s,flags[i].comcond.data());
  while(1){ /* run through all possible events  */
    ncycle++;
    newhit=0;
    for(i=0;i<s.model().nflags;i++){
      nevents=flags[i].nevents;
      if(fstate[i].hit==ncycle&&fstate[i].tstar<=smin){
	for(j=0;j<nevents;j++){
	  fstate[i].vrhs[j]=xpp::evaluate(s,flags[i].comrhs[j].data());
	  in=flags[i].lhs[j];
	  if(flags[i].type[j]==0)
	        setvar(s,in+1,fstate[i].vrhs[j]);
	 
	}
      }
    }
    for(i=0;i<s.model().nflags;i++){
      nevents=flags[i].nevents;
      if(fstate[i].hit==ncycle&&fstate[i].tstar<=smin){
	for(j=0;j<nevents;j++){
	  
	  in=flags[i].lhs[j];
	  if(flags[i].type[j]==0){
	     ynew[in]=fstate[i].vrhs[j];
	     /* setvar(s,in+1,ynew[in]); if this screws up */
	  }
	  else {
	    if(flags[i].type[j]==1)
	      xpp::set_val(s,s.model().upar_names[in],fstate[i].vrhs[j]);
	    else{

	      if((flags[i].type[j]==2)&&(fstate[i].vrhs[j]>0))xpp::send_output(s,ynew,*tnew);
	      if((flags[i].type[j]==3)&&(fstate[i].vrhs[j]>0))xpp::send_halt(ynew,*tnew);
	    }
	  }

	}
	if(flags[i].anypars){
	  xpp::evaluate_derived(s);
	  xpp::redraw_params();
	}
      }
    }

    for(i=0;i<neq;i++){
      ynew[i]=getvar(s,i+1); /* if this screws up */
    }
    for(i=0;i<s.model().nflags;i++){
      fstate[i].f1=xpp::evaluate(s,flags[i].comcond.data());
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
 
  *frac=smin;
  return(1);
}

/*  here are the ODE drivers */

int one_flag_step_symp(xpp::Session &s, double *y, double dt, double *work, int neq, double *tim, int *istart)
{
  double yold[MAXODE],told;
  int i,hit;
  double frac,dtt=dt;
  int nstep=0; 
  while(1){
    for(i=0;i<neq;i++)
      yold[i]=y[i];
    told=*tim;
    xpp::one_step_symp(s,y,dtt,work,neq,tim);
    if((hit=one_flag_step(s,yold,y,istart,told,tim,neq,&frac))==0)
      break;
    /* Its a hit !! */
    nstep++;
    dtt=(1-frac)*dt;
    if(nstep>(s.model().nflags+2)){
      xpp::log_printf(XPP_LOG_WARN, " Working too hard?? ");
      xpp::log_printf(XPP_LOG_WARN, "smin=%g\n",frac);
      break;
    }
  }
  
  return(1);
}
 
int one_flag_step_euler(xpp::Session &s, double *y, double dt, double *work, int neq, double *tim, int *istart)
{
  double yold[MAXODE],told;
  int i,hit;
  double frac,dtt=dt;
  int nstep=0; 
  while(1){
    for(i=0;i<neq;i++)
      yold[i]=y[i];
    told=*tim;
    xpp::one_step_euler(s,y,dtt,work,neq,tim);
    if((hit=one_flag_step(s,yold,y,istart,told,tim,neq,&frac))==0)
      break;
    /* Its a hit !! */
    nstep++;
    dtt=(1-frac)*dt;
    if(nstep>(s.model().nflags+2)){
      xpp::log_printf(XPP_LOG_WARN, " Working too hard?? ");
      xpp::log_printf(XPP_LOG_WARN, "smin=%g\n",frac);
      break;
    }
  }
  
  return(1);
}
    
int one_flag_step_discrete(xpp::Session &s, double *y, double dt, double *work, int neq, double *tim, int *istart)
{
  double yold[MAXODE],told;
  int i,hit;
  double frac,dtt=dt;
  int nstep=0; 
  while(1){
    for(i=0;i<neq;i++)
      yold[i]=y[i];
    told=*tim;
    xpp::one_step_discrete(s,y,dtt,work,neq,tim);
    if((hit=one_flag_step(s,yold,y,istart,told,tim,neq,&frac))==0)
      break;
    /* Its a hit !! */
    nstep++;
    dtt=(1-frac)*dt;
    if(nstep>(s.model().nflags+2)){
      xpp::log_printf(XPP_LOG_WARN, " Working too hard?? ");
      xpp::log_printf(XPP_LOG_WARN, "smin=%g\n",frac);
      break;
    }
  }
  return(1);
}
     
int one_flag_step_heun(xpp::Session &s, double *y, double dt, double *yval[2], int neq, double *tim, int *istart)
{
  double yold[MAXODE],told;
  int i,hit;
  double frac,dtt=dt;
  int nstep=0; 
  while(1){
    for(i=0;i<neq;i++)
      yold[i]=y[i];
    told=*tim;
    xpp::one_step_heun(s,y,dtt,yval,neq,tim);
    if((hit=one_flag_step(s,yold,y,istart,told,tim,neq,&frac))==0)
      break;
    /* Its a hit !! */
    nstep++;
    dtt=(1-frac)*dt;
    if(nstep>(s.model().nflags+2)){
      xpp::log_printf(XPP_LOG_WARN, " Working too hard? ");
      xpp::log_printf(XPP_LOG_WARN, " smin=%g\n",frac);
      break;
    }
  }
  return(1);
}
    
int one_flag_step_rk4(xpp::Session &s, double *y, double dt, double *yval[3], int neq, double *tim, int *istart)
{
  double yold[MAXODE],told;
  int i,hit;
  double frac,dtt=dt;
  int nstep=0; 
  while(1){
    for(i=0;i<neq;i++)
      yold[i]=y[i];
    told=*tim;
    xpp::one_step_rk4(s,y,dtt,yval,neq,tim);
    if((hit=one_flag_step(s,yold,y,istart,told,tim,neq,&frac))==0)
      break;
    /* Its a hit !! */
    nstep++;
    dtt=(1-frac)*dt;
    if(nstep>(s.model().nflags+2)){
      xpp::log_printf(XPP_LOG_WARN, " Working too hard?");
            xpp::log_printf(XPP_LOG_WARN, "smin=%g\n",frac);
      break;
    }
  }
  return(1);
}

int one_flag_step_gear(xpp::Session &s, int neq, double *t, double tout, double *y, double hmin, double hmax, double eps, int mf, double *error, int *kflag, int *jstart, double *work, int *iwork)
{
    double yold[MAXODE],told;
  int i,hit;
  double frac;
  int nstep=0; 
  while(1){
    for(i=0;i<neq;i++)
      yold[i]=y[i];
    told=*t;
    xpp::ggear(s,neq,t,tout,y, hmin,hmax,eps,mf,error,kflag,jstart,work,iwork);
    if(*kflag<0) break;
    if((hit=one_flag_step(s,yold,y,jstart,told,t,neq,&frac))==0)
      break;
    /* Its a hit !! */
    nstep++;
    *jstart=0; /* for gear always reset  */
    if(*t==tout)break;
    if(nstep>(s.model().nflags+2)){
      xpp::log_printf(XPP_LOG_WARN, " Working too hard? ");
            xpp::log_printf(XPP_LOG_WARN, "smin=%g\n",frac);
      break;
    }
  }
  return 0;

}
int one_flag_step_rosen(xpp::Session &s, double *y,double *tstart,double tfinal,
int *istart,int n,double *work,int *ierr)
{
   double yold[MAXODE],told;
  int i,ok,hit;
  double frac;
  int nstep=0; 
  while(1){
    for(i=0;i<n;i++)
      yold[i]=y[i];
    told=*tstart;
    ok=xpp::rosen(s,y,tstart,tfinal,istart,n,work,ierr);
    if(ok==-1) break;
    if((hit=one_flag_step(s,yold,y,istart,told,tstart,n,&frac))==0)
      break;
    /* Its a hit !! */
    nstep++;
    
    if(*tstart==tfinal)break;
    if(nstep>(s.model().nflags+2)){
      xpp::log_printf(XPP_LOG_WARN, " Working too hard? ");
            xpp::log_printf(XPP_LOG_WARN, "smin=%g\n",frac);
      *ierr=-2;
      return 1;
      break;
    }
  }
  return 0;

}

int one_flag_step_dp(xpp::Session &s, int *istart, double *y, double *t, int n, double tout, double *tol, double *atol, int flag, int *kflag, double *work)
{
   double yold[MAXODE],told;
  int i,hit;
  double frac;
  int nstep=0; 
  while(1){
    for(i=0;i<n;i++)
      yold[i]=y[i];
    told=*t;
    dormprin(s,istart,y,t,n,tout,tol,atol,flag,kflag,work);
    if(*kflag!=1) break;
    if((hit=one_flag_step(s,yold,y,istart,told,t,n,&frac))==0)
      break;
    /* Its a hit !! */
    nstep++;
    
    if(*t==tout)break;
    if(nstep>(s.model().nflags+2)){
      xpp::log_printf(XPP_LOG_WARN, " Working too hard? ");
            xpp::log_printf(XPP_LOG_WARN, "smin=%g\n",frac);
      return 1;
      break;
    }
  }
  return 0;

}

#ifdef CVODE_YES
int one_flag_step_cvode(xpp::Session &s, xpp::CvodeRun &run, int *command, double *y, double *t, int n, double tout, int *kflag, double *atol, double *rtol)  /* command =0 continue, 1 is start 2 finish */
{
    double yold[MAXODE],told;
  int i,hit,neq=n;
  double frac;
  int nstep=0; 
  while(1){
    for(i=0;i<neq;i++)
      yold[i]=y[i];
    told=*t;
    xpp::ccvode(s,run,command,y,t,n,tout,kflag,atol,rtol);
    if(*kflag<0) break;
    if((hit=one_flag_step(s,yold,y,command,told,t,neq,&frac))==0)
      break;
    /* Its a hit !! */
    nstep++;
   xpp::end_cv(run);
    *command=1; /* for cvode always reset  */
    if(*t==tout)break;
    if(nstep>(s.model().nflags+2)){
      xpp::log_printf(XPP_LOG_WARN, " Working too hard? ");
            xpp::log_printf(XPP_LOG_WARN, "smin=%g\n",frac);
      return 1;
    }
  }
  return 0;

}

#endif
int one_flag_step_adap(xpp::Session &s, double *y, int neq, double *t, double tout, double eps, double *hguess, double hmin, double *work, int *ier, double epjac, int iflag, int *jstart)
{
    double yold[MAXODE],told;
  int i,hit;
  double frac;
  int nstep=0; 
  while(1){
    for(i=0;i<neq;i++)
      yold[i]=y[i];
    told=*t;
    xpp::gadaptive(s,y,neq,t,tout,eps,
		     hguess,hmin,work,ier,epjac,iflag,jstart);
    if(*ier) break;
    if((hit=one_flag_step(s,yold,y,jstart,told,t,neq,&frac))==0)
      break;
    /* Its a hit !! */
    nstep++;
    
    if(*t==tout)break;
    if(nstep>(s.model().nflags+2)){
      xpp::log_printf(XPP_LOG_WARN, " Working too hard? ");
            xpp::log_printf(XPP_LOG_WARN, "smin=%g\n",frac);
      break;
    }
  }
  return 0;

}

int one_flag_step_backeul(xpp::Session &s, double *y, double *t, double dt, int neq, double *yg, double *yp, double *yp2, double *ytemp, double *errvec, double *jac, int *istart)
{
  double yold[MAXODE],told;
  int i,hit,j;
  double frac;
  double dtt=dt; 
  int nstep=0; 
  while(1){
    for(i=0;i<neq;i++)
      yold[i]=y[i];
    told=*t;
    if((j=xpp::one_bak_step(s,y,t,dtt,neq,yg,yp,yp2,ytemp,errvec,jac,istart))!=0)
      return(j);
    if((hit=one_flag_step(s,yold,y,istart,told,t,neq,&frac))==0)
      break;
    /* Its a hit !! */
    nstep++;
    dtt=(1-frac)*dt;  
    if(nstep>(s.model().nflags+2)){
      xpp::log_printf(XPP_LOG_WARN, " Working too hard?");
            xpp::log_printf(XPP_LOG_WARN, "smin=%g\n",frac);
      break;
    }
  }
  return 0;
}

