# W652: does every option earn its place? (#725)

A study, no code changed (maintainer, 2026-11-08: the diagram group turned out to be a second editor of what autoX > Axes edits, so every option is checked). It audits the 134 grouped rows of `core/model_options.cpp` (`rows[]`, in table order and `option_groups` order) and, at the end, its 45 import-only rows. Layout decisions it builds on: [w614-options-layout.md](w614-options-layout.md) ("Decided"); W616 (#686) settled that the delay guesses and special_points get rows, `autoeval` leaves Numerics, and the diagram and plot panels edit the open view.

## Summary

Proposals for the 134 grouped rows: **113 keep** (5 of them kept as "the first view's saved axes", Q2), **4 merge**, **17 move**, **0 drop**, **0 import-only** (the 45 rows that are import-only already are, and stay). No row is read by nothing: every row has at least one reader outside the table (a grep each, the table's "read by" column). What the greps did show: 7 rows are read only by the batch-run code (`do_batch_*`, `json_silent.cpp`, `many_pops.cpp`); 1 row (`near_tolerance`) only at load time; 5 diagram rows and about two dozen plot rows only seed a view or the starting graph at load (the open view then owns the state, which is the diagram problem again).

Questions for the maintainer, most important first:

1. **`axes.x_low`/`x_high`/`y_low`/`y_high` and `axes.x_min`/`x_max`/`y_min`/`y_max`: one pair or two?** XPPAUT keeps XLO..YHI (the 2D window) and XMIN..YMAX (the 3D box) as two sets, but for a 2D plot they are one quantity: [load_eqn.c:681-686](../../reference/xppaut-8.0/load_eqn.c#L681) copies XLO..YHI over the box when AXES<5, XMIN also sets XLO unless XLO was set ([load_eqn.c:1755](../../reference/xppaut-8.0/load_eqn.c#L1755)), and the defaults are tied ([load_eqn.c:594-611](../../reference/xppaut-8.0/load_eqn.c#L594)); in 3D the XLO..YHI are unused. The row's `reset` and `parse` carry that coupling (`once_option("XLO")` in model_options.cpp). Proposed: one pair `axes.x_min`..`y_max` (plus `z_min`/`z_max` in 3D), `axes.kind` says which it is; import maps XLO..YHI to it (XPPAUT: XLO 54, XHI 72, YLO 63, YHI 66 of 218 demos; XMIN..YMAX 5). Cost: 49 repo `.odex` files rename four keys (a model giving both pairs for a 2D plot becomes an error, where XPPAUT lets XLO win silently). Four rows go and the claim logic with them.
2. **Rows that are a view's own state: one editor.** `diagram.variable/x_min/x_max/y_min/y_max` only seed the first view (autox_nox.cpp:650,1154, `autox.default_view`); `autox.configure_plot` (autoX > Axes) edits the view's real axes. The same holds for the plot rows: `axes.*`, `line.*`, `color_by.*`, `direction_field.grid`, `curves` seed the starting graph, `window.set_bounds`/`plot.*` edit the open one. Proposed (W616's decision made concrete): the group's panel is the view's editor, and the `.odex` rows are what saving the first view writes, with no separate options panel beside Axes. Is that what you want, or should a model not carry a view at all?
3. **The 8 colour rows** (4 diagram, 2 nullcline, 2 manifold): a model's or the user's? 0 of 218 demos set any of them; no command edits the diagram's, and all are global, not per view. Proposed: preferences, one colour list for the page.
4. **Five rows that are how a result is shown, not what a model is** (`equilibria.print_eigenvalues`, `draw_invariant_sets`, `draw_strong_sets`; `export.color`, `font_size`): XPPAUT asks them as y/n or in a dialog each time and no demo sets them. Proposed: preferences, still arguments of `equilibrium.configure` and the picture dialog.
5. **`equilibria.near_tolerance` is no equilibria setting**: its only reader is the loader's `scan_neartol` (odex_load.cpp:443-452) for the formulas calling `near`, and XPPAUT has no such option (no `neartol` in its `load_eqn.c`). Proposed: the model's own statement (W614 already said "out of the panels").
6. **7 batch-only rows** (`plot.nullclines.draw`, `direction_field.draw`, `export.format`, `output.data_file`, `nullclines_file`, `after_run`, `after_run_file`): read only by the code that finishes a batch run. Keep as the model's description of a batch run? The rule "every option has a place in the UI" then needs a place for them (W616 gives `output` one); `format` could instead come from the picture file's extension, like the data file's.
7. **`state_limit` and `search_limit` are one XPPAUT option** (BOUND, 33 of 218 demos, [load_eqn.c:1629](../../reference/xppaut-8.0/load_eqn.c#L1629), which the importer already splits in two). Proposed: keep two rows (a blow-up bound and a Newton search bound are different quantities), say so in the help.
8. **The 12 `analysis` rows** (histogram, histogram2, spectrum) are the last arguments of the analysis commands (commands_analysis.cpp:83-143), set by 1 of 218 demos. Keep as the model's analysis defaults (proposed) or session-only?
9. **`interface` (sliders, buttons, animation) is model content, not a setting**: sliders and buttons are a UI the model offers (and `slider` edits them at run time), the animation a companion file (already `top_level`). Proposed: model statements like `userbut`, out of `options`.
10. **The sweep's second variable has no rows.** w614 lists "second variable's four" in `sweep`, but the table has none (`plow2`, `phigh2` and their variable are only set by the `range` command, commands_run.cpp:109-114, and reset in model_options.cpp). Add the four, or leave a two-parameter sweep to the command?

Search done for this study (nothing was written): the option table read whole; for each row a grep of its member's name over `core/*.cpp`/`*.h` (outside model_options.cpp, numerics_settings.cpp, lunch-new.cpp, odex_*), with its zero-hit cases looked up by hand (`max_step`, `spec_*` are read through aliases); the registry's commands for the other editors (`numerics`, `equilibrium.configure`, `range`, `autox.configure_*`, `window.set_bounds`, `plot.*`, `histogram`); XPPAUT's `msc("NAME"` lines in `load_eqn.c`; and the 218 `.ode` files of `reference/xppaut-8.0`, counting the files whose `@` lines name each option ("N/218" below; a demo counts once). "Would a user set it": yes = set by tens of demos or needed by its kind of model; rarely = a few or none; no = a dialog's last answer nobody sets.

## The 134 rows

Each group is a table, rows in the option table's order. "Repeats": another row or editor that writes the same state. "Whose": the model's (kept in the model's file, saved by a `.setx` where marked), a view's, or a preference. Labels are the row's own (derived from the key unless the table names one).

### integration (27 rows)

| # | key (label) | XPPAUT's name, demos using it | read by (core/) | would a user set it | repeats | whose | proposal |
|---|---|---|---|---|---|---|---|
| 1 | `total_time` (Total time) | [TOTAL](../../reference/xppaut-8.0/load_eqn.c#L1581) (136/218) | integrate.cpp:783; nullcline.cpp:659 | yes | `numerics`/Numerics menu write the same member (numerics_settings.cpp): one state | the model's | keep |
| 2 | `start_time` (Start time) | [T0](../../reference/xppaut-8.0/load_eqn.c#L1613) (1/218) | delay_handle.cpp:173; pp_shoot.cpp:321 | rarely: 1 demo; the default 0 fits | same as total_time | the model's | keep |
| 3 | `dt` (Dt) | [DT](../../reference/xppaut-8.0/load_eqn.c#L1605) (65/218) | delay_handle.cpp:55; equilibria.cpp:200 | yes | same as total_time | the model's | keep |
| 4 | `store_every` (Store every N steps) | [NOUT](../../reference/xppaut-8.0/load_eqn.c#L1491), [NJMP](../../reference/xppaut-8.0/load_eqn.c#L1483) (17/218) | integrate.cpp:783,1464 | yes | same as total_time | the model's | keep |
| 5 | `store_from` (Store from) | [TRANS](../../reference/xppaut-8.0/load_eqn.c#L1621) (14/218) | solver.cpp:312; nullcline.cpp:612 | yes: the transient | same as total_time | the model's | keep |
| 6 | `storage_precision` (Storage precision) | no @ option | storage.cpp:270; load_eqn.cpp:175 | rarely: single is XPPAUT's, double for exact data (W613) | none (ours) | the model's | keep |
| 7 | `state_limit` (State limit) | [BOUND](../../reference/xppaut-8.0/load_eqn.c#L1629) (33/218) | integrate.cpp:151; dormpri.cpp:856 | yes: BOUND is in 33 demos | `search_limit` is the other half of XPPAUT's one BOUND | the model's | keep (Q7) |
| 8 | `stop_at_limit` (Stop at the limit) | no @ option | integrate.cpp:149; dormpri.cpp:856 | rarely: XPPAUT always stops | none (ours) | the model's | keep |
| 9 | `method` (Method) | [METH](../../reference/xppaut-8.0/load_eqn.c#L1507) (63/218) | solver.cpp:96; load_eqn.cpp:630 | yes | same as total_time | the model's | keep |
| 10 | `subsystem_split` (Subsystem split) | no @ option | expr_native.cpp:986 (the only reader) | rarely: only with a Poincare section or stop on a model of independent parts | none (ours, W435) | the model's | keep |
| 11 | `seed` (Seed) | [SEED](../../reference/xppaut-8.0/load_eqn.c#L1322) (0/218) | xpp_session.cpp:725,764; expr_symbols.cpp:340 | rarely: stochastic and `ran()` models | none | the model's | keep |
| 12 | `step.relative_tolerance` (Relative tolerance) | [TOL](../../reference/xppaut-8.0/load_eqn.c#L1645) (6/218) | cv2.cpp:129; odesol2.cpp:416 | rarely | same as total_time | the model's | keep |
| 13 | `step.absolute_tolerance` (Absolute tolerance) | [ATOL](../../reference/xppaut-8.0/load_eqn.c#L1637) (5/218) | cv2.cpp:129; odesol2.cpp:416 | rarely | same | the model's | keep |
| 14 | `step.min` (Minimum step) | [DTMIN](../../reference/xppaut-8.0/load_eqn.c#L1589) (1/218) | solver.cpp:90; integrate.cpp:1849 | rarely | same | the model's | keep |
| 15 | `step.max` (Maximum step) | [DTMAX](../../reference/xppaut-8.0/load_eqn.c#L1597) (0/218) | dormpri.cpp:799; solver.cpp:90 | rarely: 0 demos | same | the model's | keep |
| 16 | `jacobian.kind` (Jacobian) | no @ option | expr_native.cpp:1035; autox_evd.cpp:271; xpp_batch.cpp:119 | rarely: exact or finite differences | none (ours) | the model's | keep |
| 17 | `jacobian.band_upper` (Band upper) | [BANDUP](../../reference/xppaut-8.0/load_eqn.c#L1662) (1/218) | cv2.cpp:75; jacobian.cpp:158 | rarely: a banded model only | same | the model's | keep |
| 18 | `jacobian.band_lower` (Band lower) | [BANDLO](../../reference/xppaut-8.0/load_eqn.c#L1671) (1/218) | cv2.cpp:75; jacobian.cpp:158 | rarely: a banded model only | same | the model's | keep |
| 19 | `delay.max_delay` (Maximal delay) | [DELAY](../../reference/xppaut-8.0/load_eqn.c#L1654) (14/218) | delay_handle.cpp:28; numerics.cpp:224 | yes: delay models | same | the model's | keep |
| 20 | `delay.history_points` (History points) | [VMAXPTS](../../reference/xppaut-8.0/load_eqn.c#L1518) (2/218) | commands_run.cpp:204; volterra2.cpp:80; expr_symbols.cpp:323 | rarely: 2 demos | same | the model's | keep |
| 21 | `euler.tolerance` (Newton tolerance) | no @ option | odesol2.cpp:166; volterra2.cpp:444 | rarely: implicit Euler and Volterra only | XPPAUT's Numerics menu only, no @ option ([numerics.c:213](../../reference/xppaut-8.0/numerics.c#L213) is BVP's) | the model's | keep |
| 22 | `euler.iterations` (Newton iterations) | no @ option | odesol2.cpp:171; volterra2.cpp:446 | rarely: same | same | the model's | keep |
| 23 | `bvp.max_iterates` (BVP maximum iterates) | no @ option | pp_shoot.cpp:125,259,274 | rarely: shooting only | XPPAUT's Numerics menu only ([numerics.c:213](../../reference/xppaut-8.0/numerics.c#L213)) | the model's | keep |
| 24 | `bvp.tolerance` (BVP tolerance) | no @ option | pp_shoot.cpp:125,259,274 | rarely: same | same | the model's | keep |
| 25 | `bvp.epsilon` (BVP epsilon) | no @ option | pp_shoot.cpp:125,259,274 | rarely: same | same | the model's | keep |
| 26 | `torus.period` (Period) | [TOR_PER](../../reference/xppaut-8.0/load_eqn.c#L1535) (1/218) | integrate.cpp:2046-2051; autox_nox.cpp:1120 | rarely: models on a circle | none | the model's | keep |
| 27 | `torus.fold` (Fold) | [FOLD](../../reference/xppaut-8.0/load_eqn.c#L1569) (4/218) | integrate.cpp:2045 (`s.itor`) | rarely | none; a list, one name each | the model's | keep |

### poincare (5 rows)

| # | key (label) | XPPAUT's name, demos using it | read by (core/) | would a user set it | repeats | whose | proposal |
|---|---|---|---|---|---|---|---|
| 28 | `map` (Map) | [POIMAP](../../reference/xppaut-8.0/load_eqn.c#L1791) (1/218) | dormpri.cpp:780; integrate.cpp:2130; numerics.cpp:249 | rarely: 1 demo | a Poincare dialog (Numerics) writes the same member | the model's | keep |
| 29 | `variable` (Variable) | [POIVAR](../../reference/xppaut-8.0/load_eqn.c#L1802) (1/218) | integrate.cpp:2130; numerics.cpp:311 | rarely | same | the model's | keep |
| 30 | `sign` (Sign) | [POISGN](../../reference/xppaut-8.0/load_eqn.c#L1822) (1/218) | integrate.cpp:2130; numerics.cpp:313 | rarely | same | the model's | keep |
| 31 | `plane` (Plane) | [POIPLN](../../reference/xppaut-8.0/load_eqn.c#L1847) (1/218) | integrate.cpp:2157; numerics.cpp:312 | rarely | same | the model's | keep |
| 32 | `stop` (Stop) | [POISTOP](../../reference/xppaut-8.0/load_eqn.c#L1831) (0/218) | dormpri.cpp; integrate.cpp; numerics.cpp:314 | rarely | same | the model's | keep |

### equilibria (8 rows)

| # | key (label) | XPPAUT's name, demos using it | read by (core/) | would a user set it | repeats | whose | proposal |
|---|---|---|---|---|---|---|---|
| 33 | `max_iterations` (Equilibria: maximum iterations) | [NEWT_ITER](../../reference/xppaut-8.0/load_eqn.c#L1561) (2/218) | integrate.cpp:433,593; dae_fun.cpp:205 | rarely: NEWT_ITER 2 demos | `equilibrium.configure` writes the same member | the model's | keep |
| 34 | `newton_tolerance` (Equilibria: Newton tolerance) | [NEWT_TOL](../../reference/xppaut-8.0/load_eqn.c#L1552) (4/218) | integrate.cpp:433,593; dae_fun.cpp:204 | rarely: 4 demos | same | the model's | keep |
| 35 | `jacobian_step` (Equilibria: Jacobian step) | [JAC_EPS](../../reference/xppaut-8.0/load_eqn.c#L1544) (3/218) | solver.cpp:119; adj2.cpp:537 | rarely: 3 demos (JAC_EPS) | same | the model's | keep |
| 36 | `search_limit` (Equilibria: search limit) | [BOUND](../../reference/xppaut-8.0/load_eqn.c#L1629) (33/218) | integrate.cpp:433,593,595; equilibria.cpp | rarely | the other half of XPPAUT's BOUND, which `state_limit` also takes | the model's | keep (Q7) |
| 37 | `print_eigenvalues` (Equilibria: print eigenvalues) | no @ option | equilibria.cpp:221 | no: XPPAUT asks y/n at each equilibrium; no demo sets it | `equilibrium.configure` and the dialog's last answer | a preference (how a result is shown) | move to preferences: not a model's setting (Q4) |
| 38 | `draw_invariant_sets` (Equilibria: draw invariant sets) | no @ option | equilibria.cpp:243 | no: same | same | a preference | move to preferences (Q4) |
| 39 | `draw_strong_sets` (Equilibria: draw strong sets) | no @ option | equilibria.cpp:292 | no: same | same | a preference | move to preferences (Q4) |
| 40 | `near_tolerance` (Near tolerance) | no @ option | odex_load.cpp:443-452 (scan_neartol, a load-time read; nothing at run time) | rarely: ours, default 1e-9 | none; the formulas that call `near` | the model's formulas, not an equilibria setting | move: out of `equilibria`, into the model's statements (Q5) |

### sweep (7 rows)

| # | key (label) | XPPAUT's name, demos using it | read by (core/) | would a user set it | repeats | whose | proposal |
|---|---|---|---|---|---|---|---|
| 41 | `variable` (Variable) | [RANGEOVER](../../reference/xppaut-8.0/load_eqn.c#L1858) (3/218) | commands_run.cpp:28,109; pp_shoot.cpp:285 | rarely: 3 demos | the sweep dialog's last answer: `range` commands write the same `s.integrator.range` members (commands_run.cpp:28-36,109-121) | the model's (the Range dialog's start) | keep |
| 42 | `steps` (Steps) | [RANGESTEP](../../reference/xppaut-8.0/load_eqn.c#L1867) (3/218) | commands_run.cpp:31,115; commands_analysis.cpp:60 | rarely: 3 demos | same | the model's | keep |
| 43 | `low` (Low) | [RANGELOW](../../reference/xppaut-8.0/load_eqn.c#L1877) (3/218) | commands_run.cpp:29,110; integrate.cpp:299 | rarely: 3 demos | same | the model's | keep |
| 44 | `high` (High) | [RANGEHIGH](../../reference/xppaut-8.0/load_eqn.c#L1887) (3/218) | commands_run.cpp:30,111; integrate.cpp:300 | rarely: 3 demos | same | the model's | keep |
| 45 | `reset` (Reset) | [RANGERESET](../../reference/xppaut-8.0/load_eqn.c#L1896) (3/218) | commands_run.cpp:32,120 | rarely | same | the model's | keep |
| 46 | `old_ic` (Old ic) | [RANGEOLDIC](../../reference/xppaut-8.0/load_eqn.c#L1912) (1/218) | commands_run.cpp:33,121; integrate.cpp:302 | rarely: 1 demo | same | the model's | keep |
| 47 | `write_files` (Write files) | [RANGE](../../reference/xppaut-8.0/load_eqn.c#L1930) (3/218) | commands_run.cpp:36,124; json_silent.cpp:68,138 | rarely | same | the model's | keep |

### continuation (24 rows)

| # | key (label) | XPPAUT's name, demos using it | read by (core/) | would a user set it | repeats | whose | proposal |
|---|---|---|---|---|---|---|---|
| 48 | `mesh_intervals` (Mesh intervals) | [NTST](../../reference/xppaut-8.0/load_eqn.c#L1939) (1/218) | autox_nox.cpp:1135 | yes: NTST in 1 demo, a periodic run needs it | `autox.configure_numerics` writes the same `autox_state` members (autox_settings.cpp:162,194) | the model's (autoX's) | keep |
| 49 | `max_steps` (Max steps) | [NMAX](../../reference/xppaut-8.0/load_eqn.c#L1947) (4/218) | autox_nox.cpp:1136 | yes | `autox.configure_numerics` writes the same `autox_state` members (autox_settings.cpp:162,194) | the model's (autoX's) | keep |
| 50 | `output_every` (Output every) | [NPR](../../reference/xppaut-8.0/load_eqn.c#L1955) (4/218) | autox_nox.cpp:1137 | yes | `autox.configure_numerics` writes the same `autox_state` members (autox_settings.cpp:162,194) | the model's (autoX's) | keep |
| 51 | `collocation_points` (Collocation points) | [NCOL](../../reference/xppaut-8.0/load_eqn.c#L1963) (0/218) | autox_nox.cpp:1134 | rarely | `autox.configure_numerics` writes the same `autox_state` members (autox_settings.cpp:162,194) | the model's (autoX's) | keep |
| 52 | `min_step_size` (Min step size) | [DSMIN](../../reference/xppaut-8.0/load_eqn.c#L1973) (6/218) | autox_nox.cpp:1140 | yes | `autox.configure_numerics` writes the same `autox_state` members (autox_settings.cpp:162,194) | the model's (autoX's) | keep |
| 53 | `max_step_size` (Max step size) | [DSMAX](../../reference/xppaut-8.0/load_eqn.c#L1981) (8/218) | autox_nox.cpp:1139 | yes | `autox.configure_numerics` writes the same `autox_state` members (autox_settings.cpp:162,194) | the model's (autoX's) | keep |
| 54 | `step_size` (Step size) | [DS](../../reference/xppaut-8.0/load_eqn.c#L1989) (2/218) | autox_nox.cpp:1138 | yes | `autox.configure_numerics` writes the same `autox_state` members (autox_settings.cpp:162,194) | the model's (autoX's) | keep |
| 55 | `parameter_min` (Parameter min) | [PARMIN](../../reference/xppaut-8.0/load_eqn.c#L1998) (7/218) | autox_nox.cpp:1141 | yes | `autox.configure_numerics` writes the same `autox_state` members (autox_settings.cpp:162,194) | the model's (autoX's) | keep |
| 56 | `parameter_max` (Parameter max) | [PARMAX](../../reference/xppaut-8.0/load_eqn.c#L2006) (8/218) | autox_nox.cpp:1142 | yes | `autox.configure_numerics` writes the same `autox_state` members (autox_settings.cpp:162,194) | the model's (autoX's) | keep |
| 57 | `norm_min` (Norm min) | [NORMMIN](../../reference/xppaut-8.0/load_eqn.c#L2014) (0/218) | autox_nox.cpp:1143 | yes | `autox.configure_numerics` writes the same `autox_state` members (autox_settings.cpp:162,194) | the model's (autoX's) | keep |
| 58 | `norm_max` (Norm max) | [NORMMAX](../../reference/xppaut-8.0/load_eqn.c#L2022) (0/218) | autox_nox.cpp:1144 | yes | `autox.configure_numerics` writes the same `autox_state` members (autox_settings.cpp:162,194) | the model's (autoX's) | keep |
| 59 | `parameter_tolerance` (Parameter tolerance) | [EPSL](../../reference/xppaut-8.0/load_eqn.c#L2030) (2/218) | autox_nox.cpp:1146 | rarely | `autox.configure_numerics` writes the same `autox_state` members (autox_settings.cpp:162,194) | the model's (autoX's) | keep |
| 60 | `state_tolerance` (State tolerance) | [EPSU](../../reference/xppaut-8.0/load_eqn.c#L2039) (2/218) | autox_nox.cpp:1147 | rarely | `autox.configure_numerics` writes the same `autox_state` members (autox_settings.cpp:162,194) | the model's (autoX's) | keep |
| 61 | `special_point_tolerance` (Special point tolerance) | [EPSS](../../reference/xppaut-8.0/load_eqn.c#L2047) (2/218) | autox_nox.cpp:1148 | rarely | `autox.configure_numerics` writes the same `autox_state` members (autox_settings.cpp:162,194) | the model's (autoX's) | keep |
| 62 | `mesh_adapt_every` (Mesh adapt every) | no @ option | autox_evd.cpp:132 | rarely: AUTO's own advanced fields, in XPPAUT's Numerics window only ([auto_nox.c:773](../../reference/xppaut-8.0/auto_nox.c#L773)) | `autox.configure_numerics` writes the same `autox_state` members (autox_settings.cpp:162,194) | the model's (autoX's) | keep |
| 63 | `max_branch_switches` (Max branch switches) | no @ option | autox_evd.cpp:134 | rarely: AUTO's own advanced fields, in XPPAUT's Numerics window only ([auto_nox.c:773](../../reference/xppaut-8.0/auto_nox.c#L773)) | `autox.configure_numerics` writes the same `autox_state` members (autox_settings.cpp:162,194) | the model's (autoX's) | keep |
| 64 | `switch_one_direction` (Switch one direction) | no @ option | autox_evd.cpp:134 | rarely: AUTO's own advanced fields, in XPPAUT's Numerics window only ([auto_nox.c:773](../../reference/xppaut-8.0/auto_nox.c#L773)) | `autox.configure_numerics` writes the same `autox_state` members (autox_settings.cpp:162,194) | the model's (autoX's) | keep |
| 65 | `output_detail` (Output detail) | no @ option | autox_evd.cpp:135 | rarely: AUTO's own advanced fields, in XPPAUT's Numerics window only ([auto_nox.c:773](../../reference/xppaut-8.0/auto_nox.c#L773)) | `autox.configure_numerics` writes the same `autox_state` members (autox_settings.cpp:162,194) | the model's (autoX's) | keep |
| 66 | `max_location_iterations` (Max location iterations) | no @ option | autox_evd.cpp:136 | rarely: AUTO's own advanced fields, in XPPAUT's Numerics window only ([auto_nox.c:773](../../reference/xppaut-8.0/auto_nox.c#L773)) | `autox.configure_numerics` writes the same `autox_state` members (autox_settings.cpp:162,194) | the model's (autoX's) | keep |
| 67 | `max_newton_iterations` (Max newton iterations) | no @ option | autox_evd.cpp:137 | rarely: AUTO's own advanced fields, in XPPAUT's Numerics window only ([auto_nox.c:773](../../reference/xppaut-8.0/auto_nox.c#L773)) | `autox.configure_numerics` writes the same `autox_state` members (autox_settings.cpp:162,194) | the model's (autoX's) | keep |
| 68 | `full_newton_after` (Full newton after) | no @ option | autox_evd.cpp:138 | rarely: AUTO's own advanced fields, in XPPAUT's Numerics window only ([auto_nox.c:773](../../reference/xppaut-8.0/auto_nox.c#L773)) | `autox.configure_numerics` writes the same `autox_state` members (autox_settings.cpp:162,194) | the model's (autoX's) | keep |
| 69 | `step_adapt_every` (Step adapt every) | no @ option | autox_evd.cpp:140 | rarely: AUTO's own advanced fields, in XPPAUT's Numerics window only ([auto_nox.c:773](../../reference/xppaut-8.0/auto_nox.c#L773)) | `autox.configure_numerics` writes the same `autox_state` members (autox_settings.cpp:162,194) | the model's (autoX's) | keep |
| 70 | `suppress_branch_points` (Suppress branch points) | no @ option | autox_nox.cpp:1127; autox_run.cpp | rarely | `autox.configure_numerics` writes the same `autox_state` members (autox_settings.cpp:162,194) | the model's (autoX's) | keep |
| 71 | `follow_all_branches` (Follow all branches) | no @ option | autox_jobs.cpp:881; autox_settings.cpp:195 | yes: ours (W595) | `autox.configure_numerics` writes the same `autox_state` members (autox_settings.cpp:162,194) | the model's (autoX's) | keep |

### diagram (9 rows)

| # | key (label) | XPPAUT's name, demos using it | read by (core/) | would a user set it | repeats | whose | proposal |
|---|---|---|---|---|---|---|---|
| 72 | `variable` (Variable) | [AUTOVAR](../../reference/xppaut-8.0/load_eqn.c#L2138) (2/218) | autox_nox.cpp:1158 | rarely: 2 demos | `autox.configure_plot` (autoX > Axes) edits the open view's axes; the row only seeds the first view (autox_nox.cpp:650,1154) and `autox.default_view` | a view's (seed only) | keep as the first view's saved axes, one editor (Q2) |
| 73 | `x_min` (X min) | [AUTOXMIN](../../reference/xppaut-8.0/load_eqn.c#L2122) (6/218) | autox_nox.cpp:650,1153 | yes: a published diagram's range (6 demos) | `autox.configure_plot` (autoX > Axes) edits the open view's axes; the row only seeds the first view (autox_nox.cpp:650,1154) and `autox.default_view` | a view's (seed only) | keep as the first view's saved axes, one editor (Q2) |
| 74 | `x_max` (X max) | [AUTOXMAX](../../reference/xppaut-8.0/load_eqn.c#L2106) (6/218) | autox_nox.cpp:651,1153 | yes: a published diagram's range (6 demos) | `autox.configure_plot` (autoX > Axes) edits the open view's axes; the row only seeds the first view (autox_nox.cpp:650,1154) and `autox.default_view` | a view's (seed only) | keep as the first view's saved axes, one editor (Q2) |
| 75 | `y_min` (Y min) | [AUTOYMIN](../../reference/xppaut-8.0/load_eqn.c#L2130) (6/218) | autox_nox.cpp:652,1156 | yes: a published diagram's range (6 demos) | `autox.configure_plot` (autoX > Axes) edits the open view's axes; the row only seeds the first view (autox_nox.cpp:650,1154) and `autox.default_view` | a view's (seed only) | keep as the first view's saved axes, one editor (Q2) |
| 76 | `y_max` (Y max) | [AUTOYMAX](../../reference/xppaut-8.0/load_eqn.c#L2114) (6/218) | autox_nox.cpp:653,1155 | yes: a published diagram's range (6 demos) | `autox.configure_plot` (autoX > Axes) edits the open view's axes; the row only seeds the first view (autox_nox.cpp:650,1154) and `autox.default_view` | a view's (seed only) | keep as the first view's saved axes, one editor (Q2) |
| 77 | `colors.stable_equilibrium` (Stable equilibrium) | [SEC](../../reference/xppaut-8.0/load_eqn.c#L2064) (0/218) | autox_nox.cpp:803,841 | rarely: 0 demos; a taste | the diagram's colours are global in autox_nox.cpp, no view holds them; no command edits them | a preference | move to preferences, with the other colour rows (Q3) |
| 78 | `colors.unstable_equilibrium` (Unstable equilibrium) | [UEC](../../reference/xppaut-8.0/load_eqn.c#L2072) (0/218) | autox_nox.cpp:803,842 | rarely: 0 demos; a taste | the diagram's colours are global in autox_nox.cpp, no view holds them; no command edits them | a preference | move to preferences, with the other colour rows (Q3) |
| 79 | `colors.stable_periodic` (Stable periodic) | [SPC](../../reference/xppaut-8.0/load_eqn.c#L2080) (0/218) | autox_nox.cpp:811,843 | rarely: 0 demos; a taste | the diagram's colours are global in autox_nox.cpp, no view holds them; no command edits them | a preference | move to preferences, with the other colour rows (Q3) |
| 80 | `colors.unstable_periodic` (Unstable periodic) | [UPC](../../reference/xppaut-8.0/load_eqn.c#L2088) (0/218) | autox_nox.cpp:811,844 | rarely: 0 demos; a taste | the diagram's colours are global in autox_nox.cpp, no view holds them; no command edits them | a preference | move to preferences, with the other colour rows (Q3) |

### plot (32 rows)

| # | key (label) | XPPAUT's name, demos using it | read by (core/) | would a user set it | repeats | whose | proposal |
|---|---|---|---|---|---|---|---|
| 81 | `colormap` (Colormap) | [COLORMAP](../../reference/xppaut-8.0/load_eqn.c#L1349) (8/218) | graf_par.cpp:867; json_windows.cpp; load_eqn.cpp | rarely: 8 demos; array plots | `plot.set_colormap` writes the same member | a view's/the session's | keep |
| 82 | `curves` (Curves) | no @ option | load_eqn.cpp:162; graphics.cpp:221,238 | yes: XP/YP in about 50 demos each | `plot.set_variables`, `plot.add_curve` edit the open graph; the row is its start | the starting graph's | keep |
| 83 | `axes.kind` (Kind) | [AXES](../../reference/xppaut-8.0/load_eqn.c#L1466) (5/218) | load_eqn.cpp:169,176; graphics.cpp:234 | rarely: 5 demos | none; the open graph's own kind is `window` state | the starting graph's | keep |
| 84 | `axes.x_low` (X low) | [XLO](../../reference/xppaut-8.0/load_eqn.c#L1697) (54/218) | load_eqn.cpp:170; graphics.cpp:253,271 | yes: the most set plot option (54/218 demos; 49 repo `.odex` files) | the same quantity as `axes.x_min`..`y_max` in a 2D plot ([load_eqn.c:681-686](../../reference/xppaut-8.0/load_eqn.c#L681) copies XLO..YHI over x_3d when AXES<5; XMIN also sets XLO, [load_eqn.c:1755](../../reference/xppaut-8.0/load_eqn.c#L1755)); `window.set_bounds` edits the open graph | the starting graph's | merge into `axes.x_min`..`y_max` (Q1) |
| 85 | `axes.x_high` (X high) | [XHI](../../reference/xppaut-8.0/load_eqn.c#L1714) (72/218) | load_eqn.cpp:172; graphics.cpp:255,273 | yes: the most set plot option (72/218 demos; 49 repo `.odex` files) | the same quantity as `axes.x_min`..`y_max` in a 2D plot ([load_eqn.c:681-686](../../reference/xppaut-8.0/load_eqn.c#L681) copies XLO..YHI over x_3d when AXES<5; XMIN also sets XLO, [load_eqn.c:1755](../../reference/xppaut-8.0/load_eqn.c#L1755)); `window.set_bounds` edits the open graph | the starting graph's | merge into `axes.x_min`..`y_max` (Q1) |
| 86 | `axes.y_low` (Y low) | [YLO](../../reference/xppaut-8.0/load_eqn.c#L1705) (63/218) | load_eqn.cpp:171; graphics.cpp:254,272 | yes: the most set plot option (63/218 demos; 49 repo `.odex` files) | the same quantity as `axes.x_min`..`y_max` in a 2D plot ([load_eqn.c:681-686](../../reference/xppaut-8.0/load_eqn.c#L681) copies XLO..YHI over x_3d when AXES<5; XMIN also sets XLO, [load_eqn.c:1755](../../reference/xppaut-8.0/load_eqn.c#L1755)); `window.set_bounds` edits the open graph | the starting graph's | merge into `axes.x_min`..`y_max` (Q1) |
| 87 | `axes.y_high` (Y high) | [YHI](../../reference/xppaut-8.0/load_eqn.c#L1722) (66/218) | load_eqn.cpp:173; graphics.cpp:256,274 | yes: the most set plot option (66/218 demos; 49 repo `.odex` files) | the same quantity as `axes.x_min`..`y_max` in a 2D plot ([load_eqn.c:681-686](../../reference/xppaut-8.0/load_eqn.c#L681) copies XLO..YHI over x_3d when AXES<5; XMIN also sets XLO, [load_eqn.c:1755](../../reference/xppaut-8.0/load_eqn.c#L1755)); `window.set_bounds` edits the open graph | the starting graph's | merge into `axes.x_min`..`y_max` (Q1) |
| 88 | `axes.x_min` (X min) | [XMIN](../../reference/xppaut-8.0/load_eqn.c#L1755) (5/218) | graphics.cpp:241; load_eqn.cpp:170,172; graf_par.cpp:359 | rarely: 5 demos (3D) | the 3D box in a 3D plot, the 2D window in a 2D one (see `axes.x_low`); `window.set_bounds` | the starting graph's | keep, and take x_low..y_high (Q1) |
| 89 | `axes.x_max` (X max) | [XMAX](../../reference/xppaut-8.0/load_eqn.c#L1730) (5/218) | graphics.cpp:241; load_eqn.cpp:172 | rarely: 5 demos (3D) | the 3D box in a 3D plot, the 2D window in a 2D one (see `axes.x_low`); `window.set_bounds` | the starting graph's | keep, and take x_low..y_high (Q1) |
| 90 | `axes.y_min` (Y min) | [YMIN](../../reference/xppaut-8.0/load_eqn.c#L1769) (5/218) | graphics.cpp:242; load_eqn.cpp:171 | rarely: 5 demos (3D) | the 3D box in a 3D plot, the 2D window in a 2D one (see `axes.x_low`); `window.set_bounds` | the starting graph's | keep, and take x_low..y_high (Q1) |
| 91 | `axes.y_max` (Y max) | [YMAX](../../reference/xppaut-8.0/load_eqn.c#L1739) (5/218) | graphics.cpp:242; load_eqn.cpp:173 | rarely: 5 demos (3D) | the 3D box in a 3D plot, the 2D window in a 2D one (see `axes.x_low`); `window.set_bounds` | the starting graph's | keep, and take x_low..y_high (Q1) |
| 92 | `axes.z_min` (Z min) | [ZMIN](../../reference/xppaut-8.0/load_eqn.c#L1782) (5/218) | graphics.cpp:243,246 | rarely: 5 demos (3D) | `window.set_bounds` | the starting graph's | keep |
| 93 | `axes.z_max` (Z max) | [ZMAX](../../reference/xppaut-8.0/load_eqn.c#L1747) (5/218) | graphics.cpp:243,246 | rarely: 5 demos (3D) | `window.set_bounds` | the starting graph's | keep |
| 94 | `axes.phi` (Phi) | [PHI](../../reference/xppaut-8.0/load_eqn.c#L1681) (2/218) | graphics.cpp:337,350 | rarely: 2 demos | the 3D view's angles; `window.*` edits the open graph | the starting graph's | keep |
| 95 | `axes.theta` (Theta) | [THETA](../../reference/xppaut-8.0/load_eqn.c#L1689) (1/218) | graphics.cpp:336,350 | rarely: 1 demo | same | the starting graph's | keep |
| 96 | `line.draw` (Draw) | no @ option | graphics.cpp:195,299 | rarely: XPPAUT's LT, 2 demos | per-curve editing: `plot.edit_curve` edits an open curve; the row is only the style a new curve starts with | the starting graph's | keep |
| 97 | `line.dash` (Dash) | no @ option | graphics.cpp:195,299 | rarely | per-curve editing: `plot.edit_curve` edits an open curve; the row is only the style a new curve starts with | the starting graph's | keep |
| 98 | `line.width` (Width) | no @ option | graphics.cpp:195,299 | rarely | per-curve editing: `plot.edit_curve` edits an open curve; the row is only the style a new curve starts with | the starting graph's | keep |
| 99 | `line.marker` (Marker) | no @ option | graphics.cpp:195,299 | rarely | per-curve editing: `plot.edit_curve` edits an open curve; the row is only the style a new curve starts with | the starting graph's | keep |
| 100 | `line.marker_size` (Marker size) | no @ option | graphics.cpp:195,299 | rarely | per-curve editing: `plot.edit_curve` edits an open curve; the row is only the style a new curve starts with | the starting graph's | keep |
| 101 | `nullclines.mesh` (Nullcline mesh) | [NMESH](../../reference/xppaut-8.0/load_eqn.c#L1499) (5/218) | nullcline.cpp:314,778; commands_nullcline.cpp:78 | rarely: NMESH 5 demos | `nullcline.set_mesh` writes the same member; also `saved` in a .setx | the model's | keep |
| 102 | `nullclines.draw` (Draw) | no @ option | nullcline.cpp:512 (batch: do_batch_nclines) | rarely: NCDRAW 3 demos | `nullcline.*` commands draw on request | the model's (a batch run's) | keep (batch only, Q6) |
| 103 | `nullclines.x_color` (X color) | [XNC](../../reference/xppaut-8.0/load_eqn.c#L1267) (0/218) | nullcline.cpp:165,313,755 | rarely: 0 demos; a taste | none | a preference | move to preferences, with the other colour rows (Q3) |
| 104 | `nullclines.y_color` (Y color) | [YNC](../../reference/xppaut-8.0/load_eqn.c#L1255) (0/218) | nullcline.cpp:165,313,755 | rarely: 0 demos; a taste | none | a preference | move to preferences, with the other colour rows (Q3) |
| 105 | `direction_field.grid` (Grid) | [DFGRID](../../reference/xppaut-8.0/load_eqn.c#L2405) (3/218) | nullcline.cpp:552,570,619; commands_dirfield.cpp | rarely: 3 demos | the `dirfield.*` commands take a grid argument | the starting graph's | keep |
| 106 | `direction_field.draw` (Draw) | [DFDRAW](../../reference/xppaut-8.0/load_eqn.c#L2413) (3/218) | nullcline.cpp:528,551 (batch: do_batch_dfield) | rarely: 3 demos | `dirfield.*` commands draw on request | the model's (a batch run's) | keep (batch only, Q6) |
| 107 | `manifolds.stable_color` (Stable color) | [SMC](../../reference/xppaut-8.0/load_eqn.c#L1281) (0/218) | equilibria.cpp:271,323,395 | rarely: 0 demos; a taste | none | a preference | move to preferences, with the other colour rows (Q3) |
| 108 | `manifolds.unstable_color` (Unstable color) | [UMC](../../reference/xppaut-8.0/load_eqn.c#L1295) (0/218) | equilibria.cpp:254,305,389 | rarely: 0 demos; a taste | none | a preference | move to preferences, with the other colour rows (Q3) |
| 109 | `color_by.enabled` (Enabled) | [COLORIZE](../../reference/xppaut-8.0/load_eqn.c#L2438) (0/218) | nullcline.cpp:523 (set_colorization_stuff, at load: json_model.cpp:59, xpp_batch.cpp:304) | rarely: 0 demos | the open graph's colour-by state; `dirfield.colorize` is a different thing | the starting graph's | keep |
| 110 | `color_by.via` (Via) | [COLORVIA](../../reference/xppaut-8.0/load_eqn.c#L2431) (0/218) | nullcline.cpp:523 | rarely: 0 demos | same | the starting graph's | keep |
| 111 | `color_by.low` (Low) | [COLORLO](../../reference/xppaut-8.0/load_eqn.c#L2445) (0/218) | nullcline.cpp:523 | rarely: 0 demos | same | the starting graph's | keep |
| 112 | `color_by.high` (High) | [COLORHI](../../reference/xppaut-8.0/load_eqn.c#L2452) (0/218) | nullcline.cpp:523 | rarely: 0 demos | same | the starting graph's | keep |

### analysis (12 rows)

| # | key (label) | XPPAUT's name, demos using it | read by (core/) | would a user set it | repeats | whose | proposal |
|---|---|---|---|---|---|---|---|
| 113 | `histogram.low` (Low) | [HISTLO](../../reference/xppaut-8.0/load_eqn.c#L2291) (0/218) | commands_analysis.cpp:85; histogram.cpp:164 | rarely: 1 demo (HIST*) | the dialog's last answer: the analysis commands write the same `s.histogram` members (commands_analysis.cpp:83-143) | the analysis's last arguments | keep (Q8) |
| 114 | `histogram.high` (High) | [HISTHI](../../reference/xppaut-8.0/load_eqn.c#L2300) (0/218) | commands_analysis.cpp:86; histogram.cpp:164 | rarely: 1 demo (HIST*) | the dialog's last answer: the analysis commands write the same `s.histogram` members (commands_analysis.cpp:83-143) | the analysis's last arguments | keep (Q8) |
| 115 | `histogram.bins` (Bins) | [HISTBINS](../../reference/xppaut-8.0/load_eqn.c#L2309) (0/218) | commands_analysis.cpp:84; histogram.cpp:177 | rarely: 1 demo (HIST*) | the dialog's last answer: the analysis commands write the same `s.histogram` members (commands_analysis.cpp:83-143) | the analysis's last arguments | keep (Q8) |
| 116 | `histogram.column` (Column) | [HISTCOL](../../reference/xppaut-8.0/load_eqn.c#L2318) (0/218) | commands_analysis.cpp:83; histogram.cpp:191 | rarely: 1 demo (HIST*) | the dialog's last answer: the analysis commands write the same `s.histogram` members (commands_analysis.cpp:83-143) | the analysis's last arguments | keep (Q8) |
| 117 | `histogram2.low` (Low) | [HISTLO2](../../reference/xppaut-8.0/load_eqn.c#L2328) (0/218) | commands_analysis.cpp:102; histogram.cpp:193 | rarely: 1 demo (HIST*) | the dialog's last answer: the analysis commands write the same `s.histogram` members (commands_analysis.cpp:83-143) | the analysis's last arguments | keep (Q8) |
| 118 | `histogram2.high` (High) | [HISTHI2](../../reference/xppaut-8.0/load_eqn.c#L2337) (0/218) | commands_analysis.cpp:103; histogram.cpp:193 | rarely: 1 demo (HIST*) | the dialog's last answer: the analysis commands write the same `s.histogram` members (commands_analysis.cpp:83-143) | the analysis's last arguments | keep (Q8) |
| 119 | `histogram2.bins` (Bins) | [HISTBINS2](../../reference/xppaut-8.0/load_eqn.c#L2346) (0/218) | commands_analysis.cpp:99; histogram.cpp:177 | rarely: 1 demo (HIST*) | the dialog's last answer: the analysis commands write the same `s.histogram` members (commands_analysis.cpp:83-143) | the analysis's last arguments | keep (Q8) |
| 120 | `histogram2.column` (Column) | [HISTCOL2](../../reference/xppaut-8.0/load_eqn.c#L2355) (0/218) | commands_analysis.cpp:97; histogram.cpp:191 | rarely: 1 demo (HIST*) | the dialog's last answer: the analysis commands write the same `s.histogram` members (commands_analysis.cpp:83-143) | the analysis's last arguments | keep (Q8) |
| 121 | `spectrum.column` (Column) | [SPECCOL](../../reference/xppaut-8.0/load_eqn.c#L2366) (0/218) | markov.cpp:421; histogram.cpp:565 | rarely: 0 demos | same: the analysis commands write the same `s.histogram` members (commands_analysis.cpp:83-143) | the analysis's last arguments | keep (Q8) |
| 122 | `spectrum.column2` (Column2) | [SPECCOL2](../../reference/xppaut-8.0/load_eqn.c#L2376) (0/218) | histogram.cpp:566,585 | rarely: 0 demos | same: the analysis commands write the same `s.histogram` members (commands_analysis.cpp:83-143) | the analysis's last arguments | keep (Q8) |
| 123 | `spectrum.width` (Width) | [SPECWIDTH](../../reference/xppaut-8.0/load_eqn.c#L2386) (0/218) | histogram.cpp:539,559,566 | rarely: 0 demos | same: the analysis commands write the same `s.histogram` members (commands_analysis.cpp:83-143) | the analysis's last arguments | keep (Q8) |
| 124 | `spectrum.window` (Window) | [SPECWIN](../../reference/xppaut-8.0/load_eqn.c#L2395) (0/218) | histogram.cpp:539,565 | rarely: 0 demos | same: the analysis commands write the same `s.histogram` members (commands_analysis.cpp:83-143) | the analysis's last arguments | keep (Q8) |

### export (3 rows)

| # | key (label) | XPPAUT's name, demos using it | read by (core/) | would a user set it | repeats | whose | proposal |
|---|---|---|---|---|---|---|---|
| 125 | `format` (Format) | [PLOTFMT](../../reference/xppaut-8.0/load_eqn.c#L1220) (0/218) | json_silent.cpp:156; many_pops.cpp:291,299 (batch picture name and format only) | rarely: batch only; interactive export names its format in the command | `plot.export_pdf`/`_svg` choose per call | the model's (a batch run's) | keep (batch only, Q6) |
| 126 | `color` (Color) | [PS_COLOR](../../reference/xppaut-8.0/load_eqn.c#L2177) (0/218) | image_format.cpp:50; many_pops.cpp:304; diagram.cpp:236 | rarely: 0 demos (PS_COLOR) | the Export picture dialog edits the same member | a preference | move to preferences (Q4) |
| 127 | `font_size` (Font size) | [PS_FSIZE](../../reference/xppaut-8.0/load_eqn.c#L2168) (0/218) | image_format.cpp:51,54 | rarely: 0 demos (PS_FSIZE) | same | a preference | move to preferences (Q4) |

### interface (3 rows)

| # | key (label) | XPPAUT's name, demos using it | read by (core/) | would a user set it | repeats | whose | proposal |
|---|---|---|---|---|---|---|---|
| 128 | `sliders` (Sliders) | no @ option | commands_value.cpp:95-111 (the `slider` command edits `s.sliders`); web2 | yes: a model's controls | the `slider` command adds and edits sliders at run time | model content (a UI the model offers) | move: a model statement, not an option (Q9) |
| 129 | `buttons` (Buttons) | [BUT](../../reference/xppaut-8.0/load_eqn.c#L1151) (1/218) | userbut.cpp:45 | rarely: BUT 1 demo | none; user buttons run commands | model content | move: a model statement, not an option (Q9) |
| 130 | `animation` (Animation) | no @ option | aniparse.cpp:842,1528 | rarely: 0 demos; XPPAUT names it on the command line | `animation` commands open one | model content (a companion file; already top level) | move: a model statement, not an option (Q9) |

### output (4 rows)

| # | key (label) | XPPAUT's name, demos using it | read by (core/) | would a user set it | repeats | whose | proposal |
|---|---|---|---|---|---|---|---|
| 131 | `data_file` (Data file) | [OUTPUT](../../reference/xppaut-8.0/load_eqn.c#L1813) (0/218) | json_silent.cpp:102,124; comline.cpp:248 | rarely: OUTPUT 1 demo (`-outfile` is the usual) | the command line's -outfile | batch only: the model's batch run | keep (batch only, Q6) |
| 132 | `nullclines_file` (Nullclines file) | no @ option | nullcline.cpp:512-518; commands_nullcline.cpp:121 | rarely: NCDRAW=2 in 3 demos | the `nullcline.export` command | batch only | keep (batch only, Q6) |
| 133 | `after_run` (After run) | [POSTPROCESS](../../reference/xppaut-8.0/load_eqn.c#L2282) (0/218) | json_silent.cpp:71,81,136; histogram.cpp | rarely: POSTPROCESS 1 demo | none | batch only | keep (batch only, Q6) |
| 134 | `after_run_file` (After run file) | no @ option | json_silent.cpp:140-148 | rarely: ours | none | batch only | keep (batch only, Q6) |
## The 45 import-only rows

Read from an XPPAUT `.ode`, `.xpprc` or the command line, refused in a `.odex`. All are proposed **import-only, as they are**; none is shown. Demos: how many of the 218 set it.

| rows | what | demos | note |
|---|---|---|---|
| `QUIET`, `LOGFILE` | XPPAUT's log level and file | 0, 0 | dropped with a warning: the log is the user's (preferences) |
| `AUTOEVAL` | every table's auto evaluation | 9 | written into each `table` statement by `--convert` ([load_eqn.c:2097](../../reference/xppaut-8.0/load_eqn.c#L2097)) |
| `MAXSTOR` | the rows the store starts with | 10 | ignored: the store grows ([load_eqn.c:1527](../../reference/xppaut-8.0/load_eqn.c#L1527)) |
| `STOCH` | mean or variance of a sweep's runs | 0 | becomes `after_run` |
| `BOUND` | blow-up and search bound | 33 | becomes `state_limit` and `search_limit` (Q7) |
| `LT` | line type | 2 | becomes `line.draw`/`marker`/`marker_size` |
| `PS_FONT`, `PS_LW` | PostScript font and line width | 0, 0 | ignored: a PDF has Helvetica, each curve its own width |
| `NCDRAW` | nullclines: draw, save | 3 | becomes `nullclines.draw` and `nullclines_file` |
| `XP`, `YP`, `ZP`, `NPLOT`, `XP2`-`ZP8` (6 rows) | plotted variables | 46, 50, 1, 6, 5 | the converter writes `curves` |
| `MULTIWIN`, `SIMPLOT`, `XLO2`-`YHI8` (4 rows) | several starting graphs | 0 | dropped with a warning: one graph opens (W322) |
| `S1`-`S3`, `SLO1`-`SLO3`, `SHI1`-`SHI3` | slider slots | 0 | become `sliders` (Q9) |
| `BELL`, `TUTORIAL`, `BIGFONT`, `SMALLFONT`, `FORECOLOR`, `BACKCOLOR`, `MWCOLOR`, `DWCOLOR`, `GRADS`, `BACKIMAGE`, `WIDTH`, `HEIGHT`, `BACK` | the X11 window's bell, fonts, colours, image, size | 1 (`BELL`), 0 for the rest | accepted and not kept |

One more observation for the per-row table: 34 of the 134 rows have no `@` name in XPPAUT (they are xppautX's own, or set through XPPAUT's menus only: `euler.*`, `bvp.*`, the 8 advanced autoX settings, `jacobian.kind`, `storage_precision`, `stop_at_limit`, `subsystem_split`, `follow_all_branches` and the like); all are read and all are proposed to stay.
