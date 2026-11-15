#include "stochastic.h"

#include "adj2.h"
#include "browse.h"
#include "integrate.h"
#include "model.h"
#include "runs.h"
#include "session.h"
#include "xpp_ui.h"

namespace xpp {

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
  if(auto swept=run_range(s, &done, report);!swept)return std::unexpected(swept.error());
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
