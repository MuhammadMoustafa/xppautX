#pragma once
/* The settings syntax (W284, docs/odex.md ".setx"): .odex's options
   syntax, `key: value` entries separated by commas, groups `{ ... }`
   nested, a trailing comma allowed, a bare one-token value or a value in
   double quotes, plus lists `[ a, b ]`. A file of it is the entries of an
   unbraced top-level group. It is the one syntax of every file of ours
   that holds settings: a .setx (parameters, initial values, numerics,
   delays, boundary conditions, graphics), the members of a session file
   (.snapx: model.setx, windows.setx, marks.setx, sliders.setx,
   nullclines.setx, session.setx) and a recording's header (.recx). It is
   read by the .odex reader's own tokenizer and block grammar
   (odex_parse.cpp): there is one reader, not two.

   Reading is all or nothing (W125): parse() reads the whole text into a
   Value tree, a Block reads typed values out of it, every error at the
   file, line and column of the value with the line as written, and
   nothing is applied before the whole file has read. A Block in complete
   mode needs every key it is asked for (a session's members: nothing is
   guessed, no fallback); in partial mode a key that is absent leaves its
   target as it is (a .setx sets what it names). Either way a key nothing
   asks for is an error (done()). Lists are sized from their parsed entries,
   never a declared count; callers can impose a format's own maximum. The
   tokenizer's file budget and max_depth bound allocation and recursion. C++ only. */
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <functional>
#include <type_traits>
#include <utility>

#include "colormap.h"
#include "line_style.h"
#include "xpp_error.h"
#include "xpp_io.h" /* ReadFailed */

namespace xpp::setx {

struct Member;

/* a value of the syntax: a bare word (a number, a name), a string (its
   contents), a group of members or a list of values; where it starts */
struct Value {
    enum class Kind { word, string, group, list };
    Kind kind = Kind::word;
    std::string text;
    std::vector<Member> members;
    std::vector<Value> items;
    int line = 0, col = 0;
};

struct Member {
    std::string key;
    int line = 0, col = 0;
    Value value;
};

/* groups and lists inside each other, at most: the reader's recursion is
   this deep and no deeper */
inline constexpr int max_depth = 8;

/* text, the contents of file, read whole: the unbraced group of its
   entries (a root at line 1), or the error at the token that is wrong.
   where names the reader in the error ("settings", "session", ...). */
Result<Value> read_tree(std::string where, std::string file, std::string_view text);

/* the text a file holds, kept for the errors' lines as written */
class Document {
public:
    Document(std::string where, std::string file, std::string text)
        : where_(std::move(where)), file_(std::move(file)), text_(std::move(text)) {}
    /* the error at line:col of this file; thrown as ReadFailed (xpp_io.h) */
    [[noreturn]] void fail(int line, int col, std::string what) const;
    [[noreturn]] void fail(const Value &v, std::string what) const { fail(v.line, v.col, std::move(what)); }
    [[noreturn]] void fail(const Member &m, std::string what) const { fail(m.line, m.col, std::move(what)); }

private:
    std::string where_, file_, text_;
};

/* a group being read: typed values by key. Each getter returns whether
   the key was present (and read into its target); an absent key is an
   error in complete mode. The value's own error names the key and the
   value written. */
class Block {
public:
    Block(const Document &doc, const Value &group, bool complete, std::string name)
        : doc_(&doc), group_(&group), complete_(complete), name_(std::move(name)), read_(group.members.size(), false) {}

    bool complete() const { return complete_; }
    /* what the group is called in a message ("the numerics", "window 2") */
    const std::string &name() const { return name_; }

    bool has(std::string_view key) const { return find(key) != nullptr; }

    /* a finite number / any number (inf and nan included) / a whole
       number in int's range / true or false / a string / a bare word */
    bool real(std::string_view key, double &v);
    bool number(std::string_view key, double &v);
    bool whole(std::string_view key, int &v);
    bool flag(std::string_view key, bool &v);
    /* text the user wrote (a name, a label, a path, a formula): always a
       string in double quotes, even one word; a word names a choice, a
       number or a name of the model and is never in quotes */
    bool text(std::string_view key, std::string &v);
    /* a colour (colormap.h: #rrggbb, #rrggbbaa, a CSS name or foreground) */
    bool color(std::string_view key, Color &v);
    /* a line style (line_style.h): its keys draw, dash, width, marker and marker_size, each read as
       the others are (all of them in complete mode) */
    void line_style(LineStyle &v);
    bool word(std::string_view key, std::string &v);

    /* a nested group */
    std::optional<Block> group(std::string_view key);
    /* a list of groups, at most max of them */
    bool groups(std::string_view key, std::vector<Block> &out, std::size_t max);
    /* a list of numbers (finite) / words or strings, at most max of them */
    bool reals(std::string_view key, std::vector<double> &out, std::size_t max);
    /* exactly out.size() finite numbers */
    bool reals(std::string_view key, std::span<double> out);
    bool texts(std::string_view key, std::vector<std::string> &out, std::size_t max);
    bool words(std::string_view key, std::vector<std::string> &out, std::size_t max);

    /* the members whose keys the reader does not know beforehand
       (parameters, variables): each marked read; the reader checks the
       key and reads the value with the getters below */
    std::vector<const Member *> members();
    /* a member's value as a number (any, inf and nan included) / a string /
       a word, at its place */
    double number_of(const Member &m) const;
    std::string text_of(const Member &m) const;
    std::string word_of(const Member &m) const;
    /* a reference to a variable or a column: a word, or a string when the
       column is a user's own name with more than letters and digits */
    std::string name_of(const Member &m) const;

    /* the value of key as the value text of an option row (a bare word
       or a string) with the member, for a reader that hands the text
       to another checker */
    const Member *member(std::string_view key);

    /* the error at member m / at this group */
    [[noreturn]] void fail(const Member &m, std::string what) const { doc_->fail(m, std::move(what)); }
    /* the error at a value (an entry of a list) */
    [[noreturn]] void fail(const Value &v, std::string what) const { doc_->fail(v, std::move(what)); }
    [[noreturn]] void fail(std::string what) const { doc_->fail(*group_, std::move(what)); }

    /* every member was read: the first that was not is an error ("`k` is
       not a setting of this group") */
    void done() const;

private:
    const Member *find(std::string_view key) const;
    /* the member key, marked read; null when absent (an error in complete mode) */
    const Member *take(std::string_view key);
    const Value &scalar(const Member &m, std::string_view what) const;
    std::string kind_of(const Member &m, Value::Kind want) const;
    bool read(std::string_view key, std::string &v, Value::Kind want);

    const Document *doc_;
    const Value *group_;
    bool complete_;
    std::string name_;
    std::vector<bool> read_;
};

/* text, the contents of file, read through parse(Block &) over its top-level
   group, the value it makes or the error at the key or value that is wrong
   (ReadFailed turned into its Result, as xpp_io.h's read_lines does for
   lines). complete: every key parse asks for must be there; either way a key
   it does not ask for is refused. */
template <class F>
auto read_block(std::string where, std::string file, std::string_view text, bool complete, F &&parse)
    -> Result<std::invoke_result_t<F &, Block &>>
{
    Result<Value> root = setx::read_tree(where, file, text);
    if (!root) return std::unexpected(std::move(root.error()));
    const Document doc(where, std::move(file), std::string(text));
    try {
        Block block(doc, *root, complete, "the file");
        auto value = parse(block);
        block.done();
        return value;
    } catch (ReadFailed &e) {
        return std::unexpected(std::move(e.error));
    }
}

/* a file being written: entries at the top, groups and lists nested, the
   numbers as to_chars writes them (they read back as the same value),
   words bare and the user's text always in double quotes. A text a line cannot
   hold (a newline) is an error found at result(), not a file that does
   not read. */
class Out {
public:
    void real(std::string_view key, double v);
    void whole(std::string_view key, long long v);
    void flag(std::string_view key, bool v) { word(key, v ? "true" : "false"); }
    /* a word (a choice, a number, a name of the model): bare, never in
       quotes; text the user wrote: always in double quotes, even one word */
    void word(std::string_view key, std::string_view text);
    void text(std::string_view key, std::string_view text);
    /* a colour written as the colour type's text (always double quoted) */
    void color(std::string_view key, const Color &c) { text(key, print_color(c)); }
    /* a line style's five keys */
    void line_style(const LineStyle &v);
    /* a reference to a variable or a column: bare when it is a word, in
       quotes for a user's column named with more (Block::name_of) */
    void name(std::string_view key, std::string_view text);
    void reals(std::string_view key, std::span<const double> v);
    void texts(std::string_view key, std::span<const std::string> v);
    void words(std::string_view key, std::span<const std::string> v);
    void begin(std::string_view key);
    void end();
    /* a list of groups: begin_list, then each as begin_item ... end_item */
    void begin_list(std::string_view key);
    void end_list();
    void begin_item();
    void end_item();
    /* a line of comment at the current depth */
    void comment(std::string_view text);
    /* the text, or the error found while writing it */
    Result<std::string> result(std::string where) const;

private:
    void entry(std::string_view key);
    void put(std::string_view text);
    void quote(std::string_view text);
    std::string out_;
    int depth_ = 0;
    std::optional<std::string> bad_;
};

/* text is a bare word of the syntax: it reads back as the word */
bool is_bare_word(std::string_view text);

} // namespace xpp::setx

