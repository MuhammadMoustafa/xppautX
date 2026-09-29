#ifndef XPP_SNAPX_H
#define XPP_SNAPX_H
/* The session file, name.snapx (W57, docs/protocol.md "Session files"):
   a zip (xpp_zip.h) of ordinary files, what continuing where the user
   stopped needs. This header is its pure part (snapx.cpp, no I/O, unit
   tested by tests/test_snapx.cpp): the members' names, the manifest
   (session.txt: which model, its fingerprint, the names it had) and the
   fingerprint itself. Writing and reading the members is xpp_session.cpp's.
   C++ only. */
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
inline constexpr const char *auto_member = "model.auto";      /* AUTO's File/Save diagram's file */
inline constexpr const char *windows_member = "windows.set";  /* the plot windows and what they display */
inline constexpr const char *marks_member = "marks.set";      /* labels, arrows and markers, frozen curves */
inline constexpr const char *frozen_member = "frozen.npz";    /* the frozen curves' points */
inline constexpr const char *data_member = "data.npz";        /* the data table */

/* session.txt: one "key value" line each, the first naming the format */
struct Manifest {
    int version = 1;
    std::string model;      /* the .ode's absolute path when it was saved */
    std::string model_name; /* its file name, looked for beside the .snapx first */
    std::string sha256;     /* fingerprint() of the .ode and its includes */
    int node = 0, nmarkov = 0;
    std::vector<std::string> vars; /* the model's variables and auxiliaries (uvar_names) */
    std::vector<std::string> pars; /* its parameters (upar_names) */
    bool data = false;             /* data.npz holds the data table */
    bool operator==(const Manifest &) const = default;
};

std::string manifest_text(const Manifest &m);
/* text's manifest, or nothing when it is not one (another format's first
   line, a later version, a count that is not a number) */
std::optional<Manifest> parse_manifest(std::string_view text);

/* the fingerprint of a model: the SHA-256 (64 hex digits) of its files'
   contents, the model's own first, then each file it includes, in the
   order read (each file's length first, so no two lists of files give the
   same bytes); an edit to any of them changes it */
std::string fingerprint(std::span<const std::string> files);

/* path names a session file: it ends in .snapx (case ignored) */
bool is_session_file(std::string_view path);
/* name with .snapx added unless it has it */
std::string session_file_name(std::string_view name);

} // namespace xpp::snapx

#endif
