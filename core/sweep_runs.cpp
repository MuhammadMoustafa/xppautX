#include "expr_tables.h"
#include "sweep_runs.h"
#include "analysis_parts.h"
#include "ani_state.h"
#include "expr.h"
#include "integrate.h"
#include "my_rhs.h"
#include "solver.h"
#include "storage.h"
#include "xpp_text.h"
#include "xpp_job.h"
#include "xpp_log.h"
#include "xpp_ui.h"
#include "model.h"
#include "tabular.h"
#include "xpp_math.h"
#include "xpp_mem.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <system_error>
#include <thread>
#include <vector>
#include <new>
#include <optional>

namespace xpp {

std::string sweep_serial_reason(const AnalysisParts &s)
{
    if (!s.model().rhs_impurity.empty()) return "the model's right-hand side is not pure: " + s.model().rhs_impurity;
    if (s.integrator->rhs.function != my_rhs) return "the right-hand side is not the model's own";
    if (s.integrator->steady) return "an unchanged-digits run is on";
    if (s.animation->options.on_the_fly) return "the animation draws as the runs go (on_the_fly)";
    if (job::stop_armed()) return "a replayed recording stops this sweep at a row, which runs side by side cannot reach";
    if (recording_in_progress()) return "a recording is being made, and a Stop is replayed at a row, which runs side by side cannot reach";
    return {};
}

void say_sweep_serial(const AnalysisParts &s, std::string_view sweep, std::int64_t runs, std::string_view why)
{
    if (runs > 1 && s.thread_budget() > 1)
        log(XPP_LOG_INFO, "{}: the {} runs go one after another, not on {} threads: {}\n", sweep, runs, s.thread_budget(), why);
}

Result<bool> run_ordered(std::size_t count, unsigned threads, const std::function<void(unsigned, std::size_t)> &compute,
                         const std::function<bool(std::size_t)> &consume)
{
    bool ended_by_key = false;
    const std::size_t window = sweep_window(threads);
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<char> done(window, 0);
    std::size_t next = 0, taken = 0; /* the next item to start; the items the caller has taken in */
    bool stopping = false;

    const auto work = [&](unsigned worker) {
        for (;;) {
            std::size_t item;
            {
                std::unique_lock lock(mutex);
                changed.wait(lock, [&] { return stopping || next >= count || next < taken + window || job::cancelled(); });
                if (stopping || next >= count || job::cancelled()) return;
                item = next++;
            }
            compute(worker, item);
            {
                const std::lock_guard lock(mutex);
                done[item % window] = 1;
            }
            changed.notify_all();
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(threads);
    const auto stop_all = [&] {
        {
            const std::lock_guard lock(mutex);
            stopping = true;
        }
        changed.notify_all();
        for (std::thread &thread : pool) thread.join();
    };
    for (unsigned worker = 0; worker < threads; ++worker) {
        try {
            pool.emplace_back(work, worker);
        } catch (const std::system_error &e) {
            stop_all();
            return fail("sweep", format("cannot start a thread for the runs: {}", e.what()), command_place());
        }
    }

    for (std::size_t item = 0; item < count; ++item) {
        std::unique_lock lock(mutex);
        /* the item is done, or the cancel came before a thread started it (the ones started finish at once: the
           integration sees the cancel) */
        while (!done[item % window] && !(job::cancelled() && item >= next)) {
            changed.wait_for(lock, std::chrono::duration<double>(INPUT_POLL_SECONDS));
            lock.unlock();
            if (my_abort() == END_SWEEP_KEY) { /* the front end's Abort cancels the job; this key ends the sweep alone */
                lock.lock();
                ended_by_key = true;
                break;
            }
            lock.lock();
            changed.notify_all(); /* a thread waiting to start an item sees the cancel too */
        }
        if (ended_by_key || !done[item % window]) break;
        lock.unlock();
        const bool go = consume(item);
        lock.lock();
        done[item % window] = 0;
        taken = item + 1;
        lock.unlock();
        changed.notify_all();
        if (!go) break;
    }
    stop_all();
    return ended_by_key;
}

} // namespace xpp
