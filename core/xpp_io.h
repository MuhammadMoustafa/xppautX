#pragma once

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
#include <concepts>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "xpp_error.h" /* a file of ours that does not read (read_lines) */
#include "xpp_files.h" /* the files the readers and writers open */
#include "xpp_text.h"  /* the kernel's text formatting, which every file handle below uses */

#include <charconv>
#include <format>
#include <expected>
#include <iterator>
#include <system_error>

namespace xpp {

/* Checked stream positions. Positions returned by tell_stream are opaque:
   seek_stream uses an absolute seek, including on Windows text streams. */
Result<> seek_stream(FILE *file, long position, std::string_view path);
Result<long> tell_stream(FILE *file, std::string_view path);

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
   when it cannot be. Lines are read with LineReader and
   numbers with TokenReader below, which open their own. */
inline UniqueFile open_read(std::string_view path) noexcept { return UniqueFile(xpp::files::open_stream(path, "r")); }
inline UniqueFile open_read_binary(std::string_view path) noexcept
{
    return UniqueFile(xpp::files::open_stream(path, "rb"));
}

/* path's whole contents, byte for byte, into out; false (out empty) when
   it cannot be opened or read. std::bad_alloc is the caller's to catch. */
inline bool read_bytes(FILE *f, std::string &out, std::size_t limit = std::numeric_limits<std::size_t>::max())
{
    out.clear();
    if (!f) return false;
    std::array<char, 65536> buf;
    std::size_t n;
    while ((n = std::fread(buf.data(), 1, buf.size(), f)) > 0) {
        if (n > limit - out.size()) { out.clear(); return false; }
        out.append(buf.data(), n);
    }
    return !std::ferror(f);
}

inline bool read_bytes(std::string_view path, std::string &out, std::size_t limit = std::numeric_limits<std::size_t>::max())
{
    UniqueFile f = open_read_binary(path);
    return read_bytes(f.get(), out, limit);
}

/* fprintf's type-checked counterpart: std::format into fp, for a stream
   that stays a FILE * (autoX's fort.7/8/9, written while it runs, and the
   helpers that are handed one); a Writer prints with its own print().
   Nothing is written to a NULL fp. */
template <class... Args>
void print(FILE *fp, std::format_string<Args...> fmt, Args &&...args) noexcept
{
    const std::string s = xpp::with_format_args([&fmt](std::format_args a) { return xpp::vformat(fmt.get(), a); },
                                               std::forward<Args>(args)...);
    if (fp) std::fwrite(s.data(), 1, s.size(), fp);
}

/* ---- the file handles ---------------------------------------------------
   A line reader and a token reader replace the fgets/fscanf/feof loops
   that either cut a long line at a fixed buffer or (a bare "while
   (!feof(fp))") read the last line twice; a writer replaces
   fopen(path,"w")/fclose with a temp file renamed into place only on
   commit. Move-only handles, two ways each: opened from a path, they own
   the FILE * (closed when the handle goes); attach()ed to a FILE * the
   caller already has, they read through it without owning it, for a
   helper that takes a plain FILE * from elsewhere and must not close it
   (diagram.cpp's load_diagram). A
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
   (also "%le"/"%lf"), read(int&) "%d": each skips leading whitespace,
   then reads the run of non-whitespace characters (the token) and
   converts it with strtod/strtol, so a value read here and
   strtod(token) agree. Every value in the files this reads is
   whitespace/newline separated (never comma or other punctuation), where
   a whitespace-delimited token is exactly what fscanf's own grammar would
   have consumed too. read(long&) is fscanf "%ld" exactly, not a whole
   token: leading whitespace, an optional sign and the digits, and no
   more; what follows stays in the stream. For integer columns printed
   flush against each other, like the "%5ld" label lines of autoX's
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
    static TokenReader attach(FILE *fp) noexcept;
    /* Borrow text for this reader's lifetime, using the same token grammar. */
    static TokenReader of_text(std::string_view text) noexcept;
    /* True when only whitespace remains; leaves the next token untouched. */
    bool at_end() noexcept;
    TokenReader(const TokenReader &) = delete;
    TokenReader &operator=(const TokenReader &) = delete;
    TokenReader(TokenReader &&o) noexcept = default;
    TokenReader &operator=(TokenReader &&o) noexcept = default;
    explicit operator bool() const noexcept { return state_ != nullptr; }
    bool read(double &x) noexcept;
    bool read(int &x) noexcept;
    bool read(long &x) noexcept;
    bool read(long long &x) noexcept;
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
   never through a link: xpp::files::create_new); an unsuccessful open is
   false, reported by its caller. binary(path) is the same in
   binary mode ("wb"), for a byte-for-byte copy whose lines end as the
   source's do on every platform. file() is the FILE * to format into (print(), or
   xpp::print on it); commit() closes the temp file and renames it into
   place (xpp::files::replace_file, the one place that knows POSIX
   rename() from Windows's), returning a Result with the error and the
   original file left untouched on failure; abort() discards the temp file
   without touching path at all. Either one ends the Writer; a second
   commit returns an error. A Writer destroyed without either aborts. */
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
    template <class... Args>
    void print(std::format_string<Args...> fmt, Args &&...args) noexcept
    {
        xpp::print(file(), fmt, std::forward<Args>(args)...);
    }
    /* bytes written as they are (a binary Writer's file); false when they
       could not all be */
    bool write(std::string_view bytes) noexcept
    {
        FILE *fp = file();
        return fp && std::fwrite(bytes.data(), 1, bytes.size(), fp) == bytes.size();
    }
    /* a line and its newline as they are, with no format per line (files of millions of lines) */
    bool write_line(std::string_view line) noexcept { return write(line) && write("\n"); }
    Result<> commit() noexcept;
    const std::string &path() const noexcept { return path_; }
    void abort() noexcept;
    struct State;
    struct Free {
        void operator()(State *s) const noexcept;
    };

private:
    std::string path_;
    std::unique_ptr<State, Free> state_;
};

/* ---- A per-user settings file (keymap.json, preferences.odex) ----------------
   The file `name` in the config folder (files::config_dir), the one way
   the settings files are read and written. `where` is the Error's
   `where`. No config folder (no HOME or APPDATA, or an XPP_CONFIG_DIR that
   is not absolute) is an error naming the variable to set. read: nullopt
   for a file that is not there, else its whole contents, read only inside
   the config folder (never through a link below it), and an error when it
   cannot be read or is longer than `limit` bytes. write: makes the folder,
   then the temp-then-rename write of a Writer. */
Error no_config_folder(std::string_view where, std::string_view name);
Result<std::optional<std::string>> read_config_file(std::string_view where, std::string_view name, std::size_t limit);
Result<> write_config_file(std::string_view where, std::string_view name, std::string_view bytes);
/* The file read and parsed, parse(text, path) giving the Result<T>; T{} (the
   defaults) when the file is not there. */
template <class T, class Parse>
Result<T> load_config_file(std::string_view where, std::string_view name, std::size_t limit, Parse parse)
{
    const Result<std::optional<std::string>> text = read_config_file(where, name, limit);
    if (!text) return std::unexpected(text.error());
    if (!*text) return T{};
    return parse(**text, files::config_path(name));
}

/* ---- A file of ours, loaded: all or nothing, strict (W125) ------------
   Loading one of xppautX's own files is one operation, the same for each
   kind: read the whole file into a value (no change to the Session yet),
   checking every line and every value; then the caller applies that value
   in one step that cannot fail, or, when a line is wrong, applies nothing
   and shows the error, which names the file, the line and the line as
   written (an xpp::Error with its xpp::Place). read_lines below is that
   first half, the one way such a file is read. Every load of one of our
   files goes through it:
     - XPPAUT's set file (.set: File > Import XPPAUT session state, --convert --statefile; an import
       only, read into session_settings.h's Settings) and a parameter file (.par: --convert --statefile,
       the values panel's Load): lunch-new.cpp's read_set and
       read_parameter_file;
     - a session's state (a session.odex of a .snapx, a recording's header) and the preferences
       file: odex.h's read_state_file over the .odex reader's own grammar
       (session_settings.cpp's read_settings, xpp_session.cpp's read_windows, read_marks,
       read_random, read_sliders, nullcline.cpp's read_nullclines, xpp_session_autox.cpp's
       read_settings and read_views, xpp_prefs.cpp's parse_prefs);
     - an initial-conditions file (.ic: --convert --statefile, Initialconds/File, the
       values panel's Load): lunch-new.cpp's read_ic_file;
     - a session's state read with the rest of its members (read_session) before any is applied
       (apply_session): data.npz and frozen.npz are NPZ, checked whole
       (data_formats.cpp) the same way;
     - autoX's session members autox/diagram.csv and autox/solutions.s:
       xpp_session_autox.cpp's parse_diagram_csv;
     - a recording (.recx: Play recording, Open model of one, the command
       line): recx.cpp's read.
   (An internal set, File > Named sets, is a model's line, not a file:
   load_eqn.cpp's extract_action checks every item before it applies one;
   a model's own files, .ode and .odex, load all or nothing through
   xpp::Load, session.h.) What proves it: tests/test_lunch.cpp,
   test_session_autox.cpp and test_recx.cpp, servercheck's
   check_load_all_or_nothing, its session and player checks and
   autoxcheck's sessiondiagram section give each kind a bad value on its last line
   and find the session as it was and the error at that line.
   Foreign formats (XPPAUT's .auto, a data file) are imports, read where
   they are converted. */

/* text's lines, each without its \n or \r\n (a last line needs none; no
   bytes, no lines): the views are into text */
/* Split at each delimiter, preserving empty fields (strict text formats). */
std::vector<std::string_view> split_fields(std::string_view text, char delimiter);
std::vector<std::string_view> split_lines(std::string_view text);

/* line n (from 1) of text, the file named file, as a Place (the line as
   written): a value read whole that a later check refuses, at its line */
Place line_place(std::string file, std::string_view text, int n);

/* what a reader of Lines throws at a line that is wrong: the error, at
   its place; read_lines turns it into its Result */
struct ReadFailed {
    Error error;
};

/* a file of ours as lines, for a reader that takes them one by one */
class Lines {
public:
    /* text's lines (a \n or \r\n ends one; a last line needs none), of
       the file named file, read by where ("set file", ...: the Error's) */
    Lines(std::string where, std::string file, std::string_view text);
    const std::string &file() const noexcept { return file_; }
    /* no line is left */
    bool at_end() const noexcept { return next_ >= lines_.size(); }
    /* the line read last, from 1; 0 before the first */
    int line() const noexcept { return static_cast<int>(next_); }
    /* the next line whole; at the end of the file ReadFailed, what (the
       line due: a value's name, or "") named */
    std::string_view next(std::string_view what = {});
    /* the next line's first word, as a whole number or a number (the set
       format's line: the value, then blanks and its name, which is not
       read unless named); ReadFailed when it is not one, what named */
    int whole(std::string_view what, bool named = false);
    double real(std::string_view what, bool named = false);
    /* the name after the last line's number (blanks trimmed), empty if absent */
    std::string_view value_name() const;
    /* the name after the last line's number must equal what (blanks trimmed) */
    void check_name(std::string_view what) const;
    /* A foreign numeric record: every field present, finite and no extra token. */
    template<class... Values> void numbers(Values &...values)
    {
        auto reader = TokenReader::of_text(next("a numeric record"));
        const auto read = [&](auto &value) {
            if (!reader.read(value) || !std::isfinite(static_cast<double>(value))) return false;
            // Point branch/step numbers are interpreted by sign, including abs().
            if constexpr (std::is_integral_v<std::remove_reference_t<decltype(value)>>)
                if (value == std::numeric_limits<std::remove_reference_t<decltype(value)>>::min()) return false;
            return true;
        };
        if (!(read(values) && ...) || !reader.at_end()) fail("invalid or incomplete numeric record");
    }
    /* the next line starts with "#" (a set file's heading); ReadFailed
       when not */
    void heading(std::string_view heading);
    /* nothing but blank lines is left; ReadFailed at the first line that
       is not blank */
    void end();
    /* line n (from 1) as written; "" past the end */
    std::string_view text(int n) const noexcept;
    /* what is wrong at line n (the line read last), its place named */
    Error error(int n, std::string what) const;
    [[noreturn]] void fail(int n, std::string what) const;
    [[noreturn]] void fail(std::string what) const { fail(line(), std::move(what)); }

private:
    std::string where_, file_, text_;
    std::vector<std::string_view> lines_;
    std::size_t next_ = 0;
};

/* parse(lines) over text, the contents of file (the name its errors
   give): the value it read, or the error at the line it stopped at
   (ReadFailed) */
template <class F>
auto read_lines(std::string where, std::string file, std::string_view text, F &&parse)
    -> Result<std::invoke_result_t<F &, Lines &>>
{
    Lines lines(std::move(where), std::move(file), text);
    try {
        return parse(lines);
    } catch (ReadFailed &e) {
        return std::unexpected<Error>(std::move(e.error));
    }
}

/* the bytes of the file at path, read whole; the error when it cannot be read, or is longer than limit
   (its place the file, no line): a file a user may have been sent is read within a bound */
inline Result<std::string> read_file_bytes(std::string where, std::string_view path, std::size_t limit = std::numeric_limits<std::size_t>::max())
{
    UniqueFile f = open_read_binary(path);
    if (!f) return std::unexpected(files::open_error(std::move(where), path));
    std::string bytes;
    if (read_bytes(f.get(), bytes, limit)) return bytes;
    /* read_bytes stops without an error of the stream when the file is longer than the bound */
    return std::unexpected(files::open_error(std::move(where), path, std::ferror(f.get()) ? "cannot be read" : xpp::format("is longer than {} bytes", limit)));
}

/* the same over the file at path, read whole; the error when it cannot
   be read (its place the file, no line) */
template <class F>
auto read_file_lines(std::string where, std::string_view path, F &&parse, std::size_t limit = std::numeric_limits<std::size_t>::max())
    -> Result<std::invoke_result_t<F &, Lines &>>
{
    Result<std::string> bytes = read_file_bytes(where, path, limit);
    if (!bytes) return std::unexpected(bytes.error());
    return read_lines(std::move(where), std::string(path), *bytes, parse);
}

} // namespace xpp

