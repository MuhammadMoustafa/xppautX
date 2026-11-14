#pragma once

#include "xpp_error.h"
#include "expr_ir.h"
#include <memory>
#include <string>
#include <utility>
namespace xpp { struct IrModule; }

namespace xpp::mir {

/* The model's native code (W714): the Generator's IrModule (expr_ir.h) compiled in process by MIR (third_party/mir,
   MIT) through its API. MIR is not a sandbox: only trusted code belongs here, which is why the input is the
   Generator's trees and never text: every name an index, every number a double, every call a row of the
   module's callee table, resolved only through it. The returned addresses live until the Program is destroyed;
   callers cast them to the function's signature (IrFunction::Kind). A compile may run on any thread (the model's
   build thread, W715; the core thread for the exact Jacobian): its state is its own and compiles take turns on a
   mutex, since MIR's generator sorts a static table at each MIR_gen_init. */
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

/* A call of a function MIR compiled; every call into the native code goes through here. clang's
   -fsanitize=function (the sanitizer builds) reads the 8 bytes in front of a callee for its type signature,
   which generated code does not have, and in front of a function that opens MIR's code region on Windows those
   bytes are not mapped (CI 43362785265: an access violation in each call); here the check is off, everywhere
   else it stays. */
template <typename Result, typename... Parameters, typename... Arguments>
#if defined(__clang__)
__attribute__((no_sanitize("function")))
#endif
inline Result call(Result (*function)(Parameters...), Arguments &&...arguments)
{
    return function(std::forward<Arguments>(arguments)...);
}

} // namespace xpp::mir
