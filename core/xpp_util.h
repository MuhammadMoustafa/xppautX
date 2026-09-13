#ifndef XPP_UTIL_H
#define XPP_UTIL_H

#include <stdio.h>

/* xpp_util.c: pure helpers relocated out of X11 files */
void restore_off(void);
void restore_on(void);
void make_active(int i, int flag);
void clr_scrn(void);
int find_user_name(int type, char *oname);
void de_space(char *s);
void ind_to_sym(int ind, char *str);
void get_max(int index, double *vmin, double *vmax);
int do_calc(char *temp, double *z);
int has_eq(char *z, char *w, int *where);
double calculate(char *expr, int *ok);

/* browse_data.c */
void open_write_file(FILE **fp, char *fil, int *ok);

#endif
