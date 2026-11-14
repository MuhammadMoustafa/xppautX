/* xpp_cpu.h's physical_cores on macOS: sysctl's physical core counts. */
#include "xpp_cpu.h"
#include <cstddef>
#include <sys/sysctl.h>

namespace xpp::cpu {

std::optional<unsigned> physical_cores()
{
    for (const char *name : {"hw.perflevel0.physicalcpu", "hw.physicalcpu"}) {
        int count = 0;
        size_t size = sizeof(count);
        if (sysctlbyname(name, &count, &size, nullptr, 0) == 0 && size == sizeof(count) && count > 0)
            return static_cast<unsigned>(count);
    }
    return std::nullopt;
}

}
