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
#include "tabular.h"
#include "xpp_batch.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_log.h"
#include "xpp_util.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace xpp::odex {

namespace {

std::string trimmed(std::string_view s)
{
  size_t b = 0, e = s.size();
  while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n')) b++;
  while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n')) e--;
  return std::string(s.substr(b, e - b));
}

/* the leading word of s */
std::string leading_word(std::string_view s)
{
  size_t i = 0;
  while (i < s.size() && is_word_char(s[i])) i++;
  return std::string(s.substr(0, i));
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
      std::string line = trimmed(raw);
      if (line.empty() || line[0] == '#' || line[0] == '"' || line[0] == '@' || line[0] == '%') continue;
      declarations(line);
      for (const std::string &w : words_of(line)) first_.emplace(xpp::upper_case(w), w);
    }
    int n = 0;
    for (const std::string &raw : source) {
      n++;
      std::string line = trimmed(raw);
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
      const std::string inside = trimmed(rest.substr(1, close - 1));
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
  Converter(xpp::Session &s, bool auto_answer, const Ask &ask)
      : m_(s.model()), s_(s), spell_(m_.source), sym_(s_.parser), auto_(auto_answer), ask_(ask)
  {
  }

  std::string run()
  {
    rename_reserved();
    steady_constants();
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
    return out;
  }

private:
  /* the conversion stops: why */
  [[noreturn]] void refuse(std::string msg) const { refuse_in(m_.this_file, std::move(msg)); }

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
       sum's index, the animator's mouse, the Wiener parameters and the
       parameters an event sets */
    std::set<int> moving = {xpp::expr::SUM_INDEX};
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

  std::vector<Expr> walk(const int *p, const int *end, int fun) const
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
        Expr a = one(p, else_start - 2, fun);
        Expr b = one(else_start, after - 1, fun);
        st.push(op_expr(Expr::Kind::If, "", {std::move(c), std::move(a), std::move(b)}), first);
        p = after;
        continue;
      }
      case SUMSYM: {
        Expr hi = st.pop(), lo = st.pop(&first);
        const int j = *p++;
        Expr body = one(p, p + j, fun);
        p += j;
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
        if (in == 3 && !never_zero(y, divisor, here)) y = guarded(std::move(y));
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
      if (type == FUN1TYPE && in != 19 && in != 24) continue; /* not ran, poisson */
      if (type == FUN2TYPE && in != 17) continue;             /* not normal */
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
      if (e.text == "*" && print(a) == print(b)) return true; /* x*x */
      if (e.text == "+" || e.text == "*") return nonnegative(a) && nonnegative(b);
    }
    return false;
  }

  /* the divisor y (its instructions [from,to)) is never 0: a constant
     that is not 0, or a sum positive by its form */
  bool never_zero(const Expr &y, const int *from, const int *to) const
  {
    if (std::optional<double> v = constant_value(from, to)) return *v != 0;
    return positive(y);
  }

  /* y as XPP divides by it: if y then y else ZERO_DIVISOR (a zero y,
     -0 too, is false; NaN is true, as XPP's guard lets it through) */
  static Expr guarded(Expr y)
  {
    Expr cond = y;
    return op_expr(Expr::Kind::If, "", {std::move(cond), std::move(y), number_expr(xpp::expr::ZERO_DIVISOR)});
  }

  Expr one(const int *p, const int *end, int fun) const
  {
    std::vector<Expr> st = walk(p, end, fun);
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
  static std::string noted(const std::string &note) { return "# " + note + "\n"; }

  /* a number the reader read from text with atof (b's value, its text
     as written): the value, with a note when the text was not that
     number alone */
  std::string read_number(const Binding &b)
  {
    char *end = nullptr;
    const std::string &text = b.value.text;
    const std::string t = trimmed(text);
    std::strtod(t.c_str(), &end);
    if (!t.empty() && end && *end == '\0') return print_number(b.value.value);
    pending_ += noted(xpp::format("{}={} in the .ode: XPP reads the number at its front, {}", b.name,
                                  text.empty() ? std::string("(nothing)") : text, print_number(b.value.value)));
    return print_number(b.value.value);
  }

  std::string statement(const Statement &s)
  {
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
        if (!s.parameters_only)
          refuse(xpp::format("!{} = {} reads t, a variable, a random function or a derived quantity after it: an "
                             ".odex quantity is worked out only when parameters change when it reads only "
                             "parameters, consts and pure functions (docs/odex.md question 9)", d, f));
        out += d + " = " + f + "\n";
      }
      return out;
    }
    case Statement::Kind::Dae: return "0 = " + text(m_.aeqns[ndae_++].form) + "\n";
    case Statement::Kind::Solv: {
      const Model::AlgebraicVariable &a = m_.svars[nsol_++];
      return "solv " + name(xpp::upper_case(trimmed(a.name))) + " = " + text(a.form) + "\n";
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
    const std::string t = trimmed(s.text);
    std::strtod(t.c_str(), &end);
    if (t.empty() || !end || *end != '\0')
      pending_ += noted(xpp::format("{}(0)={} in the .ode: XPP starts {} at {} (the number at the formula's front; the "
                                    "formula is only {}'s history for delays)",
                                    name(upper), s.text, name(upper), print_number(z), name(upper)));
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
    const std::vector<ArrayInitialValue> all = array_initial_values();
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
                                  print_number(std::atof(value.c_str()))));
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
    std::string rhs = trimmed(s.text);
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
      std::string a = trimmed(arg);
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
    for (const Model::Comment &c : m_.comments) {
      std::string t = c.text;
      if (c.aflag) {
        std::string action;
        for (const auto &[key, value] : option_items(c.action, true))
          action += (action.empty() ? "" : ",") + setting_name(key) + "=" + value;
        t = "{" + action + "} " + trimmed(t.starts_with("* ") ? t.substr(2) : t);
      }
      t = trimmed(t);
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
     number ends written as that number */
  std::string options()
  {
    std::string out;
    const bool map = disc(m_.this_file) != 0;
    for (const std::string &line : m_.options) {
      std::string items;
      for (const auto &[key, value] : option_items(line, false)) {
        if (map && xpp::equal_ignoring_case(key.substr(0, std::min<size_t>(key.size(), 4)), "meth")) continue;
        items += (items.empty() ? "" : ", ") + key + "=" + option_value(key, value);
      }
      if (!items.empty()) out += "@ " + items + "\n";
    }
    /* a map ((t+1)=, or a .dis file) runs with the discrete method */
    if (map) out += noted("the .ode is a map: x' = f with the discrete method is x(t+1) = f") + "@ meth=discrete\n";
    return out;
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
    option_notes_.push_back(xpp::format("@ {}={} in the .ode: XPP reads the number at its front, {}", key, value, cut));
    return cut;
  }

  /* ---- around the statements ---- */
  std::string header()
  {
    std::string out = xpp::format("# {}: converted from {} by xppautX --convert (docs/odex.md)\n",
                                  base_name(odex_name(m_.this_file)), base_name(m_.this_file));
    for (const auto &[from, to] : renames_) out += xpp::format("# renamed: {} is {} here (.odex reserves {})\n", from, to, from);
    if (!spell_.respelled().empty()) {
      std::string lines;
      for (int n : spell_.respelled()) lines += (lines.empty() ? "" : ", ") + std::to_string(n);
      out += "# names written as their declarations spell them (.odex names have case); the .ode's lines " + lines + "\n";
    }
    for (const std::string &n : option_notes_) out += "# " + n + "\n";
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
      std::string t = trimmed(*line);
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
  std::set<int> constants_;
  std::map<std::string, double> constant_values_;
  std::vector<std::string> option_notes_;
  std::string pending_;
  int nvar_ = 0, nfix_ = 0, naux_ = 0, nfun_ = 0, ndae_ = 0, nsol_ = 0, nbc_ = 0, nflag_ = 0, nset_ = 0,
      ntab_ = 0, nmark_ = 0;
};

} // namespace

std::string odex_name(const std::string &ode)
{
  const size_t slash = ode.find_last_of("/\\");
  const size_t dot = ode.rfind('.');
  if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) return ode.substr(0, dot) + ".odex";
  return ode + ".odex";
}

std::string convert_model(xpp::Session &s, bool auto_answer, const Ask &ask)
{
  return Converter(s, auto_answer, ask).run();
}

namespace {

/* what a load compiled, to compare an .ode's Model with its .odex's: every
   program, with an .odex division read as .ode's (the one instruction the
   formats compile differently on purpose), and the model's own values */
struct Fingerprint {
  std::vector<std::vector<int>> programs;
  std::vector<double> values;
  bool operator==(const Fingerprint &) const = default;
};

std::vector<int> normalized(const std::vector<int> &prog);

/* the .odex's a/(if b then b else ZERO_DIVISOR), --convert's writing of
   an .ode division whose divisor can be 0, at the MYIF prog[i] (the
   condition b already in out): the .ode's own division of a by b
   instead, i at the division; false when this if is not that */
bool collapse_guard(const std::vector<int> &prog, size_t &i, std::vector<int> &out)
{
  using xpp::expr::number_from_halves;
  if (i + 1 >= prog.size() || prog[i + 1] < 3) return false;
  const size_t len = static_cast<size_t>(prog[i + 1] - 2); /* the then part, b again */
  const size_t t = i + 2 + len;                            /* its MYTHEN */
  if (t + 6 >= prog.size() || prog[t] != MYTHEN || prog[t + 1] != 4 || prog[t + 2] != NUMSYM ||
      number_from_halves(prog[t + 4], prog[t + 3]) != xpp::expr::ZERO_DIVISOR || prog[t + 5] != MYELSE ||
      prog[t + 6] != COM(FUN2TYPE, xpp::expr::IEEE_DIVIDE))
    return false;
  std::vector<int> b(prog.begin() + static_cast<std::ptrdiff_t>(i + 2), prog.begin() + static_cast<std::ptrdiff_t>(t));
  b.push_back(ENDEXP);
  std::vector<int> nb = normalized(b);
  nb.pop_back();
  if (out.size() < nb.size() || !std::equal(nb.begin(), nb.end(), out.end() - static_cast<std::ptrdiff_t>(nb.size())))
    return false;
  out.push_back(COM(FUN2TYPE, 3));
  i = t + 6;
  return true;
}

std::vector<int> normalized(const std::vector<int> &prog)
{
  std::vector<int> out;
  for (size_t i = 0; i < prog.size(); i++) {
    const int c = prog[i];
    if (c == MYIF && collapse_guard(prog, i, out)) continue;
    out.push_back(c == COM(FUN2TYPE, xpp::expr::IEEE_DIVIDE) ? COM(FUN2TYPE, 3) : c);
    if (c == ENDEXP) break;
    /* the operands that follow an instruction, copied as they are */
    const int operands = c == NUMSYM ? 2 : (c == MYIF || c == MYTHEN || c == SUMSYM || c == ENDFUN || c / MAXTYPE == UFUNTYPE) ? 1 : 0;
    for (int k = 0; k < operands && i + 1 < prog.size(); k++) out.push_back(prog[++i]);
  }
  return out;
}

Fingerprint fingerprint(const xpp::Session &s)
{
  const xpp::Model &m = s.model();
  Fingerprint f;
  for (int c : {m.neq, m.node, m.nmarkov, m.fix_var, m.nupar, m.nfun, m.nflags, m.naeqn, m.nsvar, m.nkernel, m.nwiener})
    f.values.push_back(c);
  for (int i = 0; i < m.node + m.fix_var + m.neq - m.node - m.nmarkov; i++) f.programs.push_back(normalized(m.programs[i]));
  for (int i = 0; i < m.nfun; i++) f.programs.push_back(normalized(m.ufun_programs[i]));
  for (int j = 0; j < m.nflags; j++) {
    f.programs.push_back(normalized(m.flags[j].comcond));
    for (int e = 0; e < m.flags[j].nevents; e++) f.programs.push_back(normalized(m.flags[j].comrhs[e]));
  }
  for (int i = 0; i < m.naeqn; i++) f.programs.push_back(normalized(m.aeqns[i].form));
  for (int i = 0; i < m.nsvar; i++) f.programs.push_back(normalized(m.svars[i].form));
  for (int i = 0; i < m.nmarkov; i++)
    for (const std::vector<int> &c : m.markov[i].command) f.programs.push_back(normalized(c));
  for (int i = 0; i < m.nupar; i++) f.values.push_back(m.default_val[i]);
  for (int i = 0; i < m.node + m.nmarkov; i++) f.values.push_back(s.last_ic[i]);
  return f;
}

} // namespace

int convert_file(const std::string &ode, bool auto_answer, const Ask &ask)
{
  std::string arg0 = "xppautX", model = ode;
  std::vector<char *> argv = {arg0.data(), model.data(), nullptr};
  if (is_odex(ode)) {
    xpp::log(XPP_LOG_ERROR, "{} is .odex already\n", ode);
    return 1;
  }
  /* the .ode built with .odex's derived quantities, as its .odex will be:
     the check compares like with like (the numbers are the same) */
  const OdeAsOdex as_odex;
  const xpp::Loaded loaded = xpp::load_model(2, argv.data(), 1);
  if (!loaded) return 1;
  const std::string out = odex_name(ode);
  std::string text;
  Fingerprint before;
  try {
    xpp::Session &s = **loaded;
    text = convert_model(s, auto_answer, ask);
    before = fingerprint(s);
  } catch (const Diagnostic &e) {
    xpp::log(XPP_LOG_ERROR, "{}: {}\n", ode, e.cause);
    return 1;
  }
  if (xpp::files::exists(out.c_str()) && !auto_answer) {
    std::optional<std::string> answer = ask ? ask(out + " exists; replace it (yes/no)", "no") : std::nullopt;
    if (!answer || (*answer != "yes" && *answer != "y")) {
      xpp::log(XPP_LOG_ERROR, "{} exists and was left as it is (--convert --auto replaces it)\n", out);
      return 1;
    }
  }
  xpp::Writer w(out.c_str());
  if (!w || !w.write(text) || !w.commit()) {
    xpp::log(XPP_LOG_ERROR, "cannot write {}\n", out);
    return 1;
  }
  /* exact by construction, and checked: the .odex builds what the .ode did */
  model = out;
  argv[1] = model.data();
  const xpp::Loaded reloaded = xpp::load_model(2, argv.data(), 1);
  if (!reloaded) {
    xpp::log(XPP_LOG_ERROR, "{} was written but does not load: a bug in --convert\n", out);
    return 1;
  }
  if (!(fingerprint(**reloaded) == before)) {
    xpp::log(XPP_LOG_ERROR, "{} was written but does not compile to what {} did: a bug in --convert\n", out, ode);
    return 1;
  }
  xpp::log(XPP_LOG_INFO, "wrote {}\n", out);
  return 0;
}

}
