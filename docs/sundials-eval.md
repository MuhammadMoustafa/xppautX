# W34: SUNDIALS CVODE against the vendored CVODE

Evaluation for the maintainer's decision (card W34, issue #72). Nothing on
master is replaced; the work is on branch `task/W34-sundials`.

## Summary

**SUNDIALS' CVODE (7.9.0) and the vendored one are the same method with the
same defaults, and on every model run they give the same accuracy and nearly
the same work.** There is no accuracy or speed to win from switching. What
switching would cost is real (about 29,000 lines of vendored C plus a CMake
configuration, a libm bridge for W159, five rewritten example baselines);
what it would buy is features xppautX does not use yet (root finding, sparse
and Krylov solvers, Adams, sensitivities). **Recommendation: do not switch
now; adopt SUNDIALS only as the vehicle of a card that needs one of those
features** (below).

## What was built

- SUNDIALS CVODE 7.9.0 (`cvode-7.9.0.tar.gz`, BSD-3, sha256 ae160233...a8e),
  built in WSL's home (`~/sundials-eval`, `~/sundials-install`) with CMake
  4.4.4 (Kitware's binary tarball: WSL had no CMake and no sudo), Release,
  static, double precision, 64-bit indices, `-ffp-contract=off`, CVODE with
  the serial vector and the dense and band solvers only. Its configure and
  build take 12 s and 6 s on three cores.
- `core/cv_backend.h`: the interface xppautX's CVODE driver (`core/cv2.cpp`)
  steps through, `CvodeMemory`. `core/cv_vendored.cpp` holds what cv2.cpp
  did with the vendored CVODE's C API (moved, numerics unchanged);
  `core/cv_sundials.cpp` (160 lines) is the adapter for SUNDIALS: BDF,
  Newton, dense or band difference-quotient Jacobian, scalar tolerances,
  2000 steps per call (the vendored CVODE's `MXSTEP_DEFAULT`; cvode.h's
  comment says 500), error text mapped to the vendored flags.
- `make SUNDIALS=<prefix of the install>` (off by default) leaves out the
  seven vendored files and the vendored backend and links the adapter.
  Without it the build is as before: `tools/examples_check.sh` on the
  default build, 184 models, 183 outputs match `tests/examples.md5`
  (`ml-range` has none by design), `WERROR=1` clean on WSL gcc 15, and the
  two moved files compile clean with UCRT gcc 16 and clang.
- Each CVODE integration logs one DEBUG line, `cvode stats: nst= nfe= rhs=
  nje= nsetups= nni= ncfn= netf=` (`rhs` counts every call of the
  right-hand side, the Jacobian's included; `nfe` is CVODE's own count of
  them). `tools/sundials_eval.py` runs both programs over the models and
  `tools/sundials_report.py` makes the tables below (models:
  `tools/models/sundials/`).
- On the SUNDIALS build, `tools/servercheck.py` (all of it), `tools/autocheck.py
  memory` (CVODE stopped by a delay, by the Poincare map) and
  `tools/examples_check.sh` ran: the first two pass unchanged, the third
  differs in the five examples that use CVODE and in no other.

## Method

Every run is `xppautX model --silent --debug` in a folder of its own, with
`@ meth=cvode, tol=T, atol=T` appended (`toler` and `atoler` together: see
finding 1) for T = 1e-3 ... 1e-10, and the examples as they are. References:

- closed forms, as aux columns the model works out in double precision
  (osc, pr, the heat equation, the oscillator at amplitude 1e6): xppautX
  stores its columns in single precision, the time column included, so
  nothing below about 1e-7 can be read from `output.dat`;
- SciPy DOP853 at 1e-13 (Van der Pol), Radau at 1e-12 (Robertson), the exact
  bounce times (ball);
- DoPri8(3) at 1e-13 for the examples, where it finishes (it costs
  3 to 36 s there); SUNDIALS' and the vendored CVODE at 1e-11 or 1e-13 for
  the stiff ones (their agreement is printed above each table).

"Error" is the maximum over rows and states of |y - ref| / (1 + |ref|); the
heat equation's is a root mean square over its 100 points. "Steps",
"f evals", "Jac evals" are CVODE's counts summed over the integrations of a
run (events and the Poincare map restart CVODE). Times are the best of 5 to
7 runs of the whole process (startup 0.027 s included); other jobs shared the
laptop (the timing pass held the machine's heavy-job lock).

## Results

### Same results

Where a model is smooth CVODE-for-CVODE the two are bit for bit alike:
the step, f and Jacobian counts and the errors of the oscillator, the heat
equation (dense and banded), the bouncing ball, the Poincare map, atcoaster
and kuramot100 are identical at every tolerance (kuramot100's whole
`output.dat` is identical; atcoaster's differs in one row by 2e-10).

| model | tol | err vendored | err SUNDIALS | steps v / S | f evals v / S | Jac v / S |
|---|---|---|---|---|---|---|
| osc (non-stiff, exact) | 1e-4 | 4.56e-2 | 4.56e-2 | 669 / 669 | 730 / 730 | 12 / 12 |
| | 1e-6 | 2.38e-4 | 2.38e-4 | 836 / 836 | 913 / 913 | 14 / 14 |
| | 1e-8 | 3.95e-6 | 3.95e-6 | 1860 / 1860 | 1935 / 1935 | 31 / 31 |
| heat, 100 equations (stiff, exact) | 1e-6 | 2.67e-6 | 2.67e-6 | 78 / 78 | 317 / 317 | 2 / 2 |
| | 1e-10 | 6.43e-10 | 6.43e-10 | 270 / 270 | 818 / 818 | 5 / 5 |
| heat, banded | 1e-6 | 2.67e-6 | 2.67e-6 | 78 / 78 | 123 / 123 | 2 / 2 |
| | 1e-10 | 6.43e-10 | 6.43e-10 | 270 / 270 | 333 / 333 | 5 / 5 |

The banded solver (`@ bandup=1, bandlo=1`) saves the same 60% of the f
evaluations in both.

### Small differences, no winner

SUNDIALS 7.9 has small changes to the step-size selection since the 1990s
code; they move the step counts by up to a few percent and the error a
little either way. They were not traced to a line.

| model | tol | err vendored | err SUNDIALS | steps v / S | f evals v / S |
|---|---|---|---|---|---|
| Van der Pol | 1e-4 | 2.65e-3 | 1.13e-2 | 1143 / 1155 | 1640 / 1701 |
| | 1e-6, 1e-8 | same | same | 2423 / 2423, 5115 / 5115 | same |
| Prothero-Robinson (stiff, exact) | 1e-8 | 9.50e-9 | 7.13e-9 | 352 / 339 | 440 / 415 |
| | 1e-10 | 5.35e-11 | 5.35e-11 | 636 / 672 | 755 / 814 |
| Robertson (stiff) | 1e-6 | 1.37e-5 | 1.43e-5 | 255 / 264 | 483 / 492 |
| | 1e-8 | 2.85e-7 | 2.22e-7 | 557 / 527 | 1010 / 959 |
| Brusselator, 100 equations | 1e-6 | | | 1298 / 1328 | 4080 / 4117 |
| Van der Pol mu=1000 | 1e-8 | | | 2864 / 2962 | 4127 / 4261 |

The five examples that use CVODE (`candelator.ode`, the sixth, does not load:
its `f(t)` is cut off mid-line at line 16), as written, against DoPri8(3):

| example | steps v / S | f evals v / S | max difference of the two outputs | error v / S against DoPri8(3) |
|---|---|---|---|---|
| atcoaster | 2474 / 2474 | 2805 / 2805 | 2e-10 (one row) | 1.5e-7 / 1.5e-7 |
| fieldnoy | 6676 / 6596 | 10608 / 10588 | 4.5e-3 | 3.4e-4 / 4.5e-3 |
| itoy | 9563 / 9510 | 13581 / 13493 | 1.3 | 1.7 / 1.7 |
| toy_ok | 12523 / 15779 | 19044 / 23815 | 3.8 | 3.2 / 4.7 |
| waterwheel | 5402 / 5400 | 11095 / 11251 | 1.6 | 4.9 / 2.1 |

itoy, toy_ok and waterwheel are oscillators and a chaotic wheel run to
tolerances of 1e-4 to 1e-5: their trajectories drift apart by a phase
whichever CVODE integrates, so the 1 to 5 above are the models' sensitivity,
not a flaw of either (at 1e-13 the two CVODEs and DoPri8(3) agree to 1e-6 on
itoy). toy_ok's 26% more steps is the one case where SUNDIALS did visibly more
work.

### The same failures

Where CVODE gives up both do, at the same time: Robertson at 1e-3 and 1e-4
("Too much work", at t = 0.379994 in both, 2000 steps), a delay equation
at 1e-10 (the error test fails repeatedly at t = 1.9521, h = 1.06e-7), and
the 1e-13 reference runs of fieldnoy and toy_ok. The wording differs
("CVode-- At t=0.379994, mxstep=2000 steps taken ..." against "At t =
0.379994360260136, mxstep steps taken before reaching tout."); xppautX's own
`servercheck.py` and `autocheck.py`'s `memory` section pass unchanged on the
SUNDIALS build. SUNDIALS counts nonlinear convergence failures (`ncfn`) where
the vendored CVODE reports 0 in the same runs (13 in toy_ok, 54 in fieldnoy):
a counting difference, since the steps and work agree.

### Delays, the Poincare map, events, stopping

All four work on the SUNDIALS build and give what the vendored CVODE gives:

- **Delays** (Hutchinson's equation, tau = 1.8): 408 (SUNDIALS) / 440 (vendored) steps at 1e-3 up
  to 13552 / 13548 at 1e-8, errors against DoPri8(3) of 2.6, 1.7 / 1.5,
  0.68 / 0.63, 0.40 / 0.42 (S / v where they differ): the history is read
  from the stored rows (every dt, single precision), which no integrator can
  improve.
- **The Poincare map** (`poimap=section`): 16 crossings, errors identical to
  every digit at every tolerance (1.2e-3: the section is interpolated on the
  stored rows, not CVODE's).
- **Events** (`event -1 y, v = -0.8*v`: a bouncing ball, 5 CVODE integrations: 4
  restarts at the bounces): identical counts and errors (the same 1.3e-3 against the exact
  bounce times; again the event is located on the stored rows).
- **Stopping**: `@ bound=1e4` on x' = x^2 stops both at t = 1.05 with the
  same message; a CVODE failure stops both runs at the same row.

### Speed

CPU seconds of the whole process, best of 7, startup (0.027 s) included;
the model's right-hand side dominates all but the first lines.

| case | vendored | SUNDIALS | steps v / S |
|---|---|---|---|
| oscillator, 20000 time units, tol 1e-8 (CVODE's own step cost) | 0.196 | 0.235 | 366,961 both |
| same, tol 1e-6 | 0.104 | 0.121 | 162,836 both |
| Brusselator, 100 equations dense, tol 1e-8 | 0.199 | 0.184 | 2671 / 2677 |
| itoy | 0.283 | 0.283 | 9563 / 9510 |
| waterwheel | 0.150 | 0.152 | 5402 / 5400 |
| kuramot100 (100 equations) | 0.413 | 0.410 | 29 both |
| toy_ok | 0.358 | 0.447 | 12523 / 15779 |

On a right-hand side that costs nothing SUNDIALS spends about 0.55 us a step
against 0.45 us (+23%), its own build at -O3 against the vendored code at
-O2 (the per-step overhead of the newer code's context and function-pointer
layers; not profiled). The dense factorisation of 100 x 100 is a little
faster (-7%). On real models the two are within noise except toy_ok, whose
extra time is its extra steps. The stripped xppautX is 135 KB larger
(6,153,720 against 6,018,552 bytes, +2.2%).

### Findings on the way

1. **XPPAUT passes CVODE the tolerances reversed** (docs/xppaut-findings.md
   33): `@ toler` is CVODE's absolute tolerance and `@ atoler` its relative
   one, the opposite of XPPAUT's manual and of ours. On an oscillator of
   amplitude 1e6, `tol=1e-3, atol=1e-10` takes 3769 steps (relative 1e-10),
   `tol=1e-10, atol=1e-3` takes 537 (relative 1e-3), the two CVODEs alike.
   Kept as it is: fixing it changes every CVODE result of a model that sets
   the two apart (the five examples; most models set them equal). The
   adapter keeps the mapping.
2. The vendored cvode.h says `mxstep` defaults to 500; the code uses 2000.
3. `examples/ode/candelator.ode` ends line 16 inside `f(t)=if(t<...`: it does
   not load in either build.

## What switching would cost

- **Build.** SUNDIALS is configured by CMake, which generates
  `sundials_config.h`, `sundials_export.h` and the version header; our Make
  build has no CMake step (WSL, MSYS2 UCRT, clang64 and macOS CI would each
  need it, or a hand-written fixed configuration kept in step with each
  SUNDIALS release). Compiling only what CVODE needs is possible without it:
  18 files / 14,200 lines of CVODE, the serial vector (2,050), the dense and
  band matrices and solvers (1,400), the Newton solver (540) and the core
  (45 files / 11,200 lines, much of it GPU, MPI, logging and profiling code
  CVODE never reaches), about 29,400 lines in all against the 6,750 lines of
  the vendored CVODE it replaces (4,180 in the seven `.cpp` files).
- **Vendoring rule** (outside code copied verbatim with an adapter header):
  doable; the adapter is `cv_sundials.cpp` (160 lines) and the hand-written
  configuration header. BSD-3 needs its licence file in `third_party/` and a
  line in the About text. Each SUNDIALS release would be a re-vendoring.
- **Numerics (W159).** SUNDIALS calls the C library's `pow` (the macro
  `SUNRpowerR`, in cvode.c's step-size formulas); `tools/mathcheck.sh`
  forbids that, and the same bits on every platform need `xpp::math::pow`:
  compile cvode.c with `-Dpow=<our bridge>` and a one-line C shim, or patch
  the source (against "verbatim"). The measurements above used libm's `pow`
  and matched the vendored code on this machine; that is not guaranteed on
  another. Everything else it uses (`sqrt`, `fabs`) is exact.
- **`extern "C"`.** All seven of externcheck's entries go (band.h,
  cvband.h, cvdense.h, cvode.h, dense.h, llnlmath.h, vector.h), with their
  entries in dupcheck.py and aliascheck.py; SUNDIALS' own C headers would
  live in `third_party/`, which externcheck does not scan.
- **Baselines.** The md5s of atcoaster, fieldnoy, itoy, toy_ok and waterwheel
  change (kuramot100 uses CVODE only with a flag and is unaffected), once,
  for every platform. Users' CVODE results would change in the last digits,
  and for sensitive models by a phase of O(1), as the tables show.
- **Size.** +135 KB in the program.
- **Not needed by xppautX today, which SUNDIALS would give**: root finding
  (`CVodeRootInit`: events and the Poincare section located inside the
  integrator's step, with its own interpolation, instead of on stored rows,
  the 1.2e-3 above), the Adams method, sparse (KLU) and Krylov linear
  solvers for systems of thousands of equations, forward and adjoint
  sensitivities (CVODES, for continuation and fitting), a stop time,
  recoverable right-hand side failures, and a maintained code base (the
  vendored one is the 1990s code with xppautX's own patches).

## Recommendation

Keep the vendored CVODE. The two agree on accuracy, steps and failures, the
vendored one is a little faster per step and smaller, and it needs no build
step or libm bridge; a switch would rewrite five baselines and change users'
CVODE results for nothing a user can see.

Take SUNDIALS when a card needs what it has: root finding for events and
the Poincare map (the best case: a 1.2e-3 section error no tolerance
removes), or sparse and Krylov solvers for large networks. That card would
vendor the needed 29,000 lines under `third_party/sundials/` with a
hand-written configuration header, route `pow` through `xpp::math`, rewrite
the five baselines, and delete the seven vendored files.

The `cv_backend.h` split on this branch is independent of that decision: it
leaves the default build's numbers unchanged (md5s verified), takes the
`ropt`/`iopt` arrays out of `CvodeRun` and gives each CVODE integration a
DEBUG line of its counts; it is also the seam through which a later switch is
one file. It can be merged on its own; `cv_sundials.cpp`, the Makefile switch
and the harness are the evaluation, to delete or keep as the maintainer
decides. Reproduce: build SUNDIALS as above, then
`make BUILDDIR=build/sundials SUNDIALS=$HOME/sundials-install build/sundials/xppautX`,
`python3 tools/sundials_eval.py --vendored ./xppautX --sundials
build/sundials/xppautX --out results` and `python3 tools/sundials_report.py
results`.

## Appendix: every table

Backend `vendored` / `sundials`; the `ref_` rows are the tight runs; "error" is explained under Method. The times of the first part come from the 5-repeat run while other jobs shared the machine; the second part (`osc1e6`, which of `toler` and `atoler` is relative) and the third (timing, 7 repeats, the heavy-job lock held) are their own runs.

### osc

Reference: exact (the aux columns, double precision).

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| tol1e-10 | sundials | 1001 | 1.41e-07 | 1.27e-07 | 3608 | 3919 | 61 | 195 | 0 | 0 | 0.032 | 0.033 |
| tol1e-10 | vendored | 1001 | 1.41e-07 | 1.27e-07 | 3608 | 3919 | 61 | 195 | 0 | 0 | 0.032 | 0.033 |
| tol1e-3 | sundials | 1001 | 1.14e-01 | 5.80e-02 | 359 | 397 | 7 | 23 | 0 | 0 | 0.031 | 0.031 |
| tol1e-3 | vendored | 1001 | 1.14e-01 | 5.80e-02 | 359 | 397 | 7 | 23 | 0 | 0 | 0.030 | 0.031 |
| tol1e-4 | sundials | 1001 | 4.56e-02 | 3.97e-02 | 669 | 730 | 12 | 40 | 0 | 0 | 0.031 | 0.031 |
| tol1e-4 | vendored | 1001 | 4.56e-02 | 3.97e-02 | 669 | 730 | 12 | 40 | 0 | 0 | 0.031 | 0.031 |
| tol1e-6 | sundials | 1001 | 2.38e-04 | 2.23e-04 | 836 | 913 | 14 | 52 | 0 | 0 | 0.031 | 0.031 |
| tol1e-6 | vendored | 1001 | 2.38e-04 | 2.23e-04 | 836 | 913 | 14 | 52 | 0 | 0 | 0.031 | 0.031 |
| tol1e-8 | sundials | 1001 | 3.95e-06 | 3.39e-06 | 1860 | 1935 | 31 | 108 | 1 | 0 | 0.031 | 0.032 |
| tol1e-8 | vendored | 1001 | 3.95e-06 | 3.39e-06 | 1860 | 1935 | 31 | 108 | 1 | 0 | 0.031 | 0.031 |
| ref_sundials | sundials | 1001 | 1.10e-10 | 1.02e-10 | 15004 | 15520 | 251 | 771 | 2 | 0 | 0.039 | 0.039 |
| ref_vendored | vendored | 1001 | 1.10e-10 | 1.06e-10 | 14994 | 15510 | 251 | 771 | 2 | 0 | 0.037 | 0.038 |
| ref_dp83 | vendored | 1001 | 5.40e-14 | 2.98e-14 | 0 | 0 | 0 | 0 | 0 | 0 | 0.030 | 0.031 |

### vdp

Reference: scipy DOP853 1e-13. The tight runs against it (trajectory error): sundials 5.01e-08; vendored 5.01e-08; dp83 5.01e-08;

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| tol1e-10 | sundials | 1001 | 4.00e-07 | 2.74e-07 | 10502 | 11993 | 181 | 682 | 123 | 0 | 0.036 | 0.037 |
| tol1e-10 | vendored | 1001 | 4.00e-07 | 2.74e-07 | 10502 | 11993 | 181 | 682 | 123 | 0 | 0.035 | 0.035 |
| tol1e-3 | sundials | 1001 | 1.13e-01 | 5.09e-02 | 832 | 1256 | 16 | 135 | 72 | 0 | 0.029 | 0.030 |
| tol1e-3 | vendored | 1001 | 1.44e-01 | 7.72e-02 | 858 | 1291 | 16 | 140 | 73 | 0 | 0.029 | 0.030 |
| tol1e-4 | sundials | 1001 | 1.13e-02 | 7.24e-03 | 1155 | 1701 | 22 | 187 | 103 | 0 | 0.030 | 0.030 |
| tol1e-4 | vendored | 1001 | 2.65e-03 | 3.71e-04 | 1143 | 1640 | 21 | 165 | 92 | 0 | 0.030 | 0.031 |
| tol1e-6 | sundials | 1001 | 1.10e-04 | 3.56e-05 | 2423 | 3029 | 43 | 230 | 112 | 0 | 0.031 | 0.031 |
| tol1e-6 | vendored | 1001 | 1.10e-04 | 3.56e-05 | 2423 | 3029 | 43 | 230 | 112 | 0 | 0.030 | 0.031 |
| tol1e-8 | sundials | 1001 | 1.61e-05 | 1.05e-05 | 5115 | 5998 | 90 | 431 | 120 | 0 | 0.033 | 0.034 |
| tol1e-8 | vendored | 1001 | 1.61e-05 | 1.05e-05 | 5115 | 5998 | 90 | 431 | 120 | 0 | 0.032 | 0.033 |
| ref_sundials | sundials | 1001 | 5.01e-08 | 3.67e-08 | 33849 | 37259 | 561 | 1814 | 114 | 0 | 0.051 | 0.051 |
| ref_vendored | vendored | 1001 | 5.01e-08 | 3.67e-08 | 33849 | 37259 | 561 | 1814 | 114 | 0 | 0.048 | 0.048 |
| ref_dp83 | vendored | 1001 | 5.01e-08 | 3.67e-08 | 0 | 0 | 0 | 0 | 0 | 0 | 0.032 | 0.033 |

### pr

Reference: exact (the aux columns, double precision).

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| tol1e-10 | sundials | 401 | 5.35e-11 | 2.70e-12 | 672 | 814 | 12 | 74 | 23 | 0 | 0.028 | 0.028 |
| tol1e-10 | vendored | 401 | 5.35e-11 | 9.77e-12 | 636 | 755 | 12 | 61 | 15 | 0 | 0.028 | 0.029 |
| tol1e-3 | sundials | 401 | 6.93e-04 | 3.92e-04 | 74 | 121 | 2 | 22 | 9 | 0 | 0.028 | 0.029 |
| tol1e-3 | vendored | 401 | 6.93e-04 | 3.92e-04 | 74 | 121 | 2 | 22 | 9 | 0 | 0.028 | 0.029 |
| tol1e-4 | sundials | 401 | 6.29e-05 | 5.63e-06 | 91 | 123 | 2 | 16 | 5 | 0 | 0.028 | 0.029 |
| tol1e-4 | vendored | 401 | 6.29e-05 | 5.63e-06 | 91 | 123 | 2 | 16 | 5 | 0 | 0.028 | 0.028 |
| tol1e-6 | sundials | 401 | 9.01e-07 | 5.97e-08 | 185 | 250 | 3 | 34 | 12 | 0 | 0.028 | 0.029 |
| tol1e-6 | vendored | 401 | 9.01e-07 | 6.01e-08 | 185 | 250 | 3 | 34 | 12 | 0 | 0.029 | 0.029 |
| tol1e-8 | sundials | 401 | 7.13e-09 | 9.13e-10 | 339 | 415 | 6 | 42 | 12 | 0 | 0.029 | 0.029 |
| tol1e-8 | vendored | 401 | 9.50e-09 | 9.38e-10 | 352 | 440 | 6 | 48 | 16 | 0 | 0.028 | 0.029 |
| ref_sundials | sundials | 401 | 1.09e-13 | 1.44e-14 | 2031 | 2255 | 34 | 138 | 16 | 0 | 0.031 | 0.031 |
| ref_vendored | vendored | 401 | 1.02e-13 | 2.18e-14 | 1971 | 2218 | 34 | 150 | 22 | 0 | 0.029 | 0.030 |

### robertson

Reference: scipy Radau 1e-12. The tight runs against it (trajectory error): sundials 2.18e-08; vendored 2.18e-08;

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| tol1e-10 | sundials | 2001 | 2.38e-08 | 6.57e-09 | 881 | 1467 | 17 | 175 | 86 | 0 | 0.033 | 0.033 |
| tol1e-10 | vendored | 2001 | 2.67e-08 | 1.03e-08 | 816 | 1354 | 15 | 150 | 69 | 0 | 0.032 | 0.033 |
| tol1e-3 | sundials | 1 | exit 1, 1 rows | exit 1, 1 rows | 2000 | 4262 | 69 | 523 | 485 | 32 | 0.032 | 0.033 |
| tol1e-3 | vendored | 1 | exit 1, 1 rows | exit 1, 1 rows | 2000 | 4262 | 69 | 523 | 485 | 9 | 0.031 | 0.032 |
| tol1e-4 | sundials | 1 | exit 1, 1 rows | exit 1, 1 rows | 2000 | 3418 | 102 | 387 | 286 | 69 | 0.030 | 0.031 |
| tol1e-4 | vendored | 1 | exit 1, 1 rows | exit 1, 1 rows | 2000 | 3418 | 102 | 387 | 286 | 10 | 0.030 | 0.031 |
| tol1e-6 | sundials | 2001 | 1.43e-05 | 3.56e-06 | 264 | 492 | 7 | 55 | 16 | 3 | 0.032 | 0.033 |
| tol1e-6 | vendored | 2001 | 1.37e-05 | 9.53e-07 | 255 | 483 | 7 | 56 | 16 | 0 | 0.032 | 0.033 |
| tol1e-8 | sundials | 2001 | 2.22e-07 | 2.86e-08 | 527 | 959 | 11 | 114 | 47 | 4 | 0.032 | 0.033 |
| tol1e-8 | vendored | 2001 | 2.85e-07 | 2.13e-07 | 557 | 1010 | 12 | 125 | 55 | 0 | 0.032 | 0.032 |
| ref_sundials | sundials | 2001 | 2.18e-08 | 6.57e-09 | 1803 | 2314 | 32 | 203 | 72 | 0 | 0.033 | 0.034 |
| ref_vendored | vendored | 2001 | 2.18e-08 | 6.57e-09 | 1737 | 2265 | 31 | 188 | 63 | 0 | 0.032 | 0.033 |

### heat

Reference: exact (the aux columns, double precision).

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| tol1e-10 | sundials | 101 | 6.43e-10 | 1.22e-11 | 270 | 818 | 5 | 40 | 8 | 0 | 0.054 | 0.054 |
| tol1e-10 | vendored | 101 | 6.43e-10 | 1.22e-11 | 270 | 818 | 5 | 40 | 8 | 0 | 0.055 | 0.056 |
| tol1e-3 | sundials | 101 | 1.30e-03 | 8.51e-06 | 28 | 137 | 1 | 10 | 0 | 0 | 0.049 | 0.050 |
| tol1e-3 | vendored | 101 | 1.30e-03 | 8.51e-06 | 28 | 137 | 1 | 10 | 0 | 0 | 0.050 | 0.050 |
| tol1e-4 | sundials | 101 | 1.84e-04 | 3.82e-06 | 43 | 161 | 1 | 14 | 2 | 0 | 0.049 | 0.050 |
| tol1e-4 | vendored | 101 | 1.84e-04 | 3.82e-06 | 43 | 161 | 1 | 14 | 2 | 0 | 0.050 | 0.051 |
| tol1e-6 | sundials | 101 | 2.67e-06 | 1.97e-08 | 78 | 317 | 2 | 25 | 8 | 0 | 0.051 | 0.051 |
| tol1e-6 | vendored | 101 | 2.67e-06 | 1.97e-08 | 78 | 317 | 2 | 25 | 8 | 0 | 0.050 | 0.051 |
| tol1e-8 | sundials | 101 | 3.12e-08 | 7.95e-10 | 154 | 494 | 3 | 28 | 6 | 0 | 0.052 | 0.053 |
| tol1e-8 | vendored | 101 | 3.12e-08 | 7.95e-10 | 154 | 494 | 3 | 28 | 6 | 0 | 0.053 | 0.053 |
| ref_sundials | sundials | 101 | 1.90e-12 | 1.25e-13 | 785 | 2242 | 14 | 70 | 8 | 0 | 0.065 | 0.066 |
| ref_vendored | vendored | 101 | 1.90e-12 | 1.22e-13 | 785 | 2242 | 14 | 70 | 8 | 0 | 0.069 | 0.069 |

### heatband

Reference: exact (the aux columns, double precision).

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| tol1e-10 | sundials | 101 | 6.43e-10 | 1.22e-11 | 270 | 333 | 5 | 40 | 8 | 0 | 0.050 | 0.051 |
| tol1e-10 | vendored | 101 | 6.43e-10 | 1.22e-11 | 270 | 333 | 5 | 40 | 8 | 0 | 0.051 | 0.051 |
| tol1e-3 | sundials | 101 | 1.30e-03 | 8.51e-06 | 28 | 40 | 1 | 10 | 0 | 0 | 0.048 | 0.048 |
| tol1e-3 | vendored | 101 | 1.30e-03 | 8.51e-06 | 28 | 40 | 1 | 10 | 0 | 0 | 0.048 | 0.048 |
| tol1e-4 | sundials | 101 | 1.84e-04 | 3.82e-06 | 43 | 64 | 1 | 14 | 2 | 0 | 0.048 | 0.048 |
| tol1e-4 | vendored | 101 | 1.84e-04 | 3.82e-06 | 43 | 64 | 1 | 14 | 2 | 0 | 0.048 | 0.049 |
| tol1e-6 | sundials | 101 | 2.67e-06 | 1.97e-08 | 78 | 123 | 2 | 25 | 8 | 0 | 0.048 | 0.049 |
| tol1e-6 | vendored | 101 | 2.67e-06 | 1.97e-08 | 78 | 123 | 2 | 25 | 8 | 0 | 0.048 | 0.049 |
| tol1e-8 | sundials | 101 | 3.12e-08 | 7.95e-10 | 154 | 203 | 3 | 28 | 6 | 0 | 0.050 | 0.050 |
| tol1e-8 | vendored | 101 | 3.12e-08 | 7.95e-10 | 154 | 203 | 3 | 28 | 6 | 0 | 0.050 | 0.050 |
| ref_sundials | sundials | 101 | 1.90e-12 | 1.25e-13 | 785 | 884 | 14 | 70 | 8 | 0 | 0.055 | 0.056 |
| ref_vendored | vendored | 101 | 1.90e-12 | 1.22e-13 | 785 | 884 | 14 | 70 | 8 | 0 | 0.056 | 0.056 |

### brus

Reference: sundials tight. The tight runs against it (trajectory error): sundials 0; vendored 9.88e-08;

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| tol1e-6 | sundials | 601 | 9.18e-04 | 3.79e-05 | 1328 | 4117 | 24 | 162 | 75 | 0 | 0.130 | 0.131 |
| tol1e-6 | vendored | 601 | 8.38e-04 | 3.08e-05 | 1298 | 4080 | 24 | 162 | 74 | 0 | 0.139 | 0.139 |
| tol1e-8 | sundials | 601 | 4.11e-05 | 1.48e-06 | 2677 | 7787 | 47 | 217 | 80 | 0 | 0.189 | 0.190 |
| tol1e-8 | vendored | 601 | 4.07e-05 | 1.48e-06 | 2671 | 7783 | 47 | 216 | 81 | 0 | 0.201 | 0.202 |
| ref_sundials | sundials | 601 | 0 | 0 | 8372 | 22919 | 139 | 505 | 78 | 0 | 0.428 | 0.429 |
| ref_vendored | vendored | 601 | 9.88e-08 | 6.02e-08 | 8335 | 22870 | 139 | 497 | 74 | 0 | 0.462 | 0.463 |

### vdpstiff

Reference: sundials tight. The tight runs against it (trajectory error): sundials 0; vendored 1.18e-06;

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| tol1e-6 | sundials | 3001 | 4.62e+01 | 1.63e-04 | 1418 | 2075 | 33 | 239 | 118 | 13 | 0.041 | 0.041 |
| tol1e-6 | vendored | 3001 | 4.62e+01 | 1.47e-04 | 1396 | 2070 | 33 | 236 | 121 | 0 | 0.032 | 0.033 |
| tol1e-8 | sundials | 3001 | 3.04e-03 | 2.03e-06 | 2962 | 4261 | 56 | 455 | 230 | 11 | 0.034 | 0.035 |
| tol1e-8 | vendored | 3001 | 3.04e-03 | 2.75e-06 | 2864 | 4127 | 55 | 422 | 209 | 0 | 0.033 | 0.034 |
| ref_sundials | sundials | 3001 | 0 | 0 | 8774 | 11225 | 152 | 954 | 427 | 0 | 0.038 | 0.039 |
| ref_vendored | vendored | 3001 | 1.18e-06 | 0 | 8735 | 11120 | 149 | 927 | 399 | 0 | 0.037 | 0.037 |

### delay

Reference: dp83 tight. The tight runs against it (trajectory error): dp83 0;

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| tol1e-10 | sundials | 20 | exit 1, 20 rows | exit 1, 20 rows | 69 | 112 | 2 | 28 | 12 | 0 | 0.029 | 0.029 |
| tol1e-10 | vendored | 20 | exit 1, 20 rows | exit 1, 20 rows | 69 | 112 | 2 | 28 | 12 | 0 | 0.028 | 0.028 |
| tol1e-3 | sundials | 601 | 2.64e+00 | 1.21e-01 | 408 | 653 | 9 | 102 | 47 | 3 | 0.028 | 0.029 |
| tol1e-3 | vendored | 601 | 2.64e+00 | 4.09e-02 | 440 | 679 | 9 | 99 | 45 | 0 | 0.028 | 0.029 |
| tol1e-4 | sundials | 601 | 1.74e+00 | 1.51e+00 | 1898 | 3175 | 38 | 735 | 344 | 5 | 0.030 | 0.030 |
| tol1e-4 | vendored | 601 | 1.49e+00 | 1.10e+00 | 1978 | 3491 | 39 | 909 | 420 | 0 | 0.029 | 0.030 |
| tol1e-6 | sundials | 601 | 6.76e-01 | 2.33e-01 | 7720 | 13977 | 154 | 5186 | 1864 | 9 | 0.033 | 0.035 |
| tol1e-6 | vendored | 601 | 6.28e-01 | 2.12e-01 | 7711 | 13984 | 153 | 5191 | 1867 | 0 | 0.033 | 0.033 |
| tol1e-8 | sundials | 601 | 3.97e-01 | 9.95e-02 | 13552 | 24496 | 264 | 9561 | 3516 | 19 | 0.039 | 0.039 |
| tol1e-8 | vendored | 601 | 4.15e-01 | 1.06e-01 | 13548 | 24455 | 264 | 9529 | 3498 | 0 | 0.037 | 0.037 |
| ref_sundials | sundials | 20 | exit 1, 20 rows | exit 1, 20 rows | 180 | 224 | 5 | 33 | 10 | 1 | 0.028 | 0.029 |
| ref_vendored | vendored | 20 | exit 1, 20 rows | exit 1, 20 rows | 180 | 224 | 5 | 33 | 10 | 0 | 0.029 | 0.029 |
| ref_dp83 | vendored | 601 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0.029 | 0.030 |

### ball

Reference: exact. The tight runs against it (trajectory error): sundials 1.32e-03; vendored 1.32e-03; dp83 1.32e-03;

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| tol1e-10 | sundials | 801 | 1.32e-03 | 3.79e-04 | 130 | 157 | 5 | 130 | 0 | 0 | 0.028 | 0.029 |
| tol1e-10 | vendored | 801 | 1.32e-03 | 3.79e-04 | 130 | 157 | 5 | 130 | 0 | 0 | 0.028 | 0.029 |
| tol1e-3 | sundials | 801 | 3.20e-02 | 9.56e-03 | 56 | 90 | 5 | 49 | 2 | 0 | 0.029 | 0.029 |
| tol1e-3 | vendored | 801 | 3.20e-02 | 9.56e-03 | 56 | 90 | 5 | 49 | 2 | 0 | 0.030 | 0.030 |
| tol1e-4 | sundials | 801 | 4.16e-03 | 1.28e-03 | 67 | 97 | 5 | 59 | 0 | 0 | 0.029 | 0.030 |
| tol1e-4 | vendored | 801 | 4.16e-03 | 1.28e-03 | 67 | 97 | 5 | 59 | 0 | 0 | 0.029 | 0.030 |
| tol1e-6 | sundials | 801 | 1.35e-03 | 3.88e-04 | 87 | 112 | 5 | 85 | 0 | 0 | 0.030 | 0.030 |
| tol1e-6 | vendored | 801 | 1.35e-03 | 3.88e-04 | 87 | 112 | 5 | 85 | 0 | 0 | 0.029 | 0.030 |
| tol1e-8 | sundials | 801 | 1.32e-03 | 3.79e-04 | 107 | 132 | 5 | 107 | 0 | 0 | 0.029 | 0.029 |
| tol1e-8 | vendored | 801 | 1.32e-03 | 3.79e-04 | 107 | 132 | 5 | 107 | 0 | 0 | 0.030 | 0.030 |
| ref_sundials | sundials | 801 | 1.32e-03 | 3.79e-04 | 161 | 179 | 5 | 161 | 0 | 0 | 0.028 | 0.029 |
| ref_vendored | vendored | 801 | 1.32e-03 | 3.79e-04 | 161 | 179 | 5 | 161 | 0 | 0 | 0.028 | 0.029 |
| ref_dp83 | vendored | 801 | 1.32e-03 | 3.79e-04 | 0 | 0 | 0 | 0 | 0 | 0 | 0.029 | 0.029 |

### poincare

Reference: exact, crossings at t = 3 pi/2 + 2 pi k, y = 1 (error: the larger of the crossing time and y).

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| tol1e-10 | sundials | 16 | 1.24e-03 | 9.59e-06 | 3608 | 3919 | 61 | 195 | 0 | 0 | 0.031 | 0.031 |
| tol1e-10 | vendored | 16 | 1.24e-03 | 9.59e-06 | 3608 | 3919 | 61 | 195 | 0 | 0 | 0.030 | 0.030 |
| tol1e-3 | sundials | 16 | 1.10e-01 | 1.10e-01 | 359 | 397 | 7 | 23 | 0 | 0 | 0.029 | 0.029 |
| tol1e-3 | vendored | 16 | 1.10e-01 | 1.10e-01 | 359 | 397 | 7 | 23 | 0 | 0 | 0.028 | 0.029 |
| tol1e-4 | sundials | 16 | 8.66e-02 | 1.58e-02 | 669 | 730 | 12 | 40 | 0 | 0 | 0.030 | 0.030 |
| tol1e-4 | vendored | 16 | 8.66e-02 | 1.58e-02 | 669 | 730 | 12 | 40 | 0 | 0 | 0.028 | 0.028 |
| tol1e-6 | sundials | 16 | 1.63e-03 | 1.39e-04 | 836 | 913 | 14 | 52 | 0 | 0 | 0.029 | 0.030 |
| tol1e-6 | vendored | 16 | 1.63e-03 | 1.39e-04 | 836 | 913 | 14 | 52 | 0 | 0 | 0.028 | 0.029 |
| tol1e-8 | sundials | 16 | 1.24e-03 | 9.59e-06 | 1860 | 1935 | 31 | 108 | 1 | 0 | 0.030 | 0.030 |
| tol1e-8 | vendored | 16 | 1.24e-03 | 9.59e-06 | 1860 | 1935 | 31 | 108 | 1 | 0 | 0.029 | 0.030 |
| ref_sundials | sundials | 16 | 1.24e-03 | 9.59e-06 | 15004 | 15520 | 251 | 771 | 2 | 0 | 0.037 | 0.038 |
| ref_vendored | vendored | 16 | 1.24e-03 | 9.59e-06 | 14994 | 15510 | 251 | 771 | 2 | 0 | 0.036 | 0.036 |
| ref_dp83 | vendored | 16 | 1.24e-03 | 9.59e-06 | 0 | 0 | 0 | 0 | 0 | 0 | 0.028 | 0.029 |

### blowup

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| asis | sundials | 21 | exit 1, 21 rows | exit 1, 21 rows | 0 | 0 | 0 | 0 | 0 | 0 | 0.028 | 0.028 |
| asis | vendored | 21 | exit 1, 21 rows | exit 1, 21 rows | 0 | 0 | 0 | 0 | 0 | 0 | 0.029 | 0.030 |
| ref_sundials | sundials | 20 | exit 1, 20 rows | exit 1, 20 rows | 2981 | 3341 | 51 | 175 | 36 | 0 | 0.030 | 0.030 |
| ref_vendored | vendored | 20 | exit 1, 20 rows | exit 1, 20 rows | 2981 | 3341 | 51 | 175 | 36 | 0 | 0.030 | 0.030 |
| ref_dp83 | vendored | 20 | exit 1, 20 rows | exit 1, 20 rows | 0 | 0 | 0 | 0 | 0 | 0 | 0.029 | 0.030 |

asis_sundials: integration failed: X out of bounds at t = 1.0500000000000003

asis_vendored: integration failed: X out of bounds at t = 1.0500000000000003

### candelator

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| asis | sundials | 0 | exit 1, 0 rows | exit 1, 0 rows | 0 | 0 | 0 | 0 | 0 | 0 | 0.029 | 0.030 |
| asis | vendored | 0 | exit 1, 0 rows | exit 1, 0 rows | 0 | 0 | 0 | 0 | 0 | 0 | 0.028 | 0.028 |
| ref_sundials | sundials | 0 | exit 1, 0 rows | exit 1, 0 rows | 0 | 0 | 0 | 0 | 0 | 0 | 0.031 | 0.032 |
| ref_vendored | vendored | 0 | exit 1, 0 rows | exit 1, 0 rows | 0 | 0 | 0 | 0 | 0 | 0 | 0.028 | 0.028 |
| ref_dp83 | vendored | 0 | exit 1, 0 rows | exit 1, 0 rows | 0 | 0 | 0 | 0 | 0 | 0 | 0.028 | 0.029 |

### atcoaster

Reference: dp83 tight. The tight runs against it (trajectory error): sundials 8.02e-08; vendored 8.02e-08; dp83 0;

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| asis | sundials | 1001 | 1.49e-07 | 2.46e-08 | 2474 | 2805 | 42 | 141 | 9 | 0 | 0.044 | 0.044 |
| asis | vendored | 1001 | 1.49e-07 | 2.46e-08 | 2474 | 2805 | 42 | 141 | 9 | 0 | 0.043 | 0.044 |
| ref_sundials | sundials | 1001 | 8.02e-08 | 0 | 7742 | 8686 | 130 | 418 | 13 | 0 | 0.069 | 0.070 |
| ref_vendored | vendored | 1001 | 8.02e-08 | 0 | 7742 | 8686 | 130 | 418 | 13 | 0 | 0.068 | 0.069 |
| ref_dp83 | vendored | 1001 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0.081 | 0.081 |

### fieldnoy

Reference: dp83 tight. The tight runs against it (trajectory error): dp83 0;

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| asis | sundials | 2001 | 4.54e-03 | 9.12e-07 | 6596 | 10588 | 143 | 1105 | 570 | 54 | 0.042 | 0.042 |
| asis | vendored | 2001 | 3.37e-04 | 4.23e-07 | 6676 | 10608 | 141 | 1094 | 558 | 0 | 0.040 | 0.040 |
| ref_sundials | sundials | 7 | exit 1, 7 rows | exit 1, 7 rows | 4920 | 5541 | 83 | 293 | 36 | 0 | 0.034 | 0.034 |
| ref_vendored | vendored | 7 | exit 1, 7 rows | exit 1, 7 rows | 4907 | 5542 | 81 | 292 | 35 | 0 | 0.034 | 0.034 |
| ref_dp83 | vendored | 2001 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 16.235 | 16.237 |

### itoy

Reference: dp83 tight. The tight runs against it (trajectory error): sundials 1.44e-06; vendored 1.52e-06; dp83 0;

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| asis | sundials | 2001 | 1.73e+00 | 7.31e-01 | 9510 | 13493 | 169 | 1324 | 675 | 3 | 0.287 | 0.288 |
| asis | vendored | 2001 | 1.67e+00 | 3.36e-01 | 9563 | 13581 | 170 | 1339 | 682 | 0 | 0.294 | 0.295 |
| ref_sundials | sundials | 2001 | 1.44e-06 | 4.25e-07 | 232250 | 262343 | 3864 | 12448 | 926 | 0 | 4.331 | 4.332 |
| ref_vendored | vendored | 2001 | 1.52e-06 | 4.52e-07 | 231767 | 261842 | 3855 | 12403 | 903 | 0 | 4.356 | 4.357 |
| ref_dp83 | vendored | 2001 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 3.288 | 3.288 |

### toy_ok

Reference: dp83 tight. The tight runs against it (trajectory error): sundials 2.34e+00; vendored 2.12e+00; dp83 0;

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| asis | sundials | 1201 | 4.72e+00 | 1.02e+00 | 15779 | 23815 | 290 | 2808 | 1464 | 13 | 0.444 | 0.445 |
| asis | vendored | 1201 | 3.17e+00 | 9.88e-01 | 12523 | 19044 | 230 | 2265 | 1179 | 0 | 0.366 | 0.366 |
| ref_sundials | sundials | 1201 | 2.34e+00 | 4.78e-01 | 333310 | 375430 | 5553 | 18046 | 1542 | 0 | 6.184 | 6.184 |
| ref_vendored | vendored | 1201 | 2.12e+00 | 1.15e+00 | 298039 | 335809 | 4966 | 16150 | 1384 | 0 | 5.674 | 5.675 |
| ref_dp83 | vendored | 1201 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 5.159 | 5.160 |

### waterwheel

Reference: dp83 tight. The tight runs against it (trajectory error): sundials 1.88e+00; vendored 2.01e+00; dp83 0;

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| asis | sundials | 4001 | 2.06e+00 | 1.74e+00 | 5400 | 11251 | 105 | 3514 | 1569 | 2 | 0.155 | 0.155 |
| asis | vendored | 4001 | 4.87e+00 | 4.63e+00 | 5402 | 11095 | 105 | 3396 | 1501 | 0 | 0.155 | 0.155 |
| ref_sundials | sundials | 4001 | 1.88e+00 | 1.86e+00 | 48769 | 84110 | 894 | 17626 | 7040 | 64 | 0.981 | 0.982 |
| ref_vendored | vendored | 4001 | 2.01e+00 | 1.95e+00 | 48197 | 82821 | 883 | 17372 | 6908 | 0 | 0.982 | 0.982 |
| ref_dp83 | vendored | 4001 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 2.230 | 2.230 |

### kuramot100

Reference: dp83 tight. The tight runs against it (trajectory error): sundials 2.19e-08; vendored 2.19e-08; dp83 0;

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| asis | sundials | 2001 | 3.70e-03 | 5.54e-05 | 29 | 238 | 2 | 14 | 0 | 1 | 0.456 | 0.457 |
| asis | vendored | 2001 | 3.70e-03 | 5.54e-05 | 29 | 238 | 2 | 14 | 0 | 0 | 0.432 | 0.432 |
| ref_sundials | sundials | 2001 | 2.19e-08 | 0 | 803 | 2247 | 14 | 75 | 5 | 0 | 3.438 | 3.439 |
| ref_vendored | vendored | 2001 | 2.19e-08 | 0 | 803 | 2247 | 14 | 75 | 5 | 0 | 5.101 | 5.102 |
| ref_dp83 | vendored | 2001 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 35.721 | 35.722 |

### osc1e6 (toler and atoler apart)


Reference: exact (the aux columns, double precision).

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| toler1e-10_atoler1e-3 | sundials | 1001 | 2.10e+01 | 2.84e-01 | 537 | 661 | 10 | 64 | 15 | 0 | 0.031 | 0.031 |
| toler1e-10_atoler1e-3 | vendored | 1001 | 2.10e+01 | 2.84e-01 | 537 | 661 | 10 | 64 | 15 | 0 | 0.031 | 0.031 |
| toler1e-3_atoler1e-10 | sundials | 1001 | 1.22e-05 | 2.57e-07 | 3769 | 3903 | 63 | 202 | 0 | 0 | 0.037 | 0.037 |
| toler1e-3_atoler1e-10 | vendored | 1001 | 1.22e-05 | 2.57e-07 | 3769 | 3903 | 63 | 202 | 0 | 0 | 0.035 | 0.035 |


### Timing pass (no reference runs)

### osclong

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| tol1e-6 | sundials | 2001 | - | - | 162836 | 176412 | 2714 | 8151 | 0 | 0 | 0.121 | 0.122 |
| tol1e-6 | vendored | 2001 | - | - | 162836 | 176412 | 2714 | 8151 | 0 | 0 | 0.104 | 0.105 |
| tol1e-8 | sundials | 2001 | - | - | 366961 | 379206 | 6116 | 18363 | 1 | 0 | 0.235 | 0.235 |
| tol1e-8 | vendored | 2001 | - | - | 366961 | 379206 | 6116 | 18363 | 1 | 0 | 0.196 | 0.197 |

### brus

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| tol1e-6 | sundials | 601 | - | - | 1328 | 4117 | 24 | 162 | 75 | 0 | 0.129 | 0.130 |
| tol1e-6 | vendored | 601 | - | - | 1298 | 4080 | 24 | 162 | 74 | 0 | 0.136 | 0.137 |
| tol1e-8 | sundials | 601 | - | - | 2677 | 7787 | 47 | 217 | 80 | 0 | 0.184 | 0.185 |
| tol1e-8 | vendored | 601 | - | - | 2671 | 7783 | 47 | 216 | 81 | 0 | 0.199 | 0.200 |

### vdpstiff

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| tol1e-6 | sundials | 3001 | - | - | 1418 | 2075 | 33 | 239 | 118 | 13 | 0.031 | 0.032 |
| tol1e-6 | vendored | 3001 | - | - | 1396 | 2070 | 33 | 236 | 121 | 0 | 0.030 | 0.031 |
| tol1e-8 | sundials | 3001 | - | - | 2962 | 4261 | 56 | 455 | 230 | 11 | 0.033 | 0.034 |
| tol1e-8 | vendored | 3001 | - | - | 2864 | 4127 | 55 | 422 | 209 | 0 | 0.031 | 0.032 |

### atcoaster

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| asis | sundials | 1001 | - | - | 2474 | 2805 | 42 | 141 | 9 | 0 | 0.041 | 0.042 |
| asis | vendored | 1001 | - | - | 2474 | 2805 | 42 | 141 | 9 | 0 | 0.041 | 0.042 |

### fieldnoy

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| asis | sundials | 2001 | - | - | 6596 | 10588 | 143 | 1105 | 570 | 54 | 0.040 | 0.041 |
| asis | vendored | 2001 | - | - | 6676 | 10608 | 141 | 1094 | 558 | 0 | 0.038 | 0.038 |

### itoy

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| asis | sundials | 2001 | - | - | 9510 | 13493 | 169 | 1324 | 675 | 3 | 0.283 | 0.284 |
| asis | vendored | 2001 | - | - | 9563 | 13581 | 170 | 1339 | 682 | 0 | 0.283 | 0.283 |

### toy_ok

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| asis | sundials | 1201 | - | - | 15779 | 23815 | 290 | 2808 | 1464 | 13 | 0.447 | 0.448 |
| asis | vendored | 1201 | - | - | 12523 | 19044 | 230 | 2265 | 1179 | 0 | 0.358 | 0.359 |

### waterwheel

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| asis | sundials | 4001 | - | - | 5400 | 11251 | 105 | 3514 | 1569 | 2 | 0.152 | 0.153 |
| asis | vendored | 4001 | - | - | 5402 | 11095 | 105 | 3396 | 1501 | 0 | 0.150 | 0.151 |

### kuramot100

| run | backend | rows | error traj | error final | steps | f evals | Jac evals | setups | err-test fails | conv fails | CPU s | wall s |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| asis | sundials | 2001 | - | - | 29 | 238 | 2 | 14 | 0 | 1 | 0.410 | 0.410 |
| asis | vendored | 2001 | - | - | 29 | 238 | 2 | 14 | 0 | 0 | 0.413 | 0.414 |

