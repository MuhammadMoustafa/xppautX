#ifndef XPP_UTIL_H
#define XPP_UTIL_H

#include <stdio.h>
#ifdef __cplusplus
#include "many_pops.h"
#include "session.h"
extern "C" {
#endif

/* xpp_util.c: pure helpers relocated out of X11 files */
void ps_restore(void);
void svg_restore(void);
void set_active_windows(void);
void new_parameter(void);
void set_default_params(void);
void clone_ode(void);
void make_active(int i, int flag);
void clr_scrn(void);
void de_space(char *s);
void get_max(int index, double *vmin, double *vmax);
int do_calc(const char *temp, double *z);
double calculate(const char *expr, int *ok);
void man_ic(void);
void set_default_ics(void);
int to_float(const char *s, double *z);
int box_set_value(int type, int i, const char *s, double *z);
void box_values_loaded(int type);
void plot_checked_vars(int how, int *isck, int n);
int find_par_or_var(const char *name, int *type, int *index);
void set_par_or_var(const char *name, int type, int index, double val);
void eq_import(double *y, int n);
/* the model's user functions, as lunch-new.cpp's file info writes them */
void user_fun_info(FILE *fp);
const char *eq_stability(int cp, int rp, int im);
void redo_stuff(void);
/* a comment's action (the source's "# ... {action}"), run when it is picked */
void do_txt_action(const char *s);

/* atexit hook: removes the Session's AUTO scratch folder (auto_state.h) if it is set, and
   clears it. Registered by xppautx_main.c, not the X11 front end. */
void xpp_cleanup_auto_dir(void);
/* another model is loaded (File > Open model, Reload): a new, empty AUTO
   scratch folder in place of the Session's, whose files (<model>.s, .b,
   .d) were the model before's */
void xpp_renew_auto_dir(void);

#ifdef __cplusplus
}

#include <string>
#include <string_view>

/* name, shortened for a fixed-width display of width characters: a longer
   one keeps its start and ends in '~' so it cannot pass for another name */
std::string short_name(std::string_view name, int width);

/* s without its white space (de_space's, up to a NUL in it) */
void de_space(std::string &s);

/* "name:formula" (do_calc's "set this name to that"): 1 with name the
   text before the first ':' (of any length) and *where the formula's
   start; 0 when z has no ':' */
int has_eq(std::string_view z, std::string &name, int *where);

/* the name of plotted column ind: T (0), a model variable, or (W77) a
   browser column data_add_col added; browse_column_name (browse.h) owns
   the naming, this is just its name for a plotted column */
std::string ind_to_sym(int ind);

/* the index of parameter (type PARAMBOX) or variable (ICBOX) oname,
   blanks ignored and case not, -1 when there is none */
#define PARAMBOX 1
#define ICBOX 2
int find_user_name(int type, std::string_view oname);

/* f() on the active plot window, or under Simulplot on each open one in
   turn (made active with make_active(i, flag)), the active one made
   active again after */
template <class F>
void for_each_shown_window(int flag, F f)
{
    if (xpp::session().plot_windows.simul == 0) {
        f();
        return;
    }
    const int ic = xpp::session().plot_windows.active;
    for (int i = 0; i < xpp::session().plot_windows.count; i++) {
        make_active(xpp::session().plot_windows.open[i], flag);
        f();
    }
    make_active(ic, flag);
}
#endif
#endif
