#pragma once
/* Starting another program, the one module for it (W622): the system's opener for a link, the installer of a downloaded
   release, the new xppautX after an update. Never a shell: the program and every argument are separate strings handed
   to the system as they are (posix_spawn, CreateProcess with each argument quoted by the C runtime's rules), so no
   character of an argument can start anything else. The program is a path or a name for the system to find; a caller
   that must not be tricked by the search path gives a full path. The child's standard streams are the null device.
   C++ only. */
#include <span>
#include <string>
#include <string_view>

#include "xpp_error.h"

namespace xpp::process {

/* whether run waits for the program to end, and is a success only when it ends with status 0 */
enum class Wait { until_it_ends, not_at_all };

/* how Windows' command line is written: every program reads it with the C runtime's rules (runtime) except Windows Installer's
   msiexec, whose own parser refuses an option in quotes ("/i" ends it with 1639) and reads a property as NAME=value, the value in
   quotes when it holds a blank, each quote inside doubled (installer). Posix passes arguments as they are, whatever this says. */
enum class Quoting { runtime, installer };

/* one argument as the command line holds it: the C runtime's rule (CommandLineToArgvW: bare when it holds no blank or quote, else
   in quotes, a quote preceded by a backslash, the backslashes before a quote or the closing quote doubled); with installer, an
   argument of the form NAME=value is written by the installer's rule above */
std::string quote_argument(std::string_view arg, Quoting quoting = Quoting::runtime);

/* an error says what could not be started or what status it ended with; place is the file or command the user was working on */
Result<> spawn(std::span<const std::string> argv, Wait wait, Quoting quoting = Quoting::runtime);

} // namespace xpp::process
