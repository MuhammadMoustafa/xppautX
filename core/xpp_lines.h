#pragma once

/* core/xpp_lines.cpp: a file of ours as text read by lines, all or nothing (W125): the text is given,
   never a file (the file system is the edge's: xpp_fs.h hands the text in, W731). Splitting text into
   lines and fields, a line's place, and Lines, the reader of one file's lines that throws ReadFailed at
   the first line that is wrong; read_lines turns that into the Result. The kernel's: it includes
   nothing outside it. */

#include <cstddef>
#include <cstdio>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "xpp_error.h"
#include "xpp_text.h"

namespace xpp {

/* text's lines, each without its \n or \r\n (a last line needs none; no
   bytes, no lines): the views are into text */
/* Split at each delimiter, preserving empty fields (strict text formats). */
std::vector<std::string_view> split_fields(std::string_view text, char delimiter);
std::vector<std::string_view> split_lines(std::string_view text);

/* line n (from 1) of text without its \n or \r\n, a view into text; "" past the end */
std::string_view nth_line(std::string_view text, int n);

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

/* Whitespace-separated tokens of text, the fscanf "%lg"/"%d" equivalents, the conversion picked by
   the type read into: read(double&) is fscanf "%lg" (also "%le"/"%lf"), read(int&) "%d": each skips
   leading whitespace, then reads the run of non-whitespace characters (the token) and converts it
   with strtod/strtol, so a value read here and strtod(token) agree. Every value in the text this
   reads is whitespace/newline separated (never comma or other punctuation), where a
   whitespace-delimited token is exactly what fscanf's own grammar would have consumed too.
   read(long&) is fscanf "%ld" exactly, not a whole token: leading whitespace, an optional sign and
   the digits, and no more; what follows stays. For integer columns printed flush against each
   other, like the "%5ld" label lines of autoX's fort.8/.s text, where "    2-1234" is 2 then -1234
   and a whitespace-delimited token would swallow both. Each read is true on success, false at the
   end of the text or where fscanf would not have returned 1. skip_line() passes over the rest of
   the current line, its \n included: true when a \n ended it, false when the text ended first. The
   text is borrowed: it must outlive the reader. (A file's text is read whole by the file system,
   xpp_fs.h; the kernel never opens one.) */
class TokenReader {
public:
    TokenReader() noexcept = default;
    /* moved, a reader of pieces keeps viewing its own piece */
    TokenReader(TokenReader &&o) noexcept { *this = std::move(o); }
    TokenReader &operator=(TokenReader &&o) noexcept
    {
        const bool in_piece = !o.piece_.empty();
        next_ = std::move(o.next_);
        piece_ = std::move(o.piece_);
        before_ = o.before_;
        pos_ = o.pos_;
        text_ = in_piece ? std::string_view(piece_) : o.text_;
        return *this;
    }
    static TokenReader of_text(std::string_view text) noexcept
    {
        TokenReader t;
        t.text_ = text;
        return t;
    }
    /* text that comes in pieces, each next() call giving the following one ("" at the end): a file read by range, so a solution
       of any length is parsed with one piece in memory. A token or a number may straddle two pieces. */
    static TokenReader of_pieces(std::function<std::string()> next) { 
        TokenReader t;
        t.next_ = std::move(next);
        return t;
    }
    /* true when only whitespace remains; leaves the next token untouched */
    bool at_end() noexcept;
    bool read(double &x) noexcept;
    bool read(int &x) noexcept;
    bool read(long &x) noexcept;
    bool read(long long &x) noexcept;
    bool skip_line() noexcept;
    /* the byte offset of the next character: how much of the text was read (of a reader of pieces, from its first) */
    std::size_t position() const noexcept { return before_ + pos_; }

private:
    int get() noexcept
    {
        if (pos_ >= text_.size() && !refill()) return EOF;
        return static_cast<unsigned char>(text_[pos_++]);
    }
    /* the next piece in place of the text read (a reader of pieces only): false at the end */
    bool refill();
    /* c, the character just read, put back (one at a time) */
    void unget(int c) noexcept
    {
        if (c != EOF) --pos_;
    }
    bool read_token(std::string &token);
    template <class T, class Convert> bool read_number(T &out, Convert strto);
    std::string_view text_;
    std::size_t pos_ = 0;
    std::function<std::string()> next_; /* the pieces of a reader of pieces; empty for a text */
    std::string piece_;                 /* the piece text_ views then */
    std::size_t before_ = 0;            /* the bytes of the pieces before it */
};

/* Text that arrives in pieces (a file read in chunks), cut into lines: line(raw) is called for each line the text so far
   completes, raw without its \n (a \r stays), and finish() hands over the unfinished last line, if any, as one that no \n ended.
   Memory is the longest line. A file's pass over every line without holding the file. */
class LineFeeder {
public:
    template <class F> void feed(std::string_view chunk, F &&line)
    {
        std::size_t at = 0;
        for (std::size_t nl; (nl = chunk.find('\n', at)) != std::string_view::npos; at = nl + 1) {
            if (tail_.empty()) {
                line(chunk.substr(at, nl - at), true);
            } else {
                tail_.append(chunk.substr(at, nl - at));
                line(std::string_view(tail_), true);
                tail_.clear();
            }
        }
        tail_.append(chunk.substr(at));
    }
    template <class F> void finish(F &&line)
    {
        if (!tail_.empty()) line(std::string_view(tail_), false);
        tail_.clear();
    }

private:
    std::string tail_;
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


} // namespace xpp
