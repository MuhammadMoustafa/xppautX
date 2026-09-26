/* The model's folder as the page's workspace: see xpp_files.h. */
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_log.h"
#include "xpp_mem.h"
#include "xpp_sha256.h"
#ifdef _WIN32
#include "xpp_win32.h"
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <mutex>
#include <new>
#include <string>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
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

struct XppFilePut {
    xpp::UniqueFile fp;
    std::string name, tmp;
    unsigned long long cap = 0, bytes = 0;
    XppSha256 sha{};
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

#ifdef _WIN32
typedef struct _stat64 Stat;
int stat_name(const char *name, Stat *st) { return _stat64(name, st); }
bool is_link(const char *name) { return xpp_path_is_link(name) != 0; }
int replace_file(const char *from, const char *to) { return xpp_replace_file(from, to); }
long long pid() { return _getpid(); }
#else
typedef struct stat Stat;
int stat_name(const char *name, Stat *st) { return lstat(name, st); }
bool is_link(const char *name)
{
    struct stat st;
    return lstat(name, &st) == 0 && S_ISLNK(st.st_mode);
}
int replace_file(const char *from, const char *to) { return std::rename(from, to); }
long long pid() { return getpid(); }
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

/* a SHA-256 as 64 hex digits and a NUL (xpp_sha256_hex) */
using Hex = std::array<char, 65>;

struct Digest {
    unsigned long long size;
    long long mtime;
    std::string sha;
};
std::mutex cache_lock;
std::map<std::string, Digest> cache;

bool file_sha(const char *name, Hex &out)
{
    xpp::UniqueFile fp;
    unsigned long long size;
    if (open_plain(name, fp, &size) != XPP_FILES_OK) return false;
    XppSha256 c;
    xpp_sha256_init(&c);
    std::vector<unsigned char> buf(1 << 16);
    size_t n;
    while ((n = std::fread(buf.data(), 1, buf.size(), fp.get())) > 0) xpp_sha256_update(&c, buf.data(), n);
    bool ok = !std::ferror(fp.get());
    fp.reset();
    xpp_sha256_hex(&c, out.data());
    return ok;
}

/* a digest is kept only for a file untouched for 2 seconds: a rewrite of
   the same size within the second of its mtime would otherwise look
   unchanged */
void remember(const std::string &name, unsigned long long size, long long mtime, const char *sha)
{
    if (static_cast<long long>(std::time(nullptr)) - mtime < 2) return;
    std::lock_guard<std::mutex> g(cache_lock);
    cache[name] = Digest{size, mtime, sha};
}

bool cached(const std::string &name, unsigned long long size, long long mtime, Hex &out)
{
    std::lock_guard<std::mutex> g(cache_lock);
    auto it = cache.find(name);
    if (it == cache.end() || it->second.size != size || it->second.mtime != mtime) return false;
    std::memcpy(out.data(), it->second.sha.c_str(), out.size());
    return true;
}

/* ---- JSON --------------------------------------------------------------------------- */

void json_str(std::string &s, const std::string &v)
{
    s += '"';
    for (unsigned char c : v) {
        if (c == '"' || c == '\\') {
            s += '\\';
            s += static_cast<char>(c);
        } else if (c < 0x20) {
            s += xpp::format("\\u{:04x}", static_cast<unsigned>(c));
        } else s += static_cast<char>(c);
    }
    s += '"';
}

void put_utf8(std::string &s, unsigned u)
{
    if (u < 0x80) s += static_cast<char>(u);
    else if (u < 0x800) {
        s += static_cast<char>(0xc0 | u >> 6);
        s += static_cast<char>(0x80 | (u & 63));
    } else {
        s += static_cast<char>(0xe0 | u >> 12);
        s += static_cast<char>(0x80 | (u >> 6 & 63));
        s += static_cast<char>(0x80 | (u & 63));
    }
}

/* a JSON string value, strictly: false for anything but a string, and for
   one that holds a control character (\u0000 included) */
bool json_string(const char *v, std::string &out)
{
    if (!v || *v != '"') return false;
    for (v++; *v != '"'; v++) {
        unsigned char c = static_cast<unsigned char>(*v);
        if (!c || c < 0x20) return false;
        if (c != '\\') {
            out += static_cast<char>(c);
            continue;
        }
        c = static_cast<unsigned char>(*++v);
        if (c == '"' || c == '\\' || c == '/') out += static_cast<char>(c);
        else if (c == 'u') {
            unsigned u = 0;
            for (int i = 0; i < 4; i++) {
                int d = *++v;
                if (d >= '0' && d <= '9') d -= '0';
                else if ((d | 32) >= 'a' && (d | 32) <= 'f') d = (d | 32) - 'a' + 10;
                else return false;
                u = u * 16 + static_cast<unsigned>(d);
            }
            if (u < 0x20 || (u >= 0xd800 && u <= 0xdfff)) return false; /* surrogates: not a file name we take */
            put_utf8(out, u);
        } else return false; /* \n, \t, ...: control characters */
    }
    return true;
}

/* ---- base64 ----------------------------------------------------------------------- */

const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int b64_value(int c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

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
        int d = b64_value(static_cast<unsigned char>(*v));
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

std::atomic<unsigned> put_serial{0};

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
            Hex sha;
            if (!cached(f.name, f.size, f.mtime, sha)) {
                if (!file_sha(e->d_name, sha)) continue;
                remember(f.name, f.size, f.mtime, sha.data());
            }
            f.sha = sha.data();
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
    Hex sha;
    unsigned long long size = 0;
    if (op[0] == 'p') {
        XppFilePut *put;
        int st = xpp_files_put_begin(name.c_str(), XPP_FILES_CAP, &put);
        if (st == XPP_FILES_OK) {
            st = put_base64(put, data_json);
            if (st == XPP_FILES_OK) st = xpp_files_put_commit(put, &size, sha.data());
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
        s += xpp::format(",\"ok\":1,\"size\":{},\"sha256\":\"{}\"}}", size, sha.data());
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
    XppSha256 c;
    xpp_sha256_init(&c);
    xpp_sha256_update(&c, buf.data(), got);
    xpp_sha256_hex(&c, sha.data());
    s += xpp::format(",\"ok\":1,\"size\":{},\"sha256\":\"{}\",\"data\":\"", size, sha.data());
    s.reserve(s.size() + (got + 2) / 3 * 4 + 4);
    base64_append(s, buf.data(), got);
    s += "\"}";
    return s;
}

[[noreturn]] void out_of_memory(const char *what)
{
    xpp_log(XPP_LOG_ERROR, "out of memory %s\n", what);
    std::exit(1);
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

std::string xpp::files_list_json() { return listing(); }

char *xpp_files_list_json(size_t *len)
{
    try {
        std::string s = listing();
        /* a raw block: the C API hands it to a caller that frees it (C++
           callers take xpp::files_list_json's std::string instead) */
        char *out = static_cast<char *>(xpp_malloc(s.size() + 1));
        std::memcpy(out, s.c_str(), s.size() + 1);
        *len = s.size();
        return out;
    } catch (const std::bad_alloc &) {
        out_of_memory("listing the model's folder");
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
        xpp_sha256_init(&p->sha);
        /* hidden (a leading dot), so neither listed nor reachable by name */
        for (int tries = 0; tries < 100 && !p->fp; tries++) {
            std::string tmp = xpp::format(".xpp-put-{}-{}.part", pid(), put_serial++);
            int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_BINARY | O_NOFOLLOW, 0644);
            if (fd < 0) {
                if (errno == EEXIST) continue;
                break;
            }
            p->fp.reset(fdopen(fd, "wb"));
            if (!p->fp) {
                close(fd);
                std::remove(tmp.c_str());
                break;
            }
            p->tmp = tmp;
        }
        if (!p->fp) return XPP_FILES_IO;
        *put = p.release();
        return XPP_FILES_OK;
    } catch (const std::bad_alloc &) {
        out_of_memory("starting an upload");
    }
}

int xpp_files_put_write(XppFilePut *put, const void *data, size_t n)
{
    if (n > put->cap - put->bytes) return XPP_FILES_TOO_LARGE;
    if (n && std::fwrite(data, 1, n, put->fp.get()) != n) return XPP_FILES_IO;
    put->bytes += n;
    xpp_sha256_update(&put->sha, data, n);
    return XPP_FILES_OK;
}

void xpp_files_put_abort(XppFilePut *put)
{
    if (!put) return;
    put->fp.reset(); /* closed before the remove: Windows cannot remove an open file */
    std::remove(put->tmp.c_str());
    delete put;
}

int xpp_files_put_commit(XppFilePut *put, unsigned long long *size, char sha256[65])
{
    int closed = std::fclose(put->fp.release()); /* its result: a write that failed at the flush */
    Stat st;
    int k = kind_of(put->name.c_str(), &st);
    int status = closed != 0 ? XPP_FILES_IO
                 : k != XPP_FILES_OK && k != XPP_FILES_NOT_FOUND ? k /* became a link or a folder meanwhile */
                 : replace_file(put->tmp.c_str(), put->name.c_str()) != 0 ? XPP_FILES_IO
                 : XPP_FILES_OK;
    if (status != XPP_FILES_OK) {
        xpp_files_put_abort(put);
        return status;
    }
    xpp_sha256_hex(&put->sha, sha256);
    *size = put->bytes;
    try {
        if (stat_name(put->name.c_str(), &st) == 0 && static_cast<unsigned long long>(st.st_size) == put->bytes)
            remember(put->name, put->bytes, static_cast<long long>(st.st_mtime), sha256);
    } catch (const std::bad_alloc &) {
        out_of_memory("finishing an upload");
    }
    delete put;
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
        out_of_memory("in a file command");
    }
}
