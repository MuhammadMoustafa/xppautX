# Code review and test report — 2026-10-01

Status: complete. Six prioritized findings, plus two minor validation gaps.

Branch: `codex/review-2026-10-01`. Reviewed application commit: `6eef7f710`.
This is a review of the current development snapshot, not only its last commit.
No application code has been changed, and no branches have been merged, reset or deleted.

## Confirmed issues

1. **P1 — Loading an output stride of zero crashes the next integration.**
   `core/lunch-new.cpp:201` reads `nout` without its semantic range check and
   `core/lunch-new.cpp:494` applies it. `core/integrate.cpp:1446` divides by it.
   A saved session with `0 nout` loads without an error; Initialconds/Go then
   exits with signal SIGFPE (subprocess return code -8). `0 DeltaT` is also
   accepted. Use the numerical settings owner's validation before applying
   set-file settings, and test that invalid values leave the old session intact.

2. **P2 — A saved added browser column disappears from the table on reopen.**
   `core/xpp_session.cpp:623` restores data before assigning added column
   definitions at line 636. `put_stored_data` refreshes the browser before those
   definitions exist; there is no refresh after assignment. Reproduction:
   integrate lecar, add `VW=v*w`, save with data, reopen, request browser rows.
   Before saving: 601 rows and eight columns, including VW. After reopening:
   601 rows and seven columns, without VW; no error. Restore and compute added
   columns before updating or drawing the restored table.

3. **P1 — Session loading bypasses added-column capacity checks.**
   `core/xpp_session.cpp:217` checks only that the saved count is nonnegative.
   A session containing 5,001 added columns is accepted, even though ordinary
   Add column checks the index against MAXODE (`core/browse_data.cpp:371`).
   Later refreshes call `DataStore::add_column` with unchecked indices into
   fixed-capacity storage (`core/storage.cpp:56`). Acceptance is reproduced;
   downstream out-of-bounds access is a source-level finding, not a reproduced
   sanitizer result. Validate the count against remaining model capacity
   before replacing the session.

4. **P2 — Added column formulas are not validated while loading a session.**
   `core/xpp_session.cpp:223` stores the formula without compiling it.
   A formula replaced with `unknown_symbol+` loads without an error and replaces
   the old session. Formula evaluation is deferred until a later browser
   refresh, whose caller ignores the computation's failure. Validate formulas
   in temporary parser state before accepting the file.

5. **P3 — Whole-file validation excludes the set file's documentary trailer.**
   `core/lunch-new.cpp:360` reads the `RHS etc ...` marker and returns without
   checking the remaining lines. Appending `not_a_valid_model_line` to a saved
   session's model.set is accepted without an error. Parse or explicitly
   validate the documented trailer so a damaged file cannot pass the whole-file
   check. The equations in that trailer are deliberately documentary and do
   not drive the restored model; this is a strict-validation gap, not evidence
   that the garbage changes a numerical result. Add rejection tests with exact
   file and line locations, or explicitly document this format exception.

6. **P2 — Random generator restoration accepts trailing garbage.**
   `core/xpp_math.cpp:173` extracts the generator and spare deviate but never
   checks for end of input. Appending `not_generator_state` to random.txt
   is accepted on load. Require complete consumption and validate the spare
   flag against the serialized format before changing the generator.

Other reproduced validation gaps: marks.set accepts type/color 999, and the
manifest accepts a repeated name line. These should be included in the file
validation audit; their downstream impact has not been established here.

## Test results

| Check | Result |
|---|---|
| WSL core build, WERROR=1 | PASS — zero warnings |
| WSL core unit tests | PASS — 27 programs, 19,946 checks |
| Native Windows UCRT build, WERROR=1 | PASS — zero warnings |
| Native Windows unit tests | PASS — 27 programs, 19,938 checks |
| Frontend committed bundle consistency | PASS |
| Frontend TypeScript | PASS |
| Frontend unit tests | PASS — 292 tests |
| Textual error/alias/dead/duplicate/key checks | PASS against current baselines |
| Full reviewer gate, including all source checks | PASS |
| Linux headless lecar smoke test | PASS — 601 rows, MD5 c281851de59ffd03b2a46428619a0c8f |
| Server protocol | PASS — 567 checks |
| HTTP frontend | PASS — 54 checks |
| Command-line modes | PASS — 15 checks |
| File associations | PASS |
| AUTO | PASS — 181 checks |
| Golden export files | PASS — 6 files |
| Example numerical baselines | PASS — 184 models; 183 matching outputs, one intentionally writes none |
| ODEX conversion parity | PASS — 177 models |
| Manual examples and options | PASS — 24 model blocks, 29 options |
| Browser desktop and files sections | PASS — 216 checks, zero failures, zero flaky results |
| Targeted ASan/UBSan build, smoke and unit tests | PASS — no sanitizer or leak reports |
| New session review probes | 12 observations — one rejection control passes, 11 regression observations fail |

The malformed-number control is rejected, while the semantic-invalid-value
probes above are accepted. Existing passing gates therefore do not establish
complete file validation.

The regression observations are grouped into the six issues above and the two
minor validation gaps; they are not eleven independent product bugs. They
intentionally return exit status 1 on this snapshot. The platform-specific
unit-test count difference is recorded without treating it as a failure.

## Reproduction and evidence

The reusable probe is `tools/review_session_probes.py`. From a built repository:

```bash
python3 tools/review_session_probes.py ./xppautX
# Native Windows: python tools/review_session_probes.py ./xppautX.exe
```

On this review machine it was executed from the WSL cache against commit
6eef7f710, with the script read from the Windows checkout. It uses the existing
`tools/xppclient.py` protocol driver, temporary saved sessions and review-owned
subprocesses. Every temporary server and directory is closed in `finally`.
The zero-stride probe deliberately exercises the observed process crash.

Representative observations:

```text
added_column_round_trip: before 601 rows / 8 columns; after 601 rows / 7 columns
malformed_number_control: rejected with a located error
zero_output_stride: accepted, no error
zero_output_stride_go: no idle event, process exit -8 (SIGFPE)
excessive_added_columns: accepted 5,001 added columns, no error
bad_added_formula: accepted unknown_symbol+, no error
set_trailing_garbage: accepted, no error
rng_trailing_garbage: accepted, no error
```

Logs in the Windows checkout: `build/review-core.log`,
`build/review/native.log`, `build/review/verify.log`,
`build/review/web-dist.log`, `build/review/web-types.log`,
`build/review/web-tests.log`, `build/review/browser.log`,
`build/review/session-probes.jsonl` and `build/review/asan.log`.
These logs are local and git-ignored. The first sandboxed browser attempt
produced no results and was discarded; the approved rerun produced the 216
passing checks reported above. The initial native invocation inherited the
wrong make shell; rerunning with `SHELL=/usr/bin/sh` completed successfully.

## Markdown coding instructions review

- `CLAUDE.md` gives clear ownership rules, process isolation, numerical
  baselines and data-level testing guidance. Its all-or-nothing file rule is
  stronger than the implementation and current tests. `docs/roadmap.md` marks
  W125 done; the reproduced gaps show that its acceptance criteria need to be
  reopened or extended.
- Error-location checking allows 339 sites without a location. This is an
  explicit migration baseline, not a failed checker: W140b is in progress.
  Report the outstanding count whenever claiming compliance with the rule.
- The build heading says Windows builds only under WSL, while later sections
  document native UCRT and clang builds. Rewrite the heading to distinguish
  Linux checks under WSL from native Windows builds.
- `README.md` still describes core C sources and `.c` ownership paths, and says
  warning cleanup is pending. The source gate requires C++, and this snapshot
  builds with no warnings. Update these architectural and status references.
- Clarify reviewer versus task-agent gate requirements: the same guide says
  full checks follow every change and later forbids task agents from running
  full verification. The distinction exists in prose but should be explicit
  at the command examples.
- `tests/README.md` describes core tests and integrations clearly, but session
  round-trip coverage should include added columns and semantically invalid
  values, not only malformed tokens and successful ordinary saves.
- `tools/verify.sh` conditionally skips Python-driven behavior checks if
  Python is unavailable; it does not fail for missing required coverage.
  Require dependencies up front or print an explicit incomplete result.
  Python was available for this review, so none of those checks were skipped.

## Scope and limitations

Manual review concentrated on saved sessions, set files, random state, browser
storage, numerical settings, the frontend transport, build gates and coding
instructions. Automated coverage is broader than the manual review. This is
not an exhaustive mathematical audit of every solver or a macOS validation.
Only the desktop and files browser sections were run; the full browser suite,
ThreadSanitizer and Valgrind were not run. The build commands were incremental
builds with the repository's compiler/flag stamps, not forced clean rebuilds.
No upstream XPPAUT defect is claimed without an upstream reproduction.

No application fixes were made or numerical baselines rewritten. Prioritize
the crashing numerical input and the unchecked column capacity, then restore
added-column round trips and complete semantic file validation. Keep existing
numerical checks while adding rejection-and-rollback tests for these cases.
