#ifndef XPP_ODEX_H
#define XPP_ODEX_H
/* The statement list both model formats are read into, and .odex, the
   model format without .ode's quirks (docs/odex.md; W74, W79). C++ only.

   Two readers make the statement list and one builder makes the Model
   from it (docs/odex.md question 10):
   - ode_read.cpp: the .ode reader, an .ode file's lines to statements,
     every quirk of .ode's kept (its formulas .ode text, Expr::Kind::Text);
   - odex_parse.cpp: the tokenizer and the grammar, .odex text to
     statements (parse), every problem an Error at a line and column;
     odex_load.cpp, the .odex reader, checks them (every name declared
     once, read where it may be, called with its arguments) and readies
     them for the builder (ready);
   - form_ode.cpp: the builder, statements to the xpp::Model (build_model,
     form_ode.h), whichever reader made them;
   - odex_print.cpp: an expression as .odex text (print), with the
     parentheses .odex's precedence needs and no others, and as the
     expression engine's text (engine_text), which the builder compiles;
   - odex_convert.cpp: xppautX --convert, a loaded .ode's Model written as
     .odex.

   The expression tree is .odex's own: an operator is the word or symbol
   .odex spells it with ("and", "mod", "!=", ...). */
#include "xpp_error.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace xpp {
struct Session; /* session.h */
struct Model;   /* model.h */
}

namespace xpp::odex {

/* a place in a model: which of Parsed::files, the line and the column
   (both from 1) */
struct Pos {
  int file = 0;
  int line = 0;
  int col = 0;
};

/* the problem cause at pos of file, as the load's own error
   (xpp::Error, xpp_error.h): what, and where */
inline Error error_at(std::string file, Pos pos, std::string cause)
{
  return Error{"model", std::move(cause), Place{std::move(file), pos.line, pos.col}};
}

/* an expression */
struct Expr {
  enum class Kind {
    Text,   /* an .ode formula (text) as the .ode reader keeps it: the
               expression engine compiles it as written, .ode's precedence
               and all */
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

/* name = value, one of a list; index the name's own (x[e] = value, an
   array's element) */
struct Binding {
  std::string name;
  Pos pos;
  Expr value;
  std::optional<Expr> index;
};

/* a statement's trailing range (.odex's arrays, W80): for index in
   lo..hi by step, each end included, step 1 when by is not written;
   index "" when the statement has none */
struct Range {
  std::string index;
  Pos pos, index_pos;
  Expr lo, hi, step;
};

/* which array statement a statement is a copy of (W80): group 0 when it
   is none; the index and its value in this copy, the range's ends and
   step. Both readers make the copies (an .ode's arrays are text its reader
   rewrites, an .odex's the .odex reader expands) and mark them so, and
   --convert writes the copies back as one statement with its range;
   interleaved when the copies are .ode's %[..] block of several lines,
   which take turns (copy k of each line, then copy k+1) */
struct ArrayCopy {
  int group = 0;
  std::string index;
  int value = 0, lo = 0, hi = 0, step = 1;
  bool interleaved = false;
};

/* an @ option: its name and its value's text */
struct Option {
  std::string name, value;
  Pos pos, value_pos;
};

/* a statement of the model; its formulas .odex's trees or, from the .ode
   reader, .ode text (Expr::Kind::Text) */
struct Statement {
  enum class Kind {
    Ode,      /* name' = expr */
    Map,      /* name(t+1) = expr (.ode) */
    Volterra, /* name(t) = expr */
    Fixed,    /* name = expr */
    Par,      /* par bindings: numbers */
    Const,    /* const bindings: numbers fixed at load (.ode's number;
                 .odex's const, whose value the .odex reader works out) */
    Init,     /* init bindings: formulas, evaluated in order with every
                 parameter set once the model is set up (.odex's) */
    InitNumbers, /* init bindings, each a number set at once (.ode's init;
                 text, when set, the x(0)=text it was read from) */
    History,  /* history bindings: a variable's values before the start
                 (.ode's x(0)=formula is an init and a history) */
    Aux,      /* aux bindings */
    Derived,  /* .ode's !name = expr, in bindings; the builder makes a
                 Fixed whose formula reads only parameters, consts and pure
                 functions one too (parameters_only) */
    Fun,      /* fun name(names) = expr, or with a body */
    Options,  /* @ options; text the @ line the Model keeps */
    OptionFile, /* .ode's options file: text, its name */
    Set,      /* set name = bindings; text its actions as the Model keeps
                 them, a=1,b=2 */
    Table,    /* table name "file" (text), or table name expr, n=, lo=,
                 hi= (table_kind says which) */
    Markov,   /* markov name count {cells}: count*count transitions */
    Wiener,   /* wiener names */
    Event,    /* event sign expr, bindings */
    Boundary, /* boundary expr */
    Network,  /* network name = text(call_args); read, text is the whole
                 definition, kind(arguments) */
    Vector,   /* .ode's vector name = text */
    Solv,     /* solv name = expr */
    Dae,      /* 0 = expr */
    Only,     /* only names */
    Group,    /* .ode's group (its lines skipped) */
    Comment   /* "text": a comment the model shows (text) */
  };
  Kind kind = Kind::Fixed;
  Pos pos;
  std::string name;
  Pos name_pos;
  /* the name's index, x[e] (an array's element) */
  std::optional<Expr> name_index;
  std::string text;
  Expr expr;
  std::vector<Binding> bindings;
  std::vector<std::string> names;
  std::vector<Pos> name_positions;
  /* each of names' index, when one has one (empty when none has) */
  std::vector<std::optional<Expr>> name_indices;
  /* the trailing range (.odex), and which array statement this one is a
     copy of (both readers' copies) */
  Range range;
  ArrayCopy array;
  /* a Derived computed only when a parameter changes, as its formula
     reads only parameters, consts and pure functions (and derived
     quantities before it): the builder's finding (form_ode.cpp), false
     for an .ode's !name = expr that reads t, a variable or a random
     function, which --convert refuses */
  bool parameters_only = false;
  /* a block function's body (has_body) */
  bool has_body = false;
  std::vector<BlockStmt> body;
  std::vector<Option> options;
  /* an event's sign, a Markov variable's number of states, a formula
     table's number of points */
  int count = 0;
  /* a formula table's lo and hi */
  double lo = 0, hi = 0;
  /* a table's kind: a file's values (text the file), a formula's (expr,
     count points from lo to hi), or .ode's 2-D table (table name @ file:
     text the file) */
  enum class TableKind { File, Formula, TwoD };
  TableKind table_kind = TableKind::File;
  /* a Markov variable's transitions, row by row */
  std::vector<Expr> cells;
  /* a network's arguments, each as written, and each one's index when it
     is an array's element, name[e] (empty when none is) */
  std::vector<std::string> call_args;
  std::vector<std::optional<Expr>> call_arg_indices;
};

/* a parsed model: its files (the model's, then those it includes) and
   its statements in order, an include's where the include was;
   ieee_division when its formulas divide as IEEE does (1/0 inf, 0/0 NaN:
   .odex's, docs/odex.md question 1), not as .ode's; derived when a fixed
   quantity that reads only parameters, consts and pure functions is a
   derived one (.odex's, docs/odex.md question 9; an .ode's fixed
   quantities stay fixed, as XPPAUT's) */
struct Parsed {
  std::vector<std::string> files;
  std::vector<Statement> statements;
  bool ieee_division = false;
  bool derived = false;
  /* while an .ode is read: the includes it is inside, 0 in its own lines */
  int included = 0;
};

/* text, the contents of file, parsed; an include is read relative to
   file's folder, as one of m's files (model_files.h). Throws Error. */
Parsed parse(xpp::Model &m, std::string_view text, const std::string &file);
/* parse, the text read from path (one of m's files) first */
Parsed parse_file(xpp::Model &m, const std::string &path);

/* the words .odex reserves (docs/odex.md "Reserved words"): a name may
   never be one */
bool is_reserved(std::string_view word);
/* name is an .odex name: a letter, then letters, digits and '_' */
bool is_name(std::string_view name);

/* the .odex reader's second half (odex_load.cpp): a parsed model checked
   and readied for the builder: a block function made the one expression
   its returns make, near's tol filled in, wiener's names given their
   value, an @ line's, a set's, a network's and a comment's text the
   Model's own, ieee_division set. Throws Error. */
Parsed ready(const Parsed &p);

/* path names an .odex model (its extension, any case) */
bool is_odex(std::string_view path);
/* the .odex model at path into the loading Session s, as get_eqn
   reads an .ode's (ode_read.h): parse_file, ready, then build_model: 1;
   a problem is logged (file, line, column) and the load fails
   (xpp::model_failed, with the problem as its diagnostic) */
int load(xpp::Session &s, const std::string &path);

/* --convert's question about a name .odex reserves: the question and a
   suggested answer; the answer ("" takes the suggestion), or nullopt
   when nobody can answer (no terminal) */
using Ask = std::function<std::optional<std::string>(const std::string &question, const std::string &suggestion)>;
/* the .ode model loaded in s as .odex text, from what the
   .ode parser understood (odex_convert.cpp); auto_answer takes every
   suggested name. Throws Error. */
std::string convert_model(xpp::Session &s, bool auto_answer, const Ask &ask);
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
/* e as the expression engine's text (an .ode formula's), with the
   parentheses the engine's precedence needs to keep e's grouping, a sign
   and an if always bracketed; an .ode formula (Text) as it is */
std::string engine_text(const Expr &e);

}

#endif
