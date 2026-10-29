# Terman episodic mismatch: controlled diagnosis (W239)

## Finding

The observed XPPAUT 8.0 versus xppautX mismatch is explained by the math-function implementation change: system libm versus xppautX's CORE-MATH bindings. In a disposable build of the pinned modern source, replacing only the expression `exp`/`pow` bindings and adaptive-step `pow` calls with system libm makes all 599,760 exported state values exactly equal to legacy XPPAUT. The modern parser, converted ODEX, solver algorithm, and inputs remain in place. No production code was changed and no parser/conversion bug was found by these controls.

This is equality at exported text precision, not a claim about internal double values or adaptive steps. It explains this recorded mismatch; it does not prove absence of other bugs.

## Controlled evidence

Both unchanged baselines reproduce the original screen's output SHA-256 hashes. Queried parameters and initial conditions agree for all 223 entries at query precision. Both use the same normalized SET file; its earlier label-only repair was already shown to preserve legacy output. Original files and original timing results remain unchanged.

All comparisons cover 4,998 output rows and 120 state variables, with matching time grids. The screening threshold is `5e-6 + 5e-5 * abs(left_value)`; it is a diagnostic threshold, not a scientific acceptance criterion.

| Modern build at tolerance 1e-3 | Maximum absolute difference from legacy | Values outside threshold |
|---|---:|---:|
| Unchanged CORE-MATH baseline | 3.116447 | 17,369 |
| System exp and pow in expressions only | 0.001633 | 3 |
| System pow in adaptive solver only | 3.116447 | 17,369 |
| System exp and pow in expressions, plus system pow in solver | 0 | 0 |
| System exp in expressions only | 5.006745 | 24,673 |
| System pow in expressions only | 8.284210 | 33,015 |

The expression and adaptive-step changes act in combination. The solver-only control does not eliminate the baseline discrepancy. Individual swaps need not reduce trajectory error monotonically.

Source anchors in the pinned modern revision: `core/expr_functions.cpp:126,151` binds the expression functions; the power operator uses the same binary-function table; `core/stiff.cpp:249,259` uses pow to choose QualRK step sizes. The diagnostic patch also replaces two pow calls in the stiff method, which this QualRK run does not exercise. `core/solver.cpp:98-106` passes the scalar tolerance to QualRK; the SET's separate absolute-tolerance field is not used by this solver. `core/xpp_math.h:35-48` documents the intentional correctly rounded math-library choice. See the existing W159/W163 entry in `docs/xppautx-vs-xppaut.md`.

## Which trajectory is correct?

CORE-MATH supplies a stronger per-function rounding guarantee. That does not establish greater accuracy of an entire adaptive ODE trajectory at a fixed tolerance. Legacy output is a compatibility reference, not ground truth.

The unchanged executables were run with only SET line 15 (QualRK scalar tolerance) changed:

| QualRK tolerance | Cross-program maximum absolute difference | Values outside threshold |
|---|---:|---:|
| 1e-3 | 3.116447 | 17,369 |
| 1e-5 | 0.000061 | 1 |
| 1e-7 | 0.000004 | 0 |
| 1e-9 | 0.000008 | 0 |

Cross-program agreement becomes very close at tighter tolerances. However, agreement is not convergence. Within each program, comparisons against its own 1e-9 run give:

| Tolerance compared with 1e-9 | Legacy maximum absolute change | Modern maximum absolute change |
|---|---:|---:|
| 1e-3 | 130.205005 | 130.205009 |
| 1e-5 | 111.121674 | 111.121674 |
| 1e-7 | 3.193924 | 3.193920 |

The 1e-9 run is a refinement reference, not a certified solution. Full pointwise convergence is not established. The maximum is pooled across state variables; it is not a normalized error norm or a biological validity criterion. We have not measured spike/burst timing, established chaos, validated the archived parameter mapping against the authors' intentions, or reproduced a published figure. An independent solver/reference and convergence of the scientific readouts would be needed before claiming scientific accuracy.

## Implications for the paper and benchmark

- Classify the original mismatch as sensitivity to the intentional math-library change, supported by the exact-output control, rather than a demonstrated conversion or solver bug.
- Retain CORE-MATH in production; matching legacy output alone is not a reason to reverse the reproducibility design.
- Keep the original Terman timings marked as not an equal-result comparison. A future comparison should predeclare accuracy/readout criteria, select tolerances using convergence evidence, and then rerun the five repetitions with matched settings. These single-run diagnostic timings do not replace the original mean/SD study.
- Report cross-program agreement, tolerance convergence, and scientific reproduction separately.

## Artifacts and reproduction

Pinned revisions, compiler/build commands, inputs and original timing methodology are in the parent bundle's `provenance.json` and README. `controls/provenance.json` confirms both baseline hashes match the original screen. `controls/loaded-values-comparison.json` records the parameter/IC check.

`investigate.py` reuses `screen.run_one` and `screen.compare`, varies only two files in a disposable build, saves the precise patches and build logs, and restores its source copies after completion. `refine.py` reuses those same helpers and the saved unmodified modern executable. `controls/` and `refinement/` retain all 13 valid diagnostic integration outputs (gzip), run logs, commands, output/binary hashes, settings and JSON comparisons. A separate read-only GPT-6-luna review checked the causal interpretation and its limits.

To reproduce in a fresh copy of the bundle (the scripts intentionally refuse to overwrite existing result directories):

```bash
uv run --locked python build.py --repo /path/to/xppautX
uv run --locked python terman-investigation/investigate.py --build-root /tmp/xpp-paper-build-XXXX
uv run --locked python terman-investigation/refine.py --build-root /tmp/xpp-paper-build-XXXX
```

Use the exact owned build directory printed by build.py. All solver execution/build work is on WSL's ext4 filesystem; saved artifacts are copied to the bundle outside the timing interval. The isolated build and run tree was removed after evidence was saved. No production code or manuscript changes were made.
