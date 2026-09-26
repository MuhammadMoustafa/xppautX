#ifndef XPP_IO_H
#define XPP_IO_H

/* core/xpp_io.cpp: the one place the core formats into and copies text
   into fixed buffers (issue: W11 step 2). sprintf and strcpy write with
   no length, so an oversized name or expression silently overflows the
   buffer; xpp_snprintf/xpp_strlcpy/xpp_strlcat never write past the
   destination's size, always NUL-terminate (when size > 0) and log a
   WARN (core/xpp_log.h), once per call site, when what they wanted to
   write did not fit. Behaviour is otherwise identical to sprintf/strcpy,
   so converting a call site that was never going to overflow changes
   nothing.

   xpp_snprintf/xpp_strlcpy/xpp_strlcat take the destination's size
   explicitly, like the C library's snprintf/strlcpy/strlcat (the file
   and line are threaded through automatically, xpp_mem.h-style, by the
   macro wrapper below, for the warning). XPP_SPRINTF(dst, fmt, ...) and
   XPP_STRCPY(dst, src) additionally take the size from sizeof(dst): dst
   must be an array (sizeof sees the whole array through a struct member
   or an indexed element too, e.g. XPP_SPRINTF(s->name, ...) or
   XPP_SPRINTF(arr[i].name, ...)); a pointer destination -- a parameter,
   a strcpy return chained through, a computed offset like buf+n -- fails
   XPP_ARRAY_SIZE_CHECK to *compile* (a negative array size), instead of
   silently taking sizeof(pointer) as the size. Find that call's real
   destination size and call xpp_snprintf/xpp_strlcpy directly with it.

   xpp_snprintf's return is the length it would have written, like
   snprintf's (not -1 on truncation): a caller that compares it against
   the buffer size to detect truncation still works. xpp_strlcpy/
   xpp_strlcat, and XPP_SPRINTF/XPP_STRCPY/XPP_STRCAT, return the BSD
   strlcpy/strlcat convention (the length of the source, or dst+src) --
   not dst, unlike strcpy/strcat's own return: no call site in core used
   that return value (checked with grep before dropping it), and keeping
   it would make every statement-context use (`XPP_STRCPY(buf, s);`, the
   overwhelming majority) warn -Wunused-value on the discarded bare `dst`
   at the end of the comma expression. */

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

int xpp_snprintf_at(char *dst, size_t size, const char *file, int line,
                     const char *fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 5, 6)))
#endif
    ;
size_t xpp_strlcpy_at(char *dst, const char *src, size_t size,
                       const char *file, int line);
size_t xpp_strlcat_at(char *dst, const char *src, size_t size,
                       const char *file, int line);

#ifdef __cplusplus
}
#endif

#define xpp_snprintf(dst, size, ...) \
    xpp_snprintf_at((dst), (size), __FILE__, __LINE__, __VA_ARGS__)
#define xpp_strlcpy(dst, src, size) \
    xpp_strlcpy_at((dst), (src), (size), __FILE__, __LINE__)
#define xpp_strlcat(dst, src, size) \
    xpp_strlcat_at((dst), (src), (size), __FILE__, __LINE__)

/* sizeof(char[N]) with a non-positive N is a compile error: catches the
   pointer case (sizeof(dst) == sizeof(char *)) without typeof /
   __builtin_types_compatible_p, which -pedantic (CFLAGS) would warn
   about as a GNU extension at every call site. A destination array that
   is genuinely exactly sizeof(char*) bytes (8 on a 64-bit build) false-
   triggers this; call xpp_snprintf/xpp_strlcpy directly there instead. */
#define XPP_ARRAY_SIZE_CHECK(dst) \
    ((void)sizeof(char[1 - 2 * (sizeof(dst) == sizeof(char *))]))

#define XPP_SPRINTF(dst, ...) \
    (XPP_ARRAY_SIZE_CHECK(dst), \
     xpp_snprintf_at((dst), sizeof(dst), __FILE__, __LINE__, __VA_ARGS__))
#define XPP_STRCPY(dst, src) \
    (XPP_ARRAY_SIZE_CHECK(dst), \
     xpp_strlcpy_at((dst), (src), sizeof(dst), __FILE__, __LINE__))
#define XPP_STRCAT(dst, src) \
    (XPP_ARRAY_SIZE_CHECK(dst), \
     xpp_strlcat_at((dst), (src), sizeof(dst), __FILE__, __LINE__))

/* ---------------------------------------------------------------------
   The file half of the module (issue: W11 step 3, W7b): a line reader
   and a token reader replace the fgets/fscanf/feof loops that either cut
   a long line at a fixed buffer or (a bare "while (!feof(fp))") read the
   last line twice; a writer replaces fopen(path,"w")/fclose with a temp
   file renamed into place only on commit. All three work two ways: opened
   from a path, they own the FILE* (closed by xpp_*_close/commit/abort);
   attached to a FILE* the caller already has, they read or write through
   it without owning it, for helper functions (xpp_snprintf-style) that
   take a plain FILE* from elsewhere and must not close it. */

/* ---- lines: any length, CR/LF tolerant --------------------------------
   xpp_line_reader_next returns the next line without its terminator (a
   bare \n, a \r\n, or a final line with none at all); NULL at real end of
   file. No size limit: a line longer than any fixed buffer comes back
   whole. The returned pointer is owned by the reader -- valid until the
   next xpp_line_reader_next or xpp_line_reader_close, not by the caller. */
typedef struct XppLineReader XppLineReader;
XppLineReader *xpp_line_reader_open(const char *path);  /* fopen(path,"r"); NULL on failure */
XppLineReader *xpp_line_reader_attach(FILE *fp);         /* wraps fp; never closes it */
const char *xpp_line_reader_next(XppLineReader *r, size_t *len);
void xpp_line_reader_close(XppLineReader *r);

/* ---- whitespace-separated tokens: fscanf "%lg"/"%d"/"%s" equivalents --
   Skips leading whitespace, then reads the run of non-whitespace
   characters (the token) and converts it: strtod/strtol on that same
   token, so xpp_token_reader_double(r,&x) and "strtod(token,0)" agree.
   Returns 1 on success, 0 at end of file or on a token that does not
   parse as the requested kind -- fscanf's own convention, so `if
   (xpp_token_reader_double(r,&x) != 1) break;` reads exactly like the
   `if (fscanf(fp,"%lg",&x) != 1) break;` it replaces. Every value in the
   files this reads is whitespace/newline separated (never comma or other
   punctuation), where a whitespace-delimited token is exactly what
   fscanf's own float/int grammar would have consumed too.
   xpp_token_reader_string copies the token into buf (xpp_strlcpy-style:
   never overflows buf, truncates and warns once if the token does not
   fit) instead of fscanf "%s"'s unbounded write. */
typedef struct XppTokenReader XppTokenReader;
XppTokenReader *xpp_token_reader_open(const char *path);
XppTokenReader *xpp_token_reader_attach(FILE *fp);
int xpp_token_reader_double(XppTokenReader *r, double *out);
int xpp_token_reader_float(XppTokenReader *r, float *out);   /* "%f"/"%g" */
int xpp_token_reader_int(XppTokenReader *r, int *out);
int xpp_token_reader_string(XppTokenReader *r, char *buf, size_t bufsize);
/* fscanf "%ld" exactly, not a whole token: leading whitespace, an optional
   sign and the digits, and no more; what follows stays in the stream. For
   integer columns printed flush against each other, like the "%5ld"
   label lines of AUTO's fort.8/.s files, where "    2-1234" is 2 then
   -1234 and a whitespace-delimited token would swallow both. 0 when no
   digit follows (the sign, if any, consumed as fscanf consumes it). */
int xpp_token_reader_long(XppTokenReader *r, long *out);
/* The rest of the current line, its \n included (the "go to the end of the
   line" after reading a line's leading fields): 1 when a \n ended it, 0
   when end of file came first. */
int xpp_token_reader_skip_line(XppTokenReader *r);
void xpp_token_reader_close(XppTokenReader *r);

/* ---- writer: temp file, renamed into place only on commit --------------
   xpp_writer_open creates a hidden temp file next to `path` ("w" text
   mode, like the fopen(path,"w") this replaces, so a Windows build still
   writes \r\n exactly as before) and logs an ERROR (xpp_log) itself,
   returning NULL, if that fails -- most callers' own "cannot open file"
   message already covers the case, this is for the rest. xpp_writer_file
   is the FILE* to format into with ordinary fprintf, or xpp::format on
   the C++ side; xpp_writer_printf is a convenience fprintf-alike over it.
   xpp_writer_commit closes the temp file and renames it into place
   (core/xpp_files.cpp's xpp_files_replace_file -- the one place that
   knows POSIX rename() from Windows's xpp_replace_file, reused here
   rather than duplicated); on failure it logs an ERROR, leaves the
   original file untouched, and returns nonzero. xpp_writer_abort closes
   and discards the temp file without touching `path` at all. Either one
   frees w; abandoning a writer (never calling either) leaks the temp
   file, so C code always pairs xpp_writer_open with one of them on every
   path out of the function -- the C++ Writer wrapper below aborts
   automatically in its destructor when not committed. */
typedef struct XppWriter XppWriter;
XppWriter *xpp_writer_open(const char *path);
/* the same in binary mode ("wb"): for a byte-for-byte copy, whose lines
   end as the source's do on every platform */
XppWriter *xpp_writer_open_binary(const char *path);
FILE *xpp_writer_file(XppWriter *w);
int xpp_writer_printf(XppWriter *w, const char *fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;
int xpp_writer_commit(XppWriter *w);
void xpp_writer_abort(XppWriter *w);

#ifdef __cplusplus
/* C++ callers get a type-checked API instead of the C wrappers above
   (their printf-style format strings and vararg passing are for C
   callers): xpp::format/xpp::format_to_buf take a std::format_string,
   checked against the argument types at compile time, no printf %-verb
   ever mismatching an argument. xpp::number is a double's shortest
   round-trip text (std::to_chars), for when "%g" would do but a
   guaranteed-reversible digit string is wanted. This part of the header
   is skipped entirely by a C file, so xpp_snprintf/XPP_SPRINTF and
   friends keep working there unchanged; C++ files in the sweep (not
   ui_json.cpp/xpp_http.cpp, converted separately by T18) use this API
   for a real fixed-array destination and a literal/simple format, and
   still use xpp_snprintf/xpp_strlcpy directly for a pointer destination
   (format_to_buf, like XPP_SPRINTF, needs an actual array: see
   XPP_ARRAY_SIZE_CHECK above) or where the original format string used
   dynamic width/precision (%*s, %.*s) that would need re-expressing in
   std::format's syntax -- not a mechanical, behaviour-preserving change,
   so those stay on the C wrappers (documented at each such call site). */
#include <array>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#if defined(__cpp_lib_format) || (defined(__has_include) && __has_include(<format>))
#include <format>
#define XPP_IO_HAVE_STD_FORMAT 1
#endif
#if defined(__cpp_lib_to_chars) || (defined(__has_include) && __has_include(<charconv>))
#include <charconv>
#define XPP_IO_HAVE_TO_CHARS 1
#endif

namespace xpp {

/* std::format can throw (bad_alloc); no exception may cross into the C
   code that calls these C++ functions (CLAUDE.md), so a failure is loud
   and final like xpp_mem's: an ERROR naming the call site, exit 1 */
[[noreturn]] void format_failed(const char *file, int line) noexcept;

#ifdef XPP_IO_HAVE_STD_FORMAT
/* Compile-time checked formatting: a bad "{}" against the argument
   types is a compile error, not a WARN at run time. No length limit
   (returns a std::string); for a fixed buffer use format_to_buf. */
template <class... Args>
std::string format(std::format_string<Args...> fmt, Args &&...args) noexcept
{
    try {
        return std::format(fmt, std::forward<Args>(args)...);
    } catch (...) {
        format_failed(__FILE__, __LINE__);
    }
}

/* The array-destination counterpart of XPP_SPRINTF: dst must be a real
   array (a template on its size, not a decayed pointer -- a pointer
   destination simply does not match this overload and fails to
   compile), formats with std::format, then copies in with
   xpp_strlcpy_at so a result that does not fit is cut and warns once,
   exactly like every other truncation in this module. file/line come
   from the XPP_FORMAT_TO_BUF macro wrapper (xpp_mem.h/XPP_SPRINTF
   style), not std::source_location, to match the rest of the module
   and because a source_location default argument cannot follow the
   variadic Args&&...args this shares with std::format's own signature. */
template <std::size_t N, class... Args>
void format_to_buf(char (&buf)[N], const char *file, int line,
                    std::format_string<Args...> fmt, Args &&...args) noexcept
{
    try {
        std::string s = std::format(fmt, std::forward<Args>(args)...);
        xpp_strlcpy_at(buf, s.c_str(), N, file, line);
    } catch (...) {
        format_failed(file, line);
    }
}
#endif

#ifdef XPP_IO_HAVE_TO_CHARS
/* A double's shortest round-trip decimal text (std::to_chars): unlike
   "%g" it never loses precision and never needs a chosen digit count.
   xpp_snprintf(..., "%.17g", ...) papers over the same problem with
   more digits than usually needed; prefer this in new C++ code. */
inline std::string number(double v)
{
    std::array<char, 32> buf;
    std::to_chars_result r = std::to_chars(buf.data(), buf.data() + buf.size(), v);
    if (r.ec == std::errc())
        return std::string(buf.data(), r.ptr);
    return std::to_string(v); /* unreachable for a finite double in 32 bytes */
}
#endif

/* A FILE * that closes itself: for a stream the core gets from an API
   that hands out a FILE * (xpp_files_open, fdopen, ...) rather than a
   path the readers/writer below could open. */
struct FileCloser {
    void operator()(FILE *fp) const noexcept
    {
        if (fp) std::fclose(fp);
    }
};
using UniqueFile = std::unique_ptr<FILE, FileCloser>;

/* ---- RAII wrappers over the C file API above --------------------------
   Thin move-only handles: a LineReader closes (if it opened the file
   itself) when it goes out of scope; a Writer that is destroyed without
   an explicit commit() aborts, leaving the original file untouched,
   exactly like the C API's "abandoned without commit" guarantee. Both
   are opened either from a path (owning) or attach()ed to a FILE* the
   caller keeps owning. */
class LineReader {
public:
    LineReader() = default;
    explicit LineReader(const char *path) noexcept : r_(xpp_line_reader_open(path)) {}
    static LineReader attach(FILE *fp) noexcept
    {
        LineReader l;
        l.r_ = xpp_line_reader_attach(fp);
        return l;
    }
    ~LineReader() { close(); }
    LineReader(const LineReader &) = delete;
    LineReader &operator=(const LineReader &) = delete;
    LineReader(LineReader &&o) noexcept : r_(o.r_) { o.r_ = nullptr; }
    LineReader &operator=(LineReader &&o) noexcept
    {
        if (this != &o) {
            close();
            r_ = o.r_;
            o.r_ = nullptr;
        }
        return *this;
    }
    explicit operator bool() const noexcept { return r_ != nullptr; }
    /* nullopt at end of file; the view is valid until the next next() or
       until this reader is destroyed/closed, same lifetime as the C API's
       pointer. */
    std::optional<std::string_view> next()
    {
        if (!r_) return std::nullopt;
        size_t len;
        const char *s = xpp_line_reader_next(r_, &len);
        if (!s) return std::nullopt;
        return std::string_view(s, len);
    }
    void close()
    {
        if (r_) {
            xpp_line_reader_close(r_);
            r_ = nullptr;
        }
    }
private:
    XppLineReader *r_ = nullptr;
};

/* The token reader, its conversion picked by the type read into:
   read(double&) is fscanf "%lg" (also "%le"/"%lf"), read(float&) "%g",
   read(int&) "%d" -- whole whitespace-delimited tokens -- and read(long&)
   fscanf "%ld" field by field (xpp_token_reader_long). Each is true on
   success, false where fscanf would not have returned 1. */
class TokenReader {
public:
    TokenReader() = default;
    explicit TokenReader(const char *path) noexcept : r_(xpp_token_reader_open(path)) {}
    static TokenReader attach(FILE *fp) noexcept
    {
        TokenReader t;
        t.r_ = xpp_token_reader_attach(fp);
        return t;
    }
    ~TokenReader() { close(); }
    TokenReader(const TokenReader &) = delete;
    TokenReader &operator=(const TokenReader &) = delete;
    TokenReader(TokenReader &&o) noexcept : r_(o.r_) { o.r_ = nullptr; }
    TokenReader &operator=(TokenReader &&o) noexcept
    {
        if (this != &o) {
            close();
            r_ = o.r_;
            o.r_ = nullptr;
        }
        return *this;
    }
    explicit operator bool() const noexcept { return r_ != nullptr; }
    bool read(double &x) noexcept { return r_ && xpp_token_reader_double(r_, &x) == 1; }
    bool read(float &x) noexcept { return r_ && xpp_token_reader_float(r_, &x) == 1; }
    bool read(int &x) noexcept { return r_ && xpp_token_reader_int(r_, &x) == 1; }
    bool read(long &x) noexcept { return r_ && xpp_token_reader_long(r_, &x) == 1; }
    bool skip_line() noexcept { return r_ && xpp_token_reader_skip_line(r_) == 1; }
    void close()
    {
        if (r_) {
            xpp_token_reader_close(r_);
            r_ = nullptr;
        }
    }
private:
    XppTokenReader *r_ = nullptr;
};

class Writer {
public:
    Writer() = default;
    explicit Writer(const char *path) noexcept : w_(xpp_writer_open(path)) {}
    static Writer binary(const char *path) noexcept
    {
        Writer w;
        w.w_ = xpp_writer_open_binary(path);
        return w;
    }
    ~Writer()
    {
        if (w_) xpp_writer_abort(w_);
    }
    Writer(const Writer &) = delete;
    Writer &operator=(const Writer &) = delete;
    Writer(Writer &&o) noexcept : w_(o.w_) { o.w_ = nullptr; }
    Writer &operator=(Writer &&o) noexcept
    {
        if (this != &o) {
            if (w_) xpp_writer_abort(w_);
            w_ = o.w_;
            o.w_ = nullptr;
        }
        return *this;
    }
    explicit operator bool() const noexcept { return w_ != nullptr; }
    FILE *file() const noexcept { return w_ ? xpp_writer_file(w_) : nullptr; }
#ifdef XPP_IO_HAVE_STD_FORMAT
    template <class... Args>
    void write(std::format_string<Args...> fmt, Args &&...args)
    {
        std::string s = format(fmt, std::forward<Args>(args)...);
        if (w_) ::fwrite(s.data(), 1, s.size(), xpp_writer_file(w_));
    }
#endif
    /* Renames the temp file into place; false (the original file left
       untouched) on failure. Either outcome ends this Writer -- a second
       commit()/abort() is a no-op. */
    bool commit()
    {
        if (!w_) return false;
        XppWriter *w = w_;
        w_ = nullptr;
        return xpp_writer_commit(w) == 0;
    }
    void abort()
    {
        if (w_) {
            xpp_writer_abort(w_);
            w_ = nullptr;
        }
    }
private:
    XppWriter *w_ = nullptr;
};

} // namespace xpp

#ifdef XPP_IO_HAVE_STD_FORMAT
#define XPP_FORMAT_TO_BUF(buf, ...) \
    xpp::format_to_buf((buf), __FILE__, __LINE__, __VA_ARGS__)
#endif

#endif /* __cplusplus */

#endif
