# Changelog

What is new, what changed and what was fixed in xppautX, newest first. The
form is [Keep a Changelog](https://keepachangelog.com/en/1.1.0/); each entry
ends with its card and issue (docs/roadmap.md, GitHub). A card that changes
what a user meets adds its line under Unreleased. How xppautX differs from
XPPAUT 8.0, area by area, is [docs/xppautx-vs-xppaut.md](docs/xppautx-vs-xppaut.md);
the bugs found in XPPAUT itself are in [docs/xppaut-findings.md](docs/xppaut-findings.md).

## [Unreleased]

### Changed

- Undo value edit (Ctrl+Z) and Redo value edit (Ctrl+Shift+Z, Ctrl+Y): a bounded stack (100) of the parameters, initial conditions and numerics before each edit, replacing the page's Keep/Restore working values checkpoint. Run from last state (the toolbar's former "Run from current") saves the initial conditions it overwrites, so Undo brings them back; runs, plots and files are not undone. In a text field Ctrl+Z stays the field's (W210, [#264](https://github.com/MuhammadMoustafa/xppautX/issues/264)).
- Reload model keeps the session file the session was saved to or opened from, so Ctrl+S after Reload saves to it with no question; the data and diagram Reload drops show as an unsaved change. New command Save a copy of the session… (File menu key B, no default key, pinnable): asks for a path, writes the `.snapx` there and leaves the session's file and its unsaved state as they were (W218, #272).
- Save session (Ctrl+S) saves to the session file the session was opened from or last saved to, with no dialog; the first save asks for a name. Save session as (Ctrl+Shift+S) always asks, and before replacing a file. The header shows a dot while the values (parameters, initial conditions, the time span, method and tolerances, the run's data) differ from the saved or loaded ones (`state.changed`); Quit's "Save this session first?" saves to the remembered file without asking again. The desktop File menus show both commands with their keys (W209, [#263](https://github.com/MuhammadMoustafa/xppautX/issues/263)).
- One keyboard dispatcher from a key to a command id, driven by the command table's default keys (Reload model gets Ctrl+R); chords show a pending-key hint and cancel with Esc or a focus change; Alt+F4, Ctrl+W, Ctrl+Q, F11, F12 and F5 are never bound or prevented. The XPPAUT one-letter keys (F then S) are now an optional preset "XPPAUT sequences", off by default, in Help > Keyboard shortcuts; the sticky File/Numerics shortcut mode and the Caps Lock status are gone (W208, [#262](https://github.com/MuhammadMoustafa/xppautX/issues/262)).
- The user's keymap is a file: `keymap.json` in the per-user config folder (`%APPDATA%\xppautX`, `~/.config/xppautX`, `~/Library/Application Support/xppautX`) holds only the user's differences (key bindings, pinned commands, the XPPAUT-sequences preset). The core owns it: one `keymap` command (get, set, reset) serves the window and the browser alike, `hello` carries the effective keys, and a bad file is refused whole, with its file, line and value, never applied in part and never silently ignored. No editor yet (W212) (W211, [#265](https://github.com/MuhammadMoustafa/xppautX/issues/265)).

- Closing the Animation or Array plot sheet returns the keyboard focus to the Tools button (their buttons sit in the Tools menu, closed by the choice); a plain `quit` read by a running computation now sends `stopped` before `bye`, as one read after the command does (W217, [#271](https://github.com/MuhammadMoustafa/xppautX/issues/271)).

- One command table in the core gives every command its id, category, label, description, kind and default keys, and `hello` sends it as `command_table`; the sidebar, command search and the desktop File menu are made from it, so they cannot disagree. The File menu now shows the same labels as the sidebar (Reload model for Reload) and, on Windows, Ctrl+O beside Open model (W207, [#261](https://github.com/MuhammadMoustafa/xppautX/issues/261)).

- "nOutput" is now "Store every N steps", `store_every` in a `.odex`, a set file, the protocol and a recording (one row stored per N output steps; 1 stores every step). The old name is no longer accepted in our files: a `.odex` with `@ nout=` or `@ njmp=` and a `set num` of `nout` fail naming it; a `.ode` and an XPPAUT `.set` or `.xpprc` still use NOUT/NJMP and `--convert` writes `store_every` (W206, [#260](https://github.com/MuhammadMoustafa/xppautX/issues/260)).

- One Continue: the legacy `C` prompt ("Continue until:") is gone. The Continue button, the key `c`, Alt+Enter (new default key) and the command search all run the `continue` command with the toolbar's For another / Until time field; it honours Store every N steps (one row per N steps) and rounds the end up to that output grid, the toolbar showing the end it will reach. A session opened from a file now continues storing rows (W213, [#267](https://github.com/MuhammadMoustafa/xppautX/issues/267)).
- Continue until rounds the end up to the next Dt grid point, never short of the end asked for (a value within 1e-6 Dt of a grid point counts as on it); the run toolbar shows "will end at t=..." beside the field, from the core's own rule (W204, W205, [#258](https://github.com/MuhammadMoustafa/xppautX/issues/258)).

- 2D Fit includes retained earlier runs, as 3D Fit did, from one bounds owner (W204, [#258](https://github.com/MuhammadMoustafa/xppautX/issues/258)).

- Fix trace colour editing: Apply keeps a trace's own colour unless a new one was picked; run-control defaults follow Run duration, Dt and the core time; shortcuts act only when they can, and busy says why in the status bar (W205, [#259](https://github.com/MuhammadMoustafa/xppautX/issues/259)).

- Start opened recordings automatically, show playback controls above plots/AUTO, and restore the step list when the desktop page connects after launch; use the established Continue menu command in the sandbox recording and include its AUTO diagram in both demo files (W201).
- Show sampled tail rates for every state, independently of plotted columns, during a run and after snapshot loading (W202).
- Refresh all state values from full-precision solver output while integrating, independently of plotted variables and stored trajectory filters (W200).
- Add one-click Run to steady state, with decimal precision, unchanged-digits hold duration and a maximum duration; show why it stopped and compare full core state values at every Dt (W193, local follow-up card).
- Fix model-picker answers and close requests being discarded before a model loads; end the unloaded session before its native picker closes (W195).
- Accept uppercase F/I shortcuts and show Caps Lock in the status bar; dismiss dialogs with × or an outside click, and close Tools dropdowns on outside click or Escape (W196, W199).
- Restore deletion of added data columns while protecting model/time columns and plotted columns; shift remaining values and recompute formulas on later runs (W197).
- Open axis variables/limits directly from 2D axis labels and 3D axis controls, including additional windows; AUTO axis popovers also dismiss on outside click (W198).
- Add a Freeze control for retaining earlier trajectories, preserve them through Fit/Redraw, and give traces visibility toggles, hover highlighting, and an editable legend name/colour (W199).
- Put run duration and Continue For another / Until time directly in the toolbar. Direct continuation stores every Dt to avoid legacy output-stride overshoot; configured numerical settings and legacy C remain unchanged (W194, local follow-up card).

- Give common run actions one permanent toolbar: Run from initial, Run from current (Last), Continue and Stop; show activity and last stored time, remove duplicate run controls, and keep parameters/solver settings in Values with full command search (W192, local follow-up card).

- Keep shortcut context visible with File/Numerics mode indicators, a Main commands/Esc action, mode-aware hints and explicit submenu cancellation guidance (W189, #242).

- Put Run first and prioritize a compact States/Parameters panel; show ten-digit inspection values and sampled tail rates, and fold Numerics/Recovery to keep the reference model's values visible without scrolling (W187/W190, #240/#243).

- Organize commands into searchable Files, Run, Analysis, Plot and Tools groups with plain labels; clicked actions use stable identities while legacy letter shortcuts remain. Show the model name and session save in the header (W186–W187, #239–#240).
- Add native Open session/Save session as actions, Ctrl/Cmd+O/S, Ctrl/Cmd+K command search, and F6/Shift+F6 pane navigation (W188–W189, #241–#242).
- Explain active solver settings and add temporary working-value checkpoints for parameter/initial-condition exploration; clarify CSV/GIF exports (W190–W191, #243–#244).

### Fixed

- A name that clashes with a member of an array (`x[1..3]'=-x[j]` then `par x2=5`) is refused with `x2 is already a member of the array x[1..3]` at the later declaration, and a clash of two plain names shows the name as written, not in capitals, in `.ode` and `.odex` alike (W214, [#245](https://github.com/MuhammadMoustafa/xppautX/issues/245)).

### Build

- Browser checks wait for a menu dialog to receive keyboard focus before answering it, preserving the phase-plane assertions on slower runners (W182, [#234](https://github.com/MuhammadMoustafa/xppautX/issues/234)).

- Balance Windows sanitizer CI across three automatic AUTO partitions and a combined protocol/web job; name checks by platform, compiler and scope (W181, [#233](https://github.com/MuhammadMoustafa/xppautX/issues/233)).

- Split Linux sanitizer checks across parallel CI jobs while preserving every phase and automatic coverage of new AUTO sections (W180, [#232](https://github.com/MuhammadMoustafa/xppautX/issues/232)).

## [0.2.0] - 2026-10-04

### Build

- Commit embedded C++ assets only after complete generation; failed reads preserve existing targets and output errors name the destination (W178, [#230](https://github.com/MuhammadMoustafa/xppautX/issues/230)).

- Embed the page, icon and Linux window library as C++ spans; remove the remaining project C generators and their C linkage (W173, [#225](https://github.com/MuhammadMoustafa/xppautX/issues/225)).

### Added

- Help > Check for updates asks GitHub only on demand and opens the release page by choice; no download or installation. Run selected web2check sections in the actual Windows WebView2 window with --webview2 (W13c, [#126](https://github.com/MuhammadMoustafa/xppautX/issues/126)).

- `--check` validates `.ode`/`.odex` in memory and prints JSON diagnostics; opening `.ode` shows the converter's structured quirk findings once with their places. W75 ([#123](https://github.com/MuhammadMoustafa/xppautX/issues/123)).

- `--convert` notes `@` option words XPPAUT ignores when spaces surround `=`, while keeping the option values unchanged (W167, [#219](https://github.com/MuhammadMoustafa/xppautX/issues/219)).

- Sliders belong to the session: opening a model replaces them with its presets; saved `.snapx` sessions restore slider definitions and current and frozen nullclines (W135, [#187](https://github.com/MuhammadMoustafa/xppautX/issues/187)).

- Save a session (`name.snapx`) and open it later to continue where you stopped: the model, its values, every plot view, frozen curves, labels and the data; a session of the model already open asks to save first, and the file carries its model and opens only that one (W57, #105; W103, #152)
- Record and Play: every step of a session is written to `name.recx`, one plain text file with the model and the files it read, and played back step by step with a progress bar; a `.recx` opens in the player from the command line or a double-click, and quitting saves a recording in progress (W59, #107)
- Any number of views of the AUTO diagram, side by side, each with its own axes, ranges and variables (W50, #98)
- The page shows what the core holds: earlier runs until Erase, AUTO's hidden branches and the zoom, all saved with the session (W65, #113)
- The OS's own open and save dialogs in the desktop window, and the browser's file picker (filtered, several files at once) in browser mode (W88, #137; W90, #139)
- Double-clicking a `.ode` in Finder opens it in xppautX.app (W91, #140)
- Installable release files beside the archives: a bare `.exe`, a `.deb` (desktop entry, icons, `.ode` file type) and a `.dmg` with xppautX.app (W89, #138)
- `.odex` arrays and constants: `x[j]' = ... for j in a..b by k`, `const n = 20`, derived quantities, and `--convert` writes them (W80, #129)
- Help > About shows the author and contact (W107, #156)
- Opening or reloading a model happens in the same process, and Reload keeps the values you set by name (W61, #109)
- Every error names its file and line and shows the line as written; an error that stopped what you asked opens one dialog with OK, a warning flashes the status bar (W140, #192; W104, #153; W63, #111)
- The random generator's state and the next run's seed are saved in the session, so a session replays the same noise (W123, #174)

### Changed

- Ask every output name in a file dialog, including kinescope GIFs, manifolds, periodic-orbit files, array plots and animation frames; Cancel writes nothing and names are never cut. Defaults share the model base (`lecar.par`, `lecar.ps`, `lecar.gif`, `lecar-curves.csv`); the page gets its defaults from the core (W130, [#182](https://github.com/MuhammadMoustafa/xppautX/issues/182)).

- Logging uses one compile-time checked format API, including AUTO's table and run-time widths and precisions; printed text is unchanged (W172, [#224](https://github.com/MuhammadMoustafa/xppautX/issues/224)).

- Every save uses one atomic write owner. Native save dialogs confirm existing destinations and carry their decision into the command and recording; the core asks only for an existing target without a decision, including the browser's separate model-folder copy. New files need no question. Recordings also retain each core decision, independently of later disk contents. No and failed saves never download an older file; empty exports report “Nothing to save” before asking for a name (W129, [#181](https://github.com/MuhammadMoustafa/xppautX/issues/181)).

- Word options require two dashes (`--silent`, `--setfile`, `--logfile` and the rest); old single-dash words stop and name the new spelling. `-h` is unchanged (W156, [#208](https://github.com/MuhammadMoustafa/xppautX/issues/208)).

- The local HTTP server now closes silent or trickling request heads after five seconds and times out blocked sends after ten seconds, dropping unresponsive event streams; upload bodies keep their 30-second timeout per receive (W164, #216).
- Opening a `.ode` in every mode converts and saves a `.odex` beside it, then opens that file; reserved names and different existing text ask before proceeding, while `-silent` takes suggested names and refuses a conflict. Sessions and recordings store only `.odex` models and refuse `.ode` models; a model can no longer be read from standard input (W154, [#206](https://github.com/MuhammadMoustafa/xppautX/issues/206)).

- Import XPPAUT set checks every named value, refuses another model's names without applying anything, and immediately saves valid imports as `<name>.snapx` beside the `.set`, making that session open; a save failure keeps the imported values. Session labels regain their original XPPAUT spellings; earlier sessions with accidentally renamed labels are refused ([finding 26](docs/xppaut-findings.md#26-set-files-ignore-the-models-names)) (W153, #205)

- The AUTO view's Back button is now labelled Hide AUTO, with its tooltip explaining that AUTO stays open and Show AUTO brings it back (W152, #204)
- The player's 1x is slower: every pace is 1.5 times what it was; the speeds stay 0.5x, 1x, 2x, 4x (W150, #202)
- File > Read set and `-setfile` are now File > Import XPPAUT set and an import of the `.set` file XPPAUT wrote (it must end with XPPAUT's equations, `RHS etc ...`); a `.set` xppautX or its session wrote is refused at its end (W147, #199)
- A session's `model.set` no longer carries the model's equations at its end (the session holds the model itself); sessions saved before this are refused (W145, #197)
- Our own files (`.set`, `.par`, `.ic`, `.snapx`) load all or nothing: a bad value stops the load with the file, line and value, and nothing is applied; XPPAUT guessed ([finding 21](docs/xppaut-findings.md#21-set-par-and-ic-files-read-by-guessing)) (W125, #177)
- Editing a value is one operation: one strict number rule everywhere, a multi-value set applies all or none, with the error on its own field (W131, #183)
- Our files and commands accept only what they hold: a missing or bad piece is an error you see, not a silent default (W116)
- A missing `#include` file stops the load and says so; XPPAUT carried on without it ([finding 5](docs/xppaut-findings.md#5-model-files)) (W139, #191)
- A model's `@` options all take effect, so a model that set `newt_iter`, `newt_tol`, `jac_eps` or `poistop` now runs with them ([finding 1](docs/xppaut-findings.md#1-model-options)) (W119, #170)
- `.ode` keeps its fixed quantities fixed; only `.odex` derives them (W80, #129)
- A setting you edit during a run (parameters, initial conditions, numerics, AUTO's values) applies to the next run, not the one running (W106, #155)
- Menu items and keys are enabled by what they do (control, view, setting, ...) from one table in the core, so Abort, Quit and view commands work while a run computes (W95, #144)
- Quitting asks "Save this session first?" without stopping a computation, a plain quit says bye and exits normally, and browser mode ends 2 s after its tab closes (W59, #107; W110, #162; W112, #164; W108, #160)
- Faster live runs and large plots: a run ends with a small end event, columns are encoded in place, names are resolved once (W122, #173)
- An AUTO run that draws random numbers differs by 0.27 % since the generator belongs to the session (W141, #193)
- The Values panel shows Boundary conditions only for a model that defines them (W99, #148)
- Errors from computations (fits, AUTO, DAEs) come back as values with their place, not as lost messages (W63, #111, #157)
- The Linux release is built on Ubuntu 26.04 and needs its glibc or newer; an older Linux builds from source with `tools/build_linux.sh`, which installs what the build needs (README "An older Linux")

### Removed

- `--script` and its `.jsonl` replay format: recordings are the one playable format. `xppautX run.recx --silent` loads the snapshot and plays without an interface, exits 0 on clean playback or 1 on failure, and reproduces recorded aborts exactly. Silent playback keeps plain-name outputs in the launch folder; existing files require terminal confirmation within 60 seconds (EOF or timeout stops playback), or `--auto`. The same terminal question helper serves conversion, including piped input (W144, [#196](https://github.com/MuhammadMoustafa/xppautX/issues/196)).

- X11 command-line options (`-xorfix`, `-iconify`, `-allwin`, `-ee`, `-white`, `-bigfont`, `-smallfont`, `-forecolor`, `-backcolor`, `-backimage`, `-grads`, `-width`, `-height`, `-mwcolor`, `-dwcolor`, `-bell`, `-def`); unknown options stop with ?no such option? (W156, [#208](https://github.com/MuhammadMoustafa/xppautX/issues/208)).

- `.autox` and `.autoset`: AUTO's Save diagram saves the session (`.snapx`) and Load opens one; AUTO settings, diagram, views and orbits live under its `auto/` members. Reload keeps AUTO's settings without a separate file (W155, #207)

- File > Write set (the `w` key): a `.set` is no longer a file xppautX saves; a session (`.snapx`) holds all it did, `.par` and `.ic` stay (W147, #199)
- The options file: a model's `option` line is refused at load with the line named, and `default.opt` is never read; write the settings as `@` lines, as XPPAUT in effect already required ([finding 4](docs/xppaut-findings.md#4-model-options)) (W139, #191)

### Fixed

- Asset generators report failed output streams and refuse directory inputs consistently with libc++; cross-platform checks use canonical temporary paths and wait for exported files and AUTO menus (W179, [#231](https://github.com/MuhammadMoustafa/xppautX/issues/231)).
- Update checks refuse direct asset URLs, report opener failures, allow retry after closing a pending check, and show the actual error source (W176, [#228](https://github.com/MuhammadMoustafa/xppautX/issues/228)).
- Confine received model reads to their folder and bound table counts before allocation; preserve the solver during conversion and included files in JSON diagnostics; remove false zero-divisor and comparison warnings (W175, [#227](https://github.com/MuhammadMoustafa/xppautX/issues/227)).
- Bind native saves to the actual picker choice; confirm frozen destinations separately; preserve each browser export?s bytes; route desktop Save/CSV buttons through the picker; reject malformed array render values; choose animation PPM sequences once; open recordings with an open picker (W177, [#229](https://github.com/MuhammadMoustafa/xppautX/issues/229)).

- Windows file downloads and exclusive temporary-file opens refuse links, junctions and non-regular files on the opened handle, closing the pathname-check race (W174, [#226](https://github.com/MuhammadMoustafa/xppautX/issues/226)).

- Long wildcard patterns in file dialogs no longer exhaust the stack. Refused workspace file operations preserve the requested name and error location (W121b review, [#172](https://github.com/MuhammadMoustafa/xppautX/issues/172)).

- Windows scratch cleanup preserves folders when their owner process cannot be queried; a permission error is no longer treated as proof that the process exited (W121b, [#172](https://github.com/MuhammadMoustafa/xppautX/issues/172)).

- Preserve whole protocol names, values and file-dialog paths; report oversized input instead of discarding it silently. File-open errors name the requested file and why it failed; an unreadable workspace listing is shown as an error. The player offers the core's full 0.25–8× speed range (W121b, [#172](https://github.com/MuhammadMoustafa/xppautX/issues/172)).

- A session saved after a Periodic AUTO run (its diagram continues the period, parameter 11) opens again: the file's check took that parameter index for an error and refused the whole session. web2check opens such a session as the desktop window does and checks the main plot after Back (W102, [#151](https://github.com/MuhammadMoustafa/xppautX/issues/151)).

- macOS CI checks read every thread's state from the aligned `ps -M` table and wait for text views to render after their events (W171, [#223](https://github.com/MuhammadMoustafa/xppautX/issues/223)).

- The data table stops requesting rows at the end of the data, avoiding a render/effect loop on slow runners. CI checks establish a heavy run is computing at every value commit, wait for the slider pick to draw, and read macOS thread states from `ps -M`'s STAT column (W170, [#222](https://github.com/MuhammadMoustafa/xppautX/issues/222)).

- Recordings can write their scratch outputs through macOS's linked temp-folder ancestors. Random states use one MT19937-64 layout across compilers, retaining already matching saved states and rejecting other layouts at their file and line. Option filenames accept Windows `~` short paths; sanitizer autocheck shards track the current sections (W169, [#221](https://github.com/MuhammadMoustafa/xppautX/issues/221)).

- Non-ASCII text in the page’s log is shown as written, including accented letters and four-byte UTF-8 characters (W121a, [#172](https://github.com/MuhammadMoustafa/xppautX/issues/172)).

- Picture exports share the complete picture registry, and a stopped array range movie preserves the previous file. AUTO's CSV exports share the data registry's quoting and use LF line ends on Windows; curve fit and plot Import diagram read full double-precision CSV and DAT values through the same registry as Save data, while storage exports retain their previous bytes. Malformed or ragged text tables are refused as a whole (W137, [#189](https://github.com/MuhammadMoustafa/xppautX/issues/189)).

- Startup `--runnow` and model `@ runnow=1` use protocol jobs, announcing computing and accepting Abort. Animator grabs that integrate are computing recording steps; speed and pause during Go are retained and replayed at their checkpoints. W136 ([#188](https://github.com/MuhammadMoustafa/xppautX/issues/188)).

- The `.ode` converter omits division guards for proven nonzero divisors, including positive sum-index products, and evaluates guarded divisors once through a generated function. A `.odex` formula table may call a function written after it. A divisor that is a parameter is guarded again: one was written plainly, so setting it to 0 gave IEEE's infinity instead of XPPAUT's number. The examples keep their checksums (W165, [#217](https://github.com/MuhammadMoustafa/xppautX/issues/217)).

- Method selection uses one validator: unsuitable menu choices keep the previous method instead of switching to Adams; unknown or unsuitable `@ meth` values fail the whole load at their file and line (W132, [#184](https://github.com/MuhammadMoustafa/xppautX/issues/184)).
- A `.ode`'s `@ meth=` is read by its first letter, as XPPAUT reads it (`modeuler` is Mod. Euler, `symplectic` Stiff), and the converter writes the method's name in the `.odex`, with a note when the value named another method; the examples' `.odex` files were converted again (W132, [#184](https://github.com/MuhammadMoustafa/xppautX/issues/184)).

- A trailing `#` in a `.ode` line now starts a comment, so its words no longer become parameters or break formulas; conversion keeps the comment text. Existing include directives and Volterra convolution separators still work ([finding 28](docs/xppaut-findings.md#28-a--comment-after-a-declaration-makes-names-of-its-words)) (W160, #212).
- The local HTTP server answered any number of connections and uploads at once; it now takes at most 256 connections and 32 uploads, and one over a limit gets 503 with its body never read (W161, #213).
- Results (fits, statistics, Liapunov exponents, saved BVP points and AUTO toggles) use the status bar; every exit counts errors, including failed `-silent` writes, and a full kinescope is reported once by its command (W133, #185).
- Values > Load replaced a different `.par` or `.ic` of the same name in the model's folder without asking, and a copy into the folder could land during a run; every upload now asks Replace, Keep both or Cancel, and none is taken while a computation runs, whether it came from the page or the protocol's `file` command (W134, #186)
- The numbers no longer depend on the machine: the same model gave different results on a CPU with FMA, a glibc that picked another variant of `exp`, `sin`, `pow`, ... or a compiler that fused `a*b+c` (one CI runner differed from another computer in 29 of 184 example models); the functions `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `sinh`, `cosh`, `tanh`, `exp`, `log`, `log10`, `pow`, `hypot`, `erf`, `erfc` and `lgamma` are now correctly rounded (CORE-MATH, `xpp::math`) and the build never contracts into an FMA, so every CPU and system gives the same bits; Bessel `besselj` and `bessely` now use a vendored musl implementation over `xpp::math` too (W163, #215). Results of models that use these functions change in the last digits once ([finding 29](docs/xppaut-findings.md#29-the-numbers-depend-on-the-cpu-and-on-the-compilers-fma)) (W159, #211)
- AUTO orbit loading checks restart dimensions and grows storage before copying rows; session loads validate complete restart payloads and bound archive expansion and AUTO state (W155, #207; XPPAUT finding 27)
- Playing a recording shows its caption, keys, controls and step list over the AUTO view and the other full views, and lights AUTO's buttons (W150, #202)
- A file dialog opened after playing a recording (or an AUTO run) started in a scratch folder under the temp folder; it now starts in the folder of the file of its kind last opened or saved, else the model's folder (W151, #203)
- A session or set file with an output stride or step of 0 loaded, and the next run divided by zero; every numerics value from a file, an `@` line or the Numerics dialog is now checked by the same rule, so `@ nout=0` stops the load too ([finding 25](docs/xppaut-findings.md#25-an-output-stride-of-0-divides-by-zero)) (W145, #197)
- A browser column you added and saved with the session is back when the session is opened (W145, #197)
- A session file refuses, at its line: more added columns than the model has room for, an added column's formula that does not compile, a damaged random generator state, a mark of a type or colour that does not exist, and a manifest key given twice (W145, #197)
- A DAE run stops at a fold instead of stepping over it onto another branch or onto no solution; the results are checked against independent Python solutions ([finding 6](docs/xppaut-findings.md#6-daes)) (W127, #179; W126, #178)
- AUTO's orbit, periodic and homoclinic starts with nothing integrated refuse with "Integrate first" instead of reading outside the data ([finding 11](docs/xppaut-findings.md#11-auto-starts-from-the-last-integration-with-nothing-integrated)) (W97, #146)
- Memory errors in borrowed data columns, CVODE and AUTO's fort.8 reading (W117)
- A page's command lost on its way to the core, shown as "Failed to fetch", is sent again and taken once (W124, #176)
- The plot vanished at some window widths and Show AUTO covered the message strip (W98, #147)
- After grab Hopf + Periodic, F did nothing: Back from AUTO during a grab or plot mode now cancels it (W100, #149)
- "Not a valid set" appeared when the Param set menu was cancelled (W101, #150)
- Open session restores the main plot's rows (W102, #151)
- Fit widens a flat range, and a map step's failure is reported (W50, #98; W63, #157)
- Plot tabs keep the focus when a key's pick is held (W93, #142)

## [0.1.0] - 2026-09-28

The first release of xppautX, a fork of XPPAUT 8.0 with no X11: one program
for Linux (x64), Windows (x64) and macOS (Apple silicon and Intel).

### Added

- `xppautX model.ode` opens the front end in a window of its own: WebView2 on Windows, WKWebView on macOS, WebKitGTK 4.1 on Linux (elsewhere it names what to install and uses your browser); `--browser` serves the page to your browser
- `-silent` runs a model headless and writes `output.dat`, as XPPAUT does
- `--server` speaks the JSON protocol on stdin/stdout ([docs/protocol.md](docs/protocol.md)), the interface the page and scripts use
- The source of each binary (GPL v2) is attached to the release

### Changed

- What differs from XPPAUT 8.0, by area, is in [docs/xppautx-vs-xppaut.md](docs/xppautx-vs-xppaut.md); the bugs found in XPPAUT's own code and fixed here are in [docs/xppaut-findings.md](docs/xppaut-findings.md)

### Removed


- The X11 front end (the page replaces it)

Linux needs glibc 2.39 or newer (Ubuntu 24.04, Debian 13, Fedora 40 and
later); macOS 13.3 or newer. The macOS and Windows binaries are not signed, so
the system asks you to allow them the first time (macOS: `xattr -dr
com.apple.quarantine` on the unpacked folder, or right-click > Open; Windows:
SmartScreen's "More info" > "Run anyway").
