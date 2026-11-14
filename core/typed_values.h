#pragma once

#include "xpp_error.h"
#include "colormap.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>

/* the kinds of a value box (find_user_name, box_set_value): a parameter, an IC,
   a delay and a boundary condition */
#define PARAMBOX 1
#define ICBOX 2
#define DELAYBOX 3
#define BCBOX 4

namespace xpp {
struct Model;   /* model.h */
struct Session; /* session.h */

/* what a %formula (the text after the '%') says: its value, and the name it
   sets when it is "name:formula" (empty otherwise). The one rule of the
   %name:formula form: the name checked, the formula worked out once. */
struct WorkedOut {
  double value=0.0;
  std::string name;
};
Result<WorkedOut> work_out(Session &s, std::string_view formula, std::string_view field);
/* the formula expr worked out in s, or why it does not compile; shows
   nothing and changes nothing */
Result<double> evaluate_formula(Session &s, std::string_view expr);
/* the formula expr compiled in the Session s, or why it does not (the refusal of a command's expression argument); worked out by no one, nothing changed */
Result<> compile_formula(Session &s, std::string_view expr);
/* what was typed in a number field: a plain number (xpp::parse_number's
   rule, blanks around it allowed) or %formula (worked out in s; "%name:
   formula" must name a parameter or an IC), else the error, which
   carries `field`, the field it was typed in (Error::field). Nothing is
   shown or set: the one reading of a typed number (W131), which new_float,
   the `set` command and every box go through. */
Result<double> typed_number(Session &s, std::string_view typed, std::string_view field = {});
/* the index of parameter (type PARAMBOX) or variable (ICBOX) oname,
   blanks ignored and case not, -1 when there is none */
int find_user_name(const Model &m, int type, std::string_view oname);

} // namespace xpp
