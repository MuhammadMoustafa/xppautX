/* .odex's tokenizer, grammar and printer (core/odex.h, docs/odex.md): the
   precedence and grouping of docs/odex.md's table, every statement, and
   an error at the right line and column for each kind of mistake. */
#include "xpptest.h"
#include "odex.h"
#include "derived.h"
#include "form_ode.h"
#include "model.h"
#include "expr.h"
#include "xpp_batch.h"
#include "xpp_io.h"
#include "session.h"
#include "storage.h"
#include "menudrive.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using xpp::odex::Error;
using xpp::odex::Expr;
using xpp::odex::Parsed;
using xpp::odex::Statement;

namespace {

/* e fully parenthesized, operators first: (+ 1 2) */
std::string tree(const Expr &e)
{
  switch (e.kind) {
  case Expr::Kind::Text:
  case Expr::Kind::Number: return e.text;
  case Expr::Kind::Name: return e.text + (e.primed ? "'" : "");
  case Expr::Kind::Neg: return "(- " + tree(e.args[0]) + ")";
  case Expr::Kind::Not: return "(not " + tree(e.args[0]) + ")";
  case Expr::Kind::Binary: return "(" + e.text + " " + tree(e.args[0]) + " " + tree(e.args[1]) + ")";
  case Expr::Kind::If: return "(if " + tree(e.args[0]) + " " + tree(e.args[1]) + " " + tree(e.args[2]) + ")";
  case Expr::Kind::Index: return "([] " + tree(e.args[0]) + " " + tree(e.args[1]) + ")";
  case Expr::Kind::Call: {
    std::string s = "(" + e.text;
    for (size_t i = 0; i < e.args.size(); i++)
      s += " " + (e.arg_names[i].empty() ? "" : e.arg_names[i] + "=") + tree(e.args[i]);
    return s + ")";
  }
  }
  return "?";
}

/* text alone, as the formula of "x' =" on the line before it */
Expr expression(const char *text)
{
  Parsed p = xpp::odex::parse(std::string("x' =\n") + text, "e.odex");
  if (p.statements.size() != 1) throw xpp::odex::error_at("e.odex", xpp::odex::Pos{0, 2, 1}, "not one expression");
  return p.statements[0].expr;
}

std::string parsed(const char *text)
{
  try {
    return tree(expression(text));
  } catch (const Error &e) {
    return "error " + std::to_string(e.line - 1) + ":" + std::to_string(e.col) + " " + e.cause;
  }
}

/* the error a model gives: "line:col message", "" when it parses */
std::string model_error(const char *text)
{
  try {
    xpp::odex::parse(text, "m.odex");
  } catch (const Error &e) {
    return std::to_string(e.line) + ":" + std::to_string(e.col) + " " + e.cause;
  }
  return "";
}

bool starts(const std::string &s, const char *prefix)
{
  return s.rfind(prefix, 0) == 0;
}

std::string reprinted(const char *text)
{
  return xpp::odex::print(expression(text));
}

/* a model's statements as the .odex reader readies them for the Model
   builder, each written as .ode would say it (its formulas the text the
   expression engine compiles) and joined by '|' (an init left out), or
   "error line:col message" */
std::string lowered(const char *text)
{
  using xpp::odex::engine_text;
  using K = Statement::Kind;
  try {
    const Parsed p = xpp::odex::ready(xpp::odex::parse(text, "m.odex"));
    std::string out;
    auto add = [&out](const std::string &line) { out += (out.empty() ? "" : "|") + line; };
    for (const Statement &s : p.statements) {
      switch (s.kind) {
      case K::Comment: add("\" " + s.text); break;
      case K::Options: add(s.text); break;
      case K::Par:
      case K::Const:
        for (const xpp::odex::Binding &b : s.bindings)
          add((s.kind == K::Par ? "par " : "const ") + b.name + "=" + engine_text(b.value));
        break;
      case K::Wiener:
        for (const std::string &n : s.names) add("wiener " + n);
        break;
      case K::Network: add("special " + s.name + "=" + s.text); break;
      case K::History:
        for (const xpp::odex::Binding &b : s.bindings) add(b.name + "(0)=" + engine_text(b.value));
        break;
      case K::Aux:
      case K::Derived:
        for (const xpp::odex::Binding &b : s.bindings)
          add((s.kind == K::Aux ? "aux " : "!") + b.name + "=" + engine_text(b.value));
        break;
      case K::Ode: add(s.name + "'=" + engine_text(s.expr)); break;
      case K::Volterra: add("volt " + s.name + "=" + engine_text(s.expr)); break;
      case K::Fixed: add(s.name + "=" + engine_text(s.expr)); break;
      case K::Fun: {
        std::string line = s.name + "(";
        for (size_t i = 0; i < s.names.size(); i++) line += (i ? "," : "") + s.names[i];
        add(line + ")=" + engine_text(s.expr));
        break;
      }
      case K::Set: add("set " + s.name + " {" + s.text + "}"); break;
      case K::Table:
        add("table " + s.name + " % " + std::to_string(s.count) + " " + xpp::odex::print_number(s.lo) + " " +
            xpp::odex::print_number(s.hi) + " " + engine_text(s.expr));
        break;
      case K::Markov: {
        add("markov " + s.name + " " + std::to_string(s.count));
        for (int r = 0; r < s.count; r++) {
          std::string row;
          for (int c = 0; c < s.count; c++) row += "{" + engine_text(s.cells[r * s.count + c]) + "} ";
          add(row);
        }
        break;
      }
      case K::Event: {
        std::string line = "global " + std::to_string(s.count) + " {" + engine_text(s.expr) + "} {";
        for (size_t i = 0; i < s.bindings.size(); i++)
          line += (i ? ";" : "") + s.bindings[i].name + "=" + engine_text(s.bindings[i].value);
        add(line + "}");
        break;
      }
      case K::Boundary: add("bdry " + engine_text(s.expr)); break;
      default: break;
      }
    }
    return out;
  } catch (const Error &e) {
    return "error " + std::to_string(e.line) + ":" + std::to_string(e.col) + " " + e.cause;
  }
}

/* the model text written to build/test_odex_model.<ext> and loaded: 1
   when it loads */
int load_text(const char *text, const char *ext = "odex")
{
  std::string path = std::string("build/test_odex_model.") + ext;
  FILE *fp = fopen(path.c_str(), "w");
  if (!fp) return 0;
  fputs(text, fp);
  fclose(fp);
  char arg0[] = "test_odex";
  char *argv[] = {arg0, path.data(), nullptr};
  return xpp_load_model(2, argv, 1);
}

/* the model text's fixed variables and derived quantities as the builder
   made them, "name:fixed" or "name:derived" joined by '|' */
std::string quantities(const char *text, const char *ext = "odex")
{
  if (load_text(text, ext) != 1) return "does not load";
  std::string out;
  for (const Statement &s : xpp::model().statements) {
    if (s.kind == Statement::Kind::Fixed) out += (out.empty() ? "" : "|") + s.name + ":fixed";
    if (s.kind == Statement::Kind::Derived)
      for (const xpp::odex::Binding &b : s.bindings) out += (out.empty() ? "" : "|") + b.name + ":derived";
  }
  return out;
}

/* the value of the model's constant (a parameter, a const, a derived
   quantity) name */
double constant(const char *name)
{
  double v = -12345;
  get_val(xpp::upper_case(name), &v);
  return v;
}

} // namespace

int main(void)
{
  /* numbers */
  CHECK_STR(parsed(".5").c_str(), ".5");
  CHECK(expression(".5").value == 0.5);
  CHECK(expression("1.").value == 1.0);
  CHECK(expression("1e-3").value == 0.001);
  CHECK_STR(parsed("1e-3^2").c_str(), "(^ 1e-3 2)");
  CHECK(starts(parsed("2e"), "error 1:3 expected `'`, `=` or `(t) =` after `e`"));

  /* precedence (docs/odex.md), against .ode's quirks (odex-quirks.md) */
  CHECK_STR(parsed("2*3<4").c_str(), "(< (* 2 3) 4)");
  CHECK_STR(parsed("3-1<2").c_str(), "(< (- 3 1) 2)");
  CHECK_STR(parsed("1+2<3+4").c_str(), "(< (+ 1 2) (+ 3 4))");
  CHECK_STR(parsed("1/2<1").c_str(), "(< (/ 1 2) 1)");
  CHECK_STR(parsed("2^3^2").c_str(), "(^ 2 (^ 3 2))");
  CHECK_STR(parsed("-2^2").c_str(), "(- (^ 2 2))");
  CHECK_STR(parsed("-x^2").c_str(), "(- (^ x 2))");
  CHECK_STR(parsed("2^-3").c_str(), "(^ 2 (- 3))");
  CHECK_STR(parsed("2*-3").c_str(), "(* 2 (- 3))");
  CHECK_STR(parsed("x<-1").c_str(), "(< x (- 1))");
  CHECK_STR(parsed("-1<0").c_str(), "(< (- 1) 0)");
  CHECK_STR(parsed("a - -b").c_str(), "(- a (- b))");
  CHECK_STR(parsed("a-b-c").c_str(), "(- (- a b) c)");
  CHECK_STR(parsed("a/b/c").c_str(), "(/ (/ a b) c)");
  CHECK_STR(parsed("a mod b*c").c_str(), "(* (mod a b) c)");
  CHECK_STR(parsed("1 or 0 and 0").c_str(), "(or 1 (and 0 0))");
  CHECK_STR(parsed("1+1 and 1").c_str(), "(and (+ 1 1) 1)");
  CHECK_STR(parsed("not a < b").c_str(), "(not (< a b))");
  CHECK_STR(parsed("a != b").c_str(), "(!= a b)");
  CHECK_STR(parsed("a and not b or c").c_str(), "(or (and a (not b)) c)");
  CHECK_STR(parsed("f(x)^2").c_str(), "(^ (f x) 2)");
  CHECK_STR(parsed("x'").c_str(), "x'");

  /* if: its else takes what follows; else if chains */
  CHECK_STR(parsed("if 1>0 then 10 else 20+5").c_str(), "(if (> 1 0) 10 (+ 20 5))");
  CHECK_STR(parsed("(if 1>0 then 10 else 20)+5").c_str(), "(+ (if (> 1 0) 10 20) 5)");
  CHECK_STR(parsed("if a then 0 else if b then 1 else 2").c_str(), "(if a 0 (if b 1 2))");
  CHECK_STR(parsed("if(1>0)then 10 else 20").c_str(), "(if (> 1 0) 10 20)");
  CHECK_STR(parsed("if 1>0 then -10 else 20").c_str(), "(if (> 1 0) (- 10) 20)");
  CHECK_STR(parsed("2*if a then 1 else 2").c_str(), "(* 2 (if a 1 2))");

  /* calls: named arguments after the first */
  CHECK_STR(parsed("volterra(exp(-t), of=u, mu=0.5)").c_str(), "(volterra (exp (- t)) of=u mu=0.5)");
  CHECK_STR(parsed("max(a, b==c)").c_str(), "(max a (== b c))");
  CHECK_STR(parsed("sum(shift(u0, i'), from=0, to=9)").c_str(), "(sum (shift u0 i') from=0 to=9)");
  CHECK_STR(parsed("near(a, b, tol=1e-6)").c_str(), "(near a b tol=1e-6)");
  CHECK_STR(parsed("near(a, b)").c_str(), "(near a b)");

  /* errors, each at its line and column */
  CHECK(starts(parsed("3<2<1"), "error 1:4 comparisons do not chain"));
  CHECK(starts(parsed("2+ +3"), "error 1:4 there is no unary `+`"));
  CHECK(starts(parsed("if 1>0 then 10"), "error 1:15 expected `else`"));
  CHECK(starts(parsed("(1+2"), "error 1:5 expected `)` to close the `(` at 2:1"));
  CHECK(starts(parsed("a &&b"), "error 1:3 a statement cannot start with `&`"));
  CHECK(starts(parsed("1 then"), "error 1:3 `then` is a reserved word"));
  CHECK(starts(parsed("and"), "error 1:1 expected a value, found the reserved word `and`"));
  CHECK(starts(parsed("(a)(1)"), "error 1:4 only a function's name can be called"));

  /* the printer: .odex's own parentheses, and .ode's around a sign */
  CHECK_STR(reprinted("(2^3)^2").c_str(), "(2^3)^2");
  CHECK_STR(reprinted("2^3^2").c_str(), "2^3^2");
  CHECK_STR(reprinted("2*(3<4)").c_str(), "2*(3<4)");
  CHECK_STR(reprinted("(3<2)<1").c_str(), "(3<2)<1");
  CHECK_STR(reprinted("2*-3").c_str(), "2*(-3)");
  CHECK_STR(reprinted("-x^2+1").c_str(), "-x^2+1");
  CHECK_STR(reprinted("(-x)^2").c_str(), "(-x)^2");
  CHECK_STR(reprinted("-(a*b)").c_str(), "-(a*b)");
  CHECK_STR(reprinted("a+(b and c)").c_str(), "a+(b and c)");
  CHECK_STR(reprinted("(if a then 1 else 2)+5").c_str(), "(if a then 1 else 2)+5");
  CHECK_STR(reprinted("if a then 1 else if b then 2 else 3").c_str(), "if a then 1 else if b then 2 else 3");
  CHECK_STR(reprinted("not (a<b)").c_str(), "not a<b");
  CHECK_STR(reprinted("a-(b-c)").c_str(), "a-(b-c)");
  CHECK_STR(reprinted("max(-1, 2)").c_str(), "max(-1, 2)");
  CHECK_STR(reprinted("volterra(exp(-t), of=u)").c_str(), "volterra(exp(-t), of=u)");
  CHECK_STR(reprinted("1e-3").c_str(), "0.001");
  CHECK_STR(xpp::odex::print_number(0.1).c_str(), "0.1");

  /* every statement */
  {
    const char *text =
      "# a line comment\n"
      "/* a block /* nested */ comment\n spanning lines */\n"
      "par a = 1, b = 2*a\n"
      "init x = a\n"
      "x' = -a*x + w\n"
      "u(t) = sin(t) + volterra(exp(-t), of=u)\n"
      "w = x^2\n"
      "aux z = x + 1, z2 = 0\n"
      "const n = 3\n"
      "fun f(v, s) = v*s\n"
      "fun g(v, w) { let s = v + w  if s > 1 { return a*s } else if s > 0 { return b } else { return 0 } }\n"
      "@ total = 20, dt=0.05, meth=stiff, output=out.dat\n"
      "set fast = a = 2, b = 3\n"
      "table w1 \"w.tab\"\n"
      "table w2 exp(-abs(t)), n=51, lo=-25, hi=25\n"
      "markov m 2 {0} {a} {b} {0}\n"
      "wiener q1, q2\n"
      "event -1 x - 1, x = 0, a = a + 1\n"
      "boundary x - x'\n"
      "network k = conv(even, 51, 12, w1, x)\n"
      "solv y = 1\n"
      "0 = y + x\n"
      "only x, z\n"
      "\"{a=2} a comment the model shows\"\n";
    Parsed p;
    std::string err;
    try {
      p = xpp::odex::parse(text, "m.odex");
    } catch (const Error &e) {
      err = e.text();
    }
    CHECK_STR(err.c_str(), "");
    const std::vector<Statement> &s = p.statements;
    CHECK(s.size() == 22);
    if (s.size() == 22) {
      CHECK(s[0].kind == Statement::Kind::Par && s[0].bindings.size() == 2 && s[0].pos.line == 4);
      CHECK_STR(tree(s[0].bindings[1].value).c_str(), "(* 2 a)");
      CHECK(s[1].kind == Statement::Kind::Init);
      CHECK(s[2].kind == Statement::Kind::Ode && s[2].name == "x");
      CHECK(s[3].kind == Statement::Kind::Volterra && s[3].name == "u");
      CHECK(s[4].kind == Statement::Kind::Fixed && s[4].name == "w");
      CHECK(s[5].kind == Statement::Kind::Aux && s[5].bindings.size() == 2);
      CHECK(s[6].kind == Statement::Kind::Const && s[6].bindings[0].name == "n");
      CHECK(s[7].kind == Statement::Kind::Fun && !s[7].has_body && s[7].names.size() == 2);
      CHECK(s[8].kind == Statement::Kind::Fun && s[8].has_body && s[8].body.size() == 2);
      CHECK(s[9].kind == Statement::Kind::Options && s[9].options.size() == 4);
      if (s[9].options.size() == 4) {
        CHECK_STR(s[9].options[0].value.c_str(), "20");
        CHECK_STR(s[9].options[3].value.c_str(), "out.dat");
      }
      CHECK(s[10].kind == Statement::Kind::Set && s[10].name == "fast" && s[10].bindings.size() == 2);
      CHECK(s[11].kind == Statement::Kind::Table && s[11].text == "w.tab");
      CHECK(s[12].kind == Statement::Kind::Table && s[12].count == 51 && s[12].lo == -25 && s[12].hi == 25);
      CHECK(s[13].kind == Statement::Kind::Markov && s[13].count == 2 && s[13].cells.size() == 4);
      CHECK(s[14].kind == Statement::Kind::Wiener && s[14].names.size() == 2);
      CHECK(s[15].kind == Statement::Kind::Event && s[15].count == -1 && s[15].bindings.size() == 2);
      CHECK(s[16].kind == Statement::Kind::Boundary);
      CHECK_STR(tree(s[16].expr).c_str(), "(- x x')");
      CHECK(s[17].kind == Statement::Kind::Network && s[17].text == "conv" && s[17].call_args.size() == 5);
      CHECK(s[18].kind == Statement::Kind::Solv && s[18].name == "y");
      CHECK(s[19].kind == Statement::Kind::Dae);
      CHECK(s[20].kind == Statement::Kind::Only && s[20].names.size() == 2);
      CHECK(s[21].kind == Statement::Kind::Comment && s[21].text == "{a=2} a comment the model shows");
    }
  }

  /* statement errors */
  CHECK(starts(model_error("par and = 1\n"), "1:5 `and` is a reserved word and cannot be a name"));
  CHECK(starts(model_error("x' = 1\npar = 1\n"), "2:5 expected a name"));
  CHECK(starts(model_error("x' = 1\n/* never\n closed\n"), "2:1 this /* comment is never closed"));
  CHECK(starts(model_error("x' = 1\n\"open\n"), "2:1 this string is never closed"));
  CHECK(starts(model_error("fun g(v) { if v > 0 { return 1 } }\n"), "1:34 fun g can end here without a return"));
  CHECK(starts(model_error("fun g(v) { return 1\nx' = 1\n"), "2:1 nothing can follow a return"));
  CHECK(starts(model_error("fun g(v) { return 1\n"), "2:1 missing `}` for the block opened at 1:10"));
  CHECK(starts(model_error("fun g(v) { let v = 1 return v }\n"), "1:16 v is already a name here"));
  CHECK(starts(model_error("@ total=2*3\n"), "1:9 the option total's value `2*3` is not a number"));
  CHECK(starts(model_error("@ total=(4)\n"), "1:9 the option total's value `(4)` is neither"));
  CHECK(starts(model_error("@ total=10 dt=1\n"), "1:12 expected `,` between the items of an @ line"));
  CHECK(starts(model_error("par a=1 b=2\n"), "1:9 expected `,` between the items of par"));
  CHECK(model_error("@ total = 0.03, dt=0.01\nx'=1\n").empty());
  CHECK(model_error("par a=1,\n  b=2\nx'=a*b\n").empty());
  CHECK(starts(model_error("x = 1 +\n"), "2:1 expected a value, found the end of the file"));
  CHECK(starts(model_error("done\n"), "2:1 expected `'`, `=` or `(t) =` after `done`"));
  CHECK(starts(model_error("x(t+1) = 1\n"), "1:2 expected `'`, `=` or `(t) =` after `x`"));
  CHECK(starts(model_error("markov m 1 {0}\n"), "1:8 m needs at least 2 states"));
  CHECK(starts(model_error("event 2 x, x=0\n"), "1:7 the event's direction is 1, -1 or 0"));
  CHECK(starts(model_error("include \"no/such/file.incx\"\n"), "1:9 cannot read the included file no/such/file.incx"));
  CHECK(xpp::odex::is_reserved("volterra") && !xpp::odex::is_reserved("e") && !xpp::odex::is_reserved("int"));
  CHECK(xpp::odex::is_reserved("near") && !xpp::odex::is_reserved("neartol"));
  CHECK(xpp::odex::is_name("v_1") && !xpp::odex::is_name("1v") && !xpp::odex::is_name("_v"));
  CHECK(starts(model_error("par near = 1\nx' = 1\n"), "1:5 `near` is a reserved word and cannot be a name"));

  /* the statements a model becomes, their formulas the expression
     engine's text: parentheses keep .odex's grouping, a sign and an if
     always bracketed */
  CHECK_STR(lowered("x' = 2*3<4\n").c_str(), "x'=(2*3)<4");
  CHECK_STR(lowered("x' = 2^3^2\n").c_str(), "x'=2^(3^2)");
  CHECK_STR(lowered("x' = (2^3)^2\n").c_str(), "x'=2^3^2");
  CHECK_STR(lowered("x' = -x^2\n").c_str(), "x'=(-x^2)");
  CHECK_STR(lowered("x' = 2*-x\n").c_str(), "x'=2*(-x)");
  CHECK_STR(lowered("x' = a-(b-c)\npar a=1,b=2,c=3\n").c_str(), "x'=a-(b-c)|par a=1|par b=2|par c=3");
  CHECK_STR(lowered("x' = 1 or 0 and 0\n").c_str(), "x'=1|0&0");
  CHECK_STR(lowered("x' = (1 or 0) and x\n").c_str(), "x'=(1|0)&x");
  CHECK_STR(lowered("x' = 1+1 and 1\n").c_str(), "x'=(1+1)&1");
  CHECK_STR(lowered("x' = x != 1\n").c_str(), "x'=not(x==1)");
  CHECK_STR(lowered("x' = not x < 1\n").c_str(), "x'=not(x<1)");
  CHECK_STR(lowered("x' = x mod 3\n").c_str(), "x'=mod(x,3)");
  CHECK_STR(lowered("x' = (if x>0 then 10 else 20)+5\n").c_str(), "x'=(if(x>0)then(10)else(20))+5");
  CHECK_STR(lowered("x' = if x>0 then 1 else if x<0 then -1 else 0\n").c_str(),
            "x'=(if(x>0)then(1)else((if(x<0)then((-1))else(0))))");
  CHECK_STR(lowered("x' = sum(shift(x, i'), from=0, to=2)\n").c_str(), "x'=(sum(0,2)of(shift(x,i')))");
  CHECK_STR(lowered("u(t) = sin(t)+volterra(exp(-t), of=u, mu=0.5)\n").c_str(), "volt u=sin(t)+(int[0.5]{exp((-t))#u})");
  /* par takes numbers (docs/odex.md question 9) */
  CHECK_STR(lowered("par a = -2, b = 1e-3\nx' = b\n").c_str(), "par a=-2|par b=0.001|x'=b");
  CHECK(starts(lowered("par a = 2*pi\nx' = a\n"), "error 1:9 a parameter's value is a number, not `2*pi`"));
  CHECK_STR(lowered("init x = a\npar a = 2\nx' = -x\n").c_str(), "par a=2|x'=(-x)");
  CHECK_STR(lowered("x' = -delay(x, 1)\nhistory x = cos(t)\n").c_str(), "x'=(-delay(x,1))|x(0)=cos(t)");
  /* an event's formula whole: the builder takes its actions one by one,
     so == needs no rewriting (W79) */
  CHECK_STR(lowered("x' = 1\nevent 1 x, x = (x == 1) + (x != 2)\n").c_str(),
            "x'=1|global 1 {x} {x=x==1+not(x==2)}");
  CHECK_STR(lowered("fun g(v, w) { let s = v + w  if s > 1 { return 2*s } else if s > 0 { return 1 } return 0 }\nx' = g(x, 1)\n").c_str(),
            "g(v,w)=(if((v+w)>1)then(2*(v+w))else((if((v+w)>0)then(1)else(0))))|x'=g(x,1)");
  CHECK_STR(lowered("fun g(v) { if v > 0 { let s = 2  return s } let s = 3  return s }\nx' = g(x)\n").c_str(),
            "g(v)=(if(v>0)then(2)else(3))|x'=g(x)");
  CHECK_STR(lowered("x' = 1\nevent -1 x - 1, x = 0, out_put = 1\n").c_str(), "x'=1|global -1 {x-1} {x=0;out_put=1}");
  CHECK_STR(lowered("x' = 1\nboundary x - x'\n").c_str(), "x'=1|bdry x-x'");
  CHECK_STR(lowered("x' = w\nw = x^2\naux w = w\n").c_str(), "x'=w|w=x^2|aux w=w");
  CHECK_STR(lowered("x' = 1\ntable f exp(-abs(t)), n=51, lo=-25, hi=25\n").c_str(), "x'=1|table f % 51 -25 25 exp((-abs(t)))");
  CHECK_STR(lowered("x' = 1\nset s = x = -1, meth = stiff\n").c_str(), "x'=1|set s {x=-1,meth=stiff}");
  CHECK_STR(lowered("x' = 1\n@ total = 20, meth=stiff\n").c_str(), "x'=1|@ total=20,meth=stiff");
  CHECK_STR(lowered("markov m 2 {0} {a} {1} {0}\npar a=1\n").c_str(), "markov m 2|{0} {a} |{1} {0} |par a=1");
  CHECK_STR(lowered("x' = 1\n\"{a=1} a comment\"\n").c_str(), "x'=1|\" {a=1} a comment");

  /* near(a, b[, tol=]) (docs/odex.md question 7, W78): |a-b| <=
     tol*max(1,|a|,|b|), tol the model's neartol (default 1e-9) or the
     call's own, always written as a literal */
  {
    const std::string t9 = xpp::odex::print_number(1e-9);
    const std::string t6 = xpp::odex::print_number(1e-6);
    CHECK_STR(lowered("x' = 1\naux flag = near(x, 1)\n").c_str(),
              ("x'=1|aux flag=(abs((x)-(1))<=(" + t9 + ")*max(1,max(abs(x),abs(1))))").c_str());
    CHECK_STR(lowered("x' = 1\naux flag = near(x, 1, tol=1e-6)\n").c_str(),
              ("x'=1|aux flag=(abs((x)-(1))<=(" + t6 + ")*max(1,max(abs(x),abs(1))))").c_str());
    CHECK_STR(lowered("x' = 1\n@ neartol=1e-6\naux flag = near(x, 1)\n").c_str(),
              ("x'=1|aux flag=(abs((x)-(1))<=(" + t6 + ")*max(1,max(abs(x),abs(1))))").c_str());
    /* the @ line drops neartol, keeping any options beside it */
    CHECK_STR(lowered("x' = 1\n@ neartol=1e-6, dt=.1\n").c_str(), "x'=1|@ dt=.1");
    CHECK_STR(lowered("x' = 1\nevent 1 near(x, 1) - 1, x = 0\n").c_str(),
              ("x'=1|global 1 {(abs((x)-(1))<=(" + t9 + ")*max(1,max(abs(x),abs(1))))-1} {x=0}").c_str());
  }

  /* what the checks refuse, at its line and column */
  CHECK(starts(lowered("x' = y\n"), "error 1:6 the name `y` is not declared"));
  CHECK(starts(lowered("par V = 1\nx' = v\n"), "error 2:6 the name `v` is not declared (names have case: did you mean `V`?)"));
  CHECK(starts(lowered("par V = 1\npar v = 2\nx' = 1\n"), "error 2:5 `v` and `V` (1:5) differ only by case"));
  CHECK(starts(lowered("par Sin = 1\nx' = 1\n"), "error 1:5 `Sin` would be read as the built-in `sin`"));
  CHECK(starts(lowered("par a = 1\npar a = 2\nx' = 1\n"), "error 2:5 `a` is already declared at 1:5"));
  CHECK(starts(lowered("x' = z\naux z = x\n"), "error 1:6 `z` is an aux quantity"));
  CHECK(starts(lowered("x' = max(x)\n"), "error 1:6 max takes 2 arguments, not 1"));
  CHECK(starts(lowered("fun f(a) = a\nx' = f(x, 1)\n"), "error 2:6 f takes 1 argument, not 2"));
  CHECK(starts(lowered("x' = sin\n"), "error 1:6 `sin` is a built-in function: call it"));
  CHECK(starts(lowered("par a = 1\nfun g(v) { let a = v  return a }\nx' = g(x)\n"), "error 2:12 fun g cannot set `a`, a parameter"));
  CHECK(starts(lowered("par a = 1\ninit a = 2\nx' = 1\n"), "error 2:6 `a` is not a variable"));
  CHECK(starts(lowered("par a = x\nx' = 1\n"), "error 1:9 a parameter's value is a number, not `x`"));
  CHECK(starts(lowered("x' = delay(2*x, 1)\n"), "error 1:12 delay's first argument is the name of a variable"));
  CHECK(starts(lowered("aux z = volterra(x)\nx' = 1\n"), "error 1:9 volterra(...) belongs in"));
  CHECK(starts(lowered("x' = x'\n"), "error 1:6 `x'` cannot be read here"));
  CHECK(starts(lowered("x' = x[1]\n"), "error 1:6 the name `x1` is not declared"));
  CHECK(starts(lowered("x' = sum(i', from=0)\n"), "error 1:6 sum needs from= and to="));
  CHECK(starts(lowered("x' = 1\nset s = a = x+1\n"), "error 2:13 a set's value is a number or a name, not `x+1` (a=)"));
  CHECK(starts(lowered("x' = 1\naux flag = near(x)\n"), "error 2:12 near takes 2 or 3 arguments, not 1"));
  CHECK(starts(lowered("x' = 1\naux flag = near(x, 1, 1, 1)\n"), "error 2:12 near takes 2 or 3 arguments, not 4"));
  CHECK(starts(lowered("x' = 1\naux flag = near(x, 1, 1e-6)\n"), "error 2:23 near's third argument must be named tol="));
  CHECK(starts(lowered("x' = 1\naux flag = near(x, 1, eps=1e-6)\n"), "error 2:27 near has no argument named `eps`: only tol="));
  CHECK(starts(lowered("x' = 1\naux flag = near(x, 1, tol=-1)\n"), "error 2:27 near's tol= is not a positive number"));
  CHECK(starts(lowered("x' = 1\naux flag = near(x, 1, tol=x)\n"), "error 2:27 near's tol= is not a positive number"));
  CHECK(starts(lowered("x' = 1\n@ neartol=-1\n"), "error 2:11 @ neartol=-1 is not a positive number"));
  CHECK(starts(lowered("x' = 1\n@ neartol=0\n"), "error 2:11 @ neartol=0 is not a positive number"));

  /* arrays (docs/odex.md "Arrays", W80): a trailing range on a
     statement, its index named, both ends included, by a positive step */
  {
    Parsed p = xpp::odex::parse("x[j]' = -x[j] + x[j-1]  for j in 1..n\naux s[j] = x[j]^2  for j in 1..n by 2\n",
                                "m.odex");
    CHECK(p.statements.size() == 2);
    if (p.statements.size() == 2) {
      const Statement &a = p.statements[0], &b = p.statements[1];
      CHECK(a.kind == Statement::Kind::Ode && a.name == "x" && a.name_index && tree(*a.name_index) == "j");
      CHECK_STR(tree(a.expr).c_str(), "(+ (- ([] x j)) ([] x (- j 1)))");
      CHECK(a.range.index == "j" && tree(a.range.lo) == "1" && tree(a.range.hi) == "n" && tree(a.range.step) == "1");
      CHECK(a.range.index_pos.line == 1 && a.range.index_pos.col == 29);
      CHECK(b.kind == Statement::Kind::Aux && b.bindings.size() == 1 && b.bindings[0].index);
      CHECK(tree(b.range.step) == "2");
    }
  }
  CHECK(starts(model_error("x[j]' = 1 for j in 1..3..2\n"), "1:24 a range is a..b, its step written `by k`"));
  CHECK(starts(model_error("x[j]' = 1 for j in 1..3 step 2\n"), "1:25 a range's step is written `by k`"));
  CHECK(starts(model_error("x[j]' = 1 for j in 1..3 for i in 1..2\n"), "1:25 a statement takes one range"));
  CHECK(starts(model_error("x[j]' = 1 for j 1..3\n"), "1:17 expected `in` after for j"));
  CHECK(starts(model_error("x[j]' = 1 for by in 1..3\n"), "1:15 `by` is a reserved word"));
  CHECK(starts(model_error("par const = 1\n"), "1:5 `const` is a reserved word"));
  CHECK(starts(model_error("x' = 1\n!d = 2\n"), "2:1 `!d = expr` is .ode's: in .odex write `d = expr`"));
  CHECK(model_error("par in = 1\nx' = in\n").empty()); /* for and in are words only in a range */
  /* the copies, each element the name followed by its index's value */
  CHECK_STR(lowered("const n = 3\nx[j]' = -x[j] for j in 1..n\n").c_str(), "const n=3|x1'=(-x1)|x2'=(-x2)|x3'=(-x3)");
  CHECK_STR(lowered("x[j]' = j for j in 0..4 by 2\n").c_str(), "x0'=0|x2'=2|x4'=4");
  CHECK_STR(lowered("x[2*j+1]' = x[2*j+1] for j in 0..1\n").c_str(), "x1'=x1|x3'=x3");
  CHECK_STR(lowered("x[1]' = 0\nx[j]' = x[j-1] for j in 2..3\n").c_str(), "x1'=0|x2'=x1|x3'=x2");
  CHECK_STR(lowered("x[j]' = if j == 1 then 0 else x[j] for j in 1..2\n").c_str(),
            "x1'=(if(1==1)then(0)else(x1))|x2'=(if(2==1)then(0)else(x2))");
  CHECK_STR(lowered("x[j]' = 1 for j in 1..2\ninit x[j] = j/2 for j in 1..2\naux s[j] = x[j]^2 for j in 1..2\n").c_str(),
            "x1'=1|x2'=1|aux s1=x1^2|aux s2=x2^2");
  CHECK_STR(lowered("x[j]' = w[j] for j in 1..2\nwiener w[j] for j in 1..2\n").c_str(), "x1'=w1|x2'=w2|wiener w1|wiener w2");
  CHECK_STR(lowered("x[j]' = 1 for j in 0..1\nevent 1 t - 1, x[j] = 0 for j in 0..1\n").c_str(),
            "x0'=1|x1'=1|global 1 {t-1} {x0=0}|global 1 {t-1} {x1=0}");
  CHECK_STR(lowered("x[j]' = 1 for j in 0..1\ntable w \"w.tab\"\nnetwork k[j] = conv(even, 2, 1, w, x[j]) for j in 0..1\n").c_str(),
            "x0'=1|x1'=1|table w % 0 0 0 0|special k0=conv(even,2,1,w,x0)|special k1=conv(even,2,1,w,x1)");
  /* what an array refuses, at its line and column, naming the copy */
  CHECK(starts(lowered("x[j]' = y[j] for j in 1..2\n"), "error 1:9 the name `y1` is not declared (where j = 1)"));
  CHECK(starts(lowered("x[j]' = x[j-1] for j in 0..1\n"), "error 1:11 the index of x `j-1` is -1: an element's index is 0 or more (where j = 0)"));
  CHECK(starts(lowered("par j = 1\nx[j]' = 1 for j in 1..2\n"), "error 2:15 the index `j` is a parameter, declared at 1:5"));
  CHECK(starts(lowered("par n = 3\nx[j]' = 1 for j in 1..n\n"), "error 2:23 the range's last index reads numbers, the consts before it"));
  CHECK(starts(lowered("x[j]' = 1 for j in 1..2.5\n"), "error 1:23 the range's last index `2.5` is 2.5, not a whole number"));
  CHECK(starts(lowered("x[j]' = 1 for j in 3..1\n"), "error 1:20 the range 3..1 is empty"));
  CHECK(starts(lowered("x[j]' = 1 for j in 1..3 by 0\n"), "error 1:28 the range's step is a whole number above 0, not 0"));
  CHECK(starts(lowered("x[j]' = 1 for j in 1..3 by -1\n"), "error 1:28 the range's step is a whole number above 0, not -1"));
  CHECK(starts(lowered("x' = 1\n@ total=10 for j in 1..2\n"), "error 2:12 this statement takes no range"));
  CHECK(starts(lowered("x[j]' = 1 for j in 1..2\nx[2]' = 1\n"), "error 2:1 `x2` is already declared at 1:1"));

  /* const: fixed at load, from numbers and the consts before it */
  CHECK_STR(lowered("const n = 2, m = n*3+1\nx' = m\n").c_str(), "const n=2|const m=7|x'=m");
  CHECK(starts(lowered("par a = 1\nconst n = a\nx' = n\n"), "error 2:11 the value of const n reads numbers, the consts before it"));
  CHECK(starts(lowered("const n = m\nconst m = 1\nx' = n\n"), "error 1:11 the value of const n reads numbers, the consts before it"));
  CHECK(starts(lowered("const n = 1\nx' = 1\nevent 1 t, n = 2\n"), "error 3:12 an event sets a variable"));

  /* derived quantities (docs/odex.md question 9): a fixed quantity that
     reads only parameters, consts and pure functions is worked out when
     a parameter changes, whichever reader */
  CHECK_STR(quantities("par a = 2\nconst n = 3\nfun f(u) = u*a\nfun g(u) = u+ran(1)\n"
                       "d = -a\ne = f(n)+d\nw = x*a\nr = ran(1)\nq = g(1)\nk = t*a\nv = x\nx' = d+e+w+r+q+k+v\n")
                .c_str(),
            "d:derived|e:derived|w:fixed|r:fixed|q:fixed|k:fixed|v:fixed");
  /* one read later, or needed as a variable (aux of its name, delay), stays one */
  CHECK_STR(quantities("par a = 2\ne = d*2\nd = a\nx' = e\n").c_str(), "e:fixed|d:derived");
  CHECK_STR(quantities("par a = 2\nd = a\naux d = d\nx' = d\n").c_str(), "d:fixed");
  CHECK_STR(quantities("par a = 2\nd = a\nx' = delay(d, 1)\n").c_str(), "d:fixed");
  /* an .ode's fixed quantity of parameters stays fixed, as XPPAUT's; the
     same .odex d = expr is derived (maintainer, 2026-09-29) */
  CHECK_STR(quantities("par a=2\n!tr=-a\nw=a*2\nv=x\nx'=tr+w+v\n", "ode").c_str(), "TR:derived|W:fixed|V:fixed");
  CHECK_STR(quantities("par a = 2\ntr = -a\nw = a*2\nv = x\nx' = tr+w+v\n").c_str(), "tr:derived|w:derived|v:fixed");
  {
    /* --convert's check builds the .ode as the .odex will be */
    const OdeAsOdex as_odex;
    CHECK_STR(quantities("par a=2\nw=a*2\nx'=w\n", "ode").c_str(), "W:derived");
  }
  CHECK_STR(quantities("par a=2\nw=a*2\nx'=w\n", "ode").c_str(), "W:fixed");
  CHECK(load_text("par a = 2\nd = a*3\nx' = d\n") == 1 && constant("d") == 6);
  {
    /* worked out again when a parameter changes, as .ode's ! is */
    set_val("A", 5);
    evaluate_derived();
    CHECK(constant("d") == 15);
  }

  /* --convert: what the .ode's reader understood, its quirks explicit */
  {
    char arg0[] = "test_odex", model[] = "tools/models/odex_quirks.ode";
    char *argv[] = {arg0, model, nullptr};
    std::string text, err;
    CHECK(xpp_load_model(2, argv, 1) == 1);
    try {
      text = xpp::odex::convert_model(true, xpp::odex::Ask());
    } catch (const Error &e) {
      err = e.text();
    }
    CHECK_STR(err.c_str(), "");
    auto has = [&text](const char *line) {
      const bool found = text.find(std::string("\n") + line + "\n") != std::string::npos;
      if (!found) printf("  missing line: %s\n", line);
      return found;
    };
    CHECK(has("# renamed: and is and_ here (.odex reserves and)"));
    CHECK(has("par a = 2, b = 3"));
    CHECK(has("par and_ = 1"));
    CHECK(has("par Gk = 0.5"));
    CHECK(has("x' = (2^3)^2+a*(3<4)-Gk"));
    CHECK(has("y' = -x^2+(if x>0 then 1 else 2)+5"));
    CHECK(has("z' = 1/(if x then x else 2.23e-15)+x/2+x/(1+exp(-x))+x/a+and_"));
    CHECK(has("aux w = x and y or z"));
    CHECK(has("# y=2*3 in the .ode: XPP reads the number at its front, 2"));
    CHECK(has("init x = 1, y = 2"));
    CHECK(has("const nn = 5"));
    CHECK(has("dd = a*2"));
    CHECK(has("v[j]' = -v[j]+j for j in 1..3"));
    CHECK(has("aux sq[j] = v[j]^2 for j in 1..3"));
    CHECK(has("event 1 x-1, y = y/(if x then x else 2.23e-15), z = 0"));
    CHECK(has("@ total=2, dt=.1"));
    CHECK(has("# anything here is kept as a comment"));
    CHECK(text.find("# @ total=2*3 in the .ode: XPP reads the number at its front, 2") != std::string::npos);
    CHECK(text.find("the .ode's lines 8") != std::string::npos); /* gk spelled Gk */
    /* what the converted text reads back as */
    std::string back;
    try {
      xpp::odex::ready(xpp::odex::parse(text, "odex_quirks.odex"));
    } catch (const Error &e) {
      back = e.text();
    }
    CHECK_STR(back.c_str(), "");
  }

  /* --convert refuses an .ode's !d = expr that reads a variable: .odex's
     d = expr would be worked out at every evaluation, not only when a
     parameter changes */
  {
    std::string err;
    CHECK(load_text("par a=1\n!d=x*a\nx'=d\n", "ode") == 1);
    try {
      xpp::odex::convert_model(true, xpp::odex::Ask());
    } catch (const Error &e) {
      err = e.cause;
    }
    CHECK(starts(err, "!d = x*a reads t, a variable, a random function or a derived quantity after it"));
  }

  /* near(a, b[, tol=]) numerically: tools/models/near_test.odex's aux
     columns, integrated as -silent does (Initialconds/Go) and read from
     the stored columns (not the translation's text) */
  {
    char arg0[] = "test_odex", model[] = "tools/models/near_test.odex";
    char *argv[] = {arg0, model, nullptr};
    CHECK(xpp_load_model(2, argv, 1) == 1);
    xpp_batch_start();
    run_the_commands(M_IG);
    const DataStore &d = xpp::session().data_store;
    CHECK(d.rows > 0);
    if (d.rows > 0) {
      /* t=0's row: t, x, then the auxes in order */
      auto aux = [&d](int k) { return static_cast<double>(d.col[2 + k][0]); };
      CHECK(aux(0) == 1);
      CHECK(aux(1) == 0);
      CHECK(aux(2) == 1); /* the boundary, |a-b| == tol*max(...): near is <=, so true */
      CHECK(aux(3) == 0);
      CHECK(aux(4) == 1); /* within the default 1e-9 */
    }
  }

  TEST_REPORT("odex: grammar");
}
