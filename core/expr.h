#pragma once
/* The expression engine: a model's formulas, compiled once as it loads and
   evaluated at every step. Four files, one job each:

   - expr_symbols.cpp, the symbol table: the names a formula may use (the
     built-in functions and operators, then the model's parameters,
     variables, user functions, tables, kernels, networks), what each
     compiles to, and the parameters' and variables' values by name
     (add_con, add_var, get_val, set_val ...);
   - expr_compile.cpp, the compiler: a formula's text, tokens (the symbol
     table's), checked, then a program in reverse Polish order (add_expr);
   - expr_eval.cpp, the evaluator: runs a program on its stack (evaluate),
     every right-hand side at every step, the hottest code there is;
   - expr_functions.cpp, the built-in functions a program calls (heaviside,
     max, mod, ran, delay, shift, the comparisons ...).

   A compiled program is the interface between a compiler and the
   evaluator: an int array ending in ENDEXP, whose format expr_program.h
   documents. A second front end (the .odex parser, W74) writes the same
   programs, and evaluate() and the built-ins serve both. The four files
   share expr_internal.h; the rest of the core uses this header. */

#include <cstdint>
#include <map>
#include <numbers>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <stddef.h>
#include <utility>
#include "xpp_error.h"
#include "expr_compiler.h"

/* the built-in symbols, the symbol table's first entries (expr_symbols.cpp) */
#define STDSYM 96
/* the built-in argument symbols ARG1..ARG20: a user function with more
   arguments gets its own (ParserState::arg_symbol) */
#define BUILTIN_ARGS 20

namespace xpp {

struct Session; /* session.h */
struct ParserState;
struct TABULAR; /* expr_tables.h */
class Random; /* xpp_math.h */

/* What a compiled formula calls that a run owns (the program's delay, integral and network terms), as an
   interface the expressions own and the Session's constructor fills with the run's functions (session.cpp),
   so the evaluator never includes what implements them. Each takes the Session the formula runs in. */
struct ExprHost {
  /* delay(tau, x) and delay(tau, x, shift): the delayed value of variable x (as the program holds it) */
  double (*delay)(Session &s, double tau, double variable);
  double (*delay_shift)(Session &s, double tau, double shift, double variable);
  /* integral equation kernel `index`'s value at the step in progress */
  double (*kernel_value)(Session &s, int index);
  /* network `index`'s and vectorizer `index`'s value at x */
  double (*network_value)(Session &s, double x, int index);
  double (*vector_value)(Session &s, double x, int index);
  /* the right-hand side is native and reads nothing a run changes (autoX's workers and a sweep's threads need it) */
  bool (*pure_rhs_ready)(const Session &s);
};

/* a 2-D table (not supported): 1 */
int add_2d_table(std::string_view name, std::string_view file);

/* the number at source[*ind] (the parser's grammar): its text into num
   (at most 39 characters and a NUL), its value into *value, *ind past
   it; 1 (with a WARN when report is set) when it is not a number */
int do_num(std::string_view source, double *value, int *ind, int report);

/* The symbol table and the compiler are a Session's ParserState (with its Model's names and programs through
   ParserState::compiler): a load passes the one it builds (W47d3), a command or a run the one it runs in. */

/* starts a load: the symbol table back to the built-ins, no parameters,
   variables, user functions or kernels */
void init_rpn(ParserState &p);
/* the symbol table's entries, each returning 0 when the name was added
   and 1 (said why) when it was not: a parameter of value, a kernel of
   mu and formula expr ("kerexpr#expr" is a convolution), network index's
   name (with vectorizer set, vectorizer index's), table index's name, user function name (index, narg
   arguments; add_ufun also compiles expr) */
int add_con(ParserState &p, std::string_view name, double value);
int add_kernel(ParserState &p, std::string_view name, double mu, std::string_view expr);
int add_net_name(ParserState &p, int index, std::string_view name, int vectorizer);
int add_table_name(ParserState &p, int index, std::string_view name);
int add_ufun_name(ParserState &p, std::string_view name, int index, int narg);
int add_ufun(ParserState &p, std::string_view junk, std::string_view expr, int narg);
/* user function index with the arguments args and the formula rhs */
int add_ufun_new(ParserState &p, int index, std::string_view rhs, std::span<const std::string> args);
/* compiles expr into command, ending in ENDEXP;
   *length is the program's length, ENDEXP included; 0 when it compiled,
   1 (said why) when it did not (expr_compile.cpp) */
int add_expr(ParserState &p, std::string_view expr, std::vector<int> &command, int *length = nullptr);
/* program's value, run on p's stacks: the right-hand side, every step
   (expr_eval.cpp). A Program runs its native function when it has one,
   else its instructions, decoded from its RPN once; a program held only
   as RPN (an expression entered once) is decoded for the evaluation. */
struct Program;
double evaluate(ParserState &p, const Program &program);
double evaluate(ParserState &p, std::span<const int> program);
/* A native program's value from the caller's own arrays, with p only read:
   for a program translate_model proved array_pure (it reads c, v and the
   immutable lookup tables, never p's stacks), so autoX's collocation workers
   share p const (expr_eval.cpp). */
double evaluate_pure(const ParserState &p, const Program &program, double *constants, double *variables);
/* The exact Jacobian's entries of p (expr_program.h JacobianProgram) into out, evaluated from their graph
   (its nodes' values kept in `values`, p's own in it), from the constants c and the variables v, with the tables only
   read: what p's native function computes once built, bit for bit the same (expr_eval.cpp). */
struct JacobianProgram;
namespace expr { class Symbolic; }
void evaluate_derivatives(const std::vector<TABULAR> &tables, const expr::Symbolic &graph, const JacobianProgram &p, const double *c, const double *v,
                          std::vector<double> &values, std::span<double> out);
/* name as the symbol table keeps it: blanks removed, upper case */
std::string converted(std::string_view name);
/* name (as converted makes it) is a built-in symbol, the symbol table's
   first STDSYM, the same in every Session */
bool is_builtin_symbol(std::string_view name);
int builtin_arity(std::string_view name); /* .odex calls: -1 absent, -2 named arguments. */
/* pi, the constant PI of every Session's symbol table */
inline constexpr double pi_value = std::numbers::pi;
/* name (upper case) is a built-in function of numbers alone: the one- and
   two-argument functions the evaluator calls through its tables, not the
   operators, delay, shift, sum or the random functions */
bool is_pure_builtin(std::string_view name);
/* the pure built-in name (the case .odex writes it in) at args, nullopt
   when it is none or takes another number of arguments: the value the
   evaluator gives, from the same tables */
std::optional<double> pure_builtin_value(std::string_view name, std::span<const double> args);
/* the symbol table by name (as converted makes it, of any length): a
   variable's, a lookup table's or a parameter's index (-1 when name is
   not one); a parameter's or variable's value got or set (1 when name is
   one); add_var adds a variable (0 when it did) */
int get_var_index(const ParserState &p, std::string_view name);
int find_lookup(const ParserState &p, std::string_view name);
int get_param_index(const ParserState &p, std::string_view name);
int get_val(const ParserState &p, std::string_view name, double *value);
int set_val(ParserState &p, std::string_view name, double value);
/* where a parameter's or variable's value lives (nullptr when name is not
   one): a loop that sets or reads it row after row looks it up once */
double *value_slot(ParserState &p, std::string_view name);
int add_var(ParserState &p, std::string_view name, double value);

/* a name the symbol table knows: its length, what it compiles to (com,
   an instruction of expr_program.h), its number of arguments and its
   priority (the compiler's) */
struct ExprSymbol {
  std::string name;
  int len = 0;
  int com = 0;
  int arg = 0;
  int pri = 0;
};

/* User functions nest this deep at most in the interpreter: sized so the thread's
   stack (1 MB default on Windows) and the evaluation stack cannot overflow before it. */
constexpr int MAX_UFUN_DEPTH = 256;

/* the evaluator's two stacks (expr_eval.cpp): the values it computes, and
   the arguments of the user functions it is inside */
struct ExprStack {
  std::vector<double> values;
  int top = 0;
  std::vector<double> args;
  int nargs = 0;
  int ufun_depth = 0;
};

/* Reserve a compiled expression and the functions it can enter before
   evaluation starts; never allocate on the evaluator's push path. */
void size_expr_stack(ParserState &p, std::span<const int> code, int arguments);

/* The expression engine's state, a Session's (session.h): the constants
   (parameters and numbers) and variables as the programs read them, how
   many constants and symbols there are (an expression compiled after the
   load adds its own above the Model's ncon_start/nsym_start and rolls
   back to them), whether a parse reports its errors (errout), the symbol
   table (its first STDSYM the built-ins, which the constructor puts
   there: expr_symbols.cpp) and the evaluator's stacks */
struct ParserState {
  ParserState();
  /* Indexed in the symbol owner: exact names and longest token prefixes.
     The index follows nsym; a slot written again (set_symbol after nsym
     was rolled back) is dropped from it first, a renamed one re-indexed. */
  int symbol_named(std::string_view name) const;
  int symbol_prefix(const char *source, int &length) const;
  /* the symbol of argument i (from 0) of a user function, ARG{i+1}: the
     built-ins hold the first BUILTIN_ARGS, a symbol past them is appended
     to the table when first needed */
  int arg_symbol(int i);
  /* the arguments the table has had symbols for: no function reads more, and
     the compiler's stack holds that many (kept, not counted from the table:
     every compiled expression asks, and a rolled-back table only makes it an
     upper bound) */
  int arg_count() const { return arg_count_; }
  void symbol_name_changed(int index);
  void forget_symbols_from(int index) const;

  /* The constants (parameters), the variables and the symbols grow as
     the model adds them (add_con, add_var, set_symbol), so a model has no
     limit on their number (W243). A pointer into constants or variables
     (value_slot) is valid until the next add. The symbols always keep
     two entries past nsym, of which the last is the
     compiler's temporary symbol. */
  std::vector<double> constants;
  std::vector<double> variables;
  int ncon = 0;
  int nsym = STDSYM;
  int errout = 0;
  std::vector<ExprSymbol> symbols;
  /* room for the symbol nsym and the compiler's temporary one */
  void room_for_symbol()
  {
    if (symbols.size() < static_cast<size_t>(nsym) + 3) symbols.resize(2 * symbols.size());
  }
  ExprStack stack;
  /* the run's functions the formulas call (ExprHost), set by the Session's constructor */
  const ExprHost *host = nullptr;
  /* the model's user functions, kernels, symbol counts and name index (expr_compiler.h): the Model's, set by the
     Session's constructor */
  CompilerTables *compiler = nullptr;
  /* what a formula reads of the Session it runs in, set by the Session's constructor: the lookup tables its table
     terms read (and add_table_name sizes), the generator its random terms draw from, and the Session itself as
     an opaque handle, given back to the ExprHost's functions, never read here */
  std::vector<TABULAR> *tables = nullptr;
  Random *random = nullptr;
  Session *owner = nullptr;
  /* why a step or an evaluation cannot go on (a delay out of range, a DAE solve, a user function nested past
     MAX_UFUN_DEPTH, a program that cannot be run), the first one recorded by stop_integration: the integration
     ends after the step and returns it (integrate.cpp take_step_error); what no caller takes the command shows
     when it ends (ui_json.cpp) */
  std::optional<Error> step_error;
  /* how many instructions the interpreter has run (a program's count at each evaluation, a sum's body's at
     each index, a called function's at each call): what says when a native build is due (model_native.cpp
     NativeBuild) */
  std::uint64_t interpreted_ops = 0;
  /* the Model builder's count of variables read (in_vars) and the named
     auxiliary variables (naux of aux_names) (form_ode.cpp) */
  struct {
    int in_vars = 0, naux = 0;
    std::vector<std::string> aux_names; /* the n-th aux name (naux of them) */
  } build;
private:
  void sync_symbols() const;
  void unindex_symbol(int index) const;
  mutable std::vector<std::string> indexed_names_;
  mutable std::map<std::string, std::set<int>, std::less<>> names_;
  int arg_count_ = BUILTIN_ARGS;
};


/* a step or an evaluation that cannot go on: records why, the first only (ParserState::step_error) */
inline void stop_integration(ParserState &p, Error why)
{
  if (!p.step_error) p.step_error = std::move(why);
}
/* the recorded reason (there must be one), cleared: the error the command shows (integrate.cpp's take_step_error also resets the DAE solve) */
inline Error take_stop(ParserState &p)
{
  Error why = std::move(*p.step_error);
  p.step_error.reset();
  return why;
}

} // namespace xpp
