/* The model's folder as the page's workspace: see xpp_files.h. Folder
   operations (listing, the wildcard match, the current folder, AUTO's
   scratch folders) are core/xpp_files_dir.cpp, behind the same header;
   this file keeps the page's file API and the core's own single-file
   operations. */
#include "xpp_files.h"
#include "xpp_files_internal.h"
#include "xpp_io.h"
#include "xpp_log.h"
#include "xpp_mem.h"
#include "xpp_sha256.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
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
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK 0
#endif
#ifndef O_TEXT
#define O_TEXT 0
#endif

struct XppFilePut {
    xpp::Writer w; /* binary: the temp file beside name, renamed over it at commit */
    std::string name;
    unsigned long long cap = 0, bytes = 0;
    xpp::Sha256 sha;
};

namespace {

/* ---- names ------------------------------------------------------------------ */

bool device_name(const char *name)
{
    /* CON, NUL, COM1.txt ...: Windows opens the device whatever follows the dot */
    static const char *const devices[] = {"con", "prn", "aux", "nul", "conin$", "conout$"};
    std::string stem;
    for (const char *p = name; *p && *p != '.'; p++) stem += static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
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
   never followed here) and replace_file are this file's own. */
using namespace xpp::files;

#ifdef _WIN32
int stat_name(const char *name, Stat *st) { return _stat64(name, st); }
int replace_file(const char *from, const char *to) { return xpp_replace_file(from, to); }
#else
int stat_name(const char *name, Stat *st) { return lstat(name, st); }
int replace_file(const char *from, const char *to) { return std::rename(from, to); }
#endif

/* what a name is on disk: NOT_FOUND, REFUSED (not a plain file) or OK */
int kind_of(const char *name, Stat *st)
{
    if (is_link(name)) return XPP_FILES_REFUSED;
    if (stat_name(name, st) != 0) return errno == ENOENT ? XPP_FILES_NOT_FOUND : XPP_FILES_IO;
    return S_ISREG(st->st_mode) ? XPP_FILES_OK : XPP_FILES_REFUSED;
}

int open_plain(const char *name, xpp::UniqueFile &fp, unsigned long long *size)
{
    Stat st;
    int k = kind_of(name, &st);
    if (k != XPP_FILES_OK) return k;
    /* O_NOFOLLOW: a link made between the check and the open is not followed */
    int fd = open(name, O_RDONLY | O_BINARY | O_NOFOLLOW | O_NONBLOCK);
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
    *size = static_cast<unsigned long long>(st.st_size);
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
    if (open_plain(name, fp, &size) != XPP_FILES_OK) return false;
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
   a file name below (xpp_files_name_ok), and calls the shared decoder in
   "strict" mode: false for a control character or an unpaired surrogate,
   which are not a name we take. */

void json_str(std::string &s, const std::string &v)
{
    s += '"';
    xpp::json_encode_string(s, v);
    s += '"';
}

/* a JSON string value, strictly: false for anything but a string, and for
   one that holds a control character (\u0000 included) or an unpaired
   surrogate */
bool json_string(const char *v, std::string &out)
{
    return xpp::json_decode_string(v, out, static_cast<size_t>(-1), /*strict=*/true);
}

/* ---- base64 ----------------------------------------------------------------------- */

constexpr std::string_view B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void base64_append(std::string &s, const unsigned char *p, size_t n)
{
    size_t i = 0;
    for (; i + 3 <= n; i += 3) {
        s += B64[p[i] >> 2];
        s += B64[(p[i] & 3) << 4 | p[i + 1] >> 4];
        s += B64[(p[i + 1] & 15) << 2 | p[i + 2] >> 6];
        s += B64[p[i + 2] & 63];
    }
    if (n - i == 1) {
        s += B64[p[i] >> 2];
        s += B64[(p[i] & 3) << 4];
        s += "==";
    } else if (n - i == 2) {
        s += B64[p[i] >> 2];
        s += B64[(p[i] & 3) << 4 | p[i + 1] >> 4];
        s += B64[(p[i + 1] & 15) << 2];
        s += '=';
    }
}

/* put_base64's answer when the data is not a base64 string */
const int NOT_BASE64 = -1;

/* the JSON string value v (base64, padded or not) into a put */
int put_base64(XppFilePut *put, const char *v)
{
    if (!v || *v != '"') return NOT_BASE64;
    std::array<unsigned char, 3 * 1024> out;
    size_t k = 0;
    int q[4], nq = 0, pad = 0;
    for (v++; *v != '"'; v++) {
        if (!*v) return NOT_BASE64;
        if (*v == '=') {
            pad++;
            continue;
        }
        int d = xpp::base64_value(static_cast<unsigned char>(*v));
        if (d < 0 || pad) return NOT_BASE64;
        q[nq++] = d;
        if (nq == 4) {
            out[k++] = static_cast<unsigned char>(q[0] << 2 | q[1] >> 4);
            out[k++] = static_cast<unsigned char>(q[1] << 4 | q[2] >> 2);
            out[k++] = static_cast<unsigned char>(q[2] << 6 | q[3]);
            nq = 0;
            if (k == out.size()) {
                int st = xpp_files_put_write(put, out.data(), k);
                if (st != XPP_FILES_OK) return st;
                k = 0;
            }
        }
    }
    if (nq == 1 || pad > 2) return NOT_BASE64;
    if (nq >= 2) out[k++] = static_cast<unsigned char>(q[0] << 2 | q[1] >> 4);
    if (nq == 3) out[k++] = static_cast<unsigned char>(q[1] << 4 | q[2] >> 2);
    return k ? xpp_files_put_write(put, out.data(), k) : XPP_FILES_OK;
}

/* ---- the listing ------------------------------------------------------------------ */

struct Entry {
    std::string name;
    unsigned long long size;
    long long mtime;
    std::string sha;
};

std::string listing()
{
    std::vector<Entry> files;
    if (DIR *d = opendir(".")) {
        while (struct dirent *e = readdir(d)) {
            if (!xpp_files_name_ok(e->d_name)) continue; /* ".", "..", hidden, unreachable */
            Stat st;
            if (kind_of(e->d_name, &st) != XPP_FILES_OK) continue;
            Entry f{e->d_name, static_cast<unsigned long long>(st.st_size), static_cast<long long>(st.st_mtime), ""};
            if (!cached(f.name, f.size, f.mtime, f.sha)) {
                if (!file_sha(e->d_name, f.sha)) continue;
                remember(f.name, f.size, f.mtime, f.sha);
            }
            files.push_back(std::move(f));
        }
        closedir(d);
    }
    std::sort(files.begin(), files.end(), [](const Entry &a, const Entry &b) { return a.name < b.name; });
    std::string s = "{\"files\":[";
    for (size_t i = 0; i < files.size(); i++) {
        if (i) s += ',';
        s += "{\"name\":";
        json_str(s, files[i].name);
        s += xpp::format(",\"size\":{},\"mtime\":{},\"sha256\":", files[i].size, files[i].mtime);
        json_str(s, files[i].sha);
        s += '}';
    }
    s += "]}";
    return s;
}

/* the `file` event, with what follows its name */
std::string file_event(const char *op, const std::string &name)
{
    std::string s = "{\"ev\":\"file\",\"op\":";
    json_str(s, op);
    if (!name.empty()) {
        s += ",\"name\":";
        json_str(s, name);
    }
    return s;
}

void fail_event(std::string &s, int status)
{
    s += ",\"ok\":0,\"error\":";
    json_str(s, xpp_files_status_text(status));
    s += '}';
}

std::string command(const char *op, const char *name_json, const char *data_json)
{
    std::string name;
    if (std::strcmp(op, "list") == 0) {
        std::string s = file_event(op, name), l = listing();
        s += ",\"ok\":1,";
        s.append(l, 1, std::string::npos); /* {"files":[...]} without its brace */
        return s;
    }
    bool named = json_string(name_json, name) && xpp_files_name_ok(name.c_str());
    std::string s = file_event(op, name_json && named ? name : std::string());
    if (std::strcmp(op, "get") != 0 && std::strcmp(op, "put") != 0) {
        s += ",\"ok\":0,\"error\":\"unknown op\"}";
        return s;
    }
    if (!named) {
        fail_event(s, XPP_FILES_BAD_NAME);
        return s;
    }
    std::string sha;
    unsigned long long size = 0;
    if (op[0] == 'p') {
        XppFilePut *put;
        int st = xpp_files_put_begin(name.c_str(), XPP_FILES_CAP, &put);
        if (st == XPP_FILES_OK) {
            st = put_base64(put, data_json);
            if (st == XPP_FILES_OK) st = xpp_files_put_commit(put, &size, sha);
            else xpp_files_put_abort(put);
        }
        if (st == NOT_BASE64) {
            s += ",\"ok\":0,\"error\":\"data is not a base64 string\"}";
            return s;
        }
        if (st != XPP_FILES_OK) {
            fail_event(s, st);
            return s;
        }
        s += xpp::format(",\"ok\":1,\"size\":{},\"sha256\":\"{}\"}}", size, sha);
        return s;
    }
    xpp::UniqueFile fp;
    int st = open_plain(name.c_str(), fp, &size);
    if (st == XPP_FILES_OK && size > XPP_FILES_CAP) {
        fp.reset();
        st = XPP_FILES_TOO_LARGE;
    }
    if (st != XPP_FILES_OK) {
        fail_event(s, st);
        return s;
    }
    std::vector<unsigned char> buf(size ? static_cast<size_t>(size) : 1);
    size_t got = std::fread(buf.data(), 1, static_cast<size_t>(size), fp.get());
    fp.reset();
    if (got != size) {
        fail_event(s, XPP_FILES_IO);
        return s;
    }
    xpp::Sha256 c;
    c.update(buf.data(), got);
    s += xpp::format(",\"ok\":1,\"size\":{},\"sha256\":\"{}\",\"data\":\"", size, c.hex());
    s.reserve(s.size() + (got + 2) / 3 * 4 + 4);
    base64_append(s, buf.data(), got);
    s += "\"}";
    return s;
}

} // namespace

/* ---- the C API ------------------------------------------------------------------- */

int xpp_files_name_ok(const char *name)
{
    if (!name || !name[0] || name[0] == '.' || name[0] == ' ') return 0;
    size_t n = std::strlen(name);
    if (n > 255 || std::strstr(name, "..")) return 0;
    for (const char *p = name; *p; p++) {
        unsigned char c = static_cast<unsigned char>(*p);
        if (c < 0x20 || c == 0x7f || std::strchr("/\\:<>\"|?*", c)) return 0;
    }
    /* Windows drops a trailing dot or space, so "a.set." would be a.set */
    if (name[n - 1] == '.' || name[n - 1] == ' ') return 0;
    return !device_name(name);
}

int xpp_files_replace_file(const char *from, const char *to) { return replace_file(from, to); }

const char *xpp_files_status_text(int status)
{
    switch (status) {
    case XPP_FILES_OK: return "ok";
    case XPP_FILES_BAD_NAME: return "not a plain file name in the model's folder";
    case XPP_FILES_NOT_FOUND: return "no such file";
    case XPP_FILES_REFUSED: return "not a plain file (a link, a folder or a device)";
    case XPP_FILES_TOO_LARGE: return "larger than 64 MB";
    default: return "the file could not be read or written";
    }
}

std::string xpp_files_list_json()
{
    try {
        return listing();
    } catch (const std::bad_alloc &) {
        xpp_out_of_memory("listing the model's folder");
    }
}

int xpp_files_open(const char *name, FILE **fp, unsigned long long *size)
{
    if (!xpp_files_name_ok(name)) return XPP_FILES_BAD_NAME;
    xpp::UniqueFile f;
    int st = open_plain(name, f, size);
    *fp = f.release();
    return st;
}

int xpp_files_put_begin(const char *name, unsigned long long cap, XppFilePut **put)
{
    *put = nullptr;
    if (!xpp_files_name_ok(name)) return XPP_FILES_BAD_NAME;
    Stat st;
    int k = kind_of(name, &st);
    if (k != XPP_FILES_OK && k != XPP_FILES_NOT_FOUND) return k;
    try {
        std::unique_ptr<XppFilePut> p = std::make_unique<XppFilePut>();
        p->name = name;
        p->cap = cap;
        p->w = xpp::Writer::binary(name); /* hidden (a leading dot): neither listed nor reachable by name */
        if (!p->w) return XPP_FILES_IO;
        *put = p.release();
        return XPP_FILES_OK;
    } catch (const std::bad_alloc &) {
        xpp_out_of_memory("starting an upload");
    }
}

int xpp_files_put_write(XppFilePut *put, const void *data, size_t n)
{
    if (n > put->cap - put->bytes) return XPP_FILES_TOO_LARGE;
    if (n && std::fwrite(data, 1, n, put->w.file()) != n) return XPP_FILES_IO;
    put->bytes += n;
    put->sha.update(data, n);
    return XPP_FILES_OK;
}

void xpp_files_put_abort(XppFilePut *put) { delete put; /* its writer discards the temp file */ }

int xpp_files_put_commit(XppFilePut *put, unsigned long long *size, std::string &sha256)
{
    std::unique_ptr<XppFilePut> p(put);
    Stat st;
    int k = kind_of(p->name.c_str(), &st);
    if (k != XPP_FILES_OK && k != XPP_FILES_NOT_FOUND) return k; /* became a link or a folder meanwhile */
    if (!p->w.commit()) return XPP_FILES_IO;
    *size = p->bytes;
    try {
        sha256 = p->sha.hex();
        if (stat_name(p->name.c_str(), &st) == 0 && static_cast<unsigned long long>(st.st_size) == p->bytes)
            remember(p->name, p->bytes, static_cast<long long>(st.st_mtime), sha256);
    } catch (const std::bad_alloc &) {
        xpp_out_of_memory("finishing an upload");
    }
    return XPP_FILES_OK;
}

const char *xpp_files_ask_mode(const char *title)
{
    /* the file selectors' titles (file_selector() callers): "Load SET
       File", "Read initial data", "Import Diagram", "Select an ODE file",
       "Library:" open a file; "Save ...", "Write ...", "Postscript",
       "GIF plot", "Clone ODE file", ... write one */
    static const char *const reads[] = {"load", "read", "import", "open", "select", "library"};
    std::string word; /* at most 15 letters: kept in the string itself, no allocation */
    while (title && *title == ' ') title++;
    for (const char *p = title; p && word.size() < 15 && std::isalpha(static_cast<unsigned char>(*p)); p++)
        word += static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
    for (const char *r : reads)
        if (word == r) return "read";
    return "write";
}

void xpp_files_command(const char *op, const char *name_json, const char *data_json,
                       void (*emit)(const char *line, size_t n))
{
    try {
        std::string s = command(op ? op : "", name_json, data_json);
        emit(s.data(), s.size());
    } catch (const std::bad_alloc &) {
        xpp_out_of_memory("in a file command");
    }
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
        xpp::log(XPP_LOG_WARN, "Cannot read {} \n", first);
        return;
    }
    xpp::Writer w = xpp::Writer::binary(to);
    if (!w) {
        xpp::log(XPP_LOG_WARN, "Cannot write {} \n", to);
        return;
    }
    copy_bytes(in.get(), w.file());
    in.reset();
    if (second) copy_bytes(second.get(), w.file());
    second.reset();
    w.commit();
}

} // namespace

namespace {

/* xpp_files_observe_reads()'s observer (the core thread's) */
void (*read_observer)(const char *path);

} // namespace

void xpp_files_observe_reads(void (*observer)(const char *path)) { read_observer = observer; }

FILE *xpp_files_open_stream(const char *path, const char *mode)
{
    FILE *f = path ? std::fopen(path, mode) : nullptr;
    if (f && read_observer && mode[0] == 'r' && !std::strchr(mode, '+')) read_observer(path);
    return f;
}

int xpp_files_stream_fd(FILE *f)
{
#ifdef _WIN32
    if (_fileno(f) < 0 && !std::freopen("NUL", "w", f)) return -1;
#endif
    return fileno(f);
}

FILE *xpp_files_create_new(const char *path, int binary)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | (binary ? O_BINARY : O_TEXT), 0666);
    if (fd < 0) return nullptr;
    FILE *fp = fdopen(fd, binary ? "wb" : "w");
    if (!fp) {
        close(fd);
        std::remove(path);
    }
    return fp;
}

int xpp_files_exists(const char *path)
{
    Stat st;
    return path && stat_follow(path, &st) == 0;
}

int xpp_files_remove(const char *path) { return std::remove(path); }

void xpp_files_copy(const char *from, const char *to) { concat(from, xpp::UniqueFile(), to); }

void xpp_files_prepend(const char *from, const char *to)
{
    xpp::UniqueFile own(std::fopen(to, "rb"));
    if (!own) {
        xpp_files_copy(from, to);
        return;
    }
    concat(from, std::move(own), to);
}

void xpp_files_move(const char *from, const char *to)
{
    /* POSIX rename() replaces an existing destination; on Windows it fails,
       so the old file was silently kept and the source left behind */
    if (std::rename(from, to) == 0) return;
    std::remove(to);
    if (std::rename(from, to) == 0) return;
    xpp_files_copy(from, to); /* the source may still be open: copy, then try to drop it */
    std::remove(from);
}

/* xpp_files_is_dir, xpp_files_dir_writable, xpp_files_make_temp_dir,
   xpp_files_remove_temp_dir, xpp_files_cleanup_stale_temp_dirs and
   xpp_files_list_dir are core/xpp_files_dir.cpp. */
