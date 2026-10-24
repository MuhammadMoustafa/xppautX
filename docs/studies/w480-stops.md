# W480 stage 1: every stop says why (audit)

Card W480 "Every stop says why", stage 1 (issue #543), 2026-10-24. Read-only audit of core/ (AUTO's files
included): every place where a computation ends before what was asked, or fails, and what the user is told.
Nothing in core/ or tests/ was changed. Rule (AGENTS.md, Code quality, "Every stop says why"): the stop reports
once what stopped (the variable and t, or the AUTO point and branch), the setting that caused it with its
value, and the model line where there is one; no silent `break`, nothing only at DEBUG; an error is an
`xpp::Error` with an `xpp::Place`.

How it was checked: **R** = confirmed by running `./xppautX` (`--silent`, or `--server` through
tools/xppclient.py), models in the session scratchpad; **S** = by reading the source only. 

## Summary

47 places (14 in integrate.cpp, 12 in the solvers and their helpers, 8 in equilibria, shooting, fits, nullclines
and analyses, 13 in AUTO). What the user sees today (codes used in the table):

| code | what the user sees today | count |
|---|---|---|
| N | nothing: the run or sweep just ends, or goes on with wrong data | 13 |
| L | one log line (WARN/INFO), not an error and not shown by a client | 6 |
| F | only AUTO's fort.9 file (one of the four is a trace by design) | 4 |
| P | a shown error or message that lacks the time, the variable, the setting with its value, or the real place | 15 |
| OK | a correct report (what, where, why); I5's table of every variable is DEBUG only, a detail of an error that is shown | 9 |

14 of the 34 N/L/P entries were confirmed by running (R); the rest by reading.

The worst silent stops (all confirmed by running unless said):

1. **A range or sweep swallows every solver failure** (integrate.cpp:1725, `range_flag`): Gear, CVODE, QualRK
   fail at the first parameter value, the sweep ends there, 0 rows, no message (I2). With a fixed-step solver
   that can fail (backward Euler, Volterra) the failed step is ignored and the run goes on to the end with
   unconverged values: 1001 rows where the single run stops at t=0.98 (I3).
2. **`Min step` above `Dt`** ends a run at its first row, exit code 0, nothing printed but "Integration not
   completed -- will write anyway..." (integrate.cpp:1707, I1).
3. **Rosenbrock never ends** on a solution that overflows: `rosen` retries a step forever because
   `absh<hmin` can never be true after `absh=MAX(hmin,...)` (odesol2.cpp:508); no message, only Escape (S5).
4. **A global event with `arret` halts the run with no message**, the last row is the only trace (I6).
5. **A bound exceeded inside a range** ends that run silently and the sweep goes on (I4); the Monte Carlo
   search silently counts past its 400-point list (`fixptlist.n++` before the `m<MAXFP` test, then a read of
   `x[i]` for i up to n: probably an out-of-bounds read, to confirm with asancheck, I12).
6. **AUTO**: a failed set-up (`Initialization Error CRASH!!`) and a missing restart label only log; a special
   point whose location failed ("Possible special point"), "Multiplier inaccurate" and singular pivots are
   fort.9 notes only; the stpnt readers' failures are ignored (A1-A4, A7-A9).

Where a report exists it is often incomplete in the same way: the message names no time, no variable, no
setting or value ("Step size too small", "Too many iterates", "kflag=-1: minimum step too big"), and its place
is the pseudo-place of the command (`command:1`, source `{"cmd":"key"...}`) instead of the model line.
Unreviewed jargon survives in CVODE's texts ("oh well", "???", "Hmmm?") and Gear's `kflag=-N`.

Already correct (keep as the model of the rest): a NaN or out-of-bounds variable in a run (variable, t,
model line), DAE failure (t and why), AUTO's branch ends (Par Min/Max, Norm, Max points, no convergence, Stop:
`auto_stop.cpp`, the `autoinfo` event, checked by servercheck's `check_auto_stop`), the .recx player (file,
line, step), DoPri's bound stop, a delay history that cannot be allocated.

Settings are named by their `core/model_options.cpp` keys and labels (`min_step` "Minimum step", `bound`,
`tolerance`, `abs_tolerance`, `eul_tol`, `eul_iter`, `singpt_max_iterates`, `singpt_newton_tolerance`,
`bvp_max_iterates`, `bvp_tolerance`, AUTO's Dsmin, ITNW, ITMX, NMX).

## Findings by file

Columns: id; file:line; the stop; today (code, how verified); what to show (the message proposed; `{...}` are
filled in); how a check triggers it (a model and setting).

### core/integrate.cpp (W477 numerics #1, #14; W476 integrate loop)

| id | file:line | stop | today | should see | trigger |
|---|---|---|---|---|---|
| I1 | integrate.cpp:1707 | `fabs(dt)<fabs(min_step)` returns 1 before a step | N, R (1 row, exit 0; --silent prints only the generic "Integration not completed") | `Dt {dt} is below Minimum step (min_step={min}); no step was taken` at the model's `@ dt` line, as an error (a failed run exits 1) | `x'=-x; @ meth=gear,dt=0.01,min_step=0.1` (also qualrk) |
| I2 | integrate.cpp:1725 | a solver's failure in a range or manipulation (`range_flag||suppress_bounds`, not dense) returns 1 and drops the error | N, R (Gear, CVODE: sweep ends at i=0, 0 rows, bottom line only) | the solver's error with the sweep position: `Range stopped at a=1 (step 0 of 3): Gear: step size fell below min_step=1e-12 at t=0.9946, X's error test failing` | blow-up model (`x'=a*x^2`), `@ meth=gear`, Range over `a` 1 to 3 |
| I3 | integrate.cpp:1738-1745 | the same flags make a failed fixed-step advance (backward Euler "Too many iterates", Volterra) a non-stop: the loop goes on with the state it had | N, R (range of 2 runs: 1001 rows each, the single run stops at t=0.98) | the error of S1 and the end of that run, `at a={p}` | blow-up model, `@ meth=backward_euler`, Range over `a` |
| I4 | integrate.cpp:1796, 1833 | a bound exceeded in a range or in Draw invariant sets (`range_flag||suppress_bounds`): inner `break` then `ieqn<neq+1` ends the run, returns 0 | N, R (4 runs of RK4 ended at t=1.0 to 0.33, no message) | one INFO/WARN line per run (or one sweep summary, W480 stage 2 decides): `X reached bound=1000 at t=1.01 for a=1.67 (run 2 of 4)`; a summary `3 of 4 runs ended at the bound` | blow-up model, Range over `a` 1 to 3 |
| I5 | integrate.cpp:1779-1812 | NaN or out of bounds in a plain run | OK/D, R (variable, t, model line shown; the f(t-1), f(t) table of every variable is DEBUG only) | keep the error; add the setting (`bound=1000`) to the bound text and the offending state of the variables at INFO inside the error's detail, not DEBUG | `x'=sqrt(1-t*10)`, `@ meth=euler` |
| I6 | integrate.cpp:1826, flags.cpp:278 | a `global ... {arret=1}` event sets `stop_flag`; the loop `break`s | N, R (euler: ends at t=3, nothing printed, exit 0) | `Stopped at t=3: event 'global 1 x-3' set arret=1 (model line 3)` as an INFO message; a stop that is asked for is not a failure (exit 0) | `x'=1; global 1 x-3 {x=0;arret=1}` |
| I7 | integrate.cpp:1822-1823 | Escape, `/` | OK (cancel; the `stopped` event says where) | none | Escape through the protocol (exists) |
| I8 | integrate.cpp:1970-1974, 1829 | `poincare_stop`: the run ends at the first section crossing; and a Poincare map that finds no crossing stores no row | N, S | `Stopped at the section crossing t={t}` (INFO); and `No crossing of {section} in [0,{total}] (poincare_variable, poincare_plane)` | `x'=-x; Poincare map` with a plane never reached |
| I9 | integrate.cpp:1031, 1178 | --silent's tail: "Errors occured in range integration", "Integration not completed -- will write anyway..." with no cause; the window shows nothing at all | P, R | remove both: the error of the stop is the report (printed once by the command) | any of I1-I4 under `--silent` |
| I10 | integrate.cpp:1967, 2214 | storage cannot grow (`stor_full`) | OK, S (error, command place; the cap is `INT_MAX/2` rows) | add the row count and the model's `maxstor`-like setting if there is one | `XPP_STORAGE_FAIL_GROW` (exists, servercheck) |
| I11 | integrate.cpp:560-642 (eq_range_sweep) | a parameter value at which Sing pts finds no root: the sweep goes on and **stores the old x as the row** (5 rows for 5 values, 3 without a root); one error at the end, the first only | P, R | per value (or one summary): `a=0.5: no equilibrium found from X=0.5 (singpt_max_iterates=100, singpt_newton_tolerance=1e-7)`; no row for a value without a root, or a row of NaN | `x'=x^2+a`, Equilibria Range over `a` -1 to 1 |
| I12 | integrate.cpp:445-505 (Monte Carlo) | a guess that does not converge is dropped silently (no count); more than MAXFP=400 distinct points: `fixptlist.n++` runs on, `m<MAXFP` guards only the store, then the browser loop reads `x[i]`, i up to n | N, S (limit; memory error probable) | a summary `Monte Carlo: 38 of 100 guesses converged to 3 distinct equilibria`; at 400 `Monte Carlo: stopped at 400 equilibria (list full)`; no overrun | `x'=sin(x)` over [-1000,1000], `# Guesses` 5000 (asancheck) |
| I13 | integrate.cpp:891-899, 1031-1040 | `range_sweep` returns -1 when a run stopped early; `do_range(M_I2)` ignores it | N, S | covered by I2/I3; the command ends with the error | 2-parameter range with a failing corner |
| I14 | integrate.cpp:2188-2201 (`shoot_easy`, `shoot`: Draw invariant sets) | `suppress_bounds=1`: a trajectory that leaves the box, and a solver failure, both end it silently by design | N, S | an INFO line per branch `Manifold branch ended at t={t}: {X} reached bound` (it is what drawing the set means; the solver error must still be shown) | saddle with `Draw invariant sets` |

### core/solver.cpp, odesol2.cpp, stiff.cpp, gear.cpp, cv2.cpp, dormpri.cpp (W477 numerics #5, #6, #8, #15; W476)

The solvers' `Solver::failed()` (solver.cpp:244) builds `fail(method, text, command_place())`: the place is the
command that started the run (`command:1`), no `t`, no setting. All rows below are the same defect in
different solvers; the fix is one place, `failed()`, given the time and the settings of the method.

| id | file:line | stop | today | should see | trigger |
|---|---|---|---|---|---|
| S1 | solver.cpp:62-63 (odesol2.cpp:158,171; volterra2.cpp:355,365) | backward Euler / Volterra: singular Jacobian (-1), more than `eul_iter` iterates (-2) | P, R ("Too many iterates", place `command:1`) | `Backward Euler: no convergence at t=0.98: {iterates} iterates without the change below eul_tol (eul_iter=100, eul_tol=1e-7), X's equation (model line 2)` | blow-up, `@ meth=backward_euler` |
| S2 | solver.cpp:80-85, gear.cpp:757,881,1022,1053 | Gear `kflag` -1 to -4 | P, R ("kflag=-1: minimum step too big") | `Gear: step size fell below Minimum step (min_step=1e-12) at t=0.9946; the error test of X failed (tolerance=0.001)` (-2: `required order too big`, -4: `tolerance=0.001 is too small for the round-off at t=...: use {suggested}`) | blow-up, `@ meth=gear` |
| S3 | solver.cpp:101-107, stiff.cpp:95-113,155-230 | QualRK / Stiff: stepsize close to 0, too small (`hnext<=min_step`), more than MAXSTP=100000 steps, MAXTRY=40 retries, singular | P, S (QualRK on a blow-up stops at the bound first; the other texts not run) | the same form: `QualRK: step size {h} fell below min_step={min} at t={t}`, `more than 100000 steps between output times at t={t} (raise Dt or tolerance={tol})`; MAXSTP/MAXTRY become named constants with their reason | `x'=-1e6*(x-cos(t))`, `meth=qualrk`, `tolerance=1e-12`; `@ min_step=0.1`-like |
| S4 | solver.cpp:127-140, cv2.cpp:115-141 | CVODE failure codes -3 to -9 | P, R (-6 text has CVODE's own t, h; the xppautX text is "Converg. failure -- oh well!") | drop the jargon (`oh well`, `???`, `Hmmm?`); name `t`, `h`, `tolerance`/`abs_tolerance` values and the variable CVODE names (it already gives `error_var`); -3 `more than {mxstep} steps between output times` | blow-up, `@ meth=cvode` |
| S5 | solver.cpp:171-177, odesol2.cpp:442-519 | Rosenbrock: `if(absh<hmin)` can never be true after `absh=MAX(hmin,...)`, so a step whose error stays above `rtol` (an overflowing solution) is retried forever | N, R (no message and no end in 30 s, 51 rows); this is a bug in XPPAUT's rosen too (docs/xppaut-findings.md candidate) | the loop ends with `Rosenbrock: step size fell below {hmin} at t={t}`; `ierr` already exists | blow-up, `@ meth=rosenbrock,total=1.5` |
| S6 | dormpri.cpp:1395-1398, 365,381,512,965,981,1090 | DoPri5/8: -2 attempts, -3 step too small, -4 stiff | P, R (range run: error "Step size too small", place `command:1`; t and h are in separate WARN lines `Exit of dopri5 at t=..., step size too small h=...`) | one error: `DoPri5: step size h={h} fell below the rounding limit at t={t}`; the WARNs go (reported twice today, three lines) | blow-up, `@ meth=dopri5`, Range |
| S7 | flags.cpp:456 | `flag_loop` gives up after `flag_count+2` events inside one step: "Working too hard??" | L, S (WARN, the run goes on) | `Events: more than {n} events inside one step at t={t} (smin={frac}); the step was taken without the later ones` with the event's model line | two `global` conditions that re-trigger each other |
| S8 | gear.cpp:455,494 | `get_evec` (eigenvector for a manifold): "Pivot failed", "max iterates exceeded" | L, S | an error of the command `Eigenvector at {eigenvalue} not found (maximum iterates={maxit})`, instead of a bare WARN | Sing pts then Draw invariant sets at a defective saddle |
| S9 | del_stab.cpp:272,292,297 | delay stability's root search: singular Jacobian, "Failed to converge", "Max iterates exceeded" | L, S | `Characteristic root near {guess} not found (maximum iterates={maxit})` | `x'=-a*delay(x,tau)`, Sing pts with a large `tau` |
| S10 | delay_handle.cpp:134 | `delay(x,tau)` with tau<0 or above the `delay` setting: `stop_integration` with no place | P, R ("Delay negative or too large", no place, no tau, no t) | `delay: tau={tau} is outside 0..{delay} (the @ delay setting) at t={t}, in the equation of X` at the model line | `par tau=2; x'=-delay(x,tau); @ delay=1` |
| S11 | dae_fun.cpp:139-152 | DAE: singular Jacobian, no convergence, out of bounds, fold | OK, S (t and the reason; model place) | add `singpt_max_iterates` and the tolerance to the no-convergence text | exists in servercheck? (add: an `aeqn` with no root) |
| S12 | expr_eval.cpp:245 | a user function nests deeper than MAX_UFUN_DEPTH | OK, S | none | recursive `f(x)=f(x)` (check) |

### core/nullcline.cpp, histogram.cpp, do_fit.cpp, pp_shoot.cpp, gear.cpp (equilibria)

| id | file:line | stop | today | should see | trigger |
|---|---|---|---|---|---|
| E1 | gear.cpp:164-167 (`do_sing`) | Newton did not converge, or eigenvalues failed; `newton` merges four causes in one `ierr=1` (singular Jacobian, residual above `bound`, `singpt_max_iterates`, a step error) | P, R ("Could not converge to root", place `command:1` and the key) | `Sing pts: no equilibrium from X=0.5: Newton {exceeded singpt_max_iterates=100, met a singular Jacobian, or left the box (bound=1000)} after {k} iterations, last change {r} above singpt_newton_tolerance=1e-7` (make the cause a returned enum) | `x'=x^2+1`, Sing pts Go |
| N1 | nullcline.cpp:263-384, 665 | an RHS evaluation error while a nullcline or direction field is computed: `return`/`break` on `step_error` | P, S (the command layer takes it at ui_json.cpp:903; the grid point is not named) | add the grid point `at X=..., Y=...` to the error | a model whose RHS fails at a grid point (`1/(x-0.5)` is not an error: use a `stop_integration` source such as the delay or a user-function depth) |
| P1 | pp_shoot.cpp:394,441 | `ode_int` fails inside a shot: `iret=-4`; the integrator's own error is dropped (and at 441 never kept) | P, S ("Unable to complete integration") | keep and show the integrator's error (what, t) with `at shot {k}, iterate {i}` | BVP on a blow-up model, `@ meth=gear` |
| P2 | pp_shoot.cpp:101-118, 265 | `bad_shoot`: "Maximum iterates exceeded" (no count or residual), "Bad Jacobian -- uninvertable" (no which); -5/-6 (Escape, `/`) are unknown to it | P, S | `Shoot: no convergence after {bvp_max_iterates=20} iterations: residual {err} above bvp_tolerance=1e-5`; `iret` becomes an enum returned in a `Result` (W477 auto.md #9) | BVP with `@ bvp_max_iterates=1` |
| P3 | pp_shoot.cpp:154-157 | Range Shoot: the first failing parameter ends the whole sweep and the message does not name the value | P, S | `Range Shoot stopped at a={p} (step {i} of {n}): {the shot's message}` | Range shoot over a parameter that loses the solution |
| F1 | do_fit.cpp:271 | the fit ended at `maxiter` (bottom line `Max iterations exceeded...` only) and the parameters are replaced by the unfinished values | P, S | `Fit: stopped after maxiter={n} iterations, chisq={c} (the last change {d} is not below tol={tol}); parameters set to the last iterate` | `Fit` with `maxiter=1` |
| F2 | do_fit.cpp:442 | "Singular matrix encountered" | P, S | add the iteration, `lambda` and which parameter column is dependent | fit two parameters that act identically |
| H1 | histogram.cpp:110-130, 330, 524 | post-processing after a run (histogram, Fourier, mean) does nothing when the run stored fewer than 2 rows or the model has fewer than 2 variables | N, S | `Fourier: the run stored {rows} row(s); nothing to analyse` | a run stopped at its first step (I1) with Fourier on |

### AUTO: gogoauto.cpp, autlib1/3/5.cpp, xpp_math_auto.cpp, auto_nox.cpp, auto_stop.cpp (W477 auto.md #1, #3, #9)

| id | file:line | stop | today | should see | trigger |
|---|---|---|---|---|---|
| A1 | gogoauto.cpp:488 | `set_function_pointers` finds no solver for (ips, isw, itp): logs "Initialization Error CRASH!!", `list` is uninitialised, nothing runs and `go_go_auto` returns 1 | L, S (log_auto only; the run ends with no points and no error) | `AUTO: no continuation for ips={ips}, isw={isw}, itp={itp}` as an `xpp::AutoFailed` (the W477 fix of the else branch) | unreachable from the forms today; force with an impossible `ips/isw` through a restart of a foreign `.s` file |
| A2 | gogoauto.cpp:73 | "Restart label not found" (label missing in fort.3 while the diagram has it) returns 0, ignored | L, S (`auto restart` validates its label first: "Restart: no point labelled 99" is shown, R) | `AUTO: restart label {n} is not in {fort.3 path}` | stale `.s` file |
| A3 | autlib3.cpp:1783-1799,2042,2309,2697,3014; autlib1.cpp:3998,5278 | stpnt readers return 1 on a short read; both callers ignore it and continue with half-zero data | N, S | `AUTO: the restart data of label {n} ({file}:{line}) ends before {what}` (one error from the reader, W477 #3) | truncate fort.3 after a label |
| A4 | autlib1.cpp:1707-1739, 5693-5730 | the location of a special point (fold, branch point, Hopf, period doubling) did not converge in ITMX iterations: the point is given up and the branch goes on | F, S (fort.9 NOTE only) | `AUTO: Br {b} Pt {p}: the special point between points {p} and {p+1} was not located in ITMX={n} iterations (SP kind: {LP|BP|HB|PD...}); it is missing from the diagram` | a Hopf with `itmx=1` |
| A5 | autlib1.cpp:1551-1579, 2438-2464, 5018-5050; auto_stop.cpp | no convergence at a step (fixed or at Dsmin): the branch ends (MX) | OK/L, R (ends reported in the `autoinfo` event `stop` and AUTO's Output via `log_auto` INFO: `no convergence even at the smallest step (Dsmin 0.001)`; a `--server` client without the subscription, and the console, show nothing; the settings ITNW/EPSL/EPSU are not named) | keep; add a WARN-level shown message for the abnormal ends (noconv) naming `ITNW`, `EPSL`, `EPSU`, `Dsmin` with values and the last point; user stops and limits stay INFO | tools/models/auto_stop.odex `rl1=10` (exists) |
| A6 | autlib1.cpp:1571, 2456, 5041 | "Retrying step" (iid>=2) | F (a trace by design) | none | |
| A7 | xpp_math_auto.cpp:152, 251 | tiny pivot in GE/NLVC: a NOTE at iid>=; AUTO carries on with the tiny pivot | N, S | at the end of a branch with no convergence caused by a singular system: name it `the Jacobian is singular (pivot {p} < {rsmall})` in the stop text | `x'=a*x` at a=0 (every point is a steady state) |
| A8 | autlib1.cpp:5971-5990 | "Multiplier inaccurate": the stability and period-doubling/torus detection of the following points are unreliable | F, S | `AUTO: Br {b} Pt {p}: the Floquet multipliers are inaccurate (try Ntst, Ncol larger)` shown, with the diagram's point marked | periodic orbit with `ntst=5` |
| A9 | autlib5.cpp:637 | HomCont `IEQUIB=2 NOT ALLOWED WITH ISTART=3` | F, S | an error naming both settings | homoclinic set-up |
| A10 | autlib1.cpp:306,311 | `DSMIN/DSMAX less than 0: will use absolute value` | L, S | keep (it states the correction); name the value | `dsmin=-0.001` |
| A11 | auto_nox.cpp:553-585 | `AutoFailed`, `jacobian_failure`: shown once, `AUTO stopped: ...`; place is `command:1` when the error has none | OK/P, S | give the error the model line when it has one (`Jacobian not finite` does) | exists (W275) |
| A12 | autevd.cpp (XPP_DIAGRAM_POINTS_LIMIT) | the diagram holds its most points | OK, S (checked by servercheck W432) | none | exists |
| A13 | json_player.cpp:268,326 | a recording step cannot be played | OK (file, line, step) | none | exists |

## Grouping with W476 and W477

By file, as their reports group: integrate.cpp I1-I14 (W477 numerics #1, #14; W476's integrate loop);
solver.cpp, odesol2.cpp, stiff.cpp, gear.cpp, cv2.cpp, dormpri.cpp, flags.cpp, del_stab.cpp, delay_handle.cpp,
dae_fun.cpp, expr_eval.cpp S1-S12 (W477 numerics #5, #6, #8, #15, #16, #18, #19); gear.cpp (Sing pts), nullcline.cpp,
pp_shoot.cpp, do_fit.cpp, histogram.cpp E1, N1, P1-P3, F1, F2, H1 (W477 numerics #4, #22, auto.md #9);
AUTO's files A1-A13 (W477 auto.md #1, #3, #9). Entries kept as references only, needing no change: I7, I10,
S11, S12, A5 (its abnormal ends excepted), A12, A13.

## Proposed stage 2 cards

Order: the check first (it fails until each card lands, each card lists the cases it turns green), then the
shared message builder, then the areas. Cards are packed for one agent per pack (AGENTS.md Workflow).

1. **W480a "Every stop says why: the servercheck section"** (tools/servercheck.py, tools/models/stops/*.odex,
   docs/protocol.md). Adds the section below with every case as an expected-message check; cases whose card
   is not merged yet are listed `known` and flip to checks as their card lands (a check that passes a
   case the card did not fix is a bug in the check). Haiku-sized.
2. **W480b "A stop's message names its time, variable and setting"** (solver.cpp, solver.h, xpp_error.h only
   if a field is needed, integrate.cpp's returns, odesol2.cpp, stiff.cpp, gear.cpp, cv2.cpp, dormpri.cpp;
   covers I1, I9, S1-S6, S10, S11). `Solver::failed()` takes the time and the method's settings
   (`tolerance`, `min_step`, `eul_iter`, ...) and the place becomes the model line of the variable where one
   is known; remove CVODE's jargon and DoPri's duplicate WARN lines; name MAXSTP and MAXTRY; I1's
   `Dt<min_step` becomes an error; **fix S5's infinite loop** (a real bug, add its entry to
   docs/xppaut-findings.md, rosen is XPPAUT's code at odesol2's origin). Numerics: messages only, md5s
   unchanged. Sonnet.
3. **W480c "Ranges, sweeps and events say why a run ended"** (integrate.cpp, flags.cpp, pp_shoot.cpp's range,
   do_eq_range, monte carlo; covers I2, I3, I4, I6, I8, I11-I14, S7, P3). Decisions for the maintainer first
   (settle-design): a range run that ends at the bound is legitimate (the sweep goes on): one INFO line per
   run, or one summary per sweep? A solver failure in a range stops the sweep with the error and the
   parameter value (I2) and I3 stops being a silent continuation (this changes the numbers of any range that
   today hits it: record in the card, `examples_check` shows which models). Also Monte Carlo's MAXFP overrun
   (asancheck first). Sonnet or opus.
4. **W480d "Equilibria, shooting, fits and delays say why"** (gear.cpp, pp_shoot.cpp, do_fit.cpp,
   del_stab.cpp, nullcline.cpp, histogram.cpp, delay_handle.cpp; covers E1, N1, P1, P2, F1, F2, H1, S8, S9).
   Newton returns an enum of its four causes; `bvshoot` returns a `Result<ShootEnd>` (combines W477 auto.md
   #9) and carries the integrator's error. Sonnet.
5. **W480e "AUTO says why"** (gogoauto.cpp, autlib1.cpp, autlib3.cpp, autlib5.cpp, xpp_math_auto.cpp,
   auto_stop.cpp, auto_nox.cpp; covers A1-A5, A7-A9, A11). The fort.9 NOTEs that mean a lost point (special
   point not located, inaccurate multipliers, singular pivot) become `autoinfo` notes and an AUTO Output
   WARN; the stpnt readers fail with an `AutoFailed` (shared with W477 auto.md #1 and #3: one card, not two).
   `go_go_auto` returns void. Needs `autcheck`, `autocompare` (numbers unchanged). Opus: the reasons live in
   translated Fortran.
6. **W480f "Stops under --silent and --convert"**: remove I9's two tail lines and make a stopped
   `--silent` run's exit code and its printed error the same as the window's and `--server`'s (one
   behaviour, three front ends); can be part of W480b.

## Proposed servercheck section: "W480: every stop says why"

One fresh `--server` per case (tools/models/stops/*.odex, small models named for the stop), each case run
through the window's command (key `i` then `g`, the Range form, Sing pts, Shoot) and, where the stop is a
run's, once under `--silent` (stderr and exit code). A check reads data only: the `message` error event's
`error` text, its `file` and `line`, `rows` in `state`, and the log lines; never a time.

A case passes when the text contains the parts the rule demands. Expected parts (a regular expression per
part, kept in one table in the check so a wording change is one edit):

| case | model and setting | expected in the error | also |
|---|---|---|---|
| min_step | `x'=-x`, gear, `min_step=0.1`, `dt=0.01` | `Dt`, `0.01`, `min_step`, `0.1` | `--silent` exit 1 |
| gear | `x'=a*x^2`, gear, `a=1` | `Gear`, `t = 0.99`, `min_step` | line of the model equation |
| qualrk / stiff | the blow-up model and `bound=1000` | `X`, `bound`, `t =` | existing text kept |
| cvode | the blow-up model | `CVode`, `t =`, `tolerance`; no `oh well` | |
| dopri5, dopri8 | the blow-up model | `DoPri`, `t =`, `X` | one message, not two |
| rosenbrock | blow-up, `total=1.5` | an error within 20 s of the run's own wait for `idle` (a hang fails the safety timeout, never a time compare) | no loop |
| backward_euler | blow-up | `Backward Euler`, `eul_iter`, `100`, `t =` | |
| bound | `x'=x*x`, rk4, `bound=100` | `X`, `bound`, `100`, `t =` | exists, kept |
| NaN | `x'=sqrt(1-10*t)` | `X is NaN`, `t =`, model line 3 | |
| event halt | `global 1 x-3 {x=0;arret=1}` | an INFO message `Stopped at t=3` naming the event | exit 0 |
| range, gear | the gear case over `a` 1 to 3 | `a=1`, the gear message | not 0 rows silently |
| range, backward Euler | the same | the error and a stop at `a=1` | rows less than a full run |
| range, rk4 at the bound | the same | one summary `... ended at the bound` | |
| delay | `par tau=2; x'=-delay(x,tau); @ delay=1` | `tau=2`, `delay`, `t =`, a place | |
| DAE | an `aeqn` with no root (`0=x^2+1+y`) | `t =`, the iterate setting and value | exists in part |
| Sing pts | `x'=x^2+1` | `singpt_max_iterates`, a cause, no `command:1` | |
| eq range | `x'=x^2+a`, `a` -1 to 1 | `a=0` (first value without a root), no row stored for it | `state.rows` equals the values with a root |
| Monte Carlo | `x'=sin(x)` over [-1000,1000], 5000 guesses | `converged`, `400` | under ASan: no report |
| shoot | BVP with `bvp_max_iterates=1` | `bvp_max_iterates`, `bvp_tolerance`, the residual | |
| shoot, solver fails | BVP on the blow-up model | the integrator's own message | |
| fit | `maxiter=1` | `maxiter`, `chisq` | |
| AUTO | tools/models/auto_stop.odex: noconv-min / noconv-fixed | a WARN `message` naming `Dsmin`/`IADS`, the branch and the point | `autoinfo.stop` kept |
| AUTO special point | a Hopf run with `itmx=1` | `special point`, `ITMX`, the point | |
| AUTO set-up | an impossible restart (truncated fort.3) | `restart data`, file and line | |

Modes: every case runs in `--server`; the ones that are a plain run also in `--silent`; none needs the window.
Together with each card's own gate, a stage 2 card that adds a stop turns its row from `known` to a check.
