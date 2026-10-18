# W378: large stiff networks, stage 1 (#440)

Stage 2's measurements on the Ubuntu comparison machine are **pending**.
This card changes no production code or user-facing behaviour. The study
kit is `paper/benchmarks/large-stiff/`, deliberately git-ignored like the
rest of the paper; the coordinator must copy that directory separately.

The study compares the existing CVODE dense and banded solvers with
SUNDIALS 7.9.0 CVODE/KLU and left-preconditioned restarted GMRES. Both
families use 1000, 2000, 3000 and 5000 states. Only 50 and 200 states were
run here, without timing. No large-size performance conclusion is made.

The standalone adapter loads the model through `xpp::load_model`, calls
`my_rhs` for every engine and reads the compiled sparse derivative
program produced by `core/expr_derivative.cpp`. There is no second
equation translator. The unchanged core is linked privately in a shared
library to isolate its old CVODE C ABI from SUNDIALS' incompatible ABI.
Study-only hooks instrument actual LU setup functions only when the
stage-2 command explicitly enables measurement. Instrumented factorization
diagnostics run separately from the headline time/RSS repeats.

The cable has Dirichlet ends and diffusion coefficient `(n+1)^2`, a unit
interval refinement. The Morris-Lecar ring reuses size-scaling's lecdiff
conductances, with capacitance 0.001. Its cell order is folded
`0,1,m-1,2,m-2,...`, with voltage and gate interleaved. This keeps both
wraparound edges inside half-bandwidth 4; no corner is dropped. GMRES
uses an exact band LU of `I-gamma*J` as its left preconditioner. This is a
strong baseline for these banded networks, not a general sparse-network
preconditioner. The legacy rows retain their native difference Jacobian;
KLU, reference and GMRES preconditioner use the exact sparse program.

Dependencies for the maintainer (nothing installed by the card):

```sh
sudo apt install build-essential cmake libsuitesparse-dev time valgrind
```

Use SUNDIALS **7.9.0**, configured as in W34 with static double-precision
CVODE, serial vectors, sparse/band matrices, band/KLU/SPGMR solvers and
`-ffp-contract=off`; the kit README has the complete configuration.
`tools/comparison-setup.sh` is unchanged: these dependencies belong to
this study until another card needs them.

## Stage-1 evidence (2026-10-18, WSL, untimed)

Every generated model loaded through the unchanged core. Compiled and
interpreted initial RHS agree exactly (relative difference 0 in all four
cases); all engines call this same RHS function. Exact-Jacobian patterns
have 148/598 nonzeros for cable n=50/200, half-bandwidth 1; 150/600 for
ring n=50/200, half-bandwidth 4. Both ring corners remain inside the band.

Errors are max `abs(y-reference)/(1+abs(reference))` over every state and
101 common output times; endpoint uses the last row. The study writes
double precision, avoiding W34's production output.dat precision floor.
Reference is SUNDIALS band/exact at 5e-10, checked against 1e-9.

| Model | Reference halving difference, along run | Final time |
|---|---:|---:|
| Cable 50 | 5.20e-9 | 0.2 |
| Cable 200 | 5.29e-9 | 0.2 |
| Ring 50 | 2.53e-8 | 2 |
| Ring 200 | 3.92e-7 | 2 |

The reference limit is 1e-6, 100 times below the common accuracy target
1e-4. These are sampled trajectory convergence checks, not a proof of
a uniform error bound between output times.

| Model | Solver | rtol = atol | Error along run | Endpoint error |
|---|---|---:|---:|---:|
| Cable 50 | Legacy dense and band, each | 1e-5 | 3.44e-5 | 1.51e-5 |
| Cable 50 | GMRES, exact band LU preconditioner | 1e-5 | 3.25e-5 | 1.44e-5 |
| Cable 200 | Legacy dense and band, each | 1e-5 | 2.87e-5 | 1.02e-5 |
| Cable 200 | GMRES, exact band LU preconditioner | 1e-5 | 2.33e-5 | 7.92e-6 |
| Ring 50 | Legacy dense and band, each | 1e-7 | 1.81e-5 | 5.94e-6 |
| Ring 50 | GMRES, exact band LU preconditioner | 1e-6 | 2.94e-5 | 1.01e-5 |
| Ring 200 | Legacy dense and band, each | 1e-7 | 1.22e-5 | 2.91e-6 |
| Ring 200 | GMRES, exact band LU preconditioner | 1e-6 | 5.53e-5 | 1.20e-5 |

All 12 solver/workload rows pass. The loose pilot uses 1e-5 with an
unchanged 3e-3 agreement limit. An earlier 1e-4 pilot failed on ring 200:
legacy dense error 0.00574684 along the run, 0.000447763 at the endpoint.
That negative result is retained; no error limit was widened. Each
solver then tightens its own tolerance by factors of ten until it meets
the common accuracy target. Equal tolerances do not imply equal accuracy.

**KLU remains unvalidated locally:** SuiteSparse is absent. Stage 1
records this omission; stage 2 refuses to run without KLU. Review added
short n=200 one-shot planning probes, actual factor-timer smoke checks
and two capped cache profiles. No speed or memory saving against another
solver is established by these planning observations; n=5000 remains
unrun locally.

Gates: both study adapters built with `-Werror`; the application build
and all 58 unit-test executables passed; five Python scripts passed
`py_compile`; four synthetic protocol tests passed (rotation and fresh
runs, failure retention, GNU time parsing, cache misses per factor).
Two actual cache smoke profiles stop at exactly three DenseFactor calls.
No verify.sh, full
web2check, sanitizer suite, timing series or large model run was started.
Gates ran on WSL ext4 under the prescribed heavy slot.

The ignored kit's `stage1-results/` contains trajectories, solver counts,
source/input hashes, logs and correctness.json, including the failed
loose-pilot log. Copy these with the kit. Original absolute WSL paths
identify where the runs took place; they are not portable commands.
The current review's rerun and planning evidence are in
`stage1-review-results/`; the original evidence and failed pilot remain.

## Running stage 2

The [kit README](../../paper/benchmarks/large-stiff/README.md) has the full
pinned SUNDIALS configuration and commands. Additional dependencies are
listed above; SUNDIALS 7.9.0's switches are
`-DSUNDIALS_ENABLE_KLU=ON -DSUNDIALS_ENABLE_SUNLINSOL_KLU=ON`
and `-DKLU_INCLUDE_DIR=/usr/include/suitesparse`. Nothing was installed.

From the Ubuntu clone's root, after copying the kit and following
docs/comparison-machine.md:

```sh
KIT=paper/benchmarks/large-stiff
python3 "$KIT/build.py" --sundials "$HOME/w378-deps/sundials-install" --out build/w378
python3 "$KIT/study.py" check --worker build/w378/worker --sizes 50 200 --out build/w378/check-ubuntu
python3 "$KIT/study.py" check --worker build/w378/worker --sizes 1000 2000 3000 5000 --out build/w378/pilot-ubuntu
tools/comparison-setup.sh --record
# MACHINE is the exact JSON path printed above; governor performance, fixed turbo.
tools/comparison-setup.sh --ready
python3 "$KIT/study.py" measure --worker build/w378/worker --cpu 2 --pilot build/w378/pilot-ubuntu --machine-record "$MACHINE" --out build/w378/results-w354-ubuntu
python3 "$KIT/study.py" cache --worker build/w378/worker --cpu 2 --out build/w378/results-w354-ubuntu
python3 "$KIT/study.py" summarize --out build/w378/results-w354-ubuntu
```

The measurement and cache commands each call `--ready`. Five measured
fresh processes follow one excluded warm-up per engine/workload, in
rotating order, one thread pinned to CPU 2. Fresh HOME/config, LC_ALL=C,
machine record, source/input/binary hashes, individual runs and failures
are retained. Summaries use mean/sample SD; GNU time gives peak RSS,
with unrounded parent wall/child CPU clocks retained too. The same dense
workload checks drift at the start and end. Direct solves have no Krylov
iteration count (explicit null); setup and Newton counts are saved.
GMRES saves iterations and preconditioner solves.

Native time/RSS cover the entire worker process, including load, model
compilation and output. Factor diagnostics wrap legacy DenseFactor and
BandFactor; SUNDIALS uses its solver's `ops->setup` callback to instrument
SUNLinSolSetup_Band/KLU (linker wrapping cannot intercept addresses
resolved inside the same library object). KLU's first setup includes
symbolic analysis. The short band/GMRES/reference probes now exercise
those callbacks and retain each factor's time; KLU awaits Ubuntu.

### Bounded cache diagnostic

At the largest n (5000), each family's cache process collects **three
DenseFactor calls**, then ends immediately: six calls total, not a full
integration. Startup/load instrumentation is off (`--instr-atstart=no`);
a client request starts it at the first linear setup, and
`--toggle-collect=DenseFactor` collects only LU and descendants. A
per-run counter stops instrumentation at setup three and unwinds the
owned C++ CVODE state. A failed setup aborts without more attempts.
Each legacy dense setup makes one DenseFactor call; both actual n=200
profiles independently record `calls=3` at that function.

The truncated trajectory is marked as a cache diagnostic, not accepted
as a normal integration. `cache-summary.json` reports all event totals
and I1, D1 read/write, and last-level read/write **misses per
factorization**, divided by the verified count of three. It uses
callgrind's `totals:` record: `summary:` may be zero after instrumentation
is stopped. Cache state is not flushed between the three calls. Keep
raw profiles and cache geometry. This tests the existing storage's
locality; no alternative LU layout is implemented.

### Planning estimate after review

Reserve **30–125 minutes** for stage 2 on a comparable or faster Ubuntu
CPU, excluding dependencies/build and W354's other studies. `measure`
dominates the central estimate. These are extrapolated planning ranges,
not measured Ubuntu results or speed comparisons; manual readiness waits
are separate. The kit's review estimates.json retains the arithmetic:

| Step | Central model | Planning range |
|---|---:|---:|
| Small `check`, n=50/200 | seconds | 0.25–1 min |
| Full-ladder pilot and tolerance selection | 12 min | 6–25 min |
| `measure`, including warm-up, diagnostics and drift | 32 min | 15–65 min |
| `cache`, 3 LU calls per family | 14 min | 7–30 min |

The single-run n=200 wall observations were cable/ring: dense
0.077507/0.158355 s, band 0.070232/0.105320 s, GMRES
0.070002/0.103289 s, reference 0.072175/0.112546 s. Actual dense
factor totals were 0.0014833/0.0098649 s across 13/69 calls.
With q=n/200, use `(T200-F200)*q+F200*q^3` for dense, separating
launch/model/output from the generic cubic factor work, and `T200*q`
for band/KLU and the strongly preconditioned GMRES/reference. KLU's
planning proxy is band time, not a measurement. Sum(q)=55 and
sum(q^3)=20125 on the four-size ladder. The README gives candidate
tolerance counts and all arithmetic; pilot and iteration assumptions
are explicit. There are 160 measured processes, 32 warm-ups, 32 factor
diagnostics and two drift probes, plus calibration/references.

The n=200 three-call cache regions took 0.515008/0.692936 s; whole
processes took 1.537973/2.036661 s. For these fixed-band inputs,
core/dense.cpp skips zero pivot-row updates (`a_kj != 0`), so its actual
sampled work is quadratic with bounded fill, below the generic dense
cubic upper model. Extrapolate the profiled region by 625=(5000/200)^2
and the remaining startup/load by 25: 814 s (13.6 min) for both
families. Allowing roughly half to twice the model and startup variation
gives 7–30 min, well below an hour. The fixed call count bounds work;
there is no speed-based correctness gate or full-trajectory callgrind run.
Different iteration counts, fill, tolerance refinements or CPU/cache
costs require updating the estimate from the Ubuntu pilot. No repeat
timing series or large-size run was made on the laptop.

## Reuse and limits

Searched W34's adapter, build switch, evaluation/report scripts and study;
size-scaling's generator; ModelDB's C translator; the core RHS and exact
derivative/Jacobian owners; comparison-setup's machine/readiness logic.
Reused their settings, error metric, model conductances and existing APIs.
The simpler hand-written RHS was rejected because the shared core bridge
ensures equation, compiler and math-function parity. No translator is
duplicated. The bridge is not a proposed production architecture.

Commands use argv arrays; artifact directories are fresh, with no shell
composed from model input or recursive deletion. The existing bounded
loader validates models. Artifact paths and the kit are trusted local
study inputs. No production trust boundary changed. No temporary files
or own background runs remain; the existing coordinator agent-register
entry belongs to the coordinator.

## Results to fill on Ubuntu

| Quantity | Cable | Morris-Lecar ring |
|---|---|---|
| Equal-accuracy time break-even n | pending | pending |
| Peak-memory break-even n | pending | pending |
| Dense LU cache simulation at n=5000 | pending | pending |
| Recommendation of verbatim vendor or no adoption | pending | pending |
