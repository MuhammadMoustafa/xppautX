#include "my_rhs.h"
#include "expr.h"
#include "dae_fun.h"
#include <stdlib.h> 
#include "getvar.h"
#include "simplenet.h"
#include "form_ode.h"
#include "model.h"
#include "expr_native.h"
#include "xpp_globals.h"

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
void fixed_values(const xpp::Model &m, double *c, double *v, xpp::Session *s, Set &&set, Evaluate &&evaluate)
{
  if (m.native_rhs) {
    xpp::mir::call(m.native_rhs,c,v,s,nullptr);
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
void rhs_order(const xpp::Model &m, double t, const double *y, double *ydot, double *c, double *v, xpp::Session *s,
               Set &&set, Evaluate &&evaluate, Between &&between)
{
  state_values(m,t,y,m.node,0,set);
  /* the fixed variables and the equations in one native call (W469): a
     model with native_equations has no network or DAE, so between reads
     and writes nothing they do (do_daes only records that no DAE failed) */
  if (m.native_equations) {
    between();
    xpp::mir::call(m.native_rhs,c,v,s,ydot);
    return;
  }
  fixed_values(m,c,v,s,set,evaluate);
  between();
  equation_values(m,ydot,evaluate);
}
}

void extra(xpp::Session &s, double *y__y, double t, int nod, int neq)
{
  const xpp::Model &m=s.model();
  const int fix=m.fix_var,nmark=m.nmarkov;
  int i;
  if(nod>=neq)return;
  const auto set=[&](int index,double value){setvar(s,index,value);};
  state_values(m,t,y__y,nod,nmark,set);
  fixed_values(m,s.parser.constants.data(),s.parser.variables.data(),&s,set,
               [&](const xpp::Program &p){return xpp::evaluate(s,p);});
  /* I dont think this is generally needed  */

  for(i=nod+nmark;i<neq;i++)
  y__y[i]=xpp::evaluate(s,s.model().programs[i+fix-nmark]);
}

void set_fix_rhs(xpp::Session &s, double t, double *y)
{
  const xpp::Model &m=s.model();
  state_values(m,t,y,m.node,m.nmarkov,[&](int i,double value){setvar(s,i,value);});
  fix_only(s);
  xpp::eval_all_nets(s);
}

int my_rhs(xpp::Session &s, double t, double *y, double *ydot, int neq)
{
  const xpp::Model &m=s.model();
  if (m.native_build || m.jacobian.build) xpp::build_native(s,false); /* a build due starts, finished code is taken: here, never mid-evaluation (W715) */
  rhs_order(m,t,y,ydot,s.parser.constants.data(),s.parser.variables.data(),&s,
            [&](int i,double value){setvar(s,i,value);},
            [&](const xpp::Program &p){return xpp::evaluate(s,p);},
            [&]{xpp::eval_all_nets(s);xpp::do_daes(s);});
  if(neq>m.node) vec_rhs(t,y,ydot,neq);
  return 1;
}

bool pure_rhs_ready(const xpp::Session &s)
{
  return s.model().rhs_impurity.empty() && !s.model().native_build && s.integrator.rhs.function==my_rhs;
}

void pure_rhs(const xpp::Session &s, const double *y, double *ydot, double *c, double *v)
{
  /* the native code only reads s's tables (Model::rhs_impurity) */
  rhs_order(s.model(),0.0,y,ydot,c,v,const_cast<xpp::Session *>(&s),[&](int i,double value){v[i]=value;},
            [&](const xpp::Program &p){return xpp::evaluate_pure(s,p,c,v);},[]{});
}

void update_based_on_current(xpp::Session &s)
{
  fix_only(s);
  xpp::eval_all_nets(s);
}

void fix_only(xpp::Session &s)
{
  const xpp::Model &m=s.model();
  fixed_values(m,s.parser.constants.data(),s.parser.variables.data(),&s,
               [&](int i,double value){setvar(s,i,value);},
               [&](const xpp::Program &p){return xpp::evaluate(s,p);});
}

void rhs_only(xpp::Session &s, double *ydot)
{
  equation_values(s.model(),ydot,[&](const xpp::Program &p){return xpp::evaluate(s,p);});
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
