#pragma once

#include "solver.h"

#include <array>
#include <initializer_list>

#include <cstddef>
#include <string>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace xpp {

/* the numerics' value types: the words of each are model_options.cpp's
   (poincare_map_names, section_sign_names), the number of a word its
   position there */
/* the Poincare map's kind: a section, the extrema or the period of a variable */
enum class PoincareMap { none, section, max, period };
/* which crossings of the section count: those going down, either way, or up
   (the sign of the crossing's direction) */
enum class SectionSign { negative = -1, both = 0, positive = 1 };
/* What the independent subsystems are split by (W433, Model::subsystems): the
   equations alone, or also the run, where a Poincare section or stop makes one
   variable decide every other variable's run */
enum class SubsystemSplit { equations, run };
inline constexpr std::array<std::string_view, 2> subsystem_split_names{"equations", "run"};
/* What the data store keeps each value as (W613, storage.h): the integrator's value rounded to a float, XPPAUT's own, or
   the double itself; the words are the .odex's `storage_precision` */
enum class StoragePrecision { single, double_ };
inline constexpr std::array<std::string_view, 2> storage_precision_names{"single", "double"};
/* the sign a number's own sign says (XPPAUT's +1, -1 and 0) */
constexpr SectionSign section_sign(double x) { return x < 0 ? SectionSign::negative : x == 0 ? SectionSign::both : SectionSign::positive; }
/* the crossings' words, by the sign's number plus 1 (negative, both, positive); the .odex's and the commands' */
inline constexpr std::array<std::string_view, 3> section_sign_names{"negative", "both", "positive"};

/* The equilibria's settings (the nUmerics menu's Sing pt ctrl, the options'
   `equilibria` group): Newton's tolerance and iterations, the Jacobian's
   step and the limit past which a search gives up, and what a search shows
   beside the equilibrium it finds (off: the search asks nothing) */
struct EquilibriaSettings {
  double newton_tolerance = 0, jacobian_step = 0, search_limit = 0;
  int max_iterations = 0;
  bool print_eigenvalues = false, draw_invariant_sets = false, draw_strong_sets = false;
};

/* The numerics settings in use (the nUmerics menu, the model's @ options,
   the command line), a Session's (session.h) */
struct NumericsSettings {
  /* the time span: from start_time for total_time, step delta_t, every store_every-th step
     stored once |t| is past store_from */
  double start_time = 0, total_time = 0, delta_t = 0, store_from = 0;
  int store_every = 0;
  /* what the data store keeps each value as (W613); the store itself converts when it changes (storage_precision_changed) */
  StoragePrecision storage_precision = StoragePrecision::single;
  /* the method and its step bounds, tolerances, the limit on the variables
     (and whether passing it ends the run) and the delays' maximum */
  method::Id method = method::DISCRETE;
  double min_step = 0, max_step = 0, relative_tolerance = 0, absolute_tolerance = 0, state_limit = 0, delay = 0;
  bool stop_at_limit = true;
  /* the equations compiled at load (MIR, W258, W714); false runs the interpreter (read when a model loads) */
  bool compile = true;
  /* the Volterra equations' memory, in points */
  int max_points = 0;
  EquilibriaSettings equilibria;
  /* the boundary value solver */
  double bvp_epsilon = 0, bvp_tolerance = 0;
  int bvp_max_iterates = 0;
  /* the backward Euler's Newton solve */
  double eul_tol = 0;
  int max_eul_iter = 0;
  /* the nullclines' mesh */
  int nullcline_mesh = 0;
  /* the Poincare map: its kind, the section's variable, sign and plane,
     extrema, stop on section */
  PoincareMap poincare_map = PoincareMap::none;
  int poincare_variable = 0, poiext = 0;
  SectionSign poincare_sign = SectionSign::both;
  bool poincare_stop = false;
  /* the independent subsystems the reports show (Model::subsystems is the equations' own) */
  SubsystemSplit subsystem_split = SubsystemSplit::equations;
  double poincare_plane = 0;
  /* the torus and its period (each variable's flag is Session::itor) */
  bool torus = false;
  double torus_period = 6.2831853071795864770;
  /* the random numbers' seed: rand_seed is the NEXT run's (shown/set by
     "@ seed=", Stochastic > New seed, --newseed; W71's "a seed per run"),
     applied and logged when that run starts (xpp_math.cpp Random::seed); the
     seed a run actually used is last_seed, empty before any run, for the
     protocol's state and a saved data file's header/metadata. Once used,
     rand_seed is redrawn from a separate seed stream seeded by it
     (xpp::next_seed), so an untouched field still gives fresh noise next
     time. */
  int rand_seed = 12345678;
  std::optional<int> last_seed;
  /* CVODE's banded Jacobian (the nUmerics menu's Stiff settings) */
  bool cv_bandflag = false;
  int cv_bandupper = 1, cv_bandlower = 1;
  /* how every Jacobian of the model is formed: JACOBIAN_DIFFERENCE or
     JACOBIAN_EXACT (jacobian.h, W260) */
  int jacobian = 0;
  /* the global flags' crossing tolerance */
  double stol = 1.e-10;
  /* recompute the tables and kernels when a parameter changes
     (Numerics' AutoEval, @ autoeval=) */
  bool auto_evaluate = false;
  /* a run's state: data were stored (inflag), storing is on (storflag),
     a range stops (endsing), pauses
     between runs (pauser), shoots (shoot) or follows its parameter
     (par_fol); the browser shows a Fourier transform (fft) or a
     histogram (hist); nullclines were computed (null_here) */
  bool inflag = false, storflag = false, endsing = false, pauser = false;
  bool shoot = false, par_fol = false, fft = false, hist = false, null_here = false;
};


struct Session; /* session.h */
struct OptionRow; /* model_options.h */
struct Place; /* xpp_error.h */
struct Model; /* model.h */

/* The main numerics as data (W106, docs/protocol.md "The numerics as
   data"): what the Numerics menu's items that ask for a value edit (Total,
   Start time, tRansient, Dt, Ncline mesh, Sing pt ctrl, store every, Bounds,
   Method and its tolerances, dElay, bndVal), sent as the "numerics" event
   and set by `set` with kind `num`, so a front end edits them in fields of
   its own. The menu (the keyboard, scripts) asks through the same
   checks (numerics_settings_ask): a bad value changes nothing and says
   why, whichever way it came. The fields are the option table's
   numerics rows (model_options.h). numerics_settings.cpp; nothing
   escapes it. */

/* the event: {"ev":"numerics","fields":[...]} with the values now */
typedef void (*NumericsSettingsEmit)(const char *line, size_t len);
void numerics_settings_init(NumericsSettingsEmit emit);

/* {"cmd":"data"} with or without "numerics": sent (s's) at once, and
   then at each update, whatever it holds */
void numerics_settings_subscribe(const Session &s, int on);
/* send s's event if the values changed since the one sent last */
void numerics_settings_update(const Session &s);

/* whether numerics_settings_set would take `text` for field `key`, changing
   nothing: 0, or -1 with the same why */
int numerics_settings_check(Session &s, std::string_view key, std::string_view text, std::string &why);

/* set s's field `key` (the event's keys: total, dt, method, ...) to `text`
   (a number; for method a name, as the event's choices, or its number),
   then apply the numerics as leaving the Numerics menu does (do_meth): 0
   when set, -1 with why (a sentence naming the field) when not. Nothing
   is thrown. A menu supplies the command place saved before asking. */
int numerics_settings_set(Session &s, std::string_view key, std::string_view text, std::string &why, const Place *place = nullptr);

/* the choices of choice row f (the method, the Jacobian) that s's model cannot have, as a JSON object of each one's name and why:
   what the "numerics" event's and the panels' `unavailable` say; "" when the model can have them all */
std::string unavailable_choices_json(const Session &s, const OptionRow &f);

/* ---- the settings a session holds (session_settings.h) ----
   The option table's rows marked `saved`, in the table's order; each is read and written as text under its
   group and .odex key. A repeatable row's text is its items, one a line. */
std::vector<const OptionRow *> saved_settings();
/* the saved row whose name (option_name) is name, nullptr when none is */
const OptionRow *saved_setting(std::string_view name);
/* whether s's model, run by method, has the setting (the delay of a model
   with delays, the Volterra points of an integral-equation method); one it
   has not is not written and not accepted */
bool setting_applies(const Session &s, const OptionRow &row, int method);
/* s's value of row as text: a number, the method's or a choice's word, a
   variable's name, a name */
std::string setting_text(const Session &s, const OptionRow &row);
/* the saved settings keys with s's values, for a message that names what
   caused it (W480): "min_step = 1e-12, relative_tolerance = 0.001" */
std::string named_settings(const Session &s, std::initializer_list<std::string_view> keys);
/* where an error about settings is: the line the model sets the first of them on, else the
   command that ran the computation */
Place settings_place(const Model &m, std::initializer_list<std::string_view> settings);
/* why text is not a value of row for s's model (the rule the Numerics menu, `set num` and the @ lines check; the first
   bad line of a repeatable row's list), nothing when it is; changes nothing */
std::optional<std::string> setting_problem(Session &s, const OptionRow &row, std::string_view text);
/* row set to text, already checked by setting_problem: the member only
   (the caller applies the numerics as a whole: do_meth, the delays); a repeatable row is emptied (its reset), then
   given each line of text */
void setting_write(Session &s, const OptionRow &row, std::string_view text);

/* one value of numerics_settings_set_all: the field's key and the text a user would type */
struct NumericsValue {
  std::string_view key;
  std::string text;
};
/* set every value, all or nothing: each is checked first, then all are written together
   (an order the settings keep, as min_step at most max_step, is judged on the result, so a
   pair given together may cross the old values) and applied once. 0 when set, -1 with why
   (a sentence naming the field) and nothing changed. */
/* Check the proposed values and their pair rules without changing the session. */
int numerics_settings_check_all(Session &s, std::span<const NumericsValue> values, std::string &why, const Place *place = nullptr);
int numerics_settings_set_all(Session &s, std::span<const NumericsValue> values, std::string &why, const Place *place = nullptr);
/* field `key`'s value now as JSON text (a number, true or false, or a choice's quoted word); empty for no such field */
std::string numerics_settings_json_value(const Session &s, std::string_view key);


} // namespace xpp
