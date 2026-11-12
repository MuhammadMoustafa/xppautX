#pragma once
/* W711 spike: the Generator's (expr_native.cpp) second output beside the C text it writes for TinyCC: the same
   walk of a model's RPN, as expression trees and statements, for a back end that takes no text (MIR's API,
   tools/mir-spike/xpp_mir_ir.cpp). Only the model's right-hand side function (p_rhs) is written this way.
   Every name is an index; every function the code calls is in `callees`, so a back end resolves a call
   only through that table. */
#include <array>
#include <string>
#include <vector>

namespace xpp {

/* A function the code calls, returning a double: the Session first when `session`, then `doubles` doubles,
   then one int when `index` (the C prototypes of expr_native.cpp's helpers, and of the math table's functions) */
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
    Argument,      /* a user function's argument, only in an inlined body (the copy puts the argument there) */
    Add,           /* args[0] + args[1], in that order */
    Subtract,
    Multiply,
    Divide,        /* IEEE: 1/0 is inf, 0/0 is NaN */
    GuardedDivide, /* args[0] / (args[1]==0 ? zero_divisor : args[1]), the XPP division */
    Select,        /* args[0]==0 ? args[1] : args[2] */
    Call           /* callees[callee](args..., index), the Session first when the callee takes it */
};

struct IrNode {
    IrOp op = IrOp::Number;
    int index = 0;  /* a variable, constant, temporary or argument; a call's int argument */
    int callee = 0; /* a call's callee */
    double number = 0;
    std::array<int, 3> args{-1, -1, -1}; /* nodes */
};

struct IrStatement {
    enum class Kind : unsigned char {
        Temporary,            /* t[index] = value */
        Variable,             /* v[index] = value */
        Output,               /* o[index] = value */
        ReturnWithoutOutput   /* the equations are written only when the caller gives an output array */
    } kind = Kind::Temporary;
    int index = 0;
    int value = -1; /* a node */
};

struct RhsIr {
    std::vector<IrNode> nodes;
    std::vector<IrStatement> statements;
    std::vector<IrCallee> callees;
    int temporaries = 0;
    double zero_divisor = 0; /* what GuardedDivide divides by when its divisor is zero */
    /* why the function cannot be written this way (a sum, a user function's call, a conditional that
       runs statements): then `statements` is not to be lowered; "" when it can */
    std::string unsupported;
};

} // namespace xpp
