/* Pure helpers: graph bookkeeping, parameters, time. Nothing here
   touches a window. */
#include <cmath>
#include "model.h"
#include "session.h"
#include "ode_read.h"
#include "xpp_util.h"
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

#define DELAYBOX 3
#define BCBOX 4
#define PARAM 1
#define IC 2
#define REAL_SMALL 1.e-6

/* ---- graph bookkeeping ----------------------------------------------- */

void create_a_pop(Session &s)
{
    int i;
    for (i = 1; i < static_cast<int>(s.plot_windows.graph.size()); i++)
        if (s.plot_windows.graph[i].Use == 0) break;
    if (i == static_cast<int>(s.plot_windows.graph.size())) {
        s.plot_windows.graph.emplace_back();
        s.plot_display.emplace_back();
        s.marks_shown.windows.emplace_back();
        s.phase_shown.windows.emplace_back();
        s.plot_windows.current = &s.plot_windows.graph[s.plot_windows.active];
    }
    copy_graph(s, i, s.plot_windows.active);
    s.plot_display[i]={};
    s.marks_shown.windows[i]={};
    s.phase_shown.windows[i]={};
    s.plot_windows.graph[i].x11Wid=extra_plot_width;
    s.plot_windows.graph[i].x11Hgt=extra_plot_height;
    s.plot_windows.graph[i].w = i + 1;
    // Protocol auxiliary windows occupy 101 through 105; plots skip this range.
    if(s.plot_windows.graph[i].w>=WIN_AUTO)s.plot_windows.graph[i].w+=WIN_APLOT-WIN_AUTO+1;
    set_active_windows(s);
    make_active(s, i, 1);
    ui.create_plot_window(s);
    get_draw_area(s);
}

void destroy_a_pop(Session &s)
{
    const int index=s.plot_windows.active;
    if(index==0){
        command_error("windows","Cannot destroy the main plot window");
        return;
    }
    kill_frz(s);
    destroy_labels_and_grobs(s,s.plot_windows.graph[index].w);
    ui.destroy_plot_window(s);
    s.plot_windows.graph[index].Use=0;
    make_active(s,0,1);
    set_active_windows(s);
}

void kill_all_pops(Session &s)
{
    for(int i=1;i<static_cast<int>(s.plot_windows.graph.size());i++){
        if(!s.plot_windows.graph[i].Use)continue;
        make_active(s,i,1);
        destroy_a_pop(s);
    }
    make_active(s,0,1);
}

void make_active(xpp::Session &s, int i, int flag)
{
    s.plot_windows.active = i;
    s.plot_windows.current = &s.plot_windows.graph[s.plot_windows.active];
    s.plot_windows.draw_win = s.plot_windows.current->w;
    get_draw_area_flag(s, flag);
}

int graph_of(const xpp::Session &s, XppWinId w)
{
    for (int i = 0; i < static_cast<int>(s.plot_windows.graph.size()); i++)
        if (s.plot_windows.graph[i].Use && s.plot_windows.graph[i].w == w) return i;
    return 0;
}

void clr_scrn(xpp::Session &s)
{
    ui.blank_draw_window(s);
    do_axes(s);
}

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

int find_user_name(const xpp::Model &m, int type, std::string_view oname)
{
  const auto *entry=m.name(oname);
  if(!entry)return -1;
  return type==PARAMBOX ? entry->parameter : type==ICBOX ? entry->column : -1;
}

int has_eq(std::string_view z, std::string &name, int *where)
{
  size_t i=z.find(':');
  if(i==std::string_view::npos)return(0);
  name.assign(z.substr(0,i));
  *where=static_cast<int>(i)+1;
  return(1);
 }

Result<double> evaluate_formula(xpp::Session &s, std::string_view expr)
{
  std::vector<int> com;
  int i;
  double z=0.0;
  const bool bad=add_expr(s,expr,com,&i)!=0;
  if(!bad)z=evaluate(s,com.data());
  s.parser.ncon=s.model().ncon_start;
  s.parser.nsym=s.model().nsym_start;
  if(bad)return std::unexpected(Error{"formula","Illegal formula ..",command_place(),""});
  return z;
}

double calculate(xpp::Session &s, std::string_view expr, int *ok)
{
  const Result<double> r=evaluate_formula(s,expr);
  *ok=ok_or_show(r)?1:0;
  return r.value_or(0.0);
}

/* a typed value refused: what, and the field it was typed in */
static std::unexpected<Error> refused_value(std::string what,std::string_view field)
{
  return std::unexpected(Error{"value",std::move(what),command_place(),std::string(field)});
}

/* what a %formula (the text after the '%') says: its value, and the name it
   sets when it is "name:formula" (empty otherwise). The one rule of the
   %name:formula form: the name checked, the formula worked out once. */
struct WorkedOut {
  double value=0.0;
  std::string name;
};

static Result<WorkedOut> work_out(xpp::Session &s, std::string_view formula, std::string_view field)
{
  WorkedOut w;
  if(formula.empty())return w; /* nothing typed after the % is 0 */
  int i;
  if(has_eq(formula,w.name,&i)){
    if(find_user_name(s.model(),PARAM,w.name)<0&&find_user_name(s.model(),IC,w.name)<0)
      return refused_value("No such name!",field);
    formula=formula.substr(static_cast<size_t>(i));
  }
  const Result<double> r=evaluate_formula(s,formula);
  if(!r){
    Error e=r.error();
    e.field=field;
    return std::unexpected(std::move(e));
  }
  w.value=*r;
  return w;
}

/* the parameter or IC `name` set to value (a "%name:formula" or do_calc) */
static void set_named(xpp::Session &s, const std::string &name, double value)
{
  const int i=find_user_name(s.model(),PARAM,name);
  set_val(s,name,value);
  if(i>-1){
    redraw_params();
    return;
  }
  s.last_ic[find_user_name(s.model(),IC,name)]=value;
  redraw_ics();
}

Result<double> typed_number(xpp::Session &s, std::string_view typed, std::string_view field)
{
  const std::string_view text=trim_blanks(typed);
  if(text.starts_with('%')){
    const Result<WorkedOut> w=work_out(s,text.substr(1),field);
    if(!w)return std::unexpected(w.error());
    return w->value;
  }
  double v;
  if(!parse_number(text,v))return refused_value(xpp::format("\"{}\" is not a number",text),field);
  return v;
}

int do_calc(xpp::Session &s, std::string_view temp, double *z)
{
  const Result<WorkedOut> w=work_out(s,temp,{});
  if(!ok_or_show(w))return -1;
  *z=w->value;
  if(w->name.empty())return 1;
  set_named(s,w->name,w->value);
  return 0;
}

void set_active_windows(xpp::Session &s)
{
  int i,np=0;
  s.plot_windows.open.resize(s.plot_windows.graph.size());
   for(i=0;i<static_cast<int>(s.plot_windows.graph.size());i++){
   if(s.plot_windows.graph[i].Use==1){
     s.plot_windows.open[np]=i;
     np++;
   }
 }
 s.plot_windows.count=np;
}  

void check_windows(xpp::Session &s)
{
 double zip,zap;
 check_val(&s.plot_windows.current->xmin,&s.plot_windows.current->xmax,&s.plot_windows.current->xbar,&s.plot_windows.current->dx);
 check_val(&s.plot_windows.current->ymin,&s.plot_windows.current->ymax,&s.plot_windows.current->ybar,&s.plot_windows.current->dy);
 check_val(&s.plot_windows.current->zmin,&s.plot_windows.current->zmax,&s.plot_windows.current->zbar,&s.plot_windows.current->dz);
 check_val(&s.plot_windows.current->xlo,&s.plot_windows.current->xhi,&zip,&zap);
 check_val(&s.plot_windows.current->ylo,&s.plot_windows.current->yhi,&zip,&zap);
} 

/* graf_par.h's */
void check_val(double *x1, double *x2, double *xb, double *xd)
{
 double temp;

/* 
  see get_max for details
*/   
      
 if(*x1==*x2){
   temp=.05*lmax(fabs(*x1),1.0);
   *x1=*x1-temp;
   *x2=*x2+temp;
 }
 if(*x1>*x2){
	     temp=*x2;
             *x2=*x1;
             *x1=temp;
	     
            }
	    *xb=.5*(*x1+*x2);
	    *xd=2.0/(*x2-*x1);

}

std::string batch_plot_name(const xpp::Session &s, int i)
{
  const std::string &file=s.model().this_file,&set=s.this_internset;
  const std::string &format=s.plot_export.format;
  return i<0?xpp::format("{:.100}{:.100}.{:.10}",file,set,format)
            :xpp::format("{:.100}{:.100}_{:04d}.{:.10}",file,set,i,format);
}

void dump_ps(xpp::Session &s, int i)
{
  const std::string filename=batch_plot_name(s,i);
   const xpp::ImageFormat *fmt=xpp::find_image_format_by_extension(s.plot_export.format);
   if (!fmt || !fmt->write) {
     xpp::command_error("mkplot",xpp::format("Unsupported batch picture format {}",s.plot_export.format));
     return;
   }
   if(xpp::ok_or_show(xpp::begin_picture(s,*fmt,filename,s.plot_export.color)) && s.plot_file.writer)
     vector_restore(s);
}

void   redo_stuff(xpp::Session &s)
    {
      evaluate_derived(s);
   xpp::re_evaluate_kernels(s);
	  xpp::ok_or_show(redo_all_fun_tables(s));
        evaluate_derived(s);
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

double TypedFields::number(std::string_view field, const std::string &text)
{
  if(error_)return 0;
  const Result<double> r=typed_number(s_,text,field);
  if(r)return *r;
  fail(field,r.error().what);
  return 0;
}

/* the fault of v outside min..max, or nothing */
static std::optional<std::string> out_of_range(double v, std::optional<double> min, std::optional<double> max)
{
  if(min&&v<*min)return xpp::format("{:g} is below {:g}",v,*min);
  if(max&&v>*max)return xpp::format("{:g} is above {:g}",v,*max);
  return std::nullopt;
}

int TypedFields::integer(std::string_view field, const std::string &text, std::optional<double> min,
                         std::optional<double> max)
{
  if(error_)return 0;
  int v=0;
  if(!parse_int(trim_blanks(text),v)){
    fail(field,xpp::format("\"{}\" is not a whole number",text));
    return 0;
  }
  if(const auto bad=out_of_range(v,min,max)){
    fail(field,*bad);
    return 0;
  }
  return v;
}

double TypedFields::real(std::string_view field, const std::string &text, std::optional<double> min,
                         std::optional<double> max)
{
  if(error_)return 0;
  double v=0;
  if(!parse_number(trim_blanks(text),v)){
    fail(field,xpp::format("\"{}\" is not a number",text));
    return 0;
  }
  if(const auto bad=out_of_range(v,min,max)){
    fail(field,*bad);
    return 0;
  }
  return v;
}

bool TypedFields::boolean(std::string_view field, const std::string &text)
{
  if(error_)return false;
  if(text=="true")return true;
  if(text!="false")fail(field,xpp::format("\"{}\" is not true or false",text));
  return false;
}

Color TypedFields::color(std::string_view field, const std::string &text)
{
  if(error_)return foreground_color;
  if(const auto c=parse_color(trim_blanks(text)))return *c;
  fail(field,xpp::format("\"{}\" is not a colour: write #rrggbb, #rrggbbaa, a CSS colour name or foreground",text));
  return foreground_color;
}

int TypedFields::choice(std::string_view field, const std::string &text, std::span<const std::string> choices)
{
  if(error_)return 0;
  for(std::size_t i=0;i<choices.size();i++)
    if(choices[i]==text)return static_cast<int>(i);
  std::string names;
  for(const std::string &c:choices)names+=(names.empty()?"":", ")+c;
  fail(field,xpp::format("\"{}\" is not one of {}",text,names));
  return 0;
}

void TypedFields::fail(std::string_view field, std::string_view what)
{
  if(!error_)error_=xpp::format("{}: {}",field,what);
}

/* the value typed for a number box: checked whole, worked out once, and a
   "%name:formula" also sets its name, whose value the box takes */
static Result<double> box_number(xpp::Session &s,std::string_view text,std::string_view field)
{
  const std::string_view t=trim_blanks(text);
  if(!t.starts_with('%'))return typed_number(s,t,field);
  const Result<WorkedOut> w=work_out(s,t.substr(1),field);
  if(!w)return std::unexpected(w.error());
  if(!w->name.empty())set_named(s,w->name,w->value);
  return w->value;
}

/* store the text typed for entry i of a box of the given type: a number
   (ICs, parameters: a plain number or %formula, nothing else) or, for BCs
   and delays, a string. A refusal changes nothing and is returned, not shown. */
Result<void> box_set_value(xpp::Session &s, int type,int i,std::string_view text,std::string_view field)
{
  switch(type){
  case ICBOX:{
    const Result<double> z=box_number(s,text,field);
    if(!z)return std::unexpected(z.error());
    s.last_ic[i]=*z;
    break;}
  case PARAMBOX:{
    const Result<double> z=box_number(s,text,field);
    if(!z)return std::unexpected(z.error());
    set_val(s,s.model().upar_names[i],*z);
    break;}
  case BCBOX:
    set_bc_formula(s,i,text);
    break;
  case DELAYBOX:
    s.delay_string[i]=text;
    break;
  }
  return {};
}

/* every entry of a box was just stored: recompute what depends on them */
void box_values_loaded(xpp::Session &s, int type)
{
  if(type==PARAMBOX){
    xpp::re_evaluate_kernels(s);
    xpp::ok_or_show(redo_all_fun_tables(s));
  }
  if(type==DELAYBOX){
   xpp::ok_or_show(xpp::do_init_delay(s,s.numerics.delay));
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

/* ---- AUTO's private scratch directory (session.h: AutoState::dir),
   made by xpp::files::make_temp_dir ---- */
void cleanup_auto_dir(xpp::Session &s)
{
  std::string &dir=s.auto_state.dir;
  if (!dir.empty()) {
    xpp::files::remove_temp_dir(dir.c_str());
    dir.clear();
  }
}

void renew_auto_dir(xpp::Session &s)
{
  std::string &dir=s.auto_state.dir;
  if (dir.empty()) return; /* none was made: AUTO writes beside the model */
  xpp::files::remove_temp_dir(dir.c_str());
  dir=xpp::files::make_temp_dir();
}

} // namespace xpp
