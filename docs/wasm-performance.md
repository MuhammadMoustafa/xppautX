# W9 Wasm performance investigation

[W184](https://github.com/MuhammadMoustafa/xppautX/issues/236), 2026-10-04.
This follows [the adoption assessment](performance-adoption.md). The Wasm
prototype and diagnostic tools remain on `task/W9-wasm`; no runtime or
numerical policy change is merged into master or included in v0.2.0.
The [exact-FMA follow-up](wasm-exact-fma.md) (W185) demonstrates a 13.8%
improvement by selectively compiling the existing software FMA at `-O3`,
with independent MPFR checks; it remains outside the production build.
Reproduction commits are `318da681` (investigation tools) and `6d0202e5`
(reviewed warm-up and runtime rounding witness), both on `task/W9-wasm`.

Maintainer decision after W184/W185: the full W9 port is declined as not
planned for current priorities. The prototype and evidence remain available.
See the [published evaluation](https://github.com/MuhammadMoustafa/xppautX/discussions/238).

## What the evidence establishes

Wasm is not uniformly slow for this core. An arithmetic-only model with
the same 100-equation evaluator and sum structure runs at essentially
native speed. The measured kuramot100 gap is dominated by correctly
rounded sine and its software fused multiply-add implementation. Porting
more UI or filesystem code would not remove that demonstrated cost.

Turning off live plotted-series delivery leaves most of the gap. More
aggressive compiler flags did not demonstrate a speed improvement on
the transcendental workload. This is a workload, runtime and numerical
implementation result, not a universal ratio for WebAssembly.

A separate hardware-assisted diagnostic removes most of the measured
penalty (3.658s to 1.430s). This demonstrates an optimization opportunity,
but its relaxed rounding semantics prevent production adoption. Keep W9
for browser portability; pursue an exact FMA implementation before making
a speed claim. W34 and W111 do not address this measured bottleneck.

## Method and controls

- Maintainer's x86_64 laptop, WSL-native checkout, emsdk 6.0.11 / LLVM 24,
  Node 24.19.0, native GCC 15. The native numerical owner dispatches to
  its hardware-FMA CORE-MATH build; Wasm uses the portable CORE-MATH build.
- Same W9 application core for every platform; numerical libraries copied
  verbatim. Builds use `-ffp-contract=off`; no fast-math or approximate
  transcendental replacements. The explicitly separate relaxed-FMA
  diagnostic is not part of this policy-preserving comparison.
- `kuramot100.odex` at total=10 for profiling and repeated comparisons.
  The existing full total=100 result is 10.609s native / 37.034s Wasm.
  `wasm-arithmetic.odex` preserves the 100-equation, user-function and sum
  structure but replaces the trigonometric `h(phi)` with `phi`.
- One warm-up per variant, followed by five alternating samples for the
  baseline and combined trial, or three for the separated flag trials.
  Timings exclude process/module startup and parameter configuration.
  One heavy local task at a time; make uses at most four jobs.
- Live-series off means `data` subscribes to no optional events during
  timing. The same final plotted series is fetched after timing and
  compared using `Object.is`, including negative-zero distinctions.
  Progress and command-completion events still operate normally.
- Every performance case also runs the existing lecar numerical,
  active-run abort and unsafe-name checks. Timings never decide pass/fail;
  no sleep-based synchronization was added.

## Computation versus delivery

Unprofiled warmed medians, seconds:

| Workload | Live series | Native | Wasm `-O2` | Wasm/native |
|---|---|---|---|---|
| kuramot100, total=10 | on | 1.145 | 3.812 | 3.33 |
| kuramot100, total=10 | off | 1.138 | 3.704 | 3.25 |
| arithmetic control, total=10 | off | 0.404 | 0.409 | 1.01 |

The on/off blocks ran sequentially; their small difference is not a
precise attribution of transport overhead because frequency/temperature
and ordinary variance were not independently controlled. The important
result is that disabling live delivery leaves a large computation gap.
Wasm's timed event count drops from about 242 to 41, and bytes from about
35,354 to 9,561. Native delivers about 18-19 events with series off.
The slower computation also emits more clock-paced progress events, so
event count alone does not identify the cause of the slower computation.

The arithmetic control refutes an unavoidable 3.5x penalty for this
evaluator. It does not isolate every opcode or establish near-native
performance for every solver/model. Its measured samples were native
413, 404, 412, 399, 398ms and Wasm 409, 395, 409, 415, 409ms.

## Worker CPU profile

The optimized baseline link retained function names with
`--profiling-funcs`, then Node's `--cpu-prof` captured the parent and eight
worker profiles. The profile includes one warm-up and five total=10 runs
with live series off, plus the short existing correctness fixtures.
Analysis de-duplicates recursive ancestor names within each sample.

| Frame | Sampled seconds | Share of sampled integration time |
|---|---|---|
| `xpp::integrate` inclusive | 22.593 | denominator |
| `xpp::math::sin` inclusive | 17.716 | 78.4% |
| software `fma` self | 11.979 | 53.0% |
| `cr_sin` self | 5.191 | 23.0% |
| `eval_rpn` self | 4.702 | 20.8% |

The FMA time is included in sine's inclusive time; these rows must not be
summed. Idle time across worker lifetimes is excluded from the integration
denominator. Sampled times are observations with profiler overhead, not
exact call counts or a mathematical proof of causality.

The source supports the attribution: kuramot100's `h(phi)` repeatedly
calls sine; CORE-MATH's sine uses explicit FMA for range reduction and
error compensation. Native selects the FMA-instruction version when the
CPU supports it. The SDK's musl `fma.c` implements the exact fused result
through integer significand multiplication, exponent alignment and
rounding on the portable Wasm path. The profile samples that function.

## Isolated numerical kernels

The benchmark uses the actual application's sine objects and numerical
header, two million binary-exact inputs in [0,1), five warm-ups and five
samples. Each result's binary64 bits feed an FNV-1a fingerprint. The
timings include loop, indirect-call and fingerprint overhead; they are
not pure instruction latencies.
There are 1,024 distinct kernel inputs, repeated to amortize overhead;
matching fingerprints establish agreement for this finite set, not
exhaustive correctness over all binary64 values.

| Kernel | Native median ms | Wasm median ms | Ratio | Fingerprint |
|---|---|---|---|---|
| Arithmetic | 6.148 | 11.607 | 1.89 | `a7e6b13e80453d25` |
| Correctly rounded sine | 28.717 | 192.821 | 6.71 | `b5706e6c8c5aee20` |
| Explicit FMA | 5.304 | 50.370 | 9.50 | `da23cfb980453d25` |

All three fingerprints match across platforms. The arithmetic microkernel
ratio includes overhead absent from the model-level comparison and is
not a substitute for that model's 1.01 ratio.

These are the final reviewed kernels, built with `-Wall -Wextra -Werror`
on both targets. The rounding witness reads volatile inputs so it is
evaluated by the runtime. Earlier one-warm-up runs showed considerable
variation: Node's sine median ranged from 74.709 to 188.967ms, including
visible acceleration within one sample sequence. Five warm-up passes
improve the protocol but do not guarantee a particular JIT tier or CPU
frequency. The application comparison and profile, rather than any one
microkernel ratio, support the main conclusion. Recorded samples, including
earlier runs, are retained in [the measurement data](wasm-performance-data.md).

## Policy-preserving compiler experiments

Each trial builds a separate module. No floating-point reassociation or
contraction is enabled. Medians are from the paired native controls in
each block, not from mixing the fastest values across different blocks.

| Wasm flags | kuramot100 native / Wasm (s) | Ratio | Arithmetic native / Wasm (s) |
|---|---|---|---|
| `-O2` | 1.138 / 3.704 | 3.25 | 0.404 / 0.409 |
| `-O3` | 1.038 / 3.930 | 3.79 | 0.382 / 0.379 |
| `-O3 -flto` | 1.088 / 5.326 | 4.90 | 0.415 / 0.474 |
| `-O3 -flto -msimd128` | 1.032 / 5.357 | 5.19 | 0.379 / 0.475 |

None is a demonstrated improvement on the target transcendental workload.
In these trials adding LTO to O3 worsens it; adding SIMD does not recover
that cost. This does not establish that LTO or SIMD always hurts Wasm,
or exhaust other flag combinations. Keep the current `-O2` build until
a different configuration demonstrates a useful gain on representative
workloads. The existing 106 math unit checks pass on both baseline and
the combined O3/LTO/SIMD libraries.

## Hardware-assisted diagnostic and correctness boundary

An isolated replacement of the C-library `fma` symbol with relaxed SIMD
does not alter the application's Makefile or select a production path.
It is an explicitly non-production diagnostic in `tools/wasmfmaexperiment.cpp`.

The decisive same-run comparison alternated native, baseline Wasm and
diagnostic Wasm, with one warm-up and five samples each:

| Variant | kuramot100 total=10 median (s) | Ratio to native |
|---|---|---|
| Native | 1.113 | 1.00 |
| Exact portable Wasm | 3.658 | 3.29 |
| Relaxed-FMA diagnostic | 1.430 | 1.28 |

The diagnostic reduces measured runtime by 60.9% (2.56x speedup versus
baseline Wasm). All 18 plotted-series comparisons in this same-run
experiment, including warm-ups, match exactly, and its twelve existing
checks pass. The diagnostic's separate 106 math unit checks also pass.
All three two-million-result kernel fingerprints match the baseline;
its final Node sine kernel median is 47.253ms versus portable 192.821ms,
and FMA is 11.662ms versus 50.370ms. Those microkernel blocks were separate;
the same-run model comparison is the stronger gain measurement.

This intervention strongly supports the profile's causal attribution:
accelerating FMA removes most of the large trig-workload penalty on this
runtime. It does not prove that the replacement is correctly rounded
for all inputs on every runtime, and it is not approved for adoption.

Ordinary multiplication followed by addition is not a replacement for
exact FMA. For a=1+2^-27, b=1-2^-27, c=-1, FMA returns -2^-54; separate
multiplication/addition returns zero. CORE-MATH needs this difference
for error-free transforms and correct rounding.

WebAssembly relaxed SIMD permits fused or unfused multiply-add depending
on the runtime/hardware. Even successful tests on this laptop cannot
establish the project's required cross-platform guarantee. See the
[proposal's semantics](https://github.com/WebAssembly/relaxed-simd/blob/main/proposals/relaxed-simd/Overview.md#relaxed-fused-multiply-add-and-fused-negative-multiply-add)
and [Emscripten's SIMD mapping](https://emscripten.org/docs/porting/simd.html).

Chrome 154 / V8 15.4 on Windows also matches all three fingerprints and
the runtime rounding witness for both kernels. Its final sine medians
were 199.345ms portable and 97.980ms diagnostic. Browser FMA and arithmetic
samples varied markedly, and the variants ran in separate blocks; these
are numerical and directional checks, not a fair browser speedup estimate.
The same-run Node application comparison remains the controlled estimate.

## Practical decision and remaining limits

W9 demonstrated optional browser portability. It already runs
some arithmetic models near native speed. For trigonometric-heavy work,
the current exact portable math path carries a substantial measured cost.
Do not expect porting more UI/file code, W34's solver replacement, or
W111's native alignment to remove it.

A worthwhile speed investigation targets the exact FMA/sine path, with
proof or validation appropriate to correctly rounded numerics. Batching
events may improve responsiveness and overhead on other workloads but
cannot explain the current large gap. Faster flags or approximate math
must not substitute for an exact numerical contract.

Application performance profiles and timings here are from Node/V8 under
WSL; isolated numerical kernels were also checked in Chrome on Windows.
This is not a cross-browser or cross-architecture study. The prototype's
browser-worker functional checks passed previously; application-level
browser timings, persistence, full UI/file workflows, AUTO, and exception
handling remain adoption gates. No general performance promise is made.

The review searched and reused `wasmcheck.mjs`'s series comparator and
session driver, `wasm/src/client.ts`'s event/menu waits, `wasm/core.js`'s
module startup and the `xpp_math.h`/CORE-MATH numerical owner. The
diagnostic's C ABI is needed because the vendored C objects call `fma`.
No numerical library was copied or modified. Benchmark paths are local
CLI inputs; no production endpoint or filesystem permission was added.
The existing unsafe-basename and active-abort checks still pass. The
arithmetic control is a benchmark fixture, not a solver implementation.

## Reproduction

Use the retained `task/W9-wasm` branch, a WSL-native checkout and the SDK
already installed under the home folder. The existing `tools/wslrun.sh`
syncs committed source before running commands on WSL's native filesystem.
The benchmark controls live in `tools/wasmcheck.mjs`; series extraction,
exact comparison and protocol/menu waits are reused rather than copied.
The math kernel calls the numerical owner; it does not reconstruct sine.
`tools/wasmprofile.py` reads Node profiles and checks kernel fingerprints.

```sh
source ~/emsdk/emsdk_env.sh
export EMCC_CORES=4
JOBS=4 tools/wasmcheck.sh
node tools/wasmcheck.mjs --bench --bench-total 10 --bench-rounds 5 --bench-series off
node tools/wasmcheck.mjs --bench --bench-total 10 --bench-rounds 5 --bench-series on
node tools/wasmcheck.mjs --bench --bench-model tools/models/wasm-arithmetic.odex --bench-total 10 --bench-rounds 5 --bench-series off
```

For a profile, relink with `--profiling-funcs` appended to
`WASM_LDFLAGS`, then invoke Node with `--cpu-prof --cpu-prof-dir=DIR`.
Use a fresh owned directory per profile run to avoid combining stale
samples. For compiler trials, use the same Makefile with `WASM=1`,
`CC=emcc CXX=em++ AR=emar`, separate `BUILDDIR`, and the listed `OPT`
flags; copy the existing client/glue into that module directory and
select it with `--wasm DIR`. Emscripten documents
[profiling without changing optimization](https://emscripten.org/docs/tools_reference/emcc.html#profiling-funcs)
and [its optimization pipeline](https://emscripten.org/docs/optimizing/Optimizing-Code.html).

Build the isolated kernels after the native and Wasm application objects
exist (native flags below select the x86 FMA build used for this study):

```sh
g++ -std=c++23 -O2 -Wall -Wextra -Werror -ffp-contract=off -mfma -DXPP_CORE_MATH_FMA -Icore tools/wasmmathbench.cpp build/obj/core_math_sin.o build/obj/core_math_sin_fma.o -lm -o build/native-math
em++ -std=c++23 -O2 -Wall -Wextra -Werror -ffp-contract=off -Icore tools/wasmmathbench.cpp build/wasm/core_math_sin.o -sENVIRONMENT=web,node -o build/wasm-math.js
build/native-math > build/native-math.log
node build/wasm-math.js > build/wasm-math.log
python3 tools/wasmprofile.py PROFILE_DIR build/native-math.log build/wasm-math.log
em++ -std=c++23 -O2 -ffp-contract=off -pthread -Icore tests/test_math.cpp build/wasm/libxppcore.a -sENVIRONMENT=node -sEXIT_RUNTIME=1 -o build/wasm-test-math.js
node build/wasm-test-math.js
```

For the non-production ceiling, compile `tools/wasmfmaexperiment.cpp` with
`em++ -O2 -mrelaxed-simd -c`. Append that object to the application's
`WASM_LDFLAGS` when linking a separate diagnostic module, and before the
libraries when linking the math kernels/unit checks. Do not replace the
baseline module: keep its compiled objects and glue in separate folders.
Run `--compare-wasm DIAGNOSTIC_DIR` with the baseline's `--wasm` directory
to alternate all three variants in one invocation. The override is never
enabled in the application's Makefile or selected by ordinary builds.
