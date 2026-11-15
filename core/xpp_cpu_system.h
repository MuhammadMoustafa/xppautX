#pragma once
#include "xpp_cpu.h"

namespace xpp::cpu {
// Physical cores, restricted to performance cores when the OS identifies them: once per platform
// (xpp_cpu_win.cpp, xpp_cpu_linux.cpp, xpp_cpu_mac.cpp). The program's start measures it and passes it in
// (XppProgram::measure_physical_threads); nothing below the application asks the system.
std::optional<unsigned> physical_cores();
// The physical cores (the logical processors when the topology is unknown): what a run of jobs or of a sweep shares.
// Measured on the first call, process-wide, since settings events ask for it repeatedly.
unsigned physical_threads();
}
