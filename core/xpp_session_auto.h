#ifndef XPP_SESSION_AUTO_H
#define XPP_SESSION_AUTO_H
/* The session's auto/ members: settings, views, full-precision diagram
   and AUTO's restart orbits. The pure serializers are xpp_session_auto.cpp;
   xpp_session_auto_io.cpp gathers and restores the session's state. The
   only archive reader/writer is xpp_session.cpp (.snapx). */
#include <deque>
#include <expected>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "auto_settings.h"
#include "diagram.h"
#include "display_state.h"
#include "xpp_error.h"
#include "xpp_zip.h"

namespace xpp {
struct Session; /* session.h */
}

namespace xpp::snapx::auto_members {

inline constexpr const char *settings_member = "settings.txt"; /* settings_text below */
inline constexpr const char *diagram_member = "diagram.csv";   /* diagram_csv below */
inline constexpr const char *solutions_member = "solutions.s"; /* AUTO's solution file (fort.8), as AUTO wrote it */
inline constexpr const char *views_member = "views.txt";       /* views_text below */

/* settings.txt: one "key value" line each, the keys of the `auto` `set`
   command (docs/protocol.md "AUTO's settings as data"): each Numerics key
   and its number, "pars" and AUTO's parameters' names, "plot", "var",
   "par1", "par2", "xmin", "xmax", "ymin", "ymax", then one "mark NAME
   VALUE" per Mark value. A name there is none of is "-". Numbers are the
   shortest text that reads back as the same double. */
std::string settings_text(const AutoSettingsSet &s);
/* settings.txt read: the settings, and the line each key is on ("ntst",
   "pars", "var", "xmax", "mark0", ...), for a check against a session
   (auto_settings_check) to name the line of the value it refuses */
struct SettingsRead {
    AutoSettingsSet set;
    std::map<std::string, int, std::less<>> lines;
};
/* text's settings (the file named file, for its errors), every one of
   them given once; the error at the line that is not one of these, gives
   a key twice or a value that does not read, or a key that is missing */
Result<SettingsRead> parse_settings(std::string_view text, std::string file);

/* one of the AUTO window's views of the diagram (W50): its axes as the
   `auto` `set` command's keys name them (plot, var, par1, par2 and the
   ranges xmin, xmax, ymin, ymax; a name there is none of is empty) and the
   zoom the page shows of them */
struct SavedView {
    int plot = 0;
    std::string var, par1, par2;
    std::array<double, 4> range{};
    xpp::Zoom zoom;
    bool operator==(const SavedView &) const = default;
};
struct SavedViews {
    std::vector<SavedView> views;
    int active = 0;
    bool operator==(const SavedViews &) const = default;
};

/* Bound the in-memory objects a sent member can create; these exceed normal
   saved diagrams/views while keeping tiny rows from allocating without limit. */
inline constexpr std::size_t saved_views_limit = 4096;
inline constexpr std::size_t saved_points_limit = 1000000;

/* views.txt: one line per view in order, "view PLOT VAR PAR1 PAR2 XMIN
   XMAX YMIN YMAX ZOOMX ZOOMY" (names and numbers as settings_text's, a
   zoom LO:HI or "-" for none), then "active K" */
std::string views_text(const SavedViews &v);
/* views.txt read: the views, and the line each one is on */
struct ViewsRead {
    SavedViews views;
    std::vector<int> lines;
};
/* text's views (the file named file); the error at the line that is not
   one, or when there is no view or the active one is not one of them */
Result<ViewsRead> parse_views(std::string_view text, std::string file);

/* diagram.csv: a header row of names, then one row per point in the
   order stored, every field of DIAGRAM: calc, ibr, ntot, itp, lab, nfpar,
   icp1..icp4, flag2, from, norm, per, torper, par1..par20, then for each
   variable x of vars (the model's first node) u0.x, uhi.x, ulo.x, ubar.x
   and last evr1, evi1, ... evrN, eviN (its eigenvalues or Floquet
   multipliers, auto_stability.h; zeros: not computed). Numbers as
   settings_text's, so a point reads back bit for bit. */
std::string diagram_csv(const std::deque<DiagramPoint> &points, std::span<const std::string> vars);
/* text's points, of n variables each, their arrays filled (the DIAGRAM
   pointers and index are diagram_restore's), the file named file; the
   error at the line that is not of a diagram of n variables */
Result<std::deque<DiagramPoint>> parse_diagram_csv(std::string_view text, int n, std::string file);

/* ---- this session's (xpp_session_auto_io.cpp) ---- */

/* AUTO's members (settings, diagram, solutions, views) of the session s
   after entries', each named prefix and its name (a session file's
   "auto/"); the error when AUTO's solution file
   cannot be read (the orbits a grab restarts from) */
Result<> add_members(const Session &s, std::vector<xpp::zip::Entry> &entries, std::string_view prefix);

/* AUTO's members of a file as members_read reads them, for
   restore_members: the solution file's bytes are the members' own */
struct Members {
    AutoSettingsSet settings;
    std::deque<DiagramPoint> points;
    SavedViews views;
    std::string_view solutions;
};

/* AUTO's members of the file named name (prefix as add_members') read
   for the session s, whose model the file's is, each checked whole (the
   settings and views against what AUTO accepts for this model): the error
   at the member's line ("name/auto/settings.txt", its line) when one is
   missing, does not read or holds a value AUTO refuses */
Result<Members> members_read(const Session &s, const std::map<std::string, std::string> &members, std::string_view prefix,
                             const std::string &name);

/* m, as members_read checked it, restored into the session s: AUTO's
   settings, the diagram, its solution file and the views of it, with the
   AUTO window opened and the diagram drawn; the error, nothing restored,
   when the solution file cannot be written */
Result<> restore_members(Session &s, Members m);

} // namespace xpp::snapx::auto_members

#endif
