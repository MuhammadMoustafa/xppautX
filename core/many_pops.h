#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <cstddef>
#include <string>
#include <vector>
#include "struct.h"
#include "xpp_types.h"

namespace xpp {

/* The split view (W720): the main area's graphs and autoX views in one group or two side by side (right) or one above the other
   (down). The first group holds the main graph, always; a window is in the second group when its `second` is set (a graph's, an
   autoX view's), and the split ends with the last one. Within a group the tabs keep the order of plot_windows.open and of the views. */
enum class SplitDirection { none, right, down };
/* the direction's word in commands, events and the session file; the other way, or none for a word that is no direction */
std::string_view split_direction_name(SplitDirection direction);
std::optional<SplitDirection> split_direction_from_name(std::string_view name);
/* a window's group's word in events and the session file, and back (nullopt: neither word) */
inline constexpr std::string_view GROUP_FIRST = "first", GROUP_SECOND = "second";
inline std::string_view group_name(bool second) { return second ? GROUP_SECOND : GROUP_FIRST; }
std::optional<bool> group_second_from_name(std::string_view name);
/* the first group's share of the main area when a split starts: half and half */
inline constexpr double SPLIT_SHARE_DEFAULT = 0.5;
/* neither group goes below this share of the area: it keeps a tab strip and a plot usable (the page's minimum is the same fraction) */
inline constexpr double SPLIT_SHARE_MIN = 0.2;
struct WindowSplit {
    SplitDirection direction = SplitDirection::none;
    double share = SPLIT_SHARE_DEFAULT;     /* the first group's share, 0..1 */
    std::array<std::string, 2> shown;       /* each group's shown window (split_upkeep keeps them true) */
};

/* The plot windows: every window's graph settings, the active one, and
   the windows a command draws on (all open ones under Simulplot). */
struct XppPlotWindows {
    std::vector<GRAPH> graph{1};   /* graph[i].Use: window i is open */
    GRAPH *current;        /* &graph[active] */
    int active;            /* the active window's index */
    int count;             /* how many are open */
    std::vector<int> open{0};      /* the open windows' indices in the tabs' order (move_window), count of them; the main graph's, 0, first */
    int simul;             /* Simulplot: draw on every open window */
    XppWinId draw_win;     /* graph[active].w, the window drawn into */
    WindowSplit split;     /* the split view's direction, share and shown windows */
};

/* The windows that are neither graphs nor autoX views have a name each, fixed
   and reserved (window_name_problem): the data table, the animation and
   the array plot (W515). */
inline constexpr std::string_view DATA_WINDOW = "data", ANIMATION_WINDOW = "animation",
                                  ARRAY_PLOT_WINDOW = "array_plot_1";
/* whether each of them is hidden (a window hidden keeps everything it holds) */
struct ToolWindowsHidden {
    bool data = false, animation = false, array_plot = false;
};

struct Session; /* session.h */

/* Names on every external boundary; numeric handles stay in drawing code. */
inline constexpr std::size_t WINDOW_NAME_BYTES = 1024; // Bound names supplied by clients and files.
enum class WindowKind { graph, diagram, data, animation, array_plot };
struct WindowTarget { WindowKind kind; int index; };
std::optional<WindowTarget> find_window(const Session &s, std::string_view name);
/* every open window, in the order window.list gives them */
struct WindowInfo { std::string name; WindowKind kind; bool shown; bool second; };
std::vector<WindowInfo> list_windows(const Session &s);
/* those windows as a JSON array of {name, kind, shown}: window.list's result and the `windows` event's */
std::string windows_json(const Session &s);
/* `"windows":[...],"split":{...}` (split_json): the members window.list's result and the `windows` event share */
std::string windows_fields(const Session &s);
/* what graph window w holds that cannot be made again (its texts, arrows,
   markers, pointers and frozen curves), as words; empty when nothing */
std::string window_content(const Session &s, XppWinId w);
/* the kind's word in events and results */
std::string_view window_kind_name(WindowKind kind);
/* whether the window is hidden (it exists: a find_window result) */
bool window_hidden(const Session &s, const WindowTarget &target);
void set_window_hidden(Session &s, const WindowTarget &target, bool hidden);
/* the window's place among the windows of its kind (a graph or an autoX view) in the tabs' order, from 0 */
long long window_position(const Session &s, const WindowTarget &target);
/* Move a window to `position` among the windows of its kind in the tabs' order (graphs: the main graph is 0 and stays; autoX
   views from 0); the problem as words, nothing changed, or empty. Windows keep their slots: only the order moves. */
std::string move_window(Session &s, const WindowTarget &target, long long position);
/* whether the graph or autoX view is in the second group of the split */
bool window_second(const Session &s, const WindowTarget &target);
/* The split view's moves, each the problem as words with nothing changed, or empty: the window (not the main graph, a graph or an autoX
   view) goes to the second group and the split takes the direction; back to the first group, the split ending with the last one;
   the first group's share (SPLIT_SHARE_MIN to 1 - SPLIT_SHARE_MIN, a split in place). */
std::string split_window(Session &s, const WindowTarget &target, SplitDirection direction);
std::string join_window(Session &s, const WindowTarget &target);
std::string set_split_share(Session &s, double share);
/* The split made true after what a command did (a window closed, renamed or selected): no second-group window ends it, and each
   group's shown window is the selected one when it is the group's, else the one it showed, else its first. */
void split_upkeep(Session &s);
/* the split as the `windows` event and window.list carry it: {"direction","share","shown":{"first","second"},"selected"} (the selected
   window is the focused group's shown one) */
std::string split_json(const Session &s);
std::string next_window_name(const Session &s, WindowKind kind);
std::string window_name(const Session &s, XppWinId handle);
/* the animation's and the array plot's names, which no session changes */
std::string_view fixed_window_name(XppWinId handle);
std::string window_name_problem(std::string_view name);
std::string window_name_problem(const Session &s, std::string_view name, std::string_view replacing = {});
/* The session s's plot windows: which are open (set_active_windows), the
   active one (make_active), the plot window (its graph's index) whose
   window is w (graph_of; 0, the main one, when none in use is), and the
   active window's bounds made valid (check_windows) */
void set_active_windows(Session &s);
/* Create and activate a graph; the UI callback only presents its surface. */
void create_a_pop(Session &s, std::string_view name = {});
void make_active(Session &s, int i, int flag);
int graph_of(const Session &s, XppWinId w);
void check_windows(Session &s);
/* f() on the active plot window of s, or under Simulplot on each open one
   in turn (made active with make_active(s, i, flag)), the active one made
   active again after */
template <class F>
void for_each_shown_window(auto &s, int flag, F f) /* s is a Session, incomplete here: read where it is complete */
{
    if (s.plot_windows->simul == 0) {
        f();
        return;
    }
    const int ic = s.plot_windows->active;
    for (int i = 0; i < s.plot_windows->count; i++) {
        make_active(s, s.plot_windows->open[i], flag);
        f();
    }
    make_active(s, ic, flag);
}
/* s's active plot window blanked, its axes drawn again */
void clr_scrn(Session &s);
/* Clear plot (window.clear, the key E): the windows a command draws on (every open one under Simulplot)
   blanked, and a data client told */
void clear_windows(Session &s);

/* Select a graph for the duration of a command, then restore the previous graph. */
class ActiveGraph {
public:
    ActiveGraph(Session &s,int index);
    ~ActiveGraph();
    ActiveGraph(const ActiveGraph &)=delete;
    ActiveGraph &operator=(const ActiveGraph &)=delete;
private:
    Session &s_;
    int previous_;
    std::string selected_;
};

} // namespace xpp
