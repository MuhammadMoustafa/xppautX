#pragma once
#include <optional>
#include <string_view>
#include <vector>

namespace xpp::cpu {
// Physical cores, restricted to performance cores when the OS identifies them.
std::optional<unsigned> physical_cores();
// Bounds protect sysfs parsing from oversized ids, ranges and lines.
inline constexpr unsigned max_cpu_ids = 65536; // More than current OS CPU limits.
inline constexpr size_t max_sysfs_line = max_cpu_ids * 6; // A decimal id and comma per CPU.
std::optional<unsigned> topology_id(std::string_view text);
std::optional<std::vector<unsigned>> cpu_list(std::string_view text);
}
