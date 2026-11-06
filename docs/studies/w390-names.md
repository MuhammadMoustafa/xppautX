# W390 Readable names for the numerical code: stage 1 inventory (#452)

Read-only inventory (maintainer, 2026-10-15: "can we change these ugly names like sgefa and sgesl"). No code changed. Line numbers are master at 3fbeb7eb. third_party/ is excluded; the vendored CVODE's API stays until W34, so its names are listed only where core/ has its own copies.

Scope read: function, type, struct field and file-scope names in core/ (autoX's files included: autox_*, setubv*, gogoautox, conpar*, diagram), found by grep for Fortran, LINPACK, EISPACK, f2c and AUTO-style names, then the definitions read where a name's meaning was unclear. Local variables (the f2c `i__1`, `z__1`, `a_dim1` family, about 1,400 occurrences in xpp_math_eispack.cpp alone) are summarised by pattern, not listed one by one: stage 2 generates the full local list by script, per pack.

Proposed names follow core's own snake_case (a function says what it does; a type says what it is). "Origin" says where the algorithm comes from: LINPACK, EISPACK, BLAS, Hairer (DOPRI), Numerical Recipes, XPPAUT (its own code, cited in docs/xppaut-findings.md), AUTO-07p / AUTO2000 (the autoX Fortran, translated by f2c, so it is ours: AGENTS.md "One codebase"), CVODE, or f2c (the runtime helpers). A name marked **?** has a meaning I could not settle from the code: the maintainer or the origin's source (reference/auto-07p, reference/xppaut-8.0) settles it before stage 2.

## Review against the rules (2026-11-06, before stage 2)

The maintainer approved the names on the condition that they follow AGENTS.md's
rules and format. Checked; stage 2 fixes these before it renames anything:

1. **About 40 rows are marked `?`, and some are guesses that are wrong.** Two of
   three checked against `reference/auto-07p/src` were: `stbif` "stores
   branching data" (ae.f90), not `step_bifurcation`; `pdble` "preprocesses
   restart data for switching branches at a period doubling" (periodic.f90),
   not `double_precision_copy`. Every `?` row (and section 6's ambiguous list)
   is checked against AUTO-07p's or XPPAUT's own source and named from what it
   does there, the source line cited in the row; a meaning that cannot be
   established keeps its old name and says so.
2. **Rule 4 makes new aliases** (`Index`, `Single`, `Real`, `Complex`) and
   treats autoX's code apart. One codebase and one name per thing: the f2c
   types become the std types directly everywhere (`std::ptrdiff_t`, `float`,
   `double`, `std::complex<double>` in place of the `{r, i}` struct), bits
   unchanged.
3. **No abbreviations left in a proposed name:** `start_bl`, `start_c1`,
   `svd_nd`, `homoclinic_pvls`, `homoclinic_fn_ffho`, `objective_fn`,
   `user_fn_fold` keep Fortran's letters; each gets a name that says what it is.
4. **autoX's name** (W599): proposals use `autox` (lowercase in code), never
   `auto_`; `go_go_autox` is named for what it does, not transliterated.

## 0. Rules proposed for stage 2 (for the maintainer)

1. **One naming style:** core's snake_case. Fortran's `s`/`d` prefixes (single/double) go: they say a precision, and most of our routines are double (`sscal` and `saxpy` are double, so the `s` misleads).
2. **Keep the origin:** each renamed routine keeps its original name in a one-line comment at its definition, e.g. `/* sgefa: LINPACK's factor */`, so the studies, docs/xppaut-findings.md and docs/auto07p-findings.md, which cite XPPAUT's and AUTO-07p's sources, still trace.
3. **AUTO's documented names:** AUTO-07p's own output and documentation use `ISW`, `IPS`, `ITP`, `NTST`, `NCOL`, `NICP`, `RL0`, `RL1`, ... (fort.7, fort.8, the diagram). The field names in `iap_type` and `rap_type` (section 3) are readable, but the AUTO names stay in the comment on each field, and the file formats keep AUTO's columns.
4. **Types:** `integer`, `real`, `doublereal`, `doublecomplex` (autox_f2c.h:15-18) become `Index`, `Single`, `Real`, `Complex` (or the std types directly where the code is not autoX's). One name per thing (aliascheck): the f2c names go in one step, not kept as aliases.
5. **Macros of the function-pointer types** (`FUNI_TYPE`, `BCNI_TYPE`, `ICNI_TYPE`, `FNCS_TYPE_AE`, `FNCS_TYPE_BVP`, `PVLI_TYPE_AE`, `PVLI_TYPE_BVP`, `STPNT_TYPE_AE`, `STPNT_TYPE_BVP`, autox_c.h) become typed function-pointer aliases with a readable name, after the ones in section 4 are renamed.
6. **Duplicates merge, they are not just renamed:** the rows marked **dup** in sections 1 and 2 are one operation in two files. stage 2 keeps one (the owner: xpp_math for numerics, per AGENTS.md "Single source"), not two renamed copies.
7. **Renames go in packs by area** (section 5), each after the cards that change its files have merged; numbers bit-identical (examples md5s, autocompare, goldencheck, odexcheck).

## 1. Linear algebra (LU, banded, dense, Householder)

### 1.1 core/xpp_math.cpp (LINPACK and EISPACK helpers, owner xpp_math)

| Current | Proposed | Meaning | Origin |
|---|---|---|---|
| `sgefa` (xpp_math.h) | `lu_factor` | dense LU factorisation with partial pivoting, in place | LINPACK sgefa |
| `sgesl` | `lu_solve` | solve with the factors of sgefa | LINPACK sgesl |
| `sgefa_det_sign` | `lu_det_sign` | sign of the determinant from the pivots | ours, from sgefa |
| `saxpy` (xpp_math.cpp:249) | `axpy` | y += a x, strided | BLAS level 1 (single name, double code) |
| `sscal` | `scale` | x *= a | BLAS level 1 |
| `bandfac` | `band_factor` | banded LU factorisation | LINPACK dgbfa-style |
| `bandsol` | `band_solve` | solve with the banded factors | LINPACK dgbsl-style |
| `hqr` (xpp_math.cpp:470, `void hqr(int ...)`) | **dup** of 1.3 | EISPACK hqr, a second copy | EISPACK |
| `orthes` (xpp_math.cpp:421) | **dup** of 1.3 | EISPACK orthes, a second copy | EISPACK |
| `eigenvalues` (xpp_math.cpp:617) | `real_eigenvalues` | eigenvalues of a real matrix through orthes and hqr (the two copies above) | ours |
| `sign` (xpp_math.cpp:410) | **dup** of `d_sign` (2.1) | FORTRAN SIGN(a,b): |a| with the sign of b | Fortran intrinsic |
| `largest_magnitude`, `bracketed_root`, `product_accumulate`, `all_finite`, `subtract_scaled` (xpp_math.h) | keep | ours, readable | ours |
| `bessel_i`, `bessel_i0`, `bessel_i1`, `bessel_i_scaled`, `bessel_j`, `bessel_y` | keep | Bessel functions, musl-based (W163) | musl |
| `xpp_musl_cos`, `xpp_musl_log`, `xpp_musl_sin` | keep | our copy of musl's kernels, the names say the source | musl |
| `fft`, `fft_real` | keep | complex and real FFT (span-typed) | ours |

### 1.2 core/xpp_math_autox.cpp and .h (AUTO's linear algebra, owner xpp_math_autox)

| Current | Proposed | Meaning | Origin |
|---|---|---|---|
| `ge` (xpp_math_autox.cpp:192) | `gauss_eliminate` | Gaussian elimination with pivoting, writes the diagnostics to fp9 | AUTO (autox_lib1 uses it) |
| `complete_pivot`, `complete_eliminate_row` | keep | already readable | ours |
| `prjctn` | `project_onto_subspace` **?** | projection of a vector through the null space (AUTO's PRJCTN) | AUTO |
| `eig` (xpp_math_autox.cpp:54) | `eigen_decompose` | eigenvalues and eigenvectors of AUTO's Jacobian (the eigen-solver of the stability check) | AUTO |
| `eigho` (xpp_math_autox.cpp:966; also in autox_lib5.cpp) | `eigen_homoclinic` **?** | eigen routine for HomCont's problem | AUTO HomCont |
| `dhhap`, `dhhpr` | `householder_apply`, `householder_prepare` **?** | Householder reflector apply and prepare (names: `dh` = Householder) | LINPACK/EISPACK-style |
| `nlvc` | `null_vector` **?** | the null vector of a matrix (AUTO's NLVC) | AUTO |
| `flowkm` | `flow_kernel_matrix` **?** | Kernel (null space) of the flow matrix, AUTO's FLOWKM (SVD tolerance autox_svd_tolerance) | AUTO |
| `dgemm` (xpp_math_autox.cpp:519, .h:69) | **dup** of 1.4 (`dgemm` in xpp_math.cpp and eispack) | matrix multiply | BLAS |
| `xerbla`, `lsame` (xpp_math_autox.h) | `blas_error`, `same_char` | BLAS error handler, case-insensitive character compare | BLAS |

### 1.3 core/xpp_math_eispack.cpp (EISPACK and BLAS-1 translated by f2c, 7,262 lines, owner xpp_math)

| Current | Proposed | Meaning | Origin |
|---|---|---|---|
| `rg` | `real_general_eigen` | eigenvalues (and vectors) of a real general matrix: balance, reduce, QR | EISPACK rg |
| `hqr` (line 144), `hqr2` (499) | `hessenberg_qr_values`, `hessenberg_qr_vectors` | QR on Hessenberg form, values, values and vectors | EISPACK hqr, hqr2 |
| `hqr3lc` | `hessenberg_qr_complex_check` **?** | hqr variant used by AUTO's stability check (name says the complex limit check) | EISPACK-derived, AUTO use |
| `cdiv` (1192) | `complex_divide` | complex division | EISPACK cdiv |
| `balanc`, `balbak` | `balance_matrix`, `unbalance_vectors` | balancing a matrix and back-transforming its vectors | EISPACK |
| `elmhes`, `eltran` | `hessenberg_reduce`, `hessenberg_transform` | reduction to Hessenberg form by elimination, and its accumulated transform | EISPACK |
| `orthes`, `ortran` | `hessenberg_orthogonal`, `hessenberg_orthogonal_transform` | reduction to Hessenberg form by Householder, and its transform | EISPACK |
| `qzhes`, `qzit`, `qzval` | `qz_reduce`, `qz_iterate`, `qz_values` | generalised eigenproblem (QZ) steps | EISPACK |
| `epslon` | `machine_epsilon` | the rounding unit | EISPACK |
| `exchng`, `split`, `sig22`, `sndrtg`, `qrstep`, `prse` | `exchange_rows`, `split_block`, `sigma_2x2`, `rotate_givens`, `qr_step`, `parse_diagonal` **?** | helpers of the SVD and the QR | EISPACK / Demmel-Kahan |
| `ezsvd`, `ndsvd`, `sigmin`, `ndrotg` | `svd_tiny`, `svd_nd`, `smallest_singular`, `givens_rotation` **?** | Demmel-Kahan SVD and its smallest singular value | Demmel-Kahan (LAPACK-style) |
| `dnrm2` (3098) | `norm2` | Euclidean norm | BLAS level 1 |
| `ddot` (3284) | `dot` | dot product | BLAS level 1 |
| `dscal` (3368) | `scale` (merge with 1.1's `sscal`) | x *= a | BLAS level 1 |
| `idamax` (3436) | `index_max_abs` | index of the largest |x| | BLAS level 1 |
| `daxpy` (3503) | **dup** of `axpy` (1.1) | y += a x | BLAS level 1 |
| `drot` (3583), `dswap` (3646) | `rotate`, `swap` | plane rotation, swap | BLAS level 1 |
| `dgemc` (3734) | `matrix_multiply_check` **?** | matrix multiply with a transposition flag | BLAS-like |
| `dgemm` (3981) | `matrix_multiply` (**dup** of 1.2's, one copy) | matrix multiply | BLAS |

### 1.4 Other LU and banded copies (the "three LU solves" of W30)

| File | Current | Proposed | Note |
|---|---|---|---|
| core/dense.cpp, dense.h | `gefa`, `gesl` (names already in comments: "called gefa and gesl") | `dense_factor`, `dense_solve` | a third LU, same algorithm as sgefa in other words: **dup**, one owner |
| core/band.cpp, band.h | `gbfa`, `gbsl` | `band_factor`, `band_solve` | the banded LU of band.cpp, with bandfac/bandsol of 1.1: **dup** |
| core/xpp_math_autox.cpp | `ge` (1.2) | `gauss_eliminate` | Gaussian elimination in AUTO's own form: **dup** of the above in words |

## 2. Helpers from f2c and the complex or sign helpers

### 2.1 core/f2c_helpers.cpp and autox_f2c.h (f2c runtime helpers)

| Current | Proposed | Meaning | Origin |
|---|---|---|---|
| `d_sign` (f2c_helpers.cpp:32) | `sign_of` (**dup** of `sign`, 1.1: keep one) | sign transfer | f2c runtime |
| `f__cabs` (f2c_helpers.cpp:11) | `complex_magnitude` | magnitude of a complex number, overflow-safe | f2c runtime |
| `z_abs` (f2c_helpers.cpp:38) | `complex_abs` (**dup** of `c_abs`, 2.2, and f__cabs) | magnitude of a doublecomplex | f2c runtime |
| `abs`, `max`, `min` (autox_f2c.h:38 onward) | std `abs`, `max`, `min` | f2c-style templates, replaceable by std::abs etc. | f2c |
| `integer`, `real`, `doublereal`, `doublecomplex` (autox_f2c.h:15-18) | `Index`, `Single`, `Real`, `Complex` | type names of the translated Fortran | f2c |
| `f2c::abs` (used in xpp_math_autox.cpp) | std::abs | the namespace's own abs | f2c |

### 2.2 core/del_stab.cpp and .h (XPPAUT's delay stability, complex helpers)

| Current | Proposed | Meaning | Origin |
|---|---|---|---|
| `c_abs` (del_stab.cpp:175) | **dup** of `complex_abs` (2.1) | magnitude | f2c |
| `cdeterm` | `complex_determinant` | determinant of a complex matrix | XPPAUT |
| `cdif`, `cdivv`, `cmlt`, `cexp2` | `complex_subtract`, `complex_divide` (**dup** of 1.3's cdiv), `complex_multiply`, `complex_exp2` | complex arithmetic | XPPAUT |
| `rtoc`, `make_z` | `real_to_complex`, `make_complex` | conversions | XPPAUT |
| `switch_rows`, `test_sign`, `process_root`, `find_positive_root`, `get_arg`, `plot_args` | keep or readable as is | delay stability's root search | XPPAUT |
| `do_delay_sing` | keep | the delay equilibrium search | XPPAUT |

### 2.3 core/expr_functions.cpp (parser comparison functions, user-visible through the expression language)

| Current | Proposed | Meaning | Origin |
|---|---|---|---|
| `dle` (line 84) | `less_equal` | x <= y as 1 or 0 | XPPAUT (the parser's operator table) |
| `dlt` (100) | `less_than` | x < y | XPPAUT |
| `dge` | `greater_equal` | x >= y | XPPAUT |
| `dne` | `not_equal` | x != y | XPPAUT |
| `dnot` | `logical_not` | not x | XPPAUT |

These are parser built-ins: the names are not user-visible in the manual (grep of docs/manual finds no `dle`), so renaming changes no user text, but the choice of the operator names is for the maintainer (AGENTS.md "named choices") **?**.

## 3. Types and fields (struct level)

### 3.1 AUTO's integer and real parameter arrays (core/autox_c.h:46-88 and :90-108)

`iap_type` (the integer array IAP) and `rap_type` (the real array RAP). Proposed names; the AUTO name in the comment.

| iap field (AUTO) | Proposed | Meaning (AUTO-07p's documentation, to check in reference/auto-07p) |
|---|---|---|
| `ndim` (1) | `dimension` | number of ODEs |
| `ips` (2) | `problem` | problem type (`AutoxProblem`) |
| `irs` (3) | `restart_label` | restart label (0 for none) |
| `ilp` (4) | `fold_detection` | locate fold points |
| `ntst` (5) | `mesh_intervals` | mesh intervals |
| `ncol` (6) | `collocation_points` | collocation points per interval |
| `iad` (7), `iads` (8) | `adapt_mode`, `adapt_steps` **?** | mesh adaptation (AUTO's IAD, IADS) |
| `isp` (9) | `special_points` | special point detection |
| `isw` (10) | `branch_switch` | branch switching |
| `iplt` (11) | `plot_index` | plot axis index |
| `nbc` (12), `nbc0` (24) | `boundary_conditions`, `boundary_conditions_used` **?** | number of boundary conditions |
| `nint` (13) | `integral_conditions` | integral conditions |
| `nmx` (14) | `max_steps` | steps |
| `nuzr` (15), `iuzr` (26) | `user_zeros`, `user_zero_index` | user zero functions |
| `npr` (16) | `output_frequency` | output frequency |
| `mxbf` (17) | `max_bifurcations` | bifurcation limit |
| `iid` (18) | `diagnostics` | diagnostic level |
| `itmx` (19), `itnw` (20), `nwtn` (21) | `max_iterations`, `newton_iterations`, `newton_mode` **?** | Newton's limits and mode |
| `jac` (22) | `jacobian_mode` | 0 differences, 1 supplied (autox_c.h:32-33) |
| `ndm` (23) | `max_dimension` | maximum dimension of the printed data |
| `nnt0` (25) | `mesh_intervals_initial` **?** | |
| `itp` (27) | `point_type` (`AutoxPointCode`) | point code |
| `itpst` (28) | `start_kind` | start kind of a two-parameter continuation |
| `nfpr` (29) | `free_parameters` | number of free parameters |
| `ibr` (30) | `branch` | branch number |
| `nit` (31), `ntot` (32), `nins` (33) | `iterations`, `points_total`, `points_inserted` **?** | counters |
| `istop` (34) | `stop_flag` | stop |
| `nbif` (35), `ipos` (36), `lab` (37) | `bifurcations`, `position`, `label` **?** | |
| `nicp` (41) | `continuation_parameters` | number of continuation parameters |
| `run` | keep | the run's own state (AutoxRun) |

`rap_type` (autox_c.h:90-106, the real array RAP):

| rap field (AUTO) | Proposed | Meaning |
|---|---|---|
| `ds` (1) | `step_size` | arc-length step |
| `dsmin` (2), `dsmax` (3) | `step_min`, `step_max` | step limits |
| `dsold` (5) | `step_previous` | the previous step |
| `rl0` (6), `rl1` (7) | `parameter_min`, `parameter_max` | parameter window (the diagram's bounds) |
| `a0` (8), `a1` (9) | `weight0`, `weight1` **?** | AUTO's arc-length weights (meaning to check) |
| `epsl` (11), `epsu` (12), `epss` (13) | `tolerance_parameter`, `tolerance_state`, `tolerance_special` **?** | Newton tolerances (per AUTO's documentation) |
| `det` (14) | `determinant` | determinant of the Jacobian |
| `tivp` (15) | `time_initial` | initial time (for the time-dependent start) |

### 3.2 AutoxRun's common-block members (core/autox_state.h:355-363)

These are AUTO's Fortran common blocks, now nested structs; stage 2 names the nested struct and its fields.

| Current | Proposed | Fields (current -> proposed) |
|---|---|---|
| `blhom` | `homoclinic` | `itwist`, `istart`, `iequib`, `nfixed`, `npsi`, `nunstab`, `nstab`, `nrev` (keep the field names, they are readable) |
| `blhmp` | `homoclinic_indices` | `ipsi`, `ifixed`, `irev` |
| `blhmu` | `homoclinic_pu` | `pu0`, `pu1` -> `pu_start`, `pu_end` **?** |
| `bcnn` | `boundary_count` | `nbcn` -> `count` |
| `blhma` | `homoclinic_scalars` | `compzero` (= autox_hmachho) |
| `bleig` | `eigen_workspace` | `rr`, `ri`, `v`, `vt`, `xequib`, `ineig` (-> `real_part`, `imag_part`, `vectors`, `vectors_transposed`, `equilibrium`, `eigen_count`) |
| `blhme` | `eigen_check` | `vrprev`, `ieigc` -> `previous_vector`, `counts` **?** |
| `beyn` | `beyn_workspace` | `cprev`, `iflag` (Beyn's method) |

### 3.3 Other

| Current | Proposed | Where |
|---|---|---|
| `SolvbvWork` (autox_state.h:241) | keep | the solver's work (already readable) |
| `AutoxAssemblyScratch`, `AutoxGlobalScratch`, `AutoxGlobalRotations` | keep | already readable |
| `function_list`, `autobv_function_list`, `autoae_function_list` (autox_c.h:220-240) | `problem_functions`, `boundary_value_functions`, `algebraic_functions` | the function tables of AUTO's problem |

## 4. autoX's Fortran routines: the names that are not readable (by area)

AUTO-07p's own names, from autox_c.h's prototypes and the definitions in autox_lib1/2/3/5.cpp, gogoautox.cpp, setubv2.cpp, conpar2.cpp, autox_pp.cpp. "Origin" is AUTO (the autoX Fortran translated by f2c; AUTO-07p's routine of the same name in reference/auto-07p, where the name is AUTO's). Each file's comment says "translated from autoX's autox_libN.f by f2c".

### 4.1 Problem setup, the start and the user functions

| Current | Proposed | Meaning | File |
|---|---|---|---|
| `autobv`, `autoae` | `run_boundary_problem`, `run_algebraic_problem` | the problem drivers (BV and algebraic) | autox_lib1 |
| `funi` (autox_lib3.cpp, called from many files) | `user_rhs` | the model's right-hand side and Jacobian | autox_lib3, autox_pp |
| `stpnt` | `start_point` | the start point of a problem | autox_pp.cpp, lib1/3/5 |
| `icni` (lib3), `bcni` (lib3) | `integral_conditions_fn`, `boundary_conditions_fn` | user's integral and boundary conditions | autox_lib3 |
| `fopt`, `bcnd`, `icnd`, `func` (autox_pp.cpp) | `objective_fn`, `boundary_fn`, `integral_fn`, `rhs_fn` **?** | the user's problem functions, in autox_pp | autox_pp |
| `setubv` (setubv2.cpp) | `setup_problem` **?** | sets the problem up: the mesh and the matrix, in setubv2.cpp | setubv2 |
| `conpar` (conpar2.cpp) | `continuation_parameters` **?** (name in conpar2 is `conpar_process`) | continuation parameter setup | conpar2 |

### 4.2 Continuation, the mesh and the Newton step (autox_lib1, lib2, lib3)

| Current | Proposed | Meaning | Origin |
|---|---|---|---|
| `solvae`, `solvbv` | `solve_algebraic`, `solve_boundary` | the Newton solve of the algebraic and BV problems | AUTO |
| `stepbv` | `step_boundary` | one continuation step (BV) | AUTO |
| `stdrbv`, `stdirae` | `step_direction_boundary`, `step_direction_algebraic` | the step's direction (BV and algebraic) | AUTO |
| `stprae`, `stplae`, `stplbv` | `step_predict_algebraic`, `step_locate_algebraic`, `step_locate_boundary` | prediction and locating a step | AUTO |
| `stpswae` | `step_switch_algebraic` | a branch-switch step | AUTO |
| `stupbv` | `step_setup_boundary` | the step's setup | AUTO |
| `cnrlae`, `cnrlbv` | `continuation_locate_algebraic`, `continuation_locate_boundary` | locating the continuation | AUTO |
| `contae`, `contbv` | `continue_algebraic`, `continue_boundary` | the continuation | AUTO |
| `extrbv` | `extrapolate_boundary` | extrapolation of the solution | AUTO |
| `lcspae`, `lcspbv` | `locate_special_algebraic`, `locate_special_boundary` | locating a special point | AUTO |
| `rsptbv` | `restart_point_boundary` | restart point | AUTO |
| `swprc` | `switch_procedure` **?** | the branch-switch procedure | AUTO |
| `setrtn`, `setrhs` | `set_return`, `set_rhs` | setup of the return and the right-hand side | AUTO |
| `setzero`, `setrhs` (lib2) | `clear_array`, `set_rhs` | | AUTO |
| `brbd` | `bordered_matrix` **?** | the bordered Jacobian (AUTO's BRBD) | AUTO |
| `bcksub`, `redrhs`, `reduce`, `reduce_merge`, `redrhs_merge` (lib2) | `back_substitute`, `reduce_rhs`, `reduce_matrix` **?** | the tree condensation of the bordered system | AUTO |
| `dimrge` | `merge_dimensions` **?** | dimension merge of the condensation | AUTO |
| `copycp`, `cpyrhs` (lib2) | `copy_block`, `copy_rhs` | copies | AUTO |

### 4.3 Mesh, adaptation and the solution's points (autox_lib1)

| Current | Proposed | Meaning | Origin |
|---|---|---|---|
| `newmsh`, `newlab`, `msh` | `new_mesh`, `new_label`, `mesh_points` **?** | mesh adaptation and the labels of new points | AUTO |
| `adapt`, `adptds` | `adapt_mesh`, `adapt_step` | adaptation and the step size change | AUTO |
| `cntdif`, `cpnts`, `eqdf`, `findlb`, `ordr`, `readlb`, `sthd`, `stbif` | `count_differences`, `copy_points`, `equidistribute`, `find_label`, `order_points`, `read_label`, `step_header`, `step_bifurcation` **?** | mesh and solution bookkeeping | AUTO |
| `wint`, `intwts`, `genwts`, `interp`, `rintg`, `pdble`, `faft`, `fftr` | `gauss_weights`, `integration_weights`, `lagrange_weights`, `interpolate`, `integrate_mesh`, `double_precision_copy` **?**, `fourier_transform_fft`, `fourier_real` **?** | quadrature weights and interpolation over the mesh (AUTO's WINT, INTWTS, GENWTS, INTERP, RINTG), the Fourier transform (FAFT) | AUTO |
| `mueller` | `mueller_root` | Mueller's root method | AUTO |
| `nrmlz`, `rnrm2`, `rnrmsq`, `rmnups`, `rmxups` | `normalise`, `norm_points`, `norm_squared_points`, `min_points`, `max_points` **?** | the norms and extremes over a solution | AUTO |
| `scaleb`, `skip3`, `pi` (lib1) | `scale_by`, `skip_three`, `pi_constant` **?** | | AUTO |
| `interp` (autox_lib1:3896; a second `interp` in tabular.cpp) | `interpolate_mesh` | the mesh interpolation (not `tabular`'s) | AUTO |
| `init1`, `init` | `initialise_run` **?** | | AUTO |
| `rinpr` | `inner_product` | inner product (AUTO's RINPR) | AUTO |
| `headng` | `diagram_headings` | the headings of the diagram columns | AUTO |
| `wrjac`, `wrline`, `wrtbv8`, `wrtbv9`, `wrtsp8` | `write_jacobian`, `write_line`, `write_solution_v8`, `write_solution_v9`, `write_solution_sp8` | the output writers (fort.7/8/9 formats: keep the FORMAT's shape) | AUTO |
| `tpspbv` | `time_periodic_boundary` **?** | time-periodic BV | AUTO |
| `eighi` (autox_lib5) | `eigen_hopf` **?** | eigen of HomCont's Hopf problem | AUTO HomCont |

### 4.4 HomCont (autox_lib5.cpp, autox_lib3's ho-family, gogoautox)

| Current | Proposed | Meaning | Origin |
|---|---|---|---|
| `fnho`, `ffho`, `bcho`, `fbho`, `icho`, `fiho`, `inho` | `homoclinic_fn`, `homoclinic_fn_ffho`... **?** (keep the `ho` family as one group: `homoclinic_rhs`, `homoclinic_bc`, `homoclinic_integral`, ...) | the homoclinic problem's functions | AUTO HomCont |
| `preho`, `stpnho`, `stpho`, `pvlsho`, `psiho`, `prjcti` | `homoclinic_prepare`, `homoclinic_start`, `homoclinic_start_point`, `homoclinic_pvls`, `homoclinic_psi`, `project_integral` **?** | HomCont's steps | AUTO HomCont |
| `fnlpae`, `fnlpbv`, `fnuzae`, `fnuzbv`, `fnspbv`, `fnbpae`, `fnhbae`, `fnclosedae`, `fnclosedbv` | `user_fn_fold`, ..., `user_fn_closed_bv` **?** | the test functions of the special points (fold, user zero, special point, Hopf, closed branch) | AUTO / ours |
| `stpnae`, `stpnbv`, `stpnub`, `stpnus`, `stpnwp`, `pvlsae`, `pvlsbv`, `pvli_*` (macros) | `start_algebraic`, `start_boundary`, `start_unit_bv`, `start_unit_s`, `start_wp`, `pvls_algebraic`, `pvls_boundary` | the start and the PVLS (pseudo-arc) functions | AUTO |
| `fncs` (macros FNCS_TYPE_*) | `function_pointer_*` | | AUTO |
| `stpnbl`, `stpnc1`, `stpnc2`, `stpnhb`, `stpnhw`, `stpnlp`, `stpnpd`, `stpnpl`, `stpnpo`, `stpnps`, `stpnsp`, `stpntr`, `stpnwp` (autox_lib3) | `start_<kind>`: `start_bl`, `start_c1`, `start_c2`, `start_hb`, `start_hw`, `start_lp`, `start_pd`, `start_pl`, `start_po`, `start_ps`, `start_sp`, `start_tr`, `start_wp` **?** | per-problem start functions | AUTO |
| `fnbl`, `fnc1`, `fnc2`, `fnds`, `fnhb`, `fnhw`, `fnlp`, `fnpd`, `fnpe`, `fnpl`, `fnpo`, `fnps`, `fnsp`, `fnti`, `fntr`, `fnwp`, `fnws` (autox_lib3) | `fn_<kind>` (same kind codes as above) | per-problem functions | AUTO |
| `bcbl`, `bcpd`, `bcpl`, `bcpo`, `bcps`, `bctr`, `icbl`, `icpd`, `icpe`, `icpl`, `icpo`, `icps`, `ictr` | `bc_<kind>`, `ic_<kind>` | per-problem boundary and integral conditions | AUTO |
| `ffbl`, `ffc2`, `ffhb`, `ffhw`, `ffpd`, `ffpe`, `ffpl`, `ffpo`, `ffsp`, `ffws`, `fibl`, `fipo`, `fflp`, `ffho` | `ff_<kind>`, `fi_<kind>` | per-problem Jacobian-function pieces | AUTO |
| `fbbl`, `fbho`, `fiho` | `fb_<kind>`, `fi_homoclinic` | | AUTO |
| `chdim` | `change_dimension` **?** | | AUTO |
| `autox_*` (autox_evd, autox_run, ...) | keep | already readable | ours |

The per-problem kind codes (`bl`, `c1`, `c2`, `hb`, `hw`, `lp`, `pd`, `pe`, `pl`, `po`, `ps`, `sp`, `ti`, `tr`, `wp`, `ws`) are AUTO's problem names: `bl` boundary layer?, `c1`/`c2` codim-2 variants, `hb` Hopf (BP?), `hw` Hopf-wave?, `lp` fold, `pd` period doubling, `pe` periodic-end?, `pl` plane?, `po` periodic orbit, `ps` pseudo?, `sp` special point, `ti` time-integral, `tr` torus, `wp` wave-pattern, `ws` ... **? (the meanings need AUTO-07p's documentation: reference/auto-07p)**. Stage 2 keeps these two-letter codes in the comment until their meaning is settled from AUTO's sources, and names the functions by the problem's full name then (`fold`, `period_doubling`, `torus`, ...) only where the meaning is certain.

### 4.5 conpar, setubv and the autoX support (setubv2.cpp, conpar2.cpp, autox_codim.cpp, autox_pp.cpp)

| Current | Proposed | File |
|---|---|---|
| `setubv` | `setup_problem` **? (dup with 4.1)** | setubv2 |
| `setubv_make_aa_bb_cc`, `setubv_make_fa_aa_bb`, `setubv_make_fc_dd` | keep (already readable) | setubv2 |
| `conpar_process`, `conpar_default_wrapper`, `first_maximum` | keep | conpar2 |
| `go_go_autox`, `one_parameter`, `two_parameter`, `algebraic`, `boundary`, `problem_functions` | keep (readable; `go_go_autox` is a name to rename to `run_autox_problem` **?**) | gogoautox |

## 5. Packs by area (stage 2 order)

1. **Linear algebra** (section 1 and 1.4): `sgefa`, `sgesl`, `saxpy`, `sscal`, `bandfac`, `bandsol`, the EISPACK file, the BLAS-1 names, the three LU copies (merged into xpp_math, not renamed twice). Depends on no other card; the md5s prove it. First pack.
2. **Integrators** (section 6, odesol2, gear, stiff, dormpri, the `d` names). Waits for W377 (dense output) and W469 (the one right-hand side) if they still touch these files.
3. **Equilibria and the stability helpers** (`rooter`, `newton`, `del_stab`'s complex helpers, `f2c_helpers`): after the f2c types go.
4. **autoX numerics** (sections 3 and 4): the largest pack; by subgroup (problem functions, continuation, mesh, HomCont, per-problem kinds). Waits for W596's branch (autox_jobs.cpp) to merge, since it reads these names.
5. **Types** (`integer`, `real`, `doublereal`, `doublecomplex`, the f2c macros): last, in one step across core/, so the aliascheck sees no second name.

## 6. Integrators (origins to confirm against the XPPAUT source before stage 2)

| File | Current | Proposed | Meaning | Origin |
|---|---|---|---|---|
| gear.cpp | `gear` (driver), `ggear` (one step) | `gear_integrate`, `gear_step` | Gear's variable-order multistep method | XPPAUT (its Gear code; **? the exact source**) |
| gear.cpp:35 | `sqr2` (static) | `scaled_norm` **?** | a scaled norm used in step control | XPPAUT |
| stiff.cpp | `stiff` (driver), `adaptive`, `gadaptive`, `jacobn`, `rkck`, `rkqs` | `rosenbrock_integrate`, `rosenbrock_adaptive`, `rosenbrock_adaptive_gear` **?**, `jacobian_numeric`, `cash_karp_step`, `cash_karp_adaptive_step` | the stiff (Rosenbrock) integrator and the Cash-Karp steps it uses | Numerical Recipes (Press et al.) style **? (confirm in reference/xppaut-8.0's stiff.c)** |
| dormpri.cpp | `dormand_prince`, `make_dormand_prince` (readable), `dopcor`, `dopcor5`, `hinit`, `dprhs` | `dopri_correct`, `dopri5_correct`, `dopri_initial_step`, `dopri_rhs` | DOPRI5 and DOP853, Hairer's | Hairer (DOPRI, DOP853) |
| odesol2.cpp | `euler`, `mod_euler`, `rung_kut`, `rb23`, `rosen`, `abmpc`, `adams`, `symplect3`, `bak_euler`, `one_bak_step`, `one_step_*`, `fixed_steps`, `get_the_jac`, `discrete` | `euler_step`, `modified_euler_step`, `runge_kutta_fixed`, `rb23_step`, `rosenbrock_step`, `adams_predictor_corrector`, `adams_step`, `symplectic_step_3`, `backward_euler`, ... | the fixed-step and special methods | XPPAUT (its own; **`rb23`, `rosen`: origin ?**) |
| equilibria.cpp | `rooter`, `newton`, `getjac`, `get_evec`, `pr_evec`, `do_sing`, `do_sing_info` | keep `newton`; `rooter` -> `refine_root` **?**; `getjac` -> `jacobian_at` **?** | equilibrium search | XPPAUT |
| integrate.cpp | `mswtch` (static) | `switch_flag_state` **?** | the global flags' switch | XPPAUT |

## 7. File-scope and constants

| Current | Proposed | Where | Note |
|---|---|---|---|
| `c_b81`, `c_b170`, (and the f2c `c_b*` family) | keep as named constants (`zero_real`, `one_real`) | xpp_math_eispack.cpp | the translated constants |
| `autox_hmach` (autox_c.h:24) | `autox_relative_step` **?** | autox_c.h | AUTO's HMACH: the relative step and convergence tolerance |
| `autox_rlarge` (25) | `autox_practical_infinity` | autox_c.h | AUTO's RLARGE |
| `autox_hmach1` (26) | `autox_one_plus_relative_step` | autox_c.h | 1 + HMACH |
| `autox_hmachho` (27) | `autox_homoclinic_zero_threshold` | autox_c.h | HomCont's compzero |
| `autox_svd_tolerance` (28) | keep | autox_c.h | already readable |
| `autox_divergence_ratio` (29) | keep | | already readable (DELREF) |
| `hopf_start_steps` (autox_lib1:313) | keep | | readable |
| `user_zero_diagnostic_level`, `algebraic_user_zero_width` (autox_lib1:2501-2503) | keep | | readable |

## 8. Collisions and ambiguous names (for the maintainer)

- **Duplicate algorithms (merge, one owner):** hqr and orthes (xpp_math.cpp:421, :470 and xpp_math_eispack.cpp:144 and the xpp_math_autox.h prototype); three LU solves (xpp_math.cpp `sgefa`/`sgesl`, dense.cpp `gefa`/`gesl`, band.cpp `gbfa`/`gbsl`, and `bandfac`/`bandsol`, plus autoX's `ge`); two BLAS-1 sets (`saxpy`/`sscal` in xpp_math.cpp, `daxpy`/`dscal`/`ddot`/`dnrm2`/`drot`/`dswap`/`idamax` in xpp_math_eispack.cpp); the sign helpers (`sign` xpp_math.cpp:410, `d_sign` f2c_helpers.cpp:32); the complex magnitudes (`c_abs` del_stab.cpp:175, `z_abs` f2c_helpers.cpp:38, `f__cabs` f2c_helpers.cpp:11); the FFT family (`fft`, `fft_real` xpp_math.cpp; `fftr` autox_lib3.cpp:1802 and simplenet.cpp; `faft` autox_lib2.cpp:161); `dgemm` (xpp_math.cpp, xpp_math_autox.cpp:519, eispack); the eigenvalue family (`eig` xpp_math_autox.cpp:54, `eigenvalues` xpp_math.cpp:617, `rg` and `eighi`, `eigho` in both xpp_math_autox.cpp:966 and autox_lib5.cpp).
- **Same name, different things:** `interp` (autox_lib1.cpp:3896 vs tabular.cpp and simplenet.cpp); `reduce` (autox_lib2.cpp:599, the tree condensation, vs the xpp_math helpers' use of the word); `eig` (xpp_math_autox.cpp:54 vs csv_export.cpp:102, a file name); `stpnt` (the start in autox_pp.cpp and lib1/3/5, a macro family).
- **Ambiguous (the meaning to settle before a name is chosen):** `prjctn`, `nlvc`, `flowkm`, `dhhap`, `dhhpr`, `hqr3lc`, `dgemc`, `ezsvd`, `ndsvd`, `sndrtg`, `prse`, `sig22`, `eigho`, `eighi`, `brbd`, `dimrge`, `swprc`, `chdim`, `tpspbv`, `faft`, `wint`, `genwts`, `intwts`, `rintg`, `msh`, `newlab`, `stbif`, `sthd`, `cntdif`, `cpnts`, `eqdf`, `readlb`, `findlb`, `ordr`, `skip3`, `scaleb`, the two-letter problem kinds (section 4.4), the AUTO and HomCont names (`ffho`, `fbho`, `fiho`, `icho`, `bcho`, `fnho`, `inho`, `preho`), and the integrator names in section 6 (`rb23`, `rosen`, `gear`'s origin). Each is marked **?** above.
- **Names a user can see:** the parser's `dle`, `dlt`, `dge`, `dne`, `dnot` (section 2.3) are built-ins of the expression language (the manual does not name them; the maintainer decides whether the parser names change). AUTO's `ISW`, `IPS`, `ITP`, the fort.7/8/9 column names and the `NTST`/`NCOL` in the output are the file formats: they stay as they are.

## 9. Counts (approximate, by grep)

- Functions and routines (definitions and file-scope helpers, core/ outside third_party/, excluding the vendored CVODE and the xppaut-only helpers already readable): about 260 names in sections 1, 2, 4 and 6; about 40 in section 4.4 alone.
- Types and macros: 4 f2c types, 9 function-pointer macros, 3 function-list structs, 2 common-block structs (`iap_type`, `rap_type`), 1 nested AutoxRun group of 8 structs (section 3.2).
- Struct fields: 37 iap fields, 15 rap fields, 8 nested AutoxRun structs with about 25 fields (section 3).
- File-scope constants: about 10 (section 7).
- Locals: the f2c `i__N`, `z__N`, `a_dim1`, `a_offset` family (about 1,400 uses in xpp_math_eispack.cpp; a few hundred more in autox_lib*.cpp); a script in stage 2 lists them.

## 10. Not settled (to check before stage 2)

- The origins marked **? (XPPAUT's Gear, rb23, rosen, the stiff routine, rooter, mswtch)**: check against reference/xppaut-8.0 (the 8.0 source) before the comment at each definition says where it comes from.
- The meanings of AUTO's two-letter problem kinds and the iap/rap fields marked **?**: check against reference/auto-07p's documentation and source before the names are chosen.
- The dup pairs in section 8: which copy is the owner, and whether the two differ in any number (the examples md5s would show it): decided in stage 2, not here.
