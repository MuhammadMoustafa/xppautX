/* The two compilers behind native_code.h: each module goes to the one named, and its functions are looked up in the one that holds them. */
#include "native_code.h"

namespace xpp::native {

Result<std::unique_ptr<Code>> Code::compile(const IrModule &module, Compiler compiler, Place place)
{
    if (compiler == Compiler::TinyCC) {
        auto program = tcc::Program::compile(module, std::move(place));
        if (!program) return std::unexpected(program.error());
        return std::unique_ptr<Code>(new Code(compiler, std::move(*program)));
    }
    auto program = mir::Program::compile(module, std::move(place));
    if (!program) return std::unexpected(program.error());
    return std::unique_ptr<Code>(new Code(compiler, std::move(*program)));
}

Result<void *> Code::function(const std::string &name) const
{
    return std::visit([&name](const auto &program) { return program->function(name); }, program_);
}

} // namespace xpp::native
