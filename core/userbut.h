#ifndef _userbut_h_
#define _userbut_h_


#ifdef __cplusplus
extern "C" {
#endif

#define USERBUTMAX 20

void add_user_button(const char *s); /* parse "name:keys" from an @ button line */

#ifdef __cplusplus
}

#include <string>

/* a button of the model's (@ button=name:keys): its name and command */
struct USERBUT {
  std::string bname;
  int com;
};
#endif
#endif
