#ifndef XPP_MODEL_OPTIONS_H
#define XPP_MODEL_OPTIONS_H

/* The model's options as one table (W119): every @ option's names, the
   member it sets, its default and, for the main numerics, the label and
   check the Numerics menu and the protocol's `set num` use. What reads it:
   set_option (an "@ name=value" of the model, .xpprc, the command line or
   an internal set), the defaults a load gives what no source set
   (set_option_defaults), and numerics_settings.cpp (the "numerics" event,
   `set num` and the menu's questions). model_options.cpp. */

#include <array>
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
namespace setx { class Block; }
/* Shared preset/session reader. Before model compilation, check_names is false;
   the .odex loader checks names against declarations instead. */
std::vector<XppSlider> read_sliders(const Model &m, setx::Block &file, bool check_names = true);

namespace option_limit {
/* Ten million initial rows/history samples allow large recordings without int-sized requests. */
inline constexpr int storage_rows = 10000000, history_points = 10000000;
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

/* what the Numerics menu and `set num` accept for a numerics setting;
   allocation-sized rows also have an inclusive maximum */
/* (choice: one of the row's choices, by name or number) */
enum class OptionRule { any, nonzero, positive, nonnegative, whole_positive, method, choice };
/* which methods use a numerics setting (xpp::SolverTraits), so a front end
   can say the others do not */
enum class OptionUse { always, step_or_rel, step, rel, newton, delays, integral_history };

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
inline constexpr std::string_view continuation = "continuation";
inline constexpr std::string_view diagram = "diagram";
inline constexpr std::string_view plot = "plot";
inline constexpr std::string_view export_ = "export";
inline constexpr std::string_view sweep = "sweep";
inline constexpr std::string_view analysis = "analysis";
inline constexpr std::string_view interface = "interface";
inline constexpr std::string_view output = "output";
} // namespace option_group
/* Groups follow the work they affect, in converter output order (W317). */
inline constexpr std::array<std::string_view, 11> option_groups{
    option_group::integration, option_group::poincare, option_group::equilibria, option_group::continuation, option_group::diagram, option_group::plot, option_group::export_, option_group::sweep, option_group::analysis, option_group::interface, option_group::output};

/* what a .odex writes as an option's value: a number, a whole number, a
   bare word (a variable's, a parameter's, a name), one of the row's
   choices (option_choices), a path or a text, both in double quotes */
enum class OptionKind { number, whole, name, choice, path, text, sliders };

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
  std::string_view alias;         /* a second name, or "" */
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
  /* the member: a number (atof), a whole number (atoi) or text */
  Member<double> real;
  Member<int> whole;
  Member<std::string> text;
  /* the value when it is not just the member's: nullptr when taken, else
     why not (logged, the option left unset) */
  const char *(*parse)(Session &, const OptionValue &) = nullptr;
  /* only "0" or "1", else the load fails */
  bool zero_or_one = false;
  /* the default a load gives it when no source set it, nullptr: the
     member's own initial value */
  void (*reset)(Session &) = nullptr;
  /* a numerics setting: its key in the "numerics" event and `set num`,
     the menu's label, what it accepts and which methods use it */
  std::string_view key, label;
  OptionRule rule = OptionRule::any;
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
  /* the value's sign says "forever" (the total time) */
  bool signed_forever = false;
};

/* why the number v is not one rule accepts ("must be a number above 0"),
   nullptr when it is: the one check of a numerics setting's value, for
   an @ line, the Numerics menu, `set num` and a set file alike
   (OptionRule::method's name is checked by numerics_settings.cpp) */
const char *rule_problem(OptionRule rule, double v, int maximum = 0);

/* the value text names among choices (its name in any case, or its
   number), -1 when none */
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

/* the key a .odex writes for a row (its name in lower case, a numbered
   row's digit index after it) */
std::string odex_key(const OptionRow &row, int index);

/* the row a .odex key names, exactly (lower case, a numbered row's digit
   included), and the digit; nullptr when none does */
const OptionRow *find_odex_option(std::string_view key, int &index);

/* the words a choice row takes, in a .odex (the registry's keys for the
   method), empty for any other row */
std::vector<std::string> option_choices(const OptionRow &row);

/* the word of a choice row's value as the .ode, an .xpprc or the menus give
   it (the Jacobian's by name or number, the others by the first letter), ""
   when it selects none; the method's is solver.h's pick_method */
std::string option_choice(const OptionRow &row, std::string_view value);

/* the JSON Schema of a .odex's options block, built from the table: what
   `xppautX --options-schema` prints and docs/odex-options.schema.json holds
   (tools/optionschema.sh checks they agree) */
std::string option_schema();
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

} // namespace xpp
#endif
