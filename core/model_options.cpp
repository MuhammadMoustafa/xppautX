#include "xpp_files.h"
/* The model's options as one table (model_options.h, W119). */
#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <string>
#include <string_view>

#include "model_options.h"
#include "image_format.h"
#include "odex_block.h"
#include "odex.h"
#include "xpp_util.h"
#include "autox_settings.h"
#include "xpp_session_autox.h"
#include "browse.h"
#include "data_formats.h"
#include "colormap.h"
#include "expr_native.h"
#include "graphics.h"
#include "histogram.h"
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

/* where XPPAUT's ncdraw=2 saves the nullclines */
constexpr const char *nullclines_dump_file = "nullclines.dat";

/* XPPAUT's most curves in one window (xp2 to xp8 and the first) */
constexpr int max_plot_curves = 8;
/* what a .ode's quiet and logfile become: a warning each */
constexpr std::string_view log_level_is_the_users = "what the log shows is the user's, not a model's (the preferences' log_level, --verbose and --quiet), so the option is dropped";
constexpr std::string_view log_file_is_the_users = "where the log goes is the user's, not a model's (the preferences' log_file and --logfile), so the option is dropped";
/* what a .ode's several starting graphs (multiwin, simplot, xlo2 to yhi8) become: one warning */
constexpr std::string_view several_graphs = "xppautX opens one graph, so curves 2 to N join graph_1 and the options of several starting graphs are dropped: several graphs are a session's (window.create)";

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

/* style as the line every new curve starts with (the curves already there start again with it) */
const char *start_line_style_into(Session &s, const LineStyle &style, const OptionValue &v)
{
  if (v.apply) {
    s.plot_settings.start_line_style = style;
    reset_all_line_type(s);
  }
  return nullptr;
}

/* the variable v names (0 the time), -1 for none */
int variable_of(const Session &s, const OptionValue &v)
{
  int i = -1;
  find_variable(s, v.text, &i);
  return i;
}

/* a colour option's value: our own colour words, or on import XPPAUT's number in the row's own way (foreign);
   refused (why, kept alive in the caller) when it is none */
const char *color_into(Color &member, const OptionRow &row, const OptionValue &v, bool foreign, std::string &why)
{
  std::optional<Color> c = parse_color(v.text);
  if (!c && foreign && row.foreign) c = parse_color(row.foreign(v.text));
  if (!c) {
    why = "not a colour: `#rrggbb`, `#rrggbbaa`, a CSS colour name or foreground";
    return why.c_str();
  }
  if (v.apply) member = *c;
  return nullptr;
}

/* XPPAUT's colour number as our colour's word: 0..10 */
std::string foreign_plot_color(std::string_view value)
{
  int i = 0;
  if (!parse_int(value, i) || i < 0 || i > LAST_PLOT_COLOR) return {};
  return print_color(xppaut_color(i));
}

/* XPPAUT's AUTO colour number as our colour's word: 0 or the slots 20 to 29 of its named colours */
std::string foreign_autox_color(std::string_view value)
{
  int i = 0;
  if (!parse_int(value, i) || (i != 0 && (i < 20 || i > 19 + LAST_PLOT_COLOR))) return {};
  return print_color(xppaut_color(i == 0 ? 0 : i - 19));
}

const char *variable_into(const Session &s, int &member, const OptionValue &v)
{
  const int i = variable_of(s, v);
  if (i < 0) return "no such variable";
  if (v.apply) member = i;
  return nullptr;
}

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

/* a data file a path option names (own_file's rule), its extension picking the data format: into target */
const char *data_file_into(Session &s, const OptionValue &v, std::string &target)
{
  std::string path;
  if (const char *why = own_file(s, v, path)) return why;
  if (!data_format_of_file(path)) return "must end in the extension of a data format (.dat, .csv, .csv.gz, .npz)";
  if (v.apply) target = path;
  return nullptr;
}

/* the words a .odex writes for the choice rows (option_choices); the
   Jacobian's are jacobian.h's, the method's the registry's */
constexpr std::array<std::string_view, 2> boolean_names{"false", "true"};
constexpr std::array<std::string_view, 2> axes_names{"2", "3"};
/* autoX's output detail levels, as the words the row takes (autox_settings.h's names) */
constexpr std::array<std::string_view, autox_output_detail_names.size()> autox_output_detail_words = [] {
  std::array<std::string_view, autox_output_detail_names.size()> words;
  for (std::size_t i = 0; i < words.size(); ++i) words[i] = autox_output_detail_names[i];
  return words;
}();
constexpr std::array<std::string_view, 4> poincare_map_names{"none", "section", "max", "period"};
/* XPPAUT's dfdraw 0 to 5, in its order: what a batch run draws of the direction field, and whether it also saves it */
constexpr std::array<std::string_view, 6> direction_field_draw_names{"none", "arrows", "scaled_arrows", "flow", "arrows_and_save", "scaled_arrows_and_save"};
/* XPPAUT's postprocess 0 to 7, in its order (read on import only; ours is histogram.h's after_run_names) */
constexpr std::array<std::string_view, 8> xppaut_postprocess_words{"none", "histogram", "fourier_series", "power_spectrum", spectrum_type_names[0], spectrum_type_names[1], spectrum_type_names[2], "histogram_2d"};

/* the word of words that value starts with, "" when none */
template <class Words> std::string first_letter_word(const Words &words, std::string_view value)
{
  if (value.empty()) return {};
  for (const auto &w : words)
    if (std::tolower(static_cast<unsigned char>(std::string_view(w)[0])) == std::tolower(static_cast<unsigned char>(value[0]))) return std::string(w);
  return {};
}

/* XPPAUT's yes or no as a boolean's word: y... or 1 (or t...), n... or 0 (or f...) */
std::string foreign_boolean(std::string_view value)
{
  if (value.empty()) return {};
  switch (std::tolower(static_cast<unsigned char>(value[0]))) {
  case 'y': case 't': case '1': return "true";
  case 'n': case 'f': case '0': return "false";
  default: return {};
  }
}

/* the word of words that value, a number from 0, counts to (XPPAUT's numbered choices), "" when none */
template <class Words> std::string numbered_word(const Words &words, std::string_view value)
{
  int i = 0;
  if (!parse_int(value, i) || i < 0 || static_cast<std::size_t>(i) >= words.size()) return {};
  return std::string(words[static_cast<std::size_t>(i)]);
}

/* XPPAUT's Poincare sign, a number whose sign says which crossings */
std::string foreign_section_sign(std::string_view value)
{
  double x = 0;
  if (!parse_number(value, x)) return {};
  return std::string(section_sign_names[static_cast<std::size_t>(static_cast<int>(section_sign(x)) + 1)]);
}

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
  {.group = option_group::integration, .kind = OptionKind::number, .odex = "total_time", .help = "How long a run integrates, in model time.", .xppaut_names = {"TOTAL"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.total_time; }),
   /* twenty time units, XPPAUT's */
   .reset = [](Session &s) { s.numerics.total_time = 20; },
   .key = "total_time", .label = "Total time", .rule = OptionRule::nonnegative, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::number, .odex = "start_time", .help = "The time a run starts at.", .xppaut_names = {"T0"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.start_time; }),
   /* time starts at 0 */
   .reset = [](Session &s) { s.numerics.start_time = 0; },
   .key = "start_time", .label = "Start time", .saved = true},
  {.group = option_group::integration, .kind = OptionKind::number, .odex = "dt", .help = "The time step of a fixed-step method, and the first step of an adaptive one.", .xppaut_names = {"DT"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.delta_t; }),
   /* twenty steps per time unit, XPPAUT's */
   .reset = [](Session &s) { s.numerics.delta_t = .05; },
   .key = "dt", .label = "Dt", .rule = OptionRule::nonzero, .saved = true, .applied = dt_changed},
  {.group = option_group::integration, .kind = OptionKind::whole, .odex = "store_every", .help = "Keep one step of every this many in the data table.", .xppaut_names = {"NOUT", "NJMP"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.numerics.store_every; }),
   /* every step stored */
   .reset = [](Session &s) { s.numerics.store_every = 1; },
   .key = "store_every", .label = "Store every N steps", .rule = OptionRule::whole_positive, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::number, .odex = "store_from", .help = "Keep no data before this time; the run before it is a transient.", .xppaut_names = {"TRANS"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.store_from; }),
   /* store from the start */
   .reset = [](Session &s) { s.numerics.store_from = 0; },
   .key = "store_from", .label = "Store from", .saved = true},
  {.group = option_group::integration, .kind = OptionKind::choice, .odex = "storage_precision", .help = "What the data table keeps each value as: a single-precision float as XPPAUT does, or the integrator's double.", .once = true,
   .index = choice_member([](auto &s) -> auto & { return s.numerics.storage_precision; }),
   /* single: XPPAUT's float, half the memory */
   .reset = [](Session &s) { s.numerics.storage_precision = StoragePrecision::single; },
   .key = "storage_precision", .label = "Storage precision", .rule = OptionRule::choice, .choices = storage_precision_names, .saved = true, .applied = storage_precision_changed},
  {.group = option_group::integration, .kind = OptionKind::number, .odex = "state_limit", .help = "A state beyond plus or minus this value stops the run.", .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.state_limit; }),
   /* XPPAUT's: past it a solution is taken to blow up */
   .reset = [](Session &s) { s.numerics.state_limit = 100; },
   .key = "state_limit", .label = "State limit", .rule = OptionRule::positive, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::choice, .odex = "stop_at_limit", .help = "Whether a state beyond the state limit stops the run, or only marks it.", .once = true,
   .boolean = member([](auto &s) -> auto & { return s.numerics.stop_at_limit; }),
   /* XPPAUT's: a solution past the limit ends the run */
   .reset = [](Session &s) { s.numerics.stop_at_limit = true; },
   .key = "stop_at_limit", .label = "Stop at the limit", .rule = OptionRule::choice, .choices = boolean_names, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::choice, .odex = "method", .help = "The integration method.", .xppaut_names = {"METH", "METHOD"}, .once = true,
   .index = choice_member([](auto &s) -> auto & { return s.numerics.method; }),
   /* Integral equations require Volterra; otherwise the usual RK4 default. */
   .reset = [](Session &s) {
     s.numerics.method = pick_method(s.model(), s.model().nkernel > 0 ? "Volterra" : "Runge-Kutta", command_place()).value();
   },
   .key = "method", .label = "Method", .rule = OptionRule::method, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::choice, .odex = "subsystem_split", .help = "What splits the model into independent subsystems: its equations alone, or also the run (a Poincare section or stop).", .once = true,
   .index = choice_member([](auto &s) -> auto & { return s.numerics.subsystem_split; }),
   /* the equations alone: a Poincare section or stop does not join the states */
   .reset = [](Session &s) { s.numerics.subsystem_split = SubsystemSplit::equations; },
   .key = "subsystem_split", .label = "Subsystem split", .rule = OptionRule::choice, .choices = subsystem_split_names, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::whole, .odex = "seed", .help = "The seed of the random numbers a run draws; the same seed gives the same run.", .xppaut_names = {"SEED"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.numerics.rand_seed; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     int i = 0;
     if (whole_in(v, i) || i < 0) return "not a seed (a whole number of at least 0)";
     if (!v.apply) return nullptr;
     s.numerics.rand_seed = i;
     s.random.seed(s.numerics.rand_seed);
     return nullptr;
   }},
  {.group = option_group::integration, .kind = OptionKind::number, .odex = "step.relative_tolerance", .help = "The relative error an adaptive method accepts in one step.", .xppaut_names = {"TOL"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.relative_tolerance; }),
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.relative_tolerance = .001; },
   .key = "relative_tolerance", .label = "Relative tolerance", .rule = OptionRule::positive, .use = OptionUse::relative_tolerance, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::number, .odex = "step.absolute_tolerance", .help = "The absolute error an adaptive method accepts in one step.", .xppaut_names = {"ATOL"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.absolute_tolerance; }),
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.absolute_tolerance = .001; },
   .key = "absolute_tolerance", .label = "Absolute tolerance", .rule = OptionRule::positive, .use = OptionUse::absolute_tolerance, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::number, .odex = "step.min", .help = "The smallest step an adaptive method takes before it gives up.", .xppaut_names = {"DTMIN"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.min_step; }),
   /* as small as an adaptive method may need */
   .reset = [](Session &s) { s.numerics.min_step = 1e-12; },
   .key = "min_step", .label = "Minimum step", .rule = OptionRule::positive, .use = OptionUse::min_step, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::number, .odex = "step.max", .help = "The largest step an adaptive method takes.", .xppaut_names = {"DTMAX"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.max_step; }),
   /* one time unit */
   .reset = [](Session &s) { s.numerics.max_step = 1; },
   .key = "max_step", .label = "Maximum step", .rule = OptionRule::positive, .use = OptionUse::max_step, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::choice, .odex = "jacobian.kind", .help = "Whether an implicit method gets the Jacobian by differences or from the exact derivatives.", .once = true,
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
  {.group = option_group::integration, .kind = OptionKind::whole, .odex = "jacobian.band_upper", .help = "The diagonals above the main one that a banded Jacobian keeps.", .xppaut_names = {"BANDUP"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.numerics.cv_bandupper; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (const char *why = whole_into(s.numerics.cv_bandupper, v)) return why;
     if (v.apply) s.numerics.cv_bandflag=true;
     return nullptr;
   }, .rule = OptionRule::nonnegative, .maximum = option_limit::bandwidth},
  {.group = option_group::integration, .kind = OptionKind::whole, .odex = "jacobian.band_lower", .help = "The diagonals below the main one that a banded Jacobian keeps.", .xppaut_names = {"BANDLO"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.numerics.cv_bandlower; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (const char *why = whole_into(s.numerics.cv_bandlower, v)) return why;
     if (v.apply) s.numerics.cv_bandflag=true;
     return nullptr;
   }, .rule = OptionRule::nonnegative, .maximum = option_limit::bandwidth},
  {.group = option_group::integration, .kind = OptionKind::number, .odex = "delay.max_delay", .help = "The longest delay a model uses; it sets how much history is kept.", .xppaut_names = {"DELAY"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.delay; }),
   /* no delays kept */
   .reset = [](Session &s) { s.numerics.delay = 0; },
   .key = "delay", .label = "Maximal delay", .rule = OptionRule::nonnegative, .use = OptionUse::delays, .saved = true, .applied = chk_delay},
  {.group = option_group::integration, .kind = OptionKind::whole, .odex = "delay.history_points", .help = "How many past points of the history a delay model keeps.", .xppaut_names = {"VMAXPTS"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.numerics.max_points; }), .rule = OptionRule::whole_positive, .maximum = option_limit::history_points, .use = OptionUse::integral_history, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::number, .odex = "delay.real_guess", .help = "The largest real part the search for a delay equilibrium's eigenvalues looks for.", .once = true,
   .real = member([](auto &s) -> auto & { return s.delay.alpha_max; }), .use = OptionUse::delays},
  {.group = option_group::integration, .kind = OptionKind::number, .odex = "delay.imaginary_guess", .help = "The largest imaginary part the search for a delay equilibrium's eigenvalues looks for.", .once = true,
   .real = member([](auto &s) -> auto & { return s.delay.omega_max; }), .use = OptionUse::delays},
  {.group = option_group::integration, .kind = OptionKind::whole, .odex = "delay.grid", .help = "How many points a side the grid of a delay equilibrium's eigenvalue search has.", .once = true,
   .whole = member([](auto &s) -> auto & { return s.delay.grid; }), .rule = OptionRule::whole_positive, .use = OptionUse::delays},
  {.group = option_group::integration, .kind = OptionKind::number, .odex = "euler.tolerance", .help = "The Newton tolerance of the implicit Euler method.", .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.eul_tol; }),
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.eul_tol = 1e-7; },
   .key = "eul_tol", .label = "Newton tolerance", .rule = OptionRule::positive, .use = OptionUse::newton, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::whole, .odex = "euler.iterations", .help = "The most Newton iterations the implicit Euler method makes in a step.", .once = true,
   .whole = member([](auto &s) -> auto & { return s.numerics.max_eul_iter; }),
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.max_eul_iter = 10; },
   .key = "eul_iter", .label = "Newton iterations", .rule = OptionRule::whole_positive, .use = OptionUse::newton, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::whole, .odex = "bvp.max_iterates", .help = "The most Newton iterates a boundary value solve makes.", .once = true,
   .whole = member([](auto &s) -> auto & { return s.numerics.bvp_max_iterates; }),
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.bvp_max_iterates = 20; },
   .key = "bvp_max_iterates", .label = "BVP maximum iterates", .rule = OptionRule::whole_positive, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::number, .odex = "bvp.tolerance", .help = "The tolerance a boundary value solve stops at.", .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.bvp_tolerance; }),
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.bvp_tolerance = 1e-5; },
   .key = "bvp_tolerance", .label = "BVP tolerance", .rule = OptionRule::positive, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::number, .odex = "bvp.epsilon", .help = "The step that differences the boundary value problem's Jacobian.", .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.bvp_epsilon; }),
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.bvp_epsilon = 1e-5; },
   .key = "bvp_epsilon", .label = "BVP epsilon", .rule = OptionRule::positive, .saved = true},
  {.group = option_group::integration, .kind = OptionKind::number, .odex = "torus.period", .help = "The period of the variables that live on a circle or torus.", .xppaut_names = {"TOR_PER"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.torus_period; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (const char *why = number_into(s.numerics.torus_period, v)) return why;
     if (v.apply) s.numerics.torus=true;
     return nullptr;
   },
   .rule = OptionRule::positive},
  {.group = option_group::integration, .kind = OptionKind::name, .odex = "torus.fold", .help = "A variable that is taken modulo the torus period; each names one.",.xppaut_names = {"FOLD"}, 
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     const int i = variable_of(s, v);
     if (i < 1) return "no such variable";
     if (!v.apply) return nullptr;
     s.itor[i - 1] = 1;
     s.numerics.torus=true;
     return nullptr;
   },
   .repeatable = true},
  {.group = option_group::poincare, .kind = OptionKind::choice, .odex = "map", .help = "What a run records at a Poincare section: nothing, the section, the maxima or the period.", .xppaut_names = {"POIMAP"}, .once = true,
   .index = choice_member([](auto &s) -> auto & { return s.numerics.poincare_map; }),
   /* no Poincare map */
   .reset = [](Session &s) { s.numerics.poincare_map = PoincareMap::none; },
   .foreign = [](std::string_view v) { return first_letter_word(poincare_map_names, v); },
   .rule = OptionRule::choice, .choices = poincare_map_names, .saved = true},
  {.group = option_group::poincare, .kind = OptionKind::name, .odex = "variable", .help = "The variable the Poincare section is taken on.", .xppaut_names = {"POIVAR"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.numerics.poincare_variable; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.numerics.poincare_variable, v);
   },
   /* the first variable */
   .reset = [](Session &s) { s.numerics.poincare_variable = 1; }, .saved = true},
  {.group = option_group::poincare, .kind = OptionKind::choice, .odex = "sign", .help = "Which crossings of the section count: upward, downward or both.", .xppaut_names = {"POISGN"}, .once = true,
   .index = choice_member<1>([](auto &s) -> auto & { return s.numerics.poincare_sign; }),
   /* crossings upwards */
   .reset = [](Session &s) { s.numerics.poincare_sign = SectionSign::positive; },
   .foreign = foreign_section_sign,
   .rule = OptionRule::choice, .choices = section_sign_names, .saved = true},
  {.group = option_group::poincare, .kind = OptionKind::number, .odex = "plane", .help = "The value of the variable at the section.", .xppaut_names = {"POIPLN"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.poincare_plane; }),
   /* the section at 0 */
   .reset = [](Session &s) { s.numerics.poincare_plane = 0; }, .saved = true},
  {.group = option_group::poincare, .kind = OptionKind::choice, .odex = "stop", .help = "Whether a crossing of the section stops the run.", .xppaut_names = {"POISTOP"}, .once = true,
   .boolean = member([](auto &s) -> auto & { return s.numerics.poincare_stop; }),
   /* a section does not stop the run */
   .reset = [](Session &s) { s.numerics.poincare_stop = false; },
   .rule = OptionRule::choice, .choices = boolean_names, .saved = true},
  {.group = option_group::equilibria, .kind = OptionKind::whole, .odex = "max_iterations", .help = "The most Newton iterations the search for an equilibrium makes.", .xppaut_names = {"NEWT_ITER"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.numerics.equilibria.max_iterations; }),
   /* Newton's method converges in far fewer or not at all */
   .reset = [](Session &s) { s.numerics.equilibria.max_iterations = 100; },
   .key = "max_iterations", .label = "Equilibria: maximum iterations", .rule = OptionRule::whole_positive, .saved = true},
  {.group = option_group::equilibria, .kind = OptionKind::number, .odex = "newton_tolerance", .help = "The tolerance the search for an equilibrium stops at.", .xppaut_names = {"NEWT_TOL"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.equilibria.newton_tolerance; }),
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.equilibria.newton_tolerance = .001; },
   .key = "newton_tolerance", .label = "Equilibria: Newton tolerance", .rule = OptionRule::positive, .saved = true},
  {.group = option_group::equilibria, .kind = OptionKind::number, .odex = "jacobian_step", .help = "The step that differences the Jacobian of the equilibrium search.", .xppaut_names = {"JAC_EPS"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.equilibria.jacobian_step; }),
   /* XPPAUT's */
   .reset = [](Session &s) { s.numerics.equilibria.jacobian_step = .001; },
   .key = "jacobian_step", .label = "Equilibria: Jacobian step", .rule = OptionRule::positive, .saved = true},
  {.group = option_group::equilibria, .kind = OptionKind::number, .odex = "search_limit", .help = "A state beyond plus or minus this value ends the equilibrium search.", .once = true,
   .real = member([](auto &s) -> auto & { return s.numerics.equilibria.search_limit; }),
   /* XPPAUT's bound: the search gives up where a variable passes it */
   .reset = [](Session &s) { s.numerics.equilibria.search_limit = 100; },
   .key = "search_limit", .label = "Equilibria: search limit", .rule = OptionRule::positive, .saved = true},
  {.group = option_group::equilibria, .kind = OptionKind::choice, .odex = "print_eigenvalues", .help = "Whether the eigenvalues of an equilibrium are printed.", .once = true,
   .boolean = member([](auto &s) -> auto & { return s.numerics.equilibria.print_eigenvalues; }),
   .reset = [](Session &s) { s.numerics.equilibria.print_eigenvalues = false; },
   .key = "print_eigenvalues", .label = "Equilibria: print eigenvalues", .rule = OptionRule::choice, .choices = boolean_names, .saved = true},
  {.group = option_group::equilibria, .kind = OptionKind::choice, .odex = "draw_invariant_sets", .help = "Whether the stable and unstable sets of a saddle are drawn.", .once = true,
   .boolean = member([](auto &s) -> auto & { return s.numerics.equilibria.draw_invariant_sets; }),
   .reset = [](Session &s) { s.numerics.equilibria.draw_invariant_sets = false; },
   .key = "draw_invariant_sets", .label = "Equilibria: draw invariant sets", .rule = OptionRule::choice, .choices = boolean_names, .saved = true},
  {.group = option_group::equilibria, .kind = OptionKind::choice, .odex = "draw_strong_sets", .help = "Whether the strong stable and unstable sets are drawn.", .once = true,
   .boolean = member([](auto &s) -> auto & { return s.numerics.equilibria.draw_strong_sets; }),
   .reset = [](Session &s) { s.numerics.equilibria.draw_strong_sets = false; },
   .key = "draw_strong_sets", .label = "Equilibria: draw strong sets", .rule = OptionRule::choice, .choices = boolean_names, .saved = true},
  /* ---- the other numerics ---- */
  /* near()'s tolerance: the .odex reader takes it into the formulas that
     call near (odex_load.cpp scan_neartol); nothing to set at run time */
  {.group = option_group::equilibria, .odex = "near_tolerance", .help = "How close two values must be for the model's near function to call them equal.", .rule = OptionRule::positive},
  /* ---- the range integration and a batch run ---- */
  {.group = option_group::sweep, .kind = OptionKind::name, .odex = "variable", .help = "The parameter or variable a sweep varies.", .xppaut_names = {"RANGEOVER"}, .once = true,
   .text = member([](auto &s) -> auto & { return s.integrator.range.item; }),
   /* the first variable */
   .reset = [](Session &s) { s.integrator.range.item = s.model().uvar_names[0]; }, .saved = true},
  {.group = option_group::sweep, .kind = OptionKind::whole, .odex = "steps", .help = "How many steps the sweep makes between its ends.", .xppaut_names = {"RANGESTEP"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.integrator.range.steps; }),
   /* twenty runs */
   .reset = [](Session &s) { s.integrator.range.steps = 20; }, .rule = OptionRule::whole_positive, .saved = true},
  {.group = option_group::sweep, .kind = OptionKind::number, .odex = "low", .help = "The value the sweep starts at.", .xppaut_names = {"RANGELOW"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.integrator.range.plow; }),
   /* 0 to 1 */
   .reset = [](Session &s) { s.integrator.range.plow = s.integrator.range.plow2 = 0; }, .saved = true},
  {.group = option_group::sweep, .kind = OptionKind::number, .odex = "high", .help = "The value the sweep ends at.", .xppaut_names = {"RANGEHIGH"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.integrator.range.phigh; }),
   .reset = [](Session &s) { s.integrator.range.phigh = s.integrator.range.phigh2 = 1; }, .saved = true},
  {.group = option_group::sweep, .kind = OptionKind::choice, .odex = "reset", .help = "Whether each run of the sweep starts from the initial conditions.", .xppaut_names = {"RANGERESET"}, .once = true,
   .boolean = member([](auto &s) -> auto & { return s.integrator.range.reset; }),
   /* each run starts from the initial data */
   .reset = [](Session &s) { s.integrator.range.reset = true; },
   .rule = OptionRule::choice, .choices = boolean_names, .saved = true},
  {.group = option_group::sweep, .kind = OptionKind::choice, .odex = "old_ic", .help = "Whether each run of the sweep starts where the one before ended.", .xppaut_names = {"RANGEOLDIC"}, .once = true,
   .boolean = member([](auto &s) -> auto & { return s.integrator.range.oldic; }),
   /* the same initial data for each run */
   .reset = [](Session &s) { s.integrator.range.oldic = true; },
   .rule = OptionRule::choice, .choices = boolean_names, .saved = true},
  {.group = option_group::sweep, .kind = OptionKind::choice, .odex = "write_files", .help = "Whether each run of the sweep is written to a file of its own.",
   .xppaut_names = {"RANGE"}, .once = true,
   .boolean = member([](auto &s) -> auto & { return s.integrator.range.write_files; }),
   .rule = OptionRule::choice, .choices = boolean_names, .saved = true},
  /* ---- autoX ---- */
  {.group = option_group::continuation, .kind = OptionKind::whole, .odex = "mesh_intervals", .help = "How many mesh intervals a periodic orbit or boundary value solution has.",
   .xppaut_names = {"NTST"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.autox_state.options.ntst; }), .autox_numeric = AUTOX_NUM_MESH_INTERVALS},
  {.group = option_group::continuation, .kind = OptionKind::whole, .odex = "max_steps", .help = "The most continuation steps a run takes before it stops.",
   .xppaut_names = {"NMAX"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.autox_state.options.nmx; }), .autox_numeric = AUTOX_NUM_MAX_STEPS},
  {.group = option_group::continuation, .kind = OptionKind::whole, .odex = "output_every", .help = "A diagram point is kept every this many steps.",
   .xppaut_names = {"NPR"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.autox_state.options.npr; }), .autox_numeric = AUTOX_NUM_OUTPUT_EVERY},
  {.group = option_group::continuation, .kind = OptionKind::whole, .odex = "collocation_points", .help = "The collocation points in each mesh interval.",
   .xppaut_names = {"NCOL"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.autox_state.options.ncol; }), .autox_numeric = AUTOX_NUM_COLLOCATION_POINTS},
  {.group = option_group::continuation, .kind = OptionKind::number, .odex = "min_step_size", .help = "The smallest continuation step.",
   .xppaut_names = {"DSMIN"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.autox_state.options.dsmin; }), .autox_numeric = AUTOX_NUM_MIN_STEP_SIZE},
  {.group = option_group::continuation, .kind = OptionKind::number, .odex = "max_step_size", .help = "The largest continuation step.",
   .xppaut_names = {"DSMAX"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.autox_state.options.dsmax; }), .autox_numeric = AUTOX_NUM_MAX_STEP_SIZE},
  {.group = option_group::continuation, .kind = OptionKind::number, .odex = "step_size", .help = "The first continuation step.",
   .xppaut_names = {"DS"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.autox_state.options.ds; }), .autox_numeric = AUTOX_NUM_STEP_SIZE},
  {.group = option_group::continuation, .kind = OptionKind::number, .odex = "parameter_min", .help = "A continuation run stops when the parameter falls below this.",
   .xppaut_names = {"PARMIN"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.autox_state.options.rl0; }), .autox_numeric = AUTOX_NUM_PARAMETER_MIN},
  {.group = option_group::continuation, .kind = OptionKind::number, .odex = "parameter_max", .help = "A continuation run stops when the parameter rises above this.",
   .xppaut_names = {"PARMAX"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.autox_state.options.rl1; }), .autox_numeric = AUTOX_NUM_PARAMETER_MAX},
  {.group = option_group::continuation, .kind = OptionKind::number, .odex = "norm_min", .help = "A continuation run stops when the solution's norm falls below this.",
   .xppaut_names = {"NORMMIN"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.autox_state.options.a0; }), .autox_numeric = AUTOX_NUM_NORM_MIN},
  {.group = option_group::continuation, .kind = OptionKind::number, .odex = "norm_max", .help = "A continuation run stops when the solution's norm rises above this.",
   .xppaut_names = {"NORMMAX"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.autox_state.options.a1; }), .autox_numeric = AUTOX_NUM_NORM_MAX},
  {.group = option_group::continuation, .kind = OptionKind::number, .odex = "parameter_tolerance", .help = "The tolerance the parameter converges to in the Newton iteration.",
   .xppaut_names = {"EPSL"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.autox_state.options.epsl; }), .autox_numeric = AUTOX_NUM_PARAMETER_TOLERANCE},
  {.group = option_group::continuation, .kind = OptionKind::number, .odex = "state_tolerance", .help = "The tolerance the state converges to in the Newton iteration.",
   .xppaut_names = {"EPSU"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.autox_state.options.epsu; }), .autox_numeric = AUTOX_NUM_STATE_TOLERANCE},
  {.group = option_group::continuation, .kind = OptionKind::number, .odex = "special_point_tolerance", .help = "The tolerance the special points of a branch are located to.",
   .xppaut_names = {"EPSS"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.autox_state.options.epss; }), .autox_numeric = AUTOX_NUM_SPECIAL_POINT_TOLERANCE},
  {.group = option_group::continuation, .kind = OptionKind::whole, .odex = "mesh_adapt_every", .help = "The mesh is adapted every this many steps; 0 keeps it as it is.", .once = true,
   .whole = member([](auto &s) -> auto & { return s.autox_state.advanced.iad; }), .autox_numeric = AUTOX_NUM_MESH_ADAPT_EVERY},
  {.group = option_group::continuation, .kind = OptionKind::whole, .odex = "max_branch_switches", .help = "The most times a run switches to another branch.", .once = true,
   .whole = member([](auto &s) -> auto & { return s.autox_state.advanced.max_branch_switches; }), .autox_numeric = AUTOX_NUM_MAX_BRANCH_SWITCHES},
  {.group = option_group::continuation, .kind = OptionKind::choice, .odex = "switch_one_direction", .help = "Whether a branch switch continues in one direction only.", .once = true,
   .boolean = member([](auto &s) -> auto & { return s.autox_state.advanced.switch_one_direction; }),
   .rule = OptionRule::choice, .autox_numeric = AUTOX_NUM_SWITCH_ONE_DIRECTION, .choices = boolean_names},
  {.group = option_group::continuation, .kind = OptionKind::choice, .odex = "output_detail", .help = "How much of each point the run writes to the Output.", .once = true,
   .index = choice_member([](auto &s) -> auto & { return s.autox_state.advanced.iid; }),
   .foreign = [](std::string_view v) { return numbered_word(autox_output_detail_words, v); },
   .rule = OptionRule::choice, .autox_numeric = AUTOX_NUM_OUTPUT_DETAIL, .choices = autox_output_detail_words},
  {.group = option_group::continuation, .kind = OptionKind::whole, .odex = "max_location_iterations", .help = "The most iterations that locate a special point.", .once = true,
   .whole = member([](auto &s) -> auto & { return s.autox_state.advanced.itmx; }), .autox_numeric = AUTOX_NUM_MAX_LOCATION_ITERATIONS},
  {.group = option_group::continuation, .kind = OptionKind::whole, .odex = "max_newton_iterations", .help = "The most Newton iterations in one continuation step.", .once = true,
   .whole = member([](auto &s) -> auto & { return s.autox_state.advanced.itnw; }), .autox_numeric = AUTOX_NUM_MAX_NEWTON_ITERATIONS},
  {.group = option_group::continuation, .kind = OptionKind::whole, .odex = "full_newton_after", .help = "A full Newton iteration is taken after this many steps; before it the Jacobian is kept.", .once = true,
   .whole = member([](auto &s) -> auto & { return s.autox_state.advanced.nwtn; }), .autox_numeric = AUTOX_NUM_FULL_NEWTON_AFTER},
  {.group = option_group::continuation, .kind = OptionKind::whole, .odex = "step_adapt_every", .help = "The step size is adapted every this many steps.", .once = true,
   .whole = member([](auto &s) -> auto & { return s.autox_state.advanced.iads; }), .autox_numeric = AUTOX_NUM_STEP_ADAPT_EVERY},
  {.group = option_group::continuation, .kind = OptionKind::choice, .odex = "suppress_branch_points", .help = "Whether branch points are left out of the diagram.", .once = true,
   .boolean = member([](auto &s) -> auto & { return s.autox_state.suppress_bp; }),
   .rule = OptionRule::choice, .autox_numeric = AUTOX_NUM_SUPPRESS_BRANCH_POINTS, .choices = boolean_names},
  {.group = option_group::continuation, .kind = OptionKind::choice, .odex = "follow_all_branches", .help = "Whether a run follows every branch it finds, not only the one it started on.", .once = true,
   .boolean = member([](auto &s) -> auto & { return s.autox_state.follow_all_branches; }),
   .rule = OptionRule::choice, .autox_numeric = AUTOX_NUM_FOLLOW_ALL_BRANCHES, .choices = boolean_names},
  {.group = option_group::continuation, .kind = OptionKind::text, .odex = "special_points", .help = "The codimension-two points a run detects: a comma-separated list of their names, empty for none.", .once = true,
   .show = [](const Session &s) {
     std::string names;
     for (std::size_t i = 0; i < autox_codim_kinds.size(); ++i)
       if (s.autox_state.special_points[i]) names += (names.empty() ? "" : ",") + std::string(autox_codim_keys[i]);
     return names;
   },
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     std::array<bool, autox_codim_kinds.size()> on{};
     std::string_view rest = v.text;
     while (!rest.empty()) {
       const std::size_t comma = rest.find(',');
       std::string_view word = rest.substr(0, comma);
       rest = comma == std::string_view::npos ? std::string_view() : rest.substr(comma + 1);
       while (word.starts_with(' ')) word.remove_prefix(1);
       while (word.ends_with(' ')) word.remove_suffix(1);
       const auto at = std::ranges::find(autox_codim_keys, word);
       if (at == autox_codim_keys.end()) return "not a list of codimension-two point names";
       on[static_cast<std::size_t>(at - autox_codim_keys.begin())] = true;
     }
     if (v.apply) s.autox_state.special_points = on;
     return nullptr;
   }},
  {.group = option_group::diagram, .kind = OptionKind::name, .odex = "variable", .help = "The variable the continuation diagram plots up.", .xppaut_names = {"AUTOVAR"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.autox_state.options.var; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     const int i = variable_of(s, v);
     if (i < 1) return "no such variable";
     if (v.apply) s.autox_state.options.var = i - 1;
     return nullptr;
   }},
  {.group = option_group::diagram, .kind = OptionKind::number, .odex = "x_min", .help = "The left edge of the continuation diagram.", .xppaut_names = {"AUTOXMIN"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.autox_state.options.xmin; })},
  {.group = option_group::diagram, .kind = OptionKind::number, .odex = "x_max", .help = "The right edge of the continuation diagram.", .xppaut_names = {"AUTOXMAX"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.autox_state.options.xmax; })},
  {.group = option_group::diagram, .kind = OptionKind::number, .odex = "y_min", .help = "The bottom edge of the continuation diagram.", .xppaut_names = {"AUTOYMIN"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.autox_state.options.ymin; })},
  {.group = option_group::diagram, .kind = OptionKind::number, .odex = "y_max", .help = "The top edge of the continuation diagram.", .xppaut_names = {"AUTOYMAX"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.autox_state.options.ymax; })},
  {.group = option_group::diagram, .kind = OptionKind::color, .odex = "colors.stable_equilibrium", .help = "The colour of the stable equilibria in the diagram.", .xppaut_names = {"SEC"}, .once = true,
   .color = member([](auto &s) -> auto & { return s.autox_state.stable_eq_color; }), .foreign = foreign_autox_color},
  {.group = option_group::diagram, .kind = OptionKind::color, .odex = "colors.unstable_equilibrium", .help = "The colour of the unstable equilibria in the diagram.", .xppaut_names = {"UEC"}, .once = true,
   .color = member([](auto &s) -> auto & { return s.autox_state.unstable_eq_color; }), .foreign = foreign_autox_color},
  {.group = option_group::diagram, .kind = OptionKind::color, .odex = "colors.stable_periodic", .help = "The colour of the stable periodic orbits in the diagram.", .xppaut_names = {"SPC"}, .once = true,
   .color = member([](auto &s) -> auto & { return s.autox_state.stable_po_color; }), .foreign = foreign_autox_color},
  {.group = option_group::diagram, .kind = OptionKind::color, .odex = "colors.unstable_periodic", .help = "The colour of the unstable periodic orbits in the diagram.", .xppaut_names = {"UPC"}, .once = true,
   .color = member([](auto &s) -> auto & { return s.autox_state.unstable_po_color; }), .foreign = foreign_autox_color},
  {.group = option_group::plot, .kind = OptionKind::choice, .odex = "colormap", .help = "The colours a colour-coded curve or an array plot runs through.", .xppaut_names = {"COLORMAP"}, .once = true,
   .index = choice_member([](auto &s) -> auto & { return s.colormap; }),
   .foreign = [](std::string_view v) { return numbered_word(colormap_names, v); }, .imported_default = "jet",
   .rule = OptionRule::choice, .choices = colormap_names, .wide = true},
  /* the curves of the starting graph, each `{ x, y, z }` (the time, then the first variable twice, when left out); the
     first holds the axes' variables */
  {.group = option_group::plot, .kind = OptionKind::curves, .odex = "curves", .help = "The curves of the starting graph, each with the variables on its axes; the first holds the axes' variables.", .xppaut_names = {"CURVES"}, .once = true,
   .maximum = max_plot_curves},
  {.group = option_group::plot, .kind = OptionKind::choice, .odex = "axes.kind", .help = "A two- or three-dimensional plot.", .xppaut_names = {"AXES"}, .once = true,
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
  {.group = option_group::plot, .kind = OptionKind::number, .odex = "axes.x_low", .help = "The left edge of the two-dimensional window.", .xppaut_names = {"XLO"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.plot_settings.my_xlo; }),
   /* the default total's span, 0 to 20 */
   .reset = [](Session &s) {
     s.plot_settings.my_xlo = s.plot_settings.x_3d[0] = 0;
     s.options_set.mark(once_option("plot.axes.x_min"));
   }},
  {.group = option_group::plot, .kind = OptionKind::number, .odex = "axes.x_high", .help = "The right edge of the two-dimensional window.", .xppaut_names = {"XHI"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.plot_settings.my_xhi; }),
   .reset = [](Session &s) {
     s.plot_settings.my_xhi = s.plot_settings.x_3d[1] = 20;
     s.options_set.mark(once_option("plot.axes.x_max"));
   }},
  {.group = option_group::plot, .kind = OptionKind::number, .odex = "axes.y_low", .help = "The bottom edge of the two-dimensional window.", .xppaut_names = {"YLO"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.plot_settings.my_ylo; }),
   /* -1 to 1 */
   .reset = [](Session &s) {
     s.plot_settings.my_ylo = s.plot_settings.y_3d[0] = -1;
     s.options_set.mark(once_option("plot.axes.y_min"));
   }},
  {.group = option_group::plot, .kind = OptionKind::number, .odex = "axes.y_high", .help = "The top edge of the two-dimensional window.", .xppaut_names = {"YHI"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.plot_settings.my_yhi; }),
   .reset = [](Session &s) {
     s.plot_settings.my_yhi = s.plot_settings.y_3d[1] = 1;
     s.options_set.mark(once_option("plot.axes.y_max"));
   }},
  /* the 3D box; xmin and ymin set the 2D window's low end too, unless
     something set that */
  {.group = option_group::plot, .kind = OptionKind::number, .odex = "axes.x_min", .help = "The low end of the three-dimensional box in x.", .xppaut_names = {"XMIN"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.plot_settings.x_3d[0]; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (const char *why = number_into(s.plot_settings.x_3d[0], v)) return why;
     if (v.apply && v.source.claim(s, once_option("plot.axes.x_low"))) s.plot_settings.my_xlo = s.plot_settings.x_3d[0];
     return nullptr;
   },
   .reset = [](Session &s) {
     s.plot_settings.x_3d[0] = -box_side;
     s.options_set.mark(once_option("plot.axes.x_low"));
   }},
  {.group = option_group::plot, .kind = OptionKind::number, .odex = "axes.x_max", .help = "The high end of the three-dimensional box in x.", .xppaut_names = {"XMAX"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.plot_settings.x_3d[1]; }),
   .reset = [](Session &s) {
     s.plot_settings.x_3d[1] = box_side;
     s.options_set.mark(once_option("plot.axes.x_high"));
   }},
  {.group = option_group::plot, .kind = OptionKind::number, .odex = "axes.y_min", .help = "The low end of the three-dimensional box in y.", .xppaut_names = {"YMIN"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.plot_settings.y_3d[0]; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (const char *why = number_into(s.plot_settings.y_3d[0], v)) return why;
     if (v.apply && v.source.claim(s, once_option("plot.axes.y_low"))) s.plot_settings.my_ylo = s.plot_settings.y_3d[0];
     return nullptr;
   },
   .reset = [](Session &s) {
     s.plot_settings.y_3d[0] = -box_side;
     s.options_set.mark(once_option("plot.axes.y_low"));
   }},
  {.group = option_group::plot, .kind = OptionKind::number, .odex = "axes.y_max", .help = "The high end of the three-dimensional box in y.", .xppaut_names = {"YMAX"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.plot_settings.y_3d[1]; }),
   .reset = [](Session &s) {
     s.plot_settings.y_3d[1] = box_side;
     s.options_set.mark(once_option("plot.axes.y_high"));
   }},
  {.group = option_group::plot, .kind = OptionKind::number, .odex = "axes.z_min", .help = "The low end of the three-dimensional box in z.", .xppaut_names = {"ZMIN"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.plot_settings.z_3d[0]; }),
   .reset = [](Session &s) { s.plot_settings.z_3d[0] = -box_side; }},
  {.group = option_group::plot, .kind = OptionKind::number, .odex = "axes.z_max", .help = "The high end of the three-dimensional box in z.", .xppaut_names = {"ZMAX"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.plot_settings.z_3d[1]; }),
   .reset = [](Session &s) { s.plot_settings.z_3d[1] = box_side; }},
  {.group = option_group::plot, .kind = OptionKind::number, .odex = "axes.phi", .help = "The azimuth the three-dimensional plot is viewed from, in degrees.", .xppaut_names = {"PHI"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.drawing.phi0; })},
  {.group = option_group::plot, .kind = OptionKind::number, .odex = "axes.theta", .help = "The elevation the three-dimensional plot is viewed from, in degrees.", .xppaut_names = {"THETA"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.drawing.theta0; })},
  /* the line every new curve starts with */
  {.group = option_group::plot, .kind = OptionKind::choice, .odex = "line.draw", .help = "Whether a new curve is drawn as a line, as points or as both.", .once = true,
   .index = choice_member([](auto &s) -> auto & { return s.plot_settings.start_line_style.draw; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     const std::optional<LineDraw> draw = parse_line_draw(v.text);
     if (!draw) return "not line, points or both";
     LineStyle style = s.plot_settings.start_line_style;
     style.draw = *draw;
     return start_line_style_into(s, style, v);
   },
   .rule = OptionRule::choice, .choices = line_draw_names},
  {.group = option_group::plot, .kind = OptionKind::choice, .odex = "line.dash", .help = "The dash pattern of a new curve's line.", .once = true,
   .index = choice_member([](auto &s) -> auto & { return s.plot_settings.start_line_style.dash; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     const std::optional<LineDash> dash = parse_line_dash(v.text);
     if (!dash) return "not solid, dashed, dotted or dash_dot";
     LineStyle style = s.plot_settings.start_line_style;
     style.dash = *dash;
     return start_line_style_into(s, style, v);
   },
   .rule = OptionRule::choice, .choices = line_dash_names},
  {.group = option_group::plot, .kind = OptionKind::number, .odex = "line.width", .help = "The width of a new curve's line.", .once = true,
   .real = member([](auto &s) -> auto & { return s.plot_settings.start_line_style.width; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     const std::optional<double> width = parse_line_size(v.text, false);
     if (!width) return "not a number above 0";
     LineStyle style = s.plot_settings.start_line_style;
     style.width = *width;
     return start_line_style_into(s, style, v);
   }},
  {.group = option_group::plot, .kind = OptionKind::choice, .odex = "line.marker", .help = "The marker drawn at the points of a new curve.", .once = true,
   .index = choice_member([](auto &s) -> auto & { return s.plot_settings.start_line_style.marker; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     const std::optional<LineMarker> marker = parse_line_marker(v.text);
     if (!marker) return "not circle, square, triangle, diamond, cross, plus or star";
     LineStyle style = s.plot_settings.start_line_style;
     style.marker = *marker;
     return start_line_style_into(s, style, v);
   },
   .rule = OptionRule::choice, .choices = line_marker_names},
  {.group = option_group::plot, .kind = OptionKind::number, .odex = "line.marker_size", .help = "The size of the markers of a new curve.", .once = true,
   .real = member([](auto &s) -> auto & { return s.plot_settings.start_line_style.marker_size; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     const std::optional<double> size = parse_line_size(v.text, true);
     if (!size) return "not a number of at least 0";
     LineStyle style = s.plot_settings.start_line_style;
     style.marker_size = *size;
     return start_line_style_into(s, style, v);
   }},
  {.group = option_group::plot, .kind = OptionKind::whole, .odex = "nullclines.mesh", .help = "How many points a side the nullclines are found on.", .xppaut_names = {"NMESH"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.numerics.nullcline_mesh; }),
   /* a 40 by 40 grid: smooth nullclines, quickly */
   .reset = [](Session &s) { s.numerics.nullcline_mesh = 40; },
   .key = "nullcline_mesh", .label = "Nullcline mesh", .rule = OptionRule::whole_positive, .maximum = option_limit::mesh, .saved = true},
  {.group = option_group::plot, .kind = OptionKind::choice, .odex = "nullclines.draw", .help = "Whether the nullclines are drawn after a run.", .once = true,
   .boolean = member([](auto &s) -> auto & { return s.nullclines.draw; }),
   .rule = OptionRule::choice, .choices = boolean_names},
  {.group = option_group::plot, .kind = OptionKind::color, .odex = "nullclines.x_color", .help = "The colour of the x nullcline.", .xppaut_names = {"XNC"}, .once = true,
   .color = member([](auto &s) -> auto & { return s.nullclines.x_null_color; }), .foreign = foreign_plot_color},
  {.group = option_group::plot, .kind = OptionKind::color, .odex = "nullclines.y_color", .help = "The colour of the y nullcline.", .xppaut_names = {"YNC"}, .once = true,
   .color = member([](auto &s) -> auto & { return s.nullclines.y_null_color; }), .foreign = foreign_plot_color},
  {.group = option_group::plot, .kind = OptionKind::whole, .odex = "direction_field.grid", .help = "How many arrows a side the direction field has.", .xppaut_names = {"DFGRID"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.nullclines.df_grid; }),
   .rule = OptionRule::whole_at_least_two, .maximum = option_limit::mesh},
  {.group = option_group::plot, .kind = OptionKind::choice, .odex = "direction_field.draw", .help = "What a run draws of the direction field, and whether it also saves it.", .xppaut_names = {"DFDRAW"}, .once = true,
   .index = choice_member([](auto &s) -> auto & { return s.nullclines.df_batch; }),
   .reset = [](Session &s) { s.nullclines.df_batch = 0; },
   .foreign = [](std::string_view v) { return numbered_word(direction_field_draw_names, v); },
   .rule = OptionRule::choice, .choices = direction_field_draw_names},
  {.group = option_group::plot, .kind = OptionKind::color, .odex = "manifolds.stable_color", .help = "The colour of the stable manifold of a saddle.", .xppaut_names = {"SMC"}, .once = true,
   .color = member([](auto &s) -> auto & { return s.manifolds.stable_color; }), .foreign = foreign_plot_color},
  {.group = option_group::plot, .kind = OptionKind::color, .odex = "manifolds.unstable_color", .help = "The colour of the unstable manifold of a saddle.", .xppaut_names = {"UMC"}, .once = true,
   .color = member([](auto &s) -> auto & { return s.manifolds.unstable_color; }), .foreign = foreign_plot_color},
  {.group = option_group::plot, .kind = OptionKind::choice, .odex = "color_by.enabled", .help = "Whether a curve is coloured by a quantity along it.", .xppaut_names = {"COLORIZE"}, .once = true,
   .boolean = member([](auto &s) -> auto & { return s.nullclines.colorize_flag; }),
   .reset = [](Session &s) { s.nullclines.colorize_flag = false; },
   .rule = OptionRule::choice, .choices = boolean_names},
  {.group = option_group::plot, .kind = OptionKind::name, .odex = "color_by.via", .help = "The variable a colour-coded curve takes its colour from.", .xppaut_names = {"COLORVIA"}, .once = true,
   .text = member([](auto &s) -> auto & { return s.nullclines.color_via; })},
  {.group = option_group::plot, .kind = OptionKind::number, .odex = "color_by.low", .help = "The value of the colouring variable that gets the first colour.", .xppaut_names = {"COLORLO"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.nullclines.color_via_lo; })},
  {.group = option_group::plot, .kind = OptionKind::number, .odex = "color_by.high", .help = "The value of the colouring variable that gets the last colour.", .xppaut_names = {"COLORHI"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.nullclines.color_via_hi; })},
  {.group = option_group::analysis, .kind = OptionKind::number, .odex = "histogram.low", .help = "The low end of the histogram's range.", .xppaut_names = {"HISTLO"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.histogram.info.xlo; })},
  {.group = option_group::analysis, .kind = OptionKind::number, .odex = "histogram.high", .help = "The high end of the histogram's range.", .xppaut_names = {"HISTHI"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.histogram.info.xhi; })},
  {.group = option_group::analysis, .kind = OptionKind::whole, .odex = "histogram.bins", .help = "How many bins the histogram has.", .xppaut_names = {"HISTBINS"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.histogram.info.nbins; }), .rule = OptionRule::whole_at_least_two, .maximum = option_limit::histogram_bins},
  {.group = option_group::analysis, .kind = OptionKind::name, .odex = "histogram.column", .help = "The column of the data the histogram counts.", .xppaut_names = {"HISTCOL"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.histogram.info.col; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.histogram.info.col, v);
   }},
  {.group = option_group::analysis, .kind = OptionKind::number, .odex = "histogram2.low", .help = "The low end of the second axis of the two-dimensional histogram.", .xppaut_names = {"HISTLO2"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.histogram.info.ylo; })},
  {.group = option_group::analysis, .kind = OptionKind::number, .odex = "histogram2.high", .help = "The high end of the second axis of the two-dimensional histogram.", .xppaut_names = {"HISTHI2"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.histogram.info.yhi; })},
  {.group = option_group::analysis, .kind = OptionKind::whole, .odex = "histogram2.bins", .help = "How many bins the second axis of the two-dimensional histogram has.", .xppaut_names = {"HISTBINS2"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.histogram.info.nbins2; }), .rule = OptionRule::whole_at_least_two, .maximum = option_limit::histogram_bins},
  {.group = option_group::analysis, .kind = OptionKind::name, .odex = "histogram2.column", .help = "The column on the second axis of the two-dimensional histogram.", .xppaut_names = {"HISTCOL2"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.histogram.info.col2; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.histogram.info.col2, v);
   }},
  {.group = option_group::analysis, .kind = OptionKind::name, .odex = "spectrum.column", .help = "The column of the data the spectrum is taken of.", .xppaut_names = {"SPECCOL"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.histogram.spec_col; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.histogram.spec_col, v);
   }},
  {.group = option_group::analysis, .kind = OptionKind::name, .odex = "spectrum.column2", .help = "The second column of a cross spectrum or a coherence.", .xppaut_names = {"SPECCOL2"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.histogram.spec_col2; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     return variable_into(s, s.histogram.spec_col2, v);
   }},
  {.group = option_group::analysis, .kind = OptionKind::whole, .odex = "spectrum.width", .help = "How many points the spectrum's windows are long.", .xppaut_names = {"SPECWIDTH"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.histogram.spec_wid; }), .rule = OptionRule::whole_positive, .maximum = option_limit::spectrum_samples},
  {.group = option_group::analysis, .kind = OptionKind::whole, .odex = "spectrum.window", .help = "The window the spectrum's segments are weighted with.", .xppaut_names = {"SPECWIN"}, .once = true,
   .whole = member([](auto &s) -> auto & { return s.histogram.spec_win; })},
  /* the picture files a batch run writes: PDF or SVG (XPPAUT's ps imports as pdf: PostScript went with W479, #542) */
  {.group = option_group::export_, .kind = OptionKind::choice, .odex = "format", .help = "The vector format a picture is exported in.", .xppaut_names = {"PLOTFMT"}, .once = true,
   /* the format is kept as its word */
   .index = {[](const Session &s) { return choice_index(picture_format_names, s.plot_export.format); },
             [](Session &s, int i) { s.plot_export.format = std::string(picture_format_names[static_cast<std::size_t>(i)]); }},
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (v.apply) s.plot_export.format = v.text;
     return nullptr;
   },
   .foreign = [](std::string_view v) { return v == "ps" ? std::string("pdf") : std::string(); },
   .rule = OptionRule::choice, .choices = picture_format_names},
  {.group = option_group::export_, .kind = OptionKind::choice, .odex = "color", .help = "Whether an exported picture is in colour.", .xppaut_names = {"PS_COLOR"}, .once = true,
   .boolean = member([](auto &s) -> auto & { return s.plot_export.color; }),
   .rule = OptionRule::choice, .choices = boolean_names},
  {.group = option_group::export_, .kind = OptionKind::number, .odex = "font_size", .help = "The font size of an exported picture.", .xppaut_names = {"PS_FSIZE"}, .once = true,
   .real = member([](auto &s) -> auto & { return s.plot_export.font_size; }),
   .rule = OptionRule::positive},
  {.group = option_group::interface, .kind = OptionKind::sliders, .odex = "sliders", .help = "The sliders the window offers: a parameter or variable each, with its range.", .once = true},
  {.group = option_group::interface, .kind = OptionKind::text, .odex = "buttons", .help = "The buttons of the window, each running a command or a set.",.xppaut_names = {"BUT"}, 
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (v.apply) add_user_button(s, v.text);
     return nullptr;
   },
   .repeatable = true},
  {.group = option_group::interface, .kind = OptionKind::path, .odex = "animation", .help = "The animation file the window plays beside a run.", .once = true,
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     if (!files::name_ok(v.text) || !files::has_extension(v.text, ".anix")) return "must be an .anix base name";
     if (v.apply && !s.model().command_line.ani_file) {
       s.animation.options.file = include_path(s.model().this_file, v.text);
       s.animation.options.use_file = 1;
     }
     return nullptr;
   }, .wide = true, .top_level = true},
  {.group = option_group::output, .kind = OptionKind::path, .odex = "data_file", .help = "The file a run writes its data to, named from the model's folder.", .xppaut_names = {"OUTPUT"}, .once = true,
   .text = member([](auto &s) -> auto & { return s.batch_out_file; }),
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
  {.group = option_group::output, .kind = OptionKind::path, .odex = "nullclines_file", .help = "The file the nullclines are saved to after a run.", .once = true,
   .text = member([](auto &s) -> auto & { return s.nullclines.file; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     /* the nullclines saved after a run: named like data_file, from the model's folder */
     return data_file_into(s, v, s.nullclines.file);
   }},
  /* ---- the histogram and the spectrum a batch run writes ---- */
  {.group = option_group::output, .kind = OptionKind::choice, .odex = "after_run", .help = "What a batch run makes of its data once it has run: an analysis, or the mean or variance of its runs.", .xppaut_names = {"POSTPROCESS"}, .once = true,
   .index = choice_member([](auto &s) -> auto & { return s.histogram.post_process; }),
   .reset = [](Session &s) { s.histogram.post_process = AfterRun::none; },
   .foreign = [](std::string_view v) { return numbered_word(xppaut_postprocess_words, v); },
   .rule = OptionRule::choice, .choices = after_run_names},
  {.group = option_group::output, .kind = OptionKind::path, .odex = "after_run_file", .help = "The file the after_run analysis is written to, so the data file keeps the trajectory.", .once = true,
   .text = member([](auto &s) -> auto & { return s.after_run_file; }),
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     /* named like data_file, from the model's folder */
     return data_file_into(s, v, s.after_run_file);
   }},
  /* XPPAUT's quiet and logfile (import only): the log is the user's, not a model's (the preferences' log_level and log_file) */
  {.name = "QUIET", .dropped = log_level_is_the_users},
  {.name = "LOGFILE", .dropped = log_file_is_the_users},
  /* XPPAUT's autoeval (import only): each formula table's own auto_evaluate, which `--convert` writes in its `table` statement */
  {.kind = OptionKind::whole, .name = "AUTOEVAL", .once = true, .in_statement = true,
   .parse = [](Session &s, const OptionValue &v) -> const char * {
     int f = 0;
     if (const char *why = whole_in(v, f)) return why;
     if (v.apply) set_autox_eval_flags(s, f);
     return nullptr;
   }},
  /* XPPAUT's maxstor (import only): the data store starts with DataStore::start_rows and grows to fit, so it has no size to set */
  {.name = "MAXSTOR", .once = true, .expand = [](std::string_view) -> ForeignOptions { return {}; }},
  /* XPPAUT's stoch (import only): 1 and 2 are the mean and the variance of the runs of a sweep, the after_run that makes them */
  {.name = "STOCH", .once = true,
   .expand = [](std::string_view value) -> ForeignOptions {
     if (value == "1") return {{{"output.after_run", "mean_of_runs"}}, nullptr};
     if (value == "2") return {{{"output.after_run", "variance_of_runs"}}, nullptr};
     if (value == "0") return {};
     return {{}, "not 0, 1 or 2"};
   }},
  /* a .ode's bound (import only) is both the state limit and the search limit */
  {.name = "BOUND", .once = true,
   .expand = [](std::string_view value) -> ForeignOptions {
     double x = 0;
     if (!parse_number(value, x)) return {{}, "not a number"};
     if (const char *why = rule_problem(OptionRule::positive, x)) return {{}, why};
     return {{{"integration.state_limit", std::string(value)}, {"equilibria.search_limit", std::string(value)}}, nullptr};
   }},
  /* XPPAUT's lt (import only): 1 a solid line, 0 a dot at each point, -n a circle of radius n at each point */
  {.name = "LT", .once = true,
   .expand = [](std::string_view value) -> ForeignOptions {
     int lt = 0;
     const std::optional<LineStyle> style = parse_int(value, lt) ? line_style_of_xppaut(lt) : std::nullopt;
     if (!style) return {{}, "not a line type from -5 to 1"};
     if (style->draw == LineDraw::line) return {{{"plot.line.draw", "line"}}, nullptr};
     return {{{"plot.line.draw", "points"}, {"plot.line.marker", "circle"}, {"plot.line.marker_size", number(style->marker_size)}}, nullptr};
   }},
  /* XPPAUT's PostScript font and line width (import only): a PDF has Helvetica, and each curve has its own width */
  {.name = "PS_FONT", .once = true, .expand = [](std::string_view) -> ForeignOptions { return {}; }},
  {.name = "PS_LW", .once = true, .expand = [](std::string_view) -> ForeignOptions { return {}; }},
  /* XPPAUT's ncdraw (import only): 1 draws them, 2 saves them (to nullclines.dat) */
  {.name = "NCDRAW", .once = true,
   .expand = [](std::string_view value) -> ForeignOptions {
     if (value == "0") return {{{"plot.nullclines.draw", "false"}}, nullptr};
     if (value == "1") return {{{"plot.nullclines.draw", "true"}}, nullptr};
     if (value == "2") return {{{"plot.nullclines.draw", "false"}, {"output.nullclines_file", nullclines_dump_file}}, nullptr};
     return {{}, "not 0, 1 or 2"};
   }},
  /* XPPAUT's plotted variables (import only): xp, yp and zp are the first curve of `curves`, nplot says how many there are and
     xp2 to zp8 are the others (the converter writes the list: odex_convert.cpp); with no window of ours opened by a model, they
     only name the variables */
  {.kind = OptionKind::name, .name = "XP", .once = true, .curve_field = CurveField::x,
   .parse = [](Session &s, const OptionValue &v) -> const char * { return variable_into(s, s.plot_settings.curves[0].x, v); }},
  {.kind = OptionKind::name, .name = "YP", .once = true, .curve_field = CurveField::y,
   .parse = [](Session &s, const OptionValue &v) -> const char * { return variable_into(s, s.plot_settings.curves[0].y, v); }},
  {.kind = OptionKind::name, .name = "ZP", .once = true, .curve_field = CurveField::z,
   .parse = [](Session &s, const OptionValue &v) -> const char * { return variable_into(s, s.plot_settings.curves[0].z, v); }},
  {.kind = OptionKind::whole, .name = "NPLOT", .once = true, .curve_field = CurveField::count,
   .rule = OptionRule::whole_positive, .maximum = max_plot_curves},
  {.kind = OptionKind::name, .name = "XP", .first_digit = '2', .last_digit = '8', .curve_field = CurveField::x,
   .parse = [](Session &s, const OptionValue &v) -> const char * { int i = 0; return variable_into(s, i, v); }},
  {.kind = OptionKind::name, .name = "YP", .first_digit = '2', .last_digit = '8', .curve_field = CurveField::y,
   .parse = [](Session &s, const OptionValue &v) -> const char * { int i = 0; return variable_into(s, i, v); }},
  {.kind = OptionKind::name, .name = "ZP", .first_digit = '2', .last_digit = '8', .curve_field = CurveField::z,
   .parse = [](Session &s, const OptionValue &v) -> const char * { int i = 0; return variable_into(s, i, v); }},
  /* XPPAUT's several starting graphs (import only): a model opens one graph; the session opens more */
  {.name = "MULTIWIN", .dropped = several_graphs},
  {.name = "SIMPLOT", .dropped = several_graphs},
  {.kind = OptionKind::number, .name = "XLO", .first_digit = '2', .last_digit = '8', .dropped = several_graphs,
   .parse = [](Session &, const OptionValue &v) -> const char * { double x = 0; return number_into(x, v); }},
  {.kind = OptionKind::number, .name = "XHI", .first_digit = '2', .last_digit = '8', .dropped = several_graphs,
   .parse = [](Session &, const OptionValue &v) -> const char * { double x = 0; return number_into(x, v); }},
  {.kind = OptionKind::number, .name = "YLO", .first_digit = '2', .last_digit = '8', .dropped = several_graphs,
   .parse = [](Session &, const OptionValue &v) -> const char * { double x = 0; return number_into(x, v); }},
  {.kind = OptionKind::number, .name = "YHI", .first_digit = '2', .last_digit = '8', .dropped = several_graphs,
   .parse = [](Session &, const OptionValue &v) -> const char * { double x = 0; return number_into(x, v); }},
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
  /* the X11 window's bell, fonts, colours, image, size and paper: still
     accepted (old .ode and .xpprc files set them), no longer kept */
  {.name = "BELL", .zero_or_one = true},
  {.name = "TUTORIAL", .zero_or_one = true}, /* the tutorial went with W229 */
  {.name = "BIGFONT", .xppaut_names = {"BIG"}},
  {.name = "SMALLFONT", .xppaut_names = {"SMALL"}},
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

/* the rows run in the order of option_groups, and inside a group the unsectioned keys come first, then each
   section's rows together (W615); every .odex row has its key */
constexpr bool rows_in_order()
{
  std::size_t group = 0;
  std::string_view section_now;
  bool in_section = false;
  std::array<std::string_view, 64> closed{};
  std::size_t closed_count = 0;
  for (const OptionRow &row : rows) {
    if (row.odex.empty()) {
      if (!row.group.empty()) return false;
      continue;
    }
    if (row.group.empty()) return false;
    std::size_t g = 0;
    while (g < option_groups.size() && option_groups[g] != row.group) ++g;
    if (g == option_groups.size() || g < group) return false;
    const std::size_t dot = row.odex.find('.');
    const std::string_view section = dot == std::string_view::npos ? std::string_view() : row.odex.substr(0, dot);
    if (g != group) {
      group = g;
      in_section = false;
      closed_count = 0;
    }
    if (section.empty()) {
      if (in_section) return false;
      continue;
    }
    if (in_section && section == section_now) continue;
    for (std::size_t i = 0; i < closed_count; ++i)
      if (closed[i] == section) return false;
    if (in_section) closed[closed_count++] = section_now;
    section_now = section;
    in_section = true;
  }
  return true;
}
/* every .odex row says what it does, in one line */
constexpr bool rows_have_help()
{
  for (const OptionRow &row : rows)
    if (!row.odex.empty() && (row.help.empty() || row.help.size() > OPTION_HELP_MOST)) return false;
  return true;
}
static_assert(rows_have_help(), "every .odex row has a help line of at most OPTION_HELP_MOST characters");
/* a row with a .odex key takes its name from its group and key (option_name), never a name of its own */
constexpr bool grouped_rows_unnamed()
{
  for (const OptionRow &row : rows)
    if (!row.odex.empty() && !row.name.empty()) return false;
  return true;
}
static_assert(grouped_rows_unnamed(), "a row with a .odex key is named by its group and key, not a name of its own");
static_assert(rows_in_order(), "the option table runs in option_groups order, a group's sections each in one run");

/* the length of row's name that name starts with (and the digit after a
   numbered one), 0 when it does not */
std::size_t match_length(const OptionRow &row, std::string_view n, std::string_view name, int &index)
{
  if (n.empty() || name.size() < n.size() || !equal_ignoring_case(name.substr(0, n.size()), n)) return 0;
  if (!row.first_digit) return n.size();
  if (name.size() <= n.size()) return 0;
  const char d = name[n.size()];
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
  return *std::ranges::find_if(rows, [name](const OptionRow &r) { return r.once && option_name(r) == name; });
}

/* the pairs of settings that keep an order (model_options.h), named by their rows */
struct OptionPair {
  std::string_view lo, hi; /* row names (option_name) */
  bool strict;             /* lo must be below hi; else at most hi */
};
constexpr OptionPair pairs[] = {
  {"integration.step.min", "integration.step.max", false},
  {"plot.axes.x_low", "plot.axes.x_high", true}, {"plot.axes.y_low", "plot.axes.y_high", true},
  {"plot.axes.x_min", "plot.axes.x_max", true}, {"plot.axes.y_min", "plot.axes.y_max", true}, {"plot.axes.z_min", "plot.axes.z_max", true},
  {"analysis.histogram.low", "analysis.histogram.high", true}, {"analysis.histogram2.low", "analysis.histogram2.high", true},
  {"diagram.x_min", "diagram.x_max", true}, {"diagram.y_min", "diagram.y_max", true},
};

std::optional<std::string> order_problem(std::string_view lo_name, double lo, std::string_view hi_name, double hi, bool strict)
{
  if (strict ? lo < hi : lo <= hi) return std::nullopt;
  return xpp::format("{} ({}) must be {} {} ({})", lo_name, number(lo), strict ? "below" : "at most", hi_name, number(hi));
}

std::optional<PairProblem> option_pairs_problem(const std::function<std::optional<double>(const OptionRow &)> &value_of)
{
  for (const OptionPair &p : pairs) {
    const OptionRow &lo = once_option(p.lo), &hi = once_option(p.hi);
    const std::optional<double> a = value_of(lo), b = value_of(hi);
    if (!a || !b) continue;
    if (auto why = order_problem(p.lo, *a, p.hi, *b, p.strict)) return PairProblem{*why, p.lo, p.hi};
  }
  return std::nullopt;
}

std::optional<PairProblem> option_pairs_problem(const Session &s)
{
  return option_pairs_problem([&](const OptionRow &r) -> std::optional<double> { return r.real ? r.real(s) : r.whole(s); });
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
  case OptionRule::whole_at_least_two:
    return v == std::floor(v) && v >= 2 && v <= INT_MAX ? nullptr : "must be a whole number of at least 2";
  case OptionRule::any:
  case OptionRule::method:
  case OptionRule::choice:
    break;
  }
  return nullptr;
}

int choice_index(std::span<const std::string_view> choices, std::string_view text)
{
  const auto at = std::ranges::find(choices, text);
  return at == choices.end() ? -1 : static_cast<int>(at - choices.begin());
}

std::span<const OptionRow> option_rows() { return rows; }

const OptionRow *find_option(std::string_view name, int &index, bool xppaut_names)
{
  const OptionRow *best = nullptr;
  std::size_t best_length = 0;
  for (const OptionRow &row : rows) {
    const std::string own = option_name(row);
    for (std::string_view n : {std::string_view(own), xppaut_names ? row.xppaut_names[0] : "", xppaut_names ? row.xppaut_names[1] : ""}) {
      int i = 0;
      const std::size_t length = match_length(row, n, name, i);
      if (length > best_length) {
        best = &row;
        best_length = length;
        index = i;
      }
    }
  }
  return best;
}

/* a numbered row's digit, "" for the others */
static std::string digit_of(const OptionRow &row, int index)
{
  return row.first_digit ? std::string(1, static_cast<char>('0' + index)) : std::string();
}

std::string option_name(const OptionRow &row)
{
  if (row.odex.empty()) return std::string(row.name);
  return std::string(row.group) + "." + std::string(row.odex);
}

/* a row's .odex key: its `odex` key, the digit of a numbered row */
std::string option_label(const OptionRow &row)
{
  if (!row.label.empty()) return std::string(row.label);
  std::string label(row.odex);
  std::ranges::replace(label, '_', ' ');
  std::ranges::replace(label, '.', ' ');
  if (!label.empty()) label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
  return label;
}

std::string odex_key(const OptionRow &row, int index)
{
  return std::string(row.odex) + digit_of(row, index);
}

std::string option_setting_name(const OptionRow &row, int index)
{
  return option_name(row) + digit_of(row, index);
}

std::string odex_option_setting(std::string_view group, std::string_view key)
{
  int index = 0;
  const OptionRow *row = find_odex_option(group, key, index);
  return row ? option_setting_name(*row, index) : std::string();
}

std::string odex_option_key(std::string_view key)
{
  int index = 0;
  const OptionRow *row = find_option(key, index, true);
  return row && !row->odex.empty() ? odex_key(*row, index) : std::string();
}

bool set_holds(const OptionRow &row)
{
  return !row.wide && std::ranges::find(set_groups, row.group) != set_groups.end();
}

namespace {

/* the row key (a numbered row's digit included) names, and the digit; nullptr when none does.
   group: the row's group, or nullopt for a row of any group (a hint that names the right one) */
const OptionRow *find_by_odex_key(const std::optional<std::string_view> &group, std::string_view key, int &index)
{
  for (const OptionRow &row : rows) {
    if (row.odex.empty()) continue;
    if (group && (row.top_level ? !group->empty() : row.group != *group)) continue;
    if (!row.first_digit) {
      if (row.odex != key) continue;
      index = 0;
      return &row;
    }
    if (key.size() != row.odex.size() + 1 || key.substr(0, row.odex.size()) != row.odex) continue;
    const char d = key.back();
    if (d < row.first_digit || d > row.last_digit) continue;
    index = d - '0';
    return &row;
  }
  return nullptr;
}

} // namespace

const OptionRow *find_odex_option(std::string_view group, std::string_view key, int &index)
{
  return find_by_odex_key(group, key, index);
}

const OptionRow *find_odex_option_anywhere(std::string_view key, int &index)
{
  return find_by_odex_key(std::nullopt, key, index);
}

const OptionRow &curves_option()
{
  return *std::ranges::find(rows, OptionKind::curves, &OptionRow::kind);
}

const OptionRow &slider_presets_option()
{
  return *std::ranges::find(rows, OptionKind::sliders, &OptionRow::kind);
}

bool xppaut_slider_option(const OptionRow &row)
{
  return std::ranges::contains(slider_slot_parsers, row.parse);
}

bool option_written_quoted(const OptionRow &row)
{
  return row.kind == OptionKind::path || row.kind == OptionKind::text || row.kind == OptionKind::color;
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
  if (choice_index(row.choices, value) >= 0) return std::string(value);
  if (row.foreign) return row.foreign(value);
  if (row.boolean) return foreign_boolean(value);
  if (row.rule == OptionRule::choice) return choice_index(row.choices, value) < 0 ? std::string() : std::string(value);
  return first_letter_word(option_choices(row), value);
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

/* why a value is none of the words of a choice row: "not false or true" */
std::string choice_problem(const OptionRow &row)
{
  std::string why = "not ";
  for (std::size_t i = 0; i < row.choices.size(); ++i)
    why += (i ? (i + 1 == row.choices.size() ? " or " : ", ") : "") + std::string(row.choices[i]);
  return why;
}

/* option name set to value (apply), or only checked; why it is not one,
   empty when it is (or another source set it, which set_option leaves) */
std::optional<std::string> option_value(Session &s, std::string_view name, std::string_view value, bool force, const OptionsSet *mask, bool apply,
                                        bool xppaut_names)
{
  int index = 0;
  const OptionRow *row = find_option(name, index, xppaut_names);
  if (!row) return unknown_option;
  const bool importing = Load::running() && files::has_extension(Load::place().file, ".ode");
  const std::optional<double> total = importing ? imported_total(*row, value) : std::nullopt;
  /* the value as a NUL-ended text */
  const std::string text = total ? number(*total) : std::string(value);
  const OptionSource source{force, mask};
  const OptionValue v{text.c_str(), index, source, apply};
  const char *why = nullptr;
  std::string choice_why, color_why; /* why, when it is a choice's or a colour's */
  /* Check the row's shared numeric bounds before its parser can apply a value. */
  double x = 0;
  const char *refused = parse_number(text, x) ? rule_problem(row->rule, x, row->maximum) : nullptr;
  if (std::string autox_why; row->autox_numeric >= 0 && parse_number(text, x) && !autox_settings_num_ok(row->autox_numeric, x, autox_why)) return autox_why;
  if (row->zero_or_one && text != "0" && text != "1") why = "not 0 or 1";
  else if (row->once && !source.may(s, *row)) return std::nullopt;
  else if (refused) why = refused;
  else if (row->rule == OptionRule::method) {
    const Place place = Load::running() ? Load::place() : command_place();
    const auto picked = importing || xppaut_names ? pick_method(s.model(), text, place) : method_by_key(s.model(), text, place);
    if (!picked) return picked.error().what;
    if (apply) s.numerics.method = *picked;
  }
  else if (row->expand) {
    /* an XPPAUT option that is several of ours: each is set as written, by its own name */
    const ForeignOptions ours = row->expand(text);
    if (ours.why) return std::string(ours.why);
    for (const auto &[key, value] : ours.items)
      if (const auto why = option_value(s, key, value, force, mask, apply, false)) return why;
  }
  else if (row->kind == OptionKind::curves) {
    const auto curves = odex::read_block("options", s.model().this_file, "curves: " + text, false,
      [&](odex::Block &block) { return read_starting_curves(&s, block); });
    if (!curves) return curves.error().what;
    if (apply) s.plot_settings.curves = *curves;
  }
  else if (row->kind == OptionKind::sliders) {
    const auto sliders = odex::read_block("options", s.model().this_file, "sliders: " + text, false,
      [&](odex::Block &block) { return read_sliders(s.model(), block); });
    if (!sliders) return sliders.error().what;
    if (apply) s.sliders = *sliders;
  }
  else if (row->parse) {
    why = row->parse(s, v);
  }
  else if (row->color) why = color_into(row->color(s), *row, v, importing || xppaut_names, color_why);
  else if (row->boolean || row->index) {
    /* XPPAUT's spellings (y, 1, s, ...) belong to a foreign file, never to our own */
    const std::string word = importing || xppaut_names ? option_choice(*row, text) : text;
    const int i = choice_index(row->choices, word);
    if (i < 0) {
      choice_why = choice_problem(*row);
      why = choice_why.c_str();
    }
    else if (apply && row->boolean) row->boolean(s) = i != 0;
    else if (apply) row->index.write(s, i);
  }
  else if (row->real) why = number_into(row->real(s), v);
  else if (row->whole) why = whole_into(row->whole(s), v);
  else if (row->text && apply) row->text(s) = text;
  if (!why && apply && row->once) s.options_set.mark(*row);
  return why ? std::optional<std::string>(why) : std::nullopt;
}

} // namespace

std::vector<XppSlider> read_sliders(const Model &m, xpp::odex::Block &file, bool check_names)
{
    std::vector<xpp::odex::Block> items;
    file.groups("sliders", items, odex::max_file_tokens);
    std::vector<XppSlider> sliders;
    for (xpp::odex::Block &g : items) {
        xpp::XppSlider slider;
        for (std::string_view key : {"name", "low", "high"})
            if (!g.has(key)) g.fail(xpp::format("slider preset requires {}", key));
        slider.var = g.name_of(*g.member("name"));
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


std::vector<PlotCurve> read_starting_curves(const Session *s, xpp::odex::Block &file)
{
    std::vector<xpp::odex::Block> items;
    file.groups("curves", items, static_cast<std::size_t>(max_plot_curves));
    if (items.empty()) file.fail("curves holds at least one curve");
    std::vector<PlotCurve> curves;
    for (xpp::odex::Block &g : items) {
        PlotCurve curve;
        const std::array<std::pair<std::string_view, int PlotCurve::*>, 3> axes{{{"x", &PlotCurve::x}, {"y", &PlotCurve::y}, {"z", &PlotCurve::z}}};
        for (const auto &[key, member] : axes) {
            if (!g.has(key)) continue;
            const xpp::odex::Member &m = *g.member(key);
            if (!s) continue; /* the names are the model's: checked when the options are applied */
            int column = -1;
            find_variable(*s, g.name_of(m), &column);
            if (column < 0) g.fail(m, "no such variable");
            curve.*member = column;
        }
        g.done();
        curves.push_back(curve);
    }
    return curves;
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
    xpp::log(XPP_LOG_WARN, "Option {} not recognized\n", name);
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
