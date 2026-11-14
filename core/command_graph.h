#pragma once
#include <optional>
#include "xpp_error.h"
namespace xpp { namespace command { class Arguments; } namespace command { struct Command; } struct Session; }

/* What the registry commands that act on one graph share (commands_window.cpp, commands_plot.cpp): the graph a
   command names with `window` (the selected one when it names none), made the active one while the command
   works, since the core's drawing code works on the active window. */

namespace xpp::command {

/* the index of the graph the command acts on: its `window` argument, or the selected window; an error when
   that is no graph */
Result<int> graph_of_command(Session &s, const Command &command, const Arguments &args, const Place &place);

/* the same for a command whose `window` is optional: none (the active graph) when it names none */
Result<std::optional<int>> optional_graph_of_command(Session &s, const Command &command, const Arguments &args, const Place &place);

} // namespace xpp::command
