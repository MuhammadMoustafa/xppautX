#include "xpptest.h"
#include "xpp_cpu_system.h"
#include "xpp_io.h"
#include "autox_parallel.h"
#include <thread>
#ifdef __linux__
#include <sched.h>
#endif
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <algorithm>
#include <stdio.h>
#include <optional>
#include <string>
#include <vector>

TEST_CASE("test_cpu_count")
{
    const auto cpus = xpp::cpu::cpu_list("0-3,8-11\n");
    CHECK(cpus && *cpus == std::vector<unsigned>({0,1,2,3,8,9,10,11}));
    CHECK(xpp::cpu::cpu_list(" 2,4-5 \r\n") == std::optional(std::vector<unsigned>{2,4,5}));
    CHECK(xpp::cpu::cpu_list("0") == std::optional(std::vector<unsigned>{0}));
    for (const char *bad : {"", " ", "0,", ",0", "0,,2", "3-1", "0-", "-1", "1-2-3",
                            "1,1", "2,1", "0-3,2-4", "65536", "0-65536", "999999999999999999",
                            "a", "0 1", "0 - 3", "0,+1"})
        CHECK(!xpp::cpu::cpu_list(bad));
    CHECK(!xpp::cpu::cpu_list(std::string(xpp::cpu::max_sysfs_line + 1, '0')));
    const auto all = xpp::cpu::cpu_list("0-65535");
    CHECK(all && all->size() == xpp::cpu::max_cpu_ids && all->back() == 65535);
    CHECK(xpp::cpu::topology_id(" 12\n") == 12);
    CHECK(xpp::cpu::topology_id("0") == 0);
    for (const char *bad : {"-1", "65536", "", "12x", "1 2", "2147483648"})
        CHECK(!xpp::cpu::topology_id(bad));
    const auto physical = xpp::cpu::physical_cores();
#ifdef _WIN32
    CHECK(physical && *physical >= 1);
#endif
    const unsigned logical = std::thread::hardware_concurrency();
    const unsigned available = physical.value_or(logical);
    CHECK(xpp::AutoxParallel::automatic_threads() == std::clamp(available, 1u, xpp::AutoxParallel::max_threads));
#ifdef __linux__
    /* a process kept to one processor has one core, whatever the machine has (W817: a run on one CPU compiles in its own thread) */
    cpu_set_t kept, one;
    if (sched_getaffinity(0, sizeof(kept), &kept) == 0) {
        CPU_ZERO(&one);
        for (int cpu = 0; cpu < CPU_SETSIZE && CPU_COUNT(&one) == 0; ++cpu)
            if (CPU_ISSET(cpu, &kept)) CPU_SET(cpu, &one);
        if (sched_setaffinity(0, sizeof(one), &one) == 0) {
            const auto pinned = xpp::cpu::physical_cores();
            CHECK(!pinned || *pinned == 1);
            CHECK(sched_setaffinity(0, sizeof(kept), &kept) == 0);
        }
    }
#endif
#ifdef _WIN32
    /* likewise on Windows (W819): a process kept to one processor of its group has one core */
    DWORD_PTR process = 0, system = 0;
    if (GetProcessAffinityMask(GetCurrentProcess(), &process, &system) && process != 0) {
        const DWORD_PTR one = process & (~process + 1); /* its lowest processor */
        if (SetProcessAffinityMask(GetCurrentProcess(), one)) {
            const auto pinned = xpp::cpu::physical_cores();
            CHECK(pinned && *pinned == 1);
            CHECK(SetProcessAffinityMask(GetCurrentProcess(), process));
        }
    }
#endif
    xpp::print(stdout, "CPU count: logical {}, physical/performance {}, autoX used {}\n",
               logical, physical.value_or(0), xpp::AutoxParallel::automatic_threads());
    TEST_REPORT("cpu count");
}
