/* xpp_https.h on macOS: no HTTPS client yet (W625: its update is the release page). */
#include "xpp_https.h"

#include <string>

namespace xpp::https {

Result<Reply> get(std::string_view, const Chunk &)
{
    return fail(std::string(WHERE), "this system has no HTTPS client for updates yet: open the release page", Place{std::string(PLACE), 1});
}

} // namespace xpp::https
