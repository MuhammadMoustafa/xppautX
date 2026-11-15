/* The model's own values (its parameters, ICs, sliders and equilibria) in a Session, by name (W726: moved out of xpp_util, the kernel's). */
#include <algorithm>
#include <cctype>
#include <cmath>
#include <numeric>
#include "model.h"
#include "session_values.h"
#include "typed_values.h"
#include "typed_fields.h"
#include "value_undo.h"
#include "ode_read.h"
#include "xpp_text.h"
#include "xpp_log.h"
#include "xpp_ui.h"
#include "expr.h"
#include "browse.h"
#include "integrate.h"
#include "tabular.h"
#include "volterra2.h"
#include "delay_handle.h"
#include "form_ode.h"
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
#include "session.h"
#include "autox_state.h"
#include "plot_state.h"
#include "storage.h"

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
      temp=.05*std::max(fabs(*vmin),1.0);
     *vmin=*vmin-temp;
     *vmax=*vmax+temp;
    }
 
 }


void   set_default_params(xpp::Session &s, std::optional<int> only)
 {
 for(int i=0;i<s.model().nupar;i++){
   if(only && *only!=i)continue;
   set_val(s.parser,s.model().upar_names[i],s.model().default_val[i]);
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

void set_par_or_var(xpp::Session &s, std::string_view name,int type,int index,double val)
{
  set_val(s.parser,name,val);
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

/* the parameter or IC `name` set to value (a "%name:formula" or do_calc) */
static void set_named(xpp::Session &s, const std::string &name, double value)
{
  const int i=find_user_name(s.model(),PARAMBOX,name);
  set_val(s.parser,name,value);
  if(i>-1){
    redraw_params();
    return;
  }
  s.last_ic[find_user_name(s.model(),ICBOX,name)]=value;
  redraw_ics();
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
    set_val(s.parser,s.model().upar_names[i],*z);
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
   xpp::ok_or_show(xpp::do_init_delay(s,s.numerics->delay));
  }
}

/*  form_ic  --  u_i(0) = F(i)  where  "i" is represented by "t"
    or  
    u[5..20]=f([j]) 
*/

void evaluate_ar_ic(xpp::Session &s, const char *v, const char *f, int j1, int j2)
{
  int j;
  int i,flag;
  double z;
  std::string vp, fp;
  for(j=j1;j<=j2;j++){
    subsk(v,vp,j,1);
    i=find_user_name(s.model(),ICBOX,vp);
    if(i>=0){
      subsk(f,fp,j,1);
      flag=do_calc(s,fp,&z);
      if(flag!=-1)
	s.last_ic[i]=z;
      else 
	return;
    }
  }

}
void arr_ic_start(xpp::Session &s)
{
  int i;
  s.integrator->array_ics=s.model().array_ics; /* the run's own, which Initialconds edits */
  const ArrayIcs &ics=s.integrator->array_ics;
  if(ics.defined==0) return;
  for(i=0;i<NAR_IC;i++){
    if(ics.ics[i].type==2){
      evaluate_ar_ic(s,ics.ics[i].var.c_str(),ics.ics[i].formula.c_str(),
		     ics.ics[i].j1,ics.ics[i].j2);
    }
  }

}

namespace {
xpp::Result<std::vector<int>> ic_problem(std::string what)
{
  return xpp::fail("initial conditions",std::move(what),xpp::command_place());
}
}

xpp::Result<std::vector<int>> set_ic_formula(xpp::Session &s, std::string_view variable, std::string_view formula)
{
  const xpp::Model &m=s.model();
  std::string text(variable),base;
  int j1,j2,array;
  search_array(text.data(),base,&j1,&j2,&array);
  std::vector<int> indices;
  std::vector<double> values;
  if(array!=1){
    const int index=find_user_name(m,ICBOX,variable);
    if(index<0)return ic_problem(xpp::format("{} is not a variable of the model",variable));
    const xpp::Result<double> value=evaluate_formula(s.session_parts,formula);
    if(!value)return ic_problem(xpp::format("{}: {}",formula,value.error().what));
    indices.push_back(index);
    values.push_back(*value);
  }
  else{
    if(j2<j1)return ic_problem(xpp::format("{} is an empty range of variables",variable));
    for(int j=j1;j<=j2;j++){
      xpp::TypedFields fields;
      std::string name,element;
      subsk(base,name,j,1,&fields);
      subsk(formula,element,j,1,&fields);
      if(fields.error())return ic_problem(*fields.error());
      const int index=find_user_name(m,ICBOX,name);
      if(index<0)return ic_problem(xpp::format("{} is not a variable of the model (element {} of {})",name,j,variable));
      const xpp::Result<double> value=evaluate_formula(s.session_parts,element);
      if(!value)return ic_problem(xpp::format("{} for {}: {}",element,name,value.error().what));
      indices.push_back(index);
      values.push_back(*value);
    }
  }
  xpp::push_value_undo(s);
  for(std::size_t k=0;k<indices.size();k++)s.last_ic[static_cast<std::size_t>(indices[k])]=values[k];
  /* the array's formula is kept, as the model's own are (arr_ic_start) */
  if(array==1)array_ic_for(s.integrator->array_ics,base.c_str(),j1,j2).formula=formula;
  redraw_ics();
  return indices;
}

xpp::Result<std::vector<int>> set_ic_array(xpp::Session &s, std::string_view name, int size, std::string_view formula)
{
  const xpp::Model &m=s.model();
  const int first=find_user_name(m,ICBOX,name);
  if(first<0)return ic_problem(xpp::format("{} is not a variable of the model",name));
  if(size<1||size>m.node-first)
    return ic_problem(xpp::format("{} elements from {} do not fit: the model has {} differential equations, {} is number {}",size,name,m.node,name,first+1));
  std::vector<int> indices;
  std::vector<double> values;
  for(int k=0;k<size;k++){
    set_val(s.parser,"t",static_cast<double>(k)); /* the formula's t is the element's offset from the first */
    const xpp::Result<double> value=evaluate_formula(s.session_parts,formula);
    if(!value)return ic_problem(xpp::format("{}: {}",formula,value.error().what));
    indices.push_back(first+k);
    values.push_back(*value);
  }
  for(std::size_t k=0;k<indices.size();k++)s.last_ic[static_cast<std::size_t>(indices[k])]=values[k];
  redraw_ics();
  return indices;
}

} // namespace xpp
