/* recent.txt (W232, core/xpp_recent.h): what a file may hold, every kind of
   bad file refused whole with its file, line and value, the newest-first
   list kept to its limit, and the hello `start` object, in a config folder
   of its own (XPP_CONFIG_DIR: the user's real file is never touched). The
   protocol side (hello, Cancel at the start) is tools/servercheck.py's
   start screen section. */
#include "xpptest.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_recent.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {

constexpr const char *FILE_LABEL = "r.txt";

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

struct Bad {
    const char *what;
    std::string text;
    int line;
    const char *value; /* in the error's text */
};

void check_bad_files()
{
    const std::string a = abs_path("a.odex"), b = abs_path("b.odex");
    std::vector<Bad> bad = {
        {"an empty line", a + "\n\n" + b + "\n", 2, "empty"},
        {"a relative path", a + "\nrelative/m.odex\n", 2, "relative/m.odex"},
        {"a path listed twice", a + "\n" + b + "\n" + a + "\n", 3, "twice"},
        {"a control character", a + "\n" + b + "\x01x\n", 2, "control"},
        {"a NUL byte", a + std::string("\0x", 2) + "\n", 1, "control"},
        {"a path past the limit", a + "\n" + abs_path(std::string(xpp::recent::MAX_PATH_BYTES, 'x')) + "\n", 2, "longer"},
    };
    std::string many;
    for (std::size_t i = 0; i <= xpp::recent::MAX_ENTRIES; i++) many += abs_path("m" + std::to_string(i) + ".odex") + "\n";
    bad.push_back({"more entries than are kept", many, static_cast<int>(xpp::recent::MAX_ENTRIES) + 1, "more than"});
    for (const Bad &t : bad) {
        const auto r = xpp::recent::parse_recent(t.text, FILE_LABEL);
        CHECK(!r);
        if (r) continue;
        if (r.error().place.file != FILE_LABEL || r.error().place.line != t.line || r.error().what.find(t.value) == std::string::npos) {
            printf("bad file %s: %s\n", t.what, r.error().text().c_str());
            CHECK(false);
        }
    }
    /* a file past the byte limit is refused before a line is read */
    const auto huge = xpp::recent::parse_recent(std::string(xpp::recent::MAX_FILE_BYTES + 1, 'x'), FILE_LABEL);
    CHECK(!huge && huge.error().what.find("longer than") != std::string::npos);
}

void check_good_files()
{
    CHECK(xpp::recent::parse_recent("", FILE_LABEL).value().empty());
    const std::vector<std::string> two = {abs_path("a.odex"), abs_path("b c.odex")};
    const auto r = xpp::recent::parse_recent(xpp::recent::serialize_recent(two), FILE_LABEL);
    CHECK(r && *r == two);
    /* the last line needs no line end, and a CRLF file is read */
    CHECK(xpp::recent::parse_recent(two[0] + "\r\n" + two[1], FILE_LABEL).value() == two);
}

void check_file(const std::string &dir)
{
    const std::string file = xpp::recent::recent_path();
    CHECK(!file.empty() && file.find(dir.substr(dir.rfind("config"))) != std::string::npos);
    CHECK(!xpp::files::exists(file));
    CHECK(xpp::recent::load_recent().value().empty()); /* a missing file is no entries */

    const std::string a = abs_path("a.odex"), b = abs_path("b.odex");
    CHECK(xpp::recent::note(a).has_value() && xpp::files::exists(file));
    CHECK(xpp::recent::note(b).has_value());
    CHECK((xpp::recent::load_recent().value() == std::vector<std::string>{b, a})); /* newest first */
    CHECK(xpp::recent::note(a).has_value());
    CHECK((xpp::recent::load_recent().value() == std::vector<std::string>{a, b})); /* moved up, not repeated */

    /* the list is cut at the limit, the oldest going */
    for (std::size_t i = 0; i < xpp::recent::MAX_ENTRIES + 3; i++) CHECK(xpp::recent::note(abs_path("n" + std::to_string(i) + ".odex")).has_value());
    const auto full = xpp::recent::load_recent();
    CHECK(full && full->size() == xpp::recent::MAX_ENTRIES && full->front() == abs_path("n" + std::to_string(xpp::recent::MAX_ENTRIES + 2) + ".odex"));

    /* hello's start: every entry listed, one that is not there marked missing, not dropped */
    const std::string start = xpp::recent::start_json(full);
    CHECK(start.find("\"missing\":true") != std::string::npos && start.find("\"error\"") == std::string::npos);

    /* a bad file is the error, shown with no entries; note leaves it as it is */
    xpp::Writer w = xpp::Writer::binary(file);
    CHECK(w && w.write("not an absolute path\n") && w.commit().has_value());
    CHECK(!xpp::recent::load_recent());
    const std::string shown = xpp::recent::start_json(xpp::recent::load_recent());
    CHECK(shown.find("\"recent\":[]") != std::string::npos && shown.find("\"line\":1") != std::string::npos);
    CHECK(!xpp::recent::note(a));
    std::string kept;
    CHECK(xpp::read_bytes(file, kept) && kept == "not an absolute path\n");
}

} // namespace

int main(void)
{
    check_bad_files();
    check_good_files();

    xpp::TempDir temp;
    CHECK(!temp.path().empty());
    const std::string dir = temp.path() + "/config/xppautX";
    CHECK(set_config_dir(dir.c_str()));
    check_file(dir);
    std::filesystem::remove_all(temp.path() + "/config");
    /* a relative override would follow the working folder: an error, never a guess */
    CHECK(set_config_dir("relative/folder"));
    CHECK(xpp::recent::recent_path().empty());
    const auto none = xpp::recent::load_recent();
    CHECK(!none && none.error().what.find("XPP_CONFIG_DIR") != std::string::npos && !xpp::recent::note(abs_path("a.odex")));
    set_config_dir(nullptr);
    TEST_REPORT("recent");
}
