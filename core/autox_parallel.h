#pragma once
// autoX's persistent collocation pool, owned by the running Session's AutoxLib.
struct AutoxLib;
#include <functional>
#include <cstddef>
#include <memory>

namespace xpp {
struct Session;
class AutoxParallel {
public:
    // W251 measured no gain past four workers on PY_S1Bf.
    static constexpr unsigned max_threads = 4;
    // W325 UCRT: work=512 loses, 1280 pays (ndim^2*NTST*NCOL).
    static constexpr std::ptrdiff_t min_collocation_work = 1024;
    static unsigned automatic_threads();
    // Choose once after autoX has established the run's dimension and mesh.
    static void select_run(AutoxLib &source, std::ptrdiff_t ndim, std::ptrdiff_t ntst, std::ptrdiff_t ncol);
    // The count a preference gives: 0 is automatic, else the number itself.
    static unsigned threads_for(unsigned preferred) { return preferred ? preferred : automatic_threads(); }
    // threads is at least 1; the callers' sources keep it within the logical processors (xpp_prefs.h).
    explicit AutoxParallel(unsigned threads);
    ~AutoxParallel();
    AutoxParallel(const AutoxParallel &) = delete;
    AutoxParallel &operator=(const AutoxParallel &) = delete;
    unsigned threads() const { return threads_; }
    void prepare(AutoxLib &source, std::ptrdiff_t ndim, std::ptrdiff_t nbc);
    void publish_rhs(Session &s);
    void intervals(std::ptrdiff_t n, const std::function<void(std::ptrdiff_t, std::ptrdiff_t, AutoxLib &)> &work);
private:
    struct State;
    unsigned threads_;
    std::unique_ptr<State> state_;
};
bool autox_parallel_ready(const Session &s);
} // namespace xpp
