# W390 Readable names for the numerical code: stage 1 inventory (#452)

Read-only inventory (maintainer, 2026-10-15: "can we change these ugly names like sgefa and sgesl"). No code changed. Line numbers are master at 3fbeb7eb. third_party/ is excluded; the vendored CVODE's API stays until W34, so its names are listed only where core/ has its own copies.

Scope read: function, type, struct field and file-scope names in core/ (autoX's files included: autox_*, setubv*, gogoautox, conpar*, diagram), found by grep for Fortran, LINPACK, EISPACK, f2c and AUTO-style names, then the definitions read where a name's meaning was unclear. Local variables (the f2c `i__1`, `z__1`, `a_dim1` family, about 1,400 occurrences in xpp_math_eispack.cpp alone) are summarised by pattern, not listed one by one: stage 2 generates the full local list by script, per pack.

Proposed names follow core's own snake_case (a function says what it does; a type says what it is). "Origin" says where the algorithm comes from: LINPACK, EISPACK, BLAS, Hairer (DOPRI), Numerical Recipes, XPPAUT (its own code, cited in docs/xppaut-findings.md), AUTO-07p / AUTO2000 (the autoX Fortran, translated by f2c, so it is ours: AGENTS.md "One codebase"), CVODE, or f2c (the runtime helpers). Stage 2's first commit (this file as it now stands) settled every name that was marked `?` against the origin's own source (reference/auto-07p, reference/xppaut-8.0): each row below cites the source line it was named from, and no name keeps an abbreviation or a Fortran letter code.

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

## 0. Rules (applied in stage 2)

1. **One naming style:** core's snake_case, no abbreviation and no Fortran letter code; a name says what the routine does. Fortran's `s`/`d` precision prefixes go.
2. **Keep the origin:** each renamed routine keeps its original name in a one-line comment at its definition, e.g. `/* sgefa: LINPACK's factor */`, so the studies, docs/xppaut-findings.md and docs/auto07p-findings.md, which cite XPPAUT's and AUTO-07p's sources, still trace. Those two findings files are not edited: their citations are XPPAUT's and AUTO-07p's own names.
3. **AUTO's documented names stay:** `iap_type` and `rap_type`'s fields (`ndim`, `ips`, `ntst`, `ncol`, `isw`, `rl0`, ...) are AUTO's documented constants (fort.7, fort.8, the diagram, the manual), so the field names are kept; section 8 says why none was renamed.
4. **Types:** the f2c types become the std types directly, no alias (aliascheck). An f2c complex becomes `std::complex<double>` only where every operation keeps f2c's formula.
5. **Duplicates merge** where one function gives the same bits as the other (the examples md5s and a bitwise comparison prove it); a pair that differs in a number is two functions with two names (section 9).
6. **Packs by area, one commit each,** numbers bit-identical (examples md5s, autoxcompare, goldencheck, odexcheck).

## What the kind codes of the per-problem functions mean (section 5)

AUTO's two-letter codes name the problem kind a function belongs to (AUTO-07p's `FN`, `BC`, `IC`, `FB`, `FI` and `STPN` prefixes, then the kind): `fn` the equations, `ff` the function evaluation `fn` calls, `bc` the boundary conditions, `fb` their evaluation, `ic` the integral conditions, `fi` their evaluation, `stpn` the starting data. The kinds, from the headline of the AUTO-07p or XPPAUT routine:

| Code | Kind | Source |
|---|---|---|
| `lp` | `fold` | toolboxae.f90:304 STPNLP: continuation of folds |
| `c1` | `optimization_single` | optimization.f90:114 FNC1: optimization of algebraic systems (one parameter) |
| `c2` | `optimization_multi` | optimization.f90:191 FNC2: optimization of algebraic systems (more than one parameter) |
| `ds` | `discrete_map` | maps.f90:62 FNDS: continuing fixed points of a map |
| `ti` | `time_integration` | timeint.f90:37 FNTI: time integration |
| `hb` | `hopf` | equilibrium.f90:67 FNHB: 2-parameter continuation of Hopf points in ODEs and maps |
| `hw` | `hopf_wave` | parabolic.f90:234 FNHW: 2-parameter continuation of Hopf points in waves |
| `ws` | `wave_steady` | parabolic.f90:110 FNWS: spatially homogeneous solutions of parabolic systems, to find travelling-wave bifurcations (IPS 11) |
| `wp` | `wave_train` | XPPAUT 8.0 autlib3.c:1018: continuation of travelling waves (IPS 12) |
| `sp` | `parabolic_stationary` | autox_lib3.cpp:1157: parabolic PDEs, stationary states |
| `pe` | `parabolic_evolution` | parabolic.f90:389 FNPE: one implicit Euler time step; autox_lib3.cpp:1217: time evolution of parabolic PDEs |
| `ps` | `periodic` | periodic.f90:352 STPNPS: continuation of periodic solutions |
| `pd` | `period_doubling` | periodic.f90:1459 STPNPD: 2-parameter continuation of period doublings |
| `pl` | `periodic_fold` | periodic.f90:631 STPNPL: 2-parameter continuation of folds of periodic solutions |
| `tr` | `torus` | periodic.f90:1728 STPNTR: 2-parameter continuation of torus bifurcations |
| `po` | `periodic_optimization` | optimization.f90:370,612: optimization of periodic solutions |
| `bl` | `boundary_fold` | toolboxbv.f90:839 STPNBL: 2-parameter continuation of folds of a BVP |
| `ho` | `homoclinic` | homcont.f90: HomCont's homoclinic problem (IPS 9) |

## 1. Linear algebra (LU, banded, dense, Householder, EISPACK, BLAS, SVD)

| Current | Final name | Meaning and source |
|---|---|---|
| `sgefa` | `lu_factor` | dense LU with partial pivoting in place (LINPACK sgefa) |
| `sgesl` | `lu_solve` | solve with sgefa's factors (LINPACK sgesl) |
| `sgefa_columns` | `lu_factor_columns` | sgefa for a transposed (column-major) block (xpp_math.cpp) |
| `sgesl_columns` | `lu_solve_columns` | sgesl for a transposed block (xpp_math.cpp) |
| `sgefa_det_sign` | `lu_determinant_sign` | sign of the determinant from the pivots (xpp_math.cpp) |
| `saxpy` | `add_multiple` | y += a x, strided (BLAS level 1 axpy; double code, the `s` misled) |
| `sscal` | `scale_vector` | x *= a, strided (BLAS level 1 scal) |
| `bandfac` | `band_factor` | banded LU without pivoting (xpp_math.cpp; not CVODE's pivoting gbfa) |
| `bandsol` | `band_solve` | solve with bandfac's factors |
| `eigenvalues` | `real_eigenvalues` | eigenvalues of a real matrix through orthes and hqr |
| `prjctn` | `homoclinic_projection_conditions` | NUNSTAB (or NSTAB) projection boundary conditions onto the unstable (stable) manifold of an equilibrium: AUTO-07p homcont.f90:2151 |
| `eigho` | `homoclinic_eigen_ordered` | DGEEV eigenvalues/vectors of HomCont's linearization, ordered by real part: homcont.f90:2005 |
| `dhhap` | `householder_apply` | applies a Householder plane rotation defined by v and beta to a matrix: AUTO-07p floquet.f90:448 |
| `dhhpr` | `householder_compute` | computes a Householder plane rotation (Golub-van Loan Alg. 3.3-1): floquet.f90:311 |
| `nlvc` | `matrix_null_vector` | finds a null vector of a singular matrix whose null space is k-dimensional: AUTO-07p support.f90:230 |
| `flowkm` | `floquet_multipliers` | Floquet multipliers by the deflated circuit pencil method: floquet.f90:64 |
| `ge` | `gauss_eliminate` | Gaussian elimination with complete pivoting, determinant and pivot log (AUTO's GE, xpp_math_autox.cpp) |
| `eig` | `autox_eigenvalues` | eigenvalues of autoX's Jacobian through rg, as complex numbers (xpp_math_autox.cpp:54) |
| `xerbla` | `blas_error` | BLAS error handler |
| `lsame` | `same_letter` | BLAS: case-insensitive letter compare |
| `rg` | `real_general_eigen` | EISPACK rg: eigenvalues (and vectors) of a real general matrix |
| `hqr2` | `hessenberg_qr_vectors` | EISPACK hqr2: eigenvalues and vectors of a Hessenberg matrix |
| `hqr3lc` | `hessenberg_quasi_triangular` | HQR3 reduces an upper Hessenberg matrix to quasi-triangular form by unitary similarity (hqr3_loc.f: XPPAUT 8.0 eispack.c:6257) |
| `balanc` | `balance_matrix` | EISPACK balanc |
| `balbak` | `balance_back_transform` | EISPACK balbak |
| `elmhes` | `hessenberg_reduce_elimination` | EISPACK elmhes: Hessenberg reduction by elimination |
| `eltran` | `hessenberg_transform_elimination` | EISPACK eltran: elmhes's accumulated transform |
| `ortran` | `hessenberg_transform_orthogonal` | EISPACK ortran: orthes's accumulated transform |
| `qzhes` | `qz_reduce_hessenberg` | EISPACK qzhes: first QZ step |
| `qzit` | `qz_iterate` | EISPACK qzit |
| `qzval` | `qz_eigenvalues` | EISPACK qzval |
| `epslon` | `machine_epsilon` | EISPACK epslon: the rounding unit |
| `exchng` | `exchange_diagonal_blocks` | exchanges consecutive 1x1 or 2x2 diagonal blocks of an upper Hessenberg matrix with their eigenvalues, accumulating the transform: XPPAUT 8.0 eispack.c:6804 |
| `qrstep` | `implicit_qr_step` | one implicit QR step on rows and columns nl..nu of an upper Hessenberg matrix: eispack.c:6977 |
| `sig22` | `singular_values_2x2` | SVD of the 2x2 upper triangular matrix [[a,b],[0,c]]: eispack.c:5903 |
| `sndrtg` | `rotation_matching_signs` | ndrotg's variant with sign(f)=sign(cs), sign(g)=sign(sn): eispack.c:6216 |
| `ndrotg` | `givens_rotation` | faster drotg, g unchanged on return, cs and sn with -sn*f+cs*g = 0: eispack.c:4519 |
| `prse` | `print_bidiagonal` | debug routine to print s and e: eispack.c:5862 |
| `ezsvd` | `singular_value_decomposition` | Demmel-Kahan SVD, easy-to-use front of ndsvd with defaults: eispack.c:4440 |
| `ndsvd` | `singular_value_decomposition_detailed` | LINPACK SVD modified by Demmel and Kahan for relative accuracy, every parameter given: eispack.c:4574 |
| `sigmin` | `smallest_singular_value_2x2` | smallest singular value of the 2x2 matrix ((a,b),(0,c)): eispack.c:6158 |
| `dnrm2` | `euclidean_norm` | BLAS level 1 nrm2 |
| `ddot` | `dot_product` | BLAS level 1 dot |
| `dscal` | `scale_vector` | BLAS level 1 scal; one with sscal |
| `idamax` | `largest_magnitude_index` | BLAS level 1 iamax |
| `daxpy` | `add_multiple` | BLAS level 1 axpy; one with saxpy |
| `drot` | `rotate_plane` | BLAS level 1 rot |
| `dswap` | `swap_vectors` | BLAS level 1 swap |
| `dgemc` | `copy_matrix_transposed` | copies an M by N matrix, transposed when TRANS is true: eispack.c:3773 |
| `dgemm` | `matrix_multiply` | BLAS level 3 gemm |

## 2. f2c and complex helpers, the parser's comparison functions

| Current | Final name | Meaning and source |
|---|---|---|
| `d_sign` | `transfer_sign` | FORTRAN SIGN(a,b): |a| with the sign of b, -0 counts as positive (f2c d_sign; one with xpp_math's sign) |
| `f__cabs` | `complex_magnitude` | |re + i im| without overflow in the squares (f2c cabs.c) |
| `z_abs` | `complex_magnitude_of` | magnitude of an f2c complex (f2c z_abs) |
| `cdeterm` | `complex_determinant` | determinant of a complex matrix (XPPAUT del_stab.c) |
| `cdif` | `complex_subtract` | complex difference (XPPAUT del_stab.c) |
| `cmlt` | `complex_multiply` | complex product (XPPAUT del_stab.c) |
| `cdivv` | `complex_divide` | complex quotient (XPPAUT del_stab.c) |
| `cexp2` | `complex_exponential` | complex exponential (XPPAUT del_stab.c) |
| `rtoc` | `make_complex` | builds a complex from its parts (XPPAUT del_stab.c) |
| `make_z` | `fill_characteristic_matrix` | fills the delay system's characteristic matrix at lambda (del_stab.cpp) |
| `c_abs` | `complex_modulus` | sqrt(re^2+im^2) of a del_stab complex (not the overflow-safe cabs: different bits) |
| `cdiv` | `complex_divide_scaled` | EISPACK cdiv: complex division by the scaled Smith form (xpp_math_eispack.cpp) |
| `dle` | `less_equal` | x <= y as 1 or 0, the expression language's operator (expr_functions.cpp) |
| `dlt` | `less_than` | x < y as 1 or 0 |
| `dge` | `greater_equal` | x >= y as 1 or 0 |
| `dne` | `not_equal` | x != y as 1 or 0 |
| `dnot` | `logical_not` | not x as 1 or 0 |

## 3. autoX: drivers, user functions, start and set-up

| Current | Final name | Meaning and source |
|---|---|---|
| `autoae` | `continue_algebraic_problem` | AUTO-07p ae.f90:19 AUTOAE: the driver for algebraic problems |
| `autobv` | `continue_boundary_problem` | AUTO-07p bvp.f90:22 AUTOBV: the driver for boundary value problems |
| `funi` | `problem_function` | AUTO's FUNI (interfaces.f90:60): the interface to the user's right-hand side |
| `bcni` | `boundary_function` | AUTO's BCNI (interfaces.f90:141): the interface to the user's boundary conditions |
| `icni` | `integral_function` | AUTO's ICNI (interfaces.f90:236): the interface to the user's integral conditions |
| `stpnt` | `user_start` | AUTO's user routine STPNT (user_c.f90:115): the starting solution |
| `func` | `user_right_hand_side` | AUTO's user routine FUNC (user_c.f90:102) |
| `bcnd` | `user_boundary_conditions` | AUTO's user routine BCND (user_c.f90:127) |
| `icnd` | `user_integral_conditions` | AUTO's user routine ICND (user_c.f90:140) |
| `fopt` | `user_objective` | AUTO's user routine FOPT (user_c.f90:154): the objective of an optimization |
| `fopi` | `objective_interface` | interface to the user's FOPT, objective and Jacobian by differencing (XPPAUT 8.0 autlib3.c:4890) |
| `pvls` | `user_output_parameters` | AUTO's user routine PVLS: additional output parameter values |
| `setubv` | `setup_boundary_system` | AUTO's SETUBV (solvebv.f90:293): the collocation system's blocks |
| `conpar` | `condense_parameters` | AUTO's CONPAR (solvebv.f90:777): condensation of the system by eliminating the local variables |
| `go_go_autox` | `run_autox_problem` | gogoautox.cpp: builds the problem's function table and runs it |
| `function_list` | `problem_function_table` | a run's function table |
| `autobv_function_list` | `boundary_function_table` | special-point test function: AUTO-07p's FN... of ae.f90, bvp.f90 or floquet.f90 (fnbt, fnrn, fncp, fngh, fnclosed are autoX's own, autox_lib1.cpp) |
| `autoae_function_list` | `algebraic_function_table` | special-point test function: AUTO-07p's FN... of ae.f90, bvp.f90 or floquet.f90 (fnbt, fnrn, fncp, fngh, fnclosed are autoX's own, autox_lib1.cpp) |
| `stpnae` | `algebraic_start` | start of an algebraic problem from the user's start (autox_lib1.cpp) |
| `stpnus` | `user_start_algebraic` | start of an algebraic problem straight from the user's values |
| `stpnbv` | `boundary_start` | start of a boundary value problem from the user's start or a restart (autox_lib1.cpp) |
| `stpnub` | `user_start_boundary` | start of a boundary value problem from the user's values |
| `pvlsae` | `output_parameters_algebraic` | output hook of algebraic problems (PVLI_TYPE_AE) |
| `pvlsbv` | `output_parameters_boundary` | output hook of boundary value problems (PVLI_TYPE_BVP) |
| `init` | `initialize_run` | AUTO's INIT: reads the constants and generates the starting data (autlib1.c:417 for init1) |
| `init1` | `initialize_constants` | general initialization, redefinition of constants, check and perturb step size and limits: XPPAUT 8.0 autlib1.c:417 |
| `chdim` | `check_dimensions` | checks dimensions: XPPAUT 8.0 autlib1.c:360 |
| `pi` | `multiple_of_pi` | r times pi (autox_lib1.cpp) |

## 4. autoX: continuation, mesh, condensation and output

| Current | Final name | Meaning and source |
|---|---|---|
| `solvae` | `solve_algebraic` | AUTO ae.f90:435: main predictor-corrector loop (Newton) of algebraic problems |
| `solvbv` | `solve_boundary` | AUTO solvebv.f90:26: the Newton solve of the collocation system |
| `stepbv` | `step_boundary` | AUTO bvp.f90:464: controls Newton's solve for the next point on a branch (BVP) |
| `stdrbv` | `start_direction_boundary` | AUTO bvp.f90:999: direction vector to start a branch when none is given (BVP) |
| `stdirae` | `start_direction_algebraic` | algebraic problems' start direction (autox_lib1.cpp:1812) |
| `stprae` | `second_point_algebraic` | AUTO ae.f90:271: finds the second point on the initial branch |
| `stpswae` | `switch_branch_algebraic` | the branch-switch step of algebraic problems (autox_lib1.cpp:1363) |
| `swpnt` | `retrieve_branching_data` | AUTO/XPPAUT autlib1.c:2166: retrieves the branching data U, U-dot, PAR(ICP(1)) |
| `swprc` | `second_branch_point` | controls the second point on a bifurcating branch: XPPAUT 8.0 autlib1.c:2244 |
| `stplae` | `store_diagram_algebraic` | AUTO ae.f90:870: stores the bifurcation diagram on unit 7 (algebraic) |
| `stplbv` | `store_diagram_boundary` | AUTO bvp.f90:1475: writes the bifurcation diagram on unit 7 (BVP) |
| `stupbv` | `store_derivative` | AUTO bvp.f90:428: stores U-prime in UPOLDP |
| `cnrlae` | `control_algebraic` | AUTO ae.f90:41: controls the bifurcation analysis of algebraic problems |
| `cnrlbv` | `control_boundary` | AUTO bvp.f90:79: controls the computation of solution branches |
| `contae` | `extrapolate_algebraic` | XPPAUT 8.0 autlib1.c:1298: initial approximation to the next solution by extrapolating the two preceding points |
| `contbv` | `extrapolate_boundary` | AUTO bvp.f90:365: new rates of change from the two latest points, then the current point becomes the old one |
| `extrbv` | `predict_boundary_null_space` | XPPAUT 8.0 autlib1.c:5414: initial approximation to the next solution by the null space of the Jacobian |
| `lcspae` | `locate_special_algebraic` | AUTO ae.f90:624: secant method locating special points (algebraic) |
| `lcspbv` | `locate_special_boundary` | AUTO bvp.f90:1230: secant method locating folds, branch points, user zeros (BVP) |
| `rsptbv` | `restart_boundary` | AUTO bvp.f90:750: restarts a branch at the point labelled IRS |
| `setrtn` | `initialize_rotations` | AUTO bvp.f90:975: initialization for rotations |
| `setrhs` | `build_right_hand_side` | builds the collocation system's blocks and right-hand side (autox_lib2.cpp:218; XPPAUT 8.0 autlib2.c:520) |
| `setzero` | `zero_right_hand_side` | zeroes the right-hand side blocks (autox_lib2.cpp:744) |
| `conrhs` | `condense_right_hand_side` | AUTO solvebv.f90:931 CONRHS: condensation of the right-hand side |
| `copycp` | `copy_condensed_system` | AUTO solvebv.f90:970: copies the condensed system CONPAR made into workspace |
| `cpyrhs` | `copy_right_hand_side` | AUTO solvebv.f90:1009: copies the right-hand side |
| `brbd` | `solve_bordered_block_diagonal` | AUTO solvebv.f90:544: solves linear systems with the bordered almost-block-diagonal profile |
| `bcksub` | `back_substitute` | AUTO solvebv.f90:1510: back substitution |
| `redrhs` | `reduce_right_hand_side` | reduction of the right-hand side in the condensation tree (autox_lib2.cpp) |
| `reduce` | `reduce_condensation` | AUTO solvebv.f90:1031: reduces the non-overlapping, then overlapping, pieces of the condensation tree |
| `dimrge` | `solve_end_system` | AUTO solvebv.f90:1408: assembles and solves the small end system from the first and last blocks, the boundary rows and the integral rows |
| `infpar` | `determine_local_variables` | XPPAUT 8.0 autlib2.c:2414: the local variables by back substitution after the parameters |
| `print1` | `print_system` | prints the blocks of the condensed system (autox_lib2.cpp:1039; AUTO solvebv.f90:1697) |
| `faft` | `transpose_right_hand_side` | copies ff(j,i) into fa(i,j) (autox_lib2.cpp:199) |
| `newmsh` | `redistribute_mesh` | AUTO mesh.f90:395: redistributes the mesh according to EQDF |
| `newlab` | `new_restart_label` | AUTO io.f90:1159: a suitable label when restarting |
| `msh` | `uniform_mesh` | AUTO mesh.f90:19: a uniform mesh on [0,1] |
| `adapt` | `adapt_mesh` | AUTO mesh.f90:259: adapts the mesh so EQDF's increase is equidistributed; interpolates UPS and VPS |
| `adptds` | `adapt_step` | AUTO mesh.f90:224: adapts the step along the branch to the previous step's Newton iterations (IADS > 0) |
| `cntdif` | `central_difference_coefficients` | AUTO mesh.f90:135: central difference coefficients for the Nth derivative at uniform points |
| `cpnts` | `collocation_point_positions` | AUTO mesh.f90:85: the collocation points on [0,1] |
| `genwts` | `collocation_weights` | AUTO mesh.f90:35: weights of the collocation method for the value and the first derivative |
| `wint` | `integration_weights` | AUTO mesh.f90:160: integration weights from interpolation at NCOL+1 equally spaced points |
| `intwts` | `lagrange_weights` | AUTO mesh.f90:451: weights for Lagrange interpolation |
| `interp` | `interpolate_mesh` | AUTO mesh.f90:354: interpolant on the new mesh (not tabular.cpp's interp) |
| `eqdf` | `equidistribution_function` | AUTO mesh.f90:480: the monotone function whose increase the mesh equidistributes |
| `ordr` | `order_mesh_points` | XPPAUT 8.0 autlib1.c:3816: index of the old-mesh interval each new point lies in |
| `rinpr` | `mesh_inner_product` | XPPAUT 8.0 autlib1.c:4622: L2 inner product of UPS and VPS, the first NDIM1 components |
| `rnrmsq` | `norm_squared` | AUTO mesh.f90:554: norm-squared of UPS, the first NDIM1 components |
| `rintg` | `integral_of_component` | AUTO mesh.f90:641: integral of the IC'th component of UPS |
| `rnrm2` | `component_norm` | AUTO mesh.f90:711: L2 norm of the IC'th component of UPS |
| `rmxups` | `component_maximum` | AUTO mesh.f90:781: maximum of the I'th component of UPS |
| `rmnups` | `component_minimum` | AUTO mesh.f90:852: minimum of the I'th component of UPS |
| `nrmlz` | `normalize_vector` | AUTO support.f90:273: scales V to discrete L2 norm 1 |
| `scaleb` | `scale_direction` | AUTO mesh.f90:903: scales (DVPS,RLD) to norm 1 |
| `mueller` | `mueller_root` | AUTO support.f90:31: Mueller's method with bracketing |
| `findlb` | `find_restart_label` | AUTO io.f90:1212: finds the restart label in the solution file |
| `readlb` | `read_restart_label` | AUTO io.f90:1398: reads the solution at the restart label |
| `sthd` | `write_header` | AUTO io.f90:561: writes the user-defined constants on unit 7, preceded by 0 |
| `headng` | `write_headings` | AUTO io.f90:964: writes the column headings |
| `stbif` | `store_branching_data` | AUTO ae.f90:799: stores the branching data (STU, STUD) |
| `wrjac` | `write_jacobian` | AUTO ae.f90:1053: writes the Jacobian |
| `wrline` | `write_diagram_line` | AUTO io.f90:1090: one line of output on units 6 and 7 |
| `wrtsp8` | `write_special_point_restart` | AUTO ae.f90:989: restart information on special points, plotting points, on unit 8 |
| `wrtbv8` | `write_solution_restart` | AUTO bvp.f90:1622: plotting and restart data on unit 8 |
| `wrtbv9` | `write_solution_extra` | AUTO bvp.f90:1803: additional output on unit 9 |
| `tpspbv` | `secondary_bifurcation_type` | XPPAUT 8.0 autlib1.c:6885: type of a secondary periodic bifurcation |
| `pdble` | `period_doubling_restart` | AUTO periodic.f90:321: preprocesses restart data for switching branches at a period doubling |
| `fnlpae` | `fold_test_algebraic` | special-point test function: AUTO-07p's FN... of ae.f90, bvp.f90 or floquet.f90 (fnbt, fnrn, fncp, fngh, fnclosed are autoX's own, autox_lib1.cpp) |
| `fnlpbv` | `fold_test_boundary` | special-point test function: AUTO-07p's FN... of ae.f90, bvp.f90 or floquet.f90 (fnbt, fnrn, fncp, fngh, fnclosed are autoX's own, autox_lib1.cpp) |
| `fnbpae` | `branch_point_test_algebraic` | special-point test function: AUTO-07p's FN... of ae.f90, bvp.f90 or floquet.f90 (fnbt, fnrn, fncp, fngh, fnclosed are autoX's own, autox_lib1.cpp) |
| `fnbpbv` | `branch_point_test_boundary` | special-point test function: AUTO-07p's FN... of ae.f90, bvp.f90 or floquet.f90 (fnbt, fnrn, fncp, fngh, fnclosed are autoX's own, autox_lib1.cpp) |
| `fnhbae` | `hopf_test_algebraic` | special-point test function: AUTO-07p's FN... of ae.f90, bvp.f90 or floquet.f90 (fnbt, fnrn, fncp, fngh, fnclosed are autoX's own, autox_lib1.cpp) |
| `fnbtae` | `bogdanov_takens_test_algebraic` | special-point test function: AUTO-07p's FN... of ae.f90, bvp.f90 or floquet.f90 (fnbt, fnrn, fncp, fngh, fnclosed are autoX's own, autox_lib1.cpp) |
| `fnrnae` | `resonance_test_algebraic` | special-point test function: AUTO-07p's FN... of ae.f90, bvp.f90 or floquet.f90 (fnbt, fnrn, fncp, fngh, fnclosed are autoX's own, autox_lib1.cpp) |
| `fncpae` | `cusp_test_algebraic` | special-point test function: AUTO-07p's FN... of ae.f90, bvp.f90 or floquet.f90 (fnbt, fnrn, fncp, fngh, fnclosed are autoX's own, autox_lib1.cpp) |
| `fnghae` | `generalized_hopf_test_algebraic` | special-point test function: AUTO-07p's FN... of ae.f90, bvp.f90 or floquet.f90 (fnbt, fnrn, fncp, fngh, fnclosed are autoX's own, autox_lib1.cpp) |
| `fnuzae` | `user_zero_test_algebraic` | special-point test function: AUTO-07p's FN... of ae.f90, bvp.f90 or floquet.f90 (fnbt, fnrn, fncp, fngh, fnclosed are autoX's own, autox_lib1.cpp) |
| `fnuzbv` | `user_zero_test_boundary` | special-point test function: AUTO-07p's FN... of ae.f90, bvp.f90 or floquet.f90 (fnbt, fnrn, fncp, fngh, fnclosed are autoX's own, autox_lib1.cpp) |
| `fnspbv` | `secondary_bifurcation_test_boundary` | special-point test function: AUTO-07p's FN... of ae.f90, bvp.f90 or floquet.f90 (fnbt, fnrn, fncp, fngh, fnclosed are autoX's own, autox_lib1.cpp) |
| `fnclosedae` | `closed_test_algebraic` | special-point test function: AUTO-07p's FN... of ae.f90, bvp.f90 or floquet.f90 (fnbt, fnrn, fncp, fngh, fnclosed are autoX's own, autox_lib1.cpp) |
| `fnclosedbv` | `closed_test_boundary` | special-point test function: AUTO-07p's FN... of ae.f90, bvp.f90 or floquet.f90 (fnbt, fnrn, fncp, fngh, fnclosed are autoX's own, autox_lib1.cpp) |

## 5. autoX: the per-problem-kind functions

| Current | Final name | Meaning and source |
|---|---|---|
| `fnlp` | `fold_equations` | AUTO's equations of the problem kind fold (toolboxae.f90:304 STPNLP: continuation of folds) |
| `fnc1` | `optimization_single_equations` | AUTO's equations of the problem kind optimization_single (optimization.f90:114 FNC1: optimization of algebraic systems (one parameter)) |
| `fnc2` | `optimization_multi_equations` | AUTO's equations of the problem kind optimization_multi (optimization.f90:191 FNC2: optimization of algebraic systems (more than one parameter)) |
| `fnds` | `discrete_map_equations` | AUTO's equations of the problem kind discrete_map (maps.f90:62 FNDS: continuing fixed points of a map) |
| `fnti` | `time_integration_equations` | AUTO's equations of the problem kind time_integration (timeint.f90:37 FNTI: time integration) |
| `fnhb` | `hopf_equations` | AUTO's equations of the problem kind hopf (equilibrium.f90:67 FNHB: 2-parameter continuation of Hopf points in ODEs and maps) |
| `fnhw` | `hopf_wave_equations` | AUTO's equations of the problem kind hopf_wave (parabolic.f90:234 FNHW: 2-parameter continuation of Hopf points in waves) |
| `fnws` | `wave_steady_equations` | AUTO's equations of the problem kind wave_steady (parabolic.f90:110 FNWS: spatially homogeneous solutions of parabolic systems, to find travelling-wave bifurcations (IPS 11)) |
| `fnwp` | `wave_train_equations` | AUTO's equations of the problem kind wave_train (XPPAUT 8.0 autlib3.c:1018: continuation of travelling waves (IPS 12)) |
| `fnsp` | `parabolic_stationary_equations` | AUTO's equations of the problem kind parabolic_stationary (autox_lib3.cpp:1157: parabolic PDEs, stationary states) |
| `fnpe` | `parabolic_evolution_equations` | AUTO's equations of the problem kind parabolic_evolution (parabolic.f90:389 FNPE: one implicit Euler time step; autox_lib3.cpp:1217: time evolution of parabolic PDEs) |
| `fnps` | `periodic_equations` | AUTO's equations of the problem kind periodic (periodic.f90:352 STPNPS: continuation of periodic solutions) |
| `fnpd` | `period_doubling_equations` | AUTO's equations of the problem kind period_doubling (periodic.f90:1459 STPNPD: 2-parameter continuation of period doublings) |
| `fnpl` | `periodic_fold_equations` | AUTO's equations of the problem kind periodic_fold (periodic.f90:631 STPNPL: 2-parameter continuation of folds of periodic solutions) |
| `fntr` | `torus_equations` | AUTO's equations of the problem kind torus (periodic.f90:1728 STPNTR: 2-parameter continuation of torus bifurcations) |
| `fnpo` | `periodic_optimization_equations` | AUTO's equations of the problem kind periodic_optimization (optimization.f90:370,612: optimization of periodic solutions) |
| `fnbl` | `boundary_fold_equations` | AUTO's equations of the problem kind boundary_fold (toolboxbv.f90:839 STPNBL: 2-parameter continuation of folds of a BVP) |
| `fnho` | `homoclinic_equations` | AUTO's equations of the problem kind homoclinic (homcont.f90: HomCont's homoclinic problem (IPS 9)) |
| `fflp` | `fold_function` | AUTO's function evaluation of the problem kind fold (toolboxae.f90:304 STPNLP: continuation of folds) |
| `ffc2` | `optimization_multi_function` | AUTO's function evaluation of the problem kind optimization_multi (optimization.f90:191 FNC2: optimization of algebraic systems (more than one parameter)) |
| `ffhb` | `hopf_function` | AUTO's function evaluation of the problem kind hopf (equilibrium.f90:67 FNHB: 2-parameter continuation of Hopf points in ODEs and maps) |
| `ffhw` | `hopf_wave_function` | AUTO's function evaluation of the problem kind hopf_wave (parabolic.f90:234 FNHW: 2-parameter continuation of Hopf points in waves) |
| `ffws` | `wave_steady_function` | AUTO's function evaluation of the problem kind wave_steady (parabolic.f90:110 FNWS: spatially homogeneous solutions of parabolic systems, to find travelling-wave bifurcations (IPS 11)) |
| `ffsp` | `parabolic_stationary_function` | AUTO's function evaluation of the problem kind parabolic_stationary (autox_lib3.cpp:1157: parabolic PDEs, stationary states) |
| `ffpe` | `parabolic_evolution_function` | AUTO's function evaluation of the problem kind parabolic_evolution (parabolic.f90:389 FNPE: one implicit Euler time step; autox_lib3.cpp:1217: time evolution of parabolic PDEs) |
| `ffpd` | `period_doubling_function` | AUTO's function evaluation of the problem kind period_doubling (periodic.f90:1459 STPNPD: 2-parameter continuation of period doublings) |
| `ffpl` | `periodic_fold_function` | AUTO's function evaluation of the problem kind periodic_fold (periodic.f90:631 STPNPL: 2-parameter continuation of folds of periodic solutions) |
| `fftr` | `torus_function` | AUTO's function evaluation of the problem kind torus (periodic.f90:1728 STPNTR: 2-parameter continuation of torus bifurcations) |
| `ffpo` | `periodic_optimization_function` | AUTO's function evaluation of the problem kind periodic_optimization (optimization.f90:370,612: optimization of periodic solutions) |
| `ffbl` | `boundary_fold_function` | AUTO's function evaluation of the problem kind boundary_fold (toolboxbv.f90:839 STPNBL: 2-parameter continuation of folds of a BVP) |
| `ffho` | `homoclinic_function` | AUTO's function evaluation of the problem kind homoclinic (homcont.f90: HomCont's homoclinic problem (IPS 9)) |
| `bcps` | `periodic_boundary_conditions` | AUTO's boundary conditions of the problem kind periodic (periodic.f90:352 STPNPS: continuation of periodic solutions) |
| `bcpd` | `period_doubling_boundary_conditions` | AUTO's boundary conditions of the problem kind period_doubling (periodic.f90:1459 STPNPD: 2-parameter continuation of period doublings) |
| `bcpl` | `periodic_fold_boundary_conditions` | AUTO's boundary conditions of the problem kind periodic_fold (periodic.f90:631 STPNPL: 2-parameter continuation of folds of periodic solutions) |
| `bctr` | `torus_boundary_conditions` | AUTO's boundary conditions of the problem kind torus (periodic.f90:1728 STPNTR: 2-parameter continuation of torus bifurcations) |
| `bcpo` | `periodic_optimization_boundary_conditions` | AUTO's boundary conditions of the problem kind periodic_optimization (optimization.f90:370,612: optimization of periodic solutions) |
| `bcbl` | `boundary_fold_boundary_conditions` | AUTO's boundary conditions of the problem kind boundary_fold (toolboxbv.f90:839 STPNBL: 2-parameter continuation of folds of a BVP) |
| `bcho` | `homoclinic_boundary_conditions` | AUTO's boundary conditions of the problem kind homoclinic (homcont.f90: HomCont's homoclinic problem (IPS 9)) |
| `icps` | `periodic_integral_conditions` | AUTO's integral conditions of the problem kind periodic (periodic.f90:352 STPNPS: continuation of periodic solutions) |
| `icpd` | `period_doubling_integral_conditions` | AUTO's integral conditions of the problem kind period_doubling (periodic.f90:1459 STPNPD: 2-parameter continuation of period doublings) |
| `icpl` | `periodic_fold_integral_conditions` | AUTO's integral conditions of the problem kind periodic_fold (periodic.f90:631 STPNPL: 2-parameter continuation of folds of periodic solutions) |
| `icpe` | `parabolic_evolution_integral_conditions` | AUTO's integral conditions of the problem kind parabolic_evolution (parabolic.f90:389 FNPE: one implicit Euler time step; autox_lib3.cpp:1217: time evolution of parabolic PDEs) |
| `icpo` | `periodic_optimization_integral_conditions` | AUTO's integral conditions of the problem kind periodic_optimization (optimization.f90:370,612: optimization of periodic solutions) |
| `icbl` | `boundary_fold_integral_conditions` | AUTO's integral conditions of the problem kind boundary_fold (toolboxbv.f90:839 STPNBL: 2-parameter continuation of folds of a BVP) |
| `ictr` | `torus_integral_conditions` | AUTO's integral conditions of the problem kind torus (periodic.f90:1728 STPNTR: 2-parameter continuation of torus bifurcations) |
| `icho` | `homoclinic_integral_conditions` | AUTO's integral conditions of the problem kind homoclinic (homcont.f90: HomCont's homoclinic problem (IPS 9)) |
| `fbbl` | `boundary_fold_boundary_function` | AUTO's boundary function evaluation of the problem kind boundary_fold (toolboxbv.f90:839 STPNBL: 2-parameter continuation of folds of a BVP) |
| `fbho` | `homoclinic_boundary_function` | AUTO's boundary function evaluation of the problem kind homoclinic (homcont.f90: HomCont's homoclinic problem (IPS 9)) |
| `fibl` | `boundary_fold_integral_function` | AUTO's integral function evaluation of the problem kind boundary_fold (toolboxbv.f90:839 STPNBL: 2-parameter continuation of folds of a BVP) |
| `fipo` | `periodic_optimization_integral_function` | AUTO's integral function evaluation of the problem kind periodic_optimization (optimization.f90:370,612: optimization of periodic solutions) |
| `fiho` | `homoclinic_integral_function` | AUTO's integral function evaluation of the problem kind homoclinic (homcont.f90: HomCont's homoclinic problem (IPS 9)) |
| `stpnlp` | `fold_starting_data` | AUTO's starting data of the problem kind fold (toolboxae.f90:304 STPNLP: continuation of folds) |
| `stpnc1` | `optimization_single_starting_data` | AUTO's starting data of the problem kind optimization_single (optimization.f90:114 FNC1: optimization of algebraic systems (one parameter)) |
| `stpnc2` | `optimization_multi_starting_data` | AUTO's starting data of the problem kind optimization_multi (optimization.f90:191 FNC2: optimization of algebraic systems (more than one parameter)) |
| `stpnhb` | `hopf_starting_data` | AUTO's starting data of the problem kind hopf (equilibrium.f90:67 FNHB: 2-parameter continuation of Hopf points in ODEs and maps) |
| `stpnhw` | `hopf_wave_starting_data` | AUTO's starting data of the problem kind hopf_wave (parabolic.f90:234 FNHW: 2-parameter continuation of Hopf points in waves) |
| `stpnwp` | `wave_train_starting_data` | AUTO's starting data of the problem kind wave_train (XPPAUT 8.0 autlib3.c:1018: continuation of travelling waves (IPS 12)) |
| `stpnps` | `periodic_starting_data` | AUTO's starting data of the problem kind periodic (periodic.f90:352 STPNPS: continuation of periodic solutions) |
| `stpnpd` | `period_doubling_starting_data` | AUTO's starting data of the problem kind period_doubling (periodic.f90:1459 STPNPD: 2-parameter continuation of period doublings) |
| `stpnpl` | `periodic_fold_starting_data` | AUTO's starting data of the problem kind periodic_fold (periodic.f90:631 STPNPL: 2-parameter continuation of folds of periodic solutions) |
| `stpntr` | `torus_starting_data` | AUTO's starting data of the problem kind torus (periodic.f90:1728 STPNTR: 2-parameter continuation of torus bifurcations) |
| `stpnpo` | `periodic_optimization_starting_data` | AUTO's starting data of the problem kind periodic_optimization (optimization.f90:370,612: optimization of periodic solutions) |
| `stpnbl` | `boundary_fold_starting_data` | AUTO's starting data of the problem kind boundary_fold (toolboxbv.f90:839 STPNBL: 2-parameter continuation of folds of a BVP) |
| `stpnho` | `homoclinic_starting_data` | AUTO's starting data of the problem kind homoclinic (homcont.f90: HomCont's homoclinic problem (IPS 9)) |

## 6. HomCont and the common blocks

| Current | Final name | Meaning and source |
|---|---|---|
| `inho` | `homoclinic_read_constants` | XPPAUT 8.0 autlib5.c:833: reads HomCont's constants from fort.11, sets IAP's re-defined constants |
| `preho` | `homoclinic_preprocess` | AUTO homcont.f90:1320: special homoclinic orbit preprocessing |
| `stpho` | `homoclinic_start_point` | XPPAUT 8.0 autlib5.c:1191: a starting point for homoclinic continuation (ISTART 2 calls STPNHO, 3 sets up the homotopy) |
| `psiho` | `homoclinic_degeneracy_condition` | AUTO homcont.f90:1795: the degenerate homoclinic conditions PSI(IS)=0 |
| `pvlsho` | `homoclinic_output_parameters` | HomCont's PVLS: eigenvalues and degeneracy parameters (autlib5.c:1331) |
| `eighi` | `homoclinic_eigen` | interface to eigho with its workspace (autlib5.c:1655) |
| `prjcti` | `homoclinic_projection` | interface to prjctn with its workspace (autlib5.c:443) |
| `blhom` | `homoclinic_constants` | common block BLHOM: HomCont's constants (autox_state.h) |
| `blhmp` | `homoclinic_indices` | common block BLHMP: ipsi, ifixed, irev |
| `blhmu` | `homoclinic_endpoints` | common block BLHMU: pu0, pu1, the orbit's end values |
| `bcnn` | `boundary_condition_count` | common block BCNN |
| `blhma` | `homoclinic_zero_threshold` | common block BLHMA: compzero |
| `bleig` | `homoclinic_eigen_workspace` | common block BLEIG: rr, ri, v, vt, xequib, ineig |
| `blhme` | `homoclinic_eigen_check` | common block BLHME: vrprev, ieigc |
| `beyn` | `beyn_workspace` | Beyn's projection method's workspace |

## 7. Integrators and equilibria (not renamed in this stage: see section 10)

| Current | Final name | Meaning and source |
|---|---|---|
| `gear` | `gear_integrate` | XPPAUT 8.0 gear.c:1143: Gear's multistep driver |
| `ggear` | `gear_step` | one Gear step |
| `sqr2` | `square_of` | XPPAUT 8.0 gear.c:1136: z*z |
| `stiff` | `rosenbrock_stiff_step` | Numerical Recipes' stiff (Rosenbrock) step, XPPAUT 8.0 stiff.c |
| `adaptive` | `adaptive_integration` | XPPAUT 8.0 stiff.c:89: adaptive integration, the flag-aware driver (gadaptive when there are no flags) |
| `gadaptive` | `adaptive_integration_without_flags` | adaptive's call when NFlags == 0 (stiff.c:94) |
| `jacobn` | `stiff_numerical_jacobian` | stiff's numerical Jacobian |
| `rkck` | `cash_karp_step` | Numerical Recipes' Cash-Karp Runge-Kutta step |
| `rkqs` | `cash_karp_adaptive_step` | Numerical Recipes' quality-controlled Cash-Karp step |
| `dopcor` | `dormand_prince_853_correct` | Hairer's DOP853 core |
| `dopcor5` | `dormand_prince_5_correct` | Hairer's DOPRI5 core |
| `hinit` | `dormand_prince_initial_step` | Hairer's initial step size |
| `dprhs` | `dormand_prince_right_hand_side` | the right-hand side of the DOPRI code |
| `rb23` | `rosenbrock_driver` | XPPAUT 8.0 odesol2.c:470: Rosenbrock step, flag-aware (rosen without flags) |
| `rosen` | `rosenbrock_steps` | XPPAUT 8.0 odesol2.c:485: Rosenbrock step, banded routines as well |
| `mod_euler` | `modified_euler_steps` | modified Euler (odesol2.cpp) |
| `rung_kut` | `runge_kutta_fixed` | fixed-step Runge-Kutta (odesol2.cpp) |
| `abmpc` | `adams_bashforth_moulton` | Adams-Bashforth-Moulton predictor-corrector (odesol2.cpp) |
| `symplect3` | `symplectic_third_order` | third-order symplectic step (odesol2.cpp) |
| `rooter` | `newton_iterate` | XPPAUT 8.0 gear.c:1058: Newton iteration for a root, with a pivoted LU |
| `getjac` | `numerical_jacobian` | XPPAUT 8.0 gear.c:996: Jacobian by differencing |
| `mswtch` | `copy_state` | XPPAUT 8.0 integrate.c:81: MSWTCH(u,v) is a memcpy of the node values |

## 8. Types and fields (the old section 3, settled)

- **`iap_type` and `rap_type` fields keep their names.** They are AUTO-07p's documented constants and integer/real parameter arrays (`ndim`, `ips`, `irs`, `ilp`, `ntst`, `ncol`, `isw`, `itp`, `rl0`, `rl1`, `a0`, `a1`, `epsl`, `epsu`, `epss`, `ds`, `dsmin`, `dsmax`, ...), named in AUTO-07p's manual, in fort.7/fort.8 and in the diagram's columns (rule 3). The `?` fields of the first inventory (`iad`, `iads`, `nbc0`, `nnt0`, `nwtn`, `nit`, `nins`, `ipos`) are settled by AUTO-07p's own documentation in the same way: they are AUTO's names, so they stay. The brief of this stage fixed this.
- **The common blocks** (`blhom`, `blhmp`, `blhmu`, `bcnn`, `blhma`, `bleig`, `blhme`, `beyn`) are Fortran common-block names, not documented constants: renamed in section 6; their fields are already readable.
- **The f2c types** `integer`, `real`, `doublereal`, `doublecomplex` become `std::ptrdiff_t`, `float`, `double` directly in the last pack of this stage; `f2c::abs`, `f2c::min` and `f2c::max` are not `std::`'s (they keep -0.0 and a NaN's sign where `std::fabs` clears it, and pick the other operand of an unordered pair: autox_f2c.h), so the three functions stay, with a name that says it (`fortran::`).
- **The function-pointer macros** (`FUNI_TYPE`, `BCNI_TYPE`, ..., autox_c.h) are not renamed here: they are declarations, not names of numerical routines (see section 10).

## 9. Duplicates and look-alikes (the old section 8, decided from the code)

| Pair | Decision | Evidence |
|---|---|---|
| `sign` (xpp_math.cpp) and `d_sign` (f2c_helpers.cpp) | merged into one, `transfer_sign` in xpp_math | the same function; f2c's ternary and `fabs` differ only in the sign bit of a NaN first argument |
| `saxpy`, `sscal` (xpp_math.cpp) and `daxpy`, `dscal` (eispack) | merged into `add_multiple`, `scale_vector` | the same loop for every stride used (positive); the unrolled copies add and multiply in the same order, and no FMA contracts |
| `idamax` (eispack) and `largest_magnitude` (xpp_math) | merged: `largest_magnitude` (one more than it for Fortran's 1-based index at the callers) | the same first-maximum rule for every n; `n < 1` gives -1 there and 0 in LINPACK, which the callers handle |
| xpp_math.cpp's `hqr` and `orthes` and eispack's | **not** merged; named `gear_hessenberg_qr_values` and `gear_hessenberg_reduce` against `hessenberg_qr_values` and `hessenberg_reduce_householder` | xpp_math's hqr has a fixed 1e-10 convergence, 30 iterations per root and no vectors; EISPACK's converges by floating-point addition with a shared 30n budget: a merge changes roots and failure status (the comment at xpp_math.cpp hqr says so) |
| `c_abs` (del_stab.cpp) and `f__cabs` (f2c) | **not** merged; `complex_modulus` and `complex_magnitude` | `sqrt(re^2 + im^2)` against the overflow-safe scaled form: different last bits |
| `cdivv` (del_stab.cpp) and `cdiv` (eispack) | **not** merged; `complex_divide` and `complex_divide_scaled` | the quotient by `|w|^2` against Smith's scaling by `|br| + |bi|`: different last bits |
| `gbfa`, `gbsl` (band.cpp, CVODE's copy) and `bandfac`, `bandsol` | **not** merged; `gbfa` and `gbsl` stay (vendored CVODE API until W34) | gbfa pivots, bandfac does not: different numbers |
| `ge` (AUTO's complete pivoting with determinant and log) and `sgefa` (partial pivoting) | **not** merged | the pivot order differs (the comment at xpp_math_autox.cpp:183 and xpp_math.cpp sgefa) |
| `dgemm` | one definition (eispack); xpp_math_autox.cpp's mention is a call | the first inventory's second copy was a call |
| `fftr` (autox_lib3.cpp:1802 and simplenet.cpp) | not an FFT: AUTO-07p's FFTR (periodic.f90:1589) is the torus continuation's function, now `torus_function`; `faft` is a transposed copy (`transpose_right_hand_side`) | autox_lib2.cpp:199 |
| `dle`, `dlt`, `dge`, `dne`, `dnot` | renamed (section 2); the expression language's own operator spellings are in the parser's table, not these C++ names | expr_functions.cpp |

## 10. Left for a later run of this card

The sections the run that settled this file did not rename are listed at its end by the final report: integrators and equilibria (section 7 above, its names settled here), the function-pointer macros of autox_c.h, and anything the report names as not finished.
