/* keymap.json: see xpp_keymap.h. */
#include "xpp_keymap.h"

#include "command_table.h"
#include "json_error.h"
#include "json_reader.h"
#include "xpp_files.h"
#include "xpp_io.h"

#include <algorithm>
#include <charconv>
#include <set>
#include <utility>

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

std::vector<std::string_view> parts_of(std::string_view key)
{
    std::vector<std::string_view> parts;
    for (std::size_t from = 0;;) {
        const std::size_t space = key.find(' ', from);
        parts.push_back(key.substr(from, space == std::string_view::npos ? space : space - from));
        if (space == std::string_view::npos) return parts;
        from = space + 1;
    }
}

/* a is b, or the start of b (a chord's first part), or b is the start of a */
bool keys_clash(std::string_view a, std::string_view b)
{
    const std::vector<std::string_view> pa = parts_of(a), pb = parts_of(b);
    return std::equal(pa.begin(), pa.begin() + static_cast<std::ptrdiff_t>(std::min(pa.size(), pb.size())), pb.begin());
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

bool is_command(const CommandRow &row) { return row.category != CommandCategory::Layer; }

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
            if (key == "bindings") read_bindings(value, map);
            else if (key == "pinned") read_pinned(value, map);
            else if (key == "preset") read_preset(value, map);
            else fail(value, xpp::format("unknown setting {} (the settings are bindings, pinned and preset)", quoted(key)));
        }
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
        for (std::string_view part : parts_of(key))
            if (is_reserved_key(part)) fail(v, xpp::format("{} is reserved (the browser or system uses it)", quoted(part)));
        return key;
    }

    const CommandRow &read_command(const std::string &id, const char *at) const
    {
        const CommandRow *row = find_command_by_id(id);
        if (!row) fail(at, xpp::format("unknown command {}", quoted(id)));
        return *row;
    }

    void read_bindings(const char *v, Keymap &map)
    {
        if (*v != '{') fail(v, xpp::format("bindings is an object of command id: keys, not {}", shown(v)));
        std::string id;
        const char *cursor = v, *value = nullptr;
        while (json::js_member(cursor, id, value)) {
            if (!is_command(read_command(id, value)))
                fail(value, xpp::format("{} only switches the shortcut layer, it takes no keys", quoted(id)));
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
            if (!read_command(id, elem).pinnable) fail(elem, xpp::format("{} cannot be pinned", quoted(id)));
            if (std::ranges::find(map.pinned, id) != map.pinned.end()) fail(elem, xpp::format("{} is pinned twice", quoted(id)));
            map.pinned.push_back(std::move(id));
        }
    }

    void read_preset(const char *v, Keymap &map) const
    {
        std::string preset;
        if (!json::js_string(v, preset) || (preset != PRESET_DEFAULT && preset != PRESET_XPPAUT))
            fail(v, xpp::format("the preset is \"{}\" or \"{}\", not {}", PRESET_DEFAULT, PRESET_XPPAUT, shown(v)));
        map.preset = std::move(preset);
    }

    /* the effective map has no two commands on one key (nor one key the start of another's chord):
       the table's keys of the commands the file leaves alone, then the file's, in its order */
    void check_clashes(const Keymap &map) const
    {
        struct Owner {
            std::string_view id, key;
        };
        std::vector<Owner> owners;
        for (const CommandRow &row : COMMANDS)
            if (!map.bindings.contains(std::string(row.id)))
                for (std::string_view key : row.default_keys)
                    if (!key.empty()) owners.push_back({row.id, key});
        for (const Placed &p : placed_) {
            for (const Owner &o : owners) {
                if (!keys_clash(o.key, p.key)) continue;
                if (o.id == p.id)
                    fail(p.at, xpp::format("{} and {} of {} clash (one is the start of the other)", quoted(o.key), quoted(p.key),
                                           quoted(p.id)));
                fail(p.at, xpp::format("{} is already {} ({}){}", quoted(p.key), find_command_by_id(o.id)->label, o.id,
                                       o.key == p.key ? "" : xpp::format(": its key {} starts it", quoted(o.key))));
            }
            owners.push_back({p.id, p.key});
        }
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
    const std::vector<std::string_view> parts = parts_of(key);
    if (parts.size() > MAX_CHORD_PARTS) return xpp::format("a key has at most {} parts", MAX_CHORD_PARTS);
    for (std::string_view part : parts)
        if (!is_part(part)) return xpp::format("{} is not a key press: {}", quoted(part), KEY_GRAMMAR);
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
    std::string out = "{\n  \"preset\": ";
    json_append_string(out, map.preset);
    out += ",\n  \"pinned\": [";
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

std::vector<std::string> effective_keys(const Keymap &map, std::string_view id)
{
    if (const auto it = map.bindings.find(std::string(id)); it != map.bindings.end()) return it->second;
    std::vector<std::string> keys;
    if (const CommandRow *row = find_command_by_id(id))
        for (std::string_view key : row->default_keys)
            if (!key.empty()) keys.emplace_back(key);
    return keys;
}

std::string path() { return files::config_path(FILE_NAME); }

Result<Keymap> load_keymap()
{
    const Result<std::optional<std::string>> text = read_config_file(WHERE, FILE_NAME, MAX_FILE_BYTES);
    if (!text) return std::unexpected(text.error());
    if (!*text) return Keymap{};
    return parse_keymap(**text, path());
}

Result<> save(const Keymap &map) { return write_config_file(WHERE, FILE_NAME, serialize(map)); }

Result<> reset()
{
    const std::string file = path();
    if (file.empty()) return std::unexpected(no_config_folder(WHERE, FILE_NAME));
    if (files::exists(file) && files::remove(file) != 0)
        return fail(std::string(WHERE), "cannot be reset: the file cannot be removed", Place{file});
    return {};
}

std::string effective_json(const Result<Keymap> &loaded)
{
    const Keymap defaults;
    const Keymap &map = loaded ? *loaded : defaults;
    std::string out = loaded ? "{\"ok\":true" : "{\"ok\":false";
    if (!loaded) json::append_error(out, loaded.error());
    out += ",\"path\":";
    json_append_string(out, path());
    out += ",\"preset\":";
    json_append_string(out, map.preset);
    out += ",\"pinned\":[";
    append_strings(out, map.pinned, ",");
    out += "],\"reserved\":[";
    for (std::size_t i = 0; i < RESERVED_KEYS.size(); i++) {
        if (i) out += ',';
        json_append_string(out, RESERVED_KEYS[i]);
    }
    out += xpp::format("],\"limits\":{{\"keys\":{},\"parts\":{},\"key_bytes\":{}}},\"commands\":[", MAX_KEYS_PER_COMMAND,
                       MAX_CHORD_PARTS, MAX_KEY_BYTES);
    bool first = true;
    for (const CommandRow &row : COMMANDS) {
        if (!is_command(row)) continue;
        if (!first) out += ',';
        first = false;
        out += "{\"id\":";
        json_append_string(out, row.id);
        out += ",\"keys\":[";
        append_strings(out, effective_keys(map, row.id), ",");
        out += map.bindings.contains(std::string(row.id)) ? "],\"source\":\"user\"}" : "],\"source\":\"default\"}";
    }
    return out + "]}";
}

} // namespace xpp::keymap
