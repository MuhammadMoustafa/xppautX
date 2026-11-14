#pragma once

/* xpp_win.cpp, Windows only: the UTF-8 <-> UTF-16 conversion the *_win.cpp files share
   (the core's text is UTF-8, the Windows API's wide). Declares no Windows type, so
   any file may include it. */
#include <string>
#include <string_view>

namespace xpp::win {
/* UTF-16 of UTF-8 text; empty when the text is empty or not valid UTF-8 */
std::wstring widen(std::string_view text);
/* UTF-8 of UTF-16 text; empty when the text is empty */
std::string narrow(std::wstring_view text);
} // namespace xpp::win
