#pragma once
/* The model's native code (W821): one module's IrModule (expr_ir.h) compiled by one of two compilers behind this interface. TinyCC
   (xpp_tcc.h) compiles it at once, from C text, and is the first tier a run starts on; MIR (xpp_mir.h) generates better code, for the
   time it takes to compile, and takes over where a run has work enough for it (model_native.cpp chooses when). Both give the same
   bits. Nothing above the IR knows which compiler made a Code, except through compiler(). */
#include "expr_ir.h"
#include "xpp_error.h"
#include "xpp_mir.h"
#include "xpp_tcc.h"
#include <memory>
#include <string>
#include <utility>
#include <variant>

namespace xpp::native {

enum class Compiler : unsigned char { TinyCC, MIR };

/* the compiled functions of one module, from one compiler; the addresses live until the Code is destroyed */
class Code {
public:
    /* the module's functions compiled by `compiler`; an error of ours or of the compiler's is the Error, at `place` */
    static Result<std::unique_ptr<Code>> compile(const IrModule &module, Compiler compiler, Place place);
    Compiler compiler() const { return compiler_; }
    /* a module function's code */
    Result<void *> function(const std::string &name) const;

private:
    Code(Compiler compiler, std::variant<std::unique_ptr<mir::Program>, std::unique_ptr<tcc::Program>> program)
        : compiler_(compiler), program_(std::move(program))
    {
    }
    Compiler compiler_;
    std::variant<std::unique_ptr<mir::Program>, std::unique_ptr<tcc::Program>> program_;
};

/* A call of a function of native code; every call into it goes through here: clang's -fsanitize=function (the sanitizer builds)
   reads the 8 bytes in front of a callee for its type signature, which generated code does not have, and in front of a function
   that opens MIR's code region on Windows those bytes are not mapped (CI 43362785265: an access violation in each call); here the
   check is off, everywhere else it stays. */
template <typename Result, typename... Parameters, typename... Arguments>
#if defined(__clang__)
__attribute__((no_sanitize("function")))
#endif
inline Result call(Result (*function)(Parameters...), Arguments &&...arguments)
{
    return function(std::forward<Arguments>(arguments)...);
}

} // namespace xpp::native
