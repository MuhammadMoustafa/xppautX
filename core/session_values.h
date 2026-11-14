#pragma once

/* The model's own values in a Session, by name: its parameters, initial conditions, sliders and
   equilibria, and the plot commands that set variables (core/session_values.cpp). Moved out of
   xpp_util.h at W726: xpp_util is the kernel's and includes nothing outside it. */
#include <cstdio>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include "xpp_error.h"

namespace xpp {
struct Session; /* session.h */
struct Model;   /* model.h */
struct XppSlider;

/* a slider names a parameter (*type PARAMBOX) or a variable (ICBOX) of
   m, *index its index; 0 if neither */
int find_par_or_var(const Model &m, std::string_view name, int *type, int *index);
/* why slider cannot be one of m's (the slider command and .snapx's
   sliders.set both ask); nullopt when it can, an empty name an empty slot */
std::optional<std::string> slider_wrong(const Model &m, const XppSlider &slider, bool check_names = true);
/* the least and greatest of s's data column index, widened when equal */
void get_max(const Session &s, int index, double *vmin, double *vmax);
/* every parameter (or only entry `only`) back to the model's value, the tables redone once */
void set_default_params(Session &s, std::optional<int> only = std::nullopt);
void set_default_ics(Session &s, std::optional<int> only = std::nullopt);
void set_par_or_var(Session &s, std::string_view name, int type, int index, double val);
void eq_import(Session &s, double *y, int n);
/* the plot of s redrawn into the picture file begun (image_format.h's
   restore), which it then closes */
Result<> vector_restore(Session &s);
/* What plot.set_variables plots: curves against time, a phase portrait, an array plot */
enum class PlotHow { time_series, phase_plane, array_plot };
/* the variables (numbered from 1) plotted in the active window as `how` says: curves against time (at least
   one), a phase portrait (two or three axes), or an array plot from the first of two variables to the later one */
Result<> plot_variables(Session &s, PlotHow how, std::span<const int> variables);
/* a comment's action (the source's "# ... {action}"), run on s when it is
   picked */
Result<> do_txt_action(Session &s, std::string_view action);
/* the model m's user functions, as lunch-new.cpp's file info writes them */
void user_fun_info(const Model &m, FILE *fp);
/* the name of plotted column ind: T (0), a model variable, or (W77) a
   data column data_add_col added; browse_column_name (browse.h) owns
   the naming, this is just its name for a plotted column */
std::string ind_to_sym(const Session &s, int ind);

} // namespace xpp
