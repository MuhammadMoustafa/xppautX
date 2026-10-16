#include "xpp_files.h"
/* The model's options as one table (model_options.h, W119). */
#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <string>
#include <string_view>

#include "model_options.h"
#include "setx.h"
#include "odex.h"
#include "xpp_util.h"
#include "auto_settings.h"
#include "xpp_session_auto.h"
#include "browse.h"
#include "colormap.h"
#include "expr_native.h"
#include "command_table.h"
#include "graphics.h"
#include "jacobian.h"
#include "model.h"
#include "model_files.h"
#include "numerics.h"
#include "pp_shoot.h"
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

/* what the log file's option says of a file that could not be opened */
constexpr const char *log_open_failed = "log file open failed";

/* the file a path option names, from the model's folder, into path: inside
   it, or outside it when the user allows (approve_model_path, which asks
   once); else why not. A stored bad preferences file stops the load. */
const char *own_file(Session &s, const OptionValue &v, std::string &path)
{
  path = include_path(s.model().this_file, v.text);
  if (path.empty()) return "must name a file";
  const Result<> allowed = approve_model_path(s.model(), path, Load::running() ? Load::place() : command_place());
  if (allowed) return nullptr;
  if (allowed.error().where != PATH_REFUSED) model_failed(allowed.error());
  return "names a path outside the model's folder that was refused";
}

/* the words a .odex writes for the choice rows (option_choices); the
   Jacobian's are jacobian.h's, the method's the registry's */
constexpr std::array<std::string_view, 2> yes_no_names{"no", "yes"};
constexpr std::array<std::string_view, 2> axes_names{"2", "3"};
constexpr std::array<std::string_view, 4> poincare_map_names{"none", "section", "max", "period"};

/* Only foreign S1-S3 options need numbered slots; native presets are a list. */
XppSlider &slider_slot(Session &s, std::size_t index)
{
  if (s.sliders.size() <= index) s.sliders.resize(index + 1);
  return s.sliders[index];
}

/* @ s1..s3, slo1..3 and shi1..3 (import only): one slot's parameter, low or high */
template <std::size_t slot> const char *slider_var(Session &s, const OptionValue &v)
{
  if (v.apply) slider_slot(s, slot).var = v.text;
  return nullptr;
}
template <std::size_t slot, double XppSlider::*end> const char *slider_end(Session &s, const OptionValue &v)
{
  double value = 0;
  if (const char *why = number_in(v, value)) return why;
  if (v.apply) slider_slot(s, slot).*end = value;
  return nullptr;
}
constexpr std::array slider_slot_parsers{
  slider_var<0>, slider_var<1>, slider_var<2>,
  slider_end<0, &XppSlider::lo>, slider_end<1, &XppSlider::lo>, slider_end<2, &XppSlider::lo>,
  slider_end<0, &XppSlider::hi>, slider_end<1, &XppSlider::hi>, slider_end<2, &XppSlider::hi>};

constexpr OptionRow rows[] = {
  /* ---- the numerics the Numerics menu and `set num` edit, in the menu's
     order ---- */
  {.group = option_group::integration, .kind = OptionKind::number, .name = "TOTAL_TIME", .xppaut_names = {"TOTAL"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.total_time; }),
   /* twenty time units, XPPAUT's */
   .reset = [](Session &s) { s.numerics.total_time = 20; },
   .key = "total_time", .label = "Total time", .rule = OptionRule::positive, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::number, .name = "START_TIME", .xppaut_names = {"T0"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.start_time; }),
   /* time starts at 0 */
   .reset = [](Session &s) { s.numerics.start_time = 0; },
   .key = "start_time", .label = "Start time", .saved = true},
  {.group = option_group::integration, .kind = OptionKind::number, .name = "TRANSIENT_TIME", .xppaut_names = {"TRANS"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.transient_time; }),
   /* no transient: store from the start */
   .reset = [](Session &s) { s.numerics.transient_time = 0; },
   .key = "transient_time", .label = "Transient time", .saved = true},
  {.group = option_group::integration, .kind = OptionKind::number, .name = "DT", .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.delta_t; }),
   /* twenty steps per time unit, XPPAUT's */
   .reset = [](Session &s) { s.numerics.delta_t = .05; },
   .key = "dt", .label = "Dt", .rule = OptionRule::nonzero, .saved = true, .applied = dt_changed},
  {.group = option_group::plot, .kind = OptionKind::whole, .name = "NULLCLINE_MESH", .xppaut_names = {"NMESH"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.numerics.nullcline_mesh; }),
   /* a 40 by 40 grid: smooth nullclines, quickly */
   .reset = [](Session &s) { s.numerics.nullcline_mesh = 40; },
   .key = "nullcline_mesh", .label = "Nullcline mesh", .rule = OptionRule::whole_positive, .maximum = option_limit::mesh, .saved = true},
  {.group = option_group::equilibria, .kind = OptionKind::whole, .name = "SINGPT_MAX_ITERATES", .xppaut_names = {"NEWT_ITER"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.numerics.singpt_max_iterates; }),
   /* Newton's method converges in far fewer or not at all */
   .reset = [](Session &s) { s.numerics.singpt_max_iterates = 100; },
   .key = "singpt_max_iterates", .label = "Sing pt: maximum iterates", .rule = OptionRule::whole_positive, .saved = true},
  {.group = option_group::equilibria, .kind = OptionKind::number, .name = "SINGPT_NEWTON_TOLERANCE", .xppaut_names = {"NEWT_TOL"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.singpt_newton_tolerance; }),
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.singpt_newton_tolerance = .001; },
   .key = "singpt_newton_tolerance", .label = "Sing pt: Newton tolerance", .rule = OptionRule::positive, .saved = true},
  {.group = option_group::equilibria, .kind = OptionKind::number, .name = "SINGPT_JACOBIAN_EPSILON", .xppaut_names = {"JAC_EPS"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.singpt_jacobian_epsilon; }),
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.singpt_jacobian_epsilon = .001; },
   .key = "singpt_jacobian_epsilon", .label = "Sing pt: Jacobian epsilon", .rule = OptionRule::positive, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::whole, .name = "STORE_EVERY", .xppaut_names = {"NOUT", "NJMP"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.numerics.store_every; }),
   /* every step stored */
   .reset = [](Session &s) { s.numerics.store_every = 1; },
   .key = "store_every", .label = STORE_EVERY_LABEL, .rule = OptionRule::whole_positive, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::number, .name = "BOUND", .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.bound; }),
   /* XPPAUT's: past it a solution is taken to blow up */
   .reset = [](Session &s) { s.numerics.bound = 100; },
   .key = "bound", .label = "Bounds", .rule = OptionRule::positive, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::choice, .name = "METHOD", .xppaut_names = {"METH"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.numerics.method; }),
   /* Integral equations require Volterra; otherwise the usual RK4 default. */
   .reset = [](Session &s) {
     s.numerics.method = pick_method(s.model(), s.model().nkernel > 0 ? "Volterra" : "Runge-Kutta", command_place()).value();
   },
   .key = "method", .label = "Method", .rule = OptionRule::method, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::choice, .name = "JACOBIAN", .once = true,
   .whole = member([](auto &s) -> auto & { return s.numerics.jacobian; }),
   /* during a load, exact is checked once the model is compiled
      (xpp_batch.cpp), at the place kept here */
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     const int i = choice_index(jacobian_names, v.text);
     if (i < 0) return "not difference or exact";
     if (i == JACOBIAN_EXACT && !Load::running() && !ensure_exact_jacobian(s)) return s.model().jacobian.unavailable.c_str();
     if (!v.apply) return nullptr;
     if (i == JACOBIAN_EXACT && Load::running()) s.model().jacobian.requested = Load::place();
     s.numerics.jacobian = i;
     return nullptr;
   },
   /* XPPAUT's differences: a model gives XPPAUT's numbers unless it asks */
   .reset = [](Session &s) { s.numerics.jacobian = JACOBIAN_DIFFERENCE; },
   .key = JACOBIAN_KEY, .label = "Jacobian", .rule = OptionRule::choice,
   .choices = jacobian_names,
   /* before the Jacobian is tried, exact is offered (the reason comes
      once it was chosen) */
   .unavailable = [](const Model &m, int i) -> std::string_view {
     return i == JACOBIAN_EXACT && m.jacobian.tried && !m.jacobian.available() ? std::string_view(m.jacobian.unavailable) : std::string_view();
   },
   .prepare = [](Session &s, int i) { if (i == JACOBIAN_EXACT) ensure_exact_jacobian(s); }, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::number, .name = "TOLERANCE", .xppaut_names = {"TOL"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.tolerance; }),
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.tolerance = .001; },
   .key = "tolerance", .label = "Tolerance", .rule = OptionRule::positive, .use = OptionUse::step_or_rel, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::number, .name = "MIN_STEP", .xppaut_names = {"DTMIN"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.min_step; }),
   /* as small as an adaptive method may need */
   .reset = [](Session &s) { s.numerics.min_step = 1e-12; },
   .key = "min_step", .label = "Minimum step", .rule = OptionRule::positive, .use = OptionUse::step, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::number, .name = "MAX_STEP", .xppaut_names = {"DTMAX"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.max_step; }),
   /* one time unit */
   .reset = [](Session &s) { s.numerics.max_step = 1; },
   .key = "max_step", .label = "Maximum step", .rule = OptionRule::positive, .use = OptionUse::step_or_dense, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::number, .name = "ABS_TOLERANCE", .xppaut_names = {"ATOL"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.abs_tolerance; }),
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.abs_tolerance = .001; },
   .key = "abs_tolerance", .label = "Absolute tolerance", .rule = OptionRule::positive, .use = OptionUse::rel, .saved = true},
  {.real = member([](auto &s) -> auto & { return s.numerics.eul_tol; }),
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.eul_tol = 1e-7; },
   .key = "eul_tol", .label = "Newton tolerance", .rule = OptionRule::positive, .use = OptionUse::newton, .saved = true},
  {.whole = member([](auto &s) -> auto & { return s.numerics.max_eul_iter; }),
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.max_eul_iter = 10; },
   .key = "eul_iter", .label = "Newton iterations", .rule = OptionRule::whole_positive, .use = OptionUse::newton, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::number, .name = "DELAY", .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.delay; }),
   /* no delays kept */
   .reset = [](Session &s) { s.numerics.delay = 0; },
   .key = "delay", .label = "Maximal delay", .rule = OptionRule::nonnegative, .use = OptionUse::delays, .saved = true, .applied = chk_delay},
  {.whole = member([](auto &s) -> auto & { return s.numerics.bvp_max_iterates; }),
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.bvp_max_iterates = 20; },
   .key = "bvp_max_iterates", .label = "BVP maximum iterates", .rule = OptionRule::whole_positive, .saved = true, .applied = reset_bvp},
  {.real = member([](auto &s) -> auto & { return s.numerics.bvp_tolerance; }),
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.bvp_tolerance = 1e-5; },
   .key = "bvp_tolerance", .label = "BVP tolerance", .rule = OptionRule::positive, .saved = true, .applied = reset_bvp},
  {.real = member([](auto &s) -> auto & { return s.numerics.bvp_epsilon; }),
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.bvp_epsilon = 1e-5; },
   .key = "bvp_epsilon", .label = "BVP epsilon", .rule = OptionRule::positive, .saved = true, .applied = reset_bvp},

  /* ---- the other numerics ---- */
  /* near()'s tolerance: the .odex reader takes it into the formulas that
     call near (odex_load.cpp scan_neartol); nothing to set at run time */
  {.group = option_group::equilibria, .name = "NEARTOL", .rule = OptionRule::positive},
  {.group = option_group::integration, .kind = OptionKind::whole, .name = "VMAXPTS", .once = true,
   .whole = member([](auto &s) -> auto & { return s.numerics.max_points; }), .rule = OptionRule::whole_positive, .maximum = option_limit::history_points, .use = OptionUse::integral_history, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::whole, .name = "STORAGE_ROWS", .xppaut_names = {"MAXSTOR"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.data_store.max_rows; }),
   /* rows the data store starts with: a run of 5000 steps (it grows) */
   .reset = [](Session &s) { s.data_store.max_rows = 5000; }, .rule = OptionRule::whole_positive, .maximum = option_limit::storage_rows},
  {.group = option_group::integration, .kind = OptionKind::number, .name = "TORUS_PERIOD", .xppaut_names = {"TOR_PER"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.torus_period; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (const char *why = number_into(s.numerics.torus_period, v)) return why;
     if (v.apply) s.numerics.torus = 1;
     return nullptr;
   }},
  {.group = option_group::integration, .kind = OptionKind::name, .name = "FOLD",
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     const int i = variable_of(s, v);
     if (i < 1) return "no such variable";
     if (!v.apply) return nullptr;
     s.itor[i - 1] = 1;
     s.numerics.torus = 1;
     return nullptr;
   },
   .repeatable = true},
  {.group = option_group::integration, .kind = OptionKind::whole, .name = "BANDUP", .once = true,
   .whole = member([](auto &s) -> auto & { return s.numerics.cv_bandupper; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (const char *why = whole_into(s.numerics.cv_bandupper, v)) return why;
     if (v.apply) s.numerics.cv_bandflag = 1;
     return nullptr;
   }, .rule = OptionRule::nonnegative, .maximum = option_limit::bandwidth},
  {.group = option_group::integration, .kind = OptionKind::whole, .name = "BANDLO", .once = true,
   .whole = member([](auto &s) -> auto & { return s.numerics.cv_bandlower; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (const char *why = whole_into(s.numerics.cv_bandlower, v)) return why;
     if (v.apply) s.numerics.cv_bandflag = 1;
     return nullptr;
   }, .rule = OptionRule::nonnegative, .maximum = option_limit::bandwidth},
  {.group = option_group::poincare, .kind = OptionKind::choice, .name = "POINCARE_MAP", .xppaut_names = {"POIMAP"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.numerics.poincare_map; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     int map = 0;
     switch (v.text[0]) {
     case 'n': case 'N': map = 0; break;
     case 's': case 'S': map = 1; break;
     case 'm': case 'M': map = 2; break;
     case 'p': case 'P': map = 3; break;
     default: return "not none, section, max or period";
     }
     if (v.apply) s.numerics.poincare_map = map;
     return nullptr;
   },
   /* no Poincare map */
   .reset = [](Session &s) { s.numerics.poincare_map = 0; },
   .choices = poincare_map_names, .saved = true},
  {.group = option_group::poincare, .kind = OptionKind::name, .name = "POINCARE_VARIABLE", .xppaut_names = {"POIVAR"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.numerics.poincare_variable; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.numerics.poincare_variable, v);
   },
   /* the first variable */
   .reset = [](Session &s) { s.numerics.poincare_variable = 1; }, .saved = true},
  {.group = option_group::poincare, .kind = OptionKind::whole, .name = "POINCARE_SIGN", .xppaut_names = {"POISGN"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.numerics.poincare_sign; }),
   /* crossings upwards */
   .reset = [](Session &s) { s.numerics.poincare_sign = 1; }, .saved = true},
  {.group = option_group::poincare, .kind = OptionKind::whole, .name = "POINCARE_STOP", .xppaut_names = {"POISTOP"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.numerics.poincare_stop; }),
   /* a section does not stop the run */
   .reset = [](Session &s) { s.numerics.poincare_stop = 0; }, .saved = true},
  {.group = option_group::poincare, .kind = OptionKind::number, .name = "POINCARE_PLANE", .xppaut_names = {"POIPLN"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.poincare_plane; }),
   /* the section at 0 */
   .reset = [](Session &s) { s.numerics.poincare_plane = 0; }, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::whole, .name = "SEED", .once = true,
   .whole = member([](auto &s) -> auto & { return s.numerics.rand_seed; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     int i = 0;
     if (whole_in(v, i) || i < 0) return "not a seed (a whole number of at least 0)";
     if (!v.apply) return nullptr;
     s.numerics.rand_seed = i;
     s.random.seed(s.numerics.rand_seed);
     return nullptr;
   }},
  {.group = option_group::integration, .kind = OptionKind::whole, .name = "STOCH", .once = true,
   .whole = member([](auto &s) -> auto & { return s.stochastic.flag; })},
  {.group = option_group::diagram, .kind = OptionKind::whole, .name = "AUTOEVAL", .once = true,
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     int f = 0;
     if (const char *why = whole_in(v, f)) return why;
     if (v.apply) set_auto_eval_flags(s, f);
     return nullptr;
   }},

  /* ---- the plot: the defaults of xp..zp before nplot's, which copies
     them, and of xlo..yhi before xmin..zmax's ---- */
  {.group = option_group::plot, .kind = OptionKind::name, .name = "XP", .once = true,
   .whole = member([](auto &s) -> auto & { return s.plot_settings.ixplt; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.plot_settings.ixplt, v);
   },
   /* the time */
   .reset = [](Session &s) { s.plot_settings.ixplt = 0; }},
  {.group = option_group::plot, .kind = OptionKind::name, .name = "YP", .once = true,
   .whole = member([](auto &s) -> auto & { return s.plot_settings.iyplt; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.plot_settings.iyplt, v);
   },
   /* the first variable */
   .reset = [](Session &s) { s.plot_settings.iyplt = 1; }},
  {.group = option_group::plot, .kind = OptionKind::name, .name = "ZP", .once = true,
   .whole = member([](auto &s) -> auto & { return s.plot_settings.izplt; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.plot_settings.izplt, v);
   },
   /* the first variable */
   .reset = [](Session &s) { s.plot_settings.izplt = 1; }},
  {.group = option_group::plot, .kind = OptionKind::whole, .name = "NPLOT", .once = true,
   .whole = member([](auto &s) -> auto & { return s.plot_settings.npltv; }),
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
  {.group = option_group::plot, .kind = OptionKind::name, .name = "XP", .first_digit = '2', .last_digit = '8',
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.plot_settings.ix_plt[v.index], v);
   }},
  {.group = option_group::plot, .kind = OptionKind::name, .name = "YP", .first_digit = '2', .last_digit = '8',
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.plot_settings.iy_plt[v.index], v);
   }},
  {.group = option_group::plot, .kind = OptionKind::name, .name = "ZP", .first_digit = '2', .last_digit = '8',
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.plot_settings.iz_plt[v.index], v);
   }},
  {.group = option_group::plot, .kind = OptionKind::number, .name = "XLO", .first_digit = '2', .last_digit = '8',
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return number_into(s.plot_settings.x_lo[v.index], v);
   }},
  {.group = option_group::plot, .kind = OptionKind::number, .name = "XHI", .first_digit = '2', .last_digit = '8',
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return number_into(s.plot_settings.x_hi[v.index], v);
   }},
  {.group = option_group::plot, .kind = OptionKind::number, .name = "YLO", .first_digit = '2', .last_digit = '8',
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return number_into(s.plot_settings.y_lo[v.index], v);
   }},
  {.group = option_group::plot, .kind = OptionKind::number, .name = "YHI", .first_digit = '2', .last_digit = '8',
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return number_into(s.plot_settings.y_hi[v.index], v);
   }},
  {.group = option_group::plot, .kind = OptionKind::whole, .name = "SIMPLOT",
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (v.apply) s.plot_windows.simul = 1;
     return nullptr;
   }, .wide = true},
  {.group = option_group::plot, .kind = OptionKind::whole, .name = "MULTIWIN",
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (v.apply) s.plot_settings.multi_win = 1;
     return nullptr;
   }, .wide = true},
  {.group = option_group::plot, .kind = OptionKind::choice, .name = "AXES", .once = true,
   .whole = member([](auto &s) -> auto & { return s.plot_settings.axes; }),
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
   .reset = [](Session &s) { s.plot_settings.axes = 0; },
   .choices = axes_names},
  /* the 2D window; each default is the 3D box's too, which then keeps it */
  {.group = option_group::plot, .kind = OptionKind::number, .name = "XLO", .once = true,
   .real = member([](auto &s) -> auto & { return s.plot_settings.my_xlo; }),
   /* the default total's span, 0 to 20 */
   .reset = [](Session &s) {
     s.plot_settings.my_xlo = s.plot_settings.x_3d[0] = 0;
     s.options_set.mark(once_option("XMIN"));
   }},
  {.group = option_group::plot, .kind = OptionKind::number, .name = "XHI", .once = true,
   .real = member([](auto &s) -> auto & { return s.plot_settings.my_xhi; }),
   .reset = [](Session &s) {
     s.plot_settings.my_xhi = s.plot_settings.x_3d[1] = 20;
     s.options_set.mark(once_option("XMAX"));
   }},
  {.group = option_group::plot, .kind = OptionKind::number, .name = "YLO", .once = true,
   .real = member([](auto &s) -> auto & { return s.plot_settings.my_ylo; }),
   /* -1 to 1 */
   .reset = [](Session &s) {
     s.plot_settings.my_ylo = s.plot_settings.y_3d[0] = -1;
     s.options_set.mark(once_option("YMIN"));
   }},
  {.group = option_group::plot, .kind = OptionKind::number, .name = "YHI", .once = true,
   .real = member([](auto &s) -> auto & { return s.plot_settings.my_yhi; }),
   .reset = [](Session &s) {
     s.plot_settings.my_yhi = s.plot_settings.y_3d[1] = 1;
     s.options_set.mark(once_option("YMAX"));
   }},
  /* the 3D box; xmin and ymin set the 2D window's low end too, unless
     something set that */
  {.group = option_group::plot, .kind = OptionKind::number, .name = "XMIN", .once = true,
   .real = member([](auto &s) -> auto & { return s.plot_settings.x_3d[0]; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (const char *why = number_into(s.plot_settings.x_3d[0], v)) return why;
     if (v.apply && v.source.claim(s, once_option("XLO"))) s.plot_settings.my_xlo = s.plot_settings.x_3d[0];
     return nullptr;
   },
   .reset = [](Session &s) {
     s.plot_settings.x_3d[0] = -box_side;
     s.options_set.mark(once_option("XLO"));
   }},
  {.group = option_group::plot, .kind = OptionKind::number, .name = "XMAX", .once = true,
   .real = member([](auto &s) -> auto & { return s.plot_settings.x_3d[1]; }),
   .reset = [](Session &s) {
     s.plot_settings.x_3d[1] = box_side;
     s.options_set.mark(once_option("XHI"));
   }},
  {.group = option_group::plot, .kind = OptionKind::number, .name = "YMIN", .once = true,
   .real = member([](auto &s) -> auto & { return s.plot_settings.y_3d[0]; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (const char *why = number_into(s.plot_settings.y_3d[0], v)) return why;
     if (v.apply && v.source.claim(s, once_option("YLO"))) s.plot_settings.my_ylo = s.plot_settings.y_3d[0];
     return nullptr;
   },
   .reset = [](Session &s) {
     s.plot_settings.y_3d[0] = -box_side;
     s.options_set.mark(once_option("YLO"));
   }},
  {.group = option_group::plot, .kind = OptionKind::number, .name = "YMAX", .once = true,
   .real = member([](auto &s) -> auto & { return s.plot_settings.y_3d[1]; }),
   .reset = [](Session &s) {
     s.plot_settings.y_3d[1] = box_side;
     s.options_set.mark(once_option("YHI"));
   }},
  {.group = option_group::plot, .kind = OptionKind::number, .name = "ZMIN", .once = true,
   .real = member([](auto &s) -> auto & { return s.plot_settings.z_3d[0]; }),
   .reset = [](Session &s) { s.plot_settings.z_3d[0] = -box_side; }},
  {.group = option_group::plot, .kind = OptionKind::number, .name = "ZMAX", .once = true,
   .real = member([](auto &s) -> auto & { return s.plot_settings.z_3d[1]; }),
   .reset = [](Session &s) { s.plot_settings.z_3d[1] = box_side; }},
  {.group = option_group::plot, .kind = OptionKind::number, .name = "PHI", .once = true,
   .real = member([](auto &s) -> auto & { return s.drawing.phi0; })},
  {.group = option_group::plot, .kind = OptionKind::number, .name = "THETA", .once = true,
   .real = member([](auto &s) -> auto & { return s.drawing.theta0; })},
  {.group = option_group::plot, .kind = OptionKind::whole, .name = "LT", .once = true,
   .whole = member([](auto &s) -> auto & { return s.plot_settings.start_line_type; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     int i = 0;
     if (whole_in(v, i) || i >= 2 || i <= -6) return "not a line type from -5 to 1";
     if (!v.apply) return nullptr;
     s.plot_settings.start_line_type = i;
     reset_all_line_type(s);
     return nullptr;
   }},
  {.group = option_group::plot, .kind = OptionKind::whole, .name = "YNC", .once = true,
   .whole = member([](auto &s) -> auto & { return s.nullclines.y_null_color; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return color_into(s.nullclines.y_null_color, v);
   }},
  {.group = option_group::plot, .kind = OptionKind::whole, .name = "XNC", .once = true,
   .whole = member([](auto &s) -> auto & { return s.nullclines.x_null_color; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return color_into(s.nullclines.x_null_color, v);
   }},
  {.group = option_group::plot, .kind = OptionKind::whole, .name = "SMC", .once = true,
   .whole = member([](auto &s) -> auto & { return s.manifolds.stable_color; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return color_into(s.manifolds.stable_color, v);
   }},
  {.group = option_group::plot, .kind = OptionKind::whole, .name = "UMC", .once = true,
   .whole = member([](auto &s) -> auto & { return s.manifolds.unstable_color; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return color_into(s.manifolds.unstable_color, v);
   }},
  {.group = option_group::plot, .kind = OptionKind::whole, .name = "COLORMAP", .once = true,
   .whole = member([](auto &s) -> auto & { return s.colormap; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     int i = 0;
     if (whole_in(v, i) || i < 0 || i >= 7) return "not a colour map from 0 to 6";
     if (v.apply) s.colormap = i;
     return nullptr;
   }, .wide = true},
  {.group = option_group::export_, .kind = OptionKind::name, .name = "PLOTFMT", .once = true,
   .text = member([](auto &s) -> auto & { return s.plot_export.format; })},
  {.group = option_group::export_, .kind = OptionKind::name, .name = "PS_FONT", .once = true,
   .text = member([](auto &s) -> auto & { return s.plot_file.ps_font; })},
  {.group = option_group::export_, .kind = OptionKind::number, .name = "PS_LW", .once = true,
   .real = member([](auto &s) -> auto & { return s.plot_file.ps_lw; })},
  {.group = option_group::export_, .kind = OptionKind::whole, .name = "PS_FSIZE", .once = true,
   .whole = member([](auto &s) -> auto & { return s.plot_file.ps_font_size; })},
  {.group = option_group::export_, .kind = OptionKind::whole, .name = "PS_COLOR", .once = true,
   .whole = member([](auto &s) -> auto & { return s.plot_file.ps_color_flag; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (const char *why = whole_into(s.plot_file.ps_color_flag, v)) return why;
     if (v.apply) s.plot_export.color = s.plot_file.ps_color_flag;
     return nullptr;
   }},
  {.group = option_group::plot, .kind = OptionKind::whole, .name = "DFGRID", .once = true,
   .whole = member([](auto &s) -> auto & { return s.nullclines.df_grid; })},
  {.group = option_group::plot, .kind = OptionKind::whole, .name = "DFDRAW", .once = true,
   .whole = member([](auto &s) -> auto & { return s.nullclines.df_batch; })},
  {.group = option_group::plot, .kind = OptionKind::whole, .name = "NCDRAW", .once = true,
   .whole = member([](auto &s) -> auto & { return s.nullclines.nc_batch; })},
  {.group = option_group::plot, .kind = OptionKind::name, .name = "COLORVIA", .once = true,
   .text = member([](auto &s) -> auto & { return s.nullclines.color_via; })},
  {.group = option_group::plot, .kind = OptionKind::whole, .name = "COLORIZE", .once = true,
   .whole = member([](auto &s) -> auto & { return s.nullclines.colorize_flag; })},
  {.group = option_group::plot, .kind = OptionKind::number, .name = "COLORLO", .once = true,
   .real = member([](auto &s) -> auto & { return s.nullclines.color_via_lo; })},
  {.group = option_group::plot, .kind = OptionKind::number, .name = "COLORHI", .once = true,
   .real = member([](auto &s) -> auto & { return s.nullclines.color_via_hi; })},
  {.group = option_group::interface, .kind = OptionKind::sliders, .name = "SLIDERS", .once = true},
  /* XPPAUT slider slots are import-only. */
  {.kind = OptionKind::name, .name = "S1", .once = true,
   .parse = slider_var<0>},
  {.kind = OptionKind::name, .name = "S2", .once = true,
   .parse = slider_var<1>},
  {.kind = OptionKind::name, .name = "S3", .once = true,
   .parse = slider_var<2>},
  {.kind = OptionKind::number, .name = "SLO1", .once = true,
   .parse = slider_end<0, &XppSlider::lo>},
  {.kind = OptionKind::number, .name = "SLO2", .once = true,
   .parse = slider_end<1, &XppSlider::lo>},
  {.kind = OptionKind::number, .name = "SLO3", .once = true,
   .parse = slider_end<2, &XppSlider::lo>},
  {.kind = OptionKind::number, .name = "SHI1", .once = true,
   .parse = slider_end<0, &XppSlider::hi>},
  {.kind = OptionKind::number, .name = "SHI2", .once = true,
   .parse = slider_end<1, &XppSlider::hi>},
  {.kind = OptionKind::number, .name = "SHI3", .once = true,
   .parse = slider_end<2, &XppSlider::hi>},

  /* ---- the range integration and a batch run ---- */
  {.group = option_group::sweep, .kind = OptionKind::name, .name = "RANGEOVER", .once = true,
   .text = member([](auto &s) -> auto & { return s.integrator.range.item; }),
   /* the first variable */
   .reset = [](Session &s) { s.integrator.range.item = s.model().uvar_names[0]; }, .saved = true},
  {.group = option_group::sweep, .kind = OptionKind::whole, .name = "RANGESTEP", .once = true,
   .whole = member([](auto &s) -> auto & { return s.integrator.range.steps; }),
   /* twenty runs */
   .reset = [](Session &s) { s.integrator.range.steps = 20; }, .saved = true},
  {.group = option_group::sweep, .kind = OptionKind::number, .name = "RANGELOW", .once = true,
   .real = member([](auto &s) -> auto & { return s.integrator.range.plow; }),
   /* 0 to 1 */
   .reset = [](Session &s) { s.integrator.range.plow = s.integrator.range.plow2 = 0; }, .saved = true},
  {.group = option_group::sweep, .kind = OptionKind::number, .name = "RANGEHIGH", .once = true,
   .real = member([](auto &s) -> auto & { return s.integrator.range.phigh; }),
   .reset = [](Session &s) { s.integrator.range.phigh = s.integrator.range.phigh2 = 1; }, .saved = true},
  {.group = option_group::sweep, .kind = OptionKind::choice, .name = "RANGERESET", .once = true,
   .whole = member([](auto &s) -> auto & { return s.integrator.range.reset; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return yes_no_into(s.integrator.range.reset, v);
   },
   /* each run starts from the initial data */
   .reset = [](Session &s) { s.integrator.range.reset = 1; },
   .choices = yes_no_names, .saved = true},
  {.group = option_group::sweep, .kind = OptionKind::choice, .name = "RANGEOLDIC", .once = true,
   .whole = member([](auto &s) -> auto & { return s.integrator.range.oldic; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return yes_no_into(s.integrator.range.oldic, v);
   },
   /* the same initial data for each run */
   .reset = [](Session &s) { s.integrator.range.oldic = 1; },
   .choices = yes_no_names, .saved = true},
  {.group = option_group::sweep, .kind = OptionKind::choice, .name = "RANGEFILES",
   .xppaut_names = {"RANGE"}, .once = true,
   .boolean = member([](auto &s) -> auto & { return s.integrator.range.write_files; }),
   .rule = OptionRule::choice, .choices = yes_no_names, .saved = true},
  {.group = option_group::output, .kind = OptionKind::path, .name = "OUTPUT", .once = true,
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     /* the batch data file, named from the model's folder */
     std::string path;
     if (const char *why = own_file(s, v, path)) return why;
     if (v.apply) {
       s.batch_out_file = path;
       s.output_place = Load::running() ? Load::place() : command_place();
       if (s.output_place.source.empty())
         s.output_place.source = model_source_line(s.model(), s.output_place.file, s.output_place.line);
     }
     return nullptr;
   }},

  /* ---- AUTO ---- */
  {.group = option_group::continuation, .kind = OptionKind::whole, .name = "NTST", .once = true,
   .whole = member([](auto &s) -> auto & { return s.auto_state.options.ntst; }), .rule = OptionRule::whole_positive, .maximum = auto_max_mesh_intervals},
  {.group = option_group::continuation, .kind = OptionKind::whole, .name = "NMAX", .once = true,
   .whole = member([](auto &s) -> auto & { return s.auto_state.options.nmx; }), .rule = OptionRule::whole_positive, .maximum = static_cast<int>(snapx::auto_members::saved_points_limit)},
  {.group = option_group::continuation, .kind = OptionKind::whole, .name = "NPR", .once = true,
   .whole = member([](auto &s) -> auto & { return s.auto_state.options.npr; })},
  {.group = option_group::continuation, .kind = OptionKind::whole, .name = "NCOL", .once = true,
   .whole = member([](auto &s) -> auto & { return s.auto_state.options.ncol; }), .rule = OptionRule::whole_positive, .maximum = auto_max_collocation_points},
  {.group = option_group::continuation, .kind = OptionKind::number, .name = "DSMIN", .once = true,
   .real = member([](auto &s) -> auto & { return s.auto_state.options.dsmin; })},
  {.group = option_group::continuation, .kind = OptionKind::number, .name = "DSMAX", .once = true,
   .real = member([](auto &s) -> auto & { return s.auto_state.options.dsmax; })},
  {.group = option_group::continuation, .kind = OptionKind::number, .name = "DS", .once = true,
   .real = member([](auto &s) -> auto & { return s.auto_state.options.ds; })},
  {.group = option_group::continuation, .kind = OptionKind::number, .name = "PARMIN", .once = true,
   .real = member([](auto &s) -> auto & { return s.auto_state.options.rl0; })},
  {.group = option_group::continuation, .kind = OptionKind::number, .name = "PARMAX", .once = true,
   .real = member([](auto &s) -> auto & { return s.auto_state.options.rl1; })},
  {.group = option_group::continuation, .kind = OptionKind::number, .name = "NORMMIN", .once = true,
   .real = member([](auto &s) -> auto & { return s.auto_state.options.a0; })},
  {.group = option_group::continuation, .kind = OptionKind::number, .name = "NORMMAX", .once = true,
   .real = member([](auto &s) -> auto & { return s.auto_state.options.a1; })},
  {.group = option_group::continuation, .kind = OptionKind::number, .name = "EPSL", .once = true,
   .real = member([](auto &s) -> auto & { return s.auto_state.options.epsl; })},
  {.group = option_group::continuation, .kind = OptionKind::number, .name = "EPSU", .once = true,
   .real = member([](auto &s) -> auto & { return s.auto_state.options.epsu; })},
  {.group = option_group::continuation, .kind = OptionKind::number, .name = "EPSS", .once = true,
   .real = member([](auto &s) -> auto & { return s.auto_state.options.epss; })},
  {.group = option_group::diagram, .kind = OptionKind::number, .name = "AUTOXMAX", .once = true,
   .real = member([](auto &s) -> auto & { return s.auto_state.options.xmax; })},
  {.group = option_group::diagram, .kind = OptionKind::number, .name = "AUTOYMAX", .once = true,
   .real = member([](auto &s) -> auto & { return s.auto_state.options.ymax; })},
  {.group = option_group::diagram, .kind = OptionKind::number, .name = "AUTOXMIN", .once = true,
   .real = member([](auto &s) -> auto & { return s.auto_state.options.xmin; })},
  {.group = option_group::diagram, .kind = OptionKind::number, .name = "AUTOYMIN", .once = true,
   .real = member([](auto &s) -> auto & { return s.auto_state.options.ymin; })},
  {.group = option_group::diagram, .kind = OptionKind::name, .name = "AUTOVAR", .once = true,
   .whole = member([](auto &s) -> auto & { return s.auto_state.options.var; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     const int i = variable_of(s, v);
     if (i < 1) return "no such variable";
     if (v.apply) s.auto_state.options.var = i - 1;
     return nullptr;
   }},
  {.group = option_group::diagram, .kind = OptionKind::whole, .name = "SEC", .once = true,
   .whole = member([](auto &s) -> auto & { return s.auto_state.stable_eq_color; })},
  {.group = option_group::diagram, .kind = OptionKind::whole, .name = "UEC", .once = true,
   .whole = member([](auto &s) -> auto & { return s.auto_state.unstable_eq_color; })},
  {.group = option_group::diagram, .kind = OptionKind::whole, .name = "SPC", .once = true,
   .whole = member([](auto &s) -> auto & { return s.auto_state.stable_po_color; })},
  {.group = option_group::diagram, .kind = OptionKind::whole, .name = "UPC", .once = true,
   .whole = member([](auto &s) -> auto & { return s.auto_state.unstable_po_color; })},

  /* ---- the histogram and the spectrum a batch run writes ---- */
  {.group = option_group::analysis, .kind = OptionKind::whole, .name = "POSTPROCESS", .once = true,
   .whole = member([](auto &s) -> auto & { return s.histogram.post_process; })},
  {.group = option_group::analysis, .kind = OptionKind::number, .name = "HISTLO", .once = true,
   .real = member([](auto &s) -> auto & { return s.histogram.info.xlo; })},
  {.group = option_group::analysis, .kind = OptionKind::number, .name = "HISTHI", .once = true,
   .real = member([](auto &s) -> auto & { return s.histogram.info.xhi; })},
  {.group = option_group::analysis, .kind = OptionKind::whole, .name = "HISTBINS", .once = true,
   .whole = member([](auto &s) -> auto & { return s.histogram.info.nbins; }), .rule = OptionRule::whole_positive, .maximum = option_limit::histogram_bins},
  {.group = option_group::analysis, .kind = OptionKind::name, .name = "HISTCOL", .once = true,
   .whole = member([](auto &s) -> auto & { return s.histogram.info.col; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.histogram.info.col, v);
   }},
  {.group = option_group::analysis, .kind = OptionKind::number, .name = "HISTLO2", .once = true,
   .real = member([](auto &s) -> auto & { return s.histogram.info.ylo; })},
  {.group = option_group::analysis, .kind = OptionKind::number, .name = "HISTHI2", .once = true,
   .real = member([](auto &s) -> auto & { return s.histogram.info.yhi; })},
  {.group = option_group::analysis, .kind = OptionKind::whole, .name = "HISTBINS2", .once = true,
   .whole = member([](auto &s) -> auto & { return s.histogram.info.nbins2; }), .rule = OptionRule::whole_positive, .maximum = option_limit::histogram_bins},
  {.group = option_group::analysis, .kind = OptionKind::name, .name = "HISTCOL2", .once = true,
   .whole = member([](auto &s) -> auto & { return s.histogram.info.col2; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.histogram.info.col2, v);
   }},
  {.group = option_group::analysis, .kind = OptionKind::name, .name = "SPECCOL", .once = true,
   .whole = member([](auto &s) -> auto & { return s.histogram.spec_col; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.histogram.spec_col, v);
   }},
  {.group = option_group::analysis, .kind = OptionKind::name, .name = "SPECCOL2", .once = true,
   .whole = member([](auto &s) -> auto & { return s.histogram.spec_col2; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.histogram.spec_col2, v);
   }},
  {.group = option_group::analysis, .kind = OptionKind::whole, .name = "SPECWIDTH", .once = true,
   .whole = member([](auto &s) -> auto & { return s.histogram.spec_wid; }), .rule = OptionRule::whole_positive, .maximum = option_limit::spectrum_samples},
  {.group = option_group::analysis, .kind = OptionKind::whole, .name = "SPECWIN", .once = true,
   .whole = member([](auto &s) -> auto & { return s.histogram.spec_win; })},

  /* ---- the log, the user's buttons ---- */
  {.group = option_group::output, .kind = OptionKind::whole, .name = "QUIET",
   .parse = [](Session &, const OptionValue &v) -> const char * {
     if (v.apply && !log_settings.quiet_from_command_line) log_settings.verbose = v.text[0] == '0';
     return nullptr;
   },
   .zero_or_one = true},
  {.group = option_group::output, .kind = OptionKind::path, .name = "LOGFILE",
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (log_settings.file_from_command_line) return nullptr;
     /* named on the command line it is the user's own, as typed */
     std::string path = v.text;
     if (!v.source.force)
       if (const char *why = own_file(s, v, path)) return why;
     if (v.apply && !log_open_file(path)) return log_open_failed;
     return nullptr;
   }},
  {.group = option_group::interface, .kind = OptionKind::text, .name = "BUT",
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (v.apply) add_user_button(s, v.text);
     return nullptr;
   },
   .repeatable = true},
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

/* a row's place in the table */
static std::size_t row_index(const OptionRow &row) { return static_cast<std::size_t>(&row - rows); }

bool OptionsSet::has(const OptionRow &row) const
{
  const std::size_t i = row_index(row);
  return i < bits_.size() && bits_[i];
}

void OptionsSet::mark(const OptionRow &row)
{
  const std::size_t i = row_index(row);
  if (i >= bits_.size()) bits_.resize(std::size(rows), false);
  bits_[i] = true;
}

bool OptionSource::may(const Session &s, const OptionRow &row) const
{
  return force || !s.options_set.has(row) || (mask && !mask->has(row));
}

bool OptionSource::claim(Session &s, const OptionRow &row) const
{
  if (!may(s, row)) return false;
  s.options_set.mark(row);
  return true;
}

const OptionRow &once_option(std::string_view name)
{
  return *std::ranges::find_if(rows, [name](const OptionRow &r) { return r.once && r.name == name; });
}

const char *rule_problem(OptionRule rule, double v, int maximum)
{
  if (maximum && v > maximum) return "exceeds the allocation maximum";
  switch (rule) {
  case OptionRule::nonzero: return v != 0 ? nullptr : "must be a number other than 0";
  case OptionRule::positive: return v > 0 ? nullptr : "must be a number above 0";
  case OptionRule::nonnegative: return v >= 0 ? nullptr : "must be a number of at least 0";
  case OptionRule::whole_positive:
    return v == std::floor(v) && v >= 1 && v <= INT_MAX ? nullptr : "must be a whole number of at least 1";
  case OptionRule::any:
  case OptionRule::method:
  case OptionRule::choice:
    break;
  }
  return nullptr;
}

int choice_index(std::span<const std::string_view> choices, std::string_view text)
{
  const std::string lower = lower_case(std::string(text));
  int i = 0;
  for (std::size_t k = 0; k < choices.size(); ++k)
    if (choices[k] == lower) return static_cast<int>(k);
  if (parse_int(text, i) && i >= 0 && static_cast<std::size_t>(i) < choices.size()) return i;
  return -1;
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

/* a row's .odex key: its name in lower case, the digit of a numbered row */
std::string odex_key(const OptionRow &row, int index)
{
  return lower_case(std::string(row.name)) + (row.first_digit ? std::string(1, static_cast<char>('0' + index)) : std::string());
}

std::string odex_option_key(std::string_view key)
{
  int index = 0;
  const OptionRow *row = find_option(upper_case(std::string(key)), index, true);
  return row && !row->group.empty() ? odex_key(*row, index) : std::string();
}

bool set_holds(const OptionRow &row)
{
  return !row.wide && std::ranges::find(set_groups, row.group) != set_groups.end();
}

const OptionRow *find_odex_option(std::string_view key, int &index)
{
  for (const OptionRow &row : rows) {
    if (row.group.empty()) continue;
    const std::string name = lower_case(std::string(row.name));
    if (!row.first_digit) {
      if (key != name) continue;
      index = 0;
      return &row;
    }
    if (key.size() != name.size() + 1 || !key.starts_with(name)) continue;
    const char d = key.back();
    if (d < row.first_digit || d > row.last_digit) continue;
    index = d - '0';
    return &row;
  }
  return nullptr;
}

const OptionRow &slider_presets_option()
{
  return *std::ranges::find(rows, OptionKind::sliders, &OptionRow::kind);
}

bool xppaut_slider_option(const OptionRow &row)
{
  return std::ranges::contains(slider_slot_parsers, row.parse);
}

std::vector<std::string> option_choices(const OptionRow &row)
{
  std::vector<std::string> words;
  if (row.rule == OptionRule::method) {
    for (const SolverInfo &info : solvers()) words.emplace_back(info.key);
    return words;
  }
  for (std::string_view w : row.choices) words.emplace_back(w);
  return words;
}

std::string option_choice(const OptionRow &row, std::string_view value)
{
  const std::vector<std::string> words = option_choices(row);
  if (row.rule == OptionRule::choice) {
    const int i = choice_index(row.choices, value);
    return i < 0 ? std::string() : words[static_cast<std::size_t>(i)];
  }
  if (value.empty()) return {};
  for (const std::string &w : words)
    if (lower_case(std::string(1, w[0])) == lower_case(std::string(1, value[0]))) return w;
  return {};
}

const OptionRow *numerics_option(std::string_view key)
{
  const auto it = std::ranges::find_if(rows, [key](const OptionRow &r) { return !r.key.empty() && r.key == key; });
  return it == std::ranges::end(rows) ? nullptr : &*it;
}

std::optional<double> imported_total(const OptionRow &row, std::string_view value)
{
  double total = 0;
  if (row.key != "total_time" || !parse_number(value, total) || total >= 0) return std::nullopt;
  return -total;
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
  int index = 0;
  const OptionRow *row = find_option(upper, index, xppaut_names);
  if (!row) return unknown_option;
  const bool importing = Load::running() && files::has_extension(Load::place().file, ".ode");
  const std::optional<double> total = importing ? imported_total(*row, value) : std::nullopt;
  /* the value as a NUL-ended text */
  const std::string text = total ? number(*total) : std::string(value);
  const OptionSource source{force, mask};
  const OptionValue v{text.c_str(), index, source, apply};
  const char *why = nullptr;
  /* Check the row's shared numeric bounds before its parser can apply a value. */
  double x = 0;
  const char *refused = parse_number(text, x) ? rule_problem(row->rule, x, row->maximum) : nullptr;
  if (row->zero_or_one && text != "0" && text != "1") why = "not 0 or 1";
  else if (row->once && !source.may(s, *row)) return std::nullopt;
  else if (refused) why = refused;
  else if (row->rule == OptionRule::method) {
    const auto picked = pick_method(s.model(), text, Load::running() ? Load::place() : command_place());
    if (!picked) return picked.error().what;
    if (apply) s.numerics.method = *picked;
  }
  else if (row->kind == OptionKind::sliders) {
    const auto sliders = setx::read_block("options", s.model().this_file, "sliders: " + text, false,
      [&](setx::Block &block) { return read_sliders(s.model(), block); });
    if (!sliders) return sliders.error().what;
    if (apply) s.sliders = *sliders;
  }
  else if (row->parse) {
    why = row->parse(s, v);
    if (why == log_open_failed) return files::write_error("option", value).what;
  }
  else if (row->boolean) {
    /* Numeric choices belong to a foreign .ode import, never our own files. */
    const std::string named = importing ? option_choice(*row, text) : lower_case(text);
    const auto picked = std::ranges::find(row->choices, named);
    if (picked == row->choices.end()) why = "not yes or no";
    else if (apply) row->boolean(s) = picked != row->choices.begin();
  }
  else if (row->real) why = number_into(row->real(s), v);
  else if (row->whole) why = whole_into(row->whole(s), v);
  else if (row->text && apply) row->text(s) = text;
  if (!why && apply && row->once) s.options_set.mark(*row);
  return why ? std::optional<std::string>(why) : std::nullopt;
}

} // namespace

std::vector<XppSlider> read_sliders(const Model &m, xpp::setx::Block &file, bool check_names)
{
    std::vector<xpp::setx::Block> items;
    file.groups("sliders", items, odex::max_file_tokens);
    std::vector<XppSlider> sliders;
    for (xpp::setx::Block &g : items) {
        xpp::XppSlider slider;
        for (std::string_view key : {"name", "low", "high"})
            if (!g.has(key)) g.fail(xpp::format("slider preset requires {}", key));
        g.text("name", slider.var);
        if (const auto why = xpp::slider_wrong(m, slider, check_names)) g.fail(*g.member("name"), *why);
        g.number("low", slider.lo);
        g.number("high", slider.hi);
        if (const auto why = xpp::slider_wrong(m, slider, check_names)) g.fail(*g.member("high"), *why);
        g.number("step", slider.step);
        if (const auto why = xpp::slider_wrong(m, slider, check_names)) g.fail(*g.member("step"), *why);
        g.done();
        sliders.push_back(std::move(slider));
    }
    return sliders;
}


std::string option_error_text(std::string_view name, std::string_view value, std::string_view why)
{
  return xpp::format("the option `{}={}`: {}", name, value, why);
}

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
  const std::string what = option_error_text(name, value, *why);
  if (Load::running()) model_failed(what);
  const Error e{"options", what, command_place()};
  show_error(e);
}

void set_option_defaults(Session &s)
{
  for (const OptionRow &row : rows) {
    if (!row.reset || (row.once && s.options_set.has(row))) continue;
    row.reset(s);
    if (row.once) s.options_set.mark(row);
  }
}

} // namespace xpp
