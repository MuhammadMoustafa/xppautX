
#include "xpp_ui.h"
#include "session.h"
#include "xpp_util.h"
#include "adj2.h"
#include "integrate.h"
#include "numerics.h"
#include <string>
#include <strings.h>
#include <array>

#include "menudrive.h"
#include "menus.h"
#include <stdlib.h> 
#include <stdio.h>
#include <math.h>
#include "browse.h"
#include "volterra2.h"
#include "odesol2.h"
#include "pp_shoot.h"
#include "storage.h"
#include "delay_handle.h"
#include "colormap.h"
#include "flags.h"
#include "form_ode.h"
#include "load_eqn.h"
#include "parserslow.h"
#include "model.h"
#define VOLTERRA 6
#define BACKEUL 7
#define RKQS 8
#define STIFF 9
#define CVODE 10
#define GEAR 5
#define DP5 11
#define DP83 12
#define RB23 13
#define SYMPLECT 14

/*   This is numerics.c    
 *   The input is primitive and eventually, I want to make it so
	that it uses nice windows for input. 
	For now, I just will let it remain command driven
*/

int cv_bandflag=0,cv_bandupper=1,cv_bandlower=1;

/*   This is the input for the various functions */

/*   I will need access to storage  */

void chk_volterra()
{
  if (xpp::model().nkernel>0)METHOD=VOLTERRA;
}

void  check_pos(int *j)
{
  if(*j<=0)*j=1;
 }

void quick_num(int com)
{
  static const char *const key="tsrdnviobec";
  if(com>=0&&com<11)
    get_num_par(key[com]);
}

void set_total(double total)
{
  int n;
  n=(total/fabs(DELTA_T))+1;
  TEND=n*fabs(DELTA_T);
}

void  get_num_par(char ch)
{
  double temp;
  int tmp;
   switch(ch){
               case 'a':
                       make_adj();
		       break;

		case 't': flash(0);
			 /* total */
			 new_float("total :",&TEND);
			  FOREVER=0;
			  if(TEND<0)
			  {
			    FOREVER=1;
			    TEND=-TEND;
                          }

			flash(0);
			break;
		case 's': flash(1);
			 /* start */
			 new_float("start time :",&T0);
			flash(1);
			break;
		case 'r': flash(2);
			 /* transient */
			 new_float("transient :",&TRANS);
			flash(2);
			break;
		case 'd': flash(3);
			 /* DT */
		         temp=DELTA_T;
			 new_float("Delta t :",&DELTA_T);
		         if(DELTA_T==0.0)DELTA_T=temp;
		         if(DELAY>0.0) {
			  free_delay();
			  if(alloc_delay(DELAY)){
			    INFLAG=0; /*  Make sure no last ics allowed */
			  }
			}
			  else 
			    free_delay();
		       if(xpp::model().nkernel>0){
			 INFLAG=0;
			 xpp::session().integrator.my_start=1;
			 alloc_kernels(1);
		       }
			flash(3);
			break;
		case 'n': flash(4);
			 /* ncline */
			 new_int("ncline mesh :",&NMESH);
                          check_pos(&NMESH);

			flash(4);
			break;
		case 'v':
		        
		         new_int("Maximum iterates :",&BVP_MAXIT);
		         check_pos(&BVP_MAXIT);
		         new_float("Tolerance :",&BVP_TOL);
		         new_float("Epsilon :",&BVP_EPS);
		         reset_bvp();
		         break;
		case 'i': flash(5);
			 /* sing pt */
			 new_int("Maximum iterates :",&EVEC_ITER);
			 check_pos(&EVEC_ITER);
			 new_float("Newton tolerance :",&EVEC_ERR);
			 new_float("Jacobian epsilon :",&NEWT_ERR);
		       if(xpp::model().nflags>0)
			 new_float("SMIN :",&STOL);
		       
			flash(5);
			break;
		case 'o': flash(6);
			 /* noutput */
			new_int("n_out :",&NJMP);
			 check_pos(&NJMP);

			flash(6);
			break;
		case 'b': flash(7);
			 /* bounds */
			new_float("Bounds :",&BOUND); BOUND=fabs(BOUND);

			flash(7);
			break;
		case 'm': flash(8);
			 /* method */
			 get_method();
			 if(METHOD==VOLTERRA&&xpp::model().nkernel==0){
			   err_msg("Volterra only for integral eqns");
			   METHOD=4; 
			 }
		       if(xpp::model().nkernel>0)METHOD=VOLTERRA;
			if(METHOD==GEAR||METHOD==RKQS||METHOD==STIFF)
		{
		 new_float("Tolerance :",&TOLER);
		 new_float("minimum step :",&HMIN);
		 new_float("maximum step :",&HMAX);
		}
			if(METHOD==CVODE||METHOD==DP5||METHOD==DP83||METHOD==RB23)
			  {
			    new_float("Relative tol:",&TOLER);
			    new_float("Abs. Toler:",&ATOLER);
			  }

		       if(METHOD==BACKEUL||METHOD==VOLTERRA){
			 new_float("Tolerance :",&EulTol);
			 new_int("MaxIter :",&MaxEulIter);
		       }
		       if(METHOD==VOLTERRA){
			 tmp=MaxPoints;
			 new_int("MaxPoints:",&tmp);
			 new_int("AutoEval(1=yes) :",&AutoEvaluate);
			 allocate_volterra(tmp,1);
		       }
			 
		       if(METHOD==CVODE||METHOD==RB23)
			 {
			   new_int("Banded system(0/1)?",&cv_bandflag);
			   if(cv_bandflag==1){
			     new_int("Lower band:",&cv_bandlower);
			     new_int("Upper band:",&cv_bandupper);
			   }
			 }
		       if(METHOD==SYMPLECT){
			 if((xpp::model().node%2)!=0){
			   err_msg("Symplectic is only for even dimensions");
			   METHOD=4;
			 }
		       }
			flash(8);
			break;
		case 'e': flash(9);
			 /* delay */
                        if(xpp::model().ndelays==0)break;
			new_float("Maximal delay :",&DELAY);
                        new_float("real guess :", &AlphaMax);
			   new_float("imag guess :", &OmegaMax); 
		        new_int("DelayGrid :",&DelayGrid);
		        if(DELAY>0.0) {
			  free_delay();
			  if(alloc_delay(DELAY)){
			    INFLAG=0; /*  Make sure no last ics allowed */
			  }
			}
			  else 
			    free_delay();
			  
			flash(9);
			break;
		case 'c': flash(10);
			 /* color */
			 if(color_table.enabled==0)break;
			  set_col_par();
			flash(10);
			break;
		    case 'h': flash(11);
		          do_stochast();
		          flash(11);
		          break;      
		case 'f': flash(11);
			 /* FFT */
			flash(11);
			break;
		case 'p': flash(12);
			 /*Poincare map */
		        get_pmap_pars();
			flash(12);
			break;
		case 'u': flash(13);
			 /* ruelle */
                       ruelle();
			flash(13);
			break;
		case 'k': flash(14);
			 /*lookup table */
                        new_lookup();
			flash(14);
			break;
		case 27: 
		       do_meth();
		      TEND=fabs(TEND);
		       alloc_meth();
			show_main_menu(MAIN_MENU);
			break;

		}  /* End num switch */
	   } 

void chk_delay()
{
  if(DELAY>0.0) {
			  free_delay();
			  if(alloc_delay(DELAY)){
			    INFLAG=0; /*  Make sure no last ics allowed */
			  }
			}
			  else 
			    free_delay();
}

void set_delay()
{
 if(xpp::model().ndelays==0)return;
 if(DELAY>0.0){
   free_delay();
   if(alloc_delay(DELAY)){
     INFLAG=0;
   }
 }
}

void ruelle()
{
   new_int("x-axis shift ",&(xpp::session().plot_windows.current->xshft));
   new_int("y-axis shift ",&(xpp::session().plot_windows.current->yshft));
   new_int("z-axis shift",&(xpp::session().plot_windows.current->zshft));
   if(xpp::session().plot_windows.current->xshft<0)xpp::session().plot_windows.current->xshft=0;
   if(xpp::session().plot_windows.current->yshft<0)xpp::session().plot_windows.current->yshft=0;
   if(xpp::session().plot_windows.current->zshft<0)xpp::session().plot_windows.current->zshft=0;
}

void compute_one_period(double period,double *x,const char *name)
{
  int opm=POIMAP;
  double ot=TRANS,ote=TEND;
  TRANS=0;
  T0=0;
  xpp::session().data_store.current_time=0;
  TEND=period;
  POIMAP=0; /* turn off poincare map */
  reset_browser();

  usual_integrate_stuff(x);
  {
    xpp::Writer w(xpp::format("orbit.{}.dat",name).c_str());
    if(w){
      write_mybrowser_data(w.file());
      w.commit();
    }
    else{
      TRANS=ot;
      POIMAP=opm;
      TEND=ote;
      return;
    }
  }
  new_adjoint();
  {
    xpp::Writer w(xpp::format("adjoint.{}.dat",name).c_str());
    if(w){
      write_mybrowser_data(w.file());
      w.commit();
      data_back();
    }
  }
  new_h_fun(1);
  {
    xpp::Writer w(xpp::format("hfun.{}.dat",name).c_str());
    if(w){
      write_mybrowser_data(w.file());
      w.commit();
      data_back();
    }
  }

  reset_browser();

  TRANS=ot;
  POIMAP=opm;
  TEND=ote;

}
void get_pmap_pars_com(int l)
{
 static const char *const mkey="nsmp";
 char ch;
 static const char *n[]={"*0Variable","Section","Direction (+1,-1,0)","Stop on sect(y/n)"};
 std::array<std::string, 4> values;
 static const char *yn[]={"N","Y"};
 int status;
 int i1=POIVAR;

 ch=mkey[l];

 POIMAP=0;
 if(ch=='s')POIMAP=1;
 if(ch=='m')POIMAP=2;
 if(ch=='p')POIMAP=3;

 if(POIMAP==0)return;

 values[0] = ind_to_sym(i1);
 values[1] = xpp::format("{:.16g}", POIPLN);
 values[2] = xpp::format("{}", POISGN);
 values[3] = yn[SOS];
 static const int kinds[]={XPP_FIELD_NAME_IN(0),XPP_FIELD_NUMBER,XPP_FIELD_INTEGER,XPP_FIELD_TEXT};
 status=do_string_box_of(4,1,"Poincare map",n,values,45,kinds);
 if(status!=0){
              find_variable(values[0].c_str(),&i1);
	      if(i1<0) { POIMAP=0;
                         err_msg("No such section");
			 return;
		       }
	      POIVAR=i1;
	      POISGN=atoi(values[2].c_str());
	      if(values[3][0]=='Y'||values[3][0]=='y')SOS=1;
	      else SOS=0;
	      POIPLN=atof(values[1].c_str());
	    }

}

void get_method()
{
 char ch;
 int i;
 ch = static_cast<char>(menu_choose(&menu_method,METHOD));
 for(i=0;i<menu_method.n;i++)
 if(ch==menu_method.keys[i])METHOD=i;
 }

void user_set_color_par(int flag,const char *via,double lo,double hi)
{
  int ivar;
   xpp::session().plot_windows.current->min_scale=lo;
  if(hi>lo)
    xpp::session().plot_windows.current->color_scale=(hi-lo);
  else
    xpp::session().plot_windows.current->color_scale=1;
  
  if(strncasecmp("speed",via,5)==0)
    {
      xpp::session().plot_windows.current->ColorFlag=1;
    }
  else
    {
      find_variable(via,&ivar);
      if(ivar>=0){
	xpp::session().plot_windows.current->ColorValue=ivar;
	xpp::session().plot_windows.current->ColorFlag=2;
      }
      else
	{
	  xpp::session().plot_windows.current->ColorFlag=0; /* no valid colorizing */

	}
    }
  if(flag==0){ /* force overwrite  */
    xpp::session().plot_windows.current->ColorFlag=0;
  
  }

}
 
void set_col_par_com(int i)
   {
    int j,ivar;
    double temp[2];
    float maxder=0.0,minder=0.0,sum=0.0;
    char ch;
   xpp::session().plot_windows.current->ColorFlag=i;
   if(xpp::session().plot_windows.current->ColorFlag==0){
   /* set color to black/white */
    return;
    }
    if(xpp::session().plot_windows.current->ColorFlag==2){
      std::string name=ind_to_sym(xpp::session().plot_windows.current->ColorValue);
      new_string_of("Color via:",name,XPP_FIELD_NAME_IN(0));
      find_variable(name.c_str(),&ivar);

      if(ivar>=0)
	xpp::session().plot_windows.current->ColorValue=ivar;
      else{
	
	err_msg("No such quantity!");
	xpp::session().plot_windows.current->ColorFlag=0;
	return;
      }
    }

   /*   This will be uncommented    ..... */
    ch=TwoChoice("(O)ptimize","(C)hoose","Color","oc");
 
    if(ch=='c')
    {
     temp[0]=xpp::session().plot_windows.current->min_scale;
     temp[1]=xpp::session().plot_windows.current->min_scale+xpp::session().plot_windows.current->color_scale;
     new_float("Min :",&temp[0]);
     new_float("Max :",&temp[1]);
     if(temp[1]>temp[0]&&((xpp::session().plot_windows.current->ColorFlag==2)
     ||(xpp::session().plot_windows.current->ColorFlag==1&&temp[0]>=0.0)))
     {
      xpp::session().plot_windows.current->min_scale=temp[0];
      xpp::session().plot_windows.current->color_scale=(temp[1]-temp[0]);
     }
     else{
       err_msg("Min>=Max or Min<0 error");
     }
     return;
    }
    if(xpp::session().plot_windows.current->ColorFlag==1)
    {
    if(xpp::session().data_store.rows<2)return;
    maxder=0.0;
    minder=1.e20;
  for(i=1;i<xpp::session().browser.view.maxrow;i++)
  {
   sum=0.0;
   for(j=0;j<xpp::model().node;j++)
   sum+=static_cast<float>(fabs(static_cast<double>(xpp::session().browser.view.data[1+j][i]-xpp::session().browser.view.data[1+j][i-1])));
   if(sum<minder)minder=sum;
   if(sum>maxder)maxder=sum;
  }
  if(minder>=0.0&&maxder>minder)
  {
   xpp::session().plot_windows.current->color_scale=(maxder-minder)/(fabs(DELTA_T*NJMP));
   xpp::session().plot_windows.current->min_scale=minder/(fabs(DELTA_T*NJMP));
  }
 }
 else
 {
  get_max(xpp::session().plot_windows.current->ColorValue,&temp[0],&temp[1]);
  xpp::session().plot_windows.current->min_scale=temp[0];
  xpp::session().plot_windows.current->color_scale=(temp[1]-temp[0]);
  if(xpp::session().plot_windows.current->color_scale==0.0)xpp::session().plot_windows.current->color_scale=1.0;
 }
  
}

void do_meth()
{
 if(xpp::model().nkernel>0)METHOD=VOLTERRA;
 switch(METHOD)
 {
  case 0: xpp::session().integrator.solver=discrete; DELTA_T=1;break;
  case 1: xpp::session().integrator.solver=euler;break;
  case 2: xpp::session().integrator.solver=mod_euler;break;
  case 3: xpp::session().integrator.solver=rung_kut;break;
  case 4: xpp::session().integrator.solver=adams;break;
  case 5: NJMP=1;break;
  case 6: xpp::session().integrator.solver=volterra;break;
  case SYMPLECT: 
       xpp::session().integrator.solver=symplect3;
       break;
 case BACKEUL: xpp::session().integrator.solver=bak_euler;break;
 case RKQS:
 case STIFF:
 case CVODE:
 case DP5:
 case DP83:
 case RB23:
   NJMP=1; break;
  default: xpp::session().integrator.solver=rung_kut;
 }
}

