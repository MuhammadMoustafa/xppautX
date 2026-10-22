#include "xpp_mem.h"
#include "auto_unknowns.h"
#include "solver.h"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <array>
#include <span>
#include <vector>
#include <stdlib.h>
#include "auto_f2c.h"
#include "auto_c.h" /* func, stpnt, bcnd */
#include "session.h"
#include "odesol2.h"
#include "auto_nox.h"
#include "derived.h"
#include "pp_shoot.h"
#include "auto_parallel.h"  /* auto_parallel_ready() */
#include "tabular.h"  /* redo_all_fun_tables() */
#include "jacobian.h"
#include "xpp_job.h"
#include "load_eqn.h"
#include "expr.h"
#include "my_rhs.h"
#include "integrate.h"

/*    Hooks to xpp RHS     */


namespace {

/* dfdu (ndim by ndim, by columns) at u, where the right-hand side is f,
   with the constants c and, for ijac 2, the dfdp columns of the
   continuation parameters icp[0..nfpr): the model's exact Jacobian
   (W260); a parameter of AUTO's own (the period, ...) is no constant of
   the model, so its column is 0. One that is not finite ends the run as
   a Stop does (W275): the failure is kept on lib (the error, with AUTO's
   point: its first continuation parameter's value), the Jacobian is the identity
   and the parameter columns 0, a system that solves, and the solve in progress
   is not used (autlib1.cpp), and the job is
   cancelled, so the next solve returns to the last converged point and
   the branch ends with an end point to restart from (autlib1.cpp). A
   collocation worker's lib is merged into the Session's, and the job
   cancelled, by its caller when the workers are done
   (auto_parallel.cpp: a job's cancel is read by the main thread). */
void exact_derivatives(const xpp::Session &s, AutoLib &lib, const double *c, integer ndim, const double *u, const double *f, const integer *icp, integer nfpr, integer ijac, double *dfdu, double *dfdp)
{
  const int n=static_cast<int>(ndim);
  try {
    xpp::exact_jacobian(s,lib.jacobian_scratch,c,0.0,u,f,n,{xpp::JacobianLayout::ColumnMajor},dfdu);
    if(ijac<2)return;
    for(integer i=0;i<nfpr;i++){
      double *column=dfdp+icp[i]*ndim;
      const int k=s.auto_state.parameter_layout.model_index(static_cast<int>(icp[i]));
      if(k>=0 && k<s.model().nupar)xpp::exact_constant_derivative(s,lib.jacobian_scratch,c,s.model().upar_con[k],0.0,u,f,n,column);
      else std::fill_n(column,n,0.0);
    }
  } catch (const xpp::JacobianFailed &failed) {
    if(!lib.jacobian_failure){
      lib.jacobian_failure=failed.error;
      const int k=s.auto_state.parameter_layout.model_index(static_cast<int>(icp[0]));
      if(k>=0 && k<s.model().nupar)
        lib.jacobian_failure->what+=xpp::format(" (AUTO at {}={})",s.model().upar_names[k],c[s.model().upar_con[k]]);
    }
    std::fill_n(dfdu,n*n,0.0);
    for(int i=0;i<n;i++)dfdu[i*n+i]=1;
    if(ijac>=2)for(integer i=0;i<nfpr;i++)std::fill_n(dfdp+icp[i]*ndim,n,0.0);
    if(&lib==&s.auto_lib)xpp::job::cancel_current();
  }
}

/* column = b column + add (n values; b n by n, by columns; add none: 0),
   with the scratch t (n values) */
void chain(int n, const double *b, double *column, const double *add, double *t)
{
  for(int i=0;i<n;i++){
    double sum=add?add[i]:0;
    for(int k=0;k<n;k++)sum+=b[i+k*n]*column[k];
    t[i]=sum;
  }
  std::copy_n(t,n,column);
}

bool continued_constants(const xpp::Session &s, double *constants, const integer *icp, integer count, std::span<const double> parameters)
{
  bool changed=false;
  for(integer i=0;i<count;i++){
    const int k=s.auto_state.parameter_layout.model_index(static_cast<int>(icp[i]));
    if(k>=0 && k<s.model().nupar){
      double &constant=constants[s.model().upar_con[k]];
      // Signed zero can change a derived expression too.
      changed |= std::bit_cast<std::uint64_t>(constant)!=std::bit_cast<std::uint64_t>(parameters[k]);
      constant=parameters[k];
    }
  }
  return changed;
}

/* The right-hand side f of a model with no function tables, no networks and
   native programs (Model::auto_rhs_pure), on the constants c and variables v:
   a collocation worker's own copies (auto_parallel.cpp: s is const, so a
   worker writes only its own lib) or, serially, the Session's.
   The derived constants were current when the run began (the workers'
   prepare() copied them); a state difference changes no parameter, so they
   are worked out again only when a continued parameter actually changed,
   including either side of its difference. With no function table, nothing
   else depends on a parameter. */
void pure_values(const xpp::Session &s, double *c, double *v, const double *u, const integer *icp, integer nfpr, std::span<const double> parameters, double *f)
{
  if(continued_constants(s,c,icp,nfpr,parameters))
    xpp::evaluate_derived_pure(s,c,v);
  pure_rhs(s,u,f,c,v);
}

void worker_func(const xpp::Session &s, AutoLib &worker, integer ndim, const double *u, const integer *icp, integer nfpr, std::span<const double> parameters, integer ijac, double *f, double *dfdu, double *dfdp)
{
  double *c=worker.rhs_constants.data();
  pure_values(s,c,worker.rhs_variables.data(),u,icp,nfpr,parameters,f);
  if(ijac!=0&&s.numerics.jacobian==xpp::JACOBIAN_EXACT)exact_derivatives(s,worker,c,ndim,u,f,icp,nfpr,ijac,dfdu,dfdp);
}

}

/* The problem defined functions that run the model (auto_c.h's C++
   section): AUTO calls them with the Session whose run it is. */
bool auto_model_parameter(const xpp::Session &s, integer slot)
{
  return s.auto_state.parameter_layout.model_index(static_cast<int>(slot))>=0;
}

double auto_parameter_value(const xpp::Session &s, integer slot,
                           std::span<const integer> free, std::span<const double> parameters)
{
  const int k=s.auto_state.parameter_layout.model_index(static_cast<int>(slot));
  if(k>=0 && std::find(free.begin(),free.end(),slot)==free.end())
    return s.parser.constants[s.model().upar_con[k]];
  return parameters[slot];
}

int func(xpp::Session &s, integer ndim, const double *u, const integer *icp, integer nfpr, const double *par, integer ijac, double *f, double *dfdu, double *dfdp, AutoLib *worker)
{
   const auto parameters=s.auto_state.parameter_layout.parameters(par);
   if (worker && worker->pure_rhs) {
     worker_func(s,*worker,ndim,u,icp,nfpr,parameters,ijac,f,dfdu,dfdp);
     return 0;
   }
   const bool exact=ijac!=0&&s.numerics.jacobian==xpp::JACOBIAN_EXACT;
   int i,j;
   /* the right-hand side and the Jacobian only read the point */
   double *x=const_cast<double *>(u);
   if(xpp::auto_parallel_ready(s))
     pure_values(s,s.parser.constants.data(),s.parser.variables.data(),x,icp,nfpr,parameters,f);
   else{
     continued_constants(s,s.parser.constants.data(),icp,nfpr,parameters);
     xpp::evaluate_derived(s);
     if(auto r=xpp::redo_all_fun_tables(s);!r)xpp::auto_fail(r.error());
     s.integrator.rhs(0.0,x,f,ndim);
     if(s.integrator.step_error)xpp::auto_fail(take_step_error(s).error());
   }
   const double *c=s.parser.constants.data();
   if(exact)exact_derivatives(s,s.auto_lib,c,ndim,x,f,icp,nfpr,ijac,dfdu,dfdp);
   else if(ijac==1){
     xpp::jacobian(s,0.0,x,f,ndim,s.numerics.singpt_jacobian_epsilon,{xpp::JacobianLayout::ColumnMajor},dfdu);
   }
   if(!xpp::solver_info(s.numerics.method).traits.discrete||s.numerics.store_every==1)return 0;
   /* the map applied store_every times: its exact Jacobian is the product
      of each step's, and each step adds its own dependence on the
      parameters to what the steps before passed on */
   const size_t nn=static_cast<size_t>(ndim);
   auto &zz = s.auto_lib.scratch.map_state;
   auto &step = s.auto_lib.scratch.map_jacobian;
   auto &step_p = s.auto_lib.scratch.map_parameters;
   auto &scratch = s.auto_lib.scratch.map_chain;
   zz.resize(nn);
   if (exact) {
     step.resize(xpp::size_product(nn,nn));
     step_p.resize(xpp::size_product(nn,s.auto_lib.parameter_count));
     scratch.resize(nn);
   }
   for(i=1;i<s.numerics.store_every;i++){
     for(j=0;j<ndim;j++)
       zz[j]=f[j];
     s.integrator.rhs(0.0,zz.data(),f,ndim);
     if(s.integrator.step_error)xpp::auto_fail(take_step_error(s).error());
     if(!exact)continue;
     exact_derivatives(s,s.auto_lib,c,ndim,zz.data(),f,icp,nfpr,ijac,step.data(),step_p.data());
     const int n=static_cast<int>(ndim);
     for(int k=0;k<n;k++)chain(n,step.data(),dfdu+k*ndim,nullptr,scratch.data());
     if(ijac<2)continue;
     for(integer p=0;p<nfpr;p++)chain(n,step.data(),dfdp+icp[p]*ndim,step_p.data()+icp[p]*ndim,scratch.data());
   }

   return 0;

} /* func_ */

int stpnt(xpp::Session &s, integer ndim, doublereal t, doublereal *u, doublereal *par)
{
  int i;

  double p;

  const auto &layout=s.auto_state.parameter_layout;
  const auto unknowns=layout.unknowns(par);
  const auto parameters=layout.parameters(par);
  for(int k=0;k<s.model().nupar;k++)parameters[k]=s.parser.constants[s.model().upar_con[k]];

  if(s.auto_state.new_period_flag==0){  
    for(i=0;i<ndim;i++)
      u[i]=s.last_ic[i];
    return 0;
  }

  get_start_period(s,&p);
  unknowns[xpp::auto_unknown::period]=p;
  if(s.auto_state.homo_flag!=1)get_start_orbit(s,u,t,p,ndim);
  if(s.auto_state.homo_flag==1){
    /* the orbit from its point nearest the equilibrium (find_best_homo_shift) */
    get_start_orbit(s,u,t+s.auto_state.homo_shift/p,p,ndim);
    for(i=0;i<ndim;i++){
      unknowns[xpp::auto_unknown::homoclinic_equilibrium_start+i]=s.auto_state.homo_l[i];

    }
  }
  if(s.auto_state.homo_flag==2){ /* heteroclinic */
    for(i=0;i<ndim;i++){
      unknowns[xpp::auto_unknown::homoclinic_equilibrium_start+i]=s.auto_state.homo_l[i];
      unknowns[xpp::auto_unknown::homoclinic_equilibrium_start+i+ndim]=s.auto_state.homo_r[i];

    }

  }
  return 0;

} /* stpnt_ */

/* Subroutine */ int bcnd(xpp::Session &s, integer ndim, const double *par, const integer *icp, integer nbc, const double *u0, const double *u1, integer ijac, double *fb, double *dbc)
{
/* Hooks to the XPP bc parser!! */

 const bool changed=continued_constants(s,s.parser.constants.data(),icp,s.auto_state.run.nicp,
                     s.auto_state.parameter_layout.parameters(par));

 /* with no function table the derived constants depend on the parameters
    alone (pure_values) */
 const bool pure=s.model().auto_rhs_pure;
 if(changed||!pure)xpp::evaluate_derived(s);
 if(!pure)if(auto r=xpp::redo_all_fun_tables(s);!r)xpp::auto_fail(r.error());
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
