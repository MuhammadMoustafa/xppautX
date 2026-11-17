#pragma once


#include <array>
#include <initializer_list>

#include <cstddef>
#include <string>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace xpp {

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
   numerics rows (model_options.h). numerics_options.cpp; nothing
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

/* whether s's model, run by method, has the option (the delay of a model
   with delays, the Volterra points of an integral-equation method); one it
   has not is not written and not accepted */
bool setting_applies(const Session &s, const OptionRow &row, int method);
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
   (an order the options keep, as min_step at most max_step, is judged on the result, so a
   pair given together may cross the old values) and applied once. 0 when set, -1 with why
   (a sentence naming the field) and nothing changed. */
/* Check the proposed values and their pair rules without changing the session. */
int numerics_settings_check_all(Session &s, std::span<const NumericsValue> values, std::string &why, const Place *place = nullptr);
int numerics_settings_set_all(Session &s, std::span<const NumericsValue> values, std::string &why, const Place *place = nullptr);
/* field `key`'s value now as JSON text (a number, true or false, or a choice's quoted word); empty for no such field */
std::string numerics_settings_json_value(const Session &s, std::string_view key);


} // namespace xpp
