#pragma once
/* A sweep's independent runs side by side on threads (W434).

   A sweep (Initial conds > Range and its two-parameter form, the parameter range, Stochastic > Compute, the Lyapunov
   exponent over a range, the array plot's range: integrate.cpp's range_sweep) integrates the model once for each value
   of what it varies. When "Use old I.C.s" is on, every run starts from the same initial data and no run reads what
   another left, so the runs may be computed at the same time. A run is computed on a worker: a Session of the sweep's
   Model made by make_sweep_worker, which holds what an integration reads and none of the windows, plots and
   browser, so a run reaches the front end for nothing (IntegratorState::on_worker). The runs are joined in their serial
   order on the calling (main) thread by run_ordered: the rows, the files, the messages and the front end's drawing come
   out as if the runs had run one after another, whatever the thread count and whichever run finished first.

   Which models may: sweep_serial_reason. A sweep that may not runs its runs one after another on the Session itself,
   the same code (range_sweep's compute_run), and says why when more than one thread was asked for.

   sweep_runs.cpp. */
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "xpp_error.h"

namespace xpp {

struct Session; /* session.h */

/* why s's runs may not be computed side by side on workers, or "" when they may: the model's right-hand side is not
   pure (Model::rhs_impurity, the same condition autoX's collocation threads need), or the Session itself does what a
   worker does not (the animation draws as the run goes, a replay armed a stop at a row, an unchanged-digits run) */
std::string sweep_serial_reason(const Session &s);

/* says, at INFO, that the runs of the sweep named (Range, Range Equilibria, Range Shoot) go one after another though the
   thread setting gave more than one thread, and why, when there is more than one run */
void say_sweep_serial(const Session &s, std::string_view sweep, std::int64_t runs, std::string_view why);

/* a worker for a sweep's runs: a Session of s's Model with the numerics, the parser's constants and variables, the
   tables, the global flags and the initial data s has now, room for `rows` rows, its own solver and generator. s is
   untouched. The worker is made once for the sweep and reused for one run after another. */
std::unique_ptr<Session> make_sweep_worker(Session &s, int rows);

/* how many runs a thread may be ahead of the one the caller has taken in: the results waiting in order are held in
   memory, so a long sweep does not hold every run's rows at once */
inline constexpr std::size_t SWEEP_RUNS_AHEAD = 4;
inline constexpr std::size_t sweep_window(unsigned threads) { return SWEEP_RUNS_AHEAD * threads; }

/* Runs the items 0 to count-1 on `threads` threads: compute(worker, item), worker being the thread's number from 0
   (it must not throw: a run's failure is its result), and hands each item to consume(item) on the calling thread, in
   the order 0, 1, 2, ..., as soon as it and the ones before it are done. consume gives false to stop (nothing is
   started after it, and the items computed ahead are dropped). A cancel (xpp_job.h) ends it too: the item the caller is
   at and the ones not started are not waited for, and the front end is polled for Abort meanwhile. At most
   sweep_window(threads) items are started ahead of the one taken in. True when the front end's `/` key ended it (as it
   ends a sweep on one thread, without a cancel); the error is that a thread could not be started. */
inline constexpr int END_SWEEP_KEY = '/';
Result<bool> run_ordered(std::size_t count, unsigned threads, const std::function<void(unsigned worker, std::size_t item)> &compute,
                         const std::function<bool(std::size_t item)> &consume);

} // namespace xpp
