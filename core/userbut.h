#ifndef _userbut_h_
#define _userbut_h_


#ifdef __cplusplus
extern "C" {
#endif

#define USERBUTMAX 20

typedef struct {
  char bname[10]; /* ui_json.cpp sends it as a C string */
  int com;
} USERBUT;

extern int nuserbut;
extern USERBUT userbut[USERBUTMAX];

void add_user_button(const char *s); /* parse "name:keys" from an @ button line */

#ifdef __cplusplus
}
#endif
#endif
