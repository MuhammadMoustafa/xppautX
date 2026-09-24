/* core/xpp_io.h's implementation. C API (extern "C"), C++ inside: a
   small mutex-guarded set dedupes the "truncated" warning per call site
   (file:line), so a call made every integration step does not flood the
   log. No exception crosses into C (CLAUDE.md, "C and C++"): the only
   things that can throw here are the set's own allocations, caught so a
   formatting call can never itself abort the caller. */
#include "xpp_io.h"
#include "xpp_files.h"
#include "xpp_log.h"

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <mutex>
#include <string>
#include <unordered_set>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace {

std::mutex g_warned_mutex;
std::unordered_set<std::string> g_warned_sites;

/* True the first time this file:line is seen, false after (so the
   caller warns once). Never throws: a std::bad_alloc from the set is
   swallowed and treated as "not yet warned", which just means that one
   site may warn more than once under memory pressure. */
bool first_time_at(const char *file, int line)
{
    char key[512];
    std::snprintf(key, sizeof key, "%s:%d", file ? file : "?", line);
    try {
        std::lock_guard<std::mutex> lock(g_warned_mutex);
        return g_warned_sites.insert(key).second;
    } catch (...) {
        return true;
    }
}

void warn_once(const char *file, int line, const char *fmt, ...)
{
    if (!first_time_at(file, line)) return;
    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    xpp_log(XPP_LOG_WARN, "%s:%d: %s\n", file ? file : "?", line, msg);
}

/* strlen(s), but never looks past s[maxlen-1] (s need not be
   NUL-terminated within maxlen bytes). Avoids strnlen, which is POSIX,
   not C99/MinGW-portable across every target this file builds on. */
size_t bounded_len(const char *s, size_t maxlen)
{
    size_t n = 0;
    while (n < maxlen && s[n] != '\0') n++;
    return n;
}

} // namespace

int xpp_snprintf_at(char *dst, size_t size, const char *file, int line,
                     const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int want = std::vsnprintf(dst, size, fmt, ap);
    va_end(ap);

    if (want < 0) {
        /* an encoding error, not a size problem: vsnprintf may still
           leave dst without a NUL, so terminate it ourselves. */
        if (size > 0) dst[0] = '\0';
        warn_once(file, line, "xpp_snprintf: formatting error (fmt \"%s\")",
                   fmt ? fmt : "?");
        return want;
    }
    if (size > 0 && (size_t)want >= size) {
        warn_once(file, line,
                   "xpp_snprintf: wanted %d bytes, buffer is %zu: truncated",
                   want, size);
    }
    return want;
}

size_t xpp_strlcpy_at(char *dst, const char *src, size_t size,
                       const char *file, int line)
{
    size_t srclen = std::strlen(src);
    if (size > 0) {
        size_t n = srclen < size - 1 ? srclen : size - 1;
        if (n > 0) std::memcpy(dst, src, n);
        dst[n] = '\0';
    }
    if (srclen >= size) {
        warn_once(file, line,
                   "xpp_strlcpy: wanted %zu bytes, buffer is %zu: truncated",
                   srclen, size);
    }
    return srclen;
}

size_t xpp_strlcat_at(char *dst, const char *src, size_t size,
                       const char *file, int line)
{
    size_t dstlen = bounded_len(dst, size);
    size_t srclen = std::strlen(src);

    if (dstlen >= size) {
        /* dst was not NUL-terminated within size: nothing safe to
           append. Report and leave dst untouched, like BSD strlcat. */
        warn_once(file, line,
                   "xpp_strlcat: destination not NUL-terminated within "
                   "%zu bytes", size);
        return size + srclen;
    }
    size_t avail = size - dstlen - 1;
    size_t n = srclen < avail ? srclen : avail;
    if (n > 0) std::memcpy(dst + dstlen, src, n);
    dst[dstlen + n] = '\0';
    if (srclen > avail) {
        warn_once(file, line,
                   "xpp_strlcat: wanted %zu bytes, buffer is %zu: truncated",
                   dstlen + srclen, size);
    }
    return dstlen + srclen;
}

/* ===================================================================
   The file half: line reader, token reader, writer (issue: W11 step 3,
   W7b). See xpp_io.h for the contract. */

struct XppLineReader {
    std::FILE *fp = nullptr;
    bool owns = false;
    std::string line;
};

struct XppTokenReader {
    std::FILE *fp = nullptr;
    bool owns = false;
};

struct XppWriter {
    std::FILE *fp = nullptr;
    std::string tmp, target;
};

namespace {

/* ---- lines -------------------------------------------------------- */

/* Reads one line via fgetc, growing `line` without limit; strips a
   trailing \r so \n and \r\n both end a line. Returns false only at a
   real end of file (nothing at all was read) so an unterminated final
   line still comes back once, not twice. */
bool read_line(std::FILE *fp, std::string &line)
{
    line.clear();
    int c;
    bool any = false;
    while ((c = std::fgetc(fp)) != EOF) {
        any = true;
        if (c == '\n') break;
        line.push_back(static_cast<char>(c));
    }
    if (!any) return false;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return true;
}

/* ---- tokens --------------------------------------------------------- */

/* Skips leading whitespace, then collects the run of non-whitespace
   characters into `tok`. False at end of file before any token starts.
   The whitespace that ends the token goes back to the stream, as fscanf
   leaves it, so a line read after a token on an attached FILE* sees the
   same rest of the line. */
bool read_token(std::FILE *fp, std::string &tok)
{
    tok.clear();
    int c;
    do {
        c = std::fgetc(fp);
    } while (c != EOF && std::isspace(static_cast<unsigned char>(c)));
    if (c == EOF) return false;
    while (c != EOF && !std::isspace(static_cast<unsigned char>(c))) {
        tok.push_back(static_cast<char>(c));
        c = std::fgetc(fp);
    }
    if (c != EOF) std::ungetc(c, fp);
    return true;
}

/* ---- writer's temp file ---------------------------------------------- */

std::atomic<unsigned> writer_serial{0};

long long writer_pid()
{
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

/* path split at the last slash (either kind: a checkout may build on
   either platform); dir is empty for a bare file name (today's model
   files: always relative to the working directory). */
void split_path(const std::string &path, std::string &dir, std::string &base)
{
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) {
        dir.clear();
        base = path;
    } else {
        dir = path.substr(0, pos);
        base = path.substr(pos + 1);
    }
}

} // namespace

XppLineReader *xpp_line_reader_open(const char *path)
{
    std::FILE *fp = path ? std::fopen(path, "r") : nullptr;
    if (!fp) return nullptr;
    try {
        XppLineReader *r = new XppLineReader;
        r->fp = fp;
        r->owns = true;
        return r;
    } catch (const std::bad_alloc &) {
        std::fclose(fp);
        return nullptr;
    }
}

XppLineReader *xpp_line_reader_attach(FILE *fp)
{
    if (!fp) return nullptr;
    try {
        XppLineReader *r = new XppLineReader;
        r->fp = fp;
        r->owns = false;
        return r;
    } catch (const std::bad_alloc &) {
        return nullptr;
    }
}

const char *xpp_line_reader_next(XppLineReader *r, size_t *len)
{
    if (!r || !r->fp || !read_line(r->fp, r->line)) {
        if (len) *len = 0;
        return nullptr;
    }
    if (len) *len = r->line.size();
    return r->line.c_str();
}

void xpp_line_reader_close(XppLineReader *r)
{
    if (!r) return;
    if (r->owns && r->fp) std::fclose(r->fp);
    delete r;
}

XppTokenReader *xpp_token_reader_open(const char *path)
{
    std::FILE *fp = path ? std::fopen(path, "r") : nullptr;
    if (!fp) return nullptr;
    try {
        XppTokenReader *r = new XppTokenReader;
        r->fp = fp;
        r->owns = true;
        return r;
    } catch (const std::bad_alloc &) {
        std::fclose(fp);
        return nullptr;
    }
}

XppTokenReader *xpp_token_reader_attach(FILE *fp)
{
    if (!fp) return nullptr;
    try {
        XppTokenReader *r = new XppTokenReader;
        r->fp = fp;
        r->owns = false;
        return r;
    } catch (const std::bad_alloc &) {
        return nullptr;
    }
}

int xpp_token_reader_double(XppTokenReader *r, double *out)
{
    std::string tok;
    if (!r || !r->fp || !read_token(r->fp, tok)) return 0;
    char *end = nullptr;
    double v = std::strtod(tok.c_str(), &end);
    if (end == tok.c_str()) return 0;
    *out = v;
    return 1;
}

/* strtof, not (float)strtod: fscanf "%f"/"%g" rounds the decimal
   straight to float, and rounding through double first can differ. */
int xpp_token_reader_float(XppTokenReader *r, float *out)
{
    std::string tok;
    if (!r || !r->fp || !read_token(r->fp, tok)) return 0;
    char *end = nullptr;
    float v = std::strtof(tok.c_str(), &end);
    if (end == tok.c_str()) return 0;
    *out = v;
    return 1;
}

int xpp_token_reader_int(XppTokenReader *r, int *out)
{
    std::string tok;
    if (!r || !r->fp || !read_token(r->fp, tok)) return 0;
    char *end = nullptr;
    long v = std::strtol(tok.c_str(), &end, 10);
    if (end == tok.c_str()) return 0;
    *out = static_cast<int>(v);
    return 1;
}

/* fscanf "%ld"'s own grammar, one character of lookahead pushed back as
   fscanf pushes it back, so a field that follows with no space between
   (AUTO's "%5ld" columns) is the next read's. */
int xpp_token_reader_long(XppTokenReader *r, long *out)
{
    if (!r || !r->fp) return 0;
    std::string num;
    int c;
    do {
        c = std::fgetc(r->fp);
    } while (c != EOF && std::isspace(static_cast<unsigned char>(c)));
    if (c == '+' || c == '-') {
        num.push_back(static_cast<char>(c));
        c = std::fgetc(r->fp);
    }
    while (c != EOF && std::isdigit(static_cast<unsigned char>(c))) {
        num.push_back(static_cast<char>(c));
        c = std::fgetc(r->fp);
    }
    if (c != EOF) std::ungetc(c, r->fp);
    if (num.empty() || !std::isdigit(static_cast<unsigned char>(num.back()))) return 0;
    *out = std::strtol(num.c_str(), nullptr, 10);
    return 1;
}

int xpp_token_reader_skip_line(XppTokenReader *r)
{
    if (!r || !r->fp) return 0;
    int c;
    while ((c = std::fgetc(r->fp)) != EOF)
        if (c == '\n') return 1;
    return 0;
}

int xpp_token_reader_string(XppTokenReader *r, char *buf, size_t bufsize)
{
    std::string tok;
    if (!r || !r->fp || !read_token(r->fp, tok)) return 0;
    if (bufsize > 0) xpp_strlcpy(buf, tok.c_str(), bufsize);
    return 1;
}

void xpp_token_reader_close(XppTokenReader *r)
{
    if (!r) return;
    if (r->owns && r->fp) std::fclose(r->fp);
    delete r;
}

namespace {

XppWriter *writer_open(const char *path, const char *mode)
{
    if (!path || !*path) {
        xpp_log(XPP_LOG_ERROR, "xpp_writer_open: no destination path given\n");
        return nullptr;
    }
    std::string dir, base;
    split_path(path, dir, base);
    char name[320];
    int n = std::snprintf(name, sizeof name, "%s%s.%s.tmp-%lld-%u",
                           dir.c_str(), dir.empty() ? "" : "/", base.c_str(),
                           writer_pid(), writer_serial.fetch_add(1, std::memory_order_relaxed));
    if (n < 0 || static_cast<size_t>(n) >= sizeof name) {
        xpp_log(XPP_LOG_ERROR, "xpp_writer_open: path too long: %s\n", path);
        return nullptr;
    }
    std::FILE *fp = std::fopen(name, mode);
    if (!fp) {
        xpp_log(XPP_LOG_ERROR, "xpp_writer_open: cannot create a temp file for %s\n", path);
        return nullptr;
    }
    try {
        XppWriter *w = new XppWriter;
        w->fp = fp;
        w->tmp = name;
        w->target = path;
        return w;
    } catch (const std::bad_alloc &) {
        std::fclose(fp);
        std::remove(name);
        xpp_log(XPP_LOG_ERROR, "out of memory opening %s for writing\n", path);
        return nullptr;
    }
}

} // namespace

XppWriter *xpp_writer_open(const char *path) { return writer_open(path, "w"); }

XppWriter *xpp_writer_open_binary(const char *path) { return writer_open(path, "wb"); }

FILE *xpp_writer_file(XppWriter *w) { return w ? w->fp : nullptr; }

int xpp_writer_printf(XppWriter *w, const char *fmt, ...)
{
    if (!w || !w->fp) return -1;
    va_list ap;
    va_start(ap, fmt);
    int r = std::vfprintf(w->fp, fmt, ap);
    va_end(ap);
    if (r < 0) xpp_log(XPP_LOG_ERROR, "xpp_writer_printf: write failed for %s\n", w->target.c_str());
    return r;
}

int xpp_writer_commit(XppWriter *w)
{
    if (!w) return -1;
    int closed = std::fclose(w->fp);
    w->fp = nullptr;
    if (closed != 0) {
        xpp_log(XPP_LOG_ERROR, "xpp_writer_commit: write failed for %s\n", w->target.c_str());
        std::remove(w->tmp.c_str());
        delete w;
        return -1;
    }
    if (xpp_files_replace_file(w->tmp.c_str(), w->target.c_str()) != 0) {
        xpp_log(XPP_LOG_ERROR, "xpp_writer_commit: cannot replace %s\n", w->target.c_str());
        std::remove(w->tmp.c_str());
        delete w;
        return -1;
    }
    delete w;
    return 0;
}

void xpp_writer_abort(XppWriter *w)
{
    if (!w) return;
    if (w->fp) std::fclose(w->fp);
    std::remove(w->tmp.c_str());
    delete w;
}

namespace xpp {
void format_failed(const char *file, int line) noexcept
{
    xpp_log(XPP_LOG_ERROR, "out of memory formatting a string at %s:%d\n", file, line);
    std::exit(1);
}
}
