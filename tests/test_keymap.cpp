/* keymap.json (W211, core/xpp_keymap.h): the key grammar, every kind of bad
   file refused whole with its file, line and value, the effective keys, and
   the file's load, save and reset in a folder of its own (XPP_CONFIG_DIR:
   the user's real file is never touched). The protocol side (get, set,
   reset, hello) is tools/servercheck.py's keymap section. */
#include "xpptest.h"
#include "command_table.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_keymap.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>

namespace {

constexpr const char *FILE_LABEL = "k.json";

/* the table is what the grammar and the file rules are checked against */
void check_table()
{
    std::set<std::string_view> ids;
    std::set<std::string> keys;
    for (const xpp::CommandRow &row : xpp::COMMANDS) {
        CHECK(ids.insert(row.id).second); /* an id is in one menu only: find_command_by_id's contract */
        for (std::string_view key : row.default_keys) {
            if (key.empty()) continue;
            CHECK(xpp::keymap::key_problem(key).empty());
            CHECK(!xpp::keymap::is_reserved_key(key));
            CHECK(keys.insert(std::string(key)).second); /* no two defaults on one key */
        }
    }
}

void check_keys()
{
    for (const char *good : {"Ctrl+O", "Ctrl+Shift+S", "Alt+Enter", "F1", "F24", "Escape", "S", "F S", "Ctrl+K Ctrl+S", "Ctrl++",
                             "Ctrl+Alt+Shift+Meta+ArrowRight", "Space", "1", "/"})
        CHECK(xpp::keymap::key_problem(good).empty());
    for (const char *bad : {"", " ", "ctrl+o", "Shift+Ctrl+S", "Ctrl+Ctrl+S", "Ctrl+", "Ctrl", "o", "F0", "F25", "F01", "A B C",
                            "A  B", " A", "A ", "Ctrl+Enter+X", "Esc", "ArrowLeft+A", "\xc3\xa9", "Ctrl+Shift", "Hyper+A", "a"})
        CHECK(!xpp::keymap::key_problem(bad).empty());
    CHECK(!xpp::keymap::key_problem(std::string(xpp::keymap::MAX_KEY_BYTES + 1, 'A')).empty());
    CHECK(xpp::keymap::is_reserved_key("Ctrl+W") && xpp::keymap::is_reserved_key("F12") && !xpp::keymap::is_reserved_key("Ctrl+S"));
}

/* XPP_CONFIG_DIR set to dir, or unset for nullptr (MinGW has no setenv) */
bool set_config_dir(const char *dir)
{
#ifdef _WIN32
    return _putenv_s(xpp::files::CONFIG_DIR_ENV, dir ? dir : "") == 0;
#else
    return dir ? setenv(xpp::files::CONFIG_DIR_ENV, dir, 1) == 0 : unsetenv(xpp::files::CONFIG_DIR_ENV) == 0;
#endif
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
    const std::string many_keys = "{\"bindings\": {\"reload\": [\"A\",\"B\",\"C\",\"D\",\"E\",\"F\",\"G\",\"H\",\n\"I\"]}}";
    const Bad bad[] = {
        {"empty", "", 1, "not valid JSON"},
        {"trailing comma", "{\"preset\": \"default\",}", 1, "not valid JSON"},
        {"text after", "{}\n{}", 2, "after its value"},
        {"top is an array", "[]", 1, "one JSON object"},
        {"too deep", deep.c_str(), 1, "nested too deeply"},
        {"too long", too_long.c_str(), 1, "longer than"},
        {"NUL", nul.c_str(), 1, "NUL"},
        {"unknown setting", "{\n  \"preset\": \"default\",\n  \"colour\": 1\n}", 3, "colour"},
        {"setting twice", "{\n\"preset\": \"default\",\n\"preset\": \"xppaut\"}", 3, "preset"},
        {"unknown preset", "{\n\"preset\": \"emacs\"}", 2, "emacs"},
        {"preset not a string", "{\"preset\": 3}", 1, "3"},
        {"bindings not an object", "{\"bindings\": []}", 1, "[]"},
        {"unknown command", "{\n\"bindings\": {\n\"nosuch\": [\"A\"]}}", 3, "nosuch"},
        {"layer command", "{\"bindings\": {\"file\": [\"A\"]}}", 1, "file"},
        {"command twice", "{\"bindings\": {\n\"reload\": [\"A\"],\n\"reload\": [\"B\"]}}", 3, "reload"},
        {"keys not an array", "{\"bindings\": {\"reload\": \"A\"}}", 1, "\"A\""},
        {"key not a string", "{\"bindings\": {\"reload\": [\n7]}}", 2, "7"},
        {"malformed key", "{\"bindings\": {\"reload\": [\n\"ctrl+r\"]}}", 2, "ctrl+r"},
        {"modifier order", "{\"bindings\": {\"reload\": [\"Shift+Ctrl+R\"]}}", 1, "Shift+Ctrl+R"},
        {"empty key", "{\"bindings\": {\"reload\": [\"\"]}}", 1, "empty"},
        {"too many parts", "{\"bindings\": {\"reload\": [\"A B C\"]}}", 1, "A B C"},
        {"too many keys", many_keys.c_str(), 2, "more than"},
        {"reserved", "{\"bindings\": {\"reload\": [\n\"Ctrl+W\"]}}", 2, "Ctrl+W"},
        {"reserved in a chord", "{\"bindings\": {\"reload\": [\"Ctrl+K F12\"]}}", 1, "F12"},
        {"clash with a default", "{\"bindings\": {\n\"reload\": [\"Ctrl+O\"]}}", 2, "Open model"},
        {"clash of two", "{\"bindings\": {\"reload\": [\"Ctrl+B\"],\n\"help\": [\"Ctrl+B\"]}}", 2, "Reload model"},
        {"one key twice", "{\"bindings\": {\"reload\": [\"Ctrl+B\",\n\"Ctrl+B\"]}}", 2, "clash"},
        {"chord start", "{\"bindings\": {\"reload\": [\"F\"],\n\"help\": [\"F S\"]}}", 2, "F S"},
        {"pinned not an array", "{\"pinned\": {}}", 1, "{}"},
        {"pinned unknown", "{\"pinned\": [\n\"nosuch\"]}", 2, "nosuch"},
        {"pinned not pinnable", "{\"pinned\": [\"quit\"]}", 1, "quit"},
        {"pinned twice", "{\"pinned\": [\"reload\",\n\"reload\"]}", 2, "twice"},
        {"pinned not a string", "{\"pinned\": [true]}", 1, "true"},
    };
    for (const Bad &b : bad) {
        const xpp::Result<xpp::keymap::Keymap> r = xpp::keymap::parse_keymap(std::string_view(b.text, b.text == nul.c_str() ? nul.size() : std::strlen(b.text)), FILE_LABEL);
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
    auto empty = xpp::keymap::parse_keymap("{}", FILE_LABEL);
    CHECK(empty && empty->bindings.empty() && empty->pinned.empty() && empty->preset == "default");
    CHECK(xpp::keymap::parse_keymap("", FILE_LABEL).has_value() == false);

    const std::string text =
        "{\r\n \"preset\": \"xppaut\",\r\n \"pinned\": [\"reload\", \"help\"],\r\n"
        " \"bindings\": {\"reload\": [\"Ctrl+B\"], \"savesession\": [], \"initialconds\": [\"F S\", \"Alt+I\"]}}\r\n";
    auto map = xpp::keymap::parse_keymap(text, FILE_LABEL);
    CHECK(map.has_value());
    if (!map) return;
    CHECK(map->preset == "xppaut");
    CHECK(map->pinned == std::vector<std::string>({"reload", "help"}));
    CHECK(xpp::keymap::effective_keys(*map, "reload") == std::vector<std::string>({"Ctrl+B"}));
    CHECK(xpp::keymap::effective_keys(*map, "savesession").empty());       /* removed */
    CHECK(xpp::keymap::effective_keys(*map, "openmodel") == std::vector<std::string>({"Ctrl+O"})); /* the table's */
    CHECK(xpp::keymap::effective_keys(*map, "help").empty());
    /* a key a user took from a command that kept no other: free for another one */
    CHECK(xpp::keymap::parse_keymap("{\"bindings\": {\"openmodel\": [], \"reload\": [\"Ctrl+O\"]}}", FILE_LABEL).has_value());
    /* a default key moved back to the command that has it */
    CHECK(xpp::keymap::parse_keymap("{\"bindings\": {\"openmodel\": [\"Ctrl+O\"]}}", FILE_LABEL).has_value());

    /* what is written reads back the same, and is the one text */
    const std::string again = xpp::keymap::serialize(*map);
    auto back = xpp::keymap::parse_keymap(again, FILE_LABEL);
    CHECK(back && back->bindings == map->bindings && back->pinned == map->pinned && back->preset == map->preset);
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
    map.bindings["reload"] = {"Ctrl+B"};
    map.pinned = {"help"};
    CHECK(xpp::keymap::save(map).has_value());
    CHECK(xpp::files::exists(file));
    auto loaded = xpp::keymap::load_keymap();
    CHECK(loaded && loaded->bindings == map.bindings && loaded->pinned == map.pinned);
    const std::string json = xpp::keymap::effective_json(loaded);
    CHECK(json.starts_with("{\"ok\":true"));
    CHECK(json.find("{\"id\":\"reload\",\"keys\":[\"Ctrl+B\"],\"source\":\"user\"}") != std::string::npos);
    CHECK(json.find("{\"id\":\"openmodel\",\"keys\":[\"Ctrl+O\"],\"source\":\"default\"}") != std::string::npos);

    /* a bad file on disk is an error naming the file, and the effective keymap is the defaults, marked */
    {
        xpp::Writer w = xpp::Writer::binary(file);
        CHECK(w && w.write("{\"bindings\": {\"reload\": [\"Ctrl+W\"]}}") && w.commit().has_value());
    }
    auto bad = xpp::keymap::load_keymap();
    CHECK(!bad && bad.error().place.file == file && bad.error().place.line == 1);
    const std::string marked = xpp::keymap::effective_json(bad);
    CHECK(marked.starts_with("{\"ok\":false,\"error\":"));
    CHECK(marked.find("\"keys\":[\"Ctrl+W\"]") == std::string::npos);
    CHECK(marked.find("{\"id\":\"openmodel\",\"keys\":[\"Ctrl+O\"],\"source\":\"default\"}") != std::string::npos);
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
