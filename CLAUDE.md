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
    wsl -e bash -lc "cd /mnt/c/gitRepos/xppautX && ./xppautX examples/ode/lecar.ode -silent && md5sum output.dat"

Run the GUI (WSLg shows the X11 window on the Windows desktop):

    wsl -e bash -lc "cd /mnt/c/gitRepos/xppautX && ./xppaut examples/ode/lecar.ode"

GUI regression check (builds HEAD, drives both GUIs with the same keys via
XSendEvent, compares screenshots; run it for any change that touches menus,
dispatch or X11 files). It takes ~10 minutes: run it once per finished batch
of work, not per edit. It uses a private Xvfb display (installed) so it does
not steal focus; `NEW=/abs/path/xppaut` compares a frozen copy so you can keep
building while it runs:

    wsl -e bash -lc "cd /mnt/c/gitRepos/xppautX && tools/guicheck.sh"

Web front end regression check (the counterpart of guicheck for
`xppautX`; run it for changes to `web/`, ui_json.c or xpp_http.c, once
per batch). From Git Bash on Windows, where Node and Chrome are (not WSL):

    PATH=/c/Strawberry/c/bin:$PATH node tools/webtest.mjs

It builds nothing: it drives `./xppautX[.exe]` (`--bin` to point elsewhere)
through tools/web_steps.txt in a headless Chrome or Edge and checks what
each step claims about the result (a dialog's kind and title, a value the
server computed, a canvas actually drawn to, client state fields) — no
screenshots, no pixel comparison. CSS selectors in steps must not contain
spaces (use `>`). `const client` of the page is what steps and settling
look at.

`make ltocheck` (run by verify.sh) links both programs with LTO into
build/lto and fails on `-Wlto-type-mismatch`: an extern whose type or
array bound differs from its definition, which a normal build cannot see.

Metrics: `make x11free` (sources compiling without X11 headers, 99/121),
`tools/coredeps.sh -v` (symbols core objects import from X11 objects, 0)
and verify.sh's `C++: N / M sources` (core/*.cpp over all core sources).
The tree builds with 0 warnings (gcc 13 and MinGW gcc 13): verify.sh builds
with `make WERROR=1`, which makes every category ever reported an error;
`tools/warnings.sh` counts a clean build's warnings by flag and file.

`sudo` inside WSL needs the user's password; apt installs must be run by the user.
The Windows-side gcc at C:\Strawberry\c\bin is MinGW-w64 without X11 headers: use it
only for the native X11-free build, from Git Bash:

    PATH=/c/Strawberry/c/bin:$PATH mingw32-make -j8 xppautx BUILDDIR=build/win
    python3 tools/servercheck.py --server ./xppautX.exe
    python3 tools/webcheck.py --bin ./xppautX.exe

Windows API code lives only in `core/xpp_win32.c` (windows.h macros clash
with core names like `max`, `MessageBox`, `VARTYPE`); the core's own
`strupr`/`strlwr` are renamed on Windows in parserslow.h/parser.h.

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
  Command logic is all core (phase 3 step 2); `XppUi` only holds
  interaction primitives, window management and a few whole dialogs.
- `tools/guicheck.sh` also compares files the session writes (clone,
  save as, kinescope frames, browser tables, array plot PS/GIF) and fails
  when a `shotw` dialog never appeared. `tools/xdrive.c` script commands:
  key, keyw (key to a window by title), sleep, shot, shotw (dialog by
  title; `a|b` alternatives), clickw, names (list window titles). Without a
  window manager a pending prompt swallows the next key: add `key Escape`
  before a new section. `tools/gui_test.ani` is the animation it loads.
- Window code split off core files keeps a `*win.c` name (`aniwin.c`,
  `aplotwin.c`); the upstream file keeps the logic and becomes core.
- The Makefile's `UI_SOURCES` list is the X11 set; everything else goes
  into `libxppcore.a`. `core/xpp_batch.c` is the headless entry point;
  `xpp_load_model()` there is the start shared with the server.
- `core/ui_json.c` + `core/xppautx_main.c` (`SERVER_SOURCES`) are the JSON
  protocol front end (docs/protocol.md). When adding an `XppUi` field, give
  it a `j_` implementation too, and extend `tools/servercheck.py` for new
  protocol behaviour; `tools/verify.sh` runs it. Every command ends with
  `state` then `idle`; a client waits for `idle`.
- `xppautX` (`make xppautx`) is one program: `core/xppautx_main.c` picks
  browser mode (the default), `--server` (the protocol on stdin/stdout) or
  `-silent` (xpp_batch_main, no interface at all). It carries
  `core/xpp_http.c` (HTTP + Server-Sent Events on
  127.0.0.1, threads, sockets; it includes no core header) and
  `build/.../web_assets.c`, generated from `web/` by `tools/embed.c`. The
  protocol lines go through `out_line()` in ui_json.c, which switches to
  xpp_http.c in web mode. Input never touches the core thread: reader
  threads (xpp_http.c, or the --server stdin reader) push lines into
  `core/xpp_inbox.c` (control and normal queues) and `read_line()` takes
  them from there; `-silent` starts no reader. Abort and Quit cancel the
  running job from the reader thread (`core/xpp_job.{h,cpp}`, by sequence
  number); computations ask `xpp_job_cancelled()` or go through the
  throttled checkpoints `my_abort()`/`byeauto_()`. ui_json.c's `classify()`
  says which lines are control lines; docs/protocol.md "Commands during a
  command" is the contract. Rebuild after
  editing `web/` files; `node web/serve.js` still serves them from disk.
- `core/xpp_log.[ch]` is the one logging module, quiet by default:
  `xpp_log(level, fmt, ...)` with ERROR/WARN/INFO/DEBUG, threshold WARN,
  raised by `--verbose`/`--debug` (`-verbose`/`-debug` for xppaut).
  printf semantics (the caller writes the newline); output goes to
  `-logfile`'s file if given, else stderr, which browser mode shows in the
  page's log. `plintf()` is INFO, `err_msg()`'s headless default ERROR.
  AUTO's table goes through `xpp_log_auto()`: INFO on the console, always
  written in browser mode, where the AUTO window's Output panel shows it.
- The X11 front end is frozen and will be removed once the new web UI
  covers it; new UI work goes into ui_json.c and `web/`;
  docs/front-end-gaps.md tracks parity.
- Pop-up menu arrays in menus.c (`main_menu` etc.) start with the title:
  item i is `main_menu[i+1]` with key `main_menu_keys[i]`.

## C and C++

The core stays C and converts to C++ progressively (decision 2026-09-23;
aim: 70%+ C++ over time, verify.sh's `C++: N / M sources` is the metric).

- The rule: a task that fixes or refactors a core file converts that file
  to .cpp as part of the task. Mechanical sweeps (renames, logging calls,
  warning fixes across many files) do not convert anything.
- Converting is `git mv core/x.c core/x.cpp` and nothing in the Makefile:
  source lists name files without an extension, core/*.cpp builds with
  $(CXX) (-std=c++17, gnu++17 on Windows) and programs with any C++ object
  link with $(CXX). Then fix what C++ rejects: K&R definitions and `f()`
  declarations (in C++ `()` means no arguments) become prototypes, casts
  from `void *` (malloc) become explicit, identifiers that are C++ keywords
  (`new`, `delete`, `class`, `this`, `template`, `or`, `and`, `not`, ...)
  are renamed, designated initializers must follow member order (or wait
  for C++20), string literals are `const char *`, and `int` is not an enum.
- The API stays C: core headers are `extern "C"` (`#ifdef __cplusplus`
  guards, added by tools/cxx_guard_headers.py; a new header that declares
  functions or variables gets the same guard by hand, after its
  #includes, never with an #include inside it). A .cpp that calls C code
  defined in a file without a header declares it `extern "C"`.
- No exception may cross into C: C++ code called from C catches what it
  can throw (std::bad_alloc included) or uses only non-throwing code, and
  C callbacks called from C++ are assumed not to throw.
- Use C++ where it clarifies: RAII (std::vector, std::string,
  std::unique_ptr) for allocations the task touches, std::atomic,
  std::chrono, anonymous namespaces for file-local state. No behaviour
  change: verify.sh's checksums still guard the numerics.
- llnltyps.h makes `bool` a macro for int (CVODE's structs, which C and
  C++ must lay out alike): convert the CVODE files together, and include
  C++ standard headers before it.
- `core/xpp_job.cpp` was the first file converted (std::atomic,
  std::chrono).

## Conventions

- Upstream mergeability is no longer a goal (2026-09-23): refactor for
  single responsibility and clean code, numerics included. Numerical
  results must not change: tools/verify.sh's checksums and saved AUTO
  diagram are the guard.
- Tests check data (output files, protocol events, UI state), never
  pixels: do not add screenshot comparisons.
- `core/fftn.c` does `#include __FILE__`; the Makefile's `-I.` is required for it.
- `core/sbml2xpp.c` needs libsbml and is not built, same as upstream.
- The refactoring scripts under `tools/` (guard_x11_headers.py, move_funcs.py,
  ui_seam_refactor.py, phase2_step*.py, cxx_guard_headers.py) are one-shot and already applied;
  keep them for the record, do not re-run them.
- Files on disk may be CRLF (Windows checkout); scripts that edit them must
  preserve line endings. Python written with `newline=''` does.
- Commit messages: imperative subject, body explains why, include the metric
  deltas.
