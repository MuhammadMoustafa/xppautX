# W308: Inventory of Every Operation for the Dotted Command Convention

Stage 0 study for card [W352 / #413](https://github.com/MuhammadMoustafa/xppautX/issues/413) (2026-10-14),
implementing the convention decided in [issue #367](https://github.com/MuhammadMoustafa/xppautX/issues/367),
[Discussion #415](https://github.com/MuhammadMoustafa/xppautX/discussions/415), and [docs/command-design.md](../command-design.md).

## 1. Context and Convention Rules

On 2026-10-14, the maintainer approved the unified command design for xppautX:
1. **One command shape**: Every operation is a registry command named `resource.verb` in snake_case (e.g. `run.initial`, `auto.grab`, `window.create`).
2. **Unified protocol**: JSON Lines everywhere (`{"cmd":"auto.grab","label":12}`). For `--server` text input, a tokenizer parses `--flag value` lines into the same registry validator (`auto.grab --label 12`). Positional arguments are rejected.
3. **Data types**: Strict typed arguments (`integer`, `real`, `boolean`, `choice`, `text`, `expression`, `name`, `file`, `path`, `color`, `window`, `list`, `map`). No `null` and no deep nesting.
4. **Colors in hex**: One `color` type: `#rrggbb` or `foreground` (theme-aware). Numeric color indices (0–10) are eliminated.
5. **Windows by name**: Windows are named strings (`window1`, `phase_plane`). Window numbers on the wire are eliminated.
6. **Results and events**: Every command completes with a single `done` event containing a typed `result` data payload (replacing text-only `idle`). Events follow `resource.event` naming (`window.created`, `plot.series`, `auto.diagram`, `session.state`). Heavy data streams are managed via `events.subscribe`.
7. **Forms and dialogs as arguments**: Interactive dialogs (`do_string_box_of`, `new_int`, etc.) and mouse operations (`get_mouse_xy`, `rubber_band`) become command arguments in data space rather than pixel queries or interactive blocking ask prompts. The interactive ask mechanism is retained only for core questions (file selection, overwrite confirmation, compute values).
8. **Navigation removed from command table**: Legacy menu layer switching (`file`, `numerics`, `exit` in [core/command_table.h](../../core/command_table.h#L156-L158)) belongs strictly to front-end keymaps and is omitted from the command registry.

---

## 2. Comprehensive Inventory of Operations

### 2.1 Resource `run` (Integration, Trajectories, and Sweeps)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `I` -> `G` | Initialconds -> (G)o | `{"cmd":"key","menu":"main","item":"run_initial"}` / `{"cmd":"action","action":"integrate"}` | none | `run.initial` | none | default | `{"status":"completed","steps":integer,"time":real}` | `job.progress`, `plot.series`, `session.state`, `done` |
| `I` -> `L` | Initialconds -> (L)ast | `{"cmd":"key","menu":"main","item":"run_last"}` | none | `run.last` | none | default | `{"status":"completed","steps":integer,"time":real}` | `job.progress`, `plot.series`, `session.state`, `done` |
| `C` | Continue integration | `{"cmd":"continue"}` (`extra`, `until`) | none | `run.continue` | `until`: optional real, `extra`: optional real | mutex(`until`, `extra`) | `{"status":"completed","end_time":real}` | `job.progress`, `plot.series`, `session.state`, `done` |
| Alt+S | (Page toolbar / search) | `{"cmd":"steady","decimals":N,"hold":H,"maximum":M}` | none | `run.steady` | `decimals`: integer (0..15), `hold`: real (>0), `maximum`: real (>0) | default | `{"status":"converged"\|"maximum_reached","decimals":integer,"time":real}` | `job.progress`, `plot.series`, `session.state`, `done` |
| `I` -> `B` | Initialconds -> (B)ackward | `{"cmd":"key","win":"main","key":"b"}` | none | `run.backward` | none | default | `{"status":"completed","steps":integer,"time":real}` | `job.progress`, `plot.series`, `session.state`, `done` |
| `I` -> `S` | Initialconds -> (S)hift | `{"cmd":"key","win":"main","key":"s"}` | none | `run.shift` | none | default | `{"status":"completed","steps":integer,"time":real}` | `job.progress`, `plot.series`, `session.state`, `done` |
| `I` -> `R` | Initialconds -> (R)ange | `{"cmd":"key","win":"main","key":"r"}` | `do_string_box_of` "Range Integrate" ([core/integrate.cpp:204](../../core/integrate.cpp#L204)): Range over, Steps, Start, End, Reset storage, Use old ic's, Cycle color, Movie | `run.range` | `variable`: name, `start`: real, `end`: real, `steps`: integer (>0), `reset_storage`: optional boolean, `use_old_ic`: optional boolean, `cycle_color`: optional boolean, `movie`: optional boolean | default | `{"runs_completed":integer,"total_steps":integer}` | `job.progress`, `plot.series`, `session.state`, `done` |
| `I` -> `2` | Initialconds -> (2)par range | `{"cmd":"key","win":"main","key":"2"}` | `do_string_box_of` "Double Range Integrate" ([core/integrate.cpp:250](../../core/integrate.cpp#L250)): Vary1, Start1, End1, Vary2, Start2, End2, Steps, Reset, OldIC, Cycle, Movie, Crv/Array, Steps2 | `run.double_range` | `variable1`: name, `start1`: real, `end1`: real, `variable2`: name, `start2`: real, `end2`: real, `steps1`: integer (>0), `steps2`: integer (>0), `mode`: choice(`curve`,`array`), `reset_storage`: optional boolean, `use_old_ic`: optional boolean, `cycle_color`: optional boolean, `movie`: optional boolean | default | `{"grid_points":integer,"total_steps":integer}` | `job.progress`, `plot.series`, `session.state`, `done` |
| `I` -> `D` | Initialconds -> DAE guess | `{"cmd":"key","win":"main","key":"d"}` | `new_string_of` per algebraic variable ([core/dae_fun.cpp:298](../../core/dae_fun.cpp#L298)) | `run.dae_guess` | `guesses`: optional map[name, expression] | default | `{"status":"solved"\|"failed","residuals":map[name, real]}` | `session.state`, `done` |
| Esc (during run) | Stop | `{"cmd":"abort"}` | none | `run.abort` | none | default | `{"stopped_at":real}` | `job.stopped`, `done` |
| `I` -> `H` | Initialconds -> s(H)oot | `{"cmd":"key","win":"main","key":"h"}` | `new_int` "Which? (1-N)" ([core/integrate.cpp:1035](../../core/integrate.cpp#L1035)) | `run.shoot` | `manifold_index`: integer (>=1) | default | `{"status":"completed","steps":integer}` | `job.progress`, `plot.series`, `done` |

---

### 2.2 Resource `ic` (Initial Conditions)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `I` -> `N` | Initialconds -> (N)ew | `{"cmd":"set","kind":"init",...}` / `{"cmd":"values","values":{...}}` | string box per variable | `ic.set` | `values`: map[name, real] | default | `{"applied":map[name, real]}` | `session.state`, `done` |
| `I` -> `M` | Initialconds -> (M)ouse | `{"cmd":"click"}` / `mouse_ask` "mouse" ([core/json_prompts.cpp:471](../../core/json_prompts.cpp#L471)) | `mouse_ask` coordinates click | `ic.at` | `coordinates`: map[name, real], `window`: optional window | default | `{"applied":map[name, real]}` | `plot.series`, `session.state`, `done` |
| `I` -> `I` | Initialconds -> m(I)ce | `{"cmd":"key","win":"main","key":"i"}` | repeated `mouse_ask` until click outside | `ic.mouse_stream` | `points`: list[map[name, real]], `window`: optional window | default | `{"trajectories_run":integer}` | `plot.series`, `session.state`, `done` |
| `I` -> `F` | Initialconds -> (F)ile | `{"cmd":"key","win":"main","key":"f"}` | `file_selector` "*.dat" | `ic.load` | `file`: file | default | `{"file":file,"applied":map[name, real]}` | `session.state`, `done` |
| `I` -> `U` | Initialconds -> form(U)la | `{"cmd":"key","win":"main","key":"u"}` | `new_string_of` "Formula:" ([core/integrate.cpp:1170](../../core/integrate.cpp#L1170)) | `ic.formula` | `variable`: name, `formula`: expression | default | `{"variable":name,"formula":expression}` | `session.state`, `done` |
| (Data panel) | (Set row as IC) | `{"cmd":"browser","op":"get"}` | none | `ic.from_data_row` | `row`: integer (>=0) | default | `{"row":integer,"values":map[name, real]}` | `session.state`, `done` |
| (Model load) | (Array IC start) | (Automatic on model init) | `new_int` "# elements", `new_string_of` formula ([core/integrate.cpp:1295](../../core/integrate.cpp#L1295)) | `ic.array` | `name`: name, `size`: integer (>0), `formula`: expression | default | `{"name":name,"size":integer}` | `session.state`, `done` |

---

### 2.3 Resource `equilibrium` (Fixed Points and Stability)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `S` -> `G` | Equilibria -> (G)o | `{"cmd":"key","win":"main","key":"g"}` | `TwoChoice` "Print eigenvalues? (Y/N)", "Draw Invariant Sets? (Y/N)" ([core/gear.cpp:177](../../core/gear.cpp#L177)) | `equilibrium.find` | `print_eigenvalues`: optional boolean, `draw_invariant_sets`: optional boolean, `draw_strong_sets`: optional boolean | default | `{"converged":boolean,"equilibrium":map[name, real],"eigenvalues":list[map[string, real]],"stability":choice("stable","unstable","saddle")}` | `plot.series`, `session.state`, `done` |
| `S` -> `M` | Equilibria -> (M)ouse | `{"cmd":"click"}` / `mouse_ask` | `mouse_ask` click guess | `equilibrium.guess` | `coordinates`: map[name, real], `window`: optional window | default | `{"converged":boolean,"equilibrium":map[name, real],"eigenvalues":list[map[string, real]]}` | `plot.series`, `session.state`, `done` |
| `S` -> `R` | Equilibria -> (R)ange | `{"cmd":"key","win":"main","key":"r"}` | `do_string_box_of` "Range Equilibria" ([core/integrate.cpp:143](../../core/integrate.cpp#L143)): Parameter, Steps, Low, High, Shoot, Color, Movie, Monte Carlo | `equilibrium.range` | `parameter`: name, `start`: real, `end`: real, `steps`: integer (>0), `shoot`: optional boolean, `color`: optional boolean, `movie`: optional boolean, `monte_carlo`: optional boolean | default | `{"points_found":integer}` | `plot.series`, `done` |
| `S` -> `C` | Equilibria -> monte(C)ar | `{"cmd":"key","win":"main","key":"c"}` | `new_int` "Shoot", `new_int` "# Guesses", `new_float` "Tolerance", bounds per var ([core/integrate.cpp:330](../../core/integrate.cpp#L330)) | `equilibrium.search` | `guesses`: integer (>0), `tolerance`: real (>0), `shoot`: optional boolean, `bounds`: map[name, list[real, 2]] | default | `{"found":integer,"equilibria":list[map[name, real]]}` | `plot.series`, `session.state`, `done` |
| `E` -> `I` | (Equilibrium window) -> (I)mport | `{"cmd":"key","win":"equilibrium","key":"i"}` | none | `equilibrium.import_as_ic` | none | default | `{"applied":map[name, real]}` | `session.state`, `done` |
| (Silent/CLI) | (Write equilibrium to file) | `{"cmd":"equilibrium","op":"write","file":F,"shoot":N}` | none | `equilibrium.export` | `file`: file, `shoot`: optional boolean | default | `{"file":file,"written":boolean}` | `done` |
| `U` -> `I` | Numerics -> sIng pt | `{"cmd":"set","kind":"num","name":"singpt_..."}` | string prompts for iterates, tolerance, epsilon | `equilibrium.configure` | `max_iterates`: optional integer, `newton_tolerance`: optional real, `jacobian_epsilon`: optional real | default | `{"settings":map[string, real]}` | `session.state`, `done` |

---

### 2.4 Resource `nullcline` (Nullclines and Invariant Curves)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `N` -> `N` | Nullclines -> (N)ew | `{"cmd":"key","win":"main","key":"n"}` | none | `nullcline.compute` | none | default | `{"curves_computed":integer}` | `plot.series`, `done` |
| `N` -> `R` | Nullclines -> (R)estore | `{"cmd":"key","win":"main","key":"r"}` | none | `nullcline.redraw` | none | default | `{"drawn":boolean}` | `plot.series`, `done` |
| `N` -> `A` | Nullclines -> (A)uto | `{"cmd":"key","win":"main","key":"a"}` | none | `nullcline.auto_mode` | `enabled`: boolean | default | `{"auto_redraw":boolean}` | `session.state`, `done` |
| `N` -> `M` | Nullclines -> (M)anual | `{"cmd":"key","win":"main","key":"m"}` | none | (Merged into `nullcline.auto_mode`) | `enabled`: false | default | `{"auto_redraw":boolean}` | `session.state`, `done` |
| `N` -> `F` -> `F` | Freeze cline -> (F)reeze | `{"cmd":"key","win":"main","key":"f"}` | none | `nullcline.freeze` | none | default | `{"frozen_curves_count":integer}` | `plot.series`, `done` |
| `N` -> `F` -> `D` | Freeze cline -> (D)elete all | `{"cmd":"key","win":"main","key":"d"}` | none | `nullcline.clear_frozen` | none | default | `{"cleared":boolean}` | `plot.series`, `done` |
| `N` -> `F` -> `R` | Freeze cline -> (R)ange | `{"cmd":"key","win":"main","key":"r"}` | `do_string_box_of` "Range Clines" ([core/nullcline.cpp:308](../../core/nullcline.cpp#L308)): Par, Start, End, Steps | `nullcline.range_freeze` | `parameter`: name, `start`: real, `end`: real, `steps`: integer (>0) | default | `{"steps_frozen":integer}` | `plot.series`, `done` |
| `N` -> `F` -> `A` | Freeze cline -> (A)nimate | `{"cmd":"key","win":"main","key":"a"}` | `new_int` "Delay (msec)" ([core/nullcline.cpp:455](../../core/nullcline.cpp#L455)) | `nullcline.animate` | `delay_ms`: optional integer (>=0) | default | `{"animated":boolean}` | `done` |
| `N` -> `S` | Nullclines -> (S)ave | `{"cmd":"key","win":"main","key":"s"}` | `file_selector` | `nullcline.export` | `file`: file, `format`: optional choice("csv","parquet","tsv") | default | `{"file":file,"points_written":integer}` | `done` |
| `U` -> `N` | Numerics -> Nullcline mesh | `{"cmd":"set","kind":"num","name":"nullcline_mesh"}` | string prompt mesh size | `nullcline.set_mesh` | `mesh`: integer (>0) | default | `{"nullcline_mesh":integer}` | `session.state`, `done` |

---

### 2.5 Resource `dirfield` (Direction Fields and Flows)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `D` -> `D` | Two-D Fun -> (D)irect Field | `{"cmd":"key","win":"main","key":"d"}` | `new_int` "Grid:" ([core/nullcline.cpp:614](../../core/nullcline.cpp#L614)) | `dirfield.compute` | `grid`: integer (>0), `scaled`: optional boolean | default | `{"grid":integer,"vectors_drawn":integer}` | `plot.series`, `done` |
| `D` -> `F` | Two-D Fun -> (F)low | `{"cmd":"key","win":"main","key":"f"}` | `new_int` "Grid:" | `dirfield.flow` | `grid`: integer (>0) | default | `{"grid":integer,"trajectories_run":integer}` | `plot.series`, `done` |
| `D` -> `N` | Two-D Fun -> (N)o dir. fld. | `{"cmd":"key","win":"main","key":"n"}` | none | `dirfield.clear` | none | default | `{"cleared":boolean}` | `plot.cleared`, `done` |
| `D` -> `C` | Two-D Fun -> (C)olorize | `{"cmd":"key","win":"main","key":"c"}` | `new_int` "Grid:" | `dirfield.colorize` | `grid`: integer (>0) | default | `{"grid":integer,"cells_colored":integer}` | `plot.series`, `done` |
| `D` -> `S` | Two-D Fun -> (S)caled Dir.Fld | `{"cmd":"key","win":"main","key":"s"}` | `new_int` "Grid:" | `dirfield.compute` | `grid`: integer (>0), `scaled`: true | default | `{"grid":integer,"vectors_drawn":integer}` | `plot.series`, `done` |
| (CLI/Silent) | (Write dfield data) | `{"cmd":"dfield","op":"write","file":F}` | none | `dirfield.export` | `file`: file | default | `{"file":file,"written":boolean}` | `done` |

---

### 2.6 Resource `window` (Plot Windows, Zooming, and Views)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `M` -> `C` | Make window -> (C)reate | `{"cmd":"key","win":"main","key":"c"}` | none | `window.create` | `name`: optional text | default | `{"window":window}` | `window.created`, `session.state`, `done` |
| `M` -> `K` | Make window -> (K)ill all | `{"cmd":"key","win":"main","key":"k"}` | none | `window.destroy_all` | none | default | `{"closed_count":integer}` | `window.destroyed`, `session.state`, `done` |
| `M` -> `D` | Make window -> (D)estroy | `{"cmd":"key","win":"main","key":"d"}` | none | `window.destroy` | `window`: optional window | default | `{"window":window,"closed":boolean}` | `window.destroyed`, `session.state`, `done` |
| `M` -> `B` | Make window -> (B)ottom | `{"cmd":"key","win":"main","key":"b"}` | none | `window.send_to_bottom` | `window`: optional window | default | `{"window":window}` | `session.state`, `done` |
| `M` -> `A` | Make window -> (A)uto | `{"cmd":"key","win":"main","key":"a"}` | none | `window.set_redraw_mode` | `mode`: choice("auto","manual"), `window`: optional window | default | `{"redraw_mode":choice("auto","manual")}` | `session.state`, `done` |
| `M` -> `M` | Make window -> (M)anual | `{"cmd":"key","win":"main","key":"m"}` | none | (Merged into `window.set_redraw_mode`) | `mode`: "manual", `window`: optional window | default | `{"redraw_mode":"manual"}` | `session.state`, `done` |
| `M` -> `S` | Make window -> (S)imPlot On/Off | `{"cmd":"key","win":"main","key":"s"}` | none | `window.set_simultaneous_plot` | `enabled`: boolean | default | `{"simultaneous_plot":boolean}` | `session.state`, `done` |
| `W` -> `W` | Window -> (W)indow | `{"cmd":"display"}` | `do_string_box_of` "Window" ([core/graf_par.cpp:468](../../core/graf_par.cpp#L468)): Xlo, Xhi, Ylo, Yhi | `window.set_bounds` | `x_min`: real, `x_max`: real, `y_min`: real, `y_max`: real, `window`: optional window | default | `{"x_min":real,"x_max":real,"y_min":real,"y_max":real}` | `plot.shown`, `session.state`, `done` |
| `W` -> `Z` | Window -> (Z)oom In | `{"cmd":"click"}` / `mouse_ask` "rubber" | rubber band drag box | `window.zoom` | `x_min`: real, `x_max`: real, `y_min`: real, `y_max`: real, `window`: optional window | default | `{"zoom_applied":boolean}` | `plot.shown`, `session.state`, `done` |
| `W` -> `O` | Window -> Zoom (O)ut | `{"cmd":"key","win":"main","key":"o"}` | none | `window.zoom_out` | `factor`: optional real, `window`: optional window | default | `{"new_bounds":map[string, real]}` | `plot.shown`, `session.state`, `done` |
| `W` -> `F` | Window -> (F)it | `{"cmd":"key","win":"main","key":"f"}` | none | `window.fit` | `window`: optional window | default | `{"x_min":real,"x_max":real,"y_min":real,"y_max":real}` | `plot.shown`, `session.state`, `done` |
| `W` -> `D` | Window -> (D)efault | `{"cmd":"key","win":"main","key":"d"}` | none | `window.reset_default` | `window`: optional window | default | `{"x_min":real,"x_max":real,"y_min":real,"y_max":real}` | `plot.shown`, `session.state`, `done` |
| `W` -> `S` | Window -> (S)croll | `{"cmd":"key","win":"main","key":"s"}` | arrow keys interaction | `window.scroll` | `dx`: real, `dy`: real, `window`: optional window | default | `{"new_center":list[real, 2]}` | `plot.shown`, `done` |
| `R` | Redraw plot | `{"cmd":"redraw"}` | none | `window.redraw` | `window`: optional window | default | `{"redrawn":boolean}` | `plot.shown`, `done` |
| `E` | Clear plot | `{"cmd":"key","menu":"main","item":"erase"}` | none | `window.clear` | `window`: optional window | default | `{"cleared":boolean}` | `plot.cleared`, `done` |
| (GUI double click) | (Rename window) | none | none | `window.rename` | `window`: window, `name`: text | default | `{"window":window,"new_name":text}` | `window.renamed`, `session.state`, `done` |

---

### 2.7 Resource `plot` (Curves, Axes, Colormaps, and Rendering)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `G` -> `A` | Curves -> (A)dd curve | `{"cmd":"key","win":"main","key":"a"}` | `do_string_box_of` "Add curve" ([core/graf_par.cpp:732](../../core/graf_par.cpp#L732)): x, y, z, color, line type | `plot.add_curve` | `x`: name, `y`: name, `z`: optional name, `color`: optional color, `line_type`: optional choice("solid","dashed","dotted","points"), `window`: optional window | default | `{"curve_index":integer}` | `plot.series`, `session.state`, `done` |
| `G` -> `D` | Curves -> (D)elete last | `{"cmd":"key","win":"main","key":"d"}` | none | `plot.delete_curve` | `curve_index`: optional integer, `window`: optional window | default | `{"remaining_curves":integer}` | `plot.shown`, `session.state`, `done` |
| `G` -> `R` | Curves -> (R)emove all | `{"cmd":"key","win":"main","key":"r"}` | none | `plot.delete_all_curves` | `window`: optional window | default | `{"cleared":boolean}` | `plot.shown`, `session.state`, `done` |
| `G` -> `E` | Curves -> (E)dit curve | `{"cmd":"key","win":"main","key":"e"}` | `new_int` "Edit 0-N:", then `do_string_box_of` | `plot.edit_curve` | `curve_index`: integer, `x`: optional name, `y`: optional name, `z`: optional name, `color`: optional color, `line_type`: optional choice("solid","dashed","dotted","points"), `window`: optional window | default | `{"curve_index":integer,"updated":boolean}` | `plot.shown`, `session.state`, `done` |
| `G` -> `F` -> `F` | Freeze -> (F)reeze | `{"cmd":"key","win":"main","key":"f"}` | `new_string` "Key", `new_string` "Name" | `plot.freeze_curve` | `name`: optional text, `key`: optional text, `window`: optional window | default | `{"slot":integer,"name":text}` | `plot.series`, `session.state`, `done` |
| `G` -> `F` -> `D` | Freeze -> (D)elete | `{"cmd":"key","win":"main","key":"d"}` | `new_int` "Slot:" | `plot.delete_frozen` | `slot`: integer, `window`: optional window | default | `{"slot":integer,"deleted":boolean}` | `plot.shown`, `session.state`, `done` |
| `G` -> `F` -> `E` | Freeze -> (E)dit | `{"cmd":"key","win":"main","key":"e"}` | `do_string_box_of` "Edit Freeze" ([core/graf_par.cpp:982](../../core/graf_par.cpp#L982)): Key, Name, Color | `plot.edit_frozen` | `slot`: integer, `name`: optional text, `key`: optional text, `color`: optional color, `window`: optional window | default | `{"slot":integer,"updated":boolean}` | `plot.shown`, `session.state`, `done` |
| `G` -> `F` -> `R` | Freeze -> (R)emove all | `{"cmd":"key","win":"main","key":"r"}` | none | `plot.delete_all_frozen` | `window`: optional window | default | `{"cleared":boolean}` | `plot.shown`, `session.state`, `done` |
| `G` -> `F` -> `K` | Freeze -> (K)ey | `{"cmd":"key","win":"main","key":"k"}` | `menu_freeze_key` (No key / Key) | `plot.set_frozen_legend` | `visible`: boolean, `window`: optional window | default | `{"legend_visible":boolean}` | `plot.shown`, `session.state`, `done` |
| `G` -> `F` -> `B` | Freeze -> (B)if.Diag | `{"cmd":"key","win":"main","key":"b"}` | `file_selector` | `plot.import_bifurcation` | `file`: file, `window`: optional window | default | `{"file":file,"curves_imported":integer}` | `plot.series`, `done` |
| `G` -> `F` -> `C` | Freeze -> (C)lr. BD | `{"cmd":"key","win":"main","key":"c"}` | none | `plot.clear_bifurcation` | `window`: optional window | default | `{"cleared":boolean}` | `plot.shown`, `done` |
| `G` -> `F` -> `O` | Freeze -> (O)n/(O)ff freeze | `{"cmd":"key","win":"main","key":"o"}` | none | `plot.set_auto_freeze` | `enabled`: boolean | default | `{"auto_freeze":boolean}` | `session.state`, `done` |
| `G` -> `P` | Curves -> (P)ostscript | `{"cmd":"key","win":"main","key":"p"}` | `do_string_box_of` "Postscript parameters" ([core/my_ps.cpp:385](../../core/my_ps.cpp#L385)), `file_selector` | `plot.export_postscript` | `file`: file, `color`: optional boolean, `font`: optional text, `font_size`: optional integer, `line_width`: optional real, `window`: optional window | default | `{"file":file,"bytes_written":integer}` | `done` |
| `G` -> `V` | Curves -> S(V)G | `{"cmd":"key","win":"main","key":"v"}` | `file_selector` | `plot.export_svg` | `file`: file, `window`: optional window | default | `{"file":file,"bytes_written":integer}` | `done` |
| `G` -> `X` | Curves -> a(X)es opts | `{"cmd":"key","win":"main","key":"x"}` | `do_string_box_of` "Axes options" ([core/graf_par.cpp:268](../../core/graf_par.cpp#L268)): xlabel, ylabel, zlabel, etc. | `plot.set_axes_labels` | `x_label`: optional text, `y_label`: optional text, `z_label`: optional text, `window`: optional window | default | `{"labels_updated":boolean}` | `plot.shown`, `session.state`, `done` |
| `G` -> `O` | Curves -> exp(O)rt data | `{"cmd":"key","win":"main","key":"o"}` | `menu_save_what` ("The data table", "What the plot shows"), `file_selector` | `plot.export_data` | `file`: file, `format`: choice("csv","parquet","tsv"), `window`: optional window | default | `{"file":file,"rows_written":integer}` | `done` |
| `G` -> `C` -> `*` | Curves -> (C)olormap | `{"cmd":"key","win":"main","key":"*"}` | `menu_colormap` (7 choices) | `plot.set_colormap` | `colormap`: choice("jet","periodic","hot","cool","blue_red","gray","cubehelix") | default | `{"colormap":choice("jet","periodic","hot","cool","blue_red","gray","cubehelix")}` | `plot.shown`, `session.state`, `done` |
| `V` -> `2` | Axes -> 2D | `{"cmd":"display","axes":0}` | none | `plot.set_2d` | `x`: name, `y`: name, `window`: optional window | default | `{"axes":"2d","x":name,"y":name}` | `plot.shown`, `session.state`, `done` |
| `V` -> `3` | Axes -> 3D | `{"cmd":"display","axes":5}` | none | `plot.set_3d` | `x`: name, `y`: name, `z`: name, `window`: optional window | default | `{"axes":"3d","x":name,"y":name,"z":name}` | `plot.shown`, `session.state`, `done` |
| `3` | 3D view | `{"cmd":"key","menu":"main","item":"3dparams"}` | `do_string_box_of` "3D Parameters" ([core/graf_par.cpp:544](../../core/graf_par.cpp#L544)): Xmin, Xmax, Ymin, Ymax, Zmin, Zmax, Theta, Phi | `plot.set_3d_box` | `x_min`: real, `x_max`: real, `y_min`: real, `y_max`: real, `z_min`: real, `z_max`: real, `theta`: real, `phi`: real, `window`: optional window | default | `{"box_updated":boolean}` | `plot.shown`, `session.state`, `done` |
| `X` | Variable vs time | `{"cmd":"key","menu":"main","item":"xivst"}` | `new_string_of` "Plot vs t:" ([core/graf_par.cpp:491](../../core/graf_par.cpp#L491)) | `plot.variable_vs_time` | `variable`: name, `window`: optional window | default | `{"x":"t","y":name}` | `plot.shown`, `session.state`, `done` |

---

### 2.8 Resource `annotation` (Text, Pointers, Arrows, and Markers)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `T` -> `T` | Text,etc -> (T)ext | `{"cmd":"text"}` / `mouse_ask` | `new_string` "Text:", `new_int` "Size", `mouse_ask` | `annotation.add_text` | `text`: text, `x`: real, `y`: real, `font_size`: optional integer (0..4), `font`: optional text, `color`: optional color, `window`: optional window | default | `{"id":integer}` | `plot.shown`, `session.state`, `done` |
| `T` -> `A` | Text,etc -> (A)rrow | `{"cmd":"text"}` | `mouse_ask` trajectory click | `annotation.add_flow_arrow` | `x`: real, `y`: real, `window`: optional window | default | `{"id":integer}` | `plot.shown`, `session.state`, `done` |
| `T` -> `P` | Text,etc -> (P)ointer | `{"cmd":"text"}` | `mouse_ask` drag start and end | `annotation.add_pointer` | `start_x`: real, `start_y`: real, `end_x`: real, `end_y`: real, `color`: optional color, `size`: optional real, `window`: optional window | default | `{"id":integer}` | `plot.shown`, `session.state`, `done` |
| `T` -> `M` | Text,etc -> (M)arker | `{"cmd":"text"}` | `do_string_box_of` "Add Marker" ([core/grobs.cpp:269](../../core/grobs.cpp#L269)), `mouse_ask` | `annotation.add_marker` | `marker_type`: choice("square","circle","triangle","cross","diamond"), `x`: real, `y`: real, `size`: optional real, `color`: optional color, `window`: optional window | default | `{"id":integer}` | `plot.shown`, `session.state`, `done` |
| `T` -> `S` | Text,etc -> marker(S) | `{"cmd":"text"}` | `do_string_box_of` "Add Markers" ([core/grobs.cpp:291](../../core/grobs.cpp#L291)) | `annotation.add_data_markers` | `x_variable`: name, `y_variable`: name, `marker_type`: choice("square","circle","triangle","cross","diamond"), `stride`: optional integer, `color`: optional color, `window`: optional window | default | `{"count":integer}` | `plot.shown`, `session.state`, `done` |
| `T` -> `E` -> `M` | Edit -> (M)ove | `{"cmd":"click"}` | `mouse_ask` pick and drop | `annotation.move` | `id`: integer, `new_x`: real, `new_y`: real, `window`: optional window | default | `{"id":integer,"moved":boolean}` | `plot.shown`, `session.state`, `done` |
| `T` -> `E` -> `C` | Edit -> (C)hange | `{"cmd":"text"}` | `do_string_box_of` / string prompts | `annotation.edit` | `id`: integer, `text`: optional text, `size`: optional real, `color`: optional color, `window`: optional window | default | `{"id":integer,"updated":boolean}` | `plot.shown`, `session.state`, `done` |
| `T` -> `E` -> `D` | Edit -> (D)elete | `{"cmd":"text"}` | `TwoChoice` "Delete ?" | `annotation.delete` | `id`: integer, `window`: optional window | default | `{"id":integer,"deleted":boolean}` | `plot.shown`, `session.state`, `done` |
| `T` -> `D` | Text,etc -> (D)elete all | `{"cmd":"key","win":"main","key":"d"}` | none | `annotation.delete_all` | `window`: optional window | default | `{"cleared":boolean}` | `plot.shown`, `session.state`, `done` |

---

### 2.9 Resource `kinescope` (Frame Capture and Animation Playback)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `K` -> `C` | Kinescope -> (C)apture | `{"cmd":"key","win":"main","key":"c"}` | none | `kinescope.capture` | none | default | `{"frame_count":integer}` | `session.state`, `done` |
| `K` -> `R` | Kinescope -> (R)eset | `{"cmd":"key","win":"main","key":"r"}` | none | `kinescope.reset` | none | default | `{"cleared":boolean}` | `session.state`, `done` |
| `K` -> `P` | Kinescope -> (P)layback | `{"cmd":"key","win":"main","key":"p"}` | none | `kinescope.play` | none | default | `{"frames_played":integer}` | `done` |
| `K` -> `A` | Kinescope -> (A)utoplay | `{"cmd":"key","win":"main","key":"a"}` | `new_int` "Number of cycles", `new_int` "Msec between frames" ([core/commands.cpp:88](../../core/commands.cpp#L88)) | `kinescope.autoplay` | `cycles`: integer (>0), `frame_ms`: integer (>=0) | default | `{"cycles_completed":integer}` | `done` |
| `K` -> `S` | Kinescope -> (S)ave | `{"cmd":"key","win":"main","key":"s"}` | `file_selector` "*.gif" | `kinescope.export_frames` | `file`: file | default | `{"file":file,"frames_saved":integer}` | `done` |
| `K` -> `M` | Kinescope -> (M)ake AniGif | `{"cmd":"key","win":"main","key":"m"}` | `file_selector` "*.gif" | `kinescope.export_gif` | `file`: file | default | `{"file":file,"bytes_written":integer}` | `done` |

---

### 2.10 Resource `bvp` (Boundary Value Problems)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `B` -> `R` | Bndry Value Prob -> (R)ange | `{"cmd":"key","win":"main","key":"r"}` | `do_string_box_of` "Range Shoot" ([core/pp_shoot.cpp:322](../../core/pp_shoot.cpp#L322)): Parameter, Start, End, Steps, etc. | `bvp.solve_range` | `parameter`: name, `start`: real, `end`: real, `steps`: integer (>0) | default | `{"solutions_found":integer}` | `plot.series`, `done` |
| `B` -> `N` | Bndry Value Prob -> (N)o show | `{"cmd":"key","win":"main","key":"n"}` | none | `bvp.set_show_mode` | `mode`: "final" | default | `{"show_mode":"final"}` | `done` |
| `B` -> `S` | Bndry Value Prob -> (S)how | `{"cmd":"key","win":"main","key":"s"}` | none | `bvp.set_show_mode` | `mode`: "steps" | default | `{"show_mode":"steps"}` | `done` |
| `B` -> `P` | Bndry Value Prob -> (P)eriodic | `{"cmd":"key","win":"main","key":"p"}` | `do_string_box_of` "Periodic BCs" ([core/pp_shoot.cpp:188](../../core/pp_shoot.cpp#L188)) | `bvp.solve_periodic` | `variable`: name, `section`: real, `period_guess`: real | default | `{"converged":boolean,"period":real}` | `plot.series`, `session.state`, `done` |
| `B` -> `H` | Bndry Value Prob -> Homoclinic | `{"cmd":"key","win":"main","key":"h"}` | none | `bvp.solve_homoclinic` | none | default | `{"converged":boolean}` | `plot.series`, `session.state`, `done` |
| `U` -> `V` | Numerics -> bndVal | `{"cmd":"set","kind":"num","name":"bvp_..."}` | string prompts for iterates, tolerance, epsilon | `bvp.configure` | `max_iterates`: optional integer, `tolerance`: optional real, `epsilon`: optional real | default | `{"settings":map[string, real]}` | `session.state`, `done` |

---

### 2.11 Resource `analysis` (Stochastic, Fits, Spectra, Adjoints, and Poincare Maps)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `U` -> `H` -> `N` | Stochastic -> New seed | `{"cmd":"set","kind":"options","name":"seed"}` | `new_int` "Seed:" ([core/markov.cpp:357](../../core/markov.cpp#L357)) | `analysis.seed_random` | `seed`: integer (>=0) | default | `{"seed":integer}` | `session.state`, `done` |
| `U` -> `H` -> `C` | Stochastic -> Compute | `{"cmd":"key","win":"main","key":"c"}` | none | `analysis.stochastic_integrate` | `trials`: integer (>0) | default | `{"trials_completed":integer}` | `plot.series`, `done` |
| `U` -> `H` -> `D` | Stochastic -> Data | `{"cmd":"key","win":"main","key":"d"}` | none | `analysis.load_stochastic_run` | `trial`: integer | default | `{"loaded":boolean}` | `browser.updated`, `done` |
| `U` -> `H` -> `M` | Stochastic -> Mean | `{"cmd":"key","win":"main","key":"m"}` | none | `analysis.load_stochastic_mean` | none | default | `{"loaded":boolean}` | `browser.updated`, `done` |
| `U` -> `H` -> `V` | Stochastic -> Variance | `{"cmd":"key","win":"main","key":"v"}` | none | `analysis.load_stochastic_variance` | none | default | `{"loaded":boolean}` | `browser.updated`, `done` |
| `U` -> `H` -> `H` | Stochastic -> Histogram | `{"cmd":"browser","op":"postprocess","kind":"hist"}` | `new_int` "Number of bins", `new_float` Low, Hi ([core/histogram.cpp:171](../../core/histogram.cpp#L171)) | `analysis.histogram` | `column`: name, `bins`: integer (>0), `low`: real, `high`: real | default | `{"bins":integer,"total_samples":integer}` | `browser.updated`, `plot.series`, `done` |
| `U` -> `H` -> `O` | Stochastic -> Old hist | `{"cmd":"key","win":"main","key":"o"}` | none | `analysis.reload_histogram` | none | default | `{"reloaded":boolean}` | `browser.updated`, `plot.series`, `done` |
| `U` -> `H` -> `2` | Stochastic -> 2D-hist | `{"cmd":"browser","op":"postprocess","kind":"hist2d"}` | prompts for bins, low, hi for 2 vars ([core/histogram.cpp:184](../../core/histogram.cpp#L184)) | `analysis.histogram_2d` | `column1`: name, `column2`: name, `bins1`: integer (>0), `bins2`: integer (>0), `low1`: real, `high1`: real, `low2`: real, `high2`: real | default | `{"bins_x":integer,"bins_y":integer}` | `browser.updated`, `plot.series`, `done` |
| `U` -> `H` -> `F` | Stochastic -> Fourier | `{"cmd":"browser","op":"postprocess","kind":"fourier"}` | none | `analysis.fourier_series` | `column`: name | default | `{"harmonics":integer}` | `browser.updated`, `done` |
| `U` -> `H` -> `P` | Stochastic -> Power | `{"cmd":"browser","op":"postprocess","kind":"power"}` | none | `analysis.power_spectrum` | `column`: name | default | `{"frequencies":integer}` | `browser.updated`, `plot.series`, `done` |
| `U` -> `H` -> `I` | Stochastic -> fIt data | `{"cmd":"key","win":"main","key":"i"}` | `do_string_box_of` "Fit" ([core/do_fit.cpp:528](../../core/do_fit.cpp#L528)) | `analysis.fit_curve` | `file`: file, `column`: name, `parameters`: list[name], `tolerance`: optional real | default | `{"converged":boolean,"parameters":map[name, real]}` | `session.state`, `done` |
| `U` -> `H` -> `S` | Stochastic -> Stat | `{"cmd":"key","win":"main","key":"s"}` | none | `analysis.statistics` | `column`: name | default | `{"mean":real,"variance":real,"std_dev":real}` | `done` |
| `U` -> `H` -> `L` | Stochastic -> Liapunov | `{"cmd":"key","win":"main","key":"l"}` | none | `analysis.lyapunov_exponent` | `transient`: optional real, `iterations`: optional integer | default | `{"max_lyapunov_exponent":real}` | `done` |
| `U` -> `H` -> `A` | Stochastic -> stAutocor | `{"cmd":"key","win":"main","key":"a"}` | none | `analysis.autocorrelation` | `column`: name, `max_lag`: optional integer | default | `{"lags_computed":integer}` | `browser.updated`, `done` |
| `U` -> `H` -> `X` | Stochastic -> Xcorrel etc | `{"cmd":"key","win":"main","key":"x"}` | none | `analysis.cross_correlation` | `column1`: name, `column2`: name, `max_lag`: optional integer | default | `{"lags_computed":integer}` | `browser.updated`, `done` |
| `U` -> `H` -> `E` | Stochastic -> spEc.dns | `{"cmd":"key","win":"main","key":"e"}` | prompts for window and width | `analysis.spectral_density` | `column`: name, `window_size`: integer (>0), `window_type`: choice("rectangular","hamming","hann","bartlett") | default | `{"points":integer}` | `browser.updated`, `done` |
| `U` -> `P` | Numerics -> Poincare map | `{"cmd":"set","kind":"num","name":"poincare_..."}` | `do_string_box_of` "Poincare map" ([core/numerics.cpp:319](../../core/numerics.cpp#L319)) | `analysis.poincare_map` | `type`: choice("none","section","max_min","period"), `variable`: optional name, `section_value`: optional real, `direction`: optional choice("positive","negative","both"), `stop_on_cross`: optional boolean | default | `{"poincare_configured":boolean}` | `session.state`, `done` |
| `A` -> `*` | Phase space -> (A)ll/(N)one/(C)hoose | `{"cmd":"key","win":"main","key":"*"}` | `menu_torus` (checklist in Choose) | `analysis.phase_space_torus` | `mode`: choice("all","none","choose"), `variables`: optional list[name] | default | `{"torus_variables":list[name]}` | `session.state`, `done` |
| `U` -> `C` | Numerics -> Color by value | `{"cmd":"set","kind":"options","name":"colorvia"}` | `new_string_of` "Color via:", `new_float` Min, Max ([core/numerics.cpp:389](../../core/numerics.cpp#L389)) | `analysis.color_coding` | `mode`: choice("none","velocity","variable"), `variable`: optional name, `low`: optional real, `high`: optional real | default | `{"color_coding":choice("none","velocity","variable")}` | `plot.shown`, `session.state`, `done` |
| `U` -> `A` -> `N` | Adjoint -> (N)ew adj | `{"cmd":"key","win":"main","key":"n"}` | none | `analysis.adjoint_compute` | none | default | `{"computed":boolean}` | `done` |
| `U` -> `A` -> `M` | Adjoint -> (M)ake H | `{"cmd":"key","win":"main","key":"m"}` | `new_string_of` coupling expression ([core/adj2.cpp:245](../../core/adj2.cpp#L245)) | `analysis.adjoint_interaction` | `coupling`: expression | default | `{"computed":boolean}` | `done` |
| `U` -> `A` -> `A` | Adjoint -> (A)djoint | `{"cmd":"key","win":"main","key":"a"}` | none | `analysis.adjoint_load` | none | default | `{"loaded":boolean}` | `browser.updated`, `done` |
| `U` -> `A` -> `O` | Adjoint -> (O)rbit | `{"cmd":"key","win":"main","key":"o"}` | none | `analysis.adjoint_load_orbit` | none | default | `{"loaded":boolean}` | `browser.updated`, `done` |
| `U` -> `A` -> `H` | Adjoint -> (H)fun | `{"cmd":"key","win":"main","key":"h"}` | none | `analysis.adjoint_load_interaction` | none | default | `{"loaded":boolean}` | `browser.updated`, `done` |
| `U` -> `A` -> `P` | Adjoint -> (P)arameters | `{"cmd":"key","win":"main","key":"p"}` | `new_int` Max iterates, `new_float` tolerance ([core/adj2.cpp:197](../../core/adj2.cpp#L197)) | `analysis.adjoint_configure` | `max_iterates`: integer (>0), `tolerance`: real (>0) | default | `{"updated":boolean}` | `session.state`, `done` |
| `U` -> `A` -> `R` | Adjoint -> (R)ange | `{"cmd":"key","win":"main","key":"r"}` | `new_int` "Range over parameters?(0/1)" ([core/adj2.cpp:492](../../core/adj2.cpp#L492)) | `analysis.adjoint_range` | `parameter`: name, `start`: real, `end`: real, `steps`: integer (>0) | default | `{"steps_completed":integer}` | `done` |

---

### 2.12 Resource `table` (Lookup Tables)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `U` -> `K` -> `E` | Tables -> (E)dit | `{"cmd":"key","win":"main","key":"e"}` | `pick_from_list` table name, `new_int` autoeval, `new_int` npts, `new_float` xlo/xhi, `new_string_of` formula ([core/tabular.cpp:162](../../core/tabular.cpp#L162)) | `table.edit` | `name`: name, `formula`: expression, `x_min`: real, `x_max`: real, `points`: integer (>0), `auto_evaluate`: optional boolean | default | `{"table":name,"points_evaluated":integer}` | `session.state`, `done` |
| `U` -> `K` -> `V` | Tables -> (V)iew | `{"cmd":"key","win":"main","key":"v"}` | `pick_from_list` table name | `table.view_in_browser` | `name`: name | default | `{"table":name,"shown_in_browser":boolean}` | `browser.updated`, `done` |

---

### 2.13 Resource `browser` (Data Browser and Storage Operations)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `B` -> `F` | Data browser -> (F)ind | `{"cmd":"browser","op":"find"}` | `do_string_box_of` "Find Data" ([core/browse_data.cpp:728](../../core/browse_data.cpp#L728)): Variable, Value | `browser.find` | `column`: name, `value`: real | default | `{"row_found":integer}` | `browser.updated`, `done` |
| `B` -> `G` | Data browser -> (G)et | `{"cmd":"browser","op":"get"}` | none | `browser.get_row_as_ic` | `row`: optional integer | default | `{"row":integer,"applied":map[name, real]}` | `session.state`, `done` |
| `B` -> `R` | Data browser -> (R)eplace | `{"cmd":"browser","op":"replace"}` | string prompts for column and replacement formula | `browser.replace_column` | `column`: name, `formula`: expression | default | `{"column":name,"replaced":boolean}` | `browser.updated`, `done` |
| `B` -> `U` | Data browser -> (U)nreplace | `{"cmd":"browser","op":"unreplace"}` | none | `browser.undo_replace` | none | default | `{"undone":boolean}` | `browser.updated`, `done` |
| `B` -> `T` | Data browser -> (T)able | `{"cmd":"browser","op":"table"}` | `do_string_box_of` "Tabulate" ([core/browse_data.cpp:704](../../core/browse_data.cpp#L704)) | `browser.tabulate_expression` | `formula`: expression, `start`: real, `step`: real, `steps`: integer (>0) | default | `{"rows_generated":integer}` | `browser.updated`, `done` |
| `B` -> `H` | Data browser -> (H)ome: first | `{"cmd":"browser","op":"first"}` | none | `browser.set_slice_start` | `row`: integer (>=0) | default | `{"first_row":integer}` | `browser.updated`, `done` |
| `B` -> `E` | Data browser -> (E)nd: last | `{"cmd":"browser","op":"last"}` | none | `browser.set_slice_end` | `row`: integer (>=0) | default | `{"last_row":integer}` | `browser.updated`, `done` |
| `B` -> `S` | Data browser -> re(S)tore | `{"cmd":"browser","op":"restore"}` | none | `browser.restore_full_slice` | none | default | `{"first_row":0,"last_row":integer}` | `browser.updated`, `done` |
| `B` -> `A` | Data browser -> (A)dd column | `{"cmd":"browser","op":"addcol"}` | string prompts for column name and formula | `browser.add_column` | `name`: name, `formula`: expression | default | `{"column":name,"added":boolean}` | `browser.updated`, `done` |
| `B` -> `D` | Data browser -> (D)elete column | `{"cmd":"browser","op":"delcol"}` | none | `browser.delete_column` | none | default | `{"deleted":boolean}` | `browser.updated`, `done` |
| `B` -> `L` | Data browser -> (L)oad | `{"cmd":"browser","op":"load"}` | `file_selector` | `browser.load` | `file`: file | default | `{"file":file,"rows_loaded":integer}` | `browser.updated`, `done` |
| `B` -> `W` | Data browser -> (W)rite | `{"cmd":"browser","op":"write"}` | `menu_save_what` ("The data table", "What the plot shows"), `file_selector` | `browser.export` | `file`: file, `format`: choice("csv","parquet","tsv"), `target`: optional choice("table","plot_curves") | default | `{"file":file,"rows_written":integer}` | `done` |
| (Page scroll) | (Paging rows) | `{"cmd":"browser","from":N}` | none | `browser.page` | `from_row`: integer (>=0), `count`: integer (>0) | default | `{"first_row":integer,"total_rows":integer,"rows":list[list[real]]}` | `done` |
| `F` -> `T` | File -> Transpose data | `{"cmd":"key","menu":"file","item":"transpose"}` | `do_string_box_of` "Transpose Data" ([core/adj2.cpp:77](../../core/adj2.cpp#L77)) | `browser.transpose` | `row_start`: integer, `row_end`: integer, `row_step`: integer, `col_start`: integer, `col_end`: integer | default | `{"transposed_rows":integer,"transposed_cols":integer}` | `browser.updated`, `done` |

---

### 2.14 Resource `auto` (Bifurcation Continuation and AUTO Diagnostics)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `F` -> `A` | File -> AUTO continuation | `{"cmd":"key","menu":"file","item":"auto"}` | none | `auto.open` | none | default | `{"open":boolean}` | `auto.diagram`, `session.state`, `done` |
| `A` -> `R` (AUTO) | AUTO -> (R)un | `{"cmd":"auto","op":"restart"}` | `menu_auto_start` ("Steady state", "Periodic", "Bdry Value", "Homoclinic", "Heteroclinic") or special pt continuation | `auto.run` | `start_type`: optional choice("steady_state","periodic","boundary_value","homoclinic","heteroclinic"), `branch_type`: optional choice("switch","extend","new_point","two_parameter","fixed_period","doubling"), `direction`: optional choice("forward","backward","both") | default | `{"points_computed":integer,"branches_tracked":integer}` | `job.progress`, `auto.diagram`, `done` |
| `A` -> `G` (AUTO) | AUTO -> (G)rab | `{"cmd":"auto","op":"grab","label":N}` | `new_string_of` "Mark values: how many?" / mouse click on diagram point | `auto.grab` | `label`: optional integer, `index`: optional integer, `run`: optional integer, `x`: optional real, `y`: optional real | default | `{"grabbed":boolean,"point":map[string, real]}` | `auto.diagram`, `done` |
| `A` -> `N` (AUTO) | AUTO -> (N)umerics | `{"cmd":"auto","op":"set",...}` | `do_string_box_of` "AutoNum" ([core/auto_nox.cpp:764](../../core/auto_nox.cpp#L764)): 22 fields | `auto.configure_numerics` | `mesh_intervals`: optional integer, `max_points`: optional integer, `print_every`: optional integer, `collocation_points`: optional integer, `step_size`: optional real, `step_min`: optional real, `step_max`: optional real, `parameter_min`: optional real, `parameter_max`: optional real, `norm_min`: optional real, `norm_max`: optional real, `tolerance_parameters`: optional real, `tolerance_solution`: optional real, `tolerance_special`: optional real, `adapt_mesh_every`: optional integer, `branch_switch_limit`: optional integer, `output_detail`: optional integer, `locate_iterations`: optional integer, `newton_iterations`: optional integer, `full_newton_iterations`: optional integer, `adapt_step_every`: optional integer, `skip_branch_points`: optional boolean | default | `{"settings_updated":boolean}` | `auto.settings`, `session.state`, `done` |
| `A` -> `A` (AUTO) | AUTO -> (A)xes | `{"cmd":"auto","op":"set",...}` | `do_string_box_of` "AutoPlot" ([core/auto_nox.cpp:871](../../core/auto_nox.cpp#L871)): Plot type, vars, bounds | `auto.configure_plot` | `plot_type`: choice("hi","norm","hi_lo","period","two_parameter","frequency","average"), `variable`: name, `parameter1`: name, `parameter2`: optional name, `x_min`: real, `x_max`: real, `y_min`: real, `y_max`: real | default | `{"plot_configured":boolean}` | `auto.view`, `done` |
| `A` -> `U` (AUTO) | AUTO -> (U)sr period | `{"cmd":"auto","op":"set","marks":[...]}` | `new_string_of` count, prompts | `auto.configure_marks` | `marks`: list[map[string, real]] | default | `{"marks_count":integer}` | `auto.settings`, `done` |
| `A` -> `C` (AUTO) | AUTO -> (C)lear | `{"cmd":"auto","op":"clear"}` | none | `auto.clear_diagram` | none | default | `{"cleared":boolean}` | `auto.diagram`, `done` |
| `A` -> `D` (AUTO) | AUTO -> re(D)raw | `{"cmd":"auto","op":"redraw"}` | none | `auto.redraw` | none | default | `{"redrawn":boolean}` | `auto.diagram`, `done` |
| (AUTO window) | (Clear grabbed point) | `{"cmd":"auto","op":"point","lab":0}` | none | `auto.clear_grab` | none | default | `{"cleared":boolean}` | `auto.diagram`, `done` |
| `A` -> `F` -> `I` | AUTO File -> Import orbit | `{"cmd":"key","win":"auto","key":"i"}` | none | `auto.load_orbit_into_browser` | `label`: optional integer | default | `{"orbit_loaded":boolean,"rows":integer}` | `browser.updated`, `done` |
| `A` -> `F` -> `S` | AUTO File -> Save diagram | `{"cmd":"session","op":"save"}` | file save prompt (.snapx) | `auto.save_session` | `file`: optional file | default | `{"file":file,"saved":boolean}` | `done` |
| `A` -> `F` -> `L` | AUTO File -> Load diagram | `{"cmd":"session","op":"open"}` | file open prompt (.snapx) | `auto.open_session` | `file`: file | default | `{"file":file,"loaded":boolean}` | `auto.diagram`, `session.state`, `done` |
| `A` -> `F` -> `P` | AUTO File -> Postscript | `{"cmd":"key","win":"auto","key":"p"}` | `file_selector` | `auto.export_postscript` | `file`: file | default | `{"file":file,"bytes_written":integer}` | `done` |
| `A` -> `F` -> `V` | AUTO File -> SVG | `{"cmd":"key","win":"auto","key":"v"}` | `file_selector` | `auto.export_svg` | `file`: file | default | `{"file":file,"bytes_written":integer}` | `done` |
| `A` -> `F` -> `R` | AUTO File -> Reset diagram | `{"cmd":"key","win":"auto","key":"r"}` | `TwoChoice` "Destroy AUTO diagram & files" ([core/auto_nox.cpp:1608](../../core/auto_nox.cpp#L1608)) | `auto.reset_all` | none | default | `{"reset":boolean}` | `auto.diagram`, `done` |
| `A` -> `F` -> `W` | AUTO File -> Write pts | `{"cmd":"key","win":"auto","key":"w"}` | `file_selector` | `auto.export_points` | `file`: file | default | `{"file":file,"points_written":integer}` | `done` |
| `A` -> `F` -> `A` | AUTO File -> All info | `{"cmd":"key","win":"auto","key":"a"}` | `file_selector` | `auto.export_all_info` | `file`: file | default | `{"file":file,"written":boolean}` | `done` |
| `A` -> `F` -> `D` | AUTO File -> init Data | `{"cmd":"key","win":"auto","key":"d"}` | `file_selector` | `auto.export_initial_data` | `file`: file | default | `{"file":file,"written":boolean}` | `done` |
| `A` -> `F` -> `T` | AUTO File -> Toggle redraw | `{"cmd":"key","win":"auto","key":"t"}` | none | `auto.set_auto_redraw` | `enabled`: boolean | default | `{"auto_redraw":boolean}` | `done` |
| `A` -> `F` -> `N` | AUTO File -> auto raNge | `{"cmd":"key","win":"auto","key":"n"}` | none | `auto.range_branch` | `branch`: integer | default | `{"runs_completed":integer}` | `auto.diagram`, `done` |
| `A` -> `F` -> `E` | AUTO File -> sElect 2par pt | `{"cmd":"key","win":"auto","key":"e"}` | none | `auto.select_2parameter_point` | `label`: integer | default | `{"selected":boolean}` | `auto.diagram`, `done` |
| `A` -> `F` -> `B` | AUTO File -> draw laBled | `{"cmd":"key","win":"auto","key":"b"}` | none | `auto.draw_labeled_orbits` | none | default | `{"orbits_drawn":integer}` | `plot.series`, `done` |
| `A` -> `F` -> `O` | AUTO File -> lOad branch | `{"cmd":"key","win":"auto","key":"o"}` | none | `auto.load_branch_into_browser` | `branch`: integer | default | `{"rows_loaded":integer}` | `browser.updated`, `done` |
| `A` -> `F` -> `X` | AUTO File -> eXport CSV | `{"cmd":"key","win":"auto","key":"x"}` | `file_selector` | `auto.export_csv` | `file`: file | default | `{"file":file,"points_written":integer}` | `done` |
| `A` -> `F` -> `F` | AUTO File -> Import diagram | `{"cmd":"key","win":"auto","key":"f"}` | `file_selector` | `auto.import_diagram` | `file`: file | default | `{"file":file,"points_imported":integer}` | `auto.diagram`, `done` |

---

### 2.15 Resource `animation` (Custom Geometric Animations)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `V` -> `T` | Axes -> Toon | `{"cmd":"key","win":"main","key":"t"}` | none | `animation.open` | none | default | `{"open":boolean}` | `session.state`, `done` |
| `N` -> `F` (Ani) | Animation -> (F)ile | `{"cmd":"ani","op":"file"}` | `file_selector` "*.ani" | `animation.load` | `file`: file | default | `{"file":file,"loaded":boolean}` | `session.state`, `done` |
| `N` -> `G` (Ani) | Animation -> (G)o | `{"cmd":"ani","op":"go"}` | none | `animation.play` | none | default | `{"playing":boolean}` | `done` |
| `N` -> `R` (Ani) | Animation -> (R)eset | `{"cmd":"ani","op":"reset"}` | none | `animation.reset` | none | default | `{"frame":0}` | `done` |
| `N` -> `S` (Ani) | Animation -> (S)kip | `{"cmd":"ani","op":"skip"}` | `new_int` "Skip count" | `animation.set_frame_skip` | `skip`: integer (>=0) | default | `{"skip":integer}` | `done` |
| `N` -> `M` (Ani) | Animation -> (M)peg | `{"cmd":"ani","op":"mpeg"}` | `do_string_box_of` "Frame saving" ([core/aniparse.cpp:1079](../../core/aniparse.cpp#L1079)) | `animation.export_frames` | `base_name`: text, `start_frame`: integer, `end_frame`: integer | default | `{"frames_saved":integer}` | `done` |
| `N` -> `O` (Ani) | Animation -> (O)n the fly | `{"cmd":"ani","op":"fly"}` | none | `animation.set_live_draw` | `enabled`: boolean | default | `{"live_draw":boolean}` | `done` |
| `N` -> `A` (Ani) | Animation -> gr(A)b | `{"cmd":"ani","op":"grab"}` | mouse click in animation window | `animation.grab_point` | `x`: real, `y`: real | default | `{"grabbed":boolean}` | `done` |

---

### 2.16 Resource `array_plot` (Spatiotemporal Plots)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `V` -> `A` | Axes -> Array | `{"cmd":"aplot"}` | none | `array_plot.open` | none | default | `{"open":boolean}` | `session.state`, `done` |
| `P` -> `D` (Aplot) | Array plot -> re(D)raw | `{"cmd":"aplot","op":"redraw"}` | none | `array_plot.redraw` | none | default | `{"redrawn":boolean}` | `done` |
| `P` -> `E` (Aplot) | Array plot -> (E)dit | `{"cmd":"aplot","op":"edit"}` | `do_string_box_of` "Edit arrayplot" ([core/arrayplot.cpp:246](../../core/arrayplot.cpp#L246)): 9 fields | `array_plot.configure` | `variable_prefix`: text, `start_index`: integer, `end_index`: integer, `row_start`: integer, `row_end`: integer, `column_min`: real, `column_max`: real, `color_style`: choice("continuous","contour"), `colormap`: choice("jet","periodic","hot","cool","blue_red","gray","cubehelix") | default | `{"configured":boolean}` | `done` |
| `P` -> `F` (Aplot) | Array plot -> (F)it | `{"cmd":"aplot","op":"fit"}` | none | `array_plot.fit` | none | default | `{"fitted_min":real,"fitted_max":real}` | `done` |
| `P` -> `R` (Aplot) | Array plot -> (R)ange | `{"cmd":"aplot","op":"range"}` | `do_string_box_of` "Array range saving" ([core/arrayplot.cpp:65](../../core/arrayplot.cpp#L65)) | `array_plot.range` | `parameter`: name, `start`: real, `end`: real, `steps`: integer (>0) | default | `{"steps_completed":integer}` | `done` |
| `P` -> `P` (Aplot) | Array plot -> (P)rint | `{"cmd":"aplot","op":"print"}` | `do_string_box_of` "Print arrayplot" ([core/arrayplot.cpp:190](../../core/arrayplot.cpp#L190)), `file_selector` | `array_plot.export_postscript` | `file`: file | default | `{"file":file,"bytes_written":integer}` | `done` |
| `P` -> `G` (Aplot) | Array plot -> (G)IF | `{"cmd":"aplot","op":"gif"}` | `file_selector` | `array_plot.export_gif` | `file`: file | default | `{"file":file,"bytes_written":integer}` | `done` |

---

### 2.17 Resource `set` (Named Parameter, State, and Options Sets)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `F` -> `G` | File -> Apply set… | `{"cmd":"applyset","name":N,"scope":S,"graph":G}` | `pick_from_list` set, scope, graph ([core/commands.cpp:138](../../core/commands.cpp#L138)) | `set.apply` | `name`: text, `scope`: optional choice("all","parameters","initial","options","plot","numerics"), `window`: optional window | default | `{"applied":text,"scope":text}` | `session.state`, `done` |
| `F` -> `O` | File -> Save values… | `{"cmd":"saveset","name":N,"scope":S,"graph":G,"last_state":B}` | prompts for mode (new/replace), name, scope, graph, initial/last | `set.save_new` | `name`: text, `scope`: optional choice("all","parameters","initial","options","plot","numerics"), `window`: optional window, `use_last_state`: optional boolean | default | `{"name":text,"saved":boolean}` | `session.state`, `done` |
| `F` -> `O` (replace) | File -> Save values… | `{"cmd":"saveset"}` | confirm replacement | `set.save_into` | `name`: text, `scope`: optional choice("all","parameters","initial","options","plot","numerics"), `window`: optional window, `use_last_state`: optional boolean | default | `{"name":text,"updated":boolean}` | `session.state`, `done` |
| `F` -> `J` | File -> Save as defaults… | `{"cmd":"savedefaults","scope":S,"graph":G,"last_state":B}` | prompts for scope, graph, initial/last | `set.save_defaults` | `scope`: optional choice("all","parameters","initial","options","plot","numerics"), `window`: optional window, `use_last_state`: optional boolean | default | `{"saved_as_defaults":boolean}` | `session.state`, `done` |
| `F` -> `I` | File -> Export values… | `{"cmd":"exportset","file":F,"name":N,"scope":S,...}` | prompts for scope, name, file | `set.export` | `file`: file, `name`: text, `scope`: optional choice("all","parameters","initial","options","plot","numerics"), `window`: optional window, `use_last_state`: optional boolean | default | `{"file":file,"name":text,"exported":boolean}` | `done` |
| `F` -> `Z` | File -> Rename set… | `{"cmd":"renameset","from":A,"to":B}` | prompts for existing set, new name | `set.rename` | `from`: text, `to`: text | default | `{"from":text,"to":text,"renamed":boolean}` | `session.state`, `done` |
| `F` -> `F` | File -> Delete set… | `{"cmd":"deleteset","name":N}` | prompt for set, confirm | `set.delete` | `name`: text | default | `{"name":text,"deleted":boolean}` | `session.state`, `done` |
| `F` -> `K` | File -> Restore defaults… | `{"cmd":"restoredefaults","scope":S,"graph":G}` | prompts for scope, graph | `set.restore_defaults` | `scope`: optional choice("all","parameters","initial","options","plot","numerics"), `window`: optional window | default | `{"restored":boolean}` | `session.state`, `done` |

---

### 2.18 Resource `value` (Parameters, Sliders, and Values Undo/Redo)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `P` | Parameters | `{"cmd":"set","kind":"par",...}` / `{"cmd":"values"}` | string box per parameter | `value.set_parameters` | `values`: map[name, real] | default | `{"updated":map[name, real]}` | `session.state`, `done` |
| Ctrl+Z / `Z` | Undo value edit | `{"cmd":"key","menu":"main","item":"undo"}` | none | `value.undo` | none | default | `{"undone":boolean}` | `session.state`, `done` |
| Ctrl+Y / `Y` | Redo value edit | `{"cmd":"key","menu":"main","item":"redo"}` | none | `value.redo` | none | default | `{"redone":boolean}` | `session.state`, `done` |
| (Values panel) | (Interactive slider) | `{"cmd":"slide","name":N,"value":V}` | none | `value.slide` | `name`: name, `value`: real | default | `{"name":name,"value":real}` | `session.state`, `done` |
| (Values panel) | (Configure slider) | `{"cmd":"slider","slot":S,"name":N,"low":L,"high":H,"step":T}` | none | `value.configure_slider` | `slot`: integer (>=0), `name`: name, `low`: real, `high`: real, `step`: optional real | default | `{"slot":integer,"configured":boolean}` | `session.state`, `done` |
| (Values panel) | (Reset value checkpoint) | `{"cmd":"values","op":"write"}` | none | `value.snapshot_checkpoint` | none | default | `{"checkpoint_taken":boolean}` | `session.state`, `done` |

---

### 2.19 Resource `session` (Session Files, Models, and System Lifecycle)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| Ctrl+O / `F` -> `M` | File -> Open model… | `{"cmd":"open","file":F}` / `{"cmd":"open","example":E}` | `file_selector` "*.ode;*.odex" | `session.open_model` | `file`: optional file, `example`: optional text | mutex(`file`, `example`) | `{"model":text,"variables":list[name],"parameters":list[name]}` | `session.state`, `done` |
| Ctrl+R / `F` -> `E` | File -> Reload model | `{"cmd":"reload"}` | none | `session.reload_model` | none | default | `{"reloaded":boolean}` | `session.state`, `done` |
| `F` -> `N` | File -> Open session… | `{"cmd":"session","op":"open","file":F}` | `file_selector` "*.snapx" | `session.open` | `file`: file | default | `{"file":file,"loaded":boolean}` | `session.state`, `done` |
| Ctrl+S / `F` -> `V` | File -> Save session | `{"cmd":"session","op":"save"}` | file prompt if first save | `session.save` | none | default | `{"file":file,"saved":boolean}` | `session.state`, `done` |
| Ctrl+Shift+S / `F` -> `W` | File -> Save session as… | `{"cmd":"session","op":"save","file":F}` | `file_selector` "*.snapx" | `session.save_as` | `file`: file | default | `{"file":file,"saved":boolean}` | `session.state`, `done` |
| `F` -> `B` | File -> Save a copy of the session… | `{"cmd":"session","op":"save","file":F,"copy":true}` | `file_selector` "*.snapx" | `session.save_copy` | `file`: file | default | `{"file":file,"copied":boolean}` | `done` |
| `F` -> `R` | File -> Import settings | `{"cmd":"set","kind":"file","file":F}` | `file_selector` "*.set;*.setx" | `session.import_settings` | `file`: file | default | `{"file":file,"settings_applied":integer}` | `session.state`, `done` |
| `F` -> `S` | File -> Export simulation information | `{"cmd":"key","menu":"file","item":"saveinfo"}` | `file_selector` "*.info" | `session.export_info` | `file`: file | default | `{"file":file,"written":boolean}` | `done` |
| `F` -> `P` | File -> Model source | `{"cmd":"equations"}` | none | `session.get_source` | none | default | `{"source":text,"equations":list[text]}` | `done` |
| Ctrl+Q / `F` -> `Q` | File -> Quit | `{"cmd":"quit","save":B}` | `confirm` "Save session first?" | `session.quit` | `save`: optional boolean | default | `{"quitting":true}` | `done` |
| (Client init) | (State query) | `{"cmd":"state"}` | none | `session.get_state` | none | default | (full state object) | `session.state`, `done` |

---

### 2.20 Resource `recording` (Replayable Session Steps)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `F` -> `D` | File -> Record steps | `{"cmd":"record","op":"start"}` | `file_selector` "*.recx" | `recording.start` | `file`: file | default | `{"file":file,"recording":true}` | `session.state`, `done` |
| `F` -> `D` (stop) | File -> Record steps | `{"cmd":"record","op":"stop"}` | none | `recording.stop` | none | default | `{"file":file,"steps_recorded":integer}` | `session.state`, `done` |
| (Page title bar) | (Add note to recording) | `{"cmd":"record","op":"note","note":T}` | string input dialog | `recording.add_note` | `note`: text | default | `{"note_added":boolean}` | `done` |
| `F` -> `Y` | File -> Play recording… | `{"cmd":"play","file":F}` | `file_selector` "*.recx" | `recording.open` | `file`: file | default | `{"file":file,"total_steps":integer}` | `session.state`, `done` |
| (Player bar) | (Start play) | `{"cmd":"play","op":"start"}` | none | `recording.play` | none | default | `{"playing":boolean}` | `done` |
| (Player bar) | (Pause play) | `{"cmd":"play","op":"pause"}` | none | `recording.pause` | none | default | `{"paused":boolean}` | `done` |
| (Player bar) | (Step play) | `{"cmd":"play","op":"step"}` | none | `recording.step` | none | default | `{"step_index":integer}` | `done` |
| (Player bar) | (Set playback speed) | `{"cmd":"play","op":"speed","speed":N}` | speed selector | `recording.set_speed` | `speed`: choice("real_time","fast","unlimited") | default | `{"speed":text}` | `done` |
| (Player bar) | (Close player) | `{"cmd":"play","op":"close"}` | none | `recording.close` | none | default | `{"closed":boolean}` | `session.state`, `done` |

---

### 2.21 Resource `keymap` (Keyboard Bindings and Quick Access)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| (Page command palette) | Keyboard shortcuts… | `{"cmd":"keymap"}` | keymap editor dialog | `keymap.get` | none | default | `{"bindings":map[text, list[text]],"pinned":list[text],"preset":text}` | `done` |
| (Keymap editor) | (Save bindings) | `{"cmd":"keymap","bindings":{...},"pinned":[...]}` | none | `keymap.set` | `bindings`: optional map[text, list[text]], `pinned`: optional list[text], `preset`: optional choice("default","xppaut") | default | `{"saved":boolean}` | `session.state`, `done` |
| (Keymap editor) | (Reset shortcuts) | `{"cmd":"keymap","reset":true}` | confirm | `keymap.reset` | none | default | `{"reset":true}` | `session.state`, `done` |

---

### 2.22 Resource `events` (Event Streams and Protocol Subscriptions)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| (Client handshake) | (Subscribe heavy events) | `{"cmd":"data","data":[...]}` | none | `events.subscribe` | `events`: list[choice("plot.series","auto.diagram","job.progress","marks.data","browser.data")] | default | `{"subscribed":list[text]}` | `done` |
| (Client handshake) | (Unsubscribe heavy events) | `{"cmd":"data","data":[]}` | none | `events.unsubscribe` | `events`: list[choice("plot.series","auto.diagram","job.progress","marks.data","browser.data")] | default | `{"unsubscribed":list[text]}` | `done` |

---

### 2.23 Resource `tool` (Auxiliary Utilities)

| Today Key Path | Today Menu Item | Today Protocol | Today Forms & Asks | Proposed Dotted Name | Typed Named Arguments | Group / Mutex | Result Payload | Events Emitted |
|---|---|---|---|---|---|---|---|---|
| `F` -> `C` | File -> Calculator | `{"cmd":"key","menu":"file","item":"calculator"}` | `new_string_of` expression loop ([core/json_prompts.cpp:541](../../core/json_prompts.cpp#L541)) | `tool.calculator` | `expression`: expression | default | `{"expression":expression,"value":real}` | `done` |

---

## 3. Unification of Multi-Route Operations

In the current codebase, several operations are duplicated across multiple disparate routes (e.g. legacy menu hotkeys, window layer shortcuts, protocol custom commands, and page action dispatchers). The dotted registry unifies each set of routes into a single command:

1. **Window bounds setting**:
   - Route A: Main menu `W` -> `W` ([core/commands.cpp:401](../../core/commands.cpp#L401)) opening `do_string_box_of` "Window" ([core/graf_par.cpp:468](../../core/graf_par.cpp#L468)).
   - Route B: Protocol `{"cmd":"display","xlo":...,"xhi":...,"ylo":...,"yhi":...}` ([core/json_windows.cpp:260](../../core/json_windows.cpp#L260)).
   - *Unified*: `window.set_bounds --x_min <val> --x_max <val> --y_min <val> --y_max <val> [--window <name>]`.
2. **Continue integration**:
   - Route A: Main menu `C` ([core/commands.cpp:270](../../core/commands.cpp#L270)) extending for one `total_time` duration.
   - Route B: Protocol `{"cmd":"continue","until":T}` or `{"cmd":"continue","extra":D}` ([core/ui_json.cpp:558](../../core/ui_json.cpp#L558)).
   - *Unified*: `run.continue [--until <T> | --extra <D>]`. Omitted arguments default to extending by `total_time`.
3. **Session saving**:
   - Route A: File menu `V` / `W` ([core/commands.cpp:533](../../core/commands.cpp#L533)) calling `xpp_session_save`.
   - Route B: AUTO File menu `S` ([core/menus.cpp:535](../../core/menus.cpp#L535)) calling session save.
   - Route C: Protocol `{"cmd":"session","op":"save","file":...}` ([core/ui_json.cpp:489](../../core/ui_json.cpp#L489)).
   - *Unified*: `session.save` and `session.save_as --file <path>`.
4. **Data export**:
   - Route A: Curves menu `exp(O)rt data` ([core/menus.cpp:351](../../core/menus.cpp#L351), `M_GO`).
   - Route B: Browser `(W)rite` ([core/menus.cpp:443](../../core/menus.cpp#L443), `menu_browser_window`).
   - Route C: Protocol `{"cmd":"browser","op":"write","file":...}` ([core/json_state.cpp:268](../../core/json_state.cpp#L268)).
   - *Unified*: `browser.export --file <name> --format <fmt> [--target <table|plot_curves>]`.
5. **Initial condition specification**:
   - Route A: Initialconds -> `(N)ew` ([core/commands.cpp:266](../../core/commands.cpp#L266)).
   - Route B: Protocol `{"cmd":"set","kind":"init","name":...,"value":...}` ([core/json_state.cpp:499](../../core/json_state.cpp#L499)).
   - Route C: Protocol `{"cmd":"values","values":{...}}` ([core/json_state.cpp:714](../../core/json_state.cpp#L714)).
   - *Unified*: `ic.set --values <map>`.

---

## 4. Front-End Capability Gaps Today

Several operations exist only in one front end today and cannot be accessed from others:
- **Desktop Window / X11 only**:
  - Nullcline animation (`nullcline.animate` / `M_NFA` in [core/commands.cpp:278](../../core/commands.cpp#L278)). The web client has no rendering hook for the timer loop.
  - Interactive scroll (`window.scroll` / `M_WS` in [core/commands.cpp:294](../../core/commands.cpp#L294)). The web client implements its own pan gestures.
  - In-place calculator (`tool.calculator` / `M_FC` in [core/commands.cpp:389](../../core/commands.cpp#L389)). Reached only via terminal or modal text loop.
- **Web UI (`web2/`) only**:
  - `run.steady` ([core/ui_json.cpp:572](../../core/ui_json.cpp#L572)). Exposed as a dedicated button and keybinding (Alt+S) on the web toolbar, but has no single-letter shortcut in the legacy XPPAUT main menu.
  - Dynamic parameter sliders (`value.slide`, `value.configure_slider` in [core/json_state.cpp:664](../../core/json_state.cpp#L664)). Present only in the web Values panel.
  - Snapshot undo/redo stack (`value.undo`, `value.redo` in [core/command_table.h:106](../../core/command_table.h#L106)). Main menu has `z` and `y`, but old XPPAUT only supported a single checkpoint.
- **Server / CLI only**:
  - Direct file export for equilibrium manifolds (`equilibrium.export` / [core/ui_json.cpp:532](../../core/ui_json.cpp#L532)).
  - Direct file export for direction fields (`dirfield.export` / [core/ui_json.cpp:531](../../core/ui_json.cpp#L531)).

---

## 5. Proposed Resource and Verb Taxonomy

### 5.1 Resource List (23 Resources)
1. `run`: Integrator trajectories, sweeps, continuation, and termination.
2. `ic`: Initial conditions inspection and assignment.
3. `equilibrium`: Fixed points, eigensystems, saddles, invariant manifolds.
4. `nullcline`: 2D nullcline computation, freezing, and animation.
5. `dirfield`: 2D direction and flow fields.
6. `window`: Plot window management, viewport bounds, zooming, and pan.
7. `plot`: Curve styling, axes options, 3D boxes, colormaps, and hardcopy.
8. `annotation`: Text labels, arrows, pointers, and data markers.
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
- **File & Serialization**: `load`, `save`, `save_as`, `save_copy`, `export`, `import`, `export_postscript`, `export_svg`, `export_gif`, `export_csv`.
- **State Stacking**: `undo`, `redo`, `freeze`, `clear_frozen`, `add_note`.
- **Protocol Control**: `subscribe`, `unsubscribe`.

---

## 6. Open Naming Questions for the Maintainer

1. **`run` vs `integrate`**:
   - *Current proposal*: `run.initial`, `run.last`, `run.continue`, `run.range`.
   - *Alternative*: `integrate.initial`, `integrate.continue`, etc.
   - *Recommendation*: Keep `run`. It is shorter, matches common speech, avoids awkwardness when running to steady state (`run.steady`) or non-ODE solvers, and aligns with modern UI toolbar conventions ("Run", "Continue").
2. **`annotation` vs `marker` vs `text`**:
   - Today XPPAUT groups labels, lines, arrows, pointers, and geometric shapes under `Text,etc` (`menu_text` in [core/menus.cpp:399](../../core/menus.cpp#L399)).
   - *Recommendation*: Resource `annotation` cleanly captures text labels, arrows, pointers, and markers without ambiguity.
3. **`value` vs `parameter`**:
   - Should parameter changes belong to `parameter.set` and state tweaks to `ic.set`, or should interactive value tweaking across parameters/ICs remain unified under `value.set`?
   - *Recommendation*: Use `value.set_parameters` and `ic.set` for batch assignments, but retain `value.slide` and `value.undo` for the shared numeric editing stack.
4. **AUTO 2-parameter continuation naming**:
   - In AUTO, 2-parameter continuation can be started from a Hopf point, a Limit point (fold), a Period Doubling point, or a Torus point.
   - *Current proposal*: `auto.run --branch_type two_parameter`.
   - *Question*: Should the start point type be an explicit argument (`--start_point hopf`) or automatically inferred from the grabbed point?
   - *Recommendation*: Automatically infer from the grabbed point if omitted, but allow explicit override `--start_point`.
5. **Data Export Verb Unification**:
   - In several places we export curves or tables (`export_postscript`, `export_svg`, `export_csv`, `export_data`).
   - *Question*: Should hardcopy exports be sub-verbs like `plot.export --format postscript` or distinct commands `plot.export_postscript`?
   - *Recommendation*: Use `plot.export_postscript` and `plot.export_svg` as dedicated commands since their options differ (PostScript takes font, font size, linewidth; SVG does not).
