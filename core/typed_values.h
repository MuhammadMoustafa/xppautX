#pragma once

#include "xpp_error.h"
#include "xpp_types.h"
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

/* temp ("name:formula" sets name, or a formula) worked out in the Session
   s into *z */
int do_calc(Session &s, std::string_view temp, double *z);
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
/* One reading of the fields typed in a form, all or nothing: the first
   fault is kept with the field it belongs to, no later field is read once
   there is one, and the caller applies nothing when error() is set. */
class TypedFields {
 public:
  explicit TypedFields(Session &s) : s_(s) {}
  /* typed_number's reading of `text`; 0 after a fault */
  double number(std::string_view field, const std::string &text);
  /* a whole number (parse_int), within min..max where given; 0 after a fault */
  int integer(std::string_view field, const std::string &text, std::optional<double> min = std::nullopt,
              std::optional<double> max = std::nullopt);
  /* a plain real number (parse_number: no %formula), within min..max where
     given; 0 after a fault */
  double real(std::string_view field, const std::string &text, std::optional<double> min = std::nullopt,
              std::optional<double> max = std::nullopt);
  /* `true` or `false`; false after a fault */
  bool boolean(std::string_view field, const std::string &text);
  /* a colour (colormap.h's parse_color); the foreground after a fault */
  Color color(std::string_view field, const std::string &text);
  /* one of choices, by its name, exactly: its index; 0 after a fault */
  int choice(std::string_view field, const std::string &text, std::span<const std::string> choices);
  void fail(std::string_view field, std::string_view what);
  const std::optional<std::string> &error() const { return error_; }

 private:
  Session &s_;
  std::optional<std::string> error_;
};
/* store the text typed for entry i of the box `type`; the refusal, shown by
   no one, changes nothing (typed_number) */
Result<void> box_set_value(Session &s, int type, int i, std::string_view text, std::string_view field = {});
void box_values_loaded(Session &s, int type);
/* the index of parameter (type PARAMBOX) or variable (ICBOX) oname,
   blanks ignored and case not, -1 when there is none */
int find_user_name(const Model &m, int type, std::string_view oname);

} // namespace xpp
