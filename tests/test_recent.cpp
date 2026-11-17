/* The recent models (W232, core/xpp_recent.h) in the settings file: the
   newest-first list kept to its limit, a bad file left as it is, and the
   hello `start` object, in a config folder of its own (XPP_CONFIG_DIR: the
   user's real file is never touched). What the file may hold is
   test_prefs.cpp's. The protocol side (hello, Cancel at the start) is
   tools/servercheck.py's start screen section. */
#include "xpptest.h"
#include "xpp_fs_local.h"
#include "test_support.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_settings.h"
#include "xpp_recent.h"
#include "model_switch.h"
#include "xpp_error.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace {
xpp::LocalFileSystem fs; /* the disk, as the program's start makes it */
}

namespace {

void check_file(const std::string &dir)
{
    const std::string file = xpp::settings::state_path(fs);
    CHECK(!file.empty() && file.find(dir.substr(dir.rfind("config"))) != std::string::npos);
    CHECK(!xpp::files::exists(file));
    CHECK(xpp::recent::load_recent(fs).value().empty()); /* a missing file is no entries */

    const std::string a = abs_path("a.odex"), b = abs_path("b.odex");
    CHECK(xpp::recent::note(fs, a).has_value() && xpp::files::exists(file));
    CHECK(xpp::recent::note(fs, b).has_value());
    CHECK((xpp::recent::load_recent(fs).value() == std::vector<std::string>{b, a})); /* newest first */
    CHECK(xpp::recent::note(fs, a).has_value());
    CHECK((xpp::recent::load_recent(fs).value() == std::vector<std::string>{a, b})); /* moved up, not repeated */

    /* the list is cut at the limit, the oldest going */
    for (std::size_t i = 0; i < xpp::settings::MAX_RECENT_MODELS + 3; i++) CHECK(xpp::recent::note(fs, abs_path("n" + std::to_string(i) + ".odex")).has_value());
    const auto full = xpp::recent::load_recent(fs);
    CHECK(full && full->size() == xpp::settings::MAX_RECENT_MODELS && full->front() == abs_path("n" + std::to_string(xpp::settings::MAX_RECENT_MODELS + 2) + ".odex"));

    /* hello's start: every entry listed, one that is not there marked missing, not dropped */
    const std::string start = xpp::recent::start_json(fs, full);
    CHECK(start.find("\"missing\":true") != std::string::npos && start.find("\"error\"") == std::string::npos);

    /* a bad file is the error, shown with no entries; note leaves it as it is */
    xpp::Writer w = xpp::Writer::binary(file);
    CHECK(w && w.write("recent_models: [\"relative.odex\"],\n") && w.commit().has_value());
    CHECK(!xpp::recent::load_recent(fs));
    const std::string shown = xpp::recent::start_json(fs, xpp::recent::load_recent(fs));
    CHECK(shown.find("\"recent\":[]") != std::string::npos && shown.find("\"line\":1") != std::string::npos);
    CHECK(!xpp::recent::note(fs, a));
    std::string kept;
    CHECK(xpp::read_bytes(file, kept) && kept == "recent_models: [\"relative.odex\"],\n");
}

} // namespace

TEST_CASE("test_recent")
{
    xpp::TempDir temp;
    CHECK(!temp.path().empty());
    const std::string dir = temp.path() + "/config/xppautX";
    CHECK(set_config_dir(dir.c_str()));
    check_file(dir);
    std::filesystem::remove_all(temp.path() + "/config");
    /* a relative override would follow the working folder: an error, never a guess */
    CHECK(set_config_dir("relative/folder"));
    CHECK(xpp::settings::state_path(fs).empty());
    const auto none = xpp::recent::load_recent(fs);
    CHECK(!none && none.error().what.find("XPP_CONFIG_DIR") != std::string::npos && !xpp::recent::note(fs, abs_path("a.odex")));
    set_config_dir(nullptr);
    TEST_REPORT("recent");
}
