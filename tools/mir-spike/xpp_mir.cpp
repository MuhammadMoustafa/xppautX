#include "xpp_mir.h"
#include <csetjmp>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
extern "C" {
#include "mir.h"
#include "mir-gen.h"
#include "c2mir/c2mir.h"
}

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

/* c2mir reads its text a character at a time */
struct Text {
    const char *at;
};
int next_char(void *data)
{
    auto &text = *static_cast<Text *>(data);
    return *text.at ? static_cast<unsigned char>(*text.at++) : EOF;
}

/* the supplied symbols, by name, for MIR_link's import resolver */
const std::map<std::string, const void *> *imports = nullptr;
void *resolve(const char *name)
{
    const auto it = imports->find(name);
    return it == imports->end() ? nullptr : const_cast<void *>(it->second);
}
} // namespace

struct Program::State {
    MIR_context_t context = nullptr;
    MIR_module_t module = nullptr;
    bool generator = false, c_front_end = false;
    std::string message;
    Place place;

    ~State()
    {
        if (!context) return;
        if (generator) MIR_gen_finish(context);
        if (c_front_end) c2mir_finish(context);
        MIR_finish(context);
    }
    std::unexpected<Error> failure(const std::string &operation) const
    {
        return fail("MIR", message.empty() ? operation : message, place);
    }
};

Program::Program(Place place) : state_(std::make_unique<State>())
{
    state_->place = std::move(place);
}

Program::~Program() = default;

namespace {
/* The calls into MIR, in a frame of their own with only trivial locals: a
   jump out of MIR (`raised`) never skips a destructor. Returns the text of
   what failed, or nullptr. */
const char *build(Program::State &state, const char *source, unsigned level)
{
    if (setjmp(error_exit)) return error_text;
    state.context = MIR_init();
    MIR_set_error_func(state.context, raised);
    c2mir_init(state.context);
    state.c_front_end = true;
    c2mir_options options{};
    options.ignore_warnings_p = 1;
    options.no_prepro_p = 1; /* generated text has no directive: #include reads no file */
    /* c2mir counts an error only when it has a stream to print it to: a null
       message_file makes a syntax error a success. The stream is a temporary
       file, its text read back into the failure; stderr when none can be made. */
    FILE *messages = std::tmpfile();
    options.message_file = messages ? messages : stderr;
    Text text{source};
    const int compiled = c2mir_compile(state.context, &options, next_char, &text, "model.c", nullptr);
    if (messages) {
        std::rewind(messages);
        char line[MESSAGE_MAX];
        while (std::fgets(line, sizeof line, messages)) state.message += line;
        std::fclose(messages);
    }
    if (!compiled) return "cannot compile source";
    state.module = DLIST_TAIL(MIR_module_t, *MIR_get_module_list(state.context));
    MIR_load_module(state.context, state.module);
    MIR_gen_init(state.context);
    state.generator = true;
    MIR_gen_set_optimize_level(state.context, level);
    MIR_link(state.context, MIR_set_gen_interface, resolve);
    return nullptr;
}
} // namespace

Result<std::unique_ptr<Program>> Program::compile(const std::string &source,
                                                std::span<const Symbol> symbols,
                                                Place place, unsigned level)
{
    auto program = std::unique_ptr<Program>(new Program(std::move(place)));
    auto &state = *program->state_;
    std::map<std::string, const void *> table;
    for (const auto &symbol : symbols) table.emplace(symbol.name, symbol.address);
    imports = &table;
    const char *failed = build(state, source.c_str(), level);
    imports = nullptr;
    if (failed) {
        if (state.message.empty()) state.message = failed;
        state.context = nullptr; /* a context MIR jumped out of is not finished: it is left, a leak on the error path */
        return state.failure("MIR failed");
    }
    return program;
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
