# AUTO collocation workers (W247, [#303](https://github.com/MuhammadMoustafa/xppautX/issues/303))

The settled design is W251 / Discussion #309. Its ignored prototype is in
`paper/benchmarks/auto-parallel/` in the main checkout. The integrators,
boundary conditions, integral constraints, reduction and back substitution
remain on the calling thread.

`compile_model` records helper purity and variable reads, including the
transitive dependencies of user functions. Every equation, fixed variable
and derived parameter used by AUTO must have native code. Only table lookup
and integer shift are allowed native helpers: lookup reads the table without
writing it, and integer shift uses no Session. Networks, DAEs, Markov state,
kernels, function tables, random draws, delay, variable/constant shift and
SET exclude the model. Function tables are excluded even with autoeval off:
the setting can change, and their normal AUTO path rebuilds parameter-dependent
values. A fixed quantity cannot read a later fixed quantity; derived
parameters cannot read variables. Reading SUM's leftover index outside a sum
also excludes a program. Discrete maps with `store_every > 1` remain serial.

`AutoLib::collocation` owns one persistent pool; its thread count includes
the caller, which runs the first chunk. The default is hardware
concurrency, capped at four (W251 measured no benefit beyond four on PY_S1Bf),
or one when hardware concurrency is unknown. Explicit counts exist only in
the internal C++ constructor for tests; this card adds no environment switch,
protocol setting or UI.

Static contiguous chunks compute setubv's AA/BB Jacobians and FA residuals.
The same original numerical loops serve both serial and parallel execution.
Each worker owns an AutoLib with its finite-difference arrays, constants and
variables. Extended RHS wrappers reach scratch through `iap->lib`, so they
also use the worker's arrays. HomCont RHS settings are copied and rotation
counts are read-only; their boundary updates stay serial. `funi`'s temporary
vectors, collocation weights, parameter perturbations and per-chunk buffers
are local. Shared mesh inputs are read-only; output blocks are disjoint.
Scratch audit: none left shared and mutable on workers.

After an RHS section, the calling thread publishes the final interval's
constants and variables, preserving the serial Session state, including SUM's
index. Condensation records each subtraction from the shared D block and
replays it in interval/elimination/row/column order; it never sums partial
results or reassociates arithmetic. Workers catch every exception into their
own failure slot. The calling thread waits for all chunks and reports the
failure through AUTO's existing command error path. Workers never poll Stop,
call the UI or log; cancellation is checked on the caller between sections.

The ownership search covered `xpp_mem`, `xpp_error`, `xpp_util`, Model/Session,
`expr_native`'s helper list, `my_rhs`, `derived`, `tabular`, `auto_jacobian`,
autlib3/autlib5 wrappers and all setubv/conpar accesses. Reused: native Program
calls, the original mesh algorithms, `allocate_global_memory`, AUTO's central
and forward difference helpers, derived evaluation order, the Session error
renderer, and the existing full-diagram comparison in `test_session_auto`.
RHS evaluation order now has one implementation for Session and worker arrays.
Native input validation remains in the existing compiler; the purity analysis
uses validated instruction indices, and model input cannot choose a thread
count or introduce generated C text. Chunk boundaries use quotient/remainder
arithmetic so even a maximal mesh count cannot overflow during partitioning.
The Model builder already limits derived quantities to preceding dependencies;
workers never treat a recurrence through derived constants as pure. Valid
costly expressions and continuations still consume CPU as on the serial path;
cancellation is observed between sections. No new trust boundary is introduced.

`test_session_auto` compares every field of all diagram points, including
parameters, orbit statistics and eigenvalues, for interpreted serial and
compiled 1/2/4-thread periodic continuation. It also compares final evaluation
arrays, checks serial selection for an impure model, transitive helper
impurity, refused recursive code, forward fixed reads, SUM state, function
tables, maps with multiple stored steps, pool recovery after a failure, and
contiguous partitioning at the maximum `long` without allocating mesh data.

The test executable can measure the W246 run without adding a product setting:

```sh
build/ucrt/tests/test_session_auto.exe /c/gitRepos/xppautX/paper/benchmarks/auto/models/PY_S1Bf_bench.odex 1
build/ucrt/tests/test_session_auto.exe /c/gitRepos/xppautX/paper/benchmarks/auto/models/PY_S1Bf_bench.odex 2
build/ucrt/tests/test_session_auto.exe /c/gitRepos/xppautX/paper/benchmarks/auto/models/PY_S1Bf_bench.odex 4
```

It runs the equilibrium branch, grabs the first HB, then times the periodic
continuation. Every AUTO setting comes from W246's model. It writes the full
diagram to `build/w247-diagram-N.csv` for byte comparison. Timings are reported
and never used as pass/fail criteria.

Measured on Windows/UCRT GCC 16.1, `-g -O2 -ffp-contract=off`, 2026-10-10
(AMD Family 25 Model 97, 32 logical processors). One fresh process per count;
the measured interval is periodic continuation alone, after the equilibrium
branch and HB grab, as in W246.

| Threads (caller included) | Periodic wall seconds | Speedup over one |
|---:|---:|---:|
| 1 | 5.034313 | 1.00 |
| 2 | 3.126176 | 1.61 |
| 4 | 2.093092 | 2.41 |

Each computes 403 equilibrium + 336 periodic points. All three full diagram
exports have SHA256
`7a7a265ec74c6a3b5d53c8867d9b37c337044989bba6c3823d0f5b6d9c9b2477`.
These timings include the calling thread's condensation replay and all
remaining serial AUTO work, not just the parallel sections. The tiny Hopf
unit model is too small to benefit from dispatch; its timing is printed,
never asserted.

For the threads-off comparison, master `88d63b42f24cabae9041c4b6ab57bd498af20713`
was archived into `build/w247/master/` and built with the same UCRT flags and
`-j4 WERROR=1`. `build/w247/serial_timing.py` runs lecar's Hopf parameter set,
sets its fixed point as the initial condition, computes the equilibrium
branch, grabs HB and times 2,000 periodic points with `--no-compile`. It uses
`tools/xppclient.py`, private scratch directories, one excluded warm-up per
binary and five measured fresh processes per binary, alternating order.

Median wall times: master **2.519546 s**, W247 **2.534349 s** (+0.59%). The run
ranges overlap (master 2.517113–2.618664 s, W247 2.516208–2.566044 s); these
samples do not establish a slowdown. All ten plotted diagrams match, SHA256
`f1bbfb40b5860521f947e5a1c65af6a71f49ad90b39cc83d60a8f6e305a4ea2b`.
The comparison uses a freshly built pinned master, rather than the older
binary in the main checkout. Raw samples are in
`build/w247/serial-periodic-timing.json`; build and check logs are in
`build/w247/`.

The final UCRT build has zero compiler warnings and all 36,615 unit checks
pass (37 report groups; 161 in `session_auto`). All 199 AUTO checks and all
five retained foreign-model conversion checks pass. Web2's typecheck,
dupcheck, deadcheck, aliascheck, errorcheck, sessioncheck and sleepcheck also
pass; no checker baseline or sanitizer suppression was changed. The reviewer
runs WSL verify and ThreadSanitizer; this task did not run either.

`tools/examples_check.sh --bin xppautX.exe` passes: 184 models, all 183
outputs match the unchanged `tests/examples.md5`; `ml-range.odex` writes
no output by design. Heavy commands ran sequentially, with `-j4` builds
and `JOBS=1` for the model sweeps. No background process was started, and
the changes are left uncommitted as the card brief requires.

One default-temp AUTO sweep reported two directory-count/cleanup failures.
Its new server PID 30640 collided with a pre-existing
`xppautoX-30640-1/one` fixture dated 15:31, before this sweep; diagram and
orbit checks passed. No existing temp file was removed. The full rerun used
a fresh workspace-owned `build/w247/gates-tmp/` for `TMPDIR`, `TMP` and
`TEMP`, and passed all 199 checks. The original failure is retained in
`build/w247/autocheck-final.log`; the isolated result is
`build/w247/autocheck-isolated.log`. This is recorded as an environment
collision, rather than silently treating the original failures as passes.
