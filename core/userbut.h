#pragma once

#include <string>
#include <string_view>

namespace xpp {
struct Session; /* session.h */

#define USERBUTMAX 20

/* a button of the model's (@ button=name:keys): its name and command */
struct USERBUT {
  std::string bname;
  int com;
};

/* whether an @ button line's keys are a command sequence of kbs.h's list (the import drops the lines that are not) */
bool is_button_sequence(std::string_view spec);

/* parse "name:keys" from an @ button line into a button of s; nullptr, or why the line is not a button
   (the keys are a command sequence of kbs.h's list); nothing is added when apply is false */
const char *add_user_button(Session &s, std::string_view spec, bool apply);

} // namespace xpp
