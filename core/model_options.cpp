#include "xpp_files.h"
/* The model's options as one table (model_options.h, W119). */
#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <string>
#include <string_view>

#include "model_options.h"
#include "browse.h"
#include "colormap.h"
#include "command_table.h"
#include "graphics.h"
#include "session.h"
#include "solver.h"
#include "tabular.h"
#include "userbut.h"
#include "xpp_batch.h"
#include "xpp_error.h"
#include "xpp_ui.h"
#include "xpp_globals.h"
#include "xpp_io.h"
#include "xpp_log.h"
#include "xpp_math.h"

namespace xpp {

namespace {

/* a 3D box side's default: wide enough for most models' variables */
constexpr double box_side = 12;

/* the value as a number (into x): why it is not one, nullptr when it is */
const char *number_in(const OptionValue &v, double &x)
{
  return parse_number(v.text, x) ? nullptr : "not a number";
}

/* the value as a whole number in int's range (into i) */
const char *whole_in(const OptionValue &v, int &i)
{
  double x = 0;
  if (!parse_number(v.text, x) || x != std::floor(x) || x < INT_MIN || x > INT_MAX) return "not a whole number";
  i = static_cast<int>(x);
  return nullptr;
}

/* the value as a number (whole number) into a member of the Session,
   when it is one and the value is applied (v.apply) */
const char *number_into(double &member, const OptionValue &v)
{
  double x = 0;
  if (const char *why = number_in(v, x)) return why;
  if (v.apply) member = x;
  return nullptr;
}
const char *whole_into(int &member, const OptionValue &v)
{
  int i = 0;
  if (const char *why = whole_in(v, i)) return why;
  if (v.apply) member = i;
  return nullptr;
}

/* the variable v names (0 the time), -1 for none */
int variable_of(const Session &s, const OptionValue &v)
{
  int i = -1;
  find_variable(s, v.text, &i);
  return i;
}

const char *color_into(int &member, const OptionValue &v)
{
  int i = 0;
  static_assert(LAST_PLOT_COLOR == 10, "the message below names the last colour");
  if (whole_in(v, i) || i < 0 || i > LAST_PLOT_COLOR) return "not a colour from 0 to 10";
  if (v.apply) member = i;
  return nullptr;
}

const char *variable_into(const Session &s, int &member, const OptionValue &v)
{
  const int i = variable_of(s, v);
  if (i < 0) return "no such variable";
  if (v.apply) member = i;
  return nullptr;
}

/* yes (y..., Y...: 1) or no (n..., N...: 0) into member */
const char *yes_no_into(int &member, const OptionValue &v)
{
  switch (v.text[0]) {
  case 'y': case 'Y': if (v.apply) member = 1; return nullptr;
  case 'n': case 'N': if (v.apply) member = 0; return nullptr;
  default: return "not yes or no";
  }
}

constexpr OptionRow rows[] = {
  /* ---- the numerics the Numerics menu and `set num` edit, in the menu's
     order; total's sign there says "forever" ---- */
  {.name = "TOTAL_TIME", .xppaut_names = {"TOTAL"}, .flag = Option::TOTAL_TIME,
   .real = [](Session &s) -> double & { return s.numerics.total_time; },
   /* twenty time units, XPPAUT's */
   .reset = [](Session &s) { s.numerics.total_time = 20; },
   .key = "total_time", .label = "Total time"},
  {.name = "START_TIME", .xppaut_names = {"T0"}, .flag = Option::START_TIME,
   .real = [](Session &s) -> double & { return s.numerics.start_time; },
   /* time starts at 0 */
   .reset = [](Session &s) { s.numerics.start_time = 0; },
   .key = "start_time", .label = "Start time"},
  {.name = "TRANSIENT_TIME", .xppaut_names = {"TRANS"}, .flag = Option::TRANSIENT_TIME,
   .real = [](Session &s) -> double & { return s.numerics.transient_time; },
   /* no transient: store from the start */
   .reset = [](Session &s) { s.numerics.transient_time = 0; },
   .key = "transient_time", .label = "Transient time"},
  {.name = "DT", .flag = Option::DT,
   .real = [](Session &s) -> double & { return s.numerics.delta_t; },
   /* twenty steps per time unit, XPPAUT's */
   .reset = [](Session &s) { s.numerics.delta_t = .05; },
   .key = "dt", .label = "Dt", .rule = OptionRule::nonzero},
  {.name = "NULLCLINE_MESH", .xppaut_names = {"NMESH"}, .flag = Option::NULLCLINE_MESH,
   .whole = [](Session &s) -> int & { return s.numerics.nullcline_mesh; },
   /* a 40 by 40 grid: smooth nullclines, quickly */
   .reset = [](Session &s) { s.numerics.nullcline_mesh = 40; },
   .key = "nullcline_mesh", .label = "Nullcline mesh", .rule = OptionRule::whole_positive},
  {.name = "SINGPT_MAX_ITERATES", .xppaut_names = {"NEWT_ITER"}, .flag = Option::SINGPT_MAX_ITERATES,
   .whole = [](Session &s) -> int & { return s.numerics.singpt_max_iterates; },
   /* Newton's method converges in far fewer or not at all */
   .reset = [](Session &s) { s.numerics.singpt_max_iterates = 100; },
   .key = "singpt_max_iterates", .label = "Sing pt: maximum iterates", .rule = OptionRule::whole_positive},
  {.name = "SINGPT_NEWTON_TOLERANCE", .xppaut_names = {"NEWT_TOL"}, .flag = Option::SINGPT_NEWTON_TOLERANCE,
   .real = [](Session &s) -> double & { return s.numerics.singpt_newton_tolerance; },
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.singpt_newton_tolerance = .001; },
   .key = "singpt_newton_tolerance", .label = "Sing pt: Newton tolerance", .rule = OptionRule::positive},
  {.name = "SINGPT_JACOBIAN_EPSILON", .xppaut_names = {"JAC_EPS"}, .flag = Option::SINGPT_JACOBIAN_EPSILON,
   .real = [](Session &s) -> double & { return s.numerics.singpt_jacobian_epsilon; },
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.singpt_jacobian_epsilon = .001; },
   .key = "singpt_jacobian_epsilon", .label = "Sing pt: Jacobian epsilon", .rule = OptionRule::positive},
  {.name = "STORE_EVERY", .xppaut_names = {"NOUT", "NJMP"}, .flag = Option::STORE_EVERY,
   .whole = [](Session &s) -> int & { return s.numerics.store_every; },
   /* every step stored */
   .reset = [](Session &s) { s.numerics.store_every = 1; },
   .key = "store_every", .label = STORE_EVERY_LABEL, .rule = OptionRule::whole_positive},
  {.name = "BOUND", .flag = Option::BOUND,
   .real = [](Session &s) -> double & { return s.numerics.bound; },
   /* XPPAUT's: past it a solution is taken to blow up */
   .reset = [](Session &s) { s.numerics.bound = 100; },
   .key = "bound", .label = "Bounds", .rule = OptionRule::positive},
  {.name = "METHOD", .xppaut_names = {"METH"}, .flag = Option::METHOD,
   .whole = [](Session &s) -> int & { return s.numerics.method; },
   /* Integral equations require Volterra; otherwise the usual RK4 default. */
   .reset = [](Session &s) {
     s.numerics.method = pick_method(s.model(), s.model().nkernel > 0 ? "Volterra" : "Runge-Kutta", command_place()).value();
   },
   .key = "method", .label = "Method", .rule = OptionRule::method},
  {.name = "TOLERANCE", .xppaut_names = {"TOL"}, .flag = Option::TOLERANCE,
   .real = [](Session &s) -> double & { return s.numerics.tolerance; },
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.tolerance = .001; },
   .key = "tolerance", .label = "Tolerance", .rule = OptionRule::positive, .use = OptionUse::step_or_rel},
  {.name = "MIN_STEP", .xppaut_names = {"DTMIN"}, .flag = Option::MIN_STEP,
   .real = [](Session &s) -> double & { return s.numerics.min_step; },
   /* as small as an adaptive method may need */
   .reset = [](Session &s) { s.numerics.min_step = 1e-12; },
   .key = "min_step", .label = "Minimum step", .rule = OptionRule::positive, .use = OptionUse::step},
  {.name = "MAX_STEP", .xppaut_names = {"DTMAX"}, .flag = Option::MAX_STEP,
   .real = [](Session &s) -> double & { return s.numerics.max_step; },
   /* one time unit */
   .reset = [](Session &s) { s.numerics.max_step = 1; },
   .key = "max_step", .label = "Maximum step", .rule = OptionRule::positive, .use = OptionUse::step},
  {.name = "ABS_TOLERANCE", .xppaut_names = {"ATOL"}, .flag = Option::ABS_TOLERANCE,
   .real = [](Session &s) -> double & { return s.numerics.abs_tolerance; },
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.abs_tolerance = .001; },
   .key = "abs_tolerance", .label = "Absolute tolerance", .rule = OptionRule::positive, .use = OptionUse::rel},
  {.real = [](Session &s) -> double & { return s.numerics.eul_tol; },
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.eul_tol = 1e-7; },
   .key = "eul_tol", .label = "Newton tolerance", .rule = OptionRule::positive, .use = OptionUse::newton},
  {.whole = [](Session &s) -> int & { return s.numerics.max_eul_iter; },
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.max_eul_iter = 10; },
   .key = "eul_iter", .label = "Newton iterations", .rule = OptionRule::whole_positive, .use = OptionUse::newton},
  {.name = "DELAY", .flag = Option::DELAY,
   .real = [](Session &s) -> double & { return s.numerics.delay; },
   /* no delays kept */
   .reset = [](Session &s) { s.numerics.delay = 0; },
   .key = "delay", .label = "Maximal delay", .rule = OptionRule::nonnegative, .use = OptionUse::delays},
  {.whole = [](Session &s) -> int & { return s.numerics.bvp_max_iterates; },
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.bvp_max_iterates = 20; },
   .key = "bvp_max_iterates", .label = "BVP maximum iterates", .rule = OptionRule::whole_positive},
  {.real = [](Session &s) -> double & { return s.numerics.bvp_tolerance; },
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.bvp_tolerance = 1e-5; },
   .key = "bvp_tolerance", .label = "BVP tolerance", .rule = OptionRule::positive},
  {.real = [](Session &s) -> double & { return s.numerics.bvp_epsilon; },
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.bvp_epsilon = 1e-5; },
   .key = "bvp_epsilon", .label = "BVP epsilon", .rule = OptionRule::positive},

  /* ---- the other numerics ---- */
  {.name = "VMAXPTS", .flag = Option::VMAXPTS,
   .whole = [](Session &s) -> int & { return s.numerics.max_points; }},
  {.name = "STORAGE_ROWS", .xppaut_names = {"MAXSTOR"}, .flag = Option::STORAGE_ROWS,
   .whole = [](Session &s) -> int & { return s.data_store.max_rows; },
   /* rows the data store starts with: a run of 5000 steps (it grows) */
   .reset = [](Session &s) { s.data_store.max_rows = 5000; }},
  {.name = "TORUS_PERIOD", .xppaut_names = {"TOR_PER"}, .flag = Option::TORUS_PERIOD,
   .real = [](Session &s) -> double & { return s.numerics.torus_period; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (const char *why = number_into(s.numerics.torus_period, v)) return why;
     if (v.apply) s.numerics.torus = 1;
     return nullptr;
   }},
  {.name = "FOLD",
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     const int i = variable_of(s, v);
     if (i < 1) return "no such variable";
     if (!v.apply) return nullptr;
     s.itor[i - 1] = 1;
     s.numerics.torus = 1;
     return nullptr;
   }},
  {.name = "BANDUP", .flag = Option::BANDUP,
   .whole = [](Session &s) -> int & { return s.numerics.cv_bandupper; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (const char *why = whole_into(s.numerics.cv_bandupper, v)) return why;
     if (v.apply) s.numerics.cv_bandflag = 1;
     return nullptr;
   }},
  {.name = "BANDLO", .flag = Option::BANDLO,
   .whole = [](Session &s) -> int & { return s.numerics.cv_bandlower; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (const char *why = whole_into(s.numerics.cv_bandlower, v)) return why;
     if (v.apply) s.numerics.cv_bandflag = 1;
     return nullptr;
   }},
  {.name = "POINCARE_MAP", .xppaut_names = {"POIMAP"}, .flag = Option::POINCARE_MAP,
   .whole = [](Session &s) -> int & { return s.numerics.poincare_map; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     int map = 0;
     switch (v.text[0]) {
     case 's': case 'S': map = 1; break;
     case 'm': case 'M': map = 2; break;
     case 'p': case 'P': map = 3; break;
     default: return "not section, max or period";
     }
     if (v.apply) s.numerics.poincare_map = map;
     return nullptr;
   },
   /* no Poincare map */
   .reset = [](Session &s) { s.numerics.poincare_map = 0; }},
  {.name = "POINCARE_VARIABLE", .xppaut_names = {"POIVAR"}, .flag = Option::POINCARE_VARIABLE,
   .whole = [](Session &s) -> int & { return s.numerics.poincare_variable; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.numerics.poincare_variable, v);
   },
   /* the first variable */
   .reset = [](Session &s) { s.numerics.poincare_variable = 1; }},
  {.name = "POINCARE_SIGN", .xppaut_names = {"POISGN"}, .flag = Option::POINCARE_SIGN,
   .whole = [](Session &s) -> int & { return s.numerics.poincare_sign; },
   /* crossings upwards */
   .reset = [](Session &s) { s.numerics.poincare_sign = 1; }},
  {.name = "POINCARE_STOP", .xppaut_names = {"POISTOP"}, .flag = Option::POINCARE_STOP,
   .whole = [](Session &s) -> int & { return s.numerics.poincare_stop; },
   /* a section does not stop the run */
   .reset = [](Session &s) { s.numerics.poincare_stop = 0; }},
  {.name = "POINCARE_PLANE", .xppaut_names = {"POIPLN"}, .flag = Option::POINCARE_PLANE,
   .real = [](Session &s) -> double & { return s.numerics.poincare_plane; },
   /* the section at 0 */
   .reset = [](Session &s) { s.numerics.poincare_plane = 0; }},
  {.name = "SEED", .flag = Option::SEED,
   .whole = [](Session &s) -> int & { return s.numerics.rand_seed; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     int i = 0;
     if (whole_in(v, i) || i < 0) return "not a seed (a whole number of at least 0)";
     if (!v.apply) return nullptr;
     s.numerics.rand_seed = i;
     s.random.seed(s.numerics.rand_seed);
     return nullptr;
   }},
  {.name = "STOCH", .flag = Option::STOCH,
   .whole = [](Session &s) -> int & { return s.stochastic.flag; }},
  {.name = "AUTOEVAL", .flag = Option::AUTOEVAL,
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     int f = 0;
     if (const char *why = whole_in(v, f)) return why;
     if (v.apply) set_auto_eval_flags(s, f);
     return nullptr;
   }},

  /* ---- the plot: the defaults of xp..zp before nplot's, which copies
     them, and of xlo..yhi before xmin..zmax's ---- */
  {.name = "XP", .flag = Option::XP,
   .whole = [](Session &s) -> int & { return s.plot_settings.ixplt; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.plot_settings.ixplt, v);
   },
   /* the time */
   .reset = [](Session &s) { s.plot_settings.ixplt = 0; }},
  {.name = "YP", .flag = Option::YP,
   .whole = [](Session &s) -> int & { return s.plot_settings.iyplt; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.plot_settings.iyplt, v);
   },
   /* the first variable */
   .reset = [](Session &s) { s.plot_settings.iyplt = 1; }},
  {.name = "ZP", .flag = Option::ZP,
   .whole = [](Session &s) -> int & { return s.plot_settings.izplt; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.plot_settings.izplt, v);
   },
   /* the first variable */
   .reset = [](Session &s) { s.plot_settings.izplt = 1; }},
  {.name = "NPLOT", .flag = Option::NPLOT,
   .whole = [](Session &s) -> int & { return s.plot_settings.npltv; },
   /* one curve, every extra one (xp2=, ...) starting as the main one in
     the default window */
   .reset = [](Session &s) {
     PlotSettings &p = s.plot_settings;
     p.npltv = 1;
     for (int i = 0; i < 10; i++) {
       p.ix_plt[i] = p.ixplt;
       p.iy_plt[i] = p.iyplt;
       p.iz_plt[i] = p.izplt;
       p.x_lo[i] = 0;
       p.y_lo[i] = -1;
       p.x_hi[i] = 20;
       p.y_hi[i] = 1;
     }
   }},
  {.name = "XP", .first_digit = '2', .last_digit = '8',
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.plot_settings.ix_plt[v.index], v);
   }},
  {.name = "YP", .first_digit = '2', .last_digit = '8',
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.plot_settings.iy_plt[v.index], v);
   }},
  {.name = "ZP", .first_digit = '2', .last_digit = '8',
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.plot_settings.iz_plt[v.index], v);
   }},
  {.name = "XLO", .first_digit = '2', .last_digit = '8',
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return number_into(s.plot_settings.x_lo[v.index], v);
   }},
  {.name = "XHI", .first_digit = '2', .last_digit = '8',
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return number_into(s.plot_settings.x_hi[v.index], v);
   }},
  {.name = "YLO", .first_digit = '2', .last_digit = '8',
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return number_into(s.plot_settings.y_lo[v.index], v);
   }},
  {.name = "YHI", .first_digit = '2', .last_digit = '8',
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return number_into(s.plot_settings.y_hi[v.index], v);
   }},
  {.name = "SIMPLOT",
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (v.apply) s.plot_windows.simul = 1;
     return nullptr;
   }},
  {.name = "MULTIWIN",
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (v.apply) s.plot_settings.multi_win = 1;
     return nullptr;
   }},
  {.name = "AXES", .flag = Option::AXES,
   .whole = [](Session &s) -> int & { return s.plot_settings.axes; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     int axes = 0;
     switch (v.text[0]) {
     case '2': axes = 0; break;
     case '3': axes = 5; break;
     default: return "not 2 or 3";
     }
     if (v.apply) s.plot_settings.axes = axes;
     return nullptr;
   },
   /* a 2D plot */
   .reset = [](Session &s) { s.plot_settings.axes = 0; }},
  /* the 2D window; each default is the 3D box's too, which then keeps it */
  {.name = "XLO", .flag = Option::XLO,
   .real = [](Session &s) -> double & { return s.plot_settings.my_xlo; },
   /* the default total's span, 0 to 20 */
   .reset = [](Session &s) {
     s.plot_settings.my_xlo = s.plot_settings.x_3d[0] = 0;
     s.options_set.mark(Option::XMIN);
   }},
  {.name = "XHI", .flag = Option::XHI,
   .real = [](Session &s) -> double & { return s.plot_settings.my_xhi; },
   .reset = [](Session &s) {
     s.plot_settings.my_xhi = s.plot_settings.x_3d[1] = 20;
     s.options_set.mark(Option::XMAX);
   }},
  {.name = "YLO", .flag = Option::YLO,
   .real = [](Session &s) -> double & { return s.plot_settings.my_ylo; },
   /* -1 to 1 */
   .reset = [](Session &s) {
     s.plot_settings.my_ylo = s.plot_settings.y_3d[0] = -1;
     s.options_set.mark(Option::YMIN);
   }},
  {.name = "YHI", .flag = Option::YHI,
   .real = [](Session &s) -> double & { return s.plot_settings.my_yhi; },
   .reset = [](Session &s) {
     s.plot_settings.my_yhi = s.plot_settings.y_3d[1] = 1;
     s.options_set.mark(Option::YMAX);
   }},
  /* the 3D box; xmin and ymin set the 2D window's low end too, unless
     something set that */
  {.name = "XMIN", .flag = Option::XMIN,
   .real = [](Session &s) -> double & { return s.plot_settings.x_3d[0]; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (const char *why = number_into(s.plot_settings.x_3d[0], v)) return why;
     if (v.apply && v.source.claim(s, Option::XLO)) s.plot_settings.my_xlo = s.plot_settings.x_3d[0];
     return nullptr;
   },
   .reset = [](Session &s) {
     s.plot_settings.x_3d[0] = -box_side;
     s.options_set.mark(Option::XLO);
   }},
  {.name = "XMAX", .flag = Option::XMAX,
   .real = [](Session &s) -> double & { return s.plot_settings.x_3d[1]; },
   .reset = [](Session &s) {
     s.plot_settings.x_3d[1] = box_side;
     s.options_set.mark(Option::XHI);
   }},
  {.name = "YMIN", .flag = Option::YMIN,
   .real = [](Session &s) -> double & { return s.plot_settings.y_3d[0]; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (const char *why = number_into(s.plot_settings.y_3d[0], v)) return why;
     if (v.apply && v.source.claim(s, Option::YLO)) s.plot_settings.my_ylo = s.plot_settings.y_3d[0];
     return nullptr;
   },
   .reset = [](Session &s) {
     s.plot_settings.y_3d[0] = -box_side;
     s.options_set.mark(Option::YLO);
   }},
  {.name = "YMAX", .flag = Option::YMAX,
   .real = [](Session &s) -> double & { return s.plot_settings.y_3d[1]; },
   .reset = [](Session &s) {
     s.plot_settings.y_3d[1] = box_side;
     s.options_set.mark(Option::YHI);
   }},
  {.name = "ZMIN", .flag = Option::ZMIN,
   .real = [](Session &s) -> double & { return s.plot_settings.z_3d[0]; },
   .reset = [](Session &s) { s.plot_settings.z_3d[0] = -box_side; }},
  {.name = "ZMAX", .flag = Option::ZMAX,
   .real = [](Session &s) -> double & { return s.plot_settings.z_3d[1]; },
   .reset = [](Session &s) { s.plot_settings.z_3d[1] = box_side; }},
  {.name = "PHI", .flag = Option::PHI,
   .real = [](Session &s) -> double & { return s.drawing.phi0; }},
  {.name = "THETA", .flag = Option::THETA,
   .real = [](Session &s) -> double & { return s.drawing.theta0; }},
  {.name = "LT", .flag = Option::LT,
   .whole = [](Session &s) -> int & { return s.plot_settings.start_line_type; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     int i = 0;
     if (whole_in(v, i) || i >= 2 || i <= -6) return "not a line type from -5 to 1";
     if (!v.apply) return nullptr;
     s.plot_settings.start_line_type = i;
     reset_all_line_type(s);
     return nullptr;
   }},
  {.name = "YNC", .flag = Option::YNC,
   .whole = [](Session &s) -> int & { return s.nullclines.y_null_color; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return color_into(s.nullclines.y_null_color, v);
   }},
  {.name = "XNC", .flag = Option::XNC,
   .whole = [](Session &s) -> int & { return s.nullclines.x_null_color; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return color_into(s.nullclines.x_null_color, v);
   }},
  {.name = "SMC", .flag = Option::SMC,
   .whole = [](Session &s) -> int & { return s.manifolds.stable_color; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return color_into(s.manifolds.stable_color, v);
   }},
  {.name = "UMC", .flag = Option::UMC,
   .whole = [](Session &s) -> int & { return s.manifolds.unstable_color; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return color_into(s.manifolds.unstable_color, v);
   }},
  {.name = "COLORMAP", .flag = Option::COLORMAP,
   .whole = [](Session &s) -> int & { return s.colormap; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     int i = 0;
     if (whole_in(v, i) || i < 0 || i >= 7) return "not a colour map from 0 to 6";
     if (v.apply) s.colormap = i;
     return nullptr;
   }},
  {.name = "PLOTFMT", .flag = Option::PLOTFMT,
   .text = [](Session &s) -> std::string & { return s.plot_export.format; }},
  {.name = "PS_FONT", .flag = Option::PS_FONT,
   .text = [](Session &s) -> std::string & { return s.plot_file.ps_font; }},
  {.name = "PS_LW", .flag = Option::PS_LW,
   .real = [](Session &s) -> double & { return s.plot_file.ps_lw; }},
  {.name = "PS_FSIZE", .flag = Option::PS_FSIZE,
   .whole = [](Session &s) -> int & { return s.plot_file.ps_font_size; }},
  {.name = "PS_COLOR", .flag = Option::PS_COLOR,
   .whole = [](Session &s) -> int & { return s.plot_file.ps_color_flag; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (const char *why = whole_into(s.plot_file.ps_color_flag, v)) return why;
     if (v.apply) s.plot_export.color = s.plot_file.ps_color_flag;
     return nullptr;
   }},
  {.name = "DFGRID", .flag = Option::DFGRID,
   .whole = [](Session &s) -> int & { return s.nullclines.df_grid; }},
  {.name = "DFDRAW", .flag = Option::DFDRAW,
   .whole = [](Session &s) -> int & { return s.nullclines.df_batch; }},
  {.name = "NCDRAW", .flag = Option::NCDRAW,
   .whole = [](Session &s) -> int & { return s.nullclines.nc_batch; }},
  {.name = "COLORVIA", .flag = Option::COLORVIA,
   .text = [](Session &s) -> std::string & { return s.nullclines.color_via; }},
  {.name = "COLORIZE", .flag = Option::COLORIZE,
   .whole = [](Session &s) -> int & { return s.nullclines.colorize_flag; }},
  {.name = "COLORLO", .flag = Option::COLORLO,
   .real = [](Session &s) -> double & { return s.nullclines.color_via_lo; }},
  {.name = "COLORHI", .flag = Option::COLORHI,
   .real = [](Session &s) -> double & { return s.nullclines.color_via_hi; }},
  /* the parameter sliders */
  {.name = "S1", .flag = Option::S1,
   .text = [](Session &s) -> std::string & { return s.sliders[0].var; }},
  {.name = "S2", .flag = Option::S2,
   .text = [](Session &s) -> std::string & { return s.sliders[1].var; }},
  {.name = "S3", .flag = Option::S3,
   .text = [](Session &s) -> std::string & { return s.sliders[2].var; }},
  {.name = "SLO1", .flag = Option::SLO1,
   .real = [](Session &s) -> double & { return s.sliders[0].lo; }},
  {.name = "SLO2", .flag = Option::SLO2,
   .real = [](Session &s) -> double & { return s.sliders[1].lo; }},
  {.name = "SLO3", .flag = Option::SLO3,
   .real = [](Session &s) -> double & { return s.sliders[2].lo; }},
  {.name = "SHI1", .flag = Option::SHI1,
   .real = [](Session &s) -> double & { return s.sliders[0].hi; }},
  {.name = "SHI2", .flag = Option::SHI2,
   .real = [](Session &s) -> double & { return s.sliders[1].hi; }},
  {.name = "SHI3", .flag = Option::SHI3,
   .real = [](Session &s) -> double & { return s.sliders[2].hi; }},

  /* ---- the range integration and a batch run ---- */
  {.name = "RANGEOVER", .flag = Option::RANGEOVER,
   .text = [](Session &s) -> std::string & { return s.integrator.range.item; },
   /* the first variable */
   .reset = [](Session &s) { s.integrator.range.item = s.model().uvar_names[0]; }},
  {.name = "RANGESTEP", .flag = Option::RANGESTEP,
   .whole = [](Session &s) -> int & { return s.integrator.range.steps; },
   /* twenty runs */
   .reset = [](Session &s) { s.integrator.range.steps = 20; }},
  {.name = "RANGELOW", .flag = Option::RANGELOW,
   .real = [](Session &s) -> double & { return s.integrator.range.plow; },
   /* 0 to 1 */
   .reset = [](Session &s) { s.integrator.range.plow = s.integrator.range.plow2 = 0; }},
  {.name = "RANGEHIGH", .flag = Option::RANGEHIGH,
   .real = [](Session &s) -> double & { return s.integrator.range.phigh; },
   .reset = [](Session &s) { s.integrator.range.phigh = s.integrator.range.phigh2 = 1; }},
  {.name = "RANGERESET", .flag = Option::RANGERESET,
   .whole = [](Session &s) -> int & { return s.integrator.range.reset; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return yes_no_into(s.integrator.range.reset, v);
   },
   /* each run starts from the initial data */
   .reset = [](Session &s) { s.integrator.range.reset = 1; }},
  {.name = "RANGEOLDIC", .flag = Option::RANGEOLDIC,
   .whole = [](Session &s) -> int & { return s.integrator.range.oldic; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return yes_no_into(s.integrator.range.oldic, v);
   },
   /* the same initial data for each run */
   .reset = [](Session &s) { s.integrator.range.oldic = 1; }},
  {.name = "RANGE", .flag = Option::RANGE,
   .whole = [](Session &) -> int & { return batch_options.range; }},
  {.name = "OUTPUT", .flag = Option::OUTPUT,
   .text = [](Session &) -> std::string & { return batch_options.out_file; }},
  {.name = "RUNNOW", .flag = Option::RUNNOW,
   .whole = [](Session &s) -> int & { return s.run_immediately; }},

  /* ---- AUTO ---- */
  {.name = "NTST", .flag = Option::NTST,
   .whole = [](Session &s) -> int & { return s.auto_state.options.ntst; }},
  {.name = "NMAX", .flag = Option::NMAX,
   .whole = [](Session &s) -> int & { return s.auto_state.options.nmx; }},
  {.name = "NPR", .flag = Option::NPR,
   .whole = [](Session &s) -> int & { return s.auto_state.options.npr; }},
  {.name = "NCOL", .flag = Option::NCOL,
   .whole = [](Session &s) -> int & { return s.auto_state.options.ncol; }},
  {.name = "DSMIN", .flag = Option::DSMIN,
   .real = [](Session &s) -> double & { return s.auto_state.options.dsmin; }},
  {.name = "DSMAX", .flag = Option::DSMAX,
   .real = [](Session &s) -> double & { return s.auto_state.options.dsmax; }},
  {.name = "DS", .flag = Option::DS,
   .real = [](Session &s) -> double & { return s.auto_state.options.ds; }},
  {.name = "PARMIN", .flag = Option::PARMIN,
   .real = [](Session &s) -> double & { return s.auto_state.options.rl0; }},
  {.name = "PARMAX", .flag = Option::PARMAX,
   .real = [](Session &s) -> double & { return s.auto_state.options.rl1; }},
  {.name = "NORMMIN", .flag = Option::NORMMIN,
   .real = [](Session &s) -> double & { return s.auto_state.options.a0; }},
  {.name = "NORMMAX", .flag = Option::NORMMAX,
   .real = [](Session &s) -> double & { return s.auto_state.options.a1; }},
  {.name = "EPSL", .flag = Option::EPSL,
   .real = [](Session &s) -> double & { return s.auto_state.options.epsl; }},
  {.name = "EPSU", .flag = Option::EPSU,
   .real = [](Session &s) -> double & { return s.auto_state.options.epsu; }},
  {.name = "EPSS", .flag = Option::EPSS,
   .real = [](Session &s) -> double & { return s.auto_state.options.epss; }},
  {.name = "AUTOXMAX", .flag = Option::AUTOXMAX,
   .real = [](Session &s) -> double & { return s.auto_state.options.xmax; }},
  {.name = "AUTOYMAX", .flag = Option::AUTOYMAX,
   .real = [](Session &s) -> double & { return s.auto_state.options.ymax; }},
  {.name = "AUTOXMIN", .flag = Option::AUTOXMIN,
   .real = [](Session &s) -> double & { return s.auto_state.options.xmin; }},
  {.name = "AUTOYMIN", .flag = Option::AUTOYMIN,
   .real = [](Session &s) -> double & { return s.auto_state.options.ymin; }},
  {.name = "AUTOVAR", .flag = Option::AUTOVAR,
   .whole = [](Session &s) -> int & { return s.auto_state.options.var; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     const int i = variable_of(s, v);
     if (i < 1) return "no such variable";
     if (v.apply) s.auto_state.options.var = i - 1;
     return nullptr;
   }},
  {.name = "SEC", .flag = Option::SEC,
   .whole = [](Session &s) -> int & { return s.auto_state.stable_eq_color; }},
  {.name = "UEC", .flag = Option::UEC,
   .whole = [](Session &s) -> int & { return s.auto_state.unstable_eq_color; }},
  {.name = "SPC", .flag = Option::SPC,
   .whole = [](Session &s) -> int & { return s.auto_state.stable_po_color; }},
  {.name = "UPC", .flag = Option::UPC,
   .whole = [](Session &s) -> int & { return s.auto_state.unstable_po_color; }},

  /* ---- the histogram and the spectrum a batch run writes ---- */
  {.name = "POSTPROCESS", .flag = Option::POSTPROCESS,
   .whole = [](Session &s) -> int & { return s.histogram.post_process; }},
  {.name = "HISTLO", .flag = Option::HISTLO,
   .real = [](Session &s) -> double & { return s.histogram.info.xlo; }},
  {.name = "HISTHI", .flag = Option::HISTHI,
   .real = [](Session &s) -> double & { return s.histogram.info.xhi; }},
  {.name = "HISTBINS", .flag = Option::HISTBINS,
   .whole = [](Session &s) -> int & { return s.histogram.info.nbins; }},
  {.name = "HISTCOL", .flag = Option::HISTCOL,
   .whole = [](Session &s) -> int & { return s.histogram.info.col; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.histogram.info.col, v);
   }},
  {.name = "HISTLO2", .flag = Option::HISTLO2,
   .real = [](Session &s) -> double & { return s.histogram.info.ylo; }},
  {.name = "HISTHI2", .flag = Option::HISTHI2,
   .real = [](Session &s) -> double & { return s.histogram.info.yhi; }},
  {.name = "HISTBINS2", .flag = Option::HISTBINS2,
   .whole = [](Session &s) -> int & { return s.histogram.info.nbins2; }},
  {.name = "HISTCOL2", .flag = Option::HISTCOL2,
   .whole = [](Session &s) -> int & { return s.histogram.info.col2; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.histogram.info.col2, v);
   }},
  {.name = "SPECCOL", .flag = Option::SPECCOL,
   .whole = [](Session &s) -> int & { return s.histogram.spec_col; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.histogram.spec_col, v);
   }},
  {.name = "SPECCOL2", .flag = Option::SPECCOL2,
   .whole = [](Session &s) -> int & { return s.histogram.spec_col2; },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.histogram.spec_col2, v);
   }},
  {.name = "SPECWIDTH", .flag = Option::SPECWIDTH,
   .whole = [](Session &s) -> int & { return s.histogram.spec_wid; }},
  {.name = "SPECWIN", .flag = Option::SPECWIN,
   .whole = [](Session &s) -> int & { return s.histogram.spec_win; }},

  /* ---- the log, the user's buttons ---- */
  {.name = "QUIET",
   .parse = [](Session &, const OptionValue &v) -> const char * {
     if (v.apply && !log_settings.quiet_from_command_line) log_settings.verbose = v.text[0] == '0';
     return nullptr;
   },
   .zero_or_one = true},
  {.name = "LOGFILE",
   .parse = [](Session &, const OptionValue &v) -> const char * {
     if (v.apply && !log_settings.file_from_command_line && !log_open_file(v.text))
       return "log file open failed";
     return nullptr;
   }},
  {.name = "BUT",
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (v.apply) add_user_button(s, v.text);
     return nullptr;
   }},
  /* the X11 window's bell, fonts, colours, image, size and paper: still
     accepted (old .ode and .xpprc files set them), no longer kept */
  {.name = "BELL", .zero_or_one = true},
  {.name = "TUTORIAL", .zero_or_one = true}, /* the tutorial went with W229 */
  {.name = "BIGFONT", .alias = "BIG"},
  {.name = "SMALLFONT", .alias = "SMALL"},
  {.name = "FORECOLOR"},
  {.name = "BACKCOLOR"},
  {.name = "MWCOLOR"},
  {.name = "DWCOLOR"},
  {.name = "GRADS", .zero_or_one = true},
  {.name = "BACKIMAGE"},
  {.name = "WIDTH"},
  {.name = "HEIGHT"},
  {.name = "BACK"},
};

/* the length of row's name that name starts with (and the digit after a
   numbered one), 0 when it does not */
std::size_t match_length(const OptionRow &row, std::string_view n, std::string_view upper, int &index)
{
  if (n.empty() || !upper.starts_with(n)) return 0;
  if (!row.first_digit) return n.size();
  if (upper.size() <= n.size()) return 0;
  const char d = upper[n.size()];
  if (d < row.first_digit || d > row.last_digit) return 0;
  index = d - '0';
  return n.size() + 1;
}

} // namespace

bool OptionSource::may(const Session &s, Option o) const
{
  return force || !s.options_set.has(o) || (mask && !mask->has(o));
}

bool OptionSource::claim(Session &s, Option o) const
{
  if (!may(s, o)) return false;
  s.options_set.mark(o);
  return true;
}

const char *rule_problem(OptionRule rule, double v)
{
  switch (rule) {
  case OptionRule::nonzero: return v != 0 ? nullptr : "must be a number other than 0";
  case OptionRule::positive: return v > 0 ? nullptr : "must be a number above 0";
  case OptionRule::nonnegative: return v >= 0 ? nullptr : "must be a number of at least 0";
  case OptionRule::whole_positive:
    return v == std::floor(v) && v >= 1 && v <= INT_MAX ? nullptr : "must be a whole number of at least 1";
  case OptionRule::any:
  case OptionRule::method:
    break;
  }
  return nullptr;
}

std::span<const OptionRow> option_rows() { return rows; }

const OptionRow *find_option(std::string_view upper_name, int &index, bool xppaut_names)
{
  const OptionRow *best = nullptr;
  std::size_t best_length = 0;
  for (const OptionRow &row : rows)
    for (std::string_view n : {row.name, row.alias, xppaut_names ? row.xppaut_names[0] : "", xppaut_names ? row.xppaut_names[1] : ""}) {
      int i = 0;
      const std::size_t length = match_length(row, n, upper_name, i);
      if (length > best_length) {
        best = &row;
        best_length = length;
        index = i;
      }
    }
  return best;
}

std::string odex_option_key(std::string_view key)
{
  const std::string upper = upper_case(std::string(key));
  int index = 0;
  const OptionRow *row = find_option(upper, index, true);
  /* a name a .odex reads as it stands (its own, or a longer spelling of
     its row's) stays; XPPAUT's word for a row becomes the row's name */
  if (!row || find_option(upper, index, false) == row) return std::string(key);
  return lower_case(std::string(row->name));
}

const OptionRow *numerics_option(std::string_view key)
{
  const auto it = std::ranges::find_if(rows, [key](const OptionRow &r) { return !r.key.empty() && r.key == key; });
  return it == std::ranges::end(rows) ? nullptr : &*it;
}

namespace {

/* what option_value says of a name no option has */
constexpr const char *unknown_option = "not an option";

/* option name set to value (apply), or only checked; why it is not one,
   empty when it is (or another source set it, which set_option leaves) */
std::optional<std::string> option_value(Session &s, std::string_view name, std::string_view value, bool force, const OptionsSet *mask, bool apply,
                                        bool xppaut_names)
{
  const std::string upper = upper_case(std::string(name));
  /* the value as a NUL-ended text */
  const std::string text(value);
  int index = 0;
  const OptionRow *row = find_option(upper, index, xppaut_names);
  if (!row) return unknown_option;
  const OptionSource source{force, mask};
  const OptionValue v{text.c_str(), index, source, apply};
  const char *why = nullptr;
  /* a number the setting's rule refuses (a parser checks its own) */
  double x = 0;
  const char *refused = !row->parse && parse_number(text, x) ? rule_problem(row->rule, x) : nullptr;
  if (row->zero_or_one && text != "0" && text != "1") why = "not 0 or 1";
  else if (row->flag != Option::none && !source.may(s, row->flag)) return std::nullopt;
  else if (refused) why = refused;
  else if (row->rule == OptionRule::method) {
    const auto picked = pick_method(s.model(), text, Load::running() ? Load::place() : command_place());
    if (!picked) return picked.error().what;
    if (apply) s.numerics.method = *picked;
  }
  else if (row->parse) {
    why = row->parse(s, v);
    if (why && upper == "LOGFILE") return files::open_error("option", value).what;
  }
  else if (row->real) why = number_into(row->real(s), v);
  else if (row->whole) why = whole_into(row->whole(s), v);
  else if (row->text && apply) row->text(s) = text;
  if (!why && apply && row->flag != Option::none) s.options_set.mark(row->flag);
  return why ? std::optional<std::string>(why) : std::nullopt;
}

} // namespace

std::optional<std::string> option_problem(Session &s, std::string_view name, std::string_view value, bool xppaut_names)
{
  return option_value(s, name, value, true, nullptr, false, xppaut_names);
}

void set_option(Session &s, std::string_view name, std::string_view value, bool force, const OptionsSet *mask)
{
  const auto why = option_value(s, name, value, force, mask, true, true);
  if (!why) return;
  if (*why == unknown_option) {
    xpp::log(XPP_LOG_WARN, "Option {} not recognized\n", upper_case(std::string(name)));
    return;
  }
  /* the load stops, at the option's line; outside a load (an internal
     set, checked first) the error is shown and nothing else changes */
  if (Load::running()) model_failed(xpp::format("@ {}={}: {}", name, value, *why));
  const Error e{"options", xpp::format("@ {}={}: {}", name, value, *why), command_place()};
  show_error(e);
}

void set_option_defaults(Session &s)
{
  for (const OptionRow &row : rows) {
    if (!row.reset || (row.flag != Option::none && s.options_set.has(row.flag))) continue;
    row.reset(s);
    if (row.flag != Option::none) s.options_set.mark(row.flag);
  }
}

} // namespace xpp
