# The new front end (web2)

The first browser front end (`web/`, removed at T18) copied the X11 program
pixel for pixel: the core sent drawing primitives in window pixels and the
page replayed them on canvases. The front end in `web2/` replaced it with a
modern interface:
the core sends **data** (the numbers a view shows) and the page draws them
itself, with modern type and rendering, zoom, pan, a point readout, legends,
exports, the browser's own file dialogs, a layout that works from a phone to
a wide desktop, and full keyboard and screen reader support.

This document is the design and the plan. `xppautX model.ode` serves web2
at `/` (T17); protocol 2 (T18) carries data only.

Decisions already taken (not revisited here): data-level plotting; TypeScript,
a light framework and uPlot, bundled with esbuild into the files xppautX
embeds; tests check data and UI state, never pixels; the C core stays C and
the JSON protocol is the seam; `web/` kept working until web2 covered it,
then went too (T18; the X11 program already went, W8); upstream
mergeability is not a goal.

## Contents

1. [What exists](#1-what-exists)
2. [Protocol v2: the data events](#2-protocol-v2-the-data-events)
3. [Commands, prompts and components](#3-commands-prompts-and-components)
4. [Files: open and save with the browser's dialogs](#4-files-open-and-save-with-the-browsers-dialogs)
5. [Architecture of the page](#5-architecture-of-the-page)
6. [Type and theme](#6-type-and-theme)
7. [Responsive layout](#7-responsive-layout)
8. [Accessibility and UX](#8-accessibility-and-ux)
9. [Tests](#9-tests)
10. [Migration plan](#10-migration-plan)

## 1. What exists

| Piece | Where | What it does |
|---|---|---|
| `series` and `plots` events, `data` command | `core/plot_data.cpp` (C API in `plot_data.h`, called from `core/ui_json.cpp`), `core/series_enc.cpp`, docs/protocol.md "The plot as data" | Every plot window's curves as numbers, one `series` per window: T and every plotted column, float32 values printed with 9 digits or base64 float32 (`enc` `f32`), sent at the end of a command when the window's data or curves changed, and for the active window in `append` parts while an integration runs. `plots` lists the windows (axes, labels, 3D view, curves) and the active one. |
| Page | `web2/src/` | Preact + TypeScript. A store fed by protocol events, a session that sends commands, the layout shell, the command menu, the plot (uPlot), prompts as dialogs, notifications, status bar. |
| Build | `web2/build.mjs`, `web2/package.json` | esbuild bundles `src/` into `web2/dist/` (`app.js`, `app.css`, `index.html`, the Inter font and its licence). `dist/` is committed. |
| Embedding | `Makefile` `WEB2_FILES`, `tools/embed.c` | xppautX serves `web2/dist` at `/` (`/v1/` and `/v2/` redirect there). |
| Tests | `web2/test/`, `tools/web2check.mjs`, `tools/servercheck.py`, `tools/webcheck.py` | Reducer and plot-model unit tests; a browser session asserting store and plot state (desktop, keyboard only, 390x844 touch); the `series` numbers against `output.dat`; the page's assets. |

The plot windows are tabs (Makewindow's windows, the core's active one
selected; picking a tab makes it active). Each shows its window's curves (a
phase plane in xy mode, a time plot in aligned mode), starts at the
window's axes (Viewaxes), keeps its own zoom when the tabs switch, and
adds, all in the client without a round trip: drag a box to zoom, the wheel
zooms about the pointer, Shift+drag or the middle button pans, pinch and
one-finger drag on touch, a double click (or `0`) goes back to the window's
axes, Undo zoom (`Ctrl+Z`) steps back, the nearest point is named under the
mouse, on a tap, or by stepping with `[` `]` from the keyboard, curves can be
hidden from the legend, and PNG and CSV export what is shown.

The AUTO view (T11a) opens when the core opens AUTO's window (File/Auto)
and closes when the core destroys it: a panel anchored to the right on
screens from 48 rem, a full-screen sheet on phones, both ending above the
status bar (whose Stop stops a run, A10). It draws the diagram from the
`diagram` events (store/diagram.ts holds them exactly, full and `add`
increments): a curve per branch and stability run (stable solid, unstable
dashed, periodic branches as their maximum and minimum), the labelled
points as crosses with their type and number, and the segment from a Hopf
point to the first point of the periodic branch that starts there (XPP
leaves it blank; the data do not name a branch's starting label, so the
join is made where a periodic branch starts at a Hopf label's parameter and
its max..min spans the label's value). Zoom, pan, reset, undo and the
readout (branch, point, kind, label, values) work as on the plot, with `<`
`>` stepping from label to label; the AUTO window's buttons and hotkeys
send `auto` ops, their prompts are the ordinary dialogs. Back hides the
panel (Show AUTO brings it back), Close is done with AUTO: it stops a
running continuation first, then closes the window.

T11b answers the core's asks on the diagram in the view itself. Grab is a
mode of the diagram (an instruction bar with Take and Cancel): arrows,
`[` `]`, Page Up and Down, Home and End move the core's cursor from point
to point, Tab and Shift+Tab from label to label, Enter takes the point,
Escape cancels, and a click or a tap takes the nearest point; each step is
a `grab` answer with the point's index (`point`), so the cursor, the info
strip and the circle are the core's. Axes/Zoom (and Zoom out) draw their
box, Axes/Scroll its drag, as the plot's modes on the diagram, answered in
its data coordinates; a click on a two-parameter diagram stores the point
(`auto point`, marked on the diagram) for AUTO's File/sElect 2par pt. Under the
diagram, the `autoinfo` data: the strip as a panel (branch, point, type,
label, parameters, norm, the plotted variable, the period of a periodic
orbit) and the stability circle as a small unit circle, a dot for a value
inside, a cross for one outside, with the eigenvalues or multipliers
listed. A periodic branch is joined to the Hopf point its run started from
(the diagram's `from`); only a diagram without it (loaded from a file)
falls back to where the points lie.

### Build and run

Building xppautX needs no Node: `web2/dist` is committed and embedded. Only
someone who edits `web2/src` needs Node 22+:

```bash
cd web2
npm ci                 # pinned versions (package-lock.json)
npm run build          # web2/dist, commit it with the change
npm run check          # fails when dist/ is not what src/ builds (CI runs this)
npm test               # unit tests (store, plot model, keys)
npm run typecheck
npm run watch          # rebuild on save; then make xppautx embeds it
```

Why committed and not built by make: CI and users building from source must
not need npm; esbuild's output is deterministic for pinned versions, so
`npm run check` in CI proves the committed bundle matches the source. The
bundle is ~95 KB of JS (Preact 4 KB, uPlot 50 KB), 11 KB of CSS, 48 KB of font.

## 2. Protocol v2: the data events

### Principles

- **Data beside drawing, then instead of it.** Every view gets a data event
  that says what the view shows, in the model's own quantities (plot
  coordinates, not pixels). The classic `draw` ops kept flowing while `web/`
  existed (the data events went only to a client that asked for them);
  protocol 2 (T18) dropped them.
- **One subscription command.** `{"cmd":"data","events":["series", ...]}`
  declares the set of data events the client wants (`[]` stops them; a new
  command replaces the set). Each named event is sent at the end of that
  command, which is what a client that (re)connects needs. `hello.features`
  lists the names the server knows, so a newer page can tell an older
  server.
- **Sent when it changed.** At the end of a command the server compares what
  it would send with what it sent (a signature, as `diagram` does) and sends
  only on a difference. A command that only redraws sends nothing.
- **Numbers as JSON.** float32 storage values go out with 9 significant
  digits, which round-trip exactly; `null` is NaN. A binary encoding
  (`"enc":"f32"` in the `data` command, base64 of little-endian floats) is
  an option for very long runs, not the default; web2 asks for it.
- **Increments for long runs.** A long integration or AUTO run sends `append`
  parts at most ~10 times a second (like `progress` and `diagram add`), and
  the final state at the end.
- **Every command still ends with `state` then `idle`.** Data events come
  before them.

### The events, in the order to build them

| # | Event | For | Contents | Core work |
|---|---|---|---|---|
| 1 | `series` (**done**) | main plot | the active window's curves: storage columns (T always), colour, line or points, lag shifts, axis labels | done |
| 2 | `series` `op:"append"` | live plot | `from`, the new rows of the same columns, during an integration; the final `series` at the end | yes: from the integrator's plot hook (`plot_the_graphs`), throttled |
| 3 | `plots` (**done**) | plot windows as tabs | per window: `win`, title, 2D/3D, axes ranges (`xlo..yhi`), labels, the 3D box and angles (`theta`, `phi`, `persp`), and its curves; `series` is sent per window (each with its `win`), appends for the active one | done (`core/plot_data.cpp`) |
| 4 | `nullclines` (**done**) | phase plane | per window: the x and y nullclines as flat segment lists `[x1,y1,x2,y2,...]` in plot coordinates, colours, plus the frozen nullclines | `core/phase_data.cpp`, recorded where `nullcline.c` draws them |
| 5 | `dfield` (**done**) | phase plane | direction field: grid of `[x,y,ux,uy]` (unit direction in plot units) and each arrow's speed, scaled by the client; flow: the trajectories as `x`, `y` point lists per curve, NaN between two | `core/phase_data.cpp`, from `redraw_dfield`/`direct_field_com` and the integrator's `plot_one_graph` |
| 6 | `marks` (**done**) | plot | equilibria found by Sing pts (x, y, stability type), labels, arrows and markers of Text,etc, frozen curves (Graphic stuff/Freeze) as series | `core/marks_data.cpp`, recorded where `eq_symb` (graphics.c), `draw_label` (grobs.cpp) and `draw_freeze`/`create_crv` (graf_par.c) draw them |
| 7 | `diagram` + `autoinfo` (**done**) | AUTO view | the diagram's points (and `from`, the label a branch started from); the info strip as fields (branch, point, type, label, parameters, norm, the plotted variable, period) and the stability circle (e^λ or Floquet multipliers `[[re,im],...]`, and a steady state's eigenvalues) | `core/auto_data.cpp`, reported by `auto_nox.c` where it draws the strip and the circle |
| 8 | `browser` (exists) | data table | rows and columns on request: already data | none |
| 9 | `aplot` (exists) | array plot | cells as colour indices; add `values` (the numbers) so the client picks its colour map | small |
| 10 | `ani` `frame` (**done**) | animation | the frame's primitives (line, rect, circle, ellipse, comet dot, text) in unit coordinates of the `.ani`'s `dimension` box (y up, not clamped), their colours (XPP index or `#rrggbb` of the colour map), widths and fonts; the frame's row, time, box and the classic window's size; thinned to 25 a second while Go plays | `core/aniparse.cpp` computes a frame in the `.ani`'s coordinates and gives each primitive to the pixel ops and to `core/ani_data.cpp` (docs/protocol.md "The animation as data") |
| 11 | kinescope | kinescope | nothing new: the client keeps data snapshots (series + marks + viewport) as frames and renders or exports them itself | none (the `pixels` ask stays: web2 renders the picture from its data) |

Equilibrium, equations, source, message, progress and state events are
already data and stay as they are.

### How the two paths coexist

1. Until task T17 both pages work against the same binary: `draw` ops are
   always sent, data events only after `data`. In browser mode both pages
   can even be open at once.
2. T17 makes web2 the page at `/` (classic at `/v1/`), and the VS Code panel
   switches to it.
3. T18 removes `web/` and the `draw`, `palette` and window-size (`size`)
   paths that only it used (the X11 program itself was already removed by
   W8), and `hello`'s `char`; the `XppUi` pixel primitives have no front-end
   implementation any more (headless no-ops; the data modules are fed by
   the code that calls them). `pixels` stays: the core's frame, GIF and
   kinescope writers still ask for a window's picture, and web2 renders it
   from its data. The protocol number is 2 (docs/protocol.md "Removed in
   protocol 2"). State after T18: one page (web2 at `/`, `/v1/` and `/v2/`
   redirect), no `web/`, no webtest.mjs; servercheck, autocheck, webcheck
   and web2check check data events only.

## 3. Commands, prompts and components

### Commands

XPP's single-letter hotkeys stay the command vocabulary: the core owns
every command (`commands.c`), and the menus come from `hello.menus`. The
new page adds direct manipulation that maps onto existing commands:

| UI action | Protocol |
|---|---|
| menu item, hotkey (focus on the page or the plot) | `key` |
| Integrate button | `key` `i`, then answers the menu with `g` (the session's key sequence) |
| edit a parameter or IC, slider, Default | `set`, `slide`, `default` |
| legend toggle, zoom, pan, reset, readout, export | client only |
| "Use this view" (make the client's zoom the window's axes, so PostScript/SVG export and Restore agree) | new `{"cmd":"view","win":w,"xlo":..,"xhi":..,"ylo":..,"yhi":..}` (T9) |
| Abort | `abort` |
| open / save a file | the `file` ask plus the file endpoints (section 4) |

### Prompts (`ask`) as components

| ask kind | Component | Notes |
|---|---|---|
| `menu` | modal menu list (role `menu`), keys shown as `kbd`, the key answers | done |
| `choice` | the same list, with the question | done |
| `string`, `form` | modal form, first field focused and selected, Enter submits (from a select too), Escape cancels | done; a `*n` field is a select of `hello.lists[n]` (`protocol/lists.ts`): a numbered item (`2 Box`) answers its number, a value the list lacks is kept as an option of its own |
| `checklist` | checkbox list with All and None | done |
| `alert` | a notification (toast); the ask is answered at once, so the run is not blocked | done |
| `file` | the browser's open or save dialog (section 4, `ui/FileDialog.tsx`); the core's listing is the second tab, "In the model's folder" | done |
| `mouse`, `rubber`, `drag` | a plot mode (`plot/pick.ts`, the store's `pick`): a crosshair (click or tap picks), a box or line (drag it), or a drag of the plot, with an instruction bar and Cancel (Done for a drag); Escape cancels; from the keyboard, arrows move the crosshair or the free corner (Shift: ten times as far), Enter picks or fixes a corner, arrows drag in a drag. Answered in data coordinates (`xd`, `yd`, `xd2`, `yd2`, docs/protocol.md), so no pixel maths; the drag's events queue while the core works. When the core's window moves (Window/Zoom, Viewaxes), the plot shows it again (the client zoom is one Undo away). Asks for windows the page does not draw yet (AUTO, 3D) say so and offer Cancel | done |
| `grab` (AUTO) | a mode of the AUTO view: arrows, `[` `]`, Tab to the labels, Enter takes, Escape cancels, a click or tap takes the nearest point; answered by index (`point`) | done (T11b) |
| `pixels` | answered `ok:0` by the session: web2 renders frames from data (kinescope, GIF) itself | done / T15 |

A prompt never steals keys it does not use: the menu dialog takes only its
own keys; everything else is ignored while a prompt is open, as the protocol
requires.

## 4. Files: open and save with the browser's dialogs

The core reads and writes files on its own machine, in its working directory
(the model's folder), and many of XPP's files refer to others by relative
name: `.set` files, `.auto` diagrams, `#include`d files, `table` files, data
files for the browser's Load, `-anifile`. The page runs on the same machine
(127.0.0.1) but the browser never tells a page where a picked file lives.

### Options

| Option | For | Against |
|---|---|---|
| A. Keep the core's file selector (restyled) | true paths, relative names work, no copying | not the browser's dialog (the user's decision); a custom file browser is what users dislike in XPP today |
| B. Native OS dialogs opened by the core (`GetOpenFileName`, `zenity`, `osascript`) | true paths | three platforms of C, blocks the core thread, fails when the page is not on the core's machine (VS Code remote), not the browser's dialog |
| C. Browser dialogs, files copied through the page into the model's folder | the browser's own dialogs on every browser, works remote, the model's folder stays the one place XPP reads and writes, so relative names keep working | a copy per open; a save lands in the model's folder as well as where the user chose |
| D. Browser dialogs, files only in the browser (File System Access handles), the core reading them through the page | no copies on disk | the core would need every read and write routed through the page, including relative references it resolves itself; Chromium only |

### Choice: C, the model's folder as the workspace

- **The working directory stays the workspace.** XPP keeps reading and
  writing there, so relative references resolve as they always have, and
  scripts, `-silent` and the VS Code extension see the same files.
- **Open** (a `file` ask for reading: Read set, Load diagram, the browser's
  Load, `session load`): the page shows the browser's picker
  (`showOpenFilePicker` where available, else `<input type=file multiple>`).
  The picked files are uploaded into the working directory; the ask is
  answered with the base name. Picking several files at once (a `.set` and
  the table files it uses) uploads them all, so their relative names
  resolve. A file whose name exists with other content is not overwritten
  without a confirm (Replace, Keep both as `name-2.ext`, Cancel); one with
  the same content is not copied.
- **Save** (a `file` ask for writing: Write set, Save diagram, the browser's
  Write, `session save`, Save info, PostScript/SVG): where the File System
  Access API exists (Chrome, Edge, the VS Code webview), the page shows
  `showSaveFilePicker` with the ask's name suggested; the core writes the
  file into the working directory under that base name, and the page copies
  it to the picked location. Elsewhere (Firefox, Safari) the page asks for
  the name, the core writes it, and the page offers it as a download. Either
  way the working directory has the latest copy, so a later Read set by name
  finds it.
- **Missing companions.** When the core reports it cannot open a file (an
  error naming it), the notification offers "Add file…", which uploads it
  under that name and repeats the command.
- **The core's listing stays reachable** as a secondary tab of the dialog
  ("In the model's folder"), for the rare ask that needs a path elsewhere
  on the core's machine; it answers with `cd` and `file` as today.
- **Endpoints** (xpp_http.cpp, token-protected like `/cmd`, base names only,
  no separators, no `..`, no dot files, a size cap of 64 MB):
  `GET /files` (listing: name, size, mtime, sha-256), `GET /files/NAME`,
  `PUT /files/NAME` (streams the body to a temporary file and renames, so a
  failed upload leaves nothing). The request reader gets a streaming body
  path; today it reads at most 8 KB. `--server` (stdio) clients such as the
  VS Code extension write files themselves; they get
  `{"cmd":"file","op":"put","name":...,"data":base64}` and `get` for
  parity (T5).

### Built (T5)

- **Core**: `core/xpp_files.cpp` (C API in `xpp_files.h`, SHA-256 in
  `xpp_sha256.cpp`) holds the rules and the file work: which names are
  reachable, the listing with digests, reading, and the write through a
  hidden temporary file renamed into place; `xpp_http.cpp` serves it as
  `/files` (its request reader now reads the head, then streams a body of
  any declared length up to the endpoint's cap) and `ui_json.cpp` as the
  `file` command. The `file` ask carries `mode` (`read` or `write`, from
  the selector's title). docs/protocol.md "Files" is the contract.
- **Page**: `ui/FileDialog.tsx` (the two tabs, the replace confirm),
  `pickers.ts` (the pickers, the input and download fallbacks),
  `protocol/files.ts` (the endpoints), `store/files.ts` (pure: names,
  Keep both names, the upload plan, the running command's record and the
  missing file of an error), the session's `openFiles`, `saveFile`,
  `resolveReplace` and `addMissingFile`.
- **Missing companions**: the core's errors do not name the file ("Cannot
  open file"), so the page takes the name its command's `file` ask for
  reading was answered with (or a name the error gives, `File<x> not
  found`). "Add file…" uploads the pick under that name, then sends the
  command again (with the File or nUmerics menu key first when the command
  was in that menu) and answers its prompts as they were answered.

## 5. Architecture of the page

```
protocol/   types.ts (events, commands), transport.ts (SSE + POST),
            decode.ts (JSON or base64 float32 columns, pure), lists.ts
            (form fields that pick from hello.lists, pure)
store/      store.ts (generic store), state.ts (AppState + reducer), series.ts
            (float32 columns; appends fill growing buffers in place),
            plots.ts (the plot windows: each one's series and zoom, the
            active one), values.ts, table.ts, diagram.ts (the AUTO
            diagram's points, labels, axes and the view's zoom)
session.ts  the only sender: commands, key sequences, answers, abort
plot/       model.ts (series -> curves, pure), nearest.ts, viewmath.ts,
            plotKeys.ts (pure), decimate.ts (what of a long curve changes
            pixels, pure), chart.ts (uPlot adapter), interactions.ts
            (mouse, wheel, touch), pick.ts (plot modes of the mouse,
            rubber and drag asks, pure), phase.ts and marks.ts (what the
            chart draws besides the curves and the legend's names for it,
            pure), richtext.ts (XPP's label markup as Unicode runs, pure),
            colors.ts, export.ts, registry.ts,
            diagramModel.ts (diagram -> curves, label marks, Hopf joins,
            nearest point, readout; pure), diagramChart.ts (uPlot adapter)
ui/         App.tsx (shell), TitleBar, MenuPanel, Plots (the windows' tabs),
            PlotView (one window), ValuesPanel, TableView, AutoView (with auto.css), AniView (the
            animation), AskDialog, Toasts, StatusBar, Messages, hotkeys.ts,
            theme.ts, context.ts
ani/        frame.ts (a frame's primitives decoded, the box at the
            dimension's aspect, unit <-> canvas, pixel sizes, pure),
            render.ts (canvas drawing, __xpp.ani())
testhook.ts window.__xpp for tests
```

Rules:

- **One direction.** Transport → session → store → components. Components
  read the store through `useStore(selector)` and act through the session;
  none touches the transport. The chart reports gestures through callbacks
  and never writes the store itself.
- **One job per module.** Pure logic (reducers, the plot model, key maps,
  zoom maths, nearest point) has no DOM and is unit-tested in Node; adapters
  (uPlot, EventSource, downloads) are thin.
- **The store is the truth for anything a test checks**: connection, busy,
  the open prompt, the plot windows (each one's series, viewport and undo
  history, the active one), the readout, notifications, the drawer, the
  theme. A chart's own state (ranges, visible curves) is read through
  `__xpp.plot(win)` (the active window's without `win`).
- **State slices** follow the views: `plots` (windows, T6, done),
  `values` (parameters, ICs, sliders, T3, done), `diagram` (AUTO), `table` (browser),
  `ani` (T13, done), `aplot`, `files` (T5, done). Each gets its reducer file under `store/` and its
  view under `ui/`.

### Libraries

- **Preact** (4 KB) over Svelte: esbuild compiles TSX itself, so the build
  needs no compiler plugin or preprocessor to pin; the runtime is tiny; the
  component model is React's, which most contributors know; the store is
  plain TypeScript, so nothing is tied to the framework.
- **uPlot** (50 KB) for every 2D plot: canvas, fast with 10^5-10^6 points,
  an xy mode for phase planes, hooks for drawing extras. Its xy mode draws
  every segment, so the phase plane's lines and all point curves use path
  builders from `plot/decimate.ts`: a segment that paints no device pixel
  an earlier one has not painted is left out (a limit cycle run a thousand
  times costs one cycle), traced a slice per task for long curves, the last
  complete trace standing in meanwhile. A time plot keeps uPlot's line,
  which keeps a min and max per pixel column. Nullclines,
  direction-field arrows, equilibria and labels draw in its `draw` hook on
  the same canvas; no second library.
- **3D** plots (XPP's 3D is curves in a box) are projected in the client
  with the window's angles on uPlot's canvas; three.js (600 KB) is not worth
  it for line plots.
- **AUTO** uses uPlot too (branches as xy series, labels as points).
- Nothing else at runtime. Dev only: esbuild, TypeScript, @types/node,
  the Inter font package.

## 6. Type and theme

- **Inter** (variable, Latin subset, 48 KB, SIL OFL 1.1) is self-hosted in
  `web2/dist` and embedded in xppautX: the program must work offline, so no
  font or script comes from a CDN. Tabular numerals (`tnum`) keep columns of
  numbers aligned. Code and tables of numbers use the system monospace
  stack (`ui-monospace, Cascadia Code, SF Mono, Menlo, Consolas`). Inter's
  Greek subset (19 KB, `inter-greek.woff2`, loaded only for Greek text)
  draws XPP's symbol-font labels, which the page turns into Unicode Greek
  (`plot/richtext.ts`, T8).
- **Tokens** on `:root` (`web2/src/theme.css`): a type scale
  (0.75/0.875/1/1.125 rem), a spacing scale (0.25..1.5 rem), colours for
  light and `[data-theme=dark]`, the minimum target size.
- **Light, dark or system**: the title bar's Theme button cycles
  Auto → Light → Dark; the choice is remembered in the browser; Auto follows
  `prefers-color-scheme`.
- **Curve colours**: XPP's eleven colour indices (0 the foreground, then
  red .. purple) map to a palette per theme (`plot/colors.ts`), keeping the
  hue XPP names.

## 7. Responsive layout

Rules (each checkable; `tools/web2check.mjs` checks R1, R3 and R5 at 390x844):

- **R1** No horizontal page scroll at any width from 320 px: the page does
  not scroll sideways; grid children have `min-width: 0`; long text wraps
  (`overflow-wrap: anywhere`) or ellipsizes.
- **R2** Sizes are `rem` and `%` (type, spacing, targets, panel widths);
  `px` only for hairlines and canvas text.
- **R3** Breakpoints (`min-width` in rem, so they follow the user's font
  size):
  - below 48 rem (phones, small tablets): one column; the command menu is a
    drawer (the title bar's Menu button; Escape, the scrim or a choice
    closes it, focus moves in and back); the file name and the Classic link
    are hidden; forms stack label over field;
  - 48-80 rem: the menu as a 13 rem column beside the work area;
  - from 80 rem: a 15 rem menu, and room for the values panel (T3) as a
    right column.
- **R4** Components adapt to their own width with container queries (the
  plot's tools wrap under the legend below 34 rem), not to the window.
- **R5** Touch: on a coarse pointer every control is at least 44x44 px; the
  plot takes one-finger pan, two-finger pinch zoom (about the fingers'
  midpoint) and tap to read a point; `touch-action: none` on the plot area
  only, so the page still scrolls elsewhere.
- **R6** Panels still to come (values, AUTO, data table, animation) are
  columns or floating panels on wide screens, tabs under the plot on medium
  ones, and full-screen sheets with a Back button on phones.
- **R7** Dialogs fit the viewport (`max-height: 100%`, scroll inside), with
  a 1 rem margin.

## 8. Accessibility and UX

Target: WCAG 2.2 AA. Rules:

- **A1 Contrast**: text at least 4.5:1 against its background (the tokens:
  `--fg-muted` #525c6b is 6.8:1 on white, dark #a3adbb 7.6:1 on #171b21;
  white on the accent 5.5:1); the focus ring (6.5:1, dark 8.9:1), field
  edges (`--field-border`, 3.5:1 and 4:1) and every curve colour at least
  3:1 against the plot. Buttons are identified by their text, so their
  decorative border may be lighter.
- **A2 Visible focus**: `:focus-visible` draws a 2 px ring in `--focus` on
  every control and on the plot; nothing removes it.
- **A3 Keyboard, all of it**: every command by its hotkey or the menu;
  Tab reaches every control in reading order (a skip link jumps to the
  plot); the plot has its own keys (arrows pan, `+`/`-` zoom, `0` reset,
  `Ctrl+Z` undo, `[` `]` `PageUp` `PageDown` `Home` `End` step through the
  points, `{` `}` change curve, Escape clears); Tab is never taken as an XPP
  key.
- **A4 Dialogs**: `role=dialog`, `aria-modal`, labelled by their title;
  focus moves in (first field, else first control), Tab cycles inside,
  Escape cancels, focus returns to where it was.
- **A5 Names and roles**: landmarks (header, nav, main, footer); menus as
  `role=menu`/`menuitem` with `aria-keyshortcuts`; the plot as
  `role=application` with a label naming its curves and points and a
  description of its keys; the readout and the status as `role=status`
  (announced politely); errors as `role=alert`; legend toggles with
  `aria-pressed`; the menu button with `aria-expanded`/`aria-controls`.
- **A6 Motion**: `prefers-reduced-motion` removes transitions; nothing
  flashes or moves by itself.
- **A7 Colour scheme**: follows `prefers-color-scheme` unless the user
  chose; colour is never the only carrier (curves are named in the legend
  and the readout; hidden curves are struck through as well as faded).
- **A8 Targets**: 32 px with a mouse, 44 px on touch.
- **A9 Consistency**: one spacing and one type scale; buttons, fields and
  dialogs share their shapes; the same action has the same name
  everywhere.
- **A10 Long runs**: the status bar shows Working… with a progress bar and
  the one Stop control (`abort`, also Escape) for whatever runs, in any
  view; it exists only while something runs and turns into a disabled
  "Stopping…" until the run's `idle`. Views have no Abort buttons of their
  own (decision 2026-09-23): a view's close (×) means "done with it", and
  stops a running job first, while Stop keeps the view and its partial
  result (an AUTO branch ending on its EP, to grab and continue). Buttons
  that would queue behind the run (Integrate) are disabled while it runs.
- **A11 Notifications, not modal alerts**: errors and the core's alerts are
  toasts that do not take the focus or stop the run; errors stay until
  dismissed, information for six seconds; all of them also go to Messages.
- **A12 Undo where cheap**: zoom and pan have an undo history (a gesture is
  one step); parameter and IC edits get Undo in T3 (the previous value is
  in the store); Default stays as XPP's reset.
- **A13 Empty and error states**: no data says so and offers Integrate;
  a lost connection shows "Reconnecting…"; a stopped core says so and
  points to Messages; a prompt kind not built yet says so and offers
  Cancel.
- **A14 Text**: sentences in plain words, no jargon the user did not
  type; numbers with six significant digits in readouts, full precision in
  tables and exports.

## 9. Tests

- **Unit** (`npm test`, Node): reducers (the diagram's full and `add`
  events included), the plot model (modes, lag shifts), the diagram model
  (curves by branch and stability, the Hopf join), nearest point, zoom maths, the plot's key map, and the A1
  contrast rules checked on the tokens of `theme.css` and the curve
  palettes.
- **Protocol** (`tools/servercheck.py`): a Window/Zoom box answered in
  data coordinates zooms as the same box in pixels, Initialconds/Mouse in
  data coordinates starts there; `data` sends the series at once;
  after an integration the series is W against V with 601 rows and its
  numbers are exactly those of `output.dat` (as float32, printed `%.8g`); a
  redraw sends none; Xi vs t sends T and V. Live runs
  (tools/models/live.ode, 20 001 rows): several appends, contiguous from
  row 0 (from the rows already there for Continue), their rows the final
  series', nothing after it; `enc` `f32` decodes to the JSON numbers; an
  unsubscribed client gets nothing. Plot windows: `plots` at once, then
  after Makewindow create, destroy and kill all; a new window's own
  series; Xi vs t in one window sends only its series; `click` changes
  `plots.active` and sends no series; a run appends to the active window
  only and ends with every window's series. `tools/webcheck.py`: `draw=0` streams
  carry no drawing.
- **Page** (`tools/web2check.mjs`, headless Chrome or Edge through the
  DevTools protocol, shared driver `tools/cdp.mjs`): real key presses, mouse
  and touch events; assertions read `window.__xpp.state()` (the store),
  `__xpp.actions()` (the actions taken) and `__xpp.plot()` (the chart's
  ranges and curves). Desktop: integrate from the keyboard, the store's
  numbers equal `output.dat`, hover, wheel and box zoom, undo, pan, reset.
  Keyboard only: Tab to the plot, visible focus, every plot key, a prompt's
  focus trap and focus return. Prompts: Viewaxes with a variable picked from
  its select, Window/Zoom by a box drawn by mouse and by keyboard only (the
  core's view is the box within a pixel), Escape cancelling a plot mode
  (answered `ok` 0), Initialconds/Mouse by a click, Window/Scroll by an
  arrow key, a checklist answered. Phone (390x844, touch, coarse pointer): no
  sideways scroll, plot width, 44 px targets, the drawer, pinch, pan, tap.
  AUTO (lecar, examples/scripts/lecar_auto.jsonl's steps from the page):
  the store's diagram equals the `diagram` events (`__xpp.diagramEvents()`
  rebuilt in the test), the chart (`__xpp.diagram()`) has one curve per
  branch and stability run and every label, the periodic branch starts at
  its Hopf point, hover and `<` `>` name the Hopf point, wheel, box, undo,
  pan and reset, no Abort in the view, a sheet at 390x844 with 44 px
  targets, Back and Show, Close; grab from the keyboard only (G, `]`, Tab
  to the Hopf point, Enter) with the store's `autoinfo` following each
  step, the periodic branch run from it and marked `from` its label,
  Escape cancelling a grab, Axes/Zoom by a box drawn on the diagram (the
  core's axes are the box within a pixel). `tools/servercheck.py`: grab by
  index then Run gives the diagram grabbing by keys gives; `autoinfo`
  equals the strip's text and AUTO's printed eigenvalues and multipliers.
  Live: the store and the plot grow over several appends of a 20 001-row
  run and end equal to `output.dat`. Long runs (tools/models/million.ode,
  10^6 rows): every draw under 50 ms while the rows arrive, and during
  wheel zooms of the phase plane and the time plot no draw over 50 ms and
  no long task (`__xpp.plot().drawMs`, `__xpp.longTasks()`). Plot windows:
  New window adds a tab and the store holds both windows' series, Xi vs t
  changes only its window, each tab keeps its zoom across switches (by
  click and by the arrow keys, which also make the window the core's
  active one), no sideways scroll at 390 px with two tabs, Close window
  removes the tab. Marks (T8): a text with Greek, a pointer, a marker, a
  frozen curve and a Sing pts equilibrium in the store and the legend,
  drawn (`__xpp.plot().layers`), toggled, cleared by Erase; servercheck
  compares the equilibrium with the `equilibrium` event and the frozen
  curve with the `series`.
  No screenshot is compared.
- **Assets** (`tools/webcheck.py`): `/v2/`, its script and font with their
  types.
- **Files** (T5): `tools/webcheck.py`: a PUT then GET round-trips binary
  bytes, the listing's digests; traversal (`../x`, `..%2Fx`, `a/b`, `a\b`),
  dot, device and long names (400), a missing or wrong token (403), an
  upload over 64 MB (413), cut short (400) or without a length (411), a
  folder and a symbolic link (403) all refused, leaving no file and no
  temporary file. `tools/servercheck.py`: the ask's `mode`, `file` put, get
  and list, refused names and data. `tests/test_files.cpp`: SHA-256
  vectors, names, the cap, abort and replace, every selector title's mode.
  `tools/web2check.mjs`: Write set lands in the folder and is downloaded
  (the same bytes); Read set by upload restores the parameters; the same
  content is not copied again; the replace confirm (Cancel, Keep both as
  `name-2.set`); "Add file…" after a file the core could not open.
- **Animation** (T13): `tools/servercheck.py`: with tools/gui_test.ani
  (one of every command) on lecar, every primitive of a frame is the
  classic pixel op divided by the window's size within a pixel, colour for
  colour, in [0,1] except where the `.ani` leaves its box (and there the
  pixel op is clamped to the edge); load, step, seek and the end of Go send
  the frame they draw; Go sends fewer frames than it draws, in order, and
  its last; `speed`; a grab by unit coordinates; nothing without `data`.
  `npm test` (`test/ani.test.ts`): decoding, the box's aspect, unit to
  canvas and back, pixel sizes, the slice. `tools/web2check.mjs` (`ani`):
  Load by upload shows frame 0 in the store and drawn, at the box's aspect;
  arrows, Shift, Home, End, the seek slider, the delay, Space and the Play
  button move the store's frame and the drawn one (`__xpp.ani()`); a step
  after Pause goes from the frame shown; nothing plays by itself; at
  390x844 a full-screen sheet with 44 px targets and no sideways scroll.
- Each task below adds its view's checks to web2check and its events'
  checks to servercheck.

## 10. Migration plan

Tasks in dependency order; each is 1-3 days of agent work. "Core" marks
tasks that change C code or the protocol (extend docs/protocol.md and
servercheck.py with them). Every task keeps `tools/verify.sh`,
`npm run check`, `npm test` and `web2check` green and adds its own checks.

| ID | Task | Needs | Core | Acceptance |
|---|---|---|---|---|
| T1 | Scaffold: `series`, web2 build/embedding, shell, the main plot, tests | - | yes | done: this branch |
| T2 (**done**) | Live plotting: `series` `append` during integrations (throttled), store appends in place, binary option for long runs | T1 | yes | a 20 000-row run grows on screen; servercheck: the appended rows equal the final series; a 10^6-row series renders and zooms without a frame over 50 ms |
| T3 (**done**) | Values panel: parameters, ICs, BCs, delays, sliders (`@ s1=`), user buttons, Default, `%formula`, undo of an edit | T1 | no | web2check: edit a parameter, see `state`; move a slider, get a new series; undo restores; keyboard and 44 px targets; right column at 80 rem, sheet on a phone |
| T4 (**done**) | Prompts complete: `*n` selects, checklist, mouse/rubber/drag asks as plot modes; core accepts data coordinates `xd`,`yd` | T1 | small | servercheck: an answer in data coordinates; web2check: Viewaxes form with a variable select; Window/Zoom by a box drawn on the plot, by mouse and by keyboard |
| T5 (**done**) | Files: `/files` endpoints (list, get, put; streaming bodies), `file` asks through the browser's dialogs, the confirm on replace, "Add file…" for missing companions, `file` commands for `--server` | T4 | yes | webcheck: traversal and dot names refused, 64 MB cap, token required; web2check: Write set lands in the model's folder and is offered to the browser; Read set by upload restores parameters |
| T6 (**done**) | Plot windows: `plots` event, series per window, tabs, Makewindow create/kill/select | T2 | yes | servercheck: two windows, each with its curves; web2check: switch tabs, each keeps its zoom |
| T7 (**done**) | Nullclines and direction fields as data (`nullclines`, `dfield`), drawn in uPlot's draw hook | T6 | yes | servercheck: segment counts equal the classic draw ops' lines for lecar; web2check: the store holds them, they toggle in the legend |
| T8 (**done**) | Marks: Sing pts equilibria, Graphic stuff text/arrows/markers, frozen curves; Greek labels as Unicode | T6 | yes | servercheck: `marks` after Sing pts has the equilibrium's coordinates; web2check: marks listed in the store and the legend |
| T9 (**done**) | Use this view: `view` command sets the window's axes from the client's zoom; Fit | T6 | small | servercheck: `view` then `state.view` matches; PostScript export uses it |
| T10 (**done**) | Data table: virtualized browser table on `browser`, its buttons, CSV export, keyboard navigation | T3 | no | web2check: scroll to row 500, Get sets the ICs, keyboard reaches every button |
| T11a (**done**) | AUTO view from `diagram`: branches by stability, labels, zoom, pan, readout; buttons (no Abort: the status bar's Stop, A10) | T4 | no | web2check: after an AUTO run the store's diagram equals the `diagram` events; readout names a labelled point |
| T11b (**done**) | AUTO grab by point, `autoinfo`, stability circle as data | T11a | yes | servercheck: grab by index then run; web2check: grab from the keyboard |
| T12 (**done**) | Array plot view from `aplot` (with `values`), colour maps, scroll | T6 | small | web2check: cells equal the event's; scroll in time |
| T13 (**done**) | Animation: frames in unit coordinates, player controls, scaling to any size | T6 | yes | servercheck: frame primitives in [0,1]; web2check: play, pause, step, seek update the frame index |
| T14 (**done**) | 3D plots: projection and rotation in the client, angles synced with `rotate` | T6 | small | web2check: rotate by drag and keys; `state.view.three` agrees |
| T15 (**done**) | Kinescope and exports from data: capture snapshots, play, GIF/PNG from the client | T7, T8 | small | web2check: capture two frames, play them; GIF export has two frames |
| T16 (**done**) | Text views: equations, source with actions, equilibrium details, messages | T3 | no | web2check: comment action sets its parameters |
| T17 (**done**) | Switch: web2 at `/`, classic at `/v1/`; VS Code panel; docs (using-the-panel, front-end-gaps, README) | T3-T16 | small | every row of docs/front-end-gaps.md covered by web2; webshots runs against `/v1/` until T18 |
| T18 | Retire: remove `web/`, the `draw`/`palette`/`pixels` paths (the X11 program and guicheck were already removed by W8); protocol 2 | T17 | yes | verify.sh green without web/; servercheck and web2check cover what webshots did |
| T19 (**done**) | From hands-on testing: runs accumulate until Erase (XPP's behaviour), Erase blanks web2's plot, any number of sliders under the plot with ranges from the value, edits while busy coalesce into one run, ICs show initial conditions, collapsible sections with Save/Load | T15 | yes | web2check: two runs keep two curves, Erase clears, 5 edits while busy give one run, slider range, section save/load round trip |
