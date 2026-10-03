/* Folder operations: see xpp_files.h. Split out of xpp_files.cpp (W46b,
   folded in from the old core/read_dir.cpp) once the merged file grew
   past about 900 lines: is_dir, dir_writable, the
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
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h> /* getcwd, chdir (MinGW's unistd.h provides both) */

namespace xpp::files {

std::string temp_base()
{
#ifdef _WIN32
    return xpp::win32::temp_folder();
#else
    const char *base = std::getenv("TMPDIR");
    return base && base[0] ? base : "/tmp";
#endif
}

bool is_dir(std::string_view path)
{
    Stat st;
    return stat_follow(std::string(path).c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool dir_writable(std::string_view dir)
{
    if (dir.empty()) return false;
    try {
        std::string probe = xpp::format("{}/.xppautx_homecheck", dir);
        xpp::UniqueFile fp(std::fopen(probe.c_str(), "w"));
        if (!fp) return false;
        fp.reset();
        std::remove(probe.c_str());
        return true;
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("probing a folder");
    }
}

/* ---- AUTO's scratch folders ------------------------------------------------------ */

namespace {

constexpr int SCRATCH_ATTEMPTS = 1000; /* a crashed process with our reused pid may have left names */

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

std::string make_temp_dir()
{
    try {
        std::string base = temp_base();
        if (base.empty()) return {};
        for (int i = 0; i < SCRATCH_ATTEMPTS; i++) {
            std::string path = xpp::format("{}{}xppautoX-{}-{}", base, SEP, own_pid(), i);
            if (make_dir(path.c_str()) == 0) {
                /* getcwd resolves linked ancestors such as macOS's /var;
                   keep the same real name from the moment it is made. */
                std::error_code error;
                const std::string real = std::filesystem::canonical(path, error).generic_string();
                if (!error) return real;
                remove_dir(path.c_str());
                return {};
            }
            if (errno != EEXIST) break;
        }
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("making the scratch folder");
    }
    return {};
}

bool is_scratch(std::string_view path, bool root_only)
{
    try {
        std::filesystem::path folder(path);
        /* A relative read names a recording member, even while replay's
           cwd is scratch. Only explicit scratch paths bypass that server. */
        if (!folder.is_absolute()) return false;
        for (const auto &part : folder)
            if (part == "..") return false;
        std::error_code error;
        const auto base = std::filesystem::canonical(temp_base(), error);
        if (error) return false;
        while (!folder.empty()) {
            /* Check before resolving: a link inside scratch must never
               gain permission merely because its destination is scratch. */
            if (is_link(folder.string().c_str())) return false;
            long long pid;
            if (scratch_dir_pid(folder.filename().string(), &pid) && pid == own_pid()) {
                const auto parent = std::filesystem::canonical(folder.parent_path(), error);
                return !error && parent == base && is_dir(folder.string());
            }
            if (root_only || folder == folder.parent_path()) return false;
            folder = folder.parent_path();
        }
        return false;
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("naming the scratch folder");
    }
}

void remove_temp_dir(std::string_view dir_view)
{
    if (dir_view.empty()) return;
    std::string dir;
    try {
        dir = dir_view;
        if (DIR *d = opendir(dir.c_str())) {
            while (struct dirent *e = readdir(d)) {
                if (std::strcmp(e->d_name, ".") == 0 || std::strcmp(e->d_name, "..") == 0) continue;
                std::remove(xpp::format("{}{}{}", dir, SEP, static_cast<const char *>(e->d_name)).c_str());
            }
            closedir(d);
        }
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("removing the scratch folder");
    }
    remove_dir(dir.c_str());
}

void cleanup_stale_temp_dirs()
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
            remove_temp_dir(path);
        }
        closedir(d);
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("sweeping old scratch folders");
    }
}

/* ---- the folder listing ---------------------------------------------------------- */

bool list_dir(std::string_view dir, std::vector<DirEntry> &out)
{
    out.clear();
    DIR *d;
    try {
        d = opendir(std::string(dir).c_str());
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("listing a folder");
    }
    if (d == nullptr) return false;
    try {
        while (struct dirent *e = readdir(d)) {
            std::string path = xpp::format("{}/{}", dir, static_cast<const char *>(e->d_name));
            Stat st;
            bool folder = stat_follow(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
            out.push_back({e->d_name, folder});
        }
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("listing a folder");
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

bool list_matching(std::string_view wild_view, std::string_view direct, std::vector<std::string> &dirs,
                   std::vector<std::string> &files)
{
    dirs.clear();
    files.clear();
    std::vector<DirEntry> entries;
    if (!list_dir(direct, entries)) {
        xpp::log(XPP_LOG_WARN, " {} is not a directory \n", direct);
        return false;
    }
    const std::string wild(wild_view);
    for (DirEntry &e : entries) {
        if (e.folder) dirs.push_back(std::move(e.name));
        else if (wild_match(e.name.c_str(), wild.c_str())) files.push_back(std::move(e.name));
    }
    std::sort(dirs.begin(), dirs.end());
    std::sort(files.begin(), files.end());
    return true;
}

/* ---- the file selector's current folder --------------------------------------- */

namespace {
std::string cur_dir_str;
}

std::string working_dir()
{
    std::vector<char> buf(1024);
    for (;;) {
        if (getcwd(buf.data(), buf.size()) != nullptr) return buf.data();
        if (errno != ERANGE) return {};
        buf.resize(buf.size() * 2);
    }
}

std::pair<std::string, std::string> split_path(std::string_view path)
{
#ifdef _WIN32
    const size_t sep = path.find_last_of("/\\");
#else
    const size_t sep = path.find_last_of('/');
#endif
    if (sep == std::string_view::npos) return {std::string(), std::string(path)};
    /* "/x" (and on Windows "C:\x"): the root itself */
    size_t keep = sep == 0 ? 1 : sep;
#ifdef _WIN32
    if (sep == 2 && path[1] == ':') keep = 3;
#endif
    return {std::string(path.substr(0, keep)), std::string(path.substr(sep + 1))};
}

std::string absolute(std::string_view path, std::string_view dir)
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
    if (absolute || path.empty()) return std::string(path);
    std::string base = dir.empty() ? working_dir() : std::string(dir);
    if (!base.empty() && !sep(base.back())) base += '/';
    return base.append(path);
}

std::string folder_in(std::string_view dir, std::string_view name)
{
    std::string_view base = dir;
    if (name == "..") {
        while (base.size() > 1 && (base.back() == '/' || base.back() == '\\'))base.remove_suffix(1);
        const std::string parent = split_path(base).first;
        return parent.empty() ? std::string(base) : parent;
    }
    std::string into = absolute(name, dir);
    return is_dir(into) ? into : std::string();
}

std::string cur_dir() { return cur_dir_str; }

bool refresh_cur_dir()
{
    std::string cwd = working_dir();
    if (cwd.empty()) {
        xpp::log(XPP_LOG_WARN, "Can't get current directory\n");
        cur_dir_str.clear();
        return false;
    }
    cur_dir_str = std::move(cwd);
    return true;
}

int change_dir(std::string_view path)
{
    if (chdir(std::string(path).c_str()) == -1) {
        xpp::log(XPP_LOG_WARN, "Can't go to directory {}\n", path);
        return 1;
    }
    return refresh_cur_dir() ? 0 : 1;
}

} // namespace xpp::files
