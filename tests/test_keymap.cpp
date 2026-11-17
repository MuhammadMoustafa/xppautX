/* the keymap (W211, W790, core/xpp_keymap.h and its group of settings.json): the key grammar, every kind of bad
   keymap refused whole with its file, line and value, the effective keys, and the keymap's load, save and reset in a
   config folder of its own (XPP_CONFIG_DIR: the user's real file is never touched). The protocol side (get, set,
   reset, hello) is tools/servercheck.py's keymap section. */
#include "xpptest.h"
#include "test_support.h"
#include "command_def.h"
#include "xpp_files.h"
#include "xpp_fs_local.h"
#include "xpp_io.h"
#include "xpp_keymap.h"
#include "xpp_settings.h"
#include "xpp_error.h"

#include <expected>
#include <filesystem>
#include <set>
#include <string>
#include <cstdio>
#include <string_view>
#include <vector>

namespace {

constexpr const char *FILE_LABEL = "s.json";

/* a keymap group's object, read as the keymap of a settings file (its first line is the file's first: the errors'
   lines are the object's) */
xpp::Result<xpp::keymap::Keymap> parse_keymap(std::string_view object, std::string file)
{
    const xpp::Result<xpp::settings::Settings> s = xpp::settings::parse_settings("{\"version\": 4, \"keymap\": " + std::string(object) + "}", file);
    if (!s) return std::unexpected(s.error());
    return s->keymap;
}

/* a settings file holding that keymap object */
std::string settings_text(std::string_view object) { return "{\"version\": 4, \"keymap\": " + std::string(object) + "}\n"; }

xpp::Result<xpp::keymap::Keymap> keymap_of(xpp::FileSystem &fs)
{
    const xpp::Result<xpp::settings::Settings> s = xpp::settings::load_settings(fs);
    if (!s) return std::unexpected(s.error());
    return s->keymap;
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
    const auto map = parse_keymap("{}", FILE_LABEL);
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

void check_bad_keymaps()
{
    const std::string nul("{}\0{", 4);
    const std::string deep = "{\"pinned\": " + std::string(100, '[') + std::string(100, ']') + "}";
    const std::string many_keys = "{\"bindings\": {\"session.reload_model\": [\"A\",\"B\",\"C\",\"D\",\"E\",\"F\",\"G\",\"H\",\n\"I\"]}}";
    const Bad bad[] = {
        {"empty", "", 1, "not valid JSON"},
        {"trailing comma", "{\"pinned\": [],}", 1, "not valid JSON"},
        {"too deep", deep.c_str(), 1, "nested too deeply"},
        {"NUL", nul.c_str(), 1, "NUL"},
        {"unknown setting", "{\n  \"pinned\": [],\n  \"color\": 1\n}", 3, "color"},
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
        {"clash of two", "{\"bindings\": {\"session.reload_model\": [\"Ctrl+B\"],\n\"calculator.evaluate\": [\"Ctrl+B\"]}}", 2, "session.reload_model"},
        {"one key twice", "{\"bindings\": {\"session.reload_model\": [\"Ctrl+B\",\n\"Ctrl+B\"]}}", 2, "both on key"},
        {"pinned not an array", "{\"pinned\": {}}", 1, "{}"},
        {"pinned unknown", "{\"pinned\": [\n\"nosuch\"]}", 2, "nosuch"},
        {"pinned not pinnable", "{\"pinned\": [\"quit\"]}", 1, "quit"},
        {"pinned a panel command", "{\"pinned\": [\n\"window\"]}", 2, "window"},
        {"pinned twice", "{\"pinned\": [{\"name\": \"A\", \"steps\": [\"run.initial\"]},\n{\"name\": \"A\", \"steps\": [\"run.initial\"]}]}", 2, "twice"},
        /* W775: a pin is a name and its steps, each a command value */
        {"a pin with no name", "{\"pinned\": [\n{\"steps\": [\"run.initial\"]}]}", 2, "no name"},
        {"a pin with an empty name", "{\"pinned\": [{\"name\":\n\"\", \"steps\": [\"run.initial\"]}]}", 2, "empty"},
        {"a pin with no steps", "{\"pinned\": [\n{\"name\": \"A\"}]}", 2, "no steps"},
        {"a pin with an empty list of steps", "{\"pinned\": [\n{\"name\": \"A\", \"steps\": []}]}", 2, "no steps"},
        {"a pin member that is none", "{\"pinned\": [{\"name\": \"A\", \"steps\": [\"run.initial\"],\n\"keys\": []}]}", 2, "keys"},
        {"a step that is no command", "{\"pinned\": [{\"name\": \"A\", \"steps\": [\"run.initial\",\n\"nosuch\"]}]}", 2, "nosuch"},
        {"a step not a string", "{\"pinned\": [{\"name\": \"A\", \"steps\": [\n7]}]}", 2, "7"},
        {"a step of another spelling", "{\"pinned\": [{\"name\": \"A\", \"steps\": [\n\"plot.export  --format pdf\"]}]}", 2, "write the value as"},
        {"a step with a bad argument", "{\"pinned\": [{\"name\": \"A\", \"steps\": [\n\"data.export --format cvs\"]}]}", 2, "cvs"},
        /* W775: a pin of several steps is bound by pin "Name" */
        {"a binding of no pin", "{\"bindings\": {\n\"pin \\\"Nope\\\"\": [\"Ctrl+K\"]}}", 2, "no pin has that name"},
        {"a binding of a pin of one step", "{\"pinned\": [{\"name\": \"A\", \"steps\": [\"run.initial\"]}],\n\"bindings\": {\"pin \\\"A\\\"\": [\"Ctrl+K\"]}}", 2, "one step"},
        {"a pin's key another command has", "{\"pinned\": [{\"name\": \"A\", \"steps\": [\"run.initial\", \"run.initial\"]}],\n\"bindings\": {\"pin \\\"A\\\"\": [\"Ctrl+O\"]}}", 2, "both on key"},
        {"pinned not a string", "{\"pinned\": [true]}", 1, "true"},
        /* W772: a command value is checked as the command checks its arguments, and names the binding and the value */
        {"a bad value", "{\"bindings\": {\n\"model.export --language nosuch\": [\"Ctrl+E\"]}}", 2, "nosuch"},
        {"an unknown argument", "{\"bindings\": {\n\"model.export --nosuch python\": [\"Ctrl+E\"]}}", 2, "nosuch"},
        {"a value of another spelling", "{\"bindings\": {\n\"model.export  --language python\": [\"Ctrl+E\"]}}", 2, "write the value as `model.export --language python`"},
        {"a pinned bad value", "{\"pinned\": [{\"name\": \"A\", \"steps\": [\n\"model.export --language nosuch\"]}]}", 2, "nosuch"},
        {"a clash of two values", "{\"bindings\": {\"model.export --language python\": [\"Ctrl+E\"],\n\"model.export --language python_numpy\": [\"Ctrl+E\"]}}", 2, "both on key"},
    };
    for (const Bad &b : bad) {
        const xpp::Result<xpp::keymap::Keymap> r = parse_keymap(b.text == nul.c_str() ? std::string_view(nul) : std::string_view(b.text), FILE_LABEL);
        if (r) {
            CHECK(!"a bad keymap was accepted");
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

void check_good_keymaps()
{
    const auto clash = parse_keymap(R"({"bindings":{"run.continue":["Ctrl+R"]}})", FILE_LABEL);
    CHECK(!clash);
    if (!clash)
        for (const char *name : {"run.continue", "run.initial", "Ctrl+R"})
            CHECK(clash.error().what.find(name) != std::string::npos);

    auto empty = parse_keymap("{}", FILE_LABEL);
    CHECK(empty && empty->bindings.empty() && empty->pinned.empty());

    const std::string text =
        "{\r\n \"pinned\": [{\"name\": \"Reload\", \"steps\": [\"session.reload_model\"]},\r\n"
        " {\"steps\": [\"run.initial\", \"plot.export --format svg --file \\\"fig1.svg\\\"\"], \"name\": \"Figures\"}],\r\n"
        " \"bindings\": {\"session.reload_model\": [\"Ctrl+B\"], \"session.save\": [], \"run.continue\": [\"Ctrl+K\", \"Alt+I\"]}}\r\n";
    auto map = parse_keymap(text, FILE_LABEL);
    CHECK(map.has_value());
    if (!map) return;
    CHECK(xpp::keymap::pin_binding_id("Figures") == "pin \"Figures\"" && xpp::keymap::pin_binding_name("pin \"Figures\"") == "Figures"
          && !xpp::keymap::pin_binding_name("run.initial") && !xpp::keymap::pin_binding_name("pin Figures"));
    CHECK(map->pinned.size() == 2 && map->pinned[0].name == "Reload" && map->pinned[0].steps == std::vector<std::string>({"session.reload_model"})
          && map->pinned[1].name == "Figures"
          && map->pinned[1].steps == std::vector<std::string>({"run.initial", "plot.export --format svg --file \"fig1.svg\""}));
    CHECK(xpp::keymap::effective_keys(*map, *xpp::command::find("session.reload_model")) == std::vector<std::string>({"Ctrl+B"}));
    CHECK(xpp::keymap::effective_keys(*map, *xpp::command::find("session.save")).empty());       /* removed */
    CHECK(xpp::keymap::effective_keys(*map, *xpp::command::find("session.open_model")) == std::vector<std::string>({"Ctrl+O"})); /* the table's */
    CHECK(xpp::keymap::effective_keys(*map, *xpp::command::find("calculator.evaluate")).empty());
    /* a key a user took from a command that kept no other: free for another one */
    CHECK(parse_keymap("{\"bindings\": {\"session.open_model\": [], \"session.reload_model\": [\"Ctrl+O\"]}}", FILE_LABEL).has_value());
    /* a default key moved back to the command that has it */
    CHECK(parse_keymap("{\"bindings\": {\"session.open_model\": [\"Ctrl+O\"]}}", FILE_LABEL).has_value());

    /* what is written reads back the same, and is the one text */
    const std::string again = "{\"bindings\": " + xpp::keymap::bindings_json(*map) + ", \"pinned\": " + xpp::keymap::pinned_json(*map) + "}";
    const auto back = parse_keymap(again, FILE_LABEL);
    CHECK(back && back->bindings == map->bindings && back->pinned == map->pinned);
    CHECK(back && xpp::keymap::bindings_json(*back) == xpp::keymap::bindings_json(*map));
}

void check_file(const std::string &dir)
{
    xpp::LocalFileSystem fs;
    const std::string file = xpp::settings::settings_path(fs);
    CHECK(file == dir + std::string(1, std::filesystem::path::preferred_separator) + "settings.json");

    auto none = keymap_of(fs); /* no file: all defaults, not an error */
    CHECK(none && none->bindings.empty() && none->pinned.empty());
    CHECK(xpp::settings::set_keymap(fs, {}).has_value()); /* the reset: no differences */

    xpp::keymap::Keymap map;
    map.bindings["session.reload_model"] = {"Ctrl+B"};
    map.pinned = {{"Evaluate", {"calculator.evaluate"}}, {"Two", {"run.initial", "plot.export --format pdf"}}};
    const auto saved = xpp::settings::set_keymap(fs, map);
    if (!saved) std::fprintf(stderr, "save: %s\n", saved.error().text().c_str()); /* why, when it fails (W220) */
    CHECK(saved.has_value());
    auto loaded = keymap_of(fs);
    CHECK(loaded && loaded->bindings == map.bindings && loaded->pinned == map.pinned);
    const std::string json = xpp::keymap::effective_json(file, loaded);
    CHECK(json.starts_with("{\"ok\":true"));
    CHECK(json.find("{\"id\":\"session.reload_model\",\"keys\":[\"Ctrl+B\"],\"source\":\"user\"}") != std::string::npos);
    CHECK(json.find("{\"id\":\"session.open_model\",\"keys\":[\"Ctrl+O\"],\"source\":\"default\"}") != std::string::npos);

    /* W772: a command with fixed arguments is bound and pinned by its value, and the page gets what runs it */
    {
        xpp::Writer w = xpp::Writer::binary(file);
        CHECK(w && w.write(settings_text("{\"pinned\": [{\"name\": \"Python\", \"steps\": [\"model.export --language python\"]}], \"bindings\": {\"model.export --language python\": [\"Ctrl+E\"]}}")) && w.commit().has_value());
    }
    const auto valued = keymap_of(fs);
    CHECK(valued.has_value());
    const std::string valued_json = xpp::keymap::effective_json(file, valued);
    CHECK(valued_json.find("{\"id\":\"model.export --language python\",\"keys\":[\"Ctrl+E\"],\"source\":\"user\"}") != std::string::npos);
    CHECK(valued_json.find("\"values\":[{\"value\":\"model.export --language python\",\"run\":{\"cmd\":\"model.export\",\"language\":\"python\"}}]") != std::string::npos);

    /* a bad file on disk is an error naming the file, and the effective keymap is the defaults, marked */
    {
        xpp::Writer w = xpp::Writer::binary(file);
        CHECK(w && w.write("{\"version\": 4,\n\"keymap\": {\"bindings\": {\"session.reload_model\": [\"Ctrl+W\"]}}}\n") && w.commit().has_value());
    }
    auto bad = keymap_of(fs);
    CHECK(!bad && bad.error().place.file == file && bad.error().place.line == 2);
    const std::string marked = xpp::keymap::effective_json(file, bad);
    CHECK(marked.starts_with("{\"ok\":false,\"error\":"));
    CHECK(marked.find("\"keys\":[\"Ctrl+W\"]") == std::string::npos);
    CHECK(marked.find("{\"id\":\"session.open_model\",\"keys\":[\"Ctrl+O\"],\"source\":\"default\"}") != std::string::npos);
    /* the keymap a bad file holds is not replaced by a set: the error is the answer */
    CHECK(!xpp::settings::set_keymap(fs, map));
    CHECK(xpp::settings::set_keymap(fs, {}).has_value() == false);
    /* a file over the limit is refused as it is read */
    {
        xpp::Writer w = xpp::Writer::binary(file);
        CHECK(w && w.write(std::string(xpp::settings::MAX_FILE_BYTES * 2, ' ')) && w.commit().has_value());
    }
    CHECK(!keymap_of(fs));
    xpp::files::remove(file.c_str());
    CHECK(keymap_of(fs).has_value());
}

} // namespace

TEST_CASE("test_keymap")
{
    check_table();
    check_keys();
    check_bad_keymaps();
    check_good_keymaps();

    /* the config folder is made on the first save, with its missing parents */
    xpp::LocalFileSystem fs;
    xpp::TempDir temp;
    CHECK(!temp.path().empty());
    const std::string dir = temp.path() + "/config/xppautX";
    CHECK(set_config_dir(dir.c_str()));
    check_file(dir);
    std::filesystem::remove_all(temp.path() + "/config");
    /* a relative override would follow the working folder: it is an error, never a guess */
    CHECK(set_config_dir("relative/folder"));
    CHECK(xpp::files::config_dir().empty() && xpp::settings::settings_path(fs).empty());
    auto none = keymap_of(fs);
    CHECK(!none && none.error().what.find("XPP_CONFIG_DIR") != std::string::npos && !xpp::settings::set_keymap(fs, {}));
    set_config_dir(nullptr);
    TEST_REPORT("keymap");
}
