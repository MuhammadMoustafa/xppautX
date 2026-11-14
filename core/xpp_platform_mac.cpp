/* xpp_platform.h's launch word on macOS: what Finder adds to starting xppautX.app (the document it opens is
   xpp_window.h's adopt_launch_document: the window is not in the core library this file is in). */
#include "xpp_platform.h"

namespace xpp::platform {

/* the process serial number an older macOS gave an app Finder started, which would be taken for a bad option */
bool ignored_launch_argument(std::string_view word) { return word.starts_with("-psn_"); }

} // namespace xpp::platform
