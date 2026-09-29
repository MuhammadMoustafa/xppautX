/* An .odex expression as text (odex.h). As .odex (print): the
   parentheses .odex's precedence needs to read the same tree back
   (docs/odex.md "Precedence, low to high"), and a sign bracketed wherever
   .ode needed it (anywhere but at the start or after '(' or ',';
   docs/odex.md, --convert's bracket policy), no others. As the expression
   engine's text (engine_text), which the Model builder compiles: the
   parentheses the engine's own precedence needs to keep the tree's
   grouping, a sign and an if always bracketed, so no quirk of .ode's
   precedence is reached. */
#include "odex.h"
#include "xpp_io.h"

#include <cmath>
#include <string>
#include <string_view>

namespace xpp::odex {

namespace {

/* the levels of docs/odex.md's precedence list, 1 (or) to 9 (a value) */
enum Level { OR = 1, AND, NOT, CMP, ADD, MUL, NEG, POW, VALUE };

int level_of(const Expr &e)
{
  switch (e.kind) {
  case Expr::Kind::Text:
  case Expr::Kind::Number:
  case Expr::Kind::Name:
  case Expr::Kind::Call:
  case Expr::Kind::Index: return VALUE;
  case Expr::Kind::Neg: return NEG;
  case Expr::Kind::Not: return NOT;
  case Expr::Kind::If: return 0; /* its else takes all that follows */
  case Expr::Kind::Binary: break;
  }
  const std::string &op = e.text;
  if (op == "or") return OR;
  if (op == "and") return AND;
  if (op == "+" || op == "-") return ADD;
  if (op == "*" || op == "/" || op == "mod") return MUL;
  if (op == "^") return POW;
  return CMP;
}

/* e into out: parenthesized when its level is below min; lead when it
   starts right after the start, '(' or ',' (where a bare sign is kept) */
void put(std::string &out, const Expr &e, int min, bool lead)
{
  const int level = level_of(e);
  const bool paren = level < min || (e.kind == Expr::Kind::Neg && !lead);
  if (paren) {
    out += '(';
    lead = true;
  }
  switch (e.kind) {
  case Expr::Kind::Text:
    out += e.text;
    break;
  case Expr::Kind::Number:
    out += print_number(e.value);
    break;
  case Expr::Kind::Name:
    out += e.text;
    if (e.primed) out += '\'';
    break;
  case Expr::Kind::Neg:
    out += '-';
    put(out, e.args[0], POW, false);
    break;
  case Expr::Kind::Not:
    out += "not ";
    put(out, e.args[0], CMP, false);
    break;
  case Expr::Kind::Binary: {
    const std::string &op = e.text;
    int left = level, right = level + 1;
    if (level == CMP) left = ADD;
    if (level == MUL) right = NEG;
    if (level == POW) {
      left = VALUE;
      right = NEG;
    }
    put(out, e.args[0], left, lead);
    if (op == "and" || op == "or" || op == "mod")
      out += " " + op + " ";
    else
      out += op;
    put(out, e.args[1], right, false);
    break;
  }
  case Expr::Kind::If: {
    const Expr *x = &e;
    out += "if ";
    for (;;) {
      put(out, x->args[0], 1, true);
      out += " then ";
      put(out, x->args[1], 0, true);
      out += " else ";
      if (x->args[2].kind != Expr::Kind::If) break;
      x = &x->args[2];
      out += "if ";
    }
    put(out, x->args[2], 0, true);
    break;
  }
  case Expr::Kind::Call:
    out += e.text;
    out += '(';
    for (size_t i = 0; i < e.args.size(); i++) {
      if (i) out += ", ";
      if (i < e.arg_names.size() && !e.arg_names[i].empty()) out += e.arg_names[i] + "=";
      put(out, e.args[i], 0, true);
    }
    out += ')';
    break;
  case Expr::Kind::Index:
    put(out, e.args[0], VALUE, lead);
    out += '[';
    put(out, e.args[1], 0, true);
    out += ']';
    break;
  }
  if (paren) out += ')';
}

/* ---- the expression engine's text ---- */

/* an operator's priority in the engine's table (expr_symbols.cpp: | + -
   4, & * / 6, ^ and the comparisons 7, all grouping left); mod and !=
   are written as calls */
int engine_priority(const Expr &e)
{
  if (e.kind != Expr::Kind::Binary) return 100;
  const std::string &op = e.text;
  if (op == "or" || op == "+" || op == "-") return 4;
  if (op == "and" || op == "*" || op == "/") return 6;
  if (op == "mod" || op == "!=") return 100;
  return 7;
}

const Expr *named_arg(const Expr &e, std::string_view name)
{
  for (size_t i = 1; i < e.args.size(); i++)
    if (e.arg_names[i] == name) return &e.args[i];
  return nullptr;
}

class EngineText {
public:
  void put(std::string &out, const Expr &e, int parent, bool right) const
  {
    const int pri = engine_priority(e);
    const bool paren = pri < parent || (right && pri == parent);
    if (paren) out += '(';
    switch (e.kind) {
    case Expr::Kind::Text: out += e.text; break;
    case Expr::Kind::Number: out += print_number(e.value); break;
    case Expr::Kind::Name: name(out, e); break;
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
    case Expr::Kind::Binary: binary(out, e, pri); break;
    case Expr::Kind::Call: call(out, e); break;
    case Expr::Kind::Index: break; /* the .odex reader makes it a name */
    }
    if (paren) out += ')';
  }

private:
  void name(std::string &out, const Expr &e) const
  {
    out += e.text;
    if (e.primed) out += '\'';
  }

  void binary(std::string &out, const Expr &e, int pri) const
  {
    const std::string &op = e.text;
    if (op == "mod" || op == "!=") {
      out += op == "mod" ? "mod(" : "not(";
      put(out, e.args[0], op == "mod" ? 0 : 7, false);
      out += op == "mod" ? "," : "==";
      put(out, e.args[1], op == "mod" ? 0 : 7, op != "mod");
      out += ')';
      return;
    }
    put(out, e.args[0], pri, false);
    out += op == "and" ? "&" : op == "or" ? "|" : op;
    put(out, e.args[1], pri, true);
  }

  void call(std::string &out, const Expr &e) const
  {
    if (e.text == "sum") {
      out += "(sum(";
      put(out, *named_arg(e, "from"), 0, false);
      out += ',';
      put(out, *named_arg(e, "to"), 0, false);
      out += ")of(";
      put(out, e.args[0], 0, false);
      out += "))";
      return;
    }
    if (e.text == "volterra") {
      const Expr *mu = named_arg(e, "mu"), *of = named_arg(e, "of");
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
    if (e.text == "near") {
      /* |a-b| <= tol*max(1, |a|, |b|), tol its third argument (the .odex
         reader writes the model's own when the call gives none) */
      const std::string tol = print_number(e.args[2].value);
      out += "(abs((";
      put(out, e.args[0], 0, false);
      out += ")-(";
      put(out, e.args[1], 0, false);
      out += "))<=(" + tol + ")*max(1,max(abs(";
      put(out, e.args[0], 0, false);
      out += "),abs(";
      put(out, e.args[1], 0, false);
      out += "))))";
      return;
    }
    out += e.text + "(";
    for (size_t i = 0; i < e.args.size(); i++) {
      if (i) out += ',';
      put(out, e.args[i], 0, false);
    }
    out += ')';
  }

};

} // namespace

std::string engine_text(const Expr &e)
{
  std::string out;
  EngineText().put(out, e, 0, false);
  return out;
}

std::string print_number(double v)
{
  if (std::isinf(v)) return v > 0 ? "1e999" : "-1e999";
  return xpp::number(v);
}

std::string print(const Expr &e)
{
  std::string out;
  put(out, e, 0, true);
  return out;
}

}
