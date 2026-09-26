#include "derived.h"
#include "xpp_log.h"

#include <string>
#include <vector>

#include "ggets.h"
#include "parserslow.h"
#include "calc.h"
#include "xpp_io.h"

/* Derived parameter stuff !!  */
#define MAXDERIVED 200
extern double constants[];
extern int NCON;

namespace {
struct Derived {
  int index = 0;
  std::vector<int> form;
  std::string rhs;
  double value = 0.0;
};

Derived derived[MAXDERIVED];
}  // namespace

int nderived = 0;

/* This compiles all of the formulae
It is called only once during the session
*/
int compile_derived()
{
  int f[256], n;
  for (int i = 0; i < nderived; i++) {
    if (add_expr(derived[i].rhs.c_str(), f, &n) == 1) {
      xpp_log(XPP_LOG_ERROR, " Bad right-hand side for derived parameters \n");
      return 1;
    }
    derived[i].form.assign(f, f + n);
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
  for (int i = 0; i < nderived; i++) {
    derived[i].value = evaluate(derived[i].form.data());
    constants[derived[i].index] = derived[i].value;
  }
}

/* this adds a derived quantity  */
int add_derived(const char *name, const char *rhs)
{
  if (nderived >= MAXDERIVED) {
    xpp_log(XPP_LOG_ERROR, " Too many derived constants! \n");
    return 1;
  }
  int i0 = nderived;
  derived[i0].rhs = rhs;
  /* this is the constant to which it addresses */
  derived[i0].index = NCON;
  /* add the name to the recognized symbols */
  xpp_log(XPP_LOG_INFO, " derived constant[%d] is %s = %s\n", NCON, name, rhs);
  nderived++;
  return add_con(name, 0.0);
}
