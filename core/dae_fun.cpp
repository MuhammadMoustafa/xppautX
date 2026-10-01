
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
    if(add_expr(s,m.aeqns[i].rhs,f,&n)==1){
    xpp::log_printf(XPP_LOG_ERROR, " Bad right-hand side for alg-eqn \n");
    return(1);
    }
    /* n+2, zero-padded like the xpp_malloc block this replaces: evaluate(s,)
       may read a couple of entries past the parsed length. */
    m.aeqns[i].form.assign(f, f+n);
    m.aeqns[i].form.resize(n+2, 0);
  }

   for(i=0;i<m.nsvar;i++){
    if(add_expr(s,m.svars[i].rhs,f,&n)==1){
    xpp::log_printf(XPP_LOG_ERROR, " Bad initial guess for sol-var \n");
    return(1);
    }
    m.svars[i].form.assign(f, f+n);
    m.svars[i].form.resize(100, 0);
   }
     init_dae_work(s);
   return 0;
 
}

/* the solver starts afresh: no failure */
void reset_dae(xpp::Session &s)
{
  s.dae.status=DAE_SOLVED;
}

DaeRun::DaeRun(xpp::Session &s) : s_(s), outer_(!s.dae.run)
{
  if(outer_)s_.dae.run.emplace();
}

DaeRun::~DaeRun()
{
  if(outer_)s_.dae.run.reset();
}
void set_init_guess(xpp::Session &s)
{
  xpp::Model &m=s.model();
  int i;
  double z;
  reset_dae(s);
  if(m.nsvar==0)return;
  for(i=0;i<m.nsvar;i++){
   z=evaluate(s,m.svars[i].form.data());
    setvar(s,m.svars[i].index,z);
    s.dae.svar_last[i]=z;
  }
}
namespace {
/* why solve_dae failed (its status, DAE_SINGULAR ... DAE_FOLD), as a
   value: where the solutions end, and why no step went further */
xpp::Error dae_failure(const xpp::Session &s, int status)
{
  const char *why="";
  switch(status){
  case DAE_SINGULAR: why="their Jacobian is singular"; break;
  case DAE_NO_CONVERGENCE: why="Newton's method did not converge (maximum iterates exceeded)"; break;
  case DAE_OUT_OF_BOUNDS: why="the Newton update went out of bounds"; break;
  case DAE_FOLD: why="their Jacobian changed sign, a fold where this branch of solutions ends"; break;
  }
  if(s.dae.run&&s.dae.run->last_t)
    return {"DAE",xpp::format("No solution of the algebraic equations past t={:g}: {}",*s.dae.run->last_t,why)};
  return {"DAE",xpp::format("No solution of the algebraic equations at t={:g}: {}",getvar(s,0),why)};
}
}

void init_dae_work(xpp::Session &s)
{
  xpp::Model &m=s.model();

  s.dae.work.assign(m.nsvar*m.nsvar+10*m.nsvar, 0.0);
  s.dae.iwork.assign(m.nsvar, 0);
  reset_dae(s);
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
  const int ans=solve_dae(s);
  s.dae.status=ans;
  if(ans==DAE_SOLVED)return;
  /* the integration stops after this step and returns it */
  if(!s.integrator.step_error)
    s.integrator.step_error=dae_failure(s,ans);
}

/* Newton solver for algebraic stuff. A solution is accepted only where the
   equations hold (the residual within the Newton tolerance) and on the
   branch the run has followed: every Jacobian Newton factors on the way
   from the last solution has that solution's determinant sign, which a
   continuous branch keeps; a sign change means Newton crossed where the
   Jacobian is singular, a fold, past which this branch has no solution
   (W127: Newton would otherwise wander to another branch, and a loose
   tolerance accept it). The first solve of a run sets the sign; a solve
   outside a run (DaeRun) follows no branch. */
int solve_dae(xpp::Session &s)
{
  xpp::Model &m=s.model();
  int i,j,n;
  int info;
  double err,del,z,yold;
  double tol=s.numerics.evec_err,eps=s.numerics.newt_err;
  int maxit=s.numerics.evec_iter,iter=0;
  int sign=0; /* the sign of the last Jacobian factored, 0 before one */
  DaeRunBranch *run=s.dae.run?&*s.dae.run:nullptr; /* none outside a run */
  double *y,*ynew,*f,*fnew,*jac,*errvec;
  n=m.nsvar;
  if(m.nsvar==0)return DAE_SOLVED;
  if(s.dae.status<0)return s.dae.status; /* failed: until the run restarts */
  y=s.dae.work.data();
  f=y+m.nsvar;
  fnew=f+m.nsvar;
  ynew=fnew+m.nsvar;
  errvec=ynew+m.nsvar;
  jac=errvec+m.nsvar;
  for(i=0;i<n;i++){ /* copy current value as initial guess */
    y[i]=s.dae.svar_last[i];
    ynew[i]=y[i]; /* keep old guess */
  }
  while(1){
    get_dae_fun(s,y,f);
    err=0.0;
    for(i=0;i<n;i++){
      err+=fabs(f[i]);
      errvec[i]=f[i];
    }
    if(err<tol){ /* the equations hold */
      if(run){
	if(sign!=0)run->jac_sign=sign;
	run->last_t=getvar(s,0);
      }
      for(i=0;i<n;i++){
	setvar(s,m.svars[i].index,y[i]);
	s.dae.svar_last[i]=y[i];
      }
      return DAE_SOLVED;
    }
    if(iter>maxit){
      for(i=0;i<n;i++)
	setvar(s,m.svars[i].index,s.dae.svar_last[i]);
      return DAE_NO_CONVERGENCE;
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
    xpp::sgefa(jac,n,n,s.dae.iwork.data(),&info);
    if(info!=-1){
      for(i=0;i<n;i++)
	setvar(s,m.svars[i].index,ynew[i]);
      return DAE_SINGULAR;
    }
    sign=xpp::sgefa_det_sign(jac,n,n,s.dae.iwork.data());
    if(run&&run->jac_sign!=0&&sign!=run->jac_sign){
      /* Newton has crossed where the Jacobian is singular: no solution
	 near the last one continues its branch */
      for(i=0;i<n;i++)
	setvar(s,m.svars[i].index,s.dae.svar_last[i]);
      return DAE_FOLD;
    }
    xpp::sgesl(jac,n,n,s.dae.iwork.data(),errvec); /* get x=J^(-1) f */
    err=0.0;
    for(i=0;i<n;i++){
      y[i]-=errvec[i];
      err+=fabs(errvec[i]);
    } 
    if(err>(n*s.numerics.bound)){
      for(i=0;i<n;i++)
	setvar(s,m.svars[i].index,s.dae.svar_last[i]);
      return DAE_OUT_OF_BOUNDS;
    }
    /* a small update is not a solution: the loop's top accepts y only
       where the residual is within the tolerance */
    iter++;
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
    z=s.dae.svar_last[i];
    const std::string name=xpp::format("Initial {}({:g}):",
      m.svars[i].name,z);
    new_string_of(name.c_str(),m.svars[i].rhs,XPP_FIELD_EXPRESSION);
    if(add_expr(s,m.svars[i].rhs,m.svars[i].form.data(),&n)){
      err_msg("Illegal formula");
      return;
    }
    z=evaluate(s,m.svars[i].form.data());
    setvar(s,m.svars[i].index,z);
    s.dae.svar_last[i]=z;
  }
}

} // namespace xpp
