#ifndef XPP_IO_H
#define XPP_IO_H

/* core/xpp_io.cpp: the one place the core formats into and copies text
   into fixed buffers (issue: W11 step 2). sprintf and strcpy write with
   no length, so an oversized name or expression silently overflows the
   buffer; xpp_snprintf/xpp_strlcpy/xpp_strlcat never write past the
   destination's size, always NUL-terminate (when size > 0) and log a
   WARN (core/xpp_log.h), once per call site, when what they wanted to
   write did not fit. Behaviour is otherwise identical to sprintf/strcpy,
   so converting a call site that was never going to overflow changes
   nothing.

   xpp_snprintf/xpp_strlcpy/xpp_strlcat take the destination's size
   explicitly, like the C library's snprintf/strlcpy/strlcat (the file
   and line are threaded through automatically, xpp_mem.h-style, by the
   macro wrapper below, for the warning). XPP_SPRINTF(dst, fmt, ...) and
   XPP_STRCPY(dst, src) additionally take the size from sizeof(dst): dst
   must be an array (sizeof sees the whole array through a struct member
   or an indexed element too, e.g. XPP_SPRINTF(s->name, ...) or
   XPP_SPRINTF(arr[i].name, ...)); a pointer destination -- a parameter,
   a strcpy return chained through, a computed offset like buf+n -- fails
   XPP_ARRAY_SIZE_CHECK to *compile* (a negative array size), instead of
   silently taking sizeof(pointer) as the size. Find that call's real
   destination size and call xpp_snprintf/xpp_strlcpy directly with it.

   xpp_snprintf's return is the length it would have written, like
   snprintf's (not -1 on truncation): a caller that compares it against
   the buffer size to detect truncation still works. xpp_strlcpy/
   xpp_strlcat, and XPP_SPRINTF/XPP_STRCPY/XPP_STRCAT, return the BSD
   strlcpy/strlcat convention (the length of the source, or dst+src) --
   not dst, unlike strcpy/strcat's own return: no call site in core used
   that return value (checked with grep before dropping it), and keeping
   it would make every statement-context use (`XPP_STRCPY(buf, s);`, the
   overwhelming majority) warn -Wunused-value on the discarded bare `dst`
   at the end of the comma expression. */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int xpp_snprintf_at(char *dst, size_t size, const char *file, int line,
                     const char *fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 5, 6)))
#endif
    ;
size_t xpp_strlcpy_at(char *dst, const char *src, size_t size,
                       const char *file, int line);
size_t xpp_strlcat_at(char *dst, const char *src, size_t size,
                       const char *file, int line);

#ifdef __cplusplus
}
#endif

#define xpp_snprintf(dst, size, ...) \
    xpp_snprintf_at((dst), (size), __FILE__, __LINE__, __VA_ARGS__)
#define xpp_strlcpy(dst, src, size) \
    xpp_strlcpy_at((dst), (src), (size), __FILE__, __LINE__)
#define xpp_strlcat(dst, src, size) \
    xpp_strlcat_at((dst), (src), (size), __FILE__, __LINE__)

/* sizeof(char[N]) with a non-positive N is a compile error: catches the
   pointer case (sizeof(dst) == sizeof(char *)) without typeof /
   __builtin_types_compatible_p, which -pedantic (CFLAGS) would warn
   about as a GNU extension at every call site. A destination array that
   is genuinely exactly sizeof(char*) bytes (8 on a 64-bit build) false-
   triggers this; call xpp_snprintf/xpp_strlcpy directly there instead. */
#define XPP_ARRAY_SIZE_CHECK(dst) \
    ((void)sizeof(char[1 - 2 * (sizeof(dst) == sizeof(char *))]))

#define XPP_SPRINTF(dst, ...) \
    (XPP_ARRAY_SIZE_CHECK(dst), \
     xpp_snprintf_at((dst), sizeof(dst), __FILE__, __LINE__, __VA_ARGS__))
#define XPP_STRCPY(dst, src) \
    (XPP_ARRAY_SIZE_CHECK(dst), \
     xpp_strlcpy_at((dst), (src), sizeof(dst), __FILE__, __LINE__))
#define XPP_STRCAT(dst, src) \
    (XPP_ARRAY_SIZE_CHECK(dst), \
     xpp_strlcat_at((dst), (src), sizeof(dst), __FILE__, __LINE__))

#ifdef __cplusplus
/* C++ callers get a type-checked API instead of the C wrappers above
   (their printf-style format strings and vararg passing are for C
   callers): xpp::format/xpp::format_to_buf take a std::format_string,
   checked against the argument types at compile time, no printf %-verb
   ever mismatching an argument. xpp::number is a double's shortest
   round-trip text (std::to_chars), for when "%g" would do but a
   guaranteed-reversible digit string is wanted. This part of the header
   is skipped entirely by a C file, so xpp_snprintf/XPP_SPRINTF and
   friends keep working there unchanged; C++ files in the sweep (not
   ui_json.cpp/xpp_http.cpp, converted separately by T18) use this API
   for a real fixed-array destination and a literal/simple format, and
   still use xpp_snprintf/xpp_strlcpy directly for a pointer destination
   (format_to_buf, like XPP_SPRINTF, needs an actual array: see
   XPP_ARRAY_SIZE_CHECK above) or where the original format string used
   dynamic width/precision (%*s, %.*s) that would need re-expressing in
   std::format's syntax -- not a mechanical, behaviour-preserving change,
   so those stay on the C wrappers (documented at each such call site). */
#include <cstddef>
#include <string>
#include <utility>

#if defined(__cpp_lib_format) || (defined(__has_include) && __has_include(<format>))
#include <format>
#define XPP_IO_HAVE_STD_FORMAT 1
#endif
#if defined(__cpp_lib_to_chars) || (defined(__has_include) && __has_include(<charconv>))
#include <charconv>
#define XPP_IO_HAVE_TO_CHARS 1
#endif

namespace xpp {

/* std::format can throw (bad_alloc); no exception may cross into the C
   code that calls these C++ functions (CLAUDE.md), so a failure is loud
   and final like xpp_mem's: an ERROR naming the call site, exit 1 */
[[noreturn]] void format_failed(const char *file, int line) noexcept;

#ifdef XPP_IO_HAVE_STD_FORMAT
/* Compile-time checked formatting: a bad "{}" against the argument
   types is a compile error, not a WARN at run time. No length limit
   (returns a std::string); for a fixed buffer use format_to_buf. */
template <class... Args>
std::string format(std::format_string<Args...> fmt, Args &&...args) noexcept
{
    try {
        return std::format(fmt, std::forward<Args>(args)...);
    } catch (...) {
        format_failed(__FILE__, __LINE__);
    }
}

/* The array-destination counterpart of XPP_SPRINTF: dst must be a real
   array (a template on its size, not a decayed pointer -- a pointer
   destination simply does not match this overload and fails to
   compile), formats with std::format, then copies in with
   xpp_strlcpy_at so a result that does not fit is cut and warns once,
   exactly like every other truncation in this module. file/line come
   from the XPP_FORMAT_TO_BUF macro wrapper (xpp_mem.h/XPP_SPRINTF
   style), not std::source_location, to match the rest of the module
   and because a source_location default argument cannot follow the
   variadic Args&&...args this shares with std::format's own signature. */
template <std::size_t N, class... Args>
void format_to_buf(char (&buf)[N], const char *file, int line,
                    std::format_string<Args...> fmt, Args &&...args) noexcept
{
    try {
        std::string s = std::format(fmt, std::forward<Args>(args)...);
        xpp_strlcpy_at(buf, s.c_str(), N, file, line);
    } catch (...) {
        format_failed(file, line);
    }
}
#endif

#ifdef XPP_IO_HAVE_TO_CHARS
/* A double's shortest round-trip decimal text (std::to_chars): unlike
   "%g" it never loses precision and never needs a chosen digit count.
   xpp_snprintf(..., "%.17g", ...) papers over the same problem with
   more digits than usually needed; prefer this in new C++ code. */
inline std::string number(double v)
{
    char buf[32];
    std::to_chars_result r = std::to_chars(buf, buf + sizeof buf, v);
    if (r.ec == std::errc())
        return std::string(buf, r.ptr);
    return std::to_string(v); /* unreachable for a finite double in 32 bytes */
}
#endif

} // namespace xpp

#ifdef XPP_IO_HAVE_STD_FORMAT
#define XPP_FORMAT_TO_BUF(buf, ...) \
    xpp::format_to_buf((buf), __FILE__, __LINE__, __VA_ARGS__)
#endif

#endif /* __cplusplus */

#endif
