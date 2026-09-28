#ifndef XPP_ODEX_H
#define XPP_ODEX_H
/* .odex, the model format without .ode's quirks (docs/odex.md; W74). C++
   only.

   - odex_parse.cpp: the tokenizer and the grammar, text to the statements
     below (parse), every problem an Error at a line and column;
   - odex_print.cpp: an expression back to .odex text (print), with the
     parentheses .odex's precedence needs and no others;
   - odex_load.cpp: a parsed model into the xpp::Model the .ode parser
     builds (the same Model: one route after the parse);
   - odex_convert.cpp: xppautX --convert, a loaded .ode's Model written as
     .odex.

   The expression tree is .odex's own: an operator is the word or symbol
   .odex spells it with ("and", "mod", "!=", ...). */
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xpp::odex {

/* a place in a model: which of Parsed::files, the line and the column
   (both from 1) */
struct Pos {
  int file = 0;
  int line = 0;
  int col = 0;
};

/* a problem found in a model: where, and what */
struct Error {
  std::string file;
  Pos pos;
  std::string message;
  /* "file:line:col: message" */
  std::string text() const;
};

/* an expression */
struct Expr {
  enum class Kind {
    Number, /* value, text as written */
    Name,   /* text; primed for a name written x' */
    Neg,    /* -args[0] */
    Not,    /* not args[0] */
    Binary, /* args[0] text args[1]: or and < > <= >= == != + - * / mod ^ */
    If,     /* if args[0] then args[1] else args[2] */
    Call,   /* text(args...), arg_names[i] "" or the name of a named one */
    Index   /* args[0][args[1]] */
  };
  Kind kind = Kind::Number;
  Pos pos;
  double value = 0;
  std::string text;
  bool primed = false;
  std::vector<Expr> args;
  std::vector<std::string> arg_names;
};

/* a statement of a block function's body */
struct BlockStmt {
  enum class Kind { Let, Return, If };
  Kind kind = Kind::Return;
  Pos pos;
  /* let: the local's name and value; return: value */
  std::string name;
  Expr value;
  /* if: its conditions, and a block for each, then the else block when
     there is one (blocks.size() is conds.size() or one more) */
  std::vector<Expr> conds;
  std::vector<std::vector<BlockStmt>> blocks;
};

/* name = value, one of a list */
struct Binding {
  std::string name;
  Pos pos;
  Expr value;
};

/* an @ option: its name and its value's text */
struct Option {
  std::string name, value;
  Pos pos, value_pos;
};

/* a statement of the model */
struct Statement {
  enum class Kind {
    Ode,      /* name' = expr */
    Volterra, /* name(t) = expr */
    Fixed,    /* name = expr */
    Par,      /* par bindings */
    Init,     /* init bindings */
    History,  /* history bindings: a variable's values before the start */
    Aux,      /* aux bindings */
    Derived,  /* !name = expr, in bindings */
    Fun,      /* fun name(names) = expr, or with a body */
    Options,  /* @ options */
    Set,      /* set name = bindings */
    Table,    /* table name "file" (text), or table name expr, n=, lo=, hi= */
    Markov,   /* markov name count {cells}: count*count transitions */
    Wiener,   /* wiener names */
    Event,    /* event sign expr, bindings */
    Boundary, /* boundary expr */
    Network,  /* network name = text(call_args) */
    Solv,     /* solv name = expr */
    Dae,      /* 0 = expr */
    Only,     /* only names */
    Comment   /* "text": a comment the model shows (text) */
  };
  Kind kind = Kind::Fixed;
  Pos pos;
  std::string name;
  Pos name_pos;
  std::string text;
  Expr expr;
  std::vector<Binding> bindings;
  std::vector<std::string> names;
  std::vector<Pos> name_positions;
  /* a block function's body (has_body) */
  bool has_body = false;
  std::vector<BlockStmt> body;
  std::vector<Option> options;
  /* an event's sign, a Markov variable's number of states, a formula
     table's number of points */
  int count = 0;
  /* a formula table's lo and hi */
  double lo = 0, hi = 0;
  /* a Markov variable's transitions, row by row */
  std::vector<Expr> cells;
  /* a network's arguments, each as written */
  std::vector<std::string> call_args;
};

/* a parsed model: its files (the model's, then those it includes) and
   its statements in order, an include's where the include was */
struct Parsed {
  std::vector<std::string> files;
  std::vector<Statement> statements;
};

/* text, the contents of file, parsed; an include is read relative to
   file's folder. Throws Error. */
Parsed parse(std::string_view text, const std::string &file);
/* parse, the text read from path first */
Parsed parse_file(const std::string &path);

/* the words .odex reserves (docs/odex.md "Reserved words"): a name may
   never be one */
bool is_reserved(std::string_view word);
/* name is an .odex name: a letter, then letters, digits and '_' */
bool is_name(std::string_view name);

/* a parsed model checked and written as the .ode reader's lines
   (odex_load.cpp; each formula with the parentheses .ode's precedence
   needs), a parameter's value evaluated; the current Model is the one
   being built (its divisions become IEEE's). Each init's variable and
   formula, to evaluate once the Model is built. Throws Error. */
struct Lowered {
  struct Initial {
    std::string name, formula, file;
    Pos pos;
  };
  std::vector<std::string> lines;
  std::vector<Initial> initials;
};
Lowered lower(const Parsed &p);
/* an .odex model's initial values (Model::initial_values), each
   evaluated with every parameter set: set_all_vals (load_eqn.cpp) calls
   this once the model is set up, where an .ode's array initial values
   are evaluated too */
void set_initials();

/* path names an .odex model (its extension, any case) */
bool is_odex(std::string_view path);
/* the .odex model at path into the current Model and Session, as get_eqn
   reads an .ode's (load_eqn.cpp): 1; a problem is logged (file, line,
   column) and the load fails (xpp_model_failed) */
int load(const std::string &path);

/* --convert's question about a name .odex reserves: the question and a
   suggested answer; the answer ("" takes the suggestion), or nullopt
   when nobody can answer (no terminal) */
using Ask = std::function<std::optional<std::string>(const std::string &question, const std::string &suggestion)>;
/* the loaded .ode model (the current Model) as .odex text, from what the
   .ode parser understood (odex_convert.cpp); auto_answer takes every
   suggested name. Throws Error. */
std::string convert_model(bool auto_answer, const Ask &ask);
/* xppautX --convert [--auto] model.ode: the model loaded, written as
   model.odex (odex_name), which is loaded in turn and must compile to
   the same programs (exact by construction, checked); 0 when written, 1
   (said why) when not. ask answers the questions (no terminal: none). */
int convert_file(const std::string &ode, bool auto_answer, const Ask &ask);
/* model.ode's .odex: its extension made .odex */
std::string odex_name(const std::string &ode);

/* e as .odex text, with the parentheses .odex's precedence needs, and
   around a sign anywhere but at the start or after '(' or ',' (where
   .ode needed them, docs/odex.md "--convert's bracket policy") */
std::string print(const Expr &e);
/* v as an .odex number: the shortest text that reads back as v */
std::string print_number(double v);

}

#endif
