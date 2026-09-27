
#ifndef _tabular_h_
#define _tabular_h_

#include "xpplim.h" /* MAX_TAB, XPP_NAME_MAX */
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
void set_auto_eval_flags(int f);
void set_table_name(const char *name, int index);
void view_table(int index);
void new_lookup_com(int i);
double lookupxy(double x, int n, double *xv, double *yv);
double tab_interp(double xlo, double h, double x, double *y, int n, int i);
double lookup(double x, int index);
void init_table(void);
void redo_all_fun_tables(void);
int eval_fun_table(int n, double xlo, double xhi, const char *formula, double *y);
int create_fun_table(int npts, double xlo, double xhi, const char *formula, int index);
int load_table(const char *filename, int index);
int get_lookup_len(int i);


#ifdef __cplusplus
}
#endif
#endif

