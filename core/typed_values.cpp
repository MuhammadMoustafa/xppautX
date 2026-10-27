/* Reading a value a user typed (a number field, a %formula, a form's fields, a box's entry) and the parameter and IC names it can name: one reading of each, shown by no one; the front ends show what it returns. */
#include "typed_values.h"
#include "session.h"
#include "expr.h"
#include "xpp_ui.h"
#include "xpp_log.h"
#include "browse.h"
#include "xpp_util.h"
#include "model.h"
#include "xpp_io.h"
#include "tabular.h"
#include "volterra2.h"
#include "numerics.h"
#include "graf_par.h"
#include "delay_handle.h"
#include "form_ode.h"

namespace xpp {

int find_user_name(const xpp::Model &m, int type, std::string_view oname)
{
  const auto *entry=m.name(oname);
  if(!entry)return -1;
  return type==PARAMBOX ? entry->parameter : type==ICBOX ? entry->column : -1;
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
    if(find_user_name(s.model(),PARAMBOX,w.name)<0&&find_user_name(s.model(),ICBOX,w.name)<0)
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
  const int i=find_user_name(s.model(),PARAMBOX,name);
  set_val(s,name,value);
  if(i>-1){
    redraw_params();
    return;
  }
  s.last_ic[find_user_name(s.model(),ICBOX,name)]=value;
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

} // namespace xpp
