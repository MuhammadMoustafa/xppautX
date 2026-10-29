/* The session resource's registry rows (W528): opening a model, a session file or a recording's model, reloading,
   saving, importing settings, exporting the simulation information, the model's source, quitting and the state.
   A handler calls the core function the File menu's items call after their questions (model_switch.h,
   xpp_session.h, lunch-new.h); the three the front end answers itself go through front_end. */
#include "command_def.h"
#include "json_error.h"
#include "lunch-new.h"
#include "model_files.h"
#include "model_switch.h"
#include "recx.h"
#include "session.h"
#include "snapx.h"
#include "xpp_files.h"
#include "xpp_recent.h"
#include "xpp_session.h"
#include <array>

namespace xpp::command {
namespace {

constexpr const char *FILE_KEY = "file";
constexpr const char *DATA_KEY = "data";
constexpr const char *CONFIRM_KEY = "confirm";

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
            const Result<std::vector<std::string>> recent = recent::load_recent();
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

/* the data table goes in (true), is left out (false), or goes in unless it is above 50 MB, when the user is asked (left out) */
int data_choice(const Arguments &args)
{
    const std::optional<bool> data = args.optional<bool>(DATA_KEY);
    return data ? (*data ? 1 : 0) : -1;
}

Result<std::string> saved_result(const Session &s, bool saved)
{
    return ResultObject().text(FILE_KEY, s.saved_session.file).boolean("saved", saved).done();
}

Result<std::string> save(Session &s, const Command &, const Arguments &args, const Place &)
{
    return saved_result(s, xpp_session_save_here(s, data_choice(args)) != 0);
}

Result<std::string> save_as(Session &s, const Command &, const Arguments &args, const Place &)
{
    const bool saved = xpp_session_save(s, args.get<std::string>(FILE_KEY).c_str(), data_choice(args), false,
                                        args.optional<bool>(CONFIRM_KEY).value_or(false)) != 0;
    return saved_result(s, saved);
}

Result<std::string> save_copy(Session &s, const Command &, const Arguments &args, const Place &)
{
    const std::string &file = args.get<std::string>(FILE_KEY);
    const bool copied = xpp_session_save(s, file.c_str(), data_choice(args), true, args.optional<bool>(CONFIRM_KEY).value_or(false)) != 0;
    return ResultObject().text(FILE_KEY, snapx::session_file_name(file)).boolean("copied", copied).done();
}

Result<std::string> import_settings_file(Session &s, const Command &, const Arguments &args, const Place &)
{
    const std::string &file = args.get<std::string>(FILE_KEY);
    if (const Result<> applied = import_settings(s, file, true); !applied) return std::unexpected<Error>(applied.error());
    return ResultObject().text(FILE_KEY, file).boolean("applied", true).done();
}

Result<std::string> export_info(Session &s, const Command &, const Arguments &args, const Place &)
{
    const std::string &file = args.get<std::string>(FILE_KEY);
    const Result<bool> written = write_info(s, file, args.optional<bool>(CONFIRM_KEY).value_or(false));
    if (!written) return std::unexpected<Error>(written.error());
    return ResultObject().text(FILE_KEY, file).boolean("written", *written).done();
}

constexpr Argument FILE_REQUIRED = {FILE_KEY, Type::file, true};
constexpr Argument DATA = {DATA_KEY, Type::boolean, false};
constexpr Argument CONFIRM = {CONFIRM_KEY, Type::boolean, false};
constexpr std::array OPEN_MODEL_ARGUMENTS = {
    Argument{FILE_KEY, Type::path, false}, Argument{"example", Type::text, false},
    Argument{"recent", Type::integer, false, MAX_TEXT_BYTES, {}, constant(0)}};
constexpr std::array OPEN_MODEL_ONE_OF = {FILE_KEY, "example", "recent"};
constexpr std::array OPEN_MODEL_GROUPS = {Group{GroupRule::at_most_one, OPEN_MODEL_ONE_OF}};
constexpr std::array OPEN_ARGUMENTS = {FILE_REQUIRED};
constexpr std::array SAVE_ARGUMENTS = {DATA};
constexpr std::array SAVE_AS_ARGUMENTS = {FILE_REQUIRED, DATA, CONFIRM};
constexpr std::array IMPORT_ARGUMENTS = {FILE_REQUIRED};
constexpr std::array EXPORT_ARGUMENTS = {FILE_REQUIRED, CONFIRM};
constexpr std::array QUIT_ARGUMENTS = {Argument{"save", Type::boolean, false}};

constexpr std::array OPEN_MODEL_RESULTS = {ResultField{"model", "text"}, ResultField{"variables", "list[name]"},
                                           ResultField{"parameters", "list[name]"}};
constexpr std::array RELOAD_RESULTS = {ResultField{"reloaded", "boolean"}};
constexpr std::array OPEN_RESULTS = {ResultField{"file", "file"}, ResultField{"loaded", "boolean"}};
constexpr std::array SAVE_RESULTS = {ResultField{"file", "file"}, ResultField{"saved", "boolean"}};
constexpr std::array COPY_RESULTS = {ResultField{"file", "file"}, ResultField{"copied", "boolean"}};
constexpr std::array IMPORT_RESULTS = {ResultField{"file", "file"}, ResultField{"applied", "boolean"}};
constexpr std::array EXPORT_RESULTS = {ResultField{"file", "file"}, ResultField{"written", "boolean"}};
constexpr std::array SOURCE_RESULTS = {ResultField{"source", "text"}, ResultField{"equations", "list[text]"}};
constexpr std::array QUIT_RESULTS = {ResultField{"quitting", "boolean"}};
constexpr std::array STATE_RESULTS = {ResultField{"state", "the state event's members"}};
constexpr std::array LOAD_EVENTS = {xpp::events::HELLO, xpp::events::SESSION_STATE, xpp::events::DONE};
constexpr std::array SAVE_EVENTS = {xpp::events::SESSION_SAVED, xpp::events::SESSION_STATE, xpp::events::DONE};
constexpr std::array STATE_EVENTS = {xpp::events::SESSION_STATE, xpp::events::DONE};
constexpr std::array SOURCE_EVENTS = {xpp::events::SESSION_SOURCE, xpp::events::SESSION_EQUATIONS, xpp::events::DONE};
constexpr std::array DONE_EVENTS = {xpp::events::DONE};

constexpr std::array KEY_0 = {"Ctrl+O"};
constexpr std::array KEY_1 = {"Ctrl+S"};
constexpr std::array KEY_2 = {"Ctrl+Shift+S"};
constexpr std::array ROWS = {
    Command{"session.open_model", OPEN_MODEL_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "File/Open model",
            "Open a model: a model file by its path (--server's own input only), a bundled example by its name, or one of the "
            "recent models by its place in hello.start.recent. Asks first whether to save this session.",
            OPEN_MODEL_RESULTS, LOAD_EVENTS, true, open_model, OPEN_MODEL_GROUPS, KEY_0},
    Command{"session.reload_model", {}, XPP_KIND_DATA, CommandCategory::Files, "File/Reload model",
            "Load the model's file again; the parameters, initial data, numerics and AUTO settings keep their values by name.",
            RELOAD_RESULTS, LOAD_EVENTS, true, reload_model},
    Command{"session.open", OPEN_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "File/Open session",
            "Open a session file (.snapx): its model, then everything as it was saved.", OPEN_RESULTS, LOAD_EVENTS, true, open_session, {}, {}, "read", "*.snapx"},
    Command{"session.save", SAVE_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "File/Save session",
            "Save the session to the file it was last saved to or opened from; data false leaves the data table out.", SAVE_RESULTS,
            SAVE_EVENTS, true, save, {}, KEY_1},
    Command{"session.save_as", SAVE_AS_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "File/Save session as",
            "Save the session to a file of the model's folder (.snapx added); confirm true replaces an existing file without asking.",
            SAVE_RESULTS, SAVE_EVENTS, true, save_as, {}, KEY_2, "write", "*.snapx"},
    Command{"session.save_copy", SAVE_AS_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "File/Save a copy of the session",
            "Save a copy of the session, leaving the file it is saved to as it was.", COPY_RESULTS, DONE_EVENTS, true, save_copy, {}, {}, "write", "*.snapx"},
    Command{"session.import_settings", IMPORT_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Files, "File/Import settings",
            "Apply a settings file (.setx, or XPPAUT's .set, converted beside it).", IMPORT_RESULTS, STATE_EVENTS, true, import_settings_file, {}, {}, "read", "*.setx *.set"},
    Command{"session.export_info", EXPORT_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "File/Export simulation information",
            "Write the equations, numerics, initial conditions and parameters to a file.", EXPORT_RESULTS, DONE_EVENTS, true, export_info, {}, {}, "write", "*.txt"},
    Command{"session.get_source", {}, XPP_KIND_VIEW, CommandCategory::Files, "File/Model source",
            "The model's source, its active comments and its equations.", SOURCE_RESULTS, SOURCE_EVENTS, true, front_end},
    Command{"session.quit", QUIT_ARGUMENTS, XPP_KIND_CONTROL, CommandCategory::Files, "File/Quit",
            "Quit: save true saves the session (and a recording in progress) first, false quits at once, and left out asks.",
            QUIT_RESULTS, DONE_EVENTS, false, front_end},
    Command{"session.get_state", {}, XPP_KIND_VIEW, CommandCategory::Files, "File/Session/Get state",
            "The session's state: sliders, parameters, initial conditions, the view, the saved session and the recording.",
            STATE_RESULTS, STATE_EVENTS, false, front_end}};

} // namespace

std::string opened_model_result(const Session &s)
{
    const Model &m = s.model();
    std::string variables = "[", parameters = "[";
    for (int i = 0; i < m.node + m.nmarkov; ++i) {
        if (i) variables += ',';
        json_append_string(variables, m.uvar_names[static_cast<std::size_t>(i)]);
    }
    for (int i = 0; i < m.nupar; ++i) {
        if (i) parameters += ',';
        json_append_string(parameters, m.upar_names[static_cast<std::size_t>(i)]);
    }
    return ResultObject().text("model", model_title(m)).raw("variables", variables + ']').raw("parameters", parameters + ']').done();
}

std::span<const Command> session_commands() { return ROWS; }

} // namespace xpp::command
