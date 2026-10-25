# xppautX protocol

Clicked commands may use `{"cmd":"key","menu":"file","item":"savesession"}`
with a `menu` of `main`, `file` or `num` and an `item` from that menu's
the `id` of a row of `hello.command_table`. This form resolves the existing command independently
of the current letter-shortcut menu and returns to the main shortcut menu
after it finishes. Its kind is the target item's existing kind. Unknown
menus or items, and combinations with `key` or `win`, are rejected.
The optional `button` field describes the click for recording; recordings
preserve the stable command plus subsequent prompt-answer keys. Legacy
`key` commands and recordings continue to work.

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
sends `state` and `idle`. While a command runs the server can stop and
**ask** the client something (a menu, a prompt, a mouse click); it waits for
the matching `answer`, answers `state`, `browser` with `from` and `quit` at
once, and keeps any other command for after the one that asked. See
"Commands during a command" for what reaches a running computation, and
"Action kinds" for which commands those are.

## Startup

1. `hello`: protocol version (4), `about` (Help > About's lines, from core/xpp_about.cpp, the same lines the desktop window's own About box shows: an array of lines, each an array of parts `{"text":...}`, with `"url"` too when the part is a link (`https:` or `mailto:`); the first line's text is `xppautX <version>`; a blank line is `[]`), `quit` (File > Quit's question as the core asks it, W110: `question`, `recording` (the question while a recording is in progress), `choices` (`["Save session","Don't save"]`) and `keys` (`sd`), for a client that asks it itself while a command runs: `quit` with `save` below), window title, the three main menus
   (`menu_names`, `command_categories` and `command_table`: "The command
   table"), the windows' key layers (`windows`), every command's kind and
   whether it is a step (`commands`): see "Action kinds"; the limits the
   core keeps to (`limits`) and the windows' numbers (`window_ids`): see
   "Shared limits and ids"; the user's keymap (`keymap`: "Keymap").
2. `window` `create` for window 1, the main plot.
3. `state`, then `idle`.

A client that draws sends `data` next (see "The plot as data").

### Start screen (W232)

A program started with no file (the Start menu, the Dock, a launcher; a
folder instead of a file too) has a session with **no model**. `hello` then
carries `start`, and has no `window` or `state` after it (`idle` instead);
a `hello` of a loaded model has no `start`, so a client shows the start
screen exactly while `hello.start` is there. The core queues the same
`{"cmd":"open"}` that File > Open model sends, so the Open model dialog
(a `file` ask, title `Open model`, `wild` `*.ode *.odex *.snapx *.recx`) comes at
once; answering it with a file loads that model (a new `hello`), and
cancelling it leaves the start screen: no error, the program keeps running.

`start`: `{"path": the preferences.setx file ("" when there is no config folder),
"limit": how many models are kept, "recent": [{"path": absolute path,
"missing": the file is no longer there}, ...], "examples": {"folder": ...,
"names": [...], "reason": ...}}`, newest first. A recent model
that is missing is listed as missing, never dropped; opening it is an
ordinary `open` that fails with the file named. When `preferences.setx` cannot be
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
the reason. `{"cmd":"open","example":"lecar.odex"}` opens one: the name must be
in `names` (a path, `..`, a link or any other name is the error "is not one of the
bundled examples"), the example is copied into `examples/` of the config folder
(an installed folder is read-only and a model writes beside itself), asking the
Replace question of every save when a copy is there (declining ends the command
with no change), a message says where the copy is, and the copy is opened by the
`open` below. `file` is ignored when `example` is given.

Open model has one owner, `{"cmd":"open","file":path}` (empty: the dialog),
so the start screen's button, a recent entry, File > Open model, the desktop
window's native File menu and the OS's open-documents event after the start all
send it. A session with no model refuses every other command with the error
"No model is open: open one first (Open model)", ending in `idle` like any
command, except `quit` (no question: there is nothing to save), `abort`,
`answer`, `keymap`, `key` of File > Open model (`menu` `file`, `item`
`openmodel`) and `data` (accepted, nothing to send).
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
  `{"cmd":"continue","until":END}`, END the end the run was going to (a run
  going forward only, Continue's own condition): the run goes on from where
  it stopped, with differences. Nothing switches without them. web2 shows
  the action as a button on the error. An equilibrium or the adjoint a failed
  exact Jacobian stopped is the same error, with no action yet. W275: an
  AUTO run one stopped (`AUTO stopped: ...`, at the equation's line, with
  the continuation parameter's value) ends as a Stop ends it: the last
  converged point is stored as an end point (EP) with its label, and
  `autoinfo`'s `stop.why` is `jacobian`. Its action has the same label and
  the commands `{"cmd":"set","kind":"num","name":"jacobian","text":"difference"}`,
  `{"cmd":"auto","op":"restart","label":N}`, N the end point's label (no
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
and an initial-conditions file (`.ic`: `values` `read`, `--parfile`,
`--icfile`, Initialconds/File), a session file's members (`.snapx`, "Session
files"; the members are settings files too), and a recording (`.recx`, "Playing a recording"); a model's
internal set (File/Apply set, `applyset`) checks every item before it applies one,
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

## The command table

`hello.command_table` (W207, core/command_table.h) is the one list of the
commands of the main window's three menus, in the order the sidebar lists
them; the page's sidebar groups, command search and every command label are
made from it, and the desktop window's File menu takes its labels and
protocol lines from the same rows. Each row:

| field | meaning |
|---|---|
| `menu` | `main`, `file` or `num` (`hello.menu_names`): the legacy shortcut menu of the item, and the `menu` of its `key` command |
| `id` | the stable name: a `key` command's `item`, what recordings keep, what the page calls (`initialconds`, `savesession` ...); never renamed |
| `key` | the legacy one-letter key in `menu` (`"\u001b"` is Esc); `""` for a command the page runs itself (`run_initial`, `run_last`, `steady`, `keymapeditor`, W212): it has no key in any menu, no `legacy_keys`, and a `key` command naming it is refused (`PAGE_KEY`, core/command_table.h) |
| `label` | plain text for buttons, menus and search |
| `description` | one line |
| `kind` | `c`, `v`, `s`, `d` or `x`: what the command needs ("Action kinds") |
| `category` | an `id` of `hello.command_categories` |
| `pinnable` | whether it may sit in the quick-access toolbar (W212): false for Quit, the three shortcut-layer switches and the commands of `panels` |
| `primary` | listed in its group before a search; the others (Continue, Parameters and the Numerics values, under Run) appear only when searching |
| `default_keys` | keys that run it from the page (`"Ctrl+O"`: Ctrl, or Cmd on macOS; several keys separated by a space are a chord); Open model, Save session as and Reload model have one (W208), Undo value edit Ctrl+Z, Redo value edit Ctrl+Shift+Z and Ctrl+Y (W210; the page leaves them to a text field that has the focus); the page never binds Alt+F4, Ctrl+W, Ctrl+Q, F11, F12 or F5 |
| `legacy_keys` | the XPPAUT sequence: the shortcut layer's key, then the item's (`["F","S"]`, `["I"]`, `["U","Esc"]`); the page runs the command on it only when its preset "XPPAUT sequences" is on (off by default) |

`hello.command_categories` is [{`id`, `label`, `listed`, `expanded`}...] (`expanded`: the group is open before a search): `run`, `files`,
`analysis`, `plot`, `tools` (the sidebar's groups, in order, `listed`; Tools holds the Calculator),
`panels`, not listed (W229): the commands the page opens from a panel or the title bar rather than the
sidebar (`source` in the Model panel, `transpose` and `lookup` in the Data panel, `getparset` (Apply set), `restoredefaults`, `savevalues`, `savedefaults`, `exportvalues`, `renameset` and `deleteset` in the Values
panel, `keymapeditor` in Help, `record` and `play` in the title bar's Tools menu); they keep their rows for
their keys, `key` commands and recordings, and cannot be pinned; and
`layer`, not listed: the rows File, Numerics and Return to main shortcuts
only switch the legacy shortcut layer, which the page never enters (it
sends a command by its id) and never lists; the XPPAUT-sequences preset
types the layer's key as the first key of a chord. Every legacy menu item has exactly one row
(`tools/keycheck.py` fails a key a menu's handler takes with no row, or a row
no handler takes); the page shows a table that disagrees with itself
(`navigationProblems`) as an error, once.

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
browser page both read and write it through the one `keymap` command, so they
never disagree.

    {
      "preset": "default",
      "pinned": ["reload", "calculator"],
      "bindings": {
        "reload": ["Ctrl+B", "F S"],
        "savesession": []
      }
    }

- `preset`: `default`, or `xppaut` (the XPPAUT one-letter sequences, `legacy_keys` of the command table).
- `pinned`: the quick-access commands, in order; each a command whose `pinnable` is true, none twice.
- `bindings`: a command `id` of `hello.command_table` (not the three shortcut-layer rows) to its keys, which replace the table's `default_keys`; `[]` is no key. A command not named keeps the table's keys.

A key is one to two parts (a chord) separated by one space; a part is the
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
wrong type, a malformed key, a reserved key (any part of a chord), a command
with too many keys, two commands on one key (one key also clashes with another
that starts with it as a chord), a pinned command twice or not pinnable, an unknown preset.
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
| `preset`, `pinned` | the user's (`default` and `[]` when `ok` is false) |
| `reserved` | the keys no command may take |
| `limits` | `keys` (per command), `parts` (per key), `key_bytes` |
| `commands` | [{`id`, `keys`, `source`}...] for every command but the shortcut-layer rows: the effective keys, and whether they are the table's (`default`) or the user's (`user`) |

When the file is bad, the page gets that error in `keymap` (in `hello`, and in
the answer to `get`) and `commands` are the table's defaults with `source`
`default`: the defaults are what the page shows while the error is up, marked
by `ok: false`, never presented as the user's; no `message` event is sent for a
load (the page shows the error once, from `keymap`). `set` replaces a bad file
whole; `reset` removes it.

`set`'s `map` is the user map as the file holds it (`preset`, `pinned`,
`bindings`; a setting left out is its default). It is checked by the same
pipeline as the file, and an error is a `message` error whose `file` is
`the keymap sent` and whose line and `source` are in the `map` as sent (its
JSON text), the file untouched. The file is written through `xpp::Writer`
(a temporary file beside it, then a rename), one binding to a line.
A `set` or `reset` that cannot write or remove the file is a `message` error
naming it.

## Commands (client to server)

### Direct run controls (W193, W194)

`{"cmd":"steady","decimals":9,"hold":1,"maximum":50}` starts a new
trajectory from Initial. All fields are required JSON numbers; decimals
must be an integer 0–15, hold at least one positive Dt, maximum at least
hold, and durations/time/counts must be finite and fit the driver. Ordinary
trajectory output is required (no Poincare, histogram or FFT). Compare all
state doubles rounded to fixed decimal places at every configured Dt;
changed digits reset the hold. Store every N steps is temporarily 1, then restored
along with Total. `hello.steady` supplies `max_decimals`, `default_decimals`
and `default_hold`; the core owns these values.

`{"cmd":"continue","extra":10}`,
`{"cmd":"continue","until":40}` or `{"cmd":"continue"}` appends from the
current core state/time without a prompt (W213: the one Continue; the
legacy `C` prompt, "Continue until:", is gone). `extra` is a duration and
`until` an end time; both may not be given, each must be a JSON number, and
with neither the run continues for another Total (the Run duration), as the
key `c` and a user button's `c` do. The additional duration must be
positive, with finite times and counts. Requires a prior trajectory,
positive Dt and no histogram/FFT; numerical settings stay unchanged.
Continue honours Store every N steps (`store_every`): it stores one row per
N steps, so its output grid is N Dt (N times 1 for a map). The end, whether
given as `until` or as the time after `extra`, is rounded up to that grid
(never short of the end asked for; a value within 1e-6 of a grid interval of
a grid point counts as on it) and is at least one interval after the current
time; the end actually reached is the `time` of the `state` event that
follows. `hello.continue.grid_tolerance` carries that 1e-6 so the page shows
the end before the command runs. `continue` is a compute action (`kind: X`)
and a recording step with its raw command fields; its default key is
Alt+Enter (`default_keys` of the command table).

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
| `key` | `key`; `win`, `row` | A hotkey, exactly as typed in xppaut: one character, or `Escape`, `Enter`, `Tab`, `Backspace`, `Delete`, `Home`, `End`, `ArrowLeft/Right/Up/Down`, `PageUp`, `PageDown` (DOM `KeyboardEvent.key` names; X keysym names also work). Menu clicks are sent as the item's key. One letter is one command, and every key is in a menu (core/command_table.h and core/menus.cpp, the only places a key is defined; `tools/keycheck.py`). With `win` (`auto`, `browser`, `ani`, `aplot` or `equilibrium`) the key is one of that window's own layer ("Window keys" below) instead of the main window's; the browser's takes `row`, the selected row. A page's button sends its key's command: the buttons that have no key (below) are the only other commands. `button` (optional, any command's key) names the control the key came from (web2: Integrate, and every window button by its `hello.windows` id); only a recording reads it ("Recordings"). |
| `answer` | `id`, `ok` (0/1), plus the kind's fields | Reply to an `ask`. Omitting `ok` means ok. Omitting `id` answers whichever `ask` is currently pending (a client or the recording player can answer without tracking prompt ids). |
| `set` | `kind` (`par`, `ic`, `bc`, `delay`, `num`), `name` or `index`, `value` or `text` | Change a value (no redraw or run: W69 dropped the `rerun` flag this command used to take). A setting ("Action kinds"): sent during a computation it applies when that ends, never to the run in progress ("Commands during a command"). `name` is matched without regard to case, in full: a name has no length limit (W76) and every event carries it unshortened. `text` is what the X11 box takes: a number or `%formula` for `par` and `ic`, an expression for `bc` and `delay`. BCs and delays go by `index` (BC names all read `0=`). `num` sets a main numerics field by its key (`name`: `total_time`, `dt`, `method`, ...; "The numerics as data"). A formula that does not evaluate gives `message` `error`, a numerics value refused one naming the field (`Numerics: Dt must be a number other than 0`); so does what the command cannot take, nothing set (W116): a `kind` it does not have (`set takes kind par, ic, delay, bc or num, not "parm"`), a name the model does not have (`set: the model has no par nosuch`), an `index` outside the list, no `value` that is a number and no `text` (`set par iapp: its value is not a number (or its text missing)`). Several values in one command: `values` [{`kind`, `name` or `index`, `value` or `text`}...]: all or nothing (W131), every value is checked first (a kind, a name, an `index`, a number, a formula that compiles, a numerics value) and none is applied when one is refused, the one `message` `error` naming that value (`set par gca: ...`) and its `field`. A `par` or `ic` `text` is one number, all of it (`xpp::parse_number`: no `1O0`, no `5x`; blanks around it are allowed), or a `%formula`; anything else is refused (`set par gca: "abc" is not a number`), as are an answer to a number ask, a `slide` value and an `internset` value that is not a number. web2 sends every edit (a value field, a slider, Reset, a numerics field) at once, busy or idle (W106). |
| `applyset` | `set`, `scope`, `win` | **Apply set** (W319, File > Apply set…, the Values panel's Named sets): the model's set `set` (the name as the model spells it) applied as a patch over the current values, limited to `scope`: `all`, `parameters`, `initial` (the states) or one of the groups a set holds (`integration`, `poincare`, `equilibria`, `continuation`, `plot`, `sweep`, `analysis`; docs/odex.md). `win` is the target plot window, numbered as the `plots` event does (1 is the first; the active one when omitted), and only a scope of `all` or `plot` takes one; `continuation` is the next run's, a finished run keeps what it used. A parameter-only apply leaves the initial values alone: `init` formulas are not evaluated again. All or nothing: a set the model lacks, a scope it is not, a window that is not open, `win` with another scope, or an entry whose value is refused is a `message` `error` (at the set's line in the model for an entry, otherwise at the command's place) and changes nothing. One undo step. The File menu's command (`getparset`) asks the set (menu `param_set`), the scope (`set_scope`, `all` first) and, with several windows open and a scope of `all` or `plot`, the window (`set_graph`), then runs this. A setting, as `set` is. |
| `restoredefaults` | `scope`, `field`, `win` | **Restore defaults** (W319, File > Restore defaults…, the Values panel's Reset all and each field's reset): the model's own values back, for `scope` (as `applyset`'s); `field` limits it to one parameter, state or setting of the scope (not with `all`). A setting returns to the built-in value with the model's `options` over it; `win` is the plot window as in `applyset`. All or nothing, one undo step, no run. The File menu's command (`restoredefaults`, key `k`) asks the scope (menu `restore_scope`) and the window as Apply set does, then runs this. A setting, as `set` is. |
| `saveset` | `set`, `mode` (`new`, `replace`), `scope`, `win`, `state` | **Save values into a set** (W320, File > Save values…, key `o`): the current values of `scope` (as `applyset`'s: `all`, `parameters`, `initial` or one group of settings; `win` the plot window as there) written into the model file. `new` appends `set NAME { ... }` at the end of the file (the name a valid name that is not a set of the model, an import's included); `replace` writes them in place of that set's part of the same kind (the parameters, the states, or one group of its `options:`), every other part of the set as it was written, and the set must be one the model file itself writes: a set an import brought is read-only and the error asks for a name of one's own. `state` is `initial` (the default) or `last` (the last state reached, for the initial values). The rest of the file keeps its text, comments, line ends and all. The new text is parsed and checked as a model first; the file on the disk must still be the one the model loaded (a file changed since is refused, saying so, and left alone); the replacement is atomic (a temp file, then a rename); only then the model's sets, defaults, options and places follow, so `applyset` finds the new set at once. A model opened from a session file (or played from a recording) has no file of its own: the session's copy of the model changes, and saving the session keeps it. All or nothing: any error is a `message` `error` (at the set's line, or the command's place) and neither the file nor the model changes. A data command ("Action kinds"): refused during a computation. |
| `savedefaults` | `scope`, `win`, `state` | **Save as defaults** (W320, File > Save as defaults…, key `j`): the current values of `scope` written as the model's own, where the model keeps them: the values that differ from the defaults replace the numbers of the `par` and `init` lines where they stand (an `init` formula becomes the number; a state with no `init` line gets one after the last), and a group of settings replaces its entry in the `options` block (the block, or its group, is added when there is none). Restore defaults uses them at once. A default declared by an import or by an array statement cannot be rewritten in this file: the error names where it is declared. Written as `saveset` is. |
| `exportset` | `set`, `scope`, `file`, `win`, `state` | **Export values** (W320, File > Export values…, key `i`): the current values of `scope` as the set `set` of the `.odex` file `file`, which gets the first line `# Saved from <model> at <time>` when it is new; in a file that exists the set is added, and a set of that name there asks (a `choice` ask: the replace question) before its part of that kind is replaced. The file is written by the save dialogs' rules (a base name, no link, a temp file then a rename); the model file itself is refused (`saveset` writes it). The file is checked for its syntax, not against names it does not declare: a file of sets is for a model to load. |
| `renameset` | `set`, `to` | **Rename set** (W320, File > Rename set…, key `z`): the model file's own set renamed in the file and in the model. The new name is checked like a new set's, and refused when it is a set of the model; an imported set is read-only. Written as `saveset` is. |
| `deleteset` | `set` | **Delete set** (W320, File > Delete set…, key `f`): the set and its line removed from the model file. An imported set is read-only. Written as `saveset` is. |
| `slider` | `slot`, `name`, `lo`, `hi`, `step` | Set a Session slider definition. Zero-based `slot` is an existing slot or the next slot (append); empty `name` clears it. Name must be a parameter or initial condition; finite low < high, and step from 0 through high-low are required (0 chooses an automatic page step). A state event confirms the setting; it runs nothing. Open model replaces the definitions with that model's `@ s1/slo1/shi1` options. |
| `slide` | `name`, `value` | A parameter slider moved: set the parameter or variable, no run (the `rerun` flag went with `set`'s, W69; web2 sends a slider's values as `set`). A setting, as `set` is. A name the model does not have, or a `value` that is not a number, is a `message` `error` and nothing changes. |
| `userbut` | `index` | An `@ button` of the ODE file (`hello.userbuttons`). |
| `plotvars` | `how` (0 x vs t, 1 phase plane, 2 array plot), `names` | The IC box's xvst/pp/arry buttons for the checked variables. |
| `browser` | `from`, `count`, `col`, `ncol` | The data browser block the client shows (answered at once with `browser`, even during a prompt); `count` 0 stops the updates. |
| `browser` | `op` (`write`, `load`, `postprocess`); for `write` `what`, `format`, `name`, `replace`; for `load` `format`, `name` | Save data and Load with their choices given, each skipping its question (see "Saving data" below); the same commands by the keys `w` and `l` of the browser window ask them all. `postprocess` runs the model's `@ postprocess` (a histogram, a Fourier transform, ... of the data, core/histogram.cpp) and shows the result in the browser, as `--silent` does after its run. The other buttons are the window's keys ("Window keys"). |
| `values` | `op` (`write`, `read`), `kind` (`par`, `ic`), `name` | The values panel's Save and Load of XPP's own file for a section (core/lunch-new.cpp `write_parameter_file`/`load_parameter_file_named` and the `ic` ones, W66), a Load all or nothing ("Our files: all or nothing"): `write` is Save, `read` is Load; `name` given skips the file ask (as `browser`'s `write` does), omitted or empty asks for one (`ask` kind `file`, like Save/Load data). `write` fails (`message` `error`) for a `kind` other than `par` or `ic`. `op` `query` (no `kind`) writes `name` with the model's internal sets (name, whether `--silent` runs it, what it sets), parameters (their values in the file) and initial conditions, each asked for by `sets`, `pars`, `ics` (1), under its `#` heading: `--silent`'s `--qsets`, `--qpars`, `--qics`. `read` and `internset` are settings (they set values), `write` and `query` data (they write a file). |
| `dfield` | `op` `write`, `name` | Write the direction field the current plot window shows (Dir.field, key `d`) to `name`, one arrow a line (x, y and the arrow's end), its lengths in PostScript's frame whatever the window's size: `--silent`'s `--dfdraw 4`/`5` file, dirfields.dat. A window that shows none is a `message` `error`. |
| `equilibrium` | `op` `write`, `name`, `shoot` | Find the equilibrium Newton reaches from the initial conditions and write it to `name`, one variable a line (its value, then its eigenvalue's real and imaginary parts); nothing when Newton does not converge. `shoot` 1 also integrates a saddle's invariant manifolds into `UMk.dat`/`SMk.dat`: `--silent`'s `--equil 0`/`1`, equil.dat. |
| `equations` | | Send `equations`. |
| `data` | `events` (names from `hello.features`), `enc` | The data events the client wants from now on (`[]` stops them); each is sent at the end of this command. `series`: the plot windows' curves as numbers, `plots`: the plot windows themselves, `nullclines` and `dfield`: what the phase planes show besides their curves, `marks`: equilibria, text, arrows, markers and frozen curves on the plots (all in "The plot as data", below); `ani`: the animation's frames ("The animation as data"); `autoinfo`: AUTO's info strip and stability circle ("The AUTO diagram as data"); `autosettings`: AUTO's Numerics, parameters, axes and Mark values ("AUTO's settings as data"); `numerics`: the main numerics ("The numerics as data"); `player`: the open recording's complete `player` event, sent at once and again whenever it changes, so a page that connects after the file opened still gets its steps and notes ("Recording startup and all-state inspection"). `enc` `"f32"` sends these events' value arrays as base64 of little-endian float32 instead of JSON numbers (an `ani` frame is always JSON). |
| `action` | `index` | Run the action of comment `index` of `source.comments`. |
| `click` | `win` | The user selected plot window `win`. |
| `display` | `win`; `x`, `y` (`[low, high]` or `null`), `runs` (bool), `freeze` (bool) | What the page displays of plot window `win` (W65, "Display state" below): the zoom shown on each axis given (`null`: the window's own), whether its earlier runs are drawn (`runs`) and whether a replaced trajectory is kept as an earlier run (`freeze`, default true; false: a new run drops the earlier runs instead of keeping the replaced one). A key left out stays as it is. A view (see "Action kinds"): sent during a computation it runs after it (the page holds its changes for the idle anyway, to send only the last). A range whose low is not below its high, or a window that does not exist, is a `message` `error` and nothing changes. The values come back in `plots`. |
| `text` | `op` (`add`, `edit`, `delete`), `win`, `id` (`edit`, `delete`), `text`, `size` 0-4, `style` 0-3, `color` (a colour, see "Colours"), `x`, `y` | The text labels of plot window `win` (W227, #281), which the page's text dialog drives. `add` needs `text`, `x` and `y` (plot coordinates, the position of the text's baseline start); `size` (default 2), `style` (0) and `color` (`foreground`) default. `edit` changes the label `id` of that window (the `id` of its `marks` `text`): a key left out stays, and `x`, `y` move it. `delete` removes it. A text starting with `%` is stored as its rest with each `\{expr}` filled in once, as XPPAUT's Text dialog did. A view ("Action kinds"). All or nothing: a window or `id` that does not exist, a text that is empty or longer than `hello.text.max_length`, a `size` or `style` outside `hello.text`'s range, a `color` that is no colour (`text: the color is a string: #rrggbb, ...`), a value that is not a whole number or a position that is not a finite number is a `message` `error` (`text: 9 is not a text size (0 to 4)`) and nothing changes; so is an `add` when the session already has 50 texts. The label shows in `marks` at the end of the command. |
| `redraw` | | Redraw the active plot window, and the AUTO diagram when AUTO is open (for a client that reconnects). |
| `state` | | Send `state` now. |
| `auto` | `op`: `grab` (`label`, or `type`+`index`), `display` (`view`, `x`, `y`, `show`), `view` (`new`, `close` or `active`), `close`, `point` (`x`, `y`, or `xd`, `yd`), `set` (`numerics`, `axes`, `marks`, `threads`) | What the AUTO window's keys ("Window keys") do not say. `set` writes AUTO's settings without the forms (see "AUTO's settings as data"); `point` is a click on the diagram at pixel `x`, `y` of window 101 or at `xd`, `yd` in the diagram's quantities (shows its coordinates, and in a two-parameter plot stores them for AUTO's File/sElect 2par pt (`e`), which sets the two parameters to them); a run (AUTO's key `r`) AUTO cannot compute (the Numerics form's Ncol above 7 or Ntst 0, a singular Newton step) ends with a `message` `error` beginning `AUTO stopped:` and the diagram so far saved, as a cancel leaves it; the session goes on (W63a); `display` sets what the page displays of the diagram (below): `x`, `y` a view's zoom, `show` whether the branches hidden by Clear are drawn; `view` adds, closes or activates a view of the diagram ("Views of the diagram" below); `grab` grabs that stored point directly, with no ask ("Grab by label" below), a label or a type and index being required (the interactive grab is the key `g`); `close` destroys window 101, File/Auto opens it again. |
| `session` | `op` (`save`, `load`), `name`, `data` (save) | Save or open a session file, `name.snapx` (`.snapx` added unless `name` ends so; "Session files" below): File/opeN session (key `n` of the File menu) and `save`. Without `name`, `save` asks for one (`ask` kind `file`, wildcard `*.snapx`) and, as every save with no decision, before replacing a file that exists. The File menu's two save commands are the table's `savesession` (key `v`, Ctrl+S: saves to `state.session.file` with no ask and no replace question, and behaves as `savesessionas` when there is none) and `savesessionas` (key `w`, Ctrl+Shift+S: always asks, as `save` with no `name`), W209. A third, `savesessioncopy` (key `b`, no default key, W218), asks as `savesessionas` does and writes the file but leaves `state.session` and `state.changed` as they were. Reload keeps `state.session`'s file. `save`'s `data`: `true` puts the data table in, `false` leaves it out; without it the table goes in unless it is above 50 MB, when a `choice` ask (keys `l` leave it out, `s` save it) decides. `load` of a session file is `open` of it (below: its saved model, then the session). `state.session` (below) names the file the current session was last saved to or opened from. |
| `open` | `file` | Load another model in place of this one (File > Open model). Without `file`, asks with wildcard `*.ode* *.snapx *.recx`. A missing file, an invalid session or a session without its model fails at once. Then asks `choice`, keys `sd`: Save session (`s`) or Don't save (`d`); a cancel keeps the session. A `.snapx` loads its saved model from its own members, then restores its whole session, even when the same model is open. A `.recx` opens in the player and starts automatically. The opened file's folder becomes the working directory; nothing is extracted beside a session. `hello.title` identifies its saved model, e.g. `lecar.odex (saved in lecar.snapx)`. A binary file or a model that does not load is refused with its place shown and the previous session unchanged. On success, all windows but the main one are destroyed and a new `hello`, main window, `state` and first-start events follow; the client subscribes with `data` again. |
| `reload` | | Read the model's file again with the command line and in the folder it was loaded with (File/rEload, key `e` of the File menu; the window's File > Reload), as `open` does, asking first as it does (W59d: `choice` `sd`, `Reload NAME? Its values are kept by name; this model's data and diagram go. Save its session first?`): the parameters, initial data (a delay equation's history text too) and numerics keep the session's values by name (the Poincare section's variable by its name), a name the file no longer has is dropped, a new one comes with the file's value; `hello`'s `defaults` are the file's. A file that does not load changes nothing, as for `open`. |
| `record` | `op`: `start`, `stop` (`name`), `note` (`text`) | Record the session's steps into a `.recx` file ("Recordings" below): File/recorD, key `d` of the File menu, starts a recording or stops it. `start` begins one, with the session as it is now (its `@snapshot`, W59d; an error when one runs, or when the model was not read from files); `stop` writes `name.recx` (`.recx` added unless `name` ends so; without `name`, asks for one as the other File saves do: `ask` kind `file`, wildcard `*.recx`, the model's name offered; a cancel or a refused overwrite keeps recording) and ends it; `note` sets the note for the next step (replacing one set before; an error when not recording). `start` and `stop` are data, `note` control ("Action kinds"). `state.recording` says one runs. |
| `play` | `op`: `open` (`file`, `autoplay`), `start`, `pause`, `step`, `speed` (`speed`), `from` (`step`, `play`), `note` (`step`, `text`), `close` | Play a recording ("Playing a recording" below). `open` loads the `.recx` `file` (without one, asks for it: `ask` kind `file`, wildcard `*.recx`; then, as File > Open model does, whether to save this model's session first) and its model, then starts playback (set `autoplay:false` to open paused at step 0); File/plaY recording (key `y` of the File menu) and Open model of a `.recx` do the same. `start` plays (at the end, nothing), `pause` pauses (a wait in progress keeps what is left of it), `step` plays the next step, or the rest of the one running, then pauses; `speed` divides every pace by `speed` (0.25 to 8; the page offers 0.5, 1, 2, 4); these four act at once, even during a step or a computation, and have no `idle` of their own then. `from` loads the model again and runs steps 0 to `step` - 1 with no pace (their `press` events 0 ms), then pauses there (plays on with `play` 1); `step` 0 is Restart. `note` writes `text` as step `step`'s note into the `.recx` (its fingerprint kept: "Recordings") and sends `player` again. `close` leaves the player; the model stays. `open`, `from` and `note` are data, the rest control. |
| `aplot` | `op`: `scroll` (`dy` pixels), `close` | Dragging the array plot scrolls through time; `close` destroys its window. Its other buttons are the window's keys. |
| `colormap` | `name` | The session's colour scale (one of `hello.colormaps`), as Graphic stuff's Colormap does by picking one: the array plot sends itself again with the new ramp, and the pictures, the colour-coded orbit and the animation use it. A setting ("Action kinds"). |
| `ani` | `op`: `pause`, `fast`, `slow`, `speed` (`ms`), `step` (`n`), `seek` (`pos`), `mouse` (`what` down/move/up, `x`, `y` or `u`, `v`), `close` | What the animation window's keys ("Window keys") do not say: the ones that carry a number, steer a playing Go (`pause`, `fast`, `slow` and `speed` sent while it plays reach its loop) or drag. `speed` sets the delay between two frames of `go` to `ms` (0..1000; `fast` and `slow` change it by 2 within 0..100). `step` moves `n` rows from the core's position (`ani` `pos`), `seek` goes to row `pos`. `mouse` drags a grab point after the grab key, at pixel `x`, `y`, or at `u`, `v` in the animation's unit coordinates (those of the `ani` `frame` event, y up). |
| `steady` | `decimals`, `hold`, `maximum` | One-click unchanged-digits run; see Direct run controls above. |
| `continue` | exactly one of `extra`, `until` | Append directly using current core time; see Direct run controls above. |
| `abort` | none | Stop the running command's computation, at once (see below). No reply of its own: the stopped command ends with `stopped`, `state` and `idle`; outside a command it does nothing. Recorded interruptions belong to a recording step's `abort`, whose position the player arms before the computation. |
| `file` | `op` (`list`, `get`, `put`), `name`, `data` | The model's folder (the working directory) for a client that cannot reach it: `put` writes `data` (base64, at most 64 MB decoded) as `name`, `get` reads `name` back, `list` lists the folder. Answered with a `file` event, then `state` and `idle`. Names are base names only (see "Files" below). |
| `keymap` | `op` (`get`, `set`, `reset`); `map` (`set`) | The user's keymap, `keymap.json` in the per-user config folder ("Keymap"). `get` reads the file, `set` replaces the whole user map with `map` (checked as a file is, whole, then written), `reset` removes the file. Answered with a `keymap` event, then `state` and `idle`; a map that is refused or cannot be written is a `message` error and nothing changes. The user's settings, not the model's: a control command, taken any time, never a recording's step. |
| `quit` | `ask`, `save` | Exit, at once even during a computation, asking nothing: a script's and a client's quit (`--silent`, servercheck). With `ask` `true` (W59d), the user's quit: the desktop page sends it when the window's File > Quit or close box comes while the core is idle (web2 asks itself while a command runs, below). It stops a computation in progress (as `abort`; the command ends with `stopped`, `state` and `idle`), cancels a question open at the time (its command ends), then runs as a command of its own: the `choice` ask "Quit xppautX? Save this session first?" (`keys` `sd`: `s` Save session, `d` Don't save; a cancel keeps the session), as File/Quit (keys `f` `q`) asks. `s` saves the session first, as the `savesession` command does (to `state.session.file` with no ask when there is one, else a `file` ask, `*.snapx`), and a recording in progress after it, as `record` `stop` with no `name` does (the question then says so); then, as for `d`, `bye` and the exit. A cancel of any of these questions keeps the session. A plain `quit` sent while it asks still exits at once. With `save` `true` (W110), that question answered **Save session** where the client asked it itself: the desktop page asks it while a command runs (the window's close box never stops a computation), worded by `hello`'s `quit`. It stops a computation in progress as `ask` does, then saves as `s` does (the session's `file` ask, then a recording's) and says `bye` and exits; a cancelled save keeps the session (its computation already stopped). The page's **Don't save** is the plain `quit` (in the window: closing it, which sends it). |

## Action kinds

Every action has a kind (W95), defined once in the core and sent in
`hello`, so a client knows what it may still do while a computation runs
(see "Commands during a command"). A kind is one letter:

| kind | letter | what | during a computation |
|---|---|---|---|
| control | `c` | Abort, Quit, an answer | acted on |
| view | `v` | only changes what is shown: a zoom, a window picked, the data subscription, a menu that shows, Help | kept, and run after the computation |
| setting | `s` | a value the next computation uses (W106): a parameter, an initial or boundary condition, a delay, the numerics (`set`, `slide`, `applyset`, `restoredefaults`, `values` `read`, the Parameters key, the Numerics menu's items that ask a value, File/Apply set, File/Restore defaults), AUTO's Parameter, Numerics and Mark values (`auto` `set`, the AUTO window's keys `p`, `n`, `u`) | taken at once, applied when the computation ends: the run in progress keeps the values it started with |
| data | `d` | saves and loads: Save/Load values' files, Import settings, Save values into the model file (`saveset`, `savedefaults`, `exportset`, `renameset`, `deleteset`), every file written, a session, AUTO's diagram files, AUTO's Grab (the curve is still changing) | refused |
| computation | `x` | starts one: Initialconds, Continue, Range, AUTO's Run, Nullclines, Dir.field and Flow, Sing pts, Stochastic, a user button | refused |

- Each row of `hello.command_table` has a `kind` (one letter) ("The command
  table", core/command_table.h). An item that
  opens a pop-up menu has the least restrictive kind among that menu's
  items, so a menu opens when any of its items could run (Nullcline,
  Dir.field, Kinescope and Graphic stuff are views, run once a
  computation ends; stocHast and Averaging data; the Numerics items that
  ask a value settings, their dialog opening once the computation ends); every
  pop-up menu's items have their kinds in core/menus.cpp (checked when
  it compiles), for the core itself.
- `hello.windows` is the windows' key layers ("Window keys" below), by
  `win`: {`items`, `keys`, `kinds`, `ids`, `hints`}, `ids` the page's name
  for each item (`run`, `grab`, `write`, `go`, ...). A client takes a
  window's keys from here; web2 has no copy of them.
- `hello.menu_names` is `["main","file","num"]`: each menu's name,
  indexed by `state`'s `menu` number; the page's name for each item is its
  row's `id` in `hello.command_table` (below). A client looks a
  main-window key up by its id; web2 has no copy of the letters.
- `hello.commands` is every command, [{`cmd`, `op`, `kind`, `step`}...]:
  an entry with `op` is for that op, the entry without for the command's
  other lines (`browser` with `from` is a view, its `write` data); `key`'s
  `kind` is `""` (a key's is its menu item's). `step` says whether the
  command is a step of the user's: what a recording records ("Recordings")
  and what web2 runs again when the file it could not read has been added
  (`message` `file`); the client's own requests (`state`, `data`,
  `equations`, `redraw`, `file`, the browser's paging), answers and
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
- `hello.text`: what a plot text takes (core/grobs.h `LABEL_*`): `max_length` (1000 characters), `size_max` (4), `style_count` (4) and `default_size` (2).
- `hello.line_styles` (W492, #556): how a curve's style is drawn, decided once in the core (core/picture.cpp): `dash` the pattern of each dash word at a width of 1 (a wider line multiplies it by its width, at least 1; empty: solid) and `markers` the glyph of each marker word at a size of 1 (a size s multiplies it by max(s, 0.5)): `circle` the radius of a filled circle (0 for none), `polygon` the corners of a filled polygon, `strokes` its line segments, in pixels with y down. The page draws a curve's markers and dashes from it as the PDF and SVG do.
- `hello.colormaps`: the colour scales the core has, by name (the order of core/colormap.h `colormap_names`); the `colormap` command changes the session's.
- `hello.window_ids`: the window numbers of `window` events and asks:
  the AUTO diagram is `auto` (101), the animation `ani` (104), the array
  plot `aplot` (105). `plots` is the initial number of plot slots, not a
  maximum. The `plots.windows` event lists actual open plot IDs. Positive
  plot IDs skip the reserved auxiliary range 101 through 105. A command's
  `win` must be a whole ID of an open plot. Nonpositive, fractional or
  unrepresentable IDs report `win is a whole number, a positive plot window ID`;
  an ID not held by an open plot reports `window <id> is not an open plot window`.
  Both are placed errors prefixed with the command's name. Curves, frozen curves and plot windows have no fixed
  count limit (W376, #438); the page uses the received lists.

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
`only` list when it has one, else every column: what `--silent` writes as
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
format's extension. Before the name is asked, an empty selection reports
one error, `Nothing to save`. An unknown `what` or `format` is an error message.

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

The browser's `load` asks for `name` (a `file` ask, `wild` `*`) when it is
not given and reads it as `format`, or else as the format of its extension,
or else as `.dat`; the file's columns fill the stored ones in order (a CSV's
header row skipped, a 2-D `.npy` giving one column per column), at most the
storage's rows.

## Commands during a command

Animator `mouse` releases that integrate are computations:
`hello.commands` advertises the `mouse` op as computation. Down, move and
releases that do not integrate remain views. A release that integrates is
recorded as a computing step. Animator speed and pause controls taken
during a job are retained in arrival order and replayed at checkpoints;
Pause ends Go at its recorded frame. Pause while idle is a view step with
nothing running to stop.


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
    it: a setting (W106: `set`, `slide`, `applyset`, `restoredefaults`, `values`
    `read`, `auto` `set`; "Action kinds"). The computation
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
    `abort`; the recording player starts its next step after the current
    command's `idle`.

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
  recording, where each ask takes the step's next recorded answer; an
  unexpected ask stops the player.
- `abort` never has an `idle` of its own, so a client can send it at any
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
  lines (`abort`, `quit`, the stop keys and the rest this list calls acted
  on at once) are never refused, so an `abort` sent after a flood is read and stops the run. Over
  HTTP the refusal is `503 Service Unavailable` with that same event as the
  body (`application/json`), the command's `n=` number is not used up, so
  sending it again with that number is taken; web2 shows the error text. A
  body of several lines is taken line by line up to the refused one.
  The program's own lines (a recording's next step, a model the system asks
  to open) are never refused.
  The longest input line is the byte bound, 64 MB (a `--server` line over it
  is discarded and answered by one `input` error); an HTTP body is capped at 1 MB.
- A command whose job was cancelled (by `abort`, Escape, `quit`) sends
  `stopped` before its `state` and `idle`: where the computation got to,
  which is what the recording player uses to replay the interruption (below).
  A plain `quit` read by the computation's own check ends the command at once: it sends
  `stopped` too, then `bye` and the exit (it never reaches `state` and `idle`), the same
  `stopped` as when the computation sees the cancel first and the `quit` runs after the command.

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

File/recorD (key `d` of the File menu) or `{"cmd":"record","op":"start"}`
starts recording the session (W59a, core/json_record.cpp; the file's
format is core/recx.h); File/recorD again or `record` `stop` writes it,
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
{"step":"Initialconds → Go","keys":["i","g"]}
{"step":"nUmerics → Total","keys":["t"],"answers":["1e7"]}
{"step":"Initialconds → Go","keys":["i","g"],"abort":{"what":"integrate","rows":2193,"t":219.2}}
{"step":"Zoom window 1","cmd":{"cmd":"display","win":1,"x":[0,50]},"view":true}

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
  `abort`: the `stopped` event's `at`, recorded in the step
  ("Playing a recording without an interface"). `during`: the keys the running job read itself, each
  `{"key":K,"at":AT}` with where the job was when it took it: `/` ending a
  range or a shooting, Escape stopping the animation's Go (an Escape
  during a computation cancels it: that is `abort`). Animator pause and
  speed controls use `{ "cmd": <command>, "at": AT }` in the same array,
  in arrival order, and replay at their checkpoint. `files`: the file
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
is. A recording that does not read (not one, a header line missing,
twice or not one of `program:`, `model:`, `recorded:`, no `@snapshot`, a
section with no `@end`, no `@steps`, a step that is not a JSON object
with `keys` or a `cmd`, no `fingerprint:` line or a line after it) is an
`error` message at its line (`file` the `.recx`, `line`, `source`: W125)
and nothing changes: every step is read before anything is played or
loaded. One whose
fingerprint does not match plays all the same: `player`'s `intact` is
false, and the page says it was changed after it was made.

Faithful: each step runs exactly as recorded, nothing fixed, skipped or
changed. The player sends the step's command (a `key` with its `win` and
`button`, or its `cmd` whole), then answers each question the command
asks with the recording's next answer of its kind: a `menu` or `choice`
ask the next of `keys` (`Escape`: a cancel), any other the next of
`answers` (a `string`'s value, a `form`'s values, a `file`'s or any other
answer's members; `null`: a cancel). A `save_replace` owner answer is
consumed by the save operation before checking the disk, without another
prompt or input; it does not delay arming a recorded interruption. An `alert` it answers OK; a
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
this machine lacks, stops there. `play` `from` loads the snapshot again.

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
`check_player_ani`) and `tools/autocheck.py` (`play`) play recordings
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
| the internal set | `applyset` with its `set` name and scope `all` (File/Apply set) |
| `--qsets`/`--qpars`/`--qics`, `-dryrun` | `values` `query` into the output file; nothing is run |
| the run | `key` `i`, `answer` `g` (Initialconds/Go), or `answer` `r` (Range) for `@ range=1` or `@ stoch=` |
| `@ postprocess=` | `browser` `postprocess` |
| unless each run of a range wrote its own `output.dat.N`: `@ stoch=1`/`2` | `key` `u`, `key` `h`, `answer` `m` (Mean) or `v` (Variance), `key` `Escape` |
| output.dat (`@ output=`, `--outfile`; not with `--noout`) | `browser` `write`, `what` `output`, `format` `dat`, `replace` 1 |
| `--mkplot` | `key` `g`, `answer` `p` then the picture parameters form (Colour, Axes font size) answered with its own values (or `answer` `v`, SVG, for `--plotfmt svg`), then `answer` `file` |

After a run, the core computes requested nullclines (`nullclines.draw`, `nullclines.file`; a `.ode`'s `ncdraw`)
and `dfdraw` phase-plane data
in every mode, before exports: `nullclines.file` (`ncdraw=2`: `nullclines.dat`) also writes the nullclines (a row a segment: `nullcline` 1 or 2, `x1`, `y1`, `x2`, `y2`, in the data format its extension names) and
`dfdraw=4`/`5` dirfields.dat through the normal writer rules in the current
working folder (W346). These are no longer extra script commands.
After every run the script sends `--equil 0`/`1` as `equilibrium` `write`
equil.dat (`shoot` 1 for `--equil 1`). The files the command line names
(`--setfile`, `--parfile`, `--icfile`, `--readset`, `--with`) are read by the
model's start in every mode, before the script.

## Events (server to client)

| ev | fields | meaning |
|---|---|---|
| `hello` | `protocol`, `features` (optional parts the server speaks: `series`, `plots`, `nullclines`, `dfield`, `marks`, `ani`, `autoinfo`, `autosettings`, `numerics`, `player`), `title`, `file`, `output_names` (`par`, `ic`, `csv`, `curves`), `menu_names`, `command_categories`, `command_table`, `windows` (AUTO's hints, once `auto_hints`, are `windows.auto.hints`), `commands`, `lists`, `userbuttons` [name...] | First event, and again after `open` or `reload` loaded a model in its place. `lists` are what a form field `*n` picks from: 0 T and every variable, 1 ODE variables, 2 parameters, 3 both, 4 markers, 5 methods (items like `2 Box` start with the number to enter); `line_styles` (above), `colormaps` (above). `defaults` {`pars`, `ics`}: the ODE file's values, one per entry of `state`'s `pars` and `ics` in their order (what `default` restores). |
| `window` | `op` (`create`, `select`, `destroy`), `win`, `w`, `h`, `title` | Plot windows, AUTO, animation and array plot, numbered as `hello.window_ids` says. `w`, `h` are the core's pixel size of the window: what the pixel fields of `state.view`, `state.auto` and pixel answers to asks refer to. |
| `diagram` | `op` (`axes`, `reset`, `add`, `views`), `view`, ... | The AUTO diagram as data, each view of it; see "The AUTO diagram as data". |
| `autoinfo` | `info`, `stab`, `stop`, `notes` | AUTO's info strip and stability circle, and why the last branch ended, as data, for a client that asked (`data`); see "The AUTO diagram as data". |
| `autosettings` | `numerics`, `pars`, `axes`, `marks` | AUTO's settings as data, for a client that asked (`data`); see "AUTO's settings as data". |
| `numerics` | `fields` [{`key`, `label`, `value`, `integer`, `unused`, `choices`, `unavailable`}...] | The main numerics as data, for a client that asked (`data`); see "The numerics as data". |
| `series` | `win`, `rows`, `three`, `enc`, `xlabel`, `ylabel`, `zlabel`, `curves`, `shift`, `columns`; or `op` `append`, `win`, `from`, `rows`, `enc`, `columns`; or `op` `end`, `win`, `rows`, `version` | A plot window's curves as numbers, one event per window, for a client that asked (`data`), and during an integration the active window's rows as they are stored; see "The plot as data". |
| `runs` | `win`, `enc`, `erased`, `clear`, `drop`, `add` | A plot window's earlier runs changed, for a client that asked for `series`; see "Display state". |
| `autoview` | `earlier`, `show`, `active`, `views` | AUTO's hidden branches, the active view and each view's zoom, for a client that asked for `autoinfo`; see "Display state". |
| `erase` | `win` | The Erase command blanked plot window `win`, for a client that asked for `series`: it shows nothing of that window's curves (nor the earlier runs it may keep) until the window's next series or `redraw` (the `runs` event carries the same as data). Only Erase sends it: a zoom also redraws the window, and keeps what a data client shows. See "The plot as data". |
| `redraw` | `win` | The Redraw command drew window `win` again, for a client that asked for `series`: it shows the window's current series again (no series follows: the data did not change), and no earlier runs. |
| `plots` | `active`, `windows` [{`win`, `title`, `three`, `xlo`, `xhi`, `ylo`, `yhi`, `box`, `theta`, `phi`, `persp`, `zplane`, `zview`, `curves`, `shift`, `zoom`, `runs`}...] | Every plot window and the active one, for a client that asked (`data`); see "The plot as data". |
| `nullclines` | `win`, `enc`, `xname`, `yname`, `xcolor`, `ycolor`, `x`, `y`, `frozen` [{`x`,`y`}...] | A plot window's nullclines as segments in plot coordinates, for a client that asked (`data`); see "The plot as data". |
| `dfield` | `win`, `enc`, `scaled`, `color`, `n`, `du`, `dv`, `grid`, `speed`, `flows` [{`color`,`x`,`y`}...] | A plot window's direction field and Flow trajectories, for a client that asked (`data`); see "The plot as data". |
| `marks` | `win`, `enc`, `equilibria`, `text`, `arrows`, `markers`, `frozen` | A plot window's equilibria, text, arrows, markers and frozen curves, for a client that asked (`data`); see "The plot as data". |
| `state` | `sliders` [{`name`,`lo`,`hi`,`step`}...] in slot order (empty names retain vacant slots), `pars` [[name,value]...], `ics` [[name,value]...], `now` [value...] (after a first run), `rates` [number or null...] (one per `ics` entry: the largest absolute dv/dt over the last `hello.state_inspection.tail_intervals` stored intervals, null when it cannot be told: "Recording startup and all-state inspection"), `bcs` [[name,text]...] (only when the model itself defines boundary conditions, `b`/`bndry` lines or `boundary` statements: then one per variable, those it left out being 0; a model with none sends no `bcs`, the page shows no boundary-conditions section), `delays` [[name,text]...] (delay equations only), `view` {`win`,`left`,`right`,`top`,`bottom`,`xlo`,`xhi`,`ylo`,`yhi`,`three`, and `theta`,`phi` when `three`}, `auto` {`x0`,`y0`,`wid`,`hgt`,`xmin`,`xmax`,`ymin`,`ymax`} (AUTO open), `rows`, `menu`, `win`, `seed` (after a run that used one), `session` {`file`}, `changed`, `can_undo`, `can_redo`, `recording` {`steps`, `note`} (while recording), `player` {`step`, `running`, `playing`, `speed`, `fast`, `intact`} (while a recording is open in the player) | Current values; `view` maps pixels of the active window to plot coordinates (x = xlo + (xhi-xlo)(px-left)/(right-left), y likewise with bottom/top) and `auto` those of the AUTO diagram, for a readout under the mouse; `theta`, `phi` are the active window's 3D angles (degrees, meaningful when `three`), so a client that turned a 3D plot itself (`view3d`) can confirm the core agrees; `rows` is the number of stored time points, `menu` the active main menu (0 main, 1 file, 2 numerics: `hello.menu_names` names them), `now` the current state, one value per `ics` entry: where the last run ended or stopped (what Initialconds/Last and `set` `from` `last` copy into the ICs), `win` the active window; `seed` is the seed the last run actually used (each Go, do_range sweep -- Stochastic > Compute's many runs included -- or `--silent` picks and logs one, W71's "a seed per run"; absent before any run in this session); setting the numerics' seed to it and Go reproduces that run's data byte for byte; `session` is `{"file": ...}`, the session file last saved or opened (a path as it was given to `save`, the absolute path of one opened); absent before any; `changed` (W209, always sent) is whether the values differ from those the session was last saved, opened or loaded with: a parameter, an initial condition, the numerics' time span, method, tolerances and bounds, or the run's data (the rows stored and where the run ended); a view, window or AUTO diagram alone is not a change; a save (`session` `save`, Save session, Quit's save, the import of a set file) and the end of a load or of opening a session file make it `false` (the page shows the unsaved dot in its title); `can_undo` and `can_redo` say whether the table's `undo` and `redo` have a value edit to take back or put back ("Undo and redo of value edits" below); `recording` is there while a recording runs ("Recordings"): the steps recorded so far and the note set for the next one; `player` while a recording is open in the player ("Playing a recording"): `step` the next step to run (the number of steps at the end), `running` the step running (-1 between steps), `playing`, `speed`, `fast` (running steps with no pace to a `from` step), `intact` (the fingerprint matches). |
| `idle` | | The command finished. |
| `liveState` | `time`, `now` [value...], `rates` [number\|null...] | The solver's doubles for every ODE/Markov state at the latest output step of a running integration, paced at the input interval, with no subscription ("Live integration state (W200)"); `rates` as `state.rates` ("Recording startup and all-state inspection"). The final `state` follows. |
| `stopped` | `at` | The command's computation was cancelled; sent before its `state` and `idle`. `at` says where it stopped: `{"what":"integrate","rows":N,"t":T}` for an integration, N the rows in storage (as `state.rows`) and T the time of the last one stored (9 digits: the stored single-precision value exactly); `{"what":"auto","branch":B,"point":P}` for an AUTO run, P the last point it stored on branch B (the end point the cancel adds, as in the diagram's data); `{"what":"ani","frame":F}` for the animation's Go, F the frames it had shown (W59b); `{"what":"other"}` for anything else. The recording player replays the interruption from it (see "Playing a recording without an interface"). |
| `player` | `file`, `model`, `intact`, `steps` [{`note`, `step`, ...}] | A recording opened in the player ("Playing a recording"), sent when its model has loaded and again after a `play` `note`: the `.recx` (absolute path), its model's file, whether its fingerprint matches (`false`: it was changed after it was made; it plays all the same), and its steps, each its line's object ("Recordings": `step`, `keys`, `button`, `win`, `cmd`, `answers`, `view`, `abort`, `during`, `files`) with its `note` ("" for none). |
| `press` | `step`, `what`, `index`, `ms` | What the player sends next, before it sends it: `what` `key` (the step's `keys[index]`: its first key, the button's when the step has a `button`, or a menu's or a choice's answer), `answer` (`answers[index]`, a dialog's answer), `cmd` (the step's `cmd`) or `alert` (a message's OK); it goes `ms` milliseconds later (the pace at the speed set), so the page shows it first; 0 while running to a `from` step. A step's first `press` tells the page the step runs: the questions it asks until its `state` with `player.running` -1 are the player's to answer, never the page's (a `pixels` ask excepted). |
| `menu` | `which` | The main menu switched (0 main, 1 file, 2 numerics). |
| `title` | `text` | Title of the selected plot window: what it plots (`W vs V`). The server also labels unlabelled 2D axes with the plotted variables. |
| `message` | one of `error`, `bottom`, `box`, `auto`, `calc`; with `error`, `file`, `line`, `col`, `source` | Status text. Results (a fit outcome, mean, Liapunov exponent, saved BVP point, or AUTO toggle) use `bottom` (W133). `box` with empty text removes a hint box. An `error` carries its place ("Errors" above, W140): `file` with `line` 0 is a file the command could not read (W118), which web2 offers to add and run the command again. |
| `progress` | `n`, `of` | Computation progress, at most 10 a second. |
| `saved` | `saved` boolean, `file` | The save owner's result for this exact destination; only `true` authorizes delivery after the command's `idle` (W129). |
| `computing` | | The running command began computing (once per command, before its first `progress`); until its `idle` the server refuses data and computation commands ("Action kinds", "Commands during a command"). A page that connects meanwhile gets it again. |
| `equilibrium` | `type`, `cplus`, `cminus`, `rplus`, `rminus`, `im`, `values`, `eigenvalues` | Result of Sing pts. `eigenvalues`: the Jacobian's `[re,im]` pairs, one per variable; absent for a delay equation. |
| `source` | `lines`, `comments` [[text, has action]...] | File/Prt src. |
| `equations` | `lines` | One `dX/dT=...` line per equation. |
| `ani` | `pos`, `rows`, `fly`, `grab`, `skip`, `speed`, `loaded`, `open`; or `op` `frame`, ... | Animation state for its slider and toggles, sent with every frame drawn and after every `ani` command: `pos` the row the next step starts from, `speed` the ms between frames of Go, `loaded` 1 when an `.ani` file is loaded, `open` 1 while the animation window exists. With `op` `frame`: a frame as data, for a client that asked (`data`); see "The animation as data". |
| `aplot` | `title`, `nx`, `ny`, `values`, `enc`, `colormap`, `ramp`, `zmin`, `zmax`, `tlo`, `thi`, `tag` | The array plot (window 105; W492, #556 made it data only): `ny` rows of `nx` cells, row by row, `values` their numbers (as a series column; `null`/NaN off the stored rows and columns), `colormap` the session's colour scale by name and `ramp` its 256 colours from the low end to the high one: a cell is the ramp's colour at floor(256 * fraction), fraction being (value - `zmin`) / (`zmax` - `zmin`) clamped to 0..1 (core/colormap.h `ramp_color`); with `zmax` not above `zmin` no cell is coloured. `tlo` and `thi` are the times of its first and last row. `tag` the range-saving frame's text. Print writes the same cells and colours as a PDF. |
| `film` | `op` (`capture`, `reset`, `play`, `autoplay`), `count`, `win`, `cycles`, `delay` | Kinescope. The client keeps the frames: on `capture` it copies window `win` as it is drawn now; `play` shows them, `autoplay` plays `cycles` times `delay` ms apart. |
| `browser` | `rows`, `cols` (names, `T` first), `row0` (selected row), `start`, `end` (the First..Last range), `from`, `col`, `data` | Rows `from`.. as [T, column `col`, `col`+1, ...]; `null` for NaN. Sent for a `browser` block request and after any command that changed the data while the client shows the browser. |
| `ping` | | Beep. |
| `bye` | | The program is exiting normally: sent by every quit (a plain `quit`, during a computation or not, and the question's outcomes) before an error-free exit, so browser mode's `exit` event says `code` 0. Every quit path exits 1 after a reported error and sends no `bye` (W133). A crash or an error exit sends none (`exit` `code` 1). |
| `error` | `error`, `file`, `line`, `col`, `source` | The model did not load: sent instead of `hello`, then the program exits (see "A model that does not load"). |
| `file` | `op`, `name`, `ok`; `size`, `sha256` (`put`, `get`), `data` (`get`, base64), `files` (`list`: [{`name`,`size`,`mtime`,`sha256`}...]); `error`, `file`, `line`, `col`, `source`, `field` when `ok` is 0 (the common error shape above) | The answer to a `file` command (see "Files" below). |
| `keymap` | `op` (`get`, `set`, `reset`), `keymap` (the object of "Keymap") | The answer to a `keymap` command. |
| `ask` | `id`, `kind`, ... | See below. |

In browser mode (`xppautX model.ode`) events stream from `/events?t=TOKEN`.
Commands are POSTed to `/cmd?t=TOKEN`, one per request (a body of several
lines gives several commands). xppautX answers each connection on a thread
of its own, so a connection that sends nothing (a browser's preconnect) or a
stalled upload holds up no command, `abort` above all; two POSTs in flight at
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

The server sends the AUTO diagram's points (window 101) as data, so that
a client draws it and can zoom, pan and name the point under the mouse
without a round trip. The data describe exactly what XPP's diagram shows
(and what PDF and SVG export draw): after the `auto` op `clear` it
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
  the diagram was drawn again at these axes (`plot` is the view's
  projection, by name: `maximum`, `norm`, `maximum_and_minimum`, `period`,
  `two_parameter`, `frequency` or `average`; W491); the points are unchanged. Pixel `x0 + wid*(x-xmin)/(xmax-xmin)`,
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
| `lab` | [[index in the run, label, type (`EP`, `LP`, `HB`, `BP`, `PD`, `TR`, `UZ`, `MX`, `BT`, `CP`, `ZH`, `GH`, `R1`, `R2`, `R3`, `R4`, `LPD`, `LTR`, `PTR`, `TTR`)], ...] for the labelled points |

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
`{"cmd":"data","events":["autoinfo"]}`. It is sent at the end of that
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
 "stop":{"why":"parmax","text":"parameter iapp reached Par Max (0.5)","br":1,"pt":49,
         "value":0.5048396225954056,"limit":0.5},
 "notes":["Br 1 Pt 12: a Hopf bifurcation (HB) between points 12 and 13 was not located (not in ITMX=8 iterations); it is missing from the diagram"]}
```

| field | meaning |
|---|---|
| `info` | the point the strip shows: the one a grab's cursor is on (the strip changes only while grabbing); `null` before a grab, and in a new AUTO window |
| `info.point` | its index in the `diagram` data (the points the client holds, the same index in every view), -1 when they do not have it (after Clear, or a load not drawn yet) |
| `info.br`, `pt`, `type`, `sym`, `lab`, `f2` | branch and point number (positive, as in `diagram`), `type` as a run's `ty` (1 stable steady state .. 4 unstable periodic, with `stable` and `periodic` as a run's), the label's type (`EP`, `LP`, `HB`, ... or empty), the label (0 for none), and for a two-parameter point its curve kind as `f2` |
| `info.jac` | `difference` or `exact`: the Jacobian setting of the run that computed the point (as a run's `jac`) |
| `info.par` | the continuation parameter's `name` and `value`, and the second parameter's for a two-parameter point (the strip then shows both; for a one-parameter point it shows a blank name and 0) |
| `info.norm`, `var`, `u`, `per` | the norm, the variable of the Axes setting and its value, the period (AUTO's value for a steady state too: what the strip prints) |
| `info.x`, `y`, `y2` | where the active view plots the point, in the quantities of its axes (another view's are its own `diagram` data's point `info.point`) |
| `stab` | what the circle shows: the point AUTO computed last, or the grab's cursor; `null` before any |
| `stab.periodic` | 1: `circle` holds the Floquet multipliers of a periodic orbit; 0: e^λ of each eigenvalue λ of a steady state (XPP keeps them so: inside the unit circle is stable) |
| `stab.circle` | `[re,im]` per variable, the values themselves (the X11 circle clamps them to ±1.95). All `[0,0]`: not computed. Always a stored diagram point's own values, while AUTO runs and while grabbing alike (core/auto_stability.h). AUTO computes them from a run's second point on, so a run's first point, and a run that stops there, has none, unless the run restarts from a label of the same kind (a steady state from a steady label, a periodic orbit from a periodic one, one parameter): that first point is the label's solution and carries the label's values. A periodic run from a Hopf point, a two-parameter run and a period doubling's branch switch start with none; a two-parameter curve of periodic orbits (a limit point's, a period doubling's, a torus') has none at all, AUTO computing no multipliers along it. (XPPAUT stores the last values computed with every point, so its first points carry those of another point.) |
| `stop` | why the run's last branch ended; `null` until one ends, and again when a run starts or AUTO's window is new. AUTO labels the end EP (a limit, Max points, Stop, a Mark value) or MX (no convergence); this says which |
| `stop.why` | `parmin` / `parmax`: the continuation parameter went below Par Min (RL0) / above Par Max (RL1); `normmin` / `normmax`: the norm AUTO checks went below Norm Min (A0) / above Norm Max (A1); `npts`: the branch has Max points (NMX); `user`: Stop (an `abort`); `mark`: a Mark value set to stop (AUTO's UZR endpoint); `jacobian`: the exact Jacobian was not finite (W275: an error with an action follows); `noconv-min`: no convergence even at the smallest step (Dsmin); `noconv-fixed`: no convergence with a fixed step (IADS 0); `noconv-switch-min`, `noconv-switch-fixed`: the same while switching to a bifurcating branch; `noconv`: no convergence, how not noted. A no convergence's `text` names the Newton settings that gave up (`ITNW`, `EPSL`, `EPSU`) and, when the last linear solve was singular, its smallest pivot (W491); it is logged as a WARN where AUTO's Output is not shown |
| `stop.text` | the reason in words, to follow "Stopped: " (the page's status strip); AUTO's Output gets the line `Branch 1 stopped at point 49: parameter iapp reached Par Max (0.5)` for every branch that ends |
| `stop.br`, `pt` | the branch and point number of the end (positive, as in `diagram`) |
| `stop.value`, `limit` | what crossed the limit and the limit: the parameter and Par Min/Max, the norm and Norm Min/Max, the point count and NMX, the step size and Dsmin; `null` where there is none |
| `notes` | what the run lost or could not trust, in words (W491): a special point AUTO gave up locating (the corrector did not converge, its test does not apply, or not in ITMX iterations: the point is missing from the diagram), Floquet multipliers that are inaccurate (period doubling and torus detection are off until they are accurate again); `[]` for none, cleared when a run starts or AUTO's window is new. Each is also written in AUTO's Output (`Warning: ...`), or logged as a WARN where AUTO's Output is not shown |
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
tools/models/auto_stop.ode into each `stop` reason (Par Min and Max, Norm
Max, Max points, no convergence with a fixed and at the smallest step, and
Stop).

### AUTO's settings as data

`autosettings.special_points` is an object of named booleans `BT`, `CP`,
`ZH`, `GH`, `R1`, `R2`, `R3`, `R4`, `LPD`, `LTR`, `PTR`, `TTR`, all true by default. `auto` `set` accepts a partial object, e.g.
`{"cmd":"auto","op":"set","special_points":{"GH":false}}`. Set all twelve
false to disable all equilibrium, cycle and map codimension-two detection. Invalid names or
non-booleans refuse the whole transaction. Session settings require all twelve
lines (`BT true` or `BT false`, etc.); validation precedes application.

What the AUTO window's Numerics, Axes (the AutoPlot form and
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
 "axes":{"plot":"maximum_and_minimum","var":"V","par1":"iapp","par2":"phi","xmin":-0.2,"xmax":0.5,"ymin":-0.5,"ymax":0.4},
 "marks":[["iapp",0.25],["T",30]],
 "threads":"auto","threads_auto":4,"threads_max":16,
 "rules":{"ntst":{"label":"Ntst","integer":true,"min":1,"message":"Ntst must be a whole number of at least 1"},
          "ds":{"label":"Ds","nonzero":true,"message":"Ds must be a number other than 0"}, ...},
 "pairs":[{"lo":"dsmin","hi":"dsmax","strict":false,"message":"Dsmin must be at most Dsmax"}, ...],
 "step":{"key":"ds","lo":"dsmin","hi":"dsmax","message":"Ds must be from Dsmin to Dsmax in size (its sign is the direction)"}}
```

| field | meaning |
|---|---|
| `numerics` | the Numerics form's fields by AUTO's names, in the form's order: `ntst` Ntst, `nmx` Nmax, `npr` NPr, `ncol` Ncol, `ds`, `dsmin`, `dsmax`, `rl0` Par Min, `rl1` Par Max, `a0` Norm Min, `a1` Norm Max, `epsl`, `epsu`, `epss`, `iad`, `mxbf`, `iid`, `itmx`, `itnw`, `nwtn`, `iads`, `suppbp` SuppBP |
| `axes.plot` | the projection, by name: `maximum`, `norm`, `maximum_and_minimum`, `period`, `two_parameter`, `frequency`, `average` (a client labels it from its name; XPPAUT's numbers 0-4, 10, 11 are read only from an imported XPPAUT `.auto` file) |
| `axes.var` | the variable the y axis plots (Y-axis) |
| `axes.par1`, `par2` | Main Parm and Secnd Parm, two model parameters (`null` if none) |
| `axes.xmin` .. `ymax` | the diagram's axes |
| `axes` | all of it the active view's (see "Views of the diagram") |
| `marks` | Mark values: `[name, value]` per user point, where the parameter `name` (any model parameter) or the period `T` reaches `value` AUTO labels the point (UZ) |
| `threads` | AUTO's thread count, a program preference (W267) and not a setting of the model or the session: `"auto"` or a whole number from 1 to `threads_max`; the same for every model, read from `preferences.setx` (`auto_threads`) when a model is loaded |
| `threads_auto` | the count `"auto"` gives on this computer: its physical cores (the performance cores where the system says which), at most 4 |
| `threads_max` | the most `threads` may be: the computer's logical processors |
| `rules` | what each `numerics` value takes, by key (W118): `label` (the form's), `integer`, `positive`, `nonzero`, `min`, `max` (each only when it holds) and `message`, what a `set` of a value it does not take is refused with; the same table `set` checks with (core/auto_settings.cpp num_fields) |
| `pairs` | the values that must be in order: `lo` below `hi` (`strict`) or at most it, and the refusal's `message` |
| `step` | the first step `key` must be from `lo` to `hi` in size, whichever its sign, and the refusal's `message` |

Numbers are doubles in their shortest exact form, the whole-number fields
as integers; `null` when not finite.

The `data` command also accepts `diagram: true|false` (W439, #501). Diagrams
are delivered initially; omitting this field keeps that choice. `false` stops
diagram collection and delivery for this client, while AUTO still computes and
stores its complete numerical diagram. `true` replays the current diagram,
including points computed while delivery was off. This switch is independent
of `events`; `events: []` stops the listed data subscriptions. A non-boolean
`diagram` is one command error and changes no subscription.

**`{"cmd":"auto","op":"set", ...}`** writes them without the forms, with
any of the members above, each part only when given: `numerics` any
of its fields, `axes` any of `plot`, `var`, `par1`, `par2` (model parameter names), the four ranges, and `"fit":true` for
Axes/Fit afterwards, and `"view":k` for view k's axes (the active one's
without it; setting a view's axes makes it the active one), `marks` the
whole list (`[]` for none); mark storage grows with the session. Sent commands and
  session files have a 65,536-mark input allocation budget (W331, [#392](https://github.com/MuhammadMoustafa/xppautX/issues/392)).
  The legacy User action asks for a count, then a form with exactly that many values;
  cancel or one invalid value leaves the entire list unchanged.
The core checks every value first and sets all or nothing: a whole number
where the form's field is one; Ntst, Nmax, NPr, ITMX, ITNW, NWTN at least
1; Ncol 2 to 7; IID 0 to 5; IAD and IADS at least 0; SuppBP 0 or 1; Ds not
0; Dsmin, Dsmax and the EPS values above 0; Dsmin at most Dsmax, |Ds| from
Dsmin to Dsmax (T23; checked when one of the three is given), Par Min
below Par Max, Norm Min below Norm Max, Xmin below Xmax, Ymin below Ymax;
names of parameters and variables the model has. A refusal is a `message`
`error` naming the value: `AUTO settings: ` and the message the event's
`rules`, `pairs` or `step` gives it (`AUTO settings: Ncol must be a whole
number from 2 to 7`), so a client checks a form with the core's rules and
says what the core would.

`threads` (`"auto"` or a whole number from 1 to `threads_max`) is the thread
count (W267). It is checked with the rest, and saved by `prefs::update_prefs`
into the per-user `preferences.setx` (`auto_threads: auto` or
`auto_threads: 4`; `$XPP_CONFIG_DIR` names the folder) before anything of the
set is applied: a count outside the range, a word other than `"auto"`, or a
`preferences.setx` that cannot be read or written (it is never overwritten
while it is bad) refuses the whole set (`AUTO settings: AUTO's threads must be
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
`{"cmd":"data","events":["series","plots","nullclines","dfield","marks"]}` (any of
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
{"ev":"series","win":1,"rows":601,"three":0,"xlabel":"","ylabel":"","zlabel":"",
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
{"ev":"plots","active":2,"windows":[
 {"win":1,"title":"W vs V","three":0,"xlo":-0.6,"xhi":1.2,"ylo":-0.25,"yhi":1.2,
  "box":{"xmin":-0.6,"xmax":1.2,"ymin":-0.25,"ymax":1.2,"zmin":-12,"zmax":12},
  "theta":45,"phi":45,"persp":0,"zplane":-1000,"zview":1000,
  "curves":[{"x":1,"y":2,"z":1,"color":"foreground","draw":"line","dash":"solid","width":1,"marker":"circle","marker_size":3}],"shift":[0,0,0]},
 {"win":2,"title":"V vs T",...}]}
```

| field | meaning |
|---|---|
| `active` | the active window: the one keys, Viewaxes, Xi vs t, ... act on (`click` selects another) |
| `windows` | every plot window, by `win`; a window missing from the list was destroyed |
| `title` | what the window plots, `y vs x` (`z vs y vs x` in 3D), as its X11 title |
| `three` | 1 for a 3D plot |
| `xlo`, `xhi`, `ylo`, `yhi` | the window's axes (Viewaxes, Window/Zoom); in 3D the projected view's |
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
{"ev":"nullclines","win":1,"xname":"V","yname":"W","xcolor":"#f06400","ycolor":"#00e100",
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
{"ev":"dfield","win":1,"scaled":1,"color":"foreground","n":17,"du":0.1125,"dv":0.090625,
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
for every window once after `data`; the active window's first.

```
{"ev":"marks","win":1,
 "equilibria":[{"x":-0.1425351775676935,"y":0.034048992272573395,"type":"saddle","symbol":"triangle"}],
 "text":[{"x":-0.298305094,"y":0.498503745,"text":"\\1a\\0-point 6","size":3,"font":0}],
 "arrows":[{"kind":"pointer","x1":0.1,"y1":0.2,"x2":0.6,"y2":0.8,"size":0.2,"color":"#c8c800"}],
 "markers":[{"x":0.9,"y":0.1,"shape":"diamond","size":2,"color":"#00e100"}],
 "frozen":[{"key":"first run","name":"frz1","color":"#ffcd00","draw":"line","dash":"solid","width":1,"marker":"circle","marker_size":3,"x":[...],"y":[...]}]}
```

| field | meaning |
|---|---|
| `equilibria` | the points Sing pts marked (Monte Carlo's too), in the order marked, each once: `x`, `y` in plot coordinates (the window's x and y variables at the equilibrium, the doubles themselves: the `equilibrium` event's values for those variables), `type` what its symbol says (`stable`: no eigenvalue with a positive real part; `saddle`: some on each side; `unstable`: the rest) and `symbol` XPP's (`circle`, `triangle`, `box`). 2D windows only |
| `text` | Text,etc's labels: `id` the label's slot, which the `text` command names it by (it stays while the label lasts; a deleted label's is reused by the next), `x`, `y` where the text starts (its baseline, plot coordinates), `text` as drawn (`\{expr}` already replaced by its value), `source` as written (what the edit dialog starts from), `size` 0-4, `font` (1: all in the symbol font; 0 otherwise), `style` (0 normal, 1 bold, 2 italic, 3 bold italic) and `color` (a colour; the page draws style and colour, the PDF and SVG output the size and the colour, not the style). XPP's markup stays in the text: a backslash and `1` switches to the symbol font, where the Latin letters show Greek ones (`\1a` is α, Adobe Symbol encoding), `0` back to roman, `s` and `S` a subscript and a superscript one size smaller (they add up), `n` back to the baseline and size; any other character after a backslash is dropped. Bytes that are not UTF-8 are Latin-1 |
| `arrows` | Text,etc's arrows and pointers: `kind` `arrow` (only a head) or `pointer` (a head and a shaft), the head's tip at `x1`, `y1`, pointing from `x2`, `y2`; `size` the head's length as a fraction of that distance (half as wide as long); `color` a colour |
| `markers` | Text,etc's markers (Marker, marKers): `x`, `y`, `shape` (`box`, `diamond`, `triangle`, `plus`, `cross`, `circle`), `size` (1 is about 1% of the window), `color` a colour |
| `frozen` | the frozen curves of the window (2D ones): `key` (its legend text) and `name` (Freeze's Edit form), `color` its colour and its style (`draw`, `dash`, `width`, `marker`, `marker_size`, as a curve's), `x`, `y` the values of the curve's x and y columns when it was frozen (float32, as `series`: equal to that `series`' columns then) |

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
the rows stored so far instead and appends after it. A command that
appended ends, before its `state` and `idle`, with the rows stored since
the last append (one more append) and then

```
{"ev":"series","op":"end","win":1,"rows":20001,"version":7}
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
Reload carries over. Each of `set`, `slide`, `applyset`, `restoredefaults`, `values` `read` and `initialconds`' Last (Run from last state, which overwrites
the initial conditions) pushes the values as they were before it applies;
a push forgets what could be redone. The `key` commands `undo` and `redo`
(menu `main`, item `undo` / `redo`, kind `setting`: during a run they wait
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
| `auto/settings.txt`, `auto/diagram.csv`, `auto/solutions.s`, `auto/views.txt` | AUTO's settings and views; full-precision diagram and restart orbits when a diagram exists (xpp_session_auto.h); each diagram point's `jacobian` column is 0 (difference) or 1 (exact), the setting of the run that computed it, required: a `diagram.csv` without it is refused with its file and line (W278) |
| `windows.setx` | a `windows` list, every open plot slot as a group in index order (its zero-based `number`, boolean `open` and its `curves` by variable name with colour and style (`color`, `draw`, `dash`, `width`, `marker`, `marker_size`), the labels, the rotation, scales and box (`rm`, `xlo`, ...), the `zoom` and the earlier-runs toggle `previous_runs`), `active` (which window), an `auto_view` group (AUTO's hidden branches: `earlier`, `show_earlier`) and the browser's `added_columns` (name and formula) |
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
the player and starts playback automatically, as `{"cmd":"play","op":"open"}` does, without the question
File/open Model asks (nothing is open to save).

### AUTO diagram files

W339 ([#400](https://github.com/MuhammadMoustafa/xppautX/issues/400)) removes
the `autosettings.pars` and `auto set` `pars` fields; the latter is refused.
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
set by the `display` commands or by the runs themselves, and sent as data,
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
{"ev":"runs","win":1,"erased":0,"clear":0,"drop":0,"keep":0,"enc":"f32","add":[
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
1 with all the runs) after `data` asks for `series` again.

**`plots`** carries each window's `zoom` (`{"x":[low,high]|null,"y":...}`,
the part of its axes shown; `null` the window's own) and `runs` (1: the
earlier runs are drawn) and `freeze` (1, the default: a trajectory a new run
replaces is kept as an earlier run until Erase; 0: it is dropped, and only the
current run is drawn). Other axes or curves of the window (Viewaxes,
Window/Zoom, Fit, Xi vs t) drop the zoom. The `display` command sets them:
`{"cmd":"display","win":1,"x":[0,10],"y":null,"runs":false,"freeze":true}` (a missing key
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
| `menu` | `name`, `title`, `items`, `keys`, `hints`, `def` | `key` (empty or `ok:0` cancels); for an item past the end of `keys` (a menu with more items than keys: the named sets past the 26th, W229), `index`, the item's place in `items` |
| `choice` | `title`, `question`, `choices`, `keys` | `key` |
| `string` | `title`, `name`, `value`, `ok`, `cancel`, `kinds` | `value` (any length: no dialog cuts what is typed, W76) |
| `form` | `title`, `names`, `values`, `kinds` | `values` (same length, each of any length: W76 dropped `max`). A name starting with `*n` means the field picks from `hello.lists[n]`: a variable (`*0`), a parameter (`*2`), a marker (`*4`), ...; for a list whose items start with a number (`2 Box`) the value is that number. A form may also carry `columns` and `rows` (arrays of text), asking for a grid: its first `columns.length * rows.length` fields are the cells, row by row (field `r * columns.length + c` is column `c`, row `r`), and the others follow as plain fields. Viewaxes' 2D View (`X`, `Y`; `Variable`, `Min`, `Max`, `Label`) and 3D View (`X`, `Y`, `Z`, then `XLo`, `XHi`, `YLo`, `YHi` and the 3D params' ten fields) are the only grids (W225): the core reads every field whole before it sets any, and a variable that is not one, a number that is not one, a Min not below its Max (or XLo/YLo not below XHi/YHi) is a `message` error naming the field, with nothing applied. |
| `checklist` | `title`, `names`, `flags` | `flags` |
| `file` | `title`, `mode` (`read` or `write`), `file`, `wild`, `dir`, `save_dir`, `dirs`, `files` | `file` (a name in `dir`, or a full path anywhere, whole: the desktop window's own dialog answers with one, W88); a native save dialog also supplies `replace:1` and passes the ask `id` as `ask` to `__xppFileDialog`; the binding grants that exact path once for that prompt, so `replace:1` alone never authorizes an external write (W177, #229), while a browser save supplies no decision for the core's model-folder copy (W129); or `cd` (a folder name or `..`, in `dir`: only the folder listed changes, not the process's) or `wild` (a new pattern) to be asked again with that listing; `save_dir` is the writable model folder offered for a native save retry after permission denial; `dir` starts as the folder of the file of that `wild` last chosen, else the model's folder (a replay's recording's), never AUTO's or a replay's scratch folder (W151) |
| `text` | `title`, `value` (the text), `size`, `style`, `color` | `value`, `size`, `style` (whole numbers in the ranges of `hello.text`), `color` (a colour); a value outside them is a `message` `error` and the text is not added. Text,etc/Text asks a `mouse` first (where the text goes), then this; a cancel adds nothing. |
| `alert` | `button`, `message` | nothing |
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
field (`Poincare map: Direction: "up" is not one of both, increasing,
decreasing`), nothing applied. Its extra kinds: `boolean` (the answer is the
text `true` or `false`; a checkbox), `choice` (one of the names in the
form's `options` entry for the field, exactly; a select whose labels are the
names with underscores as spaces and a capital first letter, derived, never
written apart) `real` (a plain number, no `%formula`; `integer` takes
the same) and `color` (`foreground`, `#rrggbb`, `#rrggbbaa` or a CSS colour name; the page's picker gives the first three: a colour and its alpha). The form ask carries `options` (per field the names of a `choice`,
`null` for another kind) and `ranges` (per field `{min, max}` of an `integer`
or `real`, either key only when given, `null` when it has none) beside
`kinds`; the answer stays `values`, one text per field. Poincare map's form
(Numerics, Poincare map) is the first: Section `real`, Direction `choice`
(`both`, `increasing`, `decreasing`), Stop on section `boolean`. A `*n` field (above) picks from its list
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
type instead (`auto_point_symbol`'s `BP`, `EP`, `HB`, `LP`, `MX`, `PD`, `TR`,
`UZ`, `BT`, `CP`, `ZH`, `GH`, `R1`, `R2`, `R3`, `R4`, `LPD`, `LTR`, `PTR`, `TTR`: "the 2nd HB"). A label no stored point has, or a `type`/`index` with
no such point, is a `message` `error` and nothing changes -- the diagram,
`grabpt` and the info strip stay as they were. `{"cmd":"auto","op":"grab"}`
with neither field is refused (`message` `error`): the interactive grab is
the AUTO window's key `g`, `{"cmd":"key","win":"auto","key":"g"}`, which
asks (`grab`, above). A script that recorded an interactive grab (W59) replays it as the
keys it was answered with, exactly as it was driven; `grab`/`label` is for
a script that names the point instead (W56's --silent commands).

**Restarting a run.** `{"cmd":"auto","op":"restart","label":N}` (a
compute command, W275) grabs the stored point labelled `N` as the grab by
label does and runs AUTO from it with the settings of the last run (its
kind of calculation, continuation parameters and direction, and the
Numerics as they are now): the continuation goes on from the label, in the
Run menu's "extend" of that run, with no ask. A label no stored point has
is a `message` `error` ("Restart: no point labelled N") and nothing runs.
It is the second command of the action an exact Jacobian's failure in an
AUTO run offers (`error`'s `action`, above).

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

A colour is a colour ("Colours"): the `.ani`'s named colours as XPPAUT's own,
or a colour of the session's colour scale (an expression's value, 0..1). A primitive's
colour, width and text font are the pen's at that point: the colour starts as the foreground each frame, width and font
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
a paused restart. The `player` feature in `hello.features`, requested by `data`,
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
