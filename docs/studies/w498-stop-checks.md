# W498: remaining stops and checks (#562)

The integration owner reports a map with no crossing using the existing
`named_settings` renderer and the actual time interval. Crossing detection
is shared by fixed-step and dense-output methods. Monte Carlo counts
attempted and converged guesses and reports distinct stored equilibria and
Newton settings once. `do_sing_info` returns failed eigenvectors through
`FirstError`; Monte Carlo keeps an equilibrium whose manifold failed and
returns the first error to its command, rather than logging and losing it.

BVP AUTO keeps `ge`'s returned smallest pivot in `SolvbvWork`, not global
state, and gives it to the existing no-convergence renderer. A zero-pivot
error names the pivot and threshold too. No numerical operation changed.
The GE numerical fingerprint remains unchanged: the test checks the added
diagnostic suffix separately before hashing the original prefix and every
numerical output. A short saved solution now names its label as well as
its file and line; existing count/allocation limits and all-or-nothing load
remain unchanged. No path policy, trust-boundary allocation or process
launch was added.

## Coverage

`servercheck --only stops` passes 209 checks, including:

- No crossing, a section stop and the event-count note, without pixel or
  elapsed-time assertions; stderr is joined before checking its log note.
- Monte Carlo's non-converging guesses and a failed manifold (one placed
  error, with all four converged guesses and their one equilibrium kept).
- F1's maxiter and F2's singular fit, with a real data file and the asks.
- A2's missing restart label, A3's short solution rejected before load,
  A7's periodic tiny-pivot no convergence and A8's inaccurate multipliers
  on a deliberately coarse periodic mesh. Existing restart unit tests also
  exercise direct short reads, which the session reader prevents reaching.

**Unfinished A9 server case:** `inho` in core/autlib5.cpp unconditionally
sets ISTART to 2. Nothing in the protocol, files or UI can request ISTART 3,
so IEQUIB 2 with ISTART 3 cannot be triggered by servercheck. Adding a
production option or test-only protocol hook solely to reach it was
rejected under YAGNI. The existing guard and its error remain in place.
The maintainer can decide whether to expose homotopy starts in a new card.

Searched `integrate`, `do_sing_info`, all AUTO linear solves (`ge`,
`dimrge`, `stepbv`), `auto_stop`, session solution validation, `named_settings`,
Error/FirstError and the existing fit/ask helpers before changing logic.
Reused these owners and renderers. A simpler generic singular message was
rejected because it would omit the pivot and threshold; a process-wide
last-pivot variable would break session isolation and thread safety.

The pre-existing W495 validation conflicts remain documented in
[w495-start-checks.md](w495-start-checks.md); no baseline is rewritten.

## Final gates

- `make -j6 xppautx test`: zero compiler warnings; 59 unit suites,
  91,271 assertions, zero failures.
- Stops: 209 checks, zero failures. Full servercheck: 1,510 checks,
  the same five W495 invalid-start expectation failures, no new failure.
- Examples: 216 models, 215 matching outputs; ml-range has none by design.
  Odexcheck: all eight models match.
- Autocompare: 61 models, nine skipped, 141 checks, the same 24 W495
  refused-start failures; no model baseline or computed numbers changed.
- Web build/check/typecheck pass; all 362 web tests pass.
- Deadcheck, dupcheck, aliascheck, errorcheck and findingscheck pass.
  Deadcode lists exactly the 13 known Ubuntu window/http functions, none
  in this diff. Globalcheck fails in unchanged `json_io.cpp`: six internal
  symbols against baseline five (including W493's `checking_arguments`),
  with both normal and dead-code objects. Its unrelated baseline is kept.

For hostile solution input, the existing count and allocation bounds and
transactional reader were checked; the short-file server case proves the
model is not replaced. A valid file can still request work up to those
existing bounds. No new input weakness was identified.
