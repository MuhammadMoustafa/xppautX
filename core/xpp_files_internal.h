/* The platform primitives of the files module, shared by xpp_files.cpp (the
   page's files and the core's own file operations) and xpp_files_dir.cpp
   (folder listing, the current folder, autoX's scratch folders): stat-without-
   following-a-link, mkdir/rmdir, the process id and whether it is still
   running, the rename over a file, the plain-file opens and the folder
   syntax. Each wraps one OS call or a short OS-specific sequence and is
   implemented once per platform, the build picking the files:
   xpp_files_win.cpp (Windows), xpp_files_posix.cpp (Linux and macOS), and
   xpp_files_linux.cpp / xpp_files_mac.cpp for the public functions those two
   answer differently (config_base, program_file, program_dir). The shared
   path walk is implemented by xpp_files.cpp. C++ only, internal to the
   xpp_files*.cpp files (never a public header). */
#pragma once

#ifndef __cplusplus
#error "xpp_files_internal.h is C++ only"
#endif

#include <cstddef>
#include <cstdio>
#include <string_view>
#include <vector>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>

namespace xpp::files {
/* Owner-only exclusive open: create_new guards a user write; dir_writable
   checks the parent and uses it only for its own unique, hidden probe. */
FILE *create_plain(std::string_view path, bool binary);
/* One anchor rule for reads and folder walks. The caller supplies absolute
   paths, normalized for resolve_path, raw for walks that must check ../ too.
   Only components below their common ancestor cross the trust boundary. */
struct PathPart { std::string path; std::string name; bool below_anchor; };
std::vector<PathPart> path_parts(std::string_view path, std::string_view folder);

#ifdef _WIN32
using Stat = struct _stat64;
inline constexpr char SEP = '\\';
inline constexpr std::string_view SEPARATORS = "/\\"; /* every character that ends a folder name: Windows takes both */
#else
using Stat = struct stat;
inline constexpr char SEP = '/';
inline constexpr std::string_view SEPARATORS = "/";
#endif

int stat_follow(const char *path, Stat *st);
int stat_name(const char *name, Stat *st); /* never follows a link (Windows: a link is found by is_link first) */
bool is_link(const char *name);
/* a link on the way that anyone but root could have planted (Windows: any link) */
bool link_on_way(const char *name);
int make_dir(const char *path);
int remove_dir(const char *path);
long long own_pid();
bool pid_gone(long long pid);
/* whether a folder under a scratch base belongs to another user (never on Windows) */
bool owned_by_another(const Stat &st);
/* rename a file over an existing one: 0, or -1 */
int rename_over(const char *from, const char *to);
/* a file descriptor of the plain file name, or -1 with errno set (ELOOP: a
   link or not a regular file, ENOENT: not there); refused again on the
   descriptor opened, so nothing swapped in between is followed (W174).
   size: the file's size when read, 0 when written (truncated once proved plain) */
int open_plain_fd(const char *name, bool writing, unsigned long long &size);
/* the descriptor of a new, exclusively created file, or -1 */
int create_fd(const char *path, bool binary);
/* whether path's root is a drive ("C:\x": sep is the index of its last
   separator): always false off Windows */
bool drive_root(std::string_view path, std::size_t sep);
/* the user's Documents folder, "" when there is none */
std::string user_documents_folder();
/* the system's per-user settings folder (config_base without the override), "" when there is none */
std::string system_config_base();
/* where the files the program ships are, given the running program's folder */
std::string shipped_files_folder(const std::string &exe_folder);
/* the room program_file gives the system to write the executable's path in: PATH_MAX on Linux and macOS (4096 and
   1024); a longer one is no folder this program can be installed in (Windows: xpp_win's executable_path) */
inline constexpr std::size_t PATH_BUFFER_BYTES = 4096;

} // namespace xpp::files
