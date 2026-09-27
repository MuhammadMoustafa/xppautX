#include "my_rhs.h"
#include "parserslow.h"
#include "dae_fun.h"
#include "extra.h"
#include <stdlib.h> 
#include "getvar.h"
#include "simplenet.h"
#include "form_ode.h"
#include "model.h"

void extra(double *y__y, double t, int nod, int neq)
{
  xpp::Model &m=xpp::model();
  const int fix=m.fix_var,nmark=m.nmarkov;
  int i;
  if(nod>=neq)return;
  SETVAR(0,t);
  for(i=0;i<nod;i++)
  SETVAR(i+1,y__y[i]);
  for(i=nod+fix;i<nod+fix+nmark;i++)SETVAR(i+1,y__y[i-fix]);
  for(i=nod;i<nod+fix;i++)
  SETVAR(i+1,evaluate(m.programs[i].data()));
  /* I dont think this is generally needed  */

  for(i=nod+nmark;i<neq;i++)
  y__y[i]=evaluate(m.programs[i+fix-nmark].data());
}

void set_fix_rhs(double t, double *y)
{
  xpp::Model &m=xpp::model();
  const int node=m.node,fix=m.fix_var;
  int i;
  SETVAR(0,t);
  for(i=0;i<node;i++)
    SETVAR(i+1,y[i]);
  for(i=0;i<m.nmarkov;i++)
    SETVAR(i+1+node+fix,y[i+node]);
  for(i=node;i<node+fix;i++)
    SETVAR(i+1,evaluate(m.programs[i].data()));
  eval_all_nets();

  do_in_out(); 
}

int my_rhs(double t, double *y, double *ydot, int neq)
{
  xpp::Model &m=xpp::model();
  const int node=m.node,fix=m.fix_var;
  int i;
  SETVAR(0,t);
  for(i=0;i<node;i++)
  SETVAR(i+1,y[i]);

  for(i=node;i<node+fix;i++){
  SETVAR(i+1,evaluate(m.programs[i].data()));
  }
eval_all_nets();
    
    do_daes();

    do_in_out(); 
 for(i=0;i<node;i++)
  {
    ydot[i]=evaluate(m.programs[i].data());
  }
 if(neq>node)vec_rhs(t,y,ydot,neq);
	
 return(1);
}

void update_based_on_current()
{
  xpp::Model &m=xpp::model();
  const int node=m.node,fix=m.fix_var;
  int i;
   for(i=node;i<node+fix;i++)
    SETVAR(i+1,evaluate(m.programs[i].data()));
    
  eval_all_nets();
  do_in_out(); 
}

void fix_only()
{
  xpp::Model &m=xpp::model();
  const int node=m.node,fix=m.fix_var;
   int i;
  for(i=node;i<node+fix;i++)
    SETVAR(i+1,evaluate(m.programs[i].data()));

}

void rhs_only(double *y,double *ydot)
{
  xpp::Model &m=xpp::model();
  const int node=m.node;
  int i;
  for(i=0;i<node;i++){
    ydot[i]=evaluate(m.programs[i].data());
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

