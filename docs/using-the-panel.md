# Using XPP in a browser

`xppautX model.ode` starts XPP and opens the front end in your browser.
Everything runs on your machine: the address it prints is only reachable
from this computer and carries a one-time token.

```bash
xppautX examples/ode/lecar.ode      # prints http://127.0.0.1:8765/?t=... and opens it
xppautX --port 9000 model.ode       # another port
xppautX --no-open model.ode         # print the address, open it yourself
xppautX model.ode -silent           # no interface at all: writes output.dat
```

Closing the browser tab does not stop XPP; press Ctrl+C in the terminal, or
use File / Quit in the page. The VS Code extension shows the same front end
in a panel and starts the program for you.

It is the same XPPAUT: the same menus, the same single-letter hotkeys, the
same numerics. This page is only about what looks different from the X11
windows.

## Tabs instead of windows

The plot, AUTO, the animation, an array plot, the data browser, the
equations and the ODE source are tabs above the plot area, not separate
windows. A tab comes forward by itself when its window opens, or when XPP
waits for a click or a key in it (grabbing a point in AUTO, for example).

Keys go to the tab you are looking at: with the AUTO tab in front, `a`, `n`,
`r`, `g`, `d`, `c`, `u`, `p` and `f` are AUTO's own keys, as they are in the
X11 AUTO window.

The equilibrium box appears at the top of the right-hand panel, next to the
initial conditions it can import into.

## The right-hand panel

- **Initial conditions and parameters** are always visible. A value takes
  effect when you leave the field (Tab, Enter or a click elsewhere), so
  typing a value and clicking Integrate uses the value you typed. There is
  no Ok/Cancel for the whole box; Escape puts back the value that was there.
- A field takes a number or a `%formula`, as the X11 boxes do: `%2*pi`.
- **Default** puts back the values from the ODE file.
- **The checkboxes** next to the variables pick what **x vs t**, **Phase**
  and **Array** plot, like xvst, pp and arry in X11.
- **Sliders** are the three parameter sliders of the X11 main window,
  including the ones an ODE file sets with `@ s1=...`. Dragging one changes
  the value and integrates again.
- **Buttons the ODE file defines** (`@ but=name:keys`) appear above the
  sliders.
- **Boundary conditions** and **Delay initial data** are collapsed sections;
  delays appear only for delay equations.
- **Data** opens the data browser, **Equations** lists the equations.

## Saving pictures and files

Files (PostScript, GIF, `.dat`, `.set`, tables, kinescope frames) are written
next to the ODE file, by the program, exactly as in X11. The browser never
downloads anything.

Two differences:

- Pictures saved as GIF or PPM (kinescope, animation frames, array plots)
  come from what the browser drew, and their colours are rounded to 216
  shades. The GIF format takes 256 colours, and a browser smooths its lines.
- Kinescope frames live in the page. Reloading the page loses them.

## What still needs a compiler

A model that loads user C functions (`load dll`) needs that library built
for the machine that runs `xppautX`; the Windows build loads `.dll`
files. Everything else in XPPAUT works without any compiler.

Help and "Edit .xpprc" open a browser or an editor on the machine that runs
the program, as in X11. If you ever run the server on another machine, they
appear there, not in front of you.

## If something goes wrong

What XPP prints appears under **Messages** at the bottom of the page as well
as in the terminal, and a problem it reports (an illegal formula, a value
out of bounds, a file it cannot read) is shown in red at the top. One
message is shown at a time; starting the next command clears it. If the
model cannot be loaded at all, the program keeps serving the page so you can
read what it printed.
