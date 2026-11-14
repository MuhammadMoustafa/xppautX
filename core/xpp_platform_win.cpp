/* xpp_platform.h for Windows: the console, pipe or file of xpp_win.cpp, and nothing
   Finder-like to adopt from a launch. */
#include "xpp_platform.h"
#include "xpp_win.h"

namespace xpp::platform {

long read_input(char *buf, std::size_t n) { return xpp::win::read_stdin({buf, n}); }

std::optional<std::string> read_input_line(int seconds, std::size_t cap) { return xpp::win::read_stdin_line(seconds, cap); }

void set_binary_mode(int fd) { xpp::win::binary_mode(fd); /* "\n" line ends, not "\r\n" */ }

void attach_parent_console() { xpp::win::attach_console(); }

bool ignored_launch_argument(std::string_view) { return false; }

bool adopt_launch_document(std::vector<std::string> &) { return false; }

} // namespace xpp::platform
