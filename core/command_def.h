#pragma once

/* The command registry's vocabulary (W308, W353, W530): what a command
   row (Command) and its arguments (Argument) are made of, and the validated
   arguments a handler receives (Arguments). A resource's rows live in its
   own file (commands_window.cpp), each file giving one table
   (`std::span<const Command>`) that command_registry.cpp joins; the
   validator (command_validate.cpp), the server text tokenizer
   (command_text.cpp) and help/hello (command_registry.cpp) read the same
   rows. A handler never reads the JSON line: it asks Arguments. */
#include "colormap.h"
#include "command_table.h"
#include "xpp_error.h"
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace xpp { struct Session; }
namespace xpp::command {

/* Registry requests describe small operations; bulk data uses /files. */
inline constexpr std::size_t MAX_COMMAND_BYTES = 16384;
/* Room for descriptive command names and opaque client correlation ids. */
inline constexpr std::size_t MAX_TEXT_BYTES = 1024;
/* The most items a list or map argument takes unless its row says fewer or more. */
inline constexpr std::size_t MAX_ITEMS = 256;
/* The largest whole number a JSON number carries exactly (2^53 - 1): an
   integer argument's bounds unless its row narrows them. */
inline constexpr long long MAX_INTEGER = 9007199254740991LL;
inline constexpr const char *COMMAND_KEY = "cmd";
inline constexpr const char *ID_KEY = "id";
/* the one command whose server text form takes its argument by position (`help NAME`) */
inline constexpr std::string_view HELP_COMMAND = "help";

/* integer and real are finite numbers; text, expression, name, file, path
   and color are JSON strings (max_length bytes at most); window and
   new_name are W353's: an open graph or view, and a name a window may take;
   list and map hold one level of `element` values (a map's keys are names
   of its `names` kind, or any text) */
enum class Type { text, integer, real, boolean, choice, expression, model_name, file, path, color,
                  list, map, window, new_name };
/* what a name argument may name in the model, matched as the model does
   (any case): a parameter, a variable (one with an initial condition),
   either, or any name the model defines. `none` for a map whose keys are
   plain text. */
enum class NameSet { none, parameter, variable, either, any };

/* One end of a number's range: nothing, a constant, or a quantity of the
   session (a run's end time, the AUTO threads' maximum) that a callback
   reads when the command is checked. hello names the quantity; a check
   with no session (a recording's shape check) skips it. */
struct Bound {
    std::optional<double> value = std::nullopt;
    const char *quantity = nullptr;
    double (*of)(const Session &) = nullptr;
};
constexpr Bound constant(double value) { return Bound{value, nullptr, nullptr}; }
constexpr Bound session_quantity(const char *name, double (*of)(const Session &))
{
    return Bound{std::nullopt, name, of};
}

struct Argument {
    const char *name;
    Type type;
    bool required;
    std::size_t max_length = MAX_TEXT_BYTES;
    std::span<const char *const> choices = {};
    /* integer and real, and a list's or map's numbers */
    Bound min = {};
    Bound max = {};
    /* a model_name, a list of names, or a map's keys */
    NameSet names = NameSet::none;
    /* a list's or map's values: any scalar type above */
    Type element = Type::text;
    std::size_t max_items = MAX_ITEMS;
};
/* Arguments that go together: exactly one of them, at most one, or all or
   none. A grouped argument is not `required`. */
enum class GroupRule { exactly_one, at_most_one, together };
struct Group {
    GroupRule rule;
    std::span<const char *const> arguments;
};
struct ResultField {
    const char *name;
    const char *type;
};

/* A value an argument takes: a scalar, a list of scalars, or a map of
   scalars by name, in the order written. */
using Scalar = std::variant<bool, long long, double, std::string, Color>;
using Entry = std::pair<std::string, Scalar>;
using Value = std::variant<Scalar, std::vector<Scalar>, std::vector<Entry>>;

/* The validated arguments of one command, typed by their rows (never the
   JSON). T is bool, long long (integer), double (real), std::string (text,
   choice, expression, name, file, path, window, new_name) or Color. A
   required argument is always there; one a row marks optional is asked with
   optional() or has(). Asking for a name the command did not give, or in a
   type its row does not declare, is the handler's bug and throws. */
class Arguments {
public:
    bool has(std::string_view name) const { return find(name) != nullptr; }
    template <class T> const T &get(std::string_view name) const
    {
        return std::get<T>(std::get<Scalar>(need(name)));
    }
    template <class T> std::optional<T> optional(std::string_view name) const
    {
        const Value *value = find(name);
        if (!value) return std::nullopt;
        return std::get<T>(std::get<Scalar>(*value));
    }
    /* a list's items (empty when not given) */
    template <class T> std::vector<T> list(std::string_view name) const
    {
        std::vector<T> items;
        if (const Value *value = find(name))
            for (const Scalar &item : std::get<std::vector<Scalar>>(*value)) items.push_back(std::get<T>(item));
        return items;
    }
    /* a map's entries (empty when not given) */
    template <class T> std::vector<std::pair<std::string, T>> map(std::string_view name) const
    {
        std::vector<std::pair<std::string, T>> entries;
        if (const Value *value = find(name))
            for (const Entry &entry : std::get<std::vector<Entry>>(*value))
                entries.emplace_back(entry.first, std::get<T>(entry.second));
        return entries;
    }
    /* the validator's: store what the row's type made of the argument */
    void add(std::string name, Value value) { given_.emplace_back(std::move(name), std::move(value)); }

private:
    const Value *find(std::string_view name) const
    {
        for (const auto &[given, value] : given_)
            if (given == name) return &value;
        return nullptr;
    }
    const Value &need(std::string_view name) const;
    std::vector<std::pair<std::string, Value>> given_;
};

struct Command;
/* A handler runs after validation: the session, its row, the typed
   arguments and the command's place (for the errors it returns). */
using Handler = Result<std::string> (*)(Session &, const Command &, const Arguments &, const Place &);
struct Command {
    const char *name;
    std::span<const Argument> arguments;
    char kind;
    CommandCategory category;
    const char *menu_path;
    const char *legacy_keys;
    const char *description;
    std::span<const ResultField> results;
    std::span<const char *const> events;
    bool step;
    Handler execute;
    std::span<const Group> groups = {};
};

/* Every resource's table, joined in command_registry.cpp's TABLES: a card
   that moves a resource's commands adds its file's function there. */
std::span<const Command> window_commands();

/* Check the line's arguments against the row and give them typed, or the
   first fault (all or nothing: a handler runs only on success). session
   is null for a recording's shape check, which skips what needs the model
   or the session. paths_allowed: the line came from --server's own input,
   the only origin a `path` argument may come from. */
Result<Arguments> validate(Session *session, const Command &command, const char *line, const Place &place,
                           bool paths_allowed);
/* The registry's row of that name, null when there is none. */
const Command *find(std::string_view name);
/* hello and help name a type this way. */
const char *type_name(Type type);
/* The row in the server text form: the `--name value` words after the command's name into the one JSON object the validator reads. */
Result<std::string> tokenize_command(const Command &command, const char *words, const Place &place);
/* The row as hello and help describe it, a JSON object appended to out. */
void append_command(std::string &out, const Command &command);
/* Whether the client id is text of at most MAX_TEXT_BYTES. */
bool valid_id(const char *value);

} // namespace xpp::command
