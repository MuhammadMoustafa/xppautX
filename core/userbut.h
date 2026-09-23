#ifndef _userbut_h_
#define _userbut_h_


#include "xpp_types.h"
#ifdef __cplusplus
extern "C" {
#endif

#define USERBUTMAX 20

typedef struct {
  XppWinId w;
  char bname[10];
  int com;
} USERBUT;

extern int nuserbut;
extern USERBUT userbut[USERBUTMAX];

int get_button_info(char *s, char *bname, char *sc);
int find_kbs(char *sc);
void add_user_button(char *s); /* parse "name:keys" from an @ button line */

#ifdef __cplusplus
}
#endif
#endif
