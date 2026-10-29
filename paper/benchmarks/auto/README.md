# AUTO speed: XPPAUT 8.0, xppautX and AUTO-07p (W246, #302)

Question (maintainer, 2026-10-09): people report AUTO-07p as faster than
XPPAUT's AUTO; how do XPPAUT 8.0, xppautX and AUTO-07p compare on one
long continuation, and where does xppautX's time go?

## The problem

`models/PY_S1Bf.ode` (the maintainer's sandbox model: a cortical pyramidal
cell, 8 states, Traub/Pospischil kinetics, Iapp in pA). Its steady-state
I-V is N-shaped: the rest branch has an HB at 153.39 pA and a fold at
158 pA, the middle branch folds again at -23457.6 pA, and the depolarised
branch has the depolarisation-block HB at **Iapp = 16409.42 pA, V =
-19.624 mV** (`explore_xppautx.py`, with ParMin -200000).

The benchmark (`models/PY_S1Bf_bench.ode`, and its `.odex`): Iapp = 20000
pA, the initial condition the depolarised equilibrium there (Newton-
converged by xppautX's Sing pts), every AUTO constant an option of the
file, so each program runs with the same ones:

1. Steady state downward in Iapp from 20000 to the HB.
2. Periodic branch from that HB down to Iapp = 200 pA (ParMin): Ntst 100, Ncol 4, Nmax
   5000, Ds -1, Dsmin 1e-4, Dsmax 50, EPSL/EPSU/EPSS 1e-4, IAD 3, MXBF 5,
   IID 2, ITMX 8, ITNW 7, NWTN 3, JAC 0 (finite differences), THL {11: 0}
   (XPPAUT's fixed constants, core/autevd.cpp `init_auto`).

ParMin 200 stops the timed run before the branch's end, where the programs
part: unbounded (ParMin 0), the periodic branch runs from the HB (a BP at
16394.4 pA just after it) down to a fold of cycles at Iapp = 151.506 pA, where the period grows
without bound (AUTO-07p's PAR(11) climbs from 4e7 to 3e10 ms over its last
points) and the run ends with MX: an infinite-period end next to the
equilibrium fold at 158 pA, not a reachable Iapp = 0.

## Drivers

- xppautX: `hopf_xppautx.py` over `--server` (tools/xppclient.py).
- XPPAUT 8.0: `hopf_xppaut.py`, its X11 window on a private Xvfb display
  driven with xdotool by the keys a user presses; AUTO's point table on
  XPPAUT's stdout says when a run has ended. XPPAUT has no batch mode for
  AUTO.
- AUTO-07p 0.9.3 (built in WSL with OpenMP): `auto07p/py_s1bf.c` (the same
  right-hand side in C), `c.py_s1bf.eq`, `c.py_s1bf.po`, `py_s1bf.auto`. Its
  `auto_f2c.h` declares `func()` K&R style, which gcc 15's default C23
  rejects: the equation object is compiled first with `gcc -std=gnu17 -g
  -O2 -I$AUTO_DIR/include` (AUTO's own CFLAGS otherwise).

## Protocol

`bench.py` (WSL): one excluded warm-up, then five measured fresh processes
per engine, the order rotating each repetition; XPPAUT and xppautX pinned
to CPU 2 (a `taskset` wrapper that execs the program, so its PID and CPU
time are the program's), AUTO-07p's threads to CPUs 2..2+n-1. Measure: the
CPU time of the program during the periodic run (XPPAUT's and xppautX's
process; AUTO-07p's executable, a child of its Python driver), and wall
time. Builds: XPPAUT 8.0 c021b511 and xppautX 88e09741 as in
`../five-model-screen/provenance.json` (gcc 15.2, -g -O2,
-ffp-contract=off); AUTO-07p 0.9.3 (gfortran/gcc 15.2, its own flags,
OpenMP). Results: `results/wsl/` (`runs.json` every run with its labelled
points, `summary.json`, `run.log`).

## Results (2026-10-09)

Every run of every engine computes the same continuation: the steady-state
run 403 points (HB at 16409.42 pA), the periodic run 336 points (BP at
16394.43 pA, EP at 192.813 pA).

| Engine | Periodic run, CPU s | Wall s | vs xppautX |
|---|---:|---:|---:|
| XPPAUT 8.0 | 22.44 +/- 0.30 | 22.89 | 0.94 |
| xppautX | 23.88 +/- 0.09 | 23.88 | 1 |
| AUTO-07p, 1 thread | 2.63 +/- 0.03 | 2.64 | 9.1x faster |
| AUTO-07p, 2 threads | 2.84 +/- 0.03 | 1.43 | 16.7x faster (wall) |
| AUTO-07p, 4 threads | 3.49 +/- 0.18 | 0.89 | 26.8x faster (wall) |

(+/- one standard error of five runs.)

- xppautX's AUTO is 6.4 +/- 1.5% slower than XPPAUT 8.0's on this run,
  the same algorithm (both run XPPAUT's AUTO, AUTO2000's C code) evaluating the
  same interpreted right-hand side.
- AUTO-07p on one thread is 9.1x faster than xppautX (8.5x than XPPAUT),
  and its OpenMP collocation scales 1.84x on 2 threads, 2.97x on 4.

## Where xppautX's time goes

callgrind, a shortened periodic run (Nmax 60) of xppautX 88e09741
(`results/wsl/callgrind-nmx60.out`; the run also integrates 2000 ms before
AUTO, excluded below). Of AUTO's instructions (`do_auto`, 36.8e9):

| Part | Share |
|---|---:|
| the model's right-hand side (`func` -> `my_rhs` -> the expression interpreter `eval_rpn`), called by AUTO's finite-difference Jacobians (`funi`) | 84% |
| block linear algebra, `brbd` (with `conpar` 10%) | 12% |
| the rest | 4% |

Inside the right-hand side, the correctly rounded `exp` and `pow`
(CORE-MATH, W159) are about 13%; the interpreter's own work is the rest.
The interpreter, not the Fortran-to-C translation or the collocation, is
why AUTO-07p is ~9x faster: AUTO-07p calls the model as compiled C.

Consequence for W247 (parallel collocation): `setubv` holds 70% of AUTO's
instructions, but nearly all of it is right-hand-side calls; parallelising
it pays only if the interpreter can run on several threads at once (it
evaluates into the Session's shared state today), and then at most ~2x on
4 threads (Amdahl, 70% parallel). Making the right-hand side itself fast
(compiling the model's expressions, or a faster evaluator) addresses the
84%.


## W269 (#326, 2026-10-11): the compiled build, threads and the exact Jacobian

The same periodic run (steady state from Iapp = 20000 pA to the HB, then the
periodic branch to Iapp = 200 pA), timed again after xppautX began compiling
each model at load (W258/W259/W266, TinyCC), running AUTO's collocation on
threads (W247; the count is the `auto_threads` preference, set per run with
`{"cmd":"auto","op":"set","threads":N}` in a fresh `XPP_CONFIG_DIR`, so a
user's own preference is never read) and offering an exact Jacobian
(W260/W275: the Numerics' `jacobian` set to `exact` before AUTO opens,
which then runs with JAC 1). Engines (`bench.py --engines`):

- `xppaut`: XPPAUT 8.0 (W246's driver, Xvfb display `:93`).
- `xppautx-interp-1t`: xppautX `--no-compile`, one thread, finite differences.
- `xppautx-fd-Nt`, `xppautx-exact-Nt`: xppautX compiled, N = 1, 2, 4 threads,
  finite-difference or exact Jacobian; pinned to CPUs 2..2+N-1.
- `auto07p-Nt`: AUTO-07p 0.9.3, JAC 0 (W246's). `auto07p-jac-Nt`: JAC 1 with
  the Jacobian written by hand in `auto07p/py_s1bf.c` (its JAC 0 path is
  W246's), checked against central differences by `auto07p/jac_check.c`
  (largest relative difference 5e-8 over ten voltages).

Builds: XPPAUT 8.0 c021b511 and xppautX `0643234f` as
`../five-model-screen/provenance-w269.json` (WSL, gcc 15.2, -g -O2,
-ffp-contract=off); xppautX reads `models/w269/PY_S1Bf_bench.odex`, the bench
`.ode` converted by that build (W246's `.odex` is the older format, which
today's xppautX refuses: "a statement cannot start with `@`"). One excluded
warm-up, five measured runs per engine, the order rotating
(`bench.py`'s `series`, now shared with `../modeldb-267710/auto_bench.py`).
Results: `results/w269-wsl-2026-10-11/` (`runs.json` with every point,
`summary.json` with sample SDs, `run.log`, `provenance.json`).

| Engine | Periodic run, wall s | CPU s | points | XPPAUT / engine (wall) | labelled points (periodic) |
|---|---:|---:|---:|---:|---|
| xppaut | 21.93 +/- 0.47 | 21.55 +/- 0.43 | [336] | 1.0x | BP 16394.4, EP 192.813 |
| xppautx-interp-1t | 10.30 +/- 0.12 | 10.30 +/- 0.12 | [336] | 2.1x | BP 16394.4, EP 192.813 |
| xppautx-fd-1t | 4.90 +/- 0.05 | 4.90 +/- 0.05 | [336] | 4.5x | BP 16394.4, EP 192.813 |
| xppautx-fd-2t | 3.16 +/- 0.02 | 5.16 +/- 0.02 | [336] | 6.9x | BP 16394.4, EP 192.813 |
| xppautx-fd-4t | 2.26 +/- 0.04 | 5.34 +/- 0.04 | [336] | 9.7x | BP 16394.4, EP 192.813 |
| xppautx-exact-1t | 3.24 +/- 0.02 | 3.24 +/- 0.03 | [336] | 6.8x | BP 16394.4, EP 192.813 |
| xppautx-exact-2t | 2.30 +/- 0.03 | 3.45 +/- 0.03 | [336] | 9.5x | BP 16394.4, EP 192.813 |
| xppautx-exact-4t | 1.84 +/- 0.02 | 3.64 +/- 0.04 | [336] | 11.9x | BP 16394.4, EP 192.813 |
| auto07p-1t | 2.44 +/- 0.02 | 2.43 +/- 0.02 | [336] | 9.0x | BP 16394.4, EP 192.813 |
| auto07p-2t | 1.31 +/- 0.02 | 2.60 +/- 0.04 | [336] | 16.7x | BP 16394.4, EP 192.813 |
| auto07p-4t | 0.73 +/- 0.02 | 2.84 +/- 0.07 | [336] | 30.2x | BP 16394.4, EP 192.813 |
| auto07p-jac-1t | 1.69 +/- 0.01 | 1.68 +/- 0.01 | [336] | 13.0x | BP 16394.4, EP 192.813 |
| auto07p-jac-2t | 0.95 +/- 0.02 | 1.87 +/- 0.05 | [336] | 23.1x | BP 16394.4, EP 192.813 |
| auto07p-jac-4t | 0.53 +/- 0.01 | 2.06 +/- 0.03 | [336] | 41.4x | BP 16394.4, EP 192.813 |

(mean +/- sample SD of five runs; "XPPAUT / engine" is the ratio of wall times.)

- Every run of every engine gives the same 336-point branch, BP at 16394.4 and
  EP at 192.813 (W246's). xppautX's whole diagram (every point's values) is
  bit-identical between the interpreter and the compiled build and across 1, 2
  and 4 threads (one digest, `a1bd8f24...`); with the exact Jacobian it is one
  other digest on every thread count (`f34cbad2...`), its points within
  0.0076 of the finite-difference ones.
- Since W246, xppautX's AUTO went from 23.9 s (6% slower than XPPAUT) to:
  10.3 s interpreted (2.1x faster than XPPAUT; among the changes since, W253's
  `setubv_make_fa` fix, XPPAUT finding #39, and the evaluator's work, not separated here),
  4.9 s compiled (4.5x), 2.26 s compiled on 4
  threads (9.7x), 1.84 s with the exact Jacobian on 4 threads (11.9x).
- AUTO-07p is still faster: one thread 2.44 s (xppautX compiled FD 2.0x
  slower), 4 threads 0.73 s (3.1x); with exact Jacobians on both sides,
  1.69 s against 3.24 s (1.9x) and 0.53 s against 1.84 s on 4 threads (3.5x).
  xppautX's threads scale 2.17x on 4 (FD) and 1.76x (exact); AUTO-07p's
  3.35x and 3.19x (W250's note: AUTO2000's serial `brbd` remainder).
- The steady-state run is short everywhere (xppautX 0.02-0.05 s, XPPAUT
  0.65 s, AUTO-07p 0.44 s wall including its Python start).
