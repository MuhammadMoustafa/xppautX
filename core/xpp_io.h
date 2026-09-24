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
   xpp_strlcat return the length of the source (or dst+src), the BSD
   strlcpy/strlcat convention. XPP_STRCPY/XPP_STRCAT instead evaluate to
   dst, matching strcpy/strcat's own return, since a few call sites
   chain it (`p = strcpy(buf, s);`). */

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
     xpp_strlcpy_at((dst), (src), sizeof(dst), __FILE__, __LINE__), (dst))
#define XPP_STRCAT(dst, src) \
    (XPP_ARRAY_SIZE_CHECK(dst), \
     xpp_strlcat_at((dst), (src), sizeof(dst), __FILE__, __LINE__), (dst))

#endif
