#ifndef AUTO_SETTINGS_H
#define AUTO_SETTINGS_H
#include <stddef.h>
#include <array>
#include <vector>
#include <optional>
#include <string>
#include "xpplim.h"

/* AUTO's settings as data (docs/ui-v2.md T22, docs/protocol.md "AUTO's
   settings as data"): what the AUTO window's Numerics, Axes
   (its AutoPlot form) and Mark values forms edit, sent as the
   "autosettings" event and set by the `auto` `set` command, so a front end
   edits them in forms of its own, at any time.

   The forms in auto_nox.cpp stay (scripts replay them); marks use this
   same validator and Session-owned list. A set with one bad value changes
   nothing.

   auto_settings.cpp; nothing escapes it. */

namespace xpp {

struct Session; /* session.h */

/* Mesh intervals must be at least 1 for subdivision. */
inline constexpr int auto_min_mesh_intervals = 1;
/* 10000 intervals far exceed shipped meshes while bounding AUTO's mesh-sized matrices. */
inline constexpr int auto_max_mesh_intervals = 10000;
/* AUTO's collocation coefficients support only orders 2 through 7 (autlib1.cpp cpnts). */
inline constexpr int auto_min_collocation_points = 2;
inline constexpr int auto_max_collocation_points = 7;
/* AUTO diagnostic output detail levels (IID): 0 none to 5 maximum debug. */
inline constexpr int auto_min_diagnostic_level = 0;
inline constexpr int auto_max_diagnostic_level = 5;

/* Defaults for AUTO continuation and diagram settings (AutoOptions). */
inline constexpr int auto_default_ntst = 15;        // Default mesh intervals for periodic orbit / BVP discretisation
inline constexpr int auto_default_nmx = 200;        // Default maximum points along a continuation branch
inline constexpr int auto_default_npr = 50;         // Default interval to label and save full solution profiles
inline constexpr int auto_default_ncol = 4;         // Default collocation points per mesh interval
inline constexpr double auto_default_ds = 0.02;     // Default initial continuation step size
inline constexpr double auto_default_dsmax = 0.5;   // Default maximum continuation step size
inline constexpr double auto_default_dsmin = 0.001; // Default minimum continuation step size before failure
inline constexpr double auto_default_rl0 = 0.0;     // Default lower bound for principal continuation parameter
inline constexpr double auto_default_rl1 = 2.0;     // Default upper bound for principal continuation parameter
inline constexpr double auto_default_a0 = 0.0;      // Default lower bound for solution L2 norm
inline constexpr double auto_default_a1 = 1000.0;   // Default upper bound for solution L2 norm
inline constexpr double auto_default_epsl = 1e-4;   // Default relative convergence tolerance for equation parameters
inline constexpr double auto_default_epsu = 1e-4;   // Default relative convergence tolerance for solution state vector
inline constexpr double auto_default_epss = 1e-4;   // Default relative convergence tolerance for locating special points
inline constexpr double auto_default_xmax = 2.5;    // Default diagram view x-axis upper bound
inline constexpr double auto_default_xmin = -0.5;   // Default diagram view x-axis lower bound
inline constexpr double auto_default_ymax = 3.0;    // Default diagram view y-axis upper bound
inline constexpr double auto_default_ymin = -3.0;   // Default diagram view y-axis lower bound
inline constexpr int auto_default_var = 0;          // Default diagram view variable index (first model variable)

/* Defaults for AUTO advanced solver controls (ADVAUTO). */
inline constexpr int auto_default_iad = 3;   // Adapt mesh every 3 steps (0 = fixed mesh)
inline constexpr int auto_default_mxbf = 5;  // Follow up to 5 branch switches in both directions for algebraic problems
inline constexpr int auto_default_iid = 2;   // Normal diagnostic output level (unit 9 / .d file)
inline constexpr int auto_default_itmx = 8;  // Maximum special point location iterations
inline constexpr int auto_default_itnw = 7;  // Maximum Newton chord iterations per continuation step
inline constexpr int auto_default_nwtn = 3;  // Full Newton steps before switching to chord method
inline constexpr int auto_default_iads = 1;  // Adapt continuation step size every step

/* the Numerics form's fields, in its order (auto_num_par) */
enum {
    AUTO_NUM_NTST, AUTO_NUM_NMX, AUTO_NUM_NPR, AUTO_NUM_NCOL,
    AUTO_NUM_DS, AUTO_NUM_DSMIN, AUTO_NUM_DSMAX,
    AUTO_NUM_RL0, AUTO_NUM_RL1, AUTO_NUM_A0, AUTO_NUM_A1,
    AUTO_NUM_EPSL, AUTO_NUM_EPSU, AUTO_NUM_EPSS,
    AUTO_NUM_IAD, AUTO_NUM_MXBF, AUTO_NUM_IID, AUTO_NUM_ITMX, AUTO_NUM_ITNW,
    AUTO_NUM_NWTN, AUTO_NUM_IADS, AUTO_NUM_SUPPBP,
    AUTO_NUM_N
};

/* the key of Numerics field i in the event and the command ("ntst", ...),
   and its label on the core's form ("Ntst", "Par Min", ...); NULL out of range */
const char *auto_settings_num_key(int i);
const char *auto_settings_num_label(int i);

// Bound expanded mark storage from sent files or commands before allocation.
inline constexpr int auto_file_marks_limit = 65536;
// XPPAUT's legacy User form starts its first suggested period at 11.
inline constexpr double auto_initial_mark_period = 11;
// Its subsequent suggested periods are spaced by 3; extending rows preserves that sequence.
inline constexpr double auto_mark_period_spacing = 3;
// XPPAUT's foreign .auto schema always encodes nine rows, even when inactive.
inline constexpr int auto_foreign_mark_rows = 9;

/* the event: {"ev":"autosettings",...} with the settings now, whole; an
   empty string when there is no model with variables (no settings) */
std::string auto_settings_event_line(const Session &s);

/* {"cmd":"data"} with or without "autosettings": sent (s's) at once, and
   then at each update, whatever it holds */
void auto_settings_subscribe(const Session &s, int on);

/* send s's event if the settings changed since the one sent last */
void auto_settings_update(const Session &s);

/* One `auto` `set`: what it changes, each part only when given (a
   default-constructed set gives nothing). A name left empty keeps what is
   there. Filled by json_auto.cpp, read by auto_settings.cpp. */
struct AutoSettingsSet {
    std::array<int, AUTO_NUM_N> has_num{};
    std::array<double, AUTO_NUM_N> num{};
    int has_plot = 0;
    int plot = 0;
    std::string var, par1, par2;
    std::array<int, 4> has_range{}; /* xmin, xmax, ymin, ymax */
    std::array<double, 4> range{};
    int fit = 0; /* Axes/Fit after the axes are set */
    int view = -1; /* the view whose axes these are (W50), made the active one; -1: the active one */
    int nmarks = -1; /* -1: no "marks"; else the whole list's length */
    std::vector<std::string> mark_name;
    std::vector<double> mark_value;
    /* AUTO's thread count (W267), a program preference and not a setting of
       the session: prefs::AUTO_THREADS_AUTO or the count. Set by `auto`
       `set`'s "threads" and saved in the preferences file; never filled by
       auto_settings_now, so a session file does not hold it. */
    std::optional<unsigned> threads;
};

/* the thread count the preferences file holds into s (a recording's replay
   keeps the user's preferences out of it, and reads none); a bad file leaves
   the automatic count (the file's error is shown by the recent models' note,
   once). Called by the load. */
void auto_threads_load(Session &s);

/* the settings now, whole: every Numerics field, the
   axes and the Mark values, names empty where there is none (the event's
   null). What the event sends and a .snapx's settings.txt saves (xpp_session_auto.h);
   applied with auto_settings_apply, it gives the settings back. */
AutoSettingsSet auto_settings_now(const Session &s);
/* Reload carries settings already in memory; missing model names keep the
   new model's defaults. No diagram, grab or run state carries over. */
void auto_settings_reload(Session &s, const AutoSettingsSet &kept);
/* view `view`'s axes alone (W50, AutoState::views; it must be one), as the
   axes keys and `view`: applied, they give the view its axes back */
AutoSettingsSet auto_settings_view(const Session &s, int view);

/* 1 when a value of field i is AUTO's (an integer where the field is one,
   in the field's range); why names the field and its range otherwise.
   Pure: no model state. Nothing is thrown. */
int auto_settings_num_ok(int i, double v, std::string &why);

/* check the set against the model and apply it all, or nothing: 0 when
   applied, -1 with why (a sentence naming the bad value) when not. Axes
   (plot type, names, ranges, fit) draw the diagram again when AUTO's
   window is open, as the AutoPlot form does. Nothing is thrown. */
int auto_settings_apply(Session &s, const AutoSettingsSet &set, std::string &why);
/* the same checks, nothing applied, as the diagram will be when it has
   views views (-1: the views s has; a file's views.txt restores them):
   false with why and the key of the value that is wrong ("ntst",
   "var", "par1", "xmax", "mark2", ...; "view"; "" for the whole set), for
   a file of AUTO's settings to name its line (xpp_session_auto_io.cpp) */
bool auto_settings_check(const Session &s, const AutoSettingsSet &set, int views, std::string &why, std::string &key);

} // namespace xpp
#endif
