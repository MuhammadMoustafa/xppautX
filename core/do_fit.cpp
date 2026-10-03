#include "data_formats.h"

#include "do_fit.h"
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

#include "load_eqn.h"
#include "model.h"

namespace xpp {

/*  this is also X free ! */
 
#define MAX(a,b) ((a)>(b)?(a):(b))



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
  double yold[MAXODE],dp;
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
      dp=eps*MAX(eps,fabs(par));
      s.parser.constants[-ip]=par+dp;
    }
    else {
      par=yold[ip];
      dp=eps*MAX(eps,fabs(par));
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

void test_fit(xpp::Session &s)
{
 std::array<double, 1000> a{}, y0{};
 int nvars,npars,i;
 s.fit.nvars=0;
 s.fit.npars=0;
 if(get_fit_params(s)==0)return;
 parse_collist(s.fit.collist,s.fit.icols.data(),&nvars);
 
 if(nvars<=0){
   command_error("fit", "No columns given");
   return;
 }
 s.fit.nvars=nvars;
 nvars=0;
 parse_varlist(s,s.fit.varlist, s.fit.ivar.data(), &nvars);

 if(s.fit.nvars!=nvars){
   command_error("fit", "The number of columns is not the number of fitted variables");
   return;
 }
 npars=0;
 parse_parlist(s,s.fit.parlist1,s.fit.ipar.data(),&npars);

 parse_parlist(s,s.fit.parlist2,s.fit.ipar.data(),&npars);

 if(npars<=0){
   command_error("fit", "No parameters to vary");
   return;
 }
 s.fit.npars=npars;
 for(i=0;i<npars;i++)
   if(s.fit.ipar[i]>=0)
     {
       if(s.fit.ipar[i]>=s.model().node){
	 command_error("fit", "Auxiliary and Markov variables cannot be fitted");
	 return;
       }
     }
 for(i=0;i<nvars;i++){
   if(s.fit.icols[i]<2){
     command_error("fit", "A column of the data is 2 or more");
     return;
   }
   if(s.fit.ivar[i]<0||s.fit.ivar[i]>=s.model().node){
     command_error("fit", "Fit only to variables");
     return;
   }
 }
 std::vector<double> yfit_v(static_cast<size_t>(s.fit.npts)*s.fit.nvars);
  double *yfit=yfit_v.data();
  for(i=0;i<s.model().node;i++)
    y0[i]=s.last_ic[i];
  for(i=0;i<s.fit.npars;i++){
    if(s.fit.ipar[i]<0)
      a[i]=s.parser.constants[-s.fit.ipar[i]];
    else
      a[i]=s.last_ic[s.fit.ipar[i]];
  }

 print_fit_info(s);
 xpp::log(XPP_LOG_INFO, " Running the fit...\n");
 auto ok=run_fit(s,s.fit.file.c_str(), s.fit.npts,s.fit.npars,s.fit.nvars,s.fit.maxiter,s.fit.dim,
         s.fit.eps,s.fit.tol,
	 s.fit.ipar.data(),s.fit.ivar.data(),s.fit.icols.data(),
	 y0.data(),a.data(),yfit);

   if(!ok){
     ping();
     xpp::show_error(ok.error());
     return;
   }
   bottom_msg(0,*ok==FitEnd::Converged ? " Success! " : "Max iterations exceeded...");

 /* get the latest par values ...  */
 
 for(i=0;i<npars;i++){
   if(s.fit.ipar[i]<0)
     s.parser.constants[-s.fit.ipar[i]]=a[i];
   else
     s.last_ic[s.fit.ipar[i]]=a[i];
 }

}

xpp::Result<FitEnd> run_fit(xpp::Session &s, const char *filename, int npts, int npars, int nvars, int maxiter, int ndim, double eps, double tol, int *ipar, int *ivar, int *icols, double *y0, double *a, double *yfit)
/* 
   filename is where the data file is -- it is of the form:
   t1 y11 y12 .... y1m
   t2 ....
   ...
   tn yn1 ....     ynm
   icols gives the dependent variable columns -- we assume first col 
   is the times
   ndim is the number of y-pts in the a row  
   
*/
{
  int i,j,k,ictrl=0;
  xpp::Result<> ok;
  int niter=0,good_flag=0;
  double tol10=10*tol;
  double sig[MAXODE];
  double chisq=0.0,ochisq=0.0,alambda=0.0;

  xpp::Result<xpp::DataTable> read=xpp::read_data_table(filename);
  if (!read) return std::unexpected(read.error());
  const xpp::DataTable &table=*read;
  if (table.columns.size() < static_cast<std::size_t>(ndim) || table.rows() < static_cast<std::size_t>(npts))
    return xpp::fail("fit", xpp::format("The data file needs {} rows of {} numbers", npts, ndim), xpp::Place{filename, static_cast<int>(table.rows()) + 1});
  for (k=0; k<nvars; k++)
    if (icols[k]<2 || static_cast<std::size_t>(icols[k])>table.columns.size())
      return xpp::fail("fit", xpp::format("The data file has no column {}",icols[k]), xpp::Place{filename,1});
  for (i=0; i<npts; i++)
    for (j=0; j<ndim; j++)
      if (!std::isfinite(table.columns[j][i]))
        return xpp::fail("fit", xpp::format("Invalid data value {}",table.columns[j][i]), xpp::Place{filename,table.line(i)});
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

    ok=marlevstep(s,t0,y0,y,sig,a,npts,nvars,npars,
	       ivar,ipar,covar,alpha,&chisq,&alambda,work,
	       yderv.data(),yfit,&ochisq,ictrl,eps);
    niter++;
    xpp::log(XPP_LOG_INFO, " step {} is {}  -- lambda= {:g}  chisq= {:g} oldchi= {:g}\n",
	   niter,ok.has_value()?1:0,alambda,chisq,ochisq);
    xpp::log(XPP_LOG_INFO, " params: ");
    for(i=0;i<npars;i++)
      xpp::log(XPP_LOG_INFO, " {:g} ",a[i]);
    xpp::log(XPP_LOG_INFO, "\n");
    if(!ok||(niter>=maxiter))break;
    if(ochisq>chisq){
      if(((ochisq-chisq)<tol10)||(((ochisq-chisq)/MAX(1.0,chisq))<tol))
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

  if(!ok)return std::unexpected(ok.error());
  if(niter>=maxiter)return FitEnd::MaxIterations;
  ictrl=2;
  /* the covariance: a singular matrix here is still the fit's error, as
     it was shown before W63d */
  if(auto last=marlevstep(s,t0,y0,y,sig,a,npts,nvars,npars,
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

  return FitEnd::Converged;
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
  int i,j,k,ierr,ipivot[1000];

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
    if(ierr!=-1){
      return xpp::fail("fit","Singular matrix encountered",command_place());
    }
  
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
  static const char *const n[]={"File", "Fitvar","Params","Tolerance","Npts",
		    "NCols","To Col","Params","Epsilon","Max iter"};
  int status;
  std::array<std::string, 10> values;
  values[0] = s.fit.file;
  values[1] = s.fit.varlist;
  values[2] = s.fit.parlist1;
  values[3] = xpp::format("{:g}", s.fit.tol);
  values[4] = xpp::format("{}", s.fit.npts);
  values[5] = xpp::format("{}", s.fit.dim);
  values[6] = s.fit.collist;
  values[7] = s.fit.parlist2;
  values[8] = xpp::format("{:g}", s.fit.eps);
  values[9] = xpp::format("{}", s.fit.maxiter);
  static const int kinds[]={XPP_FIELD_FILE,XPP_FIELD_TEXT,XPP_FIELD_TEXT,XPP_FIELD_NUMBER,XPP_FIELD_INTEGER,
                            XPP_FIELD_INTEGER,XPP_FIELD_TEXT,XPP_FIELD_TEXT,XPP_FIELD_NUMBER,XPP_FIELD_INTEGER};
  status=do_string_box_of(5,2,"Fit",n,values,kinds);
  if(status!=0){
    s.fit.tol=atof(values[3].c_str());
    s.fit.npts=atoi(values[4].c_str());
    s.fit.dim=atoi(values[5].c_str());
    s.fit.eps=atof(values[8].c_str());
    s.fit.maxiter=atoi(values[9].c_str());
    s.fit.file=values[0];
    s.fit.varlist=values[1];
    s.fit.parlist1=values[2];
    s.fit.collist=values[6];
    s.fit.parlist2=values[7];
     return(1);
  }
  return(0);
}

/* gets a list of the data columns to use ... */

void parse_collist(std::string_view collist, int *icols, int *n)
{
  xpp::Tokens tokens(collist);
  int i=0;
  for(std::optional<std::string_view> item;(item=tokens.next(" ,"));)
    icols[i++]=atoi(std::string(*item).c_str());
  *n=i;
}

void parse_varlist(const xpp::Session &s, std::string_view varlist, int *ivars, int *n)
{
  xpp::Tokens tokens(varlist);
  int v,i=0;
  for(std::optional<std::string_view> item;(item=tokens.next(" ,"));){
    find_variable(s,*item,&v);
    if(v<=0)return;
    ivars[i++]=v-1;
  }
  *n=i;
}

void parse_parlist(const xpp::Session &s, std::string_view parlist, int *ipars, int *n)
{
  xpp::Tokens tokens(parlist);
  int v,i=0;
  for(std::optional<std::string_view> item;(item=tokens.next(" ,"));){
    find_variable(s,*item,&v);
    if(v>0){
      ipars[i+*n]=v-1;
      i++;
    }
    else {
      v=get_param_index(s,*item);
      if(v<=0)return;
      ipars[i+*n]=-v;
      i++;
    }
  }
  *n=*n+i;
}

} // namespace xpp
