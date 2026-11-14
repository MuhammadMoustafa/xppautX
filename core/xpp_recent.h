#pragma once
/* The recent models (W232): the models the user opened lately, newest
   first, kept in the preferences file (xpp_prefs.h, W287: its
   `recent_models`); the start screen lists them (hello's `start`,
   docs/protocol.md "Start screen"). The file loads all or nothing: a value
   that is wrong is an error naming the file, the line and the path, and
   nothing of it is used, so the start screen lists none and says why; the
   next Open model does not overwrite a file it could not read. A path in
   it is only ever opened through File > Open model's one owner
   (model_switch.h), which checks it again; a path that is no longer there
   is listed as missing, never dropped. C++ only. */
#include <string>
#include <string_view>
#include <vector>

#include "xpp_error.h"

namespace xpp::recent {

/* hello's `start` object for a session with no model: {"path":..., "limit":N,
   "recent":[{"path":..., "missing":bool}, ...]} and, when the file is bad,
   "error" (the common error fields) with no entries; `more` is further
   members of the object (",\"examples\":{...}"), or empty */
std::string start_json(const Result<std::vector<std::string>> &loaded, std::string_view more = {});

} // namespace xpp::recent

