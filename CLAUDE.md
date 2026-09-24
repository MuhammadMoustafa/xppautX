# xppautX — notes for Claude Code

Fork of XPPAUT 8.x being modernized. See README.md for the plan and layout.

## Build (from Windows this repo builds only under WSL)

    wsl -e bash -lc "cd /mnt/c/gitRepos/xppautX && make -j8"

Full check after any change (build xppautX, smoke-test checksum,
print the metrics). This is the gate before committing:

    wsl -e bash -lc "cd /mnt/c/gitRepos/xppautX && tools/verify.sh"

Gates come in two tiers. Every task: a clean build with 0 warnings, the
unit tests and web2's typecheck, verify.sh (always: it guards the
numerics), and `node tools/web2check.mjs --only <the task's sections>`
when web2 changed. Every 5 merged tasks, and before any push: the full
web2check, tools/asancheck.sh and the Windows servercheck (CI also runs
everything on each push). A new request that comes up while a task is
running gets its own task card rather than growing the running one.
Pushing closes issues: every GitHub issue whose card or task the pushed
commits finish is closed right after the push, with a comment naming its
commits (hash and subject); a card only partly done stays open, with a
comment on what landed.

Headless smoke test by hand (writes output.dat in cwd, expect 601 rows and
md5 c281851de59ffd03b2a46428619a0c8f for lecar.ode):

    wsl -e bash -lc "cd /mnt/c/gitRepos/xppautX && ./xppautX examples/ode/lecar.ode -silent && md5sum output.dat"

Run it (opens the browser front end):

    wsl -e bash -lc "cd /mnt/c/gitRepos/xppautX && ./xppautX examples/ode/lecar.ode"

verify.sh also runs every example model through `xppautX -silent` and
compares each output.dat's md5 with tests/examples.md5
(`tools/examples_check.sh`, ~30 s). A difference means the numerics
changed: rewrite the baseline with `tools/examples_check.sh --update` only
when the change is intended, and say which models changed in the commit.

The front end (`web2/`, the page at `/`; a `/v1/` or `/v2/` bookmark
redirects to `/`; design and plan in docs/ui-v2.md). The classic page
(`web/`) and its check `tools/webtest.mjs` were removed at T18. Building
xppautX never needs Node: `web2/dist` is built from `web2/src` and
committed. After editing `web2/src`, from Git Bash:

    cd web2 && npm ci && npm run build && npm run check && npm test && npm run typecheck
    PATH=/c/Strawberry/c/bin:$PATH mingw32-make -j8 xppautx BUILDDIR=build/win
    node tools/web2check.mjs      # state-level browser checks against ./xppautX.exe
                                  # (--only desktop,files,live,million runs a part)

and commit `web2/dist` with the source. Tests read `window.__xpp`
(`state()`, `actions()`, `sent()`, `plot()`, `diagram()`,
`diagramEvents()`, `longTasks()`), never pixels. `tools/cdp.mjs` is
web2check.mjs's headless-browser driver; web2check.mjs runs from Git Bash,
where Node and Chrome are (not WSL), and builds nothing: it drives
`./xppautX[.exe]` (`--bin` to point elsewhere).

`make ltocheck` (run by verify.sh) links xppautX with LTO into build/lto
and fails on `-Wlto-type-mismatch`: an extern whose type or array bound
differs from its definition, which a normal build cannot see.

`make asan` builds xppautX with AddressSanitizer, LeakSanitizer and
UBSan into build/asan; `tools/asancheck.sh` (CI's `sanitizers` job, not
verify.sh: it takes a few minutes) builds it and runs the smoke run,
every example, the unit tests, servercheck, webcheck and autocheck under
it, and fails on any report (written to build/asan/reports):

    wsl -e bash -lc "cd /mnt/c/gitRepos/xppautX && tools/asancheck.sh"

Metrics: verify.sh's `C++: N / M sources` (core/*.cpp over all core
sources). The tree builds with 0 warnings (gcc 13 and MinGW gcc 13):
verify.sh builds with `make WERROR=1`, which makes every category ever
reported an error; `tools/warnings.sh` counts a clean build's warnings by
flag and file.

`sudo` inside WSL needs the user's password; apt installs must be run by the user.
The Windows-side gcc at C:\Strawberry\c\bin is MinGW-w64; use it for the
native build, from Git Bash:

    PATH=/c/Strawberry/c/bin:$PATH mingw32-make -j8 xppautx BUILDDIR=build/win
    python3 tools/servercheck.py --server ./xppautX.exe
    python3 tools/webcheck.py --bin ./xppautX.exe

Windows API code lives only in `core/xpp_win32.c` (windows.h macros clash
with core names like `max`, `MessageBox`, `VARTYPE`); the core's own
`strupr`/`strlwr` are renamed on Windows in parserslow.h.

## Architecture of the split (phase 2)

- `core/xpp_ui.h` is the seam: an `XppUi` table of callbacks. Core code
  calls the historical names (`err_msg`, `new_float`, `redraw_params`,
  `TwoChoice`, `ALINE`, `set_color`, ...); those are dispatchers in
  `core/xpp_ui.c` with headless defaults. `core/ui_json.cpp` installs its
  own table before it serves a session. Adding a UI call from core: add a
  field, a headless default, a dispatcher, and a `j_` entry in ui_json.cpp.
- `core/xpp_globals.[ch]` holds shared state that used to live in main.c
  and the other X11 files (removed, issue #20). `core/xpp_util.c`,
  `core/browse_data.c`, `core/colormap.c`, `core/menus.c` hold pure code
  moved out of those files.
- Core structs that hold a window store an `XppWinId` (unsigned long); see
  `core/xpp_types.h`.
- `core/commands.c` is the command layer (phase 3): `commander` (keys),
  `run_the_commands` (`M_*` ids), and every pop-up menu. Menus are
  `XppMenu` data in `core/menus.c`; front ends show them via
  `xpp_ui.menu_choose` and switch the main menu via `xpp_ui.show_menu`.
  Command logic is all core (phase 3 step 2); `XppUi` only holds
  interaction primitives, window management and a few whole dialogs.
- `core/xpp_batch.c` is the headless entry point; `xpp_load_model()` there
  is the start shared with the server.
- `core/ui_json.cpp` + `core/xppautx_main.c` (`SERVER_SOURCES`) are the JSON
  protocol front end (docs/protocol.md). When adding an `XppUi` field, give
  it a `j_` implementation too, and extend `tools/servercheck.py` for new
  protocol behaviour; `tools/verify.sh` runs it. Every command ends with
  `state` then `idle`; a client waits for `idle`.
- `xppautX` (`make xppautx`) is one program: `core/xppautx_main.c` picks
  browser mode (the default), `--server` (the protocol on stdin/stdout) or
  `-silent` (xpp_batch_main, no interface at all). It carries
  `core/xpp_http.cpp` (HTTP + Server-Sent Events on
  127.0.0.1, threads, sockets; it includes no core header but the small
  C APIs of xpp_mem.h, xpp_inbox.h and xpp_files.h) and
  `build/.../web_assets.c`, generated by `tools/embed.c` from `web2/dist`
  (served at `/`; `/v1/` and `/v2/` redirect to `/`).
  `/files` (list, GET, streamed PUT of the model's folder) and the
  protocol's `file` command both go through `core/xpp_files.cpp`, which
  owns the name rules (base names only, no links) and the temp-then-rename
  write; docs/protocol.md "Files" is the contract. The
  protocol lines go through `out_line()` in ui_json.cpp, which switches to
  xpp_http.cpp in web mode. Input never touches the core thread: reader
  threads (xpp_http.cpp, or the --server stdin reader) push lines into
  `core/xpp_inbox.cpp` (control and normal queues) and `read_line()` takes
  them from there; `-silent` starts no reader. Abort and Quit cancel the
  running job from the reader thread (`core/xpp_job.{h,cpp}`, by sequence
  number); computations ask `xpp_job_cancelled()` or go through the
  throttled checkpoints `my_abort()`/`byeauto_()`. ui_json.cpp's `classify()`
  says which lines are control lines; docs/protocol.md "Commands during a
  command" is the contract. Computations report how far they got to
  xpp_job (`xpp_job_rows_stored` per stored row in integrate.c's `row_stored()`,
  which also feeds `XppUi.rows_stored` (web2's live `series` appends),
  `xpp_job_point_stored` per AUTO point in autevd.c addbif): a cancelled
  command sends `stopped` with that, and `--script` replays a recorded
  `{"cmd":"abort","at":...}` by arming `xpp_job_stop_at_rows/point` for
  the line before it (ui_json.cpp `script_arm_stop`). Rebuild xppautX
  after rebuilding `web2/dist`: the page is compiled in.
- `core/xpp_log.[ch]` is the one logging module, quiet by default:
  `xpp_log(level, fmt, ...)` with ERROR/WARN/INFO/DEBUG, threshold WARN,
  raised by `--verbose`/`--debug`. printf semantics (the caller writes the
  newline); output goes to
  `-logfile`'s file if given, else stderr, which browser mode shows in the
  page's log. `plintf()` is INFO, `err_msg()`'s headless default ERROR.
  AUTO's table goes through `xpp_log_auto()`: INFO on the console, always
  written in browser mode, where the AUTO window's Output panel shows it.
  The core never prints to stdout or stderr directly; `tools/stdoutcheck.sh`
  (run by verify.sh) enforces it, with a short allowlist inside the script
  for the handful of lines that are legitimately direct (the `-version`
  and `--version` text, the `XPP:` address lines).
- The X11 front end was removed (issue #20, task W8); new UI work goes
  into ui_json.cpp and `web2/` (the data-level front end: the core sends
  numbers, e.g. the `series` and `plots` events
  after `{"cmd":"data","events":["series","plots"]}`, built in
  `core/plot_data.cpp`, and the page draws them; `nullclines` and `dfield`
  come from `core/phase_data.cpp`, which records per window what
  nullcline.c and the integrator (Flow) draw and forgets it when ui_json.cpp
  blanks the window; `marks` likewise from `core/marks_data.cpp`: Sing pts'
  equilibrium symbols (graphics.c eq_symb), Text,etc's labels and objects
  (grobs.cpp draw_label) and frozen curves (graf_par.c) by their slot; the animation's frames, `ani` `frame`, come from
  `core/ani_data.cpp`, to which aniparse.cpp gives every primitive in the
  `.ani`'s unit coordinates;
  `autoinfo`, AUTO's info strip and stability circle,
  from `core/auto_data.cpp`, which auto_nox.c tells what it draws there;
  the AUTO diagram's points, `diagram`, from ui_json.cpp's `auto_diagram`).
  Protocol 2 (T18) has no pixel drawing: the classic page's `draw` ops,
  `palette` and `size` went with it. The `XppUi` pixel primitives
  (`draw_*`, `set_color`, `auto_line` ..., `ani_line` ...) stay as seams
  with headless no-op defaults and no front-end implementation; the code
  that calls them feeds the data modules, which are what the page sees.
  The `pixels` ask stays: web2 renders the picture from its data.
  docs/ui-v2.md has the protocol v2 events and the task list;
  docs/front-end-gaps.md tracks parity. A task that changes the UI
  updates its section in docs/manual/ (the manual, W12), as it does
  docs/protocol.md.
- Pop-up menu arrays in menus.c (`main_menu` etc.) start with the title:
  item i is `main_menu[i+1]` with key `main_menu_keys[i]`.

## Memory

- The core allocates with `xpp_malloc`, `xpp_calloc`, `xpp_realloc`,
  `xpp_strdup` and frees with `xpp_free` (core/xpp_mem.h), never libc's
  directly. They never return NULL: a failure logs an ERROR naming the size
  and file:line and exits 1, so callers do not check. `XPP_MEM_FAIL_AT=N`
  fails the N-th allocation (verify.sh checks the message); `--debug`
  prints the counts at exit. The exceptions (memory a library allocates or
  frees) are listed in xpp_mem.h's comment; add any new one there.
- A leak or memory error LeakSanitizer/ASan/UBSan reports in our code is
  fixed, never suppressed; tools/lsan.supp is only for code we do not own,
  with a reason per line. Memory kept for the program's life (a global set
  once) needs no free at exit: LSan sees it as reachable.

## Strings and I/O

- The core formats and copies text through `core/xpp_io.h`
  (issue: W11 step 2), never `sprintf`/`strcpy`/`vsprintf` into a fixed
  buffer: `tools/formatcheck.sh` (run by verify.sh) fails a new one. In a
  C file, format with `xpp_snprintf`/`XPP_SPRINTF` and copy with
  `xpp_strlcpy`/`XPP_STRCPY` (`xpp_strlcat`/`XPP_STRCAT` for append): the
  `XPP_*` macros take the destination's size from `sizeof(dst)`, so `dst`
  must be a real array (a struct member or an indexed element works too;
  a pointer fails to *compile*, not silently take `sizeof(pointer)` --
  find that call's real destination size, from its own allocation or its
  callers' buffers, and call `xpp_snprintf`/`xpp_strlcpy` with it
  directly). All three log a WARN, once per call site, when what they
  wanted to write did not fit, instead of overflowing.
- In a C++ file, prefer `xpp::format`/`xpp::number` (`core/xpp_io.h`,
  `std::format`/`std::to_chars`, type-checked at compile time) and
  `XPP_FORMAT_TO_BUF` (the `XPP_SPRINTF`-style array-destination form of
  `xpp::format`) over the C wrappers, unless the original format string
  has no mechanical `std::format` equivalent (`%*s`, `%.*s`, ...) or the
  destination is a pointer (`XPP_FORMAT_TO_BUF`, like `XPP_SPRINTF`,
  needs a real array): those stay on `xpp_snprintf`/`xpp_strlcpy`. The
  build is `-std=c++23`/`gnu++23` for this (present and warning-clean on
  WSL gcc 15 and MinGW gcc 13.2); avoid library parts newer than gcc 13
  ships (e.g. `std::print`) until Windows' MinGW catches up.

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
- A model's names (variables, parameters, aux, functions, arguments,
  tables) go up to `XPP_NAME_MAX` (64, core/xpplim.h); arrays holding one
  are `[XPP_NAME_MAX+1]`, dialog values `[MAX_LEN_SBOX]`. The parser
  refuses a longer name (`name_too_long`) instead of cutting it. A display
  of fixed width shortens with `short_name()` (xpp_util.c, ends in `~`);
  the JSON front end always sends names whole. tools/models/longnames.ode
  and autocheck's `names` section are the test.
- `core/fftn.c` does `#include __FILE__`; the Makefile's `-I.` is required for it.
- `core/sbml2xpp.c` needs libsbml and is not built, same as upstream.
- The refactoring scripts under `tools/` (guard_x11_headers.py, move_funcs.py,
  ui_seam_refactor.py, phase2_step*.py, cxx_guard_headers.py) are one-shot and already applied;
  keep them for the record, do not re-run them.
- Files on disk may be CRLF (Windows checkout); scripts that edit them must
  preserve line endings. Python written with `newline=''` does.
- Commit messages: imperative subject, body explains why, include the metric
  deltas.
