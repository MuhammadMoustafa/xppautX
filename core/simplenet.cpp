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

#include "delay_handle.h"
#include "model.h"

namespace xpp {

/* simple network stuff */

namespace {
bool gilparse(const xpp::Session &s, std::string_view list, std::vector<int> &ind);
void fft_conv(Ends ends,int n,double *values,const double *yy,double *fftr,double *ffti,double *dr,double *di);

/* table t's values, NULL for none (-1) */
double *table_values(xpp::Session &s, int t)
{
  return t<0 ? nullptr : s.tables[t].y;
}
} // namespace

namespace {
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
} // namespace

int add_vectorizer(xpp::Session &s, std::string_view name,char *rhs)
{
  int ivar;
  Ends il,ir;
  int ind;
  int len;
  int flag;

  const auto *entry=s.model().name(name);
  ind=entry && entry->kind==xpp::Model::NameKind::Vector && entry->index>=0 && entry->index<s.model().nvector &&
      s.model().vectors[entry->index].name==name ? entry->index : s.model().nvector;
  flag=get_vector_info(s,rhs,name,&ivar,&len,&il,&ir);

  if(flag==0)return 0;
  if(ind>=s.model().nvector)return 0;
    s.model().vectors[ind].root=ivar;
    s.model().vectors[ind].length=len;
    s.model().vectors[ind].il=il;
    s.model().vectors[ind].ir=ir;
    xpp::log(XPP_LOG_INFO, "adding vector {} based on variable {} of length {} ends {} {}\n",
	   name,ivar,len,static_cast<int>(il),static_cast<int>(ir));
 
  return 1;

}  
void add_vectorizer_name(xpp::Session &s, std::string_view name, std::string_view rhs)
{
  s.model().vectors.resize(static_cast<std::size_t>(s.model().nvector)+1);
  s.model().vectors[s.model().nvector].name=name;
  if(add_net_name(s,s.model().nvector,name,1))
    model_failed();
  s.model().nvector++;

}
double vector_value(xpp::Session &s, double x, int i)
{
  const Ends il=s.model().vectors[i].il,ir=s.model().vectors[i].ir;
  const int n=s.model().vectors[i].length,k=static_cast<int>(x);
  int root=s.model().vectors[i].root;
  if((k>=0)&&(k<n))  return s.parser.variables[root+k];
  if(il==Ends::periodic)return s.parser.variables[root+((k+n)%n)];
  if(k<0){
    if(il==Ends::zero)return 0.0;
    return s.parser.variables[root-k-1];
  }
  if(k>=n){
    if(ir==Ends::zero)return 0.0;
    return s.parser.variables[2*n-k-1+root];
  }
  return 0.0;

}  
double network_value(xpp::Session &s, double x, int i)
{
  int j=static_cast<int>(x);
  if(s.model().networks[i].kind==NetworkKind::interpolation){
    return net_interp(s,x,i);
  }
  if(j>=0&&j<s.model().networks[i].n)
    return s.networks[i].values[j];
  return 0.0;
}

namespace {

/* how many arguments a network's call reads (see NetworkForm) */
enum class Sizes { length, length_and_connections };

/* which of simplenet.cpp's readers takes a call's arguments */
enum class Reader { tabled, gillespie, interpolation, extremum, compiled };

/* A form a network's call can take: the keyword its text starts with (the
   text is upper case, blanks removed), what it computes and, for a tabled
   one, the arguments it reads in this order:
     [ends: e|0|z|p] n [ncon] [weights] [index] [delays] root [root2, f]
   ends: how the ends of a convolution are treated; ncon: the number of
   connections of each unit (an fftcon has none: n gives it); matrix: the
   arguments name the columns and then the rows (the network's n is its
   rows); function: the arguments end with root, root2 and f, and each tap
   is f(root,root2) instead of the source's value */
struct NetworkForm {
  std::string_view keyword;
  NetworkKind kind;
  Reader reader = Reader::tabled;
  bool ends = false;
  Sizes sizes = Sizes::length_and_connections;
  bool matrix = false;
  bool weights = false, index = false, delays = false;
  bool function = false;
};

/* the forms: conv, sparse, fconv, fsparse, fftcon, mmult, fmmult, delmmult,
   delsparse (docs/odex.md "network"), then the ones with a reader of their own */
constexpr NetworkForm network_forms[] = {
  {.keyword = "CONV", .kind = NetworkKind::convolution, .ends = true, .weights = true},
  {.keyword = "SPAR", .kind = NetworkKind::sparse, .weights = true, .index = true},
  {.keyword = "FCONV", .kind = NetworkKind::convolution, .ends = true, .weights = true, .function = true},
  {.keyword = "FSPAR", .kind = NetworkKind::sparse, .weights = true, .index = true, .function = true},
  {.keyword = "FFTC", .kind = NetworkKind::fft_convolution, .ends = true, .sizes = Sizes::length, .weights = true},
  {.keyword = "MMUL", .kind = NetworkKind::matrix_product, .matrix = true, .weights = true},
  {.keyword = "FMMUL", .kind = NetworkKind::matrix_product, .matrix = true, .weights = true, .function = true},
  {.keyword = "DELM", .kind = NetworkKind::delayed_matrix_product, .matrix = true, .weights = true, .delays = true},
  {.keyword = "DELS", .kind = NetworkKind::delayed_sparse, .weights = true, .index = true, .delays = true},
  {.keyword = "GILL", .kind = NetworkKind::gillespie, .reader = Reader::gillespie},
  {.keyword = "INTER", .kind = NetworkKind::interpolation, .reader = Reader::interpolation},
  {.keyword = "FINDE", .kind = NetworkKind::extremum, .reader = Reader::extremum},
  {.keyword = "IMPO", .kind = NetworkKind::convolution, .reader = Reader::compiled},
};

/* the form text (upper case, blanks removed) is the call of, null for none */
const NetworkForm *form_of(std::string_view text)
{
  for(const NetworkForm &form:network_forms)
    if(text.starts_with(form.keyword))return &form;
  return nullptr;
}

/* a call's arguments, read one at a time from args up to `sep`: each
   throws the error of what is wrong with it */

/* a count, which must be positive */
int next_positive_int(xpp::Tokens &args, const char *sep)
{
  std::string str=args.text(sep);
  int n=atoi(str.c_str());
  if(n<=0)model_failed(xpp::format("{} must be a positive integer",str));
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
    if(i<0)model_failed(xpp::format("In network {}, {} is not a table",net,name));
    return i;
  }
  int i=get_var_index(s,name);
  if(i<0)model_failed(xpp::format("In {}, {} is not a valid variable",net,name));
  return i;
}

/* table index (called tablename) holds at least needed values: the
   network reads them */
void require_table_length(const xpp::Session &s, const char *net, std::string_view tablename, int index, long long needed)
{
  const long long have=get_lookup_len(s,index);
  if(have<needed)
    model_failed(xpp::format("In {}, {} has {} values; {} are needed",net,tablename,have,needed));
}

/* how many weights a convolution of n values with ncon taps to each side
   reads: the weight of the highest tap that lands on a value (the taps
   past an end that is zero or even land on none) */
long long convolution_weights(Ends ends, int n, int ncon)
{
  const long long reach=ends==Ends::periodic?ncon:std::min<long long>(ncon,ends==Ends::zero?n-1:2LL*n-1);
  return ncon+reach+1;
}

/* a convolution's ends from its first argument */
Ends next_ends(xpp::Tokens &args, NetworkKind kind)
{
  const std::string str=args.text(",");
  const char c=str.empty()?'\0':str[0];
  if(kind==NetworkKind::fft_convolution){
    if(c=='0'||c=='Z')return Ends::zero;
    if(c=='P')return Ends::periodic;
    model_failed(xpp::format("No such fft convolution type {}",str));
  }
  if(c=='E')return Ends::even;
  if(c=='0'||c=='Z')return Ends::zero;
  if(c=='P')return Ends::periodic;
  model_failed(xpp::format("No such convolution type {}",str));
}

/* the arguments of a tabled form's call into the Model's network ind */
void read_tabled(xpp::Session &s, const NetworkForm &form, int ind, const char *name, xpp::Tokens &args)
{
  Network &net=s.model().networks[ind];
  net.kind=form.kind;
  if(form.ends)net.ends=next_ends(args,form.kind);
  const int ntot=next_positive_int(args,",");
  const int ncon=form.sizes==Sizes::length_and_connections?next_positive_int(args,","):ntot;
  std::string wgtname,indname,tauname,rootname,root2name,fname;
  int ivar;
  if(form.weights)net.weight_table=next_index(s,args,name,",",wgtname,NameKind::table);
  if(form.index)net.index_table=next_index(s,args,name,",",indname,NameKind::table);
  if(form.delays)net.taud_table=next_index(s,args,name,",",tauname,NameKind::table);
  if(form.function){
    ivar=next_index(s,args,name,",",rootname,NameKind::variable);
    next_index(s,args,name,",",root2name,NameKind::variable);
    fname=args.text(")");
    int elen;
    if(add_expr(s,xpp::format("{}({},{})",fname,rootname,root2name),net.f.rpn,&elen))
      model_failed(xpp::format("Bad function {}",fname));
    net.applies_function=true;
    /* the compiled names, which evaluate_network points at each tap */
    net.root=net.f.rpn[0];
    net.root2=net.f.rpn[1];
  }
  else {
    ivar=next_index(s,args,name,")",rootname,NameKind::variable);
    net.root=ivar;
  }
  net.n=form.matrix?ncon:ntot;
  net.ncon=form.matrix?ntot:ncon;
  /* every table the network reads has the values it will read */
  if(form.weights){
    const long long needed=form.kind==NetworkKind::convolution?convolution_weights(net.ends,ntot,ncon):
      form.kind==NetworkKind::fft_convolution?(net.ends==Ends::zero?2LL:1LL)*ntot:1LL*net.n*net.ncon;
    require_table_length(s,name,wgtname,net.weight_table,needed);
  }
  if(form.index)require_table_length(s,name,indname,net.index_table,1LL*net.n*net.ncon);
  if(form.delays)require_table_length(s,name,tauname,net.taud_table,1LL*net.n*net.ncon);
  s.networks[ind].values.assign(net.n+1,0.0);
  if(form.kind==NetworkKind::fft_convolution){
    net.ncon=net.ends==Ends::zero?2*ntot:ntot;
    s.networks[ind].fftr.assign(net.ncon+2,0.0);
    s.networks[ind].ffti.assign(net.ncon+2,0.0);
    s.networks[ind].dr.assign(net.ncon+2,0.0);
    s.networks[ind].di.assign(net.ncon+2,0.0);
    update_fft(s,ind);
  }
  if(form.delays)s.model().ndelays=1;
  xpp::log(XPP_LOG_INFO, " Added network {} ({}) {} x {} using {}{}{}{}{}{}\n",
           name,form.keyword,net.n,net.ncon,wgtname,indname.empty()?"":" and ",indname,tauname.empty()?"":" with delays ",tauname,fname.empty()?"":" "+fname);
}

/* z=GILL(meth,rxn list) e.g. z=GILL(meth,r{1-15}): meth is the evaluation
   method (0 the standard one, 1 tau-leap), root the number of reactions,
   values[0] the time of the next reaction, values[1..root] how many
   times each reaction took place, gcom the fixed quantities holding the
   reactions; false for a list that does not parse */
bool read_gillespie(xpp::Session &s, int ind, xpp::Tokens &args)
{
  Network &net=s.model().networks[ind];
  net.kind=NetworkKind::gillespie;
  args.next("(");
  int method=atoi(args.text(",").c_str());
  const std::string list=args.text(")");
  if(method>0){
    xpp::log(XPP_LOG_WARN, " Tau leaping not implemented yet. Changing to 0\n");
    method=0;
  }
  net.gill_method=method;
  if(!gilparse(s,list,net.gcom))return false;
  const int reactions=static_cast<int>(net.gcom.size());
  net.root=reactions;
  net.n=reactions+1;
  net.ncon=-1;
  /* zeroed: the first output row reads them before the first step */
  s.networks[ind].values.assign(reactions+2,0.0);
  s.networks[ind].gill_rates.assign(reactions+1,0.0);
  xpp::log(XPP_LOG_INFO, "Added gillespie chain with {:d} reactions \n",reactions);
  return true;
}

/* z=INTERP(meth,n,root): the n values root..root+n-1 interpolated at x in
   [0,n); the method (0, linear) is the only one */
void read_interpolation(xpp::Session &s, int ind, const char *name, xpp::Tokens &args)
{
  Network &net=s.model().networks[ind];
  net.kind=NetworkKind::interpolation;
  args.next("(");
  args.text(",");
  const int n=atoi(args.text(",").c_str());
  if(n<1)model_failed("Need more than 1 entry for interpolate");
  net.n=n;
  std::string rootname;
  net.root=next_index(s,args,name,")",rootname,NameKind::variable);
  xpp::log(XPP_LOG_INFO, "Added interpolator {} length {:d} on {} \n",name,net.n,rootname);
}

/* z=FINDEXT(type,n,skip,root): the extremum of every skip-th of n variables,
   type 1 the maximum, -1 the minimum, 0 both: values[0..1] the maximum and
   its index, values[2..3] the minimum and its index */
void read_extremum(xpp::Session &s, int ind, const char *name, xpp::Tokens &args)
{
  Network &net=s.model().networks[ind];
  net.kind=NetworkKind::extremum;
  args.next("(");
  const int type=atoi(args.text(",").c_str());
  if(type>1||type<(-1))
    model_failed(xpp::format("In {}, type is -1, 0 or 1, not {}",name,type));
  net.extremum=type==1?Extremum::maximum:type==-1?Extremum::minimum:Extremum::both;
  net.n=atoi(args.text(",").c_str());
  if(net.n<=0)model_failed(xpp::format("In {}, n > 0, not {}",name,net.n));
  net.ncon=atoi(args.text(",").c_str());
  if(net.ncon<=0)model_failed(xpp::format("In {}, skip >= 1, not {}",name,net.ncon));
  std::string rootname;
  net.root=next_index(s,args,name,")",rootname,NameKind::variable);
  s.networks[ind].values.assign(6,0.0);
  xpp::log(XPP_LOG_INFO, " Added findextr {}: type={:d} len={:d}  skip= {:d} using var[{:d}] \n",
           name,type,net.n,net.ncon,net.root);
}

} // namespace

bool add_spec_fun(xpp::Session &s, std::string_view name_text, char *rhs)
{
  /* the name as the messages print it */
  const std::string name_copy(name_text);
  const char *name=name_copy.c_str();
  de_space(rhs);
  xpp::to_upper(rhs);
  const NetworkForm *form=form_of(rhs);
  if(!form)return false;
  const auto *entry=s.model().name(name);
  const int ind=entry && entry->kind==xpp::Model::NameKind::Network && entry->index>=0 && entry->index<s.model().nnetwork &&
      s.model().networks[entry->index].name==name ? entry->index : s.model().nnetwork;
  if(ind>=s.model().nnetwork)
    model_failed(xpp::format("No such name {}",name));
  /* the arguments: NAME(a,b,...) */
  xpp::Tokens args(rhs);
  switch(form->reader){
  case Reader::tabled:
    args.next("(");
    read_tabled(s,*form,ind,name,args);
    return true;
  case Reader::gillespie:
    return read_gillespie(s,ind,args);
  case Reader::interpolation:
    read_interpolation(s,ind,name,args);
    return true;
  case Reader::extremum:
    read_extremum(s,ind,name,args);
    return true;
  case Reader::compiled:
    refuse_compiled_functions("import");
  }
  return false;
}

void add_special_name(xpp::Session &s, std::string_view name, char *rhs)
{
  if(is_network(rhs)){
    xpp::log(XPP_LOG_DEBUG, " netrhs = |{}| \n",rhs);
    const std::size_t index=static_cast<std::size_t>(s.model().nnetwork);
    s.model().networks.resize(index+1);
    s.networks.resize(index+1);
    s.model().networks[index].name=name;
    add_net_name(s,s.model().nnetwork,name,0);
    s.model().nnetwork++;
  }
  else
    xpp::log(XPP_LOG_WARN, " No such special type ...\n");
}

bool is_network(char *s)
{
  de_space(s);
  xpp::to_upper(s);
  return form_of(s)!=nullptr;
}

void eval_all_nets(xpp::Session &s)
{
  int i;
  for(i=0;i<s.model().nnetwork;i++)
    evaluate_network(s,i);
}

namespace {

/* the source unit i's tap j (the weight w[j+ncon]) reads in a convolution
   of n values with these ends: -1 for none */
template <Ends E>
int convolution_source(int i, int j, int n)
{
  const int k=i+j;
  if constexpr(E==Ends::even){
    int m=abs(k);
    if(m>=2*n)return -1;
    return m>=n?abs(2*n-2-m):m;
  }
  else if constexpr(E==Ends::zero)
    return k>=0&&k<n?k:-1;
  else
    return ((k%n)+n)%n;
}

/* calls f with the std::integral_constant of net's ends */
template <class F>
void by_ends(Ends ends, F &&f)
{
  switch(ends){
  case Ends::even: f(std::integral_constant<Ends,Ends::even>{}); break;
  case Ends::zero: f(std::integral_constant<Ends,Ends::zero>{}); break;
  case Ends::periodic: f(std::integral_constant<Ends,Ends::periodic>{}); break;
  }
}

/* values[i] = sum over the taps j=-ncon..ncon, in j order, of
   w[j+ncon]*source(i,k): the convolution of n values */
template <Ends E, class Source>
void convolve(const Network &net, const double *w, double *values, Source source)
{
  const int n=net.n,ncon=net.ncon;
  for(int i=0;i<n;i++){
    double sum=0.0;
    for(int j=-ncon;j<=ncon;j++){
      const int k=convolution_source<E>(i,j,n);
      if(k>=0)sum+=w[j+ncon]*source(i,k,0);
    }
    values[i]=sum;
  }
}

/* the same of the values y themselves: the interior units, whose taps all
   fall inside [0,n), four at a time with a sum each (every unit's sum is
   still added in j order: the same bits, and the compiler packs the
   four), the units at the ends one by one */
template <Ends E>
void convolve_values(const Network &net, const double *w, const double *y, double *values)
{
  const int n=net.n,ncon=net.ncon;
  const int lo=std::min(ncon,n),hi=std::max(lo,n-ncon);
  const auto unit=[&](int i){
    double sum=0.0;
    for(int j=-ncon;j<=ncon;j++){
      const int k=convolution_source<E>(i,j,n);
      if(k>=0)sum+=w[j+ncon]*y[k];
    }
    values[i]=sum;
  };
  int i=0;
  for(;i<lo;i++)unit(i);
  for(;i+4<=hi;i+=4){
    double a0=0.0,a1=0.0,a2=0.0,a3=0.0;
    for(int j=-ncon;j<=ncon;j++){
      const double wj=w[j+ncon];
      a0+=wj*y[i+j];a1+=wj*y[i+j+1];a2+=wj*y[i+j+2];a3+=wj*y[i+j+3];
    }
    values[i]=a0;values[i+1]=a1;values[i+2]=a2;values[i+3]=a3;
  }
  for(;i<n;i++)unit(i);
}

/* values[i] = sum over the connections j of w[ij]*source(i,k,ij) for the
   source k=index[ij] (none when it is negative), ij=i*ncon+j */
template <class Source>
void sparse_product(const Network &net, const double *w, const double *index, double *values, Source source)
{
  const int n=net.n,ncon=net.ncon;
  for(int i=0;i<n;i++){
    double sum=0.0;
    for(int j=0;j<ncon;j++){
      const int ij=i*ncon+j;
      const int k=static_cast<int>(index[ij]);
      if(k>=0)sum+=w[ij]*source(i,k,ij);
    }
    values[i]=sum;
  }
}

/* values[j] = sum over the columns i of w[ij]*source(j,i,ij), ij=j*ncon+i */
template <class Source>
void matrix_product(const Network &net, const double *w, double *values, Source source)
{
  const int n=net.n,ncon=net.ncon;
  for(int j=0;j<n;j++){
    double sum=0.0;
    for(int i=0;i<ncon;i++){
      const int ij=j*ncon+i;
      sum+=w[ij]*source(j,i,ij);
    }
    values[j]=sum;
  }
}

/* the extremum network: the maximum and its index, the minimum and its
   index, of every skip-th of its n variables */
void find_extremum(const Network &net, const double *y, double *values)
{
  const int n=net.n,skip=net.ncon;
  int imax=0,imin=0;
  double ymax=y[0],ymin=y[0];
  if(net.extremum!=Extremum::minimum)
    for(int i=0;i<n;i+=skip)
      if(y[i]>ymax){
        imax=i;
        ymax=y[i];
      }
  if(net.extremum!=Extremum::maximum)
    for(int i=0;i<n;i+=skip)
      if(y[i]<ymin){
        imin=i;
        ymin=y[i];
      }
  values[1]=static_cast<double>(imax);
  values[0]=ymax;
  values[3]=static_cast<double>(imin);
  values[2]=ymin;
}

} // namespace

void evaluate_network(xpp::Session &s, int ind)
{
  /* every network on every step: the definition, values and variables once */
  Network &net=s.model().networks[ind];
  NetworkValues &nv=s.networks[ind];
  const double *const variables=s.parser.variables.data();
  /* a function's root is a compiled name, not a variable */
  const double *const y=net.applies_function?nullptr:variables+net.root;
  const double *const w=table_values(s,net.weight_table);
  const double *const index=table_values(s,net.index_table);
  double *const values=nv.values.data();
  /* a tap that applies the function: f(root+unit,root2+source) */
  const auto applied=[&](int unit,int source,int){
    net.f.rpn[1]=net.root+unit;
    net.f.rpn[0]=net.root2+source;
    return evaluate(s,net.f);
  };
  const auto plain=[y](int,int source,int){ return y[source]; };
  switch(net.kind){
  case NetworkKind::extremum:
    find_extremum(net,y,values);
    break;
  case NetworkKind::interpolation: /* do nothing! */
    break;
  case NetworkKind::gillespie:
    if(net.ncon==-1&&net.gill_method>0&&!nv.gill_ready){
      nv.gill_nu.assign(static_cast<size_t>(net.root)*s.model().node,0.0);
      xpp::make_gill_nu(s,nv.gill_nu.data(),s.model().node,net.root,nv.values.data());
      nv.gill_ready=true;
    }
    if(nv.gill_rates.size() < static_cast<size_t>(net.root + 1))
      nv.gill_rates.resize(static_cast<size_t>(net.root + 1), 0.0);
    xpp::one_gill_step(s,net.gill_method,net.root,net.gcom.data(),nv.values.data(),nv.gill_rates.data());
    break;
  case NetworkKind::convolution:
    by_ends(net.ends,[&](auto ends){
      if(net.applies_function)convolve<ends.value>(net,w,values,applied);
      else convolve_values<ends.value>(net,w,y,values);
    });
    break;
  case NetworkKind::fft_convolution:
    fft_conv(net.ends,net.n,values,variables+net.root,nv.fftr.data(),nv.ffti.data(),nv.dr.data(),nv.di.data());
    break;
  case NetworkKind::sparse:
    if(net.applies_function)sparse_product(net,w,index,values,applied);
    else sparse_product(net,w,index,values,plain);
    break;
  case NetworkKind::matrix_product:
    if(net.applies_function)matrix_product(net,w,values,applied);
    else matrix_product(net,w,values,plain);
    break;
  case NetworkKind::delayed_matrix_product:
  case NetworkKind::delayed_sparse: {
    /* root indexes variables[], where t comes first; get_delay counts
       from the first state variable, as the parser's delay() does */
    const double *const tau=table_values(s,net.taud_table);
    const auto delayed=[&](int,int source,int ij){ return xpp::get_delay(s,source+net.root-1,tau[ij]); };
    if(net.kind==NetworkKind::delayed_matrix_product)matrix_product(net,w,values,delayed);
    else sparse_product(net,w,index,values,delayed);
    break;
  }
}
}

void update_all_ffts(xpp::Session &s)
{
  int i;

  for(i=0;i<s.model().nnetwork;i++)
    if(s.model().networks[i].kind==NetworkKind::fft_convolution)
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
  if(s.model().networks[ind].kind!=NetworkKind::fft_convolution)return;
  const int n=s.model().networks[ind].ncon;
  int n2=n/2;
  for(int i=0;i<n;i++)ffti[i]=0.0;
  for(int i=0;i<=n2;i++)
    fftr[i]=w[n2-i];
  for(int i=1;i<=n-1-n2;i++)
    fftr[n-i]=w[n2+i];
  xpp::fft({fftr,static_cast<size_t>(n)},{ffti,static_cast<size_t>(n)},1,1.0);
}

namespace {

void fft_conv(Ends ends,int n,double *values,const double *yy,double *fftr,double *ffti,double *dr,double *di)
{
  int i;
 double x,y;
 int n2=2*n;
  switch(ends){
  case Ends::periodic:
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
  case Ends::zero:
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
  case Ends::even: /* an fftcon's ends are zero or periodic (add_spec_fun) */
    return;
  }
}

/* parsing stuff to get gillespie code quickly */

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
        model_failed(xpp::format("No such name {}", bn));
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
int get_vector_info(const xpp::Session &ses, char *str, std::string_view name,int *root, int *length, Ends *il, Ends *ir)
{
  de_space(str);
  std::string_view s(str);
  size_t i=s.find('(');
  i=i==std::string_view::npos?s.size():i+1;
  size_t comma=s.find(',',i);
  std::string temp(s.substr(i,comma==std::string_view::npos?std::string_view::npos:comma-i));
  int ivar=get_var_index(ses,temp);
  if(ivar<0||comma==std::string_view::npos){
    model_failed(xpp::format("In vector {}, {} is not a valid variable",name,temp));
  }
  *root=ivar;
  i=comma+1;
  comma=s.find(',',i);
  if(comma==std::string_view::npos){
    model_failed(xpp::format("In vector {}, no ends given",name));
  }
  *length=atoi(std::string(s.substr(i,comma-i)).c_str());
  i=comma+1;
  /* the character at k, or 0 past the end */
  auto at=[&s](size_t k){ return k<s.size()?s[k]:'\0'; };
  *il=Ends::periodic;
  if(at(i)=='e'|| at(i)=='E')
    *il=Ends::even;
  if(at(i)=='z'|| at(i)=='Z')
    *il=Ends::zero;
  i+=2;
  *ir=Ends::periodic;
  if(at(i)=='e'|| at(i)=='E')
    *ir=Ends::even;
  if(at(i)=='z'|| at(i)=='Z')
    *ir=Ends::zero;
  return 1;
}

} // namespace xpp
