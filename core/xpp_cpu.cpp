#include "xpp_cpu.h"
#include "xpp_text.h"
#include <thread>
#include <string>
#include <algorithm>

/* physical_cores is once per platform: xpp_cpu_win.cpp, xpp_cpu_linux.cpp, xpp_cpu_mac.cpp */

namespace xpp::cpu {
unsigned logical_processors() { return std::max(1u, std::thread::hardware_concurrency()); }

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
