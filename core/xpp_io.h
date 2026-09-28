#ifndef XPP_IO_H
#define XPP_IO_H

/* core/xpp_io.cpp: the file half of the module (issue: W11 step 3, W7b).
   The C text-formatting half this header used to declare --
   xpp_snprintf/xpp_strlcpy/xpp_strlcat, XPP_SPRINTF/XPP_STRCPY/
   XPP_STRCAT, for a C file's sprintf/strcpy/vsprintf into a fixed
   buffer, which write with no length and silently overflow it -- was
   retired at W48: every core file is C++, and the last caller of the
   C++ counterpart it fed (xpp::format_to_buf) was gone too. New code
   formats into a std::string with xpp::format below (xpp::number for a
   double), or reads/writes through a fixed-size std::array/std::vector
   directly. */

#include <stddef.h>
#include <stdio.h>

#include "xpp_files.h" /* the files the readers and writers open */

/* ---------------------------------------------------------------------
   A line reader and a token reader replace the fgets/fscanf/feof loops
   that either cut a long line at a fixed buffer or (a bare "while
   (!feof(fp))") read the last line twice; a writer replaces
   fopen(path,"w")/fclose with a temp file renamed into place only on
   commit. All three work two ways: opened from a path, they own the
   FILE* (closed by xpp_*_close/commit/abort); attached to a FILE* the
   caller already has, they read or write through it without owning it,
   for a helper function that takes a plain FILE* from elsewhere and
   must not close it. */

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
   xpp_token_reader_string copies the token into buf (never overflows
   buf, truncates and warns once if the token does not fit) instead of
   fscanf "%s"'s unbounded write. */
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
   writes CRLF line ends exactly as before; created exclusively, never through a
   link: xpp_files_create_new) and logs an ERROR (xpp_log) itself,
   returning NULL, if that fails -- most callers' own "cannot open file"
   message already covers the case, this is for the rest. xpp_writer_file
   is the FILE* to format into with ordinary fprintf, or xpp::print on
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
   automatically in its destructor when not committed.

   xpp_writer_open_as picks the kind: XPP_WRITE_TEXT is xpp_writer_open;
   XPP_WRITE_BINARY the same in binary mode ("wb"), for a byte-for-byte
   copy whose lines end as the source's do on every platform;
   XPP_WRITE_APPEND writes at the end of `path` itself ("a"), no temp file:
   commit and abort both close it, and what was written stays. */
typedef struct XppWriter XppWriter;
enum { XPP_WRITE_TEXT = 0, XPP_WRITE_BINARY = 1, XPP_WRITE_APPEND = 2 };
XppWriter *xpp_writer_open(const char *path);
XppWriter *xpp_writer_open_as(const char *path, int how);
FILE *xpp_writer_file(XppWriter *w);
int xpp_writer_printf(XppWriter *w, const char *fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;
int xpp_writer_commit(XppWriter *w);
void xpp_writer_abort(XppWriter *w);

#ifdef __cplusplus
/* xpp::format takes a std::format_string, checked against the argument
   types at compile time, no printf %-verb ever mismatching an argument.
   xpp::number is a double's shortest round-trip text (std::to_chars),
   for when "%g" would do but a guaranteed-reversible digit string is
   wanted. */
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
/* The formatting itself, compiled once, here (xpp_io.cpp): the templates
   below (and xpp::log) only check the format string against the
   arguments at compile time and pack them (std::make_format_args), so a
   file that formats does not compile std::format's machinery again (W37:
   inlined in 81 files, it more than doubled the core's compile time) */
std::string vformat(std::string_view fmt, std::format_args args) noexcept;
/* the same, appended to out */
void vformat_append(std::string &out, std::string_view fmt, std::format_args args) noexcept;

/* Compile-time checked formatting: a bad "{}" against the argument
   types is a compile error, not a WARN at run time. Returns a
   std::string. */
template <class... Args>
std::string format(std::format_string<Args...> fmt, Args &&...args) noexcept
{
    return xpp::vformat(fmt.get(), std::make_format_args(args...));
}

/* the same, appended to out (a file's text built up before it is written) */
template <class... Args>
void format_append(std::string &out, std::format_string<Args...> fmt, Args &&...args) noexcept
{
    xpp::vformat_append(out, fmt.get(), std::make_format_args(args...));
}
#endif

#ifdef XPP_IO_HAVE_TO_CHARS
/* A double's shortest round-trip decimal text (std::to_chars): unlike
   "%g" it never loses precision and never needs a chosen digit count;
   prefer this in new C++ code. */
inline std::string number(double v)
{
    std::array<char, 32> buf;
    std::to_chars_result r = std::to_chars(buf.data(), buf.data() + buf.size(), v);
    if (r.ec == std::errc())
        return std::string(buf.data(), r.ptr);
    return std::to_string(v); /* unreachable for a finite double in 32 bytes */
}
#endif

/* An entry of a C table other files read as NUL-terminated text (char *)
   whose text a std::string keeps: store takes text and entry points at
   it, valid until store changes again */
inline void keep_c_text(std::string &store, char *&entry, std::string_view text)
{
    store = text;
    entry = store.data();
}

/* a and b are the same text but for the case of ASCII letters (strcasecmp's
   test in the C locale): a model's names are looked up this way */
inline bool equal_ignoring_case(std::string_view a, std::string_view b)
{
  if(a.size()!=b.size())return false;
  for(size_t i=0;i<a.size();i++){
    unsigned char x=static_cast<unsigned char>(a[i]),y=static_cast<unsigned char>(b[i]);
    if(x>='A'&&x<='Z')x=static_cast<unsigned char>(x-'A'+'a');
    if(y>='A'&&y<='Z')y=static_cast<unsigned char>(y-'A'+'a');
    if(x!=y)return false;
  }
  return true;
}

/* s in upper (lower) case, in place up to its NUL: ASCII letters only (the
   C locale's toupper/tolower), as the parser keeps a model's names;
   change_case turns the letters from..from+25 into to..to+25 */
inline void change_case(char *s, char from, char to)
{
  for(;*s;s++)
    if(*s>=from&&*s<=from+25)*s=static_cast<char>(*s-from+to);
}
inline void to_upper(char *s) { change_case(s,'a','A'); }
inline void to_lower(char *s) { change_case(s,'A','a'); }
/* s in upper case (ASCII letters only), all of it */
inline std::string upper_case(std::string s)
{
  for(char &c:s)
    if(c>='a'&&c<='z')c=static_cast<char>(c-'a'+'A');
  return s;
}
/* s in lower case (ASCII letters only), all of it */
inline std::string lower_case(std::string s)
{
  to_lower(s.data());
  return s;
}

/* strtok's tokens without writing into the text (the core's one
   tokenizer: the parser, the options, the .ani reader, the fit's lists):
   next(delims) passes over the delimiters, returns the text up to the
   next one and passes over that one too, each call naming its own
   delimiters as strtok's did; nullopt once nothing is left. rest() is
   what follows the last token returned. */
class Tokens {
public:
  explicit Tokens(std::string_view text):rest_(text){}
  std::optional<std::string_view> next(std::string_view delims)
  {
    size_t b=rest_.find_first_not_of(delims);
    if(b==std::string_view::npos){
      rest_={};
      return std::nullopt;
    }
    rest_.remove_prefix(b);
    size_t e=rest_.find_first_of(delims);
    std::string_view tok=rest_.substr(0,e);
    rest_.remove_prefix(e==std::string_view::npos?rest_.size():e+1);
    return tok;
  }
  /* the next token as text, "" when there is none */
  std::string text(std::string_view delims)
  {
    return std::string{next(delims).value_or(std::string_view())};
  }
  std::string_view rest() const { return rest_; }
private:
  std::string_view rest_;
};

/* ---- JSON strings -------------------------------------------------------
   The one place that reads or writes a JSON string's content (the core's
   text is UTF-8): the protocol (json_io.cpp's events and command objects)
   and the files module (xpp_files.cpp's /files listing and its "file"
   command) shared two near-copies of this before, one of them (buf_str)
   treating bytes above 0x7f as Latin-1 instead of decoding UTF-8, which
   sent a model's non-ASCII names back mangled (docs/protocol.md). */

/* Appends `s` (already-decoded UTF-8, as the core's own strings are)
   escaped as a JSON string's *content* to `out` -- no surrounding quotes,
   the caller adds those, since both callers build the quotes into a
   larger buffer around other text. A valid UTF-8 sequence in `s` passes
   through unchanged; '"', '\\' and the control characters are escaped
   (\n \t \r \b \f by name, the rest as \u00XX); a byte that is not part
   of a valid UTF-8 sequence -- and so is not text at all -- is escaped as
   \u00XX too, its own byte value, the same fallback buf_str always used. */
void json_encode_string(std::string &out, std::string_view s);

/* Appends `s` as a whole JSON string, quotes included (json_encode_string
   between them); a null `s` as the empty string "". The data events'
   text (plot_data, auto_data, ani_data, auto_settings) goes through this. */
void json_append_string(std::string &out, const char *s);
void json_append_string(std::string &out, std::string_view s);

/* Decodes the JSON string value at *v (which must point to its opening
   '"'): unescapes \", \\, \/, \n, \t, \r, \b, \f and \uXXXX (a surrogate
   pair combined into one code point, encoded to UTF-8) into `out`
   (cleared first), stopping at the closing '"' or the end of the input.
   At most `max` bytes are appended (SIZE_MAX for no limit); a command
   object's fixed-size destination passes its buffer size, same as the
   old js_string.

   `strict` is xpp_files' stricter rule for a file name: false for any
   other escape, control character (embedded raw or via \u), or unpaired
   surrogate, instead of the loose fallback (an unknown escape copied
   literally, a bad \u treated as U+FFFD) callers reading a command's
   ordinary text fields use. Returns false only when *v is not a '"' or,
   in strict mode, one of those rejects. */
bool json_decode_string(const char *v, std::string &out, size_t max, bool strict);

/* The digit a base64 character stands for (A-Z a-z 0-9 + /), -1 for any
   other: the page's uploads (xpp_files.cpp) and its pictures
   (json_windows.cpp) */
int base64_value(int c) noexcept;

/* A FILE * that closes itself: the read handle, for a helper that takes
   a plain FILE * (the .set, .auto and .ode readers), and for a stream the
   core gets from an API that hands out a FILE * (xpp_files_open, fdopen,
   ...). */
struct FileCloser {
    void operator()(FILE *fp) const noexcept
    {
        if (fp) std::fclose(fp);
    }
};
using UniqueFile = std::unique_ptr<FILE, FileCloser>;

/* path opened for reading, text ("r") or binary ("rb"); empty (false)
   when it cannot be. Lines and numbers are read with LineReader and
   TokenReader below, which open their own. */
inline UniqueFile open_read(const char *path) noexcept { return UniqueFile(xpp_files_open_stream(path, "r")); }
inline UniqueFile open_read_binary(const char *path) noexcept
{
    return UniqueFile(xpp_files_open_stream(path, "rb"));
}

/* path's whole contents, byte for byte, into out; false (out empty) when
   it cannot be opened or read. std::bad_alloc is the caller's to catch. */
inline bool read_bytes(const char *path, std::string &out)
{
    out.clear();
    UniqueFile f = open_read_binary(path);
    if (!f) return false;
    std::array<char, 65536> buf;
    std::size_t n;
    while ((n = std::fread(buf.data(), 1, buf.size(), f.get())) > 0) out.append(buf.data(), n);
    return !std::ferror(f.get());
}

#ifdef XPP_IO_HAVE_STD_FORMAT
/* fprintf's type-checked counterpart: std::format into fp, for a stream
   that stays a FILE * (AUTO's fort.7/8/9, written while it runs, and the
   helpers that are handed one); a Writer prints with its own print().
   Nothing is written to a NULL fp. */
template <class... Args>
void print(FILE *fp, std::format_string<Args...> fmt, Args &&...args) noexcept
{
    const std::string s = xpp::vformat(fmt.get(), std::make_format_args(args...));
    if (fp) std::fwrite(s.data(), 1, s.size(), fp);
}
#endif

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

/* The write handle: Writer(path) replaces path with text, binary(path)
   with bytes, both only at commit(); append(path) adds to its end. */
class Writer {
public:
    Writer() = default;
    explicit Writer(const char *path) noexcept : w_(xpp_writer_open(path)) {}
    static Writer binary(const char *path) noexcept
    {
        Writer w;
        w.w_ = xpp_writer_open_as(path, XPP_WRITE_BINARY);
        return w;
    }
    static Writer append(const char *path) noexcept
    {
        Writer w;
        w.w_ = xpp_writer_open_as(path, XPP_WRITE_APPEND);
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
    void print(std::format_string<Args...> fmt, Args &&...args) noexcept
    {
        xpp::print(file(), fmt, std::forward<Args>(args)...);
    }
#endif
    /* bytes written as they are (a binary Writer's file); false when they
       could not all be */
    bool write(std::string_view bytes) noexcept
    {
        FILE *fp = file();
        return fp && std::fwrite(bytes.data(), 1, bytes.size(), fp) == bytes.size();
    }
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

#endif /* __cplusplus */

#endif
