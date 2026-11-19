# Frozen comparison report

From the checkout root:

```sh
uv run --locked python paper/benchmarks/comparison/final_report.py --final ~/xppautx-comparison/results --out paper/benchmarks/results/report-w617
uv run --locked python tools/test_final_report.py
```

`~/xppautx-comparison/results/` is the comparison's one folder of record (W807, #888): the per-unit layout with every
file a real hard link (one inode each, no space), what `final_report.py --final` reads and what `run.py --reuse` imports.
It is built once, from the older per-unit tree whose entries were symlinks, by
`uv run --locked python paper/benchmarks/comparison/build_results.py --from ~/xppautx-comparison/w617-units --to ~/xppautx-comparison/results`
(refuses an existing `--to`; a dangling link is an error). The files a unit's study shares (`models/`, `provenance.json`,
`machine.json`) appear under each of its units' attempts as the same inode.

W589 (#654) reads the W583 frozen series without changing it or running a
benchmark. It reads the per-unit records of a run.py results folder directly
(`<study>/<unit>/done.json` names the attempt each unit's data is in; no composed
folder). The
output must be outside the input tree. The HTML inlines the supplied Plotly
3.3.1 (MIT; notice and license beside the script), data and scripts; the copied
overlay PNGs are its companion assets. No git-ignored report or extra package
is needed at runtime.

`data/report.json` preserves the inventory, all headline variants, raw summaries
(including start-up), acceptance evidence, label/gate evidence, exclusions and
repetition samples. `data/views.json` exports the exact values used by both
renderers; `data/coverage.json` lists profile reach counts by study and program.
The paper figure, the overview, is in `figures/paper/` as PNG (300 dpi) with an
SVG copy, its caption in the generated `README.md`, which also holds the
continuation agreement and large stiff networks tables for the text. Detailed
views go to `figures/appendix/`. The overview is one row of panels on a shared
log axis of the time ratio (single models, then each study by method, with the
median, the middle half and the count of comparable workloads; the caption
names where XPPAUT was as fast, what it could not run and how the size-scaling ratio moves with N). Each study shows only
programs run there, with xppautX the baseline.

Matched accuracy means the **equal-work gate**, not identical absolute errors
at an arbitrary ladder rung. The screen threshold is `5e-6 + 5e-5 * abs(ref)`;
the original gate also has its declared time-grid allowance where applicable
(`screen.TIMING_FRACTION`). Passed timed means are authoritative, including for
workloads with diagnostic sweeps. Every integration/scaling workload has a
profile entry for every program run in that study; aborts, `never` ladders and
unsupported sizes remain `does not reach`, with reasons, in the denominator.
Terman has untimed acceptance/convergence evidence, so no timing profile entry.
The profile shows one mean-ratio step curve per comparison program and a vertical
xppautX reference at 1, without uncertainty bands. Filters select profiles
precomputed in Python; the browser does no independent profile calculation.

Ratios always mean other / xppautX. Other time plots retain repetition ranges;
large-stiff uses its saved sample SD. The fastest equal-work continuation variant
is selected separately within each Jacobian group, as in the historical tables.
Normalised curves use the xppautX equal-work row's error and time. No curve is
extrapolated; median/quartile support needs at least three curves, and disconnected
support stays disconnected. Six workloads have timed tolerance sweeps, but only
four resolve changing error (the others reach an error floor). The view stays in
the HTML/appendix with that limitation and is omitted from the curated paper set.
Continuation cells project saved pilot-gate judgments (passed, differences,
measures and traced causes); exact cells use converged.json exact_pilots.
Missing FD gates are replayed through continuation.gate/autoxcompare on the
saved outputs, without running a program or writing the frozen data. There is
no separate label matcher: clipped labels and ignored types stay the gate's
decision. Designed skips remain unavailable with their recorded reason. Colours
mean agree, traced, different or unavailable; clicks show the gate source and
its own causes/words. Paper summaries aggregate those same states by model.
The test checks every frozen cell against its gate when the local data exists;
small gate fixtures always run. Large-stiff titles use MiB; the continuation
time finding covers XPPAUT and AUTO-07p separately.

The ten unequal-work groups are reconstructed from divergence, ladders,
unavailable and unsupported pilots, omitted references and description skips.
The tracked continuation owner documents the 600 s cycle-fold omission.
Historical issues/candidates are retained in `final_report_history.json`, with
recorded issue titles/status from `docs/roadmap.md`. Pilot-only setup/inherited
descriptions remain labelled as untimed evidence; no timing is inferred.

# Running the comparison (W699)

```sh
uv run --locked python paper/benchmarks/comparison/run.py --out OUT --xppautx X --xppaut P --auto-dir A --worker W [--reuse EARLIER] [--study S] [--only UNITS] [--plan]
uv run --locked python paper/benchmarks/comparison/watch.py OUT
```

One workload is the unit, each run on its own, one after another: `OUT/<study>/<unit>/attempt-NNN/` (the driver's output for that
workload alone), `attempt-NNN.log`, `.machine.json`, `.competing.json` (W829: the CPU other processes used), `.failure.json` and, once an attempt finished, `done.json` (attempt, binaries,
commit, the rows' reuse keys). The units of a study are what its driver prints for `--list` (a model; a model and method; a size
and duration; a continuation description, P1's fixed step apart; Terman's `episodic`; large-stiff's `pilot`, `measure`, `cache`).
Drivers take `--only UNIT` (a unit's name, or its model's), `--list` and `--plan`.

- **Plan first.** Before any timed unit run.py plans every unit: the driver's `--plan --reuse <the unit's newest attempt, else the
  same unit of --reuse's folder>` writes `plan.json` (what is imported, what would run, by the rows' own keys of W452 and W575),
  nothing timed. A plan that raises stops the run, so a reuse-path bug shows in minutes; `--plan` alone does only this. A unit whose
  set-up unit (`needs`) is not done yet is planned against the set-up's earlier attempt in `--reuse`, the one the run imports from
  until the set-up is done, so its rows are planned and not named as one reason (W827).
- **Resume.** The same command again: a unit with a done record and a plan that runs nothing is skipped; a unit a driver fix
  changed runs again in the same folder (a new attempt importing its unchanged rows). No folder-wide signature: the keys decide.
- **A failure is a unit's.** Its attempt keeps `attempt-NNN.failure.json`; the run goes on; the end lists every failed unit and
  exits non-zero. A study's set-up failing (Terman's convergence for the Terman network and its python methods, large-stiff's
  `pilot` for `measure` and `cache`) fails those units only.
- **The protocol is unchanged**: repetition order inside a workload, warm-ups, drift checks, `comparison-setup.sh --ready` before
  each timed unit. Large-stiff's drift calibration brackets all of `measure`'s cases, so `measure` stays one unit (isolating its
  cases would change what the drift covers); `pilot` and `measure` are separate, so a measurement fix never repeats the pilot.
- **A disturbed unit runs again** (W829, #910): `--ready` looks at the machine only at a unit's start, so while a timed unit runs,
  run.py reads the machine's busy CPU (`/proc/stat`'s first line) and the process table (`common.machine_snapshot`), and
  `common.competing_record` gives the CPU other processes used: the busy total over the unit minus the series' own CPU (run.py, the
  children it reaped and the live processes below it, so a process that starts and exits inside the unit is counted). No polling.
  It is written to the attempt's `.competing.json`. A unit whose competing CPU is above `DISTURBED_CPU_FRACTION` (half a CPU over
  the whole unit: a quiet desktop uses about 0.15, a build several) is disturbed: it gets no verdict, writes no done or pass-1
  record, and runs again once `--ready` passes. Disturbed again, the series stops and names the unit (`DISTURBED_RUNS` bounds the
  reruns); a disturbed unit never writes `stopped.json`. run.log names each disturbed run with its numbers and, best effort, the
  largest users the process table shows, and ends with the folder's count of disturbed runs; the report reads done.json only, and
  a judged unit's done record now carries its `competing` figures. `--ready` keeps its per-process check, which skips a process that
  starts inside its half second, as before.
- **No row of XPPAUT, AUTO-07p or Python is ever run again** (maintainer, 2026-11-17; W802, #883): their programs ran in W617. After
  the plans, a plan whose `would_run` holds any entry of another program than xppautX (an entry names its program first: `row
  auto07p-fd-1: ...`, `xppaut repetition 2: ...`, `python dop853 reference: ...`), or a unit with no earlier attempt, stops run.py
  before anything is run or timed: it prints each such row (study/unit, entry with its reason) and exits non-zero. No option allows
  it: the reader or the key is fixed, or the maintainer rules on a real change of that program's own files. Only `large-stiff`
  (W378's kit) and `server` (W295's own series) have no earlier rows, and a `--check` times nothing and is not stopped.
  What the keys leave out is what no program is given: a continuation description's `reason` text and its `differences` (the gate's),
  and an earlier row's description hash is taken from the description that attempt kept (`window.json`, `description.json`), through
  the current `description_sha`, not from the hash it recorded in its day. The python study's dopri8 ladders are reused per program
  (W805): a program's rungs, the tolerance it reached and its timed and sweep rows are imported when its own program, measurement and
  inputs are unchanged at each rung it tried and the reference is unchanged. The reference is `scipy_integrate.py` run by Python's
  interpreter, so its key is the Python tree's, not any compared program's (`integration.reference_key`, kept beside the reference
  as `reference-key.json`; a reference that changes reruns every climb). A changed xppautX reruns only its own climb and rows; a
  changed Python tree (its program is every file of `tools/python-integration`) changes the reference and reruns every climb.
- **The Python integrators are proven unchanged since W617** (W803, #884). tools/python-integration's files were renamed and
  re-read from core/ after W617 (its rows are keyed by the hash of those files as of commit 9885a178), though no number changed.
  `python_equivalence.py` (one command, `uv run --locked python paper/benchmarks/comparison/python_equivalence.py`, rerun it
  whenever tools/python-integration changes) runs W617's tree (`git archive` of its commit) and this one on every distinct model the
  python study staged and on Terman's dop853 reference, untimed, with the driver's argv, and compares the numerics each tree builds
  (every field) and the output bytes; a run over 120 s goes on the first prefix (a tenth, a hundredth ...) both trees finish, named in
  the record. `python-equivalence.json` holds both program hashes and each unit's mode, row count and output hashes.
  `common.earlier_program` reads an earlier row's Python program hash as today's only for the python arm, only when the record says
  `all_identical` and both hashes are still the ones it names (`earlier_key`, `reuse_whole`, `derived_program_sha`); no row of XPPAUT,
  AUTO-07p or xppautX goes through it. W617's data and records are never rewritten.
- **Imports are references.** An imported row's folder is linked into the new attempt (`common.link_tree`: hard links, so no space and
  no second copy; another file system is an error, not a silent copy), and the `--reuse` folder is `~/xppautx-comparison/results` (every file a real hard link, W807). Terman's XPPAUT
  convergence runs and its Python dop853 reference are imported the same way, by the same keys.
  **Each row's key is what its program reads (W827, #908):** `common.program_inputs`: XPPAUT's `.ode`, its `.inc` files and its `.set`;
  xppautX's `.odex`; the Python programs' `.odex` by its numbers (`model_key`: no display label, no option written at its default).
  The `.odex` xppautX writes beside the model is not XPPAUT's input, so a converter change reruns no XPPAUT row. A plan and a run key a
  row through the one function (`integration.current_key`, `importable`); an earlier row's recorded key has its inputs read again from
  the row's own copy of the model, and its command line is checked for every earlier row (`argv_unchanged`). A row derived from what
  an earlier attempt kept (a series before W452) is keyed the same way. The dop853 reference's model is compared without the
  array plot's three labels (`terman.DISPLAY_OPTIONS`, each named with its reason; W806): a label-only change of `episodic.odex` keeps
  the key, and any other change of the model changes it.
- T1's and E1's repetition windows come from the earlier attempt's `window.json`; T2's sized mode (`adaptive-sized`) is planned as
  its earlier attempt sized it (its pilot wall), so the plan names its rows; an unsupported program (`pilot-xppaut-unsupported.json`)
  has no rows to plan. An earlier row that finished but was not equal work is imported with that verdict, not run again.

# The server study (W295, #353)

`--server` driven from Python and run in parallel, against pure Python, as a study of run.py like the others: `server`, its unit a model
(`lecar`, `lecdiff200`, `wave`: published models of examples/ode that the export accepts; `server.py`'s MODELS says each one's parameter,
range and why), its folder `server/<model>/attempt-NNN/`, its rows' reuse keys (W452: a row reruns only when its arm's program, the
measurement code, the study's settings, the inputs or the CPUs changed), `--plan` and `--check` as for the others:

```sh
uv run --locked python paper/benchmarks/comparison/run.py --out OUT --xppautx X --xppaut P --auto-dir A --study server [--plan | --check]
uv run --locked python paper/benchmarks/comparison/server_report.py OUT [--out report.md]   # the study's report section
```

**The grid.** One parameter per model, `GRID_POINTS` = 64 values over its range (lecar and lecdiff200: the applied current `iapp`, 0 to
0.3 and 0 to 0.2, from rest through oscillation (lecar: to its quenched end); wave: the coupling `d`, 0.05 to 0.4), on RK4 at the
model's own dt with `storage_precision: double`. **Sweeps** of 1, 2, 4, 8 and 16 worker processes (#353 Q5), the grid dealt round-robin,
each worker pinned to its own CPU (the physical cores first, then their hyperthreads: the i7-11700F's 8 cores, 16 threads). Each sweep is
timed from outside, start-up included; each worker times its own start-up and runs.

**The arms** (`server_worker.py` describes them in full):

| arm | what a run is |
|---|---|
| `xppautx_server` | one persistent `xppautX --server` per worker, driven by python/xppautx's `Client`: `value.set_parameters`, `run.initial`, then the rows by `data.page` (the only way the data leaves, so it is in the run; `fetch_seconds` is that part) |
| `xppautx_silent` | one `xppautX --silent` process per run, on a copy of the model whose `par` value holds the parameter (the command line has none), the rows `output.dat` |
| `export_python`, `export_python_numpy`, `export_python_numba` | the script `xppautX --convert MODEL --to python`, `python_numpy`, `python_numba` wrote (W764, W766), imported as a module in the worker, its module-level parameter set, `run()` writing its CSV. numpy only for a model with an array line (the export's header says so) |
| `scipy` | `solve_ivp` RK45 over the python script's own `rhs`, at a tolerance matched to RK4's error (below), rows written as CSV |

A numba script reads its parameters when `run()` starts and passes them to its compiled functions (W770), so a sweep sets the parameter and
calls `run()` again with no compile. The first run compiles the script, and that compile is in the numba arm's first run's time, as the
start-up of the other arms is in theirs.

**Matched error for scipy.** RK45 is adaptive, RK4 fixed: they are compared at matched error, not matched tolerance. On the grid's middle value an
untimed pilot integrates the script's rhs with DOP853 at 1e-12 (the accuracy reference of tools/python-integration), measures xppautX's RK4
rows' largest absolute difference from it, and takes the loosest `rtol` of 1e-3 ... 1e-12 (`atol` equal) whose RK45 rows are no further. A tolerance
whose solve fails (a trial step out of cosh's range) does not count. The rtol is in the rows (`tolerance`) and the agreement pass's file.

**The check, untimed.** Before any timed sweep an agreement pass runs every arm over the whole grid on one process and compares every row
with xppautX's own (`--silent`) through tools/exportcompare.py (the comparison tools/exportcheck.py also makes): the RK4 arms to 1e-9 relative
(and 1e-12 absolute, an ulp of the C library's functions against xpp::math's), scipy to 10 x (RK4's error + RK45's error) absolute. A timed sweep
is equal work when each worker's runs end with the agreement pass's row count and last row. `--check` is the study once on a 4-value grid with
1 and 2 processes, untimed and labelled so (`check` in its provenance and done record): it shows the arms and the machinery work end to end and
measures nothing. The full timed series runs in W718's quiet window, from the same binary.
