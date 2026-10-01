# Changelog

What is new, what changed and what was fixed in xppautX, newest first. The
form is [Keep a Changelog](https://keepachangelog.com/en/1.1.0/); each entry
ends with its card and issue (docs/roadmap.md, GitHub). A card that changes
what a user meets adds its line under Unreleased. How xppautX differs from
XPPAUT 8.0, area by area, is [docs/xppautx-vs-xppaut.md](docs/xppautx-vs-xppaut.md);
the bugs found in XPPAUT itself are in [docs/xppaut-findings.md](docs/xppaut-findings.md).

## [Unreleased]

### Added

- Save a session (`name.snapx`) and open it later to continue where you stopped: the model, its values, every plot view, frozen curves, labels and the data; a session of the model already open asks to save first, and the file carries its model and opens only that one (W57, #105; W103, #152)
- AUTO's own file, `name.autox`: the diagram at full precision with AUTO's settings, saved and loaded without a whole session; it carries its model too (W92, #141; W103, #152)
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

- Our own files (`.set`, `.par`, `.ic`, `.snapx`, `.autox`, AUTO's settings) load all or nothing: a bad value stops the load with the file, line and value, and nothing is applied; XPPAUT guessed ([finding 21](docs/xppaut-findings.md#21-set-par-and-ic-files-read-by-guessing)) (W125, #177)
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

### Removed

- The options file: a model's `option` line is refused at load with the line named, and `default.opt` is never read; write the settings as `@` lines, as XPPAUT in effect already required ([finding 4](docs/xppaut-findings.md#4-model-options)) (W139, #191)

### Fixed

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
