/* core/xpp_io.h's implementation. C API (extern "C"), C++ inside: a
   small mutex-guarded set dedupes the "truncated" warning per call site
   (file:line), so a call made every integration step does not flood the
   log. No exception crosses into C (CLAUDE.md, "C and C++"): the only
   things that can throw here are the set's own allocations, caught so a
   formatting call can never itself abort the caller. */
#include "xpp_io.h"
#include "xpp_log.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_set>

namespace {

std::mutex g_warned_mutex;
std::unordered_set<std::string> g_warned_sites;

/* True the first time this file:line is seen, false after (so the
   caller warns once). Never throws: a std::bad_alloc from the set is
   swallowed and treated as "not yet warned", which just means that one
   site may warn more than once under memory pressure. */
bool first_time_at(const char *file, int line)
{
    char key[512];
    std::snprintf(key, sizeof key, "%s:%d", file ? file : "?", line);
    try {
        std::lock_guard<std::mutex> lock(g_warned_mutex);
        return g_warned_sites.insert(key).second;
    } catch (...) {
        return true;
    }
}

void warn_once(const char *file, int line, const char *fmt, ...)
{
    if (!first_time_at(file, line)) return;
    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    xpp_log(XPP_LOG_WARN, "%s:%d: %s\n", file ? file : "?", line, msg);
}

/* strlen(s), but never looks past s[maxlen-1] (s need not be
   NUL-terminated within maxlen bytes). Avoids strnlen, which is POSIX,
   not C99/MinGW-portable across every target this file builds on. */
size_t bounded_len(const char *s, size_t maxlen)
{
    size_t n = 0;
    while (n < maxlen && s[n] != '\0') n++;
    return n;
}

} // namespace

int xpp_snprintf_at(char *dst, size_t size, const char *file, int line,
                     const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int want = std::vsnprintf(dst, size, fmt, ap);
    va_end(ap);

    if (want < 0) {
        /* an encoding error, not a size problem: vsnprintf may still
           leave dst without a NUL, so terminate it ourselves. */
        if (size > 0) dst[0] = '\0';
        warn_once(file, line, "xpp_snprintf: formatting error (fmt \"%s\")",
                   fmt ? fmt : "?");
        return want;
    }
    if (size > 0 && (size_t)want >= size) {
        warn_once(file, line,
                   "xpp_snprintf: wanted %d bytes, buffer is %zu: truncated",
                   want, size);
    }
    return want;
}

size_t xpp_strlcpy_at(char *dst, const char *src, size_t size,
                       const char *file, int line)
{
    size_t srclen = std::strlen(src);
    if (size > 0) {
        size_t n = srclen < size - 1 ? srclen : size - 1;
        if (n > 0) std::memcpy(dst, src, n);
        dst[n] = '\0';
    }
    if (srclen >= size) {
        warn_once(file, line,
                   "xpp_strlcpy: wanted %zu bytes, buffer is %zu: truncated",
                   srclen, size);
    }
    return srclen;
}

size_t xpp_strlcat_at(char *dst, const char *src, size_t size,
                       const char *file, int line)
{
    size_t dstlen = bounded_len(dst, size);
    size_t srclen = std::strlen(src);

    if (dstlen >= size) {
        /* dst was not NUL-terminated within size: nothing safe to
           append. Report and leave dst untouched, like BSD strlcat. */
        warn_once(file, line,
                   "xpp_strlcat: destination not NUL-terminated within "
                   "%zu bytes", size);
        return size + srclen;
    }
    size_t avail = size - dstlen - 1;
    size_t n = srclen < avail ? srclen : avail;
    if (n > 0) std::memcpy(dst + dstlen, src, n);
    dst[dstlen + n] = '\0';
    if (srclen > avail) {
        warn_once(file, line,
                   "xpp_strlcat: wanted %zu bytes, buffer is %zu: truncated",
                   dstlen + srclen, size);
    }
    return dstlen + srclen;
}
