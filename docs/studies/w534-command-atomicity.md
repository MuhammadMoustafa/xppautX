# W534: command atomicity audit

Card [W534, #599](https://github.com/MuhammadMoustafa/xppautX/issues/599).
The audit covers every table joined by `command_registry.cpp::TABLES`, its
help handler, and the front end handlers dispatched by `json_registry.cpp`.
The boundary is the start of a computation: an integration, search, fit,
AUTO continuation or evaluation that has begun retains its own stop semantics.

## Handlers with checks after changes

- `run.configure` and `bvp.configure` (`configure_numerics`): the undo stack
  changed before validation; method extras were checked after main settings
  were applied. Check the proposed pair rules and extras against the proposed
  method first, then remember and apply the edit.
- `run.range`, `run.double_range`, `analysis.lyapunov_exponent` and
  `array_plot.range`: range settings were stored before resolving the retained
  range names; array range output was begun before that check. Resolve a local
  range and check whether trajectory output is available first.
- `analysis.stochastic_integrate`: the trial count and old stochastic results
  changed before range-name and trajectory-output checks.
- `run.initial`, `run.last`, `run.old`, `run.backward`, `run.shift`,
  `run.range`, `run.double_range`, `ic.at`, `ic.mouse_stream`: their common run
  entry reset run/DAE state before refusing unsuitable data or a missing prior
  solution. `run.shoot` also set initial data before checking output mode.
- `bvp.solve`, `bvp.solve_periodic`, `bvp.solve_range`: common setup changed
  browser and boundary compilation state before later checks. Range settings
  were also stored before those checks. Compile into local bytecode and check
  prerequisites before beginning the solve and keeping range settings. Remove
  the obsolete boundary-compiled flag and its callbacks: compilation already
  occurred on every solve, and the staged compiler leaves no use for the flag.
- `analysis.histogram`, `analysis.histogram_2d`, `analysis.autocorrelation`,
  `analysis.cross_correlation`, `analysis.spectral_density`: settings changed
  before checking the data, ranges, condition, lag count or window length.
  Their existing owners now take proposed settings and retain them after checks.
- `analysis.fourier_series`, `analysis.power_spectrum`, `analysis.statistics`:
  the chosen column changed before the minimum-data check.
- `analysis.fit_curve`: settings and parsed lists changed before list and file
  validation. The fit validates its local settings and reads the file once
  before keeping settings and starting iterations.
- `analysis.adjoint_interaction`: earlier coupling strings changed before a
  later invalid coupling name or missing adjoint was refused. Check all names
  and compile all couplings before keeping settings or replacing the old H.
- `auto.run`: grabbing a label/type or settling preceded validation of the
  requested start and temporary numerics. Resolve prospective point data and
  check the request/settings first; only then grab or settle and run.
- `auto.set_display`: target lookup selected a view before checking ranges.
- `auto.export_pdf`, `auto.export_svg`: target lookup selected a view before
  checking diagram points and opening the output. Activation follows those checks.
- `plot.export_pdf`, `plot.export_svg`, `array_plot.export_pdf`: export settings
  or labels changed before the output file was accepted. Keep settings only
  after the Writer commits.
- `equilibrium.export`: the retained shoot choice changed before the export
  could be refused. Keep the choice after a successful export.
- Graph-targeting handlers also used a duplicate `ActiveGraph` guard that could
  restore a graph instead of the previously selected AUTO/tool window. They now
  share the existing guard, which preserves that selection too.
- `tool.run_source_action`: graph settings were refreshed before action
  validation and a refusal was shown separately while the registry returned success.
  Its set owner now validates first and returns the error to the command.
- `kinescope.export_frames`: a later frame output could be refused after earlier
  files were written. Open every Writer and encode every frame before committing.
- Recording completion and immediate parameter edits recorded refused commands
  and consumed their pending note. A refusal before computation now keeps both
  the step count and the note; a computation that ran retains its recorded stop.
- `recording.set_step_note` edited the live note before validating/writing the
  recording, then restored it on error. It now serializes a candidate and keeps
  the edit only after Writer commit.

## Remaining tables

Window, annotation, plot geometry/curves/markers, equilibrium configuration,
nullcline, direction field, IC, value, table, browser, named set, session,
recording/player, keymap, event subscriptions, tool and help handlers were read
with their called owners. Their remaining argument checks precede application:
the registry checks every typed argument, name, bound, expression and group;
owner functions check combined values before writing. File loaders and named
set saves retain their existing parse/check/apply or Writer commit pipelines.
The DAE guess command's expressions are already compiled for the entire map by
the registry before the handler applies guesses; no extra compiler pass is needed.

## Searches and choices

Searched `command_validate`, `numerics_settings`, `model_options`, `typed_values`,
`xpp_io`, `xpp_files`, `value_undo`, the Model/Session owners, and every
`commands_*.cpp`, plus the called AUTO, histogram, fit, BVP, graph, recording,
keymap and subscription owners. Reused the registry validator, option pair
checker, extras checker, formula compiler, range resolver and Writer commit.
No session rollback mechanism was added. Restoring edits after a refusal would
be fewer lines, but would still consume undo history or emit changes and could
miss state; checking candidates first is the simpler complete solution.

File inputs remain subject to the existing name, size, parsing and allocation
bounds. Fit files are checked before live settings change; no shell, process or
new path interpretation was introduced. Filesystem write failures are returned
by the existing Writer; computations retain their reported partial results.

## Validation

The original `aacca913` was built in a temporary archive of the checkout;
23 assertions in the new atomicity/kinescope cases fail against that binary.
The fixed binary passes the full servercheck (2,845 checks) and 94,061 unit
checks, with a warning-free `make -j6 xppautx test` build. The 215 example
outputs, all eight odex comparisons and the golden files are unchanged.
Webcheck passes 91 checks. The web2 build/check, 366 tests and typecheck pass;
the 15 command browser sections and Help pass in `/usr/bin/google-chrome`.
Two existing focus assertions pass only after their section rerun and are
reported as FLAKY, rather than silently counted as first-run passes.

Deadcheck, dupcheck, aliascheck, errorcheck, globalcheck, optioncheck and
findingscheck pass. Deadcode reports the usual 14 platform/window entries,
with no new entry. Full autocheck has 283 passing checks and three failing
checks from the shipped `examples/recordings/lecar_auto.recx`: its snapshot
still puts `print_eigenvalues` in numerics. The original binary reproduces
those same three failures in autocheck's recording section. This card does
not add a reader fallback or migrate that unrelated recording fixture.
