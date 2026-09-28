#ifndef XPP_EXPR_PROGRAM_H
#define XPP_EXPR_PROGRAM_H
/* A compiled program: what a compiler writes (expr_compile.cpp's add_expr,
   and the .odex parser's, W74) and the evaluator runs (expr_eval.cpp's
   evaluate). C++ only, the expression engine's own (expr.h).

   A program is an array of ints read in order, in reverse Polish order: an
   operand pushes a value onto the evaluator's stack, an operator pops its
   arguments and pushes its result, and the program's value is what is on
   top when ENDEXP ends it. Most instructions are COM(type,index), index
   picking within the type:

     CONTYPE    push constant index (a parameter or a number the model named)
     VARTYPE    push variable index (0 is t)
     FUN1TYPE   pop x, push fun1[index](x) (expr_functions.cpp's table)
     FUN2TYPE   pop y then x, push fun2[index](x,y); index 0-3 are + - * /,
                done in place (a zero divisor becomes ZERO_DIVISOR), and
                IEEE_DIVIDE is / without that guard, an .odex model's
                division (1/0 is inf, 0/0 NaN: docs/odex.md), done in
                place too
     UFUNTYPE   user function index; the next int is its argument count n:
                pop n arguments onto the argument stack, push the value of
                the Model's ufun_programs[index]
     USTACKTYPE push argument index of the user function being evaluated
                (ARG1.. or its own names, from the argument stack's top)
     TABTYPE    pop x, push lookup table index at x
     NETTYPE    pop i, push network index's value i
     VECTYPE    pop i, push vectorizer index's value i
     KERTYPE    push kernel index's integral
     SVARTYPE   push COM(VARTYPE,index) itself, as a double: the variable a
     SCONTYPE   delay, shift or set names (SCONTYPE: COM(CONTYPE,index))

   and the others stand alone, some followed by ints of their own:

     NUMSYM     the next two ints are a double's second and first halves
                (number_halves): push it
     ENDFUN n   a user function's last instruction: drop its n arguments
     MYIF j     pop x; when x is 0, jump j ints ahead (to the else part)
     MYTHEN j   jump j ints ahead (past the else part)
     MYELSE     nothing (marks where the else part starts)
     SUMSYM j   pop high then low; run the j ints after it once for each
                index from low to high, SUM_INDEX's constant (I') the index,
                push the sum of their values, and jump past them; ENDSUM
                ends the part summed
     ENDDELAY   pop tau then v (an SVARTYPE): push v delayed by tau
     ENDDELSHFT pop tau, shift then v: variable v+shift delayed by tau
     ENDSHIFT   pop shift then v (SVARTYPE or SCONTYPE): push the value of
                the variable or constant shift places after v
     ENDISHIFT  pop shift then v: push the index v+shift
     ENDSET     pop value, shift then v: variable v+shift becomes value;
                push value
     INDXCOM    push 0 (the index @ of a vector formula)
     ENDEXP     the end: the program's value is the stack's top */
#include "expr.h"

#include <array>
#include <bit>

#define FUN1TYPE 9
#define FUN2TYPE 1
#define VARTYPE 3  /* standard variable */
#define CONTYPE 2  /* standard parameter */
#define UFUNTYPE   24
#define SVARTYPE 4  /* shifted variable */
#define SCONTYPE 32  /* shifted constant  */
#define NETTYPE 6
#define TABTYPE 7
#define USTACKTYPE 8
#define KERTYPE 10
#define VECTYPE 13  /* for vectorized stuff */
#define MAXTYPE 20000000  /* this is the maximum number of named stuff */

#define COM(a,b) ((a)*MAXTYPE+(b))

#define ENDEXP 999
#define ENDFUN 998
#define ENDDELAY 996
#define MYIF  995
#define MYELSE 993
#define MYTHEN 994
#define SUMSYM 990
#define ENDSUM 991
#define ENDSHIFT 988
#define NUMSYM 987
#define ENDDELSHFT 986
#define ENDISHIFT 985
#define ENDSET 981
#define INDXCOM 922

namespace xpp::expr {

/* the constant SUM(low,high)OF(...) sets to each index in turn: I', the
   second constant init_rpn adds (after PI) */
constexpr int SUM_INDEX = 1;

/* the built-in functions FUN1TYPE and FUN2TYPE call, by index
   (expr_functions.cpp) */
using Fun1 = double (*)(double);
using Fun2 = double (*)(double, double);
extern const std::array<Fun1, 26> fun1;
extern const std::array<Fun2, 23> fun2;

/* the FUN2TYPE index of an .odex model's division, which the compiler
   writes for / when the Model's ieee_division is set */
constexpr int IEEE_DIVIDE = 22;
/* what an .ode model's division divides by instead of 0 (XPP's guard) */
constexpr double ZERO_DIVISOR = 2.23E-15;

/* a double as two ints, its first and second halves in memory, and back
   (NUMSYM; the compiler's tokens hold them in that order, and reversing
   the tokens into the program swaps them) */
static_assert(sizeof(double)==2*sizeof(int),"a number is two ints");
inline std::array<int,2> number_halves(double z)
{
  return std::bit_cast<std::array<int,2>>(z);
}
inline double number_from_halves(int first, int second)
{
  return std::bit_cast<double>(std::array<int,2>{first,second});
}

/* ENDDELAY, ENDDELSHFT, ENDSHIFT and ENDISHIFT (expr_functions.cpp) */
double do_delay(double delay, double i);
double do_delay_shift(double delay, double shift, double variable);
double do_shift(double shift, double variable);
double do_ishift(double shift, double variable);

}

#endif
