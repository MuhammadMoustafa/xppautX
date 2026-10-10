/* The derived quantities (derived.h): a formula of the parameters worked
   out only when one changes (.ode's !name = expr, and what the Model
   builder finds reads only parameters, consts and pure functions,
   form_ode.cpp). They are the Model's (Model::derived), so a model loaded
   after another starts with none. */
#include "derived.h"
#include "expr.h"
#include "session.h"
#include "xpp_batch.h"
#include "xpp_log.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace xpp {

/* This compiles all of the formulae
It is called only once during the session
*/
void compile_derived(xpp::Session &s)
{
  std::array<int, 256> f;
  int n;
  for (xpp::Model::DerivedQuantity &d : s.model().derived) {
    if (add_expr(s, d.rhs, f.data(), &n) == 1)
      model_failed(xpp::Error{"derived", xpp::format("Bad right-hand side for a derived parameter: {}", d.rhs), d.where});
    d.form.rpn.assign(f.begin(), f.begin() + n);
  }
  evaluate_derived(s);
}

/* This evaluates all derived quantities in order of definition
called before any integration or numerical computation
and after changing parameters and constants
*/
void evaluate_derived(xpp::Session &s)
{
  std::array<double, MAXPAR> &constants = s.parser.constants;
  for (xpp::Model::DerivedQuantity &d : s.model().derived) constants[d.index] = evaluate(s, d.form);
}

/* this adds a derived quantity  */
int add_derived(xpp::Session &s, std::string_view name, std::string_view rhs)
{
  xpp::Model::DerivedQuantity d;
  d.rhs = rhs;
  /* this is the constant to which it addresses */
  d.index = s.parser.ncon;
  d.where = xpp::Load::place();
  /* add the name to the recognized symbols */
  xpp::log(XPP_LOG_INFO, " derived constant[{}] is {} = {}\n", s.parser.ncon, name, rhs);
  s.model().derived.push_back(std::move(d));
  return add_con(s, name, 0.0);
}

} // namespace xpp
