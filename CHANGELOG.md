# Changelog

What is new, what changed and what was fixed in xppautX, newest first. The
form is [Keep a Changelog](https://keepachangelog.com/en/1.1.0/); each entry
ends with its card and issue (docs/roadmap.md, GitHub). A card that changes
what a user meets adds its line under Unreleased. How xppautX differs from
XPPAUT 8.0, area by area, is [docs/xppautx-vs-xppaut.md](docs/xppautx-vs-xppaut.md);
the bugs found in XPPAUT itself are in [docs/xppaut-findings.md](docs/xppaut-findings.md).

## [Unreleased]

### Added

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

- The data table stops requesting rows at the end of the data, avoiding a render/effect loop on slow runners. CI checks establish a heavy run is computing at every value commit, wait for the slider pick to draw, and read macOS thread states from `ps -M`'s STAT column (W170, [#222](https://github.com/MuhammadMoustafa/xppautX/issues/222)).

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
