#pragma once
/* Starting another program, the one module for it (W622): the system's opener for a link, the installer of a downloaded
   release, the new xppautX after an update. Never a shell: the program and every argument are separate strings handed
   to the system as they are (posix_spawn, CreateProcess with each argument quoted by the C runtime's rules), so no
   character of an argument can start anything else. The program is a path or a name for the system to find; a caller
   that must not be tricked by the search path gives a full path. The child's standard streams are the null device.
   C++ only. */
#include <span>
#include <string>

#include "xpp_error.h"

namespace xpp::process {

/* whether run waits for the program to end, and is a success only when it ends with status 0 */
enum class Wait { until_it_ends, not_at_all };

/* an error says what could not be started or what status it ended with; place is the file or command the user was working on */
Result<> spawn(std::span<const std::string> argv, Wait wait);

} // namespace xpp::process
