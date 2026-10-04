# W111: evaluator layout review

The existing alignment candidate is under review, not merged or included
in v0.2.0. Its original commits are `270e00a5` and `09db8396`; their
messages record earlier layout and compiler comparisons. Those results
need rechecking on current master and the optimized release build.

## Initial current-tip comparison, 2026-10-04

WSL GCC 15, normal `-g -O2` build with `WERROR=1` and
`-ffp-contract=off`. The baseline was built from `92f3682e`, and the
candidate differs in evaluator alignment. Three full
`examples/ode/kuramot100.odex --silent` runs per variant were alternated,
reversing order in the middle round. No other local heavy task ran.
Times are observations, never correctness assertions.

| Variant | Runs (seconds) | Median | Evaluator address |
|---|---|---|---|
| Master | 10.772, 10.022, 10.148 | 10.148 | `0x11a9c0` |
| Aligned | 10.836, 10.282, 10.019 | 10.282 | `0x11b000` |

All six outputs have MD5 `8cbfea47cfa8b4c8c3a2a22f9708019b`.
This sample does not demonstrate a gain; the early runs were slower in
both variants. A fixed function boundary also does not prove all timing
effects of unrelated code are removed.

## Before merging

- Warmed, alternated measurements and controlled unrelated-code padding.
- `RELEASE=1`/LTO performance and symbol alignment.
- Instruction counts compared on identical workloads.
- Numerical baselines and warning-as-error compiler gates.

Keep the candidate only if those measurements show a useful improvement
and reduced sensitivity to the tested layouts.

## W183 adoption decision, 2026-10-04

One warm-up and five alternating samples per variant gave normal-build
medians 9.901s baseline and 9.738s aligned (1.6% faster), but release/LTO
medians 9.948s and 10.482s (5.4% slower). All 24 outputs including warm-ups
had the MD5 above. Both release variants built with WERROR=1 and the
alignment survived LTO (`eval_rpn` at 0xe74e0 versus 0xe8000).

The current alignment proposal is declined: the small normal-build gain
does not carry into release. The late release runs slowed in both
variants, so this is evidence against a robust benefit, not a universal
regression claim. The prerequisite for adoption failed; further padding
and instruction-count experiments are not needed to decline this patch.
On W9's optimized build, adding the alignment produced identical Wasm
bytes. It does not prepare that build for a speed gain.

The implementation remains unmerged. See the
[joint assessment](https://github.com/MuhammadMoustafa/xppautX/blob/master/docs/performance-adoption.md)
for samples, CPU medians and the independent solver/platform decisions.
