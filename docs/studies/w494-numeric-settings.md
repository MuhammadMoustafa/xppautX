# W494: every numeric setting, its range and where it is checked (#558)

Maintainer, 2026-10-25: a setting that must lie in a range is refused wherever it
comes from, never corrected at run time. This study lists the numeric settings of
core/model_options.cpp's table and the other places that read the same
setting, the rule each applies, the rule it needs, and the run-time correction
that existed (W494's first part, 342ff408, did AUTO's continuation numbers:
auto_settings.cpp's `num_fields`, which is not repeated here).

## The one place

- A value's own range is the row's `OptionRule` (and `maximum`) in model_options.cpp;
  `rule_problem` is the one check. The @ line / `.odex` option (`option_value`), the
  Numerics menu, `set num` (`numerics_settings_set`), the `.setx` and session
  readers (lunch-new.cpp, setx_file.cpp `Take::table`) and the options schema
  (`option_value_schema`) all go through it.
- An order between two settings is a row of `pairs` in model_options.cpp
  (`OptionPair`, `order_problem`, `option_pairs_problem`). It is checked: after a
  model's options and defaults are all in (`set_all_vals`, the error at the line of
  the upper setting), on every `set num` / Numerics menu change (the setting is left as
  it was), when a `.setx`/session file is read (at the line of the setting), and by the
  dialogs that take the same numbers (Window, the histogram dialogs), which call
  `order_problem` themselves.
- New rule `whole_at_least_two` (a grid or a number of bins: one cell has no spacing).

## The table

Rule today = before this card; needs = after. "Correction" = the run-time repair.

| Setting | Read at | Rule before | Rule now (why) | Correction |
|---|---|---|---|---|
| total_time | row, menu, set num, .setx | nonnegative | same (a run has a length) | none (an XPPAUT negative Total is an import, imported_total) |
| dt | same | nonzero | same (0 never advances) | none |
| start_time, store_from | same | any | any (a time may be any number); transient against the run's end depends on start_time and dt's sign, so `check_store_from` (solver.cpp) refuses it at the start of a run, with the setting, its value and the run (kept: no pair of two settings states it) | refusal, kept |
| store_every, nullcline_mesh | same | whole_positive (+ mesh max) | same | none |
| max_iterations, newton_tolerance, jacobian_step, search_limit, state_limit | same | positive / whole_positive | same | none |
| relative_tolerance, absolute_tolerance, min_step, max_step | same | positive | same; plus min_step <= max_step (pair: no step size would fit both) | `check_step_limits`' min_step > max_step refusal removed (unreachable) |
| eul_tol, eul_iter, delay, bvp_* | same | positive / nonneg | same | none |
| bandup, bandlo | row | nonneg, max bandwidth | same | none |
| torus_period | @ line | any (0 or negative accepted) | positive (the period divides the time into cycles) | none |
| nplot | @ line | any | whole_positive, at most 8 (XPPAUT's most curves in a window; xp2 to xp8) | `set_extra_graphs` clamp to 8 removed |
| rangestep | @ line, .setx | any | whole_positive (the Range forms already had `.min=1`) | none |
| dfgrid | @ line; Direct field dialog | any (dialog: silently ignored at <= 1) | whole_at_least_two, at most the mesh limit; the dialog refuses with the rule's words | silent `grid<=1` return replaced by the error |
| histbins, histbins2 | @ line; histogram dialogs | whole_positive; dialogs: "At least 2 bins" | whole_at_least_two (the dialogs and the options now say one thing); the dialogs call `rule_problem` | dialog texts replaced |
| histlo < histhi, histlo2 < histhi2 | @ line; histogram dialogs | dialogs only | pair | dialog texts replaced by `order_problem` |
| xlo < xhi, ylo < yhi (the 2D window) | @ line; Window dialog | corrected at load (to -2..2) | pair; the Window dialog refuses | load_eqn.cpp's reset to -2..2 removed |
| xmin < xmax, ymin < ymax | @ line | corrected at load (to -1..1) | pair (the 3D box; the defaults seed it from the 2D window, so an unset one holds) | the reset to -1..1 removed with the rest |
| zmin < zmax | @ line | corrected at load | pair | reset removed |
| autoxmin < autoxmax, autoymin < autoymax | @ line | AUTO's form | pair | none |
| parmin..parmax, normmin..normmax, ds, dsmin, dsmax, ntst.. | AUTO | auto_settings' table | W494 part 1 | removed in part 1 |
| picture_font_size | @ line | positive | same | none |
| line.width, line.marker_size | @ line | own parser (above 0, at least 0) | same, in the parser | none |
| poincare_plane | @ line | any | any (a value); the section variable, sign and map are choices | none |
| phi, theta, specwin, histcol.., colorlo/hi, colorize, dfdraw, seed, stoch, autoeval, postprocess | @ line | any | any: a view angle or a count of kinds is a choice or an unrestricted number; a choice that the program reads as a number is W381's (#443) | none |

## What is not yet through the one rule

Reported, not hidden (the card's scope was the options table and its readers):

- The range forms' `.min=1` on Steps (integrate.cpp) restate rangestep's rule in a
  second place; XPPAUT's other dialogs that read a number through `new_int` /
  `new_float` (spectrum window length, Liapunov, the cross-spectrum window, the
  sliders' ends) check as each dialog does. Each is its own card (follow-up).
- `check_val` (xpp_util.cpp) widens an equal range and swaps a reversed one; it also
  computes the window's centre and scale from values the autoscale made, which can be
  degenerate by data (a constant curve), so it stays a computation, not a
  correction of a setting; the Window dialog refuses before it.
- Plot-window settings in a `.setx` (xlo, xhi, ... of one graph) are read by
  `Take` as plain numbers; the Window dialog's pair is checked, the file's is not.
- The numbered plot rows (xlo2..xlo8, ...) have no pair.
