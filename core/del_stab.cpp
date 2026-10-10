#include "xpp_ui.h"
#include "session.h"
#include "odesol2.h"
#include "xpp_log.h"
#include <stdlib.h>
#include <algorithm>
#include <vector>

#include "gear.h"
#include "jacobian.h"

#include <math.h>
#include <stdio.h>
#include "del_stab.h"
#include "delay_handle.h"
#include "xpp_math.h"

namespace xpp {

#define Z(a,b) z[(a)+n*(b)]
/* this code takes the determinant of a complex valued matrix
*/



/* The
 code here replaces the do_sing code if the equation is
   a delay differential equation. 
*/

xpp::Result<> do_delay_sing(xpp::Session &s, double *x, double eps, double err, double big, int maxit, int n, int *ierr, float *stabinfo)
{
      double rr[2];

 double colnorm=0,colmax,colsum;
 double old_x[MAXODE],sign;
 double yp[MAXODE],y[MAXODE],dx;
 int kmem=n*(2*n+5)+50,i,j,k,okroot;

 std::vector<double> ev(2*n, 0.0);
 /* first we establish how many delays there are */
 s.delay.stab_flag=0;
 for(i=0;i<n;i++)old_x[i]=x[i];
 std::vector<double> work(kmem);
 rooter(s,x,err,eps,big,work.data(),ierr,maxit,n);
 if(*ierr!=0)
   {
     s.delay.stab_flag=1;
     for(i=0;i<n;i++)x[i]=old_x[i];
     return xpp::fail("equilibrium","Could not converge to root",command_place());
   }
 /* OKAY -- we have the root */
 s.delay.ndelay=0;
 s.integrator.rhs(0.0,x,y,n); /* one more evaluation to get delays */
 for(i=0;i<n;i++){
   s.delay.variable_shift[0][i]=x[i];  /* unshifted  */
   s.delay.variable_shift[1][i]=x[i];
 }
 std::vector<double> coef(static_cast<size_t>(n)*n*(s.delay.ndelay+1));

 /* now we must compute a bunch of jacobians  */
 /* first the normal one   */
 s.delay.stab_flag=-1;
 s.delay.which=-1;
 colmax=0.0;
 xpp::jacobian(s,0.0,x,y,n,eps,{xpp::JacobianLayout::RowMajor},coef.data());
 for(i=0;i<n;i++)
   {
     colsum=0.0;
     for(j=0;j<n;j++)colsum+=fabs(coef[j*n+i]);
     if(colsum>colmax)colmax=colsum;
   }
 colnorm=colmax;
 /* now the jacobians for the delays */
 for(k=0;k<s.delay.ndelay;k++){
   s.delay.which=k;
   colmax=0.0;
   for(i=0;i<n;i++){
     colsum=0.0;
     for(j=0;j<n;j++)
       s.delay.variable_shift[1][j]=s.delay.variable_shift[0][j];
     dx=eps*std::max(eps,fabs(x[i]));
     s.delay.variable_shift[1][i]=x[i]+dx;
     s.integrator.rhs(0.0,x,yp,n);
     s.delay.variable_shift[1][i]=x[i];
     for(j=0;j<n;j++){
       coef[j*n+i+n*n*(k+1)]=(yp[j]-y[j])/dx;
       colsum+=fabs(coef[j*n+i+n*n*(k+1)]);
     }
     if(colsum>colmax)colmax=colsum;
   }
   colnorm+=colmax;
 }
 sign=plot_args(coef.data(),s.delay.list.data(),n,s.delay.ndelay,s.delay.grid,colnorm,colnorm);

 okroot=find_positive_root(s,coef.data(),s.delay.list.data(),n,s.delay.ndelay,colnorm,err,eps,big,maxit,rr);
 if(okroot>0){
   ev[0]=rr[0];
   ev[1]=rr[1];
 }
 *stabinfo=static_cast<float>(fabs(sign));
 i=static_cast<int>(sign);
if(i==0&&okroot==1&&s.delay.alpha_max>0)
  i=2;

 /* no eigenvalue list: a delay equation has infinitely many; the
    counts say which way the dominant root lies */
 create_eq_box(s,abs(i),2,0,0,0,x,NULL,n);
 /* DING; */
 s.delay.stab_flag=1;
 if(okroot==1)*stabinfo=s.delay.alpha_max;
 return {};
}

COMPLEX cdif(COMPLEX z, COMPLEX w)
{
   COMPLEX sum;
  sum.r=z.r-w.r;
  sum.i=z.i-w.i;
  return sum;
 }

COMPLEX cmlt(COMPLEX z, COMPLEX w)
{
   COMPLEX sum;
  sum.r=z.r*w.r-z.i*w.i;
  sum.i=z.r*w.i+z.i*w.r;
  return sum;
}

COMPLEX cdivv(COMPLEX z, COMPLEX w)
{
  COMPLEX sum;
  double amp=w.r*w.r+w.i*w.i;
  sum.r=(z.r*w.r+z.i*w.i)/amp;
  sum.i=(z.i*w.r-z.r*w.i)/amp;
  return sum;
}

COMPLEX cexp2(COMPLEX z)
{
  COMPLEX sum;
  double ex=xpp::math::exp(z.r);
  sum.r=ex*xpp::math::cos(z.i);
  sum.i=ex*xpp::math::sin(z.i);
  return sum;
}

void switch_rows(COMPLEX *z, int i1, int i2, int n)
{
  COMPLEX zt;
  int j;
  for(j=0;j<n;j++){
    zt=Z(i1,j);
    Z(i1,j)=Z(i2,j);
    Z(i2,j)=zt;
  }
}

COMPLEX rtoc(double x, double y)
{
  COMPLEX sum;
  sum.i=y;
  sum.r=x;
  return sum;
}

double c_abs(COMPLEX z)
{
 return(sqrt(z.i*z.i+z.r*z.r));
}

COMPLEX cdeterm(COMPLEX *z, int n)
{
  int i,j,imax=0,k;
  double q,qmax;
  COMPLEX sign=rtoc(1.0,0.0),mult,sum,zd;
  for(j=0;j<n;j++){
    qmax=0.0;
    for(i=j;i<n;i++){
      q=c_abs(Z(i,j));
      if(q>qmax){
	qmax=q;
	imax=i;
      }
    }
    if(qmax==0.0)return(rtoc(0.0,0.0));
    switch_rows(z,imax,j,n);
    if(imax>j)sign=cmlt(rtoc(-1.0,0.0),sign);
    zd=Z(j,j);
    for(i=j+1;i<n;i++){
      mult=cdivv(Z(i,j),zd);
      for(k=j+1;k<n;k++){
	Z(i,k)=cdif(Z(i,k),cmlt(mult,Z(j,k)));
      }
    }
  }
  sum=sign;
  for(j=0;j<n;j++)
    sum=cmlt(sum,Z(j,j));
  return sum;
}
	 
void make_z(COMPLEX *z, double *delay, int n, int m, double *coef, COMPLEX lambda)
{
  int i,j,k,km;
  COMPLEX temp,eld;
  
 for(j=0;j<n;j++)
    for(i=0;i<n;i++){
      if(i==j)temp=lambda;
      else temp=rtoc(0.0,0.0);
      z[i+j*n]=cdif(temp,rtoc(coef[i+j*n],0.0)); /* initialize the array */
    }
  for(k=0;k<m;k++){
    km=(k+1)*n*n;
    temp=rtoc(-delay[k],0.0); /* convert delay to complex number */
    eld=cexp2(cmlt(temp,lambda)); /* compute exp(-lambda*tau) */
    for(j=0;j<n;j++)
      for(i=0;i<n;i++)
	z[i+j*n]=cdif(z[i+j*n],cmlt(eld,rtoc(coef[km+i+n*j],0.0)));
  }
}

int find_positive_root(xpp::Session &s, double *coef, double *delay, int n, int m, double rad, double err, double eps, double big, int maxit, double *rr)
{
  COMPLEX lambda,lambdap;
  COMPLEX det,detp;
  double jac[4];
  double xl,yl,r,xlp,ylp;

  int k;

    lambda.r=s.delay.alpha_max;
    lambda.i=s.delay.omega_max;

   std::vector<COMPLEX> z(static_cast<size_t>(n)*n);

  /* now Newtons Method for maxit times */
  for(k=0;k<maxit;k++){

    make_z(z.data(),delay,n,m,coef,lambda);
    det=cdeterm(z.data(),n);

    r=c_abs(det);
    if(r<err){ /* within the tolerance */
      process_root(lambda.r,lambda.i);
      s.delay.alpha_max=lambda.r;
      s.delay.omega_max=lambda.i;
      return 1;
    }
    xl=lambda.r;
    yl=lambda.i;
   
    /* compute the Jacobian */
    if(fabs(xl)>eps)
      r=eps*fabs(xl);
    else
      r=eps*eps;
    xlp=xl+r;
    lambdap=rtoc(xlp,yl);
    make_z(z.data(),delay,n,m,coef,lambdap);
    detp=cdeterm(z.data(),n);
   jac[0]=(detp.r-det.r)/r;
   jac[2]=(detp.i-det.i)/r;
    if(fabs(yl)>eps)
      r=eps*fabs(yl);
    else
      r=eps*eps;
    ylp=yl+r;
    lambdap=rtoc(xl,ylp);
    make_z(z.data(),delay,n,m,coef,lambdap);
    detp=cdeterm(z.data(),n);
    jac[1]=(detp.r-det.r)/r;
    jac[3]=(detp.i-det.i)/r;
    r=jac[0]*jac[3]-jac[1]*jac[2];
    if(r==0){
      xpp::log(XPP_LOG_WARN, " singular jacobian \n");
      return -1;
    }
   xlp=(jac[3]*det.r-jac[1]*det.i)/r;
   ylp=(-jac[2]*det.r+jac[0]*det.i)/r;
    xl=xl-xlp;
    yl=yl-ylp;
    r=fabs(xlp)+fabs(ylp);
    lambda.r=xl;
    lambda.i=yl;
    if(r<err)
    { /* within the tolerance */
      process_root(lambda.r,lambda.i);
      s.delay.alpha_max=lambda.r;
      s.delay.omega_max=lambda.i;
      rr[0]=s.delay.alpha_max;
      rr[1]=s.delay.omega_max;
      return 1;
    }
    if(r>big){
      xpp::log(XPP_LOG_WARN, "Failed to converge \n");
      return -1;
    }
  }

  xpp::log(XPP_LOG_WARN, "Max iterates exceeded \n");
  return -1;
}
void process_root(double real, double im)
{
  xpp::log(XPP_LOG_INFO, "Root: {:g} + I {:g} \n",real,im);
}
double get_arg(double *delay, double *coef, int m, int n, COMPLEX lambda)
{
  int i,j,k,km;
  COMPLEX temp,eld;
  double arg;
  if(m==0)return(0);  /* no delays so don't use this! */
  std::vector<COMPLEX> z(static_cast<size_t>(n)*n);
  for(j=0;j<n;j++)
    for(i=0;i<n;i++){
      if(i==j)temp=lambda;
      else temp=rtoc(0.0,0.0);
      z[i+j*n]=cdif(temp,rtoc(coef[i+j*n],0.0)); /* initialize the array */
    }
  for(k=0;k<m;k++){
    km=(k+1)*n*n;
    temp=rtoc(-delay[k],0.0); /* convert delay to complex number */
    eld=cexp2(cmlt(temp,lambda)); /* compute exp(-lambda*tau) */
    for(j=0;j<n;j++)
      for(i=0;i<n;i++)
	z[i+j*n]=cdif(z[i+j*n],cmlt(eld,rtoc(coef[km+i+n*j],0.0)));
  }
  /*  the array is done  */
  temp=cdeterm(z.data(),n);
  arg=xpp::math::atan2(temp.i,temp.r);
  return(arg);
}   

int test_sign(double old, double newval)
{
  if(old>0.0&&newval<0.0){
    if(old>2.9&&newval<-2.9)return 1;
    return(0); /* doesnt pass threshold */
  }
  if(old<0.0&&newval>0.0){
    if(old<-2.9&&newval>2.9)return -1;
    return 0;
  }
  return 0;
}

/* code for establishing delay stability
   sign=plot_args(coef,delay,n,m,npts,amax,wmax)
    coef is a real array of length  (m+1)*n^2
    each n^2 block is the jacobian with respect to the mth delay
    m total delays
    n is size of system
    npts is number of pts on each part of contour
    contour is
      i wmax -----<---------    amax+i wmax
       |                            |
       V                            ^
       |                            |
     -i wmax ----->-----------  amax-i wmax
  
     sign is the number of roots in the contour using the argument
     principle
*/  

int plot_args(double *coef, double *delay, int n, int m, int npts, double almax, double wmax)
{
  int i;
  int sign=0;
  COMPLEX lambda;
  double x,y,arg,oldarg=0.0;
  double ds;  /* steplength */
  /* first the contour from i wmax -- -i wmax */
  ds=2*wmax/npts;
   x=0.0;
  for(i=0;i<npts;i++){
    y=wmax-i*ds;
    lambda=rtoc(x,y);
    arg=get_arg(delay,coef,m,n,lambda);
    sign=sign+test_sign(oldarg,arg);
    oldarg=arg;
 
  }
 /* lower contour   */
  y=-wmax;
  ds=almax/npts;
  for(i=0;i<npts;i++){
    x=i*ds;
    lambda=rtoc(x,y);
    arg=get_arg(delay,coef,m,n,lambda);
       sign=sign+test_sign(oldarg,arg);
    oldarg=arg;
 
  }
/* right contour */
 x=almax;
 ds=2*wmax/npts;
  for(i=0;i<npts;i++){
    y=-wmax+i*ds;
    lambda=rtoc(x,y);
    arg=get_arg(delay,coef,m,n,lambda);
      sign=sign+test_sign(oldarg,arg);
    oldarg=arg;
 
  }
 
/* top contour */
  y=wmax;
  ds=almax/npts;
  for(i=0;i<npts;i++){
    x=almax-i*ds;
    lambda=rtoc(x,y);
    arg=get_arg(delay,coef,m,n,lambda);
    sign=sign+test_sign(oldarg,arg);
    oldarg=arg;
  
  }
  return sign;
}

} // namespace xpp
