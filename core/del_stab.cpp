#include "xpp_ui.h"
#include "session.h"
#include "odesol2.h"
#include "xpp_log.h"
#include <stdlib.h>
#include <algorithm>
#include <vector>

#include "equilibria.h"
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

xpp::Result<> do_delay_sing(xpp::Session &s, double *x, double eps, double err, double big, int maxit, int n, int *ierr, double *stabinfo)
{
      double rr[2]={0,0};

 double colnorm=0,colmax,colsum;
 double sign;
 std::vector<double> old_x(n),y(n);
 for(auto &shift : s.delay.variable_shift)shift.assign(s.model().neq,0.0);
 int kmem=n*(2*n+5)+50,i,j,k;

 std::vector<double> ev(2*n, 0.0);
 /* first we establish how many delays there are */
 s.delay.stab_flag=0;
 for(i=0;i<n;i++)old_x[i]=x[i];
 std::vector<double> work(kmem);
 NewtonRun run;
 if(auto r=newton_iterate(s,x,err,eps,big,work.data(),run,maxit,n);!r)return r;
 *ierr=run.end==NewtonEnd::converged?0:1;
 if(*ierr!=0)
   {
     s.delay.stab_flag=1;
     for(i=0;i<n;i++)x[i]=old_x[i];
     return std::unexpected(newton_error(s,run,old_x));
   }
 /* OKAY -- we have the root */
 s.delay.ndelay=0;
 s.integrator.rhs(0.0,x,y.data(),n); /* one more evaluation to get delays */
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
 xpp::jacobian(s,0.0,x,y.data(),n,eps,{xpp::JacobianLayout::RowMajor},coef.data());
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
   auto perturb_delay=[&](int i,double xi,double *f){
     for(int j=0;j<n;j++)
       s.delay.variable_shift[1][j]=s.delay.variable_shift[0][j];
     s.delay.variable_shift[1][i]=xi;
     s.integrator.rhs(0.0,x,f,n);
     s.delay.variable_shift[1][i]=x[i];
   };
   xpp::jacobian(perturb_delay,x,y.data(),n,eps,{xpp::JacobianLayout::RowMajor},coef.data()+n*n*(k+1));
   colmax=0.0;
   for(i=0;i<n;i++){
     colsum=0.0;
     for(j=0;j<n;j++)colsum+=fabs(coef[j*n+i+n*n*(k+1)]);
     if(colsum>colmax)colmax=colsum;
   }
   colnorm+=colmax;
 }
 sign=plot_args(coef.data(),s.delay.list.data(),n,s.delay.ndelay,s.delay.grid,colnorm,colnorm);

 const double guess_re=s.delay.alpha_max,guess_im=s.delay.omega_max;
 const NewtonRun root=find_positive_root(s,coef.data(),s.delay.list.data(),n,s.delay.ndelay,colnorm,err,eps,big,maxit,rr);
 const bool okroot=root.end==NewtonEnd::converged;
 if(okroot){
   ev[0]=rr[0];
   ev[1]=rr[1];
 }
 else /* the contour's count of roots is still shown */
   xpp::log(XPP_LOG_WARN,"No characteristic root near {} + i {}: the search {}\n",xpp::number(guess_re),xpp::number(guess_im),
            newton_cause(s,root,1).why);
 *stabinfo=fabs(sign);
 i=static_cast<int>(sign);
if(i==0&&okroot&&s.delay.alpha_max>0)
  i=2;

 /* no eigenvalue list: a delay equation has infinitely many; the
    counts say which way the dominant root lies */
 create_eq_box(s,abs(i),2,0,0,0,x,NULL,n);
 /* DING; */
 s.delay.stab_flag=1;
 if(okroot)*stabinfo=s.delay.alpha_max;
 return {};
}

/* cdif: complex difference (XPPAUT del_stab.c) */
COMPLEX complex_subtract(COMPLEX z, COMPLEX w)
{
   COMPLEX sum;
  sum.r=z.r-w.r;
  sum.i=z.i-w.i;
  return sum;
 }

/* cmlt: complex product (XPPAUT del_stab.c) */
COMPLEX complex_multiply(COMPLEX z, COMPLEX w)
{
   COMPLEX sum;
  sum.r=z.r*w.r-z.i*w.i;
  sum.i=z.r*w.i+z.i*w.r;
  return sum;
}

/* cdivv: complex quotient (XPPAUT del_stab.c) */
COMPLEX complex_divide(COMPLEX z, COMPLEX w)
{
  COMPLEX sum;
  double amp=w.r*w.r+w.i*w.i;
  sum.r=(z.r*w.r+z.i*w.i)/amp;
  sum.i=(z.i*w.r-z.r*w.i)/amp;
  return sum;
}

/* cexp2: complex exponential (XPPAUT del_stab.c) */
COMPLEX complex_exponential(COMPLEX z)
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

/* rtoc: builds a complex from its parts (XPPAUT del_stab.c) */
COMPLEX make_complex(double x, double y)
{
  COMPLEX sum;
  sum.i=y;
  sum.r=x;
  return sum;
}

/* c_abs: sqrt(re^2+im^2) of a del_stab complex (not the overflow-safe cabs: different bits) */
double complex_modulus(COMPLEX z)
{
 return(sqrt(z.i*z.i+z.r*z.r));
}

/* cdeterm: determinant of a complex matrix (XPPAUT del_stab.c) */
COMPLEX complex_determinant(COMPLEX *z, int n)
{
  int i,j,imax=0,k;
  double q,qmax;
  COMPLEX sign=make_complex(1.0,0.0),mult,sum,zd;
  for(j=0;j<n;j++){
    qmax=0.0;
    for(i=j;i<n;i++){
      q=complex_modulus(Z(i,j));
      if(q>qmax){
	qmax=q;
	imax=i;
      }
    }
    if(qmax==0.0)return(make_complex(0.0,0.0));
    switch_rows(z,imax,j,n);
    if(imax>j)sign=complex_multiply(make_complex(-1.0,0.0),sign);
    zd=Z(j,j);
    for(i=j+1;i<n;i++){
      mult=complex_divide(Z(i,j),zd);
      for(k=j+1;k<n;k++){
	Z(i,k)=complex_subtract(Z(i,k),complex_multiply(mult,Z(j,k)));
      }
    }
  }
  sum=sign;
  for(j=0;j<n;j++)
    sum=complex_multiply(sum,Z(j,j));
  return sum;
}
	 
/* make_z: fills the delay system's characteristic matrix at lambda (del_stab.cpp) */
void fill_characteristic_matrix(COMPLEX *z, double *delay, int n, int m, double *coef, COMPLEX lambda)
{
  int i,j,k,km;
  COMPLEX temp,eld;
  
 for(j=0;j<n;j++)
    for(i=0;i<n;i++){
      if(i==j)temp=lambda;
      else temp=make_complex(0.0,0.0);
      z[i+j*n]=complex_subtract(temp,make_complex(coef[i+j*n],0.0)); /* initialize the array */
    }
  for(k=0;k<m;k++){
    km=(k+1)*n*n;
    temp=make_complex(-delay[k],0.0); /* convert delay to complex number */
    eld=complex_exponential(complex_multiply(temp,lambda)); /* compute exp(-lambda*tau) */
    for(j=0;j<n;j++)
      for(i=0;i<n;i++)
	z[i+j*n]=complex_subtract(z[i+j*n],complex_multiply(eld,make_complex(coef[km+i+n*j],0.0)));
  }
}

NewtonRun find_positive_root(xpp::Session &s, double *coef, double *delay, int n, int m, double rad, double err, double eps, double big, int maxit, double *rr)
{
  COMPLEX lambda,lambdap;
  COMPLEX det,detp;
  double jac[4];
  double xl,yl,r,xlp,ylp;

  int k;
  double change=0.0;

    lambda.r=s.delay.alpha_max;
    lambda.i=s.delay.omega_max;

   std::vector<COMPLEX> z(static_cast<size_t>(n)*n);

  /* now Newtons Method for maxit times */
  for(k=0;k<maxit;k++){

    fill_characteristic_matrix(z.data(),delay,n,m,coef,lambda);
    det=complex_determinant(z.data(),n);

    r=complex_modulus(det);
    if(r<err){ /* within the tolerance */
      process_root(lambda.r,lambda.i);
      s.delay.alpha_max=lambda.r;
      s.delay.omega_max=lambda.i;
      return {NewtonEnd::converged,k,0.0,0};
    }
    xl=lambda.r;
    yl=lambda.i;
   
    /* compute the Jacobian */
    r=xpp::difference_step(eps,xl);
    xlp=xl+r;
    lambdap=make_complex(xlp,yl);
    fill_characteristic_matrix(z.data(),delay,n,m,coef,lambdap);
    detp=complex_determinant(z.data(),n);
   jac[0]=(detp.r-det.r)/r;
   jac[2]=(detp.i-det.i)/r;
    r=xpp::difference_step(eps,yl);
    ylp=yl+r;
    lambdap=make_complex(xl,ylp);
    fill_characteristic_matrix(z.data(),delay,n,m,coef,lambdap);
    detp=complex_determinant(z.data(),n);
    jac[1]=(detp.r-det.r)/r;
    jac[3]=(detp.i-det.i)/r;
    r=jac[0]*jac[3]-jac[1]*jac[2];
    if(r==0)return {NewtonEnd::singular_jacobian,k,0.0,0};
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
      return {NewtonEnd::converged,k+1,r,0};
    }
    if(r>big)return {NewtonEnd::left_box,k+1,r,0};
    change=r;
  }

  return {NewtonEnd::max_iterates,maxit,change,0};
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
      else temp=make_complex(0.0,0.0);
      z[i+j*n]=complex_subtract(temp,make_complex(coef[i+j*n],0.0)); /* initialize the array */
    }
  for(k=0;k<m;k++){
    km=(k+1)*n*n;
    temp=make_complex(-delay[k],0.0); /* convert delay to complex number */
    eld=complex_exponential(complex_multiply(temp,lambda)); /* compute exp(-lambda*tau) */
    for(j=0;j<n;j++)
      for(i=0;i<n;i++)
	z[i+j*n]=complex_subtract(z[i+j*n],complex_multiply(eld,make_complex(coef[km+i+n*j],0.0)));
  }
  /*  the array is done  */
  temp=complex_determinant(z.data(),n);
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
    lambda=make_complex(x,y);
    arg=get_arg(delay,coef,m,n,lambda);
    sign=sign+test_sign(oldarg,arg);
    oldarg=arg;
 
  }
 /* lower contour   */
  y=-wmax;
  ds=almax/npts;
  for(i=0;i<npts;i++){
    x=i*ds;
    lambda=make_complex(x,y);
    arg=get_arg(delay,coef,m,n,lambda);
       sign=sign+test_sign(oldarg,arg);
    oldarg=arg;
 
  }
/* right contour */
 x=almax;
 ds=2*wmax/npts;
  for(i=0;i<npts;i++){
    y=-wmax+i*ds;
    lambda=make_complex(x,y);
    arg=get_arg(delay,coef,m,n,lambda);
      sign=sign+test_sign(oldarg,arg);
    oldarg=arg;
 
  }
 
/* top contour */
  y=wmax;
  ds=almax/npts;
  for(i=0;i<npts;i++){
    x=almax-i*ds;
    lambda=make_complex(x,y);
    arg=get_arg(delay,coef,m,n,lambda);
    sign=sign+test_sign(oldarg,arg);
    oldarg=arg;
  
  }
  return sign;
}

} // namespace xpp
