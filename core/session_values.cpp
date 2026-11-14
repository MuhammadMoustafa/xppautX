/* The model's own values (its parameters, ICs, sliders and equilibria) in a Session, by name (W726: moved out of xpp_util, the kernel's). */
#include <algorithm>
#include <cctype>
#include <cmath>
#include <numeric>
#include "model.h"
#include "session_values.h"
#include "typed_values.h"
#include "xpp_io.h"
#include "xpp_log.h"
#include "xpp_ui.h"
#include "axes2.h"
#include "expr.h"
#include "browse.h"
#include "graf_par.h"
#include "grobs.h"
#include "integrate.h"
#include "nullcline.h"
#include "image_format.h"
#include "tabular.h"
#include "volterra2.h"
#include "arrayplot.h"
#include <string>
#include <span>
#include <string_view>
#include <time.h>
#include <chrono>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <memory>
#include "load_eqn.h"
#include "session.h"
#include "autox_state.h"
#include "many_pops.h"
#include "storage.h"
#include "struct.h"

namespace xpp {

#define REAL_SMALL 1.e-6

std::string ind_to_sym(const xpp::Session &s, int ind)
{
 return browse_column_name(s,ind);
}

void  get_max(const xpp::Session &s, int index, double *vmin, double *vmax)
{
   double x0,x1,z,temp;
   int i;
   x0=s.data->view.data[index][0];
   x1=x0;
   for(i=0;i<s.data->view.maxrow;i++)
   {
    z=s.data->view.data[index][i];
    if(z<x0)x0=z;
    if(z>x1)x1=z;
   }
   *vmin=x0;
   *vmax=x1;
    if(fabs(*vmin-*vmax)<REAL_SMALL){
      temp=.05*lmax(fabs(*vmin),1.0);
     *vmin=*vmin-temp;
     *vmax=*vmax+temp;
    }
 
 }


void user_fun_info(const xpp::Model &m, FILE *fp)
{
  for(int j=0;j<m.nfun;j++){
    std::string line=xpp::format("{}(",m.ufun_names[j]);
    for(int i=0;i<m.narg_fun[j];i++)
      line+=xpp::format("{}{}",m.ufun_args[j][i],i<m.narg_fun[j]-1?",":"");
    line+=xpp::format(") = {}\n",m.ufun_defs[j]);
    fwrite(line.data(),1,line.size(),fp);
  }
}

/* a PDF or SVG export: what the session holds for the window, drawn into the picture begun, which is then ended */
xpp::Result<> vector_restore(xpp::Session &s)
{
  redraw_dfield(s);
  if (s.plot_windows->current->Nullrestore) restore_nullclines(s);
  xpp::restore(s,0,s.data->view.maxrow);
  do_axes(s);
  draw_label(s,s.plot_windows->draw_win);
  draw_freeze(s,s.plot_windows->draw_win);
  return xpp::end_picture(s);
}

void   set_default_params(xpp::Session &s, std::optional<int> only)
 {
 for(int i=0;i<s.model().nupar;i++){
   if(only && *only!=i)continue;
   set_val(s,s.model().upar_names[i],s.model().default_val[i]);
   ui.param_box_set(i,xpp::format("{:.16g}",s.model().default_val[i]).c_str());
 }
 
 redraw_params();
 xpp::re_evaluate_kernels(s);
 xpp::ok_or_show(redo_all_fun_tables(s));
 }

/* ---- the values behind the IC, parameter, BC and delay boxes and the
   parameter sliders (logic from init_conds.c; the widgets stay there) ---- */

void   set_default_ics(xpp::Session &s, std::optional<int> only)
{
  for(int i=0;i<s.model().node+s.model().nmarkov;i++)
    if(!only || *only==i)s.last_ic[i]=s.model().default_ic[i];
   redraw_ics();
}

/* the data columns of variables `variables` (1 is the first variable) plotted: curves against time, a phase
   portrait of two or three axes, or an array plot over the variables from the first to the last */
Result<> plot_variables(xpp::Session &s,PlotHow how,std::span<const int> variables)
{
  std::vector<int> list(variables.begin(),variables.end());
  switch(how){
  case PlotHow::time_series:
    if(list.empty())return fail("plot","Give at least one variable to plot against time",command_place());
    graph_all(s,list.data(),static_cast<int>(list.size()),0);
    break;
  case PlotHow::phase_plane:
    if(list.size()<2||list.size()>static_cast<std::size_t>(plot_axes))return fail("plot",xpp::format("A phase portrait takes two or three axes, not {}",list.size()),command_place());
    graph_all(s,list.data(),static_cast<int>(list.size()),1);
    break;
  case PlotHow::array_plot:
    if(list.size()!=2||list[0]>=list[1])return fail("plot","An array plot goes from one variable to a later one: give those two",command_place());
    if(s.data->view.maxrow<2)return fail("plot","An array plot needs stored data: run first",command_place());
    optimize_aplot(s,list.data());
    break;
  }
  return {};
}

/* why slider cannot be one of m's: a name that is no parameter or
   variable (an empty one is an empty slot), a range not low < high, a
   step outside it; nullopt when it can */
std::optional<std::string> slider_wrong(const Model &m, const XppSlider &slider, bool check_names)
{
  int type, index;
  if(check_names&&!slider.var.empty()&&!find_par_or_var(m,slider.var,&type,&index))
    return xpp::format("slider name {}: the model has no parameter or variable",slider.var);
  if(!std::isfinite(slider.lo)||!std::isfinite(slider.hi)||!(slider.lo<slider.hi))
    return xpp::format("slider range {} {}: finite low < high required",slider.lo,slider.hi);
  if(!std::isfinite(slider.step)||slider.step<0||slider.step>slider.hi-slider.lo)
    return xpp::format("slider step {}: zero (automatic) or a positive step within the range required",slider.step);
  return std::nullopt;
}

/* a slider names a parameter (PARAMBOX) or a variable (ICBOX); 0 if
   neither */
int find_par_or_var(const xpp::Model &m, std::string_view name,int *type,int *index)
{
  int status=find_user_name(m,PARAMBOX,name);
  if(status==-1){
    status=find_user_name(m,ICBOX,name);
    if(status==-1)return 0;
    *type=ICBOX;
  }
  else *type=PARAMBOX;
  *index=status;
  return 1;
}

void set_par_or_var(xpp::Session &s, std::string_view name,int type,int index,double val)
{
  set_val(s,name,val);
  if(type==ICBOX)
    s.last_ic[index]=val;
}

/* ---- the equilibrium window's Import button and its label (logic from
   eig_list.c) ---- */

/* make equilibrium y (n values) the initial data; for small systems it is
   also saved alternately as the left/right equilibrium for homoclinics */
void eq_import(xpp::Session &s, double *y,int n)
{
  int i;
  set_initial_data(s,{y,static_cast<size_t>(n)});

  if(n<20){
    if(s.autox_state->homo_side==HomoclinicSide::Left){
      for(i=0;i<n;i++)
	s.autox_state->homo_l[i]=y[i];
      xpp::log(XPP_LOG_INFO, "Saved to left equilibrium\n");
    }
    else {
      for(i=0;i<n;i++)
	s.autox_state->homo_r[i]=y[i];
      xpp::log(XPP_LOG_INFO, "Saved to right equilibrium\n");
    }
    s.autox_state->homo_side=s.autox_state->homo_side==HomoclinicSide::Left?HomoclinicSide::Right:HomoclinicSide::Left;
  }
   redraw_ics();
}

} // namespace xpp
