/* The set resource's registry rows (W526): apply one of the model's named sets, load values from another file by
   scope (W321), save the current values into the model file (a new set, a set's part, the defaults, another
   file), rename and delete a set, and restore the model's own values. Each handler gets its arguments validated and typed (command_def.h) and calls the same core
   function the File menu's picks do (load_eqn.h, model_save.h): one behaviour, whichever front end asks. The
   settings of a plot (scope plot, or all) are the graph `window` names, the active graph when it is left out; the
   values saved are the initial ones unless `state` says last. */
#include "command_def.h"
#include "command_graph.h"
#include "load_eqn.h"
#include "model_options.h"
#include "model_save.h"
#include "session.h"
#include "set_load.h"
#include "xpp_io.h"
#include <algorithm>
#include <array>

namespace xpp::command {
namespace {

constexpr const char *WINDOW_KEY = "window", *SCOPE_KEY = "scope", *STATE_KEY = "state", *NAME_KEY = "name", *FILE_KEY = "file", *FROM_KEY = "from";

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
/* the part of a file of values loaded: its defaults, one of its sets, or the whole of an XPPAUT file */
constexpr std::array FROM_CHOICES = {values_origin_names[0].data(), values_origin_names[1].data(), values_origin_names[2].data()};

bool last_state_of(const Arguments &args) { return args.optional<std::string>(STATE_KEY).value_or(std::string(STATE_INITIAL)) == STATE_LAST; }

#define GRAPH_OF(name)                                                                  \
    const Result<std::optional<int>> name = optional_graph_of_command(s, command, args, place);          \
    if (!name) return std::unexpected<Error>(name.error())

Result<std::string> set_apply(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    GRAPH_OF(graph);
    const std::string &name = args.get<std::string>(NAME_KEY), &scope = args.get<std::string>(SCOPE_KEY);
    if (const Result<> applied = apply_named_set(s, name, scope, *graph); !applied) return std::unexpected<Error>(applied.error());
    return ResultObject().text("applied", name).text(SCOPE_KEY, scope).done();
}

/* the parts of a file of values that hold a scope's kind: where each comes from (its defaults, a set, an XPPAUT file),
   its set's name, and its problem when it cannot be loaded (what is wrong with it, at its line); the whole file is read
   and checked first */
Result<std::string> set_file_contents(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const std::string &file = args.get<std::string>(FILE_KEY), &scope = args.get<std::string>(SCOPE_KEY);
    const Result<ValuesSource> source = read_values_source(s, file);
    if (!source) return std::unexpected<Error>(source.error());
    std::string parts = "[";
    std::size_t count = 0;
    for (const ValuesPart &part : source->parts) {
        if (!offers(s, part, scope)) continue;
        ResultObject entry;
        entry.text(FROM_KEY, values_origin_names[static_cast<std::size_t>(part.origin)]).text(NAME_KEY, part.name);
        if (part.problem)
            entry.text("problem", part.problem->place.line > 0 ? xpp::format("line {}: {}", part.problem->place.line, part.problem->what) : part.problem->what);
        if (parts.size() > 1) parts += ',';
        parts += entry.done();
        if (++count > offered_parts_limit || parts.size() > offered_bytes_limit)
            return fail(command.name, xpp::format("{} offers more than {} parts or {} bytes of them for the scope {}: load one by its name with set.load_file", file, offered_parts_limit, offered_bytes_limit, scope), place);
    }
    return ResultObject().text(FILE_KEY, file).text("format", source->format).text(SCOPE_KEY, scope).raw("parts", parts + ']').done();
}

Result<std::string> set_load_file(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    GRAPH_OF(graph);
    const std::string &file = args.get<std::string>(FILE_KEY), &scope = args.get<std::string>(SCOPE_KEY), &from = args.get<std::string>(FROM_KEY);
    const std::string name = args.optional<std::string>(NAME_KEY).value_or(std::string{});
    const std::size_t origin = static_cast<std::size_t>(std::ranges::find(values_origin_names, from) - values_origin_names.begin());
    const bool is_set = origin == static_cast<std::size_t>(ValuesOrigin::set);
    if (is_set && name.empty()) return fail(command.name, "from set names the file's set: give its name", place);
    if (!is_set && !name.empty()) return fail(command.name, xpp::format("a file's {} has no name: left out, not \"{}\"", from, name), place);
    if (const Result<> loaded = load_values_from_file(s, file, scope, static_cast<ValuesOrigin>(origin), name, *graph); !loaded)
        return std::unexpected<Error>(loaded.error());
    return ResultObject().text(FILE_KEY, file).text(SCOPE_KEY, scope).text(FROM_KEY, from).boolean("loaded", true).done();
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
constexpr Argument VALUES_FILE = {FILE_KEY, Type::file, true};
constexpr Argument FROM = {FROM_KEY, Type::choice, true, MAX_TEXT_BYTES, FROM_CHOICES};

constexpr std::array APPLY_ARGUMENTS = {SET_NAME, SCOPE, WINDOW};
constexpr std::array CONTENTS_ARGUMENTS = {VALUES_FILE, SCOPE};
constexpr std::array LOAD_ARGUMENTS = {VALUES_FILE, SCOPE, FROM, Argument{NAME_KEY, Type::text, false}, WINDOW};
constexpr std::array SAVE_ARGUMENTS = {SET_NAME, SCOPE, WINDOW, STATE};
constexpr std::array DEFAULTS_ARGUMENTS = {SCOPE, WINDOW, STATE};
constexpr std::array EXPORT_ARGUMENTS = {Argument{FILE_KEY, Type::file, true}, SET_NAME, SCOPE, WINDOW, STATE};
constexpr std::array RENAME_ARGUMENTS = {Argument{"from", Type::text, true}, Argument{"to", Type::text, true}};
constexpr std::array DELETE_ARGUMENTS = {SET_NAME};
constexpr std::array RESTORE_ARGUMENTS = {SCOPE, WINDOW, Argument{"field", Type::text, false}};

constexpr std::array APPLY_RESULTS = {ResultField{"applied", "text"}, ResultField{SCOPE_KEY, "text"}};
constexpr std::array CONTENTS_RESULTS = {ResultField{FILE_KEY, "file"}, ResultField{"format", "text"}, ResultField{SCOPE_KEY, "text"},
                                         ResultField{"parts", "list of {from, name, problem}"}};
constexpr std::array LOAD_RESULTS = {ResultField{FILE_KEY, "file"}, ResultField{SCOPE_KEY, "text"}, ResultField{FROM_KEY, "text"}, ResultField{"loaded", "boolean"}};
constexpr std::array SAVE_NEW_RESULTS = {ResultField{NAME_KEY, "text"}, ResultField{"saved", "boolean"}};
constexpr std::array SAVE_INTO_RESULTS = {ResultField{NAME_KEY, "text"}, ResultField{"updated", "boolean"}};
constexpr std::array DEFAULTS_RESULTS = {ResultField{"saved_as_defaults", "boolean"}};
constexpr std::array EXPORT_RESULTS = {ResultField{FILE_KEY, "file"}, ResultField{NAME_KEY, "text"}, ResultField{"exported", "boolean"}};
constexpr std::array RENAME_RESULTS = {ResultField{"from", "text"}, ResultField{"to", "text"}, ResultField{"renamed", "boolean"}};
constexpr std::array DELETE_RESULTS = {ResultField{NAME_KEY, "text"}, ResultField{"deleted", "boolean"}};
constexpr std::array RESTORE_RESULTS = {ResultField{"restored", "boolean"}};

constexpr std::array STATE_EVENTS = {xpp::events::SESSION_STATE, xpp::events::DONE};
constexpr std::array DONE_EVENTS = {xpp::events::DONE};

constexpr std::array ROWS = {
    Command{"set.rename", RENAME_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "Rename set", "File/Sets",
            "Rename one of the model file's own sets.", RENAME_RESULTS, STATE_EVENTS, true, set_rename},
    Command{"set.delete", DELETE_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "Delete set", "File/Sets",
            "Delete one of the model file's own sets from the file.", DELETE_RESULTS, STATE_EVENTS, true, set_delete},
    Command{"set.load_file", LOAD_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Files, "Load a named part of a file", "File/Sets",
            "Load values from a file by scope: the file's defaults (from defaults), one of its sets (from set, name) or an XPPAUT .par, .ic or .set (from file), limited to the scope, over the current values; the plot settings go to the graph window names. The whole file is checked against this model first and an unknown name refuses it; nothing runs and the model file is not changed.",
            LOAD_RESULTS, STATE_EVENTS, true, set_load_file, {}, {}, "read", "*.odex *.par *.ic *.set"},
    Command{"set.apply", APPLY_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Files, "Apply set", "",
            "Apply one of the model's named sets over the current values: all of it, or only its parameters, initial values or one group of settings; the plot settings go to the graph window names.",
            APPLY_RESULTS, STATE_EVENTS, true, set_apply},
    Command{"set.save_new", SAVE_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "Save as new set", "",
            "Write the current values (all of them, or one scope) into the model file as a new set called name; state says whether the initial values or the last state reached are saved.",
            SAVE_NEW_RESULTS, STATE_EVENTS, true, set_save_new},
    Command{"set.save_into", SAVE_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "Replace in set", "",
            "Replace the scope's part of one of the model file's own sets by the current values; the set's other parts stay.",
            SAVE_INTO_RESULTS, STATE_EVENTS, true, set_save_into},
    Command{"set.save_defaults", DEFAULTS_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "Save as defaults", "",
            "Write the current values (all of them, or one scope) as the model's own: its par and init lines and options groups.",
            DEFAULTS_RESULTS, STATE_EVENTS, true, set_save_defaults},
    Command{"set.restore_defaults", RESTORE_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Files, "Restore defaults", "",
            "Put the model's own values back: everything, or one scope (the parameters, the initial values, one group of settings), or with field only that parameter, state or setting.",
            RESTORE_RESULTS, STATE_EVENTS, true, set_restore_defaults},
    Command{"set.file_contents", CONTENTS_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Files, "Load from file", "",
            "Read a file of values whole (an .odex, or XPPAUT's .par, .ic or .set) and list its parts that hold the scope's kind: its defaults, each set, or the XPPAUT file; a part that cannot be loaded says why. Nothing is applied.",
            CONTENTS_RESULTS, DONE_EVENTS, true, set_file_contents, {}, {}, "read", "*.odex *.par *.ic *.set"},
    Command{"set.export", EXPORT_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Files, "Export set", "",
            "Write the current values (all of them, or one scope) as the set called name of another .odex file, a new one or an existing one that asks before a set of that name is replaced.",
            EXPORT_RESULTS, STATE_EVENTS, true, set_export, {}, {}, "write", "*.odex"},};

} // namespace

std::span<const Command> set_commands() { return ROWS; }

} // namespace xpp::command
