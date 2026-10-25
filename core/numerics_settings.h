#pragma once

#include <initializer_list>

#include <cstddef>
#include <string>
#include <optional>
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
/* the sign a number's own sign says (XPPAUT's +1, -1 and 0) */
constexpr SectionSign section_sign(double x) { return x < 0 ? SectionSign::negative : x == 0 ? SectionSign::both : SectionSign::positive; }

struct Session; /* session.h */
struct OptionRow; /* model_options.h */
struct Place; /* xpp_error.h */

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

/* ---- the settings a .setx holds (W284, setx.h) ----
   The option table's rows marked `saved`, by the group of the file they
   are in ("numerics", which also takes the keyed rows with no group, or
   "range"); each is read and written as text by its key. */
std::vector<const OptionRow *> saved_settings(std::string_view group);
/* the saved row whose .setx key is key, nullptr when none is */
const OptionRow *saved_setting(std::string_view key);
/* the key a .setx gives row: its numerics key, else its name in lower case */
std::string setting_key(const OptionRow &row);
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
/* why text is not a value of row for s's model (the rule the Numerics menu,
   `set num` and the @ lines check), nothing when it is; changes nothing */
std::optional<std::string> setting_problem(Session &s, const OptionRow &row, std::string_view text);
/* row set to text, already checked by setting_problem: the member only
   (the caller applies the numerics as a whole: do_meth, the delays) */
void setting_write(Session &s, const OptionRow &row, std::string_view text);

/* the Numerics menu's question for s's field `key` (not method, which the
   menu picks from its list): its label, the value now, and the answer
   set as numerics_settings_set sets it, a refusal shown with err_msg.
   True when set; false when cancelled or refused. */
bool numerics_settings_ask(Session &s, std::string_view key);

} // namespace xpp
