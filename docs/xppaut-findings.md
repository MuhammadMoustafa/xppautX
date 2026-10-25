# XPPAUT findings

Bugs, wrong results and arbitrary limits found in XPPAUT 8.x while
modernizing it as xppautX, for the paper (maintainer, 2026-10-01). Each
entry: what XPPAUT does, the evidence, what xppautX does instead, and the
card and commit. Add an entry whenever a card finds one; keep the evidence
reproducible (a model, a command, the numbers).

Each finding cites the lines in XPPAUT's own source, never our refactored
files, so it is clear the behaviour is XPPAUT's and not ours. A citation is
a relative link into a local copy kept out of git (`reference/` is in
.gitignore): `reference/xppaut-8.0/` is the XPPAUT 8.0 source xppautX was
forked from (the tarball of Nov 2023, imported untouched at the start of
this repository), and `reference/xppaut-master/` is XPPAUT's GitHub master,
an older snapshot (2016). A citation reads `[file:lines](link into 8.0)`;
where the same code is in the older master a second link follows it
(`master`), else the citation says `master: none` (that code is different
or absent there). Code that is in 8.0 only because our own refactoring put it
there is not a finding and is not listed. Line numbers were checked against
the files when each entry was written ([W143](https://github.com/MuhammadMoustafa/xppautX/issues/195)).

What else differs from XPPAUT, beyond bugs, is
[xppautx-vs-xppaut.md](xppautx-vs-xppaut.md). Bugs found in AUTO-07p itself are
recorded in [auto07p-findings.md](auto07p-findings.md).

The entries are sections, not table rows: most need a paragraph of
evidence. The index gives one line each; its number links to the entry's
section, and every card (to its GitHub issue) and commit (to its GitHub
commit) is a link, in the index and the entries alike (maintainer,
2026-10-01).

| # | Area | Finding | Card |
|---|---|---|---|
| [1](#1-model-options) | Model options | `@ newt_iter`, `newt_tol`, `jac_eps`, `poistop` never take effect | [W119](https://github.com/MuhammadMoustafa/xppautX/issues/170) |
| [2](#2-model-options) | Model options | five option names unreachable, `parmin` and `yhi` wrong | [W119](https://github.com/MuhammadMoustafa/xppautX/issues/170) |
| [3](#3-names) | Names | names cut or overflowed at 10-11 characters, dialogs at 25 | [W76](https://github.com/MuhammadMoustafa/xppautX/issues/124) |
| [4](#4-model-options) | Model options | the options file never sets anything | [W139](https://github.com/MuhammadMoustafa/xppautX/issues/191) |
| [5](#5-model-files) | Model files | a missing `#include` file is skipped, the name is taken literally | [W139](https://github.com/MuhammadMoustafa/xppautX/issues/191) |
| [6](#6-daes) | DAEs | a DAE run steps over a fold onto another branch | [W127](https://github.com/MuhammadMoustafa/xppautX/issues/179) |
| [7](#7-auto-files-the-last-line-written-twice) | AUTO files | the last line of the `.s` dump written twice (`while(!feof)`) | [dc69f40](https://github.com/MuhammadMoustafa/xppautX/commit/dc69f40) |
| [8](#8-auto-files-everything-in-home) | AUTO files | AUTO's files all go to `$HOME`: sessions overwrite each other, a bad HOME ends the program | [dc69f40](https://github.com/MuhammadMoustafa/xppautX/commit/dc69f40) |
| [9](#9-auto-files-restart-files-and-open-handles) | AUTO files | restart files lost to Windows file semantics, a file handle leaked | [3f0eea8](https://github.com/MuhammadMoustafa/xppautX/commit/3f0eea8) |
| [10](#10-auto-restart-readers) | AUTO restart | restart readers trust unchecked reads; `fseek(-2)` on a text stream corrupts the heap | [137fd08](https://github.com/MuhammadMoustafa/xppautX/commit/137fd08) |
| [11](#11-auto-starts-from-the-last-integration-with-nothing-integrated) | AUTO | Start Periodic, Bdry Value and homoclinic starts read before the data | [W97](https://github.com/MuhammadMoustafa/xppautX/issues/146) |
| [12](#12-auto-homoclinic-test-functions-11-and-12-read-freed-memory) | AUTO | homoclinic test functions 11 and 12 read freed memory | [031eb17](https://github.com/MuhammadMoustafa/xppautX/commit/031eb17) |
| [13](#13-auto-the-plot-filter-for-two-parameter-diagrams) | AUTO | the two-parameter plot filter compares `icp2` with itself | [69cf3f0](https://github.com/MuhammadMoustafa/xppautX/commit/69cf3f0) |
| [14](#14-auto-stale-eigenvalues-at-a-runs-first-point) | AUTO | a run's first point shows the previous run's eigenvalues | [W15](https://github.com/MuhammadMoustafa/xppautX/issues/27) |
| [15](#15-auto-nan-left-in-the-models-parameters) | AUTO | a diverged run leaves NaN in the model's parameters | [W35a](https://github.com/MuhammadMoustafa/xppautX/issues/73) |
| [16](#16-auto-fit-of-a-flat-quantity) | AUTO | Fit of a flat quantity divides by an empty range | [W50](https://github.com/MuhammadMoustafa/xppautX/issues/98) |
| [17](#17-auto-and-eispack-call-exit) | AUTO, eispack | the numerics call `exit()` and end the program | [W63a](https://github.com/MuhammadMoustafa/xppautX/issues/111) |
| [18](#18-auto-files-saving-an-empty-diagram) | AUTO files | saving an empty diagram truncates the export file and leaks the handle | [8679803](https://github.com/MuhammadMoustafa/xppautX/commit/8679803) |
| [19](#19-fixed-buffers-and-tables-in-the-model-reader) | Model reader | fixed buffers cut or overflow lines, comments, file names, networks | [W29e](https://github.com/MuhammadMoustafa/xppautX/issues/53) |
| [20](#20-writes-past-the-end-and-unterminated-strings) | Model reader | writes past the end and unterminated strings | [W21](https://github.com/MuhammadMoustafa/xppautX/issues/34) |
| [21](#21-set-par-and-ic-files-read-by-guessing) | Files | `.set`, `.par`, `.ic` read with `atoi`/`atof`, applied as far as they read | [W125](https://github.com/MuhammadMoustafa/xppautX/issues/177) |
| [22](#22-adjoint-h-function-and-histogram-tables-on-moved-storage) | Analysis | adjoint, H function and histogram tables point into storage that moves | [W117](https://github.com/MuhammadMoustafa/xppautX/issues/168) |
| [23](#23-fftcon-reads-one-value-past-its-weight-table) | Networks | `fftcon` reads one value past its weight table | [W38](https://github.com/MuhammadMoustafa/xppautX/issues/81) |
| [24](#24-an-export-with-no-library-copies-uninitialised-memory) | Export | an export with no library copies uninitialised memory into the model | [d5eebae](https://github.com/MuhammadMoustafa/xppautX/commit/d5eebae) |
| [25](#25-an-output-stride-of-0-divides-by-zero) | Numerics | an output stride of 0 from a .set file or an `@ nout` line divides by zero on the next run | [W145](https://github.com/MuhammadMoustafa/xppautX/issues/197) |
| [26](#26-set-files-ignore-the-models-names) | Set files | equal counts allow another model's values: names ignored | [W153](https://github.com/MuhammadMoustafa/xppautX/issues/205) |
| [27](#27-rotation-and-boundary-value-movies-silently-drop-frames-when-full) | Kinescope | rotation and BVP movies ignore a full film buffer | [W133](https://github.com/MuhammadMoustafa/xppautX/issues/185) |
| [28](#28-a--comment-after-a-declaration-makes-names-of-its-words) | Model files | a `#` comment after a declaration makes parameters of its words (XPPAUT's own issue [Ermentrout/xppaut#11](https://github.com/Ermentrout/xppaut/issues/11)) | [W160](https://github.com/MuhammadMoustafa/xppautX/issues/212) |
| [29](#29-the-numbers-depend-on-the-cpu-and-on-the-compilers-fma) | Numerics | the numbers depend on the CPU and on whether the compiler targets FMA | [W159](https://github.com/MuhammadMoustafa/xppautX/issues/211) |
| [30](#30-auto-orbit-loading-trusts-file-dimensions-and-storage-capacity) | AUTO files | orbit dimensions and row counts can overrun fixed buffers and storage | [W155](https://github.com/MuhammadMoustafa/xppautX/issues/207) |
| [31](#31-model-options-bypass-method-suitability-and-ignore-unknown-methods) | Method selection | model options bypass suitability checks and ignore unknown methods | [W132](https://github.com/MuhammadMoustafa/xppautX/issues/184) |
| [32](#32-kinescope-cancel-still-saves-frames) | Kinescope | Cancel on the base filename still writes frames | [W130](https://github.com/MuhammadMoustafa/xppautX/issues/182) |
| [33](#33-model-table-counts-have-no-upper-bound) | Model tables | tiny models request billions of values and evaluations | [W175](https://github.com/MuhammadMoustafa/xppautX/issues/227) |
| [34](#34-model-includes-read-outside-the-model-folder) | Model files | include names can read arbitrary reachable files | [W175](https://github.com/MuhammadMoustafa/xppautX/issues/227) |
| [35](#35-array-print-invalid-render-text-selects-blue-red) | Array plot | malformed render text silently selects blue-red | [W177](https://github.com/MuhammadMoustafa/xppautX/issues/229) |
| [36](#36-cvode-reverses-the-documented-tolerance-roles) | Numerics | CVODE receives TOLER as absolute and ATOLER as relative tolerance | [W34](https://github.com/MuhammadMoustafa/xppautX/issues/72), [W183](https://github.com/MuhammadMoustafa/xppautX/issues/235) |
| [37](#37-delete-column-is-an-unconditional-placeholder) | Data browser | Delete column always reports an error and returns | W197 (local card) |
| [38](#38-a-named-set-item-is-cut-at-its-numeric-prefix-and-the-rest-dropped-silently) | Named sets | a set item's value is read with `atof`: a dot for a comma silently drops the next item | [W229](https://github.com/MuhammadMoustafa/xppautX/issues/283) |
| [39](#39-autos-collocation-computes-a-jacobian-at-every-point-and-throws-it-away) | AUTO | the residual pass of every collocation step differences a whole Jacobian per point to use only f: about half of the periodic continuation's model evaluations wasted | [W251](https://github.com/MuhammadMoustafa/xppautX/issues/307) |
| [40](#40-shift-del_shft-and-set-reach-outside-the-variables) | Expressions | `shift` and `del_shft` read, and `set` writes, outside the variables with an index computed at run time | [W265](https://github.com/MuhammadMoustafa/xppautX/issues/322) |
| [41](#41-rosenbrock-scales-past-or-short-of-its-banded-jacobian) | Numerics | Rosenbrock scales the dense matrix size even when its Jacobian is banded, and wide bands exceed the work array | [W261](https://github.com/MuhammadMoustafa/xppautX/issues/318) |
| [42](#42-bessel-i-casts-its-order-without-checking-it) | Numerics | `bessel_i` converts unchecked model input to `int` | [W271](https://github.com/MuhammadMoustafa/xppautX/issues/328) |
| [43](#43-an-xy-table-extrapolates-past-its-last-point-the-wrong-way-in-code-nothing-reaches) | Tables | a table of x and y values extrapolates past its last x with the last segment's slope reversed, in a lookup no reader reaches | [W260](https://github.com/MuhammadMoustafa/xppautX/issues/317), [W373](https://github.com/MuhammadMoustafa/xppautX/issues/435) |
| [44](#44-every-name-is-found-by-scanning-every-symbol) | Speed | every name in a formula is found by scanning the whole symbol table, so loading grows with the square of the model's size | [W245](https://github.com/MuhammadMoustafa/xppautX/issues/301) |
| [45](#45-a-model-past-maxode-overflows-the-stack-instead-of-being-refused) | Limits | a model with 4999 or more equations or fixed quantities writes past `compile_em`'s name arrays: an abort ("buffer overflow detected") or a corrupted stack, not an error | [W269](https://github.com/MuhammadMoustafa/xppautX/issues/326) |
| [46](#46-parameter-files-ignore-names-and-load-by-position) | Parameter files | reordering `.par` lines silently assigns values to the wrong parameters | [W310](https://github.com/MuhammadMoustafa/xppautX/issues/369) |
| [47](#47-auto-a-special-point-whose-location-does-not-converge-leaves-the-iterate-behind) | AUTO | when the location of a special point does not converge in ITMX steps, AUTO2000 keeps the displaced iterate and goes on from it: the branch turns round (AUTO-07p restores the last converged point) | [W300](https://github.com/MuhammadMoustafa/xppautX/issues/358) |
| [48](#48-auto-storage-has-fixed-variable-parameter-and-branch-point-limits) | AUTO | 400 variables, 36 numerical parameters, 20 stored branch points and eight selectable model parameters | [W298](https://github.com/MuhammadMoustafa/xppautX/issues/356) |
| [49](#49-auto-homoclinic-form-overflows-its-value-table-from-50-variables) | AUTO | a 100-field form holds two dimensions and two values per state, overrunning from 50 variables | [W301](https://github.com/MuhammadMoustafa/xppautX/issues/359) |
| [50](#50-auto-accepts-nan-as-a-converged-bvp-solution) | AUTO | deprecated projection conditions make tstheti singular; NaN increments pass the BVP convergence test | [W343 / #404](https://github.com/MuhammadMoustafa/xppautX/issues/404) |
| [51](#51-auto-failed-step-endpoint-stability) | AUTO | a failed equilibrium branch switch restores an earlier branch point but stores the last branch's spectrum | [W341](https://github.com/MuhammadMoustafa/xppautX/issues/402) |
| [52](#52-auto-omits-unselected-branch-parameters) | AUTO | Unselected constants are absent from a branch; older points use later values | [W339](https://github.com/MuhammadMoustafa/xppautX/issues/400) |
| [53](#53-auto-import-is-unchecked-and-partial) | AUTO files | sent `.auto` counts and short records are applied unchecked | [W327](https://github.com/MuhammadMoustafa/xppautX/issues/388) |
| [54](#54-an-extra-plot-window-keeps-the-first-windows-time-flag) | Graphs | an extra window of a multi-window model keeps the first window's time flag | [W351](https://github.com/MuhammadMoustafa/xppautX/issues/412) |
| [55](#55-auto-marks-have-a-nine-value-form-limit) | AUTO | User marks stop at nine; count changes before the value form succeeds | [W331](https://github.com/MuhammadMoustafa/xppautX/issues/392) |
| [56](#56-auto-equilibrium-period-depends-on-spectrum-order) | AUTO | the stored period comes from whichever tied complex pair the eigenvalue routine returns last | [W369](https://github.com/MuhammadMoustafa/xppautX/issues/431) |
| [57](#57-fixed-c-arrays-sized-by-input) | Expressions, Numerics | formulas, table expressions, Markov states/rates, solver pivots and fit parameters overflow fixed C arrays | [W372 / #434](https://github.com/MuhammadMoustafa/xppautX/issues/434) |
| [58](#58-negative-total-help-and-storage-full-overwrite) | Integration | Negative Total still stores data and silently overwrites on failed growth | [W347](https://github.com/MuhammadMoustafa/xppautX/issues/408) |
| [59](#59-dormand-prince-restarted-at-every-output-point) | Integration | DoPri5 and DoPri8(3) restart at every output point: no step longer than dt, so the adaptive methods run as fixed-step ones and the tolerance does not set the accuracy | [W377 / #439](https://github.com/MuhammadMoustafa/xppautX/issues/439) |
| [60](#60-input-sized-writes-and-size-arithmetic) | Sweeps, expressions, delay, AUTO | unchecked row writes, fixed evaluation stacks and overflowing history/work sizes | [W386 / #448](https://github.com/MuhammadMoustafa/xppautX/issues/448) |
| [61](#61-wiener-noise-is-silently-zero-or-has-wrong-variance-in-adaptive-methods) | Numerics | set_wieners is never called by DoPri5, CVODE, Gear or Rosenbrock, and called once per output interval with step guess by Stiff and QualRK | [W379 / #441](https://github.com/MuhammadMoustafa/xppautX/issues/441) |
| [62](#62-some-long-computations-cannot-be-stopped) | Cancellation | Nullclines, the flow, fitting, a pairwise histogram and delay history run to the end once started | [W388 / #450](https://github.com/MuhammadMoustafa/xppautX/issues/450) |
| [63](#63-recursive-user-functions-segfault-at-runtime) | Expressions | Indefinite user function recursion overflows C stack and segfaults | [W387 / #449](https://github.com/MuhammadMoustafa/xppautX/issues/449) |
| [64](#64-auto-point-numbers-wrap-to-zero) | AUTO output | point 10000 is printed as 0; later points repeat earlier numbers | [W384 / #446](https://github.com/MuhammadMoustafa/xppautX/issues/446) |
| [65](#65-plot-curves-frozen-curves-and-windows-have-fixed-limits) | Plots | 10 curves per graph, 26 frozen curves, 21 plot windows | [W376 / #438](https://github.com/MuhammadMoustafa/xppautX/issues/438) |
| [66](#66-auto-map-fold-continuation-loses-the-map-problem) | AUTO | a map fold restart changes IPS=-1 to flow IPS=1 | [W404 / #466](https://github.com/MuhammadMoustafa/xppautX/issues/466) |
| [67](#67-the-torus-integral-condition-is-identically-zero) | AUTO | the torus continuation's phase condition compares the eigenfunction with itself, so it is zero for every input | [W407 / #469](https://github.com/MuhammadMoustafa/xppautX/issues/469) |
| [68](#68-homcont-differentiates-a-changing-projection-basis) | AUTO HomCont | parameter finite differences update the projection history they differentiate | [W365 / #427](https://github.com/MuhammadMoustafa/xppautX/issues/427) |
| [69](#69-a-homoclinic-start-samples-its-orbit-with-an-unscaled-weight) | AUTO HomCont | the starting orbit is interpolated with a weight not divided by the step, its end wraps to its start, and the shift is found before the equilibrium is given | [W365 / #427](https://github.com/MuhammadMoustafa/xppautX/issues/427) |
| [70](#70-auto-evaluates-the-test-functions-at-the-last-newton-iterates-jacobian) | AUTO | the test functions of a converged point use the Jacobian of the last Newton iterate, so a flat Hopf function is located slowly and a Hopf point can go unlabelled | [W415 / #477](https://github.com/MuhammadMoustafa/xppautX/issues/477) |
| [71](#71-auto-two-parameter-map-problems-and-disabled-detection) | AUTO | map flips are labelled HB; two-parameter detection is disabled and exports have six significant digits | [W428](https://github.com/MuhammadMoustafa/xppautX/issues/490) |
| [72](#72-autos-branch-point-test-reads-the-fold-tests-determinant) | AUTO | the branch point test of an equilibrium branch reads the determinant the fold test computed with the tangent row, which changes sign when a step turns past a fold: a spurious BP, and the fold missed | [W423 / #485](https://github.com/MuhammadMoustafa/xppautX/issues/485) |
| [73](#73-periodic-phase-conditions-do-not-pass-through-the-computed-orbit) | AUTO | the uncentered periodic phase condition has a discrete residual at the previous computed orbit, so zero-length correction can move it | [W437 / #499](https://github.com/MuhammadMoustafa/xppautX/issues/499) |
| [74](#74-autos-finite-difference-jacobian-steps-every-state-by-the-largest-ones-scale) | AUTO | the finite-difference Jacobian of an equilibrium or cycle problem (`JAC=0`, always) steps every state by 1e-7 times the largest state: Hopf periods and fold norms are off on a model whose states differ in scale | [W455 / #518](https://github.com/MuhammadMoustafa/xppautX/issues/518) |
| [75](#75-the-auto-file-name-overflows-past-199-characters-of-home-and-model-name) | AUTO | starting any model writes `$HOME/<model file name>` into a 200-byte array unchecked: a long HOME or model name aborts (fortified build) or overwrites what follows | W354 series 22m (no card) |
| [76](#76-the-equilibrium-ranges-monte-carlo-answer-is-read-from-the-movie-field) | Equilibria | Equilibria > Range's Monte Carlo field reads a `y` typed in the Movie field's position (and its own `n` is ignored when Movie is `y`) | [W482 / #546](https://github.com/MuhammadMoustafa/xppautX/issues/546) |
| [77](#77-rosenbrock-retries-a-failing-step-forever-at-the-smallest-step) | Integrators | a Rosenbrock step that fails at the smallest step size never reaches its "Step size too small" exit, since the size is clamped up to that minimum first: the run does not end | [W482 / #546](https://github.com/MuhammadMoustafa/xppautX/issues/546) |
| [78](#78-a-flag-with-sign-1-repeats-one-clause-and-misses-an-exact-zero) | Events | the rising-flag test repeats one clause where the falling one tests `f1<=0`: a rising flag that lands exactly on zero is missed | [W482 / #546](https://github.com/MuhammadMoustafa/xppautX/issues/546) |
| [79](#79-the-stiff-step-that-finds-its-matrix-singular-is-retried-with-a-stale-step) | Integrators | Stiff's step that finds its matrix singular reports `*ier=-1`, which the driver does not test (it stops only for a positive one): the step is retried with a step size left over, or never set | [W488 / #552](https://github.com/MuhammadMoustafa/xppautX/issues/552) |
| [80](#80-a-networks-tables-are-never-checked-for-the-values-it-reads-and-fftcons-check-tests-the-wrong-variable) | Networks | a network reads its weight, index and delay tables with no check of their length; fftcon's check compares the keyword code, so the zero-ends test never runs | [W490 / #554](https://github.com/MuhammadMoustafa/xppautX/issues/554) |
| [81](#81-a-failed-auto-set-up-a-missing-restart-label-and-a-short-restart-read-pass-silently) | AUTO | a combination AUTO has no functions for leaves the run's function table uninitialised and only prints "Initialization Error CRASH!!"; a missing restart label only prints; a short read of the restart data is ignored and the run goes on with half-zero data | [W491 / #555](https://github.com/MuhammadMoustafa/xppautX/issues/555) |
| [82](#82-a-range-integration-ignores-every-failure-of-its-solver) | Ranges | a range or manipulation run that fails in its solver ends or continues silently |
| [83](#83-the-transient-time-compares-the-absolute-time) | Integration | `TRANS` compares \|t\|, wrong for a run not starting at 0 or going backward |
| [84](#84-the-array-plots-time-range-starts-at-0-and-ends-at-20-by-default) | Array plot | Print arrayplot and the window's time range start at 0 and end at 20 whatever the data holds (only a first row above 0 moves the start): a run of any other length prints with a wrong axis | [W492 / #556](https://github.com/MuhammadMoustafa/xppautX/issues/556) |
| [86](#86-the-cross-spectrum-averages-one-window-more-than-it-divides-by) | Spectra | the cross spectrum and coherence loop over `kwin+1` windows (the last wraps round the data) but divide by `kwin`, where the spectrum loops over `kwin`: the cross spectrum is too large by (kwin+1)/kwin (the coherence, a ratio of sums over the same windows, is unaffected) | [W489 / #553](https://github.com/MuhammadMoustafa/xppautX/issues/553) |

## 1. Model options

`@ newt_iter`, `@ newt_tol`, `@ jac_eps` and `@ poistop` in a model never
take effect: each option clears one "already set" flag (`NEWT_ITER`,
`NEWT_TOL`, `JAC_EPS`, `POISTOP`) while the defaults that follow test
another flag for the same value (`EVEC_ITER`, `EVEC_ERR`, `NEWT_ERR`, ...),
so the default overwrites what the model asked for.

- **XPPAUT 8.0:** [main.c:446](../reference/xppaut-8.0/main.c#L446) ([master 432](../reference/xppaut-master/main.c#L432)) calls load_eqn(), which reads the model's `@` options through set_option, then [main.c:455](../reference/xppaut-8.0/main.c#L455) ([master 441](../reference/xppaut-master/main.c#L441)) calls set_all_vals(), which applies the defaults. load_eqn.c set_option: [load_eqn.c:1544-1550](../reference/xppaut-8.0/load_eqn.c#L1544-L1550) ([master 1543-1549](../reference/xppaut-master/load_eqn.c#L1543-L1549)) JAC_EPS sets NEWT_ERR and clears notAlreadySet.JAC_EPS, [load_eqn.c:1552-1559](../reference/xppaut-8.0/load_eqn.c#L1552-L1559) ([master 1551-1558](../reference/xppaut-master/load_eqn.c#L1551-L1558)) NEWT_TOL sets EVEC_ERR and clears NEWT_TOL, [load_eqn.c:1561-1567](../reference/xppaut-8.0/load_eqn.c#L1561-L1567) ([master 1560-1566](../reference/xppaut-master/load_eqn.c#L1560-L1566)) NEWT_ITER sets EVEC_ITER and clears NEWT_ITER, [load_eqn.c:1831-1837](../reference/xppaut-8.0/load_eqn.c#L1831-L1837) ([master 1830-1836](../reference/xppaut-master/load_eqn.c#L1830-L1836)) POISTOP sets SOS and clears POISTOP. set_all_vals: [load_eqn.c:557](../reference/xppaut-8.0/load_eqn.c#L557) ([master 557](../reference/xppaut-master/load_eqn.c#L557)) tests notAlreadySet.SOS, [load_eqn.c:568](../reference/xppaut-8.0/load_eqn.c#L568) ([master 568](../reference/xppaut-master/load_eqn.c#L568)) EVEC_ITER, [load_eqn.c:569](../reference/xppaut-8.0/load_eqn.c#L569) ([master 569](../reference/xppaut-master/load_eqn.c#L569)) EVEC_ERR, [load_eqn.c:571](../reference/xppaut-8.0/load_eqn.c#L571) ([master 571](../reference/xppaut-master/load_eqn.c#L571)) NEWT_ERR and writes the defaults over them; [load_eqn.c:608](../reference/xppaut-8.0/load_eqn.c#L608) (master: none) has the JAC_EPS default commented out.
- **Evidence:** dae.ode, dae_ex3.ode, canonical/exdaebvp.ode give different results once the options apply; dae_ex3 fails at t=0.45 with the defaults and runs to t=3.65 with its own settings. Independent check ([W126](https://github.com/MuhammadMoustafa/xppautX/issues/178), docs/w126-dae-check.md, scipy Radau and RK4+Newton references): with the options applied dae.ode and exdaebvp.ode are 400-4000 times more accurate (errors 1e-7..1e-5 against 1e-3..3e-3, the constraint residual 1e-5 against 1e-3); dae_ex3's run to 3.65 is spurious, since the DAE has a fold at t=0.4507 and the old stop at 0.45 was right (finding 6).
- **xppautX:** one table drives every option, one flag per option.
- **Card:** [W119](https://github.com/MuhammadMoustafa/xppautX/issues/170) ([7013c6f](https://github.com/MuhammadMoustafa/xppautX/commit/7013c6f)).

## 2. Model options

`histlo2`, `histhi2`, `histbins2`, `histcol2`, `speccol2` are parsed by
nothing: unreachable. `parmin` shares XMAX's flag. `POIEXT` is never reset or
parsed. The `yhi` default writes the 3D box's `y_3d[0]` instead of
`y_3d[1]`.

- **XPPAUT 8.0:** load_eqn.c: msc() ([load_eqn.c:871-880](../reference/xppaut-8.0/load_eqn.c#L871-L880) ([master 870-879](../reference/xppaut-master/load_eqn.c#L870-L879))) matches a prefix, and set_option tests HISTLO ([load_eqn.c:2291](../reference/xppaut-8.0/load_eqn.c#L2291) ([master 2290](../reference/xppaut-master/load_eqn.c#L2290))), HISTHI ([load_eqn.c:2300](../reference/xppaut-8.0/load_eqn.c#L2300) ([master 2299](../reference/xppaut-master/load_eqn.c#L2299))), HISTBINS ([load_eqn.c:2309](../reference/xppaut-8.0/load_eqn.c#L2309) ([master 2308](../reference/xppaut-master/load_eqn.c#L2308))), HISTCOL ([load_eqn.c:2318](../reference/xppaut-8.0/load_eqn.c#L2318) ([master 2317](../reference/xppaut-master/load_eqn.c#L2317))), SPECCOL ([load_eqn.c:2366](../reference/xppaut-8.0/load_eqn.c#L2366) ([master 2327](../reference/xppaut-master/load_eqn.c#L2327))) before HISTLO2 ([load_eqn.c:2328](../reference/xppaut-8.0/load_eqn.c#L2328) (master: none)), HISTBINS2 ([load_eqn.c:2346](../reference/xppaut-8.0/load_eqn.c#L2346) (master: none)), SPECCOL2 ([load_eqn.c:2376](../reference/xppaut-8.0/load_eqn.c#L2376) ([master 2337](../reference/xppaut-master/load_eqn.c#L2337))) ..., so the shorter name always wins; PARMIN ([load_eqn.c:1998-2004](../reference/xppaut-8.0/load_eqn.c#L1998-L2004) ([master 1997-2003](../reference/xppaut-master/load_eqn.c#L1997-L2003))) tests and clears notAlreadySet.XMAX; set_all_vals [load_eqn.c:597](../reference/xppaut-8.0/load_eqn.c#L597) ([master 597](../reference/xppaut-master/load_eqn.c#L597)) writes the YHI default into y_3d[0] ([load_eqn.c:612-613](../reference/xppaut-8.0/load_eqn.c#L612-L613) ([master 611-612](../reference/xppaut-master/load_eqn.c#L611-L612)) show y_3d[0]/[1] are the 3D box's low/high).
- **Evidence:** [W119](https://github.com/MuhammadMoustafa/xppautX/issues/170)'s option table round-trip test.
- **xppautX:** each works, with its own flag.
- **Card:** [W119](https://github.com/MuhammadMoustafa/xppautX/issues/170) ([7013c6f](https://github.com/MuhammadMoustafa/xppautX/commit/7013c6f)).

## 3. Names

Names of variables, parameters and functions are cut or overflow at fixed
lengths, and so are the dialog fields that show them.

- **XPPAUT 8.0:** the name tables are `char uvar_names[MAXODE][12]` ([form_ode.c:45](../reference/xppaut-8.0/form_ode.c#L45) ([master 45](../reference/xppaut-master/form_ode.c#L45))) and `char upar_names[MAXPAR][11]` ([form_ode.c:47](../reference/xppaut-8.0/form_ode.c#L47) ([master 47](../reference/xppaut-master/form_ode.c#L47))), so a variable has at most 11 characters and a parameter 10; the parser copies a name into them with `strcpy` and no length check ([form_ode.c:711](../reference/xppaut-8.0/form_ode.c#L711) ([master 711](../reference/xppaut-master/form_ode.c#L711)) a parameter, [form_ode.c:742](../reference/xppaut-8.0/form_ode.c#L742) ([master 742](../reference/xppaut-master/form_ode.c#L742)) a Markov variable), so a longer name overflows into the next row's name; the parser's own copy of a name is `char name[20]` ([form_ode.c:1362](../reference/xppaut-8.0/form_ode.c#L1362) ([master 1362](../reference/xppaut-master/form_ode.c#L1362)), [form_ode.c:1107](../reference/xppaut-8.0/form_ode.c#L1107) ([master 1107](../reference/xppaut-master/form_ode.c#L1107)), [form_ode.c:602](../reference/xppaut-8.0/form_ode.c#L602) ([master 602](../reference/xppaut-master/form_ode.c#L602))); `MAX_LEN_SBOX` is 25 ([graf_par.h:14](../reference/xppaut-8.0/graf_par.h#L14) ([master 14](../reference/xppaut-master/graf_par.h#L14))), the width of every dialog field.
- **Evidence:** tools/models/longnames.ode (200-character names), autocheck's `names` section, tests/test_names.cpp.
- **xppautX:** no limit; names are `std::string`, fixed-width columns shorten with `~`.
- **Card:** [W76](https://github.com/MuhammadMoustafa/xppautX/issues/124) ([3e63883](https://github.com/MuhammadMoustafa/xppautX/commit/3e63883)).

## 4. Model options

The options file (`option name` in a model, else `default.opt`), documented
as setting the plot variables, axes, method, total, dt, bounds, tolerances
and window, never sets any of them: set_all_vals gives every one its
default (clearing its "not already set" flag) before it reads the file, and
read_defaults reads a value only while the flag is still set; a line it
skips is not read, so the lines after it would shift too.

- **XPPAUT 8.0:** load_eqn.c: set_all_vals ([load_eqn.c:543](../reference/xppaut-8.0/load_eqn.c#L543) ([master 543](../reference/xppaut-master/load_eqn.c#L543))) writes the defaults ([load_eqn.c:607](../reference/xppaut-8.0/load_eqn.c#L607) ([master 607](../reference/xppaut-master/load_eqn.c#L607)) DT, [load_eqn.c:617](../reference/xppaut-8.0/load_eqn.c#L617) ([master 616](../reference/xppaut-master/load_eqn.c#L616)) TEND, [load_eqn.c:619](../reference/xppaut-8.0/load_eqn.c#L619) ([master 618](../reference/xppaut-master/load_eqn.c#L618)) IXPLT, ...) and only then opens the file ([load_eqn.c:641-643](../reference/xppaut-8.0/load_eqn.c#L641-L643) ([master 640-642](../reference/xppaut-master/load_eqn.c#L640-L642))); read_defaults ([load_eqn.c:702](../reference/xppaut-8.0/load_eqn.c#L702) ([master 701](../reference/xppaut-master/load_eqn.c#L701))) tests the same flags ([load_eqn.c:725](../reference/xppaut-8.0/load_eqn.c#L725) ([master 724](../reference/xppaut-master/load_eqn.c#L724)) IXPLT ... [load_eqn.c:747](../reference/xppaut-8.0/load_eqn.c#L747) ([master 746](../reference/xppaut-master/load_eqn.c#L746)) YHI) and reads a line only when one is set; [load_eqn.c:728](../reference/xppaut-8.0/load_eqn.c#L728) ([master 727](../reference/xppaut-master/load_eqn.c#L727)) (AXES) and [load_eqn.c:741](../reference/xppaut-8.0/load_eqn.c#L741) ([master 740](../reference/xppaut-master/load_eqn.c#L740)) (DTMAX) clear the wrong flags (PaperWhite, DTMIN).
- **xppautX:** dropped: an `option` line is refused at load (file and line), `default.opt` is never looked for, and the settings go in `@` lines.
- **Card:** [W139](https://github.com/MuhammadMoustafa/xppautX/issues/191) ([970c1a2](https://github.com/MuhammadMoustafa/xppautX/commit/970c1a2)).

## 5. Model files

A model whose `#include` file cannot be opened still loads: XPPAUT prints
"Cant open include file" and goes on without it, so the functions and
settings the file held are missing and the model runs with defaults, exit
status 0 (the `-include` flag's file, by contrast, exits with -1). The file
name is everything after `#include `, so `#include <opts.inc>` and
`#include "opts.inc"` look for a file literally named with the brackets or
quotes, and so are skipped the same way. The name is opened as typed (fopen
of the bare name), so it is looked for in the working folder, not the
model's: a model opened from another folder does not find its include.

- **XPPAUT 8.0:** form_ode.c: if_include_file ([form_ode.c:1274-1296](../reference/xppaut-8.0/form_ode.c#L1274-L1296) ([master 1274-1296](../reference/xppaut-master/form_ode.c#L1274-L1296))) takes the text after the first blank as the name; do_new_parser [form_ode.c:1414](../reference/xppaut-8.0/form_ode.c#L1414) ([master 1414](../reference/xppaut-master/form_ode.c#L1414)) (`fopen(newfile,"r")`), [form_ode.c:1415-1418](../reference/xppaut-8.0/form_ode.c#L1415-L1418) ([master 1415-1418](../reference/xppaut-master/form_ode.c#L1415-L1418)) (`fnew==NULL`: plintf, `continue`); the flag's own [form_ode.c:1387-1391](../reference/xppaut-8.0/form_ode.c#L1387-L1391) ([master 1387-1391](../reference/xppaut-master/form_ode.c#L1387-L1391)) (plintf, `exit(-1)`).
- **xppautX:** an include is looked for in the folder of the file that holds the line; a file that cannot be read stops the load with that file and the `#include` line; so does the `-include` flag's (relative to the working folder).
- **Card:** [W139](https://github.com/MuhammadMoustafa/xppautX/issues/191) ([82c67a5](https://github.com/MuhammadMoustafa/xppautX/commit/82c67a5), [bc2cfaa](https://github.com/MuhammadMoustafa/xppautX/commit/bc2cfaa)).

## 6. DAEs

A DAE run steps over a fold onto another branch of solutions, or onto no
solution at all: the algebraic solve accepts whatever root Newton reaches
from the last solution, with nothing to say it is on the same branch, and it
accepts a point where the Newton update was small without checking that the
equations hold there. The manual's own dae_ex3 "exploits numerical errors to
get the DAE solver to go beyond where it should go legally".

- **XPPAUT 8.0:** dae_fun.c solve_dae: [dae_fun.c:230-243](../reference/xppaut-8.0/dae_fun.c#L230-L243) ([master 230-243](../reference/xppaut-master/dae_fun.c#L230-L243)) accept any y whose residual is below the tolerance, however far Newton went from the last solution; [dae_fun.c:256-261](../reference/xppaut-8.0/dae_fun.c#L256-L261) ([master 256-261](../reference/xppaut-master/dae_fun.c#L256-L261)) check only for an exactly singular Jacobian; [dae_fun.c:273-281](../reference/xppaut-8.0/dae_fun.c#L273-L281) ([master 273-281](../reference/xppaut-master/dae_fun.c#L273-L281)) accept y when the update is below the tolerance, without the residual (do_daes [dae_fun.c:202](../reference/xppaut-8.0/dae_fun.c#L202) ([master 202](../reference/xppaut-master/dae_fun.c#L202)), "accepts a no change error!").
- **Evidence:** dae_ex3.ode (w'=v, 0=v(1-v^2)-w, v(0)=1) has a fold at v=1/sqrt(3), t*=0.450694, past which its branch has no solution; with its own NEWT_ITER=1000 (set in the numerics menu in XPPAUT, finding 1) Newton wanders for 318 iterations at t=0.46 and lands on the branch v=-1.16, and the run goes on to t=3.65 with v jumping between branches and the constraint off by up to 0.031 (docs/w126-dae-check.md, scipy and RK4+Newton references).
- **xppautX:** a solution is accepted only where the residual is within the tolerance, and only while every Jacobian Newton factors from the last solution keeps that solution's determinant sign; a sign change is a fold, and the run stops there with "No solution of the algebraic equations past t=0.45: their Jacobian changed sign, a fold where this branch of solutions ends", its rows kept to t=0.45.
- **Card:** [W127](https://github.com/MuhammadMoustafa/xppautX/issues/179) ([b70f8f6](https://github.com/MuhammadMoustafa/xppautX/commit/b70f8f6)).

## 7. AUTO files: the last line written twice

Saving AUTO's session (File > Save auto) copies the `.s` solution file into
the `.auto` file, and loading one copies it back, with `while(!feof(f))`
around `fgets`/`fputs`. `feof` is still false going into the read that hits
the end of the file, so that read fails, leaves the buffer as the previous
line, and the loop writes that line again: every saved `.auto` has its last
`.s` line twice, and the same on loading.

- **XPPAUT 8.0:** auto_nox.c save_q_file ([auto_nox.c:2832-2837](../reference/xppaut-8.0/auto_nox.c#L2832-L2837) ([master 2803-2808](../reference/xppaut-master/auto_nox.c#L2803-L2808))) and make_q_file ([auto_nox.c:2853-2858](../reference/xppaut-8.0/auto_nox.c#L2853-L2858) ([master 2824-2829](../reference/xppaut-master/auto_nox.c#L2824-L2829))), neither checks fgets' result.
- **Evidence:** tools/models/lecar_diagram.auto, the saved diagram autocheck compares byte for byte, had the duplicated line in its reference; it is one line shorter now.
- **xppautX:** both stop on fgets' own NULL.
- **Card:** issue [#11](https://github.com/MuhammadMoustafa/xppautX/issues/11) ([dc69f40](https://github.com/MuhammadMoustafa/xppautX/commit/dc69f40)).

## 8. AUTO files: everything in `$HOME`

AUTO's working files (fort.3, fort.7, fort.8, fort.9, the swap file and the
model's `.s`, `.b`, `.d`) are all made in `$HOME`, whatever the model's
folder, with the same names for every model and every running instance. Two
models open at once, or two windows on the same model, overwrite each
other's files, so a grab can fail or return NaN. `$HOME` is only checked for
being unset: a `$HOME` that is missing or cannot be written makes fopen()
fail inside the library, which calls exit(1), ending the whole program and
losing the session. The names are built with `sprintf` into 200-byte arrays,
so a long `$HOME` plus a long model name overflows them.

- **XPPAUT 8.0:** auto_nox.c create_auto_file_name ([auto_nox.c:587-594](../reference/xppaut-8.0/auto_nox.c#L587-L594) ([master 581-587](../reference/xppaut-master/auto_nox.c#L581-L587))) and open_auto ([auto_nox.c:609-620](../reference/xppaut-8.0/auto_nox.c#L609-L620) ([master 601-612](../reference/xppaut-master/auto_nox.c#L601-L612))) take `getenv("HOME")` and fall back to the model's folder only when it is NULL; the arrays are `char this_auto_file[200]`, `fort3`, `fort7`, `fort8`, `fort9`, `TMPSWAP` ([auto_nox.c:211-217](../reference/xppaut-8.0/auto_nox.c#L211-L217) ([master 207-212](../reference/xppaut-master/auto_nox.c#L207-L212))); the failed open of fort.8 ([autlib1.c:3074-3081](../reference/xppaut-8.0/autlib1.c#L3074-L3081) ([master 3071-3078](../reference/xppaut-master/autlib1.c#L3071-L3078)) and [autlib1.c:7172-7179](../reference/xppaut-8.0/autlib1.c#L7172-L7179) ([master 7169-7176](../reference/xppaut-master/autlib1.c#L7169-L7176))) is `exit(1)`.
- **Evidence:** two xppautX processes on lecar.ode, each Run > Steady state: they share fort.8 and the `.s` of the first run is rewritten by the second (issue [#11](https://github.com/MuhammadMoustafa/xppautX/issues/11)). With `HOME` pointing at a missing folder XPPAUT's AUTO prints "Error:  Could not open fort.8" and the process ends ([5d71fb1](https://github.com/MuhammadMoustafa/xppautX/commit/5d71fb1), reproduced on Windows).
- **Measured, concurrent runs (W248, [#304](https://github.com/MuhammadMoustafa/xppautX/issues/304); XPPAUT 8.0 c021b511 built on WSL, gcc 15.2, one `$HOME`, one private X display each, 5 measured repetitions after a warm-up):** N copies of one AUTO job (steady state, then the periodic branch from the HB, 22 s alone) started at once: N=2, all 10 runs finished with the diagram printed by a run alone, but the files they leave in `$HOME` differ from a run alone's (`.s`, `.d`, fort.8, `__tmp__`); N=4, 2 of 20 runs died with SIGSEGV (3 of the 5 repetitions ran all four through). N different jobs (ParMin 200, 400, 800, 1600 pA) started at once: N=2, 7 of 10 runs died with SIGSEGV (some after AUTO printed `Restart label 9 not found`), 2 finished with every label number of the periodic branch one too high (the values are those of a run alone; a restart from such a label reads another solution), 1 was correct; N=4, 19 of 20 died, 1 finished with shifted labels. Every one of the same runs one after another, in the same `$HOME`, finished and gave the diagram of a run alone (all 60 measured runs of the sequential groups). Nothing is lost to the overwriting when the copies are identical and start together, and nearly everything is when they differ. Data: `paper/benchmarks/auto/concurrent/` (local, git-ignored). Windows: XPPAUT's native build could not be run (AUTO needs X11).
- **xppautX:** each session gets its own scratch folder (`xppautoX-<pid>-N`, removed at exit; leftovers of dead processes at start: [W19](https://github.com/MuhammadMoustafa/xppautX/issues/32)), results go next to the model, a failed open is an error shown by the command.
- **Card:** issue [#11](https://github.com/MuhammadMoustafa/xppautX/issues/11) ([dc69f40](https://github.com/MuhammadMoustafa/xppautX/commit/dc69f40)), [5d71fb1](https://github.com/MuhammadMoustafa/xppautX/commit/5d71fb1), [W19](https://github.com/MuhammadMoustafa/xppautX/issues/32).

## 9. AUTO files: restart files and open handles

The helpers that move AUTO's files around assume POSIX behaviour and never
check what they open:

- `renamef` is a bare `rename()`: on Windows it fails when the destination exists, so `<model>.s` was silently kept and fort.8 fell behind ([auto_nox.c:452-456](../reference/xppaut-8.0/auto_nox.c#L452-L456) ([master 446-450](../reference/xppaut-master/auto_nox.c#L446-L450))).
- `copyf` and `appendf` use the result of `fopen` without testing it: a failed open reads or writes through a NULL `FILE *`, which left fort.3 (the restart file) at 0 bytes and then "Restart label N not found" ([auto_nox.c:482-497](../reference/xppaut-8.0/auto_nox.c#L482-L497) ([master 476-490](../reference/xppaut-master/auto_nox.c#L476-L490)), [auto_nox.c:500-529](../reference/xppaut-8.0/auto_nox.c#L500-L529) ([master 494-522](../reference/xppaut-master/auto_nox.c#L494-L522))).
- fp8 (fort.8) is opened once per run ([autlib1.c:3074-3081](../reference/xppaut-8.0/autlib1.c#L3074-L3081) ([master 3071-3078](../reference/xppaut-master/autlib1.c#L3071-L3078))) and the only `fclose(fp8)` is inside a comment ([auto_nox.c:544-548](../reference/xppaut-8.0/auto_nox.c#L544-L548) ([master 538-542](../reference/xppaut-master/auto_nox.c#L538-L542))), so the handle leaks every run and, on Windows, which will not rename or remove an open file, fort.8 stayed beside `<model>.s`.
- the "Restart label not found" path returns without closing fp3, fp7, fp9 ([gogoauto.c:66-76](../reference/xppaut-8.0/gogoauto.c#L66-L76) ([master 66-76](../reference/xppaut-master/gogoauto.c#L66-L76)), closed at [gogoauto.c:111-113](../reference/xppaut-8.0/gogoauto.c#L111-L113) ([master 111-113](../reference/xppaut-master/gogoauto.c#L111-L113))), so one failure on Windows poisoned every later AUTO run.

- **Evidence:** on Windows, Run, Grab, Run again: before the fix fort.8 stayed beside lecar.ode.s and the second run failed with "Restart label not found"; after, only the three renamed files remain (commit [860436c](https://github.com/MuhammadMoustafa/xppautX/commit/860436c)'s check with build/grabtest.py against the MinGW build).
- **xppautX:** one file module (`xpp::files`) with checked copy/move/prepend, AUTO's files closed on every path (fort.8 is an `xpp::UniqueFile`, [W117](https://github.com/MuhammadMoustafa/xppautX/issues/168)).
- **Card:** [3f0eea8](https://github.com/MuhammadMoustafa/xppautX/commit/3f0eea8), [860436c](https://github.com/MuhammadMoustafa/xppautX/commit/860436c), [W117](https://github.com/MuhammadMoustafa/xppautX/issues/168) ([5ae5282](https://github.com/MuhammadMoustafa/xppautX/commit/5ae5282)).

## 10. AUTO restart readers

AUTO reads a restart label from fort.3 with `fscanf` calls whose results are
never tested, and uses the values it read as sizes: `readlb` reads `nar`
and loops `u[i]` for `i < nar-1`, the `stpnp*` readers read `ntsr`, `ncolrs`
and `nparr` the same way. A truncated or damaged restart file hands stack
garbage to a loop bound or a malloc size, and a short read is not reported.

Separately, `findlb` walks back to the start of the label line with
`fseek(fp3,-2,SEEK_CUR)` on a text stream. On Windows the C runtime
translates CR LF, a relative seek on a text stream is undefined there, the
stream is left mid-line, and the following `readlb` parsed the header from the
wrong place (once `nar = -444604564`) and wrote that many values past the
two-element `u`: heap corruption (0xC0000374) on Run after a Grab.

- **XPPAUT 8.0:** autlib1.c readlb ([autlib1.c:4562-4574](../reference/xppaut-8.0/autlib1.c#L4562-L4574) ([master 4559-4571](../reference/xppaut-master/autlib1.c#L4559-L4571))), likewise the size peek in rsptbv and stpnbv; autlib3.c stpnpl ([autlib3.c:2523-2535](../reference/xppaut-8.0/autlib3.c#L2523-L2535) ([master 2524-2536](../reference/xppaut-master/autlib3.c#L2524-L2536))) and the same lines of stpnpd, stpntr, stpnpo, stpnbl; the seek in findlb [autlib1.c:4533-4536](../reference/xppaut-8.0/autlib1.c#L4533-L4536) ([master 4530-4533](../reference/xppaut-master/autlib1.c#L4530-L4533)).
- **Evidence:** servercheck's Run, Grab, Run sequence (added by [137fd08](https://github.com/MuhammadMoustafa/xppautX/commit/137fd08)) dies with a corrupted heap on Windows with the old code, usually and not always; an ASan build on Linux is clean, since Linux does not translate.
- **xppautX:** every read is checked and a short read stops the load; the label's position is remembered with `ftell` and restored with an absolute seek.
- **Card:** [137fd08](https://github.com/MuhammadMoustafa/xppautX/commit/137fd08), [e97ac2f](https://github.com/MuhammadMoustafa/xppautX/commit/e97ac2f), [c837e26](https://github.com/MuhammadMoustafa/xppautX/commit/c837e26).

## 11. AUTO: starts from the last integration with nothing integrated

Start > Periodic, Bdry Value, Homoclinic and hEteroclinic take the orbit and
period from the last integration. With no row stored, the period is read from
`storage[0][-1]`, one element before the table.

- **XPPAUT 8.0:** auto_nox.c get_start_period ([auto_nox.c:1721-1725](../reference/xppaut-8.0/auto_nox.c#L1721-L1725) ([master 1709-1713](../reference/xppaut-master/auto_nox.c#L1709-L1713)), `*p=storage[0][storind-1]`), called from autpp.c ([autpp.c:78](../reference/xppaut-8.0/autpp.c#L78) ([master 76](../reference/xppaut-master/autpp.c#L76))).
- **Evidence:** asancheck reported the heap-buffer-overflow in xppautX's port of the same line; without the sanitizer the program read garbage as the period (a fresh model, Start > Periodic before any integration).
- **xppautX:** refuses with "Integrate first" when fewer than 2 rows are stored.
- **Card:** [W97](https://github.com/MuhammadMoustafa/xppautX/issues/146) ([aacb087](https://github.com/MuhammadMoustafa/xppautX/commit/aacb087)).

## 12. AUTO: homoclinic test functions 11 and 12 read freed memory

`psiho` allocates `f0` and `f1`, then frees both before the `switch` on the
test function number, and the targets for test functions 11 and 12 (orbit
flips) still read `f1[]` and `f0[]` after the free.

- **XPPAUT 8.0:** autlib5.c psiho: the frees at [autlib5.c:1488-1489](../reference/xppaut-8.0/autlib5.c#L1488-L1489) ([master 1488-1489](../reference/xppaut-master/autlib5.c#L1488-L1489)), the reads at [autlib5.c:1594-1599](../reference/xppaut-8.0/autlib5.c#L1594-L1599) ([master 1594-1599](../reference/xppaut-master/autlib5.c#L1594-L1599)) (`L11`, `f1`) and [autlib5.c:1604-1609](../reference/xppaut-8.0/autlib5.c#L1604-L1609) ([master 1604-1609](../reference/xppaut-master/autlib5.c#L1604-L1609)) (`L12`, `f0`).
- **Evidence:** gcc's `-Wuse-after-free` reports both; any homoclinic run that computes test function 11 or 12 reads freed memory, giving garbage in the test function's value depending on the allocator.
- **xppautX:** the buffer an upcoming label still needs is freed after its last use.
- **Card:** [031eb17](https://github.com/MuhammadMoustafa/xppautX/commit/031eb17).

## 13. AUTO: the plot filter for two-parameter diagrams

`check_plot_type` is meant to keep the points whose second parameter is the
plotted one, and compares `icp2` with itself.

- **XPPAUT 8.0:** auto_nox.c check_plot_type [auto_nox.c:1231](../reference/xppaut-8.0/auto_nox.c#L1231) ([master 1221](../reference/xppaut-master/auto_nox.c#L1221)) (`flag2>0 && icp2!=icp2`, never true), so a diagram with two-parameter branches is never filtered by its second parameter.
- **Evidence:** latent in the examples (flag2 is 0 for the plot type they use; the saved autocheck diagram is the same before and after the fix).
- **xppautX:** compares with `Auto.icp2`, like the icp1 test above it.
- **Card:** [69cf3f0](https://github.com/MuhammadMoustafa/xppautX/commit/69cf3f0).

## 14. AUTO: stale eigenvalues at a run's first point

AUTO does not check stability at a run's first point. XPPAUT copies one global
slot of eigenvalues (`my_ev`, filled by the previous point's check) into every
stored point without testing whose they are, so the first point is stored
with the previous run's last values (zeros in a new session): numbers that
look real in the info strip, the stability circle and saved `.auto` files.

- **XPPAUT 8.0:** autevd.c addbif [autevd.c:170-197](../reference/xppaut-8.0/autevd.c#L170-L197) ([master 170-196](../reference/xppaut-master/autevd.c#L170-L196)) passes `my_ev.evr`, `my_ev.evi` to add_point and add_diagram; the check that the slot belongs to the point is commented out ([autevd.c:178](../reference/xppaut-8.0/autevd.c#L178) ([master 177](../reference/xppaut-master/autevd.c#L177))).
- **Evidence:** autocheck's cases: a run's first point is "not computed"; a restart from a label of the same kind carries the label's values; a Hopf start is "not computed".
- **xppautX:** one module (`auto_stability`) gives a point's values only for the point they belong to, else "not computed".
- **Card:** [W15](https://github.com/MuhammadMoustafa/xppautX/issues/27) ([13ea852](https://github.com/MuhammadMoustafa/xppautX/commit/13ea852)).

## 15. AUTO: NaN left in the model's parameters

AUTO's `func` and `bcnd` write the continuation parameters into the model's
`constants[]` at every right-hand side call, and a stopped run leaves them
there, with no test that they are finite. A periodic continuation that fails
to converge leaves a parameter at `-nan`, which the next integration, the
parameter box and the saved `.set` then use.

- **XPPAUT 8.0:** autpp.c func [autpp.c:35-37](../reference/xppaut-8.0/autpp.c#L35-L37) ([master 34-35](../reference/xppaut-master/autpp.c#L34-L35)) and bcnd [autpp.c:119-121](../reference/xppaut-8.0/autpp.c#L119-L121) ([master 117-119](../reference/xppaut-master/autpp.c#L117-L119)) (`constants[...]=par[i]`); auto_nox.c Grab ([auto_nox.c:344-345](../reference/xppaut-8.0/auto_nox.c#L344-L345) ([master 339-340](../reference/xppaut-master/auto_nox.c#L339-L340))) takes the diagram point's values the same way.
- **Evidence:** QA SCI-001 (2026-09-26): a failing periodic continuation of a model with parameter Iapp left Iapp at `-nan`.
- **xppautX:** a non-finite value is never written into the model (the old value is kept with a warning), and the parameters are restored when a run stops.
- **Card:** [W35a](https://github.com/MuhammadMoustafa/xppautX/issues/73) ([9bdb7a5](https://github.com/MuhammadMoustafa/xppautX/commit/9bdb7a5)).

## 16. AUTO: Fit of a flat quantity

AUTO's Fit sets the window to the data's minimum and maximum, so a flat
quantity (a steady branch's period, all 0) gives `ymin == ymax`, and
`IYVal` divides by that empty range: `(int)` of infinity, then `300 - INT_MIN`,
a signed overflow. The plot window's own Fit widens an empty range with
`check_val`; AUTO's does not.

- **XPPAUT 8.0:** auto_nox.c auto_fit [auto_nox.c:960-967](../reference/xppaut-8.0/auto_nox.c#L960-L967) ([master 950-957](../reference/xppaut-master/auto_nox.c#L950-L957)), IYVal [auto_nox.c:437-441](../reference/xppaut-8.0/auto_nox.c#L437-L441) ([master 431-435](../reference/xppaut-master/auto_nox.c#L431-L435)); the plot window's check_val ([graf_par.c:289](../reference/xppaut-8.0/graf_par.c#L289) ([master 289](../reference/xppaut-master/graf_par.c#L289))) is not used.
- **Evidence:** UBSan stopped xppautX's server in autocheck's two-view check (Fit on the period of a steady branch), reported as a signed integer overflow.
- **xppautX:** AUTO's Fit goes through the same `check_val`.
- **Card:** [W50](https://github.com/MuhammadMoustafa/xppautX/issues/98) review ([4218f49](https://github.com/MuhammadMoustafa/xppautX/commit/4218f49)).

## 17. AUTO and eispack call `exit()`

Numerical routines end the whole program instead of reporting a failure:
the AUTO library on `NPAR > NPARX`, `NCOL > 7`, a domain error in `dhhpr`, a
zero pivot ("Division by Zero, exiting"), a failed open of fort.8, and eispack
on a failed argument check. A parameter or setting that AUTO cannot take
therefore loses the session (parameters, data, everything) with no message the
front end can show.

- **XPPAUT 8.0:** autlib1.c [autlib1.c:370-377](../reference/xppaut-8.0/autlib1.c#L370-L377) ([master 370-377](../reference/xppaut-master/autlib1.c#L370-L377)), [autlib1.c:3320-3326](../reference/xppaut-8.0/autlib1.c#L3320-L3326) ([master 3317-3322](../reference/xppaut-master/autlib1.c#L3317-L3322)), [autlib1.c:4352-4358](../reference/xppaut-8.0/autlib1.c#L4352-L4358) ([master 4349-4355](../reference/xppaut-master/autlib1.c#L4349-L4355)); autlib4.c [autlib4.c:472-486](../reference/xppaut-8.0/autlib4.c#L472-L486) ([master 474-485](../reference/xppaut-master/autlib4.c#L474-L485)), [autlib4.c:637-652](../reference/xppaut-8.0/autlib4.c#L637-L652) ([master 637-652](../reference/xppaut-master/autlib4.c#L637-L652)); autlib2.c [autlib2.c:300-308](../reference/xppaut-8.0/autlib2.c#L300-L308) ([master 300-308](../reference/xppaut-master/autlib2.c#L300-L308)); eispack.c [eispack.c:3922-3925](../reference/xppaut-8.0/eispack.c#L3922-L3925) ([master 3915-3918](../reference/xppaut-master/eispack.c#L3915-L3918)).
- **Evidence:** `Ncol` above 7 in AUTO's Numerics: XPPAUT's library prints "Dimension exceeded : NCOL=... maximum=7" and exits with status 1 ([autlib1.c:3320-3326](../reference/xppaut-8.0/autlib1.c#L3320-L3326) ([master 3317-3322](../reference/xppaut-master/autlib1.c#L3317-L3322))); autocheck's sessiondiagram section checks that xppautX refuses it at its line instead.
- **xppautX:** each is an `xpp::Error` returned to the command, which shows it once; the program keeps running.
- **Card:** [W63a](https://github.com/MuhammadMoustafa/xppautX/issues/111) ([5651fe5](https://github.com/MuhammadMoustafa/xppautX/commit/5651fe5)), [W63d](https://github.com/MuhammadMoustafa/xppautX/issues/157).

## 18. AUTO files: saving an empty diagram

The diagram exports (`write_info_out`, `write_init_data_file`,
`write_pts`) open their file with `fopen(...,"w")` and only then test
whether the diagram has any point (`d->next==NULL`), and return without
closing the file: an existing file of that name is emptied and the handle
leaks. `save_diagram` returns -1 for an empty diagram and `save_auto` then
returns without a word, leaving a `.auto` with numerics and no orbit data.

- **XPPAUT 8.0:** diagram.c write_info_out [diagram.c:246-253](../reference/xppaut-8.0/diagram.c#L246-L253) ([master 246-253](../reference/xppaut-master/diagram.c#L246-L253)), write_init_data_file [diagram.c:359-367](../reference/xppaut-8.0/diagram.c#L359-L367) ([master 359-367](../reference/xppaut-master/diagram.c#L359-L367)), write_pts [diagram.c:427-436](../reference/xppaut-8.0/diagram.c#L427-L436) ([master 426-435](../reference/xppaut-master/diagram.c#L426-L435)), save_diagram [diagram.c:583-590](../reference/xppaut-8.0/diagram.c#L583-L590) ([master 582-589](../reference/xppaut-master/diagram.c#L582-L589)); auto_nox.c save_auto [auto_nox.c:2736-2764](../reference/xppaut-8.0/auto_nox.c#L2736-L2764) ([master 2707-2734](../reference/xppaut-master/auto_nox.c#L2707-L2734)).
- **Evidence:** write the diagram to `allinfo.dat`, then again with nothing computed: XPPAUT leaves `allinfo.dat` empty.
- **xppautX:** the file is opened (through `xpp::Writer`, temp then rename) only after there is something to write, so an existing file is left alone; an empty diagram is said to the user. W129 ([#181](https://github.com/MuhammadMoustafa/xppautX/issues/181)) puts this availability check before the name dialog and uses the common save permission and checked commit.
- **Card:** [W7b](https://github.com/MuhammadMoustafa/xppautX/issues/19) ([8679803](https://github.com/MuhammadMoustafa/xppautX/commit/8679803)), [dc69f40](https://github.com/MuhammadMoustafa/xppautX/commit/dc69f40).

## 19. Fixed buffers and tables in the model reader

XPPAUT reads a model into fixed buffers with `strcpy`, `strcat`, `sprintf`
and `fgets`, none of which checks a length. Each of these either cuts a
long line or name or writes past its buffer (the sizes are XPPAUT's):

- a logical line is read in pieces of 1023 characters (`fgets(temp,MAXEXPLEN,..)`), a longer line comes in as several, and its continuation lines (a trailing backslash) are joined with `strcat` into a 1024-byte buffer with no check ([form_ode.c:2629-2660](../reference/xppaut-8.0/form_ode.c#L2629-L2660) ([master 2629-2660](../reference/xppaut-master/form_ode.c#L2629-L2660))); the first line the same ([form_ode.c:406](../reference/xppaut-8.0/form_ode.c#L406) ([master 406](../reference/xppaut-master/form_ode.c#L406)));
- `save_eqn` keeps at most `MAXLINES` (5000) lines: the check is `NLINES>MAXLINES` ([form_ode.c:408](../reference/xppaut-8.0/form_ode.c#L408) ([master 408](../reference/xppaut-master/form_ode.c#L408)), so 5000 passes and the store overflows by one) and the two places that store lines after it ([form_ode.c:447](../reference/xppaut-8.0/form_ode.c#L447) ([master 447](../reference/xppaut-master/form_ode.c#L447)), [form_ode.c:2644](../reference/xppaut-8.0/form_ode.c#L2644) ([master 2644](../reference/xppaut-master/form_ode.c#L2644))) never check;
- the `#include` name is copied into a buffer with no bound ([form_ode.c:1274-1296](../reference/xppaut-8.0/form_ode.c#L1274-L1296) ([master 1274-1296](../reference/xppaut-master/form_ode.c#L1274-L1296))), a comment's text and action into 256-byte buffers ([form_ode.c:3024-3060](../reference/xppaut-8.0/form_ode.c#L3024-L3060) ([master 3024-3060](../reference/xppaut-master/form_ode.c#L3024-L3060)), an action over 255 characters overflows and is left unterminated), an array subscript into a 20-byte `num` ([form_ode.c:2841-2880](../reference/xppaut-8.0/form_ode.c#L2841-L2880) ([master 2841-2880](../reference/xppaut-master/form_ode.c#L2841-L2880)), and a `[` with no `]` reads on past the end of the line);
- a network's arguments are copied from tokens into 256-byte buffers ([simplenet.c:1666-1713](../reference/xppaut-8.0/simplenet.c#L1666-L1713) ([master 1666-1713](../reference/xppaut-master/simplenet.c#L1666-L1713)): `import(...)` with no `(` runs past the text, more than `MAXW` (50) weights overflow `tname`); a Gillespie chain of over 1000 reactions overflows `r[1000]` and `gcom` ([markov.c:461](../reference/xppaut-8.0/markov.c#L461) ([master 461](../reference/xppaut-master/markov.c#L461)), [simplenet.c:1062](../reference/xppaut-8.0/simplenet.c#L1062) ([master 1062](../reference/xppaut-master/simplenet.c#L1062)));
- file names: `fnx[256]` for a parameter or initial-condition file ([lunch-new.c:386](../reference/xppaut-8.0/lunch-new.c#L386) ([master 386](../reference/xppaut-master/lunch-new.c#L386)), [lunch-new.c:434](../reference/xppaut-8.0/lunch-new.c#L434) ([master 434](../reference/xppaut-master/lunch-new.c#L434))) takes the name with no bound; `-outfile` is copied into `batchout[256]`/`UserOUTFILE[256]` ([main.c:127-128](../reference/xppaut-8.0/main.c#L127-L128) ([master 124-125](../reference/xppaut-master/main.c#L124-L125))) with `sprintf(batchout,argv[i+1])` ([comline.c:307-308](../reference/xppaut-8.0/comline.c#L307-L308) ([master 305-306](../reference/xppaut-master/comline.c#L305-L306))): the argument is the format string, so a name with `%n` or `%s` crashes the program (`xppautX m.ode -silent -outfile out%n%s.dat` aborts on the old code), and a name over 255 characters runs past the end; AUTO's file names are 200 bytes (finding 8);
- the Volterra history table `Memory` is defined with `MAXKER` (50) entries ([parserslow2.c:92](../reference/xppaut-8.0/parserslow2.c#L92) ([master 92](../reference/xppaut-master/parserslow2.c#L92))) but allocated one per variable ([volterra2.c:118](../reference/xppaut-8.0/volterra2.c#L118) ([master 118](../reference/xppaut-master/volterra2.c#L118)), declared `Memory[MAXODE]` at [volterra2.c:40](../reference/xppaut-8.0/volterra2.c#L40) ([master 40](../reference/xppaut-master/volterra2.c#L40))): a Volterra model with more than 50 variables writes past the table;
- names: finding 3.

- **Evidence:** ASan/valgrind reports in xppautX's port of the same code ([W21](https://github.com/MuhammadMoustafa/xppautX/issues/34), [W29e](https://github.com/MuhammadMoustafa/xppautX/issues/53)), the `-outfile` crash by hand ([1f2a893](https://github.com/MuhammadMoustafa/xppautX/commit/1f2a893)), the Volterra table by reading the two definitions ([76c3950](https://github.com/MuhammadMoustafa/xppautX/commit/76c3950)); no example hits the limits.
- **xppautX:** lines, names, file names and arguments are `std::string`/`std::vector`, the parser's line has no limit, a missing `]` or `)` is an error, a model with 5001 lines loads, and a network chain of any length works.
- **Card:** [W29e](https://github.com/MuhammadMoustafa/xppautX/issues/53) ([23bf321](https://github.com/MuhammadMoustafa/xppautX/commit/23bf321), [70dc442](https://github.com/MuhammadMoustafa/xppautX/commit/70dc442)), [1f2a893](https://github.com/MuhammadMoustafa/xppautX/commit/1f2a893), [76c3950](https://github.com/MuhammadMoustafa/xppautX/commit/76c3950), [W76](https://github.com/MuhammadMoustafa/xppautX/issues/124).

## 20. Writes past the end and unterminated strings

Small ones, each a memory error in XPPAUT's own code:

- `add_export_list` allocates `strlen(in)` bytes and `strcpy`s the string into them, one byte short ([extra.c:217-219](../reference/xppaut-8.0/extra.c#L217-L219) ([master 217-219](../reference/xppaut-master/extra.c#L217-L219))), so every `export` line overflows the heap by one NUL.
- `init_ar_ic` clears `ar_ic[i].var[i]` for i up to 49 where `var` has 20 bytes ([integrate.c:236](../reference/xppaut-8.0/integrate.c#L236) ([master 231](../reference/xppaut-master/integrate.c#L231)), [integrate.c:133](../reference/xppaut-8.0/integrate.c#L133) ([master 132](../reference/xppaut-master/integrate.c#L132))): iterations 20-49 write past the end of `var` into the next fields (the intended index is 0).
- `find_ker` copies the digits of `int[mu]{...}` into `num` and calls `atof` without terminating it ([form_ode.c:1112-1120](../reference/xppaut-8.0/form_ode.c#L1112-L1120) ([master 1112-1120](../reference/xppaut-master/form_ode.c#L1112-L1120))), so a stale digit or exponent left in the buffer changes `mu`.
- `get_root` splits an array-plot name like `u10` into `u` and `10`, then writes the digits' terminator at `me[n-i]`, one byte too late ([arrayplot.c:477](../reference/xppaut-8.0/arrayplot.c#L477) ([master 477](../reference/xppaut-master/arrayplot.c#L477))), so `atoi` reads a byte never written.
- `io_numerics` names the integration method with a 13-entry array indexed by `METHOD` ([lunch-new.c:333-335](../reference/xppaut-8.0/lunch-new.c#L333-L335) ([master 333-335](../reference/xppaut-master/lunch-new.c#L333-L335)), [lunch-new.c:344](../reference/xppaut-8.0/lunch-new.c#L344) ([master 344](../reference/xppaut-master/lunch-new.c#L344))); Rosenbrock and Symplectic are methods 13 and 14 (the 15-entry list of [lunch-new.c:105-107](../reference/xppaut-8.0/lunch-new.c#L105-L107) ([master 105-107](../reference/xppaut-master/lunch-new.c#L105-L107)) has them), so File > Write set reads past the array.
- `do_new_parser` finds the `=` of an AUX, VECTOR, SPECIAL, EXPORT or `V` declaration with `find_char` and uses its answer unchecked ([form_ode.c:1570-1573](../reference/xppaut-8.0/form_ode.c#L1570-L1573) ([master 1570-1573](../reference/xppaut-master/form_ode.c#L1570-L1573)), [form_ode.c:1580-1583](../reference/xppaut-8.0/form_ode.c#L1580-L1583) ([master 1580-1583](../reference/xppaut-master/form_ode.c#L1580-L1583)), [form_ode.c:1590-1593](../reference/xppaut-8.0/form_ode.c#L1590-L1593) ([master 1590-1593](../reference/xppaut-master/form_ode.c#L1590-L1593))), -1 when it is absent, as the index to a copy: a malformed line (`AUX foo`) writes out of bounds.

- **Evidence:** `find_ker`: valgrind ([W21](https://github.com/MuhammadMoustafa/xppautX/issues/34)) in lamvolt.ode, cuplamdif.ode and mlvolt.ode, the models with `int[.5]`; the likely cause of mlvolt.ode's one-in-four md5 difference between runs on macOS CI. `get_root`: valgrind through an array plot (servercheck). `io_numerics`: henhei.ode (Rosenbrock) wrote its set file empty and the process died. `init_ar_ic`: gcc's `-Waggressive-loop-optimizations` reports iteration 20 as undefined. `do_new_parser`: gcc's `-Wmaybe-uninitialized` ([8cd24cc](https://github.com/MuhammadMoustafa/xppautX/commit/8cd24cc)).
- **xppautX:** each is replaced by a `std::string`/`std::vector` or a checked index; the method names include all 15 methods.
- **Card:** [a1650c9](https://github.com/MuhammadMoustafa/xppautX/commit/a1650c9), [ce435fd](https://github.com/MuhammadMoustafa/xppautX/commit/ce435fd), [5557f60](https://github.com/MuhammadMoustafa/xppautX/commit/5557f60), [49ff46d](https://github.com/MuhammadMoustafa/xppautX/commit/49ff46d), [8cd24cc](https://github.com/MuhammadMoustafa/xppautX/commit/8cd24cc), [W55](https://github.com/MuhammadMoustafa/xppautX/issues/103) (the export code was removed, [96a4608](https://github.com/MuhammadMoustafa/xppautX/commit/96a4608)).

## 21. `.set`, `.par` and `.ic` files read by guessing

- every number of a `.set` file is read with `fgets(bob,255,fp)` then `atoi`/`atof` ([lunch-new.c:658-695](../reference/xppaut-8.0/lunch-new.c#L658-L695) ([master 658-695](../reference/xppaut-master/lunch-new.c#L658-L695))): a line over 254 characters is cut and its remainder is read as the next line, shifting every later value; a short file, a missing line or text in place of a number gives 0 (or the previous value) with no message;
- File > Read set applies each value as it reads it ([lunch-new.c:178-250](../reference/xppaut-8.0/lunch-new.c#L178-L250) ([master 179-250](../reference/xppaut-master/lunch-new.c#L179-L250))), so a file that goes wrong in the middle leaves the model half changed; the `Incompatible parameters` check comes only after the first two lines;
- a `.par` file is read the same way ([lunch-new.c:384-415](../reference/xppaut-8.0/lunch-new.c#L384-L415) ([master 384-415](../reference/xppaut-master/lunch-new.c#L384-L415))); a `.ic` file is read with `fscanf` into the initial conditions as it goes, and an error returns without closing the file and with the values read so far already stored ([lunch-new.c:432-465](../reference/xppaut-8.0/lunch-new.c#L432-L465) ([master 432-464](../reference/xppaut-master/lunch-new.c#L432-L464))).

- **Evidence:** tests/test_lunch (a bad last value leaves the session as it was), servercheck check_load_all_or_nothing, tests/test_names ("a field longer than its buffer still gets its start and the next field reads from the next line").
- **xppautX:** a file of ours is read whole through one pipeline, every line and value checked, and applied in one step that cannot fail; a bad line stops the load with the file, the line and the value, and nothing is applied. XPPAUT's own `.set` is still imported, read the same strict way.
- **Card:** [2fbf61e](https://github.com/MuhammadMoustafa/xppautX/commit/2fbf61e), [W116](https://github.com/MuhammadMoustafa/xppautX/issues/167) ([a84f018](https://github.com/MuhammadMoustafa/xppautX/commit/a84f018)), [W125](https://github.com/MuhammadMoustafa/xppautX/issues/177) ([a6a4d5f](https://github.com/MuhammadMoustafa/xppautX/commit/a6a4d5f)).

## 22. Adjoint, H function and histogram tables on moved storage

The adjoint, its H function and the histogram keep pointers to the stored
columns beside their own (`my_trans.data[i]=storage[i]`). When a run fills the
store, `stor_full` reallocates every column (`realloc`), which may move them,
and the tables keep pointing at the old ones. Showing the adjoint, H function
or histogram again from its menu then reads freed memory: ASan's
heap-use-after-free, garbage values without it. After a new model is loaded
the file-scope `ADJ_HERE`, `H_HERE` and the tables still point into the
previous model's store.

- **XPPAUT 8.0:** adj2.c [adj2.c:155](../reference/xppaut-8.0/adj2.c#L155) ([master 155](../reference/xppaut-master/adj2.c#L155)) lends the columns once, [adj2.c:203-216](../reference/xppaut-8.0/adj2.c#L203-L216) ([master 203-216](../reference/xppaut-master/adj2.c#L203-L216)) (adj_back and h_back) show them again later; storage.c reallocstor [storage.c:65-72](../reference/xppaut-8.0/storage.c#L65-L72) ([master 65-72](../reference/xppaut-master/storage.c#L65-L72)) moves them, called from integrate.c stor_full [integrate.c:2730](../reference/xppaut-8.0/integrate.c#L2730) ([master 2701](../reference/xppaut-master/integrate.c#L2701)).
- **Evidence:** autocheck's `memory` section fails before the change (ASan: heap-use-after-free in browser_rows) and passes after.
- **xppautX:** the sets lend the store's columns afresh each time they are shown; the adjoint's sets belong to the session.
- **Card:** [W117](https://github.com/MuhammadMoustafa/xppautX/issues/168) ([5ae5282](https://github.com/MuhammadMoustafa/xppautX/commit/5ae5282)).

## 23. `fftcon` reads one value past its weight table

A network `fftcon` builds the convolution kernel from a weight table that
the load check requires to have `n` points (periodic) or `2n` (zero padded),
and then reads `w[i+n2]` for `i = 0..n2` with `n2 = n/2`, which reaches `w[n]`,
one past a table of exactly `n` points; the zero-padded form reads up to
`w[2N]`. A model that follows the documented table length reads one double
past the table into its kernel (and `w[n2-1]` is left unused).

- **XPPAUT 8.0:** simplenet.c load check [simplenet.c:635-642](../reference/xppaut-8.0/simplenet.c#L635-L642) ([master 635-642](../reference/xppaut-master/simplenet.c#L635-L642)), update_fft [simplenet.c:1430-1461](../reference/xppaut-8.0/simplenet.c#L1430-L1461) ([master 1430-1461](../reference/xppaut-master/simplenet.c#L1430-L1461)).
- **Evidence:** found while writing tests/test_fftcon.cpp ([W31a](https://github.com/MuhammadMoustafa/xppautX/issues/58)), which needed a table of n+1 points to avoid the over-read; no example uses fftcon, so no sanitizer saw it.
- **xppautX:** the kernel is built from `w[0..n-1]`, each once, equal to the matching `conv` network (tests/test_fftcon.cpp); the manual states the table length.
- **Card:** [W38](https://github.com/MuhammadMoustafa/xppautX/issues/81) ([f0c0292](https://github.com/MuhammadMoustafa/xppautX/commit/f0c0292)).

## 24. An export with no library copies uninitialised memory

`export` lines copy the model's inputs through a user's shared library and
copy its outputs back. With no `dll_lib=` (or a library that did not load),
`my_fun` returns without writing, and `do_in_out` copies the output buffer,
allocated with `malloc` and never written, into the model's variables.

- **XPPAUT 8.0:** extra.c do_in_out [extra.c:192-210](../reference/xppaut-8.0/extra.c#L192-L210) ([master 192-210](../reference/xppaut-master/extra.c#L192-L210)) (the copy is unconditional), my_fun [extra.c:128-131](../reference/xppaut-8.0/extra.c#L128-L131) ([master 128-131](../reference/xppaut-master/extra.c#L128-L131)) (returns 0 when `dlf.loaded==-1`), add_export_list [extra.c:215-230](../reference/xppaut-8.0/extra.c#L215-L230) ([master 215-230](../reference/xppaut-master/extra.c#L215-L230)) (`malloc` of the buffers).
- **Evidence:** getfrefm.ode and testdll.ode gave a different output.dat from one CI run to the next on Windows (the heap's contents), zeros on Linux's fresh pages.
- **xppautX:** the buffers were zeroed and copied back only when the function ran; the whole compiled-function feature was later removed ([W55](https://github.com/MuhammadMoustafa/xppautX/issues/103)).
- **Card:** [d5eebae](https://github.com/MuhammadMoustafa/xppautX/commit/d5eebae), [W55](https://github.com/MuhammadMoustafa/xppautX/issues/103) ([96a4608](https://github.com/MuhammadMoustafa/xppautX/commit/96a4608)).

## 25. An output stride of 0 divides by zero

`nout` (NJMP, every how many steps a point is stored) is checked only by
the Numerics dialog, whose `check_pos` turns 0 or less into 1. A .set file
(`io_int`) and an `@ nout=`/`@ njmp=` line (`atoi`) set it unchecked, and
`integrate` divides the step count by it: 0 is SIGFPE on the next Go.
`DeltaT` 0 reaches the line before, a floating division by zero, unchecked
the same way.

- **XPPAUT 8.0:** integrate [integrate.c:1912-1914](../reference/xppaut-8.0/integrate.c#L1912-L1914) ([master 1884-1886](../reference/xppaut-master/integrate.c#L1884-L1886)) (the division), the .set read [lunch-new.c:342](../reference/xppaut-8.0/lunch-new.c#L342) ([master 342](../reference/xppaut-master/lunch-new.c#L342)), the @ lines [load_eqn.c:1483-1497](../reference/xppaut-8.0/load_eqn.c#L1483-L1497) ([master 1482-1496](../reference/xppaut-master/load_eqn.c#L1482-L1496)), the dialog's check [numerics.c:118-122](../reference/xppaut-8.0/numerics.c#L118-L122), [numerics.c:230-231](../reference/xppaut-8.0/numerics.c#L230-L231) ([master 227-228](../reference/xppaut-master/numerics.c#L227-L228)).
- **Evidence:** the code review of 2026-10-01 (docs/code-review-2026-10-01.md): a saved session whose model.set had `0 nout` loaded, and Initialconds/Go exited on SIGFPE.
- **xppautX:** every source of a numerics value checks it by one rule of model_options' table (`rule_problem`): the Numerics dialog, the `@` lines (which until [W145](https://github.com/MuhammadMoustafa/xppautX/issues/197) checked only that the value was a number) and the .set reader ([W145](https://github.com/MuhammadMoustafa/xppautX/issues/197)).
- **Card:** [W145](https://github.com/MuhammadMoustafa/xppautX/issues/197) ([#197](https://github.com/MuhammadMoustafa/xppautX/issues/197)).

## 26. Set files ignore the model's names

A `.set` with the same equation/auxiliary and parameter counts is accepted
for any open model. The names after values are ignored: values are applied
in position order, even when they belong to different variables or parameters.

- **XPPAUT 8.0:** the count-only check in [lunch-new.c:179-217](../reference/xppaut-8.0/lunch-new.c#L179-L217) ([master](../reference/xppaut-master/lunch-new.c#L179-L217)); names are discarded by [io_int/io_double, lunch-new.c:658-685](../reference/xppaut-8.0/lunch-new.c#L658-L685) ([master](../reference/xppaut-master/lunch-new.c#L658-L685)), including [ICs and parameters, lunch-new.c:561-581](../reference/xppaut-8.0/lunch-new.c#L561-L581) ([master](../reference/xppaut-master/lunch-new.c#L561-L581)).
- **Evidence:** save a set for `x'=-a*x+b`, `y'=x-y`, `par a=1,b=2`; replace its parameter line `2  b` with `9  another_models_parameter`, leaving counts unchanged. `read_lunch` accepts it and sets `b=9`. The same follows for renamed ICs and torus variables.
- **xppautX:** every named numeric line is checked against its expected model name or setting label before any values apply. The first mismatch reports its file, line and source. A valid import is saved by the session writer as `<base>.snapx` beside the `.set`, then becomes the session open; failed saves keep valid imported values applied. W129 ([#181](https://github.com/MuhammadMoustafa/xppautX/issues/181)) asks permission for this save too; No keeps the imported values applied and the session identity unchanged.
- **Card:** [W153](https://github.com/MuhammadMoustafa/xppautX/issues/205) ([#205](https://github.com/MuhammadMoustafa/xppautX/issues/205)).

## 27. Rotation and boundary-value movies silently drop frames when full

XPPAUT ignores `film_clip()`'s failure in rotation and boundary-value
movies. A movie with more than 250 frames therefore silently loses its
remaining frames. Integration ranges instead show an error.

- **XPPAUT 8.0:** [graf_par.c:618](../reference/xppaut-8.0/graf_par.c#L618) ([master](../reference/xppaut-master/graf_par.c#L618)), [pp_shoot.c:264](../reference/xppaut-8.0/pp_shoot.c#L264) ([master](../reference/xppaut-master/pp_shoot.c#L264)); integration reports the failure at [integrate.c:742](../reference/xppaut-8.0/integrate.c#L742) ([master](../reference/xppaut-master/integrate.c#L736)).
- **Evidence:** the unchecked return values in those two movie loops; capture beyond the capacity cannot add a frame.
- **xppautX:** film capture returns an error value, retained and shown once by the command, including rotation and BVP movies.
- **Card:** [W133](https://github.com/MuhammadMoustafa/xppautX/issues/185) ([#185](https://github.com/MuhammadMoustafa/xppautX/issues/185)).

## 28. A `#` comment after a declaration makes names of its words

XPPAUT treats `#` as a comment only when it starts the line. On a
declaration line, the words after a `#` are read as more declarations:
`p gr=0.01  # Changed on Oct 6th 2009` declares the parameters `#`,
`Changed`, `on`, `Oct`, `6th` and `2009` (each with the value 0), and
`-qpars` lists them. A second line with a trailing comment declares `#`
again. This was reported to XPPAUT as its own issue:
[Ermentrout/xppaut#11](https://github.com/Ermentrout/xppaut/issues/11)
(open), with a model from ModelDB as the example.

- **XPPAUT 8.0:** a statement is a comment only when its first character
  is `#` ([form_ode.c:2274-2277](../reference/xppaut-8.0/form_ode.c#L2274-L2277)
  ([master 2274-2277](../reference/xppaut-master/form_ode.c#L2274-L2277)),
  and `is_comment` [form_ode.c:2822](../reference/xppaut-8.0/form_ode.c#L2822)
  ([master 2822](../reference/xppaut-master/form_ode.c#L2822)) skips only
  leading blanks before it); the `p` line's loop
  [form_ode.c:698-712](../reference/xppaut-8.0/form_ode.c#L698-L712)
  ([master 698-712](../reference/xppaut-master/form_ode.c#L698-L712)) makes
  a parameter of every token `get_next2`
  ([form_ode.c:3107](../reference/xppaut-8.0/form_ode.c#L3107),
  [master 3107](../reference/xppaut-master/form_ode.c#L3107)) returns, `#`
  and the comment's words included.
- **Evidence:** the model `p gr=0.01<TAB># Changed on Oct 6th 2009` then
  `p a=2, b=3 # another note`: XPPAUT declares the comment's words as
  parameters; xppautX's .ode reader did the same and then refused the
  second line (`q11.ode:2: # is a name already, or one parameter too
  many`), 2026-10-01.
- **xppautX:** `#` starts a comment wherever it is (maintainer, 2026-10-01),
  in .odex already and in the .ode reader with [W160](https://github.com/MuhammadMoustafa/xppautX/issues/212); the VS Code extension
  marks a trailing `#` in a .ode as an error naming XPPAUT's issue
  (MuhammadMoustafa/XPP-ODE-Extension#1, 2026-10-01).
- **Card:** [W160](https://github.com/MuhammadMoustafa/xppautX/issues/212) ([#212](https://github.com/MuhammadMoustafa/xppautX/issues/212)).

## 29. The numbers depend on the CPU and on the compiler's FMA

XPPAUT's results are not a function of the model: the same model gives
different numbers on different machines, which a chaotic model or an
adaptive step size turns into a different plot or file. Two causes, both in
how XPPAUT's code meets the machine, neither a bug in a line of it.

1. Its expression evaluator and its solvers call the C library's `sin`,
   `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `sinh`, `cosh`, `tanh`,
   `exp`, `log`, `log10`, `pow`, `erf`, `erfc`, `lgamma` directly, and
   glibc runs FMA or plain SSE2 variants of them, chosen by the CPU at run
   time; none is correctly rounded, and the variants disagree in about one
   call in 1500 (2 million random inputs each: `pow` 1 in 1600, `exp`
   1 in 1400, `sin` 1 in 1400, `log` 1 in 15000, `tan` 1 in 80000).
2. Its arithmetic is compiled with the compiler's default of contracting
   `a*b+c` into one fused multiply-add wherever the target has one (gcc's
   `-ffp-contract=fast`, which XPPAUT's makefile does not turn off): a
   build for a CPU with FMA (x86-64-v3 or `-march=native`, which a
   distribution or a user may choose; arm64 always has one) rounds
   differently from a baseline x86-64 build, in plain `+ - * /` models too.

- **XPPAUT 8.0:** the parser's function tables, [parserslow2.c:1950-1975](../reference/xppaut-8.0/parserslow2.c#L1950-L1975) (`fun1`, `sin` ... `lgamma`; [master](../reference/xppaut-master/parserslow2.c#L1858-L1883)) and [parserslow2.c:1598-1599](../reference/xppaut-8.0/parserslow2.c#L1598-L1599) (`atan2`, `pow`; [master](../reference/xppaut-master/parserslow2.c#L1582-L1583)), and the step-size controls, [stiff.c:233-281](../reference/xppaut-8.0/stiff.c#L233-L281) ([master](../reference/xppaut-master/stiff.c#L233-L281)) and [dormpri.c:574](../reference/xppaut-8.0/dormpri.c#L574) ([master](../reference/xppaut-master/dormpri.c#L574)), all call the C library's `pow`, `sin`, ... through their own names; the makefile's flags, [Makefile:44](../reference/xppaut-8.0/Makefile#L44), say `-O2` and nothing about contraction.
- **Evidence:** xppautX before W159 has XPPAUT's numerical code and the same calls. On one machine (Ubuntu 26.04, gcc 15.2, glibc 2.43, a Zen 4 CPU), the 184 example models' `output.dat` md5s against the committed baseline: with glibc masked to its SSE2 variants (`GLIBC_TUNABLES=glibc.cpu.hwcaps=-FMA`) 11 differ (`fp`, `sine-circle`, `atcoaster`, `fr`, `geisel`, `hhred`, `itoy`, `nf3`, `r3b`, `toy_ok`, `waterwheel`); the same sources built with `-mfma -mavx2` (gcc's default contraction) 29 differ, including `ross-orbit` and `rossler-pecora`, whose equations are only `+ - *`; masking AVX-512 changes none. GitHub's ubuntu-26.04 runner (an AMD EPYC 7763) produced exactly those 29 and two differing PostScript goldens, with the same compiler and C library as the machine where they matched.
- **xppautX:** the transcendental functions are CORE-MATH's correctly rounded ones (`xpp::math`, third_party/core-math), which return the exact value rounded to nearest and so the same bits on every CPU and C library, and the build passes `-ffp-contract=off`, so no target contracts. The example baselines and the goldens were rewritten once, on purpose (W159's commit names the models). `tools/mathcheck.sh` fails a direct call of the C library's. W163 closes the Bessel gap: `besselj`/`bessely` use the vendored musl implementation over `xpp::math`, with independent reference-value tests.
- **Card:** [W159](https://github.com/MuhammadMoustafa/xppautX/issues/211) ([#211](https://github.com/MuhammadMoustafa/xppautX/issues/211)); Bessel follow-up [W163](https://github.com/MuhammadMoustafa/xppautX/issues/215) ([#215](https://github.com/MuhammadMoustafa/xppautX/issues/215)).

## 30. AUTO orbit loading trusts file dimensions and storage capacity

The solution header supplies the number of rows and variables. XPPAUT
uses them to fill a fixed `u[NAUTO]` and the existing data table without
checking the variable bound or growing the table for those rows.

- **XPPAUT 8.0:** header counts in [auto_nox.c:2908-2930](../reference/xppaut-8.0/auto_nox.c#L2908-L2930) ([master 2879-2901](../reference/xppaut-master/auto_nox.c#L2879-L2901)); unchecked writes in [auto_nox.c:2678-2724](../reference/xppaut-8.0/auto_nox.c#L2678-L2724) ([master 2649-2695](../reference/xppaut-master/auto_nox.c#L2649-L2695)).
- **Evidence:** code review of those loops; a three-row, two-variable periodic orbit with only two allocated data rows writes a third row beyond storage. The session AUTO unit check exercises exactly those counts and verifies all three times and both variables after loading.
- **xppautX:** one shared AUTO header reader bounds dimensions, parameter counts, collocation and mesh allocation; the session checks complete restart payloads before restoring anything. Orbit loading grows through the data store's owner before writing rows.
- **Card:** [W155](https://github.com/MuhammadMoustafa/xppautX/issues/207) ([#207](https://github.com/MuhammadMoustafa/xppautX/issues/207)).

## 31. Model options bypass method suitability and ignore unknown methods

XPPAUT 8.0's `@ meth` reader compares only the first character against its
keys ([load_eqn.c:1116](../reference/xppaut-8.0/load_eqn.c#L1116),
[load_eqn.c:1507-1517](../reference/xppaut-8.0/load_eqn.c#L1507-L1517);
[master:1506-1516](../reference/xppaut-master/load_eqn.c#L1506-L1516)).
An unknown key leaves METHOD unchanged and still marks the option as set.
`@ meth=symplectic` selects Stiff (the `s` key), while `@ meth=y` selects
Symplectic without checking dimension. The menu alone rejects odd dimensions
and switches to Adams ([numerics.c:280-284](../reference/xppaut-8.0/numerics.c#L280-L284);
[master:277-281](../reference/xppaut-master/numerics.c#L277-L281));
`do_meth` starts Symplectic unconditionally
([numerics.c:674-685](../reference/xppaut-8.0/numerics.c#L674-L685)), whose step
reads and writes `y[j+1]` for every even `j < n`, including beyond the final
odd coordinate ([odesol2.c:193-206](../reference/xppaut-8.0/odesol2.c#L193-L206);
[master:193-206](../reference/xppaut-master/odesol2.c#L193-L206)).
Thus an odd model with `@ meth=y` reaches the paired solver.

**xppautX:** one picker reads full names and legacy keys and validates suitability
for every input. Unknown or unsuitable options fail the whole load at the
option line; menu refusal keeps the old method. W132,
[#184](https://github.com/MuhammadMoustafa/xppautX/issues/184).

## Known and kept

Not fixed in `.ode` and so not listed above: the expression grammar's
surprises (`^` groups left, comparisons bind tighter than arithmetic,
`@ total = 0.03` with spaces is silently ignored, `@` values are read with
`atof`), inventoried in docs/odex-quirks.md. They stay in `.ode` because
files depend on them; `.odex` (docs/odex.md) is the format without them.


## 32. Kinescope Cancel still saves frames

- **XPPAUT 8.0:** [kinescope.c:166-178](../reference/xppaut-8.0/kinescope.c#L166) initializes `base` to `frame`, ignores `new_string`'s return, and calls `save_movie` whenever the retained string is nonempty. Capture a frame, choose Save, then Cancel: `frame_0.gif` is still written. The same code is in [master kinescope.c:166-178](../reference/xppaut-master/kinescope.c#L166).
- **xppautX:** Save uses a file ask and checks its result before requesting pixels or opening a writer. A canceled name produces no output, verified by servercheck.
- **Filename limit:** the related array filename form uses 25-byte values ([arrayplot.c:363-381](../reference/xppaut-8.0/arrayplot.c#L363), [pop_list.h:19](../reference/xppaut-8.0/pop_list.h#L19)); `do_string_box` copies whole strings into those slots ([pop_list.c:253-263](../reference/xppaut-8.0/pop_list.c#L253)), risking overflow rather than reliably cutting input. Our `{:.24}` initialization was a refactoring artifact that retained 24 characters, not an upstream formatting expression. W130 removes filename form fields and that cut; the general upstream limit is finding [3](#3-names).
- **Card:** [W130](https://github.com/MuhammadMoustafa/xppautX/issues/182).

## 33. Model table counts have no upper bound

**Evidence:** a model with `x'=0`, `table f % 1000000000 0 1 t`, and
`done` requests 8 GB of doubles and a billion formula evaluations.
XPPAUT checks only `npts<2` before allocation
([tabular.c:357-366](../reference/xppaut-8.0/tabular.c#L357),
[master:357-366](../reference/xppaut-master/tabular.c#L357)). File tables
likewise check only `length<2` before allocation
([tabular.c:442-457](../reference/xppaut-8.0/tabular.c#L442),
[master:442-457](../reference/xppaut-master/tabular.c#L442)).

**xppautX:** file and formula tables share a one-million-point session
budget, checked before allocation and evaluation. Errors identify the model
table line or the file table's count line. W175,
[#227](https://github.com/MuhammadMoustafa/xppautX/issues/227).

## 34. Model includes read outside the model folder

**Evidence:** `#include ../private.inc` or an absolute path goes straight
to `fopen(newfile,"r")`, without containment or link checks
([form_ode.c:1413-1414](../reference/xppaut-8.0/form_ode.c#L1413),
[master:1413-1414](../reference/xppaut-master/form_ode.c#L1413)). A received
model can therefore cause reads of files outside its folder. xppautX's
former `--check` could also expose an invalid included line in JSON `source`.

**xppautX:** the model file owner confines disk reads to the main model's
folder, rejects absolute include names and traversal, and refuses linked
descendants and non-regular opened files. A received model alone cannot
redirect reads outside its folder. A local process that can mutate the folder
concurrently can still race an ancestor-directory check; these checks do not
provide a sandbox against concurrent filesystem mutation. W175,
[#227](https://github.com/MuhammadMoustafa/xppautX/issues/227).

## 35. Array print invalid render text selects blue-red

**XPPAUT 8.0:** [arrayplot.c:388-390](../reference/xppaut-8.0/arrayplot.c#L388)
uses `atoi` for the Render field and checks only its converted range; the
same code is in [master arrayplot.c:388-390](../reference/xppaut-master/arrayplot.c#L388).
Populate an array plot, choose Print and enter `garbage` as Render: it
becomes zero and writes blue-red PostScript. An out-of-range integer
instead silently selects grey scale.

**xppautX:** the shared strict integer parser must accept the whole field,
then the render enum bounds must pass, before labels change or a file is
asked for. Malformed input reports its command place and value; servercheck
covers `garbage`. [W177, #229](https://github.com/MuhammadMoustafa/xppautX/issues/229).

## 36. CVODE reverses the documented tolerance roles

XPPAUT documents TOLER as CVODE's relative tolerance and ATOLER as its
absolute tolerance ([xppopt.html:157](../reference/xppaut-8.0/help/xppopt.html#L157)),
but passes them in the reverse roles.

- **XPPAUT 8.0:** the caller passes `&TOLER, &ATOLER`
  ([integrate.c:1811](../reference/xppaut-8.0/integrate.c#L1811),
  [integrate.c:1991](../reference/xppaut-8.0/integrate.c#L1991)); `cvode`
  names those parameters `atol, rtol`
  ([cv2.c:95](../reference/xppaut-8.0/cv2.c#L95)), and `start_cv` forwards
  `rtol, atol` to `CVodeMalloc`
  ([cv2.c:36](../reference/xppaut-8.0/cv2.c#L36)). The upstream master has
  the same call ([integrate.c:1783](../reference/xppaut-master/integrate.c#L1783))
  and parameter/initialization order ([cv2.c:95](../reference/xppaut-master/cv2.c#L95)).
- **Evidence:** W34's amplitude-1e6 oscillator took 3769 steps with
  `tol=1e-3, atol=1e-10` and 537 with the values reversed. Both CVODE
  implementations showed this mapping. The original evaluation called
  this finding 33; it is 36 in this combined index. See
  [the archived evaluation](sundials-eval.md#findings-on-the-way).
- **xppautX:** retains the historical mapping. Swapping it would change
  existing results when the two tolerances differ; this assessment does
  not change numerics or correct the manual. The discrepancy remains
  actionable independently of adopting SUNDIALS.
- **Cards:** [W34](https://github.com/MuhammadMoustafa/xppautX/issues/72),
  [W183](https://github.com/MuhammadMoustafa/xppautX/issues/235).

## 37. Delete column is an unconditional placeholder

XPPAUT 8.0's `data_del_col` reports "Sorry - not working very well yet..."
and returns before the deletion code ([browse.c:262-270](../reference/xppaut-8.0/browse.c#L262)).
The upstream master has the same unconditional return
([browse.c:261-269](../reference/xppaut-master/browse.c#L261)). The manual
nevertheless describes deleting an Addcol column.

xppautX W197 implements removal of added columns in the storage owner,
protects time/model columns and columns used by plots, shifts remaining data
and formulas, and updates affected plot indices. `tools/uiworkflowcheck.mjs`
adds two formula columns, deletes the first, checks the second's values and
checks it recomputes correctly after a fresh run. The core column name
lookup, derived-column computation and storage owner are reused.

Implementation: [a1a664c1](https://github.com/MuhammadMoustafa/xppautX/commit/a1a664c1)
(W197, local branch).

## 38. A named set item is cut at its numeric prefix and the rest dropped silently

A `set name {a=1,b=2,...}` line is split at commas, spaces and semicolons
([load_eqn.c:819-837](../reference/xppaut-8.0/load_eqn.c#L819), master:
[818](../reference/xppaut-master/load_eqn.c#L818)); each item is split at
its first `=` ([load_eqn.c:1043-1064](../reference/xppaut-8.0/load_eqn.c#L1043))
and the value is read with `atof` ([load_eqn.c:854-859](../reference/xppaut-8.0/load_eqn.c#L854),
and for an option, e.g. `xlo`, [load_eqn.c:1697-1701](../reference/xppaut-8.0/load_eqn.c#L1697)).
`atof` stops at the first character that is not part of a number and never
reports it. A dot typed for a comma therefore changes the set without a word:
XPPAUT's own `lecar.ode` has `set pp {xp=v,yp=w,xlo=-.6.xhi=1.2,ylo=-.25,yhi=1.2}`
(line 31), whose third item is `xlo` = `-.6.xhi=1.2`: `xlo` is set to -0.6, `xhi`
is never set (the window keeps its old right edge) and nothing is printed.

- **Evidence:** load `examples/ode/lecar.ode` as it was before W229, choose
  the set `pp` (Named sets, or `--internset`): `xlo` becomes -0.6,
  `xhi` stays what it was.
- **xppautX:** a `.ode` is converted once and the cut is the conversion's
  warning `set-value` (docs/odex-quirks.md), written into the `.odex` as
  `xlo = -0.6` with a comment naming the original text, so the result is
  XPPAUT's; in a `.odex` (and when a set is chosen) an item whose value is not
  a number is an error at the set's line and nothing of the set is applied
  (W125). The example's comma is fixed.
- **Card:** [W229](https://github.com/MuhammadMoustafa/xppautX/issues/283).

## 39. AUTO's collocation computes a Jacobian at every point and throws it away

Every Newton step of a boundary-value continuation (periodic orbits, `ips`
2) builds the collocation system in `setubv`: first the Jacobian blocks,
calling `funi` at each collocation point with `ijac` 2
([setubv2.c:150](../reference/xppaut-8.0/setubv2.c#L150), master:
[150](../reference/xppaut-master/setubv2.c#L150)), then the residual in
`setubv_make_fa`, which calls `funi` at the same points with the same
arguments and `ijac` 2 again
([setubv2.c:435](../reference/xppaut-8.0/setubv2.c#L435), master:
[435](../reference/xppaut-master/setubv2.c#L435)) but reads only `f`.
With XPPAUT's `JAC=0`, `ijac` 2 makes `funi` difference the right-hand side:
one call for `f`, two per state and one per free parameter
([autlib3.c:4598-4652](../reference/xppaut-8.0/autlib3.c#L4598), master:
[4599](../reference/xppaut-master/autlib3.c#L4599)). `setubv_make_fa`
therefore evaluates the model 1 + 2·ndim + nfpr times per point to use one
of them, an `f` the loop before it has just computed: about half of the
right-hand-side evaluations of a periodic continuation are wasted. The code
is AUTO2000's; AUTO-07p computes the residual in the same loop as the
Jacobian. `setrhs`, the residual-only pass `solvbv` takes when it keeps the
factored Jacobian (`ifst` 0), does the same: `ijac` 2, only `f` read
([autlib2.c:640](../reference/xppaut-8.0/autlib2.c#L640), master:
[640](../reference/xppaut-master/autlib2.c#L640)).

- **Evidence:** W246's PY_S1Bf periodic branch (8 states, 2 free
  parameters: 19 evaluations per point for one), with `funi`'s `ijac` 0 in
  `setubv_make_fa` and nothing else changed: xppautX's interpreted run
  takes 18.19 +/- 0.16 s instead of 23.94 +/- 0.12 s (1.32x, warm-up plus
  five, one pinned CPU), and the whole diagram (every point of the steady
  and periodic branches) is bit-identical in all runs
  (paper/benchmarks/auto-parallel, `results/wsl`).
- **xppautX:** W253 made both residual passes ask for `f` only (`ijac` 0).
  W359 removes `setubv_make_fa` altogether: the Jacobian visit already
  supplies `f`, so the same residual arithmetic runs there. If boundary
  differencing changes the parameter bits, that residual is invalidated
  and recomputed using the same collocation loop. A chord
  iteration still uses `setrhs`; a cycle-fold direction skips it because
  the subsequent solve discards that RHS. HomCont retains that visit:
  boundary evaluation also advances projection continuity, so omitting it
  moves Nagumo's stored states in the last bits. `test_auto_views` counts the
  callbacks and checks the corrections; `autocompare` checks stored diagrams
  at one and automatic threads, without updating the baseline.
- **Card:** [W251](https://github.com/MuhammadMoustafa/xppautX/issues/307), [W253](https://github.com/MuhammadMoustafa/xppautX/issues/310), [W359](https://github.com/MuhammadMoustafa/xppautX/issues/421).

## 40. `shift`, `del_shft` and `set` reach outside the variables

`shift(u,k)`, `del_shft(u,d,k)` and `set(u,k,value)` index the variables
(and `shift` the constants) with `u`'s index plus an integer a model
expression computes at run time. XPPAUT bounds the reads only at the top,
and one too high (`in>NCON`, `in>MAXODE`: an index at the end reads one
past it), not at all below (a negative shift reads before the first
variable), and does not bound `set`'s write at all.

- **XPPAUT 8.0:** `do_shift` checks `in>NCON` and `in>MAXODE` before
  reading `constants[in]` or `variables[in]`
  ([parserslow2.c:1854](../reference/xppaut-8.0/parserslow2.c#L1854),
  [parserslow2.c:1860](../reference/xppaut-8.0/parserslow2.c#L1860));
  `do_delay_shift` repeats the upper bound
  ([parserslow2.c:1888](../reference/xppaut-8.0/parserslow2.c#L1888));
  `ENDSET` writes `variables[iv]` with no bound
  ([parserslow2.c:2179](../reference/xppaut-8.0/parserslow2.c#L2179)).
- **Evidence:** `tests/test_compile.cpp` evaluates `shift(t,-1)`, a shift
  to exactly `variables.size()` and one past it, each 0, compiled and
  interpreted.
- **xppautX:** each index is checked against the vector's real size on
  both sides; a read outside gives 0 (what XPPAUT's check meant), a write
  outside is not done ([W259](https://github.com/MuhammadMoustafa/xppautX/issues/316) for `set`).
- **Card:** [W265](https://github.com/MuhammadMoustafa/xppautX/issues/322).

## 41. Rosenbrock scales past or short of its banded Jacobian

With banded Jacobians, Rosenbrock stores `n*(bandlo+bandup+1)` entries but
scales `n*n`: a band wider than the system leaves entries unscaled, while a
narrow band scales beyond its matrix. Band widths wider than the system can
also make the Jacobian write beyond the solver's `n*n` work allocation.

- **XPPAUT 8.0:** `rosen` reads the configured band widths and scales
  `n*n` entries before factoring the banded matrix
  ([odesol2.c:538](../reference/xppaut-8.0/odesol2.c#L538)).
- **Evidence:** a two-variable `rb23` model with `bandlo=bandup=1` fails
  with "Integration not completed"; a band width of 99 on a two-variable
  model requests more entries than the solver's work array holds.
- **xppautX:** widths are clamped to `n-1` in one place (`model_jacobian_form`, jacobian.h), read
  by Backward Euler, Rosenbrock and Jacobian construction; work allocations
  cover `n*max(n,mt)`. CVODE's own `CVBand` call is left to CVODE.
- **Card:** [W261](https://github.com/MuhammadMoustafa/xppautX/issues/318).

## 42. Bessel I casts its order without checking it

XPPAUT converts the order directly to `int`; a NaN or out-of-range model value has undefined conversion behavior. Negative orders also flow into the recurrence's square root and loop length.

- **XPPAUT 8.0:** `bessel_i` uses `n=(int)nn` ([parserslow2.c:1654](../reference/xppaut-8.0/parserslow2.c#L1654)); XPPAUT master has the same cast ([parserslow2.c:1637](../reference/xppaut-master/parserslow2.c#L1637)).
- **Evidence:** `tests/test_math.cpp` covers NaN order and argument, negative order, and order limits.
- **xppautX:** NaN and non-finite inputs return NaN, negative integer orders use `I_{-n}(x)=I_n(x)`, and orders above 100000 return NaN to bound the input-controlled recurrence.
- **Card:** [W271](https://github.com/MuhammadMoustafa/xppautX/issues/328).

## 43. An xy table extrapolates past its last point the wrong way, in code nothing reaches

A lookup table given as x and y values (`xyvals`) extrapolates linearly past either end. Before the first x it continues the first segment, as it should; past the last x it uses the last segment's slope with its sign reversed, so the table's value turns back at its last point instead of continuing.

- **XPPAUT 8.0:** `lookupxy` returns `yv[n-1]+(yv[n-2]-yv[n-1])*(x-xv[n-1])/(xv[n-1]-xv[n-2])` for `x>=xv[n-1]` ([tabular.c:215-216](../reference/xppaut-8.0/tabular.c#L215)); master has the same ([tabular.c:215-216](../reference/xppaut-master/tabular.c#L215)). The other end uses `(yv[1]-yv[0])`, the slope's right sign ([tabular.c:213-214](../reference/xppaut-8.0/tabular.c#L213)), and an evenly spaced table past its last point `(y[n-1]-y[n-2])` ([tabular.c:284](../reference/xppaut-8.0/tabular.c#L284)).
- **Evidence:** a table of the points (0,0), (1,1), (2,2) gives -1 at x=-1, continuing its line, but 1 at x=3, not 3: `2+(1-2)*(3-2)/(2-1)`.
- **Never reached:** no reader sets `xyvals`. The format is described in tabular.c's comment ([tabular.c:34-40](../reference/xppaut-8.0/tabular.c#L34)) but `load_table` reads only the evenly spaced form, so `lookupxy` ([tabular.c:207](../reference/xppaut-8.0/tabular.c#L207), called at [263](../reference/xppaut-8.0/tabular.c#L263)) is dead code in XPPAUT; the wrong slope above is in code no model can run.
- **xppautX:** W260 kept the lookup as XPPAUT computed it; W373 found it unreachable and removed it with the `xyvals` field (maintainer, 2026-10-15: no model or user needs tables of uneven x). An implementation of the format (x checked increasing at load, lookups by halves) is kept unmerged on the branch `task/W373-xyvals-tables`.
- **Card:** [W260](https://github.com/MuhammadMoustafa/xppautX/issues/317), [W373](https://github.com/MuhammadMoustafa/xppautX/issues/435).

## 44. Every name is found by scanning every symbol

Reading a formula, XPPAUT finds each name by scanning its whole symbol table, the built-in functions and then every variable, parameter and function the model declared, from the top. A model with N names whose formulas hold about N names in all costs about N squared comparisons, so its load grows faster than its size: one reason both programs' load times were superlinear in W241's size study.

- **XPPAUT 8.0:** `find_tok` tries every symbol for the longest one the text starts with ([parserslow2.c:1557-1585](../reference/xppaut-8.0/parserslow2.c#L1557-L1585); master [1541](../reference/xppaut-master/parserslow2.c#L1541)) and `find_name` compares the name with every symbol in turn ([parserslow2.c:892-907](../reference/xppaut-8.0/parserslow2.c#L892-L907); master [891](../reference/xppaut-master/parserslow2.c#L891)).
- **Evidence:** xppautX had the same scans. With the symbols indexed, the one-step `--silent` run of the 4,800-state lecdiff chain went from 835 to 274 ms (XPPAUT 8.0: 627 ms); putting the linear scan back alone made it 747 ms (W245's five-run medians, under the machine lock).
- **xppautX:** keeps a sorted index of the names (an exact name is one lookup; the tokenizer's longest match walks the prefixes while some name still starts with one), picking the first of equal names as XPPAUT's scan does, so every formula reads the same symbols.
- **Card:** [W245](https://github.com/MuhammadMoustafa/xppautX/issues/301).

## 45. A model past MAXODE overflows the stack instead of being refused

XPPAUT holds at most `MAXODE` (5000) variables, but the loader does not refuse a model with more: it copies each equation's and each fixed quantity's name into arrays of `MAXODE1` (4999) rows with no bound check, so the 4999th name is written past the array, on the stack.

- **XPPAUT 8.0:** `MAXODE` 5000 and `MAXODE1` 4999 ([xpplim.h:4-5](../reference/xppaut-8.0/xpplim.h#L4-L5); master [4-5](../reference/xppaut-master/xpplim.h#L4-L5)); `compile_em` declares `char vnames[MAXODE1][MAXVNAM], fnames[MAXODE1][MAXVNAM], ...` on the stack ([form_ode.c:1760](../reference/xppaut-8.0/form_ode.c#L1760); master [1760](../reference/xppaut-master/form_ode.c#L1760)) and fills them with `strcpy(vnames[nvar],tmp)` ([form_ode.c:1812](../reference/xppaut-8.0/form_ode.c#L1812); master [1812](../reference/xppaut-master/form_ode.c#L1812)) and `strcpy(fnames[nfix],tmp)` ([form_ode.c:1865](../reference/xppaut-8.0/form_ode.c#L1865); master [1865](../reference/xppaut-master/form_ode.c#L1865)), counting `nvar` and `nfix` with no limit. The only checks are elsewhere: `add_var` prints "too many variables !!" and returns ([parserslow2.c:464-467](../reference/xppaut-8.0/parserslow2.c#L464-L467); master [463](../reference/xppaut-master/parserslow2.c#L463)), after which loading goes on, and the old-style `init`/`var` line's `IN_VARS==MAXODE` test ([form_ode.c:775](../reference/xppaut-8.0/form_ode.c#L775); master [775](../reference/xppaut-master/form_ode.c#L775)) covers only that path.
- **Evidence:** W269's size study (paper/benchmarks/size-scaling, `cases-w269.json`'s `legacy_refusal`), XPPAUT 8.0 built as the paper's WSL build (gcc 15.2, Ubuntu's default `_FORTIFY_SOURCE`), `xppaut MODEL.ode -silent`: XPPAUT's own lecdiff200.ode resized to 3200 and 6400 cells (6400, 12800 states) and amari.ode to 6400 points end with `*** buffer overflow detected ***: terminated`; gdb puts the abort in `compile_em` at form_ode.c:1812 (name `W1800`, the 5000th equation of lecdiff) and form_ode.c:1865 (`FU4999`, amari's 5000th fixed quantity). amari at 3200 points (6400 variables) prints "too many variables !!" and "Duplicate name FU1798" and does not run. A build without fortify checks would write past the arrays unnoticed.
- **xppautX:** no `MAXODE`: the tables are sized from the loaded model (W243), and the same four models load and integrate (W269 timed them; their outputs agree between the compiled build and the interpreter).
- **Card:** [W269](https://github.com/MuhammadMoustafa/xppautX/issues/326) (found); [W243](https://github.com/MuhammadMoustafa/xppautX/issues/299) (the limit lifted).

## 46. Parameter files ignore names and load by position

XPPAUT checks only the count in `io_parameter_file`
([lunch-new.c:405](../reference/xppaut-8.0/lunch-new.c#L405),
[master](../reference/xppaut-master/lunch-new.c#L405)). Its `io_parameters`
loops in model order and assigns the next value to `upar_names[i]`
([lunch-new.c:513](../reference/xppaut-8.0/lunch-new.c#L513),
[master](../reference/xppaut-master/lunch-new.c#L513)); `io_double` reads
with `atof` and ignores the name after the number
([lunch-new.c:679](../reference/xppaut-8.0/lunch-new.c#L679),
[master](../reference/xppaut-master/lunch-new.c#L679)).

Source-traced reproducer: a model declaring `par a=1,b=2` and a file
`2   Number params`, `4  b`, `3  a` (one line each) load `a=4,b=3`
in XPPAUT, silently. xppautX loads `a=3,b=4`, as servercheck verifies,
and refuses unknown, duplicate or missing names before applying values.
The `.ic` format stays positional; XPPAUT's `io_ic_file` reads directly
into `last_ic` before detecting short or long input
([lunch-new.c:454](../reference/xppaut-8.0/lunch-new.c#L454),
[master](../reference/xppaut-master/lunch-new.c#L454)); xppautX tests both
counts without applying anything on failure.
[W310 / #369](https://github.com/MuhammadMoustafa/xppautX/issues/369).


## 47. AUTO: a special point whose location does not converge leaves the iterate behind

AUTO locates a bifurcation point, a fold or a user zero by the secant and
Mueller method: it steps back along the branch by the secant estimate,
Newton-corrects there, tests the function again, and repeats until the step
is below EPSS (`lcspae` for equilibria, `lcspbv` for periodic orbits and
boundary value problems). When the ITMX iterations do not converge it prints
`NOTE:Possible special point`, sets the test function's value to 0 and
returns, with the solution, its tangent, the old point and the monodromy
matrix left at the last displaced iterate, a point behind the one the branch
had reached
([autlib1.c:1678-1683](../reference/xppaut-8.0/autlib1.c#L1678) for `lcspae`,
master: [1677](../reference/xppaut-master/autlib1.c#L1677);
[autlib1.c:6498-6500](../reference/xppaut-8.0/autlib1.c#L6498) for `lcspbv`,
master: [6495](../reference/xppaut-master/autlib1.c#L6495)). The
continuation goes on from that point: its next tangent is the secant from
the displaced iterate to the next solution, so a step that lands back on the
branch already traced reverses the direction. AUTO-07p saved the state
before the first iteration and sets it back (`bvp.f90` `LCSPBV`, 'set back
to previous (converged) state'; `ae.f90` `LCSPAE` the same), so a
test function that changes sign by noise costs nothing.

- **Evidence:** the Morris-Lecar model with `v3=0, v4=0.3, gca=1.1, phi=0.2`
  (`tests/auto/tstauto_lecar.json`, W299), the cycles from the first Hopf
  point at NTST 15 with the description's constants. Where the cycles'
  largest multiplier is 3e6 the BP test function changes sign by noise at
  point 21; the location runs its 9 iterations and ends in the NOTE (XPPAUT
  8.0's own diagnostics file: `2 21 NOTE:Possible special point`,
  AUTO-07p's the same at its point 21). AUTO-07p continues down the cycles'
  fold and labels it at iapp 0.248413876, norm 0.381874, period 25.7582.
  XPPAUT 8.0 and xppautX turn round at point 21 (iapp rises again from
  0.248756 to 0.249114 while the norm falls from 0.3578 to 0.3433), zigzag
  over the visited branch and end the run at point 30 on the first part of
  it (iapp 0.24913, norm 0.34268, multiplier 5e6): the fold is never
  reached. The fold is real, not the mesh's: at NTST 60 and EPS 1e-9
  AUTO-07p and xppautX both find it at iapp 0.24841347, norm 0.381815,
  period 25.7824, and xppautX at NTST 15 with the missing restore added to
  `lcspbv` (a scratch build, not committed) labels it at 0.248413878, norm
  0.381875, period 25.7578, the point AUTO-07p labels.
- **xppautX:** fixed by W326, the first W302 stage: `lcspae` and `lcspbv`
  retain owned copies before the first location iteration and restore them
  when ITMX is exhausted. This includes the solution, tangent, old point,
  parameters, time derivative, step history, test value and stability data,
  plus the factored system reused by the next fold test. Successful
  locations keep their existing algorithm. The numerical baseline changes
  intentionally for runs that encounter a failed location; Morris-Lecar
  now reaches the LP at NTST 15. Its missing-LP differences against
  AUTO-07p are removed and the extra LP against XPPAUT is recorded instead.
- **Card:** [W300](https://github.com/MuhammadMoustafa/xppautX/issues/358), [W326](https://github.com/MuhammadMoustafa/xppautX/issues/387).

## 48. AUTO storage has fixed variable, parameter and branch-point limits

XPPAUT 8.0 defines `NAUTO` as 400 ([autlim.h:2](../reference/xppaut-8.0/autlim.h#L2)),
`NPARX` as 36, `NPARX2` as twice that and `NBIFX` as 20
([auto_c.h:17](../reference/xppaut-8.0/auto_c.h#L17),
[auto_c.h:18](../reference/xppaut-8.0/auto_c.h#L18),
[auto_c.h:20](../reference/xppaut-8.0/auto_c.h#L20)). Its selectable model-parameter
count and `AutoPar` array are eight
([auto_nox.c:191](../reference/xppaut-8.0/auto_nox.c#L191)). Branch-point storage
stops at `NBIFX`, discarding additional branch points
([autlib1.c:2116](../reference/xppaut-8.0/autlib1.c#L2116)). These are storage limits,
not mathematical restrictions on continuation; the 36 slots also hold AUTO's
own numerical parameters, including the period and homoclinic equilibria.
The periodic-orbit output also has four 1,000-state scratch arrays
([autlib1.c:6972](../reference/xppaut-8.0/autlib1.c#L6972)), indexed over every
model variable ([autlib1.c:7115](../reference/xppaut-8.0/autlib1.c#L7115));
W298 sizes these from the run dimension too.

W298 sizes variable and parameter storage from the loaded model and grows the
branch-point vectors as points are found. Reserved numerical slots keep their
indices; additional model parameters occupy separate slots. The default remains
the first eight model parameters. `tests/test_auto_limits.cpp` and autocheck's
`limits` section continue a generated 401-variable, 40-parameter model in its
40th parameter and check the equilibrium `x400=p39`, plus the saved diagram.
Sent solution and diagram members are checked against the model and archive
memory budget before their contents are applied.
[W298 / #356](https://github.com/MuhammadMoustafa/xppautX/issues/356).

## 49. AUTO homoclinic form overflows its value table from 50 variables

XPPAUT 8.0's `get_homo_info` declares 100 value rows
([auto_nox.c:2263](../reference/xppaut-8.0/auto_nox.c#L2263)) but builds
`2 + 2*NODE` fields ([auto_nox.c:2264](../reference/xppaut-8.0/auto_nox.c#L2264)).
Its right-state loop writes `v[i+2+NODE]`
([auto_nox.c:2280](../reference/xppaut-8.0/auto_nox.c#L2280)); at NODE=50
that includes rows 100 and 101, outside the array. This is source-level
out-of-bounds evidence; XPPAUT's GUI was not dynamically exercised here.

W301 sizes the value vector from the same model-derived field count, using
the shared form API. `tests/test_auto_limits.cpp` exercises a 401-state form:
804 fields, final left/right labels and submitted values, and both equilibrium
vectors updated. The continuation algorithm and its numerical baseline are
unchanged. [W301 / #359](https://github.com/MuhammadMoustafa/xppautX/issues/359).

## 50. AUTO accepts NaN as a converged BVP solution

XPPAUT 8.0's supplied `tstheti` model requests projection conditions
([tstheti.ode:22](../reference/xppaut-8.0/ode/tstheti.ode#L22),
[tstheti.ode:23](../reference/xppaut-8.0/ode/tstheti.ode#L23)), but `hom_bcs`
is deprecated and always returns zero
([parserslow2.c:1944](../reference/xppaut-8.0/parserslow2.c#L1944)). Thus at least
two boundary rows and their derivatives are zero. The model has seven states
but supplies only six boundary expressions; the reader fills the seventh with
zero ([form_ode.c:465](../reference/xppaut-8.0/form_ode.c#L465)), making three
boundary rows zero. This BVP is rank deficient regardless of the integrated
start or the left/right equilibrium values. The
HomCont constants are used for IPS=9
([autlib1.c:523](../reference/xppaut-8.0/autlib1.c#L523)); the supplied fixture
runs as an ordinary IPS=4 BVP, so those settings cannot supply its constraints.

The BVP Newton test ignores NaN increments when taking maxima
([autlib1.c:5662](../reference/xppaut-8.0/autlib1.c#L5662)); its parameter test
leaves `done` true when the relative increment is NaN
([autlib1.c:5686](../reference/xppaut-8.0/autlib1.c#L5686)), and accepts that
iterate as converged ([autlib1.c:5699](../reference/xppaut-8.0/autlib1.c#L5699)).
This is upstream source evidence; XPPAUT's GUI AUTO was not executed.
In xppautX's former baseline the supplied integrated orbit was finite at point
1, then all 119 continued points (2-120) had NaN states and continuation
parameter. CI 38421830601 differed between Linux and UCRT only in NaN signs.

W343 checks Newton increments and active solutions before acceptance in all
AUTO Newton paths, uses the existing failed-step retries down to Dsmin (or
fixed-step failure), restores the previous solution, and reports the branch
and attempted point through the run's command error. Final storage also refuses
nonfinite derived quantities and spectra. The fixture expects the point-2 error;
its baseline keeps only the unchanged finite supplied start. Session saving
also uses the existing populated-point flag to preserve this one-point diagram.
The start is not an independently validated connection. `test_auto_location` checks retry, restoration,
refusal and unchanged storage; `autocompare` requires the exact placed error and
finite saved data at both thread counts. [W343 / #404](https://github.com/MuhammadMoustafa/xppautX/issues/404).


### W345: a finite Nagumo connection through HomCont

The underlying planar system **can** be continued. The supplied augmented
model cannot simply be switched to IPS=9: its Jacobian at either equilibrium
has the block `per*[[0,1],[q,-c]]` (`q=1-a` on the left, `q=a` on the right)
and five zero rows/columns, hence rank 2 and five zero eigenvalues. Its IPS=4
boundary matrix has rank 4 of 7 at `a=0.5`: the four equilibrium constraints
are independent, and the other three rows vanish. HomCont would instead add
fourteen equilibrium conditions, ten identically zero, from the five dummy
equations at each end
([autlib5.c:497](../reference/xppaut-8.0/autlib5.c#L497)). Those dummy states
and the slaved `c'=0` must be removed for the hyperbolic planar formulation.

[W345 / #406](https://github.com/MuhammadMoustafa/xppautX/issues/406) supplies
[`tstheti_homcont.odex`](../examples/tstauto/tstheti_homcont.odex): only `(u,up)`
are states, `(a,c)` are parameters, and the vector field has no `per` factor.
The converter and original `examples/ode/tstheti.odex` stay unchanged:
conversion preserves the foreign model's meaning, rather than selecting a
different continuation problem. The original expected-error description and
one-point baseline are retained byte for byte as `ode_tstheti`.
`tstauto_tstheti_homcont` now describes the planar companion and its working connection.

There is no HomCont model option today (`model_options.cpp`). Integrate the
companion for 20, select AUTO Axes Par1=`a`, Par2=`c`, then Run > Heteroclinic.
In Homoclinic info enter dim unstable=dim stable=1, left `(1,0)`, right `(0,0)`.
This chooses IPS=9, IEQUIB=-2
([auto_nox.c:2378](../reference/xppaut-8.0/auto_nox.c#L2378),
[auto_nox.c:2391](../reference/xppaut-8.0/auto_nox.c#L2391)). HomCont supplies
the two projections ([autlib5.c:442](../reference/xppaut-8.0/autlib5.c#L442)),
four equilibrium equations, and integral phase condition
([autlib5.c:795](../reference/xppaut-8.0/autlib5.c#L795)). Both planar saddles
are hyperbolic for `0<a<1` since the two determinants are `-(1-a)` and `-a`.
Continue both `a` and `c` with fixed interval `T=20`; grab the first EP and
reverse DS for the other direction. No replacement `hom_bcs` is needed.

For `u=1/(1+exp(k*x))`, `u'=-k*u*(1-u)` and
`u''=k*k*u*(1-u)*(1-2*u)`. Dividing the stated equation by `u*(1-u)` gives
`(1-2*k*k)*u + k*k-c*k-a=0`, so `k=1/sqrt(2)` (decreasing front) and
**`c(a)=(1-2*a)/sqrt(2)`**, including this model's minus sign in `g`.

`autocheck nagumo` compares every stored point with that exact speed, in both
directions, at NTST=80 and 160 (NCOL=4, EPSL=EPSU=EPSS=1e-9), then lengthens
the interval to 24 at NTST=160. The speed acceptance bounds are 5e-6 at T=20
and 5e-7 at T=24. The centred exact front's endpoint tail is 8.486e-4 at T=20;
linear eigendirection projections neglect quadratic tail terms, of order
`exp(-T/sqrt(2))` (7.21e-7), rather than satisfying the infinite-interval
front exactly. Extending T by 4 reduces that scale by 16.9; the check requires
at least a fivefold reduction of maximum speed error. The 142-point branch
has maximum errors 1.59722093e-6 (NTST=80), 1.59722094e-6 (NTST=160), and
2.26004569e-7 (T=24, NTST=160): doubling the mesh changes the maximum by less
than 1e-13, and extending the interval reduces it 7.07-fold. Mesh refinement
tests that this is truncation error, not unresolved collocation. These are verified
accuracy bounds on this branch, not rigorous interval-arithmetic enclosures
or an assertion of infinite-interval equality. No native XPPAUT or independent
AUTO-07p HomCont execution is claimed.

## 51. AUTO: failed-step endpoint stability

XPPAUT 8.0's ordinary periodic MX endpoint retains the previous spectrum:
`stepbv` restores `uoldps` at [autlib1.c:5759](../reference/xppaut-8.0/autlib1.c#L5759),
`cnrlbv` skips the stability test after failure at
[autlib1.c:5218](../reference/xppaut-8.0/autlib1.c#L5218), and `addbif` stores
`my_ev` unconditionally at [autevd.c:200](../reference/xppaut-8.0/autevd.c#L200)
([master](../reference/xppaut-master/autevd.c#L200)). In the ordinary case those
values belong to the restored solution; xppautX's point-identity check had
instead made them zero at the new MX point number.

The unconditional store is wrong when switching back to an earlier equilibrium
branch point. `swpnt` copies that earlier solution from `stu` at
[autlib1.c:2214](../reference/xppaut-8.0/autlib1.c#L2214)
([master](../reference/xppaut-master/autlib1.c#L2211)); `swprc` restores it after
DSMIN failure at [autlib1.c:2455](../reference/xppaut-8.0/autlib1.c#L2455)
([master](../reference/xppaut-master/autlib1.c#L2452)). The failure jumps directly
to endpoint storage at [autlib1.c:990](../reference/xppaut-8.0/autlib1.c#L990)
([master](../reference/xppaut-master/autlib1.c#L990)), skipping `fnhbae` and its
`send_eigen` at [autlib1.c:1914](../reference/xppaut-8.0/autlib1.c#L1914)
([master](../reference/xppaut-master/autlib1.c#L1911)). Thus the endpoint gets
the spectrum of the last solution checked on the previous branch, which can
have a different Jacobian. A periodic location has the same hazard:
`lcspbv` advances from an unstored converged solution at
[autlib1.c:6469](../reference/xppaut-8.0/autlib1.c#L6469)
([master](../reference/xppaut-master/autlib1.c#L6466)), and returns before its
next stability test when that step fails at
[autlib1.c:6475](../reference/xppaut-8.0/autlib1.c#L6475)
([master](../reference/xppaut-master/autlib1.c#L6472)). The unconditional
`my_ev` store cannot establish that the spectrum belongs to this solution.
This is established by the source paths; a native
XPPAUT reproduction of this branch-switch failure was not run for this card.

[W341 / #402](https://github.com/MuhammadMoustafa/xppautX/issues/402) retains
stability with each predictor's saved solution in the existing stability owner,
and restores the pair on failure. If an equilibrium switch returns to a branch
point absent from that record, it recomputes eigenvalues from the restored
Jacobian using the existing routine. An uncached periodic solution gets a fresh
zero-step circuit pencil at the restored solution, without applying Newton
increments. Its **full** generalized eigenvalues are used: XPPAUT's `flowkm`
assumes and deflates a multiplier near +1 at
[autlib4.c:172](../reference/xppaut-8.0/autlib4.c#L172)
([master](../reference/xppaut-master/autlib4.c#L172)). A restored orbit can
have collapsed to an equilibrium, for which the spectrum need not contain
+1; forcing that deflation can discard a complex pair. The independent
rotation-pencil test has exact multipliers +/-0.75i and guards this case.
`flowkm` also mutates its matrix inputs, so reusing a pencil after its
stability test would be wrong. Regular continuation's numerical path is
unchanged. `test_auto_stability` forces DSMIN failures
in `solvae`, `swprc` and `stepbv`, checks the restored solution and exact retained
spectrum, checks absent-record equilibrium cases against the known scalar
Jacobian, and checks an absent-record BVP endpoint against a fresh reference
pencil at its restored solution (with a state-dependent Jacobian).

## 52. AUTO omits unselected branch parameters

XPPAUT 8.0's fixed eight-entry mapping is declared at
[auto_nox.c:191-192](../reference/xppaut-8.0/auto_nox.c#L191)
([master](../reference/xppaut-master/auto_nox.c#L186)) and populated
by the Parameter form at
[auto_nox.c:740-766](../reference/xppaut-8.0/auto_nox.c#L740)
([master](../reference/xppaut-master/auto_nox.c#L736)).
A diagram point stores only these eight values at
[diagram.c:119-121](../reference/xppaut-8.0/diagram.c#L119)
([master](../reference/xppaut-master/diagram.c#L119)).
No branch record carries the other model parameters. Thus changing an
unselected constant between two computations loses the older branch's
constant: its recorded states and continued values cannot determine the
omitted value. A later grab/restart or save/reopen necessarily uses the
current constant. Fresh runs also open an empty restart stream at
[gogoauto.c:35-40](../reference/xppaut-8.0/gogoauto.c#L35)
([master](../reference/xppaut-master/gogoauto.c#L35)), while `newlab`
finds the next label and branch number from that stream at
[autlib1.c:4384-4457](../reference/xppaut-8.0/autlib1.c#L4384)
([master](../reference/xppaut-master/autlib1.c#L4381)).
Retaining two fresh branches can therefore duplicate label numbers and
make an older label read the newer solution. W339 includes the retained
diagram when allocating new labels. In addition, declining the reset in
[auto_nox.c:2164-2193](../reference/xppaut-8.0/auto_nox.c#L2164) leaves
the old diagram while still requesting overwrite of its solution files
([master](../reference/xppaut-master/auto_nox.c#L2146)). W339 appends the
new solutions whenever older branches are retained. This is source-confirmed; a native XPPAUT reproduction
was not run for this card.

[W339 / #400](https://github.com/MuhammadMoustafa/xppautX/issues/400)
removes the selection and records every model parameter once per branch.
The regression uses `x'=b-a*x`: it continues `a` with `b=2`, computes a
second branch with `b=7`, then changes `b` again before grab and label
restart. It checks that the older branch restores `b=2` and `a*x=2`,
before and after saving and reopening the session. Continued values,
states and spectra are compared against the previous per-model AUTO
baselines before accepting the storage layout change.


## 53. AUTO import is unchecked and partial

XPPAUT 8.0 reads selected parameter indices, the mark count and numeric settings
straight into live state without checking `fscanf` results
([auto_nox.c:2787](../reference/xppaut-8.0/auto_nox.c#L2787),
[master:2758](../reference/xppaut-master/auto_nox.c#L2758)). Its graph reader does
likewise ([auto_nox.c:2817](../reference/xppaut-8.0/auto_nox.c#L2817),
[master:2789](../reference/xppaut-master/auto_nox.c#L2789)). Loading resets the
existing diagram before checking the file
([auto_nox.c:2882](../reference/xppaut-8.0/auto_nox.c#L2882),
[master:2853](../reference/xppaut-master/auto_nox.c#L2853)). The diagram reader
trusts the point indices and every record read, appending immediately
([diagram.c:625](../reference/xppaut-8.0/diagram.c#L625),
[master:624](../reference/xppaut-master/diagram.c#L624)); the restart tail is
copied unchecked ([auto_nox.c:2856](../reference/xppaut-8.0/auto_nox.c#L2856),
[master:2827](../reference/xppaut-master/auto_nox.c#L2827)). A truncated record can
therefore reuse earlier values or fail to terminate, while changing settings
and destroying the prior diagram. This is source evidence, not a dynamic
exploit claim against XPPAUT.

[W327](https://github.com/MuhammadMoustafa/xppautX/issues/388) reads the foreign
file into an owned temporary value, bounds its counts and expanded storage,
uses the shared settings, point and restart validators, then writes the restart
file before replacing session state. `test_session_auto` and autocheck's
`sessiondiagram` mutate `tools/models/lecar_diagram.auto`: excessive marks,
invalid model/point parameters, an invalid axis, a truncated second point and
a malformed restart tail must each fail with their original file and line,
leaving every saved AUTO member unchanged.

## 54. An extra plot window keeps the first window's time flag

XPPAUT 8.0's `set_extra_graphs` makes each extra window of a multi-window model with `create_a_pop()`, which copies the active graph, its `TimeFlag` included ([graphics.c:584](../reference/xppaut-8.0/graphics.c#L584); the copy is in `copy_graph`, [graphics.c:756](../reference/xppaut-8.0/graphics.c#L756)), then sets the window's variables, grtype and bounds ([graphics.c:585-592](../reference/xppaut-8.0/graphics.c#L585-L592)). Nothing recomputes `TimeFlag` from the new axes. A window that plots t against a variable therefore keeps the first window's flag. The equilibrium marks (`eq_symb`) skip a window whose flag is set, so a phase plane made from a time-plot first window gets no equilibrium mark, and a time plot made from a phase-plane first window gets one.

- **Evidence:** from the source, not run here (XPPAUT's window needs an X server). A model whose first window plots `t` against `x` and whose second plots `x` against `y` leaves the second window with the time flag that `create_a_pop` copied; the same happens to a third window plotting `t` against `y` after two phase planes.
- **xppautX:** the flags of a graph come from one function, `check_flags(GRAPH &)`, which `set_extra_graphs` calls on each extra window after its axes are set. `tests/test_headless_graphs.cpp` checks each window's flag and that an equilibrium mark lands only in the phase plane.
- **Card:** [W351](https://github.com/MuhammadMoustafa/xppautX/issues/412).

## 55. AUTO marks have a nine-value form limit

XPPAUT 8.0's User form offers only counts zero through nine
([auto_nox.c:702](../reference/xppaut-8.0/auto_nox.c#L702)), with nine
named rows ([auto_nox.c:706](../reference/xppaut-8.0/auto_nox.c#L706)).
It writes the count before the value dialog succeeds
([auto_nox.c:713](../reference/xppaut-8.0/auto_nox.c#L713)), then parses
values with `atof` ([auto_nox.c:730](../reference/xppaut-8.0/auto_nox.c#L730)).
W331 ([#392](https://github.com/MuhammadMoustafa/xppautX/issues/392)) replaces
fixed mark storage with a Session-owned vector and validates the whole edit
before applying it. Tests exercise 128 marks through form, protocol, solver
workspaces and session roundtrip, and reject a bad last value atomically.
## 56. AUTO equilibrium period depends on spectrum order

XPPAUT 8.0's AUTO2000 sorts the Jacobian's eigenvalues by descending real
part, then keeps the last complex eigenvalue whose real part is nearest
zero (`ar <= arev`) and stores `par[10] = 2*pi/|imag|` from it
([autlib1.c:1917-1949](../reference/xppaut-8.0/autlib1.c#L1917)). When two
pairs are equally near the axis (equally damped modes, or a conservative
system's purely imaginary pairs), which one is last depends on the order
the eigenvalue routine returned them in, so the stored period does too.

- **Evidence:** from the source; no XPPAUT run (its AUTO needs an X
  server). W358's EISPACK and LAPACK builds of xppautX, which had the same
  selection, stored periods up to 3.06 relative apart on `tstauto_pen`,
  with identical parameters and norms
  ([study](studies/w358-auto-profile.md#3-linalglapack-lapack-against-eispack)).
  Its two oscillatory modes both have real part `-eps/2`.
- **xppautX:** one owner, `auto_hopf_pair` (core/auto_stability.cpp),
  picks the pair whatever the order: AUTO-07p's conservative test and
  near-axis band (equilibrium.f90:437-453, 1e-5), real parts equal within
  `n*eps` of the spectrum's scale treated as tied, and a tie resolved by
  the lowest frequency, as AUTO-07p's sort does inside its band. The same
  pair gives the Hopf test and the period. `tstauto_pen` now stores the
  slow mode's period at every point (it matches the analytic period to
  7e-10 relative); unit tests permute every order of two pairs.
- **Card:** [W369](https://github.com/MuhammadMoustafa/xppautX/issues/431).

## 57. Fixed C arrays sized by input

Formula compilation and several numerical methods write into fixed-size arrays with no bound
check, so input larger than the array (a long formula, many variables, states or reactions)
writes or reads past its end:
- `add_expr` compiles into its caller's `int *` with no size, and its own stacks are fixed
  (`tokstak[500]`, `loopstk[100]`: [parserslow2.c:995](../reference/xppaut-8.0/parserslow2.c#L995),
  [master 994](../reference/xppaut-master/parserslow2.c#L994)); callers pass arrays far smaller than
  `MAXEXPLEN`: `command[200]` for a table's formula ([tabular.c:321](../reference/xppaut-8.0/tabular.c#L321),
  [master 321](../reference/xppaut-master/tabular.c#L321)), `command[256]`
  ([histogram.c:295](../reference/xppaut-8.0/histogram.c#L295), [flags.c:197](../reference/xppaut-8.0/flags.c#L197)),
  `com[256]` ([markov.c:341](../reference/xppaut-8.0/markov.c#L341), [master 341](../reference/xppaut-master/markov.c#L341)),
  `f[256]` ([dae_fun.c:107](../reference/xppaut-8.0/dae_fun.c#L107)), `com[200]`
  ([browse.c:381](../reference/xppaut-8.0/browse.c#L381)), `com[400]` ([calc.c:234](../reference/xppaut-8.0/calc.c#L234)).
- [stiff.c:161](../reference/xppaut-8.0/stiff.c#L161) ([master 161](../reference/xppaut-master/stiff.c#L161)) fixed pivot array `indx[700]` overflowed when integrating a stiff ODE system with more than 700 variables.
- [do_fit.c:601](../reference/xppaut-8.0/do_fit.c#L601) ([master 601](../reference/xppaut-master/do_fit.c#L601)) Marquardt-Levenberg parameter fitting used `ipivot[1000]`, overflowing on >1000 fitted parameters.
- [markov.c:109](../reference/xppaut-8.0/markov.c#L109) ([master 109](../reference/xppaut-master/markov.c#L109)) fills `st[50]`, read past its end for a Markov chain of more than 50 states; [markov.c:461](../reference/xppaut-8.0/markov.c#L461) ([master 461](../reference/xppaut-master/markov.c#L461)) holds a Gillespie step's rates in `r[1000]`, and its loop reads `r[nrxn]`, one past the last rate it set; [xAuto.h:7](../reference/xppaut-8.0/xAuto.h#L7) ([master 7](../reference/xppaut-master/xAuto.h#L7)) used `icp[100]`.
- [delay_handle.c:134](../reference/xppaut-8.0/delay_handle.c#L134) ([master 134](../reference/xppaut-master/delay_handle.c#L134)) `polint` uses `c[10]`, `d[10]` for any point count it is given; its one caller passes four points, so this one is latent.

- **XPPAUT 8.0:** cited above in `reference/xppaut-8.0` and `reference/xppaut-master`.
- **Evidence:** xppautX's build of 2026-10-15, before W372, still had these arrays: `table w % 11 0 10 0.001+...` with 120 terms ended in a segmentation fault, a right-hand side of 1500 terms exited with no output, and 20 terms ran. XPPAUT itself was not run; its source above has the same arrays.
- **xppautX:** `add_expr` compiles into a `std::vector<int>` and its stacks grow with the formula; the stiff solver's and the fit's pivots come from `size_pivots(n)`; Markov states, Gillespie rates and AUTO's free parameters are vectors sized from the model; `polint` takes exactly four points by type (`std::span<const double, polint_points>`). tests/test_parser.cpp compiles a 2000-term formula, a 300-term table formula and a 60-state chain; the stiff solver of more than 700 variables is not run by a test.
- **Card:** [W372 / #434](https://github.com/MuhammadMoustafa/xppautX/issues/434).

## 58. Negative Total: help and storage-full overwrite

XPPAUT's [numerics help:41](../reference/xppaut-master/help/xppnumerics.html#L41)
says a negative Total "will be made positive and no data will be stored".
The code stores data regardless, and the run still ends at the absolute Total:
[integrate.c:2401](../reference/xppaut-8.0/integrate.c#L2401) tests `icount>=nit`.
FOREVER is read only by [stor_full:2725-2751](../reference/xppaut-8.0/integrate.c#L2725):
when allocation fails, [2740](../reference/xppaut-8.0/integrate.c#L2740) bypasses
the overwrite question and [2747](../reference/xppaut-8.0/integrate.c#L2747)
resets the row index to zero. New samples overwrite the first rows, leaving
newest samples before older surviving samples, out of time order. In silent
mode, [2736-2738](../reference/xppaut-8.0/integrate.c#L2736) instead stops on a
full store and ignores FOREVER. [master:2696](../reference/xppaut-master/integrate.c#L2696)
has the same storage-full branches.

xppautX requires a positive Total in its files and commands; XPPAUT `.ode` and
`.set` import maps a negative Total to its absolute value. Failed storage growth
stops every mode with one placed error and preserves the existing rows, with
no overwrite question. servercheck covers failed growth and preserved rows,
negative Total refusal with its line, and finite absolute-duration import.
[W347 / #408](https://github.com/MuhammadMoustafa/xppautX/issues/408).

## 59. Dormand-Prince restarted at every output point

XPPAUT calls `dopri5`/`dop853` once per stored point, from `t` to `tout = t + dt`
([dormpri.c:62-82](../reference/xppaut-8.0/dormpri.c#L62),
[master 62](../reference/xppaut-master/dormpri.c#L62)), from the integration
loop's `solver(..., dt, ...)` ([integrate.c:1770](../reference/xppaut-8.0/integrate.c#L1770)).
Each call ends exactly on the output time, evaluates the right-hand side
again at its start, and keeps only the next step guess (`hout`); the dense
output both routines compute is never used. So no step is longer than `dt`:
the adaptive methods do a fixed-step method's work plus one evaluation per
point, and the accuracy a run gets comes from `dt`, not from `tol` (at
`tol=1e-6` lecar's error is about 1e-9, because its steps are ten times
smaller than the tolerance needs). The step limit is per call, so it renews
at every output point. XPPAUT's own help calls Dt "the output step for the
adaptivve algorithms" ([xppnumerics.html:52](../reference/xppaut-8.0/help/xppnumerics.html#L52)),
and its maximum step, DtMax ([numerics.c:253](../reference/xppaut-8.0/numerics.c#L253)),
which Gear gets ([integrate.c:1791](../reference/xppaut-8.0/integrate.c#L1791)),
never reaches DoPri: `dormprin` passes `hmax` as 0, no limit.

- **XPPAUT 8.0:** cited above.
- **Evidence:** [docs/studies/w377-dense-output.md](studies/w377-dense-output.md):
  nine models, DoPri5 and DoPri8 as now against one continuous run sampled
  from the dense output: 18.3-99.7% fewer right-hand-side calls at equal
  tolerance (lecar 4,201 to 326, PY_S1Bf 1.4 million to 154 thousand),
  with the error the tolerance asks for instead of the error `dt` forces.
- **xppautX:** DoPri5 and DoPri8(3) retain the step, FSAL derivative,
  controller and counters across output times, sample the dense polynomial,
  and pass DtMax as the maximum internal step. Flags, sections and extrema
  use bracketed dense roots, with resets invalidating the step remainder.
  Delay models stop at their uniform history updates. The run-wide 100,000
  attempt budget reports an error instead of shortening a successful run.
  See the study's stage 2 section for errors, counts and event limitations.
- **Card:** [W377 / #439](https://github.com/MuhammadMoustafa/xppautX/issues/439).

## 60. Input-sized writes and size arithmetic

Source inspection of XPPAUT 8.0 confirms all four patterns; XPPAUT itself
was not executed for these probes:

- Equilibrium sweeps write `storage[0][storind]` and increment without
  testing capacity ([integrate.c:745](../reference/xppaut-8.0/integrate.c#L745));
  shooting sweeps do the same ([pp_shoot.c:257](../reference/xppaut-8.0/pp_shoot.c#L257)).
  The Monte Carlo equilibrium browser copy also writes unchecked rows
  ([integrate.c:675](../reference/xppaut-8.0/integrate.c#L675)).
- `PUSH` writes without checking the stack pointer
  ([parserslow2.c:29](../reference/xppaut-8.0/parserslow2.c#L29)), into the
  200-double value and argument arrays
  ([parserslow2.c:87](../reference/xppaut-8.0/parserslow2.c#L87)); argument
  writes are unchecked too ([parserslow2.c:2268](../reference/xppaut-8.0/parserslow2.c#L2268)).
- History allocation casts `big/fabs(DELTA_T)` to int before adding one
  ([delay_handle.c:69](../reference/xppaut-8.0/delay_handle.c#L69)); history
  initialization repeats the cast ([delay_handle.c:213](../reference/xppaut-8.0/delay_handle.c#L213)).
- AUTO's work allocations multiply dimensions inside parentheses before
  multiplying by `sizeof(double)`
  ([autlib2.c:239](../reference/xppaut-8.0/autlib2.c#L239),
  [autlib2.c:250](../reference/xppaut-8.0/autlib2.c#L250)). Its `integer`
  is long ([auto_f2c.h:13](../reference/xppaut-8.0/auto_f2c.h#L13)), 32 bits
  on Windows even in a 64-bit process; linear index products use that type
  ([auto_f2c.h:39](../reference/xppaut-8.0/auto_f2c.h#L39)).

The master snapshot retains the same patterns:
[equilibrium rows:739](../reference/xppaut-master/integrate.c#L739),
[shooting rows:257](../reference/xppaut-master/pp_shoot.c#L257),
[unchecked push:29](../reference/xppaut-master/parserslow2.c#L29),
[200-double stacks:87](../reference/xppaut-master/parserslow2.c#L87),
[delay count:69](../reference/xppaut-master/delay_handle.c#L69), and
[AUTO allocation products:239](../reference/xppaut-master/autlib2.c#L239).

xppautX reuses `stor_full` for both sweeps, sizes evaluator stacks before
evaluation, checks history sizes and allocation failures with a model
place, and checks AUTO allocation extents in size_t with pointer-width
indices. `tests/test_input_bounds.cpp` exercises growth and forced failure
for both sweeps, a 301-deep expression, an unrepresentable delay/dt ratio,
and AUTO arithmetic beyond 2^31 without a multi-gigabyte allocation.
[W386 / #448](https://github.com/MuhammadMoustafa/xppautX/issues/448).

## 61. Wiener noise is silently zero or has wrong variance in adaptive methods

XPPAUT calls `set_wieners` ([markov.c:94](../reference/xppaut-8.0/markov.c#L94), [master 94](../reference/xppaut-master/markov.c#L94)) only in the fixed-step methods (Euler, Mod. Euler, Runge-Kutta, Adams, Backward Euler, Symplectic: [odesol2.c:130, 181, 222, 234, 266, 421](../reference/xppaut-8.0/odesol2.c#L130), [master 130, 181, 222, 234, 266, 421](../reference/xppaut-master/odesol2.c#L130); Volterra: [volterra2.c:342](../reference/xppaut-8.0/volterra2.c#L342), [master 342](../reference/xppaut-master/volterra2.c#L342)) and in `adaptive_step` ([stiff.c:115](../reference/xppaut-8.0/stiff.c#L115), [master 115](../reference/xppaut-master/stiff.c#L115)).

In adaptive methods DoPri5, DoPri8(3), CVODE, Rosenbrock and Gear, `set_wieners` is never called at all: the Wiener parameters remain at their initial value (0) throughout the run, so noise is silently zero. In Stiff and QualRK, `set_wieners` is called once per output interval at `stiff.c:115` scaled by `*hguess` (the step size guess rather than the actual step), failing to draw noise for each of the intermediate adaptive steps and producing incorrect noise variance.

- **XPPAUT 8.0:** cited above.
- **Evidence:** `wiener w; x'=w`, total 10: Euler and RK4 end around -1.30 (noise present); DoPri5, CVODE and Gear end at 0.0 (noise silently zero); Stiff ends around -1.05 and QualRK around -0.55 (one draw per output interval scaled by step guess).
- **xppautX:** a method that does not draw the noise once per step is refused in the core (`pick_method`/`check_method`, governed by trait `wiener` in `SolverTraits`); the window's numerics menu shows incompatible methods disabled with the reason, and `--server`, `--silent`, `--convert`, `.set` import or `@ meth=` show the error at its place.
- **Card:** [W379 / #441](https://github.com/MuhammadMoustafa/xppautX/issues/441).

## 62. Some long computations cannot be stopped

Esc (XPPAUT's `my_abort()`) is polled by the integrator between output points
and by AUTO, but not by several computations whose length the user sets, so
once started they run to the end:

- Nullclines: the grid of `do_cline`
  ([nullcline.c:1048](../reference/xppaut-8.0/nullcline.c#L1048); master
  [nullcline.c:1046](../reference/xppaut-master/nullcline.c#L1046)), $(n+1)^2$ evaluations for an
  n-point mesh, and the range of nullclines
  ([nullcline.c:165](../reference/xppaut-8.0/nullcline.c#L165); master
  [nullcline.c:164](../reference/xppaut-master/nullcline.c#L164)).
- The flow: the check after each trajectory is commented out, so an Esc that
  ends one trajectory starts the next
  ([nullcline.c:686](../reference/xppaut-8.0/nullcline.c#L686); master
  [nullcline.c:684](../reference/xppaut-master/nullcline.c#L684)).
- Fitting: the Levenberg-Marquardt loop
  ([do_fit.c:487](../reference/xppaut-8.0/do_fit.c#L487); master
  [do_fit.c:487](../reference/xppaut-master/do_fit.c#L487)).
- The histogram of pairwise differences, $N^2$ for N stored rows
  ([histogram.c:365](../reference/xppaut-8.0/histogram.c#L365); master
  [histogram.c:347](../reference/xppaut-master/histogram.c#L347)).
- A fixed-step method's chunk of `NJMP` steps between output points
  ([odesol2.c:284](../reference/xppaut-8.0/odesol2.c#L284); master
  [odesol2.c:284](../reference/xppaut-master/odesol2.c#L284)), and the delay history before a run
  ([delay_handle.c:238](../reference/xppaut-8.0/delay_handle.c#L238); master
  [delay_handle.c:238](../reference/xppaut-master/delay_handle.c#L238)).

xppautX checks Stop in each of these, keeping what was computed
([W388 / #450](https://github.com/MuhammadMoustafa/xppautX/issues/450)). Esc skipping a single point of an equilibrium or
shooting sweep, which XPPAUT does by design, is not counted here: in xppautX
Stop ends the sweep.

## 63. Recursive user functions segfault at runtime

XPPAUT registers a user function symbol before compiling its formula ([parserslow2.c:677, 774](../reference/xppaut-8.0/parserslow2.c#L677), [master 677, 774](../reference/xppaut-master/parserslow2.c#L677)), so self-referential (`f(x)=f(x)+1`) and mutually recursive (`f(x)=g(x)`, `g(x)=f(x)`) functions parse and load without error or warning. During evaluation, `eval_rpn` case `UFUNTYPE` ([parserslow2.c:2264-2273](../reference/xppaut-8.0/parserslow2.c#L2264), [master 2163-2172](../reference/xppaut-master/parserslow2.c#L2163)) evaluates user functions by recursively calling `eval_rpn(ufun[in])` on the C call stack with no recursion depth tracking or cycle guard. Evaluating a recursive user function without termination overflows the C call stack and crashes with a segmentation fault (SIGSEGV / exit 139).

xppautX permits recursion (such as `test.odex`'s factorial), but bounds the evaluator's call depth (`MAX_UFUN_DEPTH`). When a recursive user function exceeds the bound, evaluation stops with an `xpp::Error` naming the function's model file and line and the line as written (`model_place`), and the run preserves rows stored up to that point.
- **Card:** [W387 / #449](https://github.com/MuhammadMoustafa/xppautX/issues/449).

## 64. AUTO point numbers wrap to zero

XPPAUT 8.0's AUTO2000 uses `(ntot + 1) % 10000` for the attempted point
in diagnostics ([autlib1.c:1402](../reference/xppaut-8.0/autlib1.c#L1402))
and `ntot % 10000` for the stored point in fort.7
([autlib1.c:2967](../reference/xppaut-8.0/autlib1.c#L2967)) and the
algebraic and collocation fort.8 headers
([autlib1.c:3118](../reference/xppaut-8.0/autlib1.c#L3118),
[autlib1.c:7251](../reference/xppaut-8.0/autlib1.c#L7251)).

**Evidence:** the source expressions print point 10000 as 0 and point
10001 as 1. Only the output number wraps; the continuation count and
special-point labels continue. AUTO-07p has a related 9999-point wrap
([finding 3](auto07p-findings.md#3-point-numbers-wrap-after-9999)).

**xppautX:** removes the output modulo in every algebraic, collocation and
eigenvalue diagnostic path. fort.7/8/9 and logging keep full counts,
matching the existing diagram, info strip, table, protocol events and
session diagram. Explicit whitespace prevents an expanded number from
joining its neighbour; AUTO-07p's Python fort.7/8 readers accept these
whitespace-separated integers. Both fort.8 writers round-trip through the
shared bounded header reader in unit tests past 10000; signed fort.7 rows
and the next-point diagnostic arithmetic are covered directly too.

**Card:** [W384 / #446](https://github.com/MuhammadMoustafa/xppautX/issues/446).


## 65. Plot curves, frozen curves and windows have fixed limits

XPPAUT 8.0 defines 10 curves per graph (`MAXPERPLOT`), 26 frozen curves
(`MAXFRZ`) and 21 plot windows (`MAXPOP`) in
[struct.h:11](../reference/xppaut-8.0/struct.h#L11),
[struct.h:12](../reference/xppaut-8.0/struct.h#L12) and
[struct.h:13](../reference/xppaut-8.0/struct.h#L13).
Its graph's axis, line and colour arrays use that curve bound
([struct.h:61](../reference/xppaut-8.0/struct.h#L61)). These are storage
limits, independent of the number of model variables or available memory.

xppautX uses vectors for curves, frozen slots, plot windows and their
associated display records. The ICs xvst selection includes every checked
variable; pp retains its three spatial axes. Session files record empty
slots explicitly, so allocation follows actual parsed entries and deleted
slots keep their indices. A dishonest slot or point count fails before
application, with the member file and line. The common core creates and
removes windows in both headless and interactive modes.

Regression coverage in `tests/test_snapx.cpp` constructs 13 curves,
32 frozen slots and 107 window slots, including holes, and compares the
saved members after snapshot and session reload; protocol and page tests
exercise the corresponding data paths. Execution results are reported with
the card review.

**Card:** [W376 / #438](https://github.com/MuhammadMoustafa/xppautX/issues/438).

## 66. AUTO map fold continuation loses the map problem

XPPAUT 8.0's Run handling sets `Auto.ips=1` before continuing any algebraic fold
([auto_nox.c:1966](../reference/xppaut-8.0/auto_nox.c#L1966)).
`auto_2p_limit` initializes `ipsuse=1` and only distinguishes BVP and periodic folds
([auto_nox.c:2498](../reference/xppaut-8.0/auto_nox.c#L2498)), so a discrete model's
fixed-point equation `f(x)-x=0` becomes the flow equation `f(x)=0`.
This is a change of equations, not just a label. The Henon-with-rotation fold
`alpha=-(1+beta)^2/4` is a concrete check: xppautX inherited the path and stopped
at its second point with a nonfinite solution. W404 preserves the map problem
through the common core run setup, for all front ends.

**Card:** [W404 / #466](https://github.com/MuhammadMoustafa/xppautX/issues/466).

## 67. The torus integral condition is identically zero

XPPAUT 8.0's continuation of torus bifurcations has three integral conditions. The second, meant to fix the
phase of the complex eigenfunction `u + i v` against the previous point's, is computed from the current
point twice: `f[1] += u[ndm+i]*u[ndm2+i] - u[ndm2+i]*u[ndm+i]`
([autlib3.c:3252](../reference/xppaut-8.0/autlib3.c#L3252)), which is zero for every input, and its
Jacobian row `u[ndm2+i]`, `-u[ndm+i]` is the derivative of a function that is not there. AUTO-07p's ICTR
multiplies by the previous point's eigenfunction (`U(NDM+I)*UOLD(NDM2+I) - U(NDM2+I)*UOLD(NDM+I)`), which
is what makes the condition fix the phase. XPPAUT's system therefore has one condition fewer than it
claims: the eigenfunction's phase is left to the pseudo-arclength equation alone.

On the shipped references the change is small (forced_pp_ns and hill_cycles_torus differ from the old
run at the 1e-9 level, still agreeing with AUTO-07p), because the tangent already holds the phase in smooth
steps. tests/test_auto_codim.cpp checks the condition directly: a rotated eigenfunction now gives a nonzero
value, and its Jacobian row matches a central difference.

**Card:** [W407 / #469](https://github.com/MuhammadMoustafa/xppautX/issues/469).

## 68. HomCont differentiates a changing projection basis

XPPAUT 8.0's `bcho` differentiates the boundary conditions by perturbing each
state and free parameter and calling `fbho` again
([autlib5.c:325](../reference/xppaut-8.0/autlib5.c#L325),
[master](../reference/xppaut-master/autlib5.c#L325)), and every projection
`prjctn` computes replaces the previous Beyn basis with the one it returns
([autlib5.c:2080](../reference/xppaut-8.0/autlib5.c#L2080),
[master](../reference/xppaut-master/autlib5.c#L2080)). So each perturbed
evaluation is normalised against the previous perturbed one, and the
difference quotients differentiate a boundary function that changes with
them, not the one whose residual they are compared with. AUTO-07p 0.9.4's
BCHO passes CSAVE=.FALSE. to its perturbed evaluations, which then use the
saved basis without replacing it (homcont.f90 PRJCTN).

W365 keeps the basis of the unperturbed evaluation only. tests/test_auto_numerics.cpp
checks, on `x' = (1+s)x + p y`, `y' = (s-1)y - 2z`, `z' = 2y + (s-1)z`, that the
projections span the analytic eigenspaces and that a perturbed evaluation
leaves the saved bases and their flags unchanged. On the measured runs the
effect is below the comparison's tolerances: the three HomCont runs agree
with AUTO-07p through finding 69's fix and the descriptions' common start.

**Card:** [W365 / #427](https://github.com/MuhammadMoustafa/xppautX/issues/427).

## 69. A homoclinic start samples its orbit with an unscaled weight

XPPAUT 8.0 starts a homoclinic continuation from the last integration, at
`t*p + HOMO_SHIFT` (`get_shifted_orbit`,
[auto_nox.c:1759](../reference/xppaut-8.0/auto_nox.c#L1759),
[master](../reference/xppaut-master/auto_nox.c#L1747)). Three defects:

- The interpolation weight between two stored rows is the time past the
  first, `lam = ts - storage[0][i]`, not divided by the step
  ([auto_nox.c:1765](../reference/xppaut-8.0/auto_nox.c#L1765),
  [master](../reference/xppaut-master/auto_nox.c#L1753)): with a step of 0.05
  every mesh point takes about the earlier row. The orbit `x = 3/2 sech^2(t/2)`
  (tools/models/explicit_homoclinic.odex at q = 1) starts with an L2 norm of
  0.8659934 in XPPAUT's sampling, reproduced to every digit by a script,
  against 0.8648322 interpolated and 0.8648999642 exactly.
- `ts` is taken modulo the period, so the orbit's last point (t = 1) is
  sampled at its first; a time on no stored interval leaves `u` unset.
- `find_best_homo_shift` measures the distance to `homo_l` before
  `get_homo_info` has read it from the form
  ([auto_nox.c:2388](../reference/xppaut-8.0/auto_nox.c#L2388) and
  [2392](../reference/xppaut-8.0/auto_nox.c#L2392),
  [master](../reference/xppaut-master/auto_nox.c#L2366)): the shift is
  found from the previous run's equilibrium.

A HomCont run's first point is its starting data, which neither AUTO
corrects, and the phase condition carries its offset along the whole
curve. xppautX now samples the shifted orbit with the interpolation every
other start uses (`get_start_orbit`) and finds the shift after the form.

**Card:** [W365 / #427](https://github.com/MuhammadMoustafa/xppautX/issues/427).

## 70. AUTO evaluates the test functions at the last Newton iterate's Jacobian

XPPAUT 8.0 stops the Newton iteration of an equilibrium point as soon as the update is below EPSL and EPSU and then
evaluates the point's test functions (`pvlsae`, which reads the eigenvalues of the Jacobian in `aa`) with the Jacobian
of the iterate *before* that last update
([autlib1.c:1504](../reference/xppaut-8.0/autlib1.c#L1504),
[master](../reference/xppaut-master/autlib1.c#L1501)). AUTO-07p 0.9.4 recomputes the Jacobian at the converged point
first (`SOLVAE`, ae.f90, "Recompute Jacobian for test functions"). A test function is then off by about its slope
times the last update, and the location of a special point by the secant method (`lcspae`) sees a function that is
not the same each time the point is solved.

Evidence, the Hopf point of the PV interneuron of Ma et al. 2023 (`examples/ode/ma23.odex`, the steady run of
`tests/auto/ode_ma23.json`: SmStmplti from 0 at DS 0.005, DSMAX 2, NTST 25, EPS 1e-7/1e-7/1e-5, ITMX 8, which
`uv run --locked python tools/auto_reference.py --program xppaut --only ode_ma23` sets in XPPAUT's windows): the table lists EP, LP
(35.05591 pA) and LP (34.01230) and no HB. XPPAUT's diagnostics file shows the Hopf function change sign between
points 297 (7.8e-5, three stable eigenvalues) and 298 (-8.0e-4, five); its location then evaluates 3.3e-7, -2.9e-7,
-1.4e-7, -7.2e-8, -4.4e-8, -2.5e-8, -1.4e-8, -1.1e-8, -3.6e-9, approaching zero from one side at about 0.6 per
evaluation, uses up ITMX and prints `NOTE:Possible special point` (the exit at
[autlib1.c:1678](../reference/xppaut-8.0/autlib1.c#L1678), [master](../reference/xppaut-master/autlib1.c#L1677)),
so the point is not labelled and the periodic orbit cannot be started from it with the keys of
`tools/auto_reference.py`. AUTO-07p labels it at 553.8767 pA (period 7.932) and xppautX does too. That the missing
re-evaluation is the whole cause was not isolated by an experiment on XPPAUT. This is the model as exported, with
E_Ca 130 mV; with Table 1's 68 mV (W424, [#486](https://github.com/MuhammadMoustafa/xppautX/issues/486)) the Hopf
point moves to 539.2246 pA, and XPPAUT 8.0 labels it there.

xppautX inherits the call (`core/autlib1.cpp` `solvae`): it locates this point, but its location of a flat Hopf
function depends on the step. The real part of the Hopf pair of Mu26's control condition moves 6e-4 per pA at its
Hopf point; with DSMAX 0.3, 0.4, 0.5 and 0.7 xppautX's point is 5e-5 to 1.3e-4 pA from the root (its Hopf function
there -7.9e-8, the last update's size), with 0.6, 0.8 and 1 within 2e-7, while AUTO-07p's is within 1.5e-7 of
13.2654735 at every DSMAX and its function is 8e-13 there (docs/auto-references.md, "W415").

xppautX now follows AUTO-07p (W421): `solvae` recomputes the Jacobian at the converged point, and the fold, Hopf,
branch-point data and codimension-two tests read that one; none computes its own, and the Hopf test's eigenvalues
are taken from a copy, so the matrix stays the converged point's for the tests after it. Mu26's HB is then within
9.8e-8 pA of itself at DSMAX 0.3 to 1 (13.26547344 to 13.26547354) and Ma23's within 4e-6 pA at DSMAX 0.5 to 3,
where it moved 2.5e-5 (docs/auto-references.md, "W421").

**Card:** [W415 / #477](https://github.com/MuhammadMoustafa/xppautX/issues/477) found it;
[W421 / #483](https://github.com/MuhammadMoustafa/xppautX/issues/483) fixed it.

## 71. AUTO two-parameter map problems and disabled detection

XPPAUT 8.0 offers Two Param for equilibrium Hopf points, cycle period doublings
and tori; grabbing an LP and choosing Run starts its curve directly. These are
real menu paths, not missing functionality in the reference driver:
[auto_nox.c:1835](../reference/xppaut-8.0/auto_nox.c#L1835),
[auto_nox.c:1860](../reference/xppaut-8.0/auto_nox.c#L1860),
[auto_nox.c:1903](../reference/xppaut-8.0/auto_nox.c#L1903) and
[auto_nox.c:1966](../reference/xppaut-8.0/auto_nox.c#L1966).

The map LP problem is already recorded as [finding 66](#66-auto-map-fold-continuation-loses-the-map-problem):
`henon_rot_lp` remains refused on that basis. For a map's PD, `auto_twopar_double`
also chooses the continuous periodic-orbit problem IPS 2 rather than map IPS -1:
[auto_nox.c:2536-2549](../reference/xppaut-8.0/auto_nox.c#L2536)
([master:2508](../reference/xppaut-master/auto_nox.c#L2508)). However, that PD menu is not the map flip's
native route: the period-two HB path works with IPS -1, as recorded in `henon_rot_pd.xppaut`. The map Hopf path does preserve
IPS -1 ([auto_nox.c:2638](../reference/xppaut-8.0/auto_nox.c#L2638)); a map's
torus is shown as HB by the equilibrium/map detector.
The map flip is also called HB (unit-circle crossings use the Hopf detector,
[autlib1.c:1092-1107](../reference/xppaut-8.0/autlib1.c#L1092)), so a PD
label does not offer the PD menu in the first place.
That detector takes the logarithms of the map multipliers and calls every
nonzero imaginary part a Hopf candidate
([autlib1.c:1901-1910](../reference/xppaut-8.0/autlib1.c#L1901),
[autlib1.c:1933-1949](../reference/xppaut-8.0/autlib1.c#L1933)). Thus a real
flip has stored period 2 (`log(-1)=i*pi`); the driver translates that HB to
PD, and a complex unit-circle HB to TR, when importing new map references.
The map Hopf equations also admit the flip at angle pi
([autlib3.c:734-789](../reference/xppaut-8.0/autlib3.c#L734),
[master:716](../reference/xppaut-master/autlib3.c#L716)); W428 drives
that offered HB path for `henon_rot_pd`, retaining the map problem, rather
than treating the unrelated PD menu as the only possible route.
The actual labelled point is grabbed by its diagram position, and IRS is
checked against its label.

The two-parameter routines set ISP 0 and ILP 0:
[auto_nox.c:2510-2512](../reference/xppaut-8.0/auto_nox.c#L2510),
[auto_nox.c:2546-2548](../reference/xppaut-8.0/auto_nox.c#L2546),
[auto_nox.c:2561-2563](../reference/xppaut-8.0/auto_nox.c#L2561), and
[auto_nox.c:2634-2636](../reference/xppaut-8.0/auto_nox.c#L2634)
([master:2470](../reference/xppaut-master/auto_nox.c#L2470),
[master:2606](../reference/xppaut-master/auto_nox.c#L2606)).
Thus their curves do not label the codimension-two points that xppautX and
AUTO-07p detect. W428 keeps those missing labels as explicit traced differences,
while comparing the recorded curves and ordinary source-run special points.
No numerical tolerance is increased.

XPPAUT's curve export also limits precision to six significant digits:
[autlib1.c:3035-3041](../reference/xppaut-8.0/autlib1.c#L3035)
([master:3032](../reference/xppaut-master/autlib1.c#L3032)).
For example, `hill_cycles_pd`'s constant norm 0.41826665768 is exported as
0.418267. The file's rounding error alone exceeds the description's EPSU
1e-7 bound. W428 accounts for half the last printed unit in each coordinate;
it does not relax the Newton tolerance or alter XPPAUT's output.

Evidence: `uv run --locked python tools/auto_reference.py --program xppaut --only
 tstauto_ab_fold,tstauto_ab_hopf,zero_hopf,ode_mu26_blocked` and the committed
`.xppaut` files. The run constants are read back from each `.b` header,
including IPS, IRS, ISW, ISP, ILP and the active parameter slots.
xppautX uses the map problem and detects the codimension-two points (W364/W402).

**Card:** [W428 / #490](https://github.com/MuhammadMoustafa/xppautX/issues/490).

## 72. AUTO's branch point test reads the fold test's determinant

On an equilibrium branch XPPAUT 8.0 evaluates the fold test before the branch point test
([autlib1.c:1051](../reference/xppaut-8.0/autlib1.c#L1051), [1069](../reference/xppaut-8.0/autlib1.c#L1069)).
The fold test solves the Jacobian bordered with the *tangent* of the point before (`udot`, `rldot`) and stores that
matrix's determinant in `rap->det` ([autlib1.c:1828](../reference/xppaut-8.0/autlib1.c#L1828),
[master](../reference/xppaut-master/autlib1.c#L1825)); the branch point test returns `rap->det`
([autlib1.c:1752](../reference/xppaut-8.0/autlib1.c#L1752), [master](../reference/xppaut-master/autlib1.c#L1749)),
so it reads the fold test's matrix and not the Newton matrix of the step, whose last row is the arclength condition,
which `solvae` stored. The tangent row's determinant changes sign whenever a step turns past a fold: the tangent of
the point before and the one after point opposite ways. AUTO-07p keeps them apart: its `FNLPAE` leaves `AP%DET`
alone, with the comment "don't store DET here: it is for a different matrix than used with pseudo arclength
continuation and sometimes has a different sign"
([toolboxae.f90:733](../reference/auto-07p/src/toolboxae.f90#L733)); `FNBPAE` reads the determinant `SOLVAE`
stored ([ae.f90:548](../reference/auto-07p/src/ae.f90#L548)). The location iterations of a branch point then
mix the two: its first value is the fold matrix's, each iterate's the Newton matrix's.

Evidence, Mu26's steady branch at DSMAX 1 (`examples/ode/mu26.odex`, `tests/auto/ode_mu26_blocked.json`'s steady run
with `dsmax` 1, h current blocked; the same with the control conductances): the step from point 112 (-78.790 pA)
predicts -79.606 pA, past the fold at -79.0566, and its corrector converges at -78.849 on the branch beyond it.
The fold function is -0.6925 at 112 and -0.6781 at 113 (no sign change: the fold is not detected), the determinant
read as the branch point function 1.37e-29 and -1.69e-30 (a sign change). The location then evaluates the Newton
matrix's determinant, positive at every iterate (2.4e-29 to 2.6e-29), and still labels a BP at -78.849 pA
(-78.744 in the control condition). AUTO-07p labels neither point at DSMAX 1: its step jumps the fold the same way,
and its branch point function does not change sign.

xppautX now follows AUTO-07p (W423): `fnlpae` keeps its determinant to itself. At DSMAX 1 Mu26's steady branches
have no BP, as AUTO-07p's; the fold is still jumped, a matter of the step size (both programs label it from DSMAX
0.8 down). Two AUTO baselines change: `tstauto_pp2`'s BP at point 44 (its parameter moves 2.8e-13, its other values
by up to 5.2e-10) and `tstauto_torx_homcont`'s BP at point 8 (its states, about 2.8e-56, by 7.6e-60); both still agree with
their references.

**Card:** [W423 / #485](https://github.com/MuhammadMoustafa/xppautX/issues/485).

## 73. Periodic phase conditions do not pass through the computed orbit

XPPAUT 8.0's ICPS imposes `integral(u * uold') = 0`
([autlib3.c:1472](../reference/xppaut-8.0/autlib3.c#L1472)). The cycle fold,
period doubling, torus and periodic optimization systems repeat it at
[2448](../reference/xppaut-8.0/autlib3.c#L2448),
[2850](../reference/xppaut-8.0/autlib3.c#L2850),
[3253](../reference/xppaut-8.0/autlib3.c#L3253) and
[3756](../reference/xppaut-8.0/autlib3.c#L3756).
For an exact periodic function `integral(uold * uold') = 0`. On a collocation
mesh the stored derivative is the model right-hand side at the previous
orbit, and quadrature need not make that integral zero. Consequently the
previous computed orbit need not satisfy the next corrector's phase condition,
even when the continuation step tends to zero.

AUTO-07p centers the physical phase condition: `integral((u-uold) * uold') = 0`
([periodic.f90:303](../reference/auto-07p/src/periodic.f90#L303), and the augmented
systems at [605](../reference/auto-07p/src/periodic.f90#L605),
[998](../reference/auto-07p/src/periodic.f90#L998),
[1436](../reference/auto-07p/src/periodic.f90#L1436),
[1701](../reference/auto-07p/src/periodic.f90#L1701)). This contains the previous
orbit exactly without changing the state derivative of the phase condition.
xppautX follows that form and reuses ICPS for the augmented systems.
An independent unit case has `uold=(1,2)`, `uold'=(3,-2)`: the old integrand is
-1 at zero step, the centered integrand is exactly zero; perturbing the first
state by 0.25 gives 0.75, with derivative (3,-2). The rotation mask remains active.

The UZ test itself is just parameter minus target in both programs
([XPPAUT autlib1.c:6872](../reference/xppaut-8.0/autlib1.c#L6872),
[AUTO-07p support.f90:746](../reference/auto-07p/src/support.f90#L746)).
Their failed-search behavior differs independently: XPPAUT exits LCSPBV with
the last iterate still installed
([autlib1.c:6499](../reference/xppaut-8.0/autlib1.c#L6499)); AUTO-07p restores
its converged state
([bvp.f90:1438](../reference/auto-07p/src/bvp.f90#L1438)). W326 already supplies
that restoration in xppautX (finding 47). W437 changes the phase condition;
it does not replace or weaken the failed-location rollback.

Native XPPAUT 8.0 with the four Mu26 marks reproduces the failure. Control
labels 5 pA, reports a possible special point while locating 1 pA, labels an LP
at 0.9990257 pA and returns through 5 pA. Blocked labels 5 and 0.9999987 pA,
then LPs at 0.1009347 and 0.07001995 pA and returns to 0.08047857 pA.
Neither labels all four targets; unmarked runs approach the SNIC. The stored
native reference points retain these outcomes. With centered phase and the
existing W326 rollback, both xppautX continuations label all four targets and
have no upward current step. The UZ test, discrete phase bias and failed-search
restoration are distinct parts of this explanation.

Measured consequence on `tstauto_pp2` (the existing NTST 15 periodic description): the centered condition
locates the first cycle LP at parameter 0.7080732822986838, norm 0.4180552886559583, period 79.8935775600.
AUTO-07p's stored reference is (0.7080732823, 0.41805535218, 79.89348504), whereas XPPAUT 8.0's is
(0.708072, 0.4280795, 64.656143027). The new point agrees with AUTO-07p and fails the old XPPAUT norm and
period bounds. W437 records that discrepancy without widening a tolerance or silently dropping the XPPAUT point.
The before-update phase-impact report and the subsequent accuracy traces are in
[auto-references.md](auto-references.md#w437-user-points-on-mu26s-approach-to-the-snic-499).

The NTST 15 branches here are underresolved near a homoclinic orbit: the existing NTST 60/EPS 1e-9 study
finds no LP up to period 84.4. This demonstrates a discrepancy in discretized equations, not evidence for
physical cycle folds. The four obsolete AUTO-07p LP exceptions are removed; both LPs now remain directly compared.

**Card:** [W437 / #499](https://github.com/MuhammadMoustafa/xppautX/issues/499).

## 74. AUTO's finite-difference Jacobian steps every state by the largest one's scale

XPPAUT 8.0 runs AUTO with `JAC=0` ([autevd.c:69](../reference/xppaut-8.0/autevd.c#L69)), so `funi` asks `func` for no Jacobian
([autlib3.c:4593-4599](../reference/xppaut-8.0/autlib3.c#L4593)) and differences the right-hand side centrally with one step
for every state, `ep = HMACH*(umx + 1)`, `umx` the largest `|u[i]|` and `HMACH = 1e-7`
([autlib3.c:4609-4631](../reference/xppaut-8.0/autlib3.c#L4609), [master](../reference/xppaut-master/autlib3.c#L4610);
[auto_c.h:21](../reference/xppaut-8.0/auto_c.h#L21)). A state of 1e-5 to 1e-22 (a gating variable, a concentration) next to
a membrane voltage near -70 is stepped by 7e-6, far more than its own size, and the Jacobian, its eigenvalues and so every
Hopf test and period carry the error. AUTO-07p's `FUNI` is the same
([auto07p-findings.md](auto07p-findings.md#7-the-finite-difference-jacobian-steps-every-state-by-the-largest-ones-scale)).

Evidence: ModelDB 267710 (`examples/ode/motoneuron6c.odex`, `tests/auto/ode_motoneuron6c.json`, the steady run's Hopf point,
stored point 362). The converged point (an independent centred-difference Jacobian of the right-hand side, and xppautX's exact
Jacobian at EPS 1e-7) is at ampPar 0.1737640862 with period 608.8326; XPPAUT 8.0 labels it at 0.1737628 with period 607.215,
AUTO-07p at 0.1737626685 with 607.3187, and a centred difference with this step rule at tight tolerances at 0.1737627148
with 607.2861: the Hopf point is 1.4e-6 off and the period, which changes by about 7e5 per unit of ampPar there, 0.25
percent. XPPAUT's difference from AUTO-07p (0.1) is finding 70 (its Hopf test reads the last Newton iterate's Jacobian).
Mu26 with blocked h-current: the first fold's norm is 99.22379848 exactly and 99.22409174 by this rule (XPPAUT 8.0:
99.22409). The table is in the AUTO-07p entry.

xppautX's default `difference` Jacobian keeps the rule, so it reproduces XPPAUT's and AUTO-07p's numbers; `jacobian: exact`
(W260) gives the converged ones.

**Card:** [W455 / #518](https://github.com/MuhammadMoustafa/xppautX/issues/518).

## 75. The AUTO file name overflows past 199 characters of HOME and model name

At start-up, even with `-silent`, `do_main` calls `init_auto_win` ([main.c:468](../reference/xppaut-8.0/main.c#L468)),
whose `create_auto_file_name` writes `sprintf(this_auto_file,"%s/%s",HOME,bname)`, `HOME` the environment's (else the
model's folder) and `bname` the model file's name ([auto_nox.c:577-594](../reference/xppaut-8.0/auto_nox.c#L577)), into
`char this_auto_file[200]` ([auto_nox.c:212](../reference/xppaut-8.0/auto_nox.c#L212); the same in
[master](../reference/xppaut-master/auto_nox.c#L207)), with no bound. Together they need not be long: a 160-character home
and a 40-character model name are enough.

Evidence: W354's izhikevich study gives every run a private HOME inside its run folder. On the Ubuntu machine (XPPAUT 8.0
built with Ubuntu's default `_FORTIFY_SOURCE`) `xppaut izh_2004s_inhibition_induced_spiking.ode -silent`, with a HOME of
196 characters, ends with `*** buffer overflow detected ***: terminated` before integrating; gdb puts the abort in
`___sprintf_chk` from `create_auto_file_name` (`init_auto_win`, auto_nox.c:1555). 22 of the 28 Izhikevich models
(the 2004 ones, whose names are longer) failed; the six 2003 ones ran. A build without fortify checks writes past the array
unnoticed. The series ran from a shorter folder instead (no source of XPPAUT's is edited); xppautX has no such array.

## 76. The equilibrium range's Monte Carlo answer is read from the Movie field

Equilibria > Range's dialog has the fields parameter, steps, low, high, shoot, color column, movie and Monte Carlo, the
last two in that order. The reader takes the movie flag from `values[6]` and the Monte Carlo flag from `values[7]`, but
tests `values[6][0]` for it: `if(values[7][0]=='Y'||values[6][0]=='y')eq_range.mc=1`
([integrate.c:356](../reference/xppaut-8.0/integrate.c#L356); the same in
[master](../reference/xppaut-master/integrate.c#L351)). A capital `Y` in the Monte Carlo field works, a lower-case `y`
there does not, and a `y` in the Movie field turns Monte Carlo on. Evidence: reading the line; the Range Integrate
dialog's reader a few lines later ([integrate.c:478](../reference/xppaut-8.0/integrate.c#L478)) tests `values[7]` twice.
xppautX tests the field it reads (W482, [#546](https://github.com/MuhammadMoustafa/xppautX/issues/546)).

## 77. Rosenbrock retries a failing step forever at the smallest step

`rosen` shrinks a failed step with `absh = MAX(hmin, absh * MAX(0.1, pow(0.8*(rtol/err),1./3.)))` and gives up through
`if(absh<hmin){ *ierr=-1; return(-1); }` ([odesol2.c:588-596](../reference/xppaut-8.0/odesol2.c#L588); the same in
[master](../reference/xppaut-master/odesol2.c#L589)). The size is clamped up to `hmin` by the line above, so the test
`absh<hmin` is never true on the retry and a step that cannot succeed (a solution that blows up) loops without end.
Evidence: tools/models/stops/blowup_rosenbrock.odex never returned in xppautX before this fix. xppautX tests
`absh<=hmin` (the step has nowhere left to shrink to) and the run ends with "Step size too small" (W482, [#546](https://github.com/MuhammadMoustafa/xppautX/issues/546)).

## 78. A flag with sign 1 repeats one clause and misses an exact zero

The event test for a rising flag is `(((f0<0.0)&&(f1>0.0))||((f0<0.0)&&(f1>0.0)))&&tol>tolmin`, one clause twice
([flags.c:307](../reference/xppaut-8.0/flags.c#L307); the same in [master](../reference/xppaut-master/flags.c#L307)),
where the falling flag tests `f0>0.0&&f1<=0.0`. A condition that is exactly zero at the end of a step after being
negative is a falling event but not a rising one. xppautX tests `f0<0.0&&f1>=0.0`, the mirror of the falling case; no
example model's output changed (W482, [#546](https://github.com/MuhammadMoustafa/xppautX/issues/546)).

## 79. The stiff step that finds its matrix singular is retried with a stale step

`stiff` returns with `*ier=-1` when `sgefa` finds its matrix singular ([stiff.c:193](../reference/xppaut-8.0/stiff.c#L193);
the same in [master](../reference/xppaut-master/stiff.c#L193)), but its driver `gadaptive` ends the advance only for
`if(*ier>0)return -1;` ([stiff.c:130](../reference/xppaut-8.0/stiff.c#L130)). The singular step is not reported:
the loop tests the stale `hnext` against `hmin` (uninitialised when the first step is singular) and tries again with it,
so the solver's "singular jacobian encountered" message is never shown. Found by reading (W488,
[#552](https://github.com/MuhammadMoustafa/xppautX/issues/552)); xppautX keeps the behaviour, so a model's numbers do
not change, and names the case in a comment (stiff.cpp) for a card that decides the fix.

## 80. A network's tables are never checked for the values it reads, and fftcon's check tests the wrong variable

`conv`, `sparse`, `mmult` and the other special functions read their weight, index and delay tables through raw
pointers (`w[j+ncon]`, `cc[ij]`, `tau[ij]`: [simplenet.c:1226](../reference/xppaut-8.0/simplenet.c#L1226), the same in
[master](../reference/xppaut-master/simplenet.c#L1226)) and never test that a table holds that many values, so a table
shorter than the network reads past the end of its heap block. `fftcon` is the one that tries: it compares the table's
length with `ntot` or `2*ntot` by `if(type==FFTCONP&&ntab<ntot)` and `if(type==FFTCON0&&ntab<(2*ntot))`
([simplenet.c:635](../reference/xppaut-8.0/simplenet.c#L635), [:639](../reference/xppaut-8.0/simplenet.c#L639); the same in
[master](../reference/xppaut-master/simplenet.c#L635)), but `type` there is the keyword's code from `is_network`
([simplenet.c:336](../reference/xppaut-8.0/simplenet.c#L336)), which is 5 for every `fftcon`, the value `FFTCONP` also has
by coincidence: the periodic test applies to both kinds of ends and the zero-ends test (`FFTCON0` is 4) never does, so
an `fftcon(0, n, w, ...)` whose table holds fewer than `2n` values loads and then reads past it when the kernel is
transformed. Evidence: reading the lines (XPPAUT was not run on a short table). xppautX works out
what each network reads from its arguments and refuses a table that holds fewer values, at the network's line (W490,
[#554](https://github.com/MuhammadMoustafa/xppautX/issues/554)).

## 81. A failed AUTO set-up, a missing restart label and a short restart read pass silently

`go_go_auto` declares `function_list list;` uninitialised ([gogoauto.c:33](../reference/xppaut-8.0/gogoauto.c#L33));
`set_function_pointers`'s last branch, reached for a problem, ISW and start type it has no functions for, only prints
`Initialization Error CRASH!!` and returns ([gogoauto.c:494-499](../reference/xppaut-8.0/gogoauto.c#L494)), so
`list.type` is read uninitialised ([gogoauto.c:99-103](../reference/xppaut-8.0/gogoauto.c#L99)): neither driver runs, or
one runs with garbage pointers, and the run ends with no point and no error. A restart label missing from fort.3 prints
`Restart label not found` and returns as from a normal end ([gogoauto.c:68-75](../reference/xppaut-8.0/gogoauto.c#L68)).
The start routines read the restart data with unchecked `fscanf`s (e.g. `stpnbv`,
[autlib1.c:6004-6008](../reference/xppaut-8.0/autlib1.c#L6004)) and their callers ignore the result
([autlib1.c:5893](../reference/xppaut-8.0/autlib1.c#L5893)), so a truncated fort.3 continues from half-read data.
xppautX chooses the functions from a table and makes each of the three an error that ends the run and is shown, naming the
problem, ISW and start type, or the label, the file and what the data ended before (W491,
[#555](https://github.com/MuhammadMoustafa/xppautX/issues/555); tests/test_auto_restart.cpp truncates a restart file).

## 82. A range integration ignores every failure of its solver

`integrate` returns 1, with no error, when a solver step fails while `RANGE_FLAG` or `suppress_bounds` is set
(the adaptive and Gear/CVODE advance), and a fixed-step method that fails (backward Euler's "Too many iterates",
Volterra) is ignored altogether: the loop goes on with the state it had. The sweep's loop then ends at the first
parameter value with no row and no message, or runs to the end with unconverged values, and the only trace is
`plintf(" Errors occured in range integration \n")` in batch mode
([integrate.c:1170](../reference/xppaut-8.0/integrate.c#L1170); the same in
[master](../reference/xppaut-master/integrate.c#L1170)). Evidence: a blow-up model (`x'=a*x^2`) with Gear, CVODE and
backward Euler over `a` from 1 to 3 (W480's study, I2 and I3). xppautX stops the sweep with the solver's error, the
parameter value and the run's number (W487, [#551](https://github.com/MuhammadMoustafa/xppautX/issues/551)).

## 83. The transient time compares the absolute time

A row is stored when `fabs(MyTime)>=TRANS` ([integrate.c:904](../reference/xppaut-8.0/integrate.c#L904), 1179 and
1487; `!(fabs(*t)<TRANS)` at [:2386](../reference/xppaut-8.0/integrate.c#L2386); the same in
[master](../reference/xppaut-master/integrate.c#L904)), although the setting is described as an amount of time to
integrate before storing. For a run that starts at `T0` below 0 the rows from `T0` to 0 are stored whatever `TRANS` is
(`|t|` falls from `|T0|` to 0 and rises again), and for a backward run nothing is stored until `|t|` reaches `TRANS`.
Evidence: reading the lines. xppautX compares `t` itself in the run's direction (W487,
[#551](https://github.com/MuhammadMoustafa/xppautX/issues/551)).

## 84. The array plot's time range starts at 0 and ends at 20 by default

Print arrayplot and the array plot window set `tlo=0.0; thi=20.0` and replace only `tlo`, and only when the first
row shown is above 0 (`if(jb>0&&jb<nrows)tlo=my_browser.data[0][jb]`): [arrayplot.c:371](../reference/xppaut-8.0/arrayplot.c#L371)
and [arrayplot.c:655](../reference/xppaut-8.0/arrayplot.c#L655) (the same in [master](../reference/xppaut-master/arrayplot.c#L371)).
The end of the time range is therefore 20 for every run, so a run of 100 time units is labelled as ending at 20 and
a run of 5 has its axis stretched to 20. xppautX reads the time of the first and the last row shown (W492,
[#556](https://github.com/MuhammadMoustafa/xppautX/issues/556)); no example model's output changed.

## 86. The cross spectrum averages one window more than it divides by

`cross_spectrum` runs `for(j=0;j<=kwin;j++)` over the windows and then divides by `kwin`
([histogram.c:602](../reference/xppaut-8.0/histogram.c#L602), [histogram.c:624-627](../reference/xppaut-8.0/histogram.c#L624-L627);
[master 581](../reference/xppaut-master/histogram.c#L581)), while `spectrum` runs `j<kwin`
([histogram.c:514](../reference/xppaut-8.0/histogram.c#L514)). The extra window starts where fewer than `win` points are
left and wraps round to the start of the data (`kk=(i+j*shift)%nr`). The cross spectrum (`PSDxy`) is therefore
(kwin+1)/kwin times too large; the coherence is not, for its numerator and denominators sum over the same windows.
xppautX keeps the numbers (the Fourier menu's results stay those of XPPAUT) and names the difference here; no example
model's output changed (W489, [#553](https://github.com/MuhammadMoustafa/xppautX/issues/553)).
