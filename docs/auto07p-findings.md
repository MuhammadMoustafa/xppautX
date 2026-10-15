# AUTO-07p findings

Bugs, wrong results, crashes, reads past an array and arbitrary limits found
in AUTO-07p itself while comparing it against xppautX, for the paper
(maintainer, 2026-10-14; [W367](https://github.com/MuhammadMoustafa/xppautX/issues/429)).
Each entry: what AUTO-07p does, the evidence (the model, the run, the
numbers), the lines in AUTO-07p's own source, what xppautX does instead, and the
card and commit. Add an entry whenever a card finds one; keep the evidence
reproducible.

How this file differs from related documents:
- Bugs found in XPPAUT's AUTO (AUTO2000, as XPPAUT ships it) stay in
  [xppaut-findings.md](xppaut-findings.md).
- Where xppautX deliberately differs from AUTO-07p (a method or a tolerance
  chosen differently, not a defect) stays in
  [auto-references.md](auto-references.md).

Each finding cites the lines in AUTO-07p's own source, never our refactored
files, so it is clear the behaviour is AUTO-07p's and not ours. A citation is a
relative link into the local copy kept out of git (`reference/` is in
`.gitignore`): `reference/auto-07p/` (AUTO-07p 0.9.4, git `44cc1c4`, 2025-06-26).
A citation reads `[file:line](../reference/auto-07p/src/file#Lline)`.

The entries are sections, not table rows: most need a paragraph of evidence.
The index gives one line each; its number links to the entry's section, and
every card (to its GitHub issue) and commit (to its GitHub commit) is a link,
in the index and the entries alike.

When the file was made (W367, 2026-10-14), the AUTO studies
([W355](studies/w355-auto07p-differences.md),
[W356](studies/w356-auto-location-07p.md),
[W358](studies/w358-auto-profile.md)) and
[auto-references.md](auto-references.md) noted no defect of AUTO-07p's own,
so the index starts empty.

| # | Area | Finding | Card |
|---|---|---|---|
