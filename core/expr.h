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
#ifdef __cplusplus
extern "C" {
#endif

/* the built-in symbols, the symbol table's first entries (expr_symbols.cpp) */
#define STDSYM 96

/* starts a load: the symbol table back to the built-ins, no parameters,
   variables, user functions or kernels, and the random generator seeded */
void init_rpn(void);
/* the symbol table's entries, each returning 0 when the name was added
   and 1 (said why) when it was not: a parameter of value, a kernel of
   mu and formula expr ("kerexpr#expr" is a convolution), network index's
   name (with vectorizer set, vectorizer index's), table index's name, user function name (index, narg
   arguments; add_ufun also compiles expr) */
int add_con(const char *name, double value);
int add_kernel(const char *name, double mu, const char *expr);
int add_net_name(int index, const char *name, int vectorizer);
int add_table_name(int index, const char *name);
int add_ufun_name(const char *name, int index, int narg);
int add_ufun(const char *junk, const char *expr, int narg);
/* table index filled from a file, or from formula at nn points in
   [xlo,xhi]: 0 when it was; a 2-D table (not supported): 1 */
int add_2d_table(const char *name, const char *file);
int add_file_table(int index, const char *file);
int add_form_table(int index, int nn, double xlo, double xhi, const char *formula);
/* variable i's value (0 past the model's variables) */
void set_ivar(int i, double value);
double get_ivar(int i);
/* name without blanks, in upper case, into dest (never longer than name) */
void convert(const char *source, char *dest);

/* compiles expr into command, at most MAXEXPLEN ints ending in ENDEXP;
   *length is the program's length, ENDEXP included; 0 when it compiled,
   1 (said why) when it did not (expr_compile.cpp) */
int add_expr(const char *expr, int *command, int *length);
/* the number at source[*ind] (the parser's grammar): its text into num
   (at most 39 characters and a NUL), its value into *value, *ind past
   it; 1 (with a WARN) when it is not a number */
int do_num(const char *source, char *num, double *value, int *ind);

/* program's value (expr_eval.cpp) */
double evaluate(const int *program);

#ifdef __cplusplus
}

#include <array>
#include <span>
#include <string>
#include <string_view>

/* user function index with the arguments args and the formula rhs */
int add_ufun_new(int index, const char *rhs, std::span<const std::string> args);
/* name as the symbol table keeps it: blanks removed, upper case */
std::string converted(std::string_view name);
/* the symbol table by name (as converted makes it, of any length): a
   variable's, a lookup table's or a parameter's index (-1 when name is
   not one); a parameter's or variable's value got or set (1 when name is
   one); add_var adds a variable (0 when it did) */
int get_var_index(std::string_view name);
int find_lookup(std::string_view name);
int get_param_index(std::string_view name);
int get_val(std::string_view name, double *value);
int set_val(std::string_view name, double value);
int add_var(std::string_view name, double value);

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
constexpr int EXPR_STACK = 200;
struct ExprStack {
  std::array<double, EXPR_STACK> values{};
  int top = 0;
  std::array<double, EXPR_STACK> args{};
  int nargs = 0;
};

/* The expression engine's state, a Session's (session.h): the constants
   (parameters and numbers) and variables as the programs read them, how
   many constants and symbols there are (an expression compiled after the
   load adds its own above the Model's ncon_start/nsym_start and rolls
   back to them), whether a parse reports its errors (errout), the symbol
   table (its first STDSYM the built-ins, which the constructor puts
   there: expr_symbols.cpp) and the evaluator's stacks */
struct ParserState {
  ParserState();
  std::array<double, MAXPAR> constants{};
  std::array<double, MAXODE1> variables{};
  int ncon = 0;
  int nsym = STDSYM;
  int errout = 0;
  std::array<ExprSymbol, MAX_SYMBS> symbols;
  ExprStack stack;
};
#endif
#endif
