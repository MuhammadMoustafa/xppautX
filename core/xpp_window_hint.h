#ifndef XPP_WINDOW_HINT_H
#define XPP_WINDOW_HINT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* What xppautX says when its window library (Linux, W13e:
   xpp_window_loader.cpp) does not load, before it falls back to the
   browser: from dl_error, dlerror()'s text, and os_release, the text of
   /etc/os-release (NULL or "" when there is none). A missing library
   ("libwebkit2gtk-4.1.so.0: cannot open shared object file") becomes the
   command that installs WebKitGTK on that system -- apt, dnf, pacman or
   zypper by its ID and ID_LIKE, else the library's name -- and anything
   else is quoted as it is. Written into out (size bytes, cut to fit),
   one line ending in '\n'. */
void xpp_window_load_message(char *out, size_t size, const char *os_release, const char *dl_error);

#ifdef __cplusplus
}

#include <string>
#include <string_view>
namespace xpp {
/* xpp_window_load_message's line, whole, for C++ callers (it may throw
   std::bad_alloc) */
std::string window_load_message(std::string_view os_release, std::string_view dl_error);
} // namespace xpp
#endif
#endif
