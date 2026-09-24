# Introduction

## What xppautX is

XPPAUT (also called XPP; the two names are interchangeable) is
G. Bard Ermentrout's tool for ordinary differential equations, difference
equations, delay and functional equations, boundary value problems and
stochastic equations, with the bifurcation program AUTO built in. It
handles up to 5000 differential equations, has solvers for stiff and
delay systems, computes equilibria, invariant sets, nullclines and
Poincaré maps, and post-processes data with histograms, FFTs and a
curve fitter. **xppautX** is XPPAUT with its X11 interface (Xlib menus and
windows) replaced by a browser front end, **web2**
([Using the interface](04-using-the-interface.md)): the same menus,
sub-menus and single-letter hotkeys, running without an X server, on
Linux, macOS and Windows. The numerics — the parser, the solvers, AUTO —
are XPPAUT's own code, unchanged: every example model's output is
compared against a saved checksum (`tools/verify.sh`) so a refactor
cannot silently change a result.

XPPAUT grew out of a program called PHASEPLANE and thirty years of
development by Bard Ermentrout, with contributions credited in the
source and upstream documentation. The full story, and a complete
description of the model language and every command, is in his book,
*Simulating, Analyzing, and Animating Dynamical Systems: A Guide to
XPPAUT for Researchers and Students* (SIAM, 2002; see `CITATION.cff`).
The original manual and its LaTeX/PDF sources are kept for reference in
`docs/upstream/`; this manual (chapters 2 onward) is that text, updated
only where the interface changed (see `docs/manual/README.md`'s
"Credit").

## Getting it

Each [release](https://github.com/MuhammadMoustafa/xppautX/releases) has
one archive per platform (Linux, Windows, macOS). Unpack it and run the
program on a model — nothing else needs installing, no X server, no
Node:

```bash
./xppautX examples/ode/lecar.ode          # Linux/macOS
xppautX.exe examples\ode\lecar.ode        # Windows
```

macOS and Windows show an unsigned-binary warning the first time; the
main [README](../../README.md#installing-a-release) explains it. To
build from source instead, see the main
[README's Building section](../../README.md#building); it is not
repeated here.

## Starting it

`xppautX model.ode` loads the model and opens **web2** in a window of
its own, titled "xppautX — model.ode", with the system's own web view
(WebView2 on Windows, WebKitGTK on Linux; on macOS the page opens in
the browser until the window is tested there). Its menu
bar holds what belongs to the app rather than the model: **File** (Open
model…, which starts a second xppautX with the model you pick, in its own
window; Quit) and **Help** (Manual, Keyboard shortcuts, About: version,
commit, compiler, protocol version and license). The model's own menus
stay inside the page. Closing the window quits xppautX, as the page's
File/Quit does; after an error that stops the model, the window stays
open on the page's Messages until you close it.

The other modes:

```bash
./xppautX --browser model.ode         # web2 in your default browser; prints its address
./xppautX --no-open model.ode         # the same, printing the address without opening it
./xppautX model.ode -silent           # batch run, writes output.dat, no interface
./xppautX --server model.ode          # the JSON protocol on stdin/stdout
```

`--browser` (or `--web`) is the way to reach the page from somewhere
else (a remote machine through an SSH tunnel, the VS Code extension, a
test): it prints `XPP: http://127.0.0.1:PORT/?t=TOKEN`, the address with
the session's token, and opens it. The window never shows that address.
If the web view cannot start (no WebView2 runtime on Windows, no display
on Linux), xppautX says so in its log and uses the browser instead. A
Linux build made without WebKitGTK (`libwebkit2gtk-4.1-dev`) has no
window at all and always uses the browser; `xppautX --help` says which
you have.

To try the window by hand: start `xppautX examples/ode/lecar.ode`; the
window opens with the xppautX icon and title; Help > Manual and Help >
Keyboard shortcuts open the Help view in the page, Help > About shows
the version box; File > Open model… starts a second window with the
model you choose; File > Quit, or closing the window, ends xppautX and
leaves no process behind.

`--server` is for a front end that embeds xppautX instead of opening a
browser tab (the VS Code extension, a test script); the protocol itself
is in [docs/protocol.md](../protocol.md). `--script FILE model.ode`
replays a recorded protocol session from FILE instead of reading
commands from stdin (docs/protocol.md "Scripts"), which is how
regression tests and recorded sessions are replayed without a live
client.

`--verbose` and `--debug` raise how much xppautX logs (parser stats, the
startup banner, AUTO's table, solver chatter); by default it logs only
warnings and errors. Without `-logfile`, that log goes to the terminal,
and the browser front end also shows it live in the page's Messages
panel; `-logfile FILE` sends it to FILE instead, so a run started that
way has nothing in Messages. See
[Using the interface: the log](04-using-the-interface.md#the-log).

### Command-line options

xppautX's own options (`--browser`, `--web`, `--server`, `--script`,
`--port`, `--no-open`, `--version`, `--help`, `--verbose`, `--debug`)
must come first; every
other classic `xppaut` option (`core/comline.c`) still works and can
follow in any order. The options below still do something in xppautX;
a few classic options that only ever changed X11 window colours, fonts
or icon state (`-forecolor`, `-backcolor`, `-backimage`, `-mwcolor`,
`-dwcolor`, `-grads`, `-width`, `-height`, `-bigfont`, `-smallfont`,
`-white`, `-allwin`, `-bell`, `-xorfix`, `-ee`, `-iconify`) are still
accepted for compatibility but have nothing left to affect.

| Option | Does |
|---|---|
| `-silent` | Batch run: no interface, integrates and exits |
| `-runnow` | Runs the model immediately on startup (implied by `-silent`) |
| `-outfile FILE` | Write batch output to FILE instead of `output.dat` |
| `-noout` | Suppress writing rows to the output file |
| `-parfile FILE` | Load parameter values from FILE before starting |
| `-icfile FILE` | Load initial conditions from FILE before starting |
| `-setfile FILE` | Load a set file before starting |
| `-readset FILE` | Load a set file the way an internal set is loaded |
| `-with "STRING"` | Apply STRING as if it were an internal set |
| `-internset <0\|1>` | Run (1) or skip (0) the model's internal sets in batch |
| `-uset NAME` | Include the named internal set in a batch run |
| `-rset NAME` | Exclude the named internal set from a batch run |
| `-include FILE` | Include FILE, as the ODE file's `#include` would |
| `-qsets` / `-qpars` / `-qics` | Query internal sets, parameters or initial conditions to the output file, then exit |
| `-equil <0\|1>` | Write equilibria to `equil.dat`, and with `1` the invariant manifolds too |
| `-mkplot` | Produce a plot in batch mode |
| `-plotfmt <svg\|ps>` | Batch plot format |
| `-dfdraw N` / `-ncdraw N` | Draw the direction field / nullclines in batch, to screen or file |
| `-newseed` | Randomize the random number generator's seed |
| `-convert` | Convert an old-style (PHASEPLANE) ODE file to current syntax |
| `-anifile FILE` | Load an animation script (`.ani`) at startup |
| `-quiet <0\|1>` | Suppress the model's own console messages (independent of `-verbose`) |
| `-logfile FILE` | Send console output to FILE |
| `-verbose` / `-debug` | Raise the log level (`xppautX`'s `--verbose`/`--debug` do the same) |
| `-version` | Print the version and exit |

Running xppautX with an unrecognized option prints this same list.

## What is different from XPPAUT

web2 keeps every menu, sub-menu and hotkey, but the interaction is a
browser page, not X11 widgets: dialogs, the values panel, plots, the
data browser, animations and the AUTO view all work differently in
detail. [Using the interface](04-using-the-interface.md) describes the
current front end; [docs/front-end-gaps.md](../front-end-gaps.md) is the
row-by-row record of what changed and why.

## The resource file and environment variables

A file named `.xpprc` in your home directory (`$HOME` on Linux/macOS,
`%USERPROFILE%` on Windows) can hold options you always want, one `@`
line per xppautX invocation's worth of settings, for example:

```
# xpprc file
@ but=quit:fq
@ maxstor=50000,bell=0
@ meth=qualrk,tol=1e-6,atol=1e-6
```

xppautX still reads a few environment variables, all optional:

| Variable | Purpose |
|---|---|
| `XPPHELP` | An HTML help file for the core's `File` `Help` command to open in a browser; the page's own **Help** (this manual, built in) does not need it |
| `XPPBROWSER` | Browser that opens `XPPHELP` (Linux/macOS only; Windows uses the system default) |
| `XPPEDITOR` | Editor "Edit your .xpprc preferences file" (menu shortcut `fx`) opens |
| `XPPSTART` | Folder the file dialogs open to, e.g. a shared course directory |

Set them the usual way for your shell (`export XPPHELP=...` in
`.bashrc`, `setx XPPEDITOR ...` or a Windows Environment Variables
dialog). Nothing else — no `DISPLAY`, no X resources, no font or window
colour settings — is needed or read any more.

## License

xppautX is distributed as is, with no warranty of performance; see the
`LICENSE` file for the full terms. Bugs and feature requests are welcome
on the project's [issue tracker](https://github.com/MuhammadMoustafa/xppautX/issues).

## Where to go next

- [ODE Files](02-ode-files.md) and [Examples](03-examples.md) — the model
  language and worked models
- [Using the interface](04-using-the-interface.md) — web2 in detail:
  starting xppautX, the page layout, plots, dialogs, files, the log
- [The main commands](05-commands.md) and
  [Numerical parameters](06-numerical-parameters.md) — every command and
  every numerics setting
- [The Data Browser](07-data-browser.md),
  [Functional equations](08-functional-equations.md),
  [Auto interface](09-auto.md) and
  [Creating Animations](10-animations.md) — the specialized views
- [Creating C-files for faster simulations](11-dll-libraries.md) and
  [C Files](15-generated-c-files.md) — compiling a model's right-hand
  sides for speed
- [Quick reference](16-quick-reference.md) — the ODE file cheat sheet,
  built-in functions and the full command-line option list

See [docs/manual/README.md](README.md) for the complete chapter list and
the menu/dialog-to-section map.
