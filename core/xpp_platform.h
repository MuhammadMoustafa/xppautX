/* The small things that differ by platform and belong to no module of their own:
   how stdin is read, a stream's line-end translation, the console of a GUI-subsystem
   program, and the word macOS adds to a launch. One file per platform
   implements them, the build picking it: xpp_platform_win.cpp (Windows),
   xpp_platform_posix.cpp (the POSIX half Linux and macOS share), and
   xpp_platform_mac.cpp for the launch word. C++ only. */
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace xpp::platform {

/* up to n bytes of stdin, blocking; <= 0 at end of input or on an error */
long read_input(char *buf, std::size_t n);

/* One bounded line (at most cap bytes) of stdin, ending with a newline, a carriage return dropped,
   with a whole-line deadline of seconds; nullopt at the deadline, at the end of input or on an error */
std::optional<std::string> read_input_line(int seconds, std::size_t cap);

/* no \r\n translation on the descriptor (Windows; nothing elsewhere) */
void set_binary_mode(int fd);

/* xppautX is a GUI-subsystem program on Windows (no console from Explorer): reattach to the
   parent's console, if it has one, before any output of a command-line mode; nothing elsewhere */
void attach_parent_console();

/* a command-line word the system adds that is not an option (macOS: -psn_..., the process
   serial number an older macOS gave an app Finder started); false elsewhere */
#ifdef __APPLE__
bool ignored_launch_argument(std::string_view word);
#else
inline bool ignored_launch_argument(std::string_view) { return false; }
#endif

} // namespace xpp::platform
