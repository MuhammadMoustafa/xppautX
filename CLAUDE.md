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
web2check, tools/asancheck.sh, and the Windows unit tests (`make test`
with MinGW) and servercheck (CI also runs
everything on each push). A new request that comes up while a task is
running gets its own task card rather than growing the running one.
Pushing closes issues: every GitHub issue whose card or task the pushed
commits finish is closed right after the push, with a comment naming its
commits (hash and subject); a card only partly done stays open, with a
comment on what landed.

Headless smoke test by hand (writes output.dat in cwd, expect 601 rows and
md5 c281851de59ffd03b2a46428619a0c8f for lecar.ode):

    wsl -e bash -lc "cd /mnt/c/gitRepos/xppautX && ./xppautX examples/ode/lecar.ode -silent && md5sum output.dat"

Run it (opens its desktop window; `--browser` for the browser front end):

    wsl -e bash -lc "cd /mnt/c/gitRepos/xppautX && ./xppautX examples/ode/lecar.ode"

verify.sh also runs every example model through `xppautX -silent` and
compares each output.dat's md5 (CRs removed) with tests/examples.md5
(`tools/examples_check.sh`, ~30 s); a model that crashes or times out
fails it too. A difference means the numerics changed: rewrite the
baseline with `tools/examples_check.sh --update` only when the change is
intended, and say which models changed in the commit. Other platforms
(W17): CI's windows-core and macos-core jobs run `examples_check.sh --platform
<windows|macos> --write examples.<platform>.md5`, which compares with
tests/examples.<platform>.md5 when it exists, else with Linux's in a
first-run mode that reports the differing models without failing (a
crash still fails), and upload the md5s they computed as the artifact
`examples-md5-<platform>`. A platform that differs from Linux gets its
own baseline by committing that artifact's file as
tests/examples.<platform>.md5; when numerics change on purpose, commit
the new Linux baseline and the artifacts of that push's CI run. The local
MinGW build (gcc 13.2) matched 179 of Linux's 195 at W17:
`tools/examples_check.sh --bin xppautX.exe --platform windows` from Git
Bash.

verify.sh's checks about the source rather than the build (UTF-8, the
scripts' executable bit, stdoutcheck, formatcheck, the LTO type check,
the dead-code check) are `tools/sourcecheck.sh`; CI runs them once, in its `source` job (with
`--warnings`: tools/warnings.sh's count, and web2's dist/types/unit
tests), and its linux-core job runs `verify.sh --no-source-checks`. Every
platform runs the same behaviour checks against its own build (`<platform>-core`)
and web2check against it (`<platform>-ui`), for linux, windows and macos;
`windows-clang` runs windows-core's checks against a clang build and then
asancheck (below);
a check step runs even after another one failed (only a failed build stops them).

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

`tools/deadcode.sh` (W24; sourcecheck runs it with `--check`, about a
minute) builds every object at -O0 with -ffunction-sections
-fdata-sections -fno-common into build/deadcode (`make deadcode`), links
xppautX from the objects, the Linux window library and each unit test
with --gc-sections, and lists by file the functions and file-scope data
no link keeps (reached through a pointer table counts as reached). A
function only a unit test reaches is listed too. `--check` fails on
anything not in the allowlist inside the script, each entry with its
reason: delete dead code rather than add an entry. Linux only (MinGW's
linker keeps every function, macOS's cannot print what it drops); a
function Windows alone calls is caught by the Windows build's link.

`make ltocheck` (run by tools/sourcecheck.sh, which verify.sh runs) links xppautX with LTO into build/lto
and fails on `-Wlto-type-mismatch`: an extern whose type or array bound
differs from its definition, which a normal build cannot see.

`make asan` builds xppautX with AddressSanitizer, LeakSanitizer and
UBSan into build/asan; `tools/asancheck.sh` (CI's `linux-sanitizers` job, not
verify.sh: it takes a few minutes) builds it and runs the smoke run,
every example, the unit tests, servercheck, webcheck and autocheck under
it, and fails on any report (written to build/asan/reports):

    wsl -e bash -lc "cd /mnt/c/gitRepos/xppautX && tools/asancheck.sh"

On Windows it runs with clang (below; `--no-leaks`: no LeakSanitizer
there), about 9 minutes:

    PATH=/c/msys64/clang64/bin:$PATH MAKE=mingw32-make tools/asancheck.sh --no-leaks --builddir build/clang-asan CC=clang CXX=clang++

`tools/asancheck.sh --no-leaks` (CI's `macos-sanitizers` job, Apple clang
on macos-latest) runs the same checks with LeakSanitizer's detect_leaks
off, since Apple Silicon runners do not support it; ASan and UBSan still
run there. The script is portable to macOS (nproc/sysctl, timeout/gtimeout,
md5sum/`md5 -q`), same as tools/examples_check.sh.

The sanitizers do not see a read of memory never written; valgrind's
memcheck does. `make vg` builds xppautX at -O1 without sanitizers into
build/vg; `tools/valgrindcheck.sh` (W21; Linux, ~25 min on 32 threads, not
in CI or verify.sh) runs the smoke run, every example, the unit tests
(`TEST_RUNNER`), then servercheck and autocheck's sections side by side
under memcheck (`--origins`: also where a bad value came from, about
twice as slow, for fixing a report) and fails on
any report (build/vg/reports; tools/valgrind.supp only for code we do not
own). It sets `XPP_CHECK_SLOW=30`, which multiplies every wait of the
python checks (tools/xppclient.py), and `XPP_MEM_INIT=0` (core/xpp_mem.h).

Metrics: verify.sh's `C++: N / M sources` (core/*.cpp over all core
sources). The tree builds with 0 warnings (gcc 13, MinGW gcc 13 and
MSYS2 clang 22):
verify.sh builds with `make WERROR=1`, which makes every category ever
reported an error; `tools/warnings.sh` counts a clean build's warnings by
flag and file.

`sudo` inside WSL needs the user's password; apt installs must be run by the user.
The Windows-side gcc at C:\Strawberry\c\bin is MinGW-w64; use it for the
native build, from Git Bash:

    PATH=/c/Strawberry/c/bin:$PATH mingw32-make -j8 xppautx BUILDDIR=build/win
    python3 tools/servercheck.py --server ./xppautX.exe
    python3 tools/webcheck.py --bin ./xppautX.exe

MSYS2's CLANG64 toolchain (C:\msys64\clang64\bin: clang, libc++, lld,
compiler-rt; W23) is a second Windows compiler, never the default: CI's
`windows-clang` job. The Makefile knows clang (`CLANG`: gcc-only
`-Werror=` names dropped, webview in C++17 for libc++). Build it into its
own directory, the binary at build/clang/xppautX.exe (dynamically linked:
run it with clang64/bin on PATH):

    PATH=/c/msys64/clang64/bin:$PATH mingw32-make -j8 build/clang/xppautX.exe BUILDDIR=build/clang CC=clang CXX=clang++ WERROR=1

Windows API code lives only in `core/xpp_win32.cpp` (windows.h macros clash
with core names like `max`, `MessageBox`, `VARTYPE`); the core's own
`strupr`/`strlwr` are renamed on Windows in parserslow.h. The exceptions
are the two files that include no core header but small C APIs:
`core/xpp_http.cpp` (sockets) and `core/xpp_window.cpp` (the desktop
window's menu bar, dialogs and icon, behind `_WIN32`); windows.h never
reaches a header.

## Task agents

Work is run as a task board (docs/roadmap.md, docs/ui-v2.md). An agent
(.claude/agents/task-easy, task, task-hard: the model and effort by
difficulty) implements one card in the worktree its brief names:

- Work only there, with every path in this file adapted to it; commit on
  its branch and stop. Never merge, push, touch master, or write to GitHub.
- Gates: the per-task tier above. Iterate with `web2check --only <your
  sections>`; never run the full web2check or tools/asancheck.sh.
- Keep token use low: read the parts of files you need (grep, `sed -n`
  ranges), pipe check output through tail/grep, never paste full logs.
- Leave no `until`/`while` sleep loops or background runs behind.
- Final report: at most 15 lines: what changed, gate results as counts,
  anything unfinished or doubtful.

The reviewer (the main session) reviews, refactors, merges, runs the
5-task tier, pushes when the user says so, and then closes the finished
cards' issues (above). A new roadmap card gets its GitHub issue at once.

## Architecture of the split (phase 2)

- `core/xpp_ui.h` is the seam: an `XppUi` table of callbacks. Core code
  calls the historical names (`err_msg`, `new_float`, `redraw_params`,
  `TwoChoice`, `ALINE`, `set_color`, ...); those are dispatchers in
  `core/xpp_ui.cpp` with headless defaults. `core/ui_json.cpp` installs its
  own table before it serves a session. Adding a UI call from core: add a
  field, a headless default, a dispatcher, and a `j_` function in the
  `core/json_*.cpp` file of its responsibility (declared in
  `core/ui_json_internal.h`) with its entry in ui_json.cpp's `make_json_ui`.
- Shared state that used to live in main.c and the other X11 files
  (removed, issue #20) is grouped into structs, each defined by the module
  that owns it (W7c): `program` (xpp_globals.h: interactive, AUTO's
  scratch dir, version, tutorial), `batch_options` (xpp_batch.h),
  `log_settings` (xpp_log.h), `sliders[]` and `notAlreadySet`
  (load_eqn.h), `plot_windows` (many_pops.h, defined in xpp_util.c: the
  graphs, the active one, the Simulplot list, draw_win), `frozen_curves`
  and `plot_export` (graf_par.h), `color_table` (colormap.h),
  `text_metrics` (xpp_ui.h), `ani_options` (aniparse.h), `movie_autoplay`
  (kinescope.h). Use them through the instance (`plot_windows.current->xlo`),
  include the owner's header, never redeclare them `extern` in a .c file.
  The options that set the X11 window's fonts, colours and size are still
  accepted and no longer stored. `core/xpp_util.c`,
  `core/browse_data.cpp`, `core/colormap.cpp`, `core/menus.c` hold pure code
  moved out of those files.
- Core structs that hold a window store an `XppWinId` (unsigned long); see
  `core/xpp_types.h`.
- `core/commands.cpp` is the command layer (phase 3): `commander` (keys),
  `run_the_commands` (`M_*` ids), and every pop-up menu. Menus are
  `XppMenu` data in `core/menus.c`; front ends show them via
  `xpp_ui.menu_choose` and switch the main menu via `xpp_ui.show_menu`.
  Command logic is all core (phase 3 step 2); `XppUi` only holds
  interaction primitives, window management and a few whole dialogs.
- `core/xpp_batch.c` is the headless entry point; `xpp_load_model()` there
  is the start shared with the server.
- `core/ui_json.cpp` + `core/xppautx_main.c` (`SERVER_SOURCES`) are the JSON
  protocol front end (docs/protocol.md). ui_json.cpp holds the `XppUi`
  table, the command dispatch (`handle_line`), the input classifier,
  script replay, install and hello; the rest is split by responsibility
  into `core/json_io.cpp` (output lines, input lines, the JSON reader),
  `json_prompts.cpp` (asks, messages, long loops), `json_state.cpp` (the
  state event, the data browser, value edits, the data subscription),
  `json_windows.cpp` (plot windows, pixels, kinescope, array plot),
  `json_auto.cpp` (the AUTO window, its diagram data and settings) and
  `json_ani.cpp` (the animation window), which share
  `core/ui_json_internal.h` (C++ only, namespace `xpp::json`; shared
  mutable state in one struct, `xpp::json::session`; file-local state in
  anonymous namespaces). When adding an `XppUi` field, give
  it a `j_` implementation too, and extend `tools/servercheck.py` for new
  protocol behaviour; `tools/verify.sh` runs it. Every command ends with
  `state` then `idle`; a client waits for `idle`.
- `xppautX` (`make xppautx`) is one program: `core/xppautx_main.c` picks
  the desktop window (the default), browser mode (`--browser`/`--web`, or
  `--no-open`), `--server` (the protocol on stdin/stdout) or `-silent`
  (xpp_batch_main, no interface at all). The window (W13a) is
  `core/xpp_window.cpp` (C API in xpp_window.h) over the vendored
  `third_party/webview` (built as its own object, `webview.o`, from
  `src/webview.cc`; WebView2 through the SDK headers in
  `third_party/webview2` and webview's built-in loader on Windows,
  WebKitGTK on Linux only when `pkg-config` finds webkit2gtk-4.1, else a
  browser-only build; `WINDOW=0` forces that). On Linux (W13e) the window
  is a shared library, `libxppwindow.so` (xpp_window.cpp built with
  `XPP_WINDOW_PLUGIN`, webview.o and the icon, -fPIC, the only thing
  linked against GTK/WebKitGTK), embedded in xppautX by
  `tools/embed_bytes.c` and loaded from memory by
  `core/xpp_window_loader.cpp` only in window mode (`memfd_create`, then
  `dlopen` of `/proc/self/fd/N`; a temp file when the kernel will not
  map an executable memfd); it reaches the core only through the
  `XppWindowHost` table of `core/xpp_window_plugin.h` (linked `-z defs`,
  one export), so xppautX's NEEDED has no GTK and the one binary starts
  on any Linux. A failed load logs a WARN with the install command for
  the system (`core/xpp_window_hint.cpp`, from /etc/os-release; test:
  tests/test_window_hint.cpp) and falls back to the browser;
  `XPP_WINDOW_FAIL_LOAD=1` makes the load fail as if WebKitGTK were
  missing (modecheck). Windows and macOS link the window statically, the
  table filled at compile time. It opens centred on its monitor's work
  area, shrunk to fit it (W13f, `place_window`). It shows the page the HTTP
  server below serves, navigated to the tokened URL itself (browser mode
  prints it; the window shows it nowhere). Threads: the core keeps the
  main thread and stays single-threaded; the web view runs its own UI
  loop on a thread of its own (WebView2 wants an STA thread with a message
  loop, GTK one thread that initialises and runs it), except on macOS,
  where Cocoa needs the main thread and the session moves to a second
  thread (untested). Closing the window pushes `{"cmd":"quit"}` into the
  inbox (the protocol's Quit) and calls `xpp_http_release()`; the core's
  exit closes the window after a bye, and after an error leaves it open
  on the log until it is closed (xpp_http's at_exit waits for that, or
  Ctrl+C). If the web view cannot start (no WebView2 runtime, no
  display) xppautX logs it and falls back to browser mode. Its menu bar
  (Win32 menu; GTK 3 menu bar on Linux; none yet on macOS): File > Open
  model (a second xppautX process: the core cannot load a second model),
  Quit; Help > Manual and Keyboard shortcuts (web2's
  `window.__xppOpenHelp`, web2/src/desktop.ts, through webview_eval),
  About (a native message box). The icon is `assets/icon.svg`, made into
  `assets/icon.ico` by `tools/make_icons.py`, compiled into the Windows
  exe by `assets/xppautx.rc`. `tools/modecheck.sh` (verify.sh) checks
  `--help`, `--browser` and `--no-open`, and on Linux with the window
  xppautX's NEEDED, the failed load and a load with no display. It carries
  `core/xpp_http.cpp` (HTTP + Server-Sent Events on
  127.0.0.1, threads, sockets; it includes no core header but the small
  C APIs of xpp_mem.h, xpp_inbox.h and xpp_files.h) and
  `build/.../web_assets.c`, generated by `tools/embed.c` from `web2/dist`
  (served at `/`; `/v1/` and `/v2/` redirect to `/`).
  `/files` (list, GET, streamed PUT of the model's folder) and the
  protocol's `file` command both go through `core/xpp_files.cpp`, which
  owns the name rules (base names only, no links) and the temp-then-rename
  write; docs/protocol.md "Files" is the contract. The
  protocol lines go through `out_line()` in json_io.cpp, which switches to
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
  `xpp_job_point_stored` per AUTO point in autevd.cpp addbif): a cancelled
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
  into the JSON front end (ui_json.cpp, json_*.cpp) and `web2/` (the
  data-level front end: the core sends numbers, e.g. the `series` and
  `plots` events
  after `{"cmd":"data","events":["series","plots"]}`, built in
  `core/plot_data.cpp`, and the page draws them; `nullclines` and `dfield`
  come from `core/phase_data.cpp`, which records per window what
  nullcline.c and the integrator (Flow) draw and forgets it when json_windows.cpp
  blanks the window; `marks` likewise from `core/marks_data.cpp`: Sing pts'
  equilibrium symbols (graphics.c eq_symb), Text,etc's labels and objects
  (grobs.cpp draw_label) and frozen curves (graf_par.c) by their slot; the animation's frames, `ani` `frame`, come from
  `core/ani_data.cpp`, to which aniparse.cpp gives every primitive in the
  `.ani`'s unit coordinates;
  `autoinfo`, AUTO's info strip and stability circle,
  from `core/auto_data.cpp`, which auto_nox.cpp tells what it draws there
  (the circle always a stored diagram point's values, running or grabbed;
  those come from `core/auto_stability.cpp` (W15), the one source of a
  point's eigenvalues/multipliers: autlib1.cpp's stability checks hand it
  what they computed and autevd.cpp addbif stores what it says belongs to
  the point, zeros meaning "not computed", a run's first point included
  unless the run restarts from a label of the same kind);
  the AUTO diagram's points, `diagram`, from json_auto.cpp's `j_auto_diagram`).
  Protocol 2 (T18) has no pixel drawing: the classic page's `draw` ops,
  `palette` and `size` went with it. The `XppUi` pixel primitives
  (`draw_*`, `set_color`, `auto_line` ..., `ani_line` ...) stay as seams
  with headless no-op defaults and no front-end implementation; the code
  that calls them feeds the data modules, which are what the page sees.
  The `pixels` ask stays: web2 renders the picture from its data.
  docs/ui-v2.md has the protocol v2 events and the task list;
  docs/front-end-gaps.md tracks parity. A task that changes the UI
  updates its section in docs/manual/ (the manual, W12), as it does
  docs/protocol.md. web2 serves the manual as `web2/dist/manual.json`, built from
  docs/manual/*.md (W12b), so an edit there also needs `npm run build` in
  web2 and the new dist committed (`npm run check`, in CI, fails otherwise).
- Pop-up menu arrays in menus.c (`main_menu` etc.) start with the title:
  item i is `main_menu[i+1]` with key `main_menu_keys[i]`.

## Memory

- The core allocates with `xpp_malloc`, `xpp_calloc`, `xpp_realloc`,
  `xpp_strdup` and frees with `xpp_free` (core/xpp_mem.h), never libc's
  directly. They never return NULL: a failure logs an ERROR naming the size
  and file:line and exits 1, so callers do not check. Memory comes zeroed
  (xpp_malloc's too, and what xpp_realloc adds); `XPP_MEM_INIT=0` leaves
  it as the C library gives it, for valgrind (tools/valgrindcheck.sh). `XPP_MEM_FAIL_AT=N`
  fails the N-th allocation (verify.sh checks the message;
  `XPP_WINDOW_FAIL_LOAD=1`, xpp_window_loader.cpp, is the Linux window's
  like hook); `--debug`
  prints the counts at exit. The exceptions (memory a library allocates or
  frees) are listed in xpp_mem.h's comment; add any new one there and in
  `tools/alloccheck.sh` (sourcecheck.sh), which fails any other direct call.
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
- New code that reads or writes a file (`core/xpp_io.h`, W11 step 3) uses
  `xpp_line_reader_open`/`_attach` for a whole line of any length (no
  fixed-buffer cut, no `while(!feof)` reading the last line twice; CR/LF
  tolerant) and `xpp_token_reader_open`/`_attach` where the file is
  whitespace-separated numbers read like `fscanf` (`_double`/`_int`
  return 1/0 like `fscanf`'s own convention; `_string` is the safe,
  never-overflowing `xpp_strlcpy`-style counterpart of `fscanf "%s"`),
  over `fopen`/`fgets`/`fscanf`/`feof`. `_attach` wraps a `FILE *` the
  caller already owns (never closes it) for a helper that takes a plain
  `FILE *` from elsewhere, such as `lunch-new.cpp`'s `io_int`/`io_double`
  or `diagram.cpp`'s `load_diagram`; `_open` owns a path it opens itself.
  `xpp_writer_open(path)` opens a temp file next to `path` ("w" text
  mode) for the writing to go through its `xpp_writer_file()` FILE* with
  ordinary `fprintf` (or `xpp_writer_printf`); `xpp_writer_commit` closes
  it and renames it into place (`xpp_files_replace_file`, the same
  cross-platform rename `core/xpp_files.cpp`'s own uploads use, not
  duplicated here), logging an ERROR and leaving the original file
  untouched on failure; `xpp_writer_abort` discards the temp file without
  touching `path` at all. C++ code may use the RAII wrappers
  `xpp::LineReader`/`xpp::Writer` instead of the C API's explicit
  `_close`/`_commit`/`_abort`, and `xpp::TokenReader`, whose `read()`
  overloads pick the conversion from the target's type. Integer columns
  printed flush against each other (AUTO's `%5ld` label lines in
  fort.8/.s) are read with `xpp_token_reader_long` (fscanf `%ld`'s own
  grammar, not a whole token; `read(long&)`), and
  `xpp_token_reader_skip_line` skips the rest of a line;
  `xpp_writer_open_binary` (`xpp::Writer::binary`) is the writer for a
  byte-for-byte copy (AUTO's copyf/appendf). Files AUTO streams into
  during a run (fort.7/8/9) keep their FILE*. Not every `fopen` in the core goes through
  this yet: a write via the shared `open_write_file` (`browse_data.cpp`,
  used well beyond W7b's files) still opens its target directly, since
  giving it temp-then-rename needs every caller's `fclose` to become a
  commit too, across files outside a single task's scope.

## C and C++

The core stays C and converts to C++ progressively (decision 2026-09-23;
aim: 70%+ C++ over time, verify.sh's `C++: N / M sources` is the metric).

- The rule: a task that changes a core C file converts that file to .cpp
  as part of the task, whatever the change, sweeps included (logging
  calls, renames, warning fixes, dead code removal; maintainer's decision
  2026-09-25, replacing the sweep exemption).
- Converting is `git mv core/x.c core/x.cpp` and nothing in the Makefile:
  source lists name files without an extension, core/*.cpp builds with
  $(CXX) (-std=c++23, gnu++23 on Windows) and programs with any C++ object
  link with $(CXX). Then fix what C++ rejects: K&R definitions and `f()`
  declarations (in C++ `()` means no arguments) become prototypes, casts
  from `void *` (malloc) become explicit, identifiers that are C++ keywords
  (`new`, `delete`, `class`, `this`, `template`, `or`, `and`, `not`, ...)
  are renamed, designated initializers must follow member order
  (C++20's rule), string literals are `const char *`, and `int` is not an enum.
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
