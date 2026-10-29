/* The registry itself (W353, W530): the joined tables of every resource's
   rows, lookup by name, help and hello's description of them, running a
   validated command and its one terminal event. Rows live in their
   resources' files (commands_window.cpp ...), validation in
   command_validate.cpp and the server text form in command_text.cpp. */
#include "command_registry.h"
#include "command_def.h"
#include "json_error.h"
#include "json_number.h"
#include "json_reader.h"
#include "session.h"
#include "xpp_io.h"
#include "xpp_ui.h"
#include "xpp_files.h"
#include <algorithm>
#include <array>
#include <cstring>

namespace xpp::command {
namespace {

Result<std::string> describe(Session &, const Command &command, const Arguments &args, const Place &place);

constexpr std::array HELP_ARGUMENTS = {Argument{"name", Type::text, false}};
constexpr std::array HELP_RESULTS = {ResultField{"commands", "list[command]"}};
constexpr std::array HELP_EVENTS = {"state", "done"};
constexpr std::array HELP_ROWS = {
    Command{HELP_COMMAND.data(), HELP_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Tools, "Help/Command reference",
            "Describe registered commands, or the command named by name.", HELP_RESULTS, HELP_EVENTS, false, describe}};
std::span<const Command> help_commands() { return HELP_ROWS; }

/* every resource's table, in the order help lists them */
constexpr std::array TABLES = {help_commands, window_commands, annotation_commands, run_commands, ic_commands, bvp_commands, auto_commands, plot_commands,
                              equilibrium_commands, nullcline_commands, dirfield_commands, analysis_commands, table_commands, browser_commands, set_commands, value_commands, kinescope_commands, animation_commands, array_plot_commands, session_commands, recording_commands, keymap_commands, events_commands, tool_commands};

template <class Visit> void each_command(Visit visit)
{
    for (const auto table : TABLES)
        for (const Command &command : table()) visit(command);
}

std::string_view names_words(NameSet names)
{
    switch (names) {
    case NameSet::parameter: return "parameter";
    case NameSet::variable: return "variable";
    case NameSet::either: return "either";
    case NameSet::any: return "any";
    case NameSet::column: return "column";
    case NameSet::none: break;
    }
    return "";
}

std::string_view rule_name(GroupRule rule)
{
    switch (rule) {
    case GroupRule::exactly_one: return "exactly_one";
    case GroupRule::at_most_one: return "at_most_one";
    case GroupRule::together: return "together";
    }
    return "";
}

/* `,"min":3` for a constant, `,"min_of":"threads_max"` for the session's quantity */
void append_bound(std::string &out, const char *key, const Bound &bound)
{
    if (bound.value) {
        out += xpp::format(",\"{}\":", key);
        json::json_append_number_shortest(out, *bound.value);
    } else if (bound.quantity) {
        out += xpp::format(",\"{}_of\":", key);
        json_append_string(out, bound.quantity);
    }
}

void append_argument(std::string &out, const Argument &arg)
{
    out += "{\"name\":";
    json_append_string(out, arg.name);
    out += ",\"type\":";
    json_append_string(out, type_name(arg.type));
    out += xpp::format(",\"required\":{},\"max_length\":{},\"choices\":[", arg.required, arg.max_length);
    for (std::size_t j = 0; j < arg.choices.size(); ++j) {
        if (j) out += ',';
        json_append_string(out, arg.choices[j]);
    }
    out += ']';
    if (arg.multiline) out += ",\"multiline\":true";
    append_bound(out, "min", arg.min);
    append_bound(out, "max", arg.max);
    if (arg.names != NameSet::none) {
        out += ",\"names\":";
        json_append_string(out, names_words(arg.names));
    }
    if (arg.type == Type::list || arg.type == Type::map) {
        out += ",\"element\":";
        json_append_string(out, type_name(arg.element));
        out += xpp::format(",\"max_items\":{}", arg.max_items);
    }
    out += '}';
}

} // namespace

const Argument *picker_argument(const Command &command)
{
    const Argument *file = nullptr;
    for (const Argument &arg : command.arguments)
        if (arg.type == Type::file || (arg.type == Type::path && arg.required)) {
            if (file) return nullptr;
            file = &arg;
        }
    return file;
}

std::vector<const Command *> rows()
{
    std::vector<const Command *> result;
    each_command([&](const Command &command) { result.push_back(&command); });
    return result;
}

void append_command(std::string &out, const Command &command)
{
    out += "{\"cmd\":";
    json_append_string(out, command.name);
    out += xpp::format(",\"kind\":\"{}\",\"step\":{},\"completion\":\"done\",\"max_input_bytes\":{},\"category\":", command.kind, command.step, MAX_COMMAND_BYTES);
    json_append_string(out, category_id(command.category));
    out += ",\"menu_path\":";
    json_append_string(out, command.menu_path);
    out += ",\"default_keys\":[";
    for (std::size_t i = 0; i < command.default_keys.size(); ++i) {
        if (i) out += ',';
        json_append_string(out, command.default_keys[i]);
    }
    out += "]";
    out += ",\"description\":";
    json_append_string(out, command.description);
    if (picker_argument(command) != nullptr) {
        out += ",\"file_mode\":";
        json_append_string(out, command.file_mode);
        out += ",\"file_wild\":";
        json_append_string(out, command.file_wild);
    }
    out += ",\"arguments\":[";
    for (std::size_t i = 0; i < command.arguments.size(); ++i) {
        if (i) out += ',';
        append_argument(out, command.arguments[i]);
    }
    out += ']';
    if (!command.groups.empty()) {
        out += ",\"groups\":[";
        for (std::size_t i = 0; i < command.groups.size(); ++i) {
            if (i) out += ',';
            out += "{\"rule\":";
            json_append_string(out, rule_name(command.groups[i].rule));
            out += ",\"arguments\":[";
            for (std::size_t j = 0; j < command.groups[i].arguments.size(); ++j) {
                if (j) out += ',';
                json_append_string(out, command.groups[i].arguments[j]);
            }
            out += "]}";
        }
        out += ']';
    }
    out += ",\"result\":[";
    for (std::size_t i = 0; i < command.results.size(); ++i) {
        if (i) out += ',';
        out += "{\"name\":";
        json_append_string(out, command.results[i].name);
        out += ",\"type\":";
        json_append_string(out, command.results[i].type);
        out += '}';
    }
    out += "],\"events\":[";
    for (std::size_t i = 0; i < command.events.size(); ++i) {
        if (i) out += ',';
        json_append_string(out, command.events[i]);
    }
    out += "]}";
}

namespace {

Result<std::string> describe(Session &, const Command &command, const Arguments &args, const Place &place)
{
    const std::optional<std::string> selected = args.optional<std::string>(command.arguments.front().name);
    const Command *target = selected ? find(*selected) : nullptr;
    if (selected && !target) return fail(command.name, xpp::format("Unknown registry command \"{}\"", *selected), place);
    std::string result = "{";
    json_append_string(result, command.results.front().name);
    result += ":[";
    if (target) append_command(result, *target);
    else {
        bool first = true;
        each_command([&](const Command &row) {
            if (!first) result += ',';
            first = false;
            append_command(result, row);
        });
    }
    result += "]}";
    return result;
}

} // namespace

Result<std::string> front_end(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return ui.front_end_command(s, command, args, place);
}

Result<> run_button(Session &s,const std::string &line,const Place &place)
{
    const auto result=run(s,line.c_str(),place,false);
    if(!result)return std::unexpected(result.error());
    return {};
}

const char *type_name(Type type)
{
    /* Type's declaration order is the registry vocabulary's order. */
    static constexpr std::array NAMES={"text","integer","real","boolean","choice","expression","name","file","path",
        "color","list","map","window","new_name","list[real, 2]","list[text]"};
    static_assert(NAMES.size()==static_cast<std::size_t>(Type::text_list)+1);
    const auto index=static_cast<std::size_t>(type);
    return index<NAMES.size()?NAMES[index]:"";
}

const Command *find(std::string_view name)
{
    const Command *found = nullptr;
    each_command([&](const Command &command) {
        if (!found && name == command.name) found = &command;
    });
    return found;
}

std::string_view numerics_command(std::string_view key)
{
    for (const auto &[command_name, prefix] : {std::pair<std::string_view, std::string_view>{"run.configure", {}}, {"bvp.configure", BVP_KEY_PREFIX}}) {
        const Command *command = find(command_name);
        if (command && std::ranges::any_of(command->arguments, [&](const Argument &arg) { return std::string(prefix) + arg.name == key; }))
            return command_name;
    }
    return {};
}

std::string_view name(const char *line)
{
    line = json::skip_ws(line);
    if (*line == '{') {
        std::string word;
        if (json::js_string(json::js_find(line, COMMAND_KEY), word))
            if (const Command *command = find(word)) return command->name;
        return {};
    }
    const char *end = line;
    while (*end && *end != ' ' && *end != '\t' && *end != '\r' && *end != '\n') ++end;
    if (const Command *command = find({line, static_cast<std::size_t>(end - line)})) return command->name;
    return {};
}

bool is_step(const char *line)
{
    const Command *command = find(name(line));
    return command && command->step;
}

char kind(const char *line)
{
    const Command *command = find(name(line));
    return command ? command->kind : 0;
}

void append_commands(std::string &out)
{
    each_command([&](const Command &command) {
        out += ',';
        append_command(out, command);
    });
}

Result<> check_shape(const char *line, const Place &place)
{
    if (std::strlen(line) > MAX_COMMAND_BYTES) return fail("command", "Command exceeds the input limit", place);
    const Command *command = find(name(line));
    if (!command) return fail("command", "Unknown registry command", place);
    /* a recording's step may name a path (Open model of a file it holds): the player serves it from the recording, and run() decides again when it plays */
    const Result<Arguments> checked = validate(nullptr, *command, line, place, true);
    if (!checked) return std::unexpected<Error>(checked.error());
    return {};
}

Result<std::string> run(Session &s, const char *line, const Place &place, bool paths_allowed)
{
    if (std::strlen(line) > MAX_COMMAND_BYTES)
        return fail("command", xpp::format("Command exceeds {} bytes", MAX_COMMAND_BYTES), place);
    const Command *command = find(name(line));
    if (!command) return fail("command", "Unknown registry command", place);
    Result<Arguments> args = validate(&s, *command, line, place, paths_allowed);
    if (!args) return std::unexpected<Error>(args.error());
    if (args->has(FILE_PICKER_KEY)) {
        const Argument *file_arg = picker_argument(*command);
        std::string file = args->get<std::string>(file_arg->name);
        if (file == PICKER_FILE) {
            const std::string_view wild = command->file_wild;
            file = wild.starts_with("*.") && wild.find(' ') == std::string_view::npos
                ? files::output_name(s.model().this_file, wild.substr(1)) : std::string{};
        }
        const std::string title = xpp::format("{} {}", std::string_view(command->file_mode) == "write" ? "Save" : "Open", command->menu_path);
        if (!file_selector(title, file, command->file_wild)) return std::string("{\"cancelled\":true}");
        args->add(file_arg->name, Scalar{std::move(file)});
    }
    Result<std::string> result = command->execute(s, *command, *args, place);
    if (!result) return std::unexpected<Error>(with_source_line(result.error()));
    return result;
}

std::string done(const char *line, const Result<std::string> &result, bool stopped)
{
    std::string out = "{\"ev\":\"done\"";
    const char *wrong_at = nullptr;
    if (!json::js_problem(line, wrong_at)) {
        if (const char *id = json::js_find(line, ID_KEY); id && valid_id(id)) {
            out += ',';
            json_append_string(out, ID_KEY);
            out += ':';
            std::string text;
            json::js_string(id, text);
            json_append_string(out, text);
        }
    }
    if (!result) {
        out += ",\"status\":\"error\"";
        json::append_error(out, result.error());
    } else if (stopped) out += ",\"status\":\"stopped\"";
    else out += ",\"status\":\"ok\",\"result\":" + *result;
    out += '}';
    return out;
}

} // namespace xpp::command
