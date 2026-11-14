/* The files module's platform primitives (xpp_files_internal.h) and public
   functions for Linux and macOS, which share the POSIX file API; Windows is
   xpp_files_win.cpp. What Linux and macOS answer differently is
   xpp_files_linux.cpp and xpp_files_mac.cpp. */
#include "xpp_files.h"
#include "xpp_files_internal.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace xpp::files {

std::string temp_base()
{
    const char *base = std::getenv("TMPDIR");
    return base && base[0] ? base : "/tmp";
}

int stat_follow(const char *path, Stat *st) { return stat(path, st); }
int stat_name(const char *name, Stat *st) { return lstat(name, st); } /* never follows a link */

bool is_link(const char *name)
{
    struct stat st;
    return lstat(name, &st) == 0 && S_ISLNK(st.st_mode);
}

/* Only root can plant a root-owned link in a root-owned folder with no
   group/other write permission (OpenSSH StrictModes' ownership rule). */
bool link_on_way(const char *name)
{
    struct stat link;
    if (lstat(name, &link) != 0 || !S_ISLNK(link.st_mode)) return false;
    if (link.st_uid != 0) return true;
    const auto parent = std::filesystem::path(name).parent_path();
    struct stat folder;
    return stat(parent.empty() ? "." : parent.c_str(), &folder) != 0 ||
           folder.st_uid != 0 || (folder.st_mode & (S_IWGRP | S_IWOTH)) != 0;
}

int make_dir(const char *path) { return mkdir(path, 0700); }
int remove_dir(const char *path) { return rmdir(path); }
long long own_pid() { return getpid(); }

/* kill(pid, 0) says ESRCH: no such process. A live pid, or one this user
   may not signal (EPERM), is not gone. */
bool pid_gone(long long pid) { return kill(static_cast<pid_t>(pid), 0) != 0 && errno == ESRCH; }

/* a folder another user made under our name is not ours to empty */
bool owned_by_another(const Stat &st) { return st.st_uid != geteuid(); }

int rename_over(const char *from, const char *to) { return std::rename(from, to); }

int open_plain_fd(const char *name, bool writing, unsigned long long &size)
{
    /* O_NOFOLLOW: a link made between the check and the open is not followed */
    int fd = ::open(name, (writing ? O_WRONLY | O_CREAT : O_RDONLY) | O_NOFOLLOW | O_NONBLOCK, 0666);
    if (fd < 0) return -1;
    struct stat fst;
    if (fstat(fd, &fst) != 0 || !S_ISREG(fst.st_mode)) {
        close(fd);
        errno = ELOOP; /* refused */
        return -1;
    }
    /* Do not truncate a name before proving its opened object is plain. */
    if (writing && ftruncate(fd, 0) != 0) {
        const int error = errno;
        close(fd);
        errno = error;
        return -1;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
    size = writing ? 0 : static_cast<unsigned long long>(fst.st_size);
    return fd;
}

int create_fd(const char *path, bool)
{
    return ::open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0666);
}

bool drive_root(std::string_view, std::size_t) { return false; }

bool is_absolute(std::string_view path) { return !path.empty() && path[0] == '/'; }

std::string user_documents_folder()
{
    const char *home = std::getenv("HOME");
    return home && *home ? join(home, "Documents") : std::string();
}

bool make_executable(std::string_view path)
{
    constexpr mode_t PROGRAM_MODE = 0755; /* what the packages install a program with */
    return chmod(std::string(path).c_str(), PROGRAM_MODE) == 0;
}

int stream_fd(FILE *f) { return fileno(f); }

} // namespace xpp::files
