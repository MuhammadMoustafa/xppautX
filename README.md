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
The classic page is legacy, kept at `/v1/` until
[docs/ui-v2.md](docs/ui-v2.md) task T18 removes it.
[docs/using-the-panel.md](docs/using-the-panel.md) is the guide to the
browser front end; [docs/vscode-extension.md](docs/vscode-extension.md)
says how the VS Code extension uses the program. The web front end has its
own behavioural regression test (`tools/webtest.mjs`).

## Plan

1. **Modern build.** One Makefile (later CMake), warning-clean C99, CI on
   Linux and macOS. *(Makefile and CI done; warnings pending)*
2. **Split numerics from UI.** *(done)* Every call from the numerics into the
   front end goes through the `XppUi` callback table in `core/xpp_ui.h`.
   The historical function names (`err_msg`, `new_int`, `redraw_params`,
   `ALINE`, ...) still exist as dispatchers. The headless defaults log
   messages, decline prompts and draw nothing; `core/ui_json.c` installs
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
   3. *(done)* A protocol front end: `core/ui_json.c` is an `XppUi` table
      that speaks line-delimited JSON (drawing, state and prompts out; keys,
      answers, sizes and parameter edits in) and `make server` builds
      `xppautX --server` around it. The protocol is in
      [docs/protocol.md](docs/protocol.md); `tools/servercheck.py` drives a
      session through it in a few seconds without a display.
   4. *(done)* The front end: `web/xpp-client.js` renders the protocol in any
      browser page (canvas plots, menu column, parameter and IC panel,
      dialogs for every prompt, AUTO and animation windows). `node
      web/serve.js file.ode` runs it standalone at http://127.0.0.1:8765/.
      `xppautX` without `--server` is the same program
      with the page compiled in and a small HTTP server (`core/xpp_http.cpp`,
      127.0.0.1 only, a random token in the address) instead of Node;
      options `--port N`, `--no-open` and `--version`. The XPP-ODE extension
      frames that page in a panel with **Open in XPP Interactive**
      ([docs/vscode-extension.md](docs/vscode-extension.md)).
   5. *(done for Windows; macOS in CI)* Native builds of the program:
      `make xppautx` works with MinGW-w64 gcc on Windows (`xppautX.exe`
      needs only the system C runtime, and dll_lib models load `.dll`s)
      and on macOS with nothing extra installed. The Windows build passes
      the same protocol checks and writes the same `output.dat` as Linux
      apart from line endings. CI builds and checks both.
   6. *(done, issue #20)* Retire the legacy front end: its sources,
      Makefile target and CI steps are gone; `tools/webtest.mjs` remains
      the browser front end's own regression test.

### Metrics

Scripts that track the numerics and the protocol; `tools/verify.sh` runs
them after a build and fails on any difference:

| Script | Checks |
|---|---|
| `tools/examples_check.sh` | every `examples/**/*.ode` through `xppautX -silent`, each output's md5 against `tests/examples.md5` |
| `tools/servercheck.py` | protocol session against `xppautX --server` (menus, prompts, integration, equilibria, windows, browser, animation, kinescope, array plot, the data events) |
| `tools/autocheck.py` | AUTO continuations over the protocol (diagrams, labels, grabs, long names) |
| `tools/webcheck.py` | `xppautX` over HTTP: page, token, event stream, files, commands, exit |

`node tools/web2check.mjs` drives the new front end (web2) in a headless
browser and checks its state (docs/ui-v2.md).

`node tools/webtest.mjs [--bin BIN]` guards the classic page (`web/`, frozen
until web2 replaces it):
it builds nothing, and plays `tools/web_steps.txt` (real key presses,
clicks and drags: menus, prompts, side panel, data browser, text views,
scrolling, windows, animation, kinescope, array plot, AUTO, 3D, file
selector, calculator, errors, a narrow panel) against `./xppautX[.exe]` in
a headless Chrome or Edge, checking what each step claims about the result
(a dialog's kind and title, a value the server computed, a canvas actually
drawn to, the files the session wrote) instead of comparing screenshots.
It needs Node 22+ and a Chrome, Chromium or Edge, nothing else.

## Layout

| Path | Contents |
|---|---|
| `core/` | All C sources and headers. |
| `docs/` | Manuals (`xpp_doc.pdf`, `xpp_sum.pdf`), TeX sources, HTML help, man page, upstream `HISTORY` and `README`. |
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
the front end opens in your browser and the program serves it itself.

The binaries are not signed, because a signing identity costs money at both
Apple and Microsoft, so each system asks once before running a program it
downloaded. Neither warning means anything is wrong with the file.

**macOS.** The download is quarantined. Clear the flag, then run it:

```bash
xattr -d com.apple.quarantine xppautX
chmod +x xppautX
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

### macOS

```bash
make -j8 xppautx
./xppautX examples/ode/lecar.ode
```

### Windows

The binaries build natively with MinGW-w64 gcc (MSYS2 UCRT64, or the gcc
that ships with Strawberry Perl) from a bash shell:

```bash
make -j8 xppautx
./xppautX.exe examples/ode/lecar.ode
```

It opens the front end in your browser.

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
`tools/package_release.sh PLATFORM` makes such an archive locally.

The macOS and Windows binaries are not signed, so those systems ask the user
to allow them the first time; "Installing a release" above says what to
click. Removing the warning is not free: it needs a Developer ID certificate
from the Apple Developer Program for macOS and a code-signing certificate
for Windows, both paid and yearly. The tooling itself is free (`codesign`
and `notarytool` come with the Xcode command line tools, and `rcodesign`
signs from Linux), so only the certificates are missing.

## Documentation

For the browser front end: [docs/using-the-panel.md](docs/using-the-panel.md)
(what differs from the classic menus), [docs/protocol.md](docs/protocol.md)
(the JSON protocol), [docs/front-end-gaps.md](docs/front-end-gaps.md) (the
historical parity record) and [docs/vscode-extension.md](docs/vscode-extension.md).

The original manual is `docs/xpp_doc.pdf`; the quick summary is
`docs/xpp_sum.pdf`; the HTML help that the program's Help menu opens lives in
`docs/help/`. Upstream's install notes are in `docs/README.upstream`.

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
