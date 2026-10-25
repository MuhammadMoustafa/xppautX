
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
#include "xpp_batch.h"

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
    s.numerics.inflag=false;
    s.integrator.my_start=1;
    alloc_kernels(s,1);
  }
}

void set_total(xpp::Session &s, double total)
{
  int n;
  n=(total/fabs(s.numerics.delta_t))+1;
  s.numerics.total_time=n*fabs(s.numerics.delta_t);
}

void  get_num_par(xpp::Session &s, char ch)
{
   switch(ch){
               case 'a':
                       make_adj(s);
		       break;

		case 't': flash(0);
			 numerics_settings_ask(s,"total_time");
			flash(0);
			break;
		case 's': flash(1);
			 numerics_settings_ask(s,"start_time");
			flash(1);
			break;
		case 'r': flash(2);
			 numerics_settings_ask(s,"store_from");
			flash(2);
			break;
		case 'd': flash(3);
			 numerics_settings_ask(s,"dt");
			flash(3);
			break;
		case 'n': flash(4);
			 numerics_settings_ask(s,"nullcline_mesh");
			flash(4);
			break;
		case 'v':
		         numerics_settings_ask(s,"bvp_max_iterates");
		         numerics_settings_ask(s,"bvp_tolerance");
		         numerics_settings_ask(s,"bvp_epsilon");
		         break;
		case 'i': flash(5);
			 /* sing pt */
			 numerics_settings_ask(s,"max_iterations");
			 numerics_settings_ask(s,"newton_tolerance");
			 numerics_settings_ask(s,"jacobian_step");
			 numerics_settings_ask(s,"search_limit");
		       if(s.model().flag_count()>0)
			 new_float(s,"SMIN :",&s.numerics.stol);
		       
			flash(5);
			break;
		case 'o': flash(6);
			 numerics_settings_ask(s,"store_every");
			flash(6);
			break;
		case 'b': flash(7);
			 numerics_settings_ask(s,"state_limit");
			flash(7);
			break;
		case 'm': flash(8);
		       {
			 /* the method picked, refused as `set num` refuses it */
			 const xpp::Place place=command_place();
			 std::string why;
			 if(numerics_settings_set(s,"method",xpp::solver_info(chosen_method(s)).key,why,&place)!=0){
			   show_error(Error{"numerics",why,place});
			   flash(8);
			   break;
			 }
			const xpp::SolverTraits &traits=xpp::solver_info(s.numerics.method).traits;
			if(traits.step_tolerance){
			  numerics_settings_ask(s,"relative_tolerance");
			  numerics_settings_ask(s,"min_step");
			  numerics_settings_ask(s,"max_step");
			}
			if(traits.dense_output)numerics_settings_ask(s,"max_step");
			if(traits.rel_abs_tolerance){
			  numerics_settings_ask(s,"relative_tolerance");
			  numerics_settings_ask(s,"absolute_tolerance");
			}
		       if(traits.newton){
			 numerics_settings_ask(s,"eul_tol");
			 numerics_settings_ask(s,"eul_iter");
		       }
		       if(xpp::solver_info(s.numerics.method).traits.integral_history){
			 int tmp=s.numerics.max_points;
			 new_int("MaxPoints:",&tmp);
			 int auto_evaluate=s.numerics.auto_evaluate;
			 new_int("AutoEval(1=yes) :",&auto_evaluate);
			 s.numerics.auto_evaluate=auto_evaluate!=0;
			 allocate_volterra(s,tmp,1);
		       }
			 
		       if(traits.banded)
			 {
			   int banded=s.numerics.cv_bandflag;
			   new_int("Banded system(0/1)?",&banded);
			   s.numerics.cv_bandflag=banded!=0;
			   if(s.numerics.cv_bandflag){
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
		      s.numerics.total_time=fabs(s.numerics.total_time);
			show_main_menu(s,MAIN_MENU);
			break;

		}  /* End num switch */
	   } 

void chk_delay(xpp::Session &s)
{
  free_delay(s);
  if(s.numerics.delay<=0.0)return;
  const auto allocated=alloc_delay(s,s.numerics.delay);
  if(allocated)s.numerics.inflag=false; /* No last ICs with a fresh history. */
  else if(Load::running())model_failed(allocated.error());
  else show_error(allocated.error());
}

void set_delay(xpp::Session &s)
{
  if(s.model().ndelays>0 && s.numerics.delay>0.0)chk_delay(s);
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
  const PoincareMap opm=s.numerics.poincare_map;
  double ot=s.numerics.store_from,ote=s.numerics.total_time;
  const auto restore = [&]() {
    s.numerics.store_from=ot;
    s.numerics.poincare_map=opm;
    s.numerics.total_time=ote;
  };
  s.numerics.store_from=0;
  s.numerics.start_time=0;
  s.data_store.current_time=0;
  s.numerics.total_time=period;
  s.numerics.poincare_map=PoincareMap::none;
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
 int i1=s.numerics.poincare_variable;

 ch=mkey[l];

 s.numerics.poincare_map=PoincareMap::none;
 if(ch=='s')s.numerics.poincare_map=PoincareMap::section;
 if(ch=='m')s.numerics.poincare_map=PoincareMap::max;
 if(ch=='p')s.numerics.poincare_map=PoincareMap::period;

 if(s.numerics.poincare_map==PoincareMap::none)return;

 values[0] = ind_to_sym(s,i1);
 values[1] = xpp::format("{:.16g}", s.numerics.poincare_plane);
 values[2] = xpp::format("{}", static_cast<int>(s.numerics.poincare_sign));
 values[3] = yn[s.numerics.poincare_stop];
 static const int kinds[]={XPP_FIELD_NAME_IN(0),XPP_FIELD_NUMBER,XPP_FIELD_INTEGER,XPP_FIELD_TEXT};
 status=do_string_box_of(4,1,"Poincare map",n,values,kinds);
 if(status!=0){
              find_variable(s,values[0].c_str(),&i1);
	      if(i1<0) { s.numerics.poincare_map=PoincareMap::none;
                         command_error("numerics", "No such section");
			 return;
		       }
	      s.numerics.poincare_variable=i1;
	      s.numerics.poincare_sign=section_sign(atoi(values[2].c_str()));
	      if(values[3][0]=='Y'||values[3][0]=='y')s.numerics.poincare_stop=true;
	      else s.numerics.poincare_stop=false;
	      s.numerics.poincare_plane=atof(values[1].c_str());
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
   s.plot_windows.current->color_scale=(maxder-minder)/(fabs(s.numerics.delta_t*s.numerics.store_every));
   s.plot_windows.current->min_scale=minder/(fabs(s.numerics.delta_t*s.numerics.store_every));
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
 if(!traits.fixed_step)s.numerics.store_every=1;
 xpp::start_solver(s);
}

} // namespace xpp
