
#include "xpp_ui.h"
#include "xpp_util.h"
#include "session_values.h"
#include "adj2.h"
#include "integrate.h"
#include "numerics.h"
#include <string>
#include <string_view>
#include <strings.h>

#include <stdlib.h> 
#include <stdio.h>
#include <math.h>
#include <expected>
#include <memory>
#include "browse.h"
#include "volterra2.h"
#include "storage.h"
#include "delay_handle.h"
#include "load_eqn.h"
#include "model.h"
#include "solver.h"
#include "numerics_settings.h"
#include "xpp_io.h"
#include "xpp_batch.h"
#include "session.h"
#include "many_pops.h"
#include "model_options.h"
#include "struct.h"

namespace xpp {

/*   This is numerics.c    
 *   The input is primitive and eventually, I want to make it so
	that it uses nice windows for input. 
	For now, I just will let it remain command driven
*/


/*   This is the input for the various functions */

/*   I will need access to storage  */

Result<> check_numerics_extras(const xpp::Session &s, const NumericsExtras &extras, int method)
{
  const xpp::SolverTraits &traits=xpp::solver_info(method).traits;
  const auto refuse=[](std::string what){return xpp::fail("numerics",std::move(what),command_place());};
  const auto needs=[&](bool applies,std::string_view setting,std::string_view when)->Result<>{
    if(applies)return {};
    return xpp::fail("numerics",xpp::format("{} applies {}",setting,when),command_place());
  };
  const bool volterra=bool(extras.max_points),band=extras.banded||extras.band_lower||extras.band_upper;
  const bool delay=extras.delay_real_guess||extras.delay_imaginary_guess||extras.delay_grid;
  if(volterra)if(auto r=needs(traits.integral_history,"vmaxpts","to the Volterra method");!r)return r;
  if(band)if(auto r=needs(traits.banded,"banded, bandlo and bandup","to a method that can be banded (CVODE)");!r)return r;
  if(delay)if(auto r=needs(s.model().ndelays>0,"delay_real_guess, delay_imaginary_guess and delay_grid","to a model with delays");!r)return r;
  if(extras.max_points&&(*extras.max_points<1||*extras.max_points>option_limit::history_points))
    return refuse(xpp::format("vmaxpts must be a whole number from 1 to {}",option_limit::history_points));
  for(const auto &[name,value]:{std::pair{"bandlo",extras.band_lower},std::pair{"bandup",extras.band_upper}})
    if(value&&(*value<0||*value>option_limit::bandwidth))
      return refuse(xpp::format("{} must be a whole number from 0 to {}",name,option_limit::bandwidth));
  if(extras.delay_grid&&*extras.delay_grid<1)return refuse("delay_grid must be a whole number of at least 1");
  return {};
}

Result<> set_numerics_extras(xpp::Session &s, const NumericsExtras &extras)
{
  if (auto checked = check_numerics_extras(s, extras, s.numerics->method); !checked) return checked;
  const bool delay=extras.delay_real_guess||extras.delay_imaginary_guess||extras.delay_grid;
  /* checked whole: now applied */
  if(extras.max_points)allocate_volterra(s,*extras.max_points,1);
  if(extras.banded)s.numerics->cv_bandflag=*extras.banded;
  if(extras.band_lower){s.numerics->cv_bandlower=*extras.band_lower;if(!extras.banded)s.numerics->cv_bandflag=true;}
  if(extras.band_upper){s.numerics->cv_bandupper=*extras.band_upper;if(!extras.banded)s.numerics->cv_bandflag=true;}
  if(extras.delay_real_guess)s.delay->alpha_max=*extras.delay_real_guess;
  if(extras.delay_imaginary_guess)s.delay->omega_max=*extras.delay_imaginary_guess;
  if(extras.delay_grid)s.delay->grid=*extras.delay_grid;
  if(delay)chk_delay(s);
  return {};
}

void dt_changed(xpp::Session &s)
{
  chk_delay(s);
  if(s.model().nkernel>0){
    s.numerics->inflag=false;
    s.integrator->my_start=1;
    alloc_kernels(s,1);
  }
}

void set_total(xpp::Session &s, double total)
{
  int n;
  n=(total/fabs(s.numerics->delta_t))+1;
  s.numerics->total_time=n*fabs(s.numerics->delta_t);
}

 

void chk_delay(xpp::Session &s)
{
  free_delay(s);
  if(s.numerics->delay<=0.0)return;
  const auto allocated=alloc_delay(s,s.numerics->delay);
  if(allocated)s.numerics->inflag=false; /* No last ICs with a fresh history. */
  else if(Load::running())model_failed(allocated.error());
  else show_error(allocated.error());
}

void set_delay(xpp::Session &s)
{
  if(s.model().ndelays>0 && s.numerics->delay>0.0)chk_delay(s);
}

Result<> set_axis_shifts(xpp::Session &s, int x, int y, int z)
{
   if(x<0||y<0||z<0)return fail("rUelle plot",xpp::format("A shift is 0 or more, not {}",x<0?x:y<0?y:z),command_place());
   GRAPH &g=*s.plot_windows->current;
   g.xshft=x;
   g.yshft=y;
   g.zshft=z;
   redraw_the_graph(s);
   return {};
}

void compute_one_period(xpp::Session &s, double period,double *x,const char *name)
{
  const PoincareMap opm=s.numerics->poincare_map;
  double ot=s.numerics->store_from,ote=s.numerics->total_time;
  const auto restore = [&]() {
    s.numerics->store_from=ot;
    s.numerics->poincare_map=opm;
    s.numerics->total_time=ote;
  };
  s.numerics->store_from=0;
  s.numerics->start_time=0;
  s.data_store->current_time=0;
  s.numerics->total_time=period;
  s.numerics->poincare_map=PoincareMap::none;
  reset_data(s);

  usual_integrate_stuff(s,x);
  {
    xpp::Writer w=xpp::ask_output_writer(s,"Save orbit",".dat",xpp::format("orbit-{}",name));
    if(w){
      write_mytable_data(s,w);
      if (!xpp::ok_or_show(xpp::commit_save(w))) { restore(); return; }
    }
    else{
      restore();
      return;
    }
  }
  if(!xpp::ok_or_show(new_adjoint(s))){restore();return;}
  {
    xpp::Writer w=xpp::ask_output_writer(s,"Save adjoint",".dat",xpp::format("adjoint-{}",name));
    if(w){
      write_mytable_data(s,w);
      const bool saved=xpp::ok_or_show(xpp::commit_save(w));
      data_back(s);
      if (!saved) { restore(); return; }
    }
    else { data_back(s); restore(); return; }
  }
  if(!xpp::ok_or_show(new_h_fun(s,false))){restore();return;}
  {
    xpp::Writer w=xpp::ask_output_writer(s,"Save hfun",".dat",xpp::format("hfun-{}",name));
    if(w){
      write_mytable_data(s,w);
      const bool saved=xpp::ok_or_show(xpp::commit_save(w));
      data_back(s);
      if (!saved) { restore(); return; }
    }
    else { data_back(s); restore(); return; }
  }

  reset_data(s);

  restore();

}
Result<> set_poincare_map(xpp::Session &s, PoincareMap map, std::optional<int> variable, std::optional<double> section, std::optional<SectionSign> direction, std::optional<bool> stop_on_section)
{
 if(map==PoincareMap::none){
   s.numerics->poincare_map=PoincareMap::none;
   return {};
 }
 if(variable&&*variable<0)return fail("numerics","No such section",command_place());
 s.numerics->poincare_map=map;
 if(variable)s.numerics->poincare_variable=*variable;
 if(section)s.numerics->poincare_plane=*section;
 if(direction)s.numerics->poincare_sign=*direction;
 if(stop_on_section)s.numerics->poincare_stop=*stop_on_section;
 return {};
}

void user_set_color_par(xpp::Session &s, int flag,const char *via,double lo,double hi)
{
  int ivar;
   s.plot_windows->current->min_scale=lo;
  if(hi>lo)
    s.plot_windows->current->color_scale=(hi-lo);
  else
    s.plot_windows->current->color_scale=1;
  
  if(strncasecmp("speed",via,5)==0)
    {
      s.plot_windows->current->ColorFlag=1;
    }
  else
    {
      find_variable(s,via,&ivar);
      if(ivar>=0){
	s.plot_windows->current->ColorValue=ivar;
	s.plot_windows->current->ColorFlag=2;
      }
      else
	{
	  s.plot_windows->current->ColorFlag=0; /* no valid colorizing */

	}
    }
  if(flag==0){ /* force overwrite  */
    s.plot_windows->current->ColorFlag=0;
  
  }

}
 
Result<> set_color_coding(xpp::Session &s, ColorBy by, int variable, std::optional<std::pair<double,double>> limits)
{
  GRAPH &graph=*s.plot_windows->current;
  if(by==ColorBy::none){
    graph.ColorFlag=0; /* set color to black/white */
    return {};
  }
  if(by==ColorBy::variable&&variable<0)return fail("numerics","No such quantity!",command_place());
  const int flag=by==ColorBy::velocity?1:2;
  if(limits){
    const auto [low,high]=*limits;
    if(!(high>low&&(flag==2||low>=0.0)))
      return fail("numerics",xpp::format("The color range from {} to {} is empty or, for velocity, starts below 0",xpp::number(low),xpp::number(high)),command_place());
    graph.ColorFlag=flag;
    if(flag==2)graph.ColorValue=variable;
    graph.min_scale=low;
    graph.color_scale=high-low;
    return {};
  }
  if(flag==1){
    if(s.data_store->rows<2)
      return fail("numerics","Coloring by velocity needs a trajectory to find its range from: run one, or give the lowest and highest velocity",command_place());
    float maxder=0.0,minder=1.e20;
    for(int i=1;i<s.data->view.maxrow;i++){
      float sum=0.0;
      for(int j=0;j<s.model().node;j++)
        sum+=static_cast<float>(fabs(static_cast<double>(s.data->view.data[1+j][i]-s.data->view.data[1+j][i-1])));
      if(sum<minder)minder=sum;
      if(sum>maxder)maxder=sum;
    }
    graph.ColorFlag=flag;
    if(minder>=0.0&&maxder>minder){
      graph.color_scale=(maxder-minder)/(fabs(s.numerics->delta_t*s.numerics->store_every));
      graph.min_scale=minder/(fabs(s.numerics->delta_t*s.numerics->store_every));
    }
    return {};
  }
  double low,high;
  get_max(s,variable,&low,&high);
  graph.ColorFlag=flag;
  graph.ColorValue=variable;
  graph.min_scale=low;
  graph.color_scale=high-low;
  if(graph.color_scale==0.0)graph.color_scale=1.0;
  return {};
}

void do_meth(xpp::Session &s)
{
 const xpp::SolverTraits &traits=xpp::solver_info(s.numerics->method).traits;
 if(traits.discrete)s.numerics->delta_t=1;
 /* a method that picks its own steps stores every output time */
 if(!traits.fixed_step)s.numerics->store_every=1;
 xpp::start_solver(s);
}

} // namespace xpp
