/* Platform filesystem primitives shared by xpp_files.cpp (the page's
   files and the core's own file operations) and xpp_files_dir.cpp (folder
   listing, the current folder, AUTO's scratch folders): stat-without-
   following-a-link, mkdir/rmdir, the process id and whether it is still
   running, and the scratch-folder base. Each wraps one OS call, so it is
   inline rather than split into its own translation unit. C++ only,
   internal to the two xpp_files*.cpp files (never a public header). */
#ifndef XPP_FILES_INTERNAL_H
#define XPP_FILES_INTERNAL_H

#ifndef __cplusplus
#error "xpp_files_internal.h is C++ only"
#endif

#include <cerrno>
#include <cstdlib>
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

#ifdef _WIN32
using Stat = struct _stat64;
inline int stat_follow(const char *path, Stat *st) { return _stat64(path, st); }
inline bool is_link(const char *name) { return xpp::win32::path_is_link(name); }
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
inline int make_dir(const char *path) { return mkdir(path, 0700); }
inline int remove_dir(const char *path) { return rmdir(path); }
inline long long own_pid() { return getpid(); }
/* kill(pid, 0) says ESRCH: no such process. A live pid, or one this user
   may not signal (EPERM), is not gone. */
inline bool process_gone(long long pid) { return kill(static_cast<pid_t>(pid), 0) != 0 && errno == ESRCH; }
inline constexpr char SEP = '/';
#endif

} // namespace xpp::files
#endif
