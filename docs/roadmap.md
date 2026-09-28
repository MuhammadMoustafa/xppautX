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
| W6  | #18 | New UI: the views | W5 | done (T2-T19) |
| W7  | #19 | Core refactor for single responsibility | W4 | done (W7a-W7e) |
| W7a | #19 | Split ui_json.cpp (3000 lines) by responsibility: protocol output and the JSON reader, prompts, windows and pixels, AUTO, animation and browser, commands; one internal header; no behaviour change | none | done |
| W7b | #19 | commands.c to C++; W11 step 3's line reader and file writer in xpp_io, applied to the small files that read or write data (lunch-new, browse_data, colormap, my_svg, diagram), each converted to C++ | none | done |
| W7c | #19 | Globals into structs (xpp_globals: 58 externs grouped by owner) | W7a, W7b | done |
| W7d | #19 | AUTO's file I/O (autlib1/3, auto_nox: fort files, .s/.b/.d) through xpp_io's reader and writer; byte-identical diagrams | W7b | done |
| W7e | #19 | The File menu's dead entries (found in W7c): Bell and Tips removed (nothing has read them since X11), Help opens the page's Help at the File menu chapter (a `help` event), the manual updated; load_eqn's option merge takes the seed's flag, not the small font's | W7c | done |
| W8  | #20 | Remove the X11 front end (pulled ahead of W6, 2026-09-23: the classic web page covers its features until T17) | none | done (4136541) |
| W9  | #21 | WebAssembly build (proof of concept) | maintainer's OK to install emsdk | blocked |
| W10 | #22 | Replayable interruptions in scripts | none | done (539b300) |
| W11 | #23 | One I/O module: logging only, safe formatting, file reading and writing | none (step 3 with W7) | done (steps 1-2 then; step 3 finished by W32b (xpp_io's Writer/readers, xpp_files for every other file operation) and the W33 sweeps: tools/filecheck.sh's baseline is empty) |
| W12a | #24 | The manual as Markdown (docs/manual/), current with web2; W12 steps 1, 2, 4 | T20, T21 | done |
| W12b | #24 | web2's Help view from docs/manual/, linked from menus and dialogs; W12 step 3 | W12a, T22 | done |
| W12c | #24 | Chapter 1 of the manual rewritten for xppautX (what it is, installing, starting it, how it relates to XPPAUT); the rest stays Bard Ermentrout's text, credited | W12a | done |
| W13a | #25 | The desktop window: web2 in the OS web view, closing it ends xppautX, `--browser` keeps browser mode, the app name, the icon slot (placeholder until one is chosen), a menu bar with File (Open model, Quit) and Help (Manual, About); W13 steps 1-3 without updates | T21, W12b | done |
| W13b | #25 | .ode files open with xppautX (Windows per-user registry, Linux .desktop/MIME, macOS .app Info.plist), a second .ode a second window, no console window when started from Explorer (GUI subsystem, attaching to a parent console for the command-line modes), the Linux window icon; W13 step 4 | W13a | done |
| W13c | #126 | Check for updates (GitHub releases; asks before any download, only when chosen or opted in) and web2check once in WebView2; W13 steps 3 (updates) and 5 | W13b, a first release (#3) | blocked |
| W13d | #25 | The macOS window shipped: the macOS builds (CI's artifact and the release's archive) with `WINDOW=1`, and the installing instructions for every platform brought up to date (README, docs/manual, the release notes and the archive's README.txt: Windows' WebView2, Linux's WebKitGTK or the browser, opening an unsigned binary on macOS with `xattr -dr com.apple.quarantine`, what a tester reports); the tester's run is #4 | none (W13e done; CI's macOS `WINDOW=1` build passes, 2026-09-24) | done |
| W13e | #25 | The Linux window in the release without a hard dependency: one xppautX for every Linux (no second version or package): the window's GTK/WebKitGTK part built as a shared library embedded in xppautX (as web2/dist is) and loaded from memory (`memfd_create`, then `dlopen` of `/proc/self/fd/N`) only in window mode; when WebKitGTK is missing xppautX says so with the install command for the system (apt, dnf, pacman, zypper by /etc/os-release) and opens the browser; `-silent`, `--server` and `--browser` never load it; the release's Linux job builds and ships it | none | done |
| W13f | #25 | The window fits the screen: its first size (1280x840, scaled by the display's DPI) shrunk to the work area (the screen less the taskbar) of the monitor it opens on, and centred there (Windows; GTK on Linux; macOS left to W13d) | W13e (the same file) | done |
| W14 | #26 | Smaller downloads, a faster and fairer CI: the release archives and CI artifacts carry stripped binaries (Linux 15.2 -> 3.3 MB, Windows 18.5 -> 4.1 MB; debug info kept in local builds); the Windows job's unit tests in parallel, and no toolchain install per run (the runner image's own MinGW gcc, e.g. Strawberry Perl's, the same gcc 13 as local builds, if present; else MSYS2's installed packages cached, not reinstalled); Linux's WebKitGTK packages cached rather than fetched each run; the macOS job also runs the checks that do not compare Linux's exact checksums (modecheck, autocheck's tolerant parts, every example for crashes) | none | done |
| W15 | #27 | One source of truth for AUTO's eigenvalues: core/auto_stability.cpp (C++, autevd.c converted) takes what AUTO's stability check computes (replacing send_eigen/send_mult and the global my_ev slot) and gives a point's values only to that point, else "not computed"; a run's first point is "not computed" unless it restarts from a label of the same kind, whose values it takes; the page gets them only from the stored point; the dead plot_stab seam goes; .auto files keep their format; unit tests, autocheck cases and the saved AUTO baselines updated on purpose (upstream XPPAUT shows the previous run's values there) | none | done |
| W16 | #29 | Parallel CI, same checks: verify.sh builds the unit tests and the LTO check on every core (they built on one: 60 s and 49 s on CI); the Linux browser checks a job of their own beside verify; the macOS example models in parallel | none | done |
| W17 | #30 | Each platform checks itself, source checks once: the LTO type check, stdoutcheck, formatcheck and the warning count in one small job; build, unit tests, protocol, AUTO, command-line modes, every example and the browser checks on Linux, Windows and macOS against their own binaries (long parts as parallel jobs); the examples compared with Linux's baseline where a platform matches it exactly, else with its own, CI-generated, with CI handing out a regenerated one when numerics change on purpose | W16 (the same workflow file) | done |
| W18 | #31 | Windows passes the checks it runs since W17: `xppautX.exe --server` exits at end of input from NUL (autocheck timed out), the 4 web2check failures on the runner (a field's blur commit, the focus back to the Values/Text buttons, AUTO's Stop status), every check step of every job runs even after an earlier check failed (the examples and their artifact included; the build still gates them); a macos-browser job (web2check on macOS, which W17's card asked for and missed); the jobs named by what they test (linux-core, linux-ui, windows-core, windows-ui, macos-core, macos-ui, linux-sanitizers, source); then tests/examples.windows.md5 from CI | none | done |
| W19 | #32 | No AUTO scratch folders left behind: at start xppautX removes the xppautoX-<pid>-N folders of processes that are gone (never a live one's); the abnormal ends that can clean up do; the test tools that kill xppautX clean up after it; autocheck's scratch checks do not depend on an empty temp folder (about 1400 leftovers found by W18) | none | done |
| W20 | #33 | The UI checks pass on Windows and macOS: Shift+drag panning on both, and macOS's keyboard and focus failures (arrow keys, a dialog's first box, keys on the plot, the AUTO axes box, a 3D projection), each found as a product bug or a test/driver issue and fixed, iterated on CI until all eight jobs are green | none | done |
| W21 | #34 | Memory: no uninitialised reads, ownership in C++: tools/valgrindcheck.sh (every example, unit tests, servercheck, autocheck under memcheck) and every report fixed; one allocator enforced by a source check (form_ode.cpp's raw frees fixed); xpp_malloc zeroed by default (XPP_MEM_INIT=0 for valgrind); per-module cards moving hand-paired xpp_malloc/xpp_free to std::vector/std::string/unique_ptr as files convert (found at W20: export's uninitialised buffer) | none | ready |
| W22 | #35 | Sanitizers on macOS: a macos-sanitizers CI job building xppautX with Apple clang's AddressSanitizer and UBSan (no LeakSanitizer: not on Apple Silicon) and running tools/asancheck.sh's checks under it (smoke run, every example, unit tests, servercheck, autocheck), asancheck.sh taught a no-leaks mode for it; every report fixed | none | done |
| W23 | #36 | Clang on Windows: a windows-clang CI job with MSYS2's CLANG64 toolchain (clang + libc++ + compiler-rt), first building xppautX and passing the windows-core checks (a third compiler on Windows), then an ASan + UBSan build (compiler-rt supports both on x86_64 MinGW) running asancheck.sh's checks; every report fixed; the local MinGW gcc stays the default build | none | done |
| W24 | #39 | No dead code: a linker report (tools/deadcode.sh: -ffunction-sections, --gc-sections, --print-gc-sections over the window, browser and test builds together) lists every function and file nothing reaches; each is removed, or kept with a reason in the script (found 2026-09-25: ~180 C functions and whole files such as CVODE's SPGMR solver, cvspgmr/spgmr/iterativ/cvdiag); a source check fails new dead code; a sweep, so nothing is converted to C++ (CLAUDE.md) | none | done |
| W25 | #40 | Messages worth reading: the ~620 plintf calls (INFO, which browser mode shows in the page's log) audited: what a user needs stays INFO or becomes WARN, developer chatter from the X11 era ("Kernel mu=...", check_inout's type/index dump) goes to DEBUG or is removed; one logging API: plintf retired (every call an xpp_log with its level), a type-checked xpp::log (std::format) for C++ files, a check that fails a new plintf; a short guide in CLAUDE.md for which level a new message takes | none | done |
| W26 | #42 | Exports pandas reads as they are: CSV with a header row of names (pandas.read_csv, MATLAB readtable) for the data browser's Write (time and every variable), AUTO's Write pts and All info (one row per point: branch, point, type, label, parameters, the plotted values, stability), and an imported orbit; what is not one table (a diagram's eigenvalues or Floquet multipliers per point) goes to a second CSV keyed by branch and point, not into cells; the old whitespace formats stay readable where the core reads them back (Bif.diag, Load diagram) | none (overlaps W25 in files) | done |
| W27a | #43 | C to C++, CVODE and its linear algebra, together (llnltyps.h's bool macro: CLAUDE.md): cv2, cvode, cvband, cvdense, band, dense, llnlmath, vector | none | done |
| W27b | #44 | C to C++, AUTO: autlib4, autlib5, autpp, conpar2, setubv2, worker2, gogoauto, eispack and the f2c helpers (cabs, d_imag, d_lg10, d_sign, i_dnnt, i_nint, pow_dd, pow_di, pow_ii, r_lg10, z_abs, z_exp, z_log) | none | done |
| W27c | #45 | C to C++, integrators and numerics: integrate, odesol2, stiff, gear, dormpri, adj2, del_stab, delay_handle, volterra2, dae_fun, markov, pp_shoot, numerics, histogram, fftn (its #include __FILE__), do_fit, torus, my_rhs, derived, storage, tabular | none | done |
| W27d | #46 | C to C++, parser, model loading, front-end core and entry points: parserslow2, simplenet, load_eqn, flags, comline, read_dir, edit_rhs, menus, graf_par, graphics, axes2, nullcline, my_ps, scrngif, array_print, userbut, xpp_util, xpp_batch, xpp_session, xppautx_main; sbml2xpp.c (never built, needs libsbml) deleted as dead code | none | done |
| W28 | #47 | A const-correct C API: the dialog and message functions (err_msg, do_string_box_of, new_string, file_selector, TwoChoice, pop-up lists, menus ...) and the other core functions that take text they only read take `const char *` (and `const char * const *` for lists), so the C++ files lose their `(char *)"..."` casts and the per-file str()/strs() helpers; tools/sourcecheck.sh fails a new cast of a string literal | none | done |
| W29-0 | #48 | The unsafe-C ratchet: tools/unsafecheck.sh counts per core file the unsafe C idioms (xpp_malloc/xpp_calloc/xpp_realloc/xpp_free, fixed char buffers, xpp_strlcpy/xpp_strlcat/xpp_snprintf/XPP_SPRINTF/XPP_STRCPY, fopen/fscanf/fgets/sscanf, C casts where detectable) against a committed baseline (tests/unsafe.baseline); verify.sh prints the total and fails when a file's count grows; --update rewrites the baseline when counts drop | none | done |
| W29a | #49 | Safe C++, the ODE solvers: cv2, cvode, cvband, cvdense, band, dense, llnlmath, vector, odesol2, stiff, gear, dormpri, volterra2, dae_fun, delay_handle, markov: safe C++ in place of the unsafe C idioms (CLAUDE.md "C and C++"): RAII containers instead of hand-paired xpp_malloc/xpp_free, std::string/std::string_view instead of char buffers and xpp_strlcpy/xpp_snprintf, xpp::format/xpp::log, xpp::LineReader/TokenReader/Writer instead of fopen/fscanf/fgets, std::array/std::vector/std::span instead of raw arrays, static_cast instead of C casts; numerics unchanged (md5s); tools/unsafecheck.sh's count for these files drops to what is left with a reason | W28, W29-0 | done (rest in W33) |
| W29b | #50 | Safe C++, integration and analysis: integrate, adj2, del_stab, pp_shoot, numerics, histogram, fftn, do_fit, torus, my_rhs, derived, storage, tabular: safe C++ in place of the unsafe C idioms (CLAUDE.md "C and C++"): RAII containers instead of hand-paired xpp_malloc/xpp_free, std::string/std::string_view instead of char buffers and xpp_strlcpy/xpp_snprintf, xpp::format/xpp::log, xpp::LineReader/TokenReader/Writer instead of fopen/fscanf/fgets, std::array/std::vector/std::span instead of raw arrays, static_cast instead of C casts; numerics unchanged (md5s); tools/unsafecheck.sh's count for these files drops to what is left with a reason | W28, W29-0 | done (rest in W33) |
| W29c | #51 | Safe C++, AUTO's numerics: autlib1-5, eispack, conpar2, setubv2, worker2, f2c_helpers: safe C++ in place of the unsafe C idioms (CLAUDE.md "C and C++"): RAII containers instead of hand-paired xpp_malloc/xpp_free, std::string/std::string_view instead of char buffers and xpp_strlcpy/xpp_snprintf, xpp::format/xpp::log, xpp::LineReader/TokenReader/Writer instead of fopen/fscanf/fgets, std::array/std::vector/std::span instead of raw arrays, static_cast instead of C casts; numerics unchanged (md5s); tools/unsafecheck.sh's count for these files drops to what is left with a reason | W28, W29-0 | done |
| W29d | #52 | Safe C++, AUTO's front: auto_nox, diagram, autevd, auto_data, auto_settings, auto_stop, auto_stability, gogoauto, autpp, csv_export: safe C++ in place of the unsafe C idioms (CLAUDE.md "C and C++"): RAII containers instead of hand-paired xpp_malloc/xpp_free, std::string/std::string_view instead of char buffers and xpp_strlcpy/xpp_snprintf, xpp::format/xpp::log, xpp::LineReader/TokenReader/Writer instead of fopen/fscanf/fgets, std::array/std::vector/std::span instead of raw arrays, static_cast instead of C casts; numerics unchanged (md5s); tools/unsafecheck.sh's count for these files drops to what is left with a reason | W28, W29-0 | done (rest in W33) |
| W29e | #53 | Safe C++, the parser and model loading: parserslow2, form_ode, simplenet, load_eqn, flags, comline, read_dir, edit_rhs, extra, lunch-new, xpp_batch, xpp_session: safe C++ in place of the unsafe C idioms (CLAUDE.md "C and C++"): RAII containers instead of hand-paired xpp_malloc/xpp_free, std::string/std::string_view instead of char buffers and xpp_strlcpy/xpp_snprintf, xpp::format/xpp::log, xpp::LineReader/TokenReader/Writer instead of fopen/fscanf/fgets, std::array/std::vector/std::span instead of raw arrays, static_cast instead of C casts; numerics unchanged (md5s); tools/unsafecheck.sh's count for these files drops to what is left with a reason | W28, W29-0 | done (rest in W33) |
| W29f | #54 | Safe C++, the UI core and plot data: menus, commands, graphics, graf_par, axes2, nullcline, my_ps, my_svg, scrngif, array_print, arrayplot, aniparse, ani_data, browse_data, grobs, userbut, colormap, xpp_ui, plot_data, phase_data, marks_data, series_enc: safe C++ in place of the unsafe C idioms (CLAUDE.md "C and C++"): RAII containers instead of hand-paired xpp_malloc/xpp_free, std::string/std::string_view instead of char buffers and xpp_strlcpy/xpp_snprintf, xpp::format/xpp::log, xpp::LineReader/TokenReader/Writer instead of fopen/fscanf/fgets, std::array/std::vector/std::span instead of raw arrays, static_cast instead of C casts; numerics unchanged (md5s); tools/unsafecheck.sh's count for these files drops to what is left with a reason | W28, W29-0 | done (rest in W33) |
| W29g | #55 | Safe C++, the protocol, platform and base modules: ui_json, json_ani, json_auto, json_io, json_prompts, json_state, json_windows, xpp_http, xpp_inbox, xpp_files, xpp_io, xpp_mem, xpp_log, xpp_job, xpp_util, xpp_globals, xpp_sha256, xpp_win32, xpp_window, xpp_window_hint, xpp_window_loader, xppautx_main: safe C++ in place of the unsafe C idioms (CLAUDE.md "C and C++"): RAII containers instead of hand-paired xpp_malloc/xpp_free, std::string/std::string_view instead of char buffers and xpp_strlcpy/xpp_snprintf, xpp::format/xpp::log, xpp::LineReader/TokenReader/Writer instead of fopen/fscanf/fgets, std::array/std::vector/std::span instead of raw arrays, static_cast instead of C casts; numerics unchanged (md5s); tools/unsafecheck.sh's count for these files drops to what is left with a reason | W28, W29-0 | done (rest in W33) |
| W30 | #57 | Duplication audit: tools/dupcheck.sh (sourcecheck --check, allowlist with reasons) lists duplicated functions, blocks and struct definitions in core/; its first report completes the list of copies W32 absorbs | none | done |
| W31a | #58 | Tests for the Fourier features: stocHast's Fourier series, power spectrum, correlations, windowed spectral density, and fftcon against a direct convolution, by known answers | none | done |
| W31b | #59 | Tests for the stochastic menu's random-number features, checking statistics with tolerances (they must survive W32a's new generator) | none | done |
| W31c | #60 | Golden-file tests for the PostScript, SVG, GIF and array-print outputs (W29f's byte-compare harness, committed, run by verify.sh) | none | done |
| W32a | #61 | xpp_math: pocketfft replaces fftn and histogram's FFT wrappers; std::mt19937_64 plus our own distributions replace the Numerical Recipes generator; one LU solve; gear uses EISPACK's eigenvalues; std helpers. Stochastic models' checksums rebaselined once | W30, W31a, W31b | done |
| W32b | #62 | xpp_files: every file operation (open handles with print, safe replace, copy/append/rename/delete, temp folders) in one module; open_write_file, AUTO's file helpers and the five print copies go; tools/filecheck.sh enforces it | W30, W31c, W32a | done |
| W32c | #63 | The dialog API (xpp_ui.h) returns std::string instead of filling char[MAX_LEN_SBOX] buffers; callers changed only as the signatures require | W32b | done |
| W32d | #64 | One definition of the shared data: HIST_INFO and TABULAR once, the data store as a class (histogram's aliasing explicit), the AUTO diagram's list as a container | W32c | done |
| W33a | #65 | Sweep, the ODE solvers (W29a's files): each file converted once to the W32 modules and the rest of its unsafe C idioms (fprintf, buffers, allocations), output byte-identical | W32a-d | done |
| W33b | #66 | Sweep, integration and analysis (W29b's files): each file converted once to the W32 modules and the rest of its unsafe C idioms (fprintf, buffers, allocations), output byte-identical | W32a-d | done |
| W33c | #67 | Sweep, AUTO's numerics (W29c's files): each file converted once to the W32 modules and the rest of its unsafe C idioms (fprintf, buffers, allocations), output byte-identical | W32a-d | done |
| W33d | #68 | Sweep, AUTO's front (W29d's files): each file converted once to the W32 modules and the rest of its unsafe C idioms (fprintf, buffers, allocations), output byte-identical | W32a-d | done |
| W33e | #69 | Sweep, the parser and model loading (W29e's files): each file converted once to the W32 modules and the rest of its unsafe C idioms (fprintf, buffers, allocations), output byte-identical | W32a-d | done |
| W33f | #70 | Sweep, the UI core and plot data (W29f's files): each file converted once to the W32 modules and the rest of its unsafe C idioms (fprintf, buffers, allocations), output byte-identical | W32a-d | done |
| W33g | #71 | Sweep, the protocol, platform and base modules (W29g's files): each file converted once to the W32 modules and the rest of its unsafe C idioms (fprintf, buffers, allocations), output byte-identical | W32a-d | done |
| W34 | #72 | Evaluate SUNDIALS CVODE in place of the vendored CVODE (later; changes every CVODE result) | W33a | later |
| W35a | #73 | QA 2026-09-26 SCI-001: one JSON number writer in json_io (not finite: null) for every event, the three copies merged; AUTO never writes a non-finite point's parameters into the model; servercheck parses strictly | none | done |
| W35b | #74 | QA 2026-09-26 MI-001, WF-002: the core's text is UTF-8: one JSON string writer and reader (json_io's and xpp_files' merged) that keep UTF-8; the Windows manifest's activeCodePage UTF-8 for the narrow file APIs and argv | none | done |
| W35c | #75 | QA 2026-09-26 INPUT-001: a model that fails to load exits non-zero in every mode | none | done |
| W35d | #76 | QA 2026-09-26 UX-001, WF-001: a box refuses a keystroke or paste that could not become a valid text of its kind (a %formula excepted: the core judges it); Escape in a Values field drops its edit (the narrow sheet's Escape no longer takes it first); a formula the core refuses keeps its draft and message | none | done |
| W35e | #77 | QA 2026-09-26 DESK-001: the web view's start error (code, message) in the fallback warning, the runtime hint only when the runtime is missing | none | done |
| W35f | #78 | The example models that do not load by themselves ("noload" since W35c): none was a parser bug. clustor.ode had a missing `]` (fixed, now checked by its md5); the other eight (hedge, tstar: malformed on purpose; tstdll2, candelator, tsthom3: drafts; pHtools_*: a module that never existed) each say why at their top | none | done |
| W36 | #79 | CI's windows-clang faster: its sanitizers a job of their own (windows-clang-sanitizers); asancheck builds the unit tests with -j and runs servercheck, webcheck and autocheck side by side | none | done |
| W37 | #80 | Compile the formatting once: xpp::format, format_to_buf, xpp::log and buf_format check the format at the call and format in xpp::vformat (xpp_io.cpp), not std::format inline in every file (clang: 278 to 170 CPU-s for the core) | none | done |
| W38 | #81 | fftcon reads one past its weight table (simplenet.cpp update_fft: FFTCONP reads w[n] and skips w[n2-1], FFTCON0 w[2N]; the load check allows exactly n/2N points): decide the layout, make the check and update_fft agree, say the length in the manual | W32a | done |
| W39 | #82 | Remove examples/ode/sine-circel.ode, a misspelled byte-for-byte copy of examples/canonical/sine-circle.ode, and its baseline lines | none | done |
| W40 | #83 | web2check's timing-sensitive checks flake on the macOS runner: frame budgets judged on most frames or scaled on CI, interactions wait for the page to settle, a check that fails once is rerun and reported FLAKY | none | done |
| W41 | #84 | windows-clang (~9 min) and windows-clang-sanitizers (~11 min) under ~6 min: a cached or prebuilt clang toolchain in place of setup-msys2's ~170 s, parallel test links, autocheck and examples side by side, asancheck split into parallel parts; no check dropped | none | done |
| W42 | #85 | web2check's AUTO Stop-during-a-run checks (T22/T23/T25) race a fast native run under a CPU throttle (the macOS runner's case): make the run certainly still going at the Stop click, or judge by the stopped event's count | none | done |
| W43 | #86 | The AUTO joins W33c/W33d left (each confined to its files): fort8 via auto_fort_path(8) as a std::string, xAuto/RestartLabel declared in their headers, autlib1's thu as a vector (init()'s signature), scr as a std::string, gogoauto's stale comment and global_conpar_type; left by W33g: `Auto.hinttxt` as a std::string, xpp_util's `ind_to_sym` returning the name (reviewer, after W33f; axes2's and plot_data's copies merged into it); output byte-identical | W33c, W33d | done |
| W44 | #87 | Artifact retention in CI: `retention-days` on build.yml's uploads, 14 for the programs and the sanitizer reports, 30 for `examples-md5-*` (committed as platform baselines); release.yml keeps the default | none | done |
| W45 | #88 | web2: a second periodic AUTO run from the same HB point after a Stop returns zero points at NTST >= 1000 (found by W42; the raw protocol does not show it, so the page's side): a failing web2check first, then the fix; not reproduced (W45 tried the protocol, the page, throttled and not, NTST 700/1000): likely W42's own check timing out on NTST 1000's slower points; W42's heavy.ode run-after-Stop check covers the path | none | closed |
| W46a | #89 | Delete all dead code, not only unreached functions: unused types, structs, fields, macros, header declarations with no definition or in more than one header, #if 0 and commented-out code, set-but-unread variables, unused headers; the audit extended so sourcecheck keeps it at 0 | W33 | done |
| W46b | #90 | read_dir folds into xpp_files: listing, wildcard match and the current folder (a std::string) there; read_dir.cpp/.h deleted; if xpp_files.cpp grows too large, its folder operations move to a second implementation file (xpp_files_dir.cpp) behind the same xpp_files.h (maintainer, 2026-09-27) | W46a | done |
| W46c | #91 | The model's names as std::vector<std::string> in a new xpp::Model (core/model.h: the start of W47b, not std::string globals W47b would move again): uvar_names, upar_names, ufun_names, UFUN_ARG.args, this_file, this_internset; the last text and buffer idioms outside the owner modules | W46a | done |
| W47a | #92 | No global state, stage 1 (maintainer, 2026-09-27: globals avoided): const/constexpr for globals nothing writes, internal linkage for one-file globals, tools/globalcheck counting mutable data symbols against a baseline | W46a, W46c (both edit most declarations) | done |
| W47b | #93 | xpp::Model: everything a load produces, out of globals; a load swaps a new Model in only on success | W46c, W47a | done |
| W47c | #94 | xpp::Session: everything a run changes (data_store, plot windows, integrator, AUTO, browser, kinescope), referring to its Model; left by W47b: the parser's working state (NCON, NSYM, constants, variables), the numerics/plot settings in load_eqn, itor/last_ic/delay_string, simplenet's networks (definition mixed with per-step values), and the Model writes after a load (the browser's added column, table recomputation, boundary conditions: set_bc_formula from a `set` or a .set, pp_shoot recompiling them in place); a load is not yet build-then-swap (the fresh Model is current from the start, a failed load still exits) | W47b | done |
| W47d | #95 | Model&/Session& passed instead of a current-one global; only the session list stays; ThreadSanitizer over the reader threads | W47c | ready |
| W48 | #96 | Retire the C-only text and memory APIs nothing calls since W46c (xpp_strlcpy/strlcat/snprintf and the XPP_* macros, xpp_malloc/calloc/strdup), tabular's raw block to a std::vector if it can be, the deadcode allowlist entries and CLAUDE.md's C-file guidance with them | W46c | done |
| W49 | #97 | autoinfo: after a periodic run from a grabbed HB point the strip's state turns to the HB point after the run's last event; only the next idle sends it (seen once on macos-sanitizers, hidden elsewhere by the 0.1 s throttle): reproduce, fix, make the check deterministic | none | done |
| W50 | #98 | Any number of views of the AUTO diagram (maintainer, 2026-09-27): the one Diagram per session separate from its views, each view its own axes, ranges and variables (I-V beside I-gca), new/close view, grab from any; every view saved and restored with the session (a view is only axis values), in the session file (W57) | W47c | ready |
| W51 | #99 | Solver interface and registry (maintainer, 2026-09-27): xpp::Solver per method (name, traits, start owning its work memory, advance returning one result type in place of the kflag dialects), an explicit registry table (not static-initializer self-registration: --gc-sections links drop it silently); replaces METHOD's switches in integrate.cpp, storage.cpp and do_meth's function pointer; AUTO out of scope; md5s unchanged | W47c | ready |
| W52 | #100 | Data formats as a registry (maintainer, 2026-09-27): xpp::DataFormat (write, read where supported) and a registry table the Save data dialog lists; .dat (unchanged), CSV with a header, CSV.gz, NPZ; compression through a vendored miniz; NPZ is the session file's data; Save data writes the data table or what the plot shows (displayed, frozen and earlier curves as one long table `curve,x,y[,z]`, one array per curve in NPZ) | none (soft overlap W47c: data_store) | done |
| W53 | #101 | Picture export as a registry (maintainer, 2026-09-27): xpp::ImageFormat for PostScript, SVG and the GIFs, a registry table the export menus list; goldencheck byte for byte; no PNG (SVG covers it) | none (soft overlap W47c: plot windows) | done |
| W54 | #102 | Remove the in-program equation editor (maintainer, 2026-09-27): edit_rhs.cpp and the Edit menu (RHS's, Functions, Save as) go, Load DLL goes with W55, the manual's section with them; one of the post-load Model writes W47c lists | none | done |
| W55 | #103 | Drop compiled functions (maintainer, 2026-09-27: risks outweigh benefits): export, dll_lib/dll_fun, Load DLL, extra.cpp's library loading, xpp_dlfcn.h, -DHAVEDLL; a model using them fails to load with a clear error; the 13 examples rewritten in plain .ode or deleted, the prebuilt .so files deleted, the md5 baselines updated; manual chapter 11 removed | none (soft overlap W54: menus) | done |
| W56 | #104 | -silent as a built-in script over the Command API (maintainer, 2026-09-27): its flags and the model's @ batch options become commands run like --script; batch_integrate and the silent_* functions go; command line, output.dat and every written file unchanged (the example md5s) | W47c | ready |
| W57 | #105 | Save/Open session, continue where I stopped (maintainer, 2026-09-27): one name.snapx (registered like .ode), a zip of ordinary files (the .ode's path and fingerprint, .set, .auto, every view, frozen curves, labels, data.npz); a changed .ode keeps what fits by name; a size warning offers to leave the data out; .set/.auto alone unchanged | none | ready |
| W58 | #106 | Checks independent of machine speed (maintainer, 2026-09-27: perf is CI's, not the program's): Stop/Abort checks stop by count and check the result; frame-rate, long-task and Stop-latency checks become printed perf: lines, measured from outside through CDP, the page's measurement-only code removed; --throttle removed; XPP_CHECK_SLOW only scales safety timeouts | none | done |
| W59 | #107 | Record and Play (maintainer, 2026-09-27): the core records every step (idle to idle: I G is one step; keys an array, a dialog's answers and a clicked button inside it) into name.recx, one plain text file with the .ode and every file read, notes as # lines before a step (added while recording, in the player, or in an editor), a fingerprint excluding notes; Play replays faithfully, idle time not recorded, captions, what is pressed highlighted; a mockup approved first | W47c | ready |
| W60 | #108 | Every button sends its key's command (maintainer, 2026-09-27, decision 6): Use this view, 3D turn, Use current state, Import equilibrium and the auto/browser/ani/aplot window buttons send their key sequences instead of their own commands; AUTO Clear reaches the core; keyless buttons (slide, default, action) documented Command API commands; one letter, one command, every key in a menu: the `h` alias and the hidden `y` (a debug stress test) go | none | ready |
| W61 | #109 | Open and Reload in the same process (maintainer, 2026-09-27, decision 1): Open model asks, offers to save, resets and loads here (no second process); Reload keeps the Session's values by name; a failed load leaves the current model untouched | W47c | ready |
| W62 | #110 | No undo (maintainer, 2026-09-27, decision 2): the values panel's Undo goes, Reset one or all to the .ode's values is the way back; the plot and diagram zoom history goes, one Fit returns to the window's own ranges | none | done |
| W63 | #111 | Errors are values (design section 9): the numerics' exit() calls (autlib1/2/4, eispack) become returned errors; computations return errors instead of calling err_msg/the UI; a load's diagnostics (line, column, cause) as an `error` event | none | ready |
| W64 | #112 | Grab an AUTO label by number: `auto grab <label>` for scripts, W56 and W59's replay; the interactive grab unchanged | none | done |
| W65 | #113 | What the page displays lives in the core (maintainer, 2026-09-27, decision 3): earlier runs until Erase, AUTO's hidden branches, the zoom shown; held by the core's windows and sent as data, so the session file saves them | W47c | ready |
| W66 | #114 | The page writes no files (maintainer, 2026-09-27, single source): PNG goes; curves, the table, the kinescope GIF, .par/.ic are written by the core through W52/W53 and the File menu, the page only downloads; AUTO settings live in .auto and the session file | none | in-progress |
| W67 | #115 | Copy as set line (maintainer, 2026-09-27): a dialog names it and shows `set name {...}` with every parameter and initial condition, copied to the clipboard for the user to paste into the .ode; the program never writes the .ode | none | ready |
| W68 | #116 | Commands during a computation are discarded at the source (maintainer, 2026-09-27): the page disables buttons and keys but Esc while busy, sliders and value fields stay usable, their edits waiting for the next computation (W69); the core takes only Stop, Quit, the run's answers and view changes, and drops anything else with a log line; nothing queues | none | done |
| W69 | #117 | Value edits are sent with the next computation, not on each change (maintainer, 2026-09-27): sliders, fields, Default/Reset and loaded .par/.ic stay pending in the page, sent as one set right before Go (or any computing command); Run on change and the rerun flag go | none | done |
| W70 | #118 | Split the expression engine (maintainer, 2026-09-27): parserslow2.cpp into files by job (symbols, compiler, evaluator, built-in functions), its stack and counts into the Model; numerics unchanged | W47c | done |
| W71 | #119 | Every run has its own seed (maintainer, 2026-09-27): the seed set is the next run's (first-run noise and md5s unchanged), the next drawn from a seed stream; each run's seed logged and saved with its data; set it and Go regenerates that run exactly; -newseed logs its seed; the generator's state in the session file | none | done |
| W72 | #120 | Study: bit-identical results on every platform (maintainer, 2026-09-27): -ffp-contract=off and a vendored correctly rounded libm (CORE-MATH); measure the examples matching Linux per platform and the speed cost; the maintainer decides from the report | none | in-progress |
| W73 | #121 | .odex, inventory and spec (maintainer, 2026-09-27): the .ode quirks (negative signs, e, spaces around =, floating-point literals, several names or options on a line, left-to-right grouping, reserved words and letters as names, case-insensitive names (.odex: case-sensitive), name length (no limit: W76), if/then/else (grouping, NaN conditions), a function argument hiding a global name) from the VS Code extension checked against the code; docs/odex.md, .ode without the quirks, approved before code | none | done |
| W74 | #122 | .odex parser and --convert (maintainer, 2026-09-27): both parsers build the same Model, one route after; the converter writes what the .ode parser understood; every example's .odex md5 equals its .ode's | W73, W70 | in-progress |
| W75 | #123 | xppautX --check (maintainer, 2026-09-27): the quirks as warnings with line and column, produced once by xppautX, JSON for the extension, shown at load; a clean .ode is silent; .ode never deprecated | W73, W63 | blocked |
| W76 | #124 | No limit on a name's length (maintainer, 2026-09-27): XPP_NAME_MAX is a rule left from fixed buffers; the refusals, silent cuts and fixed fields (AUTO settings, NAME_IN, dialogs) become std::string with no cut; short_name() for fixed-width displays; a 200-character name tested end to end | none | ready |
| W77 | #125 | The browser's added column without changing the Model (left by W47c): the column is the Session's data, never a model variable; neq stays the model's | none | in-progress |
| W78 | #127 | near(a, b[, tol=]) in .odex (maintainer, 2026-09-28): approximate equality, a-b within tol*max(1, abs a, abs b), tol from `@ neartol=` (default 1e-9) or `tol=` per call; `==` stays exact; not `~=`, `eps` or `atol`; `near` reserved, `neartol` an option name; reported to the extension | W74 | blocked |

## W30 audit: the copies tools/dupcheck.sh found in core/, by the W32 card
that absorbs them (the allowlist inside tools/dupcheck.py has the full
list with a reason per entry; this is the summary).

**W32a xpp_math** (done; core/xpp_math.h lists what stayed apart and why: AUTO's ge(), EISPACK's hqr, the complex divisions, the banded solver) (LU, eigen, FFT/RNG, std helpers): the three LU solves
(core/gear.cpp's LINPACK-style sgefa/sgesl, core/autlib1.cpp's own
Gaussian elimination `ge()`, core/odesol2.cpp's bandfac/bandsol) and the
two eigenvalue routines (core/gear.cpp's `eigen()`, core/autlib1.cpp's
`eig()`) are the same algorithm written three (or two) times differently
enough that dupcheck's text/structure match cannot see it -- found by
hand, not by the tool. What the tool did find: core/gear.cpp's own
imin/Min and amax/Max (the same int/double min and max helper written
twice); core/adj2.cpp's adj_back/h_back and core/histogram.cpp's
four_back/hist_back (four near-identical ring-buffer interpolation
helpers); core/markov.cpp's mean_back/variance_back; core/parserslow2.cpp's
bessel_j/bessel_y and bessi/bessis; core/histogram.cpp's and
core/markov.cpp's own repeated 17-24 line blocks.

**W32b xpp_files** (file ops, print helpers, temp folders): the five
print-helper copies (core/edit_rhs.cpp, core/form_ode.cpp and
core/lunch-new.cpp's `put()`, core/auto_print.h's `xpp::auto_out::print`,
all the same fprintf-to-xpp::format-then-fwrite helper) plus
core/array_print.cpp's ps_bar/ps_hsb_bar, core/auto_nox.cpp's
draw_ps_axes/draw_svg_axes and save_auto_file/load_auto_file,
core/diagram.cpp's post_auto/svg_auto (and its own repeated 16-line PS/SVG
block); core/json_io.cpp's and core/xpp_files.cpp's identical
out_of_memory and b64_value helpers; core/xpp_files.cpp's
xpp_files_put_abort and core/xpp_io.cpp's xpp_writer_abort (both "discard
a temp file, leave the target alone"); the two temp-folder implementations,
core/xpp_util.cpp's and core/xpp_win32.cpp's identical scratch_dir_pid
(POSIX and Windows sides of the same scratch-dir-name parser); the
triplicated FileCloser RAII struct (core/lunch-new.cpp, core/xpp_session.cpp,
core/xpp_io.h's own).

**W32c dialog API** (xpp_ui.h, the seam): by far the largest group, 228
names declared in more than one header. Two headers are dead weight --
core/auto.h and core/aniparse_avi.h are never `#include`d anywhere and
duplicate core/auto_nox.h and core/aniparse.h respectively (delete both
files outright, along with their ANI_COM/Comet/MPEG_SAVE struct copies).
The rest are pre-XppUi-seam declarations still sitting in their old
headers (core/auto_x11.h, core/menudrive.h, core/ggets.h, core/color.h,
core/abort.h, core/calc.h, core/kinescope.h, core/txtread.h,
core/edit_rhs.h, core/graf_par.h, core/main.h, core/menu.h,
core/arrayplot.h, core/pop_list.h, core/many_pops.h, core/init_conds.h,
core/xpp_util.h) alongside xpp_ui.h's own dispatcher declarations of the
same historical names -- drop the old copy, keep xpp_ui.h's. Also here:
core/commands.cpp's help/help_num/help_file; core/json_ani.cpp,
core/json_windows.cpp, core/ui_json.cpp and core/xpp_batch.cpp's small
same-shape dispatch helpers (j_ani_show/j_reset_graphics/script_next/
do_vis_env) and core/json_windows.cpp's j_redraw_screens/j_clear_screens.

**W32d shared data** (HIST_INFO, TABULAR, the data store, the AUTO
diagram list): HIST_INFO (core/histogram.cpp, core/load_eqn.cpp) and
TABULAR (core/simplenet.cpp, core/tabular.cpp) defined twice, each file's
own comment already admitting it reads the other's layout directly.
Also: ACTION (core/form_ode.cpp, core/json_state.cpp), INTERN_SET
(core/integrate.cpp, core/load_eqn.cpp, core/comline.h) and XPPVEC
(core/integrate.cpp, core/storage.cpp) each defined more than once;
core/auto_data.cpp's and core/plot_data.cpp's identical add_str,
core/marks_data.cpp's and core/phase_data.cpp's identical add_num;
core/graf_par.cpp's edit_frz/delete_frz (frozen_curves); core/grobs.cpp's
destroy_grob/destroy_label; core/simplenet.cpp's per-connectivity-type
16-34 line blocks (the same TABULAR-driven code repeated per network type).

**Found by W32d, for W33:** core/auto_f2c.h's `min`/`max` macros break the C++ standard headers, so a header that auto_f2c.h can reach (storage.h) cannot include them yet; W33c (AUTO's numerics) replaces those macros. TABULAR.y stays a raw block while simplenet's networks keep pointers into it.

**Not owned by any W32 card, left as "keep" in the allowlist; the W33 sweep of each file looks at them again, merging or keeping each with its reason** (same-shape
per-variant dispatch that is a design pattern, not a copy to merge, or a
possible coincidental structural match worth a human's second look before
touching): core/aniparse.cpp's per-primitive draw_ani_* wrappers,
core/flags.cpp's one_flag_step_* dispatch, core/graphics.cpp's
point/bead, line/frect, point_abs/bead_abs pairs, core/extra.cpp's
set_dll_library/set_dll_function, core/odesol2.cpp's discrete/euler,
core/auto_nox.cpp's auto_twopar_double/auto_torus, core/integrate.cpp's
range_item/range_item2 and its own repeated block, core/nullcline.cpp's
repeated block, core/arrayplot.cpp's init_my_aplot/edit_aplot,
core/lunch-new.cpp's io_int/io_double and core/xpp_io.cpp's/xpp_io.h's own
intentional API pairs (line vs. token reader, RAII close()/abort()).

**Vendored/numerical, keep** (a translated-Fortran or CVODE routine whose
repeated shape is the algorithm's own): AUTO (Doedel)'s autlib1/2/3/5.cpp
(stub MPI functions, per-branch-type blocks, fnuzae/fnuzbv,
fnhd/fnhb/fnhw/fnsp/fnpe/fnpl/fnpd/fntr/fnbl/fnho, mynode/numnodes),
conpar2.cpp/worker2.cpp's shared time_start/time_end and startup block,
EISPACK's eispack.cpp, CVODE's band.cpp/dense.cpp and
cvband.cpp/cvdense.cpp, and Hairer's dormpri.cpp/dormpri.h
(hinit/hinit5 and their parallel dop853/dopri5 blocks).

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
- Split ui_json.cpp: protocol I/O, prompts, AUTO, browser.
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

## W11: One I/O module
**Goal.** The core reads, writes and formats text through one C++ module
(`core/xpp_io.cpp`, C API), so the classes of bugs that `printf` and its
relatives cause (overflowed fixed buffers, a last line read twice by a
`while(!feof)` loop, unchecked reads, stray output on the protocol's
stdout) cannot come back.
**Scope.**
1. The core never prints to stdout or stderr itself: the direct `printf`,
   `puts`, `putchar` and `fprintf(stdout|stderr)` calls go through
   `xpp_log` (quiet by default, shown in the page's log panel);
   verify.sh fails on a new one.
2. Formatting: `xpp_fmt` (a string) and `xpp_snprintf` (never past the
   buffer, reports a cut) replace `sprintf` and `strcpy` into fixed
   buffers (463 and 354 calls, 2026-09-23).
3. Files: one line reader replaces the `fgets`/`fscanf`/`feof` loops and
   one writer (errors logged, temp-then-rename, closed by RAII) replaces
   `fopen`/`fprintf`/`fclose` sequences, file by file as W7 converts them.
**Done when.** No core file calls `sprintf`, `strcpy` into a fixed
buffer, `fscanf`, or prints directly; the data files written are byte for
byte the same (the lecar checksum, tests/examples.md5, the .set round
trip).

## W12: The manual
**Goal.** XPP's manual (docs/xpp_doc.tex, docs/xpp_sum.tex, docs/help/*.html)
describes xppautX as it is, lives next to the code it describes, and is
one click away in the app.
**Scope.**
1. Convert the LaTeX to Markdown (pandoc), one file per chapter in
   docs/manual/; the .tex, .pdf and help/*.html move to docs/upstream/ as
   the historical reference.
2. Keep the model language, numerics and AUTO chapters (the core); rewrite
   the X11 window and keystroke chapters for web2 (values panel, sliders,
   AUTO view, dialogs, exports, files).
3. web2 gets a Help view (search, table of contents) built from
   docs/manual/ at build time; menu items and dialogs link to their section.
4. A task that changes the UI updates its manual section, as protocol.md.
**Split.** W12a: steps 1, 2 and 4 (docs only). W12b: step 3 (web2),
after T22 so the AUTO chapter describes its settings as they end up.
**Done when.** No chapter describes an X11 window; every menu and dialog in
web2 links to a section that describes it; web2check opens Help from a
dialog and finds its section.
W12c (maintainer, 2026-09-24): chapter 1 is Bard's first-person introduction
to XPP and needs rewriting for xppautX; the other chapters keep his words.

## W13: A desktop app
**Goal.** `xppautX model.ode` (or double-clicking a .ode file) opens a real
application window, not a browser tab with a token URL.
**Scope.**
1. xppautX opens its own window with the OS web view (the `webview` C/C++
   library: WebView2 on Windows, WKWebView on macOS, WebKitGTK on Linux),
   showing web2; the token never appears. Closing the window ends the
   process. Browser mode stays as `--browser` (VS Code extension, remote,
   headless); `--server` and `-silent` are unchanged.
2. The app's name and icon (window, taskbar/dock, executable).
3. A native menu bar for what belongs to the app, not the model: File
   (Open model..., Open recent, Close, Quit), Help (Manual from W12, keyboard
   shortcuts, About: version, commit, compiler, protocol), Check for
   updates (GitHub releases; asks before any download, and only when the
   user chooses it or opts in). The model's own menus stay in web2.
4. .ode files open with xppautX: Windows installer or registry script,
   Linux .desktop file and MIME type, macOS .app bundle with Info.plist.
   A second .ode opens a second window.
5. web2check runs once in the native web view (WebView2) besides Chrome.
**Needs.** The WebView2 SDK header (a download, maintainer's OK) and
libwebkit2gtk-4.1-dev on Linux (installed by the maintainer); an icon.
**Split.** W13a: the window, name, icon slot and menus (steps 1-3 without
updates). W13b: file associations, a window per file, updates (steps 3-5).
The window is optional at build time: without WebKitGTK (Linux) the
build is browser-only, so building xppautX never requires it.
**Done when.** Double-clicking a .ode file on Windows opens its window with
the app's name and icon; Help, About and Check for updates work; closing
the window leaves no process; browser mode passes its checks as before.
