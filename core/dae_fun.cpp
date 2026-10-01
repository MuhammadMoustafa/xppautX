
#include "dae_fun.h"
#include "session.h"
#include "expr.h"

#include <stdlib.h> 
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "getvar.h"
#include "xpp_log.h"
#include "xpp_math.h"
#include "xpp_ui.h"
#include "form_ode.h"
#include "load_eqn.h"
#include <string>
#include <string_view>
#include <vector>
#include "model.h"

namespace xpp {


/*    will have more stuff someday */

typedef struct {
  std::vector<double> work;
  std::vector<int> iwork;
  int status;
} DAEWORK;
static DAEWORK dae_work;

namespace {
/* each algebraic variable's last solution (xpp::Model has the
   definitions): the next solve's first guess */
std::array<double,MAXDAE> svar_last{};
}

/* this adds an algebraically defined variable  and a formula
   for the first guess */

int add_svar(xpp::Session &s, const char *name, const char *rhs)
{
  if(s.model().nsvar>=MAXDAE){
    xpp::log_printf(XPP_LOG_ERROR, " Too many variables\n");
    return 1;
  }
  s.model().svars[s.model().nsvar].name=name;
  s.model().svars[s.model().nsvar].rhs=rhs;
  xpp::log_printf(XPP_LOG_INFO, " Added sol-var[%d] %s = %s \n",
	 s.model().nsvar,s.model().svars[s.model().nsvar].name.c_str(),s.model().svars[s.model().nsvar].rhs.c_str());
  s.model().nsvar++;
return 0;
 }

/* adds algebraically define name to name list */

int add_svar_names(xpp::Session &s)
{
  xpp::Model &m=s.model();
  int i;
  for(i=0;i<m.nsvar;i++){
     m.svars[i].index=m.nvar;
    if(add_var(s,m.svars[i].name.c_str(),0.0)==1)
      return 1;
  }
  return 0;
}

/* adds a right-hand side to slove for zero */

int add_aeqn(xpp::Session &s, const char *rhs)
{
  xpp::Model &m=s.model();
  if(m.naeqn>=MAXDAE){
    xpp::log_printf(XPP_LOG_ERROR, " Too many equations\n");
    return 1;
  }
  m.aeqns[m.naeqn].rhs=rhs;
  m.naeqn++;
 return 0;
}

/* this compiles formulas to set to zero */
int compile_svars(xpp::Session &s)
{
  xpp::Model &m=s.model();
  int i,f[256],n;
  if(m.nsvar!=m.naeqn){
    xpp::log_printf(XPP_LOG_ERROR, " #DaeSolVar(%d) must equal #ALG_EQN(%d) ! \n",m.nsvar,m.naeqn);
    return 1;
  }
  
  for(i=0;i<m.naeqn;i++){
    if(add_expr(s,m.aeqns[i].rhs.c_str(),f,&n)==1){
    xpp::log_printf(XPP_LOG_ERROR, " Bad right-hand side for alg-eqn \n");
    return(1);
    }
    /* n+2, zero-padded like the xpp_malloc block this replaces: evaluate(s,)
       may read a couple of entries past the parsed length. */
    m.aeqns[i].form.assign(f, f+n);
    m.aeqns[i].form.resize(n+2, 0);
  }

   for(i=0;i<m.nsvar;i++){
    if(add_expr(s,m.svars[i].rhs.c_str(),f,&n)==1){
    xpp::log_printf(XPP_LOG_ERROR, " Bad initial guess for sol-var \n");
    return(1);
    }
    m.svars[i].form.assign(f, f+n);
    m.svars[i].form.resize(100, 0);
   }
     init_dae_work(s);
   return 0;
 
}

void reset_dae(xpp::Session &s)
{
  dae_work.status=1;
}
void set_init_guess(xpp::Session &s)
{
  xpp::Model &m=s.model();
  int i;
  double z;
    dae_work.status=1;
  if(m.nsvar==0)return;
  for(i=0;i<m.nsvar;i++){
   z=evaluate(s,m.svars[i].form.data());
    setvar(s,m.svars[i].index,z);
    svar_last[i]=z;
  }
}
namespace {
/* why solve_dae failed (its status -1, -2 or -3), as a value */
xpp::Error dae_failure(xpp::Session &s, int status)
{
  switch(status){
  case -1: return {"DAE"," Singular jacobian for dae\n"};
  case -2: return {"DAE"," Maximum iterates exceeded for dae\n"};
  case -3: return {"DAE"," Newton update out of bounds\n"};
  }
  return {"DAE",""};
}
}

void init_dae_work(xpp::Session &s)
{
  xpp::Model &m=s.model();

  dae_work.work.assign(m.nsvar*m.nsvar+10*m.nsvar, 0.0);
  dae_work.iwork.assign(m.nsvar, 0);
  dae_work.status=1;
}

void get_dae_fun(xpp::Session &s, double *y, double *f)
{
  xpp::Model &m=s.model();
  int i;
  /* better do this in case fixed variables depend on sol_var */
  for(i=0;i<m.nsvar;i++)
    setvar(s,m.svars[i].index,y[i]);
  for(i=m.node;i<m.node+m.fix_var;i++)
    setvar(s,i+1,evaluate(s,m.programs[i].data()));
  for(i=0;i<m.naeqn;i++)
    f[i]=evaluate(s,m.aeqns[i].form.data());
}

void do_daes(xpp::Session &s)
{
  int ans;
  ans=solve_dae(s);
  dae_work.status=ans;
  if(ans==1||ans==2)return; /* accepts a no change error! */
  /* the integration stops after this step and returns it */
  if(!s.integrator.step_error)
    s.integrator.step_error=dae_failure(s,ans);

}

/* Newton solver for algebraic stuff */
int solve_dae(xpp::Session &s)
{
  xpp::Model &m=s.model();
  int i,j,n;
  int info;
  double err,del,z,yold;
  double tol=s.numerics.evec_err,eps=s.numerics.newt_err;
  int maxit=s.numerics.evec_iter,iter=0;
  double *y,*ynew,*f,*fnew,*jac,*errvec;
  n=m.nsvar;
  if(m.nsvar==0)return 1;
  if(dae_work.status<0)return dae_work.status; /* accepts no change error */
  y=dae_work.work.data();
  f=y+m.nsvar;
  fnew=f+m.nsvar;
  ynew=fnew+m.nsvar;
  errvec=ynew+m.nsvar;
  jac=errvec+m.nsvar;
  for(i=0;i<n;i++){ /* copy current value as initial guess */
    y[i]=svar_last[i];
    ynew[i]=y[i]; /* keep old guess */
  }
  while(1){
    get_dae_fun(s,y,f);
    err=0.0;
    for(i=0;i<n;i++){
      err+=fabs(f[i]);
      errvec[i]=f[i];
    }
    if(err<tol){ /* success */
      for(i=0;i<n;i++){
	setvar(s,m.svars[i].index,y[i]);
	svar_last[i]=y[i];
      }
      return 1; 
    }
    /* compute jacobian */
    for(i=0;i<n;i++){
      z=fabs(y[i]);
      if(z<eps)z=eps;
      del=eps*z;
      yold=y[i];
      y[i]=y[i]+del;
      get_dae_fun(s,y,fnew);
      for(j=0;j<n;j++)
	jac[j*n+i]=(fnew[j]-f[j])/del;
      y[i]=yold;
    }
    xpp::sgefa(jac,n,n,dae_work.iwork.data(),&info);
    if(info!=-1){
      for(i=0;i<n;i++)
	setvar(s,m.svars[i].index,ynew[i]);
      return -1; /* singular jacobian */
    }
    xpp::sgesl(jac,n,n,dae_work.iwork.data(),errvec); /* get x=J^(-1) f */
    err=0.0;
    for(i=0;i<n;i++){
      y[i]-=errvec[i];
      err+=fabs(errvec[i]);
    } 
    if(err>(n*s.numerics.bound)){
      for(i=0;i<n;i++)
	setvar(s,m.svars[i].index,svar_last[i]);
      return(-3); /* getting too big */
    }
    if(err<tol) /* not much change */
      {
	for(i=0;i<n;i++){
	  setvar(s,m.svars[i].index,y[i]);
	  svar_last[i]=y[i];
	}
	return 2;
      }
    iter++;
    if(iter>maxit){
      for(i=0;i<n;i++)
	setvar(s,m.svars[i].index,svar_last[i]);
      return(-2); /* too many iterates */
    }
  }
}

/* interface shit -- different for Win95 */
 
void get_new_guesses(xpp::Session &s)
{
  xpp::Model &m=s.model();
  int i,n;
  /* new_string_of edits m.svars[i].rhs in place */
  double z;
  if(m.nsvar<1)return;
  for(i=0;i<m.nsvar;i++){
    z=svar_last[i];
    const std::string name=xpp::format("Initial {}({:g}):",
      m.svars[i].name,z);
    new_string_of(name.c_str(),m.svars[i].rhs,XPP_FIELD_EXPRESSION);
    if(add_expr(s,m.svars[i].rhs.c_str(),m.svars[i].form.data(),&n)){
      err_msg("Illegal formula");
      return;
    }
    z=evaluate(s,m.svars[i].form.data());
    setvar(s,m.svars[i].index,z);
    svar_last[i]=z;
  }
}

} // namespace xpp
