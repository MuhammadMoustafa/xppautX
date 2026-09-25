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
the matching `answer` and ignores other commands except `state`,
`browser` with `from`, and `quit`. See "Commands during a command" for what
reaches a running computation.

## Startup

1. `hello`: protocol version (2), window title, the three main menus
   (`main`, `file`, `num` with `_keys` and `_hints`).
2. `window` `create` for window 1, the main plot.
3. `state`, then `idle`.

A client that draws sends `data` next (see "The plot as data").

## Commands (client to server)

| cmd | fields | meaning |
|---|---|---|
| `key` | `key` | A hotkey, exactly as typed in xppaut: one character, or `Escape`, `Enter`, `Tab`, `Backspace`, `Delete`, `Home`, `End`, `ArrowLeft/Right/Up/Down`, `PageUp`, `PageDown` (DOM `KeyboardEvent.key` names; X keysym names also work). Menu clicks are sent as the item's key. |
| `answer` | `id`, `ok` (0/1), plus the kind's fields | Reply to an `ask`. Omitting `ok` means ok. Omitting `id` answers whichever `ask` is currently pending (see "Scripts": a script cannot know the id an `ask` is handed at run time, and this equally lets a plain client skip tracking it). |
| `set` | `kind` (`par`, `ic`, `bc`, `delay`), `name` or `index`, `value` or `text` | Change a value (no redraw or rerun). `name` is matched without regard to case, in full: names go up to 64 characters (`XPP_NAME_MAX`) and every event carries them unshortened. `text` is what the X11 box takes: a number or `%formula` for `par` and `ic`, an expression for `bc` and `delay`. BCs and delays go by `index` (BC names all read `0=`). A formula that does not evaluate gives `message` `error`. Several values in one command: `values` [{`kind`, `name` or `index`, `value` or `text`}...]. `rerun` 1: then clear and integrate again as `slide` does, once, and only when every value was set (the values panel's "run on change", Load). `kind` `ic` with `from` `last` and nothing else: every initial condition from where the last run ended (`state.now`, what Initialconds/Last starts from), without a run; `message` `error` "No prior solution" before any run. |
| `default` | `kind` (`par` or `ic`), `rerun` (default 0) | The Default button: values from the ODE file (`hello.defaults`); `rerun` 1 integrates again afterwards, as `set` does. |
| `slide` | `name`, `value`, `rerun` (default 1) | A parameter slider moved: set the parameter or variable, then clear and integrate again. |
| `userbut` | `index` | An `@ button` of the ODE file (`hello.userbuttons`). |
| `plotvars` | `how` (0 x vs t, 1 phase plane, 2 array plot), `names` | The IC box's xvst/pp/arry buttons for the checked variables. |
| `browser` | `from`, `count`, `col`, `ncol` | The data browser block the client shows (answered at once with `browser`, even during a prompt); `count` 0 stops the updates. |
| `browser` | `op` (`find`, `get`, `replace`, `unreplace`, `table`, `load`, `write`, `first`, `last`, `restore`, `addcol`, `delcol`), `row` | A data browser button, with `row` the selected row (the X11 browser's top row). |
| `eqimport` | | The equilibrium window's Import: the last equilibrium becomes the initial conditions. |
| `equations` | | Send `equations`. |
| `data` | `events` (names from `hello.features`), `enc` | The data events the client wants from now on (`[]` stops them); each is sent at the end of this command. `series`: the plot windows' curves as numbers, `plots`: the plot windows themselves, `nullclines` and `dfield`: what the phase planes show besides their curves, `marks`: equilibria, text, arrows, markers and frozen curves on the plots (all in "The plot as data", below); `ani`: the animation's frames ("The animation as data"); `autoinfo`: AUTO's info strip and stability circle ("The AUTO diagram as data"); `autosettings`: AUTO's Numerics, parameters, axes and Mark values ("AUTO's settings as data"). `enc` `"f32"` sends these events' value arrays as base64 of little-endian float32 instead of JSON numbers (an `ani` frame is always JSON). |
| `action` | `index` | Run the action of comment `index` of `source.comments`. |
| `click` | `win` | The user selected plot window `win`. |
| `view` | `win`, `xlo`, `xhi`, `ylo`, `yhi` | "Use this view" (docs/ui-v2.md T9): sets window `win`'s 2D axes exactly as Window/Window would (graf_par.c `update_view`), so a later PostScript/SVG export, Restore and redraw all agree with them; the `plots` and `state.view` that follow the command show the new axes. Refused (`message` `error`) and nothing changed when `xlo`..`yhi` are not all finite, `xlo>=xhi`, `ylo>=yhi`, or `win` names no open window. |
| `redraw` | | Redraw the active plot window, and the AUTO diagram when AUTO is open (for a client that reconnects). |
| `state` | | Send `state` now. |
| `auto` | `op`: `param`, `axes`, `numerics`, `run`, `grab`, `usr`, `clear`, `redraw`, `file`, `close`, `point` (`x`, `y`, or `xd`, `yd`), `set` (`numerics`, `pars`, `axes`, `marks`) | The AUTO window buttons; `set` writes AUTO's settings without the forms (see "AUTO's settings as data"); `point` is a click on the diagram at pixel `x`, `y` of window 101 or at `xd`, `yd` in the diagram's quantities (shows its coordinates, and in a two-parameter plot stores them for AUTO's File/sElect 2par pt (`e`), which sets the two parameters to them); `close` destroys window 101, File/Auto opens it again. |
| `session` | `op` (`save`, `load`), `name` | Save or load a session: `<name>.set` (File/Write set, File/Read set) and, when a diagram exists (save) or a `<name>.auto` file is found (load), `<name>.auto` too (AUTO File/Save diagram, File/Load diagram). Without `name`, asks for one (`ask` kind `file`, like any other Save/Load). A load opens the AUTO window first when `<name>.auto` exists and AUTO is not already open. `state.session` (below) names the files the current session was last saved to or loaded from. |
| `aplot` | `op`: `redraw`, `edit`, `print`, `fit`, `range`, `gif`, `close`, `scroll` (`dy` pixels) | The array plot window buttons; dragging the plot scrolls through time. |
| `rotate` | `what` (`down`, `move`, `up`), `x`, `y` | Dragging a 3D plot turns it (the active window, when `state.view.three`). |
| `view3d` | `win`, `theta`, `phi` | Sets window `win`'s 3D angles (degrees) directly and redraws (docs/ui-v2.md T14): a client that projects the box itself (web2, dragging or arrow keys) reports where it settled, so the core's own state (`plots`, `state.view.theta/phi`) and a PostScript/SVG export all agree; simpler than replaying `rotate`'s pixel deltas outside the drag the core tracks. Refused (`message` `error`) and nothing changed when `win` names no open window, it is not a 3D plot, or `theta`/`phi` is not finite. |
| `ani` | `op`: `go`, `pause`, `fast`, `slow`, `speed` (`ms`), `step` (`n`), `seek` (`pos`), `reset`, `skip`, `file`, `mpeg`, `grab`, `mouse` (`what` down/move/up, `x`, `y` or `u`, `v`), `fly`, `close` | The animation window buttons. `go` plays until the last frame; `pause`, `fast`, `slow` and `speed` sent while it plays reach its loop. `speed` sets the delay between two frames of `go` to `ms` (0..1000; `fast` and `slow` change it by 2 within 0..100). `step` moves `n` rows from the core's position (`ani` `pos`), `seek` goes to row `pos`. `file` loads an `.ani` file (a `file` ask) and, when there is data, shows its first frame. `mpeg` asks for frame saving (PPM files or `anim.gif`), done with `pixels` asks while playing. `mouse` drags a grab point after `grab`, at pixel `x`, `y`, or at `u`, `v` in the animation's unit coordinates (those of the `ani` `frame` event, y up). |
| `abort` | `at` (scripts only) | Stop the running command's computation, at once (see below). No reply of its own: the stopped command ends with `stopped`, `state` and `idle`; outside a command it does nothing. `at` is where a recorded session stopped (the `stopped` event's `at`); only a script's player reads it (see "Scripts"), anywhere else it is an ordinary `abort`. |
| `file` | `op` (`list`, `get`, `put`), `name`, `data` | The model's folder (the working directory) for a client that cannot reach it: `put` writes `data` (base64, at most 64 MB decoded) as `name`, `get` reads `name` back, `list` lists the folder. Answered with a `file` event, then `state` and `idle`. Names are base names only (see "Files" below). |
| `quit` | | Exit, at once even during a computation. |

## Commands during a command

Input is read on its own thread, so lines keep arriving while the core
computes. Every command runs as a *job*, numbered by the position of its line
in the input.

- `abort` and `quit` act the moment they arrive: they cancel the running job
  and every job whose line came before theirs, even one still waiting its
  turn, and the computation stops at its next check (every integration step;
  AUTO between continuation points). So an `abort` sent right behind a
  command stops that command, however late the core gets to it, while a
  command sent after the `abort` runs normally. An answer to a prompt sent
  after an `abort` counts as the user's last word: the rest of that command
  is not cancelled. `quit` then exits.
- While a job runs, `key`, `set`, `state`, `browser` with `from`, and
  `ani` `pause`/`fast`/`slow`/`speed` are *control* lines: the computation acts on
  them as they come (Escape stops it, a `set` changes a parameter under it,
  `state` is answered at once). Other keys are consumed, as the
  X11 program does. A control line the job does not get to runs after it
  as an ordinary command.
- Every other command sent during a job is queued and runs, in order, after
  the job's `idle` (with its own `state` and `idle`). Nothing is dropped
  except keys and edits sent while a prompt is open; an `auto` `set` sent
  then is kept and applied when the command that asked ends (not in a
  script, where the line after an ask is its answer).
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
runs out, or 1 if a `message` event of `error` kind was sent. A line that
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
interruption at AT was never reached", K being the abort line. An `at` of
`other` cannot be placed: the job runs to its end. An abort line with no
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
{"cmd":"eqimport"}

{"cmd":"key","key":"f"}
{"cmd":"key","key":"a"}

{"cmd":"auto","op":"run"}
{"cmd":"answer","key":"s"}

{"cmd":"auto","op":"grab"}
{"cmd":"answer","key":"Tab"}
{"cmd":"answer","key":"Return"}

{"cmd":"auto","op":"run"}
{"cmd":"answer","key":"p"}

{"cmd":"auto","op":"file"}
{"cmd":"answer","key":"s"}
{"cmd":"answer","file":"lecar.auto"}
```

Run it with:

    xppautX --script examples/scripts/lecar_auto.jsonl examples/ode/lecar.ode

## Events (server to client)

| ev | fields | meaning |
|---|---|---|
| `hello` | `protocol`, `features` (optional parts the server speaks: `series`, `plots`, `nullclines`, `dfield`, `marks`, `ani`, `autoinfo`, `autosettings`), `title`, `file`, `menus`, `lists`, `auto_hints`, `userbuttons` [name...], `sliders` [{`name`,`lo`,`hi`}...] | First event. `lists` are what a form field `*n` picks from: 0 T and every variable, 1 ODE variables, 2 parameters, 3 both, 4 colours, 5 markers, 6 methods (items like `2 Box` start with the number to enter). `sliders` are the ones the ODE file sets (`@ s1=...`). `defaults` {`pars`, `ics`}: the ODE file's values, one per entry of `state`'s `pars` and `ics` in their order (what `default` restores). |
| `window` | `op` (`create`, `select`, `destroy`), `win`, `w`, `h`, `title` | Plot windows 1..10, AUTO 101, animation 104. `w`, `h` are the core's pixel size of the window: what the pixel fields of `state.view`, `state.auto` and pixel answers to asks refer to. |
| `diagram` | `op` (`axes`, `reset`, `add`), ... | The AUTO diagram as data; see "The AUTO diagram as data". |
| `autoinfo` | `info`, `stab`, `stop` | AUTO's info strip and stability circle, and why the last branch ended, as data, for a client that asked (`data`); see "The AUTO diagram as data". |
| `autosettings` | `numerics`, `pars`, `axes`, `marks` | AUTO's settings as data, for a client that asked (`data`); see "AUTO's settings as data". |
| `series` | `win`, `rows`, `three`, `enc`, `xlabel`, `ylabel`, `zlabel`, `curves`, `shift`, `columns`; or `op` `append`, `win`, `from`, `rows`, `enc`, `columns` | A plot window's curves as numbers, one event per window, for a client that asked (`data`), and during an integration the active window's rows as they are stored; see "The plot as data". |
| `erase` | `win` | The Erase command blanked plot window `win`, for a client that asked for `series`: it shows nothing of that window's curves (nor the earlier runs it may keep) until the window's next series or `redraw`. Only Erase sends it: a slider's rerun or a zoom also redraw the window, and keep what a data client shows. See "The plot as data". |
| `redraw` | `win` | The Redraw command drew window `win` again, for a client that asked for `series`: it shows the window's current series again (no series follows: the data did not change), and no earlier runs. |
| `plots` | `active`, `windows` [{`win`, `title`, `three`, `xlo`, `xhi`, `ylo`, `yhi`, `xlabel`, `ylabel`, `zlabel`, `box`, `theta`, `phi`, `persp`, `zplane`, `zview`, `curves`, `shift`}...] | Every plot window and the active one, for a client that asked (`data`); see "The plot as data". |
| `nullclines` | `win`, `enc`, `xname`, `yname`, `xcolor`, `ycolor`, `x`, `y`, `frozen` [{`x`,`y`}...] | A plot window's nullclines as segments in plot coordinates, for a client that asked (`data`); see "The plot as data". |
| `dfield` | `win`, `enc`, `scaled`, `color`, `n`, `du`, `dv`, `grid`, `speed`, `flows` [{`color`,`x`,`y`}...] | A plot window's direction field and Flow trajectories, for a client that asked (`data`); see "The plot as data". |
| `marks` | `win`, `enc`, `equilibria`, `text`, `arrows`, `markers`, `frozen` | A plot window's equilibria, text, arrows, markers and frozen curves, for a client that asked (`data`); see "The plot as data". |
| `state` | `pars` [[name,value]...], `ics` [[name,value]...], `now` [value...] (after a first run), `bcs` [[name,text]...], `delays` [[name,text]...] (delay equations only), `view` {`win`,`left`,`right`,`top`,`bottom`,`xlo`,`xhi`,`ylo`,`yhi`,`three`, and `theta`,`phi` when `three`}, `auto` {`x0`,`y0`,`wid`,`hgt`,`xmin`,`xmax`,`ymin`,`ymax`} (AUTO open), `rows`, `menu`, `win`, `session` {`set`,`auto`} | Current values; `view` maps pixels of the active window to plot coordinates (x = xlo + (xhi-xlo)(px-left)/(right-left), y likewise with bottom/top) and `auto` those of the AUTO diagram, for a readout under the mouse; `theta`, `phi` are the active window's 3D angles (degrees, meaningful when `three`), so a client that turned a 3D plot itself (`view3d`) can confirm the core agrees; `rows` is the number of stored time points, `menu` the active main menu (0 main, 1 file, 2 numerics), `now` the current state, one value per `ics` entry: where the last run ended or stopped (what Initialconds/Last and `set` `from` `last` copy into the ICs), `win` the active window; `session` names the files of the last `session` `save` or `load` (`auto` absent when that session has no diagram; the member itself absent before any `session` command). |
| `idle` | | The command finished. |
| `stopped` | `at` | The command's computation was cancelled; sent before its `state` and `idle`. `at` says where it stopped: `{"what":"integrate","rows":N,"t":T}` for an integration, N the rows in storage (as `state.rows`) and T the time of the last one stored (9 digits: the stored single-precision value exactly); `{"what":"auto","branch":B,"point":P}` for an AUTO run, P the last point it stored on branch B (the end point the cancel adds, as in the diagram's data); `{"what":"other"}` for anything else. A script replays the interruption from it (see "Scripts"). |
| `menu` | `which` | The main menu switched (0 main, 1 file, 2 numerics). |
| `help` | `chapter`, `anchor` (optional) | File/Help: open the manual at this chapter (and anchor). |
| `title` | `text` | Title of the selected plot window: what it plots (`W vs V`). The server also labels unlabelled 2D axes with the plotted variables. |
| `message` | one of `error`, `bottom`, `box`, `xy`, `auto`, `calc` | Status text. `box` with empty text removes a hint box. |
| `progress` | `n`, `of` | Computation progress, at most 10 a second. |
| `equilibrium` | `type`, `cplus`, `cminus`, `rplus`, `rminus`, `im`, `values`, `eigenvalues` | Result of Sing pts. `eigenvalues`: the Jacobian's `[re,im]` pairs, one per variable; absent for a delay equation. |
| `source` | `lines`, `comments` [[text, has action]...] | File/Prt src. |
| `equations` | `lines` | One `dX/dT=...` line per equation. |
| `ani` | `pos`, `rows`, `fly`, `grab`, `skip`, `speed`, `loaded`, `open`; or `op` `frame`, ... | Animation state for its slider and toggles, sent with every frame drawn and after every `ani` command: `pos` the row the next step starts from, `speed` the ms between frames of Go, `loaded` 1 when an `.ani` file is loaded, `open` 1 while the animation window exists. With `op` `frame`: a frame as data, for a client that asked (`data`); see "The animation as data". |
| `aplot` | `title`, `nx`, `ny`, `cells` (ny rows of nx colour indices, -1 blank), `values`, `enc`, `first`, `ncolors`, `zmin`, `zmax`, `tlo`, `thi`, `tag` | The array plot (window 105): cell index k is colour `first`+k of the core's colour table (what its GIF writer paints). `values` is the same `ny` rows of `nx` cells' stored numbers, before that mapping (float32, `null`/NaN off the stored rows or columns, same layout as `cells`), for a client that picks its own colour scale from them and `zmin`/`zmax`; `enc` `"f32"` (the client's last `data` `enc`, reused here since `aplot` is not itself in the `data` subscription list) sends `values` as base64 float32 like a series column (see "The plot as data"). |
| `film` | `op` (`capture`, `reset`, `play`, `autoplay`), `count`, `win`, `cycles`, `delay` | Kinescope. The client keeps the frames: on `capture` it copies window `win` as it is drawn now; `play` shows them, `autoplay` plays `cycles` times `delay` ms apart. |
| `browser` | `rows`, `cols` (names, `T` first), `row0` (selected row), `start`, `end` (the First..Last range), `from`, `col`, `data` | Rows `from`.. as [T, column `col`, `col`+1, ...]; `null` for NaN. Sent for a `browser` block request and after any command that changed the data while the client shows the browser. |
| `ping` | | Beep. |
| `bye` | | The program is exiting. |
| `file` | `op`, `name`, `ok`; `size`, `sha256` (`put`, `get`), `data` (`get`, base64), `files` (`list`: [{`name`,`size`,`mtime`,`sha256`}...]); `error` when `ok` is 0 | The answer to a `file` command (see "Files" below). |
| `ask` | `id`, `kind`, ... | See below. |

In browser mode (`xppautX model.ode`) events stream from `/events?t=TOKEN`.
Commands are POSTed to `/cmd?t=TOKEN`, one per request (a body of several
lines gives several commands). xppautX answers each connection on a thread
of its own, so a connection that sends nothing (a browser's preconnect) or a
stalled upload holds up no command, `abort` above all; two POSTs in flight at
once may therefore reach the core in either order, and a client that needs
its order sends the next one once the last was answered, as the page does.

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

- `{"ev":"diagram","op":"axes", xmin, xmax, ymin, ymax, x0, y0, wid, hgt, plot, xlabel, ylabel}`:
  the diagram was drawn again at these axes (`plot` is `Auto.plot`: 0 hi,
  1 norm, 2 hi and lo, 3 period, 4 two parameters, 10 frequency, 11
  average); the points are unchanged. Pixel `x0 + wid*(x-xmin)/(xmax-xmin)`,
  `y0 + hgt - hgt*(y-ymin)/(ymax-ymin)` of window 101 is (x, y) in the
  core's pixels (what a pixel answer to a `grab` or `rubber` ask means).
- `{"ev":"diagram","op":"reset","keep":k, ...the axes fields}`: drop every
  point after the first `k` (all of them for 0), then the axes as above.
- `{"ev":"diagram","op":"add","from":n,"runs":[...]}`: points `n`, `n+1`, ...
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
AUTO runs; a command that changes neither sends none. core/auto_data.cpp
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
| `info.point` | its index in the `diagram` data (the points the client holds), -1 when they do not have it (after Clear, or a load not drawn yet) |
| `info.br`, `pt`, `type`, `sym`, `lab`, `f2` | branch and point number (positive, as in `diagram`), `type` as a run's `ty` (1 stable steady state .. 4 unstable periodic), the label's type (`EP`, `LP`, `HB`, ... or empty), the label (0 for none), and for a two-parameter point its curve kind as `f2` |
| `info.par` | the continuation parameter's `name` and `value`, and the second parameter's for a two-parameter point (the strip then shows both; for a one-parameter point it shows a blank name and 0) |
| `info.norm`, `var`, `u`, `per` | the norm, the variable of the Axes setting and its value, the period (AUTO's value for a steady state too: what the strip prints) |
| `info.x`, `y`, `y2` | where the diagram plots the point, in the quantities of the Axes setting |
| `stab` | what the circle shows: the point AUTO computed or a redraw plotted last, or the grab's cursor; `null` before any |
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
| `marks` | Mark values: `[name, value]` per user point, where the parameter `name` (one of `pars`) or the period `T` reaches `value` AUTO labels the point (UZ) |

Numbers are doubles in their shortest exact form, the whole-number fields
as integers; `null` when not finite.

**`{"cmd":"auto","op":"set", ...}`** writes them without the forms, with
any of the four members above, each part only when given: `numerics` any
of its fields, `pars` the first N of AUTO's parameters (an empty name keeps
one), `axes` any of `plot`, `var`, `par1`, `par2` (names among `pars`,
after this set's own `pars`), the four ranges, and `"fit":true` for
Axes/Fit afterwards, `marks` the whole list (0 to 9 pairs; `[]` for none).
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

`set` is an ordinary command: sent while AUTO (or anything) runs, it waits
for that job and applies after its `idle` (a continuation already running
keeps the settings it started with). `tools/servercheck.py` checks that the
event is what the forms show, that a form's OK and a `set` show in it,
that bad values are refused whole, and that Nmax set to 12 stops the next
run at 12 points.

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
version other data. web2 keeps the series a new run replaces as an earlier
run, drawn lighter under the current one, until `erase` or `redraw`
(store/runs.ts); an `append` from row 0 starts such a run.

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
| `theta`, `phi` | the 3D view's angles in degrees (3d-params, `rotate`, `view3d`) |
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
range, a slider's rerun, ...), the rows it stores go out as they come for
the active window, at most ten times a second:

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
core/plot_data.cpp sends at most one append per 100 ms).

### Asks

| kind | fields | answer fields |
|---|---|---|
| `menu` | `name`, `title`, `items`, `keys`, `hints`, `def` | `key` (empty or `ok:0` cancels) |
| `choice` | `title`, `question`, `choices`, `keys` | `key` |
| `string` | `title`, `name`, `value`, `ok`, `cancel`, `max`, `kinds` | `value` |
| `form` | `title`, `names`, `values`, `max`, `kinds` | `values` (same length). A name starting with `*n` means the field picks from `hello.lists[n]`: a variable (`*0`), a parameter (`*2`), a colour (`*4`), a marker (`*5`), ...; for a list whose items start with a number (`2 Box`) the value is that number. |
| `checklist` | `title`, `names`, `flags` | `flags` |
| `file` | `title`, `mode` (`read` or `write`), `file`, `wild`, `dir`, `dirs`, `files` | `file`; or `cd` (a folder name or `..`) or `wild` (a new pattern) to be asked again with that listing |
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
edit_box's right-hand sides). `file`: a file's base name. `name:N`: a name
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
