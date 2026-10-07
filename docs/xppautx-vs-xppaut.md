# xppautX compared with XPPAUT 8.0

What is XPPAUT's and what is ours, area by area, so nobody has to review
the project from the beginning to know (W148, #200). It goes beyond the
bugs: those are in [xppaut-findings.md](xppaut-findings.md) (W143), and the
"Fixed" section below only links them. How xppautX works is the
[manual](manual/README.md), which this file links instead of repeating. A row compares with XPPAUT 8.0: what xppautX added and later took out again (an Undo, PNG export, the pixel protocol, `--script`) is the changelog's (CHANGELOG.md, W149), not this file's.

**Upkeep.** One row per thing, a card in the last column. A card that
changes what an XPPAUT user meets (a feature added, changed or removed, a
file format, a default) adds or edits its row in the same commit, with its
card number; a bug or limit XPPAUT had goes to xppaut-findings.md, and its
row here, if any, links the entry. A row whose card is decided but not
finished says so in its xppautX column ("decided, not done"). "Not
verified" marks what was not checked against the code when the row was
written (2026-10-01).

XPPAUT's behaviour is cited where it is code as a relative link into
`reference/xppaut-8.0/` (the 8.0 source xppautX was forked from; local,
git-ignored; `reference/xppaut-master/` is the 2016 GitHub master), e.g.
[xpplim.h:4](../reference/xppaut-8.0/xpplim.h#L4). A row with no link
describes XPPAUT from its manual (docs/upstream/) or is marked not
verified.

Contents: [Platforms and install](#platforms-and-install) -
[The interface](#the-interface) - [Files and formats](#files-and-formats) -
[Sessions and recordings](#sessions-and-recordings) -
[The model language](#the-model-language) -
[Numerics and solvers](#numerics-and-solvers) - [AUTO](#auto) -
[Batch, the protocol, --server](#batch-the-protocol---server) -
[Limits removed](#limits-removed) - [Removed features](#removed-features) -
[Fixed](#fixed)

## Platforms and install

| What | XPPAUT 8.0 | xppautX | Card |
|---|---|---|---|
| Display | needs an X server: opens the display at start ([main.c:1432](../reference/xppaut-8.0/main.c#L1432)) | no X server; the page in a window of its own, or in a browser | W8, W13a |
| Windows, macOS | through an X server or a port (its install notes: docs/installonwindows.html, docs/installonmac.html; not verified) | native: one program, built and checked in CI on Linux, Windows and macOS; the macOS window has not been run by hand ([README](../README.md#trying-the-macos-build)) | W13a, W13d, W17 |
| Release files | source tarball and distribution packages (not verified) | Windows `.exe`, Linux `.deb` and archive, macOS `.dmg`/archive; unsigned ([README](../README.md#installing-a-release)) | W89, W14 |
| Linux window | X11 libraries linked | the window (WebKitGTK) is a library loaded only when it opens, so one binary starts on any Linux and falls back to the browser | W13e |
| Opening models | the command line | also a double-click: `.ode`, `.odex`, `.snapx`, `.recx` registered per user or by the package | W13b, W59c, W91, W155 |
| Build | per-system Makefiles (Makefile, Makefile.64, ... in the source) | one Makefile, C++23, 0 warnings on gcc and clang, CI on three systems | W0, W17 |
| Source language | C | C++ throughout: containers and RAII, no raw allocator, no `exit()` in the numerics | W27, W29, W33, W48 |
| Web assembly build | no | a proof of concept was planned, blocked (needs emsdk) | W9 |
| Update check | no | planned, blocked on a first release | W13c |

## The interface

Row-by-row parity (what the X11 windows did and where it is now):
[front-end-gaps.md](front-end-gaps.md). The user's guide:
[manual chapter 4](manual/04-using-the-interface.md) and
[using-the-panel.md](using-the-panel.md).

| What | XPPAUT 8.0 | xppautX | Card |
|---|---|---|---|
| Startup run / animator controls | startup run and grab integration use direct calls | protocol jobs announce computing and accept Abort; recordings retain animator pause/speed during Go | W136 ([#188](https://github.com/MuhammadMoustafa/xppautX/issues/188)) |
| Windows | a separate X11 window each for the plot, data browser, AUTO, animation, array plot | one window with tabs; AUTO floats over it (a sheet on narrow screens) | W5, W6, W8 |
| Data table paging | the data browser reads its stored rows directly | the page fetches buffered blocks; the last block covers the viewport through the end of the data without repeatedly requesting nonexistent rows | W170 ([#222](https://github.com/MuhammadMoustafa/xppautX/issues/222)) |
| Menus and keys | pop-up menus, single-letter hotkeys ([menus.h](../reference/xppaut-8.0/menus.h)) | the same menus as data (one command table, `core/command_table.h`, with plain labels and categories); XPPAUT's one-letter keys and File/Numerics sequences are an optional preset "XPPAUT sequences" (off by default), the default keys come from the table (Ctrl+O, Ctrl+S, Ctrl+R) | W7, W6, W208 |
| Key bindings | fixed one-letter hotkeys; changing them means editing the source | the user's own `keymap.json` in the per-user config folder (key bindings, pinned commands, the XPPAUT-sequences preset), one protocol command shared by the window and the browser, loaded all or nothing; edited in the page's keyboard shortcuts editor (record a key, conflicts ask Replace or Cancel, the system's keys refused, presets, search by keys) | W211 ([#265](https://github.com/MuhammadMoustafa/xppautX/issues/265)), W212 ([#266](https://github.com/MuhammadMoustafa/xppautX/issues/266)) |
| Quick access | the user-defined buttons of `BUT=name:keys` lines in the model (up to 20, fixed in the file) | a toolbar of pinned commands: pin any command from the command list or the editor, drag to reorder, Unpin from a menu, a More menu for what does not fit; kept in `keymap.json`, not in the model | W212 ([#266](https://github.com/MuhammadMoustafa/xppautX/issues/266)) |
| Drawing | the core draws pixels into the X window | the core sends numbers (`series`, `plots`, `nullclines`, `dfield`, `marks`, `diagram`, ...), the page draws them; the pixel events are gone | W5, W6; protocol 2 |
| Values (ICs, parameters) | a box of fields with Ok/Cancel | a side panel, edits apply when a field is left; Reset one or all returns to the model's values | W6, W62 |
| Numerics | the Numerics menu only | also a Numerics section of the values panel; edits during a run apply to the next run, never the one in progress | W106 |
| Equation editor | Edit menu: right-hand sides, functions, Save as | removed; edit the model file and Reload | W54 |
| Open another model | by restarting the program | File > Open model and Reload in the same process; a failed load keeps the model before; Reload keeps the session's file, so Ctrl+S still saves there | W61, W218 |
| Leaving | closes the window | one question, "Save this session first?"; a run is never stopped by it | W59d, W110 |
| File dialogs | XPPAUT's own file selector | the OS's own dialog in the window, the browser's picker in browser mode | W88, W90 |
| Navigation | menus arranged around letter shortcuts | permanent initial/current/steady/continue/stop run toolbar; inline duration and extra/until continuation; full-core unchanged-digits hold with bounded duration and termination reason; consolidated Values and Files actions; searchable groups; stable clicked actions and existing shortcuts | W186–W189, W192–W194 |
| Experimental values | reset or manually re-enter values | Undo value edit / Redo value edit (Ctrl+Z, Ctrl+Shift+Z / Ctrl+Y): a bounded stack of parameter, initial-condition and numerics snapshots, alongside model-default Reset; XPPAUT has no undo | W190, W210 ([#264](https://github.com/MuhammadMoustafa/xppautX/issues/264)) |
| Inspecting states and parameters | separate legacy value windows | compact States/Parameters panel with ten-digit inspection and full-precision editing; all states refresh from solver doubles during integration independently of plotted columns; sampled tail rates cover all states independently of plotted columns and indicate recent motion without certifying convergence | W187, W190, W200, W202 |
| Busy | the program does not answer while it computes | Stop at any time; a command sent during a run is discarded at the source, except control, view and setting commands | W68, W95 |
| Errors | `err_msg` text in a box | an error dialog with OK for a failed action; every error names its file and line | W104, W140 |
| Fonts, colours, window size, `-bigfont`, `-white`, `-width` ... | X resources and options | the options are accepted and no longer stored | AGENTS.md "Architecture" |
| Update check | none | Help > Check for updates, on demand; release tag page opens by choice, no download or install; retry after closing a pending check, errors retain their source (W176, [#228](https://github.com/MuhammadMoustafa/xppautX/issues/228)) | W13c ([#126](https://github.com/MuhammadMoustafa/xppautX/issues/126)) |
| Help | the info/help files (`help/`) | the manual as Markdown in the Help view, linked from menus and dialogs | W12 |
| Picture export | PostScript, GIF, SVG | PostScript, SVG and the GIFs from one registry | W53, W66 |
| Kinescope | frames kept in the X window | frames captured as data in the page; the core writes the animated GIF | W6 (T-cards) |
| `.xpprc`, `XPPEDITOR` | read at start ([load_eqn.c:1074](../reference/xppaut-8.0/load_eqn.c#L1074)); File > Xpprc edits it ([menudrive.c:120](../reference/xppaut-8.0/menudrive.c#L120)) | read at start (`core/load_eqn.cpp`) and still editable (`XPPEDITOR`) | W139 |

## Files and formats

Principle (maintainer, 2026-09-29): no backward compatibility with XPPAUT
is sought; XPPAUT's files are read as imports, never written back. Our own
files load all or nothing ([protocol.md "Our files"](protocol.md)): a bad
value stops the load with the file, line and value, and nothing is applied.

| What | XPPAUT 8.0 | xppautX | Card |
|---|---|---|---|
| `.ode` | the model; read with its quirks | converted on open to a saved `.odex`, quirks explicit and numbers preserved; that file becomes the model | W73, W154 (#206) |
| `.odex` | none | a cleaner model language, same Model; `xppautX --convert` writes it from an `.ode` | W73, W74, W79, W80 |
| `--check` | none | in-memory validation, JSON quirk findings and load errors; opening `.ode` shows the same findings once | W75 ([#123](https://github.com/MuhammadMoustafa/xppautX/issues/123)) |
| `.snapx` | none | the session: a zip of ordinary files incl. the saved model and AUTO settings, diagram, views and orbits | W57, W103, W155 |
| `.recx` | none | a recording: one text file, steps and notes | W59 |
| `.auto` (AUTO diagram) | written by File > Save diagram | Import diagram reads into the open model (to 6 digits); never written | W92, W155 |
| `.set` | written and read by File > Write set / Read set ([menus.h:29](../reference/xppaut-8.0/menus.h#L29)), read with `atoi`/`atof`, names ignored ([finding 26](xppaut-findings.md#26-set-files-ignore-the-models-names)) | never written; File > Import XPPAUT set and `--setfile` check every named value and require XPPAUT's equations trailer, all or nothing; valid imports are saved beside the `.set` as `<name>.snapx`, now open; the format stays as the session member `model.set` | W125, W147, W153 |
| `.par`, `.ic` | written and read | kept, XPPAUT's format, read all or nothing | W125, W147 |
| The options file (`option file`, `default.opt`) | never applied ([findings #4](xppaut-findings.md)) | an `option` line is refused at load; write `@` lines in the model or an included file ([manual 14](manual/14-options-file.md)) | W139 |
| `#include` of a missing file | skipped ([findings #5](xppaut-findings.md)) | an error; model reads refuse absolute include names, traversal and linked descendants outside or within the model folder | W139, W175 ([#227](https://github.com/MuhammadMoustafa/xppautX/issues/227)) |
| Data output | `output.dat`, the Data browser's Write | `.dat` unchanged, CSV with a header row, CSV.gz and NPZ from one registry; the Save data dialog lists them | W52, W26 |
| Delete added data columns | unconditional placeholder ([finding 37](xppaut-findings.md#37-delete-column-is-an-unconditional-placeholder)) | removes added columns safely; rejects time/model columns and columns used by plots; remaining formulas recompute | W197 |
| Plot retention and legends | native menus and separate frozen-curve commands | Freeze controls next-run retention; Continue appends; Fit preserves older runs; individual legend visibility and hover feedback; double click edits the local legend name/colour | W199 |
| Fit with retained runs | Fit rescales to the live data only | 2D Fit includes visible retained earlier runs, as 3D Fit does (W204, #258) |
| Continue until | an end time off the Dt grid stops where the integrator's step count falls | `until` is rounded up to the next Dt grid point, never short; the page shows "will end at t=" first, the `state` event's `time` is the end reached (W204, #258) |
| Continue | (C)ontinue asks "Continue until:" (a time, default Total) and stores at its own stride | one `continue` command (Alt+Enter, the button, the key C): a duration or an end time from the toolbar, no prompt, honours Store every N steps and rounds the end up to that output grid (Dt times N); the legacy prompt is gone (W213, #267) |
| Run from last state | Initial conditions > (L)ast, with its prompt | Run from last state: one toolbar button that starts a new run from the last state (or Ctrl+Shift+Enter); the overwritten initial conditions go on the undo stack (W192, #246; W210, #264; W212) |
| Axis editing and dismissal | Viewaxes menu and explicit Cancel | 2D axis labels and 3D controls open the existing variables/limits editor; AUTO axis popovers and dialogs dismiss with × or outside click | W196, W198 |
| Saving what the plot shows | not verified | curves, frozen and earlier ones as one table (`curve,x,y[,z]`) | W52 |
| Who writes files | the X client and the core | the core only; the page just downloads | W66 |
| File transfer safety | no page file API | base names only; Windows reads and exclusive temporary-file creation check the opened handle and refuse reparse points, folders and devices | W174 ([#226](https://github.com/MuhammadMoustafa/xppautX/issues/226)) |
| Output names | silently writes `anim.gif`, `UMN.dat`/`SMN.dat` and periodic-orbit files; filenames in 25-byte forms (24 characters plus terminator) | every export or series starts with a file ask; Cancel writes nothing; names are kept whole; one model-base rule supplies core and page defaults; sequences derive later names | W130, [#182](https://github.com/MuhammadMoustafa/xppautX/issues/182); W177, [#229](https://github.com/MuhammadMoustafa/xppautX/issues/229) |
| Save permission and failure | varied by command | native dialogs confirm existing destinations once; without a dialog decision, core asks only if its target exists; recordings keep native answers and scripts carry decisions; atomic commit reports once, only a successful save is delivered, and empty exports report before the name ask; native authorization binds the actual path to its prompt, derived files confirm separately, browser exports retain distinct bytes and desktop buttons use the picker | W129, #181; W177, [#229](https://github.com/MuhammadMoustafa/xppautX/issues/229) |
| Array print render | malformed text selects blue-red and out-of-range values silently select grey scale ([finding 35](xppaut-findings.md#35-array-print-invalid-render-text-selects-blue-red)) | strict whole integer and enum bounds; an invalid value reports before a file ask | W177, [#229](https://github.com/MuhammadMoustafa/xppautX/issues/229) |
| `.ode` written back | Edit > Save as writes one | never: Copy as set line shows `set name {...}` for the user to paste | W54, W67 |
| Compiled-function libraries (`export`, `.so`/DLL) | supported ([extra.c:110](../reference/xppaut-8.0/extra.c#L110)) | removed; a model using them fails to load | W55 |
| Where files go | AUTO's to `$HOME` ([findings #8](xppaut-findings.md)) | per-process private scratch folders, stale ones cleaned at start only when their owner is proved exited (Windows too, W121b); outputs go beside the model (not verified for every file) | W19 |
| Other formats (`.ani`, tables, `.dat` for tables) | read; table counts have no upper bound ([finding 33](xppaut-findings.md#33-model-table-counts-have-no-upper-bound)) | read; tables and `.ani` are saved with a session; file and formula tables share a 1,000,000-point allocation/evaluation budget | W103, W175 ([#227](https://github.com/MuhammadMoustafa/xppautX/issues/227)) |
| Text encoding | not verified | UTF-8 in and out | W35b |

## Sessions and recordings

| What | XPPAUT 8.0 | xppautX | Card |
|---|---|---|---|
| Continue where you stopped | not available; only `.set`/`.par`/`.ic` values | File > Save session (Ctrl+S: the file it remembers, no dialog after the first) / Save session as (Ctrl+Shift+S) / Save a copy of the session (no key; the session's file stays) / Open, with an unsaved-changes dot in the title: values, every window, labels, frozen curves, current and frozen nullclines, sliders, AUTO diagram, data, random state ([protocol.md "Session files"](protocol.md#session-files)) | W57, W209, W218 |
| A changed model | not applicable | the session carries its own model; opening it loads that version | W103 |
| Record and replay | none | File > Record writes a `.recx` of every step; opened recordings start automatically; the player steps, pauses, shows captions and notes, and keeps controls above the plot/AUTO; reconnects restore its step list | W59a-d, W201 |
| Record from a snapshot | none | a recording starts from the session's state | W59d |
| Portable replay state | none | MT19937-64 state has one explicit layout on every compiler; scratch outputs allow linked temp-folder ancestors while refusing links within scratch | W169 ([#221](https://github.com/MuhammadMoustafa/xppautX/issues/221)) |
| Replay a recording from the command line | none | `xppautX run.recx --silent` loads its snapshot and plays without an interface; exit 0 on clean playback, 1 on failure, recorded aborts exact; keeps plain-name outputs in the launch folder, asks before replacing (60 s; EOF/timeout refuses), `--auto` confirms | W144 ([#196](https://github.com/MuhammadMoustafa/xppautX/issues/196)) |
| Slider settings | model options set three slider bindings | Session definitions, including added sliders and step sizes, saved in `.snapx`; Open model replaces them with its presets | W135, [#187](https://github.com/MuhammadMoustafa/xppautX/issues/187) |
| Seeds | one global generator seed | each run has its seed, logged and saved with its data; set it and Go repeats a run exactly | W71 |

## The model language

Trailing `#` text in `.ode` lines is now read as a comment, unlike XPPAUT ([finding 28](xppaut-findings.md#28-a--comment-after-a-declaration-makes-names-of-its-words)); `#include`, `#done` and Volterra convolution separators retain their meanings (W160, #212).

| What | XPPAUT 8.0 | xppautX | Card |
|---|---|---|---|
| `.ode` quirks (comparisons bind tighter than `+`, `^` groups left, no sign after an operator, ...) | the parser's own | kept bit for bit; the list, measured, is [odex-quirks.md](odex-quirks.md) | W73 |
| `.odex` | none | usual precedence, `^` right-associative, unary minus anywhere, case-sensitive names, `and`/`or`, arrays by `for j in a..b`, `const`, `near(a,b)` ([odex.md](odex.md)) | W73, W78, W80 |
| Two syntaxes | one | the loader reads `.odex` only; the converter alone reads `.ode`, through the same Model builder; converted examples preserve their md5 | W79 |
| `--check` (quirks as warnings) | none | decided; blocked | W75 |
| Name length | variables 11, parameters 10 characters ([findings #3](xppaut-findings.md)) | no limit, including protocol names and values (W121b removes remaining field cuts) | W76, W121b ([#172](https://github.com/MuhammadMoustafa/xppautX/issues/172)) |
| Model line length and count | fixed buffers ([findings #19](xppaut-findings.md)) | none | W29e |
| Option names | some unreachable, some never applied ([findings #1, #2](xppaut-findings.md)) | one table of options (`core/model_options.cpp`); a bad `@` value stops the load, naming the file, the line and the value (W125, W140) | W119, W125 |
| `@` settings from `.xpprc`, command line, model | merged by a set of flags | one option table | W119 |
| `#include` | `#include file`, ends with `#done` | same for `.ode`; `.odex` uses `include "file"` | W139 |
| Division by zero | zero divisor replaced by 2.23e-15 | `.odex` uses IEEE; conversion preserves XPPAUT through a function evaluating uncertain divisors once, omitting guards for proven nonzero divisors ([odex.md](odex.md)) | W73, W165 ([#217](https://github.com/MuhammadMoustafa/xppautX/issues/217)) |
| `special` (conv, sparse, fftcon, ...) | supported | supported; `fftcon` weight table fixed ([findings #23](xppaut-findings.md)) | W38 |
| Compiled functions in the model | `export` and DLL functions | removed | W55 |

## Numerics and solvers

Numbers do not change unless a card says so: `tests/examples.md5` guards
every example, the goldens guard the output files.

| What | XPPAUT 8.0 | xppautX | Card |
|---|---|---|---|
| Store every N steps | `NOUT`/`NJMP` ("nOutput" in the menu) | `store_every`, "Store every N steps" (one row per N output steps; 1 stores every step); a `.odex` and our files take only the new name, a `.ode` keeps NOUT/NJMP and converts to it | W206 ([#260](https://github.com/MuhammadMoustafa/xppautX/issues/260)) |
| Names of the main numerics | `TOTAL`, `T0`, `TRANS`, `NMESH`, `NEWT_ITER`, `NEWT_TOL`, `JAC_EPS`, `METH`, `TOL`, `DTMIN`, `DTMAX`, `ATOL`, `BVP_MAXIT`/`TOL`/`EPS`, `MAXSTOR`, `TOR_PER`, `POIMAP`, `POIVAR`, `POISGN`, `POISTOP`, `POIPLN` | `total_time`, `start_time`, `transient_time`, `nullcline_mesh`, `singpt_max_iterates`, `singpt_newton_tolerance`, `singpt_jacobian_epsilon`, `method`, `tolerance`, `min_step`, `max_step`, `abs_tolerance`, `bvp_max_iterates`, `bvp_tolerance`, `bvp_epsilon`, `storage_rows`, `torus_period`, `poincare_map`, `poincare_variable`, `poincare_sign`, `poincare_stop`, `poincare_plane`; a `.odex` and our files take only the new names, a `.ode` keeps XPPAUT's and converts to them | W216 ([#270](https://github.com/MuhammadMoustafa/xppautX/issues/270)) |
| Curve fit and plot diagram import | whitespace data only | `.dat` and `.csv` through the Save data registry; CSV headers skipped | W137 ([#189](https://github.com/MuhammadMoustafa/xppautX/issues/189)) |
| Array range movies | GIF stream written during the sweep | temporary file committed after a complete sweep; Stop preserves the old file | W137 |
| Kinescope capacity | rotation and BVP movies silently drop frames when full ([findings #27](xppaut-findings.md#27-rotation-and-boundary-value-movies-silently-drop-frames-when-full)) | every capture returns a result; the command reports a full buffer once | W133 |
| Results and notices | fit outcomes, statistics and toggle states use the error-message dialog | the existing status bar info route; successful results do not count as errors | W133 |
| Integrators (Euler, RK4, Dormand-Prince, Gear, CVODE, Rosen, Stiff, Volterra, symplectic, discrete, ...) | a switch on a method number | the same methods, one `xpp::Solver` per method in a registry; results unchanged | W51 |
| Choosing an integrator | `@ meth` reads its first character, ignores an unknown key and bypasses the menu's suitability checks; menu refusal switches to Adams | one picker validates names, legacy keys and model suitability in options, menu, values panel and set import; errors keep the old method or fail the load at its line | W132, [#184](https://github.com/MuhammadMoustafa/xppautX/issues/184) |
| CVODE | vendored | vendored; whether to move to SUNDIALS is an evaluation, later | W34 |
| Fourier transform | `fftn` ([histogram.c:940](../reference/xppaut-8.0/histogram.c#L940)) | pocketfft | W32a |
| `sin`, `cos`, `exp`, `log`, `pow`, `erf`, `lgamma`, ... and `a*b+c` | the C library's, which round differently by CPU and system, and the compiler's FMA contraction: the numbers depend on the machine ([findings #29](xppaut-findings.md#29-the-numbers-depend-on-the-cpu-and-on-the-compilers-fma)) | correctly rounded (CORE-MATH, `xpp::math`) and no contraction: the same bits on every CPU and system, including Bessel `besselj`/`bessely` (vendored musl over `xpp::math`); the example md5s rebaselined once | W159, W163 |
| Random numbers | Numerical Recipes `ran1` ([markov.c:736](../reference/xppaut-8.0/markov.c#L736)) | `std::mt19937_64` and our own distributions; stochastic models' md5s rebaselined once | W32a, W71 |
| Linear solves | `sgefa`/`sgesl`, `ge`, band solves | one LU solve; eigenvalues for Gear from EISPACK | W32a |
| Model options such as `newt_iter`, `jac_eps` | silently ignored ([findings #1](xppaut-findings.md)) | take effect, so some results differ from XPPAUT's (the DAE examples, W126) | W119, W126 |
| DAE past a fold | steps over it onto another branch ([findings #6](xppaut-findings.md)) | stops at the fold | W127 |
| Store every N steps set to 0 | divides by zero ([findings #25](xppaut-findings.md)) | refused by the owner's rule | W145 |
| Bit-identical results on every platform | not a goal | studied: `-ffp-contract=off` and a correctly rounded libm; adoption deferred ([studies](studies/w72-bit-identical.md)) | W72 |
| Speed | C | measured after each C++ stage and not slower beyond noise | W109 |

## AUTO

AUTO's own numerics are the AUTO of XPPAUT 8.0, converted to C++ with
identical diagrams. The window: [manual 9](manual/09-auto.md).

| What | XPPAUT 8.0 | xppautX | Card |
|---|---|---|---|
| Window | an X11 window with its own menus | the same menus and keys (as data), a view over the page | W96, W6 |
| Diagram views | one diagram, one set of axes | any number of views of one diagram, each with its axes | W50 |
| Saving | `.auto` text file at 6 digits | `.snapx` session: every digit, settings, orbits, views | W92, W155 |
| Files during a run | `fort.*` in `$HOME`; sessions overwrite each other ([findings #8](xppaut-findings.md)) | per-process scratch folders, stale ones cleaned at start only when their owner is proved exited | W19, W121b ([#172](https://github.com/MuhammadMoustafa/xppautX/issues/172)) |
| Eigenvalues shown | a run's first point shows the previous run's ([findings #14](xppaut-findings.md)) | one source of a point's eigenvalues and multipliers | W15 |
| Grab a label | by keys in the window | also `auto grab <label>` for scripts and recordings | W64 |
| Errors | numerics call `exit()` ([findings #17](xppaut-findings.md)) | returned as errors; the program stays | W63a |
| Settings | an X11 form | settings are data (`autosettings`); checked by AUTO's own rules | W92, W118 |
| Orbit loading | trusts solution dimensions and existing storage ([finding 27](xppaut-findings.md#27-auto-orbit-loading-trusts-file-dimensions-and-storage-capacity)) | shared bounded restart reader; grows the data table before copying an orbit | W155 |
| Exports | a table | `Write pts` and All info as CSV with names and LF line ends (including Windows, W137); `.auto` never written | W26, W92 |

## Batch, the protocol, --server

| What | XPPAUT 8.0 | xppautX | Card |
|---|---|---|---|
| Batch (`--silent`) | XPPAUT `-silent` integrates, writes `output.dat` ([main.c:395](../reference/xppaut-8.0/main.c#L395)), then `silent_*` for nullclines, direction fields, equilibria ([main.c:508](../reference/xppaut-8.0/main.c#L508)) | the same files and flags, with `--silent` required (old spelling stops); a built-in script of protocol commands, no interface; exits 1 if an output cannot be written | W56, W133, W156 (#208) |
| Exit code of a model that does not load | not verified | non-zero in every mode | W35c |
| JSON protocol | none | line-delimited JSON on stdin/stdout (`--server`) and over HTTP; [protocol.md](protocol.md) | W5, W7a |
| Other programs drive it | no | the VS Code extension and `tools/*check.py` use the protocol | W5 |
| Local server | none | 127.0.0.1 only, random token in the address; `--no-open`, `--port`; 256 request threads and 32 concurrent uploads, excess requests receive 503 before their bodies are read; whole heads limited to 5 s, body receives to 30 s, blocked sends to 10 s ([protocol](protocol.md#files)) | W5, W161 (#213), W164 (#216) |
| Command line options | XPPAUT's list ([comline.c](../reference/xppaut-8.0/comline.c)) | word options require two dashes; old single-dash words stop with the new spelling; X11 options and unused `-def` are errors; new: `--browser`, `--server`, `--convert`, `--verbose`, `--debug`, `--logfile` | W13a, W156 ([#208](https://github.com/MuhammadMoustafa/xppautX/issues/208)) |
| Logging | `plintf` to stdout | one checked `xpp::log` call (AUTO uses `xpp::log_auto`), including dynamic widths and precisions; quiet by default, stdout carries only the protocol; printed text unchanged | W2, W25, W172 ([#224](https://github.com/MuhammadMoustafa/xppautX/issues/224)) |

## Limits removed

| What | XPPAUT 8.0 | xppautX | Card |
|---|---|---|---|
| Name length | 10-11 characters; 25 in dialogs | none | W76 |
| Model line, file name, comment lengths | fixed buffers | none | W29e |
| Number of variables | `MAXODE` 5000 ([xpplim.h:4](../reference/xppaut-8.0/xpplim.h#L4)) | still 5000: the Model's tables are fixed-size (`core/xpplim.h`); not removed | - |
| Parameters, flags, functions, tables, kernels, networks | 400, 2000, 50, 50, 50, 50 ([xpplim.h:8](../reference/xppaut-8.0/xpplim.h#L8)) | the same fixed limits, still enforced (`core/xpplim.h`: fixed arrays in model.h and session.h, checked in expr_symbols.cpp, flags.cpp, form_ode.cpp, simplenet.cpp); not removed yet | - |
| Stored lines of the model | 5000, overflowing by one ([findings #19](xppaut-findings.md)) | none | W29e |
| Dialog field width | 25 characters ([graf_par.h:14](../reference/xppaut-8.0/graf_par.h#L14)) | none | W32c, W76 |
| Fixed-width displays | names cut | `~` marks a shortened name in AUTO's printed columns only | W76 |
| Memory | `malloc` that could fail silently | std containers; a failed allocation exits loudly | W4, W48 |

## Removed features

| What | XPPAUT 8.0 | xppautX | Card |
|---|---|---|---|
| X11 front end | the only interface | removed; the page replaces it | W8 |
| In-program equation editor | Edit menu | removed: the model file is the source | W54 |
| Compiled functions | `export`, Load DLL | removed (risks outweigh benefits); examples rewritten | W55 |
| The options file | `default.opt`, `option` line | removed ([findings #4](xppaut-findings.md)) | W139 |
| `.set` as a user file | File > Write set | removed; Import XPPAUT set and `--setfile` check XPPAUT's names and immediately convert valid imports to a `.snapx` session beside the `.set`; the format stays as `model.set` in a `.snapx` | W147, W153 |
| `.auto` writing | File > Save diagram | removed; Save diagram saves `.snapx` | W92, W155 |
| Bell, Tips | menu items | removed | W7e |
| Font, colour and window-size options | X resources | accepted, ignored | W8 |

## Fixed

Bugs and limits of XPPAUT that xppautX fixes, with evidence and the lines
in XPPAUT's own source, are in [xppaut-findings.md](xppaut-findings.md),
one section each (the index there is by area): model options (#1, #2, #4),
names and limits (#3, #19, #20), DAEs (#6, #25), AUTO (#7-#18),
analysis and networks (#22, #23), export (#24), files (#21).
