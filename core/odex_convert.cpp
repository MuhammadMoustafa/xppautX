/* xppautX --convert (odex.h; docs/odex.md "What --convert writes"): the
   loaded .ode model written as .odex from what the .ode parser understood,
   not from its text. The statements come in the order the reader read
   them (Model::statements), an array's copies (Statement::array) written
   back as one array statement (docs/odex.md "Arrays"); every formula is the one it
   compiled, read back from its program (expr_program.h) into .odex's tree
   and printed with the parentheses .odex needs to group it the same way:
   a quirk of .ode's precedence becomes explicit parentheses, never a
   different value. The source text serves only to spell a name the way
   its declaration does (.ode keeps names in upper case), to copy the
   model's comments and what follows its done. */
#include "odex.h"
#include "expr_internal.h"
#include "form_ode.h"
#include "ode_read.h"
#include "integrate.h"
#include "load_eqn.h"
#include "model.h"
#include "session.h"
#include "solver.h"
#include "tabular.h"
#include "xpp_batch.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_log.h"
#include "xpp_util.h"
#include "xpp_ui.h"
#include "browse.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace xpp::odex {

namespace {

/* the leading word of s */
std::string leading_word(std::string_view s)
{
  size_t i = 0;
  while (i < s.size() && is_word_char(s[i])) i++;
  return std::string(s.substr(0, i));
}

/* a source line without its comment, blanks trimmed */
std::string code_of(std::string_view raw)
{
  return std::string(xpp::trim_blanks(raw.substr(0, ode_comment_start(raw))));
}

/* A model's names as its declarations spell them: the .ode reader keeps
   every name in upper case (V and v are one name); .odex has case, so a
   name is written the way the statement declaring it does, everywhere,
   and the lines that spelled it another way are listed. */
class Spelling {
public:
  explicit Spelling(const std::vector<std::string> &source)
  {
    for (const std::string &raw : source) {
      std::string line = code_of(raw);
      if (line.empty() || line[0] == '#' || line[0] == '"' || line[0] == '@' || line[0] == '%') continue;
      declarations(line);
      for (const std::string &w : words_of(line)) first_.emplace(xpp::upper_case(w), w);
    }
    int n = 0;
    for (const std::string &raw : source) {
      n++;
      std::string line = code_of(raw);
      if (line.empty() || line[0] == '#' || line[0] == '"') continue;
      for (const std::string &w : words_of(line)) {
        auto d = decl_.find(xpp::upper_case(w));
        if (d != decl_.end() && d->second != w) {
          respelled_.push_back(n);
          break;
        }
      }
    }
  }

  /* upper, a name as the reader keeps it, as its declaration spells it */
  std::string of(const std::string &upper) const
  {
    auto d = decl_.find(upper);
    if (d != decl_.end()) return d->second;
    /* an array's element, x[1..9] made X1 ... X9 */
    size_t k = upper.size();
    while (k > 0 && upper[k - 1] >= '0' && upper[k - 1] <= '9') k--;
    if (k > 0 && k < upper.size()) {
      auto b = arrays_.find(upper.substr(0, k));
      if (b != arrays_.end()) return b->second + upper.substr(k);
    }
    auto f = first_.find(upper);
    if (f != first_.end()) return f->second;
    return xpp::lower_case(upper);
  }

  /* argument i of function fun (upper case), as its definition spells it */
  std::string arg(const std::string &fun, size_t i, const std::string &upper) const
  {
    auto a = args_.find(fun);
    if (a != args_.end() && i < a->second.size() && xpp::upper_case(a->second[i]) == upper) return a->second[i];
    return xpp::lower_case(upper);
  }

  const std::vector<int> &respelled() const { return respelled_; }

private:
  void declare(const std::string &w)
  {
    if (!w.empty()) decl_.emplace(xpp::upper_case(w), w);
  }

  /* the names a line declares, as the reader tells its statements apart
     (ode_read.cpp's parse_a_string and command) */
  void declarations(const std::string &line)
  {
    if (line[0] == '!') {
      declare(leading_word(std::string_view(line).substr(1)));
      return;
    }
    const std::string first = leading_word(line);
    const std::string_view rest = std::string_view(line).substr(first.size());
    size_t k = 0;
    while (k < rest.size() && (rest[k] == ' ' || rest[k] == '\t')) k++;
    const bool command = k > 0 && k < rest.size() && rest[k] != '=' ;
    if (command) {
      const std::string lower = xpp::lower_case(first);
      const std::string_view items = rest.substr(k);
      const char c0 = lower[0];
      if (c0 == 'p' || c0 == 'n' || c0 == 'w') {
        for (const OdeItem &item : ode_items(items)) declare(item.name);
      } else if (lower.starts_with("au") || lower.starts_with("ta") || lower.starts_with("sp") ||
                 lower.starts_with("ma") || lower.starts_with("so") || (c0 == 'v' && !lower.starts_with("vec"))) {
        const std::string w = leading_word(items);
        if (items.size() > w.size() && items[w.size()] == '[') arrays_.emplace(xpp::upper_case(w), w);
        else declare(w);
      }
      return;
    }
    if (first.empty()) return;
    const char next = rest.empty() ? '\0' : rest[0];
    if (next == '[') {
      arrays_.emplace(xpp::upper_case(first), first);
      return;
    }
    if (next == '/' && (first[0] == 'd' || first[0] == 'D') && first.size() > 1) {
      declare(first.substr(1));
      return;
    }
    if (next == '\'' || next == '=' || (next == ' ' && k < rest.size() && rest[k] == '=')) {
      declare(first);
      return;
    }
    if (next == '(') {
      const size_t close = rest.find(')');
      if (close == std::string_view::npos) return;
      const std::string inside = std::string(xpp::trim_blanks(rest.substr(1, close - 1)));
      const std::string upper = xpp::upper_case(inside);
      if (upper == "0") return; /* an initial value */
      declare(first);
      if (upper == "T" || upper == "T+1") return;
      std::vector<std::string> names;
      for (const std::string &w : words_of(inside)) names.push_back(w);
      args_.emplace(xpp::upper_case(first), names);
    }
  }

  std::map<std::string, std::string> decl_, first_, arrays_;
  std::map<std::string, std::vector<std::string>> args_;
  std::vector<int> respelled_;
};

/* the kinds of name a program reads, by what its instruction indexes */
struct Symbols {
  std::map<int, std::string> by_com;
  explicit Symbols(const ParserState &p)
  {
    for (int k = 0; k < p.nsym; k++) by_com.emplace(p.symbols[k].com, p.symbols[k].name);
  }
  const std::string *find(int com) const { return by_com.count(com) ? &by_com.at(com) : nullptr; }
};

/* the conversion of the model at file stops: why */
[[noreturn]] void refuse_in(const std::string &file, std::string msg)
{
  throw error_at(file, Pos{}, std::move(msg));
}

Expr name_expr(std::string text, bool primed = false)
{
  Expr e;
  e.kind = Expr::Kind::Name;
  e.text = std::move(text);
  e.primed = primed;
  return e;
}

Expr number_expr(double v)
{
  Expr e;
  e.kind = Expr::Kind::Number;
  e.value = v;
  e.text = print_number(v);
  return e;
}

Expr op_expr(Expr::Kind kind, std::string text, std::vector<Expr> args)
{
  Expr e;
  e.kind = kind;
  e.text = std::move(text);
  e.args = std::move(args);
  e.arg_names.assign(e.args.size(), std::string());
  return e;
}

class Converter {
public:
  Converter(xpp::Session &s, bool auto_answer, const Ask &ask, std::vector<Diagnostic> &diagnostics)
      : m_(s.model()), s_(s), spell_(m_.source), sym_(s_.parser), auto_(auto_answer), ask_(ask), diagnostics_(diagnostics)
  {
  }

  std::string run()
  {
    rename_reserved();
    steady_constants();
    guard_name_ = "ode_divisor";
    while (taken_.count(xpp::upper_case(guard_name_))) guard_name_ += "_";
    std::string body;
    const std::vector<Statement> &st = m_.statements;
    for (size_t i = 0; i < st.size();) {
      /* an array's copies, one statement each, consecutive */
      const ArrayCopy &a = st[i].array;
      size_t e = i + 1;
      if (a.group && !a.interleaved)
        while (e < st.size() && st[e].array.group == a.group) e++;
      std::vector<std::string> texts;
      std::vector<int> values;
      for (size_t k = i; k < e; k++) {
        texts.push_back(statement(st[k]));
        values.push_back(st[k].array.value);
      }
      body += e - i > 1 ? as_array(texts, values, a) : texts[0];
      i = e;
    }
    body += array_initials();
    body += comments();
    body += options();
    std::string out = header();
    out += body;
    out += after_done();
    if (uses_guard_)
      out += "\nfun " + guard_name_ + "(value) = if value then value else " + print(number_expr(xpp::expr::ZERO_DIVISOR)) + "\n";
    return out;
  }

  std::string guard_name() const { return uses_guard_ ? guard_name_ : std::string(); }

private:
  /* Inspect operators at each parenthesis depth: explicit grouping removes
     ambiguity. Tokens come from the source, semantics from the loaded .ode. */
  void expression_findings(const Statement &s)
  {
    if (s.kind == Statement::Kind::Par || s.kind == Statement::Kind::Const ||
        s.kind == Statement::Kind::InitNumbers || s.kind == Statement::Kind::History ||
        s.kind == Statement::Kind::Options || s.kind == Statement::Kind::Comment ||
        s.kind == Statement::Kind::OptionFile || s.kind == Statement::Kind::Only ||
        s.kind == Statement::Kind::Network || s.kind == Statement::Kind::Vector ||
        s.kind == Statement::Kind::Group || s.kind == Statement::Kind::Wiener ||
        s.kind == Statement::Kind::Set ||
        (s.kind == Statement::Kind::Table && s.table_kind != Statement::TableKind::Formula)) return;
    if (!scanned_.insert({place_.file, place_.line}).second) return;
    const std::string raw = place_.source;
    const size_t end = ode_comment_start(raw);
    const std::string_view source(raw.data(), end == std::string::npos ? raw.size() : end);
    size_t start = source.find('=');
    start = start == std::string::npos ? 0 : start + 1;
    if (s.kind == Statement::Kind::Boundary || s.kind == Statement::Kind::Event) start = 0;
    struct Operator { std::string text; size_t col; };
    std::vector<std::vector<Operator>> scopes(1);
    std::set<std::string> reported;
    const auto report = [&](const Operator &op, const char *id, const char *severity, const char *message) {
      if (!reported.insert(id).second) return;
      Place at = place_;
      at.col = static_cast<int>(op.col + 1);
      diagnostics_.push_back({std::move(at), id, severity, message});
    };
    const auto comparison = [](const std::string &op) {
      return op == "<" || op == ">" || op == "<=" || op == ">=" || op == "==" || op == "!=";
    };
    const auto arithmetic = [](const std::string &op) {
      return op == "+" || op == "-" || op == "*" || op == "/";
    };
    const auto inspect = [&](const std::vector<Operator> &ops) {
      for (size_t i = 0; i < ops.size(); ++i) {
        const Operator &op = ops[i];
        if (comparison(op.text)) {
          for (const Operator &other : ops) {
            if (arithmetic(other.text)) report(op, "comparison-precedence", "warning", "XPP comparisons bind before arithmetic; conversion parenthesizes that meaning.");
          }
          if (i && comparison(ops[i - 1].text)) report(op, "chained-comparison", "warning", "XPP compares the preceding comparison's result; conversion keeps that grouping.");
        }
        if (op.text == "^" && i && ops[i - 1].text == "^")
          report(op, "power-associativity", "info", "XPP powers group left; conversion parenthesizes that meaning.");
        if (op.text == "^" && i && ops[i - 1].text == "neg")
          report(op, "unary-minus-power", "info", "Power binds before unary minus, as in ordinary mathematics.");
        if (comparison(op.text) && !ops.empty() && ops.front().text == "neg")
          report(op, "comparison-precedence", "warning", "XPP comparisons bind before unary minus; conversion parenthesizes that meaning.");
        if ((op.text == "&" || op.text == "|") && i + 1 < ops.size()) {
          const std::string &next = ops[i + 1].text;
          if ((op.text == "&" && arithmetic(next)) ||
              (op.text == "|" && (next == "+" || next == "-")))
            report(op, "logical-precedence", "warning", "XPP logical operators share arithmetic precedence; conversion parenthesizes that meaning.");
        }
        if (op.text == "&" && i && (ops[i - 1].text == "+" || ops[i - 1].text == "-"))
          report(op, "logical-precedence", "warning", "XPP logical operators share arithmetic precedence; conversion parenthesizes that meaning.");
      }
    };
    bool operand = true;
    bool else_branch = false;
    size_t else_depth = 0;
    bool after_if = false;
    for (size_t i = start; i < source.size();) {
      const char c = source[i];
      if (c == ' ' || c == '\t') { ++i; continue; }
      if ((c >= '0' && c <= '9') || c == '.') {
        char *last = nullptr;
        std::strtod(raw.c_str() + i, &last);
        const size_t next = static_cast<size_t>(last - raw.c_str());
        i = next > i ? next : i + 1;
        operand = false;
        continue;
      }
      if (is_word_start(c)) {
        const size_t word = i;
        do { ++i; } while (i < source.size() && is_word_char(source[i]));
        if (equal_ignoring_case(source.substr(word, i - word), "else")) {
          else_branch = true;
          else_depth = scopes.size();
        }
        operand = false;
        continue;
      }
      if (c == '(' || c == '{' || c == ',') {
        if (c == ',') { inspect(scopes.back()); scopes.back().clear(); }
        else scopes.emplace_back();
        operand = true; ++i; continue;
      }
      if (c == ')' || c == '}') {
        inspect(scopes.back());
        if (scopes.size() > 1) scopes.pop_back();
        if (else_branch && scopes.size() == else_depth) {
          after_if = true;
          else_branch = false;
        }
        operand = false; ++i; continue;
      }
      if (std::string_view("+-*/^<>=&|").find(c) != std::string_view::npos) {
        Operator op{std::string(1, c), i};
        if (after_if) report(op, "if-trailing-operator", "info", "The trailing operator applies to the whole if/then/else result.");
        after_if = false;
        if (i + 1 < source.size() && ((c == '*' && source[i + 1] == '*') ||
            ((c == '<' || c == '>' || c == '=') && source[i + 1] == '='))) {
          op.text += source[++i];
          if (op.text == "**") op.text = "^";
        }
        if (c == '-' && operand) op.text = "neg";
        if (c == '/') {
          size_t divisor = i + 1;
          while (divisor < source.size() && (source[divisor] == ' ' || source[divisor] == '\t' || source[divisor] == '(')) ++divisor;
          char *last = nullptr;
          const double value = std::strtod(raw.c_str() + divisor, &last);
          size_t next = static_cast<size_t>(last - raw.c_str());
          while (next < source.size() && (source[next] == ' ' || source[next] == '\t')) ++next;
          const bool parenthesized = source.substr(i + 1, divisor - i - 1).find('(') != std::string_view::npos;
          const bool power = next < source.size() && (source[next] == '^' || source.substr(next, 2) == "**");
          const bool literal = parenthesized ? next < source.size() && source[next] == ')' : !power;
          if (last != raw.c_str() + divisor && value == 0 && literal)
            report(op, "division-by-zero", "warning", "XPP replaces a zero divisor with 2.23e-15; conversion keeps guarded division.");
        }
        scopes.back().push_back(op);
        operand = true;
      }
      ++i;
    }
    for (const auto &ops : scopes) inspect(ops);
  }

  /* the conversion stops: why */
  [[noreturn]] void refuse(std::string msg) const
  {
    throw Error{"convert", std::move(msg), place_.file.empty() ? Place{m_.this_file} : place_};
  }

  /* the text a program was compiled from, compiled again: a formula the
     Model keeps only as text (a derived parameter's, a boundary
     condition's, a function table's) */
  std::vector<int> compiled(const std::string &text) const
  {
    std::vector<int> prog(MAXEXPLEN, 0);
    int len = 0;
    if (add_expr(s_, text, prog.data(), &len)) refuse(xpp::format("the formula {} does not compile", text));
    prog.resize(len);
    s_.parser.ncon = m_.ncon_start;
    s_.parser.nsym = m_.nsym_start;
    return prog;
  }

  void steady_constants()
  {
    /* the constants that keep their value through a run: every one but
       sum's index, the animator's mouse, parameters and derived quantities */
    std::set<int> moving = {xpp::expr::SUM_INDEX};
    for (int k = 0; k < m_.nupar; k++) moving.insert(m_.upar_con[k]);
    for (const auto &d : m_.derived) moving.insert(d.index);
    for (int k = 0; k < m_.nwiener; k++) moving.insert(m_.wiener[k]);
    for (int j = 0; j < m_.nflags; j++)
      for (int e = 0; e < m_.flags[j].nevents; e++)
        if (m_.flags[j].type[e] == 1) {
          const int c = get_param_index(s_, xpp::upper_case(m_.flags[j].lhsname[e]));
          if (c >= 0) moving.insert(c);
        }
    for (const auto &[com, n] : sym_.by_com) {
      if (com / MAXTYPE != CONTYPE || n.starts_with("MOUSE_")) continue;
      const int in = com % MAXTYPE;
      if (moving.count(in)) continue;
      constants_.insert(in);
      if (n != "PI") constant_values_[name(n)] = s_.parser.constants[in];
    }
  }

  /* ---- arrays ---- */
  /* text in pieces to compare copies with: a run of word characters and
     '.' (a name, a number), or any other character alone */
  static std::vector<std::string> pieces(std::string_view text)
  {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < text.size()) {
      size_t b = i;
      while (i < text.size() && (is_word_char(text[i]) || text[i] == '.')) i++;
      if (i == b) i++;
      out.emplace_back(text.substr(b, i - b));
    }
    return out;
  }

  /* p as a whole number written plainly (no sign, no leading 0) */
  static std::optional<long> plain_whole(std::string_view p)
  {
    if (p.empty() || p.size() > 9 || (p.size() > 1 && p[0] == '0')) return std::nullopt;
    long v = 0;
    for (char c : p) {
      if (c < '0' || c > '9') return std::nullopt;
      v = v * 10 + (c - '0');
    }
    return v;
  }

  /* index expression a*j+b */
  static std::string affine(const std::string &j, long a, long b)
  {
    std::string t = a == 1 ? j : a == -1 ? "-" + j : std::to_string(a) + "*" + j;
    if (b > 0) t += "+" + std::to_string(b);
    if (b < 0) t += "-" + std::to_string(-b);
    return t;
  }

  /* An array's copies written back as one statement (docs/odex.md
     "Arrays"): texts the copies' own, values their index. A piece that
     differs between copies is the index (a number equal to it: j) or an
     element (a name x followed by a*j+b: x[a*j+b]), found from the
     copies themselves, the range written after the statement. When a
     piece differs otherwise (a number the index works out to, which .odex
     would compute rather than read, or text that is not an index), or
     the range counts down, the copies are written as they are. */
  std::string as_array(const std::vector<std::string> &texts, const std::vector<int> &values, const ArrayCopy &a)
  {
    std::string expanded;
    for (const std::string &t : texts) expanded += t;
    if (a.step <= 0 || values.size() < 2 || expanded.empty() || values.front() != a.lo || values.back() != a.hi)
      return expanded;
    for (size_t c = 0; c < values.size(); c++)
      if (values[c] != a.lo + static_cast<int>(c) * a.step) return expanded;
    std::vector<std::vector<std::string>> p;
    for (const std::string &t : texts) p.push_back(pieces(t));
    const size_t n = p[0].size();
    for (const std::vector<std::string> &q : p)
      if (q.size() != n) return expanded;
    /* the pieces that differ: an element's name (the index at slot i) or
       the index itself (empty name) */
    struct Varying {
      std::string name;
      long a = 1, b = 0;
    };
    std::map<size_t, Varying> varying;
    for (size_t i = 0; i < n; i++) {
      bool same = true;
      for (const std::vector<std::string> &q : p) same = same && q[i] == p[0][i];
      if (same) continue;
      std::vector<long> v;
      std::string name;
      for (size_t c = 0; c < p.size(); c++) {
        const std::string &q = p[c][i];
        size_t d = q.size();
        while (d > 0 && q[d - 1] >= '0' && q[d - 1] <= '9') d--;
        const std::string head = q.substr(0, d);
        std::optional<long> k = plain_whole(std::string_view(q).substr(d));
        if (!k || (c > 0 && head != name) || (d > 0 && !is_word_start(head[0]))) return expanded;
        name = head;
        v.push_back(*k);
      }
      const long dk = values[1] - values[0], dv = v[1] - v[0];
      if (dk == 0 || dv % dk != 0) return expanded;
      Varying x;
      x.name = name;
      x.a = dv / dk;
      x.b = v[0] - x.a * values[0];
      for (size_t c = 0; c < v.size(); c++)
        if (v[c] != x.a * values[c] + x.b) return expanded;
      if (name.empty() && (x.a != 1 || x.b != 0)) return expanded;
      varying[i] = x;
    }
    /* the index: a name no piece and no declared name has */
    std::set<std::string> used = taken_;
    for (const std::vector<std::string> &q : p)
      for (const std::string &w : q) used.insert(xpp::upper_case(w));
    for (const auto &[i, x] : varying) used.insert(xpp::upper_case(x.name));
    std::string j;
    for (const char *c : {"j", "i", "k", "n", "m", "jj", "kk", "idx"})
      if (!used.count(xpp::upper_case(c))) {
        j = c;
        break;
      }
    if (j.empty()) return expanded;
    std::string out;
    for (size_t i = 0; i < n; i++) {
      auto x = varying.find(i);
      if (x == varying.end()) out += p[0][i];
      else if (x->second.name.empty()) out += j;
      else out += x->second.name + "[" + affine(j, x->second.a, x->second.b) + "]";
    }
    while (!out.empty() && out.back() == '\n') out.pop_back();
    return xpp::format("{} for {} in {}..{}{}\n", out, j, a.lo, a.hi,
                       a.step == 1 ? std::string() : xpp::format(" by {}", a.step));
  }

  /* ---- names ---- */
  /* a model's name (upper case, as the reader keeps it) in the .odex */
  std::string name(const std::string &upper) const
  {
    const std::string spelled = spell_.of(upper);
    auto r = renames_.find(spelled);
    return r == renames_.end() ? spelled : r->second;
  }

  std::string arg_name(int fun, int i) const
  {
    const std::string spelled = spell_.arg(m_.ufun_names[fun], i, m_.ufun_args[fun][i]);
    auto r = renames_.find(spelled);
    return r == renames_.end() ? spelled : r->second;
  }

  /* every name the model declares, as spelled */
  std::vector<std::string> declared() const
  {
    std::vector<std::string> out;
    for (int k = STDSYM; k < s_.parser.nsym; k++) {
      const ExprSymbol &sym = s_.parser.symbols[k];
      const int type = sym.com / MAXTYPE;
      if (sym.name == "T" || sym.name.find('\'') != std::string::npos || sym.name.find('#') != std::string::npos) continue;
      if (type == CONTYPE || type == VARTYPE || type == UFUNTYPE || type == TABTYPE || type == NETTYPE)
        if (sym.name != "PI" && !sym.name.starts_with("MOUSE_")) out.push_back(spell_.of(sym.name));
    }
    for (int i = 0; i < m_.neq; i++) out.push_back(spell_.of(m_.uvar_names[i]));
    for (int f = 0; f < m_.nfun; f++)
      for (int i = 0; i < m_.narg_fun[f]; i++) out.push_back(spell_.arg(m_.ufun_names[f], i, m_.ufun_args[f][i]));
    return out;
  }

  /* a name .odex reserves (and, or, fun, ...) or cannot spell: its new
     name, asked for one by one with a suggestion (docs/odex.md,
     question 3); --auto takes every suggestion */
  void rename_reserved()
  {
    const std::vector<std::string> names = declared();
    std::set<std::string> taken;
    for (const std::string &n : names) taken.insert(xpp::upper_case(n));
    taken_ = taken;
    std::vector<std::string> unanswered;
    for (const std::string &n : names) {
      if (renames_.count(n) || (!is_reserved(n) && is_name(n))) continue;
      std::string base;
      for (char c : n) base += is_word_char(c) ? c : '_';
      if (base.empty() || !is_word_start(base[0]) || base[0] == '_') base = "x" + base;
      std::string suggestion = base + "_";
      for (int k = 2; taken.count(xpp::upper_case(suggestion)) || is_reserved(suggestion); k++)
        suggestion = base + "_" + std::to_string(k);
      std::string chosen = suggestion;
      if (!auto_) {
        std::optional<std::string> answer =
            ask_ ? ask_(xpp::format("`{}` is {} in .odex; its new name", n, is_reserved(n) ? "a reserved word" : "not a name"),
                        suggestion)
                 : std::nullopt;
        if (!answer) {
          unanswered.push_back(n);
          continue;
        }
        chosen = answer->empty() ? suggestion : *answer;
        if (!is_name(chosen) || is_reserved(chosen) || taken.count(xpp::upper_case(chosen)))
          refuse(xpp::format("`{}` cannot be the new name of `{}`: it is taken, reserved or not a name", chosen, n));
      }
      taken.insert(xpp::upper_case(chosen));
      taken_.insert(xpp::upper_case(chosen));
      renames_[n] = chosen;
    }
    if (!unanswered.empty()) {
      std::string list;
      for (const std::string &n : unanswered) list += (list.empty() ? "" : ", ") + n;
      refuse(xpp::format("these names need a new name in .odex and nobody can answer: {} (--convert --auto takes "
                         "the suggested names)", list));
    }
  }

  /* ---- programs back to expressions ---- */
  /* the expressions a program part leaves, each with where its own
     instructions start */
  struct Stack {
    explicit Stack(const std::string &model_file) : file(model_file) {}
    const std::string &file; /* the model's, for refuse_in */
    std::vector<Expr> e;
    std::vector<const int *> at;
    void push(Expr x, const int *from)
    {
      e.push_back(std::move(x));
      at.push_back(from);
    }
    /* the top, its start in *from */
    Expr pop(const int **from = nullptr)
    {
      if (e.empty()) refuse_in(file, "a program reads more than its stack holds");
      Expr x = std::move(e.back());
      if (from) *from = at.back();
      e.pop_back();
      at.pop_back();
      return x;
    }
  };

  std::vector<Expr> walk(const int *p, const int *end, int fun, bool positive_index = false) const
  {
    Stack st(m_.this_file);
    while (p < end) {
      const int *here = p;
      const int i = *p++;
      if (i == ENDEXP || i == ENDSUM) break;
      const int *first = here; /* where the result's instructions start */
      auto call = [&st, &first](std::string f, std::vector<Expr> args) {
        st.push(op_expr(Expr::Kind::Call, std::move(f), std::move(args)), first);
      };
      switch (i) {
      case NUMSYM:
        st.push(number_expr(xpp::expr::number_from_halves(p[1], p[0])), here);
        p += 2;
        continue;
      case ENDFUN: p++; continue;
      case MYIF: {
        const int j1 = *p++;
        const int *else_start = p + j1;
        const int j2 = else_start[-1];
        const int *after = else_start + j2;
        Expr c = st.pop(&first);
        Expr a = one(p, else_start - 2, fun, positive_index);
        Expr b = one(else_start, after - 1, fun, positive_index);
        st.push(op_expr(Expr::Kind::If, "", {std::move(c), std::move(a), std::move(b)}), first);
        p = after;
        positive_index = false;
        continue;
      }
      case SUMSYM: {
        Expr hi = st.pop(), lo = st.pop(&first);
        const int j = *p++;
        /* SUM_INDEX is an integer; nested sums overwrite it rather than
           restoring it, so do not assume the outer range after one. */
        const auto lower = leaf_value(lo);
        const bool index_positive = lower && *lower >= 1 && *lower <= std::numeric_limits<int>::max();
        Expr body = one(p, p + j, fun, index_positive);
        p += j;
        positive_index = false;
        Expr e = op_expr(Expr::Kind::Call, "sum", {std::move(body), std::move(lo), std::move(hi)});
        e.arg_names = {"", "from", "to"};
        st.push(std::move(e), first);
        continue;
      }
      case ENDDELAY: {
        Expr tau = st.pop(), v = st.pop(&first);
        call("delay", {std::move(v), std::move(tau)});
        continue;
      }
      case ENDDELSHFT: {
        Expr tau = st.pop(), sh = st.pop(), v = st.pop(&first);
        call("del_shft", {std::move(v), std::move(sh), std::move(tau)});
        continue;
      }
      case ENDSHIFT:
      case ENDISHIFT: {
        Expr sh = st.pop(), v = st.pop(&first);
        call(i == ENDSHIFT ? "shift" : "ishift", {std::move(v), std::move(sh)});
        continue;
      }
      case ENDSET: {
        Expr val = st.pop(), sh = st.pop(), v = st.pop(&first);
        call("set", {std::move(v), std::move(sh), std::move(val)});
        continue;
      }
      case INDXCOM: refuse("a vector's index @ has no .odex form yet");
      case RANDUNI:
      case RANDPOI: {
        Expr a = st.pop(&first);
        call(builtin(xpp::expr::random_com(i)), {std::move(a)});
        continue;
      }
      case RANDNORM: {
        Expr y = st.pop(), x = st.pop(&first);
        call(builtin(xpp::expr::random_com(i)), {std::move(x), std::move(y)});
        continue;
      }
      default: break;
      }
      const int type = i / MAXTYPE, in = i % MAXTYPE;
      switch (type) {
      case FUN1TYPE: {
        Expr a = st.pop(&first);
        if (in == 14) st.push(op_expr(Expr::Kind::Neg, "", {std::move(a)}), first);
        else if (in == 20) st.push(op_expr(Expr::Kind::Not, "", {std::move(a)}), first);
        else call(builtin(i), {std::move(a)});
        break;
      }
      case FUN2TYPE: {
        const int *divisor = nullptr;
        Expr y = st.pop(&divisor), x = st.pop(&first);
        static const std::map<int, std::string> ops = {
          {0, "+"}, {1, "-"}, {2, "*"}, {3, "/"}, {xpp::expr::IEEE_DIVIDE, "/"}, {5, "^"}, {8, "mod"}, {9, "and"},
          {10, "or"}, {11, ">"}, {12, "<"}, {13, "=="}, {14, ">="}, {15, "<="}, {16, "!="}};
        /* XPP's division: a divisor that can be 0 written with XPP's guard */
        if (in == 3 && !never_zero(y, divisor, here, positive_index)) y = guarded(std::move(y));
        auto op = ops.find(in);
        if (op != ops.end()) st.push(op_expr(Expr::Kind::Binary, op->second, {std::move(x), std::move(y)}), first);
        else call(builtin(i), {std::move(x), std::move(y)});
        break;
      }
      case CONTYPE:
      case SCONTYPE: st.push(constant(in), here); break;
      case VARTYPE:
      case SVARTYPE: st.push(variable(in), here); break;
      case UFUNTYPE: {
        const int n = *p++;
        if (static_cast<int>(st.e.size()) < n) refuse("a function call reads more than its stack holds");
        std::vector<Expr> args(n);
        for (int k = n; k-- > 0;) args[k] = st.pop(&first);
        if (n == 0) first = here;
        call(name(m_.ufun_names[in]), std::move(args));
        positive_index = false; /* a function may contain a sum */
        break;
      }
      case USTACKTYPE:
        if (fun < 0 || in >= m_.narg_fun[fun]) refuse("an argument read outside its function");
        st.push(name_expr(arg_name(fun, in)), here);
        break;
      case TABTYPE:
      case NETTYPE: {
        const std::string *n = sym_.find(i);
        if (!n) refuse("a table or network with no name");
        Expr x = st.pop(&first);
        call(name(*n), {std::move(x)});
        break;
      }
      case KERTYPE: st.push(kernel(in), here); break;
      default: refuse(xpp::format("an instruction ({}) with no .odex form", i));
      }
    }
    return std::move(st.e);
  }

  /* ---- XPP's division ---- */
  /* the value of the program part [b,e) when it reads only numbers and
     the model's constants (not a Wiener parameter, the animator's mouse,
     sum's index or a parameter an event sets) and draws no random
     number: the expression engine's own evaluation */
  std::optional<double> constant_value(const int *b, const int *e) const
  {
    std::vector<int> prog;
    for (const int *q = b; q < e; q++) {
      const int c = *q;
      prog.push_back(c);
      if (c == NUMSYM) {
        prog.push_back(q[1]);
        prog.push_back(q[2]);
        q += 2;
        continue;
      }
      if (c == MYIF || c == MYTHEN) {
        prog.push_back(q[1]);
        q++;
        continue;
      }
      if (c == MYELSE) continue;
      const int type = c / MAXTYPE, in = c % MAXTYPE;
      if (type == FUN1TYPE || type == FUN2TYPE) continue; /* ran, poisson and normal are RANDUNI ... */
      if (type == CONTYPE && constants_.count(in)) continue;
      return std::nullopt;
    }
    prog.push_back(ENDEXP);
    return evaluate(s_,prog.data());
  }

  /* a leaf's value: a number, or a constant of the model's */
  std::optional<double> leaf_value(const Expr &e) const
  {
    if (e.kind == Expr::Kind::Number) return e.value;
    if (e.kind == Expr::Kind::Name && !e.primed) {
      auto v = constant_values_.find(e.text);
      if (v != constant_values_.end()) return v->second;
    }
    return std::nullopt;
  }

  /* e > 0 (or NaN) whatever its variables: c+exp(x), c+x^2, cosh(x) */
  bool positive(const Expr &e) const
  {
    if (std::optional<double> v = leaf_value(e)) return *v > 0;
    if (e.kind == Expr::Kind::Call && e.text == "cosh") return true;
    if (e.kind == Expr::Kind::Call && e.text == "exp") return nonnegative(e.args[0]);
    if (e.kind == Expr::Kind::Binary && e.text == "+")
      return (positive(e.args[0]) && nonnegative(e.args[1])) || (nonnegative(e.args[0]) && positive(e.args[1]));
    return false;
  }

  /* e >= 0 (or NaN) whatever its variables */
  bool nonnegative(const Expr &e) const
  {
    if (std::optional<double> v = leaf_value(e)) return *v >= 0;
    if (positive(e)) return true;
    if (e.kind == Expr::Kind::Call && (e.text == "exp" || e.text == "abs" || e.text == "sqrt" || e.text == "heav"))
      return true;
    if (e.kind == Expr::Kind::Binary) {
      const Expr &a = e.args[0], &b = e.args[1];
      if (e.text == "^" && b.kind == Expr::Kind::Number && std::fmod(b.value, 2.0) == 0) return true;
      if (e.text == "*" && a.kind == Expr::Kind::Name && b.kind == Expr::Kind::Name && print(a) == print(b)) return true; /* x*x, no repeated random call */
      if (e.text == "+" || e.text == "*") return nonnegative(a) && nonnegative(b);
    }
    return false;
  }

  /* the divisor y (its instructions [from,to)) is never 0: a constant
     that is not 0, a positive form, or a positive integer index product */
  bool never_zero(const Expr &y, const int *from, const int *to, bool positive_index) const
  {
    if (std::optional<double> v = constant_value(from, to)) return *v != 0;
    if (const auto v = leaf_value(y)) return *v != 0;
    if (positive(y)) return true;
    const auto index_product = [](const auto &self, const Expr &e) -> bool {
      if (e.kind == Expr::Kind::Name) return e.primed && e.text == "i";
      return e.kind == Expr::Kind::Binary && e.text == "*" && self(self, e.args[0]) && self(self, e.args[1]);
    };
    if (positive_index) {
      /* Products of integer indices cannot underflow to zero. Do not
         extend this rule to arbitrary nonzero floating-point factors. */
      return index_product(index_product, y);
    }
    return false;
  }

  /* y as XPP divides by it: if y then y else ZERO_DIVISOR (a zero y,
     -0 too, is false; NaN is true, as XPP's guard lets it through) */
  Expr guarded(Expr y) const
  {
    uses_guard_ = true;
    return op_expr(Expr::Kind::Call, guard_name_, {std::move(y)});
  }

  Expr one(const int *p, const int *end, int fun, bool positive_index = false) const
  {
    std::vector<Expr> st = walk(p, end, fun, positive_index);
    if (st.size() != 1) refuse("a program part leaves other than one value");
    return std::move(st[0]);
  }

  Expr expr_of(const std::vector<int> &prog, int fun = -1) const
  {
    return one(prog.data(), prog.data() + prog.size(), fun);
  }

  /* a built-in function's name, the first the symbol table gives com */
  std::string builtin(int com) const
  {
    for (int k = 0; k < STDSYM; k++)
      if (s_.parser.symbols[k].com == com) return xpp::lower_case(s_.parser.symbols[k].name);
    refuse(xpp::format("a built-in ({}) with no name", com));
  }

  Expr constant(int index) const
  {
    const std::string *n = sym_.find(COM(CONTYPE, index));
    if (!n) refuse(xpp::format("constant {} has no name", index));
    if (*n == "PI" || n->starts_with("MOUSE_")) return name_expr(xpp::lower_case(*n));
    if (*n == "I'") return name_expr("i", true);
    return name_expr(name(*n));
  }

  Expr variable(int index) const
  {
    if (index == 0) return name_expr("t");
    const std::string *n = sym_.find(COM(VARTYPE, index));
    if (!n) refuse(xpp::format("variable {} has no name", index));
    if (!n->empty() && n->back() == '\'') {
      const std::string base = n->substr(0, n->size() - 1);
      return name_expr(base == "T" ? "t" : name(base), true);
    }
    return name_expr(name(*n));
  }

  /* int{...} and int[mu]{...}: volterra(kernel, of=u, mu=m) */
  Expr kernel(int index) const
  {
    const KERNEL &k = m_.kernels[index];
    const bool conv = !k.kerexpr.empty();
    Expr e;
    e.kind = Expr::Kind::Call;
    e.text = "volterra";
    const std::vector<int> formula = k.formula.empty() ? compiled(k.expr) : k.formula;
    if (conv) {
      const std::vector<int> ker = k.kerform.empty() ? compiled(k.kerexpr) : k.kerform;
      e.args.push_back(expr_of(ker));
      e.arg_names.push_back("");
      e.args.push_back(expr_of(formula));
      e.arg_names.push_back("of");
    } else {
      e.args.push_back(expr_of(formula));
      e.arg_names.push_back("");
    }
    if (k.mu != 0) {
      e.args.push_back(number_expr(k.mu));
      e.arg_names.push_back("mu");
    }
    return e;
  }

  std::string text(const std::vector<int> &prog, int fun = -1) const { return print(expr_of(prog, fun)); }

  /* ---- statements ---- */
  /* a note on what the .ode's reader made of a statement, a comment
     above the statement */
  std::string noted(const std::string &note, std::string id, std::string severity = "warning")
  {
    if (place_.source.empty()) place_.source = model_source_line(m_, place_.file, place_.line);
    diagnostics_.push_back({place_, std::move(id), std::move(severity), note});
    return "# " + diagnostics_.back().message + "\n";
  }

  void option_note(std::string message, std::string id)
  {
    noted(message, std::move(id));
    option_notes_.push_back(diagnostics_.size() - 1);
  }

  /* a number the reader read from text with atof (b's value, its text
     as written): the value, with a note when the text was not that
     number alone */
  std::string read_number(const Binding &b)
  {
    char *end = nullptr;
    const std::string &text = b.value.text;
    const std::string t = std::string(xpp::trim_blanks(text));
    std::strtod(t.c_str(), &end);
    if (!t.empty() && end && *end == '\0') return print_number(b.value.value);
    pending_ += noted(xpp::format("{}={} in the .ode: XPP reads the number at its front, {}", b.name,
                                  text.empty() ? std::string("(nothing)") : text, print_number(b.value.value)), number_id_);
    return print_number(b.value.value);
  }

  std::string statement(const Statement &s)
  {
    place_ = model_place(m_, s.pos);
    expression_findings(s);
    pending_.clear();
    const std::string line = statement_line(s);
    return pending_ + line;
  }

  std::string statement_line(const Statement &s)
  {
    const int node = m_.node, fix = m_.fix_var;
    switch (s.kind) {
    case Statement::Kind::Ode:
    case Statement::Kind::Map:
    case Statement::Kind::Volterra: {
      const int k = nvar_++;
      const std::string x = name(xpp::upper_case(s.name));
      if (s.kind == Statement::Kind::Volterra) return x + "(t) = " + text(m_.programs[k]) + "\n";
      return x + "' = " + text(m_.programs[k]) + "\n";
    }
    case Statement::Kind::Fixed: {
      const int k = nfix_++;
      return name(xpp::upper_case(s.name)) + " = " + text(m_.programs[node + k]) + "\n";
    }
    case Statement::Kind::Aux: {
      std::string out;
      for (size_t b = 0; b < s.bindings.size(); b++) {
        const int k = naux_++;
        out += "aux " + name(m_.uvar_names[node + m_.nmarkov + k]) + " = " + text(m_.programs[node + fix + k]) + "\n";
      }
      return out;
    }
    case Statement::Kind::Fun: {
      const int f = nfun_++;
      std::string out = "fun " + name(m_.ufun_names[f]) + "(";
      for (int i = 0; i < m_.narg_fun[f]; i++) out += (i ? ", " : "") + arg_name(f, i);
      return out + ") = " + text(m_.ufun_programs[f], f) + "\n";
    }
    case Statement::Kind::Derived: {
      /* d = expr: the .odex's builder finds it reads only parameters,
         consts and pure functions, as this builder did (docs/odex.md
         question 9) */
      std::string out;
      for (const Binding &b : s.bindings) {
        const std::string d = name(xpp::upper_case(b.name));
        const std::string f = text(compiled(b.value.text));
        if (!s.parameters_only) {
          noted("XPP freezes this derived quantity when parameters change or a run starts.", "derived-frozen");
          refuse(xpp::format("!{} = {} reads t, a variable, a random function or a derived quantity after it: an "
                             ".odex quantity is worked out only when parameters change when it reads only "
                             "parameters, consts and pure functions (docs/odex.md question 9)", d, f));
        }
        out += d + " = " + f + "\n";
      }
      return out;
    }
    case Statement::Kind::Dae: return "0 = " + text(m_.aeqns[ndae_++].form) + "\n";
    case Statement::Kind::Solv: {
      const Model::AlgebraicVariable &a = m_.svars[nsol_++];
      return "solv " + name(xpp::upper_case(std::string(xpp::trim_blanks(a.name)))) + " = " + text(a.form) + "\n";
    }
    case Statement::Kind::InitNumbers: return s.text.empty() ? items(s) : initial(s);
    case Statement::Kind::History: return history(s);
    case Statement::Kind::Par:
    case Statement::Kind::Const:
    case Statement::Kind::Wiener: return items(s);
    case Statement::Kind::Table: return table(s);
    case Statement::Kind::Network: return network(s);
    case Statement::Kind::Markov: return markov();
    case Statement::Kind::Only: {
      std::string out = "only ";
      int n = 0;
      for (const std::string &w : s.names) out += (n++ ? ", " : "") + name(xpp::upper_case(w));
      return out + "\n";
    }
    case Statement::Kind::Vector: refuse("a vector statement has no .odex form yet");
    case Statement::Kind::Group: refuse("a group statement has no .odex form yet");
    case Statement::Kind::Set: return set_statement();
    case Statement::Kind::Boundary: {
      const Model::BoundaryCondition &bc = m_.bcs[nbc_++];
      return "boundary " + text(compiled(std::string(bc.string.data()))) + "\n";
    }
    case Statement::Kind::Event: return event();
    case Statement::Kind::OptionFile: refuse("the options statement (a file of options) has no .odex form yet");
    default: return std::string();
    }
  }

  /* x(0)=formula (s.text): x's initial value, and its history when the
     model has delays */
  std::string initial(const Statement &s)
  {
    const std::string upper = xpp::upper_case(s.bindings[0].name);
    const int i = find_user_name(m_, ICBOX, upper);
    if (i < 0) refuse(xpp::format("{}(0): no such variable", s.bindings[0].name));
    const double z = m_.default_ic[i];
    char *end = nullptr;
    const std::string t = std::string(xpp::trim_blanks(s.text));
    std::strtod(t.c_str(), &end);
    if (t.empty() || !end || *end != '\0')
      pending_ += noted(xpp::format("{}(0)={} in the .ode: XPP starts {} at {} (the number at the formula's front; the "
                                    "formula is only {}'s history for delays)",
                                    name(upper), s.text, name(upper), print_number(z), name(upper)), "initcond-formula");
    return "init " + name(upper) + " = " + print_number(z) + "\n";
  }

  /* x(0)=formula's history: what a delay reads before the start */
  std::string history(const Statement &s)
  {
    std::string out;
    for (const Binding &b : s.bindings) {
      const std::string upper = xpp::upper_case(b.name);
      const int i = find_user_name(m_, ICBOX, upper);
      if (m_.ndelays > 0 && i >= 0 && i < m_.node)
        out += "history " + name(upper) + " = " + text(compiled(b.value.text)) + "\n";
    }
    return out;
  }

  /* the .ode's array initial values, x[j1..j2](0)=formula: one init each,
     evaluated where XPP evaluates them, once the model is set up */
  std::string array_initials()
  {
    std::string out;
    const std::vector<ArrayInitialValue> all = array_initial_values(m_);
    for (size_t i = 0; i < all.size();) {
      size_t e = i;
      std::vector<std::string> texts;
      std::vector<int> values;
      for (; e < all.size() && all[e].group == all[i].group; e++) {
        const std::string upper = xpp::upper_case(all[e].var);
        if (find_user_name(m_, ICBOX, upper) < 0) continue;
        texts.push_back("init " + name(upper) + " = " + text(compiled(all[e].formula)) + "\n");
        values.push_back(all[e].j);
      }
      ArrayCopy a;
      a.group = all[i].group;
      if (!values.empty()) {
        a.lo = values.front();
        a.hi = values.back();
      }
      if (!texts.empty()) out += texts.size() > 1 ? as_array(texts, values, a) : texts[0];
      i = e;
    }
    return out;
  }

  /* par, init, number and wiener's name=value items */
  std::string items(const Statement &s)
  {
    number_id_ = s.kind == Statement::Kind::InitNumbers ? "init-value" : "declaration-value";
    std::string out;
    const std::vector<Binding> &list = s.bindings;
    if (s.kind == Statement::Kind::Wiener) {
      out = "wiener ";
      for (size_t k = 0; k < list.size(); k++) out += (k ? ", " : "") + name(xpp::upper_case(list[k].name));
      return out + "\n";
    }
    if (s.kind == Statement::Kind::Const) { /* .ode's number: a const */
      out = "const ";
      for (size_t k = 0; k < list.size(); k++)
        out += (k ? ", " : "") + name(xpp::upper_case(list[k].name)) + " = " + read_number(list[k]);
      return out + "\n";
    }
    out = s.kind == Statement::Kind::Par ? "par " : "init ";
    for (size_t k = 0; k < list.size(); k++) {
      const std::string value = read_number(list[k]);
      out += (k ? ", " : "") + name(xpp::upper_case(list[k].name)) + " = " + value;
    }
    return out + "\n";
  }

  std::string set_statement()
  {
    const Model::InternalSet &set = m_.intern_sets[nset_++];
    std::string out = "set " + name(xpp::upper_case(set.name)) + " = ";
    int n = 0;
    for (const auto &[key, value] : option_items(set.does, true)) {
      out += (n++ ? ", " : "") + setting_name(key) + " = " + setting_value(key, value);
    }
    return out + "\n";
  }

  /* a set's or a comment's name: a variable's or a parameter's (spelled)
     or an option's */
  std::string setting_name(const std::string &key) const
  {
    const std::string upper = xpp::upper_case(key);
    if (find_user_name(m_, ICBOX, upper) >= 0 || find_user_name(m_, PARAMBOX, upper) >= 0) return name(upper);
    return key;
  }

  /* a set's value as the .ode applies it: a number (atof's), or a name */
  std::string setting_value(const std::string &key, const std::string &value)
  {
    char *end = nullptr;
    const double z = std::strtod(value.c_str(), &end);
    if (!value.empty() && end && *end == '\0') return print_number(z);
    if (is_name(value)) {
      const std::string upper = xpp::upper_case(value);
      return find_user_name(m_, ICBOX, upper) >= 0 || find_user_name(m_, PARAMBOX, upper) >= 0 ? name(upper) : value;
    }
    pending_ += noted(xpp::format("{}={} in the .ode: XPP reads the number at its front, {}", key, value,
                                  print_number(std::atof(value.c_str()))), "set-value");
    return print_number(std::atof(value.c_str()));
  }

  std::string event()
  {
    const Model::GlobalFlag &f = m_.flags[nflag_++];
    std::string out = xpp::format("event {} {}", f.sign, text(f.comcond));
    for (int e = 0; e < f.nevents; e++) {
      const std::string upper = xpp::upper_case(f.lhsname[e]);
      std::string target = f.type[e] == 2 ? "out_put" : f.type[e] == 3 ? "arret" : name(upper);
      if (f.type[e] == 0 && xpp::equal_ignoring_case(f.lhsname[e], "no_interp")) target = "no_interp";
      out += ", " + target + " = " + text(f.comrhs[e]);
    }
    return out + "\n";
  }

  std::string table(const Statement &s)
  {
    const int k = ntab_++;
    const TABULAR &t = s_.tables[k];
    const std::string n = name(xpp::upper_case(t.name.empty() ? s.name : t.name));
    if (t.flag == 2) {
      /* the formula's variable is t; its values at n points on [xlo,xhi] */
      return xpp::format("table {} {}, n={}, lo={}, hi={}\n", n, text(compiled(t.filename)), t.n,
                         print_number(t.xlo), print_number(t.xhi));
    }
    if (t.filename.find('"') != std::string::npos) refuse(xpp::format("the table file {} has a quote in its name", t.filename));
    return "table " + n + " \"" + t.filename + "\"\n";
  }

  /* a network: its kind and arguments as written, each name spelled */
  std::string network(const Statement &s)
  {
    std::string rhs = std::string(xpp::trim_blanks(s.text));
    const size_t open = rhs.find('(');
    const size_t close = rhs.rfind(')');
    if (open == std::string::npos || close == std::string::npos || close < open)
      refuse(xpp::format("the network {} is not kind(arguments)", s.name));
    std::string out = "network " + name(xpp::upper_case(s.name)) + " = " + xpp::lower_case(rhs.substr(0, open)) + "(";
    const std::string args = rhs.substr(open + 1, close - open - 1);
    int depth = 0;
    std::string arg;
    int n = 0;
    auto flush = [&]() {
      std::string a = std::string(xpp::trim_blanks(arg));
      if (is_name(a) && !is_reserved(xpp::lower_case(a))) {
        const std::string upper = xpp::upper_case(a);
        a = (find_user_name(m_, ICBOX, upper) >= 0 || find_lookup(s_, upper) >= 0 || get_var_index(s_, upper) >= 0) ? name(upper)
                                                                                                      : xpp::lower_case(a);
      }
      out += (n++ ? ", " : "") + a;
      arg.clear();
    };
    for (char c : args) {
      if (c == '(' || c == '{') depth++;
      if (c == ')' || c == '}') depth--;
      if (c == ',' && depth == 0) flush();
      else arg += c;
    }
    flush();
    return out + ")\n";
  }

  std::string markov()
  {
    const Model::MarkovChain &mc = m_.markov[nmark_++];
    std::string out = xpp::format("markov {} {}\n", name(xpp::upper_case(mc.name)), mc.nstates);
    for (int r = 0; r < mc.nstates; r++) {
      out += "  ";
      for (int c = 0; c < mc.nstates; c++) {
        const int l = r * mc.nstates + c;
        std::string cell;
        if (mc.type == 0) cell = text(mc.command[l].empty() ? compiled(mc.trans[l]) : mc.command[l]);
        else cell = print_number(mc.fixed[l]);
        out += "{" + cell + "} ";
      }
      out += "\n";
    }
    return out;
  }

  /* ---- what is not a statement: comments, options ---- */
  std::string comments()
  {
    std::string out;
    for (const std::string &raw : m_.source) {
      const size_t comment = ode_comment_start(raw);
      if (comment != std::string::npos)
        out += std::string(xpp::trim_blanks(std::string_view(raw).substr(comment))) + "\n";
    }
    for (const Model::Comment &c : m_.comments) {
      std::string t = c.text;
      if (c.aflag) {
        std::string action;
        for (const auto &[key, value] : option_items(c.action, true))
          action += (action.empty() ? "" : ",") + setting_name(key) + "=" + value;
        t = "{" + action + "} " + std::string(xpp::trim_blanks(t.starts_with("* ") ? t.substr(2) : t));
      }
      t = std::string(xpp::trim_blanks(t));
      if (t.find('"') != std::string::npos) {
        for (char &ch : t)
          if (ch == '"') ch = '\'';
      }
      out += "\"" + t + "\"\n";
    }
    return out;
  }

  /* the @ lines, each option as the .ode applies it: an option with
     spaces around its = is dropped (XPP ignores it), a value cut where a
     number ends written as that number, a method by its name */
  std::string options()
  {
    std::string out;
    const bool map = disc(m_) != 0;
    for (const xpp::Model::OptionLine &line : m_.options) {
      place_ = line.where;
      std::string items;
      std::vector<std::string> ignored;
      for (const auto &[key, value] : option_items(line.text, false, &ignored)) {
        const bool meth = xpp::equal_ignoring_case(key.substr(0, std::min<size_t>(key.size(), 4)), "meth");
        if (map && meth) continue;
        items += (items.empty() ? "" : ", ") + key + "=" + (meth ? method_name(key, value) : option_value(key, value));
      }
      if (!ignored.empty()) {
        std::string dropped;
        for (const std::string &token : ignored) dropped += (dropped.empty() ? "" : " ") + token;
        std::string names;
        std::set<std::string> seen;
        for (size_t i = 0; i + 1 < ignored.size(); ++i) {
          if (ignored[i + 1] != "=") continue;
          const std::string &name = ignored[i];
          if (!seen.insert(name).second) continue;
          names += (names.empty() ? "" : ", ") + name;
        }
        option_note(xpp::format("@ {} in the .ode: XPP ignores an option with spaces around its ={}", dropped,
                                            names.empty() ? "" : ", so " + names + " keep their values"), "option");
      }
      if (!items.empty()) out += "@ " + items + "\n";
    }
    /* a map ((t+1)=, or a .dis file) runs with the discrete method */
    if (map) {
      for (const Statement &s : m_.statements)
        if (s.kind == Statement::Kind::Ode || s.kind == Statement::Kind::Map) {
          place_ = model_place(m_, s.pos);
          break;
        }
      out += noted("the .ode is a map: x' = f with the discrete method is x(t+1) = f", "discrete-map", "info") + "@ meth=discrete\n";
    }
    return out;
  }

  /* the method XPP runs for @ key=value: its first letter's (the load
     applied the same), noted when the whole value names another one */
  std::string method_name(const std::string &key, const std::string &value)
  {
    const auto letter = xpp::pick_method(m_, value.substr(0, 1), xpp::Place{m_.this_file});
    if (!letter) refuse(letter.error().what);
    const char *name = xpp::solver_info(*letter).name;
    const auto whole = xpp::pick_method(m_, value, xpp::Place{m_.this_file});
    if (whole && *whole != *letter)
      option_note(xpp::format("@ {}={} in the .ode: XPP reads a method by its first letter, {}", key, value, name), "method-first-letter");
    return name;
  }

  std::string option_value(const std::string &key, const std::string &value)
  {
    char *end = nullptr;
    std::strtod(value.c_str(), &end);
    const char c = value[0];
    if ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+') {
      if (end && *end == '\0') return value;
    } else {
      bool ok = true;
      for (char k : value)
        if (!is_word_char(k) && k != '.' && k != '-' && k != '/' && k != '\\' && k != ':') ok = false;
      if (ok) {
        const std::string upper = xpp::upper_case(value);
        return find_user_name(m_, ICBOX, upper) >= 0 || find_user_name(m_, PARAMBOX, upper) >= 0 ? name(upper) : value;
      }
    }
    const std::string cut = print_number(std::atof(value.c_str()));
    option_note(xpp::format("@ {}={} in the .ode: XPP reads the number at its front, {}", key, value, cut), "option-value");
    return cut;
  }

  /* ---- around the statements ---- */
  std::string header()
  {
    std::string out = xpp::format("# {}: converted from {} by xppautX --convert (docs/odex.md)\n",
                                  base_name(odex_name(m_.this_file)), base_name(m_.this_file));
    for (const auto &[from, to] : renames_) {
      place_ = model_place(m_, xpp::upper_case(from));
      const bool keyword = code_of(place_.source).starts_with(from + "=") || code_of(place_.source).starts_with(from + " =");
      out += noted(xpp::format("renamed: {} is {} here (.odex reserves {})", from, to, from), keyword ? "keyword-name" : "reserved-word");
    }
    if (!spell_.respelled().empty()) {
      std::string lines;
      for (int n : spell_.respelled()) lines += (lines.empty() ? "" : ", ") + std::to_string(n);
      const std::string message = "names written as their declarations spell them (.odex names have case); the .ode's lines " + lines;
      for (int n : spell_.respelled()) {
        place_ = Place{m_.this_file, n, 0, model_source_line(m_, m_.this_file, n)};
        /* source is flattened across includes; statement places retain
           the physical file and line. Keep the old comment's line list. */
        for (const Statement &s : m_.statements) {
          const Place at = model_place(m_, s.pos);
          if (trim_blanks(at.source) == trim_blanks(m_.source[static_cast<size_t>(n - 1)])) {
            place_ = at;
            break;
          }
        }
        const std::string comment = noted(message, "name-case");
        if (n == spell_.respelled().front()) out += comment;
      }
    }
    for (size_t n : option_notes_) out += "# " + diagnostics_[n].message + "\n";
    out += "\n";
    return out;
  }

  /* what followed the .ode's done, kept as comments */
  std::string after_done() const
  {
    xpp::LineReader lr(m_.this_file.c_str());
    if (!lr) return std::string();
    bool done = false;
    std::string out;
    while (std::optional<std::string_view> line = lr.next()) {
      if (done) {
        out += "# " + std::string(*line) + "\n";
        continue;
      }
      std::string t = std::string(xpp::trim_blanks(*line));
      if (t.empty() || !(t[0] == 'd' || t[0] == 'D')) continue;
      /* the reader's done: a word starting with d, no = ' / ( after it */
      const std::string w = leading_word(t);
      std::string_view rest = std::string_view(t).substr(w.size());
      size_t k = 0;
      while (k < rest.size() && rest[k] == ' ') k++;
      if (rest.empty() || (rest[0] == ' ' && (k >= rest.size() || rest[k] != '='))) done = true;
    }
    if (out.find_first_not_of("# \n\r\t") == std::string::npos) return std::string();
    return "\n# what followed the .ode's done:\n" + out;
  }

  static std::string base_name(const std::string &path)
  {
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
  }

  xpp::Model &m_;
  xpp::Session &s_;
  Spelling spell_;
  Symbols sym_;
  bool auto_;
  const Ask &ask_;
  std::map<std::string, std::string> renames_;
  /* every declared name and new name, in upper case (an array's index is
     none of them) */
  std::set<std::string> taken_;
  std::string guard_name_;
  mutable bool uses_guard_ = false;
  std::set<int> constants_;
  std::map<std::string, double> constant_values_;
  std::vector<size_t> option_notes_;
  std::vector<Diagnostic> &diagnostics_;
  Place place_;
  std::string number_id_;
  std::set<std::pair<std::string, int>> scanned_;
  std::string pending_;
  int nvar_ = 0, nfix_ = 0, naux_ = 0, nfun_ = 0, ndae_ = 0, nsol_ = 0, nbc_ = 0, nflag_ = 0, nset_ = 0,
      ntab_ = 0, nmark_ = 0;
};

} // namespace

std::string odex_name(const std::string &ode)
{
  const size_t slash = ode.find_last_of("/\\");
  const size_t dot = ode.rfind('.');
  if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) return ode.substr(0, dot) + std::string(extension);
  return ode + std::string(extension);
}

namespace {

/* Compare the imported Model with its conversion: IEEE division read as
   .ode division, and calls to the verified generated guard erased. */
struct Fingerprint {
  std::vector<std::vector<int>> programs;
  std::vector<double> values;
  bool operator==(const Fingerprint &) const = default;
};

std::vector<int> normalized(const std::vector<int> &prog, int guard = -1);

std::vector<int> normalized(const std::vector<int> &prog, int guard)
{
  std::vector<int> out;
  for (size_t i = 0; i < prog.size(); i++) {
    const int c = prog[i];
    if (guard >= 0 && c == COM(UFUNTYPE, guard) && i + 1 < prog.size() && prog[i + 1] == 1) {
      i++; /* guard's argument is already on the stack, evaluated once */
      continue;
    }
    /* Removing a division guard also shortens its containing branch or
       sum. Recompute those jumps from the normalized bodies. */
    if (c == MYIF || c == SUMSYM) {
      const size_t end = i + 2 + static_cast<size_t>(prog[i + 1]);
      const size_t body_end = c == MYIF ? end - 2 : end;
      const std::vector<int> body = normalized(std::vector<int>(prog.begin() + static_cast<std::ptrdiff_t>(i + 2),
                                                               prog.begin() + static_cast<std::ptrdiff_t>(body_end)), guard);
      out.push_back(c);
      out.push_back(static_cast<int>(body.size()) + (c == MYIF ? 2 : 0));
      out.insert(out.end(), body.begin(), body.end());
      if (c == MYIF) {
        const size_t after = end + static_cast<size_t>(prog[end - 1]);
        const std::vector<int> otherwise = normalized(std::vector<int>(prog.begin() + static_cast<std::ptrdiff_t>(end),
                                                                      prog.begin() + static_cast<std::ptrdiff_t>(after - 1)), guard);
        out.push_back(MYTHEN);
        out.push_back(static_cast<int>(otherwise.size()) + 1);
        out.insert(out.end(), otherwise.begin(), otherwise.end());
        out.push_back(MYELSE);
        i = after - 1;
      } else i = end - 1;
      continue;
    }
    out.push_back(c == COM(FUN2TYPE, xpp::expr::IEEE_DIVIDE) ? COM(FUN2TYPE, 3) : c);
    if (c == ENDEXP) break;
    /* the operands that follow an instruction, copied as they are */
    const int operands = c == NUMSYM ? 2 : (c == MYIF || c == MYTHEN || c == SUMSYM || c == ENDFUN || c / MAXTYPE == UFUNTYPE) ? 1 : 0;
    for (int k = 0; k < operands && i + 1 < prog.size(); k++) out.push_back(prog[++i]);
  }
  return out;
}

Fingerprint fingerprint(const xpp::Session &s, int guard = -1)
{
  const xpp::Model &m = s.model();
  Fingerprint f;
  for (int c : {m.neq, m.node, m.nmarkov, m.fix_var, m.nupar, m.nfun - (guard >= 0 ? 1 : 0), m.nflags, m.naeqn, m.nsvar, m.nkernel, m.nwiener})
    f.values.push_back(c);
  for (int i = 0; i < m.node + m.fix_var + m.neq - m.node - m.nmarkov; i++) f.programs.push_back(normalized(m.programs[i], guard));
  for (int i = 0; i < m.nfun; i++) if (i != guard) f.programs.push_back(normalized(m.ufun_programs[i], guard));
  for (int j = 0; j < m.nflags; j++) {
    f.programs.push_back(normalized(m.flags[j].comcond, guard));
    for (int e = 0; e < m.flags[j].nevents; e++) f.programs.push_back(normalized(m.flags[j].comrhs[e], guard));
  }
  for (int i = 0; i < m.naeqn; i++) f.programs.push_back(normalized(m.aeqns[i].form, guard));
  for (int i = 0; i < m.nsvar; i++) f.programs.push_back(normalized(m.svars[i].form, guard));
  for (int i = 0; i < m.nmarkov; i++)
    for (const std::vector<int> &c : m.markov[i].command) f.programs.push_back(normalized(c, guard));
  for (int i = 0; i < m.nupar; i++) f.values.push_back(m.default_val[i]);
  for (int i = 0; i < m.node + m.nmarkov; i++) f.values.push_back(s.last_ic[i]);
  return f;
}

} // namespace

int check_file(const std::string &file)
{
  std::vector<Diagnostic> diagnostics;
  std::optional<Error> error;
  LogCapture capture;
  if (is_odex(file)) {
    std::string program = "xppautX", path = file;
    char *argv[] = {program.data(), path.data(), nullptr};
    const Result<> loaded = inspect_model(2, argv, load_eqn, {});
    if (!loaded) error = loaded.error();
  } else {
    const Result<std::string> converted = convert_text(file, true, {}, {}, &diagnostics);
    if (!converted) error = converted.error();
  }
  capture.clear();
  if (error) diagnostics.push_back({error->place, "load-error", "error", error->what});
  std::string json = "{\"file\":";
  json_append_string(json, file);
  json += ",\"diagnostics\":[";
  bool warning = false, first = true;
  for (const Diagnostic &d : diagnostics) {
    if (!first) json += ',';
    first = false;
    warning = warning || d.severity == "warning";
    json += xpp::format("{{\"line\":{},\"col\":{},\"source\":", d.place.line, d.place.col);
    json_append_string(json, d.place.source);
    json += ",\"id\":";
    json_append_string(json, d.id);
    json += ",\"severity\":";
    json_append_string(json, d.severity);
    json += ",\"message\":";
    json_append_string(json, d.message);
    json += '}';
  }
  json += "]}\n";
  /* This command owns stdout, independently of model quiet/log settings. */
  std::printf("%s", json.c_str());
  return error ? 2 : warning ? 1 : 0;
}

void show_diagnostics(const std::vector<Diagnostic> &diagnostics)
{
  for (const Diagnostic &d : diagnostics) {
    const std::string message = Error{"check", d.message, d.place}.text();
    xpp::log(XPP_LOG_WARN, "{} [{}: {}]\n", message, d.severity, d.id);
  }
}

Result<std::string> convert_text(const std::string &ode, bool auto_answer, const Ask &ask,
                               const std::vector<std::string> &includes, std::vector<Diagnostic> *diagnostics)
{
  if (is_odex(ode)) return xpp::fail_reading("convert", "is .odex already", ode);
  std::string arg0 = "xppautX", model = ode;
  std::vector<std::string> args{arg0, model};
  for (const std::string &include : includes) args.insert(args.end(), {"--include", include});
  std::vector<char *> argv;
  for (std::string &a : args) argv.push_back(a.data());
  argv.push_back(nullptr);
  std::string text;
  Fingerprint before;
  std::string guard_name;
  SavedModel saved;
  const OdeAsOdex as_odex;
  std::vector<Diagnostic> findings;
  std::vector<Diagnostic> &output = diagnostics ? *diagnostics : findings;
  Result<> imported = xpp::inspect_model(static_cast<int>(args.size()), argv.data(), [](Session &s) {
    const std::string &file = s.model().this_file;
    xpp::Load::at(file);
    std::string bytes;
    if (!read_model_file(s.model(), file, bytes) || !is_model_text(bytes))
      model_failed(Error{"convert", "cannot be read as model text", Place{file}});
    xpp::UniqueFile input = open_model_file(s.model(), file);
    if (!input) model_failed(Error{"convert", "cannot be read", Place{file}});
    get_eqn(s, input.get());
  }, [&](Session &s) -> std::optional<Error> {
    try {
      Converter converter(s, auto_answer, ask, output);
      text = converter.run();
      guard_name = converter.guard_name();
      before = fingerprint(s);
      saved = SavedModel{odex_name(ode), s.model().files};
    } catch (Error &e) {
      if (e.place.file.empty()) e.place = Place{ode};
      return e;
    }
    return std::nullopt;
  });
  if (!imported) return std::unexpected(imported.error());
  model = odex_name(ode);
  argv = {arg0.data(), model.data(), nullptr};
  saved.files.push_back({model, text});
  Result<> checked = xpp::inspect_model(2, argv.data(), load_eqn,
    [&](Session &s) -> std::optional<Error> {
      /* run() writes the guard last, so every other function keeps its
         .ode number and the programs' calls compare as they are */
      const int guard = guard_name.empty() ? -1 : s.model().nfun - 1;
      /* Check the helper's compiled guard before erasing its calls from
         fingerprints; function storage is padded beyond ENDEXP. */
      constexpr size_t guard_program_size = 13; /* argument, conditional, number and function terminator */
      bool guard_valid = guard_name.empty() || guard >= 0;
      if (guard >= 0) {
        const auto program = normalized(s.model().ufun_programs[guard]);
        guard_valid = s.model().ufun_names[guard] == xpp::upper_case(guard_name) && s.model().narg_fun[guard] == 1
            && program.size() == guard_program_size && program[0] == COM(USTACKTYPE, 0) && program[1] == MYIF && program[2] == 3
            && program[3] == COM(USTACKTYPE, 0) && program[4] == MYTHEN && program[5] == 4 && program[6] == NUMSYM
            && xpp::expr::number_from_halves(program[8], program[7]) == xpp::expr::ZERO_DIVISOR
            && program[9] == MYELSE && program[10] == ENDFUN && program[11] == 1 && program[12] == ENDEXP;
      }
      if (!guard_valid || !(fingerprint(s, guard) == before))
        return Error{"convert", xpp::format("does not compile to what {} did: a bug in the converter", ode), Place{model}};
      return std::nullopt;
    }, &saved);
  if (!checked) return std::unexpected(checked.error());
  return text;
}

namespace {

Result<> write_conversion(const std::string &out, std::string_view text)
{
  /* Writer's diagnostic is rendered once by the command that opened it. */
  xpp::LogCapture messages;
  xpp::Writer w = xpp::Writer::binary(out);
  if (!w || !w.write(text) || !w.commit()) {
    messages.clear();
    return xpp::fail_reading("convert", "cannot be written", out);
  }
  return {};
}

} // namespace

Result<bool> open_ode(const std::string &ode, bool silent, const std::vector<std::string> &includes,
                      std::vector<Diagnostic> &diagnostics)
{
  const Ask ask = [&ode](const std::string &question, const std::string &suggestion) -> std::optional<std::string> {
    std::string answer = suggestion;
    if (!xpp::new_string(question, answer)) throw Error{"open", "model open cancelled", Place{ode}};
    return answer;
  };
  Result<std::string> text = convert_text(ode, silent, ask, includes, &diagnostics);
  if (!text) return std::unexpected(text.error());
  const std::string out = odex_name(ode);
  if (xpp::files::exists(out)) {
    std::string existing;
    if (!xpp::read_bytes(out, existing)) return xpp::fail_reading("convert", "cannot be read", out);
    if (xpp::split_lines(existing) == xpp::split_lines(*text)) return false; /* the same text: opened as it is */
    if (silent)
      return xpp::fail_reading("convert", xpp::format("differs from the conversion of {}; neither file was changed", ode), out);
    const int choice = xpp::file_replace_choice(true);
    if (choice == 'n') return false;
    if (choice != 'y') return xpp::fail_reading("convert", "model open cancelled", ode);
  }
  Result<> written = write_conversion(out, *text);
  if (!written) return std::unexpected(written.error());
  return true;
}

int convert_file(const std::string &ode, bool auto_answer, const Ask &ask)
{
  std::vector<Diagnostic> diagnostics;
  Result<std::string> text = convert_text(ode, auto_answer, ask, {}, &diagnostics);
  show_diagnostics(diagnostics);
  const std::string out = odex_name(ode);
  Result<> written;
  if (!text) written = std::unexpected(text.error());
  else {
    if (xpp::files::exists(out) && !auto_answer) {
      const std::optional<std::string> answer = ask ? ask(out + " exists; replace it (yes/no)", "no") : std::nullopt;
      if (!answer || (*answer != "yes" && *answer != "y"))
        written = xpp::fail_reading("convert", "exists and was left as it is (--convert --auto replaces it)", out);
    }
    if (written) written = write_conversion(out, *text);
  }
  if (!written) {
    xpp::log(XPP_LOG_ERROR, "{}\n", written.error().text());
    return 1;
  }
  xpp::log(XPP_LOG_INFO, "wrote {}\n", out);
  return 0;
}

}
