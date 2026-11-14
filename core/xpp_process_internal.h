/* The platform primitive of xpp_process.cpp: start the program, implemented
   once per platform (xpp_process_posix.cpp: posix_spawn; xpp_process_win.cpp:
   CreateProcess). Internal to the xpp_process*.cpp files. */
#pragma once

#include <span>
#include <string>
#include <string_view>

#include "xpp_process.h"

namespace xpp::process {

inline constexpr std::string_view WHERE = "start a program"; /* what the errors say could not be done */

/* argv is not empty and names a program (spawn checked); the error spawn returns */
Result<> start_program(std::span<const std::string> argv, Wait wait, Quoting quoting);

} // namespace xpp::process
