#ifndef _userbut_h_
#define _userbut_h_


#ifdef __cplusplus
extern "C" {
#endif

#define USERBUTMAX 20


#ifdef __cplusplus
}

#include <string>

/* a button of the model's (@ button=name:keys): its name and command */
struct USERBUT {
  std::string bname;
  int com;
};

namespace xpp {
struct Session; /* session.h */
}

/* parse "name:keys" from an @ button line into a button of s */
void add_user_button(xpp::Session &s, const char *spec);
#endif
#endif
