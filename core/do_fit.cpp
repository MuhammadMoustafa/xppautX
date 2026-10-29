#include <algorithm>
#include "data_formats.h"

#include "do_fit.h"
#include "jacobian.h"
#include "session.h"
#include "storage.h"
#include "form_ode.h"
#include "integrate.h"
#include "xpp_log.h"
#include "xpp_math.h"

#include "expr.h"
#include "derived.h"
#include <array>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <string>
#include <vector>
#include "delay_handle.h"
#include "browse.h"
#include "xpp_ui.h"
#include "mykeydef.h"
#include "xpp_job.h"

#include "load_eqn.h"
#include "model.h"

namespace xpp {

/*  this is also X free ! */

namespace {
/* the fitted quantity ipar names (an initial condition, else a parameter's constant: parse_parlist) */
std::string_view fit_parameter_name(const xpp::Session &s, int ipar)
{
  return ipar>=0?std::string_view(s.model().uvar_names[ipar]):parameter_name(s,-ipar);
}
}



xpp::Result<> get_fit_info(xpp::Session &s, double *y, double *a, double *t0, double eps, double *yfit, double **yderv, int npts, int npars, int nvars, int *ivar, int *ipar)
/*  
  y     initial condition
  a     initial guesses for the parameters
  t0    vector of output times
  returns why it failed (an integration or a delay's start), if it did
  eps   derivative step
  yfit  has y[i1](t0),...,y[im](t0), ..., y[i1](tn),...,y[im](tn)
        which are the values of the test functions at the npts 
	times.  yfit is (npts)*nvars int
  yderv[npar][nvars*(npts)] is the derivative of yfit with rrspect
        to the parameter

  npts   is the number of times to be fitted
  npars    the number of parameters
  nvars    the number of variables
  ipar     the vector of parameters  negative are constants
           positive are initial data
  ivar     the vector of variables
   
 */
{
  int i,iv,ip,istart=1,j,k,l,k0;
  std::vector<double> yold(s.model().node);
  double dp;
  double par;
/* set up all initial data and parameter guesses  */
  for(l=0;l<npars;l++){
    ip=ipar[l];
    if(ip<0)s.parser.constants[-ip]=a[l];
    else y[ip]=a[l];
  }
  for(i=0;i<s.model().node;i++){
    yold[i]=y[i];
  }
  if(s.delay.flag){
   /* restart initial data */
   if(auto d=do_init_delay(s,s.numerics.delay); !d)return std::unexpected(d.error());
  }
evaluate_derived(s);
  s.integrator.solver->begin(&istart);
/*   This gets the values at the desired points  */
  for(i=0;i<nvars;i++){
    iv=ivar[i];
    yfit[i]=y[iv];
  }
  for(k=1;k<npts;k++){
    if(my_abort()==ESC)return {};
    k0=k*nvars;
    auto ok=one_step_int(s,y,t0[k-1],t0[k],&istart);
    if(!ok){
         for(i=0;i<s.model().node;i++)
	y[i]=yold[i];

      return ok;
       }
   
    for(i=0;i<nvars;i++){
      iv=ivar[i];
      yfit[i+k0]=y[iv];
    }
  }   
  s.integrator.solver->finish();
  /*  Now we take the derivatives !!   */
  for(l=0;l<npars;l++){
    istart=1;
/* set up all the initial conditions   */
    for(j=0;j<nvars;j++)
      yderv[l][j]=0.0;   /* no dependence on initial data ... */
    for(i=0;i<s.model().node;i++)
      y[i]=yold[i];
    ip=ipar[l];
    if(ip<0){
      par=s.parser.constants[-ip];
      dp=xpp::difference_step(eps,par);
      s.parser.constants[-ip]=par+dp;
    }
    else {
      par=yold[ip];
      dp=xpp::difference_step(eps,par);
      y[ip]=par+dp;
      for(j=0;j<nvars;j++){
	if(ip==ivar[j])
	  yderv[l][j]=1.0;  /* ... except for those ICs that can vary */
      }
    }
    if(s.delay.flag){
   /* restart initial data */
   if(auto d=do_init_delay(s,s.numerics.delay); !d)return std::unexpected(d.error());
  }
    evaluate_derived(s);
    s.integrator.solver->begin(&istart);
   /* now loop through all the points */
    for(k=1;k<npts;k++){
      if(my_abort()==ESC)return {};
      k0=k*nvars;
      auto ok=one_step_int(s,y,t0[k-1],t0[k],&istart);
      if(!ok){
         for(i=0;i<s.model().node;i++)
	y[i]=yold[i];

       return ok;
     }
      for(i=0;i<nvars;i++){
	iv=ivar[i];
	yderv[l][i+k0]=(y[iv]-yfit[i+k0])/dp;
      }
    }
    /* Now return the parameter to its old value */
    if(ip<0)s.parser.constants[-ip]=par;
    evaluate_derived(s);
    s.integrator.solver->finish();

  }
     for(i=0;i<s.model().node;i++)
	y[i]=yold[i];
  return {};
}

xpp::Result<> one_step_int(xpp::Session &s, double *y, double t0, double t1, int *istart)
{
  xpp::Solver &solver=*s.integrator.solver;
  int neq=s.model().node;
  double dt=s.numerics.delta_t;
  double t=t0;
  xpp::Result<> r;
  if(!solver.traits().fixed_step){
    r=solver.advance({.y=y,.t=&t,.neq=neq,.start=istart,.tout=t1,.hguess=&dt});
    if(!r)return r;
    stor_delay(s,y);
    return {};
  }
  if(solver.traits().discrete){
    int nit=fabs(t0-t1);
    dt=dt/fabs(dt);
    return solver.advance({.y=y,.t=&t,.neq=neq,.start=istart,.dt=dt,.steps=nit});
  }
  int nit=static_cast<int>((t1-t0)/dt);
  r=solver.advance({.y=y,.t=&t,.neq=neq,.start=istart,.dt=dt,.steps=nit});
  if(!r)return r;
  if((dt<0&&t>t1)||(dt>0&&t<t1)){    
    dt=t1-t;
    r=solver.advance({.y=y,.t=&t,.neq=neq,.start=istart,.dt=dt,.steps=1});
    if(!r)return r;
  }

  return {};
} 

void print_fit_info(const xpp::Session &s)
{
  int i;
  xpp::log(XPP_LOG_INFO, "dim={} maxiter={} npts={} file={} tol={:g} eps={:g}\n",
	 s.fit.dim,s.fit.maxiter,s.fit.npts,s.fit.file.c_str(),s.fit.tol,s.fit.eps);

  for(i=0;i<s.fit.nvars;i++)
    xpp::log(XPP_LOG_INFO, " variable {} to col {} \n",
	   s.fit.ivar[i],s.fit.icols[i]);
  for(i=0;i<s.fit.npars;i++)
    xpp::log(XPP_LOG_INFO, " P[{}]={} \n",i,s.fit.ipar[i]);
}

namespace {
Result<DataTable> fit_data(const char *filename, int npts, int ndim, std::span<const int> icols)
{
  xpp::Result<xpp::DataTable> read=xpp::read_data_table(filename);
  if (!read) return std::unexpected(read.error());
  const xpp::DataTable &table=*read;
  if (table.columns.size() < static_cast<std::size_t>(ndim) || table.rows() < static_cast<std::size_t>(npts))
    return xpp::fail("fit", xpp::format("The data file needs {} rows of {} numbers", npts, ndim), xpp::Place{filename, static_cast<int>(table.rows()) + 1});
  for (int k=0; k<static_cast<int>(icols.size()); k++)
    if (icols[static_cast<std::size_t>(k)]<2 || static_cast<std::size_t>(icols[static_cast<std::size_t>(k)])>table.columns.size())
      return xpp::fail("fit", xpp::format("The data file has no column {}",icols[static_cast<std::size_t>(k)]), xpp::Place{filename,1});
  for (int i=0; i<npts; i++)
    for (int j=0; j<ndim; j++)
      if (!std::isfinite(table.columns[j][i]))
        return xpp::fail("fit", xpp::format("Invalid data value {}",table.columns[j][i]), xpp::Place{filename,table.line(i)});
  return std::move(*read);
}
}

Result<FitOutcome> test_fit(Session &s)
{
 return test_fit(s,s.fit);
}

Result<FitOutcome> test_fit(Session &s, FitInfo fit)
{
 int i;
 fit.nvars=0;
 fit.npars=0;
 parse_collist(fit.collist,fit.icols);
 
 if(fit.icols.empty())
   return fail("fit","No columns given",command_place());
 fit.nvars=static_cast<int>(fit.icols.size());
 parse_varlist(s,fit.varlist, fit.ivar);

 if(fit.nvars!=static_cast<int>(fit.ivar.size()))
   return fail("fit","The number of columns is not the number of fitted variables",command_place());
 fit.ipar.clear();
 parse_parlist(s,fit.parlist1,fit.ipar);
 parse_parlist(s,fit.parlist2,fit.ipar);

 if(fit.ipar.empty())
   return fail("fit","No parameters to vary",command_place());
 fit.npars=static_cast<int>(fit.ipar.size());
 for(i=0;i<fit.npars;i++)
   if(fit.ipar[i]>=0)
     {
       if(fit.ipar[i]>=s.model().node)
	 return fail("fit","Auxiliary and Markov variables cannot be fitted",command_place());
     }
 for(i=0;i<fit.nvars;i++){
   if(fit.icols[i]<2)
     return fail("fit","A column of the data is 2 or more",command_place());
   if(fit.ivar[i]<0||fit.ivar[i]>=s.model().node)
     return fail("fit","Fit only to variables",command_place());
 }
 if(fit.npts<2||fit.dim<2||fit.maxiter<1)
   return fail("fit",xpp::format("The fit needs at least 2 points (not {}), 2 columns (not {}) and 1 iteration (not {})",fit.npts,fit.dim,fit.maxiter),command_place());
  auto data = fit_data(fit.file.c_str(),fit.npts,fit.dim,fit.icols);
  if (!data) return std::unexpected(data.error());
  s.fit=std::move(fit);
  std::vector<double> initial(s.model().node);
  std::vector<double> a(s.fit.npars);
  for(i=0;i<s.model().node;i++)
    initial[i]=s.last_ic[i];
  for(i=0;i<s.fit.npars;i++){
    if(s.fit.ipar[i]<0)
      a[i]=s.parser.constants[-s.fit.ipar[i]];
    else
      a[i]=s.last_ic[s.fit.ipar[i]];
  }

 print_fit_info(s);
 xpp::log(XPP_LOG_INFO, " Running the fit...\n");
 auto ok=run_fit(s,*data, s.fit.npts,s.fit.npars,s.fit.nvars,s.fit.maxiter,
         s.fit.eps,s.fit.tol,
	 s.fit.ipar.data(),s.fit.ivar.data(),s.fit.icols.data(),
	 initial.data(),a.data());

   if(!ok){
     ping();
     return std::unexpected(ok.error());
   }
   switch(ok->end){
   case FitEnd::Converged: bottom_msg(0," Success! "); break;
   case FitEnd::Stopped: bottom_msg(0," Stopped "); break;
   case FitEnd::MaxIterations:
     bottom_msg(0,xpp::format("Fit: stopped after maxiter = {} iterations, chisq = {} (the last relative change {} is not below tol = {}); the parameters are set to the last iterate",
                              s.fit.maxiter,xpp::number(ok->chisq),xpp::number(ok->change),xpp::number(s.fit.tol)));
     break;
   }

 /* get the latest par values ...  */
 FitOutcome outcome{*ok,{}};
 for(i=0;i<s.fit.npars;i++){
   if(s.fit.ipar[i]<0)
     s.parser.constants[-s.fit.ipar[i]]=a[i];
   else
     s.last_ic[s.fit.ipar[i]]=a[i];
   outcome.parameters.emplace_back(std::string(fit_parameter_name(s,s.fit.ipar[i])),a[i]);
 }
 return outcome;
}

xpp::Result<FitResult> run_fit(xpp::Session &s, const DataTable &table, int npts, int npars, int nvars, int maxiter, double eps, double tol, int *ipar, int *ivar, int *icols, double *y0, double *a)
/* The data table and fitted columns were validated before the fit settings were applied. */
{
  int i,j,k,ictrl=0;
  xpp::Result<> ok;
  int niter=0,good_flag=0;
  double tol10=10*tol,change=0.0;
  std::vector<double> sig(nvars);
  double chisq=0.0,ochisq=0.0,alambda=0.0;

  /* the rows are in the file, so this is no more than it holds */
  std::vector<double> yfit_v(static_cast<size_t>(npts)*nvars);
  double *yfit=yfit_v.data();
  std::vector<double> t0_v(static_cast<size_t>(npts)+1);
  std::vector<double> y_v(static_cast<size_t>(npts+1)*nvars);
  double *t0=t0_v.data(), *y=y_v.data();
  for (i=0; i<npts; i++) {
    t0[i]=table.columns[0][i];
    for (k=0; k<nvars; k++) y[nvars*i+k]=table.columns[icols[k]-1][i];
  }
  xpp::log(XPP_LOG_INFO, " Data loaded ... {:f} {:f} ...  {:f} {:f} \n",
	 y[0],y[1],y[npts*nvars-2],y[npts*nvars-1]);

  std::vector<double> work_v(static_cast<size_t>(4*npars+npars*npars));
  double *work=work_v.data();
  std::vector<std::vector<double>> yderv_store(npars);
  std::vector<double *> yderv(npars);
  for(i=0;i<npars;i++){
    yderv_store[i].assign(static_cast<size_t>(npts+1)*nvars, 0.0);
    yderv[i]=yderv_store[i].data();
  }
  for(i=0;i<nvars;i++)
    sig[i]=1.0;

  std::vector<double> covar_v(static_cast<size_t>(npars)*npars);
  std::vector<double> alpha_v(static_cast<size_t>(npars)*npars);
  double *covar=covar_v.data(), *alpha=alpha_v.data();

  while(good_flag<3){  /* take 3 good steps after convergence  */
    if(my_abort()==ESC)return FitResult{FitEnd::Stopped,niter,chisq,change};

    ok=marlevstep(s,t0,y0,y,sig.data(),a,npts,nvars,npars,
	       ivar,ipar,covar,alpha,&chisq,&alambda,work,
	       yderv.data(),yfit,&ochisq,ictrl,eps);
    if(xpp::job::cancelled())return FitResult{FitEnd::Stopped,niter,chisq,change};
    niter++;
    xpp::log(XPP_LOG_INFO, " step {} is {}  -- lambda= {:g}  chisq= {:g} oldchi= {:g}\n",
	   niter,ok.has_value()?1:0,alambda,chisq,ochisq);
    xpp::log(XPP_LOG_INFO, " params: ");
    for(i=0;i<npars;i++)
      xpp::log(XPP_LOG_INFO, " {:g} ",a[i]);
    xpp::log(XPP_LOG_INFO, "\n");
    if(!ok||(niter>=maxiter))break;
    change=ochisq>chisq?(ochisq-chisq)/std::max(chisq,1.0):0.0;
    if(ochisq>chisq){
      if(((ochisq-chisq)<tol10)||(change<tol))
      {
	good_flag++;
	niter--;  /* compensate for good stuff ... */
	}
    ochisq=chisq;
    }
    else
      chisq=ochisq;

    ictrl=1;

  }

  if(!ok){
    xpp::Error failed=ok.error();
    failed.what=xpp::format("Fit iteration {}: {}",niter,failed.what);
    return std::unexpected(std::move(failed));
  }
  if(niter>=maxiter)return FitResult{FitEnd::MaxIterations,niter,chisq,change};
  ictrl=2;
  /* the covariance: a singular matrix here is still the fit's error, as
     it was shown before W63d */
  if(auto last=marlevstep(s,t0,y0,y,sig.data(),a,npts,nvars,npars,
	       ivar,ipar,covar,alpha,&chisq,&alambda,work,
	       yderv.data(),yfit,&ochisq,ictrl,eps); !last)
    return std::unexpected(last.error());
  /* have the covariance matrix -- so what?   */
  xpp::log(XPP_LOG_INFO, " covariance: \n");
  for(i=0;i<npars;i++){
    for(j=0;j<npars;j++)
      xpp::log(XPP_LOG_INFO, " {:g} ",covar[i+npars*j]);
    xpp::log(XPP_LOG_INFO, "\n");
  }

  return FitResult{FitEnd::Converged,niter,chisq,change};
}

xpp::Result<> marlevstep(xpp::Session &s, double *t0, double *y0, double *y, double *sig, double *a, int npts, int nvars, int npars, int *ivar, int *ipar, double *covar, double *alpha, double *chisq, double *alambda, double *work, double **yderv, double *yfit, double *ochisq, int ictrl, double eps)
/*   One step of Levenberg-Marquardt  
     
nvars  the number of variables to fit
ivar   their indices
npars  the number of parameters to alter
ipar   their indices
npts   the number of times
ictrl  0 to start  1 to continue  2 to finish up

t0  the npts times
y0  the NODE initial data
y   the (npts)*nvars data points to fit
sig  the nvars  weights
a  the npars initial guesses of the things to be fit
chisq  the chisquare
alpha is work array and also the curvature matrix
covar is the covariance matrix (npars x npars)
alambda is control of step size; start negative 0 to get final value
work is an array of size npar*4+npar*npar
yderv is  npar x (nptts+1)*nvars
yfit  is  (npts)*nvars  on each completed step it has the fitted soln
eps   control numerical derivative
sigma  weights on nvars
*/
{
  int i,j,k,ierr;
  int *ipivot = s.solver_work.size_pivots(npars);

  double *da,*atry,*beta,*oneda;
  da=work;
  atry=work+npars;
  beta=work+2*npars;
  oneda=work+3*npars;

  if(ictrl==0){
    *alambda=.001;
    if(auto m=mrqcof(s,t0,y0,y,sig,a,npts,nvars,npars,
	      ivar,ipar,alpha,chisq,beta,
	      yderv,yfit,eps); !m)
      return m;
    for(i=0;i<npars;i++)atry[i]=a[i];
    *ochisq=(*chisq);
  }
  for(j=0;j<npars;j++){
    for(k=0;k<npars;k++) covar[j+k*npars]=alpha[j+k*npars];
    covar[j+j*npars]=alpha[j+j*npars]*(1+(*alambda));
    oneda[j]=beta[j];
  }
  xpp::sgefa(covar,npars,npars,ipivot,&ierr);
    if(ierr!=-1)
      return xpp::fail("fit",xpp::format("Singular matrix encountered: with lambda = {}, the parameter {} (column {} of {}) does not change the fit independently of the ones before it",
                       xpp::number(*alambda),fit_parameter_name(s,ipar[ierr]),ierr+1,npars),command_place());
  
  xpp::sgesl(covar,npars,npars,ipivot,oneda);
  for(j=0;j<npars;j++){
    da[j]=oneda[j];
  }
  if(ictrl==2){  /* all done invert alpha to get the covariance */
    for(j=0;j<(npars*npars);j++)
      alpha[j]=covar[j];
    for(j=0;j<npars;j++){
      for(k=0;k<npars;k++)oneda[k]=0.0;
      oneda[j]=1.0;
      xpp::sgesl(alpha,npars,npars,ipivot,oneda);
      for(k=0;k<npars;k++)covar[j+k*npars]=oneda[k];
    }
    return {};
  }
  for(j=0;j<npars;j++) {
    atry[j]=a[j]+da[j];
  }
  if(auto m=mrqcof(s,t0,y0,y,sig,atry,npts,nvars,npars,
	   ivar,ipar,covar,chisq,da,
	   yderv,yfit,eps); !m)return m;

  if(*chisq<*ochisq){
    *alambda *= 0.1;
    for(j=0;j<npars;j++){
      for(k=0;k<npars;k++) alpha[j+k*npars]=covar[j+k*npars];
      beta[j]=da[j];
      a[j]=atry[j];
    }
  }
  else {
    *alambda *= 10.0;
  }
  return {};
}

 xpp::Result<> mrqcof(xpp::Session &s, double *t0, double *y0, double *y, double *sig, double *a, int npts, int nvars, int npars, int *ivar, int *ipar, double *alpha, double *chisq, double *beta, double **yderv, double *yfit, double eps)
{
       int i,j,k,l,k0;
       double sig2i,dy,wt;
      
       if(auto g=get_fit_info(s,y0,a,t0,eps,yfit,yderv,npts,npars,nvars,ivar,ipar); !g)
         return g;
       for(i=0;i<npars;i++){
	 beta[i]=0.0;
	 for(j=0;j<npars;j++){
	   alpha[i+j*npars]=0.0;
	 }
       }
       *chisq=0.0;
       for(i=0;i<nvars;i++){
	 sig2i=1.0/(sig[i]*sig[i]);
	 for(k=0;k<npts;k++){
	   k0=k*nvars+i;
	   dy=y[k0]-yfit[k0];
	   for(j=0;j<npars;j++){
	     wt=yderv[j][k0]*sig2i;
	     for(l=0;l<npars;l++)
	       alpha[j+l*npars] += wt*yderv[l][k0]; 
	     beta[j] += dy*wt;
	   }
	   (*chisq) += dy*dy*sig2i;

/* the last loop could be halved because of symmetry, but I am lazy 
   and this is an insignificiant amount of the CPU time since
  the evaluation step is really where all the time is used
*/
	 }
       }
       return {};
     }

int get_fit_params(xpp::Session &s)
{
  static const char *const n[]={"Fitvar","Params","Tolerance","Npts",
		    "NCols","To Col","Params","Epsilon","Max iter"};
  int status;
  std::array<std::string, 9> values;
  values[0] = s.fit.varlist;
  values[1] = s.fit.parlist1;
  values[2] = xpp::format("{:g}", s.fit.tol);
  values[3] = xpp::format("{}", s.fit.npts);
  values[4] = xpp::format("{}", s.fit.dim);
  values[5] = s.fit.collist;
  values[6] = s.fit.parlist2;
  values[7] = xpp::format("{:g}", s.fit.eps);
  values[8] = xpp::format("{}", s.fit.maxiter);
  static const int kinds[]={XPP_FIELD_TEXT,XPP_FIELD_TEXT,XPP_FIELD_NUMBER,XPP_FIELD_INTEGER,
                            XPP_FIELD_INTEGER,XPP_FIELD_TEXT,XPP_FIELD_TEXT,XPP_FIELD_NUMBER,XPP_FIELD_INTEGER};
  status=do_string_box_of(9,1,"Fit",n,values,kinds);
  if(status!=0){
    if(!file_selector("Load fit data",s.fit.file,"*"))return 0;
    s.fit.tol=atof(values[2].c_str());
    s.fit.npts=atoi(values[3].c_str());
    s.fit.dim=atoi(values[4].c_str());
    s.fit.eps=atof(values[7].c_str());
    s.fit.maxiter=atoi(values[8].c_str());
    s.fit.varlist=values[0];
    s.fit.parlist1=values[1];
    s.fit.collist=values[5];
    s.fit.parlist2=values[6];
     return(1);
  }
  return(0);
}

/* gets a list of the data columns to use ... */

void parse_collist(std::string_view collist, std::vector<int> &icols)
{
  icols.clear();
  xpp::Tokens tokens(collist);
  for(std::optional<std::string_view> item;(item=tokens.next(" ,"));)
    icols.push_back(atoi(std::string(*item).c_str()));
}

void parse_varlist(const xpp::Session &s, std::string_view varlist, std::vector<int> &ivars)
{
  ivars.clear();
  xpp::Tokens tokens(varlist);
  int v;
  for(std::optional<std::string_view> item;(item=tokens.next(" ,"));){
    find_variable(s,*item,&v);
    if(v<=0)return;
    ivars.push_back(v-1);
  }
}

void parse_parlist(const xpp::Session &s, std::string_view parlist, std::vector<int> &ipars)
{
  xpp::Tokens tokens(parlist);
  int v;
  for(std::optional<std::string_view> item;(item=tokens.next(" ,"));){
    find_variable(s,*item,&v);
    if(v>0){
      ipars.push_back(v-1);
    }
    else {
      v=get_param_index(s,*item);
      if(v<=0)return;
      ipars.push_back(-v);
    }
  }
}

} // namespace xpp
