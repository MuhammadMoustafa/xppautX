#pragma once
// AUTO's persistent collocation pool, owned by the running Session's AutoLib.
struct AutoLib;
#include <functional>
#include <memory>

namespace xpp {
struct Session;
class AutoParallel {
public:
    // W251 measured no gain past four workers on PY_S1Bf.
    static constexpr unsigned max_threads = 4;
    // W325 UCRT: work=512 loses, 1280 pays (ndim^2*NTST*NCOL).
    static constexpr long min_collocation_work = 1024;
    static unsigned automatic_threads();
    // Choose once after AUTO has established the run's dimension and mesh.
    static void select_run(AutoLib &source, long ndim, long ntst, long ncol);
    // The count a preference gives: 0 is automatic, else the number itself.
    static unsigned threads_for(unsigned preferred) { return preferred ? preferred : automatic_threads(); }
    // threads is at least 1; the callers' sources keep it within the logical processors (xpp_prefs.h).
    explicit AutoParallel(unsigned threads);
    ~AutoParallel();
    AutoParallel(const AutoParallel &) = delete;
    AutoParallel &operator=(const AutoParallel &) = delete;
    unsigned threads() const { return threads_; }
    void prepare(AutoLib &source, long ndim, long nbc);
    void publish_rhs(Session &s);
    void intervals(long n, const std::function<void(long, long, AutoLib &)> &work);
private:
    struct State;
    unsigned threads_;
    std::unique_ptr<State> state_;
};
bool auto_parallel_ready(const Session &s);
} // namespace xpp
