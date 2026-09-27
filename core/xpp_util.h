#ifndef XPP_UTIL_H
#define XPP_UTIL_H

#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

/* xpp_util.c: pure helpers relocated out of X11 files */
void restore_off(void);
void restore_on(void);
void ps_restore(void);
void svg_restore(void);
void set_active_windows(void);
void new_parameter(void);
void set_default_params(void);
void clone_ode(void);
void make_active(int i, int flag);
void clr_scrn(void);
int find_user_name(int type, const char *oname);
void short_name(char *out, const char *name, int width);
void de_space(char *s);
void ind_to_sym(int ind, char *str);
void get_max(int index, double *vmin, double *vmax);
int do_calc(const char *temp, double *z);
int has_eq(const char *z, char *w, int *where);
double calculate(const char *expr, int *ok);
void man_ic(void);
void set_default_ics(void);
int to_float(const char *s, double *z);
int box_set_value(int type, int i, const char *s, double *z);
void box_values_loaded(int type);
void plot_checked_vars(int how, int *isck, int n);
int find_par_or_var(const char *name, int *type, int *index);
void set_par_or_var(const char *name, int type, int index, double val);
void slider_rerun(void);
void eq_import(double *y, int n);
const char *eq_stability(int cp, int rp, int im);

/* atexit hook: removes program.auto_dir (xpp_globals.h) if it is set, and
   clears it. Registered by xppautx_main.c, not the X11 front end. */
void xpp_cleanup_auto_dir(void);

#ifdef __cplusplus
}
#endif
#endif
