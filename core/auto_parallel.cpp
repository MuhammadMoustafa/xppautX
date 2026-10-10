#include "auto_parallel.h"
#include "session.h"
#include "auto_c.h"
#include "my_rhs.h"
#include "solver.h"
#include <algorithm>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <thread>

namespace xpp {
struct AutoParallel::State {
    std::mutex mutex;
    std::condition_variable wake, done;
    std::vector<std::thread> workers;
    std::vector<std::unique_ptr<AutoLib>> libs;
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
        for (unsigned k = 0; k < count; ++k) libs.push_back(std::make_unique<AutoLib>());
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

unsigned AutoParallel::automatic_threads() {
    const unsigned n = std::thread::hardware_concurrency();
    return n == 0 ? 1 : std::min(n, max_threads);
}
AutoParallel::AutoParallel(unsigned threads) : threads_(std::clamp(threads, 1u, max_threads)) {}
AutoParallel::~AutoParallel() = default;

bool auto_parallel_ready(const Session &s) {
    return s.model().auto_rhs_pure && s.integrator.rhs.function == my_rhs
        && (!solver_info(s.numerics.method).traits.discrete || s.numerics.store_every == 1);
}

void AutoParallel::prepare(AutoLib &source, long ndim, long nbc) try {
    if (!state_) state_ = std::make_unique<State>(threads_);
    const Session &s = *source.session;
    for (auto &lib : state_->libs) {
        lib->session = source.session;
        iap_type local{};
        local.ndim = ndim;
        local.nbc = nbc;
        local.lib = lib.get();
        if (lib->store.uu1.size() != static_cast<size_t>(ndim)) allocate_global_memory(local);
        lib->homcont = source.homcont; // RHS wrappers read these; boundary updates stay serial.
        lib->rotations = source.rotations; // RHS wrappers only read the rotation counts.
        lib->pure_rhs = true;
        lib->rhs_constants = s.parser.constants;
        lib->rhs_variables = s.parser.variables;
    }
} catch (const std::exception &e) { auto_fail(xpp::format("collocation workers: {}",e.what())); }

void AutoParallel::publish_rhs(Session &s) {
    // The serial loop leaves the final interval's last finite-difference
    // evaluation in the Session (including SUM's index). Later serial boundary
    // evaluation and the user's Session must see exactly that same state.
    const AutoLib &last = *state_->libs[state_->last];
    s.parser.constants = last.rhs_constants;
    s.parser.variables = last.rhs_variables;
}

void AutoParallel::intervals(long n, const std::function<void(long, long, AutoLib &)> &work) try {
    // All publication and completion uses the same mutex. No calling-thread
    // write to job, libs or failures overlaps a worker's access.
    std::fill(state_->failures.begin(), state_->failures.end(), nullptr);
    const unsigned count = n < static_cast<long>(threads_) ? 1 : threads_;
    state_->last = count - 1;
    // floor(n*k/count), without multiplying a potentially maximal mesh count.
    const long pieces = static_cast<long>(count);
    const auto at = [&](unsigned k) {
        const long part = static_cast<long>(k);
        return (n / pieces) * part + (n % pieces) * part / pieces;
    };
    state_->job = [&](unsigned k) {
        work(at(k), at(k + 1), *state_->libs[k]);
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
    for (const auto &failure : state_->failures) if (failure) {
        try { std::rethrow_exception(failure); }
        catch (const AutoFailed &) { throw; }
        catch (const std::exception &e) { auto_fail(xpp::format("collocation worker: {}",e.what())); }
        catch (...) { auto_fail("collocation worker: unknown failure"); }
    }
} catch (const AutoFailed &) { throw; }
  catch (const std::exception &e) { auto_fail(xpp::format("collocation dispatch: {}",e.what())); }
} // namespace xpp
