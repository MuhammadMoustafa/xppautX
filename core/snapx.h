#pragma once
/* The session file, name.snapx (W57, docs/protocol.md "Session files"):
   a zip (xpp_zip.h) of ordinary files, what continuing where the user
   stopped needs, the model included (W103: its every file, model/<name>).
   This header is its pure part (snapx.cpp, no I/O, unit tested by
   tests/test_snapx.cpp): the members' names, the manifest (session.setx:
   the model's first file, the animation it was loaded with, whether the
   data is in) and the model's members. Reading and writing a whole file
   is xpp_session.cpp's; AUTO's members belong to the session too
   (xpp_session_auto.h). C++ only. */
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "model_files.h"
#include "xpp_error.h"
#include "xpp_zip.h"

namespace xpp::setx { class Block; }

namespace xpp::snapx {

inline constexpr std::string_view extension = ".snapx";
inline constexpr int format_version = 2; /* Reject incompatible session manifests (2: the members in the settings syntax, W285). */

/* Account for expanded objects, not their compressed text. No multiplication
   overflows and a refused claim leaves the running byte estimate unchanged. */
bool account_memory(std::size_t &used, std::size_t count, std::size_t bytes_each);
/* A settings list's staged records, checked before making its Blocks. Runtime
   slots and point storage are charged separately to the same byte estimate. */
bool memory_groups(setx::Block &block, std::string_view key, std::vector<setx::Block> &out,
                   std::size_t bytes_each, std::size_t &used);

/* the members, in the order a session file holds them: the manifest,
   the model's (model_folder below), then these */
inline constexpr const char *manifest_member = "session.setx"; /* Manifest below */
inline constexpr const char *set_member = "model.setx";       /* the session's settings (setx_file.h), complete, without graphics */
inline constexpr std::string_view auto_folder = "auto/";       /* AUTO's members, settings, views and optional diagram/orbits (xpp_session_auto.h) */
inline constexpr const char *windows_member = "windows.setx"; /* the plot windows and what they display */
inline constexpr const char *marks_member = "marks.setx";     /* labels, arrows and markers, frozen curves */
inline constexpr const char *frozen_member = "frozen.npz";    /* the frozen curves' points */
inline constexpr const char *data_member = "data.npz";        /* the data table */
inline constexpr const char *sliders_member = "sliders.setx"; /* every slider slot, including empty ones */
inline constexpr const char *nullclines_member = "nullclines.setx"; /* current and frozen segments with their axes */
inline constexpr const char *random_member = "random.txt";    /* "seed N" (the next Go's), "wiener v..." (their current values), then the generator's state (xpp::Random::save's text) */

/* the folder of the model's members: model/<name> is its file name, as
   the model names it (model_files.h), the model's own first */
inline constexpr std::string_view model_folder = "model/";

/* session.setx: a `session` group of version, model, anifile and data (setx.h) */
struct Manifest {
    int version = format_version;
    std::string model_name; /* the model's own file, as it names it: the member model/<model_name> */
    std::string anifile;    /* --anifile's animation the model was loaded with, one of its files ("": none) */
    bool data = false;      /* data.npz holds the data table (a session file's alone) */
    bool operator==(const Manifest &) const = default;
};

/* The session manifest: version, model, animation and data. */
Result<std::string> manifest_text(const Manifest &m);
/* text, the manifest member file (its errors' place), read whole: not
   a session manifest (another version, an older session's layout), a key
   manifest_text does not write or one given twice, a data other than
   0 or 1, no model: the error at its line and column */
xpp::Result<Manifest> parse_manifest(std::string file, std::string_view text);

/* files as members of the folder model_folder, in their order, after entries' */
void add_model_members(std::vector<zip::Entry> &entries, std::span<const ModelFile> files);
/* the files of entries' model_folder, in the archive's order: nothing when
   the model's own file, model_name, is not one of them */
std::optional<std::vector<ModelFile>> model_members(const std::vector<zip::Entry> &entries, std::string_view model_name);

/* name with ext added unless it has it */
std::string with_extension(std::string_view name, std::string_view ext);

/* path names a session file: it ends in .snapx (case ignored) */
bool is_session_file(std::string_view path);
/* name with .snapx added unless it has it */
std::string session_file_name(std::string_view name);

} // namespace xpp::snapx

