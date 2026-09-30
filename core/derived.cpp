/* The derived quantities (derived.h): a formula of the parameters worked
   out only when one changes (.ode's !name = expr, and what the Model
   builder finds reads only parameters, consts and pure functions,
   form_ode.cpp). They are the Model's (Model::derived), so a model loaded
   after another starts with none. */
#include "derived.h"
#include "expr.h"
#include "session.h"
#include "xpp_log.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

/* This compiles all of the formulae
It is called only once during the session
*/
int compile_derived()
{
  std::array<int, 256> f;
  int n;
  for (xpp::Model::DerivedQuantity &d : xpp::model().derived) {
    if (add_expr(d.rhs.c_str(), f.data(), &n) == 1) {
      xpp::log(XPP_LOG_ERROR, " Bad right-hand side for derived parameters \n");
      return 1;
    }
    d.form.assign(f.begin(), f.begin() + n);
  }
  evaluate_derived();
  return 0;
}

/* This evaluates all derived quantities in order of definition
called before any integration or numerical computation
and after changing parameters and constants
*/
void evaluate_derived()
{
  xpp::Session &s = xpp::session(); /* an entry point: the integrator, AUTO, the parser (W47d3-5) */
  std::array<double, MAXPAR> &constants = s.parser.constants;
  for (xpp::Model::DerivedQuantity &d : s.model().derived) constants[d.index] = evaluate(d.form.data());
}

/* this adds a derived quantity  */
int add_derived(const char *name, const char *rhs)
{
  xpp::Session &s = xpp::session(); /* an entry point: the parser (W47d3) */
  xpp::Model::DerivedQuantity d;
  d.rhs = rhs;
  /* this is the constant to which it addresses */
  d.index = s.parser.ncon;
  /* add the name to the recognized symbols */
  xpp::log(XPP_LOG_INFO, " derived constant[{}] is {} = {}\n", s.parser.ncon, name, rhs);
  s.model().derived.push_back(std::move(d));
  return add_con(name, 0.0);
}
