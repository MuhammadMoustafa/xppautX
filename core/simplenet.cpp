#include "xpp_util.h"
#include "session.h"
#include "xpp_batch.h"
#include "simplenet.h"
#include "form_ode.h"
#include "xpp_log.h"

#include "markov.h"
#include "expr.h"
#include "xpp_io.h"
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
wgt is the same centred kernel (w[j+k], k its centre) a matching
conv/conv0 network on the same v0 would read; fftcon periodic equals
conv(p,n,n/2,wgt,v0) on that table, and fftcon 0-type equals
conv(0,n,n,wgt,v0) provided wgt's first entry (the one extra tap the
zero-padded table has beyond conv's own 2n-1) is 0.

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

special ydot=import(...) ran a compiled library's function: refused
(compiled functions are not supported, W55)

*/

#include <math.h>
#include <cmath>
#include <stdio.h>

#define EVEN 0
#define ZERO 1
#define PERIODIC 2
#include "delay_handle.h"
#include "model.h"

/* simple network stuff */


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
#define IMPORT  50 /* refused: a compiled library (W55) */

namespace {
bool gilparse(const xpp::Session &s, std::string_view list, std::vector<int> &ind);

/* table t's values, NULL for none (-1) */
double *table_values(xpp::Session &s, int t)
{
  return t<0 ? nullptr : s.tables[t].y;
}
} // namespace

double net_interp(xpp::Session &s, double x, int i)
{
  int jlo=static_cast<int>(x);
  double *y;
  int n=s.model().networks[i].n;
  double dx=x-static_cast<double>(jlo);
  y=&s.parser.variables[s.model().networks[i].root];
  if(jlo<0 || jlo>(n-1))return 0.0; /* out of range */
  return (1-dx)*y[jlo]+dx*y[jlo+1];

}

int add_vectorizer(xpp::Session &s, const char *name,char *rhs)
{
  int i,ivar,il,ir;
  int ind;
  int len;
  int flag;

  for(i=0;i<s.model().nvector;i++)
       if(s.model().vectors[i].name==name)break;

  ind=i;
  flag=get_vector_info(s,rhs,name,&ivar,&len,&il,&ir);

  if(flag==0)return 0;
  
    s.model().vectors[ind].root=ivar;
    s.model().vectors[ind].length=len;
    s.model().vectors[ind].il=il;
    s.model().vectors[ind].ir=ir;
    xpp::log_printf(XPP_LOG_INFO, "adding vector %s based on variable %d of length %d ends %d %d\n",
	   name,ivar,len,il,ir);
 
  return 1;

}  
void add_vectorizer_name(xpp::Session &s, const char *name, const char *rhs)
{
  if(s.model().nvector>=MAXVEC){
    xpp::log_printf(XPP_LOG_ERROR, "Too many vectors \n");
    xpp_model_failed();
  }
  s.model().vectors[s.model().nvector].name=name;
  if(add_net_name(s,s.model().nvector,name,1))
    xpp_model_failed();
  s.model().nvector++;

}
double vector_value(xpp::Session &s, double x, int i)
{
  int il=s.model().vectors[i].il,ir=s.model().vectors[i].ir,n=s.model().vectors[i].length,k=static_cast<int>(x);
  int root=s.model().vectors[i].root;
  if((k>=0)&&(k<n))  return s.parser.variables[root+k];
  if(il==PERIODIC)return s.parser.variables[root+((k+n)%n)];
  if(k<0){
    if(il==ZERO)return 0.0;
    return s.parser.variables[root-k-1];
  }
  if(k>=n){
    if(ir==ZERO)return 0.0;
    return s.parser.variables[2*n-k-1+root];
  }
  return 0.0;

}  
double network_value(xpp::Session &s, double x, int i)
{
  int j=static_cast<int>(x);
  if(s.model().networks[i].type==INTERP){
    return net_interp(s,x,i);
  }
  if(j>=0&&j<s.model().networks[i].n)
    return s.networks[i].values[j];
  return 0.0;
}

namespace {
/* add_spec_fun's arguments, read one at a time from args up to `sep`:
   each logs what is wrong with it and returns a value the caller refuses
   (<= 0 for a count, < 0 for an index) */

/* a count, which must be positive */
int next_positive_int(xpp::Tokens &args, const char *sep)
{
  std::string str=args.text(sep);
  int n=atoi(str.c_str());
  if(n<=0)xpp::log(XPP_LOG_ERROR, " {} must be positive int \n",str);
  return n;
}

/* what a name argument must be */
enum class NameKind { table, variable };

/* a table's or a variable's name in s (into name): its index */
int next_index(const xpp::Session &s, xpp::Tokens &args, const char *net, const char *sep, std::string &name, NameKind kind)
{
  name=args.text(sep);
  if(kind==NameKind::table){
    int i=find_lookup(s,name);
    if(i<0)xpp::log_printf(XPP_LOG_ERROR, "in network %s,  %s is not a table \n",
                   net,name.c_str());
    return i;
  }
  int i=get_var_index(s,name);
  if(i<0)xpp::log_printf(XPP_LOG_ERROR, " In %s , %s is not valid variable\n",
                 net,name.c_str());
  return i;
}

/* the last arguments root,root2,f of the networks that apply a function
   (fconv, fsparse, fmmult): f(root,root2) compiled into the Model's networks[ind].f;
   false after an error */
bool next_pair_function(xpp::Session &s, xpp::Tokens &args, const char *net, int ind, int &ivar, int &ivar2, std::string &fname)
{
  std::string rootname,root2name;
  ivar=next_index(s,args,net,",",rootname,NameKind::variable);
  if(ivar<0)return false;
  ivar2=next_index(s,args,net,",",root2name,NameKind::variable);
  if(ivar2<0)return false;
  fname=args.text(")");
  int elen;
  if(add_expr(s,xpp::format("{}({},{})",fname,rootname,root2name).c_str(),s.model().networks[ind].f.data(),&elen)){
    xpp::log_printf(XPP_LOG_ERROR, " bad function %s \n",fname.c_str());
    return false;
  }
  return true;
}
} // namespace

int add_spec_fun(xpp::Session &s, const char *name, char *rhs)
{
  int i,ind;
  int type;
  int iwgt,itau,iind,ivar,ivar2;
  int ntype,ntot,ncon,ntab;
  std::string str;
  /* tokens of the right-hand side, checked as names after the copy */
  std::string rootname,wgtname,tauname,indname,fname;
  type=is_network(rhs);
    if(type==0)return 0;
  xpp::log_printf(XPP_LOG_DEBUG, "type=%d \n",type);
  for(i=0;i<s.model().nnetwork;i++)
    if(s.model().networks[i].name==name)break;
  ind=i;
  if(ind>=s.model().nnetwork){
    xpp::log_printf(XPP_LOG_ERROR, " No such name %s ?? \n",name);
    return 0;
  }
  /* the arguments: NAME(a,b,...) */
  xpp::Tokens args(rhs);
  switch(type){
  case 1: /* convolution */
    args.next("(");
    str=args.text(",");
    ntype=-1;
    if(str[0]=='E')ntype=CONVE;
    if(str[0]=='0'||str[0]=='Z')ntype=CONV0;
    if(str[0]=='P')ntype=CONVP;
    if(ntype==-1){
      xpp::log(XPP_LOG_ERROR, " No such convolution type {} \n",str);
      return 0;
    }
    ntot=next_positive_int(args,",");
    if(ntot<=0)return 0;
    ncon=next_positive_int(args,",");
    if(ncon<=0)return 0;
    iwgt=next_index(s,args,name,",",wgtname,NameKind::table);
    if(iwgt<0)return 0;
    ivar=next_index(s,args,name,")",rootname,NameKind::variable);
    if(ivar<0)return 0;
    s.networks[ind].values.assign((ntot+1),0.0);
    s.model().networks[ind].weight_table=iwgt;
    s.model().networks[ind].type=ntype;
    s.model().networks[ind].root=ivar;
    s.model().networks[ind].n=ntot;
    s.model().networks[ind].ncon=ncon;
    xpp::log_printf(XPP_LOG_INFO, " Added net %s type %d len=%d x %d using %s var[%d] \n",
	   name,ntype,ntot,ncon,wgtname.c_str(),ivar);
    
    return 1;   
    break;
  case 2: /* sparse */
    args.next("(");
    ntype=SPARSE;
    ntot=next_positive_int(args,",");
    if(ntot<=0)return 0;
    ncon=next_positive_int(args,",");
    if(ncon<=0)return 0;
    iwgt=next_index(s,args,name,",",wgtname,NameKind::table);
    if(iwgt<0)return 0;

    iind=next_index(s,args,name,",",indname,NameKind::table);
    if(iind<0)return 0;
    ivar=next_index(s,args,name,")",rootname,NameKind::variable);
    if(ivar<0)return 0;
 
    s.networks[ind].values.assign((ntot+1),0.0);
    s.model().networks[ind].weight_table=iwgt;
    s.model().networks[ind].index_table=iind;

    s.model().networks[ind].type=ntype;
    s.model().networks[ind].root=ivar;
    s.model().networks[ind].n=ntot;
    s.model().networks[ind].ncon=ncon;
    xpp::log_printf(XPP_LOG_INFO, " Added sparse %s len=%d x %d using %s var[%d]  and %s\n",
	   name,ntot,ncon,wgtname.c_str(),ivar,indname.c_str() );
    return 1;   
    break;
 case 3: /* convolution */
    args.next("(");
    str=args.text(",");
    ntype=-1;
    if(str[0]=='E')ntype=FCONVE;
    if(str[0]=='0'||str[0]=='Z')ntype=FCONV0;
    if(str[0]=='P')ntype=FCONVP;
    if(ntype==-1){
      xpp::log(XPP_LOG_ERROR, " No such convolution type {} \n",str);
      return 0;
    }
    ntot=next_positive_int(args,",");
    if(ntot<=0)return 0;
    ncon=next_positive_int(args,",");
    if(ncon<=0)return 0;
    iwgt=next_index(s,args,name,",",wgtname,NameKind::table);
    if(iwgt<0)return 0;

    if(!next_pair_function(s,args,name,ind,ivar,ivar2,fname))return 0;
    s.networks[ind].values.assign((ntot+1),0.0);
    s.model().networks[ind].weight_table=iwgt;
    s.model().networks[ind].type=ntype;
    s.model().networks[ind].root=s.model().networks[ind].f[0]; /* this is strange - I am adding the compiled names */
    s.model().networks[ind].root2=s.model().networks[ind].f[1];
    s.model().networks[ind].n=ntot;
    s.model().networks[ind].ncon=ncon;
    xpp::log_printf(XPP_LOG_INFO, " Added net %s type %d len=%d x %d using %s %s(var[%d],var[%d]) \n",
	   name,ntype,ntot,ncon,wgtname.c_str(),fname.c_str(),ivar,ivar2);
    return 1;   
    break;
  case 4: /* sparse */
    args.next("(");
    ntype=FSPARSE;
    ntot=next_positive_int(args,",");
    if(ntot<=0)return 0;
    ncon=next_positive_int(args,",");
    if(ncon<=0)return 0;
    iwgt=next_index(s,args,name,",",wgtname,NameKind::table);
    if(iwgt<0)return 0;

    iind=next_index(s,args,name,",",indname,NameKind::table);
    if(iind<0)return 0;

    if(!next_pair_function(s,args,name,ind,ivar,ivar2,fname))return 0;

    s.networks[ind].values.assign((ntot+1),0.0);
    s.model().networks[ind].weight_table=iwgt;
    s.model().networks[ind].index_table=iind;

    s.model().networks[ind].type=ntype;
    s.model().networks[ind].root=s.model().networks[ind].f[0]; /* this is strange - I am adding the compiled names */
    s.model().networks[ind].root2=s.model().networks[ind].f[1];
    s.model().networks[ind].n=ntot;
    s.model().networks[ind].ncon=ncon;
    xpp::log_printf(XPP_LOG_INFO, " Sparse %s len=%d x %d using %s %s(var[%d],var[%d]) and %s\n",
	   name,ntot,ncon,wgtname.c_str(),fname.c_str(),ivar,ivar2,indname.c_str() );
    return 1;   
    break;

  case 5: /* fft convolution */
    args.next("(");
    str=args.text(",");
    ntype=-1;
    if(str[0]=='0'||str[0]=='Z')ntype=FFTCON0;
    if(str[0]=='P')ntype=FFTCONP;
    if(ntype==-1){
      xpp::log(XPP_LOG_ERROR, " No such fft convolution type {} \n",str);
      return 0;
    }
    ntot=next_positive_int(args,",");
    if(ntot<=0)return 0;
   
    iwgt=next_index(s,args,name,",",wgtname,NameKind::table);
    if(iwgt<0)return 0;
    ntab=get_lookup_len(s,iwgt);
    if(type==FFTCONP&&ntab<ntot){
     xpp::log_printf(XPP_LOG_ERROR, " In %s, weight is length %d < %d \n",name,ntab,ntot);
     return 0;
    }
    if(type==FFTCON0&&ntab<(2*ntot)){
     xpp::log_printf(XPP_LOG_ERROR, " In %s, weight is length %d < %d \n",name,ntab,2*ntot);
     return 0;
    }
    ivar=next_index(s,args,name,")",rootname,NameKind::variable);
    if(ivar<0)return 0;
    if(ntype==FFTCON0)
      ncon=2*ntot;
    else
      ncon=ntot;
    s.networks[ind].fftr.assign(ncon+2,0.0);
    s.networks[ind].ffti.assign(ncon+2,0.0);
    s.networks[ind].dr.assign(ncon+2,0.0);
    s.networks[ind].di.assign(ncon+2,0.0);
    s.model().networks[ind].iwgt=iwgt;
    s.networks[ind].values.assign((ntot+1),0.0);
    s.model().networks[ind].weight_table=iwgt;
    s.model().networks[ind].type=ntype;
    s.model().networks[ind].root=ivar;
    s.model().networks[ind].n=ntot;
    s.model().networks[ind].ncon=ncon;
    update_fft(s,ind);

    xpp::log_printf(XPP_LOG_INFO, " Added net %s type %d len=%d x %d using %s var[%d] \n",
	   name,ntype,ntot,ncon,wgtname.c_str(),ivar);
    return 1;   
    break;
  case 6:   /* MMULT    ntot=n,ncon=m  */
    args.next("(");
    ntype=MMULT;
    ntot=next_positive_int(args,",");
    if(ntot<=0)return 0;
    ncon=next_positive_int(args,",");
    if(ncon<=0)return 0;
    iwgt=next_index(s,args,name,",",wgtname,NameKind::table);
    if(iwgt<0)return 0;

    ivar=next_index(s,args,name,")",rootname,NameKind::variable);
    if(ivar<0)return 0;
 
    s.networks[ind].values.assign((ncon+1),0.0);
    s.model().networks[ind].weight_table=iwgt;

    s.model().networks[ind].type=ntype;
    s.model().networks[ind].root=ivar;
    s.model().networks[ind].n=ncon;
    s.model().networks[ind].ncon=ntot;
    xpp::log_printf(XPP_LOG_INFO, " Added mmult %s len=%d x %d using %s var[%d]\n",
	   name,ntot,ncon,wgtname.c_str(),ivar );
    return 1;   
    break;
  case 7:  /* FMMULT */
    args.next("(");
    ntype=FMMULT;
    ntot=next_positive_int(args,",");
    if(ntot<=0)return 0;
    ncon=next_positive_int(args,",");
    if(ncon<=0)return 0;
    iwgt=next_index(s,args,name,",",wgtname,NameKind::table);
    if(iwgt<0)return 0;

    if(!next_pair_function(s,args,name,ind,ivar,ivar2,fname))return 0;
    s.networks[ind].values.assign((ncon+1),0.0);
    s.model().networks[ind].weight_table=iwgt;

    s.model().networks[ind].type=ntype;
    s.model().networks[ind].root=s.model().networks[ind].f[0]; /* this is strange - I am adding the compiled names */
    s.model().networks[ind].root2=s.model().networks[ind].f[1];
    s.model().networks[ind].n=ncon;
    s.model().networks[ind].ncon=ntot;
    xpp::log_printf(XPP_LOG_INFO, " Added fmmult %s len=%d x %d using %s %s(var[%d],var[%d])\n",
	   name,ntot,ncon,wgtname.c_str(),fname.c_str(),ivar,ivar2);
    return 1; 

  case FINDEXT:
    args.next("(");
    str=args.text(",");
    ntype=atoi(str.c_str());
    if(ntype>1||ntype<(-1)){
      xpp::log_printf(XPP_LOG_ERROR, "In %s,  type =-1,0,1 not %d \n",
	     name,ntype);
      return 0;
    }
    str=args.text(",");
    ntot=atoi(str.c_str());
    if(ntot<=0){
      xpp::log_printf(XPP_LOG_ERROR, "In %s,  n>0 not %d \n",
	     name,ntot);
      return 0;
    }
    
    str=args.text(",");
    ncon=atoi(str.c_str());
    if(ncon<=0){
      xpp::log_printf(XPP_LOG_ERROR, "In %s,  skip>=1 not %d \n",
	     name,ncon);
      return 0;
    }
    ivar=next_index(s,args,name,")",rootname,NameKind::variable);
    if(ivar<0)return 0;
    s.networks[ind].values.assign(6,0.0);
    s.model().networks[ind].type=FINDEXT;
    s.model().networks[ind].root=ivar;
    s.model().networks[ind].n=ntot;
    s.model().networks[ind].ncon=ncon;
    s.model().networks[ind].iwgt=ntype;
    xpp::log_printf(XPP_LOG_INFO, " Added findextr %s: type=%d len=%d  skip= %d using var[%d] \n",
	   name,ntype,ntot,ncon,ivar);
    return 1; 

   case 30:
    /* interpolation array 
       z=INTERP(meth,n,root)
    */
    args.next("(");
    str=args.text(",");
    ivar=atoi(str.c_str());
    s.model().networks[ind].type=INTERP;
    s.model().networks[ind].iwgt=ivar;
    str=args.text(",");
    ivar=atoi(str.c_str());
    if(ivar<1){
      xpp::log_printf(XPP_LOG_ERROR, "Need more than 1 entry for interpolate\n");
      return 0;
    }
    s.model().networks[ind].n=ivar; /* # entries in array */
    ivar=next_index(s,args,name,")",rootname,NameKind::variable);
    if(ivar<0)return 0;
    s.model().networks[ind].root=ivar;
    xpp::log_printf(XPP_LOG_INFO, "Added interpolator %s length %d on %s \n",name,s.model().networks[ind].n,rootname.c_str()); 
    return 1;

   case IMPORT:
     refuse_compiled_functions("import");
     return 0;
   case DEL_MUL:

    args.next("(");
    ntype=DEL_MUL;
    ntot=next_positive_int(args,",");
    if(ntot<=0)return 0;
    ncon=next_positive_int(args,",");
    if(ncon<=0)return 0;
    iwgt=next_index(s,args,name,",",wgtname,NameKind::table);
    if(iwgt<0)return 0;
    itau=next_index(s,args,name,",",tauname,NameKind::table);
    if(itau<0)return 0;

    ivar=next_index(s,args,name,")",rootname,NameKind::variable);
    if(ivar<0)return 0;
 
    s.networks[ind].values.assign((ncon+1),0.0);
    s.model().networks[ind].weight_table=iwgt;
    s.model().networks[ind].taud_table=itau;

    s.model().networks[ind].type=ntype;
    s.model().networks[ind].root=ivar;
    s.model().networks[ind].n=ncon;
    s.model().networks[ind].ncon=ntot;
    xpp::log_printf(XPP_LOG_INFO, " Added del_mul %s len=%d x %d using %s var[%d] with delay %s\n",
	   name,ntot,ncon,wgtname.c_str(),ivar,indname.c_str() );
    s.model().ndelays=1;
    return 1;   
    break;
    return 0;
  case DEL_SPAR:
    args.next("(");
    ntype=DEL_SPAR;
    ntot=next_positive_int(args,",");
    if(ntot<=0)return 0;
    ncon=next_positive_int(args,",");
    if(ncon<=0)return 0;
    iwgt=next_index(s,args,name,",",wgtname,NameKind::table);
    if(iwgt<0)return 0;

    iind=next_index(s,args,name,",",indname,NameKind::table);
    if(iind<0)return 0;

    itau=next_index(s,args,name,",",tauname,NameKind::table);
    if(itau<0)return 0;

    ivar=next_index(s,args,name,")",rootname,NameKind::variable);
    if(ivar<0)return 0;
 
    s.networks[ind].values.assign((ntot+1),0.0);
    s.model().networks[ind].weight_table=iwgt;
    s.model().networks[ind].index_table=iind;
    s.model().networks[ind].taud_table=itau;
    s.model().networks[ind].type=ntype;
    s.model().networks[ind].root=ivar;
    s.model().networks[ind].n=ntot;
    s.model().networks[ind].ncon=ncon;
    xpp::log_printf(XPP_LOG_INFO, " Added sparse %s len=%d x %d using %s var[%d]  and %s with dely %s\n",
	   name,ntot,ncon,wgtname.c_str(),ivar,indname.c_str(),tauname.c_str() );
    s.model().ndelays=1;
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
       
    args.next("(");
    str=args.text(",");
    ivar=atoi(str.c_str());
    str=args.text(")");
    s.model().networks[ind].type=GILLTYPE;
    if(ivar>0){
      xpp::log_printf(XPP_LOG_WARN, " Tau leaping not implemented yet. Changing to 0\n");
      ivar=0;
    }
    s.model().networks[ind].iwgt=ivar;
    if(gilparse(s,str,s.model().networks[ind].gcom)==0)
      return 0;
    ivar2=static_cast<int>(s.model().networks[ind].gcom.size());
    s.model().networks[ind].root=ivar2;
    s.model().networks[ind].n=ivar2+1;
    s.model().networks[ind].ncon=-1;
    /* zeroed: the first output row reads them before the first step */
    s.networks[ind].values.assign(ivar2+2,0.0);
    xpp::log_printf(XPP_LOG_INFO, "Added gillespie chain with %d reactions \n",ivar2);
    return 1;

  }
  return 0;
}
void add_special_name(xpp::Session &s, const char *name, char *rhs)
{
  if(is_network(rhs)){
    xpp::log_printf(XPP_LOG_DEBUG, " netrhs = |%s| \n",rhs);
    if(s.model().nnetwork>=MAXNET){
      return;
    }
    s.model().networks[s.model().nnetwork].name=name;
    add_net_name(s,s.model().nnetwork,name,0);
    s.model().nnetwork++;
  }
  else
    xpp::log_printf(XPP_LOG_WARN, " No such special type ...\n");
}

int is_network(char *s)
{
  de_space(s);
  xpp::to_upper(s);
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
  return 0;
}

void eval_all_nets(xpp::Session &s)
{
  int i;
  for(i=0;i<s.model().nnetwork;i++)
    evaluate_network(s,i);
}

void evaluate_network(xpp::Session &s, int ind)
{
   /* every network on every step: the definition, values and variables once */
   Network &net=s.model().networks[ind];
   NetworkValues &nv=s.networks[ind];
   double *const variables=s.parser.variables.data();
   int i,j,k,ij;
   int imin,imax;
   double ymin,ymax;
   int skip;
   int mmt;
   int in0;
   double sum,z;
   int n=net.n,*f;
   int ncon=net.ncon;
   double *w,*y,*cc,*values,*tau;
   int twon=2*n,root=net.root,root2=net.root2;
   cc=table_values(s,net.index_table);
   w=table_values(s,net.weight_table);
   values=nv.values.data();
   switch(net.type){
   case FINDEXT:
     y=&variables[root];
     mmt=net.iwgt;
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
     values[1]=static_cast<double>(imax);
     values[0]=ymax;
     values[3]=static_cast<double>(imin);
     values[2]=ymin;
     break;
   case INTERP: /* do nothing! */ 
     break;
   case GILLTYPE:
     if(net.ncon==-1&&net.iwgt>0&&!nv.gill_ready){
       nv.gill_nu.assign(static_cast<size_t>(net.root)*s.model().node,0.0);
       xpp::make_gill_nu(s,nv.gill_nu.data(),s.model().node,net.root,nv.values.data());
       nv.gill_ready=true;
     }
     xpp::one_gill_step(s,net.iwgt,net.root,net.gcom.data(),nv.values.data());
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
    fft_conv(0,n,values,y,nv.fftr.data(),nv.ffti.data(),nv.dr.data(),nv.di.data());
    break;
   
   case FFTCON0:
     y=&variables[root];           
      fft_conv(1,n,values,y,nv.fftr.data(),nv.ffti.data(),nv.dr.data(),nv.di.data());
    break;

   case DEL_MUL:
     tau=table_values(s,net.taud_table);
     in0=net.root;
     for(j=0;j<n;j++){
       sum=0.0;
       for(i=0;i<ncon;i++){
         ij=j*ncon+i;
	 /* root indexes variables[], where t comes first; get_delay counts
	    from the first state variable, as the parser's delay() does */
	 sum+=(w[ij]*xpp::get_delay(s,i+in0-1,tau[ij]));
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
     tau=table_values(s,net.taud_table);
     in0=net.root;
      for(i=0;i<n;i++){
       sum=0.0;
       for(j=0;j<ncon;j++){
	 ij=i*ncon+j;
	 k=static_cast<int>(cc[ij]);
         if(k>=0)
	   sum+=(w[ij]*xpp::get_delay(s,k+in0-1,tau[ij])); /* as in DEL_MUL */
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
	 k=static_cast<int>(cc[ij]);
         if(k>=0)
	   sum+=(w[ij]*y[k]);
       }
       values[i]=sum;
     }
     break;

     /*     f stuff  */           
   case FCONVE:
     f=net.f.data();

     for(i=0;i<n;i++){
       sum=0.0;
       f[1]=root+i;
       for(j=-ncon;j<=ncon;j++){
	 k=abs(i+j);
	 if(k<twon){
	   if(k>=n)k=abs(twon-2-k);
           f[0]=root2+k;

	   z=evaluate(s,f);
	   sum+=(w[j+ncon]*z);
	 }
       }
       values[i]=sum;
     }
     break;
   case FCONV0:
     f=net.f.data();
     
     for(i=0;i<n;i++){
       sum=0.0;
       f[1]=root+i;
       for(j=-ncon;j<=ncon;j++){
	 k=i+j;
	 if(k<n&&k>=0){
	
	   f[0]=root2+k;
	   z=evaluate(s,f);
	   sum+=(w[j+ncon]*z);
	 }
       }
       values[i]=sum;
     }
     break;
   case FCONVP:
     f=net.f.data();

     for(i=0;i<n;i++){
       f[1]=root+i;
       sum=0.0;
       for(j=-ncon;j<=ncon;j++){
	 k=((twon+i+j)%n);
	 f[0]=root2+k;
	 z=evaluate(s,f);
	 sum+=(w[j+ncon]*z);
       }
       values[i]=sum;
     }
     break;
   case FSPARSE:
     f=net.f.data();

     for(i=0;i<n;i++){
       f[1]=root+i;
       sum=0.0;
       for(j=0;j<ncon;j++){
	 ij=i*ncon+j;
	 k=static_cast<int>(cc[ij]);
         if(k>=0){
	   f[0]=root2+k;
	   z=evaluate(s,f);
	   sum+=(w[ij]*z);
	 }
       }
       values[i]=sum;
     }
     break;
   case FMMULT:
     f=net.f.data();

     for(j=0;j<n;j++){

       f[1]=root+j;

       sum=0.0;
       for(i=0;i<ncon;i++){
	 ij=j*ncon+i;

         f[0]=root2+i;
         
	 z=evaluate(s,f);

	 sum+=(w[ij]*z);
       }

       values[j]=sum;
     }
     break;
   }
}

void update_all_ffts(xpp::Session &s)
{
  int i;

  for(i=0;i<s.model().nnetwork;i++)
    if(s.model().networks[i].type==FFTCON0||s.model().networks[i].type==FFTCONP)
      update_fft(s,i);
}
/*
 the weight table is the same centred kernel a matching conv/conv0
 network reads (w[j+k] for lag j, k the table's centre), but with no
 extra unused or out-of-range entry: its length is exactly n (FFTCONP,
 n=the network's n) or 2n (FFTCON0, n=2 times its n), n2=n/2 its centre
 (the direct sum's own ncon, w[j+n2] its weight at lag j). Circular
 convolution by FFT needs the kernel negated in lag and reordered into
 the FFT's own bin order (fftr[i] is the kernel's value at circular lag
 i for i<=n2, at lag i-n otherwise; conv's value[i]=sum_j w[j+n2]
 y[i+j] is sum_k fftr[k] y[i-k] with k=-j mod n):
   fftr[i]      = w[n2-i]  for i = 0 .. n2        (lags 0 .. -n2)
   fftr[n-i]    = w[n2+i]  for i = 1 .. n-1-n2     (lags -1 .. n2-n = -(n-n2))
 which together read w[0..n-1] exactly once each: for n even, the two
 ranges meet without a shared or skipped index (n-1-n2 = n2-1); for n
 odd they are a plain reflection about the centre (n-1-n2 = n2).
*/
void update_fft(xpp::Session &s, int ind)
{
  double *w=table_values(s,s.model().networks[ind].weight_table);
  double *fftr=s.networks[ind].fftr.data();
  double *ffti=s.networks[ind].ffti.data();
  int type=s.model().networks[ind].type;
  int n;
  if(type==FFTCONP)
    n=s.model().networks[ind].n;
  else if(type==FFTCON0)
    n=2*s.model().networks[ind].n;
  else
    return;
  int n2=n/2;
  for(int i=0;i<n;i++)ffti[i]=0.0;
  for(int i=0;i<=n2;i++)
    fftr[i]=w[n2-i];
  for(int i=1;i<=n-1-n2;i++)
    fftr[n-i]=w[n2+i];
  xpp::fft({fftr,static_cast<size_t>(n)},{ffti,static_cast<size_t>(n)},1,1.0);
}

void fft_conv(int it,int n,double *values,double *yy,double *fftr,double *ffti,double *dr,double *di)
{
  int i;
 double x,y;
 int n2=2*n;
  switch(it){
  case 0:
    for(i=0;i<n;i++){
      di[i]=0.0;
      dr[i]=yy[i];

    }
    
    xpp::fft({dr,static_cast<size_t>(n)},{di,static_cast<size_t>(n)},1,1.0/std::sqrt(static_cast<double>(n)));

    for(i=0;i<n;i++){
      x=dr[i]*fftr[i]-di[i]*ffti[i];
      y=dr[i]*ffti[i]+di[i]*fftr[i];
      dr[i]=x;
      di[i]=y;
    } 
   
    xpp::fft({dr,static_cast<size_t>(n)},{di,static_cast<size_t>(n)},-1,1.0/std::sqrt(static_cast<double>(n)));
    for(i=0;i<n;i++)
      values[i]=dr[i];
   
    return;
  case 1:
    for(i=0;i<n2;i++){
      di[i]=0.0;
      if(i<n)
	dr[i]=yy[i];
      else
	dr[i]=0.0;
    }
    xpp::fft({dr,static_cast<size_t>(n2)},{di,static_cast<size_t>(n2)},1,1.0/std::sqrt(static_cast<double>(n2)));
    for(i=0;i<n2;i++){
      x=dr[i]*fftr[i]-di[i]*ffti[i];
      y=dr[i]*ffti[i]+di[i]*fftr[i];
      dr[i]=x;
      di[i]=y;
    } 
    xpp::fft({dr,static_cast<size_t>(n2)},{di,static_cast<size_t>(n2)},-1,1.0/std::sqrt(static_cast<double>(n2)));
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
bool gilparse(const xpp::Session &s, std::string_view list, std::vector<int> &ind)
{
  /* markov.cpp's one_gill_step holds a rate per reaction in r[1000] */
  const size_t max_reactions = 1000;
  xpp::log(XPP_LOG_DEBUG, "s=|{}|", list);
  ind.clear();
  size_t start = 0;
  for (;;) {
    size_t comma = list.find(',', start);
    std::string_view piece = list.substr(start, comma == std::string_view::npos ? std::string_view::npos : comma - start);
    std::string b;
    int f, i1 = 0, i2 = 0;
    if (!g_namelist(piece, b, f, i1, i2)) {
      xpp::log(XPP_LOG_WARN, "Bad gillespie list {}\n", list);
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
      int iv = get_var_index(s,bn);
      if (iv < 0) {
        xpp::log(XPP_LOG_ERROR, "No such name {}\n", bn);
        return false;
      }
      if (ind.size() >= max_reactions) {
        xpp::log(XPP_LOG_ERROR, "Too many reactions in {} (at most {})\n", list, max_reactions);
        return false;
      }
      ind.push_back(iv);
    }
    if (comma == std::string_view::npos) return true;
    start = comma + 1;
  }
}

} // namespace

/* vector(var,length,e|z|p,e|z|p) (spaces removed from str first): the
   first variable, the length and the two ends' kinds */
int get_vector_info(const xpp::Session &ses, char *str, const char *name,int *root, int *length, int *il, int *ir)
{
  de_space(str);
  std::string_view s(str);
  size_t i=s.find('(');
  i=i==std::string_view::npos?s.size():i+1;
  size_t comma=s.find(',',i);
  std::string temp(s.substr(i,comma==std::string_view::npos?std::string_view::npos:comma-i));
  int ivar=get_var_index(ses,temp);
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
