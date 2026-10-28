#pragma once

#include "grobs.h"
#include <optional>
#include <string_view>

namespace xpp {

/* The plot windows: every window's graph settings, the active one, and
   the windows a command draws on (all open ones under Simulplot). */
struct XppPlotWindows {
    std::vector<GRAPH> graph{1};   /* graph[i].Use: window i is open */
    GRAPH *current;        /* &graph[active] */
    int active;            /* the active window's index */
    int count;             /* how many are open */
    std::vector<int> open{0};      /* the open windows' indices, count of them */
    int simul;             /* Simulplot: draw on every open window */
    XppWinId draw_win;     /* graph[active].w, the window drawn into */
};

/* The windows that are neither graphs nor AUTO views have a name each, fixed
   and reserved (window_name_problem): the data browser, the animation and
   the array plot (W515). */
inline constexpr std::string_view BROWSER_WINDOW = "browser", ANIMATION_WINDOW = "animation",
                                  ARRAY_PLOT_WINDOW = "array_plot_1";
/* whether each of them is hidden (a window hidden keeps everything it holds) */
struct ToolWindowsHidden {
    bool browser = false, animation = false, array_plot = false;
};

struct Session; /* session.h */

/* Names on every external boundary; numeric handles stay in drawing code. */
inline constexpr std::size_t WINDOW_NAME_BYTES = 1024; // Bound names supplied by clients and files.
enum class WindowKind { graph, diagram, browser, animation, array_plot };
struct WindowTarget { WindowKind kind; int index; };
std::optional<WindowTarget> find_window(const Session &s, std::string_view name);
/* every open window, in the order window.list gives them */
struct WindowInfo { std::string name; WindowKind kind; bool shown; };
std::vector<WindowInfo> list_windows(const Session &s);
/* those windows as a JSON array of {name, kind, shown}: window.list's result and the `windows` event's */
std::string windows_json(const Session &s);
/* what graph window w holds that cannot be made again (its texts, arrows,
   markers, pointers and frozen curves), as words; empty when nothing */
std::string window_content(const Session &s, XppWinId w);
/* the kind's word in events and results */
std::string_view window_kind_name(WindowKind kind);
/* whether the window is hidden (it exists: a find_window result) */
bool window_hidden(const Session &s, const WindowTarget &target);
void set_window_hidden(Session &s, const WindowTarget &target, bool hidden);
std::string next_window_name(const Session &s, WindowKind kind);
std::string window_name(const Session &s, XppWinId handle);
/* the animation's and the array plot's names, which no session changes */
std::string_view fixed_window_name(XppWinId handle);
std::string window_name_problem(std::string_view name);
std::string window_name_problem(const Session &s, std::string_view name, std::string_view replacing = {});
void redo_stuff(Session &s);
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
/* s's active plot window blanked, its axes drawn again */
void clr_scrn(Session &s);

} // namespace xpp
