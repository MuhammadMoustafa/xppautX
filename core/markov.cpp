#include "xpp_batch.h"
#include "session.h"
#include "storage.h"
#include "xpp_ui.h"
#include "markov.h"
#include "xpp_log.h"

#include "integrate.h"
#include "do_fit.h"
#include "my_rhs.h"
#include "getvar.h"

#include <stdlib.h> 
#include "adj2.h"
#include "histogram.h"
#include "browse.h"

#include <strings.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "expr.h"
#include "form_ode.h"
#include "load_eqn.h"
#include <string>
#include <vector>
#include "model.h"



void add_wiener(xpp::Session &s, int index)
{
  s.model().wiener[s.model().nwiener]=index;
  s.model().nwiener++;
}

void set_wieners(xpp::Session &s, double dt, double *x, double t)
{
  int i;
  update_markov(s,x,t,fabs(dt));
  for(i=0;i<s.model().nwiener;i++)
    s.parser.constants[s.model().wiener[i]]=normal(0.00,1.00)/sqrt(fabs(dt));
}

void add_markov(xpp::Session &s, int nstate, const char *name)
{
  double st[50];
  int i;
  for(i=0;i<50;i++)st[i]=static_cast<double>(i);
  create_markov(s,nstate,st,0,name);
}

/* the Markov variable a transition table names (the longest name that
   is a prefix of it); a model naming none fails to load. Writes the
   table's header line when converting. */
static int markov_named(xpp::Session &s, const char *name)
{
  int len=0,index=-1;
  for(int i=0;i<s.model().nmarkov;i++){
    int ll=static_cast<int>(s.model().markov[i].name.size());
    if(strncasecmp(name,s.model().markov[i].name.c_str(),ll)==0&&len<ll){
      index=i;
      len=ll;
    }
  }
  if(index==-1){
    xpp_log(XPP_LOG_ERROR, " Markov variable |%s| not found \n",name);
    xpp_model_failed();
  }
  if(ConvertStyle)
    xpp::print(convertf,"markov {} {}\n", name, s.model().markov[index].nstates);
  return index;
}

int build_markov(xpp::Session &s, std::span<const std::string> cells, const char *name)
{
 int index=markov_named(s,name);
 int nstates=s.model().markov[index].nstates;
 xpp_log(XPP_LOG_INFO, " Building %s %d states...\n",name,nstates);
 for(int i=0;i<nstates;i++){
   for(int j=0;j<nstates;j++){
     const std::string &expr=cells[static_cast<size_t>(i*nstates+j)];
     xpp_log(XPP_LOG_INFO, "%s ",expr.c_str());
     add_markov_entry(s,index,i,j,expr.c_str());
   }
   xpp_log(XPP_LOG_INFO, "\n");
 }
 return index;
}

int old_build_markov(xpp::Session &s, FILE *fptr, const char *name)
{
  int istart;

 int i,j;
 int index=markov_named(s,name);
 int nstates=s.model().markov[index].nstates;
 xpp_log(XPP_LOG_INFO, " Building %s ...\n",name);
 {
   /* a whole line at a time, no 256-byte fgets cut, wrapping the FILE*
      the caller keeps owning */
   xpp::LineReader reader = xpp::LineReader::attach(fptr);
   for(i=0;i<nstates;i++){
    auto line_view = reader.next();
    if(!line_view){
      xpp_log(XPP_LOG_ERROR, " Unexpected end of file building markov variable |%s|\n",name);
      xpp_model_failed();
    }
    std::string line(*line_view);

   if(ConvertStyle){
     /* LineReader strips the terminator fgets used to keep; restore it
        so the converted file's line breaks match exactly. */
     xpp::print(convertf,"{}\n",line);
   }
   istart=0;
     for(j=0;j<nstates;j++){
       std::string expr = markov_cell(line.c_str(),&istart);
       xpp_log(XPP_LOG_INFO, "%s ",expr.c_str());
       add_markov_entry(s,index,i,j,expr.c_str());
     }
   xpp_log(XPP_LOG_INFO, "\n");
   }
 }
 return index;
}
  
std::string markov_cell(const char *source, int *i0)
{
 std::string dest;
 char ch;
 int flag=0;
 while(1)
   {
     ch=source[*i0];
     *i0=*i0+1;
     if(ch=='}')break;
     if(ch=='{')flag=1;
     else {
       if(flag){
	 dest.push_back(ch);
       }
     }
   }
   return dest;
}

void create_markov(xpp::Session &s, int nstates, double *st, int type, const char *name)
{
  int n2=nstates*nstates;
  int j=s.model().nmarkov;
  if(j>=MAXMARK){
    xpp_log(XPP_LOG_ERROR, "Too many Markov chains...\n");
    xpp_model_failed();
  }

  s.model().markov[j].nstates=nstates;
  s.model().markov[j].states.assign(st, st+nstates);
  if(type==0){
    s.model().markov[j].trans.assign(n2, std::string());
    s.model().markov[j].command.assign(n2, std::vector<int>());
  }
  else {
    s.model().markov[j].fixed.assign(n2, 0.0);
  }
  s.model().markov[j].name = name;
  s.model().nmarkov++;

}

void add_markov_entry(xpp::Session &s, int index, int j, int k, const char *expr)
{
  
  int l0=s.model().markov[index].nstates*j+k;
  int type=s.model().markov[index].type;
  if(type==0){
  s.model().markov[index].trans[l0]=expr;
  /*  compilation step -- can be delayed */
  /*  end of compilation   */
  
}
  else {
    s.model().markov[index].fixed[l0]=atof(expr);
  }
}

void compile_all_markov(xpp::Session &s)
{
  int index,j,k,ns,l0;
  if(s.model().nmarkov==0)return;
  for(index=0;index<s.model().nmarkov;index++){
    ns=s.model().markov[index].nstates;
    for(j=0;j<ns;j++){
      for(k=0;k<ns;k++){
	l0=ns*j+k;
	if(compile_markov(s,index,j,k)==-1){
	  xpp_log(XPP_LOG_ERROR, "Bad expression %s[%d][%d] = %s \n",
		 s.model().markov[index].name.c_str(), j,k,s.model().markov[index].trans[l0].c_str());
	  xpp_model_failed();
	}
      }
    }
  }
}

int compile_markov(xpp::Session &s, int index, int j, int k)
{
  const char *expr;
  int l0=s.model().markov[index].nstates*j+k,leng;
  int com[256];
  expr=s.model().markov[index].trans[l0].c_str();

  if(add_expr(s,expr,com,&leng))
    return -1;
  /* zero-padded by two, like the xpp_malloc block it replaces */
  s.model().markov[index].command[l0].assign(com, com+leng);
  s.model().markov[index].command[l0].resize(leng+2, 0);
  
  return 1;
}

void update_markov(xpp::Session &s, double *x, double t, double dt)
{
  int i;
  double yp[MAXODE];
  if(s.model().nmarkov==0)return;
  setvar(s,0,t);
  for(i=0;i<s.model().node;i++)setvar(s,i+1,x[i]);
  for(i=s.model().node+s.model().fix_var;i<s.model().node+s.model().fix_var+s.model().nmarkov;i++)setvar(s,i+1,x[i-s.model().fix_var]);
  for(i=s.model().node;i<s.model().node+s.model().fix_var;i++)
  setvar(s,i+1,evaluate(s,s.model().programs[i].data()));
  for(i=0;i<s.model().nmarkov;i++)
    yp[i]=new_state(s,x[s.model().node+i],i,dt);
  for(i=0;i<s.model().nmarkov;i++){
    x[s.model().node+i]=yp[i];
    setvar(s,i+s.model().node+s.model().fix_var+1,yp[i]);
  }
}

double new_state(xpp::Session &s, double old, int index, double dt)
{
  double prob,sum;
  double coin=ndrand48();
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
	 prob=evaluate(s,chain.command[rns+i].data())*dt;
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
      xpp_log(XPP_LOG_DEBUG, "ir=%d iy=%d nu=%g\n",ir+1,iy,yp[iy]-yold[iy]);
    }
    v[ir+1]=0;
  }
}

void one_gill_step(const xpp::Session &s, int meth,int nrxn,int *rxn,double *v)
{
  double rate=0,test;
  double r[1000];
  /*double rold[1000]; Not used*/
 
  int i;
  switch(meth){
  case 0: /* std gillespie method */
    for(i=0;i<nrxn;i++){
      v[i+1]=0.0;
      r[i]=getvar(s,rxn[i]);
      rate+=r[i];
    }
    if(rate<=0.0)return;
    v[0]=-log(ndrand48())/rate; /* next step */
    test=rate*ndrand48();
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

void do_stochast_com(xpp::Session &s, int i)
{
  static const char *const key="ncdmvhofpislaxe2";
  char ch=key[i];
  
  if(ch==27)return;
  switch(ch){
  case 'n': 
    new_int("Seed:",&s.numerics.rand_seed);
    nsrand48(s.numerics.rand_seed);
    break;
  case 'd':
    data_back(s);
    break;
  case 'm':
    mean_back(s);
    break;
  case 'v':
    variance_back(s);
    break;
  case 'c':
    compute_em(s);
    s.stochastic.flag=0;
    break;
  case 'h':
    compute_hist(s);
    break;
  case 'o':
    hist_back(s);
    break;
  case 'f':
    compute_fourier(s); 
    break;
  case 'p':
    compute_power(s);
    break;
  case 'i':
    test_fit(s);
    redraw_params();
    redraw_ics();
    break;
  case 's':
    column_mean(s);
    break;
  case 'l':
    do_liapunov(s);
    break;
  case 'a':
     compute_stacor(s);
     break;
  case 'x':
    compute_correl(s);
    break;
  case 'e':
    compute_sd(s);
    break;
  case '2':
    new_2d_hist(s);
    break;
  }
  
}

/* show the mean or the variance of the runs in the browser */
static void stats_back(xpp::Session &s, float **stats)
{
  if(s.stochastic.here){
    new_browse_dat(s,stats,s.stochastic.len);
    s.data_store.rows=s.stochastic.len;
  }
}

void mean_back(xpp::Session &s)
{
  stats_back(s,s.stochastic.mean.data());
}

void variance_back(xpp::Session &s)
{
  stats_back(s,s.stochastic.variance.data());
}

void compute_em(xpp::Session &s)
{
  double *x;
  x=&s.data_store.current[0];
  free_stoch(s);
  s.stochastic.flag=1;
  do_range(s,x,0);
  redraw_ics();
}

void free_stoch(xpp::Session &s)
{
  int i;
  if(s.stochastic.here){
    data_back(s);
    for(i=0;i<(s.model().neq+1);i++){
      s.stochastic.mean_rows[i]=std::vector<float>();
      s.stochastic.variance_rows[i]=std::vector<float>();
      s.stochastic.mean[i]=s.stochastic.variance[i]=nullptr;
    }
    s.stochastic.here=0;
  }
}

void init_stoch(xpp::Session &s, int len)
{
  int i,j;
  s.stochastic.n_trials=0;
  s.stochastic.len=len;
  for(i=0;i<(s.model().neq+1);i++){
    s.stochastic.mean_rows[i].assign(s.stochastic.len,0.0f);
    s.stochastic.variance_rows[i].assign(s.stochastic.len,0.0f);
    s.stochastic.mean[i]=s.stochastic.mean_rows[i].data();
    s.stochastic.variance[i]=s.stochastic.variance_rows[i].data();
  }
  for(j=0;j<s.stochastic.len;j++){
    s.stochastic.mean[0][j]=s.data_store.col[0][j];
    s.stochastic.variance[0][j]=s.data_store.col[0][j];
  }
  s.stochastic.here=1;
}

void append_stoch(xpp::Session &s, int first, int length)
{
  int i,j;
  float z;
  if(first==0)init_stoch(s,length);
  if(length!=s.stochastic.len|| !s.stochastic.here)return;
  for(i=0;i<s.stochastic.len;i++){
      for(j=1;j<=s.model().neq;j++){
	z=s.data_store.col[j][i];
	s.stochastic.mean[j][i]=s.stochastic.mean[j][i]+z;
	s.stochastic.variance[j][i]=s.stochastic.variance[j][i]+z*z;
      }
    }
  s.stochastic.n_trials++;
}

void do_stats(xpp::Session &s, int ierr)
{
  int i,j;
  float ninv,mean;
  if(ierr!=-1&&s.stochastic.n_trials>0){
    ninv=1./static_cast<float>(s.stochastic.n_trials);
    for(i=0;i<s.stochastic.len;i++){
      for(j=1;j<=s.model().neq;j++){
	mean=s.stochastic.mean[j][i]*ninv;
	s.stochastic.mean[j][i]=mean;
	s.stochastic.variance[j][i]=(s.stochastic.variance[j][i]*ninv-mean*mean);
      }
    }
 
  }
}
