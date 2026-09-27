#include "xpp_batch.h"
#include "storage.h"
#include "xpp_ui.h"
#include "markov.h"
#include "xpp_mem.h"
#include "xpp_log.h"
#include "xpp_math.h"

#include "integrate.h"
#include "browse.h"
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
#include "xpplim.h"
#include "parserslow.h"
#include "xpp_io.h"
#include <string>
#include <vector>
/* #include "browse.h" */

extern int ConvertStyle;
extern FILE *convertf;






extern int *my_ode[];
extern char *ode_names[MAXODE];
extern int NMarkov,FIX_VAR,NODE,NEQ;


extern int NLINES;
extern char *save_eqn[1000];
extern int RandSeed;
typedef struct {
  std::vector<std::vector<int>> command; /* compiled transition formulas */
  std::vector<std::string> trans;
  std::vector<double> fixed;
  int nstates;
  std::vector<double> states;
  int type;   /* 0 is default and state dependent.  1 is fixed for all time  */
  std::string name;
} MARKOV;

MARKOV markov[MAXMARK];



/* raw xpp_malloc blocks per row, not std::vector<std::vector<float>>:
   set_browser_data (browse.h) takes a plain float** and expects these
   MAXODE pointers contiguous, so my_mean/my_variance stay this shape. */
float *my_mean[MAXODE],*my_variance[MAXODE];
int stoch_len;

int STOCH_FLAG,STOCH_HERE,N_TRIALS;
int Wiener[MAXPAR];
int NWiener;
extern double constants[];



void add_wiener(int index)
{
  Wiener[NWiener]=index;
  NWiener++;
}

void set_wieners(double dt, double *x, double t)
{
  int i;
  update_markov(x,t,fabs(dt));
  for(i=0;i<NWiener;i++)
    constants[Wiener[i]]=normal(0.00,1.00)/sqrt(fabs(dt));
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
  for(int i=0;i<NMarkov;i++){
    int ll=static_cast<int>(markov[i].name.size());
    if(strncasecmp(name,markov[i].name.c_str(),ll)==0&&len<ll){
      index=i;
      len=ll;
    }
  }
  if(index==-1){
    xpp_log(XPP_LOG_ERROR, " Markov variable |%s| not found \n",name);
    xpp_model_failed();
  }
  if(ConvertStyle){
    std::string _cvt = xpp::format("markov {} {}\n", name, markov[index].nstates);
    fwrite(_cvt.data(), 1, _cvt.size(), convertf);
  }
  return index;
}

int build_markov(const char *const *ma, const char *name)  /*   FILE *fptr; */
{
 /*int nn;
 */
  int istart;


 int i,j;
 int index=markov_named(name);
 int nstates=markov[index].nstates;
 xpp_log(XPP_LOG_INFO, " Building %s %d states...\n",name,nstates);
 for(i=0;i<nstates;i++){
   /* fgets(line,256,fptr); */
   std::string line = ma[i];
   if(ConvertStyle)
     fputs(line.c_str(),convertf);
   /*nn=strlen(line)+1;*/
   /* if((save_eqn[NLINES]=(char *)malloc(nn))==NULL){
     plintf("saveeqn-prob\n");xpp_model_failed();}
     strncpy(save_eqn[NLINES++],line,nn); */
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
 /*int nn;*/
  int istart;


 int i,j;
 int index=markov_named(name);
 int nstates=markov[index].nstates;
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
     fputs(line.c_str(),convertf);
     fputc('\n',convertf);
   }
   /*nn=strlen(line)+1;*/
   /* if((save_eqn[NLINES]=(char *)malloc(nn))==NULL)xpp_model_failed();
      strncpy(save_eqn[NLINES++],line,nn); */
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
  int j=NMarkov;
  if(j>=MAXMARK){
    xpp_log(XPP_LOG_ERROR, "Too many Markov chains...\n");
    xpp_model_failed();
  }

  markov[j].nstates=nstates;
  markov[j].states.assign(st, st+nstates);
  if(type==0){
    markov[j].trans.assign(n2, std::string());
    markov[j].command.assign(n2, std::vector<int>());
  }
  else {
    markov[j].fixed.assign(n2, 0.0);
  }
  /* std::string::substr keeps the same XPP_NAME_MAX truncation the old
     fixed char[XPP_NAME_MAX+1] buffer's snprintf enforced. */
  markov[j].name = std::string(name).substr(0, XPP_NAME_MAX);
  NMarkov++;

  
}

void add_markov_entry(int index, int j, int k, const char *expr)
{
  
  int l0=markov[index].nstates*j+k;
  int type=markov[index].type;
  if(type==0){
  markov[index].trans[l0]=expr;
  /*  compilation step -- can be delayed */
 /*
  if(add_expr(expr,com,&leng)){ 
    plintf("Illegal expression %s\n",expr);
    xpp_model_failed();
  }
  markov[index].command[l0]=(int *)malloc(sizeof(int)*(leng+2));
  for(i=0;i<leng;i++){
    markov[index].command[l0][i]=com[i];
    
  }
 */
  /*  end of compilation   */
  
}
  else {
    markov[index].fixed[l0]=atof(expr);
  }
}

 
void compile_all_markov()
{
  int index,j,k,ns,l0;
  if(NMarkov==0)return;
  for(index=0;index<NMarkov;index++){
    ns=markov[index].nstates;
    for(j=0;j<ns;j++){
      for(k=0;k<ns;k++){
	l0=ns*j+k;
	if(compile_markov(index,j,k)==-1){
	  xpp_log(XPP_LOG_ERROR, "Bad expression %s[%d][%d] = %s \n",
		 markov[index].name.c_str(), j,k,markov[index].trans[l0].c_str());
	  xpp_model_failed();
	}
      }
    }
  }
}

int compile_markov(int index, int j, int k)
{
  const char *expr;
  int l0=markov[index].nstates*j+k,leng;
  int com[256];
  expr=markov[index].trans[l0].c_str();

  if(add_expr(expr,com,&leng))
    return -1;
  /* zero-padded by two, like the xpp_malloc block it replaces */
  markov[index].command[l0].assign(com, com+leng);
  markov[index].command[l0].resize(leng+2, 0);
  
  return 1;
}


void update_markov(double *x, double t, double dt)
{
  int i;
  double yp[MAXODE];
  /*  plintf(" NODE=%d x=%g \n",NODE,x[0]); */
  if(NMarkov==0)return;
  set_ivar(0,t);
  for(i=0;i<NODE;i++)set_ivar(i+1,x[i]);
  for(i=NODE+FIX_VAR;i<NODE+FIX_VAR+NMarkov;i++)set_ivar(i+1,x[i-FIX_VAR]);
  for(i=NODE;i<NODE+FIX_VAR;i++)
  set_ivar(i+1,evaluate(my_ode[i]));
  for(i=0;i<NMarkov;i++)
    yp[i]=new_state(x[NODE+i],i,dt);
  for(i=0;i<NMarkov;i++){
    x[NODE+i]=yp[i];
    set_ivar(i+NODE+FIX_VAR+1,yp[i]);
  }
}
  
  

double new_state(double old, int index, double dt)
{
  double prob,sum;
  double coin=ndrand48();
  int row=-1,rns;
  double *st;
  int i,ns=markov[index].nstates;
  int type=markov[index].type;
  st=markov[index].states.data();
  /*  plintf(" old=%g i=%d st=%g\n",old,index,st); */
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
	 prob=evaluate(markov[index].command[rns+i].data())*dt;
	 sum=sum+prob;
	 if(coin<=sum){
	   /*	   plintf("index %d switched state to %d \n",index,i); */
	   return(st[i]);
	 }
       }
     }
   }
   else{
     for(i=0;i<ns;i++){
       if(i!=row){
	 prob=markov[index].fixed[rns+i]*dt;
	 sum=sum+prob;
	 if(coin<=sum){
	   /*	   plintf("index %d switched state to %d \n",index,i); */
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
    /* plintf("rate=%g \n",rate); */
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
    /*for(i=0;i<nrxn;i++)
      rold[i]=get_ivar(rxn[i]);
	*/
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
    new_int("Seed:",&RandSeed);
    nsrand48(RandSeed);
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
    STOCH_FLAG=0;
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
  if(STOCH_HERE){
    new_browse_dat(stats,stoch_len);
    data_store.rows=stoch_len;
  }
}

void mean_back()
{
  stats_back(my_mean);
}

void variance_back()
{
  stats_back(my_variance);
}
  

void compute_em()
{
  double *x;
  x=&data_store.current[0];
  free_stoch();
  STOCH_FLAG=1;
  do_range(x,0);
  redraw_ics();
}

void free_stoch()
{
  int i;
  if(STOCH_HERE){
    data_back();
    for(i=0;i<(NEQ+1);i++){
      xpp_free(my_mean[i]);
      xpp_free(my_variance[i]);
    }
    STOCH_HERE=0;
  }
}
  

void init_stoch(int len)
{
  int i,j;
  N_TRIALS=0;
  stoch_len=len;
  for(i=0;i<(NEQ+1);i++){
    my_mean[i]=static_cast<float *>(xpp_malloc(sizeof(float)*stoch_len));
    my_variance[i]=static_cast<float *>(xpp_malloc(sizeof(float)*stoch_len));
    for(j=0;j<stoch_len;j++){
      my_mean[i][j]=0.0;
      my_variance[i][j]=0.0;
    }
  }
  for(j=0;j<stoch_len;j++){
    my_mean[0][j]=data_store.col[0][j];
    my_variance[0][j]=data_store.col[0][j];
  }
  STOCH_HERE=1;
}
    


void append_stoch(int first, int length)
{
  int i,j;
  float z;
  if(first==0)init_stoch(length);
  if(length!=stoch_len|| !STOCH_HERE)return;
  for(i=0;i<stoch_len;i++){
      for(j=1;j<=NEQ;j++){
	z=data_store.col[j][i];
	my_mean[j][i]=my_mean[j][i]+z;
	my_variance[j][i]=my_variance[j][i]+z*z;
      }
    }
  N_TRIALS++;
}

void do_stats(int ierr)
{
  int i,j;
  float ninv,mean;
  /*  STOCH_FLAG=0; */
  if(ierr!=-1&&N_TRIALS>0){
    ninv=1./static_cast<float>(N_TRIALS);
    for(i=0;i<stoch_len;i++){
      for(j=1;j<=NEQ;j++){
	mean=my_mean[j][i]*ninv;
	my_mean[j][i]=mean;
	my_variance[j][i]=(my_variance[j][i]*ninv-mean*mean);
      }
    }
 
  }
}
