/* xpp_https.h: the Linux client, libcurl loaded when the first request is made. Windows is xpp_win32.cpp's
   WinHTTP; macOS has none yet (W625: its update is the release page). */
#include "xpp_https.h"

#include <string>
#include <cstddef>

#include "xpp_io.h"
#ifdef _WIN32
#include "xpp_win32.h"
#endif
#ifdef __linux__
#include <dlfcn.h>
#endif

namespace xpp::https {

namespace {

#ifndef _WIN32 // Windows' errors are xpp_win32.cpp's
constexpr std::string_view WHERE = "https";
#endif

#ifdef __linux__

/* the library's name and the numbers of the few options and infos used, which libcurl's ABI keeps (curl/curl.h:
   CURLOPT_* are a base plus an id, CURLINFO_* a type plus an id) */
constexpr const char *LIBRARY = "libcurl.so.4";
enum : int {
    OPT_WRITEDATA = 10001, OPT_URL = 10002, OPT_USERAGENT = 10018, OPT_XFERINFODATA = 10057, OPT_NOPROGRESS = 43,
    OPT_FOLLOWLOCATION = 52, OPT_LOW_SPEED_LIMIT = 19, OPT_LOW_SPEED_TIME = 20, OPT_CONNECTTIMEOUT = 78,
    OPT_NOSIGNAL = 99, OPT_WRITEFUNCTION = 20011, OPT_XFERINFOFUNCTION = 20219, OPT_ERRORBUFFER = 10010,
    INFO_RESPONSE_CODE = 0x200002, INFO_REDIRECT_URL = 0x100000 + 31,
    CURLE_OK = 0, CURLE_WRITE_ERROR = 23, ERROR_TEXT_BYTES = 256
};
/* a stalled body is one under this many bytes a second for STALL_SECONDS */
constexpr long STALL_BYTES_PER_SECOND = 1;

struct Api {
    void *library = nullptr;
    void *(*init)() = nullptr;
    int (*setopt)(void *, int, ...) = nullptr;
    int (*perform)(void *) = nullptr;
    int (*getinfo)(void *, int, ...) = nullptr;
    void (*cleanup)(void *) = nullptr;
    std::string problem; /* why the library could not be had, "" when it could */
};

/* the library, loaded once (kept for the life of the program) */
const Api &api()
{
    static const Api loaded = [] {
        Api a;
        a.library = dlopen(LIBRARY, RTLD_NOW | RTLD_LOCAL);
        if (!a.library) {
            const char *why = dlerror();
            a.problem = xpp::format("{} could not be loaded ({})", LIBRARY, why ? why : "dlopen failed");
            return a;
        }
        const auto symbol = [&](const char *name) { return dlsym(a.library, name); };
        a.init = reinterpret_cast<void *(*)()>(symbol("curl_easy_init"));
        a.setopt = reinterpret_cast<int (*)(void *, int, ...)>(symbol("curl_easy_setopt"));
        a.perform = reinterpret_cast<int (*)(void *)>(symbol("curl_easy_perform"));
        a.getinfo = reinterpret_cast<int (*)(void *, int, ...)>(symbol("curl_easy_getinfo"));
        a.cleanup = reinterpret_cast<void (*)(void *)>(symbol("curl_easy_cleanup"));
        if (!a.init || !a.setopt || !a.perform || !a.getinfo || !a.cleanup) a.problem = xpp::format("{} has no libcurl easy interface", LIBRARY);
        return a;
    }();
    return loaded;
}

struct Transfer {
    const Chunk *chunk;
    const Api *curl;
    void *handle;
    int status = 0;
    bool stopped = false; /* the chunk said stop */
};

/* the body: only an answer with status 200 is handed on; any other answer's body is dropped by ending the request,
   its status and (for a redirect) Location are read after */
std::size_t on_body(char *data, std::size_t size, std::size_t count, void *user)
{
    auto &t = *static_cast<Transfer *>(user);
    if (t.status == 0) {
        long code = 0;
        t.curl->getinfo(t.handle, INFO_RESPONSE_CODE, &code);
        t.status = static_cast<int>(code);
    }
    if (t.status != 200) return 0;
    const std::size_t n = size * count;
    if (!(*t.chunk)(std::string_view(data, n))) {
        t.stopped = true;
        return 0;
    }
    return n;
}

/* called by libcurl about once a second, data or none: a stop is seen while a connection stalls */
int on_progress(void *user, long long, long long, long long, long long)
{
    auto &t = *static_cast<Transfer *>(user);
    if (t.status == 200 && !(*t.chunk)(std::string_view())) {
        t.stopped = true;
        return 1;
    }
    return 0;
}

struct Handle {
    const Api &curl;
    void *h;
    ~Handle()
    {
        if (h) curl.cleanup(h);
    }
};

Result<Reply> get_with_curl(std::string_view url, const Chunk &chunk)
{
    const Api &curl = api();
    if (!curl.problem.empty()) return fail(std::string(WHERE), curl.problem + ": the update cannot be downloaded here, open the release page", Place{std::string(PLACE), 1});
    Handle handle{curl, curl.init()};
    if (!handle.h) return fail(std::string(WHERE), "libcurl could not start a request", Place{std::string(PLACE), 1});
    const std::string address(url); /* NUL-terminated for libcurl; it copies it */
    std::string error(ERROR_TEXT_BYTES, '\0');
    Transfer transfer{&chunk, &curl, handle.h};
    int rc = curl.setopt(handle.h, OPT_URL, address.c_str());
    /* no redirect is followed: the caller checks each address (xpp_update.cpp); a signal never ends a transfer */
    rc |= curl.setopt(handle.h, OPT_FOLLOWLOCATION, 0L) | curl.setopt(handle.h, OPT_NOSIGNAL, 1L);
    rc |= curl.setopt(handle.h, OPT_USERAGENT, "xppautX") | curl.setopt(handle.h, OPT_ERRORBUFFER, error.data());
    rc |= curl.setopt(handle.h, OPT_CONNECTTIMEOUT, static_cast<long>(CONNECT_SECONDS));
    rc |= curl.setopt(handle.h, OPT_LOW_SPEED_LIMIT, STALL_BYTES_PER_SECOND) | curl.setopt(handle.h, OPT_LOW_SPEED_TIME, static_cast<long>(STALL_SECONDS));
    rc |= curl.setopt(handle.h, OPT_WRITEFUNCTION, &on_body) | curl.setopt(handle.h, OPT_WRITEDATA, &transfer);
    rc |= curl.setopt(handle.h, OPT_NOPROGRESS, 0L) | curl.setopt(handle.h, OPT_XFERINFOFUNCTION, &on_progress);
    rc |= curl.setopt(handle.h, OPT_XFERINFODATA, &transfer);
    if (rc != CURLE_OK) return fail(std::string(WHERE), "libcurl refused an option: it is too old for the update", Place{std::string(PLACE), 1});
    const int result = curl.perform(handle.h);
    if (transfer.status == 0) {
        long code = 0;
        curl.getinfo(handle.h, INFO_RESPONSE_CODE, &code);
        transfer.status = static_cast<int>(code);
    }
    Reply reply{transfer.status, {}};
    if (reply.status >= 300 && reply.status < 400) {
        const char *where = nullptr;
        curl.getinfo(handle.h, INFO_REDIRECT_URL, &where);
        if (where) reply.location = where;
    }
    /* a request ended on purpose (a stop, or an answer that is not a 200) is the reply as far as it got */
    const bool on_purpose = transfer.stopped || (result == CURLE_WRITE_ERROR && reply.status != 200) || result == CURLE_OK;
    if (!on_purpose) return fail(std::string(WHERE), xpp::format("the connection failed: {}", error[0] ? std::string(error.c_str()) : xpp::format("libcurl error {}", result)), Place{std::string(PLACE), 1});
    return reply;
}

#endif

} // namespace

Result<Reply> get([[maybe_unused]] std::string_view url, [[maybe_unused]] const Chunk &chunk)
{
#ifdef _WIN32
    return win32::https_get(url, chunk);
#elif defined(__linux__)
    return get_with_curl(url, chunk);
#else
    return fail(std::string(WHERE), "this system has no HTTPS client for updates yet: open the release page", Place{std::string(PLACE), 1});
#endif
}

} // namespace xpp::https
