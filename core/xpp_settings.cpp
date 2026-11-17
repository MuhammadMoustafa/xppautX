/* settings.json and state.json: see xpp_settings.h. */
#include "xpp_settings.h"

#include "command_registry.h"
#include "command_def.h"
#include "format_version.h"
#include "json_error.h"
#include "json_reader.h"
#include "model_options.h" /* choice_index */
#include "xpp_fs.h"
#include "xpp_keymap.h"
#include "xpp_lines.h"
#include "xpp_text.h"
#include "xpp_util.h" /* utc_now */

#include <algorithm>
#include <cctype>
#include <expected>
#include <set>
#include <span>
#include <utility>

namespace xpp::settings {

namespace {

constexpr std::string_view WHERE = "settings"; /* the Error's `where`, both files */
constexpr std::string_view KEYMAP_SENT = "the keymap sent";
constexpr std::string_view SETTING_SENT = "the setting sent";

/* ---- what a value may be -------------------------------------------------- */

bool has_control(std::string_view text)
{
    return std::ranges::any_of(text, [](char c) { return static_cast<unsigned char>(c) < ' ' || c == 0x7f; });
}

/* why `path` cannot be a path the file keeps, or nothing */
std::optional<std::string> path_problem(std::string_view path)
{
    if (path.empty()) return "the path is empty";
    if (path.size() > MAX_PATH_BYTES) return xpp::format("the path is longer than {} bytes", MAX_PATH_BYTES);
    if (has_control(path)) return "the path has a control character";
    if (!files::is_absolute(path)) return xpp::format("{} is not an absolute path", json::shown(path));
    return std::nullopt;
}

/* why `path` cannot be the next recent model after `before`, or nothing */
std::optional<std::string> recent_problem(std::span<const std::string> before, std::string_view path)
{
    if (const auto why = path_problem(path)) return why;
    if (std::ranges::find(before, path) != before.end()) return xpp::format("{} is listed twice", json::shown(path));
    return std::nullopt;
}

/* why `text` is not a time as utc_now writes it (2026-11-10T14:03:00Z), or nothing; "" is never set */
std::optional<std::string> stamp_problem(std::string_view text)
{
    if (text.empty()) return std::nullopt;
    constexpr std::string_view SHAPE = "dddd-dd-ddTdd:dd:ddZ";
    bool fits = text.size() == SHAPE.size();
    for (std::size_t i = 0; fits && i < SHAPE.size(); i++)
        fits = SHAPE[i] == 'd' ? (text[i] >= '0' && text[i] <= '9') : text[i] == SHAPE[i];
    if (fits) return std::nullopt;
    return xpp::format("{} is not a time like 2026-11-10T14:03:00Z", json::shown(text));
}

/* why `id` cannot be the next folded section after `before`, or nothing */
std::optional<std::string> fold_problem(std::span<const std::string> before, std::string_view id)
{
    if (id.empty()) return "a folded section has no id";
    if (id.size() > MAX_FOLD_BYTES) return xpp::format("the section id is longer than {} bytes", MAX_FOLD_BYTES);
    if (has_control(id)) return "the section id has a control character";
    if (std::ranges::find(before, id) != before.end()) return xpp::format("{} is listed twice", json::shown(id));
    return std::nullopt;
}

/* why `a` cannot be the next "Allow always" answer after `before`, or nothing;*/
std::optional<std::string> approval_problem(std::span<const OutsideApproval> before, const OutsideApproval &a)
{
    if (const auto why = path_problem(a.model)) return why;
    if (const auto why = path_problem(a.path)) return why;
    if (std::ranges::find(before, a) != before.end()) return xpp::format("{} for {} is listed twice", json::shown(a.path), json::shown(a.model));
    return std::nullopt;
}

/* ---- reading ------------------------------------------------------------- */

/* a file's text, its comments blanked, read by pointers into it: the reader's own copy (NUL-terminated) */
class Doc {
public:
    Doc(std::string text, std::string file) : text_(std::move(text)), file_(std::move(file)) {}
    const char *start() const { return text_.c_str(); }
    const std::string &text() const { return text_; }
    [[noreturn]] void fail(const char *at, std::string what) const
    {
        throw ReadFailed{Error{std::string(WHERE), std::move(what), json::place_at(file_, text_, at)}};
    }

private:
    std::string text_;
    std::string file_;
};

/* the checks every file of ours passes before its values are read: the size, no NUL byte, the JSON grammar, one object */
Doc prepare(std::string_view text, const std::string &file)
{
    if (text.size() > MAX_FILE_BYTES) {
        Place place;
        place.file = file;
        place.line = place.col = 1;
        throw ReadFailed{Error{std::string(WHERE), xpp::format("the file is longer than {} bytes", MAX_FILE_BYTES), place}};
    }
    std::string copy(text);
    std::size_t open = 0;
    const bool closed = json::blank_comments(copy, open);
    Doc d(std::move(copy), file);
    if (const std::size_t nul = text.find('\0'); nul != std::string_view::npos) d.fail(d.start() + nul, "the file contains a NUL byte");
    if (!closed) d.fail(d.start() + open, "a comment /* is never closed");
    const char *at = d.start();
    if (const char *why = json::js_problem(d.start(), at)) d.fail(at, xpp::format("the file {}, here: {}", why, json::shown(at)));
    if (*json::skip_ws(d.start()) != '{') d.fail(json::skip_ws(d.start()), xpp::format("the file must be one JSON object, not {}", json::shown(json::skip_ws(d.start()))));
    return d;
}

/* each member of the object at obj, in order: its key (decoded) and the value; a key given twice is an error */
template <class F> void each_member(const Doc &d, const char *obj, F &&f)
{
    if (*obj != '{') d.fail(obj, xpp::format("an object, not {}", json::shown(json::js_raw(obj))));
    std::set<std::string> seen;
    std::string key;
    const char *cursor = obj, *value = nullptr;
    while (json::js_member(cursor, key, value)) {
        if (!seen.insert(key).second) d.fail(value, xpp::format("{} is given twice", json::shown(key)));
        f(key, value);
    }
}

[[noreturn]] void unknown(const Doc &d, const char *value, std::string_view group, const std::string &key)
{
    d.fail(value, xpp::format("unknown setting {} in {}", json::shown(key), json::shown(group)));
}

std::string string_value(const Doc &d, const char *v, std::string_view key)
{
    std::string out;
    if (!json::js_string(v, out)) d.fail(v, xpp::format("{} is a string, not {}", key, json::shown(json::js_raw(v))));
    return out;
}

bool bool_value(const Doc &d, const char *v, std::string_view key)
{
    const std::string_view raw = json::js_raw(v);
    if (raw == "true") return true;
    if (raw == "false") return false;
    d.fail(v, xpp::format("{} is true or false, not {}", key, json::shown(raw)));
}

/* a list of strings: each checked by problem, which sees the entries before it */
using StringProblem = std::optional<std::string> (*)(std::span<const std::string>, std::string_view);
std::vector<std::string> string_list(const Doc &d, const char *arr, std::string_view key, std::size_t max, StringProblem problem)
{
    if (*arr != '[') d.fail(arr, xpp::format("{} is an array of strings, not {}", key, json::shown(json::js_raw(arr))));
    std::vector<std::string> out;
    const char *cursor = arr;
    for (const char *elem = json::js_element(cursor); elem; elem = json::js_element(cursor)) {
        if (out.size() == max) d.fail(elem, xpp::format("{} has more than {} entries", key, max));
        std::string text;
        if (!json::js_string(elem, text)) d.fail(elem, xpp::format("an entry of {} is a string, not {}", key, json::shown(json::js_raw(elem))));
        if (const auto why = problem(out, text)) d.fail(elem, *why);
        out.push_back(std::move(text));
    }
    return out;
}

void read_version(const Doc &d, const char *v, std::string_view kind)
{
    const std::string_view raw = json::js_raw(v);
    int found = 0;
    if (!xpp::parse_int(raw, found)) d.fail(v, xpp::format("the version is a whole number, not {}", json::shown(raw)));
    if (found != format_version) d.fail(v, wrong_version(found, kind));
}

/* the members of the file's top-level object, all or nothing (W125): version is checked, every other member goes to take */
template <class Take> void read_top(const Doc &d, std::string_view kind, Take &&take)
{
    const char *top = json::skip_ws(d.start());
    bool versioned = false;
    each_member(d, top, [&](const std::string &key, const char *value) {
        if (key == version_word) {
            read_version(d, value, kind);
            versioned = true;
        } else {
            take(key, value);
        }
    });
    if (!versioned) d.fail(top, xpp::format("it has no \"{}\": this program reads {} of version {}", version_word, kind, format_version));
}

/* ---- settings.json's groups ------------------------------------------------ */

void read_threads(const Doc &d, const char *v, unsigned &out)
{
    if (*v == '"') {
        if (string_value(d, v, "threads") != cpu::THREADS_AUTO_WORD)
            d.fail(v, xpp::format("threads is \"{}\" or a whole number, not {}", cpu::THREADS_AUTO_WORD, json::shown(json::js_raw(v))));
        out = cpu::THREADS_AUTO;
        return;
    }
    int n = 0;
    if (!xpp::parse_int(json::js_raw(v), n)) d.fail(v, xpp::format("threads is \"{}\" or a whole number, not {}", cpu::THREADS_AUTO_WORD, json::shown(json::js_raw(v))));
    if (const auto problem = cpu::threads_problem(n)) d.fail(v, *problem);
    out = static_cast<unsigned>(n);
}

void read_general(const Doc &d, const char *obj, Settings &s)
{
    each_member(d, obj, [&](const std::string &key, const char *v) {
        if (key == "threads") {
            read_threads(d, v, s.threads);
        } else if (key == "log_level") {
            const std::string text = string_value(d, v, key);
            const int i = choice_index(log_level_names, text);
            if (i < 0) d.fail(v, xpp::format("log_level: {} is none of error, warn, info or debug", json::shown(text)));
            s.log_level = static_cast<XppLogLevel>(i);
        } else if (key == "log_to_file") {
            s.log_to_file = bool_value(d, v, key);
        } else {
            unknown(d, v, "general", key);
        }
    });
}

void read_colors(const Doc &d, const char *obj, Colors &c)
{
    each_member(d, obj, [&](const std::string &key, const char *v) {
        const auto row = std::ranges::find_if(COLOR_KEYS, [&](const ColorKey &k) { return k.key == key; });
        if (row == COLOR_KEYS.end()) unknown(d, v, "colors", key);
        const std::string text = string_value(d, v, key);
        const auto color = parse_color(text);
        if (!color) d.fail(v, xpp::format("{}: {} is not a color (a name, or \"#rrggbb\")", key, json::shown(text)));
        c.*(row->member) = *color;
    });
}

/* the keymap group: its bindings and pinned commands, each checked as keymap.h says (the key grammar, the reserved keys, a command
   that exists, no key on two commands) */
class KeymapReader {
public:
    KeymapReader(const Doc &d, keymap::Keymap &map) : d_(d), map_(map) {}

    void read(const char *obj)
    {
        each_member(d_, obj, [&](const std::string &key, const char *v) {
            if (key == "bindings") read_bindings(v);
            else if (key == "pinned") read_pinned(v);
            else unknown(d_, v, "keymap", key);
        });
        check_clashes();
    }

private:
    /* a key of the file's, where it is, for the clash check that comes last */
    struct Placed {
        std::string id, key;
        const char *at;
    };
    /* a key of command value `id` on a key the check has seen: the owner's place and the new one's, for the error */
    struct Owner {
        std::string id, key;
        const char *at;
    };

    [[noreturn]] void fail(const char *at, std::string what) const { d_.fail(at, std::move(what)); }

    std::string read_key(const char *v) const
    {
        const std::string key = string_value(d_, v, "a key");
        if (const std::string problem = keymap::key_problem(key); !problem.empty())
            fail(v, xpp::format("{} is not a key: {}", json::shown(key), problem));
        if (keymap::is_reserved_key(key)) fail(v, xpp::format("{} is reserved (the browser or system uses it)", json::shown(key)));
        return key;
    }

    /* a binding or a pin names a command value (command_def.h): a command, with fixed arguments if it has any */
    void read_command(const std::string &id, const char *at) const
    {
        if (const Result<command::CommandValue> value = command::read_value(id, Place{}); !value)
            fail(at, xpp::format("{}: {}", json::shown(id), value.error().what));
    }

    void read_bindings(const char *v)
    {
        if (*v != '{') fail(v, xpp::format("bindings is an object of command id: keys, not {}", json::shown(json::js_raw(v))));
        map_.bindings.clear(); /* a file that holds bindings holds them whole (a folder's replace the user's) */
        std::string id;
        const char *cursor = v, *value = nullptr;
        while (json::js_member(cursor, id, value)) {
            if (map_.bindings.size() == keymap::MAX_BINDINGS) fail(value, xpp::format("bindings has more than {} commands", keymap::MAX_BINDINGS));
            read_command(id, value);
            if (map_.bindings.contains(id)) fail(value, xpp::format("{} is given twice", json::shown(id)));
            if (*value != '[') fail(value, xpp::format("the keys of {} are an array of strings, not {}", json::shown(id), json::shown(json::js_raw(value))));
            std::vector<std::string> &keys = map_.bindings[id];
            const char *cur = value;
            for (const char *elem = json::js_element(cur); elem; elem = json::js_element(cur)) {
                if (keys.size() == keymap::MAX_KEYS_PER_COMMAND)
                    fail(elem, xpp::format("{} has more than {} keys", json::shown(id), keymap::MAX_KEYS_PER_COMMAND));
                keys.push_back(read_key(elem));
                placed_.push_back({id, keys.back(), elem});
            }
        }
    }

    void read_pinned(const char *v)
    {
        if (*v != '[') fail(v, xpp::format("pinned is an array of command ids, not {}", json::shown(json::js_raw(v))));
        map_.pinned.clear();
        const char *cur = v;
        for (const char *elem = json::js_element(cur); elem; elem = json::js_element(cur)) {
            if (map_.pinned.size() == keymap::MAX_PINNED) fail(elem, xpp::format("pinned has more than {} commands", keymap::MAX_PINNED));
            std::string id;
            if (!json::js_string(elem, id)) fail(elem, xpp::format("a pinned command is a string, not {}", json::shown(json::js_raw(elem))));
            read_command(id, elem);
            if (std::ranges::find(map_.pinned, id) != map_.pinned.end()) fail(elem, xpp::format("{} is pinned twice", json::shown(id)));
            map_.pinned.push_back(std::move(id));
        }
    }

    /* `key` of command value `id` is taken, or fails when another has it */
    void take(std::vector<Owner> &owners, const std::string &id, const std::string &key) const
    {
        const auto placed = std::find_if(placed_.rbegin(), placed_.rend(), [&](const Placed &p) { return p.id == id && p.key == key; });
        const char *at = placed == placed_.rend() ? d_.start() : placed->at;
        for (const Owner &owner : owners) {
            if (owner.key != key) continue;
            fail(std::max(owner.at, at), xpp::format("{} and {} are both on key {}", json::shown(owner.id), json::shown(id), json::shown(key)));
        }
        owners.push_back({id, key, at});
    }

    /* the effective map has no two commands on one key: the table's keys of the commands the file leaves alone, then the file's, in its order */
    void check_clashes() const
    {
        std::vector<Owner> owners;
        for (const command::Command *row : command::rows()) {
            const auto keys = keymap::effective_keys(map_, *row);
            if (keys.size() > keymap::MAX_KEYS_PER_COMMAND)
                fail(d_.start(), xpp::format("{} has more than {} keys", json::shown(row->name), keymap::MAX_KEYS_PER_COMMAND));
            for (const std::string &key : keys) take(owners, std::string(row->name), key);
        }
        for (const auto &[id, keys] : map_.bindings)
            if (!command::find(id))
                for (const std::string &key : keys) take(owners, id, key);
    }

    const Doc &d_;
    keymap::Keymap &map_;
    std::vector<Placed> placed_;
};

void read_page(const Doc &d, const char *obj, Page &p)
{
    each_member(d, obj, [&](const std::string &key, const char *v) {
        if (key == "theme") {
            const std::string text = string_value(d, v, key);
            if (choice_index(THEMES, text) < 0) d.fail(v, xpp::format("theme: {} is none of system, light or dark", json::shown(text)));
            p.theme = text;
        } else if (key == "dock_hidden") {
            p.dock_hidden = bool_value(d, v, key);
        } else if (key == "dock_width") {
            if (json::js_raw(v) == "null") {
                p.dock_width.reset();
                return;
            }
            int n = 0;
            if (!xpp::parse_int(json::js_raw(v), n)) d.fail(v, xpp::format("dock_width is null or a whole number, not {}", json::shown(json::js_raw(v))));
            if (n < MIN_DOCK_WIDTH || n > MAX_DOCK_WIDTH)
                d.fail(v, xpp::format("dock_width is {} to {} pixels, not {}", MIN_DOCK_WIDTH, MAX_DOCK_WIDTH, n));
            p.dock_width = n;
        } else if (key == "folds") {
            p.folds = string_list(d, v, key, MAX_FOLDS, fold_problem);
        } else {
            unknown(d, v, "page", key);
        }
    });
}

void read_updates(const Doc &d, const char *obj, bool &check, bool folder)
{
    each_member(d, obj, [&](const std::string &key, const char *v) {
        if (key == "check_for_updates" && folder) d.fail(v, "a model folder's settings cannot turn the update check on or off: it is the user's (their settings.json)");
        else if (key == "check_for_updates") check = bool_value(d, v, key);
        else unknown(d, v, "updates", key);
    });
}

/* the settings of a file on top of `s` (the defaults for the user's file, the user's for a model folder's, whose `folder` refuses the update check) */
Settings read_settings_file(const Doc &d, Settings s, bool folder)
{
    read_top(d, "settings files", [&](const std::string &key, const char *v) {
        if (key == "general") read_general(d, v, s);
        else if (key == "colors") read_colors(d, v, s.colors);
        else if (key == "keymap") KeymapReader(d, s.keymap).read(v);
        else if (key == "page") read_page(d, v, s.page);
        else if (key == "updates") read_updates(d, v, s.check_for_updates, folder);
        else d.fail(v, xpp::format("unknown group {} (the groups are general, colors, keymap, page and updates)", json::shown(key)));
    });
    return s;
}

/* ---- state.json's values --------------------------------------------------- */

void read_approvals(const Doc &d, const char *arr, std::vector<OutsideApproval> &out)
{
    if (*arr != '[') d.fail(arr, xpp::format("outside_paths is an array of objects, not {}", json::shown(json::js_raw(arr))));
    const char *cursor = arr;
    for (const char *elem = json::js_element(cursor); elem; elem = json::js_element(cursor)) {
        if (out.size() == MAX_OUTSIDE_APPROVALS) d.fail(elem, xpp::format("outside_paths has more than {} entries", MAX_OUTSIDE_APPROVALS));
        OutsideApproval a;
        bool model = false, path = false;
        each_member(d, elem, [&](const std::string &key, const char *v) {
            if (key == "model") {
                a.model = string_value(d, v, key);
                model = true;
            } else if (key == "path") {
                a.path = string_value(d, v, key);
                path = true;
            } else {
                unknown(d, v, "outside_paths", key);
            }
        });
        if (!model || !path) d.fail(elem, "an entry of outside_paths needs a model and a path");
        if (const auto why = approval_problem(out, a)) d.fail(elem, *why);
        out.push_back(std::move(a));
    }
}

/* ---- where a key is set ---------------------------------------------------- */

/* "group.key" of every member of every group of the file text, nothing for a text that is not a settings file */
std::vector<std::string> keys_in(const std::optional<std::string> &text, const std::string &file)
{
    std::vector<std::string> keys;
    if (!text) return keys;
    try {
        const Doc d = prepare(*text, file);
        each_member(d, json::skip_ws(d.start()), [&](const std::string &group, const char *v) {
            if (*v != '{') return;
            each_member(d, v, [&](const std::string &key, const char *) { keys.push_back(group + "." + key); });
        });
    } catch (ReadFailed &) {
        keys.clear();
    }
    return keys;
}

/* ---- writing ---------------------------------------------------------------- */

void append_strings(std::string &out, const std::vector<std::string> &strings)
{
    for (std::size_t i = 0; i < strings.size(); i++) {
        if (i) out += ", ";
        json_append_string(out, strings[i]);
    }
}

std::string state_text(const State &s)
{
    std::string out = xpp::format("{{\n  \"{}\": {},\n  \"recent_models\": [", version_word, format_version);
    append_strings(out, s.recent_models);
    out += "],\n  \"last_update_check\": ";
    json_append_string(out, s.last_update_check);
    out += ",\n  \"outside_paths\": [";
    for (std::size_t i = 0; i < s.outside_paths.size(); i++) {
        if (i) out += ", ";
        out += "{\"model\": ";
        json_append_string(out, s.outside_paths[i].model);
        out += ", \"path\": ";
        json_append_string(out, s.outside_paths[i].path);
        out += "}";
    }
    return out + "]\n}\n";
}

/* the value of one change as a whole file of its own, checked as a file of ours (its errors name `file`) */
Result<> check_text(const std::string &text, const std::string &file, bool folder = false)
{
    const Result<Settings> checked = folder ? parse_folder_settings(text, file, Settings{}) : parse_settings(text, file);
    if (!checked) return std::unexpected(checked.error());
    return {};
}

std::string one_change_text(const Change &c)
{
    std::string out = xpp::format("{{\n  \"{}\": {},\n  ", version_word, format_version);
    json_append_string(out, c.group);
    out += ": {\n    ";
    json_append_string(out, c.key);
    out += ": " + c.json + "\n  }\n}\n";
    return out;
}

/* the known group and key a change names, else the error: settings.set and the keymap refuse what the file would not hold */
Result<> check_change(const Change &c, bool folder)
{
    return check_text(one_change_text(c), std::string(SETTING_SENT), folder);
}

/* the change applied to `text` in place: the value of its key replaced where it is, else the member added before the closing brace of
   its group (the group made at the end of the file when it is not there); every other byte, comment and space stays */
void apply_change(std::string &text, const Change &c)
{
    std::string shadow = text;
    std::size_t open = 0;
    json::blank_comments(shadow, open); /* the text was checked, so every comment closes */
    const char *base = shadow.c_str();
    const char *top = json::skip_ws(base);
    const char *group = json::js_find(top, c.group.c_str());
    if (group) {
        if (const char *value = json::js_find(group, c.key.c_str())) {
            const std::string_view raw = json::js_raw(value);
            text.replace(static_cast<std::size_t>(raw.data() - base), raw.size(), c.json);
            return;
        }
    }
    const char *obj = group ? group : top;
    const char *last = json::skip_value(obj) - 2; /* the last character before the object's closing brace */
    while (last > obj && std::isspace(static_cast<unsigned char>(*last))) last--;
    const bool empty = *last == '{';
    std::string member;
    json_append_string(member, c.key);
    member += ": " + c.json;
    std::string add = empty ? std::string() : std::string(",");
    if (group) {
        add += "\n    " + member;
    } else {
        std::string name;
        json_append_string(name, c.group);
        add += "\n  " + name + ": {\n    " + member + "\n  }";
    }
    text.insert(static_cast<std::size_t>(last - base) + 1, add);
}

/* the member `key` of `group` taken out of `text` in place (nothing when it is not there): its comma goes with it, the line when it held
   nothing else, every comment and other member stays. true when something was removed. */
bool remove_member(std::string &text, std::string_view group, std::string_view key)
{
    std::string shadow = text;
    std::size_t open = 0;
    json::blank_comments(shadow, open); /* the text was checked, so every comment closes */
    const char *base = shadow.c_str();
    const std::string group_name(group), key_name(key);
    const char *obj = json::js_find(json::skip_ws(base), group_name.c_str());
    if (!obj) return false;
    const char *value = json::js_find(obj, key_name.c_str());
    if (!value) return false;
    std::size_t end = static_cast<std::size_t>(value - base) + json::js_raw(value).size();
    std::size_t begin = static_cast<std::size_t>(value - base);
    while (begin > 0 && std::isspace(static_cast<unsigned char>(shadow[begin - 1]))) begin--;
    if (begin > 0 && shadow[begin - 1] == ':') begin--;
    while (begin > 0 && std::isspace(static_cast<unsigned char>(shadow[begin - 1]))) begin--;
    if (begin > 0 && shadow[begin - 1] == '"') { /* the key's closing quote: back to its opening one (a backslash escapes) */
        begin--;
        while (begin > 0 && !(shadow[begin - 1] == '"' && (begin < 2 || shadow[begin - 2] != '\\'))) begin--;
        if (begin > 0) begin--;
    }
    /* the comma after it goes (a comment beside it is not blanked here: it stays); when there is none, the one before it (the member was the last) */
    std::size_t after = end;
    while (after < text.size() && (text[after] == ' ' || text[after] == '\t')) after++;
    std::size_t comma_before = std::string::npos;
    if (after < text.size() && text[after] == ',') {
        end = after + 1;
    } else {
        std::size_t back = begin;
        while (back > 0 && std::isspace(static_cast<unsigned char>(shadow[back - 1]))) back--; /* past a comment too */
        if (back > 0 && shadow[back - 1] == ',') comma_before = back - 1;
    }
    /* the line goes too when nothing else is on it */
    std::size_t line_begin = begin, line_end = end;
    while (line_begin > 0 && (text[line_begin - 1] == ' ' || text[line_begin - 1] == '\t')) line_begin--;
    while (line_end < text.size() && (text[line_end] == ' ' || text[line_end] == '\t' || text[line_end] == '\r')) line_end++;
    if ((line_begin == 0 || text[line_begin - 1] == '\n') && line_end < text.size() && text[line_end] == '\n') {
        begin = line_begin;
        end = line_end + 1;
    }
    text.erase(begin, end - begin);
    if (comma_before != std::string::npos) text.erase(comma_before, 1);
    return true;
}

/* the text of settings.json, a new file's template when there is none */
std::string settings_template() { return xpp::format("{{\n  \"{}\": {}\n}}\n", version_word, format_version); }

Result<> write_state(FileSystem &fs, const State &s)
{
    const Result<std::string> text = serialize_state(s, state_path(fs));
    if (!text) return std::unexpected(text.error());
    return fs.write_config(WHERE, STATE_FILE, *text);
}

} // namespace

Result<Settings> parse_settings(std::string_view text, std::string file)
{
    try {
        const Doc d = prepare(text, file);
        return read_settings_file(d, Settings{}, false);
    } catch (ReadFailed &e) {
        return std::unexpected(std::move(e.error));
    }
}

Result<Settings> parse_folder_settings(std::string_view text, std::string file, const Settings &user)
{
    try {
        const Doc d = prepare(text, file);
        return read_settings_file(d, user, true);
    } catch (ReadFailed &e) {
        return std::unexpected(std::move(e.error));
    }
}

Result<State> parse_state(std::string_view text, std::string file)
{
    try {
        const Doc d = prepare(text, file);
        State s;
        read_top(d, "state files", [&](const std::string &key, const char *v) {
            if (key == "recent_models") s.recent_models = string_list(d, v, key, MAX_RECENT_MODELS, recent_problem);
            else if (key == "last_update_check") {
                s.last_update_check = string_value(d, v, key);
                if (const auto why = stamp_problem(s.last_update_check)) d.fail(v, *why);
            } else if (key == "outside_paths") read_approvals(d, v, s.outside_paths);
            else d.fail(v, xpp::format("unknown state {}", json::shown(key)));
        });
        return s;
    } catch (ReadFailed &e) {
        return std::unexpected(std::move(e.error));
    }
}

Result<std::string> serialize_state(const State &s, std::string_view file)
{
    if (s.recent_models.size() > MAX_RECENT_MODELS)
        return fail(std::string(WHERE), xpp::format("cannot be saved: recent_models has {} entries: at most {}", s.recent_models.size(), MAX_RECENT_MODELS), Place{std::string(file)});
    const std::string text = state_text(s);
    const Result<State> back = parse_state(text, std::string(file));
    if (!back) return fail(std::string(WHERE), "cannot be saved: " + back.error().what, Place{std::string(file)});
    return text;
}

std::vector<Row> setting_rows()
{
    std::vector<Row> out = {
        {"general", "threads", Kind::threads, {}, "How many threads autoX's collocation and a sweep's runs use.", true},
        {"general", "log_level", Kind::choice, log_level_names, "What the log shows; it applies from the next start.", true},
        {"general", "log_to_file", Kind::flag, {}, "Write the log to xppautx.log in the model folder's .xppautx folder; off is the terminal.", true}};
    for (const ColorKey &c : COLOR_KEYS) out.push_back({"colors", c.key, Kind::color, {}, c.description, true});
    out.push_back({"page", "theme", Kind::choice, THEMES, "The page's colors: the system's, light or dark.", true});
    out.push_back({"page", "dock_hidden", Kind::flag, {}, "Hide the Values panel.", true});
    out.push_back({"page", "dock_width", Kind::width, {}, "The Values panel's width in pixels; empty is the stylesheet's.", true});
    out.push_back({"updates", "check_for_updates", Kind::flag, {}, "Look for a newer release when the window or the browser page opens, at most once a day.", false});
    return out;
}

std::string settings_path(FileSystem &fs) { return fs.config_path(SETTINGS_FILE); }
std::string state_path(FileSystem &fs) { return fs.config_path(STATE_FILE); }

Result<Settings> load_settings(FileSystem &fs) { return load_config<Settings>(fs, WHERE, SETTINGS_FILE, MAX_FILE_BYTES, parse_settings); }
Result<State> load_state(FileSystem &fs) { return load_config<State>(fs, WHERE, STATE_FILE, MAX_FILE_BYTES, parse_state); }

Result<Settings> effective_settings(FileSystem &fs, const Result<Settings> &user, std::string_view model_folder)
{
    if (!user || model_folder.empty()) return user;
    const Result<std::optional<std::string>> text = fs.read_model_state(WHERE, model_folder, SETTINGS_FILE, MAX_FILE_BYTES);
    if (!text) return std::unexpected(text.error());
    if (!*text) return user;
    return parse_folder_settings(**text, files::join(files::join(model_folder, files::MODEL_STATE_FOLDER), SETTINGS_FILE), *user);
}

Read read_all(FileSystem &fs) { return Read{load_settings(fs), load_state(fs)}; }

std::string hello_json(FileSystem &fs, const Result<Settings> &loaded)
{
    const Page defaults;
    const Page &p = loaded ? loaded->page : defaults;
    std::string out = loaded ? "{\"ok\":true" : "{\"ok\":false";
    if (!loaded) json::append_error(out, loaded.error());
    out += ",\"path\":";
    json_append_string(out, settings_path(fs));
    out += ",\"page\":{\"theme\":";
    json_append_string(out, p.theme);
    out += p.dock_hidden ? ",\"dock_hidden\":true" : ",\"dock_hidden\":false";
    out += p.dock_width ? xpp::format(",\"dock_width\":{}", *p.dock_width) : std::string(",\"dock_width\":null");
    out += ",\"folds\":[";
    append_strings(out, p.folds);
    out += "]},\"groups\":[";
    append_strings(out, std::vector<std::string>(GROUPS.begin(), GROUPS.end()));
    out += "],\"rows\":[";
    bool first = true;
    for (const Row &r : setting_rows()) {
        out += first ? "" : ",";
        first = false;
        out += "{\"group\":";
        json_append_string(out, r.group);
        out += ",\"key\":";
        json_append_string(out, r.key);
        out += ",\"kind\":";
        json_append_string(out, kind_names[static_cast<std::size_t>(r.kind)]);
        out += ",\"help\":";
        json_append_string(out, r.help);
        out += ",\"choices\":[";
        append_strings(out, std::vector<std::string>(r.choices.begin(), r.choices.end()));
        out += xpp::format("],\"in_folder\":{}}}", r.in_folder ? "true" : "false");
    }
    return out + "]}";
}

namespace {

/* the file a target names, its text (nullopt when it is not there) and where an error puts it */
struct TargetFile {
    std::string path;
    std::optional<std::string> text;
    bool folder = false;
};

Result<TargetFile> read_target(FileSystem &fs, const Target &target)
{
    TargetFile file;
    if (target.scope == Scope::user) {
        file.path = settings_path(fs);
        const Result<std::optional<std::string>> read = fs.read_config(WHERE, SETTINGS_FILE, MAX_FILE_BYTES);
        if (!read) return std::unexpected(read.error());
        file.text = *read;
        return file;
    }
    if (target.model_folder.empty()) return fail(std::string(WHERE), "no model is open, so there is no model folder to keep settings in", Place{std::string(SETTINGS_FILE)});
    file.folder = true;
    file.path = files::join(files::join(target.model_folder, files::MODEL_STATE_FOLDER), SETTINGS_FILE);
    const Result<std::optional<std::string>> read = fs.read_model_state(WHERE, target.model_folder, SETTINGS_FILE, MAX_FILE_BYTES);
    if (!read) return std::unexpected(read.error());
    file.text = *read;
    return file;
}

/* a file that is there is checked as it stands: a bad one is the error and is never overwritten */
Result<> check_kept(const TargetFile &file)
{
    if (file.text) return check_text(*file.text, file.path, file.folder);
    return {};
}

Result<> write_target(FileSystem &fs, const Target &target, const std::string &text)
{
    if (target.scope == Scope::user) return fs.write_config(WHERE, SETTINGS_FILE, text);
    return fs.write_model_state(WHERE, target.model_folder, SETTINGS_FILE, text);
}

} // namespace

Result<std::optional<std::string>> existing_file(FileSystem &fs, const Target &target)
{
    const Result<TargetFile> file = read_target(fs, target);
    if (!file) return std::unexpected(file.error());
    return file->text ? std::optional<std::string>(file->path) : std::nullopt;
}

std::string target_path(FileSystem &fs, const Target &target)
{
    if (target.scope == Scope::user) return settings_path(fs);
    return target.model_folder.empty() ? std::string() : files::join(files::join(target.model_folder, files::MODEL_STATE_FOLDER), SETTINGS_FILE);
}

KeysSet keys_set(FileSystem &fs, std::string_view model_folder)
{
    KeysSet keys;
    for (const Scope scope : {Scope::user, Scope::folder}) {
        const Target target{scope, std::string(model_folder)};
        const Result<TargetFile> file = scope == Scope::folder && model_folder.empty() ? Result<TargetFile>(TargetFile{}) : read_target(fs, target);
        if (file) (scope == Scope::user ? keys.user : keys.folder) = keys_in(file->text, file->path);
    }
    return keys;
}

Result<> set_settings(FileSystem &fs, const std::vector<Change> &changes, const Target &target)
{
    const Result<TargetFile> file = read_target(fs, target);
    if (!file) return std::unexpected(file.error());
    std::string text = file->text ? *file->text : settings_template();
    if (const Result<> kept = check_kept(*file); !kept) return std::unexpected(kept.error());
    for (const Change &c : changes) {
        if (const Result<> ok = check_change(c, file->folder); !ok) return std::unexpected(ok.error());
        apply_change(text, c);
    }
    if (const Result<> checked = check_text(text, file->path, file->folder); !checked) return std::unexpected(checked.error());
    return write_target(fs, target, text);
}

Result<> reset_setting(FileSystem &fs, const Target &target, std::string_view group, std::string_view key)
{
    const Result<TargetFile> file = read_target(fs, target);
    if (!file) return std::unexpected(file.error());
    if (const Result<> kept = check_kept(*file); !kept) return std::unexpected(kept.error());
    if (!file->text) return {};
    std::string text = *file->text;
    if (!remove_member(text, group, key)) return {};
    if (const Result<> checked = check_text(text, file->path, file->folder); !checked) return std::unexpected(checked.error());
    return write_target(fs, target, text);
}

Result<> check_keymap(const keymap::Keymap &map)
{
    const std::string text = xpp::format("{{\n  \"{}\": {},\n  \"keymap\": {{\n    \"bindings\": {},\n    \"pinned\": {}\n  }}\n}}\n",
                                         version_word, format_version, keymap::bindings_json(map), keymap::pinned_json(map));
    return check_text(text, std::string(KEYMAP_SENT));
}

Result<> set_keymap(FileSystem &fs, const keymap::Keymap &map)
{
    if (const Result<> ok = check_keymap(map); !ok) return std::unexpected(ok.error());
    return set_settings(fs, {{"keymap", "bindings", keymap::bindings_json(map)}, {"keymap", "pinned", keymap::pinned_json(map)}});
}

Result<> update_state(FileSystem &fs, const std::function<void(State &)> &change)
{
    Result<State> s = load_state(fs);
    if (!s) return std::unexpected(s.error());
    change(*s);
    return write_state(fs, *s);
}

Result<bool> update_check_due(FileSystem &fs)
{
    const Result<Settings> s = load_settings(fs);
    if (!s) return std::unexpected(s.error());
    const Result<State> st = load_state(fs);
    if (!st) return std::unexpected(st.error());
    /* a day is the UTC date, the first ten characters of the time */
    const std::string now = utc_now();
    constexpr std::size_t DATE_CHARS = 10;
    if (!s->check_for_updates || st->last_update_check.substr(0, DATE_CHARS) == now.substr(0, DATE_CHARS)) return false;
    const Result<> saved = update_state(fs, [&](State &q) { q.last_update_check = now; });
    if (!saved) return std::unexpected(saved.error());
    return true;
}

LoadServices load_services(FileSystem &fs, const Read *read)
{
    const Result<State> st = read ? read->state : load_state(fs);
    LoadServices services;
    if (st) services.kept = st->outside_paths;
    else services.unreadable = st.error();
    services.keep = [&fs](const OutsideApproval &a) {
        return update_state(fs, [&](State &q) { remember_newest(q.outside_paths, a, MAX_OUTSIDE_APPROVALS); });
    };
    services.check_command = command::check_value;
    return services;
}

} // namespace xpp::settings
