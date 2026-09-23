# Roadmap: phase 4

The task board for the work after the 2026-09-23 batch (issues #1, #3,
#5–#11). Direction, decided by the maintainer:

- The UI is rewritten for the modern web and is no longer a pixel copy of
  X11. It is responsive, follows WCAG 2.2 AA, draws from data, and uses the
  browser's own file dialogs.
- Tests check data (output files, protocol events, UI state), never pixels.
- The numerical core stays in C and converts to C++ progressively: a task
  that fixes or refactors a file converts that file.
- Upstream mergeability is no longer a goal; single responsibility and
  clean code are. Numerical results must not change: tools/verify.sh
  guards them.
- The X11 front end is removed once the new UI covers it.

Status: `ready` (can start), `running`, `review`, `done`, `blocked`
(waits for the task named in "Needs"). Each card is mirrored as a GitHub
issue; the card here is the one kept up to date.

| ID  | Issue | Task | Needs | Status |
|-----|-------|------|-------|--------|
| W0  | #12 | C/C++ mixed build | none | done (cacadb4) |
| W1  | #13 | Screenshot tests become state tests | none | done (7121aaf) |
| W2  | #14 | Logging module, quiet by default | none | done (a47c1ad) |
| W3  | #15 | No short-name limit | none | done (5b68289) |
| W4  | #16 | Memory module and leak checks | W2 | done (1e07f3d) |
| W5  | #17 | New UI: design, protocol v2, scaffold | none | done (7c26dbf) |
| W6  | #18 | New UI: the views | W5 | running (T4, T6; T2, T3, T10 done) |
| W7  | #19 | Core refactor for single responsibility | W4 | ready |
| W8  | #20 | Remove the X11 front end | W6 | blocked |
| W9  | #21 | WebAssembly build (proof of concept) | maintainer's OK to install emsdk | blocked |
| W10 | #22 | Replayable interruptions in scripts | none | done (539b300) |

## W0: C/C++ mixed build
**Goal.** core/*.cpp builds next to core/*.c on Linux, Windows (MinGW,
MSYS2) and macOS (clang).
**Scope.**
- Headers declare an `extern "C"` API.
- Programs link with the C++ compiler.
- The LTO check, x11free and warnings tools know about .cpp files.
- verify.sh prints `C++: N / M`.
- One small file is converted as the proof.
**Done when.**
- verify.sh passes and prints the C++ metric.
- The native Windows build passes servercheck.
- CLAUDE.md states the conversion rule.

## W1: Screenshot tests become state tests
**Goal.** Web tests assert what the UI holds, not how it looks.
**Scope.**
- tools/webshots.mjs becomes tools/webtest.mjs: the same step language,
  with every `shot` replaced by an assertion on state or the DOM.
- It checks the content of the files a session writes.
- CI runs it once, against the build it just made.
**Done when.**
- No pixel comparison is left anywhere.
- A step broken on purpose fails with a clear message.
- The CI web step is faster.

## W2: Logging module, quiet by default (done)
- `core/xpp_log.[ch]` has levels ERROR/WARN/INFO/DEBUG and follows printf.
- Messages go to `-logfile` when given, else stderr.
- `--verbose` and `--debug` raise the level.
- AUTO's table is INFO on the console and always shown in the browser's
  AUTO Output panel.

## W3: No short-name limit
**Goal.** Variable, parameter, auxiliary and function names are no longer
limited to about 9 characters.
**Scope.**
- One name-length constant replaces every fixed-size name array.
- The parser, the .set file (old files must still load), AUTO's headings,
  the protocol and the X11 widgets (truncated display is fine) all cope
  with long names.
**Done when.**
- A model with 20–40 character names integrates to the same output as
  the same model with short names.
- It round-trips through a .set file, runs AUTO, and its parameters can
  be set by name.
- verify.sh passes, including the LTO check.

## W4: Memory module and leak checks
**Goal.** Allocation that fails loudly and code that does not leak.
**Scope.**
- A `xpp_mem` module whose allocators check for failure and count
  allocations in debug builds.
- An AddressSanitizer and leak-check build that runs the checks.
- Modules are converted one at a time; per the W0 rule, a converted file
  becomes C++ and uses RAII.
**Done when.** The sanitizer build runs every check without a leak or an
error report, and is part of CI.

## W5: New UI design, protocol v2 and scaffold
**Goal.** A design for the modern UI, and one view built end to end.
**Scope.**
- docs/ui-v2.md covers:
  - the data events of protocol v2, in the order to build them;
  - the file dialogs;
  - components and state, built for single responsibility;
  - the responsive layout (from 360 px phones up);
  - accessibility (WCAG 2.2 AA, keyboard, touch);
  - fonts and themes;
  - the list of implementation tasks.
- web2/ holds TypeScript, Preact or Svelte, uPlot and esbuild, embedded
  in xppautX and served at `?ui=2`.
- The time-series plot is drawn from a new `series` event.
**Done when.**
- The v2 plot is driven by a state-level test, including a narrow
  viewport and keyboard use.
- The `series` event carries the numbers in output.dat.
- verify.sh passes.

## W6: New UI views
The implementation tasks and their acceptance criteria are in
docs/ui-v2.md, section 10 (T2 to T18, in dependency order), with the
layout rules R1-R7 and the accessibility rules A1-A14 each view must
meet. In short:
- The shell and theme.
- Plots: phase plane, nullclines, direction fields.
- The AUTO view. It also draws the segment from a Hopf point to the first
  point of its periodic branch, which XPP leaves blank.
- The data browser.
- Numerics and parameter panels.
- The dialogs.
- The animation and array plot.

## W7: Core refactor for single responsibility
- Split ui_json.c: protocol I/O, drawing ops, prompts, AUTO, browser.
- Split commands.c.
- Move globals into structs.
- Every file touched converts to C++.

## W8: Remove the X11 front end
Once the new UI covers X11's features, delete the X11 sources,
tools/guicheck.sh, the x11free and coredeps metrics, and the X11 build
paths.

## W9: WebAssembly build (proof of concept)
- The core compiled with Emscripten runs in a Web Worker, speaking the
  JSON protocol over postMessage.
- The cancel token becomes a SharedArrayBuffer flag, so Abort still works.
- Files live in an IndexedDB-backed virtual filesystem.
- Needs the maintainer's OK to install emsdk.

## W10: Replayable interruptions in scripts
**Goal.** A recorded session that interrupted an integration or an AUTO
run with Esc or Abort replays to the same point.
**Scope.**
- When a job is cancelled, the core reports how far it got: the
  integration's time and rows, or AUTO's branch and point.
- The browser's recorder writes `{"cmd":"abort","at":{...}}` instead of a
  bare Escape key, and drops keys that did nothing.
- `--script` cancels exactly when the recorded point is reached.
**Done when.** A recorded session with interrupted integrations replays to
the same output.dat, and one with an interrupted AUTO run replays to the
same diagram.
