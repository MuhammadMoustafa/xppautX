#pragma once
#include <stddef.h>
#include <span>
#include <array>
#include <vector>
#include <optional>
#include <string>
#include <string_view>
#include "autox_axis.h"
#include "autox_codes.h"
#include "xpp_error.h"
#include "xpp_prefs.h"

/* autoX's settings as data (docs/ui-v2.md T22, docs/protocol.md "autoX's
   settings as data"): what the autoX window's Numerics, Axes
   (its AutoxPlot form) and Mark values forms edit, sent as the
   "autoxsettings" event and set by the `autox` `set` command, so a front end
   edits them in forms of its own, at any time.

   The forms in autox_nox.cpp stay (scripts replay them); marks use this
   same validator and Session-owned list. A set with one bad value changes
   nothing.

   autox_settings.cpp; nothing escapes it. */

namespace xpp {

struct Session; /* session.h */
struct OptionRow; /* model_options.h */

/* 10000 intervals far exceed shipped meshes while bounding autoX's mesh-sized matrices. */
inline constexpr int autox_max_mesh_intervals = 10000;
/* autoX's collocation coefficients support only orders 2 through 7 (autox_lib1.cpp cpnts). */
inline constexpr int autox_min_collocation_points = 2;
inline constexpr int autox_max_collocation_points = 7;

/* the Numerics fields in their order, each named for what it sets (W531);
   autoX's own name for it (NTST, DS, MXBF, ...) is a search word in the
   manual and the page's hover, never an input */
enum {
    AUTOX_NUM_MESH_INTERVALS, AUTOX_NUM_MAX_STEPS, AUTOX_NUM_OUTPUT_EVERY, AUTOX_NUM_COLLOCATION_POINTS,
    AUTOX_NUM_STEP_SIZE, AUTOX_NUM_MIN_STEP_SIZE, AUTOX_NUM_MAX_STEP_SIZE,
    AUTOX_NUM_PARAMETER_MIN, AUTOX_NUM_PARAMETER_MAX, AUTOX_NUM_NORM_MIN, AUTOX_NUM_NORM_MAX,
    AUTOX_NUM_PARAMETER_TOLERANCE, AUTOX_NUM_STATE_TOLERANCE, AUTOX_NUM_SPECIAL_POINT_TOLERANCE,
    AUTOX_NUM_MESH_ADAPT_EVERY, AUTOX_NUM_MAX_BRANCH_SWITCHES, AUTOX_NUM_SWITCH_ONE_DIRECTION, AUTOX_NUM_OUTPUT_DETAIL,
    AUTOX_NUM_MAX_LOCATION_ITERATIONS, AUTOX_NUM_MAX_NEWTON_ITERATIONS, AUTOX_NUM_FULL_NEWTON_AFTER,
    AUTOX_NUM_STEP_ADAPT_EVERY, AUTOX_NUM_SUPPRESS_BRANCH_POINTS, AUTOX_NUM_FOLLOW_ALL_BRANCHES,
    AUTOX_NUM_N
};

/* what autoX's IID levels print, named by it (0 to 5 are autoX's own numbers, read on import only) */
inline constexpr std::array<const char *, 6> autox_output_detail_names = {
    "none", "little", "normal", "start_jacobian", "extensive", "debugging"};

/* what a field holds: a whole number, a real, a boolean, or one of named choices
   (stored in the settings' numbers as the index, 0 or 1 for a boolean) */
enum class AutoxFieldShape { whole, real, boolean, choice };

/* a Numerics field's key and shape are its row's in the option table (the continuation group, `autox_numeric` its index here): the one
   source, which the event, `set`, the files and the registry's autox.configure_numerics all read */
/* the key of Numerics field i in the event, the command and the files
   ("mesh_intervals", ...); NULL or empty out of range */
const char *autox_settings_num_key(int i);
/* the shape of field i: its row's kind (a boolean row, the choice, a whole number or a real) */
AutoxFieldShape autox_settings_num_shape(int i);
/* field i's value as the files and forms write it: the number, `true` or
   `false`, or the choice's name */
std::string autox_settings_num_text(int i, double v);
/* the value of field i that text is (a number, a boolean word or a choice's name),
   or why it is not */
Result<double> autox_settings_num_parse(int i, std::string_view text);

// Bound expanded mark storage from sent files or commands before allocation.
inline constexpr int autox_file_marks_limit = 65536;
// XPPAUT's legacy User form starts its first suggested period at 11.
inline constexpr double autox_initial_mark_period = 11;
// Its subsequent suggested periods are spaced by 3; extending rows preserves that sequence.
inline constexpr double autox_mark_period_spacing = 3;
// XPPAUT's foreign .auto schema always encodes nine rows, even when inactive.
inline constexpr int autox_foreign_mark_rows = 9;

/* the event: {"ev":"autoxsettings",...} with the settings now, whole; an
   empty string when there is no model with variables (no settings) */
std::string autox_settings_event_line(const Session &s);

/* {"cmd":"data"} with or without "autoxsettings": sent (s's) at once, and
   then at each update, whatever it holds */
void autox_settings_subscribe(const Session &s, int on);

/* send s's event if the settings changed since the one sent last */
void autox_settings_update(const Session &s);

/* One `autox` `set`: what it changes, each part only when given (a
   default-constructed set gives nothing). A name left empty keeps what is
   there. Filled by json_autox.cpp, read by autox_settings.cpp. */
struct AutoxSettingsSet {
    std::array<std::optional<bool>, autox_codim_kinds.size()> special_points{};
    std::array<int, AUTOX_NUM_N> has_num{};
    std::array<double, AUTOX_NUM_N> num{};
    std::optional<AutoxProjection> plot;
    std::string var, par1, par2;
    std::array<int, 4> has_range{}; /* xmin, xmax, ymin, ymax */
    std::array<double, 4> range{};
    int fit = 0; /* Axes/Fit after the axes are set */
    int view = -1; /* the view whose axes these are (W50), made the active one; -1: the active one */
    std::optional<int> nmarks; /* absent: keep the current marks; present: whole list length */
    std::vector<std::string> mark_name;
    std::vector<double> mark_value;
    /* the thread count of autoX and sweeps (W267, W434), a program preference and not a setting of
       the session: prefs::THREADS_AUTO or the count. Set by `autox`
       `set`'s "threads" and saved in the preferences file; never filled by
       autox_settings_now, so a session file does not hold it. */
    std::optional<unsigned> threads;
};

/* the thread count and the colors the preferences file holds into s (a recording's replay
   keeps the user's preferences out of it, and reads none); a bad file leaves
   the automatic count and the default colors (the file's error is shown by the recent models' note,
   once). Called by the load, with the preferences the program's start read
   (read), or null to read the file now: a reload reads it again. */
void preferences_load(Session &s, const Result<prefs::Preferences> *read);

/* the settings now, whole: every Numerics field, the
   axes and the Mark values, names empty where there is none (the event's
   null). What the event sends and a .snapx's session.odex saves in its autox group (xpp_session_autox.h);
   applied with autox_settings_apply, it gives the settings back. */
AutoxSettingsSet autox_settings_now(const Session &s);
/* Reload carries settings already in memory; missing model names keep the
   new model's defaults. No diagram, grab or run state carries over. */
void autox_settings_reload(Session &s, const AutoxSettingsSet &kept);
/* view `view`'s axes alone (W50, AutoxState::views; it must be one), as the
   axes keys and `view`: applied, they give the view its axes back */
AutoxSettingsSet autox_settings_view(const Session &s, int view);

/* 1 when a value of field i is autoX's (an integer where the field is one,
   in the field's range); why names the field and its range otherwise.
   Pure: no model state. Nothing is thrown. */
int autox_settings_num_ok(int i, double v, std::string &why);
/* field i's bounds as JSON Schema keywords (", \"minimum\": 1", ...), for
   the options schema: the same rule autox_settings_num_ok checks */
std::string autox_settings_num_schema(int i);
/* Check supplied numbers together, including ordering and signed step magnitude. */
Result<> autox_settings_numbers_check(std::span<const double, AUTOX_NUM_N> num,
                                    std::span<const int, AUTOX_NUM_N> supplied);

/* check the set against the model and apply it all, or nothing: 0 when
   applied, -1 with why (a sentence naming the bad value) when not. Axes
   (plot type, names, ranges, fit) draw the diagram again when autoX's
   window is open, as the AutoxPlot form does. Nothing is thrown. */
int autox_settings_apply(Session &s, const AutoxSettingsSet &set, std::string &why);
/* the same checks, nothing applied, as the diagram will be when it has
   views views (-1: the views s has; a file's autox views restore them): the
   Error says why, its field the key of the value that is wrong ("ntst",
   "var", "par1", "xmax", "mark2", ...; "view"; "" for the whole set), for
   a file of autoX's settings to name its line (xpp_session_autox_io.cpp) */
Result<> autox_settings_check(const Session &s, const AutoxSettingsSet &set, int views);

/* the diagram group's rows that are the active view's own (its variable and axes; the colors are the session's) and whose values the
   panel reads and sets there (W616): the model's values are only what a new view starts with */
bool autox_view_row(const OptionRow &row);
/* a view row's value in the active view, as text */
std::string autox_view_text(const Session &s, const OptionRow &row);
/* the view rows given (each with its text) applied to the active view, all or nothing; check only: nothing applied */
Result<> autox_view_apply(Session &s, std::span<const std::pair<const OptionRow *, std::string>> items, bool check_only);

} // namespace xpp
