/* the keys: see xpp_keymap.h. */
#include "xpp_keymap.h"

#include "command_def.h"
#include "json_error.h"
#include "json_reader.h"
#include "xpp_text.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <expected>
#include <set>
#include <string>
#include <utility>

namespace xpp::keymap {

namespace {

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

void append_strings(std::string &out, const std::vector<std::string> &strings, std::string_view separator)
{
    for (std::size_t i = 0; i < strings.size(); i++) {
        if (i) out += separator;
        json_append_string(out, strings[i]);
    }
}

} // namespace

std::string key_problem(std::string_view key)
{
    if (key.empty()) return "it is empty";
    if (key.size() > MAX_KEY_BYTES) return xpp::format("it is longer than {} bytes", MAX_KEY_BYTES);
    if (key.find(' ') != std::string_view::npos) return "a binding is one key press, never a sequence of keys";
    if (!is_part(key)) return xpp::format("{} is not a key press: {}", json::shown(key), KEY_GRAMMAR);
    return {};
}

bool is_reserved_key(std::string_view key) { return std::ranges::find(RESERVED_KEYS, key) != RESERVED_KEYS.end(); }

std::vector<std::string> effective_keys(const Keymap &map, const command::Command &row)
{
    if (const auto it = map.bindings.find(std::string(row.name)); it != map.bindings.end()) return it->second;
    std::vector<std::string> keys;
    for (const char *key : row.default_keys) keys.emplace_back(key);
    return keys;
}

std::string bindings_json(const Keymap &map)
{
    std::string out = "{";
    bool first = true;
    for (const auto &[id, keys] : map.bindings) {
        out += first ? "\n      " : ",\n      ";
        first = false;
        json_append_string(out, id);
        out += ": [";
        append_strings(out, keys, ", ");
        out += "]";
    }
    return out + (first ? "}" : "\n    }");
}

std::string pinned_json(const Keymap &map)
{
    std::string out = "[";
    append_strings(out, map.pinned, ", ");
    return out + "]";
}

std::string effective_json(std::string_view path, const Result<Keymap> &loaded)
{
    const Keymap defaults;
    const Keymap &map = loaded ? *loaded : defaults;
    std::string out = loaded ? "{\"ok\":true" : "{\"ok\":false";
    if (!loaded) json::append_error(out, loaded.error());
    out += ",\"path\":";
    json_append_string(out, path);
    out += ",\"pinned\":[";
    append_strings(out, map.pinned, ",");
    out += "],\"reserved\":[";
    for (std::size_t i = 0; i < RESERVED_KEYS.size(); i++) {
        if (i) out += ',';
        json_append_string(out, RESERVED_KEYS[i]);
    }
    out += xpp::format("],\"limits\":{{\"keys\":{},\"key_bytes\":{}}},\"commands\":[", MAX_KEYS_PER_COMMAND, MAX_KEY_BYTES);
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
