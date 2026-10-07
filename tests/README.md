# Unit tests

```bash
make test
```

Each `test_*.cpp` (C++, like the core: W109f) is a program that links `libxppcore.a`, checks pure core code
and prints one summary line; `tools/verify.sh` runs them before the
end-to-end checks. The framework is `xpptest.h`: `CHECK`, `CHECK_STR` and
`TEST_REPORT`, about thirty lines, no dependency.

To add one, drop a `test_<thing>.cpp` in here — the Makefile picks it up from a
wildcard, so there is no list to keep in step.

## What belongs here

Pure functions whose breakage an end-to-end run would report as a puzzling
difference somewhere else rather than as a failure of the thing that broke:
the parser, name and format helpers, file round trips. Whole-session
behaviour is already covered, and better covered, by `tools/servercheck.py`
(the protocol), `tools/webcheck.py` (HTTP), `tools/web2check.mjs` (the web front end)
and `tools/examples_check.sh` (every example's output, compared with
`tests/examples.md5`, or with `tests/examples.<platform>.md5` on a platform
whose outputs differ from Linux's: AGENTS.md says how CI makes those).

## What the tests pin

`test_bessel.cpp` compares J/Y orders 0, 1, 2 and 5 with John Burkardt's
published TEST_VALUES tables (the source link is in the test). It allows
16 ULP, or 64 ULP at selected near-zero inputs; checks repeat-call bits,
order truncation, parity, tiny arguments and IEEE special values; and prints
a result-bit fingerprint for comparisons between builds (W163, #215).

Several checks pin upstream behaviour that looks like a bug and is not, so
that nobody "fixes" it into a silent change of what models compute:

- `^` groups to the left. `2^3^2` is 64, not 512.
- Unary minus binds more weakly than `^`, so `-2^2` is -4.
- AUTO's column headings are exactly 14 characters. Replacing `PAR(n)` with a
  longer name has to keep that width or the table's rows stop lining up with
  their headings.

A test that fails after a deliberate change to upstream behaviour should be
updated together with a note saying why the behaviour changed, the same way
`docs/front-end-gaps.md` records front end differences.

## Noise

The parser tests feed in bad expressions on purpose, and XPP prints its
complaint about each one. Lines like `Premature end of expression` in the
output are the tests working.
