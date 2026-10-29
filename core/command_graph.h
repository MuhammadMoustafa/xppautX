#pragma once

/* What the registry commands that act on one graph share (commands_window.cpp, commands_plot.cpp): the graph a
   command names with `window` (the selected one when it names none), made the active one while the command
   works, since the core's drawing code works on the active window. */
#include "command_def.h"
#include "many_pops.h"
#include "session.h"

namespace xpp::command {

/* the active graph while an operation on another one runs */
class ActiveGraph {
public:
    ActiveGraph(Session &s, int index) : s_(s), previous_(s.plot_windows.active)
    {
        if (index != previous_) make_active(s_, index, 1);
    }
    ~ActiveGraph()
    {
        if (s_.plot_windows.active != previous_) make_active(s_, previous_, 1);
    }
    ActiveGraph(const ActiveGraph &) = delete;
    ActiveGraph &operator=(const ActiveGraph &) = delete;

private:
    Session &s_;
    int previous_;
};

/* the index of the graph the command acts on: its `window` argument, or the selected window; an error when
   that is no graph */
Result<int> graph_of_command(Session &s, const Command &command, const Arguments &args, const Place &place);

} // namespace xpp::command
