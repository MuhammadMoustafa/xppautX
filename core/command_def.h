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
#include "json_number.h"
#include "xpp_error.h"
#include "xpp_io.h"
#include <array>
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
   list and map hold one level of `element` values (real_pair is exactly two finite reals, for search bounds; no recursive containers). A map's keys are names
   of its `names` kind, or any text. */
enum class Type { text, integer, real, boolean, choice, expression, model_name, file, path, color,
                  list, map, window, new_name, real_pair };
/* what a name argument may name in the model, matched as the model does
   (any case): a parameter, a variable (one with an initial condition),
   either, or any name the model defines. `none` for a map whose keys are
   plain text. `column`: a column of the stored data (what a plot axis takes): T, a variable, an
   auxiliary or a column the data browser added. */
enum class NameSet { none, parameter, variable, either, any, column };

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
using Scalar = std::variant<bool, long long, double, std::string, Color, std::array<double, 2>>;
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

/* a list of names as the C strings a row's choices are, made from the one list each choice has elsewhere (line_style.h,
   grobs.h, colormap.h, numerics_settings.h) so no name is written twice */
template <std::size_t N> constexpr std::array<const char *, N> c_names(const std::array<std::string_view, N> &names)
{
    std::array<const char *, N> out{};
    for (std::size_t i = 0; i < N; ++i) out[i] = names[i].data();
    return out;
}

/* a row's arguments made of two lists (a command that takes another's, then its own) */
template <std::size_t A, std::size_t B> constexpr std::array<Argument, A + B> join(const std::array<Argument, A> &a, const std::array<Argument, B> &b)
{
    std::array<Argument, A + B> out{};
    for (std::size_t i = 0; i < A; ++i) out[i] = a[i];
    for (std::size_t i = 0; i < B; ++i) out[A + i] = b[i];
    return out;
}

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

/* A command's result object, built member by member in the order its row
   declares them: `ResultObject().integer("rows", 3).text("status", "ok").done()`. */
class ResultObject {
public:
    ResultObject &boolean(std::string_view name, bool value) { return raw(name, value ? "true" : "false"); }
    ResultObject &integer(std::string_view name, long long value) { return raw(name, std::to_string(value)); }
    ResultObject &real(std::string_view name, double value)
    {
        std::string text;
        json::json_append_number_shortest(text, value);
        return raw(name, text);
    }
    ResultObject &text(std::string_view name, std::string_view value)
    {
        std::string quoted;
        json_append_string(quoted, value);
        return raw(name, quoted);
    }
    /* a member that is already JSON (an object, a list) */
    ResultObject &raw(std::string_view name, std::string_view json)
    {
        if (text_.size() > 1) text_ += ',';
        json_append_string(text_, name);
        text_ += ':';
        text_ += json;
        return *this;
    }
    std::string done() { return text_ + '}'; }

private:
    std::string text_ = "{";
};

/* Every resource's table, joined in command_registry.cpp's TABLES: a card
   that moves a resource's commands adds its file's function there. */
std::span<const Command> window_commands();
std::span<const Command> annotation_commands();
std::span<const Command> run_commands();
std::span<const Command> ic_commands();
std::span<const Command> bvp_commands();
std::span<const Command> auto_commands();
std::span<const Command> plot_commands();
std::span<const Command> analysis_commands();
std::span<const Command> table_commands();
/* A command whose arguments are numerics settings (run.configure, bvp.configure): every argument given is the
   setting `key_prefix` + its name, all checked together and set all or nothing; the result holds each one's
   value now. */
Result<std::string> configure_numerics(Session &s, const Command &command, const Arguments &args, const Place &place,
                                       std::string_view key_prefix);
/* bvp.configure's arguments are the settings' keys without this prefix */
inline constexpr std::string_view BVP_KEY_PREFIX = "bvp_";
/* the registry command that sets the numerics setting `key`, "" when none does (`set` takes the others) */
std::string_view numerics_command(std::string_view key);
std::span<const Command> equilibrium_commands();
std::span<const Command> nullcline_commands();
std::span<const Command> dirfield_commands();

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
