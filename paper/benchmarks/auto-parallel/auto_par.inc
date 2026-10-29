/* W251 (#307), test build only, never merged: the switches of the parallel
   collocation prototype, read once from the environment.
     XPP_AUTO_THREADS=N  setubv's per-interval loops and conpar's condensation on N
                         threads (static chunks of the mesh intervals); default 1
     XPP_FA_FIX=1        setubv_make_fa asks funi for f only (ijac 0), not the
                         Jacobian it throws away (ijac 2, AUTO2000's and XPPAUT's) */
#include <condition_variable>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace {

int auto_threads()
{
  static const int n = [] { const char *e = std::getenv("XPP_AUTO_THREADS"); int v = e ? std::atoi(e) : 1; return v < 1 ? 1 : v; }();
  return n;
}

bool fa_fix()
{
  static const bool on = [] { const char *e = std::getenv("XPP_FA_FIX"); return e && e[0] == '1'; }();
  return on;
}

/* auto_threads()-1 workers started once and kept: run(f) calls f(k) on worker k and
   f(0) here, and returns when all are done (a new thread per call cost more than the
   work: AUTO calls setubv and conpar a few times per Newton step) */
class Pool {
public:
  explicit Pool(int n)
  {
    for (int k = 1; k < n; k++)
      workers_.emplace_back([this, k] {
        long seen = 0;
        for (;;) {
          std::function<void(int)> job;
          {
            std::unique_lock<std::mutex> lock(m_);
            wake_.wait(lock, [&] { return stop_ || generation_ != seen; });
            if (stop_) return;
            seen = generation_;
            job = job_;
          }
          job(k);
          std::lock_guard<std::mutex> lock(m_);
          if (--pending_ == 0) done_.notify_one();
        }
      });
  }
  ~Pool()
  {
    {
      std::lock_guard<std::mutex> lock(m_);
      stop_ = true;
    }
    wake_.notify_all();
    for (auto &w : workers_) w.join();
  }
  void run(const std::function<void(int)> &f)
  {
    {
      std::lock_guard<std::mutex> lock(m_);
      job_ = f;
      pending_ = static_cast<int>(workers_.size());
      ++generation_;
    }
    wake_.notify_all();
    f(0);
    std::unique_lock<std::mutex> lock(m_);
    done_.wait(lock, [&] { return pending_ == 0; });
  }

private:
  std::vector<std::thread> workers_;
  std::mutex m_;
  std::condition_variable wake_, done_;
  std::function<void(int)> job_;
  long generation_ = 0;
  int pending_ = 0;
  bool stop_ = false;
};

Pool &pool()
{
  static Pool p(auto_threads());
  return p;
}

/* fn(begin, end) over [0, n) in auto_threads() contiguous chunks, the first on this thread */
template <class F> void par_for(int n, F &&fn)
{
  const int t = auto_threads();
  if (t <= 1 || n < t) {
    fn(0, n);
    return;
  }
  pool().run([&](int k) { fn(n * k / t, n * (k + 1) / t); });
}

} // namespace
