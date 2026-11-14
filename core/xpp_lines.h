#pragma once

/* core/xpp_lines.cpp: a file of ours as text read by lines, all or nothing (W125): the text is given,
   never a file (the file system is the edge's: xpp_fs.h hands the text in, W731). Splitting text into
   lines and fields, a line's place, and Lines, the reader of one file's lines that throws ReadFailed at
   the first line that is wrong; read_lines turns that into the Result. The kernel's: it includes
   nothing outside it. */

#include <cstddef>
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
