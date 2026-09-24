/* The model's folder as the page's workspace: see xpp_files.h. */
#include "xpp_files.h"
#include "xpp_log.h"
#include "xpp_mem.h"
#include "xpp_sha256.h"
#ifdef _WIN32
#include "xpp_win32.h"
#endif

#include <algorithm>
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
    std::FILE *fp = nullptr;
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

int open_plain(const char *name, std::FILE **fp, unsigned long long *size)
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
    *fp = fdopen(fd, "rb");
    if (!*fp) {
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

bool file_sha(const char *name, char out[65])
{
    std::FILE *fp;
    unsigned long long size;
    if (open_plain(name, &fp, &size) != XPP_FILES_OK) return false;
    XppSha256 c;
    xpp_sha256_init(&c);
    std::vector<unsigned char> buf(1 << 16);
    size_t n;
    while ((n = std::fread(buf.data(), 1, buf.size(), fp)) > 0) xpp_sha256_update(&c, buf.data(), n);
    bool ok = !std::ferror(fp);
    std::fclose(fp);
    xpp_sha256_hex(&c, out);
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

bool cached(const std::string &name, unsigned long long size, long long mtime, char out[65])
{
    std::lock_guard<std::mutex> g(cache_lock);
    auto it = cache.find(name);
    if (it == cache.end() || it->second.size != size || it->second.mtime != mtime) return false;
    std::memcpy(out, it->second.sha.c_str(), 65);
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
            char u[8];
            std::snprintf(u, sizeof u, "\\u%04x", c);
            s += u;
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
    unsigned char out[3 * 1024];
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
            if (k == sizeof out) {
                int st = xpp_files_put_write(put, out, k);
                if (st != XPP_FILES_OK) return st;
                k = 0;
            }
        }
    }
    if (nq == 1 || pad > 2) return NOT_BASE64;
    if (nq >= 2) out[k++] = static_cast<unsigned char>(q[0] << 2 | q[1] >> 4);
    if (nq == 3) out[k++] = static_cast<unsigned char>(q[1] << 4 | q[2] >> 2);
    return k ? xpp_files_put_write(put, out, k) : XPP_FILES_OK;
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
            char sha[65];
            if (!cached(f.name, f.size, f.mtime, sha)) {
                if (!file_sha(e->d_name, sha)) continue;
                remember(f.name, f.size, f.mtime, sha);
            }
            f.sha = sha;
            files.push_back(std::move(f));
        }
        closedir(d);
    }
    std::sort(files.begin(), files.end(), [](const Entry &a, const Entry &b) { return a.name < b.name; });
    std::string s = "{\"files\":[";
    for (size_t i = 0; i < files.size(); i++) {
        char num[64];
        if (i) s += ',';
        s += "{\"name\":";
        json_str(s, files[i].name);
        std::snprintf(num, sizeof num, ",\"size\":%llu,\"mtime\":%lld,\"sha256\":", files[i].size, files[i].mtime);
        s += num;
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
    char sha[65], num[192];
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
        std::snprintf(num, sizeof num, ",\"ok\":1,\"size\":%llu,\"sha256\":\"%s\"}", size, sha);
        s += num;
        return s;
    }
    std::FILE *fp;
    int st = open_plain(name.c_str(), &fp, &size);
    if (st == XPP_FILES_OK && size > XPP_FILES_CAP) {
        std::fclose(fp);
        st = XPP_FILES_TOO_LARGE;
    }
    if (st != XPP_FILES_OK) {
        fail_event(s, st);
        return s;
    }
    std::vector<unsigned char> buf(size ? static_cast<size_t>(size) : 1);
    size_t got = std::fread(buf.data(), 1, static_cast<size_t>(size), fp);
    std::fclose(fp);
    if (got != size) {
        fail_event(s, XPP_FILES_IO);
        return s;
    }
    XppSha256 c;
    xpp_sha256_init(&c);
    xpp_sha256_update(&c, buf.data(), got);
    xpp_sha256_hex(&c, sha);
    std::snprintf(num, sizeof num, ",\"ok\":1,\"size\":%llu,\"sha256\":\"%s\",\"data\":\"", size, sha);
    s += num;
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

char *xpp_files_list_json(size_t *len)
{
    try {
        std::string s = listing();
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
    return open_plain(name, fp, size);
}

int xpp_files_put_begin(const char *name, unsigned long long cap, XppFilePut **put)
{
    *put = nullptr;
    if (!xpp_files_name_ok(name)) return XPP_FILES_BAD_NAME;
    Stat st;
    int k = kind_of(name, &st);
    if (k != XPP_FILES_OK && k != XPP_FILES_NOT_FOUND) return k;
    try {
        XppFilePut *p = new XppFilePut;
        p->name = name;
        p->cap = cap;
        xpp_sha256_init(&p->sha);
        /* hidden (a leading dot), so neither listed nor reachable by name */
        for (int tries = 0; tries < 100 && !p->fp; tries++) {
            char tmp[64];
            std::snprintf(tmp, sizeof tmp, ".xpp-put-%lld-%u.part", pid(), put_serial++);
            int fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL | O_BINARY | O_NOFOLLOW, 0644);
            if (fd < 0) {
                if (errno == EEXIST) continue;
                break;
            }
            p->fp = fdopen(fd, "wb");
            if (!p->fp) {
                close(fd);
                std::remove(tmp);
                break;
            }
            p->tmp = tmp;
        }
        if (!p->fp) {
            delete p;
            return XPP_FILES_IO;
        }
        *put = p;
        return XPP_FILES_OK;
    } catch (const std::bad_alloc &) {
        out_of_memory("starting an upload");
    }
}

int xpp_files_put_write(XppFilePut *put, const void *data, size_t n)
{
    if (n > put->cap - put->bytes) return XPP_FILES_TOO_LARGE;
    if (n && std::fwrite(data, 1, n, put->fp) != n) return XPP_FILES_IO;
    put->bytes += n;
    xpp_sha256_update(&put->sha, data, n);
    return XPP_FILES_OK;
}

void xpp_files_put_abort(XppFilePut *put)
{
    if (!put) return;
    if (put->fp) std::fclose(put->fp);
    std::remove(put->tmp.c_str());
    delete put;
}

int xpp_files_put_commit(XppFilePut *put, unsigned long long *size, char sha256[65])
{
    int closed = std::fclose(put->fp);
    put->fp = nullptr;
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
    char word[16];
    size_t n = 0;
    while (title && *title == ' ') title++;
    while (title && std::isalpha(static_cast<unsigned char>(title[n])) && n + 1 < sizeof word) {
        word[n] = static_cast<char>(std::tolower(static_cast<unsigned char>(title[n])));
        n++;
    }
    word[n] = 0;
    for (const char *r : reads)
        if (std::strcmp(word, r) == 0) return "read";
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
