#pragma once
/* The session file, name.snapx (W57, docs/protocol.md "Session files"):
   a zip (xpp_zip.h) of ordinary files, what continuing where the user
   stopped needs, the model included (W103: its every file, model/<name>).
   This header is its pure part (snapx.cpp, no I/O, unit tested by
   tests/test_snapx.cpp): the members' names, the manifest (the first keys of
   the state file's session statement: the model's first file, the animation it was
   loaded with, whether the data is in) and the model's members. Reading and writing a whole file
   is xpp_session.cpp's; autoX's members belong to the session too
   (xpp_session_autox.h). C++ only. */
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "model_files.h"
#include "xpp_error.h"
#include "xpp_zip.h"

namespace xpp::odex { class Block; class Out; }

namespace xpp::snapx {

inline constexpr std::string_view extension = ".snapx";

/* Account for expanded objects, not their compressed text. No multiplication
   overflows and a refused claim leaves the running byte estimate unchanged. */
bool account_memory(std::size_t &used, std::size_t count, std::size_t bytes_each);
/* A settings list's staged records, checked before making its Blocks. Runtime
   slots and point storage are charged separately to the same byte estimate. */
bool memory_groups(odex::Block &block, std::string_view key, std::vector<odex::Block> &out,
                   std::size_t bytes_each, std::size_t &used);

/* the members, in the order a session file holds them: the state file,
   the model's (model_folder below), then these */
inline constexpr const char *state_member = "session.odex"; /* the state: `version`, par and init lines, options and one session statement (xpp_session.cpp; docs/protocol.md) */
inline constexpr std::string_view autox_folder = "autox/";  /* autoX's diagram and solution members (xpp_session_autox.h) */
inline constexpr const char *frozen_member = "frozen.npz";  /* the frozen curves' points */
inline constexpr const char *data_member = "data.npz";      /* the data table */

/* the folder of the model's members: model/<name> is its file name, as
   the model names it (model_files.h), the model's own first */
inline constexpr std::string_view model_folder = "model/";

/* the manifest, the first keys of the state file's session statement: `model` and `data` */
struct Manifest {
    std::string model_name; /* the model's own file, as it names it: the member model/<model_name> */
    bool data = false;      /* data.npz holds the data table (a session file's alone) */
    bool operator==(const Manifest &) const = default;
};

/* m's keys written into the session statement being written */
void write_manifest(odex::Out &out, const Manifest &m);
/* the manifest keys of session, the session statement's group of a state file read whole: not a session of this
   version, a model that is no .odex: ReadFailed at the value */
Manifest read_manifest(odex::Block &session);
/* m as a state file of its own, the manifest alone (a model-only archive's first member) */
Result<std::string> manifest_text(const Manifest &m);
/* text, the state member file (its errors' place), read whole for its manifest alone: not a session file (another
   version, a missing key, a model that is no .odex): the error at its line and column */
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

