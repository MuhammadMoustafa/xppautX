#ifndef XPP_UTIL_H
#define XPP_UTIL_H

#include <stdio.h>
#ifdef __cplusplus
#include "many_pops.h"
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
void redo_stuff(void);
/* a comment's action (the source's "# ... {action}"), run when it is picked */
void do_txt_action(const char *s);

/* atexit hook: removes program.auto_dir (xpp_globals.h) if it is set, and
   clears it. Registered by xppautx_main.c, not the X11 front end. */
void xpp_cleanup_auto_dir(void);

#ifdef __cplusplus
}

#include <string>
#include <string_view>

/* name, shortened for a fixed-width display of width characters: a longer
   one keeps its start and ends in '~' so it cannot pass for another name */
std::string short_name(std::string_view name, int width);

/* f() on the active plot window, or under Simulplot on each open one in
   turn (made active with make_active(i, flag)), the active one made
   active again after */
template <class F>
void for_each_shown_window(int flag, F f)
{
    if (plot_windows.simul == 0) {
        f();
        return;
    }
    const int ic = plot_windows.active;
    for (int i = 0; i < plot_windows.count; i++) {
        make_active(plot_windows.open[i], flag);
        f();
    }
    make_active(ic, flag);
}
#endif
#endif
