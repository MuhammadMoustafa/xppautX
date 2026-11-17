/* settings.json and state.json (W790, core/xpp_settings.h): comments, a change written in place with the file's comments and layout
   kept, the all-or-nothing reads with the error at its line, the limits, a bad file never overwritten, and state.json. Each file is
   in a config folder of its own (XPP_CONFIG_DIR: the user's real files are never touched). The keymap's own values are
   test_keymap.cpp's; the protocol side is tools/servercheck.py's settings section. */
#include "xpptest.h"
#include "test_support.h"
#include "xpp_fs_local.h"
#include "xpp_files.h"
#include "xpp_path.h"
#include "xpp_settings.h"
#include "format_version.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using xpp::settings::parse_settings;
using xpp::settings::parse_state;

std::string read_text(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    std::ostringstream s;
    s << f.rdbuf();
    return s.str();
}

void write_text(const std::string &path, const std::string &text)
{
    std::ofstream f(path, std::ios::binary);
    f << text;
}

/* the file's error is at `line` and says `what` */
template <class T> bool refused(const xpp::Result<T> &r, int line, std::string_view what)
{
    return !r && r.error().place.line == line && r.error().what.find(what) != std::string::npos;
}

std::string with_version(std::string_view body) { return "{\"version\": 4,\n" + std::string(body) + "}\n"; }

void check_defaults()
{
    const auto plain = parse_settings("{\"version\": 4}", "s.json");
    CHECK(plain && plain->threads == xpp::cpu::THREADS_AUTO && plain->log_level == XPP_LOG_WARN && plain->check_for_updates);
    CHECK(plain && plain->page.theme == "system" && !plain->page.dock_hidden && !plain->page.dock_width && plain->page.folds.empty());
    CHECK(parse_settings("", "s.json").has_value() == false); /* not JSON: a file of ours names its version */
}

void check_comments()
{
    /* // and slash-star comments, and a // inside a string, which is text */
    const std::string text = "{ // the top\n  \"version\": 4, /* the format */\n"
                             "  \"general\": {\"threads\": 3, \"log_level\": \"info\"},\n"
                             "  \"page\": {\"folds\": [\"a // not a comment\", \"b /* nor this */\"]}\n}\n";
    const auto s = parse_settings(text, "s.json");
    CHECK(s && s->threads == 3 && s->log_level == XPP_LOG_INFO);
    CHECK(s && s->page.folds == std::vector<std::string>({"a // not a comment", "b /* nor this */"}));
    CHECK(refused(parse_settings("{\"version\": 4 /* never closed", "s.json"), 1, "a comment /* is never closed"));
}

void check_refused()
{
    CHECK(refused(parse_settings(with_version("\"general\": {\"color\": 1}"), "s.json"), 2, "unknown setting \"color\" in \"general\""));
    CHECK(refused(parse_settings(with_version("\"nosuch\": {}"), "s.json"), 2, "unknown group \"nosuch\""));
    CHECK(refused(parse_settings(with_version("\"general\": {\"threads\": true}"), "s.json"), 2, "threads"));
    CHECK(refused(parse_settings(with_version("\"general\": {\"log_level\": \"loud\"}"), "s.json"), 2, "none of error, warn, info or debug"));
    CHECK(refused(parse_settings(with_version("\"colors\": {\"stable_equilibrium_color\": \"notacolor\"}"), "s.json"), 2, "is not a color"));
    CHECK(refused(parse_settings(with_version("\"page\": {\"theme\": \"blue\"}"), "s.json"), 2, "none of system, light or dark"));
    CHECK(refused(parse_settings(with_version("\"page\": {\"dock_width\": 100}"), "s.json"), 2, "dock_width is 288 to 800 pixels"));
    CHECK(refused(parse_settings(with_version("\"general\": {\"log_file\": \"relative.log\"}"), "s.json"), 2, "not an absolute path"));
    CHECK(refused(parse_settings(with_version("\"updates\": {\"check_for_updates\": 1}"), "s.json"), 2, "true or false"));
    CHECK(refused(parse_settings("{\"general\": {}}", "s.json"), 1, "has no \"version\""));
    CHECK(refused(parse_settings("{\"version\": 3}", "s.json"), 1, "version 3: this program reads settings files of version 4"));
    CHECK(refused(parse_settings("{\"version\": 4,\n\"page\": {},\n\"page\": {}}", "s.json"), 3, "is given twice"));
    /* all or nothing: the first bad value is the error, and no value of the file is handed over */
    const auto partly = parse_settings(with_version("\"general\": {\"threads\": 2}, \"page\": {\"theme\": \"light\"},\n\"updates\": {\"check_for_updates\": 7}}"), "s.json");
    CHECK(!partly && partly.error().place.line == 3);
    /* a file over the limit is refused before it is read, a NUL byte is the error at its place */
    CHECK(refused(parse_settings(std::string(xpp::settings::MAX_FILE_BYTES + 1, ' '), "s.json"), 1, "longer than"));
    CHECK(refused(parse_settings(std::string("{\"version\": 4}\0", 15), "s.json"), 1, "NUL byte"));
    /* the limits: folds and their length */
    std::string folds = "\"page\": {\"folds\": [";
    for (std::size_t i = 0; i <= xpp::settings::MAX_FOLDS; i++) folds += (i ? ",\"s" : "\"s") + std::to_string(i) + "\"";
    CHECK(refused(parse_settings(with_version(folds + "]}"), "s.json"), 2, "folds has more than 256 entries"));
    CHECK(refused(parse_settings(with_version("\"page\": {\"folds\": [\"a\", \"a\"]}"), "s.json"), 2, "listed twice"));
}

void check_state()
{
    const auto s = parse_state("{\"version\": 4, \"recent_models\": [\"/a/x.ode\", \"/b/y.ode\"], \"last_update_check\": \"2026-11-10T14:03:00Z\", "
                               "\"outside_paths\": [{\"model\": \"/m/a.ode\", \"path\": \"/p/q\"}]}",
                               "state.json");
    CHECK(s && s->recent_models.size() == 2 && s->last_update_check == "2026-11-10T14:03:00Z");
    CHECK(s && s->outside_paths.size() == 1 && s->outside_paths[0].path == "/p/q");
    CHECK(refused(parse_state("{\"version\": 4,\n\"recent_models\": [\"x.ode\"]}", "state.json"), 2, "not an absolute path"));
    CHECK(refused(parse_state("{\"version\": 4,\n\"last_update_check\": \"yesterday\"}", "state.json"), 2, "not a time"));
    CHECK(refused(parse_state("{\"version\": 4,\n\"color\": 1}", "state.json"), 2, "unknown state \"color\""));
    CHECK(refused(parse_state("{\"version\": 4,\n\"outside_paths\": [{\"model\": \"/m\"}]}", "state.json"), 2, "needs a model and a path"));
    std::string many = "{\"version\": 4, \"recent_models\": [";
    for (std::size_t i = 0; i <= xpp::settings::MAX_RECENT_MODELS; i++) many += (i ? ", \"/m/" : "\"/m/") + std::to_string(i) + ".ode\"";
    CHECK(refused(parse_state(many + "]}", "state.json"), 1, "has more than 10 entries"));
    /* what is written reads back the same */
    const auto back = parse_state(*xpp::settings::serialize_state(*s, "state.json"), "state.json");
    CHECK(back && back->recent_models == s->recent_models && back->outside_paths == s->outside_paths && back->last_update_check == s->last_update_check);
}

void check_files(const std::string &dir)
{
    std::filesystem::create_directories(dir); /* a test writes its own file before the program makes the folder */
    xpp::LocalFileSystem fs;
    const std::string file = xpp::settings::settings_path(fs);
    CHECK(!file.empty() && file == xpp::files::join(dir, xpp::settings::SETTINGS_FILE) /* the system's separator */);
    CHECK(!xpp::files::exists(file));
    const auto fresh = xpp::settings::load_settings(fs);
    CHECK(fresh && fresh->threads == xpp::cpu::THREADS_AUTO);

    /* a change to a file with comments: the one value is replaced, every other byte stays */
    const std::string commented = "{\n  // the user's settings\n  \"version\": 4,\n  \"general\": {\n    \"threads\": 2, // two threads\n"
                                  "    \"log_level\": \"warn\"\n  },\n  /* the colors are the defaults */\n  \"page\": {\"theme\": \"system\"}\n}\n";
    write_text(file, commented);
    CHECK(xpp::settings::set_settings(fs, {{"general", "threads", "1"} /* a count every machine has */, {"page", "theme", "\"dark\""}, {"updates", "check_for_updates", "false"}}).has_value());
    const std::string expected = "{\n  // the user's settings\n  \"version\": 4,\n  \"general\": {\n    \"threads\": 1, // two threads\n"
                                 "    \"log_level\": \"warn\"\n  },\n  /* the colors are the defaults */\n  \"page\": {\"theme\": \"dark\"},\n"
                                 "  \"updates\": {\n    \"check_for_updates\": false\n  }\n}\n";
    CHECK(read_text(file) == expected);
    const auto now = xpp::settings::load_settings(fs);
    CHECK(now && now->threads == 1 && now->page.theme == "dark" && !now->check_for_updates);

    /* a key a group does not have yet is added in its group; an empty group takes it with no comma */
    write_text(file, "{\"version\": 4, \"page\": {}}\n");
    CHECK(xpp::settings::set_settings(fs, {{"page", "dock_hidden", "true"}}).has_value());
    CHECK(read_text(file) == "{\"version\": 4, \"page\": {\n    \"dock_hidden\": true}}\n");

    /* a value the file would not take is refused, and nothing is written */
    const std::string before = read_text(file);
    CHECK(!xpp::settings::set_settings(fs, {{"general", "threads", "true"}}));
    CHECK(!xpp::settings::set_settings(fs, {{"nosuch", "x", "1"}}));
    CHECK(read_text(file) == before);

    /* a bad file is the error and is never overwritten */
    write_text(file, "{\"version\": 4, \"nosuch\": 1}\n");
    CHECK(!xpp::settings::set_settings(fs, {{"page", "theme", "\"light\""}}));
    CHECK(read_text(file) == "{\"version\": 4, \"nosuch\": 1}\n");
    CHECK(!xpp::settings::load_settings(fs));

    /* state.json: written whole, read back, and updated by its one owner */
    const std::string state = xpp::settings::state_path(fs);
    CHECK(xpp::settings::update_state(fs, [](xpp::settings::State &s) { s.recent_models.push_back("/m/a.ode"); }).has_value());
    CHECK(xpp::files::exists(state));
    const auto st = xpp::settings::load_state(fs);
    CHECK(st && st->recent_models == std::vector<std::string>{"/m/a.ode"});
    write_text(state, "{\"version\": 4, \"recent_models\": [\"relative\"]}\n");
    CHECK(!xpp::settings::update_state(fs, [](xpp::settings::State &s) { s.recent_models.clear(); }));
    CHECK(read_text(state) == "{\"version\": 4, \"recent_models\": [\"relative\"]}\n");
    xpp::files::remove(state.c_str());
    xpp::files::remove(file.c_str());
}

} // namespace

TEST_CASE("test_settings")
{
    check_defaults();
    check_comments();
    check_refused();
    check_state();

    xpp::TempDir temp;
    CHECK(!temp.path().empty());
    const std::string dir = temp.path() + "/config/xppautX";
    CHECK(set_config_dir(dir.c_str()));
    xpp::LocalFileSystem fs;
    CHECK(xpp::files::config_dir() == dir);
    check_files(dir);
    /* a relative override is an error, never a guess: no file is read */
    CHECK(set_config_dir("relative/folder"));
    const std::vector<xpp::settings::Change> one = {{"general", "threads", "2"}};
    CHECK(!xpp::settings::load_settings(fs) && !xpp::settings::set_settings(fs, one));
    set_config_dir(nullptr);
    TEST_REPORT("settings");
}
