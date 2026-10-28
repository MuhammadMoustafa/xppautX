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

struct Session; /* session.h */

/* Names on every external boundary; numeric handles stay in drawing code. */
inline constexpr std::size_t WINDOW_NAME_BYTES = 1024; // Bound names supplied by clients and files.
enum class WindowKind { graph, diagram };
struct WindowTarget { WindowKind kind; int index; };
std::optional<WindowTarget> find_window(const Session &s, std::string_view name);
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
