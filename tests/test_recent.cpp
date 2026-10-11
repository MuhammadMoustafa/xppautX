/* The recent models (W232, core/xpp_recent.h) in the preferences file: the
   newest-first list kept to its limit, a bad file left as it is, and the
   hello `start` object, in a config folder of its own (XPP_CONFIG_DIR: the
   user's real file is never touched). What the file may hold is
   test_prefs.cpp's. The protocol side (hello, Cancel at the start) is
   tools/servercheck.py's start screen section. */
#include "xpptest.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_prefs.h"
#include "xpp_recent.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {

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

void check_file(const std::string &dir)
{
    const std::string file = xpp::prefs::prefs_path();
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
    for (std::size_t i = 0; i < xpp::prefs::MAX_RECENT_MODELS + 3; i++) CHECK(xpp::recent::note(abs_path("n" + std::to_string(i) + ".odex")).has_value());
    const auto full = xpp::recent::load_recent();
    CHECK(full && full->size() == xpp::prefs::MAX_RECENT_MODELS && full->front() == abs_path("n" + std::to_string(xpp::prefs::MAX_RECENT_MODELS + 2) + ".odex"));

    /* hello's start: every entry listed, one that is not there marked missing, not dropped */
    const std::string start = xpp::recent::start_json(full);
    CHECK(start.find("\"missing\":true") != std::string::npos && start.find("\"error\"") == std::string::npos);

    /* a bad file is the error, shown with no entries; note leaves it as it is */
    xpp::Writer w = xpp::Writer::binary(file);
    CHECK(w && w.write("recent_models: [\"relative.odex\"],\n") && w.commit().has_value());
    CHECK(!xpp::recent::load_recent());
    const std::string shown = xpp::recent::start_json(xpp::recent::load_recent());
    CHECK(shown.find("\"recent\":[]") != std::string::npos && shown.find("\"line\":1") != std::string::npos);
    CHECK(!xpp::recent::note(a));
    std::string kept;
    CHECK(xpp::read_bytes(file, kept) && kept == "recent_models: [\"relative.odex\"],\n");
}

} // namespace

int main(void)
{
    xpp::TempDir temp;
    CHECK(!temp.path().empty());
    const std::string dir = temp.path() + "/config/xppautX";
    CHECK(set_config_dir(dir.c_str()));
    check_file(dir);
    std::filesystem::remove_all(temp.path() + "/config");
    /* a relative override would follow the working folder: an error, never a guess */
    CHECK(set_config_dir("relative/folder"));
    CHECK(xpp::prefs::prefs_path().empty());
    const auto none = xpp::recent::load_recent();
    CHECK(!none && none.error().what.find("XPP_CONFIG_DIR") != std::string::npos && !xpp::recent::note(abs_path("a.odex")));
    set_config_dir(nullptr);
    TEST_REPORT("recent");
}
