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
| Opening models | the command line | also a double-click: `.ode`, `.odex`, `.snapx`, `.autox`, `.recx` registered per user or by the package | W13b, W59c, W91 |
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
| Windows | a separate X11 window each for the plot, data browser, AUTO, animation, array plot | one window with tabs; AUTO floats over it (a sheet on narrow screens) | W5, W6, W8 |
| Menus and keys | pop-up menus, single-letter hotkeys ([menus.h](../reference/xppaut-8.0/menus.h)) | the same menus as data (`core/menus.cpp`), every hotkey kept | W7, W6 |
| Drawing | the core draws pixels into the X window | the core sends numbers (`series`, `plots`, `nullclines`, `dfield`, `marks`, `diagram`, ...), the page draws them; the pixel events are gone | W5, W6; protocol 2 |
| Values (ICs, parameters) | a box of fields with Ok/Cancel | a side panel, edits apply when a field is left; Reset one or all returns to the model's values | W6, W62 |
| Numerics | the Numerics menu only | also a Numerics section of the values panel; edits during a run apply to the next run, never the one in progress | W106 |
| Equation editor | Edit menu: right-hand sides, functions, Save as | removed; edit the model file and Reload | W54 |
| Open another model | by restarting the program | File > Open model and Reload in the same process; a failed load keeps the model before | W61 |
| Leaving | closes the window | one question, "Save this session first?"; a run is never stopped by it | W59d, W110 |
| File dialogs | XPPAUT's own file selector | the OS's own dialog in the window, the browser's picker in browser mode | W88, W90 |
| Busy | the program does not answer while it computes | Stop at any time; a command sent during a run is discarded at the source, except control, view and setting commands | W68, W95 |
| Errors | `err_msg` text in a box | an error dialog with OK for a failed action; every error names its file and line | W104, W140 |
| Fonts, colours, window size, `-bigfont`, `-white`, `-width` ... | X resources and options | the options are accepted and no longer stored | CLAUDE.md "Architecture" |
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
| `.ode` | the model; read with its quirks | kept, same quirks (docs/odex-quirks.md); never deprecated | W73 |
| `.odex` | none | a cleaner model language, same Model; `xppautX --convert` writes it from an `.ode` | W73, W74, W79, W80 |
| `.snapx` | none | the session: a zip of ordinary files incl. the saved model | W57, W103 |
| `.autox` | none | AUTO's diagram, settings and views with the saved model | W92, W103, W50 |
| `.recx` | none | a recording: one text file, steps and notes | W59 |
| `.auto` (AUTO diagram) | written by File > Save diagram | read as an import into the open model (to 6 digits); never written | W92 |
| `.set` | written and read by File > Write set / Read set ([menus.h:29](../reference/xppaut-8.0/menus.h#L29)), read with `atoi`/`atof`, names ignored ([finding 26](xppaut-findings.md#26-set-files-ignore-the-models-names)) | never written; File > Import XPPAUT set and `-setfile` check every named value and require XPPAUT's equations trailer, all or nothing; valid imports are saved beside the `.set` as `<name>.snapx`, now open; the format stays as the session member `model.set` | W125, W147, W153 |
| `.par`, `.ic` | written and read | kept, XPPAUT's format, read all or nothing | W125, W147 |
| The options file (`option file`, `default.opt`) | never applied ([findings #4](xppaut-findings.md)) | an `option` line is refused at load; write `@` lines in the model or an included file ([manual 14](manual/14-options-file.md)) | W139 |
| `#include` of a missing file | skipped ([findings #5](xppaut-findings.md)) | an error | W139 |
| Data output | `output.dat`, the Data browser's Write | `.dat` unchanged, CSV with a header row, CSV.gz and NPZ from one registry; the Save data dialog lists them | W52, W26 |
| Saving what the plot shows | not verified | curves, frozen and earlier ones as one table (`curve,x,y[,z]`) | W52 |
| Who writes files | the X client and the core | the core only; the page just downloads | W66 |
| `.ode` written back | Edit > Save as writes one | never: Copy as set line shows `set name {...}` for the user to paste | W54, W67 |
| Compiled-function libraries (`export`, `.so`/DLL) | supported ([extra.c:110](../reference/xppaut-8.0/extra.c#L110)) | removed; a model using them fails to load | W55 |
| Where files go | AUTO's to `$HOME` ([findings #8](xppaut-findings.md)) | per-process private scratch folders, stale ones cleaned at start; outputs go beside the model (not verified for every file) | W19 |
| Other formats (`.ani`, tables, `.dat` for tables) | read | read; tables and `.ani` are saved with a session | W103 |
| Text encoding | not verified | UTF-8 in and out | W35b |

## Sessions and recordings

| What | XPPAUT 8.0 | xppautX | Card |
|---|---|---|---|
| Continue where you stopped | not available; only `.set`/`.par`/`.ic` values | File > Save session / Open: values, every window, labels, frozen curves, AUTO diagram, data, random state ([protocol.md "Session files"](protocol.md#session-files)) | W57 |
| A changed model | not applicable | the session carries its own model; opening it loads that version | W103 |
| Record and replay | none | File > Record writes a `.recx` of every step; the player steps, pauses, shows captions and notes | W59a-d |
| Record from a snapshot | none | a recording starts from the session's state | W59d |
| Replay a script from the command line | none | `--script FILE` today; decided: removed, `xppautX run.recx -silent` plays a recording and the checks move to `--server` or a `.recx` | W10, W144 (ready) |
| Slider settings | page only, not saved | page only, not saved in a session today; decided, not done | W135 (blocked) |
| Seeds | one global generator seed | each run has its seed, logged and saved with its data; set it and Go repeats a run exactly | W71 |

## The model language

| What | XPPAUT 8.0 | xppautX | Card |
|---|---|---|---|
| `.ode` quirks (comparisons bind tighter than `+`, `^` groups left, no sign after an operator, ...) | the parser's own | kept bit for bit; the list, measured, is [odex-quirks.md](odex-quirks.md) | W73 |
| `.odex` | none | usual precedence, `^` right-associative, unary minus anywhere, case-sensitive names, `and`/`or`, arrays by `for j in a..b`, `const`, `near(a,b)` ([odex.md](odex.md)) | W73, W78, W80 |
| Two syntaxes | one | two readers, one Model builder; every example's `.odex` gives the same md5 as its `.ode` | W79 |
| `--check` (quirks as warnings) | none | decided; blocked | W75 |
| Name length | variables 11, parameters 10 characters ([findings #3](xppaut-findings.md)) | no limit | W76 |
| Model line length and count | fixed buffers ([findings #19](xppaut-findings.md)) | none | W29e |
| Option names | some unreachable, some never applied ([findings #1, #2](xppaut-findings.md)) | one table of options (`core/model_options.cpp`); a bad `@` value stops the load, naming the file, the line and the value (W125, W140) | W119, W125 |
| `@` settings from `.xpprc`, command line, model | merged by a set of flags | one option table | W119 |
| `#include` | `#include file`, ends with `#done` | same for `.ode`; `.odex` uses `include "file"` | W139 |
| Division by zero | a large finite number, not IEEE (not verified) | see [odex.md](odex.md) (old models give XPP's numbers); not verified here | W73 |
| `special` (conv, sparse, fftcon, ...) | supported | supported; `fftcon` weight table fixed ([findings #23](xppaut-findings.md)) | W38 |
| Compiled functions in the model | `export` and DLL functions | removed | W55 |

## Numerics and solvers

Numbers do not change unless a card says so: `tests/examples.md5` guards
every example, the goldens guard the output files.

| What | XPPAUT 8.0 | xppautX | Card |
|---|---|---|---|
| Kinescope capacity | rotation and BVP movies silently drop frames when full ([findings #26](xppaut-findings.md)) | every capture returns a result; the command reports a full buffer once | W133 |
| Results and notices | fit outcomes, statistics and toggle states use the error-message dialog | the existing status bar info route; successful results do not count as errors | W133 |
| Integrators (Euler, RK4, Dormand-Prince, Gear, CVODE, Rosen, Stiff, Volterra, symplectic, discrete, ...) | a switch on a method number | the same methods, one `xpp::Solver` per method in a registry; results unchanged | W51 |
| CVODE | vendored | vendored; whether to move to SUNDIALS is an evaluation, later | W34 |
| Fourier transform | `fftn` ([histogram.c:940](../reference/xppaut-8.0/histogram.c#L940)) | pocketfft | W32a |
| Random numbers | Numerical Recipes `ran1` ([markov.c:736](../reference/xppaut-8.0/markov.c#L736)) | `std::mt19937_64` and our own distributions; stochastic models' md5s rebaselined once | W32a, W71 |
| Linear solves | `sgefa`/`sgesl`, `ge`, band solves | one LU solve; eigenvalues for Gear from EISPACK | W32a |
| Model options such as `newt_iter`, `jac_eps` | silently ignored ([findings #1](xppaut-findings.md)) | take effect, so some results differ from XPPAUT's (the DAE examples, W126) | W119, W126 |
| DAE past a fold | steps over it onto another branch ([findings #6](xppaut-findings.md)) | stops at the fold | W127 |
| Output stride 0 | divides by zero ([findings #25](xppaut-findings.md)) | refused by the owner's rule | W145 |
| Bit-identical results on every platform | not a goal | studied: `-ffp-contract=off` and a correctly rounded libm; adoption deferred ([studies](studies/w72-bit-identical.md)) | W72 |
| Speed | C | measured after each C++ stage and not slower beyond noise | W109 |

## AUTO

AUTO's own numerics are the AUTO of XPPAUT 8.0, converted to C++ with
identical diagrams. The window: [manual 9](manual/09-auto.md).

| What | XPPAUT 8.0 | xppautX | Card |
|---|---|---|---|
| Window | an X11 window with its own menus | the same menus and keys (as data), a view over the page | W96, W6 |
| Diagram views | one diagram, one set of axes | any number of views of one diagram, each with its axes | W50 |
| Saving | `.auto` text file at 6 digits | `.autox`: every digit, settings, orbits, views | W92 |
| Files during a run | `fort.*` in `$HOME`; sessions overwrite each other ([findings #8](xppaut-findings.md)) | per-process scratch folders, stale ones cleaned at start | W19 |
| Eigenvalues shown | a run's first point shows the previous run's ([findings #14](xppaut-findings.md)) | one source of a point's eigenvalues and multipliers | W15 |
| Grab a label | by keys in the window | also `auto grab <label>` for scripts and recordings | W64 |
| Errors | numerics call `exit()` ([findings #17](xppaut-findings.md)) | returned as errors; the program stays | W63a |
| Settings | an X11 form | settings are data (`autosettings`); checked by AUTO's own rules | W92, W118 |
| Exports | a table | `Write pts` and All info as CSV with names; `.auto` never written | W26, W92 |

## Batch, the protocol, --server

| What | XPPAUT 8.0 | xppautX | Card |
|---|---|---|---|
| `-silent` | integrates, writes `output.dat` ([main.c:395](../reference/xppaut-8.0/main.c#L395)), then `silent_*` for nullclines, direction fields, equilibria ([main.c:508](../reference/xppaut-8.0/main.c#L508)) | the same files and flags; a built-in script of protocol commands, no interface; exits 1 if an output cannot be written | W56, W133 |
| Exit code of a model that does not load | not verified | non-zero in every mode | W35c |
| JSON protocol | none | line-delimited JSON on stdin/stdout (`--server`) and over HTTP; [protocol.md](protocol.md) | W5, W7a |
| Other programs drive it | no | the VS Code extension and `tools/*check.py` use the protocol | W5 |
| Scripts | none | `--script` (W10), decided to go in favour of `.recx` | W144 (ready) |
| Local server | none | 127.0.0.1 only, random token in the address; `--no-open`, `--port` | W5 |
| Command line options | XPPAUT's list ([comline.c](../reference/xppaut-8.0/comline.c)) | the same names are accepted; X11 ones accepted and ignored; new: `--browser`, `--server`, `--convert`, `--verbose`, `--debug`, `-logfile` | W13a |
| Logging | `plintf` to stdout | `xpp::log`, quiet by default; stdout carries only the protocol | W2, W25 |

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
| `.set` as a user file | File > Write set | removed; Import XPPAUT set and `-setfile` check XPPAUT's names and immediately convert valid imports to a `.snapx` session beside the `.set`; the format stays as `model.set` in a `.snapx` | W147, W153 |
| `.auto` writing | File > Save diagram | removed; `.autox` | W92 |
| Bell, Tips | menu items | removed | W7e |
| Font, colour and window-size options | X resources | accepted, ignored | W8 |

## Fixed

Bugs and limits of XPPAUT that xppautX fixes, with evidence and the lines
in XPPAUT's own source, are in [xppaut-findings.md](xppaut-findings.md),
one section each (the index there is by area): model options (#1, #2, #4),
names and limits (#3, #19, #20), DAEs (#6, #25), AUTO (#7-#18),
analysis and networks (#22, #23), export (#24), files (#21).
