/* The set resource's registry rows (W526): apply one of the model's named sets, save the current values into
   the model file (a new set, a set's part, the defaults, another file), rename and delete a set, and restore the
   model's own values. Each handler gets its arguments validated and typed (command_def.h) and calls the same core
   function the File menu's picks do (load_eqn.h, model_save.h): one behaviour, whichever front end asks. The
   settings of a plot (scope plot, or all) are the graph `window` names, the active graph when it is left out; the
   values saved are the initial ones unless `state` says last. */
#include "command_def.h"
#include "command_graph.h"
#include "load_eqn.h"
#include "model_options.h"
#include "model_save.h"
#include "session.h"
#include "xpp_io.h"
#include <array>

namespace xpp::command {
namespace {

constexpr const char *WINDOW_KEY = "window", *SCOPE_KEY = "scope", *STATE_KEY = "state", *NAME_KEY = "name", *FILE_KEY = "file";

/* the scopes of a set, as the C strings a row's choices are: all, the parameters, the initial values, then the
   option groups a set may hold (set_scopes, load_eqn.h, lists the same) */
constexpr std::array<const char *, 3 + set_groups.size()> scope_choices()
{
    std::array<const char *, 3 + set_groups.size()> out{set_scope::all.data(), set_scope::parameters.data(), set_scope::initial.data()};
    for (std::size_t i = 0; i < set_groups.size(); ++i) out[3 + i] = set_groups[i].data();
    return out;
}
constexpr auto SCOPE_CHOICES = scope_choices();
/* the values a save writes: the initial ones, or the state the last run reached */
constexpr std::string_view STATE_INITIAL = "initial", STATE_LAST = "last";
constexpr std::array STATE_CHOICES = {STATE_INITIAL.data(), STATE_LAST.data()};

/* the plot settings' graph: the one `window` names, none (the active one) when it is left out */
Result<std::optional<int>> graph_of(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    if (!args.has(WINDOW_KEY)) return std::optional<int>{};
    const Result<int> index = graph_of_command(s, command, args, place);
    if (!index) return std::unexpected<Error>(index.error());
    return std::optional<int>{*index};
}

bool last_state_of(const Arguments &args) { return args.optional<std::string>(STATE_KEY).value_or(std::string(STATE_INITIAL)) == STATE_LAST; }

#define GRAPH_OF(name)                                                                  \
    const Result<std::optional<int>> name = graph_of(s, command, args, place);          \
    if (!name) return std::unexpected<Error>(name.error())

Result<std::string> set_apply(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    GRAPH_OF(graph);
    const std::string &name = args.get<std::string>(NAME_KEY), &scope = args.get<std::string>(SCOPE_KEY);
    if (const Result<> applied = apply_named_set(s, name, scope, *graph); !applied) return std::unexpected<Error>(applied.error());
    return ResultObject().text("applied", name).text(SCOPE_KEY, scope).done();
}

/* a save of the current values into a set called name (a new one, or a part of one of the file's own), `member` what
   the result says of it */
Result<std::string> save_set(Session &s, const Command &command, const Arguments &args, const Place &place,
                             Result<> (*save)(Session &, std::string_view, std::string_view, std::optional<int>, bool), const char *member)
{
    GRAPH_OF(graph);
    const std::string &name = args.get<std::string>(NAME_KEY);
    if (const Result<> saved = save(s, name, args.get<std::string>(SCOPE_KEY), *graph, last_state_of(args)); !saved)
        return std::unexpected<Error>(saved.error());
    return ResultObject().text(NAME_KEY, name).boolean(member, true).done();
}

Result<std::string> set_save_new(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return save_set(s, command, args, place, save_new_set, "saved");
}

Result<std::string> set_save_into(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return save_set(s, command, args, place, save_into_set, "updated");
}

Result<std::string> set_save_defaults(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    GRAPH_OF(graph);
    if (const Result<> saved = save_defaults(s, args.get<std::string>(SCOPE_KEY), *graph, last_state_of(args)); !saved)
        return std::unexpected<Error>(saved.error());
    return ResultObject().boolean("saved_as_defaults", true).done();
}

Result<std::string> set_export(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    GRAPH_OF(graph);
    const std::string &file = args.get<std::string>(FILE_KEY), &name = args.get<std::string>(NAME_KEY);
    const Result<bool> exported = export_values(s, name, args.get<std::string>(SCOPE_KEY), file, *graph, last_state_of(args));
    if (!exported) return std::unexpected<Error>(exported.error());
    if (!*exported) return fail(command.name, xpp::format("\"{}\" exists with a set \"{}\" and replacing it was declined: nothing was written", file, name), place);
    return ResultObject().text(FILE_KEY, file).text(NAME_KEY, name).boolean("exported", true).done();
}

Result<std::string> set_rename(Session &s, const Command &, const Arguments &args, const Place &)
{
    const std::string &from = args.get<std::string>("from"), &to = args.get<std::string>("to");
    if (const Result<> renamed = rename_intern_set(s, from, to); !renamed) return std::unexpected<Error>(renamed.error());
    return ResultObject().text("from", from).text("to", to).boolean("renamed", true).done();
}

Result<std::string> set_delete(Session &s, const Command &, const Arguments &args, const Place &)
{
    const std::string &name = args.get<std::string>(NAME_KEY);
    if (const Result<> deleted = delete_intern_set(s, name); !deleted) return std::unexpected<Error>(deleted.error());
    return ResultObject().text(NAME_KEY, name).boolean("deleted", true).done();
}

Result<std::string> set_restore_defaults(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    GRAPH_OF(graph);
    if (const Result<> restored = restore_defaults(s, args.get<std::string>(SCOPE_KEY), args.optional<std::string>("field").value_or(std::string{}), *graph); !restored)
        return std::unexpected<Error>(restored.error());
    return ResultObject().boolean("restored", true).done();
}

constexpr Argument SET_NAME = {NAME_KEY, Type::text, true};
constexpr Argument SCOPE = {SCOPE_KEY, Type::choice, true, MAX_TEXT_BYTES, SCOPE_CHOICES};
constexpr Argument WINDOW = {WINDOW_KEY, Type::window, false};
constexpr Argument STATE = {STATE_KEY, Type::choice, false, MAX_TEXT_BYTES, STATE_CHOICES};

constexpr std::array APPLY_ARGUMENTS = {SET_NAME, SCOPE, WINDOW};
constexpr std::array SAVE_ARGUMENTS = {SET_NAME, SCOPE, WINDOW, STATE};
constexpr std::array DEFAULTS_ARGUMENTS = {SCOPE, WINDOW, STATE};
constexpr std::array EXPORT_ARGUMENTS = {Argument{FILE_KEY, Type::file, true}, SET_NAME, SCOPE, WINDOW, STATE};
constexpr std::array RENAME_ARGUMENTS = {Argument{"from", Type::text, true}, Argument{"to", Type::text, true}};
constexpr std::array DELETE_ARGUMENTS = {SET_NAME};
constexpr std::array RESTORE_ARGUMENTS = {SCOPE, WINDOW, Argument{"field", Type::text, false}};

constexpr std::array APPLY_RESULTS = {ResultField{"applied", "text"}, ResultField{SCOPE_KEY, "text"}};
constexpr std::array SAVE_NEW_RESULTS = {ResultField{NAME_KEY, "text"}, ResultField{"saved", "boolean"}};
constexpr std::array SAVE_INTO_RESULTS = {ResultField{NAME_KEY, "text"}, ResultField{"updated", "boolean"}};
constexpr std::array DEFAULTS_RESULTS = {ResultField{"saved_as_defaults", "boolean"}};
constexpr std::array EXPORT_RESULTS = {ResultField{FILE_KEY, "file"}, ResultField{NAME_KEY, "text"}, ResultField{"exported", "boolean"}};
constexpr std::array RENAME_RESULTS = {ResultField{"from", "text"}, ResultField{"to", "text"}, ResultField{"renamed", "boolean"}};
constexpr std::array DELETE_RESULTS = {ResultField{NAME_KEY, "text"}, ResultField{"deleted", "boolean"}};
constexpr std::array RESTORE_RESULTS = {ResultField{"restored", "boolean"}};

constexpr std::array STATE_EVENTS = {xpp::events::SESSION_STATE, xpp::events::DONE};

constexpr std::array ROWS = {
    Command{"set.apply", APPLY_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Files, "File/Apply set",
            "Apply one of the model's named sets over the current values: all of it, or only its parameters, initial values or one group of settings; the plot settings go to the graph window names.",
            APPLY_RESULTS, STATE_EVENTS, true, set_apply},
    Command{"set.save_new", SAVE_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "File/Save values",
            "Write the current values (all of them, or one scope) into the model file as a new set called name; state says whether the initial values or the last state reached are saved.",
            SAVE_NEW_RESULTS, STATE_EVENTS, true, set_save_new},
    Command{"set.save_into", SAVE_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "File/Save values",
            "Replace the scope's part of one of the model file's own sets by the current values; the set's other parts stay.",
            SAVE_INTO_RESULTS, STATE_EVENTS, true, set_save_into},
    Command{"set.save_defaults", DEFAULTS_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "File/Save as defaults",
            "Write the current values (all of them, or one scope) as the model's own: its par and init lines and options groups.",
            DEFAULTS_RESULTS, STATE_EVENTS, true, set_save_defaults},
    Command{"set.export", EXPORT_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "File/Export values",
            "Write the current values (all of them, or one scope) as the set called name of another .odex file, a new one or an existing one that asks before a set of that name is replaced.",
            EXPORT_RESULTS, STATE_EVENTS, true, set_export, {}, {}, "write", "*.odex"},
    Command{"set.rename", RENAME_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "File/Rename set",
            "Rename one of the model file's own sets.", RENAME_RESULTS, STATE_EVENTS, true, set_rename},
    Command{"set.delete", DELETE_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "File/Delete set",
            "Delete one of the model file's own sets from the file.", DELETE_RESULTS, STATE_EVENTS, true, set_delete},
    Command{"set.restore_defaults", RESTORE_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Files, "File/Restore defaults",
            "Put the model's own values back: everything, or one scope (the parameters, the initial values, one group of settings), or with field only that parameter, state or setting.",
            RESTORE_RESULTS, STATE_EVENTS, true, set_restore_defaults}};

} // namespace

std::span<const Command> set_commands() { return ROWS; }

} // namespace xpp::command
