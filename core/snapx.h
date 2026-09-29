#ifndef XPP_SNAPX_H
#define XPP_SNAPX_H
/* The session file, name.snapx (W57, docs/protocol.md "Session files"):
   a zip (xpp_zip.h) of ordinary files, what continuing where the user
   stopped needs. This header is its pure part (snapx.cpp, no I/O, unit
   tested by tests/test_snapx.cpp): the members' names, the manifest
   (session.txt: which model, its fingerprint, the names it had) and the
   fingerprint itself. Writing and reading the members is xpp_session.cpp's.
   AUTO's own file, name.autox (autox.h), has the same manifest under
   another first line and is a member of a session file. C++ only. */
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace xpp::snapx {

inline constexpr std::string_view extension = ".snapx";

/* the members, in the order a session file holds them */
inline constexpr const char *manifest_member = "session.txt"; /* Manifest below */
inline constexpr const char *set_member = "model.set";        /* File/Write set's file */
inline constexpr const char *autox_member = "model.autox";    /* AUTO's File/Save diagram's file (autox.h) */
inline constexpr const char *windows_member = "windows.set";  /* the plot windows and what they display */
inline constexpr const char *marks_member = "marks.set";      /* labels, arrows and markers, frozen curves */
inline constexpr const char *frozen_member = "frozen.npz";    /* the frozen curves' points */
inline constexpr const char *data_member = "data.npz";        /* the data table */

/* a session file of W57 to W91 held AUTO's diagram as XPPAUT's .auto
   file, which Open session still imports */
inline constexpr const char *old_auto_member = "model.auto";

/* the kind of file a manifest's first line names, "xppautX session 1" */
inline constexpr std::string_view session_kind = "session";

/* session.txt: one "key value" line each, the first naming the format */
struct Manifest {
    int version = 1;
    std::string model;      /* the .ode's absolute path when it was saved */
    std::string model_name; /* its file name, looked for beside the .snapx first */
    std::string sha256;     /* fingerprint() of the .ode and its includes */
    int node = 0, nmarkov = 0;
    std::vector<std::string> vars; /* the model's variables and auxiliaries (uvar_names) */
    std::vector<std::string> pars; /* its parameters (upar_names) */
    bool data = false;             /* data.npz holds the data table (a session file's alone) */
    bool operator==(const Manifest &) const = default;
};

/* the manifest of a file of this kind: "xppautX <kind> 1" first, and the
   data line only in a session file's */
std::string manifest_text(const Manifest &m, std::string_view kind = session_kind);
/* text's manifest, or nothing when it is not one of this kind (another
   format's first line, a later version, a count that is not a number) */
std::optional<Manifest> parse_manifest(std::string_view text, std::string_view kind = session_kind);

/* the fingerprint of a model: the SHA-256 (64 hex digits) of its files'
   contents, the model's own first, then each file it includes, in the
   order read (each file's length first, so no two lists of files give the
   same bytes); an edit to any of them changes it */
std::string fingerprint(std::span<const std::string> files);

/* path ends in extension ext (".snapx", ".autox"; case ignored) */
bool has_extension(std::string_view path, std::string_view ext);
/* name with ext added unless it has it */
std::string with_extension(std::string_view name, std::string_view ext);

/* path names a session file: it ends in .snapx (case ignored) */
bool is_session_file(std::string_view path);
/* name with .snapx added unless it has it */
std::string session_file_name(std::string_view name);

} // namespace xpp::snapx

#endif
