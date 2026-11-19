#pragma once
// autoX's persistent collocation pool, owned by the run's AutoxRun.
struct AutoxRun;
#include "autox_parts.h"
#include <chrono>
#include <functional>
#include <cstddef>
#include <memory>

namespace xpp {
class AutoxParallel {
public:
    // W251 measured no gain past four workers on PY_S1Bf.
    static constexpr unsigned max_threads = 4;
    // W325 UCRT: work=512 loses, 1280 pays (ndim^2*NTST*NCOL).
    static constexpr std::ptrdiff_t min_collocation_work = 1024;
    // How long a pool thread (a worker waiting for the next dispatch, the caller for the workers) spins before it sleeps:
    // a Newton step's dispatches follow within tens of microseconds, and a sleeping thread takes about as long to wake.
    // W835 measured ode_ma23's cycles at 4 threads 0.107 s with none, 0.096 s at 20 to 300 us, and P1 fixed (long serial
    // gaps) unchanged at 100 and 2% slower at 50; a longer spin only burns CPU after the last dispatch.
    static constexpr std::chrono::microseconds spin_time{100};
    static unsigned automatic_threads();
    // Choose once after autoX has established the run's dimension and mesh.
    static void select_run(AutoxRun &source, std::ptrdiff_t ndim, std::ptrdiff_t ntst, std::ptrdiff_t ncol);
    // The count a setting gives: 0 is automatic, else the number itself.
    static unsigned threads_for(unsigned preferred) { return preferred ? preferred : automatic_threads(); }
    // threads is at least 1; the callers' sources keep it within the logical processors (xpp_settings.h).
    explicit AutoxParallel(unsigned threads);
    ~AutoxParallel();
    AutoxParallel(const AutoxParallel &) = delete;
    AutoxParallel &operator=(const AutoxParallel &) = delete;
    unsigned threads() const { return threads_; }
    void prepare(AutoxRun &source, std::ptrdiff_t ndim, std::ptrdiff_t nbc);
    void publish_rhs(AutoxRun &source);
    void intervals(std::ptrdiff_t n, const std::function<void(std::ptrdiff_t, std::ptrdiff_t, AutoxRun &)> &work);
private:
    struct State;
    unsigned threads_;
    std::unique_ptr<State> state_;
};
bool autox_parallel_ready(const AutoxParts &s);
} // namespace xpp
