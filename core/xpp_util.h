#ifndef XPP_UTIL_H
#define XPP_UTIL_H

#include <stdio.h>
#include "xpp_types.h"
#include "many_pops.h"
#include "session.h"
#include "xpp_error.h"

#include <string>
#include <string_view>

/* the kinds of a value box find_user_name and box_set_value read */
#define PARAMBOX 1
#define ICBOX 2

namespace xpp {

/* s without its white space, in place */
void de_space(char *s);

/* an equilibrium's stability from its eigenvalues' counts: UNSTABLE,
   NEUTRAL or STABLE */
const char *eq_stability(int cp, int rp, int im);


/* at exit (xppautx_main.cpp's atexit hook): removes s's AUTO scratch
   folder (auto_state.h) if it is set, and clears it */
void cleanup_auto_dir(Session &s);
void man_ic(Session &s);
void redo_stuff(Session &s);
/* the value of the formula expr in the Session s (*ok 0, said why, when it
   does not compile) */
double calculate(Session &s, std::string_view expr, int *ok);
/* temp ("name:formula" sets name, or a formula) worked out in the Session
   s into *z */
int do_calc(Session &s, std::string_view temp, double *z);
/* the formula expr worked out in s, or why it does not compile; shows
   nothing and changes nothing (calculate shows the error) */
Result<double> evaluate_formula(Session &s, std::string_view expr);
/* what was typed in a number field: a plain number (xpp::parse_number's
   rule, blanks around it allowed) or %formula (worked out in s; "%name:
   formula" must name a parameter or an IC), else the error, which
   carries `field`, the field it was typed in (Error::field). Nothing is
   shown or set: the one reading of a typed number (W131), which new_float,
   the `set` command and every box go through. */
Result<double> typed_number(Session &s, std::string_view typed, std::string_view field = {});
/* a slider names a parameter (*type PARAMBOX) or a variable (ICBOX) of
   m, *index its index; 0 if neither */
int find_par_or_var(const Model &m, std::string_view name, int *type, int *index);
/* One validation path for protocol and saved slider settings; empty removes a slot. */
std::optional<std::string> slider_wrong(const Model &m, const XppSlider &slider);
/* The session s's plot windows: which are open (set_active_windows), the
   active one (make_active), the plot window (its graph's index) whose
   window is w (graph_of; 0, the main one, when none in use is), and the
   active window's bounds made valid (check_windows) */
void set_active_windows(Session &s);
void make_active(Session &s, int i, int flag);
int graph_of(const Session &s, XppWinId w);
void check_windows(Session &s);
/* the least and greatest of s's browser column index, widened when equal */
void get_max(const Session &s, int index, double *vmin, double *vmax);
/* the values behind the parameter and IC boxes and the sliders of s */
void new_parameter(Session &s);
void set_default_params(Session &s);
void set_default_ics(Session &s);
/* store the text typed for entry i of the box `type`; the refusal, shown by
   no one, changes nothing (typed_number) */
Result<void> box_set_value(Session &s, int type, int i, std::string_view text, std::string_view field = {});
void box_values_loaded(Session &s, int type);
void set_par_or_var(Session &s, std::string_view name, int type, int index, double val);
void eq_import(Session &s, double *y, int n);
/* the plot of s redrawn into the picture file begun (image_format.h's
   restore), which it then closes */
void ps_restore(Session &s);
void svg_restore(Session &s);
/* s's active plot window blanked, its axes drawn again */
void clr_scrn(Session &s);
/* the ICs box "xvst" (how 0) and "pp" (how 1) buttons: plot s's checked
   variables (isck, n entries) and uncheck them */
void plot_checked_vars(Session &s, int how, int *isck, int n);
/* a comment's action (the source's "# ... {action}"), run on s when it is
   picked */
void do_txt_action(Session &s, std::string_view action);
/* File > Clone: s's model file with its values now */
void clone_ode(Session &s);
/* the model m's user functions, as lunch-new.cpp's file info writes them */
void user_fun_info(const Model &m, FILE *fp);
/* another model is loaded (File > Open model, Reload): a new, empty AUTO
   scratch folder in place of the Session s's, whose files (<model>.s, .b,
   .d) were the model before's */
void renew_auto_dir(Session &s);

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
std::string ind_to_sym(const Session &s, int ind);

/* the index of parameter (type PARAMBOX) or variable (ICBOX) oname,
   blanks ignored and case not, -1 when there is none */
int find_user_name(const Model &m, int type, std::string_view oname);

/* f() on the active plot window of s, or under Simulplot on each open one
   in turn (made active with make_active(s, i, flag)), the active one made
   active again after */
template <class F>
void for_each_shown_window(Session &s, int flag, F f)
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
} // namespace xpp
#endif
