# W598: W596's thread jobs against the ideal, and min_collocation_work again

Card W598 Threads against processes for branches at once
([#664](https://github.com/MuhammadMoustafa/xppautX/issues/664)), 2026-11-10. The maintainer's decision
(2026-11-06): measure the thread jobs (`follow_all_branches` on, the `threads` setting) at 1, 2, 4 and 8
against the ideal first; worker processes are written only if threads fall clearly short. **No process code
was written.**

## What was measured

Machine: the Ubuntu comparison machine (i7-11700F, 8 cores, 16 threads, governor `performance`, turbo on; nothing else
running, `tools/comparison-setup.sh --ready` before each call). Build: `make xppautx CCACHE=` at master 6e78a0d0
(gcc 15.2). `tools/autox-profile/thread_jobs.py` runs each workload through `autoxcompare.compute()` (the
server, so the wall time includes the start-up and the `.odex` load) at each count, repetitions interleaved;
wall times are measured, never asserted. The plot is
[w598-thread-jobs.png](w598-thread-jobs.png): speed-up over 1 thread, the dashed line of each colour is that model's
ideal, min(threads, jobs), jobs being the branches (`ibr`) its diagram holds. The ideal is generous: a job that starts at
what another found (a cycle from a Hopf point) cannot start before it.

Workloads: the four `tests/autox` descriptions that set `follow_all_branches` (AUTO-07p's demos enz, lrz and ab; the
Hodgkin-Huxley equations), which run for 15 to 70 ms and so say little, and two published models of the comparison
with every branch followed: mu26 and ModelDB 267710 (`motoneuron6c`, 25 states, the comparison's P2/E2 model), each its
steady run with `follow_all_branches` on (max_steps 300 and 600).

| workload | jobs | threads | median wall s | min-max | speed-up | ideal | CPU s |
|---|---|---|---|---|---|---|---|
| enz (n=9) | 1 | 1 / 8 | 0.015 / 0.015 | 0.014-0.016 / 0.014-0.065 | 1.00 / 1.03 | 1 | 0.01 |
| hodgkin_huxley (n=9) | 3 | 1 / 2 / 4 / 8 | 0.069 / 0.115 / 0.103 / 0.097 | within 0.01 | 1 / 0.60 / 0.67 / 0.71 | 1 / 2 / 3 / 3 | 0.06 / 0.13 / 0.16 / 0.17 |
| lrz (n=9) | 5 | 1 / 2 / 4 / 8 | 0.069 / 0.052 / 0.055 / 0.057 | within 0.006 | 1 / 1.31 / 1.24 / 1.20 | 1 / 2 / 4 / 5 | 0.05 / 0.07 / 0.08 / 0.10 |
| ab, both directions (n=9) | 2 | 1 / 2 / 4 / 8 | 0.018 at every count | 0.018-0.020 | 1.00 | 1 / 2 / 2 / 2 | 0.01 |
| mu26 (n=9) | 3 | 1 / 2 / 4 / 8 | 0.858 / 0.564 / 0.441 / 0.443 | 0.849-0.879 / 0.559-0.568 / 0.436-0.455 / 0.435-0.456 | 1 / 1.52 / 1.95 / 1.94 | 1 / 2 / 3 / 3 | 0.79 / 0.81 / 0.87 / 0.88 |
| motoneuron6c (n=6) | 9 (8 at 8) | 1 / 2 / 4 / 8 | 78.2 / 62.0 / 37.2 / 25.8 | 78.1-78.3 / 61.9-62.1 / 37.1-50.3 / 25.7-30.1 | 1 / 1.26 / 2.10 / 3.02 | 1 / 2 / 4 / 8 | 77.9 / 79.8 / 83.0 / 95.6 |

(two long runs at 4 and 8 threads each had one slow repetition: the upper ends of their ranges.)

## What it shows

- **Threads do not reach the ideal, and the shortfall is not contention.** The long workload gives 3.0x at 8 threads
  against an ideal of 8 (2.1x against 4 at 4 threads); mu26 gives 1.95x against 3. The total CPU time barely moves
  (motoneuron6c 78 s at 1 thread, 83 s at 4, 96 s at 8: +23% at 8, 3.7 cores busy on average out of 8): the threads do
  not wait on a lock or fight over the allocator, there is simply a job on the critical path. motoneuron6c's diagram
  holds 731 cycle points, 295 of them one job (`-4`, a cycle from a Hopf point of the steady run, which cannot start
  before that run reaches it), and the whole run at 8 threads (25.8 s) is about what that one job costs
  (an estimate from the point counts, 78 s of CPU over 731 cycle points: I did not time the jobs one by one). Processes
  would not shorten a critical path, so this is not a case for them.
- The collocation threads inside a job (`run.collocation_threads = budget / (running + 1)`, fixed when the job starts)
  can oversubscribe the cores briefly when the first jobs start with the whole budget; the +23% CPU at 8 threads is
  the most that costs here. It was not separated from the jobs' own start-up (the 8-thread run also stops
  `-9`, below).
- **Small runs lose**: Hodgkin-Huxley (70 ms) is 30-40% slower with threads and uses up to three times the CPU; lrz
  gains 20-30% and ab and enz nothing. A thread per job costs more than a few milliseconds of work saves. The four
  `tests/autox` descriptions are too small to time (15 to 70 ms of which most is the program's start); **four are not
  enough** for a measurement, which is why two published models of the comparison were added as workloads, with every
  branch followed. Only these two have run times worth the name; mu26 has three jobs, so its ideal is 3 and no more, and
  a workload with many independent branches of equal cost, the case where 8 threads could pay, is not among the
  published models found here.
- **The diagram depends on the thread count for motoneuron6c**: 1 and 2 threads give 1931 points and 9 branches, 4
  threads 1770 points and 9, 8 threads 1767 points and 8 (the branch `-9`, 164 points, is not there at 4 and 8: the
  W596 join drops a job that repeats another it ran beside; sequentially the repeat is known only after it ran).
  The `tests/autox` follow_all descriptions are byte-equal at 1 and automatic threads (W596), so this is a
  difference that only a larger model shows. It is a design point for the maintainer (should a diagram not depend on the
  thread count, and if it may, where is that written?); the card changed no code and did not trace it further.

## min_collocation_work

`tools/autox-profile/collocation_threads.py` on a scratch build whose `min_collocation_work` is 0 (threads used at any
size): one cycle run, mesh_intervals swept (8 to 256), at 1, 2 and 4 collocation threads, 15 repetitions interleaved;
the size is ndim^2 x mesh_intervals x 4 (`ndim` 2 for lecar, 3 for lrz, 4 for Hodgkin-Huxley). Wall time includes
the program's start and the steady run before the cycles, so every speed-up is understated; where it crosses 1 is not.

| work | lecar 2 / 4 threads | Hodgkin-Huxley 2 / 4 threads | lrz 2 / 4 threads |
|---|---|---|---|
| 512 | 1.04 / 1.02 | 0.94 / 0.91 | 0.96 / 0.92 (576) |
| 1024 | 1.08 / 1.13 | 1.03 / 1.03 | 0.98 / 0.95 (1152) |
| 2048 | 1.16 / 1.28 | 1.15 / 1.20 | 1.01 / 1.00 (2304) |
| 4096 | 1.23 / 1.40 | 1.23 / 1.36 | 1.04 / 1.05 (4608) |
| 16384 | | 1.38 / 1.71 | 1.07 / 1.10 (9216) |

Threads break even between 512 and 1024 for lecar and Hodgkin-Huxley and about 2000 for lrz (whose run spends its time
outside the collocation). **1024 stays**: it is where the gain starts for the two models whose time is mostly
collocation, and the gain just above it is small, so a value in 1000 to 2000 would be as good. The value is not
changed.
