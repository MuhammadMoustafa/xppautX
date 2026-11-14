/* xpp_platform.h's launch on Linux: nothing is added to it (macOS's Finder is the one that does). */
#include "xpp_platform.h"

namespace xpp::platform {

bool ignored_launch_argument(std::string_view) { return false; }

bool adopt_launch_document(std::vector<std::string> &) { return false; }

} // namespace xpp::platform
