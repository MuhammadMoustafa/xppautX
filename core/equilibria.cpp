#include "solver.h"
#include "xpp_ui.h"
#include "session.h"
#include "odesol2.h"
#include "browse.h"
#include "xpp_log.h"
#include "xpp_math.h"
#include "equilibria.h"
#include "jacobian.h"
#include "graphics.h"
#include "flags.h"
#include "integrate.h"
#include "xpp_job.h"

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <algorithm>
#include <vector>
#include "xpp_globals.h"
#include "form_ode.h"
#include "load_eqn.h"
#include "model.h"

namespace xpp {
#define DING ping()

namespace {

/* What the equilibrium search finds from a start: the point (in x), the
   Jacobian there and its transposed copy, the eigenvalues and how many of
   them are real or complex, positive or negative. do_sing and
   do_sing_info both begin with it and differ only in what they report. */
struct Equilibrium {
  std::vector<double> buf, old_x;
  std::vector<int> ipivot;
  double *work=nullptr, *oldwork=nullptr, *eval=nullptr, *b=nullptr, *bp=nullptr;
  int rp=0, rn=0, cp=0, cn=0, im=0; /* real/complex, positive/negative; im: imaginary */
  int pose=0, nege=0;               /* the last real positive, negative eigenvalue */
  double bigpos=-1e10, bigneg=1e10; /* the largest of each sign */
  int bpos=0, bneg=0;               /* its index, -1 when it is complex */
};

enum class SingStatus { found, no_root, no_eigenvalues };

/* Newton from x for the root, then the eigenvalues of the Jacobian there;
   the counts only when found (*ierr says why not; x is put back when there
   is no root). Leaves work as the Jacobian transposed for the eigenvalue
   solver and oldwork as the Jacobian itself, work then back to oldwork
   transposed (as get_evec wants it). */
xpp::Result<SingStatus> find_equilibrium(xpp::Session &s, Equilibrium &q, double *x, double eps, double err, double big, int maxit, int n, int *ierr)
{
 int i,j;
 double temp;
 const int kmem=n*(2*n+5)+50;
 q.buf.assign(kmem,0.0);
 q.old_x.assign(x,x+n);
 q.ipivot.assign(n,0);
 q.work=q.buf.data();
 q.oldwork=q.work+n*n;
 q.eval=q.oldwork+n*n;
 q.b=q.eval+2*n;
 q.bp=q.b+n;
 double *ework=q.bp+n;
 s.manifolds.ic_flag=0;
 s.manifolds.count=0;
 if(auto r=rooter(s,x,err,eps,big,q.work,ierr,maxit,n);!r)return std::unexpected(r.error());
 if(*ierr!=0)
 {
  std::copy(q.old_x.begin(),q.old_x.end(),x);
  return SingStatus::no_root;
 }
 std::copy(q.work,q.work+n*n,q.oldwork);
/* Transpose for Eigen        */
 for(i=0;i<n;i++)
  for(j=i+1;j<n;j++)
  {
   temp=q.work[i+j*n];
   q.work[i+j*n]=q.work[i*n+j];
   q.work[i*n+j]=temp;
  }
 xpp::eigenvalues(n,q.work,q.eval,ework,ierr);
 if(*ierr!=0)return SingStatus::no_eigenvalues;
 for(i=0;i<n;i++)
 {
  double real=q.eval[2*i];
  double imag=q.eval[2*i+1];
  if(xpp::solver_info(s.numerics.method).traits.discrete)real=real*real+imag*imag-1.00;
  if(fabs(imag)<.00000001)imag=0.0;
  if(real<0.0)
  {
    if(imag!=0.0){
      q.cn++;
      if(real<q.bigneg){q.bigneg=real;q.bneg=-1;}
    }
    else
    {
     q.rn++;
     q.nege=i;
     if(real<q.bigneg){q.bigneg=real;q.bneg=i;}
    }
  }
  if(real>0.0)
  {
    if(imag!=0.0){
      q.cp++;
      if(real>q.bigpos){q.bigpos=real;q.bpos=-1;}
    }
    else
    {
     q.rp++;
     q.pose=i;
     if(real>q.bigpos){q.bigpos=real;q.bpos=i;}
    }
  }
  if((real==0.0)&&(imag!=0.0))q.im++;
 }     /* eigenvalue count */
 /* Lets change Work back to transposed oldwork */
 for(i=0;i<n;i++)
  for(j=i+1;j<n;j++)
  {
   temp=q.oldwork[i+j*n];
   q.work[i+j*n]=q.oldwork[i*n+j];
   q.work[i*n+j]=temp;
  }
 return SingStatus::found;
}

/* the equilibrium's symbol on the graph: saddle, unstable or stable */
void mark_equilibrium(xpp::Session &s, const Equilibrium &q, double *x)
{
 if(((q.rp+q.cp)!=0)&&((q.rn+q.cn)!=0))eq_symb(s,x,1);
 else
 {
   if((q.rp+q.cp)!=0)eq_symb(s,x,0);
   else eq_symb(s,x,3);
 }
}

}

/* main fixed point finder */ 
xpp::Result<> do_sing(xpp::Session &s, double *x, double eps, double err, double big, int maxit, int n, int *ierr, float *stabinfo)
{
 xpp::FirstError failure; /* an eigenvector or a manifold's */
 int i;
 xpp::Color oldcol,dummy;
 int pr;
 double oldt=s.numerics.delta_t;
 char ch;
 Equilibrium q;
 auto found=find_equilibrium(s,q,x,eps,err,big,maxit,n,ierr);
 if(!found)return std::unexpected(found.error());
 if(*found==SingStatus::no_root)
  return xpp::fail("equilibrium","Could not converge to root",command_place());
 DING;
 if(*found==SingStatus::no_eigenvalues)
  return xpp::fail("equilibrium","Could not compute eigenvalues",command_place());
 const int rp=q.rp,rn=q.rn,cp=q.cp,cn=q.cn,im=q.im,pose=q.pose,nege=q.nege;
 const int bpos=q.bpos,bneg=q.bneg;
 const double bigpos=q.bigpos,bigneg=q.bigneg;
 double *work=q.work,*oldwork=q.oldwork,*eval=q.eval,*b=q.b,*bp=q.bp;
 int *ipivot=q.ipivot.data();
/* succesfully computed evals now lets work with them */
ch='n';
if(!s.numerics.par_fol)
{
 ch=static_cast<char>(TwoChoice("YES","NO","Print eigenvalues?","yn"));
 
}
 pr=0;

 if(ch=='y')
 {
  xpp::log(XPP_LOG_INFO, "\n Eigenvalues:\n");
  pr=1;
  for(i=0;i<n;i++)
   xpp::log(XPP_LOG_INFO, " {:f}  +  i  {:f} \n",eval[2*i],eval[2*i+1]);
}
 mark_equilibrium(s,q,x);
 
 *stabinfo=static_cast<float>(cp+rp)+static_cast<float>(cn+rn)/1000.0;
 
 create_eq_box(s,cp,cn,rp,rn,im,x,eval,n);
 if(((rp==1)||(rn==1))&&(n>1))
 {
 ch='n';
 if(!s.numerics.par_fol)
 {
  ch=static_cast<char>(TwoChoice("YES","NO","Draw Invariant Sets?","yn"));
   }
  if((ch=='y')||(s.numerics.par_fol&&s.numerics.shoot))
  {
   oldt=s.numerics.delta_t;
  
   if(rp==1)
   {
     get_evec(s.random,work,oldwork,b,bp,n,maxit,err,ipivot,eval[2*pose],ierr);
     if(*ierr==0)
     {
     change_current_color(s,s.manifolds.unstable_color,&oldcol);
     pr_evec(s,x,b,n,pr,eval[2*pose],1);
      s.numerics.delta_t=fabs(s.numerics.delta_t);
      failure.keep(shoot(s,bp,x,b,1));
      failure.keep(shoot(s,bp,x,b,-1));
     change_current_color(s,oldcol,&dummy);

     }
     else
       failure.keep({"equilibrium","Failed to compute eigenvector"});
   }
   if(rn==1)
   {
     
     get_evec(s.random,work,oldwork,b,bp,n,maxit,err,ipivot,eval[2*nege],ierr);
     if(*ierr==0)
     {
        change_current_color(s,s.manifolds.stable_color,&oldcol);
	pr_evec(s,x,b,n,pr,eval[2*nege],-1);
      s.numerics.delta_t=-fabs(s.numerics.delta_t);
      failure.keep(shoot(s,bp,x,b,1));
      failure.keep(shoot(s,bp,x,b,-1));
        change_current_color(s,oldcol,&dummy);
     }
     else
       failure.keep({"equilibrium","Failed to compute eigenvector"});
   }
    s.numerics.delta_t=oldt;
  }
 }  /* end of normal shooting stuff */

 /* strong (un) stable manifold calculation  
    only one-d manifolds calculated */
 /* lets check to see if it is relevant */
 if(((rn>1)&&(bneg>=0))||((rp>1)&&(bpos>=0))) {
   ch='n';
   if(!s.numerics.par_fol)
     {
       ch=static_cast<char>(TwoChoice("YES","NO","Draw Strong Sets?","yn"));
     }

   if((ch=='y')||(s.numerics.par_fol&&s.numerics.shoot))
     {
       oldt=s.numerics.delta_t;

	 if((rp>1)&&(bpos>=0)) /* then there is a strong unstable */
	 {
	   xpp::log(XPP_LOG_INFO, "strong unstable {:g} \n",bigpos);
	   get_evec(s.random,work,oldwork,b,bp,n,maxit,err,ipivot,bigpos,ierr);
	   if(*ierr==0)
	     {
	       change_current_color(s,s.manifolds.unstable_color,&oldcol);
	       pr_evec(s,x,b,n,pr,bigpos,1);
	       s.numerics.delta_t=fabs(s.numerics.delta_t);
	       failure.keep(shoot(s,bp,x,b,1));
	       failure.keep(shoot(s,bp,x,b,-1));
	       change_current_color(s,oldcol,&dummy);
	       
	     }
	   else
	     failure.keep({"equilibrium","Failed to compute eigenvector"});   
	 }
	 
     if((rn>1)&&(bneg>=0)) /* then there is a strong stable */
	 {
	   xpp::log(XPP_LOG_INFO, "strong stable {:g} \n",bigneg);
	   get_evec(s.random,work,oldwork,b,bp,n,maxit,err,ipivot,bigneg,ierr);
	   if(*ierr==0)
	     {
	       change_current_color(s,s.manifolds.stable_color,&oldcol);
	       pr_evec(s,x,b,n,pr,bigneg,-1);
	       s.numerics.delta_t=-fabs(s.numerics.delta_t);
	       failure.keep(shoot(s,bp,x,b,1));
	       failure.keep(shoot(s,bp,x,b,-1));
	       change_current_color(s,oldcol,&dummy);
	     }
	   else
	     failure.keep({"equilibrium","Failed to compute eigenvector"});

	 }
     }
        s.numerics.delta_t=oldt;
 }

 return failure.result();
}

void save_batch_shoot(xpp::Session &s)
{
int i,k,type;
  double olddt;
  std::vector<double> x(s.model().node);
  if(s.manifolds.count<1)return;
  olddt=s.numerics.delta_t;
  s.numerics.storflag=true;
  for(k=0;k<s.manifolds.count;k++){
    for(i=0;i<s.model().node;i++)
      x[i]=s.manifolds.ic[k][i];

    type=s.manifolds.type[k];
    if(type>0){

       s.numerics.delta_t=fabs(s.numerics.delta_t);
       usual_integrate_stuff(s,x.data());
       {
         xpp::Writer w=xpp::ask_output_writer(s,"Save unstable manifold",".dat",xpp::format("eigen{}-unstable",k));
         if(!w){s.numerics.delta_t=olddt;return;}
         write_mybrowser_data(s,w);
         if(!xpp::ok_or_show(xpp::commit_save(w))){s.numerics.delta_t=olddt;return;}
       }
    }
    if(type<0){

       s.numerics.delta_t=-fabs(s.numerics.delta_t);
       usual_integrate_stuff(s,x.data());
       {
         xpp::Writer w=xpp::ask_output_writer(s,"Save stable manifold",".dat",xpp::format("eigen{}-stable",k));
         if(!w){s.numerics.delta_t=olddt;return;}
         write_mybrowser_data(s,w);
         if(!xpp::ok_or_show(xpp::commit_save(w))){s.numerics.delta_t=olddt;return;}
       }

    }
  }
  s.numerics.delta_t=olddt;

}
xpp::Result<> shoot_this_now(xpp::Session &s) /* this uses the current labeled saddle point stuff to integrate */
{
  xpp::FirstError failure;
  int i,k,type;
  xpp::Color oldcol,dummy;
  double olddt;
  std::vector<double> x(s.model().node);
  if(s.manifolds.count<1)return {};
  olddt=s.numerics.delta_t;

  for(k=0;k<s.manifolds.count;k++){
    for(i=0;i<s.model().node;i++)
      x[i]=s.manifolds.ic[k][i];
    
    type=s.manifolds.type[k];
    if(type>0){
       change_current_color(s,s.manifolds.unstable_color,&oldcol);
       s.numerics.delta_t=fabs(s.numerics.delta_t);
       failure.keep(shoot_easy(s,x.data()));
       change_current_color(s,oldcol,&dummy);
    }
    if(type<0){
      change_current_color(s,s.manifolds.stable_color,&oldcol);
       s.numerics.delta_t=-fabs(s.numerics.delta_t);
       failure.keep(shoot_easy(s,x.data()));
       change_current_color(s,oldcol,&dummy);
    }
  }
  s.numerics.delta_t=olddt;
  return failure.result();
}

/* fixed point with no requests and store manifolds */ 
xpp::Result<> do_sing_info(xpp::Session &s, double *x, double eps, double err, double big, int maxit, int n, double *er, double *em, int *ierr)
{
 Equilibrium q;
 auto found=find_equilibrium(s,q,x,eps,err,big,maxit,n,ierr);
 if(!found)return std::unexpected(found.error());
 if(*found!=SingStatus::found)return {};
 const int rp=q.rp,rn=q.rn,pose=q.pose,nege=q.nege;
 double *work=q.work,*oldwork=q.oldwork,*eval=q.eval,*b=q.b,*bp=q.bp;
 int *ipivot=q.ipivot.data();
 const int pr=0;
 for(int i=0;i<n;i++)
 {
  er[i]=eval[2*i];
  em[i]=eval[2*i+1];
 }
 mark_equilibrium(s,q,x);

 if((n>1))
 {
 
   if(rp==1)
   {
     get_evec(s.random,work,oldwork,b,bp,n,maxit,err,ipivot,eval[2*pose],ierr);

     if(*ierr==0)
     {
       pr_evec(s,x,b,n,pr,eval[2*pose],1);

     }

   }

   if(rn==1)
   {
     
     get_evec(s.random,work,oldwork,b,bp,n,maxit,err,ipivot,eval[2*nege],ierr);

     if(*ierr==0)
     {
       pr_evec(s,x,b,n,pr,eval[2*nege],-1);

     }

   }

 }

 return {};
}

void pr_evec(xpp::Session &s, double *x, double *ev, int n, int pr, double eval, int type)
{

 int i;
 double d=fabs(s.numerics.delta_t)*.1;
 s.manifolds.ic_flag=1;
 if(s.manifolds.count<7){
   const std::size_t room=static_cast<std::size_t>(std::max(n,s.model().node));
   s.manifolds.ic[s.manifolds.count].assign(room,0.0);
   s.manifolds.ic[s.manifolds.count+1].assign(room,0.0);
   for(i=0;i<n;i++){
     s.manifolds.ic[s.manifolds.count][i]=x[i]+d*ev[i];
     s.manifolds.type[s.manifolds.count]=type;
     s.manifolds.ic[s.manifolds.count+1][i]=x[i]-d*ev[i];
     s.manifolds.type[s.manifolds.count+1]=type;
   }
   s.manifolds.count+=2;
 }
 if(pr==0)return;
}

void get_evec(xpp::Random &random, double *a, double *anew, double *b, double *bp, int n, int maxit, double err, int *ipivot, double eval, int *ierr)
{
    int j,iter,jmax;
    double temp;
    double zz=fabs(eval);
    if(zz<err)zz=err;
    *ierr=0;
    for(j=0;j<n*n;j++){
    anew[j]=a[j];
    }
    for(j=0;j<n;j++)
    anew[j*(1+n)]=anew[j*(1+n)]-eval-err*err*zz;

    xpp::sgefa(anew,n,n,ipivot,ierr);
    if(*ierr!=-1) {
      xpp::log(XPP_LOG_WARN, " Pivot failed\n");
      return;
    }
    for(j=0;j<n;j++)
    {
     b[j]=1+.1*random.uniform();
     bp[j]=b[j];
    }
     iter=0;
     *ierr=0;
     while(1)
     {
      xpp::sgesl(anew,n,n,ipivot,b);
      temp=fabs(b[0]);
      jmax=0;

      for(j=0;j<n;j++)
      {

        if(fabs(b[j])>temp)
        {
         temp=fabs(b[j]);
	 jmax=j;

	}
      }
      temp=b[jmax];
      for(j=0;j<n;j++)
       b[j]=b[j]/temp;
      temp=0.0;
      for(j=0;j<n;j++)
      {
       temp=temp+fabs(b[j]-bp[j]);
       bp[j]=b[j];
      }
      if(temp<err)break;
      iter++;
      if(iter>maxit)
      {
       xpp::log(XPP_LOG_WARN, " max iterates exceeded\n");

       *ierr=1;
       break;
      }
     }
    if(*ierr==0){
      temp=fabs(b[0]);
      jmax=0;
      for(j=0;j<n;j++)
	{ 
	  if(fabs(b[j])>temp)
	    {
	      temp=fabs(b[j]);
	      jmax=j;
	    }
	}
      temp=b[jmax];
      for(j=0;j<n;j++)b[j]=b[j]/temp;
    }
     return;
  }

void getjac(xpp::Session &s, double *x, double *y, double eps, double *dermat, int n)
{
  const bool discrete=xpp::solver_info(s.numerics.method).traits.discrete;
  s.integrator.rhs(0.0,x,y,n);
  if(discrete)
    for(int i=0;i<n;i++)y[i]=y[i]-x[i];
  xpp::jacobian(s,0.0,x,y,n,eps,{xpp::JacobianLayout::RowMajor,0,0,discrete},dermat);
}

namespace {
void newton(xpp::Session &s, double *x, double err, double eps, double big, double *work, int *ierr, int maxit, int n)
{
 int i,iter,info;
 std::vector<int> ipivot(static_cast<std::size_t>(n));
 char ch;
 double *y,*dermat,*dely;
 double r;
 dermat=work;
 y=dermat+n*n;
 dely=y+n;
 iter=0;
 *ierr=0;
 
 while(1)
 {
   if(program.interactive){
     ch=my_abort();
 
     {
  
       if(ch==27)
	 {
	   *ierr=1;
	   return;
	 }
       if(ch=='/')
	 {
	   *ierr=1;
	   s.numerics.endsing=true;
	   return;
	 }
       if(ch=='p')s.numerics.pauser=true;
     }
   }
 
  getjac(s,x,y,eps,dermat,n);
  if(s.integrator.step_error)
  {
   *ierr=1;
   return;
  }
  xpp::sgefa(dermat,n,n,ipivot.data(),&info);
  if(info!=-1)
  {
   *ierr=1;
   return;
  }
  for(i=0;i<n;i++)dely[i]=y[i];
  xpp::sgesl(dermat,n,n,ipivot.data(),dely);
  r=0.0;
  for(i=0;i<n;i++)
  {
   x[i]=x[i]-dely[i];
   r=r+fabs(dely[i]);
  }
  if(r<err)
  {
     getjac(s,x,y,eps,dermat,n);
     if(xpp::solver_info(s.numerics.method).traits.discrete)
     for(i=0;i<n;i++)dermat[i*(n+1)]+=1.0;
     return; /* success !! */
  }
  if((r/static_cast<double>(n))>big)
  {
   *ierr=1;
   return;
   }
   iter++;
   if(iter>maxit)
   {
    *ierr=1;
    return;
   }
 }
}
}

xpp::Result<> rooter(xpp::Session &s, double *x, double err, double eps, double big, double *work, int *ierr, int maxit, int n)
{
 xpp::Computation computing; /* what Escape stops (xpp_job.h) */
 const std::vector<double> start(x,x+n);
 try {
   newton(s,x,err,eps,big,work,ierr,maxit,n);
 } catch (const xpp::JacobianFailed &failed) {
   *ierr=1;
   std::copy(start.begin(),start.end(),x);
   return std::unexpected(failed.error);
 }
 if(s.integrator.step_error){
   *ierr=1;
   std::copy(start.begin(),start.end(),x);
   return std::unexpected(take_step_error(s).error());
 }
 return {};
}
} // namespace xpp
