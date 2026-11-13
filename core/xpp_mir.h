#pragma once

#include "expr_ir.h"
#include "xpp_error.h"
#include <memory>
#include <string>

namespace xpp::mir {

/* The model's native code (W714): the Generator's IrModule (expr_ir.h) compiled in process by MIR (third_party/mir,
   MIT) through its API. MIR is not a sandbox: only trusted code belongs here, which is why the input is the
   Generator's trees and never text: every name an index, every number a double, every call a row of the
   module's callee table, resolved only through it. The returned addresses live until the Program is destroyed;
   callers cast them to the function's signature (IrFunction::Kind). Not thread-safe (MIR's error function has no
   user data): compile on the core thread, one at a time. */
class Program {
public:
    /* the module's functions compiled, each at the optimization level of its kind; an error of ours or MIR's is
       the Error, at `place` */
    static Result<std::unique_ptr<Program>> compile(const IrModule &module, Place place);
    ~Program();
    Program(const Program &) = delete;
    Program &operator=(const Program &) = delete;
    /* a module function's code */
    Result<void *> function(const std::string &name) const;

    struct State; /* xpp_mir.cpp's; public only for its compile's own helpers */

private:
    explicit Program(Place place);
    std::unique_ptr<State> state_;
};

} // namespace xpp::mir
