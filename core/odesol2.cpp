#include "odesol2.h"
#include "jacobian.h"
#include "session.h"
#include <stdlib.h> 
#include <stdio.h>
#include <math.h>
#include "flags.h"
#include "markov.h"
#include "delay_handle.h"
#include "load_eqn.h"
#include "numerics.h"
#include "model.h"
#include "xpp_math.h"

namespace xpp {

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))


constexpr double coefp[]={ 6.875/3.00,-7.375/3.00,4.625/3.00,-.375},
       coefc[]={ .375,2.375/3.00,-.625/3.00,0.125/3.00 };
namespace {
/* Adams-Bashforth-Moulton's parts of its work: the four last right-hand
   sides (y_p), the four start-up states (y_s) and the prediction */
struct AbmWork {
  double *y_s[4],*y_p[4],*ypred;
  AbmWork(double *work, int neq)
  {
    for(int i=0;i<4;i++){
      y_p[i]=work+(4+i)*neq;
      y_s[i]=work+(8+i)*neq;
    }
    ypred=work+3*neq;
  }
};

int abmpc(xpp::Session &s, double *y, double *t, double dt, int neq, const AbmWork &w);
}

constexpr double symp_b[]={7/24.,.75,-1./24};
constexpr double symp_B[]={2/3.,-2./3.,1.0};

namespace {
/* nt steps of a fixed-step method, storing the delays after each: its
   plain step, or, when the model has flags, the step that also checks
   them (flags.cpp's one_flag_step_*). discrete, euler, mod_euler and
   rung_kut differ only in these two. */
template <class Plain, class Flagged>
int fixed_steps(xpp::Session &s, double *y, int nt, Plain plain, Flagged flagged)
{
  if(s.model().nflags==0){
    for(int i=0;i<nt;i++){
      plain();
      stor_delay(s,y);
    }
    return(0);
  }
  for(int i=0;i<nt;i++){
    flagged();
    stor_delay(s,y);
  }
  return(0);
}
}

/* my first symplectic integrator */

int symplect3(xpp::Session &s, double *y, double *tim, double dt, int nt, int neq, int *istart, double *work)
{
 int i;
 if(s.model().nflags==0){ 
   for(i=0;i<nt;i++)
     {
       one_step_symp(s,y,dt,work,neq,tim);
       
     }
   stor_delay(s,y);
    return(0);
 }
  for(i=0;i<nt;i++)
      {
	one_flag_step_symp(s,y,dt,work,neq,tim,istart);
	stor_delay(s,y);
      }
    return(0);
}

/*   DISCRETE    */

int discrete(xpp::Session &s, double *y, double *tim, double dt, int nt, int neq, int *istart, double *work)
{
  return fixed_steps(s,y,nt,
    [&]{ one_step_discrete(s,y,dt,work,neq,tim); },
    [&]{ one_flag_step_discrete(s,y,dt,work,neq,tim,istart); });
}

/* Backward Euler  */

int bak_euler(xpp::Session &s, double *y, double *tim, double dt, int nt, int neq, int *istart, double *work)
{
 int i,j;
  double *jac,*yg,*yp,*yp2,*errvec;
  yp=work;
  yg=yp+neq;
  errvec=yg+neq;
  yp2=errvec+neq;
  jac=yp2+neq;
  if(s.model().nflags==0){
    for(i=0;i<nt;i++)
      {
	
	if((j=one_bak_step(s,y,tim,dt,neq,yg,yp,yp2,errvec,jac,istart))!=0)
	  return(j);
	stor_delay(s,y);
      }
    return(0);
  }
 for(i=0;i<nt;i++)
      {
	
	if((j=one_flag_step_backeul(s,y,tim,dt,neq,yg,yp,yp2,
				    errvec,jac,istart))!=0)
	  return(j);
	stor_delay(s,y);
      }
    return(0);
}

int one_bak_step(xpp::Session &s, double *y, double *t, double dt, int neq, double *yg, double *yp, double *yp2, double *errvec, double *jac, int *istart)
{
  int i;
  double err=0.0,err1=0.0;
  
  int iter=0,info,ipivot[MAXODE1];
  int ml=s.numerics.cv_bandlower,mr=s.numerics.cv_bandupper,mt=ml+mr+1;
  set_wieners(s,dt,y,*t);
  *t=*t+dt;
  s.integrator.rhs(*t,y,yp2,neq);
  for(i=0;i<neq;i++)yg[i]=y[i];
  while(1)
    {
      err1=0.0;
      err=0.0;
      s.integrator.rhs(*t,yg,yp,neq);
      for(i=0;i<neq;i++){
	errvec[i]=yg[i]-.5*dt*(yp[i]+yp2[i])-y[i];
	err1+=fabs(errvec[i]);
      }
      get_the_jac(s,*t,yg,yp,jac,neq,s.numerics.singpt_jacobian_epsilon,-.5*dt);
      if(s.numerics.cv_bandflag){
	for(i=0;i<neq;i++)
	  jac[i*mt+ml]+=1;
	xpp::bandfac(jac,ml,mr,neq);
        xpp::bandsol(jac,errvec,ml,mr,neq);
      }
      else {
      for(i=0;i<neq;i++)jac[i*neq+i]+=1.0;
      xpp::sgefa(jac,neq,neq,ipivot,&info);
      if(info!=-1)
	{
	 
	  return(-1);
	}
      xpp::sgesl(jac,neq,neq,ipivot,errvec);
      }
      for(i=0;i<neq;i++){
	err+=fabs(errvec[i]);
	yg[i]-=errvec[i];
      }
      if(err<s.numerics.eul_tol||err1<s.numerics.eul_tol){
	for(i=0;i<neq;i++)y[i]=yg[i];
	return(0);
      }
      iter++;
      if(iter>s.numerics.max_eul_iter)return(-2);
    }
}

void one_step_discrete(xpp::Session &s, double *y, double dt, double *yp, int neq, double *t)
{
  int j;
   set_wieners(s,dt,y,*t);
     s.integrator.rhs(*t,y,yp,neq);
     *t=*t+dt;
     for(j=0;j<neq;j++){y[j]=yp[j];
     }

}

void one_step_symp(xpp::Session &s, double *y, double h, double *f, int n, double *t)
{
  int k,j;
  for(k=0;k<3;k++){
    for(j=0;j<n;j+=2)
      y[j]+=(h*symp_b[k]*y[j+1]);
    s.integrator.rhs(*t,y,f,n);
    for(j=0;j<n;j+=2)
      y[j+1]+=(h*symp_B[k]*f[j+1]);
  }
  *t+=h;
}

void one_step_euler(xpp::Session &s, double *y, double dt, double *yp, int neq, double *t)
{
   
 int j;

   set_wieners(s,dt,y,*t);
   s.integrator.rhs(*t,y,yp,neq);
   *t+=dt;
   for(j=0;j<neq;j++)y[j]=y[j]+dt*yp[j];
}

void one_step_rk4(xpp::Session &s, double *y, double dt, double *yval[3], int neq, double *tim)
{
 int i;
 double t=*tim,t1,t2;
 set_wieners(s,dt,y,t);
 s.integrator.rhs(t,y,yval[1],neq);
 for(i=0;i<neq;i++)
   {
     yval[0][i]=y[i]+dt*yval[1][i]/6.00;
     yval[2][i]=y[i]+dt*yval[1][i]*0.5;
  }
  t1=t+.5*dt;
  s.integrator.rhs(t1,yval[2],yval[1],neq);
  for(i=0;i<neq;i++)
    {
      yval[0][i]=yval[0][i]+dt*yval[1][i]/3.00;
      yval[2][i]=y[i]+.5*dt*yval[1][i];
    }
 s.integrator.rhs(t1,yval[2],yval[1],neq);
 for(i=0;i<neq;i++)
   {
     yval[0][i]=yval[0][i]+dt*yval[1][i]/3.000;
     yval[2][i]=y[i]+dt*yval[1][i];
   }
 t2=t+dt;
 s.integrator.rhs(t2,yval[2],yval[1],neq);
 for(i=0;i<neq;i++)y[i]=yval[0][i]+dt*yval[1][i]/6.00;
 *tim=t2;
}

void one_step_heun(xpp::Session &s, double *y, double dt, double *yval[2], int neq, double *tim)
{
 int i;
 double t=*tim,t1;
  set_wieners(s,dt,y,*tim);
  s.integrator.rhs(t,y,yval[0],neq);
  for(i=0;i<neq;i++)yval[0][i]=dt*yval[0][i]+y[i];
  t1=t+dt;
  s.integrator.rhs(t1,yval[0],yval[1],neq);
  for(i=0;i<neq;i++)y[i]=.5*(y[i]+yval[0][i]+dt*yval[1][i]);
  *tim=t1;
}

/*  Euler  */

int euler(xpp::Session &s, double *y, double *tim, double dt, int nt, int neq, int *istart, double *work)
{
  return fixed_steps(s,y,nt,
    [&]{ one_step_euler(s,y,dt,work,neq,tim); },
    [&]{ one_flag_step_euler(s,y,dt,work,neq,tim,istart); });
}

/* Modified Euler  */

int mod_euler(xpp::Session &s, double *y, double *tim, double dt, int nt, int neq, int *istart, double *work)
{
 double *yval[2];

 yval[0]=work;
 yval[1]=work+neq;
 return fixed_steps(s,y,nt,
   [&]{ one_step_heun(s,y,dt,yval,neq,tim); },
   [&]{ one_flag_step_heun(s,y,dt,yval,neq,tim,istart); });
}

/*  Runge Kutta    */

int rung_kut(xpp::Session &s, double *y, double *tim, double dt, int nt, int neq, int *istart, double *work)
{
 double *yval[3];

 yval[0]=work;
 yval[1]=work+neq;
 yval[2]=work+neq+neq;

 return fixed_steps(s,y,nt,
   [&]{ one_step_rk4(s,y,dt,yval,neq,tim); },
   [&]{ one_flag_step_rk4(s,y,dt,yval,neq,tim,istart); });
}

/*   ABM   */

int adams(xpp::Session &s, double *y, double *tim, double dt, int nstep, int neq, int *ist, double *work)
{
  int istart=*ist,i,istpst,k,ik,n;
  int irk;
  double *work1;
  double x0=*tim,xst=*tim;
  work1=work;
 const AbmWork w(work,neq);
 if(istart==1)
 goto n20;
 if(istart>1) goto n350;
 istpst=0;
 goto n400;

n20:

 x0=xst;
 s.integrator.rhs(x0,y,w.y_p[3],neq);
 for(k=1;k<4;k++)
 {
  rung_kut(s,y,&x0,dt,1,neq,&irk,work1);
  stor_delay(s,y);
  for(i=0;i<neq;i++)w.y_s[3-k][i]=y[i];
  s.integrator.rhs(x0,y,w.y_p[3-k],neq);
 }
 istpst=3;
 if(istpst<=nstep) goto n400;
  ik=4-nstep;
  for(i=0;i<neq;i++)y[i]=w.y_s[ik-1][i];
  xst=xst+nstep*dt;
  istart=ik;
  goto n1000;

n350:

  ik=istart-nstep;
  if(ik<=1)goto n370;
  for(i=0;i<neq;i++)y[i]=w.y_s[ik-1][i];
  xst=xst+nstep*dt;
  istart=ik;
  goto n1000;

n370:
  for(i=0;i<neq;i++)y[i]=w.y_s[0][i];
  if(ik==1){x0=xst+dt*nstep; goto n450; }

  istpst=istart-1;

n400:

  if(istpst==nstep) goto n450;
  for(n=istpst+1;n<nstep+1;n++) {
    set_wieners(s,dt,y,x0);
   abmpc(s,y,&x0,dt,neq,w);
   stor_delay(s,y);
 }

n450:
  istart=0;
  xst=x0;

n1000:

 *tim=*tim + nstep*dt;
 *ist=istart;
 return(0);
}

namespace {
int abmpc(xpp::Session &s, double *y, double *t, double dt, int neq, const AbmWork &w)
{
 double x1,x0=*t;
 int i,k;
 for(i=0;i<neq;i++)
 {
  w.ypred[i]=0;
  for(k=0;k<4;k++)w.ypred[i]=w.ypred[i]+coefp[k]*w.y_p[k][i];
  w.ypred[i]=y[i]+dt*w.ypred[i];
 }

 for(i=0;i<neq;i++)
 for(k=3;k>0;k--)w.y_p[k][i]=w.y_p[k-1][i];
 x1=x0+dt;
 s.integrator.rhs(x1,w.ypred,w.y_p[0],neq);

 for(i=0;i<neq;i++)
 {
  w.ypred[i]=0;
  for(k=0;k<4;k++)w.ypred[i]=w.ypred[i]+coefc[k]*w.y_p[k][i];
  y[i]=y[i]+dt*w.ypred[i];
 }
   *t=x1;
 s.integrator.rhs(x1,y,w.y_p[0],neq);
 
 return(1);
 
}
} // namespace

/* this is rosen  - rosenbock step 
    This uses banded routines as well */
int rb23(xpp::Session &s, double *y,double *tstart,double tfinal,
 int *istart,int n,double *work,int *ierr)
{
int out =-1;
 if(s.model().nflags==0)
 {
   out = rosen(s,y,tstart,tfinal,istart,n,work,ierr);
 }
 else
 {
   out = one_flag_step_rosen(s,y,tstart,tfinal,istart,n,work,ierr);
 }
 return(out);
}
 
int rosen(xpp::Session &s, double *y,double *tstart,double tfinal,
int *istart,int n,double *work,int *ierr)
{
 double &htry=s.integrator.rosen_htry; /* the step the last call ended with */
 double epsjac=s.numerics.singpt_jacobian_epsilon;
 double eps=1e-15,hmin,hmax;
 double tdir=1,t0=*tstart,t=t0;
 double atol=s.numerics.abs_tolerance,rtol=s.numerics.tolerance;
 double sqrteps=sqrt(eps);
 double thresh=atol/rtol,absh,h;
 double d=1/(2.+sqrt(2.)),e32=6.+sqrt(2.),tnew;
 /*double ninf;  Is this needed?*/
 int i,n2=n*n,done=0,info,ml=s.numerics.cv_bandlower,mr=s.numerics.cv_bandupper,mt=ml+mr+1;
 int ipivot[MAXODE1],nofailed;
 double temp,err,tdel;
 double *k1,*k2,*k3,*f0,*f1,*f2,*dfdt,*ynew,*dfdy;
 *ierr=1;
 k1=work;
 k2=k1+n;
 k3=k2+n;
 f0=k3+n;
 f1=f0+n;
 f2=f1+n;
 dfdt=f2+n;
 ynew=dfdt+n;
 dfdy=ynew+n;

 if(t0>tfinal)tdir=-1;
 hmax=fabs(tfinal-t);
 if(*istart==1)
   htry=hmax;
 s.integrator.rhs(t0,y,f0,n);
 hmin=16*eps*fabs(t);
 absh = MIN(hmax, MAX(hmin, htry));
 while(!done)
   {
     nofailed=1;
     hmin = 16*eps*fabs(t);
     absh = MIN(hmax, MAX(hmin, absh));
     h = tdir * absh;
     if(1.1*absh >= fabs(tfinal - t)){
       h = tfinal - t;
       absh = fabs(h);
       done = 1;
     }
     get_the_jac(s,t,y,f0,dfdy,n,epsjac,1.0);
     tdel = (t + tdir*MIN(sqrteps*MAX(fabs(t),fabs(t+h)),absh)) - t;
     s.integrator.rhs(t+tdel,y,f1,n);
     for(i=0;i<n;i++)
       dfdt[i]=(f1[i]-f0[i])/tdel;
     while(1){ /* advance a step  */
      for(i=0;i<n2;i++)
	 dfdy[i]=-h*d*dfdy[i];
       for(i=0;i<n;i++)
	 k1[i]=f0[i]+(h*d)*dfdt[i];
       if(s.numerics.cv_bandflag){
	  for(i=0;i<n;i++)
	 dfdy[i*mt+ml]+=1;
        
	 xpp::bandfac(dfdy,ml,mr,n);
	 xpp::bandsol(dfdy,k1,ml,mr,n);
       }
       else{
	  for(i=0;i<n;i++)
	 dfdy[i*n+i]+=1;
	
	 xpp::sgefa(dfdy,n,n,ipivot,&info);
	 xpp::sgesl(dfdy,n,n,ipivot,k1);
       }
       for(i=0;i<n;i++)
	 ynew[i]=y[i]+.5*h*k1[i];
       s.integrator.rhs(t+.5*h,ynew,f1,n);
       for(i=0;i<n;i++)
	 k2[i]=f1[i]-k1[i];
       if(s.numerics.cv_bandflag)
	 xpp::bandsol(dfdy,k2,ml,mr,n);
       else
	 xpp::sgesl(dfdy,n,n,ipivot,k2);
       for(i=0;i<n;i++){
	 k2[i]=k2[i]+k1[i];
	 ynew[i]=y[i]+h*k2[i];
       }
       tnew=t+h;
       s.integrator.rhs(tnew,ynew,f2,n);
       for(i=0;i<n;i++)
	 k3[i]=f2[i] - e32*(k2[i] - f1[i]) - 2*(k1[i] - f0[i]) + (h*d)*dfdt[i];
       if(s.numerics.cv_bandflag)
	 xpp::bandsol(dfdy,k3,ml,mr,n);
       else
	 xpp::sgesl(dfdy,n,n,ipivot,k3);
       /*ninf=0;  This is not used anywhere?
       */
       err=0.0;
       for(i=0;i<n;i++){
	 temp=MAX(MAX(fabs(y[i]),fabs(ynew[i])),thresh);
	 temp=fabs(k1[i]-2*k2[i]+k3[i])/temp;
	 if(err<temp)err=temp;
       }
       err=err*(absh/6);
       if(err>rtol){
	 if(absh<hmin){
           *ierr=-1;
	   return(-1);
	 }
	 absh = MAX(hmin, absh * MAX(0.1, xpp::math::pow(0.8*(rtol/err),1./3.)));
	 h = tdir * absh;
	 nofailed=0;
	 done=0;
       }
       else {
	 break;
       }
     }
     if(nofailed==1){
       temp=1.25*xpp::math::pow(err/rtol,1./3.);
       if(temp>0.2)
	 absh=absh/temp;
       else
	 absh=5*absh;
     }
     t=tnew;
     for(i=0;i<n;i++){
       y[i]=ynew[i];
       f0[i]=f2[i];
     }
   }
 *tstart=t;
 htry=h;
 *istart=0;
 return(0);
}

 /* this assumes that yp is already computed; scal is the solver's factor
    (Backward Euler's -dt/2, rb23's 1) */
void get_the_jac(xpp::Session &s, double t,double *y,double *yp,
	    double *dfdy,int neq,double eps,double scal)
{
  xpp::JacobianForm form;
  if(s.numerics.cv_bandflag)
    form={xpp::JacobianLayout::Banded,s.numerics.cv_bandlower,s.numerics.cv_bandupper};
  const int entries=s.numerics.cv_bandflag?neq*(form.lower+form.upper+1):neq*neq;
  xpp::jacobian(s,t,y,yp,neq,eps,form,dfdy);
  for(int i=0;i<entries;i++)dfdy[i]*=scal;
}

} // namespace xpp
