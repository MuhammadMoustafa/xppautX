#include "model.h"
#include "session.h"
#include "xpp_ui.h"
#include "storage.h"
#include "xpp_util.h"
#include "xpp_math.h"
#include "pp_shoot.h"

#include "my_rhs.h"
#include "adj2.h"
#include "load_eqn.h"

#include "expr.h"
#include "browse.h"
#include "graf_par.h"
#include "integrate.h"
#include "xpp_job.h"
#include "lunch-new.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <array>
#include <string>
#include <vector>
#include <math.h>
#include "getvar.h"
#include "delay_handle.h"

#define ESCAPE 27

#define NOCHANGE 2
#define NUMICS -1
#define BADINT -4
#define TOOMANY -2
#define BADJAC -3
#define PARAM 1
#define IC 2


namespace {
struct {
  std::string item;
  int steps,side,cycle,movie;
  double plow,phigh;
} shoot_range;
}  // namespace

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
  for(i=n;i<n+s.model().fix_var;i++)setvar(s,i+1,evaluate(s,s.model().programs[i].data()));
 
  for(i=0;i<n;i++)f[i]=evaluate(s,s.bcs[i].com.data());
}

void compile_bvp(xpp::Session &s)
{
 int i;
 int len;
 reset_bvp(s);
 if(s.numerics.bvp_flag==0)return;

 s.parser.ncon=s.model().ncon_start;
 s.parser.nsym=s.model().nsym_start;
 s.numerics.bvp_flag=0;
 for(i=0;i<s.model().node;i++){

   if(add_expr(s,s.bcs[i].string.data(),s.bcs[i].com.data(),&len)){
     err_msg(xpp::format("Bad syntax on {} th BC",i+1).c_str());
     return;
   }
 }
 s.numerics.bvp_flag=1;
}

void reset_bvp(xpp::Session &s)
{
 s.numerics.bvp_flag=1;
} 

void init_shoot_range(std::string_view s)
{
 shoot_range.item=s;
 shoot_range.phigh=1.0;
 shoot_range.plow=0.0;
 shoot_range.side=0;
 shoot_range.cycle=0;
 shoot_range.steps=10;
 shoot_range.movie=0;
}
  
void dump_shoot_range(FILE *fp, int f)
{
  io_string(shoot_range.item,fp,f);
  io_int(&shoot_range.side,fp,f,"BVP side");
  io_int(&shoot_range.cycle,fp,f,"color cycle flag 1=on");
  io_int(&shoot_range.steps,fp,f,"BVP range steps");
  io_double(&shoot_range.plow,fp,f,"BVP range low");
  io_double(&shoot_range.phigh,fp,f,"BVP range high");

}

void bad_shoot(int iret)
{
 switch(iret){
 case NOCHANGE:
   err_msg("No change from last point. Saving anyway");
   break;
 case NUMICS:
   err_msg("Number BCS not equal number ICs");
   break;
 case BADINT:
   err_msg("Unable to complete integration");
   break;
 case TOOMANY:
   err_msg("Maximum iterates exceeded");
   break;
 case BADJAC:
   err_msg("Bad Jacobian -- uninvertable");
   break;
 }
}

void do_sh_range(xpp::Session &s, double *ystart, double *yend)
{
 double parlo,parhi,dpar,temp;
 int npar,i,j,ierr;
 int side,cycle,icol,color;

 if(set_up_sh_range(s)==0)return;
 swap_color(s,&color,0);
 parhi=shoot_range.phigh;
 parlo=shoot_range.plow;
 npar=shoot_range.steps;
 dpar=(parhi-parlo)/static_cast<double>(npar);
 side=shoot_range.side;
 cycle=shoot_range.cycle;
 s.data_store.rows=0;
 icol=0;
 if(shoot_range.movie==1)
   reset_film(s);
 for(i=0;i<=npar;i++)
   {
     temp=parlo+dpar*static_cast<double>(i);
     set_val(s,shoot_range.item,temp);
     bottom_msg(2,xpp::format("{}={:.16g}",shoot_range.item,temp).c_str());
     if(shoot_range.movie==1)
       clr_scrn(s);
     
     xpp::ok_or_show(bvshoot(s,ystart,yend,s.numerics.bvp_tol,s.numerics.bvp_eps,s.numerics.bvp_maxit,&ierr,s.model().node,0,
	     0,0,0,0.0));
     if(ierr==-5)continue;
     if(ierr<0){ 
       bad_shoot(ierr);

       refresh_browser(s,s.data_store.rows);
       swap_color(s,&color,1);
       return;
     }
     s.data_store.col[0][s.data_store.rows]=temp;
     if(side==0)for(j=0;j<s.model().node;j++)s.data_store.col[j+1][s.data_store.rows]=ystart[j];
     else for(j=0;j<s.model().node;j++)s.data_store.col[j+1][s.data_store.rows]=yend[j];
     s.data_store.rows++;
     set_cycle(s,cycle,&icol);
     get_ic(s,0,ystart);
     if(const xpp::Result<> r=last_shot(s,0);!r)xpp::show_error(r.error());
     if(shoot_range.movie==1)xpp_ui.film_clip(s);
     ping();
   }
  refresh_browser(s,s.data_store.rows);
  auto_freeze_it(s);     
 swap_color(s,&color,1);

}

int set_up_periodic(xpp::Session &s, int *ipar, int *ivar, double *sect, int *ishow)
{
 static const char *n[]={"Freq. Par.","*1Sect. Var","Section","Show(Y/N)"};
 std::array<std::string, 4> values;
 int status,i;
 static const char *yn[]={"N","Y"};
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
		 err_msg("No such parameter");
		 return(0);
	       }
	       i=find_user_name(s.model(),IC,values[1].c_str());
	       if(i>-1)
		 *ivar=i;
	       else {
		 err_msg("No such variable");
		 return(0);
	       }
	       *sect=atof(values[2].c_str());
	       if(values[3][0]=='Y'||values[3][0]=='y')*ishow=1;
	       else *ishow=0;
	       return(1);
	     }
  return(0);
}

void find_bvp_com(xpp::Session &s, int com)
{
 int ishow=0,iret;
 int iper=0,ivar=0,ipar=0,pflag;
 double sect=0.0;
 double oldpar;
 double ystart[MAXODE],oldtrans;
 double yend[MAXODE];
 /*  Window temp=main_win; */
 if(s.model().nmarkov>0||s.model().nkernel>0){
   err_msg("Can't do BVP with integral or markov eqns");
   return;
 }
 wipe_rep(s.browser);
 data_back(s);
 compile_bvp(s);
 if(s.numerics.fft||s.numerics.hist||s.delay.flag||s.numerics.bvp_flag==0)return;
 s.numerics.storflag=0;
 s.integrator.range_flag=1;
 s.numerics.poimap=0;
 oldtrans=s.numerics.trans;
 s.numerics.trans=0.0;
 get_ic(s,1,ystart);
 switch(com){
 case 0:
   do_sh_range(s,ystart,yend);
   return;
 case 3:
   if(s.model().nupar==0)goto bye;
   pflag=set_up_periodic(s,&ipar,&ivar,&sect,&ishow);
   if(pflag==0)goto bye;
   iper=1;
   get_val(s,s.model().upar_names[ipar],&oldpar);
   break;
        
 case 2: 
   ishow=1;
   iper=0;
   break;
 case 1:
 default:
   iper=0;
   break;
 }
 {
 const xpp::Result<> shot=iper
   ? bvshoot(s,ystart,yend,s.numerics.bvp_tol,s.numerics.bvp_eps,s.numerics.bvp_maxit,&iret,s.model().node,ishow,
	iper,ipar,ivar,sect)
   : bvshoot(s,ystart,yend,s.numerics.bvp_tol,s.numerics.bvp_eps,s.numerics.bvp_maxit,&iret,s.model().node,ishow,0,0,0,0.0 );
 if(!shot)xpp::show_error(shot.error());
 bad_shoot(iret);
 if(iret==1||iret==2) {
 get_ic(s,0,ystart);  
 redraw_ics();
 if(ishow){
   reset_graphics(s);
 }
 const xpp::Result<> last=last_shot(s,1);
 s.numerics.inflag=1;
 refresh_browser(s,s.data_store.rows);
 auto_freeze_it(s);
 ping();
 if(!last)xpp::show_error(last.error());
}
else 
 if(iper)set_val(s,s.model().upar_names[ipar],oldpar);
 }
  
bye:  s.numerics.trans=oldtrans;
}

xpp::Result<> last_shot(xpp::Session &s, int flag)
{
 int i;
 double *x;
 x=&s.data_store.current[0];
 s.integrator.my_start=1;
 get_ic(s,2,x);
 s.numerics.storflag=flag;
 s.data_store.current_time=s.numerics.t0;
 if(flag){
  s.data_store.col[0][0]=static_cast<float>(s.numerics.t0);
  extra(s,x,s.numerics.t0,s.model().node,s.model().neq);
  for(i=0;i<s.model().neq;i++)s.data_store.col[1+i][0]=static_cast<float>(x[i]);
  s.data_store.rows=1;

}
 return integrate(s,&s.data_store.current_time,x,s.numerics.tend,s.numerics.delta_t,1,s.numerics.njmp,&s.integrator.my_start)
   .transform([](int){});
}

int set_up_sh_range(xpp::Session &s)
{
static const char *n[]={"*2Range over","Steps","Start","End",
		     "Cycle color(Y/N)",
		       "Side(0/1)", "Movie(Y/N)" };
 std::array<std::string, 7> values;
 int status,i;
 static  const char *yn[]={"N","Y"};
 values[0] = shoot_range.item;
 values[1] = xpp::format("{}", shoot_range.steps);
 values[2] = xpp::format("{:g}", shoot_range.plow);
 values[3] = xpp::format("{:g}", shoot_range.phigh);
 values[4] = yn[shoot_range.cycle];
 values[5] = xpp::format("{}", shoot_range.side);
 values[6] = yn[shoot_range.movie];

 static const int kinds[]={XPP_FIELD_NAME_IN(2),XPP_FIELD_INTEGER,XPP_FIELD_NUMBER,XPP_FIELD_NUMBER,
                           XPP_FIELD_TEXT,XPP_FIELD_INTEGER,XPP_FIELD_TEXT};
 status=do_string_box_of(7,1,"Range Shoot",n,values,kinds);
 if(status!=0){
   shoot_range.item=values[0];
   i=find_user_name(s.model(),PARAM,shoot_range.item);
   if(i<0){
        err_msg("No such parameter");
       return(0);
     }
   
   shoot_range.steps=atoi(values[1].c_str());
   if(shoot_range.steps<=0)shoot_range.steps=10;
   shoot_range.plow=atof(values[2].c_str());
   shoot_range.phigh=atof(values[3].c_str());
   if(values[4][0]=='Y'||values[4][0]=='y')shoot_range.cycle=1;
   else shoot_range.cycle=0;
 if(values[6][0]=='Y'||values[6][0]=='y')shoot_range.movie=1;
   else shoot_range.movie=0;

   shoot_range.side=atoi(values[5].c_str());

 return(1);
 }

 return(0);
}

xpp::Result<> bvshoot(xpp::Session &s, double *y, double *yend, double err, double eps, int maxit, int *iret, int n, int ishow, int iper, int ipar, int ivar, double sect)
{
 xpp::Computation computing; /* what Escape stops (xpp_job.h) */
 xpp::FirstError shown; /* the drawn curve's failure */
 double dev,error,ytemp;

  int ntot=n;
 int i,istart=1,j;
 int ipvt[MAXODE1];
 char esc;
 int info,niter=0;
 double dt=s.numerics.delta_t,t;
 double t0=s.numerics.t0;
 double t1=s.numerics.t0+s.numerics.tend*dt/fabs(dt);

 if(iper)ntot=n+1;
 std::vector<double> jac_v(static_cast<size_t>(ntot)*ntot);
 std::vector<double> f_v(ntot), fdev_v(ntot), y0_v(ntot), y1_v(ntot);
 double *jac=jac_v.data(), *f=f_v.data(), *fdev=fdev_v.data(), *y0=y0_v.data(), *y1=y1_v.data();

 for(i=0;i<n;i++)
   y0[i]=y[i];
 if(iper)  get_val(s,s.model().upar_names[ipar],&y0[n]);

 while(1){
   esc=my_abort();

           {
            
             if(esc==ESCAPE) {*iret=-5;break;}
	     if(esc=='/'){*iret=-6;break;}
	    
           }
         
  t=t0;
 istart=1;
 if(iper)set_val(s,s.model().upar_names[ipar],y0[n]);

 {
   const xpp::Result<> run=ode_int(s,y,&t,&istart,ishow);
   /* drawn as it runs, a failure only ends the curve */
   if(ishow)shown.keep(run);
   else if(!run){
     *iret=-4;
     goto bye;
   }
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
   *iret=1;
   goto bye;
  
 }
 niter++;
 if(niter>maxit){
   *iret=-2;
   goto bye;
 }      /* Too many iterates   */

 /*   create the Jacobian matrix ...   */
 
 for(j=0;j<ntot;j++){
   for(i=0;i<n;i++) y[i]=y0[i];
    if(fabs(y0[j])<eps)dev=eps*eps;
	else dev=eps*fabs(y0[j]);
   
    if(j<n) y[j]=y[j]+dev;
     ytemp=y0[j];
     y0[j]=y0[j]+dev;
  
     if(j==n)
         set_val(s,s.model().upar_names[ipar],y0[j]);
       
     t=t0;
     istart=1;

      if(!ode_int(s,y,&t,&istart,0)){
	*iret=-4;
	goto bye;
      }

     do_bc(s,y0,t0,y,t1,fdev,n);
     if(iper)fdev[n]=y[ivar]-sect;
     y0[j]=ytemp;
     for(i=0;i<ntot;i++)jac[j+i*ntot]=(fdev[i]-f[i])/dev;
 }

  xpp::sgefa(jac,ntot,ntot,ipvt,&info);
  if(info!=-1){
    *iret=-3;
    goto bye;
  }
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
    *iret=2;
    goto bye;
  }
}
  
 bye:
   return shown.result();
}

