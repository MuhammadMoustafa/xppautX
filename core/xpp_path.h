#pragma once

/* core/xpp_path.cpp: the rules of a file's name as text (W731): no file is touched, so the kernel
   owns them and every module may use them. A path's folder and base name, its extension, whether it
   is absolute, the names the program writes (a model's outputs, a run's file), whether a name is a
   plain, safe base name (the page's files, a model's .anix), and the Error of a file that cannot be
   opened. What needs the disk or the working folder (absolute, exists, a folder's contents) is the
   file system's (xpp_fs.h for the inner rings, xpp_files.h at the edge). Namespace xpp::files, as the
   names always were. */

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "xpp_error.h"

namespace xpp::files {

inline constexpr std::size_t NAME_MAX_BYTES = 255; /* Common filesystem basename limit; refuse longer names without cutting them. */

#ifdef _WIN32
inline constexpr char SEP = '\\';
inline constexpr std::string_view SEPARATORS = "/\\"; /* every character that ends a folder name: Windows takes both */
#else
inline constexpr char SEP = '/';
inline constexpr std::string_view SEPARATORS = "/";
#endif

/* All offered defaults: model base without its extension, optional qualifier, extension. */
std::string output_name(std::string_view model_file, std::string_view ext,
                        std::string_view what = {});

/* the file of run i of a run that writes a file per run (a sweep's data, a batch run's picture): the path's
   base name with the run's number before its extension, out.dat -> out_0007.dat (4 digits, more when i needs
   them); the dot is searched in the base name only, so a dot in a folder's name is not an extension */
std::string run_file_name(std::string_view path, int i);
/* Case-insensitive suffix on the basename; an extension alone is not a filename. */
bool has_extension(std::string_view path, std::string_view ext);
bool name_ok(std::string_view name, bool allow_hidden = false);
/* what a name that name_ok refuses is, in words, for an error */
inline constexpr const char *NOT_A_PLAIN_NAME = "not a plain file name in the model's folder";

/* path's folder ("" when it names none: a bare name) and its last part,
   either separator ('/', or '\' too on Windows) */
std::pair<std::string, std::string> split_path(std::string_view path);
/* path starts at the root ("/x", and on Windows "C:\x" or "\x") */
bool is_absolute(std::string_view path);
/* the file or folder `name` in the folder dir, with the platform's separator */
std::string join(std::string_view dir, std::string_view name);
/* path under the folder dir, one '/' between them (dir may end in a separator already): the text of
   absolute() once the folder is known */
std::string append_path(std::string_view dir, std::string_view path);

/* Every failed open names the requested file and the system or explicit reason. */
Error open_error(std::string where, std::string_view file, std::string_view why = {});

} // namespace xpp::files
