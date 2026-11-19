#include "my_rhs.h"
#include "expr_tables.h"
#include <type_traits>
#include <cstddef>
#include <cmath>
#include <algorithm>
#include "delay_handle.h"
#include "markov.h"
#include "xpp_math.h"
#include "integrate.h"
#include "expr.h"
#include "dae_fun.h"
#include <stdlib.h> 
#include <memory>
#include <string>
#include <vector>
#include "getvar.h"
#include "simplenet.h"
#include "model.h"
#include "model_native.h"
#include "expr_program.h"
#include "run_parts.h"
#include "native_code.h"

/* ---- the networks a right-hand side evaluates (simplenet.h: the Model's networks, read by simplenet.cpp) ---- */
namespace xpp {

namespace {

double net_interp(xpp::RunParts &s, double x, int i)
{
  int jlo=static_cast<int>(x);
  double *y;
  int n=s.model().networks[i].n;
  double dx=x-static_cast<double>(jlo);
  y=&s.parser.variables[s.model().networks[i].root];
  if(jlo<0 || jlo>(n-1))return 0.0; /* out of range */
  return (1-dx)*y[jlo]+dx*y[jlo+1];

}

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

double vector_value(xpp::RunParts &s, double x, int i)
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

double network_value(xpp::RunParts &s, double x, int i)
{
  int j=static_cast<int>(x);
  if(s.model().networks[i].kind==NetworkKind::interpolation){
    return net_interp(s,x,i);
  }
  if(j>=0&&j<s.model().networks[i].n)
    return s.networks[i].values[j];
  return 0.0;
}

void evaluate_network(xpp::RunParts &s, int ind)
{
  /* every network on every step: the definition, values and variables once */
  Network &net=s.model().networks[ind];
  NetworkValues &nv=s.networks[ind];
  const double *const variables=s.parser.variables.data();
  /* a function's root is a compiled name, not a variable */
  const double *const y=net.applies_function?nullptr:variables+net.root;
  const double *const w=table_values(s.tables,net.weight_table);
  const double *const index=table_values(s.tables,net.index_table);
  double *const values=nv.values.data();
  /* a tap that applies the function: f(root+unit,root2+source) */
  const auto applied=[&](int unit,int source,int){
    net.f.rpn[1]=net.root+unit;
    net.f.rpn[0]=net.root2+source;
    /* the interpreter's instructions of the two operands with them (Program::ops is made at the first evaluation) */
    if(!net.f.ops.empty()){
      net.f.ops[0].a=net.f.rpn[0]%MAXTYPE;
      net.f.ops[1].a=net.f.rpn[1]%MAXTYPE;
    }
    return evaluate(s.parser,net.f);
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
    const double *const tau=table_values(s.tables,net.taud_table);
    const auto delayed=[&](int,int source,int ij){ return xpp::get_delay(s,source+net.root-1,tau[ij]); };
    if(net.kind==NetworkKind::delayed_matrix_product)matrix_product(net,w,values,delayed);
    else sparse_product(net,w,index,values,delayed);
    break;
  }
}
}

void eval_all_nets(xpp::RunParts &s)
{
  int i;
  for(i=0;i<s.model().nnetwork;i++)
    evaluate_network(s,i);
}

} // namespace xpp

namespace {
// Session evaluation and autoX's private arrays share the same state/fixed/RHS order.
template <class Set>
void state_values(const xpp::Model &m, double t, const double *y, int node, int markov, Set &&set)
{
  set(0,t);
  for (int i=0;i<node;++i) set(i+1,y[i]);
  for (int i=0;i<markov;++i) set(i+1+node+m.fix_var,y[i+node]);
}

/* The fixed variables into the variables v, from the constants c (the
   Session s's, or autoX's own copies): by the one native function
   translate_model made for the right-hand side (W468), else each program in
   order. Both read the values v holds, whatever node states the caller set
   (W715: a fixed variable's own function is not compiled when p_rhs
   computes it, so this is the one way to evaluate them). */
template <class Set, class Evaluate>
void fixed_values(const xpp::Model &m, double *c, double *v, xpp::ParserState *s, std::uint64_t *native_work, Set &&set, Evaluate &&evaluate)
{
  if (m.native_rhs) {
    if (native_work) *native_work+=m.native_rhs_nodes;
    xpp::native::call(m.native_rhs,c,v,s,nullptr);
    return;
  }
  for (int i=m.node;i<m.node+m.fix_var;++i) set(i+1,evaluate(m.programs[i]));
}

template <class Evaluate>
void equation_values(const xpp::Model &m, double *ydot, Evaluate &&evaluate)
{
  for (int i=0;i<m.node;++i) ydot[i]=evaluate(m.programs[i]);
}

template <class Set, class Evaluate, class Between>
void rhs_order(const xpp::Model &m, double t, const double *y, double *ydot, double *c, double *v, xpp::ParserState *s, std::uint64_t *native_work,
               Set &&set, Evaluate &&evaluate, Between &&between)
{
  state_values(m,t,y,m.node,0,set);
  /* the fixed variables and the equations in one native call (W469): a
     model with native_equations has no network or DAE, so between reads
     and writes nothing they do (do_daes only records that no DAE failed) */
  if (m.native_equations) {
    between();
    if (native_work) *native_work+=m.native_rhs_nodes;
    xpp::native::call(m.native_rhs,c,v,s,ydot);
    return;
  }
  fixed_values(m,c,v,s,native_work,set,evaluate);
  between();
  equation_values(m,ydot,evaluate);
}
}

void extra(xpp::RunParts &s, double *y__y, double t, int nod, int neq)
{
  const xpp::Model &m=s.model();
  const int fix=m.fix_var,nmark=m.nmarkov;
  int i;
  if(nod>=neq)return;
  const auto set=[&](int index,double value){setvar(s.parser,index,value);};
  state_values(m,t,y__y,nod,nmark,set);
  fixed_values(m,s.parser.constants.data(),s.parser.variables.data(),&s.parser,&s.parser.native_work,set,
               [&](const xpp::Program &p){return xpp::evaluate(s.parser,p);});
  /* I dont think this is generally needed  */

  for(i=nod+nmark;i<neq;i++)
  y__y[i]=xpp::evaluate(s.parser,s.model().programs[i+fix-nmark]);
}

void set_fix_rhs(xpp::RunParts &s, double t, double *y)
{
  const xpp::Model &m=s.model();
  state_values(m,t,y,m.node,m.nmarkov,[&](int i,double value){setvar(s.parser,i,value);});
  fix_only(s);
  xpp::eval_all_nets(s);
}

int my_rhs(xpp::RunParts &s, double t, double *y, double *ydot, int neq)
{
  const xpp::Model &m=s.model();
  /* a build due starts, finished code is taken: here, never mid-evaluation (W715); asked at every call, so first the cheap question */
  const auto attention=[&](const std::shared_ptr<xpp::NativeBuild> &b){return b && b->needs_attention(s.parser.interpreted_ops,s.parser.native_work);};
  if (attention(m.native_build) || attention(m.jacobian.build)) xpp::build_native(s.session_parts,false);
  rhs_order(m,t,y,ydot,s.parser.constants.data(),s.parser.variables.data(),&s.parser,&s.parser.native_work,
            [&](int i,double value){setvar(s.parser,i,value);},
            [&](const xpp::Program &p){return xpp::evaluate(s.parser,p);},
            [&]{xpp::eval_all_nets(s);xpp::do_daes(s);});
  if(neq>m.node) vec_rhs(t,y,ydot,neq);
  return 1;
}

bool pure_rhs_ready(const xpp::RunParts &s)
{
  return s.model().rhs_impurity.empty() && !s.model().native_build && s.integrator->rhs.function==my_rhs;
}

void pure_rhs(const xpp::RunParts &s, const double *y, double *ydot, double *c, double *v)
{
  /* the native code only reads s's tables (Model::rhs_impurity); a collocation worker counts no native_work, which the
     core thread's counter alone keeps (every worker writing one counter ping-ponged its cache line, 60% of pd_down's cache misses) */
  rhs_order(s.model(),0.0,y,ydot,c,v,&s.parser,nullptr,[&](int i,double value){v[i]=value;},
            [&](const xpp::Program &p){return xpp::evaluate_pure(s.parser,p,c,v);},[]{});
}

void update_based_on_current(xpp::RunParts &s)
{
  fix_only(s);
  xpp::eval_all_nets(s);
}

void fix_only(xpp::RunParts &s)
{
  const xpp::Model &m=s.model();
  fixed_values(m,s.parser.constants.data(),s.parser.variables.data(),&s.parser,&s.parser.native_work,
               [&](int i,double value){setvar(s.parser,i,value);},
               [&](const xpp::Program &p){return xpp::evaluate(s.parser,p);});
}

void rhs_only(xpp::RunParts &s, double *ydot)
{
  equation_values(s.model(),ydot,[&](const xpp::Program &p){return xpp::evaluate(s.parser,p);});
}
 
void vec_rhs(double t, double *y, double *ydot, int neq)
{

}

/***    
    This is the order in which quantities are evaluated
    
1.  Fixed variables
2.  network stuff 
3.  DAEs
4.  External C code
5.  RH sides of the ODEs

For Auxilliary stuff

external C code is not evaluated but fixed are

***/
