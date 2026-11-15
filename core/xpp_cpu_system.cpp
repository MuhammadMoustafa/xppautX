#include "xpp_cpu_system.h"

namespace xpp::cpu {

unsigned physical_threads()
{
    // Topology is process-wide and settings events ask for this repeatedly.
    static const unsigned count = [] {
        const auto physical = physical_cores();
        // Logical processors are used only when there is no usable physical topology.
        return physical ? *physical : logical_processors();
    }();
    return count;
}

}
