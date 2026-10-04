# W9, W34 and W111 adoption assessment

Assessment [W183](https://github.com/MuhammadMoustafa/xppautX/issues/235),
2026-10-04. Implementation branches remain outside master and v0.2.0.

The follow-up [Wasm profile and optimization report](wasm-performance.md)
(W184) isolates exact software FMA as the main trig-workload cost, tests
a nonportable hardware diagnostic. The later exact-FMA investigation
and maintainer decision close W9 as not planned; see the
[published evaluation](https://github.com/MuhammadMoustafa/xppautX/discussions/238).

## Different layers, no required dependency

| Card | Layer | Potential benefit | Relationship |
|---|---|---|---|
| [W9](https://github.com/MuhammadMoustafa/xppautX/issues/21) | Execution platform: Emscripten core in a browser worker | Browser use without installing a native executable | Runs the same evaluator and solvers; does not require W34 or W111 |
| [W34](https://github.com/MuhammadMoustafa/xppautX/issues/72) | CVODE solver implementation | New solver capabilities, maintenance; workload-dependent performance | Affects models selecting CVODE; independent of browser execution |
| [W111](https://github.com/MuhammadMoustafa/xppautX/issues/163) | Native evaluator function placement | Reduce CPU instruction-layout sensitivity | Can affect any native solver calling the evaluator; does not establish a Wasm optimization |

Changing CVODE does not remove the expression-evaluation cost. Native
alignment and a solver replacement could coexist, but their combined gain
must be measured, not added from separate reports. Building SUNDIALS for
Wasm would require its own target build, numerical bridge and validation;
the native SUNDIALS archive cannot be reused in the browser build.

## Wasm: measured portability proof, no speed gain

The existing W9 prototype uses `-O2` when compiling but omitted optimization
when linking. Review commit `23ea4b4d` passes `$(OPT)` at link time too,
following [Emscripten's optimization guidance](https://emscripten.org/docs/optimizing/Optimizing-Code.html).
The benchmark review extends the existing `tools/wasmcheck.mjs` owner:
one warm-up per variant, three alternating samples, median reporting, and
comparison of every stored kuramot100 value and column. Times never
determine whether a check passes. The existing protocol client and
numerical-series extraction are reused; no sleep was introduced. The
final shared comparator uses `Object.is`, also preserving negative zero;
the timing table below was recorded before that comparator refactoring.
The final check reran all twelve existing assertions and four complete
kuramot100 comparisons (warm-up and one measured pair) with the exact
comparator: all sixteen passed. A single measured pair is verification,
not an additional performance study.

WSL, emsdk 6.0.11, native GCC `-g -O2`, Wasm compilation and linking `-O2`,
`-ffp-contract=off`, one heavy local job at a time. Native and Wasm use the
same W9 core revision. These are integration times through the same client,
excluding module/process startup, with series delivery enabled.

| Variant | Warm-up (s) | Measured samples (s) | Median (s) |
|---|---|---|---|
| Native | 10.415 | 10.597, 11.164, 10.609 | 10.609 |
| Wasm | 36.418 | 36.974, 37.145, 37.034 | 37.034 |

Wasm is 3.49 times slower on this evaluator-heavy workload. All eight
complete kuramot100 series had identical serialized representations. The twelve existing checks also
pass: lecar against native protocol and printed output, active-run abort,
and refusal of unsafe base names. This is one workload on one machine in
Node's Wasm runtime, not a claim about every solver or browser.

Optimized linking reduced the Wasm file from 3,464,754 to 3,076,198 bytes
and its JavaScript from 269,513 to 119,470 bytes. The earlier unoptimized
comparison also passed all twelve checks, but grouped best-of-three timing
is unsuitable for claiming a gain against the alternating result.

Adding W111's `[[gnu::aligned(4096)]]` to W9's actual `eval_rpn`, then
recompiling that object and relinking with the same flags, produced an
**identical Wasm binary**. Before and after SHA-256:
`bb937c753b2683c376bc17fbcfdb109a7d3ca147b0b9a26520bc58046b049571`.
For this build W111 contributes nothing to W9's emitted module.

W9 is not ready as a complete application replacement. Its report leaves
IDBFS persistence/reload, file commands and replacement, AUTO, and full UI
integration unverified. The page currently starts with persistence off.
The build also needs review of C++ exception handling: the core contains
catch handlers, while optimized Emscripten builds require explicit
exception-catching configuration. No adoption claim is based on untested
error paths or an altered floating-point policy.

The optimized module also passed all four existing native-browser checks:
isolated worker integration (601 rows), active-run abort, and explicit
refusal to start without cross-origin isolation headers. This verifies
the small proof page, not the complete application's UI or persistence.

## SUNDIALS: existing evaluation

W34's evaluation at `e03a786c` compares SUNDIALS CVODE 7.9.0 and the
vendored CVODE. It records equivalent accuracy on several closed-form
models, differences in adaptive work on others, and changed outputs in
five existing CVODE examples. Its timing pass found no consistent speed
advantage: approximately 23% greater per-step cost on a cheap oscillator,
7% less time on one dense 100-equation case, and additional work on toy_ok.
Those timings are the existing agent's best-of-seven measurements.
W183 also reran osclong, heatband and toy_ok with the existing harness,
three repeats per configuration, no tight reference runs. All 48 selected
integrations exited successfully (16 configurations, three repeats), with
the same step and right-hand-side counts as the archived evaluation.
Best CPU times for osclong at 1e-8 were 0.196s vendored versus 0.233s
SUNDIALS; toy_ok was 0.354s versus 0.430s; heatband was approximately
equal. This focused recheck does not repeat the entire accuracy study.

The candidate adds build/dependency work and needs a bridge from its
`pow` calls to the project's portable numerical owner before cross-platform
adoption. The adapter also ignores initialization API return codes;
its passing normal-model checks do not establish allocation-failure safety.
Do not merge the optional backend abstraction solely to retain an
implementation with no selected use.

The useful result is that SUNDIALS should be reconsidered for a concrete
capability such as root finding or sparse/Krylov solving, rather than as
a general speed change. Preserve the evaluation and the discovered
CVODE tolerance-mapping discrepancy independently of any solver switch.

## Native alignment

W111's earlier current-tip comparison did not demonstrate improvement:
normal-build medians were 10.148 seconds without alignment and 10.282
with it, with identical numerical output.

The warmed recheck used one warm-up and five alternating full silent runs
per variant, reporting both wall and child CPU time. The normal baseline
was built at `92f3682e`; its core and third-party sources are identical to
current master `0eb89196`. The alignment candidate is `79a1b32d`. The
release comparison builds both variants from that same tree with only
the alignment removed for its control, using `RELEASE=1`, `WERROR=1`,
`-O2 -flto=auto` and `-ffp-contract=off`, with `-j4`. Both release builds
and the refreshed normal candidate build passed without warnings.

| Build | Baseline wall median (s) | Aligned wall median (s) | Baseline CPU median (s) | Aligned CPU median (s) | Observation |
|---|---|---|---|---|---|
| Normal | 9.901 | 9.738 | 9.897 | 9.734 | Aligned 1.6% faster |
| Release/LTO | 9.948 | 10.482 | 9.945 | 10.482 | Aligned 5.4% slower |

Normal wall samples, baseline: 9.849, 10.012, 9.848, 10.152, 9.901;
aligned: 10.117, 9.748, 9.718, 9.735, 9.738. Release samples, baseline:
10.184, 9.867, 9.897, 9.948, 11.197; aligned: 10.110, 9.883, 10.482,
10.740, 11.943. The late release samples grew slower in both variants;
these measurements do not isolate CPU temperature or frequency effects.
They do establish that these trials do not support a robust gain.

All 24 outputs, including warm-ups, have MD5
`8cbfea47cfa8b4c8c3a2a22f9708019b`. Release `eval_rpn` starts at
`0xe74e0` without alignment and `0xe8000` with it, so the intended
alignment survived LTO. The difference is not a failed attribute.

Decline the current alignment patch. Controlled layout perturbations and
instruction-count checks would be necessary before accepting a future
placement or PGO proposal; the current candidate already fails the
prerequisite of a useful improvement across the normal and release builds.
Closing it does not claim that native code-layout sensitivity is solved.

## Adoption decision

| Card | Decision | Reason |
|---|---|---|
| W34 | Complete the evaluation and close; retain the vendored CVODE | No consistent speed/accuracy gain for current workloads; preserve the report and tolerance finding, without merging the adapter |
| W111 | Close the current alignment proposal as not planned | Small normal-build gain reverses under release/LTO; no benefit to the Wasm module |
| W9 | Close the full port as not planned; preserve prototype and reports | Maintainer decision after W184/W185: browser portability does not justify remaining persistence, error handling, AUTO, UI/file integration and maintenance for current priorities |

There is no supported combined speed improvement to adopt from these
three candidates. Further speed work should profile a representative mix
of evaluator-heavy and solver-heavy models, then measure a targeted
change against the existing native release build. SUNDIALS and Wasm
remain options when their capabilities justify their costs.
