/* W706: the C text through c2mir, MIR's C front end (the only file of the adapter that links it) */
#include "xpp_mir_state.h"
#include <cstdio>
extern "C" {
#include "c2mir/c2mir.h"
}

namespace xpp::mir {

namespace {
constexpr size_t MESSAGE_MAX = 512; /* what one diagnostic line is cut to */

/* c2mir reads its text a character at a time */
struct Text {
    const char *at;
};
int next_char(void *data)
{
    auto &text = *static_cast<Text *>(data);
    return *text.at ? static_cast<unsigned char>(*text.at++) : EOF;
}

const char *front(Program::State &state, const void *input)
{
    c2mir_init(state.context);
    state.finish_front = c2mir_finish;
    c2mir_options options{};
    options.ignore_warnings_p = 1;
    options.no_prepro_p = 1; /* generated text has no directive: #include reads no file */
    /* c2mir counts an error only when it has a stream to print it to: a null
       message_file makes a syntax error a success. The stream is a temporary
       file, its text read back into the failure; stderr when none can be made. */
    FILE *messages = std::tmpfile();
    options.message_file = messages ? messages : stderr;
    Text text{static_cast<const char *>(input)};
    const int compiled = c2mir_compile(state.context, &options, next_char, &text, "model.c", nullptr);
    if (messages) {
        std::rewind(messages);
        char line[MESSAGE_MAX];
        while (std::fgets(line, sizeof line, messages)) state.message += line;
        std::fclose(messages);
    }
    if (!compiled) return "cannot compile source";
    state.module = DLIST_TAIL(MIR_module_t, *MIR_get_module_list(state.context));
    return nullptr;
}
} // namespace

Result<std::unique_ptr<Program>> Program::compile(const std::string &source, std::span<const Symbol> symbols,
                                                  Place place, unsigned level)
{
    return assemble(std::move(place), symbols, level, front, source.c_str());
}

} // namespace xpp::mir
