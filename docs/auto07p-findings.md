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
- Deliberate differences in method or choices (such as tolerance formulas,
  alternative algorithms, or unverified codim-2 detection differences) stay in
  [auto-references.md](auto-references.md).
- Real defects of AUTO-07p's own belong here.

Each finding cites the lines in AUTO-07p's own source, never our refactored
files, so it is clear the behaviour is AUTO-07p's and not ours. A citation is a
relative link into the local copy kept out of git (`reference/` is in
`.gitignore`): `reference/auto-07p/` (AUTO-07p 0.9.4, git `44cc1c4`, 2025-06-26).
A citation reads `[file:line](../reference/auto-07p/src/file#Lline)`.

The entries are sections, not table rows: most need a paragraph of evidence.
The index gives one line each; its number links to the entry's section, and
every card (to its GitHub issue) and commit (to its GitHub commit) is a link,
in the index and the entries alike.

As of card W367 ([#429](https://github.com/MuhammadMoustafa/xppautX/issues/429)),
no defects of AUTO-07p's own have been identified in the comparison studies
([w355](studies/w355-auto07p-differences.md),
[w356](studies/w356-auto-location-07p.md),
[w358](studies/w358-auto-profile.md), and
[auto-references.md](auto-references.md)): the differences documented there
are improvements, optimizations, alternative methods, or traced reference
comparisons, with no crashes, memory faults, or wrong results in AUTO-07p itself.
The index below is therefore empty.

| # | Area | Finding | Card |
|---|---|---|---|
