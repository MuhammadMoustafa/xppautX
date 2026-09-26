#include "simplenet.h"
#include "form_ode.h"
#include "xpp_mem.h"
#include "xpp_log.h"
#include "xpp_io.h"

#include "aniparse.h"
#include "extra.h"
#include "ggets.h"
#include "markov.h"
#include "parserslow.h"
#include "tabular.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <string_view>
#include <vector>

/* 
  n is the number of values to return
  ncon is the number of connections each guy gets
  index[n][ncon] is the list of indices to which it connects
  weight[n][ncon] is the list of weights
  root is the address of lowest entry in the variable array

  To wit:
  for sparse type
  name(i) produces values[i]
  value[i] = sum(k=0..ncon-1)of(w[i][k]*root[index[i][k]])
    
ODE FILE CALL:
   
special f=sparse( n, ncon, w,index,rootname)
special f=sconv(type,n,ncon,w,rootname)
index,w are tables that are loaded by the "tabular"
command.
rootname here is the name of the root quantity.
type is either ep0  
for conv.


conv0 conve convp type
     
value[i]=    sum(j=-ncon;j<=ncon;i++){
              k=i+j;
        0 type      if(k>=0 && k<n)
              value[i]+=wgt[j+ncon]*rootname[k]
        e type k=abs(i+j); if(k<2n)
                               if(k>=n)k=n-k;
			       ...
        p type 
           k=mod(2*n+i+j,n)
        
for example  discretized diffusion
tabular dd % 3 -1 1 3*abs(t)-2
special diff=conv(even, 51, 2, dd,v0)
v[0..50]'=f(v[j],w[j])+d*diff([j])

another example
nnetwork 

tabular wgt % 51 -25 25 .032*cos(.5*pi*t/25)
special stot=conv(0, 51, 25, wgt,v0)
v[0..50]'=f(v[j],w[j])-gsyn*stot([j])*(v([j])-vsyn)


last example -- random sparse network 51 cells 5 connections each
 
tabular w % 251 0 250 .2*rand(1)     
tabular con % 251 0 250 flr(rand(1)*5)
special stot=sparse(51,5,w,con,v0)
v[0..50]'=f(v[j],w[j])-gsyn*stot([j])*(v([j])-vsyn)

more stuff:
special k= delmmult(n,m,w,tau,root)
          w has n*m values and tau has n*m delays
          n is the length of root (cols) and m is number of rows
          k(i) = sum(j=0,n-1) w[i*n+j] delay(root(j),tau[i*n+j])
          Note delays can only work on variables and not on 
          fixed  values
special k=delsparse(m,nc,w,index,tau,root)
          m is the number of return values
          nc is number of connections per node. Must be the sam
          w is  n*nc is weights
          index is  m*nc that has the indices of connections

          tau is m*nc is list of delays
      
       k[i]=sum( 0<=j < nc) w[j+nc*i]*delay(root[c[j+nc*i]],tau[j+nc*i])
 

special f=mmult(n,m,w,root)  -- note that here m is the number of return
                          values and n is the size of input root
f[j] = sum(i=0,n-1)of(w(i+n*j)*root(i)) j=0,..,m-1
special f=fmmult(n,m,w,root1,root2,fname)
f[j] = sum(i=0,n-1)of(w(i+n*j)*fname(root1[i],root2[j])   
special f=fsparse( n, ncon,w,index,root1,root2,fname)
special f=fconv(type,n,ncon,w,root1,root2,fname)
sum(j=-ncon,ncon)w(j)*fname(root1[i+j],root2[i])
similarly for fsparse

special k=fftcon(type,n,wgt,v0)

uses the fft to convolve v0 with the wgt which must be of
length n if type=periodic
       2n if type=0

special f=interp(type,n,root)
        this produces an interpolated value of x for 
        x \in [0,n)
        type =0 for linear (all that works now)
        root0,....rootn-1  are values at the integers
        Like a table, but the integer values are variables
        
special f=findext(type,n,skip,root)
if type=1  mx(0)=maximum mx(1)=index
if type=-1 mx(2)=minimum mx(3)=index
if type=0 mx(0)=maximum mx(1)=index,mx(2)=minimum mx(3)=index
          

special ydot=import(soname,sofun,nret,root,w1,w2,...wm)
  
 run right hand side in C 
 soname is shared object library say mlnet.so
 sofun  is shared object function 
 nret is the number of return values
 root is the name of the first variable



sofun(int nret, int root, double *con, double *var, double *z[50],double *ydot)

*z[50] contains a list of pointers  z[0] -> w1, .... 50 is hard coded


NOTE that the user-defined parameters start at #6 and are in order
including derived parameters but XPP takes care of this so start at 0

 
 

*/


#include <math.h>
#include <stdio.h>

#define EVEN 0
#define ZERO 1
#define PERIODIC 2
#define MAXW 50
extern int NODE,NDELAYS;
#include "delay_handle.h"
#include "fftn.h"

#define IC 2


/* simple network stuff */

#define MAXVEC 100

typedef struct {
  std::string name;
  int root,length,il,ir;
} VECTORIZER;


VECTORIZER my_vec[MAXVEC];
int n_vector=0;

typedef struct {
  double xlo,xhi,dx;
  double *y,*x;
  int n,flag,interp,autoeval;
  int xyvals;   
/* flag=0 if virgin array, flag=1 if already allocated; flag=2 for function
		         interp=0 for normal interpolation, interp=1 for 'step'
    table   and finally, xyvals=1 if both x and y vals are needed (xyvals=0
    is faster lookup )*/
  char filename[128],name[XPP_NAME_MAX+1];
}TABULAR;

extern TABULAR my_table[MAX_TAB];

typedef struct {
  int type,ncon,n;
  std::string name;
  std::string soname,sofun;

  int root,root2;
  int f[20];
  int iwgt;
  std::vector<int> gcom; /* for group commands */

  std::vector<double> values;
  /* weight, index and taud (for delays) are tables' values (my_table),
     but a gillespie chain's weight is its own gill_nu */
  double *weight,*index,*taud;
  std::vector<double> gill_nu;
  std::vector<double> fftr,ffti,dr,di;
  double *wgtlist[MAXW];
} NETWORK;

#define CONVE 0
#define CONV0 1
#define CONVP 2
#define SPARSE 3
#define FCONVE 10
#define FCONV0 11
#define FCONVP 12
#define FSPARSE 13
#define FFTCON0 4
#define FFTCONP 5
#define MMULT 6
#define FMMULT 7 
#define GILLTYPE 25
#define INTERP 30 
#define FINDEXT 35  /* find extrema in list of variables */
#define DEL_MUL 40  /* for delayed coupled networks  - global coupling */
#define DEL_SPAR 41 /* sparse with unequal in degree and delays  */
#define IMPORT  50 /* not really a network type   */

namespace {
/* a strtok token's text ("" for none) */
std::string token(const char *s) { return s ? s : ""; }
bool gilparse(std::string_view s, std::vector<int> &ind);
bool parse_import(std::string_view s, std::string &soname, std::string &sofun, int *n,
                  std::string &vname, std::vector<std::string> &tname);
} // namespace

extern double variables[],constants[];




NETWORK my_net[MAXNET];
int n_network=0;
double net_interp(double x, int i)
{
  int jlo=(int)x;
  double *y;
  int n=my_net[i].n;
  double dx=x-(double)jlo;
  y=&variables[my_net[i].root];
  if(jlo<0 || jlo>(n-1))return 0.0; /* out of range */
  return (1-dx)*y[jlo]+dx*y[jlo+1];
  

}

int add_vectorizer(const char *name,char *rhs)
{
  int i,ivar,il,ir;
  int ind;
  int len;
  int flag;

  for(i=0;i<n_vector;i++)
       if(my_vec[i].name==name)break;

  ind=i;
  flag=get_vector_info(rhs,name,&ivar,&len,&il,&ir);

  if(flag==0)return 0;
  
    my_vec[ind].root=ivar;
    my_vec[ind].length=len;
    my_vec[ind].il=il;
    my_vec[ind].ir=ir;
    xpp_log(XPP_LOG_INFO, "adding vector %s based on variable %d of length %d ends %d %d\n",
	   name,ivar,len,il,ir);
 
  return 1;

}  
void add_vectorizer_name(const char *name, const char *rhs)
{
  if(n_vector>=MAXVEC){
    xpp_log(XPP_LOG_ERROR, "Too many vectors \n");
    exit(0);
  }
  if(name_too_long(name))exit(0);
  my_vec[n_vector].name=name;
  if(add_vector_name( n_vector,name))
    exit(0);
  n_vector++;

}
double vector_value(double x, int i)
{
  int il=my_vec[i].il,ir=my_vec[i].ir,n=my_vec[i].length,k=(int)x;
  int root=my_vec[i].root;
  if((k>=0)&&(k<n))  return variables[root+k];
  if(il==PERIODIC)return variables[root+((k+n)%n)];
  if(k<0){
    if(il==ZERO)return 0.0;
    return variables[root-k-1];
  }
  if(k>=n){
    if(ir==ZERO)return 0.0;
    return variables[2*n-k-1+root];
  }
  return 0.0;



}  
double network_value(double x, int i)
{
  int j=(int)x;
  if(my_net[i].type==INTERP){
    return net_interp(x,i);
  }
  if(j>=0&&j<my_net[i].n)
    return my_net[i].values[j];
  return 0.0;
}
 

int add_spec_fun(const char *name, char *rhs)
{
  int i,ind,elen,err;
  int type;
  int iwgt,itau,iind,ivar,ivar2;
  int ntype,ntot,ncon,ntab;
  char *str;
  /* tokens of the right-hand side, checked as names after the copy */
  std::string rootname,wgtname,tauname,indname,root2name,fname,junk;
  std::string sofun,soname;
  std::vector<std::string> tname;
  type=is_network(rhs);
    if(type==0)return 0;
  xpp_log(XPP_LOG_DEBUG, "type=%d \n",type);
  for(i=0;i<n_network;i++)
    if(my_net[i].name==name)break;
  ind=i;
  if(ind>=n_network){
    xpp_log(XPP_LOG_ERROR, " No such name %s ?? \n",name);
    return 0;
  }
  switch(type){
  case 1: /* convolution */
    get_first(rhs,"(");
    str=get_next(",");
    ntype=-1;
    if(str[0]=='E')ntype=CONVE;
    if(str[0]=='0'||str[0]=='Z')ntype=CONV0;
    if(str[0]=='P')ntype=CONVP;
    if(ntype==-1){
      xpp_log(XPP_LOG_ERROR, " No such convolution type %s \n",str);
      return 0;
    }
    str=get_next(",");
    ntot=atoi(str);
    if(ntot<=0){
      xpp_log(XPP_LOG_ERROR, " %s must be positive int \n",str);
      return 0;
    }
    str=get_next(",");
    ncon=atoi(str);
    if(ncon<=0){
       xpp_log(XPP_LOG_ERROR, " %s must be positive int \n",str);
      return 0;
    }
    str=get_next(",");
    wgtname=token(str);
    iwgt=find_lookup(wgtname.c_str());
    if(iwgt<0){
      xpp_log(XPP_LOG_ERROR, "in network %s,  %s is not a table \n",
	     name,wgtname.c_str());
      return 0;
    }
    str=get_next(")");
    rootname=token(str);
    ivar=get_var_index(rootname.c_str());
    if(ivar<0){
      xpp_log(XPP_LOG_ERROR, " In %s , %s is not valid variable\n",
	     name,rootname.c_str());
      return 0;
    }
    my_net[ind].values.assign((ntot+1),0.0);
    my_net[ind].weight=my_table[iwgt].y;
    my_net[ind].type=ntype;
    my_net[ind].root=ivar;
    my_net[ind].n=ntot;
    my_net[ind].ncon=ncon;
    xpp_log(XPP_LOG_INFO, " Added net %s type %d len=%d x %d using %s var[%d] \n",
	   name,ntype,ntot,ncon,wgtname.c_str(),ivar);
    
    return 1;   
    break;
  case 2: /* sparse */
    get_first(rhs,"(");
    str=get_next(",");
    ntype=SPARSE;
    ntot=atoi(str);
    
    if(ntot<=0){
      xpp_log(XPP_LOG_ERROR, " %s must be positive int \n",str);
      return 0;
    }
    str=get_next(",");
    ncon=atoi(str);
    
    if(ncon<=0){
       xpp_log(XPP_LOG_ERROR, " %s must be positive int \n",str);
      return 0;
    }
    str=get_next(",");
    wgtname=token(str);
    iwgt=find_lookup(wgtname.c_str());
    
    if(iwgt<0){
      xpp_log(XPP_LOG_ERROR, "in network %s,  %s is not a table \n",
	     name,wgtname.c_str());
      return 0;
    }

     str=get_next(",");
    indname=token(str);
    iind=find_lookup(indname.c_str());
    
    if(iind<0){
      xpp_log(XPP_LOG_ERROR, "in network %s,  %s is not a table \n",
	     name,indname.c_str());
      return 0;
    }
    str=get_next(")");
    rootname=token(str);
       ivar=get_var_index(rootname.c_str());
  

   if(ivar<0){
      xpp_log(XPP_LOG_ERROR, " In %s , %s is not valid variable\n",
	     name,rootname.c_str());
      return 0;
    }
 
    my_net[ind].values.assign((ntot+1),0.0);
    my_net[ind].weight=my_table[iwgt].y;
    my_net[ind].index=my_table[iind].y;

    my_net[ind].type=ntype;
    my_net[ind].root=ivar;
    my_net[ind].n=ntot;
    my_net[ind].ncon=ncon;
    xpp_log(XPP_LOG_INFO, " Added sparse %s len=%d x %d using %s var[%d]  and %s\n",
	   name,ntot,ncon,wgtname.c_str(),ivar,indname.c_str() );
    return 1;   
    break;
 case 3: /* convolution */
    get_first(rhs,"(");
    str=get_next(",");
    ntype=-1;
    if(str[0]=='E')ntype=FCONVE;
    if(str[0]=='0'||str[0]=='Z')ntype=FCONV0;
    if(str[0]=='P')ntype=FCONVP;
    if(ntype==-1){
      xpp_log(XPP_LOG_ERROR, " No such convolution type %s \n",str);
      return 0;
    }
    str=get_next(",");
    ntot=atoi(str);
    if(ntot<=0){
      xpp_log(XPP_LOG_ERROR, " %s must be positive int \n",str);
      return 0;
    }
    str=get_next(",");
    ncon=atoi(str);
    if(ncon<=0){
       xpp_log(XPP_LOG_ERROR, " %s must be positive int \n",str);
      return 0;
    }
    str=get_next(",");
    wgtname=token(str);
    iwgt=find_lookup(wgtname.c_str());
    if(iwgt<0){
      xpp_log(XPP_LOG_ERROR, "in network %s,  %s is not a table \n",
	     name,wgtname.c_str());
      return 0;
    }


    str=get_next(",");
    rootname=token(str);
    ivar=get_var_index(rootname.c_str());
    if(ivar<0){
      xpp_log(XPP_LOG_ERROR, " In %s , %s is not valid variable\n",
	     name,rootname.c_str());
      return 0;
    }

    str=get_next(",");
    root2name=token(str);
    ivar2=get_var_index(root2name.c_str());
    if(ivar2<0){
      xpp_log(XPP_LOG_ERROR, " In %s , %s is not valid variable\n",
	     name,root2name.c_str());
      return 0;
    }
    str=get_next(")");
    fname=token(str);
    junk=xpp::format("{}({},{})",fname,rootname,root2name);
    if(add_expr(junk.c_str(),my_net[ind].f,&elen)){
      xpp_log(XPP_LOG_ERROR, " bad function %s \n",fname.c_str());
      return 0;
    }
    my_net[ind].values.assign((ntot+1),0.0);
    my_net[ind].weight=my_table[iwgt].y;
    my_net[ind].type=ntype;
    my_net[ind].root=my_net[ind].f[0]; /* this is strange - I am adding the compiled names */
    my_net[ind].root2=my_net[ind].f[1];
    my_net[ind].n=ntot;
    my_net[ind].ncon=ncon;
    xpp_log(XPP_LOG_INFO, " Added net %s type %d len=%d x %d using %s %s(var[%d],var[%d]) \n",
	   name,ntype,ntot,ncon,wgtname.c_str(),fname.c_str(),ivar,ivar2);
    return 1;   
    break;
  case 4: /* sparse */
    get_first(rhs,"(");
    str=get_next(",");
    ntype=FSPARSE;
    ntot=atoi(str);
    
    if(ntot<=0){
      xpp_log(XPP_LOG_ERROR, " %s must be positive int \n",str);
      return 0;
    }
    str=get_next(",");
    ncon=atoi(str);
    
    if(ncon<=0){
       xpp_log(XPP_LOG_ERROR, " %s must be positive int \n",str);
      return 0;
    }
    str=get_next(",");
    wgtname=token(str);
    iwgt=find_lookup(wgtname.c_str());
    
    if(iwgt<0){
      xpp_log(XPP_LOG_ERROR, "in network %s,  %s is not a table \n",
	     name,wgtname.c_str());
      return 0;
    }

     str=get_next(",");
    indname=token(str);
    iind=find_lookup(indname.c_str());
    
    if(iind<0){
      xpp_log(XPP_LOG_ERROR, "in network %s,  %s is not a table \n",
	     name,indname.c_str());
      return 0;
    }


    str=get_next(",");
    rootname=token(str);
       ivar=get_var_index(rootname.c_str());
  

   if(ivar<0){
      xpp_log(XPP_LOG_ERROR, " In %s , %s is not valid variable\n",
	     name,rootname.c_str());
      return 0;
    }
 

    str=get_next(",");
    root2name=token(str);
    ivar2=get_var_index(root2name.c_str());
    if(ivar2<0){
      xpp_log(XPP_LOG_ERROR, " In %s , %s is not valid variable\n",
	     name,root2name.c_str());
      return 0;
    }
    str=get_next(")");
    fname=token(str);
    junk=xpp::format("{}({},{})",fname,rootname,root2name);
    if(add_expr(junk.c_str(),my_net[ind].f,&elen)){
      xpp_log(XPP_LOG_ERROR, " bad function %s \n",fname.c_str());
      return 0;
    }

    my_net[ind].values.assign((ntot+1),0.0);
    my_net[ind].weight=my_table[iwgt].y;
    my_net[ind].index=my_table[iind].y;

    my_net[ind].type=ntype;
    my_net[ind].root=my_net[ind].f[0]; /* this is strange - I am adding the compiled names */
    my_net[ind].root2=my_net[ind].f[1];
    my_net[ind].n=ntot;
    my_net[ind].ncon=ncon;
    xpp_log(XPP_LOG_INFO, " Sparse %s len=%d x %d using %s %s(var[%d],var[%d]) and %s\n",
	   name,ntot,ncon,wgtname.c_str(),fname.c_str(),ivar,ivar2,indname.c_str() );
    return 1;   
    break;

  case 5: /* fft convolution */
    get_first(rhs,"(");
    str=get_next(",");
    ntype=-1;
    /* if(str[0]=='E')ntype=CONVE; */
    if(str[0]=='0'||str[0]=='Z')ntype=FFTCON0;
    if(str[0]=='P')ntype=FFTCONP;
    if(ntype==-1){
      xpp_log(XPP_LOG_ERROR, " No such fft convolution type %s \n",str);
      return 0;
    }
    str=get_next(",");
    ntot=atoi(str);
    if(ntot<=0){
      xpp_log(XPP_LOG_ERROR, " %s must be positive int \n",str);
      return 0;
    }
   
    str=get_next(",");
    wgtname=token(str);
    iwgt=find_lookup(wgtname.c_str());
    if(iwgt<0){
      xpp_log(XPP_LOG_ERROR, "in network %s,  %s is not a table \n",
	     name,wgtname.c_str());
      return 0;
    }
    ntab=get_lookup_len(iwgt);
    if(type==FFTCONP&&ntab<ntot){
     xpp_log(XPP_LOG_ERROR, " In %s, weight is length %d < %d \n",name,ntab,ntot);
     return 0;
    }
    if(type==FFTCON0&&ntab<(2*ntot)){
     xpp_log(XPP_LOG_ERROR, " In %s, weight is length %d < %d \n",name,ntab,2*ntot);
     return 0;
    }
    str=get_next(")");
    rootname=token(str);
    ivar=get_var_index(rootname.c_str());
    if(ivar<0){
      xpp_log(XPP_LOG_ERROR, " In %s , %s is not valid variable\n",
	     name,rootname.c_str());
      return 0;
    }
    if(ntype==FFTCON0)
      ncon=2*ntot;
    else
      ncon=ntot;
    my_net[ind].fftr.assign(ncon+2,0.0);
    my_net[ind].ffti.assign(ncon+2,0.0);
    my_net[ind].dr.assign(ncon+2,0.0);
    my_net[ind].di.assign(ncon+2,0.0);
    my_net[ind].iwgt=iwgt;
    my_net[ind].values.assign((ntot+1),0.0);
    my_net[ind].weight=my_table[iwgt].y;
    my_net[ind].type=ntype;
    my_net[ind].root=ivar;
    my_net[ind].n=ntot;
    my_net[ind].ncon=ncon;
    update_fft(ind);

    xpp_log(XPP_LOG_INFO, " Added net %s type %d len=%d x %d using %s var[%d] \n",
	   name,ntype,ntot,ncon,wgtname.c_str(),ivar);
    return 1;   
    break;
  case 6:   /* MMULT    ntot=n,ncon=m  */
    get_first(rhs,"(");
    str=get_next(",");
    ntype=MMULT;
    ntot=atoi(str);
    
    if(ntot<=0){
      xpp_log(XPP_LOG_ERROR, " %s must be positive int \n",str);
      return 0;
    }
    str=get_next(",");
    ncon=atoi(str);
    
    if(ncon<=0){
       xpp_log(XPP_LOG_ERROR, " %s must be positive int \n",str);
      return 0;
    }
    str=get_next(",");
    wgtname=token(str);
    iwgt=find_lookup(wgtname.c_str());
    
    if(iwgt<0){
      xpp_log(XPP_LOG_ERROR, "in network %s,  %s is not a table \n",
	     name,wgtname.c_str());
      return 0;
    }

    str=get_next(")");
    rootname=token(str);
       ivar=get_var_index(rootname.c_str());
  

   if(ivar<0){
      xpp_log(XPP_LOG_ERROR, " In %s , %s is not valid variable\n",
	     name,rootname.c_str());
      return 0;
    }
 
    my_net[ind].values.assign((ncon+1),0.0);
    my_net[ind].weight=my_table[iwgt].y;

    my_net[ind].type=ntype;
    my_net[ind].root=ivar;
    my_net[ind].n=ncon;
    my_net[ind].ncon=ntot;
    xpp_log(XPP_LOG_INFO, " Added mmult %s len=%d x %d using %s var[%d]\n",
	   name,ntot,ncon,wgtname.c_str(),ivar,indname.c_str() );
    return 1;   
    break;
  case 7:  /* FMMULT */
     get_first(rhs,"(");
    str=get_next(",");
    ntype=FMMULT;
    ntot=atoi(str);
    
    if(ntot<=0){
      xpp_log(XPP_LOG_ERROR, " %s must be positive int \n",str);
      return 0;
    }
    str=get_next(",");
    ncon=atoi(str);
    
    if(ncon<=0){
       xpp_log(XPP_LOG_ERROR, " %s must be positive int \n",str);
      return 0;
    }
    str=get_next(",");
    wgtname=token(str);
    iwgt=find_lookup(wgtname.c_str());
    
    if(iwgt<0){
      xpp_log(XPP_LOG_ERROR, "in network %s,  %s is not a table \n",
	     name,wgtname.c_str());
      return 0;
    }

    str=get_next(",");
    rootname=token(str);
       ivar=get_var_index(rootname.c_str());
  

   if(ivar<0){
      xpp_log(XPP_LOG_ERROR, " In %s , %s is not valid variable\n",
	     name,rootname.c_str());
      return 0;
    }
  str=get_next(",");
    root2name=token(str);
    ivar2=get_var_index(root2name.c_str());
    if(ivar2<0){
      xpp_log(XPP_LOG_ERROR, " In %s , %s is not valid variable\n",
	     name,root2name.c_str());
      return 0;
    }
    str=get_next(")");
    fname=token(str);
    junk=xpp::format("{}({},{})",fname,rootname,root2name);
    if(add_expr(junk.c_str(),my_net[ind].f,&elen)){
      xpp_log(XPP_LOG_ERROR, " bad function %s \n",fname.c_str());
      return 0;
    }
    /*for(i=0;i<elen;i++)
      printf("%d %d \n",i,my_net[ind].f[i]);
    */
    my_net[ind].values.assign((ncon+1),0.0);
    my_net[ind].weight=my_table[iwgt].y;

    my_net[ind].type=ntype;
    my_net[ind].root=my_net[ind].f[0]; /* this is strange - I am adding the compiled names */
    my_net[ind].root2=my_net[ind].f[1];
    my_net[ind].n=ncon;
    my_net[ind].ncon=ntot;
    xpp_log(XPP_LOG_INFO, " Added fmmult %s len=%d x %d using %s %s(var[%d],var[%d])\n",
	   name,ntot,ncon,wgtname.c_str(),fname.c_str(),ivar,ivar2);
    return 1; 

  case FINDEXT:
    get_first(rhs,"(");
    str=get_next(",");
    ntype=atoi(str);
    if(ntype>1||ntype<(-1)){
      xpp_log(XPP_LOG_ERROR, "In %s,  type =-1,0,1 not %s \n",
	     name,ntype);
      return 0;
    }
    str=get_next(",");
    ntot=atoi(str);
    if(ntot<=0){
      xpp_log(XPP_LOG_ERROR, "In %s,  n>0 not %s \n",
	     name,ntot);
      return 0;
    }
    
    str=get_next(",");
    ncon=atoi(str);
    if(ncon<=0){
      xpp_log(XPP_LOG_ERROR, "In %s,  skip>=1 not %s \n",
	     name,ncon);
      return 0;
    }
    str=get_next(")");
    rootname=token(str);
    ivar=get_var_index(rootname.c_str());
    if(ivar<0){
      xpp_log(XPP_LOG_ERROR, " In %s , %s is not valid variable\n",
	     name,rootname.c_str());
      return 0;
    }
    my_net[ind].values.assign(6,0.0);
    my_net[ind].type=FINDEXT;
    my_net[ind].root=ivar;
    my_net[ind].n=ntot;
    my_net[ind].ncon=ncon;
    my_net[ind].iwgt=ntype;
    xpp_log(XPP_LOG_INFO, " Added findextr %s: type=%d len=%d  skip= %d using var[%d] \n",
	   name,ntype,ntot,ncon,ivar);
    return 1; 

   case 30:
    /* interpolation array 
       z=INTERP(meth,n,root)
    */
    get_first(rhs,"(");
    str=get_next(",");
    ivar=atoi(str);
    my_net[ind].type=INTERP;
    my_net[ind].iwgt=ivar;
    str=get_next(",");
    ivar=atoi(str);
    if(ivar<1){
      xpp_log(XPP_LOG_ERROR, "Need more than 1 entry for interpolate\n");
      return 0;
    }
    my_net[ind].n=ivar; /* # entries in array */
    str=get_next(")");
    rootname=token(str);
    ivar=get_var_index(rootname.c_str());
    if(ivar<0){
      xpp_log(XPP_LOG_ERROR, " In %s , %s is not valid variable\n",
	     name,rootname.c_str());
      return 0;
    }
    my_net[ind].root=ivar;
    xpp_log(XPP_LOG_INFO, "Added interpolator %s length %d on %s \n",name,my_net[ind].n,rootname.c_str()); 
    return 1;

   case IMPORT:
     ntype=IMPORT;
     err=parse_import(rhs,soname,sofun,&ncon,rootname,tname);
     ntab=static_cast<int>(tname.size());
     if(err==0)return 0;
     my_net[ind].values.assign((ncon+1),0.0);
     my_net[ind].n=ncon;
     ivar=get_var_index(rootname.c_str());
     if(ivar<0){
      xpp_log(XPP_LOG_ERROR, " In %s , %s is not valid variable\n",
	     name,rootname.c_str());
      return 0;
    }
     my_net[ind].soname=soname;
     my_net[ind].sofun=sofun;
     my_net[ind].root=ivar;
     my_net[ind].type=ntype;
     my_net[ind].ncon=0;
     for(i=0;i<ntab;i++){
       iwgt=find_lookup(tname[i].c_str());
       xpp::log(XPP_LOG_DEBUG, "Found {}\n",tname[i]);
       if(iwgt<0){
	 xpp::log(XPP_LOG_ERROR, "in network {},  {} is not a table \n",
		name,tname[i]);
	 return 0;
       }
       my_net[ind].wgtlist[i]=my_table[iwgt].y;
     }
     xpp_log(XPP_LOG_INFO, " Added import %s len=%d  with %s %s var[%d] %d weights\n",
	    name,my_net[ind].n,soname.c_str(),sofun.c_str(),ivar,ntab );
     
     return 1;
   case DEL_MUL:

    get_first(rhs,"(");
    str=get_next(",");
    ntype=DEL_MUL;
    ntot=atoi(str);
    
    if(ntot<=0){
      xpp_log(XPP_LOG_ERROR, " %s must be positive int \n",str);
      return 0;
    }
    str=get_next(",");
    ncon=atoi(str);
    
    if(ncon<=0){
       xpp_log(XPP_LOG_ERROR, " %s must be positive int \n",str);
      return 0;
    }
    str=get_next(",");
    wgtname=token(str);
    iwgt=find_lookup(wgtname.c_str());
    
    if(iwgt<0){
      xpp_log(XPP_LOG_ERROR, "in network %s,  %s is not a table \n",
	     name,wgtname.c_str());
      return 0;
    }
    str=get_next(",");
    tauname=token(str);
    itau=find_lookup(tauname.c_str());
    
    if(itau<0){
      xpp_log(XPP_LOG_ERROR, "in network %s,  %s is not a table \n",
	     name,tauname.c_str());
      return 0;
    }

     

    str=get_next(")");
    rootname=token(str);
       ivar=get_var_index(rootname.c_str());
  

   if(ivar<0){
      xpp_log(XPP_LOG_ERROR, " In %s , %s is not valid variable\n",
	     name,rootname.c_str());
      return 0;
    }
 
    my_net[ind].values.assign((ncon+1),0.0);
    my_net[ind].weight=my_table[iwgt].y;
    my_net[ind].taud=my_table[itau].y;

    my_net[ind].type=ntype;
    my_net[ind].root=ivar;
    my_net[ind].n=ncon;
    my_net[ind].ncon=ntot;
    xpp_log(XPP_LOG_INFO, " Added del_mul %s len=%d x %d using %s var[%d] with delay %s\n",
	   name,ntot,ncon,wgtname.c_str(),ivar,indname.c_str(),tauname.c_str() );
    NDELAYS=1;
    return 1;   
    break;
    return 0;
  case DEL_SPAR:
   get_first(rhs,"(");
    str=get_next(",");
    ntype=DEL_SPAR;
    ntot=atoi(str);
    
    if(ntot<=0){
      xpp_log(XPP_LOG_ERROR, " %s must be positive int \n",str);
      return 0;
    }
    str=get_next(",");
    ncon=atoi(str);
    
    if(ncon<=0){
       xpp_log(XPP_LOG_ERROR, " %s must be positive int \n",str);
      return 0;
    }
    str=get_next(",");
    wgtname=token(str);
    iwgt=find_lookup(wgtname.c_str());
    
    if(iwgt<0){
      xpp_log(XPP_LOG_ERROR, "in network %s,  %s is not a table \n",
	     name,wgtname.c_str());
      return 0;
    }

     str=get_next(",");
    indname=token(str);
    iind=find_lookup(indname.c_str());
    
    if(iind<0){
      xpp_log(XPP_LOG_ERROR, "in network %s,  %s is not a table \n",
	     name,indname.c_str());
      return 0;
    }


     str=get_next(",");
    tauname=token(str);
    itau=find_lookup(tauname.c_str());
    
    if(itau<0){
      xpp_log(XPP_LOG_ERROR, "in network %s,  %s is not a table \n",
	     name,tauname.c_str());
      return 0;
    }

    
    str=get_next(")");
    rootname=token(str);
       ivar=get_var_index(rootname.c_str());
  

   if(ivar<0){
      xpp_log(XPP_LOG_ERROR, " In %s , %s is not valid variable\n",
	     name,rootname.c_str());
      return 0;
    }
 
    my_net[ind].values.assign((ntot+1),0.0);
    my_net[ind].weight=my_table[iwgt].y;
    my_net[ind].index=my_table[iind].y;
    my_net[ind].taud=my_table[itau].y;
    my_net[ind].type=ntype;
    my_net[ind].root=ivar;
    my_net[ind].n=ntot;
    my_net[ind].ncon=ncon;
    xpp_log(XPP_LOG_INFO, " Added sparse %s len=%d x %d using %s var[%d]  and %s with dely %s\n",
	   name,ntot,ncon,wgtname.c_str(),ivar,indname.c_str(),tauname.c_str() );
    NDELAYS=1;
    return 1;   
    break;


    return 0;
  case 10:
    /* 
       z=GILL(meth,rxn list)
       e.g
       z=GILL(meth,r{1-15})
       GILL is different -
       iwgt=evaluation method - 0 is standard
                                1 - tau-leap
       root=number of reactions
       values[0]=time of next reaction
       values[1..root]=number of times this rxn took place
       gcom contains list of all the fixed holding the reactions
    */ 
       
    get_first(rhs,"(");
    str=get_next(",");
    ivar=atoi(str);
    str=get_next(")");
    my_net[ind].type=GILLTYPE;
    if(ivar>0){
      xpp_log(XPP_LOG_WARN, " Tau leaping not implemented yet. Changing to 0\n");
      ivar=0;
    }
    my_net[ind].iwgt=ivar;
    if(gilparse(str,my_net[ind].gcom)==0)
      return 0;
    ivar2=static_cast<int>(my_net[ind].gcom.size());
    my_net[ind].root=ivar2;
    my_net[ind].n=ivar2+1;
    my_net[ind].ncon=-1;
    /* zeroed: the first output row reads them before the first step */
    my_net[ind].values.assign(ivar2+2,0.0);
    xpp_log(XPP_LOG_INFO, "Added gillespie chain with %d reactions \n",ivar2);
    return 1;

    /*  case 8:  
    get_first(rhs,"(");
    str=get_next(",");
    ntot=atoi(str);
    str=get_next("{");
    i=0;
    elen=strlen(str);
    
    while(1){
      cc=str[i];
      if(cc=='}'){junk[i]=0;
                   break;
      }
      junk[i]=cc;
      i++;
      if(i==elen){
	plintf("Illegal syntax for GROUP %s \n",str);
	return 0;
      }
      
    }
    plintf("total=%d str=%s\n",ntot,junk);
    
    return 0; */
    
    
  }
  return 0;
}
void add_special_name(const char *name, char *rhs)
{
  if(is_network(rhs)){
    xpp_log(XPP_LOG_DEBUG, " netrhs = |%s| \n",rhs);
    if(n_network>=MAXNET){
      return;
    }
    if(name_too_long(name))exit(0);
    my_net[n_network].name=name;
    add_net_name(n_network,name);
    n_network++;
  }
  else
    xpp_log(XPP_LOG_WARN, " No such special type ...\n");
}

int is_network(char *s)
{
  /*int n;
  */
  de_space(s);
  strupr(s);
 /* n=strlen(s); Not used*/
  if(s[0]=='C' &&s[1]=='O' &&s[2]=='N' && s[3]=='V')return 1;
  if(s[0]=='S' &&s[1]=='P' &&s[2]=='A' && s[3]=='R')return 2;
  if(s[0]=='F'&&s[1]=='C' &&s[2]=='O' &&s[3]=='N' && s[4]=='V')return 3;
  if(s[0]=='F' && s[1]=='S' &&s[2]=='P' &&s[3]=='A' && s[4]=='R')return 4;
  if(s[0]=='F' && s[1]=='F' && s[2]=='T' && s[3]=='C' )return 5; 
  if(s[0]=='M' &&s[1]=='M' &&s[2]=='U' && s[3]=='L')return 6;
  if(s[0]=='F'&& s[1]=='M' &&s[2]=='M' &&s[3]=='U' && s[4]=='L')return 7;
  if(s[0]=='G'&& s[1]=='I' &&s[2]=='L' &&s[3]=='L')return 10;
  if(s[0]=='I' && s[1]=='N' &&s[2]=='T' && s[3]=='E' && s[4]=='R')return INTERP;
  if(s[0]=='F' && s[1]=='I' &&s[2]=='N' && s[3]=='D' && s[4]=='E')return FINDEXT;
   if(s[0]=='D' &&s[1]=='E' &&s[2]=='L' && s[3]=='M')return DEL_MUL;
   if(s[0]=='D' &&s[1]=='E' &&s[2]=='L' && s[3]=='S')return DEL_SPAR;
   if(s[0]=='I' &&s[1]=='M' &&s[2]=='P' && s[3]=='O')return IMPORT;  
  /* if(s[0]=='G'&& s[1]=='R' && s[2]=='O' && s[3]=='U')return 8; */
  return 0;
}
  

void eval_all_nets()
{
  int i;
  for(i=0;i<n_network;i++)
    evaluate_network(i);
}

void evaluate_network(int ind)
{
   int i,j,k,ij;
   int imin,imax;
   double ymin,ymax;
   int skip;
   int mmt;
   int in0;
   double sum,z;
   int n=my_net[ind].n,*f;
   int ncon=my_net[ind].ncon;
   double *w,*y,*cc,*values,*tau;
   int twon=2*n,root=my_net[ind].root,root2=my_net[ind].root2;
   cc=my_net[ind].index;
   w=my_net[ind].weight;
   values=my_net[ind].values.data();
   /*  y=&variables[my_net[ind].root]; */
   switch(my_net[ind].type){
   case FINDEXT:
     y=&variables[root];
     mmt=my_net[ind].iwgt;
     skip=ncon;
     imax=0;
     ymax=y[0];
     imin=0;
     ymin=y[0];
     i=0;

     if(mmt==1||mmt==0){ /* get max  */
       while(i<n){

	 if(y[i]>ymax){
	   imax=i;
	   ymax=y[i];
	 }
	 i+=skip;
       }

     }
     if(mmt==(-1)||mmt==0){ /* get min */
       i=0;
       while(i<n){

	 if(y[i]<ymin){
	   imin=i;
	   ymin=y[i];
	 }
	 i+=skip;
       }

     }
     values[1]=(double)imax;
     values[0]=ymax;
     values[3]=(double)imin;
     values[2]=ymin;
     break;
   case INTERP: /* do nothing! */ 
     break;
   case GILLTYPE:
     if(my_net[ind].ncon==-1&&my_net[ind].iwgt>0){
       my_net[ind].gill_nu.assign(static_cast<size_t>(my_net[ind].root)*NODE,0.0);
       my_net[ind].weight=my_net[ind].gill_nu.data();
       make_gill_nu(my_net[ind].weight,NODE,my_net[ind].root,my_net[ind].values.data());
       my_net[ind].ncon=0;
     }
     one_gill_step(my_net[ind].iwgt,my_net[ind].root,my_net[ind].gcom.data(),my_net[ind].values.data());
     break;
   case CONVE:
     y=&variables[root];
     for(i=0;i<n;i++){
       sum=0.0;
       for(j=-ncon;j<=ncon;j++){
	 k=abs(i+j);
	 if(k<twon){
	   if(k>=n)k=abs(twon-2-k);
	   sum+=(w[j+ncon]*y[k]);
	 }
       }
       values[i]=sum;
     }
     break;
     case CONV0:
     y=&variables[root];
     for(i=0;i<n;i++){
       sum=0.0;
       for(j=-ncon;j<=ncon;j++){
	 k=i+j;
	 if(k<n&&k>=0)
	   sum+=(w[j+ncon]*y[k]);
       }
       values[i]=sum;
     }
     break;
     case CONVP:
     y=&variables[root];
     for(i=0;i<n;i++){
       sum=0.0;
       for(j=-ncon;j<=ncon;j++){
	 k=((twon+i+j)%n);
	 sum+=(w[j+ncon]*y[k]);
       }
       values[i]=sum;
     }
     break;
   case FFTCONP:
     y=&variables[root];
    fft_conv(0,n,values,y,my_net[ind].fftr.data(),my_net[ind].ffti.data(),my_net[ind].dr.data(),my_net[ind].di.data());
    break;
   
   case FFTCON0:
     y=&variables[root];           
      fft_conv(1,n,values,y,my_net[ind].fftr.data(),my_net[ind].ffti.data(),my_net[ind].dr.data(),my_net[ind].di.data());
    break;

   case IMPORT:
     get_import_values(n,values,my_net[ind].soname.c_str(),my_net[ind].sofun.c_str(),my_net[ind].root,my_net[ind].wgtlist,variables,&constants[6]);
     break;
   case DEL_MUL:
     tau=my_net[ind].taud;
     in0=my_net[ind].root;
     for(j=0;j<n;j++){
       sum=0.0;
       for(i=0;i<ncon;i++){
         ij=j*ncon+i;
	 /* root indexes variables[], where t comes first; get_delay counts
	    from the first state variable, as the parser's delay() does */
	 sum+=(w[ij]*get_delay(i+in0-1,tau[ij]));
       }
       values[j]=sum;
     }
     break;
   case MMULT:
     y=&variables[root];
     for(j=0;j<n;j++){
       sum=0.0;
       for(i=0;i<ncon;i++){
	 ij=j*ncon+i;
	 sum+=(w[ij]*y[i]);
       }
       values[j]=sum;
     }
     break;
   case DEL_SPAR:
     tau=my_net[ind].taud;
     in0=my_net[ind].root;
      for(i=0;i<n;i++){
       sum=0.0;
       for(j=0;j<ncon;j++){
	 ij=i*ncon+j;
	 k=(int)cc[ij];
         if(k>=0)
	   sum+=(w[ij]*get_delay(k+in0-1,tau[ij])); /* as in DEL_MUL */
       }
       values[i]=sum;
     }  
      break;
   case SPARSE:
     y=&variables[root];
     for(i=0;i<n;i++){
       sum=0.0;
       for(j=0;j<ncon;j++){
	 ij=i*ncon+j;
	 k=(int)cc[ij];
         if(k>=0)
	   sum+=(w[ij]*y[k]);
       }
       values[i]=sum;
     }
     break;

     /*     f stuff  */           
   case FCONVE:
     f=my_net[ind].f;

     for(i=0;i<n;i++){
       sum=0.0;
       f[1]=root+i;
       for(j=-ncon;j<=ncon;j++){
	 k=abs(i+j);
	 if(k<twon){
	   if(k>=n)k=abs(twon-2-k);
           f[0]=root2+k;

	   z=evaluate(f);
	   sum+=(w[j+ncon]*z);
	 }
       }
       values[i]=sum;
     }
     break;
   case FCONV0:
     f=my_net[ind].f;
     
     for(i=0;i<n;i++){
       sum=0.0;
       f[1]=root+i;
       for(j=-ncon;j<=ncon;j++){
	 k=i+j;
	 if(k<n&&k>=0){
	
	   f[0]=root2+k;
	   z=evaluate(f);
	   sum+=(w[j+ncon]*z);
	 }
       }
       values[i]=sum;
     }
     break;
   case FCONVP:
     f=my_net[ind].f;

     for(i=0;i<n;i++){
       f[1]=root+i;
       sum=0.0;
       for(j=-ncon;j<=ncon;j++){
	 k=((twon+i+j)%n);
	 f[0]=root2+k;
	 z=evaluate(f);
	 sum+=(w[j+ncon]*z);
       }
       values[i]=sum;
     }
     break;
   case FSPARSE:
     f=my_net[ind].f;

     for(i=0;i<n;i++){
       f[1]=root+i;
       sum=0.0;
       for(j=0;j<ncon;j++){
	 ij=i*ncon+j;
	 k=(int)cc[ij];
         if(k>=0){
	   f[0]=root2+k;
	   z=evaluate(f);
	   sum+=(w[ij]*z);
	 }
       }
       values[i]=sum;
     }
     break;
   case FMMULT:
     f=my_net[ind].f;

     for(j=0;j<n;j++){

       f[1]=root+j;

       sum=0.0;
       for(i=0;i<ncon;i++){
	 ij=j*ncon+i;

         f[0]=root2+i;
         
	 z=evaluate(f);

	 sum+=(w[ij]*z);
       }

       values[j]=sum;
     }
     break;
   }
}

void update_all_ffts()
{
  int i;

  for(i=0;i<n_network;i++)
    if(my_net[i].type==FFTCON0||my_net[i].type==FFTCONP)
      update_fft(i);
}
/*  
 tabular weights are of size 2k+1
 and go from -k ... k
 for FFT's 
 they are reordered as follows
 fftr[i]=wgt[i+k] i = 0.. k
 fftr[i+k]=wgt[i] i=1 .. k-1
*/	 
void update_fft(int ind)
{
  int i;
  int dims[2];
  double *w=my_net[ind].weight;
  double *fftr=my_net[ind].fftr.data();
  double *ffti=my_net[ind].ffti.data();
  int n,n2;
  int type=my_net[ind].type;
  if(type==FFTCONP){
    n=my_net[ind].n;
    n2=n/2;
    for(i=0;i<n;i++)ffti[i]=0.0;
    for(i=0;i<=n2;i++)
      fftr[i]=w[i+n2];
    for(i=0;i<n2;i++)
      fftr[n2+i+1]=w[i];
    dims[0]=n;
    fftn(1,dims,fftr,ffti,1,1.);
    /* plintf("index=%d n=%d n2=%d \n",ind,n,n2); 
    for(i=0;i<n;i++)
    plintf("(%g , %g)\n",fftr[i],ffti[i]); */
  }
  if(type==FFTCON0){
    n=2*my_net[ind].n;
    n2=n/2;
    for(i=0;i<n;i++)ffti[i]=0.0;
    for(i=0;i<=n2;i++)
      fftr[i]=w[i+n2];
    for(i=1;i<n2;i++)
      fftr[n2+i]=w[i];
    dims[0]=n;
    fftn(1,dims,fftr,ffti,1,1.);
  }
  /* for(i=0;i<10;i++)printf("fftr,i=%g %g %g %g\n",fftr[i],ffti[i],fftr[n-1-i],ffti[n-1-i]); */
}


void fft_conv(int it,int n,double *values,double *yy,double *fftr,double *ffti,double *dr,double *di)
{
  int i;
 int dims[2];
 double x,y;
 int n2=2*n;
  switch(it){
  case 0:
    dims[0]=n;
    for(i=0;i<n;i++){
      di[i]=0.0;
      dr[i]=yy[i];

    }
    
    fftn(1,dims,dr,di,1,-2.0);



    for(i=0;i<n;i++){
      x=dr[i]*fftr[i]-di[i]*ffti[i];
      y=dr[i]*ffti[i]+di[i]*fftr[i];
      dr[i]=x;
      di[i]=y;
    } 
   
    fftn(1,dims,dr,di,-1,-2.0);
    for(i=0;i<n;i++)
      values[i]=dr[i];
   
    return;
  case 1:
     dims[0]=n2;
    for(i=0;i<n2;i++){
      di[i]=0.0;
      if(i<n)
	dr[i]=yy[i];
      else
	dr[i]=0.0;
    }
    fftn(1,dims,dr,di,1,-2.0);
    for(i=0;i<n2;i++){
      x=dr[i]*fftr[i]-di[i]*ffti[i];
      y=dr[i]*ffti[i]+di[i]*fftr[i];
      dr[i]=x;
      di[i]=y;
    } 
    fftn(1,dims,dr,di,-1,-2.0);
    for(i=0;i<n;i++)
      values[i]=dr[i];
    return;
    
  }
}

/* parsing stuff to get gillespie code quickly */

namespace {

/* plucks info out of  xxx{aa-bb}  or returns string: root xxx, flag 1
   and the range aa..bb, or root the whole of s and flag 0. false when
   the range has no '-'. */
bool g_namelist(std::string_view s, std::string &root, int &flag, int &i1, int &i2)
{
  size_t ir = s.rfind('{');
  flag = 0;
  if (ir == std::string_view::npos) {
    root = s;
    return true;
  }
  root = s.substr(0, ir);
  flag = 1;
  size_t dash = s.find('-', ir + 1);
  if (dash == std::string_view::npos) {
    xpp::log(XPP_LOG_DEBUG, "Illegal syntax {}\n", s);
    return false;
  }
  i1 = atoi(std::string(s.substr(ir + 1, dash - ir - 1)).c_str());
  size_t close = s.find('}', dash + 1);
  if (close == std::string_view::npos) close = s.size();
  i2 = atoi(std::string(s.substr(dash + 1, close - dash - 1)).c_str());
  return true;
}

/* the reactions a gillespie chain lists, "x,y{1-3},...", as variable
   indices into ind */
bool gilparse(std::string_view s, std::vector<int> &ind)
{
  /* markov.cpp's one_gill_step holds a rate per reaction in r[1000] */
  const size_t max_reactions = 1000;
  xpp::log(XPP_LOG_DEBUG, "s=|{}|", s);
  ind.clear();
  size_t start = 0;
  for (;;) {
    size_t comma = s.find(',', start);
    std::string_view piece = s.substr(start, comma == std::string_view::npos ? std::string_view::npos : comma - start);
    std::string b;
    int f, i1 = 0, i2 = 0;
    if (!g_namelist(piece, b, f, i1, i2)) {
      xpp::log(XPP_LOG_WARN, "Bad gillespie list {}\n", s);
      return false;
    }
    std::vector<std::string> names;
    if (f == 0) {
      xpp::log(XPP_LOG_DEBUG, "added {}\n", b);
      names.push_back(b);
    } else {
      xpp::log(XPP_LOG_DEBUG, "added {}{{{}-{}}}\n", b, i1, i2);
      for (int id = i1; id <= i2; id++)
        names.push_back(xpp::format("{}{}", b, id));
    }
    for (const std::string &bn : names) {
      int iv = get_var_index(bn.c_str());
      if (iv < 0) {
        xpp::log(XPP_LOG_ERROR, "No such name {}\n", bn);
        return false;
      }
      if (ind.size() >= max_reactions) {
        xpp::log(XPP_LOG_ERROR, "Too many reactions in {} (at most {})\n", s, max_reactions);
        return false;
      }
      ind.push_back(iv);
    }
    if (comma == std::string_view::npos) return true;
    start = comma + 1;
  }
}

/* the next argument of import(...) from i: up to its ',' (0) or ')' (1);
   -1 when s ends first */
int getimpstr(std::string_view s, size_t &i, std::string &out)
{
  size_t end = s.find_first_of(",)", i);
  if (end == std::string_view::npos) return -1;
  out = s.substr(i, end - i);
  int k = s[end] == ')' ? 1 : 0;
  i = end + 1;
  return k;
}

bool import_error()
{
  xpp_log(XPP_LOG_ERROR, "k=import(soname,sofun,nret,var0,w1,...,wm)\n");
  return false;
}

/* import(soname,sofun,nret,var0,w1,...,wm): its library, function,
   number of values, first variable and weight tables */
bool parse_import(std::string_view s, std::string &soname, std::string &sofun, int *n,
                  std::string &vname, std::vector<std::string> &tname)
{
  std::string temp;
  size_t i = s.find('(');
  if (i == std::string_view::npos) return import_error();
  i++;

  if (getimpstr(s, i, soname) != 0) return import_error();
  if (getimpstr(s, i, sofun) != 0) return import_error();
  if (getimpstr(s, i, temp) != 0) return import_error();
  *n = atoi(temp.c_str());
  if (*n <= 0) return import_error();

  int j = getimpstr(s, i, vname);
  if (j < 0) return import_error();
  tname.clear();
  if (j == 1) {
    xpp_log(XPP_LOG_INFO, "No weights....\n");
    return true;
  }
  do {
    j = getimpstr(s, i, temp);
    if (j < 0) return import_error();
    if (tname.size() >= MAXW) {
      xpp::log(XPP_LOG_ERROR, "import: at most {} weights\n", MAXW);
      return false;
    }
    tname.push_back(temp);
  } while (j == 0);
  return true;
}

} // namespace


/* vector(var,length,e|z|p,e|z|p) (spaces removed from str first): the
   first variable, the length and the two ends' kinds */
int get_vector_info(char *str, const char *name,int *root, int *length, int *il, int *ir)
{
  de_space(str);
  std::string_view s(str);
  size_t i=s.find('(');
  i=i==std::string_view::npos?s.size():i+1;
  size_t comma=s.find(',',i);
  std::string temp(s.substr(i,comma==std::string_view::npos?std::string_view::npos:comma-i));
  int ivar=get_var_index(temp.c_str());
  if(ivar<0||comma==std::string_view::npos){
    xpp::log(XPP_LOG_ERROR, " In vector {} , {} is not valid variable\n",
	     name,temp);
    return 0;
  }
  *root=ivar;
  i=comma+1;
  comma=s.find(',',i);
  if(comma==std::string_view::npos){
    xpp::log(XPP_LOG_ERROR, " In vector {} , no ends given\n",name);
    return 0;
  }
  *length=atoi(std::string(s.substr(i,comma-i)).c_str());
  i=comma+1;
  /* the character at k, or 0 past the end */
  auto at=[&s](size_t k){ return k<s.size()?s[k]:'\0'; };
  *il=PERIODIC;
  if(at(i)=='e'|| at(i)=='E')
    *il=EVEN;
  if(at(i)=='z'|| at(i)=='Z')
    *il=ZERO;
  i+=2;
  *ir=PERIODIC;
  if(at(i)=='e'|| at(i)=='E')
    *ir=EVEN;
  if(at(i)=='z'|| at(i)=='Z')
    *ir=ZERO;
  return 1;
}
