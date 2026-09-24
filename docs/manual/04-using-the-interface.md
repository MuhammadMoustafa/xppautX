# Using the interface

Upstream XPPAUT's interface was a set of X11 windows: a main window with
its own menu, and separate windows for parameters, initial data, the
data browser, AUTO, and so on, each independently resizeable and
iconifiable. This fork (xppautX) replaced that with **web2**, a single
responsive page served by the program itself and opened in your browser.
It is the same XPPAUT underneath: the same menus, the same single-letter
hotkeys, the same numerics ([The main commands](05-commands.md),
[Numerical parameters](06-numerical-parameters.md)). This chapter covers
what looks and behaves differently from the X11 windows the rest of this
manual otherwise describes; where a feature has no web2 equivalent yet,
it says so plainly instead of describing the old window.

## Starting xppautX

    xppautX examples/ode/lecar.ode      # the page in a window of its own
    xppautX --browser model.ode         # in your browser: prints http://127.0.0.1:8765/?t=... and opens it
    xppautX --port 9000 model.ode       # another port
    xppautX --no-open model.ode         # print the address, open it yourself
    xppautX --verbose | --debug model.ode  # raise the log level (see "The log", below)
    xppautX --version                   # which release this is

Everything runs on your machine: the page is served on 127.0.0.1 only,
at an address with a one-time token. Closing the window quits xppautX
(see [Starting it](01-introduction.md#starting-it) for its menu bar). In
browser mode, closing the tab does not stop xppautX at once; press
`Ctrl+C` in the terminal, or use `File` `Quit` in the page. The VS Code extension
(docs/vscode-extension.md) shows the same page in a panel and starts the
program for you.

xppautX also has two front-end-free modes, for scripting and automated
checks rather than interactive use:

- **`xppautX model.ode -silent`**: no interface at all
  (`xpp_batch_main`); loads the model, does whatever the ODE file's `@`
  options, an options file, or further command-line flags
  ([Quick reference](16-quick-reference.md#command-line-arguments)) tell
  it to (typically: integrate) and writes `output.dat`, then exits. This
  is what to use from a shell script or a test.
- **`xppautX --server model.ode`**: the same JSON protocol web2 speaks
  over HTTP, instead over stdin/stdout, for a process that wants to drive
  xppautX directly (docs/protocol.md is the contract). `--script FILE`
  replays a recorded session of that protocol (used by this project's own
  checks); a recorded `{"cmd":"abort",...}` line replays to the same
  point in a run, not just to some arbitrary later one, so an interrupted
  integration or AUTO run reproduces exactly (docs/roadmap.md W10).

`--web` (browser mode, opening the page) is the default when none of
`--server`, `--script` or `-silent` is given.

## The page layout

The page has a title bar (the model's name, the connection state, a
Classic-style link is not offered — web2 is the only page), the **main
menu** as a panel (a column beside the plot from 48 rem wide, a drawer
below that, opened from the title bar), the plot area with its tabs, and
a **status bar** along the bottom that shows "Working…" with a progress
bar and one **Stop** control whenever a command is running (in any view;
there is no per-view Abort button — see "Long-running commands" below).
What XPP prints appears under **Messages** at the bottom of the page as
well as in the terminal ("The log", below); a problem it reports (an
illegal formula, a value out of bounds, a file it cannot read) shows as a
toast notification and stays until dismissed. One message is shown at a
time; starting the next command clears it. If the model cannot be loaded
at all, the program keeps serving the page so you can read what it
printed.

## Tabs instead of windows

The plot, AUTO, the animation, an array plot, the data browser, the
equations and the ODE source are **tabs** above the plot area, not
separate windows (docs/front-end-gaps.md "Different on purpose"). A tab
comes forward by itself when its window opens in X11 terms, or when XPP
waits for a click or a key in it (grabbing a point in AUTO, for example).
Keys go to the tab you are looking at: with the AUTO tab in front, `a`,
`n`, `r`, `g`, `d`, `c`, `u`, `p` and `f` are AUTO's own keys, as they are
in the X11 AUTO window. `Window/Bottom` (raise a plot window) does
nothing: tabs replace stacking, and there is nothing to iconify or
resize — a tab always fills the space it has.

The equilibrium box, which upstream XPP keeps as its own small window,
appears at the top of the right-hand panel, next to the initial
conditions it can import into (`(I)nitial conds` `s(H)oot`, see
[The main commands](05-commands.md)).

## The values panel

In place of the separate parameter, initial-data, delay and boundary
value windows, one **values panel** holds them all, always visible
beside the plot (a sheet on a phone):

- **Parameters** and **State** are always visible. State has two
  columns: **Initial**, the initial conditions you edit, and **Now**, the
  last point of the latest run (read only). `Go` runs from Initial;
  `Last` (Initialconds/Last) copies Now into Initial, then runs;
  **← Use current state** copies Now into Initial without running.
- A value takes effect when you leave the field (Tab, Enter or a click
  elsewhere), so typing a value and clicking `Integrate` uses the value
  you typed — there is no `Ok`/`Cancel` for the whole box, as in the X11
  windows; `Escape` puts back the value that was there. Each changed
  field has a reset button whose tooltip shows the ODE file's value.
- A field takes a number or a `%formula`, exactly as the X11 boxes did
  (see "Formulas as values", below): `%2*pi`.
- **Default** puts back the values from the ODE file, as the X11
  Parameter window's Default button did.
- **The checkboxes** next to the variables pick what **x vs t**,
  **Phase** and **Array** plot, like `xvst`, `pp` and `arry` in X11.
- **Sliders** sit under the plot, any number of them (the X11 main
  window had three), including the ones an ODE file sets with `@ s1=...`;
  each is added or edited with a dialog (searchable variable, min, max,
  step/precision), not the small binding window upstream describes.
  Dragging one changes the value and integrates again.
- **Buttons the ODE file defines** (`@ but=name:keys`) appear above the
  sliders.
- **Boundary conditions** and **Delay initial data** are collapsed
  sections; delays appear only for delay equations.
- **Data** opens the Data tab ([The Data Browser](07-data-browser.md)),
  **Equations** lists the equations (the X11 equation-listing window).

Sections can be collapsed and their state (and the sliders) saved to and
loaded from a settings file.

## Plots and axes

Each plot window from X11 (a phase plane in xy mode, a time plot in
aligned mode) is a tab, starting at the window's own axes
(`Viewaxes`), keeping its own zoom while tabs switch, and all done
without a round trip to the core: drag a box to zoom, the wheel zooms
about the pointer, Shift+drag or the middle button pans, pinch and
one-finger drag on touch, a double click (or `0`) goes back to the
window's axes, `Ctrl+Z` undoes a zoom step, the nearest point is named
under the mouse, on a tap, or by stepping with `[` `]` from the keyboard,
curves can be hidden from the legend, and PNG and CSV export what is
shown. "Use this view" makes the client's current zoom the window's axes,
so PostScript/SVG export and Restore agree with what's on screen.

A **Fit** button sits in the plot's own top-right corner, over the chart
itself, so it stays in reach after a scroll or a zoom that loses the data
— not just in the toolbar above the plot, which keeps its own Fit too. It
does what `Window` `Fit` does: sets the window's axes to the data's
extent. 3D plots have the same corner button, since `Window` `Fit` fits
their box the same way.

Nullclines, direction fields, equilibria (Sing pts), Text/etc labels and
markers, and frozen curves all draw on the same plot from data the core
sends (docs/ui-v2.md sections 2 and 3), not as separate drawing
operations, so they scale and theme with the rest of the page.

3D plots are XPP's curves-in-a-box, projected and rotated in the browser
with the window's angles; rotate by dragging or the keys that rotated the
X11 window.

## The AUTO view

See [Auto interface](09-auto.md#the-auto-view).

## The animation view

See [Creating Animations](10-animations.md#the-animation-view).

## Dialogs and prompts

Every X11 pop-up menu, string box, form, yes/no and file selector is now
a dialog: `role=dialog`, focus moves to its first field, Tab cycles
inside it, Escape cancels, and focus returns to where it was. A form
field that picked from a fixed X11 list (`*n` in the protocol) is now a
proper select; a mouse-driven prompt (pick a point, drag a box, drag the
plot) is a mode of the plot itself, with an instruction bar and Cancel,
answered by clicking, tapping or the keyboard (arrows move a crosshair or
corner, Enter picks or fixes it) — never in raw pixels, so the same
prompt works with a mouse, a trackpad or touch.

## Long-running commands

There is no per-view Abort button (upstream's Escape-to-abort still
works from the keyboard). The status bar's **Stop** is the one control
for whatever is running, in any view; it exists only while something
runs and turns into a disabled "Stopping…" until the run's next idle
point. A view's own close (×) means "done with it": it stops a running
job first, then closes; **Stop** itself keeps the view and whatever
partial result it has (an interrupted AUTO branch ending on its last
point, ready to Grab and continue). Buttons that would queue behind the
run (`Integrate`) are disabled while it runs, and other clicks show
"Busy — press Stop to stop" instead of doing nothing.

## Saving pictures and files

Files (PostScript/SVG, GIF, `.dat`, `.set`, tables, kinescope frames) are
written next to the ODE file, by the program, exactly as in X11. The
browser never silently downloads anything on its own.

- **Open** (Read set, Load diagram, the browser's Load, ...): the page
  shows the browser's own file picker; picked files are uploaded into
  xppautX's working directory (the model's folder) so relative names in
  `#include`, tables and diagrams keep resolving as they always have. A
  name that already exists with different content asks to Replace, Keep
  both, or Cancel.
- **Save** (Write set, Save diagram, PostScript/SVG, ...): where the
  browser supports it, a native Save dialog is offered with the name
  suggested; xppautX writes the file into the working directory and the
  page then offers (or directly saves) that same copy. Elsewhere the page
  offers the file as a download. Either way the working directory has
  the latest copy, so a later Read by name finds it.
- **Missing companions**: when xppautX reports it cannot open a file, the
  notification offers "Add file…", which uploads it under that name and
  repeats the command.
- **The core's own file listing** stays reachable as a second tab of the
  dialog ("In the model's folder"), for the rare case that needs a path
  elsewhere on the machine running xppautX.

Two differences from X11 worth knowing:

- Pictures saved as GIF or PPM (kinescope, animation frames, array plots)
  come from what the browser drew, not a copy of an X11 pixmap, and their
  colours are rounded to 216 shades (the GIF format takes 256 colours,
  and a browser smooths its lines).
- Kinescope frames live in the page as data (not bitmaps): reloading the
  page loses them.

## What still needs a compiler

A model that loads user C functions (`load dll`,
[Creating C-files for faster simulations](11-dll-libraries.md)) needs
that library built for the machine that runs xppautX; the Windows build
loads `.dll` files. Everything else in XPPAUT works without any
compiler.

File > Help opens this manual in the page's Help view. "Edit .xpprc"
opens an editor on the machine that runs the program, as in X11 (the
`XPPEDITOR` environment variable, [Introduction](01-introduction.md)). If
you ever run the server on another machine (a remote VS Code session,
say), the editor appears there, not in front of you.

## The log

What xppautX prints — including AUTO's table (`Output`, in the AUTO
view) — goes to **Messages** at the bottom of the page, as well as to
the terminal or `-logfile`'s file, quiet by default (warnings and
errors); `--verbose`/`--debug` raise the level (core/xpp_log.h). The
core never prints to stdout or stderr directly, so nothing bypasses
Messages.

## Formulas as values

You can enter a formula instead of a plain number in the values panel or
almost any dialog field that asks for one: the first character must be
`%`, e.g. `%2*pi` or `%sin(1.5)`; it is evaluated and converted to a
number when the field takes effect.
