# W651 Command ids audited: one verb per act, grouped by what they act on (#724)

Proposal for the maintainer, shown before any code (card W651). Source: the 259 rows of
`core/commands_*.cpp` (read by a script from the row initialisers: id, category, label, menu
path, description). Decisions of 2026-11-08 apply: ids are grouped by what a command acts on,
menus by task; one verb per act in id and label alike; `analysis.*` and `tool.*` are menu names
and dissolve into objects; `ic`, `bvp`, `dirfield` are spelled out; no alias, so every file that
names an id moves with it.

Totals: **259 commands; 102 change (157 unchanged). 87 ids are renamed or moved, 2 ids are removed
(Q1), and 35 labels change (verb only; W650 shortens labels later), 13 of those with the id unchanged.**
Every changed row is in section 2. **Status: approved by the maintainer on 2026-11-09 ("accept"), with
the decisions of section 5; the code follows this list.**

## 1. The verb vocabulary

Counts are the commands whose id has the verb as one of its first two words / whose label starts
with it (today). "Keep" means no change.

| Act | One verb | Replaces (ids / labels today) | What a user expects the word to mean |
|---|---|---|---|
| Take an object away for good (a curve, annotation, column, set, slider, frozen curve, imported diagram) | **delete** | delete 8/7 (kept), remove 1/3 (value.remove_slider; Remove all curves, Remove all frozen curves), clear 2 of its 5 ids (`nullcline.clear_frozen`, `plot.clear_bifurcation`) | "Delete" is final and names the object; "remove" and "clear" read as softer |
| Empty a display or a selection, the objects stay | **clear** | clear 5/5: `window.clear`, `dirfield.clear`, `autox.clear_grab` stay; the label "Clear" of `autox.hide_earlier` goes | Blank the picture; nothing is lost |
| Close a window or the player | **close** | destroy 2/1 (`window.destroy`, `window.destroy_all`); close 1/2 kept | The window goes; what is in it was already its own |
| Make a window or view invisible and visible again | **hide** / **show** | kept (hide 2/1, show 1/8) | Nothing is lost, it comes back as it was |
| Put a computed or stored result into the data table | **show** | load 8 of its 13 ids (`analysis.load_stochastic_*`, `adjoint_load*`, `autox.load_*_into_browser`) while their labels already say Show (8 labels); `table.view_in_browser` (view) | The thing appears; no file is read |
| Read one of our files into the program | **load** | load 13/8 keeps this meaning only | A file we wrote, read back whole (all or nothing) |
| Read another program's file or state | **import** | import 4/4 kept; load for `value.load_file`, `ic.load` (read XPPAUT's .par and .ic); `equilibrium.import_as_ic` (nothing is imported: it sets) | Foreign format, converted on the way in |
| Open a window or a document | **open** | open 7/6 kept; play in the label of `recording.open` | Window appears, or the file becomes the current one |
| Make an extra window or view | **new** | create 1/0 (`window.create`); new 2/3 kept | "New graph", "New autoX view" |
| Write our own file that can be loaded back | **save** | save 7/7 kept; replace in the label of `set.save_into` | Stores the session, a set |
| Write a derived or foreign-format file (PDF, SVG, CSV, GIF, data, text) | **export** | export 19/14 kept; write 0/3 (autox points, all info, initial data), make 0/2 (kinescope GIF, H function: GIF), save in the label of kinescope frames | Leaves the program for another tool |
| Compute a derived object (nullclines, adjoint, histogram, statistics, runs) | **compute** | compute 3/4 kept; make 0/2 and the histogram, spectrum and statistics descriptions that say "Make" | Run the maths, keep the result |
| Give one value | **set** | set 27/16 kept; new (seed), restore/default words | One field changes |
| Open a group of settings of one object | **configure** | configure 12/1 kept; edit (label of array_plot.configure) | A dialog of the object's settings; labelled by the settings noun |
| Change one existing member of a collection | **edit** | edit 4/4 kept; change 0/1 (`annotation.edit`) | A curve, annotation, table |
| Return to the default or first state | **reset** | reset 5/4 kept; default (autox.default_view, window.reset_default) | Back to how it started |
| Go back to what it was | **restore** | restore 2/2 kept; use (`autox.use_last_plot`); restore in the label of `nullcline.redraw` | Put an earlier state back |
| Draw again | **redraw** | redraw 4/4 kept; restore (label of `nullcline.redraw`) | Same data, fresh picture |
| Fit the view or a scale to the data | **fit** | fit 4/4 kept; `analysis.fit_curve` is another act (Q4) | The view follows the data |
| Stop a running job | **stop** | abort 1 (`run.abort`), stop 2/2 | Ends it, with its reason (Q3) |
| Read the values back | **get** / **list** | get 5/4 and list 2/2 kept; `table.view_in_browser`'s view | get one object's state, list a collection's members |
| Choose a point | **grab** / **pick** / **select** | kept (Q9) | See Q9 |
| Plot, draw | **draw** (put on a graph), plot only as a noun | draw 6/6 (draw, redraw), plot as a verb 1/0 | "Plot" names the window, not an act |

## 2. Every command whose id or label changes

Refs outside core = occurrences of the old id in docs, examples, tools, web2 (source, tests and
dist), tests, manual, CHANGELOG (section 4). Q = question in section 5.

| Old id | New id | Old label | New label | Reason | Refs outside core | Q |
|---|---|---|---|---|---|---|
| `analysis.poincare_map` | `poincare_map.set` | Poincare map | same | object (analysis is a menu) | 26 |  |
| `analysis.histogram` | `histogram.compute` | Histogram | same | object (analysis is a menu) | 35 |  |
| `analysis.histogram_2d` | `histogram.compute_2d` | 2D histogram | same | object (analysis is a menu) | 14 |  |
| `analysis.reload_histogram` | `histogram.show_last` | Show last histogram | same | object; verb reload -> show (the label already says show) | 6 |  |
| `analysis.statistics` | `statistics.compute` | Column statistics | same | object (analysis is a menu) | 7 |  |
| `analysis.fit_curve` | `fit.parameters` | Fit data | same | object (analysis is a menu); fit.compute rejected: "fit" is the view act elsewhere | 28 | Q4 |
| `analysis.lyapunov_exponent` | `lyapunov_exponent.compute` | Lyapunov exponent | same | object (analysis is a menu) | 15 |  |
| `analysis.fourier_series` | `spectrum.fourier_series` | Fourier series | same | object (analysis is a menu) | 8 |  |
| `analysis.power_spectrum` | `spectrum.power` | Power spectrum | same | object (analysis is a menu) | 7 |  |
| `analysis.spectral_density` | `spectrum.density` | Spectral density | same | object (analysis is a menu) | 25 |  |
| `analysis.autocorrelation` | `spectrum.autocorrelation` | Autocorrelation | same | object (analysis is a menu) | 9 |  |
| `analysis.cross_correlation` | `spectrum.cross_correlation` | Cross-correlation | same | object (analysis is a menu) | 14 |  |
| `analysis.stochastic_integrate` | `stochastic.compute` | Compute stochastic runs | same | object (analysis is a menu) | 13 |  |
| `analysis.load_stochastic_run` | `stochastic.show_run` | Show last run data | same | object; verb load -> show (label says show; load is for reading a file) | 18 |  |
| `analysis.load_stochastic_mean` | `stochastic.show_mean` | Show mean | same | object; verb load -> show (label says show; load is for reading a file) | 8 |  |
| `analysis.load_stochastic_variance` | `stochastic.show_variance` | Show variance | same | object; verb load -> show (label says show; load is for reading a file) | 8 |  |
| `analysis.seed_random` | `stochastic.set_seed` | New seed | Set seed | object; verb: it sets a value, "new" is for windows | 12 | Q6 |
| `analysis.adjoint_compute` | `adjoint.compute` | Compute adjoint | same | object (analysis is a menu) | 9 |  |
| `analysis.adjoint_interaction` | `adjoint.compute_h_function` | Make H function | Compute H function | object; verb make -> compute (maintainer example had make_h_function) | 16 | Q5 |
| `analysis.adjoint_range` | `adjoint.range` | Adjoint over a range | same | object (analysis is a menu) | 12 |  |
| `analysis.adjoint_load` | `adjoint.show` | Show adjoint | same | object; verb load -> show | 10 |  |
| `analysis.adjoint_load_orbit` | `adjoint.show_orbit` | Show orbit | same | object; verb load -> show | 5 |  |
| `analysis.adjoint_load_interaction` | `adjoint.show_h_function` | Show H function | same | object; verb load -> show | 8 |  |
| `analysis.adjoint_configure` | `adjoint.configure` | Adjoint settings | same | object (analysis is a menu) | 9 |  |
| `analysis.phase_space_torus` | `plot.set_torus` | Phase space torus | same | object (analysis is a menu) | 17 |  |
| `analysis.color_coding` | `plot.set_color_coding` | Colour by value | same | object (analysis is a menu) | 24 |  |
| `annotation.edit` | same | Change annotation | Edit annotation | label verb change -> edit | 18 |  |
| `array_plot.configure` | same | Edit array plot | Array plot settings | label verb: edit -> settings (configure rows are labelled by their settings) | 27 |  |
| `autox.configure_plot` | `autox.configure_axes` | Axes | same | object: it sets axes (label Axes), "plot" is the window | 46 |  |
| `autox.save_session` | **removed** | Save diagram | same | removed: the same act as session.save_as (both call xpp_session_save) | 4 | Q1 |
| `autox.open_session` | **removed** | Open diagram | same | removed: the same act as session.open (both call xpp_session_load) | 3 | Q1 |
| `autox.reset_all` | `autox.reset_diagram` | Reset diagram | same | object: the label says diagram, the id said all | 2 |  |
| `autox.export_points` | same | Write points | Export points | label verb: write -> export | 2 |  |
| `autox.export_all_info` | same | Write all info | Export all info | label verb: write -> export | 2 |  |
| `autox.export_initial_data` | same | Write initial data | Export initial data | label verb: write -> export | 2 |  |
| `autox.load_orbit_into_browser` | `autox.show_orbit` | Load orbit into browser | Show orbit | verb load -> show; "browser" is not a name a user sees | 6 |  |
| `autox.load_branch_into_browser` | `autox.show_branch` | Load branch into browser | Show branch | verb load -> show; "browser" is not a name a user sees | 2 |  |
| `autox.use_last_plot` | `autox.restore_plot` | Use last plot | Restore last plot | verb: use -> restore (go back to an earlier state) | 2 |  |
| `autox.default_view` | `autox.reset_view` | Default diagram view | Reset diagram view | verb: default -> reset | 8 |  |
| `autox.zoom` | `autox.zoom_in` | Zoom diagram in | same | pair: zoom_out has its partner zoom_in | 6 |  |
| `autox.hide_earlier` | same | Clear | Hide earlier branches | label verb: "Clear" -> hide (id says hide) | 8 |  |
| `browser.export` | `data.export` | Export data | same | object: the page and the menu call it Table and Data, `browser` is XPPAUT's word | 84 | Q8 |
| `browser.load` | `data.load` | Load data | same | object: the page and the menu call it Table and Data, `browser` is XPPAUT's word | 15 | Q8 |
| `browser.find` | `data.find` | Find | same | object: the page and the menu call it Table and Data, `browser` is XPPAUT's word | 14 | Q8 |
| `browser.replace_column` | `data.replace_column` | Replace | same | object: the page and the menu call it Table and Data, `browser` is XPPAUT's word | 15 | Q8 |
| `browser.undo_replace` | `data.undo_replace` | Unreplace | Undo replace | object: the page and the menu call it Table and Data, `browser` is XPPAUT's word; label verb: "Unreplace" -> undo | 12 | Q8 |
| `browser.tabulate_column` | `data.tabulate_column` | Tabulate column | same | object: the page and the menu call it Table and Data, `browser` is XPPAUT's word | 20 | Q8 |
| `browser.set_slice_start` | `data.set_slice_start` | First row | same | object: the page and the menu call it Table and Data, `browser` is XPPAUT's word | 17 | Q8 |
| `browser.set_slice_end` | `data.set_slice_end` | Last row | same | object: the page and the menu call it Table and Data, `browser` is XPPAUT's word | 15 | Q8 |
| `browser.restore_full_slice` | `data.restore_full_slice` | All rows | same | object: the page and the menu call it Table and Data, `browser` is XPPAUT's word | 13 | Q8 |
| `browser.add_column` | `data.add_column` | Add column | same | object: the page and the menu call it Table and Data, `browser` is XPPAUT's word | 19 | Q8 |
| `browser.delete_column` | `data.delete_column` | Delete column | same | object: the page and the menu call it Table and Data, `browser` is XPPAUT's word | 15 | Q8 |
| `browser.transpose` | `data.transpose` | Transpose data | same | object: the page and the menu call it Table and Data, `browser` is XPPAUT's word | 15 | Q8 |
| `browser.page` | `data.page` | Browser page | same | object: the page and the menu call it Table and Data, `browser` is XPPAUT's word | 49 | Q8 |
| `bvp.solve` | `boundary_value.solve` | Solve boundary values | same | abbreviation | 15 |  |
| `bvp.solve_periodic` | `boundary_value.solve_periodic` | Solve periodic orbit | same | abbreviation | 11 |  |
| `bvp.solve_range` | `boundary_value.solve_range` | Solve over a range | same | abbreviation | 15 |  |
| `bvp.configure` | `boundary_value.configure` | Boundary value settings | same | abbreviation | 20 |  |
| `dirfield.compute` | `direction_field.compute` | Compute direction field | same | abbreviation | 11 |  |
| `dirfield.flow` | `direction_field.flow` | Flow | same | abbreviation | 8 |  |
| `dirfield.colorize` | `direction_field.colorize` | Colourize | same | abbreviation | 5 |  |
| `dirfield.clear` | `direction_field.clear` | Clear direction field | same | abbreviation | 5 |  |
| `dirfield.export` | `direction_field.export` | Export direction field | same | abbreviation | 7 |  |
| `equilibrium.import_as_ic` | `initial_conditions.set_from_equilibrium` | Import as initial conditions | Set as initial conditions | object (it acts on initial conditions); verb import -> set (nothing is imported) | 12 |  |
| `ic.set` | `initial_conditions.set` | Set initial conditions | same | abbreviation | 55 |  |
| `ic.formula` | `initial_conditions.set_by_formula` | Initial value by formula | Set initial value by formula | abbreviation; id and label gain the verb (value.set_by_formula is the parameter twin) | 20 |  |
| `ic.array` | `initial_conditions.set_by_array_formula` | Initial values by array formula | Set initial values by array formula | abbreviation; id and label gain the verb | 13 |  |
| `ic.load` | `initial_conditions.import` | Load initial conditions | Import initial conditions | abbreviation; verb load -> import (reads XPPAUT's .ic: one number per equation) | 15 | Q2 |
| `ic.from_data_row` | `initial_conditions.set_from_data_row` | Initial conditions from data row | Set initial conditions from data row | abbreviation; id and label gain the verb | 23 |  |
| `ic.at` | `run.from_point` | Run from a point | same | object: it runs, it does not set initial conditions | 18 |  |
| `ic.mouse_stream` | `run.from_every_point` | Run from every point | same | object (it runs); name: label says every point, "mouse stream" is not a word a user sees | 14 |  |
| `kinescope.export_gif` | same | Make kinescope GIF | Export kinescope GIF | label verb: make -> export | 14 |  |
| `kinescope.export_frames` | same | Save kinescope frames | Export kinescope frames | label verb: save -> export | 19 |  |
| `nullcline.redraw` | same | Restore nullclines | Redraw nullclines | label verb: restore -> redraw (id says redraw) | 4 |  |
| `nullcline.auto_mode` | `nullcline.set_redraw_mode` | Nullcline redraw mode | same | verb: a setting takes set (twin of window.set_redraw_mode) | 5 |  |
| `nullcline.clear_frozen` | `nullcline.delete_frozen` | Delete frozen nullclines | same | verb: clear -> delete (the label already says delete) | 4 |  |
| `plot.delete_all_curves` | same | Remove all curves | Delete all curves | label verb: remove -> delete | 5 |  |
| `plot.delete_all_frozen` | same | Remove all frozen curves | Delete all frozen curves | label verb: remove -> delete | 6 |  |
| `plot.import_bifurcation` | `plot.load_bifurcation` | Import bifurcation diagram | Load bifurcation diagram | verb: import -> load (the file is autoX's own Write points file, not a foreign one) | 9 |  |
| `plot.clear_bifurcation` | `plot.delete_bifurcation` | Clear bifurcation diagram | Delete bifurcation diagram | verb: clear -> delete (it removes an imported object) | 5 |  |
| `recording.open` | same | Play recording | Open recording | label verb: play -> open (id says open; recording.play plays) | 32 |  |
| `run.abort` | `run.stop` | Stop | same | verb: abort -> stop (the label is Stop) | 84 | Q3 |
| `run.dae_guess` | `initial_conditions.set_dae_guess` | DAE guess | Set DAE guess | object (sets guesses of initial conditions); id and label gain the verb | 11 |  |
| `set.save_into` | same | Replace in set | Save into set | label verb: replace -> save | 23 |  |
| `set.file_contents` | `set.list_file_contents` | Load from file | List file contents | verb: it lists, it does not load (set.load_file loads) | 26 |  |
| `table.view_in_browser` | `table.show` | View table | Show table | verb view -> show; "browser" is not a name a user sees | 11 | Q8 |
| `tool.calculator` | `calculator.evaluate` | Calculator | same | object (tool is a menu name) | 46 |  |
| `tool.values` | `values.open` | Values | same | object (tool is a menu name) | 7 |  |
| `tool.browser` | `data.open` | Table | same | object (tool is a menu name); browser is now data | 10 | Q8 |
| `tool.command_palette` | `command_palette.open` | Search commands | same | object (tool is a menu name) | 20 |  |
| `tool.keyboard_shortcuts` | `keymap.edit` | Keyboard shortcuts | same | object (tool is a menu name) | 15 |  |
| `tool.manual` | `manual.open` | Manual | same | object (tool is a menu name) | 4 |  |
| `tool.about` | `about.open` | About xppautX | same | object (tool is a menu name) | 6 |  |
| `tool.run_source_action` | `source.run_action` | Run source action | same | object (tool is a menu name) | 16 |  |
| `tool.run_user_button` | `button.run` | Run user button | Run model button | object (tool is a menu name) | 17 |  |
| `value.load_file` | `value.import_file` | Load values file | Import values file | verb: it reads files XPPAUT wrote (.par, .ic) | 42 | Q2 |
| `value.remove_slider` | `value.delete_slider` | Remove slider | Delete slider | verb: remove -> delete | 9 |  |
| `window.reset_default` | `window.reset_view` | Default view | Reset view | verb: default -> reset, object view | 8 |  |
| `window.zoom` | `window.zoom_in` | Zoom in | same | pair: zoom_out has its partner zoom_in | 11 |  |
| `window.create` | `window.new` | New graph | same | verb: create -> new (label New graph; autox.new_view) | 65 |  |
| `window.destroy_all` | `window.close_extra` | Close extra graphs | same | verb: destroy -> close (label says Close extra graphs) | 10 |  |
| `window.destroy` | `window.close` | Destroy window | Close window | verb: destroy -> close | 58 |  |

Objects the moves create (new or grown resources): `histogram`, `spectrum`, `stochastic`,
`statistics`, `lyapunov_exponent`, `fit`, `poincare_map`, `adjoint`, `boundary_value`,
`direction_field`, `initial_conditions`, `values`, `calculator`, `command_palette`, `manual`,
`about`, `source`, `button`, plus `run`, `plot`, `window`, `keymap` and `browser` which gain rows.
Nothing remains in `analysis.*` or `tool.*`.

## 3. Collisions

Checked by script: no new id equals an existing id, and no two new ids coincide (73 new ids,
all distinct, none among the 259 current ids). Near misses to know about:

- `initial_conditions.*` is new, but `ic.*` and `value.*` both set things; `value.set_parameters`,
  `value.set_by_formula` and `initial_conditions.set_by_formula` are twins on purpose.
- `browser.open` (from `tool.browser`) joins `browser.*`; if Q8 renames `browser` to `data` it
  becomes `data.open`. `table.*` (function tables) stays untouched: `data` is why `table` is not
  used for the data table (Q8, accepted).
- `window.close` and `window.new` are free; `run.stop` is free (`recording.stop` is another object).
- `command_palette.open`, `values.open`, `manual.open`, `about.open` are page commands with no
  handler (`page_tool`); their ids still go through hello and the keymap.
- A resource named `set` (named sets) next to the verb `set` stays readable (`set.apply`,
  `value.set_parameters`); left as is.

## 4. What moves with it

Occurrences of the 89 old ids (word match, so `ic.set` does not match `ic.set_...`), per area:

| Area | Occurrences | Files |
|---|---|---|
| core/ (rows, `json_silent.cpp`, `nullcline.cpp`, `command_registry.cpp`, ...) | 151 | 41 |
| docs/protocol.md | 185 | 1 |
| docs/manual/ | 29 | 5 (3 files name `analysis.` or `tool.`) |
| docs/ other (studies, roadmap, findings, ui-v2) | 275 | 9 |
| examples/ (3 .recx, 1 more) | 4 | 3 |
| tools/ (servercheck.py, webcheck.py, autoxcheck.py, xppclient.py, web2check.mjs, steadycheck.py, ...) | 696 | 10 |
| web2/src and web2/test | 138 | 34 |
| web2/dist (rebuilt, not edited) | 105 | 2 |
| tests/ | 14 | 2 |
| CHANGELOG.md and others | 30 | 2 |
| paper/ | 1 | 1 |
| **Total outside core** | **1477** | |

Beyond ids themselves:

- core/commands_analysis.cpp (26 rows), commands_tool.cpp (8), commands_ic.cpp, commands_bvp.cpp,
  commands_dirfield.cpp are named after the old resources: they split and rename by object.
- The event `dirfield.field` (34 uses) and the option keys `bvp.max_iterates`, `bvp.tolerance`,
  `bvp.epsilon` (7 uses each, .odex `options` keys) carry the abbreviation too (Q7). Stale
  mentions `bvp.set_show_mode` and `dirfield.configure` exist only in docs/studies; they name no row.
- Default keys sit on rows (`run.abort` has KEY_1); keymap files and recordings reference ids:
  the three `.recx` in examples/recordings, and `tests/` fixtures.
- Hand-written label text is not in web2 (W636: the page reads the row), so the 34 label changes
  cost docs/manual's menu paths, servercheck's label check and CHANGELOG; W650 shortens labels
  afterwards (Show orbit, Export kinescope GIF already repeat their menu's words).

## 5. Questions and the maintainer's decisions (2026-11-09, rule: "no duplication and clear names")

1. **`autox.save_session`, `autox.open_session`**: decided: removed. They called the same functions as
   `session.save_as` and `session.open`. The empty-diagram refusal went with them. The registry puts a
   command in one menu place, so autoX > File no longer lists Save diagram and Open diagram; File > Save
   session as and File > Open session are the one place. `xpp_session_load` (nothing else called it) went too.
2. **load or import**: decided: a reader of a format only XPPAUT writes is import (`value.import_file`,
   `initial_conditions.import`); `set.load_file` stays load.
3. **`run.abort`**: decided: `run.stop`; the control-line classifier's `ABORT_COMMAND` follows; a recording's
   `abort` field stays a field.
4. **`analysis.fit_curve`**: decided: `fit.parameters` (not `fit.compute`); the label stays "Fit data".
5. **Verbs of result objects**: decided: compute is the one verb for a derived result
   (`adjoint.compute_h_function`); result nouns without a verb stay (`spectrum.density`).
6. **Seed**: decided: `stochastic.set_seed`, labelled "Set seed".
7. **Events and option keys**: decided: they follow (`direction_field.field`, `boundary_value.max_iterates`,
   `.tolerance`, `.epsilon`; with Q8 also the event `data.updated`, which was `browser.updated`). The
   recordings' embedded session files were rewritten with them.
8. **`browser` object**: decided: `data` (14 commands, plus `table.show`; the 13 `browser.*` rows and
   `tool.browser` are in the table above). `table.*` stays the formula tables. The window kind name
   The window kind `browser`, hello's `window_names.browser` and `limits.browser_rows/cols`, the page's
   `LayerWindow 'browser'` and the saved session key `browser_hidden` were not renamed: over 100 sites and a
   saved file format; left for the maintainer's decision.
9. **grab, pick, select**: decided: stay.
10. **`color` in ids**: decided: ids keep `color`; the spelling of user text is W669's.

Left as they were: the registry tables' order (`TABLES` in core/command_registry.cpp), because it fixes
the order of the menus and the menus do not change place, so `commands_analysis.cpp` and
`commands_tool.cpp` keep their rows (their ids now name their objects). `commands_ic.cpp`,
`commands_bvp.cpp`, `commands_dirfield.cpp` and `commands_browser.cpp` became `commands_initial_conditions.cpp`,
`commands_boundary_value.cpp`, `commands_direction_field.cpp` and `commands_data.cpp`.

A saved keymap (keymap.json, the config folder) that binds or pins an old id is refused whole by
`parse_keymap` with "unknown command" at its line, naming the id; the keymap shown is the defaults marked
as an error. No translation was added (no fallbacks). The page's own settings in localStorage store no
command ids.
