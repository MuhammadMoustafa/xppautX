#pragma once
/* Save values into the model file (W320, sets-design.md sections 6, 7 and 9):
   a new set, a set's part replaced, the defaults, an export to another .odex,
   a set renamed or deleted. Every one builds the new text of the file with
   odex_write.h (the rest of the file kept as written), parses and checks it,
   checks the file has not changed on disk since the model was loaded, replaces
   it atomically (a Writer: temp file, then rename), and only then updates the
   live model (its sets, defaults, options, places and the text it keeps of the
   file). A save that fails changes neither the file nor the model. One path
   for the window, --server, --silent and a recording's step. C++ only. */
#include "xpp_error.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xpp {

struct Session; /* session.h */

/* `set name { ... }` appended to the model file, holding the current values of
   scope (as apply_named_set's scopes: all, parameters, initial or a group of
   settings), the plot settings of graph (the active one when none); last_state:
   the initial values saved are the last state reached */
Result<> save_new_set(Session &s, std::string_view name, std::string_view scope, std::optional<int> graph, bool last_state);
/* the kind(s) of scope replaced in the model's own set `name` by the current
   values; the set's other parts stay as they are. A set an import brought is
   read-only: saving over it is the error that asks for a name of one's own. */
Result<> save_into_set(Session &s, std::string_view name, std::string_view scope, std::optional<int> graph, bool last_state);
/* the current values of scope written as the model's defaults: the par and init
   lines, the options block's groups. Restore defaults uses them at once. */
Result<> save_defaults(Session &s, std::string_view scope, std::optional<int> graph, bool last_state);
/* the current values of scope as the set `name` of the .odex file: a new file
   (with the "Saved from <model> at <time>" header), or one that exists, where a
   set of that name asks before its part is replaced. false when the question
   was answered no (nothing written). */
Result<bool> export_values(Session &s, std::string_view name, std::string_view scope, std::string_view file, std::optional<int> graph,
                           bool last_state);
/* the model's own set renamed or deleted, in the file */
Result<> rename_intern_set(Session &s, std::string_view from, std::string_view to);
Result<> delete_intern_set(Session &s, std::string_view name);
/* the sets the model file itself wrote, in order: the ones that may be replaced, renamed or deleted */
std::vector<std::string> own_set_names(const Session &s);

}

