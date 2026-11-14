/* The UTF-8 <-> UTF-16 conversion shared by the *_win.cpp files (xpp_win.h). Windows only
   (the Makefile builds it nowhere else). <windows.h> is included only here and in the
   *_win.cpp files (never a header): its macros clash with core names such as max,
   MessageBox and VARTYPE. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "xpp_win.h"

namespace xpp::win {

std::optional<std::wstring> widen(std::string_view text, InvalidUtf8 invalid)
{
    if (text.empty()) return std::wstring{};
    const DWORD flags = invalid == InvalidUtf8::refuse ? MB_ERR_INVALID_CHARS : 0;
    const int n = MultiByteToWideChar(CP_UTF8, flags, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (n <= 0) return std::nullopt;
    std::wstring wide(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, flags, text.data(), static_cast<int>(text.size()), wide.data(), n);
    return wide;
}

std::string narrow(std::wstring_view text)
{
    if (text.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), n, nullptr, nullptr);
    return out;
}

} // namespace xpp::win
