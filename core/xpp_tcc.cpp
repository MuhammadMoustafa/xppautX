#include "xpp_tcc.h"
#include "xpp_mem.h"
#include "../third_party/tinycc/libtcc.h"
#include <new>

namespace xpp::tcc {

struct Program::State {
    std::unique_ptr<TCCState, decltype(&tcc_delete)> compiler{tcc_new(), tcc_delete};
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
    if (state_->compiler)
        tcc_set_error_func(state_->compiler.get(), state_.get(), State::diagnostic);
}

Program::~Program() = default;

Result<std::unique_ptr<Program>> Program::compile(const std::string &source,
                                                std::span<const Symbol> symbols,
                                                Place place)
{
    auto program = std::unique_ptr<Program>(new Program(std::move(place)));
    auto &state = *program->state_;
    auto *compiler = state.compiler.get();
    if (!compiler) return state.failure("cannot create a compiler state");
    /* No standard includes, runtime libraries or default library search.
       Set before output type: that is when TinyCC initializes its paths. */
    if (tcc_set_options(compiler, "-nostdlib -nostdinc -Wl,-nostdlib") < 0 ||
        tcc_set_output_type(compiler, TCC_OUTPUT_MEMORY) < 0)
        return state.failure("cannot configure memory compilation");
    for (const auto &symbol : symbols) {
        if (tcc_add_symbol(compiler, symbol.name, symbol.address) < 0)
            return state.failure("cannot add symbol " + std::string(symbol.name));
    }
    if (tcc_compile_string(compiler, source.c_str()) < 0)
        return state.failure("cannot compile source");
    /* Relocated exactly once, here: TinyCC exits on a second attempt. */
    if (tcc_relocate(compiler) < 0)
        return state.failure("cannot relocate compiled code");
    return program;
}

Result<void *> Program::function(const std::string &name) const
{
    void *address = tcc_get_symbol(state_->compiler.get(), name.c_str());
    if (!address) return fail("TinyCC", "function not found: " + name, state_->place);
    return address;
}

} // namespace xpp::tcc
