/* The small things that differ by platform and belong to no module of their own:
   how stdin is read, a stream's line-end translation, the console of a GUI-subsystem
   program, and what macOS's Finder adds to a launch. One file per platform
   implements them, the build picking it: xpp_platform_win.cpp (Windows),
   xpp_platform_posix.cpp (the POSIX half Linux and macOS share), and
   xpp_platform_linux.cpp / xpp_platform_mac.cpp for the launch. C++ only. */
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xpp::platform {

/* up to n bytes of stdin, blocking; <= 0 at end of input or on an error */
long read_stdin(char *buf, std::size_t n);

/* One bounded line (at most cap bytes) of stdin, ending with a newline, a carriage return dropped,
   with a whole-line deadline of seconds; nullopt at the deadline, at the end of input or on an error */
std::optional<std::string> read_stdin_line(int seconds, std::size_t cap);

/* no \r\n translation on the descriptor (Windows; nothing elsewhere) */
void binary_mode(int fd);

/* xppautX is a GUI-subsystem program on Windows (no console from Explorer): reattach to the
   parent's console, if it has one, before any output of a command-line mode; nothing elsewhere */
void attach_console();

/* a command-line word the system adds that is not an option (macOS: -psn_..., the process
   serial number an older macOS gave an app Finder started); false elsewhere */
bool ignored_launch_argument(std::string_view word);

/* macOS: the document Finder or `open` gave xppautX.app when it launched it (an Apple Event, not an
   argument) is the model, loaded from its own folder as File > Open model loads one, and an app Finder
   started (in "/") starts in the home folder, where the start screen's Open model dialog begins:
   args gets the model's name added and the working folder is changed, true when args changed;
   false elsewhere, and when there was no document */
bool adopt_launch_document(std::vector<std::string> &args);

} // namespace xpp::platform
