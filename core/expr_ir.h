#pragma once
/* The Generator's (expr_native.cpp) output: a model's programs as expression trees and flat statement lists,
   for core/xpp_mir.cpp to lower through MIR's API. No text is written or read anywhere: every name is an index,
   every number a double, and every function the code calls is a row of `callees` (a function of ours at an
   address we gave it, or a function of the module), so a back end calls nothing else. */
#include <array>
#include <span>
#include <string>
#include <vector>

namespace xpp {

/* A function the code calls, returning a double. An external one (`address`) takes the Session first when
   `session`, then `doubles` doubles, then one int when `index` (the helpers of expr_native.cpp and the math
   table's functions); one with no address is the module's function of that name, whose parameters its
   IrFunction::Kind gives. */
struct IrCallee {
    std::string name;
    const void *address = nullptr;
    bool session = false;
    int doubles = 0;
    bool index = false;
};

enum class IrOp : unsigned char {
    Number,        /* number */
    Constant,      /* c[index] */
    Variable,      /* v[index] */
    Temporary,     /* the value a statement stored in temporary `index` */
    Argument,      /* a user function's double argument `index` */
    LoopCounter,   /* the integer counter of loop `index`, as a double */
    Workspace,     /* w[index], the exact Jacobian's workspace */
    Operand,       /* (double)R[index], a network pair formula's operand */
    Add,           /* args[0] + args[1], in that order */
    Subtract,
    Multiply,
    Divide,        /* IEEE: 1/0 is inf, 0/0 is NaN */
    GuardedDivide, /* args[0] / (args[1]==0 ? zero_divisor : args[1]), the XPP division */
    Select,        /* args[0]==0 ? args[1] : args[2] */
    Call           /* callees[callee](arguments..., index): its `args[1]` arguments, the nodes IrModule::call_arguments
                      holds from `args[0]`; the Session (and for a module function the arrays) as its callee says */
};

struct IrNode {
    IrOp op = IrOp::Number;
    int index = 0;  /* a variable, constant, temporary, argument or operand; a call's int argument */
    int callee = 0; /* a call's callee */
    double number = 0;
    std::array<int, 3> args{-1, -1, -1}; /* nodes; a call's first argument position and count */
};

struct IrStatement {
    enum class Kind : unsigned char {
        Temporary,            /* t[index] = value */
        Variable,             /* v[index] = value */
        Constant,             /* c[index] = value */
        Output,               /* o[index] = value */
        Workspace,            /* w[index] = value */
        ReturnWithoutOutput,  /* the equations are written only when the caller gives an output array */
        LoopBegin,            /* loop `index` opens: slot = 0, its counter runs from value to other (integers, the bounds truncated) */
        LoopEnd,              /* loop `index` closes: slot = slot + value, then the next counter or the end */
        IfBegin,              /* the statements to IfElse run when value is 0 */
        IfElse,               /* the statements to IfEnd run when it is not (a NaN is not 0) */
        IfEnd                 /* the conditional ends */
    } kind = Kind::Temporary;
    int index = 0;
    int value = -1; /* a node */
    int other = -1; /* a node: LoopBegin's high bound */
    int slot = -1;  /* a temporary: LoopBegin's and LoopEnd's total */
};

struct IrFunction {
    /* the C signature each has (all pointers but the doubles):
       User      double (c, v, S, a0, a1, ...)
       Equation  double (c, v, S, R)   R the program's RPN, a network pair formula reads its operands
       Rhs       void   (c, v, S, o)
       Jacobian  void   (c, v, S, w, o) */
    enum class Kind : unsigned char { User, Equation, Rhs, Jacobian } kind = Kind::Equation;
    std::string name;
    int doubles = 0; /* a User function's arguments */
    std::vector<IrStatement> statements;
    int result = -1; /* the value a User or Equation function returns, a node */
    int temporaries = 0;
    int loops = 0;
};

struct IrModule {
    std::vector<IrNode> nodes;
    std::vector<int> call_arguments;
    std::vector<IrCallee> callees;
    std::vector<IrFunction> functions;
    double zero_divisor = 0; /* what GuardedDivide divides by when its divisor is zero */

    std::span<const int> arguments(const IrNode &call) const
    {
        return std::span<const int>(call_arguments).subspan(static_cast<size_t>(call.args[0]), static_cast<size_t>(call.args[1]));
    }
};

} // namespace xpp
