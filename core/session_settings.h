#pragma once
/* The settings of a session, in .odex syntax (docs/odex.md "Session files"): a session file's parameter values
   are its `par` lines, the initial values its `init` lines, the numerics and the sweep's the rows of its `options`
   block (the option table's rows marked `saved`, model_options.h, under their .odex keys); the others (the run
   position, the delays, the boundary conditions, ...) are groups of its `session { ... }` statement, named here, once,
   for reading and writing alike. Every key is named and complete, without the graphics, which the session's windows
   hold for every window. The same Settings also carry an imported XPPAUT .set (lunch-new.h), graphics included.

   A session file is read whole and checked (read_settings), then
   applied in one step (apply_settings): a file with a bad value
   changes nothing (W125). XPPAUT's own .set is read by lunch-new.cpp
   into the same Settings, which is applied as it is (File > Import XPPAUT
   session state, --convert --statefile). C++ only. */
#include <span>
#include <string>
#include <utility>
#include <vector>
#include <cstddef>

#include "display_state.h" /* Zoom */
#include "struct.h"    /* GRAPH */
#include "xpp_error.h"
namespace xpp { namespace odex { class Block; } namespace odex { class Out; } namespace odex { struct StateFile; } struct AddedColumn; }

namespace xpp {

struct Session;   /* session.h */
struct OptionRow; /* model_options.h */

/* Four MiB is far above ordinary settings; the shared lexer separately caps token storage. */
inline constexpr std::size_t settings_bytes_limit = 4u * 1024 * 1024;

/* what a settings file holds. It starts as a copy of a session's own
   (capture_settings) and a file overrides what it names, so a file that
   names little leaves the rest as it was. */
struct Settings {
    /* the option table's rows a file holds (the ones marked `saved`), as text, in the table's order */
    using Setting = std::pair<const OptionRow *, std::string>;
    std::vector<Setting> options;
    double current_time = 0, last_time = 0;
    int my_start = 0, inflag = 0, delay_flag = 0;
    std::vector<std::string> delays, bcs;
    std::vector<double> last_ic, current, params;
    /* the active window's settings, its curves' variables by number */
    GRAPH graph{};
    /* the file holds the active window's graphics (a settings file does, a
       session's does not: its windows hold every window's) */
    bool graphics = true;
    /* the value of the table's row set to text */
    void set_setting(const OptionRow *row, std::string text);
};

/* which settings: those with the active window's graphics (what an XPPAUT .set holds, compared and applied by
   lunch-new.cpp) or a session's (complete: every key must be there; no graphics) */
enum class SettingsUse { file, session };

/* s's settings now */
Settings capture_settings(const Session &s, SettingsUse use);

/* f's `version`, par and init lines and options block, the start of a session file */
Result<std::string> settings_statements(const Session &s, const Settings &f);
/* f's groups of a session statement's keys, written into out (the caller opens and closes the statement) */
void write_settings(odex::Out &out, const Session &s, const Settings &f);
/* f as the text of a file of its own: `version`, the par and init lines, the options block and a session statement
   with these groups; the error when a text cannot be written */
Result<std::string> settings_text(const Session &s, const Settings &f);

/* file's par and init lines, options block and the settings of its session group read for s (its model's
   names): every key and value checked, ReadFailed at the one that is wrong. Nothing of s changes. */
Settings read_settings(Session &s, const odex::StateFile &file, odex::Block &session);

/* f's storage precision put in s, its stored data converted: before a session's data table is put in place, so the
   table is kept at the precision the session was saved with */
void apply_storage_precision(Session &s, const Settings &f);

/* f put in s, in one step (the front end shown it when redraw) */
void apply_settings(Session &s, const Settings &f, bool redraw);

/* the value of parameter i of s's model */
double parameter_value(const Session &s, int i);

/* ---- the plot window's settings, shared by the session's windows and the graphics of an XPPAUT .set ---- */

/* a window's curves as a session names them: the variables by name (the time,
   a variable, a column the browser added), the line and the color */
void write_curves(odex::Out &out, const Session &s, const GRAPH &g);
/* the window's curves into g (nvars, xv, yv, zv, line, color); the
   variables are looked up as find_variable does, then in added (the
   columns a session's windows add, not yet in s) */
void read_curves(odex::Block &window, const Session &s, GRAPH &g, std::span<const AddedColumn> added, std::size_t &expanded_bytes);
/* a plot window as a group holds it (a session's windows, the graphics of the settings):
   its curves, then its other settings (the rotation, the scales, the box, ...);
   added: as read_curves' */
void write_window(odex::Out &out, const Session &s, const GRAPH &g);
void read_window(odex::Block &window, const Session &s, GRAPH &g, std::span<const AddedColumn> added, std::size_t &expanded_bytes);
/* a zoom, the part of the axes shown, as a group `zoom` of the owner's: x and y, each with `set`, `low` and `high`;
   a set one has low below high */
void write_zoom(odex::Out &out, const Zoom &z);
Zoom read_zoom(odex::Block &owner);
/* from's curves (their number, variables, lines and colors) given to to */
void copy_curves(const GRAPH &from, GRAPH &to);
/* those settings of from given to to, its other fields kept (a restored
   window, as made, takes the saved one's) */
void copy_graph_settings(const GRAPH &from, GRAPH &to);

} // namespace xpp
