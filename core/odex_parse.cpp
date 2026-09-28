/* .odex's tokenizer and grammar (odex.h, docs/odex.md "Grammar"): a
   model's text to its statements. The tokenizer never strips white space
   first (a word ends where a word ends, whatever the spacing); comments,
   '#' to the end of the line and nested / * ... * / blocks, go with the
   white space. Every problem is an Error at a line and column; nothing is
   dropped, cut or skipped. */
#include "odex.h"
#include "xpp_io.h"

#include <array>
#include <charconv>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace xpp::odex {

std::string Error::text() const
{
  return xpp::format("{}:{}:{}: {}", file, pos.line, pos.col, message);
}

namespace {

/* the words docs/odex.md reserves: the statement keywords (and solv and
   only, the .ode statements .odex keeps with their own spelling), the
   operator words, volterra, the built-in functions (every one the
   expression engine has, lgamma, poisson and besselis included) and
   constants (t, pi, and the animator's mouse_x ... mouse_vy) */
constexpr auto reserved_words = std::to_array<std::string_view>({
  "par", "init", "aux", "fun", "let", "return", "if", "then", "else", "set",
  "table", "markov", "wiener", "event", "boundary", "network", "include",
  "solv", "only", "history",
  "and", "or", "not", "mod", "volterra",
  "sin", "cos", "tan", "asin", "acos", "atan", "atan2", "sinh", "cosh",
  "tanh", "exp", "ln", "log", "log10", "sqrt", "heav", "sign", "flr", "ran",
  "abs", "delay", "shift", "ishift", "del_shft", "sum", "of", "max", "min",
  "normal", "besselj", "bessely", "besseli", "besselis", "erf", "erfc",
  "hom_bcs", "lgamma", "poisson",
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

/* a number's text to its value, false when the text is not all of one */
bool read_number(std::string_view text, double &value)
{
  if (text.empty()) return false;
  if (text[0] == '+') text.remove_prefix(1);
  const char *first = text.data(), *last = text.data() + text.size();
  std::from_chars_result r = std::from_chars(first, last, value);
  return r.ec == std::errc() && r.ptr == last;
}

class Lexer {
public:
  Lexer(std::string_view src, int file, const std::string &name) : s_(src), file_(file), name_(name) {}

  std::vector<Token> run()
  {
    std::vector<Token> out;
    for (;;) {
      bool space = skip_blank();
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
        if (i_ < s_.size() && s_[i_] == '.') {
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
        size_t b = ++i_;
        while (i_ < s_.size() && s_[i_] != '"' && s_[i_] != '\n') i_++;
        if (i_ >= s_.size() || s_[i_] != '"') fail(t.pos, "this string is never closed on its line");
        t.kind = Token::Kind::String;
        t.text = std::string(s_.substr(b, i_ - b));
        i_++;
      } else {
        static constexpr std::array<std::string_view, 4> two = {"<=", ">=", "==", "!="};
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

  [[noreturn]] void fail(Pos p, std::string msg) const { throw Error{name_, p, std::move(msg)}; }

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

constexpr int max_include_depth = 16;

class Parser {
public:
  Parser(Parsed &out, int file, std::string_view src, int depth)
      : out_(out), file_(file), src_(src), depth_(depth)
  {
    toks_ = Lexer(src, file, out.files[file]).run();
  }

  void model()
  {
    while (!at_end()) statement();
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
    throw Error{out_.files[p.file], p, std::move(msg)};
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

  /* a list statement ends at the end of its line: another item on the
     same line needs its comma (par a=1 b=2 would otherwise read b=2 as
     a statement of its own) */
  void end_of_list(std::string_view what)
  {
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
    if (t.kind == Token::Kind::Punct && t.text == "@") {
      take();
      options(s);
      return;
    }
    if (t.kind == Token::Kind::Punct && t.text == "!") {
      take();
      s.kind = Statement::Kind::Derived;
      s.bindings.push_back(binding("a derived parameter's name"));
      end_of_list("a derived parameter");
      push(std::move(s));
      return;
    }
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
    if (word == "par" || word == "init" || word == "aux" || word == "history") {
      take();
      s.kind = word == "par" ? Statement::Kind::Par : word == "init" ? Statement::Kind::Init
             : word == "aux" ? Statement::Kind::Aux : Statement::Kind::History;
      bindings(s, word);
      push(std::move(s));
    } else if (word == "fun") {
      take();
      function(s);
    } else if (word == "set") {
      take();
      s.kind = Statement::Kind::Set;
      name_of(s, "the set's name");
      expect_punct("=", "after the set's name");
      bindings(s, "set");
      push(std::move(s));
    } else if (word == "table") {
      take();
      table(s);
    } else if (word == "markov") {
      take();
      markov(s);
    } else if (word == "wiener" || word == "only") {
      take();
      s.kind = word == "wiener" ? Statement::Kind::Wiener : Statement::Kind::Only;
      for (;;) {
        const Token &n = word == "wiener" ? declared_name("a name") : name_token("a name");
        s.names.push_back(n.text);
        s.name_positions.push_back(n.pos);
        if (!at_punct(",")) break;
        take();
      }
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
    } else if (word == "include") {
      take();
      include();
    } else if (is_reserved(word)) {
      fail(t.pos, xpp::format("`{}` is a reserved word and cannot start a statement", word));
    } else if (at_punct("=", 1)) {
      s.kind = Statement::Kind::Fixed;
      name_of(s, "a name");
      take();
      s.expr = expr();
      push(std::move(s));
    } else if (at_punct("(", 1) && at_word("t", 2) && at_punct(")", 3) && at_punct("=", 4)) {
      s.kind = Statement::Kind::Volterra;
      name_of(s, "a name");
      take();
      take();
      take();
      take();
      s.expr = expr();
      push(std::move(s));
    } else {
      fail(peek(1).pos, xpp::format("expected `'`, `=` or `(t) =` after `{}`, found {}", word, describe(peek(1))));
    }
  }

  void push(Statement s) { out_.statements.push_back(std::move(s)); }

  const Token &name_token(std::string_view what)
  {
    const Token &t = peek();
    if (t.kind != Token::Kind::Name || t.primed) fail(t.pos, xpp::format("expected {}, found {}", what, describe(t)));
    return take();
  }

  void name_of(Statement &s, std::string_view what)
  {
    const Token &n = declared_name(what);
    s.name = n.text;
    s.name_pos = n.pos;
  }

  Binding binding(std::string_view what)
  {
    Binding b;
    const Token &n = declared_name(what);
    b.name = n.text;
    b.pos = n.pos;
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
     space between), up to a comma: an option's value, a file's name */
  std::string raw_word(Pos &at)
  {
    at = peek().pos;
    if (at_end() || at_punct(",")) return std::string();
    size_t b = peek().begin, e = take().end;
    while (!at_end() && !peek().space_before && !at_punct(",")) e = take().end;
    return std::string(src_.substr(b, e - b));
  }

  void options(Statement &s)
  {
    s.kind = Statement::Kind::Options;
    for (;;) {
      Option o;
      const Token &n = name_token("an option's name");
      o.name = n.text;
      o.pos = n.pos;
      expect_punct("=", xpp::format("after the option {}", o.name));
      o.value = raw_word(o.value_pos);
      check_option_value(o);
      s.options.push_back(std::move(o));
      if (!at_punct(",")) break;
      take();
    }
    end_of_list("an @ line");
    push(std::move(s));
  }

  /* an option's value is a number, whole, or a name or a file's name:
     never an expression cut where a number ends (.ode's atof) */
  void check_option_value(const Option &o) const
  {
    const std::string &v = o.value;
    if (v.empty()) fail(o.value_pos, xpp::format("the option {} has no value", o.name));
    char c = v[0];
    double z;
    if (is_digit(c) || c == '.' || c == '-' || c == '+') {
      if (!read_number(v, z))
        fail(o.value_pos, xpp::format("the option {}'s value `{}` is not a number", o.name, v));
      return;
    }
    for (char k : v)
      if (!is_name_char(k) && k != '.' && k != '-' && k != '/' && k != '\\' && k != ':')
        fail(o.value_pos, xpp::format("the option {}'s value `{}` is neither a number nor a name", o.name, v));
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
    for (;;) {
      const Pos at = peek().pos;
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
    push(std::move(s));
  }

  void include()
  {
    const Token &t = peek();
    if (t.kind != Token::Kind::String) fail(t.pos, xpp::format("expected the file's name in quotes, found {}", describe(t)));
    std::string name = take().text;
    if (depth_ >= max_include_depth) fail(t.pos, xpp::format("too many includes inside each other at {}", name));
    /* relative to the including file's folder */
    const std::string &here = out_.files[file_];
    size_t slash = here.find_last_of("/\\");
    bool absolute = !name.empty() && (name[0] == '/' || name[0] == '\\' || (name.size() > 1 && name[1] == ':'));
    std::string path = (slash == std::string::npos || absolute) ? name : here.substr(0, slash + 1) + name;
    std::string text;
    if (!xpp::read_bytes(path.c_str(), text)) fail(t.pos, xpp::format("cannot read the included file {}", path));
    out_.files.push_back(path);
    Parser(out_, static_cast<int>(out_.files.size()) - 1, text, depth_ + 1).model();
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
      "table", "markov", "wiener", "event", "boundary", "network", "include",
      "solv", "only", "history", "and", "or", "not", "mod", "of"});
    for (std::string_view k : words)
      if (k == w) return true;
    return false;
  }

  Parsed &out_;
  int file_;
  std::string_view src_;
  int depth_;
  std::vector<Token> toks_;
  size_t i_ = 0;
  int last_line_ = 0;
};

} // namespace

bool is_reserved(std::string_view word)
{
  for (std::string_view w : reserved_words)
    if (w == word) return true;
  return false;
}

Parsed parse(std::string_view text, const std::string &file)
{
  Parsed out;
  out.files.push_back(file);
  Parser(out, 0, text, 0).model();
  return out;
}

Parsed parse_file(const std::string &path)
{
  std::string text;
  if (!xpp::read_bytes(path.c_str(), text)) throw Error{path, Pos{}, "cannot read the file"};
  return parse(text, path);
}

}
