
#include "dae_fun.h"
#include "xpp_mem.h"
#include "gear.h"
#include "parserslow.h"

#include "ggets.h"
#include <stdlib.h> 
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "xpplim.h"
#include "getvar.h"
#include "xpp_io.h"
#include "xpp_log.h"
#include "xpp_ui.h"
#include <string>
#include <vector>
#define MAXDAE 400

extern double variables[];
extern int NVAR;

extern int DelayErr;

extern double EVEC_ERR,NEWT_ERR,BOUND;
extern int EVEC_ITER;

extern int NODE,FIX_VAR;
extern int *my_ode[];

/*    will have more stuff someday */

typedef struct {
  std::vector<double> work;
  std::vector<int> iwork;
  int status;
} DAEWORK;
DAEWORK dae_work;

typedef struct {
  std::string name, rhs;
  std::vector<int> form;
  int index;
  double value,last;
}SOL_VAR;

typedef struct {
  std::string rhs;
  std::vector<int> form;
} DAE_EQN;

SOL_VAR svar[MAXDAE];
DAE_EQN aeqn[MAXDAE];

int nsvar=0,naeqn=0;


/* this adds an algebraically defined variable  and a formula
   for the first guess */

int add_svar(const char *name, const char *rhs)
{
  if(nsvar>=MAXDAE){
    xpp_log(XPP_LOG_ERROR, " Too many variables\n");
    return 1;
  }
  if(name_too_long(name))return 1;
  svar[nsvar].name=name;
  svar[nsvar].rhs=rhs;
  xpp_log(XPP_LOG_INFO, " Added sol-var[%d] %s = %s \n",
	 nsvar,svar[nsvar].name.c_str(),svar[nsvar].rhs.c_str());
  nsvar++;
return 0;
 }

/* adds algebraically define name to name list */

int add_svar_names()
{
  int i;
  for(i=0;i<nsvar;i++){
     svar[i].index=NVAR;
    if(add_var(svar[i].name.c_str(),0.0)==1)
      return 1;
  }
  return 0;
}

/* adds a right-hand side to slove for zero */

int add_aeqn(const char *rhs)
{
  if(naeqn>=MAXDAE){
    xpp_log(XPP_LOG_ERROR, " Too many equations\n");
    return 1;
  }
  aeqn[naeqn].rhs=rhs;
  naeqn++;
 return 0;
}


/* this compiles formulas to set to zero */
int compile_svars()
{
  int i,f[256],n;
  if(nsvar!=naeqn){
    xpp_log(XPP_LOG_ERROR, " #SOL_VAR(%d) must equal #ALG_EQN(%d) ! \n",nsvar,naeqn);
    return 1;
  }
  
  for(i=0;i<naeqn;i++){
    if(add_expr(aeqn[i].rhs.c_str(),f,&n)==1){
    xpp_log(XPP_LOG_ERROR, " Bad right-hand side for alg-eqn \n");
    return(1);
    }
    /* n+2, zero-padded like the xpp_malloc block this replaces: evaluate()
       may read a couple of entries past the parsed length. */
    aeqn[i].form.assign(f, f+n);
    aeqn[i].form.resize(n+2, 0);
  }

   for(i=0;i<nsvar;i++){
    if(add_expr(svar[i].rhs.c_str(),f,&n)==1){
    xpp_log(XPP_LOG_ERROR, " Bad initial guess for sol-var \n");
    return(1);
    }
    svar[i].form.assign(f, f+n);
    svar[i].form.resize(100, 0);
   }
     init_dae_work();
   return 0;
 
}

void reset_dae()
{
  dae_work.status=1;
}
void set_init_guess()
{
  int i;
  double z;
    dae_work.status=1;
  if(nsvar==0)return;
  for(i=0;i<nsvar;i++){
   z=evaluate(svar[i].form.data());
    SETVAR(svar[i].index,z);
    svar[i].value=z;
    svar[i].last=z;
  }
}
void err_dae()
{
  
  switch(dae_work.status){
  case 2: 
    err_msg(" Warning - no change in Iterates");
    break;
  case -1:
    err_msg(" Singular jacobian for dae\n");
    
    break;
  case -2:
    err_msg(" Maximum iterates exceeded for dae\n");
    
    break;
  case -3:
    err_msg(" Newton update out of bounds\n");
    break;
  }
  dae_work.status=1;
}

void init_dae_work()
{

  dae_work.work.assign(nsvar*nsvar+10*nsvar, 0.0);
  dae_work.iwork.assign(nsvar, 0);
  dae_work.status=1;
}

void get_dae_fun(double *y, double *f)
{
  int i;
  /* better do this in case fixed variables depend on sol_var */
  for(i=0;i<nsvar;i++)
    SETVAR(svar[i].index,y[i]);
  for(i=NODE;i<NODE+FIX_VAR;i++)
    SETVAR(i+1,evaluate(my_ode[i]));
  for(i=0;i<naeqn;i++)
    f[i]=evaluate(aeqn[i].form.data());
}

void do_daes()
{
  int ans;
  ans=solve_dae();
  dae_work.status=ans;
  if(ans==1||ans==2)return; /* accepts a no change error! */
  DelayErr=1;
  
  
}

/* Newton solver for algebraic stuff */
int solve_dae()
{
  int i,j,n;
  int info;
  double err,del,z,yold;
  double tol=EVEC_ERR,eps=NEWT_ERR;
  int maxit=EVEC_ITER,iter=0;
  double *y,*ynew,*f,*fnew,*jac,*errvec;
  n=nsvar;
  if(nsvar==0)return 1;
  if(dae_work.status<0)return dae_work.status; /* accepts no change error */
  y=dae_work.work.data();
  f=y+nsvar;
  fnew=f+nsvar;
  ynew=fnew+nsvar;
  errvec=ynew+nsvar;
  jac=errvec+nsvar;
  for(i=0;i<n;i++){ /* copy current value as initial guess */
    y[i]=svar[i].last;
    ynew[i]=y[i]; /* keep old guess */
  }
  while(1){
    get_dae_fun(y,f);
    err=0.0;
    for(i=0;i<n;i++){
      err+=fabs(f[i]);
      errvec[i]=f[i];
    }
    if(err<tol){ /* success */
      for(i=0;i<n;i++){
	SETVAR(svar[i].index,y[i]);
	svar[i].last=y[i];
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
      get_dae_fun(y,fnew);
      for(j=0;j<n;j++)
	jac[j*n+i]=(fnew[j]-f[j])/del;
      y[i]=yold;
    }
    sgefa(jac,n,n,dae_work.iwork.data(),&info);
    if(info!=-1){
      for(i=0;i<n;i++)
	SETVAR(svar[i].index,ynew[i]);
      return -1; /* singular jacobian */
    }
    sgesl(jac,n,n,dae_work.iwork.data(),errvec,0); /* get x=J^(-1) f */
    err=0.0;
    for(i=0;i<n;i++){
      y[i]-=errvec[i];
      err+=fabs(errvec[i]);
    } 
    if(err>(n*BOUND)){
      for(i=0;i<n;i++)
	SETVAR(svar[i].index,svar[i].last);
      return(-3); /* getting too big */
    }
    if(err<tol) /* not much change */
      {
	/* plintf(" no change .... \n"); */
	for(i=0;i<n;i++){
	  SETVAR(svar[i].index,y[i]);
	  svar[i].last=y[i];
	}
	return 2;
      }
    iter++;
    if(iter>maxit){
      /* plintf(" Too many iterates ... \n"); */
      for(i=0;i<n;i++)
	SETVAR(svar[i].index,svar[i].last);
      return(-2); /* too many iterates */
    }
  }
}


/* interface shit -- different for Win95 */
 
void get_new_guesses()
{
  int i,n;
  /* new_string_of edits a fixed dialog buffer in place (its C API takes
     char *value, not a std::string); svar[i].rhs keeps its old 80-byte
     editable capacity (add_svar's original allocation) via this local
     buffer, then takes the edited text back. name stays on xpp_snprintf
     too: its "%.*s" dynamic precision has no mechanical xpp::format
     equivalent (CLAUDE.md's xpp::format carve-out). */
  char name[XPP_NAME_MAX+40];
  char rhs_buf[80];
  double z;
  if(nsvar<1)return;
  for(i=0;i<nsvar;i++){
    z=svar[i].last;
    xpp_snprintf(name,sizeof(name),"Initial %.*s(%g):",XPP_NAME_MAX,svar[i].name.c_str(),z);
    xpp_strlcpy(rhs_buf,svar[i].rhs.c_str(),sizeof(rhs_buf));
    new_string_of(name,rhs_buf,XPP_FIELD_EXPRESSION);
    svar[i].rhs=rhs_buf;
    if(add_expr(svar[i].rhs.c_str(),svar[i].form.data(),&n)){
      err_msg("Illegal formula");
      return;
    }
    z=evaluate(svar[i].form.data());
    SETVAR(svar[i].index,z);
    svar[i].value=z;
    svar[i].last=z;
  }
}














