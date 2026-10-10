#include "xpp_batch.h"
#include "session.h"
#include "load_eqn.h"
#include "form_ode.h"
#include "volterra2.h"
#include "xpp_log.h"
#include "delay_handle.h"
#include "markov.h"

#include <stdlib.h> 
#include "getvar.h"
#include <math.h>
#include <stdio.h>
#include "expr.h"
#include <algorithm>
#include <vector>
#include "model.h"
#include "xpp_math.h"

namespace xpp {
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

/*  This is an implicit solver for volterra integral and integro-differential
    equations.  It is based on code found in Peter Linz's book
    ont Volterra equations.
    One tries to evaluate:
    
       int_0^t ( (t-t')^-mu K(t,t',u) dt')
    where  0 <= mu < 1 and K(t,t',u) is cts and Lipschitz.
    The product method is used combined with the trapezoidal rule for integration.  
    The method is A-stable since it is an implicit scheme.  
    
    The kernel structure contains the constant mu and the expression for
    evaluating K(t,t',u)

*/

#define CONV 2


double ker_val(xpp::Session &s, int in)
{
 if(s.volterra.kn_flag)return(s.volterra.kernels[in].k_n);
 return(s.volterra.kernels[in].k_n1);
}

void alloc_v_memory(xpp::Session &s)  /* allocate stuff for volterra equations */
{
  std::array<KERNEL,MAXKER> &kernels=s.model().kernels;
  int i,len;
  /* add_expr's program, as long as any other formula's (MAXEXPLEN) */
  std::vector<int> formula(MAXEXPLEN);
  /* the program's len commands and two zeros after them */
  auto program=[&formula](int len){
    std::vector<int> p(len+2,0);
    std::copy(formula.begin(),formula.begin()+len,p.begin());
    return p;
  };

/* First parse the kernels   since these were deferred */
  for(i=0;i<s.model().nkernel;i++){
     s.volterra.kernels[i].k_n=0.0;
     if(add_expr(s,kernels[i].expr,formula.data(),&len)){
       model_failed(xpp::Error{"volterra",xpp::format("Illegal kernel {}={}",kernels[i].name,kernels[i].expr),
                               xpp::model_place(s.model(),kernels[i].name)});
     }
     kernels[i].formula.rpn=program(len);
     if(kernels[i].flag==CONV){
       if(add_expr(s,kernels[i].kerexpr,formula.data(),&len)){
	 model_failed(xpp::Error{"volterra",xpp::format("Illegal convolution {}={}",kernels[i].name,kernels[i].kerexpr),
	                         xpp::model_place(s.model(),kernels[i].name)});
       }
       kernels[i].kerform.rpn=program(len);
     }
   }
  allocate_volterra(s,s.numerics.max_points,0);
}

void allocate_volterra(xpp::Session &s, int npts, int flag)
{
  int i;
  int ntot=s.model().node+s.model().fix_var+s.model().nmarkov;
  npts=abs(npts);
  s.numerics.max_points=npts;
  /* now allocate the memory   */
  if(s.model().nkernel==0)return;
  /* flag==1 (a new grid) used to free the old blocks first; assigning the
     vectors again replaces them either way, so flag no longer matters */
  if(static_cast<int>(s.volterra.memory.size())<ntot)s.volterra.memory.resize(ntot);
  for(i=0;i<ntot;i++)
    s.volterra.memory[i].assign(s.numerics.max_points,0.0);

  s.volterra.current_point=0;
  s.volterra.kn_flag=1;
  alloc_kernels(s,flag);
}

void re_evaluate_kernels(xpp::Session &s)
{
  std::array<KERNEL,MAXKER> &kernels=s.model().kernels;
  int i,j,n=s.numerics.max_points;

  if(s.numerics.auto_evaluate==0)return;
  for(i=0;i<s.model().nkernel;i++){
    if(kernels[i].flag==CONV){
      for(j=0;j<=n;j++){
	setvar(s,0,s.numerics.start_time+s.numerics.delta_t*j);
	s.volterra.kernels[i].cnv[j]=evaluate(s,kernels[i].kerform);
      }
    }  
  }
}

void alloc_kernels(xpp::Session &s, int flag)
{
  std::array<KERNEL,MAXKER> &kernels=s.model().kernels;
  int i,n=s.numerics.max_points;
  int j;
  double mu;
  for(i=0;i<s.model().nkernel;i++){
    if(kernels[i].flag==CONV){
      s.volterra.kernels[i].cnv.assign(n+1,0.0);
      for(j=0;j<=n;j++){
	setvar(s,0,s.numerics.start_time+s.numerics.delta_t*j);
	s.volterra.kernels[i].cnv[j]=evaluate(s,kernels[i].kerform);
      }
    }
    /* Do the alpha functions here later  */
   if(kernels[i].mu>0.0){
     mu=kernels[i].mu;
     s.volterra.kernels[i].al.assign(n+1,0.0);
     for(j=0;j<=n;j++)s.volterra.kernels[i].al[j]=alpbetjn(mu,s.numerics.delta_t,j);
   }
  }
}

/* the following is the main driver for evaluating the sums in the 
   kernel the results here are used in the implicit solver.  The integral
   up to t_n-1 is evaluated and placed in sum.  Kn-> Kn-1   
   
   the weights al and bet are computed in general, but specifically
   for mu=0,.5 since these involve no transcendental functions

   */

/***   FIX THIS TO DO MORE GENERAL STUFF   
       K(t,t',u,u') someday...
***/

void init_sums(xpp::Session &s, double t0, int n, double dt, int i0, int iend, int ishift)
{
  std::array<KERNEL,MAXKER> &kernels=s.model().kernels;
   double t=t0+n*dt,tp=t0+i0*dt;
   double sum[MAXODE],al,alpbet,mu;
   int nvar=s.model().fix_var+s.model().node+s.model().nmarkov;
   int l,ioff,ker,i;
   setvar(s,0,t);
   setvar(s,s.model().prime_start,tp);
   for(l=0;l<nvar;l++)setvar(s,l+1,s.volterra.memory[l][ishift]);
   for(ker=0;ker<s.model().nkernel;ker++){
     s.volterra.kernels[ker].k_n1=s.volterra.kernels[ker].k_n;
     mu=kernels[ker].mu;
     if(mu==0.0)al=.5*dt;
     else al=alpha1n(mu,dt,t,tp);
     sum[ker]=al*evaluate(s,kernels[ker].formula);
     if(kernels[ker].flag==CONV)
       sum[ker]=sum[ker]*s.volterra.kernels[ker].cnv[n-i0];
     
   }
   for(i=1;i<=iend;i++){
     ioff=(ishift+i)%s.numerics.max_points;
     tp+=dt;
     setvar(s,s.model().prime_start,tp);
     for(l=0;l<nvar;l++)setvar(s,l+1,s.volterra.memory[l][ioff]);
     for(ker=0;ker<s.model().nkernel;ker++){
       mu=kernels[ker].mu;
       if(mu==0.0)alpbet=dt;
       else alpbet=s.volterra.kernels[ker].al[n-i0-i];
       if(kernels[ker].flag==CONV)
	 sum[ker]+=(alpbet*evaluate(s,kernels[ker].formula)
		    *s.volterra.kernels[ker].cnv[n-i0-i]);
       else sum[ker]+=(alpbet*evaluate(s,kernels[ker].formula));
     }
   }
   for(ker=0;ker<s.model().nkernel;ker++){
     s.volterra.kernels[ker].sum=sum[ker];
     
   }
}

/* the following functions compute integrals for the piecewise 
   -- constant -- product integration rule.  Thus they agree with
   the trapezoid rule for mu=0 and there is a special case for mu=.5
   since that involves no transcendentals.  Later I will put in the
   piecewise --linear-- method
*/

double alpha1n(double mu, double dt, double t, double t0)
{
  double m1;
  if(mu==.5)return(sqrt(fabs(t-t0))-sqrt(fabs(t-t0-dt)));
  m1=1-mu;
  return(.5*(xpp::math::pow(fabs(t-t0),m1)-xpp::math::pow(fabs(t-t0-dt),m1))/m1);
}

double alpbetjn(double mu, double dt, int l)
{
  double m1;
  double dif=l*dt;
  if(mu==.5)return(sqrt(dif+dt)-sqrt(fabs(dif-dt)));
  m1=1-mu;
  return(.5*(xpp::math::pow(dif+dt,m1)-xpp::math::pow(fabs(dif-dt),m1))/m1);
}
double betnn(double mu, double dt, double t0, double t)
{
 double m1;
 if(mu==.5)return(sqrt(dt));
 m1=1-mu;
 return(.5*xpp::math::pow(dt,m1)/m1);
}

void get_kn(xpp::Session &s, double *y, double t)  /* uses the guessed value y to update Kn  */
{
  std::array<KERNEL,MAXKER> &kernels=s.model().kernels;
  int i;

  setvar(s,0,t);
  setvar(s,s.model().prime_start,t);
  for(i=0;i<s.model().node;i++)
    setvar(s,i+1,y[i]);
  for(i=s.model().node;i<s.model().node+s.model().fix_var;i++)
    setvar(s,i+1,xpp::evaluate(s,s.model().programs[i]));
  for(i=0;i<s.model().nkernel;i++){
    if(kernels[i].flag==CONV)
      s.volterra.kernels[i].k_n=s.volterra.kernels[i].sum+
	s.volterra.kernels[i].betnn*evaluate(s,kernels[i].formula)*s.volterra.kernels[i].cnv[0];
    else 
      s.volterra.kernels[i].k_n=s.volterra.kernels[i].sum+s.volterra.kernels[i].betnn*evaluate(s,kernels[i].formula);
  }
}
     
int volterra(xpp::Session &s, double *y, double *t, double dt, int nt, int neq, int *istart, double *work)
{
  std::array<KERNEL,MAXKER> &kernels=s.model().kernels;
  double *jac,*yg,*yp,*yp2,*ytemp,*errvec;
  double z,mu,bet;
  int i,j;
  yp=work;
  yg=yp+neq;
  ytemp=yg+neq;
  errvec=ytemp+neq;
  yp2=errvec+neq;
  jac=yp2+neq;

                                         /*  Initialization of everything   */  
  if(*istart==1){
    s.volterra.current_point=0;
    s.volterra.kn_flag=1;
    for(i=0;i<s.model().nkernel;i++){              /* zero the integrals              */
      s.volterra.kernels[i].k_n=0.0;
      s.volterra.kernels[i].k_n1=0.0;
      mu=kernels[i].mu;                 /*  compute bet_nn                 */
      if(mu==0.0)bet=.5*dt;
      else bet=betnn(mu,dt,*t,*t);
      s.volterra.kernels[i].betnn=bet;
    }
    setvar(s,0,*t);
    setvar(s,s.model().prime_start,*t);
    for(i=0;i<s.model().node;i++)
      if(!s.model().eq_type[i])setvar(s,i+1,y[i]);  /* assign initial data             */
    for(i=s.model().node;i<s.model().node+s.model().fix_var;i++)
      setvar(s,i+1,xpp::evaluate(s,s.model().programs[i])); /* set fixed variables  for pass 1 */
    for(i=0;i<s.model().node;i++)
      if(s.model().eq_type[i]){  
	z=xpp::evaluate(s,s.model().programs[i]);           /* reset IC for integral eqns      */
	setvar(s,i+1,z);
	y[i]=z;    
      }
    for(i=s.model().node;i<s.model().node+s.model().fix_var;i++)       /* pass 2 for fixed variables      */   
      setvar(s,i+1,xpp::evaluate(s,s.model().programs[i]));
    for(i=0;i<s.model().node+s.model().fix_var+s.model().nmarkov;i++)
      s.volterra.memory[i][0]=getvar(s,i+1);        /* save everything                 */
    s.volterra.current_point=1;
    *istart=0;
  }

  for(i=0;i<nt;i++)                      /* the real computation            */
    {
      *t=*t+dt;
      set_wieners(s,dt,y,*t);
      if((j=volt_step(s,y,*t,dt,neq,yg,yp,yp2,ytemp,errvec,jac))!=0)
	return(j);
      stor_delay(s,y); 
    }
 return(0);
}

int volt_step(xpp::Session &s, double *y, double t, double dt, int neq, double *yg, double *yp, double *yp2, double *ytemp, double *errvec, double *jac)
{
 int i0,iend,ishift,i,iter=0,info,ipivot[MAXODE1],j,ind;
 int n1=s.model().node+1;
 double dt2=.5*dt,err;
 double del,yold,fac,delinv;
 i0=MAX(0,s.volterra.current_point-s.numerics.max_points);
 iend=MIN(s.volterra.current_point-1,s.numerics.max_points-1);
 ishift=i0%s.numerics.max_points;
 init_sums(s,s.numerics.start_time,s.volterra.current_point,dt,i0,iend,ishift); /*  initialize all the sums */
 s.volterra.kn_flag=0;
 for(i=0;i<neq;i++){
   setvar(s,i+1,y[i]);
   yg[i]=y[i];
 }
 for(i=s.model().node;i<s.model().node+s.model().nmarkov;i++)
   setvar(s,i+1+s.model().fix_var,y[i]);
 setvar(s,0,t-dt);
 for(i=s.model().node;i<s.model().node+s.model().fix_var;i++)
   setvar(s,i+1,xpp::evaluate(s,s.model().programs[i]));
 for(i=0;i<s.model().node;i++){
   if(!s.model().eq_type[i])yp2[i]=y[i]+dt2*xpp::evaluate(s,s.model().programs[i]);
   else yp2[i]=0.0;
 }
 s.volterra.kn_flag=1;
 while(1){
   get_kn(s,yg,t);
    for(i=s.model().node;i<s.model().node+s.model().fix_var;i++)
     setvar(s,i+1,xpp::evaluate(s,s.model().programs[i]));
   for(i=0;i<s.model().node;i++){
     yp[i]=xpp::evaluate(s,s.model().programs[i]);
     if(s.model().eq_type[i])errvec[i]=-yg[i]+yp[i];
     else errvec[i]=-yg[i]+dt2*yp[i]+yp2[i];
   }
   /*   Compute Jacobian     */
   for(i=0;i<s.model().node;i++){
     del=s.numerics.singpt_jacobian_epsilon*MAX(s.numerics.singpt_jacobian_epsilon,fabs(yg[i]));
     yold=yg[i];
     yg[i]+=del;
     delinv=1./del;
     get_kn(s,yg,t);
      for(j=s.model().node;j<s.model().node+s.model().fix_var;j++)
       setvar(s,j+1,xpp::evaluate(s,s.model().programs[j]));
     for(j=0;j<s.model().node;j++){
       fac=delinv;
       if(!s.model().eq_type[j])fac*=dt2;
       jac[j*s.model().node+i]=(xpp::evaluate(s,s.model().programs[j])-yp[j])*fac;
     }
     yg[i]=yold;
   }
   
   for(i=0;i<s.model().node;i++)
     jac[n1*i]-=1.0;
   xpp::sgefa(jac,s.model().node,s.model().node,ipivot,&info);
   if(info!=-1)
     {
	 
       return(-1); /* Jacobian is singular   */
     }
   err=0.0;
   xpp::sgesl(jac,s.model().node,s.model().node,ipivot,errvec);
   for(i=0;i<s.model().node;i++){
	err=MAX(fabs(errvec[i]),err);
	yg[i]-=errvec[i];
      }
   if(err<s.numerics.eul_tol) break;
   iter++;
   if(iter>s.numerics.max_eul_iter)return(-2);  /* too many iterates   */
   
 }
 /* We have a good point; lets save it    */
 get_kn(s,yg,t);
 for(i=0;i<s.model().node;i++)y[i]=yg[i];
 ind=s.volterra.current_point%s.numerics.max_points;
 for(i=0;i<s.model().node+s.model().fix_var+s.model().nmarkov;i++)
   s.volterra.memory[i][ind]=getvar(s,i+1);
 s.volterra.current_point++;

 return(0);
 
}

} // namespace xpp
