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
int find_user_name(int type, char *oname);
void short_name(char *out, const char *name, int width);
void de_space(char *s);
void ind_to_sym(int ind, char *str);
void get_max(int index, double *vmin, double *vmax);
int do_calc(char *temp, double *z);
int has_eq(char *z, char *w, int *where);
double calculate(char *expr, int *ok);
void man_ic(void);
void set_default_ics(void);
int to_float(char *s, double *z);
int box_set_value(int type, int i, char *s, double *z);
void box_values_loaded(int type);
void plot_checked_vars(int how, int *isck, int n);
int find_par_or_var(char *name, int *type, int *index);
void set_par_or_var(char *name, int type, int index, double val);
void slider_rerun(void);
void eq_import(double *y, int n);
char *eq_stability(int cp, int rp, int im);

/* browse_data.c */
void open_write_file(FILE **fp, char *fil, int *ok);

/* A private scratch directory, mode 0700, under $TMPDIR/$TMP or /tmp
   (POSIX; xpp_util.c) or the system temp path (Windows; xpp_win32.cpp).
   Returns a malloc'd absolute path, or NULL on failure. */
char *xpp_make_temp_dir(void);
/* Removes every file directly under dir (no subdirectories are expected)
   and then dir itself. Safe to call with NULL. */
void xpp_remove_temp_dir(const char *dir);
/* atexit hook: removes program.auto_dir (xpp_globals.h) if it is set, and
   clears it. Registered by xppautx_main.c, not the X11 front end. */
void xpp_cleanup_auto_dir(void);

#ifdef __cplusplus
}
#endif
#endif
