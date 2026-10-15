# W371: Data structures audit across core/

Card [W371 / #433](https://github.com/MuhammadMoustafa/xppautX/issues/433), 2026-10-15.
Audited all data structures across `core/` (XPPAUT's code and AUTO's code alike; `third_party/` excluded) against the tasks they perform. This study evaluates array layouts, cache strides, dynamic memory allocations in hot loops, associative containers, and lookups.

## 1. Method

Searches covered all 108 C++ sources and headers in `core/`. Searches identified:
1. Multi-dimensional array structures: `vector<vector<...>>`, `xpp::Matrix`, `xpp::Matrix3`, flat index calculations `[i + n*j]`, `ARRAY2D`/`ARRAY3D`, and fixed C arrays.
2. Allocations in hot paths: `std::vector` declarations and dynamic heap reallocations (`push_back`/`emplace_back`, `resize`, `assign`) in per-step or per-point routines.
3. Associative containers and lookups: `std::map`, `std::unordered_map`, `std::set`, pointer chasing, and linear searches (`for (...) if (...)`).
4. Fortran memory layouts in AUTO and numerical libraries: row-major vs. column-major layouts, stride lengths, and inner-loop orientation against memory order.

Key search patterns used:
```sh
git grep -n -E "vector<.*vector<" core/
git grep -n -E "(std::map|std::unordered_map|std::set|std::unordered_set)" core/
git grep -n -E "(Matrix|Matrix3)" core/
git grep -n -E "\[.*(\+|\*).*\]" core/
git grep -n -E "(\*next|->next)" core/
git grep -n -E "(lookup|find_|search)" core/
```

---

## 2. Inventory of structures in hot paths

| File:Line | Structure | Job | Access pattern | Size in practice | Verdict |
|---|---|---|---|---|---|
| `core/tabular.cpp:185` | `const double *xv, *yv` | Explicit-coordinate table lookup (`lookupxy`) | Linear search `for (int i=1; i<n; i++) if (x <= xv[i])` | $n$ points (up to $10^6$) | **Finding 1**: $O(n)$ search in hot RHS evaluation; replace with binary search ($O(\log n)$) |
| `core/xpp_math.cpp:282` | Flat `double *a` (row-major) | Dense LU factorization (`sgefa`) for implicit/stiff solvers | `saxpy` runs along columns with stride `lda = n` in inner loop | $n \times n$ ($n \sim 10\text{--}1000$) | **Finding 2**: Non-contiguous column-stride inner loop in row-major matrix; interchange loops for row-contiguous elimination |
| `core/stiff.cpp:123` | `int indx[700]` (stack array) | Pivot indices for Rosenbrock LU factorization | Passed to `xpp::sgefa` | Hardcoded 700 | **Finding 3**: Hard limit violating W243; buffer overflow if $n > 700$. Hoist to `s.solver_work.size_pivots(n)` |
| `core/stiff.cpp:149` | Flat `double *a` | Jacobian addition $a = -J + \frac{1}{\gamma h}I$ | Outer loop `i`, inner loop `j` accessing `a[i + n*j]` (stride $n$) | $n \times n$ | **Finding 4**: Column-stride loop over row-major array; swap loop indices to walk stride 1 |
| `core/jacobian.cpp:24` | `std::vector<double>` `v`, `w`, `out` | Scratch and output buffers for exact Jacobian `entries()` | 3 heap allocations per call; returned by value | $m\text{.node} + 1$, workspace, nonzeros | **Finding 5**: 1,200 heap allocations per Newton iteration in AUTO periodic; hoist to persistent scratch |
| `core/jacobian.cpp:131` | `std::vector<double> f1, xp` | Temporary vectors for numerical finite-difference Jacobian | 2--3 heap allocations per Jacobian evaluation | $n$ doubles | **Finding 6**: Transient heap allocations in solver step; hoist scratch to `SolverWork` |
| `core/adj2.cpp:348` | `std::vector<std::vector<double>> jac_store` | Limit-cycle Jacobian trajectory storage | $n^2$ separate vectors; accessed as `jac[i + j*n][k]` | $n^2 \times NT$ (e.g. $100 \times 1000$) | **Finding 7**: Vector-of-vectors in hot loop; flatten to 1D `jac[k * n2 + i]` (contiguous at each time step) |
| `core/adj2.cpp:456` | `std::vector<int> ipvt` | Pivot buffer in `step_eul` | Heap allocation on every Euler step of adjoint solver | $n$ integers | **Finding 8**: Allocated $NT \times \text{maxit}$ times (e.g. 50,000 mallocs); hoist to caller `adjoint()` |
| `core/volterra.h:45` | `std::vector<std::vector<double>> memory` | Variable history storage for Volterra equations | Outer loop over history `i`, inner loop over `l` accessing `memory[l][ioff]` | `ntot` $\times$ `max_points` | **Finding 9**: Vector-of-vectors in hot history sum; flatten to `memory[ioff * ntot + l]` for contiguous time-slices |
| `core/conpar2.cpp:158` | `std::vector<std::vector<pair<int, double>>> dlog` | Thread-safe logging of updates to matrix $D$ in `conpar` | Dynamic `emplace_back` per interval across worker threads | $na$ intervals $\times$ pairs | **Finding 10**: Dynamic vector reallocations in hottest AUTO routine; replace with pre-sized flat buffers |
| `core/autlib2.cpp:73, 342` | `std::vector<doublereal> ft, e, fcc, sol1..3` | Temporary vectors in `solvbv` and `brbd` | Allocated on every Newton/chord iteration | $\sim ndim \cdot ncol \cdot ntst$ | **Finding 11**: 6 heap allocations per Newton step; hoist into existing `lib.solvbv` (`SolvbvWork`) |
| `core/auto_nox.h:106` | 8 $\times$ `std::vector<double>` per `DIAGRAM` | Diagram point state, stability, parameters | Allocated individually per point stored | $8 \times n$ doubles per point | **Finding 12**: 80,000 heap allocations for 10,000 points; flatten point payload vectors |
| `core/do_fit.cpp:325` | `std::vector<std::vector<double>> yderv_store` | Parameter derivative matrix for Levenberg-Marquardt fit | `npars` separate vectors; dereferenced via `yderv.data()` | `npars` $\times (npts+1) \cdot nvars$ | **Finding 13**: Vector-of-vectors for 2D matrix; flatten to single buffer `(npts+1) * nvars * npars` |
| `core/integrate.h:163` | `std::vector<std::vector<double>> x, er, em` | Monte Carlo fixed point search storage | `MAXFP = 400` inner vectors each of size `node` | $3 \times 400$ heap allocations | **Finding 14**: 1,200 separate heap allocations for 400 points; flatten to three $400 \times \text{node}$ buffers |
| `core/conpar2.cpp:52` | `xpp::Matrix<double> a` | Column swapping in AUTO condensation (`swap_columns`) | Loops over rows: `swap(a(first, row), a(second, row))` with stride `cols` | `cols` $\times$ `rows` | Fine: inherent to AUTO's row-contiguous $A(NCA, NRA)$ block layout; elimination arithmetic dominates and is contiguous |
| `core/storage.h:84` | `std::vector<std::vector<float>> columns_` | Trajectory storage in `DataStore` | Plotting reads columns contiguously (`col[c][r]`); step writes across columns | `max_rows` $\times$ `ncol` | Fine: column-major contiguous storage is required for fast multi-thousand-point graphics rendering |
| `core/dense.h:86` | `DenseMatRec` | CVODE dense linear solver matrix | Contiguous 1D `values` with column pointer table; column-major stride 1 | $n \times n$ | Fine: single contiguous buffer; LINPACK factorization walks columns contiguously |
| `core/band.h:86` | `BandMatRec` | CVODE band linear solver matrix | Contiguous 1D `values` with column pointer table; column-major stride 1 | $(smu + ml + 1) \times n$ | Fine: flat buffer for banded elements; cache-friendly |
| `core/xpp_math.h:193` | `xpp::Matrix`, `Matrix3` | Bounded multidimensional views for AUTO Fortran arrays | Inline operator `()`: `data[i + j*dim1]`, checked once at construction | Non-owning span views | Fine: zero-overhead RAII views replacing raw macro pointer math |
| `core/expr_eval.cpp:40` | Flat vectors `constants`, `variables`, `values` | Bytecode stack and registers in `eval_rpn` | Raw pointers `constants.data()`, `variables.data()`, `values.data()`, local `top` | Fixed size stacks | Fine: hottest code path; zero allocation, flat linear array traversal |
| `core/delay_handle.h:41` | Flat `std::vector<double> work` | DDE circular history buffer | Flat 1D buffer sized `rows * nodes`; `stor_delay` writes contiguous block | `rows` $\times$ `nodes` | Fine: flat ring buffer, single allocation, contiguous history updates |
| `core/simplenet.cpp:858` | Flat 1D arrays `w`, `y` | Neural/connection matrix multiplication (`MMULT`) | Row-major dot product `w[j*ncon + i] * y[i]` with stride 1 | `ncon` $\times$ `n` | Fine: contiguous stride-1 inner loop |
| `core/auto_jacobian.h:23` | Column-major raw array `out` | AUTO central-difference Jacobian assembly | Writes `out[j + i * ld]` with inner loop $j = 0\dots nout-1$ (stride 1) | $nout \times n$ | Fine: inner loop is contiguous stride 1; scratch buffers passed from `AutoLib` |
| `core/nullcline.cpp:289` | `std::vector<float> x_null, y_null` | Nullcline segment coordinates | Flat 1D vector sized once by `null_storage` ($4 \cdot course^2$) | Up to $4 \cdot course^2$ floats | Fine: flat contiguous memory for polyline generation |

---

## 3. Findings (most valuable first)

### Finding 1: Linear search in table interpolation during RHS evaluation
- **Location:** `core/tabular.cpp:185-203` (`lookupxy`).
- **What is wrong:** For tabular functions defined with explicit $(x, y)$ coordinate pairs (`xyvals=1`), `lookupxy` executes a linear search `for (int i=1; i<n; i++) if (x <= xv[i])` through the $x$-coordinate array on every table lookup. Table coordinates are strictly monotonically increasing ($x_1 < x_2 < \dots < x_n$).
- **Frequency:** Called from `lookup()`, which is evaluated during right-hand side (RHS) computation. In numerical integration (Runge-Kutta, CVODE) and AUTO Jacobian evaluations, RHS is called thousands to millions of times per run.
- **Cost:** An $O(n)$ search in the inner loop. For a table of $n = 1,000$ points, an average of 500 element comparisons and branches are executed per lookup. For tables approaching `table_points_limit = 1,000,000`, linear search becomes catastrophic.
- **Better structure:** `std::lower_bound` (or `std::upper_bound`) over `std::span<const double>(xv, n)`. Because $x$ is sorted, binary search executes $\lceil \log_2(n) \rceil$ comparisons (10 comparisons for $n=1,000$; 20 for $n=10^6$), exactly matching the interval lookup.
- **Confidence:** Certain (standard algorithmic optimization with identical floating-point results).
- **Workload to measure:** An ODE model using an `xyvals` lookup table (e.g. 1000 points) integrated with `--silent` under callgrind; measure instruction count in `lookupxy`.

### Finding 2: Column-stride inner loop in row-major dense LU factorization (`sgefa`)
- **Location:** `core/xpp_math.cpp:282-311` (`sgefa`).
- **What is wrong:** `sgefa` was ported from LINPACK (originally designed for Fortran column-major order) to row-major layout (`row i at a[i*lda]`), but the loop structure was not adapted. In the elimination step (`xpp_math.cpp:306`):
  ```cpp
  saxpy(n - k, t, (a + k * lda + k - 1), lda, (a + k * lda + j - 1), lda);
  ```
  Both `sx` and `sy` are accessed with stride `lda = n`. For an $N \times N$ matrix, the inner loop strides across $N$ distinct cache lines on every step, causing severe cache line underutilization and memory bandwidth pressure.
- **Frequency:** Called on every Jacobian factorization and Newton step in `gear.cpp:838`, `stiff.cpp:152`, `odesol2.cpp:148, 467`, `dae_fun.cpp:256`, `adj2.cpp:469`, `pp_shoot.cpp:449`, `volterra2.cpp:354`, and `do_fit.cpp:434`.
- **Better structure:** Row-oriented Gaussian elimination: interchange the loops so the outer loop iterates over the rows below the pivot ($i = k+1 \dots n$) and the inner loop updates row $i$ across columns ($j = k+1 \dots n$) with stride 1:
  ```cpp
  for (int i = k; i < n; i++) {
      double factor = a[i * lda + (k - 1)];
      for (int j = k; j < n; j++) {
          a[i * lda + j] += factor * a[(k - 1) * lda + j];
      }
  }
  ```
- **Gain:** Transforms inner loop access from stride-$N$ to contiguous stride 1 (vectorizable with SIMD/AVX). For moderate to large systems ($N \ge 50$), eliminates cache misses and speeds up LU factorization significantly.
- **Confidence:** Certain. Requires preserving exact partial pivoting tie-breaking rules and pivot recording.
- **Workload to measure:** A high-dimensional stiff system (e.g. 100-variable network model) solved with Gear or Rosenbrock stiff solver; measure cycle/instruction count in `sgefa`.

### Finding 3: Hardcoded stack buffer limit and overflow vulnerability in `stiff.cpp`
- **Location:** `core/stiff.cpp:123` (`int indx[700];`).
- **What is wrong:** `stiff()` allocates `indx[700]` as a fixed stack array and passes it directly to `xpp::sgefa(a, n, n, indx, &info)` at line 152. If a model has $N > 700$ ODE variables and runs the stiff solver, `sgefa` writes past the end of `indx`, causing stack buffer corruption. This violates the repository rule that all limits in `core/` are lifted (W243).
- **Better structure:** Allocate pivot buffer from `s.solver_work.size_pivots(n)` (the standard mechanism used by `odesol2.cpp:148` and `volterra2.cpp:354`), which resizes dynamically and stays persistent across steps with zero per-step allocation.
- **Gain:** Eliminates fixed 700-variable limit, prevents memory corruption, and unifies pivot management with the rest of `core/`.
- **Confidence:** Certain.

### Finding 4: Column-stride Jacobian matrix construction in `stiff.cpp`
- **Location:** `core/stiff.cpp:148-151`.
- **What is wrong:** In the Rosenbrock matrix setup loop:
  ```cpp
  for (i=0; i<n; i++) {
      for (j=0; j<n; j++) a[i+n*j] = -dfdy[i+n*j];
      a[i+n*i] += 1.0/(GAM*h);
  }
  ```
  The outer loop is `i` and the inner loop is `j`. With index `i + n*j`, the inner loop strides across memory with step $n$ doubles ($8n$ bytes).
- **Better structure:** Invert loops to `for (j=0; j<n; j++) for (i=0; i<n; i++)`, or flatten to a single loop `for (k=0; k<n*n; k++) a[k] = -dfdy[k];` followed by adding the diagonal entries `a[i * (n + 1)] += 1.0/(GAM*h)`.
- **Gain:** Replaces strided writes with a single contiguous vector copy / `std::transform` / `memcpy` and $n$ diagonal updates.
- **Confidence:** Certain.

### Finding 5: Heap allocations in exact Jacobian evaluation hot loops
- **Location:** `core/jacobian.cpp:24-45` (`entries()`).
- **What is wrong:** On every call to `exact_jacobian` or `exact_constant_derivative`, `entries()` allocates three `std::vector<double>` buffers on the heap (`v` of size `node + 1`, `w` of size `workspace`, `out` of size `p.rows.size()`) and returns `out` by value.
- **Frequency:** In AUTO periodic orbit continuation with NTST=100 and NCOL=4 (400 collocation points), each Newton iteration evaluates the exact Jacobian at 400 collocation points. This produces $400 \times 3 = 1,200$ heap allocations per Newton iteration. Over a continuation run of 100 points, this causes $>300,000$ heap allocations.
- **Better structure:** Pre-allocate persistent workspace vectors in `Session` or `SolverWork` / `AutoLib::scratch` sized once at model load time (`m.node + 1`, `m.jacobian.workspace`, `m.jacobian.states.rows.size()`), and pass them into `entries()`.
- **Gain:** Completely eliminates thousands of malloc/free cycles per continuation point without changing arithmetic order.
- **Confidence:** High. W358 showed that memory allocations account for 4.6% of instructions in `py_s1bf` and 13% in small AUTO models.
- **Workload to measure:** `tools/auto-profile` workload `py_s1bf` (periodic branch) with exact Jacobian; compare malloc call counts and self-cost in `entries()`.

### Finding 6: Vector-of-vectors matrix and transient heap allocation in adjoint solver (`adj2.cpp`)
- **Location:** `core/adj2.cpp:348-356, 456, 467`.
- **What is wrong:**
  1. `jac_store` is defined as `std::vector<std::vector<double>> jac_store(n2)` with `std::vector<double *> jac_v(n2)`. It allocates $N^2$ separate heap vectors of size $NT$. In `step_eul` (`adj2.cpp:464, 467`), accessing `jac[i + j*node][k]` and `jac[i][k2]` dereferences through scattered pointer arrays on every step of the iteration.
  2. Inside `step_eul` (`adj2.cpp:456`), `std::vector<int> ipvt(node);` allocates a new vector on the heap on EVERY single Euler step. For $NT = 1,000$ and $\text{maxit} = 50$, `step_eul` performs 50,000 heap allocations.
- **Better structure:**
  1. Flatten `jac_store` to a single contiguous 1D vector `std::vector<double> jac(n2 * nt)` with layout `jac[k * n2 + (i + j*node)]`. At time step $k$, all $N^2$ Jacobian entries are contiguous in cache.
  2. Hoist `ipvt` allocation into caller `adjoint()` alongside `work_v`.
- **Gain:** Eliminates $N^2 + 1$ heap allocations at startup and 50,000 heap allocations in `step_eul`, replacing non-contiguous pointer chasing with contiguous cache-line reads.
- **Confidence:** Certain.
- **Workload to measure:** Adjoint computation on `examples/ode/lecar.ode` (or any oscillator); count allocations and instructions in `step_eul`.

### Finding 7: Vector-of-vectors state history in Volterra integral equations
- **Location:** `core/volterra.h:45`, `core/volterra2.cpp:90-92, 161, 176`.
- **What is wrong:** `s.volterra.memory` is defined as `std::vector<std::vector<double>> memory` (`ntot` vectors of length `max_points`). In the inner history evaluation loop of `init_sums` (`volterra2.cpp:176`):
  ```cpp
  for (l = 0; l < nvar; l++) setvar(s, l + 1, s.volterra.memory[l][ioff]);
  ```
  For each history step `i` ($1 \dots \text{max\_points}$), the loop iterates across `nvar` distinct heap allocations at index `ioff`.
- **Better structure:** Store history as a flat 2D vector `std::vector<double> memory(max_points * ntot)` with layout `memory[ioff * ntot + l]`. All `nvar` state variables at history step `ioff` are contiguous in memory.
- **Gain:** Replaces $N$ scattered pointer lookups with a single contiguous slice per history step, enabling direct pointer passing or vector copy into the evaluator.
- **Confidence:** Certain.

### Finding 8: Dynamic vector-of-vectors reallocations in multi-threaded condensation (`conpar2.cpp`)
- **Location:** `core/conpar2.cpp:20, 158-168`.
- **What is wrong:** For multi-threaded condensation, `dlog` is defined as `std::vector<std::vector<std::pair<integer, doublereal>>> dlog(*na);`. Each mesh interval dynamically grows an inner vector via `emplace_back` at line 119:
  ```cpp
  (*arg->dlog)[interval].emplace_back(at, rm * b(l, pivot_row));
  ```
  This causes frequent dynamic heap reallocations within worker threads in AUTO's most computationally intensive function (conpar is 38% to 53% of instructions in periodic runs).
- **Better structure:** Pre-size the log buffers (or provide a flat single vector of triples `(interval, at, term)` or pre-sized per-interval capacity) using the known bound $(constraints - nbc) \times parameters \times (end - left)$, or allocate once per worker thread.
- **Gain:** Eliminates all dynamic vector growth and heap reallocations during threaded condensation, keeping exact serial reduction order intact.
- **Confidence:** High.
- **Workload to measure:** `tools/auto-profile` workload `py_s1bf` on 2 or 4 threads; measure instruction count and malloc calls in `conpar_process`.

### Finding 9: Transient vector allocations in AUTO block solver (`solvbv` and `brbd`)
- **Location:** `core/autlib2.cpp:73, 342-348`.
- **What is wrong:** On every call to `solvbv` and `brbd` (every Newton iteration and chord step):
  - Line 73 allocates `std::vector<doublereal> ft(iap->ndim * iap->ncol * (iap->ntst + 1));`.
  - Lines 342-348 allocate `e`, `fcc`, `sol1`, `sol2`, `sol3` as local vectors on the heap.
  `SolvbvWork` already exists in `lib.solvbv` to manage persistent buffers across steps (`a`, `b`, `c`, `d`, `a1`, `a2`, etc., resized once at line 77). Allocating `ft`, `e`, `fcc`, `sol1..3` separately violates the design pattern established by `SolvbvWork`.
- **Better structure:** Add `ft`, `e`, `fcc`, `sol1`, `sol2`, `sol3` as members of `SolvbvWork` and size them inside `SolvbvWork::resize()`.
- **Gain:** Eliminates 6 heap allocations per Newton iteration in AUTO.
- **Confidence:** Certain.
- **Workload to measure:** `tstauto_lecar` or `py_s1bf` periodic branch with `tools/auto-profile`; count `malloc` calls in `solvbv` and `brbd`.

### Finding 10: High per-point heap fragmentation in bifurcation diagram storage
- **Location:** `core/auto_nox.h:100-120`, `core/diagram.h:16-18`.
- **What is wrong:** `AutoDiagram::points` is a `std::deque<DIAGRAM>`. Each `DIAGRAM` struct independently owns 8 `std::vector` instances (`uhi`, `ulo`, `u0`, `ubar`, `evr`, `evi`, `par`, `parameter_indices`). Storing 10,000 points produces 80,000 independent heap allocations, causing heap fragmentation and poor cache locality during diagram redraws and browser loading.
- **Better structure:** Flatten variable-sized fields: store the state trajectories and eigenvalues in contiguous per-branch column tables or a single contiguous payload vector per point, referencing sub-spans.
- **Gain:** Reduces heap allocations by 8x for diagram points and improves cache locality during diagram queries.
- **Confidence:** Medium (clean refactoring, requires updating diagram serialization in `xpp_session_auto_io.cpp`).
- **Workload to measure:** AUTO run with 5,000 points; measure total memory footprint and heap allocations during diagram storage.

### Finding 11: Vector-of-vectors derivative matrix in parameter fitting (`do_fit.cpp`)
- **Location:** `core/do_fit.cpp:325-330`.
- **What is wrong:** `yderv_store` is allocated as `std::vector<std::vector<double>> yderv_store(npars)` with a pointer array `yderv(npars)`.
- **Better structure:** Flatten to a single `std::vector<double>` of size `npars * (npts + 1) * nvars` with an inline view helper `yderv(p, i, v)`.
- **Gain:** Replaces `npars + 1` heap allocations with 1 allocation.
- **Confidence:** Certain.

### Finding 12: Vector-of-vectors in Monte Carlo fixed point list (`integrate.cpp`)
- **Location:** `core/integrate.h:163`, `core/integrate.cpp:363-369`.
- **What is wrong:** `FixedPointList` stores `x`, `er`, `em` as `std::vector<std::vector<double>>` sized to `MAXFP = 400`, each containing a `vector<double>` of size `node`. This creates $3 \times 400 = 1,200$ heap allocations.
- **Better structure:** Replace each with a single flat vector `std::vector<double>` of size `MAXFP * node` indexed as `x[i * node + j]`.
- **Gain:** Eliminates 1,200 heap allocations, reducing initialization to 3 allocations.
- **Confidence:** Certain.

---

## 4. Proposed follow-up cards

### Pack A: Linear Algebra & Solvers (sgefa, stiff)
- **Card 1 (sgefa row-contiguous LU)**: `core/xpp_math.cpp`, `core/xpp_math.h`. Reorient `sgefa` elimination inner loop to row-stride 1, eliminating cache thrashing across all implicit and stiff solvers.
- **Card 2 (stiff solver limit lift and loop order)**: `core/stiff.cpp`. Lift 700-variable stack array limit to `size_pivots(n)` and convert column-stride matrix setup loop to contiguous order.

### Pack B: Table & Jacobian Hot Paths (tabular, jacobian)
- **Card 3 (binary search table interpolation)**: `core/tabular.cpp`, `core/tabular.h`. Replace $O(n)$ linear scan in `lookupxy` with `std::lower_bound` ($O(\log n)$), accelerating RHS table evaluations.
- **Card 4 (persistent exact Jacobian scratch)**: `core/jacobian.cpp`, `core/jacobian.h`. Hoist heap vector allocations in `entries()` to persistent scratch, removing thousands of mallocs per Newton iteration.
- **Card 5 (reusable numerical Jacobian scratch)**: `core/jacobian.cpp`. Hoist transient `f1` and `xp` allocations in `jacobian()` into `SolverWork` scratch buffers.

### Pack C: Vector-of-Vectors Flattening (adj2, volterra, do_fit, integrate)
- **Card 6 (flatten adjoint Jacobian matrix & hoist pivot)**: `core/adj2.cpp`. Replace $N^2$ scattered vectors with flat 2D array and hoist `ipvt` out of `step_eul`, saving 50,000 mallocs per run.
- **Card 7 (flatten Volterra history buffer)**: `core/volterra.h`, `core/volterra2.cpp`. Replace `memory` vector-of-vectors with row-contiguous flat buffer for contiguous time-slice reads.
- **Card 8 (flatten fitting derivatives and fixed point lists)**: `core/do_fit.cpp`, `core/integrate.h`, `core/integrate.cpp`. Flatten `yderv_store` and `FixedPointList` (`x`, `er`, `em`), eliminating >1,200 scattered heap vectors.

### Pack D: AUTO Newton Step Buffers (autlib2, conpar2)
- **Card 9 (hoist solvbv/brbd scratch into SolvbvWork)**: `core/autlib2.cpp`, `core/auto_state.h`. Move `ft`, `e`, `fcc`, `sol1..3` into `SolvbvWork`, eliminating 6 heap allocations per Newton iteration.
- **Card 10 (pre-size parallel condensation dlog)**: `core/conpar2.cpp`. Replace dynamic `vector<vector<pair>>` with pre-sized flat buffers for worker threads in threaded `conpar`.

### Pack E: Diagram Point Memory Footprint (auto_nox, diagram)
- **Card 11 (compact diagram point representation)**: `core/auto_nox.h`, `core/diagram.h`, `core/diagram.cpp`. Group or flatten 8 separate vectors per `DIAGRAM` point, reducing diagram heap allocations by 8x.
