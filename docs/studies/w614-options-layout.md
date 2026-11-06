# W614: the options' layout and structure

Proposal for the maintainer (2026-11-06), from the audit of the option table
(`core/model_options.cpp`, 134 grouped rows) against the page. The rule it serves
(AGENTS.md, Code quality): every setting has its key in its `.odex` group and a
place in the UI, the UI grouped, named and ordered as the syntax.

## What is wrong today

1. **Two vocabularies in one table.** A row's `.odex` key is its name
   lowercased. 49 grouped rows have readable names (`total_time`,
   `poincare_map`, `mesh_intervals`); 85 are one word, and about 70 of those
   keep XPPAUT's abbreviation (`xp`, `nplot`, `dfdraw`, `sec`, `histbins2`,
   `rangeover`, `autoxmax`, `postprocess`).
2. **Numbers that stand for choices**, against "a choice is named": `DFDRAW`
   (0-5), `POSTPROCESS`, `COLORIZE`, `STOCH`, `AUTOEVAL`, `SIMPLOT`/`MULTIWIN`
   (any value means on), `QUIET` (0/1).
3. **One panel derived, the rest hand-written.** Only Numerics is built from the
   table (by key, ignoring the group, so it also shows equilibria rows,
   `nullcline_mesh` and five settings that are no row: `eul_tol`, `eul_iter`,
   `bvp_*`). Poincare, equilibria, Range, histogram, plot, export and autoX's
   Numerics are hand-typed argument lists with their own order; autoX's has two
   more copies (`autox_num_keys`, web2's `NUM_KEYS`) and 10 settings with no row.
4. **Panel names are not the group names**: continuation shows as "Numerics" and
   "autoX settings", diagram as "Axes", sweep as "Range"; output has no place.
5. **The table is out of group order** (`NULLCLINE_MESH` among integration,
   `AUTOEVAL` among integration, `NEARTOL`, `SEED`, ...).
6. **No check** ties a row to a UI place or an order.

## Proposed structure

**One row, one setting.** Every row has a snake_case key; XPPAUT's spelling is
only an import name (`xppaut_names`). Every choice a named choice, every yes/no
a boolean. A row carries what every place needs: group, key, kind, label
(derived from the key), choices, default, rule, one-line help. The manual,
`help options`, the JSON schema and each UI panel are generated from the rows.

**Groups are panels, sections are sub-keys.** A group is one UI panel with the
group's name as its title. Where a group is large, a dotted sub-key is a section
inside the panel (the `.odex` already reads `nullclines.draw`). The table is
sorted by group, then section, then the order the panel shows; the UI never
re-sorts.

**Groups, in this order** (workflow order: run, analyse the run, continue,
draw, write):

| # | group | panel (title = group) | sections and rows |
|---|---|---|---|
| 1 | `integration` | Integration (Numerics menu) | method, total_time, start_time, dt, store_every, store_from, storage_precision (W613), state_limit, stop_at_limit; `step.` relative/absolute tolerance, min, max; `jacobian.` kind, band_upper, band_lower; `delay.` max_delay, history_points (VMAXPTS); `euler.` tolerance, iterations; `bvp.` max_iterates, tolerance, epsilon; subsystem_split; seed |
| 2 | `poincare` | Poincaré | map, variable, sign, plane, stop |
| 3 | `equilibria` | Equilibria | max_iterations, newton_tolerance, jacobian_step, search_limit, print_eigenvalues, draw_invariant_sets, draw_strong_sets |
| 4 | `sweep` | Sweep (Range) | variable, low, high, steps, reset, old_ic, write_files; second variable's four |
| 5 | `continuation` | Continuation (autoX's Numerics) | every autoX numerics setting, the 10 without a row included; `autox_num_keys` and web2's copy go |
| 6 | `diagram` | Diagram (autoX's view) | plot, variable, par1, par2, x/y min/max; `colors.` stable/unstable equilibrium, stable/unstable periodic (SEC, UEC, SPC, UPC: one source with the view's own colours) |
| 7 | `plot` | Graph (each graph's Settings) | `axes.` x, y, z, ranges, 3D angles, kind; `curves` (the list replacing NPLOT and XP2-8/YP/ZP); `line.` default draw, dash, width, marker; `nullclines.` mesh, x/y colour, draw; `direction_field.` grid, draw (named); `manifolds.` stable/unstable colour; `color_by` (one named choice replacing COLORIZE) with via, low, high; colormap |
| 8 | `analysis` | Analysis | `histogram.` column, low, high, bins; `histogram2.` same; `spectrum.` column, column2, width, window |
| 9 | `export` | Picture (File > Export picture) | format, color, font_size |
| 10 | `interface` | Interface | sliders, buttons, animation |
| 11 | `output` | Output (what a run writes by itself) | data_file (OUTPUT), after_run (one named choice replacing POSTPROCESS: none, histogram, spectrum, ...), nullclines_file, log_file, log_level (replacing QUIET) |

Moves: `sweep` comes up beside the runs (it was after export); `nullcline_mesh`
goes to `plot.nullclines`; `AUTOEVAL` leaves `diagram`.

## Options proposed for removal (import-only or dropped)

| option | today | proposal |
|---|---|---|
| `STOCH` | an internal flag markov.cpp sets during a stochastic run (`s.stochastic.flag`); a model setting it breaks nothing only by luck | drop: not a setting |
| `STORAGE_ROWS` (MAXSTOR) | the rows the store starts with; it grows anyway | import-only, ignored |
| `SIMPLOT`, `MULTIWIN` | window layout, any value means on | import-only: windows are session state (W322) |
| `NPLOT`, `XP2-8`, `YP2-8`, `ZP2-8`, `XLO2-8`... | XPPAUT's numbered curves and windows | import-only, converted to `plot.curves` and the default graphs |
| `AUTOEVAL` | sets every table's auto-evaluation; filed under diagram | a table's own property (its `table` statement), import-only as an option |
| `NEARTOL` | read at parse time into formulas calling `near()`; nothing at run time | a model-level tolerance, not a run setting: kept in the model's statements, out of the panels |

## Decided (maintainer, 2026-11-06; #684)

All six questions below were settled as proposed, with these changes:
- `plot.curves` holds the axes variables as its first curve; `axes` keeps
  only the ranges and the 3D view (no `axes.x/y/z`: one source).
- `output` is what a model's run writes by itself (`data_file`,
  `nullclines_file`, `after_run`, `after_run_file`), one path rule for all
  (OUTPUT's: relative to the model's folder, outside it asked), each with a
  file picker in the Output panel; the log (`log_level`, `log_file`) moves
  to preferences.
- Several starting graphs are not built; XPPAUT's `multiwin`, `simplot`,
  `xlo2-8` import with a warning that one graph opens.
- Checks: a `static_assert` on the table's order, optioncheck (key, label,
  help on every grouped row; no option-key literal in web2/src), a
  web2check `options` section.

Implementation: W615 (the table, #685), then W616 (the panels, #686).

## Open questions (as asked)

1. The structure: groups as panels, dotted sections, the table generating every panel, the order above.
2. The renames: every row gets a snake_case key; XPPAUT names import-only (no alias kept, per "One name per thing").
3. The removals above, one by one.
4. `output`: a panel in the window (File > Output) for what `--silent` writes, or preferences?
5. `curves` and default graphs in a model's `options` (the model's starting layout), while the live windows stay the session's (W322).
6. The check: a source check that every grouped row has a UI place, generated forms only, and the table in group order.
