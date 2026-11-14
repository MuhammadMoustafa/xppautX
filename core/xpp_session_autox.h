#pragma once
/* The session's autoX state: its settings and views, which the state file's session statement holds as the group
   `autox` (xpp_session_autox.cpp writes and reads them, xpp_session_autox_io.cpp gathers and restores the
   session's), and the members autox/diagram.csv and autox/solutions.s. The only archive reader/writer is
   xpp_session.cpp (.snapx). */
#include <deque>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <cstddef>
#include <utility>

#include "autox_settings.h"
#include "display_state.h"
#include "xpp_error.h"
#include "xpp_zip.h"
#include "autox_axis.h"
#include "autox_nox.h"
namespace xpp { enum AutoxCalculation : int; namespace odex { class Block; } namespace odex { class Out; } }

namespace xpp {
struct Session; /* session.h */
class Lines;
}

namespace xpp::snapx::autox_members {

inline constexpr const char *diagram_member = "diagram.csv";   /* diagram_csv below */
inline constexpr const char *solutions_member = "solutions.s"; /* autoX's solution file (fort.8), as autoX wrote it */

/* the group `settings` of the autox group, being written: each Numerics key (the continuation options' keys) and
   its value, the special points, `plot`, `var`, `par1`, `par2` (a name there is none of is ""), `xmin`, `xmax`,
   `ymin`, `ymax`, then the `marks` list of groups of `name` and `value`. Numbers are the shortest text that reads
   back as the same double. */
void write_autox_settings(odex::Out &out, const AutoxSettingsSet &s);
/* the settings group read, and where each key is ("mesh_intervals", "var", "xmax", "mark0", ...), as a line and a
   column, for a check against a session (autox_settings_check) to name the place of the value it refuses */
struct SettingsRead {
    AutoxSettingsSet set;
    std::map<std::string, std::pair<int, int>, std::less<>> places;
};
/* the settings, every one of them given once; ReadFailed at a key that is missing or not a setting, given twice or
   a value that does not read */
SettingsRead read_autox_settings(odex::Block &settings);

/* one of the autoX window's views of the diagram (W50): its axes as the
   `autox` `set` command's keys name them (plot, var, par1, par2 and the
   ranges xmin, xmax, ymin, ymax; a name there is none of is empty) and the
   zoom the page shows of them */
struct SavedView {
    AutoxProjection plot = AutoxProjection::maximum;
    std::string var, par1, par2;
    std::array<double, 4> range{};
    xpp::Zoom zoom;
    std::string name;
    bool hidden = false;
    bool second = false; /* in the split view's second group (many_pops.h) */
    bool operator==(const SavedView &) const = default;
};
struct SavedViews {
    std::vector<SavedView> views;
    int active = 0;
    bool operator==(const SavedViews &) const = default;
};

/* Bound the in-memory objects a sent member can create; these exceed normal
   saved diagrams/views while keeping tiny rows from allocating without limit. */
inline constexpr std::size_t saved_parameters_limit = zip::archive_bytes_limit / sizeof(std::string);
inline constexpr std::size_t saved_views_limit = 4096;
inline constexpr std::size_t saved_points_limit = 1000000;

/* the `views` list and the `active` view's name of the autox group, being written: per view its quoted name,
   `hidden`, `plot`, `var`, `par1`, `par2`, `xmin`, `xmax`, `ymin`, `ymax` and `zoom` (names and numbers as
   write_autox_settings') */
void write_views(odex::Out &out, const SavedViews &v);
/* the views read, and where each one is (its line and column) */
struct ViewsRead {
    SavedViews views;
    std::vector<std::pair<int, int>> places;
};
/* the views of the autox group: at least one, each named once with a name a window can have, the active one
   among them; ReadFailed at the value that is wrong */
ViewsRead read_views(odex::Block &autox);

/* diagram.csv: branch records of all model parameters once, a header row, then one row per point in the
   order stored, every field of DIAGRAM: calc, ibr, ntot, itp, lab, nfpar,
   icp1..icp4, flag2, from, norm, per, torper, branch id and semicolon-separated continued indices/values, then for each
   variable x of vars (the model's first node) u0.x, uhi.x, ulo.x, ubar.x
   and last evr1, evi1, ... evrN, eviN (its eigenvalues or Floquet
   multipliers, autox_stability.h; zeros: not computed). Numbers as
   settings_text's, so a point reads back bit for bit. */
/* model_parameters: the model's parameter count, which an autoX unknown's file integer follows (autox_axis.h) */
std::string diagram_csv(const std::deque<DIAGRAM> &points, int model_parameters, std::span<const std::string> vars);
/* text's points, of n variables each, their arrays filled (the DIAGRAM
   pointers and index are diagram_restore's), the file named file; the
   error at the line that is not of a diagram of n variables and n_pars parameters */
Result<std::deque<DIAGRAM>> parse_diagram_csv(std::string_view text, int n, int n_pars, std::string file);

/* Foreign diagram records, consuming exactly the declared last index + 1 points.
   The model parameter values seed immutable branch constants. */
std::deque<DIAGRAM> read_foreign_diagram(Lines &lines, int n,
    std::span<const int> imported, std::span<const double> parameters, AutoxCalculation calc, AutoxCalculation flag2, double torper);

/* ---- this session's (xpp_session_autox_io.cpp) ---- */

/* autoX's diagram and solution members of the session s after entries', each named prefix and its name (a session
   file's "autox/"), when it has a diagram; the error when autoX's solution file cannot be read (the orbits a grab
   restarts from) */
Result<> add_members(const Session &s, std::vector<xpp::zip::Entry> &entries, std::string_view prefix);
/* the group `autox` of the session statement being written: autoX's settings, views and active view as they are in s */
void write_state(odex::Out &out, const Session &s);

/* autoX's state of a file as members_read reads it, for restore_members: the solution file's bytes are the members' own */
struct Members {
    AutoxSettingsSet settings;
    std::deque<DIAGRAM> points;
    SavedViews views;
    std::string_view solutions;
};

/* autoX's state of the file named name read for the session s, whose model the file's is: the group autox of the
   session statement and the members of the file (prefix as add_members'), each checked whole (the settings and
   views against what autoX accepts for this model); ReadFailed at the key or value of the group, or at the member's
   line ("name/autox/diagram.csv", its line), that is missing, does not read or holds a value autoX refuses */
Members members_read(const Session &s, const std::map<std::string, std::string> &members, std::string_view prefix,
                     const std::string &name, odex::Block &session);

/* m, as members_read or the foreign import checked it, restored into s: autoX's
   settings, the diagram, its solution file and the views of it, with the
   autoX window opened and the diagram drawn; the error, nothing restored,
   when the solution file cannot be written. A foreign import gives no saved
   view list and keeps the existing views around its imported active axes. */
Result<> restore_members(Session &s, Members m);

} // namespace xpp::snapx::autox_members

