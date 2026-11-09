#pragma once

/* The model's options as one table (W119): every @ option's names, the
   member it sets, its default and, for the main numerics, the label and
   check the Numerics menu and the protocol's `set num` use. What reads it:
   set_option (an "@ name=value" of the model, .xpprc, the command line or
   an internal set), the defaults a load gives what no source set
   (set_option_defaults), and numerics_settings.cpp (the "numerics" event,
   `set num` and the menu's questions). model_options.cpp. */

#include <array>
#include "colormap.h"
#include <cstddef>
#include <functional>
#include <type_traits>
#include <span>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xpp {

struct Session; /* session.h */
struct Model;   /* model.h */

struct OptionRow;
struct XppSlider;
struct PlotCurve; /* load_eqn.h */
namespace odex { class Block; }
/* Shared preset/session reader. Before model compilation, check_names is false;
   the .odex loader checks names against declarations instead. */
std::vector<XppSlider> read_sliders(const Model &m, odex::Block &file, bool check_names = true);

namespace option_limit {
/* Ten million history samples allow large histories without int-sized requests. */
inline constexpr int history_points = 10000000;
/* A 4096-side mesh or histogram bounds quadratic work/storage to about 16 million cells. */
inline constexpr int mesh = 4096, histogram_bins = 4096;
/* A hundred thousand diagonals allow large banded models without int-sized bandwidths. */
inline constexpr int bandwidth = 100000;
/* One million spectral samples bound FFT workspaces while permitting long windows. */
inline constexpr int spectrum_samples = 1000000;
} // namespace option_limit

/* which options a source has set already (a Session's, session.h): the
   rows with `once` (OptionRow), each kept by its place in the table, so a
   new row needs no flag of its own */
class OptionsSet {
public:
  bool has(const OptionRow &row) const;
  void mark(const OptionRow &row);
private:
  std::vector<bool> bits_;
};

/* a row's member as `member([](auto &s) -> auto & { return s.x.y; })`: one
   generic lambda gives the read that takes the Session (a Session&, to
   write) and the one that takes a const Session& */
template <class T>
struct Member {
  T &(*write)(Session &) = nullptr;
  const T &(*read)(const Session &) = nullptr;
  explicit operator bool() const { return write != nullptr; }
  T &operator()(Session &s) const { return write(s); }
  const T &operator()(const Session &s) const { return read(s); }
};
template <class F>
constexpr auto member(F)
{
  using T = std::remove_cvref_t<std::invoke_result_t<F, Session &>>;
  return Member<T>{[](Session &s) -> T & { return F{}(s); }, [](const Session &s) -> const T & { return F{}(s); }};
}

/* the member of a choice row that is an enum, read and written as its number
   in the row's words: `choice_member([](auto &s) -> auto & { return s.x.y; })`.
   A word's number is the enum's value plus Offset (1 for SectionSign, whose
   first word, negative, is -1) */
struct ChoiceMember {
  int (*read)(const Session &) = nullptr;
  void (*write)(Session &, int) = nullptr;
  explicit operator bool() const { return read != nullptr; }
};
template <int Offset = 0, class F>
constexpr ChoiceMember choice_member(F)
{
  using T = std::remove_cvref_t<std::invoke_result_t<F, Session &>>;
  return {[](const Session &s) { return static_cast<int>(F{}(s)) + Offset; },
          [](Session &s, int i) { F{}(s) = static_cast<T>(i - Offset); }};
}

/* what the Numerics menu and `set num` accept for a numerics setting;
   allocation-sized rows also have an inclusive maximum */
/* (choice: one of the row's choices, by name or number) */
enum class OptionRule { any, nonzero, positive, nonnegative, whole_positive, whole_at_least_two, method, choice };
/* which methods use a numerics setting (xpp::SolverTraits), so a front end
   can say the others do not */
enum class OptionUse { always, relative_tolerance, absolute_tolerance, min_step, max_step, newton, delays, integral_history };

/* the source of the value set_option is applying: whether it may set an
   option another source (or an earlier line of its own, mask) set */
struct OptionSource {
  bool force;                /* an internal set, the command line: always */
  const OptionsSet *mask;    /* the options unset when this source began */
  /* row may be set from here */
  bool may(const Session &s, const OptionRow &row) const;
  /* row may be set from here; marked set when so */
  bool claim(Session &s, const OptionRow &row) const;
};

/* an option's value as its parser gets it */
struct OptionValue {
  const char *text;          /* as atof/atoi read it */
  int index;                 /* the digit after a numbered name (xp2: 2) */
  const OptionSource &source;
  /* the value is applied; false: only checked, the Session left as it is
     (option_problem), so a parser writes nothing then */
  bool apply;
};

/* The groups of a .odex's `options` block (docs/odex.md): each row of the
   table belongs to one. A row with no group (the X11 window's leftovers,
   a numerics setting the model cannot set) is no .odex option. */
namespace option_group {
inline constexpr std::string_view integration = "integration";
inline constexpr std::string_view poincare = "poincare";
inline constexpr std::string_view equilibria = "equilibria";
inline constexpr std::string_view sweep = "sweep";
inline constexpr std::string_view continuation = "continuation";
inline constexpr std::string_view diagram = "diagram";
inline constexpr std::string_view plot = "plot";
inline constexpr std::string_view analysis = "analysis";
inline constexpr std::string_view export_ = "export";
inline constexpr std::string_view interface = "interface";
inline constexpr std::string_view output = "output";
} // namespace option_group
/* Groups in workflow order: run, analyse the run, continue, draw, write (W614, W615). The option
   table's rows, a .odex's options block and every panel keep it. */
inline constexpr std::array<std::string_view, 11> option_groups{
    option_group::integration, option_group::poincare, option_group::equilibria, option_group::sweep, option_group::continuation, option_group::diagram, option_group::plot, option_group::analysis, option_group::export_, option_group::interface, option_group::output};

/* The groups a `set`'s `options:` may hold: each is one graph's or one run's
   own configuration. The rest are the model's or the session's as a whole
   (diagram's axes name a view, export and output write files, interface holds
   the buttons and sliders). */
inline constexpr std::array<std::string_view, 7> set_groups{
    option_group::integration, option_group::poincare, option_group::equilibria, option_group::continuation,
    option_group::plot, option_group::sweep, option_group::analysis};

/* what a .odex writes as an option's value: a number, a whole number, a
   bare word (a variable's, a parameter's, a name), one of the row's
   choices (option_choices), a path or a text, both in double quotes, a colour
   (always in double quotes) */
enum class OptionKind { number, whole, name, choice, path, text, color, sliders, curves };

/* which of a curve's options an XPPAUT row (xp, yp, zp, nplot and their numbered ones) is: the converter writes them as the `curves` list */
enum class CurveField { none, x, y, z, count };

/* what an XPPAUT option (import only) says in our own options: the keys and
   their values as a .odex writes them (a path unquoted), or why the value is
   none */
struct ForeignOptions {
  std::vector<std::pair<std::string, std::string>> items;
  const char *why = nullptr;
};

/* the longest help line of a row */
inline constexpr std::size_t OPTION_HELP_MOST = 120;

/* One option. Its names match as prefixes of the name given, upper case,
   the longest winning (so "method" and "methodx" are one, and "dt" is not
   "dtminimum"); a numbered one (xp2..xp8) is its name and one digit. */
struct OptionRow {
  /* the .odex options block's group and the kind of value it takes ("" and
     number for a row that is no .odex option); first, so a row names them
     before its name */
  std::string_view group;
  OptionKind kind = OptionKind::number;
  std::string_view name;          /* "" for a numerics setting the model cannot set */
  /* the key a .odex writes in the row's group (W615): snake_case, with a dotted section
     ("step.min", "axes.x_low") where the group is large; "" for a row that is no .odex option.
     The rows run in the order of option_groups, then their sections (a section's rows together,
     after the group's unsectioned ones), the order the panels show them. */
  std::string_view odex;
  /* what the option does, in one line (a sentence, no more than OPTION_HELP_MOST characters): the schema's description and the
     panels' help; every .odex row has it (optioncheck) */
  std::string_view help;
  /* XPPAUT's names for it, read in a .ode, an .xpprc and the command line
     (foreign formats) and refused in a .odex: "" when XPPAUT's name is the name */
  std::array<std::string_view, 2> xppaut_names;
  char first_digit = 0, last_digit = 0; /* numbered: the digits after name */
  /* a source sets it once: the first to set it keeps it (the command line,
     then the model, then .xpprc; a later @ line of the same source may set
     it again), and a load gives the default only to one nothing set. A row
     that may be given any number of times (fold=, but=, xp2=, ...) has no
     `once`. */
  bool once = false;
  /* an XPPAUT option (import only) that a `--convert` writes into a statement of the model, not into options (autoeval:
     each formula table's auto_evaluate): no key, and no warning */
  bool in_statement = false;
  /* an XPPAUT curve option (import only), or none */
  CurveField curve_field = CurveField::none;
  /* an XPPAUT option read on import only that a .odex has no key for: why it is dropped, said once as a warning
     (""; the converter says it) */
  std::string_view dropped;
  /* the member: a number (atof), a whole number (atoi) or text */
  Member<double> real;
  Member<int> whole;
  Member<bool> boolean;
  ChoiceMember index; /* a choice row's enum */
  Member<Color> color; /* a colour row's colour */
  Member<std::string> text;
  /* a row whose value is no one member's (a list of choices, special_points): its text as it is shown and written; `parse` sets it */
  std::string (*show)(const Session &) = nullptr;
  /* the value when it is not just the member's: nullptr when taken, else
     why not (logged, the option left unset) */
  const char *(*parse)(Session &, const OptionValue &) = nullptr;
  /* only "0" or "1", else the load fails */
  bool zero_or_one = false;
  /* the default a load gives it when no source set it, nullptr: the
     member's own initial value */
  void (*reset)(Session &) = nullptr;
  /* a choice row's word for the value XPPAUT's files spell otherwise (a first
     letter, a number), "" when it is none: a .ode, an .xpprc and the command
     line only, never our own files; nullptr: the row's words alone (a boolean's
     are y/n and 0/1) */
  std::string (*foreign)(std::string_view) = nullptr;
  /* the value an .ode that does not set the option is converted with (XPPAUT's own default, where ours is
     another: @ colormap, jet); empty: none, the option's default stays */
  std::string_view imported_default;
  /* an XPPAUT option that is several of ours (bound, ncdraw, lt): the options it sets */
  ForeignOptions (*expand)(std::string_view) = nullptr;
  /* a numerics setting: its key in the "numerics" event and `set num`,
     the menu's label, what it accepts and which methods use it */
  std::string_view key, label;
  OptionRule rule = OptionRule::any;
  /* autoX numeric rule index; the rule itself lives in autox_settings. */
  int autox_numeric = -1;
  int maximum = 0; /* 0: no allocation-sized upper bound; otherwise inclusive */
  OptionUse use = OptionUse::always;
  /* OptionRule::choice: the names of the values 0, 1, ..., and why a
     model cannot have value i ("" when it can) */
  std::span<const std::string_view> choices;
  std::string_view (*unavailable)(const Model &, int) = nullptr;
  /* makes what choosing value i needs, before `unavailable` is asked
     (the exact Jacobian, on its first need) */
  void (*prepare)(Session &, int) = nullptr;
  /* a .odex block may name it more than once (fold, but: each names one) */
  bool repeatable = false;
  /* a .setx holds its value (W284): the numerics group, or the range
     group, by the row's key. A choice row's value is the index of its
     word in `choices` (or, for the method, the solver's number). */
  bool saved = false;
  /* what a change of the numerics setting needs, after its member is
     written (the dt's steps, the delay's storage, the BVP's arrays) */
  void (*applied)(Session &) = nullptr;
  /* a setting of the session or the window as a whole, not one graph's or one
     run's own configuration: a set never holds it (sets-design.md section 2) */
  bool wide = false;
  /* A file companion belongs directly in options, outside the settings groups. */
  bool top_level = false;
};

/* why the number v is not one rule accepts ("must be a number above 0"),
   nullptr when it is: the one check of a numerics setting's value, for
   an @ line, the Numerics menu, `set num` and a set file alike
   (OptionRule::method's name is checked by numerics_settings.cpp) */
const char *rule_problem(OptionRule rule, double v, int maximum = 0);

/* the settings that keep an order (the lower end of a range below its upper
   end, the smallest step not above the largest): the one rule every source
   checks (an @ line, a settings file, the Numerics menu, `set num`, the
   window dialogs); the pairs are model_options.cpp's table */
/* why lo (named lo_name) and hi (named hi_name) break the order ("xlo (3) must be
   below xhi (1)"), nothing when they keep it */
std::optional<std::string> order_problem(std::string_view lo_name, double lo, std::string_view hi_name, double hi, bool strict);
/* the first pair value_of (a row's value, nothing when the source has none) breaks */
struct PairProblem {
  std::string text;
  std::string_view lo, hi; /* the pair's rows */
};
std::optional<PairProblem> option_pairs_problem(const std::function<std::optional<double>(const OptionRow &)> &value_of);
/* the same for the Session's values now */
std::optional<PairProblem> option_pairs_problem(const Session &s);

/* the value text names among choices (exactly), -1 when none */
int choice_index(std::span<const std::string_view> choices, std::string_view text);

/* the table, the numerics settings first, in the Numerics menu's order */
std::span<const OptionRow> option_rows();

/* the row whose name the option name (upper case) starts with, the longest
   such, and the digit after a numbered one; nullptr when none. xppaut_names:
   XPPAUT's own names for a row count too (a .ode, .xpprc or command line),
   as a .odex does not take them */
const OptionRow *find_option(std::string_view upper_name, int &index, bool xppaut_names);

/* the option key as a .odex writes it: the key of the row a name matches
   (XPPAUT's names and the longer spellings a prefix match takes too:
   NOUT is store_every, xplot is xp), "" when no row of a group does */
std::string odex_option_key(std::string_view key);

/* a row's label, as the panels and the schema show it: the numerics menu's own, else the key in words (underscores and dots
   as spaces, a capital first letter: "step.min" is "Step min") */
std::string option_label(const OptionRow &row);

/* the key a .odex writes for a row (its `odex` key, a numbered row's digit
   index after it) */
std::string odex_key(const OptionRow &row, int index);

/* the name a row goes by inside (a set's item, an @ line applied, set_option's name): its name in lower case, a
   numbered row's digit after it; the .odex key is the file's own word for it (odex_key) */
std::string option_setting_name(const OptionRow &row, int index);

/* the setting name of the row a .odex key names in group, "" when none does */
std::string odex_option_setting(std::string_view group, std::string_view key);

/* the row a .odex key names in group ("" for the options block itself), exactly (a numbered row's digit
   included), and the digit; nullptr when none does */
const OptionRow *find_odex_option(std::string_view group, std::string_view key, int &index);
/* the row a .odex key names in whichever group has it (to say which group a misplaced key belongs to) */
const OptionRow *find_odex_option_anywhere(std::string_view key, int &index);

/* a set may hold the row: its group is one of set_groups and it is no `wide` row */
bool set_holds(const OptionRow &row);

/* the curves of a model's `curves` list (a window's are setx_file.h's read_curves, with their colours and lines); the groups (x, y, z: a variable's name each); with a Session the names are
   checked and read as columns, without one only the shape of the list */
std::vector<PlotCurve> read_starting_curves(const Session *s, odex::Block &file);

/* the starting graph's `curves` row */
const OptionRow &curves_option();
/* the slider presets' row (`sliders`, a list) */
const OptionRow &slider_presets_option();
/* row is one of XPPAUT's numbered slider slots (S1-S3, SLO1-3, SHI1-3):
   no .odex option, but an .ode's, which the converter turns into presets */
bool xppaut_slider_option(const OptionRow &row);

/* whether a .odex writes row's value text in double quotes: a path, a text or a colour */
bool option_written_quoted(const OptionRow &row);

/* the words a choice row takes, in a .odex (the registry's keys for the
   method), empty for any other row */
std::vector<std::string> option_choices(const OptionRow &row);

/* the word of a choice row's value as the .ode, an .xpprc or the command line
   spell it (XPPAUT's), "" when it selects none; the method's is solver.h's pick_method */
std::string option_choice(const OptionRow &row, std::string_view value);

/* the JSON Schema of a .odex's options block, built from the table: what
   `xppautX --options-schema` prints and docs/odex-options.schema.json holds
   (tools/optionschema.sh checks they agree) */
std::string option_schema();
/* the members of a row's JSON Schema that bound its value, each led by a comma (`, "minimum": 1`): what its rule, its maximum and
   autoX's own rule say; the schema's and the panels' one source of a limit */
std::string option_limit_members(const OptionRow &row);
/* one row's value as a JSON Schema: what its kind and rule say (the options
   block's and a .setx's, setx_file.h) */
std::string option_value_schema(const OptionRow &row);

/* the row with `once` named name (upper case): for an option whose parser
   sets or claims another's */
const OptionRow &once_option(std::string_view name);

/* the numerics setting with key, nullptr when none */
const OptionRow *numerics_option(std::string_view key);

/* what an option's error says, in no file's syntax (the option may come
   from a .ode, a .odex's options block, a .setx or the command line); the
   error carries the file and line it was read from */
std::string option_error_text(std::string_view name, std::string_view value, std::string_view why);

/* option name set to value (force: even when another source set it; mask:
   the options this source may set again) */
void set_option(Session &s, std::string_view name, std::string_view value, bool force, const OptionsSet *mask);

/* why value is not one option name takes ("not an option", "not a
   number", ...), empty when it is: the checks set_option makes, nothing
   applied (an internal set checks every item before it applies one);
   xppaut_names as find_option's */
std::optional<std::string> option_problem(Session &s, std::string_view name, std::string_view value, bool xppaut_names);

/* each option no source set gets its default, in the table's order */
void set_option_defaults(Session &s);

/* XPPAUT's negative Total, read on import only: the run's length |Total|
   (XPPAUT runs that long; its sign only let a full store overwrite itself,
   W347); nothing for any other row or value */
std::optional<double> imported_total(const OptionRow &row, std::string_view value);

} // namespace xpp
