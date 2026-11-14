/* xpp_https.h on Windows: WinHTTP, xpp_win.cpp's https_get. */
#include "xpp_https.h"
#include "xpp_win.h"

namespace xpp::https {

Result<Reply> get(std::string_view url, const Chunk &chunk) { return win::https_get(url, chunk); }

} // namespace xpp::https
