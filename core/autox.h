#ifndef XPP_AUTOX_H
#define XPP_AUTOX_H
/* AUTO's own file, name.autox (W92, docs/protocol.md "AUTO files"): AUTO's
   work alone, without a whole session, as a zip (xpp_zip.h) of ordinary
   files: the manifest (snapx.h's Manifest), the model it is a diagram of,
   every file of it (W103: snapx.h's model members, model/<name>), AUTO's
   settings, the diagram at full precision and AUTO's solution file (the
   orbits a grab restarts from). The AUTO window's File/Save diagram
   writes one; opening one (File > Open model, the command line, AUTO's
   Load diagram) loads its model and then the diagram (xpp_session.h's
   reader, the one both files that carry a model share). A session file
   (snapx.h) holds AUTO's members too, in its folder auto/.

   The first part is pure (autox.cpp: the members' names and the text of
   settings.txt and diagram.csv, no I/O, unit tested by
   tests/test_autox.cpp); the second gathers AUTO's state of this session
   into the members and puts it back (autox_io.cpp), and imports an
   XPPAUT .auto (AUTO's Load diagram).
   C++ only. */
#include <deque>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "auto_settings.h"
#include "diagram.h"
#include "display_state.h"
#include "xpp_zip.h"

namespace xpp {
struct Session; /* session.h */
}

namespace xpp::autox {

inline constexpr std::string_view extension = ".autox";
/* the manifest's first line, "xppautX autox 1" (snapx::manifest_text) */
inline constexpr std::string_view kind = "autox";

/* the members, in the order an .autox holds them: the manifest, the
   model's (snapx.h's model_folder), then AUTO's */
inline constexpr const char *manifest_member = "autox.txt";   /* snapx.h's Manifest, of kind autox */
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
/* text's settings, every one of them given; nothing when a line is not
   one (a key it does not know is left for a later writer) */
std::optional<AutoSettingsSet> parse_settings(std::string_view text);

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

/* views.txt: one line per view in order, "view PLOT VAR PAR1 PAR2 XMIN
   XMAX YMIN YMAX ZOOMX ZOOMY" (names and numbers as settings_text's, a
   zoom LO:HI or "-" for none), then "active K" */
std::string views_text(const SavedViews &v);
/* text's views; nothing when a line is not one, there is no view or the
   active one is not one of them */
std::optional<SavedViews> parse_views(std::string_view text);

/* diagram.csv: a header row of names, then one row per point in the
   order stored, every field of DIAGRAM: calc, ibr, ntot, itp, lab, nfpar,
   icp1..icp4, flag2, from, norm, per, torper, par1..par20, then for each
   variable x of vars (the model's first node) u0.x, uhi.x, ulo.x, ubar.x
   and last evr1, evi1, ... evrN, eviN (its eigenvalues or Floquet
   multipliers, auto_stability.h; zeros: not computed). Numbers as
   settings_text's, so a point reads back bit for bit. */
std::string diagram_csv(const std::deque<DiagramPoint> &points, std::span<const std::string> vars);
/* text's points, of n variables each, their arrays filled (the DIAGRAM
   pointers and index are diagram_restore's); nothing when it is not a
   diagram of n variables or a row does not read */
std::optional<std::deque<DiagramPoint>> parse_diagram_csv(std::string_view text, int n);

/* ---- this session's (autox_io.cpp) ---- */

/* AUTO's work in the session s as an .autox's bytes, its model included;
   nothing when the diagram is empty (or AUTO's solution file cannot be
   read, which an error message says) */
std::optional<std::string> file_bytes(const Session &s);

/* AUTO's members (settings, diagram, solutions, views) of the session s
   after entries', each named prefix and its name (a session file's "auto/") */
void add_members(const Session &s, std::vector<xpp::zip::Entry> &entries, std::string_view prefix);

/* AUTO's members of a file (named name in messages; prefix as
   add_members') restored into the session s, whose model the file's is:
   AUTO's settings, the diagram, its solution file and the views of it,
   with the AUTO window opened and the diagram drawn. False with an error message when nothing
   was restored. */
bool restore_members(Session &s, const std::map<std::string, std::string> &members, std::string_view prefix, const std::string &name);

/* AUTO's File/Load diagram of an XPPAUT .auto, path: imported into the
   session s, its
   diagram to the 6 digits it prints. The diagram before is replaced but
   not reset: the caller asks for that. False with an error message when
   nothing was read. (An .autox is opened as a model is: xpp_model_open.) */
bool import_file(Session &s, const std::string &path);

} // namespace xpp::autox

#endif
