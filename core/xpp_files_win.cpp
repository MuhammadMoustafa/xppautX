/* The files module's platform primitives (xpp_files_internal.h) and public
   functions for Windows; Linux and macOS are xpp_files_posix.cpp. The
   Windows API itself is xpp_win.cpp's. */
#include "xpp_files.h"
#include "xpp_files_internal.h"
#include "xpp_win.h"

#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include <direct.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>

namespace xpp::files {

std::string temp_base() { return xpp::win::temp_folder(); }

int stat_follow(const char *path, Stat *st) { return _stat64(path, st); }
int stat_name(const char *name, Stat *st) { return _stat64(name, st); } /* a link is found by is_link first */
bool is_link(const char *name) { return xpp::win::path_is_link(name); }
bool link_on_way(const char *name) { return is_link(name); }
int make_dir(const char *path) { return _mkdir(path); }
int remove_dir(const char *path) { return _rmdir(path); }
long long own_pid() { return _getpid(); }
bool process_gone(long long pid) { return pid >= 0 && xpp::win::process_gone(static_cast<unsigned long>(pid)); }

bool owned_by_another(const Stat &) { return false; }

int rename_over(const char *from, const char *to) { return xpp::win::move_over(from, to) ? 0 : -1; }

int open_plain_fd(const char *name, bool writing, unsigned long long &size)
{
    /* and refused again on the handle opened, so nothing swapped in between is followed (W174) */
    return xpp::win::open_plain(name, false, !writing, size, writing);
}

int create_fd(const char *path, bool binary)
{
    unsigned long long size;
    return xpp::win::open_plain(path, true, binary, size);
}

bool drive_root(std::string_view path, std::size_t sep) { return sep == 2 && path[1] == ':'; }

bool is_absolute(std::string_view path)
{
    const auto sep = [](char c) { return c == '/' || c == '\\'; };
    return (!path.empty() && sep(path[0])) ||
           (path.size() > 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':' && sep(path[2]));
}

std::string documents_folder() { return xpp::win::documents_folder(); }

std::string system_config_base() { return xpp::win::app_data_folder(); }

std::string program_file() { return xpp::win::executable_path(); }

std::string shipped_files_folder(const std::string &exe_folder) { return exe_folder; }

bool make_executable(std::string_view path) { return exists(path); }

int stream_fd(FILE *f)
{
    if (_fileno(f) < 0 && !std::freopen("NUL", "w", f)) return -1;
    return fileno(f);
}

} // namespace xpp::files
