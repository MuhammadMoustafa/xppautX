# xppautX — notes for Claude Code

Fork of XPPAUT 8.x being modernized. See README.md for the plan and layout.

## Code quality

The rules every change keeps (maintainer's decisions), each with what
enforces it; "review" means no check can, so the reviewer reads for it.
The sections below give the details.

| Rule | Enforced by |
|---|---|
| Single source: one module owns each kind of operation (memory, logging, text, files, dialogs, numerics); a new helper goes into its owner, two copies are merged (Conventions); no logic is written before the common files and the code are searched for it, and the report says what was searched and reused (maintainer, 2026-10-01; Task agents) | dupcheck (textual copies); review (the same algorithm in other words, and the report's search) |
| One name per thing: no namespace alias, type alias, using-declaration or renaming #define of our own names (W113) | aliascheck |
| No global state: what a load makes is the Model's, what a run changes the Session's, passed as `Model&`/`Session&`; the session list is the only global (W47) | globalcheck (external and internal-linkage state, each baseline line with its reason, W120), sessioncheck |
| No fallbacks: our own files and commands load and accept only what they hold, a missing or bad piece is a shown error, no code for older files of ours; importing a foreign format is fine | review (W116) |
| Our files load all or nothing: one read pipeline parses the whole file, checks every line and value, then applies in one step (W125); a bad value in any file stops the load with the file, its line and the value, and nothing is applied: .ode, .odex, .set, .par, .ic, .snapx, .recx alike (maintainer, 2026-10-01; .autox and .autoset went at W155: AUTO's files are the session's auto/ members) | review (W125) |
| No unexplained literal or default: a limit, id, interval or default is a named constant in its owner, with a one-line reason; what the page needs too comes in `hello` | review (W118, W121) |
| Security is designed in wherever input crosses a trust boundary (maintainer, 2026-10-01): the HTTP server and its token (xpp_http.cpp: 127.0.0.1 only, every route checks the token in constant time), `/files` and the `file` command (xpp_files.cpp: base names only, no links, temp-then-rename), the page's bound calls (`__xppFileDialog`, `__xppCloseWindow`), anything that starts a process (`XPPEDITOR`), temp folders, and every reader of a file a user may have been sent (.ode, .odex, scripts, our session files): no path outside its folder, no unbounded allocation or recursion from a value in the input, no shell built from input. A card that touches one says in its report what it checked and what an attacker could still do | review; asancheck and valgrindcheck for memory errors |
| Errors are values: a computation returns an `xpp::Error`, the command that ran it shows it once (W63) | review |
| Every error names its file and line (and the source line): read from a file, the file and its line; caused by a model line at run time, that line; from a command, the command (in a script, its line); one error value, one renderer, one event (maintainer, 2026-10-01; W140) | errorcheck (W140: none without a place, outside its allowlist), review |
| No dead code | deadcode.sh, deadcheck.py |
| Safe C++: RAII, std containers, `xpp::format`/`xpp::log`, the I/O readers and writers, `static_cast` | unsafecheck, alloccheck, formatcheck, literalcheck, filecheck, stdoutcheck |
| C++ API: `extern "C"` only where C really calls in (W109) | externcheck |
| A memory or thread error is fixed, never suppressed | asancheck (with UBSan), tsancheck |
| Builds with 0 warnings on WSL gcc 15, UCRT gcc 16 and clang | `WERROR=1` builds (UCRT and clang64 on each merged tip: gcc 16 alone flags a discarded `std::expected`, clang alone a namespace self-alias) |
| Numerics change only on purpose, and are the same on every CPU, system and compiler: `xpp::math`'s correctly rounded functions, never the C library's, and no FMA contraction (W159) | examples md5s (one baseline, every platform), goldencheck, odexcheck, mathcheck |
| What is ours and what is XPPAUT's is recorded as it changes, so it never needs a review from the beginning (maintainer, 2026-10-01): a card that changes what a user meets adds its line under Unreleased in CHANGELOG.md (W149), one that changes what an XPPAUT user meets (new, changed, removed, a limit lifted) updates its row in docs/xppautx-vs-xppaut.md (W148), one that fixes a bug of XPPAUT's own adds its entry to docs/xppaut-findings.md (below); each with its card | review (at merge) |
| Tests check data, never pixels, and never pass or fail on machine speed (W58); a check waits for a condition, never for a time (maintainer, 2026-10-02: a sleep is not a synchronisation): a sleep in a check is only the poll interval inside a wait for a condition, or a lower bound proving something does not happen, its reason beside it | review; sleepcheck (W166) |
| Agents stop only their own processes, by PID, never by name | review |
| A bug, wrong result or arbitrary limit found in XPPAUT itself is recorded in docs/xppaut-findings.md (for the paper), with its evidence and the lines in XPPAUT's own source, never our refactored code, as relative links into the local, git-ignored copies `reference/xppaut-8.0` (the 8.0 source xppautX was forked from, `git archive c021b51`) and `reference/xppaut-master` (XPPAUT's GitHub master), e.g. `[load_eqn.c:1544](../reference/xppaut-8.0/load_eqn.c#L1544)` (maintainer, 2026-10-01); the index number links to its entry, and every card and commit links to its GitHub issue or commit (maintainer, 2026-10-01) | review |

## Build (Linux builds and checks run under WSL; the Windows builds are native)

Nothing builds or checks in WSL over /mnt/c (maintainer, 2026-09-28):
WSL reads the Windows disk through 9P, several times slower, and runs
side by side there stalled. Commit, then run the build and every check
from WSL's own copy with `tools/wslrun.sh`, from Git Bash in the
checkout (the main one or a worktree):

    tools/wslrun.sh make -j4 WERROR=1 xppautx test
    tools/wslrun.sh tools/odexcheck.sh
    tools/wslrun.sh python3 tools/servercheck.py

Windows-side tools (the MSYS2 builds, web2, web2check against
xppautX.exe) run from Git Bash on the checkout itself, as below.

The reviewer's full check (build xppautX, smoke-test checksum, print
the metrics), once on a wave's merged tip; agents never run it (their
tier is below):

    tools/wslrun.sh tools/verify.sh

and amend the commit if it fails. `tools/wslrun.sh` (W84) runs a check
script in WSL against the checkout's HEAD from a git clone on WSL's own
filesystem (~/.cache/xppautx-verify/<checkout folder>, build/ kept
between runs): verify.sh read over /mnt/c took 22-25 minutes, from
there 6.5-9.5 (once 22, just after WSL had started up; the cause is
not pinned down). It refuses a checkout with uncommitted changes, since
only HEAD would be checked. A check run in place over /mnt/c
(`wsl -e bash -lc "cd /mnt/c/..."`) still works but is not used.

Gates come in two tiers. Every task (the agent, in its worktree): a
clean build with 0 warnings (`make WERROR=1`), the unit tests, web2's
typecheck, and only the checks the task touches, run directly (the
examples md5s, goldencheck, servercheck, autocheck, odexcheck, `node
tools/web2check.mjs --only <the task's sections>` when web2 changed).
Agents never run verify.sh (maintainer, 2026-09-28): five agents each
waiting their turn at it, one at a time, left the last one idle for 40
minutes. The reviewer merges a wave's finished branches one after
another (rebuilding web2/dist on a conflict there, with web2's checks
and the task's web2check sections) and runs verify.sh once on the
merged tip (`--no-source-checks` when only `web2/` changed: the source
checks rebuild and relink the whole core and cannot see web2, and CI's
`source` job runs them on every push); a failure is traced to its
branch by verifying the branches alone. One verify.sh at a time (W84):
the script takes a lock (`flock`, $TMPDIR/xppautx-verify.lock) and a
second run waits for it. The Windows build (UCRT gcc 16, `WERROR=1`)
also runs on each merged tip: its libstdc++ marks `std::expected`
`[[nodiscard]]`, which WSL's gcc 15 does not. Pushes go in batches
(maintainer, 2026-09-30): several merged cards, one push, never one per
wave or card; the push's CI run (every platform: asancheck, the full
web2check, the Windows and macOS builds and tests) is the full tier, so it
is not run locally before a push as well, except when Actions minutes are
short or for what CI cannot see; a CI failure is fixed and goes with the
next batch. A new request that comes up while a task is
running gets its own task card rather than growing the running one.
Pushing closes issues: every GitHub issue whose card or task the pushed
commits finish is closed right after the push, with a comment naming its
commits (hash and subject); a card only partly done stays open, with a
comment on what landed.

Headless smoke test by hand (writes output.dat in cwd, expect 601 rows and
md5 c281851de59ffd03b2a46428619a0c8f for lecar.ode):

    tools/wslrun.sh sh -c 'make -j4 xppautx && ./xppautX examples/ode/lecar.ode --silent && md5sum output.dat'

Run it (opens its desktop window; `--browser` for the browser front end):

    wsl -e bash -lc "cd /mnt/c/gitRepos/xppautX && ./xppautX examples/ode/lecar.ode"

verify.sh also runs every example model through `xppautX --silent` and
compares each output.dat's md5 (CRs removed) with tests/examples.md5
(`tools/examples_check.sh`, ~30 s); a model that crashes or times out
fails it too. A difference means the numerics changed: rewrite the
baseline with `tools/examples_check.sh --update` only when the change is
intended, and say which models changed in the commit. The numbers are
the same on every CPU, system and compiler (W159, maintainer
2026-10-01): the transcendental functions are xpp_math's correctly
rounded ones (`xpp::math::exp`, `sin`, `pow`, ..., CORE-MATH vendored in
third_party/core-math, each built twice on x86, once with FMA, the copy
picked at run time; `tools/mathcheck.sh`, in sourcecheck, fails a direct
call of the C library's), and the build passes `-ffp-contract=off`, so
no target fuses `a*b+c`. One baseline, tests/examples.md5, for every
platform: CI's windows-core and windows-clang compare with it strictly;
macos-core (`--platform macos`, no tests/examples.macos.md5) still runs
it in the first-run mode that reports differing models without failing
(a crash still fails) until a run shows macOS matches, and then turns
strict like Windows. A run with a differing model uploads its md5s and
the differing outputs as the artifact `examples-md5-<platform>`; the
programs are the artifacts `xppautX-<platform>` (kept 14 days, like the
sanitizer reports; the md5 artifacts 30: W44). A platform that differs
is a bug to trace (W159's way: the CPU flags and the compiler's target),
not a baseline of its own. Bessel `besselj`/`bessely` use the vendored musl implementation over
`xpp::math` (W163, #215), so they also give the same bits on every platform. From Git Bash:
`tools/examples_check.sh --bin xppautX.exe`.

`tools/goldencheck.py` (W31c, run by verify.sh) drives `xppautX --server`
through lecar.ode and vanderpol.ode to write PostScript (with a nullcline
and a direction field), SVG, kinescope GIFs and an array-print PostScript,
and compares each byte for byte (CRs removed) with tests/golden/
(`--update` rewrites them); a difference names the file and its first
differing line. They guard the conversion of the output code's fprintf
calls (W32b, W33).

verify.sh's checks about the source rather than the build (UTF-8, the
scripts' executable bit, stdoutcheck, formatcheck, literalcheck, sessioncheck, mathcheck, the LTO
type check, the dead-code checks, the global state check, the duplication check) are `tools/sourcecheck.sh`; CI runs them once, in its `source` job (with
`--warnings`: tools/warnings.sh's count, and web2's dist/types/unit
tests), and its linux-core job runs `verify.sh --no-source-checks`. Every
platform runs the same behaviour checks against its own build (`<platform>-core`)
and web2check against it (`<platform>-ui`), for linux, windows and macos;
`windows-clang` runs windows-core's checks against a clang build (its
servercheck/webcheck/autocheck/examples run side by side, W41), and
`windows-clang-sanitizers` (a 6-shard matrix, each building its own ASan
binary: below) asancheck with clang, beside it; a check step runs even
after another one failed (only a failed build stops them).

The front end (`web2/`, the page at `/`; a `/v1/` or `/v2/` bookmark
redirects to `/`; design and plan in docs/ui-v2.md). The classic page
(`web/`) and its check `tools/webtest.mjs` were removed at T18. Building
xppautX never needs Node: `web2/dist` is built from `web2/src` and
committed. After editing `web2/src`, from Git Bash:

    cd web2 && npm ci && npm run build && npm run check && npm test && npm run typecheck
    PATH=/c/msys64/ucrt64/bin:$PATH mingw32-make -j4 xppautx BUILDDIR=build/ucrt
    node tools/web2check.mjs      # state-level browser checks against ./xppautX.exe
                                  # (--only desktop,files,live,million runs a part)

and commit `web2/dist` with the source. Tests read `window.__xpp`
(`state()`, `actions()`, `sent()`, `plot()`, `diagram()`,
`diagramEvents()`), never pixels. `tools/cdp.mjs` is web2check.mjs's
headless-browser driver; web2check.mjs runs from Git Bash, where Node and
Chrome are (not WSL), and builds nothing: it drives `./xppautX[.exe]`
(`--bin` to point elsewhere). Its browser downloads into
build/web2check-downloads/<pid> (W142: set once when cdp.mjs starts the
browser, removed when it closes, a dead run's folder removed by the next),
never the user's Downloads.
A section a check fails in is rerun once (macos-ui, the slowest runner,
failed a different check nearly every time, always passing on a rerun,
W40): still failing is a FAIL, passing on the rerun is FLAKY (counted at
the end, not silently a pass). `XPP_CHECK_SLOW` scales safety timeouts for
a slow runner; it never scales a pass/fail budget (W58: performance is for
CI, not the program -- a check whose outcome depends on wall-clock speed is
a design problem, not a speed problem, and the program carries no code that
exists only to measure or slow it). Frame draw times, long tasks and Stop
latency print as `perf: <name> <value>` lines instead, measured but never
failed, from outside through CDP: `tools/cdp.mjs`'s `installPerfObserver`
injects a PerformanceObserver (long tasks) and a requestAnimationFrame
sampler into the page before it loads (`Page.addScriptToEvaluateOnNewDocument`,
so it needs no cooperation from web2/src), into `window.__xppPerf`, read
back through `Runtime.evaluate`; a frame's own draw time is not observable
from outside without the app's cooperation, so the gap before the next
animation frame stands in for it. A check whose *correctness* (not its
speed) needs a run still in progress uses a heavier model (W42's heavy.ode)
so that holds without racing the clock, and a check that can know its
stopping point ahead of time arms it exactly (`xpp::job::stop_at_rows`/`_point`,
the same mechanism `--script`'s abort replay uses: docs/protocol.md "Scripts"),
in a one-shot `--script` subprocess, rather than racing a live Abort.

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

`tools/deadcheck.py` (W46a; sourcecheck runs it with `--check`, a few
seconds) reads the source for the dead code a linker cannot see, across
core/ (uses counted in core/ and tests/, comments and literals
stripped): a macro nothing expands, a type nothing names, a struct field
nothing reads (only ever assigned counts as unread), a declaration with
no definition, a declaration repeated in a second header or again in a
.cpp (it lives only in the header of the file that defines it), `#if 0`
or `#ifdef` of a macro nothing defines (or one the file always defines
just above), commented-out code (a comment most of whose lines read as
statements; a comment that explains stays), a header nothing includes or
with nothing in it. `--check` fails on anything not in the
allowlist inside the script, each entry with its reason ("owner API" for
xpp_mem/xpp_io/xpp_files/xpp_log/xpp_math): delete dead code rather than
add an entry. Heuristic and line-based like dupcheck (a name that is also
a common word elsewhere reads as used). A .cpp's own unused macro and a
parameter set but never read are the compiler's (`-Wunused-macros`,
`-Wunused-but-set-parameter` in WARN, errors under WERROR=1, like
`-Wunused-but-set-variable`).

`tools/dupcheck.sh` (W30; sourcecheck runs it with `--check`, a few
seconds; `tools/dupcheck.py` does the work, `python3 tools/dupcheck.py`
with no wrapper also runs) is a duplication audit of core/: normalising
whitespace, comments and (for functions) identifiers, it lists duplicated
functions, runs of >= 16 duplicated lines and a struct/typedef defined
more than once (a declaration repeated in two headers is deadcheck's,
above). `--check`
fails on anything not in the allowlist inside the script, each entry
naming which W32 card (docs/roadmap.md) absorbs the copy, or "vendored/
numerical, keep" for a translated-Fortran/CVODE routine whose repeated
shape is the original source's own; merge into the owner rather than add
an entry. Heuristic and line-based (not a real C++ parser), so it cannot
see two implementations of the same algorithm written differently
(sgefa/sgesl vs. ge() vs. bandfac/bandsol, the three LU solves; gear.cpp's
eigen() vs. autlib1.cpp's eig()): those were found by hand for the W30
audit note, not by this script.

`tools/aliascheck.sh` (W113; sourcecheck runs it with `--check`, a
second; `tools/aliascheck.py` does the work) enforces one name per thing
(maintainer, 2026-10-01): it fails, in core/ and tests/, a second name for
one of our own names -- `using X = <our type>;`, `namespace a = b;`, a
`typedef` of our type, `using xpp::name;` inside namespace xpp, a
`#define` that only renames an identifier -- unless it is in the
allowlist inside the script, each entry with its reason; since W138 it
also fails, in web2/src, a renamed import (`import {a as b}`) and two
exported functions or consts of one name. Aliases of std::
types, new function-pointer types, typedefs of builtins and
`typedef struct {...} name;` definitions need no entry; the platform
shims and vendored CVODE/AUTO names are entries. Use the real name.

`make ltocheck` (run by tools/sourcecheck.sh, which verify.sh runs) links xppautX with LTO into build/lto
and fails on `-Wlto-type-mismatch`: an extern whose type or array bound
differs from its definition, which a normal build cannot see.

`make asan` builds xppautX with AddressSanitizer, LeakSanitizer and
UBSan into build/asan; `tools/asancheck.sh` (CI's `linux-sanitizers` job, not
verify.sh: it takes a few minutes) builds it and runs the smoke run,
every example, the unit tests, servercheck, webcheck and autocheck under
it (servercheck, webcheck and autocheck side by side, their waits doubled
by `XPP_CHECK_SLOW=2`), and fails on any report (written to build/asan/reports):

    tools/wslrun.sh tools/asancheck.sh

On Windows it runs with clang (below; `--no-leaks`: no LeakSanitizer
there), about 9 minutes:

    PATH=/c/msys64/clang64/bin:$PATH MAKE=mingw32-make tools/asancheck.sh --no-leaks --builddir build/clang-asan CC=clang CXX=clang++

`--only LIST` (W41) runs only some of the script's phases:
build,smoke,examples,unittests, then any of servercheck,webcheck,autocheck
(or the `checks` alias for all three; whichever of these are named run
side by side, `run_check` printing "NAME finished after Ns" as each
finishes) -- `autocheck=SECTION+SECTION...` (the names
`tools/autocheck.py --list` prints) narrows autocheck to just those
sections. `--skip-build` skips the build phase even when `--only`
includes it, for a shard given an already-built tree (a build is only
57-89s here, cheaper than an artifact round trip, so CI does not use this
currently; it stays for local iteration). Default: every phase, in order
(the whole script; Linux's `linux-sanitizers` and macOS's
`macos-sanitizers` still run it this way, a few minutes each).
`windows-clang-sanitizers` is a 6-shard matrix, each with its own build:
smoke+examples, unit tests, servercheck, webcheck, and autocheck split
into two (roughly half its sections each) since it is the likeliest of
the three side-by-side checks to be the long pole; each shard's own
"finished after Ns" lines say for sure, and whether a shard needs
splitting further.

`tools/asancheck.sh --no-leaks` (CI's `macos-sanitizers` job, Apple clang
on macos-latest) runs the same checks with LeakSanitizer's detect_leaks
off, since Apple Silicon runners do not support it; ASan and UBSan still
run there. The script is portable to macOS (nproc/sysctl, timeout/gtimeout,
md5sum/`md5 -q`), same as tools/examples_check.sh.

`make tsan` (W47e) builds xppautX with ThreadSanitizer into build/tsan; `tools/tsancheck.sh` builds it and runs servercheck and webcheck under it side by side (waits doubled by `XPP_CHECK_SLOW`), reports written to build/tsan/reports, and fails on any. It checks that the reader threads (xpp_http.cpp, the --server stdin reader) touch only the inbox and xpp_job's atomics. Linux only, a few minutes, not in verify.sh or CI yet; tools/tsan.supp only for code we do not own, a reason per line:

    tools/wslrun.sh tools/tsancheck.sh

The sanitizers do not see a read of memory never written; valgrind's
memcheck does. `make vg` builds xppautX at -O1 without sanitizers into
build/vg; `tools/valgrindcheck.sh` (W21; Linux, ~25 min on 32 threads, not
in CI or verify.sh) runs the smoke run, every example, the unit tests
(`TEST_RUNNER`), then servercheck and autocheck's sections side by side
under memcheck (`--origins`: also where a bad value came from, about
twice as slow, for fixing a report) and fails on
any report (build/vg/reports; tools/valgrind.supp only for code we do not
own). It sets `XPP_CHECK_SLOW=30`, which multiplies every wait of the
python checks (tools/xppclient.py).
`XPP_NO_THROTTLE=1` (core/xpp_job.h, W49) turns off `xpp::every`'s
throttling of progress events, so a check sees every intermediate event
on any machine instead of only on a slow one (servercheck's autoinfo
checks run under it).

Metrics: verify.sh's `C++: N / M sources` (core/*.cpp over all core
sources). The tree builds with 0 warnings (WSL gcc 15, MSYS2 UCRT64 gcc 16,
CI's MinGW gcc 15 and MSYS2 clang 22):
verify.sh builds with `make WERROR=1`, which makes every category ever
reported an error; `tools/warnings.sh` counts a clean build's warnings by
flag and file.

`sudo` inside WSL needs the user's password; apt installs must be run by the user.
The Windows-side gcc is MSYS2's UCRT64 (C:\msys64\ucrt64\bin, gcc 16: the
toolchain the release is built with, on the same UCRT runtime as CI's
windows-core; its binary needs no MSYS2 DLL). Use it for the native
build, from Git Bash, into build/ucrt (objects of another toolchain must
not be mixed in):

    PATH=/c/msys64/ucrt64/bin:$PATH mingw32-make -j4 xppautx BUILDDIR=build/ucrt
    python3 tools/servercheck.py --server ./xppautX.exe
    python3 tools/webcheck.py --bin ./xppautX.exe

MSYS2's CLANG64 toolchain (C:\msys64\clang64\bin: clang, libc++, lld,
compiler-rt; W23) is a second Windows compiler, never the default: CI's
`windows-clang` job. CI installs it through `.github/actions/msys2-clang64`
(W41), a composite action every windows-clang job uses: it caches the
installed clang64 tree keyed on `.github/msys2-clang64-packages.txt`, as a
single 7z archive (`%RUNNER_TEMP%\msys2-clang64.7z`) restored and extracted
with 7-Zip rather than through actions/cache's own directory caching --
that always tars a path before the runner's tar.exe extracts it file by
file, and on this tree (tens of thousands of small files) that alone took
over two minutes even on a cache hit; a single 7z file plus 7-Zip's own
(non-tar) extraction is seconds. Bump the packages file's version line to
force a fresh install (a package bump, say).
The Makefile knows clang (`CLANG`: gcc-only
`-Werror=` names dropped, webview in C++17 for libc++). Build it into its
own directory, the binary at build/clang/xppautX.exe (dynamically linked:
run it with clang64/bin on PATH):

    PATH=/c/msys64/clang64/bin:$PATH mingw32-make -j4 build/clang/xppautX.exe BUILDDIR=build/clang CC=clang CXX=clang++ WERROR=1

Windows API code lives only in `core/xpp_win32.cpp` (windows.h macros clash
with core names like `max`, `MessageBox`, `VARTYPE`). The exceptions
are the two files that include no core header but small APIs:
`core/xpp_http.cpp` (sockets) and `core/xpp_window.cpp` (the desktop
window's menu bar, dialogs and icon, behind `_WIN32`); windows.h never
reaches a header.

## Task agents

Work is run as a task board (docs/roadmap.md, docs/ui-v2.md). An agent
(.claude/agents/task-easy, task, task-hard: the model and effort by
difficulty) implements one card in the worktree its brief names:

- Work only there, with every path in this file adapted to it; commit on
  its branch and stop. Never merge, push, touch master, or write to GitHub.
- Read "Code quality" at the top first: every rule there holds for your
  change, and the rows enforced by "review" are what the reviewer reads
  your diff for, since no check will catch them (no fallbacks, all or
  nothing loads, named constants, errors with their file and line,
  security). Every source is C++ already: a new core/*.c or tests/*.c
  fails sourcecheck.
- Security (maintainer, 2026-10-01): a card that touches a trust
  boundary ("Code quality"'s security row: the HTTP server, its token,
  `/files`, the `file` command, the page's bound calls, starting a
  process, temp files, reading a file a user may have been sent) is
  thought through as an attack, not only as a feature: what a hostile
  page, file or name could make it do. Say in the report what you
  checked; a weakness you found but did not fix is reported, never left
  silent.
- Docs move with the change: docs/protocol.md for the protocol,
  docs/manual/ for the UI (then web2's `npm run build`), CHANGELOG.md,
  docs/xppautx-vs-xppaut.md and docs/xppaut-findings.md as "Code
  quality" says, docs/odex-quirks.md for a new .ode quirk (and name it in
  the report: the reviewer posts it to the VS Code extension's issue).
- No duplicated logic, strictly (maintainer, 2026-10-01; "Single source"
  under Conventions): search before you write any logic, not only a
  helper (a parse, a check of a value, a conversion, a loop over the
  model's names, a file read or write, a format, an error built). First
  the common modules that own kinds of operation (xpp_mem, xpp_log,
  xpp_io, xpp_files, xpp_ui, xpp_math, xpp_error, xpp_util, the Model's
  and the Session's own files; web2's src/ for the page), then the whole
  of core/ (or web2/src) by what the logic does: grep the operation's
  words and the names it touches, not only a function name you guess.
  Found: call it, or extend it in its module. Not found: write it once,
  in its owner. Never a second copy, nor the same algorithm in other
  words. If the owner is outside your card's files, say so in the report
  instead of copying it. The report names what you searched for and what
  you reused or extended; a duplicate the review finds sends the card
  back.
- Gates: the per-task tier above. Iterate with `web2check --only <your
  sections>`; never run verify.sh, the full web2check or
  tools/asancheck.sh. Commit before `tools/wslrun.sh` (it checks HEAD
  and refuses uncommitted changes). The machine is a laptop that
  overheats (maintainer, 2026-09-29): build with -j4, and never run two
  heavy things side by side (a build, servercheck, autocheck, web2check).
- Keep token use low: read the parts of files you need (grep, `sed -n`
  ranges), pipe check output through tail/grep, never paste full logs.
- Leave no `until`/`while` sleep loops or background runs behind.
- Stop only your own processes, by their PID (`taskkill /PID`, `kill`),
  never by name (`taskkill /IM xppautX.exe`, `pkill xppautX`): that also
  ends the maintainer's own xppautX and other agents' runs.
- Background tasks are registered (maintainer, 2026-09-27): every
  background run (a command run in the background, a background agent, a
  server or program left running) gets a line in
  `C:\gitRepos\xppautX\.claude\background-tasks.md` (the main
  checkout's, whatever worktree you are in; local, not committed) the
  moment it starts: its id, what it runs, where, who started it and who
  closes it. Whoever closes it stops it or confirms it ended, then
  deletes its line. An agent closes its own before its final report and
  says the file holds none of its lines; the reviewer checks the file is
  empty of finished work before a merge, a push, and each report to the
  maintainer.
- Before the final report, squash work-in-progress and debug commits
  ("wip", "w", "dbg") into commits whose messages say what and why
  (Conventions' commit messages): the branch is merged as it stands.
- Final report: at most 15 lines: what changed, gate results as counts,
  anything unfinished or doubtful.

The reviewer (the main session) reviews, refactors, merges, runs the
5-task tier, pushes when the user says so, and then closes the finished
cards' issues (above). A new roadmap card gets its GitHub issue at once.
Every card's review, before its merge, does three things (maintainer,
2026-10-01): checks the card is met and every "review" row of Code
quality holds; checks the agent followed this section (it stayed in its
card's scope, searched before writing logic and duplicated none: each new
function or block in the diff is grepped for an existing one, ran the
gates its report claims, squashed its wip commits,
closed its register lines, moved the docs, reported what it checked at a
trust boundary), naming a broken rule in the card's note so the next
brief says it louder; and simplifies and refactors the diff even when it
passes: what the change does not need is removed (a dead branch, a flag
nothing varies, a check of what cannot happen, a comment that restates
the code), a new helper moves into its owner, code the diff copied is
merged, a function it made long is split, names say what things are. The
behaviour stays: the card's gates are rerun after it, and it is committed
apart from the agent's work as "Review: ...".

Models are named by family, never by version (maintainer, 2026-10-01),
so a newer one is used the day it ships: the agents' `model:` is the bare
alias (haiku, sonnet, opus). Codex agents (maintainer, 2026-10-01) are a
second pool: docs and one-file mechanical edits luna (low effort), every
other code card sol (medium); a card whose cause or design is still
unknown goes to `task-hard` on sonnet or opus (the Agent tool's model
override; sonnet, high did W159), or to Codex sol at high effort; never
astra, which is token hungry (maintainer, 2026-10-01). Start each card on the cheapest model and
effort that may do it and adapt (maintainer, 2026-10-01: agents code
well from clear instructions): the ladder, effort by effort and within one
the cheaper model first (maintainer): low, then medium, then high, each
with luna, haiku, sol, sonnet, opus in that order; a
failure that shows the card is clearly harder than its step skips ahead
rather than climbing one rung at a time (a Claude agent's effort is its definition's:
`task-easy` low, `task` medium, `task-hard` high, the model set by the
Agent tool's override); any card may start at its bottom
when its brief is precise (the files and lines, what done means, the
exact gates), a card the brief cannot pin down starts higher; when the
review finds the work fell short (a missed requirement, a wrong cause, a
fix the reviewer had to redo), the card goes back one step up with the
review's list, and the step that did the work is noted
on the card, so the starting point per kind of card follows the record
(maintainer, 2026-10-03, from this repo's and laser_pointer's record of
about 30 cards: any code card starts at sol, medium, or sonnet, medium
when Codex has no room, which did about 17 of them, a 36-file refactor
and the protocol and trust-boundary cards included, with at most one
review round; luna and haiku only for docs and one-file mechanical edits,
since luna fell short on 5 of 7 other cards and haiku on 2 of 2; size is
not difficulty: a large, precisely briefed change stays at medium, as sol
at high on four such cards brought the same review fixes at far more
tokens; high effort only for an unknown, a cause to find or a design the
brief cannot settle, as sonnet, high did W159's numerics, or as the climb
when medium fell short); each the newest listed model of its family
(the task-board skill's `codex_model.py <family>`: Codex has no aliases),
run by the reviewer with `codex exec -C <worktree> -s workspace-write -m
<model> -o <report>` and registered by the reviewer. A Codex agent follows
every rule of this section as written, with four differences of its
sandbox: it writes only in its worktree and the system's temp folder, so
it starts no background runs and does not touch the register; it cannot commit (the worktree's git
data is in the main checkout's .git, whose lock file the Windows sandbox
refuses even with `--add-dir`, W153), so it leaves its work uncommitted
and its report ends with the commit message, which the reviewer commits
with the Codex trailer before the review; it runs its gates from Git Bash in the
worktree (the UCRT build `mingw32-make -j4 xppautx BUILDDIR=build/ucrt
WERROR=1`, its unit tests, servercheck/autocheck against that exe,
web2's checks and `web2check --only`, run as any agent runs them: the
reviewer starts Codex with `XPP_CHECK_NO_BROWSER_SANDBOX=1` in its
environment, which its commands inherit, since Chrome's own sandbox
cannot start inside Codex's, W168); and the WSL gates
(`tools/wslrun.sh`) are the reviewer's, run on its branch at review, with
the card's web2check sections again (the task-board skill's
`codex_gates.sh`, before the review: a failure goes back to the same
Codex session). Before a batch the
reviewer reads both pools' limits (the app's usage for Claude, the
task-board skill's `codex_limits.py` for Codex) and gives each card to the
pool with room, saying which in the batch proposal.

## Architecture of the split (phase 2)

- `core/xpp_ui.h` is the seam: an `XppUi` table of callbacks. Core code
  calls the historical names (`err_msg`, `new_float`, `redraw_params`,
  `TwoChoice`, `ALINE`, `set_color`, ...); those are dispatchers in
  `core/xpp_ui.cpp` with headless defaults. `core/ui_json.cpp` installs its
  own table before it serves a session. Adding a UI call from core: add a
  field, a headless default, a dispatcher, and a `j_` function in the
  `core/json_*.cpp` file of its responsibility (declared in
  `core/ui_json_internal.h`) with its entry in ui_json.cpp's `make_json_ui`.
  A call that acts on a Session's windows or data takes the caller's
  `xpp::Session &` as its first parameter, field and dispatcher alike
  (W47d6: `redraw_the_graph(s)`, `new_float(s, ...)`); the front end's own
  operations core code reaches with no Session (an ask, a checkpoint)
  take its client's, `xpp::json::client()`.
- State is grouped into structs, each defined by the module that owns it
  (W7c). What a run changes is a member of `xpp::Session`
  (core/session.h, W47c): `data_store` (storage.h, W32d: the stored
  columns, their rows and the current point; histogram and Fourier
  results borrow columns through `lend_columns`), `plot_windows`
  (many_pops.h: the graphs, the active one, the Simulplot list,
  draw_win), `frozen_curves` and `plot_export` (graf_par.h), the
  integrator's state, AUTO's (`auto_state`, `auto_lib`, and the
  `diagram`, read through `diagram_count`/`diagram_point`/
  `diagram_first`/`_next`/`_prev`), the browser, the kinescope, the
  numerics and plot settings, the parser's working state, `sliders` and
  `options_set` (which @ options a load gave, W119), the tables, the boundary conditions in use, and the
  drawing, label, array plot and animator state; a function takes the
  `xpp::Session &s` (or the part it uses) from its caller, never a
  current one ("No global state" below). What stays process-wide is a global of its
  owner: `program` (xpp_globals.h: interactive, version, tutorial),
  `batch_options` (xpp_batch.h), `xpp::log_settings` (xpp_log.h),
  `color_table` (colormap.h), `text_metrics` (xpp_ui.h), the command-line
  flags (comline.h). Include the owner's header, never redeclare
  anything `extern` in a .cpp.
  `core/xpp_util.cpp`, `core/browse_data.cpp`, `core/colormap.cpp`,
  `core/menus.cpp` hold pure code moved out of those files.
- Core structs that hold a window store an `XppWinId` (unsigned long); see
  `core/xpp_types.h`.
- `core/commands.cpp` is the command layer (phase 3): `commander` (keys),
  `run_the_commands` (`M_*` ids), and every pop-up menu. Menus are
  `XppMenu` data in `core/menus.cpp`; front ends show them via
  `xpp::ui.menu_choose` and switch the main menu via `xpp::ui.show_menu`.
  Command logic is all core (phase 3 step 2); `XppUi` only holds
  interaction primitives, window management and a few whole dialogs.
- `core/xpp_batch.cpp` holds `xpp::load_model()`, the start every mode
  shares, and `xpp::batch_start()`, the set-up with no interface (--silent,
  a unit test); what --silent runs is `core/json_silent.cpp`'s built-in
  script, played through the JSON front end (W56).
- `core/ui_json.cpp` + `core/xppautx_main.cpp` (`SERVER_SOURCES`) are the JSON
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
- `xppautX` (`make xppautx`) is one program: `core/xppautx_main.cpp` picks
  the desktop window (the default), browser mode (`--browser`/`--web`, or
  `--no-open`), `--server` (the protocol on stdin/stdout) or `--silent`
  (no interface at all: json_ui_silent plays json_silent.cpp's built-in
  script of protocol commands, W56). The window (W13a) is
  `core/xpp_window.cpp` (xpp_window.h, namespace `xpp::window`) over the vendored
  `third_party/webview` (built as its own object, `webview.o`, from
  `core/xpp_webview.cpp`, which includes it and adds `xpp::webview_create`:
  webview_create with the error it failed with, W35e; WebView2 through the SDK headers in
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
  thread (untested); there a file Finder opens in xppautX.app comes as an
  open-documents Apple Event, which xpp_window.cpp handles (W91: the
  launch's is the model, `xpp::window::launch_document`, a later one opens
  as File > Open model; CI's macos-core checks it with `open -a`). Closing the window (its x, or the menu bar's Quit; on macOS also
  Cmd+Q and the Dock's Quit, through `windowShouldClose:` and
  `applicationShouldTerminate:` added to webview's delegate classes; W59d,
  W110) never stops a computation: the window calls the page's
  `__xppQuit` (web2/src/desktop.ts, through webview_eval, as Help does),
  and the window stays. While the core is idle the page sends
  `{"cmd":"quit","ask":true}` and the core asks "Save this session
  first?" (the one leave question, model_switch.cpp's
  `xpp_session_may_leave`, as F Q does); while a command runs the page
  asks the same question itself (its wording, answers and keys from
  hello's `quit`, model_switch.h's `xpp::quit_question` and `LEAVE_*`: one
  source), the run going on: Cancel leaves it alone, Save session sends
  `{"cmd":"quit","save":true}` (the run stops, the session and a recording
  in progress are saved, then bye), Don't save calls the window's bound
  `__xppCloseWindow`, which closes it as below. A plain `{"cmd":"quit"}`
  (scripts, --server clients) quits at once. When the window closes
  (Don't save, a page without the hook, or the core already exiting) it
  pushes the plain quit and calls `xpp::http::release()`, and ends the
  process after EXIT_GRACE even if a computation never reaches a
  checkpoint; the core's
  exit closes the window after a bye, and after an error leaves it open
  on the log until it is closed (xpp_http's at_exit waits for that, or
  Ctrl+C). If the web view cannot start (no WebView2 runtime, no
  display) xppautX logs it and falls back to browser mode. Its menu bar
  (Win32 menu; GTK 3 menu bar on Linux; on macOS only the app menu's Quit xppautX, Cmd+Q): File > Open
  model and Reload (W61: loaded in this process, core/model_switch.cpp,
  once the command has returned; a failed load keeps the model before;
  Open model picks the file in the OS dialog below), Quit; a `file` ask
  opens the OS's own open or save dialog (W88: web2 calls the page's
  `__xppFileDialog`, bound with webview_bind and run on the UI thread by
  `pick_file` in xpp_window.cpp, and answers the ask with the true path;
  its filter is web2's `wildExtensions`; browser mode keeps the browser's
  picker, W90); Help > Manual and Keyboard shortcuts (web2's
  `window.__xppOpenHelp`, web2/src/desktop.ts, through webview_eval),
  About (a native message box). The icon is `assets/icon.svg`, made into
  `assets/icon.ico` by `tools/make_icons.py`, compiled into the Windows
  exe by `assets/xppautx.rc`. `tools/modecheck.sh` (verify.sh) checks
  `--help`, `--browser` and `--no-open`, and on Linux with the window
  xppautX's NEEDED, the failed load and a load with no display. It carries
  `core/xpp_http.cpp` (HTTP + Server-Sent Events on
  127.0.0.1, threads, sockets; it includes no core header but the small
  APIs of xpp_mem.h, xpp_inbox.h and xpp_files.h) and
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
  them from there; `--silent` starts no reader. Abort and Quit cancel the
  running job from the reader thread (`core/xpp_job.{h,cpp}`, by sequence
  number); computations ask `xpp::job::cancelled()` or go through the
  throttled checkpoints `my_abort()`/`byeauto_()`. ui_json.cpp's `classify()`
  says which lines are control lines; docs/protocol.md "Commands during a
  command" is the contract. Computations report how far they got to
  xpp_job (`xpp::job::report_rows` per stored row in integrate.cpp's `row_stored()`,
  which also feeds `XppUi.rows_stored` (web2's live `series` appends),
  `xpp::job::report_point` per AUTO point in autevd.cpp addbif): a cancelled
  command sends `stopped` with that, and `--script` replays a recorded
  `{"cmd":"abort","at":...}` by arming `xpp::job::stop_at_rows/point` for
  the line before it (ui_json.cpp `script_arm_stop`). Rebuild xppautX
  after rebuilding `web2/dist`: the page is compiled in.
- `core/xpp_log.{h,cpp}` is the one logging module, quiet by default:
  `xpp::log(level, fmt, args...)` with ERROR/WARN/INFO/DEBUG, threshold
  WARN, raised by `--verbose`/`--debug`. The caller writes the newline;
  output goes to
  `--logfile`'s file if given, else stderr, which browser mode shows in the
  page's log. There is no `plintf()` any more (retired at W25, ~620
  call sites): code prefers `xpp::log` (std::format-checked, same idea as
  xpp::format in xpp_io.h) whenever the format string converts
  mechanically, and falls back to `xpp::log_printf(level, fmt, ...)`
  (printf's, its format checked by the compiler's format attribute) for a
  dynamic width/precision (`%*s`, `%.*s`) or a pointer destination
  (W109a renamed the C `xpp_log` to it).
  `err_msg()`'s headless default is ERROR. An INFO message additionally
  honours the model's own `@ quiet=1` (`xpp::log_settings.verbose`), what
  plintf() used to gate itself on. Picking a level for a new message:
  ERROR stops the action (a model that does not parse, a file that
  cannot be opened — usually via err_msg); WARN is a real problem that
  is not fatal (a duplicate name, a CLI usage mistake, a numerical
  warning); INFO is progress or a result a user reading `--verbose`
  output or the browser page's log panel wants (the startup banner,
  parser stats, a confirmation); DEBUG (`xpp::log(XPP_LOG_DEBUG, ...)`)
  is developer tracing, dumps and
  internal chatter nobody reads by default — when a message carries
  nothing at all (a bare "here", an unused value dump), remove it rather
  than downgrading it. `tools/stdoutcheck.sh` also fails a new `plintf(`
  call so it cannot creep back in.
  An error is an `xpp::Error` (core/xpp_error.h, W140a): what failed, a
  `where` for the log and an `xpp::Place` (file, line, col, the line as
  written; 0 when unknown, and a file with no line is one that could not
  be read, which the page offers to add). `Error::text()` is the one
  rendering, `file:line:col: what`, for the console, the log and --silent;
  every error event the page gets (`error`, and a `message` error) carries
  the same fields (docs/protocol.md "Errors"). `err_msg`, `show_error`,
  `fail`/`fail_reading`, `err_reading` and `model_failed` take it. While a
  model loads, its ERROR/WARN lines are held and printed with their place,
  and a failed load is printed once, by `load_model`.
  `tools/errorcheck.py` (sourcecheck) fails an error made or reported with
  no place (err_msg, an ERROR log line, `fail` without a Place or with an
  empty one) unless its `ALLOWED` list names it with its reason (a result
  shown through err_msg until W133, out of memory, CVODE's argument
  checks); tests/errors.baseline is empty since W140b. A command's error is
  `command_error(cmd, what)` at `command_place()` (xpp_ui.h: in a --script
  or a .recx Play, the step's file and line), a model line's
  `model_place(m, name)` (model_files.h).
  AUTO's table goes through `xpp::log_auto()` (`xpp::log_auto_printf`
  for printf's formats): INFO on the console, always
  written in browser mode, where the AUTO window's Output panel shows it.
  The core never prints to stdout or stderr directly; `tools/stdoutcheck.sh`
  (run by verify.sh) enforces it, with a short allowlist inside the script
  for the handful of lines that are legitimately direct (the `--version`
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
- Pop-up menu arrays in menus.cpp (`main_menu` etc.) start with the title:
  item i is `main_menu[i+1]` with key `main_menu_keys[i]`.

## Memory

- Every core allocation is C++: `std::vector`, `std::string`,
  `std::unique_ptr` (RAII) instead of a hand-paired malloc/free. The raw
  allocator this section used to describe (`xpp_malloc`/`xpp_calloc`/
  `xpp_realloc`/`xpp_strdup`/`xpp_free`, `XPP_MEM_FAIL_AT`,
  `XPP_MEM_INIT`) was retired at W48, once tabular.cpp -- its last
  caller -- moved to a `std::vector`. What is left is `xpp::out_of_memory`
  (core/xpp_mem.h): code whose `std::string`/`std::vector` could not
  allocate (it caught `std::bad_alloc` where no exception may pass: a
  `noexcept` function, a callback a C library or the system calls)
  calls it instead of letting the exception escape, for the same loud,
  final exit (an ERROR naming what was being built, then exit 1) the
  allocator itself always took; `--debug` no longer prints allocation
  counts. `XPP_WINDOW_FAIL_LOAD=1` (xpp_window_loader.cpp) is the Linux
  window's own like hook. The exceptions (memory a library allocates or
  frees) are listed in xpp_mem.h's comment; add any new one there and in
  `tools/alloccheck.sh` (sourcecheck.sh), which fails any other direct
  call to the C library's malloc/calloc/realloc/strdup/free.
- A leak or memory error LeakSanitizer/ASan/UBSan reports in our code is
  fixed, never suppressed; tools/lsan.supp is only for code we do not own,
  with a reason per line. Memory kept for the program's life (a global set
  once) needs no free at exit: LSan sees it as reachable.

## Strings and I/O

- The core formats text with `xpp::format`/`xpp::number` (`core/xpp_io.h`,
  `std::format`/`std::to_chars`, type-checked at compile time), never
  `sprintf`/`strcpy`/`vsprintf` into a fixed buffer: `tools/formatcheck.sh`
  (run by verify.sh) fails a new one. The C text API this section used to
  describe (`xpp_snprintf`/`xpp_strlcpy`/`xpp_strlcat`,
  `XPP_SPRINTF`/`XPP_STRCPY`/`XPP_STRCAT`, `XPP_FORMAT_TO_BUF`) was
  retired at W48, once every core file was C++ and nothing called it any
  more; the build is `-std=c++23`/`gnu++23` for `xpp::format`'s
  `std::format` (present and warning-clean on WSL gcc 15, CI's MinGW
  gcc 15 and MSYS2 UCRT64 gcc 16; avoid library parts newer than the
  oldest of these, gcc 15, or than clang's libc++ ships).
- New code that reads or writes a file (`core/xpp_io.h`, W11 step 3) uses
  its move-only handles over `fopen`/`fgets`/`fscanf`/`feof`:
  `xpp::LineReader` for a whole line of any length (`next()`; no
  fixed-buffer cut, no `while(!feof)` reading the last line twice; CR/LF
  tolerant), `xpp::TokenReader` where the file is whitespace-separated
  numbers read like `fscanf` (its `read()` overloads pick the conversion
  from the target's type and are false where fscanf would not return 1),
  and `xpp::Writer`, which opens a temp file next to `path` ("w" text
  mode) for the writing to go through (`print()`, or `file()` with
  ordinary `fprintf`); `commit()` closes it and renames it into place
  (`xpp::files::replace_file`, the same cross-platform rename
  `core/xpp_files.cpp`'s own uploads use, not duplicated here), logging
  an ERROR and leaving the original file untouched on failure; `abort()`,
  or a Writer that goes without either, discards the temp file without
  touching `path` at all. A reader opened from a path owns its file;
  `attach(fp)` reads a `FILE *` the caller already owns (never closes it)
  for a helper that takes a plain `FILE *` from elsewhere, such as
  `lunch-new.cpp`'s `io_int`/`io_double` or `diagram.cpp`'s
  `load_diagram`. Integer columns printed flush against each other
  (AUTO's `%5ld` label lines in fort.8/.s) are read with `read(long&)`
  (fscanf `%ld`'s own grammar, not a whole token), and `skip_line()`
  skips the rest of a line; `xpp::Writer::binary(path)` writes a file
  byte for byte, and `xpp::print(FILE *, ...)` is the one overload for a
  stream. (The C API under the handles, `xpp_line_reader_*`,
  `xpp_token_reader_*` and `xpp_writer_*`, was retired at W109a, and
  with it the append mode nothing used.) Reading a
  whole file's bytes is `xpp::open_read`/`open_read_binary` (an
  `xpp::UniqueFile`). Asking before overwriting a user's file is
  `open_writer_asking` (browse.h), a Writer that commits in place.
- Every other file operation is `core/xpp_files.h` (W32b), namespace
  `xpp::files`: a stream kept open across calls (`open_stream`: AUTO's
  fort.3/7/8/9 during a run, the array plot's GIF movie, an input
  script), an exclusive create (`create_new`, every temp file), `exists`,
  `dir_writable`, `remove`, `copy`, `prepend` (AUTO's "append": new bytes
  ahead of the file's own), `move`, and AUTO's scratch folders
  (`make_temp_dir`, `remove_temp_dir`, `cleanup_stale_temp_dirs`; the
  Windows API parts in xpp_win32.cpp); a path they only read is a
  `std::string_view`.
  The folder half (listing and its wildcard match, the file selector's
  current folder, is_dir, dir_writable, AUTO's scratch folders) is
  `core/xpp_files_dir.cpp` behind the same header (W46b, which folded
  read_dir in); the two share the platform primitives of
  `core/xpp_files_internal.h`.
  `tools/filecheck.sh` (sourcecheck) counts each core file's direct
  fopen/freopen/remove/rename/unlink/mkdir/rmdir/opendir/tmpfile calls
  outside xpp_files, xpp_io and xpp_win32 against `tests/files.baseline`
  and fails on growth; the W33 sweeps take the rest to 0
  (`--update` after a drop).

## C and C++

Every core source is C++ since W27 (2026-09-25; decided 2026-09-23,
converted file by file, then the remaining 71 at once), and so are the
unit tests (tests/test_job.c, the last C one, at W109f): tools/sourcecheck.sh
fails a new core/*.c or tests/*.c. verify.sh's `C++: N / M sources` is
N = M. The API between the files is C++ too (W109, maintainer 2026-09-30,
done in six stages by W109f, 2026-10-01: below); what follows the goals
is how the files were converted, kept for a file brought in from outside.

The extension was only the first step. The goal (maintainer, 2026-09-25)
is safe C++ in place of the unsafe C idioms, file by file (the W29 cards):
- memory: `std::vector`, `std::string`, `std::unique_ptr` (RAII) instead of
  hand-paired allocation (xpp_mem's raw allocator was retired at W48);
- text: `std::string` for text that is kept, `std::string_view` for a
  parameter that only reads it, instead of fixed `char` buffers and
  C string copies (the C text API was retired at W48); `const char *` only where a
  C function needs a NUL-terminated string, for a string literal a function
  returns, or across the few `extern "C"` boundaries left (below);
- formatting: `xpp::format`/`xpp::log` (type-checked) instead of printf
  formats;
- files: `xpp::LineReader`/`TokenReader`/`Writer` instead of
  `fopen`/`fscanf`/`fgets`;
- arrays: `std::array`, `std::vector`, `std::span` instead of raw arrays
  and pointer-plus-length pairs; `static_cast` instead of C casts.
Numerics do not change (the md5s). A task that touches a file moves what it
touches to these. `tools/unsafecheck.sh` (comments stripped first, like
tools/formatcheck.sh) counts every core/*.cpp and core/*.h's unsafe C
idioms per file, in five categories (memory: malloc/free-style raw
allocation and new[]/delete[]; buffers: fixed `char name[N]`
declarations; text: the raw strcpy/sprintf/fprintf family; files: fopen/fscanf/fgets and
the rest of stdio; casts: a heuristic match on a C-style cast). `--check`
(verify.sh runs this) compares against the committed `tests/unsafe.baseline`
and fails naming any file/category whose count grew past it (a count that
dropped is fine); `--update` rewrites the baseline after an intended
change. Plain `tools/unsafecheck.sh` prints the per-file table.

`tools/globalcheck.sh` (W47a; sourcecheck runs it with `--check` on the
objects deadcode.sh has just built in build/deadcode, a few seconds)
lists each core object's external mutable data symbols -- nm's B
(.bss), D (.data) and C (common), but not the D symbols in
.data.rel.ro, const data the loader relocates (a const table of
pointers) -- and compares their count per file with the committed
`tests/globals.baseline`, failing on growth and naming the file's
symbols; since W120 it counts internal-linkage state too (nm's b and d: a
file-scope static, an anonymous-namespace variable, a static local),
against `tests/globals_internal.baseline`. Each baseline line is `FILE
COUNT REASON`, the reason saying why the file keeps it or which card
takes it, and a line without one fails. A const global does not count.
`--update` rewrites the baselines after a drop; plain
`tools/globalcheck.sh` builds build/obj and prints every symbol by file
(`--builddir DIR` reads objects already built there). Linux only, like
deadcode.sh (GNU nm's section column). At W47a: 300 (from 461); at W47b: 266; at W47c: 21; at W120: 16
external, 615 internal at -O0 (461 of them vendored EISPACK).

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
- Text a function only reads is `std::string_view` (W109; the dialog
  API, xpp_ui.h, since W109e), and `const char *` only where the text goes
  on to a C function or a library that needs it NUL-terminated (the
  drawing's text primitives, the protocol's JSON reader, the Windows API)
  or across the C boundary below; `char *` says the function writes into it. A string literal is never cast to
  `char *`: `tools/literalcheck.sh` (sourcecheck.sh) fails one.
- The API is C++ (W109, maintainer 2026-09-30, replacing W27's "the API
  stays C"): a core header declares C++ functions, in namespace `xpp`
  (an owner module's own namespace inside it where it has one:
  `xpp::files`), with C++ types at the boundary: `std::string_view` for
  text a function only reads, `std::span` instead of a pointer and a
  length, references for an out-parameter, `bool` for a yes/no, a class
  with RAII for a handle. No `extern "C"` and no `#ifdef __cplusplus`
  guard: a header is C++ only. `extern "C"` stays only where C really
  calls across: the Linux window library's one export and its tables
  (xpp_window_plugin.h: dlsym finds `xpp_window_plugin_init` by its C
  name), data the build generates as C (tools/embed.c's web assets,
  tools/embed_bytes.c's icon and window library), and a C library's own
  function a header hides (rand_s). `tools/externcheck.sh` (sourcecheck)
  fails an `extern "C"` in core/ or tests/ that its allowlist does not
  name, each entry with its reason. W109 is done (its six stages,
  W109a-f, docs/roadmap.md "W109: the core's API in C++, in stages"):
  what the allowlist names is that permanent boundary, and the vendored
  CVODE's seven headers (band, cvband, cvdense, cvode, dense, llnlmath,
  vector), whose C API stays until W34 (#72) decides whether SUNDIALS
  replaces CVODE. Code brought in from outside follows what each stage
  did: drop the guard, put the declarations in the namespace, give the
  boundary C++ types, rename the callers (a function's module prefix
  becomes its namespace: `xpp_files_exists` is `xpp::files::exists`,
  `xpp_job_cancelled` `xpp::job::cancelled`, `xpp_log` is
  `xpp::log_printf`). Each stage was timed before and after (examples_check's
  wall time, kuramot100.ode --silent, an AUTO run, and callgrind's
  instruction counts for the last two, which do not depend on code
  layout): a stage slower beyond the noise was not merged, and if C++
  had cost speed the card would have stopped (maintainer, 2026-09-30); none
  did.
- No exception may cross code compiled as C (a C library, an OS or
  webview callback): C++ called from there catches what it can throw
  (std::bad_alloc included) or uses only non-throwing code, and C
  callbacks called from C++ are assumed not to throw. The core itself is
  all C++, so an exception may pass through its functions:
  `xpp::LoadFailed` does, from the parser to xpp::load_model (W47c).
- Use C++ where it clarifies: RAII (std::vector, std::string,
  std::unique_ptr) for allocations the task touches, std::atomic,
  std::chrono, anonymous namespaces for file-local state. No behaviour
  change: verify.sh's checksums still guard the numerics.
- CVODE is C++ (W27a): llnltyps.h's `bool` is C++'s own, and the header
  refuses to be included from C (`#error`).
- `core/xpp_job.cpp` was the first file converted (std::atomic,
  std::chrono).

## Conventions

- Single source (maintainer, 2026-09-26): every kind of operation lives
  in one module, so a bug in it is fixed in one place. Memory is
  xpp_mem, logging xpp_log, text formatting and reading/writing text
  xpp_io, files and folders xpp_files, dialogs xpp_ui, numerics (the
  Fourier transform, random numbers, linear solves, special functions)
  xpp_math (core/xpp_math.h says which copies stay elsewhere and why). A new helper
  goes into the module that owns its kind of operation, never a local
  copy in the file that needs it (no per-file `put`/`print`/`str`
  wrappers, no second typedef of a struct, no platform code outside its
  platform file). Where two copies exist, merging them into the owner is
  part of any task that touches one. The source checks enforce what they
  can (alloccheck, stdoutcheck, formatcheck, literalcheck).
  Unify the operation, not only its calls (maintainer, 2026-10-01): W11,
  W32b and the W33 sweeps put every file open, line/token read and safe
  write into xpp_io/xpp_files, and filecheck counts the raw calls; but
  "load one of our files" was never one operation: .set (lunch-new's
  io_int, one function for reading and writing by a flag), .snapx,
  .autox, .autoset and .recx each read and applied values in their own
  loop, so a bad line left the lines before it applied, five copies of
  the same bug that no check could see. A card that unifies something
  names the operation at the level a user meets it ("load a file of
  ours: all or nothing"), lists every place that performs it, and says
  what proves they all go through the one owner; a check that counts
  calls proves the calls only.

- No global state (maintainer, 2026-09-27): global variables are avoided.
  A new piece of state goes into its owner's struct, never a new global;
  what a load produces belongs to `xpp::Model` (core/model.h, from W46c),
  what a run changes to `xpp::Session`, passed as `Model&`/`Session&`
  (W47a-d in docs/roadmap.md took the ~500 globals there in stages; 21
  process-wide ones are left). There is no current Model or Session to
  read (W47d6 retired `xpp::model()` and `xpp::session()`): a function
  takes `Session&` (or `const Model&`, or the part it uses) from its
  caller, and a Session knows its Model (`s.model()`). The one global
  that holds Sessions is the session list (`xpp::client_session()`,
  core/session.h: one Session per client, and the process serves one),
  read only where a Session is chosen and passed down from there: a
  protocol command (ui_json.cpp's handle_line, `run(Session&, line)`,
  `commander`, `run_the_commands`), the program's start and exit
  (xppautx_main.cpp), and the JSON front end's own asks and checkpoints,
  which core code reaches through the XppUi seam with no Session
  (`xpp::json::client()`, ui_json_internal.h). A load gives the Session
  it made (`xpp::load_model` returns it, `xpp::Loaded`); the program's
  start, --silent, --convert and File > Open model take it from there.
  `tools/sessioncheck.sh` (sourcecheck) fails a read of the list or of
  client() outside those owners, and any `xpp::session()`/`xpp::model()`.
  A hot loop gets the Session once where the run starts (the solvers
  through their Solver or IntegratorState, AUTO's routines through
  `iap->lib`). A load (`xpp::Load` in xpp::load_model, core/session.h) builds a
  fresh Model and Session and keeps them only when it finishes: a parse
  error's `xpp::model_failed` throws `xpp::LoadFailed`, and the Model and
  Session before are the client's again, untouched (W47c). A value nothing writes after initialization is
  `const`/`constexpr`, and one file's own state has internal linkage.
  `tools/globalcheck.sh` (sourcecheck) fails a file whose mutable data,
  external or internal-linkage, grows past its baseline
  (`tests/globals.baseline`, `tests/globals_internal.baseline`).

- Upstream mergeability is no longer a goal (2026-09-23): refactor for
  single responsibility and clean code, numerics included. Numerical
  results must not change: tools/verify.sh's checksums and saved AUTO
  diagram are the guard.
- Tests check data (output files, protocol events, UI state), never
  pixels: do not add screenshot comparisons.
- A model's names (variables, parameters, aux, functions, arguments,
  tables) have no length limit (W76, which retired `XPP_NAME_MAX`,
  `MAX_LEN_SBOX` and `name_too_long`): they are `std::string` everywhere,
  and no dialog, form or protocol field cuts one (the `string` and `form`
  asks carry no `max`). A display or file column of fixed width (AUTO's
  printed headings and info strip) shortens with `short_name()`
  (xpp_util.cpp, ends in `~`); the JSON front end always sends names
  whole. tools/models/longnames.ode (200-character names), autocheck's
  `names` section and tests/test_names.cpp are the test.
- The refactoring scripts under `tools/` (guard_x11_headers.py, move_funcs.py,
  ui_seam_refactor.py, phase2_step*.py, cxx_guard_headers.py) are one-shot and already applied;
  keep them for the record, do not re-run them.
- Files on disk may be CRLF (Windows checkout); scripts that edit them must
  preserve line endings. Python written with `newline=''` does.
- Commit messages: imperative subject, body explains why, include the metric
  deltas.
