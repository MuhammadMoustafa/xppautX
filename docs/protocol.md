# xppautX protocol

`xppautX --server file.ode [xppaut options]` loads the model the way `xppaut`
does and then talks line-delimited JSON: one object per line, UTF-8, on
stdin (commands, `"cmd"`) and stdout (events, `"ev"`). stderr carries the
core's own log output. The implementation is `core/ui_json.cpp` and the
`core/json_*.cpp` files it is split into (`core/ui_json_internal.h`);
`tools/servercheck.py` is a working client, the browser page (`web2/`,
served by `xppautX` itself) a full one. This is protocol 2: see "Removed in
protocol 2" at the end for what protocol 1 had besides.

The core is single-threaded. A command runs to completion, then the server
sends `state` and `idle`. While a command runs the server can stop and
**ask** the client something (a menu, a prompt, a mouse click); it waits for
the matching `answer`, answers `state`, `browser` with `from` and `quit` at
once, and keeps any other command for after the one that asked. See
"Commands during a command" for what reaches a running computation, and
"Action kinds" for which commands those are.

## Startup

1. `hello`: protocol version (2), `about` (Help > About's text: version, commit, compiler, credit, the author's contact details and the issue tracker; core/xpp_about.cpp, the same string the desktop window's own About box shows), `quit` (File > Quit's question as the core asks it, W110: `question`, `recording` (the question while a recording is in progress), `choices` (`["Save session","Don't save"]`) and `keys` (`sd`), for a client that asks it itself while a command runs: `quit` with `save` below), window title, the three main menus
   (`main`, `file`, `num` with `_keys`, `_hints` and `_kinds`), the
   windows' key layers (`windows`) and the other commands' kinds
   (`commands`): see "Action kinds".
2. `window` `create` for window 1, the main plot.
3. `state`, then `idle`.

A client that draws sends `data` next (see "The plot as data").

### A model that does not load

When the model does not load (a line the reader does not understand, a
formula that does not compile, a name given twice), the server sends one
`error` event instead of `hello` and exits with status 1 (W63c). The log
(stderr, the page's `log` event) says what it always said; the event is
the same as values:

    {"ev":"error","file":"bad.ode","line":3,"col":0,"cause":"Premature end of expression\n-X+A*\n    ^\nERROR compiling X'","source":"x'=-x+a*"}

- `file`: the file the problem is in, as the server was given it, or the
  file the model includes (`#include`) when the problem is there.
- `line`, `col`: where, both from 1; 0 when not known. An .ode problem
  has its line (a statement continued with `\` is at the line it starts
  at) and column 0; an .odex one has both. A problem of the whole model
  (no equations, too many boundary conditions) has line 0.
- `cause`: what is wrong, the error and warning lines logged about that
  line, joined with `\n` (a formula and a caret under where it stops
  making sense are two of them).
- `source`: line `line` of `file` as written ("" when `line` is 0).

In browser and window mode the server keeps serving after the exit, so
the page gets the event (and `exit` after it) whenever it connects. The
core's type for it is `xpp::Diagnostic` (core/diagnostic.h), which
`xpp::load_model` (core/xpp_batch.h) returns for a load that fails.

## Commands (client to server)

| cmd | fields | meaning |
|---|---|---|
| `key` | `key`; `win`, `row` | A hotkey, exactly as typed in xppaut: one character, or `Escape`, `Enter`, `Tab`, `Backspace`, `Delete`, `Home`, `End`, `ArrowLeft/Right/Up/Down`, `PageUp`, `PageDown` (DOM `KeyboardEvent.key` names; X keysym names also work). Menu clicks are sent as the item's key. One letter is one command, and every key is in a menu (core/menus.cpp, the only place a key is defined; `tools/keycheck.py`). With `win` (`auto`, `browser`, `ani`, `aplot` or `equilibrium`) the key is one of that window's own layer ("Window keys" below) instead of the main window's; the browser's takes `row`, the selected row. A page's button sends its key's command: the buttons that have no key (below) are the only other commands. `button` (optional, any command's key) names the control the key came from (web2: Integrate, and every window button by its `hello.windows` id); only a recording reads it ("Recordings"). |
| `answer` | `id`, `ok` (0/1), plus the kind's fields | Reply to an `ask`. Omitting `ok` means ok. Omitting `id` answers whichever `ask` is currently pending (see "Scripts": a script cannot know the id an `ask` is handed at run time, and this equally lets a plain client skip tracking it). |
| `set` | `kind` (`par`, `ic`, `bc`, `delay`, `num`), `name` or `index`, `value` or `text` | Change a value (no redraw or run: W69 dropped the `rerun` flag this command used to take). A setting ("Action kinds"): sent during a computation it applies when that ends, never to the run in progress ("Commands during a command"). `name` is matched without regard to case, in full: a name has no length limit (W76) and every event carries it unshortened. `text` is what the X11 box takes: a number or `%formula` for `par` and `ic`, an expression for `bc` and `delay`. BCs and delays go by `index` (BC names all read `0=`). `num` sets a main numerics field by its key (`name`: `total`, `dt`, `method`, ...; "The numerics as data"). A formula that does not evaluate gives `message` `error`, a numerics value refused one naming the field (`Numerics: Dt must be a number other than 0`); so does what the command cannot take, nothing set (W116): a `kind` it does not have (`set takes kind par, ic, delay, bc or num, not "parm"`), a name the model does not have (`set: the model has no par nosuch`), an `index` outside the list, no `value` that is a number and no `text` (`set par iapp: its value is not a number (or its text missing)`). Several values in one command: `values` [{`kind`, `name` or `index`, `value` or `text`}...]. web2 sends every edit (a value field, a slider, Reset, a numerics field) at once, busy or idle (W106). |
| `default` | `kind` (`par` or `ic`) | The Default button: values from the ODE file (`hello.defaults`); no run (the `rerun` flag went with `set`'s, W69). A setting, as `set` is. Another `kind` is a `message` `error` and nothing changes. |
| `slide` | `name`, `value` | A parameter slider moved: set the parameter or variable, no run (the `rerun` flag went with `set`'s, W69; web2 sends a slider's values as `set`). A setting, as `set` is. A name the model does not have, or a `value` that is not a number, is a `message` `error` and nothing changes. |
| `userbut` | `index` | An `@ button` of the ODE file (`hello.userbuttons`). |
| `plotvars` | `how` (0 x vs t, 1 phase plane, 2 array plot), `names` | The IC box's xvst/pp/arry buttons for the checked variables. |
| `browser` | `from`, `count`, `col`, `ncol` | The data browser block the client shows (answered at once with `browser`, even during a prompt); `count` 0 stops the updates. |
| `browser` | `op` (`write`, `load`, `postprocess`); for `write` `what`, `format`, `name`, `replace`; for `load` `format`, `name` | Save data and Load with their choices given, each skipping its question (see "Saving data" below); the same commands by the keys `w` and `l` of the browser window ask them all. `postprocess` runs the model's `@ postprocess` (a histogram, a Fourier transform, ... of the data, core/histogram.cpp) and shows the result in the browser, as `-silent` does after its run. The other buttons are the window's keys ("Window keys"). |
| `values` | `op` (`write`, `read`), `kind` (`par`, `ic`), `name` | The values panel's Save and Load of XPP's own file for a section (core/lunch-new.cpp `io_parameter_file`/`io_ic_file`, W66): `write` is Save, `read` is Load; `name` given skips the file ask (as `browser`'s `write` does), omitted or empty asks for one (`ask` kind `file`, like Save/Load data or File/Write set). `write` fails (`message` `error`) for a `kind` other than `par` or `ic`. `op` `internset` (no `kind`) is File/Get par set (keys `f`, `g`) for the internal set `index` (0-based) or `name`, with no ask: its values and options, its plot settings on the current window; one the model does not have is a `message` `error`. `op` `query` (no `kind`) writes `name` with the model's internal sets (name, whether `-silent` runs it, what it sets), parameters (their values in the file) and initial conditions, each asked for by `sets`, `pars`, `ics` (1), under its `#` heading: `-silent`'s `-qsets`, `-qpars`, `-qics`. `read` and `internset` are settings (they set values), `write` and `query` data (they write a file). |
| `dfield` | `op` `write`, `name` | Write the direction field the current plot window shows (Dir.field, key `d`) to `name`, one arrow a line (x, y and the arrow's end), its lengths in PostScript's frame whatever the window's size: `-silent`'s `-dfdraw 4`/`5` file, dirfields.dat. A window that shows none is a `message` `error`. |
| `equilibrium` | `op` `write`, `name`, `shoot` | Find the equilibrium Newton reaches from the initial conditions and write it to `name`, one variable a line (its value, then its eigenvalue's real and imaginary parts); nothing when Newton does not converge. `shoot` 1 also integrates a saddle's invariant manifolds into `UMk.dat`/`SMk.dat`: `-silent`'s `-equil 0`/`1`, equil.dat. |
| `equations` | | Send `equations`. |
| `data` | `events` (names from `hello.features`), `enc` | The data events the client wants from now on (`[]` stops them); each is sent at the end of this command. `series`: the plot windows' curves as numbers, `plots`: the plot windows themselves, `nullclines` and `dfield`: what the phase planes show besides their curves, `marks`: equilibria, text, arrows, markers and frozen curves on the plots (all in "The plot as data", below); `ani`: the animation's frames ("The animation as data"); `autoinfo`: AUTO's info strip and stability circle ("The AUTO diagram as data"); `autosettings`: AUTO's Numerics, parameters, axes and Mark values ("AUTO's settings as data"); `numerics`: the main numerics ("The numerics as data"). `enc` `"f32"` sends these events' value arrays as base64 of little-endian float32 instead of JSON numbers (an `ani` frame is always JSON). |
| `action` | `index` | Run the action of comment `index` of `source.comments`. |
| `click` | `win` | The user selected plot window `win`. |
| `display` | `win`; `x`, `y` (`[low, high]` or `null`), `runs` (bool) | What the page displays of plot window `win` (W65, "Display state" below): the zoom shown on each axis given (`null`: the window's own) and whether its earlier runs are drawn. A view (see "Action kinds"): sent during a computation it runs after it (the page holds its changes for the idle anyway, to send only the last). A range whose low is not below its high, or a window that does not exist, is a `message` `error` and nothing changes. The values come back in `plots`. |
| `redraw` | | Redraw the active plot window, and the AUTO diagram when AUTO is open (for a client that reconnects). |
| `state` | | Send `state` now. |
| `auto` | `op`: `grab` (`label`, or `type`+`index`), `display` (`view`, `x`, `y`, `show`), `view` (`new`, `close` or `active`), `close`, `point` (`x`, `y`, or `xd`, `yd`), `set` (`numerics`, `pars`, `axes`, `marks`) | What the AUTO window's keys ("Window keys") do not say. `set` writes AUTO's settings without the forms (see "AUTO's settings as data"); `point` is a click on the diagram at pixel `x`, `y` of window 101 or at `xd`, `yd` in the diagram's quantities (shows its coordinates, and in a two-parameter plot stores them for AUTO's File/sElect 2par pt (`e`), which sets the two parameters to them); a run (AUTO's key `r`) AUTO cannot compute (the Numerics form's Ncol above 7 or Ntst 0, a singular Newton step) ends with a `message` `error` beginning `AUTO stopped:` and the diagram so far saved, as a cancel leaves it; the session goes on (W63a); `display` sets what the page displays of the diagram (below): `x`, `y` a view's zoom, `show` whether the branches hidden by Clear are drawn; `view` adds, closes or activates a view of the diagram ("Views of the diagram" below); `grab` grabs that stored point directly, with no ask ("Grab by label" below), a label or a type and index being required (the interactive grab is the key `g`); `close` destroys window 101, File/Auto opens it again. |
| `session` | `op` (`save`, `load`), `name`, `data` (save) | Save or open a session file, `name.snapx` (`.snapx` added unless `name` ends so; "Session files" below): File/saVe session and File/opeN session, keys `v` and `n` of the File menu. Without `name`, asks for one (`ask` kind `file`, wildcard `*.snapx`). `save`'s `data`: `true` puts the data table in, `false` leaves it out; without it the table goes in unless it is above 50 MB, when a `choice` ask (keys `l` leave it out, `s` save it) decides. `load` of a session file is `open` of it (below: its saved model, then the session). `state.session` (below) names the file the current session was last saved to or opened from. |
| `open` | `file` | Load another model in place of this one (File/open Model, key `m` of the File menu; the desktop window's File > Open model sends it with the file picked). Without `file`, asks for one (`ask` kind `file`, wildcard `*.ode* *.autox *.snapx`). A file that is not there fails at once (`message` `error`, nothing asked); so does a session file (`.snapx`) or an AUTO file (`.autox`) that cannot be read or has no model in it ("Session files" below). Then asks `choice` with `keys` `sd` (`Open NAME? This model's data and diagram go. Save its session first?`, Save session or Don't save, the question every way of leaving a session asks, W59d): `s` saves the session first (as `session` `save` with no `name`: a `file` ask; cancelling it keeps the model), `d` does not, a cancel keeps the model. A session file or an AUTO file loads the model saved in it, from its saved files and never the disk's (however the `.ode` there has changed, or when there is none), then restores the session or loads the diagram; when the model open is that one (the same files, byte for byte), an AUTO file's diagram goes into it with nothing asked, the data kept and no model loaded, while a session file still asks (it replaces this session's values, data and diagram) and then loads the model again with its session. The model is loaded from its own folder (a saved model from the folder of its file), which becomes the working directory (the page's files, "Files" below: outputs go there, and nothing is written beside a session or AUTO file), with a command line of the file alone (and `-anifile` with its saved animation when it was loaded with one). `hello`'s `title` names a saved model's file and the one it is saved in, `lecar.ode (saved in lecar.autox)`. A file that is not a model's text (a zip, another binary file) is refused, as a model that does not load (below), its bytes never shown. Loaded: every window but the main one is destroyed (`window` `destroy`), and a new `hello` follows, then the main window, `state`, and the rest of a first start (the ICs the file sets, `-anifile`'s animation, `@ runnow`); a client handles it as it does a reconnection's (it sends `data` again). A model that does not load (a parse error, a bad option) sends a `message` `error` naming both files, the log says why, and nothing else changes: the model before, its values, windows, data and folder are as they were. |
| `reload` | | Read the model's file again with the command line and in the folder it was loaded with (File/rEload, key `e` of the File menu; the window's File > Reload), as `open` does, asking first as it does (W59d: `choice` `sd`, `Reload NAME? Its values are kept by name; this model's data and diagram go. Save its session first?`): the parameters, initial data (a delay equation's history text too) and numerics keep the session's values by name (the Poincare section's variable by its name), a name the file no longer has is dropped, a new one comes with the file's value; `hello`'s `defaults` are the file's. A file that does not load changes nothing, as for `open`. |
| `record` | `op`: `start`, `stop` (`name`), `note` (`text`) | Record the session's steps into a `.recx` file ("Recordings" below): File/recorD, key `d` of the File menu, starts a recording or stops it. `start` begins one, with the session as it is now (its `@snapshot`, W59d; an error when one runs, or when the model was not read from files); `stop` writes `name.recx` (`.recx` added unless `name` ends so; without `name`, asks for one as the other File saves do: `ask` kind `file`, wildcard `*.recx`, the model's name offered; a cancel or a refused overwrite keeps recording) and ends it; `note` sets the note for the next step (replacing one set before; an error when not recording). `start` and `stop` are data, `note` control ("Action kinds"). `state.recording` says one runs. |
| `play` | `op`: `open` (`file`), `start`, `pause`, `step`, `speed` (`speed`), `from` (`step`, `play`), `note` (`step`, `text`), `close` | Play a recording ("Playing a recording" below). `open` loads the `.recx` `file` (without one, asks for it: `ask` kind `file`, wildcard `*.recx`; then, as File > Open model does, whether to save this model's session first) and its model, paused at step 0; File/plaY recording (key `y` of the File menu) and Open model of a `.recx` do the same. `start` plays (at the end, nothing), `pause` pauses (a wait in progress keeps what is left of it), `step` plays the next step, or the rest of the one running, then pauses; `speed` divides every pace by `speed` (0.25 to 8; the page offers 0.5, 1, 2, 4); these four act at once, even during a step or a computation, and have no `idle` of their own then. `from` loads the model again and runs steps 0 to `step` - 1 with no pace (their `press` events 0 ms), then pauses there (plays on with `play` 1); `step` 0 is Restart. `note` writes `text` as step `step`'s note into the `.recx` (its fingerprint kept: "Recordings") and sends `player` again. `close` leaves the player; the model stays. `open`, `from` and `note` are data, the rest control. |
| `aplot` | `op`: `scroll` (`dy` pixels), `close` | Dragging the array plot scrolls through time; `close` destroys its window. Its other buttons are the window's keys. |
| `ani` | `op`: `pause`, `fast`, `slow`, `speed` (`ms`), `step` (`n`), `seek` (`pos`), `mouse` (`what` down/move/up, `x`, `y` or `u`, `v`), `close` | What the animation window's keys ("Window keys") do not say: the ones that carry a number, steer a playing Go (`pause`, `fast`, `slow` and `speed` sent while it plays reach its loop) or drag. `speed` sets the delay between two frames of `go` to `ms` (0..1000; `fast` and `slow` change it by 2 within 0..100). `step` moves `n` rows from the core's position (`ani` `pos`), `seek` goes to row `pos`. `mouse` drags a grab point after the grab key, at pixel `x`, `y`, or at `u`, `v` in the animation's unit coordinates (those of the `ani` `frame` event, y up). |
| `abort` | `at` (scripts only) | Stop the running command's computation, at once (see below). No reply of its own: the stopped command ends with `stopped`, `state` and `idle`; outside a command it does nothing. `at` is where a recorded session stopped (the `stopped` event's `at`); only a script's player reads it (see "Scripts"), anywhere else it is an ordinary `abort`. |
| `file` | `op` (`list`, `get`, `put`), `name`, `data` | The model's folder (the working directory) for a client that cannot reach it: `put` writes `data` (base64, at most 64 MB decoded) as `name`, `get` reads `name` back, `list` lists the folder. Answered with a `file` event, then `state` and `idle`. Names are base names only (see "Files" below). |
| `quit` | `ask`, `save` | Exit, at once even during a computation, asking nothing: a script's and a client's quit (`--script`, `-silent`, servercheck). With `ask` `true` (W59d), the user's quit: the desktop page sends it when the window's File > Quit or close box comes while the core is idle (web2 asks itself while a command runs, below). It stops a computation in progress (as `abort`; the command ends with `stopped`, `state` and `idle`), cancels a question open at the time (its command ends), then runs as a command of its own: the `choice` ask "Quit xppautX? Save this session first?" (`keys` `sd`: `s` Save session, `d` Don't save; a cancel keeps the session), as File/Quit (keys `f` `q`) asks. `s` saves the session first, as `session` `save` with no `name` does (a `file` ask, `*.snapx`), and a recording in progress after it, as `record` `stop` with no `name` does (the question then says so); then, as for `d`, `bye` and the exit. A cancel of any of these questions keeps the session. A plain `quit` sent while it asks still exits at once. With `save` `true` (W110), that question answered **Save session** where the client asked it itself: the desktop page asks it while a command runs (the window's close box never stops a computation), worded by `hello`'s `quit`. It stops a computation in progress as `ask` does, then saves as `s` does (the session's `file` ask, then a recording's) and says `bye` and exits; a cancelled save keeps the session (its computation already stopped). The page's **Don't save** is the plain `quit` (in the window: closing it, which sends it). |

## Action kinds

Every action has a kind (W95), defined once in the core and sent in
`hello`, so a client knows what it may still do while a computation runs
(see "Commands during a command"). A kind is one letter:

| kind | letter | what | during a computation |
|---|---|---|---|
| control | `c` | Abort, Quit, an answer | acted on |
| view | `v` | only changes what is shown: a zoom, a window picked, the data subscription, a menu that shows, Help | kept, and run after the computation |
| setting | `s` | a value the next computation uses (W106): a parameter, an initial or boundary condition, a delay, the numerics (`set`, `slide`, `default`, `values` `read`/`internset`, the Parameters key, the Numerics menu's items that ask a value, File/Get par set), AUTO's Parameter, Numerics and Mark values (`auto` `set`, the AUTO window's keys `p`, `n`, `u`) | taken at once, applied when the computation ends: the run in progress keeps the values it started with |
| data | `d` | saves and loads: Save/Load values' files, Write/Read set, every file written, a session, AUTO's diagram files, AUTO's Grab (the curve is still changing) | refused |
| computation | `x` | starts one: Initialconds, Continue, Range, AUTO's Run, Nullclines, Dir.field and Flow, Sing pts, Stochastic, a user button | refused |

- `hello.menus` has `main_kinds`, `file_kinds` and `num_kinds`, one letter
  per item, parallel to `main_keys` etc. (core/menus.cpp). An item that
  opens a pop-up menu has the least restrictive kind among that menu's
  items, so a menu opens when any of its items could run (Nullcline,
  Dir.field, Kinescope and Graphic stuff are views, run once a
  computation ends; stocHast and Averaging data; the Numerics items that
  ask a value settings, their dialog opening once the computation ends); every
  pop-up menu's items have their kinds in core/menus.cpp too (checked when
  it compiles), for the core itself.
- `hello.windows` is the windows' key layers ("Window keys" below), by
  `win`: {`items`, `keys`, `kinds`, `ids`, `hints`}, `ids` the page's name
  for each item (`run`, `grab`, `write`, `go`, ...). A client takes a
  window's keys from here; web2 has no copy of them.
- `hello.commands` is every command but `key` (whose kind is its menu
  item's), [{`cmd`, `op`, `kind`}...]: an entry with `op` is for that op,
  the entry without for the command's other lines (`browser` with `from`
  is a view, its `write` data). It is core/ui_json.cpp's command table,
  the one handle_line dispatches from: every command of the table above
  is in it.

The core says when the running command begins computing: `computing`
(once per command, before its first `progress`; its `idle` ends it). A
client disables data and computation actions from then until that
`idle`, and nothing merely because a command it sent itself is running.
Settings stay enabled throughout: an edit is sent at once, and the core
applies it when it can (below).

## Window keys

The windows other than the main one have a key layer of their own: `{"cmd":"key","win":W,"key":k}`, one letter one command, defined in core/menus.cpp (`menu_auto_window`, `menu_browser_window`, `menu_ani_window`, `menu_aplot_window`, `menu_equilibrium_window`), which a page's buttons send (their keys, kinds and names come in `hello.windows`: "Action kinds"). A key the window's layer does not have is ignored; an unknown `win` is a `message` `error`. Like any command they start after the one before ends: while a command runs they are dropped (the page does not send them).

| `win` | keys |
|---|---|
| `auto` | `p` Parameter, `a` Axes, `n` Numerics, `r` Run, `g` Grab (interactive, "Grab by point"), `u` Usr period, `c` Clear (redraws the axes; the diagram's points stay), `d` reDraw, `f` File |
| `browser` | `f` Find, `g` Get (the selected `row` becomes the initial conditions), `r` Replace, `u` Unreplace, `t` Table, `h` Home (the first row kept is `row`), `e` End (the last), `s` reStore, `a` Add column, `d` Delete column, `l` Load, `w` Write (Save data) |
| `ani` | `f` File, `g` Go, `r` Reset, `s` Skip, `m` Mpeg, `o` On the fly, `a` grAb |
| `aplot` | `d` reDraw, `e` Edit, `f` Fit, `r` Range, `p` Print, `g` GIF |
| `equilibrium` | `i` Import: the last equilibrium becomes the initial conditions |

Use this view, the 3D turn and Use current state have no layer of their own: they are main-window keys, `w` `w` (Window/Window, whose four numbers the page answers), `3` (3d-params, whose Theta and Phi the page answers) and `i` `l` (Initialconds/Last, which runs). The buttons with no key at all are the commands with their own row above: `slide`, `default`, `values`, `userbut`, `action`, `plotvars`, `click`, `redraw` and the parameter-carrying `set`, `auto` (`set`, `point`, `grab` by label, `close`), `browser` (`write`, `load` with their choices, the paging request), `ani` (`step`, `seek`, `speed`, `mouse`, `pause`, `fast`, `slow`, `close`) and `aplot` (`scroll`, `close`). A command the protocol does not have (the removed `view`, `view3d`, `rotate`, `eqimport`) is a `message` `error` "Unknown command X", an `op` of `auto`, `browser`, `ani` or `aplot` it no longer has "Unknown auto op X" and so on.

## Saving data

The browser's `write` (Save data) writes the data table or what the plot
shows in one of the registered data formats (core/data_formats.cpp, one line
per format):

| `format` | Extension | What is written |
|---|---|---|
| `dat` | `.dat` | XPP's own: a row per line, `%.8g` values each followed by a blank, no names (byte for byte what a batch run writes as output.dat) |
| `csv` | `.csv` | a header row of the column names, then a row per point, each stored float in the shortest text that reads back as it; `
` line ends |
| `csv.gz` | `.csv.gz` | that CSV, gzipped (one member, time stamp 0) |
| `npz` | `.npz` | NumPy's `numpy.savez_compressed` format: a zip of `.npy` files, one float64 array per column named after it (`T.npy`, `V.npy`, ...) |

`what` is `table` (the rows First..Last of every column: `T`, then the
browser's `cols`), `output` (those rows of the model's output columns: its
`only` list when it has one, else every column: what `-silent` writes as
output.dat) or `plot` (the current plot window's curves, then its
frozen curves, as one long table `curve,x,y` (and `z` in 3D), one row per
point, the curves numbered from 1; in NPZ one (points, 2 or 3) array per
curve, `curve1`, `curve2`, ...). Graphic stuff > exp(O)rt (keys `g`, `o`)
is the same Save data with `what` `plot`: it asks the format and the file. `name` is the file (a name ending in a
format's extension also chooses that format when `format` is not given).
What is not given is asked, in this order: `what` as a `menu` ask named
`save_what` (keys `t`, `p`), the format as a `menu` ask named
`save_format` (one item per registered format, in the table's order, keys
`d`, `c`, `g`, `n`), the name as a `file` ask whose `wild` is `*` and the
format's extension. An existing file is only replaced after a `choice` ask
(File Exists! Overwrite?), or without it with `replace` 1 (a script cannot
know whether the file is there). An unknown `what` or `format` is an error message.

The browser's `load` asks for `name` (a `file` ask, `wild` `*`) when it is
not given and reads it as `format`, or else as the format of its extension,
or else as `.dat`; the file's columns fill the stored ones in order (a CSV's
header row skipped, a 2-D `.npy` giving one column per column), at most the
storage's rows.

## Commands during a command

Input is read on its own thread, so lines keep arriving while the core
computes. Every command runs as a *job*, numbered by the position of its line
in the input.

- `abort` and `quit` (but a `quit` with `ask`, below) act the moment they arrive: they cancel the running job
  and every job whose line came before theirs, even one still waiting its
  turn, and the computation stops at its next check (every integration step;
  AUTO between continuation points). So an `abort` sent right behind a
  command stops that command, however late the core gets to it, while a
  command sent after the `abort` runs normally. An answer to a prompt sent
  after an `abort` counts as the user's last word: the rest of that command
  is not cancelled. `quit` then exits. A `quit` with `ask` (W59d) cancels
  only a running computation, and waits its turn as an ordinary command:
  it asks, and exits only when the user says so.
- While a *computation* runs (an integration, a range of them, Sing pts, a
  boundary value problem, an AUTO run: what Escape stops; the `computing`
  event says it began), the server judges each line the moment it arrives
  by its kind ("Action kinds"; one list, core/ui_json.cpp `during_run`):
  - acted on at once, at the computation's next check: `abort`, `quit`,
    the stop keys (`key` `Escape`; `/`, which ends a range or a shooting
    for good), and what only reads or steers a view: `state`, `browser`
    with `from`, `ani` `pause`/`fast`/`slow`/`speed`;
  - taken at once, at the computation's next check, and kept for after
    it: a setting (W106: `set`, `slide`, `default`, `values`
    `read`/`internset`, `auto` `set`; "Action kinds"). The computation
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
    questions), every other command of the view kind (`data`, a
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
    `abort`; a script is read one line at a time after each `idle`, so
    every step of it starts from idle.

  Once the computation is stopping (an `abort` or Escape cancelled it), a
  line that arrives is for after it: it is taken as below, so a command
  sent right behind an `abort` runs normally, in its turn.
- Outside a computation, while a job runs (a command in its prompts, the
  animation's Go, a command finishing), `key` (a main-window key: not one
  with a `win`, which waits its turn), `set`, `state`, `browser`
  with `from` and `ani` `pause`/`fast`/`slow`/`speed` are *control* lines
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
  script, where the line after an ask is its answer and anything else
  fails the script.
- `abort` never has an `idle` of its own, so a client can send it at any
  time without upsetting its count of commands and idles.
- A command whose job was cancelled (by `abort`, Escape, `quit`) sends
  `stopped` before its `state` and `idle`: where the computation got to,
  which is what a script needs to replay the interruption (below).

## Scripts

`xppautX --script FILE model.ode [xppaut options]` plays FILE instead of
reading commands from stdin: FILE holds the same line-delimited JSON
commands a `--server` client sends, one per line (blank lines and lines
whose first non-blank character is `#` are ignored). Protocol events go to
stdout exactly as `--server` sends them. The process exits 0 when FILE
runs out, or 1 if a `message` event of `error` kind (or, for a model that
does not load, an `error` event) was sent. A line that
does not fit the dialogue stops the script at once with exit status 1 and
a message on stderr naming the line and the open question: an `answer`
when no question is open, or a command where an answer was due (a prompt
the script did not expect, such as "Draw Strong Sets?" after Sing pts on
some models).

Pacing: a script cannot see the protocol's events going by, so it cannot
itself wait for `idle` or watch for an `ask` the way a real client does.
Instead the player takes FILE's next line only when the core is ready for
it: an ordinary command's line is taken right after the previous command's
`idle` (this includes the very first line, taken after the session's own
opening `redraw`); an `answer` line is taken the moment an `ask` is sent,
since that is the only thing a script's next line can mean. Nothing is
read ahead, so a line already in FILE is never mistaken for the answer to
the wrong `ask`, and every `answer` line can omit `id` (above): a script
cannot know it in advance.

Interruptions: a recorded session that stopped a computation with Escape
or Abort replays it with `{"cmd":"abort","at":AT}` on the line right
after the command it interrupted (the `answer` that started it, for a run
started from a menu), AT being the `stopped` event's `at`. When the player
hands the core a line whose next line is such an abort, it arms a stop
for that line's job and drops the abort line: the job cancels itself
exactly where the recorded one stopped, so the rows it stored, or the AUTO
diagram, are the recorded session's, and it ends with the same `stopped`
event. An integration stops when it has stored `rows` rows; an AUTO run
when it has stored point `point` - 1 of branch `branch`, so that, as every
cancelled run does, it ends the branch on point `point`, an end point (EP)
repeating the one before. If the job ends without getting there, the
script stops with exit status 1 and "script line K: the recorded
interruption at AT was never reached", K being the abort line. The
animation's Go (`{"what":"ani","frame":F}`) stops when it has shown frame
F. An `at` of `other` cannot be placed: the job runs to its end. An abort line with no
`at` stops nothing (the player hands lines over only between commands) and
the script goes on. The browser client's "Save session script" writes
these lines: it records a `stopped` event as the abort, and leaves out the
Escape keys and Aborts it sent while the core was busy. A range
integration (Integrate/Range) stops in the first of its runs that stores
`rows` rows.

examples/scripts/lecar_auto.jsonl is a complete example: it selects the
Le Car model's "hopf" parameter set, finds its fixed point by Newton and
imports it as the initial condition (a Hopf bifurcation is only on the
branch from a converged point), runs an AUTO steady-state continuation
from there, grabs the Hopf point AUTO finds, starts the periodic branch
that bifurcates from it, and saves the diagram:

```
{"cmd":"key","key":"f"}
{"cmd":"key","key":"g"}
{"cmd":"answer","key":"d"}

{"cmd":"key","key":"s"}
{"cmd":"answer","key":"g"}
{"cmd":"answer","key":"n"}
{"cmd":"key","win":"equilibrium","key":"i"}

{"cmd":"key","key":"f"}
{"cmd":"key","key":"a"}

{"cmd":"key","win":"auto","key":"r"}
{"cmd":"answer","key":"s"}

{"cmd":"key","win":"auto","key":"g"}
{"cmd":"answer","key":"Tab"}
{"cmd":"answer","key":"Return"}

{"cmd":"key","win":"auto","key":"r"}
{"cmd":"answer","key":"p"}

{"cmd":"key","win":"auto","key":"f"}
{"cmd":"answer","key":"s"}
{"cmd":"answer","file":"lecar.autox"}
```

Run it with:

    xppautX --script examples/scripts/lecar_auto.jsonl examples/ode/lecar.ode

## Recordings

File/recorD (key `d` of the File menu) or `{"cmd":"record","op":"start"}`
starts recording the session (W59a, core/json_record.cpp; the file's
format is core/recx.h); File/recorD again or `record` `stop` writes it,
`name.recx`, one plain text file that any editor opens:

```
xppautx-recording 1
program: xppautX 8.1
model: lecar.ode
recorded: 2026-09-30T10:14:02Z

@snapshot
UEsDBBQAAAAAA...(the session file, base64)
@end

@file lecar.ode
par iapp=0.1
...
@end

@steps
# First run: the cell fires once and settles.
{"step":"Initialconds → Go","keys":["i","g"]}
{"step":"nUmerics → Total","keys":["t"],"answers":["1e7"]}
{"step":"Initialconds → Go","keys":["i","g"],"abort":{"what":"integrate","rows":2193,"t":219.2}}
{"step":"Zoom window 1","cmd":{"cmd":"display","win":1,"x":[0,50]},"view":true}

fingerprint: 5f0c...(64 hex digits)
```

- The header: the format, the program (`--version`'s), the model's file
  as it names itself, and when the recording began (UTC).
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
  that is not text (an `.autox` or `.snapx` opened while recording, a
  binary table) is a `@binary NAME` section instead: its bytes in base64,
  76 digits a line, then `@end` (W59b). A recording holds no data: a
  replay computes it again.
- `@steps`: one JSON object a line, one per step. A step is everything
  from leaving idle to the next `idle`: the command, the answers to what
  it asked, and where it stopped when an `abort` or Escape cancelled it.
  `step` is a label for a person, from the menus (core/menus.cpp: the
  main menu's item, then each menu item picked, `→` between them; a
  window key's window title first; another command's name and values).
  `keys`: the key the step began with (as the `key` command sent it: a
  character or a named key), then the key of every `menu` or `choice` ask
  it answered (`Escape` for a cancel). `win`: a window key's layer
  ("Window keys"). `button`: the control the key came from, when the
  page said so (the `key` command's `button`); the key stays in `keys`,
  what a replay sends. `cmd`: a command other than `key`, whole (`set`,
  `display`, `values`, ...). `answers`: the answer to every other ask, in
  order: a `string` ask's value, a `form`'s values (an array), any other
  ask's answer without `cmd`, `id` and `ok` (`{"file":"x.set"}`,
  `{"xd":1.5,"yd":-60}`, a grab's `{"key":"Tab"}`), `null` for a cancel;
  a `pixels` ask (the client's, not the user's) and an `alert` have none.
  `view`: the step only changes what is shown (its kind, "Action kinds",
  refined by the menu item it picked), so a player may run it quickly.
  `abort`: the `stopped` event's `at`, as a script's abort line has it
  ("Scripts"). `during`: the keys the running job read itself, each
  `{"key":K,"at":AT}` with where the job was when it took it: `/` ending a
  range or a shooting, Escape stopping the animation's Go (an Escape
  during a computation cancels it: that is `abort`). `files`: the file
  sections the step read, counted from 0 in the file's order, in the
  order it read them; the player serves each read from them. Every zoom or pan (`display`) is a step of its own. A
  setting sent while a command had not computed yet (a value edited while
  the Initialconds menu is open) is a step of its own just before that
  command's, since it applied before anything computed. Not steps: `state`,
  `data`, `equations`, `redraw`, `browser` paging (`from`), `file`, an
  `abort`, `quit`, and `record` itself; nor File/recorD's own opening of
  the File menu (the `f` just before the `d` that stops).
- `# ` lines just before a step are its note (a note of several lines is
  several `#` lines; `#` alone is an empty line of it): `record` `note`
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
back. `play` and `record` are not steps either.

## Playing a recording

File/plaY recording (key `y` of the File menu), Open model of a `.recx`,
or `{"cmd":"play","op":"open","file":...}` opens a recording in the
player (W59b, core/json_player.cpp): it asks, as Open model does, whether
to save this model's session first, then loads the session the recording
began from, its `@snapshot` (W59d): its model, from the files saved there,
then everything as it was when Record was pressed, as Open model of a
`.snapx` restores it (the data table excepted: the steps compute it). The
`player` event lists the steps, and `state.player` says where the player
is. A recording that does not read (not one, no `@snapshot`, a section
with no `@end`, no `@steps`, a step that is not a JSON object with `keys`
or a `cmd`) is an `error` message and nothing changes. One whose
fingerprint does not match plays all the same: `player`'s `intact` is
false, and the page says it was changed after it was made.

Faithful: each step runs exactly as recorded, nothing fixed, skipped or
changed. The player sends the step's command (a `key` with its `win` and
`button`, or its `cmd` whole), then answers each question the command
asks with the recording's next answer of its kind: a `menu` or `choice`
ask the next of `keys` (`Escape`: a cancel), any other the next of
`answers` (a `string`'s value, a `form`'s values, a `file`'s or any other
answer's members; `null`: a cancel); an `alert` it answers OK; a
`pixels` ask is the client's, as always. With the step's last input it
arms its `abort`, or its first `during` key, as a script arms an abort
line ("Scripts"): the job stops, or is handed the key, exactly where the
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
this machine lacks, stops there. `play` `from` loads the snapshot again.

Pace: only computation takes time. Before each input the player sends
`press` (what it is about to send, `ms` before it sends it): a step's
first input 700 ms plus the note's reading time (35 ms a character, at
most 3 s), a menu's key 600, a dialog's answer 900 plus 40 a character
of it (at most 2500), a message 1200; a view step's a third. After a
step's `idle` it waits 800 ms (a view step 250) before the next, never
merging or skipping one. Each is divided by `speed`. `pause` stops the
clock (the wait keeps what is left), `step` runs one step, `from` N runs
the steps before N with no wait (their `press` events 0 ms), then pauses
(or plays on).
The player acts from the command loop's and a question's wait for input,
so the core stays on its one thread and keeps reading the client's lines
while it waits. `tools/servercheck.py` (`check_player`,
`check_player_ani`) and `tools/autocheck.py` (`play`) play recordings
back and compare the data.

## -silent

`xppautX model.ode -silent` is a script of these same commands, built in
(core/json_silent.cpp): after the model loads with no interface at all (a
model that does not load exits 1), the command line and the model's `@`
options say which commands, and they are played as `--script` plays a
file, with no reader, their events going nowhere and what the core says
going to the log as with no interface (nothing new on stdout; an error
message leaves the exit status 0, as before). Each step is made when its
turn comes, after the steps before it ran, since an internal set may
change any option. The questions the commands ask are answered by the
script's next lines; one it does not expect stops it (exit 1). For each
run (once, or for each internal set in turn with `-internset`/`-uset`/`-rset`,
each set's data then in `<set>.dat` unless `-outfile` is given):

| what | commands |
|---|---|
| the internal set | `values` `internset` with its `index` (File/Get par set) |
| `-qsets`/`-qpars`/`-qics`, `-dryrun` | `values` `query` into the output file; nothing is run |
| the run | `key` `i`, `answer` `g` (Initialconds/Go), or `answer` `r` (Range) for `@ range=1` or `@ stoch=` |
| `@ postprocess=` | `browser` `postprocess` |
| unless each run of a range wrote its own `output.dat.N`: `@ stoch=1`/`2` | `key` `u`, `key` `h`, `answer` `m` (Mean) or `v` (Variance), `key` `Escape` |
| output.dat (`@ output=`, `-outfile`; not with `-noout`) | `browser` `write`, `what` `output`, `format` `dat`, `replace` 1 |
| `-mkplot` | `key` `g`, `answer` `p` then the PostScript form answered with its own values (or `answer` `v`, SVG, for `-plotfmt svg`), then `answer` `file` |

and after every run: `@ ncdraw=2` (on a phase plane) `key` `n`, `answer`
`n`, `key` `n`, `answer` `s`, `answer` `file` nullclines.dat; `@ dfdraw=4`/`5`
(on a phase plane) `key` `d`, `answer` `d` or `s`, `answer` the grid,
`dfield` `write` dirfields.dat; `-equil 0`/`1` `equilibrium` `write`
equil.dat (`shoot` 1 for `-equil 1`). The files the command line names
(`-setfile`, `-parfile`, `-icfile`, `-readset`, `-with`) are read by the
model's start in every mode, before the script.

## Events (server to client)

| ev | fields | meaning |
|---|---|---|
| `hello` | `protocol`, `features` (optional parts the server speaks: `series`, `plots`, `nullclines`, `dfield`, `marks`, `ani`, `autoinfo`, `autosettings`, `numerics`), `title`, `file`, `menus` (with `_kinds`), `windows` (AUTO's hints, once `auto_hints`, are `windows.auto.hints`), `commands`, `lists`, `userbuttons` [name...], `sliders` [{`name`,`lo`,`hi`}...] | First event, and again after `open` or `reload` loaded a model in its place. `lists` are what a form field `*n` picks from: 0 T and every variable, 1 ODE variables, 2 parameters, 3 both, 4 colours, 5 markers, 6 methods (items like `2 Box` start with the number to enter). `sliders` are the ones the ODE file sets (`@ s1=...`). `defaults` {`pars`, `ics`}: the ODE file's values, one per entry of `state`'s `pars` and `ics` in their order (what `default` restores). |
| `window` | `op` (`create`, `select`, `destroy`), `win`, `w`, `h`, `title` | Plot windows 1..10, AUTO 101, animation 104. `w`, `h` are the core's pixel size of the window: what the pixel fields of `state.view`, `state.auto` and pixel answers to asks refer to. |
| `diagram` | `op` (`axes`, `reset`, `add`, `views`), `view`, ... | The AUTO diagram as data, each view of it; see "The AUTO diagram as data". |
| `autoinfo` | `info`, `stab`, `stop` | AUTO's info strip and stability circle, and why the last branch ended, as data, for a client that asked (`data`); see "The AUTO diagram as data". |
| `autosettings` | `numerics`, `pars`, `axes`, `marks` | AUTO's settings as data, for a client that asked (`data`); see "AUTO's settings as data". |
| `numerics` | `fields` [{`key`, `label`, `value`, `integer`, `unused`, `choices`}...] | The main numerics as data, for a client that asked (`data`); see "The numerics as data". |
| `series` | `win`, `rows`, `three`, `enc`, `xlabel`, `ylabel`, `zlabel`, `curves`, `shift`, `columns`; or `op` `append`, `win`, `from`, `rows`, `enc`, `columns` | A plot window's curves as numbers, one event per window, for a client that asked (`data`), and during an integration the active window's rows as they are stored; see "The plot as data". |
| `runs` | `win`, `enc`, `erased`, `clear`, `drop`, `add` | A plot window's earlier runs changed, for a client that asked for `series`; see "Display state". |
| `autoview` | `earlier`, `show`, `active`, `views` | AUTO's hidden branches, the active view and each view's zoom, for a client that asked for `autoinfo`; see "Display state". |
| `erase` | `win` | The Erase command blanked plot window `win`, for a client that asked for `series`: it shows nothing of that window's curves (nor the earlier runs it may keep) until the window's next series or `redraw` (the `runs` event carries the same as data). Only Erase sends it: a zoom also redraws the window, and keeps what a data client shows. See "The plot as data". |
| `redraw` | `win` | The Redraw command drew window `win` again, for a client that asked for `series`: it shows the window's current series again (no series follows: the data did not change), and no earlier runs. |
| `plots` | `active`, `windows` [{`win`, `title`, `three`, `xlo`, `xhi`, `ylo`, `yhi`, `xlabel`, `ylabel`, `zlabel`, `box`, `theta`, `phi`, `persp`, `zplane`, `zview`, `curves`, `shift`, `zoom`, `runs`}...] | Every plot window and the active one, for a client that asked (`data`); see "The plot as data". |
| `nullclines` | `win`, `enc`, `xname`, `yname`, `xcolor`, `ycolor`, `x`, `y`, `frozen` [{`x`,`y`}...] | A plot window's nullclines as segments in plot coordinates, for a client that asked (`data`); see "The plot as data". |
| `dfield` | `win`, `enc`, `scaled`, `color`, `n`, `du`, `dv`, `grid`, `speed`, `flows` [{`color`,`x`,`y`}...] | A plot window's direction field and Flow trajectories, for a client that asked (`data`); see "The plot as data". |
| `marks` | `win`, `enc`, `equilibria`, `text`, `arrows`, `markers`, `frozen` | A plot window's equilibria, text, arrows, markers and frozen curves, for a client that asked (`data`); see "The plot as data". |
| `state` | `pars` [[name,value]...], `ics` [[name,value]...], `now` [value...] (after a first run), `bcs` [[name,text]...] (only when the model itself defines boundary conditions, `b`/`bndry` lines or `boundary` statements: then one per variable, those it left out being 0; a model with none sends no `bcs`, the page shows no boundary-conditions section), `delays` [[name,text]...] (delay equations only), `view` {`win`,`left`,`right`,`top`,`bottom`,`xlo`,`xhi`,`ylo`,`yhi`,`three`, and `theta`,`phi` when `three`}, `auto` {`x0`,`y0`,`wid`,`hgt`,`xmin`,`xmax`,`ymin`,`ymax`} (AUTO open), `rows`, `menu`, `win`, `seed` (after a run that used one), `session` {`file`}, `recording` {`steps`, `note`} (while recording), `player` {`step`, `running`, `playing`, `speed`, `fast`, `intact`} (while a recording is open in the player) | Current values; `view` maps pixels of the active window to plot coordinates (x = xlo + (xhi-xlo)(px-left)/(right-left), y likewise with bottom/top) and `auto` those of the AUTO diagram, for a readout under the mouse; `theta`, `phi` are the active window's 3D angles (degrees, meaningful when `three`), so a client that turned a 3D plot itself (`view3d`) can confirm the core agrees; `rows` is the number of stored time points, `menu` the active main menu (0 main, 1 file, 2 numerics), `now` the current state, one value per `ics` entry: where the last run ended or stopped (what Initialconds/Last and `set` `from` `last` copy into the ICs), `win` the active window; `seed` is the seed the last run actually used (each Go, do_range sweep -- Stochastic > Compute's many runs included -- or `-silent` picks and logs one, W71's "a seed per run"; absent before any run in this session); setting the numerics' seed to it and Go reproduces that run's data byte for byte; `session` is `{"file": ...}`, the session file last saved or opened (a path as it was given to `save`, the absolute path of one opened); absent before any; `recording` is there while a recording runs ("Recordings"): the steps recorded so far and the note set for the next one; `player` while a recording is open in the player ("Playing a recording"): `step` the next step to run (the number of steps at the end), `running` the step running (-1 between steps), `playing`, `speed`, `fast` (running steps with no pace to a `from` step), `intact` (the fingerprint matches). |
| `idle` | | The command finished. |
| `stopped` | `at` | The command's computation was cancelled; sent before its `state` and `idle`. `at` says where it stopped: `{"what":"integrate","rows":N,"t":T}` for an integration, N the rows in storage (as `state.rows`) and T the time of the last one stored (9 digits: the stored single-precision value exactly); `{"what":"auto","branch":B,"point":P}` for an AUTO run, P the last point it stored on branch B (the end point the cancel adds, as in the diagram's data); `{"what":"ani","frame":F}` for the animation's Go, F the frames it had shown (W59b); `{"what":"other"}` for anything else. A script replays the interruption from it (see "Scripts"). |
| `player` | `file`, `model`, `intact`, `steps` [{`note`, `step`, ...}] | A recording opened in the player ("Playing a recording"), sent when its model has loaded and again after a `play` `note`: the `.recx` (absolute path), its model's file, whether its fingerprint matches (`false`: it was changed after it was made; it plays all the same), and its steps, each its line's object ("Recordings": `step`, `keys`, `button`, `win`, `cmd`, `answers`, `view`, `abort`, `during`, `files`) with its `note` ("" for none). |
| `press` | `step`, `what`, `index`, `ms` | What the player sends next, before it sends it: `what` `key` (the step's `keys[index]`: its first key, the button's when the step has a `button`, or a menu's or a choice's answer), `answer` (`answers[index]`, a dialog's answer), `cmd` (the step's `cmd`) or `alert` (a message's OK); it goes `ms` milliseconds later (the pace at the speed set), so the page shows it first; 0 while running to a `from` step. A step's first `press` tells the page the step runs: the questions it asks until its `state` with `player.running` -1 are the player's to answer, never the page's (a `pixels` ask excepted). |
| `menu` | `which` | The main menu switched (0 main, 1 file, 2 numerics). |
| `help` | `chapter`, `anchor` (optional) | File/Help: open the manual at this chapter (and anchor). |
| `copy` | `what`, `text` | File/cOpy set line (key `o`): text for the page to put on the clipboard (`what` is `set`: a `set <name> {par=value,...,var=value,...}` line, every parameter and initial condition as they are now, numbers printed to read back exactly). The core first asks the set's name (a `string` ask, pre-filled with the first free `set1`, `set2`, ...; refused with an `error` message if not a name the parser reads or already a set of the model), then a `choice` ask that shows the line (`Copy` `c` / `Cancel` `n`); both answers are recorded like any other. The page shows the line as well, so it can be copied by hand when the clipboard is refused. |
| `title` | `text` | Title of the selected plot window: what it plots (`W vs V`). The server also labels unlabelled 2D axes with the plotted variables. |
| `message` | one of `error`, `bottom`, `box`, `auto`, `calc` | Status text. `box` with empty text removes a hint box. |
| `progress` | `n`, `of` | Computation progress, at most 10 a second. |
| `computing` | | The running command began computing (once per command, before its first `progress`); until its `idle` the server refuses data and computation commands ("Action kinds", "Commands during a command"). A page that connects meanwhile gets it again. |
| `equilibrium` | `type`, `cplus`, `cminus`, `rplus`, `rminus`, `im`, `values`, `eigenvalues` | Result of Sing pts. `eigenvalues`: the Jacobian's `[re,im]` pairs, one per variable; absent for a delay equation. |
| `source` | `lines`, `comments` [[text, has action]...] | File/Prt src. |
| `equations` | `lines` | One `dX/dT=...` line per equation. |
| `ani` | `pos`, `rows`, `fly`, `grab`, `skip`, `speed`, `loaded`, `open`; or `op` `frame`, ... | Animation state for its slider and toggles, sent with every frame drawn and after every `ani` command: `pos` the row the next step starts from, `speed` the ms between frames of Go, `loaded` 1 when an `.ani` file is loaded, `open` 1 while the animation window exists. With `op` `frame`: a frame as data, for a client that asked (`data`); see "The animation as data". |
| `aplot` | `op`: `scroll` (`dy` pixels), `close` | Dragging the array plot scrolls through time; `close` destroys its window. Its other buttons are the window's keys. |
| `film` | `op` (`capture`, `reset`, `play`, `autoplay`), `count`, `win`, `cycles`, `delay` | Kinescope. The client keeps the frames: on `capture` it copies window `win` as it is drawn now; `play` shows them, `autoplay` plays `cycles` times `delay` ms apart. |
| `browser` | `rows`, `cols` (names, `T` first), `row0` (selected row), `start`, `end` (the First..Last range), `from`, `col`, `data` | Rows `from`.. as [T, column `col`, `col`+1, ...]; `null` for NaN. Sent for a `browser` block request and after any command that changed the data while the client shows the browser. |
| `ping` | | Beep. |
| `bye` | | The program is exiting normally: sent by every quit (a plain `quit`, during a computation or not, and the question's outcomes) before the exit, so browser mode's `exit` event says `code` 0. A crash or an error exit sends none (`exit` `code` 1). |
| `error` | `file`, `line`, `col`, `cause`, `source` | The model did not load: sent instead of `hello`, then the program exits (see "A model that does not load"). |
| `file` | `op`, `name`, `ok`; `size`, `sha256` (`put`, `get`), `data` (`get`, base64), `files` (`list`: [{`name`,`size`,`mtime`,`sha256`}...]); `error` when `ok` is 0 | The answer to a `file` command (see "Files" below). |
| `ask` | `id`, `kind`, ... | See below. |

In browser mode (`xppautX model.ode`) events stream from `/events?t=TOKEN`.
Commands are POSTed to `/cmd?t=TOKEN`, one per request (a body of several
lines gives several commands). xppautX answers each connection on a thread
of its own, so a connection that sends nothing (a browser's preconnect) or a
stalled upload holds up no command, `abort` above all; two POSTs in flight at
once may therefore reach the core in either order, and a client that needs
its order sends the next one once the last was answered, as the page does.

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

The server sends the AUTO diagram's points (window 101) as data, so that
a client draws it and can zoom, pan and name the point under the mouse
without a round trip. The data describe exactly what XPP's diagram shows
(and what PostScript and SVG export draw): after the `auto` op `clear` it
is empty (web2 does not send it: its Clear only hides the branches so far).
An Axes change (the AutoPlot form, last 1 par, last 2 par), File/Load
diagram and File/Reset diagram draw the diagram again at once, so a client
always holds the diagram in the current quantities without a reDraw.

A point is one `add_point()` of `core/auto_nox.c`, in the quantities the
axes plot (`auto_xy_plot`: the parameter against the maximum, norm,
period, ... of the Axes setting), in the order it was plotted.

**Views of the diagram** (W50). The one diagram has any number of views
(at least one), each with its own axes (plot type, variable, parameters,
ranges) and zoom: I-V beside I-gca in two-parameter work. Every `diagram`
event names its `view` (0, 1, ...) and applies to that view's points and
axes; a client keeps one list per view. Every view holds one entry per
point of the diagram, in the same order, so a point's index is the same
in every view (the grab, `info.point`, Clear's `earlier`): a point a view
does not plot (a one-parameter point in a two-parameter view, a
two-parameter point in another) comes with `d` 0 and `x`, `y` `null`. One
view is the active one (`autoview`'s `active`): the Axes menu (the AutoPlot
form, zoom, Fit, Scroll, last 1 and 2 par, Default), the exports, the info
strip and a Run go by its axes (its parameters are the ones a run
continues in), and a grab's point by its data.

- `{"ev":"diagram","op":"views","n":N}`: the diagram has N views now; the
  client drops its lists after the first N, or adds empty ones up to N.
  It comes before the events of a new view, and after a view is closed
  (the views after it move down one: the events that follow send their
  lists again).
- `{"cmd":"auto","op":"view","new":1}` adds a view with the active one's
  axes and makes it active (the Axes menu's "new (V)iew", key `v`, does the
  same); `{"cmd":"auto","op":"view","close":k}` closes view k (an error
  message when there is no view k, or it is the last one: one always
  stays); `{"cmd":"auto","op":"view","active":k}` makes view k the active
  one. They are views (kind `v`): sent while AUTO runs, they run after it
  ("Commands during a command"), while every view gets the run's points
  as they come.

- `{"ev":"diagram","op":"axes","view":v, xmin, xmax, ymin, ymax, x0, y0, wid, hgt, plot, xlabel, ylabel}`:
  the diagram was drawn again at these axes (`plot` is `Auto.plot`: 0 hi,
  1 norm, 2 hi and lo, 3 period, 4 two parameters, 10 frequency, 11
  average); the points are unchanged. Pixel `x0 + wid*(x-xmin)/(xmax-xmin)`,
  `y0 + hgt - hgt*(y-ymin)/(ymax-ymin)` of window 101 is (x, y) in the
  core's pixels (what a pixel answer to a `grab` or `rubber` ask means).
- `{"ev":"diagram","op":"reset","view":v,"keep":k, ...the axes fields}`: drop every
  point of view v after the first `k` (all of them for 0), then the axes as above.
- `{"ev":"diagram","op":"add","view":v,"from":n,"runs":[...]}`: points `n`, `n+1`, ... of view v
  (`n` is the number of points the client holds) in runs of points that
  share their branch, kind and style and whose numbers count up by one:

| run field | meaning |
|---|---|
| `br`, `pt` | branch, and the number of the run's first point (absolute values) |
| `ty` | 1 stable steady state, 2 unstable steady state, 3 stable periodic, 4 unstable periodic |
| `f2` | two-parameter curve (1 limit point, 2 limit point of periodics, 3 Hopf, 4 torus, 5 branch point, 6 period doubling, 7 fixed period); absent for one parameter |
| `d` | how it is drawn: 0 not at all (the next line starts from it), 1 a line back to the point before it, 2 filled circles of radius 3 at y and y2, 3 open circles |
| `c`, `lw` | colour (the core's colour index: 0 the foreground, 20..29 red .. purple) and line width |
| `new` | 1: the run's first point starts a new line (no line back) |
| `from` | the label the continuation that computed the run's first point started from (Auto.irs: the point Grab took), on the first point of such a continuation only; absent otherwise, and for a diagram loaded from a file or computed before AUTO kept it. A periodic branch's `from` names the Hopf point it bifurcates from |
| `x`, `y` | the values, one per point (`null` for NaN) |
| `y2` | the second value (the minimum, for hi and lo), when it differs from `y` anywhere in the run |
| `lab` | [[index in the run, label, type (`EP`, `LP`, `HB`, `BP`, `PD`, `TR`, `UZ`, `MX`)], ...] for the labelled points (XPP marks them with a cross at y and y2, the number beside it) |

A run of AUTO sends `add` events as the points come, at most a few a
second. A redraw that plots the same points at other axes
(reDraw, Fit, zoom, scroll) sends only `axes`: the server
compares the points it plots again with what it sent, and only when they
differ (another Axes quantity, new points) does it send `reset` with how
many still agree and `add` for the rest. The points of a redraw go out
when the redraw is over, not while it runs. A new AUTO window starts empty.

**`autoinfo`**: what the AUTO window's info strip and stability circle
show, as data, for a client that asked with
`{"cmd":"data","events":["autoinfo"]}`. It is sent at the end of that
command, and then whenever what it says changed: before every ask (so
every step of a grab brings the point's strip and circle before the grab
asks again), at the end of a command, and at most ten times a second while
AUTO runs; a command that changes neither sends none. A redraw of the
diagram (reDraw, new axes, a loaded diagram) leaves the strip and the
circle as they were: after a run, the run's last point; after a grab, the
grabbed point. core/auto_data.cpp
keeps it, from what auto_nox.c shows there; `stop` comes from
core/auto_stop.cpp, which autlib1.c tells where it ends a branch (T23).

```
{"ev":"autoinfo",
 "info":{"point":18,"br":1,"pt":19,"type":2,"sym":"HB","lab":2,
         "par":[{"name":"iapp","value":0.2624638},{"name":"phi","value":0.2}],
         "norm":0.2891081,"var":"V","u":-0.1989438,"per":14.44537,"x":0.2624638,"y":-0.1989438,"y2":-0.1989438},
 "stab":{"periodic":0,"circle":[[0.9069413,0.4214016],[0.9069413,-0.4214016]],
         "eig":[[6.093e-05,0.434962],[6.093e-05,-0.434962]]},
 "stop":{"why":"parmax","text":"parameter iapp reached Par Max (0.5)","br":1,"pt":49,
         "value":0.5048396225954056,"limit":0.5}}
```

| field | meaning |
|---|---|
| `info` | the point the strip shows: the one a grab's cursor is on (the strip changes only while grabbing); `null` before a grab, and in a new AUTO window |
| `info.point` | its index in the `diagram` data (the points the client holds, the same index in every view), -1 when they do not have it (after Clear, or a load not drawn yet) |
| `info.br`, `pt`, `type`, `sym`, `lab`, `f2` | branch and point number (positive, as in `diagram`), `type` as a run's `ty` (1 stable steady state .. 4 unstable periodic), the label's type (`EP`, `LP`, `HB`, ... or empty), the label (0 for none), and for a two-parameter point its curve kind as `f2` |
| `info.par` | the continuation parameter's `name` and `value`, and the second parameter's for a two-parameter point (the strip then shows both; for a one-parameter point it shows a blank name and 0) |
| `info.norm`, `var`, `u`, `per` | the norm, the variable of the Axes setting and its value, the period (AUTO's value for a steady state too: what the strip prints) |
| `info.x`, `y`, `y2` | where the active view plots the point, in the quantities of its axes (another view's are its own `diagram` data's point `info.point`) |
| `stab` | what the circle shows: the point AUTO computed last, or the grab's cursor; `null` before any |
| `stab.periodic` | 1: `circle` holds the Floquet multipliers of a periodic orbit; 0: e^λ of each eigenvalue λ of a steady state (XPP keeps them so: inside the unit circle is stable) |
| `stab.circle` | `[re,im]` per variable, the values themselves (the X11 circle clamps them to ±1.95). All `[0,0]`: not computed. Always a stored diagram point's own values, while AUTO runs and while grabbing alike (core/auto_stability.h). AUTO computes them from a run's second point on, so a run's first point, and a run that stops there, has none, unless the run restarts from a label of the same kind (a steady state from a steady label, a periodic orbit from a periodic one, one parameter): that first point is the label's solution and carries the label's values. A periodic run from a Hopf point, a two-parameter run and a period doubling's branch switch start with none; a two-parameter curve of periodic orbits (a limit point's, a period doubling's, a torus') has none at all, AUTO computing no multipliers along it. (XPPAUT stores the last values computed with every point, so its first points carry those of another point.) |
| `stop` | why the run's last branch ended; `null` until one ends, and again when a run starts or AUTO's window is new. AUTO labels the end EP (a limit, Max points, Stop, a Mark value) or MX (no convergence); this says which |
| `stop.why` | `parmin` / `parmax`: the continuation parameter went below Par Min (RL0) / above Par Max (RL1); `normmin` / `normmax`: the norm AUTO checks went below Norm Min (A0) / above Norm Max (A1); `npts`: the branch has Max points (NMX); `user`: Stop (an `abort`); `mark`: a Mark value set to stop (AUTO's UZR endpoint); `noconv-min`: no convergence even at the smallest step (Dsmin); `noconv-fixed`: no convergence with a fixed step (IADS 0); `noconv-switch-min`, `noconv-switch-fixed`: the same while switching to a bifurcating branch; `noconv`: no convergence, how not noted |
| `stop.text` | the reason in words, to follow "Stopped: " (the page's status strip); AUTO's Output gets the line `Branch 1 stopped at point 49: parameter iapp reached Par Max (0.5)` for every branch that ends |
| `stop.br`, `pt` | the branch and point number of the end (positive, as in `diagram`) |
| `stop.value`, `limit` | what crossed the limit and the limit: the parameter and Par Min/Max, the norm and Norm Min/Max, the point count and NMX, the step size and Dsmin; `null` where there is none |
| `stab.eig` | steady states only: the eigenvalues λ = log z of the `circle` values, `[null,null]` where z is 0 (Re λ below about -745); the imaginary part is only known modulo 2π, its principal value |

Numbers are doubles in the shortest of 15 or 17 digits that reads back
exactly, `null` when not finite. `tools/servercheck.py` checks that the
point is the `diagram` data's point it names and that the circle holds
what AUTO printed (its fort.9) for that point, and runs
tools/models/auto_stop.ode into each `stop` reason (Par Min and Max, Norm
Max, Max points, no convergence with a fixed and at the smallest step, and
Stop).

### AUTO's settings as data

What the AUTO window's Numerics, Parameter, Axes (the AutoPlot form and
the plot type) and Mark values forms edit, as data, for a client with
forms of its own (web2, docs/ui-v2.md T22). **`autosettings`** is sent to a
client that asked with `{"cmd":"data","events":["autosettings"]}` at the
end of that command, and then whenever the settings changed: at the end of
a command and before every ask. The settings exist from the model's load,
before AUTO's window opens (a model with more variables than AUTO takes
has none, and gets no event). core/auto_settings.cpp keeps it.

```
{"ev":"autosettings",
 "numerics":{"ntst":15,"nmx":2000,"npr":500,"ncol":4,"ds":0.02,"dsmin":1e-05,"dsmax":0.02,
             "rl0":-0.2,"rl1":0.5,"a0":0,"a1":1000,"epsl":0.0001,"epsu":0.0001,"epss":0.0001,
             "iad":3,"mxbf":5,"iid":2,"itmx":8,"itnw":7,"nwtn":3,"iads":1,"suppbp":0},
 "pars":["iapp","phi","v1","v2","v3","v4","gca","vk"],
 "axes":{"plot":2,"var":"V","par1":"iapp","par2":"phi","xmin":-0.2,"xmax":0.5,"ymin":-0.5,"ymax":0.4},
 "marks":[["iapp",0.25],["T",30]]}
```

| field | meaning |
|---|---|
| `numerics` | the Numerics form's fields by AUTO's names, in the form's order: `ntst` Ntst, `nmx` Nmax, `npr` NPr, `ncol` Ncol, `ds`, `dsmin`, `dsmax`, `rl0` Par Min, `rl1` Par Max, `a0` Norm Min, `a1` Norm Max, `epsl`, `epsu`, `epss`, `iad`, `mxbf`, `iid`, `itmx`, `itnw`, `nwtn`, `iads`, `suppbp` SuppBP |
| `pars` | AUTO's parameters, the Parameter form's Par1.. (as many as the model has, at most 8): the model parameters AUTO can continue in |
| `axes.plot` | the plot type: 0 hi, 1 norm, 2 hi and lo, 3 period, 4 two parameters, 10 frequency, 11 average |
| `axes.var` | the variable the y axis plots (Y-axis) |
| `axes.par1`, `par2` | Main Parm and Secnd Parm, two of `pars` (`null` if none) |
| `axes.xmin` .. `ymax` | the diagram's axes |
| `axes` | all of it the active view's (see "Views of the diagram") |
| `marks` | Mark values: `[name, value]` per user point, where the parameter `name` (one of `pars`) or the period `T` reaches `value` AUTO labels the point (UZ) |

Numbers are doubles in their shortest exact form, the whole-number fields
as integers; `null` when not finite.

**`{"cmd":"auto","op":"set", ...}`** writes them without the forms, with
any of the four members above, each part only when given: `numerics` any
of its fields, `pars` the first N of AUTO's parameters (an empty name keeps
one), `axes` any of `plot`, `var`, `par1`, `par2` (names among `pars`,
after this set's own `pars`), the four ranges, and `"fit":true` for
Axes/Fit afterwards, and `"view":k` for view k's axes (the active one's
without it; setting a view's axes makes it the active one), `marks` the
whole list (0 to 9 pairs; `[]` for none).
The core checks every value first and sets all or nothing: a whole number
where the form's field is one; Ntst, Nmax, NPr, ITMX, ITNW, NWTN at least
1; Ncol 2 to 7; IID 0 to 5; IAD and IADS at least 0; SuppBP 0 or 1; Ds not
0; Dsmin, Dsmax and the EPS values above 0; Dsmin at most Dsmax, |Ds| from
Dsmin to Dsmax (T23; checked when one of the three is given), Par Min
below Par Max, Norm Min below Norm Max, Xmin below Xmax, Ymin below Ymax;
names of parameters and variables the model has. A refusal is a `message`
`error` naming the value (`AUTO settings: Ncol must be a whole number from
2 to 7`). Axes (and new parameters) draw an open diagram again in its new
quantities, as the AutoPlot form's OK does. The forms stay: `set` is a
second way to the same fields (Numerics, `param`, Axes, `usr`), and each
shows what the other wrote.

`set` is a setting (W106): sent while AUTO (or anything) computes, the
core takes it at once and applies it after that job's `idle`, as a command
of its own (a continuation already running keeps the settings it started
with; the next run uses the new ones). `tools/servercheck.py` checks that the
event is what the forms show, that a form's OK and a `set` show in it,
that bad values are refused whole, and that Nmax set to 12 stops the next
run at 12 points.

### The numerics as data

The main numerics, the Numerics menu's values (W106), as data for a client
with fields of its own (web2's values panel). **`numerics`** is sent to a
client that asked with `{"cmd":"data","events":["numerics"]}` at the end of
that command, and then at the end of any command that changed them.
core/numerics_settings.cpp keeps it.

```
{"ev":"numerics","fields":[
  {"key":"total","label":"Total","value":30},
  {"key":"dt","label":"Dt","value":0.05},
  {"key":"nout","label":"nOutput","value":1,"integer":true},
  {"key":"method","label":"Method","value":3,"integer":true,
   "choices":["Discrete","Euler","Mod. Euler","Runge-Kutta",...]},
  {"key":"tol","label":"Tolerance","value":0.001,"unused":true}, ...]}
```

The fields, in the menu's order: `total` (below 0: integrate for ever, as
the menu's), `t0`, `trans`, `dt`, `nmesh` (Ncline ctrl), `newt_iter`,
`newt_tol`, `jac_eps` (sIng pt ctrl), `nout`, `bound`, `method` (its number,
`choices` its names by number), `tol`, `dtmin`, `dtmax`, `atol`, `eul_tol`,
`eul_iter` (the method's own), `delay` (only for a model with delays),
`bvp_maxit`, `bvp_tol`, `bvp_eps` (bndVal). `integer` marks a whole-number
field; `unused` one the current method does not use.

**`{"cmd":"set","kind":"num","name":KEY,"value":V}`** (or `"text"`) sets one
field. The core checks the value first and changes nothing on a bad one,
with a `message` `error` naming the field (`Numerics: nOutput must be a
whole number of at least 1`): a number; `dt` not 0; the tolerances,
`bound`, `newt_tol`, `jac_eps` above 0; `delay` at least 0; the whole-number
fields at least 1; `method` a name of `choices` (any case) or its number, one
the model can use (Volterra only for integral equations, and only it then;
Symplectic only for an even dimension). Then it applies the numerics as
leaving the Numerics menu does (the delays' and integrals' memory for a new
`dt` or `delay`, a fresh solver; a method that picks its own steps stores
every output time, NOUT 1). The menu stays: `set` is a second way to the
same fields. A setting ("Commands during a command"): sent during a run it
applies when the run ends.

### The plot as data

The page (docs/ui-v2.md) draws the plots itself from numbers. Five events carry them, each sent only to
a client that asked with
`{"cmd":"data","events":["series","plots","nullclines","dfield","marks"]}` (any of
the names alone works too), at the end of that command and then at the end
of every command after which what it says has changed. Nothing else sends
them, so a command that only redraws sends none. They come before the
command's `state` and `idle`, in that order: `plots`, the `series`, the
`nullclines`, the `dfield`, the `marks`.

**`series`**: one event per plot window (`win` 1..10), each at the end of
a command after which what that window shows has changed: the stored data
(an integration, Continue, a browser Load or Replace, ...; this changes
every window), the window's curves (Xi vs t, Viewaxes, Graphic
stuff/Add curve, ...), or the window itself (a new one gets its series at
once). The active window's comes first. Switching the active window sends
none: the client has every window's already. A client that shows one plot
keeps the event whose `win` is the active one (`plots.active` or
`state.win`).

```
{"ev":"series","win":1,"rows":601,"three":0,"xlabel":"","ylabel":"","zlabel":"",
 "curves":[{"x":1,"y":2,"z":1,"color":0,"line":1}],"shift":[0,0,0],
 "columns":[{"col":0,"name":"T","data":[0,0.0500000007,...]},
            {"col":1,"name":"V","data":[-0.143999994,...]},
            {"col":2,"name":"W","data":[0.0299999993,...]}]}
```

| field | meaning |
|---|---|
| `win` | the plot window (1..10), as in `window` and `plots` |
| `rows` | stored rows: every column has this many values (0 before an integration) |
| `three` | 1 when the window is a 3D plot (then `z` matters) |
| `xlabel`, `ylabel`, `zlabel` | the window's axis labels; empty means "the plotted column's name" |
| `curves` | the window's curves (`MyGraph->nvars` of them): `x`, `y`, `z` are storage columns (0 is T, `i` is `cols[i]` of `browser`), `color` the XPP colour index (0 foreground, 1..10 red .. purple), `line` > 0 a line, <= 0 points of radius `-line` |
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
(a window made or destroyed, the active one, a window's axes, curves,
labels or 3D view); its text is compared with the last one sent.

```
{"ev":"plots","active":2,"windows":[
 {"win":1,"title":"W vs V","three":0,"xlo":-0.6,"xhi":1.2,"ylo":-0.25,"yhi":1.2,
  "xlabel":"","ylabel":"","zlabel":"",
  "box":{"xmin":-0.6,"xmax":1.2,"ymin":-0.25,"ymax":1.2,"zmin":-12,"zmax":12},
  "theta":45,"phi":45,"persp":0,"zplane":-1000,"zview":1000,
  "curves":[{"x":1,"y":2,"z":1,"color":0,"line":1}],"shift":[0,0,0]},
 {"win":2,"title":"V vs T",...}]}
```

| field | meaning |
|---|---|
| `active` | the active window: the one keys, Viewaxes, Xi vs t, ... act on (`click` selects another) |
| `windows` | every plot window, by `win`; a window missing from the list was destroyed |
| `title` | what the window plots, `y vs x` (`z vs y vs x` in 3D), as its X11 title |
| `three` | 1 for a 3D plot |
| `xlo`, `xhi`, `ylo`, `yhi` | the window's axes (Viewaxes, Window/Zoom); in 3D the projected view's |
| `xlabel`, `ylabel`, `zlabel` | the axis labels; empty means "the plotted column's name" |
| `box` | the 3D box (3d-params, Viewaxes in 3D): the data ranges of x, y and z |
| `theta`, `phi` | the 3D view's angles in degrees (3d-params, `3`) |
| `persp`, `zplane`, `zview` | perspective on (1) or off, and its planes |
| `curves`, `shift` | as in `series` |

Numbers are the doubles themselves, in the shortest of 15 or 17 digits that
reads back exactly; `null` for a value that is not finite. Makewindow
(`m`, then `c` create, `d` destroy the active window, `k` kill all but
window 1) changes the list; `click` with a window's `win` makes it active.

**`nullclines`** and **`dfield`**: what a phase plane shows besides its
curves, one event per plot window, each saying what XPP's window
shows: Nullcline/New, Dir.field/flow and the redraws that draw them again
fill them; anything that blanks the window empties them unless it draws them
again (Erase; Viewaxes to other variables, which redraws without them). So after Erase both are
empty, and a later redraw brings the nullclines back (they are redrawn
while Nullcline/Restore is on) but not the field Erase turned off. Each is
sent at the end of a command when the window's content differs from the
last one sent (compared value for value: drawing the same again sends
nothing), and for every window once after `data`. The active window's
comes first.

```
{"ev":"nullclines","win":1,"xname":"V","yname":"W","xcolor":2,"ycolor":7,
 "x":[-0.596100688,0.475000024,-0.600000024,0.503065467,...],"y":[...],
 "frozen":[{"x":[...],"y":[...]}]}
```

| field | meaning |
|---|---|
| `xname`, `yname` | the variables of the x- and y-nullclines (where their derivative is 0): the window's x and y axes when they were computed; `""` before any |
| `xcolor`, `ycolor` | their XPP colour indices (as `curves` `color`; 2 and 7 unless the model sets them) |
| `x`, `y` | the x- and y-nullcline as line segments, 4 values each: `[x1,y1,x2,y2, x1,y1,x2,y2, ...]`, in plot coordinates, in the order XPP draws them; empty when the window does not show them |
| `frozen` | the frozen nullclines (Nullcline/Freeze) the window shows, the same colours, each set's `x` and `y` as above |

```
{"ev":"dfield","win":1,"scaled":1,"color":0,"n":17,"du":0.1125,"dv":0.090625,
 "grid":[-0.600000024,-0.25,0.305723518,0.952120364,...],"speed":[0.0421,...],
 "flows":[{"color":0,"x":[-0.600000024,-0.59258616,...,null,...],"y":[...]}]}
```

| field | meaning |
|---|---|
| `n` | grid points a side (Dir.field's Grid + 1); 0 when the window shows no field (then `grid` and `speed` are empty) |
| `du`, `dv` | the grid's spacing in plot units, x and y |
| `grid` | one arrow per grid point, 4 values each: `[x,y,ux,uy, ...]`, the point and the field's direction there as a unit vector in plot coordinates (`0,0` where the field is 0), x outer, y inner, in XPP's order |
| `speed` | one per arrow: the length of the field's (x, y) components there, in plot units per unit time |
| `scaled` | 1 (Scaled Dir.Fld): every arrow the same length, a quarter of a grid cell's diagonal in XPP; 0 (Direct field): lengths in proportion to the speed, the fastest a whole cell's diagonal. The client scales the arrows to its own grid spacing on screen: a direction in plot units maps to pixels with the axes' scales, so it is normalized after that mapping. |
| `color` | the arrows' XPP colour index (the window's first curve's) |
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
for every window once after `data`; the active window's first.

```
{"ev":"marks","win":1,
 "equilibria":[{"x":-0.1425351775676935,"y":0.034048992272573395,"type":"saddle","symbol":"triangle"}],
 "text":[{"x":-0.298305094,"y":0.498503745,"text":"\\1a\\0-point 6","size":3,"font":0}],
 "arrows":[{"kind":"pointer","x1":0.1,"y1":0.2,"x2":0.6,"y2":0.8,"size":0.2,"color":5}],
 "markers":[{"x":0.9,"y":0.1,"shape":"diamond","size":2,"color":7}],
 "frozen":[{"key":"first run","name":"frz1","color":4,"line":1,"x":[...],"y":[...]}]}
```

| field | meaning |
|---|---|
| `equilibria` | the points Sing pts marked (Monte Carlo's too), in the order marked, each once: `x`, `y` in plot coordinates (the window's x and y variables at the equilibrium, the doubles themselves: the `equilibrium` event's values for those variables), `type` what its symbol says (`stable`: no eigenvalue with a positive real part; `saddle`: some on each side; `unstable`: the rest) and `symbol` XPP's (`circle`, `triangle`, `box`). 2D windows only |
| `text` | Text,etc's labels: `x`, `y` where the text starts (its baseline, plot coordinates), `text` as drawn (`\{expr}` already replaced by its value), `size` 0-4, `font` (1: all in the symbol font; 0 otherwise). XPP's markup stays in the text: a backslash and `1` switches to the symbol font, where the Latin letters show Greek ones (`\1a` is α, Adobe Symbol encoding), `0` back to roman, `s` and `S` a subscript and a superscript one size smaller (they add up), `n` back to the baseline and size; any other character after a backslash is dropped. Bytes that are not UTF-8 are Latin-1 |
| `arrows` | Text,etc's arrows and pointers: `kind` `arrow` (only a head) or `pointer` (a head and a shaft), the head's tip at `x1`, `y1`, pointing from `x2`, `y2`; `size` the head's length as a fraction of that distance (half as wide as long); `color` an XPP colour index |
| `markers` | Text,etc's markers (Marker, marKers): `x`, `y`, `shape` (`box`, `diamond`, `triangle`, `plus`, `cross`, `circle`), `size` (1 is about 1% of the window), `color` |
| `frozen` | the frozen curves of the window (2D ones): `key` (its legend text) and `name` (Freeze's Edit form), `color` an XPP colour index, `line` 1 a line, 0 points (a negative colour in the form), `x`, `y` the values of the curve's x and y columns when it was frozen (float32, as `series`: equal to that `series`' columns then) |

Label and object positions are the stored float32 values, 9 digits.

**Binary values.** After `{"cmd":"data","events":["series"],"enc":"f32"}`
every `series` event (full or append) has `"enc":"f32"` and each column's
`data` is a string (and so is every value array of `nullclines`,
`dfield` and the frozen curves of `marks`, which then carry `"enc":"f32"` too): the base64 (RFC 4648, with padding) of the values as
IEEE float32, 4 bytes each, least significant byte first, whatever the
host's byte order. NaN and infinities travel as they are. That is 5.3
characters a value instead of about 12, and nothing to parse: a million
rows of three columns is 16 MB, which a browser decodes in milliseconds
(`Uint8Array.fromBase64`, or `atob`). The decoded floats are exactly the
JSON numbers read as float32. `data` without `enc`, or any other `enc`,
means JSON numbers. A client should read `enc` on every event: a server
that does not know the option sends JSON numbers.

**Live runs.** While an integration runs (Initialconds/Go, Continue, a
range, ...), the rows it stores go out as they come for
the active window, at most about once a display frame (60 times a second):

```
{"ev":"series","op":"append","win":1,"from":1200,"rows":2400,
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
the rows stored so far instead and appends after it. The command still
ends with the full `series` (always after appends, else when something
changed), before its `state` and `idle`, and no append follows it; its
first rows are what the appends delivered. The other windows get only
their full `series`, after the active one's. A client that did not ask for
`series` gets neither.

The appends come from the integrator itself (`rows_stored()` in
core/xpp_ui.h, called for every stored row; `plot_data_rows_stored` in
core/plot_data.cpp sends at most one append per 1/60 s, so a page that
draws each on its next animation frame extends the curve at every frame;
W82).

### Session files

A session file, `name.snapx` (W57, core/xpp_session.cpp; its pure part,
the member names, the manifest and the model's members, core/snapx.h),
is a zip of ordinary files, in this order:

| Member | What it holds |
|---|---|
| `session.txt` | the manifest: `xppautX session 1`, then `name` (the model's own file, as the model names it), `anifile` (the animation `-anifile` loaded, one of the model's files; only when there is one), `data` (1 when `data.npz` is there), one `key value` line each |
| `model/<name>` | the model (W103): its `.ode` or `.odex` first (`model/` and `name`), then every other file its load read, each by the name the model gives it (a path as the model writes it, relative to its folder or whole): the files it includes, its file tables, its options file, `-anifile`'s animation; byte for byte |
| `model.set` | File/Write set's file (values, numerics, delays, boundary conditions, the active window's graphics): the original XPPAUT reads it |
| `auto/settings.txt`, `auto/diagram.csv`, `auto/solutions.s`, `auto/views.txt` | AUTO's members, as an AUTO file has them ("AUTO files" below: its settings, the diagram at full precision, its orbits and the views of it), when there is a diagram |
| `windows.set` | every plot window (its variables by name, labels, `# Graphics` block, zoom and earlier-runs toggle), which is active, AUTO's hidden branches (`autoview`'s `earlier` and `show`), the browser's added columns; set-file lines, a value and its name |
| `marks.set` | the text labels, arrows and markers, and the frozen curves' settings, per window |
| `frozen.npz` | the frozen curves' points, one (points, 3) array `curve<slot>` each, when there are some |
| `data.npz` | the data table as Save data's NPZ writes it (`T.npy`, `V.npy`, ..., `seed.npy`), unless left out |

Opening one (`open`, `session` `load`, the command line) loads the model
saved in it, from those saved files alone: an edit to the `.ode` since,
or its absence, makes no difference, and a file the model reads that is
not saved in it is a file that is not there. Then it restores the
members: `state`, `plots`, `marks` (but Sing pts' equilibrium symbols),
`autoview` and the diagram in each of its views are as they were at the
save, the diagram exactly (every digit). The earlier runs a window keeps until Erase are
not saved. A file without its model (`model/<name>` missing, as in every
session file saved before W103), or without its manifest, is refused (a
`message` `error` says what is missing) and nothing changes. So is one
whose manifest has a line it does not write (`its session.txt: its line 5
is not one it has: "later 1"`), and (W116) one with a member it always
writes missing (`model.set`, `windows.set`, `marks.set`; `data.npz` when
`data` is 1, and only then; AUTO's four when one is there; the points of
every frozen curve `marks.set` lists) or one whose member does not read: a
line missing or not a number, a variable, a window or a slot it does not
have. The members are read against the model's load before it is kept,
so the session before stays exactly as it was, its model included; the
error names the member and the line: `s1.snapx could not be loaded
(s1.snapx: its windows.set, line 11: the file ends here); lecar.ode is
still loaded` (at the start, the command line's file, the `error` event,
then exit 1). The command
line opens one too, `xppautX name.snapx` (every mode but `-silent`),
from its own folder.

A recording, `name.recx` (W59c), is registered with the OS beside them (the
same scripts in `tools/associate/`), and `xppautX name.recx` (every mode
but `-silent`) starts the recording's own model and opens the recording in
the player, as `{"cmd":"play","op":"open"}` does, without the question
File/open Model asks (nothing is open to save).

### AUTO files

AUTO's own file, `name.autox` (W92, core/autox_io.cpp; its pure part, the
member names and the text of its members, core/autox.h; read and written
by the reader and writer it shares with session files, core/xpp_session.cpp),
holds AUTO's work with its model, without a whole session: the AUTO
window's File/Save diagram (key `s` of its File menu, a `file` ask with
wildcard `*.autox`) writes one, `.autox` added unless the name ends so (a
name ending in `.auto` gets an `x`). Opening one (File/Load diagram, key
`l`, wildcard `*.autox *.auto`; `open`; the command line) loads its saved
model as `open` does, then its diagram; with that model already open
(the same files, byte for byte) only the diagram loads, in place of the
one there, and the data stays. It is a zip of ordinary files, in this
order:

| Member | What it holds |
|---|---|
| `autox.txt` | the manifest, as a session file's `session.txt` but for its first line, `xppautX autox 1`, and no `data` line: `name` (the model's own file) and `anifile` |
| `model/<name>` | the model, as in a session file |
| `settings.txt` | AUTO's settings, one `key value` line each, with the keys of `auto` `set` ("AUTO's settings as data" above): every Numerics key (`ntst` ... `suppbp`), `pars` and AUTO's parameters' names, `plot`, `var`, `par1`, `par2`, `xmin`, `xmax`, `ymin`, `ymax`, and one `mark NAME VALUE` line per Mark value; `-` stands for no name |
| `diagram.csv` | the diagram, a header row of names and one row per point in the order stored: `calc`, `ibr` (branch), `ntot` (point, negative when stable), `itp` (type), `lab` (label), `nfpar`, `icp1`..`icp4`, `flag2`, `from` (the label its run started from), `norm`, `per`, `torper`, `par1`..`par20` (AUTO's parameters' values), then for each variable x `u0.x`, `uhi.x`, `ulo.x`, `ubar.x`, and last `evr1`, `evi1` ... `evrN`, `eviN` (the eigenvalues or Floquet multipliers, zeros when not computed) |
| `solutions.s` | AUTO's solution file (`fort.8`) as AUTO wrote it: the solutions at the labelled points, which a grab restarts from |
| `views.txt` | the views of the diagram (W50, "Views of the diagram" above), one line each in order, `view PLOT VAR PAR1 PAR2 XMIN XMAX YMIN YMAX ZOOMX ZOOMY` (the axes as `settings.txt` names them, each zoom `LO:HI` or `-` for the whole axis), then `active K`; restored exactly, ranges included |

Every number is the shortest text that reads back as the same double, so
a diagram saved and loaded is the same bit for bit, and continues from a
grabbed point as the one saved would. A file without its model is
refused, as a session file is; so is one without `settings.txt`,
`diagram.csv`, `solutions.s` or `views.txt` (every file saved before W50
lacks the last), or with one that does not read (a key `settings.txt`
does not have, or one missing; a row of `diagram.csv` cut short): an
error names the member (`d1.autox: its views.txt is missing`, `its
diagram.csv cannot be read`; in a session file, `auto/views.txt`), and
nothing changes, the model open included (W116). Save diagram, and Save
session with a diagram, are refused with an error when AUTO's solution
file cannot be read (the orbits a grab restarts from). A setting the model no longer takes
leaves AUTO's settings as they were, which a `message` `bottom` says. An
XPPAUT `.auto` file still loads into the model open, as an import (after
asking whether to destroy the diagram there is): its settings, its
diagram (to the 6 digits it prints) and its solutions; File/Save diagram
then writes an `.autox`. Nothing writes a `.auto` any more.

### Display state

What the page displays is the core's (W65, docs/ui-v2.md): the earlier runs
of each plot window until Erase, its zoom, whether the earlier runs are
drawn, and AUTO's hidden branches and zoom. They are members of the
session (`xpp::Session::plot_display`, `auto_view`, core/display_state.h),
set by the `display` commands or by the runs themselves, and sent as data,
so a session file can save them. The page keeps a copy it changes at once
(a drag cannot wait for a round trip) and sends the change; the core's
events set it back, and it leaves a window's own alone while its change is
on its way.

**`runs`** (with `series`): a window's earlier runs. A full series with
other curves (Xi vs t, Viewaxes, ...) forgets them; a full series of other
data (its `version`) keeps the current run as an earlier one, unless the
current one grew by appends in this command (the full series ends that run)
or was erased; an append that starts again before the rows the client holds
(a new run under way, the next run of a range) keeps the current run first.
Erase forgets them and hides the current run (`erased` 1) until its next
run or Redraw. At most 50 earlier runs and 4 million rows in all are kept,
the oldest dropped.

```
{"ev":"runs","win":1,"erased":0,"clear":0,"drop":0,"enc":"f32","add":[
 {"rows":601,"three":0,"curves":[...],"shift":[0,0,0],
  "columns":[{"col":0,"name":"T","data":[...]},{"col":2,"name":"W","data":[...]}]}]}
```

| field | meaning |
|---|---|
| `win` | the plot window |
| `erased` | 1: Erase blanked the window, the current series is not drawn |
| `clear` | 1: forget every earlier run held first |
| `drop` | forget this many of the oldest ones first |
| `add` | runs to append, newest last: `rows`, `three`, `curves`, `shift` and `columns` (`col`, `name`, `data`) as in `series`, of the columns the run's curves use |

Sent when it changes (not at all for a window with none), and whole (`clear`
1 with all the runs) after `data` asks for `series` again.

**`plots`** carries each window's `zoom` (`{"x":[low,high]|null,"y":...}`,
the part of its axes shown; `null` the window's own) and `runs` (1: the
earlier runs are drawn). Other axes or curves of the window (Viewaxes,
Window/Zoom, Fit, Xi vs t) drop the zoom. The `display` command sets them:
`{"cmd":"display","win":1,"x":[0,10],"y":null,"runs":false}` (a missing key
is left as it is).

**`autoview`** (with `autoinfo`): `{"ev":"autoview","earlier":n,"show":0,
"active":0,"views":[{"zoom":{"x":null,"y":null}},...]}`, sent at the end of
a command when it changed, and at once after `data`. `earlier`: the
diagram's points before this index are the branches computed before Clear
(the AUTO window's key `c` sets it to the points so far, in every view; a
diagram of fewer points, File/Reset diagram, lowers it, closing AUTO clears
it); `show` 1: they are drawn; `active` the active view; `views` one entry
per view of the diagram (W50), its `zoom` as above, dropped by other axes
of that view (closing AUTO drops every view's zoom; the views stay).
`{"cmd":"auto","op":"display","view":1,"show":true,"x":[...],"y":null}`
sets `show` and view 1's zoom (the active view's without `view`).

### Asks

| kind | fields | answer fields |
|---|---|---|
| `menu` | `name`, `title`, `items`, `keys`, `hints`, `def` | `key` (empty or `ok:0` cancels) |
| `choice` | `title`, `question`, `choices`, `keys` | `key` |
| `string` | `title`, `name`, `value`, `ok`, `cancel`, `kinds` | `value` (any length: no dialog cuts what is typed, W76) |
| `form` | `title`, `names`, `values`, `kinds` | `values` (same length, each of any length: W76 dropped `max`). A name starting with `*n` means the field picks from `hello.lists[n]`: a variable (`*0`), a parameter (`*2`), a colour (`*4`), a marker (`*5`), ...; for a list whose items start with a number (`2 Box`) the value is that number. |
| `checklist` | `title`, `names`, `flags` | `flags` |
| `file` | `title`, `mode` (`read` or `write`), `file`, `wild`, `dir`, `dirs`, `files` | `file` (a name in `dir`, or a full path anywhere, whole: the desktop window's own dialog answers with one, W88); or `cd` (a folder name or `..`) or `wild` (a new pattern) to be asked again with that listing |
| `alert` | `button`, `message` | nothing |
| `mouse` | `win` | `x`, `y`; or `xd`, `yd` (data coordinates, below) |
| `rubber` | `win`, `flag` (0 box, 1 line) | `x`, `y`, `x2`, `y2`; or `xd`, `yd`, `xd2`, `yd2` |
| `grab` | `win` | `key`; or `x`, `y` (or `xd`, `yd`) for a click on the diagram; or `point`, a point of the `diagram` data by its index (with `key`, that key after it) |
| `drag` | `win` | `what` (`down`, `move`, `up`), `x`, `y` (or `xd`, `yd`) for each pointer event; cancel or a key ends. Window/Scroll and AUTO Axes/Scroll ask it again after every event. |
| `pixels` | `win`, or `film` (a kinescope frame index) | `w`, `h`, `rgb` (base64 of w*h*3 bytes). Frame, GIF and kinescope writers use it: only the client has the picture, which web2 renders from the data it holds (the window's chart, or a kinescope frame's snapshot). |

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
text. The kinds are a client's guide, not a check: the core reads what it
is answered as it always did. A `*n` field (above) picks from its list
whatever its kind. web2 marks a field whose text its kind does not take
and does not answer until it is corrected (web2/src/store/fieldKinds.ts);
`tools/servercheck.py` checks the kinds of a `formula`, a form of
numbers, an `integer` and a `name:0` ask.

**Data coordinates.** A point of a `mouse`, `rubber`, `drag` or `grab`
answer can be given in the plot's own quantities instead of pixels: `xd`,
`yd` for the point (`xd2`, `yd2` for a rubber band's second corner), in the
coordinates `state.view` maps to (the active window's `xlo`..`xhi`,
`ylo`..`yhi`; for the AUTO diagram, window 101, those of `state.auto` or
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

**Grab by label.** `{"cmd":"auto","op":"grab","label":N}` grabs the
stored point labelled `N` directly, with no ask: the same outcome as the
interactive grab above ending with `Return` on that point (`grabpt`, the
parameters, the info strip, the stability circle, and what a following
Run starts from). `{"cmd":"auto","op":"grab","type":"HB","index":k}`
grabs the `k`-th (1-based, in stored order) labelled point of that AUTO
type instead (`auto_bif_sym`'s `BP`, `EP`, `HB`, `LP`, `MX`, `PD`, `TR`,
`UZ`: "the 2nd HB"). A label no stored point has, or a `type`/`index` with
no such point, is a `message` `error` and nothing changes -- the diagram,
`grabpt` and the info strip stay as they were. `{"cmd":"auto","op":"grab"}`
with neither field is refused (`message` `error`): the interactive grab is
the AUTO window's key `g`, `{"cmd":"key","win":"auto","key":"g"}`, which
asks (`grab`, above). A script that recorded an interactive grab (W59) replays it as the
keys it was answered with, exactly as it was driven; `grab`/`label` is for
a script that names the point instead (W56's -silent commands).

**The file ask's mode.** `mode` says whether the command opens the file
(`read`: Read set, Load diagram, the browser's Load, Import, ...) or saves
one (`write`: Write set, Save diagram, PostScript, SVG, ...), so a client
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

A NAME is percent-decoded, then must be a base name: no `/` or `\`, no
`..`, no leading dot (hidden files, `.` and `..`), no leading space and no
trailing dot or space, no control characters, none of `: * ? " < > |`
(which also refuses drive letters), not a Windows device name (`CON`,
`NUL`, `COM1`, ...), at most 255 bytes. Anything else is refused (400, or
`ok` 0). A name that is a symbolic link, a folder or anything but a plain
file is refused too (403): nothing outside the folder is reached through
it. The server listens on 127.0.0.1 only.

### The animation as data

A client that asked with `{"cmd":"data","events":["ani"]}` gets each frame
of the animation (window 104) as data, in the animation's own coordinates,
and draws it at any size itself (web2, docs/ui-v2.md T13).
core/aniparse.cpp evaluates a frame in the `.ani` file's coordinates (its
`dimension` box, [0,1] x [0,1] unless the file says otherwise, y up) and
hands each primitive to core/ani_data.cpp, which sends it in unit
coordinates:

    u = (x - xlo) / (xhi - xlo)      v = (y - ylo) / (yhi - ylo)

so (0,0) is the box's bottom left and (1,1) its top right. They are not
clamped: where the `.ani` puts something outside its box the value lies
outside [0,1]. XPP's own window put a primitive at `u*w`, `h - v*h`.

```
{"ev":"ani","op":"frame","pos":0,"rows":601,"t":0,"speed":5,"skip":1,
 "dim":[-0.6,-0.1,0.4,0.6],"w":280,"h":350,
 "prims":[["text",0.05,0.928571,"lecar  ",9,3,0],["line",0,0.142857,1,0.142857,0,1],
          ["circle",0.456,0.185714,0.03,0.0428571,10,0,0],["dot",0.456,0.185714,2,1],...]}
```

| field | meaning |
|---|---|
| `pos`, `rows` | the stored row the frame shows (0-based), of how many |
| `t` | the frame's time (9 digits; `null` when not finite) |
| `speed`, `skip` | ms between two frames of Go, rows per step |
| `dim` | the `dimension` box: `xlo`, `ylo`, `xhi`, `yhi`. A client that keeps its aspect `(xhi-xlo)/(yhi-ylo)` has equal units along x and y |
| `w`, `h` | the core's pixel size of the animation window: what line widths, dots and text sizes are relative to |
| `prims` | the primitives in drawing order, each an array (below); unit coordinates have 6 significant digits, `null` where the `.ani` evaluated to NaN |

| primitive | arguments |
|---|---|
| `line` | `u1`, `v1`, `u2`, `v2`, colour, width |
| `rect` | `u1`, `v1`, `u2`, `v2` (two opposite corners, either order), colour, width, fill (0/1) |
| `circle` | `u`, `v`, `ru`, `rv`, colour, width, fill: the centre, and the radius over the box's width (`ru`) and over its height (`rv`); XPP draws it with the mean of the two in pixels, `(ru*w + rv*h)/2` |
| `ellipse` | `u`, `v`, `ru`, `rv`, colour, width, fill: the centre and the two radii, as `circle`'s |
| `dot` | `u`, `v`, `r`, colour: a filled circle of `r` pixels (a comet's, with a negative thickness) |
| `text` | `u`, `v`, string, colour, size (0..4), font (0 roman, 1 symbol: Greek letters): from the baseline's left end |

A colour is an XPP colour index (0 the foreground, 1..10 red .. purple, as
`curves` `color`) for the `.ani`'s named colours, or `"#rrggbb"` for a
colour of the colour map (an expression's value, 0..1). A primitive's
colour, width and text font are the pen's at that point: the colour starts at 0 each frame, width and font
carry over from the frame before, `settext` sets the font and colour for
the text after it, and text takes the colour last set, as XPP draws it.
Widths are in pixels (0 is a thin line of one pixel).

A frame goes out when the core draws it: after `step`, `seek`, `reset`, a
`file` that shows the first frame, `grab` and its `mouse`, Fly's frames
during an integration, and Go's. Frames that come faster than 25 a second
(Go, Fly) are thinned: the latest one goes when 40 ms have passed since the
last one sent, and the last frame drawn always goes, at the latest at the
end of the command, before its `state` and `idle`. After `data` with `ani`
the last frame drawn (if any) goes at the end of that command. The comets'
trails are the core's, so a thinned frame loses nothing. A new `.ani` file
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
- the `size` command (a canvas size in pixels for windows 1..10, 101 and
  104): the core keeps its default window sizes;
- `hello`'s `char` (the font cell the ops laid text out on).

Everything the page shows comes from the data events: `series`, `plots`,
`nullclines`, `dfield`, `marks`, `diagram`, `autoinfo`, `autosettings`, `ani` `frame`,
`aplot`, `erase`, `redraw`. The `pixels` ask stays: web2 answers it from
those.
