/* keymap.json (W211, core/xpp_keymap.h): the key grammar, every kind of bad
   file refused whole with its file, line and value, the effective keys, and
   the file's load, save and reset in a folder of its own (XPP_CONFIG_DIR:
   the user's real file is never touched). The protocol side (get, set,
   reset, hello) is tools/servercheck.py's keymap section. */
#include "xpptest.h"
#include "test_support.h"
#include "command_def.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_keymap.h"
#include "format_version.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>

namespace {

constexpr const char *FILE_LABEL = "k.json";

/* the registry's rows are what the grammar and the file rules are checked against: every default key is a key,
   none is reserved, and no two commands share one (the keymap's own check, which loading a file runs too) */
/* a keymap file's text with the format version as its first member, on its first line (the version is what every file of
   ours says; a test writes only what it tests) */
std::string keymap_text(std::string_view text)
{
    const std::size_t brace = text.find('{');
    if (text.empty() || text[0] != '{' || brace == std::string_view::npos) return std::string(text);
    const std::string_view rest = text.substr(brace + 1);
    const std::size_t next = rest.find_first_not_of(" \r\n\t");
    const bool empty = next != std::string_view::npos && rest[next] == '}';
    return "{\"version\": " + std::to_string(xpp::format_version) + (empty ? "" : ",") + std::string(rest);
}

void check_table()
{
    std::set<std::string> keys;
    for (const xpp::command::Command *row : xpp::command::rows()) {
        for (const char *key : row->default_keys) {
            CHECK(xpp::keymap::key_problem(key).empty());
            CHECK(!xpp::keymap::is_reserved_key(key));
            CHECK(keys.insert(key).second); /* no two defaults on one key */
        }
    }
    const auto map = xpp::keymap::parse_keymap(keymap_text("{}"), FILE_LABEL);
    if (!map) std::printf("the defaults: %s\n", map.error().text().c_str());
    CHECK(map.has_value());
}

void check_keys()
{
    for (const char *good : {"Ctrl+O", "Ctrl+Shift+S", "Alt+Enter", "F1", "F24", "Escape", "S", "Ctrl++",
                             "Ctrl+Alt+Shift+Meta+ArrowRight", "Space", "1", "/"})
        CHECK(xpp::keymap::key_problem(good).empty());
    for (const char *bad : {"", " ", "ctrl+o", "Shift+Ctrl+S", "Ctrl+Ctrl+S", "Ctrl+", "Ctrl", "o", "F0", "F25", "F01", "A B C D E",
                            "A  B", " A", "A ", "F S", "Ctrl+K Ctrl+S", "Ctrl+Enter+X", "Esc", "ArrowLeft+A", "\xc3\xa9", "Ctrl+Shift", "Hyper+A", "a"})
        CHECK(!xpp::keymap::key_problem(bad).empty());
    CHECK(!xpp::keymap::key_problem(std::string(xpp::keymap::MAX_KEY_BYTES + 1, 'A')).empty());
    CHECK(xpp::keymap::is_reserved_key("Ctrl+W") && xpp::keymap::is_reserved_key("F12") && !xpp::keymap::is_reserved_key("Ctrl+S"));
}

struct Bad {
    const char *name, *text;
    int line;
    const char *value; /* in the error's text: the value the user must see */
};

void check_bad_files()
{
    const std::string too_long = std::string(xpp::keymap::MAX_FILE_BYTES + 10, ' ') + "{}";
    const std::string nul("{}\0{", 4);
    const std::string deep = "{\"pinned\": " + std::string(100, '[') + std::string(100, ']') + "}";
    const std::string many_keys = "{\"bindings\": {\"session.reload_model\": [\"A\",\"B\",\"C\",\"D\",\"E\",\"F\",\"G\",\"H\",\n\"I\"]}}";
    const Bad bad[] = {
        {"empty", "", 1, "not valid JSON"},
        {"trailing comma", "{\"pinned\": [],}", 1, "not valid JSON"},
        {"text after", "{}\n{}", 2, "after its value"},
        {"top is an array", "[]", 1, "one JSON object"},
        {"too deep", deep.c_str(), 1, "nested too deeply"},
        {"too long", too_long.c_str(), 1, "longer than"},
        {"NUL", nul.c_str(), 1, "NUL"},
        {"unknown setting", "{\n  \"pinned\": [],\n  \"colour\": 1\n}", 3, "colour"},
        {"setting twice", "{\n\"pinned\": [],\n\"pinned\": []}", 3, "pinned"},
        {"a preset", "{\n\"preset\": \"xppaut\"}", 2, "preset"},
        {"the default preset", "{\"pinned\": [],\n\"preset\": \"default\"}", 2, "preset"},
        {"bindings not an object", "{\"bindings\": []}", 1, "[]"},
        {"unknown command", "{\n\"bindings\": {\n\"nosuch\": [\"A\"]}}", 3, "nosuch"},
        {"layer command", "{\"bindings\": {\"file\": [\"A\"]}}", 1, "file"},
        {"command twice", "{\"bindings\": {\n\"session.reload_model\": [\"A\"],\n\"session.reload_model\": [\"B\"]}}", 3, "session.reload_model"},
        {"keys not an array", "{\"bindings\": {\"session.reload_model\": \"A\"}}", 1, "\"A\""},
        {"key not a string", "{\"bindings\": {\"session.reload_model\": [\n7]}}", 2, "7"},
        {"malformed key", "{\"bindings\": {\"session.reload_model\": [\n\"ctrl+r\"]}}", 2, "ctrl+r"},
        {"modifier order", "{\"bindings\": {\"session.reload_model\": [\"Shift+Ctrl+R\"]}}", 1, "Shift+Ctrl+R"},
        {"empty key", "{\"bindings\": {\"session.reload_model\": [\"\"]}}", 1, "empty"},
        {"a sequence", "{\"bindings\": {\"session.reload_model\": [\n\"F S\"]}}", 2, "never a sequence"},
        {"a chord", "{\"bindings\": {\"session.reload_model\": [\"Ctrl+K Ctrl+S\"]}}", 1, "Ctrl+K Ctrl+S"},
        {"too many keys", many_keys.c_str(), 2, "more than"},
        {"reserved", "{\"bindings\": {\"session.reload_model\": [\n\"Ctrl+W\"]}}", 2, "Ctrl+W"},
        {"clash with a default", "{\"bindings\": {\n\"session.reload_model\": [\"Ctrl+O\"]}}", 2, "session.open_model"},
        {"clash of two", "{\"bindings\": {\"session.reload_model\": [\"Ctrl+B\"],\n\"tool.calculator\": [\"Ctrl+B\"]}}", 2, "session.reload_model"},
        {"one key twice", "{\"bindings\": {\"session.reload_model\": [\"Ctrl+B\",\n\"Ctrl+B\"]}}", 2, "both on key"},
        {"pinned not an array", "{\"pinned\": {}}", 1, "{}"},
        {"pinned unknown", "{\"pinned\": [\n\"nosuch\"]}", 2, "nosuch"},
        {"pinned not pinnable", "{\"pinned\": [\"quit\"]}", 1, "quit"},
        {"pinned a panel command", "{\"pinned\": [\n\"window\"]}", 2, "window"},
        {"pinned twice", "{\"pinned\": [\"session.reload_model\",\n\"session.reload_model\"]}", 2, "twice"},
        {"pinned not a string", "{\"pinned\": [true]}", 1, "true"},
    };
    for (const Bad &b : bad) {
        const xpp::Result<xpp::keymap::Keymap> r = xpp::keymap::parse_keymap(b.text == nul.c_str() ? std::string(nul) : keymap_text(b.text), FILE_LABEL);
        if (r) {
            CHECK(!"a bad file was accepted");
            std::printf("  ... %s\n", b.name);
            continue;
        }
        const xpp::Error &e = r.error();
        const bool placed = e.place.file == FILE_LABEL && e.place.line == b.line && e.text().starts_with(xpp::format("{}:{}:", FILE_LABEL, b.line));
        const bool named = e.what.find(b.value) != std::string::npos;
        if (!placed || !named) std::printf("  ... %s: %s\n", b.name, e.text().c_str());
        CHECK(placed);
        CHECK(named);
    }
}

void check_good_files()
{
    const auto clash = xpp::keymap::parse_keymap(keymap_text(R"({"bindings":{"run.continue":["Ctrl+R"]}})"), FILE_LABEL);
    CHECK(!clash);
    if (!clash)
        for (const char *name : {"run.continue", "run.initial", "Ctrl+R"})
            CHECK(clash.error().what.find(name) != std::string::npos);

    auto empty = xpp::keymap::parse_keymap(keymap_text("{}"), FILE_LABEL);
    CHECK(empty && empty->bindings.empty() && empty->pinned.empty());
    CHECK(xpp::keymap::parse_keymap("", FILE_LABEL).has_value() == false);
    /* the version: none, another, or not a number is refused, naming both versions */
    for (const auto &[file, wanted] : {std::pair<const char *, const char *>{"{}", "this program reads keymaps of version 4"},
                                       {"{\"version\": 3}", "version 3: this program reads keymaps of version 4"},
                                       {"{\"version\": \"4\"}", "the version is a whole number"},
                                       {"{\"version\": 4, \"version\": 4}", "is given twice"}}) {
        const auto refused = xpp::keymap::parse_keymap(file, FILE_LABEL);
        CHECK(!refused && refused.error().what.find(wanted) != std::string::npos);
    }

    const std::string text = keymap_text(
        "{\r\n \"pinned\": [\"session.reload_model\", \"tool.calculator\"],\r\n"
        " \"bindings\": {\"session.reload_model\": [\"Ctrl+B\"], \"session.save\": [], \"run.continue\": [\"Ctrl+K\", \"Alt+I\"]}}\r\n");
    auto map = xpp::keymap::parse_keymap(text, FILE_LABEL);
    CHECK(map.has_value());
    if (!map) return;
    CHECK(map->pinned == std::vector<std::string>({"session.reload_model", "tool.calculator"}));
    CHECK(xpp::keymap::effective_keys(*map, *xpp::command::find("session.reload_model")) == std::vector<std::string>({"Ctrl+B"}));
    CHECK(xpp::keymap::effective_keys(*map, *xpp::command::find("session.save")).empty());       /* removed */
    CHECK(xpp::keymap::effective_keys(*map, *xpp::command::find("session.open_model")) == std::vector<std::string>({"Ctrl+O"})); /* the table's */
    CHECK(xpp::keymap::effective_keys(*map, *xpp::command::find("tool.calculator")).empty());
    /* a key a user took from a command that kept no other: free for another one */
    CHECK(xpp::keymap::parse_keymap(keymap_text("{\"bindings\": {\"session.open_model\": [], \"session.reload_model\": [\"Ctrl+O\"]}}"), FILE_LABEL).has_value());
    /* a default key moved back to the command that has it */
    CHECK(xpp::keymap::parse_keymap(keymap_text("{\"bindings\": {\"session.open_model\": [\"Ctrl+O\"]}}"), FILE_LABEL).has_value());

    /* what is written reads back the same, and is the one text */
    const std::string again = xpp::keymap::serialize(*map);
    auto back = xpp::keymap::parse_keymap(again, FILE_LABEL);
    CHECK(back && back->bindings == map->bindings && back->pinned == map->pinned);
    CHECK(back && xpp::keymap::serialize(*back) == again);
    CHECK(xpp::keymap::parse_keymap(xpp::keymap::serialize({}), FILE_LABEL).has_value());
}

void check_file(const std::string &dir)
{
    CHECK(xpp::files::config_dir() == dir); /* the override names the folder */
    const std::string file = xpp::keymap::path();
    CHECK(file == dir + std::string(1, std::filesystem::path::preferred_separator) + "keymap.json");

    auto none = xpp::keymap::load_keymap(); /* no file: all defaults, not an error */
    CHECK(none && none->bindings.empty() && none->pinned.empty());
    CHECK(xpp::keymap::reset().has_value()); /* nothing to remove is fine */

    xpp::keymap::Keymap map;
    map.bindings["session.reload_model"] = {"Ctrl+B"};
    map.pinned = {"tool.calculator"};
    const auto saved = xpp::keymap::save(map);
    if (!saved) std::fprintf(stderr, "save: %s\n", saved.error().text().c_str()); /* why, when it fails (W220) */
    CHECK(saved.has_value());
    CHECK(xpp::files::exists(file));
    auto loaded = xpp::keymap::load_keymap();
    CHECK(loaded && loaded->bindings == map.bindings && loaded->pinned == map.pinned);
    const std::string json = xpp::keymap::effective_json(loaded);
    CHECK(json.starts_with("{\"ok\":true"));
    CHECK(json.find("{\"id\":\"session.reload_model\",\"keys\":[\"Ctrl+B\"],\"source\":\"user\"}") != std::string::npos);
    CHECK(json.find("{\"id\":\"session.open_model\",\"keys\":[\"Ctrl+O\"],\"source\":\"default\"}") != std::string::npos);

    /* a bad file on disk is an error naming the file, and the effective keymap is the defaults, marked */
    {
        xpp::Writer w = xpp::Writer::binary(file);
        CHECK(w && w.write(keymap_text("{\"bindings\": {\"session.reload_model\": [\"Ctrl+W\"]}}")) && w.commit().has_value());
    }
    auto bad = xpp::keymap::load_keymap();
    CHECK(!bad && bad.error().place.file == file && bad.error().place.line == 1);
    const std::string marked = xpp::keymap::effective_json(bad);
    CHECK(marked.starts_with("{\"ok\":false,\"error\":"));
    CHECK(marked.find("\"keys\":[\"Ctrl+W\"]") == std::string::npos);
    CHECK(marked.find("{\"id\":\"session.open_model\",\"keys\":[\"Ctrl+O\"],\"source\":\"default\"}") != std::string::npos);
    /* a file over the limit is refused as it is read */
    {
        xpp::Writer w = xpp::Writer::binary(file);
        CHECK(w && w.write(std::string(xpp::keymap::MAX_FILE_BYTES * 2, ' ')) && w.commit().has_value());
    }
    CHECK(!xpp::keymap::load_keymap());

    CHECK(xpp::keymap::reset().has_value());
    CHECK(!xpp::files::exists(file));
    CHECK(xpp::keymap::load_keymap().has_value());
}

} // namespace

int main(void)
{
    check_table();
    check_keys();
    check_bad_files();
    check_good_files();

    /* the config folder is made on the first save, with its missing parents */
    xpp::TempDir temp;
    CHECK(!temp.path().empty());
    const std::string dir = temp.path() + "/config/xppautX";
    CHECK(set_config_dir(dir.c_str()));
    check_file(dir);
    std::filesystem::remove_all(temp.path() + "/config");
    /* a relative override would follow the working folder: it is an error, never a guess */
    CHECK(set_config_dir("relative/folder"));
    CHECK(xpp::files::config_dir().empty() && xpp::keymap::path().empty());
    auto none = xpp::keymap::load_keymap();
    CHECK(!none && none.error().what.find("XPP_CONFIG_DIR") != std::string::npos && !xpp::keymap::save({}));
    set_config_dir(nullptr);
    TEST_REPORT("keymap");
}
