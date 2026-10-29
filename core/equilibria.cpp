#include "xpp_files.h"
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
#include "model_files.h"
#include "numerics_settings.h"

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <algorithm>
#include <span>
#include <string>
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
  NewtonRun run;                    /* how Newton's method ended */
};

enum class SingStatus { found, no_root, no_eigenvalues };

/* Newton from x for the root, then the eigenvalues of the Jacobian there;
   the counts only when found (q.run says why there is no root, *ierr that
   there is none; x is put back when there is no root). With ghost, a start
   that has no root goes on with its own Jacobian (W518: the ghost of a fold
   has its near-zero eigenvalue), q.run still saying there was no root.
   Leaves work as the Jacobian transposed for the eigenvalue
   solver and oldwork as the Jacobian itself, work then back to oldwork
   transposed (as get_evec wants it). */
xpp::Result<SingStatus> find_equilibrium(xpp::Session &s, Equilibrium &q, double *x, double eps, double err, double big, int maxit, int n, int *ierr, bool ghost=false)
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
 if(auto r=rooter(s,x,err,eps,big,q.work,q.run,maxit,n);!r)return std::unexpected(r.error());
 *ierr=q.run.end==NewtonEnd::converged?0:1;
 if(*ierr!=0)
 {
  std::copy(q.old_x.begin(),q.old_x.end(),x);
  if(!ghost)return SingStatus::no_root;
  getjac(s,x,q.oldwork,eps,q.work,n); /* oldwork is free until the copy below: getjac's scratch */
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

NewtonCause newton_cause(const Session &s, const NewtonRun &run, std::size_t variables)
{
 switch(run.end){
 case NewtonEnd::singular_jacobian:
   return {xpp::format("met a singular Jacobian after {} iterations",run.iterations),""};
 case NewtonEnd::left_box:
   return {xpp::format("went out of the search box after {} iterations (a step of {} per variable is above {})",
                       run.iterations,xpp::number(run.change/static_cast<double>(variables)),named_settings(s,{"search_limit"})),
           "search_limit"};
 case NewtonEnd::max_iterates:
   return {xpp::format("exceeded {} with the last change {} not below {}",named_settings(s,{"max_iterations"}),
                       xpp::number(run.change),named_settings(s,{"newton_tolerance"})),
           "max_iterations"};
 case NewtonEnd::cancelled:
   return {xpp::format("was stopped after {} iterations",run.iterations),""};
 case NewtonEnd::converged:
 case NewtonEnd::step_error:
   break;
 }
 return {};
}

Error newton_error(const Session &s, const NewtonRun &run, std::span<const double> start)
{
 const Model &m=s.model();
 std::string from;
 for(std::size_t i=0;i<start.size();i++)
   from+=xpp::format("{}{} = {}",i?", ":"",m.uvar_names[i],xpp::number(start[i]));
 const NewtonCause cause=newton_cause(s,run,start.size());
 Place place=cause.setting.empty()?Place{}:model_place(m,cause.setting);
 if(place.line==0&&run.end!=NewtonEnd::cancelled)place=model_place(m,m.uvar_names[run.worst]);
 if(place.line==0)place=command_place();
 return {"equilibrium",xpp::format("No equilibrium from {}: Newton's method {}",from,cause.why),place};
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
 s.manifolds.ic_flag=0;
 s.manifolds.count=0;
 auto found=find_equilibrium(s,q,x,eps,err,big,maxit,n,ierr);
 if(!found)return std::unexpected(found.error());
 if(*found==SingStatus::no_root)
  return std::unexpected(newton_error(s,q.run,q.old_x));
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
 ch=s.print_eigenvalues?'y':'n';
 
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
  ch=s.draw_invariant_sets?'y':'n';
   }
  if((ch=='y')||(s.numerics.par_fol&&s.numerics.shoot))
  {
   oldt=s.numerics.delta_t;
  
   if(rp==1)
   {
     const xpp::Result<> evec=get_evec(s,work,oldwork,b,bp,n,maxit,err,ipivot,eval[2*pose]);
     if(evec)
     {
     change_current_color(s,s.manifolds.unstable_color,&oldcol);
     pr_evec(s,x,b,n,pr,eval[2*pose],1);
      s.numerics.delta_t=fabs(s.numerics.delta_t);
      failure.keep(shoot(s,bp,x,b,1));
      failure.keep(shoot(s,bp,x,b,-1));
     change_current_color(s,oldcol,&dummy);

     }
     else
       failure.keep(evec);
   }
   if(rn==1)
   {
     
     const xpp::Result<> evec=get_evec(s,work,oldwork,b,bp,n,maxit,err,ipivot,eval[2*nege]);
     if(evec)
     {
        change_current_color(s,s.manifolds.stable_color,&oldcol);
	pr_evec(s,x,b,n,pr,eval[2*nege],-1);
      s.numerics.delta_t=-fabs(s.numerics.delta_t);
      failure.keep(shoot(s,bp,x,b,1));
      failure.keep(shoot(s,bp,x,b,-1));
        change_current_color(s,oldcol,&dummy);
     }
     else
       failure.keep(evec);
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
       ch=s.draw_strong_sets?'y':'n';
     }

   if((ch=='y')||(s.numerics.par_fol&&s.numerics.shoot))
     {
       oldt=s.numerics.delta_t;

	 if((rp>1)&&(bpos>=0)) /* then there is a strong unstable */
	 {
	   xpp::log(XPP_LOG_INFO, "strong unstable {:g} \n",bigpos);
	   const xpp::Result<> evec=get_evec(s,work,oldwork,b,bp,n,maxit,err,ipivot,bigpos);
     if(evec)
	     {
	       change_current_color(s,s.manifolds.unstable_color,&oldcol);
	       pr_evec(s,x,b,n,pr,bigpos,1);
	       s.numerics.delta_t=fabs(s.numerics.delta_t);
	       failure.keep(shoot(s,bp,x,b,1));
	       failure.keep(shoot(s,bp,x,b,-1));
	       change_current_color(s,oldcol,&dummy);
	       
	     }
	   else
	     failure.keep(evec);   
	 }
	 
     if((rn>1)&&(bneg>=0)) /* then there is a strong stable */
	 {
	   xpp::log(XPP_LOG_INFO, "strong stable {:g} \n",bigneg);
	   const xpp::Result<> evec=get_evec(s,work,oldwork,b,bp,n,maxit,err,ipivot,bigneg);
     if(evec)
	     {
	       change_current_color(s,s.manifolds.stable_color,&oldcol);
	       pr_evec(s,x,b,n,pr,bigneg,-1);
	       s.numerics.delta_t=-fabs(s.numerics.delta_t);
	       failure.keep(shoot(s,bp,x,b,1));
	       failure.keep(shoot(s,bp,x,b,-1));
	       change_current_color(s,oldcol,&dummy);
	     }
	   else
	     failure.keep(evec);

	 }
     }
        s.numerics.delta_t=oldt;
 }

 return failure.result();
}

Result<> save_batch_shoot(Session &s,std::string_view file,bool confirm)
{
  if(s.manifolds.count<1)return {};
  const double olddt=s.numerics.delta_t;
  /* every branch starts at the saddle's time, as the drawn ones do; the session's clock is put back after */
  const double begun=s.data_store.current_time;
  const auto result=[&]() -> Result<> {
    std::vector<double> x(s.model().neq);
    s.numerics.storflag=true;
    for(int k=0;k<s.manifolds.count;++k){
      std::copy(s.manifolds.ic[k].begin(),s.manifolds.ic[k].end(),x.begin());
      if(s.manifolds.type[k]==0)continue;
      const bool unstable=s.manifolds.type[k]>0;
      s.numerics.delta_t=unstable?fabs(olddt):-fabs(olddt);
      s.data_store.current_time=BRANCH_START_TIME;
      const BranchStoreFrom branch(s);
      if(const auto run=integrate_from(s,x.data());!run)return std::unexpected(run.error());
      if(job::cancelled())return {};
      const std::string name=files::output_name(file,".dat",xpp::format("eigen{}-{}",k,unstable?"unstable":"stable"));
      Result<> opened;
      Writer writer=confirm?open_writer_replacing(name,false,&opened):open_writer_asking(name,false,&opened);
      if(!opened)return opened;
      if(!writer)return fail("equilibrium.export",xpp::format("Manifold output {} was not opened",name),command_place());
      write_mybrowser_data(s,writer);
      if(const auto saved=commit_save(writer);!saved)return saved;
    }
    return {};
  }();
  s.numerics.delta_t=olddt;
  s.data_store.current_time=begun;
  return result;
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

xpp::Result<EquilibriumNear> equilibrium_near(xpp::Session &s, std::span<const double> start)
{
 const int n=static_cast<int>(start.size());
 const auto &eq=s.numerics.equilibria;
 Equilibrium q;
 EquilibriumNear root;
 root.x.assign(start.begin(),start.end());
 int ierr=0;
 auto found=find_equilibrium(s,q,root.x.data(),eq.jacobian_step,eq.newton_tolerance,eq.search_limit,eq.max_iterations,n,&ierr,true);
 if(!found)return std::unexpected(found.error());
 if(*found==SingStatus::no_eigenvalues)return xpp::fail("equilibrium","Could not compute eigenvalues",command_place());
 root.found=q.run.end==NewtonEnd::converged;
 root.re.resize(static_cast<std::size_t>(n));
 root.im.resize(static_cast<std::size_t>(n));
 for(int i=0;i<n;i++){
  root.re[static_cast<std::size_t>(i)]=q.eval[2*i];
  root.im[static_cast<std::size_t>(i)]=q.eval[2*i+1];
 }
 return root;
}

/* fixed point with no requests and store manifolds */ 
xpp::Result<> do_sing_info(xpp::Session &s, double *x, double eps, double err, double big, int maxit, int n, double *er, double *em, int *ierr)
{
 xpp::FirstError failure; /* report a failed manifold through the calling command */
 Equilibrium q;
 s.manifolds.ic_flag=0;
 s.manifolds.count=0;
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

 if(n>1)
 {
   /* an eigenvector that cannot be found only leaves out its manifold */
   auto manifold=[&](double eigenvalue,int type){
     if(const xpp::Result<> evec=get_evec(s,work,oldwork,b,bp,n,maxit,err,ipivot,eigenvalue);evec)
       pr_evec(s,x,b,n,pr,eigenvalue,type);
     else
       failure.keep(evec);
   };
   if(rp==1)manifold(eval[2*pose],1);
   if(rn==1)manifold(eval[2*nege],-1);
 }

 return failure.result();
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

xpp::Result<> get_evec(xpp::Session &s, double *a, double *anew, double *b, double *bp, int n, int maxit, double err, int *ipivot, double eval)
{
    int j,iter,jmax,ierr;
    double temp;
    double zz=fabs(eval);
    if(zz<err)zz=err;
    ierr=0;
    for(j=0;j<n*n;j++){
    anew[j]=a[j];
    }
    for(j=0;j<n;j++)
    anew[j*(1+n)]=anew[j*(1+n)]-eval-err*err*zz;

    xpp::sgefa(anew,n,n,ipivot,&ierr);
    if(ierr!=-1)
      return xpp::fail("equilibrium",xpp::format("No eigenvector for the eigenvalue {}: the Jacobian shifted by it is singular ({})",
                       xpp::number(eval),named_settings(s,{"newton_tolerance"})),settings_place(s.model(),{"newton_tolerance"}));
    for(j=0;j<n;j++)
    {
     b[j]=1+.1*s.random.uniform();
     bp[j]=b[j];
    }
     iter=0;
     ierr=0;
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
        return xpp::fail("equilibrium",xpp::format("No eigenvector for the eigenvalue {}: the inverse iteration exceeded {} with the last change {} not below {}",
                         xpp::number(eval),named_settings(s,{"max_iterations"}),xpp::number(temp),named_settings(s,{"newton_tolerance"})),
                         settings_place(s.model(),{"max_iterations","newton_tolerance"}));
     }
    {
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
     return {};
  }

void getjac(xpp::Session &s, double *x, double *y, double eps, double *dermat, int n)
{
  const bool discrete=xpp::solver_info(s.numerics.method).traits.discrete;
  s.integrator.rhs(0.0,x,y,n);
  if(discrete)
    for(int i=0;i<n;i++)y[i]=y[i]-x[i];
  xpp::jacobian(s,0.0,x,y,n,eps,{xpp::JacobianLayout::RowMajor,0,0,discrete},dermat);
}

int worst_residual(const double *f, int n)
{
 int k=0;
 for(int i=1;i<n;i++)
   if(fabs(f[i])>fabs(f[k]))k=i;
 return k;
}

namespace {
void newton(xpp::Session &s, double *x, double err, double eps, double big, double *work, NewtonRun &run, int maxit, int n)
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
 run=NewtonRun{};
 auto stop=[&](NewtonEnd end){
   run.end=end;
   run.iterations=iter;
   run.worst=worst_residual(y,n);
 };
 
 while(1)
 {
   if(program.interactive){
     ch=my_abort();
 
     {
  
       if(ch==27)
	 {
	   stop(NewtonEnd::cancelled);
	   return;
	 }
       if(ch=='/')
	 {
	   stop(NewtonEnd::cancelled);
	   s.numerics.endsing=true;
	   return;
	 }
       if(ch=='p')s.numerics.pauser=true;
     }
   }
 
  getjac(s,x,y,eps,dermat,n);
  if(s.integrator.step_error)
  {
   stop(NewtonEnd::step_error);
   return;
  }
  xpp::sgefa(dermat,n,n,ipivot.data(),&info);
  if(info!=-1)
  {
   stop(NewtonEnd::singular_jacobian);
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
  run.change=r;
  if(r<err)
  {
     getjac(s,x,y,eps,dermat,n);
     if(xpp::solver_info(s.numerics.method).traits.discrete)
     for(i=0;i<n;i++)dermat[i*(n+1)]+=1.0;
     run.iterations=iter+1;
     return; /* success !! */
  }
  if((r/static_cast<double>(n))>big)
  {
   stop(NewtonEnd::left_box);
   return;
   }
   iter++;
   if(iter>maxit)
   {
    stop(NewtonEnd::max_iterates);
    return;
   }
 }
}
}

xpp::Result<> rooter(xpp::Session &s, double *x, double err, double eps, double big, double *work, NewtonRun &run, int maxit, int n)
{
 xpp::Computation computing; /* what Escape stops (xpp_job.h) */
 const std::vector<double> start(x,x+n);
 try {
   newton(s,x,err,eps,big,work,run,maxit,n);
 } catch (const xpp::JacobianFailed &failed) {
   std::copy(start.begin(),start.end(),x);
   return std::unexpected(failed.error);
 }
 if(s.integrator.step_error){
   std::copy(start.begin(),start.end(),x);
   return std::unexpected(take_step_error(s).error());
 }
 return {};
}
} // namespace xpp
