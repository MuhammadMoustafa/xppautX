/* xpp_cpu.h's physical_cores on Windows: the processor topology, of the processors this process may run on (its
   affinity: start /affinity, a job object; W819, as Linux's since W817). */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "xpp_cpu_system.h"

#include <cstddef>
#include <vector>

namespace xpp::cpu {

namespace {
/* The processors this process may run on, as Windows keeps them: one group and its mask, or every processor (a
   process spread over several groups, or an affinity that cannot be read, is not restricted). */
struct Affinity {
    bool restricted = false;
    WORD group = 0;
    KAFFINITY mask = 0;
};

Affinity process_affinity()
{
    USHORT groups[1] = {};
    USHORT count = 1;
    DWORD_PTR process = 0, system = 0;
    if (!GetProcessGroupAffinity(GetCurrentProcess(), &count, groups) || count != 1
        || !GetProcessAffinityMask(GetCurrentProcess(), &process, &system) || process == 0)
        return {};
    return {process != system, groups[0], static_cast<KAFFINITY>(process)};
}

/* whether core may run this process: one of its logical processors is in the affinity */
bool may_run(const PROCESSOR_RELATIONSHIP &core, const Affinity &affinity)
{
    if (!affinity.restricted) return true;
    for (WORD i = 0; i < core.GroupCount && i < ANYSIZE_ARRAY; ++i)
        if (core.GroupMask[i].Group == affinity.group && (core.GroupMask[i].Mask & affinity.mask) != 0) return true;
    return false;
}
} // namespace

std::optional<unsigned> physical_cores()
{
    const Affinity affinity = process_affinity();
    constexpr DWORD max_topology_bytes = 16 * 1024 * 1024; // Bound the OS topology buffer.
    DWORD bytes = 0;
    if (GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &bytes)
        || GetLastError() != ERROR_INSUFFICIENT_BUFFER || bytes == 0 || bytes > max_topology_bytes)
        return std::nullopt;
    // ULONGLONG storage supplies the alignment required by the variable-sized records.
    std::vector<ULONGLONG> buffer((bytes + sizeof(ULONGLONG) - 1) / sizeof(ULONGLONG));
    auto *records = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(buffer.data());
    if (!GetLogicalProcessorInformationEx(RelationProcessorCore, records, &bytes)) return std::nullopt;
    unsigned count = 0;
    BYTE highest = 0;
    for (size_t offset = 0; offset < bytes;) {
        constexpr size_t header = offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, Processor);
        if (bytes - offset < header) return std::nullopt;
        const auto *record = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(
            reinterpret_cast<const unsigned char *>(buffer.data()) + offset);
        if (record->Relationship != RelationProcessorCore || record->Size < header + sizeof(PROCESSOR_RELATIONSHIP)
            || record->Size > bytes - offset || record->Size % alignof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX) != 0)
            return std::nullopt;
        offset += record->Size;
        // A process kept to efficiency cores counts those: the highest class is that of the cores it may run on.
        if (!may_run(record->Processor, affinity)) continue;
        const BYTE efficiency = record->Processor.EfficiencyClass;
        if (efficiency > highest) { highest = efficiency; count = 0; }
        if (efficiency == highest) ++count;
    }
    if (count == 0) return std::nullopt;
    return count;
}

} // namespace xpp::cpu
