# xppautX protocol

`xppautX --server file.ode [xppaut options]` loads the model the way `xppaut`
does and then talks line-delimited JSON: one object per line, UTF-8, on
stdin (commands, `"cmd"`) and stdout (events, `"ev"`). stderr carries the
core's own log output. The implementation is `core/ui_json.c`;
`tools/servercheck.py` is a working client, `web/xpp-client.js` a full one.

The core is single-threaded. A command runs to completion, then the server
sends `state` and `idle`. While a command runs the server can stop and
**ask** the client something (a menu, a prompt, a mouse click); it waits for
the matching `answer` and ignores other commands except `size`, `state`,
`browser` with `from`, and `quit`. See "Commands during a command" for what
reaches a running computation.

## Startup

1. `hello`: protocol version, window title, font cell size, the three main
   menus (`main`, `file`, `num` with `_keys` and `_hints`).
2. `palette`: 256 colours; drawing ops refer to these indices.
3. `window` `create` for window 1, the main plot.
4. `state`, then the first drawing, then `idle`.

Send `size` for window 1 as soon as the canvas size is known.

## Commands (client to server)

| cmd | fields | meaning |
|---|---|---|
| `key` | `key` | A hotkey, exactly as typed in xppaut: one character, or `Escape`, `Enter`, `Tab`, `Backspace`, `Delete`, `Home`, `End`, `ArrowLeft/Right/Up/Down`, `PageUp`, `PageDown` (DOM `KeyboardEvent.key` names; X keysym names also work). Menu clicks are sent as the item's key. |
| `answer` | `id`, `ok` (0/1), plus the kind's fields | Reply to an `ask`. Omitting `ok` means ok. Omitting `id` answers whichever `ask` is currently pending (see "Scripts": a script cannot know the id an `ask` is handed at run time, and this equally lets a plain client skip tracking it). |
| `size` | `win`, `w`, `h` | Canvas size in pixels; the plot is redrawn. For the AUTO diagram (`win` 101) the size includes the axis margins and applies when the current command ends; the server answers with `window` `create` for 101 and redraws the diagram. |
| `set` | `kind` (`par`, `ic`, `bc`, `delay`), `name` or `index`, `value` or `text` | Change a value (no redraw or rerun). `name` is matched without regard to case, in full: names go up to 64 characters (`XPP_NAME_MAX`) and every event carries them unshortened. `text` is what the X11 box takes: a number or `%formula` for `par` and `ic`, an expression for `bc` and `delay`. BCs and delays go by `index` (BC names all read `0=`). A formula that does not evaluate gives `message` `error`. |
| `default` | `kind` (`par` or `ic`) | The Default button: values from the ODE file. |
| `slide` | `name`, `value`, `rerun` (default 1) | A parameter slider moved: set the parameter or variable, then clear and integrate again. |
| `userbut` | `index` | An `@ button` of the ODE file (`hello.userbuttons`). |
| `plotvars` | `how` (0 x vs t, 1 phase plane, 2 array plot), `names` | The IC box's xvst/pp/arry buttons for the checked variables. |
| `browser` | `from`, `count`, `col`, `ncol` | The data browser block the client shows (answered at once with `browser`, even during a prompt); `count` 0 stops the updates. |
| `browser` | `op` (`find`, `get`, `replace`, `unreplace`, `table`, `load`, `write`, `first`, `last`, `restore`, `addcol`, `delcol`), `row` | A data browser button, with `row` the selected row (the X11 browser's top row). |
| `eqimport` | | The equilibrium window's Import: the last equilibrium becomes the initial conditions. |
| `equations` | | Send `equations`. |
| `data` | `events` (names from `hello.features`), `enc` | The data events the client wants from now on (`[]` stops them); each is sent at the end of this command. `series`: the plot windows' curves as numbers, `plots`: the plot windows themselves (both in "The plot as data", below). `enc` `"f32"` sends the series' values as base64 of little-endian float32 instead of JSON numbers. |
| `action` | `index` | Run the action of comment `index` of `source.comments`. |
| `click` | `win` | The user selected plot window `win`. |
| `redraw` | | Redraw the active plot window, and the AUTO diagram when AUTO is open (for a client that reconnects). |
| `state` | | Send `state` now. |
| `auto` | `op`: `param`, `axes`, `numerics`, `run`, `grab`, `usr`, `clear`, `redraw`, `file`, `close`, `point` (`x`, `y`) | The AUTO window buttons; `point` is a click on the diagram (shows and stores its coordinates); `close` destroys window 101, File/Auto opens it again. |
| `session` | `op` (`save`, `load`), `name` | Save or load a session: `<name>.set` (File/Write set, File/Read set) and, when a diagram exists (save) or a `<name>.auto` file is found (load), `<name>.auto` too (AUTO File/Save diagram, File/Load diagram). Without `name`, asks for one (`ask` kind `file`, like any other Save/Load). A load opens the AUTO window first when `<name>.auto` exists and AUTO is not already open. `state.session` (below) names the files the current session was last saved to or loaded from. |
| `aplot` | `op`: `redraw`, `edit`, `print`, `fit`, `range`, `gif`, `close`, `scroll` (`dy` pixels) | The array plot window buttons; dragging the plot scrolls through time. |
| `rotate` | `what` (`down`, `move`, `up`), `x`, `y` | Dragging a 3D plot turns it (the active window, when `state.view.three`). |
| `ani` | `op`: `go`, `pause`, `fast`, `slow`, `step` (`n`), `seek` (`pos`), `reset`, `skip`, `file`, `mpeg`, `grab`, `mouse` (`what` down/move/up, `x`, `y`), `fly`, `close` | The animation window buttons. `go` plays until the last frame; `pause`, `fast`, `slow` sent while it plays reach its loop. `mpeg` asks for frame saving (PPM files or `anim.gif`), done with `pixels` asks while playing. `mouse` drags a grab point after `grab`. `size` for win 104 resizes the picture when the command ends. |
| `abort` | `at` (scripts only) | Stop the running command's computation, at once (see below). No reply of its own: the stopped command ends with `stopped`, `state` and `idle`; outside a command it does nothing. `at` is where a recorded session stopped (the `stopped` event's `at`); only a script's player reads it (see "Scripts"), anywhere else it is an ordinary `abort`. |
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
- While a job runs, `key`, `set`, `size`, `state`, `browser` with `from`, and
  `ani` `pause`/`fast`/`slow` are *control* lines: the computation acts on
  them as they come (Escape stops it, a `set` changes a parameter under it,
  `size` and `state` are answered at once). Other keys are consumed, as the
  X11 program does. A control line the job does not get to runs after it
  as an ordinary command.
- Every other command sent during a job is queued and runs, in order, after
  the job's `idle` (with its own `state` and `idle`). Nothing is dropped
  except keys and edits sent while a prompt is open.
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
| `hello` | `protocol`, `features` (optional parts the server speaks: `series`, `plots`), `title`, `file`, `char` {`w`,`h`,`bw`,`bh`}, `menus`, `lists`, `auto_hints`, `userbuttons` [name...], `sliders` [{`name`,`lo`,`hi`}...] | First event. Text is laid out on a `char.w` x `char.h` monospace cell. `lists` are what a form field `*n` picks from: 0 T and every variable, 1 ODE variables, 2 parameters, 3 both, 4 colours, 5 markers, 6 methods (items like `2 Box` start with the number to enter). `sliders` are the ones the ODE file sets (`@ s1=...`). |
| `palette` | `colors` (256 `#rrggbb`) | Colour table; sent again after a colormap change. |
| `window` | `op` (`create`, `select`, `destroy`), `win`, `w`, `h`, `title` | Plot windows 1..10, AUTO 101 (stability circle 102, info strip 103), animation 104. |
| `draw` | `win`, `ops` | Drawing, see below. |
| `diagram` | `op` (`axes`, `reset`, `add`), ... | The AUTO diagram as data, beside its drawing; see "The AUTO diagram as data". |
| `series` | `win`, `rows`, `three`, `enc`, `xlabel`, `ylabel`, `zlabel`, `curves`, `shift`, `columns`; or `op` `append`, `win`, `from`, `rows`, `enc`, `columns` | A plot window's curves as numbers, one event per window, for a client that asked (`data`), and during an integration the active window's rows as they are stored; see "The plot as data". |
| `plots` | `active`, `windows` [{`win`, `title`, `three`, `xlo`, `xhi`, `ylo`, `yhi`, `xlabel`, `ylabel`, `zlabel`, `box`, `theta`, `phi`, `persp`, `zplane`, `zview`, `curves`, `shift`}...] | Every plot window and the active one, for a client that asked (`data`); see "The plot as data". |
| `state` | `pars` [[name,value]...], `ics` [[name,value]...], `bcs` [[name,text]...], `delays` [[name,text]...] (delay equations only), `view` {`win`,`left`,`right`,`top`,`bottom`,`xlo`,`xhi`,`ylo`,`yhi`,`three`}, `auto` {`x0`,`y0`,`wid`,`hgt`,`xmin`,`xmax`,`ymin`,`ymax`} (AUTO open), `rows`, `menu`, `win`, `session` {`set`,`auto`} | Current values; `view` maps pixels of the active window to plot coordinates (x = xlo + (xhi-xlo)(px-left)/(right-left), y likewise with bottom/top) and `auto` those of the AUTO diagram, for a readout under the mouse; `rows` is the number of stored time points, `menu` the active main menu (0 main, 1 file, 2 numerics), `win` the active window; `session` names the files of the last `session` `save` or `load` (`auto` absent when that session has no diagram; the member itself absent before any `session` command). |
| `idle` | | The command finished. |
| `stopped` | `at` | The command's computation was cancelled; sent before its `state` and `idle`. `at` says where it stopped: `{"what":"integrate","rows":N,"t":T}` for an integration, N the rows in storage (as `state.rows`) and T the time of the last one stored (9 digits: the stored single-precision value exactly); `{"what":"auto","branch":B,"point":P}` for an AUTO run, P the last point it stored on branch B (the end point the cancel adds, as in the diagram's data); `{"what":"other"}` for anything else. A script replays the interruption from it (see "Scripts"). |
| `menu` | `which` | The main menu switched (0 main, 1 file, 2 numerics). |
| `title` | `text` | Title of the selected plot window: what it plots (`W vs V`). The server also labels unlabelled 2D axes with the plotted variables. |
| `message` | one of `error`, `bottom`, `box`, `xy`, `auto`, `calc` | Status text. `box` with empty text removes a hint box. |
| `progress` | `n`, `of` | Computation progress, at most 10 a second. |
| `equilibrium` | `type`, `cplus`, `cminus`, `rplus`, `rminus`, `im`, `values` | Result of Sing pts. |
| `source` | `lines`, `comments` [[text, has action]...] | File/Prt src. |
| `equations` | `lines` | One `dX/dT=...` line per equation. |
| `ani` | `pos`, `rows`, `fly`, `grab`, `skip`, `speed` | Animation state for its slider and toggles; sent with every frame. |
| `aplot` | `title`, `nx`, `ny`, `cells` (ny rows of nx colour indices, -1 blank), `first`, `ncolors`, `zmin`, `zmax`, `tlo`, `thi`, `tag` | The array plot (window 105): cell index k is palette colour `first`+k. |
| `film` | `op` (`capture`, `reset`, `play`, `autoplay`), `count`, `win`, `cycles`, `delay` | Kinescope. The client keeps the frames: on `capture` it copies window `win` as it is drawn now; `play` shows them, `autoplay` plays `cycles` times `delay` ms apart. |
| `browser` | `rows`, `cols` (names, `T` first), `row0` (selected row), `start`, `end` (the First..Last range), `from`, `col`, `data` | Rows `from`.. as [T, column `col`, `col`+1, ...]; `null` for NaN. Sent for a `browser` block request and after any command that changed the data while the client shows the browser. |
| `ping` | | Beep. |
| `bye` | | The program is exiting. |
| `ask` | `id`, `kind`, ... | See below. |

In browser mode (`xppautX model.ode`) events stream from `/events?t=TOKEN`;
`/events?t=TOKEN&draw=0` is the same stream without the `draw` events, for
a page that draws from data (web2): a long run's drawing is tens of
megabytes it would only throw away.

Two more events come from the host, not the server: `log` {`text`} carries what the
server printed on stderr (xppaut reports model errors, such as a formula that does
not parse, only there) and `exit` {`code`} says the process ended. `xpp-client.js`
shows one error at a time in a red box (the newest replaces it and the next
command clears it; a load failure or crash stays with the output that explains it)
and keeps everything under "Messages".

### Drawing ops

Each op is an array; coordinates are pixels from the top-left of the window.

| op | arguments |
|---|---|
| `clear` | fill the window with the background |
| `color` | index into the palette; 0 is the foreground, -1 the background |
| `lw` | line width in pixels |
| `dash` | dash pattern: 0 solid, 1-9 the xppaut patterns (`dashes[]` in `graphics_x11.c`: 1 is 1 on 6 off, 3 is 4/2, 4 is 1/3, 5 is 4/4, 6 is 1/5, 7 is 4/4/4/1) |
| `line` | x1, y1, x2, y2 |
| `point` | x, y, r; r 0 is one pixel, else a filled circle of radius r/1.414 |
| `bead` | x, y (a filled circle of radius 2) |
| `frect`, `rect` | x, y, w, h |
| `circle`, `fcircle` | x, y, r |
| `ellipse`, `fellipse` | x, y, w, h (bounding box) |
| `cursor` | x, y, or no arguments (the AUTO grab cursor; drawn on its own overlay above the diagram, not into it - `x,y` shows it there, no arguments hides it) |
| `text` | x, y, string; baseline at y, always in the foreground colour, small font |
| `rtext` | x, y, string; baseline at y, current colour and `font` |
| `stext` | x, y, string, size 0-4; XPP rich text in the foreground colour: backslash `1` symbol (Greek), `0` roman, `s` subscript, `S` superscript, `n` normal |
| `font` | size 0-4, font (0 roman, 1 symbol), colour; for following `rtext` |

### The AUTO diagram as data

Window 101 is drawn with the ops above like any other, and that stays what
it shows (and what PostScript, SVG and the X11 program draw). Beside it the
server sends the diagram's points as data, so that a client can zoom, pan
and name the point under the mouse without a round trip. The data describe
exactly what the drawing shows: after Clear it is empty, after File/Load or
Reset diagram it is unchanged until reDraw, as the picture is.

A point is one `add_point()` of `core/auto_nox.c`, in the quantities the
axes plot (`auto_xy_plot`: the parameter against the maximum, norm,
period, ... of the Axes setting), in the order it was plotted.

- `{"ev":"diagram","op":"axes", xmin, xmax, ymin, ymax, x0, y0, wid, hgt, plot, xlabel, ylabel}`:
  the diagram was drawn again at these axes (`plot` is `Auto.plot`: 0 hi,
  1 norm, 2 hi and lo, 3 period, 4 two parameters, 10 frequency, 11
  average); the points are unchanged. Pixel `x0 + wid*(x-xmin)/(xmax-xmin)`,
  `y0 + hgt - hgt*(y-ymin)/(ymax-ymin)` of window 101 is where the drawing
  has (x, y).
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
| `c`, `lw` | palette colour and line width |
| `new` | 1: the run's first point starts a new line (no line back) |
| `x`, `y` | the values, one per point (`null` for NaN) |
| `y2` | the second value (the minimum, for hi and lo), when it differs from `y` anywhere in the run |
| `lab` | [[index in the run, label, type (`EP`, `LP`, `HB`, `BP`, `PD`, `TR`, `UZ`, `MX`)], ...] for the points whose label the drawing marks (a cross at y and y2, the number at x+8, y+8) |

A run of AUTO sends `add` events as the points come, at most a few a
second like the drawing. A redraw that plots the same points at other axes
(reDraw, Fit, zoom, scroll, a resize) sends only `axes`: the server
compares the points it plots again with what it sent, and only when they
differ (another Axes quantity, new points) does it send `reset` with how
many still agree and `add` for the rest. The points of a redraw go out
when the redraw is over, not while it runs. A new AUTO window starts empty.

`web/xpp-client.js` keeps the data and, over the diagram, zooms with the
wheel and pans with Shift+drag or the middle button, drawing from the data;
a tooltip names the point under the mouse. Anything that makes the core
draw the diagram again (a `clear` op for window 101) shows the core's view
again.

### The plot as data

The new front end (docs/ui-v2.md) draws the plots itself from numbers
instead of replaying drawing ops. Two events carry them, each sent only to a
client that asked with `{"cmd":"data","events":["series","plots"]}` (either
name alone works too), at the end of that command and then at the end of
every command after which what it says has changed. Nothing else sends
them, so a command that only redraws sends none. They come before the
command's `state` and `idle`, `plots` first.

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
time.

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
| `theta`, `phi` | the 3D view's angles in degrees (3d-params, `rotate`) |
| `persp`, `zplane`, `zview` | perspective on (1) or off, and its planes |
| `curves`, `shift` | as in `series` |

Numbers are the doubles themselves, in the shortest of 15 or 17 digits that
reads back exactly; `null` for a value that is not finite. Makewindow
(`m`, then `c` create, `d` destroy the active window, `k` kill all but
window 1) changes the list; `click` with a window's `win` makes it active.

**Binary values.** After `{"cmd":"data","events":["series"],"enc":"f32"}`
every `series` event (full or append) has `"enc":"f32"` and each column's
`data` is a string: the base64 (RFC 4648, with padding) of the values as
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
| `string` | `title`, `name`, `value`, `ok`, `cancel`, `max` | `value` |
| `form` | `title`, `names`, `values`, `max` | `values` (same length). A name starting with `*n` means the field picks from `hello.lists[n]`: a variable (`*0`), a parameter (`*2`), a colour (`*4`), a marker (`*5`), ...; for a list whose items start with a number (`2 Box`) the value is that number. |
| `checklist` | `title`, `names`, `flags` | `flags` |
| `file` | `title`, `file`, `wild`, `dir`, `dirs`, `files` | `file`; or `cd` (a folder name or `..`) or `wild` (a new pattern) to be asked again with that listing |
| `alert` | `button`, `message` | nothing |
| `mouse` | `win` | `x`, `y`; or `xd`, `yd` (data coordinates, below) |
| `rubber` | `win`, `flag` (0 box, 1 line) | `x`, `y`, `x2`, `y2`; or `xd`, `yd`, `xd2`, `yd2` |
| `grab` | `win` | `key`, or `x`, `y` (or `xd`, `yd`) for a click on the diagram |
| `drag` | `win` | `what` (`down`, `move`, `up`), `x`, `y` (or `xd`, `yd`) for each pointer event; cancel or a key ends. Window/Scroll and AUTO Axes/Scroll ask it again after every event. |
| `pixels` | `win`, or `film` (a kinescope frame index) | `w`, `h`, `rgb` (base64 of w*h*3 bytes). Frame, GIF and kinescope writers use it: only the client has the picture. |

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

## Not yet implemented

docs/front-end-gaps.md lists what the X11 front end still does that this
protocol does not.
