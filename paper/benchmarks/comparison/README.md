# Frozen comparison report

From the checkout root:

```sh
uv run --locked python paper/benchmarks/comparison/final_report.py --final ~/xppautx-comparison/w617-units --out paper/benchmarks/results/report-w617
uv run --locked python tools/test_final_report.py
```

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
workload alone), `attempt-NNN.log`, `.machine.json`, `.failure.json` and, once an attempt finished, `done.json` (attempt, binaries,
commit, the rows' reuse keys). The units of a study are what its driver prints for `--list` (a model; a model and method; a size
and duration; a continuation description, P1's fixed step apart; Terman's `episodic`; large-stiff's `pilot`, `measure`, `cache`).
Drivers take `--only UNIT` (a unit's name, or its model's), `--list` and `--plan`.

- **Plan first.** Before any timed unit run.py plans every unit: the driver's `--plan --reuse <the unit's newest attempt, else the
  same unit of --reuse's folder>` writes `plan.json` (what is imported, what would run, by the rows' own keys of W452 and W575),
  nothing timed. A plan that raises stops the run, so a reuse-path bug shows in minutes; `--plan` alone does only this.
- **Resume.** The same command again: a unit with a done record and a plan that runs nothing is skipped; a unit a driver fix
  changed runs again in the same folder (a new attempt importing its unchanged rows). No folder-wide signature: the keys decide.
- **A failure is a unit's.** Its attempt keeps `attempt-NNN.failure.json`; the run goes on; the end lists every failed unit and
  exits non-zero. A study's set-up failing (Terman's convergence for the Terman network and its python methods, large-stiff's
  `pilot` for `measure` and `cache`) fails those units only.
- **The protocol is unchanged**: repetition order inside a workload, warm-ups, drift checks, `comparison-setup.sh --ready` before
  each timed unit. Large-stiff's drift calibration brackets all of `measure`'s cases, so `measure` stays one unit (isolating its
  cases would change what the drift covers); `pilot` and `measure` are separate, so a measurement fix never repeats the pilot.
- The planned limits: T1's and T2's sized modes (`adaptive-repeat-N`, `adaptive-sized`) are planned from their first mode, so
  their plan runs the pilots again on a resume.
