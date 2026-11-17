/* Folder operations: see xpp_files.h. Split out of xpp_files.cpp (W46b,
   folded in from the old core/read_dir.cpp) once the merged file grew
   past about 900 lines: is_dir, dir_writable, the
   scratch folders autoX runs in, the folder listing, the Unix-style
   wildcard match and the file selector's current folder. */
#include "xpp_files.h"
#include <cstdint>
#include "xpp_files_internal.h" /* IWYU pragma: keep (xpp_files.h is the public face; the implementation needs its internals) */
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
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h> /* getcwd, chdir (MinGW's unistd.h provides both) */
#include <sys/types.h>
#include <new>
#include <system_error>
#include <utility>

namespace xpp::files {

std::string config_base()
{
    /* an override is a whole path: a relative one would follow the working folder, which opening a model changes */
    if (const char *own = std::getenv(CONFIG_DIR_ENV); own && own[0]) return is_absolute(own) ? own : std::string();
    return system_config_base();
}

std::string config_dir()
{
    constexpr std::string_view APP = "xppautX"; /* the folder's name under the system's config folder */
    const std::string base = config_base();
    if (base.empty()) return {};
    if (const char *own = std::getenv(CONFIG_DIR_ENV); own && own[0]) return base;
    return join(base, APP);
}

std::string program_dir()
{
    const std::string exe = program_file();
    if (exe.empty()) return {};
    return shipped_files_folder(split_path(exe).first);
}

std::string config_path(std::string_view name)
{
    const std::string dir = config_dir();
    return dir.empty() ? std::string() : join(dir, name);
}

namespace {
bool walk_dirs(std::string_view path, std::string_view anchor, bool create)
{
    const std::string base = absolute(anchor.empty() ? "." : anchor);
    for (const PathPart &component : path_parts(absolute(path), base)) {
        const std::string &part = component.path;
        /* a bare drive ("C:") is no folder to make or test (W220). */
        if (part.size() == 2 && part[1] == ':' && std::isalpha(static_cast<unsigned char>(part[0]))) continue;
        if (component.below_anchor && link_on_way(part.c_str())) { errno = ELOOP; return false; }
        Stat st;
        if (stat_follow(part.c_str(), &st) == 0) {
            if (!S_ISDIR(st.st_mode)) { errno = ENOTDIR; return false; }
        } else {
            if (!create || errno != ENOENT) return false;
            if (make_dir(part.c_str()) != 0 && errno != EEXIST) return false;
            /* Even a newly made ancestor must not have been swapped for a link. */
            if (is_link(part.c_str())) { errno = ELOOP; return false; }
            if (!is_dir(part)) { errno = ENOTDIR; return false; }
        }
    }
    return true;
}
} // namespace

bool make_dirs(std::string_view path, std::string_view anchor) { return walk_dirs(path, anchor, true); }
bool check_dirs(std::string_view path, std::string_view anchor) { return walk_dirs(path, anchor, false); }

bool is_dir(std::string_view path)
{
    Stat st;
    return stat_follow(std::string(path).c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

namespace {

constexpr std::string_view SCRATCH_PREFIX = "xppautoX-";        /* autoX's scratch folders: <prefix><pid>-<N> */
constexpr std::string_view PROBE_PREFIX = ".xppautx-homecheck-"; /* dir_writable's probe file: <prefix><pid>-<N> */

/* name is exactly "<prefix><pid>-<N>" (digits; the pid may be negative
   as %ld reads it): *pid */
bool named_pid(std::string_view name, std::string_view prefix, long long *pid)
{
    if (!name.starts_with(prefix)) return false;
    const char *p = name.data() + prefix.size(), *end = name.data() + name.size();
    std::from_chars_result r = std::from_chars(p, end, *pid);
    if (r.ec != std::errc() || r.ptr == end || *r.ptr != '-') return false;
    int idx;
    r = std::from_chars(r.ptr + 1, end, idx);
    return r.ec == std::errc() && r.ptr == end;
}

/* the entries of `parent` named "<prefix><pid>-<N>" whose pid is no longer running are removed: a killed run's scratch
   folders (folder true) or a killed probe's plain file. A link is never followed; nothing else is touched. */
void sweep_stale(const std::string &parent, std::string_view prefix, bool folder)
{
    DIR *d = opendir(parent.c_str());
    if (d == nullptr) return;
    while (struct dirent *e = readdir(d)) {
        long long pid;
        if (!named_pid(e->d_name, prefix, &pid)) continue;
        if (!pid_gone(pid)) continue; /* still running, or cannot tell: leave it alone */
        std::string path = parent;
        path += SEP;
        path += e->d_name;
        Stat st;
        /* a link is never followed (lstat; is_link for Windows' reparse points) */
        if (is_link(path.c_str()) || stat_name(path.c_str(), &st) != 0) continue;
        if (folder ? !S_ISDIR(st.st_mode) : !S_ISREG(st.st_mode)) continue;
        if (owned_by_another(st)) continue; /* a folder another user made under our name is not ours to empty */
        if (folder) remove_temp_dir(path);
        else std::remove(path.c_str());
    }
    closedir(d);
}

} // namespace

bool dir_writable(std::string_view dir)
{
    if (dir.empty() || !check_dirs(dir)) return false;
    try {
        /* a probe a killed process left between its create and its remove (any pid no longer running) goes first */
        sweep_stale(std::string(dir), PROBE_PREFIX, false);
        constexpr int probe_attempts = 100; /* Skip exclusive names left by an interrupted probe. */
        for (int attempt = 0; attempt < probe_attempts; ++attempt) {
            const std::string probe = xpp::format("{}/{}{}-{}", dir, PROBE_PREFIX, own_pid(), attempt);
            xpp::UniqueFile fp(create_plain(probe, true));
            if (!fp) {
                if (errno == EEXIST || errno == ELOOP) continue;
                return false;
            }
            fp.reset();
            std::remove(probe.c_str());
            return true;
        }
        return false;
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("probing a folder");
    }
}

std::string writable_save_folder(std::string_view model_folder)
{
    if (dir_writable(model_folder)) return std::string(model_folder);
    const std::string documents = user_documents_folder();
    return dir_writable(documents) ? documents : std::string();
}

/* ---- autoX's scratch folders ------------------------------------------------------ */

namespace {

constexpr int SCRATCH_ATTEMPTS = 1000; /* a crashed process with our reused pid may have left names */

/* the model folder's scratch parent, `.xppautx/tmp`, when it is there (nothing is made); links below the folder are never followed */
std::string existing_model_tmp(std::string_view model_folder)
{
    const std::string tmp = join(join(model_folder, MODEL_STATE_FOLDER), MODEL_TMP_FOLDER);
    return check_dirs(tmp, model_folder) ? tmp : std::string();
}

} // namespace

Result<std::string> model_state_dir(std::string_view model_folder)
{
    const std::string dir = join(model_folder, MODEL_STATE_FOLDER);
    if (check_dirs(dir, model_folder)) return dir;
    if (errno != ENOENT || !make_dirs(dir, model_folder)) return std::unexpected(write_error("folder", dir));
    /* the scratch and the log are never committed: written when the folder is made */
    /* the program's own folder, so not through the replay's write guard (a recording's replay makes its scratch in the recording's folder) */
    const std::string ignore = join(dir, ".gitignore");
    xpp::UniqueFile file(create_plain(ignore, true));
    if (!file || std::fwrite(MODEL_STATE_GITIGNORE.data(), 1, MODEL_STATE_GITIGNORE.size(), file.get()) != MODEL_STATE_GITIGNORE.size() ||
        std::fflush(file.get()) != 0)
        return std::unexpected(write_error("folder", ignore));
    return dir;
}

Result<std::string> make_temp_dir(std::string_view model_folder)
{
    try {
        std::string base;
        if (model_folder.empty() || is_scratch(model_folder, true)) {  /* a scratch folder (a replay's) holds no .xppautx of its own */
            base = temp_base();
        } else {
            const Result<std::string> state = model_state_dir(model_folder);
            if (!state) return std::unexpected(state.error());
            base = join(*state, MODEL_TMP_FOLDER);
            if (!check_dirs(base, model_folder) && (errno != ENOENT || !make_dirs(base, model_folder)))
                return std::unexpected(write_error("folder", base));
            sweep_stale(base, SCRATCH_PREFIX, true);
        }
        if (base.empty()) return fail("scratch folder", "the system names no temp folder", Place{"the system's temp folder"});
        for (int i = 0; i < SCRATCH_ATTEMPTS; i++) {
            std::string path = xpp::format("{}{}{}{}-{}", base, SEP, SCRATCH_PREFIX, own_pid(), i);
            if (make_dir(path.c_str()) == 0) {
                /* getcwd resolves linked ancestors such as macOS's /var;
                   keep the same real name from the moment it is made. */
                std::error_code error;
                const std::string real = std::filesystem::canonical(path, error).generic_string();
                if (!error) return real;
                remove_dir(path.c_str());
                return fail("scratch folder", xpp::format("cannot resolve {}: {}", path, error.message()), Place{path});
            }
            if (errno != EEXIST) break;
        }
        return std::unexpected(write_error("scratch folder", join(base, "xppautoX")));
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("making the scratch folder");
    }
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
        /* Check every ancestor before canonicalizing, including the temp
           base: a user-planted alias must never gain scratch permission. */
        for (const PathPart &part : path_parts(folder.string(), folder.root_path().string()))
            if (link_on_way(part.path.c_str())) return false;
        std::error_code error;
        const auto base = std::filesystem::canonical(temp_base(), error);
        if (error) return false;
        while (!folder.empty()) {
            long long pid;
            if (named_pid(folder.filename().string(), SCRATCH_PREFIX, &pid) && pid == own_pid()) {
                const auto parent = std::filesystem::canonical(folder.parent_path(), error);
                /* in the system's temp folder, or in a model folder's .xppautx/tmp (make_temp_dir) */
                const bool in_model_tmp = parent.filename() == MODEL_TMP_FOLDER && parent.parent_path().filename() == MODEL_STATE_FOLDER;
                return !error && (parent == base || in_model_tmp) && is_dir(folder.string());
            }
            if (root_only || folder == folder.parent_path()) return false;
            folder = folder.parent_path();
        }
        return false;
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("naming the scratch folder");
    }
}

namespace {

/* a file or a link, removed by its own name (never its target); on Windows a
   folder link needs RemoveDirectory, which remove() does not do */
void remove_entry(const char *path)
{
    if (std::remove(path) != 0) remove_dir(path);
}

} // namespace

/* the pending browser save (xpp_files.h offer_download): its folder and file name, under one lock (the HTTP thread asks, the core sets) */
namespace {
std::mutex download_lock;
std::string download_folder, download_name;
}  // namespace

void remove_temp_dir(std::string_view dir_view)
{
    if (dir_view.empty()) return;
    try {
        const std::string root(dir_view);
        Stat st;
        if (is_link(root.c_str())) { /* a link named as our folder: the link goes, its target stays */
            remove_entry(root.c_str());
            return;
        }
        if (stat_name(root.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) return;
        /* Folders found so far. A folder's subfolders come after it, so removing
           from the back empties every folder before it is removed. No recursion:
           a deep folder cannot overflow the stack. */
        std::vector<std::string> folders{root};
        for (std::size_t i = 0; i < folders.size(); i++) {
            DIR *d = opendir(folders[i].c_str());
            if (d == nullptr) continue;
            while (struct dirent *e = readdir(d)) {
                if (std::strcmp(e->d_name, ".") == 0 || std::strcmp(e->d_name, "..") == 0) continue;
                std::string entry = folders[i] + SEP + e->d_name;
                if (!is_link(entry.c_str()) && stat_name(entry.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
                    folders.push_back(std::move(entry));
                else
                    remove_entry(entry.c_str());
            }
            closedir(d);
        }
        for (auto it = folders.rbegin(); it != folders.rend(); ++it) remove_dir(it->c_str());
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("removing the scratch folder");
    }
}

void cleanup_stale_temp_dirs(std::string_view model_folder)
{
    try {
        if (const std::string base = temp_base(); !base.empty()) sweep_stale(base, SCRATCH_PREFIX, true);
        if (!model_folder.empty())
            if (const std::string tmp = existing_model_tmp(model_folder); !tmp.empty()) sweep_stale(tmp, SCRATCH_PREFIX, true);
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

/* Keep only the latest star's retry position: hostile patterns never recurse. */
bool wild_match(const char *string, const char *pattern)
{
    const char *after_star = nullptr;
    const char *retry = nullptr;
    for (;;) {
        if (*pattern == '*') {
            do { pattern++; } while (*pattern == '*');
            if (!*pattern) return true;
            after_star = pattern;
            retry = string;
            continue;
        }
        if (!*string) return !*pattern;
        const char *next = pattern;
        bool matched = false;
        if (*next == '?') {
            next++;
            matched = true;
        } else if (*next == '[') {
            const bool reverse = next[1] == INVERT;
            if (reverse) next++;
            int prev = 256;
            bool in_class = false;
            while (*++next && *next != ']') {
                if (*next == '-' && next[1]) {
                    next++;
                    if (*string <= *next && *string >= prev) in_class = true;
                } else if (*string == *next) in_class = true;
                prev = *next;
            }
            if (*next != ']') return false; /* An unfinished class cannot match. */
            next++;
            matched = in_class != reverse;
        } else if (*next) {
            if (*next == '\\') next++;
            if (!*next) return false; /* A trailing escape has no literal. */
            matched = *string == *next++;
        }
        if (matched) {
            string++;
            pattern = next;
        } else if (after_star && *retry) {
            string = ++retry;
            pattern = after_star;
        } else return false;
    }
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

/* ---- the working folder ------------------------------------------------------- */

std::string working_dir()
{
    std::vector<char> buf(1024);
    for (;;) {
        if (getcwd(buf.data(), buf.size()) != nullptr) return buf.data();
        if (errno != ERANGE) return {};
        buf.resize(buf.size() * 2);
    }
}

std::string absolute(std::string_view path, std::string_view dir)
{
    if (is_absolute(path) || path.empty()) return std::string(path);
    return append_path(dir.empty() ? working_dir() : std::string(dir), path);
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

int change_dir(std::string_view path)
{
    if (chdir(std::string(path).c_str()) == -1) {
        xpp::log(XPP_LOG_WARN, "Can't go to directory {}\n", path);
        return 1;
    }
    if (working_dir().empty()) {
        xpp::log(XPP_LOG_WARN, "Can't get current directory\n");
        return 1;
    }
    return 0;
}

} // namespace xpp::files

namespace xpp::files {

void offer_download(std::string_view folder, std::string_view name)
{
    std::lock_guard<std::mutex> hold(download_lock);
    if (!download_folder.empty() && download_folder != folder) remove_temp_dir(download_folder);
    download_folder = folder;
    download_name = name;
}

bool is_download(std::string_view path)
{
    std::lock_guard<std::mutex> hold(download_lock);
    const auto [folder, name] = split_path(path);
    return !download_folder.empty() && folder == download_folder && name == download_name;
}

std::string download_path(std::string_view name)
{
    std::lock_guard<std::mutex> hold(download_lock);
    if (download_folder.empty() || name != download_name) return {};
    return join(download_folder, download_name);
}

void download_done()
{
    std::lock_guard<std::mutex> hold(download_lock);
    remove_temp_dir(download_folder);
    download_folder.clear();
    download_name.clear();
}

} // namespace xpp::files
