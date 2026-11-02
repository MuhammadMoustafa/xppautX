# Frozen comparison report

From the checkout root:

```sh
uv run --locked python paper/benchmarks/comparison/final_report.py --final ~/xppautx-comparison/final --out paper/benchmarks/results/report-20261102
uv run --locked python tools/test_final_report.py
```

W589 (#654) reads the W583 frozen series without changing it or running a
benchmark. It reads the study folders named by `status.json` directly. The
output must be outside the input tree. The HTML inlines the supplied Plotly
3.3.1 (MIT; notice and license beside the script), data and scripts; the copied
overlay PNGs are its companion assets. No git-ignored report or extra package
is needed at runtime.

`data/report.json` preserves the inventory, all headline variants, raw summaries
(including start-up), acceptance evidence, label/gate evidence, exclusions and
repetition samples. `data/views.json` exports the exact values used by both
renderers; `data/coverage.json` lists profile reach counts by study and program.
The two curated paper figures are in `figures/paper/` as PNG (300 dpi) with
SVG copies, their captions in its generated `README.md`: overview,
continuation-time, and for the text the continuation agreement and large stiff networks tables. Detailed
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
Missing FD gates are replayed through continuation.gate/autocompare on the
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
