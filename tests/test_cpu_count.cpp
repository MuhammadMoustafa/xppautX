#include "xpptest.h"
#include "xpp_cpu.h"
#include "xpp_io.h"
#include "auto_parallel.h"
#include <thread>
#include <algorithm>

int main()
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
    CHECK(xpp::AutoParallel::automatic_threads() == std::clamp(available, 1u, xpp::AutoParallel::max_threads));
    xpp::print(stdout, "CPU count: logical {}, physical/performance {}, AUTO used {}\n",
               logical, physical.value_or(0), xpp::AutoParallel::automatic_threads());
    TEST_REPORT("cpu count");
}
