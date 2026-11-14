/* The files module's platform primitives (xpp_files_internal.h) and public
   functions for Windows; Linux and macOS are xpp_files_posix.cpp. The
   one of the files that include <windows.h> (never a header). */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "xpp_files.h"
#include "xpp_files_internal.h"
#include "xpp_win.h"

#include <cctype>
#include <cstddef>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include <shlobj.h>
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>

namespace xpp::files {

namespace {

/* the final component itself is opened, reparse points and non-regular files refused, and a CRT fd and size
   come from that same handle; -1 sets errno. create is an exclusive creation; overwrite opens or creates and
   truncates only after the handle check; binary selects the CRT translation mode. */
int open_plain(const char *path, bool create, bool binary, unsigned long long &size, bool overwrite = false)
{
    /* The core's paths are UTF-8, as the narrow calls beside this one read
       them under the manifest's activeCodePage UTF-8 (assets/xppautx.rc). */
    const std::optional<std::wstring> wide = xpp::win::widen(path, xpp::win::InvalidUtf8::refuse);
    if (!wide) { errno = EINVAL; return -1; }
    HANDLE h = CreateFileW(wide->c_str(), create || overwrite ? GENERIC_WRITE : GENERIC_READ,
                          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                          create ? CREATE_NEW : overwrite ? OPEN_ALWAYS : OPEN_EXISTING,
                          FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    /* never FILE_FLAG_BACKUP_SEMANTICS: it opens folders, which are not
       wanted here, and for a process with backup or restore privilege (an
       administrator) it bypasses the folder's access rules (CI, 2026-10-03:
       --silent wrote into a folder denying writes) */
    if (h == INVALID_HANDLE_VALUE) {
        const DWORD e = GetLastError();
        /* a folder or a folder's junction cannot be opened without backup
           semantics: denied, which names it a refused kind, not an I/O error
           (only the error's name: nothing was opened) */
        const DWORD a = e == ERROR_ACCESS_DENIED ? GetFileAttributesW(wide->c_str()) : INVALID_FILE_ATTRIBUTES;
        if (a != INVALID_FILE_ATTRIBUTES && (a & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
            errno = ELOOP;
            return -1;
        }
        errno = e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND ? ENOENT
              : e == ERROR_FILE_EXISTS || e == ERROR_ALREADY_EXISTS ? EEXIST
              : e == ERROR_ACCESS_DENIED || e == ERROR_SHARING_VIOLATION ? EACCES : EIO;
        return -1;
    }
    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle(h, &info) || GetFileType(h) != FILE_TYPE_DISK
        || (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY))) {
        CloseHandle(h);
        errno = ELOOP; /* xpp_files maps a refused handle to its plain-file error. */
        return -1;
    }
    /* Truncate only the checked plain handle, never the name before checking it. */
    if (overwrite && !SetEndOfFile(h)) {
        CloseHandle(h);
        errno = EIO;
        return -1;
    }
    size = overwrite ? 0 : (static_cast<unsigned long long>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
    const int fd = _open_osfhandle(reinterpret_cast<intptr_t>(h),
                                 (create || overwrite ? _O_WRONLY : _O_RDONLY) | (binary ? _O_BINARY : _O_TEXT));
    if (fd < 0) CloseHandle(h); /* A successful conversion transfers ownership to the CRT. */
    return fd;
}

} // namespace

/* the system temp folder without its trailing backslash; empty when there is none */
std::string temp_base()
{
    std::array<char, MAX_PATH> base; /* GetTempPathA writes it */
    DWORD n = GetTempPathA(static_cast<DWORD>(base.size()), base.data());
    if (n == 0 || n >= base.size()) return std::string();
    if (base[n - 1] == '\\') n--;
    return std::string(base.data(), n);
}


int stat_follow(const char *path, Stat *st) { return _stat64(path, st); }
int stat_name(const char *name, Stat *st) { return _stat64(name, st); } /* a link is found by is_link first */
/* a link is never read or written through; true for a symbolic link or another reparse point (a junction) */
bool is_link(const char *name)
{
    DWORD a = GetFileAttributesA(name);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

bool link_on_way(const char *name) { return is_link(name); }
int make_dir(const char *path) { return _mkdir(path); }
int remove_dir(const char *path) { return _rmdir(path); }
long long own_pid() { return _getpid(); }
/* Only a proved exit permits deleting that PID's scratch folders; a failed query (including access denied) preserves them. */
bool pid_gone(long long pid)
{
    if (pid < 0) return false;
    /* Limited query rights suffice; a denied query never authorizes scratch cleanup. */
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (h == NULL) return pid != 0 && GetLastError() == ERROR_INVALID_PARAMETER;
    DWORD code;
    const bool gone = GetExitCodeProcess(h, &code) && code != STILL_ACTIVE;
    CloseHandle(h);
    return gone;
}

bool owned_by_another(const Stat &) { return false; }

/* Another process may hold the target open for a moment, an antivirus or
   the search indexer reading a file just written: the move then fails with
   a sharing, lock or access error, gone a few milliseconds later (W159's
   push: test_lunch's set files, 2 runs in 3 here). Those errors are tried
   again, MOVE_RETRIES times MOVE_RETRY_MS apart (half a second in all,
   longer than such a scan holds a small file); any other fails at once. */
constexpr int MOVE_RETRIES = 50;
constexpr DWORD MOVE_RETRY_MS = 10;

/* rename from to to, replacing to when it exists (rename() does not) */
int rename_over(const char *from, const char *to)
{
    for (int attempt = 0;; attempt++) {
        if (MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return 0;
        const DWORD e = GetLastError();
        if (attempt == MOVE_RETRIES
            || (e != ERROR_SHARING_VIOLATION && e != ERROR_LOCK_VIOLATION && e != ERROR_ACCESS_DENIED)) {
            errno = e == ERROR_ACCESS_DENIED || e == ERROR_SHARING_VIOLATION || e == ERROR_LOCK_VIOLATION ? EACCES : EIO;
            return -1;
        }
        Sleep(MOVE_RETRY_MS);
    }
}

int open_plain_fd(const char *name, bool writing, unsigned long long &size)
{
    /* and refused again on the handle opened, so nothing swapped in between is followed (W174) */
    return open_plain(name, false, !writing, size, writing);
}

int create_fd(const char *path, bool binary)
{
    unsigned long long size;
    return open_plain(path, true, binary, size);
}

bool drive_root(std::string_view path, std::size_t sep) { return sep == 2 && path[1] == ':'; }

bool is_absolute(std::string_view path)
{
    const auto sep = [](char c) { return c == '/' || c == '\\'; };
    return (!path.empty() && sep(path[0])) ||
           (path.size() > 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':' && sep(path[2]));
}

/* the system Documents folder, including redirected folders; empty when unavailable */
std::string user_documents_folder()
{
    std::array<char, MAX_PATH> path; /* SHGetFolderPathA's required system buffer. */
    if (SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, path.data()) != S_OK) return {};
    return path.data();
}


/* %APPDATA%, the per-user roaming folder, without a trailing backslash; empty when it is not set */
std::string system_config_base()
{
    std::array<char, MAX_PATH> dir; /* GetEnvironmentVariableA writes it */
    const DWORD n = GetEnvironmentVariableA("APPDATA", dir.data(), static_cast<DWORD>(dir.size()));
    if (n == 0 || n >= dir.size()) return std::string();
    return std::string(dir.data(), n);
}


/* the running program's full path, empty when it cannot be had */
std::string program_file()
{
    std::array<char, MAX_PATH> exe; /* GetModuleFileNameA writes it */
    const DWORD n = GetModuleFileNameA(nullptr, exe.data(), static_cast<DWORD>(exe.size()));
    if (n == 0 || n >= exe.size()) return std::string(); /* n == size: cut short */
    return std::string(exe.data(), n);
}


std::string shipped_files_folder(const std::string &exe_folder) { return exe_folder; }

bool make_executable(std::string_view path) { return exists(path); }

int stream_fd(FILE *f)
{
    if (_fileno(f) < 0 && !std::freopen("NUL", "w", f)) return -1;
    return fileno(f);
}

} // namespace xpp::files
