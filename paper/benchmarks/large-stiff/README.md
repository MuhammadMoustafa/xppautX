# W378 (#440), large stiff networks

**Stage 1 only on the laptop. Stage 2 on the Ubuntu comparison machine.**
No code in `core/`, baseline, or production build switch is changed. Copy
this entire ignored folder to `paper/benchmarks/large-stiff/` on the merged
comparison checkout. The committed record is `docs/studies/w378-large-stiff.md`.
Only the Python standard library and the already installed C++ toolchain
are needed by the kit. Keep all generated output under `build/`.

## Reuse and equations

Read/reused: W34's `core/cv_sundials.cpp`, Makefile `SUNDIALS=DIR`,
`tools/sundials_eval.py`, `tools/sundials_report.py`, and
`docs/sundials-eval.md` on `task/W34-sundials` (read using `git show`).
The BDF/Newton, scalar tolerance and 2000 steps/output-call settings and
the max `abs(y-ref)/(1+abs(ref))` error metric come from that work.
W34's best-of-repeats timing is intentionally replaced by rerun-plan
section 4's five repeats, sample SD, excluded warm-up and rotating order.
Its old adapter predates the current core's exact Jacobian API, so it is
not copied into production or cherry-picked.

The cable is a unit-interval diffusion/reaction discretization:
`x_i'=(n+1)^2*(x_(i-1)-2*x_i+x_(i+1))-x_i`, Dirichlet ends zero,
`T=.2`. Initial conditions contain two sine modes and a local Gaussian.
The ring reuses the conductances and activation functions of
`size-scaling/scaling.py:lecdiff`, with `iapp=.1`, capacitance `.001`,
diffusive coupling 5, `T=2`, voltage initial condition
`-.49+.05*cos(2*pi*i/cells)` and gates `.1`. Both are transient, smooth,
deterministic networks; n counts states, so a ring has n/2 cells.

The ring's ordering is **folded**: `0,1,m-1,2,m-2,...`, each cell's v,w
adjacent. Both corner couplings are retained and become ordinary entries
within half-bandwidth 4. Cable half-bandwidth is 1. The generator's bounds
are checked against the actual nonzero pattern, not assumed by the solver.

`bridge.cpp` loads each generated `.odex` with `xpp::load_model`, and
every engine calls the very same `my_rhs`. It reads the state pattern and
values from the compiled derivative program produced by
`core/expr_derivative.cpp`; it does not numerically guess sparsity.
`modeldb-267710/auto07p/odex_to_c.py` was inspected: no translation is
needed, so it is not duplicated. The initial compiled/interpreted RHS
check is strict relative error <=1e-12 (exact zero handled separately).
The RHS passed to all CVODE callbacks is identical by construction.

There are two incompatible C ABIs named CVode. A private PIC shared
library contains the unchanged core/legacy solver, with a version script
exporting only the study bridge and `-Bsymbolic` binding its internals.
The worker links SUNDIALS statically, separately. This is a study adapter,
not a proposed application architecture; all rows use the same worker,
loader, model compiler and double-precision trajectory writer. Whole
process time and RSS therefore describe this worker, not the production
CLI or a pure linear algebra microbenchmark.

Rows: legacy-dense and legacy-band (the native difference Jacobian),
KLU (exact sparse CSC Jacobian), GMRES(30) with left preconditioning by
**exact band LU of I-gamma*J**, rebuilt when CVODE requests setup.
The reference is SUNDIALS band CVODE with exact derivatives at 1e-9 and
5e-10. This strong preconditioner tests Krylov against the natural direct
solve for these networks; it does not establish a result for arbitrary
irregular sparse graphs. `reference` is available as a worker solver for
additional method/Jacobian controls, but is not a headline row.

## Dependencies and build on Ubuntu (maintainer installs)

First follow `docs/comparison-machine.md` and `tools/comparison-setup.sh`.
Additional packages (not added to comparison-setup, as required):

```sh
sudo apt install build-essential cmake libsuitesparse-dev time valgrind
```

Use W34's CVODE 7.9.0 source distribution, **not an unpinned distro
SUNDIALS version**. Archive sha256:
`ae1602334e17b410462fda3d08cfe6ec4610f2e76e50f775fd64c1a4a5671a8e`.
Obtain `cvode-7.9.0.tar.gz` from LLNL SUNDIALS' v7.9.0 release; retain
the archive, its hash, and CMakeCache.txt with the machine artifacts.
Below `SRC` is the unpacked source outside the repository; `PREFIX` and
the SUNDIALS build also stay outside the repository.

```sh
SRC="$HOME/w378-deps/cvode-7.9.0"
PREFIX="$HOME/w378-deps/sundials-install"
cmake -S "$SRC" -B "$HOME/w378-deps/sundials-build" \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_INSTALL_LIBDIR=lib \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS='-ffp-contract=off' \
  -DBUILD_STATIC_LIBS=ON -DBUILD_SHARED_LIBS=OFF \
  -DSUNDIALS_PRECISION=DOUBLE -DSUNDIALS_INDEX_SIZE=64 \
  -DSUNDIALS_ENABLE_CVODE=ON -DSUNDIALS_ENABLE_MPI=OFF \
  -DSUNDIALS_ENABLE_OPENMP=OFF -DSUNDIALS_ENABLE_KLU=ON \
  -DSUNDIALS_ENABLE_SUNLINSOL_KLU=ON -DKLU_INCLUDE_DIR=/usr/include/suitesparse \
  -DSUNDIALS_ENABLE_C_EXAMPLES=OFF -DSUNDIALS_ENABLE_CXX_EXAMPLES=OFF \
  -DSUNDIALS_ENABLE_EXAMPLES_INSTALL=OFF
cmake --build "$HOME/w378-deps/sundials-build" -j4
cmake --install "$HOME/w378-deps/sundials-build"
uv run --locked python paper/benchmarks/large-stiff/build.py --sundials "$PREFIX" --out build/w378
```

The CVODE distribution enables the serial, band, sparse, SPGMR and
Newton components by default. The build uses the explicit component
link list from W34, extended for sparse/Krylov. It records commands and
whether KLU was found in `build/w378/build.json`. Missing KLU is an error
for every stage-2 series, never a missing row disguised as success.
The local WSL installation at `~/sundials-install` has no KLU, so only
stage 1 may pass `--allow-no-klu`; its report names that omission.

## Stage 1, untimed correctness only (laptop)

From Git Bash in this worktree, with a clean committed tracked tree:

```sh
tools/wslrun.sh bash /mnt/c/gitRepos/xppautX/.claude/worktrees/W378/paper/benchmarks/large-stiff/small.sh
tools/wslrun.sh bash /mnt/c/gitRepos/xppautX/.claude/worktrees/W378/paper/benchmarks/large-stiff/gates.sh
```

`small.sh` copies the ignored kit into the WSL clone's `build/w378-study/`
before building or checking on ext4. It never runs a timing mode. The
report is `build/w378/correctness/correctness.json` in that clone.
Use a new `--out` path if repeating: existing results are refused.
The small.sh wrapper accepts that output path as its first argument.
The delivered `stage1-results/` contains the passing small checks and
logs, including the earlier failed 1e-4 loose pilot. No laptop timing
series was run. The build/application gate passed all 58 unit programs,
the five Python modules compiled, and all four synthetic protocol tests
passed after review. The review also ran only short n=200 one-shot
planning probes and three-call cache smoke tests; no repeat timing series
or large-n run was started. Current review evidence is in
`stage1-review-results/`; the original `stage1-results/` is retained.
Correctness compares all n states at 101 common nominal times, at both
n=50 and 200, including the endpoint. A loose 1e-5 run must be within
3e-3. Tolerance is then tightened by factors of 10 until the common
accuracy target **1e-4** is met; this is solver-specific, not an assertion
that equal tolerance implies equal global error. The reference's
halving difference must be <=1e-6 (100 times below that target).

## Stage 2, exact foreground commands on Ubuntu

No laptop timing. Clean merged comparison checkout on Ubuntu's own disk,
machine to itself, CPU 2 (confirm availability with `lscpu -e`), one
thread. Do not build or run another study concurrently. Set the governor
to performance as the maintainer, fix and record turbo state, then:

```sh
KIT=paper/benchmarks/large-stiff
uv run --locked python -m py_compile "$KIT"/*.py
uv run --locked python "$KIT/study.py" check --worker build/w378/worker \
  --sizes 50 200 --out build/w378/check-ubuntu
# Large untimed calibration/reference pass, separate from the timed series:
uv run --locked python "$KIT/study.py" check --worker build/w378/worker \
  --sizes 1000 2000 3000 5000 --out build/w378/pilot-ubuntu
tools/comparison-setup.sh --record
# Set MACHINE to the exact path printed by --record, not a wildcard.
MACHINE="$HOME/xppautx-comparison/provenance/machine-YYYYMMDDTHHMMSSZ.json"
tools/comparison-setup.sh --ready
uv run --locked python "$KIT/study.py" measure --worker build/w378/worker --cpu 2 \
  --pilot build/w378/pilot-ubuntu --machine-record "$MACHINE" \
  --out build/w378/results-w354-ubuntu
# Separate cache-simulation diagnostic, never a timed repeat:
uv run --locked python "$KIT/study.py" cache --worker build/w378/worker --cpu 2 \
  --out build/w378/results-w354-ubuntu
uv run --locked python "$KIT/study.py" summarize --out build/w378/results-w354-ubuntu
```

`measure` itself calls `--ready` before the warm-up/measurement series;
`cache` calls it separately. If readiness refuses, resolve the condition
and use a fresh output folder (no automatic sleep/poll retry). Calibration
is not repeated by `measure`. The command copies its pilot inputs and
references into the result directory. Keep the original folder as well.
One excluded warm-up per workload/engine, then five measured fresh
processes; order rotates each repetition. All subprocesses get fresh HOME
and XPP_CONFIG_DIR, LC_ALL=C, DISPLAY unset and BLAS/OpenMP threads 1.
`taskset` pins each measured child. GNU `/usr/bin/time -v` records wall,
user+system CPU and peak RSS. The same explicit measurement branch also
retains unrounded wall and child CPU clocks (W34's run_once approach),
since GNU time rounds seconds in its text. Summaries use those unrounded
clocks and report mean and sample SD. These
are whole-process measurements, with process startup, model compilation
and output included. Never turn time into a correctness pass/fail rule.

Factorization diagnostics run in **separate fresh processes after** the
headline series. Explicit `W378_FACTOR_LOG` enables GNU linker wrappers
around legacy DenseFactor/BandFactor and setup callbacks around SUNDIALS
SUNLinSolSetup_Band/KLU. The latter must use the solver's `ops->setup`:
linker wrapping does not intercept addresses resolved inside the same
SUNDIALS object file. The small planning probes exercise the band hook.
Each records wall and CPU time of the actual factor function, return
flag and kind. KLU setup includes its first symbolic analysis and later
refactorizations; band setup includes copying as implemented by SUNDIALS.
Log I/O occurs outside each timed function, but its perturbation is why
these are not the headline time/RSS runs. Default correctness runs read
no clocks; profiling and planning require explicit requests.
`factors.jsonl` retains individual events and result.json their
counts, totals and per-factor means. Direct legacy linear-iteration
counts are unavailable, explicitly null; Newton iterations and setup
counts are preserved. GMRES records linear iterations and preconditioner
solves; its Jacobian count counts exact preconditioner evaluations.

The cache step is **exactly three DenseFactor calls per family at the
largest n (5000), six calls total**, then the run ends. It is not a whole
trajectory under callgrind. `--instr-atstart=no --collect-atstart=no`
leaves startup/load uninstrumented. The first legacy linear setup starts
instrumentation with a callgrind client request; `--toggle-collect=DenseFactor`
collects only LU and its descendants. After the third setup the bridge
stops instrumentation and unwinds the C++ CVODE run through its normal
owners. A failed setup aborts immediately, without retrying beyond the cap.
Every dense setup calls DenseFactor once (core/cvdense.cpp); n=200 smoke
profiles independently show `calls=3` at DenseFactor for both families.

The saved trajectory is deliberately incomplete and is never checked as
a normal integration. Stats must say `cache_complete=true` and
`factorizations=3`. `cache-summary.json` reports each family's instruction/
data totals and **I1, D1 read/write, and last-level read/write misses per
factorization**, dividing collected totals by three. The parser reads
callgrind's `totals:` record (its `summary:` can be zero after stopping
instrumentation). Retain callgrind.out and its cache geometry, plus
`callgrind_annotate --inclusive=yes ...`. Cache state evolves naturally
through the three setups; no cache flush is inserted between calls.
This diagnoses locality in the existing dense storage; it does not
implement or time an alternative LU layout.

Artifacts: commands, stdout/stderr, all trajectories, stats, failures,
input/source/binary hashes, machine.json, build.json, runs.json,
summary.csv/JSON and crossings.json. Only complete five-pass rows receive
timing summaries; failed runs remain. A first measured win on this ladder
is not an interpolated crossing. Report nonmonotonic wins, uncertainty,
the banded baseline, and memory's process/model overhead before making
any recommendation. The human study conclusion belongs in the committed
page after stage 2; nothing is vendored by this kit.

## Stage-2 planning estimate (review, not benchmark evidence)

Reserve **30–125 minutes total**, on a comparable or faster Ubuntu CPU;
the native `measure` step dominates the central estimate. This excludes
dependency installation/build and the other W354 studies. It replaces
the original unbounded day-long cache allowance.

| Step | Model estimate | Planning range |
|---|---:|---:|
| `check`, n=50 and 200, including KLU | seconds | 0.25–1 min |
| Large `check` / pilot, full ladder and tolerance selection | about 12 min | 6–25 min |
| `measure`, warm-ups + five repeats + factor diagnostics + drift | about 32 min | 15–65 min |
| `cache`, 3 LU calls per family, n=5000 | about 14 min | 7–30 min |

These are work-scaled engineering estimates, **not measured Ubuntu
results or speed assertions**. `stage1-review-results/planning/` retains
the single-run n=200 inputs, clocks, counts, factor logs and profiles.
`stage1-review-results/estimates.json` records the arithmetic and planning
ranges; `cache-summary.json` records the n=200 misses per factorization.
No five-repeat series was run on the laptop. Each native probe has one
fresh process; the wall clock includes launch, load, output and driver
checks. Factor clock reads occur only because this explicit planning
command requests them. The observations (seconds) were:

| n=200 | Dense whole run | Dense factor total | Band whole run | GMRES whole run | Reference whole run |
|---|---:|---:|---:|---:|---:|
| Cable | 0.077507 | 0.0014833 (13 calls) | 0.070232 | 0.070002 | 0.072175 |
| Ring | 0.158355 | 0.0098649 (69 calls) | 0.105320 | 0.103289 | 0.112546 |

With q=n/200, estimate dense time as
`(T200-F200)*q + F200*q^3`: keep launch/model/output work separate from
the generic cubic LU cost. Band/KLU and the strongly preconditioned
GMRES/reference rows use `T200*q`, assuming similar iteration counts.
KLU is not installed here: its planning proxy is banded time, **not a
KLU measurement**. For the ladder, sum(q)=55 and sum(q^3)=20125. Pilot
allowances are one candidate tolerance per cable solver; for the ring,
three dense/band/KLU candidates and two GMRES candidates, plus two
reference runs and a probe per workload. Measurement has seven runs per
engine/workload (warm-up, five measured, one factor diagnostic), plus two
smallest-cable drift runs: 160 measured, 32 warm-ups, 32 diagnostics.

For cache planning, the three-call instrumented regions at n=200 took
0.515008 s (cable) and 0.692936 s (ring); whole process times were
1.537973 and 2.036661 s. The actual LU has an `a_kj != 0` update guard
in core/dense.cpp: these fixed-band matrices retain bounded fill, making
its sampled work quadratic rather than the generic fully dense cubic
upper model. The profiles collected 3,136,782 / 3,769,845 instructions
across three calls. Use 625=(5000/200)^2 for the instrumented region and
25 for the remaining launch/load cost:
`625*(.515008+.692936)+25*((1.537973-.515008)+(2.036661-.692936))`
= about **814 seconds (13.6 min) for both families together**.
The 7–30 minute range allows roughly half to twice that cost and startup
variation. Three calls, not a time-dependent cutoff, bound the work.

The ranges also allow CPU/cache differences, compiler/loading cost and
changed adaptive iteration counts. They are not hard runtime bounds;
extra tolerance refinements or different fill require revising the
estimate from the Ubuntu pilot. No pass/fail condition depends on speed.
Dependency builds and manual readiness waits are outside these run ranges.
To reproduce only the short planning probes, explicitly:

```sh
tools/wslrun.sh bash /mnt/c/gitRepos/xppautX/.claude/worktrees/W378/paper/benchmarks/large-stiff/review-checks.sh NEW_TAG
```

This runs n=50/200 correctness, one-shot n=200 planning/cache probes and
the build/unit gates in one foreground heavy slot. Use a new tag to
preserve earlier artifacts. It never generates or runs n=5000 locally.

## Review limits

No install, network, shell built from model input, or background process
is started by the drivers. Subprocesses use argv arrays and output
folders are freshly created, never recursively deleted. Model parsing is
the existing core's bounded loader. This is trusted local study tooling,
not a service for hostile commands or artifact paths. All rules about
production security and numerical reproducibility remain unchanged.
The simpler alternative (a hand-written or translated RHS) was rejected
because the core bridge removes equation/compiler/math-library drift
and permits direct reuse of exact derivative sparsity.
