/* The .odex reader (odex.h): the statements parse() read, checked (every
   name declared once, where it may be read, called with its arguments)
   and readied for the Model builder (form_ode.cpp's build_model), the
   one an .ode model's statements go through too (docs/odex.md question
   10). What .odex means where .ode means something else is said in the
   statements themselves: an initial value a formula (evaluated with the
   parameters set, once the model is set up), a history only a history,
   a block function the one expression its returns make, near() with its
   tol, and the model's divisions IEEE's (Parsed::ieee_division). A
   const's value is worked out here, and an array statement (a trailing
   range, for j in lo..hi by step: docs/odex.md "Arrays") made its copies,
   each element x[e] the name x followed by e's value, each copy marked
   (Statement::array) for --convert. */
#include "odex.h"
#include "expr.h"
#include "form_ode.h"
#include "model.h"
#include "model_files.h"
#include "model_options.h"
#include "session.h"
#include "xpp_batch.h"
#include "xpp_io.h"
#include "xpp_files.h"
#include "xpp_log.h"
#include "xpp_util.h"
#include "xpp_math.h"

#include <array>
#include <cmath>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xpp::odex {

namespace {


const char *kind_name(Model::NameKind k)
{
  switch (k) {
  case Model::NameKind::Variable: return "a variable";
  case Model::NameKind::Markov: return "a Markov variable";
  case Model::NameKind::Fixed: return "a fixed variable";
  case Model::NameKind::Aux: return "an aux quantity";
  case Model::NameKind::Param: return "a parameter";
  case Model::NameKind::Const: return "a const";
  case Model::NameKind::Wiener: return "a Wiener parameter";
  case Model::NameKind::Function: return "a function";
  case Model::NameKind::Table: return "a table";
  case Model::NameKind::Network: return "a network";
  case Model::NameKind::Algebraic: return "an algebraic variable";
  case Model::NameKind::Vector:
  case Model::NameKind::Derived:
  case Model::NameKind::Set:
  case Model::NameKind::Kernel: break;
  }
  return "a name";
}

/* a value a formula reads */
bool is_value(Model::NameKind k)
{
  return k != Model::NameKind::Aux && k != Model::NameKind::Function && k != Model::NameKind::Table && k != Model::NameKind::Network;
}


/* where a formula is: what else it may read */
struct Scope {
  /* a function's arguments */
  const std::vector<std::string> *args = nullptr;
  /* a boundary condition: a variable's value at the right end, x' */
  bool primed_vars = false;
  /* inside sum(): its index i'; inside volterra's kernel: t' */
  bool sum_index = false;
  bool kernel = false;
  /* volterra(...) allowed: an ODE's, a Volterra equation's or a fixed
     variable's formula (the .ode reader looks for its kernels there) */
  bool volterra = false;
};

/* the statements that may take a range: every one but an @ line, a
   comment the model shows, only and a set */
bool takes_range(Statement::Kind k)
{
  return k != Statement::Kind::Options && k != Statement::Kind::Comment && k != Statement::Kind::Only && k != Statement::Kind::Set;
}

class Loader {
public:
  explicit Loader(Model &m, const Parsed &p) : p_(p), m_(m) { m_.names.clear(); }

  Parsed run()
  {
    const std::vector<Statement> statements = expanded();
    for (const Statement &s : statements) in_copy(s.array, [&] { declare(s); });
    for (const Statement &s : p_.statements)
      if (!s.range.index.empty())
        if (const Model::Name *d = find(s.range.index))
          fail(s.range.index_pos, xpp::format("the index `{}` is {}, declared at {}:{}: an index is named like "
                                              "no declared name", s.range.index, kind_name(d->kind),
                                              d->pos.line, d->pos.col));
    scan_neartol();
    Parsed out;
    out.files = p_.files;
    out.ieee_division = true;
    out.derived = true;
    for (const Statement &s : statements) in_copy(s.array, [&] { ready(s, out.statements); });
    return out;
  }

private:
  [[noreturn]] void fail(Pos pos, std::string msg) const { throw error_at(p_.files[pos.file], pos, std::move(msg)); }

  /* f(), a problem in an array's copy (a) saying which copy */
  template <class F> static void in_copy(const ArrayCopy &a, F f)
  {
    try {
      f();
    } catch (Error &e) {
      if (a.group) e.what += xpp::format(" (where {} = {})", a.index, a.value);
      throw;
    }
  }

  /* ---- consts and arrays ---- */
  /* the value of e, worked out at load: numbers, the consts before it and
     the range's index (in index, its value in value), with + - * / mod ^
     and a sign (docs/odex.md "Arrays"); what is an index or a range's end
     says so in what */
  double load_value(const Expr &e, std::string_view what, const std::string &index, int value) const
  {
    switch (e.kind) {
    case Expr::Kind::Number: return e.value;
    case Expr::Kind::Name: {
      if (!e.primed && !index.empty() && e.text == index) return value;
      const Model::Name *c = m_.name(e.text);
      if (!e.primed && c && c->load_constant && c->load_constant->first == e.text) return c->load_constant->second;
      fail(e.pos, xpp::format("{} reads numbers, the consts before it and the range's index, not `{}{}`", what,
                              e.text, e.primed ? "'" : ""));
    }
    case Expr::Kind::Neg: return -load_value(e.args[0], what, index, value);
    case Expr::Kind::Binary: {
      const std::string &op = e.text;
      if (op == "+" || op == "-" || op == "*" || op == "/" || op == "mod" || op == "^") {
        const double a = load_value(e.args[0], what, index, value), b = load_value(e.args[1], what, index, value);
        if (op == "+") return a + b;
        if (op == "-") return a - b;
        if (op == "*") return a * b;
        if (op == "/") return a / b;
        if (op == "^") return xpp::math::pow(a, b);
        /* mod as the expression engine's: in [0, b) */
        const double z = std::fmod(a, b);
        return z < 0 ? z + b : z;
      }
      break;
    }
    default: break;
    }
    fail(e.pos, xpp::format("{} is worked out at load from numbers, consts and the range's index with + - * / mod "
                            "^: `{}` is not", what, print(e)));
  }

  /* e's value, a whole number (and not negative with nonnegative) */
  int whole(const Expr &e, std::string_view what, const std::string &index, int value, bool nonnegative) const
  {
    const double v = load_value(e, what, index, value);
    if (!(v == std::floor(v)) || std::fabs(v) > 1e9)
      fail(e.pos, xpp::format("{} `{}` is {}, not a whole number", what, print(e), print_number(v)));
    if (nonnegative && v < 0) fail(e.pos, xpp::format("{} `{}` is {}: an element's index is 0 or more", what, print(e), print_number(v)));
    return static_cast<int>(v);
  }

  /* name with its index's value: x[e] is the name x followed by e's */
  std::string element(const std::string &name, const Expr &index, const std::string &ix, int value) const
  {
    return name + std::to_string(whole(index, xpp::format("the index of {}", name), ix, value, true));
  }

  /* element(), for a name the statement declares: the array it is a
     member of is kept for the error a clash with it gives */
  std::string defined(const std::string &base, const Expr &index, const std::string &ix, int value)
  {
    std::string name = element(base, index, ix, value);
    if (group_) m_.names[xpp::upper_case(name)].array_members[group_] = {name,array_text(base, lo_, hi_, step_)};
    return name;
  }

  /* e with the range's index its value (value) and every element x[e]
     the name it is */
  Expr resolved(const Expr &e, const std::string &index, int value) const
  {
    if (e.kind == Expr::Kind::Name && !e.primed && !index.empty() && e.text == index) {
      Expr n;
      n.kind = Expr::Kind::Number;
      n.pos = e.pos;
      n.value = value;
      n.text = std::to_string(value);
      return n;
    }
    if (e.kind == Expr::Kind::Index) {
      const Expr &a = e.args[0];
      if (a.kind != Expr::Kind::Name || a.primed) fail(e.pos, "only a name can be indexed: x[j]");
      Expr n = a;
      n.text = element(a.text, e.args[1], index, value);
      return n;
    }
    Expr out = e;
    for (Expr &a : out.args) a = resolved(a, index, value);
    return out;
  }

  void resolve_block(std::vector<BlockStmt> &body, const std::string &index, int value) const
  {
    for (BlockStmt &b : body) {
      b.value = resolved(b.value, index, value);
      for (Expr &c : b.conds) c = resolved(c, index, value);
      for (std::vector<BlockStmt> &blk : b.blocks) resolve_block(blk, index, value);
    }
  }

  /* s's copy for index = value (index "" for a statement with no range):
     its formulas and names resolved */
  Statement copy(const Statement &s, const std::string &index, int value)
  {
    Statement c = s;
    c.range = Range();
    if (c.name_index) c.name = defined(c.name, *c.name_index, index, value);
    c.name_index.reset();
    c.expr = resolved(c.expr, index, value);
    for (Binding &b : c.bindings) {
      if (b.index) b.name = defined(b.name, *b.index, index, value);
      b.index.reset();
      b.value = resolved(b.value, index, value);
    }
    for (size_t i = 0; i < c.name_indices.size(); i++)
      if (c.name_indices[i]) c.names[i] = defined(c.names[i], *c.name_indices[i], index, value);
    c.name_indices.clear();
    for (size_t i = 0; i < c.call_arg_indices.size(); i++)
      if (c.call_arg_indices[i]) c.call_args[i] = element(c.call_args[i], *c.call_arg_indices[i], index, value);
    c.call_arg_indices.clear();
    for (Expr &e : c.cells) e = resolved(e, index, value);
    resolve_block(c.body, index, value);
    if (c.kind == Statement::Kind::Const) {
      /* its value now, the consts before it read */
      for (Binding &b : c.bindings) {
        const double v = load_value(b.value, xpp::format("the value of const {}", b.name), index, value);
        Expr n;
        n.kind = Expr::Kind::Number;
        n.pos = b.value.pos;
        n.value = v;
        n.text = print_number(v);
        b.value = std::move(n);
        m_.names[xpp::upper_case(b.name)].load_constant=std::pair{b.name,v};
      }
    }
    return c;
  }

  /* the model's statements, each array statement's copies in its place
     (for j in lo..hi by step: lo, lo+step, ... up to hi) */
  std::vector<Statement> expanded()
  {
    std::vector<Statement> out;
    int group = 0;
    for (const Statement &s : p_.statements) {
      const Range &r = s.range;
      if (r.index.empty()) {
        group_ = 0;
        out.push_back(copy(s, std::string(), 0));
        continue;
      }
      if (!takes_range(s.kind)) fail(r.pos, "this statement takes no range");
      const int lo = whole(r.lo, "the range's first index", std::string(), 0, false);
      const int hi = whole(r.hi, "the range's last index", std::string(), 0, false);
      const int step = whole(r.step, "the range's step", std::string(), 0, false);
      if (step <= 0) fail(r.step.pos, xpp::format("the range's step is a whole number above 0, not {}", step));
      if (hi < lo) fail(r.lo.pos, xpp::format("the range {}..{} is empty: a range counts up from its first index to "
                                              "its last", lo, hi));
      group++;
      group_ = group, lo_ = lo, hi_ = hi, step_ = step;
      for (int k = lo; k <= hi; k += step) {
        const ArrayCopy a{group, r.index, k, lo, hi, step, false, std::string()};
        in_copy(a, [&] { out.push_back(copy(s, r.index, k)); });
        out.back().array = a;
      }
    }
    return out;
  }

  /* ---- the names ---- */
  void add(const std::string &name, Model::NameKind kind, Pos declared_at, int arity = 0)
  {
    /* a name an import brought is reported at the import */
    const Pos pos = import_.line ? import_ : declared_at;
    const std::string via = import_.line ? xpp::format(" (declared at {}:{} of {})", declared_at.line, declared_at.col,
                                                       p_.files[static_cast<size_t>(declared_at.file)]) : std::string();
    const Model::Name *member_name = m_.name(name);
    std::string member_of;
    if(member_name){
      const auto member=member_name->array_members.find(declaring_.group);
      if(member!=member_name->array_members.end() && member->second.first==name)member_of=member->second.second;
    }
    const std::string upper = xpp::upper_case(name);
    const Model::Name *same = m_.name(name);
    if (same && same->declared && same->written == name)
      fail(pos, name_clash(name, member_of, same->array,
                           xpp::format("{}:{}", same->pos.line, same->pos.col)) + via);
    if (same && same->declared)
      fail(pos, xpp::format("`{}` and `{}` ({}:{}) differ only by case: this version of xppautX keeps a model's "
                            "names without case and cannot hold both", name, same->written,
                            same->pos.line, same->pos.col) + via);
    if (reads_as_builtin(upper))
      fail(pos, xpp::format("`{}` would be read as the built-in `{}`: this version of xppautX keeps a model's "
                            "names without case", name, xpp::lower_case(upper)) + via);
    Model::Name &entry = m_.names[upper];
    entry.kind = kind;
    entry.pos = pos;
    entry.arity = arity;
    entry.array = member_of;
    entry.written = name;
    entry.declared = true;
  }

  /* the expression engine's own names, which the .ode reader matches in
     upper case whatever a model's case */
  static bool reads_as_builtin(const std::string &upper)
  {
    if (is_builtin_symbol(upper)) return true;
    static constexpr auto more = std::to_array<std::string_view>({"PI", "T", "MOUSE_X", "MOUSE_Y", "MOUSE_VX", "MOUSE_VY"});
    for (std::string_view m : more)
      if (m == upper) return true;
    return is_reserved(xpp::lower_case(upper));
  }

  void declare(const Statement &s)
  {
    declaring_ = s.array;
    import_ = s.imported_at;
    switch (s.kind) {
    case Statement::Kind::Ode:
    case Statement::Kind::Volterra: add(s.name, Model::NameKind::Variable, s.name_pos); break;
    case Statement::Kind::Markov: add(s.name, Model::NameKind::Markov, s.name_pos); break;
    case Statement::Kind::Fixed: add(s.name, Model::NameKind::Fixed, s.name_pos); break;
    case Statement::Kind::Aux:
      /* an aux may record a fixed variable under its own name (aux ica =
         ica, as lecar.ode does): the column takes the name */
      for (const Binding &b : s.bindings) {
        const Model::Name *d = find(b.name);
        if (!(d && d->kind == Model::NameKind::Fixed)) add(b.name, Model::NameKind::Aux, b.pos);
      }
      break;
    case Statement::Kind::Par:
      for (const Binding &b : s.bindings) add(b.name, Model::NameKind::Param, b.pos);
      break;
    case Statement::Kind::Const:
      for (const Binding &b : s.bindings) add(b.name, Model::NameKind::Const, b.pos);
      break;
    case Statement::Kind::Wiener:
      for (size_t i = 0; i < s.names.size(); i++) add(s.names[i], Model::NameKind::Wiener, s.name_positions[i]);
      break;
    case Statement::Kind::Fun: add(s.name, Model::NameKind::Function, s.name_pos, static_cast<int>(s.names.size())); break;
    case Statement::Kind::Table: add(s.name, Model::NameKind::Table, s.name_pos, 1); break;
    case Statement::Kind::Network: add(s.name, Model::NameKind::Network, s.name_pos, 1); break;
    case Statement::Kind::Solv: add(s.name, Model::NameKind::Algebraic, s.name_pos); break;
    default: break;
    }
  }

  const Model::Name *find(const std::string &name) const
  {
    const Model::Name *entry = m_.name(name);
    return entry && entry->declared && entry->written == name ? entry : nullptr;
  }

  /* the model's own near()'s tol, @ neartol= (default 1e-9, docs/odex.md
     question 7): scanned once, whole-model (not position-sensitive: a
     near() before the @ line that sets it still reads it), never among
     the options the Model keeps (which would warn "not recognized") */
  void scan_neartol()
  {
    for (const Statement &s : p_.statements) {
      if (s.kind != Statement::Kind::Options) continue;
      for (const Option &o : s.options) {
        if (o.name != "neartol") continue;
        double v = 0;
        if (!xpp::parse_number(o.value, v) || !(v > 0))
          fail(o.value_pos, xpp::format("neartol: {} is not a positive number", o.value));
        neartol_ = v;
      }
    }
  }

  /* "`X` is not declared", with the name of another case if there is one */
  [[noreturn]] void undeclared(const Expr &e, std::string_view what) const
  {
    const Model::Name *entry = m_.name(e.text);
    if (entry && entry->declared)
      fail(e.pos, xpp::format("{} `{}` is not declared (names have case: did you mean `{}`?)", what, e.text, entry->written));
    fail(e.pos, xpp::format("{} `{}` is not declared", what, e.text));
  }

  /* ---- checking a formula ---- */
  void check(const Expr &e, const Scope &sc) const
  {
    switch (e.kind) {
    case Expr::Kind::Text:
    case Expr::Kind::Number: return;
    case Expr::Kind::Name: check_name(e, sc); return;
    case Expr::Kind::Neg:
    case Expr::Kind::Not:
    case Expr::Kind::Binary:
    case Expr::Kind::If:
      for (const Expr &a : e.args) check(a, sc);
      return;
    case Expr::Kind::Index: fail(e.pos, "an element x[e] reads here only as a name"); /* copy() resolved it */
    case Expr::Kind::Call: check_call(e, sc); return;
    }
  }

  void check_name(const Expr &e, const Scope &sc) const
  {
    if (e.primed) {
      if (e.text == "i" && sc.sum_index) return;
      if (e.text == "t" && sc.kernel) return;
      const Model::Name *d = find(e.text);
      if (sc.primed_vars && d && (d->kind == Model::NameKind::Variable || d->kind == Model::NameKind::Markov)) return;
      fail(e.pos, xpp::format("`{}'` cannot be read here (x' is a variable at a boundary condition's right end, "
                              "i' sum's index, t' volterra's variable of integration)", e.text));
    }
    if (sc.args)
      for (const std::string &a : *sc.args)
        if (a == e.text) return;
    if (e.text == "t" || e.text == "pi" || e.text.starts_with("mouse_")) {
      if (is_reserved(e.text)) return;
    }
    if (builtin_arity(e.text) != -1 || e.text == "near")
      fail(e.pos, xpp::format("`{}` is a built-in function: call it", e.text));
    const Model::Name *d = find(e.text);
    if (!d) undeclared(e, "the name");
    if (d->kind == Model::NameKind::Aux)
      fail(e.pos, xpp::format("`{}` is an aux quantity, computed after every formula: a formula cannot read it", e.text));
    if (!is_value(d->kind))
      fail(e.pos, xpp::format("`{}` is {}: call it", e.text, kind_name(d->kind)));
  }

  void no_named(const Expr &e) const
  {
    for (size_t i = 0; i < e.arg_names.size(); i++)
      if (!e.arg_names[i].empty())
        fail(e.args[i].pos, xpp::format("{} takes no named argument ({}=)", e.text, e.arg_names[i]));
  }

  void check_call(const Expr &e, const Scope &sc) const
  {
    const int n = static_cast<int>(e.args.size());
    const int arity = builtin_arity(e.text);
    if (e.text == "sum") {
      check_sum(e, sc);
      return;
    }
    if (e.text == "volterra") {
      check_volterra(e, sc);
      return;
    }
    if (e.text == "near") {
      check_near(e, sc);
      return;
    }
    if (arity >= 0) {
      no_named(e);
      if (n != arity) fail(e.pos, xpp::format("{} takes {} argument{}, not {}", e.text, arity, arity == 1 ? "" : "s", n));
      if (e.text == "delay" || e.text == "del_shft" || e.text == "set" || e.text == "shift" || e.text == "ishift") {
        const Expr &v = e.args[0];
        /* a variable of the expression engine's (a state, fixed, Markov or
           algebraic variable); shift's may be a constant too */
        const Model::Name *d = v.kind == Expr::Kind::Name && !v.primed ? find(v.text) : nullptr;
        const bool var = d && (d->kind == Model::NameKind::Variable || d->kind == Model::NameKind::Markov || d->kind == Model::NameKind::Fixed ||
                               d->kind == Model::NameKind::Algebraic);
        const bool con = d && (d->kind == Model::NameKind::Param || d->kind == Model::NameKind::Const || d->kind == Model::NameKind::Wiener);
        const bool ok = var || ((e.text == "shift" || e.text == "ishift") && con);
        if (!ok) fail(v.pos, xpp::format("{}'s first argument is the name of a variable", e.text));
        for (int i = 1; i < n; i++) check(e.args[i], sc);
        return;
      }
      for (const Expr &a : e.args) check(a, sc);
      return;
    }
    const Model::Name *d = find(e.text);
    if (!d) undeclared(e, "the function");
    if (d->kind != Model::NameKind::Function && d->kind != Model::NameKind::Table && d->kind != Model::NameKind::Network)
      fail(e.pos, xpp::format("`{}` is {}, not a function", e.text, kind_name(d->kind)));
    no_named(e);
    if (n != d->arity)
      fail(e.pos, xpp::format("{} takes {} argument{}, not {}", e.text, d->arity, d->arity == 1 ? "" : "s", n));
    for (const Expr &a : e.args) check(a, sc);
  }

  /* sum(expr, from=lo, to=hi): expr for each i' from lo to hi */
  void check_sum(const Expr &e, const Scope &sc) const
  {
    const Expr *from = nullptr, *to = nullptr;
    for (size_t i = 1; i < e.args.size(); i++) {
      if (e.arg_names[i] == "from") from = &e.args[i];
      else if (e.arg_names[i] == "to") to = &e.args[i];
      else fail(e.args[i].pos, "sum's arguments after the first are from= and to=");
    }
    if (!from || !to) fail(e.pos, "sum needs from= and to=: sum(expr, from=lo, to=hi)");
    check(*from, sc);
    check(*to, sc);
    Scope inner = sc;
    inner.sum_index = true;
    check(e.args[0], inner);
  }

  /* volterra(kernel, of=u, mu=m) */
  void check_volterra(const Expr &e, const Scope &sc) const
  {
    if (!sc.volterra)
      fail(e.pos, "volterra(...) belongs in an ODE's, a Volterra equation's or a fixed variable's formula");
    for (size_t i = 1; i < e.args.size(); i++) {
      if (e.arg_names[i] == "of") check(e.args[i], sc);
      else if (e.arg_names[i] == "mu") {
        const Expr &m = e.args[i];
        if (m.kind != Expr::Kind::Number || m.value < 0 || m.value >= 1)
          fail(m.pos, "volterra's mu is a number in [0,1)");
      } else {
        fail(e.args[i].pos, "volterra's arguments after the first are of= and mu=");
      }
    }
    Scope inner = sc;
    inner.kernel = true;
    inner.volterra = false;
    check(e.args[0], inner);
  }

  /* near(a, b[, tol=t]) (docs/odex.md question 7): not a built-in of the
     expression engine; engine_text (odex_print.cpp) writes it with abs
     and max (checked to have the arities near needs) */
  void check_near(const Expr &e, const Scope &sc) const
  {
    const int n = static_cast<int>(e.args.size());
    if (n < 2 || n > 3) fail(e.pos, xpp::format("near takes 2 or 3 arguments, not {}", n));
    if (!e.arg_names[1].empty())
      fail(e.args[1].pos, xpp::format("near's second argument is b, not a named argument (`{}=`)", e.arg_names[1]));
    check(e.args[0], sc);
    check(e.args[1], sc);
    if (n == 3) {
      if (e.arg_names[2] != "tol") {
        fail(e.args[2].pos, e.arg_names[2].empty() ? std::string("near's third argument must be named tol=")
                                                    : xpp::format("near has no argument named `{}`: only tol=",
                                                                  e.arg_names[2]));
      }
      const Expr &t = e.args[2];
      if (t.kind != Expr::Kind::Number || !(t.value > 0)) fail(t.pos, "near's tol= is not a positive number");
    }
  }

  /* ---- block functions ---- */
  /* what follows a block when its end is reached without a return */
  struct Rest {
    const std::vector<BlockStmt> *stmts;
    size_t i;
    const std::map<std::string, Expr> *env;
    const Rest *next;
  };

  static Expr substituted(const Expr &e, const std::map<std::string, Expr> &env)
  {
    if (e.kind == Expr::Kind::Name && !e.primed) {
      auto it = env.find(e.text);
      if (it != env.end()) return it->second;
    }
    Expr out = e;
    for (Expr &a : out.args) a = substituted(a, env);
    return out;
  }

  /* body[i..] as one expression: its lets substituted, its ifs if/then/else
     (docs/odex.md: the jumps the one-line if compiles to) */
  Expr flatten(const Statement &f, const std::vector<BlockStmt> &body, size_t i, std::map<std::string, Expr> env,
               const Rest *rest) const
  {
    if (i >= body.size()) {
      if (!rest) fail(f.name_pos, xpp::format("fun {} can end without a return", f.name));
      return flatten(f, *rest->stmts, rest->i, *rest->env, rest->next);
    }
    const BlockStmt &b = body[i];
    switch (b.kind) {
    case BlockStmt::Kind::Let: {
      if (const Model::Name *d = find(b.name))
        fail(b.pos, xpp::format("fun {} cannot set `{}`, {}: a block function writes no parameter or state variable",
                                f.name, b.name, kind_name(d->kind)));
      Expr v = substituted(b.value, env);
      env[b.name] = std::move(v);
      return flatten(f, body, i + 1, std::move(env), rest);
    }
    case BlockStmt::Kind::Return: return substituted(b.value, env);
    case BlockStmt::Kind::If: break;
    }
    const Rest after{&body, i + 1, &env, rest};
    Expr out = b.blocks.size() > b.conds.size() ? flatten(f, b.blocks.back(), 0, env, &after)
                                                : flatten(f, body, i + 1, env, rest);
    for (size_t k = b.conds.size(); k-- > 0;) {
      Expr e;
      e.kind = Expr::Kind::If;
      e.pos = b.pos;
      e.args.push_back(substituted(b.conds[k], env));
      e.args.push_back(flatten(f, b.blocks[k], 0, env, &after));
      e.args.push_back(std::move(out));
      out = std::move(e);
    }
    return out;
  }

  /* ---- statements readied for the builder ---- */
  static Scope formula_scope(bool volterra = false)
  {
    Scope sc;
    sc.volterra = volterra;
    return sc;
  }

  /* e checked, near's tol filled in (its third argument, the model's
     neartol when the call gives none) */
  Expr formula(const Expr &e, const Scope &sc) const
  {
    check(e, sc);
    Expr out = e;
    fill_near(out);
    return out;
  }

  void fill_near(Expr &e) const
  {
    for (Expr &a : e.args) fill_near(a);
    if (e.kind != Expr::Kind::Call || e.text != "near" || e.args.size() != 2) return;
    Expr tol;
    tol.kind = Expr::Kind::Number;
    tol.pos = e.pos;
    tol.value = neartol_;
    tol.text = print_number(neartol_);
    e.args.push_back(std::move(tol));
    e.arg_names.push_back("tol");
  }

  /* a set's key: a variable (its initial value), a parameter, or an option's key */
  void set_key(const Binding &b) const
  {
    const Model::Name *d = find(b.name);
    int index = 0;
    if ((d && (d->kind == Model::NameKind::Variable || d->kind == Model::NameKind::Markov || d->kind == Model::NameKind::Param)) ||
        xpp::find_odex_option(b.name, index))
      return;
    fail(b.pos, xpp::format("a set holds variables, parameters and options, and `{}` is none of them", b.name));
  }

  /* a set's value: a number (signed) or a name, as .ode's sets hold them */
  std::string set_value(const Binding &b) const
  {
    const Expr &v = b.value;
    if (v.kind == Expr::Kind::Number) return print_number(v.value);
    if (v.kind == Expr::Kind::Neg && v.args[0].kind == Expr::Kind::Number) return "-" + print_number(v.args[0].value);
    if (v.kind == Expr::Kind::Name && !v.primed) return v.text;
    fail(v.pos, xpp::format("a set's value is a number or a name, not `{}` ({}=)", print(v), b.name));
  }

  /* each binding's variable: a variable (a Markov one too, with markov) */
  void variables(const Statement &s, bool markov, std::string_view what) const
  {
    for (const Binding &b : s.bindings) {
      const Model::Name *d = find(b.name);
      if (!d || !(d->kind == Model::NameKind::Variable || (markov && d->kind == Model::NameKind::Markov)))
        fail(b.pos, xpp::format("`{}` is not a variable: {}", b.name, what));
    }
  }

  void ready(const Statement &in, std::vector<Statement> &out)
  {
    Statement s = in;
    switch (s.kind) {
    case Statement::Kind::Options:
      if (s.options.empty()) return;
      break;
    case Statement::Kind::Par:
      /* numbers only (docs/odex.md question 9): a quantity worked out
         from the parameters is d = expr */
      for (Binding &b : s.bindings) {
        const Expr &v = b.value;
        const bool neg = v.kind == Expr::Kind::Neg && v.args[0].kind == Expr::Kind::Number;
        if (v.kind != Expr::Kind::Number && !neg)
          fail(v.pos, xpp::format("a parameter's value is a number, not `{}` (par takes numbers; a quantity worked "
                                  "out from parameters is `{} = ...` outside par)", print(v), b.name));
        if (neg) {
          Expr n = v.args[0];
          n.pos = v.pos;
          n.value = -n.value;
          n.text = print_number(n.value);
          b.value = std::move(n);
        }
      }
      break;
    case Statement::Kind::Init:
      variables(s, true, "init gives a variable its initial value");
      for (Binding &b : s.bindings) b.value = formula(b.value, formula_scope());
      break;
    case Statement::Kind::History:
      variables(s, false, "history gives a variable its values before the start");
      for (Binding &b : s.bindings) b.value = formula(b.value, formula_scope());
      break;
    case Statement::Kind::Ode:
    case Statement::Kind::Volterra:
    case Statement::Kind::Fixed: s.expr = formula(s.expr, formula_scope(true)); break;
    case Statement::Kind::Aux:
      for (Binding &b : s.bindings) b.value = formula(b.value, formula_scope());
      break;
    case Statement::Kind::Fun: {
      Scope sc;
      sc.args = &in.names;
      s.expr = formula(s.has_body ? flatten(in, in.body, 0, {}, nullptr) : in.expr, sc);
      s.has_body = false;
      s.body.clear();
      break;
    }
    case Statement::Kind::Set:
      for (size_t i = 0; i < s.bindings.size(); i++) {
        set_key(s.bindings[i]);
        s.text += (i ? "," : "") + s.bindings[i].name + "=" + set_value(s.bindings[i]);
      }
      break;
    case Statement::Kind::Table:
      if (s.table_kind == Statement::TableKind::Formula) s.expr = formula(s.expr, formula_scope());
      break;
    case Statement::Kind::Markov:
      for (Expr &c : s.cells) c = formula(c, formula_scope());
      break;
    case Statement::Kind::Wiener:
      /* a Wiener parameter starts at 0, as .ode's wiener w does */
      for (size_t i = 0; i < s.names.size(); i++) {
        Binding b;
        b.name = s.names[i];
        b.pos = s.name_positions[i];
        b.value.kind = Expr::Kind::Number;
        b.value.pos = b.pos;
        b.value.text = "0";
        s.bindings.push_back(std::move(b));
      }
      break;
    case Statement::Kind::Event:
      s.expr = formula(s.expr, formula_scope());
      for (Binding &b : s.bindings) {
        const Model::Name *d = find(b.name);
        /* what flags.cpp's compile_flags finds: a column (a variable or
           an aux quantity), a parameter, or out_put, arret, no_interp */
        const bool target = d && (d->kind == Model::NameKind::Variable || d->kind == Model::NameKind::Markov || d->kind == Model::NameKind::Aux ||
                                  d->kind == Model::NameKind::Param);
        if (!target && b.name != "out_put" && b.name != "arret" && b.name != "no_interp")
          fail(b.pos, xpp::format("an event sets a variable, an aux quantity, a parameter, out_put, arret or "
                                  "no_interp, not `{}`", b.name));
        b.value = formula(b.value, formula_scope());
      }
      break;
    case Statement::Kind::Boundary: {
      Scope sc;
      sc.primed_vars = true;
      s.expr = formula(s.expr, sc);
      break;
    }
    case Statement::Kind::Network: {
      std::string def = s.text + "(";
      for (size_t i = 0; i < s.call_args.size(); i++) def += (i ? "," : "") + s.call_args[i];
      s.text = def + ")";
      break;
    }
    case Statement::Kind::Solv:
    case Statement::Kind::Dae: s.expr = formula(s.expr, formula_scope()); break;
    default: break;
    }
    out.push_back(std::move(s));
  }

  const Parsed &p_;
  Model &m_;
  /* the array each declared member name belongs to, by the range statement
     (its group) that made it; the range being expanded; the array
     statement being declared */
  int group_ = 0, lo_ = 0, hi_ = 0, step_ = 1;
  ArrayCopy declaring_;
  /* where the import is that brought the statement being declared */
  Pos import_;
  /* near()'s default tol, @ neartol= (docs/odex.md question 7) */
  double neartol_ = 1e-9;
};

} // namespace

bool is_odex(std::string_view path)
{
  return xpp::files::has_extension(path, extension);
}

std::string array_text(std::string_view base, int lo, int hi, int step)
{
  return step > 1 ? xpp::format("{}[{}..{} by {}]", base, lo, hi, step) : xpp::format("{}[{}..{}]", base, lo, hi);
}

std::string name_clash(std::string_view name, std::string_view array, std::string_view earlier_array,
                       std::string_view earlier_at)
{
  const std::string who = array.empty() ? xpp::format("`{}`", name) : xpp::format("`{}`, a member of the array {},", name, array);
  if (earlier_array.empty()) return xpp::format("{} is already declared at {}", who, earlier_at);
  return xpp::format("{} is already a member of the array {}, declared at {}", who, earlier_array, earlier_at);
}

Parsed ready(Model &m, const Parsed &p)
{
  return Loader(m, p).run();
}

int load(xpp::Session &s, const std::string &path, std::string text)
{
  try {
    build_model(s, ready(s.model(), parse(s.model(), text, path)));
  } catch (const Error &e) {
    xpp::model_failed(e);
  }
  /* the model's source: the .odex file's own lines */
  std::vector<std::string> &source = s.model().source;
  source.clear();
  xpp::LineReader lr = xpp::LineReader::of_text(std::move(text));
  while (std::optional<std::string_view> line = lr.next()) source.push_back(std::string(*line) + "\n");
  return 1;
}

}
