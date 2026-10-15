#ifndef XPP_EXPR_DECODE_H
#define XPP_EXPR_DECODE_H
/* The one decoder of a compiled program's instructions (expr_program.h
   describes the format, W276): it reads the ints once, checks the whole
   program (every operand's range, the stack's depth at every instruction,
   the if/then/else jumps, the sums' extents, a user function's argument
   count) and hands back the instructions decoded, for a walker over one
   value domain to run: Generator::sequence (C text, expr_native.cpp) and
   the differentiator's Reader (a Symbolic term, expr_derivative.cpp). A
   walker checks no structure of its own, only what its domain refuses.
   The interpreter (expr_eval.cpp) keeps its own loop over the ints: it
   runs every right-hand side at every step, and a program it runs was
   checked when it was compiled, so decoding there would cost a pass
   per evaluation. C++ only, the expression engine's own (expr.h). */
#include "expr_program.h"

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace xpp::expr {

/* the pseudo-instruction ending an else part (the join of a MYIF's two
   values), put where the else part ends: nothing in a program has it */
constexpr int JOIN = -1;

/* One instruction. op is what the program holds (or JOIN), type and
   in its COM(type,index) halves. MYELSE and ENDFUN are checked and left
   out; MYIF, MYTHEN and JOIN carry no operand (the walker keeps its own
   frame, the decoder has checked the jumps). A SUMSYM is followed by
   its body's instructions and an ENDSUM. The program's end, ENDEXP, is
   left out. */
struct Instr {
    int op = 0, type = 0, in = 0;
    std::size_t pc = 0;   /* where it is in the program */
    double number = 0;    /* NUMSYM */
    int count = 0;        /* UFUNTYPE: its argument count */
};

/* The instructions of program p, or why it is not one. arguments is the
   argument count of the user function p is, or -1 for an equation (which
   has no ENDFUN and no arguments to read). maximum_depth, when supplied,
   receives the largest value-stack depth, including nested SUM bodies. */
std::expected<std::vector<Instr>, std::string> decode(const Session &s, std::span<const int> p, int arguments,
                                                   std::size_t *maximum_depth = nullptr);

}

#endif
