/* Folder operations: see xpp_files.h. Split out of xpp_files.cpp (W46b,
   folded in from the old core/read_dir.cpp) once the merged file grew
   past about 900 lines: xpp_files_is_dir, xpp_files_dir_writable, the
   scratch folders AUTO runs in, the folder listing, the Unix-style
   wildcard match and the file selector's current folder. */
#include "xpp_files.h"
#include "xpp_files_internal.h"
#include "xpp_io.h"
#include "xpp_log.h"
#include "xpp_mem.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h> /* getcwd, chdir (MinGW's unistd.h provides both) */

using namespace xpp::files;

int xpp_files_is_dir(const char *path)
{
    Stat st;
    return path && stat_follow(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int xpp_files_dir_writable(const char *dir)
{
    if (dir == nullptr || dir[0] == 0) return 0;
    try {
        std::string probe = xpp::format("{}/.xppautx_homecheck", dir);
        xpp::UniqueFile fp(std::fopen(probe.c_str(), "w"));
        if (!fp) return 0;
        fp.reset();
        std::remove(probe.c_str());
        return 1;
    } catch (const std::bad_alloc &) {
        xpp_out_of_memory("probing a folder");
    }
}

/* ---- AUTO's scratch folders ------------------------------------------------------ */

namespace {

/* name is exactly "xppautoX-<pid>-<N>" (digits; the pid may be negative
   as %ld reads it): *pid */
bool scratch_dir_pid(std::string_view name, long long *pid)
{
    constexpr std::string_view prefix = "xppautoX-";
    if (!name.starts_with(prefix)) return false;
    const char *p = name.data() + prefix.size(), *end = name.data() + name.size();
    std::from_chars_result r = std::from_chars(p, end, *pid);
    if (r.ec != std::errc() || r.ptr == end || *r.ptr != '-') return false;
    int idx;
    r = std::from_chars(r.ptr + 1, end, idx);
    return r.ec == std::errc() && r.ptr == end;
}

} // namespace

std::string xpp_files_make_temp_dir()
{
    try {
        std::string base = temp_base();
        if (base.empty()) return {};
        for (int i = 0; i < 1000; i++) { /* a crashed run with our pid may have left one */
            std::string path = xpp::format("{}{}xppautoX-{}-{}", base, SEP, own_pid(), i);
            if (make_dir(path.c_str()) == 0) return path;
            if (errno != EEXIST) break;
        }
    } catch (const std::bad_alloc &) {
        xpp_out_of_memory("making the scratch folder");
    }
    return {};
}

void xpp_files_remove_temp_dir(const char *dir)
{
    if (dir == nullptr) return;
    try {
        if (DIR *d = opendir(dir)) {
            while (struct dirent *e = readdir(d)) {
                if (std::strcmp(e->d_name, ".") == 0 || std::strcmp(e->d_name, "..") == 0) continue;
                std::remove(xpp::format("{}{}{}", dir, SEP, static_cast<const char *>(e->d_name)).c_str());
            }
            closedir(d);
        }
    } catch (const std::bad_alloc &) {
        xpp_out_of_memory("removing the scratch folder");
    }
    remove_dir(dir);
}

void xpp_files_cleanup_stale_temp_dirs(void)
{
    try {
        std::string base = temp_base();
        if (base.empty()) return;
        DIR *d = opendir(base.c_str());
        if (d == nullptr) return;
        while (struct dirent *e = readdir(d)) {
            long long pid;
            if (!scratch_dir_pid(e->d_name, &pid)) continue;
            std::string path = xpp::format("{}{}{}", base, SEP, static_cast<const char *>(e->d_name));
            Stat st;
            if (stat_follow(path.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) continue;
            if (!process_gone(pid)) continue; /* still running, or cannot tell: leave it alone */
            xpp_files_remove_temp_dir(path.c_str());
        }
        closedir(d);
    } catch (const std::bad_alloc &) {
        xpp_out_of_memory("sweeping old scratch folders");
    }
}

/* ---- the folder listing ---------------------------------------------------------- */

bool xpp_files_list_dir(const char *dir, std::vector<XppDirEntry> &out)
{
    out.clear();
    DIR *d = opendir(dir);
    if (d == nullptr) return false;
    try {
        while (struct dirent *e = readdir(d)) {
            std::string path = xpp::format("{}/{}", dir, static_cast<const char *>(e->d_name));
            Stat st;
            bool folder = stat_follow(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
            out.push_back({e->d_name, folder});
        }
    } catch (const std::bad_alloc &) {
        xpp_out_of_memory("listing a folder");
    }
    closedir(d);
    return true;
}

/* ---- the Unix-style wildcard match ------------------------------------------------
   wildmatch.c - in the public domain (see the file history for its
   attribution). Syntax: * any run of characters, ? any one character,
   [r3z]/[a-d]/[!a-d] a character class, its negation with '!' or '^'. */

namespace {

constexpr char INVERT = '!'; /* the character that inverts a character class */

int star(const char *string, const char *pattern);

bool wild_match(const char *string, const char *pattern)
{
    int prev;    /* the previous character in a character class */
    int matched; /* the character class has been matched */
    int reverse; /* the character class is inverted */

    for (; *pattern; string++, pattern++)
        switch (*pattern) {
        case '\\':
            /* a literal match with the following character; fall through */
            pattern++;
            [[fallthrough]];
        default:
            if (*string != *pattern) return false;
            continue;
        case '?':
            if (*string == '\0') return false;
            continue;
        case '*':
            /* a trailing star matches everything */
            return *++pattern ? star(string, pattern) : true;
        case '[':
            reverse = pattern[1] == INVERT;
            if (reverse) pattern++;
            for (prev = 256, matched = 0; *++pattern && *pattern != ']'; prev = *pattern)
                if (*pattern == '-' ? *string <= *++pattern && *string >= prev : *string == *pattern) matched = 1;
            if (matched == reverse) return false;
            continue;
        }

    return *string == '\0';
}

int star(const char *string, const char *pattern)
{
    while (!wild_match(string, pattern))
        if (*++string == '\0') return 0;
    return 1;
}

} // namespace

bool xpp_files_list_matching(const char *wild, const char *direct,
                             std::vector<std::string> &dirs, std::vector<std::string> &files)
{
    dirs.clear();
    files.clear();
    std::vector<XppDirEntry> entries;
    if (!xpp_files_list_dir(direct, entries)) {
        xpp::log(XPP_LOG_WARN, " {} is not a directory \n", direct);
        return false;
    }
    for (XppDirEntry &e : entries) {
        if (e.folder) dirs.push_back(std::move(e.name));
        else if (wild_match(e.name.c_str(), wild)) files.push_back(std::move(e.name));
    }
    std::sort(dirs.begin(), dirs.end());
    std::sort(files.begin(), files.end());
    return true;
}

/* ---- the file selector's current folder --------------------------------------- */

namespace {
std::string cur_dir_str;
}

std::string xpp_files_working_dir()
{
    std::vector<char> buf(1024);
    for (;;) {
        if (getcwd(buf.data(), buf.size()) != nullptr) return buf.data();
        if (errno != ERANGE) return {};
        buf.resize(buf.size() * 2);
    }
}

std::pair<std::string, std::string> xpp_files_split_path(const std::string &path)
{
#ifdef _WIN32
    const size_t sep = path.find_last_of("/\\");
#else
    const size_t sep = path.find_last_of('/');
#endif
    if (sep == std::string::npos) return {std::string(), path};
    /* "/x" (and on Windows "C:\x"): the root itself */
    size_t keep = sep == 0 ? 1 : sep;
#ifdef _WIN32
    if (sep == 2 && path[1] == ':') keep = 3;
#endif
    return {path.substr(0, keep), path.substr(sep + 1)};
}

std::string xpp_files_absolute(const std::string &path, const std::string &dir)
{
#ifdef _WIN32
    const auto sep = [](char c) { return c == '/' || c == '\\'; };
    const bool absolute = (!path.empty() && sep(path[0])) ||
                          (path.size() > 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':' &&
                           sep(path[2]));
#else
    const auto sep = [](char c) { return c == '/'; };
    const bool absolute = !path.empty() && path[0] == '/';
#endif
    if (absolute || path.empty()) return path;
    std::string base = dir.empty() ? xpp_files_working_dir() : dir;
    if (!base.empty() && !sep(base.back())) base += '/';
    return base + path;
}

const char *xpp_files_cur_dir(void) { return cur_dir_str.c_str(); }

int xpp_files_refresh_cur_dir(void)
{
    std::string cwd = xpp_files_working_dir();
    if (cwd.empty()) {
        xpp::log(XPP_LOG_WARN, "Can't get current directory\n");
        cur_dir_str.clear();
        return 0;
    }
    cur_dir_str = std::move(cwd);
    return 1;
}

int xpp_files_change_dir(const char *path)
{
    if (path == nullptr) {
        cur_dir_str.clear();
        return 0;
    }
    if (chdir(path) == -1) {
        xpp::log(XPP_LOG_WARN, "Can't go to directory {}\n", path);
        return 1;
    }
    return xpp_files_refresh_cur_dir() != 0 ? 0 : 1;
}
