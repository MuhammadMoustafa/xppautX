# W377: adaptive steps across output intervals

Issue: [#439](https://github.com/MuhammadMoustafa/xppautX/issues/439).

## Scope and decision

This is a measurement study, with a disposable continuous-integration
prototype, not a production solver change. The event-safe production path
requires coordination with `core/integrate.cpp`, which this card's brief
explicitly forbids editing while another agent changes it. No numerical
baseline has been rewritten. Instruction counts remain for the coordinator's
WSL/callgrind review; RHS counts are not instruction counts or a speedup claim.

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

Raw 17-digit state CSVs and the generated source/executable remain under
ignored `build/w377/`. The compact counts and input provenance accompany
this study in `w377-counts.csv` and `w377-counts.json`.
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

Pursue dense output, but do not merge an unconditional production replacement
from this prototype. At equal `1e-6` local tolerances it reduces RHS work in
all 18 model/method pairs, by **18.3% to 99.7%**. That establishes a useful
optimization opportunity, not equivalent output accuracy or measured runtime
speed. All 68 completed runs satisfy the stage-count identities above; four
tight DoPri5 runs are incomplete. The complete input/run records, including
those failures, are in the accompanying JSON.

On lecar, DoPri5 changes from 4,201 to 326 RHS calls (**92.2% fewer**), but
its maximum reference difference changes from `9.96e-10` to `5.77e-6`.
DoPri8 changes from 7,801 to 380 (**95.1% fewer**), while that difference
changes from `3.32e-11` to `1.29e-5`. Tightening continuous tolerance to
`1e-11` gives 2,546 / 1,404 calls, respectively, with differences
`3.99e-10` / `2.13e-10`; the reference itself moves by `2.13e-10`.
Consequently those last digits do not establish which path is more accurate.

The slow analytic control needs just 50 / 77 RHS calls with continuous
DoPri5 / DoPri8 versus 14,001 / 26,001 now, with absolute errors
`1.37e-7` / `2.43e-9`. The stiff-ish control saves **46.4% / 42.2%** at
`1e-6`, but errors increase from `1.41e-7` / `1.42e-10` to
`5.68e-7` / `6.40e-6`. Over-tightening it to `1e-11` costs more calls
than the current `1e-6` runs. A runtime claim needs accuracy-controlled
callgrind comparisons, not the most favorable call-count ratio.

The difficult workloads prevent a blanket accuracy conclusion:

- Episodic's full-horizon errors are roughly 113--134 for the `1e-6`
  runs, despite its two tight DoPri8 references differing by `0.00396`.
  Neither schedule at this tolerance is a convincing pointwise reproduction
  of the tight trajectory.
- Amari contains `heav(u[j])`, a discontinuous RHS. Tight continuous DoPri5
  still differs from the DoPri8 reference by about `0.459`, whereas
  DoPri8's own tight refinement changes by `0.000175`. Within-method
  refinement alone does not validate the branch taken across a discontinuity.
- PY_S1Bf saves 89.0% / 93.6% of RHS calls, but changes errors from
  `9.77e-5` / `2.35e-7` to `0.439` / `0.0550`; its tight-reference
  refinement is `1.95e-6`.
- The nominal 100,000-attempt limit is per call. One continuous call
  encounters it where the restarted schedule renews it at every output.
  Tight DoPri5 stops at 4,250 / 3,571 outputs for episodic and
  92,118 / 58,194 for PY_S1Bf (`1e-11` / `1e-12`). The implementation
  must define a continuation budget without silently shortening a long run.

Use one dense path after the shared output/event contract is implemented and
validated. Retaining an ordinary user-selectable restart mode is not needed
to establish these savings. The next implementation needs coordination with
the excluded `integrate.cpp` work, retained solver state, event/reset
invalidation, backwards integration and delay/torus behavior checks. The
coordinator should measure both solver-inclusive and whole-run instructions
in WSL at declared output-error targets; include dense polynomial work and
the extra DoPri8 stages. This card's **production implementation remains
unfinished**, rather than interpreting the file-ownership restriction as
evidence that dense output does not pay. No CHANGELOG, comparison-table or
checksum update is appropriate until user-visible numerical behavior changes.

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
