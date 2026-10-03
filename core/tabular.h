
#ifndef _tabular_h_
#define _tabular_h_

#include "xpplim.h" /* MAX_TAB */
#include "xpp_error.h"

#include <string>
#include <string_view>
#include <vector>

namespace xpp {

struct Session; /* session.h */

/* Bound both allocation and formula evaluation from hostile table counts to 8 MB per session. */
inline constexpr int table_points_limit = 1000000;

/* a model's table (xpp::Model's tables, model.h): file or function values y on [xlo,xhi] step dx (x too
   when xyvals); y stays a raw double* because simplenet.cpp's networks
   keep pointers into it (weights, indices, delays); y_storage owns that
   block (tabular.cpp's resize_values), a std::vector instead of a
   hand-paired xpp_realloc/xpp_free */
struct TABULAR {
  double xlo=0.0,xhi=0.0,dx=0.0;
  double *y=nullptr,*x=nullptr;
  std::vector<double> y_storage;
  int n=0,flag=0,interp=0,autoeval=0;
  int xyvals=0;
/* flag=0 if virgin array, flag=1 if already allocated; flag=2 for function
   interp=0 for normal interpolation, interp=1 for 'step'
   interp=2 for cubic spline
   and finally, xyvals=1 if both x and y vals are needed (xyvals=0
   is faster lookup) */
  std::string filename; /* the file, or a function table's formula */
  std::string name;
};

/* the value at x of the table of n points (xv, yv), linearly
   interpolated; the cubic through y[i-1..i+2] (from xlo, step h) at x */
double lookupxy(double x, int n, double *xv, double *yv);
double tab_interp(double xlo, double h, double x, double *y, int n, int i);

/* Tables return why they failed; the command (or the model load) shows it. */

int get_lookup_len(Session &s, int i);
/* the Session s's function tables again, after a parameter changed: the
   first failure, the others still done */
Result<> redo_all_fun_tables(Session &s);
/* table index's value at x in the Session s (the evaluator's TABTYPE) */
double lookup(const Session &s, double x, int index);
/* table index's formula worked out at n points of [xlo,xhi] into y (its
   error at the table's line); formula is a
   std::string (a table's own) and not a string_view: AUTO redoes the
   tables at every right-hand side (redo_all_fun_tables), and a
   string_view there cost two instructions a call (W109c) */
Result<> eval_fun_table(Session &s, int index, int n, double xlo, double xhi, const std::string &formula, double *y);
/* the session s's tables: @ autoeval= for each, View, Numerics' tables
   menu (i: 0 view, 1 edit) and which table it picks, -1 for none */
void set_auto_eval_flags(Session &s, int f);
void view_table(Session &s, int index);
void new_lookup_com(Session &s, int i);
int select_table(const Session &s);
/* the session s's table index is called name; every table back to none
   (a load's start) */
void set_table_name(Session &s, std::string_view name, int index);
void init_table(Session &s);
Result<> create_fun_table(Session &s, int npts, double xlo, double xhi, std::string_view formula, int index);
/* table index read from the file filename (quoted or not): one of the
   model's own files (model_file, model_files.h: a file table of the
   model) or a file the user picked (Numerics' table file) */
Result<> load_table(Session &s, std::string_view filename, int index, int model_file);

} // namespace xpp
#endif
