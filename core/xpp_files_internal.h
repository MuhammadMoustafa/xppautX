/* Platform filesystem primitives shared by xpp_files.cpp (the page's
   files and the core's own file operations) and xpp_files_dir.cpp (folder
   listing, the current folder, AUTO's scratch folders): stat-without-
   following-a-link, mkdir/rmdir, the process id and whether it is still
   running, and the scratch-folder base. Each wraps one OS call, so it is
   inline rather than split into its own translation unit. The shared path
   walk is implemented by xpp_files.cpp. C++ only,
   internal to the two xpp_files*.cpp files (never a public header). */
#pragma once

#ifndef __cplusplus
#error "xpp_files_internal.h is C++ only"
#endif

#include <cerrno>
#include <cstdio>
#include <string_view>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#include "xpp_win32.h"
#include <direct.h>
#include <process.h>
#else
#include <signal.h>
#include <unistd.h>
#endif

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
inline int stat_follow(const char *path, Stat *st) { return _stat64(path, st); }
inline bool is_link(const char *name) { return xpp::win32::path_is_link(name); }
inline bool link_on_way(const char *name) { return is_link(name); }
inline int make_dir(const char *path) { return _mkdir(path); }
inline int remove_dir(const char *path) { return _rmdir(path); }
inline long long own_pid() { return _getpid(); }
inline bool process_gone(long long pid) { return pid >= 0 && xpp::win32::process_gone(static_cast<unsigned long>(pid)); }
inline constexpr char SEP = '\\';
#else
using Stat = struct stat;
inline int stat_follow(const char *path, Stat *st) { return stat(path, st); }
inline bool is_link(const char *name)
{
    struct stat st;
    return lstat(name, &st) == 0 && S_ISLNK(st.st_mode);
}
/* Only root can plant a root-owned link in a root-owned folder with no
   group/other write permission (OpenSSH StrictModes' ownership rule). */
inline bool link_on_way(const char *name)
{
    struct stat link;
    if (lstat(name, &link) != 0 || !S_ISLNK(link.st_mode)) return false;
    if (link.st_uid != 0) return true;
    const auto parent = std::filesystem::path(name).parent_path();
    struct stat folder;
    return stat(parent.empty() ? "." : parent.c_str(), &folder) != 0 ||
           folder.st_uid != 0 || (folder.st_mode & (S_IWGRP | S_IWOTH)) != 0;
}
inline int make_dir(const char *path) { return mkdir(path, 0700); }
inline int remove_dir(const char *path) { return rmdir(path); }
inline long long own_pid() { return getpid(); }
/* kill(pid, 0) says ESRCH: no such process. A live pid, or one this user
   may not signal (EPERM), is not gone. */
inline bool process_gone(long long pid) { return kill(static_cast<pid_t>(pid), 0) != 0 && errno == ESRCH; }
inline constexpr char SEP = '/';
#endif

} // namespace xpp::files
