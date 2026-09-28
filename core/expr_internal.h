#ifndef XPP_EXPR_INTERNAL_H
#define XPP_EXPR_INTERNAL_H
/* What the expression engine's files share (expr.h): the .ode compiler's
   tokens, which are indexes into the symbol table's built-ins
   (expr_symbols.cpp), and the tests on what a symbol compiles to. C++
   only; nothing outside expr_*.cpp includes it. */
#include "expr_program.h"
#include "session.h"

/* tokens: the built-in symbols' indexes (expr_symbols.cpp's table) */
#define LPAREN 0
#define RPAREN 1
#define COMMA  2
#define MINUS 4
#define NEGATE 9
#define STARTTOK 10
#define ENDTOK 11
#define DELSYM  42
#define SHIFTSYM 64
#define DELSHFTSYM 65
#define ISHIFTSYM 67
#define INDX 68
#define SETSYM  72
#define FIRST_ARG 73 /* ARG1, the first of 20 */
#define NUMTOK 59
/* the compiler's temporary symbol: a delay's, shift's or set's variable */
#define LASTTOK (MAX_SYMBS-2)

namespace xpp::expr {

/* the current Session's symbol table */
inline std::array<ExprSymbol, MAX_SYMBS> &symbols()
{
  return xpp::session().parser.symbols;
}

/* what an instruction (a symbol's com) is: a user function, a parameter,
   a variable, a lookup table */
inline bool is_ufun(int com) { return com / MAXTYPE == UFUNTYPE; }
inline bool is_ucon(int com) { return com / MAXTYPE == CONTYPE; }
inline bool is_uvar(int com) { return com / MAXTYPE == VARTYPE; }
inline bool is_lookup(int com) { return com / MAXTYPE == TABTYPE; }

}

#endif
