# xppautX

A modernization of [XPPAUT](https://sites.pitt.edu/~phase/bard/bardware/xpp/xpp.html),
Bard Ermentrout's ODE / phase-plane / bifurcation tool.

The goal is a cross-platform XPPAUT that runs natively on Windows, macOS and
Linux without an X server, keeps every feature and every single-letter menu
hotkey of the original, and exposes the numerics as a library that other
tools (for example a VS Code extension) can drive directly.

Current status: **phase 2 done, phase 3 started**. The numerics no longer depend on X11.
`make lib` builds `libxppcore.a` from the 83 UI-free sources; `make cli`
builds `xppcore-cli`, a headless runner that links only the library and
produces byte-identical `output.dat` to `xppaut -silent`. The X11 program
still builds and behaves as before.

## Plan

1. **Modern build.** One Makefile (later CMake), warning-clean C99, CI on
   Linux and macOS. *(Makefile and CI done; warnings pending)*
2. **Split numerics from UI.** *(done)* Every call from the numerics into the
   front end goes through the `XppUi` callback table in `core/xpp_ui.h`.
   The historical function names (`err_msg`, `new_int`, `redraw_params`,
   `ALINE`, ...) still exist as dispatchers, so upstream diffs stay
   mergeable; the X11 implementations carry an `x11_` prefix and are wired
   in by `core/ui_x11.c`. The headless defaults log messages, decline
   prompts and draw nothing.
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
      plot-window commands) left the X11 set, and the rest moved into
      `commands.c`/`xpp_util.c`. What a front end still provides are
      interaction primitives (menus, prompts, string/edit boxes, checklist,
      rubber band, scroll, mouse position), window management (plot windows,
      kinescope frames) and three whole dialogs (source viewer, calculator,
      text placement).
   2.5 *(done)* Logic out of the remaining X11 windows: the data browser's
      commands (`browse_data.c`), the IC/parameter box values and sliders
      (`xpp_util.c`), the animation language and its drawing geometry
      (`aniparse.c`, drawing through `xpp_ui.ani_*`; the window is
      `aniwin.c`), array plot settings and printing (`arrayplot.c`; window
      `aplotwin.c`), AUTO diagram grabbing (`auto_nox.c`), the GIF encoder
      (`scrngif.c`), user buttons (`userbut.c`) and the equilibrium import.
   3. A protocol front end: an `XppUi` table that speaks line-delimited
      JSON (drawing, redraw notices and state out; keys, menu picks and
      prompt answers in), and an `xppcore-server` binary around it.
   4. The webview renderer in the VS Code extension.
   5. Native Windows and macOS builds of the server (or a WebAssembly build).

### Metrics

Two scripts track the split; both must stay green (`tools/verify.sh` runs
them after a build and checks the output checksums):

| Script | Measures | Now |
|---|---|---|
| `tools/x11free.sh` | sources that compile with X11 headers stubbed out | 92 / 114 |
| `tools/coredeps.sh` | symbols those objects import from X11 objects | 0 |

`tools/guicheck.sh [REF]` guards the X11 program itself: it builds `REF`
(default `HEAD`), drives both GUIs through the keys in `tools/gui_keys.txt`
with `tools/xdrive.c` and compares the screenshots and the files the
session writes. It covers every menu, the data browser, the animation,
array plot, equilibrium and AUTO windows, and runs on a private Xvfb display
when `xvfb` is installed (about ten minutes).

## Layout

| Path | Contents |
|---|---|
| `core/` | All C sources and headers. `core/bitmaps/` holds the `#include`d X bitmaps. |
| `docs/` | Manuals (`xpp_doc.pdf`, `xpp_sum.pdf`), TeX sources, HTML help, man page, upstream `HISTORY` and `README`. |
| `examples/` | `ode/` example models, `canonical/`, `tstauto/` AUTO tests. |
| `build/legacy/` | The upstream Makefile variants, kept for reference. |
| `tools/` | `animsvgwww`, `default.opt`. |
| `contrib/` | The CUDA experiment and the XppBetty Java GUI jars from upstream. |

## Building

Requires gcc (or clang), make, and X11 headers.

### Linux

```bash
sudo apt install build-essential libx11-dev   # Debian / Ubuntu
make -j8
./xppaut examples/ode/lecar.ode
```

### macOS

Install [XQuartz](https://www.xquartz.org/) or `brew install libx11`, then:

```bash
make -j8 X11_INC=-I/opt/X11/include X11_LIB=-L/opt/X11/lib
# Homebrew instead of XQuartz:
make -j8 X11_INC=-I$(brew --prefix)/include X11_LIB=-L$(brew --prefix)/lib
```

### Windows

Phase 0 builds only under WSL. Windows 11 ships WSLg, so the X11 window opens
on the Windows desktop with no extra X server.

One-time setup inside WSL:

```bash
wsl -e bash -lc "sudo apt install -y build-essential libx11-dev"
```

Build and run from a Windows terminal:

```bash
wsl -e bash -lc "cd /mnt/c/gitRepos/xppautX && make -j8"
```

```bash
wsl -e bash -lc "cd /mnt/c/gitRepos/xppautX && ./xppaut examples/ode/lecar.ode"
```

Adjust the path if you cloned somewhere else.

### Headless smoke test

`-silent` integrates without opening a window and writes `output.dat` in the
current directory:

```bash
./xppaut examples/ode/lecar.ode -silent
head output.dat
```

### Library and headless runner

```bash
make lib          # libxppcore.a: the numerics, no X11
make cli          # xppcore-cli: batch runner linked against the library only
./xppcore-cli examples/ode/lecar.ode   # writes output.dat, same as xppaut -silent
```

### Makefile knobs

| Variable | Default | Purpose |
|---|---|---|
| `CC` | `gcc` | Compiler |
| `OPT` | `-g -O2` | Optimisation / debug flags |
| `WARN` | `-Wall` | Warning flags |
| `X11_INC`, `X11_LIB` | empty | Extra `-I` / `-L` for non-standard X11 locations |

Objects go to `build/obj/`. `make clean` removes them and the binary.

## Documentation

The original manual is `docs/xpp_doc.pdf`; the quick summary is
`docs/xpp_sum.pdf`; the HTML help that the program's Help menu opens lives in
`docs/help/`. Upstream's install notes are in `docs/README.upstream`.

## License

GPL v2, as upstream. See `LICENSE`. XPPAUT is copyright Bard Ermentrout.
