/* The session resource's registry rows (W528): saving, importing settings, exporting the simulation information, the model's source, quitting and the state.
   A handler calls the core function the File menu's items call after their questions (xpp_session.h, lunch-new.h); the three the front end answers itself go through front_end. */
#include "command_def.h"
#include "export_model.h"
#include "lunch-new.h"
#include "recx.h"
#include "session.h"
#include "snapx.h"
#include "xpp_path.h"
#include "xpp_text.h"
#include "xpp_session.h"
#include "model.h"
#include "event_registry.h"
#include "command_category.h"
#include "xpp_error.h"
#include <array>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace xpp::command {
namespace {

constexpr const char *FILE_KEY = "file";
constexpr const char *DATA_KEY = "data";
constexpr const char *LANGUAGE_KEY = "language";

/* the data table goes in (true), is left out (false), or goes in unless it is above 50 MB, when the user is asked (left out) */
int data_choice(const Arguments &args)
{
    const std::optional<bool> data = args.optional<bool>(DATA_KEY);
    return data ? (*data ? 1 : 0) : -1;
}

Result<std::string> saved_result(const Session &s, bool saved)
{
    return ResultObject().text(FILE_KEY, s.saved_session->file).boolean("saved", saved).done();
}

Result<std::string> save(Session &s, const Command &, const Arguments &args, const Place &)
{
    return saved_result(s, xpp_session_save_here(s, data_choice(args)) != 0);
}

Result<std::string> save_as(Session &s, const Command &, const Arguments &args, const Place &)
{
    const bool saved = xpp_session_save(s, args.get<std::string>(FILE_KEY).c_str(), data_choice(args), false, false) != 0;
    return saved_result(s, saved);
}

Result<std::string> save_copy(Session &s, const Command &, const Arguments &args, const Place &)
{
    const std::string &file = args.get<std::string>(FILE_KEY);
    const bool copied = xpp_session_save(s, file.c_str(), data_choice(args), true, false) != 0;
    return ResultObject().text(FILE_KEY, snapx::session_file_name(file)).boolean("copied", copied).done();
}

Result<std::string> import_xppaut_state_file(Session &s, const Command &, const Arguments &args, const Place &)
{
    const std::string &file = args.get<std::string>(FILE_KEY);
    const Result<std::vector<std::string>> changes = import_xppaut_state(s, file, true);
    if (!changes) return std::unexpected<Error>(changes.error());
    std::string changed = "[";
    for (const std::string &what : *changes) {
        if (changed.size() > 1) changed += ',';
        json_append_string(changed, what);
    }
    return ResultObject().text(FILE_KEY, file).boolean("applied", true).raw("changes", changed + ']').done();
}

Result<std::string> export_info(Session &s, const Command &, const Arguments &args, const Place &)
{
    const std::string &file = args.get<std::string>(FILE_KEY);
    const Result<bool> written = write_info(s, file);
    if (!written) return std::unexpected<Error>(written.error());
    return ResultObject().text(FILE_KEY, file).boolean("written", *written).done();
}

/* session.file_exists: whether a file of the model's folder, or the folder a window granted (W774), is there now: the page asks before a several-file export */
Result<std::string> file_exists(Session &s, const Command &, const Arguments &args, const Place &)
{
    const std::string &file = args.get<std::string>(FILE_KEY);
    return ResultObject().text(FILE_KEY, file).boolean("exists", s.model().fs->exists(file)).done();
}

Result<std::string> export_model_as(Session &s, const Command &, const Arguments &args, const Place &)
{
    const Result<std::string> written = export_model(s, *language_named(args.get<std::string>(LANGUAGE_KEY)), args.optional<std::string>(FILE_KEY).value_or(std::string{}));
    if (!written) return std::unexpected<Error>(written.error());
    return ResultObject().text(FILE_KEY, *written).done();
}

constexpr Argument FILE_REQUIRED = {FILE_KEY, Type::file, true};
constexpr Argument DATA = {DATA_KEY, Type::boolean, false};
constexpr std::array SAVE_ARGUMENTS = {DATA};
constexpr std::array SAVE_AS_ARGUMENTS = {FILE_REQUIRED, DATA};
constexpr std::array IMPORT_ARGUMENTS = {FILE_REQUIRED};
constexpr std::array EXPORT_ARGUMENTS = {FILE_REQUIRED};
constexpr std::array FILE_EXISTS_ARGUMENTS = {FILE_REQUIRED};
constexpr auto LANGUAGE_CHOICES = c_names(language_names);
constexpr std::array MODEL_EXPORT_ARGUMENTS = {Argument{LANGUAGE_KEY, Type::choice, true, MAX_TEXT_BYTES, LANGUAGE_CHOICES}, Argument{FILE_KEY, Type::file, false}};
constexpr std::array QUIT_ARGUMENTS = {Argument{"save", Type::boolean, false}};

constexpr std::array SAVE_RESULTS = {ResultField{"file", "file"}, ResultField{"saved", "boolean"}};
constexpr std::array COPY_RESULTS = {ResultField{"file", "file"}, ResultField{"copied", "boolean"}};
constexpr std::array IMPORT_RESULTS = {ResultField{"file", "file"}, ResultField{"applied", "boolean"}, ResultField{"changes", "list[text]"}};
constexpr std::array EXPORT_RESULTS = {ResultField{"file", "file"}, ResultField{"written", "boolean"}};
constexpr std::array MODEL_EXPORT_RESULTS = {ResultField{"file", "file"}};
constexpr std::array FILE_EXISTS_RESULTS = {ResultField{"file", "file"}, ResultField{"exists", "boolean"}};
constexpr std::array SOURCE_RESULTS = {ResultField{"source", "text"}, ResultField{"equations", "list[text]"}};
constexpr std::array QUIT_RESULTS = {ResultField{"quitting", "boolean"}};
constexpr std::array STATE_RESULTS = {ResultField{"state", "the state event's members"}};
constexpr std::array SAVE_EVENTS = {xpp::events::SESSION_SAVED, xpp::events::SESSION_STATE, xpp::events::DONE};
constexpr std::array STATE_EVENTS = {xpp::events::SESSION_STATE, xpp::events::DONE};
constexpr std::array SOURCE_EVENTS = {xpp::events::SESSION_SOURCE, xpp::events::SESSION_EQUATIONS, xpp::events::DONE};
constexpr std::array DONE_EVENTS = {xpp::events::DONE};

constexpr std::array KEY_1 = {"Ctrl+S"};
constexpr std::array KEY_2 = {"Ctrl+Shift+S"};
constexpr std::array ROWS = {
    Command{"session.save", SAVE_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "Save session", "File",
            "Save the session to the file it was last saved to or opened from; data false leaves the data table out.", SAVE_RESULTS,
            SAVE_EVENTS, true, save, {}, KEY_1},
    Command{"session.save_as", SAVE_AS_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "Save session as", "File",
            "Save the session to a file of the model's folder (.snapx added).",
            SAVE_RESULTS, SAVE_EVENTS, true, save_as, {}, KEY_2, "write", "*.snapx"},
    Command{"session.save_copy", SAVE_AS_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "Save a copy", "File",
            "Save a copy of the session, leaving the file it is saved to as it was.", COPY_RESULTS, DONE_EVENTS, true, save_copy, {}, {}, "write", "*.snapx"},
    Command{"session.export_info", EXPORT_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "Simulation information", "File/Export",
            "Write the equations, numerics, initial conditions and parameters to a file.", EXPORT_RESULTS, DONE_EVENTS, true, export_info, {}, {}, "write", "*.txt", "", "Export"},
    Command{"model.export", MODEL_EXPORT_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "Model as", "File/Export",
            "Write the model, with the values it has now (parameters, initial data, the integration settings), as a program in a language that runs with no xppautX and writes the rows of a run to a CSV file beside itself (python: the standard library; python_numpy: numpy arrays, an array line of the model one slice; python_numba: python's statements compiled by numba); "
            "left out, file is asked for (the result's file is empty when that question was cancelled). A model the language cannot say (tables, delays, events, Markov variables, networks, ...) is refused, naming what and its line.",
            MODEL_EXPORT_RESULTS, DONE_EVENTS, true, export_model_as, {}, {}, "write", "*", "", "Export", Feature::none, {}, LANGUAGE_KEY},
    Command{"session.file_exists", FILE_EXISTS_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Files, "File exists", "",
            "Whether a file is there now: a base name of the model's folder, or a full path in the folder a file dialog granted; the page asks before it writes several files.",
            FILE_EXISTS_RESULTS, DONE_EVENTS, false, file_exists},
    Command{"session.import_xppaut_state", IMPORT_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Files, "Import XPPAUT session state", "File/Sets",
            "Apply a whole set file XPPAUT wrote (its parameters, initial conditions, numerics, delays, boundary conditions, active graph and run position), all or nothing; the result names what it changed. Nothing is written beside it.",
            IMPORT_RESULTS, STATE_EVENTS, true, import_xppaut_state_file, {}, {}, "read", "*.set"},
    Command{"session.quit", QUIT_ARGUMENTS, XPP_KIND_CONTROL, CommandCategory::Files, "Quit", "File",
            "Quit: save true saves the session (and a recording in progress) first, false quits at once, and left out asks.",
            QUIT_RESULTS, DONE_EVENTS, false, front_end},
    Command{"session.get_source", {}, XPP_KIND_VIEW, CommandCategory::Files, "Model source", "Windows",
            "The model's source, its active comments and its equations.", SOURCE_RESULTS, SOURCE_EVENTS, true, front_end},
    Command{"session.get_state", {}, XPP_KIND_VIEW, CommandCategory::Files, "Get session state", "",
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
