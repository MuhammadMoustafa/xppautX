/* .odex's tokenizer and grammar (odex.h, docs/odex.md "Grammar"): a
   model's text to its statements. The tokenizer never strips white space
   first (a word ends where a word ends, whatever the spacing); comments,
   '#' to the end of the line and nested / * ... * / blocks, go with the
   white space. Every problem is an Error at a line and column; nothing is
   dropped, cut or skipped. */
#include "odex.h"
#include "expr.h"
#include "snapx.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "model_files.h"
#include "model_options.h"
#include "load_eqn.h"
#include "setx.h"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xpp::odex {

namespace {

/* the words docs/odex.md reserves: the statement keywords (and solv and
   only, the .ode statements .odex keeps with their own spelling), the
   operator words, volterra, near (W78: near(a,b[,tol=]), a translation,
   never an expression-engine built-in), the built-in functions (every
   one the expression engine has, lgamma, poisson and besselis included)
   and constants (t, pi, and the animator's mouse_x ... mouse_vy); const
   and by (W80: const n = 20, and a range's step, for j in 1..n by 2; for
   and in are words only where a range starts, after a statement) */
constexpr auto reserved_words = std::to_array<std::string_view>({
  "par", "init", "aux", "fun", "let", "return", "if", "then", "else", "set",
  "table", "markov", "wiener", "event", "boundary", "network", "import",
  "solv", "only", "history", "const", "by", "options",
  "and", "or", "not", "mod", "volterra", "near", "of",
  "t", "pi", "mouse_x", "mouse_y", "mouse_vx", "mouse_vy",
});

bool is_letter(char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_digit(char c)
{
  return c >= '0' && c <= '9';
}

bool is_name_char(char c)
{
  return is_letter(c) || is_digit(c) || c == '_';
}

struct Token {
  enum class Kind { Name, Number, String, Punct, End };
  Kind kind = Kind::End;
  /* a name (without its '), a number as written, a string's contents,
     a symbol */
  std::string text;
  double value = 0;
  bool primed = false;
  /* white space or a comment comes before it */
  bool space_before = false;
  Pos pos;
  /* where it is in the text */
  size_t begin = 0, end = 0;
};

/* The token cap budgets objects at 80 bytes each on the supported 64-bit targets. */
constexpr std::size_t max_token_object_bytes = 80;
static_assert(sizeof(Token) <= max_token_object_bytes);

/* a number's text to its value, false when the text is not all of one */
bool read_number(std::string_view text, double &value)
{
  if (text.empty()) return false;
  if (text[0] == '+') text.remove_prefix(1);
  return xpp::parse_number(text, value);
}

class Lexer {
public:
  Lexer(std::string_view src, int file, const std::string &name) : s_(src), file_(file), name_(name) {}

  std::vector<Token> run()
  {
    std::vector<Token> out;
    for (;;) {
      bool space = skip_blank();
      if (out.size() >= max_file_tokens)
        fail(here(), xpp::format("the file has more than {} tokens (including its end)", max_file_tokens));
      Token t;
      t.space_before = space || out.empty();
      t.pos = here();
      t.begin = i_;
      if (i_ >= s_.size()) {
        t.kind = Token::Kind::End;
        t.end = i_;
        out.push_back(t);
        return out;
      }
      char c = s_[i_];
      if (is_letter(c)) {
        size_t b = i_;
        while (i_ < s_.size() && is_name_char(s_[i_])) i_++;
        t.kind = Token::Kind::Name;
        t.text = std::string(s_.substr(b, i_ - b));
        if (i_ < s_.size() && s_[i_] == '\'') {
          t.primed = true;
          i_++;
        }
      } else if (is_digit(c) || (c == '.' && i_ + 1 < s_.size() && is_digit(s_[i_ + 1]))) {
        size_t b = i_;
        while (i_ < s_.size() && is_digit(s_[i_])) i_++;
        /* 1..n is 1, then .. (a range), never the number 1. */
        if (i_ < s_.size() && s_[i_] == '.' && s_.substr(i_, 2) != "..") {
          i_++;
          while (i_ < s_.size() && is_digit(s_[i_])) i_++;
        }
        /* an exponent only with its digits: 2e is 2 then the name e */
        if (i_ < s_.size() && (s_[i_] == 'e' || s_[i_] == 'E')) {
          size_t k = i_ + 1;
          if (k < s_.size() && (s_[k] == '+' || s_[k] == '-')) k++;
          if (k < s_.size() && is_digit(s_[k])) {
            i_ = k;
            while (i_ < s_.size() && is_digit(s_[i_])) i_++;
          }
        }
        t.kind = Token::Kind::Number;
        t.text = std::string(s_.substr(b, i_ - b));
        if (!read_number(t.text, t.value)) fail(t.pos, xpp::format("`{}` is not a number", t.text));
      } else if (c == '"') {
        /* in a string only \" and \\ are escapes: a path, a narration */
        i_++;
        std::string text;
        while (i_ < s_.size() && s_[i_] != '"' && s_[i_] != '\n') {
          if (s_[i_] == '\\') {
            const char escaped = i_ + 1 < s_.size() ? s_[i_ + 1] : '\0';
            if (escaped != '"' && escaped != '\\')
              fail(here(), "a backslash in a string is written \\\\ (the only escapes are \\\" and \\\\)");
            text += escaped;
            i_ += 2;
          } else {
            text += s_[i_++];
          }
        }
        if (i_ >= s_.size() || s_[i_] != '"') fail(t.pos, "this string is never closed on its line");
        t.kind = Token::Kind::String;
        t.text = std::move(text);
        i_++;
      } else {
        static constexpr std::array<std::string_view, 5> two = {"<=", ">=", "==", "!=", ".."};
        t.kind = Token::Kind::Punct;
        t.text = std::string(1, c);
        for (std::string_view p : two)
          if (s_.substr(i_, 2) == p) t.text = std::string(p);
        i_ += t.text.size();
      }
      t.end = i_;
      out.push_back(std::move(t));
    }
  }

private:
  Pos here() const { return Pos{file_, line_, static_cast<int>(i_ - line_start_) + 1}; }

  void newline()
  {
    line_++;
    line_start_ = i_;
  }

  [[noreturn]] void fail(Pos p, std::string msg) const
  {
    Place place = line_place(name_, s_, p.line);
    place.col = p.col;
    throw Error{"model", std::move(msg), std::move(place)};
  }

  /* passes white space and comments; true when there were some */
  bool skip_blank()
  {
    bool any = false;
    while (i_ < s_.size()) {
      char c = s_[i_];
      if (c == '\n') {
        i_++;
        newline();
      } else if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') {
        i_++;
      } else if (c == '#') {
        while (i_ < s_.size() && s_[i_] != '\n') i_++;
      } else if (s_.substr(i_, 2) == "/*") {
        skip_block_comment();
      } else {
        break;
      }
      any = true;
    }
    return any;
  }

  /* a / * ... * / comment, which may hold others */
  void skip_block_comment()
  {
    std::vector<Pos> open;
    while (i_ < s_.size()) {
      if (s_.substr(i_, 2) == "/*") {
        open.push_back(here());
        i_ += 2;
      } else if (s_.substr(i_, 2) == "*/") {
        i_ += 2;
        open.pop_back();
        if (open.empty()) return;
      } else {
        if (s_[i_] == '\n') {
          i_++;
          newline();
        } else {
          i_++;
        }
      }
    }
    fail(open.back(), "this /* comment is never closed");
  }

  std::string_view s_;
  int file_;
  const std::string &name_;
  size_t i_ = 0;
  int line_ = 1;
  size_t line_start_ = 0;
};

/* how a token reads in a message */
std::string describe(const Token &t)
{
  switch (t.kind) {
  case Token::Kind::End: return "the end of the file";
  case Token::Kind::String: return "a string";
  case Token::Kind::Number: return xpp::format("`{}`", t.text);
  case Token::Kind::Name: return xpp::format("`{}{}`", t.text, t.primed ? "'" : "");
  case Token::Kind::Punct: break;
  }
  return xpp::format("`{}`", t.text);
}

class Parser {
public:
  Parser(xpp::Model *model, Parsed &out, int file, std::string_view src, int depth)
      : model_(model), out_(out), file_(file), src_(src), depth_(depth)
  {
    toks_ = Lexer(src, file, out.files[file]).run();
  }

  void model()
  {
    while (!at_end()) statement();
  }

  /* a settings file (setx.h): the entries of an unbraced group to the end
     of the text, read with the block grammar the options use */
  setx::Value settings_file()
  {
    setx::Value root;
    root.kind = setx::Value::Kind::group;
    root.line = root.col = 1;
    std::map<std::string, int> keys;
    while (!at_end()) {
      const Token &key = name_token("a key");
      expect_punct(":", xpp::format("after {}", key.text));
      setting_entry(root, keys, 0, key);
      if (at_end()) break;
      expect_punct(",", "between the settings");
    }
    return root;
  }

private:
  /* ---- tokens ---- */
  const Token &peek(size_t k = 0) const
  {
    size_t j = i_ + k;
    return j < toks_.size() ? toks_[j] : toks_.back();
  }
  bool at_end() const { return peek().kind == Token::Kind::End; }
  bool at_punct(std::string_view p, size_t k = 0) const
  {
    const Token &t = peek(k);
    return t.kind == Token::Kind::Punct && t.text == p;
  }
  bool at_word(std::string_view w, size_t k = 0) const
  {
    const Token &t = peek(k);
    return t.kind == Token::Kind::Name && !t.primed && t.text == w;
  }
  const Token &take()
  {
    const Token &t = peek();
    if (i_ < toks_.size() - 1) i_++;
    last_line_ = t.pos.line;
    return t;
  }
  [[noreturn]] void fail(Pos p, std::string msg) const
  {
    throw error_at(out_.files[p.file], p, std::move(msg));
  }
  void expect_punct(std::string_view p, std::string_view what)
  {
    if (!at_punct(p)) fail(peek().pos, xpp::format("expected `{}` {}, found {}", p, what, describe(peek())));
    take();
  }
  void expect_word(std::string_view w, std::string_view what)
  {
    if (!at_word(w)) fail(peek().pos, xpp::format("expected `{}` {}, found {}", w, what, describe(peek())));
    take();
  }

  /* a name being declared: never a reserved word, never primed */
  const Token &declared_name(std::string_view what)
  {
    const Token &t = peek();
    if (t.kind != Token::Kind::Name || t.primed)
      fail(t.pos, xpp::format("expected {}, found {}", what, describe(t)));
    if (is_reserved(t.text))
      fail(t.pos, xpp::format("`{}` is a reserved word and cannot be a name", t.text));
    return take();
  }

  /* a declared name's index, [expr], when one follows */
  std::optional<Expr> name_index()
  {
    if (!at_punct("[")) return std::nullopt;
    take();
    Expr e = expr();
    expect_punct("]", "to close the index");
    return e;
  }

  /* a range starts here: `for`, on the line the statement ended on */
  bool at_range() const { return at_word("for") && peek().pos.line == last_line_; }

  /* for index in lo..hi [by step] (docs/odex.md "Arrays") */
  Range range()
  {
    Range r;
    r.pos = take().pos;
    const Token &n = declared_name("the range's index");
    r.index = n.text;
    r.index_pos = n.pos;
    expect_word("in", xpp::format("after for {}", r.index));
    r.lo = expr();
    expect_punct("..", "between the range's first and last index (a..b)");
    r.hi = expr();
    if (at_punct(".."))
      fail(peek().pos, "a range is a..b, its step written `by k` (not a..b..k)");
    if (at_word("by")) {
      take();
      r.step = expr();
    } else {
      r.step.kind = Expr::Kind::Number;
      r.step.pos = r.pos;
      r.step.value = 1;
      r.step.text = "1";
    }
    if (at_word("step") && peek().pos.line == last_line_)
      fail(peek().pos, "a range's step is written `by k`");
    if (at_range()) fail(peek().pos, "a statement takes one range");
    return r;
  }

  /* a list statement ends at the end of its line: another item on the
     same line needs its comma (par a=1 b=2 would otherwise read b=2 as
     a statement of its own) */
  void end_of_list(std::string_view what)
  {
    if (at_range()) return;
    if (!at_end() && peek().pos.line == last_line_)
      fail(peek().pos, xpp::format("expected `,` between the items of {}, found {}", what, describe(peek())));
  }

  /* ---- statements ---- */
  void statement()
  {
    const Token &t = peek();
    Statement s;
    s.pos = t.pos;
    if (t.kind == Token::Kind::String) {
      s.kind = Statement::Kind::Comment;
      s.text = take().text;
      push(std::move(s));
      return;
    }
    if (t.kind == Token::Kind::Punct && t.text == "!")
      fail(t.pos, "`!d = expr` is .ode's: in .odex write `d = expr` (a formula of parameters is worked out "
                  "when they change)");
    if (t.kind == Token::Kind::Number && t.text == "0" && at_punct("=", 1)) {
      take();
      take();
      s.kind = Statement::Kind::Dae;
      s.expr = expr();
      push(std::move(s));
      return;
    }
    if (t.kind != Token::Kind::Name)
      fail(t.pos, xpp::format("a statement cannot start with {}", describe(t)));
    if (t.primed) {
      if (is_reserved(t.text)) fail(t.pos, xpp::format("`{}` is a reserved word and cannot be a name", t.text));
      s.kind = Statement::Kind::Ode;
      s.name = t.text;
      s.name_pos = t.pos;
      take();
      expect_punct("=", xpp::format("after {}'", s.name));
      s.expr = expr();
      push(std::move(s));
      return;
    }
    const std::string word = t.text;
    if (word == "par" || word == "init" || word == "aux" || word == "history" || word == "const") {
      take();
      s.kind = word == "par" ? Statement::Kind::Par : word == "init" ? Statement::Kind::Init
             : word == "aux" ? Statement::Kind::Aux : word == "const" ? Statement::Kind::Const
             : Statement::Kind::History;
      bindings(s, word);
      push(std::move(s));
    } else if (word == "fun") {
      take();
      function(s);
    } else if (word == "options") {
      take();
      options(s);
    } else if (word == "set") {
      take();
      set_block(s);
    } else if (word == "table") {
      take();
      table(s);
    } else if (word == "markov") {
      take();
      markov(s);
    } else if (word == "wiener" || word == "only") {
      take();
      s.kind = word == "wiener" ? Statement::Kind::Wiener : Statement::Kind::Only;
      bool indexed = false;
      for (;;) {
        const Token &n = word == "wiener" ? declared_name("a name") : name_token("a name");
        s.names.push_back(n.text);
        s.name_positions.push_back(n.pos);
        s.name_indices.push_back(name_index());
        indexed = indexed || s.name_indices.back();
        if (!at_punct(",")) break;
        take();
      }
      if (!indexed) s.name_indices.clear();
      end_of_list(word);
      push(std::move(s));
    } else if (word == "event") {
      take();
      event(s);
    } else if (word == "boundary") {
      take();
      s.kind = Statement::Kind::Boundary;
      s.expr = expr();
      push(std::move(s));
    } else if (word == "network") {
      take();
      network(s);
    } else if (word == "solv") {
      take();
      s.kind = Statement::Kind::Solv;
      name_of(s, "the algebraic variable's name");
      expect_punct("=", xpp::format("after {} (its first guess)", s.name));
      s.expr = expr();
      push(std::move(s));
    } else if (word == "import") {
      take();
      import(s, false);
    } else if (word == "from" && peek(1).kind == Token::Kind::String) {
      take();
      import(s, true);
    } else if (is_reserved(word)) {
      fail(t.pos, xpp::format("`{}` is a reserved word and cannot start a statement", word));
    } else {
      /* x = ..., x(t) = ..., or an array's element: x[e]' = ..., x[e] =
         ..., x[e](t) = ... */
      name_of(s, "a name");
      if (s.name_index && at_punct("'") && !peek().space_before) {
        take();
        s.kind = Statement::Kind::Ode;
        expect_punct("=", xpp::format("after {}[...]'", s.name));
      } else if (at_punct("=")) {
        take();
        s.kind = Statement::Kind::Fixed;
      } else if (at_punct("(") && at_word("t", 1) && at_punct(")", 2) && at_punct("=", 3)) {
        take();
        take();
        take();
        take();
        s.kind = Statement::Kind::Volterra;
      } else {
        fail(peek().pos, xpp::format("expected `'`, `=` or `(t) =` after `{}`, found {}", word, describe(peek())));
      }
      s.expr = expr();
      push(std::move(s));
    }
  }

  /* s, its range with it when one follows */
  void push(Statement s)
  {
    if (at_range()) s.range = range();
    out_.statements.push_back(std::move(s));
  }

  const Token &name_token(std::string_view what)
  {
    const Token &t = peek();
    if (t.kind != Token::Kind::Name || t.primed) fail(t.pos, xpp::format("expected {}, found {}", what, describe(t)));
    return take();
  }

  /* a declared name, and its index when it is an array's element */
  void name_of(Statement &s, std::string_view what)
  {
    const Token &n = declared_name(what);
    s.name = n.text;
    s.name_pos = n.pos;
    s.name_index = name_index();
  }

  Binding binding(std::string_view what)
  {
    Binding b;
    const Token &n = declared_name(what);
    b.name = n.text;
    b.pos = n.pos;
    b.index = name_index();
    expect_punct("=", xpp::format("after {}", b.name));
    b.value = expr();
    return b;
  }

  void bindings(Statement &s, std::string_view what)
  {
    for (;;) {
      s.bindings.push_back(binding("a name"));
      if (!at_punct(",")) break;
      take();
    }
    end_of_list(what);
  }

  /* the text of the tokens from here that touch each other (no white
     space between), up to a comma or a closing brace: a bare value */
  std::string raw_word(Pos &at)
  {
    at = peek().pos;
    if (at_end() || at_punct(",") || at_punct("}") || at_punct("]")) return std::string();
    size_t b = peek().begin, e = take().end;
    while (!at_end() && !peek().space_before && !at_punct(",") && !at_punct("}") && !at_punct("]")) e = take().end;
    return std::string(src_.substr(b, e - b));
  }

  /* { key: value, key: value, ... }: each key is read and its value
     handed to entry, which reads it; a trailing comma is allowed. The
     keys are read only here, so none is a reserved word. */
  template <class F>
  void entries(std::string_view what, F entry)
  {
    const Pos open = peek().pos;
    expect_punct("{", xpp::format("to open {}", what));
    while (!at_punct("}")) {
      if (at_end()) fail(peek().pos, xpp::format("missing `}}` for the block opened at {}:{}", open.line, open.col));
      const Token &key = name_token("a key");
      expect_punct(":", xpp::format("after {}", key.text));
      entry(key);
      if (!at_punct(",")) break;
      take();
    }
    expect_punct("}", "to close the block");
  }

  /* the entry of group whose key and colon were read: its value */
  void setting_entry(setx::Value &group, std::map<std::string, int> &keys, int depth, const Token &key)
  {
    const auto [first, fresh] = keys.emplace(key.text, key.pos.line);
    if (!fresh) fail(key.pos, xpp::format("`{}` is given twice (the first is at line {})", key.text, first->second));
    setx::Member m;
    m.key = key.text;
    m.line = key.pos.line;
    m.col = key.pos.col;
    m.value = setting_value(depth);
    group.members.push_back(std::move(m));
  }

  /* a settings value: a string, a { group }, a [ list ] or a bare word;
     depth the groups and lists around it */
  setx::Value setting_value(int depth)
  {
    setx::Value v;
    v.line = peek().pos.line;
    v.col = peek().pos.col;
    if (depth >= setx::max_depth)
      fail(peek().pos, xpp::format("groups and lists are nested at most {} deep", setx::max_depth));
    if (peek().kind == Token::Kind::String) {
      v.kind = setx::Value::Kind::string;
      v.text = take().text;
    } else if (at_punct("{")) {
      v.kind = setx::Value::Kind::group;
      std::map<std::string, int> keys;
      entries("a group", [&](const Token &key) { setting_entry(v, keys, depth + 1, key); });
    } else if (at_punct("[")) {
      v.kind = setx::Value::Kind::list;
      const Pos open = take().pos;
      while (!at_punct("]")) {
        if (at_end()) fail(peek().pos, xpp::format("missing `]` for the list opened at {}:{}", open.line, open.col));
        v.items.push_back(setting_value(depth + 1));
        if (!at_punct(",")) break;
        take();
      }
      expect_punct("]", "to close the list");
    } else {
      Pos at;
      v.text = raw_word(at);
      if (v.text.empty()) fail(at, xpp::format("a value is due, found {}", describe(peek())));
    }
    return v;
  }

  /* options { group: { key: value, ... }, ... } (docs/odex.md "Options") */
  void options(Statement &s)
  {
    s.kind = Statement::Kind::Options;
    if (options_at_) fail(s.pos, xpp::format("options is written once in a file (the first is at line {})", options_at_));
    options_at_ = s.pos.line;
    groups_block(s.options, false);
    push(std::move(s));
  }

  /* { group: { key: value, ... }, ... }: the entries of an options block or of a
     set's `options:`, into out; a set's holds only the groups and keys that sets
     hold (set_holds) */
  void groups_block(std::vector<Option> &out, bool in_set)
  {
    std::vector<std::string> groups;
    std::map<std::string, int> keys;
    entries(in_set ? "the set's options" : "the options block", [&](const Token &g) {
      if (std::ranges::find(option_groups, g.text) == option_groups.end()) {
        std::string all;
        for (std::string_view name : option_groups) all += (all.empty() ? "" : ", ") + std::string(name);
        fail(g.pos, xpp::format("`{}` is not a group of options ({})", g.text, all));
      }
      if (in_set && std::ranges::find(set_groups, g.text) == set_groups.end()) {
        std::string all;
        for (std::string_view name : set_groups) all += (all.empty() ? "" : ", ") + std::string(name);
        fail(g.pos, xpp::format("a set does not hold the group `{}` (a set holds {})", g.text, all));
      }
      if (std::ranges::find(groups, g.text) != groups.end()) fail(g.pos, xpp::format("the group {} is written twice", g.text));
      groups.push_back(g.text);
      entries(xpp::format("the group {}", g.text), [&](const Token &key) { out.push_back(option(g.text, key, keys, in_set)); });
    });
  }

  /* the entry of group whose key was read: its value, by the kind of the
     option the key names. XPPAUT's names, prefixes of a key and a key in
     another group are no keys; a key is given once unless it may repeat. */
  Option option(const std::string &group, const Token &key, std::map<std::string, int> &keys, bool in_set)
  {
    Option o;
    o.group = group;
    o.name = key.text;
    o.pos = key.pos;
    int index = 0;
    const OptionRow *row = find_odex_option(o.name, index);
    if (!row) {
      const OptionRow *xppaut = find_option(upper_case(o.name), index, true);
      fail(key.pos, xppaut && !xppaut->group.empty()
                      ? xpp::format("`{}` is XPPAUT's word: the key is `{}`", o.name, odex_key(*xppaut, index))
                      : xpp::format("`{}` is not an option", o.name));
    }
    if (row->group != group) fail(key.pos, xpp::format("`{}` is in the group {}, not {}", o.name, row->group, group));
    if (in_set && !set_holds(*row))
      fail(key.pos, xpp::format("a set does not hold `{}`: it is a setting of the whole session, not of one graph or run", o.name));
    const auto [first, fresh] = keys.emplace(o.name, key.pos.line);
    if (!fresh && !row->repeatable)
      fail(key.pos, xpp::format("`{}` is given twice (the first is at line {})", o.name, first->second));
    if (row->kind == OptionKind::sliders) {
      o.value_pos = peek().pos;
      const std::size_t begin = peek().begin;
      const setx::Value value = setting_value(0);
      if (value.kind != setx::Value::Kind::list) fail(o.value_pos, "sliders takes a list of presets");
      o.value = std::string(src_.substr(begin, toks_[i_ - 1].end - begin));
      setx::Value root;
      root.kind = setx::Value::Kind::group;
      root.members.push_back({"sliders", key.pos.line, key.pos.col, value});
      const setx::Document doc("options", out_.files[file_], std::string(src_));
      setx::Block block(doc, root, false, "interface");
      try { static_cast<void>(read_sliders(*model_, block, false)); } /* validates the presets */
      catch (ReadFailed &e) { throw std::move(e.error); }
      o.tree = value;
      return o;
    }
    const bool quoted = row->kind == OptionKind::path || row->kind == OptionKind::text;
    o.value_pos = peek().pos;
    if (peek().kind == Token::Kind::String) {
      if (!quoted) fail(o.value_pos, xpp::format("the option {} takes a bare word or number, not a string", o.name));
      o.value = take().text;
      if (o.value.empty()) fail(o.value_pos, xpp::format("the option {} has an empty string", o.name));
      return o;
    }
    if (quoted)
      fail(o.value_pos, xpp::format("the option {} takes {} in double quotes, found {}", o.name,
                                    row->kind == OptionKind::path ? "a path" : "a text", describe(peek())));
    o.value = raw_word(o.value_pos);
    check_bare_value(o, *row);
    return o;
  }

  /* a bare value is a number, or a word of letters, digits and _ . + -;
     a choice row's is one of its words */
  void check_bare_value(const Option &o, const OptionRow &row) const
  {
    const std::string &v = o.value;
    if (v.empty()) fail(o.value_pos, xpp::format("the option {} has no value", o.name));
    double z;
    if (row.kind == OptionKind::number || row.kind == OptionKind::whole) {
      if (!read_number(v, z)) fail(o.value_pos, xpp::format("the option {}'s value `{}` is not a number", o.name, v));
      return;
    }
    if (row.kind == OptionKind::choice) {
      const std::vector<std::string> words = option_choices(row);
      if (std::ranges::find(words, v) != words.end()) return;
      std::string all;
      for (const std::string &w : words) all += (all.empty() ? "" : ", ") + w;
      fail(o.value_pos, xpp::format("the option {}'s value `{}` is none of {}", o.name, v, all));
    }
    for (char k : v)
      if (!is_name_char(k) && k != '.' && k != '-' && k != '+')
        fail(o.value_pos, xpp::format("the option {}'s value `{}` is not a word (letters, digits and _ . + -)", o.name, v));
  }

  /* set name { key: value, ..., options: { group: { key: value } } }: a
     variable's or a parameter's value, a number or a name each, and the
     settings under `options:` */
  void set_block(Statement &s)
  {
    s.kind = Statement::Kind::Set;
    name_of(s, "the set's name");
    std::map<std::string, int> keys;
    entries(xpp::format("the set {}", s.name), [&](const Token &key) {
      const auto [first, fresh] = keys.emplace(key.text, key.pos.line);
      if (!fresh) fail(key.pos, xpp::format("`{}` is given twice in the set (the first is at line {})", key.text, first->second));
      if (key.text == "options") {
        groups_block(s.options, true);
        return;
      }
      Binding b;
      b.name = key.text;
      b.pos = key.pos;
      b.value = set_value();
      s.bindings.push_back(std::move(b));
    });
    push(std::move(s));
  }

  /* a set's value: a number, signed or not, or a name */
  Expr set_value()
  {
    bool negative = false;
    if (at_punct("-") || at_punct("+")) negative = take().text == "-";
    const Token &t = peek();
    Expr v;
    v.pos = t.pos;
    if (t.kind == Token::Kind::Number) {
      v.kind = Expr::Kind::Number;
      v.text = t.text;
      v.value = t.value;
    } else if (t.kind == Token::Kind::Name && !t.primed && !negative) {
      v.kind = Expr::Kind::Name;
      v.text = t.text;
    } else {
      fail(t.pos, xpp::format("a set's value is a number or a name, found {}", describe(t)));
    }
    take();
    if (!negative) return v;
    Expr n = node(Expr::Kind::Neg, v.pos);
    n.args.push_back(std::move(v));
    return n;
  }

  void function(Statement &s)
  {
    s.kind = Statement::Kind::Fun;
    name_of(s, "the function's name");
    expect_punct("(", xpp::format("after fun {}", s.name));
    if (!at_punct(")")) {
      for (;;) {
        const Token &a = declared_name("an argument's name");
        for (const std::string &other : s.names)
          if (other == a.text) fail(a.pos, xpp::format("{} names two arguments", a.text));
        s.names.push_back(a.text);
        s.name_positions.push_back(a.pos);
        if (!at_punct(",")) break;
        take();
      }
    }
    expect_punct(")", "after the function's arguments");
    if (at_punct("{")) {
      s.has_body = true;
      std::vector<std::string> scope = s.names;
      Pos close;
      s.body = block(scope, close);
      if (!returns(s.body))
        fail(close, xpp::format("fun {} can end here without a return", s.name));
    } else {
      expect_punct("=", xpp::format("or `{{` after fun {}(...)", s.name));
      s.expr = expr();
    }
    push(std::move(s));
  }

  /* { block_stmt ... }: the names in scope (the arguments, the locals
     before) in scope, where the closing brace is in close */
  std::vector<BlockStmt> block(std::vector<std::string> &scope, Pos &close)
  {
    const Pos open = peek().pos;
    expect_punct("{", "to open a block");
    std::vector<BlockStmt> out;
    const size_t outer = scope.size();
    bool done = false;
    while (!at_punct("}")) {
      if (at_end()) fail(peek().pos, xpp::format("missing `}}` for the block opened at {}:{}", open.line, open.col));
      if (done) fail(peek().pos, "nothing can follow a return in its block");
      BlockStmt b;
      b.pos = peek().pos;
      if (at_word("let")) {
        take();
        b.kind = BlockStmt::Kind::Let;
        const Token &n = declared_name("the local's name");
        for (const std::string &other : scope)
          if (other == n.text) fail(n.pos, xpp::format("{} is already a name here", n.text));
        b.name = n.text;
        expect_punct("=", xpp::format("after let {}", b.name));
        b.value = expr();
        scope.push_back(b.name);
      } else if (at_word("return")) {
        take();
        b.kind = BlockStmt::Kind::Return;
        b.value = expr();
        done = true;
      } else if (at_word("if")) {
        take();
        b.kind = BlockStmt::Kind::If;
        for (;;) {
          b.conds.push_back(expr());
          Pos c;
          b.blocks.push_back(block(scope, c));
          if (!at_word("else")) break;
          take();
          if (at_word("if")) {
            take();
            continue;
          }
          b.blocks.push_back(block(scope, c));
          break;
        }
        done = returns_if(b);
      } else {
        fail(peek().pos, xpp::format("expected `let`, `return`, `if` or `}}`, found {}", describe(peek())));
      }
      out.push_back(std::move(b));
    }
    close = peek().pos;
    take();
    scope.resize(outer);
    return out;
  }

  static bool returns_if(const BlockStmt &b)
  {
    if (b.blocks.size() == b.conds.size()) return false; /* no else */
    for (const std::vector<BlockStmt> &blk : b.blocks)
      if (!returns(blk)) return false;
    return true;
  }

  /* every path through body ends in a return */
  static bool returns(const std::vector<BlockStmt> &body)
  {
    for (const BlockStmt &b : body) {
      if (b.kind == BlockStmt::Kind::Return) return true;
      if (b.kind == BlockStmt::Kind::If && returns_if(b)) return true;
    }
    return false;
  }

  /* a number, or a signed one */
  double signed_number(std::string_view what)
  {
    Expr e = unary();
    const Expr *n = &e;
    bool neg = false;
    if (e.kind == Expr::Kind::Neg) {
      neg = true;
      n = &e.args[0];
    }
    if (n->kind != Expr::Kind::Number) fail(e.pos, xpp::format("{} is a number", what));
    return neg ? -n->value : n->value;
  }

  int whole_number(std::string_view what)
  {
    const Token &t = peek();
    if (t.kind != Token::Kind::Number) fail(t.pos, xpp::format("expected {}, found {}", what, describe(t)));
    double v = t.value;
    if (v != static_cast<double>(static_cast<int>(v)) || t.text.find_first_of(".eE") != std::string::npos)
      fail(t.pos, xpp::format("{} is a whole number, not {}", what, t.text));
    take();
    return static_cast<int>(v);
  }

  void named_number_key(std::string_view key)
  {
    expect_punct(",", xpp::format("before {}=", key));
    expect_word(key, "in a formula table");
    expect_punct("=", xpp::format("after {}", key));
  }

  void table(Statement &s)
  {
    s.kind = Statement::Kind::Table;
    name_of(s, "the table's name");
    if (peek().kind == Token::Kind::String) {
      s.text = take().text;
      push(std::move(s));
      return;
    }
    s.table_kind = Statement::TableKind::Formula;
    s.expr = expr();
    named_number_key("n");
    s.count = whole_number("the number of points");
    named_number_key("lo");
    s.lo = signed_number("lo");
    named_number_key("hi");
    s.hi = signed_number("hi");
    push(std::move(s));
  }

  void markov(Statement &s)
  {
    s.kind = Statement::Kind::Markov;
    name_of(s, "the Markov variable's name");
    s.count = whole_number("the number of states");
    if (s.count < 2) fail(s.name_pos, xpp::format("{} needs at least 2 states", s.name));
    for (int k = 0; k < s.count * s.count; k++) {
      expect_punct("{", xpp::format("to open transition {} of {} ({} of them)", k + 1, s.name, s.count * s.count));
      s.cells.push_back(expr());
      expect_punct("}", "to close the transition");
    }
    push(std::move(s));
  }

  void event(Statement &s)
  {
    s.kind = Statement::Kind::Event;
    int sign = 1;
    if (at_punct("-") || at_punct("+")) sign = take().text == "-" ? -1 : 1;
    const Token &n = peek();
    int v = whole_number("the event's direction (1, -1 or 0)");
    if (v != 0 && v != 1) fail(n.pos, "the event's direction is 1, -1 or 0");
    s.count = sign * v;
    s.expr = expr();
    while (at_punct(",")) {
      take();
      Binding b;
      const Token &t = name_token("a name to set");
      b.name = t.text;
      b.pos = t.pos;
      b.index = name_index();
      expect_punct("=", xpp::format("after {}", b.name));
      b.value = expr();
      s.bindings.push_back(std::move(b));
    }
    end_of_list("an event");
    push(std::move(s));
  }

  void network(Statement &s)
  {
    s.kind = Statement::Kind::Network;
    name_of(s, "the network's name");
    expect_punct("=", xpp::format("after network {}", s.name));
    const Token &f = name_token("the network's kind (conv, sparse, ...)");
    s.text = f.text;
    expect_punct("(", xpp::format("after {}", s.text));
    bool indexed = false;
    for (;;) {
      const Pos at = peek().pos;
      /* an array's element, name[e] */
      if (peek().kind == Token::Kind::Name && !peek().primed && at_punct("[", 1)) {
        s.call_args.push_back(take().text);
        s.call_arg_indices.push_back(name_index());
        indexed = true;
        if (at_punct(")")) break;
        expect_punct(",", xpp::format("between the arguments of {}", s.text));
        continue;
      }
      s.call_arg_indices.emplace_back();
      int depth = 0;
      size_t b = peek().begin, e = b;
      while (!at_end() && !(depth == 0 && (at_punct(",") || at_punct(")")))) {
        if (at_punct("(") || at_punct("{")) depth++;
        if (at_punct(")") || at_punct("}")) depth--;
        e = take().end;
      }
      if (e == b) fail(at, xpp::format("an argument of {} is missing", s.text));
      s.call_args.emplace_back(src_.substr(b, e - b));
      if (at_punct(")")) break;
      expect_punct(",", xpp::format("between the arguments of {}", s.text));
    }
    take();
    if (!indexed) s.call_arg_indices.clear();
    push(std::move(s));
  }

  /* import "f.odex" and from "f.odex" import a, b (docs/odex.md
     "Imports"): the file read where the import stands, its statements as
     import_statements takes them */
  void import(Statement &s, bool selective)
  {
    Import imp;
    imp.selective = selective;
    imp.at = s.pos;
    const Token &t = peek();
    if (t.kind != Token::Kind::String)
      fail(t.pos, xpp::format("expected the file's name in quotes, found {}", describe(t)));
    imp.file = take().text;
    if (selective) {
      expect_word("import", "after the file's name");
      for (;;) {
        imp.names.push_back(name_token("a name to import").text);
        if (!at_punct(",")) break;
        take();
      }
    }
    end_of_list("an import");
    if (at_range()) fail(peek().pos, "an import takes no range");
    if (xpp::files::has_extension(imp.file, ".ode")) fail(t.pos, "import reads .odex text, never a .ode model");
    if (depth_ >= max_include_depth) fail(t.pos, xpp::format("too many imports inside each other at {}", imp.file));
    if (++out_.imports > max_imports) fail(t.pos, xpp::format("a model imports at most {} files in all", max_imports));
    const std::string path = xpp::include_path(out_.files[file_], imp.file);
    if (std::ranges::find(out_.importing, path) != out_.importing.end())
      fail(t.pos, xpp::format("{} imports itself (through {})", imp.file, out_.importing.back()));
    std::string text;
    if (const xpp::Result<> allowed = xpp::approve_model_path(*model_, path, xpp::Place{out_.files[t.pos.file], t.pos.line, t.pos.col, {}}); !allowed)
      throw allowed.error();
    if (!xpp::read_model_file(*model_, path, text)) fail(t.pos, xpp::format("cannot read the imported file {}", path));
    out_.files.push_back(path);
    const size_t first = out_.statements.size();
    out_.importing.push_back(path);
    Parser(model_, out_, static_cast<int>(out_.files.size()) - 1, text, depth_ + 1).model();
    out_.importing.pop_back();
    std::vector<Statement> file(std::make_move_iterator(out_.statements.begin() + first),
                                std::make_move_iterator(out_.statements.end()));
    out_.statements.erase(out_.statements.begin() + first, out_.statements.end());
    for (Statement &x : import_statements(std::move(file), imp, out_.files)) out_.statements.push_back(std::move(x));
  }

  /* ---- expressions, low precedence to high (docs/odex.md) ---- */
  static Expr node(Expr::Kind k, Pos p, std::string text = std::string())
  {
    Expr e;
    e.kind = k;
    e.pos = p;
    e.text = std::move(text);
    return e;
  }
  /* a binary operation, placed where its left operand starts */
  static Expr binary(std::string op, Pos, Expr a, Expr b)
  {
    Expr e = node(Expr::Kind::Binary, a.pos, std::move(op));
    e.args.push_back(std::move(a));
    e.args.push_back(std::move(b));
    return e;
  }

  Expr expr() { return or_expr(); }

  Expr or_expr()
  {
    Expr e = and_expr();
    while (at_word("or")) {
      Pos p = take().pos;
      e = binary("or", p, std::move(e), and_expr());
    }
    return e;
  }

  Expr and_expr()
  {
    Expr e = not_expr();
    while (at_word("and")) {
      Pos p = take().pos;
      e = binary("and", p, std::move(e), not_expr());
    }
    return e;
  }

  Expr not_expr()
  {
    if (at_word("not")) {
      Expr e = node(Expr::Kind::Not, take().pos);
      e.args.push_back(cmp_expr());
      return e;
    }
    return cmp_expr();
  }

  bool at_cmp() const
  {
    static constexpr std::array<std::string_view, 6> ops = {"<", ">", "<=", ">=", "==", "!="};
    for (std::string_view o : ops)
      if (at_punct(o)) return true;
    return false;
  }

  Expr cmp_expr()
  {
    Expr e = add_expr();
    if (at_cmp()) {
      const Token &op = take();
      std::string text = op.text;
      Pos p = op.pos;
      e = binary(text, p, std::move(e), add_expr());
      if (at_cmp())
        fail(peek().pos, "comparisons do not chain: write (a < b) and (b < c)");
    }
    return e;
  }

  Expr add_expr()
  {
    Expr e = mul_expr();
    while (at_punct("+") || at_punct("-")) {
      const Token &op = take();
      std::string text = op.text;
      Pos p = op.pos;
      e = binary(text, p, std::move(e), mul_expr());
    }
    return e;
  }

  Expr mul_expr()
  {
    Expr e = unary();
    while (at_punct("*") || at_punct("/") || at_word("mod")) {
      const Token &op = take();
      std::string text = op.text;
      Pos p = op.pos;
      e = binary(text, p, std::move(e), unary());
    }
    return e;
  }

  Expr unary()
  {
    if (at_punct("-")) {
      Expr e = node(Expr::Kind::Neg, take().pos);
      e.args.push_back(pow_expr());
      return e;
    }
    if (at_punct("+")) fail(peek().pos, "there is no unary `+`: drop it");
    return pow_expr();
  }

  Expr pow_expr()
  {
    Expr e = postfix();
    if (at_punct("^")) {
      Pos p = take().pos;
      e = binary("^", p, std::move(e), unary());
    }
    return e;
  }

  Expr postfix()
  {
    const bool bare_name = peek().kind == Token::Kind::Name;
    Expr e = primary();
    for (;;) {
      if (at_punct("(")) {
        if (!bare_name || e.kind != Expr::Kind::Name || e.primed) fail(peek().pos, "only a function's name can be called");
        e.kind = Expr::Kind::Call;
        call_args(e);
      } else if (at_punct("[")) {
        Pos p = take().pos;
        Expr ix = node(Expr::Kind::Index, p);
        ix.args.push_back(std::move(e));
        ix.args.push_back(expr());
        expect_punct("]", "to close the index");
        e = std::move(ix);
      } else {
        return e;
      }
    }
  }

  void call_args(Expr &e)
  {
    take();
    if (at_punct(")")) {
      take();
      return;
    }
    for (;;) {
      std::string name;
      if (!e.args.empty() && peek().kind == Token::Kind::Name && !peek().primed && at_punct("=", 1)) {
        name = take().text;
        take();
      }
      e.args.push_back(expr());
      e.arg_names.push_back(name);
      if (!at_punct(",")) break;
      take();
    }
    expect_punct(")", xpp::format("to close {}(", e.text));
  }

  Expr primary()
  {
    const Token &t = peek();
    switch (t.kind) {
    case Token::Kind::Number: {
      Expr e = node(Expr::Kind::Number, t.pos, t.text);
      e.value = t.value;
      take();
      return e;
    }
    case Token::Kind::Name:
      if (!t.primed && t.text == "if") return if_expr();
      if (!t.primed && is_keyword(t.text))
        fail(t.pos, xpp::format("expected a value, found the reserved word `{}`", t.text));
      {
        Expr e = node(Expr::Kind::Name, t.pos, t.text);
        e.primed = t.primed;
        take();
        return e;
      }
    case Token::Kind::Punct:
      if (t.text == "(") {
        const Pos open = take().pos;
        Expr e = expr();
        if (!at_punct(")"))
          fail(peek().pos, xpp::format("expected `)` to close the `(` at {}:{}, found {}", open.line, open.col, describe(peek())));
        take();
        return e;
      }
      break;
    default:
      break;
    }
    fail(t.pos, xpp::format("expected a value, found {}", describe(t)));
  }

  Expr if_expr()
  {
    Expr e = node(Expr::Kind::If, take().pos);
    e.args.push_back(expr());
    expect_word("then", "after the if's condition");
    e.args.push_back(expr());
    expect_word("else", "(an if needs its else)");
    e.args.push_back(expr());
    return e;
  }

  /* a reserved word that is never a value (the built-in functions, t and
     pi are values or calls) */
  static bool is_keyword(std::string_view w)
  {
    static constexpr auto words = std::to_array<std::string_view>({
      "par", "init", "aux", "fun", "let", "return", "if", "then", "else",
      "table", "markov", "wiener", "event", "boundary", "network", "import",
      "solv", "only", "history", "const", "by", "options", "and", "or", "not", "mod", "of"});
    for (std::string_view k : words)
      if (k == w) return true;
    return false;
  }

  /* the model an import reads its files through; none for a settings file */
  xpp::Model *model_;
  Parsed &out_;
  int file_;
  std::string_view src_;
  int depth_;
  std::vector<Token> toks_;
  size_t i_ = 0;
  int last_line_ = 0;
  /* the line of this file's options block, 0 before it */
  int options_at_ = 0;
};

} // namespace

bool is_reserved(std::string_view word)
{
  if (builtin_arity(word) != -1) return true;
  for (std::string_view w : reserved_words)
    if (w == word) return true;
  return false;
}

bool is_name(std::string_view name)
{
  if (name.empty() || !is_letter(name.front())) return false;
  return std::ranges::all_of(name, is_name_char);
}

Parsed parse(xpp::Model &m, std::string_view text, const std::string &file)
{
  Parsed out;
  out.files.push_back(file);
  out.importing.push_back(file);
  Parser(&m, out, 0, text, 0).model();
  return out;
}

Result<setx::Value> read_settings_tree(const std::string &where, const std::string &file, std::string_view text)
{
  try {
    Parsed out;
    out.files.push_back(file);
    return Parser(nullptr, out, 0, text, 0).settings_file();
  } catch (Error &e) {
    e.where = where;
    return std::unexpected(std::move(e));
  }
}

}
