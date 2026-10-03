# XPPAUT findings

Bugs, wrong results and arbitrary limits found in XPPAUT 8.x while
modernizing it as xppautX, for the paper (maintainer, 2026-10-01). Each
entry: what XPPAUT does, the evidence, what xppautX does instead, and the
card and commit. Add an entry whenever a card finds one; keep the evidence
reproducible (a model, a command, the numbers).

Each finding cites the lines in XPPAUT's own source, never our refactored
files, so it is clear the behaviour is XPPAUT's and not ours. A citation is
a relative link into a local copy kept out of git (`reference/` is in
.gitignore): `reference/xppaut-8.0/` is the XPPAUT 8.0 source xppautX was
forked from (the tarball of Nov 2023, imported untouched at the start of
this repository), and `reference/xppaut-master/` is XPPAUT's GitHub master,
an older snapshot (2016). A citation reads `[file:lines](link into 8.0)`;
where the same code is in the older master a second link follows it
(`master`), else the citation says `master: none` (that code is different
or absent there). Code that is in 8.0 only because our own refactoring put it
there is not a finding and is not listed. Line numbers were checked against
the files when each entry was written ([W143](https://github.com/MuhammadMoustafa/xppautX/issues/195)).

What else differs from XPPAUT, beyond bugs, is
[xppautx-vs-xppaut.md](xppautx-vs-xppaut.md).

The entries are sections, not table rows: most need a paragraph of
evidence. The index gives one line each; its number links to the entry's
section, and every card (to its GitHub issue) and commit (to its GitHub
commit) is a link, in the index and the entries alike (maintainer,
2026-10-01).

| # | Area | Finding | Card |
|---|---|---|---|
| [1](#1-model-options) | Model options | `@ newt_iter`, `newt_tol`, `jac_eps`, `poistop` never take effect | [W119](https://github.com/MuhammadMoustafa/xppautX/issues/170) |
| [2](#2-model-options) | Model options | five option names unreachable, `parmin` and `yhi` wrong | [W119](https://github.com/MuhammadMoustafa/xppautX/issues/170) |
| [3](#3-names) | Names | names cut or overflowed at 10-11 characters, dialogs at 25 | [W76](https://github.com/MuhammadMoustafa/xppautX/issues/124) |
| [4](#4-model-options) | Model options | the options file never sets anything | [W139](https://github.com/MuhammadMoustafa/xppautX/issues/191) |
| [5](#5-model-files) | Model files | a missing `#include` file is skipped, the name is taken literally | [W139](https://github.com/MuhammadMoustafa/xppautX/issues/191) |
| [6](#6-daes) | DAEs | a DAE run steps over a fold onto another branch | [W127](https://github.com/MuhammadMoustafa/xppautX/issues/179) |
| [7](#7-auto-files-the-last-line-written-twice) | AUTO files | the last line of the `.s` dump written twice (`while(!feof)`) | [dc69f40](https://github.com/MuhammadMoustafa/xppautX/commit/dc69f40) |
| [8](#8-auto-files-everything-in-home) | AUTO files | AUTO's files all go to `$HOME`: sessions overwrite each other, a bad HOME ends the program | [dc69f40](https://github.com/MuhammadMoustafa/xppautX/commit/dc69f40) |
| [9](#9-auto-files-restart-files-and-open-handles) | AUTO files | restart files lost to Windows file semantics, a file handle leaked | [3f0eea8](https://github.com/MuhammadMoustafa/xppautX/commit/3f0eea8) |
| [10](#10-auto-restart-readers) | AUTO restart | restart readers trust unchecked reads; `fseek(-2)` on a text stream corrupts the heap | [137fd08](https://github.com/MuhammadMoustafa/xppautX/commit/137fd08) |
| [11](#11-auto-starts-from-the-last-integration-with-nothing-integrated) | AUTO | Start Periodic, Bdry Value and homoclinic starts read before the data | [W97](https://github.com/MuhammadMoustafa/xppautX/issues/146) |
| [12](#12-auto-homoclinic-test-functions-11-and-12-read-freed-memory) | AUTO | homoclinic test functions 11 and 12 read freed memory | [031eb17](https://github.com/MuhammadMoustafa/xppautX/commit/031eb17) |
| [13](#13-auto-the-plot-filter-for-two-parameter-diagrams) | AUTO | the two-parameter plot filter compares `icp2` with itself | [69cf3f0](https://github.com/MuhammadMoustafa/xppautX/commit/69cf3f0) |
| [14](#14-auto-stale-eigenvalues-at-a-runs-first-point) | AUTO | a run's first point shows the previous run's eigenvalues | [W15](https://github.com/MuhammadMoustafa/xppautX/issues/27) |
| [15](#15-auto-nan-left-in-the-models-parameters) | AUTO | a diverged run leaves NaN in the model's parameters | [W35a](https://github.com/MuhammadMoustafa/xppautX/issues/73) |
| [16](#16-auto-fit-of-a-flat-quantity) | AUTO | Fit of a flat quantity divides by an empty range | [W50](https://github.com/MuhammadMoustafa/xppautX/issues/98) |
| [17](#17-auto-and-eispack-call-exit) | AUTO, eispack | the numerics call `exit()` and end the program | [W63a](https://github.com/MuhammadMoustafa/xppautX/issues/111) |
| [18](#18-auto-files-saving-an-empty-diagram) | AUTO files | saving an empty diagram truncates the export file and leaks the handle | [8679803](https://github.com/MuhammadMoustafa/xppautX/commit/8679803) |
| [19](#19-fixed-buffers-and-tables-in-the-model-reader) | Model reader | fixed buffers cut or overflow lines, comments, file names, networks | [W29e](https://github.com/MuhammadMoustafa/xppautX/issues/53) |
| [20](#20-writes-past-the-end-and-unterminated-strings) | Model reader | writes past the end and unterminated strings | [W21](https://github.com/MuhammadMoustafa/xppautX/issues/34) |
| [21](#21-set-par-and-ic-files-read-by-guessing) | Files | `.set`, `.par`, `.ic` read with `atoi`/`atof`, applied as far as they read | [W125](https://github.com/MuhammadMoustafa/xppautX/issues/177) |
| [22](#22-adjoint-h-function-and-histogram-tables-on-moved-storage) | Analysis | adjoint, H function and histogram tables point into storage that moves | [W117](https://github.com/MuhammadMoustafa/xppautX/issues/168) |
| [23](#23-fftcon-reads-one-value-past-its-weight-table) | Networks | `fftcon` reads one value past its weight table | [W38](https://github.com/MuhammadMoustafa/xppautX/issues/81) |
| [24](#24-an-export-with-no-library-copies-uninitialised-memory) | Export | an export with no library copies uninitialised memory into the model | [d5eebae](https://github.com/MuhammadMoustafa/xppautX/commit/d5eebae) |
| [25](#25-an-output-stride-of-0-divides-by-zero) | Numerics | an output stride of 0 from a .set file or an `@ nout` line divides by zero on the next run | [W145](https://github.com/MuhammadMoustafa/xppautX/issues/197) |
| [26](#26-set-files-ignore-the-models-names) | Set files | equal counts allow another model's values: names ignored | [W153](https://github.com/MuhammadMoustafa/xppautX/issues/205) |
| [27](#27-rotation-and-boundary-value-movies-silently-drop-frames-when-full) | Kinescope | rotation and BVP movies ignore a full film buffer | [W133](https://github.com/MuhammadMoustafa/xppautX/issues/185) |
| [28](#28-a--comment-after-a-declaration-makes-names-of-its-words) | Model files | a `#` comment after a declaration makes parameters of its words (XPPAUT's own issue [Ermentrout/xppaut#11](https://github.com/Ermentrout/xppaut/issues/11)) | [W160](https://github.com/MuhammadMoustafa/xppautX/issues/212) |
| [29](#29-the-numbers-depend-on-the-cpu-and-on-the-compilers-fma) | Numerics | the numbers depend on the CPU and on whether the compiler targets FMA | [W159](https://github.com/MuhammadMoustafa/xppautX/issues/211) |
| [30](#30-auto-orbit-loading-trusts-file-dimensions-and-storage-capacity) | AUTO files | orbit dimensions and row counts can overrun fixed buffers and storage | [W155](https://github.com/MuhammadMoustafa/xppautX/issues/207) |
| [31](#31-model-options-bypass-method-suitability-and-ignore-unknown-methods) | Method selection | model options bypass suitability checks and ignore unknown methods | [W132](https://github.com/MuhammadMoustafa/xppautX/issues/184) |
| [32](#32-kinescope-cancel-still-saves-frames) | Kinescope | Cancel on the base filename still writes frames | [W130](https://github.com/MuhammadMoustafa/xppautX/issues/182) |
| [33](#33-cvode-gets-toler-as-its-absolute-and-atoler-as-its-relative-tolerance) | Numerics | `@ toler` is CVODE's absolute and `@ atoler` its relative tolerance, the reverse of the manual | [W34](https://github.com/MuhammadMoustafa/xppautX/issues/72) |

## 1. Model options

`@ newt_iter`, `@ newt_tol`, `@ jac_eps` and `@ poistop` in a model never
take effect: each option clears one "already set" flag (`NEWT_ITER`,
`NEWT_TOL`, `JAC_EPS`, `POISTOP`) while the defaults that follow test
another flag for the same value (`EVEC_ITER`, `EVEC_ERR`, `NEWT_ERR`, ...),
so the default overwrites what the model asked for.

- **XPPAUT 8.0:** [main.c:446](../reference/xppaut-8.0/main.c#L446) ([master 432](../reference/xppaut-master/main.c#L432)) calls load_eqn(), which reads the model's `@` options through set_option, then [main.c:455](../reference/xppaut-8.0/main.c#L455) ([master 441](../reference/xppaut-master/main.c#L441)) calls set_all_vals(), which applies the defaults. load_eqn.c set_option: [load_eqn.c:1544-1550](../reference/xppaut-8.0/load_eqn.c#L1544-L1550) ([master 1543-1549](../reference/xppaut-master/load_eqn.c#L1543-L1549)) JAC_EPS sets NEWT_ERR and clears notAlreadySet.JAC_EPS, [load_eqn.c:1552-1559](../reference/xppaut-8.0/load_eqn.c#L1552-L1559) ([master 1551-1558](../reference/xppaut-master/load_eqn.c#L1551-L1558)) NEWT_TOL sets EVEC_ERR and clears NEWT_TOL, [load_eqn.c:1561-1567](../reference/xppaut-8.0/load_eqn.c#L1561-L1567) ([master 1560-1566](../reference/xppaut-master/load_eqn.c#L1560-L1566)) NEWT_ITER sets EVEC_ITER and clears NEWT_ITER, [load_eqn.c:1831-1837](../reference/xppaut-8.0/load_eqn.c#L1831-L1837) ([master 1830-1836](../reference/xppaut-master/load_eqn.c#L1830-L1836)) POISTOP sets SOS and clears POISTOP. set_all_vals: [load_eqn.c:557](../reference/xppaut-8.0/load_eqn.c#L557) ([master 557](../reference/xppaut-master/load_eqn.c#L557)) tests notAlreadySet.SOS, [load_eqn.c:568](../reference/xppaut-8.0/load_eqn.c#L568) ([master 568](../reference/xppaut-master/load_eqn.c#L568)) EVEC_ITER, [load_eqn.c:569](../reference/xppaut-8.0/load_eqn.c#L569) ([master 569](../reference/xppaut-master/load_eqn.c#L569)) EVEC_ERR, [load_eqn.c:571](../reference/xppaut-8.0/load_eqn.c#L571) ([master 571](../reference/xppaut-master/load_eqn.c#L571)) NEWT_ERR and writes the defaults over them; [load_eqn.c:608](../reference/xppaut-8.0/load_eqn.c#L608) (master: none) has the JAC_EPS default commented out.
- **Evidence:** dae.ode, dae_ex3.ode, canonical/exdaebvp.ode give different results once the options apply; dae_ex3 fails at t=0.45 with the defaults and runs to t=3.65 with its own settings. Independent check ([W126](https://github.com/MuhammadMoustafa/xppautX/issues/178), docs/w126-dae-check.md, scipy Radau and RK4+Newton references): with the options applied dae.ode and exdaebvp.ode are 400-4000 times more accurate (errors 1e-7..1e-5 against 1e-3..3e-3, the constraint residual 1e-5 against 1e-3); dae_ex3's run to 3.65 is spurious, since the DAE has a fold at t=0.4507 and the old stop at 0.45 was right (finding 6).
- **xppautX:** one table drives every option, one flag per option.
- **Card:** [W119](https://github.com/MuhammadMoustafa/xppautX/issues/170) ([7013c6f](https://github.com/MuhammadMoustafa/xppautX/commit/7013c6f)).

## 2. Model options

`histlo2`, `histhi2`, `histbins2`, `histcol2`, `speccol2` are parsed by
nothing: unreachable. `parmin` shares XMAX's flag. `POIEXT` is never reset or
parsed. The `yhi` default writes the 3D box's `y_3d[0]` instead of
`y_3d[1]`.

- **XPPAUT 8.0:** load_eqn.c: msc() ([load_eqn.c:871-880](../reference/xppaut-8.0/load_eqn.c#L871-L880) ([master 870-879](../reference/xppaut-master/load_eqn.c#L870-L879))) matches a prefix, and set_option tests HISTLO ([load_eqn.c:2291](../reference/xppaut-8.0/load_eqn.c#L2291) ([master 2290](../reference/xppaut-master/load_eqn.c#L2290))), HISTHI ([load_eqn.c:2300](../reference/xppaut-8.0/load_eqn.c#L2300) ([master 2299](../reference/xppaut-master/load_eqn.c#L2299))), HISTBINS ([load_eqn.c:2309](../reference/xppaut-8.0/load_eqn.c#L2309) ([master 2308](../reference/xppaut-master/load_eqn.c#L2308))), HISTCOL ([load_eqn.c:2318](../reference/xppaut-8.0/load_eqn.c#L2318) ([master 2317](../reference/xppaut-master/load_eqn.c#L2317))), SPECCOL ([load_eqn.c:2366](../reference/xppaut-8.0/load_eqn.c#L2366) ([master 2327](../reference/xppaut-master/load_eqn.c#L2327))) before HISTLO2 ([load_eqn.c:2328](../reference/xppaut-8.0/load_eqn.c#L2328) (master: none)), HISTBINS2 ([load_eqn.c:2346](../reference/xppaut-8.0/load_eqn.c#L2346) (master: none)), SPECCOL2 ([load_eqn.c:2376](../reference/xppaut-8.0/load_eqn.c#L2376) ([master 2337](../reference/xppaut-master/load_eqn.c#L2337))) ..., so the shorter name always wins; PARMIN ([load_eqn.c:1998-2004](../reference/xppaut-8.0/load_eqn.c#L1998-L2004) ([master 1997-2003](../reference/xppaut-master/load_eqn.c#L1997-L2003))) tests and clears notAlreadySet.XMAX; set_all_vals [load_eqn.c:597](../reference/xppaut-8.0/load_eqn.c#L597) ([master 597](../reference/xppaut-master/load_eqn.c#L597)) writes the YHI default into y_3d[0] ([load_eqn.c:612-613](../reference/xppaut-8.0/load_eqn.c#L612-L613) ([master 611-612](../reference/xppaut-master/load_eqn.c#L611-L612)) show y_3d[0]/[1] are the 3D box's low/high).
- **Evidence:** [W119](https://github.com/MuhammadMoustafa/xppautX/issues/170)'s option table round-trip test.
- **xppautX:** each works, with its own flag.
- **Card:** [W119](https://github.com/MuhammadMoustafa/xppautX/issues/170) ([7013c6f](https://github.com/MuhammadMoustafa/xppautX/commit/7013c6f)).

## 3. Names

Names of variables, parameters and functions are cut or overflow at fixed
lengths, and so are the dialog fields that show them.

- **XPPAUT 8.0:** the name tables are `char uvar_names[MAXODE][12]` ([form_ode.c:45](../reference/xppaut-8.0/form_ode.c#L45) ([master 45](../reference/xppaut-master/form_ode.c#L45))) and `char upar_names[MAXPAR][11]` ([form_ode.c:47](../reference/xppaut-8.0/form_ode.c#L47) ([master 47](../reference/xppaut-master/form_ode.c#L47))), so a variable has at most 11 characters and a parameter 10; the parser copies a name into them with `strcpy` and no length check ([form_ode.c:711](../reference/xppaut-8.0/form_ode.c#L711) ([master 711](../reference/xppaut-master/form_ode.c#L711)) a parameter, [form_ode.c:742](../reference/xppaut-8.0/form_ode.c#L742) ([master 742](../reference/xppaut-master/form_ode.c#L742)) a Markov variable), so a longer name overflows into the next row's name; the parser's own copy of a name is `char name[20]` ([form_ode.c:1362](../reference/xppaut-8.0/form_ode.c#L1362) ([master 1362](../reference/xppaut-master/form_ode.c#L1362)), [form_ode.c:1107](../reference/xppaut-8.0/form_ode.c#L1107) ([master 1107](../reference/xppaut-master/form_ode.c#L1107)), [form_ode.c:602](../reference/xppaut-8.0/form_ode.c#L602) ([master 602](../reference/xppaut-master/form_ode.c#L602))); `MAX_LEN_SBOX` is 25 ([graf_par.h:14](../reference/xppaut-8.0/graf_par.h#L14) ([master 14](../reference/xppaut-master/graf_par.h#L14))), the width of every dialog field.
- **Evidence:** tools/models/longnames.ode (200-character names), autocheck's `names` section, tests/test_names.cpp.
- **xppautX:** no limit; names are `std::string`, fixed-width columns shorten with `~`.
- **Card:** [W76](https://github.com/MuhammadMoustafa/xppautX/issues/124) ([3e63883](https://github.com/MuhammadMoustafa/xppautX/commit/3e63883)).

## 4. Model options

The options file (`option name` in a model, else `default.opt`), documented
as setting the plot variables, axes, method, total, dt, bounds, tolerances
and window, never sets any of them: set_all_vals gives every one its
default (clearing its "not already set" flag) before it reads the file, and
read_defaults reads a value only while the flag is still set; a line it
skips is not read, so the lines after it would shift too.

- **XPPAUT 8.0:** load_eqn.c: set_all_vals ([load_eqn.c:543](../reference/xppaut-8.0/load_eqn.c#L543) ([master 543](../reference/xppaut-master/load_eqn.c#L543))) writes the defaults ([load_eqn.c:607](../reference/xppaut-8.0/load_eqn.c#L607) ([master 607](../reference/xppaut-master/load_eqn.c#L607)) DT, [load_eqn.c:617](../reference/xppaut-8.0/load_eqn.c#L617) ([master 616](../reference/xppaut-master/load_eqn.c#L616)) TEND, [load_eqn.c:619](../reference/xppaut-8.0/load_eqn.c#L619) ([master 618](../reference/xppaut-master/load_eqn.c#L618)) IXPLT, ...) and only then opens the file ([load_eqn.c:641-643](../reference/xppaut-8.0/load_eqn.c#L641-L643) ([master 640-642](../reference/xppaut-master/load_eqn.c#L640-L642))); read_defaults ([load_eqn.c:702](../reference/xppaut-8.0/load_eqn.c#L702) ([master 701](../reference/xppaut-master/load_eqn.c#L701))) tests the same flags ([load_eqn.c:725](../reference/xppaut-8.0/load_eqn.c#L725) ([master 724](../reference/xppaut-master/load_eqn.c#L724)) IXPLT ... [load_eqn.c:747](../reference/xppaut-8.0/load_eqn.c#L747) ([master 746](../reference/xppaut-master/load_eqn.c#L746)) YHI) and reads a line only when one is set; [load_eqn.c:728](../reference/xppaut-8.0/load_eqn.c#L728) ([master 727](../reference/xppaut-master/load_eqn.c#L727)) (AXES) and [load_eqn.c:741](../reference/xppaut-8.0/load_eqn.c#L741) ([master 740](../reference/xppaut-master/load_eqn.c#L740)) (DTMAX) clear the wrong flags (PaperWhite, DTMIN).
- **xppautX:** dropped: an `option` line is refused at load (file and line), `default.opt` is never looked for, and the settings go in `@` lines.
- **Card:** [W139](https://github.com/MuhammadMoustafa/xppautX/issues/191) ([970c1a2](https://github.com/MuhammadMoustafa/xppautX/commit/970c1a2)).

## 5. Model files

A model whose `#include` file cannot be opened still loads: XPPAUT prints
"Cant open include file" and goes on without it, so the functions and
settings the file held are missing and the model runs with defaults, exit
status 0 (the `-include` flag's file, by contrast, exits with -1). The file
name is everything after `#include `, so `#include <opts.inc>` and
`#include "opts.inc"` look for a file literally named with the brackets or
quotes, and so are skipped the same way. The name is opened as typed (fopen
of the bare name), so it is looked for in the working folder, not the
model's: a model opened from another folder does not find its include.

- **XPPAUT 8.0:** form_ode.c: if_include_file ([form_ode.c:1274-1296](../reference/xppaut-8.0/form_ode.c#L1274-L1296) ([master 1274-1296](../reference/xppaut-master/form_ode.c#L1274-L1296))) takes the text after the first blank as the name; do_new_parser [form_ode.c:1414](../reference/xppaut-8.0/form_ode.c#L1414) ([master 1414](../reference/xppaut-master/form_ode.c#L1414)) (`fopen(newfile,"r")`), [form_ode.c:1415-1418](../reference/xppaut-8.0/form_ode.c#L1415-L1418) ([master 1415-1418](../reference/xppaut-master/form_ode.c#L1415-L1418)) (`fnew==NULL`: plintf, `continue`); the flag's own [form_ode.c:1387-1391](../reference/xppaut-8.0/form_ode.c#L1387-L1391) ([master 1387-1391](../reference/xppaut-master/form_ode.c#L1387-L1391)) (plintf, `exit(-1)`).
- **xppautX:** an include is looked for in the folder of the file that holds the line; a file that cannot be read stops the load with that file and the `#include` line; so does the `-include` flag's (relative to the working folder).
- **Card:** [W139](https://github.com/MuhammadMoustafa/xppautX/issues/191) ([82c67a5](https://github.com/MuhammadMoustafa/xppautX/commit/82c67a5), [bc2cfaa](https://github.com/MuhammadMoustafa/xppautX/commit/bc2cfaa)).

## 6. DAEs

A DAE run steps over a fold onto another branch of solutions, or onto no
solution at all: the algebraic solve accepts whatever root Newton reaches
from the last solution, with nothing to say it is on the same branch, and it
accepts a point where the Newton update was small without checking that the
equations hold there. The manual's own dae_ex3 "exploits numerical errors to
get the DAE solver to go beyond where it should go legally".

- **XPPAUT 8.0:** dae_fun.c solve_dae: [dae_fun.c:230-243](../reference/xppaut-8.0/dae_fun.c#L230-L243) ([master 230-243](../reference/xppaut-master/dae_fun.c#L230-L243)) accept any y whose residual is below the tolerance, however far Newton went from the last solution; [dae_fun.c:256-261](../reference/xppaut-8.0/dae_fun.c#L256-L261) ([master 256-261](../reference/xppaut-master/dae_fun.c#L256-L261)) check only for an exactly singular Jacobian; [dae_fun.c:273-281](../reference/xppaut-8.0/dae_fun.c#L273-L281) ([master 273-281](../reference/xppaut-master/dae_fun.c#L273-L281)) accept y when the update is below the tolerance, without the residual (do_daes [dae_fun.c:202](../reference/xppaut-8.0/dae_fun.c#L202) ([master 202](../reference/xppaut-master/dae_fun.c#L202)), "accepts a no change error!").
- **Evidence:** dae_ex3.ode (w'=v, 0=v(1-v^2)-w, v(0)=1) has a fold at v=1/sqrt(3), t*=0.450694, past which its branch has no solution; with its own NEWT_ITER=1000 (set in the numerics menu in XPPAUT, finding 1) Newton wanders for 318 iterations at t=0.46 and lands on the branch v=-1.16, and the run goes on to t=3.65 with v jumping between branches and the constraint off by up to 0.031 (docs/w126-dae-check.md, scipy and RK4+Newton references).
- **xppautX:** a solution is accepted only where the residual is within the tolerance, and only while every Jacobian Newton factors from the last solution keeps that solution's determinant sign; a sign change is a fold, and the run stops there with "No solution of the algebraic equations past t=0.45: their Jacobian changed sign, a fold where this branch of solutions ends", its rows kept to t=0.45.
- **Card:** [W127](https://github.com/MuhammadMoustafa/xppautX/issues/179) ([b70f8f6](https://github.com/MuhammadMoustafa/xppautX/commit/b70f8f6)).

## 7. AUTO files: the last line written twice

Saving AUTO's session (File > Save auto) copies the `.s` solution file into
the `.auto` file, and loading one copies it back, with `while(!feof(f))`
around `fgets`/`fputs`. `feof` is still false going into the read that hits
the end of the file, so that read fails, leaves the buffer as the previous
line, and the loop writes that line again: every saved `.auto` has its last
`.s` line twice, and the same on loading.

- **XPPAUT 8.0:** auto_nox.c save_q_file ([auto_nox.c:2832-2837](../reference/xppaut-8.0/auto_nox.c#L2832-L2837) ([master 2803-2808](../reference/xppaut-master/auto_nox.c#L2803-L2808))) and make_q_file ([auto_nox.c:2853-2858](../reference/xppaut-8.0/auto_nox.c#L2853-L2858) ([master 2824-2829](../reference/xppaut-master/auto_nox.c#L2824-L2829))), neither checks fgets' result.
- **Evidence:** tools/models/lecar_diagram.auto, the saved diagram autocheck compares byte for byte, had the duplicated line in its reference; it is one line shorter now.
- **xppautX:** both stop on fgets' own NULL.
- **Card:** issue [#11](https://github.com/MuhammadMoustafa/xppautX/issues/11) ([dc69f40](https://github.com/MuhammadMoustafa/xppautX/commit/dc69f40)).

## 8. AUTO files: everything in `$HOME`

AUTO's working files (fort.3, fort.7, fort.8, fort.9, the swap file and the
model's `.s`, `.b`, `.d`) are all made in `$HOME`, whatever the model's
folder, with the same names for every model and every running instance. Two
models open at once, or two windows on the same model, overwrite each
other's files, so a grab can fail or return NaN. `$HOME` is only checked for
being unset: a `$HOME` that is missing or cannot be written makes fopen()
fail inside the library, which calls exit(1), ending the whole program and
losing the session. The names are built with `sprintf` into 200-byte arrays,
so a long `$HOME` plus a long model name overflows them.

- **XPPAUT 8.0:** auto_nox.c create_auto_file_name ([auto_nox.c:587-594](../reference/xppaut-8.0/auto_nox.c#L587-L594) ([master 581-587](../reference/xppaut-master/auto_nox.c#L581-L587))) and open_auto ([auto_nox.c:609-620](../reference/xppaut-8.0/auto_nox.c#L609-L620) ([master 601-612](../reference/xppaut-master/auto_nox.c#L601-L612))) take `getenv("HOME")` and fall back to the model's folder only when it is NULL; the arrays are `char this_auto_file[200]`, `fort3`, `fort7`, `fort8`, `fort9`, `TMPSWAP` ([auto_nox.c:211-217](../reference/xppaut-8.0/auto_nox.c#L211-L217) ([master 207-212](../reference/xppaut-master/auto_nox.c#L207-L212))); the failed open of fort.8 ([autlib1.c:3074-3081](../reference/xppaut-8.0/autlib1.c#L3074-L3081) ([master 3071-3078](../reference/xppaut-master/autlib1.c#L3071-L3078)) and [autlib1.c:7172-7179](../reference/xppaut-8.0/autlib1.c#L7172-L7179) ([master 7169-7176](../reference/xppaut-master/autlib1.c#L7169-L7176))) is `exit(1)`.
- **Evidence:** two xppautX processes on lecar.ode, each Run > Steady state: they share fort.8 and the `.s` of the first run is rewritten by the second (issue [#11](https://github.com/MuhammadMoustafa/xppautX/issues/11)). With `HOME` pointing at a missing folder XPPAUT's AUTO prints "Error:  Could not open fort.8" and the process ends ([5d71fb1](https://github.com/MuhammadMoustafa/xppautX/commit/5d71fb1), reproduced on Windows).
- **xppautX:** each session gets its own scratch folder (`xppautoX-<pid>-N`, removed at exit; leftovers of dead processes at start: [W19](https://github.com/MuhammadMoustafa/xppautX/issues/32)), results go next to the model, a failed open is an error shown by the command.
- **Card:** issue [#11](https://github.com/MuhammadMoustafa/xppautX/issues/11) ([dc69f40](https://github.com/MuhammadMoustafa/xppautX/commit/dc69f40)), [5d71fb1](https://github.com/MuhammadMoustafa/xppautX/commit/5d71fb1), [W19](https://github.com/MuhammadMoustafa/xppautX/issues/32).

## 9. AUTO files: restart files and open handles

The helpers that move AUTO's files around assume POSIX behaviour and never
check what they open:

- `renamef` is a bare `rename()`: on Windows it fails when the destination exists, so `<model>.s` was silently kept and fort.8 fell behind ([auto_nox.c:452-456](../reference/xppaut-8.0/auto_nox.c#L452-L456) ([master 446-450](../reference/xppaut-master/auto_nox.c#L446-L450))).
- `copyf` and `appendf` use the result of `fopen` without testing it: a failed open reads or writes through a NULL `FILE *`, which left fort.3 (the restart file) at 0 bytes and then "Restart label N not found" ([auto_nox.c:482-497](../reference/xppaut-8.0/auto_nox.c#L482-L497) ([master 476-490](../reference/xppaut-master/auto_nox.c#L476-L490)), [auto_nox.c:500-529](../reference/xppaut-8.0/auto_nox.c#L500-L529) ([master 494-522](../reference/xppaut-master/auto_nox.c#L494-L522))).
- fp8 (fort.8) is opened once per run ([autlib1.c:3074-3081](../reference/xppaut-8.0/autlib1.c#L3074-L3081) ([master 3071-3078](../reference/xppaut-master/autlib1.c#L3071-L3078))) and the only `fclose(fp8)` is inside a comment ([auto_nox.c:544-548](../reference/xppaut-8.0/auto_nox.c#L544-L548) ([master 538-542](../reference/xppaut-master/auto_nox.c#L538-L542))), so the handle leaks every run and, on Windows, which will not rename or remove an open file, fort.8 stayed beside `<model>.s`.
- the "Restart label not found" path returns without closing fp3, fp7, fp9 ([gogoauto.c:66-76](../reference/xppaut-8.0/gogoauto.c#L66-L76) ([master 66-76](../reference/xppaut-master/gogoauto.c#L66-L76)), closed at [gogoauto.c:111-113](../reference/xppaut-8.0/gogoauto.c#L111-L113) ([master 111-113](../reference/xppaut-master/gogoauto.c#L111-L113))), so one failure on Windows poisoned every later AUTO run.

- **Evidence:** on Windows, Run, Grab, Run again: before the fix fort.8 stayed beside lecar.ode.s and the second run failed with "Restart label not found"; after, only the three renamed files remain (commit [860436c](https://github.com/MuhammadMoustafa/xppautX/commit/860436c)'s check with build/grabtest.py against the MinGW build).
- **xppautX:** one file module (`xpp::files`) with checked copy/move/prepend, AUTO's files closed on every path (fort.8 is an `xpp::UniqueFile`, [W117](https://github.com/MuhammadMoustafa/xppautX/issues/168)).
- **Card:** [3f0eea8](https://github.com/MuhammadMoustafa/xppautX/commit/3f0eea8), [860436c](https://github.com/MuhammadMoustafa/xppautX/commit/860436c), [W117](https://github.com/MuhammadMoustafa/xppautX/issues/168) ([5ae5282](https://github.com/MuhammadMoustafa/xppautX/commit/5ae5282)).

## 10. AUTO restart readers

AUTO reads a restart label from fort.3 with `fscanf` calls whose results are
never tested, and uses the values it read as sizes: `readlb` reads `nar`
and loops `u[i]` for `i < nar-1`, the `stpnp*` readers read `ntsr`, `ncolrs`
and `nparr` the same way. A truncated or damaged restart file hands stack
garbage to a loop bound or a malloc size, and a short read is not reported.

Separately, `findlb` walks back to the start of the label line with
`fseek(fp3,-2,SEEK_CUR)` on a text stream. On Windows the C runtime
translates CR LF, a relative seek on a text stream is undefined there, the
stream is left mid-line, and the following `readlb` parsed the header from the
wrong place (once `nar = -444604564`) and wrote that many values past the
two-element `u`: heap corruption (0xC0000374) on Run after a Grab.

- **XPPAUT 8.0:** autlib1.c readlb ([autlib1.c:4562-4574](../reference/xppaut-8.0/autlib1.c#L4562-L4574) ([master 4559-4571](../reference/xppaut-master/autlib1.c#L4559-L4571))), likewise the size peek in rsptbv and stpnbv; autlib3.c stpnpl ([autlib3.c:2523-2535](../reference/xppaut-8.0/autlib3.c#L2523-L2535) ([master 2524-2536](../reference/xppaut-master/autlib3.c#L2524-L2536))) and the same lines of stpnpd, stpntr, stpnpo, stpnbl; the seek in findlb [autlib1.c:4533-4536](../reference/xppaut-8.0/autlib1.c#L4533-L4536) ([master 4530-4533](../reference/xppaut-master/autlib1.c#L4530-L4533)).
- **Evidence:** servercheck's Run, Grab, Run sequence (added by [137fd08](https://github.com/MuhammadMoustafa/xppautX/commit/137fd08)) dies with a corrupted heap on Windows with the old code, usually and not always; an ASan build on Linux is clean, since Linux does not translate.
- **xppautX:** every read is checked and a short read stops the load; the label's position is remembered with `ftell` and restored with an absolute seek.
- **Card:** [137fd08](https://github.com/MuhammadMoustafa/xppautX/commit/137fd08), [e97ac2f](https://github.com/MuhammadMoustafa/xppautX/commit/e97ac2f), [c837e26](https://github.com/MuhammadMoustafa/xppautX/commit/c837e26).

## 11. AUTO: starts from the last integration with nothing integrated

Start > Periodic, Bdry Value, Homoclinic and hEteroclinic take the orbit and
period from the last integration. With no row stored, the period is read from
`storage[0][-1]`, one element before the table.

- **XPPAUT 8.0:** auto_nox.c get_start_period ([auto_nox.c:1721-1725](../reference/xppaut-8.0/auto_nox.c#L1721-L1725) ([master 1709-1713](../reference/xppaut-master/auto_nox.c#L1709-L1713)), `*p=storage[0][storind-1]`), called from autpp.c ([autpp.c:78](../reference/xppaut-8.0/autpp.c#L78) ([master 76](../reference/xppaut-master/autpp.c#L76))).
- **Evidence:** asancheck reported the heap-buffer-overflow in xppautX's port of the same line; without the sanitizer the program read garbage as the period (a fresh model, Start > Periodic before any integration).
- **xppautX:** refuses with "Integrate first" when fewer than 2 rows are stored.
- **Card:** [W97](https://github.com/MuhammadMoustafa/xppautX/issues/146) ([aacb087](https://github.com/MuhammadMoustafa/xppautX/commit/aacb087)).

## 12. AUTO: homoclinic test functions 11 and 12 read freed memory

`psiho` allocates `f0` and `f1`, then frees both before the `switch` on the
test function number, and the targets for test functions 11 and 12 (orbit
flips) still read `f1[]` and `f0[]` after the free.

- **XPPAUT 8.0:** autlib5.c psiho: the frees at [autlib5.c:1488-1489](../reference/xppaut-8.0/autlib5.c#L1488-L1489) ([master 1488-1489](../reference/xppaut-master/autlib5.c#L1488-L1489)), the reads at [autlib5.c:1594-1599](../reference/xppaut-8.0/autlib5.c#L1594-L1599) ([master 1594-1599](../reference/xppaut-master/autlib5.c#L1594-L1599)) (`L11`, `f1`) and [autlib5.c:1604-1609](../reference/xppaut-8.0/autlib5.c#L1604-L1609) ([master 1604-1609](../reference/xppaut-master/autlib5.c#L1604-L1609)) (`L12`, `f0`).
- **Evidence:** gcc's `-Wuse-after-free` reports both; any homoclinic run that computes test function 11 or 12 reads freed memory, giving garbage in the test function's value depending on the allocator.
- **xppautX:** the buffer an upcoming label still needs is freed after its last use.
- **Card:** [031eb17](https://github.com/MuhammadMoustafa/xppautX/commit/031eb17).

## 13. AUTO: the plot filter for two-parameter diagrams

`check_plot_type` is meant to keep the points whose second parameter is the
plotted one, and compares `icp2` with itself.

- **XPPAUT 8.0:** auto_nox.c check_plot_type [auto_nox.c:1231](../reference/xppaut-8.0/auto_nox.c#L1231) ([master 1221](../reference/xppaut-master/auto_nox.c#L1221)) (`flag2>0 && icp2!=icp2`, never true), so a diagram with two-parameter branches is never filtered by its second parameter.
- **Evidence:** latent in the examples (flag2 is 0 for the plot type they use; the saved autocheck diagram is the same before and after the fix).
- **xppautX:** compares with `Auto.icp2`, like the icp1 test above it.
- **Card:** [69cf3f0](https://github.com/MuhammadMoustafa/xppautX/commit/69cf3f0).

## 14. AUTO: stale eigenvalues at a run's first point

AUTO does not check stability at a run's first point. XPPAUT copies one global
slot of eigenvalues (`my_ev`, filled by the previous point's check) into every
stored point without testing whose they are, so the first point is stored
with the previous run's last values (zeros in a new session): numbers that
look real in the info strip, the stability circle and saved `.auto` files.

- **XPPAUT 8.0:** autevd.c addbif [autevd.c:170-197](../reference/xppaut-8.0/autevd.c#L170-L197) ([master 170-196](../reference/xppaut-master/autevd.c#L170-L196)) passes `my_ev.evr`, `my_ev.evi` to add_point and add_diagram; the check that the slot belongs to the point is commented out ([autevd.c:178](../reference/xppaut-8.0/autevd.c#L178) ([master 177](../reference/xppaut-master/autevd.c#L177))).
- **Evidence:** autocheck's cases: a run's first point is "not computed"; a restart from a label of the same kind carries the label's values; a Hopf start is "not computed".
- **xppautX:** one module (`auto_stability`) gives a point's values only for the point they belong to, else "not computed".
- **Card:** [W15](https://github.com/MuhammadMoustafa/xppautX/issues/27) ([13ea852](https://github.com/MuhammadMoustafa/xppautX/commit/13ea852)).

## 15. AUTO: NaN left in the model's parameters

AUTO's `func` and `bcnd` write the continuation parameters into the model's
`constants[]` at every right-hand side call, and a stopped run leaves them
there, with no test that they are finite. A periodic continuation that fails
to converge leaves a parameter at `-nan`, which the next integration, the
parameter box and the saved `.set` then use.

- **XPPAUT 8.0:** autpp.c func [autpp.c:35-37](../reference/xppaut-8.0/autpp.c#L35-L37) ([master 34-35](../reference/xppaut-master/autpp.c#L34-L35)) and bcnd [autpp.c:119-121](../reference/xppaut-8.0/autpp.c#L119-L121) ([master 117-119](../reference/xppaut-master/autpp.c#L117-L119)) (`constants[...]=par[i]`); auto_nox.c Grab ([auto_nox.c:344-345](../reference/xppaut-8.0/auto_nox.c#L344-L345) ([master 339-340](../reference/xppaut-master/auto_nox.c#L339-L340))) takes the diagram point's values the same way.
- **Evidence:** QA SCI-001 (2026-09-26): a failing periodic continuation of a model with parameter Iapp left Iapp at `-nan`.
- **xppautX:** a non-finite value is never written into the model (the old value is kept with a warning), and the parameters are restored when a run stops.
- **Card:** [W35a](https://github.com/MuhammadMoustafa/xppautX/issues/73) ([9bdb7a5](https://github.com/MuhammadMoustafa/xppautX/commit/9bdb7a5)).

## 16. AUTO: Fit of a flat quantity

AUTO's Fit sets the window to the data's minimum and maximum, so a flat
quantity (a steady branch's period, all 0) gives `ymin == ymax`, and
`IYVal` divides by that empty range: `(int)` of infinity, then `300 - INT_MIN`,
a signed overflow. The plot window's own Fit widens an empty range with
`check_val`; AUTO's does not.

- **XPPAUT 8.0:** auto_nox.c auto_fit [auto_nox.c:960-967](../reference/xppaut-8.0/auto_nox.c#L960-L967) ([master 950-957](../reference/xppaut-master/auto_nox.c#L950-L957)), IYVal [auto_nox.c:437-441](../reference/xppaut-8.0/auto_nox.c#L437-L441) ([master 431-435](../reference/xppaut-master/auto_nox.c#L431-L435)); the plot window's check_val ([graf_par.c:289](../reference/xppaut-8.0/graf_par.c#L289) ([master 289](../reference/xppaut-master/graf_par.c#L289))) is not used.
- **Evidence:** UBSan stopped xppautX's server in autocheck's two-view check (Fit on the period of a steady branch), reported as a signed integer overflow.
- **xppautX:** AUTO's Fit goes through the same `check_val`.
- **Card:** [W50](https://github.com/MuhammadMoustafa/xppautX/issues/98) review ([4218f49](https://github.com/MuhammadMoustafa/xppautX/commit/4218f49)).

## 17. AUTO and eispack call `exit()`

Numerical routines end the whole program instead of reporting a failure:
the AUTO library on `NPAR > NPARX`, `NCOL > 7`, a domain error in `dhhpr`, a
zero pivot ("Division by Zero, exiting"), a failed open of fort.8, and eispack
on a failed argument check. A parameter or setting that AUTO cannot take
therefore loses the session (parameters, data, everything) with no message the
front end can show.

- **XPPAUT 8.0:** autlib1.c [autlib1.c:370-377](../reference/xppaut-8.0/autlib1.c#L370-L377) ([master 370-377](../reference/xppaut-master/autlib1.c#L370-L377)), [autlib1.c:3320-3326](../reference/xppaut-8.0/autlib1.c#L3320-L3326) ([master 3317-3322](../reference/xppaut-master/autlib1.c#L3317-L3322)), [autlib1.c:4352-4358](../reference/xppaut-8.0/autlib1.c#L4352-L4358) ([master 4349-4355](../reference/xppaut-master/autlib1.c#L4349-L4355)); autlib4.c [autlib4.c:472-486](../reference/xppaut-8.0/autlib4.c#L472-L486) ([master 474-485](../reference/xppaut-master/autlib4.c#L474-L485)), [autlib4.c:637-652](../reference/xppaut-8.0/autlib4.c#L637-L652) ([master 637-652](../reference/xppaut-master/autlib4.c#L637-L652)); autlib2.c [autlib2.c:300-308](../reference/xppaut-8.0/autlib2.c#L300-L308) ([master 300-308](../reference/xppaut-master/autlib2.c#L300-L308)); eispack.c [eispack.c:3922-3925](../reference/xppaut-8.0/eispack.c#L3922-L3925) ([master 3915-3918](../reference/xppaut-master/eispack.c#L3915-L3918)).
- **Evidence:** `Ncol` above 7 in AUTO's Numerics: XPPAUT's library prints "Dimension exceeded : NCOL=... maximum=7" and exits with status 1 ([autlib1.c:3320-3326](../reference/xppaut-8.0/autlib1.c#L3320-L3326) ([master 3317-3322](../reference/xppaut-master/autlib1.c#L3317-L3322))); autocheck's sessiondiagram section checks that xppautX refuses it at its line instead.
- **xppautX:** each is an `xpp::Error` returned to the command, which shows it once; the program keeps running.
- **Card:** [W63a](https://github.com/MuhammadMoustafa/xppautX/issues/111) ([5651fe5](https://github.com/MuhammadMoustafa/xppautX/commit/5651fe5)), [W63d](https://github.com/MuhammadMoustafa/xppautX/issues/157).

## 18. AUTO files: saving an empty diagram

The diagram exports (`write_info_out`, `write_init_data_file`,
`write_pts`) open their file with `fopen(...,"w")` and only then test
whether the diagram has any point (`d->next==NULL`), and return without
closing the file: an existing file of that name is emptied and the handle
leaks. `save_diagram` returns -1 for an empty diagram and `save_auto` then
returns without a word, leaving a `.auto` with numerics and no orbit data.

- **XPPAUT 8.0:** diagram.c write_info_out [diagram.c:246-253](../reference/xppaut-8.0/diagram.c#L246-L253) ([master 246-253](../reference/xppaut-master/diagram.c#L246-L253)), write_init_data_file [diagram.c:359-367](../reference/xppaut-8.0/diagram.c#L359-L367) ([master 359-367](../reference/xppaut-master/diagram.c#L359-L367)), write_pts [diagram.c:427-436](../reference/xppaut-8.0/diagram.c#L427-L436) ([master 426-435](../reference/xppaut-master/diagram.c#L426-L435)), save_diagram [diagram.c:583-590](../reference/xppaut-8.0/diagram.c#L583-L590) ([master 582-589](../reference/xppaut-master/diagram.c#L582-L589)); auto_nox.c save_auto [auto_nox.c:2736-2764](../reference/xppaut-8.0/auto_nox.c#L2736-L2764) ([master 2707-2734](../reference/xppaut-master/auto_nox.c#L2707-L2734)).
- **Evidence:** write the diagram to `allinfo.dat`, then again with nothing computed: XPPAUT leaves `allinfo.dat` empty.
- **xppautX:** the file is opened (through `xpp::Writer`, temp then rename) only after there is something to write, so an existing file is left alone; an empty diagram is said to the user. W129 ([#181](https://github.com/MuhammadMoustafa/xppautX/issues/181)) puts this availability check before the name dialog and uses the common save permission and checked commit.
- **Card:** [W7b](https://github.com/MuhammadMoustafa/xppautX/issues/19) ([8679803](https://github.com/MuhammadMoustafa/xppautX/commit/8679803)), [dc69f40](https://github.com/MuhammadMoustafa/xppautX/commit/dc69f40).

## 19. Fixed buffers and tables in the model reader

XPPAUT reads a model into fixed buffers with `strcpy`, `strcat`, `sprintf`
and `fgets`, none of which checks a length. Each of these either cuts a
long line or name or writes past its buffer (the sizes are XPPAUT's):

- a logical line is read in pieces of 1023 characters (`fgets(temp,MAXEXPLEN,..)`), a longer line comes in as several, and its continuation lines (a trailing backslash) are joined with `strcat` into a 1024-byte buffer with no check ([form_ode.c:2629-2660](../reference/xppaut-8.0/form_ode.c#L2629-L2660) ([master 2629-2660](../reference/xppaut-master/form_ode.c#L2629-L2660))); the first line the same ([form_ode.c:406](../reference/xppaut-8.0/form_ode.c#L406) ([master 406](../reference/xppaut-master/form_ode.c#L406)));
- `save_eqn` keeps at most `MAXLINES` (5000) lines: the check is `NLINES>MAXLINES` ([form_ode.c:408](../reference/xppaut-8.0/form_ode.c#L408) ([master 408](../reference/xppaut-master/form_ode.c#L408)), so 5000 passes and the store overflows by one) and the two places that store lines after it ([form_ode.c:447](../reference/xppaut-8.0/form_ode.c#L447) ([master 447](../reference/xppaut-master/form_ode.c#L447)), [form_ode.c:2644](../reference/xppaut-8.0/form_ode.c#L2644) ([master 2644](../reference/xppaut-master/form_ode.c#L2644))) never check;
- the `#include` name is copied into a buffer with no bound ([form_ode.c:1274-1296](../reference/xppaut-8.0/form_ode.c#L1274-L1296) ([master 1274-1296](../reference/xppaut-master/form_ode.c#L1274-L1296))), a comment's text and action into 256-byte buffers ([form_ode.c:3024-3060](../reference/xppaut-8.0/form_ode.c#L3024-L3060) ([master 3024-3060](../reference/xppaut-master/form_ode.c#L3024-L3060)), an action over 255 characters overflows and is left unterminated), an array subscript into a 20-byte `num` ([form_ode.c:2841-2880](../reference/xppaut-8.0/form_ode.c#L2841-L2880) ([master 2841-2880](../reference/xppaut-master/form_ode.c#L2841-L2880)), and a `[` with no `]` reads on past the end of the line);
- a network's arguments are copied from tokens into 256-byte buffers ([simplenet.c:1666-1713](../reference/xppaut-8.0/simplenet.c#L1666-L1713) ([master 1666-1713](../reference/xppaut-master/simplenet.c#L1666-L1713)): `import(...)` with no `(` runs past the text, more than `MAXW` (50) weights overflow `tname`); a Gillespie chain of over 1000 reactions overflows `r[1000]` and `gcom` ([markov.c:461](../reference/xppaut-8.0/markov.c#L461) ([master 461](../reference/xppaut-master/markov.c#L461)), [simplenet.c:1062](../reference/xppaut-8.0/simplenet.c#L1062) ([master 1062](../reference/xppaut-master/simplenet.c#L1062)));
- file names: `fnx[256]` for a parameter or initial-condition file ([lunch-new.c:386](../reference/xppaut-8.0/lunch-new.c#L386) ([master 386](../reference/xppaut-master/lunch-new.c#L386)), [lunch-new.c:434](../reference/xppaut-8.0/lunch-new.c#L434) ([master 434](../reference/xppaut-master/lunch-new.c#L434))) takes the name with no bound; `-outfile` is copied into `batchout[256]`/`UserOUTFILE[256]` ([main.c:127-128](../reference/xppaut-8.0/main.c#L127-L128) ([master 124-125](../reference/xppaut-master/main.c#L124-L125))) with `sprintf(batchout,argv[i+1])` ([comline.c:307-308](../reference/xppaut-8.0/comline.c#L307-L308) ([master 305-306](../reference/xppaut-master/comline.c#L305-L306))): the argument is the format string, so a name with `%n` or `%s` crashes the program (`xppautX m.ode -silent -outfile out%n%s.dat` aborts on the old code), and a name over 255 characters runs past the end; AUTO's file names are 200 bytes (finding 8);
- the Volterra history table `Memory` is defined with `MAXKER` (50) entries ([parserslow2.c:92](../reference/xppaut-8.0/parserslow2.c#L92) ([master 92](../reference/xppaut-master/parserslow2.c#L92))) but allocated one per variable ([volterra2.c:118](../reference/xppaut-8.0/volterra2.c#L118) ([master 118](../reference/xppaut-master/volterra2.c#L118)), declared `Memory[MAXODE]` at [volterra2.c:40](../reference/xppaut-8.0/volterra2.c#L40) ([master 40](../reference/xppaut-master/volterra2.c#L40))): a Volterra model with more than 50 variables writes past the table;
- names: finding 3.

- **Evidence:** ASan/valgrind reports in xppautX's port of the same code ([W21](https://github.com/MuhammadMoustafa/xppautX/issues/34), [W29e](https://github.com/MuhammadMoustafa/xppautX/issues/53)), the `-outfile` crash by hand ([1f2a893](https://github.com/MuhammadMoustafa/xppautX/commit/1f2a893)), the Volterra table by reading the two definitions ([76c3950](https://github.com/MuhammadMoustafa/xppautX/commit/76c3950)); no example hits the limits.
- **xppautX:** lines, names, file names and arguments are `std::string`/`std::vector`, the parser's line has no limit, a missing `]` or `)` is an error, a model with 5001 lines loads, and a network chain of any length works.
- **Card:** [W29e](https://github.com/MuhammadMoustafa/xppautX/issues/53) ([23bf321](https://github.com/MuhammadMoustafa/xppautX/commit/23bf321), [70dc442](https://github.com/MuhammadMoustafa/xppautX/commit/70dc442)), [1f2a893](https://github.com/MuhammadMoustafa/xppautX/commit/1f2a893), [76c3950](https://github.com/MuhammadMoustafa/xppautX/commit/76c3950), [W76](https://github.com/MuhammadMoustafa/xppautX/issues/124).

## 20. Writes past the end and unterminated strings

Small ones, each a memory error in XPPAUT's own code:

- `add_export_list` allocates `strlen(in)` bytes and `strcpy`s the string into them, one byte short ([extra.c:217-219](../reference/xppaut-8.0/extra.c#L217-L219) ([master 217-219](../reference/xppaut-master/extra.c#L217-L219))), so every `export` line overflows the heap by one NUL.
- `init_ar_ic` clears `ar_ic[i].var[i]` for i up to 49 where `var` has 20 bytes ([integrate.c:236](../reference/xppaut-8.0/integrate.c#L236) ([master 231](../reference/xppaut-master/integrate.c#L231)), [integrate.c:133](../reference/xppaut-8.0/integrate.c#L133) ([master 132](../reference/xppaut-master/integrate.c#L132))): iterations 20-49 write past the end of `var` into the next fields (the intended index is 0).
- `find_ker` copies the digits of `int[mu]{...}` into `num` and calls `atof` without terminating it ([form_ode.c:1112-1120](../reference/xppaut-8.0/form_ode.c#L1112-L1120) ([master 1112-1120](../reference/xppaut-master/form_ode.c#L1112-L1120))), so a stale digit or exponent left in the buffer changes `mu`.
- `get_root` splits an array-plot name like `u10` into `u` and `10`, then writes the digits' terminator at `me[n-i]`, one byte too late ([arrayplot.c:477](../reference/xppaut-8.0/arrayplot.c#L477) ([master 477](../reference/xppaut-master/arrayplot.c#L477))), so `atoi` reads a byte never written.
- `io_numerics` names the integration method with a 13-entry array indexed by `METHOD` ([lunch-new.c:333-335](../reference/xppaut-8.0/lunch-new.c#L333-L335) ([master 333-335](../reference/xppaut-master/lunch-new.c#L333-L335)), [lunch-new.c:344](../reference/xppaut-8.0/lunch-new.c#L344) ([master 344](../reference/xppaut-master/lunch-new.c#L344))); Rosenbrock and Symplectic are methods 13 and 14 (the 15-entry list of [lunch-new.c:105-107](../reference/xppaut-8.0/lunch-new.c#L105-L107) ([master 105-107](../reference/xppaut-master/lunch-new.c#L105-L107)) has them), so File > Write set reads past the array.
- `do_new_parser` finds the `=` of an AUX, VECTOR, SPECIAL, EXPORT or `V` declaration with `find_char` and uses its answer unchecked ([form_ode.c:1570-1573](../reference/xppaut-8.0/form_ode.c#L1570-L1573) ([master 1570-1573](../reference/xppaut-master/form_ode.c#L1570-L1573)), [form_ode.c:1580-1583](../reference/xppaut-8.0/form_ode.c#L1580-L1583) ([master 1580-1583](../reference/xppaut-master/form_ode.c#L1580-L1583)), [form_ode.c:1590-1593](../reference/xppaut-8.0/form_ode.c#L1590-L1593) ([master 1590-1593](../reference/xppaut-master/form_ode.c#L1590-L1593))), -1 when it is absent, as the index to a copy: a malformed line (`AUX foo`) writes out of bounds.

- **Evidence:** `find_ker`: valgrind ([W21](https://github.com/MuhammadMoustafa/xppautX/issues/34)) in lamvolt.ode, cuplamdif.ode and mlvolt.ode, the models with `int[.5]`; the likely cause of mlvolt.ode's one-in-four md5 difference between runs on macOS CI. `get_root`: valgrind through an array plot (servercheck). `io_numerics`: henhei.ode (Rosenbrock) wrote its set file empty and the process died. `init_ar_ic`: gcc's `-Waggressive-loop-optimizations` reports iteration 20 as undefined. `do_new_parser`: gcc's `-Wmaybe-uninitialized` ([8cd24cc](https://github.com/MuhammadMoustafa/xppautX/commit/8cd24cc)).
- **xppautX:** each is replaced by a `std::string`/`std::vector` or a checked index; the method names include all 15 methods.
- **Card:** [a1650c9](https://github.com/MuhammadMoustafa/xppautX/commit/a1650c9), [ce435fd](https://github.com/MuhammadMoustafa/xppautX/commit/ce435fd), [5557f60](https://github.com/MuhammadMoustafa/xppautX/commit/5557f60), [49ff46d](https://github.com/MuhammadMoustafa/xppautX/commit/49ff46d), [8cd24cc](https://github.com/MuhammadMoustafa/xppautX/commit/8cd24cc), [W55](https://github.com/MuhammadMoustafa/xppautX/issues/103) (the export code was removed, [96a4608](https://github.com/MuhammadMoustafa/xppautX/commit/96a4608)).

## 21. `.set`, `.par` and `.ic` files read by guessing

- every number of a `.set` file is read with `fgets(bob,255,fp)` then `atoi`/`atof` ([lunch-new.c:658-695](../reference/xppaut-8.0/lunch-new.c#L658-L695) ([master 658-695](../reference/xppaut-master/lunch-new.c#L658-L695))): a line over 254 characters is cut and its remainder is read as the next line, shifting every later value; a short file, a missing line or text in place of a number gives 0 (or the previous value) with no message;
- File > Read set applies each value as it reads it ([lunch-new.c:178-250](../reference/xppaut-8.0/lunch-new.c#L178-L250) ([master 179-250](../reference/xppaut-master/lunch-new.c#L179-L250))), so a file that goes wrong in the middle leaves the model half changed; the `Incompatible parameters` check comes only after the first two lines;
- a `.par` file is read the same way ([lunch-new.c:384-415](../reference/xppaut-8.0/lunch-new.c#L384-L415) ([master 384-415](../reference/xppaut-master/lunch-new.c#L384-L415))); a `.ic` file is read with `fscanf` into the initial conditions as it goes, and an error returns without closing the file and with the values read so far already stored ([lunch-new.c:432-465](../reference/xppaut-8.0/lunch-new.c#L432-L465) ([master 432-464](../reference/xppaut-master/lunch-new.c#L432-L464))).

- **Evidence:** tests/test_lunch (a bad last value leaves the session as it was), servercheck check_load_all_or_nothing, tests/test_names ("a field longer than its buffer still gets its start and the next field reads from the next line").
- **xppautX:** a file of ours is read whole through one pipeline, every line and value checked, and applied in one step that cannot fail; a bad line stops the load with the file, the line and the value, and nothing is applied. XPPAUT's own `.set` is still imported, read the same strict way.
- **Card:** [2fbf61e](https://github.com/MuhammadMoustafa/xppautX/commit/2fbf61e), [W116](https://github.com/MuhammadMoustafa/xppautX/issues/167) ([a84f018](https://github.com/MuhammadMoustafa/xppautX/commit/a84f018)), [W125](https://github.com/MuhammadMoustafa/xppautX/issues/177) ([a6a4d5f](https://github.com/MuhammadMoustafa/xppautX/commit/a6a4d5f)).

## 22. Adjoint, H function and histogram tables on moved storage

The adjoint, its H function and the histogram keep pointers to the stored
columns beside their own (`my_trans.data[i]=storage[i]`). When a run fills the
store, `stor_full` reallocates every column (`realloc`), which may move them,
and the tables keep pointing at the old ones. Showing the adjoint, H function
or histogram again from its menu then reads freed memory: ASan's
heap-use-after-free, garbage values without it. After a new model is loaded
the file-scope `ADJ_HERE`, `H_HERE` and the tables still point into the
previous model's store.

- **XPPAUT 8.0:** adj2.c [adj2.c:155](../reference/xppaut-8.0/adj2.c#L155) ([master 155](../reference/xppaut-master/adj2.c#L155)) lends the columns once, [adj2.c:203-216](../reference/xppaut-8.0/adj2.c#L203-L216) ([master 203-216](../reference/xppaut-master/adj2.c#L203-L216)) (adj_back and h_back) show them again later; storage.c reallocstor [storage.c:65-72](../reference/xppaut-8.0/storage.c#L65-L72) ([master 65-72](../reference/xppaut-master/storage.c#L65-L72)) moves them, called from integrate.c stor_full [integrate.c:2730](../reference/xppaut-8.0/integrate.c#L2730) ([master 2701](../reference/xppaut-master/integrate.c#L2701)).
- **Evidence:** autocheck's `memory` section fails before the change (ASan: heap-use-after-free in browser_rows) and passes after.
- **xppautX:** the sets lend the store's columns afresh each time they are shown; the adjoint's sets belong to the session.
- **Card:** [W117](https://github.com/MuhammadMoustafa/xppautX/issues/168) ([5ae5282](https://github.com/MuhammadMoustafa/xppautX/commit/5ae5282)).

## 23. `fftcon` reads one value past its weight table

A network `fftcon` builds the convolution kernel from a weight table that
the load check requires to have `n` points (periodic) or `2n` (zero padded),
and then reads `w[i+n2]` for `i = 0..n2` with `n2 = n/2`, which reaches `w[n]`,
one past a table of exactly `n` points; the zero-padded form reads up to
`w[2N]`. A model that follows the documented table length reads one double
past the table into its kernel (and `w[n2-1]` is left unused).

- **XPPAUT 8.0:** simplenet.c load check [simplenet.c:635-642](../reference/xppaut-8.0/simplenet.c#L635-L642) ([master 635-642](../reference/xppaut-master/simplenet.c#L635-L642)), update_fft [simplenet.c:1430-1461](../reference/xppaut-8.0/simplenet.c#L1430-L1461) ([master 1430-1461](../reference/xppaut-master/simplenet.c#L1430-L1461)).
- **Evidence:** found while writing tests/test_fftcon.cpp ([W31a](https://github.com/MuhammadMoustafa/xppautX/issues/58)), which needed a table of n+1 points to avoid the over-read; no example uses fftcon, so no sanitizer saw it.
- **xppautX:** the kernel is built from `w[0..n-1]`, each once, equal to the matching `conv` network (tests/test_fftcon.cpp); the manual states the table length.
- **Card:** [W38](https://github.com/MuhammadMoustafa/xppautX/issues/81) ([f0c0292](https://github.com/MuhammadMoustafa/xppautX/commit/f0c0292)).

## 24. An export with no library copies uninitialised memory

`export` lines copy the model's inputs through a user's shared library and
copy its outputs back. With no `dll_lib=` (or a library that did not load),
`my_fun` returns without writing, and `do_in_out` copies the output buffer,
allocated with `malloc` and never written, into the model's variables.

- **XPPAUT 8.0:** extra.c do_in_out [extra.c:192-210](../reference/xppaut-8.0/extra.c#L192-L210) ([master 192-210](../reference/xppaut-master/extra.c#L192-L210)) (the copy is unconditional), my_fun [extra.c:128-131](../reference/xppaut-8.0/extra.c#L128-L131) ([master 128-131](../reference/xppaut-master/extra.c#L128-L131)) (returns 0 when `dlf.loaded==-1`), add_export_list [extra.c:215-230](../reference/xppaut-8.0/extra.c#L215-L230) ([master 215-230](../reference/xppaut-master/extra.c#L215-L230)) (`malloc` of the buffers).
- **Evidence:** getfrefm.ode and testdll.ode gave a different output.dat from one CI run to the next on Windows (the heap's contents), zeros on Linux's fresh pages.
- **xppautX:** the buffers were zeroed and copied back only when the function ran; the whole compiled-function feature was later removed ([W55](https://github.com/MuhammadMoustafa/xppautX/issues/103)).
- **Card:** [d5eebae](https://github.com/MuhammadMoustafa/xppautX/commit/d5eebae), [W55](https://github.com/MuhammadMoustafa/xppautX/issues/103) ([96a4608](https://github.com/MuhammadMoustafa/xppautX/commit/96a4608)).

## 25. An output stride of 0 divides by zero

`nout` (NJMP, every how many steps a point is stored) is checked only by
the Numerics dialog, whose `check_pos` turns 0 or less into 1. A .set file
(`io_int`) and an `@ nout=`/`@ njmp=` line (`atoi`) set it unchecked, and
`integrate` divides the step count by it: 0 is SIGFPE on the next Go.
`DeltaT` 0 reaches the line before, a floating division by zero, unchecked
the same way.

- **XPPAUT 8.0:** integrate [integrate.c:1912-1914](../reference/xppaut-8.0/integrate.c#L1912-L1914) ([master 1884-1886](../reference/xppaut-master/integrate.c#L1884-L1886)) (the division), the .set read [lunch-new.c:342](../reference/xppaut-8.0/lunch-new.c#L342) ([master 342](../reference/xppaut-master/lunch-new.c#L342)), the @ lines [load_eqn.c:1483-1497](../reference/xppaut-8.0/load_eqn.c#L1483-L1497) ([master 1482-1496](../reference/xppaut-master/load_eqn.c#L1482-L1496)), the dialog's check [numerics.c:118-122](../reference/xppaut-8.0/numerics.c#L118-L122), [numerics.c:230-231](../reference/xppaut-8.0/numerics.c#L230-L231) ([master 227-228](../reference/xppaut-master/numerics.c#L227-L228)).
- **Evidence:** the code review of 2026-10-01 (docs/code-review-2026-10-01.md): a saved session whose model.set had `0 nout` loaded, and Initialconds/Go exited on SIGFPE.
- **xppautX:** every source of a numerics value checks it by one rule of model_options' table (`rule_problem`): the Numerics dialog, the `@` lines (which until [W145](https://github.com/MuhammadMoustafa/xppautX/issues/197) checked only that the value was a number) and the .set reader ([W145](https://github.com/MuhammadMoustafa/xppautX/issues/197)).
- **Card:** [W145](https://github.com/MuhammadMoustafa/xppautX/issues/197) ([#197](https://github.com/MuhammadMoustafa/xppautX/issues/197)).

## 26. Set files ignore the model's names

A `.set` with the same equation/auxiliary and parameter counts is accepted
for any open model. The names after values are ignored: values are applied
in position order, even when they belong to different variables or parameters.

- **XPPAUT 8.0:** the count-only check in [lunch-new.c:179-217](../reference/xppaut-8.0/lunch-new.c#L179-L217) ([master](../reference/xppaut-master/lunch-new.c#L179-L217)); names are discarded by [io_int/io_double, lunch-new.c:658-685](../reference/xppaut-8.0/lunch-new.c#L658-L685) ([master](../reference/xppaut-master/lunch-new.c#L658-L685)), including [ICs and parameters, lunch-new.c:561-581](../reference/xppaut-8.0/lunch-new.c#L561-L581) ([master](../reference/xppaut-master/lunch-new.c#L561-L581)).
- **Evidence:** save a set for `x'=-a*x+b`, `y'=x-y`, `par a=1,b=2`; replace its parameter line `2  b` with `9  another_models_parameter`, leaving counts unchanged. `read_lunch` accepts it and sets `b=9`. The same follows for renamed ICs and torus variables.
- **xppautX:** every named numeric line is checked against its expected model name or setting label before any values apply. The first mismatch reports its file, line and source. A valid import is saved by the session writer as `<base>.snapx` beside the `.set`, then becomes the session open; failed saves keep valid imported values applied. W129 ([#181](https://github.com/MuhammadMoustafa/xppautX/issues/181)) asks permission for this save too; No keeps the imported values applied and the session identity unchanged.
- **Card:** [W153](https://github.com/MuhammadMoustafa/xppautX/issues/205) ([#205](https://github.com/MuhammadMoustafa/xppautX/issues/205)).

## 27. Rotation and boundary-value movies silently drop frames when full

XPPAUT ignores `film_clip()`'s failure in rotation and boundary-value
movies. A movie with more than 250 frames therefore silently loses its
remaining frames. Integration ranges instead show an error.

- **XPPAUT 8.0:** [graf_par.c:618](../reference/xppaut-8.0/graf_par.c#L618) ([master](../reference/xppaut-master/graf_par.c#L618)), [pp_shoot.c:264](../reference/xppaut-8.0/pp_shoot.c#L264) ([master](../reference/xppaut-master/pp_shoot.c#L264)); integration reports the failure at [integrate.c:742](../reference/xppaut-8.0/integrate.c#L742) ([master](../reference/xppaut-master/integrate.c#L736)).
- **Evidence:** the unchecked return values in those two movie loops; capture beyond the capacity cannot add a frame.
- **xppautX:** film capture returns an error value, retained and shown once by the command, including rotation and BVP movies.
- **Card:** [W133](https://github.com/MuhammadMoustafa/xppautX/issues/185) ([#185](https://github.com/MuhammadMoustafa/xppautX/issues/185)).

## 28. A `#` comment after a declaration makes names of its words

XPPAUT treats `#` as a comment only when it starts the line. On a
declaration line, the words after a `#` are read as more declarations:
`p gr=0.01  # Changed on Oct 6th 2009` declares the parameters `#`,
`Changed`, `on`, `Oct`, `6th` and `2009` (each with the value 0), and
`-qpars` lists them. A second line with a trailing comment declares `#`
again. This was reported to XPPAUT as its own issue:
[Ermentrout/xppaut#11](https://github.com/Ermentrout/xppaut/issues/11)
(open), with a model from ModelDB as the example.

- **XPPAUT 8.0:** a statement is a comment only when its first character
  is `#` ([form_ode.c:2274-2277](../reference/xppaut-8.0/form_ode.c#L2274-L2277)
  ([master 2274-2277](../reference/xppaut-master/form_ode.c#L2274-L2277)),
  and `is_comment` [form_ode.c:2822](../reference/xppaut-8.0/form_ode.c#L2822)
  ([master 2822](../reference/xppaut-master/form_ode.c#L2822)) skips only
  leading blanks before it); the `p` line's loop
  [form_ode.c:698-712](../reference/xppaut-8.0/form_ode.c#L698-L712)
  ([master 698-712](../reference/xppaut-master/form_ode.c#L698-L712)) makes
  a parameter of every token `get_next2`
  ([form_ode.c:3107](../reference/xppaut-8.0/form_ode.c#L3107),
  [master 3107](../reference/xppaut-master/form_ode.c#L3107)) returns, `#`
  and the comment's words included.
- **Evidence:** the model `p gr=0.01<TAB># Changed on Oct 6th 2009` then
  `p a=2, b=3 # another note`: XPPAUT declares the comment's words as
  parameters; xppautX's .ode reader did the same and then refused the
  second line (`q11.ode:2: # is a name already, or one parameter too
  many`), 2026-10-01.
- **xppautX:** `#` starts a comment wherever it is (maintainer, 2026-10-01),
  in .odex already and in the .ode reader with [W160](https://github.com/MuhammadMoustafa/xppautX/issues/212); the VS Code extension
  marks a trailing `#` in a .ode as an error naming XPPAUT's issue
  (MuhammadMoustafa/XPP-ODE-Extension#1, 2026-10-01).
- **Card:** [W160](https://github.com/MuhammadMoustafa/xppautX/issues/212) ([#212](https://github.com/MuhammadMoustafa/xppautX/issues/212)).

## 29. The numbers depend on the CPU and on the compiler's FMA

XPPAUT's results are not a function of the model: the same model gives
different numbers on different machines, which a chaotic model or an
adaptive step size turns into a different plot or file. Two causes, both in
how XPPAUT's code meets the machine, neither a bug in a line of it.

1. Its expression evaluator and its solvers call the C library's `sin`,
   `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `sinh`, `cosh`, `tanh`,
   `exp`, `log`, `log10`, `pow`, `erf`, `erfc`, `lgamma` directly, and
   glibc runs FMA or plain SSE2 variants of them, chosen by the CPU at run
   time; none is correctly rounded, and the variants disagree in about one
   call in 1500 (2 million random inputs each: `pow` 1 in 1600, `exp`
   1 in 1400, `sin` 1 in 1400, `log` 1 in 15000, `tan` 1 in 80000).
2. Its arithmetic is compiled with the compiler's default of contracting
   `a*b+c` into one fused multiply-add wherever the target has one (gcc's
   `-ffp-contract=fast`, which XPPAUT's makefile does not turn off): a
   build for a CPU with FMA (x86-64-v3 or `-march=native`, which a
   distribution or a user may choose; arm64 always has one) rounds
   differently from a baseline x86-64 build, in plain `+ - * /` models too.

- **XPPAUT 8.0:** the parser's function tables, [parserslow2.c:1950-1975](../reference/xppaut-8.0/parserslow2.c#L1950-L1975) (`fun1`, `sin` ... `lgamma`; [master](../reference/xppaut-master/parserslow2.c#L1858-L1883)) and [parserslow2.c:1598-1599](../reference/xppaut-8.0/parserslow2.c#L1598-L1599) (`atan2`, `pow`; [master](../reference/xppaut-master/parserslow2.c#L1582-L1583)), and the step-size controls, [stiff.c:233-281](../reference/xppaut-8.0/stiff.c#L233-L281) ([master](../reference/xppaut-master/stiff.c#L233-L281)) and [dormpri.c:574](../reference/xppaut-8.0/dormpri.c#L574) ([master](../reference/xppaut-master/dormpri.c#L574)), all call the C library's `pow`, `sin`, ... through their own names; the makefile's flags, [Makefile:44](../reference/xppaut-8.0/Makefile#L44), say `-O2` and nothing about contraction.
- **Evidence:** xppautX before W159 has XPPAUT's numerical code and the same calls. On one machine (Ubuntu 26.04, gcc 15.2, glibc 2.43, a Zen 4 CPU), the 184 example models' `output.dat` md5s against the committed baseline: with glibc masked to its SSE2 variants (`GLIBC_TUNABLES=glibc.cpu.hwcaps=-FMA`) 11 differ (`fp`, `sine-circle`, `atcoaster`, `fr`, `geisel`, `hhred`, `itoy`, `nf3`, `r3b`, `toy_ok`, `waterwheel`); the same sources built with `-mfma -mavx2` (gcc's default contraction) 29 differ, including `ross-orbit` and `rossler-pecora`, whose equations are only `+ - *`; masking AVX-512 changes none. GitHub's ubuntu-26.04 runner (an AMD EPYC 7763) produced exactly those 29 and two differing PostScript goldens, with the same compiler and C library as the machine where they matched.
- **xppautX:** the transcendental functions are CORE-MATH's correctly rounded ones (`xpp::math`, third_party/core-math), which return the exact value rounded to nearest and so the same bits on every CPU and C library, and the build passes `-ffp-contract=off`, so no target contracts. The example baselines and the goldens were rewritten once, on purpose (W159's commit names the models). `tools/mathcheck.sh` fails a direct call of the C library's. W163 closes the Bessel gap: `besselj`/`bessely` use the vendored musl implementation over `xpp::math`, with independent reference-value tests.
- **Card:** [W159](https://github.com/MuhammadMoustafa/xppautX/issues/211) ([#211](https://github.com/MuhammadMoustafa/xppautX/issues/211)); Bessel follow-up [W163](https://github.com/MuhammadMoustafa/xppautX/issues/215) ([#215](https://github.com/MuhammadMoustafa/xppautX/issues/215)).

## 30. AUTO orbit loading trusts file dimensions and storage capacity

The solution header supplies the number of rows and variables. XPPAUT
uses them to fill a fixed `u[NAUTO]` and the existing data table without
checking the variable bound or growing the table for those rows.

- **XPPAUT 8.0:** header counts in [auto_nox.c:2908-2930](../reference/xppaut-8.0/auto_nox.c#L2908-L2930) ([master 2879-2901](../reference/xppaut-master/auto_nox.c#L2879-L2901)); unchecked writes in [auto_nox.c:2678-2724](../reference/xppaut-8.0/auto_nox.c#L2678-L2724) ([master 2649-2695](../reference/xppaut-master/auto_nox.c#L2649-L2695)).
- **Evidence:** code review of those loops; a three-row, two-variable periodic orbit with only two allocated data rows writes a third row beyond storage. The session AUTO unit check exercises exactly those counts and verifies all three times and both variables after loading.
- **xppautX:** one shared AUTO header reader bounds dimensions, parameter counts, collocation and mesh allocation; the session checks complete restart payloads before restoring anything. Orbit loading grows through the data store's owner before writing rows.
- **Card:** [W155](https://github.com/MuhammadMoustafa/xppautX/issues/207) ([#207](https://github.com/MuhammadMoustafa/xppautX/issues/207)).

## 31. Model options bypass method suitability and ignore unknown methods

XPPAUT 8.0's `@ meth` reader compares only the first character against its
keys ([load_eqn.c:1116](../reference/xppaut-8.0/load_eqn.c#L1116),
[load_eqn.c:1507-1517](../reference/xppaut-8.0/load_eqn.c#L1507-L1517);
[master:1506-1516](../reference/xppaut-master/load_eqn.c#L1506-L1516)).
An unknown key leaves METHOD unchanged and still marks the option as set.
`@ meth=symplectic` selects Stiff (the `s` key), while `@ meth=y` selects
Symplectic without checking dimension. The menu alone rejects odd dimensions
and switches to Adams ([numerics.c:280-284](../reference/xppaut-8.0/numerics.c#L280-L284);
[master:277-281](../reference/xppaut-master/numerics.c#L277-L281));
`do_meth` starts Symplectic unconditionally
([numerics.c:674-685](../reference/xppaut-8.0/numerics.c#L674-L685)), whose step
reads and writes `y[j+1]` for every even `j < n`, including beyond the final
odd coordinate ([odesol2.c:193-206](../reference/xppaut-8.0/odesol2.c#L193-L206);
[master:193-206](../reference/xppaut-master/odesol2.c#L193-L206)).
Thus an odd model with `@ meth=y` reaches the paired solver.

**xppautX:** one picker reads full names and legacy keys and validates suitability
for every input. Unknown or unsuitable options fail the whole load at the
option line; menu refusal keeps the old method. W132,
[#184](https://github.com/MuhammadMoustafa/xppautX/issues/184).

## Known and kept

Not fixed in `.ode` and so not listed above: the expression grammar's
surprises (`^` groups left, comparisons bind tighter than arithmetic,
`@ total = 0.03` with spaces is silently ignored, `@` values are read with
`atof`), inventoried in docs/odex-quirks.md. They stay in `.ode` because
files depend on them; `.odex` (docs/odex.md) is the format without them.


## 32. Kinescope Cancel still saves frames

- **XPPAUT 8.0:** [kinescope.c:166-178](../reference/xppaut-8.0/kinescope.c#L166) initializes `base` to `frame`, ignores `new_string`'s return, and calls `save_movie` whenever the retained string is nonempty. Capture a frame, choose Save, then Cancel: `frame_0.gif` is still written. The same code is in [master kinescope.c:166-178](../reference/xppaut-master/kinescope.c#L166).
- **xppautX:** Save uses a file ask and checks its result before requesting pixels or opening a writer. A canceled name produces no output, verified by servercheck.
- **Filename limit:** the related array filename form uses 25-byte values ([arrayplot.c:363-381](../reference/xppaut-8.0/arrayplot.c#L363), [pop_list.h:19](../reference/xppaut-8.0/pop_list.h#L19)); `do_string_box` copies whole strings into those slots ([pop_list.c:253-263](../reference/xppaut-8.0/pop_list.c#L253)), risking overflow rather than reliably cutting input. Our `{:.24}` initialization was a refactoring artifact that retained 24 characters, not an upstream formatting expression. W130 removes filename form fields and that cut; the general upstream limit is finding [3](#3-names).
- **Card:** [W130](https://github.com/MuhammadMoustafa/xppautX/issues/182).

## 33. CVode gets TOLER as its absolute and ATOLER as its relative tolerance

- **XPPAUT 8.0:** the manual says TOLER "is the relative tolerance for CVODE" and ATOLER its absolute one. The integrator calls `cvode(..., &TOLER, &ATOLER)` ([integrate.c:1811](../reference/xppaut-8.0/integrate.c#L1811), [integrate.c:1991](../reference/xppaut-8.0/integrate.c#L1991); [master:1783](../reference/xppaut-master/integrate.c#L1783)), which names its parameters `atol, rtol` ([cv2.c:95](../reference/xppaut-8.0/cv2.c#L95); master: the same line), so TOLER arrives as `atol`; `start_cv` then hands CVODE `rtol, atol` in that order ([cv2.c:36](../reference/xppaut-8.0/cv2.c#L36); master: the same line) and CVODE's relative tolerance is ATOLER, its absolute one TOLER. The Dormand-Prince integrators called on the next line get `&TOLER,&ATOLER` in the order they expect, so only CVode is reversed.
- **Evidence** (tools/models/sundials/osc1e6.odex, an oscillator of amplitude 1e6, `@ meth=cvode`, 100 time units): `@ tol=1e-3, atol=1e-10` takes 3769 steps and ends within 1e-5 (scaled) of the exact solution, a relative tolerance of 1e-10; `@ tol=1e-10, atol=1e-3` takes 537 steps and ends off by 21, a relative tolerance of 1e-3 (docs/sundials-eval.md). With both set equal, as most models do, nothing shows.
- **xppautX:** unchanged on purpose: every CVode result would change (the examples' md5s of atcoaster, fieldnoy, itoy, toy_ok and waterwheel, which set the two apart). The mapping is kept as XPPAUT has it and named in core/cv2.cpp's comment; the manual's sentence (docs/manual/02-ode-files.md, 16-quick-reference.md) is XPPAUT's and still says the opposite. Whether to swap is the maintainer's decision.
- **Card:** [W34](https://github.com/MuhammadMoustafa/xppautX/issues/72).
