/* The settings panels' registry rows (W616): read and write the values of one option group (model_options.h, options_panels.h), whichever
   group it is. `options.get` reads a group's values (a graph's, for the plot group), `options.configure` sets some of them, all or nothing,
   one undo step, and `output.configure` is the same for the output group, the one File menu item that opens a panel of its own. A
   group's own commands (run.configure, equilibrium.configure, ...) stay for scripts and recordings, and the page opens the group's panel
   in place of their forms (Command::panel). */
#include "command_def.h"
#include "command_graph.h"
#include "load_eqn.h"
#include "model_options.h"
#include "numerics_settings.h"
#include "options_panels.h"
#include "xpp_io.h"
#include "event_registry.h"
#include "command_category.h"
#include "xpp_error.h"
#include <array>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>
namespace xpp { struct Session; }

namespace xpp::command {
namespace {

/* the values are text, multiline so that a sets row's (model_options.h names_sets: a name a line) reaches configure_group, which refuses a line break in any other row's */
constexpr const char *GROUP_KEY = "group", *VALUES_KEY = "values", *WINDOW_KEY = "window";

constexpr auto GROUP_CHOICES = c_names(option_groups);
constexpr std::array GET_ARGUMENTS = {Argument{GROUP_KEY, Type::choice, true, MAX_TEXT_BYTES, GROUP_CHOICES}, Argument{WINDOW_KEY, Type::window, false}};
constexpr std::array CONFIGURE_ARGUMENTS = {Argument{GROUP_KEY, Type::choice, true, MAX_TEXT_BYTES, GROUP_CHOICES},
                                            Argument{VALUES_KEY, Type::map, true, MAX_COMMAND_BYTES, {}, {}, {}, NameSet::none, Type::text, MAX_ITEMS, true},
                                            Argument{WINDOW_KEY, Type::window, false}};
/* the output group's paths are picked in the file dialog (the picker argument `file`, given with file_picker), for the path setting `setting` names */
constexpr const char *SETTING_KEY = "setting", *FILE_KEY = "file";
constexpr std::array OUTPUT_ARGUMENTS = {Argument{VALUES_KEY, Type::map, false, MAX_COMMAND_BYTES, {}, {}, {}, NameSet::none, Type::text, MAX_ITEMS, true},
                                         Argument{SETTING_KEY, Type::text, false}, Argument{FILE_KEY, Type::file, false}};
constexpr std::array PICKED = {SETTING_KEY, FILE_KEY};
constexpr std::array OUTPUT_GROUPS = {Group{GroupRule::together, PICKED}};
constexpr std::array GET_RESULTS = {ResultField{GROUP_KEY, "choice"}, ResultField{VALUES_KEY, "map[text, text]"},
                                    ResultField{"unavailable", "map[text, map[text, text]]"}};
constexpr std::array CONFIGURE_RESULTS = {ResultField{GROUP_KEY, "choice"}, ResultField{"applied", "list[text]"}};
constexpr std::array GET_EVENTS = {xpp::events::DONE};
constexpr std::array CONFIGURE_EVENTS = {xpp::events::RUN_NUMERICS, xpp::events::AUTOX_SETTINGS, xpp::events::PLOT_SHOWN, xpp::events::SESSION_STATE,
                                         xpp::events::DONE};

Result<std::string> options_get(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const Result<std::optional<int>> graph = optional_graph_of_command(s, command, args, place);
    if (!graph) return std::unexpected<Error>(graph.error());
    const std::string &group = args.get<std::string>(GROUP_KEY);
    const auto settings = option_group_values(s, group, *graph, command.name);
    if (!settings) return std::unexpected<Error>(settings.error());
    std::string values = "{", unavailable = "{";
    for (const auto &[row, text] : *settings) {
        if (values.size() > 1) values += ',';
        json_append_string(values, row->odex);
        values += ':';
        json_append_string(values, text);
        if (const std::string no = unavailable_choices_json(s, *row); !no.empty()) {
            if (unavailable.size() > 1) unavailable += ',';
            json_append_string(unavailable, row->odex);
            unavailable += ':' + no;
        }
    }
    return ResultObject().text(GROUP_KEY, group).raw(VALUES_KEY, values + '}').raw("unavailable", unavailable + '}').done();
}

/* the values given for group's settings applied, each named by its .odex key; with `extra`, one more value (the path the dialog picked) */
Result<std::string> configure_group(Session &s, const Command &command, const Arguments &args, const Place &place, const std::string &group,
                                    std::optional<std::pair<std::string, std::string>> extra = std::nullopt)
{
    const Result<std::optional<int>> graph = optional_graph_of_command(s, command, args, place);
    if (!graph) return std::unexpected<Error>(graph.error());
    std::vector<SetItem> items;
    std::string applied = "[";
    std::vector<std::pair<std::string, std::string>> given = args.map<std::string>(VALUES_KEY);
    if (extra) given.push_back(std::move(*extra));
    for (const auto &[key, text] : given) {
        int index = 0;
        const OptionRow *row = find_odex_option(group, key, index);
        if (!row || !option_in_panel(*row)) return fail(command.name, xpp::format("{} is no setting of {}", key, group), place);
        if (text.find_first_of(row->names_sets ? "\r\t" : "\n\r\t") != std::string::npos)
            return fail(command.name, xpp::format("{}: the value holds a {}", key, row->names_sets ? "tab or carriage return" : "line break or tab"), place);
        items.push_back({option_setting_name(*row, index), text, true, place});
        if (applied.size() > 1) applied += ',';
        json_append_string(applied, key);
    }
    if (items.empty()) return fail(command.name, "Give at least one setting", place);
    if (const Result<> done = apply_option_group(s, group, items, place, *graph, command.name); !done) return std::unexpected<Error>(done.error());
    return ResultObject().text(GROUP_KEY, group).raw("applied", applied + ']').done();
}

Result<std::string> options_configure(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return configure_group(s, command, args, place, args.get<std::string>(GROUP_KEY));
}

Result<std::string> output_configure(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    std::optional<std::pair<std::string, std::string>> picked;
    if (args.has(SETTING_KEY)) {
        int index = 0;
        const OptionRow *row = find_odex_option(option_group::output, args.get<std::string>(SETTING_KEY), index);
        if (!row || row->kind != OptionKind::path) return fail(command.name, xpp::format("{} is no path setting of output", args.get<std::string>(SETTING_KEY)), place);
        picked.emplace(args.get<std::string>(SETTING_KEY), args.get<std::string>(FILE_KEY));
    }
    return configure_group(s, command, args, place, std::string(option_group::output), std::move(picked));
}

constexpr std::array ROWS = {
    Command{"options.get", GET_ARGUMENTS, XPP_KIND_CONTROL, CommandCategory::Panels, "Get settings", "",
            "The values of one group of settings as their text, those the method and model use, with the choices the model cannot have; the plot group's are a graph's.",
            GET_RESULTS, GET_EVENTS, false, options_get},
    Command{"options.configure", CONFIGURE_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Panels, "Settings", "",
            "Set some of one group's settings by their option keys, checked together: all or nothing, one undo step. The plot group's go to the graph given.",
            CONFIGURE_RESULTS, CONFIGURE_EVENTS, true, options_configure},
    Command{"output.configure", OUTPUT_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Files, "Output", "File",
            "Set what a run writes by itself (the data file, the nullclines file, what a batch run makes of its data), by the output group's option keys; a path can be picked in the file dialog.",
            CONFIGURE_RESULTS, CONFIGURE_EVENTS, true, output_configure, OUTPUT_GROUPS, {}, "write", "*", "output"}};

} // namespace

std::span<const Command> options_commands() { return ROWS; }

} // namespace xpp::command
