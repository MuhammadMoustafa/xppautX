# W434 Sweeps on CPU threads: stage 1 study (#496)

Read-only study (maintainer, 2026-10-17: "the sweeps are independent integrations"). No code changed. Line numbers are master at 3fbeb7eb. Stage 2 is the card that makes the change; this file says what it has to do and what is still open.

## 1. The sweeps and their entry points

| Sweep (the user's name) | Command | Entry point (file:line) | Runs |
|---|---|---|---|
| Initial conds > Range | `run.range` (commands_run.cpp:92-101), flag 0 | `run_range` integrate.cpp:1079 -> `sweep_range` :903 -> `range_sweep` :688 | i = 0..steps, initial data varies (IC) |
| Initial conds > 2par range | `run.double_range` (commands_run.cpp:~120-131), flag 1 | same, `rtype` RANGE_TOGETHER or RANGE_INDEPENDENT | (nit+1)(nit2+1) runs; j outer, i inner (:745) |
| Parameter range (Range over a parameter, `type == PARAM`) | `run.range` with a parameter item | same `range_sweep`; the parameter is set with `set_val` (:778) | as above |
| Equilibrium range | `equilibrium.range` (commands_equilibrium.cpp:183) -> `equilibrium_range` :622 | `eq_range_sweep` integrate.cpp:530 | one equilibrium search per step |
| Range shoot | `bvp.solve_range` (commands_bvp.cpp:43) -> `bvp_solve_range` pp_shoot.cpp:280 | `do_sh_range` pp_shoot.cpp:94 (one `bvshoot` per step) | one boundary-value solve per step |

Other callers of `run_range` follow the same rules: `analysis.lyapunov` with `range_over_parameters` (adj2.cpp:534-554) and `array_plot.range` (commands_array_plot.cpp:96-102). Stage 2 covers them with the same path.

## 2. What one integration reads and writes in the Session

The Session members are in core/session.h (`Session` at :72). The lines below are the members a run touches, with what it does to each.

Written by every run of a sweep (in `range_sweep`, integrate.cpp:688-897):
- `data_store` (session.h:99, storage.h:54): `rows`, the stored columns `col[][]`, the current state `current` and `current_time`. A run stores its rows here (:811-816); the rows are reset after a run when `reset` is set (:871).
- `integrator` (session.h:137, `IntegratorState` integrate.h:258): `my_start` (per run), `range_flag` (run_range :1086), `stop_note`, `step_error`, `steady`/`steady_result`, `solver` (a `unique_ptr<Solver>`), `rhs` (`RightHandSide`, integrate.h:215, which holds a `Session *`), `flags` (the global flags' states, flags.cpp), `rosen_htry` and `gear_pivot` (the last Rosenbrock step and Gear's pivots, which carry from one integration to the next unless reset), `adj_range`, `halt_flag`, `stop_flag`.
- `parser` (session.h:168, `ParserState` expr.h:160): `constants` and `variables`, which are the parameter values and the fixed and derived variables the right-hand side reads and writes. `set_val` (expr_symbols.cpp:759) writes a parameter here; `my_rhs` writes the fixed variables and `extra()` (my_rhs.cpp:60-90, the write at :68 through `&s`); `evaluate_derived` (derived.cpp:36) writes the derived ones.
- `tables` (session.h:212) and `networks` (session.h:205): `redo_all_fun_tables` (tabular.cpp:252) rebuilds the tables a parameter changes; `re_evaluate_kernels` (volterra2.cpp:102) rebuilds the Volterra kernels (`volterra`, session.h:243).
- `solver_work` (session.h:139) and `jacobian_scratch` (session.h:140): the solvers' work memory.
- `numerics` (session.h:164, `NumericsSettings` load_eqn.h:53): `storflag`, `pauser`, `endsing`, `poiext`, `inflag`, `par_fol`, and the seed chain `rand_seed`/`last_seed` (seed_this_run, integrate.cpp:666-673).
- `random` (session.h:234, `Random` xpp_math.h:160) and `stochastic` (session.h:230, `StochasticState` markov.h:57): the stochastic runs draw from Session::random; `append_stoch` and `do_stats` write the stochastic statistics.
- `delay` (session.h:224, `DelayState`), `histogram` (226), `dae` (247), `adjoint` (228): per-run state of the delay, DAE and adjoint solvers.
- `plot_windows` (session.h:101): `color[0]` of the current window (:697, :749, :887), the batch nullclines and direction field (`do_batch_nclines`, `do_batch_dfield`, :848), the film (`reset_film`, `RangeFilm` integrate.cpp:507), `draw_label`.
- `browser` (session.h:159): `refresh_browser`, `data_get_mybrowser`, `wipe_rep` (eq_range_sweep :538).
- `last_ic` (session.h:143): read by `get_ic` (integrate.cpp:1519) at the start of each run when `oldic==1`; written by `set_initial_data` (:1510) once, at the end of a sweep with `oldic==0` (:881-882) and in `do_sh_range` after every shot (pp_shoot.cpp:141).
- `last_equilibrium`, `last_equilibrium_eigenvalues` (session.h:181-182): written by each equilibrium search of an equilibrium sweep.
- `array_plot` (session.h:266), `autox_state`/`diagram` (session.h:155-156, through `autox_freeze_it` and `autox_get_info`), `shoot_range` (session.h:251, read by `do_sh_range`).

Process-wide (not the Session), so a worker may read them but must not write them without a lock: `xpp::job` (xpp_job.h: the cancel flag and the row and point counters, atomics per xpp_job.h:100), `program` (xpp_globals.h), `xpp::log_settings` (xpp_log.h), and the front end's table `XppUi` (xpp_ui.h: `ping`, `bottom_msg`, `show_progress`, the film, every draw call). The front end's calls are main-thread only.

Read-only in a sweep: the Model (`model()`, const), `last_ic` when `oldic==1`, and `shoot_range` / `eq_range` (the settings).

**The chaining is the first constraint.** In `range_sweep`, a run with `oldic==0` starts from `x`, which the previous run left at its end (`x` is the caller's buffer and the loop never resets it; the sweep's own end writes the last state back with `set_initial_data`, :881-883). With `oldic==1` every run starts from `get_ic(s,1,x)`, which is `s.last_ic` (:1519-1526), and `last_ic` does not change during the sweep. So only `oldic==1` makes the runs independent. The same holds for `eq_range_sweep` (`x` is the Newton start of step i+1 and is not reset between steps) and for `do_sh_range` (`ystart` carries from shot to shot through `set_initial_data` at pp_shoot.cpp:141). Stage 2 runs a chained sweep on one thread, and says so in the log (INFO), since the user asked why it was not parallel.

## 3. What has to be per-thread for runs to go side by side

A worker needs its own copy of everything in section 2 that a run writes and that a later run could read. Per run:
1. The solver: a fresh `Solver` through `xpp::start_solver` (IntegratorState `solver`), its work memory (`solver_work`, `jacobian_scratch`), `gear_pivot`, `rosen_htry`, `steady`, `step_error`, `stop_note`, `flags`, `dae`, `delay`.
2. The right-hand side: a `RightHandSide` bound to the worker's view (its `session` pointer, integrate.h:215-218), with its own `parser.constants` and `parser.variables`.
3. The numerics' per-run flags (`storflag`, `pauser`, `endsing`, `poiext`, `inflag`, `par_fol`).
4. The random generator and the stochastic state, if the run draws numbers (section 4).
5. The output: a run's rows go into the worker's own buffer, not into `data_store`; the join copies them in order.
6. The equilibrium result (`fixptlist`, `last_equilibrium*`) and the log lines (held per run, replayed in order).

Stays on the main thread, in run order: the plot windows and the film, `draw_label`, `refresh_browser`, the batch nullclines and direction field, `ping`, `bottom_msg`, `show_progress`, `write_this_run` (files named by the run index, :977-989), `autox_freeze_it`, `do_this_liaprun`, the stochastic statistics (`do_stats`), and `set_initial_data` at the end.

## 4. Is W251's pure compiled right-hand side enough?

No, not alone.

What W251 proved (roadmap row W251; W251's own note: "threads need a pure compiled RHS (no Session writes in func), the interpreter stays serial"): AUTO's collocation may use threads when the RHS is a native function that reads only its arguments and the model's immutable tables. The condition is `Model::autox_rhs_pure` (model.h:150), with `pure_rhs` (my_rhs.cpp:94-98) as the function that takes `c` and `v` as its arguments. `autox_parallel_ready` (autox_parallel.cpp:69-73) is the predicate: `autox_rhs_pure`, `rhs.function == my_rhs`, and a discrete solver only with `store_every == 1`.

Why it is not enough for sweeps:
- `pure_rhs` covers the right-hand side, and only it. The integrator also needs events, flags, delays, DAEs, the Jacobian of the implicit solvers, the tables and kernels that a parameter rebuilds (`redo_all_fun_tables`, `re_evaluate_kernels`), `extra()` and the store (section 2). None of those is pure today.
- `pure_rhs` reaches the Session through a `const_cast<xpp::Session *>` (my_rhs.cpp:96-97). Stage 2 removes it: the function takes the worker's `c` and `v` and the model only.
- The predicate is about the autoX RHS programs (`autox_rhs_pure` is set by compile_model for those). The sweep's own RHS path (`my_rhs`, :84-91) is the same function in the solvers but binds to the Session, so a sweep needs the same proof for its own path, and a check that the model has no network, table or stochastic term that a worker would share.
- Interpreted models (not every model compiles to native code) keep the Session's interpreter, which writes its own stacks, so they stay serial. How many examples are pure is not measured here; stage 2 measures it over examples/ode.

So W251's condition is the right starting gate (reuse `autox_parallel_ready`, whose name says autoX: stage 2 moves it to a neutral owner), plus the per-run state of section 3.

## 5. Random numbers

Today: `seed_this_run` (integrate.cpp:666-673) seeds `Session::random` once per sweep from `numerics.rand_seed`, then advances the seed stream with `xpp::next_seed` (xpp_math.h:184-189, a pure function of the seed). So a stochastic sweep's run i draws from the stream that runs 0..i-1 left, and its numbers depend on the run order.

Two ways to keep a run's numbers fixed when runs go on threads:
- **A (recommended for stage 2): a sweep that draws random numbers stays serial.** Any sweep with `stochastic.flag`, Monte Carlo (`eq_range.mc`), or a model that calls `ran()`, `normal()` or `poisson()` (the parser's draws from Session::random, xpp_math.h:142-160) runs on one thread, logged. Every number is unchanged; no md5 moves.
- **B: each run seeds its own generator.** The seeds are computed serially before the fan-out by applying `next_seed` i times, so run i always gets seed_i and any thread count gives the same numbers. This is deterministic, but it changes what a stochastic sweep gives today: the examples' md5s and the stochastic baselines change, so it is a numerics change, and AGENTS.md requires that to be on purpose. Question for the maintainer (section 8).

Either way, the thread count must never change a number (the card's own rule).

## 6. Order of results, failures, and Stop

Results keep the serial order. Each worker fills its run's result (rows, progress, the early-stop note, a failure, the held log lines). The join runs in run order (j, then i), with the code the serial loop has now: store the rows, count `progress.runs` and `progress.steps`, write the run's file with index `j*(nit+1)+i` (:977-989), and log the early stop. A failure in run k stops the serial sweep at k, so the join stops there too; runs after k that finished on a thread are discarded, and runs after k not yet started are not started (a per-sweep flag, not the front end's cancel, so the join stays exact). The error text keeps its serial wording ("Range stopped at ... (run k of n)", :822-827).

Stop: every worker reads `xpp::job::cancelled()` between runs (as `range_sweep` does after each run, :821) and at the integrator's checkpoints (integrate.cpp, through `xpp::job`), so Stop ends every run at its next checkpoint; the join then reports the completed runs in order, exactly as the serial sweep reports a Stop (`sweep_result`, commands_run.cpp:87). The "every stop says why" rule holds: the join names the run that was cut and why (the job's Stop), as the serial loop does.

Bit-identical at any thread count holds when every run is a function of its inputs only: its start (`last_ic` with `oldic==1`), its parameter values, its seed (section 5), and no state left by an earlier run. The one thing to check in stage 2: `gear_pivot`, `rosen_htry`, the flags and the equilibrium's `fixptlist` persist from run to run in the serial loop. If a serial run starts from a value a previous run left in them, a worker that starts fresh gives a different number. Stage 2 finds each such carry (the serial sweep with a fresh state per run against the current one) before it parallelises; a carry that changes numbers is a bug to fix, not a baseline to move.

## 7. Shared piece with autoX's two directions (W596, branch task/W596-two-directions, not merged)

`git show task/W596-two-directions:core/autox_jobs.cpp` (396 lines) already does the same shape for two autoX jobs:
- `Direction` (autox_jobs.cpp:152-160): one job's run, its error, the table lines it writes, and whether it was skipped or started.
- Evaluation copies (:260-329): the Session's `parser.constants` and `variables` are copied into each job (`run.rhs_constants`, `run.rhs_variables`, :312-314) and each job is marked `pure_rhs` (:312) only when `autox_parallel_ready(s)` holds (:265-266); a serial job restores the Session's constants before the next (`restore_constants`, :268-271). After the join the Session takes the last started job's constants (:322-329), the same end state as a serial run.
- `run_jobs` (:176-228): one thread per job, a mutex and a condition variable, a wait that polls the front end's Abort (`byeautox_`, every INPUT_POLL_SECONDS), a join; a job whose turn comes after a cancel is not started; each thread holds its log lines (`AutoxLogHold`, xpp_log.h:133 on the branch) into `job.table`, replayed at the join.
- The join writes the two directions in the fixed order +DS then -DS, renumbered (:331 on).
- Thread count: `budget = state.threads ? state.threads : AutoxParallel::physical_threads()` (:304), and `collocation_threads` split among the jobs (:310).

Stage 2 candidate: extract the common part into one owner (a small module beside xpp_job.h: a per-item evaluation copy, the ordered join, the log hold, the cancel check). Two differences make a sweep need more than W596's code: a sweep has up to thousands of runs, so a thread per run is wasteful and it needs a persistent pool (AutoxParallel's pool, autox_parallel.h on master, sized to the physical cores; its `intervals(n, work)` takes an `AutoxRun&`, so the pool is autoX-typed and would move with the change); and a sweep's join writes rows and files, not renumbered tables. W596 and W434 should share the owner, not copy it (AGENTS.md "Single source"). Coordinate with W596 before either merges: W596 is in progress.

## 8. Design questions for the maintainer (count: 2)

1. Random numbers: keep stochastic and Monte Carlo sweeps serial (A, numbers unchanged), or give each run its own seed (B, numbers change, baselines move)? This study recommends A.
2. Thread count: share the autoX thread setting (`autox_threads`, xpp_prefs.h:71-72, W267) or give sweeps their own count? Either way the count is a preference, never a result.

## 9. Stage 2 plan

Steps:
1. Refactor only, serial, numbers unchanged: move the per-run state of section 3 into a `SweepRun` built from the Session (run's solver, parser copy, flags, rows, log hold). The serial loop uses it too, so there is one code path.
2. Remove the `const_cast` in `pure_rhs` (my_rhs.cpp:96-97); give the sweep's `RightHandSide` a worker view; reuse `autox_parallel_ready` (moved to a neutral owner) plus the no-network, no-table, no-random conditions. Log why a sweep is serial.
3. Find each cross-run carry (section 6) and make the start of every run explicit; check it against the serial loop run with fresh state per run.
4. Fan out on the shared pool (section 7) with the ordered join, the per-run failure rule and Stop, for run.range, run.double_range, the parameter range, equilibrium.range, bvp.solve_range and lyapunov over parameters, one at a time, each with its own md5 and check.
5. Measure with `tools/heavy.sh --alone` over thread counts 1, 2, 4 and the physical count, on a cheap model and a heavy one (for the paper; W268 compares a GPU against these numbers).

Risks: hidden cross-run state (section 6); the parser's mutable name caches (`mutable` in expr.h:206-207, written when a symbol is first needed) written during a worker's evaluation; memory per worker (a copy of the parser's arrays and the solver's work); oversubscription against autoX's collocation threads; Stop latency of a long run (a run is cut only at a checkpoint); progress events from several threads (`report_rows` and the row counters are atomics, xpp_job.h, but the front end's calls stay on the main thread); the xpp::log sink's thread safety (to check: xpp_log.cpp); models that are not pure, which stay serial and so gain nothing.

Checks (stage 2): bit-identical to serial at 1, 2, 4 and the physical count (examples md5s with the thread count set by a check-only setting, goldencheck, odexcheck, and servercheck's range sections with the thread count varied; a sweep stopped at its middle run, and a sweep that fails at run k, both equal to serial in rows and messages); tsancheck (`tools/tsancheck.sh` runs servercheck and webcheck under ThreadSanitizer: the sweep sections join it); asancheck; valgrindcheck for the join; and the timed series with `--alone` for the paper.

## 10. Open

- Whether any model in examples/ode is pure (native RHS, no network, no table or random term): unknown until stage 2 measures it.
- How many sweeps a chained model (`oldic==0`) gives in practice: a user who wants parallel runs needs `oldic==1` ("Use old I.C.s" = 1, lunch-new.cpp:358), which the page should say.
