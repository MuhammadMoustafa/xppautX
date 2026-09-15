# X11 front end vs the JSON front end

What a user of the X11 windows can do, and whether the protocol front end
(`xppcore-server` + `web/xpp-client.js`, also the VS Code panel) does it.
Checked against the X11 sources window by window; update this file when a
row changes.

## Works the same

| X11 | JSON front end |
|---|---|
| Main, File, Numerics menus, hotkeys, hints | Menu panel and keys |
| Every pop-up menu, string box, form, yes/no, file selector, alert | Dialogs; form fields `*n` pick from the X11 lists; the file selector lists folders and files |
| Plot windows: create, kill, select, zoom, fit, scroll, text/arrows/markers, 3D rotation by dragging | Tabs; drag asks for Scroll; rotate command |
| x,y readout under the mouse (plot and AUTO) | From `state.view` / `state.auto` |
| IC, parameter, BC and delay boxes with %formulas, Default, xvst/pp/arry | Side panel |
| The three parameter sliders, `@ s1=...` presets | Side panel |
| `@ button` user buttons | Side panel |
| Data browser and all its buttons | Data tab |
| Equilibrium window with Import | Top of the side panel |
| Equations window, source viewer with comment actions | Tabs |
| AUTO window: every button, grab, hotkeys, scroll, close, point readout | AUTO tab |
| Animation window: Go, Pause, Fast, Slow, step, slider, Skip, File, Grab, Fly, frame saving, Close, resize | Animation tab |
| Array plot: Redraw, Edit, Print, Fit, Range, GIF, Close, drag to scroll | Array tab |
| Kinescope: capture, reset, playback, autoplay, save, animated GIF | Kinescope tab |
| Calculator | Prompt shows the last answer |
| `-runnow`, tutorial, `-anifile`, errors printed by xppaut | Handled at start; errors shown in the panel |

## Different on purpose

- One window at a time: plot windows, AUTO, animation, array plot, data,
  source and equations are tabs of one area, not separate windows. A
  prompt that wants a click in a window brings its tab forward.
- Values typed in a box take effect when the field is left; there is no
  Ok/Cancel for the whole box.
- Window/Bottom (raise a plot window) does nothing: tabs replace stacking.
- Keys go to the tab that is shown (AUTO hotkeys on the AUTO tab), not to
  the window under the pointer.
- Saved frames and GIFs come from the canvas the client drew, with colours
  rounded to the 216 web-safe ones (the GIF writer takes 256 colours and a
  canvas smooths lines). The X11 files are pixel copies of the window.
- Kinescope frames live in the client: reloading the page loses them.
- The AUTO diagram stores a point for 2-parameter work on click, not on
  every mouse move.

## Not done

- User functions from a DLL/shared library (`load dll`) need the library
  built for the machine that runs the server; the native Windows build
  loads `.dll` files.
- Help (`xpp_hlp`) and `.xpprc` editing start a browser or editor on the
  machine that runs the server, as X11 does; a remote server cannot show
  them to the user.
