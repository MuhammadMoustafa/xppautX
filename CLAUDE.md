# xppautX — notes for Claude Code

Fork of XPPAUT 8.x being modernized. See README.md for the plan and layout.

## Build (from Windows this repo builds only under WSL)

    wsl -e bash -lc "cd /mnt/c/gitRepos/xppautX && make -j8"

Full check after any change (build both binaries, smoke-test checksums,
print the metrics). This is the gate before committing:

    wsl -e bash -lc "cd /mnt/c/gitRepos/xppautX && tools/verify.sh"

Headless smoke test by hand (writes output.dat in cwd, expect 601 rows and
md5 c281851de59ffd03b2a46428619a0c8f for lecar.ode):

    wsl -e bash -lc "cd /mnt/c/gitRepos/xppautX && ./xppaut examples/ode/lecar.ode -silent && wc -l output.dat"
    wsl -e bash -lc "cd /mnt/c/gitRepos/xppautX && ./xppcore-cli examples/ode/lecar.ode && md5sum output.dat"

Run the GUI (WSLg shows the X11 window on the Windows desktop):

    wsl -e bash -lc "cd /mnt/c/gitRepos/xppautX && ./xppaut examples/ode/lecar.ode"

GUI regression check (builds HEAD, drives both GUIs with the same keys via
XSendEvent, compares screenshots; run it for any change that touches menus,
dispatch or X11 files):

    wsl -e bash -lc "cd /mnt/c/gitRepos/xppautX && tools/guicheck.sh"

Metrics: `make x11free` (sources compiling without X11 headers, 85/111) and
`tools/coredeps.sh -v` (symbols core objects import from X11 objects, 0).
Clean-build warning baseline with gcc 13 is ~520; verify.sh's count is for
the incremental build only.

`sudo` inside WSL needs the user's password; apt installs must be run by the user.
The Windows-side gcc at C:\Strawberry\c\bin is Perl's MinGW without X11 headers — do not use it.

## Architecture of the split (phase 2)

- `core/xpp_ui.h` is the seam: an `XppUi` table of callbacks. Core code
  calls the historical names (`err_msg`, `new_float`, `redraw_params`,
  `TwoChoice`, `ALINE`, `set_color`, ...); those are dispatchers in
  `core/xpp_ui.c` with headless defaults. `core/ui_x11.c` installs the X11
  table from `init_X()`. X11 implementations are renamed `x11_<name>`.
  Adding a UI call from core: add a field, a headless default, a dispatcher,
  and the `x11_` entry; never call an X11 file from a core file directly.
- `core/xpp_globals.[ch]` holds shared state that used to live in main.c
  and other X11 files. `core/xpp_util.c`, `core/browse_data.c`,
  `core/colormap.c`, `core/menus.c` hold pure code moved out of X11 files.
- Core structs that hold a window store an `XppWinId` (unsigned long, same
  representation as an X11 Window); see `core/xpp_types.h`.
- X-typed declarations in headers are wrapped in
  `#if defined(_XLIB_H_) || defined(_X11_XLIB_H_)`. Every X11 .c file must
  therefore include `<X11/Xlib.h>` on its first line.
- `core/commands.c` is the command layer (phase 3): `commander` (keys),
  `run_the_commands` (`M_*` ids), and every pop-up menu. Menus are
  `XppMenu` data in `core/menus.c`; front ends show them via
  `xpp_ui.menu_choose` and switch the main menu via `xpp_ui.show_menu`.
  A menu's layout numbers (width, row) are the X11 `pop_up_list` arguments.
  The "commands" group at the end of `XppUi` lists handlers still living in
  X11 files; moving them into core shrinks that group.
- The Makefile's `UI_SOURCES` list is the X11 set; everything else goes
  into `libxppcore.a`. `core/xpp_batch.c` is the headless entry point.

## Conventions

- Keep upstream function and file names so upstream patches stay mergeable.
- `core/fftn.c` does `#include __FILE__`; the Makefile's `-I.` is required for it.
- `core/sbml2xpp.c` needs libsbml and is not built, same as upstream.
- The refactoring scripts under `tools/` (guard_x11_headers.py, move_funcs.py,
  ui_seam_refactor.py, phase2_step*.py) are one-shot and already applied;
  keep them for the record, do not re-run them.
- Files on disk may be CRLF (Windows checkout); scripts that edit them must
  preserve line endings. Python written with `newline=''` does.
- Commit messages: imperative subject, body explains why, include the metric
  deltas.
