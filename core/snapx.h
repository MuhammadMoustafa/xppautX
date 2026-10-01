#ifndef XPP_SNAPX_H
#define XPP_SNAPX_H
/* The session file, name.snapx (W57, docs/protocol.md "Session files"):
   a zip (xpp_zip.h) of ordinary files, what continuing where the user
   stopped needs, the model included (W103: its every file, model/<name>).
   This header is its pure part (snapx.cpp, no I/O, unit tested by
   tests/test_snapx.cpp): the members' names, the manifest (session.txt:
   the model's first file, the animation it was loaded with, whether the
   data is in) and the model's members. Reading and writing a whole file
   is xpp_session.cpp's, for both kinds that carry a model. AUTO's own
   file, name.autox (autox.h), has the same manifest under another first
   line and the same model members. C++ only. */
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "model_files.h"
#include "xpp_error.h"
#include "xpp_zip.h"

namespace xpp::snapx {

inline constexpr std::string_view extension = ".snapx";

/* the members, in the order a session file holds them: the manifest,
   the model's (model_folder below), then these */
inline constexpr const char *manifest_member = "session.txt"; /* Manifest below */
inline constexpr const char *set_member = "model.set";        /* the session's set file */
inline constexpr std::string_view auto_folder = "auto/";       /* AUTO's members, as an .autox has them (autox.h) */
inline constexpr const char *windows_member = "windows.set";  /* the plot windows and what they display */
inline constexpr const char *marks_member = "marks.set";      /* labels, arrows and markers, frozen curves */
inline constexpr const char *frozen_member = "frozen.npz";    /* the frozen curves' points */
inline constexpr const char *data_member = "data.npz";        /* the data table */
inline constexpr const char *random_member = "random.txt";    /* "seed N" (the next Go's), "wiener v..." (their current values), then the generator's state (xpp::Random::save's text) */

/* the kind of file a manifest's first line names, "xppautX session 1" */
inline constexpr std::string_view session_kind = "session";

/* the folder of the model's members: model/<name> is its file name, as
   the model names it (model_files.h), the model's own first */
inline constexpr std::string_view model_folder = "model/";

/* session.txt: one "key value" line each, the first naming the format */
struct Manifest {
    int version = 1;
    std::string model_name; /* the model's own file, as it names it: the member model/<model_name> */
    std::string anifile;    /* -anifile's animation the model was loaded with, one of its files ("": none) */
    bool data = false;      /* data.npz holds the data table (a session file's alone) */
    bool operator==(const Manifest &) const = default;
};

/* the manifest of a file of this kind: "xppautX <kind> 1" first, and the
   data line only in a session file's */
std::string manifest_text(const Manifest &m, std::string_view kind = session_kind);
/* text, the manifest member file (its errors' place), read whole: not
   one of this kind (another format's first line, a later version), a key
   manifest_text does not write or one given twice, a data line other than
   0 or 1, no name: the error at its line (xpp::read_lines) */
xpp::Result<Manifest> parse_manifest(std::string file, std::string_view text, std::string_view kind = session_kind);

/* files as members of the folder model_folder, in their order, after entries' */
void add_model_members(std::vector<zip::Entry> &entries, std::span<const ModelFile> files);
/* the files of entries' model_folder, in the archive's order: nothing when
   the model's own file, model_name, is not one of them */
std::optional<std::vector<ModelFile>> model_members(const std::vector<zip::Entry> &entries, std::string_view model_name);

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
