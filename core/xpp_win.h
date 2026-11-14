#pragma once

/* xpp_win.cpp, Windows only: the UTF-8 <-> UTF-16 conversion the *_win.cpp files share
   (the core's text is UTF-8, the Windows API's wide). Declares no Windows type, so
   any file may include it. */
#include <optional>
#include <string>
#include <string_view>

namespace xpp::win {
/* what widen does with bytes that are not valid UTF-8: a name the system must find
   exactly (a path, a URL) is refused; text only shown (a dialog's title, its filter's
   label) gets U+FFFD in their place */
enum class InvalidUtf8 { refuse, replace };
/* UTF-16 of UTF-8 text (empty for empty text); nullopt only when it is not valid UTF-8
   and invalid is refuse */
std::optional<std::wstring> widen(std::string_view text, InvalidUtf8 invalid);
/* UTF-8 of UTF-16 text; empty when the text is empty */
std::string narrow(std::wstring_view text);
} // namespace xpp::win
