# Faster exact FMA investigation

[W185](https://github.com/MuhammadMoustafa/xppautX/issues/237), 2026-10-04.
Follow-up to [the Wasm performance report](wasm-performance.md). Experiments
remain on `task/W9-wasm`; no numerical/runtime change is merged into master
or included in v0.2.0.
Reproduction tools are retained in `534942d6`; separate review commit
`856fded0` makes the browser wait for the current fixture's identity.

## Result and decision

There is a demonstrated improvement without relaxing FMA rounding.
Rebuilding the SDK's existing software FMA at `-O3`, while keeping the
application and CORE-MATH objects at `-O2`, reduces the measured integration
median from 3.569s to 3.075s: 13.8% less time, or 1.16x speedup. All independent
numerical checks and plotted-series comparisons pass. This improves the
software path; it does not give Wasm an exact hardware FMA instruction.

Prefer investigating this unchanged-algorithm build configuration over a
new bespoke FMA or approximate sine implementation. The arithmetic rewrite
tested below adds complexity and helps less. Neither candidate is adopted
in the application build: integration needs a pinned numerical-library
source and license, its owning build rule, representative models and a
cross-browser/architecture validation matrix. The measured candidate still
takes about 2.79x native time on the trigonometric model.

## What changed in each experiment

All experiments use emsdk 6.0.11 / LLVM 24, Node 24.19.0 under WSL, and the
same W9 application source as W184. The baseline numerical/core objects
and protocol glue are reused. The compiler keeps `-ffp-contract=off`;
there is no fast-math, reassociation, relaxed SIMD or approximate function.
One heavy task runs at a time; make and SDK workers are capped at four.

| Candidate | Change | Outcome |
|---|---|---|
| Baseline | SDK's linked software FMA | Reference |
| `original` | Compile SDK `fma.c` separately at `-O3` and override that C-library symbol | Best measured gain; same algorithm |
| `int128` | Same `-O3` build, replacing only the manual integer product with unsigned 128-bit multiplication | Passes checks; smaller gain |
| `wide` | Same integer rewrite, compiled with `-mwide-arithmetic` | Compiler supports it; Node and Chrome reject the module |

The SDK FMA source used here has SHA-256
`c0b849078fd9bf1db0acedaae646928819b758904b49986279c2237c58f48bf3`.
The experiment only copies it into ignored build storage. Parentheses are
added around existing shift counts to eliminate precedence warnings without
changing their meaning. Candidate objects build with `-Wall -Wextra -Werror`.
No vendored CORE-MATH source is edited, and ordinary builds enable none of
these overrides. The existing link warns about pthreads with growing Wasm
memory; that existing configuration is identical across the compared modules.

The integer rewrite preserves an exact operation: normalized significands
have at most 54 bits, so their product fits in 108 bits and hence in unsigned
128-bit storage. It replaces integer multiplication only; the original
normalization, alignment, cancellation, exceptional-value handling and
rounding remain. Without wide-arithmetic instructions, the compiler still
has to implement this integer operation using the older Wasm instruction set.

## Application measurements

Same-run comparisons use native, baseline Wasm and candidate Wasm, one
warm-up followed by five alternating samples per variant. They integrate
kuramot100 to total=10. Live series are disabled during timing; the same
final plotted data are fetched afterwards and compared bit for bit with
`Object.is`, including signed zero. Startup and configuration are excluded.
Timings are observations, never test thresholds.

| Candidate block | Native median (s) | Baseline Wasm (s) | Candidate Wasm (s) | Time reduction |
|---|---|---|---|---|
| Existing FMA at `-O3` | 1.104 | 3.569 | 3.075 | 13.8% |
| Integer rewrite at `-O3` | 1.062 | 3.616 | 3.256 | 10.0% |

Each block passes twelve existing lecar/protocol/abort/unsafe-name checks
and eighteen complete plotted-series comparisons, including warm-ups: 30
assertions per block. Each candidate also passes 106 math unit checks.
The existing two-million-result kernel fingerprints match W184's reference
for arithmetic, sine and FMA. Those kernels cover 1,024 distinct inputs;
their timings include hashing/call overhead and are secondary to the paired
application result. [The samples](wasm-exact-fma-data.md) record all timings.

Selective optimization of FMA is different from W184's whole-application
`-O3` trial: here the evaluator, solver and CORE-MATH objects retain their
baseline compilation. The local improvement does not contradict the earlier
failure of global flags to speed up the full program.

## Independent numerical reference

`tools/wasmfmaoracle.py` generates 51,825 vectors using MPFR 4.2.2:

- 13,824 combinations of 24 signed boundary values: zero, subnormals,
  the normal/subnormal boundary, values around one, largest finite values,
  infinities and quiet NaNs.
- 20,000 deterministic random triples of binary64 bit patterns.
- 18,000 cancellation cases: rounded products cancelled exactly and by
  the neighboring binary64 values.
- The explicit single-rounding witness `(1+2^-27)*(1-2^-27)-1`.

MPFR computes the operation at 8,192-bit precision, enough to represent
the exact finite binary64 product and sum even at the largest exponent
separation, then rounds once to binary64 nearest/ties-to-even. The reference
does not call any candidate FMA. Every non-NaN result is checked bit for bit,
including signed zero and infinity; NaNs are checked by classification.
NaN payloads and floating-point exception flags are outside this check.

Node passes all 51,825 vectors for baseline, `original` and `int128`, with
zero failures. Chrome 154 / V8 15.4 on Windows independently passes the
same vectors for all three. Both reject `wide` during `WebAssembly.validate`.
The browser uses the same comparison owner as Node; it waits for the current
fixture's identity, never a delay. No sleep or speed-based assertion was added.

This finite sample is strong regression evidence, not an exhaustive proof
over all input triples or a substitute for the library's correctness analysis.
The unchanged-algorithm candidate is preferable precisely because it changes
the build configuration rather than the mathematical contract.

## Other routes and limits

Wide arithmetic provides exact integer instructions that could accelerate
software FMA. LLVM here emits `i64.mul_wide_u`, but the actual Node and Chrome
runtimes reject the candidate. It remains a future target rather than a
portable default. The current [proposal list](https://github.com/WebAssembly/proposals)
places it in phase 4; the [proposal](https://github.com/WebAssembly/wide-arithmetic/blob/main/proposals/wide-arithmetic/Overview.md)
defines its integer semantics. This is a different feature from relaxed FMA.
No Firefox, Safari or ARM measurements were made in this investigation.

LLVM libc's [generic FMA](https://github.com/llvm/llvm-project/blob/main/libc/src/__support/FPUtil/generic/FMA.h)
also uses exact significand arithmetic and rounding support. It is a library
implementation with supporting headers, not an instruction that bypasses
Wasm's limitation. It was inspected, not ported or benchmarked; no performance
claim is made for replacing musl with it.

CORE-MATH's existing sine calls FMA for an exact product residual, range
reduction and small-input handling. Its source comments tie the rounding
test to a correctness proof. Replacing those calls by separate multiply/add
would invalidate the assumed residuals. A different correctly rounded sine
algorithm remains possible, but requires its own error bounds and hard-case
validation. No drop-in replacement was established here.

## Reproduction and review

Use `task/W9-wasm`, commit source before `tools/wslrun.sh`, and run from its
WSL-native clone, with the installed emsdk and MPFR runtime:

```sh
JOBS=4 EMCC_CORES=4 tools/wasmcheck.sh
bash tools/wasmfmaexperiment.sh
```

The script generates candidates and MPFR vectors in `build/w185`, verifies
them, links separate application modules, runs math checks and prints matched
timings. Its exit trap restores the baseline module. The restored baseline
Wasm SHA-256 is
`d8c807e79d97c486769aa29659ab0c62f125356f4153ef85db24303b5efab672`.

For Chrome verification, copy `build/w185/fma-*.js`, `fma-*.wasm` and
`vectors.json` into `build/wasm`, then run `node tools/wasmfmapagecheck.mjs`
on the host with Chrome. It reuses `cdp.mjs` and `wasm/serve.mjs`, uses a
temporary owned profile, and closes its server and browser by its own PID.

Review searched the numerical owner and existing FMA/error-free-transform
call sites before writing experiments. `wasmcheck.mjs` still owns application
measurement and series equality, `wasmmathbench.cpp` owns isolated kernels,
and `wasmfmafixtures.mjs` owns the new Node/browser FMA comparison. No second
protocol parser or sine algorithm was introduced. Local SDK/fixture paths
are experiment inputs; no production trust boundary or endpoint changed.

Master changes are documentation only. The release and production runtime
remain unchanged. These experiments establish a promising exact optimization
candidate, not a completed production Wasm port or universal speed guarantee.
