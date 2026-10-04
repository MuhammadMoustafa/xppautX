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
