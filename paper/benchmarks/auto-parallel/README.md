# Parallel AUTO collocation: prototype and speed-up (W251, #307)

Question (maintainer, 2026-10-10): once the model is compiled (W249), how much
does running AUTO's collocation on threads gain, and with the same results?

## Method

- A test build, never merged: the pinned xppautX (88e09741, the paper's WSL
  build, same make command) patched by `../rhs-ceiling/apply_patch.py` (the
  hand-written PY_S1Bf right-hand side, W249) and then `apply_parallel.py`,
  which brings in this folder's `setubv2.cpp`, `conpar2.cpp` and
  `auto_par.inc` (88e09741's files with the changes below) and patches
  `autpp.cpp`'s `func`. Environment switches:
  - `XPP_HAND_RHS=4` (pure): `func` calls the compiled right-hand side with
    `Iapp` from AUTO's own `par` and writes no Session state. Stock `func`
    writes the parameters into the Session, re-evaluates derived values and
    tables, and the right-hand side stores every variable there: shared
    state that threads would race on (finite differences perturb a
    parameter per call). The interpreter cannot be threaded without
    per-thread evaluation state, so the threads are measured with the
    compiled model only.
  - `XPP_FA_FIX=1`: `setubv_make_fa` asks `funi` for `f` only, not the
    finite-difference Jacobian it discards (docs/xppaut-findings.md #39).
  - `XPP_AUTO_THREADS=N`: setubv's Jacobian loop and residual loop, and
    conpar's condensation, run over the mesh intervals in N contiguous
    chunks on a persistent pool (a thread started per call was slower than
    the work at 8 threads: setubv and conpar run a few times per Newton
    step). conpar's updates of the shared `d` block, summed over all
    intervals, are recorded per interval and applied after the threads in
    interval order, the serial order, so no sum is reassociated.
- `parallel.py` (WSL): W246's AUTO run (`../auto/hopf_xppautx.py`: steady
  state, then the periodic branch from the first HB to the EP at 192.813),
  one excluded warm-up and five measured runs per variant, order rotating,
  N threads pinned to N distinct physical cores (CPUs 2, 4, ...; AMD Ryzen 9
  8945HX, 16 cores). Every run's whole diagram (each point of both
  branches: branch, number, x, y, y2) is hashed and compared with the
  interpreter's.
- Results: `results/wsl/` (`runs.json`, `summary.json`, `run.log`).

## Results (2026-10-10)

Periodic run, wall time (CPU time equals it on one thread):

| Variant | Wall s | vs interpreter | CPU s |
|---|---:|---:|---:|
| interpreter (stock) | 23.94 +/- 0.12 | 1 | 23.94 |
| interpreter + make_fa fix | 18.19 +/- 0.16 | 1.32x | 18.19 |
| compiled RHS (W249) | 6.87 +/- 0.07 | 3.48x | 6.87 |
| compiled, pure func | 5.99 +/- 0.06 | 4.00x | 5.99 |
| pure + make_fa fix | 4.85 +/- 0.11 | 4.94x | 4.85 |
| ... 2 threads | 3.44 +/- 0.02 | 6.97x | 4.88 |
| ... 4 threads | 2.81 +/- 0.04 | **8.53x** | 5.04 |
| ... 8 threads | 2.76 +/- 0.02 | 8.68x | 5.59 |

AUTO-07p on the same run (W246): 2.63 s on one thread.

- Every run of every variant has the interpreter's diagram, bit for bit
  (digests `3c0f16696ba0` steady, `a1bd8f247a97` periodic, all 48 runs).
- The make_fa fix alone is worth 1.32x with today's interpreter and 1.24x
  with a compiled model, and changes nothing else.
- Removing the Session writes from `func` (needed anyway for threads) is
  worth 1.15x on one thread.
- Threads: 1.41x on 2, 1.73x on 4, then flat. The part left serial (the
  rest of `brbd`: the reduction, `dimrge`, back substitution; setubv's
  zeroing of its blocks; step control, stability, output) is about 2.2 s:
  Amdahl's law caps the threads near 2x here. 2 threads give most of the
  gain; more than 4 give nothing on this model (ndim 8, NTST 100). A larger
  model (more work per interval) would scale further.
- With 4 threads xppautX's AUTO is about as fast as AUTO-07p on one
  thread, with XPPAUT's results unchanged.

## gcc or TinyCC, with the fix and threads (2026-10-10)

`apply_parallel.py TREE --tcc` (on a tree patched with `apply_patch.py --tcc`,
linked with W250's libtcc) adds `XPP_HAND_RHS=5`: `func` calls the
TinyCC-compiled model (`../rhs-ceiling/tcc_rhs.inc`) the same pure way.
`parallel.py --variants interpreter,pure-fa,pure-fa-2t,pure-fa-4t,tcc-pure-fa,tcc-pure-fa-2t,tcc-pure-fa-4t`,
results `results/wsl-tcc/`:

| Threads | gcc -O2, wall s | TinyCC, wall s | TinyCC / gcc | TinyCC vs interpreter (24.53 s) |
|---:|---:|---:|---:|---:|
| 1 | 4.76 +/- 0.03 | 6.80 +/- 0.08 | 1.43 | 3.61x |
| 2 | 3.44 +/- 0.01 | 4.92 +/- 0.03 | 1.43 | 4.99x |
| 4 | 2.79 +/- 0.01 | 3.96 +/- 0.02 | 1.42 | **6.20x** |

Every run's diagram is the interpreter's, bit for bit. The gap does not
shrink with threads: the right-hand side runs on the threads too, so its
share of the time stays the same.

## What this says

1. The make_fa fix is free: one argument, bit-identical, 1.24-1.32x. It goes
   first, with or without a compiled model.
2. Threads need a pure compiled right-hand side (no Session writes), so
   they come after compiling the model; the interpreter stays serial.
3. The thread count changes no result (the diagram is bit-identical for 1,
   2, 4 and 8), so it is only a speed knob. It saturates by 4 here; beyond
   that the serial part of brbd is the next target (AUTO-07p parallelises
   the reduction too).
