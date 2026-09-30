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

- **`xppautX model.ode -silent`**: no interface at all; loads the model,
  does whatever the ODE file's `@` options, an options file, or further
  command-line flags
  ([Quick reference](16-quick-reference.md#command-line-arguments)) tell
  it to (typically: integrate) and writes `output.dat`, then exits. It
  runs these as the commands the page sends (Initialconds/Go, Save data,
  ...), so a run from the shell and the same run in the page do the same
  thing. This is what to use from a shell script or a test.
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
The bar keeps a fixed height and, at its right end, a fixed-width slot
for the progress bar and Stop/"Stopping…", reserved whether or not a run is going, so a
command starting or ending never resizes the bar or shifts the plot
above it.
What XPP prints appears under **Messages** at the bottom of the page as
well as in the terminal ("The log", below); a problem it reports (an
illegal formula, a value out of bounds, a file it cannot read) shows as a
dialog with an OK button (Enter or Escape closes it; errors that arrive
together are lines of one dialog; each also stays in Messages), while a
warning only flashes the status bar. One status message is shown at a
time; starting the next command clears it. If the model cannot be loaded
at all, the program keeps serving the page so you can read what it
printed, and the top of the page says where the problem is: the file
(the model's, or a file it includes), the line number with the line as
you wrote it (and, in an `.odex` model, a caret under the column), and
what is wrong, such as a formula that stops making sense and where.
Correct the line and start XPP again.

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
  **← Use current state** sends the same keys, `i` then `l`: it copies Now
  into Initial and runs (a button sends its key's command; there is no
  copy-without-running).
- A value is set when you leave the field (Tab, Enter or a click
  elsewhere): the field shows it, and the next computation uses it. The
  values are *settings*: you can edit them while a computation runs too,
  and the edit is taken at once but applied when that computation ends —
  the run in progress keeps the values it started with, the next one
  uses yours. There is nothing pending to send: the value shown is the
  value. There is no `Ok`/`Cancel` for the whole box, as in the
  X11 windows; `Escape` puts back the value that was there. Each changed
  field has a reset button whose tooltip shows the ODE file's value, and
  there is no undo: reset (one field, or **Reset all**) is the way back.
- A parameter or initial condition takes a number or a `%formula`,
  exactly as the X11 boxes did (see "Formulas as values", below):
  `%2*pi`; a boundary condition or delay takes an expression. Anything
  else (letters in a parameter box) is refused, not sent: see "What a box
  accepts", below.
- **Reset all** puts back the values from the ODE file, as the X11
  Parameter window's Default button did.
- **The checkboxes** next to the variables pick what **x vs t**,
  **Phase** and **Array** plot, like `xvst`, `pp` and `arry` in X11.
- **Sliders** sit under the plot, any number of them (the X11 main
  window had three), including the ones an ODE file sets with `@ s1=...`;
  each is added or edited with a dialog (searchable variable, min, max,
  step/precision), not the small binding window upstream describes.
  Dragging one changes the value as it moves (during a run too, for the
  next run); nothing runs until you run it.
- **Buttons the ODE file defines** (`@ but=name:keys`) appear above the
  sliders.
- **Boundary conditions** and **Delay initial data** are collapsed
  sections; delays appear only for delay equations.
- **Numerics** holds the main numerical parameters of the Numerics menu
  ([Numerical parameters](06-numerical-parameters.md)): Total, Start
  time, Transient, Dt, the nullcline mesh, the equilibrium (Sing pt)
  controls, nOutput, Bounds, the Method (a list) and its tolerances and
  step limits, the maximal delay (delay equations only) and the boundary
  value controls. A field the current method does not use is greyed. They
  are settings like the others: editable during a run, for the next one.
  A value the program does not take is refused on the field with its
  reason (`Dt must be a number other than 0`).
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
window's axes, the nearest point is named
under the mouse, on a tap, or by stepping with `[` `]` from the keyboard,
curves can be hidden from the legend, and CSV export (written by xppautX
itself, then offered as a download) what is shown. The zoom shown and
the earlier runs (drawn lighter until Erase, with a legend toggle) are
held by the core's window and sent as data, so a saved session
(File/saVe session) keeps the zoom and the toggle; the earlier runs
themselves are not saved. There is no zoom
history: "Reset view" and the **Fit** button
below are the way back. "Use this view" makes the client's current zoom
the window's axes, so PostScript/SVG export and Restore agree with
what's on screen.

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

## What a box accepts

Every box checks what you type as you type it, by what it asks for:

| the box asks for | it takes | for example |
|---|---|---|
| a whole number | digits, with a sign | Nmax, Ntst, a range's Steps, the animation's frame skip |
| a number | `1e-3`, `-0.5`, `.5` | a window's limits, slider Min, Max and Step, AUTO's Ds, Mark values |
| a number or %formula | a number, or `%` and a formula | parameters, initial conditions, nUmerics' Total, Dt, ... |
| an expression | a formula, its brackets matched | boundary conditions, delays, a column's formula, the calculator |
| a name | one of the model's names (it suggests them) | Xi vs t, the data browser's Replace and Tabulate |
| a file name | a name only: no folder, no leading dot, none of `\ / : * ? " < > \|` | Save's file name, the frames' base name |
| text | anything | labels, search boxes |

Some also have a range (Ncol from 2 to 7, a Step above 0). A keystroke, a
paste or a drop that would leave a box with text it never takes, and that
is not on the way to text it does (a stray letter in a number box, say,
or a second decimal point), is refused outright: the box's text does not
change, and a brief message under it names what was wrong (typing
`0.05abc` into a number box types `0.05` and no more; pasting `0.05abc`
over a box leaves it as it was and names the pasted text and where in it
the trouble starts). The box is not marked invalid for this, since its
actual text is still fine.

A box that is left with text that is not what it takes once you leave it
or press Enter — a half-typed number (`-`, `1e-`, a lone `%`) has a red
border and says what is missing ("`1e-` needs an exponent's digits"); a
`%formula` XPP itself refuses once you leave the field (an unknown name,
say) is marked the same way, with XPP's own message. Such a text is never
sent: Enter does nothing, a dialog's OK is disabled, and leaving the box
keeps the text there, still marked, until you correct it or press Escape,
which puts back the value that was there — always the box's own key
first, wherever the focus is: even a panel that closes on Escape (the
values panel as a narrow-screen sheet, say) lets a marked box inside it
drop its own text on the first Escape before a second one closes the
panel. Nothing is silently changed back. The core's own asks say what
each field wants (the protocol's `kinds`); a field they do not describe
takes any text, as before.

## Long-running commands

There is no per-view Abort button (upstream's Escape-to-abort still
works from the keyboard). The status bar's **Stop** is the one control
for whatever is running, in any view; it exists only while something
runs and turns into a disabled "Stopping…" until the run's next idle
point. A view's own close (×) means "done with it": it stops a running
job first, then closes; **Stop** itself keeps the view and whatever
partial result it has (an interrupted AUTO branch ending on its last
point, ready to Grab and continue).

While a computation runs (an integration, a range, Sing pts, a
boundary value problem, an AUTO run), the status bar says what
(`Running Go… Esc stops`). What you can do meanwhile depends on what an
action is; every button and menu item has one of five kinds:

| Kind | Examples | During a computation |
|---|---|---|
| Control | Stop (Escape), Quit, answering a question | works |
| View | zoom, pan, the legend, switching plot windows, New window, Window/zoom, Viewaxes, Xi vs t, Help, opening or closing a panel, a menu with anything in it that is a view (Nullcline, Dir.field, Kinescope, Graphic stuff) | works; one the program itself carries out (a menu item, New window) runs as soon as the computation ends |
| Setting | the values panel's fields and sliders (parameters, initial and boundary conditions, delays, Numerics), Parameters, the Numerics menu's items, File/Get par set, AUTO's Parameter, Numerics and Mark values | works: taken at once, applied when the computation ends (the run in progress keeps its values); a menu item's dialog opens then |
| Data | Save and Load of values' files, Write set and Read set, every file written, Save session, AUTO's Save and Load diagram, AUTO's Grab | disabled |
| Computation | Integrate and Initialconds, Continue, Range, AUTO's Run, Nullclines, Dir.field and Flow, Sing pts (equilibria), Stochastic, a model's own buttons | disabled |

A disabled control says so in its tooltip ("Not while a computation
runs: available when it ends"), and a key of those kinds typed during a
computation does nothing and is not kept to act afterwards; Escape is
Stop. Menus still open: only their items that would save, load or
compute are greyed out. Nothing is disabled merely because the program
is busy for a moment otherwise (catching up with a zoom or a window you
picked): a click then is carried out in its turn, never lost. While the
program asks you something, only answering or cancelling that question
applies. Typing ahead into menus still works: I then G typed quickly
integrates, because the G answers the menu the I opens. Settings stay
usable during a run: an edit is sent at once and shown as the value,
and it applies when the run ends, never to the run in progress; so do
AUTO's Parameter, Numerics and Mark values forms (for the next AUTO
run).

## Saving pictures and files

Files (PostScript/SVG, GIF, `.dat`, `.set`, tables, kinescope frames) are
written next to the ODE file, by the program, exactly as in X11. The
browser never silently downloads anything on its own.

- **Open** (Read set, Load diagram, the browser's Load, ...): the page
  shows a prompt whose "Choose file…" opens the browser's own file picker, showing
  the files of the kind the command reads (`*.set`, ...); picked files are uploaded into
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

In the desktop window (not the browser), every Open and Save shows the
operating system's own file dialog instead: it starts in the model's
folder with the suggested name, lists the files of the kind the command
reads or writes (All files is one choice away, for the tables a `.set`
uses), and xppautX reads or writes the file right where you picked it,
with nothing copied into the model's folder. Cancel there cancels the
command. A save onto an existing file asks "File Exists! Overwrite?" in
the page, as before, rather than in the dialog. File > Open model… uses
the same dialog.

Two differences from X11 worth knowing:

- Pictures saved as GIF or PPM (kinescope, animation frames, array plots)
  are written by xppautX itself, but from pixels the browser drew, not a
  copy of an X11 pixmap, and their colours are rounded to 216 shades (the
  GIF format takes 256 colours, and a browser smooths its lines).
- Kinescope frames live in the page as data (not bitmaps): reloading the
  page loses them.

## No compiler needed

Everything in xppautX works without a compiler. A model that called a
compiled C library (`export`, `dll_lib`/`dll_fun`, a network's
`import`) no longer loads: those statements were removed.

File > Help opens this manual in the page's Help view. Its About button shows the version, the author's contact details and where to report a problem (the desktop window's Help > About shows the same text; in the window the addresses are plain text to select and copy, in a browser they are links). "Edit .xpprc"
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

You can enter a formula instead of a plain number in the values panel and
in the prompts that ask for one number (the "number or %formula" boxes
above: nUmerics' Total, Dt, ...): the first character must be `%`, e.g.
`%2*pi` or `%sin(1.5)`; it is evaluated and converted to a number when
the field takes effect. A form's number fields (a window's limits, AUTO's
Numerics) take plain numbers only.
