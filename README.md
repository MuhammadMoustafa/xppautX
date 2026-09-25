# xppautX

A modernization of [XPPAUT](https://sites.pitt.edu/~phase/bard/bardware/xpp/xpp.html),
Bard Ermentrout's ODE / phase-plane / bifurcation tool.

The goal is a cross-platform XPPAUT that runs natively on Windows, macOS and
Linux without an X server, keeps every feature and every single-letter menu
hotkey of the original, and exposes the numerics as a library that other
tools (for example a VS Code extension) can drive directly.

Current status: **phase 3 steps 1-5 done, legacy front end removed
(issue #20)**. `make lib` builds `build/obj/libxppcore.a` and `make xppautx`
builds **`xppautX`**, the one program, which takes what to do from
the command line:

```bash
./xppautX examples/ode/lecar.ode           # the front end in your browser
./xppautX examples/ode/lecar.ode -silent   # no interface: writes output.dat
./xppautX --server examples/ode/lecar.ode  # the JSON protocol on stdin/stdout
```

`-silent` writes an `output.dat`; `tools/verify.sh` checks it against a
known-good checksum. Names in a model (variables, parameters, auxiliaries,
functions and their arguments, tables) can be up to 64 characters long
(`XPP_NAME_MAX` in `core/xpplim.h`); XPPAUT 8 cut them to about 10.

It needs nothing else (no X server, no Node), builds natively on Windows,
and covers every feature of classic XPPAUT
([docs/front-end-gaps.md](docs/front-end-gaps.md) is the historical parity
record). The browser front end is **web2** (the plot drawn from data,
zoom, pan, touch, keyboard, light and dark themes); `xppautX` prints and
opens its address; [docs/ui-v2.md](docs/ui-v2.md) is its design and plan.
[docs/using-the-panel.md](docs/using-the-panel.md) is the guide to the
browser front end; [docs/vscode-extension.md](docs/vscode-extension.md)
says how the VS Code extension uses the program. The browser front end has
its own behavioural regression test (`tools/web2check.mjs`).

## Plan

1. **Modern build.** One Makefile (later CMake), warning-clean C99, CI on
   Linux and macOS. *(Makefile and CI done; warnings pending)*
2. **Split numerics from UI.** *(done)* Every call from the numerics into the
   front end goes through the `XppUi` callback table in `core/xpp_ui.h`.
   The historical function names (`err_msg`, `new_int`, `redraw_params`,
   `ALINE`, ...) still exist as dispatchers. The headless defaults log
   messages, decline prompts and draw nothing; `core/ui_json.cpp` installs
   the browser front end's table.
3. **New front end.** Menus and hotkeys come from the menu tables in
   `core/menus.c` and are dispatched through the same `M_*` switch, so
   behaviour stays identical. Targets: a webview inside the
   [XPP-ODE VS Code extension](https://github.com/MuhammadMoustafa/XPP-ODE-Extension)
   and, optionally, a standalone desktop shell. Steps:
   1. *(done)* Command layer in core: `commander`, `run_the_commands` and
      every pop-up menu live in `core/commands.c`; the menus are `XppMenu`
      data in `core/menus.c`, shown through `xpp_ui.menu_choose`.
   2. *(done)* Every command handler is core code: `graf_par.c`,
      `torus.c`, `edit_rhs.c` and `core/grobs.c` (labels, arrows, markers,
      plot-window commands) left the front-end set, and the rest moved into
      `commands.c`/`xpp_util.c`. What a front end still provides are
      interaction primitives (menus, prompts, string/edit boxes, checklist,
      rubber band, scroll, mouse position), window management (plot windows,
      kinescope frames) and three whole dialogs (source viewer, calculator,
      text placement).
   2.5 *(done)* Logic out of the remaining front-end windows: the data
      browser's commands (`browse_data.c`), the IC/parameter box values
      and sliders (`xpp_util.c`), the animation language and its drawing
      geometry (`aniparse.c`, drawing through `xpp_ui.ani_*`), array plot
      settings and printing (`arrayplot.c`), AUTO diagram grabbing
      (`auto_nox.c`), the GIF encoder (`scrngif.c`), user buttons
      (`userbut.c`) and the equilibrium import.
   3. *(done)* A protocol front end: `core/ui_json.cpp` is an `XppUi` table
      that speaks line-delimited JSON (data, state and prompts out; keys,
      answers and parameter edits in) and `make server` builds
      `xppautX --server` around it. The protocol is in
      [docs/protocol.md](docs/protocol.md); `tools/servercheck.py` drives a
      session through it in a few seconds without a display.
   4. *(done)* The front end: first a page that replayed the core's pixel
      drawing (`web/`, removed at docs/ui-v2.md T18 with protocol 1's
      `draw`, `palette` and `size`), now web2 (`web2/`), which draws plots,
      the AUTO diagram and animations from data events. `xppautX` without
      `--server` is the same program
      with the page compiled in and a small HTTP server (`core/xpp_http.cpp`,
      127.0.0.1 only, a random token in the address) instead of Node;
      options `--port N`, `--no-open` and `--version`; since W13a it shows
      that page in a desktop window of its own by default (`--browser` for
      a browser tab). The XPP-ODE extension
      frames that page in a panel with **Open in XPP Interactive**
      ([docs/vscode-extension.md](docs/vscode-extension.md)).
   5. *(done)* Native builds of the program: `make xppautx` works with
      MinGW-w64 gcc on Windows (`xppautX.exe` needs only the system C
      runtime, and dll_lib models load `.dll`s) and on macOS with nothing
      extra installed; both build with their window on by default (W13a,
      W13d). The Windows and macOS builds pass the same protocol checks
      and write the same `output.dat` as Linux apart from line endings.
      CI builds and checks all three; nobody has run the macOS window by
      hand yet ("Trying the macOS build" below).
   6. *(done, issue #20)* Retire the legacy front end: its sources,
      Makefile target and CI steps are gone; `tools/web2check.mjs` is the
      browser front end's own regression test.

### Metrics

Scripts that track the numerics and the protocol; `tools/verify.sh` runs
them after a build and fails on any difference:

| Script | Checks |
|---|---|
| `tools/examples_check.sh` | every `examples/**/*.ode` through `xppautX -silent`, each output's md5 against `tests/examples.md5` (`--platform windows`/`macos`: against `tests/examples.<platform>.md5` when committed, CI-generated) |
| `tools/servercheck.py` | protocol session against `xppautX --server` (menus, prompts, integration, equilibria, windows, browser, animation, kinescope, array plot, the data events) |
| `tools/autocheck.py` | AUTO continuations over the protocol (diagrams, labels, grabs, long names) |
| `tools/webcheck.py` | `xppautX` over HTTP: page, token, event stream, files, commands, exit |

`node tools/web2check.mjs` drives the front end (web2) in a headless
browser and checks its state (docs/ui-v2.md).

## Layout

| Path | Contents |
|---|---|
| `core/` | All C sources and headers. |
| `docs/` | `manual/` (the current manual, Markdown), `upstream/` (the original TeX/PDF/HTML manual, historical reference), man page, upstream `HISTORY` and `README`. |
| `examples/` | `ode/` example models, `canonical/`, `tstauto/` AUTO tests. |
| `build/legacy/` | The upstream Makefile variants, kept for reference. |
| `tools/` | `animsvgwww`, `default.opt`. |
| `contrib/` | The CUDA experiment and the XppBetty Java GUI jars from upstream. |

## Installing a release

Each release attaches one archive per platform. Unpack it and run the
program on a model:

```bash
./xppautX examples/ode/lecar.ode
```

on Windows, `xppautX.exe examples\ode\lecar.ode`. Nothing else is needed:
the front end opens in a window of its own (the system's web view:
WebView2 on Windows, which ships with Windows 10 and 11; the system's
WKWebView on macOS; WebKitGTK 4.1 on Linux, where a system without it
gets the browser and the command that installs it), served by the
program itself. Its menu bar has File (Open model…, Quit) and Help
(Manual, Keyboard shortcuts, About), and closing the window quits — except
on macOS, which has no menu bar of its own yet (W13d). `--browser` opens
the same page in your browser instead and prints its address (`XPP:
http://127.0.0.1:...`), for a remote machine or the VS Code extension;
`xppautX --help` lists the modes. To check the window by hand: the title
reads "xppautX — lecar.ode" with the xppautX icon, Help > Manual opens the
page's Help, Help > About shows the version, and after File > Quit (or
closing the window) no xppautX process is left.

**Double-clicking a .ode file.** Each archive's `tools/associate/` sets
xppautX as the opener, per user (no admin rights, and easy to undo):

```bash
# Windows (PowerShell)
powershell -File tools\associate\xppautx-associate.ps1 -Register    # -WhatIf first, to see what it would write
powershell -File tools\associate\xppautx-associate.ps1 -Unregister  # undo

# Linux
tools/associate/install-linux.sh                # installs into ~/.local/share; --prefix DIR for elsewhere
tools/associate/install-linux.sh --uninstall     # undo
```

Run it once; after that, double-clicking a `.ode` file opens it in its own
xppautX window (a second `.ode` opens a second window: the core cannot load
a second model into a running session). macOS double-click association is
still `make app` from source (below); the release archive has no bundle yet.

The binaries are not signed or notarized, because a signing identity costs
money at both Apple and Microsoft, so each system asks once before running a
program it downloaded. Neither warning means anything is wrong with the file.

**macOS.** The release picks `xppautX-*-macos-arm64.tar.gz` for Apple
silicon Macs (M1 and later) and `xppautX-*-macos-x64.tar.gz` for Intel
Macs. The download is quarantined; clear the flag on the unpacked folder,
then run it:

```bash
xattr -dr com.apple.quarantine xppautX-*-macos-*/
./xppautX-*-macos-*/xppautX examples/ode/lecar.ode
```

For a program you double-click instead, right-click it, choose **Open**, and
confirm **Open** in the dialog; after that it starts normally.

**Windows.** SmartScreen shows "Windows protected your PC". Choose **More
info**, then **Run anyway**. If the zip came from a browser you can also
untick the file's **Unblock** box in its Properties before unpacking.

**Linux.** No warning. If the file lost its permissions in the zip:

```bash
chmod +x xppautX
```

Neither warning appears when the program is installed by a package manager
or built from source, so those routes skip this entirely.

## Building

The released **xppautX** program needs nothing installed.

If you want to build from source, you need gcc (or clang) and make.

### Linux

```bash
make -j8 xppautx
./xppautX examples/ode/lecar.ode
```

The window needs WebKitGTK to build (`sudo apt install
libwebkit2gtk-4.1-dev` on Debian and Ubuntu); the Makefile uses it when
`pkg-config` finds `webkit2gtk-4.1`, and otherwise builds a browser-only
xppautX (`WINDOW=0` asks for that anywhere). The window is a small library
of its own, embedded in xppautX and loaded only when the window opens, so
the same binary still starts on a system without WebKitGTK: `-silent`,
`--server` and `--browser` never touch it, and the window mode says what
to install and uses the browser.

### macOS

```bash
make -j8 xppautx
./xppautX examples/ode/lecar.ode
```

The window (WKWebView) is on by default (W13d; `WINDOW=0` builds
browser-only). It has no menu bar of its own yet, and CI is the only
place it has run so far ("Trying the macOS build" below) -- report back
if you try it.

`make app` assembles `xppautX.app` (the binary, `assets/icon.icns`, and
`tools/associate/Info.plist.in`'s `.ode` document type, so Finder offers
xppautX and double-clicking a `.ode` opens it); drag it to Applications by
hand. Untested: no macOS machine has built or run the bundle yet.

### Windows

The binaries build natively with MinGW-w64 gcc (MSYS2 UCRT64, or the gcc
that ships with Strawberry Perl) from a bash shell:

```bash
make -j8 xppautx
./xppautX.exe examples/ode/lecar.ode
```

It opens the front end in its own window (WebView2, with webview's own
loader: no DLL to ship). The icon comes from `assets/icon.svg`, made
into `assets/icon.ico` by `python3 tools/make_icons.py` (Pillow and a
Chrome or Edge); swapping the icon is those two files.

### Headless smoke test

`-silent` integrates without opening a window and writes `output.dat` in the
current directory:

```bash
./xppautX examples/ode/lecar.ode -silent
head output.dat
```

### Library and headless runner

```bash
make lib          # build/obj/libxppcore.a: the numerics
make xppautx      # xppautX: the one program
./xppautX examples/ode/lecar.ode -silent   # writes output.dat
```

### Makefile knobs

| Variable | Default | Purpose |
|---|---|---|
| `CC` | `gcc` | Compiler |
| `OPT` | `-g -O2` | Optimisation / debug flags |
| `WARN` | `-Wall` | Warning flags |

Objects go to `build/obj/`. `make clean` removes them and the binary.

## Releases

Tagging `v*` runs `.github/workflows/release.yml`, which builds xppautX
on Linux, Windows and macOS (arm64 and x64), checks each build with
`tools/servercheck.py` and `tools/webcheck.py`, and attaches one archive per
platform plus the source of those binaries to the GitHub release.
`tools/package_release.sh PLATFORM` makes such an archive locally, stripping
debug info from the binary it packages so the download is smaller (a local
build with `make xppautx` keeps it: `OPT` defaults to `-g -O2`).

The macOS and Windows binaries are not signed, so those systems ask the user
to allow them the first time; "Installing a release" above says what to
click. Removing the warning is not free: it needs a Developer ID certificate
from the Apple Developer Program for macOS and a code-signing certificate
for Windows, both paid and yearly. The tooling itself is free (`codesign`
and `notarytool` come with the Xcode command line tools, and `rcodesign`
signs from Linux), so only the certificates are missing.

## Trying the macOS build

CI builds the macOS window, but nobody has opened it on a Mac yet: if
you have one, this is the part that needs it.

1. **Get a build.** Either a tagged [release](https://github.com/MuhammadMoustafa/xppautX/releases)
   archive (`xppautX-*-macos-arm64.tar.gz` for Apple silicon,
   `xppautX-*-macos-x64.tar.gz` for Intel), or, for the latest commit, the
   `macos-arm64`/`macos-x64` artifact from a run of the `release` workflow
   in [Actions](https://github.com/MuhammadMoustafa/xppautX/actions), or
   the `xppaut-macos` artifact from a `build` workflow run (the plain
   binary, window included, not packaged with the README/examples/license
   the release archive has).
2. **Open it.** Unpack the archive, clear the quarantine flag
   (`xattr -dr com.apple.quarantine` on the unpacked folder), then either
   run `./xppautX examples/ode/lecar.ode` in Terminal, or double-click
   `xppautX` in Finder and choose **Open** when Gatekeeper asks (then quit
   it and relaunch from Terminal with a model argument, since Finder
   cannot pass one).
3. **Try it.** With the window open: run the model (Initialconds > Go,
   or the keys `i g`); open AUTO (File > Auto) and start a continuation
   (Run > Steady state); resize and move the window; then close it and
   check that no xppautX process is left (`ps aux | grep xppautX`). The
   macOS window has no menu bar of its own yet (Open model, Help and About
   are in the Windows and Linux windows' menus only): everything is in the
   page, and a second model is a second `./xppautX other.ode`.
4. **Report it** on [issue #4](https://github.com/MuhammadMoustafa/xppautX/issues/4):
   your macOS version, Apple silicon or Intel, what happened at each step
   above, and if anything looked wrong, the Terminal output of the same
   run with `--verbose` added.

## Documentation

For the browser front end: [docs/using-the-panel.md](docs/using-the-panel.md)
(what differs from the classic menus), [docs/protocol.md](docs/protocol.md)
(the JSON protocol), [docs/front-end-gaps.md](docs/front-end-gaps.md) (the
historical parity record) and [docs/vscode-extension.md](docs/vscode-extension.md).

The manual (model language, numerics, AUTO, the current front end) is
[docs/manual/](docs/manual/README.md), kept current with the code (W12). The
original XPPAUT TeX/PDF manual and HTML help it was converted from live in
`docs/upstream/` as historical reference. Upstream's install notes are in
`docs/README.upstream`.

## How to cite

If xppautX was useful in work you publish, please cite it (`CITATION.cff`,
or GitHub's "Cite this repository") and cite XPPAUT itself: the numerics are
Bard Ermentrout's, described in *Simulating, Analyzing, and Animating
Dynamical Systems: A Guide to XPPAUT for Researchers and Students* (SIAM,
2002). Citing is a request, not a licence condition.

## License

GPL v2, as upstream. See `LICENSE`. XPPAUT is copyright Bard Ermentrout.

That applies to anything built from this repository, including `xppautX`
inside another program: ship the licence text with the binaries and point to
the source they were built from (each release attaches it). Software that
only talks to `xppautX` over the protocol, in another process, is a separate
program and can have its own licence, as the VS Code extension does.
