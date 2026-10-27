/* The model's own values (its parameters, ICs, sliders and equilibria) and the text helpers they use. */
#include <cmath>
#include "model.h"
#include "session.h"
#include "ode_read.h"
#include "xpp_util.h"
#include "typed_values.h"
#include "xpp_io.h"
#include "xpp_log.h"
#include "xpp_files.h"
#include "xpp_ui.h"
#include "axes2.h"
#include "graphics.h"
#include "xpp_globals.h"
#include "expr.h"
#include "browse.h"
#include "graf_par.h"
#include "grobs.h"
#include "integrate.h"
#include "nullcline.h"
#include "picture.h"
#include "my_svg.h"
#include "image_format.h"
#include "tabular.h"
#include "volterra2.h"
#include "derived.h"
#include "form_ode.h"
#include "delay_handle.h"
#include "numerics.h"
#include <array>
#include <algorithm>
#include <string>
#include <string_view>
#include <time.h>
#include <chrono>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "load_eqn.h"
#include "auto_nox.h"

namespace xpp {

#define REAL_SMALL 1.e-6

std::string utc_now()
{
  return xpp::format("{:%Y-%m-%dT%H:%M:%SZ}", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
}

std::string ind_to_sym(const xpp::Session &s, int ind)
{
 return browse_column_name(s,ind);
}

void  get_max(const xpp::Session &s, int index, double *vmin, double *vmax)
{
   float x0,x1,z;
   double temp;
   int i;
   x0=s.browser.view.data[index][0];
   x1=x0;
   for(i=0;i<s.browser.view.maxrow;i++)
   {
    z=s.browser.view.data[index][i];
    if(z<x0)x0=z;
    if(z>x1)x1=z;
   }
   *vmin=static_cast<double>(x0);
   *vmax=static_cast<double>(x1);
    if(fabs(*vmin-*vmax)<REAL_SMALL){
      temp=.05*lmax(fabs(*vmin),1.0);
     *vmin=*vmin-temp;
     *vmax=*vmax+temp;
    }
 
 }

std::string short_name(std::string_view name, int width)
{
  const size_t w=width>0?static_cast<size_t>(width):0;
  if(name.size()<=w)
    return std::string(name);
  return w?std::string(name.substr(0,w-1))+'~':std::string();
}

void de_space(std::string &s)
{
  de_space(s.data());
  s.resize(strlen(s.c_str()));
}

void de_space(char *s)
{
  int n=strlen(s);
  int i,j=0;
  char ch;
  for(i=0;i<n;i++){
    ch=s[i];
    if(!isspace(ch)){
      s[j]=ch;
      j++;
    }
  }
  s[j]=0;
}

int has_eq(std::string_view z, std::string &name, int *where)
{
  size_t i=z.find(':');
  if(i==std::string_view::npos)return(0);
  name.assign(z.substr(0,i));
  *where=static_cast<int>(i)+1;
  return(1);
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
void vector_restore(xpp::Session &s)
{
  redraw_dfield(s);
  if (s.plot_windows.current->Nullrestore) restore_nullclines(s);
  xpp::restore(s,0,s.browser.view.maxrow);
  do_axes(s);
  draw_label(s,s.plot_windows.draw_win);
  draw_freeze(s,s.plot_windows.draw_win);
  xpp::end_picture(s);
}

void new_parameter(xpp::Session &s)
{
  int done,index;
  double z;
  std::string name;
  while(1){
    name.clear();
    done=new_string_of("Parameter:",name,XPP_FIELD_NAME_IN(2));
    if(name.empty()||done==0){redo_stuff(s); return;}
    if(strncasecmp(name.data(),"DEFAULT",7  )==0){
      set_default_params(s);
      continue;
    }

    else {
      index=find_user_name(s.model(),PARAMBOX,name.data());
      if(index>=0){
	get_val(s,s.model().upar_names[index],&z);
	done=new_float(s,xpp::format("{} :",name),&z);
	if(done==0){
	  set_val(s,s.model().upar_names[index],z);
	  ui.param_box_set(index,xpp::format("{:.16g}",z).c_str());
	  ui.param_box_redraw(index);
	}
        if(done==-1){
         redo_stuff(s);
	  return;
	}
      }
    }
  }
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

void man_ic(xpp::Session &s)
{
  int done,index=0;
  double z;
  while(1){
    z=s.last_ic[index];
    done=new_float(s,xpp::format("{} :",s.model().uvar_names[index]),&z);
    if(done==0){
      s.last_ic[index]=z;
      ui.ic_box_set(index,xpp::format("{:.16g}",z).c_str());
      ui.ic_box_redraw(index);
      index++;
      if(index>=s.model().node+s.model().nmarkov)return;
    }
    if(done==-1)return;
  }
}

/* the ICs box "xvst" (how 0) and "pp" (how 1) buttons: plot the checked
   variables (isck, n entries) and uncheck them */
void plot_checked_vars(xpp::Session &s,int how,int *isck,int n)
{
  int i;
  std::vector<int> selected;
  std::array<int, plot_axes> axes{};
  for(i=0;i<n;i++)
    if(isck[i]){
      selected.push_back(i+1);
      isck[i]=0;
    }
  if(how==0&&!selected.empty())
    graph_all(s,selected.data(),static_cast<int>(selected.size()),0);
  if(how==1&&selected.size()>1){
    if(selected.size()>axes.size()){
      command_error("pp","A phase portrait takes at most three axes");
      return;
    }
    std::copy(selected.begin(),selected.end(),axes.begin());
    graph_all(s,axes.data(),static_cast<int>(selected.size()),1);
  }
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
    if(s.auto_state.homo_side==HomoclinicSide::Left){
      for(i=0;i<n;i++)
	s.auto_state.homo_l[i]=y[i];
      xpp::log(XPP_LOG_INFO, "Saved to left equilibrium\n");
    }
    else {
      for(i=0;i<n;i++)
	s.auto_state.homo_r[i]=y[i];
      xpp::log(XPP_LOG_INFO, "Saved to right equilibrium\n");
    }
    s.auto_state.homo_side=s.auto_state.homo_side==HomoclinicSide::Left?HomoclinicSide::Right:HomoclinicSide::Left;
  }
   redraw_ics();
}

/* cp/rp: complex/real eigenvalues with positive real part, im: imaginary */
const char *eq_stability(int cp, int rp, int im)
{
 if(cp>0||rp>0)return "UNSTABLE";
 else if(im>0)return "NEUTRAL";
 else return "STABLE";
}

/* ---- a comment with an action in the ODE file was picked (logic from
   txtread.c): run its "name=value ..." settings ---- */

void do_txt_action(xpp::Session &s, std::string_view action)
{
 get_graph(s);
 if(const xpp::Result<> r=extract_action(s,action,xpp::Place{s.model().this_file},false);!r){
   show_error(r.error());
   return;
 }
 ping();
  xpp::chk_delay(s);
  redraw_params();
  redraw_ics();
  reset_graph(s);
}

} // namespace xpp
