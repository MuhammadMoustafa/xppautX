/* xpp_cpu.h's physical_cores on Linux: the topology sysfs keeps. */
#include "xpp_cpu.h"
#include "xpp_io.h"
#include <set>
#include <algorithm>
#include <string>
#include <utility>

namespace xpp::cpu {

namespace {
// A sysfs attribute is one line of at most a page, the kernel's own bound.
std::optional<std::string> system_line(LineReader reader)
{
    auto line = reader.next();
    if (!line) return std::nullopt;
    std::string value(*line);
    if (reader.next()) return std::nullopt;
    return value;
}
}

std::optional<unsigned> physical_cores()
{
    const auto online_text = system_line(LineReader("/sys/devices/system/cpu/online"));
    if (!online_text) return std::nullopt;
    const auto online = cpu_list(*online_text);
    if (!online) return std::nullopt;
    // An absent Intel hybrid PMU list means the OS does not distinguish core classes.
    LineReader performance_reader("/sys/devices/cpu_core/cpus");
    std::optional<std::vector<unsigned>> performance;
    if (performance_reader) {
        const auto line = system_line(std::move(performance_reader));
        if (!line) return std::nullopt;
        performance = cpu_list(*line);
        if (!performance) return std::nullopt;
    }
    std::set<std::pair<unsigned, unsigned>> cores;
    for (unsigned cpu : *online) {
        if (performance && !std::binary_search(performance->begin(), performance->end(), cpu)) continue;
        const auto base = format("/sys/devices/system/cpu/cpu{}/topology/", cpu);
        const auto package_text = system_line(LineReader(base + "physical_package_id"));
        const auto core_text = system_line(LineReader(base + "core_id"));
        if (!package_text || !core_text) return std::nullopt;
        const auto package = topology_id(*package_text), core = topology_id(*core_text);
        if (!package || !core) return std::nullopt;
        cores.emplace(*package, *core);
    }
    if (cores.empty()) return std::nullopt;
    return static_cast<unsigned>(cores.size());
}

}
