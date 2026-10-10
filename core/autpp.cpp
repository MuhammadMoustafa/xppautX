#include "solver.h"
#include <array>
#include <stdlib.h>
#include "auto_f2c.h"
#include "auto_c.h" /* func, stpnt, bcnd */
#include "session.h"
#include "odesol2.h"
#include "auto_nox.h"
#include "derived.h"
#include "pp_shoot.h"
#include "tabular.h"  /* redo_all_fun_tables() */
#include "jacobian.h"
#include "load_eqn.h"
#include "expr.h"
#include "my_rhs.h"

/*    Hooks to xpp RHS     */


/* The problem defined functions that run the model (auto_c.h's C++
   section): AUTO calls them with the Session whose run it is. */
int func(xpp::Session &s, integer ndim, const double *u, const integer *icp, const double *par, integer ijac, double *f, double *dfdu, double *dfdp, AutoLib *worker)
{
   if (worker && worker->pure_rhs) {
     auto &c=worker->rhs_constants;
     auto &v=worker->rhs_variables;
     for (int k=0;k<s.auto_state.npar;++k) c[s.auto_state.par_index[k]]=par[k];
     xpp::evaluate_derived(s,c.data(),v.data(),true);
     pure_rhs(s,u,f,c.data(),v.data());
     return 0;
   }
   int i,j;
   std::array<double,NAUTO> zz; /* the store_every steps' scratch */
   /* the right-hand side and the Jacobian only read the point */
   double *x=const_cast<double *>(u);
   for(i=0;i<s.auto_state.npar;i++){
     s.parser.constants[s.auto_state.par_index[i]]=par[i];
     
   }
   xpp::evaluate_derived(s);
   if(auto r=xpp::redo_all_fun_tables(s);!r)xpp::auto_fail(r.error().what);
   s.integrator.rhs(0.0,x,f,ndim);
   if(ijac==1){
     xpp::jacobian(s,0.0,x,f,ndim,s.numerics.singpt_jacobian_epsilon,{xpp::JacobianLayout::ColumnMajor},dfdu);
   }
   if(!xpp::solver_info(s.numerics.method).traits.discrete||s.numerics.store_every==1)return 0;
   for(i=1;i<s.numerics.store_every;i++){
     for(j=0;j<ndim;j++)
       zz[j]=f[j];
     s.integrator.rhs(0.0,zz.data(),f,ndim);
   }

   return 0;

} /* func_ */

int stpnt(xpp::Session &s, integer ndim, doublereal t, doublereal *u, doublereal *par)
{
  int i;

  double p;

  for(i=0;i<s.auto_state.npar;i++)
    par[i] = s.parser.constants[s.auto_state.par_index[i]];

  if(s.auto_state.new_period_flag==0){  
    for(i=0;i<ndim;i++)
      u[i]=s.last_ic[i];
    return 0;
  }

  get_start_period(s,&p);
  par[10]=p;
  if(s.auto_state.homo_flag!=1)get_start_orbit(s,u,t,p,ndim);
  if(s.auto_state.homo_flag==1){

    get_shifted_orbit(s,u,t,p,ndim);
    for(i=0;i<ndim;i++){
      par[11+i]=s.auto_state.homo_l[i];

    }
  }
  if(s.auto_state.homo_flag==2){ /* heteroclinic */
    for(i=0;i<ndim;i++){
      par[11+i]=s.auto_state.homo_l[i];
      par[11+i+ndim]=s.auto_state.homo_r[i];

    }

  }
  return 0;

} /* stpnt_ */

/* Subroutine */ int bcnd(xpp::Session &s, integer ndim, const double *par, const integer *icp, integer nbc, const double *u0, const double *u1, integer ijac, double *fb, double *dbc)
{
 int i;
/* Hooks to the XPP bc parser!! */

 for(i=0;i<s.auto_state.npar;i++){
     s.parser.constants[s.auto_state.par_index[i]]=par[i];
 }

 xpp::evaluate_derived(s);
 if(auto r=xpp::redo_all_fun_tables(s);!r)xpp::auto_fail(r.error().what);
 /* the boundary conditions only read the two ends */
 xpp::do_bc(s,const_cast<double *>(u0),0.0,const_cast<double *>(u1),1.0,fb,nbc);

    return 0;
} /* bcnd_ */

/* AUTO's user routines that xppautX does not supply: stubs, with
   auto_c.h's prototypes (which AUTO calls them through) */
/* Subroutine */ int icnd(integer ndim, const doublereal *par, const integer *icp, integer nint,
	 const doublereal *u, const doublereal *uold, const doublereal *udot,
	 const doublereal *upold, integer ijac,
	 doublereal *fi, doublereal *dint)
{
    return 0;
} /* icnd_ */

/* Subroutine */ int fopt(integer ndim, const doublereal *u, const integer *icp,
	 const doublereal *par, integer ijac,
	 doublereal *fs, doublereal *dfdu, doublereal *dfdp)
{
/*     ---------- ---- */
    return 0;
} /* fopt_ */

/*  Not sure what to do here; I think  do nothing  since IEQUIB is always
    -2 
*/
int pvls (integer ndim, const doublereal *u,
          doublereal *par)
{
  return 0;
}

