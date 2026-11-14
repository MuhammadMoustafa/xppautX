# W729 (#807): include-what-you-use over core, stage 1

Stage 1 of the card: the tool, the mapping file, and what IWYU's advice is
worth. No source file was changed; stage 2 applies the advice after W724
(Session's members out of session.h) has merged, since it moves about a hundred
files' includes and the advice below is of the tree before it. Measured on
master 194f9402, Ubuntu, 16 threads, IWYU 0.23 on clang 19, gcc 15's libstdc++.

## What was built

    tools/iwyucheck.sh [--jobs N] [--only STEM,...] [--db-only]

- **Compile database**: `tools/iwyucheck.py` runs `make -Bn objects` and one
  object target per tests/test_*.cpp, takes each `g++ ... -c core/x.cpp` line
  the Makefile prints (without `-MMD -MP -MF`, `-o`) and writes
  `build/iwyu/compile_commands.json`: 281 commands (201 core, 80 tests). The
  Makefile stays the one source of flags; nothing is copied. (`bear` is not
  installed, and a dry run needs no build.) `-w` is added to IWYU's own
  arguments, since the Makefile's gcc-only `-W` flags are not its business.
- **The run**: `iwyu_tool -p build/iwyu -j1 <source> -- -Xiwyu --mapping_file=tools/iwyu.imp
  -Xiwyu --check_also=core/*.h -w`, one `iwyu_tool` per source, four at a time
  (`JOBS`, tools/jobs.sh), sorted by file so the output is the same each time
  (two full runs compared: identical summaries). `--check_also` makes IWYU
  advise every core header a source includes, not only the one paired with
  it (without it 76 headers were advised, with it 103 of 177; the others
  already have correct includes); a header seen from several sources keeps
  the advice of the first, in file order.
- **Output**: `build/iwyu/<dir>__<name>.txt` (IWYU's text for each source and
  the headers it checked, as printed, minus a libstdc++ mapping warning that
  names no file of ours) and `summary.txt`. `build/` is git-ignored: nothing of
  it is committed.
- **Time**: 2m55s to 5m47s for the whole tree at -j4 (13-22 CPU minutes; the
  spread is the laptop-class machine's, the work is the same). `--only` takes
  a few seconds a file.

## The mapping, tools/iwyu.imp

IWYU 0.23 knows libstdc++ to gcc 13. Its raw advice over our tree had no
`<bits/...>` suggestion (its built-in map holds), but:

| raw advice | cause | in the mapping |
|---|---|---|
| `#include <format>` "for vector" (140 times) | gcc 15's `<format>` is where IWYU finds a `vector` declaration | `std::vector` to `<vector>` |
| `#include <ranges>` "for pair" (56), "for find" (38) | likewise | `std::pair` to `<utility>`, `std::ranges::find`, `find_if`, `__find_fn` to `<algorithm>` |
| `#include <stddef.h>` "for size_t" (16), "for NULL" | the C header is the first declaration | `size_t`, `ptrdiff_t`, `NULL` to `<cstddef>` (AGENTS.md: the API is C++) |
| `<initializer_list>` (75), `<functional>` for `identity` (18), `<compare>` for `operator<` (9), `_Deque_iterator` and kin | implicit uses: a braced list, a defaulted ranges projection, a generated comparison, libstdc++'s own internals. No mapping can drop an include, so `tools/iwyucheck.py`'s IMPLICIT set leaves them out of every count (the advice files keep them) | none possible |
| `#include ".../third_party/core-math/core_math.h"` (tests/test_math.cpp, absolute path) | a private engine of xpp_math.h | `core_math.h` and anything of third_party/core-math to `"xpp_math.h"` |
| a file outside xpp_files told to include `xpp_files_internal.h` | an internal header of xpp_files*.cpp | to `"xpp_files.h"` (no file does today: the guard for stage 2) |

The mapping removed 258 of 2122 suggested additions of includes before any
human read them. Our owners (xpp_io, xpp_files, xpp_error, xpp_log, xpp_mem,
xpp_math) each define their own symbols; none re-exports another's (xpp_io.h
includes xpp_error.h and xpp_files.h for its own declarations, and a user of
`Result` and `format` is rightly told to include both), so IWYU already names
the owner for every symbol: the two private pieces above are the only ones.
No `// IWYU pragma` was added to any header (a source change; stage 2 would add
`keep` where a header is needed for a reason IWYU cannot see, none found yet).

## The advice, per module

Counts are of files IWYU printed a block for (a header with correct includes
prints none), over core/ and tests/, implicit additions left out. "Remove" is
an include IWYU would delete, "add inc" an include it would add, "add fwd" a
forward declaration it would add (what a removed include is replaced by when
only a pointer or reference is used), "rm in .h" removals that are in headers.

| module | files | remove | add inc | add fwd | rm in .h |
|---|---:|---:|---:|---:|---:|
| integrators | 32 | 88 | 117 | 6 | 10 |
| plot | 24 | 82 | 93 | 4 | 11 |
| autoX | 41 | 80 | 237 | 11 | 20 |
| protocol front end | 22 | 76 | 186 | 11 | 10 |
| tests | 80 | 73 | 443 | 3 | 0 |
| model and loading | 30 | 66 | 128 | 6 | 11 |
| analysis | 15 | 40 | 66 | 2 | 8 |
| foundation | 17 | 39 | 41 | 2 | 6 |
| commands | 33 | 38 | 296 | 6 | 2 |
| expressions | 20 | 32 | 93 | 4 | 6 |
| application | 12 | 31 | 38 | 4 | 5 |
| session files | 10 | 25 | 66 | 8 | 12 |
| export | 12 | 18 | 45 | 5 | 5 |
| ui seam | 6 | 16 | 11 | 0 | 5 |
| platform | 7 | 13 | 31 | 2 | 0 |
| animation | 4 | 8 | 18 | 1 | 3 |
| session | 6 | 6 | 15 | 0 | 2 |
| **total** | 371 | **731** | **1924** | **75** | |

Of the 731 removals, 473 are our headers, 109 C headers (`<stdio.h>`, `<math.h>`,
`<stdlib.h>`, `<string.h>`), 149 C++ library headers. By kind of file:
188 core .cpp (542 removals, 1351 additions, 9 forward declarations), 103 core
headers (116 removals, 130 additions, 63 forward declarations), 80 tests (73
removals, 443 additions: tests lean on transitive includes). Forward
declarations are a header's tool: 63 of the 75 are in headers.

The 15 core files with most removals (remove / add include / add forward declaration):
xpp_util.cpp 19/4/0, load_eqn.cpp 15/4/0, integrate.cpp 14/11/0,
ui_json.cpp 14/26/1, xppautx_main.cpp 14/8/0, autox_nox.cpp 13/25/0,
gear.cpp 12/1/0, arrayplot.cpp 11/4/0, many_pops.cpp 11/14/0,
graf_par.cpp 10/5/0, json_state.cpp 10/18/0, lunch-new.cpp 10/11/0,
autox_run.cpp 9/6/0, form_ode.cpp 9/9/0, numerics.cpp 9/4/0.
The most removed of our headers: form_ode.h (28 files), xpp_util.h (24),
xpp_log.h (18), xpp_globals.h (14), xpp_ui.h (13), command_registry.h (12),
menus.h (12), colormap.h and graf_par.h (11 each).

## Reach: how much the widest headers would drop

By tools/depgraph.py's own parser and closure (imported, not copied): the
edge of each removed quoted include is dropped by its line, each added quoted
include is added, and the number of core/*.cpp (of 201) that reach each header
is counted again. "After removals" is the cut alone; "after both" the net one,
what applying the whole advice does.

| header | now | after removals | after removals and additions |
|---|---:|---:|---:|
| xpp_error.h | 189 | 188 | 189 |
| xpp_files.h | 188 | 185 | 187 |
| xpp_io.h | 188 | 185 | 187 |
| xpp_mem.h | 172 | 168 | 169 |
| colormap.h | 164 | 154 | 161 |
| xpp_log.h | 158 | 142 | 144 |
| line_style.h | 155 | 133 | 142 |
| text_style.h | 153 | 124 | 128 |
| odex_block.h | 150 | 142 | 144 |
| odex.h | 149 | 141 | 143 |
| model_features.h | 147 | 140 | 141 |
| xpp_math.h | 145 | 141 | 143 |
| struct.h | 136 | 129 | 135 |
| expr_program.h | 135 | 129 | 129 |
| session.h | 132 | 124 | 127 |
| model.h | 100 | 90 | 91 |
| sum over all 177 headers | 12409 | 11401 (-8.1%) | 11759 (-5.2%) |

The widest headers are widest because every file needs them (xpp_error.h,
xpp_io.h): IWYU cannot cut those and does not claim to. The cuts are in the
second rank: text_style.h -16%, line_style.h -8%, xpp_log.h -9%, colormap.h -2%
net. They come from a few headers' removals (odex_block.h includes line_style.h
and text_style.h it does not need, say), so stage 2 gets most of the cut from
headers. session.h is the number that W724 will change; the table is to be
rerun after it.

## Is the advice right? Wrong advice found

- **It compiles.** The advice of 12 sources (the 10 with most removals, plus
  autox_nox.cpp and ui_json.cpp) was applied to a scratch copy of core/ with
  `fix_include --noreorder --nocomments --nosafe_headers` (headers it advises
  too: 80 files changed in the largest) and each source compiled with
  `g++ -std=c++23 -fsyntax-only`: 12 of 12 compile. Scratch deleted.
- **Only the platform the run is on is seen.** IWYU sees Linux, no
  `XPP_WINDOW_PLUGIN` variants beyond the Makefile's, and does not see the
  files that do not build there (xpp_win32.cpp, the macOS code). Three
  removals sit inside a conditional: `<sys/wait.h>` (xpp_http.cpp:71, the
  POSIX half), `<unistd.h>` (xpp_window.cpp:72) and `<type_traits>`
  (xpp_window.cpp:53). Right for Linux; a stage 2 must check each against
  macOS and Windows, or keep them (`// IWYU pragma: keep`, a source change
  stage 2 would make where a line is needed on another system). Nothing was
  suggested for a Windows-only include, since those files are not analysed.
- **Order-sensitive includes.** xpp_log.h includes `<cstdio>` first "so
  MinGW's libstdc++ picks its C99 printf": IWYU keeps it (FILE is used), but
  `fix_include`'s default reorders includes. Stage 2 applies with
  `--noreorder` and reads every `<cstdio>`/`<stdio.h>` diff.
- **A macro in a stringified argument** (W723's XPP_FORMAT_VERSION in
  `JSON_UI_STR(XPP_FORMAT_VERSION)`, xpp_about.cpp and ui_json.cpp): IWYU
  0.23 keeps `format_version.h` there ("for XPP_FORMAT_VERSION"), it sees the
  expansion. A check of every removed header that defines a macro the file
  names (by text) found no other case. No pragma needed today.
- **Implicit uses** (initializer_list, identity, generated comparison,
  libstdc++ internals): noise, left out of the counts as above.
- **Ordering of the same name in two systems**: `<stdio.h>` vs `<cstdio>`.
  IWYU keeps whichever spelling is there and adds a C++ name only for a symbol
  mapped above; 109 removals are C headers it finds unused. Whether to prefer the
  C++ names everywhere is a style card of its own, not IWYU's.
- **A vendored file does not parse**: xpp_webview.cpp (third_party/webview's
  `unique_ptr` of an incomplete type) gives an IWYU error and no advice; it is
  in the compile database but its advice is empty. Everything else ran.
- **tests/ add many includes** (443, 5.5 a file): they lean on what the core
  headers include. Applying the tests' advice would pin them to the headers
  they use; useful, but later and apart.

## Recommendation

**Not in sourcecheck.** It is 3 to 6 minutes and 13 to 22 CPU minutes against
sourcecheck's seconds, and it holds 731 removals now: a gate that fails at zero
fails today. After stage 2 has brought the advice near zero:

1. A CI job of its own (linux, one run, `tools/iwyucheck.sh`, about 6 minutes at
   four cores, ccache does not help IWYU), failing when a file's removals plus
   additions exceed a committed baseline (`tests/iwyu.baseline`, the way
   unsafecheck's works: a count that dropped is fine), so new code cannot add
   an unused include or lean on a transitive one. Kept out of verify.sh.
2. Noise is low after the mapping: a dozen mapping entries and one IMPLICIT set
   cover everything seen; wrong advice found is the platform kind (3 cases) and
   the ordering case, none a macro.

**Stage 2 should apply first**, after W724 and a rerun of this tool:
(a) the headers' removals (116, 103 headers, with their forward declarations):
they are what cuts reach (text_style.h, line_style.h, xpp_log.h, odex.h), and
a header's include is paid by every file under it; (b) the core .cpp removals of
our own headers (form_ode.h, xpp_util.h, xpp_log.h, xpp_globals.h, xpp_ui.h,
command_registry.h, menus.h) with the additions the same advice asks for, a
module at a time, starting with integrators, plot and autoX (the most
removals); (c) the C and C++ library removals last (small, no reach, and the
platform risk above); (d) the tests apart. Each module: apply with
`fix_include --noreorder`, build, the unit tests, then regenerate docs/deps.html
(`uv run --locked python tools/depgraph.py`).

Commands: `tools/iwyucheck.sh` (the run and `build/iwyu/summary.txt`),
`tools/iwyucheck.sh --db-only`, `tools/iwyucheck.sh --only xpp_util,gear`.

## Stage 2: the advice applied (after W724, master af8b5e3d)

Rerun on the new tree: 745 removals, 1815 additions, 78 forward declarations.
Applied with a small script of our own, not `fix_include`: fix_include takes a
comment above an `#include` as attached to it and deleted 202 lines of
documentation (the first attempt, sent back by review). The script deletes
only the lines IWYU names, by line number, and only when the line is an
`#include` or exactly one forward declaration; adds an include after the last
include of its kind; and puts a new forward declaration in the file's one-line
`namespace xpp { ... }` form, merged into an existing such line. Lines deleted
that are not includes: 3 forward declarations (command_def.h, diagram.h,
xpp_util.h), no comment. 364 files changed, core/ and tests/.
Left out on purpose: removals of C headers (`<stdio.h>`, `<math.h>`, ...: 116,
used on a platform IWYU does not see), removals inside a conditional, the
implicit additions, and a removal of a forward declaration that shares its line
with another. After it: build 0 warnings (-Werror), unit tests, examples_check
(215 of 215), servercheck (3163), autoxcompare (0 failures), sourcecheck, all
green; docs/deps.html regenerated.

IWYU was wrong in seven places, each kept with an `IWYU pragma: keep` and a
reason: a generic lambda that needs a type complete (core/grobs.h, struct.h;
core/model_options.cpp, delay_handle.h), a `std::vector` of types destroyed
where it is declared (core/odex_parse.cpp, load_eqn.h; core/session.h,
model_parts.h: the advice that repeated 38 times, because after the apply
Session holds `std::vector<BoundaryCondition>` and IWYU wants a forward
declaration of it, which cannot destroy the vector), enums an opaque
declaration cannot stand in for (core/autox_c.h, xpp_math_autox.h), and the
internal header of xpp_files*.cpp, which the mapping sends to xpp_files.h
(core/xpp_files.cpp, core/xpp_files_dir.cpp, tests/test_files.cpp). IWYU does
not analyse tests/ headers, so tests/load_args.h got comline.h and xpp_error.h
by hand.

Reach (core/*.cpp of 201), after W724 and before the apply, then after: sum
over all 177 headers 7180 to 6205; session.h 132 to 127; xpp_log.h 158 to 144;
colormap.h 164 to 161. docs/deps.html has the rest.
