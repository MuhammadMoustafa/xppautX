# Changelog

What is new, what changed and what was fixed in xppautX, newest first. The
form is [Keep a Changelog](https://keepachangelog.com/en/1.1.0/); each entry
ends with its card and issue (docs/roadmap.md, GitHub). A card that changes
what a user meets adds its line under Unreleased. How xppautX differs from
XPPAUT 8.0, area by area, is [docs/xppautx-vs-xppaut.md](docs/xppautx-vs-xppaut.md);
the bugs found in XPPAUT itself are in [docs/xppaut-findings.md](docs/xppaut-findings.md).

## [Unreleased]

- Detect and locate R1-R4 resonances on two-parameter cycle curves, with named detection controls and diagram/session labels (W400, [#462](https://github.com/MuhammadMoustafa/xppautX/issues/462)).
- A special point whose location does not converge on a curve of cycles or a boundary-value problem is given up with a note and the branch goes on, as in AUTO-07p, instead of ending with "no convergence" (W400, [#462](https://github.com/MuhammadMoustafa/xppautX/issues/462)).

- Plot windows, curves per graph (including ICs xvst), and frozen curves grow with what is added; sessions and recordings preserve all slots and reject invalid indices before applying (W376, [#438](https://github.com/MuhammadMoustafa/xppautX/issues/438)).

- AUTO prints and saves the full point count beyond 9999, with whitespace separating numbers that outgrow fort.7/8/9 columns (W384, [#446](https://github.com/MuhammadMoustafa/xppautX/issues/446)).
- AUTO reuses the Newton Jacobian's residual and skips the discarded residual for a cycle-fold direction; pivot diagnostics follow IID (W359, [#421](https://github.com/MuhammadMoustafa/xppautX/issues/421)).
- AUTO retains repeated evaluation and linear-solve scratch, and caches the automatic worker count for the process (W375, [#437](https://github.com/MuhammadMoustafa/xppautX/issues/437)).

- AUTO labels BT, CP, ZH and GH on equilibrium fold/Hopf curves using AUTO-07p 0.9.4 test functions (GH uses the first Lyapunov coefficient), enabled by default with individual detection controls; fold curves preserve their starting auxiliary period (W364, [#426](https://github.com/MuhammadMoustafa/xppautX/issues/426)).

- Building from source stops on any compiler warning (`-Werror` is on by default, on gcc and clang alike); `make WERROR=0` builds with warnings as warnings, for a compiler not tested here (W401, [#463](https://github.com/MuhammadMoustafa/xppautX/issues/463)).

- DoPri5 and DoPri8(3) retain adaptive steps across output times and sample dense output; Dt is the output interval and DtMax caps internal steps. Flags, sections and extrema use dense roots; resets restart from the root and the run-wide attempt limit reports an error. Delay history remains on its Dt grid (W377, [#439](https://github.com/MuhammadMoustafa/xppautX/issues/439)).

- A user function that recurses without end stops the run past 256 nested calls with its file and line, keeping the rows already stored; recursion that ends (XPPAUT's ode/test.ode factorial) still runs (W387, [#449](https://github.com/MuhammadMoustafa/xppautX/issues/449)).

- AUTO's mesh adaptation follows AUTO-07p 0.9.4: a new mesh point is placed from the nearer old point by the monitor's own density, the mesh's ends are kept exact, and wave trains (IPS 12) adapt as periodic; 13 stored diagrams change in their last digits, agreement with AUTO-07p unchanged (W361, [#423](https://github.com/MuhammadMoustafa/xppautX/issues/423)).

- Stop ends what used to run to the end once started: an equilibrium or shooting sweep (Stop no longer skips only the current point), nullclines and a range of them, the flow, fitting, a histogram of pairwise differences, a fixed-step method's steps between output points and the delay history; what was computed is kept (W388, [#450](https://github.com/MuhammadMoustafa/xppautX/issues/450)).

- The server's input queue is bounded: while the core computes, at most 1024 normal lines (64 MB) wait for it, and the next is refused with a `Busy` error naming its command and id (HTTP: 503 with the same event) so a client can send it again; Abort and the other control lines always get through, so a Stop sent after a flood still stops the run. The longest input line is now 64 MB (was 256 MB) (W389, [#451](https://github.com/MuhammadMoustafa/xppautX/issues/451)).

- Grow equilibrium and shooting sweep storage through the integration store's growth path; size expression stacks from compiled programs, check delay history sizes, and use checked allocation sizes and pointer-width AUTO indices on Windows (W386, [#448](https://github.com/MuhammadMoustafa/xppautX/issues/448)).

- A model declaring Wiener noise runs only on numerical methods that draw it each step (Euler, Mod. Euler, Runge-Kutta, Adams, Backward Euler, Volterra, Symplectic, Discrete); incompatible methods appear disabled in the window's numerics menu with the reason, and --server, --silent, --convert, .set or @ meth= report the error (W379, [#441](https://github.com/MuhammadMoustafa/xppautX/issues/441)).

- Require a positive Total duration in our files and commands, importing XPPAUT negative Total as its absolute value. A full store that cannot grow stops every mode with one error and preserves stored rows (W347, [#408](https://github.com/MuhammadMoustafa/xppautX/issues/408)).

- Range integration can write each reset run to its own file in the window, --server and --silent; `sweep.rangefiles: yes/no` is saved with range settings, and importing `.ode` `@ range=1` enables it (W349, [#410](https://github.com/MuhammadMoustafa/xppautX/issues/410)).

- Continue ODE Hopf curves with the reduced 2n+2 system through zero frequency to a labelled neutral-saddle curve; use structured Hopf/fold Jacobians and reject obsolete Hopf restart layouts (W363, [#425](https://github.com/MuhammadMoustafa/xppautX/issues/425)).
- Lift fixed array limits for formula lengths, tabular expressions, stiff and Marquardt-Levenberg solver pivots, and Markov states and reaction rates; formulas compile directly into dynamically growing vectors and numerical algorithms size work buffers from input rather than hardcoded limits (W372, [#434](https://github.com/MuhammadMoustafa/xppautX/issues/434)).

- Speed up AUTO condensation with cached row maxima and physical pivot swaps, preserving numerical results and interval threading (W360, [#422](https://github.com/MuhammadMoustafa/xppautX/issues/422)).

- AUTO equilibrium periods select the complex pair independently of spectrum order, choosing the lowest frequency for equal damping within roundoff and conservative near-axis ties (W369, [#431](https://github.com/MuhammadMoustafa/xppautX/issues/431)).

- Grabbing an AUTO point sets only its node initial values and preserves Markov initial states; initial-data copies check the state length (W366, [#428](https://github.com/MuhammadMoustafa/xppautX/issues/428)).

- Locate AUTO special points with AUTO-07p Mueller steps, retry-corrected abscissae, refreshed test histories, coincidence rules and multiplier stability criteria; seven explained diagram changes pass the unchanged reference tolerances (W356, [#418](https://github.com/MuhammadMoustafa/xppautX/issues/418)).
- W331 ([#392](https://github.com/MuhammadMoustafa/xppautX/issues/392)): AUTO marks grow with the session; forms, protocol and session files preserve more than nine marks and reject invalid edits atomically.

- AUTO's problem, point and stability codes are types (`core/auto_codes.h`); a diagram or AUTO solution file with a point type that is none of AUTO's is refused with its file and line, and numerics, labels and function-table selections are unchanged (W329, [#390](https://github.com/MuhammadMoustafa/xppautX/issues/390)).
- Compute requested nullclines and direction fields after runs in every mode; PS and SVG exports include the session's fields, nullclines, curves, axes, labels and frozen curves, including in `--silent` (W346, [#407](https://github.com/MuhammadMoustafa/xppautX/issues/407)).

- Add `examples/tstauto/tstheti_homcont.odex`, a planar HomCont heteroclinic example checked against the exact Nagumo front speed in both directions, with mesh and interval refinement; keep the original singular XPPAUT conversion unchanged (W345, [#406](https://github.com/MuhammadMoustafa/xppautX/issues/406)).
- Save values into the model file (W320, [#380](https://github.com/MuhammadMoustafa/xppautX/issues/380)): File > Save values… writes the current values as a new set at the end of the model file, or in place of one kind (the parameters, the initial values or one group of settings) of a set the file writes, keeping that set's other parts and the rest of the file as written. Save as defaults… rewrites the `par` and `init` lines or an options group, and Restore defaults uses them at once. Export values… writes a set into another `.odex` (a new file starts `# Saved from X at T`), and Rename set… and Delete set… change the file's own sets. One writer for the window, `--server` and recordings (`saveset`, `savedefaults`, `exportset`, `renameset`, `deleteset`): the new text is parsed and checked first, a file changed on the disk since the load is refused, the replacement is atomic, an imported set is read-only. Copy set line and its `copy` event are gone: Save values writes the set into the file.
- A model's extra plot windows set their time flag from their own axes, so a window plotting t is a time plot and gets no equilibrium marks (W351, [#412](https://github.com/MuhammadMoustafa/xppautX/issues/412)).
- Keep model-defined plot windows and equilibrium marks in headless sessions, using the same graph and mark bookkeeping as the window and server modes (W348, [#409](https://github.com/MuhammadMoustafa/xppautX/issues/409)).
- Apply set and Restore defaults are two core commands (`applyset`, `restoredefaults`) for the window, `--server`, `--silent` and recordings: Apply set takes a scope (all, the parameters, the initial values or one group of settings) and, for the plot settings, a target window, and applies only the set's entries of that scope over the current values (a parameter-only apply leaves the initial values alone), all or nothing; Restore defaults puts the model's own values back for a field, a section or everything. File > Named sets is now Apply set… (it asks the scope after the set), File > Restore defaults… is new, the `default` and `values` `internset` commands are gone, and `examples/recordings/lecar_auto.recx` answers the scope (W319, [#379](https://github.com/MuhammadMoustafa/xppautX/issues/379))
- AUTO file failures keep their file location; numerical failures name the command or recorded step, and failed stream positioning and EISPACK eigenvalue computation stop with an error (W328, [#389](https://github.com/MuhammadMoustafa/xppautX/issues/389)).

- Refuse malformed or oversized XPPAUT `.auto` imports whole, with the file and offending line; preserve the current settings, diagram and restart solutions on failure (W327, [#388](https://github.com/MuhammadMoustafa/xppautX/issues/388)).
- A `.odex` set's top-level keys are parameters and states only; its settings go under `options: { group: { key: value } }`, from the groups a set may hold (`integration`, `poincare`, `equilibria`, `continuation`, `plot`, `sweep`, `analysis`; not `diagram`, `export`, `interface`, `output`, nor `colormap`, `simplot`, `multiwin`). A setting at the top level, an unknown key or group, or a group a set does not hold is an error at its line and column, and a set is checked whole (its values too) when the model loads. Set names are their own namespace: two sets of one name, an imported file's included, are an error naming both places. `--convert` writes XPPAUT's flat keys by XPPAUT's precedence (a model name first), then nests the settings; the shipped `.odex` examples with sets are rewritten (W318, [#378](https://github.com/MuhammadMoustafa/xppautX/issues/378)).

- AUTO keeps the model's parameters apart from its own unknowns (the period, the homoclinic and test-function values): a model's parameters are numbered in its own order, with no first eight at special places. AUTO needs a model parameter to continue: a model without one is refused with an error. A one-parameter point no longer names a second parameter: the info strip, the diagram's CSV export and Write all info showed the model's second parameter for it (whichever was second, not the Axes' Par2), and now show none, as for a period. A session's AUTO solutions load only in the new layout, an older one refused at its header; XPPAUT's `.auto` files still import (W340, [#401](https://github.com/MuhammadMoustafa/xppautX/issues/401)).

- Remove AUTO's parameter selection and Parameter form: Axes and Run offer every model parameter. Branch records keep all parameter values once for exact grab, label restart, exports and session reopening; the new diagram layout rejects old session files (W339, [#400](https://github.com/MuhammadMoustafa/xppautX/issues/400)).

- Allow POSIX system links along read, write-folder and scratch paths when both the link and its parent belong to root and the parent has no group/other write permission; user-planted links and final file links remain refused, and Windows still refuses every reparse point (W311, [#370](https://github.com/MuhammadMoustafa/xppautX/issues/370)).

- Preserve the restored solution's eigenvalues or Floquet multipliers at AUTO's failed-step MX endpoint, including equilibrium branch switching and special-point searches that restore an unstored solution; uncached endpoints use the full spectrum without assuming a trivial multiplier (W341, [#402](https://github.com/MuhammadMoustafa/xppautX/issues/402)).

- Allow system/user links above the write folder boundary, including macOS temp/config ancestors; continue refusing links below it and rechecking created folders (W342, [#403](https://github.com/MuhammadMoustafa/xppautX/issues/403)).

- Refuse nonfinite AUTO solutions: retry failed Newton steps with smaller step sizes and report the attempted branch point when retries fail; never store NaN states, parameters, norms or spectra, and preserve a populated single-point diagram when saving. The singular deprecated `hom_bcs` tstheti fixture now expects an error instead of treating NaN points as a connection (W343, [#404](https://github.com/MuhammadMoustafa/xppautX/issues/404)).

- Keep AUTO's Parameter form selection unchanged on OK: unselected fields are blank, and the form and protocol select exactly the filled, distinct model parameter names in order (W338, [#399](https://github.com/MuhammadMoustafa/xppautX/issues/399)).

- Name option groups by their work and accept slider presets as a list of any length; old native group names and S1-S3 keys are refused, while XPPAUT options convert to the new groups (W317, [#377](https://github.com/MuhammadMoustafa/xppautX/issues/377)).

- Call mixed sets "Named sets": they hold parameters, initial values and settings; menu keys are unchanged (W316, [#376](https://github.com/MuhammadMoustafa/xppautX/issues/376)).

- Remove startup `@ runnow` and `--runnow`; conversion drops the option with a comment and note. Animator release actions remain available (W312, [#371](https://github.com/MuhammadMoustafa/xppautX/issues/371)).
- Warn once per load about states without `init`, naming their equation files and lines; they still start at 0. Conversion writes explicit zero initial values (W313, [#372](https://github.com/MuhammadMoustafa/xppautX/issues/372)).
- Remove prefixed imports (`import "f.odex" as c1`) and dotted names; whole-file and selective imports remain (W315, [#374](https://github.com/MuhammadMoustafa/xppautX/issues/374)).
- Explain that a `.odex` without state equations is a file to import into a model when opened alone, in every mode (W324, [#384](https://github.com/MuhammadMoustafa/xppautX/issues/384)).

- AUTO selects its collocation workers once per run and keeps small problems on one thread, avoiding worker overhead while preserving the diagram and large-model speedups (W325, [#386](https://github.com/MuhammadMoustafa/xppautX/issues/386)).

- AUTO restores the converged solution, tangent, continuation history and stability data when locating a special point exhausts ITMX. Morris-Lecar continuation at NTST 15 now reaches the fold of cycles instead of turning round after a failed branch-point search (W326, [#387](https://github.com/MuhammadMoustafa/xppautX/issues/387)).

- AUTO's homoclinic form sizes its values from the model, so it works beyond 49 variables (XPPAUT's overruns its 100 fields there: [findings #49](docs/xppaut-findings.md)) (W301, [#359](https://github.com/MuhammadMoustafa/xppautX/issues/359)).

- `tools/autocompare.py` also checks the special points of every AUTO continuation of tests/auto against AUTO-07p 0.9.4 and XPPAUT 8.0 (`tests/auto/*.auto07p`, `*.xppaut`, written by `tools/auto_reference.py`), by type and order, within a tolerance derived per quantity from the location and Newton tolerances; 26 models agree with AUTO-07p and 24 with XPPAUT 8.0 to roundoff and the location tolerance, and three disagreements with AUTO-07p are traced and recorded ([docs/auto-references.md](docs/auto-references.md), [findings #47](docs/xppaut-findings.md): AUTO2000 does not restore the last converged point when the location of a special point fails). (W300, [#358](https://github.com/MuhammadMoustafa/xppautX/issues/358))

- AUTO's variable, parameter and branch-point storage grows with the model and run: the former 400-variable, 36-parameter, 20-branch-point and eight selected-parameter limits are lifted. The first eight model parameters remain the initial selection (W298, [#356](https://github.com/MuhammadMoustafa/xppautX/issues/356)).

- Parameter files (`.par`) load by name in any order, refusing unknown, duplicate or missing parameters without changing any values; positional `.ic` files still require exactly one value per differential equation. Both imports use the 4 MiB settings bound (W310, [#369](https://github.com/MuhammadMoustafa/xppautX/issues/369)).
- Opening a `.ode` asks each outside path once across conversion and loading;
  Allow always also applies to its generated `.odex`. Log streams refuse a
  link swapped into the final name before opening (W297,
  [#355](https://github.com/MuhammadMoustafa/xppautX/issues/355)).

- Writes offer to create missing folders through the shared question policy; permission errors name the folder and advise another destination, and the desktop save dialog retries in the model folder. No privilege elevation (W303, [#361](https://github.com/MuhammadMoustafa/xppautX/issues/361)).

- All question kinds share one answering policy: `--check` declines conversion name questions, and `--silent` offers the same Replace / Open existing .odex choice as the window and server; `--yes` prints the suggested answer in every front end (W304, [#363](https://github.com/MuhammadMoustafa/xppautX/issues/363)).

- File readers refuse allocation-sized options above named maxima, cap the shared `.odex`/`.setx` lexer at 1,048,576 tokens, and reject ranges longer than 100000 elements before expansion; saved array/transpose sizes and AUTO session mesh/continuation sizes are checked too, with the file and line (W296, [#354](https://github.com/MuhammadMoustafa/xppautX/issues/354)).

- Large models load faster through one Model-owned name index: native UCRT load medians fall from 230.4 to 183.4 ms for lecdiff's 4,800 states (20%), and from 130.3 to 116.0 ms for amari's 2,400 points (11%; five-run medians in one exclusive series, including native compilation). Name errors retain their text and source line (W291, [#349](https://github.com/MuhammadMoustafa/xppautX/issues/349)).

- A path a model names outside its own folder (`..` or an absolute one: an include, an import, a table, the animation file, `output` and `logfile`) is a question instead of a refusal: the dialog (or `--server`'s `ask`, or the terminal's 60 s question in `--silent` and `--convert`) names the model, its line and the path, and offers **Allow always** (kept in the preferences for this model file and this exact path, 24 answers at most, the oldest forgotten), **Allow once** and **Refuse**; no answer refuses and stops the load with the file and line, and `--yes` allows once and prints it; `--check` asks nothing, so such a path is a load error there unless the preferences already allow it. A link on the way, or a name that is not a plain one, is still refused with no question; an allowed path is still opened without following a link. A `logfile` named in a model used to be written wherever it said (W286, [#346](https://github.com/MuhammadMoustafa/xppautX/issues/346)).

- A model has no limit on its variables, parameters or symbols: XPPAUT stops at 5000 variables (`MAXODE`), 400 parameters and 10000 parser symbols, and xppautX sizes its tables by what the model declares (an old-style file's first line sizes nothing; the tables grow with the lines read). A model with over 4995 variables now also gets its primed variables, so Averaging and boundary value problems work for it. The data browser's Add column keeps room for 1000 added columns (W243, [#299](https://github.com/MuhammadMoustafa/xppautX/issues/299)).

- The program's preferences are one per-user file, `preferences.setx` in the config folder beside `keymap.json`, in the `.setx` syntax and loaded all or nothing; the recent models moved into it (`recent_models`), so `recent.txt` is no longer read and the start screen lists none until models are opened again (delete the old file); `keymap.json` stays its own file. Results never depend on it (W287, [#342](https://github.com/MuhammadMoustafa/xppautX/issues/342))

- An option's error reads `the option `dt` = `abc`: not a number` and carries the file and line of the `.ode`, `.odex` or `.setx` it came from, instead of always the `@ name=value` of an `.ode`. Behind it, an option is one row of the option table and its member (the "set once" flag, the session's changed-flag over the numerics, the page's numerics keys and the Numerics menu's follow-up are the row's or checked against it, `tools/optioncheck.py`) (W293, [#351](https://github.com/MuhammadMoustafa/xppautX/issues/351))

- `@ output=<file>` (and `output:` in a `.odex`'s `batch` group) names the batch data file as XPPAUT's batchout does: `--silent` writes there, from the model's folder, and a path that leaves the folder (`..`, an absolute one) stops the load with the file and line; a `.ode` used to convert it to the number 0 and write a file named `0` (W273, [#330](https://github.com/MuhammadMoustafa/xppautX/issues/330)).

- Each AUTO branch records the Jacobian it was computed with (difference or exact, the setting at its run): the diagram data's runs and the info strip carry `jac`, the session's `auto/diagram.csv` and File/eXport CSV a `jacobian` column; a session saved before this has no such column and is refused with its file and line (W278, [#335](https://github.com/MuhammadMoustafa/xppautX/issues/335)).

- An AUTO run an exact Jacobian stops (a derivative not finite where the right-hand side is) now ends as a Stop does: the last converged point is stored as an end point, and the error offers "Switch to differences and continue", which sets the Jacobian to difference and restarts the continuation from that label with the run's settings (`{"cmd":"auto","op":"restart","label":N}`) (W275, [#332](https://github.com/MuhammadMoustafa/xppautX/issues/332)).

- Settings files are `.setx`, in the syntax of a model's `options` block (named keys, any order, loaded all or nothing with the file, line and column of the first wrong key or value): the parameters, initial values, numerics (the Jacobian included), delays, boundary conditions and the active window's graphics, the numerics under the option table's own keys; a file that names only some of them sets only those. XPPAUT's `.set` is import only: File > Import settings and `--setfile` of one write the `.setx` beside it (asking before another text replaces one that is there, as `.ode` to `.odex` does) and apply it, and no longer save a `.snapx` session. The JSON Schema of a `.setx` is docs/setx.schema.json (`xppautX --setx-schema`) (W284, [#344](https://github.com/MuhammadMoustafa/xppautX/issues/344))

- A session file's (`.snapx`) settings members are in the same syntax, so adding a setting is one key: `model.setx`, `windows.setx`, `marks.setx`, `sliders.setx`, `nullclines.setx` and the manifest `session.setx`, and a recording's (`.recx`) header is a `recording` group of it; a session or recording of the older layout is refused with the ordinary parse error, and the shipped recording is regenerated. The numerics' Jacobian setting is saved with the session and restored (W285, W274, [#345](https://github.com/MuhammadMoustafa/xppautX/issues/345), [#331](https://github.com/MuhammadMoustafa/xppautX/issues/331))

- Windows loads the About dialog's common-controls DLL only when About opens: a one-step `lecar.odex --silent` run takes 34.3 ms instead of 35.3 ms (stripped UCRT build, 21-run medians; W290, [#348](https://github.com/MuhammadMoustafa/xppautX/issues/348)).

- `--auto` is now `-y` / `--yes` (a modifier of any mode: `--silent --yes`, `--convert --yes`), and every answer taken without asking is printed with its question: a rename, a file replaced, a recording's recorded answer in `--silent`. `--silent` no longer takes a conversion's suggested names silently: it asks on the terminal (60 s), and with no answer it stops and names what needs one (W288, [#343](https://github.com/MuhammadMoustafa/xppautX/issues/343)).

- Large models load faster through indexed symbol, source-location and initial-state lookups: Windows one-step `.odex` runs fall from 835 to 274 ms for lecdiff's 4,800 states and from 502 to 168 ms for amari's 2,400 states (five-run medians, stripped UCRT build; lecar stays about 38 ms). Compiled code is still built in memory at each load (W245, [#301](https://github.com/MuhammadMoustafa/xppautX/issues/301)).

- `import` replaces `include` in a `.odex`: `import "f.odex"` takes everything, `from "f.odex" import a, b` those names and what they depend on, `import "cell.odex" as c1` everything with every name prefixed `c1.` (names may now contain dots: `c1.v`), so one cell file imported twice gives two cells. A name that is already declared is an error at the import line, an import cycle too, imports nest to 16 files and a model makes at most 256; an imported file's options are not imported. A `.odex` with `include` stops with the parser's error (W282, [#339](https://github.com/MuhammadMoustafa/xppautX/issues/339)).

- The JSON Schema of a `.odex`'s `options` block is generated from the option table (core/model_options.cpp, the one source) into docs/odex-options.schema.json, for the VS Code extension and linters: `xppautX --options-schema` prints it and `tools/optionschema.sh --check`, in sourcecheck, fails when the committed file differs from the table (W281, [#338](https://github.com/MuhammadMoustafa/xppautX/issues/338)).

- A `.odex`'s options are one block, `options { numerics: { total_time: 100, method: stiff }, plot: { xp: v, yp: w } }`, and a named set is `set hopf { v3: 0, v4: 0.3 }` (entries `key: value`, comma separated, a trailing comma allowed; paths and the button text in double quotes, every other value one bare word, so the methods are `runge_kutta`, `modified_euler`, `backward_euler`, `dopri8`, ...). `options` is a reserved word; the keys are exact (XPPAUT's names and prefixes such as `xplot` are errors). The `@` form is a `.ode`'s and what `--convert` reads; every example `.odex` is regenerated, and a `.odex` with `@` lines stops with the parser's error at its line. Copy set line writes the new form (W280, [#337](https://github.com/MuhammadMoustafa/xppautX/issues/337)).

- `--convert` keeps a `.ode`'s statement order and each comment on a line of its own where it was (comments used to move to the end of the `.odex`); every converted example is regenerated, results unchanged (W283, [#340](https://github.com/MuhammadMoustafa/xppautX/issues/340)).

- AUTO defaults to physical cores (performance cores where the system identifies them), at most four workers, reducing the automatic count on machines with SMT; results remain identical (W270, [#327](https://github.com/MuhammadMoustafa/xppautX/issues/327)).

- An exact Jacobian, as a numerics setting: `@ jacobian=exact` (or the Numerics section's Jacobian) differentiates the model's right-hand sides symbolically, through its fixed variables, derived parameters, user functions and tables, and compiles the derivatives with the model; equilibria, Gear, CVODE, Rosenbrock, Backward Euler, Stiff, the adjoints and AUTO (`JAC=1`, its collocation threads included) all use it. `difference`, XPPAUT's finite differences, stays the default, so a model gives XPPAUT's numbers unless it asks. A model with random numbers, Markov variables, delays, sums and the like has none: the setting shows why, and its own `@ jacobian=exact` is a load error at that line. A run that meets a point where an exact derivative is not finite (`sqrt` at 0, ...) stops there with the equation's line, the variable, the time and the state; an integration's error offers **Switch to differences and continue** (W260, [#317](https://github.com/MuhammadMoustafa/xppautX/issues/317)).

- `bessel_i` returns NaN for non-finite inputs or orders above 100000, handles negative integer orders by symmetry, and retains XPPAUT's truncation of non-integer orders (W271, [#328](https://github.com/MuhammadMoustafa/xppautX/issues/328)).

- AUTO's thread count is a setting of the program: AUTO's Numerics has a Computer group with Threads, Automatic (physical cores, at most 4) or any count from 1 to the computer's logical processors, kept in `preferences.setx` for every model and run; the results are the same for every count (W267, [#324](https://github.com/MuhammadMoustafa/xppautX/issues/324)).
- AUTO collocation uses a persistent pool on up to four threads for a compiled pure model; every result is unchanged across thread counts. Interpreted or impure models stay serial (W247, [#303](https://github.com/MuhammadMoustafa/xppautX/issues/303)).

- DAE guesses and residuals, Markov transitions, kernel integrands and convolutions, event conditions and actions, derived parameters and network pair formulas now compile alongside equations and user functions. Unsupported programs and their user-function callers stay interpreted with individual source warnings; independent programs still compile (W266, [#323](https://github.com/MuhammadMoustafa/xppautX/issues/323)).

- Banded Backward Euler and Rosenbrock clamp widths to the system size, scale only the stored Jacobian, and reserve enough work space for the band (W261, [#318](https://github.com/MuhammadMoustafa/xppautX/issues/318)).
- A `shift` or `del_shft` whose computed index falls outside the variables (or constants) reads 0 instead of memory outside them (W265, [#322](https://github.com/MuhammadMoustafa/xppautX/issues/322)).
- One finite-difference Jacobian for the whole program (equilibria, AUTO, Backward Euler, Rosenbrock, the stiff integrator, delay stability): `(f(x+r)-f(x))/r`, the implicit solvers scaling afterwards, so Backward Euler and Rosenbrock (banded too) differ from XPPAUT in the last bits (W255, [#312](https://github.com/MuhammadMoustafa/xppautX/issues/312)).
- Gear's method and the adjoints use that Jacobian too (W264, [#321](https://github.com/MuhammadMoustafa/xppautX/issues/321)): Gear scales by `a0*h` afterwards, the adjoint's step floor is `eps^2` instead of `eps`; last bits differ from XPPAUT.
- Compile core equations and user functions with built-in TinyCC at load time; unsupported models run in the interpreter with a source warning. `--no-compile` selects interpreted execution (W258, [#315](https://github.com/MuhammadMoustafa/xppautX/issues/315)).
- More models run compiled: lookup tables, networks, vectorizers, kernels, sums, delays and shifts, `set`, the vector index `@` and random draws now compile as well (176 of the 184 example models, from 89). Where the interpreter has a function for an operation (a table lookup, a delay, a draw from the session's generator) the compiled code calls that same function, so results and random sequences are bit-identical to `--no-compile`; the only model still interpreted is one with a recursive user function (W259, [#316](https://github.com/MuhammadMoustafa/xppautX/issues/316)).

- AUTO's periodic and boundary-value runs are faster (1.32x on a measured periodic branch): the residual no longer computes a Jacobian it never reads; results are unchanged (W253, [#310](https://github.com/MuhammadMoustafa/xppautX/issues/310)).
- Build: vendor TinyCC and its in-memory compiler adapter; model compilation is not connected yet, so user behavior is unchanged (W254, [#311](https://github.com/MuhammadMoustafa/xppautX/issues/311)).

## [0.1.0-beta.2] - 2026-10-09

Second beta: a start screen with recent models and the bundled examples, text on a plot as an object, one axis dialog, panels you can hide and resize, and fixes.

**Updating from beta.1:** beta.1's Check for updates cannot see this release (it asked GitHub only for the latest stable release, and every release so far is a pre-release), so download beta.2 from this page by hand. From beta.2 on, a beta's Check for updates tells you of newer betas.

- Fixed: **Check for updates in a beta** failed with HTTP 404, since GitHub's "latest release" never names a pre-release and every release is one. A pre-release build now reads the newest releases including pre-releases and offers the highest version by SemVer precedence (0.1.0-beta.2 over beta.1, the release 0.1.0 over any of its betas); a release build is never offered a beta. beta.1 itself still has the old check, so it cannot learn of beta.2; from beta.2 on the check works (W234, [#290](https://github.com/MuhammadMoustafa/xppautX/issues/290)).
- **Opening the browser starts no shell** on Linux and macOS: the page address (and an About link) is passed to `xdg-open`/`open` as one argument with `posix_spawnp`, where a `'` in it could break out of the shell quoting; under WSL it opens with `rundll32.exe url.dll,FileProtocolHandler` instead of `cmd.exe /c start`, which parsed `&`, `^` and `%` (W231, [#287](https://github.com/MuhammadMoustafa/xppautX/issues/287)).
- **The start screen lists the bundled examples.** The `.odex` models in the `examples/` folder beside the program (in the `.app`, Contents/Resources/examples) are listed under Examples; a click opens a copy of the example in `examples/` of the settings folder (an installed folder is read-only, and a model writes its files beside itself), asking before it replaces a copy you changed. A program with no examples folder says so on the start screen (W233, [#289](https://github.com/MuhammadMoustafa/xppautX/issues/289)).
- **Starting without a file shows the Open model dialog, and Cancel leaves a start screen** instead of an error. The start screen has the Open model button and the models you opened lately (kept in `recent.txt` beside `keymap.json`; a model whose file is gone is listed as missing). The dialog is now one: the start asks the same File > Open model dialog, which takes `.ode`, `.odex`, `.snapx` and `.recx` (at the start it took only `.ode*`), and the desktop window's native dialog offers the same types from one list (W232, [#288](https://github.com/MuhammadMoustafa/xppautX/issues/288)).
- **The desktop window opens maximized** (title bar, menu bar and taskbar kept), not fullscreen; un-maximizing gives the former centred size. Size and position are not remembered (W230, [#284](https://github.com/MuhammadMoustafa/xppautX/issues/284)).
- **One axis dialog, in columns.** Clicking an axis name on a plot opens one dialog with a column per axis (X, Y, and Z on a 3D plot), each with its variable, min, max and label; on a 3D plot it also holds the view window and the 3D parameters (perspective, ZPlane, ZView, Theta, Phi, the rotation movie). It applies all or nothing: a bad variable, a number that is not one, or a Min not below its Max is an error naming the field, and nothing is changed (W225, [#279](https://github.com/MuhammadMoustafa/xppautX/issues/279)).
- **The Tools group is only the Calculator** (W229, [#283](https://github.com/MuhammadMoustafa/xppautX/issues/283)). Named sets and Copy set line are beside the parameters in the Values panel, Transpose and Lookup tables in the Data panel, Model source in the Model panel, Keyboard shortcuts in Help, Record and Play recording in the title bar's Tools menu; their keys are unchanged. Removed: File > Help (the title bar's Help and F1 remain), Tutorial (`@ tutorial` is accepted and ignored), Preferences (`.xpprc` is still read at start; `XPPEDITOR` is no longer used) and Clone model. A keymap that names a removed command is an error at its line.
- Named parameter sets: a model with none says so, and a model with more than 26 can pick any of them (the first 26 have a letter, the rest are picked in the list); a model with exactly 500 sets no longer gets a menu that silently does nothing. `examples/ode/lecar.ode`'s set `pp` had a dot for a comma (`xlo=-.6.xhi=1.2`), which XPPAUT also reads as `xlo` alone; fixed (docs/xppaut-findings.md, 38).
- **The Plot group lists only the plot commands the plot does not offer itself** (W228, [#282](https://github.com/MuhammadMoustafa/xppautX/issues/282)). No longer in the sidebar, search or the toolbar's pins (their keys still work): Zoom and view (the plot's Fit, Reset view and Use this view), and Plot axes and 3D view (a click on an axis name opens the axis dialog, with its 3D column). A keymap that pins one of them is an error at its line. Kept in the group because nothing else does them: Plot windows and Captured frames (items with no other control: Autoplay, Save; Kill all, Bottom, Auto, Manual, SimPlot), Curves and export (Freeze, its keys and colours included: the Freeze checkbox only keeps replaced runs in the page), Nullclines and Direction fields (the legend only shows or hides what they compute), Labels and annotations (arrows, pointers, markers and Delete all have no dialog yet), Phase space, Variable vs time, Redraw, Clear and Color by value.
- **Text on a plot is an object, like a trace.** Adding text (Labels and annotations > Text, then a click on the plot) opens one dialog for the text, its size, style (normal, bold, italic, bold italic) and colour; double-click a text on the plot to open it again and change any of it, move it, or delete it. Texts are saved in sessions with their style and colour; a session saved before this one that holds texts cannot be opened (W227, [#281](https://github.com/MuhammadMoustafa/xppautX/issues/281)).
- Fixed: the Xlabel/Ylabel (and Zlabel in 3D) of the Viewaxes form now show on the plot at once, and clearing one goes back to the column name; a 3D plot titles its axes.
- The Commands and Values panels can be hidden and shown with buttons in the title bar and resized by dragging their inner edge or with the arrow keys; the layout is remembered (W226, [#280](https://github.com/MuhammadMoustafa/xppautX/issues/280)).
- **About, with links that work.** Help > About (the page and the window's own box) now shows the author, links to the author's email, GitHub and LinkedIn, the source and the issue tracker, and credits XPPAUT and G. Bard Ermentrout. Every link opens in your default browser, in the desktop window as well (#278).

## [0.1.0-beta.1] - 2026-10-08

First public beta of xppautX, a modernised XPPAUT 8.0: the same equations, solvers and AUTO, with a new interface.

- **Your models work.** XPPAUT `.ode` files are converted once to `.odex` when opened; results match XPPAUT's, and are the same bit for bit on Windows, Linux and macOS.
- **A new interface** in the desktop window or a browser: a command list with search (Ctrl+K), keyboard shortcuts you can change (Help > Change shortcuts), a toolbar you can pin commands to, and plots, values, tables and AUTO diagrams as live panels.
- **Runs:** Run from initial, Run from last state, one Continue (Alt+Enter), Run to steady state (Alt+S), Store every N steps, and undo of value edits (Ctrl+Z).
- **Sessions:** save everything with Ctrl+S (a dot in the title shows unsaved changes), Save as, Save a copy, Reload keeping your values; recordings (`.recx`) replay a whole analysis.
- **Headless and scripting:** `--silent` runs and a JSON protocol on stdin/stdout or HTTP.
- **Settings have descriptive names** (`total_time`, `transient_time`, `storage_rows`, ...); XPPAUT's old words are accepted only when converting a `.ode` or on the command line.

Downloads: Windows installer (`.msi`, installs for all users and opens our file types) or portable `.zip` (no admin rights); Linux `.deb` or `.tar.gz`; macOS `.dmg`; `SHA256SUMS.txt` to check them. The source is the tag's source archive.

Known limits of this beta: the macOS builds are tested by the automatic checks only, not by hand; the installers are not signed, so Windows SmartScreen or macOS Gatekeeper may warn; the one-letter XPPAUT key sequences are an optional preset (off by default).

The detailed change log is CHANGELOG.md in the repository; how xppautX differs from XPPAUT, area by area, is docs/xppautx-vs-xppaut.md.

## [Development before the first beta]

### Added

- Keyboard shortcuts editor (the command Keyboard shortcuts… in the command list's Tools group and its search, and Help > Keyboard shortcuts): every command with its category, keys and source (default, changed, user); search by name, description, id or the keys pressed; click a key and press the combination (Esc cancels, Backspace clears); a key another command has asks "Replace / Cancel"; the keys the system keeps are refused with a message; add a second key, reset one command or all, choose the preset Default or XPPAUT sequences. Fully keyboard reachable and announced to a screen reader. Pinned quick-access toolbar under the run buttons: pin a command from the command list or the editor, drag to reorder, Unpin / Move from its context menu, a More menu for what does not fit. Everything is saved in `keymap.json` through the one `keymap` command, and the page's keyboard layer now runs your keys (the core's reserved list, not a copy). New commands with keys: Run from initial (Ctrl+Enter), Run from last state (Ctrl+Shift+Enter), Run to steady state (Alt+S); the button "Run from current" is now "Run from last state" (W212, [#266](https://github.com/MuhammadMoustafa/xppautX/issues/266)).

### Changed

- Run to steady state is no longer stopped by the storage limit: it needs only the last values, so when the store fills it keeps every second row and stores every second event from then on, and runs until it settles, reaches the Maximum duration, is stopped or fails; the `storage-limit` result is gone. The message of any other storage stop (a run without steady state) names the limit and its value (W219, [#273](https://github.com/MuhammadMoustafa/xppautX/issues/273)).
- Undo value edit (Ctrl+Z) and Redo value edit (Ctrl+Shift+Z, Ctrl+Y): a bounded stack (100) of the parameters, initial conditions and numerics before each edit, replacing the page's Keep/Restore working values checkpoint. Run from last state (the toolbar's former "Run from current") saves the initial conditions it overwrites, so Undo brings them back; runs, plots and files are not undone. In a text field Ctrl+Z stays the field's (W210, [#264](https://github.com/MuhammadMoustafa/xppautX/issues/264)).
- Reload model keeps the session file the session was saved to or opened from, so Ctrl+S after Reload saves to it with no question; the data and diagram Reload drops show as an unsaved change. New command Save a copy of the session… (File menu key B, no default key, pinnable): asks for a path, writes the `.snapx` there and leaves the session's file and its unsaved state as they were (W218, #272).
- Save session (Ctrl+S) saves to the session file the session was opened from or last saved to, with no dialog; the first save asks for a name. Save session as (Ctrl+Shift+S) always asks, and before replacing a file. The header shows a dot while the values (parameters, initial conditions, the time span, method and tolerances, the run's data) differ from the saved or loaded ones (`state.changed`); Quit's "Save this session first?" saves to the remembered file without asking again. The desktop File menus show both commands with their keys (W209, [#263](https://github.com/MuhammadMoustafa/xppautX/issues/263)).
- One keyboard dispatcher from a key to a command id, driven by the command table's default keys (Reload model gets Ctrl+R); chords show a pending-key hint and cancel with Esc or a focus change; Alt+F4, Ctrl+W, Ctrl+Q, F11, F12 and F5 are never bound or prevented. The XPPAUT one-letter keys (F then S) are now an optional preset "XPPAUT sequences", off by default, in Help > Keyboard shortcuts; the sticky File/Numerics shortcut mode and the Caps Lock status are gone (W208, [#262](https://github.com/MuhammadMoustafa/xppautX/issues/262)).
- The user's keymap is a file: `keymap.json` in the per-user config folder (`%APPDATA%\xppautX`, `~/.config/xppautX`, `~/Library/Application Support/xppautX`) holds only the user's differences (key bindings, pinned commands, the XPPAUT-sequences preset). The core owns it: one `keymap` command (get, set, reset) serves the window and the browser alike, `hello` carries the effective keys, and a bad file is refused whole, with its file, line and value, never applied in part and never silently ignored. (W211, [#265](https://github.com/MuhammadMoustafa/xppautX/issues/265)).

- Closing the Animation or Array plot sheet returns the keyboard focus to the Tools button (their buttons sit in the Tools menu, closed by the choice); a plain `quit` read by a running computation now sends `stopped` before `bye`, as one read after the command does (W217, [#271](https://github.com/MuhammadMoustafa/xppautX/issues/271)).

- One command table in the core gives every command its id, category, label, description, kind and default keys, and `hello` sends it as `command_table`; the sidebar, command search and the desktop File menu are made from it, so they cannot disagree. The File menu now shows the same labels as the sidebar (Reload model for Reload) and, on Windows, Ctrl+O beside Open model (W207, [#261](https://github.com/MuhammadMoustafa/xppautX/issues/261)).

- The vague numerics names say what they are, in a `.odex`, a set file's checks, the protocol (`numerics` keys, `set num`) and a recording: `total_time`, `start_time`, `transient_time`, `nullcline_mesh`, `singpt_max_iterates`, `singpt_newton_tolerance`, `singpt_jacobian_epsilon` (Sing pt: the labels say the same), `method`, `tolerance`, `min_step`, `max_step`, `abs_tolerance`, `bvp_max_iterates`, `bvp_tolerance`, `bvp_epsilon`, `storage_rows`, `torus_period`, `poincare_map`, `poincare_variable`, `poincare_sign`, `poincare_stop`, `poincare_plane`; the Numerics menu's commands are `total_time`, `start_time`, `transient_time` and `nullcline_mesh`, and "Ncline mesh" is "Nullcline mesh", "Abs. tolerance" "Absolute tolerance". A `.odex` with XPPAUT's word (`@ total=`, `@ meth=`, `@ trans=`, ...) or a `set`/narration action with one fails naming it; a `.ode`, an XPPAUT `.set`, `.xpprc` and the command line keep XPPAUT's words and `--convert` writes the new names (W216, [#270](https://github.com/MuhammadMoustafa/xppautX/issues/270)).

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

- Playing a recording no longer makes the keymap read as the defaults (the user's keymap file is no longer served from the recording or recorded as a file the model read), and the keymap's folder is made when the working folder is on another drive on Windows (W220, [#274](https://github.com/MuhammadMoustafa/xppautX/issues/274)).
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

- `--script` and its `.jsonl` replay format: recordings are the one playable format. `xppautX run.recx --silent` loads the snapshot and plays without an interface, exits 0 on clean playback or 1 on failure, and reproduces recorded aborts exactly. Silent playback keeps plain-name outputs in the launch folder; existing files require terminal confirmation within 60 seconds (EOF or timeout stops playback), or `--yes`. The same terminal question helper serves conversion, including piped input (W144, [#196](https://github.com/MuhammadMoustafa/xppautX/issues/196)).

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
