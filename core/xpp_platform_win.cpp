/* xpp_platform.h for Windows: the console, pipe or file of xpp_win32.cpp, and nothing
   Finder-like to adopt from a launch. */
#include "xpp_platform.h"
#include "xpp_win32.h"

namespace xpp::platform {

long read_stdin(char *buf, std::size_t n) { return xpp::win32::read_stdin({buf, n}); }

std::optional<std::string> read_stdin_line(int seconds, std::size_t cap) { return xpp::win32::read_stdin_line(seconds, cap); }

void binary_mode(int fd) { xpp::win32::binary_mode(fd); /* "\n" line ends, not "\r\n" */ }

void attach_console() { xpp::win32::attach_console(); }

bool ignored_launch_argument(std::string_view) { return false; }

bool adopt_launch_document(std::vector<std::string> &) { return false; }

} // namespace xpp::platform
