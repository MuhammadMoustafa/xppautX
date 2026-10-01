#ifndef XPP_MODEL_OPTIONS_H
#define XPP_MODEL_OPTIONS_H

/* The model's options as one table (W119): every @ option's names, the
   member it sets, its default and, for the main numerics, the label and
   check the Numerics menu and the protocol's `set num` use. What reads it:
   set_option (an "@ name=value" of the model, .xpprc, the command line or
   an internal set), the defaults a load gives what no source set
   (set_option_defaults), and numerics_settings.cpp (the "numerics" event,
   `set num` and the menu's questions). model_options.cpp. */

#include <bitset>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace xpp {

struct Session; /* session.h */

/* An option that a source sets once: the first to set it keeps it
   (the command line, then the model, then .xpprc; a later @ line of the
   same source may set it again), and a load gives the default only to
   one nothing set. An option that may be given any number of times (fold=,
   but=, xp2=, ...) has none. */
enum class Option : unsigned char {
  none,
  /* the numerics */
  TOTAL, T0, TRANS, DT, NMESH, NEWT_ITER, NEWT_TOL, JAC_EPS, NOUT, BOUND,
  METH, TOL, DTMIN, DTMAX, ATOL, DELAY, VMAXPTS, MAXSTOR, TOR_PER, BANDUP,
  BANDLO, POIMAP, POIVAR, POISGN, POISTOP, POIPLN, SEED, STOCH, AUTOEVAL,
  /* the plot */
  XP, YP, ZP, NPLOT, AXES, XLO, XHI, YLO, YHI, XMIN, XMAX, YMIN, YMAX,
  ZMIN, ZMAX, PHI, THETA, LT, YNC, XNC, SMC, UMC, COLORMAP, PLOTFMT,
  PS_FONT, PS_LW, PS_FSIZE, PS_COLOR, DFGRID, DFDRAW, NCDRAW, COLORVIA,
  COLORIZE, COLORLO, COLORHI, S1, S2, S3, SLO1, SLO2, SLO3, SHI1, SHI2,
  SHI3,
  /* the range integration and a batch run */
  RANGEOVER, RANGESTEP, RANGELOW, RANGEHIGH, RANGERESET, RANGEOLDIC, RANGE,
  OUTPUT, RUNNOW, TUTORIAL,
  /* AUTO */
  NTST, NMAX, NPR, NCOL, DSMIN, DSMAX, DS, PARMIN, PARMAX, NORMMIN,
  NORMMAX, EPSL, EPSU, EPSS, AUTOXMAX, AUTOYMAX, AUTOXMIN, AUTOYMIN,
  AUTOVAR, SEC, UEC, SPC, UPC,
  /* the histogram and the spectrum a batch run writes */
  POSTPROCESS, HISTLO, HISTHI, HISTBINS, HISTCOL, HISTLO2, HISTHI2,
  HISTBINS2, HISTCOL2, SPECCOL, SPECCOL2, SPECWIDTH, SPECWIN,
  count
};

/* which options a source has set already (a Session's, session.h) */
class OptionsSet {
public:
  bool has(Option o) const { return bits_[static_cast<std::size_t>(o)]; }
  void mark(Option o) { bits_.set(static_cast<std::size_t>(o)); }
private:
  std::bitset<static_cast<std::size_t>(Option::count)> bits_;
};

/* what the Numerics menu and `set num` accept for a numerics setting */
enum class OptionRule { any, nonzero, positive, nonnegative, whole_positive, method };
/* which methods use a numerics setting (xpp::SolverTraits), so a front end
   can say the others do not */
enum class OptionUse { always, step_or_rel, step, rel, newton, delays };

/* the source of the value set_option is applying: whether it may set an
   option another source (or an earlier line of its own, mask) set */
struct OptionSource {
  bool force;                /* an internal set, the command line: always */
  const OptionsSet *mask;    /* the options unset when this source began */
  /* o may be set from here */
  bool may(const Session &s, Option o) const;
  /* o may be set from here; marked set when so */
  bool claim(Session &s, Option o) const;
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

/* One option. Its names match as prefixes of the name given, upper case,
   the longest winning (so "meth" and "method" are one, and "dt" is not
   "dtmin"); a numbered one (xp2..xp8) is its name and one digit. */
struct OptionRow {
  std::string_view name;          /* "" for a numerics setting the model cannot set */
  std::string_view alias;         /* a second name, or "" */
  char first_digit = 0, last_digit = 0; /* numbered: the digits after name */
  Option flag = Option::none;
  /* the member: a number (atof), a whole number (atoi) or text */
  double &(*real)(Session &) = nullptr;
  int &(*whole)(Session &) = nullptr;
  std::string &(*text)(Session &) = nullptr;
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
  OptionUse use = OptionUse::always;
};

/* why the number v is not one rule accepts ("must be a number above 0"),
   nullptr when it is: the one check of a numerics setting's value, for
   an @ line, the Numerics menu, `set num` and a set file alike
   (OptionRule::method's name is checked by numerics_settings.cpp) */
const char *rule_problem(OptionRule rule, double v);

/* the table, the numerics settings first, in the Numerics menu's order */
std::span<const OptionRow> option_rows();

/* the row whose name the option name (upper case) starts with, the longest
   such, and the digit after a numbered one; nullptr when none */
const OptionRow *find_option(std::string_view upper_name, int &index);

/* the numerics setting with key, nullptr when none */
const OptionRow *numerics_option(std::string_view key);

/* option name set to value (force: even when another source set it; mask:
   the options this source may set again) */
void set_option(Session &s, std::string_view name, std::string_view value, bool force, const OptionsSet *mask);

/* why value is not one option name takes ("not an option", "not a
   number", ...), nullptr when it is: the checks set_option makes, nothing
   applied (an internal set checks every item before it applies one) */
const char *option_problem(Session &s, std::string_view name, std::string_view value);

/* each option no source set gets its default, in the table's order */
void set_option_defaults(Session &s);

} // namespace xpp
#endif
