/* xpp_https.h on Windows: the request through WinHTTP (the system's own client and certificate store):
   TLS verified, no redirect followed, a body that stalls ends the request. Written by reading, built by
   Windows CI (W622). */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include "xpp_https.h"
#include "xpp_win.h"

#include <string>
#include <vector>

namespace xpp::https {

namespace {

/* a WinHTTP handle, closed when it goes */
class WinHttpHandle {
public:
    explicit WinHttpHandle(HINTERNET h) : h_(h) {}
    ~WinHttpHandle()
    {
        if (h_) WinHttpCloseHandle(h_);
    }
    WinHttpHandle(const WinHttpHandle &) = delete;
    WinHttpHandle &operator=(const WinHttpHandle &) = delete;
    HINTERNET get() const { return h_; }

private:
    HINTERNET h_;
};

std::unexpected<Error> https_failure(const char *what)
{
    return fail("https", std::string(what) + " (Windows error " + std::to_string(GetLastError()) + ")", Place{std::string(PLACE), 1});
}

constexpr DWORD BODY_PIECE_BYTES = 64 * 1024; /* read this much of the body at a time */

} // namespace

Result<Reply> get(std::string_view url, const Chunk &chunk)
{
    const std::wstring wide_url = win::widen(url);
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof parts;
    parts.dwSchemeLength = parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (wide_url.empty() || !WinHttpCrackUrl(wide_url.c_str(), static_cast<DWORD>(wide_url.size()), 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS)
        return https_failure("the address is not an https address");
    const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    const std::wstring target = std::wstring(parts.lpszUrlPath, parts.dwUrlPathLength) + std::wstring(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    WinHttpHandle session(WinHttpOpen(L"xppautX", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.get()) return https_failure("WinHTTP could not start");
    constexpr int MS = 1000;
    WinHttpSetTimeouts(session.get(), CONNECT_SECONDS * MS, CONNECT_SECONDS * MS, STALL_SECONDS * MS, STALL_SECONDS * MS);
    WinHttpHandle connection(WinHttpConnect(session.get(), host.c_str(), parts.nPort, 0));
    if (!connection.get()) return https_failure("the connection could not be made");
    WinHttpHandle request(WinHttpOpenRequest(connection.get(), L"GET", target.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!request.get()) return https_failure("the request could not be made");
    /* no redirect is followed: the caller checks each address (xpp_update.cpp) */
    DWORD never = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(request.get(), WINHTTP_OPTION_REDIRECT_POLICY, &never, sizeof never);
    if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request.get(), nullptr))
        return https_failure("the connection failed");
    DWORD status = 0, size = sizeof status;
    if (!WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX))
        return https_failure("the answer has no status");
    Reply reply{static_cast<int>(status), {}};
    if (status >= 300 && status < 400) {
        DWORD bytes = 0;
        WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER, &bytes, WINHTTP_NO_HEADER_INDEX);
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && bytes > 0) {
            std::wstring location(bytes / sizeof(wchar_t), L'\0');
            if (WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, location.data(), &bytes, WINHTTP_NO_HEADER_INDEX)) {
                location.resize(bytes / sizeof(wchar_t));
                reply.location = win::narrow(location);
            }
        }
        return reply;
    }
    if (status != 200) return reply;
    std::vector<char> piece(BODY_PIECE_BYTES);
    for (;;) {
        DWORD got = 0;
        if (!WinHttpReadData(request.get(), piece.data(), BODY_PIECE_BYTES, &got)) return https_failure("the download failed");
        if (got == 0) return reply; /* the end of the body */
        if (!chunk(std::string_view(piece.data(), got))) return reply;
    }
}

} // namespace xpp::https
