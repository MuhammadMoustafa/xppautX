#pragma once
/* The model's native code compiled by TinyCC (W821, the first tier: it compiles a module in milliseconds, so a run starts on
   its code at once): the IrModule (expr_ir.h) lowered to C text by xpp_tcc_lower.cpp, compiled in memory by libtcc
   (third_party/tinycc, LGPL-2.1) here. The C names only what the lowering generated, and reads every number from a table
   the driver fills, so no text of the model reaches the compiler. */
#include "expr_ir.h"
#include "xpp_error.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace xpp::tcc {

/* One external function the C text calls: the name the text gives it and its address (TinyCC's symbol table). */
struct Symbol {
    std::string name;
    const void *address;
};

/* The C text of a module with what it needs from the driver (xpp_tcc_lower.cpp's lower). */
struct Source {
    std::string text;
    std::vector<double> numbers;                       /* the table N the text reads (index 0 is the zero divisor) */
    std::vector<Symbol> symbols;                       /* the external functions the text calls */
    std::vector<std::pair<std::string, std::string>> functions; /* each module function: its name, then its name in the text */
};

/* The C text of `module`, or the error of a module the lowering cannot write (a call of a function it lacks, a tree too deep). */
Result<Source> lower(const IrModule &module, const Place &place);

/* Only trusted code belongs here: the text is ours, but TinyCC is not a sandbox. The returned addresses live until the
   Program is destroyed; callers cast them to the function's signature (IrFunction::Kind). */
class Program {
public:
    /* the module's functions compiled; an error of ours or TinyCC's is the Error, at `place` */
    static Result<std::unique_ptr<Program>> compile(const IrModule &module, Place place);
    ~Program();
    Program(const Program &) = delete;
    Program &operator=(const Program &) = delete;
    /* a module function's code */
    Result<void *> function(const std::string &name) const;

    struct State; /* xpp_tcc.cpp's */

private:
    explicit Program(Place place);
    std::unique_ptr<State> state_;
};

} // namespace xpp::tcc
