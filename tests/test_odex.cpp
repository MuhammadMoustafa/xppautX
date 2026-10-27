/* .odex's tokenizer, grammar and printer (core/odex.h, docs/odex.md): the
   precedence and grouping of docs/odex.md's table, every statement, and
   an error at the right line and column for each kind of mistake. */
#include "load_args.h"
#include "xpptest.h"
#include "odex.h"
#include "derived.h"
#include "expr.h"
#include "expr_program.h"
#include "form_ode.h"
#include "ode_read.h"
#include "model.h"
#include "expr.h"
#include "xpp_batch.h"
#include "xpp_ui.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "session.h"
#include "storage.h"
#include "xpp_util.h"
#include "typed_values.h"
#include "menudrive.h"
#include "solver.h"
#include "tabular.h"

#include <cmath>
#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <vector>

using xpp::Error;
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
  Parsed p = xpp::odex::parse(xpp::client_session().model(), std::string("x' =\n") + text, "e.odex");
  if (p.statements.size() != 1) throw xpp::odex::error_at("e.odex", xpp::odex::Pos{0, 2, 1}, "not one expression");
  return p.statements[0].expr;
}

std::string parsed(const char *text)
{
  try {
    return tree(expression(text));
  } catch (const Error &e) {
    return "error " + std::to_string(e.place.line - 1) + ":" + std::to_string(e.place.col) + " " + e.what;
  }
}

/* the error a model gives: "line:col message", "" when it parses */
std::string model_error(const char *text)
{
  try {
    xpp::odex::parse(xpp::client_session().model(), text, "m.odex");
  } catch (const Error &e) {
    return std::to_string(e.place.line) + ":" + std::to_string(e.place.col) + " " + e.what;
  }
  return "";
}

/* text into the file at path (an import's file) */
void put(const char *path, const char *text)
{
  xpp::Writer w(path);
  CHECK(w && w.write(text) && w.commit());
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
  try {
    const Parsed p = xpp::odex::ready(xpp::client_session().model(), xpp::odex::parse(xpp::client_session().model(), text, "m.odex"));
    std::string out;
    auto add = [&out](const std::string &line) { out += (out.empty() ? "" : "|") + line; };
    for (const Statement &s : p.statements) {
      switch (s.kind) {
      case Statement::Kind::Comment: add("\" " + s.text); break;
      case Statement::Kind::Options: {
        std::string line = "@ ";
        for (const xpp::odex::Option &o : s.options) line += (line.size() > 2 ? "," : "") + o.name + "=" + o.value;
        add(line);
        break;
      }
      case Statement::Kind::Par:
      case Statement::Kind::Const:
        for (const xpp::odex::Binding &b : s.bindings)
          add((s.kind == Statement::Kind::Par ? "par " : "const ") + b.name + "=" + engine_text(b.value));
        break;
      case Statement::Kind::Wiener:
        for (const std::string &n : s.names) add("wiener " + n);
        break;
      case Statement::Kind::Network: add("special " + s.name + "=" + s.text); break;
      case Statement::Kind::History:
        for (const xpp::odex::Binding &b : s.bindings) add(b.name + "(0)=" + engine_text(b.value));
        break;
      case Statement::Kind::Aux:
      case Statement::Kind::Derived:
        for (const xpp::odex::Binding &b : s.bindings)
          add((s.kind == Statement::Kind::Aux ? "aux " : "!") + b.name + "=" + engine_text(b.value));
        break;
      case Statement::Kind::Ode: add(s.name + "'=" + engine_text(s.expr)); break;
      case Statement::Kind::Volterra: add("volt " + s.name + "=" + engine_text(s.expr)); break;
      case Statement::Kind::Fixed: add(s.name + "=" + engine_text(s.expr)); break;
      case Statement::Kind::Fun: {
        std::string line = s.name + "(";
        for (size_t i = 0; i < s.names.size(); i++) line += (i ? "," : "") + s.names[i];
        add(line + ")=" + engine_text(s.expr));
        break;
      }
      case Statement::Kind::Set: {
        std::string line = "set " + s.name + " {" + s.text + "}";
        for (const xpp::odex::Option &o : s.options) line += " " + o.group + "." + o.name + "=" + o.value;
        add(line);
        break;
      }
      case Statement::Kind::Table:
        add("table " + s.name + " % " + std::to_string(s.count) + " " + xpp::odex::print_number(s.lo) + " " +
            xpp::odex::print_number(s.hi) + " " + engine_text(s.expr));
        break;
      case Statement::Kind::Markov: {
        add("markov " + s.name + " " + std::to_string(s.count));
        for (int r = 0; r < s.count; r++) {
          std::string row;
          for (int c = 0; c < s.count; c++) row += "{" + engine_text(s.cells[r * s.count + c]) + "} ";
          add(row);
        }
        break;
      }
      case Statement::Kind::Event: {
        std::string line = "global " + std::to_string(s.count) + " {" + engine_text(s.expr) + "} {";
        for (size_t i = 0; i < s.bindings.size(); i++)
          line += (i ? ";" : "") + s.bindings[i].name + "=" + engine_text(s.bindings[i].value);
        add(line + "}");
        break;
      }
      case Statement::Kind::Boundary: add("bdry " + engine_text(s.expr)); break;
      default: break;
      }
    }
    return out;
  } catch (const Error &e) {
    return "error " + std::to_string(e.place.line) + ":" + std::to_string(e.place.col) + " " + e.what;
  }
}

/* the model text written to build/test_odex_model.<ext> and loaded: 1
   when it loads */
int load_text(const char *text, const char *ext = "odex")
{
  std::string path = std::string("build/test_odex_model.") + ext;
  if (std::string(ext) == "ode") xpp::files::remove(xpp::odex::odex_name(path));
  FILE *fp = fopen(path.c_str(), "w");
  if (!fp) return 0;
  fputs(text, fp);
  fclose(fp);
  char arg0[] = "test_odex";
  char *argv[] = {arg0, path.data(), nullptr};
  return load_args(2, argv, 1).has_value();
}

/* the model text's fixed variables and derived quantities as the builder
   made them, "name:fixed" or "name:derived" joined by '|' */
std::string quantities(const char *text, const char *ext = "odex")
{
  if (load_text(text, ext) != 1) return "does not load";
  std::string out;
  for (const Statement &s : xpp::client_session().model().statements) {
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
  xpp::get_val(xpp::client_session(), xpp::upper_case(name), &v);
  return v;
}

} // namespace

int main(void)
{
  xpp::batch_options.yes = true; /* Conversion fixtures accept the UI owner's suggested names. */
  /* --check wins over --yes for every question kind, without changing entries. */
  xpp::batch_options.check = true;
  std::string suggestion = "kept";
  CHECK(xpp::new_string("name", suggestion) == 0);
  CHECK(suggestion == "kept");
  CHECK(!xpp::confirm("replace?"));
  CHECK(xpp::file_selector("file", suggestion, "*") == 0);
  CHECK(suggestion == "kept");
  const std::array<std::string, 2> answers{"Allow", "Refuse"};
  CHECK(xpp::Choose("path", "permission?", answers, "ar", 'a') == 0);
  xpp::batch_options.check = false;

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
      "options { integration: { total_time: 20, dt: 0.05, method: stiff }, output: { output: \"out.dat\" } }\n"
      "set \"fast\" { a: 2, b: 3 }\n"
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
      p = xpp::odex::parse(xpp::client_session().model(), text, "m.odex");
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
  CHECK(starts(model_error("options { integration: { total_time: 2*3 } }\n"), "1:38 the option total_time's value `2*3` is not a number"));
  CHECK(starts(model_error("options { integration: { total_time: (4) } }\n"), "1:38 the option total_time's value `(4)` is not a number"));
  CHECK(starts(model_error("options { integration: { total_time: 10 dt: 1 } }\n"), "1:41 expected `}` to close the block, found `dt`"));
  CHECK(starts(model_error("@ total_time=10\n"), "1:1 a statement cannot start with `@`"));
  CHECK(starts(model_error("options { integration: { total: 10 } }\n"), "1:26 `total` is XPPAUT's word: the key is `total_time`"));
  CHECK(starts(model_error("options { plot: { total_time: 10 } }\n"), "1:19 `total_time` is in the group integration, not plot"));
  CHECK(starts(model_error("options { integration: { dt: 1, dt: 2 } }\n"), "1:33 `dt` is given twice"));
  CHECK(starts(model_error("options { integration: { method: Runge-Kutta } }\n"), "1:34 the option method's value `Runge-Kutta` is none of"));
  CHECK(starts(model_error("options { output: { logfile: out.dat } }\n"), "1:30 the option logfile takes a path in double quotes"));
  CHECK(starts(model_error("options { integration: { dt: \"1\" } }\n"), "1:30 the option dt takes a bare word or number"));
  CHECK(starts(model_error("options { nowhere: { } }\n"), "1:11 `nowhere` is not a group of options"));
  CHECK(starts(model_error("options { }\noptions { }\n"), "2:1 options is written once in a file"));
  CHECK(starts(model_error("par a=1 b=2\n"), "1:9 expected `,` between the items of par"));
  CHECK(model_error("options { integration: { total_time: 0.03, dt: 0.01, }, }\nx'=1\n").empty());
  CHECK(model_error("options { output: { logfile: \"C:/Users/RUNNER~1/AppData/Local/Temp/out side\\\"x\\\\.dat\" } }\nx'=1\n").empty());
  CHECK(model_error("par a=1,\n  b=2\nx'=a*b\n").empty());
  CHECK(starts(model_error("x = 1 +\n"), "2:1 expected a value, found the end of the file"));
  CHECK(starts(model_error("done\n"), "2:1 expected `'`, `=` or `(t) =` after `done`"));
  CHECK(starts(model_error("x(t+1) = 1\n"), "1:2 expected `'`, `=` or `(t) =` after `x`"));
  CHECK(starts(model_error("markov m 1 {0}\n"), "1:8 m needs at least 2 states"));
  CHECK(starts(model_error("event 2 x, x=0\n"), "1:7 the event's direction is 1, -1 or 0"));
  CHECK(starts(model_error("import \"no/such/file.odex\"\n"), "1:8 cannot read the imported file no/such/file.odex"));
  CHECK(starts(model_error("include \"f.odex\"\n"), "1:9 expected `'`, `=` or `(t) =` after `include`")); /* no include any more */

  /* imports (W282): everything, parts; a clash, a cycle, a missing name */
  {
    put("build/test_import_cell.odex",
        "par k = 2\npar unused = 5\nv' = -k*v + drive\ninit v = 1\nfun f(u) = k*u\ndrive = f(v)\naux z = unused\n"
        "options { integration: { dt: .5 } }\n");
    put("build/test_import_a.odex", "import \"test_import_b.odex\"\npar fa = 1\n");
    put("build/test_import_b.odex", "import \"test_import_a.odex\"\npar fb = 1\n");
    CHECK_STR(lowered("import \"build/test_import_cell.odex\"\n").c_str(),
              "par k=2|par unused=5|v'=(-k)*v+drive|f(u)=k*u|drive=f(v)|aux z=unused");
    CHECK_STR(lowered("from \"build/test_import_cell.odex\" import f\n").c_str(), "par k=2|f(u)=k*u");
    CHECK_STR(lowered("from \"build/test_import_cell.odex\" import drive\n").c_str(),
              "par k=2|v'=(-k)*v+drive|f(u)=k*u|drive=f(v)");
    CHECK_STR(lowered("from \"build/test_import_cell.odex\" import f\nx' = f(1)\n").c_str(), "par k=2|f(u)=k*u|x'=f(1)");
    CHECK(starts(lowered("par k = 1\nimport \"build/test_import_cell.odex\"\n"), "error 2:1 `k` is already declared at 1:5"));
    CHECK(starts(lowered("import \"build/test_import_cell.odex\"\nimport \"build/test_import_cell.odex\"\n"), "error 2:1 `k` is already declared"));
    /* W318: a set's name is its own, an imported file's sets included */
    put("build/test_import_sets.odex", "par k = 1\nx' = -x\nset \"s\" { k: 2 }\n");
    CHECK(starts(lowered("set \"s\" { k: 3 }\nimport \"build/test_import_sets.odex\"\n"),
                 "error 2:1 the set `s` is defined twice: at m.odex:1:1 and at build/test_import_sets.odex:3:1"));
    remove("build/test_import_sets.odex");
    CHECK(starts(model_error("from \"build/test_import_cell.odex\" import nosuch\n"), "1:1 build/test_import_cell.odex declares no `nosuch`"));
    CHECK(starts(model_error("import \"build/test_import_a.odex\"\n"), "1:8 test_import_a.odex imports itself"));
    CHECK(starts(model_error("import \"build/test_import_cell.odex\" as c1\n"), "1:38 expected `,` between the items of an import"));
    CHECK(!xpp::odex::is_name("c1.v") && !xpp::odex::is_name("c1.") && !xpp::odex::is_name("c1..v"));
    remove("build/test_import_cell.odex");
    remove("build/test_import_a.odex");
    remove("build/test_import_b.odex");
  }
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
  /* par, const and init take a fixed value (docs/odex.md "Fixed values"): numbers, arithmetic, pi, consts,
     the pure functions of those; a parameter, a state or a function is an error at its line */
  CHECK_STR(lowered("par a = -2, b = 1e-3\nx' = b\n").c_str(), "par a=-2|par b=0.001|x'=b");
  CHECK_STR(lowered("par a = 1/30\nx' = a\n").c_str(), "par a=0.03333333333333333|x'=a");
  CHECK_STR(lowered("const k = 2\npar a = 2*k + max(1, 3), b = sin(0) + pi - pi\nx' = a+b\n").c_str(), "const k=2|par a=7|par b=0|x'=a+b");
  CHECK_STR(lowered("const c[j] = j*j for j in 0..2\npar a = c[2] + c1\nx' = a\n").c_str(), "const c0=0|const c1=1|const c2=4|par a=5|x'=a");
  CHECK_STR(lowered("par a = 2\ninit x = 2*pi\nx' = -x\n").c_str(), "par a=2|x'=(-x)");
  CHECK(starts(lowered("par a = 2\ninit x = a\nx' = -x\n"), "error 2:10 the initial value of x can't use the parameter a; use a const or a number"));
  CHECK(starts(lowered("x' = -x\ninit x = x\n"), "error 2:10 the initial value of x can't use the variable x; use a const or a number"));
  CHECK(starts(lowered("fun f(u) = u\nx' = -x\ninit x = f(1)\n"), "error 3:10 the initial value of x can't call the function f; use a const or a number"));
  CHECK(starts(lowered("x' = -x\ninit x = delay(x, 1)\n"), "error 2:16 the initial value of x can't use the variable x"));
  CHECK(starts(lowered("par a = ran(1)\nx' = a\n"), "error 1:9 the value of parameter a can't call ran"));
  CHECK_STR(lowered("x' = -x\ninit x = ran(2)\n").c_str(), "x'=(-x)");
  CHECK(starts(lowered("x' = -x\ninit x = max(1)\n"), "error 2:10 max takes 2 arguments, not 1"));
  CHECK(starts(lowered("x' = -x\ninit x = normal(0, 1)\n"), "error 2:10 the initial value of x can't call normal"));
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
  /* W318: a set's keys are parameters and states, its settings are under options: */
  CHECK_STR(lowered("x' = 1\nset \"s\" { x: -1, options: { integration: { method: stiff, dt: 0.1 } } }\n").c_str(),
            "x'=1|set s {x=-1} integration.method=stiff integration.dt=0.1");
  CHECK_STR(lowered("par dt = 2\nx' = 1\nset \"s\" { dt: 3, options: { integration: { dt: 0.05 } } }\n").c_str(),
            "par dt=2|x'=1|set s {dt=3} integration.dt=0.05");
  CHECK(starts(lowered("x' = 1\nset \"s\" { x: -1, method: stiff }\n"),
               "error 2:18 `method` is a setting: a set holds it under `options: { integration: { method: ... } }`"));
  CHECK(starts(lowered("x' = 1\nset \"s\" { zz: 1 }\n"), "error 2:11 a set's keys are the model's parameters and states, and `zz` is neither"));
  CHECK(starts(lowered("x' = 1\nset \"s\" { options: { nosuch: { dt: 1 } } }\n"), "error 2:22 `nosuch` is not a group of options"));
  CHECK(starts(lowered("x' = 1\nset \"s\" { options: { output: { quiet: 1 } } }\n"), "error 2:22 a set does not hold the group `output`"));
  CHECK(starts(lowered("x' = 1\nset \"s\" { options: { diagram: { autovar: x } } }\n"), "error 2:22 a set does not hold the group `diagram`"));
  CHECK(starts(lowered("x' = 1\nset \"s\" { options: { plot: { colormap: hot } } }\n"), "error 2:30 a set does not hold `colormap`"));
  CHECK(starts(lowered("x' = 1\nset \"s\" { options: { integration: { nosuch: 1 } } }\n"), "error 2:37 `nosuch` is not an option"));
  CHECK(starts(lowered("x' = 1\nset \"s\" { options: { plot: { dt: 1 } } }\n"), "error 2:30 `dt` is in the group integration, not plot"));
  CHECK(starts(lowered("x' = 1\nset \"s\" { x: 1 }\nset \"s\" { x: 2 }\n"), "error 3:1 the set `s` is defined twice: at m.odex:2:1 and at m.odex:3:1"));
  CHECK(starts(lowered("x' = 1\nset \"s\" { x: a }\n"), "error 2:14 a set's value for `x` is a number, not `a`"));
  CHECK_STR(lowered("x' = 1\noptions { integration: { total_time: 20, method: stiff } }\n").c_str(), "x'=1|@ total_time=20,method=stiff");
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
    CHECK_STR(lowered("x' = 1\noptions { equilibria: { neartol: 1e-6 } }\naux flag = near(x, 1)\n").c_str(),
              ("x'=1|@ neartol=1e-6|aux flag=(abs((x)-(1))<=(" + t6 + ")*max(1,max(abs(x),abs(1))))").c_str());
    CHECK_STR(lowered("x' = 1\noptions { equilibria: { neartol: 1e-6 }, integration: { dt: .1 } }\n").c_str(), "x'=1|@ neartol=1e-6,dt=.1");
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
  CHECK(starts(lowered("par a = x\nx' = 1\n"), "error 1:9 the value of parameter a can't use the variable x; use a const or a number"));
  CHECK(starts(lowered("x' = delay(2*x, 1)\n"), "error 1:12 delay's first argument is the name of a variable"));
  CHECK(starts(lowered("aux z = volterra(x)\nx' = 1\n"), "error 1:9 volterra(...) belongs in"));
  CHECK(starts(lowered("x' = x'\n"), "error 1:6 `x'` cannot be read here"));
  CHECK(starts(lowered("x' = x[1]\n"), "error 1:6 the name `x1` is not declared"));
  CHECK(starts(lowered("x' = sum(i', from=0)\n"), "error 1:6 sum needs from= and to="));
  CHECK(starts(lowered("x' = 1\nset \"s\" { a: x+1 }\n"), "error 2:15 expected `}` to close the block, found `+`"));
  CHECK(starts(lowered("x' = 1\naux flag = near(x)\n"), "error 2:12 near takes 2 or 3 arguments, not 1"));
  CHECK(starts(lowered("x' = 1\naux flag = near(x, 1, 1, 1)\n"), "error 2:12 near takes 2 or 3 arguments, not 4"));
  CHECK(starts(lowered("x' = 1\naux flag = near(x, 1, 1e-6)\n"), "error 2:23 near's third argument must be named tol="));
  CHECK(starts(lowered("x' = 1\naux flag = near(x, 1, eps=1e-6)\n"), "error 2:27 near has no argument named `eps`: only tol="));
  CHECK(starts(lowered("x' = 1\naux flag = near(x, 1, tol=-1)\n"), "error 2:27 near's tol= is not a positive number"));
  CHECK(starts(lowered("x' = 1\naux flag = near(x, 1, tol=x)\n"), "error 2:27 near's tol= is not a positive number"));
  CHECK(starts(lowered("x' = 1\noptions { equilibria: { neartol: -1 } }\n"), "error 2:34 neartol: -1 is not a positive number"));
  CHECK(starts(lowered("x' = 1\noptions { equilibria: { neartol: 0 } }\n"), "error 2:34 neartol: 0 is not a positive number"));

  /* arrays (docs/odex.md "Arrays", W80): a trailing range on a
     statement, its index named, both ends included, by a positive step */
  {
    Parsed p = xpp::odex::parse(xpp::client_session().model(), "x[j]' = -x[j] + x[j-1]  for j in 1..n\naux s[j] = x[j]^2  for j in 1..n by 2\n",
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
  /* A native range is checked before copying; a large step counts only its copies. */
  CHECK(starts(lowered("x[j]' = 1 for j in 0..100000\n"), "error 1:11 the range has 100001 elements, more than the maximum 100000"));
  CHECK(lowered("x[j]' = 1 for j in 0..1000000000 by 1000000000\n").find("error") == std::string::npos);
  try {
    xpp::odex::parse(xpp::client_session().model(), std::string(4u * 1024 * 1024, ','), "punctuation.odex");
    CHECK(false);
  } catch (const Error &e) {
    CHECK(e.place.file == "punctuation.odex" && e.place.line == 1);
    CHECK(e.what.find("tokens") != std::string::npos);
  }
  CHECK(starts(lowered("x[j]' = y[j] for j in 1..2\n"), "error 1:9 the name `y1` is not declared (where j = 1)"));
  CHECK(starts(lowered("x[j]' = x[j-1] for j in 0..1\n"), "error 1:11 the index of x `j-1` is -1: an element's index is 0 or more (where j = 0)"));
  CHECK(starts(lowered("par j = 1\nx[j]' = 1 for j in 1..2\n"), "error 2:15 the index `j` is a parameter, declared at 1:5"));
  CHECK(starts(lowered("par n = 3\nx[j]' = 1 for j in 1..n\n"), "error 2:23 the range's last index can't use `n`"));
  CHECK(starts(lowered("x[j]' = 1 for j in 1..2.5\n"), "error 1:23 the range's last index `2.5` is 2.5, not a whole number"));
  CHECK(starts(lowered("x[j]' = 1 for j in 3..1\n"), "error 1:20 the range 3..1 is empty"));
  CHECK(starts(lowered("x[j]' = 1 for j in 1..3 by 0\n"), "error 1:28 the range's step is a whole number above 0, not 0"));
  CHECK(starts(lowered("x[j]' = 1 for j in 1..3 by -1\n"), "error 1:28 the range's step is a whole number above 0, not -1"));
  CHECK(starts(lowered("x' = 1\noptions { integration: { total_time: 10 } } for j in 1..2\n"), "error 2:45 this statement takes no range"));
  CHECK(starts(lowered("x[j]' = 1 for j in 1..2\nx[2]' = 1\n"), "error 2:1 `x2` is already a member of the array x[1..2], declared at 1:1"));

  /* const: fixed at load, from numbers and the consts before it */
  CHECK_STR(lowered("const n = 2, m = n*3+1\nx' = m\n").c_str(), "const n=2|const m=7|x'=m");
  CHECK(starts(lowered("par a = 1\nconst n = a\nx' = n\n"), "error 2:11 the value of const n can't use `a`"));
  CHECK(starts(lowered("const n = m\nconst m = 1\nx' = n\n"), "error 1:11 the value of const n can't use `m`"));
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
  CHECK_STR(quantities("par a=2\n!tr=-a\nw=a*2\nv=x\nx'=tr+w+v\n", "ode").c_str(), "tr:derived|w:derived|v:fixed");
  CHECK_STR(quantities("par a = 2\ntr = -a\nw = a*2\nv = x\nx' = tr+w+v\n").c_str(), "tr:derived|w:derived|v:fixed");
  CHECK_STR(quantities("par a=2\nw=a*2\nx'=w\n", "ode").c_str(), "w:derived");
  CHECK(load_text("par a = 2\nd = a*3\nx' = d\n") == 1 && constant("d") == 6);
  {
    /* worked out again when a parameter changes, as .ode's ! is */
    xpp::set_val(xpp::client_session(), "A", 5);
    xpp::evaluate_derived(xpp::client_session());
    CHECK(constant("d") == 15);
  }

  /* W160: strip once for every physical .ode line, including the first. */
  {
    CHECK(load_text("p a=1 # some words\\ ignored continuation\n"
                    "p b=2# another comment\ninit x=0 # words\n"
                    "aux y=x # words\nnumber n=3 # words\n"
                    "x'=-x # words\ndone\n", "ode") == 1);
    CHECK(constant("a") == 1 && constant("b") == 2 && constant("n") == 3);
    CHECK(xpp::client_session().model().nupar == 2);
    CHECK(xpp::client_session().model().files.front().bytes.find("# some words") != std::string::npos);
    const std::string converted=*xpp::odex::convert_text("build/test_odex_model.ode");
    CHECK(converted.find("# some words\\ ignored continuation") != std::string::npos);
    CHECK(converted.find("# another comment") != std::string::npos);
    CHECK(model_error(converted.c_str()).empty());
    CHECK(load_text("  # whole-line comment\nx'=-x # ignored\n", "ode") == 1);
    CHECK(load_text("x'=int{exp(-t)#x} # trailing\n", "ode") == 1);
    CHECK(load_text("x'=int[.5]{exp(-t)#x} # trailing\n", "ode") == 1);
    CHECK(xpp::ode_comment_start("#include file.inc # note") == 18);
    CHECK(xpp::ode_comment_start("#done # note") == 6);
    CHECK(xpp::ode_comment_start("k conv 0 exp(-t)#x # note") == 19);
    CHECK(xpp::ode_comment_start("\" display # text") == std::string::npos);
    {
      xpp::Writer include("build/test_ode_comments.inc");
      CHECK(include.write("p included=7 # included words\n#done # end include\np ignored=9\n"));
      CHECK(include.commit());
    }
    CHECK(load_text("#include test_ode_comments.inc # include words\n"
                    "init x=4 # initial words\nx'=-x # formula words\n", "ode") == 1);
    CHECK(constant("included") == 7);
    CHECK(xpp::client_session().model().default_ic[0] == 4);
    CHECK(xpp::client_session().model().nupar == 1);
  }

  /* --convert: what the .ode's reader understood, its quirks explicit */
  {
    const xpp::Result<std::string> converted = xpp::odex::convert_text("tools/models/odex_quirks.ode");
    CHECK(converted.has_value());
    const std::string text = converted ? *converted : "";
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
    CHECK(has("z' = 1/ode_divisor(x)+x/2+x/(1+exp(-x))+x/ode_divisor(a)+and_"));
    CHECK(has("aux w = x and y or z"));
    CHECK(has("# y=2*3 in the .ode: XPP reads the number at its front, 2"));
    CHECK(has("init x = 1, y = 2"));
    CHECK(has("const nn = 5"));
    CHECK(has("dd = a*2"));
    CHECK(has("v[j]' = -v[j]+j for j in 1..3"));
    CHECK(has("aux sq[j] = v[j]^2 for j in 1..3"));
    CHECK(has("event 1 x-1, y = y/ode_divisor(x), z = 0"));
    CHECK(has("  integration: { total_time: 6, dt: .1 },"));
    CHECK(has("# anything here is kept as a comment"));
    CHECK(text.find("the .ode's lines 8") != std::string::npos); /* gk spelled Gk */
    /* what the converted text reads back as */
    std::string back;
    try {
      xpp::odex::ready(xpp::client_session().model(), xpp::odex::parse(xpp::client_session().model(), text, "odex_quirks.odex"));
    } catch (const Error &e) {
      back = e.text();
    }
    CHECK_STR(back.c_str(), "");
  }

  /* W165: uncertain divisors use a function argument, evaluated once;
     positive integer sum indices and positive forms need no guard. */
  {
    CHECK(load_text("par a=2\nnumber c=3\nx'=0\n"
                    "aux g=1/a\naux z=1/0\naux k=1/c\naux q=1/(x*x+1)\n"
                    "aux e=1/exp(x)\naux ep=1/exp(x*x)\naux ss=sum(1,4)of(1/(i'*i'))\n"
                    "aux uu=sum(0,4)of(1/(i'*i'))\ndone\n", "ode") == 1);
    const auto result = xpp::odex::convert_text("build/test_odex_model.ode");
    CHECK(result.has_value());
    const std::string text = result ? *result : "";
    CHECK(text.find("aux g = 1/ode_divisor(a)") != std::string::npos);
    CHECK(text.find("aux z = 1/ode_divisor(0)") != std::string::npos);
    CHECK(text.find("aux k = 1/c") != std::string::npos);
    CHECK(text.find("aux q = 1/(x*x+1)") != std::string::npos);
    CHECK(text.find("aux e = 1/ode_divisor(exp(x))") != std::string::npos);
    CHECK(text.find("aux ep = 1/exp(x*x)") != std::string::npos);
    CHECK(text.find("aux ss = sum(1/(i'*i'), from=1, to=4)") != std::string::npos);
    CHECK(text.find("aux uu = sum(1/ode_divisor(i'*i'), from=0, to=4)") != std::string::npos);
    CHECK(text.find("fun ode_divisor(value) = if value then value else 2.23e-15") != std::string::npos);
  }

  /* W175: the .ode and conversion share checked solver names and keys. */
  {
    CHECK(load_text("x'=-x\ny'=x\n@ meth=modeuler, total=1\ndone\n", "ode") == 1);
    const auto result = xpp::odex::convert_text("build/test_odex_model.ode");
    CHECK(result.has_value());
    const std::string text = result ? *result : "";
    CHECK(text.find("integration: { method: modified_euler, total_time: 1 }") != std::string::npos);
    CHECK(text.find("first letter") == std::string::npos);
    CHECK(load_text("x'=-x\ny'=x\n@ meth=symplectic\n@ total=2\n@ meth=euler\ndone\n", "ode") == 1);
    const auto later = xpp::odex::convert_text("build/test_odex_model.ode");
    CHECK(later && later->find("integration: { method: euler, total_time: 2 }") != std::string::npos);
  }

  /* W318: --convert writes a set's model names flat (XPPAUT's precedence: a parameter named dt
     takes dt=), the settings nested under options: by group; the result loads. */
  {
    CHECK(load_text("par dt=2\nx'=-x\ninit x=1\nset trial {dt=0.1,total=5,xlo=-2,x=3,xlo=-3}\ndone\n", "ode") == 1);
    const auto result = xpp::odex::convert_text("build/test_odex_model.ode");
    CHECK(result.has_value());
    const std::string text = result ? *result : "";
    CHECK(text.find("set \"trial\" { dt: 0.1, x: 3, options: { integration: { total_time: 5 }, plot: { xlo: -3 } } }") != std::string::npos);
    if (result) CHECK(model_error(text.c_str()).empty());
  }

  /* W167: keep ignored, spaced @ words visible in the conversion note. */
  {
    CHECK(load_text("x'=0\n@ total=7 dt=.5\n@ parmin = -.2 parmax = .5\ndone\n", "ode") == 1);
    const auto result = xpp::odex::convert_text("build/test_odex_model.ode");
    CHECK(result.has_value());
    const std::string text = result ? *result : "";
    CHECK(text.find("integration: { total_time: 7, dt: .5 }") != std::string::npos);
    CHECK(text.find("# @ parmin = -.2 parmax = .5 in the .ode: XPP ignores an option with spaces around its =, so parmin, parmax keep their values") != std::string::npos);
    CHECK(text.find("XPP ignores an option") == text.rfind("XPP ignores an option"));
    CHECK(load_text("x'=0\n@ total=7 dt=.5\ndone\n", "ode") == 1);
    const auto clean = xpp::odex::convert_text("build/test_odex_model.ode");
    CHECK(clean.has_value());
    CHECK(clean && clean->find("XPP ignores an option") == std::string::npos);
  }

  {
    CHECK(load_text("x'=1/ran(1)\ndone\n", "ode") == 1);
    xpp::Session &session = xpp::client_session();
    session.random.seed(1);
    const double divisor = session.random.uniform();
    const double next = session.random.uniform();
    session.random.seed(1);
    CHECK(xpp::evaluate(session, session.model().programs[0].rpn.data()) == 1/divisor);
    CHECK(session.random.uniform() == next); /* exactly one draw */
    CHECK(load_text("par a=2\nx'=1/a\ndone\n", "ode") == 1);
    xpp::set_val(xpp::client_session(), "A", 0);
    CHECK(xpp::evaluate(xpp::client_session(), xpp::client_session().model().programs[0].rpn.data()) == 1/xpp::expr::ZERO_DIVISOR);
    CHECK(load_text("par ode_divisor=2\nx'=1/x\ndone\n", "ode") == 1);
    const auto collision = xpp::odex::convert_text("build/test_odex_model.ode");
    CHECK(collision && collision->find("fun ode_divisor_(value)") != std::string::npos);
    CHECK(load_text("x'=0\naux nested=sum(1,2)of(sum(0,0)of(i')+1/(i'*i'))\ndone\n", "ode") == 1);
    const auto nested = xpp::odex::convert_text("build/test_odex_model.ode");
    CHECK(nested && nested->find("+1/ode_divisor(i'*i')") != std::string::npos);
  }

  {
    /* Function tables evaluate at load, including forward guard calls. */
    CHECK(load_text("table tab % 3 -1 1 exp(t/a)\npar a=2\nx'=0\n@ dt=.1 total=1\ndone\n", "ode") == 1);
    const auto table = xpp::odex::convert_text("build/test_odex_model.ode");
    CHECK(table && table->find("table tab exp(t/ode_divisor(a))") != std::string::npos);
    CHECK(table && table->find("integration: { dt: .1, total_time: 1 }") != std::string::npos);
  }

  /* --convert refuses an .ode's !d = expr that reads a variable: .odex's
     d = expr would be worked out at every evaluation, not only when a
     parameter changes */
  {
    CHECK(load_text("par a=1\n!d=x*a\nx'=d\n", "ode") == 0);
    const xpp::Result<std::string> converted = xpp::odex::convert_text("build/test_odex_model.ode");
    CHECK(!converted && starts(converted.error().what, "!d = x*a reads t, a variable, a random function or a derived quantity after it"));
  }

  /* near(a, b[, tol=]) numerically: tools/models/near_test.odex's aux
     columns, integrated as --silent does (Initialconds/Go) and read from
     the stored columns (not the translation's text) */
  {
    char arg0[] = "test_odex", model[] = "tools/models/near_test.odex";
    char *argv[] = {arg0, model, nullptr};
    CHECK(load_args(2, argv, 1).has_value());
    xpp::batch_start(xpp::client_session());
    run_the_commands(xpp::client_session(), xpp::M_IG);
    const xpp::DataStore &d = xpp::client_session().data_store;
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

  /* W261: Backward Euler and Rosenbrock keep a 2-state band with the
     effective width (a requested width of 99 is still the full matrix). */
  {
    auto integrate = [](int method, int width) {
      CHECK(load_text("x'=-x+y\ny'=-y\ninit x=1,y=2\n@ total=.1 dt=.01\ndone\n","ode"));
      xpp::Session &s=xpp::client_session();
      s.numerics.method=static_cast<xpp::method::Id>(method);
      s.numerics.cv_bandflag=width>0;
      s.numerics.cv_bandlower=width;
      s.numerics.cv_bandupper=width;
      xpp::batch_start(s);
      run_the_commands(s,xpp::M_IG);
      const xpp::DataStore &d=s.data_store;
      CHECK(d.rows>0);
      if(d.rows==0)return std::array<double,2>{};
      CHECK(std::fabs(static_cast<double>(d.col[0][d.rows-1])-.1)<1e-6);
      return std::array<double,2>{static_cast<double>(d.col[1][d.rows-1]),
                                  static_cast<double>(d.col[2][d.rows-1])};
    };
    for(int method:{xpp::method::RB23,xpp::method::BACKEUL}){
      const auto dense=integrate(method,0);
      const auto band=integrate(method,1);
      const auto oversized=integrate(method,99);
      for(int i=0;i<2;i++){
        CHECK(std::fabs(band[i]-dense[i])<1e-8);
        CHECK(std::fabs(oversized[i]-dense[i])<1e-8);
      }
    }
  }

  /* Diagnostics keep the defining equation ahead of an aux name and
     preserve CRLF source lines, including names in binding lists. */
  {
    auto model=std::make_unique<xpp::Model>();
    xpp::Model &m=*model;
    m.this_file="indexed.odex";
    const std::string text="par gain = 1\r\nv' = gain\r\naux v = v\n";
    m.files.push_back({m.this_file,text});
    Parsed p=xpp::odex::parse(m,text,m.this_file);
    m.statements=std::move(p.statements);
    m.statement_files=std::move(p.files);
    xpp::index_model_places(m);
    const xpp::Place equation=xpp::model_place(m,"V");
    CHECK(equation.file==m.this_file && equation.line==2);
    CHECK(equation.source=="v' = gain");
    const xpp::Place parameter=xpp::model_place(m,"GAIN");
    CHECK(parameter.line==1 && parameter.source=="par gain = 1");
    CHECK(xpp::model_source_line(m,m.this_file,3)=="aux v = v");
    CHECK(xpp::model_source_line(m,m.this_file,4).empty());
  }

  /* W291: model names share one index while function arguments remain
     parser-local; a fixed quantity can own both a parser slot and an aux
     column. Reload replaces the whole index, a refused load preserves it. */
  {
    CHECK(load_text("par gain=2\nfun f(arg)=arg+gain\nv'=f(v)\nq=v*gain\naux q=q\ninit v=1\n", "odex"));
    const xpp::Session &s=xpp::client_session();
    const xpp::Model &m=s.model();
    CHECK(xpp::find_user_name(m,PARAMBOX," G a i n ")==0);
    CHECK(xpp::find_user_name(m,ICBOX,"v")==0);
    CHECK(xpp::find_user_name(m,ICBOX,"q")==1);
    CHECK(xpp::get_var_index(s,"q")==2);
    CHECK(xpp::get_param_index(s,"gain")==m.upar_con[0]);
    CHECK(xpp::get_var_index(s,"arg")==-1);
    CHECK(m.name("arg")==nullptr);
    CHECK(m.name("f") && m.name("f")->kind==xpp::Model::NameKind::Function);
    CHECK(xpp::model_place(m,"q").line==4);
    const size_t count=m.names.size();
    double value=0;
    CHECK(xpp::get_val(s,"gain",&value));
    CHECK(value==2 && m.names.size()==count);
    CHECK(!load_text("par gain=2\nv'=missing\n", "odex"));
    CHECK(xpp::client_session().model().name("q")!=nullptr);
    CHECK(load_text("z'=0\n", "odex"));
    CHECK(xpp::client_session().model().name("q")==nullptr);
    CHECK(xpp::find_user_name(xpp::client_session().model(),ICBOX,"z")==0);
  }

  /* W313: only states without init are supplied by conversion. */
  {
    CHECK(load_text("x'=0\ny'=0\ninit x=2\n", "odex"));
    const auto missing = xpp::missing_initial_values(xpp::client_session().model());
    CHECK(missing.size() == 1 && missing[0]->name == "y" && missing[0]->pos.line == 2);
    CHECK(xpp::client_session().last_ic[1] == 0);
    put("build/test_odex_model.ode", "x'=0\ny'=0\ninit x=2\ndone\n");
    const auto converted = xpp::odex::convert_text("build/test_odex_model.ode");
    CHECK(converted && converted->find("init y = 0\n") != std::string::npos);
    CHECK(converted && load_text(converted->c_str(), "odex"));
    CHECK(xpp::missing_initial_values(xpp::client_session().model()).empty());
    CHECK(load_text("x[j]'=0 for j in 1..4\ninit x[2]=1\n", "odex"));
    CHECK(xpp::missing_initial_values(xpp::client_session().model()).size() == 3);
    put("build/test_odex_model.ode", "x[1..3]'=0\nx[1..3](0)=2\ndone\n");
    const auto array = xpp::odex::convert_text("build/test_odex_model.ode");
    CHECK(array && load_text(array->c_str(), "odex"));
    CHECK(xpp::missing_initial_values(xpp::client_session().model()).empty());
  }
  /* W312: the removed option is diagnosed and dropped from conversion. */
  {
    CHECK(starts(model_error("options { output: { runnow: 1 } }\nx'=0\n"), "1:21 `runnow` is not an option"));
    put("build/test_odex_model.ode", "x'=0\n@ runnow=1\ndone\n");
    std::vector<xpp::odex::Diagnostic> notes;
    const auto converted = xpp::odex::convert_text("build/test_odex_model.ode", {}, &notes);
    CHECK(converted && converted->find("batch:") == std::string::npos);
    CHECK(converted && converted->find("# @ runnow=1") != std::string::npos);
    CHECK(std::ranges::any_of(notes, [](const auto &n) { return n.id == "runnow-removed" && n.place.line == 2; }));
  }
  /* W324: parameter/auxiliary-only files are importable, not runnable. */
  {
    for (const char *text : {"# empty\n", "par gain=2\n", "par gain=2\naux z=gain\n"}) {
      put("build/test_odex_model.odex", text);
      std::string arg0="test_odex", path="build/test_odex_model.odex";
      char *argv[]={arg0.data(), path.data(), nullptr};
      const auto loaded=load_args(2, argv, 1);
      CHECK(!loaded && loaded.error().place.file == path);
      CHECK(!loaded && loaded.error().what == "it has no equations: it is a file to import into a model, not a model to run");
    }
    put("build/test_import_values.odex", "par gain=2\n");
    CHECK(load_text("import \"test_import_values.odex\"\nx'=gain\ninit x=0\n", "odex"));
    xpp::files::remove("build/test_import_values.odex");
  }

  /* W317: native groups are strict, and preset lists exceed foreign S1-S3 slots. */
  {
    CHECK(starts(model_error("options { numerics: { dt: .1 } }\nx'=0\n"), "1:11 `numerics` is not a group"));
    CHECK(starts(model_error("options { interface: { s1: x } }\nx'=0\n"), "1:24 `s1` is not an option"));
    const char *presets = "par a=1\nx'=0\ninit x=0\noptions { interface: { sliders: ["
      "{ name: a, low: 0, high: 2 }, { name: x, low: -1, high: 1 },"
      "{ name: a, low: 0, high: 3 }, { name: x, low: -2, high: 2 },"
      "{ name: a, low: 0, high: 4, step: .1 } ] } }\n";
    CHECK(load_text(presets, "odex"));
    CHECK(xpp::client_session().sliders.size() == 5);
    CHECK(xpp::client_session().sliders.back().step == .1);
    CHECK(load_text("x'=0\ninit x=0\noptions { interface: { sliders: [] } }\n", "odex"));
    CHECK(xpp::client_session().sliders.empty());
    CHECK(!model_error("x'=0\noptions { interface: { sliders: [ { name: x, low: 2, high: 1 } ] } }\n").empty());
    CHECK(!model_error("x'=0\noptions { interface: { sliders: [ { name: x, low: 0, high: 1, extra: 1 } ] } }\n").empty());
  }

  /* W490: a model's tables, functions, networks, sets and events have no limit (they were 50, 50, 50, 500, 2000) */
  {
    std::string big = "par a = 1\nx[j]' = -x[j] for j in 0..2\ninit x[j] = 1 for j in 0..2\ntable w t, n=3, lo=0, hi=2\n";
    const int many = 600;
    for (int i = 0; i < 60; i++) {
      big += xpp::format("table t{} t*{}, n=3, lo=0, hi=2\n", i, i + 1);
      big += xpp::format("fun f{}(u) = u+{}\n", i, i);
      big += xpp::format("network n{} = conv(even, 3, 1, w, x0)\n", i);
    }
    for (int i = 0; i < many; i++) big += xpp::format("set \"s{}\" {{ a: {} }}\n", i, i);
    for (int i = 0; i < 2100; i++) big += xpp::format("event 1 x0 > {}, a = {}\n", i + 10, i);
    big += "aux last_table = t59(1)+f59(1)+n59(0)\n";
    CHECK(load_text(big.c_str()) == 1);
    xpp::Session &s = xpp::client_session();
    CHECK(s.ntable == 61 && s.model().nfun == 60 && s.model().nnetwork == 60);
    CHECK(s.model().intern_sets.size() == static_cast<std::size_t>(many) && s.model().flag_count() == 2100);
  }
  /* a network's tables hold the values it reads: a convolution of one tap each side reads three weights */
  {
    put("build/test_odex_model.odex", "table w t, n=2, lo=0, hi=1\nx[j]' = -x[j] for j in 0..2\ninit x[j] = 1 for j in 0..2\nnetwork k = conv(even, 3, 1, w, x0)\n");
    std::string arg0 = "test_odex", path = "build/test_odex_model.odex";
    char *argv[] = {arg0.data(), path.data(), nullptr};
    const auto loaded = load_args(2, argv, 1);
    CHECK(!loaded && loaded.error().place.line == 4 && loaded.error().what == "In k, W has 2 values; 3 are needed");
  }

  /* W497: a table re-read (the Numerics' load table) must keep every network's values in range: a
     shorter file is refused at the network's line, and the table keeps its values */
  {
    put("build/w_long.tab", "3\n0\n2\n1\n2\n3\n");
    put("build/w_short.tab", "2\n0\n1\n1\n2\n");
    put("build/test_odex_model.odex", "table w \"build/w_long.tab\"\nx[j]' = -x[j] for j in 0..2\ninit x[j] = 1 for j in 0..2\nnetwork k = conv(even, 3, 1, w, x0)\n");
    std::string arg0 = "test_odex", path = "build/test_odex_model.odex";
    char *argv[] = {arg0.data(), path.data(), nullptr};
    CHECK(load_args(2, argv, 1).has_value());
    xpp::Session &s = xpp::client_session();
    const int index = s.model().networks[0].weight_table;
    const auto shorter = xpp::load_table(s, "build/w_short.tab", index, 0);
    CHECK(!shorter && shorter.error().place.line == 4 && shorter.error().what == "In network k, table W has 2 values; 3 are needed");
    CHECK(xpp::get_lookup_len(s, index) == 3 && s.tables[index].y[2] == 3.0);
  }

  /* W505: every table number is read through the typed reader, all or nothing: a word, a nan or
     an infinite value or endpoint is refused at its line, and the table keeps its values */
  {
    put("build/w_word.tab", "3\n0\n2\n1\nnonsense\n3\n");
    put("build/w_nan_value.tab", "3\n0\n2\n1\n2\nnan\n");
    put("build/w_inf_end.tab", "3\n0\ninf\n1\n2\n3\n");
    put("build/w_range.tab", "3\n-1e308\n1e308\n1\n2\n3\n");
    std::string arg0 = "test_odex", path = "build/test_odex_model.odex";
    char *argv[] = {arg0.data(), path.data(), nullptr};
    CHECK(xpp::load_model(2, argv, 1).has_value());
    xpp::Session &s = xpp::client_session();
    const int index = s.model().networks[0].weight_table;
    const auto word = xpp::load_table(s, "build/w_word.tab", index, 0);
    CHECK(!word && word.error().place.line == 5 && word.error().place.source == "nonsense");
    const auto nan_value = xpp::load_table(s, "build/w_nan_value.tab", index, 0);
    CHECK(!nan_value && nan_value.error().place.line == 6 && nan_value.error().place.source == "nan");
    const auto inf_end = xpp::load_table(s, "build/w_inf_end.tab", index, 0);
    CHECK(!inf_end && inf_end.error().place.line == 3 && inf_end.error().place.source == "inf");
    const auto wide = xpp::load_table(s, "build/w_range.tab", index, 0);
    CHECK(!wide && wide.error().place.line == 3);
    CHECK(xpp::get_lookup_len(s, index) == 3 && s.tables[index].y[2] == 3.0 && s.tables[index].xlo == 0.0);
  }

  /* W497: a user function takes any number of arguments: ARG1..ARG20 are built in, and the rest are
     added as a function needs them */
  {
    std::string params, call;
    for (int i = 1; i <= 25; i++) {
      params += (i > 1 ? "," : "") + xpp::format("a{}", i);
      call += (i > 1 ? "," : "") + std::to_string(i);
    }
    CHECK(load_text(("fun f(" + params + ") = a1 + a25\nx' = -x\ninit x = 0\naux y = f(" + call + ")\n").c_str()) == 1);
    xpp::Session &s = xpp::client_session();
    std::vector<int> command;
    int length = 0;
    CHECK(xpp::add_expr(s, "f(" + call + ")", command, &length) == 0);
    CHECK(xpp::evaluate(s, command.data()) == 26.0);
    CHECK(s.parser.arg_count() == 25);
  }

  TEST_REPORT("odex: grammar");
}
