/* The preferences file (W287, core/xpp_prefs.h): what a file may hold, every
   kind of bad file refused whole with its file, line and value, the round
   trip, a missing file meaning the defaults, no config folder the shown
   error, and every list and the file bounded; in a config folder of its own
   (XPP_CONFIG_DIR: the user's real file is never touched). */
#include "xpptest.h"
#include "xpp_cpu.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_prefs.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace {

constexpr const char *FILE_LABEL = "p.setx";

bool set_config_dir(const char *dir)
{
#ifdef _WIN32
    return _putenv_s(xpp::files::CONFIG_DIR_ENV, dir ? dir : "") == 0;
#else
    return dir ? setenv(xpp::files::CONFIG_DIR_ENV, dir, 1) == 0 : unsetenv(xpp::files::CONFIG_DIR_ENV) == 0;
#endif
}

/* the absolute path of a made-up file, on either system */
std::string abs_path(const std::string &name)
{
#ifdef _WIN32
    return "C:/models/" + name;
#else
    return "/models/" + name;
#endif
}

std::string quoted(const std::string &path) { return "\"" + path + "\""; }

struct Bad {
    const char *what;
    std::string text;
    int line;
    const char *value; /* in the error's text */
};

void check_bad_files()
{
    const std::string a = quoted(abs_path("a.odex")), b = quoted(abs_path("b.odex"));
    std::vector<Bad> bad = {
        {"a relative path", "recent_models: [" + a + ",\n  \"relative/m.odex\"],\n", 2, "relative/m.odex"},
        {"an empty path", "recent_models: [" + a + ",\n  \"\"],\n", 2, "empty"},
        {"a path listed twice", "recent_models: [" + a + ",\n" + b + ",\n" + a + "],\n", 3, "twice"},
        {"a control character", "recent_models: [" + a + ",\n" + quoted(abs_path("b\x01x")) + "],\n", 2, "control"},
        {"a path past the limit", "recent_models: [" + a + ",\n" + quoted(abs_path(std::string(xpp::prefs::MAX_PATH_BYTES, 'x'))) + "],\n", 2, "longer"},
        {"a group where a list is due", "recent_models: { x: 1 },\n", 1, "list"},
        {"a number where a path is due", "recent_models: [3],\n", 1, "absolute"},
        {"a key that is not a preference", "recent_models: [" + a + "],\nsurprise: 1,\n", 2, "surprise"},
    };
    std::string many = "recent_models: [";
    for (std::size_t i = 0; i <= xpp::prefs::MAX_RECENT_MODELS; i++) many += quoted(abs_path("m" + std::to_string(i) + ".odex")) + ",";
    bad.push_back({"more entries than are kept", many + "],\n", 1, "at most"});
    for (const Bad &t : bad) {
        const auto r = xpp::prefs::parse_prefs(t.text, FILE_LABEL);
        CHECK(!r);
        if (r) continue;
        if (r.error().place.file != FILE_LABEL || r.error().place.line != t.line || r.error().what.find(t.value) == std::string::npos) {
            printf("bad file %s: %s\n", t.what, r.error().text().c_str());
            CHECK(false);
        }
    }
    /* a syntax error is the reader's, with the file */
    const auto syntax = xpp::prefs::parse_prefs("recent_models: [\n", FILE_LABEL);
    CHECK(!syntax && syntax.error().place.file == FILE_LABEL);
    /* a file past the byte limit is refused before it is read */
    const auto huge = xpp::prefs::parse_prefs(std::string(xpp::prefs::MAX_FILE_BYTES + 1, 'x'), FILE_LABEL);
    CHECK(!huge && huge.error().what.find("longer than") != std::string::npos);
}

/* "Allow always" answers (W286): [ { model, path } ... ] */
std::string entry(const std::string &model, const std::string &path)
{
    return "{ model: " + quoted(abs_path(model)) + ", path: " + quoted(abs_path(path)) + " }";
}

void check_outside_paths()
{
    const std::string a = entry("a.odex", "x.tab"), b = entry("a.odex", "y.tab");
    for (const Bad &t : std::vector<Bad>{
             {"a pair without its path", "outside_paths: [{ model: " + quoted(abs_path("a.odex")) + " }],\n", 1, "needs a"},
             {"a relative path", "outside_paths: [" + a + ",\n{ model: " + quoted(abs_path("a.odex")) + ", path: \"rel/x.tab\" }],\n", 2, "rel/x.tab"},
             {"a relative model", "outside_paths: [\n{ model: \"a.odex\", path: " + quoted(abs_path("x.tab")) + " }],\n", 2, "a.odex"},
             {"a pair listed twice", "outside_paths: [" + a + ",\n" + b + ",\n" + a + "],\n", 3, "twice"},
             {"a key of an entry that is not one", "outside_paths: [{ model: " + quoted(abs_path("a.odex")) + ", path: " + quoted(abs_path("x")) + ",\n surprise: 1 }],\n", 2, "surprise"},
             {"an entry that is not a group", "outside_paths: [3],\n", 1, "group"},
         }) {
        const auto r = xpp::prefs::parse_prefs(t.text, FILE_LABEL);
        CHECK(!r);
        if (r) continue;
        if (r.error().place.line != t.line || r.error().what.find(t.value) == std::string::npos) {
            printf("bad outside_paths %s: %s\n", t.what, r.error().text().c_str());
            CHECK(false);
        }
    }
    std::string many = "outside_paths: [";
    for (std::size_t i = 0; i <= xpp::prefs::MAX_OUTSIDE_APPROVALS; i++) many += entry("a.odex", "t" + std::to_string(i)) + ",\n";
    const auto too_many = xpp::prefs::parse_prefs(many + "],\n", FILE_LABEL);
    CHECK(!too_many && too_many.error().what.find("at most") != std::string::npos);

    /* the round trip; the newest first, a repeat moves to the front, the oldest dropped when full */
    xpp::prefs::Preferences p;
    for (std::size_t i = 0; i < xpp::prefs::MAX_OUTSIDE_APPROVALS; i++)
        xpp::prefs::remember_newest(p.outside_paths, xpp::prefs::OutsideApproval{abs_path("a.odex"), abs_path("t" + std::to_string(i))},
                                    xpp::prefs::MAX_OUTSIDE_APPROVALS);
    CHECK(p.outside_paths.size() == xpp::prefs::MAX_OUTSIDE_APPROVALS && p.outside_paths.front().path == abs_path("t23"));
    xpp::prefs::remember_newest(p.outside_paths, xpp::prefs::OutsideApproval{abs_path("a.odex"), abs_path("t5")}, xpp::prefs::MAX_OUTSIDE_APPROVALS);
    CHECK(p.outside_paths.size() == xpp::prefs::MAX_OUTSIDE_APPROVALS && p.outside_paths.front().path == abs_path("t5"));
    xpp::prefs::remember_newest(p.outside_paths, xpp::prefs::OutsideApproval{abs_path("b.odex"), abs_path("new")}, xpp::prefs::MAX_OUTSIDE_APPROVALS);
    CHECK(p.outside_paths.size() == xpp::prefs::MAX_OUTSIDE_APPROVALS && p.outside_paths.back().path == abs_path("t1")); /* t0 went */
    const auto text = xpp::prefs::serialize_prefs(p);
    CHECK(text.has_value());
    const auto back = xpp::prefs::parse_prefs(*text, FILE_LABEL);
    CHECK(back && back->outside_paths == p.outside_paths);
    xpp::prefs::Preferences bad;
    bad.outside_paths = {{abs_path("a.odex"), "relative"}};
    CHECK(!xpp::prefs::serialize_prefs(bad));
}

void check_good_files()
{
    /* an empty file, or one with no recent models, is the defaults */
    CHECK(xpp::prefs::parse_prefs("", FILE_LABEL).value().recent_models.empty());
    CHECK(xpp::prefs::parse_prefs("recent_models: [],\n", FILE_LABEL).value().recent_models.empty());
    /* the round trip, with a path that needs quoting and one with a backslash, the list at its limit */
    xpp::prefs::Preferences p;
    p.recent_models = {abs_path("a.odex"), abs_path("b c.odex"), abs_path("d\\e.odex")};
    for (std::size_t i = p.recent_models.size(); i < xpp::prefs::MAX_RECENT_MODELS; i++) p.recent_models.push_back(abs_path("m" + std::to_string(i)));
    const auto text = xpp::prefs::serialize_prefs(p);
    CHECK(text.has_value());
    const auto back = xpp::prefs::parse_prefs(*text, FILE_LABEL);
    CHECK(back && back->recent_models == p.recent_models);
    /* a CRLF file is read */
    std::string crlf;
    for (const char c : *text) crlf += c == '\n' ? std::string("\r\n") : std::string(1, c);
    CHECK(xpp::prefs::parse_prefs(crlf, FILE_LABEL).value().recent_models == p.recent_models);
    /* a value that cannot be written is refused when saving, not written as a file that does not read */
    xpp::prefs::Preferences bad;
    bad.recent_models = {"relative.odex"};
    CHECK(!xpp::prefs::serialize_prefs(bad));
    bad.recent_models = {abs_path("a\nb")};
    CHECK(!xpp::prefs::serialize_prefs(bad));
}

/* AUTO's thread count (W267): "auto" or a whole number from 1 to the logical processors */
void check_auto_threads()
{
    using xpp::prefs::AUTO_THREADS_AUTO;
    const unsigned most = xpp::cpu::logical_processors();
    CHECK(xpp::prefs::parse_prefs("", FILE_LABEL).value().auto_threads == AUTO_THREADS_AUTO);
    CHECK(xpp::prefs::parse_prefs("auto_threads: auto,\n", FILE_LABEL).value().auto_threads == AUTO_THREADS_AUTO);
    /* every count the computer has round-trips; the list beside it is untouched */
    for (unsigned n = 1; n <= most; n++) {
        xpp::prefs::Preferences p;
        p.recent_models = {abs_path("a.odex")};
        p.auto_threads = n;
        const auto text = xpp::prefs::serialize_prefs(p);
        CHECK(text.has_value());
        const auto back = xpp::prefs::parse_prefs(*text, FILE_LABEL);
        CHECK(back && back->auto_threads == n && back->recent_models == p.recent_models);
    }
    const auto automatic = xpp::prefs::serialize_prefs({});
    CHECK(automatic && xpp::prefs::parse_prefs(*automatic, FILE_LABEL).value().auto_threads == AUTO_THREADS_AUTO);
    /* a bad value is refused at its line, with the value */
    const std::string beyond = std::to_string(most + 1);
    const Bad bad[] = {
        {"zero", "recent_models: [],\nauto_threads: 0,\n", 2, "from 1 to"},
        {"negative", "auto_threads: -1,\n", 1, "from 1 to"},
        {"past the processors", "\nauto_threads: " + beyond + ",\n", 2, "from 1 to"},
        {"a word", "auto_threads: many,\n", 1, "many"},
        {"a fraction", "auto_threads: 1.5,\n", 1, "1.5"},
        {"a list", "auto_threads: [2],\n", 1, "auto_threads"},
    };
    for (const Bad &t : bad) {
        const auto r = xpp::prefs::parse_prefs(t.text, FILE_LABEL);
        CHECK(!r);
        if (r) continue;
        if ((r.error().place.file != FILE_LABEL || r.error().place.line != t.line || r.error().what.find(t.value) == std::string::npos)) {
            printf("bad threads %s: %s\n", t.what, r.error().text().c_str());
            CHECK(false);
        }
    }
    xpp::prefs::Preferences p;
    p.auto_threads = most + 1;
    CHECK(!xpp::prefs::serialize_prefs(p)); /* not written as a file that does not read */
    CHECK(!xpp::prefs::auto_threads_problem(1) && !xpp::prefs::auto_threads_problem(most));
    CHECK(xpp::prefs::auto_threads_problem(0) && xpp::prefs::auto_threads_problem(most + 1));
}

void check_file()
{
    const std::string file = xpp::prefs::prefs_path();
    CHECK(!file.empty() && !xpp::files::exists(file));
    CHECK(xpp::prefs::load_prefs().value().recent_models.empty()); /* a missing file is the defaults */

    xpp::prefs::Preferences p;
    p.recent_models = {abs_path("a.odex")};
    CHECK(xpp::prefs::save_prefs(p).has_value() && xpp::files::exists(file));
    CHECK(xpp::prefs::load_prefs().value().recent_models == p.recent_models);
    /* an answer kept is found for its model and its path only */
    CHECK(xpp::prefs::approve_outside_path(abs_path("a.odex"), abs_path("x.tab")).has_value());
    CHECK(xpp::prefs::outside_path_approved(abs_path("a.odex"), abs_path("x.tab")).value());
    CHECK(!xpp::prefs::outside_path_approved(abs_path("a.odex"), abs_path("y.tab")).value());
    CHECK(!xpp::prefs::outside_path_approved(abs_path("b.odex"), abs_path("x.tab")).value());
    CHECK(xpp::prefs::load_prefs().value().recent_models == p.recent_models); /* the other group untouched */
    CHECK(xpp::prefs::update_prefs([](xpp::prefs::Preferences &q) { q.recent_models.push_back(abs_path("b.odex")); }).has_value());
    CHECK((xpp::prefs::load_prefs().value().recent_models == std::vector<std::string>{abs_path("a.odex"), abs_path("b.odex")}));

    /* a bad file is the error with its file and line; update leaves it as it is, nothing applied */
    xpp::Writer w = xpp::Writer::binary(file);
    const std::string badtext = "recent_models: [\"relative.odex\"],\n";
    CHECK(w && w.write(badtext) && w.commit().has_value());
    const auto r = xpp::prefs::load_prefs();
    CHECK(!r && r.error().place.file == file && r.error().place.line == 1);
    bool called = false;
    CHECK(!xpp::prefs::update_prefs([&](xpp::prefs::Preferences &) { called = true; }));
    CHECK(!called);
    std::string kept;
    CHECK(xpp::read_bytes(file, kept) && kept == badtext);
}

} // namespace

int main(void)
{
    check_bad_files();
    check_outside_paths();
    check_good_files();
    check_auto_threads();

    xpp::TempDir temp;
    CHECK(!temp.path().empty());
    const std::string dir = temp.path() + "/config/xppautX";
    CHECK(set_config_dir(dir.c_str()));
    check_file();
    std::filesystem::remove_all(temp.path() + "/config");
    /* a relative override would follow the working folder: an error naming the variable, never a guess */
    CHECK(set_config_dir("relative/folder"));
    CHECK(xpp::prefs::prefs_path().empty());
    const auto none = xpp::prefs::load_prefs();
    CHECK(!none && none.error().what.find("XPP_CONFIG_DIR") != std::string::npos);
    CHECK(!xpp::prefs::save_prefs({}) && !xpp::prefs::update_prefs([](xpp::prefs::Preferences &) {}));
    set_config_dir(nullptr);
    TEST_REPORT("prefs");
}
