/* The model's folder as the page's workspace: see xpp_files.h. Folder
   operations (listing, the wildcard match, the current folder, AUTO's
   scratch folders) are core/xpp_files_dir.cpp, behind the same header;
   this file keeps the page's file API and the core's own single-file
   operations. */
#include "xpp_files.h"
#include "xpp_files_internal.h"
#include "xpp_io.h"
#include "xpp_job.h"
#include "xpp_log.h"
#include "xpp_mem.h"
#include "xpp_sha256.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <map>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifndef O_TEXT
#define O_TEXT 0
#endif

struct xpp::files::Put {
    xpp::Writer w; /* binary: the temp file beside name, renamed over it at commit */
    std::string name;
    unsigned long long cap = 0, bytes = 0;
    xpp::Sha256 sha;
};

namespace {

/* ---- names ------------------------------------------------------------------ */

bool device_name(std::string_view name)
{
    /* CON, NUL, COM1.txt ...: Windows opens the device whatever follows the dot */
    static const char *const devices[] = {"con", "prn", "aux", "nul", "conin$", "conout$"};
    std::string stem;
    for (char c : name.substr(0, name.find('.'))) stem += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    while (!stem.empty() && stem.back() == ' ') stem.pop_back();
    for (const char *d : devices)
        if (stem == d) return true;
    return stem.size() == 4 && (stem.compare(0, 3, "com") == 0 || stem.compare(0, 3, "lpt") == 0)
           && stem[3] >= '0' && stem[3] <= '9';
}

/* ---- the file system ------------------------------------------------------------ */

/* is_link, stat_follow, make_dir, remove_dir, own_pid, temp_base,
   process_gone, SEP and the Stat type are xpp_files_internal.h, shared
   with xpp_files_dir.cpp. stat_name (lstat, not stat_follow: a link is
   never followed here) and rename_over are this file's own. */
using namespace xpp::files;

#ifdef _WIN32
int stat_name(const char *name, Stat *st) { return _stat64(name, st); }
int rename_over(const char *from, const char *to) { return xpp::win32::move_over(from, to) ? 0 : -1; }
#else
int stat_name(const char *name, Stat *st) { return lstat(name, st); }
int rename_over(const char *from, const char *to) { return std::rename(from, to); }
#endif

/* what a name is on disk: NOT_FOUND, REFUSED (not a plain file) or OK */
int kind_of(const char *name, Stat *st)
{
    if (is_link(name)) return XPP_FILES_REFUSED;
    if (stat_name(name, st) != 0) return errno == ENOENT ? XPP_FILES_NOT_FOUND : XPP_FILES_IO;
    return S_ISREG(st->st_mode) ? XPP_FILES_OK : XPP_FILES_REFUSED;
}

int open_plain(const char *name, xpp::UniqueFile &fp, unsigned long long &size)
{
    Stat st;
    int k = kind_of(name, &st); /* a folder, a link or a device named at once */
    if (k != XPP_FILES_OK) return k;
#ifdef _WIN32
    /* and refused again on the handle opened, so nothing swapped in between is followed (W174) */
    int fd = xpp::win32::open_plain(name, false, true, size);
#else
    /* O_NOFOLLOW: a link made between the check and the open is not followed */
    int fd = ::open(name, O_RDONLY | O_BINARY | O_NOFOLLOW | O_NONBLOCK);
#endif
    if (fd < 0) return errno == ENOENT ? XPP_FILES_NOT_FOUND : errno == ELOOP ? XPP_FILES_REFUSED : XPP_FILES_IO;
#ifndef _WIN32
    struct stat fst;
    if (fstat(fd, &fst) != 0 || !S_ISREG(fst.st_mode)) {
        close(fd);
        return XPP_FILES_REFUSED;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
    st.st_size = fst.st_size;
#endif
    fp.reset(fdopen(fd, "rb"));
    if (!fp) {
        close(fd);
        return XPP_FILES_IO;
    }
#ifndef _WIN32
    size = static_cast<unsigned long long>(st.st_size);
#endif
    return XPP_FILES_OK;
}

/* ---- the listing's digests, kept while a file does not change ----------------- */

struct Digest {
    unsigned long long size;
    long long mtime;
    std::string sha;
};
std::mutex cache_lock;
std::map<std::string, Digest> cache;

/* the file's SHA-256 as 64 hex digits */
bool file_sha(const char *name, std::string &out)
{
    xpp::UniqueFile fp;
    unsigned long long size;
    if (open_plain(name, fp, size) != XPP_FILES_OK) return false;
    xpp::Sha256 c;
    std::vector<unsigned char> buf(1 << 16);
    size_t n;
    while ((n = std::fread(buf.data(), 1, buf.size(), fp.get())) > 0) c.update(buf.data(), n);
    bool ok = !std::ferror(fp.get());
    fp.reset();
    out = c.hex();
    return ok;
}

/* a digest is kept only for a file untouched for 2 seconds: a rewrite of
   the same size within the second of its mtime would otherwise look
   unchanged */
void remember(const std::string &name, unsigned long long size, long long mtime, const std::string &sha)
{
    if (static_cast<long long>(std::time(nullptr)) - mtime < 2) return;
    std::lock_guard<std::mutex> g(cache_lock);
    cache[name] = Digest{size, mtime, sha};
}

bool cached(const std::string &name, unsigned long long size, long long mtime, std::string &out)
{
    std::lock_guard<std::mutex> g(cache_lock);
    auto it = cache.find(name);
    if (it == cache.end() || it->second.size != size || it->second.mtime != mtime) return false;
    out = it->second.sha;
    return true;
}

/* ---- JSON ---------------------------------------------------------------
   The reader/writer for a JSON string's content are xpp_io.h's
   (xpp::json_encode_string/json_decode_string): json_io.cpp's protocol
   events and command objects share them, so a fix (UTF-8, surrogate
   pairs) lands in one place. This module keeps its own stricter rule for
   a file name below (xpp::files::name_ok), and calls the shared decoder in
   "strict" mode: false for a control character or an unpaired surrogate,
   which are not a name we take. */

/* a JSON string value, strictly: false for anything but a string, and for
   one that holds a control character (\u0000 included) or an unpaired
   surrogate */
bool json_string(const char *v, std::string &out)
{
    return xpp::json_decode_string(v, out, static_cast<size_t>(-1), /*strict=*/true);
}

/* put_base64's answer when the data is not a base64 string */
const int NOT_BASE64 = -1;

/* the JSON string value v (base64, padded or not) into a put */
int put_base64(xpp::files::Put &put, const char *v)
{
    if (!v || *v != '"') return NOT_BASE64;
    std::string out;
    xpp::Base64Decoder d(out);
    for (v++; *v != '"'; v++) {
        if (!*v || !d.feed(*v)) return NOT_BASE64;
        if (out.size() >= 3 * 1024) {
            int st = xpp::files::put_write(put, out);
            if (st != XPP_FILES_OK) return st;
            out.clear();
        }
    }
    if (!d.finish()) return NOT_BASE64;
    return out.empty() ? XPP_FILES_OK : xpp::files::put_write(put, out);
}

/* ---- the listing ------------------------------------------------------------------ */

void command_error(xpp::files::CommandResult &result, std::string_view why)
{
    result.error = xpp::Error{"file", std::string(why), xpp::Place{result.name}};
}

xpp::files::CommandResult listing()
{
    xpp::files::CommandResult result;
    std::vector<xpp::files::DirEntry> entries;
    if (!xpp::files::list_dir(".", entries)) {
        result.error = xpp::files::open_error("files", ".");
        return result;
    }
    for (const auto &e : entries) {
        if (!xpp::files::name_ok(e.name)) continue;
        Stat st;
        if (kind_of(e.name.c_str(), &st) != XPP_FILES_OK) continue;
        xpp::files::FileEntry f{e.name, static_cast<unsigned long long>(st.st_size), static_cast<long long>(st.st_mtime), ""};
        if (!cached(f.name, f.size, f.mtime, f.sha)) {
            if (!file_sha(f.name.c_str(), f.sha)) continue;
            remember(f.name, f.size, f.mtime, f.sha);
        }
        result.files.push_back(std::move(f));
    }
    std::sort(result.files.begin(), result.files.end(), [](const auto &a, const auto &b) { return a.name < b.name; });
    return result;
}

xpp::files::CommandResult run_command(std::string_view op, const char *name_json, const char *data_json)
{
    if (op == "list") return listing();
    xpp::files::CommandResult result;
    std::string name;
    const bool decoded = json_string(name_json, name);
    if (decoded) result.name = name; /* Echo the requested name even when refused. */
    const bool named = decoded && xpp::files::name_ok(name);
    if (op != "get" && op != "put") {
        command_error(result, "unknown op");
        return result;
    }
    if (!named) {
        command_error(result, xpp::files::status_text(XPP_FILES_BAD_NAME));
        return result;
    }
    int st;
    if (op == "put") {
        xpp::files::Put *put;
        st = xpp::files::put_begin(name, XPP_FILES_CAP, put);
        if (st == XPP_FILES_OK) {
            st = put_base64(*put, data_json);
            if (st == XPP_FILES_OK) st = xpp::files::put_commit(put, result.size, result.sha);
            else xpp::files::put_abort(put);
        }
        if (st != XPP_FILES_OK)
            command_error(result, st == NOT_BASE64 ? "data is not a base64 string" : xpp::files::status_text(st));
        return result;
    }
    xpp::UniqueFile fp;
    st = open_plain(name.c_str(), fp, result.size);
    if (st == XPP_FILES_OK && result.size > XPP_FILES_CAP) st = XPP_FILES_TOO_LARGE;
    if (st != XPP_FILES_OK) {
        command_error(result, xpp::files::status_text(st));
        return result;
    }
    result.bytes.resize(static_cast<size_t>(result.size));
    const size_t got = std::fread(result.bytes.data(), 1, result.bytes.size(), fp.get());
    if (got != result.size) {
        command_error(result, xpp::files::status_text(XPP_FILES_IO));
        result.bytes.clear();
        return result;
    }
    xpp::Sha256 c;
    c.update(result.bytes.data(), got);
    result.sha = c.hex();
    return result;
}

/* an upload refused because a computation runs (xpp_files.h put_begin):
   the same WARN the protocol's classifier logs for a refused command */
int refused_busy(std::string_view name)
{
    xpp::log(XPP_LOG_WARN, "refused during a computation: an upload of {}\n", name);
    return XPP_FILES_BUSY;
}

} // namespace

/* ---- the API --------------------------------------------------------------------- */

namespace xpp::files {

bool name_ok(std::string_view name, bool allow_hidden)
{
    if (name.empty() || (!allow_hidden && name[0] == '.') || name[0] == ' ') return false;
    const size_t n = name.size();
    if (n > NAME_MAX_BYTES || name.find("..") != std::string_view::npos) return false;
    constexpr std::string_view reserved = "/\\:<>\"|?*";
    for (char ch : name) {
        unsigned char c = static_cast<unsigned char>(ch);
        if (c < 0x20 || c == 0x7f || reserved.find(ch) != std::string_view::npos) return false;
    }
    /* Windows drops a trailing dot or space, so "a.set." would be a.set */
    if (name[n - 1] == '.' || name[n - 1] == ' ') return false;
    return !device_name(name);
}

int replace_file(std::string_view from, std::string_view to)
{
    if (!write_path_ok(to)) return -1;
    return rename_over(std::string(from).c_str(), std::string(to).c_str());
}

const char *status_text(int status)
{
    switch (status) {
    case XPP_FILES_OK: return "ok";
    case XPP_FILES_BAD_NAME: return "not a plain file name in the model's folder";
    case XPP_FILES_NOT_FOUND: return "no such file";
    case XPP_FILES_REFUSED: return "not a plain file (a link, a folder or a device)";
    case XPP_FILES_TOO_LARGE: return "larger than 64 MB";
    case XPP_FILES_BUSY: return xpp::job::REFUSED_WHILE_COMPUTING;
    default: return "the file could not be read or written";
    }
}



int open(std::string_view name, FILE *&fp, unsigned long long &size)
{
    if (!name_ok(name)) return XPP_FILES_BAD_NAME;
    xpp::UniqueFile f;
    int st = open_plain(std::string(name).c_str(), f, size);
    fp = f.release();
    return st;
}

int output_status(std::string_view path)
{
    if(!name_ok(split_path(path).second))return XPP_FILES_BAD_NAME;
    Stat st;
    const int status=kind_of(std::string(path).c_str(),&st);
    return status==XPP_FILES_NOT_FOUND ? XPP_FILES_OK : status;
}

int put_begin(std::string_view name, unsigned long long cap, Put *&put)
{
    put = nullptr;
    if (!name_ok(name)) return XPP_FILES_BAD_NAME;
    if (xpp::job::computing()) return refused_busy(name); /* before a byte of it is read */
    try {
        std::unique_ptr<Put> p = std::make_unique<Put>();
        p->name = name;
        const int k = output_status(p->name);
        if (k != XPP_FILES_OK) return k;
        p->cap = cap;
        p->w = xpp::Writer::binary(p->name); /* hidden (a leading dot): neither listed nor reachable by name */
        if (!p->w) return XPP_FILES_IO;
        put = p.release();
        return XPP_FILES_OK;
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("starting an upload");
    }
}

int put_write(Put &put, std::string_view bytes)
{
    const size_t n = bytes.size();
    if (n > put.cap - put.bytes) return XPP_FILES_TOO_LARGE;
    if (n && !put.w.write(bytes)) return XPP_FILES_IO;
    put.bytes += n;
    put.sha.update(bytes.data(), n);
    return XPP_FILES_OK;
}

void put_abort(Put *put) { delete put; /* its writer discards the temp file */ }

int put_commit(Put *put, unsigned long long &size, std::string &sha256)
{
    std::unique_ptr<Put> p(put);
    Stat st;
    {
        /* decided with the rename, atomically: no computation runs now,
           and none begins until the file is in place */
        xpp::job::OutsideComputation outside;
        if (!outside) return refused_busy(p->name);
        int k = kind_of(p->name.c_str(), &st);
        if (k != XPP_FILES_OK && k != XPP_FILES_NOT_FOUND) return k; /* became a link or a folder meanwhile */
        if (!p->w.commit()) return XPP_FILES_IO;
    }
    size = p->bytes;
    try {
        sha256 = p->sha.hex();
        if (stat_name(p->name.c_str(), &st) == 0 && static_cast<unsigned long long>(st.st_size) == p->bytes)
            remember(p->name, p->bytes, static_cast<long long>(st.st_mtime), sha256);
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("finishing an upload");
    }
    return XPP_FILES_OK;
}

std::string output_name(std::string_view model_file, std::string_view ext, std::string_view what)
{
    std::string base = split_path(model_file).second;
    const std::size_t dot = base.rfind('.');
    if (dot != std::string::npos && dot > 0) base.resize(dot);
    if (!what.empty()) { base += '-'; base += what; }
    return base + std::string(ext);
}

std::string frame_name(std::string_view first, std::string_view ext, int frame)
{
    if(frame==0)return std::string(first);
    const auto [folder,base]=split_path(first);
    const std::string name=output_name(base,ext,xpp::format("frame{}",frame));
    return folder.empty()?name:absolute(name,folder);
}

const char *ask_mode(std::string_view title)
{
    /* the file selectors' titles (file_selector() callers): "Load Auto",
       "Read initial data", "Import XPPAUT set", "Import Diagram", "Select an ODE file",
       "Library:", "Play recording" open a file; "Save ...", "Write ...", "Postscript",
       "GIF plot", "Clone ODE file", ... write one */
    static const char *const reads[] = {"load", "read", "import", "open", "select", "library", "play"};
    std::string word; /* at most 15 letters: kept in the string itself, no allocation */
    const size_t from = title.find_first_not_of(' ');
    title.remove_prefix(from == std::string_view::npos ? title.size() : from);
    for (size_t i = 0; i < title.size() && word.size() < 15 && std::isalpha(static_cast<unsigned char>(title[i])); i++)
        word += static_cast<char>(std::tolower(static_cast<unsigned char>(title[i])));
    for (const char *r : reads)
        if (word == r) return "read";
    return "write";
}

CommandResult command(std::string_view op, const char *name_json, const char *data_json)
{
    return run_command(op, name_json, data_json);
}

Error open_error(std::string where, std::string_view file, std::string_view why)
{
    const std::string reason = why.empty() ? std::strerror(errno) : std::string(why);
    return Error{std::move(where), xpp::format("cannot open {}: {}", file, reason), Place{std::string(file)}};
}

/* ---- the core's own files ------------------------------------------------------ */

namespace {

/* the rest of from, byte for byte, into to */
void copy_bytes(FILE *from, FILE *to)
{
    std::vector<char> buf(1 << 16);
    size_t n;
    while ((n = std::fread(buf.data(), 1, buf.size(), from)) > 0) std::fwrite(buf.data(), 1, n, to);
}

/* to becomes first's bytes, then (when given) second's. Binary: AUTO's
   files carry their own line ends, which text mode would rewrite. Both
   opens are checked: on Windows a file still open elsewhere cannot be
   opened, and writing into a NULL FILE * left fort.3 empty, which AUTO
   then reported as "Restart label N not found". Both are closed before
   the rename: Windows cannot replace an open file. */
void concat(const char *first, xpp::UniqueFile second, const char *to)
{
    xpp::UniqueFile in(std::fopen(first, "rb"));
    if (!in) {
        xpp::log(XPP_LOG_WARN, "{}\n", open_error("copy",first).text());
        return;
    }
    xpp::Writer w = xpp::Writer::binary(to);
    if (!w) {
        xpp::log(XPP_LOG_WARN, "{}\n", open_error("copy",to).text());
        return;
    }
    copy_bytes(in.get(), w.file());
    in.reset();
    if (second) copy_bytes(second.get(), w.file());
    second.reset();
    if(const xpp::Result<> saved=w.commit();!saved)xpp::log(XPP_LOG_ERROR,"{}\n",saved.error().text());
}

} // namespace

namespace {

/* observe_reads()'s observer and serve_reads()'s server (the core
   thread's) */
void (*read_observer)(const std::string &path);
struct FileServer {
    bool (*read)(const std::string &path, std::string *copy) = nullptr;
    bool (*write)(std::string_view path, bool opening, int kind) = nullptr;
} read_server;

/* a read the server has a say in */
bool served_read(std::string_view path) { return read_server.read && !xpp::files::is_scratch(path); }

} // namespace

void observe_reads(void (*observer)(const std::string &path)) { read_observer = observer; }

void serve_reads(bool (*server)(const std::string &path, std::string *copy),
                 bool (*write)(std::string_view path, bool opening, int kind)) { read_server = {server, write}; }

bool write_path_ok(std::string_view path, bool opening)
{
    if (!read_server.read) return true;
    const auto [dir, base] = split_path(absolute(path));
    /* Replay creates only plain files: no traversal, subdirectory or link
       can turn its own scratch folder into a path to the user's files. */
    Stat st;
    const int kind = kind_of(std::string(path).c_str(), &st);
    /* Writer's exclusive temporary has a leading dot; the rest is a
       plain name, checked by the same owner as its eventual target. */
    const std::string_view plain = std::string_view(base).starts_with('.') ? std::string_view(base).substr(1) : base;
    const bool allowed = read_server.write ? read_server.write(path, opening, kind) : is_scratch(dir, true);
    if (allowed && name_ok(plain) && (kind == XPP_FILES_OK || kind == XPP_FILES_NOT_FOUND)) return true;
    errno = EACCES;
    return false;
}

FILE *open_read_within(std::string_view path, std::string_view folder)
{
    const std::filesystem::path base = std::filesystem::path(absolute(folder.empty() ? "." : folder)).lexically_normal();
    const std::filesystem::path target = std::filesystem::path(absolute(path));
    const std::filesystem::path relative = target.lexically_relative(base);
    if (relative.empty() || relative.is_absolute()) { errno = EACCES; return nullptr; }
    std::filesystem::path checked = base;
    for (const auto &part : relative) {
        if (part == "..") { errno = EACCES; return nullptr; }
        if (part == ".") continue;
        if (!name_ok(part.string(), true)) { errno = EACCES; return nullptr; }
        checked /= part;
        if (is_link(checked.string().c_str())) { errno = EACCES; return nullptr; }
    }
    if (served_read(path)) return open_stream(path, "rb");
    xpp::UniqueFile fp;
    unsigned long long size;
    const int status = open_plain(checked.string().c_str(), fp, size);
    if (status != XPP_FILES_OK) { errno = status == XPP_FILES_NOT_FOUND ? ENOENT : EACCES; return nullptr; }
    if (read_observer) read_observer(std::string(path));
    return fp.release();
}

FILE *open_stream(std::string_view path, const char *mode)
{
    const bool reading = mode[0] == 'r' && !std::strchr(mode, '+');
    if (!reading && !write_path_ok(path, true)) return nullptr;
    std::string name, copy;
    bool served = false;
    try {
        name = path;
        if (reading && served_read(path)) {
            if (!read_server.read(name, &copy)) {
                errno = ENOENT;
                return nullptr;
            }
            served = true;
        }
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("serving a file");
    }
    FILE *f = std::fopen(served ? copy.c_str() : name.c_str(), mode);
    if (f && read_observer && reading) read_observer(name);
    return f;
}

int stream_fd(FILE *f)
{
#ifdef _WIN32
    if (_fileno(f) < 0 && !std::freopen("NUL", "w", f)) return -1;
#endif
    return fileno(f);
}

FILE *create_new(std::string_view path_view, bool binary)
{
    if (!write_path_ok(path_view)) return nullptr;
    const std::string path(path_view);
#ifdef _WIN32
    unsigned long long size;
    int fd = xpp::win32::open_plain(path.c_str(), true, binary, size);
#else
    int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | (binary ? O_BINARY : O_TEXT), 0666);
#endif
    if (fd < 0) return nullptr;
    FILE *fp = fdopen(fd, binary ? "wb" : "w");
    if (!fp) {
        close(fd);
        std::remove(path.c_str());
    }
    return fp;
}

bool exists(std::string_view path_view)
{
    const std::string path(path_view);
    Stat st;
    if (served_read(path) && read_server.read(path, nullptr)) return true;
    return stat_follow(path.c_str(), &st) == 0;
}

int remove(std::string_view path) { return std::remove(std::string(path).c_str()); }

void copy(std::string_view from, std::string_view to)
{
    concat(std::string(from).c_str(), xpp::UniqueFile(), std::string(to).c_str());
}

void prepend(std::string_view from_view, std::string_view to_view)
{
    const std::string from(from_view), to(to_view);
    xpp::UniqueFile own(std::fopen(to.c_str(), "rb"));
    if (!own) {
        copy(from, to);
        return;
    }
    concat(from.c_str(), std::move(own), to.c_str());
}

void move(std::string_view from_view, std::string_view to_view)
{
    const std::string from(from_view), to(to_view);
    /* POSIX rename() replaces an existing destination; on Windows it fails,
       so the old file was silently kept and the source left behind */
    if (std::rename(from.c_str(), to.c_str()) == 0) return;
    std::remove(to.c_str());
    if (std::rename(from.c_str(), to.c_str()) == 0) return;
    copy(from, to); /* the source may still be open: copy, then try to drop it */
    std::remove(from.c_str());
}

/* is_dir, dir_writable, make_temp_dir, remove_temp_dir,
   cleanup_stale_temp_dirs and list_dir are core/xpp_files_dir.cpp. */

} // namespace xpp::files
