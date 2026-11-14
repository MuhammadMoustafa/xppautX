#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <stddef.h>

namespace xpp::cpu {
// Logical processors the system reports, at least 1 (hardware_concurrency may report 0).
unsigned logical_processors();
// Physical cores, restricted to performance cores when the OS identifies them.
std::optional<unsigned> physical_cores();
// The physical cores (the logical processors when the topology is unknown): what a run of jobs or of a sweep shares.
unsigned physical_threads();
// Bounds on what a sysfs CPU list may name, so a malformed one is refused, not expanded.
inline constexpr unsigned max_cpu_ids = 65536; // More than current OS CPU limits.
inline constexpr size_t max_sysfs_line = max_cpu_ids * 6; // A decimal id and comma per CPU.
std::optional<unsigned> topology_id(std::string_view text);
std::optional<std::vector<unsigned>> cpu_list(std::string_view text);

/* the thread count of autoX and sweeps (W267, W434): the file says "auto" or a whole number from 1
   to the logical processors (logical_processors); THREADS_AUTO
   is "auto", the count AutoxParallel::automatic_threads
   gives. Any count gives the same results (W247). */
inline constexpr unsigned THREADS_AUTO = 0;
inline constexpr std::string_view THREADS_AUTO_WORD = "auto";
/* why n cannot be the thread count, or nothing; the one check of the file's
   reader and writer and of the protocol's `set` */
std::optional<std::string> threads_problem(long long n);
}
