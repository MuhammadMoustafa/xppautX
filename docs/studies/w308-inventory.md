# W308: Inventory of Every Operation for the Dotted Command Convention

Stage 0 study for card [W352 / #413](https://github.com/MuhammadMoustafa/xppautX/issues/413) (2026-10-14),
implementing the convention decided in [issue #367](https://github.com/MuhammadMoustafa/xppautX/issues/367),
[Discussion #415](https://github.com/MuhammadMoustafa/xppautX/discussions/415), and [docs/command-design.md](../command-design.md).

**Approved by the maintainer 2026-10-27, after W502's refresh, the annotation split included: W353 builds on it.**

**Refreshed 2026-10-27 (W502).** Every row of section 2 was audited against
the code, not only the roadmap: `core/command_table.h`, `core/menus.cpp`, the
command handlers, `ui_json.cpp`'s dispatch table (every protocol command now
has a row) and `docs/protocol.md`. Of the 220 rows of the 2026-10-14
inventory, 85 were right, 134 were corrected, 1 was removed and 27 were
added (246 now); 6 proposed names changed (listed below). The audit found:

- **Stale file:line links.** About 60 links pointed at the wrong line after
  the tree moved; every one now points at the line it names (the form's
  title, the ask, the handler).
- **Wrong protocol cells.** The browser's operations are window keys
  (`{"cmd":"key","win":"browser","key":"f"}`), not `{"cmd":"browser","op":"find"}`;
  `click` selects a plot window and is not a mouse answer (a mouse, rubber
  or drag *ask* is answered with `xd`, `yd`); `auto` has no `clear` or
  `redraw` op (they are the AUTO window's keys `c` and `d`); `set` of the
  numerics uses the keys `max_iterations`, `newton_tolerance`,
  `jacobian_step`, `search_limit`, `nullcline_mesh`, `bvp_*` (W484), not
  `singpt_*`.
- **Wrong or missing arguments.** Double range has `Vary both`
  (`together` or `independently`), not a curve/array mode; Range
  Equilibria has a stability column, not a colour; Axes options set the
  axes' origins, not labels (labels are in the Axes 2D/3D grid form); `3`
  sets the perspective and angles, the box is the Axes 3D form; zoom out is
  a rubber box or a click, not a factor; Poincare map's direction is
  `negative`, `both`, `positive`; marker shapes are `box`, `diamond`,
  `triangle`, `plus`, `cross`, `circle`; set scopes are `all`, `parameters`,
  `initial` and the setting groups (`integration`, `poincare`, `equilibria`,
  `continuation`, `plot`, `sweep`, `analysis`); AUTO's plot types are the
  names `autosettings.axes.plot` uses; Tabulate asks a variable and an
  x interval; Transpose, Fit, Periodic BCs, Edit arrayplot, Array range
  saving and Frame saving had other fields than the rows said; the
  Liapunov, stAutocor, Xcorrel and spEc.dns rows were mapped to the wrong
  asks; Adjoint Range only sets a flag; No show and Show run the solver.
- **Removed.** Boundary-value `Homoclinic` (the menu has four items).
  Rows that mixed up what an operation does were corrected, not removed:
  AUTO Clear hides earlier branches (it is not a reset), `Clear grab` is
  AUTO File `c`, Arrow is a head only and Pointer a head and a shaft
  (neither is a flow arrow).
- **Added (27).** The protocol and menu operations that had no row:
  `run.old`, `run.configure` (Total, Start time, Transient, Dt, Method,
  Store every, Bounds, Delay), `window.select`, `window.set_display`,
  `plot.set_variables`, `plot.set_shifted_axes` (rUelle plot), twelve AUTO
  rows (`zoom`, `zoom_out`, `fit`, `default_view`, `scroll`,
  `use_last_plot`, `new_view`, `close_view`/`activate_view`, `set_display`,
  `pick`, `close`, `set_threads`), the animation's pause, speed, step,
  seek and close, `array_plot.scroll`/`close`, `value.save_file`/`load_file`,
  `recording.seek`/`set_step_note`, `tool.run_user_button` and
  `tool.run_source_action`.
- **Renamed (6).** `annotation.add_flow_arrow` is `annotation.add_arrow`;
  `bvp.set_show_mode` (the No show and Show rows) is `bvp.solve` with
  `show`; `auto.clear_diagram` is `auto.hide_earlier`;
  `plot.set_axes_labels` is `plot.set_axes_origin`; `plot.set_3d_box` is
  `plot.set_3d` (the Axes form) and `plot.set_3d_view` (the `3` key);
  `browser.tabulate_expression` is `browser.tabulate_column`.
- **Moved, proposed and awaiting the maintainer's decision.** The
  coordinator recommends keeping annotations as their own resource,
  `annotation.*`: only what the user places (text labels, arrows, pointers,
  single markers), each with an id. Marks computed from data go under
  plotting: `plot.add_data_markers` (markers every N rows along a curve,
  moved here from `annotation`), equilibrium marks (made by the equilibrium
  commands, no command of their own) and frozen curves (already `plot.*`).
  Section 5.1 and question 2 of section 6 follow; if the maintainer
  decides otherwise, only these names change.
- **Taken into account since 2026-10-14.** PDF and SVG replace PostScript
  (W492); colours are quoted strings: a CSS name, `"foreground"` or a hex
  (W494's rule); ranges and ordered pairs (W494), named choices (W499),
  typed forms (W485, W487), stop messages (W498) and the terminal rule
  (W501). Today's remaining numbers-for-choices (`Side(0/1)`, `(1/0)` asks,
  `Y/N` text, `Spec type`, `Window type`) are marked in the rows as gaps for
  W353 to turn into booleans and named choices.

Docs found out of step with the code, left for their own card: the
protocol's Poincare `Direction` names (`increasing`/`decreasing`; the code
and this inventory use `negative`/`both`/`positive`), the AUTO window key `p`
that `menu_auto_window` no longer has, and the `diagram` field of `data` (the
opt-out of AUTO diagram delivery), which `docs/protocol.md` does not
describe. The `M_BH` item (boundary-value Homoclinic) is still in
`menudrive.h` with no menu entry.

This is an inventory and a proposal, not an implemented registry. The key
and menu columns record the current legacy menus, which will change; they
are not design inputs. The existing protocol still uses numeric window IDs
and some numeric choices; the proposed registry uses named windows and
choices. A `{"cmd":"key","key":"x"}` in a row is the last key of the path in the
Key Path column, sent after the keys before it (the menus ask and the keys
answer them); `{"cmd":"key","menu":"main","item":ID}` names a command by its
`hello.command_table` id and needs no path. Main-window keys have no
`win: main`; AUTO File rows are keys of the File pop-up opened by `f`. Consult
[the protocol](../protocol.md#commands-client-to-server) for exact current
fields. Proposed dotted names, result payloads and dotted events in the
right-hand columns are design suggestions, not today's wire contract.

## 1. Context and Convention Rules

On 2026-10-14, the maintainer approved the unified command design for xppautX:
1. **One command shape**: Every operation is a registry command named `resource.verb` in snake_case (e.g. `run.initial`, `auto.grab`, `window.create`).
2. **Unified protocol**: JSON Lines everywhere (`{"cmd":"auto.grab","label":12}`). For `--server` text input, a tokenizer parses `--flag value` lines into the same registry validator (`auto.grab --label 12`). Positional arguments are rejected.
3. **Data types**: Strict typed arguments (`integer`, `real`, `boolean`, `choice`, `text`, `expression`, `name`, `file`, `path`, `color`, `window`, `list`, `map`). No `null` and no deep nesting.
4. **Colours**: A colour is a quoted string: a CSS name, `"foreground"` (theme-aware), `"#rrggbb"` or `"#rrggbbaa"`. Numeric colour indices are read on XPPAUT import only.
5. **Windows by name**: Windows are named strings (`graph_1`, `diagram_1`, `"phase plane"`). Window numbers on the wire are eliminated.
6. **Results and events**: Every command completes with a single `done` event containing a typed `result` data payload (replacing text-only `idle`). Events follow `resource.event` naming (`window.created`, `plot.series`, `auto.diagram`, `session.state`). Heavy data streams are managed via `events.subscribe`.
7. **Forms and dialogs as arguments**: Interactive dialogs (`do_string_box_of`, `new_int`, etc.) and mouse operations (`get_mouse_xy`, `rubber_band`) become command arguments in data space rather than pixel queries or interactive blocking ask prompts. Today typed form asks and mouse/rubber/drag asks remain; the latter can already accept data coordinates. Core questions such as file selection and overwrite confirmation still need answers.
8. **Navigation removed from command table**: Legacy menu layer switching (`file`, `numerics`, `exit` in [core/command_table.h](../../core/command_table.h#L156-L158)) belongs strictly to front-end keymaps and is omitted from the command registry.

---

## 2. Comprehensive Inventory of Operations

### 2.1 Resource `run` (Integration, Trajectories, and Sweeps)

**Moved onto the registry (W521, #585): every row below is implemented under its proposed name; see docs/protocol.md's registry table. The "Today" columns describe the forms that were deleted.** Readings that differ from the row: `ic.mouse_stream` takes `x` and `y` lists (a list holds single values); `ic.formula` takes a state or an array range `u[1..5]` and `ic.array` the offset formula; `run.dae_guess` returns `guesses`; `bvp.solve` and `bvp.solve_periodic` take a required `show`; `bvp.solve_range`'s `side` is `start` or `end`; `run.configure`'s `method` is text the numerics owner checks; `run.abort` has no `done` of its own.

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `I` -> `G` | Initialconds -> (G)o | `{"cmd":"key","menu":"main","item":"run_initial"}` / main key sequence `i`, `g` | none | `run.initial` | none | default | `{"status":"completed","steps":integer,"time":real}` | `job.progress`, `plot.series`, `session.state`, `done` |
| `I` -> `L` | Initialconds -> (L)ast | `{"cmd":"key","menu":"main","item":"run_last"}` | none | `run.last` | none | default | `{"status":"completed","steps":integer,"time":real}` | `job.progress`, `plot.series`, `session.state`, `done` |
| `I` -> `O` | Initialconds -> (O)ld | `{"cmd":"key","key":"o"}` after `i` | none; runs from the stored initial values like Go without refreshing the DAE guesses; proposal: merged into `run.initial` | `run.old` | none | default | `{"status":"completed","steps":integer,"time":real}` | `job.progress`, `plot.series`, `session.state`, `done` |
| `C` | Continue integration | `{"cmd":"continue"}` (`extra`, `until`) | none | `run.continue` | `until`: optional real, `extra`: optional real | mutex(`until`, `extra`) | `{"status":"completed","end_time":real}` | `job.progress`, `plot.series`, `session.state`, `done` |
| Alt+S | (Page toolbar / search) | `{"cmd":"steady","decimals":N,"hold":H,"maximum":M}` | none | `run.steady` | `decimals`: integer (0..15), `hold`: real (>0), `maximum`: real (>0) | default | `{"status":"settled"\|"limit"\|"stopped"\|"failed","decimals":integer,"time":real}` | `job.progress`, `plot.series`, `session.state`, `done` |
| `I` -> `B` | Initialconds -> (B)ackward | `{"cmd":"key","key":"b"}` after `i` (the Initialconds menu's key; sets Dt negative for the run) | none | `run.backward` | none | default | `{"status":"completed","steps":integer,"time":real}` | `job.progress`, `plot.series`, `session.state`, `done` |
| `I` -> `S` | Initialconds -> (S)hift | `{"cmd":"key","key":"s"}` after `i`; an error "No prior solution" before a first run | none | `run.shift` | none | default | `{"status":"completed","steps":integer,"time":real}` | `job.progress`, `plot.series`, `session.state`, `done` |
| `I` -> `R` | Initialconds -> (R)ange | `{"cmd":"key","key":"r"}` | typed form "Range Integrate" ([core/integrate.cpp:290](../../core/integrate.cpp#L290), W487): Range over (a parameter or variable), Steps (integer >= 1), Start, End, Reset storage, Use old ic's, Cycle color, Movie, Write each run to its own file (booleans) | `run.range` | `variable`: name, `start`: real, `end`: real, `steps`: integer (>0), `reset_storage`: optional boolean, `use_old_ic`: optional boolean, `cycle_color`: optional boolean, `movie`: optional boolean, `write_each_run_to_file`: optional boolean | default | `{"runs_completed":integer,"total_steps":integer}` | `job.progress`, `plot.series`, `session.state`, `done` |
| `I` -> `2` | Initialconds -> (2)par range | `{"cmd":"key","key":"2"}` | typed form "Double Range Integrate" ([core/integrate.cpp:325](../../core/integrate.cpp#L325), W487): Vary1, Start1, End1, Vary2, Start2, End2, Steps, Reset storage, Use old ic's, Cycle color, Movie (booleans), Vary both (choice `together` or `independently`), Steps2, Write each run to its own file | `run.double_range` | `variable1`: name, `start1`: real, `end1`: real, `variable2`: name, `start2`: real, `end2`: real, `steps1`: integer (>0), `steps2`: integer (>0), `vary`: choice(`together`,`independently`), `reset_storage`: optional boolean, `use_old_ic`: optional boolean, `cycle_color`: optional boolean, `movie`: optional boolean, `write_each_run_to_file`: optional boolean | default | `{"grid_points":integer,"total_steps":integer}` | `job.progress`, `plot.series`, `session.state`, `done` |
| `I` -> `D` | Initialconds -> DAE guess | `{"cmd":"key","key":"d"}` | `new_string_of` per algebraic variable ([core/dae_fun.cpp:301](../../core/dae_fun.cpp#L301)) | `run.dae_guess` | `guesses`: optional map[name, expression] | default | `{"status":"solved"\|"failed","residuals":map[name, real]}` | `session.state`, `done` |
| Esc (during run) | Stop | `{"cmd":"abort"}` | none | `run.abort` | none | default | `{"stopped_at":real}` | `job.stopped`, `done` |
| `U` -> `T`/`S`/`R`/`D`/`M`/`O`/`B`/`E` | Numerics -> Total, Start time, Transient, Dt, Method, Store every N steps, Bounds, Delay | `{"cmd":"set","kind":"num","name":KEY,"value":V}` with the keys `total_time`, `start_time`, `store_from`, `dt`, `method`, `store_every`, `state_limit`, `stop_at_limit`, `delay`, `relative_tolerance`, `absolute_tolerance`, `min_step`, `max_step`, `eul_tol`, `eul_iter` (docs/protocol.md "The numerics as data") | numerics prompts, each range-checked in one place (W494: `total_time` above 0, `min_step` at most `max_step`, `dt` not 0); `method` and `jacobian` by name (W484) | `run.configure` | `total_time`: optional real (>0), `start_time`: optional real, `store_from`: optional real, `dt`: optional real (not 0), `method`: optional choice from `numerics.fields[method].choices`, `store_every`: optional integer (>=1), `state_limit`: optional real (>0), `stop_at_limit`: optional boolean, `delay`: optional real (>=0), `relative_tolerance`, `absolute_tolerance`, `min_step`, `max_step`, `eul_tol`: optional real, `eul_iter`: optional integer | default | `{"settings":map[string, real]}` | `numerics`, `session.state`, `done` |
| `I` -> `H` | Initialconds -> s(H)oot | `{"cmd":"key","key":"h"}` | `new_int` "Which? (1-N)" ([core/integrate.cpp:1130](../../core/integrate.cpp#L1130)); with no shooting data the command is an error ("No shooting data available"), an index out of range is "Out of range" | `run.shoot` | `manifold_index`: integer (>=1) | default | `{"status":"completed","steps":integer}` | `job.progress`, `plot.series`, `done` |

---

### 2.2 Resource `ic` (Initial Conditions)

**Moved onto the registry (W521, #585): every row below is implemented under its proposed name; see docs/protocol.md's registry table. The "Today" columns describe the forms that were deleted.** Readings that differ from the row: `ic.mouse_stream` takes `x` and `y` lists (a list holds single values); `ic.formula` takes a state or an array range `u[1..5]` and `ic.array` the offset formula; `run.dae_guess` returns `guesses`; `bvp.solve` and `bvp.solve_periodic` take a required `show`; `bvp.solve_range`'s `side` is `start` or `end`; `run.configure`'s `method` is text the numerics owner checks; `run.abort` has no `done` of its own.

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `I` -> `N` | Initialconds -> (N)ew | `{"cmd":"set","kind":"ic",...}` / `{"cmd":"set","values":[...]}` | string box per variable | `ic.set` | `values`: map[name, real] | default | `{"applied":map[name, real]}` | `session.state`, `done` |
| `I` -> `M` | Initialconds -> (M)ouse | a `mouse` ask ("Click on initial data"), answered with `{"cmd":"answer","xd":x,"yd":y}` (data coordinates) or pixels | `mouse_ask` "mouse" ([core/json_prompts.cpp:518](../../core/json_prompts.cpp#L518)): one click in the active 2D window; an error "Not in a usable 2D plane" when the window is not a phase plane of two states. The click sets the window's two variables only | `ic.at` | `coordinates`: map[name, real], `window`: optional window | default | `{"applied":map[name, real]}` | `plot.series`, `session.state`, `done` |
| `I` -> `I` | Initialconds -> m(I)ce | `{"cmd":"key","key":"i"}` | repeated `mouse` asks until the ask is cancelled; each click starts a run from that point. Same 2D-plane requirement as Mouse | `ic.mouse_stream` | `points`: list[map[name, real]], `window`: optional window | default | `{"trajectories_run":integer}` | `plot.series`, `session.state`, `done` |
| `I` -> `F` | Initialconds -> (F)ile | `{"cmd":"key","key":"f"}` | `file_selector` "Read initial data" "*.dat"; the file is read all or nothing (W125) | `ic.load` | `file`: file | default | `{"file":file,"applied":map[name, real]}` | `session.state`, `done` |
| `I` -> `U` | Initialconds -> form(U)la | `{"cmd":"key","key":"u"}` | `new_string_of` "Formula:" ([core/integrate.cpp:1270](../../core/integrate.cpp#L1270)) | `ic.formula` | `variable`: name, `formula`: expression | default | `{"variable":name,"formula":expression}` | `session.state`, `done` |
| (Data panel) | (Set row as IC) | `{"cmd":"key","win":"browser","key":"g","row":N}` | none | `ic.from_data_row` | `row`: integer (>=0) | default | `{"row":integer,"values":map[name, real]}` | `session.state`, `done` |
| (Model load) | (Array IC start) | (Automatic on model init) | `new_int` "Number elements:", `new_string_of` formula ([core/integrate.cpp:1395](../../core/integrate.cpp#L1395)) | `ic.array` | `name`: name, `size`: integer (>0), `formula`: expression | default | `{"name":name,"size":integer}` | `session.state`, `done` |

---

### 2.3 Resource `equilibrium` (Fixed Points and Stability)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `S` -> `G` | Equilibria -> (G)o | `{"cmd":"key","key":"g"}` | `TwoChoice` "Print eigenvalues?" ([core/equilibria.cpp:209](../../core/equilibria.cpp#L209)), then for a saddle "Draw Invariant Sets?" ([core/equilibria.cpp:231](../../core/equilibria.cpp#L231)) and "Draw Strong Sets?" ([core/equilibria.cpp:280](../../core/equilibria.cpp#L280)); YES/NO answers today, booleans in the proposal | `equilibrium.find` | `print_eigenvalues`: optional boolean, `draw_invariant_sets`: optional boolean, `draw_strong_sets`: optional boolean | default | `{"converged":boolean,"equilibrium":map[name, real],"eigenvalues":list[map[string, real]],"stability":choice("stable","unstable","saddle")}` | `plot.series`, `session.state`, `done` | **Moved onto the registry at W522 ([#586](https://github.com/MuhammadMoustafa/xppautX/issues/586)).**
| `S` -> `M` | Equilibria -> (M)ouse | a `mouse` ask ("Click on guess"), answered with `xd`, `yd` | `mouse` ask ([core/json_prompts.cpp:518](../../core/json_prompts.cpp#L518)): one click in the active 2D window ("Not in a usable 2D plane" otherwise); the click sets the window's two variables of the guess | `equilibrium.guess` | `coordinates`: map[name, real], `window`: optional window | default | `{"converged":boolean,"equilibrium":map[name, real],"eigenvalues":list[map[string, real]]}` | `plot.series`, `session.state`, `done` | **Moved onto the registry at W522 ([#586](https://github.com/MuhammadMoustafa/xppautX/issues/586)).**
| `S` -> `R` | Equilibria -> (R)ange | `{"cmd":"key","key":"r"}` | typed form "Range Equilibria" ([core/integrate.cpp:224](../../core/integrate.cpp#L224), W487): Range over (a parameter), Steps (>= 1), Start, End, Shoot, Stability col (integer), Movie, Monte Carlo (booleans except the column) | `equilibrium.range` | `parameter`: name, `start`: real, `end`: real, `steps`: integer (>0), `shoot`: optional boolean, `stability_column`: optional integer, `movie`: optional boolean, `monte_carlo`: optional boolean | default | `{"points_found":integer}` | `plot.series`, `done` | **Moved onto the registry at W522 ([#586](https://github.com/MuhammadMoustafa/xppautX/issues/586)).**
| `S` -> `C` | Equilibria -> monte(C)ar | `{"cmd":"key","key":"c"}` | `new_int` "Append(1/0", `new_int` "Shoot (1/0)", `new_int` "# Guesses:", `new_float` "Tolerance:", then `<var>_lo :` and `<var>_hi :` per variable ([core/integrate.cpp:356](../../core/integrate.cpp#L356)); the two (1/0) asks are numbers standing for yes/no | `equilibrium.search` | `append`: optional boolean, `guesses`: integer (>0), `tolerance`: real (>0), `shoot`: optional boolean, `bounds`: map[name, list[real, 2]] | default | `{"found":integer,"equilibria":list[map[name, real]]}` | `plot.series`, `session.state`, `done` | **Moved onto the registry at W522 ([#586](https://github.com/MuhammadMoustafa/xppautX/issues/586)).**
| `E` -> `I` | (Equilibrium window) -> (I)mport | `{"cmd":"key","win":"equilibrium","key":"i"}` | none | `equilibrium.import_as_ic` | none | default | `{"applied":map[name, real]}` | `session.state`, `done` | **Moved onto the registry at W522 ([#586](https://github.com/MuhammadMoustafa/xppautX/issues/586)).**
| (`--silent`, protocol) | (Write equilibrium to file) | `{"cmd":"equilibrium","op":"write","name":F,"shoot":N}` | none | `equilibrium.export` | `file`: file, `shoot`: optional boolean | default | `{"file":file,"written":boolean}` | `done` | **Moved onto the registry at W522 ([#586](https://github.com/MuhammadMoustafa/xppautX/issues/586)).**
| `U` -> `I` | Numerics -> sIng pt | `{"cmd":"set","kind":"num","name":KEY,"value":V}` with the keys `max_iterations`, `newton_tolerance`, `jacobian_step`, `search_limit` (W484 renamed the `singpt_*` names) | numerics prompts for the four values; each is range-checked in one place (W494) | `equilibrium.configure` | `max_iterates`: optional integer, `newton_tolerance`: optional real, `jacobian_epsilon`: optional real (key `jacobian_step`), `search_limit`: optional real | default | `{"settings":map[string, real]}` | `session.state`, `done` | **Moved onto the registry at W522 ([#586](https://github.com/MuhammadMoustafa/xppautX/issues/586)).**

---

W522 recheck: a delay model has no finite eigenvalue list, so the typed result returns `[]` while the existing event retains its absent eigenvalues. Range steps are intervals (the endpoints are both computed). `equilibrium.export` derives manifold output names from `file` and reports a failed branch through `done`. The existing mixed-direction saddle export can reach the integrator's `store_from` guard after its forward branches advance the shared clock; W522 keeps those numbers and returns the placed error, instead of announcing a successful write after showing the error. Resetting each branch clock needs a numerical follow-up by the coordinator; it is outside W522's unchanged-numerics requirement ([#586](https://github.com/MuhammadMoustafa/xppautX/issues/586)).

### 2.4 Resource `nullcline` (Nullclines and Invariant Curves)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `N` -> `N` | Nullclines -> (N)ew | `{"cmd":"key","key":"n"}` | none | `nullcline.compute` | none | default | `{"curves_computed":integer}` | `plot.series`, `done` | **Moved onto the registry at W522 ([#586](https://github.com/MuhammadMoustafa/xppautX/issues/586)).**
| `N` -> `R` | Nullclines -> (R)estore | `{"cmd":"key","key":"r"}` | none | `nullcline.redraw` | none | default | `{"drawn":boolean}` | `plot.series`, `done` | **Moved onto the registry at W522 ([#586](https://github.com/MuhammadMoustafa/xppautX/issues/586)).**
| `N` -> `A` | Nullclines -> (A)uto | `{"cmd":"key","key":"a"}` | none | `nullcline.auto_mode` | `enabled`: boolean | default | `{"auto_redraw":boolean}` (a per-window flag, `Nullrestore`) | `session.state`, `done` | **Moved onto the registry at W522 ([#586](https://github.com/MuhammadMoustafa/xppautX/issues/586)).**
| `N` -> `M` | Nullclines -> (M)anual | `{"cmd":"key","key":"m"}` | none | (Merged into `nullcline.auto_mode`) | `enabled`: false | default | `{"auto_redraw":boolean}` | `session.state`, `done` | **Moved onto the registry at W522 ([#586](https://github.com/MuhammadMoustafa/xppautX/issues/586)).**
| `N` -> `F` -> `F` | Freeze cline -> (F)reeze | `{"cmd":"key","key":"f"}` after `n`, `f` (nothing is frozen until Nullclines/New has computed) | none | `nullcline.freeze` | none | default | `{"frozen_curves_count":integer}` | `plot.series`, `done` | **Moved onto the registry at W522 ([#586](https://github.com/MuhammadMoustafa/xppautX/issues/586)).**
| `N` -> `F` -> `D` | Freeze cline -> (D)elete all | `{"cmd":"key","key":"d"}` | none | `nullcline.clear_frozen` | none | default | `{"cleared":boolean}` | `plot.series`, `done` | **Moved onto the registry at W522 ([#586](https://github.com/MuhammadMoustafa/xppautX/issues/586)).**
| `N` -> `F` -> `R` | Freeze cline -> (R)ange | `{"cmd":"key","key":"r"}` | `do_string_box_of` "Range Clines" ([core/nullcline.cpp:348](../../core/nullcline.cpp#L348)): Par, Start, End, Steps | `nullcline.range_freeze` | `parameter`: name, `start`: real, `end`: real, `steps`: integer (>0) | default | `{"steps_frozen":integer}` | `plot.series`, `done` | **Moved onto the registry at W522 ([#586](https://github.com/MuhammadMoustafa/xppautX/issues/586)).**
| `N` -> `F` -> `A` | Freeze cline -> (A)nimate | `{"cmd":"key","key":"a"}` | `new_int` "Delay (msec)" ([core/nullcline.cpp:499](../../core/nullcline.cpp#L499)) | `nullcline.animate` | `delay_ms`: optional integer (>=0) | default | `{"animated":boolean}` | `done` | **Moved onto the registry at W522 ([#586](https://github.com/MuhammadMoustafa/xppautX/issues/586)).**
| `N` -> `S` | Nullclines -> (S)ave | `{"cmd":"key","key":"s"}` | `file_selector` "Save nullclines" "*.dat" ([core/nullcline.cpp:476](../../core/nullcline.cpp#L476)), and `TwoChoice` "Save Frozen Clines?" ([core/nullcline.cpp:162](../../core/nullcline.cpp#L162)) | `nullcline.export` | `file`: file, `format`: optional choice from the supported export formats | default | `{"file":file,"points_written":integer}` | `done` | **Moved onto the registry at W522 ([#586](https://github.com/MuhammadMoustafa/xppautX/issues/586)).**
| `U` -> `N` | Numerics -> Nullcline mesh | `{"cmd":"set","kind":"num","name":"nullcline_mesh","value":N}` | string prompt mesh size | `nullcline.set_mesh` | `mesh`: integer (>0) | default | `{"nullcline_mesh":integer}` | `session.state`, `done` | **Moved onto the registry at W522 ([#586](https://github.com/MuhammadMoustafa/xppautX/issues/586)).**

---

### 2.5 Resource `dirfield` (Direction Fields and Flows)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `D` -> `D` | Two-D Fun -> (D)irect Field | `{"cmd":"key","key":"d"}` | `new_int` "Grid:" ([core/nullcline.cpp:654](../../core/nullcline.cpp#L654)) | `dirfield.compute` | `grid`: integer (>=2, W494's `dfgrid`), `scaled`: optional boolean | default | `{"grid":integer,"vectors_drawn":integer}` | `plot.series`, `done` | **Moved onto the registry at W522 ([#586](https://github.com/MuhammadMoustafa/xppautX/issues/586)).**
| `D` -> `F` | Two-D Fun -> (F)low | `{"cmd":"key","key":"f"}` | `new_int` "Grid:" (the same ask and range) | `dirfield.flow` | `grid`: integer (>=2, W494's `dfgrid`) | default | `{"grid":integer,"trajectories_run":integer}` | `plot.series`, `done` | **Moved onto the registry at W522 ([#586](https://github.com/MuhammadMoustafa/xppautX/issues/586)).**
| `D` -> `N` | Two-D Fun -> (N)o dir. fld. | `{"cmd":"key","key":"n"}` | none | `dirfield.clear` | none | default | `{"cleared":boolean}` | `plot.cleared`, `done` | **Moved onto the registry at W522 ([#586](https://github.com/MuhammadMoustafa/xppautX/issues/586)).**
| `D` -> `C` | Two-D Fun -> (C)olorize | `{"cmd":"key","key":"c"}` | `new_int` "Grid:" | `dirfield.colorize` | `grid`: integer (>=2, W494's `dfgrid`) | default | `{"grid":integer,"cells_colored":integer}` | `plot.series`, `done` | **Moved onto the registry at W522 ([#586](https://github.com/MuhammadMoustafa/xppautX/issues/586)).**
| `D` -> `S` | Two-D Fun -> (S)caled Dir.Fld | `{"cmd":"key","key":"s"}` | `new_int` "Grid:" | `dirfield.compute` | `grid`: integer (>=2, W494's `dfgrid`), `scaled`: true | default | `{"grid":integer,"vectors_drawn":integer}` | `plot.series`, `done` | **Moved onto the registry at W522 ([#586](https://github.com/MuhammadMoustafa/xppautX/issues/586)).**
| (`--silent`, protocol) | (Write dfield data) | `{"cmd":"dfield","op":"write","name":F}` | none | `dirfield.export` | `file`: file | default | `{"file":file,"written":boolean}` | `done` | **Moved onto the registry at W522 ([#586](https://github.com/MuhammadMoustafa/xppautX/issues/586)).**

---

### 2.6 Resource `window` (Plot Windows, Zooming, and Views)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `M` -> `C` | Make window -> (C)reate | `{"cmd":"key","key":"c"}` | none | `window.create` | `name`: optional text | default | `{"window":window}` | `window.created`, `session.state`, `done` |
| `M` -> `K` | Make window -> (K)ill all | `{"cmd":"key","key":"k"}` | `yes_no_box` confirmation before every extra window is closed | `window.destroy_all` | none | default | `{"closed_count":integer}` | `window.destroyed`, `session.state`, `done` |
| `M` -> `D` | Make window -> (D)estroy | `{"cmd":"key","key":"d"}` | none (destroys the active window; the main one cannot be destroyed: an error) | `window.destroy` | `window`: optional window | default | `{"window":window,"closed":boolean}` | `window.destroyed`, `session.state`, `done` | **Moved onto the registry at W515 (#579)**; `confirm` (optional boolean) answers the question a graph holding texts, arrows, markers or frozen curves asks; an AUTO view, the animation and the array plot (`array_plot_1`) are destroyed with it, the main graph and the data browser refuse.
| (new, W515) | (Hide a window) | none (the page hid AUTO itself) | none | `window.hide` | `window`: optional window | default | `{"window":window,"shown":boolean}` | `windows`, `session.state`, `done` |
| (new, W515) | (Show a hidden window) | none | none | `window.show` | `window`: optional window | default | `{"window":window,"shown":boolean}` | `windows`, `session.state`, `done` |
| (new, W515) | (List the windows) | none | none | `window.list` | none | default | `{"windows":list of {"name":text,"kind":choice,"shown":boolean}}` | `session.state`, `done` |
| `M` -> `B` | Make window -> (B)ottom | `{"cmd":"key","key":"b"}` | none | `window.send_to_bottom` | `window`: optional window | default | `{"window":window}` | `session.state`, `done` |
| `M` -> `A` | Make window -> (A)uto | `{"cmd":"key","key":"a"}` | none | `window.set_redraw_mode` | `mode`: choice("auto","manual"), `window`: optional window | default | `{"redraw_mode":choice("auto","manual")}` | `session.state`, `done` |
| `M` -> `M` | Make window -> (M)anual | `{"cmd":"key","key":"m"}` | none | (Merged into `window.set_redraw_mode`) | `mode`: "manual", `window`: optional window | default | `{"redraw_mode":"manual"}` | `session.state`, `done` |
| `M` -> `S` | Make window -> (S)imPlot On/Off | `{"cmd":"key","key":"s"}` | none | `window.set_simultaneous_plot` | `enabled`: boolean | default | `{"simultaneous_plot":boolean}` | `session.state`, `done` |
| `W` -> `W` | Window -> (W)indow | `{"cmd":"display"}` | `do_string_box_of` "Window" ([core/graf_par.cpp:481](../../core/graf_par.cpp#L481)): X Lo, X Hi, Y Lo, Y Hi (checked as ordered pairs, W494: Hi above Lo) | `window.set_bounds` | `x_min`: real, `x_max`: real, `y_min`: real, `y_max`: real, `window`: optional window | default | `{"x_min":real,"x_max":real,"y_min":real,"y_max":real}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W523 (#587)**.
| `W` -> `Z` | Window -> (Z)oom In | a `rubber` ask (`flag` 0, a box), answered with `xd`, `yd`, `xd2`, `yd2` (data coordinates) or pixels | `rubber` ask ([core/json_prompts.cpp:524](../../core/json_prompts.cpp#L524)); a click with no drag (the two corners equal) halves the range about the point; page-free by data coordinates today, so only the registry name and typed arguments are missing | `window.zoom` | `x_min`: real, `x_max`: real, `y_min`: real, `y_max`: real, `window`: optional window | default | `{"zoom_applied":boolean}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W523 (#587)**.
| `W` -> `O` | Window -> Zoom (O)ut | a `rubber` ask like Zoom In, answered with `xd`, `yd`, `xd2`, `yd2` | `rubber` ask ([core/json_prompts.cpp:524](../../core/json_prompts.cpp#L524)); a click with no drag doubles the range about the point, a box makes the current view fit into the box. There is no zoom factor today | `window.zoom_out` | `factor`: optional real (> 1; today only 2 by a click), or the box `x_min`, `x_max`, `y_min`, `y_max` the current view shrinks into, `window`: optional window | default | `{"new_bounds":map[string, real]}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W523 (#587)**.
| `W` -> `F` | Window -> (F)it | `{"cmd":"key","key":"f"}` | none | `window.fit` | `window`: optional window | default | `{"x_min":real,"x_max":real,"y_min":real,"y_max":real}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W523 (#587)**.
| `W` -> `D` | Window -> (D)efault | `{"cmd":"key","key":"d"}` | none | `window.reset_default` | `window`: optional window | default | `{"x_min":real,"x_max":real,"y_min":real,"y_max":real}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W523 (#587)**.
| (Page) | (Display state of a window) | `{"cmd":"display","win":W,"x":[lo,hi],"y":[lo,hi],"runs":B,"freeze":B}` | none | `window.set_display` | `window`: optional window, `x_range`: optional list[real, 2] or null, `y_range`: optional list[real, 2] or null, `show_earlier_runs`: optional boolean, `keep_replaced_run`: optional boolean | default | `{"x_range":list[real, 2],"y_range":list[real, 2]}` | `plots`, `session.state`, `done` | **Moved onto the registry at W523 (#587)**.
| `W` -> `S` | Window -> (S)croll | a `drag` ask loop (`what` `down`, `move`, `up` with `xd`, `yd`), ended by a cancel or a key | `drag` asks ([core/json_windows.cpp:261](../../core/json_windows.cpp#L261)); the view moves by half the drag; page-free by data coordinates today, but a script must send every pointer event; proposal: one `window.scroll` with `dx`, `dy` | `window.scroll` | `dx`: real, `dy`: real, `window`: optional window | default | `{"new_center":list[real, 2]}` | `plot.shown`, `done` | **Moved onto the registry at W523 (#587)**.
| `R` | Redraw plot | `{"cmd":"redraw"}` (redraws the active plot window, and the AUTO diagram when AUTO is open) | none | `window.redraw` | `window`: optional window | default | `{"redrawn":boolean}` | `plot.shown`, `done` | **Moved onto the registry at W523 (#587)**.
| `E` | Clear plot | `{"cmd":"key","menu":"main","item":"erase"}` | none | `window.clear` | `window`: optional window | default | `{"cleared":boolean}` | `plot.cleared`, `done` | **Moved onto the registry at W523 (#587)**.
| (Page: choosing a window) | (Select a plot window) | `{"cmd":"click","win":W}` | none | `window.select` | `window`: window | default | `{"window":window}` | `session.state`, `done` |
| (new: windows have numbers today) | (Rename window: a name of its own, W308 question 6) | none | none | `window.rename` | `window`: window, `name`: text | default | `{"window":window,"new_name":text}` | `window.renamed`, `session.state`, `done` |

---

### 2.7 Resource `plot` (Curves, Axes, Colormaps, and Rendering)

Also here (proposed, awaiting the maintainer): marks computed from the data, not placed by the user: `plot.add_data_markers` and the frozen curves. Equilibrium marks come from the equilibrium commands (`marks.equilibria`) and have no command of their own.

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `G` -> `A` | Curves -> (A)dd curve | `{"cmd":"key","key":"a"}` | typed form "New Curve" ([core/graf_par.cpp:781](../../core/graf_par.cpp#L781)): X-axis, Y-axis, Z-axis (variable names), Colour (a quoted colour), Draw, Dash, Width (above 0), Marker, Marker size | `plot.add_curve` | `x`: name, `y`: name, `z`: optional name, `color`: optional color, `draw`, `dash`, `width`, `marker`, `marker_size`: typed style fields, `window`: optional window | default | `{"curve_index":integer}` | `plot.series`, `session.state`, `done` | **Moved onto the registry at W524 (#588)**; `x`, `y` required, the rest optional (the first curve's); `plot.series` is the `plots` event. |
| `G` -> `D` | Curves -> (D)elete last | `{"cmd":"key","key":"d"}` | none; today deletes the last curve (the first one stays) | `plot.delete_curve` | `curve_index`: optional integer, `window`: optional window | default | `{"remaining_curves":integer}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W524 (#588)**. |
| `G` -> `R` | Curves -> (R)emove all | `{"cmd":"key","key":"r"}` | none; today removes every curve but the first | `plot.delete_all_curves` | `window`: optional window | default | `{"cleared":boolean}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W524 (#588)**. |
| `G` -> `E` | Curves -> (E)dit curve | `{"cmd":"key","key":"e"}` | `new_int` "Edit 0-N :" ([core/graf_par.cpp:809](../../core/graf_par.cpp#L809)), then the typed form "Edit curve N" (as New Curve) | `plot.edit_curve` | `curve_index`: integer, `x`: optional name, `y`: optional name, `z`: optional name, `color`: optional color, `draw`, `dash`, `width`, `marker`, `marker_size`: typed style fields, `window`: optional window | default | `{"curve_index":integer,"updated":boolean}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W524 (#588)**. |
| `G` -> `F` -> `F` | Freeze -> (F)reeze | `{"cmd":"key","key":"f"}` | opens the typed form "Edit Freeze" ([core/graf_par.cpp:1036](../../core/graf_par.cpp#L1036)) on the new frozen curve: Colour, Draw, Dash, Width, Marker, Marker size, Key, Name | `plot.freeze_curve` | `name`: optional text, `key`: optional text, `color`: optional color, `draw`, `dash`, `width`, `marker`, `marker_size`: typed style fields (choices `draw`, `dash`, `marker` by name, `width` and `marker_size` reals), `window`: optional window | default | `{"slot":integer,"name":text}` | `plot.series`, `session.state`, `done` | **Moved onto the registry at W524 (#588)**. |
| `G` -> `F` -> `D` | Freeze -> (D)elete | `{"cmd":"key","key":"d"}` | `pick_from_list` "freeze_curves" ([core/graf_par.cpp:1226](../../core/graf_par.cpp#L1226)): the window's frozen curves by name; `marks.frozen` carries no slot number today, so a slot is a proposal | `plot.delete_frozen` | `slot`: integer, `window`: optional window | default | `{"slot":integer,"deleted":boolean}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W524 (#588)**; the `marks` event's frozen curves carry `slot`. |
| `G` -> `F` -> `E` | Freeze -> (E)dit | `{"cmd":"key","key":"e"}` | `do_string_box_of` "Edit Freeze" ([core/graf_par.cpp:1036](../../core/graf_par.cpp#L1036)): Colour, Draw, Dash, Width, Marker, Marker size, Key, Name; the curve is picked from a list by name first | `plot.edit_frozen` | `slot`: integer, `name`: optional text, `key`: optional text, `color`: optional color, `window`: optional window | default | `{"slot":integer,"updated":boolean}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W524 (#588)**. |
| `G` -> `F` -> `R` | Freeze -> (R)emove all | `{"cmd":"key","key":"r"}` | none | `plot.delete_all_frozen` | `window`: optional window | default | `{"cleared":boolean}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W524 (#588)**. |
| `G` -> `F` -> `K` | Freeze -> (K)ey | `{"cmd":"key","key":"k"}` | `menu_freeze_key` (No key / Key); Key asks a `mouse` click for the legend's position ([core/graf_par.cpp:904](../../core/graf_par.cpp#L904)) | `plot.set_frozen_legend` | `visible`: boolean, `x`: optional real, `y`: optional real (the legend's position in plot coordinates), `window`: optional window | default | `{"legend_visible":boolean}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W524 (#588)**. |
| `G` -> `F` -> `B` | Freeze -> (B)if.Diag | `{"cmd":"key","key":"b"}` | `file_selector` "Import Diagram" ([core/graf_par.cpp:1190](../../core/graf_par.cpp#L1190)): a six-column AUTO Write pts file, checked whole before it is used | `plot.import_bifurcation` | `file`: file, `window`: optional window | default | `{"file":file,"curves_imported":integer}` | `plot.series`, `done` | **Moved onto the registry at W524 (#588)**. |
| `G` -> `F` -> `C` | Freeze -> (C)lr. BD | `{"cmd":"key","key":"c"}` | none | `plot.clear_bifurcation` | `window`: optional window | default | `{"cleared":boolean}` | `plot.shown`, `done` | **Moved onto the registry at W524 (#588)**. |
| `G` -> `F` -> `O` | Freeze -> (O)n/(O)ff freeze | `{"cmd":"key","key":"o"}` | none | `plot.set_auto_freeze` | `enabled`: boolean | default | `{"auto_freeze":boolean}` | `session.state`, `done` | **Moved onto the registry at W524 (#588)**. |
| `G` -> `P` | Curves -> PDF | keys `g`, `p`, picture form and file ask | typed form "Picture parameters" ([core/image_format.cpp:50](../../core/image_format.cpp#L50): Colour boolean, Axes font size real 1..100), then `file_selector` for the `.pdf` | `plot.export_pdf` | `file`: file, `color`: optional boolean, `axes_font_size`: optional real (1..100), `window`: optional window | default | `{"file":file}` | `done` | **Moved onto the registry at W524 (#588)**; `bytes_written` too, as `plot.export_svg`; an existing file asks the replace question as every save does (no `replace` argument). |
| `G` -> `V` | Curves -> S(V)G | keys `g`, `v`, picture form and file ask | the same typed form "Picture parameters" and a `file_selector` for the `.svg` (one picture module for PDF and SVG, W492) | `plot.export_svg` | `file`: file, `color`: optional boolean, `axes_font_size`: optional real (1..100), `window`: optional window | default | `{"file":file,"bytes_written":integer}` | `done` | **Moved onto the registry at W524 (#588)**. |
| `G` -> `X` | Curves -> a(X)es opts | `{"cmd":"key","key":"x"}` | `do_string_box_of` "Axes options" ([core/graf_par.cpp:267](../../core/graf_par.cpp#L267)): X-origin, Y-origin, Z-origin, X-org(1=on), Y-org(1=on), Z-org(1=on) (numbers standing for booleans), Axes font size; the axis labels are in the Axes (2D/3D) form | `plot.set_axes_origin` | `x_origin`: optional real, `y_origin`: optional real, `z_origin`: optional real, `x_origin_shown`: optional boolean, `y_origin_shown`: optional boolean, `z_origin_shown`: optional boolean, `axes_font_size`: optional real (1..100), `window`: optional window | default | `{"origin_updated":boolean}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W524 (#588)**. |
| `G` -> `O` | Curves -> exp(O)rt data | `{"cmd":"key","key":"o"}` after `g`, then the format and file asks (Save data with "what the plot shows") | the data format ask (the data-formats registry) and `file_selector`; the plot's own columns are written | `plot.export_data` | `file`: file, `format`: choice from the supported export formats, `window`: optional window | default | `{"file":file,"rows_written":integer}` | `done` | **Moved onto the registry at W524 (#588)**; `format` is a data format id (`dat`, `csv`, `csv.gz`, `npz`), by the file's extension when left out. |
| `G` -> `C` -> `*` | Curves -> (C)olormap | `{"cmd":"colormap","name":N}` | `hello.colormaps` | `plot.set_colormap` | `colormap`: choice from `hello.colormaps` | default | `{"colormap":choice from `hello.colormaps`}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W524 (#588)**. |
| `V` -> `2` | Axes -> 2D | keys `v`, `2`, then the typed grid form "2D View" | grid form "2D View" ([core/graf_par.cpp:175](../../core/graf_par.cpp#L175)): per axis the Variable, Min, Max (ordered, W494) and Label | `plot.set_2d` | `x`: name, `y`: name, `x_min`: real, `x_max`: real, `y_min`: real, `y_max`: real, `x_label`: optional text, `y_label`: optional text, `window`: optional window | default | `{"axes":"2d","x":name,"y":name}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W524 (#588)**; every argument optional (the graph's own is kept). |
| `V` -> `3` | Axes -> 3D | keys `v`, `3`, then the typed grid form "3D View" | grid form "3D View" ([core/graf_par.cpp:175](../../core/graf_par.cpp#L175)): per axis Variable, Min, Max, Label, then XLo, XHi, YLo, YHi (the screen window) and the 3D view parameters of the `3` key | `plot.set_3d` | `x`: name, `y`: name, `z`: name, `x_min`: real, `x_max`: real, `y_min`: real, `y_max`: real, `z_min`: real, `z_max`: real, `x_label`: optional text, `y_label`: optional text, `z_label`: optional text, `window_x_min`: real, `window_x_max`: real, `window_y_min`: real, `window_y_max`: real, `window`: optional window | default | `{"axes":"3d","x":name,"y":name,"z":name}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W524 (#588)**; every argument optional (the graph's own is kept). |
| `3` | 3D view | `{"cmd":"key","menu":"main","item":"3dparams"}` | `do_string_box_of` "3D Parameters" ([core/graf_par.cpp:569](../../core/graf_par.cpp#L569)): Persp (1=On), ZPlane, ZView, Theta, Phi, Movie(Y/N), Vary (theta/phi), Start angle, Increment, Number increments; a 3D window only | `plot.set_3d_view` | `perspective`: optional boolean, `z_plane`: optional real, `z_view`: optional real, `theta`: optional real, `phi`: optional real, `movie`: optional boolean, `vary`: optional choice(`theta`,`phi`), `start_angle`: optional real, `increment`: optional real, `increments`: optional integer (>=1), `window`: optional window | default | `{"view_updated":boolean}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W524 (#588)**. |
| `U` -> `U` | Numerics -> rUelle plot | `{"cmd":"key","key":"u"}` after `u`; no `set` key for the shifts | `new_int` "x-axis shift ", "y-axis shift ", "z-axis shift" (negative values become 0) | `plot.set_shifted_axes` | `x_shift`: integer (>=0), `y_shift`: integer (>=0), `z_shift`: integer (>=0), `window`: optional window | default | `{"x_shift":integer,"y_shift":integer,"z_shift":integer}` | `plots`, `session.state`, `done` | **Moved onto the registry at W524 (#588)**. |
| `X` | Variable vs time | `{"cmd":"key","menu":"main","item":"xivst"}` | `new_string_of` "Plot vs t:" ([core/graf_par.cpp:516](../../core/graf_par.cpp#L516)) | `plot.variable_vs_time` | `variable`: name, `window`: optional window | default | `{"x":"t","y":name}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W524 (#588)**. |
| `T` -> `S` | Text,etc -> marker(S) | `{"cmd":"key","key":"s"}` after `t` | typed form "Add Markers" ([core/grobs.cpp:305](../../core/grobs.cpp#L305)): Type, Colour, Size, Number, Row1, Skip; the markers sit on the window's first curve, every Skip-th row from Row1 | `plot.add_data_markers` | `shape`: choice(`box`,`diamond`,`triangle`,`plus`,`cross`,`circle`), `color`: optional color, `size`: optional real, `count`: integer (>=1), `first_row`: integer (>=0), `stride`: integer (>=1), `window`: optional window | default | `{"count":integer}` | `marks`, `session.state`, `done` | **Moved onto the registry at W524 (#588)**. |
| (Page: IC box buttons) | (xvst / pp / array buttons for the checked variables) | `{"cmd":"plotvars","how":0\|1\|2,"names":[...]}` | none | `plot.set_variables` | `how`: choice(`time_series`,`phase_plane`,`array_plot`), `names`: list[name], `window`: optional window | default | `{"how":choice,"names":list[name]}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W524 (#588)**; `names` are the model's states; `time_series`, `phase_plane` (first on the x axis), `array_plot` (from the first of two to the later); replaces the `plotvars` command. |

---

### 2.8 Resource `annotation` (Text, Pointers, Arrows, and Markers)

Only what the user places, each with an id (proposed, awaiting the maintainer). Today only text labels have a direct command (`text`) and an id (`marks.text[].id`); arrows, pointers and markers are placed by a mouse or rubber ask (answerable in data coordinates) and carry no id in `marks`, so a registry that edits or deletes them needs the id added to the `marks` event first. `MAXLAB` (50 labels) and `MAXGROB` (400 objects) are fixed limits that "An array's size has one source" would lift, for a later card.

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `T` -> `T` | Text,etc -> (T)ext | `{"cmd":"text","op":"add","win":W,"text":T,"x":X,"y":Y,"size":N,"style":N,"color":C}` (plot coordinates, no pointer needed); the menu path asks a `mouse` first, then the `text` ask | `mouse` ask for the position, then the `text` ask (text, size 0-4, style 0-3, colour); up to `hello.text.max_length` characters and 50 labels (`MAXLAB`, [core/grobs.h:15](../../core/grobs.h#L15)) | `annotation.add_text` | `text`: text, `x`: real, `y`: real, `size`: optional integer (0..4), `style`: optional integer (0..3), `color`: optional color, `window`: optional window | default | `{"id":integer}` (the label's slot, as `marks.text[].id`) | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W523 (#587)**; the id argument is `annotation_id` (`id` is the request id).
| `T` -> `A` | Text,etc -> (A)rrow (a head only) | `{"cmd":"key","key":"a"}` after `t`; no direct command (`text` handles labels only) | `new_float` "Size: ", `ask_color` "Colour: " (a quoted colour), then a `rubber` ask (`flag` 1, a line) for the tip and tail ([core/grobs.cpp:351](../../core/grobs.cpp#L351)); `marks.arrows` has no id | `annotation.add_arrow` | `tip_x`: real, `tip_y`: real, `tail_x`: real, `tail_y`: real, `size`: optional real, `color`: optional color, `window`: optional window | default | `{"id":integer}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W523 (#587)**; the id argument is `annotation_id` (`id` is the request id).
| `T` -> `P` | Text,etc -> (P)ointer (a head and a shaft) | `{"cmd":"key","key":"p"}` after `t`; no direct command | `new_float` "Size: ", `ask_color` "Colour: ", then a `rubber` ask (`flag` 1) for the tip and tail ([core/grobs.cpp:351](../../core/grobs.cpp#L351)); `marks.arrows` `kind` `pointer`, no id | `annotation.add_pointer` | `tip_x`: real, `tip_y`: real, `tail_x`: real, `tail_y`: real, `size`: optional real, `color`: optional color, `window`: optional window | default | `{"id":integer}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W523 (#587)**; the id argument is `annotation_id` (`id` is the request id).
| `T` -> `M` | Text,etc -> (M)arker | `{"cmd":"key","key":"m"}` after `t`; no direct command | typed form "Add Marker" ([core/grobs.cpp:294](../../core/grobs.cpp#L294)): Type (box, diamond, triangle, plus, cross, circle), Colour, Size; then a `mouse` ask for the position; `marks.markers` has no id | `annotation.add_marker` | `shape`: choice(`box`,`diamond`,`triangle`,`plus`,`cross`,`circle`), `x`: real, `y`: real, `size`: optional real, `color`: optional color, `window`: optional window | default | `{"id":integer}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W523 (#587)**; the id argument is `annotation_id` (`id` is the request id).
| `T` -> `E` -> `M` | Edit -> (M)ove | `text` `edit` with `x`, `y` moves a label by `id`; arrows and markers: no direct command | `mouse` ask to pick the nearest label or object, then a `mouse` ask for the drop; ids exist only for labels, so `id` for arrows and markers needs the `marks` event to carry one | `annotation.move` | `id`: integer, `new_x`: real, `new_y`: real, `window`: optional window | default | `{"id":integer,"moved":boolean}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W523 (#587)**; the id argument is `annotation_id` (`id` is the request id).
| `T` -> `E` -> `C` | Edit -> (C)hange | `{"cmd":"text","op":"edit","win":W,"id":N,...}` for a label; arrows and markers: no direct command | `do_string_box_of` / string prompts | `annotation.edit` | `id`: integer, `text`: optional text, `size`: optional real, `color`: optional color, `window`: optional window | default | `{"id":integer,"updated":boolean}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W523 (#587)**; the id argument is `annotation_id` (`id` is the request id).
| `T` -> `E` -> `D` | Edit -> (D)elete | `{"cmd":"text","op":"delete","win":W,"id":N}` for a label; arrows and markers: no direct command | `mouse` ask to pick the object, then `TwoChoice` "Delete ?" | `annotation.delete` | `id`: integer, `window`: optional window | default | `{"id":integer,"deleted":boolean}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W523 (#587)**; the id argument is `annotation_id` (`id` is the request id).
| `T` -> `D` | Text,etc -> (D)elete all | `{"cmd":"key","key":"d"}` after `t` (every label and object of the active window) | none | `annotation.delete_all` | `window`: optional window | default | `{"cleared":boolean}` | `plot.shown`, `session.state`, `done` | **Moved onto the registry at W523 (#587)**; the id argument is `annotation_id` (`id` is the request id).

---

### 2.9 Resource `kinescope` (Frame Capture and Animation Playback)

**Moved onto the registry (W527, #591): every row below is implemented under its proposed name; the "Today" columns describe the forms that were deleted.** Readings that differ from the row: playing or saving with no frames is an error ("The kinescope holds no frames"); `kinescope.autoplay`'s `cycles` and `frame_ms` are optional (the last autoplay's when left out); `kinescope.export_frames` and `export_gif` name their `file` and ask the client for each frame's picture.

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `K` -> `C` | Kinescope -> (C)apture | `{"cmd":"key","key":"c"}` | the client answers a `pixels` ask (`film`: a frame index) with its picture; page-free only for a client that can render the window itself | `kinescope.capture` | none | default | `{"frame_count":integer}` | `session.state`, `done` | **Moved onto the registry at W527 ([#591](https://github.com/MuhammadMoustafa/xppautX/issues/591)).**
| `K` -> `R` | Kinescope -> (R)eset | `{"cmd":"key","key":"r"}` | none | `kinescope.reset` | none | default | `{"cleared":boolean}` | `session.state`, `done` | **Moved onto the registry at W527 ([#591](https://github.com/MuhammadMoustafa/xppautX/issues/591)).**
| `K` -> `P` | Kinescope -> (P)layback | `{"cmd":"key","key":"p"}` | `pixels` asks, one per frame, answered by the client's pictures | `kinescope.play` | none | default | `{"frames_played":integer}` | `done` | **Moved onto the registry at W527 ([#591](https://github.com/MuhammadMoustafa/xppautX/issues/591)).**
| `K` -> `A` | Kinescope -> (A)utoplay | `{"cmd":"key","key":"a"}` | `new_int` "Number of cycles", `new_int` "Msec between frames" ([core/commands.cpp:88](../../core/commands.cpp#L88)) | `kinescope.autoplay` | `cycles`: integer (>0), `frame_ms`: integer (>=0) | default | `{"cycles_completed":integer}` | `done` | **Moved onto the registry at W527 ([#591](https://github.com/MuhammadMoustafa/xppautX/issues/591)).**
| `K` -> `S` | Kinescope -> (S)ave | `{"cmd":"key","key":"s"}` | `file_selector` "Save kinescope frames" "*.gif" (frames are `pixels` asks) | `kinescope.export_frames` | `file`: file | default | `{"file":file,"frames_saved":integer}` | `done` | **Moved onto the registry at W527 ([#591](https://github.com/MuhammadMoustafa/xppautX/issues/591)).**
| `K` -> `M` | Kinescope -> (M)ake AniGif | `{"cmd":"key","key":"m"}` | `file_selector` "*.gif" | `kinescope.export_gif` | `file`: file | default | `{"file":file,"bytes_written":integer}` | `done` | **Moved onto the registry at W527 ([#591](https://github.com/MuhammadMoustafa/xppautX/issues/591)).**

---

### 2.10 Resource `bvp` (Boundary Value Problems)

**Moved onto the registry (W521, #585): every row below is implemented under its proposed name; see docs/protocol.md's registry table. The "Today" columns describe the forms that were deleted.** Readings that differ from the row: `ic.mouse_stream` takes `x` and `y` lists (a list holds single values); `ic.formula` takes a state or an array range `u[1..5]` and `ic.array` the offset formula; `run.dae_guess` returns `guesses`; `bvp.solve` and `bvp.solve_periodic` take a required `show`; `bvp.solve_range`'s `side` is `start` or `end`; `run.configure`'s `method` is text the numerics owner checks; `run.abort` has no `done` of its own.

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `B` -> `R` | Bndry Value Prob -> (R)ange | `{"cmd":"key","key":"r"}` | `do_string_box_of` "Range Shoot" ([core/pp_shoot.cpp:305](../../core/pp_shoot.cpp#L305)): Range over (a parameter), Steps, Start, End, Cycle color(Y/N), Side(0/1), Movie(Y/N); yes/no as text and Side as a number today | `bvp.solve_range` | `parameter`: name, `start`: real, `end`: real, `steps`: integer (>0), `cycle_color`: optional boolean, `side`: optional integer (0 or 1 today; a named choice to settle in W353), `movie`: optional boolean | default | `{"solutions_found":integer}` | `plot.series`, `done` |
| `B` -> `N` | Bndry Value Prob -> (N)o show (runs the solver, shows only the end) | `{"cmd":"key","key":"n"}` | none | `bvp.solve` | `show`: optional boolean (false: only the final solution; true: every Newton step) | default | `{"converged":boolean}` | `plot.series`, `session.state`, `done` |
| `B` -> `S` | Bndry Value Prob -> (S)how (runs the solver, shows every step) | `{"cmd":"key","key":"s"}` | none | (Merged into `bvp.solve`) | `show`: true | default | `{"converged":boolean}` | `plot.series`, `session.state`, `done` |
| `B` -> `P` | Bndry Value Prob -> (P)eriodic | `{"cmd":"key","key":"p"}` | `do_string_box_of` "Periodic BCs" ([core/pp_shoot.cpp:171](../../core/pp_shoot.cpp#L171)): Freq. Par., Sect. Var, Section, Show(Y/N) (Show as text Y/N today) | `bvp.solve_periodic` | `frequency_parameter`: name, `section_variable`: name, `section`: real, `show`: optional boolean | default | `{"converged":boolean,"period":real}` | `plot.series`, `session.state`, `done` |
| `U` -> `V` | Numerics -> bndVal | `{"cmd":"set","kind":"num","name":KEY,"value":V}` with the keys `bvp_max_iterates`, `bvp_tolerance`, `bvp_epsilon` | numerics prompts for the three values, range-checked in one place (W494) | `bvp.configure` | `max_iterates`: optional integer, `tolerance`: optional real, `epsilon`: optional real | default | `{"settings":map[string, real]}` | `session.state`, `done` |

---

### 2.11 Resource `analysis` (Stochastic, Fits, Spectra, Adjoints, and Poincare Maps)

**Moved onto the registry (W525, #589): every row below is implemented under its proposed name; see docs/protocol.md's registry table. The "Today" columns describe the forms that were deleted (only `browser` `postprocess` was a protocol form of its own; the menus' keys stay and call the same functions).** Readings that differ from the row: every argument that has a setting in the session is optional and left out keeps it (the rows said required); `analysis.stochastic_integrate`'s `trials` is the number of runs (the Range settings' steps plus one, at least 2) and `analysis.fit_curve` reads a file of the model's folder (`file`, not the browser's data), its `data_columns` are the file's column numbers and `variables` and `parameters` required; `analysis.poincare_map`'s `type` is required and its kinds `none`, `section`, `max_min`, `period` (the model file still says `max` until W381's rename of the option); `analysis.phase_space_torus` also takes `period`; `analysis.color_coding` takes `window`; `analysis.lyapunov_exponent` takes the sweep's arguments with `range_over_parameters` and also says `runs_completed`; a load of the mean, variance, histogram, adjoint or H function that does not exist is an error, not `false`; `analysis.statistics`' `variance` is of the stored rows (no sample correction); `analysis.adjoint_interaction`'s `couplings` name differential equations.

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `U` -> `H` -> `N` | Stochastic -> New seed | `{"cmd":"set","kind":"options","name":"seed"}` | `new_int` "Seed:" ([core/markov.cpp:347](../../core/markov.cpp#L347)) | `analysis.seed_random` | `seed`: integer (>=0) | default | `{"seed":integer}` | `session.state`, `done` |
| `U` -> `H` -> `C` | Stochastic -> Compute | `{"cmd":"key","key":"c"}` | runs the Range Integrate form (W487) with the stochastic flag on: the number of runs is its Steps; the mean and variance are kept for Mean and Variance | `analysis.stochastic_integrate` | `trials`: integer (>0) | default | `{"trials_completed":integer}` | `plot.series`, `done` |
| `U` -> `H` -> `D` | Stochastic -> Data | `{"cmd":"key","key":"d"}` | none; puts the data of the last run back into the browser (no trial number is asked) | `analysis.load_stochastic_run` | none | default | `{"loaded":boolean}` | `browser.updated`, `done` |
| `U` -> `H` -> `M` | Stochastic -> Mean | `{"cmd":"key","key":"m"}` | none | `analysis.load_stochastic_mean` | none | default | `{"loaded":boolean}` | `browser.updated`, `done` |
| `U` -> `H` -> `V` | Stochastic -> Variance | `{"cmd":"key","key":"v"}` | none | `analysis.load_stochastic_variance` | none | default | `{"loaded":boolean}` | `browser.updated`, `done` |
| `U` -> `H` -> `H` | Stochastic -> Histogram | `{"cmd":"browser","op":"postprocess","kind":"hist"}` | `new_int` "Number of bins ", `new_float` "Low ", "Hi ", the variable (`new_string_of`), `new_string_of` "Condition " ([core/histogram.cpp:641](../../core/histogram.cpp#L641)) | `analysis.histogram` | `column`: name, `bins`: integer (>0), `low`: real, `high`: real, `condition`: optional expression | default | `{"bins":integer,"total_samples":integer}` | `browser.updated`, `plot.series`, `done` |
| `U` -> `H` -> `O` | Stochastic -> Old hist | `{"cmd":"key","key":"o"}` | none | `analysis.reload_histogram` | none | default | `{"reloaded":boolean}` | `browser.updated`, `plot.series`, `done` |
| `U` -> `H` -> `2` | Stochastic -> 2D-hist | `{"cmd":"browser","op":"postprocess","kind":"hist2d"}` | "Variable 1 ", "Variable 2 ", then bins, low and hi for each ([core/histogram.cpp:197](../../core/histogram.cpp#L197)); needs at least 3 stored rows | `analysis.histogram_2d` | `column1`: name, `column2`: name, `bins1`: integer (>0), `bins2`: integer (>0), `low1`: real, `high1`: real, `low2`: real, `high2`: real | default | `{"bins_x":integer,"bins_y":integer}` | `browser.updated`, `plot.series`, `done` |
| `U` -> `H` -> `F` | Stochastic -> Fourier | `{"cmd":"browser","op":"postprocess","kind":"fourier"}` | none | `analysis.fourier_series` | `column`: name | default | `{"harmonics":integer}` | `browser.updated`, `done` |
| `U` -> `H` -> `P` | Stochastic -> Power | `{"cmd":"browser","op":"postprocess","kind":"power"}` | none | `analysis.power_spectrum` | `column`: name | default | `{"frequencies":integer}` | `browser.updated`, `plot.series`, `done` |
| `U` -> `H` -> `I` | Stochastic -> fIt data | `{"cmd":"key","key":"i"}` | `do_string_box_of` "Fit" ([core/do_fit.cpp:553](../../core/do_fit.cpp#L553)): Fitvar, Params, Tolerance, Npts, NCols, To Col, Params, Epsilon, Max iter; the data come from the browser (Load first) | `analysis.fit_curve` | `variables`: list[name], `parameters`: list[name], `tolerance`: optional real, `points`: optional integer, `columns`: optional integer, `data_columns`: optional list[name], `epsilon`: optional real, `max_iterations`: optional integer | default | `{"converged":boolean,"parameters":map[name, real]}` | `session.state`, `done` |
| `U` -> `H` -> `S` | Stochastic -> Stat | `{"cmd":"key","key":"s"}` | none | `analysis.statistics` | `column`: name | default | `{"mean":real,"variance":real,"std_dev":real}` | `done` |
| `U` -> `H` -> `L` | Stochastic -> Liapunov | `{"cmd":"key","key":"l"}` | `new_int` "Range over parameters?(0/1)" ([core/adj2.cpp:492](../../core/adj2.cpp#L492)): a number standing for yes/no; with 1 the Range Integrate form follows and the exponent is stored per run, with 0 the exponent is shown in a bottom message | `analysis.lyapunov_exponent` | `range_over_parameters`: optional boolean (with it, the arguments of `run.range`), `jacobian_step` comes from `equilibrium.configure` | default | `{"max_lyapunov_exponent":real}` | `done` |
| `U` -> `H` -> `A` | Stochastic -> stAutocor | `{"cmd":"key","key":"a"}` | `new_int` "Number of bins ", `new_float` "Low ", "Hi ", then "Variable " ([core/histogram.cpp:599](../../core/histogram.cpp#L599)) | `analysis.autocorrelation` | `column`: name, `bins`: integer (>=2), `low`: real, `high`: real | default | `{"lags_computed":integer}` | `browser.updated`, `done` |
| `U` -> `H` -> `X` | Stochastic -> Xcorrel etc | `{"cmd":"key","key":"x"}` | `new_int` "Number of bins " (the lags), `new_int` "(0)Direct or (1) FFT ", then "Variable 1 ", "Variable 2 " ([core/histogram.cpp:576](../../core/histogram.cpp#L576)) | `analysis.cross_correlation` | `column1`: name, `column2`: name, `lags`: integer (>=2), `method`: choice(`direct`,`fft`) | default | `{"lags_computed":integer}` | `browser.updated`, `done` |
| `U` -> `H` -> `E` | Stochastic -> spEc.dns | `{"cmd":"key","key":"e"}` | `new_int` "(0) PSDx, (1) PSDxy, (2) COHxy:", the variable(s), `new_int` "Window length ", `new_int` "0:sqr 1:par 2:ham 3:bart 4:han " ([core/histogram.cpp:530](../../core/histogram.cpp#L530)); numbers standing for choices | `analysis.spectral_density` | `type`: choice(`psd_x`,`psd_xy`,`coherence_xy`), `column`: name, `column2`: optional name, `window_length`: integer (>0), `window`: choice(`square`,`parzen`,`hamming`,`bartlett`,`hann`) | default | `{"points":integer}` | `browser.updated`, `done` |
| `U` -> `P` | Numerics -> Poincare map | the menu item picks the type (`n` none, `s` section, `m` max/min, `p` period), then the typed form; settings by `{"cmd":"set","kind":"num",...}` where the key exists | typed form "Poincare map" ([core/numerics.cpp:312](../../core/numerics.cpp#L312), W485): Variable (a name), Section (real), Direction (choice `negative`, `both`, `positive`), Stop on section (boolean) | `analysis.poincare_map` | `type`: choice(`none`,`section`,`max_min`,`period`), `variable`: optional name, `section`: optional real, `direction`: optional choice(`negative`,`both`,`positive`), `stop_on_section`: optional boolean | default | `{"poincare_configured":boolean}` | `session.state`, `done` |
| `A` -> `*` | Phase space -> (A)ll/(N)one/(C)hoose | `{"cmd":"key","key":"*"}` | `menu_torus` (checklist in Choose) | `analysis.phase_space_torus` | `mode`: choice("all","none","choose"), `variables`: optional list[name] | default | `{"torus_variables":list[name]}` | `session.state`, `done` |
| `U` -> `C` | Numerics -> Color by value | Numerics menu Color by value form | `new_string_of` "Color via:", `TwoChoice` "Color" ((O)ptimize or (C)hoose), `new_float` "Min :", "Max :"; it colours a plot window, so it is a plotting setting (category Plot in the command table) ([core/numerics.cpp:380](../../core/numerics.cpp#L380)) | `analysis.color_coding` | `mode`: choice("none","velocity","variable"), `variable`: optional name, `low`: optional real, `high`: optional real | default | `{"color_coding":choice("none","velocity","variable")}` | `plot.shown`, `session.state`, `done` |
| `U` -> `A` -> `N` | Adjoint -> (N)ew adj | `{"cmd":"key","key":"n"}` | none | `analysis.adjoint_compute` | none | default | `{"computed":boolean}` | `done` |
| `U` -> `A` -> `M` | Adjoint -> (M)ake H | `{"cmd":"key","key":"m"}` | `new_string_of` "Coupling for <name> eqn:" for each equation ([core/adj2.cpp:245](../../core/adj2.cpp#L245)) | `analysis.adjoint_interaction` | `couplings`: map[name, expression] | default | `{"computed":boolean}` | `done` |
| `U` -> `A` -> `A` | Adjoint -> (A)djoint | `{"cmd":"key","key":"a"}` | none | `analysis.adjoint_load` | none | default | `{"loaded":boolean}` | `browser.updated`, `done` |
| `U` -> `A` -> `O` | Adjoint -> (O)rbit | `{"cmd":"key","key":"o"}` | none | `analysis.adjoint_load_orbit` | none | default | `{"loaded":boolean}` | `browser.updated`, `done` |
| `U` -> `A` -> `H` | Adjoint -> (H)fun | `{"cmd":"key","key":"h"}` | none | `analysis.adjoint_load_interaction` | none | default | `{"loaded":boolean}` | `browser.updated`, `done` |
| `U` -> `A` -> `P` | Adjoint -> (P)arameters | `{"cmd":"key","key":"p"}` | `new_int` Max iterates, `new_float` tolerance ([core/adj2.cpp:197](../../core/adj2.cpp#L197)) | `analysis.adjoint_configure` | `max_iterates`: integer (>0), `tolerance`: real (>0) | default | `{"updated":boolean}` | `session.state`, `done` |
| `U` -> `A` -> `R` | Adjoint -> (R)ange | `{"cmd":"key","key":"r"}` | none: it sets a flag (`adj_range`) so the next range run also computes the adjoint at each step | `analysis.adjoint_range` | `enabled`: optional boolean (the range itself is `run.range`) | default | `{"adjoint_range":boolean}` | `session.state`, `done` |

---

### 2.12 Resource `table` (Lookup Tables)

**Moved onto the registry (W525, #589): every row below is implemented under its proposed name; see docs/protocol.md's registry table. The "Today" columns describe the Tables menu's forms, which stay for the keys.** Readings that differ from the row: `table.edit` takes the table by `name` and, since a table is either a formula or a file, the formula table's `formula`, `x_min`, `x_max`, `points` and `auto_evaluate` or the file table's `file` (left out: its current file), each optional and left out kept; `table.view_in_browser` refuses a table with more points than the data table has rows.

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `U` -> `K` -> `E` | Tables -> (E)dit | `{"cmd":"key","key":"e"}` | `pick_from_list` "table", `new_int` "Auto-evaluate? (1/0)", `new_int` "NPts: ", `new_float` "Xlo: ", "Xhi: ", `new_string_of` "Formula :" ([core/tabular.cpp:156](../../core/tabular.cpp#L156)); the (1/0) is a number standing for yes/no | `table.edit` | `name`: name, `formula`: expression, `x_min`: real, `x_max`: real, `points`: integer (>0), `auto_evaluate`: optional boolean | default | `{"table":name,"points_evaluated":integer}` | `session.state`, `done` |
| `U` -> `K` -> `V` | Tables -> (V)iew | `{"cmd":"key","key":"v"}` | `pick_from_list` table name | `table.view_in_browser` | `name`: name | default | `{"table":name,"shown_in_browser":boolean}` | `browser.updated`, `done` |

---

### 2.13 Resource `browser` (Data Browser and Storage Operations)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `B` -> `F` | Data browser -> (F)ind | `{"cmd":"key","win":"browser","key":"f"}` | `do_string_box_of` "Find Data" ([core/browse_data.cpp:738](../../core/browse_data.cpp#L738)): Variable, Value | `browser.find` | `column`: name, `value`: real | default | `{"row_found":integer}` | `browser.updated`, `done` | **Moved onto the registry at W526 (#590)**. |
| `B` -> `G` | Data browser -> (G)et | `{"cmd":"key","win":"browser","key":"g","row":N}` | none | `browser.get_row_as_ic` | `row`: optional integer | default | `{"row":integer,"applied":map[name, real]}` | `session.state`, `done` | **Moved onto the registry at W526 (#590)**: it is `ic.from_data_row` (W521), not a second command. |
| `B` -> `R` | Data browser -> (R)eplace | `{"cmd":"key","win":"browser","key":"r"}` | `get_dialog_of` "Replace": Variable:, then Formula: ([core/browse_data.cpp:680](../../core/browse_data.cpp#L680)) | `browser.replace_column` | `column`: name, `formula`: expression | default | `{"column":name,"replaced":boolean}` | `browser.updated`, `done` | **Moved onto the registry at W526 (#590)**; `formula` is text (a sequence, `&x` and `@x` are not expressions). |
| `B` -> `U` | Data browser -> (U)nreplace | `{"cmd":"key","win":"browser","key":"u"}` | none | `browser.undo_replace` | none | default | `{"undone":boolean}` | `browser.updated`, `done` | **Moved onto the registry at W526 (#590)**. |
| `B` -> `T` | Data browser -> (T)able | `{"cmd":"key","win":"browser","key":"t"}` | `do_string_box_of` "Tabulate" ([core/browse_data.cpp:709](../../core/browse_data.cpp#L709)): Variable, Xlo, Xhi, then `file_selector` "Save table" "*.tab" | `browser.tabulate_column` | `variable`: name, `x_low`: real, `x_high`: real, `file`: file | default | `{"file":file,"rows_written":integer}` | `done` | **Moved onto the registry at W526 (#590)**; the range must have `x_low` below `x_high`. |
| `B` -> `H` | Data browser -> (H)ome: first | `{"cmd":"key","win":"browser","key":"h"}` | none | `browser.set_slice_start` | `row`: integer (>=0) | default | `{"first_row":integer}` | `browser.updated`, `done` | **Moved onto the registry at W526 (#590)**; selects the row too. |
| `B` -> `E` | Data browser -> (E)nd: last | `{"cmd":"key","win":"browser","key":"e"}` | none | `browser.set_slice_end` | `row`: integer (>=0) | default | `{"last_row":integer}` | `browser.updated`, `done` | **Moved onto the registry at W526 (#590)**; the row is the last one kept, inclusive, and selected. |
| `B` -> `S` | Data browser -> re(S)tore | `{"cmd":"key","win":"browser","key":"s"}` | none | `browser.restore_full_slice` | none | default | `{"first_row":0,"last_row":integer}` | `browser.updated`, `done` | **Moved onto the registry at W526 (#590)**: keeps all the data again and replots it, the key `s` the same. |
| `B` -> `A` | Data browser -> (A)dd column | `{"cmd":"key","win":"browser","key":"a"}` | `get_dialog` "Add Column": Name, then Formula: ([core/browse_data.cpp:412](../../core/browse_data.cpp#L412)) | `browser.add_column` | `name`: name, `formula`: expression | default | `{"column":name,"added":boolean}` | `browser.updated`, `done` | **Moved onto the registry at W526 (#590)**. |
| `B` -> `D` | Data browser -> (D)elete column | `{"cmd":"key","win":"browser","key":"d"}` | `get_dialog` "Delete added column": Name ([core/browse_data.cpp:371](../../core/browse_data.cpp#L371)) | `browser.delete_column` | `name`: name | default | `{"deleted":boolean}` | `browser.updated`, `done` | **Moved onto the registry at W526 (#590)**. |
| `B` -> `L` | Data browser -> (L)oad | `{"cmd":"browser","op":"load"}` | `file_selector` | `browser.load` | `file`: file | default | `{"file":file,"rows_loaded":integer}` | `browser.updated`, `done` | **Moved onto the registry at W526 (#590)**; `file` left out is asked for, `format` is an optional choice. |
| `B` -> `W` | Data browser -> (W)rite | `{"cmd":"browser","op":"write"}` | `menu_save_what` ("The data table", "What the plot shows"), `file_selector` | `browser.export` | `file`: file, `format`: choice from the supported export formats, `target`: optional choice("table","plot_curves") | default | `{"file":file,"rows_written":integer}` | `done` | **Moved onto the registry at W526 (#590)**; `file` left out is asked for, `target` also takes `output` (the model's output columns). |
| (Page scroll) | (Paging rows) | `{"cmd":"browser","from":N}` | none | `browser.page` | `from_row`: integer (>=0), `count`: integer (>0) | default | `{"first_row":integer,"total_rows":integer,"rows":list[list[real]]}` | `done` | **Moved onto the registry at W526 (#590)**; `first_column` and `columns` are optional too, `count` 0 stops the block's events. |
| `F` -> `T` | File -> Transpose data | `{"cmd":"key","menu":"file","item":"transpose"}` | `do_string_box_of` "Transpose Data" ([core/adj2.cpp:77](../../core/adj2.cpp#L77)): Column 1, NCols, ColSkip, Row 1, NRows, RowSkip | `browser.transpose` | `first_column`: name, `columns`: integer, `column_skip`: integer, `first_row`: integer, `rows`: integer, `row_skip`: integer | default | `{"transposed_rows":integer,"transposed_cols":integer}` | `browser.updated`, `done` | **Moved onto the registry at W526 (#590)**; a left-out argument keeps its last value, the first row is 0, what reads past the data is refused. |
| (Data panel) | (the model's `@ postprocess`) | `{"cmd":"browser","op":"postprocess"}` | none | `browser.postprocess` | none | default | `{"rows":integer}` | `browser.updated`, `done` | **Added at W526 (#590)**: the old op had no row; --silent runs it after its run. |

---

### 2.14 Resource `auto` (Bifurcation Continuation and AUTO Diagnostics)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `F` -> `A` | File -> AUTO continuation | `{"cmd":"key","menu":"file","item":"auto"}` | none | `auto.open` | none | default | `{"open":boolean}` | `auto.diagram`, `session.state`, `done` |
| `A` -> `R` (AUTO) | AUTO -> (R)un | `{"cmd":"key","win":"auto","key":"r"}` (the Run menus ask), or `{"cmd":"auto","op":"restart","label":N}` to go on from a stored point with no ask | `menu_auto_start` ("Steady state", "Periodic", "Bdry Value", "Homoclinic", "Heteroclinic") or special pt continuation | `auto.run` | `start_type`: optional choice("steady_state","periodic","boundary_value","homoclinic","heteroclinic"), `branch_type`: optional choice("switch","extend","new_point","two_parameter","fixed_period","doubling"), `direction`: optional choice("forward","backward","both") | default | `{"points_computed":integer,"branches_tracked":integer}` | `job.progress`, `auto.diagram`, `done` |
| `A` -> `G` (AUTO) | AUTO -> (G)rab | `{"cmd":"auto","op":"grab","label":N}` | an interactive `grab` ask (arrow keys, `Tab`, `Home`, `End`, a click, or `point`: a point of the `diagram` data), taken with `Return` | `auto.grab` | `label`: optional integer, `index`: optional integer, `run`: optional integer, `x`: optional real, `y`: optional real | default | `{"grabbed":boolean,"point":map[string, real]}` | `auto.diagram`, `done` |
| `A` -> `N` (AUTO) | AUTO -> (N)umerics | `{"cmd":"auto","op":"set",...}` | `do_string_box_of` "AutoNum" ([core/auto_nox.cpp:631](../../core/auto_nox.cpp#L631)): the numerics grid, the SuppBP checkbox and the twelve special-point switches (booleans `true`/`false`, W499) | `auto.configure_numerics` | `mesh_intervals`: optional integer, `max_points`: optional integer, `print_every`: optional integer, `collocation_points`: optional integer, `step_size`: optional real, `step_min`: optional real, `step_max`: optional real, `parameter_min`: optional real, `parameter_max`: optional real, `norm_min`: optional real, `norm_max`: optional real, `tolerance_parameters`: optional real, `tolerance_solution`: optional real, `tolerance_special`: optional real, `adapt_mesh_every`: optional integer, `branch_switch_limit`: optional integer, `output_detail`: optional integer, `locate_iterations`: optional integer, `newton_iterations`: optional integer, `full_newton_iterations`: optional integer, `adapt_step_every`: optional integer, `skip_branch_points`: optional boolean, `special_points`: optional map[name, boolean] (BT, CP, ZH, GH, R1..R4, LPD, LTR, PTR, TTR) | default | `{"settings_updated":boolean}` | `auto.settings`, `session.state`, `done` |
| `A` -> `A` (AUTO) | AUTO -> (A)xes | `{"cmd":"auto","op":"set",...}` | the plot-type menu (Hi, Norm, hI-lo, Period, Two par, last 1 par, last 2 par, fRequency, Average, and the view items below), then `do_string_box_of` "AutoPlot" ([core/auto_nox.cpp:733](../../core/auto_nox.cpp#L733)): Y axis, Par1, Par2, Xmin, Ymin, Xmax, Ymax | `auto.configure_plot` | `plot_type`: choice("maximum","norm","maximum_and_minimum","period","two_parameter","frequency","average") (the names `autosettings.axes.plot` uses), `variable`: name, `parameter1`: name, `parameter2`: optional name, `x_min`: real, `x_max`: real, `y_min`: real, `y_max`: real | default | `{"plot_configured":boolean}` | `auto.view`, `done` |
| `A` -> `U` (AUTO) | AUTO -> (U)sr period | `{"cmd":"auto","op":"set","marks":[...]}` | `new_string_of` count, prompts | `auto.configure_marks` | `marks`: list[map[string, real]] | default | `{"marks_count":integer}` | `auto.settings`, `done` |
| `A` -> `C` (AUTO) | AUTO -> (C)lear (the branches so far are hidden until shown again; the points stay) | `{"cmd":"key","win":"auto","key":"c"}`; the earlier branches are shown again by `{"cmd":"auto","op":"display","show":true}` | none | `auto.hide_earlier` | none | default | `{"hidden_points":integer}` | `auto.diagram`, `done` |
| `A` -> `D` (AUTO) | AUTO -> re(D)raw | `{"cmd":"key","win":"auto","key":"d"}`; `{"cmd":"redraw"}` also redraws the diagram | none | `auto.redraw` | none | default | `{"redrawn":boolean}` | `auto.diagram`, `done` |
| (Page: preferences) | (AUTO threads) | `{"cmd":"auto","op":"set","threads":"auto"\|N}` | none | `auto.set_threads` | `threads`: choice(`auto`) or integer (1..`threads_max`) | default | `{"threads":choice}` | `autosettings`, `done` |
| (Page: close AUTO) | (Close the AUTO window) | `{"cmd":"auto","op":"close"}` | none | `auto.close` | none | default | `{"closed":boolean}` | `diagram`, `done` |
| (Page: click on the diagram) | (Pick a point) | `{"cmd":"auto","op":"point","xd":X,"yd":Y}` (or pixels `x`, `y`) | none | `auto.pick` | `x`: real, `y`: real, `view`: optional name | default | `{"label":integer,"values":map[name, real]}` | `autoinfo`, `diagram`, `done` |
| (Page: diagram zoom) | (Display state of a view) | `{"cmd":"auto","op":"display","view":k,"x":[lo,hi],"y":[lo,hi],"show":B}` | none | `auto.set_display` | `view`: optional name, `x_range`: optional list[real, 2] or null, `y_range`: optional list[real, 2] or null, `show_earlier`: optional boolean | default | `{"x_range":list[real, 2],"y_range":list[real, 2]}` | `diagram`, `done` |
| (Page: view tabs) | (Close or activate a view) | `{"cmd":"auto","op":"view","close":k}` or `{"cmd":"auto","op":"view","active":k}` | none | `auto.close_view`, `auto.activate_view` | `view`: name | default | `{"views":integer}` | `diagram`, `done` |
| `A` -> `A` -> `V` | AUTO Axes -> new (V)iew | `{"cmd":"auto","op":"view","new":true}` | none | `auto.new_view` | none | default | `{"view":integer}` | `diagram`, `done` |
| `A` -> `A` -> `1`/`2` | AUTO Axes -> last 1 par / last 2 par | `{"cmd":"key","win":"auto","key":"a"}` then `1` or `2` | none | `auto.use_last_plot` | `parameters`: choice(`one`,`two`) | default | `{"plot":choice}` | `diagram`, `done` |
| `A` -> `A` -> `S` | AUTO Axes -> (S)croll | `{"cmd":"key","win":"auto","key":"a"}` then `s` | a `drag` ask loop (`down`, `move`, `up` with `xd`, `yd`) until cancelled; proposal: one command with `dx`, `dy` | `auto.scroll` | `dx`: real, `dy`: real, `view`: optional | default | `{"x_range":list[real, 2],"y_range":list[real, 2]}` | `diagram`, `done` |
| `A` -> `A` -> `D` | AUTO Axes -> (D)efault | `{"cmd":"key","win":"auto","key":"a"}` then `d` | none | `auto.default_view` | `view`: optional | default | `{"x_range":list[real, 2],"y_range":list[real, 2]}` | `diagram`, `done` |
| `A` -> `A` -> `F` | AUTO Axes -> (F)it | `{"cmd":"key","win":"auto","key":"a"}` then `f` | none | `auto.fit` | `view`: optional | default | `{"x_range":list[real, 2],"y_range":list[real, 2]}` | `diagram`, `done` |
| `A` -> `A` -> `O` | AUTO Axes -> Zoom (O)ut | `{"cmd":"key","win":"auto","key":"a"}` then `o` | `rubber` ask like Zoom in | `auto.zoom_out` | the box the view shrinks into, or `view` | default | `{"x_range":list[real, 2],"y_range":list[real, 2]}` | `diagram`, `done` |
| `A` -> `A` -> `Z` | AUTO Axes -> (Z)oom in | `{"cmd":"key","win":"auto","key":"a"}` then `z` | `rubber` ask (`flag` 0) in the diagram, answered with `xd`, `yd`, `xd2`, `yd2` | `auto.zoom` | `x_min`: real, `x_max`: real, `y_min`: real, `y_max`: real, `view`: optional integer (a view name in the registry) | default | `{"x_range":list[real, 2],"y_range":list[real, 2]}` | `diagram`, `done` |
| `A` -> `F` -> `C` | AUTO File -> Clear grab | `{"cmd":"key","win":"auto","key":"c"}` in the File menu | none | `auto.clear_grab` | none | default | `{"cleared":boolean}` | `auto.diagram`, `done` |
| `A` -> `F` -> `I` | AUTO File -> Import orbit | `{"cmd":"key","win":"auto","key":"i"}` | none | `auto.load_orbit_into_browser` | `label`: optional integer | default | `{"orbit_loaded":boolean,"rows":integer}` | `browser.updated`, `done` |
| `A` -> `F` -> `S` | AUTO File -> Save diagram | `{"cmd":"session","op":"save"}` | file save prompt (.snapx) | `auto.save_session` | `file`: optional file | default | `{"file":file,"saved":boolean}` | `done` |
| `A` -> `F` -> `L` | AUTO File -> Load diagram | `{"cmd":"session","op":"open"}` | file open prompt (.snapx) | `auto.open_session` | `file`: file | default | `{"file":file,"loaded":boolean}` | `auto.diagram`, `session.state`, `done` |
| `A` -> `F` -> `P` | AUTO File -> PDF | `{"cmd":"key","win":"auto","key":"p"}` in the File menu, the picture form and the file ask | typed form "Picture parameters" (Colour, Axes font size), then `file_selector` for the `.pdf` | `auto.export_pdf` | `file`: file, `window`: window | default | `{"file":file}` | `done` |
| `A` -> `F` -> `V` | AUTO File -> SVG | `{"cmd":"key","win":"auto","key":"v"}` | the same typed form "Picture parameters", then `file_selector` for the `.svg` | `auto.export_svg` | `file`: file | default | `{"file":file,"bytes_written":integer}` | `done` |
| `A` -> `F` -> `R` | AUTO File -> Reset diagram | `{"cmd":"key","win":"auto","key":"r"}` | `TwoChoice` "Destroy AUTO diagram & files" ([core/auto_nox.cpp:1330](../../core/auto_nox.cpp#L1330)) | `auto.reset_all` | none | default | `{"reset":boolean}` | `auto.diagram`, `done` |
| `A` -> `F` -> `W` | AUTO File -> Write pts | `{"cmd":"key","win":"auto","key":"w"}` | `file_selector` | `auto.export_points` | `file`: file | default | `{"file":file,"points_written":integer}` | `done` |
| `A` -> `F` -> `A` | AUTO File -> All info | `{"cmd":"key","win":"auto","key":"a"}` | `file_selector` | `auto.export_all_info` | `file`: file | default | `{"file":file,"written":boolean}` | `done` |
| `A` -> `F` -> `D` | AUTO File -> init Data | `{"cmd":"key","win":"auto","key":"d"}` | `file_selector` | `auto.export_initial_data` | `file`: file | default | `{"file":file,"written":boolean}` | `done` |
| `A` -> `F` -> `T` | AUTO File -> Toggle redraw | `{"cmd":"key","win":"auto","key":"t"}` | none | `auto.set_auto_redraw` | `enabled`: boolean | default | `{"auto_redraw":boolean}` | `done` |
| `A` -> `F` -> `N` | AUTO File -> auto raNge | `{"cmd":"key","win":"auto","key":"n"}` | none | `auto.range_branch` | `branch`: integer | default | `{"runs_completed":integer}` | `auto.diagram`, `done` |
| `A` -> `F` -> `E` | AUTO File -> sElect 2par pt | `{"cmd":"key","win":"auto","key":"e"}` | none | `auto.select_2parameter_point` | `label`: integer | default | `{"selected":boolean}` | `auto.diagram`, `done` |
| `A` -> `F` -> `B` | AUTO File -> draw laBled | `{"cmd":"key","win":"auto","key":"b"}` | none | `auto.draw_labeled_orbits` | none | default | `{"orbits_drawn":integer}` | `plot.series`, `done` |
| `A` -> `F` -> `O` | AUTO File -> lOad branch | `{"cmd":"key","win":"auto","key":"o"}` | none | `auto.load_branch_into_browser` | `branch`: integer | default | `{"rows_loaded":integer}` | `browser.updated`, `done` |
| `A` -> `F` -> `X` | AUTO File -> eXport CSV | `{"cmd":"key","win":"auto","key":"x"}` | `file_selector` "Export CSV" "*.csv" | `auto.export_csv` | `file`: file | default | `{"file":file,"points_written":integer}` | `done` |
| `A` -> `F` -> `F` | AUTO File -> Import diagram | `{"cmd":"key","win":"auto","key":"f"}` | `file_selector` "Import XPPAUT diagram" "*.auto" | `auto.import_diagram` | `file`: file | default | `{"file":file,"points_imported":integer}` | `auto.diagram`, `done` |

---

Moved at W516 ([#580](https://github.com/MuhammadMoustafa/xppautX/issues/580)): every row of this table is on the registry, in `core/commands_auto.cpp`, and the old `{"cmd":"auto","op":...}` forms are removed. `auto.close`, `auto.close_view` and `auto.activate_view` are `window.close` and `window.select` (W353); `auto.run` is the row of `A` -> `R` and takes the kind, `switch`, the start and the Numerics by their W531 names.

### 2.15 Resource `animation` (Custom Geometric Animations)

**Moved onto the registry (W527, #591): every row below is implemented under its proposed name except `animation.close`, which `window.hide` and `window.destroy` already do for any window.** Readings that differ from the row: `animation.set_frame_skip`'s `skip` is at least 1 (0 would never move); `animation.export_frames` plays the animation writing every frame, so exactly one of `ppm` and `animated_gif` is true, and its result is `frames_saved`; `animation.grab_point` presses at `x`, `y` (the animation's own coordinates), moves to the optional `to_x`, `to_y` and releases, in one command; `animation.set_speed`, `animation.pause`, `animation.faster` and `animation.slower` answer `speed_ms`, `animation.step` (`rows`, negative goes back) and `animation.seek` answer `row`; the steering commands taken by a running Go have their `done` when it ends.

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `V` -> `T` | Axes -> Toon | `{"cmd":"key","key":"t"}` | none | `animation.open` | none | default | `{"open":boolean}` | `session.state`, `done` | **Moved onto the registry at W527 ([#591](https://github.com/MuhammadMoustafa/xppautX/issues/591)).**
| (Ani window) `F` | Animation -> (F)ile | `{"cmd":"key","win":"ani","key":"f"}` | `file_selector` "*.ani" | `animation.load` | `file`: file | default | `{"file":file,"loaded":boolean}` | `session.state`, `done` | **Moved onto the registry at W527 ([#591](https://github.com/MuhammadMoustafa/xppautX/issues/591)).**
| (Ani window) `G` | Animation -> (G)o | `{"cmd":"key","win":"ani","key":"g"}` | none | `animation.play` | none | default | `{"playing":boolean}` | `done` | **Moved onto the registry at W527 ([#591](https://github.com/MuhammadMoustafa/xppautX/issues/591)).**
| (Ani window) `R` | Animation -> (R)eset | `{"cmd":"key","win":"ani","key":"r"}` | none | `animation.reset` | none | default | `{"frame":0}` | `done` | **Moved onto the registry at W527 ([#591](https://github.com/MuhammadMoustafa/xppautX/issues/591)).**
| (Ani window) `S` | Animation -> (S)kip | `{"cmd":"key","win":"ani","key":"s"}` | `get_dialog_of` "Frame skip": Increment: ([core/aniparse.cpp:1044](../../core/aniparse.cpp#L1044)) | `animation.set_frame_skip` | `skip`: integer (>=0) | default | `{"skip":integer}` | `done` | **Moved onto the registry at W527 ([#591](https://github.com/MuhammadMoustafa/xppautX/issues/591)).**
| (Ani window) `M` | Animation -> (M)peg | `{"cmd":"key","win":"ani","key":"m"}` | `do_string_box_of` "Frame saving" ([core/aniparse.cpp:1020](../../core/aniparse.cpp#L1020)): PPM 0/1, AniGif(0/1), then `file_selector` "Save animation frames" "*.ppm" | `animation.export_frames` | `ppm`: optional boolean, `animated_gif`: optional boolean, `file`: file | default | `{"frames_saved":integer}` | `done` | **Moved onto the registry at W527 ([#591](https://github.com/MuhammadMoustafa/xppautX/issues/591)).**
| (Ani window) `O` | Animation -> (O)n the fly | `{"cmd":"key","win":"ani","key":"o"}` | none | `animation.set_live_draw` | `enabled`: boolean | default | `{"live_draw":boolean}` | `done` | **Moved onto the registry at W527 ([#591](https://github.com/MuhammadMoustafa/xppautX/issues/591)).**
| (Ani window) `A` | Animation -> gr(A)b | `{"cmd":"key","win":"ani","key":"a"}`, then `{"cmd":"ani","op":"mouse","what":"down"\|"move"\|"up","u":U,"v":V}` | pointer events in the animation's unit coordinates (`u`, `v`) or pixels | `animation.grab_point` | `x`: real, `y`: real | default | `{"grabbed":boolean}` | `ani`, `done` | **Moved onto the registry at W527 ([#591](https://github.com/MuhammadMoustafa/xppautX/issues/591)).**
| (Ani window) | (Close the window) | `{"cmd":"ani","op":"close"}` | none | `animation.close` | none | default | `{"closed":boolean}` | `ani`, `done` | **Not a command of its own at W527 ([#591](https://github.com/MuhammadMoustafa/xppautX/issues/591)): `window.hide` and `window.destroy` close any window (W515).**
| (Ani window) | (Speed, step and seek) | `{"cmd":"ani","op":"speed","ms":N}`, `{"cmd":"ani","op":"step","n":N}`, `{"cmd":"ani","op":"seek","pos":N}` | none | `animation.set_speed`, `animation.step`, `animation.seek` | `ms`: integer (0..1000), `rows`: integer, `row`: integer (>=0) | default | `{"row":integer}` | `ani`, `done` | **Moved onto the registry at W527 ([#591](https://github.com/MuhammadMoustafa/xppautX/issues/591)).**
| (Ani window) | (Pause / faster / slower) | `{"cmd":"ani","op":"pause"\|"fast"\|"slow"}` | none | `animation.pause`, `animation.faster`, `animation.slower` | none | default | `{"speed_ms":integer}` | `ani`, `done` | **Moved onto the registry at W527 ([#591](https://github.com/MuhammadMoustafa/xppautX/issues/591)).**

---

### 2.16 Resource `array_plot` (Spatiotemporal Plots)

**Moved onto the registry (W527, #591): every row below is implemented under its proposed name except `array_plot.close`, which `window.hide` and `window.destroy` already do for any window.** Readings that differ from the row: `array_plot.configure`'s `z_min` must be below `z_max` and every count is at least 1 (an argument left out keeps the plot's); `array_plot.export_pdf` also takes the three labels the Print form held (`top_label`, `side_label`, `bottom_label`); `array_plot.range`'s arguments are `still`, `tag`, `file` and those of `run.range`, and it answers the runs completed as `steps_completed`; `array_plot.scroll` takes the absolute `first_row`, which the `aplot` event now carries.

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `V` -> `A` | Axes -> Array | main keys `v`, `a` | none | `array_plot.open` | none | default | `{"open":boolean}` | `session.state`, `done` | **Moved onto the registry at W527 ([#591](https://github.com/MuhammadMoustafa/xppautX/issues/591)).**
| (Aplot window) `D` | Array plot -> re(D)raw | `{"cmd":"key","win":"aplot","key":"d"}` | none | `array_plot.redraw` | none | default | `{"redrawn":boolean}` | `done` | **Moved onto the registry at W527 ([#591](https://github.com/MuhammadMoustafa/xppautX/issues/591)).**
| (Aplot window) `E` | Array plot -> (E)dit | `{"cmd":"key","win":"aplot","key":"e"}` | `do_string_box_of` "Edit arrayplot" ([core/arrayplot.cpp:243](../../core/arrayplot.cpp#L243)): Column 1, NCols, Row 1, NRows, RowSkip, Zmin, Zmax, Autoplot(0/1), ColSkip | `array_plot.configure` | `first_column`: name, `columns`: integer, `first_row`: integer, `rows`: integer, `row_skip`: integer, `z_min`: real, `z_max`: real, `auto_redraw`: boolean, `column_skip`: integer (ordered `z_min` below `z_max`; the palette is the session's `colormap`) | default | `{"configured":boolean}` | `done` | **Moved onto the registry at W527 ([#591](https://github.com/MuhammadMoustafa/xppautX/issues/591)).**
| (Aplot window) `F` | Array plot -> (F)it | `{"cmd":"key","win":"aplot","key":"f"}` | none | `array_plot.fit` | none | default | `{"fitted_min":real,"fitted_max":real}` | `done` | **Moved onto the registry at W527 ([#591](https://github.com/MuhammadMoustafa/xppautX/issues/591)).**
| (Aplot window) `R` | Array plot -> (R)ange | `{"cmd":"key","win":"aplot","key":"r"}` | `do_string_box_of` "Array range saving" ([core/arrayplot.cpp:67](../../core/arrayplot.cpp#L67)): Still(1/0), Tag(0/1), then `file_selector` "Save array frames" "*.gif", then the Range Integrate form; one GIF per run | `array_plot.range` | `still`: boolean, `tag`: boolean, `file`: file, and the arguments of `run.range` | default | `{"steps_completed":integer}` | `done` | **Moved onto the registry at W527 ([#591](https://github.com/MuhammadMoustafa/xppautX/issues/591)).**
| (Aplot window) `P` | Array plot -> Print PDF | `{"cmd":"key","win":"aplot","key":"p"}` | the Picture parameters form and `file_selector` "Save array plot" "*.pdf" | `array_plot.export_pdf` | `file`: file | default | `{"file":file}` | `done` | **Moved onto the registry at W527 ([#591](https://github.com/MuhammadMoustafa/xppautX/issues/591)).**
| (Aplot window) `G` | Array plot -> (G)IF | `{"cmd":"key","win":"aplot","key":"g"}` | `file_selector` "Save array frames" "*.gif" (frames are `pixels` asks) | `array_plot.export_gif` | `file`: file | default | `{"file":file,"bytes_written":integer}` | `done` | **Moved onto the registry at W527 ([#591](https://github.com/MuhammadMoustafa/xppautX/issues/591)).**
| (Aplot window) | (Drag to scroll through time / close) | `{"cmd":"aplot","op":"scroll","dy":PIXELS}`, `{"cmd":"aplot","op":"close"}` | none; scrolling is in pixels today | `array_plot.scroll`, `array_plot.close` | `first_row`: integer (replaces the pixel `dy`) | default | `{"first_row":integer}` | `aplot`, `done` | **Moved onto the registry at W527 ([#591](https://github.com/MuhammadMoustafa/xppautX/issues/591)).**

---

### 2.17 Resource `set` (Named Parameter, State, and Options Sets)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `F` -> `G` | File -> Apply set… | `{"cmd":"applyset","set":N,"scope":S,"win":G}` | `pick_from_list` set, scope and (with two windows open and scope `all` or `plot`) the window ([core/commands.cpp:137](../../core/commands.cpp#L137)) | `set.apply` | `name`: text, `scope`: optional choice("all","parameters","initial","integration","poincare","equilibria","continuation","plot","sweep","analysis"), `window`: optional window | default | `{"applied":text,"scope":text}` | `session.state`, `done` | **Moved onto the registry at W526 (#590)**; `scope` is required, `window` names the graph for `all` and `plot`. |
| `F` -> `O` | File -> Save values… | `{"cmd":"saveset","set":N,"scope":S,"win":G,"state":S}` | prompts for mode (new/replace), name, scope, graph, initial/last | `set.save_new` | `name`: text, `scope`: optional choice("all","parameters","initial","integration","poincare","equilibria","continuation","plot","sweep","analysis"), `window`: optional window, `state`: optional choice("initial","last") | default | `{"name":text,"saved":boolean}` | `session.state`, `done` | **Moved onto the registry at W526 (#590)**; `scope` is required. |
| `F` -> `O` (replace) | File -> Save values… | `{"cmd":"saveset"}` | confirm replacement | `set.save_into` | `name`: text, `scope`: optional choice("all","parameters","initial","integration","poincare","equilibria","continuation","plot","sweep","analysis"), `window`: optional window, `state`: optional choice("initial","last") | default | `{"name":text,"updated":boolean}` | `session.state`, `done` | **Moved onto the registry at W526 (#590)**; `scope` is required. |
| `F` -> `J` | File -> Save as defaults… | `{"cmd":"savedefaults","scope":S,"win":G,"state":S}` | prompts for scope, graph, initial/last | `set.save_defaults` | `scope`: optional choice("all","parameters","initial","integration","poincare","equilibria","continuation","plot","sweep","analysis"), `window`: optional window, `state`: optional choice("initial","last") | default | `{"saved_as_defaults":boolean}` | `session.state`, `done` | **Moved onto the registry at W526 (#590)**; `scope` is required. |
| `F` -> `I` | File -> Export values… | `{"cmd":"exportset","file":F,"set":N,"scope":S,...}` | prompts for scope, name, file | `set.export` | `file`: file, `name`: text, `scope`: optional choice("all","parameters","initial","integration","poincare","equilibria","continuation","plot","sweep","analysis"), `window`: optional window, `state`: optional choice("initial","last") | default | `{"file":file,"name":text,"exported":boolean}` | `done` | **Moved onto the registry at W526 (#590)**; `scope` is required. |
| `F` -> `Z` | File -> Rename set… | `{"cmd":"renameset","set":A,"to":B}` | prompts for existing set, new name | `set.rename` | `from`: text, `to`: text | default | `{"from":text,"to":text,"renamed":boolean}` | `session.state`, `done` | **Moved onto the registry at W526 (#590)**. |
| `F` -> `F` | File -> Delete set… | `{"cmd":"deleteset","set":N}` | prompt for set, confirm | `set.delete` | `name`: text | default | `{"name":text,"deleted":boolean}` | `session.state`, `done` | **Moved onto the registry at W526 (#590)**. |
| `F` -> `K` | File -> Restore defaults… | `{"cmd":"restoredefaults","scope":S,"win":G}` | prompts for scope, graph | `set.restore_defaults` | `scope`: optional choice("all","parameters","initial","integration","poincare","equilibria","continuation","plot","sweep","analysis"), `window`: optional window | default | `{"restored":boolean}` | `session.state`, `done` | **Moved onto the registry at W526 (#590)**; `scope` is required and `field` limits it to one parameter, state or setting. |

---

### 2.18 Resource `value` (Parameters, Sliders, and Values Undo/Redo)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `P` | Parameters | `{"cmd":"set","kind":"par",...}` / `{"cmd":"set","values":[...]}` | string box per parameter | `value.set_parameters` | `values`: map[name, real] | default | `{"updated":map[name, real]}` | `session.state`, `done` | **Moved onto the registry at W526 (#590)**. |
| Ctrl+Z / `Z` | Undo value edit | `{"cmd":"key","menu":"main","item":"undo"}` | none | `value.undo` | none | default | `{"undone":boolean}` | `session.state`, `done` | **Moved onto the registry at W526 (#590)**; nothing to undo is an error. |
| Ctrl+Y / `Y` | Redo value edit | `{"cmd":"key","menu":"main","item":"redo"}` | none | `value.redo` | none | default | `{"redone":boolean}` | `session.state`, `done` | **Moved onto the registry at W526 (#590)**; nothing to redo is an error. |
| (Values panel) | (Interactive slider) | `{"cmd":"slide","name":N,"value":V}` | none | `value.slide` | `name`: name, `value`: real | default | `{"name":name,"value":real}` | `session.state`, `done` | **Moved onto the registry at W526 (#590)**. |
| (Values panel) | (Configure slider) | `{"cmd":"slider","slot":S,"name":N,"lo":L,"hi":H,"step":T}` | none | `value.configure_slider` | `slot`: integer (>=0), `name`: name, `low`: real, `high`: real, `step`: optional real | default | `{"slot":integer,"configured":boolean}` | `session.state`, `done` | **Moved onto the registry at W526 (#590)**; `step` 0 is the page's own, left out keeps the slot's. |
| (Values panel) | (Save / load the parameters or initial conditions as a file) | `{"cmd":"values","op":"write"\|"read","kind":"par"\|"ic","name":F}` | `file_selector` when `name` is omitted; the load is all or nothing | `value.save_file`, `value.load_file` | `kind`: choice(`parameters`,`initial_conditions`), `file`: file | default | `{"file":file,"kind":choice}` | `session.state`, `done` | **Moved onto the registry at W526 (#590)**; one command each for `value.save_file` and `value.load_file`, `file` left out is asked for. |
| (Values panel) | (a parameter typed as a %formula) | `{"cmd":"set","kind":"par","text":"%..."}` | the Parameters box | `value.set_by_formula` | `name`: name, `formula`: expression | default | `{"name":name,"value":real}` | `session.state`, `done` | **Added at W526 (#590)**: the formula is worked out once, the parameter keeps the number (as `ic.formula`'s counterpart for a parameter). |
| (Values panel) | (Remove a slider) | `{"cmd":"slider","slot":S,"name":""}` | none | `value.remove_slider` | `slot`: integer (>=0) | default | `{"slot":integer,"removed":boolean}` | `session.state`, `done` | **Added at W526 (#590)**: an empty name no longer stands for removal. |

---

### 2.19 Resource `session` (Session Files, Models, and System Lifecycle)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| Ctrl+O / `F` -> `M` | File -> Open model… | `{"cmd":"open","file":F}` / `{"cmd":"open","example":E}` | `file_selector` "*.ode;*.odex" | `session.open_model` | `file`: optional file, `example`: optional text | mutex(`file`, `example`) | `{"model":text,"variables":list[name],"parameters":list[name]}` | `session.state`, `done` |
| Ctrl+R / `F` -> `E` | File -> Reload model | `{"cmd":"reload"}` | none | `session.reload_model` | none | default | `{"reloaded":boolean}` | `session.state`, `done` |
| `F` -> `N` | File -> Open session… | `{"cmd":"open","file":F}` | `file_selector` "*.snapx" | `session.open` | `file`: file | default | `{"file":file,"loaded":boolean}` | `session.state`, `done` |
| Ctrl+S / `F` -> `V` | File -> Save session | `{"cmd":"session","op":"save"}` | file prompt if first save | `session.save` | none | default | `{"file":file,"saved":boolean}` | `session.state`, `done` |
| Ctrl+Shift+S / `F` -> `W` | File -> Save session as… | `{"cmd":"session","op":"save","name":F}` | `file_selector` "*.snapx" | `session.save_as` | `file`: file | default | `{"file":file,"saved":boolean}` | `session.state`, `done` |
| `F` -> `B` | File -> Save a copy of the session… | `{"cmd":"key","menu":"file","item":"savesessioncopy"}` | `file_selector` "*.snapx" | `session.save_copy` | `file`: file | default | `{"file":file,"copied":boolean}` | `done` |
| `F` -> `R` | File -> Import settings | File menu key `r`, then file ask | `file_selector` "Import settings" "*.setx *.set" | `session.import_settings` | `file`: file | default | `{"file":file,"settings_applied":integer}` | `session.state`, `done` |
| `F` -> `S` | File -> Export simulation information | `{"cmd":"key","menu":"file","item":"saveinfo"}` | `file_selector` "Save info" "*.pars*" | `session.export_info` | `file`: file | default | `{"file":file,"written":boolean}` | `done` |
| `F` -> `P` | File -> Model source | `{"cmd":"equations"}` | none | `session.get_source` | none | default | `{"source":text,"equations":list[text]}` | `done` |
| Ctrl+Q / `F` -> `Q` | File -> Quit | `{"cmd":"quit","save":B}` | `confirm` "Save session first?" | `session.quit` | `save`: optional boolean | default | `{"quitting":true}` | `done` |
| (Client init) | (State query) | `{"cmd":"state"}` | none | `session.get_state` | none | default | (full state object) | `session.state`, `done` |

---

### 2.20 Resource `recording` (Replayable Session Steps)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `F` -> `D` | File -> Record steps | `{"cmd":"record","op":"start"}` | none: recording starts at once with the session as it is now (W59d) | `recording.start` | none | default | `{"file":file,"recording":true}` | `session.state`, `done` |
| `F` -> `D` (stop) | File -> Record steps | `{"cmd":"record","op":"stop"}` | `file_selector` "Save recording" "*.recx" | `recording.stop` | `file`: optional file | default | `{"file":file,"steps_recorded":integer}` | `session.state`, `done` |
| (Page title bar) | (Add note to recording) | `{"cmd":"record","op":"note","text":T}` | string input dialog | `recording.add_note` | `note`: text | default | `{"note_added":boolean}` | `done` |
| `F` -> `Y` | File -> Play recording… | `{"cmd":"play","op":"open","file":F}` | `file_selector` "*.recx" | `recording.open` | `file`: file | default | `{"file":file,"total_steps":integer}` | `session.state`, `done` |
| (Player bar) | (Start play) | `{"cmd":"play","op":"start"}` | none | `recording.play` | none | default | `{"playing":boolean}` | `done` |
| (Player bar) | (Pause play) | `{"cmd":"play","op":"pause"}` | none | `recording.pause` | none | default | `{"paused":boolean}` | `done` |
| (Player bar) | (Step play) | `{"cmd":"play","op":"step"}` | none | `recording.step` | none | default | `{"step_index":integer}` | `done` |
| (Player bar) | (Set playback speed) | `{"cmd":"play","op":"speed","speed":N}` | speed selector | `recording.set_speed` | `speed`: real (0.25..8) | default | `{"speed":text}` | `done` |
| (Player bar) | (Jump to a step, optionally playing) | `{"cmd":"play","op":"from","step":N,"play":B}` | none | `recording.seek` | `step`: integer (>=0), `play`: optional boolean | default | `{"step":integer}` | `player`, `done` |
| (Player bar) | (Close player) | `{"cmd":"play","op":"close"}` | none | `recording.close` | none | default | `{"closed":boolean}` | `session.state`, `done` |
| (Player bar) | (Edit a step's note) | `{"cmd":"play","op":"note","step":N,"text":T}` | none | `recording.set_step_note` | `step`: integer (>=0), `note`: text | default | `{"step":integer}` | `player`, `done` |

---

### 2.21 Resource `keymap` (Keyboard Bindings and Quick Access)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| (Page command palette) | Keyboard shortcuts… | `{"cmd":"keymap"}` | keymap editor dialog | `keymap.get` | none | default | `{"bindings":map[text, list[text]],"pinned":list[text],"preset":text}` | `done` |
| (Keymap editor) | (Save bindings) | `{"cmd":"keymap","op":"set","map":{...}}` | none | `keymap.set` | `bindings`: optional map[text, list[text]], `pinned`: optional list[text], `preset`: optional choice("default","xppaut") | default | `{"saved":boolean}` | `session.state`, `done` |
| (Keymap editor) | (Reset shortcuts) | `{"cmd":"keymap","op":"reset"}` | confirm | `keymap.reset` | none | default | `{"reset":true}` | `session.state`, `done` |

---

### 2.22 Resource `events` (Event Streams and Protocol Subscriptions)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| (Client handshake) | (Subscribe heavy events) | `{"cmd":"data","events":[...]}` | none | `events.subscribe` | `events`: list of named supported streams (`hello.features`), `encoding`: optional choice(`json`,`f32`), `diagram`: optional boolean (today the `diagram` field of `data`, undocumented in docs/protocol.md) | default | `{"subscribed":list[text]}` | `done` |
| (Client handshake) | (Unsubscribe heavy events) | `{"cmd":"data","events":[]}` | none | `events.unsubscribe` | `events`: list of named supported streams | default | `{"unsubscribed":list[text]}` | `done` |

---

### 2.23 Resource `tool` (Auxiliary Utilities)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `F` -> `C` | File -> Calculator | `{"cmd":"key","menu":"file","item":"calculator"}` | `new_string_of` expression loop ([core/json_prompts.cpp:588](../../core/json_prompts.cpp#L588)) | `tool.calculator` | `expression`: expression | default | `{"expression":expression,"value":real}` | `done` |
| (Page: Model panel) | (An active comment of the model source) | `{"cmd":"action","index":N}` | none | `tool.run_source_action` | `index`: integer (>=0) | default | `{"ran":boolean}` | `session.state`, `done` |
| (Page: model buttons) | (A user button of the model, an `@ button`) | `{"cmd":"userbut","index":N}` | none | `tool.run_user_button` | `index`: integer (>=0) | default | `{"ran":boolean}` | `session.state`, `done` |

### 2.24 Current additions and data-space interaction contract

The following supplements the rows above. Each proposed operation takes
named windows and data-space values and returns structured data to any
server client; none requires the page. `done` and dotted event names are
proposals. Today the event names are `series`, `plots`, `nullclines`,
`dfield`, `marks`, `diagram`, `aplot` and `ani`, with `state` and `idle`.

| Operation / proposed command | Today and gap | Arguments and result for the registry |
|---|---|---|
| Zoom / rubber band: `window.zoom` | `display` sets data ranges; legacy rubber asks accept `xd`, `yd`, `xd2`, `yd2`. | Named window and ordered plot-coordinate bounds; return bounds; `plots`. |
| Pan: `window.scroll` | Legacy drag asks accept data coordinates; `display` sets the resulting range. | Named window and data-coordinate offsets; return bounds; `plots`. |
| Use this view: `window.use_view` | Page answers Window/Window's form; no direct command. `display` is display state, not model bounds. | Named window and ordered visible ranges; apply and return model bounds; `plots`. |
| Fit: `window.fit` | Window/Fit key; display zoom may be client-owned. | Named window; return fitted data bounds; `plots`. |
| Pick point: `auto.pick` | `auto point` accepts `xd`, `yd`; grab takes label or type/index; grab asks accept data coordinates or a point index. | Named diagram and data coordinates or point identity; return selected point and values; `diagram`. |
| Text / label: `annotation.add_text`, `annotation.edit`, `annotation.move`, `annotation.delete` | `text` already accepts plot coordinates and emits `marks`; menu placement asks for mouse. | Named window, text or ID, data coordinates and typed style; return ID and updated mark; `marks`. |
| Arrow / pointer / marker: `annotation.add_arrow`, `annotation.add_pointer`, `annotation.add_marker`, `annotation.edit` | Menu operations ask for mouse or rubber coordinates; `text` is the only direct command, and only labels have an `id` in `marks` (arrows and markers have none). | Named window, data points, named shape, colour and style; return mark ID and geometry; `marks`. |
| Marks computed from data: `plot.add_data_markers` (markers every N rows of the first curve), equilibrium marks (made by `equilibrium.find`, `equilibrium.range` and `equilibrium.search`, cleared by `window.clear`), frozen curves | Marker(S) asks a form; `marks.equilibria` and `marks.frozen` carry the rest; no direct command makes or removes an equilibrium mark. | Named window, row selection by `first_row`, `count`, `stride`; return the markers; `marks`. |
| Curves: `plot.freeze_curve`, `plot.edit_frozen`, `plot.delete_frozen`, `plot.delete_curve` | Menu forms and slots today; `marks` carries frozen curves. | Named window and curve identity, data and typed style; return changed identity; `series`, `marks`. |
| Nullclines: `nullcline.compute`, `nullcline.freeze`, `nullcline.range_freeze`, `nullcline.animate` | Menu and typed form asks today; animation has no general direct command. | Named window, variable names, range and frame selection; return segments and frames in plot coordinates; `nullclines`. |
| Direction field: `dirfield.compute`, `dirfield.flow`, `dirfield.clear` | Menu and grid ask today; `dfield` carries geometry. | Named window, grid at least 2 and named mode; return vectors/trajectories in plot coordinates; `dfield`, `series`. |
| 3D view: `plot.set_3d_box` | Form asks for bounds and angles; page answers the 3D turn. | Named window, ordered data-space box and real angles; return box and angles; `plots`. |
| Picture: `plot.export_pdf`, `plot.export_svg`, `auto.export_pdf`, `auto.export_svg` | Menu, form and file asks; core picture writer handles PDF/SVG. | Named graph or diagram, file and typed picture settings; return file; data remains available in `series`, `plots`, `marks` or `diagram`. |
| GIF / kinescope: `kinescope.capture`, `kinescope.export_gif` | Kinescope and GIF currently request `pixels` from the client. | Named window, data-frame selection and output file; return frame metadata and file. A core data-to-frame renderer is needed to eliminate the pixel ask. |
| Array plot: `array_plot.configure`, `array_plot.fit`, `array_plot.export_pdf`, `array_plot.export_gif` | Window keys/forms; `aplot` emits array data; scroll currently takes pixel `dy`. | Typed variables, data/time range and named palette; return array values/ranges and output file; `aplot`. Replace pixel scrolling with a time range. |
| Animation: `animation.play`, `animation.grab_point`, `animation.export_frames` | `ani` emits unit-coordinate primitives; `ani mouse` accepts `u`, `v`; frame export can ask for pixels. | Named animation, unit or model coordinates, frame range and file; return frame primitives and output metadata; `ani`. Core rendering is needed for page-free pixel output. |

Other operations added or materially changed since the original inventory:

| Current operation | Registry proposal / constraint |
|---|---|
| `colormap` with `name`, choices in `hello.colormaps` | `plot.set_colormap`: named palette; current list includes viridis and additional maps. |
| `data` subscription to `ani`, `autoinfo`, `autosettings`, `numerics`, `player` | `events.subscribe`: accept named streams; `diagram` is sent by AUTO independently. |
| `auto` `view` and `display`, `grab` by label or type/index | Named diagram views; return view bounds and selected point. Today's view IDs are numeric. |
| `file` list/get/put | `file.list`, `file.get`, `file.put`: base names and typed contents; return file data/status. |
| `colormap`, typed `text`, `steady`, direct `continue`, and `aplot`/`ani` controls | Keep direct typed operations in the registry; do not force them through keys or dialogs. |

Validation applies equally to forms and commands: W494 requires ordered
bounds (including 3D, histogram and AUTO axes), `min_step <= max_step`,
`nplot` 1..8, `dfgrid` and `histbins` at least 2, positive `torus_period`
and `rangestep` at least 1. AUTO also checks signed step magnitude and
ordered limits. W485 typed form fields include booleans, named choices and
numeric ranges; validate the whole answer before applying it. W499 names
`findext` as `both`, `maximum`, or `minimum` and makes `SuppBP` boolean.
W498 stop diagnostics carry the cause and place, including no Poincare
crossing, Monte Carlo counts and failed manifolds; proposed results must
preserve those diagnostics instead of reporting every early stop as success.
With a page open, W501 leaves warnings/errors and the browser address on the
terminal; verbose/debug includes the full log. Server and silent output are
unchanged.

## Using the commands without the page

Everything below is in [docs/protocol.md](../protocol.md); a claim marked
"missing today" is not.

1. **Start and wait.** `xppautX --server model.odex` (or `.ode`) reads one
   JSON object per line on stdin and writes events on stdout; stderr is the
   log. A client reads until `{"ev":"idle"}` after every command it sends
   (the first `idle` follows the model's load). A Python client needs only
   `subprocess.Popen(..., stdin=PIPE, stdout=PIPE, text=True)` and
   `json.dumps`/`json.loads`; `tools/servercheck.py` and `tools/xppclient.py`
   are working examples.
2. **Ask for data, not pictures.** Send
   `{"cmd":"data","events":["series","plots","nullclines","dfield","marks"]}`
   (add `"enc":"f32"` for base64 float32 columns). Each event arrives at the
   end of every command that changes it.
3. **Run.** `{"cmd":"key","menu":"main","item":"run_initial"}` (the
   `hello.command_table` id; `run_last`, `steady`, `continue` with `until` or
   `extra` are the same kind of command). Read to `idle`.
4. **Plot from the data.** The `series` event carries each window's curves
   as numeric columns, `plots` the windows' bounds, axes and variables,
   `nullclines` and `dfield` the phase plane's segments and arrows in plot
   coordinates, `marks` the equilibria, labels, arrows, markers and frozen
   curves, `diagram` the AUTO points. Draw them with any plotting library;
   nothing needs a pixel.
5. **Annotate.** `{"cmd":"text","op":"add","text":"peak","x":1,"y":2}`
   (plot coordinates; `win` is the plot window's number, 1 when omitted);
   the label shows in `marks.text` with its `id`, which `edit` and `delete`
   take. Missing today: a direct command for arrows, pointers and markers
   (a script answers the menu's `mouse` or `rubber` ask with data
   coordinates `xd`, `yd`, `xd2`, `yd2`), and an `id` on them.
6. **Zoom, pick, move on the plot.** The asks `mouse`, `rubber`, `drag` and
   `grab` take data coordinates instead of pixels, so a script can answer a
   zoom box, an initial-condition click or an AUTO point without the page.
   Missing today: one command with the arguments in it (`window.zoom` with a
   box, `window.scroll` with `dx`, `dy`, `auto.pick`); Scroll is a stream
   of drag events. `display` sets the shown ranges; the model's own bounds
   change only through the Window form ("Use this view" in the page answers
   that form).
7. **Export a picture.** PDF and SVG are written by the core from the same
   data, with no page: send `{"cmd":"key","menu":"main","item":"graphic"}`,
   answer the `menu` ask with `p` (PDF) or `v` (SVG), the typed "Picture
   parameters" form with `["true","12"]` (Colour, Axes font size) and the
   `file` ask with the name. Missing today: a single command that takes the
   file and the settings. GIF, the kinescope and the array plot's GIF still
   ask the client for the picture (`pixels`), so a client without a renderer
   cannot make them; the proposed registry needs the core to render them.
8. **Windows are numbers on the wire today** (`win`: 1 is the first plot
   window, 101 the AUTO diagram, 105 the array plot); names are a W353
   target.

See [protocol: plot data](../protocol.md#the-plot-as-data),
[questions](../protocol.md#question-answering), and
[pictures](../protocol.md#colours-and-line-styles-w478-w479-w492-541-542-556).

---

## 3. Unification of Multi-Route Operations

In the current codebase, several operations are duplicated across multiple disparate routes (e.g. legacy menu hotkeys, window layer shortcuts, protocol custom commands, and page action dispatchers). The dotted registry unifies each set of routes into a single command:

1. **Window bounds setting**:
   - Route A: Main menu `W` -> `W` opening `do_string_box_of` "Window" ([core/graf_par.cpp:481](../../core/graf_par.cpp#L481)).
   - Route B: Protocol `{"cmd":"display","x":[lo,hi],"y":[lo,hi]}` (display zoom; Use this view applies it via the Window form) ([core/json_windows.cpp:242](../../core/json_windows.cpp#L242)).
   - *Unified*: `window.set_bounds --x_min <val> --x_max <val> --y_min <val> --y_max <val> [--window <name>]`.
2. **Continue integration**:
   - Route A: Main menu `C` ([core/commands.cpp:268](../../core/commands.cpp#L268)) extending for one `total_time` duration.
   - Route B: Protocol `{"cmd":"continue","until":T}` or `{"cmd":"continue","extra":D}` ([core/ui_json.cpp:591](../../core/ui_json.cpp#L591)).
   - *Unified*: `run.continue [--until <T> | --extra <D>]`. Omitted arguments default to extending by `total_time`.
3. **Session saving**:
   - Route A: File menu `V` / `W` / `B` ([core/commands.cpp:529](../../core/commands.cpp#L529)) calling `xpp_session_save`.
   - Route B: AUTO File menu `S` (`menu_auto_file`, [core/menus.cpp:525](../../core/menus.cpp#L525)) calling session save.
   - Route C: Protocol `{"cmd":"session","op":"save","name":...}` ([core/ui_json.cpp:522](../../core/ui_json.cpp#L522)).
   - *Unified*: `session.save` and `session.save_as --file <path>`.
4. **Data export**:
   - Route A: Curves menu `exp(O)rt data` (`export_graf_data`, [core/graf_par.cpp:1232](../../core/graf_par.cpp#L1232)).
   - Route B: Browser `(W)rite` (`menu_browser_window`, [core/menus.cpp:436](../../core/menus.cpp#L436)).
   - Route C: Protocol `{"cmd":"browser","op":"write","name":...}` ([core/json_state.cpp:266](../../core/json_state.cpp#L266)).
   - *Unified*: `browser.export --file <name> --format <fmt> [--target <table|plot_curves>]`.
5. **Initial condition specification**:
   - Route A: Initialconds -> `(N)ew` (`M_IN`, [core/integrate.cpp:1115](../../core/integrate.cpp#L1115)).
   - Route B: Protocol `{"cmd":"set","kind":"ic","name":...,"value":...}` ([core/json_state.cpp:441](../../core/json_state.cpp#L441)).
   - Route C: Protocol `{"cmd":"set","values":[...]}` ([core/json_state.cpp:506](../../core/json_state.cpp#L506)).
   - *Unified*: `ic.set --values <map>`.

---

## 4. Capability Gaps Today

The JSON server can be driven by any language, including when an operation
asks a form or a data-coordinate mouse question. Direct typed commands are
still missing for many menu operations. The page holds display zoom; Use this
view applies it through the Window form. Kinescope and frame/GIF export ask a
client for pixels, so a page-free client must render them itself. Array-plot
scroll uses pixels. The proposals in section 2.24 identify the data-space
commands and core rendering needed to close those gaps.

---

## 5. Proposed Resource and Verb Taxonomy

### 5.1 Resource List (23 Resources)
1. `run`: Integrator trajectories, sweeps, continuation, and termination.
2. `ic`: Initial conditions inspection and assignment.
3. `equilibrium`: Fixed points, eigensystems, saddles, invariant manifolds.
4. `nullcline`: 2D nullcline computation, freezing, and animation.
5. `dirfield`: 2D direction and flow fields.
6. `window`: Plot window management, viewport bounds, zooming, and pan.
7. `plot`: Curves and frozen curves, axes, 3D views, colormaps, marks computed from data (`plot.add_data_markers`; equilibrium marks come from `equilibrium`), and hardcopy.
8. `annotation`: What the user places on a plot, each with an id: text labels, arrows, pointers and single markers (proposed; awaiting the maintainer).
9. `kinescope`: Screen capture, frame cycling, and movie export.
10. `bvp`: Boundary value problem solver and shooting.
11. `analysis`: Stochastics, statistics, Fourier/spectral analysis, Lyapunov exponents, Poincare sections, adjoints.
12. `table`: Function tables and discrete lookups.
13. `browser`: Data table viewer, column math, slicing, and table export.
14. `auto`: Bifurcation continuation, diagram views, special points, and diagnostics.
15. `animation`: Custom geometric/diagrammatic animations.
16. `array_plot`: Multi-variable spatiotemporal raster plots.
17. `set`: Named parameter and option sets (.setx).
18. `value`: Interactive parameter values, sliders, and checkpoint undo/redo.
19. `session`: Session files (.snapx), model files, reload, and process lifecycle.
20. `recording`: Step-by-step recording and replay (.recx).
21. `keymap`: Shortcut bindings and toolbar quick-access pinning.
22. `events`: Heavy data event subscriptions.
23. `tool`: Quick calculator and evaluation helpers.

### 5.2 Verb List (Standardized Verbs)
- **Lifecycle & Navigation**: `open`, `close`, `create`, `destroy`, `destroy_all`, `quit`, `reset`, `reload`.
- **Execution & Solving**: `initial`, `last`, `continue`, `steady`, `backward`, `shift`, `range`, `double_range`, `shoot`, `abort`, `compute`, `solve`, `solve_range`, `solve_periodic`, `solve_homoclinic`.
- **Query & Data Retrieval**: `get`, `find`, `search`, `page`, `get_state`, `get_source`.
- **Modification & Setting**: `set`, `set_bounds`, `set_axes`, `set_colormap`, `set_redraw_mode`, `configure`, `edit`, `move`, `rename`, `add`, `delete`, `clear`, `fit`, `zoom`, `zoom_out`, `scroll`.
- **File & Serialization**: `load`, `save`, `save_as`, `save_copy`, `export`, `import`, `export_pdf`, `export_svg`, `export_gif`, `export_csv`.
- **State Stacking**: `undo`, `redo`, `freeze`, `clear_frozen`, `add_note`.
- **Protocol Control**: `subscribe`, `unsubscribe`.

---

## 6. Open Naming Questions for the Maintainer

1. **`run` vs `integrate`**:
   - *Current proposal*: `run.initial`, `run.last`, `run.continue`, `run.range`.
   - *Alternative*: `integrate.initial`, `integrate.continue`, etc.
   - *Recommendation*: Keep `run`. It is shorter, matches common speech, avoids awkwardness when running to steady state (`run.steady`) or non-ODE solvers, and aligns with modern UI toolbar conventions ("Run", "Continue").
2. **`annotation` vs `marker` vs `text`**:
   - Today XPPAUT groups labels, lines, arrows, pointers, and geometric shapes under `Text,etc` (`menu_text` in [core/menus.cpp:387](../../core/menus.cpp#L387)).
   - *Recommendation* (the coordinator's, awaiting the maintainer): Resource `annotation` for what the user places (text labels, arrows, pointers, single markers), each with an id; marks computed from the data (markers every N rows, equilibrium marks, frozen curves) belong to plotting, `plot.*`.
3. **`value` vs `parameter`**:
   - Should parameter changes belong to `parameter.set` and state tweaks to `ic.set`, or should interactive value tweaking across parameters/ICs remain unified under `value.set`?
   - *Recommendation*: Use `value.set_parameters` and `ic.set` for batch assignments, but retain `value.slide` and `value.undo` for the shared numeric editing stack.
4. **AUTO 2-parameter continuation naming**:
   - In AUTO, 2-parameter continuation can be started from a Hopf point, a Limit point (fold), a Period Doubling point, or a Torus point.
   - *Current proposal*: `auto.run --branch_type two_parameter`.
   - *Question*: Should the start point type be an explicit argument (`--start_point hopf`) or automatically inferred from the grabbed point?
   - *Recommendation*: Automatically infer from the grabbed point if omitted, but allow explicit override `--start_point`.
5. **Picture export naming**: Should PDF and SVG use one `plot.export`
   command with a named format or separate verbs? The common picture writer
   supports both; the names remain for the maintainer to settle.
