#include "my_rhs.h"
#include "expr.h"
#include "dae_fun.h"
#include <stdlib.h> 
#include "getvar.h"
#include "simplenet.h"
#include "form_ode.h"
#include "model.h"

void extra(xpp::Session &s, double *y__y, double t, int nod, int neq)
{
  const xpp::Model &m=s.model();
  const int fix=m.fix_var,nmark=m.nmarkov;
  int i;
  if(nod>=neq)return;
  setvar(s,0,t);
  for(i=0;i<nod;i++)
  setvar(s,i+1,y__y[i]);
  for(i=nod+fix;i<nod+fix+nmark;i++)setvar(s,i+1,y__y[i-fix]);
  for(i=nod;i<nod+fix;i++)
  setvar(s,i+1,xpp::eval_program(s,i));
  /* I dont think this is generally needed  */

  for(i=nod+nmark;i<neq;i++)
  y__y[i]=xpp::eval_program(s,i+fix-nmark);
}

void set_fix_rhs(xpp::Session &s, double t, double *y)
{
  const xpp::Model &m=s.model();
  const int node=m.node,fix=m.fix_var;
  int i;
  setvar(s,0,t);
  for(i=0;i<node;i++)
    setvar(s,i+1,y[i]);
  for(i=0;i<m.nmarkov;i++)
    setvar(s,i+1+node+fix,y[i+node]);
  for(i=node;i<node+fix;i++)
    setvar(s,i+1,xpp::eval_program(s,i));
  xpp::eval_all_nets(s);
}

int my_rhs(xpp::Session &s, double t, double *y, double *ydot, int neq)
{
  const xpp::Model &m=s.model();
  const int node=m.node,fix=m.fix_var;
  int i;
  setvar(s,0,t);
  for(i=0;i<node;i++)
  setvar(s,i+1,y[i]);

  for(i=node;i<node+fix;i++){
  setvar(s,i+1,xpp::eval_program(s,i));
  }
xpp::eval_all_nets(s);
    
    xpp::do_daes(s);
 for(i=0;i<node;i++)
  {
    ydot[i]=xpp::eval_program(s,i);
  }
 if(neq>node)vec_rhs(t,y,ydot,neq);
	
 return(1);
}

void update_based_on_current(xpp::Session &s)
{
  const xpp::Model &m=s.model();
  const int node=m.node,fix=m.fix_var;
  int i;
   for(i=node;i<node+fix;i++)
    setvar(s,i+1,xpp::eval_program(s,i));
    
  xpp::eval_all_nets(s);
}

void fix_only(xpp::Session &s)
{
  const xpp::Model &m=s.model();
  const int node=m.node,fix=m.fix_var;
   int i;
  for(i=node;i<node+fix;i++)
    setvar(s,i+1,xpp::eval_program(s,i));

}

void rhs_only(xpp::Session &s, double *ydot)
{
  const xpp::Model &m=s.model();
  const int node=m.node;
  int i;
  for(i=0;i<node;i++){
    ydot[i]=xpp::eval_program(s,i);
  }
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

