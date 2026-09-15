# xppcore-server protocol

`xppcore-server file.ode [xppaut options]` loads the model the way `xppaut`
does and then talks line-delimited JSON: one object per line, UTF-8, on
stdin (commands, `"cmd"`) and stdout (events, `"ev"`). stderr carries the
core's own log output. The implementation is `core/ui_json.c`;
`tools/servercheck.py` is a working client, `web/xpp-client.js` a full one.

The core is single-threaded. A command runs to completion, then the server
sends `state` and `idle`. While a command runs the server can stop and
**ask** the client something (a menu, a prompt, a mouse click); it waits for
the matching `answer` and ignores other commands except `size`, `state` and
`quit`. During long computations it polls for `key` (Escape aborts, like
the X11 program) and `abort`.

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
| `answer` | `id`, `ok` (0/1), plus the kind's fields | Reply to an `ask`. Omitting `ok` means ok. |
| `size` | `win`, `w`, `h` | Canvas size in pixels; the plot is redrawn. For the AUTO diagram (`win` 101) the size includes the axis margins and applies when the current command ends; the server answers with `window` `create` for 101 and redraws the diagram. |
| `set` | `kind` (`par`, `ic`, `bc`, `delay`), `name` or `index`, `value` or `text` | Change a value (no redraw or rerun). `text` is what the X11 box takes: a number or `%formula` for `par` and `ic`, an expression for `bc` and `delay`. BCs and delays go by `index` (BC names all read `0=`). A formula that does not evaluate gives `message` `error`. |
| `default` | `kind` (`par` or `ic`) | The Default button: values from the ODE file. |
| `slide` | `name`, `value`, `rerun` (default 1) | A parameter slider moved: set the parameter or variable, then clear and integrate again. |
| `userbut` | `index` | An `@ button` of the ODE file (`hello.userbuttons`). |
| `plotvars` | `how` (0 x vs t, 1 phase plane), `names` | The IC box's xvst/pp buttons for the checked variables. |
| `browser` | `from`, `count`, `col`, `ncol` | The data browser block the client shows (answered at once with `browser`, even during a prompt); `count` 0 stops the updates. |
| `browser` | `op` (`find`, `get`, `replace`, `unreplace`, `table`, `load`, `write`, `first`, `last`, `restore`, `addcol`, `delcol`), `row` | A data browser button, with `row` the selected row (the X11 browser's top row). |
| `eqimport` | | The equilibrium window's Import: the last equilibrium becomes the initial conditions. |
| `equations` | | Send `equations`. |
| `action` | `index` | Run the action of comment `index` of `source.comments`. |
| `click` | `win` | The user selected plot window `win`. |
| `redraw` | | Redraw the active plot window, and the AUTO diagram when AUTO is open (for a client that reconnects). |
| `state` | | Send `state` now. |
| `auto` | `op`: `param`, `axes`, `numerics`, `run`, `grab`, `usr`, `clear`, `redraw`, `file` | The AUTO window buttons. |
| `ani` | `op`: `step` (`n`), `reset`, `file`, `close` | The animation window buttons. |
| `abort` | | Stop a running computation. |
| `quit` | | Exit immediately. |

## Events (server to client)

| ev | fields | meaning |
|---|---|---|
| `hello` | `protocol`, `title`, `file`, `char` {`w`,`h`,`bw`,`bh`}, `menus`, `userbuttons` [name...], `sliders` [{`name`,`lo`,`hi`}...] | First event. Text is laid out on a `char.w` x `char.h` monospace cell. `sliders` are the ones the ODE file sets (`@ s1=...`). |
| `palette` | `colors` (256 `#rrggbb`) | Colour table; sent again after a colormap change. |
| `window` | `op` (`create`, `select`, `destroy`), `win`, `w`, `h`, `title` | Plot windows 1..10, AUTO 101 (stability circle 102, info strip 103), animation 104. |
| `draw` | `win`, `ops` | Drawing, see below. |
| `state` | `pars` [[name,value]...], `ics` [[name,value]...], `bcs` [[name,text]...], `delays` [[name,text]...] (delay equations only), `rows`, `menu`, `win` | Current values; `rows` is the number of stored time points, `menu` the active main menu (0 main, 1 file, 2 numerics), `win` the active window. |
| `idle` | | The command finished. |
| `menu` | `which` | The main menu switched (0 main, 1 file, 2 numerics). |
| `title` | `text` | Title of the selected plot window: what it plots (`W vs V`). The server also labels unlabelled 2D axes with the plotted variables. |
| `message` | one of `error`, `bottom`, `box`, `xy`, `auto`, `calc` | Status text. `box` with empty text removes a hint box. |
| `progress` | `n`, `of` | Computation progress, at most 10 a second. |
| `equilibrium` | `type`, `cplus`, `cminus`, `rplus`, `rminus`, `im`, `values` | Result of Sing pts. |
| `source` | `lines`, `comments` [[text, has action]...] | File/Prt src. |
| `equations` | `lines` | One `dX/dT=...` line per equation. |
| `browser` | `rows`, `cols` (names, `T` first), `row0` (selected row), `start`, `end` (the First..Last range), `from`, `col`, `data` | Rows `from`.. as [T, column `col`, `col`+1, ...]; `null` for NaN. Sent for a `browser` block request and after any command that changed the data while the client shows the browser. |
| `ping` | | Beep. |
| `bye` | | The program is exiting. |
| `ask` | `id`, `kind`, ... | See below. |

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
| `cross` | x, y (the AUTO grab cursor, drawn with XOR: drawing it twice erases it) |
| `text` | x, y, string; baseline at y, always in the foreground colour, small font |
| `rtext` | x, y, string; baseline at y, current colour and `font` |
| `stext` | x, y, string, size 0-4; XPP rich text in the foreground colour: backslash `1` symbol (Greek), `0` roman, `s` subscript, `S` superscript, `n` normal |
| `font` | size 0-4, font (0 roman, 1 symbol), colour; for following `rtext` |

### Asks

| kind | fields | answer fields |
|---|---|---|
| `menu` | `name`, `title`, `items`, `keys`, `hints`, `def` | `key` (empty or `ok:0` cancels) |
| `choice` | `title`, `question`, `choices`, `keys` | `key` |
| `string` | `title`, `name`, `value`, `ok`, `cancel`, `max` | `value` |
| `form` | `title`, `names`, `values`, `max` | `values` (same length). A name starting with `*n` means the field names a variable (`*0`), colour (`*4`) or marker (`*5`). |
| `checklist` | `title`, `names`, `flags` | `flags` |
| `file` | `title`, `file`, `wild`, `dir` | `file` |
| `alert` | `button`, `message` | nothing |
| `mouse` | `win` | `x`, `y` |
| `rubber` | `win`, `flag` (0 box, 1 line) | `x`, `y`, `x2`, `y2` |
| `grab` | `win` | `key`, or `x`, `y` for a click on the diagram |

## Not yet implemented

Kinescope playback and saving, array plots and window scrolling send a
`message` `error` instead of working, and there is no animation playback
loop (`Go`) yet. docs/front-end-gaps.md lists what the X11 front end still
does that this protocol does not.
