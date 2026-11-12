#pragma once

#include "../../core/expr_ir.h"
#include "../../core/xpp_error.h"
#include <memory>
#include <span>
#include <string>

/* W706 spike: the adapter to third_party/mir (MIR master dc2c763), the same shape as
   core/xpp_tcc.h so a later card can put one compile entry in front of either.
   It is not in the default build (Makefile target `mirspike`).
   The two ways in (W711): the C text through c2mir (xpp_mir_c.cpp, W706's), and the model's right-hand side
   as the Generator's trees (core/expr_ir.h) built through MIR's API (xpp_mir_ir.cpp: no c2mir linked).
   This file is what both share: MIR's context, its errors, the link and the function lookup. */
namespace xpp::mir {

struct Symbol {
    const char *name;
    const void *address;
};

/* The compiled code. MIR is not a sandbox: only trusted, generated code belongs here, and every external
   reference must be a supplied symbol. The returned addresses live until the Program is destroyed. */
class Program {
public:
    /* W706: trusted, generated C compiled with no preprocessor (no #include reaches the file system).
       level: MIR's optimization level, 0 to 3 */
    static Result<std::unique_ptr<Program>> compile(const std::string &source,
                                                   std::span<const Symbol> symbols,
                                                   Place place, unsigned level);
    /* W711: p_rhs from trees; its imports are ir's callees, nothing else */
    static Result<std::unique_ptr<Program>> compile(const RhsIr &ir, Place place, unsigned level);
    ~Program();
    Program(const Program &) = delete;
    Program &operator=(const Program &) = delete;
    /* a function's code or a data item's storage */
    Result<void *> function(const std::string &name) const;
    /* where the compile's time went: the front end (c2mir, or the trees lowered through MIR's API) and MIR's
       load, optimization, code generation and link, in milliseconds (measured, never a result) */
    struct Timing {
        double front_ms = 0, generate_ms = 0;
    };
    Timing timing() const;

    struct State; /* public only for the front ends' calls into MIR */
    /* a front end puts the code into the context and sets state.module; it returns the text of what
       failed, or nullptr, and runs in a frame MIR's errors jump out of: only trivial locals */
    using Front = const char *(*)(State &, const void *input);
    static Result<std::unique_ptr<Program>> assemble(Place place, std::span<const Symbol> symbols, unsigned level,
                                                     Front front, const void *input);

private:
    explicit Program(Place place);
    std::unique_ptr<State> state_;
};

} // namespace xpp::mir
