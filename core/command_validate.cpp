/* The registry's one validator (W353, W530): a JSON command line against its
   row's argument types, into typed Arguments, all or nothing. Every front end
   reaches it through command::run (and a recording's shape check through
   command::check_shape), so a type is checked the same wherever the line
   came from. Input is bounded before anything grows from it: the line (16384
   bytes, checked by run), each text (max_length), each list or map
   (max_items), and one level of nesting. */
#include "command_def.h"
#include "json_reader.h"
#include "many_pops.h"
#include "session.h"
#include "typed_values.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <stdexcept>

namespace xpp::command {
namespace {

struct Check {
    Session *session;
    const Command &command;
    const char *line;
    const Place &place;
    bool paths_allowed;
};

bool is_text_type(Type type)
{
    switch (type) {
    case Type::text: case Type::choice: case Type::expression: case Type::model_name: case Type::file:
    case Type::path: case Type::color: case Type::window: case Type::new_name:
        return true;
    default:
        return false;
    }
}

std::string_view name_set_words(NameSet names)
{
    switch (names) {
    case NameSet::parameter: return "parameter";
    case NameSet::variable: return "variable";
    case NameSet::either: return "parameter or variable";
    default: return "name";
    }
}

bool names_a(const Model &model, NameSet names, std::string_view text)
{
    switch (names) {
    case NameSet::parameter: return find_user_name(model, PARAMBOX, text) >= 0;
    case NameSet::variable: return find_user_name(model, ICBOX, text) >= 0;
    case NameSet::either: return find_user_name(model, PARAMBOX, text) >= 0 || find_user_name(model, ICBOX, text) >= 0;
    default: return model.name(text) != nullptr;
    }
}

std::unexpected<Error> refuse(const Check &c, std::string what) { return fail(c.command.name, std::move(what), c.place); }

/* the bound as a number; none when it is not set, or is the session's and
   there is no session */
std::optional<double> resolve(const Check &c, const Bound &bound)
{
    if (bound.value) return bound.value;
    if (bound.of && c.session) return bound.of(*c.session);
    return std::nullopt;
}

Result<> in_range(const Check &c, const Argument &arg, std::string_view label, double number)
{
    if (const auto low = resolve(c, arg.min); low && number < *low)
        return refuse(c, xpp::format("Argument \"{}\" must be at least {}", label, *low));
    if (const auto high = resolve(c, arg.max); high && number > *high)
        return refuse(c, xpp::format("Argument \"{}\" must be at most {}", label, *high));
    return {};
}

Result<Scalar> read_number(const Check &c, const Argument &arg, Type type, const char *value, std::string_view label)
{
    const std::string_view raw = json::js_raw(value);
    Scalar number;
    double as_double = 0;
    if (type == Type::integer) {
        long long whole = 0;
        const char *end = raw.data() + raw.size();
        const auto parsed = std::from_chars(raw.data(), end, whole);
        if (raw.empty() || parsed.ec != std::errc() || parsed.ptr != end || whole > MAX_INTEGER || whole < -MAX_INTEGER)
            return refuse(c, xpp::format("Argument \"{}\" must be a whole number of at most 15 digits", label));
        number = whole;
        as_double = static_cast<double>(whole);
    } else {
        if (!parse_number(raw, as_double) || !std::isfinite(as_double))
            return refuse(c, xpp::format("Argument \"{}\" must be a finite number", label));
        number = as_double;
    }
    if (const Result<> ranged = in_range(c, arg, label, as_double); !ranged) return std::unexpected<Error>(ranged.error());
    return number;
}

/* a text type's value: decoded, bounded, then what its type adds */
Result<Scalar> read_text(const Check &c, const Argument &arg, Type type, const char *value, std::string_view label)
{
    std::string text;
    if (!json_decode_string(value, text, std::string::npos, true) || text.size() > arg.max_length)
        return refuse(c, xpp::format("Argument \"{}\" must be text of at most {} bytes", label, arg.max_length));
    const auto bad = [&](std::string what) { return refuse(c, std::move(what)); };
    switch (type) {
    case Type::window:
        if (c.session && !find_window(*c.session, text)) return bad(xpp::format("Unknown window \"{}\"", text));
        break;
    case Type::choice:
        if (std::ranges::find(arg.choices, text) == arg.choices.end())
            return bad(xpp::format("Invalid choice \"{}\" for {}", text, label));
        break;
    case Type::new_name: {
        std::string replacing;
        json::js_string(json::js_find(c.line, "window"), replacing);
        const std::string problem = c.session ? window_name_problem(*c.session, text, replacing) : window_name_problem(text);
        if (!problem.empty()) return bad(problem);
        break;
    }
    case Type::model_name:
        if (c.session && !names_a(c.session->model(), arg.names, text))
            return bad(xpp::format("Argument \"{}\": no {} named \"{}\" in the model", label, name_set_words(arg.names), text));
        break;
    case Type::expression:
        if (c.session && c.session->model().loaded()) {
            if (const Result<> compiled = compile_formula(*c.session, text); !compiled)
                return bad(xpp::format("Argument \"{}\": {}", label, compiled.error().what));
        }
        break;
    case Type::file:
        if (!files::name_ok(text))
            return bad(xpp::format("Argument \"{}\": \"{}\" is {}", label, text, files::status_text(XPP_FILES_BAD_NAME)));
        break;
    case Type::path:
        if (text.empty()) return bad(xpp::format("Argument \"{}\" must not be empty", label));
        if (!c.paths_allowed)
            return bad(xpp::format("Argument \"{}\" is a path, which only --server's own input may give", label));
        break;
    case Type::color:
        if (const auto color = parse_color(text)) return Scalar{*color};
        return bad(xpp::format("Argument \"{}\" must be a CSS colour name, \"#rrggbb\", \"#rrggbbaa\" or \"foreground\"", label));
    default:
        break;
    }
    return Scalar{std::move(text)};
}

/* the scalar of type at value (an argument, a list's item or a map's value) */
Result<Scalar> read_scalar(const Check &c, const Argument &arg, Type type, const char *value, std::string_view label)
{
    if (type == Type::boolean) {
        bool enabled = false;
        if (!parse_bool(json::js_raw(value), enabled))
            return refuse(c, xpp::format("Argument \"{}\" must be boolean", label));
        return Scalar{enabled};
    }
    if (type == Type::integer || type == Type::real) return read_number(c, arg, type, value, label);
    if (is_text_type(type)) return read_text(c, arg, type, value, label);
    return refuse(c, xpp::format("Argument \"{}\" cannot hold a {}", label, type_name(type)));
}

Result<std::vector<Scalar>> read_list(const Check &c, const Argument &arg, const char *value)
{
    std::vector<Scalar> items;
    if (*value != '[')
        return refuse(c, xpp::format("Argument \"{}\" must be a list", arg.name));
    const char *cursor = value;
    while (const char *item = json::js_element(cursor)) {
        if (items.size() >= arg.max_items)
            return refuse(c, xpp::format("Argument \"{}\" must have at most {} items", arg.name, arg.max_items));
        const std::string label = xpp::format("{}[{}]", arg.name, items.size() + 1);
        if (*item == '[' || *item == '{')
            return refuse(c, xpp::format("Argument \"{}\" holds single values, not lists or maps", label));
        Result<Scalar> read = read_scalar(c, arg, arg.element, item, label);
        if (!read) return std::unexpected<Error>(read.error());
        items.push_back(std::move(*read));
    }
    return items;
}

Result<std::vector<Entry>> read_map(const Check &c, const Argument &arg, const char *value)
{
    std::vector<Entry> entries;
    if (*value != '{')
        return refuse(c, xpp::format("Argument \"{}\" must be a map", arg.name));
    const char *cursor = value;
    std::string key;
    const char *item = nullptr;
    while (json::js_member(cursor, key, item)) {
        if (entries.size() >= arg.max_items)
            return refuse(c, xpp::format("Argument \"{}\" must have at most {} entries", arg.name, arg.max_items));
        const std::string label = xpp::format("{}.{}", arg.name, key);
        if (key.size() > arg.max_length)
            return refuse(c, xpp::format("A key of argument \"{}\" must be text of at most {} bytes", arg.name, arg.max_length));
        if (arg.names != NameSet::none && c.session && !names_a(c.session->model(), arg.names, key))
            return refuse(c, xpp::format("Argument \"{}\": no {} named \"{}\" in the model", arg.name, name_set_words(arg.names), key));
        const std::string folded = arg.names != NameSet::none ? lower_case(key) : key;
        if (std::ranges::any_of(entries, [&](const Entry &e) { return (arg.names != NameSet::none ? lower_case(e.first) : e.first) == folded; }))
            return refuse(c, xpp::format("Argument \"{}\" names \"{}\" twice", arg.name, key));
        if (*item == '[' || *item == '{')
            return refuse(c, xpp::format("Argument \"{}\" holds single values, not lists or maps", label));
        Result<Scalar> read = read_scalar(c, arg, arg.element, item, label);
        if (!read) return std::unexpected<Error>(read.error());
        entries.emplace_back(key, std::move(*read));
    }
    return entries;
}

/* how many of the group's arguments the line gave */
std::size_t given_count(const Arguments &args, const Group &group)
{
    return static_cast<std::size_t>(std::ranges::count_if(group.arguments, [&](const char *name) { return args.has(name); }));
}

std::string quoted_names(const Group &group)
{
    std::string out;
    for (const char *name : group.arguments) out += xpp::format("{}\"{}\"", out.empty() ? "" : ", ", name);
    return out;
}

Result<> check_groups(const Check &c, const Arguments &args)
{
    for (const Group &group : c.command.groups) {
        const std::size_t given = given_count(args, group);
        if (group.rule == GroupRule::exactly_one && given != 1)
            return refuse(c, xpp::format("Exactly one of {} is required", quoted_names(group)));
        if (group.rule == GroupRule::at_most_one && given > 1)
            return refuse(c, xpp::format("At most one of {} may be given", quoted_names(group)));
        if (group.rule == GroupRule::together && given != 0 && given != group.arguments.size())
            return refuse(c, xpp::format("{} go together: give all or none", quoted_names(group)));
    }
    return {};
}

} // namespace

const Value &Arguments::need(std::string_view name) const
{
    if (const Value *value = find(name)) return *value;
    throw std::logic_error(xpp::format("Argument \"{}\" was not given", name));
}

bool valid_id(const char *value)
{
    std::string text;
    return json_decode_string(value, text, std::string::npos, true) && text.size() <= MAX_TEXT_BYTES;
}

Result<Arguments> validate(Session *session, const Command &command, const char *line, const Place &place,
                           bool paths_allowed)
{
    const Check c{session, command, line, place, paths_allowed};
    const char *wrong_at = nullptr;
    if (const char *why = json::js_problem(line, wrong_at))
        return fail(command.name, xpp::format("Command {} at character {}", why, wrong_at - line + 1), place);
    Arguments args;
    const char *cursor = line, *value = nullptr;
    std::string key;
    std::vector<std::string> seen;
    while (json::js_member(cursor, key, value)) {
        if (std::ranges::find(seen, key) != seen.end())
            return fail(command.name, xpp::format("Repeated argument \"{}\"", key), place);
        seen.push_back(key);
        if (key == COMMAND_KEY) continue;
        if (key == ID_KEY) {
            if (valid_id(value)) continue;
            return fail(command.name, "id must be text of at most 1024 bytes", place);
        }
        const auto arg = std::ranges::find(command.arguments, key, &Argument::name);
        if (arg == command.arguments.end())
            return fail(command.name, xpp::format("Unknown argument \"{}\"", key), place);
        if (arg->type == Type::list) {
            Result<std::vector<Scalar>> items = read_list(c, *arg, value);
            if (!items) return std::unexpected<Error>(items.error());
            args.add(key, std::move(*items));
        } else if (arg->type == Type::map) {
            Result<std::vector<Entry>> entries = read_map(c, *arg, value);
            if (!entries) return std::unexpected<Error>(entries.error());
            args.add(key, std::move(*entries));
        } else {
            Result<Scalar> scalar = read_scalar(c, *arg, arg->type, value, key);
            if (!scalar) return std::unexpected<Error>(scalar.error());
            args.add(key, std::move(*scalar));
        }
    }
    for (const Argument &arg : command.arguments)
        if (arg.required && !args.has(arg.name))
            return fail(command.name, xpp::format("Missing argument \"{}\"", arg.name), place);
    if (const Result<> grouped = check_groups(c, args); !grouped) return std::unexpected<Error>(grouped.error());
    return args;
}

} // namespace xpp::command
