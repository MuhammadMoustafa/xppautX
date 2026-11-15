#include "model_failed.h"
#include "storage.h"
#include "xpp_ui.h"
#include "markov.h"
#include "xpp_log.h"

#include "integrate.h"
#include "my_rhs.h"
#include "getvar.h"

#include <stdlib.h> 
#include "adj2.h"
#include "browse.h"

#include <strings.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "expr.h"
#include <string>
#include <vector>
#include <cmath>
#include <expected>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <string_view>
#include <unordered_map>
#include <utility>
#include "model.h"
#include "model_record.h"
#include "xpp_math.h"
#include "expr_program.h"
#include "model_place.h"
#include "odex.h"
#include "session.h"
#include "xpp_text.h"

namespace xpp {



namespace {

/* an Ornstein-Uhlenbeck process's tau and sigma now (a parameter's current
   value); false, with the run's step error set, when they are not a
   positive tau and a sigma of at least 0 (a parameter edited to one) */
bool ou_scales(xpp::Session &s, const xpp::Model::OuProcess &o, double t, double &tau, double &sigma)
{
  const auto read=[&s](const xpp::Model::OuValue &v){return v.constant>=0?s.parser.constants[v.constant]:v.number;};
  tau=read(o.tau);
  sigma=read(o.sigma);
  if(tau>0&&std::isfinite(tau)&&sigma>=0&&std::isfinite(sigma))return true;
  if(!s.parser.step_error){
    const bool bad_tau=!(tau>0&&std::isfinite(tau));
    s.parser.step_error=xpp::Error{"integration",
      xpp::format("{} at t = {}: {} = {}, {}",o.name,xpp::number(t),bad_tau?"tau":"sigma",xpp::number(bad_tau?tau:sigma),
                  bad_tau?"but the correlation time must be positive":"but the standard deviation must be at least 0"),
      xpp::model_place(s.model(),o.name)};
  }
  return false;
}

}

/* a run's start: each process at its init, else a draw from its stationary
   distribution, N(0, sigma^2 tau/2) */
void start_ou(xpp::Session &s)
{
  for(const xpp::Model::OuProcess &o : s.model().ou){
    double tau=0,sigma=0;
    double value=0;
    if(ou_scales(s,o,s.numerics->start_time,tau,sigma))
      value=o.init?*o.init:s.random.normal(0.0,sigma*sqrt(tau/2));
    s.parser.constants[o.slot]=value;
  }
}

void set_wieners(xpp::Session &s, double dt, double *x, double t)
{
  int i;
  update_markov(s,x,t,fabs(dt));
  for(i=0;i<s.model().nwiener;i++)
    s.parser.constants[s.model().wiener[i]]=s.random.normal(0.00,1.00)/sqrt(fabs(dt));
  /* the exact update over |dt| (the stationary process is the same run
     backwards): eta e^(-dt/tau) + sigma sqrt(tau/2 (1 - e^(-2dt/tau))) N(0,1) */
  for(const xpp::Model::OuProcess &o : s.model().ou){
    double tau=0,sigma=0;
    if(!ou_scales(s,o,t,tau,sigma))continue;
    const double decay=xpp::math::exp(-fabs(dt)/tau);
    double &eta=s.parser.constants[o.slot];
    eta=eta*decay+s.random.normal(0.0,sigma*sqrt(tau/2*(1-decay*decay)));
  }
}

void update_markov(xpp::Session &s, double *x, double t, double dt)
{
  int i;
  if(s.model().nmarkov==0)return;
  setvar(s,0,t);
  for(i=0;i<s.model().node;i++)setvar(s,i+1,x[i]);
  for(i=s.model().node+s.model().fix_var;i<s.model().node+s.model().fix_var+s.model().nmarkov;i++)setvar(s,i+1,x[i-s.model().fix_var]);
  fix_only(s);
  /* every chain moves with the others' variables at their old values (the
     transition formulas read them), so the variables are set after; a
     chain reads of x only its own state, so x itself holds the new ones */
  for(i=0;i<s.model().nmarkov;i++)
    x[s.model().node+i]=new_state(s,x[s.model().node+i],i,dt);
  for(i=0;i<s.model().nmarkov;i++)
    setvar(s,i+s.model().node+s.model().fix_var+1,x[s.model().node+i]);
}

double new_state(xpp::Session &s, double old, int index, double dt)
{
  double prob,sum;
  double coin=s.random.uniform();
  int row=-1,rns;
  double *st;
  xpp::Model::MarkovChain &chain=s.model().markov[index];
  int i,ns=chain.nstates;
  int type=chain.type;
  st=chain.states.data();
  for(i=0;i<ns;i++)
    if(fabs(st[i]-old)<.0001){
      row=i;
      break;
    }
  if(row==-1)return(old);
  rns=row*ns;
  sum=0.0;
   if(type==0){
     for(i=0;i<ns;i++){
       if(i!=row){
	 prob=evaluate(s,chain.command[rns+i])*dt;
	 sum=sum+prob;
	 if(coin<=sum){
	   return(st[i]);
	 }
       }
     }
   }
   else{
     for(i=0;i<ns;i++){
       if(i!=row){
	 prob=chain.fixed[rns+i]*dt;
	 sum=sum+prob;
	 if(coin<=sum){
	   return(st[i]);
	 }
       }
     }
   }
     
  return(old);
}

void make_gill_nu(xpp::Session &s, double *nu,int n,int m,double *v)
{
  /* nu[j+m*i] = nu_{i,j} i=1,n-1 -- assume first eqn is tr'=tr+z(0)
     i species j reaction
    need this for improved tau stepper
   */
  int ir,iy;

  std::vector<double> yold_buf(n, 0.0), yp_buf(n, 0.0);
  double *yp=yp_buf.data(), *yold=yold_buf.data();
  for(ir=0;ir<m;ir++)
    v[ir+1]=0;
  rhs_only(s,yold);
  for(ir=0;ir<m;ir++){
    v[ir+1]=1;
    rhs_only(s,yp);
    for(iy=0;iy<n;iy++){
      nu[ir+m*iy]=yp[iy];
      xpp::log(XPP_LOG_DEBUG, "ir={:d} iy={:d} nu={:g}\n",ir+1,iy,yp[iy]-yold[iy]);
    }
    v[ir+1]=0;
  }
}

void one_gill_step(xpp::Session &s, int meth,int nrxn,int *rxn,double *v,double *r)
{
  double rate=0,test;
  int i;
  switch(meth){
  case 0: /* std gillespie method */
    for(i=0;i<nrxn;i++){
      v[i+1]=0.0;
      r[i]=getvar(s,rxn[i]);
      rate+=r[i];
    }
    r[nrxn]=0.0;
    if(rate<=0.0)return;
    v[0]=-xpp::math::log(s.random.uniform())/rate; /* next step */
    test=rate*s.random.uniform();
    rate=r[0];
    for(i=0;i<nrxn;i++){
      if(test<rate){
	v[i+1]=1.0;
	break;
      }
      rate+=r[i+1];
    }
    break;
  case 1: /* tau stepping method  */
    perror("Tau stepping method not implemented yet.");
    break;

  }

}

Result<> set_random_seed(xpp::Session &s, int seed)
{
  if(seed<0)return fail("seed",xpp::format("The seed is a whole number of at least 0, not {}",seed),command_place());
  s.numerics->rand_seed=seed;
  s.random.seed(seed);
  return {};
}

/* show the mean or the variance of the runs in the browser */
static Result<> stats_back(xpp::Session &s, ColumnTable stats)
{
  if(!s.stochastic->here)
    return fail("stochastic","No stochastic runs yet: run Stochastic Compute first",command_place());
  new_browse_dat(s,stats,s.stochastic->len);
  s.data_store->rows=s.stochastic->len;
  return {};
}

Result<> mean_back(xpp::Session &s)
{
  return stats_back(s,ColumnTable(s.stochastic->mean.data()));
}

Result<> variance_back(xpp::Session &s)
{
  return stats_back(s,ColumnTable(s.stochastic->variance.data()));
}

Result<int> stochastic_integrate(xpp::Session &s, const RunReport &report)
{
  if(auto items=resolve_range_items(s,false);!items)return std::unexpected(items.error());
  if(auto allowed=run_allowed(s,"range");!allowed)return std::unexpected(allowed.error());
  free_stoch(s);
  s.stochastic->flag=true;
  /* the flag is only for this sweep, whatever it ends in */
  struct Unflag {
    xpp::Session &s;
    ~Unflag(){s.stochastic->flag=false;}
  } unflag{s};
  s.integrator->range.rtype=RANGE_SINGLE;
  RangeProgress done;
  if(auto swept=run_range(s,0,&done,report);!swept)return std::unexpected(swept.error());
  redraw_ics();
  return done.runs;
}

void free_stoch(xpp::Session &s)
{
  int i;
  if(s.stochastic->here){
    data_back(s);
    for(i=0;i<(s.model().neq+1);i++){
      s.stochastic->mean_rows[i].clear();
      s.stochastic->variance_rows[i].clear();
      s.stochastic->mean[i]=s.stochastic->variance[i]=Column();
    }
    s.stochastic->here=0;
  }
}

void retype_stochastic(xpp::Session &s)
{
  for(std::size_t i=0;i<s.stochastic->mean_rows.size();i++){
    s.stochastic->mean_rows[i].convert(s.data_store->precision());
    s.stochastic->variance_rows[i].convert(s.data_store->precision());
    s.stochastic->mean[i]=s.stochastic->mean_rows[i].view();
    s.stochastic->variance[i]=s.stochastic->variance_rows[i].view();
  }
}

void init_stoch(xpp::Session &s, int len)
{
  int i,j;
  s.stochastic->n_trials=0;
  s.stochastic->len=len;
  const std::size_t columns=static_cast<std::size_t>(s.model().neq)+1;
  s.stochastic->mean_rows.resize(columns);
  s.stochastic->variance_rows.resize(columns);
  s.stochastic->mean.resize(columns);
  s.stochastic->variance.resize(columns);
  for(i=0;i<(s.model().neq+1);i++){
    s.stochastic->mean_rows[i].assign(s.stochastic->len,s.data_store->precision());
    s.stochastic->variance_rows[i].assign(s.stochastic->len,s.data_store->precision());
    s.stochastic->mean[i]=s.stochastic->mean_rows[i].view();
    s.stochastic->variance[i]=s.stochastic->variance_rows[i].view();
  }
  for(j=0;j<s.stochastic->len;j++){
    s.stochastic->mean[0][j]=s.data_store->col[0][j];
    s.stochastic->variance[0][j]=s.data_store->col[0][j];
  }
  s.stochastic->here=1;
}

void append_stoch(xpp::Session &s, int first, int length)
{
  int i,j;
  double z;
  if(first==0)init_stoch(s,length);
  if(length!=s.stochastic->len|| !s.stochastic->here)return;
  for(i=0;i<s.stochastic->len;i++){
      for(j=1;j<=s.model().neq;j++){
	z=s.data_store->col[j][i];
	s.stochastic->mean[j][i]=s.stochastic->mean[j][i]+z;
	s.stochastic->variance[j][i]=s.stochastic->variance[j][i]+s.data_store->round(z*z);
      }
    }
  s.stochastic->n_trials++;
}

void do_stats(xpp::Session &s, int ierr)
{
  int i,j;
  double ninv,mean;
  if(ierr!=-1&&s.stochastic->n_trials>0){
    ninv=s.data_store->round(1./s.stochastic->n_trials);
    for(i=0;i<s.stochastic->len;i++){
      for(j=1;j<=s.model().neq;j++){
	mean=s.data_store->round(s.stochastic->mean[j][i]*ninv);
	s.stochastic->mean[j][i]=mean;
	s.stochastic->variance[j][i]=s.data_store->round(s.stochastic->variance[j][i]*ninv)-s.data_store->round(mean*mean);
      }
    }
 
  }
}

} // namespace xpp
