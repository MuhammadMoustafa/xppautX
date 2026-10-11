#include "xpp_cpu.h"
#include "xpp_io.h"
#include "xpp_win32.h"
#include <set>
#include <algorithm>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

namespace xpp::cpu {
std::optional<unsigned> topology_id(std::string_view text)
{
    int id;
    if (!parse_int(trim_blanks(text), id) || id < 0 || static_cast<unsigned>(id) >= max_cpu_ids)
        return std::nullopt;
    return static_cast<unsigned>(id);
}

std::optional<std::vector<unsigned>> cpu_list(std::string_view text)
{
    if (text.size() > max_sysfs_line) return std::nullopt;
    text = trim_blanks(text);
    std::vector<unsigned> cpus;
    while (!text.empty()) {
        const size_t comma = text.find(',');
        const auto part = text.substr(0, comma);
        if (part.find_first_not_of("0123456789-") != std::string_view::npos) return std::nullopt;
        const size_t dash = part.find('-');
        const auto first = topology_id(part.substr(0, dash));
        const auto last = dash == std::string_view::npos ? first : topology_id(part.substr(dash + 1));
        if (!first || !last || *last < *first || (!cpus.empty() && *first <= cpus.back()))
            return std::nullopt;
        for (unsigned id = *first; id <= *last; ++id) cpus.push_back(id);
        if (comma == std::string_view::npos) break;
        text.remove_prefix(comma + 1);
        if (text.empty()) return std::nullopt;
    }
    if (cpus.empty()) return std::nullopt;
    return cpus;
}

#ifdef __linux__
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
#endif

std::optional<unsigned> physical_cores()
{
#ifdef _WIN32
    return win32::processor_core_count();
#elif defined(__linux__)
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
#elif defined(__APPLE__)
    for (const char *name : {"hw.perflevel0.physicalcpu", "hw.physicalcpu"}) {
        int count = 0;
        size_t size = sizeof(count);
        if (sysctlbyname(name, &count, &size, nullptr, 0) == 0 && size == sizeof(count) && count > 0)
            return static_cast<unsigned>(count);
    }
    return std::nullopt;
#else
    return std::nullopt;
#endif
}
}
