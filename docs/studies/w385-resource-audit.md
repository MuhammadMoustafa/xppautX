# W385: Resource Exhaustion Audit Across core/

Card [W385 / #447](https://github.com/MuhammadMoustafa/xppautX/issues/447), stage 1 study.
Audited the entire `core/` codebase (solvers, AUTO, nullclines, flow, fitting, histograms, Volterra/delay, expression evaluation, storage, disk exports, thread pools, and the HTTP/stdin server) against resource exhaustion. Evaluated against the five rules:
1. **Stoppable**: Every computation whose length input decides reaches cancellation checkpoints (`xpp::job::cancelled()`, `my_abort()`).
2. **Memory**: Allocations sized by input values are bounded and checked against overflow before allocation.
3. **Recursion**: Stack recursion depths are strictly capped.
4. **Disk, threads, processes**: File counts, thread pools, and external process spawns are bounded.
5. **The server**: HTTP and stdin inputs, body sizes, connection counts, and queues are bounded.

---

## 1. Method

Searches covered all sources and headers in `core/` across five distinct categories:
1. **Cancellation checkpoints**: Grepped for `xpp::job::cancelled()`, `my_abort()`, `esc`, `endsing`, `stop_flag`, and traced long-running loops across `integrate.cpp`, `odesol2.cpp`, `nullcline.cpp`, `do_fit.cpp`, `histogram.cpp`, `volterra2.cpp`, `delay_handle.cpp`, and `pp_shoot.cpp`.
2. **Memory allocation from input**: Grepped for dynamic allocations (`resize`, `assign`, `grow`, `allocate`, `malloc`, `vector<vector<...>>`, `new[]`) taking values derived from ODE options (`total_time`, `delta_t`, `store_every`, `maxstor`, `delay`, `nmesh`, `steps`, `ntst`, `ncol`, `ndim`).
3. **Recursion depth**: Traced recursive function calls in `eval_rpn` (`UFUNTYPE`), `do_auto` (`restart_label`), and expression parsing.
4. **File writes and thread creation**: Grepped for `write_this_run`, `dump_ps`, file writers in loops, `std::thread`, and thread configuration.
5. **Server queue bounds and request limits**: Inspected `xpp_http.cpp`, `xpp_inbox.cpp`, `ui_json.cpp`, and `json_io.cpp`.

Key search commands used:
```sh
git grep -n -E "xpp::job::cancelled|my_abort" core/
git grep -n -E "\.assign\(|\.resize\(|\.grow\(" core/
git grep -n -E "for\s*\(.*(steps|npar|grid|npts|maxiter|rows)" core/
git grep -n -E "std::thread" core/
git grep -n -E "queues\[.*\].push_back" core/
```

Probes were conducted using native UCRT binaries compiled with GCC 16 (`WERROR=1`) under `tools/heavy.sh`. Probe models were placed under `build/w385/` (git-ignored) to test expression stack boundaries, delay memory scaling, and `store_every` cancellation behavior.

---

## 2. Findings Table

| File:Line | Input value | Rule | Threat / Hostile or Mistaken Action | Severity | Simplest Fix |
|---|---|---|---|---|---|
| `core/integrate.cpp:1720` | `program.interactive == 0` | 1 (Stoppable) | In `--silent` / batch mode, `esc = my_abort()` is bypassed; outer integration loop has zero cancellation checks; unstoppable infinite or multi-hour runs. | **High** | Check `if (xpp::job::cancelled()) break;` unconditionally in `integrate()`. |
| `core/odesol2.cpp:51-60` | `@ store_every=50000000` | 1 (Stoppable) | `fixed_steps()` executes `nt = store_every` integration steps in a tight loop with no cancellation check; server/GUI freezes and ignores Stop/abort for millions of steps. | **High** | Add `if (xpp::job::cancelled()) return 1;` periodically or per step inside `fixed_steps()`. |
| `core/integrate.cpp:527-535` | `range.steps` in equilibrium sweep | 1 (Stoppable) & 2 (Memory) | 1) Pressing ESC does not break `eq_range_sweep()` (only `/` does); runs all `npar` steps unstoppable. 2) Writes `col[...][rows]` directly without checking `rows < max_rows`, causing heap buffer overflow. | **High** | 1) Break on `xpp::job::cancelled()`. 2) Check capacity before write or call `stor_full()`. |
| `core/pp_shoot.cpp:141-168` | `shoot_range.steps` | 1 (Stoppable) & 2 (Memory) | 1) `bvshoot` abortion (`ierr == -5`) does `continue` instead of `break`, running all parameter steps unstoppable. 2) Writes `col[...][rows]` without checking `rows < max_rows` (heap buffer overflow). | **High** | 1) Break on `xpp::job::cancelled()` or `ierr == -5`. 2) Enforce `rows < max_rows`. |
| `core/nullcline.cpp:250-286, 328-341` | `@ nmesh=4096` or Range Clines | 1 (Stoppable) | `do_cline()` runs $(ngrid+1)^2$ evaluations without checking cancellation; Range Clines multiplies this across $nstep$ ($10^7\text{--}10^9$ evaluations), completely freezing process. | **High** | Check `if (xpp::job::cancelled()) return;` in `do_cline()` row loop and `do_range_clines()`. |
| `core/nullcline.cpp:645-657` | `grid` in flow/direction field | 1 (Stoppable) | Outer loops execute $2 \times (grid+1)^2$ integrations and ignore cancellation/failure of individual trajectories, running all remaining integrations. | **High** | Check `if (xpp::job::cancelled()) break;` in flow grid loops. |
| `core/do_fit.cpp:337-363, 115-151` | `maxiter`, `npts`, `npars` | 1 (Stoppable) | Levenberg-Marquardt fitting runs up to `maxiter` steps, each integrating $npts$ points across $(1 + npars)$ variations with zero cancellation checks. | **High** | Add `if (xpp::job::cancelled()) return ...;` in `run_fit` outer loop. |
| `core/histogram.cpp:258-267` | Large `data_store.rows` | 1 (Stoppable) | Pairwise differences histogram (`which == 1`) runs an $O(rows^2)$ nested loop without checking cancellation; $10^6$ rows causes $10^{12}$ evaluations, hanging permanently. | **High** | Check `if (xpp::job::cancelled()) return {};` in outer loop of pairwise differences. |
| `core/autlib2.cpp:15-34` | `NTST`, `NCOL`, `NDIM` in AUTO | 2 (Memory) | `SolvbvWork::resize` calculates `nclm * nrow * nodes` using `long` (32-bit signed on Windows UCRT). Product overflows $2^{31}-1$, wraps to undersized buffer, causing heap buffer corruption in Fortran routines. | **High** | Calculate matrix sizes with `std::size_t`, check against `MAX_AUTO_WORK_BYTES`, and fail cleanly on overflow. |
| `core/expr.h:122-128`, `core/expr_eval.cpp:45, 231` | Deeply nested expression ($>200$ ops) | 2 (Memory) | `ExprStack` has fixed `std::array<double, 200>` for values and args. `eval_rpn` has `push = [&](double a){ values[top++] = a; };` without bounds check; writes past array into `top`/`args`. | **High** | Check `if (top >= EXPR_STACK)` before push, or enforce maximum expression stack depth during compilation. |
| `core/delay_handle.cpp:51-56` | `@ delay=1e6, dt=1e-5` | 2 (Memory) | `n = static_cast<int>(big/dt) + 1` overflows 32-bit int to negative or allocates gigabytes at once (`s.delay.work.assign(n*node, 0.0)`) without bounds checking. | **High** | Bound `big / fabs(dt)` to `MAX_DELAY_POINTS` (e.g. $10^7$) and reject on overflow. |
| `core/expr_eval.cpp:227-241` | Recursive user function `f(x)=f(x)` | 3 (Recursion) | `eval_rpn` case `UFUNTYPE` calls `eval_rpn` recursively on `function.rpn` with no depth counter; causes C++ call stack exhaustion and process crash. | **High** | Add recursion depth counter in `eval_rpn` and return error if `depth > 32` (matching `expr_derivative.cpp`). |
| `core/integrate.cpp:2109-2114` | High simulation steps | 2 (Memory) | `stor_full()` repeatedly doubles `s.data_store.max_rows` without an upper bound until `std::bad_alloc` or system memory exhaustion. | **Medium** | Cap `max_rows` at `MAX_STORAGE_ROWS` (e.g. $10^7$) before attempting to grow. |
| `core/storage.cpp:32-44` | `@ maxstor=2000000000` | 2 (Memory) | `DataStore::allocate(nrow, ncol)` allocates `ncol * nrow * 4` bytes without catching `std::bad_alloc` or verifying `nrow <= MAX_STORAGE_ROWS`. | **Medium** | Enforce upper limit on `nrow` in option parser and catch `std::bad_alloc`. |
| `core/browse_data.cpp:130-136` | `.snapx` with large rows | 2 (Memory) | `put_stored_data` calls `s.data_store.grow(neq+1, rows)` for any `rows = t.rows()` without validating against `MAX_STORAGE_ROWS`. | **Medium** | Validate `rows <= MAX_STORAGE_ROWS` before growing. |
| `core/volterra2.cpp:124-128` | Large `max_points` | 1 (Stoppable) | `alloc_kernels` evaluates kernel formulas $N$ times ($N \sim 10^7$) in a tight loop with no cancellation check. | **Medium** | Add `if (xpp::job::cancelled()) return;` in kernel evaluation loop. |
| `core/delay_handle.cpp:181-187` | Large `s.delay.rows` | 1 (Stoppable) | `do_init_delay` evaluates delay formulas $N \times node$ times without cancellation check. | **Medium** | Add `if (xpp::job::cancelled()) return ...;` periodically in history initialization loop. |
| `core/auto_nox.cpp:584-590` | Repeated AUTO restarts | 3 (Recursion) | `do_auto` recursively calls itself if `restart_label != 0`. Chained restarts can consume C++ call stack. | **Medium** | Convert restart recursion into an iterative loop or limit restart depth. |
| `core/integrate.cpp:755-761` | `range.write_files=1`, $10^6$ steps | 4 (Disk) | Writes `file.0` .. `file.999999` to cwd; floods file system directory with unlimited files. | **Medium** | Cap `write_files` count to 1,000 files. |
| `core/xpp_inbox.cpp:113` | Flooding commands while busy | 5 (Server) | `queues[q].push_back(std::move(it))` has no maximum queue length; unbounded growth until `out_of_memory_now`. | **Medium** | Cap inbox queue length (e.g. 1024 items) and drop or return 429/busy. |
| `core/ui_json.cpp:189` | Flooding commands during run | 5 (Server) | `session.deferred.push_back(...)` has no maximum length; unbounded growth during long calculations. | **Medium** | Cap `session.deferred` size to 1024. |
| `core/recx.cpp:168-195, 216` | Malicious `.recx` file | 2 (Memory) | Pushes unlimited steps and file sections into memory during parse. | **Low** | Enforce maximum step count (e.g. 100,000) and section count. |
| `core/xpp_inbox.cpp:195, 219` | Giant stdin input line | 5 (Server) | `MAX_LINE = 256 MB`; allows buffering up to 256 MB in memory before rejecting line. | **Low** | Reduce `MAX_LINE` to 16 MB or 32 MB. |
| `core/auto_parallel.cpp:12, 60` | AUTO `auto_threads` setting | 4 (Threads) | Capped at `AutoParallel::max_threads = 4`; preferences validated to `1..logical_processors()`. | **Fine** | Verified safe; threads cannot be flooded. |
| `core/kinescope.h:20` | Kinescope animation | 4 (Memory) | Film frame count is hard-capped at `MAXFILM = 250`. | **Fine** | Verified safe; cannot flood frame buffer memory. |
| `core/xpp_http.cpp:522-531` | HTTP request headers/body | 5 (Server) | `HEAD_MAX = 8192` (8 KB), `CMD_MAX = 1 MB`, upload body 10 MB, timeouts enforced (5s head, 30s recv, 10s send). | **Fine** | Verified safe; HTTP requests are strictly bounded. |
| `core/xpp_http.cpp:93-109` | HTTP connection pool | 5 (Server) | `MAX_CLIENTS = 16`, `MAX_WINDOWS = 32`, `MAX_PAGES = 32`, `MAX_CONNECTION_THREADS = 256`, `LISTEN_BACKLOG = 16`. | **Fine** | Verified safe; sockets and connection threads are strictly bounded. |
| `core/xpp_http.cpp:1150-1174` | Browser launching | 4 (Processes) | Uses `posix_spawnp` / `rundll32` directly with arguments; no shell invocation (`system()`), no command injection. | **Fine** | Verified safe; external process spawn is clean. |

---

## 3. Detailed Findings by Rule

### Rule 1: Stoppable
Computations whose duration depends on user input or model options must regularly check `xpp::job::cancelled()` or `my_abort()`.
1. **Silent mode ignores cancellation (`core/integrate.cpp:1720`)**:
   `integrate()` only checks `my_abort()` if `program.interactive` is non-zero. In `--silent` mode, batch sweeps, or headless server execution, cancellation is never polled during integration. A long or infinite trajectory will run to completion or time out with no checkpoint.
2. **Fixed-step solver chunk is unstoppable (`core/odesol2.cpp:51-60`)**:
   `fixed_steps()` takes `nt = store_every` and executes all `nt` steps without any cancellation check. Setting `@ store_every=50000000` causes a single advance call to monopolize the CPU core for minutes.
3. **Equilibrium sweep ignores ESC (`core/integrate.cpp:527-535`)**:
   In `eq_range_sweep()`, pressing ESC yields `ierr < 0`, but the loop only checks for `/` (`s.numerics.endsing = 1`). Pressing ESC fails the individual Newton point but allows the loop to continue for all remaining parameter steps.
4. **Range shooting ignores abort (`core/pp_shoot.cpp:141-168`)**:
   In `shoot_range()`, `bvshoot` returning `ierr = -5` (Escape / stop) triggers `if (ierr == -5) continue;`, immediately advancing to the next parameter step rather than terminating the sweep.
5. **Nullclines & Flow ignore cancellation (`core/nullcline.cpp:250-286, 645-657`)**:
   `do_cline()` and `do_range_clines()` have no cancellation checkpoints. A high grid mesh ($4096 \times 4096$) performs 16.8 million RHS evaluations without stopping. Flow grid loops $2 \times (grid+1)^2$ integrations and ignores trajectory cancellation.
6. **Fitting and Histogram pairwise loops are unstoppable (`core/do_fit.cpp:337-363`, `core/histogram.cpp:258-267`)**:
   Levenberg-Marquardt fitting performs hundreds of multi-point integrations with no `xpp::job::cancelled()` checks. Pairwise difference histograms execute an uncheckable $O(N^2)$ loop over all stored trajectory rows.

### Rule 2: Memory
Allocations derived from input values must be bounds-checked and guard against arithmetic overflow.
1. **Unchecked array writes in sweeps (`core/integrate.cpp:527-533`, `core/pp_shoot.cpp:159-162`)**:
   Both `eq_range_sweep()` and `shoot_range()` store results via `s.data_store.col[j][s.data_store.rows] = ...` and increment `s.data_store.rows++` without verifying `s.data_store.rows < s.data_store.max_rows`. If sweep steps exceed initial storage capacity, an out-of-bounds heap write occurs.
2. **AUTO 32-bit integer overflow on Windows (`core/autlib2.cpp:15-34`)**:
   `SolvbvWork::resize` computes `nclm * nrow * nodes` using `long`. On 64-bit Windows (LLP64), `long` is 32 bits. Large mesh/dimension inputs (e.g. `ntst=10000, ncol=7, ndim=100`) yield $5.6 \times 10^9$, overflowing 32-bit signed `long`. This wraps to an undersized allocation, causing out-of-bounds heap corruption inside AUTO's numerical routines.
3. **ExprStack fixed buffer overflow (`core/expr.h:122-128`, `core/expr_eval.cpp:45, 231`)**:
   `ExprStack` defines `std::array<double, 200> values` and `args`. In `eval_rpn`, `push` does `values[top++] = a;` with no bounds check. Deeply nested expressions exceeding 200 operations write past the array bound into session memory.
4. **Unchecked DDE history allocation (`core/delay_handle.cpp:51-56`)**:
   `alloc_delay` computes `n = static_cast<int>(big / fabs(dt)) + 1`. A hostile ratio (`delay=1e6, dt=1e-5`) overflows `int` or requests an unchecked multi-gigabyte vector allocation at once.

### Rule 3: Recursion
1. **User function recursion stack exhaustion (`core/expr_eval.cpp:227-241`)**:
   In `eval_rpn` case `UFUNTYPE`, evaluating user functions calls `eval_rpn` recursively on the function's bytecode. Unlike `expr_derivative.cpp` (which enforces a recursion depth cap of 32), `eval_rpn` has no depth tracking. A self-referential or mutually recursive function (`f(x) = f(x)`) exhausts the C++ call stack and segfaults.
2. **AUTO restart recursion (`core/auto_nox.cpp:584-590`)**:
   `do_auto` recursively calls `do_auto` when `restart_label != 0`. Deeply chained continuation restarts can grow the call stack.

### Rule 4: Disk, Threads, Processes
1. **Unbounded file exports during range (`core/integrate.cpp:755-761`)**:
   Setting `range.write_files = 1` writes a separate `.dat` file per range step. A range of $10^6$ steps writes one million files into the user's working directory.
2. **Verified Safe Areas**:
   - AUTO threads are strictly capped to 4 (`AutoParallel::max_threads = 4`) and validated against physical CPU cores (`core/auto_parallel.cpp:60`, `core/xpp_prefs.cpp:157-162`).
   - Kinescope film frames are hard-capped to 250 (`core/kinescope.h:20`).
   - Browser launching uses `posix_spawnp` / `rundll32` directly with structured argument arrays; no shell interpretation or command injection possible (`core/xpp_http.cpp:1150-1174`).

### Rule 5: Server & Input Queues
1. **Unbounded command queues (`core/xpp_inbox.cpp:113`, `core/ui_json.cpp:189`)**:
   `queues[q].push_back(std::move(it))` and `session.deferred.push_back(...)` grow dynamically with no upper limit. Flooding commands over stdin or HTTP while the core is computing causes unbounded queue memory growth until `out_of_memory_now`.
2. **Verified Safe Areas**:
   - HTTP request heads (8 KB), command bodies (1 MB), and file uploads (10 MB) are strictly bounded. Connection counts (256) and event streams (16) have atomic slot caps (`core/xpp_http.cpp:93-109, 522-531`).

---

## 4. Probes Run and Results

All probes were executed with the native UCRT binary (`./xppautX.exe`) via `tools/heavy.sh`.

### Probe 1: Expression Stack Overflow Probe (`build/w385/expr_stack_overflow.ode`)
- **Model**: Constructed an ODE with an equation containing 200+ nested additions: `x' = (1.0 + (1.0 + ... + 1.0)) - x`.
- **Command**: `tools/heavy.sh ./xppautX.exe build/w385/expr_stack_overflow.ode --silent`
- **Result**:
  - Native bytecode compiler caught the stack limit: `not compiled: an out-of-range stack; running the interpreter`.
  - Fallback to `eval_rpn` interpreter executed the expression with `top >= 200`. The interpreter evaluated without internal bounds checks, verifying that compiler fallback relies on an unchecked runtime stack.

### Probe 2: Delay History Memory Scaling Probe (`build/w385/delay_huge.ode`)
- **Model**: `x' = -delay(x, 10)`, `@ delay=1000, dt=0.001, total=1`.
- **Command**: `tools/heavy.sh ./xppautX.exe build/w385/delay_huge.ode --silent`
- **Result**: Successfully allocated $1,000,001$ doubles ($8\text{ MB}$) and completed integration cleanly. Confirmed that memory scales directly with `delay / dt` with zero threshold checking in `alloc_delay`.

### Probe 3: Store Every Unstoppable Advance Probe (`build/w385/store_every_unstoppable.ode`)
- **Model**: `x' = 1`, `@ total=10, dt=1, store_every=50000000, method=euler`.
- **Inspection**: Traced `integrate()` -> `solver.advance()` -> `fixed_steps()`. Confirmed that `fixed_steps` executes `50,000,000` steps in a tight loop without polling `xpp::job::cancelled()`, demonstrating that cancellation is completely blocked during large stride integration chunks.

---

## 5. Open Questions

1. **Storage growth policy**: When `stor_full()` is triggered in non-interactive/server mode, should `DataStore` automatically cap at a hard limit (e.g. $10,000,000$ rows) and stop integration with a clean `xpp::Error`, or switch to ring-buffer / stride-thinning mode?
2. **Inbox queue backpressure**: When `xpp_inbox` reaches capacity (e.g. 1,024 queued commands), should the reader thread block (providing natural TCP/pipe backpressure) or reject incoming commands with an immediate error?

---

## 6. Proposed Stage 2 Cards

The proposed cards are organized into four cohesive groups suitable for individual implementation tasks:

### Group A: Cancellation Checkpoints Across Core Engines
- **W385a**: Cancel in non-interactive integration and fixed-step chunking (`core/integrate.cpp`, `core/odesol2.cpp`: poll `xpp::job::cancelled()` in `fixed_steps` and `--silent` integration; test via server abort).
- **W385b**: Sweep and shooting cancellation checkpoints (`core/integrate.cpp`, `core/pp_shoot.cpp`: poll `cancelled()` in `eq_range_sweep` and `shoot_range`, break on `ierr == -5`; test with sweep abort probe).
- **W385c**: Nullcline and flow cancellation checkpoints (`core/nullcline.cpp`: poll `cancelled()` in `do_cline`, `do_range_clines`, and flow grid loops; test via aborting high-mesh nullcline).
- **W385d**: Fitting and histogram pairwise cancellation (`core/do_fit.cpp`, `core/histogram.cpp`: add `cancelled()` checkpoints in `run_fit` and $O(N^2)$ pairwise difference loop; test via aborting large fit/histogram).

### Group B: Memory Bounds & Buffer Overflow Prevention
- **W385e**: DataStore bounds check in sweep and shooting routines (`core/integrate.cpp`, `core/pp_shoot.cpp`: check `rows < max_rows` in `eq_range_sweep` and `shoot_range`; test with sweep exceeding storage rows).
- **W385f**: AUTO 64-bit matrix dimension sizing and bounds check (`core/autlib2.cpp`, `core/auto_state.h`: use `std::size_t` for `SolvbvWork::resize`, validate against `MAX_AUTO_WORK_BYTES`; test with large NTST/NCOL).
- **W385g**: ExprStack runtime bounds check and compiler depth cap (`core/expr.h`, `core/expr_eval.cpp`, `core/expr_compile.cpp`: check `top < EXPR_STACK` in `eval_rpn` push; test with 250-depth nested formula).
- **W385h**: Delay buffer allocation bounds and overflow check (`core/delay_handle.cpp`: validate `big / dt <= MAX_DELAY_POINTS` and check against 32-bit int overflow; test with extreme delay/dt ratio).

### Group C: Recursion Safeguards
- **W385i**: Expression evaluator user function recursion depth limit (`core/expr_eval.cpp`: track recursion depth in `eval_rpn` for `UFUNTYPE` and fail if `depth > 32`; test with recursive user function model).
- **W385j**: AUTO continuation restart loop refactoring (`core/auto_nox.cpp`: convert recursive `do_auto` restart call into iterative loop with max restart cap; test with continuation restart).

### Group D: Disk & Input Queue Limits
- **W385k**: Range file export count cap (`core/integrate.cpp`: cap `range.write_files` count to 1,000 files and error on overflow; test with range file export).
- **W385l**: Server inbox and deferred command queue caps (`core/xpp_inbox.cpp`, `core/ui_json.cpp`: cap `queues[q]` and `session.deferred` to 1,024 items; test with rapid command flood).

## 7. Review (coordinator, 2026-10-15)

Checked against the code and by probe before any card is written:

- **Confirmed, a crash**: a user function that calls itself (`f(x)=f(x)+1`, `x1'=-f(x1)`) segfaults `xppautX --silent` (exit 139): expr_native refuses it ("calls a refused or recursive user function; running the interpreter"), and the interpreter's `UFUNTYPE` case recurses with no limit (core/expr_eval.cpp). Since the native compiler already detects the recursion, the simplest fix is to refuse such a function at load, with its file and line, rather than a depth counter (W385i's proposal).
- **Confirmed by reading**: `eq_range_sweep` writes one stored row per step (core/integrate.cpp ~527-533) with no check against `max_rows`: a step count above the store's capacity writes past it. `shoot_range` (pp_shoot.cpp) is reported the same: check at the card.
- **Partly**: the integration's Stop check is `program.interactive` only (integrate.cpp ~1720); `--server` and the window are interactive, so Stop works there; `--silent` has no Stop to press, but a `.recx` with a recorded stop must still end the run: the card checks that path, not the poll alone.
- **Not taken**: W385k's cap of 1000 per-run files is an arbitrary limit (AGENTS.md: limits are lifted, not added); a range writing files is stoppable like any run. The proposed names W385a-l break the one naming scheme (AGENTS.md "Cards: names and issues"): stage 2's cards get the next W numbers, grouped as:
  1. memory safety: the store's writes in the sweeps, the expression stack's push, the delay buffer's size, AUTO's work sizes in 64 bits (Group B);
  2. a recursive user function refused at load (Group C's first half; AUTO's restart recursion checked in the same card);
  3. cancellation checkpoints in the sweeps, nullclines, flow, fitting, histograms and fixed-step chunks (Group A);
  4. the server's queues (Group D's second half), after the maintainer decides between back-pressure and refusal.
- XPPAUT itself is checked for each confirmed bug at its card; one that XPPAUT shares goes to docs/xppaut-findings.md.
