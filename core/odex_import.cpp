/* .odex imports (docs/odex.md "Imports", W282): the statements of an
   imported file as the importing file takes them. `import "f.odex"` takes
   them all; `from "f.odex" import a, b` the statements that declare a and b
   and, again and again, those that declare what the statements taken read;
   `import "f.odex" as c1` all of them, every name they declare written
   c1.name wherever it is read. The parser (odex_parse.cpp) reads the file
   and hands its statements here. */
#include "odex.h"
#include "xpp_io.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace xpp::odex {

namespace {

/* each global name of a statement, as it is declared or read there: not an
   argument of a function, a local of a block, nor a range's index */
class Names {
public:
  using Visit = std::function<void(std::string &name, bool declares)>;
  explicit Names(Visit visit) : visit_(std::move(visit)) {}

  void statement(Statement &s)
  {
    locals_.clear();
    if (!s.range.index.empty()) {
      locals_.insert(s.range.index);
      expr(s.range.lo);
      expr(s.range.hi);
      expr(s.range.step);
    }
    if (s.kind == Statement::Kind::Fun) locals_.insert(s.names.begin(), s.names.end());
    if (!s.name.empty()) name(s.name, declares_name(s.kind));
    optional(s.name_index);
    if (s.kind == Statement::Kind::Wiener || s.kind == Statement::Kind::Only)
      for (std::string &n : s.names) name(n, s.kind == Statement::Kind::Wiener);
    for (std::optional<Expr> &i : s.name_indices) optional(i);
    for (Binding &b : s.bindings) {
      name(b.name, declares_binding(s.kind));
      optional(b.index);
      expr(b.value);
    }
    expr(s.expr);
    for (Expr &c : s.cells) expr(c);
    block(s.body);
    for (std::string &arg : s.call_args) words(arg);
    for (std::optional<Expr> &i : s.call_arg_indices) optional(i);
  }

  /* a statement of kind k declares its name (a set's label too: one name each) */
  static bool declares_name(Statement::Kind k)
  {
    using enum Statement::Kind;
    return k == Ode || k == Map || k == Volterra || k == Fixed || k == Solv || k == Markov ||
           k == Fun || k == Table || k == Network || k == Set;
  }

  /* its bindings declare their names */
  static bool declares_binding(Statement::Kind k)
  {
    return k == Statement::Kind::Par || k == Statement::Kind::Const || k == Statement::Kind::Aux;
  }

private:
  void name(std::string &n, bool declares)
  {
    if (!locals_.count(n)) visit_(n, declares);
  }

  void optional(std::optional<Expr> &e)
  {
    if (e) expr(*e);
  }

  void expr(Expr &e)
  {
    switch (e.kind) {
    case Expr::Kind::Text:
    case Expr::Kind::Number: return;
    case Expr::Kind::Name:
      /* i' is a sum's index */
      if (!(e.primed && e.text == "i")) name(e.text, false);
      return;
    case Expr::Kind::Call:
      name(e.text, false);
      for (Expr &a : e.args) expr(a);
      return;
    case Expr::Kind::Index:
      if (!e.args.empty() && e.args[0].kind == Expr::Kind::Name) name(e.args[0].text, false);
      for (size_t i = 1; i < e.args.size(); i++) expr(e.args[i]);
      return;
    case Expr::Kind::Neg:
    case Expr::Kind::Not:
    case Expr::Kind::Binary:
    case Expr::Kind::If:
      for (Expr &a : e.args) expr(a);
      return;
    }
  }

  void block(std::vector<BlockStmt> &body)
  {
    const std::set<std::string> outer = locals_;
    for (BlockStmt &b : body) {
      expr(b.value);
      for (Expr &c : b.conds) expr(c);
      for (std::vector<BlockStmt> &inner : b.blocks) block(inner);
      if (b.kind == BlockStmt::Kind::Let) locals_.insert(b.name);
    }
    locals_ = outer;
  }

  /* a network's argument as written: its names */
  void words(std::string &text)
  {
    text = replace_words(text, [this](std::string_view w) {
      std::string n(w);
      name(n, false);
      return n;
    }, true);
  }

  Visit visit_;
  std::set<std::string> locals_;
};

/* a name that is x3 for an array x (x followed by digits), x3's x; "" when
   it is no element's */
std::string array_of(const std::string &name)
{
  size_t digits = name.size();
  while (digits > 0 && name[digits - 1] >= '0' && name[digits - 1] <= '9') digits--;
  return digits > 0 && digits < name.size() ? name.substr(0, digits) : std::string();
}

/* the names some statements declare, and the bases of their arrays (x of
   x[j]): a name is declared by them when it is one, or an element (x3) of
   an array */
struct Declared {
  std::set<std::string> names, arrays;

  void add(Statement s)
  {
    Names([this](std::string &n, bool declares) {
      if (declares) names.insert(n);
    }).statement(s);
    const bool array = !s.range.index.empty() || s.name_index;
    if (array && !s.name.empty()) arrays.insert(s.name);
    for (const Binding &b : s.bindings)
      if (array || b.index) arrays.insert(b.name);
  }

  bool has(const std::string &name) const
  {
    if (names.count(name)) return true;
    const std::string base = array_of(name);
    return !base.empty() && arrays.count(base) > 0;
  }
};

/* the statements, a list statement (par a = 1, b = 2) as one statement for
   each of its items, which means the same */
std::vector<Statement> unlisted(std::vector<Statement> in)
{
  std::vector<Statement> out;
  for (Statement &s : in) {
    if (s.bindings.size() < 2 || s.kind == Statement::Kind::Event) {
      out.push_back(std::move(s));
      continue;
    }
    for (Binding &b : s.bindings) {
      Statement one = s;
      one.bindings = {std::move(b)};
      out.push_back(std::move(one));
    }
  }
  return out;
}

/* the statements of an import from: those that declare the names asked for
   and, round after round, those that declare what the statements taken
   read (an init or a history of a name taken, an event that sets one, a
   constraint on an algebraic variable go with it) */
std::vector<Statement> selected(std::vector<Statement> in, const Import &imp, const std::vector<std::string> &files)
{
  in = unlisted(std::move(in));
  std::vector<Declared> declared(in.size());
  std::vector<std::set<std::string>> reads(in.size());
  std::vector<std::set<std::string>> binds(in.size()); /* names an init, history or event sets */
  Declared all;
  std::set<std::string> algebraic;
  for (size_t i = 0; i < in.size(); i++) {
    declared[i].add(in[i]);
    all.add(in[i]);
    Names([&](std::string &n, bool declares) {
      if (!declares) reads[i].insert(n);
    }).statement(in[i]);
    const Statement &s = in[i];
    if (s.kind == Statement::Kind::Solv) algebraic.insert(s.name);
    if (s.kind == Statement::Kind::Init || s.kind == Statement::Kind::History || s.kind == Statement::Kind::Event)
      for (const Binding &b : s.bindings) binds[i].insert(b.name);
  }
  for (const std::string &n : imp.names)
    if (!all.has(n))
      throw error_at(files[static_cast<size_t>(imp.at.file)], imp.at, xpp::format("{} declares no `{}`", imp.file, n));
  std::set<std::string> needed(imp.names.begin(), imp.names.end());
  std::vector<bool> taken(in.size(), false);
  const auto wants = [&](const std::set<std::string> &names, bool by_array) {
    return std::any_of(needed.begin(), needed.end(), [&](const std::string &n) {
      return names.count(n) > 0 || (by_array && names.count(array_of(n)) > 0);
    });
  };
  for (bool more = true; more;) {
    more = false;
    for (size_t i = 0; i < in.size(); i++) {
      if (taken[i]) continue;
      bool take = false;
      switch (in[i].kind) {
      case Statement::Kind::Init:
      case Statement::Kind::History:
      case Statement::Kind::Event: take = wants(binds[i], true); break;
      case Statement::Kind::Dae:
        take = std::any_of(reads[i].begin(), reads[i].end(), [&](const std::string &n) { return algebraic.count(n) && needed.count(n); });
        break;
      case Statement::Kind::Comment:
      case Statement::Kind::Options:
      case Statement::Kind::Only:
      case Statement::Kind::Set:
      case Statement::Kind::Boundary: break;
      default: take = wants(declared[i].names, false) || wants(declared[i].arrays, true); break;
      }
      if (!take) continue;
      taken[i] = true;
      more = true;
      needed.insert(reads[i].begin(), reads[i].end());
    }
  }
  std::vector<Statement> out;
  for (size_t i = 0; i < in.size(); i++)
    if (taken[i]) out.push_back(std::move(in[i]));
  return out;
}

} // namespace

std::vector<Statement> import_statements(std::vector<Statement> file, const Import &imp, const std::vector<std::string> &files)
{
  std::erase_if(file, [](const Statement &s) { return s.kind == Statement::Kind::Options; });
  if (imp.selective) file = selected(std::move(file), imp, files);
  if (!imp.prefix.empty()) {
    Declared declared;
    for (const Statement &s : file) declared.add(s);
    for (Statement &s : file)
      Names([&](std::string &n, bool) {
        if (declared.has(n)) n = imp.prefix + "." + n;
      }).statement(s);
  }
  for (Statement &s : file) s.imported_at = imp.at;
  return file;
}

} // namespace xpp::odex
