#pragma once
#include <stddef.h>
#include <span>
#include <array>
#include <vector>
#include <optional>
#include <string>
#include <string_view>
#include "auto_axis.h"
#include "auto_codes.h"
#include "xpp_error.h"

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

/* 10000 intervals far exceed shipped meshes while bounding AUTO's mesh-sized matrices. */
inline constexpr int auto_max_mesh_intervals = 10000;
/* AUTO's collocation coefficients support only orders 2 through 7 (autlib1.cpp cpnts). */
inline constexpr int auto_min_collocation_points = 2;
inline constexpr int auto_max_collocation_points = 7;

/* the Numerics fields in their order, each named for what it sets (W531);
   AUTO's own name for it (NTST, DS, MXBF, ...) is a search word in the
   manual and the page's hover, never an input */
enum {
    AUTO_NUM_MESH_INTERVALS, AUTO_NUM_MAX_STEPS, AUTO_NUM_OUTPUT_EVERY, AUTO_NUM_COLLOCATION_POINTS,
    AUTO_NUM_STEP_SIZE, AUTO_NUM_MIN_STEP_SIZE, AUTO_NUM_MAX_STEP_SIZE,
    AUTO_NUM_PARAMETER_MIN, AUTO_NUM_PARAMETER_MAX, AUTO_NUM_NORM_MIN, AUTO_NUM_NORM_MAX,
    AUTO_NUM_PARAMETER_TOLERANCE, AUTO_NUM_STATE_TOLERANCE, AUTO_NUM_SPECIAL_POINT_TOLERANCE,
    AUTO_NUM_MESH_ADAPT_EVERY, AUTO_NUM_MAX_BRANCH_SWITCHES, AUTO_NUM_SWITCH_ONE_DIRECTION, AUTO_NUM_OUTPUT_DETAIL,
    AUTO_NUM_MAX_LOCATION_ITERATIONS, AUTO_NUM_MAX_NEWTON_ITERATIONS, AUTO_NUM_FULL_NEWTON_AFTER,
    AUTO_NUM_STEP_ADAPT_EVERY, AUTO_NUM_SUPPRESS_BRANCH_POINTS,
    AUTO_NUM_N
};

/* what AUTO's IID levels print, named by it (0 to 5 are AUTO's own numbers, read on import only) */
inline constexpr std::array<const char *, 6> auto_output_detail_names = {
    "none", "little", "normal", "start_jacobian", "extensive", "debugging"};

/* what a field holds: a whole number, a real, a boolean, or one of named choices
   (stored in the settings' numbers as the index, 0 or 1 for a boolean) */
enum class AutoFieldShape { whole, real, boolean, choice };

/* the Numerics fields' keys and shapes, by AUTO_NUM_* (the one list: the event, `set`, the files and the registry's
   auto.configure_numerics all name them so) */
inline constexpr std::array<const char *, AUTO_NUM_N> auto_num_keys = {
    "mesh_intervals", "max_steps", "output_every", "collocation_points", "step_size", "min_step_size", "max_step_size",
    "parameter_min", "parameter_max", "norm_min", "norm_max", "parameter_tolerance", "state_tolerance",
    "special_point_tolerance", "mesh_adapt_every", "max_branch_switches", "switch_one_direction", "output_detail",
    "max_location_iterations", "max_newton_iterations", "full_newton_after", "step_adapt_every", "suppress_branch_points"};
inline constexpr std::array<AutoFieldShape, AUTO_NUM_N> auto_num_shapes = {
    AutoFieldShape::whole, AutoFieldShape::whole, AutoFieldShape::whole, AutoFieldShape::whole, AutoFieldShape::real,
    AutoFieldShape::real, AutoFieldShape::real, AutoFieldShape::real, AutoFieldShape::real, AutoFieldShape::real,
    AutoFieldShape::real, AutoFieldShape::real, AutoFieldShape::real, AutoFieldShape::real, AutoFieldShape::whole,
    AutoFieldShape::whole, AutoFieldShape::boolean, AutoFieldShape::choice, AutoFieldShape::whole, AutoFieldShape::whole,
    AutoFieldShape::whole, AutoFieldShape::whole, AutoFieldShape::boolean};

/* the key of Numerics field i in the event, the command and the files
   ("mesh_intervals", ...); NULL or empty out of range */
const char *auto_settings_num_key(int i);
/* field i's value as the files and forms write it: the number, `true` or
   `false`, or the choice's name */
std::string auto_settings_num_text(int i, double v);
/* the value of field i that text is (a number, a boolean word or a choice's name),
   or why it is not */
Result<double> auto_settings_num_parse(int i, std::string_view text);

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
    std::array<std::optional<bool>, auto_codim_kinds.size()> special_points{};
    std::array<int, AUTO_NUM_N> has_num{};
    std::array<double, AUTO_NUM_N> num{};
    std::optional<AutoProjection> plot;
    std::string var, par1, par2;
    std::array<int, 4> has_range{}; /* xmin, xmax, ymin, ymax */
    std::array<double, 4> range{};
    int fit = 0; /* Axes/Fit after the axes are set */
    int view = -1; /* the view whose axes these are (W50), made the active one; -1: the active one */
    std::optional<int> nmarks; /* absent: keep the current marks; present: whole list length */
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
/* field i's bounds as JSON Schema keywords (", \"minimum\": 1", ...), for
   the options schema: the same rule auto_settings_num_ok checks */
std::string auto_settings_num_schema(int i);
/* Check supplied numbers together, including ordering and signed step magnitude. */
Result<> auto_settings_numbers_check(std::span<const double, AUTO_NUM_N> num,
                                    std::span<const int, AUTO_NUM_N> supplied);

/* check the set against the model and apply it all, or nothing: 0 when
   applied, -1 with why (a sentence naming the bad value) when not. Axes
   (plot type, names, ranges, fit) draw the diagram again when AUTO's
   window is open, as the AutoPlot form does. Nothing is thrown. */
int auto_settings_apply(Session &s, const AutoSettingsSet &set, std::string &why);
/* the same checks, nothing applied, as the diagram will be when it has
   views views (-1: the views s has; a file's views.txt restores them): the
   Error says why, its field the key of the value that is wrong ("ntst",
   "var", "par1", "xmax", "mark2", ...; "view"; "" for the whole set), for
   a file of AUTO's settings to name its line (xpp_session_auto_io.cpp) */
Result<> auto_settings_check(const Session &s, const AutoSettingsSet &set, int views);

} // namespace xpp
