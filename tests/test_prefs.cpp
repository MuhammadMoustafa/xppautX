/* The preferences file (W287, core/xpp_prefs.h): what a file may hold, every
   kind of bad file refused whole with its file, line and value, the round
   trip, a missing file meaning the defaults, no config folder the shown
   error, and every list and the file bounded; in a config folder of its own
   (XPP_CONFIG_DIR: the user's real file is never touched). */
#include "xpptest.h"
#include "test_support.h"
#include "xpp_cpu.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_prefs.h"
#include "format_version.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace {

constexpr const char *FILE_LABEL = "p.odex";

/* a preferences file's text: the format version, then the statement holding text, which every position is two lines below */
std::string versioned_prefs(const std::string &text) { return "version " + std::to_string(xpp::format_version) + "\npreferences {\n" + text + "}\n"; }
xpp::Result<xpp::prefs::Preferences> parse(const std::string &text) { return xpp::prefs::parse_prefs(versioned_prefs(text), FILE_LABEL); }

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
        {"a number where a path is due", "recent_models: [3],\n", 1, "double quotes"},
        {"a key that is not a preference", "recent_models: [" + a + "],\nsurprise: 1,\n", 2, "surprise"},
    };
    std::string many = "recent_models: [";
    for (std::size_t i = 0; i <= xpp::prefs::MAX_RECENT_MODELS; i++) many += quoted(abs_path("m" + std::to_string(i) + ".odex")) + ",";
    bad.push_back({"more entries than are kept", many + "],\n", 1, "at most"});
    for (const Bad &t : bad) {
        const auto r = parse(t.text);
        CHECK(!r);
        if (r) continue;
        if (r.error().place.file != FILE_LABEL || r.error().place.line != t.line + 2 || r.error().what.find(t.value) == std::string::npos) {
            printf("bad file %s: %s\n", t.what, r.error().text().c_str());
            CHECK(false);
        }
    }
    /* the version: none, or another, is refused naming both versions */
    for (const auto &[text, wanted] : {std::pair<std::string, std::string>{"preferences {}\n", "this program reads preferences files of version 4"},
                                       {"version 3\npreferences {}\n", "version 3: this program reads preferences files of version 4"}}) {
        const auto refused = xpp::prefs::parse_prefs(text, FILE_LABEL);
        CHECK(!refused && refused.error().what.find(wanted) != std::string::npos);
    }
    /* a syntax error is the reader's, with the file */
    const auto syntax = parse("recent_models: [\n");
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
        const auto r = parse(t.text);
        CHECK(!r);
        if (r) continue;
        if (r.error().place.line != t.line + 2 || r.error().what.find(t.value) == std::string::npos) {
            printf("bad outside_paths %s: %s\n", t.what, r.error().text().c_str());
            CHECK(false);
        }
    }
    std::string many = "outside_paths: [";
    for (std::size_t i = 0; i <= xpp::prefs::MAX_OUTSIDE_APPROVALS; i++) many += entry("a.odex", "t" + std::to_string(i)) + ",\n";
    const auto too_many = parse(many + "],\n");
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
    CHECK(parse("").value().recent_models.empty());
    CHECK(parse("recent_models: [],\n").value().recent_models.empty());
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

/* autoX's thread count (W267): "auto" or a whole number from 1 to the logical processors */
void check_threads()
{
    using xpp::prefs::THREADS_AUTO;
    const unsigned most = xpp::cpu::logical_processors();
    CHECK(parse("").value().threads == THREADS_AUTO);
    CHECK(parse("threads: auto,\n").value().threads == THREADS_AUTO);
    /* every count the computer has round-trips; the list beside it is untouched */
    for (unsigned n = 1; n <= most; n++) {
        xpp::prefs::Preferences p;
        p.recent_models = {abs_path("a.odex")};
        p.threads = n;
        const auto text = xpp::prefs::serialize_prefs(p);
        CHECK(text.has_value());
        const auto back = xpp::prefs::parse_prefs(*text, FILE_LABEL);
        CHECK(back && back->threads == n && back->recent_models == p.recent_models);
    }
    const auto automatic = xpp::prefs::serialize_prefs({});
    CHECK(automatic && xpp::prefs::parse_prefs(*automatic, FILE_LABEL).value().threads == THREADS_AUTO);
    /* a bad value is refused at its line, with the value */
    const std::string beyond = std::to_string(most + 1);
    const Bad bad[] = {
        {"zero", "recent_models: [],\nthreads: 0,\n", 2, "from 1 to"},
        {"negative", "threads: -1,\n", 1, "from 1 to"},
        {"past the processors", "\nthreads: " + beyond + ",\n", 2, "from 1 to"},
        {"a word", "threads: many,\n", 1, "many"},
        {"a fraction", "threads: 1.5,\n", 1, "1.5"},
        {"a list", "threads: [2],\n", 1, "threads"},
    };
    for (const Bad &t : bad) {
        const auto r = parse(t.text);
        CHECK(!r);
        if (r) continue;
        if ((r.error().place.file != FILE_LABEL || r.error().place.line != t.line + 2 || r.error().what.find(t.value) == std::string::npos)) {
            printf("bad threads %s: %s\n", t.what, r.error().text().c_str());
            CHECK(false);
        }
    }
    xpp::prefs::Preferences p;
    p.threads = most + 1;
    CHECK(!xpp::prefs::serialize_prefs(p)); /* not written as a file that does not read */
    CHECK(!xpp::prefs::threads_problem(1) && !xpp::prefs::threads_problem(most));
    CHECK(xpp::prefs::threads_problem(0) && xpp::prefs::threads_problem(most + 1));
}

/* the colors (W655): each by its key, a quoted color; none is XPPAUT's defaults, a bad one is refused at its line */
void check_colors()
{
    const xpp::prefs::Preferences none = parse("").value();
    CHECK(none.colors.stable_equilibrium == xpp::xppaut_color(1) && none.colors.unstable_manifold == xpp::xppaut_color(5));
    xpp::prefs::Preferences p;
    for (const xpp::prefs::ColorKey &c : xpp::prefs::COLOR_KEYS) p.colors.*c.member = xpp::xppaut_color(3);
    p.colors.y_nullcline = xpp::Color{1, 2, 3, 128, false};
    const auto text = xpp::prefs::serialize_prefs(p);
    CHECK(text.has_value());
    const auto back = xpp::prefs::parse_prefs(*text, FILE_LABEL);
    CHECK(back && back->colors.x_nullcline == xpp::xppaut_color(3) && back->colors.y_nullcline == p.colors.y_nullcline);
    for (const char *bad : {"x_nullcline_color: red,\n", "x_nullcline_color: \"nosuch\",\n"}) {
        const auto r = parse(bad);
        CHECK(!r && r.error().place.line == 3);
        if (r) printf("color accepted: %s\n", bad);
        else if (r.error().place.line != 3) printf("bad color: %s\n", r.error().text().c_str());
    }
}

/* the log (W619): its level by name and the file it goes to, an absolute path; none is the defaults */
void check_log()
{
    CHECK(parse("").value().log_level == XPP_LOG_WARN && parse("").value().log_file.empty());
    for (std::size_t i = 0; i < xpp::log_level_names.size(); i++) {
        xpp::prefs::Preferences p;
        p.log_level = static_cast<XppLogLevel>(i);
        p.log_file = abs_path("log.txt");
        const auto text = xpp::prefs::serialize_prefs(p);
        CHECK(text.has_value());
        const auto back = xpp::prefs::parse_prefs(*text, FILE_LABEL);
        CHECK(back && back->log_level == p.log_level && back->log_file == p.log_file);
    }
    const Bad bad[] = {
        {"a number", "log_level: 2,\n", 1, "`2` is none of"},
        {"another word", "\nlog_level: loud,\n", 2, "loud"},
        {"a relative path", "log_file: \"log.txt\",\n", 1, "not an absolute path"},
        {"a bare word", "log_file: log,\n", 1, "log_file"},
    };
    for (const Bad &t : bad) {
        const auto r = parse(t.text);
        CHECK(!r);
        if (r) continue;
        if (r.error().place.file != FILE_LABEL || r.error().place.line != t.line + 2 || r.error().what.find(t.value) == std::string::npos) {
            printf("bad log %s: %s\n", t.what, r.error().text().c_str());
            CHECK(false);
        }
    }
    xpp::prefs::Preferences p;
    p.log_file = "relative.txt";
    CHECK(!xpp::prefs::serialize_prefs(p));
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
    const std::string badtext = versioned_prefs("recent_models: [\"relative.odex\"],\n");
    CHECK(w && w.write(badtext) && w.commit().has_value());
    const auto r = xpp::prefs::load_prefs();
    CHECK(!r && r.error().place.file == file && r.error().place.line == 3);
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
    check_threads();
    check_log();
    check_colors();

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
