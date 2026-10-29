# Five-model XPPAUT / xppautX timing screen

This directory is a reproducible **Linux/WSL headless process** comparison, not a pure solver microbenchmark, a continuation benchmark, or a UI responsiveness measurement. No manuscript text was changed.

## Files

- `sources/`: unmodified model files from XPPAUT 8.0 commit `c021b511eee95a43576227656c6530e9af78ca27`, plus Terman's original `episodic.ode` and `.set` from the pinned ModelDB commit in `provenance.json`.
- `models/`: the five explicit-settings `.ode` inputs, their five generated `.odex` conversions, conversion logs, and the normalized Terman set file and label-repair record.
- `build.py`: rebuild both pinned revisions in separate Linux `/tmp` trees. No package installation or production-source changes.
- `summarize.py`: regenerate `results/report.md` and numerical details from saved results.
- `screen.py`: stage inputs on Linux `/tmp`, run one excluded warm-up and five measured launches per program/model, compare exported numerical results, and save CSV/JSON and compressed outputs.
- `provenance.json` and `build-logs/`: source hashes, compiler, CPU, OS, revisions, exact build commands and logs. The legacy log retains the first build failure before compatibility flags were added.
- `pilot-results/`: initial pilot, including the rejected Terman set file. Pilot wall times used Python's timeout polling and are not the reported benchmark timings.
- `pilot-corrected/`: Terman diagnostic after label normalization. The normalized SET produces exactly the same legacy output as the original SET.
- `results/`: measured timings, summary statistics, comparisons, logs and full gzip-compressed exported arrays for every warm-up and repetition.

## Design

Both binaries are built in the same WSL Linux x86-64 environment with GCC/G++ 15.2, `-g -O2`, no LTO, and `-ffp-contract=off`. Legacy C compatibility flags relax compiler diagnostics, without changing the source. xppautX is built with `WINDOW=0`; both are run without a display. Their mathematical libraries remain those of each implementation (system libm versus xppautX's reproducible math implementation), so results measure the combined implementation, not the isolated effect of C++, x64, or a single change.

Each child is pinned to logical CPU 2. Jobs are sequential; executable order alternates by model/repetition. A fresh HOME prevents personal settings from leaking into runs. Inputs, working directories, and output files are on WSL's native filesystem, not `/mnt/c`. Results are copied to this directory only after measurements. The host is shared and not a dedicated benchmark machine; CPU time is recorded alongside elapsed time. Five repetitions are a screen, not a definitive performance study.

The timed region includes process startup, parsing/loading, numerical integration and output writing. ODE-to-ODEX conversion, staging, compression and output comparison are outside timing. `Popen.wait()` uses a blocking wait; a separate safety timer kills only the runner's own child on timeout. This avoids short timings being quantized by Python's timeout wait polling.

Standard deviations are **sample SD (`ddof=1`)**, computed from the five measured launches; repetition 0 is the excluded warm-up. Failed runs are retained and never silently replaced. A mean/SD is produced only when all five runs complete with valid output. Passing output validation is separate from cross-program agreement.

| Model | States | Method | Total | Output dt |
|---|---:|---|---:|---:|
| Terman episodic | 120 | QualRK | 1999 | 0.4 |
| lecdiff200 | 400 | RK4 | 20 | 0.05 |
| kuramot100 | 100 | RK4 | 100 | 0.05 |
| amari | 200 | Euler | 100 | 0.05 |
| clustor | 366 | QualRK | 500 | 1 |

Both sides receive identical parameter/initial values and numerical settings for each case. The bundled-model tolerances are 0.001, minimum step 1e-12, maximum step 1, output stride 1, storage 20,000 rows, and bound 100,000. For Terman, the published SET overrides values: tolerance 0.001, absolute tolerance 0.01, minimum step 1e-12, maximum step 1, output stride 1, bound 5,000. Its nonintegral Total/dt ends at 1998.8 on both programs. Settings are recorded in `cases.json`, `.ode`, `.odex`, and the SET.

## Documented input repairs

1. `clustor.ode` source line 12 lacks the final `]` in `w1_[j`. Add that one character in the benchmark copy before conversion. The untouched source remains in `sources/`. The existing repository ODEX also has that closing bracket.
2. Terman's original SET uses 31 parameter labels inconsistent with its supplied ODE. Legacy XPPAUT loads the numerical values by position; xppautX correctly rejects the label mismatch. The script asks **the legacy executable** for its ordered parameter names (`-qpars`), then replaces only those labels, preserving every original numerical value and its order. Repairs are in `models/episodic.set-label-repairs.json`. Original-versus-normalized SET output hashes from the two legacy pilots are identical. This preserves the archive's legacy execution; it is not independent proof of the authors' intended parameter mapping.

Source: [Terman et al. model and paper](https://modeldb.science/182758), [archive instructions](https://github.com/ModelDBRepository/182758). The archive relates episodic activity to Figure 3; this screen does not claim reproduction of that figure.

## Output agreement

Checks require a successful exit, finite rectangular arrays, the expected state count, increasing time, and the expected final time. Each paired run compares all exported states at every stored time; comparison results include shape/time-grid agreement, exact numerical equality, maximum absolute error, and a declared screening tolerance of `5e-6 + 5e-5 * abs(legacy_value)`. That tolerance is a diagnostic threshold, not a scientific acceptance criterion. Exact equality is at the programs' exported text precision, **not internal floating-point bit identity**. The summary also checks deterministic output hashes across the five runs of each executable.

## Re-run

From WSL, with the repository's pinned commits available and build dependencies already installed:

```bash
uv run --locked python paper/benchmarks/five-model-screen/build.py --repo "$PWD"
# Use the two binary paths printed by build.py:
uv run --locked python paper/benchmarks/five-model-screen/screen.py \
  --legacy /tmp/xpp-paper-build-XXXX/legacy/xppaut \
  --modern /tmp/xpp-paper-build-XXXX/modern/xppautX \
  --result-name results-new
```

`--pilot-only --result-name pilot-new` runs only the excluded pilot. `--models episodic` selects one case. Existing nonempty result directories are not overwritten. Linux build directories are temporary and should be deleted by their creator after the results are saved; build.py prints the exact directory it created.

## Threads

In the pinned xppautX source, `core/xpp_http.cpp:6-10` describes a main-thread numerical core and connection/logging threads. `core/xpp_job.cpp` implements cancellation/checkpoints. This architecture supports communication and responsive controls while calculation is running; it does not parallelize a single integration or AUTO continuation. UI responsiveness and stop latency need a separate interactive measurement and are not established by these headless timings.
## Recorded result

See [results/report.md](results/report.md), [summary.csv](results/summary.csv), and [comparison.json](results/comparison.json). The executed runner is preserved in results/executed-screen.py. Source and model hashes and the SET equivalence check are in results/input-validation.json.

Implementation search: reviewed tools/run_example.sh and tools/examples_check.sh for existing batch execution/output conventions, core/json_silent.cpp for headless execution, and the solver/converter option owners. Their broad example-regression workflow does not implement paired repeated timing or sample statistics; this standalone study uses Python standard-library subprocess, resource, statistics, gzip, csv and json.

## Terman diagnosis (W239)

The [controlled investigation](terman-investigation/report.md) isolates the original mismatch to the intentional math-library change. Matching the legacy math functions restores exact exported agreement. Tighter tolerances give close cross-program agreement, but pointwise tolerance convergence remains unestablished. The original screen is retained unchanged; diagnostic runs do not replace its timing statistics.

For the complete research status, artifact map, safe rerun instructions and remaining work, start with the [paper benchmark handoff](../../BENCHMARK_HANDOFF.md).

## W269 (#326, 2026-10-11): the compiled build, three engines

Since this screen, xppautX compiles every program a model holds with TinyCC
when it loads (W258/W259/W266); `--no-compile` runs the interpreter. W269
reran the screen with three engines: XPPAUT 8.0, xppautX compiled (its
default) and xppautX `--no-compile`, plus the sandbox model.

- Builds: XPPAUT 8.0 c021b511 with W238's exact build command; xppautX at
  `0643234f` (the W269 worktree's HEAD, pushed master) with W238's command
  (`WINDOW=0`, `-g -O2`, `-ffp-contract=off`): `provenance-w269.json`,
  `build.py --provenance provenance-w269.json --tmp DIR`. Built and run on
  WSL ext4 under `~/w269` (not `/tmp`: `/tmp` is now a tmpfs that WSL empties
  whenever the distribution stops between calls, which deleted the first
  build; `--tmp` names the folder).
- Protocol unchanged (warm-up plus five measured fresh processes per engine,
  CPU 2, fresh HOME, LC_ALL=C, the screen's comparison), the order of the
  three engines rotating each repetition (two engines keep W238's
  alternation). `screen.py --engines legacy,modern,interp --models ...`;
  `measure()` is the timing loop, shared with `../modeldb-267710`.
- The sandbox model `PY_S1Bf` (`EXTRA_CASES`): `../auto/models/PY_S1Bf_bench.ode`
  copied verbatim into `sources/`, at Iapp = 2000 pA where the cell spikes
  (rhs-ceiling's integration, W249), its own settings (stiff, tolerance 1e-8,
  2000 ms, dt 0.01, 200001 rows and two aux columns); only `@ maxstor=200010`
  is added so XPPAUT keeps every row.
- The W238 command still runs W238's two engines and five cases unchanged;
  xppautX's conversion flag is now `--yes` (W288), picked from `--help`.

Results: `results-w269-2026-10-11/` (runs, timings, summary, comparison,
every output, `models/` as converted by the W269 build, `cases.json`,
`compile-counts.json`, `provenance.json`). Tables: `../w269_tables.py screen`.

| Model | XPPAUT 8.0 | xppautX compiled | xppautX --no-compile | XPPAUT / compiled | --no-compile / compiled | compiled vs XPPAUT | compiled vs --no-compile |
|---|---:|---:|---:|---:|---:|---|---|
| episodic | 2.947 +/- 0.077 | 1.445 +/- 0.025 | 3.261 +/- 0.015 | 2.04x | 2.26x | differs (max 3.12, 17369 outside) | exact |
| lecdiff200 | 0.232 +/- 0.002 | 0.198 +/- 0.015 | 0.300 +/- 0.019 | 1.17x | 1.52x | exact | exact |
| kuramot100 | 10.962 +/- 0.350 | 4.622 +/- 0.014 | 9.821 +/- 0.145 | 2.37x | 2.12x | exact | exact |
| amari | 0.217 +/- 0.008 | 0.292 +/- 0.002 | 0.287 +/- 0.009 | 0.75x | 0.98x | exact | exact |
| clustor | 0.483 +/- 0.007 | 0.312 +/- 0.020 | 0.476 +/- 0.003 | 1.55x | 1.53x | exact | exact |
| PY_S1Bf | 22.045 +/- 0.233 | 8.532 +/- 0.224 | 21.415 +/- 0.140 | 2.58x | 2.51x | differs (max 0.262, 552 outside) | exact |

Wall seconds, mean +/- sample SD of five measured runs. Valid runs 90/90; every engine repeats its own output in all five runs: True.

- 90/90 runs valid; each engine repeats its own output in all five runs.
- Compiling changes no number: compiled and `--no-compile` give identical
  exported values on all six models, and on the five W238 models the
  compiled outputs have the same hashes as W238's xppautX (88e09741) and the
  XPPAUT outputs the same hashes as W238's XPPAUT.
- Agreement with XPPAUT is W238's: exact on lecdiff200, kuramot100, amari
  and clustor; Terman differs as diagnosed in section W239 (math library,
  max 3.12); PY_S1Bf's spiking stiff run differs too (max 0.262 in Iionic,
  552 of 2,000,010 values outside the screen tolerance), the same kind of
  last-bit amplification (not investigated further).
- Speed: the compiled build is 2.0-2.6x faster than XPPAUT on the models whose
  right-hand side dominates (episodic, kuramot100, PY_S1Bf), 1.2-1.6x on
  lecdiff200 and clustor, where start-up and output weigh more; on amari it is
  slower (0.75x): amari's time is its `conv` special (a network), which the
  compiler does not touch, plus xppautX's longer start-up (W245; size-scaling).
- Program counts (`compile-counts.json`, `compile_counts.py`: `--server
  --verbose`, the load's "Compiled N of M programs"): every program of every
  model compiles (episodic 173/173, lecdiff200 409/409, kuramot100 101/101,
  amari 401/401, clustor 428/428, PY_S1Bf 33/33), none left to the
  interpreter. Exact Jacobian: available for episodic, lecdiff200, clustor and
  PY_S1Bf; refused for amari ("uses a network") and kuramot100 ("uses a
  function table, whose values the parameters change").
