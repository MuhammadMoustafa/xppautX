# xppautX

A modernization of [XPPAUT](https://sites.pitt.edu/~phase/bard/bardware/xpp/xpp.html),
Bard Ermentrout's ODE / phase-plane / bifurcation tool.

The goal is a cross-platform XPPAUT that runs natively on Windows, macOS and
Linux without an X server, keeps every feature and every single-letter menu
hotkey of the original, and exposes the numerics as a library that other
tools (for example a VS Code extension) can drive directly.

Current status: **phase 0**. This is the upstream XPPAUT 8.x source, relocated
into a cleaner tree and building with a plain Makefile. It is still the X11
program. Nothing user-facing has changed yet.

## Plan

1. **Modern build.** One Makefile (later CMake), warning-clean C99, CI on
   Linux and macOS. *(in progress)*
2. **Split numerics from UI.** Replace the direct calls from the integrators,
   parser and AUTO driver into X11 dialogs with a callback interface, and
   build the numerics as `libxppcore`.
3. **New front end.** Menus and hotkeys are generated from the existing
   `MENUDEF` tables in `core/menu.c` and dispatched through the same `M_*`
   switch in `core/menudrive.c`, so behaviour stays identical. Targets: a
   webview inside the
   [XPP-ODE VS Code extension](https://github.com/MuhammadMoustafa/XPP-ODE-Extension)
   and, optionally, a standalone desktop shell.

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
