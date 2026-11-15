#include "model_failed.h"
#include "storage.h"
#include "xpp_ui.h"
#include "markov.h"
#include "xpp_log.h"

#include "integrate.h"
#include "my_rhs.h"
#include "getvar.h"

#include <stdlib.h> 

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
#include "run_parts.h"
#include "xpp_text.h"

namespace xpp {



namespace {

/* an Ornstein-Uhlenbeck process's tau and sigma now (a parameter's current
   value); false, with the run's step error set, when they are not a
   positive tau and a sigma of at least 0 (a parameter edited to one) */
bool ou_scales(xpp::RunParts &s, const xpp::Model::OuProcess &o, double t, double &tau, double &sigma)
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
void start_ou(xpp::RunParts &s)
{
  for(const xpp::Model::OuProcess &o : s.model().ou){
    double tau=0,sigma=0;
    double value=0;
    if(ou_scales(s,o,s.numerics->start_time,tau,sigma))
      value=o.init?*o.init:s.random.normal(0.0,sigma*sqrt(tau/2));
    s.parser.constants[o.slot]=value;
  }
}

void set_wieners(xpp::RunParts &s, double dt, double *x, double t)
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

void update_markov(xpp::RunParts &s, double *x, double t, double dt)
{
  int i;
  if(s.model().nmarkov==0)return;
  setvar(s.parser,0,t);
  for(i=0;i<s.model().node;i++)setvar(s.parser,i+1,x[i]);
  for(i=s.model().node+s.model().fix_var;i<s.model().node+s.model().fix_var+s.model().nmarkov;i++)setvar(s.parser,i+1,x[i-s.model().fix_var]);
  fix_only(s);
  /* every chain moves with the others' variables at their old values (the
     transition formulas read them), so the variables are set after; a
     chain reads of x only its own state, so x itself holds the new ones */
  for(i=0;i<s.model().nmarkov;i++)
    x[s.model().node+i]=new_state(s,x[s.model().node+i],i,dt);
  for(i=0;i<s.model().nmarkov;i++)
    setvar(s.parser,i+s.model().node+s.model().fix_var+1,x[s.model().node+i]);
}

double new_state(xpp::RunParts &s, double old, int index, double dt)
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
	 prob=evaluate(s.parser,chain.command[rns+i])*dt;
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

void make_gill_nu(xpp::RunParts &s, double *nu,int n,int m,double *v)
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

void one_gill_step(xpp::RunParts &s, int meth,int nrxn,int *rxn,double *v,double *r)
{
  double rate=0,test;
  int i;
  switch(meth){
  case 0: /* std gillespie method */
    for(i=0;i<nrxn;i++){
      v[i+1]=0.0;
      r[i]=getvar(s.parser,rxn[i]);
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

Result<> set_random_seed(xpp::RunParts &s, int seed)
{
  if(seed<0)return fail("seed",xpp::format("The seed is a whole number of at least 0, not {}",seed),command_place());
  s.numerics->rand_seed=seed;
  s.random.seed(seed);
  return {};
}

} // namespace xpp
