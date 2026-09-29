#ifndef XPP_AUTOX_H
#define XPP_AUTOX_H
/* AUTO's own file, name.autox (W92, docs/protocol.md "AUTO files"): AUTO's
   work alone, without a whole session, as a zip (xpp_zip.h) of ordinary
   files: the manifest (the model's path, fingerprint and names, snapx.h's
   Manifest), AUTO's settings, the diagram at full precision and AUTO's
   solution file (the orbits a grab restarts from). The AUTO window's File/
   Save diagram writes one, its Load diagram reads one (or imports an
   XPPAUT .auto), and a session file (snapx.h) holds one as its
   model.autox.

   The first part is pure (autox.cpp: the members' names and the text of
   settings.txt and diagram.csv, no I/O, unit tested by
   tests/test_autox.cpp); the second writes and reads the file for this
   session (autox_io.cpp), the one reader and writer every caller shares.
   C++ only. */
#include <deque>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "auto_settings.h"
#include "diagram.h"

namespace xpp::autox {

inline constexpr std::string_view extension = ".autox";
/* the manifest's first line, "xppautX autox 1" (snapx::manifest_text) */
inline constexpr std::string_view kind = "autox";

/* the members, in the order an .autox holds them */
inline constexpr const char *manifest_member = "autox.txt";   /* snapx.h's Manifest, of kind autox */
inline constexpr const char *settings_member = "settings.txt"; /* settings_text below */
inline constexpr const char *diagram_member = "diagram.csv";   /* diagram_csv below */
inline constexpr const char *solutions_member = "solutions.s"; /* AUTO's solution file (fort.8), as AUTO wrote it */

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

/* bytes are a zip, which an .autox is (an XPPAUT .auto is text) */
bool is_zip(std::string_view bytes);

/* ---- this session's (autox_io.cpp) ---- */

/* AUTO's work in this session as an .autox's bytes; nothing when the
   diagram is empty (or AUTO's solution file cannot be read, which an
   error message says) */
std::optional<std::string> file_bytes();

/* the .autox bytes (named name in messages) restored into this session:
   AUTO's settings, the diagram and its solution file, with the AUTO
   window opened and the diagram drawn. A file of another model (other
   variables or parameters) is refused; a model edited since (its
   fingerprint) is warned about when warn_changed. False with an error
   message when nothing was restored. */
bool load_bytes(std::string_view bytes, const std::string &name, bool warn_changed);

/* AUTO's File/Load diagram without its dialog: path read, an .autox
   (load_bytes, warning of an edited model) or an XPPAUT .auto (imported, its
   diagram to the 6 digits it prints). The diagram before is replaced but
   not reset: the caller asks for that. False with an error message when
   nothing was read. */
bool load_file(const std::string &path);

} // namespace xpp::autox

#endif
