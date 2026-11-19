#pragma once
/* What the lowerings of an IrModule (expr_ir.h) share: the depth a tree may have, and the parameters a function of the
   module takes and a call passes, so that MIR's code (xpp_mir.cpp) and TinyCC's C text (xpp_tcc_lower.cpp) declare and
   call the same functions. */
#include "expr_ir.h"
#include <vector>

namespace xpp::ir {

/* A tree deeper than this is refused: the lowerings recurse on it, so a hostile deep expression cannot exhaust the host's
   stack (8 MB on every platform, Makefile's MAINSTACK). The parser's own limit is 256 levels (odex_parse.cpp
   max_expression_depth, long operator chains included) and an inlined user function adds at most its 64 nodes at a call;
   a level costs about 0.7 KB of stack, 5 KB under AddressSanitizer, so 1024 fits all of them. */
constexpr int MAX_TREE_DEPTH = 1024;

/* A pointer parameter of a function of the module or of a call: the constants c, the variables v, the parser S, the RPN R
   (an equation's: a call of one passes null), the output o (a right-hand side's or a Jacobian's: never called). */
enum class Hidden : unsigned char { Constants, Variables, Parser, Rpn, Output };

/* the parameter's name in both lowerings' code */
inline const char *hidden_name(Hidden h)
{
    switch (h) {
    case Hidden::Constants: return "c";
    case Hidden::Variables: return "v";
    case Hidden::Parser: return "S";
    case Hidden::Rpn: return "R";
    case Hidden::Output: return "o";
    }
    return "";
}

/* The hidden parameters a function of the module takes, before its doubles (IrFunction::Kind's C signatures). */
inline std::vector<Hidden> own_hidden(const IrFunction &f)
{
    std::vector<Hidden> hidden{Hidden::Constants, Hidden::Variables, Hidden::Parser};
    if (f.kind == IrFunction::Kind::Equation) hidden.push_back(Hidden::Rpn);
    if (f.kind == IrFunction::Kind::Rhs || f.kind == IrFunction::Kind::Jacobian) hidden.push_back(Hidden::Output);
    return hidden;
}

/* What a call of a callee passes before its IrNode arguments, how many doubles it takes after them, and whether an int (the
   call node's index) comes last: a module function (`module_function`) takes its own hidden parameters and doubles; an
   external one takes the parser when it says so, its doubles and the index when it takes one. */
struct CallShape {
    std::vector<Hidden> before;
    int doubles = 0;
    bool index = false;
};
inline CallShape call_shape(const IrCallee &target, const IrFunction *module_function)
{
    CallShape shape;
    if (module_function) {
        shape.before = own_hidden(*module_function);
        shape.doubles = module_function->doubles;
    } else {
        if (target.parser) shape.before.push_back(Hidden::Parser);
        shape.doubles = target.doubles;
        shape.index = target.index;
    }
    return shape;
}

} // namespace xpp::ir
