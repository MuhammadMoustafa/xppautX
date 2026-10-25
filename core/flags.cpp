#include "model.h"
#include "my_rhs.h"
#include "session.h"
#include "flags.h"
#include "form_ode.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "xpp_log.h"

#include "cv2.h"
#include "stiff.h"
#include "derived.h"
#include "gear.h"
#include "integrate.h"
#include "expr.h"

#include <algorithm>
#include <functional>
#include <vector>
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



int split_events(const char *cond, const char *rest, std::vector<FlagEvent> &events)
{
  std::string temp,name;
  events.clear();
  for(const char *p=rest;*p;p++){
    char ch=*p;
    if(ch=='{'||ch==' ')continue;
    if(ch=='}'||ch==';'){
      if(static_cast<int>(events.size())==MAX_EVENTS){
	xpp::log(XPP_LOG_WARN, " Too many events per flag \n");
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
    xpp::log(XPP_LOG_WARN, " No events for condition {} \n",cond);
    return(1);
  }
  return(0);
}

int add_global(xpp::Session &s, const char *cond, int sign, const std::vector<FlagEvent> &events)
{
  xpp::Model &m=s.model();
  if(static_cast<int>(events.size())>MAX_EVENTS){
    xpp::log(XPP_LOG_WARN, " Too many events per flag \n");
    return(1);
  }
  xpp::Model::GlobalFlag &f=m.flags.emplace_back();
  f.cond=cond;
  for(size_t e=0;e<events.size();e++){
    f.lhsname[e]=events[e].name;
    f.rhs[e]=events[e].formula;
  }
  f.sign=sign;
  f.nevents=static_cast<int>(events.size());
  return(0);
}

/* expr compiled (add_expr), its ENDEXP included; false if it does not
   parse */
static bool compile(xpp::Session &s, const std::string &expr, std::vector<int> &out)
{
  int nc;
  if(xpp::add_expr(s,expr,out,&nc))return false;
  return true;
}

int compile_flags(xpp::Session &s)
{
  std::vector<xpp::Model::GlobalFlag> &flags=s.model().flags;
  int j;
  int i,index;
  if(s.model().flag_count()==0)return(0);
  s.integrator.flags.assign(s.model().flag_count(),xpp::FlagState{});
  for(j=0;j<s.model().flag_count();j++){
    if(!compile(s,flags[j].cond,flags[j].comcond.rpn)){
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
      if(!compile(s,flags[j].rhs[i],flags[j].comrhs[i].rpn)){
	xpp::log(XPP_LOG_WARN, "Illegal event {} for global {}\n",
	       flags[j].rhs[i],flags[j].cond);
      return(1);
      }
    }
  }
  return(0);
}

/*  here is the shell code for a loop around  integration step  */

namespace {
void apply_flag_hits(xpp::Session &s, double *ynew, double time, int neq, double smin)
{
  auto &flags=s.model().flags;
  int i,j,in,ncycle=0,newhit,nevents,sign;
  double f0,f1,tol;
  constexpr double tolmin=1e-10; // Existing cascade threshold, shared with sampled flags.
  double *tnew=&time;
  for(i=0;i<s.model().flag_count();i++)
    s.integrator.flags[i].f0=xpp::evaluate(s,flags[i].comcond);
  while(1){ /* run through all possible events  */
    ncycle++;
    newhit=0;
    for(i=0;i<s.model().flag_count();i++){
      nevents=flags[i].nevents;
      if(s.integrator.flags[i].hit==ncycle&&s.integrator.flags[i].tstar<=smin){
	for(j=0;j<nevents;j++){
	  s.integrator.flags[i].vrhs[j]=xpp::evaluate(s,flags[i].comrhs[j]);
	  in=flags[i].lhs[j];
	  if(flags[i].type[j]==0)
	        setvar(s,in+1,s.integrator.flags[i].vrhs[j]);
	 
	}
      }
    }
    for(i=0;i<s.model().flag_count();i++){
      nevents=flags[i].nevents;
      if(s.integrator.flags[i].hit==ncycle&&s.integrator.flags[i].tstar<=smin){
	for(j=0;j<nevents;j++){
	  
	  in=flags[i].lhs[j];
	  if(flags[i].type[j]==0){
	     ynew[in]=s.integrator.flags[i].vrhs[j];
	     /* setvar(s,in+1,ynew[in]); if this screws up */
	  }
	  else {
	    if(flags[i].type[j]==1)
	      xpp::set_val(s,s.model().upar_names[in],s.integrator.flags[i].vrhs[j]);
	    else{

	      if((flags[i].type[j]==2)&&(s.integrator.flags[i].vrhs[j]>0))xpp::send_output(s,ynew,*tnew);
	      if((flags[i].type[j]==3)&&(s.integrator.flags[i].vrhs[j]>0))xpp::send_halt(s);
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
    for(i=0;i<s.model().flag_count();i++){
      s.integrator.flags[i].f1=xpp::evaluate(s,flags[i].comcond);
      if(s.integrator.flags[i].hit>0)continue; /* already hit so dont do anything */
      f1=s.integrator.flags[i].f1;
      sign=flags[i].sign;
      f0=s.integrator.flags[i].f0;
      tol=fabs(f1-f0);
      switch(sign){
      case 1:
	if(f0<=0.0&&f1>=0.0&&tol>tolmin){
	  s.integrator.flags[i].tstar=smin;
	  s.integrator.flags[i].hit=ncycle+1;
	  newhit=1;
	}
	break;
      case -1:
	if(f0>=0.0&&f1<=0.0&&tol>tolmin){
	  s.integrator.flags[i].tstar=smin;
	  s.integrator.flags[i].hit=ncycle+1;
	  newhit=1;
	}
	break; 
      case 0:
	if(f0*f1<=0&&(f1!=0||f0!=0)&&tol>tolmin){
	  s.integrator.flags[i].tstar=smin;
	  s.integrator.flags[i].hit=ncycle+1;
	  newhit=1;
	}
      }
    }
    if(newhit==0)break;
  }
 
}
}

void apply_dense_flags(xpp::Session &s, double *y, double time, int neq, std::span<const int> hits)
{
  for(int i=0;i<s.model().flag_count();++i){
    s.integrator.flags[i].hit=0;
    s.integrator.flags[i].tstar=0;
  }
  for(int i:hits)s.integrator.flags[i].hit=1;
  set_fix_rhs(s,time,y);
  apply_flag_hits(s,y,time,neq,0);
}

int one_flag_step(xpp::Session &s, double *yold, double *ynew, int *istart, double told, double *tnew, int neq, double *frac)
{
  std::vector<xpp::Model::GlobalFlag> &flags=s.model().flags;
  double dt=*tnew-told;
  double f0,f1,tol,tolmin=1e-10;
  double smin=2;
  int sign,i,j,ncycle=0;

  if(s.model().flag_count()==0)return(0);
  for(i=0;i<s.model().flag_count();i++){
    s.integrator.flags[i].tstar=2.0;
    s.integrator.flags[i].hit=0;
  }
  /* If this is the first call, then need f1  */
  if(*istart==1){
    for(i=0;i<neq;i++)
      setvar(s,i+1,yold[i]);
    setvar(s,0,told);
    for(i=0;i<s.model().flag_count();i++)
    *istart=0;

  }
  for(i=0;i<s.model().flag_count();i++){
    sign=flags[i].sign;
    s.integrator.flags[i].f0=s.integrator.flags[i].f1;
    f0=s.integrator.flags[i].f0;
    for(j=0;j<neq;j++)
      setvar(s,j+1,ynew[j]);
    setvar(s,0,*tnew);
    f1=xpp::evaluate(s,flags[i].comcond);
    s.integrator.flags[i].f1=f1;
    tol=fabs(f1-f0);
    switch(sign){
    case 1:
      if(f0<0.0&&f1>=0.0&&tol>tolmin){
	s.integrator.flags[i].hit=ncycle+1;
	s.integrator.flags[i].tstar=f0/(f0-f1);
      }
      break;
    case -1:
      if(f0>0.0&&f1<=0.0&&tol>tolmin){
	s.integrator.flags[i].hit=ncycle+1;
	s.integrator.flags[i].tstar=f0/(f0-f1);
      }
      break;
    case 0:
      if(fabs(f1)<MY_DBL_EPS){
	s.integrator.flags[i].hit=ncycle+1;
	s.integrator.flags[i].tstar=told;
      }
      break;
    }
    if(flags[i].nointerp==1)
      {
	s.integrator.flags[i].tstar=1.0;
      }

      if(smin>s.integrator.flags[i].tstar)smin=s.integrator.flags[i].tstar;

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
  apply_flag_hits(s,ynew,*tnew,neq,smin);

  *frac=smin;
  return(1);
}

/*  here are the ODE drivers: every one is flag_loop around its own step  */

namespace {

/* How a flag_loop ended: no event in the last step, a step that failed (the
   caller holds its code), or events that kept coming. */
enum class FlagEnd { done, step_failed, too_hard };

struct NoHit {
  void operator()() const {}
};

/* Take the method's step; when an event fired inside it, go back to the
   event (dt shortened by the fraction left) and step again, up to the flag count+2
   times. step(dtt) takes the step of length dtt (a method with no fixed
   step ignores it) and says whether it can go on; on_hit runs after each
   crossing (an integrator that must restart). With tend, the loop also ends
   when the step reaches that time. */
template <class Step, class Hit = NoHit>
FlagEnd flag_loop(xpp::Session &s, double *y, int neq, double *t, int *istart,
                  double dt, Step &&step, const double *tend = nullptr,
                  Hit &&on_hit = Hit{})
{
  std::vector<double> &yold=s.solver_work.flag_yold;
  yold.resize(neq);
  double frac,dtt=dt;
  int nstep=0;
  while(1){
    std::copy(y,y+neq,yold.begin());
    double told=*t;
    if(!step(dtt))return FlagEnd::step_failed;
    if(one_flag_step(s,yold.data(),y,istart,told,t,neq,&frac)==0)
      return FlagEnd::done;
    /* Its a hit !! */
    nstep++;
    dtt=(1-frac)*dt;
    on_hit();
    if(tend&&*t==*tend)return FlagEnd::done;
    if(nstep>(s.model().flag_count()+2)){
      xpp::log(XPP_LOG_WARN, " Working too hard?? smin={:g}\n",frac);
      return FlagEnd::too_hard;
    }
  }
}

}

void one_flag_steps(xpp::Session &s, double *y, int neq, double *tim, int *istart, double dt, const std::function<void(double)> &step)
{
  flag_loop(s,y,neq,tim,istart,dt,[&](double dtt){ step(dtt); return true; });
}

int one_flag_step_gear(xpp::Session &s, int neq, double *t, double tout, double *y, double hmin, double hmax, double eps, int mf, double *error, int *kflag, int *jstart, double *work, int *iwork)
{
  flag_loop(s,y,neq,t,jstart,0,[&](double){
    xpp::ggear(s,neq,t,tout,y, hmin,hmax,eps,mf,error,kflag,jstart,work,iwork);
    return *kflag>=0; },
    &tout,[&]{ *jstart=0; /* for gear always reset  */ });
  return 0;
}

int one_flag_step_rosen(xpp::Session &s, double *y,double *tstart,double tfinal,
int *istart,int n,double *work,int *ierr)
{
  FlagEnd end=flag_loop(s,y,n,tstart,istart,0,[&](double){
    return xpp::rosen(s,y,tstart,tfinal,istart,n,work,ierr)!=-1; },&tfinal);
  if(end!=FlagEnd::too_hard)return 0;
  *ierr=-2;
  return 1;
}

int one_flag_step_cvode(xpp::Session &s, xpp::CvodeRun &run, int *command, double *y, double *t, int n, double tout, int *kflag, double *atol, double *rtol)  /* command =0 continue, 1 is start 2 finish */
{
  FlagEnd end=flag_loop(s,y,n,t,command,0,[&](double){
    xpp::ccvode(s,run,command,y,t,n,tout,kflag,atol,rtol);
    return *kflag>=0; },
    &tout,[&]{ xpp::end_cv(run); *command=1; /* for cvode always reset  */ });
  return end==FlagEnd::too_hard;
}

int one_flag_step_adap(xpp::Session &s, double *y, int neq, double *t, double tout, double eps, double *hguess, double hmin, double *work, int *ier, double epjac, int iflag, int *jstart)
{
  flag_loop(s,y,neq,t,jstart,0,[&](double){
    xpp::gadaptive(s,y,neq,t,tout,eps,hguess,hmin,work,ier,epjac,iflag,jstart);
    return *ier==0; },&tout);
  return 0;
}

int one_flag_step_backeul(xpp::Session &s, double *y, double *t, double dt, int neq, double *yg, double *yp, double *yp2, double *errvec, double *jac, int *istart)
{
  int j=0;
  flag_loop(s,y,neq,t,istart,dt,[&](double dtt){
    j=xpp::one_bak_step(s,y,t,dtt,neq,yg,yp,yp2,errvec,jac,istart);
    return j==0; });
  return j;
}
