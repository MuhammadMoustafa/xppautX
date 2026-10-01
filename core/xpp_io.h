#ifndef XPP_IO_H
#define XPP_IO_H

/* core/xpp_io.cpp: text formatting (xpp::format, xpp::number) and the file
   handles (xpp::LineReader, xpp::TokenReader, xpp::Writer; issue: W11
   step 3, W7b). C++ in namespace xpp (W109a). The C text-formatting half
   this header used to declare -- xpp_snprintf/xpp_strlcpy/xpp_strlcat,
   XPP_SPRINTF/XPP_STRCPY/XPP_STRCAT, for a C file's sprintf/strcpy/
   vsprintf into a fixed buffer, which write with no length and silently
   overflow it -- was retired at W48, and the C API under the handles
   (xpp_line_reader_*, xpp_token_reader_*, xpp_writer_*) at W109a, once
   nothing but the handles called it. New code formats into a std::string
   with xpp::format below (xpp::number for a double), or reads/writes
   through a fixed-size std::array/std::vector directly.

   xpp::format takes a std::format_string, checked against the argument
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
#include <vector>

#include "xpp_files.h" /* the files the readers and writers open */

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

/* text without the white space (isspace's: blank, tab, CR, LF, VT, FF) around
   it: a typed value is read after this, so " 5 " is 5 and "5 x" is still
   refused; the one trim of the core. A view into `text`: copy it before
   `text` goes. */
inline std::string_view trim_blanks(std::string_view text)
{
  const std::string_view blanks=" \t\r\n\v\f";
  const size_t b=text.find_first_not_of(blanks);
  if(b==std::string_view::npos)return {};
  return text.substr(b,text.find_last_not_of(blanks)-b+1);
}

/* text, all of it, as a decimal number (strtod's grammar in the C locale:
   inf and nan too, no hexadecimal, no leading space); false when the text
   is not one number or is out of double's range (a subnormal is in it). Not std::from_chars:
   macOS's libc++ has no floating-point from_chars before macOS 26, and the
   release runs on 13.3 */
bool parse_number(std::string_view text, double &value);
/* text, all of it, as a whole number in int's range (an optional '-',
   then digits); false otherwise, value untouched */
bool parse_int(std::string_view text, int &value);

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

/* c starts a word (a letter or '_'), c is in one (a digit too): a model's
   names are words */
inline bool is_word_start(char c)
{
  return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_';
}
inline bool is_word_char(char c)
{
  return is_word_start(c)||(c>='0'&&c<='9');
}
/* the words of text in order, each as written: runs of word characters
   that start with a letter or '_' (a formula's names and calls; the e of
   a number's exponent, 1e-3, is none) */
inline std::vector<std::string> words_of(std::string_view text)
{
  std::vector<std::string> out;
  size_t i=0;
  while(i<text.size()){
    if(is_word_start(text[i])&&(i==0||!is_word_char(text[i-1]))){
      size_t b=i;
      while(i<text.size()&&is_word_char(text[i]))i++;
      out.emplace_back(text.substr(b,i-b));
    }
    else
      i++;
  }
  return out;
}

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

/* Base64 (RFC 4648, padded): the one encoder and decoder. base64_append
   (below) encodes bytes all at once (the series event's float32 columns,
   series_enc.cpp; a file's bytes, xpp_files.cpp). Base64Decoder takes digits one at a time
   ('=' padding too; any other character makes feed() false) and appends
   the bytes to `out` (the page's uploads, xpp_files.cpp, a recording's
   binary files, recx.cpp); finish() writes the last bytes, padded or not, and is false for a
   dangling digit or too much padding. */
class Base64Decoder {
public:
    explicit Base64Decoder(std::string &out) : out_(out) {}
    bool feed(char c);
    bool finish();
private:
    std::string &out_;
    std::array<int, 4> q_{};
    int n_ = 0, pad_ = 0;
};
/* bytes as base64, appended to out */
void base64_append(std::string &out, std::string_view bytes);
/* the base64 text decoded, appended to out: false when it is not base64 */
bool base64_decode_append(std::string &out, std::string_view text);

/* A FILE * that closes itself: the read handle, for a helper that takes
   a plain FILE * (the .set, .auto and .ode readers), and for a stream the
   core gets from an API that hands out a FILE * (xpp::files::open, fdopen,
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
inline UniqueFile open_read(std::string_view path) noexcept { return UniqueFile(xpp::files::open_stream(path, "r")); }
inline UniqueFile open_read_binary(std::string_view path) noexcept
{
    return UniqueFile(xpp::files::open_stream(path, "rb"));
}

/* the lines of fp before its position, the one it is in counting (a last
   line without its newline too): the line a reader just read, for a
   message saying where a file is wrong; the position is kept. Exact for a
   file opened binary (open_read_binary: LineReader takes \r\n as well);
   on Windows a text stream's position is not a byte count. */
int lines_read(FILE *fp);

/* path's whole contents, byte for byte, into out; false (out empty) when
   it cannot be opened or read. std::bad_alloc is the caller's to catch. */
inline bool read_bytes(std::string_view path, std::string &out)
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

/* ---- the file handles ---------------------------------------------------
   A line reader and a token reader replace the fgets/fscanf/feof loops
   that either cut a long line at a fixed buffer or (a bare "while
   (!feof(fp))") read the last line twice; a writer replaces
   fopen(path,"w")/fclose with a temp file renamed into place only on
   commit. Move-only handles, two ways each: opened from a path, they own
   the FILE * (closed when the handle goes); attach()ed to a FILE * the
   caller already has, they read through it without owning it, for a
   helper that takes a plain FILE * from elsewhere and must not close it
   (lunch-new.cpp's io_int/io_double, diagram.cpp's load_diagram). A
   handle that could not be made is false. */

/* Lines of any length, CR/LF tolerant: next() gives the next line without
   its terminator (a bare \n, a \r\n, or a final line with none at all),
   nullopt at the real end of the file. No size limit: a line longer than
   any fixed buffer comes back whole. The view is the reader's, valid
   until the next next() or until the reader is closed or destroyed. */
class LineReader {
public:
    LineReader() noexcept = default;
    /* fopen(path, "r") through xpp::files::open_stream */
    explicit LineReader(std::string_view path) noexcept;
    /* text's lines, as a file of those bytes gives them (a model's saved
       file, model_files.h) */
    static LineReader of_text(std::string text) noexcept;
    /* reads fp, never closes it */
    static LineReader attach(FILE *fp) noexcept;
    LineReader(const LineReader &) = delete;
    LineReader &operator=(const LineReader &) = delete;
    LineReader(LineReader &&o) noexcept = default;
    LineReader &operator=(LineReader &&o) noexcept = default;
    explicit operator bool() const noexcept { return state_ != nullptr; }
    std::optional<std::string_view> next();
    /* the line n lines on (n from 1: the next one), the lines before it
       read; "" when there are fewer (an error's source line, xpp_error.h) */
    std::string line(int n);
    /* what a reader reads, and Free, which deletes it where it is
       complete (xpp_io.cpp) */
    struct State;
    struct Free {
        void operator()(State *s) const noexcept;
    };

private:
    std::unique_ptr<State, Free> state_;
};

/* Whitespace-separated tokens, the fscanf "%lg"/"%g"/"%d" equivalents, the
   conversion picked by the type read into: read(double&) is fscanf "%lg"
   (also "%le"/"%lf"), read(float&) "%g" (strtof, rounded straight to
   float as fscanf does), read(int&) "%d": each skips leading whitespace,
   then reads the run of non-whitespace characters (the token) and
   converts it with strtod/strtof/strtol, so a value read here and
   strtod(token) agree. Every value in the files this reads is
   whitespace/newline separated (never comma or other punctuation), where
   a whitespace-delimited token is exactly what fscanf's own grammar would
   have consumed too. read(long&) is fscanf "%ld" exactly, not a whole
   token: leading whitespace, an optional sign and the digits, and no
   more; what follows stays in the stream. For integer columns printed
   flush against each other, like the "%5ld" label lines of AUTO's
   fort.8/.s files, where "    2-1234" is 2 then -1234 and a
   whitespace-delimited token would swallow both. Each read is true on
   success, false at the end of the file or where fscanf would not have
   returned 1. skip_line() passes over the rest of the current line, its
   \n included (the "go to the end of the line" after reading a line's
   leading fields): true when a \n ended it, false when the end of the
   file came first. */
class TokenReader {
public:
    TokenReader() noexcept = default;
    explicit TokenReader(std::string_view path) noexcept;
    static TokenReader attach(FILE *fp) noexcept;
    TokenReader(const TokenReader &) = delete;
    TokenReader &operator=(const TokenReader &) = delete;
    TokenReader(TokenReader &&o) noexcept = default;
    TokenReader &operator=(TokenReader &&o) noexcept = default;
    explicit operator bool() const noexcept { return state_ != nullptr; }
    bool read(double &x) noexcept;
    bool read(float &x) noexcept;
    bool read(int &x) noexcept;
    bool read(long &x) noexcept;
    bool skip_line() noexcept;
    void close() noexcept { state_.reset(); }
    struct State;
    struct Free {
        void operator()(State *s) const noexcept;
    };

private:
    std::unique_ptr<State, Free> state_;
};

/* The write handle. Writer(path) creates a hidden temp file next to path
   ("w" text mode, like the fopen(path,"w") it replaces, so a Windows
   build writes CRLF line ends exactly as before; created exclusively,
   never through a link: xpp::files::create_new) and logs an ERROR itself
   when that fails (most callers' own "cannot open file" message already
   covers the case, this is for the rest). binary(path) is the same in
   binary mode ("wb"), for a byte-for-byte copy whose lines end as the
   source's do on every platform. file() is the FILE * to format into (print(), or
   xpp::print on it); commit() closes the temp file and renames it into
   place (xpp::files::replace_file, the one place that knows POSIX
   rename() from Windows's), false (an ERROR logged, the original file
   left untouched) on failure; abort() closes and discards the temp file
   without touching path at all. Either one ends the Writer (a second is a
   no-op); a Writer destroyed without either aborts. */
class Writer {
public:
    Writer() noexcept = default;
    explicit Writer(std::string_view path) noexcept;
    static Writer binary(std::string_view path) noexcept;
    ~Writer();
    Writer(const Writer &) = delete;
    Writer &operator=(const Writer &) = delete;
    Writer(Writer &&o) noexcept = default;
    Writer &operator=(Writer &&o) noexcept;
    explicit operator bool() const noexcept { return state_ != nullptr; }
    FILE *file() const noexcept;
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
    bool commit() noexcept;
    void abort() noexcept;
    struct State;
    struct Free {
        void operator()(State *s) const noexcept;
    };

private:
    std::unique_ptr<State, Free> state_;
};

} // namespace xpp

#endif
