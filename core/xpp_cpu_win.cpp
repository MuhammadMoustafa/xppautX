/* xpp_cpu.h's physical_cores on Windows: the processor topology. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "xpp_cpu.h"

#include <cstddef>
#include <vector>

namespace xpp::cpu {

std::optional<unsigned> physical_cores()
{
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
        const BYTE efficiency = record->Processor.EfficiencyClass;
        if (efficiency > highest) { highest = efficiency; count = 0; }
        if (efficiency == highest) ++count;
        offset += record->Size;
    }
    if (count == 0) return std::nullopt;
    return count;
}

} // namespace xpp::cpu
