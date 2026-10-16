# W377: adaptive steps across output intervals

Issue: [#439](https://github.com/MuhammadMoustafa/xppautX/issues/439).

The measurement sections through "Gates and review handoff" preserve the stage 1 record.
The production implementation and its validation are documented in
[stage 2 below](#stage-2-production-dense-integration-w377-439).

## Scope and decision

Stage 1 measured a disposable continuous-integration prototype. Stage 2 now
implements one production dense-output path for DoPri5 and DoPri8(3), including
events and resets in the shared integration loop. Dt is the output interval;
DtMax caps internal steps. The two changed DoPri example baselines are recorded
below. Instruction counts remain for the coordinator's WSL/callgrind review;
RHS counts are not instruction counts or a speedup claim.

## Method and reproducibility

The executable uses this worktree's UCRT build, MSYS2 GCC 16.1.0 (Rev5), `-O2`, and
`-ffp-contract=off`. `w377-measure.py` generates an instrumented copy of
`core/dormpri.cpp` in `build/w377`, compiles it separately, and links its object
before the production core archive. It never edits `core/` or replaces the
production executable. All measurements run sequentially through
`tools/heavy.sh`; no timing measurements are made.

The temporary `dprhs` counter counts every actual RHS call, including initial
evaluations, rejected attempts, and DOP853's three additional dense-output
stages. Accepted steps come from `DpRun::naccpt`; rejected attempts are
`nstep-naccpt`, including initial rejections which Hairer's `nrejct` excludes.
The measurement harness is study tooling only: no counter, environment switch,
output file, or extra global is added to the shipped program.

The counters can be independently checked from the stages. Let M be the
number of output intervals, A accepted steps and R rejected attempts. In
these runs the first call initializes automatically and later restarted
calls receive a nonzero step guess. DoPri5 now uses `M+6*(A+R)+1` RHS calls;
continuous uses `2+6*(A+R)`. DoPri8 now uses `M+12*A+11*R+1`; continuous with
dense output uses `2+15*A+11*R`. Thus DoPri8 pays three extra RHS evaluations
per accepted step for dense output. This cost must be included when choosing
between approaches; fewer restart calls alone is insufficient.

For each model and method, **now** calls the unchanged routine for every
scheduled output interval, passing `dp_hout` as the next initial step guess,
exactly as `dormprin` does. **Continuous** calls the same routine once for the
whole horizon, with `iout=2` and dense coefficients for all states. It samples
the existing polynomial at the same output times; the endpoint is the last
whole output interval, not a rounded-up time. Both use scalar relative and
absolute tolerances of `1e-6`, identical initial conditions and parameters,
the existing stiffness checks, and the existing default maximum-step policy.
This compares output scheduling, not the paper's original choice of method.
The local tolerance is not a bound on the global trajectory error or on
the interpolated output error. An output-limited run can substantially
overresolve the requested tolerance, so equal tolerance does not imply
equal observed accuracy.

For each method, two additional continuous runs use `1e-11` and `1e-12`.
The reference is DoPri8 at `1e-12`; the refinement column compares that
reference with DoPri8 at `1e-11`. Errors are maximum absolute differences
over every state at every scheduled time, excluding auxiliaries and time.
These are numerical convergence controls from the same solver family, not an
independent implementation. Long spiking/chaotic trajectories can have large
phase errors despite small local-error tolerances; a poorly converged
reference cannot justify an accuracy claim. The two scalar controls have
analytic references: `exp(-.01*t)` and `cos(t)` respectively.

The paper archive is ignored by git and absent from this worktree. Fixtures
are copied read-only from the main checkout's
`paper/benchmarks/five-model-screen/models/`; the script preserves input
SHA-256 hashes. The episodic model uses its normalized archival SET (parameters
and initial conditions), changing only its method line to DoPri5/DoPri8.
PY_S1Bf uses the screen's documented `Iapp=2000` transformation. The original
screen has mixed solver/tolerance settings; none of its timing numbers is
reused here.

The slow control is `x'=-.01*x`, `x(0)=1`, T=100, dt=.05. The stiff-ish
control is `x'=-20*(x-cos(t))-sin(t)`, `x(0)=1`, T=30, dt=.05. This has a
stable fast transient and a smooth slow exact solution; it is not a claim
that explicit DoPri is suitable for general stiff systems.

Reproduce from Git Bash in this worktree (after the required UCRT build):

```sh
export PATH="/c/msys64/ucrt64/bin:$PATH"
python docs/studies/w377-measure.py fixtures
python docs/studies/w377-measure.py prepare
tools/heavy.sh bash build/w377/build.sh
tools/heavy.sh python docs/studies/w377-measure.py run
python docs/studies/w377-measure.py summarize
```

The generated stage 1 files under ignored `build/w377/` were cleaned after
review; the commands above regenerate them. The compact counts and input
provenance accompany this study in `w377-counts.csv` and `w377-counts.json`.
An initial Kuramoto fixture retained its legacy `d` terminator and therefore
did not enter DoPri. Its eight uninstrumented runs were discarded, the
terminator transformation was corrected, and all eight were rerun. Only
runs with actual solver counters appear in the recorded study.

## Measurements

<!-- W377 BEGIN DATA -->

### lecar

T=30, dt=0.05; reference refinement delta=2.13e-10.

| Method | Path | Tolerance | RHS | Accepted | Rejected | Max error | Status |
|---|---|---:|---:|---:|---:|---:|---|
| dopri5 | now | 1e-06 | 4,201 | 600 | 0 | 9.96e-10 | complete |
| dopri5 | continuous | 1e-06 | 326 | 49 | 5 | 5.77e-06 | complete |
| dopri5 | continuous | 1e-11 | 2,546 | 421 | 3 | 3.99e-10 | complete |
| dopri8 | now | 1e-06 | 7,801 | 600 | 0 | 3.32e-11 | complete |
| dopri8 | continuous | 1e-06 | 380 | 23 | 3 | 1.29e-05 | complete |
| dopri8 | continuous | 1e-11 | 1,404 | 81 | 17 | 2.13e-10 | complete |

### episodic

T=1998.8, dt=0.4; reference refinement delta=0.00396.

| Method | Path | Tolerance | RHS | Accepted | Rejected | Max error | Status |
|---|---|---:|---:|---:|---:|---:|---|
| dopri5 | now | 1e-06 | 112,110 | 16,678 | 1,174 | 134 | complete |
| dopri5 | continuous | 1e-06 | 84,818 | 12,453 | 1,683 | 127 | complete |
| dopri5 | continuous | 1e-11 | 600,008 | 99,337 | 664 | not complete | step cap at 4250 outputs |
| dopri8 | now | 1e-06 | 134,915 | 9,588 | 1,351 | 113 | complete |
| dopri8 | continuous | 1e-06 | 110,215 | 6,029 | 1,798 | 133 | complete |
| dopri8 | continuous | 1e-11 | 448,116 | 26,124 | 5,114 | 0.00396 | complete |

### lecdiff200

T=20, dt=0.05; reference refinement delta=4.58e-07.

| Method | Path | Tolerance | RHS | Accepted | Rejected | Max error | Status |
|---|---|---:|---:|---:|---:|---:|---|
| dopri5 | now | 1e-06 | 2,843 | 407 | 0 | 7.61e-07 | complete |
| dopri5 | continuous | 1e-06 | 794 | 132 | 0 | 1.09e-05 | complete |
| dopri5 | continuous | 1e-11 | 2,192 | 364 | 1 | 6.29e-08 | complete |
| dopri8 | now | 1e-06 | 5,201 | 400 | 0 | 6.29e-08 | complete |
| dopri8 | continuous | 1e-06 | 1,117 | 67 | 10 | 0.000207 | complete |
| dopri8 | continuous | 1e-11 | 1,676 | 94 | 24 | 4.58e-07 | complete |

### kuramot100

T=100, dt=0.05; reference refinement delta=8.36e-08.

| Method | Path | Tolerance | RHS | Accepted | Rejected | Max error | Status |
|---|---|---:|---:|---:|---:|---:|---|
| dopri5 | now | 1e-06 | 14,019 | 2,003 | 0 | 4.78e-09 | complete |
| dopri5 | continuous | 1e-06 | 338 | 54 | 2 | 6.86e-05 | complete |
| dopri5 | continuous | 1e-11 | 1,124 | 185 | 2 | 4.92e-09 | complete |
| dopri8 | now | 1e-06 | 26,049 | 2,004 | 0 | 4.78e-09 | complete |
| dopri8 | continuous | 1e-06 | 481 | 29 | 4 | 0.00262 | complete |
| dopri8 | continuous | 1e-11 | 781 | 49 | 4 | 8.36e-08 | complete |

### amari

T=100, dt=0.05; reference refinement delta=0.000175.

| Method | Path | Tolerance | RHS | Accepted | Rejected | Max error | Status |
|---|---|---:|---:|---:|---:|---:|---|
| dopri5 | now | 1e-06 | 14,091 | 2,004 | 11 | 0.459 | complete |
| dopri5 | continuous | 1e-06 | 386 | 52 | 12 | 0.459 | complete |
| dopri5 | continuous | 1e-11 | 1,352 | 206 | 19 | 0.459 | complete |
| dopri8 | now | 1e-06 | 27,379 | 2,047 | 74 | 0.102 | complete |
| dopri8 | continuous | 1e-06 | 2,473 | 87 | 106 | 0.102 | complete |
| dopri8 | continuous | 1e-11 | 3,767 | 141 | 150 | 0.000175 | complete |

### clustor

T=500, dt=1; reference refinement delta=5e-10.

| Method | Path | Tolerance | RHS | Accepted | Rejected | Max error | Status |
|---|---|---:|---:|---:|---:|---:|---|
| dopri5 | now | 1e-06 | 3,525 | 504 | 0 | 4.84e-09 | complete |
| dopri5 | continuous | 1e-06 | 452 | 67 | 8 | 1.11e-05 | complete |
| dopri5 | continuous | 1e-11 | 1,616 | 264 | 5 | 2.22e-10 | complete |
| dopri8 | now | 1e-06 | 6,585 | 507 | 0 | 6.14e-11 | complete |
| dopri8 | continuous | 1e-06 | 686 | 39 | 9 | 6.02e-05 | complete |
| dopri8 | continuous | 1e-11 | 1,351 | 76 | 19 | 5e-10 | complete |

### slow

T=100, dt=0.05; reference refinement delta=1.97e-11.

| Method | Path | Tolerance | RHS | Accepted | Rejected | Max error | Status |
|---|---|---:|---:|---:|---:|---:|---|
| dopri5 | now | 1e-06 | 14,001 | 2,000 | 0 | 1.33e-15 | complete |
| dopri5 | continuous | 1e-06 | 50 | 8 | 0 | 1.37e-07 | complete |
| dopri5 | continuous | 1e-11 | 248 | 41 | 0 | 2.79e-12 | complete |
| dopri8 | now | 1e-06 | 26,001 | 2,000 | 0 | 1.44e-15 | complete |
| dopri8 | continuous | 1e-06 | 77 | 5 | 0 | 2.43e-09 | complete |
| dopri8 | continuous | 1e-11 | 122 | 8 | 0 | 1.96e-11 | complete |

### stiffish

T=30, dt=0.05; reference refinement delta=1.64e-13.

| Method | Path | Tolerance | RHS | Accepted | Rejected | Max error | Status |
|---|---|---:|---:|---:|---:|---:|---|
| dopri5 | now | 1e-06 | 7,771 | 1,195 | 0 | 1.41e-07 | complete |
| dopri5 | continuous | 1e-06 | 4,166 | 687 | 7 | 5.68e-07 | complete |
| dopri5 | continuous | 1e-11 | 38,246 | 6,367 | 7 | 3.69e-12 | complete |
| dopri8 | now | 1e-06 | 7,849 | 604 | 0 | 1.42e-10 | complete |
| dopri8 | continuous | 1e-06 | 4,533 | 294 | 11 | 6.4e-06 | complete |
| dopri8 | continuous | 1e-11 | 30,190 | 2,003 | 13 | 1.55e-13 | complete |

### PY_S1Bf

T=2000, dt=0.01; reference refinement delta=1.95e-06.

| Method | Path | Tolerance | RHS | Accepted | Rejected | Max error | Status |
|---|---|---:|---:|---:|---:|---:|---|
| dopri5 | now | 1e-06 | 1,400,001 | 200,000 | 0 | 9.77e-05 | complete |
| dopri5 | continuous | 1e-06 | 153,512 | 23,306 | 2,279 | 0.439 | complete |
| dopri5 | continuous | 1e-11 | 600,008 | 99,862 | 139 | not complete | step cap at 92118 outputs |
| dopri8 | now | 1e-06 | 2,600,001 | 200,000 | 0 | 2.35e-07 | complete |
| dopri8 | continuous | 1e-06 | 165,272 | 9,269 | 2,385 | 0.055 | complete |
| dopri8 | continuous | 1e-11 | 664,982 | 38,832 | 7,500 | 1.95e-06 | complete |

<!-- W377 END DATA -->

## Recommendation

Stage 2 implements the maintainer's decision: one continuous DoPri path, with
retained solver state, dense output, DtMax and event/reset handling. There is
no user-selectable restart mode. Its production measurements and checks are
below; the original stage 1 tables above describe the prototype, whose maximum
step policy differed from production.

At equal local tolerances, removing the output-step restriction can increase
trajectory error: the restarted schedule often overresolved that tolerance.
Production saves RHS calls in 17 of 18 model/method pairs, while clustor/DoPri8
costs more with DtMax=Dt=1. Episodic, discontinuous Amari and PY_S1Bf prevent a
blanket accuracy claim. The tolerance controls local error, not the global
trajectory difference. Accuracy-controlled whole-run and solver-inclusive
instruction measurements remain for the coordinator; RHS counts alone do not
establish a runtime improvement.

## Other adaptive methods

| Method | What survives an output call | Output-time behavior |
|---|---|---|
| DoPri5 / DoPri8 | Only the next step guess (`IntegratorState::dp_hout`) | `dormprin` calls `dopri5`/`dop853` to `tout`; local `DpRun`, FSAL derivative, counters, and controller history start again. `hmax=0` becomes `xend-x`. |
| Gear | Work arrays, Nordsieck history, order (`jstart`), saved step/controller data | `ggear` clips `h` to `tout-*t` at L70 and returns at L860. It is output-limited but is **not** a fresh multistep restart per output. |
| QualRK | Predicted `hguess` | `gadaptive` rebuilds its initial derivative/scaling and clips `h=x2-x` before `rkqs`; returns the predicted next step. |
| Stiff | Predicted `hguess` | Same `gadaptive` driver; clipped output endpoints; stiff steps recompute Jacobians. |
| Rosenbrock | `IntegratorState::rosen_htry` | `rosen` sets `hmax=abs(tfinal-t)`, evaluates `f0` at entry, and ends at `tfinal`; no retained interpolant. |
| CVODE | Persistent `CvodeRun` memory, history, derivatives | `ccvode` uses `CVode(...,NORMAL)`. `CVode` steps to/past `tout`, then interpolates using this vendored version's `CVodeDky` (the older name, rather than `CVodeGetDky`). It already separates internal steps from output times. |

These are source audits, not performance measurements of those methods.
Relevant owners are `solver.cpp`, `gear.cpp:ggear`, `stiff.cpp:gadaptive`,
`odesol2.cpp:rosen`, `cv2.cpp:ccvode`, and `cvode.cpp:CVode/CVodeDky`.

## Events and production integration

The current event machinery does not locate arbitrary events exactly in the
mathematical sense. `flags.cpp:one_flag_step` checks endpoint signs and uses a
linear crossing fraction plus `stol`; `nointerp` intentionally places an event
at the endpoint. `integrate.cpp` also interpolates Poincare sections/extrema
from output samples and handles bounds, stopping, torus wrapping, delay
history, storage, cancellation and steady-state monitoring around those
samples. Sampling a long step and leaving these owners unaware would not
establish event correctness. In particular, two crossings inside a bracket
can have equal endpoint signs, and a state/parameter reset invalidates every
coefficient beyond the first event.

A production path should keep one accepted-step interpolant in the Session's
solver, with an internal endpoint separate from the requested output time.
For each accepted interval it must evaluate event functions on the dense
state, bracket crossings in the requested direction, refine the earliest
root with a safeguarded scalar solve, and evaluate **all** state components
at that root. Poincare extrema require roots of the actual RHS component,
not just a root of the selected state. Time sections can use their known
times directly. Apply simultaneous global flags in the existing owner's
order, respect `nointerp`, discard the remainder after a reset, and restart
from the event state. A halt or stop-on-section returns the root state/time.
An interpolated state after a bound violation must not be returned as valid.

Endpoint sign tests alone cannot guarantee detection of arbitrary oscillatory
or tangent event functions. A concrete event contract must specify bracketing
subdivision and root tolerances and test multiple crossings, grazing roots,
simultaneous flags, state and parameter resets, backwards runs, and stops
between output samples. A promise of exact event detection without those
qualifications would be false. No event behavior is changed by this study.

The necessary shared output/event integration work belongs partly to
`core/integrate.cpp`. Per the brief, that file (and `pp_shoot`, `expr*`,
`delay_handle`, and AUTO files) was not edited. The prototype deliberately
measures deterministic ODEs outside that event pipeline; it is not an event
implementation ready to merge.

## Search, reuse, and simplicity

Before writing the harness, searched `dormpri.cpp` and all `core/*.cpp` for
`contd5`, `contd8`, `rcont`, `solout`, and the step counters; `flags.cpp` for
`one_flag_step`, `flag_loop`, and `one_flag_step_dp`; `integrate.h`,
`session.h`, `solver.h/.cpp`, and `integrate.cpp` for solver ownership,
`dp_hout`, output scheduling, Poincare and stopping; the other method owners
for clipping and interpolation; and `xpp_math.h` for the common numerical
operations. This checkout has dense coefficient generation but no remaining
`contd5`/`contd8` evaluator functions. The harness evaluates their nested
polynomials directly from the existing coefficients. It reuses the actual
DoPri routines, `my_rhs`, model loading, SET import, and correctly rounded
model math; it does not reimplement the models in Python or change math
libraries. AUTO does not call these output wrappers and is untouched.

The simpler candidate, integrating to the final time in `dormprin` and
returning cached rows, was rejected for production because it would advance
the live model before the shared event/stop pipeline could act. A new public
mode switch would not repair that contract. No option or parallel production
implementation was added. No file reader, protocol, process-start interface,
or other production trust boundary is changed.

## Gates and review handoff

Native gates, sequentially through `tools/heavy.sh` in Git Bash:

| Gate | Result |
|---|---|
| UCRT `mingw32-make -j4 xppautx test BUILDDIR=build/ucrt WERROR=1` | Build passed, zero warnings; all 52 unit-test executables passed. |
| `tools/examples_check.sh --bin xppautX.exe` | 185 models passed: all 184 stored outputs matched `tests/examples.md5`; `ml-range.odex` produces no output by design. |
| `python tools/servercheck.py --server ./xppautX.exe` | 1,169 checks passed, zero failed. |
| `python tools/goldencheck.py --bin ./xppautX.exe` | Six golden files matched, zero failures. |

The specified `python3` invocations first exited 127 because Git Bash has
no executable by that name. The same scripts then passed using the installed
Python 3.14.7 interpreter's `python` launcher, still inside `heavy.sh`.
No sandbox permission was changed and no link was created. Git Bash prints
an incidental `mkdir /c/Users/muham: Permission denied` startup message; the
heavy-run queue and the successful gates still operated normally.

The coordinator must run the WSL gates, odexcheck and callgrind. No WSL
build/check, merge, push, commit, or independent background run was started.
The coordinator-owned pre-existing W377 assignment in the main checkout's
background-task register was not edited from this worktree. The roadmap
records this card as study measured, with implementation and review pending.

## Stage 2: production dense integration (W377, #439)

DoPri5 and DoPri8(3) retain their internal endpoint, FSAL derivative,
controller/stiffness history and counters across output calls. Dt samples the
dense polynomial; DtMax follows Gear's `numerics.max_step` path. Backwards
runs use signed steps with a bounded initial-step magnitude. The 100,000
accepted-plus-rejected attempt budget includes flag restarts; exhaustion is
a command-placed Error, never a successful shortened run.

Each accepted step (or remainder after a section) is inspected once in 32
subintervals; its earliest event stays pending until output reaches it.
`xpp_math::bracketed_root` uses safeguarded secant/bisection to
`8*epsilon*max(1,abs(time))`. Extrema use the actual RHS. Simultaneous flags
within 128 scaled epsilons retain the original cascade/application order;
resets discard the step remainder. `nointerp` defers to the accepted endpoint.
Halts and stop-on-section return the full root state/time. Bounds and
nonfinite states are checked before returning a sample, including resets.
Tangencies without a sampled zero and crossings faster than the subdivision
can be missed: this finite bracketing contract is not arbitrary exact-event
detection. Reduce DtMax for faster event functions.

Delay history has uniform Dt slots, indexed by tau/Dt without timestamps.
DoPri stops at each history update and invalidates its derivative; intermediate
section returns add no history slot. This preserves the existing delay
scheme. Torus/caller state edits invalidate retained coefficients. The live
expression variables are restored to the returned state/time. Only the
trajectory loop consumes section returns; fitting/shooting still reach their
requested endpoint, with flags/halts active. Cancellation is checked between
accepted steps.

Search/reuse: integrate's output/events and all solver callers (AUTO included),
flags.cpp, solver.cpp, Gear's DtMax path, stiff's clipping, cv2/CVODE's NORMAL
interpolation, and xpp_math/core scalar root routines. Reused DoPri's actual
coefficients/FSAL stages, Gear's setting, CVODE's internal/output ownership
pattern, the flag application owner and section storage/stop pipeline. No
scalar bracket refiner existed in the common math owner; added it there.
AUTO does not call these wrappers. Per-output restart cannot span Dt; caching
a whole trajectory would advance past resets before they act. Neither is a
second mode. No file/path/process/protocol trust boundary changed; the shared
max-step option retains finite-positive validation and now advertises DoPri.

`tests/test_dormpri.cpp` adds 24,861 checks across both orders: exact output
and live-expression times, analytic decay, backwards samples/events, dense
sections (including auxiliaries, a root at t=.371 between output samples
and every component evaluated there), extrema, state/parameter resets,
simultaneous/multiple/grazing events, nointerp, halt, wrapping, bounds, delay
history, fitting endpoints and budget exhaustion. Fixtures are
`tools/models/w377-dense.odex` and `tools/models/w377-delay.odex`.

### Production measurements

[w377-stage2.csv](w377-stage2.csv) and [w377-stage2.json](w377-stage2.json)
record all nine models, input/source/archive hashes, compiler/flags, and all
54 runs. Inputs match stage 1 byte-for-byte; every completed run satisfies
the stage-count identity. Instrumentation exists only in the generated copy
under `build/w377-stage2`, exercising the actual production Solver. This
ignored directory is kept with its raw 17-digit trajectories, generated
source/executable, fixtures, manifest, counts and build provenance; obsolete
pre-guard backups and unrelated check artifacts were removed. Stage 1's
harness is frozen to 16a6ac81, with only its disposable registry seam adapted.
Before-errors are archived stage 1 errors against its tight DoPri8 reference;
after-errors use production DoPri8 at 1e-12, with its 1e-11 refinement below.
The scalar controls use exp(-.01*t) and cos(t). These are solver-family
convergence controls, not an independent reference or a global-error bound.
All production fixtures have DtMax=1; stage 1 ignored that setting.

| Model | Method | RHS before → after | Max error before → after | Reference refinement |
|---|---|---:|---:|---:|
| lecar | dopri5 | 4,201 → 368 | 9.96e-10 → 5.77e-06 | 2.13e-10 |
| lecar | dopri8 | 7,801 → 590 | 3.32e-11 → 3.17e-06 | 2.13e-10 |
| episodic | dopri5 | 112,110 → 87,032 | 134 → 123 | 0.000618 |
| episodic | dopri8 | 134,915 → 119,191 | 113 → 111 | 0.000618 |
| lecdiff200 | dopri5 | 2,843 → 794 | 7.61e-07 → 1.09e-05 | 4.58e-07 |
| lecdiff200 | dopri8 | 5,201 → 1,117 | 6.29e-08 → 0.000207 | 4.58e-07 |
| kuramot100 | dopri5 | 14,019 → 674 | 4.78e-09 → 4.67e-07 | 6.63e-11 |
| kuramot100 | dopri8 | 26,049 → 1,592 | 4.78e-09 → 8.86e-07 | 6.63e-11 |
| amari | dopri5 | 14,091 → 752 | 0.459 → 0.459 | 0.000175 |
| amari | dopri8 | 27,379 → 3,514 | 0.102 → 0.102 | 0.000175 |
| clustor | dopri5 | 3,525 → 3,026 | 4.84e-09 → 4.1e-09 | 1.22e-13 |
| clustor | dopri8 | 6,585 → 7,592 | 6.14e-11 → 1.22e-13 | 1.22e-13 |
| slow | dopri5 | 14,001 → 614 | 1.33e-15 → 3.1e-14 | 4.44e-16 |
| slow | dopri8 | 26,001 → 1,517 | 1.44e-15 → 3.33e-16 | 4.44e-16 |
| stiffish | dopri5 | 7,771 → 4,178 | 1.41e-07 → 5.32e-07 | 1.64e-13 |
| stiffish | dopri8 | 7,849 → 4,533 | 1.42e-10 → 6.4e-06 | 1.64e-13 |
| PY_S1Bf | dopri5 | 1,400,001 → 153,512 | 9.77e-05 → 0.439 | 1.95e-06 |
| PY_S1Bf | dopri8 | 2,600,001 → 165,272 | 2.35e-07 → 0.055 | 1.95e-06 |

At tolerance 1e-6 all 18 runs complete: 17 use fewer RHS calls (11.7–95.6%
fewer), while clustor/DoPri8 uses **15.3% more**. Its DtMax=Dt=1 prevents
longer steps and DOP853 pays three additional dense stages per accepted step.
The other accuracy trade-offs remain material: episodic and Amari retain
large full-trajectory errors, and PY_S1Bf changes to about .439/.055 error.
No runtime or general accuracy improvement is claimed. Fifty of 54 runs
complete; the four tight DoPri5 episodic/PY_S1Bf runs stop at exactly 100,000
attempts with an error. Every tight DoPri8 reference completes.

Reproduce sequentially from Git Bash after the UCRT build:

```sh
export PATH="/c/msys64/ucrt64/bin:$PATH"
python docs/studies/w377-stage2.py fixtures
python docs/studies/w377-stage2.py prepare
tools/heavy.sh bash build/w377-stage2/build.sh
tools/heavy.sh python docs/studies/w377-stage2.py run
tools/heavy.sh python docs/studies/w377-stage2.py summarize
```

### Stage 2 gates

The final UCRT build/unit and example checks were rerun on 3110207a with
these documentation updates; the baseline was not updated again.

| Gate | Result |
|---|---|
| UCRT `-j4 WERROR=1 xppautx test` | Zero compiler warnings; 56 executables, 68,233 checks, zero failures. |
| examples_check, then --update once | 185 entries checked; only the two DoPri8 outputs below change; all other methods unchanged. |
| goldencheck (`--bin`, its supported argument) | Six unchanged files, zero failures; script unchanged. |
| servercheck | 1,193 checks, zero failures; script unchanged. |
| autocompare | 90 checks, 40 models, 9 existing skips, 5,936 points; all diagrams unchanged. |
| web2 manual build/check/typecheck | Passed; rebuilt dist/manual.json. |
| Frozen stage 1 harness smoke | Builds and reproduces slow/DoPri5: 14,001 restarted and 50 continuous RHS calls. |

Intentional DoPri8 baseline changes (retained controller and dense sampling):

- `examples/canonical/tyson.odex`: `e0505e7fc192353f098deb27f81ad892` → `5e92aa3cdf3e06dbf152fb4ddf5b42ff`.
- `examples/ode/tsthomi.odex`: `4d7afb9dd15acb7818ba28f3e552b485` → `c37678b20a8e7cf5cd041bbb81cbf676`.

All heavy commands ran sequentially through heavy.sh; its lock worked.
The wrapper printed a user-profile mkdir permission diagnostic but acquired
a slot and completed each command; no lock path or script was changed.
The initial npm cache access was denied; a worktree-local cache succeeded.
The coordinator reports passing WSL build/unit tests, sourcecheck,
examples_check, odexcheck, goldencheck, servercheck, autocompare and ASan
(build, smoke, examples, unit tests, servercheck) on review tip 3110207a.
Callgrind remains a review measurement. The coordinator's WIP and two review
commits are preserved; these study updates remain uncommitted. No merge,
push, link creation or independent background run was started; roadmap is
untouched.
