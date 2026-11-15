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

/* the least and greatest of s's data column index, widened when equal */
void get_max(const Session &s, int index, double *vmin, double *vmax);
/* every parameter (or only entry `only`) back to the model's value, the tables redone once */
void set_default_params(Session &s, std::optional<int> only = std::nullopt);
void set_default_ics(Session &s, std::optional<int> only = std::nullopt);
void set_par_or_var(Session &s, std::string_view name, int type, int index, double val);
void eq_import(Session &s, double *y, int n);
/* the name of plotted column ind: T (0), a model variable, or (W77) a
   data column data_add_col added; browse_column_name (browse.h) owns
   the naming, this is just its name for a plotted column */
std::string ind_to_sym(const Session &s, int ind);

/* temp ("name:formula" sets name, or a formula) worked out in the Session
   s into *z */
int do_calc(Session &s, std::string_view temp, double *z);
/* the array initial conditions (`u[5..20]=f([j])`) of the model worked out into the Session's initial data (the run's start) */
void evaluate_ar_ic(Session &s, const char *v, const char *f, int j1, int j2);
void arr_ic_start(Session &s);
/* the initial conditions by formula (initial_conditions.set_by_formula): `variable` a state, or an array range name[j1..j2] whose
   formula reads [j] as the element's number (an array's formula is kept like the model's own); the states set (indices),
   or why none was, nothing changed */
Result<std::vector<int>> set_ic_formula(Session &s, std::string_view variable, std::string_view formula);
/* `size` states from `name` on set to the formula, its t the element's offset from the first (initial_conditions.set_by_array_formula) */
Result<std::vector<int>> set_ic_array(Session &s, std::string_view name, int size, std::string_view formula);
/* store the text typed for entry i of the box `type`; the refusal, shown by
   no one, changes nothing (typed_number) */
Result<void> box_set_value(Session &s, int type, int i, std::string_view text, std::string_view field = {});
void box_values_loaded(Session &s, int type);

} // namespace xpp
