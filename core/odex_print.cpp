/* An .odex expression back to text (odex.h): the parentheses .odex's
   precedence needs to read the same tree back (docs/odex.md "Precedence,
   low to high"), and a sign bracketed wherever .ode needed it (anywhere
   but at the start or after '(' or ','; docs/odex.md, --convert's bracket
   policy), no others. */
#include "odex.h"
#include "xpp_io.h"

#include <cmath>
#include <string>

namespace xpp::odex {

namespace {

/* the levels of docs/odex.md's precedence list, 1 (or) to 9 (a value) */
enum Level { OR = 1, AND, NOT, CMP, ADD, MUL, NEG, POW, VALUE };

int level_of(const Expr &e)
{
  switch (e.kind) {
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

} // namespace

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
