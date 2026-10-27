# W477 Audit: cleaning, refactoring and duplication before release

Card: [#540](https://github.com/MuhammadMoustafa/xppautX/issues/540). Study date:
2026-10-26. Tree inspected: `c53494aa` (`task/W477-cleaning-audit`). This is a
study only. No source, test, baseline, allowlist or roadmap is changed.
Line numbers below refer to this tree, not to the eventual follow-up branches.

The highest release value is in completing the common input operations:
bounded compressed/data reads, checked table numbers, one startup argument
reader, and transactional animation replacement. Mechanical refactoring of
numerical kernels ranks below these. Passing source checks does not establish
that these operations have one owner or that a reader validates its input.

## Method and coverage

Inventoried all of `core/` (310 files), `tests/` (286) and `tools/` (205),
including fixtures and nested tool directories. Searched these trees for
numerical solves/eigenvalues/norms, number/name parsing, file reading,
option dispatch, error construction, mutable/static state, fixed sizes,
process launch, sleeps/deadlines, and historical migration comments.
Read the corresponding owners and callers; used function/block and file-size
inventories to select long implementations for closer inspection. This is a
whole-tree search with targeted source review, not a claim of a proof for every
line or every possible input. `third_party/` and `reference/` were excluded;
`web2/src` was read only for duplication with the core.

Searched the existing owners before proposing abstractions: `xpp_io`,
`xpp_files*`, `xpp_error`, `xpp_ui`, `xpp_util`, `xpp_math*`, `model_options`,
`auto_settings`, `model_files`, `session`, and `cstrip`. The proposals reuse
those owners. No implementation was added. Current code and
`docs/roadmap.md:1123–1146` were checked together: the old W30/W32 inventory
is historical evidence, not a current to-do list.

| Area reviewed | Result and boundary |
|---|---|
| Integration, solvers, equilibria, fitting, shooting, spectra, nullclines | Shared LU/eigenvalue ownership mostly established. Keep differing numerical contracts; see findings 9 and 17. Transpose still bypasses typed validation (finding 4). W488/W496 already changed the old Stiff and spectrum defects; do not propose them again. |
| AUTO (`autlib*`, `auto_*`, `autpp`, `autevd`, `diagram`, `setubv*`, `gogoauto`, math AUTO/EISPACK) | Included in the same searches for sizes, reads, state, errors and duplication. Most mutable computation state is already `AutoLib`/Session state. The reported augmented-matrix copies remain justified; large setup functions are a later refactor (findings 9, 17). |
| Model/parser/converter, expressions, networks and tables | W490's fixed-value reader and W497's table-length check are present. Table *values* remain unchecked (finding 3). Foreign `.ode` conversion has intentionally distinct spelling rules; do not blindly replace every `atoi` found by grep. |
| Saved sessions/settings, recordings and animation | `read_lines`/setx, archive bounds and model-file permissions are reusable owners. Animation replacement still mutates before validation; the stream adapter reopens a file (findings 5, 6). |
| Graphics, pictures, data and protocol | W492's PDF/SVG picture owner and line-style hello data are present. Mark glyphs still have a core/page copy (finding 7). JSON numbers have a custom finite cutoff (finding 8). |
| Base/platform/HTTP/inbox/job modules | Keep process/thread resources distinct from model state. No new shell-from-input claim: browser launch uses argv with `posix_spawnp`; model-file access goes through approval and `open_read_within`. This is source review, not a penetration test. |
| Tests | Repeated config environment helpers are actionable (finding 10). Keep awkward-value fixtures and explicit independent numerical expectations. No speed assertion was established by the clock/sleep search. |
| Tools, including profiling/reference/comparison and Python integration tools | Reuse `xppclient` and `cstrip`; one lexer user was missed (finding 11). Independent reference algorithms are kept (finding 17). The ratchets themselves have limits worth fixing (finding 12). |

## Findings requiring work

Sizes are rough **changed lines including checks**, not promises of deleted
lines. “Small fix” means no separate card for that item; the final table packs
it with work in the same area. Proposed card titles are unnumbered: the
coordinator assigns the next W numbers and issues. No issue was created here.

### 1. Bound gzip expansion as ZIP expansion is already bounded

**Evidence:** `core/xpp_zip.cpp:21–41` has `Sink::limit`, but the gzip output
callback does not use it:

```cpp
// xpp_zip.cpp:24,31,41
std::size_t limit = std::numeric_limits<std::size_t>::max();
s.bytes.append(static_cast<const char *>(buf), static_cast<std::size_t>(len));
return put_deflated(buf, len, user) ? 1 : 0;
```

The same owner's ZIP callback, `xpp_zip.cpp:50–53`, already checks
`ofs > s.limit || n > s.limit - static_cast<std::size_t>(ofs)`.
`gunzip_member` at line 101 creates an unrestricted Sink; line 110 appends its
expanded member to the aggregate. `data_formats.cpp:217–222` reads `.csv.gz`
with unbounded `read_bytes` and then invokes `zip::gunzip`.

**Rule:** security at every file reader; single source; no unbounded allocation
from input. **Change:** apply the owner's existing archive budget to compressed
input and cumulative gzip output, including concatenated members; share the
bounded append operation in `xpp_zip`, preserving unrestricted *writing* where
needed. Return a refusal rather than treating a budget failure as OOM.
**Class: card**, approximately 100–180 lines, proposed **Bound compressed and
NumPy data imports** (also finding 2). A small compressed file can currently
exhaust process memory; no resource-exhausting file was run for this audit.

### 2. NPY dimensions bound bytes but do not bound metadata or conversion

**Evidence:** `core/data_formats.cpp:343–354,378–382`:

```cpp
v = v * 10 + static_cast<std::size_t>(ch - '0');
// ...
if (cols && rows > data.size() / size / cols) return false;
// ...
for (std::size_t j = 0; j < cols; j++) {
    std::vector<double> c(rows);
```

For shape `(0, 18446744073709551615)` on a 64-bit machine, with an empty
payload, the size guard compares `0 > 0` and permits the column loop. Every
iteration adds column/name metadata. Decimal accumulation also wraps without
a check. ZIP's expanded-byte/member limits do not limit the number of columns
encoded in one small `.npy` header. Additionally, `parse_npz` at line 398 casts
an arbitrary seed double directly to `int`; CSV's corresponding seed path at
lines 166–171 uses `strtol` and checks only that *some* characters were read.

**Rule:** security, single source for numbers, all-or-nothing validation.
**Change:** checked dimension parsing and independent row/column/metadata
budgets; reject overflow before allocation. Use checked integer conversion for
seed metadata, with the same contract for CSV and NPZ. Keep legitimate empty
arrays supported within the budget. Test zero dimensions, huge dimensions,
overflow text, invalid/nonfinite seeds and truncated payloads.
**Class: card**, approximately 120–220 lines, same proposed import card as
finding 1. Static counterexample, not a measured memory-exhaustion test.

### 3. Checked table length, unchecked table numbers

**Evidence:** `core/tabular.cpp:360–390` checks point count with `parse_int`
and `table_count_ok`, but still reads `xlo=atof(line1->c_str())`,
`xhi=atof(line2->c_str())`, and `values[i]=atof(line->c_str())`.
Compare the reusable owner `core/xpp_io.cpp:443–457`: `parse_number` requires
the entire token and rejects range errors; `core/auto_settings.cpp:262–281` explicitly checks finiteness in numeric
validation. `TypedFields::real` (`core/xpp_util.cpp:495–508`) itself does not
add that check, so merely routing tables through it would be insufficient.
Thus `nonsense` becomes zero
in a table while the shared numeric reader refuses it. `nan` endpoints also
pass `xlo>=xhi` because that comparison is false.

**Rule:** single source, security, bad values stop a load with file/line/value.
**Change:** parse each endpoint/value through the shared number reader with
explicit finiteness checks and the existing `problem` place builder. Check
whether trailing nonempty lines are valid foreign-format input before deciding
how to handle them. Keep the temporary values and one final apply; retain
W497's network-length check.
**Class: card**, approximately 60–110 lines, proposed **Validate table numbers
through the common reader**. High release value: silent changes to model data.

### 4. Transpose applies unchecked form values and changes state on cancel

**Evidence:** `core/adj2.cpp:69–74` releases the current transpose and calls
`data_back` *before* `do_string_box_of`. Lines 88–94 use `atoi` and write six
settings individually; `create_transpose` at lines 104–120 uses them in
allocation/index arithmetic. Its upper clamping uses `inrow>s.data_store.rows`
then assigns `inrow=s.data_store.rows`, whereas stored rows are indexed below
`rows`. A negative row/skip is not checked here. Field-kind metadata describes
a form but is not a core validation substitute.

**Rule:** one behaviour in every front end; single numeric owner;
all-or-nothing changes; an array writer checks its bounds.
**Change:** parse a temporary transpose specification with `TypedFields`,
validate every selected source index against actual column/row counts, then
replace the data and settings only on success. Cancel preserves the previous
transpose. Reuse `browse_column_name`/variable lookup rather than introducing
a second naming policy.
**Class: card**, approximately 100–180 lines. **Existing owner:** W494
**Every numeric setting has its range, checked in one place** (#558), whose
roadmap row explicitly leaves the whole-core form audit open. Send this finding
there; do not create a duplicate card unless the coordinator splits its scope.

### 5. Animation replacement is rollback code rather than parse/check/apply

**Evidence:** `core/aniparse.cpp:767–770` frees the previous animation and
forgets its displayed data before calling `load_ani_file`. That reader applies
commands to `s` as it reads (line 744); its failure lambda at lines 729–738
frees commands and restores only saved speed/flag/axes. It cannot restore the
previous animation commands already freed by the caller. The comment at lines
714–717 describes partial rollback, not preservation of the prior animation.

**Rule:** single source at the operation level; all-or-nothing loads; simplest
clean way. `.ani` is foreign input, but importing it should still leave a
working animation intact on refusal.
**Change:** build an animation value off to the side, validate the entire file
and expansion bounds, then swap it into the Session and forget display data.
Use the existing `TypedFields` and model-file reader; remove the manual rollback
list. Test cancellation/error after valid commands with an old animation loaded.
**Class: card**, approximately 180–320 lines, proposed **Replace animations
only after a complete successful read**. W500 already replaced `.ani` numeric
conversion; this is the remaining transaction boundary, not that old work.

### 6. Stream adapter reads a model file twice

**Evidence:** `core/model_files.cpp:93–95` opens, reads/budgets and records
bytes. `open_model_file`, lines 101–103, calls that reader and then opens the
path again when the model is not saved in an archive. In contrast,
`model_file_lines` at lines 112–117 builds its reader from the returned bytes.

**Rule:** one read pipeline; single source; security at file input.
**Change:** have FILE consumers read the bytes that were checked/recorded, or
migrate them to the existing text/token reader. Reuse the already implemented
saved-copy adapter if a true stream is necessary. Do not reopen the source
path. A file changed between opens can currently differ from the recorded
model; both opens still use the permission owner, so this is not evidence of a
path-traversal bypass.
**Class: card**, approximately 70–160 lines, proposed **Read recorded model
bytes once for every consumer**. Preserve the callers' text/binary semantics.

### 7. Annotation marker geometry still has a core/page copy

**Evidence:** the same glyph is expressed as relative pen instructions in
`core/grobs.cpp:103–139` and absolute paths in `web2/src/plot/marks.ts:60–67`:

```text
grobs.cpp:107–108 (box): 0,-6,-6, 1,12,0, 1,0,12, 1,-12,0, 1,0,-12
marks.ts:61 (box): [[[-6,-6],[6,-6],[6,6],[-6,6],[-6,-6]]]
grobs.cpp:118 (triangle): 0,-6,-6, 1,12,0, 1,-6,12, 1,-6,-12
marks.ts:63 (triangle): [[[-6,-6],[6,-6],[0,6],[-6,-6]]]
```

The circle differs too: the core's polygon walk is stored in the table, while
`marks.ts:76–78` requests a 24-sided polygon. W492 already supplies *curve*
glyphs/dashes from `picture.cpp` in `hello.line_styles`; that does not cover
these annotation glyphs.

**Rule:** single source; one behaviour, front ends differ only in presentation.
**Change:** give annotation glyph geometry one core owner and send its paths
in hello, following the established curve-style mechanism. Share data, not a
second drawing framework. Decide the intended circle shape deliberately;
preserve coordinate/size presentation differences that the contract requires.
**Class: card**, approximately 140–240 lines, proposed **Share annotation
glyphs between pictures and the page**. Lower priority than input integrity.

### 8. JSON finiteness is a custom range with an unexplained cutoff

**Evidence:** `core/json_number.cpp:10` says
`return v == v && v <= 1e308 && v >= -1e308;`. Both number writers call it;
therefore finite values between `1e308` and `DBL_MAX` are emitted as `null`.
Compare `core/auto_settings.cpp:160`:
`o += std::isfinite(v) ? xpp::number(v) : std::string("null");`.
These are two definitions of the same JSON validity decision. Also, the
function named `json_append_number_shortest` (lines 27–29) implements a
15/17-digit round trip, not a shortest-representation algorithm.

**Rule:** single source; no unexplained default/limit; one behaviour.
**Change:** use standard finiteness and the existing number-formatting owner.
Add edge-value checks for both writers. Keep the existing 15/17-digit byte
format if recordings depend on it, and name/document it accurately; changing
serialization spelling needs explicit compatibility/baseline review.
**Class: card**, approximately 45–90 lines, proposed **Use one JSON numeric
validity rule**. A smaller “rename only” fix was rejected because it leaves
valid numeric results turned into null.

### 9. Startup options still bypass the command-line owner

**Evidence:** `core/comline.cpp:104–195` parses checked options from `my_cmd`;
lines 127–130 reject a nonrepeatable option given twice; `whole_number` at
lines 137–140 calls `parse_int`. But `core/xppautx_main.cpp:251–294` scans and
compacts argv first, including `port = atoi(argv[++i])` at line 278 and repeated
mode assignments at lines 271–277. Those consumed arguments cannot be checked
by `check_command_line`. `core/xpp_batch.cpp:53–91` scans argv again for quiet
and logfile; its comment permits repeats although the new table marks each
nonrepeatable. `comline.cpp:204–214` parses once to check and again to apply.

**Rule:** single source; choice named; simplest clean way; comments match code.
**Change:** parse startup and load arguments into one value; validate port and
mode combinations there; apply the already parsed logging options before
loading. Keep early help/version/schema commands explicit. Remove the old
quiet/logfile rescan and update the contradictory comment. Do not introduce a
registry just for this one executable.
**Class: card**, approximately 180–300 lines, proposed **Parse every startup
option once**. W493 completed the model-option half; this proposal concerns
remaining main/batch scans, not a reversal of that migration.

### 10. Four unit tests copy the config environment operation

**Evidence:** `tests/test_options.cpp:68–75`, `tests/test_keymap.cpp:51–58`,
`tests/test_recent.cpp:21–28`, `tests/test_prefs.cpp:21–28` all contain:

```cpp
#ifdef _WIN32
    return _putenv_s(xpp::files::CONFIG_DIR_ENV, dir ? dir : "") == 0;
#else
    return dir ? setenv(xpp::files::CONFIG_DIR_ENV, dir, 1) == 0 : unsetenv(xpp::files::CONFIG_DIR_ENV) == 0;
#endif
```

Recent/prefs additionally copy `abs_path` (`recent:31–39`, `prefs:31–39`),
including both `return "C:/models/" + name;` and `return "/models/" + name;`.
Dupcheck's default `core_files` at `tools/dupcheck.py:108–114` does not scan tests.

**Rule:** single source; no per-file helper copies. **Change:** put these
small fixture operations in a test support header used by the four tests.
Keep test assertions and independent expected data local; do not make a generic
fixture class or move test-only environment mutation into the product.
**Class: small fix**, approximately 45–75 lines, packed as **Unify repeated
unit-test setup helpers**. No separate card for each copy.

### 11. A source-check lexer remains outside the shared owner

**Evidence:** `tools/keycheck.py:31–36` strips block/line comments with two
regular expressions, including `re.sub(r'//[^\n]*', '', text)`. The shared
owner at `tools/cstrip.py:19–48,55–56` tracks quoted literals and exports
`strip_comments`. On a line `const char *s="https://example";`, keycheck's
operation removes part of the string; the owner preserves it. These perform
the same comment-removal operation with different correctness.

**Rule:** single source; finished migration leftovers. **Change:** use
`cstrip.strip_comments` in keycheck, with a focused regression for comment
markers in literals. W481 already created that owner; there is no reason to
maintain this weaker copy. `deadcheck.lex` additionally returns comments and
is not the same API; the AWK consumers serve shell pipelines. Keep those
until an actual shared need justifies extending an interface.
**Class: small fix**, approximately 15–35 lines, packed with finding 13 as
**Finish source-check cleanup and refresh obsolete reasons**.

### 12. Global ratchet can accept an incomplete object inventory

**Evidence:** `tools/globalcheck.sh:63–66` silently continues when an expected
object is absent; lines 81–84 fail only when the symbol inventory is empty
*and* no top-level `.o` exists. A builddir holding a subset of objects can
therefore print a smaller total and pass (the check fails on growth only).
The dead-code build is the intended complete producer, but the public
`--builddir` argument does not enforce that precondition.

**Rule:** checks that can be trusted; simplest clean way. **Change:** verify
the expected core object inventory for the selected configuration before
counting, or consume the build's authoritative object list. Do not accept an
arbitrary partial folder as a clean global-state audit. Test omission of an
object that owns mutable state.
**Class: small fix**, approximately 35–65 lines, packed with finding 11/13.
This is a check weakness, not a newly observed product global.

### 13. Obsolete exception reasons and an explicitly unreachable fallback

**Evidence and proposed action:**

| File:line / entry | Current evidence | Classification, rule, change and rough size |
|---|---|---|
| `tools/externcheck.sh:25–31`: seven CVODE headers, reason “W34 (#72) decides” | `docs/roadmap.md:90` marks **W34 Evaluate SUNDIALS CVODE** done with the old solver retained; line 274 marks **W183 Assess W9, W34 and W111** done without adopting a replacement. The absorbing decision is settled, not pending. | **Card**, C++ API / one codebase: audit those seven headers and C++ callers and remove unnecessary C linkage; do not alter arithmetic. Proposed **Finish the retained CVODE API migration**, 120–240 lines including checks. A numerical keep reason cannot justify C linkage when all callers are C++. |
| `tools/aliascheck.py:27–33`: `H_BIAS`, `HMIN_DEFAULT`, `HMAX_INV_DEFAULT`, `RDIV`, `Q_MAX`, `logical`, `doublecomplex`, all justified as “vendored” | The first five still rename constants (`cvode.cpp:53–54,74,150`, `cvode.h:427`); `auto_f2c.h:19` still says `typedef integer logical`. AGENTS.md says vendored means `third_party/` only. `doublecomplex` at line 18 is an actual struct definition, not a second name for an existing type. | **Card**, one name / one codebase: use the underlying constants/types where they are aliases; retain the real complex type under its accurate reason. Pack CVODE aliases with its API migration; replace `logical` in AUTO separately, 80–160 lines. No argument to replace numerically distinct f2c abs/min/max functions. |
| `tools/errorcheck.py:83`: `no destination path given` | That text is absent from `core/xpp_io.cpp`; current `Writer::commit` at line 406 returns `cannot be written: no open temporary file` with `Place{path_}`. | **Small fix**, no dead code: remove the unmatched allow entry, about 1–3 lines, in the source-check cleanup pack. |
| `tools/errorcheck.py:90–91`: `model_failed(what) outside a load ... never reached` | `core/xpp_batch.cpp:304–308` builds an explicitly placeless Error in that branch. The reason itself says no production use exists. | **Card**, YAGNI / errors as values: prove callers' load precondition, then remove the fallback and exemption together (or document a real external caller with its place). Pack with startup/load ownership, 20–45 lines. Do not silently replace a needed error by an assertion. |
| `tools/formatcheck.sh:9`: “xpp_http.cpp is excluded: W7 converts it separately” | The actual `ALLOW` is empty and the file loop scans core sources. W7 is long completed; the exclusion is not there. | **Small fix**, comments match code: delete/update that sentence, 1–3 lines, same source-check cleanup pack. |
| `core/ui_json_internal.h:1–2`: “the C API is ui_json.h”; `core/xpp_io.h:364–365`: “LineReader and LineReader” | The protocol headers are C++; the second reader is `TokenReader`. | **Small fix**, comments match code: correct these nearby descriptions, 2–5 lines, attach to W500's existing buffer/util cleanup rather than a standalone card. |

## Allowlist and baseline dispositions

### 14. Dupcheck: retain small adapters, reconsider numerical extraction only later

**Class: keep**, zero lines required now, except inaccurate “vendored” wording
can travel with finding 13. Rule considered: single source versus simplest
clean way and numerical stability. All 10 reported pairs are accounted for:

| Pair and quoted evidence from both sites | Decision and absorbing-card state |
|---|---|
| `cvband.cpp:355–374` / `cvdense.cpp:312–331`: both scale BDF corrections with `N_VScale(TWO/(ONE + gamrat), b, b);`, after respectively `BandBacksolve(M, pivots.data(), b)` and `DenseBacksolve(M, pivots.data(), b)` | **Keep** matrix-specific callbacks; the shared correction is only three statements. W33a is done (`roadmap:83`). A new matrix-policy layer would cost more than it removes. Owning CVODE still follows every nonnumerical rule; “vendored” is not an exemption. |
| `cvband.cpp:379–392` / `cvdense.cpp:336–349`: `BandFreeMat(M); BandFreeMat(savedJ); delete cvband_mem;` versus `DenseFreeMat(M); DenseFreeMat(savedJ); delete cvdense_mem;` | **Keep** type-specific cleanup callbacks; address linkage/aliases independently in finding 13. |
| `cvband.cpp:165–183` / `cvdense.cpp:137–155`: repeated `#define N (cv_mem->cv_N)` through the CVODE member substitutions | **Keep now**, but these nonnumeric macros can be simplified during retained-CVODE cleanup. A shared header of temporary field macros would spread them, not remove the indirection. |
| `graphics.cpp:561–572` / `584–595`: both clip with `if((x1>x_right)||(x1<x_left)||(y1>y_top)||(y1<y_bottom))return;`, then `scale_to_screen`; final calls are `point` and `bead` | **Keep** short primitive adapters. W33f is done (`roadmap:88`); no delayed absorbing task remains. |
| `odesol2.cpp:80–86` / `255–261`: `return fixed_steps(...)` with `one_step_discrete(...)` versus `one_step_euler(...)` | **Keep**: the operation is already unified in `fixed_steps`. W33a done. Another wrapper would obscure the selected method. |
| `xpp_http.cpp:1306–1312` / `xpp_inbox.cpp:181–187`: lock, set `released=true` / `closed=true`, broadcast `released_cond` / `ready`, unlock | **Keep** independent shutdown invariants over independent locks. The earlier callback duplication was actually merged by W398 (`roadmap:1043`); these are not that missed copy. |
| `autlib3.cpp:857–883` / `1101–1127`: both `findlb(...); readlb(...);`, `period=par[auto_unknown::period]`, `rimhb=tpi/period`, and matrix zero/diagonal setup | **Keep now** formulation setup: subsequent `funi` versus `fnws` Jacobians and assembly differ. W337 is done (`roadmap:982`), owning the kernels, not promising to merge problem formulations. Extracting just the common setup might save tens of lines but is not a release blocker. |
| `xpp_math_eispack.cpp:212–248` / `590–626`: `*ierr=0; norm=0.; k=1;` followed by the same Hessenberg norm and isolated-root loop | **Keep now** hqr/hqr2 working-state initialization. |
| `xpp_math_eispack.cpp:251–412` / `629–790`: both begin `its = 0; na = en - 1; enm2 = na - 1;`, then use the same double-QR iteration; both have `tst1 = s;` and `if (tst2 == tst1)` for floating-addition convergence | **Keep now**, not because AUTO is excluded: a shared QR implementation must explicitly support eigenvector accumulation without changing operation order. W337 ownership is complete; this would be a new numerical refactor requiring its own exact tests, roughly 250–450 changed lines. |
| `xpp_math_eispack.cpp:415–438` / `801–824`: row/column modification; both have `h__[k + j * h_dim1] -= p * x;` and `h__[k + 1 + j * h_dim1] -= p * y;` at 422–423 / 808–809 | **Keep now** with the preceding hqr/hqr2 contract, not a separate micro-helper. |

The source checker reports **normalised** block sizes of 19, 19, 24, 119 and
18 lines. Source ranges include comments/blank lines; these are not five
additional complete numerical algorithms. No active “W32 will absorb this”
promise remains in dupcheck's ALLOW. `discrete`'s entry explicitly records
that the old loop was merged. Keep that useful explanation rather than treating
its completed card number alone as proof of staleness.

### 15. Remaining baselines are mostly real owner/platform exceptions

**Class: keep** unless finding 13 or 16 names work. Rule considered: no dead
code, no global state, safe C++, C++ API and every error has a place.

| Check / locations | Entry audit and disposition |
|---|---|
| `tools/deadcheck.py:51–56` | Five entries: Windows `rand_s`, POSIX `environ`, and generated `web_assets`, `icon_png`, `window_lib`. Definitions come from libc/generated build sources; no obsolete absorbing card. Keep all five. The check also rejects unmatched entries. |
| `tools/deadcode.sh:33` | `ALLOW=""`: no stale allow entries to remove. This Ubuntu host omits WebKitGTK; its known window/HTTP-only functions must be reported as host limitations, not deleted as unused on every platform. See actual gate result below. |
| `tests/unsafe.baseline:1–4` | Six `files` matches: `xpp_files.cpp 3`, `xpp_io.cpp 1`, `xpp_io.h 1`, `xpp_log.cpp 1`. These are owner-level stream lifetime/I/O operations, not unfinished W33 sweeps. Keep. This heuristic does not count all raw buffers, unchecked parsing or bounds: findings 1–4 passing it is expected. |
| `tests/files.baseline`, `tests/errors.baseline` | Both empty. Nothing stale to lower. No direct file-operation regression and no unallowlisted placeless-error match. Manually review error propagation; these counts do not prove it. |
| `tools/errorcheck.py:74–89` | CVODE argument checks, terminal OOM paths, thread/port startup failures and CLI usage still have real contexts where no model line exists. Keep those reasons; remove the obsolete Writer match and investigate the explicitly unreachable model fallback as in finding 13. |
| `tools/externcheck.sh:23–24` | Keep `xpp_window_plugin_init` (dlsym export) and `rand_s` (C runtime). Seven CVODE header entries belong to finding 13. |
| `tools/aliascheck.py:20–26` | Keep platform shims: `stat`/`_stat64`, sockets and descriptor functions. They are platform type/function differences, not aliases of our own domain names. The remaining seven entries (five CVODE constants, `doublecomplex`, `logical`) are reviewed in finding 13. |
| External globals, `tests/globals.baseline:1–7` | Colormap/UI installation, protocol client, program and logging configuration are process/front-end resources, not per-model numerical state. Command-line state needs finer ownership (finding 16); do not relocate the entire protocol client into Model. |
| Internal globals, `tests/globals_internal.baseline:1–27` | Checked every reason against its named owner: data subscription clients (ani/marks/phase/plot/numerics), protocol IO/asks/player/record/windows, session list/load, cached const tables/about/topology, file/HTTP/inbox/job/log/window/loader/process resources. The reasons remain meaningful; no AUTO numerical global exemption remains. `comline` is the exception in finding 16. W500 has already refreshed counts; do not repeat its old baseline cleanup. |

### 16. Command-line application state is load-owned despite its global exemption

**Evidence:** `tests/globals.baseline:2` permits
`core/comline.cpp 6 the command line's options (process-wide, AGENTS.md)`;
`tests/globals_internal.baseline:3` permits one more for the same reason.
But `core/comline.cpp:198–228` stores `CommandLine applied`, six external
variables (one vector and five booleans), reparses/replaces them for each model
load, and copies `cl.include` into global `include_files`. Its comment explicitly
mentions File > Open and Reload. Those are a load's derived values, not immutable
startup settings. This is a narrower concern than the accepted process-wide
startup argv or log configuration.

**Rule:** what a load makes belongs to the Model/Session; one source of values.
**Change:** distinguish the immutable parsed startup request from values a Load
applies, passing the latter to consumers rather than publishing a global copy.
Update the two baseline entries after the move. Check failed load/reload and
second-session use; no new registry/current-session accessor.
**Class: card**, approximately 120–240 lines, pack with **Parse every startup
option once** (finding 9) because the same functions and gates are involved.
This is an ownership defect supported by the code, not a claim that a completed
roadmap card promised to remove every permitted process resource.

## Deliberate keeps and later refactoring

### 17. Keep distinct numerical contracts and existing independent oracles

**Class: keep**, zero release change. Rule considered: single source, YAGNI,
no unintended numerical change. Search included AUTO and CVODE, not only the
ordinary integration solvers.

- **LU:** `xpp_math.cpp:294–349` uses LINPACK partial pivots and row storage;
  AUTO's `xpp_math_auto.cpp` `ge`/`nlvc` use complete pivots and null-space/
  determinant contracts (`xpp_math_auto.h:14–39`). CVODE's `dense.cpp:161–188`
  backsolve uses `real**` column storage, e.g. `col_k=a[k]; b[i]+=mult*col_k[i]`;
  LINPACK's `xpp_math.cpp:334–346` instead uses `saxpy(...a+lda*(k+1)+k,lda,...)`.
  They are related elimination algorithms, but differ in layout, pivot/error
  interfaces and zero-update skipping. Do not hide conversion copies in an
  adapter or substitute AUTO's solve. A future same-order CVODE/LINPACK
  unification needs a dedicated numerical study, not a blanket exception for
  all nonnumerical code around them.
- **Eigenvalues:** `xpp_math.cpp:468–617` is the fixed-tolerance equilibrium QR
  route; `xpp_math_eispack.cpp:136,489,1844,2132,2659` provides AUTO's hqr/hqr2
  and generalized QZ variants, with vector accumulation and floating-addition
  convergence. `xpp_math.h:9–15` names the distinction. Keep separate algorithms
  inside the existing math owner. W337 is done; it already moved ownership.
- **Norms:** `adj2.cpp:528–539` does `sum+=vi*vi; sum=sqrt(sum); vi=vi/sum`;
  `xpp_math_eispack.cpp:3098–3281` `dnrm2` uses cutlo/cuthi and scaled phases
  to avoid overflow/underflow; `vector.cpp:274–287` is a weighted RMS.
  AUTO's `autlib1.cpp:3990–4063` integrates a collocation-weighted L2 norm.
  These are different reductions/normalizations, not interchangeable helpers.
  Moving adjoint normalization to the math owner could be tidy, but no second
  caller needing exactly its contract was established. Keep for release.
- **Foreign import and independent tools:** `.ode` import intentionally accepts
  XPPAUT grammar while `.odex`/setx are strict. `tools/odex_model.py` and
  `tools/python-integration/` independently evaluate/reference model behaviour;
  sharing the C++ parser/solver would weaken the oracle. Keep independent
  implementations with explicit refusal sets. Duplicate *test transport* or
  *source lexing* has no such justification (findings 10, 11).
- **Input helpers whose parameters do vary:** `slider_wrong(...,check_names)`
  is not YAGNI: `model_options.cpp:1195–1210` passes the pre-compilation
  `check_names` flag, while `json_state.cpp:686` uses the checked default.
  Keep the parameter rather than deleting it after finding only the direct
  protocol caller. `AutoParallel`'s persistent workers/scratch and failure
  publication (`auto_parallel.cpp:15–166`) have real repeated callers and
  concurrency invariants; do not replace them with fresh threads per interval.

### 18. Long modules: refactor by responsibility, after correctness fixes

**Rule considered:** simplest clean way, single source, YAGNI.

| File:line | Classification, proposed change and rough size |
|---|---|
| `core/xpp_util.cpp:220–281,472–652` / `xpp_util.h` | **Card, existing W500 Cleanups left by wave 2 (#564)**: typed field parsing, parameter/IC edits, slider validation and window operations inhabit one utility module. Its roadmap row explicitly reserves the split and Buf cleanup. Split by actual owners/callers, preserving shared `TypedFields`; approximately 250–450 moved/changed lines. Do not propose another parallel card. |
| `core/ui_json_internal.h:281–309`, `json_io.cpp:37–90` | **Card, existing W500**: `Buf` wraps a single `std::string` plus pointer-based wrappers while newer event producers return strings. Move directly to the existing string/JSON append owners; do not replace it with a generic serialization class. Approximately 300–600 changed call-site lines across protocol files, including AUTO. |
| `core/form_ode.cpp:332–577` (`compiler`) | **Keep for release**, later card **Separate legacy model declarations from compilation**, approximately 160–280 changed lines. It dispatches old-format declaration kinds and compiles equations in one function. Split declaration handling only when a concrete parser change needs it; keep foreign import support and conversion gates. Length alone does not justify another parser framework. |
| `core/integrate.cpp:1632–1990` (`integrate`) | **Keep for release**, later card **Separate integration observation from stepping**, approximately 160–300 changed lines. It mixes solver advance, sections, storage and display/progress. Extract existing post-step operations without changing their order if a future card needs the seam. W487/W498 already fixed stop reporting; no fresh rewrite before release. |
| `core/autlib1.cpp:572–918` (`init1`), `auto_nox.cpp:1766–2140` (branch start/extension commands) | **Keep for release**, later card **Separate AUTO start preparation from command presentation**, approximately 180–320 changed lines. W491 already supplies the problem/calculation tables; reuse them. Keep formulation-specific matrix work local, not a registry with flags for every numerical branch. |
| `tools/servercheck.py:260`, `tools/autocheck.py:158`, `tools/web2check.mjs` section runner | **Keep** behavioural sections and existing transport owners. File length is a navigation cost, not evidence that test cases are duplicated. Split sections into modules only when their setup can be passed explicitly without recreating globals or making shared ordering obscure, roughly 200–500 moved lines per area if later needed. |

### 19. User-added annotations still use fixed-capacity arrays

**Evidence:** `core/grobs.h:15–16` defines `MAXLAB 50` and `MAXGROB 400`;
`core/session.h:261–262` uses `std::array<LABEL,MAXLAB>` and
`std::array<GROB,MAXGROB>`. `grobs.cpp:65–75` searches those 50 slots and
returns -1 when full; `add_grob` at lines 221–236 similarly searches 400 slots.
`xpp_session.cpp:408,427` also uses these limits in saved-file group reading.
These are quantities a user adds, not dimensions a numerical method fixes.

**Rule:** an array's size has one source; input-sized collections are vectors;
no unexplained limit. **Change:** keep annotation records in Session vectors,
preserving stable IDs/tombstones as needed by protocol clients. Replace the
reader's slot-count caps with a documented aggregate input-memory budget;
update marks subscriptions and hello from the same owner. Reuse the existing
marks API rather than adding a second annotation store. Check more than 50
labels/400 objects and saved-session round trips. AUTO views do not own a
second annotation array; include all graph consumers in the change.
**Class: card**, approximately 180–300 lines, proposed **Size annotations from
their actual records**. The current tree still has these caps despite the
recent parser/solver limit lifts; this does not reopen those completed cards.

The simpler approach considered was a short list of dupcheck matches plus
blanket removal of “vendored” exemptions. It was rejected: it misses trust
boundary/ownership defects and risks changing numerical contracts with no
release benefit. Conversely, a new general parser/serializer/matrix framework
is unnecessary: the proposals above use existing owners and concrete values.

## Gates and limitations

No implementation gates, example runs, numeric baseline rewrites, browser
checks or full verify run were requested or performed. A build is needed only
for the linker/global source checks. Checks run against the unchanged tree:

| Source check | Result |
|---|---|
| `tools/dupcheck.sh --check` | PASS: 5 function pairs, 5 block pairs, all allowed. |
| `uv run --locked python tools/deadcheck.py --check` | PASS: nothing dead beyond the five allow entries. |
| `tools/unsafecheck.sh --check` | PASS: 6 idioms, baseline 6, delta 0. |
| `tools/filecheck.sh --check` | PASS: 0 direct operations, baseline 0. |
| `uv run --locked python tools/errorcheck.py --check` | PASS: 0 errors without places, baseline 0. |
| `tools/externcheck.sh` | PASS: 9 C-linkage sites in the allowlist, 7 headers. |
| `tools/aliascheck.sh --check` | PASS: 0 unallowed second names in 453 files. |
| `uv run --locked python tools/findingscheck.py` | PASS: 2 findings documents. |
| alloccheck, formatcheck, literalcheck, stdoutcheck, sessioncheck, mathcheck, sleepcheck | PASS: all 7 unchanged. |
| `uv run --locked python tools/keycheck.py` | PASS: main/File/numerics keys agree. |
| `JOBS=6 tools/deadcode.sh --check` | Exit 1: exactly the 13 known host-only/window-related entries, 0 additional entries. No allowlist changed. The required -O0 linker-check build completed. |
| `tools/globalcheck.sh --check --builddir build/deadcode` | PASS: 13 external (baseline 13), 87 internal (baseline 91). The four absent symbols are `xpp_window.cpp` 2→0 and `xpp_window_loader.cpp` 2→0 on this no-WebKitGTK host; retain portable baselines. |

Security review is static: it established input-dependent unbounded work in
findings 1–2 and unchecked data in findings 3–4; it does not certify all readers
or claim arbitrary code execution. No dangerous allocation probe was run.
No product numbers changed. No follow-up is implemented by this study.
The linker-check build output was removed after recording the results; no
background run or scratch file remains. W494
and W500 remain existing work in progress; findings assigned there are evidence
for the coordinator, not permission to grow a running card silently.

## Ranked follow-up cards

Correctness/security first; low-risk small fixes can be packed with their
owners. Sizes below include the findings' checks, and are overlapping estimates
within a pack. Proposed titles need coordinator-assigned W numbers/issues.

| Rank | Proposed title / existing card | Findings | Rough changed lines | Why it matters for release |
|---|---|---|---|---|
| 1 | Bound compressed and NumPy data imports | 1, 2 | 220–400 | A tiny sent file can cause unbounded expansion/metadata work; unify budgets and checked dimension/seed conversion. |
| 2 | Validate table numbers through the common reader | 3 | 60–110 | Malformed/nonfinite table values silently enter model data despite W497's checked length. |
| 3 | Existing W494 Every numeric setting has its range, checked in one place (#558) | 4 | 100–180 | Transpose needs core bounds and transactional application, including cancel; coordinate its remaining form audit. |
| 4 | Parse every startup option once | 9, 16; model-load fallback part of 13 | 300–540 | Eliminate validation bypass, repeated scans and load-derived globals in the same file area and gates. |
| 5 | Replace animations only after a complete successful read | 5 | 180–320 | A refused file currently discards the working animation; simplify rollback into a value and swap. |
| 6 | Read recorded model bytes once for every consumer | 6 | 70–160 | Parsed bytes must equal the checked/saved model bytes; remove the second open. |
| 7 | Use one JSON numeric validity rule | 8 | 45–90 | Finite results above 1e308 must not silently become null; review serialization spelling separately. |
| 8 | Finish source-check cleanup and refresh obsolete reasons | 11, 12; small tool fixes in 13 | 55–105 | Small same-area pack: correct shared lexing, detect partial inventories and remove stale exceptions/comments. |
| 9 | Unify repeated unit-test setup helpers | 10 | 45–75 | Small same-area pack; remove four platform environment copies without generalizing the test framework. |
| 10 | Finish the retained CVODE API migration | CVODE linkage/constant entries in 13; 14's macro cleanup if useful | 200–400 | Completed SUNDIALS evaluation no longer justifies postponing C++ linkage/alias cleanup; leave numerical order intact. |
| 11 | Replace AUTO's logical alias with its actual type | f2c entry in 13 | 80–160 | Apply one-name/one-codebase rules to AUTO too; preserve integer semantics and the real complex struct. |
| 12 | Share annotation glyphs between pictures and the page | 7 | 140–240 | Remove the remaining geometry copy using the established hello-data mechanism. |
| 13 | Existing W500 Cleanups left by wave 2 (#564) | 18, small header-comment fixes in 13 | 550–1050 | Finish the already named Buf/utility cleanup after overlapping waves; do not spawn a duplicate migration. |
| 14 | Size annotations from their actual records | 19 | 180–300 | Remove arbitrary user-visible slot caps; preserve stable references and bounded file input. |
| Later | Separate legacy model declarations from compilation; separate integration observation from stepping; separate AUTO start preparation from command presentation | 18 | Separate owner-sized cards, 160–320 each | Navigation/maintenance benefit, but less release value than input integrity; keep exact arithmetic/event order. |
