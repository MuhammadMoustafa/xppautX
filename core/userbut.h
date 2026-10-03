#ifndef _userbut_h_
#define _userbut_h_

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

/* parse "name:keys" from an @ button line into a button of s */
void add_user_button(Session &s, std::string_view spec);
/* the keys that run command com (menudrive.h's M_*), as a button's
   spec names them ("ig" for M_IG); empty when no keys do */
std::string_view command_keys(int com);

} // namespace xpp
#endif
