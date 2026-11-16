/* keymap.json: see xpp_keymap.h. */
#include "xpp_keymap.h"
#include "format_version.h"

#include "command_def.h"
#include "json_error.h"
#include "json_reader.h"
#include "xpp_fs.h"
#include "xpp_lines.h"

#include <algorithm>
#include <charconv>
#include <set>
#include <utility>
#include <expected>
#include <span>
#include <system_error>

namespace xpp::keymap {

namespace {

constexpr std::string_view WHERE = "keymap"; /* the Error's `where` */

/* ---- the key grammar ----------------------------------------------------- */

/* the modifiers, in the one order they are written */
constexpr std::array<std::string_view, 4> MODIFIERS = {"Ctrl+", "Alt+", "Shift+", "Meta+"};

/* the keys that are not one printable character (DOM KeyboardEvent.key names, "Space" for " ") */
constexpr std::array<std::string_view, 15> NAMED_KEYS = {
    "Enter", "Escape", "Tab", "Backspace", "Delete", "Insert", "Home", "End", "PageUp", "PageDown",
    "ArrowLeft", "ArrowRight", "ArrowUp", "ArrowDown", "Space"};

/* F1 to F24: the function keys keyboards and the DOM have */
constexpr int MAX_FUNCTION_KEY = 24;

constexpr std::string_view KEY_GRAMMAR =
    "write Ctrl+, Alt+, Shift+ and Meta+ in that order, then a capital letter, a digit or symbol, F1 to F24, or a name "
    "such as Enter, Escape, ArrowLeft or Space";

bool is_function_key(std::string_view key)
{
    if (key.size() < 2 || key.size() > 3 || key[0] != 'F' || key[1] == '0') return false;
    int n = 0;
    const auto r = std::from_chars(key.data() + 1, key.data() + key.size(), n);
    return r.ec == std::errc() && r.ptr == key.data() + key.size() && n >= 1 && n <= MAX_FUNCTION_KEY;
}

bool is_base_key(std::string_view key)
{
    if (std::ranges::find(NAMED_KEYS, key) != NAMED_KEYS.end() || is_function_key(key)) return true;
    if (key.size() != 1) return false;
    const unsigned char c = static_cast<unsigned char>(key[0]);
    return c > ' ' && c < 0x7f && !(c >= 'a' && c <= 'z'); /* printable ASCII; letters in capitals */
}

bool is_part(std::string_view part)
{
    for (std::string_view modifier : MODIFIERS)
        if (part.starts_with(modifier) && part.size() > modifier.size()) part.remove_prefix(modifier.size());
    return is_base_key(part);
}

/* ---- reading ------------------------------------------------------------- */

constexpr std::size_t MAX_SHOWN = 40; /* a value in an error: the first line, cut here */

std::string cut(std::string_view text)
{
    const std::size_t newline = text.find('\n');
    if (newline != std::string_view::npos) text = text.substr(0, newline);
    return text.size() > MAX_SHOWN ? std::string(text.substr(0, MAX_SHOWN)) + "..." : std::string(text);
}

std::string quoted(std::string_view text) { return "\"" + cut(text) + "\""; }


/* one pass over the text: every value checked and put in a Keymap, which is
   the caller's only when the whole file was good (a problem throws ReadFailed) */
class Reader {
public:
    Reader(std::string_view text, std::string file) : text_(text), file_(std::move(file)) {}

    Keymap read()
    {
        const char *const start = text_.c_str();
        if (text_.size() > MAX_FILE_BYTES)
            fail(start + MAX_FILE_BYTES, xpp::format("the file is longer than {} bytes", MAX_FILE_BYTES));
        if (const std::size_t nul = text_.find('\0'); nul != std::string::npos)
            fail(start + nul, "the file contains a NUL byte");
        const char *at = start;
        if (const char *why = json::js_problem(start, at))
            fail(at, xpp::format("the file {}, here: {}", why, quoted(at)));
        const char *top = json::skip_ws(start);
        if (*top != '{') fail(top, xpp::format("the file must be one JSON object, not {}", quoted(top)));

        Keymap map;
        std::set<std::string> seen;
        std::string key;
        const char *cursor = top, *value = nullptr;
        while (json::js_member(cursor, key, value)) {
            if (!seen.insert(key).second) fail(value, xpp::format("{} is given twice", quoted(key)));
            if (key == version_word) read_version(value);
            else if (key == "bindings") read_bindings(value, map);
            else if (key == "pinned") read_pinned(value, map);
            else fail(value, xpp::format("unknown setting {} (the settings are version, bindings and pinned)", quoted(key)));
        }
        if (!seen.contains(std::string(version_word)))
            fail(top, xpp::format("it has no \"{}\": this program reads keymaps of version {}", version_word, format_version));
        check_clashes(map);
        return map;
    }

private:
    /* a key of the file's, where it is, for the clash check that comes last */
    struct Placed {
        std::string id, key;
        const char *at;
    };

    [[noreturn]] void fail(const char *at, std::string what) const
    {
        const char *const start = text_.c_str();
        Place place = line_place(file_, text_, json::js_line(start, at));
        const char *line_start = at;
        while (line_start > start && line_start[-1] != '\n') line_start--;
        place.col = static_cast<int>(at - line_start) + 1;
        throw ReadFailed{Error{std::string(WHERE), std::move(what), std::move(place)}};
    }

    std::string shown(const char *value) const { return quoted(json::js_raw(value)); }

    /* the string at v, a key string checked */
    std::string read_key(const char *v) const
    {
        std::string key;
        if (!json::js_string(v, key)) fail(v, xpp::format("a key is a string, not {}", shown(v)));
        if (const std::string problem = key_problem(key); !problem.empty())
            fail(v, xpp::format("{} is not a key: {}", quoted(key), problem));
        if (is_reserved_key(key)) fail(v, xpp::format("{} is reserved (the browser or system uses it)", quoted(key)));
        return key;
    }

    /* a binding or a pin names a command value (command_def.h): a command, with fixed arguments if it has any */
    void read_command(const std::string &id, const char *at) const
    {
        if (const Result<command::CommandValue> value = command::read_value(id, Place{}); !value)
            fail(at, xpp::format("{}: {}", quoted(id), value.error().what));
    }

    void read_version(const char *v) const
    {
        const std::string_view raw = json::js_raw(v);
        int found = 0;
        if (!xpp::parse_int(raw, found)) fail(v, xpp::format("the version is a whole number, not {}", shown(v)));
        if (found != format_version) fail(v, wrong_version(found, "keymaps"));
    }

    void read_bindings(const char *v, Keymap &map)
    {
        if (*v != '{') fail(v, xpp::format("bindings is an object of command id: keys, not {}", shown(v)));
        std::string id;
        const char *cursor = v, *value = nullptr;
        while (json::js_member(cursor, id, value)) {
            read_command(id, value);
            if (map.bindings.contains(id)) fail(value, xpp::format("{} is given twice", quoted(id)));
            if (*value != '[') fail(value, xpp::format("the keys of {} are an array of strings, not {}", quoted(id), shown(value)));
            std::vector<std::string> &keys = map.bindings[id];
            for (int i = 0; const char *elem = json::js_elem(value, i); i++) {
                if (keys.size() == MAX_KEYS_PER_COMMAND)
                    fail(elem, xpp::format("{} has more than {} keys", quoted(id), MAX_KEYS_PER_COMMAND));
                keys.push_back(read_key(elem));
                placed_.push_back({id, keys.back(), elem});
            }
        }
    }

    void read_pinned(const char *v, Keymap &map)
    {
        if (*v != '[') fail(v, xpp::format("pinned is an array of command ids, not {}", shown(v)));
        for (int i = 0; const char *elem = json::js_elem(v, i); i++) {
            std::string id;
            if (!json::js_string(elem, id)) fail(elem, xpp::format("a pinned command is a string, not {}", shown(elem)));
            read_command(id, elem);
            if (std::ranges::find(map.pinned, id) != map.pinned.end()) fail(elem, xpp::format("{} is pinned twice", quoted(id)));
            map.pinned.push_back(std::move(id));
        }
    }

    /* the effective map has no two commands on one key: the table's keys of the commands the file leaves alone,
       then the file's, in its order */
    struct Owner { std::string id, key; const char *at; };

    /* `key` of command value `id` is taken, or fails when another has it */
    void take(std::vector<Owner> &owners, const std::string &id, const std::string &key) const
    {
        const auto placed = std::find_if(placed_.rbegin(), placed_.rend(), [&](const Placed &p) { return p.id == id && p.key == key; });
        const char *at = placed == placed_.rend() ? text_.c_str() : placed->at;
        for (const Owner &owner : owners) {
            if (owner.key != key) continue;
            fail(std::max(owner.at, at), xpp::format("{} and {} are both on key {}", quoted(owner.id), quoted(id), quoted(key)));
        }
        owners.push_back({id, key, at});
    }

    void check_clashes(const Keymap &map) const
    {
        std::vector<Owner> owners;
        for (const command::Command *row : command::rows()) {
            const auto keys = effective_keys(map, *row);
            if (keys.size() > MAX_KEYS_PER_COMMAND) fail(text_.c_str(), xpp::format("{} has more than {} keys", quoted(row->name), MAX_KEYS_PER_COMMAND));
            for (const std::string &key : keys) take(owners, std::string(row->name), key);
        }
        for (const auto &[id, keys] : map.bindings)
            if (!command::find(id))
                for (const std::string &key : keys) take(owners, id, key);
    }

    std::string text_; /* NUL-terminated: the JSON reader reads pointers */
    std::string file_;
    std::vector<Placed> placed_;
};

} // namespace

std::string key_problem(std::string_view key)
{
    if (key.empty()) return "it is empty";
    if (key.size() > MAX_KEY_BYTES) return xpp::format("it is longer than {} bytes", MAX_KEY_BYTES);
    if (key.find(' ') != std::string_view::npos) return "a binding is one key press, never a sequence of keys";
    if (!is_part(key)) return xpp::format("{} is not a key press: {}", quoted(key), KEY_GRAMMAR);
    return {};
}

bool is_reserved_key(std::string_view key) { return std::ranges::find(RESERVED_KEYS, key) != RESERVED_KEYS.end(); }

Result<Keymap> parse_keymap(std::string_view text, std::string file)
{
    try {
        return Reader(text, std::move(file)).read();
    } catch (ReadFailed &e) {
        return std::unexpected<Error>(std::move(e.error));
    }
}

namespace {

void append_strings(std::string &out, const std::vector<std::string> &strings, std::string_view separator)
{
    for (std::size_t i = 0; i < strings.size(); i++) {
        if (i) out += separator;
        json_append_string(out, strings[i]);
    }
}

} // namespace

std::string serialize(const Keymap &map)
{
    std::string out = xpp::format("{{\n  \"{}\": {},\n  \"pinned\": [", version_word, format_version);
    append_strings(out, map.pinned, ", ");
    out += "],\n  \"bindings\": {";
    bool first = true;
    for (const auto &[id, keys] : map.bindings) {
        out += first ? "\n    " : ",\n    ";
        first = false;
        json_append_string(out, id);
        out += ": [";
        append_strings(out, keys, ", ");
        out += "]";
    }
    out += first ? "}\n}\n" : "\n  }\n}\n";
    return out;
}

std::vector<std::string> effective_keys(const Keymap &map, const command::Command &row)
{
    if (const auto it = map.bindings.find(std::string(row.name)); it != map.bindings.end()) return it->second;
    std::vector<std::string> keys;
    for (const char *key : row.default_keys) keys.emplace_back(key);
    return keys;
}

std::string path(FileSystem &fs) { return fs.config_path(FILE_NAME); }

Result<Keymap> load_keymap(FileSystem &fs)
{
    const auto loaded = load_config<Keymap>(fs, WHERE, FILE_NAME, MAX_FILE_BYTES, parse_keymap);
    if (!loaded) return loaded;
    return parse_keymap(serialize(*loaded), path(fs));
}

Result<> save(FileSystem &fs, const Keymap &map) { return fs.write_config(WHERE, FILE_NAME, serialize(map)); }

Result<> reset(FileSystem &fs) { return fs.remove_config(WHERE, FILE_NAME); }

std::string effective_json(FileSystem &fs, const Result<Keymap> &loaded)
{
    const Keymap defaults;
    const Keymap &map = loaded ? *loaded : defaults;
    std::string out = loaded ? "{\"ok\":true" : "{\"ok\":false";
    if (!loaded) json::append_error(out, loaded.error());
    out += ",\"path\":";
    json_append_string(out, path(fs));
    out += ",\"pinned\":[";
    append_strings(out, map.pinned, ",");
    out += "],\"reserved\":[";
    for (std::size_t i = 0; i < RESERVED_KEYS.size(); i++) {
        if (i) out += ',';
        json_append_string(out, RESERVED_KEYS[i]);
    }
    out += xpp::format("],\"limits\":{{\"keys\":{},\"key_bytes\":{}}},\"commands\":[", MAX_KEYS_PER_COMMAND,
                       MAX_KEY_BYTES);
    bool first = true;
    for (const command::Command *entry : command::rows()) {
        const command::Command &row = *entry;
        if (!first) out += ',';
        first = false;
        out += "{\"id\":";
        json_append_string(out, row.name);
        out += ",\"keys\":[";
        append_strings(out, effective_keys(map, row), ",");
        out += map.bindings.contains(std::string(row.name)) ? "],\"source\":\"user\"}" : "],\"source\":\"default\"}";
    }
    /* the command values (a command with arguments) the user bound, beside the rows, and every one bound or pinned with what the page sends to run it */
    std::set<std::string> values;
    for (const auto &[id, keys] : map.bindings) {
        if (command::find(id)) continue;
        out += ",{\"id\":";
        json_append_string(out, id);
        out += ",\"keys\":[";
        append_strings(out, keys, ",");
        out += "],\"source\":\"user\"}";
        values.insert(id);
    }
    values.insert(map.pinned.begin(), map.pinned.end());
    out += "],\"values\":[";
    first = true;
    for (const std::string &id : values) {
        const Result<command::CommandValue> value = command::read_value(id, Place{});
        if (!value || !value->has_arguments) continue;
        out += first ? "{\"value\":" : ",{\"value\":";
        first = false;
        json_append_string(out, id);
        out += ",\"run\":" + value->run + "}";
    }
    return out + "]}";
}

} // namespace xpp::keymap
