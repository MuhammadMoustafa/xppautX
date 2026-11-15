#pragma once
/* The lookup tables a formula reads (a model's tab statements and function tables): what the evaluator and the
   native code index. The tables are a Session's (session.h, its `tables`); creating and editing them is tabular.h's. */
#include <string>
#include <string_view>
#include <vector>

namespace xpp {

/* a model's table (xpp::Model's tables, model.h): file or function values y on [xlo,xhi]
   step dx; y stays a raw double* because simplenet.cpp's networks
   keep pointers into it (weights, indices, delays); y_storage owns that
   block (tabular.cpp's resize_values), a std::vector */
struct TABULAR {
  double xlo=0.0,xhi=0.0,dx=0.0;
  double *y=nullptr;
  std::vector<double> y_storage;
  int n=0,flag=0,interp=0,autoeval=1;
/* flag=0 if virgin array, flag=1 if already allocated; flag=2 for function
   interp=0 for normal interpolation, interp=1 for 'step'
   interp=2 for cubic spline */
  std::string filename; /* the file, or a function table's formula */
  std::string name;
};

/* the table's value at x (the evaluator's TABTYPE) */
double lookup(const TABULAR &table, double x);
/* the slope of that table's interpolation at x: the derivative of lookup
   (W260), 0 on a step table's steps */
double lookup_slope(const TABULAR &table, double x);

} // namespace xpp
