
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
#include "pp_shoot.h"
#include "storage.h"
#include "delay_handle.h"
#include "colormap.h"
#include "flags.h"
#include "form_ode.h"
#include "load_eqn.h"
#include "expr.h"
#include "model.h"
#include "solver.h"
#include "numerics_settings.h"
#include "xpp_io.h"

namespace xpp {

/*   This is numerics.c    
 *   The input is primitive and eventually, I want to make it so
	that it uses nice windows for input. 
	For now, I just will let it remain command driven
*/


/*   This is the input for the various functions */

/*   I will need access to storage  */

void quick_num(xpp::Session &s, int com)
{
  static const char *const key="tsrdnviobec";
  if(com>=0&&com<11)
    get_num_par(s,key[com]);
}

void dt_changed(xpp::Session &s)
{
  chk_delay(s);
  if(s.model().nkernel>0){
    s.numerics.inflag=0;
    s.integrator.my_start=1;
    alloc_kernels(s,1);
  }
}

void set_total(xpp::Session &s, double total)
{
  int n;
  n=(total/fabs(s.numerics.delta_t))+1;
  s.numerics.tend=n*fabs(s.numerics.delta_t);
}

void  get_num_par(xpp::Session &s, char ch)
{
   switch(ch){
               case 'a':
                       make_adj(s);
		       break;

		case 't': flash(0);
			 numerics_settings_ask(s,"total");
			flash(0);
			break;
		case 's': flash(1);
			 numerics_settings_ask(s,"t0");
			flash(1);
			break;
		case 'r': flash(2);
			 numerics_settings_ask(s,"trans");
			flash(2);
			break;
		case 'd': flash(3);
			 numerics_settings_ask(s,"dt");
			flash(3);
			break;
		case 'n': flash(4);
			 numerics_settings_ask(s,"nmesh");
			flash(4);
			break;
		case 'v':
		         numerics_settings_ask(s,"bvp_maxit");
		         numerics_settings_ask(s,"bvp_tol");
		         numerics_settings_ask(s,"bvp_eps");
		         break;
		case 'i': flash(5);
			 /* sing pt */
			 numerics_settings_ask(s,"newt_iter");
			 numerics_settings_ask(s,"newt_tol");
			 numerics_settings_ask(s,"jac_eps");
		       if(s.model().nflags>0)
			 new_float(s,"SMIN :",&s.numerics.stol);
		       
			flash(5);
			break;
		case 'o': flash(6);
			 numerics_settings_ask(s,"nout");
			flash(6);
			break;
		case 'b': flash(7);
			 numerics_settings_ask(s,"bound");
			flash(7);
			break;
		case 'm': flash(8);
		       {
			 /* the method picked, refused as `set num` refuses it */
			 const xpp::Place place=command_place();
			 std::string why;
			 if(numerics_settings_set(s,"method",xpp::solver_info(chosen_method(s)).name,why,&place)!=0){
			   show_error(Error{"numerics",why,place});
			   flash(8);
			   break;
			 }
			const xpp::SolverTraits &traits=xpp::solver_info(s.numerics.method).traits;
			if(traits.step_tolerance){
			  numerics_settings_ask(s,"tol");
			  numerics_settings_ask(s,"dtmin");
			  numerics_settings_ask(s,"dtmax");
			}
			if(traits.rel_abs_tolerance){
			  numerics_settings_ask(s,"tol");
			  numerics_settings_ask(s,"atol");
			}
		       if(traits.newton){
			 numerics_settings_ask(s,"eul_tol");
			 numerics_settings_ask(s,"eul_iter");
		       }
		       if(xpp::solver_info(s.numerics.method).traits.integral_history){
			 int tmp=s.numerics.max_points;
			 new_int("MaxPoints:",&tmp);
			 new_int("AutoEval(1=yes) :",&s.numerics.auto_evaluate);
			 allocate_volterra(s,tmp,1);
		       }
			 
		       if(traits.banded)
			 {
			   new_int("Banded system(0/1)?",&s.numerics.cv_bandflag);
			   if(s.numerics.cv_bandflag==1){
			     new_int("Lower band:",&s.numerics.cv_bandlower);
			     new_int("Upper band:",&s.numerics.cv_bandupper);
			   }
			 }
		       }
			flash(8);
			break;
		case 'e': flash(9);
			 /* delay */
                        if(s.model().ndelays==0)break;
			numerics_settings_ask(s,"delay");
                        new_float(s,"real guess :", &s.delay.alpha_max);
			   new_float(s,"imag guess :", &s.delay.omega_max); 
		        new_int("DelayGrid :",&s.delay.grid);
		        chk_delay(s);

			flash(9);
			break;
		case 'c': flash(10);
			 /* color */
			 if(color_table.enabled==0)break;
			  set_col_par(s);
			flash(10);
			break;
		    case 'h': flash(11);
		          do_stochast(s);
		          flash(11);
		          break;      
		case 'p': flash(12);
			 /*Poincare map */
		        get_pmap_pars(s);
			flash(12);
			break;
		case 'u': flash(13);
			 /* ruelle */
                       ruelle(s);
			flash(13);
			break;
		case 'k': flash(14);
			 /*lookup table */
                        new_lookup(s);
			flash(14);
			break;
		case 27: 
		       do_meth(s);
		      s.numerics.tend=fabs(s.numerics.tend);
			show_main_menu(s,MAIN_MENU);
			break;

		}  /* End num switch */
	   } 

void chk_delay(xpp::Session &s)
{
  if(s.numerics.delay>0.0) {
			  free_delay(s);
			  if(alloc_delay(s,s.numerics.delay)){
			    s.numerics.inflag=0; /*  Make sure no last ics allowed */
			  }
			}
			  else 
			    free_delay(s);
}

void set_delay(xpp::Session &s)
{
 if(s.model().ndelays==0)return;
 if(s.numerics.delay>0.0){
   free_delay(s);
   if(alloc_delay(s,s.numerics.delay)){
     s.numerics.inflag=0;
   }
 }
}

void ruelle(xpp::Session &s)
{
   new_int("x-axis shift ",&(s.plot_windows.current->xshft));
   new_int("y-axis shift ",&(s.plot_windows.current->yshft));
   new_int("z-axis shift",&(s.plot_windows.current->zshft));
   if(s.plot_windows.current->xshft<0)s.plot_windows.current->xshft=0;
   if(s.plot_windows.current->yshft<0)s.plot_windows.current->yshft=0;
   if(s.plot_windows.current->zshft<0)s.plot_windows.current->zshft=0;
}

void compute_one_period(xpp::Session &s, double period,double *x,const char *name)
{
  int opm=s.numerics.poimap;
  double ot=s.numerics.trans,ote=s.numerics.tend;
  const auto restore = [&]() {
    s.numerics.trans=ot;
    s.numerics.poimap=opm;
    s.numerics.tend=ote;
  };
  s.numerics.trans=0;
  s.numerics.t0=0;
  s.data_store.current_time=0;
  s.numerics.tend=period;
  s.numerics.poimap=0; /* turn off poincare map */
  reset_browser(s);

  usual_integrate_stuff(s,x);
  {
    xpp::Writer w=xpp::ask_output_writer(s,"Save orbit",".dat",xpp::format("orbit-{}",name));
    if(w){
      write_mybrowser_data(s,w);
      if (!xpp::ok_or_show(xpp::commit_save(w))) { restore(); return; }
    }
    else{
      restore();
      return;
    }
  }
  new_adjoint(s);
  {
    xpp::Writer w=xpp::ask_output_writer(s,"Save adjoint",".dat",xpp::format("adjoint-{}",name));
    if(w){
      write_mybrowser_data(s,w);
      const bool saved=xpp::ok_or_show(xpp::commit_save(w));
      data_back(s);
      if (!saved) { restore(); return; }
    }
    else { data_back(s); restore(); return; }
  }
  new_h_fun(s,1);
  {
    xpp::Writer w=xpp::ask_output_writer(s,"Save hfun",".dat",xpp::format("hfun-{}",name));
    if(w){
      write_mybrowser_data(s,w);
      const bool saved=xpp::ok_or_show(xpp::commit_save(w));
      data_back(s);
      if (!saved) { restore(); return; }
    }
    else { data_back(s); restore(); return; }
  }

  reset_browser(s);

  restore();

}
void get_pmap_pars_com(xpp::Session &s, int l)
{
 static const char *const mkey="nsmp";
 char ch;
 static const char *const n[]={"*0Variable","Section","Direction (+1,-1,0)","Stop on sect(y/n)"};
 std::array<std::string, 4> values;
 static const char *const yn[]={"N","Y"};
 int status;
 int i1=s.numerics.poivar;

 ch=mkey[l];

 s.numerics.poimap=0;
 if(ch=='s')s.numerics.poimap=1;
 if(ch=='m')s.numerics.poimap=2;
 if(ch=='p')s.numerics.poimap=3;

 if(s.numerics.poimap==0)return;

 values[0] = ind_to_sym(s,i1);
 values[1] = xpp::format("{:.16g}", s.numerics.poipln);
 values[2] = xpp::format("{}", s.numerics.poisgn);
 values[3] = yn[s.numerics.sos];
 static const int kinds[]={XPP_FIELD_NAME_IN(0),XPP_FIELD_NUMBER,XPP_FIELD_INTEGER,XPP_FIELD_TEXT};
 status=do_string_box_of(4,1,"Poincare map",n,values,kinds);
 if(status!=0){
              find_variable(s,values[0].c_str(),&i1);
	      if(i1<0) { s.numerics.poimap=0;
                         command_error("numerics", "No such section");
			 return;
		       }
	      s.numerics.poivar=i1;
	      s.numerics.poisgn=atoi(values[2].c_str());
	      if(values[3][0]=='Y'||values[3][0]=='y')s.numerics.sos=1;
	      else s.numerics.sos=0;
	      s.numerics.poipln=atof(values[1].c_str());
	    }

}

int chosen_method(const xpp::Session &s)
{
 const char ch = static_cast<char>(menu_choose(&menu_method,s.numerics.method));
 for(int i=0;i<menu_method.n;i++)
   if(ch==menu_method.keys[i])return i;
 return s.numerics.method;
}

void user_set_color_par(xpp::Session &s, int flag,const char *via,double lo,double hi)
{
  int ivar;
   s.plot_windows.current->min_scale=lo;
  if(hi>lo)
    s.plot_windows.current->color_scale=(hi-lo);
  else
    s.plot_windows.current->color_scale=1;
  
  if(strncasecmp("speed",via,5)==0)
    {
      s.plot_windows.current->ColorFlag=1;
    }
  else
    {
      find_variable(s,via,&ivar);
      if(ivar>=0){
	s.plot_windows.current->ColorValue=ivar;
	s.plot_windows.current->ColorFlag=2;
      }
      else
	{
	  s.plot_windows.current->ColorFlag=0; /* no valid colorizing */

	}
    }
  if(flag==0){ /* force overwrite  */
    s.plot_windows.current->ColorFlag=0;
  
  }

}
 
void set_col_par_com(xpp::Session &s, int i)
   {
    int j,ivar;
    double temp[2];
    float maxder=0.0,minder=0.0,sum=0.0;
    char ch;
   s.plot_windows.current->ColorFlag=i;
   if(s.plot_windows.current->ColorFlag==0){
   /* set color to black/white */
    return;
    }
    if(s.plot_windows.current->ColorFlag==2){
      std::string name=ind_to_sym(s,s.plot_windows.current->ColorValue);
      new_string_of("Color via:",name,XPP_FIELD_NAME_IN(0));
      find_variable(s,name.c_str(),&ivar);

      if(ivar>=0)
	s.plot_windows.current->ColorValue=ivar;
      else{
	
	command_error("numerics", "No such quantity!");
	s.plot_windows.current->ColorFlag=0;
	return;
      }
    }

   /*   This will be uncommented    ..... */
    ch=TwoChoice("(O)ptimize","(C)hoose","Color","oc");
 
    if(ch=='c')
    {
     temp[0]=s.plot_windows.current->min_scale;
     temp[1]=s.plot_windows.current->min_scale+s.plot_windows.current->color_scale;
     new_float(s,"Min :",&temp[0]);
     new_float(s,"Max :",&temp[1]);
     if(temp[1]>temp[0]&&((s.plot_windows.current->ColorFlag==2)
     ||(s.plot_windows.current->ColorFlag==1&&temp[0]>=0.0)))
     {
      s.plot_windows.current->min_scale=temp[0];
      s.plot_windows.current->color_scale=(temp[1]-temp[0]);
     }
     else{
       command_error("numerics", "Min>=Max or Min<0 error");
     }
     return;
    }
    if(s.plot_windows.current->ColorFlag==1)
    {
    if(s.data_store.rows<2)return;
    maxder=0.0;
    minder=1.e20;
  for(i=1;i<s.browser.view.maxrow;i++)
  {
   sum=0.0;
   for(j=0;j<s.model().node;j++)
   sum+=static_cast<float>(fabs(static_cast<double>(s.browser.view.data[1+j][i]-s.browser.view.data[1+j][i-1])));
   if(sum<minder)minder=sum;
   if(sum>maxder)maxder=sum;
  }
  if(minder>=0.0&&maxder>minder)
  {
   s.plot_windows.current->color_scale=(maxder-minder)/(fabs(s.numerics.delta_t*s.numerics.njmp));
   s.plot_windows.current->min_scale=minder/(fabs(s.numerics.delta_t*s.numerics.njmp));
  }
 }
 else
 {
  get_max(s,s.plot_windows.current->ColorValue,&temp[0],&temp[1]);
  s.plot_windows.current->min_scale=temp[0];
  s.plot_windows.current->color_scale=(temp[1]-temp[0]);
  if(s.plot_windows.current->color_scale==0.0)s.plot_windows.current->color_scale=1.0;
 }
  
}

void do_meth(xpp::Session &s)
{
 const xpp::SolverTraits &traits=xpp::solver_info(s.numerics.method).traits;
 if(traits.discrete)s.numerics.delta_t=1;
 /* a method that picks its own steps stores every output time */
 if(!traits.fixed_step)s.numerics.njmp=1;
 xpp::start_solver(s);
}

} // namespace xpp
