/* xpp_https.h on Windows: WinHTTP, xpp_win32.cpp's https_get. */
#include "xpp_https.h"
#include "xpp_win32.h"

namespace xpp::https {

Result<Reply> get(std::string_view url, const Chunk &chunk) { return win32::https_get(url, chunk); }

} // namespace xpp::https
