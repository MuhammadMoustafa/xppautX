/* An .odex model into the xpp::Model the .ode parser builds (odex.h):
   one route after the parse (docs/odex.md). The statements parse() read
   are checked (every name declared once, where it may be read, called
   with its arguments), then written as the .ode reader's own statements,
   each formula with the parentheses .ode's precedence needs to keep
   .odex's grouping (a sign always bracketed, if/then/else whole): no
   .ode quirk can be reached from them. form_ode.cpp's reader builds the
   Model from those lines (get_eqn_lines), the same code an .ode model
   goes through. Where .odex means something else, this file does it:
   a parameter's value is an expression (evaluated here, in order), an
   initial value one evaluated with the parameters set (after the build),
   a division IEEE's (Model::ieee_division), and a block function the
   one expression its returns make. */
#include "odex.h"
#include "expr.h"
#include "form_ode.h"
#include "model.h"
#include "session.h"
#include "xpp_batch.h"
#include "xpp_io.h"
#include "xpp_log.h"
#include "xpp_util.h"

#include <array>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#ifndef M_PI
# define M_PI 3.14159265358979323846264338327950288
#endif

namespace xpp::odex {

namespace {

/* what a declared name is */
enum class Kind { Variable, Markov, Fixed, Aux, Param, Derived, Wiener, Function, Table, Network, Algebraic };

const char *kind_name(Kind k)
{
  switch (k) {
  case Kind::Variable: return "a variable";
  case Kind::Markov: return "a Markov variable";
  case Kind::Fixed: return "a fixed variable";
  case Kind::Aux: return "an aux quantity";
  case Kind::Param: return "a parameter";
  case Kind::Derived: return "a derived parameter";
  case Kind::Wiener: return "a Wiener parameter";
  case Kind::Function: return "a function";
  case Kind::Table: return "a table";
  case Kind::Network: return "a network";
  case Kind::Algebraic: return "an algebraic variable";
  }
  return "a name";
}

/* a value a formula reads */
bool is_value(Kind k)
{
  return k != Kind::Aux && k != Kind::Function && k != Kind::Table && k != Kind::Network;
}

struct Decl {
  Kind kind;
  Pos pos;
  int arity = 0;
};

/* the built-in functions and their number of arguments (sum and volterra
   have named ones: checked apart) */
int builtin_arity(std::string_view f)
{
  static constexpr auto one = std::to_array<std::string_view>({
    "sin", "cos", "tan", "asin", "acos", "atan", "sinh", "cosh", "tanh", "exp", "ln", "log",
    "log10", "sqrt", "heav", "sign", "flr", "ran", "abs", "erf", "erfc", "hom_bcs", "lgamma",
    "poisson"});
  static constexpr auto two = std::to_array<std::string_view>({
    "atan2", "max", "min", "normal", "besselj", "bessely", "besseli", "besselis", "delay",
    "shift", "ishift"});
  for (std::string_view n : one)
    if (n == f) return 1;
  for (std::string_view n : two)
    if (n == f) return 2;
  if (f == "del_shft" || f == "set") return 3;
  if (f == "sum" || f == "volterra") return -2;
  return -1;
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
  /* a parameter's value: numbers, pi and the parameters before it */
  bool par_value = false;
};

/* an .odex expression as .ode formula text (priority of an operator in
   .ode's table, expr_symbols.cpp: | + - 4, & * / 6, ^ and the
   comparisons 7, all grouping left) */
int ode_priority(const Expr &e)
{
  if (e.kind != Expr::Kind::Binary) return 100;
  const std::string &op = e.text;
  if (op == "or" || op == "+" || op == "-") return 4;
  if (op == "and" || op == "*" || op == "/") return 6;
  if (op == "mod" || op == "!=") return 100; /* written as calls */
  return 7;
}

class Loader {
public:
  explicit Loader(const Parsed &p) : p_(p) {}

  Lowered run()
  {
    for (const Statement &s : p_.statements) declare(s);
    out_.lines.push_back("# " + p_.files[0]);
    for (const Statement &s : p_.statements) lower(s);
    out_.lines.push_back("done");
    /* a variable with a history and no init starts at 0, not at the
       number the reader finds at the history's front */
    std::vector<Lowered::Initial> zero;
    for (const Binding &b : historied_)
      if (!initialized_.count(b.name)) zero.push_back({b.name, "0", p_.files[b.pos.file], b.pos});
    out_.initials.insert(out_.initials.begin(), zero.begin(), zero.end());
    return std::move(out_);
  }

private:
  [[noreturn]] void fail(Pos pos, std::string msg) const { throw Error{p_.files[pos.file], pos, std::move(msg)}; }

  /* ---- the names ---- */
  void add(const std::string &name, Kind kind, Pos pos, int arity = 0)
  {
    auto same = decls_.find(name);
    if (same != decls_.end())
      fail(pos, xpp::format("`{}` is already declared at {}:{}", name, same->second.pos.line, same->second.pos.col));
    const std::string upper = xpp::upper_case(name);
    auto folded = folded_.find(upper);
    if (folded != folded_.end())
      fail(pos, xpp::format("`{}` and `{}` ({}:{}) differ only by case: this version of xppautX keeps a model's "
                            "names without case and cannot hold both", name, folded->second,
                            decls_[folded->second].pos.line, decls_[folded->second].pos.col));
    if (reads_as_builtin(upper))
      fail(pos, xpp::format("`{}` would be read as the built-in `{}`: this version of xppautX keeps a model's "
                            "names without case", name, xpp::lower_case(upper)));
    decls_[name] = Decl{kind, pos, arity};
    folded_[upper] = name;
  }

  /* the expression engine's own names, which the .ode reader matches in
     upper case whatever a model's case */
  static bool reads_as_builtin(const std::string &upper)
  {
    const std::array<ExprSymbol, MAX_SYMBS> &symbols = xpp::session().parser.symbols;
    for (int i = 0; i < STDSYM; i++)
      if (symbols[i].name == upper) return true;
    static constexpr auto more = std::to_array<std::string_view>({"PI", "T", "MOUSE_X", "MOUSE_Y", "MOUSE_VX", "MOUSE_VY"});
    for (std::string_view m : more)
      if (m == upper) return true;
    return is_reserved(xpp::lower_case(upper));
  }

  void declare(const Statement &s)
  {
    switch (s.kind) {
    case Statement::Kind::Ode:
    case Statement::Kind::Volterra: add(s.name, Kind::Variable, s.name_pos); break;
    case Statement::Kind::Markov: add(s.name, Kind::Markov, s.name_pos); break;
    case Statement::Kind::Fixed: add(s.name, Kind::Fixed, s.name_pos); break;
    case Statement::Kind::Aux:
      /* an aux may record a fixed variable under its own name (aux ica =
         ica, as lecar.ode does): the column takes the name */
      for (const Binding &b : s.bindings) {
        const Decl *d = find(b.name);
        if (!(d && d->kind == Kind::Fixed)) add(b.name, Kind::Aux, b.pos);
      }
      break;
    case Statement::Kind::Par:
      for (const Binding &b : s.bindings) add(b.name, Kind::Param, b.pos);
      break;
    case Statement::Kind::Derived:
      for (const Binding &b : s.bindings) add(b.name, Kind::Derived, b.pos);
      break;
    case Statement::Kind::Wiener:
      for (size_t i = 0; i < s.names.size(); i++) add(s.names[i], Kind::Wiener, s.name_positions[i]);
      break;
    case Statement::Kind::Fun: add(s.name, Kind::Function, s.name_pos, static_cast<int>(s.names.size())); break;
    case Statement::Kind::Table: add(s.name, Kind::Table, s.name_pos, 1); break;
    case Statement::Kind::Network: add(s.name, Kind::Network, s.name_pos, 1); break;
    case Statement::Kind::Solv: add(s.name, Kind::Algebraic, s.name_pos); break;
    default: break;
    }
  }

  const Decl *find(const std::string &name) const
  {
    auto it = decls_.find(name);
    return it == decls_.end() ? nullptr : &it->second;
  }

  /* "`X` is not declared", with the name of another case if there is one */
  [[noreturn]] void undeclared(const Expr &e, std::string_view what) const
  {
    auto folded = folded_.find(xpp::upper_case(e.text));
    if (folded != folded_.end())
      fail(e.pos, xpp::format("{} `{}` is not declared (names have case: did you mean `{}`?)", what, e.text, folded->second));
    fail(e.pos, xpp::format("{} `{}` is not declared", what, e.text));
  }

  /* ---- checking a formula ---- */
  void check(const Expr &e, const Scope &sc) const
  {
    switch (e.kind) {
    case Expr::Kind::Number: return;
    case Expr::Kind::Name: check_name(e, sc); return;
    case Expr::Kind::Neg:
    case Expr::Kind::Not:
    case Expr::Kind::Binary:
    case Expr::Kind::If:
      for (const Expr &a : e.args) check(a, sc);
      return;
    case Expr::Kind::Index: fail(e.pos, "indexing (x[i]) is not part of .odex yet");
    case Expr::Kind::Call: check_call(e, sc); return;
    }
  }

  void check_name(const Expr &e, const Scope &sc) const
  {
    if (e.primed) {
      if (e.text == "i" && sc.sum_index) return;
      if (e.text == "t" && sc.kernel) return;
      const Decl *d = find(e.text);
      if (sc.primed_vars && d && (d->kind == Kind::Variable || d->kind == Kind::Markov)) return;
      fail(e.pos, xpp::format("`{}'` cannot be read here (x' is a variable at a boundary condition's right end, "
                              "i' sum's index, t' volterra's variable of integration)", e.text));
    }
    if (sc.args)
      for (const std::string &a : *sc.args)
        if (a == e.text) return;
    if (sc.par_value) {
      if (e.text == "pi") return;
      auto v = par_values_.find(e.text);
      if (v != par_values_.end()) return;
      fail(e.pos, xpp::format("a parameter's value can read numbers, pi and the parameters before it, not `{}`", e.text));
    }
    if (e.text == "t" || e.text == "pi" || e.text.starts_with("mouse_")) {
      if (is_reserved(e.text)) return;
    }
    if (builtin_arity(e.text) != -1)
      fail(e.pos, xpp::format("`{}` is a built-in function: call it", e.text));
    const Decl *d = find(e.text);
    if (!d) undeclared(e, "the name");
    if (d->kind == Kind::Aux)
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
    if (sc.par_value) fail(e.pos, xpp::format("a parameter's value cannot call {}", e.text));
    const int arity = builtin_arity(e.text);
    if (e.text == "sum") {
      check_sum(e, sc);
      return;
    }
    if (e.text == "volterra") {
      check_volterra(e, sc);
      return;
    }
    if (arity >= 0) {
      no_named(e);
      if (n != arity) fail(e.pos, xpp::format("{} takes {} argument{}, not {}", e.text, arity, arity == 1 ? "" : "s", n));
      if (e.text == "delay" || e.text == "del_shft" || e.text == "set" || e.text == "shift" || e.text == "ishift") {
        const Expr &v = e.args[0];
        /* a variable of the expression engine's (a state, fixed, Markov or
           algebraic variable); shift's may be a constant too */
        const Decl *d = v.kind == Expr::Kind::Name && !v.primed ? find(v.text) : nullptr;
        const bool var = d && (d->kind == Kind::Variable || d->kind == Kind::Markov || d->kind == Kind::Fixed ||
                               d->kind == Kind::Algebraic);
        const bool con = d && (d->kind == Kind::Param || d->kind == Kind::Derived || d->kind == Kind::Wiener);
        const bool ok = var || ((e.text == "shift" || e.text == "ishift") && con);
        if (!ok) fail(v.pos, xpp::format("{}'s first argument is the name of a variable", e.text));
        for (int i = 1; i < n; i++) check(e.args[i], sc);
        return;
      }
      for (const Expr &a : e.args) check(a, sc);
      return;
    }
    const Decl *d = find(e.text);
    if (!d) undeclared(e, "the function");
    if (d->kind != Kind::Function && d->kind != Kind::Table && d->kind != Kind::Network)
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

  /* ---- .ode text ---- */
  std::string ode(const Expr &e) const
  {
    std::string out;
    put(out, e, 0, false);
    return out;
  }

  void put(std::string &out, const Expr &e, int parent, bool right) const
  {
    const int pri = ode_priority(e);
    const bool paren = pri < parent || (right && pri == parent);
    if (paren) out += '(';
    switch (e.kind) {
    case Expr::Kind::Number: out += print_number(e.value); break;
    case Expr::Kind::Name: {
      auto v = par_values_.find(e.text);
      if (subst_pars_ && !e.primed && v != par_values_.end())
        out += v->second;
      else if (subst_pars_ && e.text == "pi")
        out += print_number(M_PI);
      else
        out += e.text + (e.primed ? "'" : "");
      break;
    }
    case Expr::Kind::Neg:
      out += "(-";
      put(out, e.args[0], 7, false);
      out += ')';
      break;
    case Expr::Kind::Not:
      out += "not(";
      put(out, e.args[0], 0, false);
      out += ')';
      break;
    case Expr::Kind::If:
      out += "(if(";
      put(out, e.args[0], 0, false);
      out += ")then(";
      put(out, e.args[1], 0, false);
      out += ")else(";
      put(out, e.args[2], 0, false);
      out += "))";
      break;
    case Expr::Kind::Binary: {
      const std::string &op = e.text;
      /* an event's actions: the reader splits them at every '=', so an
         equality is written without one: a<=b & a>=b is a==b, NaN too */
      if (in_event_ && (op == "==" || op == "!=")) {
        out += op == "!=" ? "not((" : "((";
        put(out, e.args[0], 7, false);
        out += "<=";
        put(out, e.args[1], 7, true);
        out += ")&(";
        put(out, e.args[0], 7, false);
        out += ">=";
        put(out, e.args[1], 7, true);
        out += "))";
        break;
      }
      if (op == "mod" || op == "!=") {
        out += op == "mod" ? "mod(" : "not(";
        put(out, e.args[0], op == "mod" ? 0 : 7, false);
        out += op == "mod" ? "," : "==";
        put(out, e.args[1], op == "mod" ? 0 : 7, op != "mod");
        out += ')';
        break;
      }
      put(out, e.args[0], pri, false);
      out += op == "and" ? "&" : op == "or" ? "|" : op;
      put(out, e.args[1], pri, true);
      break;
    }
    case Expr::Kind::Call: put_call(out, e); break;
    case Expr::Kind::Index: break; /* refused by check */
    }
    if (paren) out += ')';
  }

  static const Expr *named(const Expr &e, std::string_view name)
  {
    for (size_t i = 1; i < e.args.size(); i++)
      if (e.arg_names[i] == name) return &e.args[i];
    return nullptr;
  }

  void put_call(std::string &out, const Expr &e) const
  {
    if (e.text == "sum") {
      out += "(sum(";
      put(out, *named(e, "from"), 0, false);
      out += ',';
      put(out, *named(e, "to"), 0, false);
      out += ")of(";
      put(out, e.args[0], 0, false);
      out += "))";
      return;
    }
    if (e.text == "volterra") {
      const Expr *mu = named(e, "mu"), *of = named(e, "of");
      out += "(int";
      if (mu) out += "[" + print_number(mu->value) + "]";
      out += '{';
      put(out, e.args[0], 0, false);
      if (of) {
        out += '#';
        put(out, *of, 0, false);
      }
      out += "})";
      return;
    }
    out += e.text + "(";
    for (size_t i = 0; i < e.args.size(); i++) {
      if (i) out += ',';
      put(out, e.args[i], 0, false);
    }
    out += ')';
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
      if (const Decl *d = find(b.name))
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

  /* ---- statements to .ode lines ---- */
  static Scope formula_scope(bool volterra = false)
  {
    Scope sc;
    sc.volterra = volterra;
    return sc;
  }

  std::string formula(const Expr &e, const Scope &sc) const
  {
    check(e, sc);
    return ode(e);
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

  /* par's values, evaluated here in order (a value is an expression:
     docs/odex.md) */
  double par_value(const Binding &b)
  {
    Scope sc;
    sc.par_value = true;
    check(b.value, sc);
    subst_pars_ = true;
    const std::string text = ode(b.value);
    subst_pars_ = false;
    int ok = 0;
    const double z = calculate(text.c_str(), &ok);
    if (!ok) fail(b.value.pos, xpp::format("the value of {} does not evaluate", b.name));
    return z;
  }

  void lower(const Statement &s)
  {
    std::vector<std::string> &L = out_.lines;
    switch (s.kind) {
    case Statement::Kind::Comment: L.push_back("\" " + s.text); break;
    case Statement::Kind::Options: {
      std::string line = "@ ";
      for (size_t i = 0; i < s.options.size(); i++)
        line += (i ? "," : "") + s.options[i].name + "=" + s.options[i].value;
      L.push_back(line);
      break;
    }
    case Statement::Kind::Par:
      for (const Binding &b : s.bindings) {
        const std::string value = print_number(par_value(b));
        par_values_[b.name] = value;
        L.push_back("par " + b.name + "=" + value);
      }
      break;
    case Statement::Kind::Init:
      /* evaluated once the model is set up (set_initials), with every
         parameter set: no line of the reader's */
      for (const Binding &b : s.bindings) {
        const Decl *d = find(b.name);
        if (!d || (d->kind != Kind::Variable && d->kind != Kind::Markov))
          fail(b.pos, xpp::format("`{}` is not a variable: init gives a variable its initial value", b.name));
        const std::string text = formula(b.value, formula_scope());
        out_.initials.push_back({b.name, text, p_.files[b.value.pos.file], b.value.pos});
        initialized_.insert(b.name);
      }
      break;
    case Statement::Kind::History:
      /* the reader's x(0)=formula: the formula is x's history (t before
         the start), the initial value its own (0 unless an init says) */
      for (const Binding &b : s.bindings) {
        const Decl *d = find(b.name);
        if (!d || d->kind != Kind::Variable)
          fail(b.pos, xpp::format("`{}` is not a variable: history gives a variable its values before the start", b.name));
        L.push_back(b.name + "(0)=" + formula(b.value, formula_scope()));
        historied_.push_back(b);
      }
      break;
    case Statement::Kind::Ode: L.push_back(s.name + "'=" + formula(s.expr, formula_scope(true))); break;
    case Statement::Kind::Volterra: L.push_back("volt " + s.name + "=" + formula(s.expr, formula_scope(true))); break;
    case Statement::Kind::Fixed: L.push_back(s.name + "=" + formula(s.expr, formula_scope(true))); break;
    case Statement::Kind::Aux:
      for (const Binding &b : s.bindings) L.push_back("aux " + b.name + "=" + formula(b.value, formula_scope()));
      break;
    case Statement::Kind::Derived:
      for (const Binding &b : s.bindings) L.push_back("!" + b.name + "=" + formula(b.value, formula_scope()));
      break;
    case Statement::Kind::Fun: {
      Scope sc;
      sc.args = &s.names;
      const Expr body = s.has_body ? flatten(s, s.body, 0, {}, nullptr) : s.expr;
      std::string line = s.name + "(";
      for (size_t i = 0; i < s.names.size(); i++) line += (i ? "," : "") + s.names[i];
      L.push_back(line + ")=" + formula(body, sc));
      break;
    }
    case Statement::Kind::Set: {
      std::string line = "set " + s.name + " {";
      for (size_t i = 0; i < s.bindings.size(); i++) line += (i ? "," : "") + s.bindings[i].name + "=" + set_value(s.bindings[i]);
      L.push_back(line + "}");
      break;
    }
    case Statement::Kind::Table:
      if (s.text.size() || !s.count)
        L.push_back("table " + s.name + " " + s.text);
      else
        L.push_back(xpp::format("table {} % {} {} {} {}", s.name, s.count, print_number(s.lo), print_number(s.hi),
                                formula(s.expr, formula_scope())));
      break;
    case Statement::Kind::Markov: {
      L.push_back(xpp::format("markov {} {}", s.name, s.count));
      for (int r = 0; r < s.count; r++) {
        std::string row;
        for (int c = 0; c < s.count; c++) row += "{" + formula(s.cells[r * s.count + c], formula_scope()) + "} ";
        L.push_back(row);
      }
      break;
    }
    case Statement::Kind::Wiener:
    case Statement::Kind::Only: {
      std::string line = s.kind == Statement::Kind::Wiener ? "wiener " : "only ";
      for (size_t i = 0; i < s.names.size(); i++) line += (i ? "," : "") + s.names[i];
      L.push_back(line);
      break;
    }
    case Statement::Kind::Event: {
      std::string line = xpp::format("global {} {{{}}} {{", s.count, formula(s.expr, formula_scope()));
      for (size_t i = 0; i < s.bindings.size(); i++) {
        const Binding &b = s.bindings[i];
        const Decl *d = find(b.name);
        /* what flags.cpp's compile_flags finds: a column (a variable or
           an aux quantity), a parameter, or out_put, arret, no_interp */
        const bool target = d && (d->kind == Kind::Variable || d->kind == Kind::Markov || d->kind == Kind::Aux ||
                                  d->kind == Kind::Param);
        if (!target && b.name != "out_put" && b.name != "arret" && b.name != "no_interp")
          fail(b.pos, xpp::format("an event sets a variable, an aux quantity, a parameter, out_put, arret or "
                                  "no_interp, not `{}`", b.name));
        in_event_ = true;
        line += (i ? ";" : "") + b.name + "=" + formula(b.value, formula_scope());
        in_event_ = false;
      }
      L.push_back(line + "}");
      break;
    }
    case Statement::Kind::Boundary: {
      Scope sc;
      sc.primed_vars = true;
      L.push_back("bdry " + formula(s.expr, sc));
      break;
    }
    case Statement::Kind::Network: {
      std::string line = "special " + s.name + "=" + s.text + "(";
      for (size_t i = 0; i < s.call_args.size(); i++) line += (i ? "," : "") + s.call_args[i];
      L.push_back(line + ")");
      break;
    }
    case Statement::Kind::Solv: L.push_back("solv " + s.name + "=" + formula(s.expr, formula_scope())); break;
    case Statement::Kind::Dae: L.push_back("0=" + formula(s.expr, formula_scope())); break;
    }
  }

  const Parsed &p_;
  std::map<std::string, Decl> decls_;
  std::map<std::string, std::string> folded_;
  std::map<std::string, std::string> par_values_;
  bool subst_pars_ = false;
  /* writing an event's actions (put) */
  bool in_event_ = false;
  Lowered out_;
  std::set<std::string> initialized_;
  std::vector<Binding> historied_;
};

/* the .odex file's own lines, for the model's source (Model::source) */
std::vector<std::string> source_lines(const std::string &path)
{
  std::vector<std::string> out;
  xpp::LineReader lr(path.c_str());
  while (std::optional<std::string_view> line = lr.next()) out.push_back(std::string(*line) + "\n");
  return out;
}

} // namespace

bool is_odex(std::string_view path)
{
  return path.size() > 5 && xpp::equal_ignoring_case(path.substr(path.size() - 5), ".odex");
}

Lowered lower(const Parsed &p)
{
  xpp::Model &m = xpp::model();
  m.ieee_division = true;
  /* calculate() rolls the symbol table back to the Model's own: before
     the build that is the built-ins */
  m.ncon_start = 0;
  m.nsym_start = STDSYM;
  return Loader(p).run();
}

int load(const std::string &path)
{
  xpp::Model &m = xpp::model();
  try {
    const Lowered low = lower(parse_file(path));
    if (get_eqn_lines(low.lines) != 1) xpp_model_failed();
    for (const Lowered::Initial &init : low.initials)
      m.initial_values.push_back({init.name, init.formula, Error{init.file, init.pos, ""}.text()});
  } catch (const Error &e) {
    xpp::log(XPP_LOG_ERROR, "{}\n", e.text());
    xpp_model_failed();
  }
  m.source = source_lines(path);
  return 1;
}

void set_initials()
{
  xpp::Model &m = xpp::model();
  for (const Model::InitialValue &init : m.initial_values) {
    int ok = 0;
    const double z = calculate(init.formula.c_str(), &ok);
    if (!ok) {
      xpp::log(XPP_LOG_ERROR, "{} the initial value of {} does not evaluate\n", init.where, init.name);
      xpp_model_failed();
    }
    const int i = find_user_name(ICBOX, init.name);
    xpp::session().last_ic[i] = z;
    m.default_ic[i] = z;
    set_val(converted(init.name), z);
  }
}

}
