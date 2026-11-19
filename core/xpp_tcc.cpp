/* The TinyCC driver (W821): lowers the module to C (xpp_tcc_lower.cpp), gives TinyCC the external functions as symbols, compiles the
   text in memory and relocates it once; the number table is filled after relocation, since the text's data is only then placed. */
#include "xpp_tcc.h"
#include "../third_party/tinycc/libtcc.h"
#include "xpp_mem.h"
#include "native_functions.h"
#include <cstring>
#include <new>
#include <unordered_map>

namespace xpp::tcc {

struct Program::State {
    std::unique_ptr<TCCState, decltype(&tcc_delete)> compiler{tcc_new(), tcc_delete};
    std::unordered_map<std::string, void *> functions; /* the module's functions by their IR names */
    std::string message;
    Place place;

    static void diagnostic(void *opaque, const char *text) noexcept
    {
        try {
            auto &state = *static_cast<State *>(opaque);
            if (!state.message.empty()) state.message += '\n';
            state.message += text;
        } catch (const std::bad_alloc &) {
            out_of_memory("saving a TinyCC diagnostic");
        }
    }

    std::unexpected<Error> failure(const std::string &operation) const
    {
        return fail("TinyCC", message.empty() ? operation : message, place);
    }
};

Program::Program(Place place) : state_(std::make_unique<State>())
{
    state_->place = std::move(place);
    if (state_->compiler) tcc_set_error_func(state_->compiler.get(), state_.get(), State::diagnostic);
}

Program::~Program() = default;

Result<std::unique_ptr<Program>> Program::compile(const IrModule &module, Place place)
{
    auto source = lower(module, place);
    if (!source) return std::unexpected(source.error());
    auto program = std::unique_ptr<Program>(new Program(std::move(place)));
    auto &state = *program->state_;
    auto *compiler = state.compiler.get();
    if (!compiler) return state.failure("cannot create a compiler state");
    /* No standard includes, runtime libraries or default library search: the text declares what it uses. Set before the
       output type: that is when TinyCC initializes its paths. Warnings are off: the text is generated, and a warning
       would only hide the error that matters. */
    if (tcc_set_options(compiler, "-nostdlib -nostdinc -w -Wl,-nostdlib") < 0 || tcc_set_output_type(compiler, TCC_OUTPUT_MEMORY) < 0)
        return state.failure("cannot configure memory compilation");
    for (const Symbol &symbol : source->symbols) {
        if (tcc_add_symbol(compiler, symbol.name.c_str(), symbol.address) < 0) return state.failure("cannot add symbol " + symbol.name);
    }
    if (tcc_compile_string(compiler, source->text.c_str()) < 0) return state.failure("cannot compile source");
    /* Relocated exactly once, here: TinyCC exits on a second attempt. */
    if (tcc_relocate(compiler) < 0) return state.failure("cannot relocate compiled code");
    void *table = tcc_get_symbol(compiler, "N");
    if (!table) return state.failure("the number table is missing");
    std::memcpy(table, source->numbers.data(), source->numbers.size() * sizeof(double));
    for (const auto &[name, symbol] : source->functions) {
        void *address = tcc_get_symbol(compiler, symbol.c_str());
        if (!address) return state.failure("function not found: " + name);
        state.functions.emplace(name, address);
    }
    return program;
}

Result<void *> Program::function(const std::string &name) const
{
    return native::function_at(state_->functions, name, state_->place, "TinyCC");
}

} // namespace xpp::tcc
