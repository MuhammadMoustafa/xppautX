/* The core's allocator (xpp_mem.h): the C library's, made loud on failure
   and counted.

   C++ with a C API; nothing here throws (no operator new, and the
   function-local statics are trivially constructed or built from getenv). */
#include "xpp_mem.h"
#include "xpp_log.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(__APPLE__)
#include <malloc/malloc.h>
#define USABLE_SIZE(p) malloc_size(p)
#elif defined(_WIN32)
#include <malloc.h>
#define USABLE_SIZE(p) _msize(p)
#elif defined(__GLIBC__)
#include <malloc.h>
#define USABLE_SIZE(p) malloc_usable_size(p)
#endif

namespace {

std::atomic<unsigned long long> n_calls{0}; /* every allocating call */
std::atomic<unsigned long long> n_allocs{0};
std::atomic<unsigned long long> n_reallocs{0};
std::atomic<unsigned long long> n_frees{0};
std::atomic<unsigned long long> n_bytes{0};
std::atomic<long long> n_live{0};

constexpr auto relaxed = std::memory_order_relaxed;

long long usable(void *p)
{
#ifdef USABLE_SIZE
    return p != nullptr ? static_cast<long long>(USABLE_SIZE(p)) : 0;
#else
    (void)p;
    return 0;
#endif
}

/* XPP_MEM_FAIL_AT=N: the N-th allocating call fails (0: none) */
unsigned long long fail_at()
{
    static const unsigned long long n = [] {
        const char *s = std::getenv("XPP_MEM_FAIL_AT");
        return s != nullptr ? std::strtoull(s, nullptr, 10) : 0ULL;
    }();
    return n;
}

/* XPP_MEM_INIT=0: memory from xpp_malloc/xpp_realloc is left as the C
   library gives it, for valgrind (tools/valgrindcheck.sh) to see a read of
   what was never written; otherwise it is zeroed */
bool zero_init()
{
    static const bool z = [] {
        const char *s = std::getenv("XPP_MEM_INIT");
        return s == nullptr || std::strcmp(s, "0") != 0;
    }();
    return z;
}

/* p, which was given *size bytes, grown to all the C library holds for it
   (its usable size; realloc to that size stays in place), and *size set to
   that: every byte of it is then the program's to zero. Zeroing only what
   was asked for would leave the rest to a later xpp_realloc growing in
   place, and writing past what was asked for is undefined (glibc's
   _FORTIFY_SOURCE aborts on it). */
void *whole(void *p, size_t *size)
{
#ifdef USABLE_SIZE
    size_t u = USABLE_SIZE(p);
    if (u > *size) {
        void *q = std::realloc(p, u);
        if (q != nullptr) {
            p = q;
            *size = u;
        }
    }
#endif
    return p;
}

/* zeroes bytes from..size of p, grown first to its usable size */
void *zeroed(void *p, size_t from, size_t size)
{
    p = whole(p, &size);
    if (size > from) std::memset(static_cast<char *>(p) + from, 0, size - from);
    return p;
}

/* counts the call and says whether the test hook fails it */
bool injected_failure(size_t n)
{
    n_bytes.fetch_add(n, relaxed);
    unsigned long long seq = n_calls.fetch_add(1, relaxed) + 1;
    return seq == fail_at();
}

/* Messages are formatted here and logged with "%s": xpp_log.c is C, and
   MinGW's C printf may not know %zu or %llu, where C++'s snprintf does. */
[[noreturn]] void die(const char *what, size_t n, size_t size, const char *file, int line)
{
    char msg[256];
    const char *why = fail_at() != 0 ? " (XPP_MEM_FAIL_AT)" : "";
    if (size != 0)
        std::snprintf(msg, sizeof msg, "out of memory: %s(%zu x %zu bytes) at %s:%d%s\n", what, n,
                      size, file, line, why);
    else
        std::snprintf(msg, sizeof msg, "out of memory: %s(%zu bytes) at %s:%d%s\n", what, n, file,
                      line, why);
    xpp_log(XPP_LOG_ERROR, "%s", msg);
    std::exit(1);
}

void *got(void *p)
{
    n_live.fetch_add(usable(p), relaxed);
    return p;
}

/* the counts at exit, at DEBUG; a static object's destructor runs after the
   atexit handlers registered once main() has begun */
struct ExitReport {
    ~ExitReport()
    {
        XppMemStats s = xpp_mem_stats();
        char msg[256];
        std::snprintf(msg, sizeof msg,
                      "memory: %llu allocations, %llu reallocations, %llu frees, "
                      "%llu bytes requested, %lld bytes still held\n",
                      s.allocs, s.reallocs, s.frees, s.bytes, s.live_bytes);
        xpp_log(XPP_LOG_DEBUG, "%s", msg);
    }
};
ExitReport exit_report;

} // namespace

void *xpp_malloc_at(size_t n, const char *file, int line)
{
    if (injected_failure(n)) die("malloc", n, 0, file, line);
    void *p = std::malloc(n != 0 ? n : 1);
    if (p == nullptr) die("malloc", n, 0, file, line);
    if (zero_init()) p = zeroed(p, 0, n != 0 ? n : 1);
    n_allocs.fetch_add(1, relaxed);
    return got(p);
}

void *xpp_calloc_at(size_t n, size_t size, const char *file, int line)
{
    if (size != 0 && n > SIZE_MAX / size) die("calloc (overflow)", n, size, file, line);
    if (injected_failure(n * size)) die("calloc", n, size, file, line);
    void *p = n != 0 && size != 0 ? std::calloc(n, size) : std::calloc(1, 1);
    if (p == nullptr) die("calloc", n, size, file, line);
    if (zero_init()) p = zeroed(p, n * size, n != 0 && size != 0 ? n * size : 1);
    n_allocs.fetch_add(1, relaxed);
    return got(p);
}

void *xpp_realloc_at(void *p, size_t n, const char *file, int line)
{
    if (injected_failure(n)) die("realloc", n, 0, file, line);
    long long before = usable(p);
    void *q = std::realloc(p, n != 0 ? n : 1);
    if (q == nullptr) die("realloc", n, 0, file, line);
    /* the old block was zeroed to its usable size, so what grows is new */
    if (zero_init()) q = zeroed(q, static_cast<size_t>(before), n != 0 ? n : 1);
    (p != nullptr ? n_reallocs : n_allocs).fetch_add(1, relaxed);
    n_live.fetch_sub(before, relaxed);
    return got(q);
}

char *xpp_strdup_at(const char *s, const char *file, int line)
{
    if (s == nullptr) return nullptr;
    size_t n = std::strlen(s) + 1;
    char *c = static_cast<char *>(xpp_malloc_at(n, file, line));
    std::memcpy(c, s, n);
    return c;
}

void xpp_free(void *p)
{
    if (p == nullptr) return;
    n_live.fetch_sub(usable(p), relaxed);
    n_frees.fetch_add(1, relaxed);
    std::free(p);
}

XppMemStats xpp_mem_stats(void)
{
    XppMemStats s;
    s.allocs = n_allocs.load(relaxed);
    s.reallocs = n_reallocs.load(relaxed);
    s.frees = n_frees.load(relaxed);
    s.bytes = n_bytes.load(relaxed);
    s.live_bytes = n_live.load(relaxed);
    return s;
}
