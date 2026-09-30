#include <array>
#include <stdlib.h>
#include "auto_f2c.h" 
#include "session.h"
#include "odesol2.h"
#include "auto_nox.h"
#include "derived.h"
#include "pp_shoot.h"
#include "tabular.h"  /* redo_all_fun_tables() */
#include "gear.h"     /* getjactrans() */
#include "load_eqn.h"
#include "expr.h"

/*    Hooks to xpp RHS     */


/* AUTO calls these through auto_c.h's "problem defined functions"
   prototypes; give them C linkage so the callers (autlib3.cpp,
   autlib5.cpp) find the unmangled symbol regardless of which
   translation unit's (differently const-qualified) prototype is in
   scope where each is called from. */
extern "C" int func(integer ndim, double *u, integer *icp, double *par, integer ijac, double *f, double *dfdu, double *dfdp)
{
   int i,j;
   std::array<double,NAUTO> zz,y,yp,xp; /* getjactrans's scratch, and the NJMP steps' */
   for(i=0;i<xpp::session().auto_state.npar;i++){
     xpp::session().parser.constants[xpp::session().auto_state.par_index[i]]=par[i];
     
   }
   evaluate_derived();
   if(auto r=redo_all_fun_tables();!r)xpp::auto_fail(r.error().what);
   xpp::session().integrator.rhs(0.0,u,f,ndim);
   if(ijac==1){
     getjactrans(xpp::session(),u,y.data(),yp.data(),xp.data(),xpp::session().numerics.newt_err,dfdu,ndim);
   }
   if(xpp::session().numerics.method>0||xpp::session().numerics.njmp==1)return 0;
   for(i=1;i<xpp::session().numerics.njmp;i++){
     for(j=0;j<ndim;j++)
       zz[j]=f[j];
     xpp::session().integrator.rhs(0.0,zz.data(),f,ndim);
   }

   return 0;

} /* func_ */

extern "C" int stpnt(integer ndim, doublereal t, doublereal *u, doublereal *par)
{
  xpp::Session &s=xpp::session();
  int i;

  double p;

  for(i=0;i<s.auto_state.npar;i++)
    par[i] = s.parser.constants[s.auto_state.par_index[i]];

  if(s.auto_state.new_period_flag==0){  
    for(i=0;i<ndim;i++)
      u[i]=s.last_ic[i];
    return 0;
  }

  get_start_period(&p);
  par[10]=p;
  if(s.auto_state.homo_flag!=1)get_start_orbit(u,t,p,ndim);
  if(s.auto_state.homo_flag==1){

    get_shifted_orbit(u,t,p,ndim);
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

/* Subroutine */ extern "C" int bcnd(integer ndim, double *par, integer *icp, integer nbc, double *u0, double *u1, integer ijac, double *fb, double *dbc)
{
 int i;
/* Hooks to the XPP bc parser!! */

 for(i=0;i<xpp::session().auto_state.npar;i++){
     xpp::session().parser.constants[xpp::session().auto_state.par_index[i]]=par[i];
 }

 evaluate_derived();
 if(auto r=redo_all_fun_tables();!r)xpp::auto_fail(r.error().what);
 do_bc(xpp::session(),u0,0.0,u1,1.0,fb,nbc);

    return 0;
} /* bcnd_ */

/* AUTO's user routines that xppautX does not supply: stubs, with
   auto_c.h's prototypes (which AUTO calls them through) */
/* Subroutine */ extern "C" int icnd(integer ndim, const doublereal *par, const integer *icp, integer nint,
	 const doublereal *u, const doublereal *uold, const doublereal *udot,
	 const doublereal *upold, integer ijac,
	 doublereal *fi, doublereal *dint)
{
    return 0;
} /* icnd_ */

/* Subroutine */ extern "C" int fopt(integer ndim, const doublereal *u, const integer *icp,
	 const doublereal *par, integer ijac,
	 doublereal *fs, doublereal *dfdu, doublereal *dfdp)
{
/*     ---------- ---- */
    return 0;
} /* fopt_ */

/*  Not sure what to do here; I think  do nothing  since IEQUIB is always
    -2 
*/
extern "C" int pvls (integer ndim, const doublereal *u,
          doublereal *par)
{
  return 0;
}

