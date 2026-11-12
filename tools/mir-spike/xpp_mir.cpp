#include "xpp_mir.h"
#include "xpp_mir_state.h"
#include <csetjmp>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <map>

namespace xpp::mir {

namespace {
/* MIR reports an error through a function that must not return and has no
   user data: it records the text and jumps back to the compile that called
   MIR (one compile at a time; the spike is single-threaded). */
constexpr size_t MESSAGE_MAX = 512; /* what one MIR diagnostic is cut to */
std::jmp_buf error_exit;
char error_text[MESSAGE_MAX];

void MIR_NO_RETURN raised(MIR_error_type_t, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    std::vsnprintf(error_text, sizeof error_text, format, args);
    va_end(args);
    std::longjmp(error_exit, 1);
}

/* the supplied symbols, by name, for MIR_link's import resolver */
const std::map<std::string, const void *> *imports = nullptr;
void *resolve(const char *name)
{
    const auto it = imports->find(name);
    return it == imports->end() ? nullptr : const_cast<void *>(it->second);
}

/* The calls into MIR, in a frame of their own with only trivial locals: a
   jump out of MIR (`raised`) never skips a destructor. Returns the text of
   what failed, or nullptr. */
const char *build(Program::State &state, unsigned level, Program::Front front, const void *input)
{
    if (setjmp(error_exit)) return error_text;
    const auto start = std::chrono::steady_clock::now();
    state.context = MIR_init();
    MIR_set_error_func(state.context, raised);
    if (const char *failed = front(state, input)) return failed;
    const auto fronted = std::chrono::steady_clock::now();
    MIR_load_module(state.context, state.module);
    MIR_gen_init(state.context);
    state.generator = true;
    MIR_gen_set_optimize_level(state.context, level);
    MIR_link(state.context, MIR_set_gen_interface, resolve);
    const auto done = std::chrono::steady_clock::now();
    state.front_ms = std::chrono::duration<double, std::milli>(fronted - start).count();
    state.generate_ms = std::chrono::duration<double, std::milli>(done - fronted).count();
    return nullptr;
}
} // namespace

Program::State::~State()
{
    if (!context) return;
    if (generator) MIR_gen_finish(context);
    if (finish_front) finish_front(context);
    MIR_finish(context);
}

std::unexpected<Error> Program::State::failure(const std::string &operation) const
{
    return fail("MIR", message.empty() ? operation : message, place);
}

Program::Program(Place place) : state_(std::make_unique<State>())
{
    state_->place = std::move(place);
}

Program::~Program() = default;

Result<std::unique_ptr<Program>> Program::assemble(Place place, std::span<const Symbol> symbols, unsigned level,
                                                   Front front, const void *input)
{
    auto program = std::unique_ptr<Program>(new Program(std::move(place)));
    auto &state = *program->state_;
    std::map<std::string, const void *> table;
    for (const auto &symbol : symbols) table.emplace(symbol.name, symbol.address);
    imports = &table;
    const char *failed = build(state, level, front, input);
    imports = nullptr;
    if (failed) {
        if (state.message.empty()) state.message = failed;
        state.context = nullptr; /* a context MIR jumped out of is not finished: it is left, a leak on the error path */
        return state.failure("MIR failed");
    }
    return program;
}

Program::Timing Program::timing() const
{
    return {state_->front_ms, state_->generate_ms};
}

Result<void *> Program::function(const std::string &name) const
{
    for (MIR_item_t item = DLIST_HEAD(MIR_item_t, state_->module->items); item;
         item = DLIST_NEXT(MIR_item_t, item)) {
        const char *item_name = nullptr;
        switch (item->item_type) {
        case MIR_func_item: item_name = item->u.func->name; break;
        case MIR_bss_item: item_name = item->u.bss->name; break;
        case MIR_data_item: item_name = item->u.data->name; break;
        default: break;
        }
        if (item_name && name == item_name) return item->addr;
    }
    return fail("MIR", "function not found: " + name, state_->place);
}

} // namespace xpp::mir
