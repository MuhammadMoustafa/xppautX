#include "xpp_batch.h"
#include "session.h"
#include "storage.h"
#include "xpp_ui.h"
#include "markov.h"
#include "xpp_log.h"

#include "integrate.h"
#include "do_fit.h"
#include "my_rhs.h"

#include <stdlib.h> 
#include "adj2.h"
#include "histogram.h"
#include "browse.h"

#include <strings.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "parserslow.h"
#include "form_ode.h"
#include "load_eqn.h"
#include <string>
#include <vector>
#include "model.h"



void add_wiener(int index)
{
  xpp::model().wiener[xpp::model().nwiener]=index;
  xpp::model().nwiener++;
}

void set_wieners(double dt, double *x, double t)
{
  int i;
  update_markov(x,t,fabs(dt));
  for(i=0;i<xpp::model().nwiener;i++)
    xpp::session().parser.constants[xpp::model().wiener[i]]=normal(0.00,1.00)/sqrt(fabs(dt));
}

void add_markov(int nstate, const char *name)
{
  double st[50];
  int i;
  for(i=0;i<50;i++)st[i]=static_cast<double>(i);
  create_markov(nstate,st,0,name);
}

/* file-local: builds the "{...}" substring extracted from a line, used
   only by build_markov/old_build_markov below */
static std::string extract_expr(const char *source, int *i0);

/* the Markov variable a transition table names (the longest name that
   is a prefix of it); a model naming none fails to load. Writes the
   table's header line when converting. */
static int markov_named(const char *name)
{
  int len=0,index=-1;
  for(int i=0;i<xpp::model().nmarkov;i++){
    int ll=static_cast<int>(xpp::model().markov[i].name.size());
    if(strncasecmp(name,xpp::model().markov[i].name.c_str(),ll)==0&&len<ll){
      index=i;
      len=ll;
    }
  }
  if(index==-1){
    xpp_log(XPP_LOG_ERROR, " Markov variable |%s| not found \n",name);
    xpp_model_failed();
  }
  if(ConvertStyle)
    xpp::print(convertf,"markov {} {}\n", name, xpp::model().markov[index].nstates);
  return index;
}

int build_markov(const char *const *ma, const char *name)
{
  int istart;

 int i,j;
 int index=markov_named(name);
 int nstates=xpp::model().markov[index].nstates;
 xpp_log(XPP_LOG_INFO, " Building %s %d states...\n",name,nstates);
 for(i=0;i<nstates;i++){
   std::string line = ma[i];
   if(ConvertStyle)
     xpp::print(convertf,"{}",line);
   istart=0;
     for(j=0;j<nstates;j++){
       std::string expr = extract_expr(line.c_str(),&istart);
       xpp_log(XPP_LOG_INFO, "%s ",expr.c_str());
       add_markov_entry(index,i,j,expr.c_str());
     }
   xpp_log(XPP_LOG_INFO, "\n");
 }
 return index;
}

int old_build_markov(FILE *fptr, const char *name)
{
  int istart;

 int i,j;
 int index=markov_named(name);
 int nstates=xpp::model().markov[index].nstates;
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
       std::string expr = extract_expr(line.c_str(),&istart);
       xpp_log(XPP_LOG_INFO, "%s ",expr.c_str());
       add_markov_entry(index,i,j,expr.c_str());
     }
   xpp_log(XPP_LOG_INFO, "\n");
   }
 }
 return index;
}
  
static std::string extract_expr(const char *source, int *i0)
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

void create_markov(int nstates, double *st, int type, const char *name)
{
  int n2=nstates*nstates;
  int j=xpp::model().nmarkov;
  if(j>=MAXMARK){
    xpp_log(XPP_LOG_ERROR, "Too many Markov chains...\n");
    xpp_model_failed();
  }

  xpp::model().markov[j].nstates=nstates;
  xpp::model().markov[j].states.assign(st, st+nstates);
  if(type==0){
    xpp::model().markov[j].trans.assign(n2, std::string());
    xpp::model().markov[j].command.assign(n2, std::vector<int>());
  }
  else {
    xpp::model().markov[j].fixed.assign(n2, 0.0);
  }
  /* std::string::substr keeps the same XPP_NAME_MAX truncation the old
     fixed char[XPP_NAME_MAX+1] buffer's snprintf enforced. */
  xpp::model().markov[j].name = std::string(name).substr(0, XPP_NAME_MAX);
  xpp::model().nmarkov++;

}

void add_markov_entry(int index, int j, int k, const char *expr)
{
  
  int l0=xpp::model().markov[index].nstates*j+k;
  int type=xpp::model().markov[index].type;
  if(type==0){
  xpp::model().markov[index].trans[l0]=expr;
  /*  compilation step -- can be delayed */
  /*  end of compilation   */
  
}
  else {
    xpp::model().markov[index].fixed[l0]=atof(expr);
  }
}

void compile_all_markov()
{
  int index,j,k,ns,l0;
  if(xpp::model().nmarkov==0)return;
  for(index=0;index<xpp::model().nmarkov;index++){
    ns=xpp::model().markov[index].nstates;
    for(j=0;j<ns;j++){
      for(k=0;k<ns;k++){
	l0=ns*j+k;
	if(compile_markov(index,j,k)==-1){
	  xpp_log(XPP_LOG_ERROR, "Bad expression %s[%d][%d] = %s \n",
		 xpp::model().markov[index].name.c_str(), j,k,xpp::model().markov[index].trans[l0].c_str());
	  xpp_model_failed();
	}
      }
    }
  }
}

int compile_markov(int index, int j, int k)
{
  const char *expr;
  int l0=xpp::model().markov[index].nstates*j+k,leng;
  int com[256];
  expr=xpp::model().markov[index].trans[l0].c_str();

  if(add_expr(expr,com,&leng))
    return -1;
  /* zero-padded by two, like the xpp_malloc block it replaces */
  xpp::model().markov[index].command[l0].assign(com, com+leng);
  xpp::model().markov[index].command[l0].resize(leng+2, 0);
  
  return 1;
}

void update_markov(double *x, double t, double dt)
{
  int i;
  double yp[MAXODE];
  if(xpp::model().nmarkov==0)return;
  set_ivar(0,t);
  for(i=0;i<xpp::model().node;i++)set_ivar(i+1,x[i]);
  for(i=xpp::model().node+xpp::model().fix_var;i<xpp::model().node+xpp::model().fix_var+xpp::model().nmarkov;i++)set_ivar(i+1,x[i-xpp::model().fix_var]);
  for(i=xpp::model().node;i<xpp::model().node+xpp::model().fix_var;i++)
  set_ivar(i+1,evaluate(xpp::model().programs[i].data()));
  for(i=0;i<xpp::model().nmarkov;i++)
    yp[i]=new_state(x[xpp::model().node+i],i,dt);
  for(i=0;i<xpp::model().nmarkov;i++){
    x[xpp::model().node+i]=yp[i];
    set_ivar(i+xpp::model().node+xpp::model().fix_var+1,yp[i]);
  }
}

double new_state(double old, int index, double dt)
{
  double prob,sum;
  double coin=ndrand48();
  int row=-1,rns;
  double *st;
  xpp::Model::MarkovChain &chain=xpp::model().markov[index];
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
	 prob=evaluate(chain.command[rns+i].data())*dt;
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

void make_gill_nu(double *nu,int n,int m,double *v)
{
  /* nu[j+m*i] = nu_{i,j} i=1,n-1 -- assume first eqn is tr'=tr+z(0)
     i species j reaction
    need this for improved tau stepper
   */
  int ir,iy;

  std::vector<double> y_buf(n, 0.0), yold_buf(n, 0.0), yp_buf(n, 0.0);
  double *y=y_buf.data(), *yp=yp_buf.data(), *yold=yold_buf.data();
  for(ir=0;ir<m;ir++)
    v[ir+1]=0;
  rhs_only(y,yold);
  for(ir=0;ir<m;ir++){
    v[ir+1]=1;
    rhs_only(y,yp);
    for(iy=0;iy<n;iy++){
      nu[ir+m*iy]=yp[iy];
      xpp_log(XPP_LOG_DEBUG, "ir=%d iy=%d nu=%g\n",ir+1,iy,yp[iy]-yold[iy]);
    }
    v[ir+1]=0;
  }
}

void one_gill_step(int meth,int nrxn,int *rxn,double *v)
{
  double rate=0,test;
  double r[1000];
  /*double rold[1000]; Not used*/
 
  int i;
  switch(meth){
  case 0: /* std gillespie method */
    for(i=0;i<nrxn;i++){
      v[i+1]=0.0;
      r[i]=get_ivar(rxn[i]);
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

void do_stochast_com(int i)
{
  static const char *const key="ncdmvhofpislaxe2";
  char ch=key[i];
  
  if(ch==27)return;
  switch(ch){
  case 'n': 
    new_int("Seed:",&xpp::session().numerics.rand_seed);
    nsrand48(xpp::session().numerics.rand_seed);
    break;
  case 'd':
    data_back();
    break;
  case 'm':
    mean_back();
    break;
  case 'v':
    variance_back();
    break;
  case 'c':
    compute_em();
    xpp::session().stochastic.flag=0;
    break;
  case 'h':
    compute_hist();
    break;
  case 'o':
    hist_back();
    break;
  case 'f':
    compute_fourier(); 
    break;
  case 'p':
    compute_power();
    break;
  case 'i':
    test_fit();
    redraw_params();
    redraw_ics();
    break;
  case 's':
    column_mean();
    break;
  case 'l':
    do_liapunov();
    break;
  case 'a':
     compute_stacor();
     break;
  case 'x':
    compute_correl();
    break;
  case 'e':
    compute_sd();
    break;
  case '2':
    new_2d_hist();
    break;
  }
  
}

/* show the mean or the variance of the runs in the browser */
static void stats_back(float **stats)
{
  if(xpp::session().stochastic.here){
    new_browse_dat(stats,xpp::session().stochastic.len);
    xpp::session().data_store.rows=xpp::session().stochastic.len;
  }
}

void mean_back()
{
  stats_back(xpp::session().stochastic.mean.data());
}

void variance_back()
{
  stats_back(xpp::session().stochastic.variance.data());
}

void compute_em()
{
  double *x;
  x=&xpp::session().data_store.current[0];
  free_stoch();
  xpp::session().stochastic.flag=1;
  do_range(x,0);
  redraw_ics();
}

void free_stoch()
{
  int i;
  if(xpp::session().stochastic.here){
    data_back();
    for(i=0;i<(xpp::model().neq+1);i++){
      xpp::session().stochastic.mean_rows[i]=std::vector<float>();
      xpp::session().stochastic.variance_rows[i]=std::vector<float>();
      xpp::session().stochastic.mean[i]=xpp::session().stochastic.variance[i]=nullptr;
    }
    xpp::session().stochastic.here=0;
  }
}

void init_stoch(int len)
{
  int i,j;
  xpp::session().stochastic.n_trials=0;
  xpp::session().stochastic.len=len;
  for(i=0;i<(xpp::model().neq+1);i++){
    xpp::session().stochastic.mean_rows[i].assign(xpp::session().stochastic.len,0.0f);
    xpp::session().stochastic.variance_rows[i].assign(xpp::session().stochastic.len,0.0f);
    xpp::session().stochastic.mean[i]=xpp::session().stochastic.mean_rows[i].data();
    xpp::session().stochastic.variance[i]=xpp::session().stochastic.variance_rows[i].data();
  }
  for(j=0;j<xpp::session().stochastic.len;j++){
    xpp::session().stochastic.mean[0][j]=xpp::session().data_store.col[0][j];
    xpp::session().stochastic.variance[0][j]=xpp::session().data_store.col[0][j];
  }
  xpp::session().stochastic.here=1;
}

void append_stoch(int first, int length)
{
  int i,j;
  float z;
  if(first==0)init_stoch(length);
  if(length!=xpp::session().stochastic.len|| !xpp::session().stochastic.here)return;
  for(i=0;i<xpp::session().stochastic.len;i++){
      for(j=1;j<=xpp::model().neq;j++){
	z=xpp::session().data_store.col[j][i];
	xpp::session().stochastic.mean[j][i]=xpp::session().stochastic.mean[j][i]+z;
	xpp::session().stochastic.variance[j][i]=xpp::session().stochastic.variance[j][i]+z*z;
      }
    }
  xpp::session().stochastic.n_trials++;
}

void do_stats(int ierr)
{
  int i,j;
  float ninv,mean;
  if(ierr!=-1&&xpp::session().stochastic.n_trials>0){
    ninv=1./static_cast<float>(xpp::session().stochastic.n_trials);
    for(i=0;i<xpp::session().stochastic.len;i++){
      for(j=1;j<=xpp::model().neq;j++){
	mean=xpp::session().stochastic.mean[j][i]*ninv;
	xpp::session().stochastic.mean[j][i]=mean;
	xpp::session().stochastic.variance[j][i]=(xpp::session().stochastic.variance[j][i]*ninv-mean*mean);
      }
    }
 
  }
}
