#ifndef XPP_SETX_FILE_H
#define XPP_SETX_FILE_H
/* A settings file, name.setx (W284, docs/odex.md ".setx"): the values,
   numerics, delays, boundary conditions and graphics of a model, in the
   settings syntax (setx.h), every key named and in any order. It is also a
   session file's model.setx (W285, snapx.h): the session's own copy of the
   same settings, complete and without the graphics, which windows.setx
   holds for every window.

   The numerics and the range keys are the option table's rows marked
   `saved` (model_options.h, numerics_settings.h), under the key the table
   gives them; the others (the values, the delays, the graphics, ...) are
   named here, once, for reading and writing alike.

   A settings file is read whole and checked (read_settings), then
   applied in one step (apply_settings): a file with a bad value
   changes nothing (W125). XPPAUT's own .set is read by lunch-new.cpp
   into the same Settings, which is written as a .setx (File > Import,
   --setfile). C++ only. */
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "adj2.h"      /* AdjointState's transpose */
#include "arrayplot.h" /* APLOT */
#include "browse.h"    /* AddedColumn */
#include "integrate.h" /* RangeVars, EquilibriumRange */
#include "pp_shoot.h"  /* ShootRange */
#include "setx.h"
#include "struct.h"    /* GRAPH */
#include "xpp_error.h"

namespace xpp {

struct Session;   /* session.h */
struct OptionRow; /* model_options.h */

inline constexpr std::string_view setx_extension = ".setx";

/* 4096 sides bound a saved array/transpose rectangle to 16 million cells. */
inline constexpr int settings_dimension_limit = 4096;
/* Four MiB is far above ordinary settings; the shared lexer separately caps token storage. */
inline constexpr std::size_t settings_bytes_limit = 4u * 1024 * 1024;

/* what a settings file holds. It starts as a copy of a session's own
   (capture_settings) and a file overrides what it names, so a file that
   names little leaves the rest as it was. */
struct Settings {
    /* the option table's rows a file holds, as text, in the table's order */
    using Setting = std::pair<const OptionRow *, std::string>;
    std::vector<Setting> numerics, range;
    double current_time = 0, last_time = 0;
    int my_start = 0, inflag = 0, delay_flag = 0;
    std::vector<std::string> delays, bcs;
    std::vector<double> last_ic, current, params;
    /* the active window's settings, its curves' variables by number */
    GRAPH graph{};
    decltype(AdjointState::transpose) transpose;
    std::vector<std::string> coupling;
    APLOT aplot{};
    int torus = 0;
    double torus_period = 0;
    std::vector<int> itor;
    /* the range's own settings that the table has no row for */
    std::string range_item2;
    double range_low2 = 0, range_high2 = 0;
    int range_cycle = 0;
    EquilibriumRange eq_range;
    ShootRange shoot_range;
    /* the file holds the active window's graphics (a settings file does, a
       session's model.setx does not: windows.setx holds every window's) */
    bool graphics = true;
    /* the value of the table's row (the numerics' or the range's by its
       group) set to text */
    void set_setting(const OptionRow *row, std::string text);
};

/* which file: a settings file a user keeps (partial: it sets what it
   names; with the active window's graphics) or a session's model.setx
   (complete: every key must be there; no graphics) */
enum class SettingsUse { file, session };

/* s's settings now */
Settings capture_settings(const Session &s, SettingsUse use);

/* settings as a .setx's text; the error when a text cannot be written */
Result<std::string> setx_text(const Session &s, const Settings &f);

/* text, the .setx named file, read for s (its model's names; what the
   file does not name is s's own): every key and value checked, or the
   error at the one that is wrong. Nothing of s changes. */
Result<Settings> read_settings(Session &s, std::string file, std::string_view text, SettingsUse use);

/* f put in s, in one step (the front end shown it when redraw) */
void apply_settings(Session &s, const Settings &f, bool redraw);

/* the JSON Schema of a .setx, from the description the reader and the writer
   share and the option table: what `xppautX --setx-schema` prints and
   docs/setx.schema.json holds (tools/optionschema.sh checks they agree) */
std::string setx_schema();

/* the value of parameter i of s's model */
double parameter_value(const Session &s, int i);

/* ---- the plot window's settings, shared with windows.setx ---- */

/* a window's curves as .setx names them: the variables by name (the time,
   a variable, a column the browser added), the line and the colour */
void write_curves(setx::Out &out, const Session &s, const GRAPH &g);
/* the window's curves into g (nvars, xv, yv, zv, line, color); the
   variables are looked up as find_variable does, then in added (the
   columns a session's windows.setx adds, not yet in s) */
void read_curves(setx::Block &window, const Session &s, GRAPH &g, std::span<const AddedColumn> added, std::size_t &expanded_bytes);
/* a plot window as a group holds it (windows.setx, the graphics of a .setx):
   its curves, then its other settings (the rotation, the scales, the box, ...);
   added: as read_curves' */
void write_window(setx::Out &out, const Session &s, const GRAPH &g);
void read_window(setx::Block &window, const Session &s, GRAPH &g, std::span<const AddedColumn> added, std::size_t &expanded_bytes);
/* from's curves (their number, variables, lines and colours) given to to */
void copy_curves(const GRAPH &from, GRAPH &to);
/* those settings of from given to to, its other fields kept (a restored
   window, as made, takes the saved one's) */
void copy_graph_settings(const GRAPH &from, GRAPH &to);

} // namespace xpp
#endif
