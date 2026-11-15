#pragma once
/* autoX's settings the option table's rows set (the continuation group's ntst, ds, ..., the advanced numerics, the codimension-two
   kinds): plain data, which a Session's AutoxState (autox_state.h, autoX's) extends with the window's and the runs' state (W757). */
#include <array>
#include "autox_codes.h"

/* the autoX settings the model's @ options give (autox_ntst=, ...), which
   init_autox_win copies into AutoxState::bifur */
// A nonnegative norm has no useful lower default; the upper bound leaves ordinary branches unrestricted.
inline constexpr double autox_default_norm_min=0.0;
inline constexpr double autox_default_norm_max=1e100;
struct AutoxOptions {
  int ntst=15,nmx=200,npr=50,ncol=4;
  double ds=.02,dsmax=.5,dsmin=.001;
  double rl0=0.0,rl1=2,a0=autox_default_norm_min,a1=autox_default_norm_max;
  double xmax=2.5,xmin=-.5,ymax=3.0,ymin=-3.0;
  double epsl=1e-4,epsu=1e-4,epss=1e-4;
  int var=0;
};

namespace xpp {
struct ADVAUTOX {
  int iad = 3;
  /* autoX's MXBF is one signed number: the count, negative for one direction only (W531 keeps them apart) */
  int max_branch_switches = 5;
  bool switch_one_direction = false;
  int iid = 2; /* an index of autox_output_detail_names */
  int itmx = 8;
  int itnw = 7;
  int nwtn = 3;
  int iads = 1;
};
} // namespace xpp

/* every codimension-two kind detected, as AUTO-07p does by default */
constexpr std::array<bool, xpp::autox_codim_kinds.size()> all_special_points()
{
  std::array<bool, xpp::autox_codim_kinds.size()> on{};
  on.fill(true);
  return on;
}

/* what the option table sets of autoX, a base of AutoxState */
struct AutoxSettings {
  /* the model's @ options for autoX */
  AutoxOptions options;
  /* the advanced numerics */
  xpp::ADVAUTOX advanced{};
  /* Numerics' SuppBP: no branch point detection */
  bool suppress_bp=false;
  /* a run from the current state continues both directions, +DS and -DS, and every branch the jobs find (autox_queue.h)
     as jobs at once (autox_jobs.cpp); off: only the direction the sign of DS gives */
  bool follow_all_branches=true;
  // AUTO-07p enables codimension-two detection by default.
  std::array<bool, xpp::autox_codim_kinds.size()> special_points=all_special_points();
};
