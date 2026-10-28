#include "mykeydef.h"
#include "model.h"
#include "session.h"
#include "xpp_ui.h"
#include "storage.h"
#include "xpp_util.h"
#include "typed_values.h"
#include "many_pops.h"
#include "xpp_math.h"
#include "pp_shoot.h"
#include "numerics_settings.h"

#include "my_rhs.h"
#include "adj2.h"
#include "jacobian.h"
#include "load_eqn.h"

#include "expr.h"
#include "browse.h"
#include "graf_par.h"
#include "integrate.h"
#include "xpp_job.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <math.h>
#include "getvar.h"
#include "delay_handle.h"

namespace xpp {


#define PARAM 1
#define IC 2



/*   more general mixed boundary types   */

void do_bc(xpp::Session &s, double *y__0, double t0, double *y__1, double t1, double *f, int n)
{
 int n0=s.model().prime_start;
 int i;

 setvar(s,0,t0);
 setvar(s,n0,t1);

 for(i=0;i<n;i++){
   setvar(s,i+1,y__0[i]);
   setvar(s,i+n0+1,y__1[i]);
 }
  for(i=n;i<n+s.model().fix_var;i++)setvar(s,i+1,xpp::evaluate(s,s.model().programs[i]));
 
  for(i=0;i<n;i++)f[i]=evaluate(s,s.bcs[i].com.data());
}

xpp::Result<> compile_bvp(xpp::Session &s)
{
 int i;
 int len;
 reset_bvp(s);
 if(!s.numerics.bvp_flag)return {};

 s.parser.ncon=s.model().ncon_start;
 s.parser.nsym=s.model().nsym_start;
 s.numerics.bvp_flag=false;
 for(i=0;i<s.model().node;i++){

   if(add_expr(s,s.bcs[i].string.data(),s.bcs[i].com,&len))
     return xpp::fail("boundary",xpp::format("Bad syntax in boundary condition {}: {}",i+1,s.bcs[i].string.data()),
                      xpp::model_place(s.model(),xpp::odex::Statement::Kind::Boundary,i));
 }
 s.numerics.bvp_flag=true;
 return {};
}

void reset_bvp(xpp::Session &s)
{
 s.numerics.bvp_flag=true;
} 

void init_shoot_range(xpp::Session &s, std::string_view item)
{
 s.shoot_range.item=item;
 s.shoot_range.phigh=1.0;
 s.shoot_range.plow=0.0;
 s.shoot_range.side=0;
 s.shoot_range.cycle=0;
 s.shoot_range.steps=10;
 s.shoot_range.movie=0;
}
  

xpp::Result<int> do_sh_range(xpp::Session &s, double *ystart, double *yend)
{
 double parlo,parhi,dpar,temp;
 int npar,i,j;
 int side,cycle,icol;
 xpp::Color color;
 std::optional<xpp::Error> failure;

 swap_color(s,&color,0);
 parhi=s.shoot_range.phigh;
 parlo=s.shoot_range.plow;
 npar=s.shoot_range.steps;
 dpar=(parhi-parlo)/static_cast<double>(npar);
 side=s.shoot_range.side;
 cycle=s.shoot_range.cycle;
 s.data_store.rows=0;
 icol=0;
 xpp::Result<> film;
 if(s.shoot_range.movie==1)
   reset_film(s);
 for(i=0;i<=npar;i++)
   {
     if(xpp::job::cancelled())break;
     temp=parlo+dpar*static_cast<double>(i);
     set_val(s,s.shoot_range.item,temp);
     bottom_msg(2,xpp::format("{}={:.16g}",s.shoot_range.item,temp));
     if(s.shoot_range.movie==1)
       clr_scrn(s);
     
     const xpp::Result<Shot> shot=bvshoot(s,ystart,yend,s.numerics.bvp_tolerance,s.numerics.bvp_epsilon,s.numerics.bvp_max_iterates,s.model().node,0,
	     0,0,0,0.0);
     if(xpp::job::cancelled())break;
     if(!shot){ /* a value with no solution ends the sweep, naming it */
       failure=shot.error();
       failure->what=xpp::format("Range Shoot stopped at {} = {} (step {} of {}): {}",s.shoot_range.item,xpp::number(temp),i+1,npar+1,failure->what);
       break;
     }
     if(shot->end==ShootEnd::cancelled)break;
     if(s.data_store.rows==s.data_store.max_rows){
       const auto grown=stor_full(s);
       if(!grown){failure=grown.error();break;}
     }
     s.data_store.col[0][s.data_store.rows]=temp;
     if(side==SHOOT_SIDE_START)for(j=0;j<s.model().node;j++)s.data_store.col[j+1][s.data_store.rows]=ystart[j];
     else for(j=0;j<s.model().node;j++)s.data_store.col[j+1][s.data_store.rows]=yend[j];
     s.data_store.rows++;
     set_cycle(s,cycle,&icol);
     set_initial_data(s,{ystart,static_cast<size_t>(s.model().node)});
     if(const xpp::Result<> r=last_shot(s,0);!r&&!failure)failure=r.error();
     if(s.shoot_range.movie==1 && film)film=ui.film_clip(s);
     ping();
   }
  refresh_browser(s,s.data_store.rows);
  auto_freeze_it(s);     
 swap_color(s,&color,1);
 if(failure)return std::unexpected(std::move(*failure));
 if(!film)return std::unexpected(film.error());
 return s.data_store.rows;
}

int set_up_periodic(xpp::Session &s, int *ipar, int *ivar, double *sect, int *ishow)
{
 static const char *const n[]={"Freq. Par.","*1Sect. Var","Section","Show(Y/N)"};
 std::array<std::string, 4> values;
 int status,i;
 static const char *const yn[]={"N","Y"};
 values[0] = s.model().upar_names[*ipar];
 values[1] = s.model().uvar_names[*ivar];
 values[2] = xpp::format("{:g}", *sect);
 values[3] = yn[*ishow];
 
 static const int kinds[]={XPP_FIELD_NAME_IN(2),XPP_FIELD_NAME_IN(1),XPP_FIELD_NUMBER,XPP_FIELD_TEXT};
 status=do_string_box_of(4,1,"Periodic BCs",n,values,kinds);
 if(status!=0){
               i=find_user_name(s.model(),PARAM,values[0].c_str());
	       if(i>-1)
		 *ipar=i;
	       else {
		 command_error("bvp", "No such parameter");
		 return(0);
	       }
	       i=find_user_name(s.model(),IC,values[1].c_str());
	       if(i>-1)
		 *ivar=i;
	       else {
		 command_error("bvp", "No such variable");
		 return(0);
	       }
	       *sect=atof(values[2].c_str());
	       if(values[3][0]=='Y'||values[3][0]=='y')*ishow=1;
	       else *ishow=0;
	       return(1);
	     }
  return(0);
}

namespace {

/* what a boundary-value command needs before it solves: a model it can solve for, the boundary
   conditions compiled, the storage cleared, and the settings every solve runs under (the plot's
   store-from time zero while it does, put back when it ends) */
xpp::Result<> begin_shoot(xpp::Session &s)
{
 if(s.model().nmarkov>0||s.model().nkernel>0)
   return xpp::fail("bvp","Can't do BVP with integral or markov eqns",xpp::command_place());
 wipe_rep(s.browser);
 data_back(s);
 if(auto compiled=compile_bvp(s);!compiled)return compiled;
 if(s.numerics.fft||s.numerics.hist)
   return xpp::fail("bvp","The data table shows a Fourier or histogram result: choose Stochastic, Data to see the trajectory again before solving",xpp::command_place());
 if(s.delay.flag)
   return xpp::fail("bvp","Boundary value problems cannot be solved for a model with delays",xpp::command_place());
 s.numerics.storflag=false;
 s.integrator.range_flag=1;
 s.numerics.poincare_map=PoincareMap::none;
 return {};
}

struct ShootStoreFrom {
 xpp::Session &s;
 double before;
 explicit ShootStoreFrom(xpp::Session &session) : s(session), before(session.numerics.store_from){s.numerics.store_from=0.0;}
 ~ShootStoreFrom(){s.numerics.store_from=before;}
};

/* the solution bvshoot found made the initial data and integrated and stored (the periodic solve's period
   back as it was when it found none) */
xpp::Result<> keep_shot(xpp::Session &s, const xpp::Result<Shot> &shot, double *ystart, bool show, std::optional<std::pair<int,double>> period)
{
 xpp::FirstError problems;
 const auto restore_period=[&]{if(period)set_val(s,s.model().upar_names[static_cast<std::size_t>(period->first)],period->second);};
 if(!shot){
   restore_period();
   return std::unexpected(shot.error());
 }
 problems.keep(shot->curve);
 if(shot->end==ShootEnd::update_met)
   bottom_msg(0,"No change from last point. Saving anyway");
 if(shot->end==ShootEnd::cancelled){
   restore_period();
   return problems.result();
 }
 set_initial_data(s,{ystart,static_cast<size_t>(s.model().node)});
 redraw_ics();
 if(show)
   reset_graphics(s);
 problems.keep(last_shot(s,1));
 s.numerics.inflag=true;
 refresh_browser(s,s.data_store.rows);
 auto_freeze_it(s);
 ping();
 return problems.result();
}

} // namespace

xpp::Result<> bvp_solve(xpp::Session &s, bool show)
{
 if(auto begun=begin_shoot(s);!begun)return begun;
 const ShootStoreFrom store_from(s);
 std::vector<double> ystart(s.model().neq),yend(s.model().neq);
 get_ic(s,1,ystart.data());
 const xpp::Result<Shot> shot=bvshoot(s,ystart.data(),yend.data(),s.numerics.bvp_tolerance,s.numerics.bvp_epsilon,s.numerics.bvp_max_iterates,s.model().node,show?1:0,
	0,0,0,0.0);
 return keep_shot(s,shot,ystart.data(),show,std::nullopt);
}

xpp::Result<double> bvp_solve_periodic(xpp::Session &s, int ipar, int ivar, double sect, bool show)
{
 if(s.model().nupar==0)
   return xpp::fail("bvp","The model has no parameter to be the frequency",xpp::command_place());
 if(auto begun=begin_shoot(s);!begun)return std::unexpected(begun.error());
 const ShootStoreFrom store_from(s);
 std::vector<double> ystart(s.model().neq),yend(s.model().neq);
 get_ic(s,1,ystart.data());
 double oldpar;
 get_val(s,s.model().upar_names[static_cast<std::size_t>(ipar)],&oldpar);
 const xpp::Result<Shot> shot=bvshoot(s,ystart.data(),yend.data(),s.numerics.bvp_tolerance,s.numerics.bvp_epsilon,s.numerics.bvp_max_iterates,s.model().node,show?1:0,
	1,ipar,ivar,sect);
 if(auto kept=keep_shot(s,shot,ystart.data(),show,std::pair{ipar,oldpar});!kept)return std::unexpected(kept.error());
 double period;
 get_val(s,s.model().upar_names[static_cast<std::size_t>(ipar)],&period);
 return period;
}

xpp::Result<int> bvp_solve_range(xpp::Session &s)
{
 if(auto begun=begin_shoot(s);!begun)return std::unexpected(begun.error());
 const ShootStoreFrom store_from(s);
 std::vector<double> ystart(s.model().neq),yend(s.model().neq);
 get_ic(s,1,ystart.data());
 return do_sh_range(s,ystart.data(),yend.data());
}

void find_bvp_com(xpp::Session &s, int com)
{
 /* the Boundary value items of an imported button sequence (kbs.h): the questions the registry's bvp.* commands take as arguments are asked here */
 switch(com){
 case 0:
   if(set_up_sh_range(s)!=0)xpp::ok_or_show(bvp_solve_range(s));
   break;
 case 3:{
   int ipar=0,ivar=0,ishow=0;
   double sect=0.0;
   if(s.model().nupar==0||set_up_periodic(s,&ipar,&ivar,&sect,&ishow)==0)break;
   xpp::ok_or_show(bvp_solve_periodic(s,ipar,ivar,sect,ishow!=0));
   break;}
 default:
   xpp::ok_or_show(bvp_solve(s,com==2));
   break;
 }
}

xpp::Result<> last_shot(xpp::Session &s, int flag)
{
 int i;
 double *x;
 x=&s.data_store.current[0];
 s.integrator.my_start=1;
 get_ic(s,2,x);
 s.numerics.storflag=flag;
 s.data_store.current_time=s.numerics.start_time;
 if(flag){
  s.data_store.col[0][0]=static_cast<float>(s.numerics.start_time);
  extra(s,x,s.numerics.start_time,s.model().node,s.model().neq);
  for(i=0;i<s.model().neq;i++)s.data_store.col[1+i][0]=static_cast<float>(x[i]);
  s.data_store.rows=1;

}
 return integrate(s,&s.data_store.current_time,x,s.numerics.total_time,s.numerics.delta_t,1,s.numerics.store_every,&s.integrator.my_start)
   .transform([](int){});
}

int set_up_sh_range(xpp::Session &s)
{
static const char *const n[]={"*2Range over","Steps","Start","End",
		     "Cycle color(Y/N)",
		       "Side(0/1)", "Movie(Y/N)" };
 std::array<std::string, 7> values;
 int status,i;
 static const char *const yn[]={"N","Y"};
 values[0] = s.shoot_range.item;
 values[1] = xpp::format("{}", s.shoot_range.steps);
 values[2] = xpp::format("{:g}", s.shoot_range.plow);
 values[3] = xpp::format("{:g}", s.shoot_range.phigh);
 values[4] = yn[s.shoot_range.cycle];
 values[5] = xpp::format("{}", s.shoot_range.side);
 values[6] = yn[s.shoot_range.movie];

 static const int kinds[]={XPP_FIELD_NAME_IN(2),XPP_FIELD_INTEGER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,
                           XPP_FIELD_TEXT,XPP_FIELD_INTEGER,XPP_FIELD_TEXT};
 status=do_string_box_of(7,1,"Range Shoot",n,values,kinds);
 if(status!=0){
   s.shoot_range.item=values[0];
   i=find_user_name(s.model(),PARAM,s.shoot_range.item);
   if(i<0){
        command_error("bvp", "No such parameter");
       return(0);
     }
   
   s.shoot_range.steps=atoi(values[1].c_str());
   if(s.shoot_range.steps<=0)s.shoot_range.steps=10;
   s.shoot_range.plow=atof(values[2].c_str());
   s.shoot_range.phigh=atof(values[3].c_str());
   if(values[4][0]=='Y'||values[4][0]=='y')s.shoot_range.cycle=1;
   else s.shoot_range.cycle=0;
 if(values[6][0]=='Y'||values[6][0]=='y')s.shoot_range.movie=1;
   else s.shoot_range.movie=0;

   s.shoot_range.side=atoi(values[5].c_str());

 return(1);
 }

 return(0);
}

xpp::Result<Shot> bvshoot(xpp::Session &s, double *y, double *yend, double err, double eps, int maxit, int n, int ishow, int iper, int ipar, int ivar, double sect)
{
 xpp::Computation computing; /* what Escape stops (xpp_job.h) */
 xpp::FirstError shown; /* the drawn curve's failure */
 double dev,error,ytemp;

 int ntot=n;
 int i,istart=1,j;
 int info,niter=0;
 double dt=s.numerics.delta_t,t;
 double t0=s.numerics.start_time;
 double t1=s.numerics.start_time+s.numerics.total_time*dt/fabs(dt);

 if(iper)ntot=n+1;
 std::vector<int> ipvt_values(ntot);
 int *const ipvt=ipvt_values.data();
 std::vector<double> jac_v(static_cast<size_t>(ntot)*ntot);
 std::vector<double> f_v(ntot), fdev_v(ntot), y0_v(ntot), y1_v(ntot);
 double *jac=jac_v.data(), *f=f_v.data(), *fdev=fdev_v.data(), *y0=y0_v.data(), *y1=y1_v.data();
 const auto limits_place=[&]{return settings_place(s.model(),{"bvp_max_iterates","bvp_tolerance"});};
 /* the integration failed: its own error, at the iterate (and Jacobian column) it failed in */
 const auto integration_failed=[&](xpp::Error e,std::string_view where){
   e.what=xpp::format("Shoot: the integration failed at iterate {}{}: {}",niter,where,e.what);
   return std::unexpected(std::move(e));
 };

 for(i=0;i<n;i++)
   y0[i]=y[i];
 if(iper)  get_val(s,s.model().upar_names[ipar],&y0[n]);

 while(1){
   const char esc=my_abort();
   if(esc==ESC||esc=='/')return Shot{ShootEnd::cancelled,shown.result()};

  t=t0;
 istart=1;
 if(iper)set_val(s,s.model().upar_names[ipar],y0[n]);

 {
   const xpp::Result<> run=ode_int(s,y,&t,&istart,ishow);
   /* drawn as it runs, a failure only ends the curve */
   if(ishow)shown.keep(run);
   else if(!run)return integration_failed(run.error(),"");
 }
 for(i=0;i<n;i++){
   y1[i]=y[i];
 }

 do_bc(s,y0,t0,y1,t1,f,n);
 if(iper)f[n]=y1[ivar]-sect;
 error=0.0;
 for(i=0;i<ntot;i++)error+=fabs(f[i]);
 if(error<err){
   for(i=0;i<n;i++)y[i]=y0[i]; /*   Good values .... */
  if(iper){ 
    set_val(s,s.model().upar_names[ipar],y0[n]);
    redraw_params();
  }
   
   for(i=0;i<n;i++)yend[i]=y1[i];
   return Shot{ShootEnd::residual_met,shown.result()};
 }
 niter++;
 if(niter>maxit) /* Too many iterates   */
   return xpp::fail("bvp",xpp::format("Shoot: no convergence after {} iterations: the residual of the boundary conditions {} is not below {}",
                    named_settings(s,{"bvp_max_iterates"}),xpp::number(error),named_settings(s,{"bvp_tolerance"})),limits_place());

 /*   create the Jacobian matrix ...   */
 
 for(j=0;j<ntot;j++){
   for(i=0;i<n;i++) y[i]=y0[i];
    dev=xpp::difference_step(eps,y0[j]);
   
    if(j<n) y[j]=y[j]+dev;
     ytemp=y0[j];
     y0[j]=y0[j]+dev;
  
     if(j==n)
         set_val(s,s.model().upar_names[ipar],y0[j]);
       
     t=t0;
     istart=1;

      if(const xpp::Result<> run=ode_int(s,y,&t,&istart,0);!run)
        return integration_failed(run.error(),xpp::format(", perturbing {}",j<n?s.model().uvar_names[j]:s.model().upar_names[ipar]));

     do_bc(s,y0,t0,y,t1,fdev,n);
     if(iper)fdev[n]=y[ivar]-sect;
     y0[j]=ytemp;
     for(i=0;i<ntot;i++)jac[j+i*ntot]=(fdev[i]-f[i])/dev;
 }

  xpp::sgefa(jac,ntot,ntot,ipvt,&info);
  if(info!=-1)
    return xpp::fail("bvp",xpp::format("Shoot: the Jacobian of the boundary conditions is singular at iterate {} (residual {})",niter,xpp::number(error)),
                     limits_place());
  for(i=0;i<ntot;i++)fdev[i]=f[i];
  xpp::sgesl(jac,ntot,ntot,ipvt,fdev);
  error=0.0;
  for(i=0;i<ntot;i++){
    y0[i]=y0[i]-fdev[i];
    error+=fabs(fdev[i]);
  }
 
 for(i=0;i<n;i++)y[i]=y0[i];
  if(error<1.e-10){
   for(i=0;i<n;i++)yend[i]=y1[i];
    return Shot{ShootEnd::update_met,shown.result()};
  }
}
}

} // namespace xpp
