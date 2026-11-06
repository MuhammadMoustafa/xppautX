#include "xpptest.h"
#include "sweep_runs.h"
#include "xpp_math.h"

#include <atomic>
#include <cstddef>
#include <vector>

/* the seeds a sweep gives its runs, and the ordered executor that computes the runs on threads (W434): what the
   sweep's results depend on is the order the runs are taken in and the seed each has, never the thread count */
int main()
{
    const std::vector<int> seeds = xpp::seed_stream(7, 5);
    CHECK(seeds.size() == 5 && seeds[0] == 7);
    for (std::size_t k = 0; k + 1 < seeds.size(); ++k) CHECK(seeds[k + 1] == xpp::next_seed(seeds[k]));
    CHECK(xpp::seed_stream(7, 3) == std::vector<int>(seeds.begin(), seeds.begin() + 3));
    CHECK(xpp::seed_stream(7, 0).empty());

    constexpr std::size_t COUNT = 200;
    for (const unsigned threads : {1u, 2u, 4u, 7u}) {
        std::vector<long> result(COUNT, -1); /* each item written once, by the thread that computes it */
        std::vector<std::size_t> order;
        std::atomic<std::size_t> started{0}, taken{0}, most_ahead{0};
        const auto ran = xpp::run_ordered(
            COUNT, threads,
            [&](unsigned, std::size_t item) {
                const std::size_t ahead = ++started - taken;
                for (std::size_t seen = most_ahead; ahead > seen && !most_ahead.compare_exchange_weak(seen, ahead);) {}
                result[item] = static_cast<long>(item * item);
            },
            [&](std::size_t item) {
                order.push_back(item);
                CHECK(result[item] == static_cast<long>(item * item)); /* taken in after it was computed */
                ++taken;
                return true;
            });
        CHECK(ran && !*ran);
        bool in_order = order.size() == COUNT;
        for (std::size_t k = 0; in_order && k < COUNT; ++k) in_order = order[k] == k;
        CHECK(in_order);
        CHECK(most_ahead <= xpp::sweep_window(threads) + threads); /* the window, and the items in a thread's hands */
    }

    /* consume gives false: nothing is started after what is already ahead, and the ones before were taken in order */
    {
        constexpr unsigned threads = 4;
        std::atomic<std::size_t> started{0};
        std::vector<std::size_t> order;
        const auto ran = xpp::run_ordered(
            COUNT, threads, [&](unsigned, std::size_t) { ++started; },
            [&](std::size_t item) {
                order.push_back(item);
                return item < 10;
            });
        CHECK(ran && !*ran);
        CHECK(order.size() == 11 && order.back() == 10);
        CHECK(started <= 11 + xpp::sweep_window(threads) + threads);
    }

    CHECK(xpp::run_ordered(0, 3, [](unsigned, std::size_t) {}, [](std::size_t) { return true; }));
    TEST_REPORT("sweep runs");
}
