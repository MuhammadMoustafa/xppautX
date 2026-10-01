#ifndef NUMERICS_SETTINGS_H
#define NUMERICS_SETTINGS_H

#include <cstddef>
#include <string>
#include <string_view>

namespace xpp {

struct Session; /* session.h */

/* The main numerics as data (W106, docs/protocol.md "The numerics as
   data"): what the Numerics menu's items that ask for a value edit (Total,
   Start time, tRansient, Dt, Ncline mesh, Sing pt ctrl, nOutput, Bounds,
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

/* set s's field `key` (the event's keys: total, dt, method, ...) to `text`
   (a number; for method a name, as the event's choices, or its number),
   then apply the numerics as leaving the Numerics menu does (do_meth): 0
   when set, -1 with why (a sentence naming the field) when not. Nothing
   is thrown. */
int numerics_settings_set(Session &s, std::string_view key, std::string_view text, std::string &why);

/* the Numerics menu's question for s's field `key` (not method, which the
   menu picks from its list): its label, the value now, and the answer
   set as numerics_settings_set sets it, a refusal shown with err_msg.
   True when set; false when cancelled or refused. */
bool numerics_settings_ask(Session &s, std::string_view key);

} // namespace xpp
#endif
