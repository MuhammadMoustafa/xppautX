/* The derived quantities (derived.h): a formula of the parameters worked
   out only when one changes (.ode's !name = expr, and what the Model
   builder finds reads only parameters, consts and pure functions,
   form_ode.cpp). They are the Model's (Model::derived), so a model loaded
   after another starts with none. */
#include "derived.h"
#include "expr.h"
#include "model_failed.h"
#include "xpp_log.h"
#include "expr_program.h"
#include "native_code.h"
#include "xpp_error.h"
#include "xpp_text.h"

#include <string>
#include <utility>
#include <vector>

namespace xpp {

/* This compiles all of the formulae
It is called only once during the session
*/
void compile_derived(ParserState &p)
{
  int n;
  for (xpp::CompilerTables::DerivedQuantity &d : p.compiler->derived) {
    if (add_expr(p, d.rhs, d.form.rpn, &n) == 1)
      model_failed(xpp::Error{"derived", xpp::format("Bad right-hand side for a derived parameter: {}", d.rhs), d.where});
  }
  evaluate_derived(p);
}

/* This evaluates all derived quantities in order of definition
called before any integration or numerical computation
and after changing parameters and constants
*/
void evaluate_derived(ParserState &p)
{
  if (p.compiler->derived_native()) {
    p.native_work+=p.compiler->native_derived_nodes;
    native::call(p.compiler->native_derived,p.constants.data(),p.variables.data(),&p,nullptr);
    return;
  }
  for (const auto &d : p.compiler->derived)
    p.constants[d.index] = evaluate(p,d.form);
}

void evaluate_derived_pure(const ParserState &p, double *constants, double *variables)
{
  if (p.compiler->derived_native()) {
    native::call(p.compiler->native_derived,constants,variables,const_cast<ParserState *>(&p),nullptr);
    return;
  }
  for (const auto &d : p.compiler->derived)
    constants[d.index] = evaluate_pure(p,d.form,constants,variables);
}

/* this adds a derived quantity  */
int add_derived(ParserState &p, std::string_view name, std::string_view rhs)
{
  xpp::CompilerTables::DerivedQuantity d;
  d.rhs = rhs;
  /* this is the constant to which it addresses */
  d.index = p.ncon;
  d.where = xpp::LoadSite::place();
  /* add the name to the recognized symbols */
  xpp::log(XPP_LOG_INFO, " derived constant[{}] is {} = {}\n", p.ncon, name, rhs);
  p.compiler->derived.push_back(std::move(d));
  return add_con(p, name, 0.0);
}

} // namespace xpp
