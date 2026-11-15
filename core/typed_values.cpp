/* Reading a value a user typed (a number field, a %formula, a form's fields, a box's entry) and the parameter and IC names it can name: one reading of each, shown by no one; the front ends show what it returns. */
#include <cstddef>
#include <expected>
#include <memory>
#include <utility>
#include <vector>
#include "typed_values.h"
#include "expr.h"
#include "xpp_ui.h"
#include "xpp_util.h"
#include "model.h"
#include "model_record.h"
#include "xpp_text.h"
#include "session.h"

namespace xpp {

int find_user_name(const xpp::Model &m, int type, std::string_view oname)
{
  const auto *entry=m.record->name(oname);
  if(!entry)return -1;
  return type==PARAMBOX ? entry->parameter : type==ICBOX ? entry->column : -1;
}

/* expr compiled in s, and worked out when `value`; the symbols it added are taken back */
static Result<double> worked_formula(xpp::Session &s, std::string_view expr, bool value)
{
  std::vector<int> com;
  int i;
  double z=0.0;
  const bool bad=add_expr(s,expr,com,&i)!=0;
  if(!bad&&value)z=evaluate(s,com);
  s.parser.ncon=s.model().ncon_start;
  s.parser.nsym=s.model().nsym_start;
  if(bad)return std::unexpected(Error{"formula","Illegal formula ..",command_place(),""});
  return z;
}

Result<double> evaluate_formula(xpp::Session &s, std::string_view expr)
{
  return worked_formula(s,expr,true);
}

Result<> compile_formula(xpp::Session &s, std::string_view expr)
{
  const Result<double> compiled=worked_formula(s,expr,false);
  if(!compiled)return std::unexpected(compiled.error());
  return {};
}

/* a typed value refused: what, and the field it was typed in */
static std::unexpected<Error> refused_value(std::string what,std::string_view field)
{
  return std::unexpected(Error{"value",std::move(what),command_place(),std::string(field)});
}

Result<WorkedOut> work_out(xpp::Session &s, std::string_view formula, std::string_view field)
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

/* a slider names a parameter (PARAMBOX) or a variable (ICBOX); 0 if
   neither */
int find_par_or_var(const Model &m, std::string_view name,int *type,int *index)
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

} // namespace xpp
