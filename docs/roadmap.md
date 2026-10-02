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
| W34 | #72 | Evaluate SUNDIALS CVODE in place of the vendored CVODE (later; changes every CVODE result; 2026-09-30: after the W109 stages, alone, as an evaluation: SUNDIALS vs the vendored CVODE on the CVODE example models, accuracy, speed and which outputs change, for the maintainer to decide; W109b leaves the vendored CVODE headers to it) | W33a | later |
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
| W47d | #95 | Model&/Session& passed instead of a current-one global; only the session list stays (about 2500 call sites, 2026-09-30: in stages, the first stage writes the staging plan here and does one coherent part); ThreadSanitizer split out as W47e; the stages are W47d1-W47d6 below, the plan in "W47d: Model and Session passed, in stages" at the end | W47c | done |
| W47d1 | #95 | Stage 1, the command layer and the front end: a Session knows its Model (`s.model()`); ui_json.cpp's handle_line chooses the command's Session once and passes it to every command (the command table's `run(Session&, line)`), commands.cpp (`commander`, `run_the_commands`, the menus), json_*.cpp, model_switch.cpp (load_requested returns the new Session), xpp_session.cpp, autox_io.cpp, xppautx_main.cpp; the XppUi j_ callbacks, asks and checkpoints read the current one once (entry points until W47d6). Proof: those files' xpp::session()/xpp::model() uses 285 to 50, all entry points; md5s, goldencheck, servercheck, autocheck | W47c | done |
| W47d2 | #95 | Stage 2, what the commands reach: the data modules and the browser (plot_data, phase_data, marks_data, ani_data, auto_data, csv_export, browse_data, numerics_settings, userbut, xpp_ui, xpp_util, histogram, tabular, diagram, derived, lunch-new: 442 uses) and the drawing and picture export (graphics, axes2, grobs, my_ps, my_svg, graf_par, arrayplot, aniparse, nullcline: 470); two commits by group if it helps. Their functions take Session& (or the member they use: a GRAPH&, the DataStore&) from the commands and handle_line's `*_update(s)`; the data modules' subscriptions and caches stay file-local. Proof: those files' xpp::session()/xpp::model() uses 912 to 56, all entry points (functions called from W47d3-5's files or XppUi's defaults, read once at their top); goldencheck byte for byte, md5s, servercheck, autocheck | W47d1 | done |
| W47d3 | #95 | Stage 3, the parser and the load (load_eqn, form_ode, ode_read, expr_symbols, expr_compile, expr_functions, xpp_batch, comline, odex_convert, odex_load: 379 uses): xpp::Load hands its fresh Model and Session to the readers (`load.model()`, `load.session()`), which take Model&/Session& instead of reading the current ones; the model's post-load writes (xpp_batch) likewise. Proof: the counts; test_load and test_names, the md5s, servercheck's load and failed-load checks, goldencheck. Done: those files' uses 381 to 14, all entry points (the lookups, add_expr and add_net_name the integrator and AUTO call, the evaluator's shift/delay built-ins, xpp_batch_start, --convert after its loads); the parser's helpers in stage 2's files (tables, derived quantities, -parfile/-icfile, the array plot's defaults) take the Session too | W47d2 | done |
| W47d4 | #95 | Stage 4, the integrator and the right-hand side, the hot path (integrate, numerics, storage, torus, pp_shoot, do_fit, markov, adj2, gear: 475; my_rhs, expr_eval, simplenet, flags, delay_handle, del_stab, dae_fun, volterra2, odesol2, stiff, cv2, dormpri: 385), two commits (driver, then the right-hand side). A run takes the Session once where it starts (the command, or the Solver's start, W51) and keeps it for its steps: the solvers' fixed `rhs(t, y, ydot, neq)` reaches the Model and Session through the Solver object or IntegratorState, never per step through a global; nothing new per step. Proof: md5s unchanged, examples_check's and a heavy model's run time unchanged (measured before and after, as a note, not a gate), odexcheck, unit tests | W47d3 | done |
| W47d5 | #95 | Stage 5, AUTO (auto_nox, auto_settings, autevd, autpp, gogoauto, autlib1-5, setubv2, auto_stop: 547 uses): the AUTO window's functions take Session& from the commands (auto_key has it since W47d1); a run's Session goes into the AUTO library's own state (AutoLib), from which the translated routines, whose callback signatures (funi, stpnt, bcni, icni, fopi, pvli) are fixed, reach it; no signature of theirs changes. Proof: autocheck, the saved AUTO diagram in verify.sh, servercheck's AUTO sections, test_auto_*. Done: those files' uses 564 to 0 (with diagram.cpp's 14, the diagram's accessors taking the AutoDiagram); a run's iap_type points at its AutoLib (`iap->lib`), which holds the run's Session from go_go_auto(s) to its end, so the translated routines reach both by a member read; the model's own AUTO routines (autpp.cpp's func, stpnt, bcnd) take the Session from their callers, the function-pointer types unchanged; the pivot and note printers without an iap (ge, nlvc, brbd, dimrge, print1, flowkm) take fort.9, skip3 fort.3; the settings event is a ChangedEvent of the Session; autox_io's File menu, get_scale/set_scale take it too, get_param_index(name) is gone | W47d4 | done |
| W47d6 | #95 | Stage 6, the seam and the end: the XppUi callbacks that need a Session get it (a parameter, or the front end's session per client), the entry points W47d1-5 left (json_windows' j_ functions, asks, checkpoints, flush_pending) with them; xpp::session() and xpp::model() go: the session list (one per client; the reader threads' inbox behind its locks, W47e) is the only global; a source check fails a new current-one read outside its owner. Proof: 0 uses; globalcheck; every check of the per-task tier and W47e's tsancheck. Done: xpp::session(), xpp::model() and xpp_current.h are gone (69 uses to 0, and about 40 more hidden behind the no-Session wrappers: find_variable, find_user_name, get_val/set_val, set_ivar/get_ivar, add_expr, evaluate, do_calc, evaluate_derived, redo_all_fun_tables, browse_column_name, ind_to_sym, the model's file readers); 41 XppUi fields and their dispatchers take the caller's Session& (the windows, the kinescope, the array plot, the AUTO window, the animator, the mouse, the recorder and player, new_float); the session list is session.cpp's `client_session()`, read by handle_line and xppautx_main.cpp (start, exit) and, for the asks, checkpoints, flush_pending and the state flush, by the front end's `xpp::json::client()`; `xpp::load_model` returns the Session it loaded (`xpp::Loaded`), which the start, -silent, --convert and File > Open model use; tools/sessioncheck.sh (sourcecheck) counts each owner's reads | W47d5 | done |
| W47e | #158 | ThreadSanitizer (split from W47d, 2026-09-30): make tsan with clang, tools/tsancheck.sh runs servercheck and webcheck under it, fails on any report; every race in our code fixed | none | done |
| W48 | #96 | Retire the C-only text and memory APIs nothing calls since W46c (xpp_strlcpy/strlcat/snprintf and the XPP_* macros, xpp_malloc/calloc/strdup), tabular's raw block to a std::vector if it can be, the deadcode allowlist entries and CLAUDE.md's C-file guidance with them | W46c | done |
| W49 | #97 | autoinfo: after a periodic run from a grabbed HB point the strip's state turns to the HB point after the run's last event; only the next idle sends it (seen once on macos-sanitizers, hidden elsewhere by the 0.1 s throttle): reproduce, fix, make the check deterministic | none | done |
| W50 | #98 | Any number of views of the AUTO diagram (maintainer, 2026-09-27): the one Diagram per session separate from its views, each view its own axes, ranges and variables (I-V beside I-gca), new/close view, grab from any; every view saved and restored with the session (a view is only axis values), in the session file (W57) | W47c | done |
| W51 | #99 | Solver interface and registry (maintainer, 2026-09-27): xpp::Solver per method (name, traits, start owning its work memory, advance returning one result type in place of the kflag dialects), an explicit registry table (not static-initializer self-registration: --gc-sections links drop it silently); replaces METHOD's switches in integrate.cpp, storage.cpp and do_meth's function pointer; AUTO out of scope; md5s unchanged | W47c | done |
| W52 | #100 | Data formats as a registry (maintainer, 2026-09-27): xpp::DataFormat (write, read where supported) and a registry table the Save data dialog lists; .dat (unchanged), CSV with a header, CSV.gz, NPZ; compression through a vendored miniz; NPZ is the session file's data; Save data writes the data table or what the plot shows (displayed, frozen and earlier curves as one long table `curve,x,y[,z]`, one array per curve in NPZ) | none (soft overlap W47c: data_store) | done |
| W53 | #101 | Picture export as a registry (maintainer, 2026-09-27): xpp::ImageFormat for PostScript, SVG and the GIFs, a registry table the export menus list; goldencheck byte for byte; no PNG (SVG covers it) | none (soft overlap W47c: plot windows) | done |
| W54 | #102 | Remove the in-program equation editor (maintainer, 2026-09-27): edit_rhs.cpp and the Edit menu (RHS's, Functions, Save as) go, Load DLL goes with W55, the manual's section with them; one of the post-load Model writes W47c lists | none | done |
| W55 | #103 | Drop compiled functions (maintainer, 2026-09-27: risks outweigh benefits): export, dll_lib/dll_fun, Load DLL, extra.cpp's library loading, xpp_dlfcn.h, -DHAVEDLL; a model using them fails to load with a clear error; the 13 examples rewritten in plain .ode or deleted, the prebuilt .so files deleted, the md5 baselines updated; manual chapter 11 removed | none (soft overlap W54: menus) | done |
| W56 | #104 | -silent as a built-in script over the Command API (maintainer, 2026-09-27): its flags and the model's @ batch options become commands run like --script; batch_integrate and the silent_* functions go; command line, output.dat and every written file unchanged (the example md5s) | W47c | done |
| W57 | #105 | Save/Open session, continue where I stopped (maintainer, 2026-09-27): one name.snapx (registered like .ode), a zip of ordinary files (the .ode's path and fingerprint, .set, .auto, every view, frozen curves, labels, data.npz); a changed .ode keeps what fits by name; a size warning offers to leave the data out; .set/.auto alone unchanged | none | done |
| W58 | #106 | Checks independent of machine speed (maintainer, 2026-09-27: perf is CI's, not the program's): Stop/Abort checks stop by count and check the result; frame-rate, long-task and Stop-latency checks become printed perf: lines, measured from outside through CDP, the page's measurement-only code removed; --throttle removed; XPP_CHECK_SLOW only scales safety timeouts | none | done |
| W59 | #107 | Record and Play (maintainer, 2026-09-27): the core records every step (idle to idle: I G is one step; keys an array, a dialog's answers and a clicked button inside it) into name.recx, one plain text file with the .ode and every file read, notes as # lines before a step (added while recording, in the player, or in an editor), a fingerprint excluding notes; Play replays faithfully, idle time not recorded, captions, what is pressed highlighted; a mockup approved first (mockup docs/mockups/record-play.html; 2026-09-30: liked; the note field wraps; the note as a large caption above the plot; the keys in a box at the top of the plot; progress one segment per step; approved) | W47c | done |
| W59a | #107 | Record and Play, stage 1, the core records (2026-09-30, W59 split in three after the mockup's approval): File > Record starts and stops; every step idle to idle (its keys as an array, a dialog's answers, a clicked button, an Abort's row/point) goes to name.recx: a header, the .ode and every text file the session read as sections, the steps as JSON lines with the step label from the menu data, notes as # lines before a step (a protocol command sets the note for the next step), a fingerprint of files and steps (not notes) at the end; idle time not recorded. Protocol and docs; servercheck/autocheck read the file back | W47c | done |
| W59b | #107 | Record and Play, stage 2, the player: the core replays a .recx step by step (play, pause, step, speed; view-only steps quick, never skipped; a changed fingerprint warns and still plays), sending what each step pressed before it runs; web2's player as the approved mockup (docs/mockups/record-play.html: the note as a large caption above the plot, the keys in a box at the top of the plot with the menu item lit, progress one segment per step, the step list with note editing and Save, the changed-file banner); left by W59a: Quit or Open model while recording asks to save it first (as for the session), keys a running job reads itself (/ during a range, Escape during the animation) recorded, an .autox/.snapx loaded while recording embedded) | W59a | done |
| W59c | #107 | Record and Play, stage 3: .recx registered with the OS like .ode (W13's association code, associatecheck), opening in the player | W59b | done |
| W59d | #159 | Record from a snapshot, and one question to leave a session (maintainer, 2026-09-30): a recording carries the session's state at Record (a .snapx without the data table) and Play starts from it; every quit (F Q, the menu bar's Quit, the window's x) and Open model/Reload ask "Save this session first?" (Save session S, Don't save D, Cancel Esc), a recording in progress saved with it; a quit during a computation stops it, then asks; --server/-silent/--script quit without asking | W59c | done |
| W60 | #108 | Every button sends its key's command (maintainer, 2026-09-27, decision 6): Use this view, 3D turn, Use current state, Import equilibrium and the auto/browser/ani/aplot window buttons send their key sequences instead of their own commands; AUTO Clear reaches the core; keyless buttons (slide, default, action) documented Command API commands; one letter, one command, every key in a menu: the `h` alias and the hidden `y` (a debug stress test) go | none | done |
| W61 | #109 | Open and Reload in the same process (maintainer, 2026-09-27, decision 1): Open model asks, offers to save, resets and loads here (no second process); Reload keeps the Session's values by name; a failed load leaves the current model untouched | W47c | done |
| W62 | #110 | No undo (maintainer, 2026-09-27, decision 2): the values panel's Undo goes, Reset one or all to the .ode's values is the way back; the plot and diagram zoom history goes, one Fit returns to the window's own ranges | none | done |
| W63 | #111 | Errors are values (design section 9), split as the issue asks (AUTO's exits first, 2026-09-28): W63a, W63b, W63c, W63d | W63a-d | done |
| W63a | #111 | The numerics' exit() calls (autlib1/2/4, eispack) become errors returned up to the command, which reports it and leaves the program running; md5s and AUTO output unchanged | none | done |
| W63b | #111 | Computations return an error value (what failed, where) instead of calling err_msg or the UI (err_msg in 36 core files; integrate.cpp's film_clip/put_text); the command layer turns it into the message (landed 2026-09-30: xpp_error.h, integration and solvers, equilibria, delays, DAEs, shooting, storage, the PS/SVG/CSV writers; the rest is W63d) | W63a | done |
| W63c | #111 | A load's diagnostics as values (line, column, cause), sent as an `error` event the page shows next to the line; after W79's readers | W79 | done |
| W63d | #157 | Errors as values for the rest after W63b: fitting (do_fit, with one_step_int), tables, histogram/Fourier, adjoints/Liapunov, AUTO's fort.8 opens, aniparse's loader, auto_nox's mixed functions, the range sweeps' bottom_msg progress | W63b | done |
| W64 | #112 | Grab an AUTO label by number: `auto grab <label>` for scripts, W56 and W59's replay; the interactive grab unchanged | none | done |
| W65 | #113 | What the page displays lives in the core (maintainer, 2026-09-27, decision 3): earlier runs until Erase, AUTO's hidden branches, the zoom shown; held by the core's windows and sent as data, so the session file saves them | W47c | done |
| W66 | #114 | The page writes no files (maintainer, 2026-09-27, single source): PNG goes; curves, the table, the kinescope GIF, .par/.ic are written by the core through W52/W53 and the File menu, the page only downloads; AUTO settings live in .auto and the session file | none | done |
| W67 | #115 | Copy as set line (maintainer, 2026-09-27): a dialog names it and shows `set name {...}` with every parameter and initial condition, copied to the clipboard for the user to paste into the .ode; the program never writes the .ode | none | done |
| W68 | #116 | Commands during a computation are discarded at the source (maintainer, 2026-09-27): the page disables buttons and keys but Esc while busy, sliders and value fields stay usable, their edits waiting for the next computation (W69); the core takes only Stop, Quit, the run's answers and view changes, and drops anything else with a log line; nothing queues | none | done |
| W69 | #117 | Value edits are sent with the next computation, not on each change (maintainer, 2026-09-27): sliders, fields, Default/Reset and loaded .par/.ic stay pending in the page, sent as one set right before Go (or any computing command); Run on change and the rerun flag go | none | done |
| W70 | #118 | Split the expression engine (maintainer, 2026-09-27): parserslow2.cpp into files by job (symbols, compiler, evaluator, built-in functions), its stack and counts into the Model; numerics unchanged | W47c | done |
| W71 | #119 | Every run has its own seed (maintainer, 2026-09-27): the seed set is the next run's (first-run noise and md5s unchanged), the next drawn from a seed stream; each run's seed logged and saved with its data; set it and Go regenerates that run exactly; -newseed logs its seed; the generator's state in the session file | none | done |
| W72 | #120 | Study: bit-identical results on every platform (maintainer, 2026-09-27): -ffp-contract=off and a vendored correctly rounded libm (CORE-MATH); measure the examples matching Linux per platform and the speed cost; the maintainer decides from the report | none | done (report merged; adoption deferred) |
| W73 | #121 | .odex, inventory and spec (maintainer, 2026-09-27): the .ode quirks (negative signs, e, spaces around =, floating-point literals, several names or options on a line, left-to-right grouping, reserved words and letters as names, case-insensitive names (.odex: case-sensitive), name length (no limit: W76), if/then/else (grouping, NaN conditions), a function argument hiding a global name) from the VS Code extension checked against the code; docs/odex.md, .ode without the quirks, approved before code | none | done |
| W74 | #122 | .odex parser and --convert (maintainer, 2026-09-27): both parsers build the same Model, one route after; the converter writes what the .ode parser understood; every example's .odex md5 equals its .ode's | W73, W70 | done |
| W75 | #123 | xppautX --check (maintainer, 2026-09-27): the quirks as warnings with line and column, produced once by xppautX, JSON for the extension, shown at load; a clean .ode is silent; .ode never deprecated | W73, W63c | blocked |
| W76 | #124 | No limit on a name's length (maintainer, 2026-09-27): XPP_NAME_MAX is a rule left from fixed buffers; the refusals, silent cuts and fixed fields (AUTO settings, NAME_IN, dialogs) become std::string with no cut; short_name() for fixed-width displays; a 200-character name tested end to end | none | done |
| W77 | #125 | The browser's added column without changing the Model (left by W47c): the column is the Session's data, never a model variable; neq stays the model's | none | done |
| W78 | #127 | near(a, b[, tol=]) in .odex (maintainer, 2026-09-28): approximate equality, a-b within tol*max(1, abs a, abs b), tol from `@ neartol=` (default 1e-9) or `tol=` per call; `==` stays exact; not `~=`, `eps` or `atol`; `near` reserved, `neartol` an option name; reported to the extension | W74 | done |
| W79 | #128 | Two readers and one Model builder (maintainer, 2026-09-28): the .ode reader (quirks kept) and the .odex reader each produce W74's statement list, one builder makes the Model from it; W74's translation to .ode lines goes, with its event `==` rewrite and the IEEE division flag in shared code; every md5 unchanged as .ode and .odex | W74, W76 | done |
| W80 | #129 | .odex arrays, const and derived quantities (maintainer, 2026-09-28): `x[j]' = ... for j in a..b by k` on each statement, explicit index, both ends included, const bounds, no blocks or logic, --convert writes arrays; `const n = 20` (.ode's number); no `!`, the loader makes a parameter-only `d = expr` a derived quantity (same numbers, AUTO included) | W79 | done |
| W81 | #130 | Audit the manual against the program (maintainer, 2026-09-28): every menu item, key, option and model block the manual names checked against the program; stale parts fixed or removed (chapter 15's `-m` C files, PNG export after W66); a script that loads the manual's model blocks keeps it honest | none (soft overlap W66: PNG mentions) | done |
| W82 | #131 | Live integration plotting less smooth than before (maintainer, 2026-09-28): a perf line for a live run's frame gaps (measured, never failed), bisect 8539ca9..master, fix the cause; CI perf: 10^6 time plot draw 202 to 320 ms on Linux | none | done |
| W83 | #132 | The progress bar changes the layout (maintainer, 2026-09-28): progress in the status bar beside Ready, on the right, in space the bar always keeps; the status bar and plot keep their size idle, running and after | none | done |
| W84 | #133 | Faster review gates (maintainer, 2026-09-28): one verify.sh at a time (it takes a flock and a second run waits); the reviewer verifies a wave's merged tip once instead of every branch after each rebase; a web2-only task runs `verify.sh --no-source-checks`; verify.sh from a WSL-native copy timed against /mnt/c (22.6 min cold, 25.3 warm) and there (about 6.5 min cold), so adopted: `tools/wslrun.sh` | none | done |
| W85 | #134 | The plot's axes before a run are the model's (maintainer, 2026-09-28): with no data, uPlot padded and rounded the window (-110..60 read as -130..80); the chart's scales take the range as given | none | done |
| W86 | #135 | Progress bar at the right end of the status bar (maintainer, 2026-09-28; W83 put it after Ready): the fixed slot moves to the bar's right end, nothing moves as it shows or hides | none | done |
| W87 | #136 | Smaller release binaries (maintainer, 2026-09-28): the stripped CI binaries grew 10-15% with W74 (Windows 1.80 -> 2.09 MB zipped); measure -Os, -flto and --gc-sections (size, and speed on heavy/million -silent and the examples) and adopt the best that costs no speed and no numerics (every platform's examples md5); local and test builds keep -g | none | done |
| W88 | #137 | Native file dialogs in the desktop window (maintainer, 2026-09-28: the browse dialog looks strange, why not the OS one): a `file` ask in the window opens the OS dialog (IFileOpenDialog/IFileSaveDialog, GTK FileChooser, NSOpenPanel) with true paths and no copy; browser mode is W90 (decided 2026-09-29) | W61 | done |
| W89 | #138 | Installable release files beside the archives (maintainer, 2026-09-28: bare files for an easier install): the bare stripped .exe, a .deb (system-wide desktop entry, icons, .ode MIME type; Recommends WebKitGTK 4.1) and a .dmg with xppautX.app (Info.plist.in, unsigned), each built and checked in release.yml | none | done |
| W90 | #139 | Browser mode's file dialog is the browser's picker only (maintainer, 2026-09-29: "picker only with extension filter as long as the program will work as expected"): opened directly, filtered by the ask's pattern, several files at once, no tabs; the copy into the model's folder stays (a page never learns a path); every read/write ask, Cancel, the Replace confirm, Firefox/Safari keep working | none | done |
| W91 | #140 | macOS: double-clicking a .ode opens it in xppautX.app (maintainer, 2026-09-29): handle Finder's open-documents event in the macOS window code; at launch it is the model, while running it opens through W61's Open; no document shows W88's Open dialog | W61, W88 | done |
| W92 | #141 | .autox, the AUTO window's own file (maintainer, 2026-09-29): save and load only AUTO's work without a whole session. One name.autox holds the diagram at full precision (every point's values, label, type, stability), AUTO's settings (numerics, parameter and axes choices) and the .ode's path and fingerprint; the AUTO window's Save/Load diagram write and read it; loading a .auto still works, converted on read (saving it gives .autox); .snapx carries the same .autox in place of .auto (one reader and writer); nothing writes .auto any more, no export (maintainer, 2026-09-29: no backward compatibility with XPPAUT is sought; reading its files is import only) | W57 | done |
| W93 | #142 | CI's UI checks fail on Linux and macOS, not Windows (coordinator, 2026-09-29): linux-ui's "runs: a third run (Go) keeps both earlier ones" (new at 7594af1: W88/W80/W56/W57) and macos-ui's files section "no [data-file-input=open]" (since 7004796); web2check now prints what a thrown attempt recorded; find each cause and fix it | none | done |
| W94 | #143 | CI checks still FLAKY after W93 (coordinator, 2026-09-29): macos-ui's AUTO Stop race (File/Auto does not open the AUTO view on the first attempt) and kinescope playback (a poll can miss frame 0 on a slow runner); both now print what they saw; fix each so its outcome does not depend on the runner's speed (W58) | none | folded into W95 (#144) |
| W95 | #144 | Actions enabled by kind, one table in the core (maintainer, 2026-09-29): every menu item (menus.cpp XppMenu) and every keyless command (one table, the Command API's list) has a kind -- control (Abort, Quit), view (zoom, pan, legend, tabs, Help, panels), data (Save/Load values, Write/Read set, file writes, Save session), computation (Go, Range, AUTO Run, Nullclines, Dir.field, Equilibria, Stochastic); the core sends them in hello; while a computation runs only control and view work, data and computation are disabled (no doubt which data is saved), menus open with those items disabled; the page's own catch-up commands (held display, tab pick, AUTO settings) disable nothing, so no click is lost after an idle; value edits during a run stay pending (#117); replaces web2's disabled={busy} and takenWhileBusy; hello's tables replace web2/src/protocol/windowKeys.ts (the windows' keys copied from menus.cpp); AUTO's keyless commands (auto set from the page's Parameter/Numerics/Mark values forms; auto display, grab, point, close) are in the keyless table; W94's flaky checks fold in | W96 | done |
| W96 | #145 | AUTO's menus as XppMenu data in menus.cpp (maintainer, 2026-09-29): the 10 menus auto_nox.cpp builds with auto_pop_up_list (Start, Hopf Pt, Periodic, Branch Pt, Per. Doub., Torus, Plot Type, Mark values ...) move to menus.cpp like every other menu, so W95 has one table; no behaviour change (the menus the page shows, autocheck, servercheck, the md5s), a servercheck case checks each AUTO menu against menus.cpp | none | done |
| W97 | #146 | AUTO Start/Periodic, Bdry Value and homoclinic starts with nothing integrated read outside the data (coordinator, 2026-09-29: asancheck on the W92/W96 tip, get_start_period's col[0][rows-1] with rows 0); they refuse with "Integrate first"; autocheck autox checks it, and its continuation check grabs a label the diagram has | none | done |
| W98 | #147 | The plot disappears at some window widths, and Show AUTO covers the message strip (maintainer, 2026-09-29, screenshots): every width shows the plot, nothing covers a message; web2check reads the layout's state at several widths | none | done |
| W99 | #148 | Boundary conditions listed for every variable though the model has no BVP (maintainer, 2026-09-29: amari.ode lists 200), always open: show the section only when the model defines boundary conditions, collapsed at first | none | done |
| W100 | #149 | F lost around the AUTO window (maintainer, 2026-09-29): after grab Hopf + Periodic, AUTO's F does nothing and a continuation seems to run on; after Back, the main window's F only focuses; find and fix each, web2check both sequences | none | done |
| W101 | #150 | "Not a valid set" when the Param set menu is cancelled (maintainer, 2026-09-29, seen while saving): a cancel says nothing; find how a save reached the Param set menu | none | done |
| W102 | #151 | A reopened .snapx with a diagram shows an empty main plot after Back (maintainer, 2026-09-29, desktop window), though the data table is in the file: the core restores and sends the rows (autocheck sessiondata, 2026-09-29, fresh and same server, with and without AUTO), and browser mode draws them; not reproduced in the desktop window's File/Open from AUTO's view, so the cause is likely web2's layout or draw path: recheck after W95 and W98 | none | ready |
| W103 | #152 | .autox and .snapx carry their model and open only it (maintainer, 2026-09-29): both always save the .ode and every file it read (tables, .ani, included files); opening one (File > Open model, a double-click, the command line) loads that saved version, then the diagram into AUTO (and, for .snapx, the session); nothing written beside it, outputs go to the file's own folder, the title names the saved model; when the model already open is identical to the saved one only the diagram (session) loads, else it switches as File > Open does (asking to save first). No fallbacks (maintainer, 2026-09-29): a file with no saved model (every one saved before this card) is refused with an error saying so, nothing loaded; the .snapx model.auto import and session load's older .set+.auto pair are removed; any other non-model file opened as a model is refused clearly, no bytes printed; AUTO's Load diagram of an XPPAUT .auto (an import, W92) stays. Files saved before this card are not converted (maintainer, 2026-09-29: nothing released yet, no work to keep) and get no code of their own ("let's make the program clear"): no detection of an older file, no special message, no conversion, no version branch; the reader requires the model member like every other one, so such a file fails its ordinary check (a member missing) with that error | none | done |
| W104 | #153 | An error that stops an action as a centred dialog with OK (maintainer, 2026-09-29: "the user needs to know what went wrong"; go easy with the dialog): an error message (err_msg: the action the user asked for did not happen) opens a centred dialog with OK and is kept in Messages; several at once are one dialog listing them; warnings and progress stay in the status bar, a new warning flashing it briefly; page-only (the core already marks errors); web2check reads the dialog's state | none | done |
| W105 | #154 | The checks leak disk (coordinator, 2026-09-29, after the maintainer's full disk and restarts): wslrun.sh keeps a WSL clone per worktree forever (34 GB), cdp.mjs leaves a Chrome profile per run in %TEMP% (4.5 GB); wslrun.sh prunes the clones of removed worktrees, cdp.mjs deletes its profile after Chrome exits and sweeps stale ones | none | done |
| W106 | #155 | A fifth kind, setting (maintainer, 2026-09-29: "numerics is better to be settings"; "they will be used in the next run"): parameters, initial conditions, boundary conditions, delays, the numerics and AUTO's Parameter/Numerics/Mark values forms; an edit during a computation is not held (maintainer, 2026-09-29: "edits don't wait; they are applied and recorded, but they have no effect on the current run"): the page sends it at once and shows it as the current value (no "pending" banner, #117's holding removed); the core records it at once (state, a session or set saved later, a recording) and the running computation keeps the values it started with, the new ones applied when it ends, before any other command; a Numerics section in the Values panel (Total, dt, method, tolerances ...) editable during a run like parameters; the Numerics menu stays for the keyboard, its dialog opening when a run ends; W95's kinds table, hello, docs/protocol.md "Action kinds" and the manual updated. AUTO's Grab stays data (maintainer, 2026-09-29) | none | done |
| W107 | #156 | About with the author's name and contact (maintainer, 2026-09-29): Help > About shows the author as Muhammad Ahmad (the name the maintainer publishes under), email muhammadmoustafa22@gmail.com, GitHub https://github.com/MuhammadMoustafa, LinkedIn https://www.linkedin.com/in/muhammad-ahmad-62743a125/ (no website while it is outdated), a link to the issue tracker to report a problem, beside what it shows now (version, commit, compiler, protocol, the XPPAUT credit to Bard Ermentrout, GPL v2); one text in the core, shown by the desktop window and, in browser mode, by the page's Help | none | done |
| W108 | #160 | Browser mode ends 2 s after its tab closes (maintainer, 2026-09-30; about 12 s today, which looked like never): the page says it is leaving (pagehide beacon) and the core waits about 2 s for a reload; the heartbeat stays for a page gone without a word; the browser's own "Leave site?" before closing | none | done |
| W109 | #161 | The core's API in C++ (maintainer, 2026-09-30): extern "C" kept only where C calls across (the window plugin's table, callbacks to C libraries), the rest C++ linkage in namespace xpp with C++ types at the boundary; CLAUDE.md's C and C++ section to the new rule; a source check on new extern "C"; in stages by module; speed measured before and after each stage (examples_check wall time, kuramot100.ode -silent, an AUTO run; 3 runs each, nothing else running): a stage that is slower beyond the noise is not merged, and if C++ costs speed the card stops (maintainer, 2026-09-30); the stages are W109a-W109f below, the plan in "W109: the core's API in C++, in stages" at the end | W47d6 | done |
| W109a | #161 | Stage 1, the owner modules (xpp_io, xpp_files, xpp_log, xpp_math, xpp_mem) and their callers, with the staging plan, tools/externcheck.sh and CLAUDE.md's new rule: no extern "C" in their headers, their API in namespace xpp (`xpp::files::exists`, `xpp::log_printf`, `xpp::out_of_memory`, `xpp::fft`, ...) with std::string_view/std::span/references at the boundary; xpp_io's C API under the handles (`xpp_line_reader_*`, `xpp_token_reader_*`, `xpp_writer_*`) retired into LineReader/TokenReader/Writer. Done: headers with extern "C" 78 to 74 (of 125); externcheck allows 127 lines; speed: instruction counts the same (callgrind: kuramot100 shortened 7,115,206,679 vs 7,115,208,960; the AUTO script 23,001,123,785 vs 22,994,163,044), wall time decided by code layout (min/median s, master vs W109a, -g -O2: examples_check 19.08/19.21 vs 19.79/19.89, kuramot100 9.35/9.37 vs 10.18/10.20, AUTO 3.90/3.96 vs 3.95/4.00; with -falign-functions=64 W109a is the faster: "W109: the core's API in C++, in stages") | W47d6 | done |
| W109b | #161 | Stage 2, the integrator, the solvers and CVODE, the hot path (adj2, band, cv2, cvband, cvdense, cvode, dae_fun, del_stab, delay_handle, dense, do_fit, gear, histogram, integrate, llnlmath, markov, numerics, numerics_settings, odesol2, pp_shoot, stiff, storage, vector, volterra2: 24 headers): C++ linkage in namespace xpp (CVODE's in its own), std::span for a pointer and a length where a caller has one, the solvers' fixed rhs signature and the step loops unchanged; timed as the plan says, instruction counts too. Done: the 17 headers that are xppautX's own in namespace xpp (their defining files wrapped in it, the callers qualified), std::span for polint, norm_vec and histogram.cpp's spectral routines; the vendored CVODE's 7 (band, cvband, cvdense, cvode, dense, llnlmath, vector) left for W34 (#72) (maintainer, 2026-09-30); headers with extern "C" 74 to 57, externcheck 127 lines to 110; callgrind: kuramot100 at total=10 7,115,200,486 vs 7,115,208,383, the AUTO script 22,994,372,510 vs 22,994,405,848 | W109a | done |
| W109c | #161 | Stage 3, the parser and the load (comline, derived, expr, form_ode, load_eqn, lunch-new, ode_read, simplenet, tabular, xpp_batch, xpp_util: 11 headers): namespace xpp, std::string_view for the names and lines they read; expr.h's evaluator entry points keep their shape (the right-hand side calls them every step). Done: the 11 headers in namespace xpp, no guard (the 13 files defining them wrapped in it, xpp_util.cpp's three graf_par.h functions left outside; about 300 call sites qualified in 21 files and 7 tests); the prefixed names lose the prefix (xpp_model_failed is xpp::model_failed(), xpp_batch_start batch_start, xpp_reset_options reset_options, xpp_cleanup_auto_dir/xpp_renew_auto_dir cleanup_auto_dir/renew_auto_dir); std::string_view for the text about 45 functions only read (the symbol table's add_*, add_expr, do_num, set_option, msc, the internal sets', the tables', calculate/do_calc/to_float, parse_it, find_char, subsk, ...), the .c_str() of about 80 calls dropped; convert, dead once do_intern_set used converted, removed; eval_fun_table keeps a const std::string & (AUTO redoes the tables at every right-hand side, and a string_view cost redo_all_fun_tables 2 instructions a call, +627 ppm on the AUTO script); the using-declarations of odex:: and expr:: names stay outside namespace xpp (aliascheck); headers with extern "C" 57 to 46, externcheck 110 to 99 left; callgrind (master 35b9f33 vs this stage, WSL gcc 15 -g -O2): kuramot100 at total=10 7,115,204,064 vs 7,115,217,928 (+1.9 ppm, the load's parsing), the AUTO script 22,994,366,310 vs 22,994,156,167 (-9 ppm) | W109b | done |
| W109d | #161 | Stage 4, AUTO (autevd, auto_c, auto_data, auto_f2c, auto_nox, auto_settings, auto_stability, auto_stop: 8 headers, and the extern "C" definitions in auto_data, auto_settings, auto_stability, auto_stop and autpp.cpp): C++ linkage for the translated routines and their callback types (funi, stpnt, bcni, icni, fopi, pvli: their signatures stay), namespace xpp for the front end's. Done: auto_c.h and auto_f2c.h (the translated routines, f2c's helpers, the model's problem functions icnd/fopt/pvls and the callback macros, signatures unchanged) C++ linkage in the global namespace, no guard; the front end's six (autevd, auto_data, auto_nox, auto_settings, auto_stability, auto_stop) in namespace xpp with their types (DIAGRAM, AUTOAX, BIFUR, AutoStopAt, AutoSettingsSet, ...; auto_stop's, auto_data's and auto_stability's C typedefs plain structs and enums), their defining files wrapped in it; go_go_auto declared in auto_c.h instead of auto_nox.cpp; callers qualified in autlib1, csv_export, diagram, gogoauto, ui_json, auto_state.h, diagram.h and 4 tests (calls taking a Session find theirs by ADL); headers with extern "C" 46 to 38, externcheck 99 to 76 left; callgrind (master 3f1d6a0 vs this stage, WSL gcc 15 -g -O2): kuramot100 at total=10 7,115,332,456 vs 7,115,336,685 (+0.6 ppm), the AUTO script 22,994,411,533 vs 22,994,454,776 (+1.9 ppm) | W109c | done |
| W109e | #161 | Stage 5, the drawing, the data modules, the UI seam and the commands (ani_data, aniparse, array_print, axes2, browse, colormap, graf_par, graphics, grobs, kbs, many_pops, marks_data, menudrive, menus, my_ps, my_svg, nullcline, phase_data, plot_data, scrngif, tutor, ui_json, userbut, xpp_ui: 24 headers, the data modules' extern "C" definitions and four empty extern "C" blocks in json_*.cpp): namespace xpp, std::string_view for the dialog API's text (W28's const char *), the data modules' emit callback a std::string_view. Done: the 24 headers in namespace xpp, no guard (their defining files wrapped in it: ani_data, aniparse, array_print, axes2, browse_data, colormap, commands, graf_par, graphics, grobs, marks_data, menus, my_ps, my_svg, nullcline, phase_data, plot_data, scrngif, userbut, xpp_ui; ui_json.h's functions in ui_json.cpp, json_model.cpp and json_player.cpp; xpp_util.cpp's check_val, batch_plot_name and dump_ps); the data modules' extern "C" definitions and the four empty extern "C" blocks gone; the dialog API's text std::string_view, the XppUi table's fields and the j_ functions with it (TwoChoice's NULL title and File/plaY's NULL path are an empty view, send_simple and send_window lose their NULLs), and a few more texts only read (alter_curve, add_label, window_layer, chk_seq, make_d_table, open_writer_asking, add_stor_col, write_dfield, add_user_button, marks_data_label, ani_data_text); the four data modules' emit a std::string_view (json_io's data_emit takes one; its (line, n) form stays for auto_data, auto_settings and numerics_settings, whose callbacks still take a pointer and a length); the xpp_ prefixes become the namespace (xpp_set_ui is set_ui, the table xpp_ui ui, xpp_cmap_rgb cmap_rgb, xpp_menu_index/_kind/_label, xpp_main_menu_kind/_item, xpp_window_layer(s)), xpp_build_colormap keeping its name until W119 (xpp_batch.cpp calls it) has merged; the drawing's text primitives (put_text, text_abs, the ps_ and svg_ writers) and ps_init/svg_init (image_format.h's table) keep const char *; callers outside the namespace qualified (arrayplot, autlib1, diagram, flags, model_switch, torus, xpp_session, xpp_window, xpp_window_loader, xppautx_main); headers with extern "C" 38 to 14, externcheck 76 to 21 left; callgrind (master 44555f3 vs this stage, WSL gcc 15 -g -O2): kuramot100 at total=10 7,115,322,409 vs 7,115,316,491 (-0.8 ppm), the AUTO script 22,994,542,246 vs 22,994,566,907 (+1.1 ppm) | W109d | done |
| W109f | #161 | Stage 6, the threads and the window's edges, and the end (xpp_http, xpp_inbox, xpp_job, xpp_webview, xpp_win32, xpp_window: 6 headers; tests/test_job.c, the one C caller, becomes .cpp): namespace xpp; what stays extern "C" is externcheck's permanent list (xpp_window_plugin.h, the library's export and its tables; the generated C data of tools/embed.c and embed_bytes.c; rand_s), with no W109 entry left. Done: the 6 headers C++ only, each module in a namespace of its own (xpp::job, xpp::inbox, xpp::http, xpp::window, xpp::win32; xpp_webview's in xpp, since webview is the library's own namespace), their defining files wrapped in it (file-local names that the public ones would hide renamed: http's emit/start/active, the window's run/set_model, job's running/computing/progress state); xpp_every is xpp::every(double &last, ...), the job's progress an enum class (Reported) in a struct (job::Progress), the inbox's queue and classifier constants two enum classes (From, Verdict) and next()'s result a third (Took), references for its out-parameters, std::string_view for a pushed line, http::start(port, show, open) for its flags, bool for every yes/no, std::span for win32::read_stdin, a std::string for webview_create's message; the window hint's two functions in xpp too; job's rows_stored/point_stored/frame_shown are report_rows/_point/_frame and win32's replace_file move_over (deadcheck reads a name declared in two headers as a repeat); the window plugin's C table keeps its C types (the inbox push through a lambda; said_bye and run now bool, XPP_WINDOW_HOST_VERSION 3); the reader threads' out-of-memory exit is xpp_mem's (out_of_memory_now, _Exit: the inbox's abort() and xpp_http's _exit copies merged); tests/test_job.c is .cpp, the Makefile's C test rule gone and sourcecheck fails a tests/*.c; the #ifdef __cplusplus guards of arrayplot, kinescope, model_switch, volterra and xpp_session dropped; CLAUDE.md and the plan say W109 is done; headers with extern "C" 14 to 8 (the plugin and the vendored CVODE's 7), externcheck 21 to 15 lines, no W109 entry; callgrind (master ebdce66 vs this stage, WSL gcc 15 -g -O2): kuramot100 at total=10 7,115,322,623 vs 7,115,317,474 (-0.7 ppm), the AUTO script 22,994,574,798 vs 22,994,383,374 (-8.3 ppm) | W109e | done |
| W110 | #162 | The leave question never stops a computation (maintainer, 2026-09-30: a misclick on x then Cancel must not lose a run): the page shows it while the run goes on; Save session stops, saves and quits; Don't save quits (works even when the core is stuck); macOS's x and Cmd+Q route to it (windowShouldClose/applicationShouldTerminate), checked by CI | W59d | done |
| W111 | #163 | The expression evaluator's speed independent of code position (found by W109a, 2026-09-30: eval_rpn moved 1 KB and kuramot100 ran 9% slower with the same instruction count): align the hot code, or PGO, or link order; measured on master first, kept only if faster and stable across unrelated changes | none | ready |
| W112 | #164 | A plain quit is a normal exit (found testing W110, 2026-09-30): the core says bye before exiting so browser mode ends at once (exit code 0, not the crash path that waits for Ctrl+C); and the watchdog ends a stopped model's log server once no page is left (W108's waits), so the terminal comes back after any exit | none | done |
| W113 | #165 | One name per thing (maintainer, 2026-10-01, after a self-alias broke every clang job): the aliases of our own names go (odex's Error = Diagnostic, SolverResult, N, K), and a source check fails a new namespace alias, using-declaration of an xpp name inside xpp, type alias of our own type or renaming #define; platform shims and new function-pointer types allowlisted with reasons | none | done |
| W114 | #175 | CI reruns only the failed jobs after a fix (maintainer, 2026-10-01): a flaky one, gh run rerun --failed; a fix, pushed with [skip ci] then build.yml's workflow_dispatch jobs input runs only the named jobs; the next batch's push runs everything | none | done |
| W115 | #166 | W108's reload check independent of machine speed (CI windows-clang, 2026-10-01: it races the 2 s wait, W58); and why a stream connected before the leave beacon does not keep the session | W108 | done |
| W116 | #167 | Code review: our own files and commands fail loudly (io_* readers' zeros, skipped session members, invented curves/windows, empty solutions.s, set/slide default 0, catch(...){}, transport.ts swallowed failures, unknown manifest keys) | none | done |
| W117 | #168 | Code review: memory (adjoint/H tables on freed columns, CVODE handles and fort.8 leaked on early exits, job begin/end unpaired, Model/Session raw new/delete) with checks that reach those paths | none | done |
| W118 | #169 | Code review: one source for what core and page share (limits, window/menu ids, menu keys, step attribute, AUTO settings rules and file, point types; hello.protocol checked, web2's older-server fallbacks gone; errors carry their file) | none | done |
| W119 | #170 | Code review: model options as one table (OptionsSet, xpp_reset_options, set_all_vals, the 957-line set_option, numerics_settings, the Numerics menu's own checks) | none | done |
| W120 | #171 | Code review: hidden file-scope state into its owners (about 150 internal-linkage globals; custom_color leaks across loads; AUTO point identity through statics; globalcheck counts local symbols; baseline lines get reasons) | none | done |
| W121 | #172 | Code review: named constants and one helper per job (ESC/KEY_NONE/intervals/ports/extensions/versions, message helpers, no per-call string caps, one JSON escaper (log mojibake), one CSV quoter, text helpers, one listing, one built-in list; xpp_files returns results, json renders) | none | ready |
| W122 | #173 | Code review: performance (full resend after a live run, name lookups per state/row, encode copies, event writes, clock reads per step, shadow copies, web2 O(n) per event) measured before and after | none | done |
| W123 | #174 | Code review quick wins: four empty extern "C" blocks, rand_state_save/load (save in .snapx or delete), reset_dae's unused s | none | done |
| W124 | #176 | A page's command lost on the way to the core (found by W116, 2026-10-01: 'Failed to fetch', once, then 14 FLAKY): find the cause (xpp_http.cpp closing an idle keep-alive socket as a POST goes out?), fix at the root, web2check fails on any lost command | none | done |
| W125 | #177 | One read pipeline for our own files (maintainer, 2026-10-01: Read set keeps values read before a bad line): parse the whole file, check every line and value, then apply in one step or nothing; .set/.par/.ic, .snapx members, .autox, .autoset, .recx all through it; strict everywhere (maintainer, 2026-10-01): a bad value in any of them stops the load naming the file, the line and the value, as a model's bad option does (W119); also File > Get par set (an internal set: W119 found a bad item shown as an error after the items before it were applied) | none | done |
| W126 | #178 | Which DAE result is right (maintainer, 2026-10-01): dae.ode, dae_ex3.ode, exdaebvp.ode before W119 (= XPPAUT, its options ignored) vs after, against independent Python solutions (scipy stiff solve_ivp with the constraint solved in the rhs, a plain fixed-step loop with Newton, other DAE libraries if available); the verdict into docs/xppaut-findings.md | none | done |
| W127 | #179 | A DAE run stops at a fold instead of stepping over it (found by W126, 2026-10-01: dae_ex3's fold at t=0.4507; with W119's loose model tolerance it ran on spuriously to 3.65): accept a step only when the constraint holds and its Jacobian is not singular or changing sign, else stop with 'no solution past t=...' | W119, W120 | done |
| W128 | #180 | Review every unification at the level of the operation (maintainer, 2026-10-01: file I/O was unified by its calls, not 'load a file of ours'): for each unification card, the operation as a user meets it, every place performing it, whether all go through the owner with the same behaviour; each gap a card | none | done |
| W129 | #181 | Save a file (one owner: ask before replacing, the replace decision carried by the command, write, commit's result shown once, the page delivers only what was saved; W128 gaps 1-4, 22; review 2026-10-01: also the session W153 writes beside an imported .set and the session W155 makes AUTO's Save diagram write) | none | ready |
| W130 | #182 | Outputs named outside the file ask (kinescope base name ignores Cancel, array plot's name a form field cut at 24 characters, fixed names never offered to the page) and default file names from one rule (W128 gaps 17, 23) | W129 | blocked |
| W131 | #183 | Edit a value as one operation: the core parses numbers with one strict rule (to_float/new_float/new_int use atof/atoi), a multi-value set is all or nothing with the error on its field, Reset all sends default (W128 gaps 5, 14, 15) | none | done |
| W132 | #184 | Pick a solver once: @ meth, the Numerics menu, the values panel and .set each check differently (the menu silently substitutes Adams, @ meth applies no method_refusal); code compares method ids instead of the solver's traits (W128 gap 6) | none | ready |
| W133 | #185 | Outcomes shown once: results sent through err_msg (histogram Mean, curve fit 'Success!', Liapunov exponent) fail scripts; a script's or -silent's exit code depends on which error function fired; 'out of film' three ways; file-read errors that do not name the file (W128 gaps 7, 8, 16, 21) | none | done |
| W134 | #186 | One route for putting a file into the model's folder: protocol file put is kind data and refused during a run, HTTP PUT /files has no kind check; the page's Open files asks Replace/Keep both, Values > Load replaces silently (W128 gap 9) | none | done |
| W135 | #187 | Sliders become Session state (a setting command, in state, saved in .snapx; today page-only, kept across Open model, lost by Save session) and nullclines/frozen nullclines are saved in .snapx (W128 gaps 10, 11) | none | ready |
| W136 | #188 | Command kinds without exceptions: -runnow/@ runnow runs outside a job (no computing event, cannot be cancelled), ani mouse (kind view) starts an integration, HTTP PUT has no kind, ani Pause/speed during Go is not recorded (W128 gaps 12, 13, 24) | none | ready |
| W137 | #189 | Complete the registries: GIF/PPM and the array plot's PostScript outside the picture table, per-format branches in five files, the range movie written in place; AUTO's CSV has its own writer and quoter (CRLF on Windows); curve-fit data and Import diagram read whitespace only (W128 gaps 18, 19, 20) | none | ready |
| W138 | #190 | aliascheck covers web2: renamed imports (windowKey as layerCommand, setCommand as autoSetCommand) and two exported setCommand functions (W128 gap 25) | none | done |
| W139 | #191 | Drop the options file (maintainer, 2026-10-01; XPPAUT never applied it, docs/xppaut-findings.md #4): a model's `option` line is refused at load, its error naming the line and saying to write the settings as @ lines (in the model or an #include file, which already works: checked 2026-10-01), default.opt is never looked for, Model::options_file and the session's copy go; (the console's missing file and line moved to W140) | W120 | done |
| W140 | #192 | Every error names its file and line (maintainer, 2026-10-01: "for all errors... a unification needed"): report an error is one operation: one error value with file, line, col and the source line (Diagnostic and Error merged), one renderer `file:line:col: what` for console/log/-silent and one event shape for the page; every file read names its line, a run-time error a model line causes names that line, a command's error names the command (in a script, the script's line); errorcheck fails a report without a location. W140a done (the type, renderer, events, page, errorcheck: 356 places without one); W140b: the call sites to 0 (cvode, form_ode/simplenet, AUTO, json_player/json_state with the command or script line, data and tables, run time with the model line, session readers, fail() calls; err_reading's "X: Cannot open X" doubled text) | W120, W122 | done |
| W141 | #193 | The hidden state W120 left (named in tests/globals_internal.baseline): the data modules' records (plot_data, phase_data, marks_data, ani_data) into the Session, the random engine into the Session, load_eqn's options_applied/interopt, const dialog labels (numerics, browse_data) | none | done |
| W142 | #194 | Browser checks download into a folder of their own (maintainer, 2026-10-01: 226 test files in ~/Downloads): cdp.mjs sets one download folder per run under build/ when it starts the browser, emptied first and removed after, never 'default'; download checks read it there; a full web2check adds nothing to ~/Downloads | none | done |
| W143 | #195 | Every XPPAUT bug and limit we fixed, in docs/xppaut-findings.md with linked source lines (maintainer, 2026-10-01): citations as relative links into reference/xppaut-8.0 and reference/xppaut-master (local, git-ignored), no upstream commit details; a round check of the roadmap, git history, front-end-gaps and issues for every XPPAUT bug, wrong result or arbitrary limit xppautX fixed, each verified in 8.0's source | none | done |
| W144 | #196 | Remove --script (maintainer, 2026-10-01): a recording (.recx) is the one replay format; one player; checks move to --server or a .recx; lecar_auto.jsonl becomes a .recx example; decided (maintainer, 2026-10-01): .recx is the only playable format and `xppautX run.recx -silent` plays it with no interface (exit 0 played clean, 1 otherwise; recorded aborts exact) | none | ready |
| W145 | #197 | Our files' values are checked by their owners' rules (code review 2026-10-01, maintainer: "confirm and fix"): a .set's nout 0 / DeltaT 0 load and Go dies on SIGFPE (read_numerics checks only method; the rules are model_options' table); a saved added column vanishes on reopen (data put before the columns); a session's added columns load past MAXODE and with formulas never compiled; the .set and .par trailers unchecked (maintainer, 2026-10-01: ours stops writing the .set's equations trailer, older .snapx refused; the .par's time line strict); Random::load takes trailing garbage; marks type/color 999 and a repeated manifest name load. Each refused with file, line, value, nothing applied; the column round trips; the probes become servercheck/autocheck cases | none | done |
| W146 | #198 | Docs and verify.sh after the review of 2026-10-01: CLAUDE.md's build heading (Linux checks under WSL, Windows builds natively), reviewer vs agent gates at the commands; README's C-era wording and warning status; verify.sh requires python3 instead of skipping its checks | none | done |
| W147 | #199 | A .set is no longer a user file (maintainer, 2026-10-01: "drop support for .set", .par and .ic kept): File > Write set and its key go; Read set and -setfile import XPPAUT's .set only; the format stays as the .snapx's model.set (no equations trailer, W145; older .snapx refused, accepted); .par/.ic stay (value sets swapped without replacing the session, -parfile for batch runs, XPPAUT's format); docs, -help and checks follow | none | done |
| W148 | #200 | xppautX compared with XPPAUT 8.0 (maintainer, 2026-10-01), beyond the bugs of docs/xppaut-findings.md: docs/xppautx-vs-xppaut.md by area (platforms, interface, files, sessions and recordings, model language and .odex, numerics, AUTO, batch and protocol, limits), each row XPPAUT's behaviour (linked into reference/xppaut-8.0), xppautX's, the card; removed features with why; Fixed links the findings; every claim checked | none | done |
| W149 | #201 | CHANGELOG.md (maintainer, 2026-10-01: "what is new, what changed, what fixed"): Keep a Changelog sections per release, Unreleased on top; backfill 0.1.0 and v0.1.0..master; every card that changes what a user meets adds its line (CLAUDE.md rule, checked at merge); a release moves Unreleased under its version and release.yml's notes take that section; what differs from XPPAUT links W148's doc and the findings, not repeated | none | done |
| W150 | #202 | The player over every view (maintainer, 2026-10-01): its keycaps, caption, controls and button light wrap only the main plots (Player.tsx PlayerStage), so a step in the AUTO view shows none of them; they show over any view a step is in. 1x slower: the core's paces (json_player.cpp) scaled by one named constant, 1.5; speeds stay 0.5/1/2/4x | none | done |
| W151 | #203 | A file dialog after an AUTO run starts in AUTO's scratch folder (maintainer, 2026-10-01, the second Play a recording opened %TEMP%/xppautoX-pid-1): find and fix why the current folder becomes it; a dialog starts in the folder of its kind's last file, else the model's | none | done |
| W152 | #204 | The AUTO view's Back becomes "Hide AUTO" (maintainer, 2026-10-01): it pairs with Show AUTO and is told apart from Close (done with AUTO); AUTO and a running continuation stay. The other full views keep Back (no Close, no Show). Manual, web2check, dist, changelog | none | done |
| W153 | #205 | A .set refuses values named for another model (maintainer, 2026-10-01): read_set checks only the counts and skips the name after each value, so another model's .set with equal counts applies in order; every named line's name checked against the model's, refused at the first that differs, nothing applied; then, as a .ode is (W154), it is written at once as a session, <name>.snapx beside the .set, which is what is open from then on (maintainer, 2026-10-01); a finding if XPPAUT does the same | none | done |
| W154 | #206 | Every .ode is converted to .odex when it opens; our files store .odex (maintainer, 2026-10-01: our formats save our format, never the quirky one; always Convert and save): opening a .ode in any mode writes <name>.odex beside it and opens that, saying so (the window with a link to the manual's .odex section, -silent on the console); .snapx/.recx store the .odex text (.autox is gone with W155), one carrying a .ode refused; the loader reads .odex only, the .ode parser only feeds the converter. The examples are converted once in the repo and the tests run on .odex; some .ode files stay as end-to-end conversion tests (.ode -> convert -> run -> the same md5) (review 2026-10-01: after W155, which removes .autox, so W154 never converts a format about to go; after W160, so the converter does not carry `#` comment words into the .odex; its "converted and saved as" message is the one W153 gives for an imported .set) | none | in-progress |
| W155 | #207 | Drop .autoset and .autox (maintainer, 2026-10-01): the session (.snapx) is the one format; it already holds AUTO's members (auto/), and an .autox opened as a new session anyway. AUTO's Save diagram saves the session, Load opens a .snapx; the .autox/.autoset code, association, page controls, docs and checks go (settings_text and the auto/ members stay, under the session); Reload keeping AUTO's settings checked first. Done (Codex sol, high, two runs: the first stopped at its usage limit, the second at OpenAI's content filter before its report, ~1.2M tokens; the review: CLAUDE.md, deadcode's stale entry): one AUTO solution header reader for every restart reader and the session, XPPAUT's .auto an explicit Import XPPAUT diagram | none | done |
| W156 | #208 | Command-line options (maintainer, 2026-10-01): the X11 options (-xorfix, -iconify, -white, -bigfont, -forecolor, -width, -height, -bell, ...) and their code removed, an unknown option an error; every word option takes two dashes (--silent, --parfile, --logfile, ...), a single-dash word refused with its new spelling; -help, manual, README, CLAUDE.md, tools, CI, associations follow | none | ready |
| W157 | #209 | macos-ui's WF-001 races the page (maintainer, 2026-10-01: fix the CI): the refused formula's draft is lost before Enter (no set sent, box unmarked, unfocused), FLAKY in 36892667260 and failing in the two runs since; the check waits on a state that proves the draft and focus before Enter, or the page keeps a typed draft across a re-render (W58) | none | done |
| W158 | #210 | linux-ui fails at the browser's start (no DevTools line in 30 s) or the page load (`__xpp` not defined) in the two runs since 36892667260 (maintainer, 2026-10-01: fix the CI): find whether W142's cdp.mjs change, the page or the runner's Chrome, fix it, and report a browser that did not start with its exit status and stderr; proof is linux-ui on ubuntu-26.04 (2026-10-01: cause not found from the logs; cdp.mjs now names a failed start or load, so the next CI run decides) | none | in-progress |
| W159 | #211 | Golden PostScript differs on the ubuntu-26.04 runner (CI 36945423266: lecar_array.ps, vanderpol.ps) while the same commit passes in WSL on Ubuntu 26.04 with the same gcc and glibc: the runner's CPU, not the source. Step 1, diagnostics (2026-10-01): goldencheck --keep, verify.sh runs the examples after a golden failure, linux-core prints CPU/gcc/glibc and uploads golden-differ-linux. Step 2: the maintainer's decision on the data (the same results on every CPU, a per-runner baseline, or another rule). Done (sonnet, high, the trial: the cause and fix right, speed measured; the review added docs, a deadcheck fix, and noted deadcheck not run): -ffp-contract=off (the runner's 29 models were FMA contraction) and CORE-MATH's correctly rounded functions in xpp::math (glibc's CPU-dispatched variants), one examples baseline for every platform | none | done |
| W160 | #212 | A trailing `#` comment on a `p` line makes parameters of its words, as XPPAUT does (Ermentrout/xppaut#11), and a second such line fails the load (`# is a name already`); .odex reads `#` as a comment but W154 converts through the .ode reader: a `#` starts a comment wherever it is (maintainer, 2026-10-01: "read as a comment of course"), record it in odex-quirks (findings 28 and the extension's issue: done 2026-10-01), test p/init/aux/number lines Done (Codex sol medium; the review only factored the converter's strip)done |
| W161 | #213 | The HTTP server has no limit on request threads or concurrent uploads (found by W134's attack review, 2026-10-01): a token holder can open many 64 MB PUTs at once; a named limit on both, over it refused at once (503, nothing read), a webcheck that exceeds it; a security card Done (Codex sol medium; the review fixed two faults it brought: a refused connection held the accept thread up to 30 s, and its head-only read lost a 411 to a reset 1 run in 3; what is left is W164) | none | done |
| W162 | #214 | webcheck's W112 quit-during-a-run check sends g while the i menu is open, so no run starts (found by W134, 2026-10-01): start a real run (heavy.ode or stop_at_rows, never a clock race) and prove quit arrives during it Done (Codex luna low; the review made the quit's stopped event the proof)done |
| W163 | #215 | Bessel besselj/bessely the same on every CPU (left by W159): port fdlibm's or musl's j0/j1/jn, y0/y1/yn into xpp_math over xpp::math's sin/cos/log; mathcheck fails jn/yn; a reference-value unit test | W159 | in-progress |
| W164 | #216 | The HTTP server's per-request deadlines (left by W161, found at its review 2026-10-02): the receive timeout is per recv, so a client trickling a byte every 29 s holds a slot for ever, and a silent connection holds one 30 s before its token is read, so any local process can fill the 256 slots; sends have no timeout. A named deadline for the whole head, a send timeout, a webcheck that holds a trickling head and sees it dropped (armed by the deadline, never a clock race), docs/protocol.md; a security card | none | in-progress |

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

## W47d: Model and Session passed, in stages

Inventory (2026-09-30, master 593b654): 1984 `xpp::session()` and 991
`xpp::model()` uses in core/*.cpp, 2975 in all: 285 in the front end and
the command layer (W47d1, which left 50 entry points), and after W47d1
442 in the data modules, 470 in drawing and export, 379 in the parser
and the load, 475 in the integrator's driver, 385 in the right-hand side
and the solvers, 547 in AUTO (2747 with W47d1's 50). How the Session can reach each group decides the
stages: a command gets it from the dispatcher; the data modules and
drawing from the commands that call them; the parser from the Load that
builds a fresh one; the right-hand side and the solvers are called
through fixed function pointers on every step (numerics and speed must
not change); AUTO's translated routines have fixed callback signatures;
the XppUi callbacks are called by core code with no Session at all.

Where the current Session is chosen, once, and passed down:

- a protocol command: ui_json.cpp's `handle_line` takes `xpp::session()`
  once per command and passes it to the command table's `run(s, line)`,
  and from there to `commander`, `run_the_commands` and every command
  function. A model switch (File > Open model, Reload) is the one thing
  that replaces it: `switch_model` returns the new Session (from
  `load_requested`, which picks up the one the load made current), and
  handle_line's end of command (`*_update`, state) uses that;
- the start of the program (xppautx_main.cpp, after the first load) and
  -silent's script (json_ui_silent: the script holds the Session it runs);
- a load: `xpp::Load` builds the Model and Session (W47d3 hands them to
  the parser from there);
- until W47d6, the entry points core code calls without a Session (the
  XppUi j_ callbacks, an ask, a checkpoint, flush_pending, the menus the
  Numerics menu opens, AUTO's File menu) read the current one once each,
  at their top, and pass it on.

A Session knows its Model (`Session::model()`, set by Load; the first
Session, made before any load, has the first Model), so a function takes
one `Session&` and needs no `Model&` beside it; a function that only
reads the model (a parser helper, a name lookup) takes `const Model&`.
A function that uses one part of the Session takes that part (the
kinescope's menu takes the `XppKinescope&`).

Each stage: its files' uses reduced to the entry points its callers in
later stages still need, the counts before and after in the commit, no
behaviour change (the md5s, goldencheck, servercheck, autocheck), and
globalcheck not grown.

## W109: the core's API in C++, in stages

Every core source is C++ since W27, but at W109's start 78 of the 125
core headers still declared a C API (`extern "C"` guards, W27's "the API
stays C", kept while the files were converted), and 17 .cpp files had
`extern "C"` definitions or blocks. The maintainer (2026-09-30): C++
linkage, in namespace xpp, with C++ types at the boundary
(std::string_view, std::span, references), keeping `extern "C"` only
where C really calls across; a source check against new ones; in stages
by module; each stage timed, and if C++ costs speed the card stops.

What truly needs C linkage, and stays (tools/externcheck.sh's permanent
entries):

- `core/xpp_window_plugin.h`: libxppwindow.so's one export,
  `xpp_window_plugin_init`, which the loader finds with dlsym by its C
  name, and the tables it trades (XppWindowHost, XppWindowApi), the C ABI
  across the shared library (W13e);
- data the build generates as C and compiles with $(CC): tools/embed.c's
  web_assets.c (`xpp_web_assets`, xpp_http.cpp) and tools/embed_bytes.c's
  icon and window library (`xpp_icon_png`, `xpp_window_lib`,
  xpp_window.cpp and xpp_window_loader.cpp);
- `rand_s`, the Windows C library's, which stdlib.h declares only under
  _CRT_RAND_S (xpp_http.cpp).

Nothing else is called from C: CVODE (W27a), AUTO's translated routines
and every callback handed to a library are compiled as C++, and the C
libraries' own headers (webview, miniz, the OS) declare their callback
types themselves. tests/test_job.c was the one C caller of a core header
(xpp_job.h); W109f made it .cpp, and the unit tests are C++ like the core
(sourcecheck fails a new tests/*.c).

The stages, by module, in the order that keeps each one small and leaves
the hot path to be timed alone:

- W109a, the owner modules (I/O, files, logging, numerics, memory):
  xpp_files, xpp_log, xpp_math, xpp_mem (xpp_io had no guard: its C API
  under the handles is retired instead);
- W109b, the integrator, the solvers and CVODE: 24 headers, of which the
  17 that are xppautX's own were converted; the vendored CVODE's 7 (band,
  cvband, cvdense, cvode, dense, llnlmath, vector) keep their C API until
  W34 (#72) decides whether SUNDIALS replaces CVODE (maintainer,
  2026-09-30);
- W109c, the parser and the load: 11;
- W109d, AUTO: 8;
- W109e, the drawing, the data modules, the UI seam and the commands: 24;
- W109f, the threads and the window's edges, test_job.c to .cpp: 6
  (xpp_window_plugin.h stays).

W109 is done (W109f, 2026-10-01): every core header but the permanent
list above and the vendored CVODE's seven (W34) is C++ in namespace xpp,
with no extern "C" and no `#ifdef __cplusplus` guard (W109f also dropped
the guards of five headers that had nothing but C++ inside them:
arrayplot, kinescope, model_switch, volterra, xpp_session); 78 headers
with extern "C" at the start, 8 at the end (externcheck: 15 lines); no
stage cost instructions beyond a few parts per million.

What a stage does to a module: drop the guard (a header is C++ only, no
`#ifdef __cplusplus`); put the declarations in namespace xpp (an owner's
own namespace inside it where it has one, `xpp::files`); a function's
module prefix becomes its namespace (`xpp_files_exists` is
`xpp::files::exists`, `xpp_out_of_memory` `xpp::out_of_memory`,
`xpp_fft` `xpp::fft`; the printf logger `xpp_log` is
`xpp::log_printf`, beside the std::format `xpp::log`); text a function
only reads is a std::string_view, a pointer and a length a std::span, an
out-parameter a reference, a yes/no a bool; a C API with no caller but
its own C++ wrapper goes (xpp_io's); the callers renamed; the module's
lines in tools/externcheck.sh lowered or deleted (the check fails on a
file whose count is below its entry, so the list follows the stages
down). A name that is the C library's or a library's own keeps its C
spelling, and a type every caller names (XppLogLevel, which the window
library's C table names too) keeps its name.

Speed (the maintainer's rule): each stage is timed before and after, the
two binaries built the same way (WSL gcc 15, the Makefile's -g -O2) and
alternated, 3 runs each with nothing else running: tools/examples_check.sh's
wall time (JOBS=4), examples/ode/kuramot100.ode -silent, and
examples/scripts/lecar_auto.jsonl (an AUTO continuation, about 4 s). W109a
found that the wall time is decided by where the linker puts the code,
not by the change:

| build (W109a's timing), min / median (s) | examples_check | kuramot100 | lecar AUTO |
|---|---|---|---|
| master, -g -O2 | 19.08 / 19.21 | 9.35 / 9.37 | 3.90 / 3.96 |
| this stage, -g -O2 | 19.79 / 19.89 | 10.18 / 10.20 | 3.95 / 4.00 |
| master, -g -O2 -falign-functions=64 | 20.35 / 20.35 | 10.69 / 10.70 | 4.02 / 4.09 |
| this stage (its first revision), -g -O2 -falign-functions=64 | 19.28 / 19.38 | 9.68 / 9.73 | 3.99 / 4.03 |

(the -g -O2 rows: the last 3 of 4 alternated rounds, the first a warm-up;
master alone before the stage, 3 runs: 19.14 / 19.29, 9.44 / 9.77,
4.03 / 4.04; the stage's first revision, which still had the append
Writer and the readers' out-of-line moves, 20.63 / 20.63, 10.73 / 10.80,
4.06 / 4.10.) Both builds execute the same instructions: callgrind counts
7,115,206,679 (master) and 7,115,208,960 (W109a) for kuramot100
shortened to total=10, and 23,001,123,785 and 22,994,163,044 for the
AUTO script. 70% of kuramot100's time is the expression interpreter
(expr_eval.cpp's eval_rpn), 20% libm's sin; W109a did not touch either,
but the files linked before the interpreter changed size, so it and
what it calls sit elsewhere (its start moved by 0x480 bytes), and on this
machine (AMD Ryzen 9 8945HX, Zen 4) that is worth 9% of kuramot100 one way
or the other: built with every function on 64 bytes, master is the slow
one and W109a the fast one, by as much. So a stage's wall time compares
layouts as much as code; each stage also reports callgrind's instruction
counts for kuramot100 (total=10) and the AUTO script, which do not
depend on the layout, and a stage whose instruction counts grow beyond
the noise is the one that costs speed. Pinning the interpreter's layout
(so that an unrelated change cannot cost the shipped program 10%) is a
question of its own, for the maintainer (the release build is LTO's, a
layout again different).
