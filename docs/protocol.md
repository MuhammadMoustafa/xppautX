# xppautX protocol

A key typed on the page is resolved there, through the keymap, and sent as the registry command it
completes. A client that has no keymap of its own sends `{"cmd":"key","key":"Ctrl+R","context":""}`
(see "Keys") and the core resolves it the same way. There is no menu and no one-letter shortcut layer in
the core: every operation is a registry command, and `hello.commands` is the one list of them.

`xppautX --check model.ode` (or `.odex`) is a separate one-shot command,
not a server event stream: it prints the one JSON report documented in
[Checking a .ode](odex.md#checking-a-ode) (W75, #123). In normal model
opening, conversion findings use the existing log route (stderr, relayed
to the page's `log` event), with their place, severity and id in the text. No event fields
are added.

`xppautX --server file.ode [xppaut options]` loads the model the way `xppaut`
does and then talks line-delimited JSON: one object per line, UTF-8, on
stdin (commands, `"cmd"`) and stdout (events, `"ev"`). stderr carries the
core's own log output. The implementation is `core/ui_json.cpp` and the
`core/json_*.cpp` files it is split into (`core/ui_json_internal.h`);
`tools/servercheck.py` is a working client, the browser page (`web2/`,
served by `xppautX` itself) a full one. This is protocol 4 (W118: what core
and page both know comes in `hello` and the events, below; the page checks
the number and shows a mismatch as an error, since core and page ship
together); see "Removed in protocol 2" at the end for what protocol 1 had
besides.

The core is single-threaded. A command runs to completion, then the server
sends `state` and `idle` for commands still on the legacy table. Registry
commands retain `state` and finish with exactly one `done` instead. While a command runs the server can stop and
**ask** the client something (a choice, a prompt, a mouse click); it waits for
the matching `answer`, answers `session.get_state`, `browser.page`, `session.quit` with `save` false and the player's controls at
once, and keeps any other command for after the one that asked. See
"Commands during a command" for what reaches a running computation, and
"Action kinds" for which commands those are.

## Typed command registry (W353, stage 1)

The core registry owns `help`, window and annotation management, equilibria, nullclines and direction fields. Every graph and
AUTO view has a unique, case-sensitive name. Defaults are `graph_1`,
`graph_2`, ... and `diagram_1`, ...; user names, including spaces, are
shown and referenced exactly as written. Names are nonempty, have no
leading or trailing space or control character, and take at most 1024
UTF-8 bytes. A closed name may be reused. An unknown name is an error.

`hello.commands` includes the registry's argument types and required flags,
choice values, kind, category, menu and XPPAUT reference paths, descriptions,
result fields and events. Each registry row has `completion: "done"`.
`help` returns the complete registry in `result.commands`; `help window.rename`
returns that command. JSON and server text use the same core validator:

```text
window.create --name "phase plane" --id "create phase plane"
window.rename --window "phase plane" --name "Phase plane"
window.select --window "Phase plane"
window.set_redraw_mode --mode manual
window.set_simultaneous_plot --enabled true
window.hide --window "Phase plane"
window.show --window "Phase plane"
window.list
window.destroy --window "Phase plane"
```

Text and window names, including the client `id`, are always double-quoted
with JSON string escapes. Choices are bare words and booleans are `true`
or `false`. Positional arguments are refused except `help NAME`.
JSON remains the only form for bound calls and recordings. A client `id`
has one type: text, at most 1024 bytes; numbers, booleans and null are errors.
Prompt-answer ids remain the existing integers on non-registry commands.
Requests take at most 16384 bytes. Unknown, repeated, missing or invalid
arguments are errors with the original command/recording place; nothing
runs. Optional `window` is the session's selected window. Optional `name`
on create chooses the next graph default; help's omitted filter lists all
commands. Window creation copies the active graph. The main graph cannot
be destroyed. AUTO views are also selected, renamed and destroyed through the
registry; AUTO's existing new-view operation assigns the next diagram name.

Registry commands emit their declared data in exactly one terminal event:
`{"ev":"done","id":"create phase plane","status":"ok","result":{"window":"phase plane"}}`.
An error has `status: "error"` and the shared `error`, `file`, `line`, `col`,
`source`, `field` fields, without a second message event. A stopped request
has `status: "stopped"`. Invalid or unreadable ids are not echoed. `state`
is retained for page compatibility; there is no registry `idle`. Commands
awaiting their resource card retain `state` plus `idle`.

`window.hide` and `window.show` keep a window and everything in it (a
graph's curves, zoom, texts, arrows, markers; a diagram view's axes and
zoom); `window.list` gives every open window as `{name, kind, shown}`, and
the same list is the `windows` event. They hold for every kind: graphs and
AUTO views, and the data browser, animation and array plot, whose names are
fixed, `browser`, `animation` and `array_plot_1`, and reserved (no graph or
view takes them, and `window.rename` and `window.select` refuse them).
`window.destroy` deletes a window and what it holds (it no longer doubles as
AUTO's hide): the main graph and the data browser cannot be destroyed;
destroying an AUTO view keeps the diagram, which a new view shows again;
destroying a graph that holds texts, arrows, markers or frozen curves
asks first (a `choice` ask, answered by the page, a `--server` client or the
terminal), or takes `confirm` `true` instead. A question about the diagram
(a grab, a zoom box, a scroll) shows its hidden views. A session file keeps
the hidden graphs, views and the data browser; the animation and the array
plot are not part of a session file.

The old Makewindow menu and keys and the `click` command are removed.
`window.create`, `window.rename`, `window.select`, `window.hide`,
`window.show`, `window.destroy`, `window.list`, `window.destroy_all`, `window.send_to_bottom`, `window.set_redraw_mode` and
`window.set_simultaneous_plot` are their sole command forms. Legacy resource
commands that target a graph (`window.set_display`, annotation operations, and set
operations) take `window` as a name; numeric `win` input is refused.
Small plot events and prompts retain their event/field spellings (`win`),
with name values. AUTO diagram `view`, autoview `active`, and AUTO's axes
`view` are names too; `diagram` `views` supplies ordered `names`. Internal
slot numbers and point labels remain implementation details and data (a frozen curve's `slot` is the one the protocol gives, in `marks`).

### Argument types (W530)

Every registry argument has one type, validated once in the core before the
command runs (a failure names the argument and why, and nothing runs); a
handler receives the typed values, never the line. `hello` and `help` give
each argument's `type`, `required`, `max_length` and `choices`, and, where
the type has them: `min` / `max` (a constant) or `min_of` / `max_of` (the
name of a session quantity, such as a run's end time, read when the command
is checked), `names` (`parameter`, `variable`, `either`, `any`), `element`
and `max_items` (a list or map). A command with groups has `groups`: a list
of `{"rule", "arguments"}` objects.

| Type | JSON value | Checked |
|---|---|---|
| `integer` | a whole number | at most 15 digits, within `min` / `max` |
| `real` | a number | finite (no NaN, no infinity, nothing beyond double range), within `min` / `max` |
| `boolean` | `true` or `false` | |
| `choice` | text | one of `choices` |
| `text` | text | at most `max_length` bytes |
| `expression` | text | compiled against the open model; refused with the compiler's message |
| `name` | text | a name the model defines, of the kind `names`, in any case |
| `file` | text | a base name in the model's folder (the `/files` rules: no separators, no leading dot, no links) |
| `path` | text | not empty; accepted only from `--server`'s own input, refused from the page, HTTP, a recording and `--silent` |
| `color` | text | a CSS colour name, `"#rrggbb"`, `"#rrggbbaa"` or `"foreground"` |
| `list` | array of single values | each an `element`, at most `max_items` |
| `map` | object of single values | each key a `name` of kind `names` (any case, once), each value an `element`, at most `max_items` |
| `window`, `new_name` | text | an open window; a name a window may take |

Lists and maps hold single values: no list or map inside another. A
recording's shape check cannot know the model or the session, so it skips
what needs them (a window, a name, an expression, a session-quantity bound);
the command checks them again when it plays.

A refused command changes nothing: all arguments and the values they lead to are checked before settings, values, windows or files change (W534, #599). A refusal before computation adds no recording step and keeps its pending note; a computation that has begun reports and records its own stop.

Argument groups, declared by a command: `exactly_one` of the named arguments,
`at_most_one`, or `together` (all or none). A grouped argument is optional by
itself.

In server text each type is written as its value is in JSON, with no commas:
numbers, `true` and `false` and choices are bare words; every kind of text
(and a map's keys) is double-quoted; a list is its items between `[` and `]`
and a map is `"key" value` pairs between `{` and `}`, items separated by blanks
(the command names here only illustrate the forms):

```text
auto.run --label 3 --threads 2 --ratio 0.5 --dry true
ic.set --values {"x" 1.5 "y" -2} --note "start"
plot.add --curves ["x" "y"] --color "#ff0000" --formula "x + y"
```

The following section is generated from `hello.commands` by
`uv run --locked python tools/servercheck.py --only registry --update-registry-docs`.
The ordinary servercheck verifies it without writing.

Equilibria, nullclines and direction fields use the registry below (W522, #586). Their old key/menu
routes and `equilibrium`/`dfield` write forms are unknown. `equilibrium.guess` takes the two plotted
variable names and their data coordinates, with an optional named graph. Search bounds are a map
of variable names to exactly two finite real numbers, lower then upper; missing variables keep
the current search bounds. Optional settings keep the session's current values.
`nullcline.export` writes the current pair as segment rows (`nullcline`, `x1`, `y1`, `x2`, `y2`),
in the requested data format, or the filename extension's format. It creates no frozen side files.
With `equilibrium.export` and `shoot: true`, stored manifolds are also written as
`<file-stem>-eigenN-stable.dat` or `<file-stem>-eigenN-unstable.dat`, without filename prompts. Each branch starts at the export's own clock and is written whole, whatever `store_from` says; the session's time is left as it was (W533).
The three analysis exports accept `confirm: true` to replace an existing output; otherwise the
common overwrite question applies. They return a placed error if the output could not be written.

<!-- registry:start -->
| Command | Arguments | Kind / category | Menu | Result | Events | Description |
|---|---|---|---|---|---|---|
| `help` | `name`: optional text (max 1024 bytes) | `v` / tools | Help/Command reference | `commands`: `list[command]` | `state`, `done` | Describe registered commands, or the command named by name. |
| `window.create` | `name`: optional new_name (max 1024 bytes) | `v` / plot | Windows/New graph | `window`: `window` | `window`, `state`, `done` | Create a graph copied from the active graph. |
| `window.rename` | `window`: required window (max 1024 bytes), `name`: required new_name (max 1024 bytes) | `v` / plot | Windows/Window/Rename | `window`: `window`, `new_name`: `text` | `window.renamed`, `window`, `state`, `done` | Rename an open graph or diagram view. |
| `window.select` | `window`: required window (max 1024 bytes) | `v` / plot | Windows/Window/Select | `window`: `window` | `window`, `state`, `done` | Bring an open window to front and select it. |
| `window.hide` | `window`: optional window (max 1024 bytes) | `v` / plot | Windows/Window/Hide | `window`: `window`, `shown`: `boolean` | `state`, `done` | Hide a window: it keeps everything it holds and comes back with window.show. |
| `window.show` | `window`: optional window (max 1024 bytes) | `v` / plot | Windows/Window/Show | `window`: `window`, `shown`: `boolean` | `state`, `done` | Show a hidden window exactly as it was. |
| `window.destroy` | `window`: optional window (max 1024 bytes), `confirm`: optional boolean | `v` / plot | Windows/Make window/Destroy | `window`: `window`, `closed`: `boolean` | `window`, `state`, `done` | Destroy a window and what it holds (a graph that holds texts, arrows, markers or frozen curves needs confirm true, or the page's answer); the main graph and the data browser stay; destroying an AUTO view keeps the diagram. |
| `window.list` | none | `v` / plot | Windows/Window/List | `windows`: `list of {name, kind, shown}` | `state`, `done` | List every window with its name, kind and whether it is shown. |
| `window.destroy_all` | none | `v` / plot | Windows/Make window/Kill all | `closed_count`: `integer` | `window`, `state`, `done` | Close every extra graph. |
| `window.set_bounds` | `window`: optional window (max 1024 bytes), `x_min`: optional real, `x_max`: optional real, `y_min`: optional real, `y_max`: optional real | `v` / plot | Plot/Window/Window | `x_min`: `real`, `x_max`: `real`, `y_min`: `real`, `y_max`: `real` | `plots`, `state`, `done` | Set the view's bounds; a bound left out keeps the view's. |
| `window.zoom` | `window`: optional window (max 1024 bytes), `x_min`: required real, `x_max`: required real, `y_min`: required real, `y_max`: required real; all or none of `x_min`, `x_max`, `y_min`, `y_max` | `v` / plot | Plot/Window/Zoom In | `x_min`: `real`, `x_max`: `real`, `y_min`: `real`, `y_max`: `real` | `plots`, `state`, `done` | Zoom in: the view becomes the box. |
| `window.zoom_out` | `window`: optional window (max 1024 bytes), `factor`: optional real [at least 1], `center_x`: optional real, `center_y`: optional real, `x_min`: optional real, `x_max`: optional real, `y_min`: optional real, `y_max`: optional real; exactly one of `factor`, `x_min`; all or none of `x_min`, `x_max`, `y_min`, `y_max`; all or none of `center_x`, `center_y` | `v` / plot | Plot/Window/Zoom Out | `x_min`: `real`, `x_max`: `real`, `y_min`: `real`, `y_max`: `real` | `plots`, `state`, `done` | Zoom out: the view grows by factor about a center (the view's own), or fills the box. |
| `window.fit` | `window`: optional window (max 1024 bytes) | `v` / plot | Plot/Window/Fit | `x_min`: `real`, `x_max`: `real`, `y_min`: `real`, `y_max`: `real` | `plots`, `state`, `done` | Fit the view to the data. |
| `window.reset_default` | `window`: optional window (max 1024 bytes) | `v` / plot | Plot/Window/Default | `x_min`: `real`, `x_max`: `real`, `y_min`: `real`, `y_max`: `real` | `plots`, `state`, `done` | Give the view its default bounds. |
| `window.scroll` | `window`: optional window (max 1024 bytes), `dx`: required real, `dy`: required real | `v` / plot | Plot/Window/Scroll | `new_center`: `list of 2 real` | `plots`, `state`, `done` | Move the view by dx and dy, in plot coordinates. |
| `window.set_display` | `window`: optional window (max 1024 bytes), `x_range`: optional list [of real, at most 2], `y_range`: optional list [of real, at most 2], `show_earlier_runs`: optional boolean, `keep_replaced_run`: optional boolean | `v` / plot | Plot/Window/Set display | `x_range`: `list of 2 real, or []`, `y_range`: `list of 2 real, or []` | `plots`, `state`, `done` | Set what the page shows of a graph: the x and y ranges ([] for the window's own), the earlier runs, keeping a replaced run. |
| `window.redraw` | `window`: optional window (max 1024 bytes) | `v` / plot | Plot/Redraw plot | `window`: `window`, `redrawn`: `boolean` | `plots`, `state`, `done` | Draw a graph again from the stored data (and its nullclines, texts and frozen curves), or send an AUTO view's diagram again. |
| `window.clear` | `window`: optional window (max 1024 bytes) | `v` / plot | Plot/Clear plot | `window`: `window`, `cleared`: `boolean` | `plots`, `state`, `done` | Clear a graph: its picture and earlier runs go until the next run. |
| `window.send_to_bottom` | `window`: optional window (max 1024 bytes) | `v` / plot | Windows/Make window/Bottom | `window`: `window` | `state`, `done` | Send a graph to the bottom. |
| `window.set_redraw_mode` | `window`: optional window (max 1024 bytes), `mode`: required choice (auto, manual) | `v` / plot | Plot/Make window/Auto or Manual | `redraw_mode`: `choice` | `state`, `done` | Choose automatic or manual graph redrawing. |
| `window.set_simultaneous_plot` | `enabled`: required boolean | `v` / plot | Plot/Make window/SimPlot | `simultaneous_plot`: `boolean` | `state`, `done` | Draw on every open graph when enabled. |
| `annotation.list` | `window`: optional window (max 1024 bytes) | `v` / plot | Plot/Annotations | `window`: `window`, `text`: `list of texts`, `arrows`: `list of arrows and pointers`, `markers`: `list of markers` | `marks`, `state`, `done` | List the texts, arrows, pointers and markers of a graph. |
| `annotation.add_text` | `window`: optional window (max 1024 bytes), `text`: required text (max 1000 bytes), `x`: required real, `y`: required real, `text_size`: optional integer [at least 0, at most 4], `style`: optional choice (normal, bold, italic, bold_italic), `color`: optional color (max 1024 bytes) | `v` / plot | Plot/Annotations/Text | `id`: `integer` | `marks`, `state`, `done` | Add a text at a point of a graph. |
| `annotation.add_arrow` | `window`: optional window (max 1024 bytes), `tip_x`: required real, `tip_y`: required real, `tail_x`: required real, `tail_y`: required real, `size`: optional real [at least 0], `color`: optional color (max 1024 bytes) | `v` / plot | Plot/Annotations/Arrow | `id`: `integer` | `marks`, `state`, `done` | Add an arrow head at the tip of a line. |
| `annotation.add_pointer` | `window`: optional window (max 1024 bytes), `tip_x`: required real, `tip_y`: required real, `tail_x`: required real, `tail_y`: required real, `size`: optional real [at least 0], `color`: optional color (max 1024 bytes) | `v` / plot | Plot/Annotations/Pointer | `id`: `integer` | `marks`, `state`, `done` | Add a pointer: a head and a shaft. |
| `annotation.add_marker` | `window`: optional window (max 1024 bytes), `shape`: required choice (box, diamond, triangle, plus, cross, circle), `x`: required real, `y`: required real, `size`: optional real [at least 0], `color`: optional color (max 1024 bytes) | `v` / plot | Plot/Annotations/Marker | `id`: `integer` | `marks`, `state`, `done` | Add a marker at a point of a graph. |
| `annotation.edit` | `annotation_id`: required integer [at least 0, at most 2147483646], `text`: optional text (max 1000 bytes), `shape`: optional choice (box, diamond, triangle, plus, cross, circle), `size`: optional real [at least 0], `text_size`: optional integer [at least 0, at most 4], `style`: optional choice (normal, bold, italic, bold_italic), `color`: optional color (max 1024 bytes) | `v` / plot | Plot/Annotations/Edit/Change | `id`: `integer`, `updated`: `boolean` | `marks`, `state`, `done` | Change a text (its text, size, style, colour) or an object (its size, colour, a marker's shape). |
| `annotation.move` | `annotation_id`: required integer [at least 0, at most 2147483646], `new_x`: required real, `new_y`: required real | `v` / plot | Plot/Annotations/Edit/Move | `id`: `integer`, `moved`: `boolean` | `marks`, `state`, `done` | Move an annotation so that its position (an object's start, its end following) is the point. |
| `annotation.delete` | `annotation_id`: required integer [at least 0, at most 2147483646] | `v` / plot | Plot/Annotations/Edit/Delete | `id`: `integer`, `deleted`: `boolean` | `marks`, `state`, `done` | Delete an annotation. |
| `annotation.delete_all` | `window`: optional window (max 1024 bytes) | `v` / plot | Plot/Annotations/Delete all | `cleared`: `boolean` | `marks`, `state`, `done` | Delete every annotation of a graph. |
| `run.initial` | none | `x` / run | Run/Run from initial | `status`: `choice`, `steps`: `integer`, `time`: `real` | `computing`, `progress`, `series`, `stopped`, `state`, `done` | Run from the initial values. |
| `run.last` | `reset_integrals`: optional boolean | `x` / run | Run/Run from last state | `status`: `choice`, `steps`: `integer`, `time`: `real` | `computing`, `progress`, `series`, `stopped`, `state`, `done` | Make the last state the initial values and run from it. |
| `run.old` | none | `x` / run | Run/Run from stored initial | `status`: `choice`, `steps`: `integer`, `time`: `real` | `computing`, `progress`, `series`, `stopped`, `state`, `done` | Run from the initial values as they stand, without refreshing the DAE guesses first. |
| `run.continue` | `until`: optional real, `extra`: optional real; at most one of `until`, `extra` | `x` / run | Run/Continue | `status`: `choice`, `end_time`: `real` | `computing`, `progress`, `series`, `stopped`, `state`, `done` | Extend the trajectory to an end time, by a duration, or by the run's total time. |
| `run.steady` | `decimals`: required integer [at least 0, at most 15], `hold`: required real, `maximum`: required real | `x` / run | Run/Steady | `status`: `choice`, `decimals`: `integer`, `time`: `real` | `computing`, `progress`, `series`, `stopped`, `state`, `done` | Run from the initial values until every state stops changing at the chosen decimals. |
| `run.backward` | none | `x` / run | Run/Run backward | `status`: `choice`, `steps`: `integer`, `time`: `real` | `computing`, `progress`, `series`, `stopped`, `state`, `done` | Run from the initial values backward in time. |
| `run.shift` | `reset_integrals`: optional boolean | `x` / run | Run/Initialconds/Shift | `status`: `choice`, `steps`: `integer`, `time`: `real` | `computing`, `progress`, `series`, `stopped`, `state`, `done` | Run from the last state with the clock going on from where it stopped. |
| `run.range` | `variable`: optional name (max 1024 bytes) [of either], `start`: optional real, `end`: optional real, `steps`: optional integer [at least 1], `reset_storage`: optional boolean, `use_old_ic`: optional boolean, `cycle_color`: optional boolean, `movie`: optional boolean, `write_each_run_to_file`: optional boolean | `x` / run | Run/Initialconds/Range | `runs_completed`: `integer`, `total_steps`: `integer` | `computing`, `progress`, `series`, `stopped`, `state`, `done` | Run once for each value of a parameter or variable between two values. |
| `run.double_range` | `variable1`: optional name (max 1024 bytes) [of either], `start1`: optional real, `end1`: optional real, `variable2`: optional name (max 1024 bytes) [of either], `start2`: optional real, `end2`: optional real, `steps1`: optional integer [at least 1], `steps2`: optional integer [at least 1], `vary`: optional choice (together, independently), `reset_storage`: optional boolean, `use_old_ic`: optional boolean, `cycle_color`: optional boolean, `movie`: optional boolean, `write_each_run_to_file`: optional boolean | `x` / run | Run/Initialconds/2par range | `grid_points`: `integer`, `total_steps`: `integer` | `computing`, `progress`, `series`, `stopped`, `state`, `done` | Run once for each pair of values of two parameters or variables. |
| `run.dae_guess` | `guesses`: optional map [of any, of expression, at most 256] | `s` / run | Run/Initialconds/DAE guess | `guesses`: `map[name, real]` | `state`, `done` | Set the initial guesses of the algebraic variables; none given keeps them. |
| `run.abort` | none | `c` / run | Run/Stop | `stopped_at`: `real` | `stopped`, `done` | Stop the running command where it is; the stopped command ends with the status stopped. |
| `run.configure` | `total_time`: optional real, `start_time`: optional real, `store_from`: optional real, `dt`: optional real, `method`: optional text (max 1024 bytes), `store_every`: optional integer [at least 1], `state_limit`: optional real, `stop_at_limit`: optional boolean, `delay`: optional real, `relative_tolerance`: optional real, `absolute_tolerance`: optional real, `min_step`: optional real, `max_step`: optional real, `eul_tol`: optional real, `eul_iter`: optional integer [at least 1], `vmaxpts`: optional integer [at least 1, at most 10000000], `autoeval`: optional boolean, `bandlo`: optional integer [at least 0, at most 100000], `bandup`: optional integer [at least 0, at most 100000], `delay_real_guess`: optional real, `delay_imaginary_guess`: optional real, `delay_grid`: optional integer [at least 1] | `s` / run | Run/Settings | `settings`: `map[name, value]` | `numerics`, `state`, `done` | Set the integration's numerics (run length, step, method, tolerances, bounds, delay); the settings are checked together. |
| `run.shoot` | `manifold_index`: required integer [at least 1] | `x` / run | Run/Initialconds/sHoot | `status`: `choice`, `steps`: `integer`, `time`: `real` | `computing`, `progress`, `series`, `stopped`, `state`, `done` | Run from a manifold the equilibria's shooting found. |
| `ic.set` | `values`: required map [of variable, of real, at most 256] | `s` / run | Run/Initial conditions/New | `applied`: `map[name, real]` | `state`, `done` | Set the initial values of some states; the others keep theirs. |
| `ic.at` | `coordinates`: required map [of variable, of real, at most 256], `window`: optional window (max 1024 bytes) | `x` / run | Run/Initial conditions/Mouse | `applied`: `map[name, real]` | `computing`, `progress`, `series`, `stopped`, `state`, `done` | Run from a point of a graph: the two states it plots take the coordinates, the others their initial values. |
| `ic.mouse_stream` | `x`: required list [of real, at most 256], `y`: required list [of real, at most 256], `window`: optional window (max 1024 bytes) | `x` / run | Run/Initial conditions/mIce | `trajectories_run`: `integer` | `computing`, `progress`, `series`, `stopped`, `state`, `done` | Run from each point of a graph in turn, x and y the coordinates of the two states it plots; the runs add up on the graph. |
| `ic.load` | `file`: required file (max 1024 bytes) | `s` / run | Run/Initial conditions/File | `file`: `file`, `applied`: `map[name, real]` | `state`, `done` | Read the initial values from a file of the model's folder, one number per differential equation. |
| `ic.formula` | `variable`: required text (max 1024 bytes), `formula`: required text (max 1024 bytes) | `s` / run | Run/Initial conditions/formUla | `variable`: `text`, `formula`: `text` | `state`, `done` | Set a state's initial value to a formula, or an array range u[1..5] to a formula reading [j] as the element's number. |
| `ic.from_data_row` | `row`: required integer [at least 0] | `s` / run | Run/Data/Get | `row`: `integer`, `values`: `map[name, real]` | `state`, `done` | Make a row of the stored data (the first is 0) the initial values. |
| `ic.array` | `name`: required name (max 1024 bytes) [of variable], `size`: required integer [at least 1], `formula`: required expression (max 1024 bytes) | `s` / run | Run/Initial conditions/formUla | `name`: `name`, `size`: `integer` | `state`, `done` | Set size states from a state on to a formula of t, the element's offset from the first. |
| `bvp.solve_range` | `parameter`: optional name (max 1024 bytes) [of parameter], `start`: optional real, `end`: optional real, `steps`: optional integer [at least 1], `cycle_color`: optional boolean, `side`: optional choice (start, end), `movie`: optional boolean | `x` / analysis | Run/Boundary values/Range | `solutions_found`: `integer` | `computing`, `progress`, `series`, `stopped`, `state`, `done` | Solve the boundary value problem for each value of a parameter between two values, storing a solution's start or end for each. |
| `bvp.solve` | `show`: required boolean | `x` / analysis | Run/Boundary values/No show, Show | `converged`: `boolean` | `computing`, `progress`, `series`, `stopped`, `state`, `done` | Solve the boundary value problem from the initial values by shooting and make the solution the initial values; show draws every step. |
| `bvp.solve_periodic` | `frequency_parameter`: required name (max 1024 bytes) [of parameter], `section_variable`: required name (max 1024 bytes) [of variable], `section`: required real, `show`: required boolean | `x` / analysis | Run/Boundary values/Periodic | `converged`: `boolean`, `period`: `real` | `computing`, `progress`, `series`, `stopped`, `state`, `done` | Solve for a periodic orbit whose period is the frequency parameter, held to a section of one variable. |
| `bvp.configure` | `max_iterates`: optional integer [at least 1], `tolerance`: optional real, `epsilon`: optional real | `s` / analysis | Run/Numerics/bndVal | `settings`: `map[name, value]` | `numerics`, `state`, `done` | Set the shooting's iteration limit, tolerance and finite-difference step; the settings are checked together. |
| `auto.open` | none | `v` / analysis | AUTO/Open AUTO | `open`: `boolean` | `window`, `diagram`, `state`, `done` | Open the AUTO window, the bifurcation package. |
| `auto.run` | `kind`: required choice (equilibrium, periodic, two_parameter, fixed_period, bvp, homoclinic, heteroclinic), `switch`: optional boolean, `label`: optional integer [at least 1], `type`: optional text (max 1024 bytes), `index`: optional integer [at least 1], `decimals`: optional integer [at least 0], `hold`: optional real [at least 0], `maximum`: optional real [at least 0], `replace_diagram`: optional boolean, `unstable_dimension`: optional integer [at least 0], `stable_dimension`: optional integer [at least 0], `left_equilibrium`: optional list [of real, at most 256], `right_equilibrium`: optional list [of real, at most 256], `mesh_intervals`: optional integer, `max_steps`: optional integer, `output_every`: optional integer, `collocation_points`: optional integer, `step_size`: optional real, `min_step_size`: optional real, `max_step_size`: optional real, `parameter_min`: optional real, `parameter_max`: optional real, `norm_min`: optional real, `norm_max`: optional real, `parameter_tolerance`: optional real, `state_tolerance`: optional real, `special_point_tolerance`: optional real, `mesh_adapt_every`: optional integer, `max_branch_switches`: optional integer, `switch_one_direction`: optional boolean, `output_detail`: optional choice (none, little, normal, start_jacobian, extensive, debugging), `max_location_iterations`: optional integer, `max_newton_iterations`: optional integer, `full_newton_after`: optional integer, `step_adapt_every`: optional integer, `suppress_branch_points`: optional boolean; at most one of `label`, `type`, `decimals`; all or none of `type`, `index`; all or none of `decimals`, `hold`, `maximum` | `x` / analysis | AUTO/Run | `points_computed`: `integer`, `branches_tracked`: `integer` | `diagram`, `autoinfo`, `autosettings`, `state`, `done` | Run AUTO from the current state or a stored point: its kind (what is computed), switch for a branch switch, its start (a label, a point type and index, or the current state, optionally settled first), the settings of this run only (ds's sign is the direction) and the equilibria of a homoclinic start. The start point's type decides which kinds fit it: hello's auto_run table, and autoinfo's start. |
| `auto.grab` | `label`: optional integer [at least 1], `type`: optional text (max 1024 bytes), `index`: optional integer [at least 1]; at most one of `label`, `type`; all or none of `type`, `index` | `d` / analysis | AUTO/Grab | `grabbed`: `boolean`, `label`: `integer`, `type`: `text`, `branch`: `integer`, `point`: `integer` | `autoinfo`, `diagram`, `state`, `done` | Grab a stored point by label or type and index; omit both to pick it on the diagram. |
| `auto.configure_numerics` | `mesh_intervals`: optional integer, `max_steps`: optional integer, `output_every`: optional integer, `collocation_points`: optional integer, `step_size`: optional real, `min_step_size`: optional real, `max_step_size`: optional real, `parameter_min`: optional real, `parameter_max`: optional real, `norm_min`: optional real, `norm_max`: optional real, `parameter_tolerance`: optional real, `state_tolerance`: optional real, `special_point_tolerance`: optional real, `mesh_adapt_every`: optional integer, `max_branch_switches`: optional integer, `switch_one_direction`: optional boolean, `output_detail`: optional choice (none, little, normal, start_jacobian, extensive, debugging), `max_location_iterations`: optional integer, `max_newton_iterations`: optional integer, `full_newton_after`: optional integer, `step_adapt_every`: optional integer, `suppress_branch_points`: optional boolean, `special_points`: optional map [of boolean, at most 256] | `s` / analysis | AUTO/Numerics | `settings_updated`: `boolean` | `autosettings`, `state`, `done` | Set AUTO's Numerics by their names (mesh_intervals, step_size, ...) and the codimension-two points it detects (special_points), all or nothing. |
| `auto.configure_plot` | `plot_type`: optional choice (maximum, norm, maximum_and_minimum, period, two_parameter, frequency, average), `variable`: optional name (max 1024 bytes) [of variable], `parameter1`: optional name (max 1024 bytes) [of parameter], `parameter2`: optional name (max 1024 bytes) [of parameter], `x_min`: optional real, `x_max`: optional real, `y_min`: optional real, `y_max`: optional real, `view`: optional window (max 1024 bytes), `fit`: optional boolean | `s` / analysis | AUTO/Axes | `plot_configured`: `boolean` | `autosettings`, `state`, `done` | Set what a view of the diagram plots: the plot type, the variable, the two parameters and the ranges, optionally fitted afterwards. |
| `auto.configure_marks` | `parameters`: required list [of text, at most 256], `values`: required list [of real, at most 256] | `s` / analysis | AUTO/Usr period | `marks_count`: `integer` | `autosettings`, `state`, `done` | Set the Mark values: the parameters (or T, the period) and the values at which AUTO labels a point. |
| `auto.set_threads` | `threads`: optional integer [at least 1, at most the session's threads_max], `automatic`: optional boolean; exactly one of `threads`, `automatic` | `s` / analysis | AUTO/Numerics | `threads`: `integer`, `automatic`: `boolean` | `autosettings`, `state`, `done` | Set AUTO's thread count, a preference of the program: a count, or automatic. |
| `auto.hide_earlier` | none | `v` / analysis | AUTO/Clear | `hidden_points`: `integer` | `diagram`, `autoview`, `state`, `done` | Hide the branches so far: new runs draw alone until set_display shows them again. |
| `auto.redraw` | none | `v` / analysis | AUTO/Redraw | `redrawn`: `boolean` | `diagram`, `autoview`, `state`, `done` | Draw the diagram again. |
| `auto.pick` | `x`: required real, `y`: required real, `view`: optional window (max 1024 bytes) | `v` / analysis | AUTO/AUTO diagram/Click | `x`: `real`, `y`: `real` | `autoinfo`, `diagram`, `state`, `done` | A point of the diagram in its plotted quantities: shown, and stored for select_2parameter_point in a two-parameter plot. |
| `auto.set_display` | `view`: optional window (max 1024 bytes), `x_range`: optional list [of real, at most 2], `y_range`: optional list [of real, at most 2], `show_earlier`: optional boolean | `v` / analysis | AUTO/Auto/Set display | `x_range`: `list`, `y_range`: `list` | `diagram`, `autoview`, `state`, `done` | Set what the page shows of a view: its zoom (a [low, high] range, [] for the whole range of an axis) and whether the hidden branches are drawn. |
| `auto.new_view` | none | `v` / analysis | Windows/New AUTO view | `window`: `window`, `x_range`: `list`, `y_range`: `list` | `diagram`, `autoview`, `state`, `done` | Add a view of the diagram, with the axes of the active one; it becomes the active view. |
| `auto.use_last_plot` | `parameters`: required choice (one, two), `view`: optional window (max 1024 bytes) | `v` / analysis | AUTO/Axes/Last plot | `plot`: `choice` | `diagram`, `autoview`, `state`, `done` | Go back to the last one-parameter or two-parameter plot's axes. |
| `auto.scroll` | `dx`: required real, `dy`: required real, `view`: optional window (max 1024 bytes) | `v` / analysis | AUTO/Axes/Scroll | `window`: `window`, `x_range`: `list`, `y_range`: `list` | `diagram`, `autoview`, `state`, `done` | Move the view's visible ranges by dx and dy, in the plotted quantities. |
| `auto.default_view` | `view`: optional window (max 1024 bytes) | `v` / analysis | AUTO/Axes/Default | `window`: `window`, `x_range`: `list`, `y_range`: `list` | `diagram`, `autoview`, `state`, `done` | Give the view the model's own diagram ranges. |
| `auto.fit` | `view`: optional window (max 1024 bytes) | `v` / analysis | AUTO/Axes/Fit | `window`: `window`, `x_range`: `list`, `y_range`: `list` | `diagram`, `autoview`, `state`, `done` | Fit the view to the diagram. |
| `auto.zoom` | `x_min`: required real, `x_max`: required real, `y_min`: required real, `y_max`: required real, `view`: optional window (max 1024 bytes) | `v` / analysis | AUTO/Axes/Zoom in | `window`: `window`, `x_range`: `list`, `y_range`: `list` | `diagram`, `autoview`, `state`, `done` | Zoom the view in to a box of its plotted quantities. |
| `auto.zoom_out` | `x_min`: required real, `x_max`: required real, `y_min`: required real, `y_max`: required real, `view`: optional window (max 1024 bytes) | `v` / analysis | AUTO/Axes/Zoom out | `window`: `window`, `x_range`: `list`, `y_range`: `list` | `diagram`, `autoview`, `state`, `done` | Zoom the view out so that what it shows now fills a box of its plotted quantities. |
| `auto.clear_grab` | none | `d` / analysis | AUTO/File/Clear grab | `cleared`: `boolean` | `autoinfo`, `diagram`, `state`, `done` | Forget the grabbed point: a following run starts from the current state. |
| `auto.load_orbit_into_browser` | `label`: optional integer [at least 1] | `d` / analysis | AUTO/File/Import orbit | `orbit_loaded`: `boolean`, `rows`: `integer` | `state`, `done` | Load the stored orbit of a labelled point (the grabbed one without label) into the data browser. |
| `auto.save_session` | `file`: optional file (max 1024 bytes) | `d` / analysis | File/AUTO/Save diagram | `file`: `file`, `saved`: `boolean` | `state`, `done` | Save the session, with its diagram and AUTO's settings (.snapx). |
| `auto.open_session` | `file`: required file (max 1024 bytes) | `d` / analysis | File/AUTO/Open diagram | `file`: `file`, `loaded`: `boolean` | `diagram`, `autoinfo`, `autosettings`, `state`, `done` | Open a saved session (.snapx), with its AUTO diagram. |
| `auto.export_pdf` | `file`: optional file (max 1024 bytes), `window`: optional window (max 1024 bytes) | `d` / analysis | File/AUTO/PDF | `file`: `file` | `state`, `done` | Write a view of the diagram as a PDF picture. |
| `auto.export_svg` | `file`: optional file (max 1024 bytes), `window`: optional window (max 1024 bytes) | `d` / analysis | File/AUTO/SVG | `file`: `file` | `state`, `done` | Write a view of the diagram as an SVG picture. |
| `auto.reset_all` | none | `d` / analysis | AUTO/File/Reset diagram | `reset`: `boolean` | `diagram`, `autoview`, `state`, `done` | Destroy the diagram and AUTO's files. |
| `auto.export_points` | `file`: optional file (max 1024 bytes) | `d` / analysis | File/AUTO/Write pts | `file`: `file`, `points_written`: `integer` | `state`, `done` | Write the diagram's points as the view plots them. |
| `auto.export_all_info` | `file`: optional file (max 1024 bytes) | `d` / analysis | File/AUTO/All info | `file`: `file`, `written`: `integer` | `state`, `done` | Write every point with its parameters, extremes and eigenvalues. |
| `auto.export_initial_data` | `file`: optional file (max 1024 bytes) | `d` / analysis | File/AUTO/init Data | `file`: `file`, `written`: `integer` | `state`, `done` | Write every point's main parameter and solution as initial data. |
| `auto.set_auto_redraw` | `enabled`: required boolean | `v` / analysis | AUTO/File/Toggle redraw | `auto_redraw`: `boolean` | `state`, `done` | Redraw the diagram automatically when enabled. |
| `auto.range_branch` | `branch`: required integer [at least 1], `first_point`: required integer [at least 1], `last_point`: required integer [at least 1] | `x` / analysis | AUTO/File/auto raNge | `runs_completed`: `integer` | `state`, `done` | Run the sweep over a stretch of a branch (its points first_point to last_point), once for each. |
| `auto.select_2parameter_point` | `x`: optional real, `y`: optional real; all or none of `x`, `y` | `d` / analysis | AUTO/File/sElect 2par pt | `selected`: `boolean` | `state`, `done` | Set the two parameters to the point picked on a two-parameter plot, or to x and y. |
| `auto.draw_labeled_orbits` | `mode`: required choice (off, draw, clear_and_draw) | `d` / analysis | AUTO/File/draw laBled | `orbits_drawn`: `choice` | `state`, `done` | Draw the orbits of the labelled points when grabbed: off, draw (no erase) or clear_and_draw. |
| `auto.load_branch_into_browser` | `branch`: required integer [at least 1], `first_point`: required integer [at least 1], `last_point`: required integer [at least 1] | `d` / analysis | AUTO/File/lOad branch | `rows_loaded`: `integer` | `state`, `done` | Load a stretch of a branch (its points first_point to last_point) into the data browser. |
| `auto.export_csv` | `file`: optional file (max 1024 bytes) | `d` / analysis | File/AUTO/eXport CSV | `file`: `file`, `points_written`: `integer` | `state`, `done` | Write the diagram and its eigenvalues as two CSV files. |
| `auto.import_diagram` | `file`: required file (max 1024 bytes) | `d` / analysis | AUTO/File/Import diagram | `file`: `file`, `points_imported`: `integer` | `diagram`, `autoinfo`, `autosettings`, `state`, `done` | Import an XPPAUT diagram (.auto) into the open model. |
| `plot.add_curve` | `window`: optional window (max 1024 bytes), `x`: required name (max 1024 bytes) [of column], `y`: required name (max 1024 bytes) [of column], `z`: optional name (max 1024 bytes) [of column], `color`: optional color (max 1024 bytes), `draw`: optional choice (line, points, both), `dash`: optional choice (solid, dashed, dotted, dash_dot), `width`: optional real, `marker`: optional choice (circle, square, triangle, diamond, cross, plus, star), `marker_size`: optional real [at least 0] | `v` / plot | Plot/Curves/Add curve | `curve_index`: `integer` | `plots`, `series`, `state`, `done` | Add a curve to a graph: the data columns it plots (x, y and, in 3D, z) and its colour and line style; what is left out is the first curve's. |
| `plot.delete_curve` | `window`: optional window (max 1024 bytes), `curve_index`: optional integer [at least 0] | `v` / plot | Plot/Curves/Delete last | `remaining_curves`: `integer` | `plots`, `state`, `done` | Delete a curve of a graph (the last one when curve_index is left out); the first curve is the graph's own and stays. |
| `plot.delete_all_curves` | `window`: optional window (max 1024 bytes) | `v` / plot | Plot/Curves/Remove all | `cleared`: `boolean` | `plots`, `state`, `done` | Delete every curve of a graph but the first. |
| `plot.edit_curve` | `window`: optional window (max 1024 bytes), `curve_index`: required integer [at least 0], `x`: optional name (max 1024 bytes) [of column], `y`: optional name (max 1024 bytes) [of column], `z`: optional name (max 1024 bytes) [of column], `color`: optional color (max 1024 bytes), `draw`: optional choice (line, points, both), `dash`: optional choice (solid, dashed, dotted, dash_dot), `width`: optional real, `marker`: optional choice (circle, square, triangle, diamond, cross, plus, star), `marker_size`: optional real [at least 0] | `v` / plot | Plot/Curves/Edit curve | `curve_index`: `integer`, `updated`: `boolean` | `plots`, `state`, `done` | Change a curve of a graph: its data columns, colour and line style; what is left out stays. |
| `plot.freeze_curve` | `window`: optional window (max 1024 bytes), `name`: optional text (max 9 bytes), `key`: optional text (max 19 bytes), `color`: optional color (max 1024 bytes), `draw`: optional choice (line, points, both), `dash`: optional choice (solid, dashed, dotted, dash_dot), `width`: optional real, `marker`: optional choice (circle, square, triangle, diamond, cross, plus, star), `marker_size`: optional real [at least 0] | `v` / plot | Plot/Freeze/Freeze | `slot`: `integer`, `name`: `text` | `plots`, `series`, `state`, `done` | Freeze the graph's first curve as it is now into a frozen curve of its own, with a legend key, a name, a colour and a line style. |
| `plot.delete_frozen` | `window`: optional window (max 1024 bytes), `slot`: required integer [at least 0] | `v` / plot | Plot/Freeze/Delete | `slot`: `integer`, `deleted`: `boolean` | `plots`, `state`, `done` | Delete one of the graph's frozen curves, by its slot (freeze_curve and the marks event give it). |
| `plot.edit_frozen` | `window`: optional window (max 1024 bytes), `slot`: required integer [at least 0], `name`: optional text (max 9 bytes), `key`: optional text (max 19 bytes), `color`: optional color (max 1024 bytes), `draw`: optional choice (line, points, both), `dash`: optional choice (solid, dashed, dotted, dash_dot), `width`: optional real, `marker`: optional choice (circle, square, triangle, diamond, cross, plus, star), `marker_size`: optional real [at least 0] | `v` / plot | Plot/Freeze/Edit | `slot`: `integer`, `updated`: `boolean` | `plots`, `state`, `done` | Change one of the graph's frozen curves: its legend key, name, colour and line style; what is left out stays. |
| `plot.delete_all_frozen` | `window`: optional window (max 1024 bytes) | `v` / plot | Plot/Freeze/Remove all | `cleared`: `boolean` | `plots`, `state`, `done` | Delete every frozen curve of a graph. |
| `plot.set_frozen_legend` | `window`: optional window (max 1024 bytes), `visible`: required boolean, `x`: optional real, `y`: optional real; all or none of `x`, `y` | `v` / plot | Plot/Freeze/Key | `legend_visible`: `boolean` | `plots`, `state`, `done` | Show or hide the frozen curves' legend; x and y, the plot coordinates of its first line, move it. |
| `plot.import_bifurcation` | `window`: optional window (max 1024 bytes), `file`: required file (max 1024 bytes) | `d` / plot | Plot/Freeze/Bif.Diag | `file`: `file`, `curves_imported`: `integer` | `plots`, `series`, `state`, `done` | Show an AUTO Write pts file of the model's folder (six columns, checked whole first) in the graph as curves by kind. |
| `plot.clear_bifurcation` | `window`: optional window (max 1024 bytes) | `v` / plot | Plot/Freeze/Clr. BD | `cleared`: `boolean` | `plots`, `state`, `done` | Remove the imported bifurcation diagram from the graph. |
| `plot.set_auto_freeze` | `enabled`: required boolean | `s` / plot | Plot/Freeze/On freeze | `auto_freeze`: `boolean` | `state`, `done` | Freeze the first curve of a graph after every run. |
| `plot.export_pdf` | `window`: optional window (max 1024 bytes), `file`: required file (max 1024 bytes), `color`: optional boolean, `axes_font_size`: optional real [at least 1, at most 100] | `d` / plot | File/Curves/PDF | `file`: `file`, `bytes_written`: `integer` | `state`, `done` | Write the graph as a PDF picture into a file of the model's folder; colour and axes_font_size are the session's when left out and stay. |
| `plot.export_svg` | `window`: optional window (max 1024 bytes), `file`: required file (max 1024 bytes), `color`: optional boolean, `axes_font_size`: optional real [at least 1, at most 100] | `d` / plot | File/Curves/SVG | `file`: `file`, `bytes_written`: `integer` | `state`, `done` | Write the graph as an SVG picture into a file of the model's folder; colour and axes_font_size are the session's when left out and stay. |
| `plot.export_data` | `window`: optional window (max 1024 bytes), `file`: required file (max 1024 bytes), `format`: optional text (max 1024 bytes) | `d` / plot | File/Curves/export data | `file`: `file`, `rows_written`: `integer` | `state`, `done` | Write what the graph shows (its curves and frozen curves, as the columns curve, x, y and z in 3D) into a file of the model's folder, in the data format named by format (dat, csv, csv.gz, npz) or by the file's extension. |
| `plot.set_axes_origin` | `window`: optional window (max 1024 bytes), `x_origin`: optional real, `y_origin`: optional real, `z_origin`: optional real, `x_origin_shown`: optional boolean, `y_origin_shown`: optional boolean, `z_origin_shown`: optional boolean, `axes_font_size`: optional real [at least 1, at most 100] | `v` / plot | Plot/Curves/axes opts | `origin_updated`: `boolean` | `plots`, `state`, `done` | Set where the graph's axes cross, whether each axis is drawn there, and the font size of the axes' text; what is left out stays. |
| `plot.set_colormap` | `colormap`: required choice (jet, periodic, hot, cool, blue_red, gray, cubehelix, viridis, plasma, inferno, magma, cividis, coolwarm, rdbu, seismic, twilight, hsv, turbo, greys, blues, reds, greens, oranges, purples) | `s` / plot | Plot/Curves/Colormap | `colormap`: `choice` | `plots`, `state`, `done` | Set the session's colour scale, which the array plot, the direction field and the pictures use. |
| `plot.set_2d` | `window`: optional window (max 1024 bytes), `x`: optional name (max 1024 bytes) [of column], `y`: optional name (max 1024 bytes) [of column], `x_min`: optional real, `x_max`: optional real, `y_min`: optional real, `y_max`: optional real, `x_label`: optional text (max 1024 bytes), `y_label`: optional text (max 1024 bytes) | `v` / plot | Plot/Axes/2D | `axes`: `choice`, `x`: `name`, `y`: `name` | `plots`, `state`, `done` | Make the graph a 2D plot of two data columns with these limits and labels; what is left out stays. |
| `plot.set_3d` | `window`: optional window (max 1024 bytes), `x`: optional name (max 1024 bytes) [of column], `y`: optional name (max 1024 bytes) [of column], `z`: optional name (max 1024 bytes) [of column], `x_min`: optional real, `x_max`: optional real, `y_min`: optional real, `y_max`: optional real, `z_min`: optional real, `z_max`: optional real, `x_label`: optional text (max 1024 bytes), `y_label`: optional text (max 1024 bytes), `z_label`: optional text (max 1024 bytes), `window_x_min`: optional real, `window_x_max`: optional real, `window_y_min`: optional real, `window_y_max`: optional real | `v` / plot | Plot/Axes/3D | `axes`: `choice`, `x`: `name`, `y`: `name`, `z`: `name` | `plots`, `state`, `done` | Make the graph a 3D plot of three data columns with these limits, labels and window on the screen; what is left out stays. |
| `plot.set_3d_view` | `window`: optional window (max 1024 bytes), `perspective`: optional boolean, `z_plane`: optional real, `z_view`: optional real, `theta`: optional real, `phi`: optional real, `movie`: optional boolean, `vary`: optional choice (theta, phi), `start_angle`: optional real, `increment`: optional real, `increments`: optional integer [at least 1] | `v` / plot | Plot/3D view | `view_updated`: `boolean` | `plots`, `state`, `done` | Set a 3D graph's perspective, planes and angles; movie true films the rotation as the angle named by vary goes from start_angle in increments steps of increment. |
| `plot.set_shifted_axes` | `window`: optional window (max 1024 bytes), `x_shift`: optional integer [at least 0], `y_shift`: optional integer [at least 0], `z_shift`: optional integer [at least 0] | `s` / analysis | Plot/Numerics/rUelle plot | `x_shift`: `integer`, `y_shift`: `integer`, `z_shift`: `integer` | `plots`, `state`, `done` | Plot each axis of the graph that many rows behind (a Ruelle-Takens plot of delayed values); what is left out stays. |
| `plot.variable_vs_time` | `window`: optional window (max 1024 bytes), `variable`: required name (max 1024 bytes) [of column] | `v` / plot | Plot/Variable vs time | `x`: `name`, `y`: `name` | `plots`, `state`, `done` | Plot a data column against time in the graph, its view fitted to the data. |
| `plot.add_data_markers` | `window`: optional window (max 1024 bytes), `shape`: optional choice (box, diamond, triangle, plus, cross, circle), `color`: optional color (max 1024 bytes), `size`: optional real [at least 0], `count`: optional integer [at least 1], `first_row`: optional integer [at least 0], `stride`: optional integer [at least 1] | `v` / plot | Plot/Text,etc/marker(S) | `count`: `integer` | `marks`, `state`, `done` | Put count markers on the graph's first curve, every stride-th row from first_row; what is left out is the last Add markers' setting. |
| `plot.set_variables` | `window`: optional window (max 1024 bytes), `how`: required choice (time_series, phase_plane, array_plot), `names`: required list [of variable, of name, at most 256] | `v` / plot | Plot/Set variables | `how`: `choice`, `names`: `list of name` | `plots`, `state`, `done` | Plot states of the model: time_series (each against time), phase_plane (two or three, the first on the x axis) or array_plot (from the first of two to the later). |
| `equilibrium.find` | none | `x` / analysis | Analysis/Equilibria/Go | `converged`: `boolean`, `equilibrium`: `map[name, real]`, `eigenvalues`: `list[map[string, real]]`, `stability`: `choice` | `equilibrium`, `marks`, `series`, `state`, `done` | Find an equilibrium from the current initial conditions. |
| `equilibrium.guess` | `coordinates`: required map [of variable, of real, at most 256], `window`: optional window (max 1024 bytes) | `x` / analysis | Analysis/Equilibria/Mouse | `converged`: `boolean`, `equilibrium`: `map[name, real]`, `eigenvalues`: `list[map[string, real]]`, `stability`: `choice` | `equilibrium`, `marks`, `series`, `state`, `done` | Find an equilibrium from a guess in plot coordinates. |
| `equilibrium.range` | `parameter`: required name (max 16384 bytes) [of parameter], `start`: required real, `end`: required real, `steps`: required integer [at least 1, at most 2147483646], `shoot`: optional boolean, `stability_column`: optional integer [at least 0, at most the session's columns], `movie`: optional boolean, `monte_carlo`: optional boolean | `x` / analysis | Analysis/Equilibria/Range | `points_found`: `integer` | `equilibrium`, `marks`, `series`, `state`, `done` | Find equilibria over a parameter range. |
| `equilibrium.search` | `append`: optional boolean, `guesses`: required integer [at least 1, at most 2147483647], `tolerance`: required real [at least 5e-324], `shoot`: optional boolean, `bounds`: required map [of variable, of list[real, 2], at most 256] | `x` / analysis | Analysis/Equilibria/Monte Carlo | `found`: `integer`, `equilibria`: `list[map[name, real]]` | `equilibrium`, `marks`, `series`, `state`, `done` | Search for equilibria within variable bounds. |
| `equilibrium.import_as_ic` | none | `s` / analysis | Analysis/Equilibrium/Import | `applied`: `map[name, real]` | `equilibrium`, `marks`, `series`, `state`, `done` | Import the last equilibrium as initial conditions. |
| `equilibrium.export` | `file`: required file (max 1024 bytes), `shoot`: optional boolean, `confirm`: optional boolean | `d` / analysis | Analysis/export | `file`: `file`, `written`: `boolean` | `done` | Write the equilibrium and its eigenvalues. |
| `equilibrium.configure` | `max_iterates`: optional integer [at least 1, at most 2147483647], `newton_tolerance`: optional real, `jacobian_epsilon`: optional real, `search_limit`: optional real, `print_eigenvalues`: optional boolean, `draw_invariant_sets`: optional boolean, `draw_strong_sets`: optional boolean | `s` / analysis | Analysis/Equilibria/Settings | `settings`: `map[string, real]`, `print_eigenvalues`: `boolean`, `draw_invariant_sets`: `boolean`, `draw_strong_sets`: `boolean` | `equilibrium`, `marks`, `series`, `state`, `done` | Configure the equilibrium search: Newton's method and what a found point prints and draws. |
| `nullcline.compute` | none | `x` / analysis | Analysis/Nullclines/New | `curves_computed`: `integer` | `nullclines`, `state`, `done` | Compute the phase plane's nullclines. |
| `nullcline.redraw` | none | `v` / analysis | Analysis/Nullclines/Restore | `drawn`: `boolean` | `nullclines`, `state`, `done` | Draw stored nullclines again. |
| `nullcline.auto_mode` | `enabled`: required boolean | `v` / analysis | Analysis/Nullclines/Auto or Manual | `auto_redraw`: `boolean` | `nullclines`, `state`, `done` | Set automatic nullcline redrawing. |
| `nullcline.freeze` | none | `v` / analysis | Analysis/Nullclines/Freeze/Freeze | `frozen_curves_count`: `integer` | `nullclines`, `state`, `done` | Freeze the current nullclines. |
| `nullcline.clear_frozen` | none | `v` / analysis | Analysis/Nullclines/Freeze/Delete all | `cleared`: `boolean` | `nullclines`, `state`, `done` | Clear frozen nullclines. |
| `nullcline.range_freeze` | `parameter`: required name (max 16384 bytes) [of parameter], `start`: required real, `end`: required real, `steps`: required integer [at least 1, at most 9999] | `x` / analysis | Analysis/Nullclines/Freeze/Range | `steps_frozen`: `integer` | `nullclines`, `state`, `done` | Freeze nullclines over a parameter range. |
| `nullcline.animate` | `delay_ms`: optional integer [at least 0, at most 2147483647] | `v` / analysis | Analysis/Nullclines/Freeze/Animate | `animated`: `boolean` | `done` | Replay frozen nullclines. |
| `nullcline.export` | `file`: required file (max 1024 bytes), `format`: optional choice (dat, csv, csv.gz, npz), `confirm`: optional boolean | `d` / analysis | Analysis/Nullclines/Save | `file`: `file`, `points_written`: `integer` | `done` | Write the current nullcline segments. |
| `nullcline.set_mesh` | `mesh`: required integer [at least 1, at most 4096] | `s` / analysis | Analysis/Numerics/Nullcline mesh | `nullcline_mesh`: `integer` | `nullclines`, `state`, `done` | Set the nullcline mesh. |
| `dirfield.compute` | `grid`: required integer [at least 2, at most 4096], `scaled`: optional boolean | `x` / analysis | Analysis/Two-D Fun/Direct or Scaled Field | `grid`: `integer`, `vectors_drawn`: `integer` | `dfield`, `flow`, `nullclines`, `state`, `done` | Compute the phase plane's direction field. |
| `dirfield.flow` | `grid`: required integer [at least 2, at most 4096] | `x` / analysis | Analysis/Two-D Fun/Flow | `grid`: `integer`, `trajectories_run`: `integer` | `dfield`, `flow`, `nullclines`, `state`, `done` | Integrate trajectories on the phase plane grid. |
| `dirfield.clear` | none | `v` / analysis | Analysis/Two-D Fun/No dir. fld. | `cleared`: `boolean` | `dfield`, `flow`, `nullclines`, `state`, `done` | Clear the direction field. |
| `dirfield.colorize` | `grid`: required integer [at least 2, at most 4096] | `x` / analysis | Analysis/Two-D Fun/Colorize | `grid`: `integer`, `cells_colored`: `integer` | `dfield`, `flow`, `nullclines`, `state`, `done` | Color the phase plane grid. |
| `dirfield.export` | `file`: required file (max 1024 bytes), `confirm`: optional boolean | `d` / analysis | Analysis/export | `file`: `file`, `written`: `boolean` | `done` | Write direction field arrows. |
| `analysis.seed_random` | `seed`: required integer [at least 0, at most 2147483647] | `d` / analysis | Analysis/Numerics/Stochastic/New seed | `seed`: `integer` | `state`, `done` | Seed the random numbers of the next run. |
| `analysis.stochastic_integrate` | `trials`: optional integer [at least 2, at most 2147483647] | `x` / analysis | Analysis/Numerics/Stochastic/Compute | `trials_completed`: `integer` | `computing`, `progress`, `series`, `stopped`, `state`, `done` | Run the model as many times as trials (the Range settings' steps plus one), keeping the mean and variance of the runs. |
| `analysis.load_stochastic_run` | none | `d` / analysis | Analysis/Numerics/Stochastic/Data | `loaded`: `boolean` | `browser`, `state`, `done` | Show the data of the last run in the data browser again. |
| `analysis.load_stochastic_mean` | none | `d` / analysis | Analysis/Numerics/Stochastic/Mean | `loaded`: `boolean` | `browser`, `state`, `done` | Show the mean of the stochastic runs in the data browser. |
| `analysis.load_stochastic_variance` | none | `d` / analysis | Analysis/Numerics/Stochastic/Variance | `loaded`: `boolean` | `browser`, `state`, `done` | Show the variance of the stochastic runs in the data browser. |
| `analysis.histogram` | `column`: optional name (max 1024 bytes) [of column], `bins`: optional integer [at least 2, at most 4096], `low`: optional real, `high`: optional real, `condition`: optional text (max 1024 bytes) | `x` / analysis | Analysis/Numerics/Stochastic/Histogram | `bins`: `integer`, `total_samples`: `integer` | `browser`, `plots`, `series`, `state`, `done` | Make the histogram of a column of the stored data, counting the rows where the condition holds. |
| `analysis.reload_histogram` | none | `d` / analysis | Analysis/Numerics/Stochastic/Old hist | `reloaded`: `boolean` | `browser`, `plots`, `series`, `state`, `done` | Show the last histogram in the data browser again. |
| `analysis.histogram_2d` | `column1`: optional name (max 1024 bytes) [of column], `column2`: optional name (max 1024 bytes) [of column], `bins1`: optional integer [at least 2, at most 4096], `bins2`: optional integer [at least 2, at most 4096], `low1`: optional real, `high1`: optional real, `low2`: optional real, `high2`: optional real | `x` / analysis | Analysis/Numerics/Stochastic/2D-hist | `bins_x`: `integer`, `bins_y`: `integer` | `browser`, `plots`, `series`, `state`, `done` | Make the two-dimensional histogram of two columns of the stored data. |
| `analysis.fourier_series` | `column`: optional name (max 1024 bytes) [of column] | `x` / analysis | Analysis/Numerics/Stochastic/Fourier | `harmonics`: `integer` | `browser`, `state`, `done` | Make the Fourier series of a column of the stored data. |
| `analysis.power_spectrum` | `column`: optional name (max 1024 bytes) [of column] | `x` / analysis | Analysis/Numerics/Stochastic/Power | `frequencies`: `integer` | `browser`, `plots`, `series`, `state`, `done` | Make the power spectrum of a column of the stored data: each harmonic's amplitude and phase. |
| `analysis.fit_curve` | `variables`: required list [of variable, of name, at most 256], `parameters`: required list [of either, of name, at most 256], `tolerance`: optional real [at least 2.2250738585072014e-308], `points`: optional integer [at least 2, at most 2147483647], `columns`: optional integer [at least 2, at most 2147483647], `data_columns`: optional list [at least 2, at most 2147483647, of integer, at most 256], `epsilon`: optional real [at least 2.2250738585072014e-308], `max_iterations`: optional integer [at least 1, at most 2147483647], `file`: optional file (max 1024 bytes) | `x` / analysis | Analysis/Numerics/Stochastic/fIt data | `converged`: `boolean`, `parameters`: `map[name, real]` | `state`, `done` | Fit parameters and initial values so that variables of the model follow columns of a data file of the model's folder (Levenberg-Marquardt). |
| `analysis.statistics` | `column`: optional name (max 1024 bytes) [of column] | `x` / analysis | Analysis/Numerics/Stochastic/Stat | `mean`: `real`, `variance`: `real`, `std_dev`: `real` | `done` | The mean, variance and standard deviation of a column of the stored data. |
| `analysis.lyapunov_exponent` | `range_over_parameters`: optional boolean, `variable`: optional name (max 1024 bytes) [of either], `start`: optional real, `end`: optional real, `steps`: optional integer [at least 1], `reset_storage`: optional boolean, `use_old_ic`: optional boolean, `cycle_color`: optional boolean, `movie`: optional boolean, `write_each_run_to_file`: optional boolean | `x` / analysis | Analysis/Numerics/Stochastic/Liapunov | `max_lyapunov_exponent`: `real`, `runs_completed`: `integer` | `browser`, `plots`, `series`, `state`, `done` | The maximal Liapunov exponent of the stored orbit, or with range_over_parameters one for each run of a sweep of the Range settings. |
| `analysis.autocorrelation` | `column`: optional name (max 1024 bytes) [of column], `bins`: optional integer [at least 2, at most 4096], `low`: optional real, `high`: optional real | `x` / analysis | Analysis/Numerics/Stochastic/stAutocor | `lags_computed`: `integer` | `browser`, `plots`, `series`, `state`, `done` | Make the histogram of the differences of a column of the stored data with itself. |
| `analysis.cross_correlation` | `column1`: optional name (max 1024 bytes) [of column], `column2`: optional name (max 1024 bytes) [of column], `lags`: optional integer [at least 2, at most 4096], `method`: optional choice (direct, fft) | `x` / analysis | Analysis/Numerics/Stochastic/Xcorrel etc | `lags_computed`: `integer` | `browser`, `plots`, `series`, `state`, `done` | Make the correlation of two columns of the stored data at a number of lags either side of 0, directly or by the FFT. |
| `analysis.spectral_density` | `type`: optional choice (psd_x, psd_xy, coherence_xy), `column`: optional name (max 1024 bytes) [of column], `column2`: optional name (max 1024 bytes) [of column], `window_length`: optional integer [at least 2, at most 2147483647], `window`: optional choice (square, parzen, hamming, bartlett, hann) | `x` / analysis | Analysis/Numerics/Stochastic/spEc.dns | `points`: `integer` | `browser`, `plots`, `series`, `state`, `done` | Make the power spectral density of a column, the cross spectrum of two or their coherence, averaged over windows of the stored data. |
| `analysis.poincare_map` | `type`: required choice (none, section, max_min, period), `variable`: optional name (max 1024 bytes) [of column], `section`: optional real, `direction`: optional choice (negative, both, positive), `stop_on_section`: optional boolean | `s` / analysis | Analysis/Numerics/Poincare map | `poincare_configured`: `boolean` | `state`, `done` | Set the Poincare map the runs store: none, the crossings of a section, the maxima and minima of a variable, or its period. |
| `analysis.phase_space_torus` | `mode`: required choice (all, none, choose), `variables`: optional list [of variable, of name, at most 256], `period`: optional real [at least 2.2250738585072014e-308] | `d` / plot | Analysis/Phase space/All, None, Choose | `torus_variables`: `list[name]` | `state`, `done` | Fold all, none or chosen variables of the phase space into a torus of the given period. |
| `analysis.color_coding` | `mode`: required choice (none, velocity, variable), `variable`: optional name (max 1024 bytes) [of column], `low`: optional real, `high`: optional real, `window`: optional window (max 1024 bytes); all or none of `low`, `high` | `v` / plot | Analysis/Numerics/Color by value | `color_coding`: `choice` | `plots`, `state`, `done` | Color a graph's orbit by its velocity or by a column, between given limits or those of the stored data. |
| `analysis.adjoint_compute` | none | `x` / analysis | Analysis/Numerics/Adjoint/New adj | `computed`: `boolean` | `done` | Compute the adjoint of the stored periodic orbit. |
| `analysis.adjoint_interaction` | `couplings`: optional map [of variable, of expression, at most 256] | `x` / analysis | Analysis/Numerics/Adjoint/Make H | `computed`: `boolean` | `done` | Make the interaction (H) function of the adjoint for the coupling of each differential equation; one left out keeps its coupling. |
| `analysis.adjoint_load` | none | `d` / analysis | Analysis/Numerics/Adjoint/Adjoint | `loaded`: `boolean` | `browser`, `state`, `done` | Show the adjoint in the data browser. |
| `analysis.adjoint_load_orbit` | none | `d` / analysis | Analysis/Numerics/Adjoint/Orbit | `loaded`: `boolean` | `browser`, `state`, `done` | Show the orbit in the data browser. |
| `analysis.adjoint_load_interaction` | none | `d` / analysis | Analysis/Numerics/Adjoint/Hfun | `loaded`: `boolean` | `browser`, `state`, `done` | Show the interaction (H) function in the data browser. |
| `analysis.adjoint_configure` | `max_iterates`: optional integer [at least 1, at most 2147483647], `tolerance`: optional real [at least 2.2250738585072014e-308] | `d` / analysis | Analysis/Numerics/Adjoint/Parameters | `updated`: `boolean` | `state`, `done` | Set the adjoint's maximum iterations and error tolerance. |
| `analysis.adjoint_range` | `enabled`: optional boolean | `x` / analysis | Analysis/Numerics/Adjoint/Range | `adjoint_range`: `boolean` | `state`, `done` | Have the next range run also compute the orbit, adjoint and H function at each step; the range itself is run.range. |
| `table.edit` | `name`: required name (max 1024 bytes) [of any], `formula`: optional expression (max 1024 bytes), `x_min`: optional real, `x_max`: optional real, `points`: optional integer [at least 2, at most 1000000], `auto_evaluate`: optional boolean, `file`: optional file (max 1024 bytes) | `d` / panels | Analysis/Numerics/Tables/Edit | `table`: `name`, `points_evaluated`: `integer` | `state`, `done` | Change a table of a formula (its formula, points, range, and whether it is evaluated again when a parameter changes) or read a table of a file again from its file or another; what is left out stays. |
| `table.view_in_browser` | `name`: required name (max 1024 bytes) [of any] | `v` / panels | Analysis/Numerics/Tables/View | `table`: `name`, `shown_in_browser`: `boolean` | `browser`, `state`, `done` | Show a table's points in the data browser. |
| `browser.find` | `column`: required name (max 1024 bytes) [of column], `value`: required real | `v` / tools | Data/Find | `row_found`: `integer` | `browser`, `state`, `done` | Select the row, from the selected one on, whose value in a column is nearest a number. |
| `browser.replace_column` | `column`: required name (max 1024 bytes) [of column], `formula`: required text (max 1024 bytes) | `d` / tools | Data/Replace | `column`: `name`, `replaced`: `boolean` | `browser`, `state`, `done` | Replace a column of the data by a formula of the columns, '&column' (its running integral), '@column' (its derivative), 'a:b' (from a to b) or 'a;b' (from a in steps of b). |
| `browser.undo_replace` | none | `d` / tools | Data/Unreplace | `undone`: `boolean` | `browser`, `state`, `done` | Put back the column the last replace overwrote. |
| `browser.tabulate_column` | `variable`: required name (max 1024 bytes) [of column], `x_low`: required real, `x_high`: required real, `file`: required file (max 1024 bytes) | `d` / tools | Data/Table | `file`: `file`, `rows_written`: `integer` | `state`, `done` | Write the rows kept between First and Last of a column as a lookup table over x_low to x_high into a file of the model's folder. |
| `browser.set_slice_start` | `row`: required integer [at least 0, at most 2147483647] | `d` / tools | Data/First | `first_row`: `integer` | `browser`, `state`, `done` | Keep the data from a row on (the first is 0) for saving and tabulating, and select that row. |
| `browser.set_slice_end` | `row`: required integer [at least 0, at most 2147483647] | `d` / tools | Data/Last | `last_row`: `integer` | `browser`, `state`, `done` | Keep the data up to and including a row for saving and tabulating, and select that row. |
| `browser.restore_full_slice` | none | `d` / tools | Data/Restore | `first_row`: `integer`, `last_row`: `integer` | `browser`, `state`, `done` | Keep all the data again, from the first row to the last, and replot it in the selected graph. |
| `browser.add_column` | `name`: required text (max 1024 bytes), `formula`: required expression (max 1024 bytes) | `d` / tools | Data/Add column | `column`: `name`, `added`: `boolean` | `browser`, `state`, `done` | Add a column computed from a formula of the others, now and after every run, as though it were another auxiliary quantity. |
| `browser.delete_column` | `name`: required name (max 1024 bytes) [of column] | `d` / tools | Data/Delete column | `deleted`: `boolean` | `browser`, `state`, `done` | Delete a column made by add_column (time and the model's columns stay; a plotted one is refused). |
| `browser.load` | `file`: optional file (max 1024 bytes), `format`: optional choice (dat, csv, csv.gz, npz) | `d` / files | File/Load data | `file`: `file`, `rows_loaded`: `integer` | `browser`, `state`, `done` | Read a data file of the model's folder into the table (its columns fill T and the columns after it in order); with no file, ask for one (the result's file is empty when that question was cancelled). |
| `browser.export` | `file`: optional file (max 1024 bytes), `format`: optional choice (dat, csv, csv.gz, npz), `target`: optional choice (table, output, plot_curves) | `d` / files | File/Export data | `file`: `file`, `rows_written`: `integer` | `state`, `done` | Save the data table (the rows kept between First and Last), the model's output columns or what the plot shows as a data file in the model's folder; left out, format follows the file's extension and file and target are asked for (the result's file is empty when a question was cancelled). |
| `browser.page` | `from_row`: required integer [at least 0, at most 2147483647], `count`: required integer [at least 0, at most 2000], `first_column`: optional integer [at least 1, at most 2147483647], `columns`: optional integer [at least 1, at most 500] | `v` / tools | Data/Browser/Page | `first_row`: `integer`, `total_rows`: `integer`, `rows`: `list[list[real]]` | `browser`, `done` | Choose the block of the table the client shows: rows from_row on (at most the limit hello gives), count rows (0 stops the block's events), the columns after T from first_column; answered at once, even during a computation. |
| `browser.transpose` | `first_column`: optional name (max 1024 bytes) [of column], `columns`: optional integer [at least 1, at most 2147483647], `column_skip`: optional integer [at least 1, at most 2147483647], `first_row`: optional integer [at least 0, at most 2147483647], `rows`: optional integer [at least 1, at most 2147483647], `row_skip`: optional integer [at least 1, at most 2147483647] | `d` / tools | Data/Transpose data | `transposed_rows`: `integer`, `transposed_cols`: `integer` | `browser`, `state`, `done` | Show the stored data transposed in the table: columns columns from first_column in steps of column_skip, and rows rows from first_row in steps of row_skip, each column a row; left out, the last values. |
| `set.apply` | `name`: required text (max 1024 bytes), `scope`: required choice (all, parameters, initial, integration, poincare, equilibria, continuation, plot, sweep, analysis), `window`: optional window (max 1024 bytes) | `s` / files | File/Apply set | `applied`: `text`, `scope`: `text` | `state`, `done` | Apply one of the model's named sets over the current values: all of it, or only its parameters, initial values or one group of settings; the plot settings go to the graph window names. |
| `set.save_new` | `name`: required text (max 1024 bytes), `scope`: required choice (all, parameters, initial, integration, poincare, equilibria, continuation, plot, sweep, analysis), `window`: optional window (max 1024 bytes), `state`: optional choice (initial, last) | `d` / files | File/Save values | `name`: `text`, `saved`: `boolean` | `state`, `done` | Write the current values (all of them, or one scope) into the model file as a new set called name; state says whether the initial values or the last state reached are saved. |
| `set.save_into` | `name`: required text (max 1024 bytes), `scope`: required choice (all, parameters, initial, integration, poincare, equilibria, continuation, plot, sweep, analysis), `window`: optional window (max 1024 bytes), `state`: optional choice (initial, last) | `d` / files | File/Save values | `name`: `text`, `updated`: `boolean` | `state`, `done` | Replace the scope's part of one of the model file's own sets by the current values; the set's other parts stay. |
| `set.save_defaults` | `scope`: required choice (all, parameters, initial, integration, poincare, equilibria, continuation, plot, sweep, analysis), `window`: optional window (max 1024 bytes), `state`: optional choice (initial, last) | `d` / files | File/Save as defaults | `saved_as_defaults`: `boolean` | `state`, `done` | Write the current values (all of them, or one scope) as the model's own: its par and init lines and options groups. |
| `set.export` | `file`: required file (max 1024 bytes), `name`: required text (max 1024 bytes), `scope`: required choice (all, parameters, initial, integration, poincare, equilibria, continuation, plot, sweep, analysis), `window`: optional window (max 1024 bytes), `state`: optional choice (initial, last) | `d` / files | File/Export values | `file`: `file`, `name`: `text`, `exported`: `boolean` | `state`, `done` | Write the current values (all of them, or one scope) as the set called name of another .odex file, a new one or an existing one that asks before a set of that name is replaced. |
| `set.rename` | `from`: required text (max 1024 bytes), `to`: required text (max 1024 bytes) | `d` / files | File/Rename set | `from`: `text`, `to`: `text`, `renamed`: `boolean` | `state`, `done` | Rename one of the model file's own sets. |
| `set.delete` | `name`: required text (max 1024 bytes) | `d` / files | File/Delete set | `name`: `text`, `deleted`: `boolean` | `state`, `done` | Delete one of the model file's own sets from the file. |
| `set.restore_defaults` | `scope`: required choice (all, parameters, initial, integration, poincare, equilibria, continuation, plot, sweep, analysis), `window`: optional window (max 1024 bytes), `field`: optional text (max 1024 bytes) | `s` / files | File/Restore defaults | `restored`: `boolean` | `state`, `done` | Put the model's own values back: everything, or one scope (the parameters, the initial values, one group of settings), or with field only that parameter, state or setting. |
| `value.set_parameters` | `values`: required map [of parameter, of real, at most 256] | `s` / run | Data/Parameters | `updated`: `map[name, real]` | `state`, `done` | Set the values of some parameters; the others keep theirs. Runs nothing. |
| `value.set_by_formula` | `name`: required name (max 1024 bytes) [of parameter], `formula`: required expression (max 1024 bytes) | `s` / run | Data/Parameter formula | `name`: `name`, `value`: `real` | `state`, `done` | Set a parameter to what a formula works out to now (once: the parameter keeps the number, not the formula). |
| `value.undo` | none | `s` / run | Data/Undo value edit | `undone`: `boolean` | `state`, `done` | Take back the last edit of the parameters, initial conditions or numerics (not runs, plots or files). |
| `value.redo` | none | `s` / run | Data/Redo value edit | `redone`: `boolean` | `state`, `done` | Put back the value edit that was undone. |
| `value.slide` | `name`: required name (max 1024 bytes) [of either], `value`: required real | `s` / run | Data/Values/Slide | `name`: `name`, `value`: `real` | `state`, `done` | Set a parameter or a state's initial value to a number, as a slider does; moves of one slider in a row are one undo step. |
| `value.configure_slider` | `slot`: required integer [at least 0], `name`: required name (max 1024 bytes) [of either], `low`: required real, `high`: required real, `step`: optional real [at least 0] | `s` / run | Data/Values/Configure slider | `slot`: `integer`, `configured`: `boolean` | `state`, `done` | Define a slider: its slot (an existing one, or the next), the parameter or state it moves, its range, and its step (0: the page picks one; left out: the slot's own). |
| `value.remove_slider` | `slot`: required integer [at least 0] | `s` / run | Data/Values/Remove slider | `slot`: `integer`, `removed`: `boolean` | `state`, `done` | Empty a slider's slot. |
| `value.save_file` | `kind`: required choice (parameters, initial_conditions), `file`: optional file (max 1024 bytes) | `d` / files | File/Values/Save file | `file`: `file`, `kind`: `choice` | `state`, `done` | Write the parameters (.par) or the initial conditions (.ic) as XPP's own file into the model's folder; with no file, ask for one (the result's file is empty when that question was cancelled). |
| `value.load_file` | `kind`: required choice (parameters, initial_conditions), `file`: optional file (max 1024 bytes) | `s` / files | File/Values/Load file | `file`: `file`, `kind`: `choice` | `state`, `done` | Read the parameters (.par) or the initial conditions (.ic) from XPP's own file of the model's folder, all or nothing; with no file, ask for one (the result's file is empty when that question was cancelled). |
| `kinescope.capture` | none | `v` / plot | File/Kinescope/Capture | `frame_count`: `integer` | `film`, `state`, `done` | Capture the plot window as the next frame of the kinescope. |
| `kinescope.reset` | none | `v` / plot | File/Kinescope/Reset | `cleared`: `boolean` | `film`, `state`, `done` | Throw away every captured frame. |
| `kinescope.play` | none | `v` / plot | File/Kinescope/Playback | `frames_played`: `integer` | `film`, `state`, `done` | Show the captured frames once. |
| `kinescope.autoplay` | `cycles`: optional integer [at least 1, at most 2147483647], `frame_ms`: optional integer [at least 0, at most 2147483647] | `v` / plot | File/Kinescope/Autoplay | `cycles_completed`: `integer` | `film`, `state`, `done` | Show the captured frames cycles times, frame_ms apart; what is left out is the last autoplay's setting. |
| `kinescope.export_frames` | `file`: required file (max 1024 bytes) | `d` / plot | File/Kinescope/Save | `file`: `file`, `frames_saved`: `integer` | `state`, `done` | Write every captured frame as a GIF file of the model's folder: file for the first, then its numbered names. |
| `kinescope.export_gif` | `file`: required file (max 1024 bytes) | `d` / plot | File/Kinescope/Make AniGif | `file`: `file`, `bytes_written`: `integer` | `state`, `done` | Write the captured frames as one animated GIF file of the model's folder. |
| `animation.open` | none | `v` / plot | Windows/Viewaxes/Toon | `open`: `boolean` | `window`, `ani`, `state`, `done` | Open the animation window. |
| `animation.load` | `file`: required file (max 1024 bytes) | `v` / plot | Windows/Animation/File | `file`: `file`, `loaded`: `boolean` | `ani`, `state`, `done` | Load an animation file (.anix or .ani) of the model's folder; its first frame shows when there is data. |
| `animation.play` | none | `v` / plot | Windows/Animation/Go | `playing`: `boolean` | `ani`, `state`, `done` | Play the stored rows as frames from the one shown to the last. |
| `animation.reset` | none | `v` / plot | Windows/Animation/Reset | `frame`: `integer` | `ani`, `state`, `done` | Back to the first frame. |
| `animation.set_frame_skip` | `skip`: required integer [at least 1, at most 100000000] | `v` / plot | Windows/Animation/Skip | `skip`: `integer` | `ani`, `state`, `done` | Show every skip-th stored row when playing. |
| `animation.export_frames` | `ppm`: optional boolean, `animated_gif`: optional boolean, `file`: required file (max 1024 bytes) | `d` / plot | File/Animation/Mpeg | `frames_saved`: `integer` | `ani`, `state`, `done` | Play the animation writing every frame, as numbered PPM files from file (ppm) or as one animated GIF file (animated_gif), one of the two, into the model's folder. |
| `animation.set_live_draw` | `enabled`: required boolean | `v` / plot | Windows/Animation/On the fly | `live_draw`: `boolean` | `ani`, `state`, `done` | Draw the animation while an integration runs. |
| `animation.grab_point` | `x`: optional real, `y`: optional real, `to_x`: optional real, `to_y`: optional real; all or none of `x`, `y`; all or none of `to_x`, `to_y` | `x` / plot | Windows/Animation/Grab | `grabbed`: `boolean` | `ani`, `series`, `state`, `done` | Pick a grab point in the animation's coordinates and optionally move it; omit coordinates to begin pointer selection. |
| `animation.set_speed` | `ms`: required integer [at least 0, at most 1000] | `v` / plot | Windows/Animation/Set speed | `speed_ms`: `integer` | `ani`, `state`, `done` | Set the delay between two frames of Go in ms; it reaches a Go that is running. |
| `animation.step` | `rows`: required integer [at least -2147483647, at most 2147483647] | `v` / plot | Windows/Animation/Step | `row`: `integer` | `ani`, `state`, `done` | Move the frame shown by rows stored rows (negative: back). |
| `animation.seek` | `row`: required integer [at least 0, at most 2147483647] | `v` / plot | Windows/Animation/Seek | `row`: `integer` | `ani`, `state`, `done` | Show the frame of stored row row (row 0 is the first). |
| `animation.pause` | none | `v` / plot | Windows/Animation/Pause | `speed_ms`: `integer` | `ani`, `state`, `done` | Stop a Go that is running at the frame it is on; with none running it changes nothing. |
| `animation.faster` | none | `v` / plot | Windows/Animation/Faster | `speed_ms`: `integer` | `ani`, `state`, `done` | Shorten the delay between two frames of Go by a step; it reaches a Go that is running. |
| `animation.slower` | none | `v` / plot | Windows/Animation/Slower | `speed_ms`: `integer` | `ani`, `state`, `done` | Lengthen the delay between two frames of Go by a step; it reaches a Go that is running. |
| `array_plot.open` | none | `v` / plot | Windows/Viewaxes/Array | `open`: `boolean` | `window`, `aplot`, `state`, `done` | Open the array plot window. |
| `array_plot.redraw` | none | `v` / plot | Windows/Array plot/Redraw | `redrawn`: `boolean` | `aplot`, `state`, `done` | Draw the array plot again from the stored data. |
| `array_plot.configure` | `first_column`: optional name (max 1024 bytes) [of column], `columns`: optional integer [at least 1, at most 1000000], `first_row`: optional integer [at least 0, at most 2147483647], `rows`: optional integer [at least 1, at most 1000000], `row_skip`: optional integer [at least 1, at most 2147483647], `z_min`: optional real, `z_max`: optional real, `auto_redraw`: optional boolean, `column_skip`: optional integer [at least 1, at most 2147483647] | `v` / plot | Windows/Array plot/Edit | `configured`: `boolean` | `aplot`, `state`, `done` | Set the columns and rows the array plot shows and the ends of its colour scale (z_min below z_max); what is left out stays. |
| `array_plot.fit` | none | `v` / plot | Windows/Array plot/Fit | `fitted_min`: `real`, `fitted_max`: `real` | `aplot`, `state`, `done` | Set the colour scale's ends to the smallest and largest value shown. |
| `array_plot.range` | `still`: optional boolean, `tag`: optional boolean, `file`: required file (max 1024 bytes), `variable`: optional name (max 1024 bytes) [of either], `start`: optional real, `end`: optional real, `steps`: optional integer [at least 1], `reset_storage`: optional boolean, `use_old_ic`: optional boolean, `cycle_color`: optional boolean, `movie`: optional boolean, `write_each_run_to_file`: optional boolean | `x` / plot | Windows/Array plot/Range | `steps_completed`: `integer` | `aplot`, `series`, `state`, `done` | Run once for each value of a parameter or variable as run.range does, saving the array plot as GIF files of the model's folder: one movie, or with still one picture per run; tag writes the swept value on each. |
| `array_plot.export_pdf` | `file`: required file (max 1024 bytes), `top_label`: optional text (max 1024 bytes), `side_label`: optional text (max 1024 bytes), `bottom_label`: optional text (max 1024 bytes) | `d` / plot | File/Array plot/Print PDF | `file`: `file` | `state`, `done` | Write the array plot as a PDF picture into a file of the model's folder; the labels left out are the last ones. |
| `array_plot.export_gif` | `file`: required file (max 1024 bytes) | `d` / plot | File/Array plot/GIF | `file`: `file`, `bytes_written`: `integer` | `state`, `done` | Write the array plot as a GIF picture into a file of the model's folder. |
| `array_plot.scroll` | `first_row`: required integer [at least 0, at most 2147483647] | `v` / plot | Windows/Array plot/Scroll | `first_row`: `integer` | `aplot`, `state`, `done` | Show the array plot from the stored row first_row on (row 0 is the first). |
| `session.open_model` | `file`: optional path (max 1024 bytes), `example`: optional text (max 1024 bytes), `recent`: optional integer [at least 0]; at most one of `file`, `example`, `recent` | `d` / files | File/Open model | `model`: `text`, `variables`: `list[name]`, `parameters`: `list[name]` | `hello`, `state`, `done` | Open a model: a model file by its path (--server's own input only), a bundled example by its name, or one of the recent models by its place in hello.start.recent. Asks first whether to save this session. |
| `session.reload_model` | none | `d` / files | File/Reload model | `reloaded`: `boolean` | `hello`, `state`, `done` | Load the model's file again; the parameters, initial data, numerics and AUTO settings keep their values by name. |
| `session.open` | `file`: required file (max 1024 bytes) | `d` / files | File/Open session | `file`: `file`, `loaded`: `boolean` | `hello`, `state`, `done` | Open a session file (.snapx): its model, then everything as it was saved. |
| `session.save` | `data`: optional boolean | `d` / files | File/Save session | `file`: `file`, `saved`: `boolean` | `saved`, `state`, `done` | Save the session to the file it was last saved to or opened from; data false leaves the data table out. |
| `session.save_as` | `file`: required file (max 1024 bytes), `data`: optional boolean, `confirm`: optional boolean | `d` / files | File/Save session as | `file`: `file`, `saved`: `boolean` | `saved`, `state`, `done` | Save the session to a file of the model's folder (.snapx added); confirm true replaces an existing file without asking. |
| `session.save_copy` | `file`: required file (max 1024 bytes), `data`: optional boolean, `confirm`: optional boolean | `d` / files | File/Save a copy of the session | `file`: `file`, `copied`: `boolean` | `done` | Save a copy of the session, leaving the file it is saved to as it was. |
| `session.import_settings` | `file`: required file (max 1024 bytes) | `s` / files | File/Import settings | `file`: `file`, `applied`: `boolean` | `state`, `done` | Apply a settings file (.setx, or XPPAUT's .set, converted beside it). |
| `session.export_info` | `file`: required file (max 1024 bytes), `confirm`: optional boolean | `d` / files | File/Export simulation information | `file`: `file`, `written`: `boolean` | `done` | Write the equations, numerics, initial conditions and parameters to a file. |
| `session.get_source` | none | `v` / files | File/Model source | `source`: `text`, `equations`: `list[text]` | `source`, `equations`, `done` | The model's source, its active comments and its equations. |
| `session.quit` | `save`: optional boolean | `c` / files | File/Quit | `quitting`: `boolean` | `done` | Quit: save true saves the session (and a recording in progress) first, false quits at once, and left out asks. |
| `session.get_state` | none | `v` / files | File/Session/Get state | `state`: `the state event's members` | `state`, `done` | The session's state: sliders, parameters, initial conditions, the view, the saved session and the recording. |
| `recording.start` | none | `d` / panels | File/Record steps | `recording`: `boolean` | `state`, `done` | Start recording the session's steps, from the session as it is now. |
| `recording.stop` | `file`: optional file (max 1024 bytes), `confirm`: optional boolean | `d` / panels | File/Record steps | `file`: `file`, `steps_recorded`: `integer` | `state`, `done` | Stop recording and save the recording (.recx added); left out, the file is asked for; confirm true replaces an existing file without asking. |
| `recording.add_note` | `note`: required text (max 4096 bytes) | `c` / panels | File/Recording/Add note | `note_added`: `boolean` | `done` | Put a note on the next recorded step. |
| `recording.open` | `file`: required path (max 1024 bytes), `play`: required boolean | `d` / panels | File/Play recording | `file`: `file`, `total_steps`: `integer` | `player`, `state`, `done` | Open a recording (.recx; its path, which only --server's own input may give) in the player: its model is loaded and its steps are ready; play says whether it starts playing at once. |
| `recording.play` | none | `c` / panels | File/Recording/Play | `playing`: `boolean` | `done` | Play the open recording from where it is. |
| `recording.pause` | none | `c` / panels | File/Recording/Pause | `paused`: `boolean` | `done` | Pause the playing recording. |
| `recording.step` | none | `c` / panels | File/Recording/Step | `step_index`: `integer` | `done` | Play the next step of the open recording. |
| `recording.set_speed` | `speed`: required real [at least 0.25, at most 8] | `c` / panels | File/Recording/Set speed | `speed`: `real` | `done` | Set the speed the recording's paces are divided by (hello.player_speed gives the bounds). |
| `recording.seek` | `step`: required integer [at least 0], `play`: optional boolean | `d` / panels | File/Recording/Seek | `step`: `integer` | `player`, `state`, `done` | Jump to a step: the steps before it run at once, then the recording plays when play is true (left out: when the step is not the first). |
| `recording.close` | none | `d` / panels | File/Recording/Close | `closed`: `boolean` | `state`, `done` | Leave the player; the model stays. |
| `recording.set_step_note` | `step`: required integer [at least 0], `note`: required text (max 4096 bytes), `confirm`: optional boolean | `d` / panels | File/Recording/Set step note | `step`: `integer` | `player`, `state`, `done` | Write a note into a step of the open recording's file; confirm true replaces the file without asking. |
| `keymap.get` | none | `c` / panels | Help/Keymap/Get | `bindings`: `map[text, list[text]]`, `pinned`: `list[text]` | `keymap`, `done` | The user's keymap: the keys of the commands they changed, and the pinned commands. |
| `keymap.set` | `bindings`: optional map [of list[text], at most 256], `pinned`: optional list [of text, at most 256] | `c` / panels | Help/Keymap/Set | `saved`: `boolean` | `keymap`, `state`, `done` | Replace the bindings or the pinned commands given (what is left out stays); the keymap is checked whole, then written. |
| `keymap.reset` | none | `c` / panels | Help/Keymap/Reset | `reset`: `boolean` | `keymap`, `state`, `done` | Remove the user's keymap: every command has its default keys. |
| `events.subscribe` | `events`: required list (series, plots, nullclines, dfield, marks, ani, autoinfo, autosettings, numerics, windows, player) [of choice, at most 256], `encoding`: optional choice (json, f32), `diagram`: optional boolean | `v` / panels | Help/Events/Subscribe | `subscribed`: `list[text]` | `state`, `done` | Send the heavy data events named (hello.features lists them) from now on, each at the end of this command; encoding f32 sends their value arrays as base64 float32 (what is left out stays as it was); diagram says whether AUTO's diagram points are sent. |
| `events.unsubscribe` | `events`: required list (series, plots, nullclines, dfield, marks, ani, autoinfo, autosettings, numerics, windows, player) [of choice, at most 256] | `v` / panels | Help/Events/Unsubscribe | `unsubscribed`: `list[text]` | `state`, `done` | Stop sending the data events named. |
| `tool.values` | none | `v` / tools | Data/Values |  | `done` | Show the parameters and initial conditions. |
| `tool.browser` | none | `v` / tools | Data/Browser |  | `done` | Open the stored data browser. |
| `tool.command_palette` | none | `v` / tools | Help/Command palette |  | `done` | Search every registered command by name, menu path and description. |
| `tool.keyboard_shortcuts` | none | `v` / tools | Help/Keyboard shortcuts |  | `done` | Edit command keys and pins. |
| `tool.manual` | none | `v` / tools | Help/Manual |  | `done` | Read the manual. |
| `tool.calculator` | `expression`: required expression (max 1024 bytes) | `v` / tools | Analysis/Calculator | `expression`: `expression`, `value`: `real` | `done` | Work out a formula of the model's names and functions. |
| `tool.run_source_action` | `index`: required integer [at least 0] | `d` / panels | Help/Tool/Run source action | `ran`: `boolean` | `state`, `done` | Run the action of an active comment of the model's source (a `{par=value,...}` block): comment `index` of session.get_source's list. |
| `tool.run_user_button` | `index`: required integer [at least 0] | `x` / panels | Help/Tool/Run user button | `ran`: `boolean` | `state`, `done` | Press a button of the model (an `@ button=name:keys` line), counted from 0 in the order the model gives them. |
<!-- registry:end -->

## Startup

1. `hello`: protocol version (4), `about` (Help > About's lines, from core/xpp_about.cpp, the same lines the desktop window's own About box shows: an array of lines, each an array of parts `{"text":...}`, with `"url"` too when the part is a link (`https:` or `mailto:`); the first line's text is `xppautX <version>`; a blank line is `[]`), `quit` (File > Quit's question as the core asks it, W110: `question`, `recording` (the question while a recording is in progress), `choices` (`["Save session","Don't save"]`) and `keys` (`sd`), for a client that asks it itself while a command runs: `quit` with `save` below), window title, the three main menus
   every command's kind and
   whether it is a step (`commands`): see "Action kinds"; the limits the
   core keeps to (`limits`) and the initial window names (`window_names`): see
   "Shared limits and ids"; the user's keymap (`keymap`: "Keymap").
2. `window` `create` for the named main graph.
3. `state`, then `done` (the answer to `session.get_state`, which the program runs first).

A client that draws sends `events.subscribe` next (see "The plot as data").

### Start screen (W232)

A program started with no file (the Start menu, the Dock, a launcher; a
folder instead of a file too) has a session with **no model**. `hello` then
carries `start`, and has no `window` or `state` after it (`idle` instead);
a `hello` of a loaded model has no `start`, so a client shows the start
screen exactly while `hello.start` is there. The core queues the same
File > Open model key that the page's button sends, so the Open model dialog
(a `file` ask, title `Open model`, `wild` `*.ode *.odex *.snapx *.recx`) comes at
once; answering it with a file loads that model (a new `hello`), and
cancelling it leaves the start screen: no error, the program keeps running.

`start`: `{"path": the preferences.setx file ("" when there is no config folder),
"limit": how many models are kept, "recent": [{"path": absolute path,
"missing": the file is no longer there}, ...], "examples": {"folder": ...,
"names": [...], "reason": ...}}`, newest first. A recent model
that is missing is listed as missing, never dropped; opening it is an
ordinary `session.open_model` that fails with the file named. When `preferences.setx` cannot be
read (a relative or repeated path, a control character, a path or a file
longer than the limits, more entries than `limit`, any other bad value), `recent` is `[]` and
`start` carries the common error fields (`error`, `file`, `line`, `col`,
`source`, `field`: "Errors"); nothing of the file is used and the next Open
model leaves it as it is (it is shown once more, as an error, and the model
still opens). The file is the program's, in the per-user config folder
beside `keymap.json`, `preferences.setx` ("Preferences", below): its
`recent_models`, absolute paths newest first; a model is
put first when it is opened from Open model (the dialog, a path, a recent
entry), from the command line or by the OS (a `.ode` is listed by the `.odex`
it converts to), and not while a recording plays or in `--silent`.

`examples` (W233): the bundled models. `folder` is `examples/` beside the
program (found from the executable: Windows, Linux `/proc/self/exe`, macOS the
`.app`'s `Contents/Resources`; no other place is searched, so a build run from a
source checkout has none), `names` the `.odex` base names in it, sorted, at most
100, and `reason` (only when `names` is empty) says why: the program's folder is
unknown, there is no examples folder, or it holds no `.odex`. The page shows
the reason. `{"cmd":"session.open_model","example":"lecar.odex"}` opens one: the name must be
in `names` (a path, `..`, a link or any other name is the error "is not one of the
bundled examples"), the example is copied into `examples/` of the config folder
(an installed folder is read-only and a model writes beside itself), asking the
Replace question of every save when a copy is there (declining ends the command
with no change), a message says where the copy is, and the copy is opened by the
`session.open_model` below.

Open model has one owner, `session.open_model` (without a file, it asks
for one with the dialog), so the start screen's button, a recent entry
(`recent`, its place in the list), File > Open model and the OS's open-documents
event after the start all reach it. A session with no model refuses every other command with the error
"No model is open: open one first (Open model)", ending in `done` like any
command, except `session.quit` (no question: there is nothing to save), `run.abort`,
`help`, `answer`, `keymap.*` and `events.*` (accepted, nothing to send).
`--silent` with no file is the error "no model file was named", exit 1.

### A model that does not load

When the model does not load (a line the reader does not understand, a
formula that does not compile, a name given twice, a value an `@` option
refuses: a colour that is none, an unknown variable, a word where a
number goes, ... at the option's line, W119), the server sends one
`error` event instead of `hello` and exits with status 1 (W63c). The log
(stderr, the page's `log` event) says it once, at its place
(`file:line:col: what`, "Errors" below), after the warnings the load
logged about the lines before it, each at its own line; the event is
the same as values ("Errors"):

    {"ev":"error","error":"Premature end of expression\n-X+A*\n    ^\nERROR compiling X'","file":"bad.ode","line":3,"col":0,"source":"x'=-x+a*"}

- `file`: the file the problem is in, as the server was given it, or the
  file the model includes (`#include`) when the problem is there.
- `line`, `col`: where, both from 1; 0 when not known. An .ode problem
  has its line (a statement continued with `\` is at the line it starts
  at) and column 0; an .odex one has both. A problem of the whole model
  (no equations, too many boundary conditions) has line 0.
- `error`: what is wrong, the error and warning lines logged about that
  line, joined with `\n` (a formula and a caret under where it stops
  making sense are two of them).
- `source`: line `line` of `file` as written ("" when `line` is 0).

In browser and window mode the server keeps serving after the exit, so
the page gets the event (and `exit` after it) whenever it connects.
`xpp::load_model` (core/xpp_batch.h) returns the `xpp::Error` of a load
that fails.

### Errors

Every error is one value, `xpp::Error` (core/xpp_error.h, W140): what
failed and where, the place being the file, its line and column from 1
(0 when not known) and that line as written. It is reported one way
(`show_error`, core/xpp_ui.h; a load's by `load_model`), rendered one
way as text for the console, the log and `--silent` (`Error::text()`:
`file:line:col: what`, leaving out what is not known, `line N: what`
with no file), and sent as one shape: the `error` event (a model that
does not load, above), a `message` with `error`, and a failed `file`
event (`ok: 0`) carry the same five
fields, always all of them, and `field`:

    {"ev":"message","error":"it is for 3 parameters, the model has 12","file":"bad.par","line":1,"col":0,"source":"3   Number params","field":""}

A command line is checked before it runs (W493): text that is not valid JSON
is a `message` error `the command line is not valid JSON (at character N)`
(`is nested too deeply` and `has text after its value` the other two), and a
command's argument that is not the kind the command reads (a string or an
object where a number goes, a fraction where a whole number goes) is a
`message` error naming the command and the argument, `auto: argument "label"
must be a number, not "x"`; the command does not run. `true`, `false` and
`null` are accepted for a flag (1 and 0).

- `error`: what failed, as the user reads it (one line or several).
- `file`: the file it is in or about, as the command was given it (a
  name in the model's folder, or a path); "" when none.
- `line`, `col`: where in it, from 1; 0 when not known. A `file` with
  `line` 0 is one the command could not read (File/Import settings, a parameter
  or IC file, a table, AUTO's files, a session or recording, W118): web2
  offers to add it to the model's folder under its own name and run the
  command again.
- `source`: line `line` of `file` as written, when it could be read;
  "" otherwise.
- `field`: the value field the error is about, as the page keys it
  (`par:gca`, `ic:v`, `num:dt`, `bc:0`: the kind, then the name in lower
  case or the `index`), "" when it is about none. A `set` of several values
  that refuses one sends it (W131), so the page shows the error on that
  field and not on the first value's.
- `action` (only when the core offers one): what the user can do about it,
  `{"label":LABEL,"commands":[COMMAND...]}`, the commands ordinary ones a
  client sends in order when the user picks it (so a recording and the
  session record them as any other steps). W260: an integration a failed
  exact Jacobian stopped (an exact derivative not finite where the
  right-hand side is: `sqrt` or `log` at 0, `1/x` at 0, a power of a base
  at most 0) is the error at the equation's line, naming the variable, the
  time and the state, offered with `"label":"Switch to differences and
  continue"` and the commands
  `{"cmd":"set","kind":"num","name":"jacobian","text":"difference"}`,
  `{"cmd":"run.continue","until":END}`, END the end the run was going to (a run
  going forward only, Continue's own condition): the run goes on from where
  it stopped, with differences. Nothing switches without them. web2 shows
  the action as a button on the error. An equilibrium or the adjoint a failed
  exact Jacobian stopped is the same error, with no action yet. W275: an
  AUTO run one stopped (`AUTO stopped: ...`, at the equation's line, with
  the continuation parameter's value) ends as a Stop ends it: the last
  converged point is stored as an end point (EP) with its label, and
  `autoinfo`'s `stop.why` is `jacobian`. Its action has the same label and
  the commands `{"cmd":"set","kind":"num","name":"jacobian","text":"difference"}`,
  `{"cmd":"auto.run","kind":K,"label":N}`, K the run's kind, N the end point's label (no
  action when the run stored no point to restart from).

web2 shows `file`, `line` (and `col`) under the error's text in the
error dialog, with `source` and a caret under `col` when there is one,
and writes the error in Messages and the status line as the console
does (`file:line: what`).

Every error has its place (W140b; `tools/errorcheck.py` fails one made
without it):
- read from a file: the file and its line (a table file, a values file, an
  `.ani`, a data file a curve fit reads, AUTO's files, a session or a
  recording), or line 0 when it cannot be read at all;
- caused by a model line, when the model loads or when it runs: that line
  (a table file a `table` line names that cannot be read; a variable an
  equation makes NaN or out of bounds; a DAE that cannot be solved, at its
  `0=` line; a CVODE failure, at the equation of the variable its error
  test or corrector failed at);
- from a command (a protocol command, a key's computation): in a recording's Play the
  `.recx` and the step's line; a command the user gave has no file, and
  the error says which command (`where`, in the log).

### Our files: all or nothing

Loading one of xppautX's own files is one operation (W125, core/xpp_io.h
`read_lines`, its comment listing every reader): the whole file is read
into a value and every line and value checked, then the value is applied
in one step, or, at the first line that is wrong, nothing is applied and
the error names the file, the line and the line as written. The same for
a settings file (`.setx`: File/Import settings, `--setfile`; an XPPAUT `.set` is converted to one), a parameter file (`.par`)
and an initial-conditions file (`.ic`: `value.load_file`, `--parfile`,
`--icfile`, Initialconds/File), a session file's members (`.snapx`, "Session
files"; the members are settings files too), and a recording (`.recx`, "Playing a recording"); a model's
internal set (File/Apply set, `set.apply`) checks every item before it applies one,
a bad item an error at the `set` line. Strict: a line that is not one of
the file's, a key given twice or missing, a number that does not read
(`1e999` too), a value out of range (a method's or a Poincare map's
number, a boundary condition longer than its room) and a line after the
last the file holds all refuse the file; a file of ours is never read in
part. A file that cannot be read at all is the error with line 0 (above).

User text is quoted in every such file and a choice never is (W490): a
name, a label, a title, a key, a path or a formula the user wrote (a
window's or a curve's name, a recording's model, a set's name in a `.odex`:
`set "hopf" { ... }`) is a string in double quotes even when it is one word,
and the reader refuses it bare, at its line and column; a choice, a number,
`true` or `false` and a name of the model's own (`method: runge_kutta`,
`xp: v`) are bare words, and the reader refuses one in quotes. The protocol's
JSON strings need no such rule: a string is always a string.

## The command rows of hello

`hello.commands` is the one list of commands: every registry row of the "Typed command registry" above
(`arguments` is present even when empty) and the few protocol lines that are not registry commands
(`answer`, `file`, `set`, `key`). The page's menu bar, command palette, argument forms, quick-access toolbar,
keymap editor and keyboard help are all made from these rows; nothing on the page lists a command by hand.
A registry row has:

| field | meaning |
|---|---|
| `cmd` | the command's name (`run.initial`): what the protocol, recordings, the keymap's bindings and pins call it; never renamed |
| `menu_path` | where the page's menu bar places it: one of File, Run, Analysis, Plot, Data, AUTO, Windows or Help, then the submenus, then the item (`File/Save session`); the palette matches it too |
| `description` | one sentence |
| `category` | `run`, `files`, `analysis`, `plot`, `tools` or `panels` (the commands the page opens from a panel or the title bar: Source, the Values panel's sets, Keyboard shortcuts, Record and Play) |
| `kind` | `c`, `v`, `s`, `d` or `x`: what the command needs ("Action kinds") |
| `default_keys` | the key that runs it (`"Ctrl+O"`: Ctrl, or Cmd on macOS); only the frequent commands have one. A key is one key press, never a sequence; any command can be bound to a key in the keymap |
| `arguments`, `groups`, `result`, `events`, `file_mode`, `file_wild` | the typed arguments the common form is made from, what the result holds, the events the command sends, and the file picker's mode and filter |

A command with no arguments runs at once; one with arguments opens one form, the same for every command
(each argument by its type: a choice a select with its names' labels, a boolean a checkbox, a window a pick of
the open windows, a name a pick of the model's names, a file the picker, a colour the colour field, a list or a map rows).

## Preferences

`preferences.setx` (W287, [#342](https://github.com/MuhammadMoustafa/xppautX/issues/342),
core/xpp_prefs.h) is the program's own file for what it keeps for the user
across runs, in the per-user config folder (`XPP_CONFIG_DIR` overrides it)
beside `keymap.json`, in the `.setx` syntax (docs/odex.md ".setx"). It is not
a model's setting: nothing a run computes depends on it. A missing file is the
defaults; a bad one (a key that is not a preference, a value of the wrong
kind, a list or path past its limit, a file past 256 KiB) is an error at its
file, line and value and none of it is used; it is read whole and written
whole, temp then rename, and never overwritten while it is bad. With no
config folder (no `HOME` or `APPDATA`, or a relative `XPP_CONFIG_DIR`) the
error names the variable to set. Its keys: `recent_models` (a list of at most
ten absolute paths, newest first: "Start screen");
`auto_threads` (`auto` or a whole number from 1 to the logical processors:
AUTO's thread count, "AUTO's settings as data"). A later group is one
member of the Preferences struct and one line of the description that reads
and writes it.

## Keymap

`keymap.json` (W211, #265, core/xpp_keymap.h) holds the user's own differences
from the command table's keys, nothing else, in the per-user config folder:
`%APPDATA%\xppautX` on Windows, `$XDG_CONFIG_HOME/xppautX` or
`~/.config/xppautX` on Linux, `~/Library/Application Support/xppautX` on
macOS. `XPP_CONFIG_DIR` names the folder instead (tests and CI use it so they
never touch the real file). That fixed path is the only one: nothing the file
says names another. The folder is made by the first save. The window and the
browser page both read and write it through the `keymap.*` commands, so they
never disagree.

    {
      "pinned": ["session.reload_model", "tool.calculator"],
      "bindings": {
        "session.reload_model": ["Ctrl+B", "F9"],
        "session.save": []
      }
    }

- `pinned`: the quick-access commands, in order; each a command whose `pinnable` is true, none twice.
- `bindings`: a registry command name of `hello.commands` to its keys, which replace the table's `default_keys`; `[]` is no key. A command not named keeps the table's keys.

A key is one key press, never a sequence or chord (W536: no letter sequences
and no XPPAUT key preset; a key with a space is refused): the
modifiers in the order `Ctrl+`, `Alt+`, `Shift+`, `Meta+` and then one key: a
capital letter, a digit or other printable ASCII symbol, `F1` to `F24`, or
`Enter`, `Escape`, `Tab`, `Backspace`, `Delete`, `Insert`, `Home`, `End`,
`PageUp`, `PageDown`, `ArrowLeft`, `ArrowRight`, `ArrowUp`, `ArrowDown` or
`Space`. One spelling per key (`Ctrl+Shift+S`, never `shift+ctrl+s`). A key
has at most 64 bytes, a command at most 8 keys, the file 65536 bytes, its
JSON 64 levels.

**Loading is all or nothing.** The whole file is parsed and every value
checked, then it is applied in one step; a file with any of these is an error,
shown with its file, line, column and the line as written, and nothing of it is
applied: not valid JSON, text after the value, nesting too deep, a NUL byte,
a file over the size limit, an unknown top-level setting, a setting or a
command twice, an unknown command id or a shortcut-layer row, a value of the
wrong type, a malformed key, a key with a space (a sequence), a reserved key, a command
with too many keys, two commands on one key (refused with both commands named), a pinned command twice or not pinnable, a `preset` (an unknown setting).
The effective map is checked: the keys of the commands the file leaves alone are
the table's. A missing file is all defaults and no error; a bad one never
falls back silently.

**Reserved keys** are the page's and the system's: `Alt+F4`, `Ctrl+W`,
`Ctrl+Q`, `F11`, `F12` and the browser's tab, window, address and reload keys
(`hello.keymap.reserved`, core/xpp_keymap.h). No command is bound to one and
the page never calls `preventDefault` on them.

`hello.keymap` and the `keymap` event's `keymap` are one object, the load's
result as the page keeps it:

| field | meaning |
|---|---|
| `ok` | false when the file could not be loaded |
| `error`, `file`, `line`, `col`, `source`, `field` | only when `ok` is false: the common error shape ("Errors"), at the file's line (an unreadable or oversized file at no line) |
| `path` | the settings file ("" when the system names no config folder) |
| `pinned` | the user's (`[]` when `ok` is false) |
| `reserved` | the keys no command may take |
| `limits` | `keys` (per command), `key_bytes` |
| `commands` | [{`id`, `keys`, `source`}...] for every command but the shortcut-layer rows: the effective keys, and whether they are the table's (`default`) or the user's (`user`) |

When the file is bad, the page gets that error in `keymap` (in `hello`, and in
the `keymap` event `keymap.get` sends) and `commands` are the table's defaults with `source`
`default`: the defaults are what the page shows while the error is up, marked
by `ok: false`, never presented as the user's; no `message` event is sent for a
load (the page shows the error once, from `keymap`). `set` replaces a bad file
whole; `keymap.reset` removes it.

`keymap.set`'s `bindings` and `pinned` are the parts of the user
map as the file holds it; a part left out stays as it is. The whole map is
checked by the same pipeline as the file, and an error is the `done` error
whose `file` is `the keymap sent` and whose line and `source` are in the map
as written out, the file untouched. The file is written through
`xpp::Writer` (a temporary file beside it, then a rename), one binding to a
line. A `keymap.set` or `keymap.reset` that cannot write or remove the file
is the `done` error naming it.

## Commands (client to server)

### Run, initial conditions and boundary values (W521)

The run, initial-condition and boundary-value operations are registry
commands (the generated table above lists their arguments, results and
events); the old forms (`continue`, `steady`, `abort`, `set` of an initial condition
and of the numerics the commands took) are gone. Navigation keys resolve through the keymap to registry commands, and the page's
argument form supplies typed values. Imported XPPAUT user buttons call their
leaf operations through `tool.run_user_button`. A run's `done` says
`completed` or `ended_early` (an event's `arret`, a section that stops it,
`/`) with the rows stored and the time reached; an error ends it as an
error with its place, and a stop (`run.abort`, Escape) as `stopped`.

- `run.initial`, `run.last`, `run.old`, `run.shift`, `run.backward`: a
  trajectory from the initial values, the last state (which overwrites the
  initial values; `reset_integrals` starts an integral-equation method's
  integrals afresh, else they go on), the initial values as they stand (the
  DAE guesses are not refreshed), the last state with the clock going on, and
  the initial values backward in time. `run.last` and `run.shift` need a prior
  solution. No run while the data table shows a Fourier or histogram result.
- `run.continue` (`until` or `extra`, at most one; neither: another Total)
  appends from the current core time. The additional duration must be
  positive, with finite times and counts; it requires a prior trajectory and
  a positive Dt. Continue honours Store every N steps: the end is rounded up
  to the output grid (Dt times N; N times 1 for a map; a value within 1e-6 of
  a grid interval of a grid point counts as on it) and is at least one
  interval after the current time; the end reached is `end_time` and the
  `time` of the `state` that follows. `hello.continue.grid_tolerance` carries
  that 1e-6.
- `run.steady` (`decimals` 0 to 15, `hold`, `maximum`, all required)
  starts a new trajectory from Initial. Hold is at least one positive Dt,
  maximum at least hold. Ordinary trajectory output is required (no
  Poincare, histogram or FFT). All state doubles are compared rounded to
  fixed decimal places at every configured Dt; changed digits reset the hold.
  Store every N steps is temporarily 1, then restored along with Total.
  `hello.steady` supplies `max_decimals`, `default_decimals` and
  `default_hold`. The result and `state.steady` have `status` (`settled`,
  `limit`, `stopped`, `failed`), `decimals` and `time`. A steady run is not
  bounded by Total storage: when the store is full it keeps every second row.
  `settled` is not an equilibrium certificate.
- `run.range` and `run.double_range`: one run for each value of a parameter
  or variable (two: together, or each with each); every argument left out
  takes the session's current range setting (the model's `sweep` options
  made it), so `{"cmd":"run.range"}` is the range the options describe.
  `done` says `runs_completed` (`grid_points`) and the rows (`total_steps`)
  they stored.
- `run.dae_guess` sets the initial guesses of the algebraic variables from
  formulas (`guesses`, a map by name; none given keeps them) and returns the
  values now used. `run.shoot` (`manifold_index`, from 1) runs from a
  manifold the equilibria's shooting found.
- `run.configure` sets the integration's numerics, all given at once and all
  or nothing (an order the settings keep, as `min_step` at most `max_step`,
  is judged on the result): `total_time`, `start_time`, `store_from`, `dt`,
  `method` (the key of the method table), `store_every`, `state_limit`,
  `stop_at_limit`, `delay`, `relative_tolerance`, `absolute_tolerance`,
  `min_step`, `max_step`, `eul_tol`, `eul_iter`; and, named as the .odex
  options name them, the method's own: `vmaxpts` and `autoeval` (the Volterra
  method), `bandlo` and `bandup` (CVODE: giving one makes the system banded),
  `delay_real_guess`, `delay_imaginary_guess` and `delay_grid` (a model with
  delays). One that does not apply to the method or the model is an error
  naming it, judged after the call's `method` is set, and takes the whole call
  back. It is one value-undo step, and `set` of `kind` `num` refuses the keys
  above (it keeps the other numerics).
- `ic.set` (`values`, a map by state name) sets initial values; `ic.formula`
  (`variable`, `formula`) sets one from a formula, or an array range
  `u[1..5]` from a formula reading `[j]` as the element's number;
  `ic.array` (`name`, `size`, `formula` of `t`, the offset) sets `size`
  states from `name` on; `ic.load` (`file`) reads a file of the model's folder
  (one number per differential equation), all or nothing;
  `ic.from_data_row` (`row`) makes a stored row the initial values. None
  runs anything. `set` refuses `kind` `ic`.
- `ic.at` (`coordinates`, a map by the two states the graph `window` plots)
  and `ic.mouse_stream` (`x` and `y`, the clicks' coordinates as lists, at
  most 256) run from the points in plot coordinates (Initialconds' Mouse and
  mIce); the first run empties the store, mIce's runs add up and ignore the
  state limit. A graph that does not plot two states against each other is
  an error.
- `bvp.solve` (`show`), `bvp.solve_periodic` (`frequency_parameter`,
  `section_variable`, `section`, `show`), `bvp.solve_range` (`parameter`,
  `start`, `end`, `steps`, `cycle_color`, `side` `start` or `end`, `movie`;
  left out: the session's) and `bvp.configure` (`max_iterates`, `tolerance`,
  `epsilon`). A failed shoot is the error, with the iterate it failed at.
- The `analysis.*` commands (W525) analyse the stored data and set what the
  analyses use; an argument left out keeps the session's setting, and a refusal
  says what the data or the setting lacks. `analysis.histogram` (`column`,
  `bins`, `low`, `high`, `condition`), `histogram_2d`, `autocorrelation`,
  `cross_correlation` (`lags`, `method` `direct` or `fft`), `spectral_density`
  (`type` `psd_x`, `psd_xy` or `coherence_xy`; `window_length` 2 or more and at
  most the stored rows; `window` `square`, `parzen`, `hamming`, `bartlett` or
  `hann`), `fourier_series`, `power_spectrum` and `statistics` read a `column`
  (a name, `T` included), show their result in the data browser (not
  `statistics`) and say how many bins, lags, points or harmonics it has;
  `reload_histogram`, `load_stochastic_run`, `load_stochastic_mean`,
  `load_stochastic_variance`, `adjoint_load`, `adjoint_load_orbit` and
  `adjoint_load_interaction` show a data set again, an error when there is none.
  `stochastic_integrate` (`trials`, the runs: the Range settings' steps plus one)
  keeps the runs' mean and variance. `fit_curve` fits `parameters` (and initial
  values) so that `variables` follow `data_columns` (the file's column numbers,
  2 or more) of `file`, a file of the model's folder with `points` rows of
  `columns` numbers; it says whether it `converged` and the values found, which
  stay set. `lyapunov_exponent` takes `range_over_parameters` and then
  `run.range`'s arguments. `adjoint_compute` needs a stored periodic orbit,
  `adjoint_interaction` the `couplings` by differential equation (an equation left
  out keeps its coupling), `adjoint_configure` the iterations and tolerance and
  `adjoint_range` whether the next range also computes the adjoint.
  `poincare_map` (`type` `none`, `section`, `max_min` or `period`; `variable`,
  `section`, `direction` `negative`, `both` or `positive`, `stop_on_section`),
  `phase_space_torus` (`mode` `all`, `none` or `choose`, `variables`, `period`
  above 0) and `color_coding` (`mode` `none`, `velocity` or `variable`,
  `variable`, `low` and `high` together, a graph `window`; without limits the
  range of the stored data) set the Poincare map, the folded variables and a
  graph's coloring. `table.edit` (`name`; for a table of a formula `formula`,
  `points`, `x_min`, `x_max`, `auto_evaluate`, for a table of a file `file`) and
  `table.view_in_browser` act on the model's lookup tables. The model's
  `@ postprocess` is the analysis command it names; the `browser` command no
  longer takes `postprocess`.
- `run.abort` stops the running command where it is; the reader thread acts on
  it the moment it arrives, so it has no `done` of its own (the stopped
  command ends with `stopped` and `done` status `stopped`); outside a command
  it does nothing. A recorded interruption belongs to a recording step's
  `run.abort`, whose position the player arms before the computation.

A run's files (a range's one file for each run, `--ncdraw` and `--dfdraw`'s)
ask before replacing one that is there, as any save does: a registry command
takes no `replace` field, the client answers the question.

`state.time` is the full-precision current core time, independent of stored
float32 plot time. After a steady run, `state.steady` contains `status`,
`decimals` and `time`. Status is `settled` (unchanged digits for the hold),
`limit`, `stopped`, or `failed`. A steady run is not bounded by Total
storage: when the store is full it keeps every second row and stores every
second event from then on, so it runs until it settles, reaches its
maximum, is stopped or fails. This transient result is
cleared when another ordinary integration starts. `settled` is not an
analytical equilibrium/stability certificate. Adaptive solver internal
steps are not comparison intervals.

| cmd | fields | meaning |
|---|---|---|
| `key` | `key` | A key typed as the keymap names it (`"Ctrl+R"`, `"Escape"`: "Keymap"): the core resolves it through the user's keymap and runs the registry command bound to it (recorded as that command). A key is one key press; a key no command has is an error. The page does the same locally and sends the command. |
| `answer` | `id`, `ok` (0/1), plus the kind's fields | Reply to an `ask`. Omitting `ok` means ok. Omitting `id` answers whichever `ask` is currently pending (a client or the recording player can answer without tracking prompt ids). |
| `set` | `kind` (`bc`, `delay`, `num`), `name` or `index`, `value` or `text` | Change a boundary condition, a delay or a numerics value no registry command takes yet (no redraw or run: W69). A setting ("Action kinds"): sent during a computation it applies when that ends, never to the run in progress ("Commands during a command"). `text` is what the X11 box takes: an expression for `bc` and `delay`; BCs and delays go by `index` (BC names all read `0=`). `num` sets a main numerics field by its key (`name`: `nullcline_mesh`, ...; "The numerics as data"; the run's and the shooting's are `run.configure`'s and `bvp.configure`'s). A numerics value refused is a `message` `error` naming the field (`Numerics: ...`); so is what the command cannot take, nothing set (W116): a `kind` it does not have (`set takes kind delay, bc or num, not "parm"`), a `par` or `ic` (`set no longer takes parameters: send value.set_parameters`), an `index` outside the list, no `value` that is a number and no `text`. Several values in one command: `values` [{`kind`, `name` or `index`, `value` or `text`}...]: all or nothing (W131), every value checked first, the one `message` `error` naming the value that is refused and its `field`. |
| `file` | `op` (`list`, `get`, `put`), `name`, `data` | The model's folder (the working directory) for a client that cannot reach it: `put` writes `data` (base64, at most 64 MB decoded) as `name`, `get` reads `name` back, `list` lists the folder. Answered with a `file` event, then `state` and `idle`. Names are base names only (see "Files" below). |

### Session, recording, keymap, events and tools (W528)

File > Open model, Reload, Open session, the saves, Import settings, Export
simulation information, Model source, Quit and the state; the recording and
the player; the user's keymap; the heavy data events a client subscribes to;
and the calculator, a source comment's action and a model button are
registry commands (the generated table above lists their arguments, results
and events: `session.*`, `recording.*`, `keymap.*`, `events.*`, `tool.*`);
the old forms (`open`, `reload`, `session`, `record`, `play`, `keymap`,
`data`, `equations`, `state`, `quit`, `action`, `userbut`) are gone and are
refused as unknown commands. The File menu's keys ask for what they need as
before and call the same core functions, so a key, a menu item and the
command do one thing. What the rows leave to say:

- `session.open_model` takes a path (`--server`'s own input only; the page
  and a recording never give one), an example's name or a recent model's
  place in `hello.start.recent` (the page's start screen). The File menu's
  key and the page's Open model button ask for the file with a `file` ask
  instead. It asks first whether to save this session (a `choice` ask, keys
  `sd`; a cancel keeps the session). A `.snapx` loads its saved model, then
  its whole session; a `.recx` is opened with `recording.open`. The result
  names the model now loaded, its variables and its parameters; a model
  that does not load is the `done` error naming the file and line, and the
  session before is kept. On success all windows but the main one are
  destroyed, and a new `hello`, `state` and first-start events follow; the
  client subscribes with `events.subscribe` again. `session.reload_model`
  loads the file again with the same questions and keeps the parameters,
  initial data and numerics by name.
- `session.save` saves to the file the session was last saved to or opened
  from (`state.session.file`), asking nothing; with none it is refused and
  says so (the File menu's Ctrl+S asks for a file then, as `session.save_as`
  does). `session.save_as` and `session.save_copy` take a file of the model's
  folder (`.snapx` added); `confirm` true replaces an existing file without
  asking, left out asks before replacing it. A copy leaves
  `state.session` and `state.changed` as they were. `data` false leaves the
  data table out, true puts it in; left out, it goes in unless it is above
  50 MB, when a `choice` ask (keys `l` leave it out, `s` save it) decides.
- `session.quit` with `save` false ends the session at once, even during a
  computation, asking nothing (a script's and a client's quit; the window's
  close sends it). Left out, it is the user's quit: it stops a computation in
  progress, then asks "Quit xppautX? Save this session first?" (keys `sd`;
  a cancel keeps the session); `save` true is that question answered Save
  session by a client that asked it itself (the page, while a command runs):
  it stops the computation, saves the session and a recording in progress,
  then says `bye` and exits. A quit that saves or asks is a command of its
  own and ends with `state` and `done` only when it is cancelled.
- `session.get_state` answers the `state` members as its result (and sends
  the `state` event); `session.get_source` sends `source` and `equations`
  and answers with the source text and the equation lines. Both, and the
  player's controls (`recording.play`, `recording.pause`, `recording.step`,
  `recording.set_speed`), are answered at once, even during a computation,
  each with its own `done`.
- `recording.start`, `recording.stop`, `recording.add_note` and the player
  commands work as "Recordings" and "Playing a recording" below describe:
  `recording.stop` without `file` asks for one; `recording.open` takes a
  `.recx` path (`--server`'s own input only; the File menu's key asks for the
  file) and `play` says whether it starts at once; `recording.seek` runs the
  steps before `step` at once and plays on when `play` is true; a note may
  hold line breaks (at most 4096 bytes).
- `keymap.get` reads the user's `keymap.json`, `keymap.set` replaces the
  bindings or the pinned commands given (what is left out
  stays; the keymap is checked whole, then written) and `keymap.reset`
  removes the file; each sends the `keymap` event ("Keymap") before its
  `done`. The user's settings, not the model's: control commands, taken any
  time, never a recording's step.
- `events.subscribe` adds the named events to those the client already gets
  (`encoding` `f32` sends their arrays as base64 float32; `diagram` says
  whether AUTO's diagram points are sent; left out, each stays as it was) and
  sends each at once, at the end of the command; `events.unsubscribe` stops
  the named ones. The names are `hello.features`: `series`, `plots`,
  `nullclines`, `dfield`, `marks`, `ani`, `autoinfo`, `autosettings`,
  `numerics`, `windows` and `player` (see their sections below).
- `tool.run_source_action` runs the action of comment `index` of
  `source.comments`; `tool.run_user_button` presses button `index` of the
  model (`hello.userbuttons`).

## Action kinds

Every action has a kind (W95), defined once in the core and sent in
`hello`, so a client knows what it may still do while a computation runs
(see "Commands during a command"). A kind is one letter:

| kind | letter | what | during a computation |
|---|---|---|---|
| control | `c` | Abort, Quit, an answer | acted on |
| view | `v` | only changes what is shown: a zoom, a window picked, the data subscription, a menu that shows, Help | kept, and run after the computation |
| setting | `s` | a value the next computation uses (W106): a parameter, an initial or boundary condition, a delay, the numerics (`set`, `value.set_parameters`, `value.slide`, `set.apply`, `set.restore_defaults`, `value.load_file`, the Parameters key, the Numerics menu's items that ask a value, File/Apply set, File/Restore defaults), AUTO's Numerics, axes, Mark values and threads (`auto.configure_numerics`, `auto.configure_plot`, `auto.configure_marks`, `auto.set_threads`, the AUTO window's keys `n`, `u`) | taken at once, applied when the computation ends: the run in progress keeps the values it started with |
| data | `d` | saves and loads: Save/Load values' files, Import settings, Save values into the model file (`set.save_new`, `set.save_into`, `set.save_defaults`, `set.export`, `set.rename`, `set.delete`), every file written (`value.save_file`, `browser.export`), a session, AUTO's diagram files, AUTO's Grab (the curve is still changing) | refused |
| computation | `x` | starts one: Initialconds, Continue, Range, AUTO's Run, Nullclines, Dir.field and Flow, Sing pts, Stochastic, a user button | refused |

- Each row of `hello.commands` has a `kind` (one letter): its registry row's (core/command_def.h). The kinds of the pop-up
  lists a command asks a choice with are in core/menus.cpp (checked when it compiles), for the core itself.
- `hello.commands` is every command, [{`cmd`, `op`, `kind`, `step`}...]:
  an entry with `op` is for that op, the entry without for the command's
  other lines (`browser.page` is a view, `browser.export` data); `key`'s
  `kind` is `""` (a key's is its menu item's). `step` says whether the
  command is a step of the user's: what a recording records ("Recordings")
  and what web2 runs again when the file it could not read has been added
  (`message` `file`); the client's own requests (`session.get_state`, `events.*`,
  `session.get_source`, `file`, the browser's paging), answers and
  aborts, and the recording's and player's own commands are not. It is
  core/ui_json.cpp's command table, the one handle_line dispatches from:
  every command of the table above is in it.

### Shared limits and ids

What a client must keep to, from the core that keeps to it (W118):

- `hello.player_speed`: `{ "min": 0.25, "max": 8 }`, the core's playback speed bounds. The page derives its powers-of-two speed buttons from these bounds; the core clamps requested speeds to this range.
- `hello.upload_error`: the core's upload-size refusal wording (`larger than 64 MB`), also used by the page before uploading.
- `hello.limits`: `upload`, the largest file the model's folder takes
  (bytes, xpp_files.h XPP_FILES_CAP, 64 MB); `browser_rows` and
  `browser_cols`, the most rows and columns one `browser` block request
  gets (2000 and 500).
- `hello.text`: what a plot text takes (core/grobs.h `LABEL_*`): `max_length` (1000 characters), `size_max` (4) and `default_size` (2); `hello.text_styles` lists the styles by name (`normal`, `bold`, `italic`, `bold_italic`).
- `hello.annotation_glyphs` (W513, #577): how the pictures draw an annotation, decided once in the core (core/grobs.cpp), for the page to draw as they do: `markers` the strokes of each marker `shape` (a list of polylines of `[x, y]` in glyph units, y up, centred on the marker's position), `half_units` the units from a marker's centre to the edge of its box at size 1 (the page maps them to its own pixels), and `arrow_head_spread` how far the two strokes of an arrow head spread either side, as a part of their length.
- `hello.line_styles` (W492, #556): how a curve's style is drawn, decided once in the core (core/picture.cpp): `dash` the pattern of each dash word at a width of 1 (a wider line multiplies it by its width, at least 1; empty: solid) and `markers` the glyph of each marker word at a size of 1 (a size s multiplies it by max(s, 0.5)): `circle` the radius of a filled circle (0 for none), `polygon` the corners of a filled polygon, `strokes` its line segments, in pixels with y down. The page draws a curve's markers and dashes from it as the PDF and SVG do.
- `hello.colormaps`: the colour scales the core has, by name (the order of core/colormap.h `colormap_names`); `plot.set_colormap` changes the session's.
- `hello.window_names`: names for the main graph, initial AUTO view,
  animation, array-plot and data browser surfaces (`main`, `auto`, `ani`,
  `aplot`, `browser`): the last three are fixed (`animation`, `array_plot_1`,
  `browser`) and no graph or view may take them.
  `plots.windows` and diagram `views.names` list the actual open windows.
  Names are strings in events, asks and commands; there are no numeric
  window handles on the wire.


The core says when the running command begins computing: `computing`
(once per command, before its first `progress`; its `idle` ends it). A
client disables data and computation actions from then until that
`idle`, and nothing merely because a command it sent itself is running.
Settings stay enabled throughout: an edit is sent at once, and the core
applies it when it can (below).

## Annotations

The things a user places on a plot window: texts, arrows, pointers and markers (W513, #577). Each has an `id`, one counter for all four kinds, that stays its own for the session's life and its session file: a deleted annotation's id is never used again. A session holds at most 100000 texts and 100000 arrows, pointers and markers (a session file with more is refused at its line). The `annotation` command works by id, in the plot's coordinates, with no click:

- `annotation.list`: `window`. The result has `window`, `text`, `arrows` and `markers`: every annotation of the graph with its `id`.
- `annotation.add_text`: `window`, `text`, `x`, `y`, `text_size` (2), `style` (`normal`), `color` (`foreground`). Adds a text; the result is its `id`. A text starting with `%` is stored as its rest with each `\{expr}` filled in once.
- `annotation.add_arrow`, `annotation.add_pointer`: `window`, `tip_x`, `tip_y`, `tail_x`, `tail_y`, `size` (0.1: the head's length as a part of the tip-to-tail distance), `color`. Adds an arrow (a head) or a pointer (a head and a shaft); the result is its `id`.
- `annotation.add_marker`: `window`, `shape` (`box`, `diamond`, `triangle`, `plus`, `cross` or `circle`), `x`, `y`, `size` (1), `color`. Adds a marker; the result is its `id`.
- `annotation.edit`: `annotation_id`, and what changes (an argument left out stays): a text's `text`, `text_size`, `style`, `color`; an arrow's or pointer's `size`, `color`; a marker's `shape`, `size`, `color`.
- `annotation.move`: `annotation_id`, `new_x`, `new_y`. Puts a text, or an arrow's, pointer's or marker's tip, there; an arrow's or pointer's tail goes the same way.
- `annotation.delete`: `annotation_id`. `annotation.delete_all`: `window`, every annotation of the graph.

The argument is `annotation_id`, not `id`, because `id` is the request's own correlation id. All or nothing: a `window` that is no graph, an `annotation_id` no annotation has, a text that is empty or longer than `hello.text.max_length`, a `text_size` outside `hello.text`'s range or a `style` that is not one of `hello.text_styles`, an arrow whose tip is its tail, or an add when the session holds 100000 of that kind is an error and nothing changes. The result of a change is also the `marks` event of the graph (for a client that asked for it with `data`). Text,etc's Edit menu (move, change, delete the annotation nearest a click) calls the same functions.

## Plot settings

A graph's curves, frozen curves, axes and exports are the `plot.*` commands of the table above (W524, #588): each takes the graph by its `window` name (the selected one when it is left out), is one command in every front end (the Curves, Axes and Numerics menus' forms call the same core functions), and ends with `done` and its result. An argument left out keeps the setting the graph has (a new curve starts from the first curve's columns and look, a frozen curve from the default name and key, a marker from the last markers' settings). All or nothing: a column that is no stored data column, a limit not below its upper one, a width not above 0, a legend `key` over 19 bytes or a `name` over 9, a `slot` the graph has no frozen curve in, a `curve_index` it has no curve at (the first curve is the graph's own and cannot be deleted), a 3D setting on a 2D graph, markers that reach past the stored rows, an unknown colour, line style word or choice, or a file name that is not a plain name in the model's folder is an error and nothing changes. `plot.set_variables` takes the model's states by name: `time_series` plots each against time, `phase_plane` two or three (the first on the x axis), `array_plot` from the first of two to the later. The exports (`plot.export_pdf`, `plot.export_svg`, `plot.export_data`, `plot.import_bifurcation`) write and read files of the model's folder; an existing file asks the replace question as every save does ("Saving data"). `plot.export_data` writes the curves and frozen curves of the graph as the columns `curve`, `x`, `y` (and `z` in 3D) in the data format named by `format` or by the file's extension. The old `colormap` and `plotvars` commands are gone: `plot.set_colormap` and `plot.set_variables` replace them.

## Sets, values and the data browser

The model's named sets, the values (parameters, sliders, .par and .ic files) and the data browser's table are the `set.*`, `value.*` and `browser.*` commands of the table above (W526, #590); each is one command in every front end. Each ask what it needs, then calls the same core functions.

- **`set.*`**: `set.apply` applies a set of the model as a patch over the current values, limited to `scope`: `all`, `parameters`, `initial` (the states) or one of the groups a set holds (`integration`, `poincare`, `equilibria`, `continuation`, `plot`, `sweep`, `analysis`; docs/odex.md); `window` is the graph the plot settings go to (the active one when left out) and only a scope of `all` or `plot` takes one; a parameter-only apply leaves the initial values alone; all or nothing, one undo step. `set.save_new` appends `set "NAME" { ... }` to the model file with the current values of `scope`; `set.save_into` replaces that scope's part of one of the file's own sets (an imported set is read-only); `state` says whether the initial values saved are the initial ones (`initial`, what is left out means) or the last state reached (`last`). `set.save_defaults` writes the values as the model's own (its `par` and `init` lines, an `options` group), `set.export` as a set of another `.odex` file (a set of that name there asks before it is replaced), `set.rename` and `set.delete` change the file's own sets. Every save builds the new text, checks it parses, checks the file has not changed on disk since the load, replaces it atomically, and only then updates the live model; a refusal changes neither. `set.restore_defaults` puts the model's own values back for `scope` (or one `field` of it, not with `all`).
- **`value.*`**: `value.set_parameters` sets some parameters (a map of numbers, all or nothing, one undo step) and `value.set_by_formula` one parameter to what a formula works out to now; `value.undo` and `value.redo` are the Ctrl+Z and Ctrl+Y of value edits (an error when there is nothing to undo or redo); `value.slide` sets a parameter or a state's initial value, a drag's steps being one undo step. `value.configure_slider` defines a slider slot (an existing one or the next; `step` 0 lets the page pick one, left out keeps the slot's), `value.remove_slider` empties one. `value.save_file` and `value.load_file` write and read XPP's own `.par` (`kind` `parameters`) or `.ic` (`initial_conditions`) file in the model's folder; with no `file` the core asks for one (the desktop window's own dialog), and a question answered with Cancel ends the command with an empty `file`. A load is all or nothing, one undo step.
- **`browser.*`**: `browser.page` chooses the block of the table the client shows (`from_row` to `count` rows, 0 stopping the block's `browser` events; the columns after `T` from `first_column`, `columns` of them) and is answered at once, even during a computation, with the rows in its `done` and the `browser` event; a `from_row` past the data is the last row. `browser.find` selects the row nearest a value, from the selected row on. `browser.set_slice_start` and `browser.set_slice_end` choose the rows First..Last that `browser.tabulate_column` and `browser.export` keep (the last inclusive; a first row not before the last, or a last row before the first, is refused); `browser.restore_full_slice` keeps all the data and replots it. `browser.add_column` adds a column computed from a formula of the others and recomputed after every run, `browser.delete_column` removes one (not one a graph plots), `browser.replace_column` replaces a column by a formula, `'&column'` (its running integral), `'@column'` (its derivative), `'a:b'` (from a to b) or `'a;b'` (from a in steps of b), and `browser.undo_replace` puts it back. `browser.transpose` shows the stored data on its side (`columns` columns from `first_column` in steps of `column_skip`, `rows` rows from `first_row` in steps of `row_skip`; every one it reads must be in the data, at most one fewer rows than the model has columns; what is left out keeps the last values). `browser.export` and `browser.load` write and read a data file ("Saving data" below; `target` `table`, `output` or `plot_curves`, `format` by name or by the file's extension, a missing file, target or format asked for as there). `browser.postprocess` runs the model's `@ postprocess` and shows its result. Getting a row as the initial conditions is `ic.from_data_row`.

A file the commands write is a base name in the model's folder; replacing one that is there asks (a `choice` ask), and a No is the error `... replacing it was declined: nothing was written`. A registry command carries no `replace` decision.

## Keys

`{"cmd":"key","key":"Ctrl+R"}` resolves the current keymap. Keys use
the keymap grammar (capital letters, canonical modifiers); a key is one key
press, never a sequence. The bound command runs as the command it is. `menu`,
`item`, `win`, `button` and `context` arguments are refused. The page resolves
the same effective bindings locally, opening the common form when arguments
are needed. Loaded custom bindings reject two commands on one key, naming
both commands and the key. The old window/menu key layers, the letter
sequences and the XPPAUT key preset are removed (W536).

## Saving data

The browser's `browser.export` (Save data) writes the data table or what the plot
shows in one of the registered data formats (core/data_formats.cpp, one line
per format):

| `format` | Extension | What is written |
|---|---|---|
| `dat` | `.dat` | XPP's own: a row per line, `%.8g` values each followed by a blank, no names (byte for byte what a batch run writes as output.dat) |
| `csv` | `.csv` | a header row of the column names, then a row per point, each stored float in the shortest text that reads back as it; `
` line ends |
| `csv.gz` | `.csv.gz` | that CSV, gzipped (one member, time stamp 0) |
| `npz` | `.npz` | NumPy's `numpy.savez_compressed` format: a zip of `.npy` files, one float64 array per column named after it (`T.npy`, `V.npy`, ...) |

`target` is `table` (the rows First..Last of every column: `T`, then the
browser's `cols`), `output` (those rows of the model's output columns: its
`only` list when it has one, else every column: what `--silent` writes as
output.dat) or `plot_curves` (the current plot window's curves, then its
frozen curves, as one long table `curve,x,y` (and `z` in 3D), one row per
point, the curves numbered from 1; in NPZ one (points, 2 or 3) array per
curve, `curve1`, `curve2`, ...). Graphic stuff > exp(O)rt (keys `g`, `o`)
is the same Save data with `target` `plot_curves`: it asks the format and the file. `file` is the file (a name ending in a
format's extension also chooses that format when `format` is not given).
What is not given is asked, in this order: `target` as a `choice` ask named
`save_what` (keys `t`, `p`), the format as a `choice` ask named
`save_format` (one item per registered format, in the table's order, keys
`d`, `c`, `g`, `n`), the name as a `file` ask whose `wild` is `*` and the
format's extension. Before the name is asked, an empty selection reports
one error, `Nothing to save`. An unknown `target` or `format` is refused naming the choices.

Default output names (W130, #182) use `xpp::files::output_name`: the model's
base without its last extension, then the output extension. A second kind
with that extension adds `-<what>`: `lecar.csv`, `lecar-curves.csv`,
`lecar-array.pdf`, `lecar-eigen1-unstable.dat`. `hello.output_names` sends
`par`, `ic`, `csv`, and `curves`, the complete defaults for the values
panel and the two CSV buttons. Clients use these names directly, including
on reconnect or model change; they do not derive names from `hello.file`.

Kinescope Save asks for its first GIF as a `file` ask; later frames each
ask for their own file (default `<first-base>-frameN.gif`). Make Anigif asks
for `<model>.gif`. Array print, table export, array range and animation
frame settings contain no filename field; names are separate `file` asks.
Fit's input file is also a separate read ask. Periodic-orbit, adjoint,
coupling-function and stable/unstable manifold outputs each ask for a
name. Cancel opens no writer. Invalid names and render types report an
error, with no fallback or truncation. Typed write answers are base names;
only the native dialog's authorized path answer can select another folder.
Live command errors identify the one-line virtual source `command:1` and
carry the command JSON in `source`; a recording uses its real step location.
Every committed `saved` event is delivered by the browser page, including
all files from a command that saves several outputs.

Every user save uses the same owner (W129, #181), including sessions,
recordings, pictures, Clone ODE, AUTO exports and the session beside an
imported XPPAUT set. Numeric `replace` on a command means `1` permits the
write/replacement, `-1` declines it, and `0` or omission leaves it undecided.
Other values are errors. With no decision, a new file is written without a
question; an existing destination asks one `choice`, keys `yn`,
`<file> exists. Replace it?`. No opens no temporary file.

The desktop's Windows, GTK and macOS save dialogs confirm replacement of
an existing file themselves. Their successful `file` answer carries
`{"cmd":"answer","id":N,"file":"<picked full path>","replace":1}`.
The core accepts this decision for a write ask and asks nothing again.
Recording stores the complete file answer unchanged; replay uses its
decision even when that destination now exists. Explicit decisions in key
commands still use the recording step's `cmd`, for saves without a file ask.
When the core makes a decision itself, recording stores an owner answer
`{"save_replace":1}` or `{"save_replace":-1}` in the step's `answers`, in
save order, instead of a conditional choice key. This includes permission
to create a new file. Play consumes these answers in the save owner, so a
browser/core No stays No even if its target is absent, and a recorded new
file needs no question if it now exists. These are recorded step answers,
not file-picker answers or additional prompts.

In browser mode, `showSaveFilePicker` confirms only its delivery destination.
The core's copy is a different file in the model's folder, which that picker
did not see, so the page's `file` answer carries no decision. The core asks
about that copy only if it exists. The download fallback uses the same core
rule. `--silent`'s built-in commands and clients with `replace:1` keep their
explicit authorization; clients needing disk-independent runs should carry
that decision rather than assume a conditional question will occur.

Each attempted save sends `{"ev":"saved","saved":true|false,"file":"<destination>"}`
before the command's `idle`. `true` means the complete temporary file was
committed to that destination. A decline, unsuccessful open, write, commit
or cancelled capture sends `false`. An empty save sends `false` with `file:""`
and its one error. A cancelled name ask starts no attempt. Failure errors
are shown once at the command's place, naming the destination in their text.
The page delivers only the matching successful destination (including a
suffix the core appended from the file dialog), at that command's
idle; an idle alone, No, or a failed commit never delivers an older file.

Array plot Range movies commit only after the sweep completes. A stopped
sweep, failed integration or cancelled pixel capture aborts the Writer and
sends `saved:false`, preserving any previous destination (W137).
The plot's Freeze > Import diagram file ask now uses wildcard `*`, so
`.dat` and `.csv` tables can both be selected; both require six columns
(x, low, high, type, branch, two-parameter flag). CSV headers are skipped.

`browser.load` asks for `file` (a `file` ask, `wild` `*`) when it is
not given and reads it as `format`, or else as the format of its extension,
or else as `.dat`; the file's columns fill the stored ones in order (a CSV's
header row skipped, a 2-D `.npy` giving one column per column), at most the
storage's rows.

## Commands during a command

`animation.grab_point` is a computation (its release may integrate). The
animation's steering commands (`animation.pause`, `animation.set_speed`,
`animation.faster`, `animation.slower`) taken during a job are retained in
arrival order and replayed at checkpoints; Pause ends Go at its recorded
frame. Each has its `done` when the running command ends, after it, in
the order sent. Pause while idle is a view step with nothing running to
stop.


Input is read on its own thread, so lines keep arriving while the core
computes. Every command runs as a *job*, numbered by the position of its line
in the input.

- `run.abort` and `session.quit` with `save` false (but a `session.quit` that asks or saves, below) act the moment they arrive: they cancel the running job
  and every job whose line came before theirs, even one still waiting its
  turn, and the computation stops at its next check (every integration step;
  AUTO between continuation points). So a `run.abort` sent right behind a
  command stops that command, however late the core gets to it, while a
  command sent after the `run.abort` runs normally. An answer to a prompt sent
  after a `run.abort` counts as the user's last word: the rest of that command
  is not cancelled. `session.quit` then exits. A `session.quit` that asks or saves (W59d, W110) cancels
  only a running computation, and waits its turn as an ordinary command:
  it asks, and exits only when the user says so.
- While a *computation* runs (an integration, a range of them, Sing pts, a
  boundary value problem, an AUTO run: what Escape stops; the `computing`
  event says it began), the server judges each line the moment it arrives
  by its kind ("Action kinds"; one list, core/ui_json.cpp `during_run`):
  - acted on at once, at the computation's next check: `run.abort`, `session.quit` with `save` false,
    the stop key (`key` `Escape`; `/`, which ends a range or a shooting
    for good), and what only reads or steers a view: `session.get_state`, `browser.page`, `animation.pause`, `animation.set_speed`, `animation.faster` and
    `animation.slower`;
  - taken at once, at the computation's next check, and kept for after
    it: a setting (W106: `set`, `value.set_parameters`, `value.slide`, `set.apply`, `set.restore_defaults`,
    `value.load_file`, `auto.configure_*`; "Action kinds"). The computation
    runs on with the values it started with (the right-hand side, the
    integrator and AUTO read nothing the setting changed); `state` sent
    meanwhile still shows those. When the running command ends (its
    `idle`), each setting taken runs as a command of its own, in the
    order sent and before any other line: it applies then (an invalid
    one gets its `message` `error` then, as a refused command's), with
    its own `state` and `idle`. So every setting has one `idle`, sent
    during a computation or not, and the next computation uses it; a
    session saved after the run holds it;
  - kept for their turn, run after the computation's `idle` with their
    own `state` and `idle`: an `answer` (the computation's own
    questions), every other command of the view kind (`events.subscribe`, a
    `display`, a `click`, a menu key that only shows, such as Window/zoom
    or a window's Axes: its menu opens then), and a key of the setting
    kind (a Numerics item: its dialog opens then);
  - refused: a command of the data or computation kind (a key that
    computes or saves, a file written, AUTO's Grab), with one
    log line when it arrives (`refused during a computation: key s`); it
    never runs, and after the computation it gets a `message` `error`
    ("Not while a computation runs: key was refused"), `state` and `idle`
    of its own, so a client's count of commands and idles stays right. (Until
    W95 every such line, views included, was dropped with no reply.) A
    client that enables its actions by kind, as web2 does, sends none: it
    disables data and computation actions from `computing` to the
    command's `idle`, sends its settings at once, and sends Escape as
    `run.abort`; the recording player starts its next step after the current
    command's `idle`.

  Once the computation is stopping (a `run.abort` or Escape cancelled it), a
  line that arrives is for after it: it is taken as below, so a command
  sent right behind a `run.abort` runs normally, in its turn.
- Outside a computation, while a job runs (a command in its prompts, the
  animation's Go, a command finishing), `key`, `set`, `session.get_state`, `browser.page`
  and the animation's steering commands (`animation.pause`,
  `animation.set_speed`, `animation.faster`, `animation.slower`) are *control* lines
  (the animation's Go acts on them between frames: Escape or Pause stops
  it, a `set` changes a parameter under it, and still has its own
  `state` and `idle` after the job). A control line the job does not get
  to runs after it as an ordinary command. A `set` a job takes after its
  computation began (a Flow's next trajectory, a range's next run) waits
  for the job's end, as during the computation.
- Every other command sent during a job, outside a computation, is queued
  and runs, in order, after the job's `idle` (with its own `state` and
  `idle`). A command a prompt reads that does not answer it (sent before
  the client saw the question: a click right behind the command that
  asks) is kept the same way, for after the command that asked (W95; it
  used to be dropped). A setting a prompt reads applies at once when the
  command has not computed yet (a value edited while the Initialconds menu
  is open counts for the run its Go starts), else after the command, and
  has its own `state` and `idle` after the command either way. Not in a
  recording, where each ask takes the step's next recorded answer; an
  unexpected ask stops the player.
- `run.abort` never has an `idle` of its own, so a client can send it at any
  time without upsetting its count of commands and idles.
- The queue of waiting lines is bounded (W389, #451). The normal lines
  waiting for the core, those in the input queue and those the command
  loop keeps for after the command that runs, are at most 1024 and 64 MB
  together (core/xpp_inbox.h `MAX_PENDING_LINES`, `MAX_PENDING_BYTES`: far
  more than a client that waits for each `idle` ever has waiting). A normal
  line that arrives at the full queue is refused, never queued and never
  waited for: the reader goes on, so nothing behind it is held up. The
  client gets one `message` `error` whose text begins `Busy:` and names the
  command (`cmd`, and its `id` when it has one), with the line, cut to 200
  bytes, as `source` (the line itself is the place; the refused command has
  no `idle` of its own and ran nothing). The client sends the line again
  later, or a message queue in front of the server redelivers it. Control
  lines (`run.abort`, `session.quit` with `save` false, the stop keys and the rest this list calls acted
  on at once) are never refused, so a `run.abort` sent after a flood is read and stops the run. Over
  HTTP the refusal is `503 Service Unavailable` with that same event as the
  body (`application/json`), the command's `n=` number is not used up, so
  sending it again with that number is taken; web2 shows the error text. A
  body of several lines is taken line by line up to the refused one.
  The program's own lines (a recording's next step, a model the system asks
  to open) are never refused.
  The longest input line is the byte bound, 64 MB (a `--server` line over it
  is discarded and answered by one `input` error); an HTTP body is capped at 1 MB.
- A command whose job was cancelled (by `run.abort`, Escape, `session.quit`) sends
  `stopped` before its `state` and `idle`: where the computation got to,
  which is what the recording player uses to replay the interruption (below).
  A `session.quit` with `save` false read by the computation's own check ends the command at once: it sends
  `stopped` too, then `bye` and the exit (it never reaches `state` and `idle`), the same
  `stopped` as when the computation sees the cancel first and the quit runs after the command.

## Playing a recording without an interface

`xppautX run.recx --silent` loads the recording's snapshot and plays its
steps through the same player as File/plaY recording, without a window,
browser, stdin reader or protocol output. Display pacing is skipped.
It exits 0 when every step played cleanly, and 1 after any error. Each
failure is printed once on stderr with its file, line and source: a
command failure names the `.recx` step, a model failure its model line.
An unexpected question, unused answer or missed recorded interruption
stops the player; nothing after that step runs. A command needing pixels
from the interface fails in this mode.

A step's `abort` arms `xpp::job::stop_at_rows`, `stop_at_point` or
`stop_at_frame` before its computation. The job stops at that exact row,
AUTO branch/point or animation frame, as interactive replay does. If it
never reaches that place, playback fails at the step. An interruption of
`other` cannot be placed and the command runs to completion.
An AUTO stop at point P ends the branch with an EP repeating point P - 1;
an integration stop counts stored rows, including the initial row. A range
stops in the first run to reach that row count.

The whole recording and every step are read and checked before a model
loads or a step runs. Input is capped at the file API's 64 MiB limit;
JSON nesting and snapshot archive expansion retain their reader limits.
Reads use the recording's embedded files. Silent playback keeps its output
in the current folder where the command was launched, under plain base
names only: no paths, subfolders or links, including the initial model load.
An existing output requires confirmation; without it playback stops at that
step, exits 1, names the file and recording step, and leaves the file
untouched. Recorded save decisions do not authorize replacing launch-folder
outputs. See [Question answering](#question-answering) for the answering policy.
Interactive Play still writes only
in its private scratch folder; use `--server` and the `file` command to
inspect those files before closing. Protocol clients wait for
`ask` and `idle`; recordings are the only playable file format.

`examples/recordings/lecar_auto.recx` records the former AUTO example's
steps: select Le Car's `hopf` named set, find its fixed point, import
it, continue the steady branch, grab the Hopf point, continue the periodic
branch, and save the diagram. Run it with:

    xppautX examples/recordings/lecar_auto.recx --silent

## Recordings

File/recorD (key `d` of the File menu) or `recording.start`
starts recording the session (W59a, core/json_record.cpp; the file's
format is core/recx.h); File/recorD again or `recording.stop` writes it,
`name.recx`, one plain text file that any editor opens:

```
recording: {
  version: 4,
  program: "xppautX 8.1",
  model: "lecar.odex",
  recorded: "2026-09-30T10:14:02Z",
},

@snapshot
UEsDBBQAAAAAA...(the session file, base64)
@end

@file lecar.odex
par iapp=0.1
...
@end

@steps
# First run: the cell fires once and settles.
{"step":"Run → Initial","cmd":{"cmd":"run.initial"}}
{"step":"Run → Configure","cmd":{"cmd":"run.configure","total_time":1e7}}
{"step":"Run → Initial","cmd":{"cmd":"run.initial"},"abort":{"what":"integrate","rows":2193,"t":219.2}}
{"step":"Zoom window 1","cmd":{"cmd":"display","window":"graph_1","x":[0,50]},"view":true}

fingerprint: 5f0c...(64 hex digits)
```

- The header (W285), everything before the first line that starts with
  `@`, is the settings syntax (docs/odex.md ".setx"): one `recording` group
  with its `version` (2; another is refused), the `program` (`--version`'s),
  the `model`'s file as it names itself and `recorded`, when the recording
  began (UTC), every key named and in any order. A text that is no such
  group, a key twice, one missing or one it does not know is refused at its
  line and column. The steps stay JSON protocol lines.
- `@snapshot` ... `@end` (W59d): the session as it was when the
  recording began, what Save session writes to a `.snapx` ("Session
  files": the model, the values, the numerics, the windows and what they
  show, AUTO's diagram) but without the data table, which the replay
  computes again; its bytes in base64, 76 digits a line, as a `@binary`
  section. It is the state at that moment, not the history before it: a
  recording begun partway through a session replays from there. Every
  recording has one: a file without it is not a recording (opening it is
  an `error` message, nothing else changes).
- `@file NAME` ... `@end`, one section per file: the model's files first
  (its `.ode` or `.odex` and every file its load read, as a session file
  saves them), then every other text file the session read while
  recording (a set, a parameter or IC file, a table, an animation), by
  the name it was opened with, a line of the file a line here. A line of
  the file that is `@end` or starts with `@@` is written with one more
  `@` in front. A file read again with other bytes (a model edited and
  reloaded) is a second section of the same name, after the first. A file
  that is not text (a `.snapx` opened while recording, a
  binary table) is a `@binary NAME` section instead: its bytes in base64,
  76 digits a line, then `@end` (W59b). A recording holds no data: a
  replay computes it again.
- `@steps`: one JSON object a line, one per step. A step is everything
  from leaving idle to the next `idle`: the command, the answers to what
  it asked, and where it stopped when a `run.abort` or Escape cancelled it.
  `step` is a label for a person: the command's menu path (`File → Save session`) or its name and values.
  `cmd`: the command, whole (`value.set_parameters`, `run.initial`, ...),
  whether it was typed, clicked in a menu or run from a key: a step is a
  command and nothing else (W536: a step that holds `keys`, `win` or
  `button`, the old key and button forms, is refused at its line, naming the
  member and its value). `answers`: the answer to every ask, in
  order: a `string` ask's value, a `form`'s values (an array), any other
  ask's answer without `cmd`, `id` and `ok` (`{"file":"x.set"}`,
  `{"xd":1.5,"yd":-60}`, a choice's or a grab's `{"key":"Tab"}`), `null` for a cancel;
  a `pixels` ask (the client's, not the user's) has none.
  `view`: the step only changes what is shown (its kind, "Action kinds",
  refined by the menu item it picked), so a player may run it quickly.
  `abort`: the `stopped` event's `at`, recorded in the step
  ("Playing a recording without an interface"). `during`: the keys the running job read itself, each
  `{"key":K,"at":AT}` with where the job was when it took it: `/` ending a
  range or a shooting, Escape stopping the animation's Go (an Escape
  during a computation cancels it: that is `run.abort`). Animator pause and
  speed controls use `{ "cmd": <command>, "at": AT }` in the same array,
  in arrival order, and replay at their checkpoint. `files`: the file
  sections the step read, counted from 0 in the file's order, in the
  order it read them; the player serves each read from them. Every zoom or pan (`window.set_display`) is a step of its own. A
  setting sent while a command had not computed yet (a value edited while
  the Initialconds menu is open) is a step of its own just before that
  command's, since it applied before anything computed. Not steps:
  `session.get_state`, `events.*`, `keymap.*`, `redraw`, `browser` paging (`from`), `file`, an
  `run.abort`, `session.quit`, and the `recording.*` commands; nor File/recorD's own opening of
  the File menu (the `f` just before the `d` that stops).
- `# ` lines just before a step are its note (a note of several lines is
  several `#` lines; `#` alone is an empty line of it): `recording.add_note`
  sets it while recording, and an editor can add or change one later.
- `fingerprint:` the SHA-256 (64 lowercase hex digits) of every line of
  the `@file` sections (their `@file` and `@end` lines included) and every
  step line, in the file's order, each followed by `\n` (a CR before it
  not counted). The header, blank lines, the notes and the fingerprint
  line itself are not in it: editing a note keeps the fingerprint, and a
  changed step or embedded file does not match it any more. A `@binary`
  section's lines, and the `@snapshot` section's, are in it as they are
  written.

Idle time is not recorded, and nothing is read from any older format:
there is none. `tools/servercheck.py` (`check_recording`) reads a file
back. The `recording.*` commands are not steps either.

## Playing a recording

File/plaY recording (key `y` of the File menu), Open model of a `.recx`,
or `recording.open` opens a recording in the
player (W59b, core/json_player.cpp): it asks, as Open model does, whether
to save this model's session first, then loads the session the recording
began from, its `@snapshot` (W59d): its model, from the files saved there,
then everything as it was when Record was pressed, as Open model of a
`.snapx` restores it (the data table excepted: the steps compute it). The
`player` event lists the steps, and `state.player` says where the player
is. A recording that does not read (not one, a header line missing,
twice or not one of `program:`, `model:`, `recorded:`, no `@snapshot`, a
section with no `@end`, no `@steps`, a step that is not a JSON object
with a `cmd`, or that holds `keys`, `win` or `button`, no `fingerprint:` line or a line after it) is an
`error` message at its line (`file` the `.recx`, `line`, `source`: W125)
and nothing changes: every step is read before anything is played or
loaded. One whose
fingerprint does not match plays all the same: `player`'s `intact` is
false, and the page says it was changed after it was made.

Faithful: each step runs exactly as recorded, nothing fixed, skipped or
changed. The player sends the step's command (its `cmd`, whole), then answers each question the command
asks with the recording's next answer of its kind: a `choice`
ask, like any other ask, the next of
`answers` (a `choice`'s `{"key":"y"}`, a `string`'s value, a `form`'s values, a `file`'s or any other
answer's members; `null`: a cancel). A `save_replace` owner answer is
consumed by the save operation before checking the disk, without another
prompt or input; it does not delay arming a recorded interruption. A
`pixels` ask is the client's, as always. With the step's last input it
arms its `abort`, or its first `during` key
("Playing a recording without an interface"): the job stops, or is handed the key, exactly where the
recorded one did. A step that asks a question it holds no answer for, ends
with answers left over, or never reaches its interruption gets a `message`
`error` (`Step N of the recording ...; the player stopped`) and the player
pauses; an open question is then the user's to answer.

Files: while a step runs, a file it reads is the recording's copy, the
next of the sections its `files` names with that name (the same name, or
the same file name from another folder), never the disk's: a file the
recording does not hold for that step is not there (an `error` message
says so). The model's own load reads the recording's first section of
each name. The replay runs in a scratch folder of its own (its outputs go
there; `file` lists and fetches them), so a play writes nothing beside
the recording and plays the same the second time. The replay starts from
the snapshot, so a recording begun later in a session (a parameter
changed, a run, a diagram computed before Record) replays exactly; a
recorded Open model of a file the recording does not hold, or a folder
this machine lacks, stops there. `recording.seek` loads the snapshot again.

Pace: only computation takes time. Before each input the player sends
`press` (what it is about to send, `ms` before it sends it): a step's
first input 700 ms plus the note's reading time (35 ms a character, at
most 3 s), a menu's key 600, a dialog's answer 900 plus 40 a character
of it (at most 2500), a message 1200; a view step's a third. Each of
these is then scaled by 1.5 (`PACE_SLOWDOWN`: 1x was a little too fast, maintainer 2026-10-01), so
the first input of a step at 1x is 1050 ms plus 1.5 times the reading time. After a
step's `idle` it waits 800 ms (a view step 250) before the next, never
merging or skipping one. The result is divided by `speed`. `pause` stops the
clock (the wait keeps what is left), `step` runs one step, `from` N runs
the steps before N with no wait (their `press` events 0 ms), then pauses
(or plays on).
The player acts from the command loop's and a question's wait for input,
so the core stays on its one thread and keeps reading the client's lines
while it waits. `tools/servercheck.py` (`check_player`,
`check_player_ani`) and `tools/autocheck.py` (`recording.open`) play recordings
back and compare the data.

Wildcard matching uses constant stack space, including long runs of `*`.
File-kind suffixes are case-insensitive and require a basename before the
extension: `model.ODEX` is an .odex file; `.odex` and `dir/.odex` are not.
Input strings are decoded whole: command and operation names, model names,
values, file paths and wildcard patterns have no per-field character caps.
An HTTP command body over 1 MB is refused with HTTP 413; a `--server` input
line over 64 MB (the queue's byte bound, W389) is discarded through its newline and produces one `message`
`error` naming the input limit. Neither executes a truncated command. A
workspace basename over 255 bytes is refused as `not a plain file name in the model's folder`, never shortened.
An unreadable workspace listing is a failed `file` event (HTTP `/files`: 500),
not an empty successful listing. Failed file opens report `cannot open FILE:
REASON`, with the requested file in the error's place and the system or explicit
reason in its text. Clients use the structured file/place fields, not this
wording, to locate files.

## --silent

`xppautX model.ode --silent` is a script of these same commands, built in
(core/json_silent.cpp): after the model loads with no interface at all (a
model that does not load exits 1), the command line and the model's `@`
options say which commands, kept as an internal list with no file
reader, their events going nowhere and what the core says
going to the log as with no interface (nothing new on stdout; an error
message leaves the exit status 1). Each step is made when its
turn comes, after the steps before it ran, since an internal set may
change any option. The questions the commands ask are answered by the
script's next lines; one it does not expect stops it (exit 1). For each
run (once, or for each internal set in turn with `--internset`/`--uset`/`--rset`,
each set's data then in `<set>.dat` unless `--outfile` is given):

| what | commands |
|---|---|
| the internal set | `set.apply` with its `name` and scope `all` |
| `--qsets`/`--qpars`/`--qics`, `-dryrun` | the internal sets, parameters and initial conditions asked for, each under its `#` heading, into the output file (core/lunch-new.cpp `write_values_query`, called by the script itself); nothing is run |
| the run | `run.initial`, or `run.range` for `@ range=1` or `@ stoch=` |
| `@ postprocess=` | the analysis it names, with the model's settings: `analysis.histogram`, `analysis.fourier_series`, `analysis.power_spectrum`, `analysis.spectral_density` (with its `type`) or `analysis.histogram_2d` |
| unless each run of a range wrote its own `output.dat.N`: `@ stoch=1`/`2` | `analysis.load_stochastic_mean` or `analysis.load_stochastic_variance` |
| output.dat (`@ output=`, `--outfile`; not with `--noout`) | the output columns' rows as `dat` (core/browse_data.cpp `save_data_file`, `browser.export`'s `target` `output`), replacing the file, called by the script itself |
| `--mkplot` | `plot.export_pdf` (or `plot.export_svg` for `--plotfmt svg`) with the picture parameters of the model |

After a run, the core computes requested nullclines (`nullclines.draw`, `nullclines.file`; a `.ode`'s `ncdraw`)
and `dfdraw` phase-plane data
in every mode, before exports: `nullclines.file` (`ncdraw=2`: `nullclines.dat`) also writes the nullclines (a row a segment: `nullcline` 1 or 2, `x1`, `y1`, `x2`, `y2`, in the data format its extension names) and
`dfdraw=4`/`5` dirfields.dat through the normal writer rules in the current
working folder (W346). These are no longer extra script commands.
After every run the script sends `--equil 0`/`1` as `equilibrium.export` with `file` `"equil.dat"` (`shoot` true for `--equil 1`, `confirm` true to replace the batch output). The files the command line names
(`--setfile`, `--parfile`, `--icfile`, `--readset`, `--with`) are read by the
model's start in every mode, before the script.

## Events (server to client)

A JSON number is any finite double: every finite value up to the largest
double (1.7976931348623157e+308) is sent as a number, written with 15 or 17
significant digits, whichever reads back exactly. NaN and the infinities are
sent as `null`, since JSON has neither (the rule is core/json_number.h's).
A `null` in a number field therefore means "not a finite number", never "a
large value".

| ev | fields | meaning |
|---|---|---|
| `hello` | `protocol`, `features` (optional parts the server speaks: `series`, `plots`, `nullclines`, `dfield`, `marks`, `ani`, `autoinfo`, `autosettings`, `numerics`, `windows`, `player`), `title`, `file`, `output_names` (`par`, `ic`, `csv`, `curves`), `commands`, `lists`, `userbuttons` [name...] | First event, and again after `open` or `reload` loaded a model in its place. `lists` are what a form field `*n` picks from: 0 T and every variable, 1 ODE variables, 2 parameters, 3 both, 4 markers, 5 methods (items like `2 Box` start with the number to enter); `line_styles` (above), `colormaps` (above). `defaults` {`pars`, `ics`}: the ODE file's values, one per entry of `state`'s `pars` and `ics` in their order (what `default` restores). |
| `windows` | `windows`: [{`name`, `kind` (`graph`, `diagram`, `browser`, `animation`, `array_plot`), `shown`}] | Every open window of the session, in `window.list`'s order, and whether `window.hide` hid it; sent when `events.subscribe` asks for `windows`, and again whenever it changes (a window made, destroyed, renamed, hidden or shown). A hidden window keeps everything it holds. |
| `window` | `op` (`create`, `select`, `destroy`), `kind` (`graph`, `diagram`, `animation`, `array_plot`), `win`, `w`, `h`, `title` | Plot windows, AUTO, animation and array plot, identified by name. `kind` distinguishes auxiliary panels from user-named graphs; `hello.window_names` gives their initial names. `w`, `h` are the core's pixel size of the window: what the pixel fields of `state.view`, `state.auto` and pixel answers to asks refer to. |
| `diagram` | `op` (`axes`, `reset`, `add`, `views`), `view`, ... | The AUTO diagram as data, each view of it; see "The AUTO diagram as data". |
| `autoinfo` | `info`, `stab`, `stop`, `notes`, `start_refined` | AUTO's info strip and stability circle, and why the last branch ended, as data, for a client that asked (`events.subscribe`); see "The AUTO diagram as data". |
| `autosettings` | `numerics`, `pars`, `axes`, `marks` | AUTO's settings as data, for a client that asked (`events.subscribe`); see "AUTO's settings as data". |
| `numerics` | `fields` [{`key`, `label`, `value`, `integer`, `unused`, `choices`, `unavailable`}...] | The main numerics as data, for a client that asked (`events.subscribe`); see "The numerics as data". |
| `series` | `win`, `rows`, `three`, `enc`, `xlabel`, `ylabel`, `zlabel`, `curves`, `shift`, `columns`; or `op` `append`, `win`, `from`, `rows`, `enc`, `columns`; or `op` `end`, `win`, `rows`, `version` | A plot window's curves as numbers, one event per window, for a client that asked (`events.subscribe`), and during an integration the active window's rows as they are stored; see "The plot as data". |
| `runs` | `win`, `enc`, `erased`, `clear`, `drop`, `add` | A plot window's earlier runs changed, for a client that asked for `series`; see "Display state". |
| `autoview` | `earlier`, `show`, `active`, `views` | AUTO's hidden branches, the active view and each view's zoom, for a client that asked for `autoinfo`; see "Display state". |
| `erase` | `win` | The Erase command blanked plot window `win`, for a client that asked for `series`: it shows nothing of that window's curves (nor the earlier runs it may keep) until the window's next series or `redraw` (the `runs` event carries the same as data). Only Erase sends it: a zoom also redraws the window, and keeps what a data client shows. See "The plot as data". |
| `redraw` | `win` | The Redraw command drew window `win` again, for a client that asked for `series`: it shows the window's current series again (no series follows: the data did not change), and no earlier runs. |
| `plots` | `active`, `windows` [{`win`, `title`, `three`, `xlo`, `xhi`, `ylo`, `yhi`, `box`, `theta`, `phi`, `persp`, `zplane`, `zview`, `curves`, `shift`, `zoom`, `runs`}...] | Every plot window and the active one, for a client that asked (`events.subscribe`); see "The plot as data". |
| `nullclines` | `win`, `enc`, `xname`, `yname`, `xcolor`, `ycolor`, `x`, `y`, `frozen` [{`x`,`y`}...] | A plot window's nullclines as segments in plot coordinates, for a client that asked (`events.subscribe`); see "The plot as data". |
| `marks` | `win`, `enc`, `equilibria`, `text`, `arrows`, `markers`, `frozen` | A plot window's equilibria, text, arrows, markers and frozen curves, for a client that asked (`events.subscribe`); see "The plot as data". |
| `state` | `sliders` [{`name`,`lo`,`hi`,`step`}...] in slot order (empty names retain vacant slots), `pars` [[name,value]...], `ics` [[name,value]...], `now` [value...] (after a first run), `rates` [number or null...] (one per `ics` entry: the largest absolute dv/dt over the last `hello.state_inspection.tail_intervals` stored intervals, null when it cannot be told: "Recording startup and all-state inspection"), `bcs` [[name,text]...] (only when the model itself defines boundary conditions, `b`/`bndry` lines or `boundary` statements: then one per variable, those it left out being 0; a model with none sends no `bcs`, the page shows no boundary-conditions section), `delays` [[name,text]...] (delay equations only), `view` {`win`,`left`,`right`,`top`,`bottom`,`xlo`,`xhi`,`ylo`,`yhi`,`three`, and `theta`,`phi` when `three`}, `auto` {`x0`,`y0`,`wid`,`hgt`,`xmin`,`xmax`,`ymin`,`ymax`} (AUTO open), `rows`, `win`, `seed` (after a run that used one), `session` {`file`}, `changed`, `can_undo`, `can_redo`, `recording` {`steps`, `note`} (while recording), `player` {`step`, `running`, `playing`, `speed`, `fast`, `intact`} (while a recording is open in the player) | Current values; `view` maps pixels of the active window to plot coordinates (x = xlo + (xhi-xlo)(px-left)/(right-left), y likewise with bottom/top) and `auto` those of the AUTO diagram, for a readout under the mouse; `theta`, `phi` are the active window's 3D angles (degrees, meaningful when `three`), so a client that turned a 3D plot itself (`view3d`) can confirm the core agrees; `rows` is the number of stored time points, `now` the current state, one value per `ics` entry: where the last run ended or stopped (what Initialconds/Last and `set` `from` `last` copy into the ICs), `win` the active window; `seed` is the seed the last run actually used (each Go, do_range sweep -- Stochastic > Compute's many runs included -- or `--silent` picks and logs one, W71's "a seed per run"; absent before any run in this session); setting the numerics' seed to it and Go reproduces that run's data byte for byte; `session` is `{"file": ...}`, the session file last saved or opened (a path as it was given to `save`, the absolute path of one opened); absent before any; `changed` (W209, always sent) is whether the values differ from those the session was last saved, opened or loaded with: a parameter, an initial condition, the numerics' time span, method, tolerances and bounds, or the run's data (the rows stored and where the run ended); a view, window or AUTO diagram alone is not a change; a save (`session` `save`, Save session, Quit's save, the import of a set file) and the end of a load or of opening a session file make it `false` (the page shows the unsaved dot in its title); `can_undo` and `can_redo` say whether the table's `undo` and `redo` have a value edit to take back or put back ("Undo and redo of value edits" below); `recording` is there while a recording runs ("Recordings"): the steps recorded so far and the note set for the next one; `player` while a recording is open in the player ("Playing a recording"): `step` the next step to run (the number of steps at the end), `running` the step running (-1 between steps), `playing`, `speed`, `fast` (running steps with no pace to a `from` step), `intact` (the fingerprint matches). |
| `idle` | | The command finished. |
| `liveState` | `time`, `now` [value...], `rates` [number\|null...] | The solver's doubles for every ODE/Markov state at the latest output step of a running integration, paced at the input interval, with no subscription ("Live integration state (W200)"); `rates` as `state.rates` ("Recording startup and all-state inspection"). The final `state` follows. |
| `stopped` | `at` | The command's computation was cancelled; sent before its `state` and `idle`. `at` says where it stopped: `{"what":"integrate","rows":N,"t":T}` for an integration, N the rows in storage (as `state.rows`) and T the time of the last one stored (9 digits: the stored single-precision value exactly); `{"what":"auto","branch":B,"point":P}` for an AUTO run, P the last point it stored on branch B (the end point the cancel adds, as in the diagram's data); `{"what":"ani","frame":F}` for the animation's Go, F the frames it had shown (W59b); `{"what":"other"}` for anything else. The recording player replays the interruption from it (see "Playing a recording without an interface"). |
| `player` | `file`, `model`, `intact`, `steps` [{`note`, `step`, ...}] | A recording opened in the player ("Playing a recording"), sent when its model has loaded and again after a `recording.set_step_note`: the `.recx` (absolute path), its model's file, whether its fingerprint matches (`false`: it was changed after it was made; it plays all the same), and its steps, each its line's object ("Recordings": `step`, `cmd`, `answers`, `view`, `abort`, `during`, `files`) with its `note` ("" for none). |
| `press` | `step`, `what`, `index`, `ms` | What the player sends next, before it sends it: `what` `answer` (`answers[index]`, a dialog's or a choice's answer) or `cmd` (the step's `cmd`); it goes `ms` milliseconds later (the pace at the speed set), so the page shows it first; 0 while running to a `from` step. A step's first `press` tells the page the step runs: the questions it asks until its `state` with `player.running` -1 are the player's to answer, never the page's (a `pixels` ask excepted). |
| `title` | `text` | Title of the selected plot window: what it plots (`W vs V`). The server also labels unlabelled 2D axes with the plotted variables. |
| `message` | one of `error`, `bottom`, `box`, `auto`, `calc`; with `error`, `file`, `line`, `col`, `source` | Status text. Results (a fit outcome, mean, Liapunov exponent, saved BVP point, or AUTO toggle) use `bottom` (W133). `box` with empty text removes a hint box. An `error` carries its place ("Errors" above, W140): `file` with `line` 0 is a file the command could not read (W118), which web2 offers to add and run the command again. |
| `progress` | `n`, `of` | Computation progress, at most 10 a second. |
| `saved` | `saved` boolean, `file` | The save owner's result for this exact destination; only `true` authorizes delivery after the command's `idle` (W129). |
| `computing` | | The running command began computing (once per command, before its first `progress`); until its `idle` the server refuses data and computation commands ("Action kinds", "Commands during a command"). A page that connects meanwhile gets it again. |
| `equilibrium` | `type`, `cplus`, `cminus`, `rplus`, `rminus`, `im`, `values`, `eigenvalues` | Result of Sing pts. `eigenvalues`: the Jacobian's `[re,im]` pairs, one per variable; absent for a delay equation. |
| `source` | `lines`, `comments` [[text, has action]...] | File/Prt src. |
| `equations` | `lines` | One `dX/dT=...` line per equation. |
| `ani` | `pos`, `rows`, `fly`, `grab`, `skip`, `speed`, `loaded`, `open`; or `op` `frame`, ... | Animation state for its slider and toggles, sent with every frame drawn and after every `ani` command: `pos` the row the next step starts from, `speed` the ms between frames of Go, `loaded` 1 when an `.anix` file is loaded, `open` 1 while the animation window exists. With `op` `frame`: a frame as data, for a client that asked (`events.subscribe`); see "The animation as data". |
| `aplot` | `title`, `nx`, `ny`, `values`, `enc`, `colormap`, `ramp`, `zmin`, `zmax`, `tlo`, `thi`, `first_row`, `tag` | The array plot (the array plot surface; W492, #556 made it data only): `ny` rows of `nx` cells, row by row, `values` their numbers (as a series column; `null`/NaN off the stored rows and columns), `colormap` the session's colour scale by name and `ramp` its 256 colours from the low end to the high one: a cell is the ramp's colour at floor(256 * fraction), fraction being (value - `zmin`) / (`zmax` - `zmin`) clamped to 0..1 (core/colormap.h `ramp_color`); with `zmax` not above `zmin` no cell is coloured. `tlo` and `thi` are the times of its first and last row, `first_row` the stored row its first row of cells shows (`array_plot.scroll`'s argument). `tag` the range-saving frame's text. Print writes the same cells and colours as a PDF. |
| `film` | `op` (`capture`, `reset`, `play`, `autoplay`), `count`, `win`, `cycles`, `delay` | Kinescope. The client keeps the frames: on `capture` it copies window `win` as it is drawn now; `play` shows them, `autoplay` plays `cycles` times `delay` ms apart. |
| `ping` | | Beep. |
| `browser` | `rows`, `cols` (names, `T` first), `row0` (selected row), `start`, `end` (the First..Last range), `from`, `col`, `data` | Rows `from`.. as [T, column `col`, `col`+1, ...]; `null` for NaN. Sent for a `browser.page` request (as its `done`'s `rows` too) and after any command that changed the data while the client shows the browser. |
| `bye` | | The program is exiting normally: sent by every quit (a `session.quit` with `save` false, during a computation or not, and the question's outcomes) before an error-free exit, so browser mode's `exit` event says `code` 0. Every quit path exits 1 after a reported error and sends no `bye` (W133). A crash or an error exit sends none (`exit` `code` 1). |
| `error` | `error`, `file`, `line`, `col`, `source` | The model did not load: sent instead of `hello`, then the program exits (see "A model that does not load"). |
| `file` | `op`, `name`, `ok`; `size`, `sha256` (`put`, `get`), `data` (`get`, base64), `files` (`list`: [{`name`,`size`,`mtime`,`sha256`}...]); `error`, `file`, `line`, `col`, `source`, `field` when `ok` is 0 (the common error shape above) | The answer to a `file` command (see "Files" below). |
| `keymap` | `op` (`get`, `set`, `reset`), `keymap` (the object of "Keymap") | The answer to a `keymap.get`, `keymap.set` or `keymap.reset` command. |
| `ask` | `id`, `kind`, ... | See below. |

In browser mode (`xppautX model.ode`) events stream from `/events?t=TOKEN`.
Commands are POSTed to `/cmd?t=TOKEN`, one per request (a body of several
lines gives several commands). xppautX answers each connection on a thread
of its own, so a connection that sends nothing (a browser's preconnect) or a
stalled upload holds up no command, `run.abort` above all; two POSTs in flight at
once may therefore reach the core in either order, and a client that needs
its order sends the next one once the last was answered, as the page does.

A client may number its commands (W124): `/cmd?t=TOKEN&p=PAGE&n=N`, `PAGE`
its own id (1 to 64 letters, digits, `-` or `_`; the page uses 32 random hex
digits, new on every load) and `N` 1, 2, ... in the order it sends them. A
POST that failed with no answer (a connection the browser or the network
dropped, before or after it got there) can then be sent again as it was:
xppautX remembers the last number each id's commands reached the core with
and answers a number not above it with 204 without running the command a
second time. The page does that, after 0.1, 0.3, 1 and 3 seconds, and says
the command did not reach xppautX only when all of these failed too; a
status it got back (403, 413, 400) it never sends again. One of `p` and `n`
without the other, or either malformed, is 400 "bad command number". A
command without them (a script, `curl`) runs as it comes, each time it is
sent.

When the page goes away (web2's `pagehide`, browser mode only: the desktop
window closes through its own path) it POSTs `/leave?t=TOKEN` with
`navigator.sendBeacon` (204; 403 without the token). xppautX then waits about
2 seconds for an event stream to connect again, which a reload does within
that, and ends the program as the watchdog would (no `bye`, no page left to
tell) if none has and no other stream still answers. This includes a program
that has stopped on an error (no `bye`, `exit` `code` 1) and only serves its
log: it keeps serving while a page is open and ends the same way once the
last page has left (W112), though not if no page ever connected. A page that vanishes
without a word (a crashed browser) is found by the heartbeat, an SSE comment
every 2 seconds, and ends the program after 10 seconds without a page. Before
closing, the browser-mode page also asks the browser's own "Leave site?"
(`beforeunload`): closing the tab loses the session.

Two more events come from the host, not the server: `log` {`text`} carries what the
server printed on stderr (xppaut reports model errors, such as a formula that does
not parse, only there; also when the Windows exe was started with no stderr
at all, as from Explorer: the host gives the stream one, T27), in chunks cut
anywhere, which the page joins into lines and classifies line by line, and `exit` {`code`} says the process ended. The page
shows one error at a time in a red box (the newest replaces it and the next
command clears it; a load failure or crash stays with the output that explains it)
and keeps everything under "Messages".

### The AUTO diagram as data

A run beginning a segment carries `parameters`, the branch's `[name,value]` pairs in model order, at full precision (W339, [#400](https://github.com/MuhammadMoustafa/xppautX/issues/400)). Its point coordinates resolve the continued values over these immutable constants.

The server sends the AUTO diagram's points (the selected AUTO view) as data, so that
a client draws it and can zoom, pan and name the point under the mouse
without a round trip. The data describe exactly what XPP's diagram shows
(and what PDF and SVG export draw): after `auto.hide_earlier` (Clear) it
is empty (web2 does not send it: its Clear only hides the branches so far).
An Axes change (the AutoPlot form, last 1 par, last 2 par), File/Load
diagram and File/Reset diagram draw the diagram again at once, so a client
always holds the diagram in the current quantities without a reDraw.

A point is one `add_point()` of `core/auto_nox.cpp`, in the quantities the
axes plot (`auto_xy_plot`: the parameter against the maximum, norm,
period, ... of the Axes setting), in the order it was plotted.

**Views of the diagram** (W50). The one diagram has any number of views
(at least one), each with its own axes (plot type, variable, parameters,
ranges) and zoom: I-V beside I-gca in two-parameter work. Every `diagram`
event names its `view` (`"diagram_1"`, `"diagram_2"`, ...) and applies to that view's points and
axes; a client keeps one list per view. Every view holds one entry per
point of the diagram, in the same order, so a point's index is the same
in every view (the grab, `info.point`, Clear's `earlier`): a point a view
does not plot (a one-parameter point in a two-parameter view, a
two-parameter point in another) comes with `d` 0 and `x`, `y` `null`. One
view is the active one (`autoview`'s `active`): the Axes menu (the AutoPlot
form, zoom, Fit, Scroll, last 1 and 2 par, Default), the exports, the info
strip and a Run go by its axes (its parameters are the ones a run
continues in), and a grab's point by its data.

- `{"ev":"diagram","op":"views","n":N,"names":["diagram_1",...]}` lists
  the open views in display order. The client preserves each named view's
  data and drops names absent from the list. Names stay unchanged when a
  preceding view closes.
- `{"cmd":"auto.new_view"}` adds a named view with the active
  one's axes and makes it active (the Axes menu's New view does the same).
  `{"cmd":"window.select","window":"diagram_1"}` selects it;
  `{"cmd":"window.destroy","window":"diagram_1"}` destroys it (the
  diagram is the session's and stays). Destroying the last view closes AUTO;
  File/Auto reopens it. `window.hide` and `window.show` take a view off
  the screen and bring it back with its axes and zoom. These are views (kind `v`),
  queued while AUTO computes, while each view receives the run's points.

- `{"ev":"diagram","op":"axes","view":"diagram_1", xmin, xmax, ymin, ymax, x0, y0, wid, hgt, plot, xlabel, ylabel}`:
  the diagram was drawn again at these axes (`plot` is the view's
  projection, by name: `maximum`, `norm`, `maximum_and_minimum`, `period`,
  `two_parameter`, `frequency` or `average`; W491); the points are unchanged. Pixel `x0 + wid*(x-xmin)/(xmax-xmin)`,
  `y0 + hgt - hgt*(y-ymin)/(ymax-ymin)` of the selected AUTO view is (x, y) in the
  core's pixels (what a pixel answer to a `grab` or `rubber` ask means).
- `{"ev":"diagram","op":"reset","view":"diagram_1","keep":k, ...the axes fields}`: drop every
  point of that named view after the first `k` (all of them for 0), then the axes as above.
- `{"ev":"diagram","op":"add","view":"diagram_1","from":n,"runs":[...]}`: points `n`, `n+1`, ... of view v
  (`n` is the number of points the client holds) in runs of points that
  share their branch, kind and style and whose numbers count up by one:

| run field | meaning |
|---|---|
| `br`, `pt` | branch, and the number of the run's first point (absolute values) |
| `ty` | 1 stable steady state, 2 unstable steady state, 3 stable periodic, 4 unstable periodic (core/auto_codes.h DiagramPointType) |
| `stable`, `periodic` | what `ty` is (W118): stable or not, a periodic orbit or a steady state; what a client reads |
| `f2` | two-parameter curve (1 limit point, 2 limit point of periodics, 3 Hopf, 4 torus, 5 branch point, 6 period doubling, 7 fixed period, 13 neutral saddle); absent for one parameter |
| `d` | how it is drawn: 0 not at all (the next line starts from it), 1 a line back to the point before it, 2 filled circles of radius 3 at y and y2, 3 open circles |
| `c`, `lw` | colour (a colour: `foreground`, `#rrggbb`, see "Colours") and line width |
| `new` | 1: the run's first point starts a new line (no line back) |
| `from` | the label the continuation that computed the run's first point started from (Auto.irs: the point Grab took), on the first point of such a continuation only; absent otherwise, and for a diagram loaded from a file or computed before AUTO kept it. A periodic branch's `from` names the Hopf point it bifurcates from |
| `jac` | how the Jacobian of the AUTO run that computed the run was formed: `difference` (XPPAUT's forward differences) or `exact` (the Numerics' Jacobian at that run, W278); a run never joins one with another setting, so one diagram can hold both |
| `x`, `y` | the values, one per point (`null` for NaN) |
| `y2` | the second value (the minimum, for hi and lo), when it differs from `y` anywhere in the run |
| `lab` | [[index in the run, label, type (a point type's name, from hello's `point_types`: `end`, `fold`, `hopf`, `branch_point`, `period_doubling`, `neimark_sacker`, `user_point`, `no_convergence`, `bogdanov_takens`, `cusp`, `zero_hopf`, `generalized_hopf`, `resonance_1_1` to `resonance_1_4`, `fold_flip`, `fold_neimark_sacker`, `flip_neimark_sacker`, `double_neimark_sacker`, `snic`, `homoclinic`, `heteroclinic`, `cycle_fold`, `cycle_branch_point`, `output`, `regular`)], ...] for the labelled points |

On equilibrium fold curves, BT means Bogdanov-Takens (stored ITP -21), CP cusp
(-22), ZH zero-Hopf (-23). On Hopf curves the encodings are BT -31, GH
generalised Hopf -32, and ZH -33. GH uses the first Lyapunov coefficient.
These are typed `AutoPointKind` values in `core/auto_codes.h`; `ty` remains
the stability classification. Their location uses the same pipeline as the
other special points (W364, #426).

A run of AUTO sends `add` events as the points come, at most a few a
second. A redraw that plots the same points at other axes
(reDraw, Fit, zoom, scroll) sends only `axes`: the server
compares the points it plots again with what it sent, and only when they
differ (another Axes quantity, new points) does it send `reset` with how
many still agree and `add` for the rest. The points of a redraw go out
when the redraw is over, not while it runs. A new AUTO window starts empty.

**`autoinfo`**: what the AUTO window's info strip and stability circle
show, as data, for a client that asked with
`{"cmd":"events.subscribe","events":["autoinfo"]}`. It is sent at the end of that
command, and then whenever what it says changed: before every ask (so
every step of a grab brings the point's strip and circle before the grab
asks again), at the end of a command, and at most ten times a second while
AUTO runs; a command that changes neither sends none. A redraw of the
diagram (reDraw, new axes, a loaded diagram) leaves the strip and the
circle as they were: after a run, the run's last point; after a grab, the
grabbed point. core/auto_data.cpp
keeps it, from what auto_nox.cpp shows there; `stop` comes from
core/auto_stop.cpp, which autlib1.cpp tells where it ends a branch (T23).

```
{"ev":"autoinfo",
  "info":{"point":18,"br":1,"pt":19,"type":2,"sym":"HB","lab":2,
         "par":[{"name":"iapp","value":0.2624638},{"name":"phi","value":0.2}],
         "norm":0.2891081,"var":"V","u":-0.1989438,"per":14.44537,"x":0.2624638,"y":-0.1989438,"y2":-0.1989438},
 "stab":{"periodic":0,"circle":[[0.9069413,0.4214016],[0.9069413,-0.4214016]],
         "eig":[[6.093e-05,0.434962],[6.093e-05,-0.434962]]},
 "stop":{"why":"parameter_max","text":"parameter iapp reached parameter_max (0.5)","br":1,"pt":49,
         "value":0.5048396225954056,"limit":0.5},
 "notes":["Br 1 Pt 12: a Hopf bifurcation (HB) between points 12 and 13 was not located (not in max_location_iterations = 8 iterations); it is missing from the diagram"]}
```

| field | meaning |
|---|---|
| `info` | the point the strip shows: the one a grab's cursor is on (the strip changes only while grabbing); `null` before a grab, and in a new AUTO window |
| `info.point` | its index in the `diagram` data (the points the client holds, the same index in every view), -1 when they do not have it (after Clear, or a load not drawn yet) |
| `info.br`, `pt`, `type`, `sym`, `lab`, `f2` | branch and point number (positive, as in `diagram`), `type` as a run's `ty` (1 stable steady state .. 4 unstable periodic, with `stable` and `periodic` as a run's), the label's type (a point type's name, `end`, `fold`, `hopf`, ...; `info.sym` is the name, its mark and label come from hello's `point_types`), the label (0 for none), and for a two-parameter point its curve kind as `f2` |
| `info.jac` | `difference` or `exact`: the Jacobian setting of the run that computed the point (as a run's `jac`) |
| `info.par` | the continuation parameter's `name` and `value`, and the second parameter's for a two-parameter point (the strip then shows both; for a one-parameter point it shows a blank name and 0) |
| `info.norm`, `var`, `u`, `per` | the norm, the variable of the Axes setting and its value, the period (AUTO's value for a steady state too: what the strip prints) |
| `info.x`, `y`, `y2` | where the active view plots the point, in the quantities of its axes (another view's are its own `diagram` data's point `info.point`) |
| `stab` | what the circle shows: the point AUTO computed last, or the grab's cursor; `null` before any |
| `stab.periodic` | 1: `circle` holds the Floquet multipliers of a periodic orbit; 0: e^λ of each eigenvalue λ of a steady state (XPP keeps them so: inside the unit circle is stable) |
| `stab.circle` | `[re,im]` per variable, the values themselves (the X11 circle clamps them to ±1.95). All `[0,0]`: not computed. Always a stored diagram point's own values, while AUTO runs and while grabbing alike (core/auto_stability.h). AUTO computes them from a run's second point on, so a run's first point, and a run that stops there, has none, unless the run restarts from a label of the same kind (a steady state from a steady label, a periodic orbit from a periodic one, one parameter): that first point is the label's solution and carries the label's values. A periodic run from a Hopf point, a two-parameter run and a period doubling's branch switch start with none; a two-parameter curve of periodic orbits (a limit point's, a period doubling's, a torus') has none at all, AUTO computing no multipliers along it. (XPPAUT stores the last values computed with every point, so its first points carry those of another point.) |
| `stop` | why the run's last branch ended; `null` until one ends, and again when a run starts or AUTO's window is new. AUTO labels the end EP (a limit, Max points, Stop, a Mark value) or MX (no convergence); this says which |
| `stop.why` | `parmin` / `parmax`: the continuation parameter went below Par Min (RL0) / above Par Max (RL1); `normmin` / `normmax`: the norm AUTO checks went below Norm Min (A0) / above Norm Max (A1); `npts`: the branch has Max points (NMX); `user`: Stop (a `run.abort`); `mark`: a Mark value set to stop (AUTO's UZR endpoint); `jacobian`: the exact Jacobian was not finite (W275: an error with an action follows); `noconv-min`: no convergence even at the smallest step (Dsmin); `noconv-fixed`: no convergence with a fixed step (IADS 0); `noconv-switch-min`, `noconv-switch-fixed`: the same while switching to a bifurcating branch; `noconv`: no convergence, how not noted. A no convergence's `text` names the Newton settings that gave up (`ITNW`, `EPSL`, `EPSU`) and, when the last linear solve was singular, its smallest pivot (W491); it is logged as a WARN where AUTO's Output is not shown |
| `stop.text` | the reason in words, to follow "Stopped: " (the page's status strip); AUTO's Output gets the line `Branch 1 stopped at point 49: parameter iapp reached Par Max (0.5)` for every branch that ends |
| `stop.why` | `parameter_min` / `parameter_max`: the continuation parameter went below `parameter_min` / above `parameter_max`; `norm_min` / `norm_max`: the norm AUTO checks went below `norm_min` / above `norm_max`; `max_steps`: the branch has `max_steps` points; `user`: Stop (an `abort`); `mark`: a Mark value set to stop (AUTO's UZR endpoint); `jacobian`: the exact Jacobian was not finite (W275: an error with an action follows); `noconv-min`: no convergence even at the smallest step (`min_step_size`); `noconv-fixed`: no convergence with a fixed step (`step_adapt_every` 0); `noconv-switch-min`, `noconv-switch-fixed`: the same while switching to a bifurcating branch; `noconv`: no convergence, how not noted; `snic`, `homoclinic`, `heteroclinic`: a branch of cycles whose period grew over 5 points to twice its shortest while its orbit lingers (a mesh point's speed at most 1e-6 of the fastest's) near the equilibria the `text` names: one with a zero eigenvalue, one saddle, two or more saddles in turn (W518: the end point's type is that name); `period_unbounded`: the same with equilibria that fit none of them, the end an `end`. A no convergence's `text` names the Newton settings that gave up (`max_newton_iterations`, `parameter_tolerance`, `state_tolerance`) and, when the last linear solve was singular, its smallest pivot (W491); it is logged as a WARN where AUTO's Output is not shown |
| `stop.text` | the reason in words, to follow "Stopped: " (the page's status strip); AUTO's Output gets the line `Branch 1 stopped at point 49: parameter iapp reached parameter_max (0.5)` for every branch that ends |
| `stop.br`, `pt` | the branch and point number of the end (positive, as in `diagram`) |
| `stop.value`, `limit` | what crossed the limit and the limit: the parameter and `parameter_min`/`parameter_max`, the norm and `norm_min`/`norm_max`, the point count and `max_steps`, the step size and `min_step_size`, the period for a global end (`snic`, `homoclinic`, `heteroclinic`, `period_unbounded`; the limit `null`); `null` where there is none |
| `notes` | what the run lost or could not trust, in words (W491): a special point AUTO gave up locating (the corrector did not converge, its test does not apply, or not in `max_location_iterations` iterations: the point is missing from the diagram), Floquet multipliers that are inaccurate (period doubling and torus detection are off until they are accurate again); `[]` for none, cleared when a run starts or AUTO's window is new. Each is also written in AUTO's Output (`Warning: ...`), or logged as a WARN where AUTO's Output is not shown |
| `start_refined` | `true` when the run's start was a steady state whose residual was above `state_tolerance` and Newton's method (the Sing pts settings) refined it before the run (W495, #559); `false` otherwise. Cleared when a run starts |
| `stab.eig` | steady states only: the eigenvalues λ = log z of the `circle` values, `[null,null]` where z is 0 (Re λ below about -745); the imaginary part is only known modulo 2π, its principal value |

Cycle-curve resonances use AUTO-07p's signed point codes: `-55` for R1 on
a fold curve, `-76` for R2 on a flip curve, and `-85`, `-86`, `-87`, `-88`
for R1, R2, R3, R4 on torus curves. Session diagrams preserve these typed
kinds and the curve's starting kind (W400, #462).

W404 (#466): maps label a Neimark-Sacker point `TR` and a flip `PD`. On map fold, flip and torus curves, `LPD` is fold-flip (28/78), `LTR` fold-torus (23/83), `PTR` flip-torus (77/87), and `TTR` torus-torus (88). Strong resonances use -25 through -28 on fold curves, -75 through -78 on flip curves and -85 through -88 on torus curves. These typed kinds use the same `special_points` booleans and session diagram pipeline as cycle resonances.

Numbers are doubles in the shortest of 15 or 17 digits that reads back
exactly, `null` when not finite. `tools/servercheck.py` checks that the
point is the `diagram` data's point it names and that the circle holds
what AUTO printed (its fort.9) for that point, and runs
tools/models/auto_stop.ode into each `stop` reason (`parameter_min` and `parameter_max`, `norm_max`, `max_steps`, no convergence with a fixed and at the smallest step, and
Stop).

### AUTO's settings as data

`autosettings.special_points` is an object of named booleans `bogdanov_takens`, `cusp`,
`zero_hopf`, `generalized_hopf`, `resonance_1_1` to `resonance_1_4`, `fold_flip`, `fold_neimark_sacker`, `flip_neimark_sacker`, `double_neimark_sacker`, all true by default. `auto.configure_numerics` accepts a partial object, e.g.
`{"cmd":"auto.configure_numerics","special_points":{"generalized_hopf":false}}`. Set all twelve
false to disable all equilibrium, cycle and map codimension-two detection. Invalid names or
non-booleans refuse the whole transaction. Session settings require all twelve
lines (`bogdanov_takens true` or `bogdanov_takens false`, etc.); validation precedes application.

What the AUTO window's Numerics, Axes (the AutoPlot form and
the plot type) and Mark values forms edit, as data, for a client with
forms of its own (web2, docs/ui-v2.md T22). **`autosettings`** is sent to a
client that asked with `{"cmd":"events.subscribe","events":["autosettings"]}` at the
end of that command, and then whenever the settings changed: at the end of
a command and before every ask. The settings exist from the model's load,
before AUTO's window opens (a model with more variables than AUTO takes
has none, and gets no event). core/auto_settings.cpp keeps it. Each setting has one name (W531), the same in the
event, `set`, the settings file, a model's `continuation` options and the manual; AUTO's own names (NTST, DS, MXBF ...) are
read on import only. `suppress_branch_points` and `switch_one_direction` are checkboxes in both forms and use `true` or
`false` in commands and saved settings (W499), `output_detail` one of the level names.

```
{"ev":"autosettings",
 "numerics":{"mesh_intervals":15,"max_steps":2000,"output_every":500,"collocation_points":4,"step_size":0.02,
             "min_step_size":1e-05,"max_step_size":0.02,"parameter_min":-0.2,"parameter_max":0.5,"norm_min":0,
             "norm_max":1000,"parameter_tolerance":0.0001,"state_tolerance":0.0001,"special_point_tolerance":0.0001,
             "mesh_adapt_every":3,"max_branch_switches":5,"switch_one_direction":false,"output_detail":"normal",
             "max_location_iterations":8,"max_newton_iterations":7,"full_newton_after":3,"step_adapt_every":1,
             "suppress_branch_points":false},
 "axes":{"plot":"maximum_and_minimum","var":"V","par1":"iapp","par2":"phi","xmin":-0.2,"xmax":0.5,"ymin":-0.5,"ymax":0.4},
 "marks":[["iapp",0.25],["T",30]],
 "threads":"auto","threads_auto":4,"threads_max":16,
 "rules":{"mesh_intervals":{"label":"Mesh intervals","integer":true,"min":1,"message":"Mesh intervals must be a whole number of at least 1"},
          "step_size":{"label":"Step size","nonzero":true,"message":"Step size must be a number other than 0"},
          "output_detail":{"label":"Output detail","choices":["none","little","normal","start_jacobian","extensive","debugging"],"message":"..."}, ...},
 "pairs":[{"lo":"min_step_size","hi":"max_step_size","strict":false,"message":"Min step size must be at most Max step size"}, ...],
 "step":{"key":"step_size","lo":"min_step_size","hi":"max_step_size","message":"The size of Step size must be from Min step size to Max step size (its sign is the direction)"}}
```

| field | meaning |
|---|---|
| `numerics` | the Numerics form's fields by their names, in the form's order, with AUTO's own name (a search word, read on import only) in brackets: `mesh_intervals` (NTST), `max_steps` (NMX), `output_every` (NPR), `collocation_points` (NCOL), `step_size` (DS; its sign is the direction), `min_step_size` (DSMIN), `max_step_size` (DSMAX), `parameter_min` (RL0), `parameter_max` (RL1), `norm_min` (A0), `norm_max` (A1), `parameter_tolerance` (EPSL), `state_tolerance` (EPSU), `special_point_tolerance` (EPSS), `mesh_adapt_every` (IAD), `max_branch_switches` (MXBF, 0 or more), `switch_one_direction` (MXBF below 0; boolean `true`/`false`, never 0/1), `output_detail` (IID: `none`, `little`, `normal`, `start_jacobian`, `extensive`, `debugging`), `max_location_iterations` (ITMX), `max_newton_iterations` (ITNW), `full_newton_after` (NWTN), `step_adapt_every` (IADS), `suppress_branch_points` (SuppBP; boolean) |
| `axes.plot` | the projection, by name: `maximum`, `norm`, `maximum_and_minimum`, `period`, `two_parameter`, `frequency`, `average` (a client labels it from its name; XPPAUT's numbers 0-4, 10, 11 are read only from an imported XPPAUT `.auto` file) |
| `axes.var` | the variable the y axis plots (Y-axis) |
| `axes.par1`, `par2` | Main Parm and Secnd Parm, two model parameters (`null` if none) |
| `axes.xmin` .. `ymax` | the diagram's axes |
| `axes` | all of it the active view's (see "Views of the diagram") |
| `marks` | Mark values: `[name, value]` per user point, where the parameter `name` (any model parameter) or the period `T` reaches `value` AUTO labels the point (UZ) |
| `threads` | AUTO's thread count, a program preference (W267) and not a setting of the model or the session: `"auto"` or a whole number from 1 to `threads_max`; the same for every model, read from `preferences.setx` (`auto_threads`) when a model is loaded |
| `threads_auto` | the count `"auto"` gives on this computer: its physical cores (the performance cores where the system says which), at most 4 |
| `threads_max` | the most `threads` may be: the computer's logical processors |
| `rules` | what each `numerics` value takes, by key (W118): `label` (the form's, derived from the key: underscores as spaces and a capital first letter), `boolean`, `choices`, `integer`, `positive`, `nonzero`, `min`, `max` (each only when it holds) and `message`, what a `set` of a value it does not take is refused with; the same table `set` checks with (core/auto_settings.cpp num_fields) |
| `pairs` | the values that must be in order: `lo` below `hi` (`strict`) or at most it, and the refusal's `message` |
| `step` | the first step `key` must be from `lo` to `hi` in size, whichever its sign, and the refusal's `message` |

Numbers are doubles in their shortest exact form, the whole-number fields
as integers; `null` when not finite.

`events.subscribe` also takes `diagram`, true or false, (W439, #501). Diagrams
are delivered initially; omitting this field keeps that choice. `false` stops
diagram collection and delivery for this client, while AUTO still computes and
stores its complete numerical diagram. `true` replays the current diagram,
including points computed while delivery was off. This switch is independent
of `events`; `events: []` stops the listed data subscriptions. A non-boolean
`diagram` is one command error and changes no subscription.

**`auto.configure_numerics`, `auto.configure_plot`, `auto.configure_marks` and `auto.set_threads`** (W516) write
the settings without the forms, each part only when given. `auto.configure_numerics` takes any of the Numerics
by their names and `special_points`; `auto.configure_plot` the axes: `plot_type`, `variable`, `parameter1`,
`parameter2` (model names), the four ranges `x_min`, `x_max`, `y_min`, `y_max`, `fit` for Axes/Fit afterwards and
`view` (a diagram view's name: its axes, made the active one; the active one's without it); `auto.configure_marks` the
whole list as two lists of equal length, `parameters` (a model parameter, or `T` for the period) and `values`
(both `[]` for none); mark storage grows with the session. Sent commands and
  session files have a 65,536-mark input allocation budget (W331, [#392](https://github.com/MuhammadMoustafa/xppautX/issues/392)).
  The legacy User action asks for a count, then a form with exactly that many values;
  cancel or one invalid value leaves the entire list unchanged.
Each command checks every value first and sets all or nothing (a refused part of one command changes nothing of
it): a whole number
where the form's field is one; `mesh_intervals`, `max_steps`, `output_every`, `max_location_iterations`,
`max_newton_iterations`, `full_newton_after` at least 1; `collocation_points` 2 to 7; `output_detail` one of its
names; `mesh_adapt_every`, `step_adapt_every` and `max_branch_switches` at least 0; the two booleans `true` or
`false`; `step_size` not 0; `min_step_size`, `max_step_size` and the three tolerances above 0; `min_step_size`
at most `max_step_size`, |`step_size`| from `min_step_size` to `max_step_size` (T23; checked when one of the three
is given), `parameter_min` below `parameter_max`, `norm_min` below `norm_max`, `x_min` below `x_max`, `y_min` below `y_max`;
names of parameters and variables the model has. A refusal is the command's `done` with `status` `error`, naming the
value: the message the event's `rules`, `pairs` or `step` gives it (`collocation_points=9: Collocation points must be a whole
number from 2 to 7`), so a client checks a form with the core's rules and
says what the core would.

`threads` (`"auto"` or a whole number from 1 to `threads_max`) is the thread
count (W267). It is checked with the rest, and saved by `prefs::update_prefs`
into the per-user `preferences.setx` (`auto_threads: auto` or
`auto_threads: 4`; `$XPP_CONFIG_DIR` names the folder) before anything of the
set is applied: a count outside the range, a word other than `"auto"`, or a
`preferences.setx` that cannot be read or written (it is never overwritten
while it is bad) refuses the whole set (`AUTO's threads must be
"auto" or a whole number from 1 to 16 (this computer's logical processors)`).
A pool already running keeps its count until the next run. The results never
depend on it (W247). It is not in `auto/settings.txt` or a session file, and a
recording's replay changes the session's count only, never the file.

AUTO's settings are saved in the session's `auto/settings.txt`
(core/xpp_session_auto.cpp, settings_text); Reload keeps them in memory.
The forms and `set` change the same fields and show what the other wrote.

`set` is a setting (W106): sent while AUTO (or anything) computes, the
core takes it at once and applies it after that job's `idle`, as a command
of its own (a continuation already running keeps the settings it started
with; the next run uses the new ones). `tools/servercheck.py` checks that the
event is what the forms show, that a form's OK and a `set` show in it,
that bad values are refused whole, and that `max_steps` set to 12 stops the next
run at 12 points.

### The numerics as data

The main numerics, the Numerics menu's values (W106), as data for a client
with fields of its own (web2's values panel). **`numerics`** is sent to a
client that asked with `{"cmd":"events.subscribe","events":["numerics"]}` at the end of
that command, and then at the end of any command that changed them.
core/numerics_settings.cpp keeps it.

```
{"ev":"numerics","fields":[
  {"key":"total_time","label":"Total time","value":30},
  {"key":"dt","label":"Dt","value":0.05},
  {"key":"store_every","label":"Store every N steps","value":1,"integer":true},
  {"key":"method","label":"Method","value":"runge_kutta",
   "choices":["discrete","euler","modified_euler","runge_kutta",...]},
  {"key":"jacobian","label":"Jacobian","value":"difference",
   "choices":["difference","exact"]},
  {"key":"stop_at_limit","label":"Stop at the limit","value":true,"boolean":true},
  {"key":"relative_tolerance","label":"Relative tolerance","value":0.001,"unused":true}, ...]}
```

Equilibrium and nullcline settings are set through their registry commands: `nullcline.set_mesh --mesh N`
and `equilibrium.configure --max_iterates N --newton_tolerance X --jacobian_epsilon X --search_limit X`.
The numerics event retains the keys `nullcline_mesh`, `max_iterations`, `newton_tolerance`, `jacobian_step`
and `search_limit`; the old `set` forms for these keys are unknown commands (W522, #586).

The fields, in the menu's order: `total_time` (the run duration, 0 or more;
a negative value is refused with its place), `start_time`, `store_from`, `dt`, `nullcline_mesh` (nullcline mesh),
`max_iterations`, `newton_tolerance`, `jacobian_step`, `search_limit` (sIng pt ctrl),
`store_every`, `state_limit` and `stop_at_limit` (a boolean: whether a variable past the limit ends the run),
`method` (its name, `choices` its names, W484 [#548](https://github.com/MuhammadMoustafa/xppautX/issues/548): the key of the
solver table, never its number), `jacobian` (W260:
how every Jacobian of the model is formed, `difference` by XPPAUT's forward differences or `exact`
from the model's own derivatives, by name as `method`), `relative_tolerance`,
`min_step`, `max_step`, `absolute_tolerance`, `eul_tol`, `eul_iter` (the method's own), `delay` (only
for a model with delays), `bvp_max_iterates`, `bvp_tolerance`, `bvp_epsilon` (bndVal). Each key
is the words of its label, and the XPPAUT words (`total`, `nmesh`, `newt_tol`, ...) are not keys
(W216). A choice's `value` is its name, as are its `choices` (a page shows each as its name with
underscores as spaces and a capital first letter, `runge_kutta` as "Runge kutta"; a label is never
written apart from the name); a yes or no is `"boolean":true` with a `true` or `false` value. `integer` marks a whole-number
field; `unused` one the current method does not use; `unavailable` the choices the model
cannot have, each by its name with why (`{"exact":"noisy.odex:2:1: uses random numbers"}`: a
model with no exact Jacobian, "Numerics" in docs/manual/06-numerical-parameters.md). The exact
Jacobian is made only when asked for (the model's own `jacobian: exact` option, or `set num jacobian exact`),
so a model's `exact` is offered until it was chosen once: the reason appears after that choice was refused.

**`{"cmd":"set","kind":"num","name":KEY,"value":V}`** (or `"text"`) sets one
field. The core checks the value first and changes nothing on a bad one,
with a `message` `error` naming the field (`Numerics: Store every N steps must be a
whole number of at least 1`): a number; `dt` not 0; the tolerances,
`state_limit`, `search_limit`, `newton_tolerance`, `jacobian_step` above 0; `stop_at_limit` `true` or `false`; `delay` at least 0; the whole-number
fields at least 1; `jacobian` exactly a name of `choices`, one the
model can have (`Numerics: Jacobian: exact is not available: WHY`); `method` exactly a name of `choices`, one
the model can use (Volterra only for integral equations, and only it then;
Symplectic only for an even dimension; methods that draw Wiener noise once per step for a model with Wiener noise, W379 [#441](https://github.com/MuhammadMoustafa/xppautX/issues/441); `unavailable` on `method` lists incompatible methods with the reason). W132 ([#184](https://github.com/MuhammadMoustafa/xppautX/issues/184))
uses the same solver picker for the menu, values panel, model options and
set import. A method refusal keeps the previous method; `@ method` rejects
unknown or unsuitable methods at its file and line. A number is no name: `set num method 3` is refused, while
the legacy `@ meth=8` key of a `.ode` (read on import only) means DoPri8(3).
Then it applies the numerics as
leaving the Numerics menu does (the delays' and integrals' memory for a new
`dt` or `delay`, a fresh solver; a method that picks its own steps stores
every output time, store_every 1). The Numerics menu asks for the same fields
with the same checks (W119): a value it refuses shows the same error and
keeps the value before. The fields are the numerics rows of the model's
option table (core/model_options.cpp), which also gives their `@` names
and defaults. A setting ("Commands during a command"): sent during a run it
applies when the run ends.

### Colours and line styles (W478, W479, W492; #541, #542, #556)

**Colours.** Every colour of ours, in every event, file and command, is one text: `#rrggbb`, `#rrggbbaa`
(the alpha, 00 clear to ff opaque, written only when not opaque) or `foreground`, the colour a page
draws dark on a light theme and light on a dark one and a picture draws black. A CSS colour name is read (a
form's colour field, a `text` command, a file) and written back as hex. There is no colour number anywhere
of ours; XPPAUT's 0 to 10 are read in a `.ode`, an `.xpprc` and a `.set` only, as the RGB of XPPAUT's own
table (`foreground`, then red `#ff0000`, `#f06400`, `#ffa500`, `#ffcd00`, `#c8c800`, `#c8eb4b`, `#00e100`,
`#00c8c8`, `#0000ff` and purple `#a020f0`). The alpha is drawn by the page, in SVG (`fill-opacity`,
`stroke-opacity`) and in PDF (an ExtGState). The colour of the colour scale (the array plot's cells, the
colour-coded orbit, the animation's expression colours) is a colour of the session's `ramp`.

**Line styles.** A curve, a frozen curve and a new curve's start (the option `line.*`) carry their style as
five keys: `draw` (`line`, `points` or `both`), `dash` (`solid`, `dashed`, `dotted` or `dash_dot`), `width`
(pixels, above 0), `marker` (`circle`, `square`, `triangle`, `diamond`, `cross`, `plus` or `star`) and
`marker_size` (pixels, 0 or more, a radius). XPPAUT's line type (-5 to 1) is read on import only. The page
draws them from `hello.line_styles`, and the PDF and SVG writers from the same table.

**Pictures.** `Graphic stuff` writes a PDF (one page, 640 by 400 points; Helvetica and Symbol, two of the
standard 14 fonts, nothing embedded; no date and no identifier, so the same plot is the same bytes) or an SVG, after
a form of the picture's parameters (`Colour`, a boolean; `Axes font size`, a number); `@ plotfmt=` picks
`pdf` or `svg` for `--mkplot` (a `.ode`'s `ps` imports as `pdf`). PostScript is gone.

### The plot as data

The page (docs/ui-v2.md) draws the plots itself from numbers. Five events carry them, each sent only to
a client that asked with
`{"cmd":"events.subscribe","events":["series","plots","nullclines","dfield","marks"]}` (any of
the names alone works too), at the end of that command and then at the end
of every command after which what it says has changed. Nothing else sends
them, so a command that only redraws sends none. They come before the
command's `state` and `idle`, in that order: `plots`, the `series`, the
`nullclines`, the `dfield`, the `marks`.

**`series`**: one event per plot window (any open plot `win`), each at the end of
a command after which what that window shows has changed: the stored data
(an integration, Continue, a browser Load or Replace, ...; this changes
every window), the window's curves (Xi vs t, Viewaxes, Graphic
stuff/Add curve, ...), or the window itself (a new one gets its series at
once). The active window's comes first. Switching the active window sends
none: the client has every window's already. A client that shows one plot
keeps the event whose `win` is the active one (`plots.active` or
`state.win`).

```
{"ev":"series","win":"graph_1","rows":601,"three":0,"xlabel":"","ylabel":"","zlabel":"",
 "curves":[{"x":1,"y":2,"z":1,"color":"foreground","draw":"line","dash":"solid","width":1,"marker":"circle","marker_size":3}],"shift":[0,0,0],
 "columns":[{"col":0,"name":"T","data":[0,0.0500000007,...]},
            {"col":1,"name":"V","data":[-0.143999994,...]},
            {"col":2,"name":"W","data":[0.0299999993,...]}]}
```

| field | meaning |
|---|---|
| `win` | an open plot window ID, as in `window` and `plots` |
| `rows` | stored rows: every column has this many values (0 before an integration) |
| `three` | 1 when the window is a 3D plot (then `z` matters) |
| `xlabel`, `ylabel`, `zlabel` | the window's axis labels (Viewaxes), the only place they are sent; a change of a label alone sends the series again; empty means "the plotted column's name" |
| `curves` | the window's curves (`MyGraph->nvars` of them): `x`, `y`, `z` are storage columns (0 is T, `i` is `cols[i]` of `browser`), `color` its colour ("Colours") and its style ("Line styles": `draw`, `dash`, `width`, `marker`, `marker_size`) |
| `shift` | row shifts of the x, y and z columns (a lag plot): point `i` pairs x row `i - shift[0]` with y row `i - shift[1]`, from row `max(shift)` on |
| `columns` | T and each column a curve uses, once each: `col`, its `name`, and `data`, one value per row (`null` for NaN) |

The values are the stored single-precision numbers printed with 9
significant digits, so they convert back to exactly the stored floats:
`output.dat` for the same run holds the same numbers printed with 8
(`tools/servercheck.py` checks this). The event carries the whole data every
time, and `version`, a number that changes when the stored data does (an
integration, a browser Load, ...): a series with the version of the one
before shows the same data again (other curves or style of it), another
version other data. The core keeps the series a new run replaces as an
earlier run, drawn lighter under the current one, until Erase or Redraw,
and sends it as a `runs` event ("Display state" below); an `append` from
row 0 starts such a run.

At the start of each integration (its first stored row) the server sends
`state` too, so the initial conditions the run starts from (Initialconds/Last
sets them to where the last run ended) show while it runs; `state.now` is
where the last run ended, or stopped.

**`plots`**: the plot windows as a list, sent when anything in it changed
(a window made or destroyed, the active one, a window's axes, curves
or 3D view); its text is compared with the last one sent.

```
{"ev":"plots","active":"graph_2","windows":[
 {"win":"graph_1","title":"graph_1","three":0,"xlo":-0.6,"xhi":1.2,"ylo":-0.25,"yhi":1.2,
  "box":{"xmin":-0.6,"xmax":1.2,"ymin":-0.25,"ymax":1.2,"zmin":-12,"zmax":12},
  "theta":45,"phi":45,"persp":0,"zplane":-1000,"zview":1000,
  "curves":[{"x":1,"y":2,"z":1,"color":"foreground","draw":"line","dash":"solid","width":1,"marker":"circle","marker_size":3}],"shift":[0,0,0]},
 {"win":"graph_2","title":"graph_2",...}]}
```

| field | meaning |
|---|---|
| `active` | the active window: the one keys, Viewaxes, Xi vs t, ... act on (`window.select` selects another) |
| `windows` | every plot window, by `win`; a window missing from the list was destroyed |
| `title` | the window's name, exactly as written by its user |
| `three` | 1 for a 3D plot |
| `xlo`, `xhi`, `ylo`, `yhi` | the window's axes (Viewaxes, Window/Zoom); in 3D the projected view's |
| `box` | the 3D box (3d-params, Viewaxes in 3D): the data ranges of x, y and z |
| `theta`, `phi` | the 3D view's angles in degrees (3d-params, `3`) |
| `persp`, `zplane`, `zview` | perspective on (1) or off, and its planes |
| `curves`, `shift` | as in `series` |

Numbers are the doubles themselves, in the shortest of 15 or 17 digits that
reads back exactly; `null` for a value that is not finite. `window.create`, `window.destroy` and `window.destroy_all` change the list; `window.select` with a window's name makes it active.

**`nullclines`** and **`dfield`**: what a phase plane shows besides its
curves, one event per plot window, each saying what XPP's window
shows: Nullcline/New, Dir.field/flow and the redraws that draw them again
fill them; anything that blanks the window empties them unless it draws them
again (Erase; Viewaxes to other variables, which redraws without them). So after Erase both are
empty, and a later redraw brings the nullclines back (they are redrawn
while Nullcline/Restore is on) but not the field Erase turned off. Each is
sent at the end of a command when the window's content differs from the
last one sent (compared value for value: drawing the same again sends
nothing), and for every window once after `events.subscribe`. The active window's
comes first.

```
{"ev":"nullclines","win":"graph_1","xname":"V","yname":"W","xcolor":"#f06400","ycolor":"#00e100",
 "x":[-0.596100688,0.475000024,-0.600000024,0.503065467,...],"y":[...],
 "frozen":[{"x":[...],"y":[...]}]}
```

| field | meaning |
|---|---|
| `xname`, `yname` | the variables of the x- and y-nullclines (where their derivative is 0): the window's x and y axes when they were computed; `""` before any |
| `xcolor`, `ycolor` | their colours (`#f06400` and `#00e100` unless the model sets them) |
| `x`, `y` | the x- and y-nullcline as line segments, 4 values each: `[x1,y1,x2,y2, x1,y1,x2,y2, ...]`, in plot coordinates, in the order XPP draws them; empty when the window does not show them |
| `frozen` | the frozen nullclines (Nullcline/Freeze) the window shows, the same colours, each set's `x` and `y` as above |

```
{"ev":"dfield","win":"graph_1","scaled":1,"color":"foreground","n":17,"du":0.1125,"dv":0.090625,
 "grid":[-0.600000024,-0.25,0.305723518,0.952120364,...],"speed":[0.0421,...],
 "flows":[{"color":"foreground","x":[-0.600000024,-0.59258616,...,null,...],"y":[...]}]}
```

| field | meaning |
|---|---|
| `n` | grid points a side (Dir.field's Grid + 1); 0 when the window shows no field (then `grid` and `speed` are empty) |
| `du`, `dv` | the grid's spacing in plot units, x and y |
| `grid` | one arrow per grid point, 4 values each: `[x,y,ux,uy, ...]`, the point and the field's direction there as a unit vector in plot coordinates (`0,0` where the field is 0), x outer, y inner, in XPP's order |
| `speed` | one per arrow: the length of the field's (x, y) components there, in plot units per unit time |
| `scaled` | 1 (Scaled Dir.Fld): every arrow the same length, a quarter of a grid cell's diagonal in XPP; 0 (Direct field): lengths in proportion to the speed, the fastest a whole cell's diagonal. The client scales the arrows to its own grid spacing on screen: a direction in plot units maps to pixels with the axes' scales, so it is normalized after that mapping. |
| `color` | the arrows' colour (the window's first curve's) |
| `flows` | Flow's trajectories in this window, one entry per curve of the window (its `color`): `x` and `y` of the points drawn, the trajectories one after the other with `null` (NaN) between two. Points that move less than 1/5000 of the axes from the last one kept are left out |

Values in these arrays are float32 (9 digits in JSON). Dir.field/flow's
Colorize (coloured cells) sends no arrows yet.

**`marks`**: what a plot window shows on top of its curves, one event per
window, saying what XPP's window shows there: the equilibria Sing
pts marked (its symbols), Text,etc's text, arrows, pointers and markers,
and Graphic stuff/Freeze's frozen curves. As with `nullclines`, drawing
fills it and anything that blanks the window empties it unless it draws
the marks again: a redraw (Redraw, Window/Zoom, Viewaxes, ...) draws the
text, objects and frozen curves again but not the equilibria, which
XPP's window loses too; Erase clears them all until the next redraw. A
freeze adds its curve at once (the window already shows it: it is the
current curve), and a deleted mark goes at the end of the command that
deleted it (Freeze/Delete and Remove all do not redraw). Sent at the end
of a command when the window's marks differ from the last ones sent, and
for every window once after `events.subscribe`; the active window's first.

```
{"ev":"marks","win":"graph_1",
 "equilibria":[{"x":-0.1425351775676935,"y":0.034048992272573395,"type":"saddle","symbol":"triangle"}],
 "text":[{"x":-0.298305094,"y":0.498503745,"text":"\\1a\\0-point 6","size":3,"font":0}],
 "arrows":[{"kind":"pointer","x1":0.1,"y1":0.2,"x2":0.6,"y2":0.8,"size":0.2,"color":"#c8c800"}],
 "markers":[{"x":0.9,"y":0.1,"shape":"diamond","size":2,"color":"#00e100"}],
 "frozen":[{"key":"first run","name":"frz1","color":"#ffcd00","draw":"line","dash":"solid","width":1,"marker":"circle","marker_size":3,"x":[...],"y":[...]}]}
```

| field | meaning |
|---|---|
| `equilibria` | the points Sing pts marked (Monte Carlo's too), in the order marked, each once: `x`, `y` in plot coordinates (the window's x and y variables at the equilibrium, the doubles themselves: the `equilibrium` event's values for those variables), `type` what its symbol says (`stable`: no eigenvalue with a positive real part; `saddle`: some on each side; `unstable`: the rest) and `symbol` XPP's (`circle`, `triangle`, `box`). 2D windows only |
| `text` | Text,etc's labels: `id` the label's id, which the `annotation.*` commands name it by (`annotation_id`) (it stays its own while the label lasts; a deleted label's is never used again), `x`, `y` where the text starts (its baseline, plot coordinates), `text` as drawn (`\{expr}` already replaced by its value), `source` as written (what the edit dialog starts from), `size` 0-4, `font` (1: all in the symbol font; 0 otherwise), `style` (`normal`, `bold`, `italic` or `bold_italic`, as `hello.text_styles`) and `color` (a colour; the page draws style and colour, the PDF and SVG output the size and the colour, not the style). XPP's markup stays in the text: a backslash and `1` switches to the symbol font, where the Latin letters show Greek ones (`\1a` is α, Adobe Symbol encoding), `0` back to roman, `s` and `S` a subscript and a superscript one size smaller (they add up), `n` back to the baseline and size; any other character after a backslash is dropped. Bytes that are not UTF-8 are Latin-1 |
| `arrows` | Text,etc's arrows and pointers: `id` (as the text's), `kind` `arrow` (only a head) or `pointer` (a head and a shaft), the head's tip at `x1`, `y1`, pointing from `x2`, `y2`; `size` the head's length as a fraction of that distance (half as wide as long); `color` a colour |
| `markers` | Text,etc's markers (Marker, marKers): `id` (as the text's), `x`, `y`, `shape` (`box`, `diamond`, `triangle`, `plus`, `cross`, `circle`), `size` (1 is about 1% of the window), `color` a colour |
| `frozen` | the frozen curves of the window (2D ones): `slot` (the number `plot.freeze_curve` gave it, which `plot.edit_frozen` and `plot.delete_frozen` take), `key` (its legend text) and `name` (Freeze's Edit form), `color` its colour and its style (`draw`, `dash`, `width`, `marker`, `marker_size`, as a curve's), `x`, `y` the values of the curve's x and y columns when it was frozen (float32, as `series`: equal to that `series`' columns then) |

Label and object positions are the stored float32 values, 9 digits.

**Binary values.** After `{"cmd":"events.subscribe","events":["series"],"encoding":"f32"}`
every `series` event (full or append) has `"enc":"f32"` and each column's
`data` is a string (and so is every value array of `nullclines`,
`dfield` and the frozen curves of `marks`, which then carry `"enc":"f32"` too): the base64 (RFC 4648, with padding) of the values as
IEEE float32, 4 bytes each, least significant byte first, whatever the
host's byte order. NaN and infinities travel as they are. That is 5.3
characters a value instead of about 12, and nothing to parse: a million
rows of three columns is 16 MB, which a browser decodes in milliseconds
(`Uint8Array.fromBase64`, or `atob`). The decoded floats are exactly the
JSON numbers read as float32. `events.subscribe` without `encoding`, or with `json`,
means JSON numbers. A client should read `enc` on every event: a server
that does not know the option sends JSON numbers.

**Live runs.** While an integration runs (Initialconds/Go, Continue, a
range, ...), the rows it stores go out as they come for
the active window, at most about once a display frame (60 times a second):

```
{"ev":"series","op":"append","win":"graph_1","from":1200,"rows":2400,
 "columns":[{"col":0,"data":[...]},{"col":1,"data":[...]},{"col":2,"data":[...]}]}
```

| field | meaning |
|---|---|
| `from` | the row the first value of each column is: the client keeps its rows `0..from-1` and drops any others it holds |
| `rows` | the rows the client holds after this event: `from` plus the number of values |
| `columns` | `col` and `data` for the columns of the window's last full `series`, in its order (no names) |

A new integration starts again from row 0, so its first append has `from`
0; Continue's first has `from` equal to the rows already there. The
appends of one command are contiguous: each `from` is the previous
`rows`. When the window's curves change during a command (so the columns
would not be the last full series'), the server sends a full `series` of
the rows stored so far instead and appends after it. A command that
appended ends, before its `state` and `idle`, with the rows stored since
the last append (one more append) and then

```
{"ev":"series","op":"end","win":"graph_1","rows":20001,"version":7}
```

instead of the whole series again (a million rows are tens of MB): the
client's series is then whole, `rows` rows (what the appends delivered),
and `version` is its data's version, as a full `series` would carry. No
append follows it. When the window's curves changed after its appends,
the command ends with the full `series` instead. A command without
appends ends with the full `series` of a window whose data changed. The
other windows get only their full `series`, after the active one's. A
client that did not ask for `series` gets none of these.

The appends come from the integrator itself (`rows_stored()` in
core/xpp_ui.h, called for every stored row; `plot_data_rows_stored` in
core/plot_data.cpp sends at most one append per 1/60 s, so a page that
draws each on its next animation frame extends the curve at every frame;
W82).

Import settings (File/Import settings, key `r` of the File menu, the
`importset` command; `--setfile`) takes a `.setx` (docs/odex.md ".setx") or
an XPPAUT `.set` (W284, replacing "Import XPPAUT set"). A `.setx` is read whole
and checked, every error at its file, line and column, and only then applied:
the groups and keys it names are set, the rest left as it was. An XPPAUT `.set`
(positional lines, a value and its label, ending with its model's equations,
`RHS etc ...`) is read the same way, every named value checked against the
model, and converted: its values are written as `<base>.setx` beside it (a
`.setx` already there with other text is replaced only after the question
`.ode` to `.odex` asks: `file_replace_choice`), and that `.setx` is read and applied. One
`message.bottom` names the file applied. No session file is written.

### Undo and redo of value edits

W210: the Session keeps a stack of snapshots of the parameters, the initial
conditions (and a delay equation's history) and the numerics, the values a
Reload carries over. Each of `set`, `value.set_parameters`, `value.set_by_formula`, `value.slide`, `set.apply`, `set.restore_defaults`, `value.load_file` and `initialconds`' Last (Run from last state, which overwrites
the initial conditions) pushes the values as they were before it applies;
a push forgets what could be redone. The registry commands `value.undo` and `value.redo`
(kind `setting`: during a run they wait
for its end like any value edit) bring back the top of the undo or the redo
stack in one step, whatever the snapshot held (the values panel, the tables
and the delay setup follow), and put the values they replaced on the other
stack. With nothing to take back or put back they fail with an `error`
naming the command (`state.can_undo` and `state.can_redo` tell the client
first). The stacks hold the last 100 snapshots (`MAX_VALUE_UNDO`,
core/value_undo.h), the oldest dropped first; a load starts them empty. Runs,
plots, data, files and the session file are not part of a snapshot, so an
undo does not change which file Ctrl+S saves to.

## Session files

A session file, `name.snapx` (W57, core/xpp_session.cpp; its pure part,
the member names, the manifest and the model's members, core/snapx.h),
is a zip of ordinary files, in this order:

| Member | What it holds |
|---|---|
| `session.setx` | the manifest (W285): a `session` group of `version` (2), `model` (the model's own file, as the model names it), `anifile` (the animation `--anifile` loaded, one of the model's files; only when there is one) and `data` (1 when `data.npz` is there); the settings syntax |
| `model/<name>` | the model (W103): its `.ode` or `.odex` first (`model/` and `name`), then every other file its load read, each by the name the model gives it (a path as the model writes it, relative to its folder or whole): the files it includes, its file tables, `--anifile`'s animation; byte for byte |
| `model.setx` | the session's settings (docs/odex.md ".setx"): the numerics (the Jacobian included) under the option table's keys, the run's position, the initial and ending values, the parameters, the delays, the boundary conditions, Transpose, the coupling, the array plot, the torus and the ranges; every key in it (a session loads nothing it does not hold), without the graphics, which `windows.setx` holds for every window |
| `auto/settings.txt`, `auto/diagram.csv`, `auto/solutions.s`, `auto/views.txt` | AUTO's settings and views (a line `view "NAME" shown|hidden PLOT ...` per view: whether `window.hide` hid it); full-precision diagram and restart orbits when a diagram exists (xpp_session_auto.h); each diagram point's `jacobian` column is 0 (difference) or 1 (exact), the setting of the run that computed it, required: a `diagram.csv` without it is refused with its file and line (W278) |
| `windows.setx` | a `windows` list, every open plot slot as a group in index order (its quoted `name` and its `curves` by variable name with colour and style (`color`, `draw`, `dash`, `width`, `marker`, `marker_size`), the labels, the rotation, scales and box (`rm`, `xlo`, ...), the `zoom` and the earlier-runs toggle `previous_runs`, and `hidden`, whether `window.hide` hid it), `active` (the quoted window name), `browser_hidden` (whether the data browser is hidden), an `auto_view` group (AUTO's hidden branches: `earlier`, `show_earlier`) and the browser's `added_columns` (name and formula) |
| `marks.setx` | the `labels` list (window, x, y, size, font, style, color, text; each value checked, a text 1 to 1000 characters, at most 50), the `arrows_and_markers` list, `freeze_each_run` and the `frozen_curves` list of used slots in slot order (zero-based slot, boolean used, window, type, color, the style keys of a curve, points, key, name), per window |
| `frozen.npz` | the frozen curves' points, one (points, 3) array `curve<slot>` each, when there are some |
| `data.npz` | the data table as Save data's NPZ writes it (`T.npy`, `V.npy`, ..., `seed.npy`), unless left out |
| `sliders.setx` | mandatory; a `sliders` list (at least the three model-option slots, at most 256), each slot a group of `name` (empty clears it), `low`, `high` and `step`; all names and ranges are checked before anything applies |
| `nullclines.setx` | mandatory; a `current` group and a `frozen` list of groups (at most 10000): each pair has its `x_axis` and `y_axis` (variable numbers) and its `x_segments` and `y_segments`, lists of four finite float coordinates per segment (at most 10000 segments a curve); axis indices and the segment limit are checked |
| `random.txt` | the random numbers' state as text: a `seed N` line (the seed the next Go uses), a `wiener` line (the Wiener parameters' current values), then the generator's state on one line, the last (the Mersenne Twister's, and whether it holds a normal deviate over, 0 or 1, and that deviate); opening restores it, so a stochastic model goes on from the same numbers |

W376 (#438): saves omit closed windows and unused frozen slots. The saved
numbers preserve gaps and recording IDs; they must increase, with window
zero first and open. Before allocation, parsed entries, runtime slots
(including gaps), curves and frozen points share a 512 MiB estimated
expansion budget, the archive's byte limit, rather than a fixed count.
An excess fails at its member's line with `too many ... for the session
memory limit`. Active windows and mark references must name open windows.
A frozen curve's `points` must match its actual NPZ arrays exactly. The
settings tokenizer and archive/NPZ readers retain their file-size and
recursion protections. Every member is validated before applying any of
it; this is also the snapshot pipeline for recordings. Layouts missing
`open` or `used` are refused.

The generator line has one portable layout (W169, #221): 312 decimal
unsigned 64-bit MT19937-64 state words in array order, the next-word index
(0 through 312), the spare-normal flag (0 or 1), and its finite binary64
value written with 17 significant decimal digits in the classic locale.
The recurrence and initialization are MT19937-64's standard parameters.
Already saved libstdc++ states have this layout; a different library's
layout is rejected at `random.txt`'s line, without conversion or fallback.
Recording scratch roots and their parent folders are compared by real
path, allowing linked temp-folder ancestors such as macOS's `/var`;
links inside scratch and writes through subfolders remain refused.

Opening one (`session.open_model`, `session.open`, the command line) loads the model
saved in it, from those saved files alone: an edit to the `.ode` since,
or its absence, makes no difference, and a file the model reads that is
not saved in it is a file that is not there. Then it restores the
members: `state`, `plots`, `marks` (but Sing pts' equilibrium symbols),
`autoview` and the diagram in each of its views are as they were at the
save, the diagram exactly (every digit). The earlier runs a window keeps until Erase are
not saved. A file without its model (`model/<name>` missing, as in every
session file saved before W103), or without its manifest, is refused (a
`message` `error` says what is missing) and nothing changes. So is one
whose manifest has a key it does not write, or a key twice (`s1.snapx/session.setx:5:3:
not a session file (.snapx) of this version: `later` is not a setting of the file`), and a session of the
older layout (W285: `session.txt` and XPPAUT's set lines, no `session.setx`): the ordinary error, with no logic to explain
it; and (W116) one with a member it always
writes missing (`model.setx`, `windows.setx`, `marks.setx`, `sliders.setx`, `nullclines.setx`, `random.txt`, which must also load as a generator state; `data.npz` when
`data` is 1, and only then; AUTO's four when one is there; the points of
every frozen curve `marks.setx` lists) or one whose member does not read: a
key missing or a value that is not a number, a value out of range, a key it
does not know, a variable, a window or a slot it does not have, a value
of AUTO's it does not take; and (W145) a numerical setting its rule
refuses, as `set` `num` and an `@` line refuse it (0 `store_every`, 0 `DeltaT`),
more added columns than the model has room for or one whose formula does
not compile, an arrow's or marker's type out of range, a colour that is none, a
manifest line given twice. Every member is read whole and checked
against the model's load before it is kept, and only then applied (W125):
the session before stays exactly as it was, its model included; the
error's place is the member's line and column, the member named inside its file
(`file` `s1.snapx/windows.setx`, `line` 11, `col` 7, `source` the line as written):
`s1.snapx/windows.setx:11:7: s1.snapx could not be loaded (the model has no
variable "nosuch"); lecar.odex is still loaded` (at the start, the
command line's file, the `error` event, then exit 1). A member missing
is named on the file itself (`s1.snapx: its random.txt is missing`). The command
line opens one too, `xppautX name.snapx` (every mode but `--silent`),
from its own folder.

A recording, `name.recx` (W59c), is registered with the OS beside them (the
same scripts in `tools/associate/`), and `xppautX name.recx` (every mode
but `--silent`) starts the recording's own model and opens the recording in
the player and starts playback automatically, as `recording.open` does, without the question
File/open Model asks (nothing is open to save).

### AUTO diagram files

W339 ([#400](https://github.com/MuhammadMoustafa/xppautX/issues/400)) removes
the `autosettings.pars` field and `auto.configure_plot`'s `pars` (an unknown argument).
Use `state.pars` for all model parameter names and `axes.par1`/`par2` for
the continuation. `auto/diagram.csv` begins with contiguous
`branch,ID,VALUE...` records, with one value per model parameter in model
order. The point header follows: the previous metadata, then `branch`,
`continued` and `values` (semicolon-separated indices and full-precision
values), then the state and spectrum columns. Point indices name model
parameters directly; AUTO's numerical indices are distinguished from them.
Only continued values and the period vary at each point. Grab and label
restart resolve them over the branch's immutable parameter record.
Old xppautX session layouts are rejected; XPPAUT `.auto` import remains.
XPPAUT omitted parameters cannot be recovered from a foreign file and use
the importing model's values.

Session ZIP input and total decompressed members are limited to 512 MiB,
with at most 4096 entries; duplicate file members are refused. Saved AUTO
views are limited to 4096 and diagram points to 1,000,000, also capped by
the 512 MiB budget for their expanded vectors. Restart headers, complete
payloads and parameter indices use AUTO's shared reader checks; restart
meshes are bounded by the same byte budget at AUTO's largest dimension. Member text,
settings, dimensions and parameter indices are checked before the new
model is kept. Members remain virtual files, with temporary model copies
named by their numeric index; archive paths are never extracted.

AUTO's File > Save diagram (`s`) calls Save session (`.snapx`), with the
same data-table choice. Load diagram (`l`) opens a `.snapx` through Open
session: it asks whether to save first and replaces the whole session,
even when its saved model is already open. The diagram's full-precision
points, settings, views and restart orbits are the session's `auto/`
members ("Session files"). Import diagram (`f`, wildcard `*.auto`)
imports a foreign XPPAUT diagram into the model open, after a reset asks
for confirmation; no `.auto` file is written.

### Display state

What the page displays is the core's (W65, docs/ui-v2.md): the earlier runs
of each plot window until Erase, its zoom, whether the earlier runs are
drawn, and AUTO's hidden branches and zoom. They are members of the
session (`xpp::Session::plot_display`, `auto_view`, core/display_state.h),
set by `window.set_display` or by the runs themselves, and sent as data,
so a session file can save them. The page keeps a copy it changes at once
(a drag cannot wait for a round trip) and sends the change; the core's
events set it back, and it leaves a window's own alone while its change is
on its way.

**`runs`** (with `series`): a window's earlier runs. A full series with
other curves (Xi vs t, Viewaxes, ...) forgets them; a full series of other
data (its `version`) keeps the current run as an earlier one, unless the
current one grew by appends in this command (the `end` closes that run)
or was erased; an append that starts again before the rows the client holds
(a new run under way, the next run of a range) keeps the current run first.
A run kept so is the series the client holds already, so the event says
`keep` instead of sending its data again. Erase forgets them and hides the
current run (`erased` 1) until its next run or Redraw. At most 50 earlier
runs per window are kept, and 16 million values (rows times columns) in
the earlier runs of all windows together: a window keeping a run drops its
own oldest ones first, never its newest.

```
{"ev":"runs","win":"graph_1","erased":0,"clear":0,"drop":0,"keep":0,"enc":"f32","add":[
 {"rows":601,"three":0,"curves":[...],"shift":[0,0,0],
  "columns":[{"col":0,"name":"T","data":[...]},{"col":2,"name":"W","data":[...]}]}]}
```

| field | meaning |
|---|---|
| `win` | the plot window |
| `erased` | 1: Erase blanked the window, the current series is not drawn |
| `clear` | 1: forget every earlier run held first |
| `drop` | forget this many of the oldest ones first |
| `keep` | 1: then the window's series, as the client holds it, becomes the newest earlier run (a new run replaces it) |
| `add` | runs to append after that, newest last: `rows`, `three`, `curves`, `shift` and `columns` (`col`, `name`, `data`) as in `series`, of the columns the run's curves use |

Sent when it changes (not at all for a window with none), and whole (`clear`
1 with all the runs) after `events.subscribe` asks for `series` again.

**`plots`** carries each window's `zoom` (`{"x":[low,high]|null,"y":...}`,
the part of its axes shown; `null` the window's own) and `runs` (1: the
earlier runs are drawn) and `freeze` (1, the default: a trajectory a new run
replaces is kept as an earlier run until Erase; 0: it is dropped, and only the
current run is drawn). Other axes or curves of the window (Viewaxes,
Window/Zoom, Fit, Xi vs t) drop the zoom. `window.set_display` sets them:
`{"cmd":"display","window":"graph_1","x":[0,10],"y":null,"runs":false,"freeze":true}` (a missing key
is left as it is).

**`autoview`** (with `autoinfo`): `{"ev":"autoview","earlier":n,"show":0,
"active":"diagram_1","views":[{"name":"diagram_1","zoom":{"x":null,"y":null}},...]}`, sent at the end of
a command when it changed, and at once after `events.subscribe`. `earlier`: the
diagram's points before this index are the branches computed before Clear
(the AUTO window's key `c` sets it to the points so far, in every view; a
diagram of fewer points, File/Reset diagram, lowers it, closing AUTO clears
it); `show` 1: they are drawn; `active` the active view; `views` one entry
per view of the diagram (W50), its `zoom` as above, dropped by other axes
of that view (closing AUTO drops every view's zoom; the views stay).
`{"cmd":"auto.set_display","view":"diagram_2","show_earlier":true,"x_range":[...],"y_range":[]}`
sets `show` and `"diagram_2"`'s zoom (the active view's without `view`; `[]` is the whole range of an axis).

## Question answering

Every core question goes through `xpp_ui`; the active front end's table
supplies the context and this owner decides who answers (W304,
[#363](https://github.com/MuhammadMoustafa/xppautX/issues/363)). The window
and `--server` ask the user. `--silent` and `--convert` ask the terminal,
reading a console or pipe line with a `TERMINAL_QUESTION_SECONDS` wait
(60 seconds); EOF or that deadline means no answer. `-y` / `--yes` takes
the suggested answer and prints `QUESTION -> ANSWER (answered automatically)`
on stderr. `--check` takes no answer, even with `--yes`, and writes no
conversion or playback output. A question declined there is a diagnostic.
A string's empty terminal answer accepts its suggestion; a choice accepts
its key or its label; confirmation accepts `y` or `yes`. No answer cancels
an entry or declines confirmation. An outside-path question's suggested
answer allows once; no answer refuses permission.

A recording or built-in command script supplies the answers held in that
command, independently of these user questions; recorded answers in
`--silent` are printed in the same format. A recording's launch-folder
output replacement is an independent question and uses the answering
policy above, never a recorded save decision.

### Asks

| kind | fields | answer fields |
|---|---|---|
| `choice` | `name`, `title`, `choices`, `keys`, `hints`, `def` | `key` (empty or `ok:0` cancels); for an item past the end of `keys` (a menu with more items than keys: the named sets past the 26th, W229), `index`, the item's place in `choices` |
| `choice` | `title`, `question`, `choices`, `keys` | `key` |
| `string` | `title`, `name`, `value`, `ok`, `cancel`, `kinds` | `value` (any length: no dialog cuts what is typed, W76) |
| `form` | `title`, `names`, `values`, `kinds` | `values` (same length, each of any length: W76 dropped `max`). A name starting with `*n` means the field picks from `hello.lists[n]`: a variable (`*0`), a parameter (`*2`), a marker (`*4`), ...; for a list whose items start with a number (`2 Box`) the value is that number. A form may also carry `columns` and `rows` (arrays of text), asking for a grid: its first `columns.length * rows.length` fields are the cells, row by row (field `r * columns.length + c` is column `c`, row `r`), and the others follow as plain fields. Viewaxes' 2D View (`X`, `Y`; `Variable`, `Min`, `Max`, `Label`) and 3D View (`X`, `Y`, `Z`, then `XLo`, `XHi`, `YLo`, `YHi` and the 3D params' ten fields) are the only grids (W225): the core reads every field whole before it sets any, and a variable that is not one, a number that is not one, a Min not below its Max (or XLo/YLo not below XHi/YHi) is a `message` error naming the field, with nothing applied. |
| `checklist` | `title`, `names`, `flags` | `flags` |
| `file` | `title`, `mode` (`read` or `write`), `file`, `wild`, `dir`, `save_dir`, `dirs`, `files` | `file` (a name in `dir`, or a full path anywhere, whole: the desktop window's own dialog answers with one, W88); a native save dialog also supplies `replace:1` and passes the ask `id` as `ask` to `__xppFileDialog`; the binding grants that exact path once for that prompt, so `replace:1` alone never authorizes an external write (W177, #229), while a browser save supplies no decision for the core's model-folder copy (W129); or `cd` (a folder name or `..`, in `dir`: only the folder listed changes, not the process's) or `wild` (a new pattern) to be asked again with that listing; `save_dir` is the writable model folder offered for a native save retry after permission denial; `dir` starts as the folder of the file of that `wild` last chosen, else the model's folder (a replay's recording's), never AUTO's or a replay's scratch folder (W151) |
| `text` | `title`, `value` (the text), `size`, `style`, `color` | `value`, `size` (a whole number in the range of `hello.text`), `style` (one of `hello.text_styles`), `color` (a colour); a value outside them is a `message` `error` and the text is not added. Text,etc/Text asks a `mouse` first (where the text goes), then this; a cancel adds nothing. |
| `mouse` | `win` | `x`, `y`; or `xd`, `yd` (data coordinates, below) |
| `rubber` | `win`, `flag` (0 box, 1 line) | `x`, `y`, `x2`, `y2`; or `xd`, `yd`, `xd2`, `yd2` |
| `grab` | `win` | `key`; or `x`, `y` (or `xd`, `yd`) for a click on the diagram; or `point`, a point of the `diagram` data by its index (with `key`, that key after it) |
| `drag` | `win` | `what` (`down`, `move`, `up`), `x`, `y` (or `xd`, `yd`) for each pointer event; cancel or a key ends. Window/Scroll and AUTO Axes/Scroll ask it again after every event. |
| `pixels` | `win`, or `film` (a kinescope frame index) | `w`, `h`, `rgb` (base64 of w*h*3 bytes). Frame, GIF and kinescope writers use it: only the client has the picture, which web2 renders from the data it holds (the window's chart, or a kinescope frame's snapshot). |

Range Integrate, Double Range Integrate and Range Equilibria are typed forms
(W487, [#551](https://github.com/MuhammadMoustafa/xppautX/issues/551)): their
yes/no fields (`Reset storage`, `Use old ic's`, `Cycle color`, `Movie`,
`Shoot`, `Monte Carlo` and `Write each run to its own file`, W349,
[#410](https://github.com/MuhammadMoustafa/xppautX/issues/410)) are
`boolean` fields answered `true` or `false`, the steps are `integer`
fields of at least 1, Start and End are `real`, and the double range's
`Vary both` is a `choice` of `together` or `independently`. Every answer is
read by its kind before any is used, a bad one is an error naming its field.
`Write each run to its own file` controls `sweep.rangefiles` and matters only
with Reset storage on. The existing `form` ask and `values` answer carry them.

A range run that fails (a solver's error) ends the sweep with that error,
prefixed `Range stopped at <parameter> = <value> (run n of N): `. A run that
ends early without failing (a state at `state_limit`, an event's `arret`, a
section that stops the run) is reported once at the end as a `message` with a
`bottom` line, `n of N runs ended early; the first at <parameter> = <value>: <why>`,
and one WARN log line per such run.

**Field kinds.** A `string` or `form` ask says what each of its fields
takes, in `kinds`: one entry per field (a `string` ask has one), from the
call site that knows it (core/xpp_ui.h `XPP_FIELD_*`: `new_int`,
`new_float`, `new_string_of`, `get_dialog_of`, `do_string_box_of`).
`integer`: a whole number, digits with a sign (the core reads it with
`atoi`; new_int). `number`: a decimal number (`atof`). `formula`: a number,
or `%` and a formula the core evaluates (new_float). `expression`: a
formula of the model's quantities (a column's formula, the calculator,
initial data from a formula). `file`: a file's base name. `name:N`: a name
from `hello.lists[N]` (`name:0` T or a variable). `text`: anything. Every
field of a prompt whose call site says nothing is `text`, and a client
reading an ask without `kinds` (an older core) treats every field as
text. For the kinds above the kinds are a client's guide, not a check: the
core reads what it is answered as it always did. A typed form (W485, #549;
core `ask_form`) is different: every field is checked by its kind, all of
them, before any is used, and a bad value is a `message` `error` naming the
field (`Poincare map: Direction: "up" is not one of negative, both,
positive`), nothing applied. Its extra kinds: `boolean` (the answer is the
text `true` or `false`; a checkbox), `choice` (one of the names in the
form's `options` entry for the field, exactly; a select whose labels are the
names with underscores as spaces and a capital first letter, derived, never
written apart) `real` (a plain number, no `%formula`; `integer` takes
the same) and `color` (`foreground`, `#rrggbb`, `#rrggbbaa` or a CSS colour name; the page's picker gives the first three: a colour and its alpha). The form ask carries `options` (per field the names of a `choice`,
`null` for another kind) and `ranges` (per field `{min, max}` of an `integer`
or `real`, either key only when given, `null` when it has none) beside
`kinds`; the answer stays `values`, one text per field. Poincare map's form
(Numerics, Poincare map) is the first: Section `real`, Direction `choice`
(`negative`, `both`, `positive`), Stop on section `boolean`. A `*n` field (above) picks from its list
whatever its kind. web2 marks a field whose text its kind does not take
and does not answer until it is corrected (web2/src/store/fieldKinds.ts);
`tools/servercheck.py` checks the kinds of a `formula`, a form of
numbers, an `integer` and a `name:0` ask.

**Data coordinates.** A point of a `mouse`, `rubber`, `drag` or `grab`
answer can be given in the plot's own quantities instead of pixels: `xd`,
`yd` for the point (`xd2`, `yd2` for a rubber band's second corner), in the
coordinates `state.view` maps to (the active window's `xlo`..`xhi`,
`ylo`..`yhi`; for the AUTO diagram, the selected AUTO view, those of `state.auto` or
the `diagram` `axes`). The server turns them into the nearest pixel of its
window at the axes it has when the answer comes (the inverse of the
mapping `state.view` describes), and the command goes on exactly as for a
click at that pixel: Window/Zoom by `xd`..`xd2`, `yd`..`yd2` gives that
box within a pixel, and Initialconds/Mouse at `xd`, `yd` starts from that
point within a pixel. A point outside the window is fine (its pixel lies
outside too). When both are there, `xd`/`yd` win over `x`/`y`. A client
that draws the plot itself (web2) answers in data coordinates and needs
no pixel geometry. `tools/servercheck.py` checks that a box in data
coordinates zooms exactly as the same box in pixels.

**Grab by point.** A grab is a cursor on the diagram's points that the
client moves until a key takes the point (`Return`) or cancels (`Escape`,
or `ok` 0): arrow keys, `Tab` (the next labelled point), `Home`, `End`,
`PageUp` and `PageDown` as in the X11 program, a click (the nearest
point), or `{"point":i}`, point `i` of the `diagram` data as the client
holds it (the order of `add`), which moves the cursor to exactly that point.
Each answer that moves it brings the new point's `autoinfo` (to a client
that asked for it) and asks again. `{"point":i,"key":"Return"}` moves and
takes in one answer. An index the data do not have (out of range) is
ignored, and so is the key that came with it: the grab asks again. So is a
point AUTO no longer has: after Reset diagram or a load the data are the
old diagram until reDraw.

**Grab by label.** `{"cmd":"auto.grab","label":N}` grabs the
stored point labelled `N` directly, with no ask: the same outcome as the
interactive grab above ending with `Return` on that point (`grabpt`, the
parameters, the info strip, the stability circle, and what a following
`auto.run` with a `label` starts from). `{"cmd":"auto.grab","type":"hopf","index":k}`
grabs the `k`-th (1-based, in stored order) labelled point of that AUTO
type instead (a point type's name, the same names as hello's `point_types`: `branch_point`, `end`, `hopf`, `fold`, `no_convergence`, `period_doubling`, `neimark_sacker`, `user_point`, and the others: "the 2nd hopf"). A label no stored point has, or a `type`/`index` with
no such point, is an error in the `done` and nothing changes -- the diagram,
`grabpt` and the info strip stay as they were. The interactive grab is
the AUTO window's key `g`, `{"cmd":"key","win":"auto","key":"g"}`,
which asks (`grab`, above). A script that recorded an interactive grab (W59) replays it as the
keys it was answered with, exactly as it was driven; `auto.grab` is for
a script that names the point instead (W56's --silent commands).

**Running AUTO: `auto.run`** (W516, W306). `{"cmd":"auto.run","kind":K,...}` runs AUTO and ends with its `done`
(`points_computed`, `branches_tracked`); an error of the run (`AUTO stopped: ...`) is the `done`'s error, shown
once. `kind` is what is computed: `equilibrium`, `periodic`, `two_parameter` (the curve of the start point's type),
`fixed_period`, `bvp`, `homoclinic`, `heteroclinic`; `switch: true` is a branch switch (at a branch point, or the
period doubling at a period doubling). The start is one of `label` (a stored point), `type` with `index` (the
`k`-th point of that type, as `auto.grab`), or neither: the current state (the initial conditions and the last
integration, for `periodic`, `bvp` and the connections), settled first by the `steady` command's rule when
`decimals`, `hold` and `maximum` are given. The start point's type is never an argument: it comes from the point,
through one table, hello's `auto_run` (`kinds`, the `current` kinds, and `rows` of `point`, `family` and the `kinds`
and `switch` kinds they allow; the first row of the point's effective type and family, `other` for any other type,
the `neutral_saddle` curve's own), and `autoinfo`'s `start` is the grabbed point's row (`label`, `type`, `family`,
`kinds`, `switch`; `null` when none is grabbed), so a page offers only the kinds that fit. A kind that does not fit is
an error naming both (`two_parameter needs a point of type hopf, ...; label 12 is a regular point of a steady
branch`). A new start from the current state with an `equilibrium` run needs `replace_diagram` when the diagram has
points: `true` begins it again, `false` adds the branch. The Numerics' names (`step_size`, whose sign is the direction,
`max_steps`, ...) given to `auto.run` hold for that run only; an omitted one is the session's. A homoclinic or
heteroclinic start takes `unstable_dimension`, `stable_dimension` and `left_equilibrium` (and `right_equilibrium`), the
session's own when omitted. Before a run, a model whose AUTO equations use `t` (directly or through a function or a
fixed variable; auxiliary quantities do not count) is refused, with each such equation's file and line, and the
start of a steady-state run whose residual is above `state_tolerance` is refined by Newton's method (a WARN naming
the equation with the largest residual) or refused as not a steady state. The AUTO window's Run key asks XPPAUT's
menus and ends in the same function. It is the command the error of an exact Jacobian's failure offers to repeat
(`error`'s `action`, above), from the end point's label.

**The file ask's mode.** `mode` says whether the command opens the file
(`read`: Import settings, Load diagram, the browser's Load, Import, ...) or saves
one (`write`: Save session, Save diagram, PDF, SVG, ...), so a client
can show an open or a save dialog. The core decides by the selector's title
(`xpp_files_ask_mode` in core/xpp_files.cpp: a title starting with Load,
Read, Import, Open, Select or Library reads, anything else writes;
tests/test_files.cpp lists every title).

## Files

XPP reads and writes in its working directory, the model's folder, and its
files refer to each other by relative name. A client that shows the
browser's own file dialogs (web2, docs/ui-v2.md section 4) copies what the
user picks into that folder and answers the `file` ask with the base name;
for a save it answers with a name, lets the core write, then fetches the
file. core/xpp_files.cpp does the work for both ways in:

- **Browser mode (HTTP, core/xpp_http.cpp)**, token-protected like `/cmd`
  (`?t=TOKEN`, else 403):
  - At most 256 connections at once (each answered on a thread of its
    own; a slow or silent one counts until its thread ends) and 32
    uploads at once (W161, #213): room for 32 tabs' parallel requests,
    and the page uploads its files one at a time. One over either limit
    gets `503 Service Unavailable` (`connection limit reached` or
    `upload limit reached`) and is closed with its body never read; a
    connection over the limit is read for its head alone, on the accept
    thread, for at most a second. The token is checked first, before
    either limit. An upload's body streams on its own; taking a name and
    committing the file are one at a time. `Expect: 100-continue` is
    answered once an upload or command is admitted.
    The whole request head has five seconds from accept, including silent
    preconnects; each receive uses only the time left, so trickling bytes
    cannot renew it (W164, #216). Five seconds gives a loopback browser
    ample scheduling slack. Bodies retain a 30-second timeout per receive
    to allow large uploads and slow disk writes; no minimum rate is imposed.
    Every send has a ten-second timeout. Event streams have no lifetime
    limit, but a blocked write that times out drops the stream, including
    replay, core events and watchdog heartbeats. Event writes hold the
    server lock, so an unresponsive stream can delay the core until that
    send times out, rather than indefinitely.
  - `GET /files` lists the folder: `{"files":[{"name","size","mtime","sha256"}...]}`,
    plain files only (no folders, links or hidden files), sorted by name,
    `mtime` in seconds since 1970, `sha256` in hex.
  - `GET /files/NAME` sends the file (`application/octet-stream`).
  - `PUT /files/NAME` stores the request body as NAME and answers
    `{"name","size","sha256"}`. It needs a `Content-Length` (411 without
    one, chunked bodies included); over 64 MB is refused (413) before a
    byte of the body is read. The body streams into a hidden temporary file
    in the folder, renamed to NAME only once complete: an upload that is
    cut short (400), over the cap or refused leaves nothing, and a file it
    replaces stays as it was until then.
- **`--server`**: the `file` command above, with the same rules.

Nothing lands in the folder while a computation runs (W134): a file a run
reads (a table, a `.par` it loads next) is never replaced under it,
whichever way the copy came. The `file` command's `put` is a data command,
refused during a computation like any other ("Commands during a
command": `message` `error` "Not while a computation runs: file was
refused" after it, then `state` and `idle`, and nothing written). `PUT
/files/NAME` cannot wait in the queue as a line does, so during a
computation (a stopping one too) it is refused with 409 and the same
words, before a byte of its body is read; the core logs the refusal as a
WARN. Both go through core/xpp_files.cpp's one decision, made again
atomically with the rename (`xpp::job::OutsideComputation`): an upload
whose body was still arriving when a computation began is refused at the
end too, and a computation that begins while an upload is being renamed
into place waits for it. Outside a computation an upload goes through at
once, even while a command waits in a prompt (a `file` ask is answered
with the name a client has just uploaded).

A NAME is percent-decoded, then must be a base name: no `/` or `\`, no
`..`, no leading dot (hidden files, `.` and `..`), no leading space and no
trailing dot or space, no control characters, none of `: * ? " < > |`
(which also refuses drive letters), not a Windows device name (`CON`,
`NUL`, `COM1`, ...), at most 255 bytes. Anything else is refused (400, or
`ok` 0). A name that is a symbolic link, a folder or anything but a plain
file is refused too (403). On Windows the open does not follow the final
component: its own handle is checked for reparse points (including
junctions), directories and non-disk files before it is read; exclusive
temporary-file creation uses the same check (W174,
[#226](https://github.com/MuhammadMoustafa/xppautX/issues/226)). Replacing
the name after that open cannot redirect the handle. This does not protect
against hard links, changes to a regular file's contents, or replacement
of an ancestor folder by someone with filesystem write access. The server
listens on 127.0.0.1 only.

Opening or inspecting a model also confines the files it names (includes,
imports, file tables, the animation file, `output` and `logfile`) to the
main model's folder. Relative plain subfolders are allowed; links,
junctions and non-regular files are refused. Path components use the same
name validation, with hidden files permitted.

A path that leaves the folder (`..`, or absolute) is the user's question
(W286, [#346](https://github.com/MuhammadMoustafa/xppautX/issues/346)),
asked while the model loads, before anything is applied: an `ask` of kind
`choice` with `title` "Path outside the model's folder", a `question` that
names the model file, the model's line (and the line as written) and the
path with the text it resolves to, `choices` `["Allow always","Allow once","Refuse"]`
and `keys` `aor`. `a` keeps the pair of the model file's absolute path and the
resolved path in the preferences (`outside_paths` of `preferences.setx`, at
most 24, newest first, the oldest dropped), so that model and that exact
path are not asked again; a changed path or another model asks again. `o`
allows it for this load, including the conversion and native load of a `.ode`.
Approvals for a `.ode` are kept under its generated `.odex` path, so **Allow
always** also applies when that native file is opened directly (W297,
[#355](https://github.com/MuhammadMoustafa/xppautX/issues/355)). `r`, a cancel, an unknown key or no answer refuses:
the load fails with the model's file and line and nothing is applied.
The suggested answer is **Allow once**; the answering policy is in
[Question answering](#question-answering). A link on the way below the folder's common
ancestor with the path, or a name that is not a plain one, is refused with no
question. The path asked about is the normalised one (`.` and `..` resolved in
the text, never through a link), and that same text is what is opened,
without following a link at its end. Log streams also check the opened
file before truncating it: a final-name link swapped in after approval
fails with the log option's file and source line, leaving the target untouched.
Model files share the archive limits (512 MiB, 4096 files), and file and
formula tables share a 1,000,000-point session budget before allocation or
evaluation. The ancestor-mutation and hard-link limitations above also
apply here (W175, [#227](https://github.com/MuhammadMoustafa/xppautX/issues/227)).

### The animation as data

A client that asked with `{"cmd":"events.subscribe","events":["ani"]}` gets each frame
of the animation (the animation surface) as data, in the animation's own coordinates,
and draws it at any size itself (web2, docs/ui-v2.md T13).
core/aniparse.cpp evaluates a frame in the `.anix` file's coordinates (its
`bounds` box, [0,1] x [0,1] unless the file says otherwise, y up) and
hands each primitive to core/ani_data.cpp, which sends it in unit
coordinates:

    u = (x - xlo) / (xhi - xlo)      v = (y - ylo) / (yhi - ylo)

so (0,0) is the box's bottom left and (1,1) its top right. They are not
clamped: where the `.anix` puts something outside its box the value lies
outside [0,1]. XPP's own window put a primitive at `u*w`, `h - v*h`.

```
{"ev":"ani","op":"frame","pos":0,"rows":601,"t":0,"speed":5,"skip":1,
 "dim":[-0.6,-0.1,0.4,0.6],"w":280,"h":350,
 "prims":[["text",0.05,0.928571,"lecar  ","blue",3,0],["line",0,0.142857,1,0.142857,"foreground",1,"solid"],
          ["circle",0.456,0.185714,0.03,0.0428571,"purple",0,0,"solid"],["dot",0.456,0.185714,2,"red"],...]}
```

| field | meaning |
|---|---|
| `pos`, `rows` | the stored row the frame shows (0-based), of how many |
| `t` | the frame's time (9 digits; `null` when not finite) |
| `speed`, `skip` | ms between two frames of Go, rows per step |
| `dim` | the `bounds` box: `xlo`, `ylo`, `xhi`, `yhi`. A client that keeps its aspect `(xhi-xlo)/(yhi-ylo)` has equal units along x and y |
| `w`, `h` | the core's pixel size of the animation window: what line widths, dots and text sizes are relative to |
| `prims` | the primitives in drawing order, each an array (below); unit coordinates have 6 significant digits, `null` where the `.anix` evaluated to NaN |

| primitive | arguments |
|---|---|
| `line` | `u1`, `v1`, `u2`, `v2`, colour, width, dash |
| `rect` | `u1`, `v1`, `u2`, `v2` (two opposite corners, either order), colour, width, fill (0/1), dash |
| `circle` | `u`, `v`, `ru`, `rv`, colour, width, fill, dash: the centre, and the radius over the box's width (`ru`) and over its height (`rv`); XPP draws it with the mean of the two in pixels, `(ru*w + rv*h)/2` |
| `ellipse` | `u`, `v`, `ru`, `rv`, colour, width, fill, dash: the centre and the two radii, as `circle`'s |
| `dot` | `u`, `v`, `r`, colour: a filled circle of `r` pixels (a comet's, with a negative thickness) |
| `text` | `u`, `v`, string, colour, size (0..4), font (0 roman, 1 symbol: Greek letters): from the baseline's left end |

Colours are the values described under "Colours", or a value on the session's
colour scale computed by a formula. Widths are in pixels (0 is a thin line
of one pixel). Dash is `solid`, `dashed`, `dotted` or `dash_dot`, with the
geometry sent in `hello.line_styles`. Each native text carries its own
colour, size and font. The `.ani` importer writes the text style that
XPPAUT's preceding commands would have supplied.

A frame goes out when the core draws it: after `step`, `seek`, `reset`, a
`file` that shows the first frame, `grab` and its `mouse`, Fly's frames
during an integration, and Go's. Frames that come faster than 25 a second
(Go, Fly) are thinned: the latest one goes when 40 ms have passed since the
last one sent, and the last frame drawn always goes, at the latest at the
end of the command, before its `state` and `done`. After `events.subscribe` with `ani`
the last frame drawn (if any) goes at the end of that command. The comets'
trails are the core's, so a thinned frame loses nothing. A new `.anix` file
forgets the frame of the old one.

## Not yet implemented

docs/front-end-gaps.md lists what the X11 front end did that this
protocol does not.

## Removed in protocol 2

Protocol 1 also drove the classic page (`web/`, removed with it, docs/ui-v2.md
T18), which replayed the core's pixel drawing:

- the `draw` event (`win`, `ops`: `clear`, `color`, `lw`, `dash`, `line`,
  `poly`, `point`, `bead`, `rect`/`frect`, `circle`/`fcircle`,
  `ellipse`/`fellipse`, `cursor`, `text`, `rtext`, `stext`, `font`) for
  every window, the AUTO stability circle (102) and info strip (103)
  included, and `/events?...&draw=0` to leave them out;
- the `palette` event (the 256 colours the ops referred to);
- the `size` command (a canvas size in pixels for open plot windows, 101 and
  104): the core keeps its default window sizes;
- `hello`'s `char` (the font cell the ops laid text out on).

Everything the page shows comes from the data events: `series`, `plots`,
`nullclines`, `dfield`, `marks`, `diagram`, `autoinfo`, `autosettings`, `ani` `frame`,
`aplot`, `erase`, `redraw`. The `pixels` ask stays: web2 answers it from
those.

### Opening foreign models (W154, #206)

Every open of `.ode`, including startup in all modes, saves and opens
its `.odex`. Names the converter must change use the existing `string`
ask: `value` is the suggestion, Cancel ends the open without writing and
keeps the previous model. If the `.odex` already has different text, the
existing `choice` question **File Exists! Overwrite?** has `keys` `yn`
and choices **Replace** (`y`) and **Open existing .odex** (`n`); Cancel
(`ok:false`) ends the open. Equal text opens unchanged. An unwritable
folder fails at the `.odex` path. No new ask kind is introduced.

The conversion confirmation uses `message.bottom`, as an imported `.set`
does. A bottom message can also carry `help:{chapter,anchor}`; the page
shows its manual link. Conversion supplies `02-ode-files`, `odex`.
Sessions and recordings carrying a `.ode` model fail with a shown error.

### On-demand update check (W13c, #126)

No protocol event or command changes: the page reads the program version
from the first line of hello.about (the text of its first line, `xppautX <version>`). A git-describe suffix
compares its base release tag. Only the page requests GitHub's latest-release
API. The window binds __xppOpenRelease(url); the core bounds and validates
the fixed https://github.com/MuhammadMoustafa/xppautX/releases/tag/ prefix and
a single nonempty tag with a safe alphabet (no further slash) before using the existing system browser opener.
It rejects invalid URLs or an opener failure. No asset is downloaded.
The page reads only tag_name and html_url: the answer is capped at 1 MiB,
the tag at 64 characters, and the URL at 256. Tags must be vX.Y.Z with
safe integer components; the URL must be this repository's tag page for
that tag. API text is rendered as text, never HTML. The native binding also
caps its JSON request and rejects traversal, quoting, escapes, query strings
and fragments before launching the existing browser opener. Its rejection
is an xpp::Error placed at Check for updates:1 and shown by the dialog.

W176 ([#228](https://github.com/MuhammadMoustafa/xppautX/issues/228)):
the native binding rejects download paths and nested tag paths. Unix opener
completion is checked synchronously, including a missing or failing opener.
Closing a pending check aborts its fetch, invalidates its result and permits a fresh check;
an old request cannot clear the new request's pending state. Page errors use
the existing error place/source representation: local version errors name
hello.about line 1; API syntax/field errors retain their actual line and source
when known; transport failures name the API without inventing a line.

### About links (W224, #278)

hello.about's links open in the system browser. In the desktop window an
anchor would navigate the web view away, so the window binds
`__xppOpenAboutLink(url)` (the page calls it from the link's click and
cancels the navigation); in a browser the anchor opens a new tab and the
binding does not exist. The core opens a URL only when it equals, byte for
byte, the target of one of the About lines' links (xpp_about.h): any other
request, whether another scheme, a file path, a lookalike or an addition
to a listed address, is refused with an xpp::Error placed at About:1.
The native About box (Win32 task dialog, GTK dialog) hands its link clicks to the same check.

### Live integration state (W200)

`{"ev":"liveState","time":0.125,"now":[1.25,2.5],"rates":[1,2]}` reports solver doubles
for every ODE/Markov state in `state.ics` order, after an accepted output step
and before transient/Poincare storage filtering. It does not change initial
conditions or the resume state. The JSON front end paces updates at its existing
50 ms input interval; it reports the latest complete vector, never individual
plotted columns. This is display pacing, not a solver step or accuracy setting.
`rates` is `state.rates` computed from the rows stored so far (next section), so it
can lag `now` by the rows the run has not stored yet (a transient is not stored).
The final `state` still supplies the exact end/stop values. No trajectory
subscription is needed. `XPP_NO_THROTTLE` exposes every accepted output step for
condition-based protocol checks. An output step follows configured `store_every`;
adaptive solver internal substeps are not display steps.

### Recording startup and all-state inspection (W201, W202)

`play open` starts a newly opened recording automatically. Set `autoplay:false`
to open it paused for inspection. `play from` with `step:0, play:false` remains
a paused restart. The `player` feature in `hello.features`, requested by `events.subscribe`,
resends the current recording's complete step list; a page connecting after the
file opened receives its metadata along with the current `state.player`.

`state.rates` and `liveState.rates` contain a number or null for every state in
`state.ics` order. `hello.state_inspection.tail_intervals` gives the window size:
ten most recent stored intervals. Rates are max |Δstate/Δt| from float32 stored
samples; they are independent of plot columns and use stored times, including
backward time. `rates` is null for a state when fewer than two rows are stored (a model
not yet run: all null), a sample is not finite, two stored times are equal,
or the time direction changes inside the window (forward and backward runs
mixed in one trajectory). Rates can be zero because float32 samples do not
resolve small motion. The unchanged-digits stopping test still uses full solver
doubles; these sampled rates do not certify equilibrium or stability.

For desktop/browser startup, initial autoplay begins after the page subscribes
to the player metadata, so the initial run is visible. Server/silent playback
starts directly. Pause cancels a pending initial start.

Writes into a missing folder ask **Create the folder <folder>?**. A model path outside its folder first asks for outside-path approval, then asks separately to create the folder. No answer refuses; `--yes` accepts and prints the answer, and `--check` asks nothing and creates nothing. Existing folders are not asked again. Links on the way are refused and new folders are private (0700 on POSIX). Permission denied names the folder and the requesting source or command, says it needs administrator rights and to choose another place; the program never elevates itself. The desktop save dialog retries in the writable model folder, or Documents, after this error. (W303, [#361](https://github.com/MuhammadMoustafa/xppautX/issues/361)).

### Reduced ODE Hopf curves (W363, #425)

The `diagram` and `autoinfo` curve field `f2` is 3 for an ODE Hopf curve
with nonnegative squared frequency and 13 for its neutral-saddle extension
with negative squared frequency. These points retain their Hopf start type
in `itp` for restarting the same formulation; this does not label the
negative-frequency segment as a Hopf bifurcation. The readout calls code 13
"neutral saddle curve". Run from a grabbed neutral-saddle label extends it.

An ODE Hopf restart solution in a `.snapx` holds `[u(n), v(n), kappa, p2]`
(`2n+2` states). The solution member rejects older `3n+2` Hopf layouts with
the member's file name, header line and source text, before applying the load.
For positive kappa, period is `2*pi/sqrt(kappa)`; at and beyond zero kappa,
`per` retains the last positive-kappa period, as in AUTO-07p, and is not a
period of the neutral saddle.

### AUTO start validation (W495, #559)

All front ends use the same checks before continuation. A refused start emits
one placed error and adds no diagram point. An initial steady-state start
checks the largest absolute RHS residual against `state_tolerance` at that equation's
model line; labelled restarts and orbit starts skip this test. The model
parameters must lie within `parameter_min`/`parameter_max` (both axes for a two-parameter run),
the primary parameter on a bound must have `step_size` directed inward, and two axes
must differ. The start's solver norm must be within `norm_min`/`norm_max` (defaults
0 and 1e100). Incompatible restart kinds name the label and needed kinds.

### Stop diagnostics completed by W498 (#562)

A map with no crossing sends a bottom message naming `poincare_map`,
`poincare_variable`, `poincare_plane` and the actual time interval, with a
suggestion to change the section or integrate longer. A Monte Carlo search
sends one summary of attempted and converged guesses, distinct equilibria,
requested guesses and Newton settings. Manifold failures are placed errors,
shown once by the command; an equilibrium already found remains available.
Periodic AUTO's no-convergence text includes a tiny reduced-system pivot
and its threshold, as equilibrium AUTO does. A short solution in a session
is refused before load, naming its label, file and line.


### Registry navigation (W517, #581)

The page's menu tree, command palette, argument forms and shortcut labels read
`hello.commands` registry rows (`arguments` is present even when empty).
`menu_path` is the slash-separated page menu path; `default_keys` is the
shortcut list (one key each). Keymap
binding and pin ids are registry command names such as `run.initial`, not the
old main-window table's ids. `keymap.get/set/reset` own the effective keys.
Each row's keys are its `default_keys` unless the user bound it; "Reset to
defaults" in the editor clears custom bindings and retains pins. Cmd on macOS is shown by the page as Ctrl in keymap files.

For a row with one `file` argument (or a required `path`), `file_picker: true`
explicitly requests the existing file dialog before execution. All other
arguments validate before the dialog; Cancel returns `{"cancelled":true}`.
`file_mode` and `file_wild` in the row describe that picker. A picked write path
still requires the native binding's single-use grant; an HTTP reply cannot
supply it. Direct file arguments retain their basename/path restrictions.

There is no `menu` ask. Remaining core questions with a keyed list use `choice`
and `choices`; navigation happens on the page. A command form submits one
registry command with typed arguments, omitting optional fields left unset.
File arguments remain base names in the model folder. A `path` is still refused
from HTTP and recordings; the core's file ask authorizes the native picker.

The page's Windows submenu sends `window.show/select/rename/hide/destroy` with
`window` as the exact name. Annotations retain their ids in `marks`; the list,
plot selection and dragging share `annotation.edit/delete/move`, with
`annotation_id` (not the request correlation `id`) and plot coordinates.
