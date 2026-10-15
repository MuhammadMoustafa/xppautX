#ifndef XPP_EXPR_H
#define XPP_EXPR_H
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
#include "xpplim.h"

#include <array>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "xpp_error.h"
#include "xpp_io.h"

/* the built-in symbols, the symbol table's first entries (expr_symbols.cpp) */
#define STDSYM 96

namespace xpp {

struct Session; /* session.h */

/* a 2-D table (not supported): 1 */
int add_2d_table(std::string_view name, std::string_view file);

/* the number at source[*ind] (the parser's grammar): its text into num
   (at most 39 characters and a NUL), its value into *value, *ind past
   it; 1 (with a WARN when report is set) when it is not a number */
int do_num(std::string_view source, char *num, double *value, int *ind, int report);

/* The symbol table and the compiler are the Session s's (its ParserState
   and its Model's names and programs): a load passes the Session it
   builds (W47d3), a command or a run the one it runs in. */

/* starts a load: the symbol table back to the built-ins, no parameters,
   variables, user functions or kernels, and the random generator seeded */
void init_rpn(Session &s);
/* the symbol table's entries, each returning 0 when the name was added
   and 1 (said why) when it was not: a parameter of value, a kernel of
   mu and formula expr ("kerexpr#expr" is a convolution), network index's
   name (with vectorizer set, vectorizer index's), table index's name, user function name (index, narg
   arguments; add_ufun also compiles expr) */
int add_con(Session &s, std::string_view name, double value);
int add_kernel(Session &s, std::string_view name, double mu, std::string_view expr);
int add_net_name(Session &s, int index, std::string_view name, int vectorizer);
int add_table_name(Session &s, int index, std::string_view name);
int add_ufun_name(Session &s, std::string_view name, int index, int narg);
int add_ufun(Session &s, std::string_view junk, std::string_view expr, int narg);
/* table index filled from a file, or from formula at nn points in
   [xlo,xhi]: what failed otherwise (the loader shows it) */
Result<> add_file_table(Session &s, int index, std::string_view file);
Result<> add_form_table(Session &s, int index, int nn, double xlo, double xhi, std::string_view formula);
/* user function index with the arguments args and the formula rhs */
int add_ufun_new(Session &s, int index, std::string_view rhs, std::span<const std::string> args);
/* compiles expr into command, ending in ENDEXP;
   *length is the program's length, ENDEXP included; 0 when it compiled,
   1 (said why) when it did not (expr_compile.cpp) */
int add_expr(Session &s, std::string_view expr, std::vector<int> &command, int *length = nullptr);
/* program's value, run on s's stacks: the right-hand side, every step
   (expr_eval.cpp) */
double evaluate(Session &s, const int *program);
/* Model-backed programs dispatch to their native function when enabled. */
struct Program;
double evaluate(Session &s, const Program &program);
/* A native program's value from the caller's own arrays, with s only read:
   for a program compile_model proved array_pure (it reads c, v and the
   immutable lookup tables, never s's stacks), so AUTO's collocation workers
   share s const (expr_eval.cpp). */
double evaluate_pure(const Session &s, const Program &program, double *constants, double *variables);
/* name as the symbol table keeps it: blanks removed, upper case */
std::string converted(std::string_view name);
/* name (as converted makes it) is a built-in symbol, the symbol table's
   first STDSYM, the same in every Session */
bool is_builtin_symbol(std::string_view name);
int builtin_arity(std::string_view name); /* .odex calls: -1 absent, -2 named arguments. */
/* the symbol table by name (as converted makes it, of any length): a
   variable's, a lookup table's or a parameter's index (-1 when name is
   not one); a parameter's or variable's value got or set (1 when name is
   one); add_var adds a variable (0 when it did) */
int get_var_index(const Session &s, std::string_view name);
int find_lookup(const Session &s, std::string_view name);
int get_param_index(const Session &s, std::string_view name);
int get_val(const Session &s, std::string_view name, double *value);
int set_val(Session &s, std::string_view name, double value);
/* where a parameter's or variable's value lives (nullptr when name is not
   one): a loop that sets or reads it row after row looks it up once */
double *value_slot(Session &s, std::string_view name);
int add_var(Session &s, std::string_view name, double value);

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

/* the evaluator's two stacks (expr_eval.cpp): the values it computes, and
   the arguments of the user functions it is inside */
struct ExprStack {
  std::vector<double> values;
  int top = 0;
  std::vector<double> args;
  int nargs = 0;
};

/* Reserve a compiled expression and the functions it can enter before
   evaluation starts; never allocate on the evaluator's push path. */
void size_expr_stack(Session &s, std::span<const int> code, int arguments);

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
  /* --convert's rewrite of the old-style file being read (form_ode.cpp,
     markov.cpp), open while it is read */
  Writer convert;
  /* the Model builder's count of variables read (in_vars), the named
     auxiliary variables (naux of aux_names) and the fixed variables'
     names (form_ode.cpp) */
  struct {
    int in_vars = 0, naux = 0;
    std::vector<std::string> aux_names; /* the n-th aux name (naux of them) */
    std::vector<std::string> fixname;   /* the n-th fixed variable's name (--convert) */
  } build;
private:
  void sync_symbols() const;
  void unindex_symbol(int index) const;
  mutable std::vector<std::string> indexed_names_;
  mutable std::map<std::string, std::set<int>, std::less<>> names_;
};

} // namespace xpp
#endif
