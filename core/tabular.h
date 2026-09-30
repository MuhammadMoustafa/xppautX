
#ifndef _tabular_h_
#define _tabular_h_

#include "xpplim.h" /* MAX_TAB */
#ifdef __cplusplus
#include <string>
#include <vector>

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

extern "C" {
#endif


/* tabular.c */
double lookupxy(double x, int n, double *xv, double *yv);
double tab_interp(double xlo, double h, double x, double *y, int n, int i);
double lookup(double x, int index);
int get_lookup_len(int i);


#ifdef __cplusplus
}

#include "xpp_error.h"

/* Tables return why they failed; the command (or the model load) shows it. */
/* the function tables again, after a parameter changed: the first
   failure, the others still done */
xpp::Result<> redo_all_fun_tables(void);
namespace xpp {
struct Session; /* session.h */
}
xpp::Result<> eval_fun_table(xpp::Session &s, int n, double xlo, double xhi, const char *formula, double *y);
/* the session s's tables: @ autoeval= for each, View, Numerics' tables
   menu (i: 0 view, 1 edit) and which table it picks, -1 for none */
void set_auto_eval_flags(xpp::Session &s, int f);
void view_table(xpp::Session &s, int index);
void new_lookup_com(xpp::Session &s, int i);
int select_table(const xpp::Session &s);
/* the session s's table index is called name; every table back to none
   (a load's start) */
void set_table_name(xpp::Session &s, const char *name, int index);
void init_table(xpp::Session &s);
xpp::Result<> create_fun_table(xpp::Session &s, int npts, double xlo, double xhi, const char *formula, int index);
/* table index read from the file filename (quoted or not): one of the
   model's own files (model_file, model_files.h: a file table of the
   model) or a file the user picked (Numerics' table file) */
xpp::Result<> load_table(xpp::Session &s, const char *filename, int index, int model_file);
#endif
#endif

