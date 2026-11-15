/* The model resource's registry rows (W528): opening a model, a session file and reloading. They are the application's own
   (W737): a load replaces the Session and the program's model switch (model_switch.h) carries it out, which the commands
   module, below it, does not include; the rows join the registry in command_registry.cpp's table, in their place. */
#include "command_def.h"
#include "model_switch.h"
#include "model.h"
#include "recx.h"
#include "session.h"
#include "snapx.h"
#include "xpp_path.h"
#include "xpp_text.h"
#include "event_registry.h"
#include "command_category.h"
#include "xpp_error.h"
#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace xpp::command {
namespace {

constexpr const char *FILE_KEY = "file";
constexpr Argument FILE_REQUIRED = {FILE_KEY, Type::file, true};

Result<std::string> open_model(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    Result<bool> asked = false;
    if (const std::optional<std::string> example = args.optional<std::string>("example")) {
        asked = xpp_model_open_example(s, *example);
    } else if (!args.has(FILE_KEY) && !args.has("recent")) {
        asked = xpp_model_open(s, nullptr);
    } else {
        std::string path;
        if (const std::optional<std::string> file = args.optional<std::string>(FILE_KEY)) path = *file;
        else {
            const long long index = args.get<long long>("recent");
            const Result<std::vector<std::string>> recent = recent::load_recent(*s.model().fs);
            if (!recent) return std::unexpected<Error>(recent.error());
            if (static_cast<std::size_t>(index) >= recent->size())
                return fail(command.name, xpp::format("There is no recent model {}: {} are kept", index, recent->size()), place);
            path = (*recent)[static_cast<std::size_t>(index)];
        }
        if (files::has_extension(path, recx::extension))
            return fail(command.name, xpp::format("\"{}\" is a recording: open it with recording.open", path), place);
        asked = xpp_model_open(s, path.c_str());
    }
    if (!asked) return std::unexpected<Error>(asked.error());
    if (!*asked) return std::string("{\"cancelled\":true}");
    return std::string("{}");
}

Result<std::string> reload_model(Session &s, const Command &, const Arguments &, const Place &)
{
    const Result<bool> reloaded = xpp_model_reload(s);
    if (!reloaded) return std::unexpected<Error>(reloaded.error());
    return ResultObject().boolean("reloaded", *reloaded).done();
}

Result<std::string> open_session(Session &s, const Command &, const Arguments &args, const Place &)
{
    const std::string &file = args.get<std::string>(FILE_KEY);
    const Result<bool> loaded = xpp_model_open(s, snapx::session_file_name(file).c_str());
    if (!loaded) return std::unexpected<Error>(loaded.error());
    return ResultObject().text(FILE_KEY, file).boolean("loaded", *loaded).done();
}

constexpr std::array OPEN_MODEL_ARGUMENTS = {
    Argument{FILE_KEY, Type::path, false}, Argument{"example", Type::text, false},
    Argument{"recent", Type::integer, false, MAX_TEXT_BYTES, {}, constant(0)}};
constexpr std::array OPEN_MODEL_ONE_OF = {FILE_KEY, "example", "recent"};
constexpr std::array OPEN_MODEL_GROUPS = {Group{GroupRule::at_most_one, OPEN_MODEL_ONE_OF}};
constexpr std::array OPEN_ARGUMENTS = {FILE_REQUIRED};
constexpr std::array OPEN_MODEL_RESULTS = {ResultField{"model", "text"}, ResultField{"variables", "list[name]"},
                                           ResultField{"parameters", "list[name]"}};
constexpr std::array RELOAD_RESULTS = {ResultField{"reloaded", "boolean"}};
constexpr std::array OPEN_RESULTS = {ResultField{"file", "file"}, ResultField{"loaded", "boolean"}};
constexpr std::array LOAD_EVENTS = {xpp::events::HELLO, xpp::events::SESSION_STATE, xpp::events::DONE};

constexpr std::array KEY_0 = {"Ctrl+O"};
constexpr std::array ROWS = {
    Command{"session.open_model", OPEN_MODEL_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "Open model", "File",
            "Open a model: a model file by its path (--server's own input only), a bundled example by its name, or one of the "
            "recent models by its place in hello.start.recent. Asks first whether to save this session.",
            OPEN_MODEL_RESULTS, LOAD_EVENTS, true, open_model, OPEN_MODEL_GROUPS, KEY_0},
    Command{"session.reload_model", {}, XPP_KIND_DATA, CommandCategory::Files, "Reload model", "File",
            "Load the model's file again; the parameters, initial data, numerics and autoX settings keep their values by name.",
            RELOAD_RESULTS, LOAD_EVENTS, true, reload_model},
    Command{"session.open", OPEN_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "Open session", "File",
            "Open a session file (.snapx): its model, then everything as it was saved.", OPEN_RESULTS, LOAD_EVENTS, true, open_session, {}, {}, "read", "*.snapx"}};

} // namespace

std::span<const Command> model_commands() { return ROWS; }

} // namespace xpp::command
