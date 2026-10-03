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
(WebView2 on Windows, the system's WKWebView on macOS, WebKitGTK on
Linux). Its menu
bar holds what belongs to the app rather than the model: **File** (Open
model… and Reload, the page's File/open Model and File/rEload, below;
Quit) and **Help** (Manual, Keyboard shortcuts, About: version,
commit, compiler, protocol version and license) — except on macOS, whose menu bar
holds only Quit xppautX (Cmd+Q) yet (untested there beyond CI). The model's
own menus stay inside the page. Closing the window, or its File > Quit
(on macOS also Cmd+Q), asks first, as the page's File/Quit does ("Quit
xppautX? Save this session first?": Save session, Don't save, Cancel).
It asks without stopping what is computing: the run goes on under the
question, and Cancel leaves it running; Save session stops it, saves and
quits; Don't save quits at once, even from a computation that does not
respond. After an error that stops the model, the window
stays open on the page's Messages until you close it (a model that does
not load shows its problem's line at the top of the page).

The other modes:

```bash
./xppautX --browser model.ode         # web2 in your default browser; prints its address
./xppautX --no-open model.ode         # the same, printing the address without opening it
./xppautX model.ode --silent           # batch run, writes output.dat, no interface
./xppautX --server model.ode          # the JSON protocol on stdin/stdout
```

`--browser` (or `--web`) is the way to reach the page from somewhere
else (a remote machine through an SSH tunnel, the VS Code extension, a
test): it prints `XPP: http://127.0.0.1:PORT/?t=TOKEN`, the address with
the session's token, and opens it. The window never shows that address.
Closing the browser tab ends xppautX about two seconds later (a reload
inside that time keeps the session), and the browser first asks "Leave
site?", since the session is lost; xppautX's own questions cannot be shown
once the tab is gone. The desktop window needs no such question.
If the web view cannot start (no WebView2 runtime on Windows, no display
on Linux), xppautX says so in its log and uses the browser instead. On
macOS the window is always available (the system's own WKWebView);
`WINDOW=0` at build time forces browser-only there. On
Linux the window needs WebKitGTK 4.1 installed; without it xppautX still
starts, says in its log (the terminal and the page's Messages) which
command installs it on your system (for instance `sudo apt install
libwebkit2gtk-4.1-0` on Debian and Ubuntu, `sudo dnf install
webkit2gtk4.1` on Fedora, `sudo pacman -S webkit2gtk-4.1` on Arch,
`sudo zypper install libwebkit2gtk-4_1-0` on openSUSE) and uses the
browser; the next start after installing it opens the window. A Linux
build made without WebKitGTK (`libwebkit2gtk-4.1-dev`) has no window at
all and always uses the browser; `xppautX --help` says which you have.

To try the window by hand: start `xppautX examples/ode/lecar.ode`; the
window opens with the xppautX icon and title; Help > Manual and Help >
Keyboard shortcuts open the Help view in the page, Help > About shows
the version box; File > Open model… loads the model you choose in the
same window, after asking; File > Quit, or closing the window, asks
whether to save the session first, then ends xppautX and
leaves no process behind. On macOS, which has no File/Help menu bar of
its own yet, close the window instead to quit, and use the model's own
menus inside the page.

**Double-clicking a .ode file** (or a `.snapx` session file, a `.recx` recording, which opens in the player, below)
opens it the same way, once xppautX is registered as its opener: run
the matching script in `tools/associate/`
once (Windows: `xppautx-associate.ps1 -Register`, a per-user registry
entry, no admin rights; Linux: `install-linux.sh`, a `.desktop` file and
MIME type under `~/.local/share`; macOS: the release's `.dmg`, or `make
app`, gives `xppautX.app`, with the types declared in its `Info.plist`).
Each script has an `-Unregister`/`--uninstall` counterpart. A second
`.ode` opened this way starts a second xppautX, its own window; File >
Open model… loads one in place of the current model instead. On macOS,
where Finder hands a file to the running app rather than to a new
program, the first double-click starts `xppautX.app` on that model, and
a later one opens in the same window as File > Open model… does, asking
first; starting the app on its own shows the Open dialog.

**Opening another model, reloading this one.** xppautX serves one model
at a time. Opening a `.ode` in any mode converts and saves `.odex` beside
it, which becomes the model from then on; see [.odex](02-ode-files.md#odex).
File/open Model (`F M` in the page, File > Open model… in
the window's menu bar) picks a `.ode` or `.odex` file (or a `.snapx`
session file, a `.recx` recording, below) and asks first: the current model's data and AUTO diagram
go, so it offers **Save session** (a session file, as File/saVe session
writes it) or **Don't save**; Escape keeps the current model (the
question every way of leaving a session asks, File/Quit's too). The new model is loaded from its own folder,
which becomes the folder the page's files are in, and every window of
the model before closes. File/rEload (`F E`, File > Reload) reads the
model's own file again, with the command line it was started with,
after the same question: edit
the `.ode` in your editor, then Reload. Its parameters, initial data
and numerics keep the values you gave them, by name; a parameter or
variable the file no longer has is left out, and one it adds comes with
the file's value. A model that cannot be loaded (a mistake in the file,
a file that is gone) changes nothing: an error says so, the log says
why, and the model before goes on as it was.

**Continuing where you stopped: session files.** File/saVe session (`F
V`) writes everything you would need to pick up tomorrow into one file,
`name.snapx`: the model itself (its `.odex`, and every file it read: the
files it includes, its tables), the
parameters, initial data and numerics, every plot window (its axes,
variables, zoom and the earlier-runs toggle), the text, arrows, markers
and frozen curves, AUTO's diagram, settings and view, and the data
table. File/opeN session (`F N`), a double-click on the file, or
`xppautX name.snapx` loads the model saved in it, from the file alone
(the `.ode` may have changed since, or be gone: the saved one is the one
loaded, and the title says so, `lecar.ode (saved in lecar.snapx)`), and
restores it all as it was saved; AUTO can grab a point of the restored
diagram and go on. The file's folder becomes the working folder, where
what you save goes; nothing is written beside it. Opening one asks first
whether to save the session open, since it takes its place. A session file without
its model (one saved before this version) is refused with an error, and so
is one with a part missing or damaged (a member cut short, a line that is
not a number, a variable the model does not have): the error names the
part and its line (`s1.snapx/windows.set:12: ...`), and the
session open stays exactly as it was, nothing of the file taken. Every
file of xppautX's loads this way, all or nothing: a set, parameter or
initial-conditions file, an AUTO file or AUTO settings file and a
recording are read and checked whole first, and a bad value anywhere in
one, even on its last line, is an error naming the file and that line,
with nothing of the file applied. The
earlier runs a window shows until Erase are
not saved (the data table is the last run's), nor are Sing pts'
equilibrium symbols. A data table above 50 MB makes Save session ask
whether to leave it out (Go computes it again). A `.snapx` is a zip of
ordinary files: renamed to `.zip`, its `model/` folder holds the model's
files, its `model.set` holds the values and numerics (the set format) and its
`data.npz` reads in NumPy (`numpy.load`). A `.set` file is no longer a
file xppautX saves or opens: the session holds what it held, and
File/Import XPPAUT set checks the names in the one XPPAUT wrote and converts it immediately to a `.snapx` session beside it, which is then open. AUTO's File > Save diagram and Load diagram save and open the session too.
Import diagram reads a foreign XPPAUT `.auto` into the model open.

`--server` is for a front end that embeds xppautX instead of opening a
browser tab (the VS Code extension, a test script); the protocol itself
is in [docs/protocol.md](../protocol.md). `xppautX run.recx --silent`
loads the recording's snapshot, plays its steps without an interface,
and exits 0 when they played cleanly or 1 otherwise. Recorded aborts stop
at their exact row or AUTO point (docs/protocol.md "Playing a recording
without an interface"). Its plain-name output files remain in the current
folder. Existing files ask `NAME exists. Replace it? [y/N] (60 s)`; a line
on the terminal or piped stdin answers. EOF or timeout refuses replacement
and stops playback with exit 1. `--auto` replaces them without asking.

`--verbose` and `--debug` raise how much xppautX logs (parser stats, the
startup banner, AUTO's table, solver chatter); by default it logs only
warnings and errors. Without `--logfile`, that log goes to the terminal,
and the browser front end also shows it live in the page's Messages
panel; `--logfile FILE` sends it to FILE instead, so a run started that
way has nothing in Messages. See
[Using the interface: the log](04-using-the-interface.md#the-log).

### Command-line options

Word options require two dashes; single letters use one (`-h`). Old single-dash words stop and name their new spelling. X11 options are removed and report ?no such option?. `--setfile` remains an import of XPPAUT `.set` files (W156, #208).

xppautX's own options (`--browser`, `--web`, `--server`,
`--port`, `--no-open`, `--version`, `--help`, `--verbose`, `--debug`)
must come first; every
supported model option (`core/comline.cpp`) uses two dashes and can
follow in any order. The options below control the model and its run.

| Option | Does |
|---|---|
| `--silent` | Batch run: no interface, integrates and exits |
| `--runnow` | Runs the model immediately on startup (implied by `--silent`) |
| `--outfile FILE` | Write batch output to FILE instead of `output.dat` |
| `--noout` | Suppress writing rows to the output file |
| `--parfile FILE` | Load parameter values from FILE before starting |
| `--icfile FILE` | Load initial conditions from FILE before starting |
| `--setfile FILE` | Import a set file XPPAUT wrote before starting |
| `--readset FILE` | Load a set file the way an internal set is loaded |
| `--with "STRING"` | Apply STRING as if it were an internal set |
| `--internset <0\|1>` | Run (1) or skip (0) the model's internal sets in batch |
| `--uset NAME` | Include the named internal set in a batch run |
| `--rset NAME` | Exclude the named internal set from a batch run |
| `--include FILE` | Include FILE, as the ODE file's `#include` would |
| `--qsets` / `--qpars` / `--qics` | Query internal sets, parameters or initial conditions to the output file, then exit |
| `--equil <0\|1>` | Write equilibria to `equil.dat`, and with `1` the invariant manifolds too |
| `--mkplot` | Produce a plot in batch mode |
| `--plotfmt <svg\|ps>` | Batch plot format |
| `--dfdraw N` / `--ncdraw N` | Draw the direction field / nullclines in batch, to screen or file |
| `--newseed` | Randomize the random number generator's seed |
| `--convert` | Convert `.ode` to `.odex`; terminal name questions have a 60-second deadline |
| `--check` | Validate `.ode` or `.odex` in memory; JSON diagnostics, no window or written files; exit 0 clean/info, 1 warnings, 2 error |
| `--auto` | Answer every terminal question: accept suggested conversion names and replace silent playback outputs |
| `--anifile FILE` | Load an animation script (`.ani`) at startup |
| `--quiet <0\|1>` | Suppress the model's own console messages (independent of `--verbose`) |
| `--logfile FILE` | Send console output to FILE |
| `--verbose` / `--debug` | Raise the log level (`xppautX`'s `--verbose`/`--debug` do the same) |
| `--version` | Print the version and exit |

An unrecognized option stops with ?no such option? and its name; `--help` prints the option list.

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
| `XPPEDITOR` | Editor "Edit your .xpprc preferences file" (menu shortcut `fx`) opens |
| `XPPSTART` | Folder the file dialogs open to, e.g. a shared course directory |

Set them the usual way for your shell (`export XPPSTART=...` in
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
- [Quick reference](16-quick-reference.md) — the ODE file cheat sheet,
  built-in functions and the full command-line option list

See [docs/manual/README.md](README.md) for the complete chapter list and
the menu/dialog-to-section map.
