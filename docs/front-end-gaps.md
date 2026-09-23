# X11 front end vs web2

What a user of the X11 windows (removed, issue #20) could do, and whether
**web2** (`xppautX`, the default page at `/`; docs/ui-v2.md) does it now.
The classic page (`web/xpp-client.js`, legacy, kept at `/v1/` until
docs/ui-v2.md task T18 removes it) is the same JSON protocol front end
this file used to track; it is no longer the subject of this file.
Checked against the X11 sources window by window; update this file when a
row changes.

## Works the same

| X11 | web2 |
|---|---|
| Main, File, Numerics menus, hotkeys, hints | Menu panel and keys |
| Every pop-up menu, string box, form, yes/no, file selector, alert | Dialogs; form fields `*n` pick from the X11 lists; the file selector lists folders and files |
| Plot windows: create, kill, select, zoom, fit, scroll, text/arrows/markers, 3D rotation by dragging | Tabs; drag asks for Scroll; rotate command; 3D projected and rotated in the client (T14) |
| x,y readout under the mouse (plot and AUTO) | From `state.view` / `state.auto` |
| IC, parameter, BC and delay boxes with %formulas, Default, xvst/pp/arry | Side panel (values panel, T3) |
| The three parameter sliders, `@ s1=...` presets | Side panel |
| `@ button` user buttons | Side panel |
| Data browser and all its buttons | Data tab (virtualized table, T10) |
| Equilibrium window with Import | Text tab's Equilibrium view (T16) |
| Equations window, source viewer with comment actions | Text tab (T16) |
| AUTO window: every button, grab, hotkeys, scroll, close, point readout | A window of its own, floating over the page (T11a/T11b): drag its title bar, pull its corner to resize, and the main plot and the value panels stay in view beside it (a tab in the narrow layout) |
| Animation window: Go, Pause, Fast, Slow, step, slider, Skip, File, Grab, Fly, frame saving, Close, resize | Animation tab (T13) |
| Array plot: Redraw, Edit, Print, Fit, Range, GIF, Close, drag to scroll | Array tab (T12) |
| Kinescope: capture, reset, playback, autoplay, save, animated GIF | Captured as data and replayed by the page; GIF and PNG made in the page (T15) |
| Calculator | Prompt shows the last answer |
| `-runnow`, tutorial, `-anifile`, errors printed by xppaut | Handled at start; errors shown in the panel |

## Not yet covered

Nothing. Fixes from hands-on use are tracked in docs/ui-v2.md (T19).

## Different on purpose

- One window at a time: plot windows, animation, array plot, data, source
  and equations are tabs of one area, not separate windows. A prompt that
  wants a click in a window brings its tab forward. AUTO is the exception:
  it floats over the page, and its position and size are remembered. Under
  760px there is no room beside the plot, so there it is a tab too.
- Values typed in a box take effect when the field is left; there is no
  Ok/Cancel for the whole box.
- Window/Bottom (raise a plot window) does nothing: tabs replace stacking.
- Keys go to the tab that is shown, not to the window under the pointer.
  AUTO's hotkeys are the exception: they work while the pointer or the
  focus is on the AUTO window, as X11 does.
- Saved frames and GIFs come from the canvas the client drew, with colours
  rounded to the 216 web-safe ones (the GIF writer takes 256 colours and a
  canvas smooths lines). The X11 files are pixel copies of the window.
- Kinescope frames live in the client: reloading the page loses them.
- Names (up to 64 characters) are shown in full, clipped with a tooltip
  where a column is narrow. The X11 boxes show the first 9 characters and a
  `~` for a longer name; AUTO's printed column headings (14 wide in both)
  do the same at 12 characters.
- The AUTO diagram stores a point for 2-parameter work on click, not on
  every mouse move.
- The AUTO diagram also zooms (mouse wheel) and pans (Shift+drag, or the
  middle button) in the client, from the diagram's data, without asking the
  core; a tooltip names the point under the mouse (branch, point, type,
  label, values). reDraw, or anything else that redraws the diagram, goes
  back to the core's view, which Axes/Zoom, Fit and Scroll still change as
  in X11.
- While a run is going, other clicks show "Busy — press Abort to stop"
  instead of doing nothing. ABORT itself shows "Stopping…" (in the AUTO
  status footer for an AUTO run, the main hint otherwise) and is disabled
  until the run's `idle`. The AUTO window's × and Close, clicked while busy,
  send Abort and close once the run's `idle` arrives, instead of being
  dropped.

## Not done

- User functions from a DLL/shared library (`load dll`) need the library
  built for the machine that runs the server; the native Windows build
  loads `.dll` files.
- Help (`xpp_hlp`) and `.xpprc` editing start a browser or editor on the
  machine that runs the server, as X11 does; a remote server cannot show
  them to the user.
