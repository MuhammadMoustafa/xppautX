/* The --server text form of a registry command (W353, W530): `name --argument
   value ...` into the one JSON object the validator reads, so a command has
   one set of rules whichever way it was typed. A value is written as its
   type is: text of every kind (text, expression, name, file, path, window,
   colour, and a map's keys) is double-quoted with JSON escapes; a number,
   true/false and a choice are bare words; a list is its items between [ and ]
   and a map is `"key" value` pairs between { and }, items and pairs
   separated by blanks, each item written as a single value is. Lists and
   maps hold single values only. */
#include "command_def.h"
#include "command_registry.h"
#include "json_error.h"
#include "json_reader.h"
#include "xpp_io.h"
#include <algorithm>
#include <cstring>

namespace xpp::command {
namespace {

struct Scan {
    const Command &command;
    const Place &place;
    std::string &out;
};

bool blank(char ch) { return ch == ' ' || ch == '\t'; }

/* a bare word ends at a blank, the end, or inside a list or map at its closing bracket */
const char *word_end(const char *cursor, bool inside)
{
    while (*cursor && !blank(*cursor) && !(inside && (*cursor == ']' || *cursor == '}'))) ++cursor;
    return cursor;
}

bool is_bare(Type type)
{
    return type == Type::choice || type == Type::boolean || type == Type::integer || type == Type::real;
}

bool number_word(std::string_view word)
{
    return !word.empty() && word.find_first_not_of("+-.0123456789eE") == std::string_view::npos;
}

Result<const char *> scan_value(const Scan &, const Argument *, Type, const char *, bool);

/* `[` items `]` or `{` pairs `}` of the argument's element type */
Result<const char *> scan_group(const Scan &scan, const Argument &arg, const char *cursor)
{
    const bool list = arg.type == Type::list;
    const char close = list ? ']' : '}';
    scan.out += *cursor++;
    bool first = true;
    for (cursor = json::skip_ws(cursor); *cursor != close; cursor = json::skip_ws(cursor)) {
        if (!*cursor) return fail(scan.command.name, xpp::format("Missing {} after the items of \"{}\"", close, arg.name), scan.place);
        if (!first) scan.out += ',';
        first = false;
        if (!list) {
            if (*cursor != '"') return fail(scan.command.name, "Map keys must be double-quoted", scan.place);
            const char *end = json::skip_value(cursor);
            const std::string key(cursor, end);
            std::string decoded;
            if (!json::js_valid(key.c_str()) || !json_decode_string(key.c_str(), decoded, std::string::npos, true))
                return fail(scan.command.name, "Invalid quoted text", scan.place);
            scan.out += key;
            scan.out += ':';
            cursor = json::skip_ws(end);
            if (!*cursor || *cursor == close) return fail(scan.command.name, xpp::format("Missing value for \"{}\"", decoded), scan.place);
        }
        const Result<const char *> end = scan_value(scan, &arg, arg.element, cursor, true);
        if (!end) return end;
        cursor = *end;
    }
    scan.out += close;
    return cursor + 1;
}

/* the JSON of the value at cursor, written for `type`; the cursor after it.
   arg is null for a name the row does not have (the validator refuses it). */
Result<const char *> scan_value(const Scan &scan, const Argument *arg, Type type, const char *cursor, bool inside)
{
    const auto bad = [&](std::string what) { return fail(scan.command.name, std::move(what), scan.place); };
    if (*cursor == '[' || *cursor == '{') {
        if (inside) return bad("Lists and maps hold single values");
        if (!arg || arg->type != (*cursor == '[' ? Type::list : Type::map))
            return bad(xpp::format("A {} is not what this argument takes", *cursor == '[' ? "list" : "map"));
        return scan_group(scan, *arg, cursor);
    }
    if (!inside && arg && (arg->type == Type::list || arg->type == Type::map))
        return bad(xpp::format("Argument \"{}\" is written between {}", arg->name, arg->type == Type::list ? "[ and ]" : "{ and }"));
    if (*cursor == '"') {
        if (arg && is_bare(type)) return bad("Choices, booleans and numbers must be bare");
        const char *end = json::skip_value(cursor);
        std::string value;
        const std::string token(cursor, end);
        if (!json::js_valid(token.c_str()) || !json_decode_string(token.c_str(), value, std::string::npos, true))
            return bad("Invalid quoted text");
        scan.out += token;
        return end;
    }
    const char *end = word_end(cursor, inside);
    const std::string_view word(cursor, static_cast<std::size_t>(end - cursor));
    /* text of every kind, and the client id, is always quoted */
    if (!arg || !is_bare(type)) return bad("Text must be double-quoted");
    if (type == Type::choice) json_append_string(scan.out, word);
    else if (type == Type::boolean) {
        if (word != "true" && word != "false") return bad(xpp::format("Argument \"{}\" must be true or false", arg->name));
        scan.out += word;
    } else {
        if (!number_word(word)) return bad(xpp::format("Argument \"{}\" must be a finite number", arg->name));
        scan.out += word;
    }
    return end;
}

} // namespace

Result<std::string> tokenize_command(const Command &command, const char *cursor, const Place &place)
{
    const char *end = cursor;
    std::string out = "{";
    json_append_string(out, COMMAND_KEY);
    out += ':';
    json_append_string(out, command.name);
    cursor = json::skip_ws(cursor);
    while (*cursor) {
        std::string key;
        const bool named = cursor[0] == '-' && cursor[1] == '-';
        if (named) {
            cursor += 2;
            end = cursor;
            while (*end && !blank(*end)) ++end;
            key.assign(cursor, end);
            cursor = json::skip_ws(end);
        } else {
            /* help NAME is the explicitly specified meta-command syntax;
               ordinary resource commands accept named arguments only. */
            if (command.name != HELP_COMMAND) return fail(command.name, "Arguments must be named", place);
            key = command.arguments.front().name;
        }
        if (!*cursor) return fail(command.name, xpp::format("Missing value for \"{}\"", key), place);
        out += ',';
        json_append_string(out, key);
        out += ':';
        const auto arg = std::ranges::find(command.arguments, key, &Argument::name);
        /* the client id is text, so it is not a row's argument */
        const Argument *row = arg == command.arguments.end() ? nullptr : &*arg;
        const Scan scan{command, place, out};
        if (!named && *cursor != '"') {
            /* a command name is a keyword, so help NAME may be bare */
            end = word_end(cursor, false);
            const std::string_view word(cursor, static_cast<std::size_t>(end - cursor));
            if (!find(word)) return fail(command.name, "Text must be double-quoted", place);
            json_append_string(out, word);
        } else {
            const Result<const char *> after = scan_value(scan, row, row ? row->type : Type::text, cursor, false);
            if (!after) return std::unexpected<Error>(after.error());
            end = *after;
        }
        if (*end && !blank(*end)) return fail(command.name, "Expected whitespace after value", place);
        cursor = json::skip_ws(end);
    }
    out += '}';
    return out;
}

Result<std::string> tokenize(const char *line, const Place &place)
{
    if (std::strlen(line) > MAX_COMMAND_BYTES)
        return fail("command", xpp::format("Command exceeds {} bytes", MAX_COMMAND_BYTES), place);
    const char *cursor = json::skip_ws(line), *end = cursor;
    while (*end && !blank(*end)) ++end;
    const Command *command = find({cursor, static_cast<std::size_t>(end - cursor)});
    if (!command) return fail("command", "Unknown registry command", place);
    return tokenize_command(*command, end, place);
}

} // namespace xpp::command
