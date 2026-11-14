#pragma once
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
                place too; INTEGER_POWER is x^n written with a literal
                whole n from 2 to MAX_INTEGER_POWER, multiplied out
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
     RANDUNI    pop x, push x times a uniform draw of (0,1): RAN(x)
     RANDPOI    pop m, push a Poisson draw of mean m: POISSON(m)
     RANDNORM   pop sd then mean, push a normal draw: NORMAL(mean,sd)
                (the three from the Session's generator, Session::random;
                the compiler writes them for the symbol table's
                COM(FUN1TYPE,RANDOM_UNIFORM) ..., random_instruction)
     ENDEXP     the end: the program's value is the stack's top */
#include "xpp_error.h"
#include "xpp_mir.h"
#include "expr_ir.h"

#include <array>
#include <atomic>
#include <bit>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <cstddef>
namespace xpp { struct Session; }

namespace xpp::expr {
using Fun1 = double (*)(double);
using Fun2 = double (*)(double, double);

/* a value of a Symbolic graph (expr_derivative.h): its node's index */
class Symbolic;
struct Term {
    int id = -1;
    friend bool operator==(Term, Term) = default;
};

/* The interpreter's instruction (expr_eval.cpp, W717): the RPN decoded once
   (expr_decode.h checked it), so that running a program reads no instruction
   twice: every operand is an index or a value here, every built-in function
   its address, and every jump the index of the instruction it goes to. */
enum class OpKind : unsigned char {
    Number,        /* push number (NUMSYM, INDXCOM, and SVARTYPE's or SCONTYPE's COM as a double) */
    Constant,      /* push constants[a] */
    Variable,      /* push variables[a] */
    Argument,      /* push argument a of the user function running (count: the stack's depth here, for ArgumentAt) */
    ArgumentAt,    /* push the value a below the top: argument of a user function whose body is inlined */
    Add,           /* FUN2TYPE's + - * / (the .ode division: a zero divisor is ZERO_DIVISOR) and IEEE_DIVIDE */
    Subtract,
    Multiply,
    Divide,
    IeeeDivide,
    AddNumber,     /* the same with their right operand number, constants[a] or variables[a] pushed first: one
                      instruction for the two, the operation as written */
    SubtractNumber,
    MultiplyNumber,
    DivideNumber,
    IeeeDivideNumber,
    AddConstant,
    SubtractConstant,
    MultiplyConstant,
    DivideConstant,
    IeeeDivideConstant,
    AddVariable,
    SubtractVariable,
    MultiplyVariable,
    DivideVariable,
    IeeeDivideVariable,
    Fun1,          /* pop x, push fun1(x) */
    Fun2,          /* pop y then x, push fun2(x,y) */
    Call,          /* user function a with count arguments (UFUNTYPE) */
    Drop,          /* an inlined user function's end: its value replaces its a arguments beneath it (ENDFUN) */
    If,            /* pop x; when x is 0 go to instruction a (MYIF) */
    Then,          /* go to instruction a (MYTHEN) */
    Sum,           /* pop high then low; run the instructions that follow, to their Return, for each index; go to a (SUMSYM) */
    Delay,         /* ENDDELAY ... RANDNORM as expr_program.h describes them */
    DelayShift,
    Shift,
    IShift,
    Set,
    Table,         /* pop x, push table a at x; likewise network a, vectorizer a */
    Network,
    Vector,
    Kernel,        /* push kernel a's integral */
    RandomUniform,
    RandomPoisson,
    RandomNormal,
    Return         /* the program's, or a sum's body's, value is the top (ENDEXP, ENDSUM) */
};
struct Op {
    OpKind kind = OpKind::Return;
    int a = 0;
    union {
        double number = 0; /* Number */
        Fun1 fun1;         /* Fun1 */
        Fun2 fun2;         /* Fun2 */
        int count;         /* Call: its argument count; Argument: the stack's depth */
    };
};
}

namespace xpp {
/* A program the Model holds: its RPN, the interpreter's form of it, and the
   function translate_model made from it (none: interpreted). The function's
   last argument is the RPN, whose first two operands a network rewrites for
   each pair (and the first two instructions of `ops` with them). */
struct Program {
    std::vector<int> rpn;
    /* the RPN decoded for the interpreter (expr_eval.cpp), made at the first evaluation, on the core thread, the
       only one that interprets (a run on threads calls native code: native_ready); remade after the RPN is
       rewritten (forget_code) */
    mutable std::vector<expr::Op> ops;
    std::vector<int> native_reads; /* variables read by this code and its user functions */
    bool array_pure = false; /* native code only uses c/v and immutable lookup tables */
    double (*native)(double *, double *, Session *, const int *) = nullptr;
    bool compiled = false; /* translate_model made native code for it (W715): `native` once build_native built the model's code */
    bool in_rhs = false; /* compiled only inside the model's one right-hand side function (W708): no native of its own */
    /* the RPN was rewritten at run time (a user function redefined, a DAE guess edited): neither the native code
       nor the interpreter's form holds any more */
    void forget_code() { native = nullptr; compiled = false; ops.clear(); }
};

/* A module MIR compiles on a thread of its own (W715; the programs' translate_model found native, or the exact
   Jacobian's derivatives), from the IrModule the build holds, while the run goes on interpreted, bit for bit
   the same. The thread starts once the interpreter has done the work of a short run (`due`, against
   ParserState::interpreted_ops: W717, so a short run never pays for a thread), or at once for a run on threads
   (native_ready); the core thread takes the finished Program at the start of a right-hand side
   (my_rhs, build_native), never mid-evaluation. The thread is detached and holds the build with the Model (a
   shared_ptr each): when the Model goes first (a reload, a quit while it compiles) the thread finishes on its own
   and its result goes with the build. */
struct NativeBuild {
    IrModule ir;            /* the thread's alone once started */
    Place place;            /* where MIR's error is reported */
    std::string rhs_symbol; /* the programs' build: the right-hand side's function (native_rhs), "" when there is none */
    bool equations = false; /* the programs' build: native_equations once built */
    std::uint64_t due = 0;  /* the interpreted_ops count the thread starts at */
    bool started = false;
    std::atomic<bool> finished{false}; /* set by the thread once `result` is written; waited on with atomic wait */
    Result<std::unique_ptr<mir::Program>> result;
};

/* Derivatives of the exact Jacobian (W260, expr_derivative.h): entry k is
   d f_rows[k] / d x of column columns[k] (a state's index from 0) in the
   state Jacobian, and of the column's own variable in the others (columns
   empty). Its value is terms[k] of the Jacobian's graph, which
   evaluate_derivatives (expr.h) computes through `nodes` (the graph's
   nodes the entries need, in their order) until MIR's code is built:
   then `function` writes entry k into out[k], from the constants c and
   the variables v (v[0] the time, v[1..] the state; only these are read)
   and the Session S (its tables, only read), bit for bit the same. */
struct JacobianProgram {
    void (*function)(const double *c, const double *v, void *S, double *out) = nullptr;
    std::vector<int> rows, columns;
    std::vector<expr::Term> terms;
    std::vector<int> nodes;
};

/* The model's exact Jacobian (W260): with respect to the state, the time
   and each constant that a right-hand side depends on (by_constant, by
   the constant's index, ascending), or why the model has none. */
struct ExactJacobian {
    std::shared_ptr<expr::Symbolic> graph; /* the derivatives' values (expr_derivative.h), when the model has them */
    std::shared_ptr<NativeBuild> build;  /* MIR's compile of them, until taken (build_native) */
    std::unique_ptr<mir::Program> code;
    JacobianProgram states, time;
    std::vector<std::pair<int, JacobianProgram>> by_constant;
    /* why there is none, when there is no graph */
    std::string unavailable;
    /* where the model's own @ jacobian=exact is, for the error when there
       is none */
    Place requested;
    /* made, or tried and failed: ensure_exact_jacobian (expr_native.h)
       makes it on first need */
    bool tried = false;
    bool available() const { return graph != nullptr; }
};
}

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
#define RANDUNI 984
#define RANDPOI 983
#define RANDNORM 982

namespace xpp::expr {

/* the constant SUM(low,high)OF(...) sets to each index in turn: I', the
   second constant init_rpn adds (after PI) */
constexpr int SUM_INDEX = 1;

/* the value of the vector index @ of a vector formula (INDXCOM) */
constexpr double CURRENT_INDEX = 0;

/* the built-in functions FUN1TYPE and FUN2TYPE call, by index
   (expr_functions.cpp), FUN1_COUNT and FUN2_COUNT of them */
constexpr std::size_t FUN1_COUNT = 26, FUN2_COUNT = 24;
extern const std::array<Fun1, FUN1_COUNT> fun1;
extern const std::array<Fun2, FUN2_COUNT> fun2;
/* the FUN2TYPE indices of + - * / (an .ode model's guarded division),
   which the evaluator does itself */
constexpr int ADD = 0, SUBTRACT = 1, MULTIPLY = 2, DIVIDE = 3;
/* the FUN2TYPE index of x^y (also written x**y) */
constexpr int POW = 5;

/* the symbol table's indices of RAN(x) (FUN1TYPE), POISSON(m) (FUN1TYPE)
   and NORMAL(mean,sd) (FUN2TYPE): their table entries are null (the
   tables hold no Session), and a program draws them with RANDUNI,
   RANDPOI and RANDNORM, instructions of their own, so that no other
   instruction's dispatch pays for them */
constexpr int RANDOM_UNIFORM = 19;
constexpr int RANDOM_POISSON = 24;
constexpr int RANDOM_NORMAL = 17;

/* the instruction the compiler writes for the symbol table's com, and
   back (odex_convert.cpp reads programs) */
constexpr int random_instruction(int com)
{
    return com == COM(FUN1TYPE, RANDOM_UNIFORM)   ? RANDUNI
           : com == COM(FUN1TYPE, RANDOM_POISSON) ? RANDPOI
           : com == COM(FUN2TYPE, RANDOM_NORMAL)  ? RANDNORM
                                                  : com;
}
constexpr int random_com(int instruction)
{
    return instruction == RANDUNI   ? COM(FUN1TYPE, RANDOM_UNIFORM)
           : instruction == RANDPOI ? COM(FUN1TYPE, RANDOM_POISSON)
           : instruction == RANDNORM ? COM(FUN2TYPE, RANDOM_NORMAL)
                                     : instruction;
}

/* the FUN2TYPE index of an .odex model's division, which the compiler
   writes for / when the Model's ieee_division is set */
constexpr int IEEE_DIVIDE = 22;
/* the FUN2TYPE index the compiler writes for x^n (x**n) when n is a
   literal whole number from 2 to MAX_INTEGER_POWER: x^n multiplied out
   (xpp_math.h integer_power), compiled and interpreted alike; any other
   exponent, a parameter's too, is POW's correctly rounded pow (W471) */
constexpr int INTEGER_POWER = 23;
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

/* ENDDELAY, ENDDELSHFT, ENDSHIFT and ENDISHIFT (expr_functions.cpp), in
   the Session s the evaluator runs in */
double do_delay(xpp::Session &s, double delay, double i);
double do_delay_shift(xpp::Session &s, double delay, double shift, double variable);
double do_shift(const xpp::Session &s, double shift, double variable);
double do_ishift(double shift, double variable);
/* ENDSET: variable v+shift becomes value (v and shift as the program
   holds them); the value comes back. A variable outside the Session's
   variables is not written. */
double do_set(xpp::Session &s, double shift, double variable, double value);

/* RANDUNI, RANDPOI and RANDNORM, from the Session's generator */
double do_random_uniform(xpp::Session &s, double x);
double do_random_poisson(xpp::Session &s, double mean);
double do_random_normal(xpp::Session &s, double mean, double sd);

}

