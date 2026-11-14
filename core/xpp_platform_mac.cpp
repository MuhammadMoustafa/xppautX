/* xpp_platform.h's launch on macOS: what Finder adds to starting xppautX.app. */
#include "xpp_platform.h"
#include "xpp_files.h"
#include "xpp_window.h"

#include <cstdlib>
#include <utility>

namespace xpp::platform {

/* the process serial number an older macOS gave an app Finder started, which would be taken for a bad option */
bool ignored_launch_argument(std::string_view word) { return word.starts_with("-psn_"); }

/* a document Finder or `open` gave xppautX.app when it launched it (an
   Apple Event, not an argument: xpp_window.h) is the model, loaded from
   its own folder as File > Open model loads one; without one, an app
   Finder started (in "/") starts in the home folder, where the
   start screen's Open model dialog begins */
bool adopt_launch_document(std::vector<std::string> &args)
{
    if (const char *doc = xpp::window::launch_document()) {
        const std::pair<std::string, std::string> where = xpp::files::split_path(doc);
        if (!where.first.empty()) xpp::files::change_dir(where.first.c_str());
        args.push_back(where.second);
        return true;
    }
    if (xpp::files::working_dir() == "/") {
        const char *home = getenv("HOME");
        if (home && *home) xpp::files::change_dir(home);
    }
    return false;
}

} // namespace xpp::platform
