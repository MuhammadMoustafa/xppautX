#ifndef XPP_UTIL_H
#define XPP_UTIL_H

#include <stdio.h>
#include "xpp_types.h"
#ifdef __cplusplus
#include "many_pops.h"
#include "session.h"
extern "C" {
#endif

/* xpp_util.c: pure helpers relocated out of X11 files */

void de_space(char *s);
int do_calc(const char *temp, double *z);
double calculate(const char *expr, int *ok);
void man_ic(void);
int to_float(const char *s, double *z);

int find_par_or_var(const char *name, int *type, int *index);
const char *eq_stability(int cp, int rp, int im);
void redo_stuff(void);


/* atexit hook: removes the Session's AUTO scratch folder (auto_state.h) if it is set, and
   clears it. Registered by xppautx_main.c, not the X11 front end. */
void xpp_cleanup_auto_dir(void);

#ifdef __cplusplus
}

/* the value of the formula expr in the Session s (*ok 0, said why, when it
   does not compile); calculate above is this in the current Session, an
   entry point (W47d4-6) */
double calculate(xpp::Session &s, const char *expr, int *ok);
/* The session s's plot windows: which are open (set_active_windows), the
   active one (make_active), the plot window (its graph's index) whose
   window is w (graph_of; 0, the main one, when none in use is), and the
   active window's bounds made valid (check_windows) */
void set_active_windows(xpp::Session &s);
void make_active(xpp::Session &s, int i, int flag);
int graph_of(const xpp::Session &s, XppWinId w);
void check_windows(xpp::Session &s);
/* the least and greatest of s's browser column index, widened when equal */
void get_max(const xpp::Session &s, int index, double *vmin, double *vmax);
/* the values behind the parameter and IC boxes and the sliders of s */
void new_parameter(xpp::Session &s);
void set_default_params(xpp::Session &s);
void set_default_ics(xpp::Session &s);
int box_set_value(xpp::Session &s, int type, int i, const char *text, double *z);
void box_values_loaded(xpp::Session &s, int type);
void set_par_or_var(xpp::Session &s, const char *name, int type, int index, double val);
void eq_import(xpp::Session &s, double *y, int n);
/* the plot of s redrawn into the picture file begun (image_format.h's
   restore), which it then closes */
void ps_restore(xpp::Session &s);
void svg_restore(xpp::Session &s);
/* s's active plot window blanked, its axes drawn again */
void clr_scrn(xpp::Session &s);
/* the ICs box "xvst" (how 0) and "pp" (how 1) buttons: plot s's checked
   variables (isck, n entries) and uncheck them */
void plot_checked_vars(xpp::Session &s, int how, int *isck, int n);
/* a comment's action (the source's "# ... {action}"), run on s when it is
   picked */
void do_txt_action(xpp::Session &s, const char *action);
/* File > Clone: s's model file with its values now */
void clone_ode(xpp::Session &s);
/* the model m's user functions, as lunch-new.cpp's file info writes them */
void user_fun_info(const xpp::Model &m, FILE *fp);
/* another model is loaded (File > Open model, Reload): a new, empty AUTO
   scratch folder in place of the Session s's, whose files (<model>.s, .b,
   .d) were the model before's */
void xpp_renew_auto_dir(xpp::Session &s);

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
int find_user_name(const xpp::Model &m, int type, std::string_view oname);
/* the same in the current Model: an entry point (W47d4-6) */
int find_user_name(int type, std::string_view oname);

/* f() on the active plot window of s, or under Simulplot on each open one
   in turn (made active with make_active(s, i, flag)), the active one made
   active again after */
template <class F>
void for_each_shown_window(xpp::Session &s, int flag, F f)
{
    if (s.plot_windows.simul == 0) {
        f();
        return;
    }
    const int ic = s.plot_windows.active;
    for (int i = 0; i < s.plot_windows.count; i++) {
        make_active(s, s.plot_windows.open[i], flag);
        f();
    }
    make_active(s, ic, flag);
}
#endif
#endif
