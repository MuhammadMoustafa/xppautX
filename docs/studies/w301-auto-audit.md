# W301: AUTO code-quality audit

Card [W301 / #359](https://github.com/MuhammadMoustafa/xppautX/issues/359),
2026-10-13. Audited `task/W301-auto-audit` from `a8c4acce`; the preceding
commits are W298's review (`213c30dc`) and implementation (`c53e1af4`). W300's
reference comparison is present (`05136f7a`, `4ed2cae4`). This is an audit and
one bounded storage fix, not a wholesale rewrite of the continuation algorithms.

## Scope, method and ownership

The scope is **34 files: 20 C++ sources and 14 headers**:
`autevd.{cpp,h}`, `autlib1.cpp` through `autlib5.cpp`, `auto_c.h`,
`auto_data.{cpp,h}`, `auto_f2c.h`, `auto_jacobian.h`, `auto_nox.{cpp,h}`,
`auto_parallel.{cpp,h}`, `auto_settings.{cpp,h}`, `auto_stability.{cpp,h}`,
`auto_state.h`, `auto_stop.{cpp,h}`, `autpp.cpp`, `conpar2.cpp`,
`diagram.{cpp,h}`, `gogoauto.cpp`, `json_auto.cpp`, `setubv2.cpp`,
`xAuto.h`, `xpp_session_auto.{cpp,h}`, and `xpp_session_auto_io.cpp`.
AUTO's EISPACK dependency and the numerical owners in `xpp_math` and `gear`
were examined separately. EISPACK is in `core/`, so it is ours too.

AGENTS.md's Code quality table and Task agents section both already say
that every cleaning/unification/limit lift covers AUTO. No rule edit was needed.
The Threads section requires worker-private scratch, immutable model inputs,
disjoint mesh blocks and caller-ordered shared condensation updates.

Before changing code, searches covered these files, all of `core/`, and the
owners `Model`, `Session`, `auto_state`, `auto_settings`, `xpp_session_auto*`,
`xpp_io`, `xpp_files`, `xpp_error`, `xpp_ui`, `xpp_math`, `auto_jacobian`,
`auto_parallel`, `data_event`, and W243/W298's model-sized tables. The storage
fix reuses `Model::node`, the existing labels/kinds vectors and the shared
`do_string_box_of(std::span<std::string>)` API. It adds no helper or state.

The counts below are **source-search candidates, not compiler-certified defects**.
Comments and string/character literals were stripped using `errorcheck.strip`.
Fixed-array candidates match `std::array<` or primitive declarations with a
literal first extent. Code candidates match assignments/comparisons of
`ips`, `itp`, `itpst`, `itp1`, `itp2`, `isw`, `isp`, `nins`, `istab`,
`stable` or `type` against a literal. Numeric slots match `par[digits]` or
`icp[digits]`. These counts exclude many symbolic C-array extents and mathematical
coefficients; the manual inventory below adds the significant missed cases.
Line numbers refer to this card's source tree. Mutable-state numbers are the
existing Linux baseline records, not a fresh Linux object audit.

Reproducible starting searches (Git Bash; inspect comments rather than counting
these raw grep results as the stripped-source totals):

```sh
git grep -n -E 'std::array|KREDO|MAX_AUT_PER|AUTO_SETTINGS_MARKS' -- core/
git grep -n -E 'ips|itpst|itp1|itp2|STABLE_EQ|STABLE_PERIODIC' -- core/
git grep -n -E 'malloc|calloc|realloc|free|new|delete|memcpy|memmove|memset' -- core/
git grep -n -E 'static|thread_local|WIN32|unlink|mkdir|popen' -- core/
git grep -n -E 'printf|scanf|fopen|fclose|rewind|ftell|fseek|auto_fail' -- core/
git grep -n -E 'parameter_slot|model_parameter|diagram_parameter' -- core/
tools/heavy.sh python3 tools/dupcheck.py --check
tools/heavy.sh python3 tools/errorcheck.py --list
```

## Inventory and disposition

Proposals P1–P11 are cards W327–W337 (issues #388–#398), named at each
proposal below.

| Idiom | Places / measured candidates | Disposition |
|---|---|---|
| 1. Fixed arrays; bare sizes/defaults/slots | 47 array declarations before the fix, **46 after**; 182 literal par/icp subscripts; fixed Mark values, collocation and MPI dimensions detailed below | Fixed homoclinic form; P5 names/sizes the rest, P6 removes obsolete MPI storage, P4 separates parameter meanings |
| 2. Integer codes | 291 broad literal-code assignments/comparisons; narrower subsets: 125 IPS and 56 ITP/ITPST sites; enums already exist for diagram classification and stability kind but APIs accept int | P3; no numeric encoding changed here |
| 3. Memory and views | **0** raw allocation/deallocation calls; **2** memcpy sites; six borrowed state/eigenvalue pointers in DIAGRAM, six backing point vectors, scratch pointer mirrors and long pointer/stride APIs | P8, after P4/P7; owning storage is already RAII |
| 4. Mutable static/file-scope state | AUTO baseline: **0 external**, **11 internal** symbols in three files; EISPACK dependency baseline: **461 internal** | P9 for client state; P10 for EISPACK scratch; no baseline widened |
| 5. Threads | Two setubv mesh dispatch sites and one conpar dispatch; private AutoLib scratch, shared Session pointer and publication assumptions | P10 verifies/seals worker inputs; P6 distinguishes dormant MPI from actual collocation workers |
| 6. OS-specific code | **0** active OS conditionals/process/filesystem calls matching WIN32/Windows/Linux/macOS, unlink/rmdir/mkdir/chdir/getcwd/system/popen/native handles | No OS-portability fix identified; file owners already used; four raw stream-position calls remain under P2 |
| 7. I/O and error places | **0** printf/scanf/open/close-style calls; four rewind/tell/seek calls; 256 xpp::print calls already use checked formatting; errorcheck reports **0** unplaced sites and has **0 AUTO ALLOWED entries**; 19 auto_fail call sites still throw text | P2; checker result does not prove preservation of a file's original Error/Place |
| 8. Duplicates and numerics | **6 AUTO duplicate-function groups** (17 named allowlist entries), **10 AUTO block groups**, plus three EISPACK block groups | P7/P6 for surrounding copies; numerical differences and owner migration under P11; detailed classification below |
| 9. W298 index mapping | auto_state.h:87–100, auto_settings.cpp:471–481, autevd.cpp:46–47,116–122, autpp.cpp:47,87,97, xpp_session_auto_io.cpp:126 | **Proposal P4 only**; no mapping change in W301 |

### 1. Arrays and unexplained sizes/defaults

Counts per file before the storage fix (literal search results, not an assertion
that every occurrence is arbitrary):

| File | Fixed-array candidates | Code candidates | Literal par/icp subscripts |
|---|---:|---:|---:|
| autevd.cpp | 0 | 1 | 10 |
| autlib1.cpp | 3 | 118 | 56 |
| autlib2.cpp | 10 | 2 | 0 |
| autlib3.cpp | 5 | 2 | 105 |
| autlib4.cpp | 2 | 0 | 0 |
| autlib5.cpp | 0 | 1 | 9 |
| auto_nox.cpp | 4 → 3 | 120 | 0 |
| auto_settings.cpp | 1 | 0 | 0 |
| auto_settings.h | 6 | 0 | 0 |
| auto_state.h | 3 | 0 | 0 |
| auto_stability.cpp | 0 | 1 | 0 |
| diagram.cpp | 1 | 2 | 0 |
| gogoauto.cpp | 2 | 42 | 0 |
| json_auto.cpp | 2 | 0 | 0 |
| setubv2.cpp | 0 | 2 | 0 |
| xAuto.h | 5 | 0 | 0 |
| xpp_session_auto.cpp | 2 | 0 | 0 |
| xpp_session_auto.h | 1 | 0 | 0 |
| Other scope files | 0 | 0 | 0 |
| **Total after W301** | **46** | **291** | **182** |

Specific meanings and follow-ups:

- **Fixed here:** `auto_nox.cpp:2148–2188`, get_homo_info, had 100 strings
  but indexed `2 + 2*node` fields. At 50 states the last index is 101; neither
  the form layout nor continuation imposes this limit. Values now use a vector
  of exactly that many fields. The 401-state unit submits 804 fields, checks
  the final left/right names and values, and applies them through the shared UI
  form. This tests the actual consumer, not just the vector's size.
- `auto_nox.cpp:684,748,812`: 9/22/7-string form schemas; auto_settings.h:66–79
  has numeric/range/mark parallel arrays; auto_settings.cpp:303 and
  auto_state.h:48 have four-axis layouts. These are schemas, not model limits.
  P5 puts cardinalities and default rationale in the settings/axis owner.
- **Mark values still have a real fixed user limit:** AUTO_SETTINGS_MARKS=9
  (auto_settings.h:47), MAX_AUT_PER=10 (auto_nox.h:11), 20-element
  AutoState::uzr_par/uzr_period (auto_state.h:105–106), and 100-element
  iuz/vuz scratch (gogoauto.cpp:52–53). The old form selects 0–9 and writes
  nine fields (auto_nox.cpp:680–713). XAUTO adds 20-element run mark arrays (xAuto.h:42–43). These extents are not one schema.
  P5 lifts the mark storage limit; it must follow P1 because foreign import can
  currently supply an unchecked nuzr. Shrinking scratch alone is unsafe.
- autlib1.cpp:5436 and autlib3.cpp:2263,2626,2999,3439,3926 have seven-value
  restart temporaries. autlib1.cpp:6405 has seven printed diagnostic maxima,
  and :2429 has nine diagnostic headings. The restart extent is tied to the
  supported collocation order; the output extent is only a presentation schema.
  Name each in its owner; do not conflate them or silently extend quadrature.
  init checks ncol (autlib1.cpp:3105); weights/points are method-specific.
- autlib2.cpp:751,758,762,765,1495–1497,1864,2147,2152 uses KREDO=1
  (auto_c.h:11) for dormant MPI reduction topology plus a two-value buffer.
  solvbv also multiplies allocations by KREDO (:107,116). P6 removes the
  unreachable MPI protocol after proving the serial reduction's arithmetic order.
- autlib4.cpp:76,90 has one-element SVD/QZ dummy arrays; flowkm calls QZ
  with vectors disabled (:328–343) and SVD with its job argument (:214).
  These are API placeholders, not maximum state counts. P8 documents the job
  precondition in a typed view; do not allocate a model-sized matrix needlessly.
- xAuto.h:10,39–43 has 100-slot icp/ithl/thl arrays and 20-slot iuz/vuz
  arrays. autevd.cpp:31–33 supplies one weighted period entry and :46–50
  supplies five continuation slots, while :69–72 copies nuzr entries. P5/P8
  remove surplus fixed extents or name actual method cardinalities and express
  the run lists with their owner. Do not size a run from imported counts before
  P1 validates them.
- diagram.cpp:439 reads eight foreign .auto parameters. That is XPPAUT's
  import schema, not xppautX's model capacity. P1 names it and validates the
  complete foreign record; it does not add compatibility code for our files.
- AutoOptions defaults (auto_state.h:33–40), auto_c.h:11–20 numerical constants,
  auto_settings.cpp's NumField bounds and autlib1.cpp's `%10000` output
  numbering need owner/reason review in P5. Mathematical coefficients are not
  automatically unexplained limits. Period slot 10 and homoclinic blocks belong
  to P4's layout owner.

### 2. Codes: reuse existing definitions

iap_type (auto_c.h:23–65), BIFUR (auto_nox.h:28–45) and XAUTO hold
IPS/ITP/ISW/ISP as integers. gogoauto.cpp:110–478 selects function tables
with integer combinations; autlib1.cpp:420–703 transforms them into augmented
problems. auto_nox.cpp:1860–1965 splits ITP into `%10` and `/10`; subsequent
start/switch functions set codes directly (:2002–2537). Literal searches also
find presentation type codes and counters, so the broad 291 must not become
291 blind substitutions.

diagram.h:33–35 already owns DiagramPointType and predicates;
auto_stability.h:33–38 owns AutoStabilityKind. Their predicates/call APIs
still accept int, and auto_nox.cpp/diagram.cpp retain numeric classification
comparisons. P3 extends these owners, keeps wire/file integers at explicit
serialization boundaries, and represents composite/signed ITP separately from
base point kind. Counts of stable dimensions (nins) are counts, not enums.

### 3. Memory: RAII exists; shape and lifetime are implicit

No active malloc/calloc/realloc/free/new[]/delete[] or xpp allocation calls were
found in the 34 files. std::vector, UniqueFile, unique_ptr<AutoParallel>
and stack guards already own resources. auto_nox.cpp:1237 copies the grabbed
state with memcpy; auto_settings.cpp:124 copies settings numerics. Neither
call establishes array dimensions or validates a reader.

DIAGRAM (auto_nox.h:64–77) borrows six pointers from DiagramPoint's six
vectors. diagram.cpp:30–40 establishes these mirrors; session parsing and
restoration must rebind them after moves. AutoGlobalScratch mirrors six
vectors owned by AutoLib::store (auto_state.h:177–194,244–248);
allocate_global_memory establishes those pointers (autlib1.cpp:6870+).
The solvbv owner has 12 floating and seven integer parallel vectors
(auto_state.h:255–258); homoclinic eigenvalue/projection blocks have parallel
vectors too (:264–270). AutoStability::Values has separate re/im vectors.

auto_c.h's numerical APIs and setubv2.cpp:12–42's arglist pass pointers,
leading dimensions and products separately. ARRAY2D/ARRAY3D macros trust those
relationships. P8 adds views in the existing numerical owner and passes spans
for vectors; it preserves the column-major layout and loop order. Do not replace
parallel numeric storage with a differently traversed layout in this card.

### 4. Static and file-scope state

The 34 explicit static tokens found are mostly functions, constant label tables
and constexpr class constants; they are not 34 mutable globals. AUTO's numerical
translation has no explicit mutable static locals in the scoped files. The
baseline identifies the actual remaining protocol state:

| Baseline / file | Count | Source ownership |
|---|---:|---|
| globals.baseline, AUTO scope | 0 | No external mutable AUTO entries |
| globals_internal.baseline, auto_data.cpp | 3 | point_of_node callback (:34), ChangedEvent and pacing clock |
| globals_internal.baseline, auto_settings.cpp | 1 | ChangedEvent (:281) |
| globals_internal.baseline, json_auto.cpp | 7 | views/client_views (:59–60), av_on/av_valid/av_sent and grab_key_after |
| globals_internal.baseline, eispack.cpp dependency | 461 | f2c static locals and by-reference constants |

The protocol baselines call these client-owned; ownership is still expressed as
anonymous-namespace data, not a client object. P9 moves subscriptions/cache to
the existing JSON client owner and session-specific diagram data to Session,
with load/session-switch/disconnect tests. It must not reset a client's
subscription just because its active Model changes.

EISPACK's allowlist reason calls it vendored, but its location is core/.
That reason cannot exempt sizes, memory or state. P10 replaces static scratch
with invocation/workspace ownership without changing the f2c arithmetic.
No baseline was rewritten: W301 did not remove counted mutable symbols.

### 5. Thread assumptions: actual workers versus old MPI

auto_parallel.cpp:17–54,93–142 owns and joins the pool; exceptions are stored
per worker and rethrown on the caller. prepare (:72–86) copies parameter
capacity, homoclinic state, rotations, parser constants and variables to each
worker's AutoLib. auto_parallel_ready (:65–68) requires a pure model RHS
and permits maps only with one stored step. autpp.cpp:84–91 evaluates the
worker's parser buffers; its serial branch mutates Session state (:95–104).

setubv2.cpp:131–153 dispatches matrix/residual mesh blocks using local iap
with lib=&worker. The two uses are the AA/BB assembly and residual assembly;
boundary/integral updates remain caller-serial (setubv_make_fc_dd).
conpar2.cpp:187–205 condenses disjoint intervals, then replays saved additions
on the caller. Cancellation is caller-polled around parallel sections. That
order is part of the bitwise baseline, not merely an optimization.

Workers still hold a mutable Session pointer and callbacks must obey purity;
parameter/mesh pointers and the model are shared. P10 makes this boundary
explicit and tests exceptional completion, cancellation and worker-count reuse.
Inspection of fnho -> ffho -> funi (autlib5.cpp:28–131) did not find an
EISPACK call on that worker RHS path. eighi/rg occurs in boundary/postprocess
paths, currently serial. EISPACK is nevertheless non-reentrant; no concurrent
EISPACK execution or data race was reproduced here. No TSan gate was run here.

mynode()/numnodes() return 0/1 (autlib2.cpp:2295–2309). KREDO's single
logical MPI node is **not** the collocation pool's thread count. Substituting
four into that old reduction topology would not implement thread safety. P6
removes the dormant transport; P10 independently verifies actual workers.

### 6–7. Files, output and errors

OS operations are already delegated: RunUnits uses files::open_stream
(gogoauto.cpp:23–39), and auto_nox/diagram use xpp file readers/writers.
No direct shell/process construction or OS branch was found in scope.
The remaining raw C stream operations are rewind at autlib1.cpp:4084,4157,
ftell at :4163, and fseek at :4194. Their return/error status is not
checked. The absolute seek is intentional for Windows text-stream positions;
P2 extends the existing I/O owner rather than recreating another seek helper.

Formatting is already xpp::print: 199 calls in autlib1, 31 in autlib2,
17 in autlib4 and nine in autlib5. Zero active printf/scanf/fopen/fclose-family
calls were found. These print calls are not all errors: fort.7/8 numerical data
and fort.9 diagnostics are outputs with a defined format.

errorcheck --list reports zero unplaced errors, and its ALLOWED list contains
no AUTO entry. However, it does not see errors hidden in text exceptions or
diagnostic output. There are 19 auto_fail calls: autlib1 4, autlib2 1,
autlib4 7, auto_parallel 4, autpp 2, gogoauto 1; auto_state.h:285 is the
throw helper definition. AutoFailed contains only a string (:282–285).
do_auto catches it and gives it a command place (auto_nox.cpp:538–560),
so those numerical failures are not ultimately displayed without a command.
But gogoauto.cpp:27–28 discards open_error/write_error's Error/Place by
passing .what; file provenance can be lost. EISPACK failure is also printed
as a NOTE (autlib1.cpp:3768) rather than always returned as an Error.
P2 preserves error values and defines one command renderer/event while retaining
useful numerical diagnostics. It must test recording step attribution too.

**Unfixed trust-boundary debt, P1:** load_auto_numerics (auto_nox.cpp:2564+)
applies fields as it reads, trusts imported nuzr, and leaves partial state after
short input. load_auto_graph similarly applies fields directly.
diagram.cpp:436–503 appends as it parses, does not validate the entire record,
and returns success after a truncated later point; indices/counts are not fully
validated. Import resets existing data before all validation (auto_nox.cpp:2635+).
make_q_file copies the foreign solution tail without running the shared
solution validator. Thus a sent foreign file can alter partial state, request
excess mark counts (including more than the 20 run slots) or plant malformed restart data. These are source-confirmed
paths, not a claim that all were dynamically exploited. W301's form fix does
not change the import path or assert it is secure.

Our .snapx path is stronger: read_lines, parse_settings,
parse_diagram_csv, check_auto_solutions, members_read/restore_members
validate before applying and bound model-dependent counts. P1 reuses those
owners/validators where the foreign grammar permits; no fallback for our files.

### 8. Every AUTO duplicate allowlist family classified

The full dupcheck passes with 14 function groups and 19 block groups. Six
function groups and ten block groups concern AUTO; block groups overlap and
must not be added as independent copied-line totals. The allowlist's 17 AUTO
function names plus three wildcard file entries are historical exemptions,
not proof that each copy is numerical shape.

| Reported function group | Classification and action |
|---|---|
| autlib1 fnuzae / fnuzbv (:1892/:6279 in dupcheck) | Same UZR scalar test and diagnostic context; not a necessary different numerical algorithm. P7 merges the common evaluator, preserving signed index and output text. |
| autlib2 mynode / numnodes (:2288/:2301) | Normalization masks the different return constants (0 vs 1). Not interchangeable routines; obsolete MPI metadata under P6, not a numerical exemption. |
| autlib3 fnlp / fnhd / fnhb / fnhw | Same finite-difference wrapper shell around distinct evaluators. P7 extends auto_jacobian and preserves each callback, nfpr/parameter range and order. |
| autlib3 fnsp / fnpe | Same wrapper shell, different augmented problem. P7; keep problem equations distinct. |
| autlib3 fnpl / fnpd / fntr / fnbl, autlib5 fnho | Same derivative orchestration. P7; no reason to duplicate allocation/perturbation/error handling merely because equations differ. |
| autlib3 icbl / autlib5 icho | Integral-wrapper shell; P7, retaining evaluator and parameter-mask differences. |

| AUTO block group (dupcheck's ranges) | Classification and action |
|---|---|
| autlib1 2594–2612 / 6438–6456, 17 lines | Point-output context fields; P7/P2 shares schema/context without merging steady and BVP equations. |
| autlib1 4342–4374 / 4392–4424, 19 lines | Integral versus squared-integral setup. The reductions differ (including final sqrt); retain that numerical operation/order, share weight/context preparation under P7 only. |
| autlib1 5606–5651 / autlib5 856–900, 22 lines | Uniform-mesh starting-point setup. P7 shares setup where identical; homoclinic-specific initialization remains explicit. |
| autlib3 908–941 / 1104–1139, 20 lines | Augmented Hopf matrices; signs/block meanings differ. Numerical construction stays until P11 can parameterize it with exact-baseline proof. |
| autlib3 1410–1442 / 1738–1770, 24 lines | Related augmented matrices for different formulations; genuine numerical differences, same P11 constraint. |
| autlib3 1459–1483 / 1783–1807, 18 lines | Identical mesh/eigenvector initialization shell; P7 can share it with explicit sizes and unchanged trig/evaluation order. |
| autlib3 2270–2330 / 2632–2691, 32 lines | Restart header/read boilerplate; P7 extends read_auto_solution_header/restart owner, retaining formulation-specific reshaping. |
| autlib3 2286–2309 / 2648–2671 / 3021–3044, 17 lines | Overlapping restart reader copy; same P7, not three new numerical algorithms. |
| autlib3 2287–2309 / 2649–2671 / 3022–3044 / 3948–3971, 16 lines | Overlaps the prior group; same restart owner under P7. |
| autlib3 3024–3052 / 3951–3979, 22 lines | Restart payload setup; same P7, with explicit target formulation sizes. |

EISPACK's three additional allowed block groups are :195–231/:567–603,
:234–395/:606–767, and :398–421/:778–801: related Hessenberg/QR work
with/without eigenvector accumulation. Keep arithmetic distinctions; P10
removes shared mutable scratch and P11 puts numerical APIs in the owner.
Do not delete an allow entry until its copy is actually removed and dupcheck
has checked the remaining matches; do not broaden wildcard entries.

**Linear solves/eigenvalues against the owner:**

- autlib1.cpp:3921, ge, uses complete row/column pivoting, determinant
  accumulation and pivot diagnostics; nlvc also uses complete pivoting.
  xpp_math.cpp:282,313's sgefa/sgesl uses LINPACK partial pivoting, while
  :348,372 contains bandfac/bandsol. They are not bitwise substitutions.
  gear already calls the owner (gear.cpp:453–467,563–570,838–847); it no
  longer owns a second dense solver. P11 moves AUTO's variant into the same
  numerical owner, preserving algorithm and trace output, not replacing it
  with the existing partial-pivot solver.
- autlib1.cpp:3727, eig, calls EISPACK rg (:3758); homoclinic eigho
  (autlib5.cpp:1366,1434) needs eigenvectors; autlib4 Floquet computation
  uses the generalized QZ/SVD path. These are more than one eigenvalue API.
  gear's old eigen is already xpp::eigenvalues (gear.cpp:91). The owner's
  hqr has a fixed 1e-10 convergence test (xpp_math.cpp:446–452); EISPACK
  uses floating precision tests and optional eigenvector accumulation.
  xpp_math.h:10–22 records the resulting differences. P11 consolidates
  ownership with explicit algorithms/policies and tests both results; it
  cannot replace AUTO's routine merely because both routines compute roots.

## Ordered follow-up card proposals

Each proposal must retain W299's exact diagrams and W300's reference checks,
unit tests, relevant autocheck sections and examples. No baseline/allowlist
increase is an acceptance criterion. Order is priority plus dependencies;
independent later cards may be staged separately by the reviewer.

### P1: W327 ([#388](https://github.com/MuhammadMoustafa/xppautX/issues/388)) — Make XPPAUT AUTO import bounded and transactional

Places: auto_nox.cpp load_auto_numerics/load_auto_graph/import_auto_file/
make_q_file, diagram.cpp load_diagram, xpp_session_auto* readers/validators.
Done: parse the entire foreign file into owned temporary settings, points and
solution data; bound count/products before allocation; validate all indices,
finite values, names and mesh settings against the loaded model; apply once.
Malformed/truncated/overlarge files leave settings, diagram and solutions
unchanged and give file, line and offending source/value. Include nuzr above
nine, invalid axes/point indices, a truncated second point and a malformed
solution tail, through window/server/silent import dispatch where available.
Order: first, before resizing mark scratch or sharing restart readers.

### P2: W328 ([#389](https://github.com/MuhammadMoustafa/xppautX/issues/389)) — Preserve AUTO Error values and centralize stream positions

Places: AutoFailed/auto_fail in auto_state.h; 19 call sites; do_auto catch;
RunUnits .what conversion; autlib1 newlab/findlb stream calls; xpp_io owner.
Done: file failures retain their original Place; numerical/command failures
are attached once at dispatch; one renderer/event in all modes and recorded
steps; stream rewind/tell/seek failures are checked by the I/O owner. Preserve
fort output and Windows absolute text-stream positioning. Lower error baseline
only if a counted site actually disappears; do not add ALLOWED entries.
Order: after P1 or independently before its new error paths land.

### P3: W329 ([#390](https://github.com/MuhammadMoustafa/xppautX/issues/390)) — Give AUTO problem, point and stability codes typed owners

Places: auto_c.h iap, XAUTO/BIFUR, gogoauto dispatch, auto_nox start/switch,
autlib1 init/point logic, diagram.h and auto_stability.h APIs.
Done: extend existing owners; IPS, decoded ITP kind/branch form and stability
classification have typed APIs, with explicit integer encode/decode at files
and protocol boundaries. Reject unknown file codes with their place. All
problem/function-table selections and labels remain bit-identical; no aliases
or second enum for a concept already owned. Test every supported problem and
special point, including signed/composite ITP and maps.
Order: third; supplies point kinds for P4.

### P4: W330 ([#391](https://github.com/MuhammadMoustafa/xppautX/issues/391)) — Store AUTO axes by kind, separate from numerical slots

Places: AutoState mapping methods, auto_settings_apply's icp2–4 shift,
DIAGRAM/GRABPT, autevd addbif/init_auto, autpp callbacks, settings names,
diagram export and snapx serialization.
Done: a stored axis is a tagged value (model parameter identity, period,
truncation interval or another named numerical quantity), not an ambiguous
integer in three spaces. The run's numerical slot layout is a separate owner;
CSV column order is a separate serialization layout. Growing/reordering the
selected model parameters never rewrites historical point axes. Define whether
model identity is its Model index or stable name in the persisted format;
resolve names on load with normal model validation. Use a deliberate current
file schema change with no old-file fallback. Test P10 distinct from period T,
reordering/expanding parameters before and after periodic/homoclinic points,
multiple views, grab/restart and snapshot roundtrips. Keep W299 CSV baseline
through explicit export order if that is the selected current schema.
Order: fourth, after P3; **proposed only, no mapping change in W301**.

### P5: W331 ([#392](https://github.com/MuhammadMoustafa/xppautX/issues/392)) — Size AUTO marks and name remaining schemas/defaults

Places: MAX_AUT_PER/AUTO_SETTINGS_MARKS, BIFUR/AutoState/XAUTO mark arrays,
gogoauto iuz/vuz, auto_per_par and settings forms; seven-value restart scratch,
NumField bounds, AutoOptions, auto_c numerical thresholds and output schemas.
Done: model-independent mark lists are Session-owned vectors, limited by memory
and a named untrusted-file budget; the form and protocol use one settings
validator and agree on cardinality. Sizes that represent a method or schema
have one owner and one-line reasons; defaults sent to the page come from it.
Keep collocation formulas/order unchanged; any genuine method-limit extension
is a separate numerical decision. Test more than nine marks, atomic rejection,
all form/set paths and file roundtrips. No shrinking array while nuzr is unchecked.
Order: fifth, after P1/P4; can split schemas from lifting marks at review.

### P6: W332 ([#393](https://github.com/MuhammadMoustafa/xppautX/issues/393)) — Remove dormant AUTO MPI topology and transport plumbing

Places: KREDO, iap mynode/numnodes, gogoauto initialization, autlib2 reduce/
bcksub/redrhs and mynode/numnodes/csend/crecv/gdsum stubs.
Done: remove unreachable MPI-only communication and fixed topology arrays;
keep the serial reduction/factorization arithmetic and actual collocation pool
unchanged. Prove numnodes=1 at the current entry point; tests compare matrices,
diagrams and one/automatic worker outputs. Remove now-stale dup/dead allowlist
entries rather than relaxing them. Do not interpret MPI node count as threads.
Order: sixth, before P8 models the reduced work-array API.
Done in W332 ([#393](https://github.com/MuhammadMoustafa/xppautX/issues/393)): removed dormant MPI topology, KREDO, node fields, stubs, and transport plumbing.

### P7: W333 ([#394](https://github.com/MuhammadMoustafa/xppautX/issues/394)) — Merge AUTO wrapper and restart copies into their owners

Places: all six function and ten block groups classified above; auto_jacobian,
shared solution header/restart readers and numerical/output context owners.
Done: common finite-difference orchestration, UZR evaluation, restart parsing
and identical mesh/context setup each have one implementation. Callback-specific
problem equations, parameter masks, signs, dimensions and floating operation
order remain explicit. Regression tests include every formulation/ref restart
family; dupcheck loses actual duplicate groups and corresponding exemptions.
Numerical matrix/reduction differences are retained with specific rationale,
not a blanket translated-Fortran exception.
Order: seventh, after P1/P2; before P8 wraps the consolidated interfaces.

### P8: W334 ([#395](https://github.com/MuhammadMoustafa/xppautX/issues/395)) — Express AUTO points and work arrays as owned data and views

Places: DIAGRAM/DiagramPoint pointer mirrors, AutoGlobalScratch/store,
AutoStability re/im, solvbv parallel vectors, auto_c numerical signatures,
setubv_parallel_arglist, XAUTO parallel run lists and ARRAY2D/3D access.
Done: owners express dimensions once and use bounded spans/matrix views from
the numerical owner; borrowed views cannot outlive/misbind storage after move,
resize or snapshot restore. Const input versus mutable output is explicit.
Preserve column-major traversal, ABI boundaries that are actually needed and
algorithm order. Test repeated resize/move/restore, dimensions and sanitizer
runs; remove obsolete pointer mirrors rather than adding aliases.
Order: eighth, after P4/P6/P7 reduce the number of interfaces to migrate.

### P9: W335 ([#396](https://github.com/MuhammadMoustafa/xppautX/issues/396)) — Put AUTO protocol subscriptions and caches in their owner

Places: auto_data's three, auto_settings' one, json_auto's seven baseline
symbols and the existing JSON client's/session-switch lifecycle.
Done: client-owned subscriptions/pacing/sent caches live in its object;
session-derived data lives in Session. Session switching and reload preserve
subscriptions but never replay stale diagram state. Test two sessions, views,
subscribe/unsubscribe/reconnect and grabbed-point keys. globalcheck --update
records a strictly lower count; review reasons and no new globals elsewhere.
Order: ninth, independent of numerical migration once lifecycle is specified.

### P10: W336 ([#397](https://github.com/MuhammadMoustafa/xppautX/issues/397)) — Make AUTO/EISPACK scratch reentrant and seal worker inputs

Places: eispack.cpp static locals/by-reference constants; auto_parallel
prepare/publish/intervals, autpp pure/serial branches, setubv and conpar dispatch,
homoclinic eigenvalue calls.
Done: scratch is invocation- or AutoLib-owned; immutable inputs are explicit;
workers cannot mutate Session parser/event/file data. Preserve caller-ordered
shared updates. Test every parallel formulation, 1/2/automatic threads,
reusing a pool after resizing, cancellation and thrown callback failures;
TSan and concurrent eigenvalue-unit calls show no owned-code races. Reduce
EISPACK's mutable baseline with --update; no suppression or widened allowlist.
Order: tenth; perform before adding eigenvalue work to collocation workers or
sharing the EISPACK owner across concurrent calls.

### P11: W337 ([#398](https://github.com/MuhammadMoustafa/xppautX/issues/398)) — Consolidate AUTO numerical ownership with exact algorithms

Places: autlib1 ge/nlvc/eig, autlib4 flowkm/SVD, autlib5 eigho/projections,
eispack, xpp_math's dense/band solves and orthes/hqr, gear callers.
Done: xpp_math owns the numerical operations with explicit complete-pivot,
partial-pivot and eigenvector/convergence semantics. AUTO's variant keeps its
pivot/reduction/QR order, determinant and diagnostics; gear's behavior is also
unchanged. Test pivot-sensitive matrices, singular cases, complex/conjugate
roots, vectors and Floquet multipliers; all AUTO/exemplar baselines unchanged.
If sharing a kernel would change bits, retain the justified algorithm shape
under the owner and record precisely why. Never replace a variant on the
strength of matching mathematical names alone.
Order: last, after P7/P8/P10 make interfaces and scratch safe to share.

## Validation and limits of this audit

The storage fix and full audit are uncommitted as requested. Native gates run
sequentially through heavy.sh, no background jobs and no process-name kills.
W299's diagrams, W300's .auto07p/.xppaut references, allowlists and baseline
counts were not updated. No counted global/error/duplicate was removed by the
one storage fix, so there is no --update baseline change to report.

Native results (all sequential heavy.sh gates):

| Gate | Result |
|---|---|
| UCRT gcc 16, WERROR=1 build and unit tests | 0 compiler warnings; 43 programs, 37,103 checks, 0 failed; auto_limits 26 checks (8 new form checks) |
| autocompare, no --update | 37 models, 9 explicitly skipped, 4,008 points, 82 checks, 0 failures; W300 AUTO-07p/XPPAUT references included; no differing model |
| autocheck input,limits,control,stability,errors,threads | 37 checks, 0 failures across six selected sections |
| examples_check | 184 models: 183 outputs match the unchanged baseline; ml-range has no output by design |
| dupcheck --check | 14 full-core function groups, 19 block groups; 0 unallowed/stale entries |
| errorcheck --list | 0 unplaced sites; no AUTO exemptions |

The first form-test attempts exposed test-fixture assumptions (headless question
policy and normalized uppercase variable names); those were corrected, then the
complete gate sequence above passed. No production fallback was added.

Linux/source/TSan validation
remains the reviewer/follow-up tier. This audit identifies code paths and
ownership debt; it does not claim dynamic proof for every foreign-input or
thread scenario listed as a follow-up.
