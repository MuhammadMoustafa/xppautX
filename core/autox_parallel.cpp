#include "autox_parallel.h"
#include "session.h"
#include "autox_c.h"
#include "my_rhs.h"
#include "solver.h"
#include "xpp_cpu.h"
#include "xpp_job.h"
#include <algorithm>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <thread>

namespace xpp {
struct AutoxParallel::State {
    std::mutex mutex;
    std::condition_variable wake, done;
    std::vector<std::thread> workers;
    std::vector<std::unique_ptr<AutoxRun>> runs;
    std::vector<std::exception_ptr> failures;
    std::function<void(unsigned)> job;
    unsigned generation = 0, pending = 0;
    bool stopping = false;
    unsigned last = 0;

    void execute(unsigned k) noexcept {
        try { job(k); }
        catch (...) { failures[k] = std::current_exception(); }
    }
    void stop() noexcept {
        { std::lock_guard lock(mutex); stopping = true; }
        wake.notify_all();
        for (auto &thread : workers) if (thread.joinable()) thread.join();
    }
    explicit State(unsigned count) : failures(count) {
        for (unsigned k = 0; k < count; ++k) runs.push_back(std::make_unique<AutoxRun>());
        try {
            for (unsigned k = 1; k < count; ++k) workers.emplace_back([this, k] {
                unsigned seen = 0;
                for (;;) {
                    {
                        std::unique_lock lock(mutex);
                        wake.wait(lock, [&] { return stopping || generation != seen; });
                        if (stopping) return;
                        seen = generation;
                    }
                    execute(k);
                    std::lock_guard lock(mutex);
                    if (--pending == 0) done.notify_one();
                }
            });
        } catch (...) { stop(); throw; }
    }
    ~State() { stop(); }
};

unsigned AutoxParallel::automatic_threads() {
    // Topology is process-wide and settings events ask for this repeatedly.
    static const unsigned count = [] {
        const auto physical = cpu::physical_cores();
        // Logical processors are used only when there is no usable physical topology.
        return std::min(physical ? *physical : cpu::logical_processors(), max_threads);
    }();
    return count;
}
AutoxParallel::AutoxParallel(unsigned threads) : threads_(std::max(threads, 1u)) {}
AutoxParallel::~AutoxParallel() = default;

bool autox_parallel_ready(const Session &s) {
    return s.model().autox_rhs_pure && s.integrator.rhs.function == my_rhs
        && (!solver_info(s.numerics.method).traits.discrete || s.numerics.store_every == 1);
}

void AutoxParallel::select_run(AutoxRun &source, std::ptrdiff_t ndim, std::ptrdiff_t ntst, std::ptrdiff_t ncol) {
    const Session &s = *source.session;
    if (!autox_parallel_ready(s)) {
        source.collocation.reset();
        return;
    }
    // Matrix work grows quadratically with dimension. Floating multiplication
    // avoids overflow at accepted input sizes; this only selects workers.
    const double work = static_cast<double>(ndim) * static_cast<double>(ndim)
        * static_cast<double>(ntst) * static_cast<double>(ncol);
    const unsigned threads = work < min_collocation_work ? 1 : threads_for(s.autox_state.threads);
    if (!source.collocation || source.collocation->threads() != threads)
        source.collocation = std::make_unique<AutoxParallel>(threads);
}

void AutoxParallel::prepare(AutoxRun &source, std::ptrdiff_t ndim, std::ptrdiff_t nbc) try {
    if (!state_) state_ = std::make_unique<State>(threads_);
    const Session &s = *source.session;
    for (auto &run : state_->runs) {
        run->session = source.session;
        run->owner = &source;
        run->parameter_count = source.parameter_count;
        iap_type local{};
        local.ndim = ndim;
        local.nbc = nbc;
        local.run = run.get();
        if (run->scratch.uu1.size() != static_cast<size_t>(ndim)) size_run_scratch(local);
        run->homcont = source.homcont; // RHS wrappers read these; boundary updates stay serial.
        run->rotations = source.rotations; // RHS wrappers only read the rotation counts (a copy of the vector, not a borrow of the source's).
        run->pure_rhs = true;
        run->rhs_constants = s.parser.constants;
        run->rhs_variables = s.parser.variables;
    }
} catch (const std::exception &e) { autox_fail(xpp::format("collocation workers: {}",e.what())); }

void AutoxParallel::publish_rhs(Session &s) {
    // The serial loop leaves the final interval's last finite-difference
    // evaluation in the Session (including SUM's index). Later serial boundary
    // evaluation and the user's Session must see exactly that same state.
    const AutoxRun &last = *state_->runs[state_->last];
    s.parser.constants = last.rhs_constants;
    s.parser.variables = last.rhs_variables;
}

void AutoxParallel::intervals(std::ptrdiff_t n, const std::function<void(std::ptrdiff_t, std::ptrdiff_t, AutoxRun &)> &work) try {
    // All publication and completion uses the same mutex. No calling-thread
    // write to job, runs or failures overlaps a worker's access.
    std::fill(state_->failures.begin(), state_->failures.end(), nullptr);
    const unsigned count = n < static_cast<std::ptrdiff_t>(threads_) ? 1 : threads_;
    state_->last = count - 1;
    // floor(n*k/count), without multiplying a potentially maximal mesh count.
    const std::ptrdiff_t pieces = static_cast<std::ptrdiff_t>(count);
    const auto at = [&](unsigned k) {
        const std::ptrdiff_t part = static_cast<std::ptrdiff_t>(k);
        return (n / pieces) * part + (n % pieces) * part / pieces;
    };
    state_->job = [&](unsigned k) {
        work(at(k), at(k + 1), *state_->runs[k]);
    };
    if (count == 1) state_->execute(0);
    else {
        {
            std::lock_guard lock(state_->mutex);
            state_->pending = threads_ - 1;
            ++state_->generation;
        }
        state_->wake.notify_all();
        state_->execute(0);
        std::unique_lock lock(state_->mutex);
        state_->done.wait(lock, [&] { return state_->pending == 0; });
    }
    state_->job = {};
    // An exact Jacobian a worker found not finite (autox_pp.cpp): its failure is
    // its run's, and the job's cancel, which the main thread alone reads,
    // ends the run as a Stop does.
    for (auto &run : state_->runs) if (run->jacobian_failure) {
        AutoxRun &main = *run->owner;
        if (!main.jacobian_failure) main.jacobian_failure = std::move(run->jacobian_failure);
        run->jacobian_failure.reset();
        job::cancel_current();
    }
    for (const auto &failure : state_->failures) if (failure) {
        try { std::rethrow_exception(failure); }
        catch (const AutoxFailed &) { throw; }
        catch (const std::exception &e) { autox_fail(xpp::format("collocation worker: {}",e.what())); }
        catch (...) { autox_fail("collocation worker: unknown failure"); }
    }
} catch (const AutoxFailed &) { throw; }
  catch (const std::exception &e) { autox_fail(xpp::format("collocation dispatch: {}",e.what())); }
} // namespace xpp
