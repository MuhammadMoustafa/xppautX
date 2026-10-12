# Model instances: choosing an imported model's groups, and coupling instances

Follows the W309 design ([design.md](design.md), reviewed in [debate.md](debate.md)), on
2026-10-11. W309 settled typed named groups in `.odex` (`par NAME {}`, `init NAME {}`,
`options NAME {}`, one `default` per kind, `preset` bundles). The maintainer then asked how
an importer chooses which of an imported model's groups it uses, and how imported cells are
coupled. This file is the proposal for a second review.

## What exists today (evidence)

- `import "cell.odex" as c1` (W282, docs/odex.md "Imports") takes every statement of
  `cell.odex` and prefixes every name it declares: `c1.v`, `c1.gca`, its sets. The same file
  imported twice under two prefixes gives two copies, which the spec presents as "two coupled
  cells". A name the file reads but does not declare stays unprefixed. An imported file's
  `options` are not imported.
- **Coupling does not work** (tested 2026-10-11 with `--check`):
  - the cell declares `par i_in = 0` and the importer writes `c1.i_in = g*(c2.v - c1.v)`:
    refused, "`c1.i_in` is already declared at 1:1";
  - the cell reads an `i_in` it does not declare: the name is not prefixed, so both instances
    read the same `i_in`; per-instance coupling is impossible.
- Arrays use a range suffix on a statement: `x[j]' = -x[j] + x[j-1]  for j in 2..n`.
  An import line takes no range today.
- W313 (decided): a state with no initial value starts at 0 with a warning naming it.

## Proposal

### 1. Choosing an instance's groups

- **At the import:** `import "cell.odex" as c1 using bursting, rest` makes cell's `bursting`
  and `rest` groups the instance's defaults (at most one group per kind; each must exist in the
  file). Without `using`, the instance uses the file's own `default` groups.
- **From the importer:** a non-default group may assign an instance's parameters:
  `par detuned { c1.iapp: 0.20, c2.iapp: 0.25 }`. It assigns only names the instance declares.
- **At run time:** each section's menu lists every instance's groups (`c1.bursting`,
  `c2.spiking`); choosing one resets only that instance's parameters to its default and applies
  the group (scoping agreed in debate.md).

### 2. Inputs: how an instance is coupled

```
# cell.odex
input i_syn                          # a slot the importer may fill
v' = (-gca*minf(v)*(v - vca) - gl*(v - vl) + iapp + i_syn)/c

# network.odex
import "cell.odex" as c1
import "cell.odex" as c2
par net default { g: 0.1 }
c1.i_syn = g*(c2.v - c1.v)
c2.i_syn = g*(c1.v - c2.v)
```

- `input NAME` declares a slot in a model file; the importer fills it for each instance with
  `PREFIX.NAME = EXPR`. Filling a name that is not an input is an error (the existing "already
  declared" rule stays for everything else).
- **Unfilled:** `input i_syn` left unfilled is 0 with a warning naming the instance and input
  (the W313 rule); `input i_syn = 0` (an explicit default) is used silently. Run on its own,
  `cell.odex` uses its inputs' defaults (0, with the warning when none is written).
- **Filled twice with `=`:** an error naming both lines (replace or add is ambiguous).
- **`+=` accumulates:** `c2.i_syn += g1*(c1.v - c2.v)` and `c2.i_syn += g3*(c3.v - c2.v)`
  sum, starting from 0, in any order. Mixing `=` and `+=` on one input is an error.
  Open question from the maintainer: only `+=`, or the other compound operators too? Claude's
  view: only `+=`. Summing currents is the real structure of coupling and a sum does not depend
  on line order; `-=` is `+=` of a negative; `*=` and `/=` have no coupling use and would invite
  order questions with mixed forms.

### 3. Many instances with a loop

The existing range suffix, allowed on an import line:

```
const n = 10
import "cell.odex" as c[j] using bursting  for j in 1..n
par spread { c[j].iapp: 0.1 + 0.01*j  for j in 1..n }
c[j].i_syn += g*(c[j-1].v - c[j].v)  for j in 2..n
c[j].i_syn += g*(c[j+1].v - c[j].v)  for j in 1..n-1
```

Instances are `c[1]` ... `c[n]`; `c[j].v` reads an instance's variable. The maintainer prefers
this explicit loop to any implicit "array import".

## Points Claude is not sure of

1. Does `input` belong in the cell file (the cell declares what may be coupled) or should the
   importer be able to add a term to any equation of an instance (no change to the cell)?
2. Is an unfilled input with a warning right, or should an input with no written default have
   to be filled?
3. Is `+=` enough, and does it compose with a `for` range and with events or other statements
   that might also want to accumulate?
4. Can an input be a vector (`input i_syn[k] for k in 1..m`) or carry a function (a synapse
   whose form the cell decides, the importer giving only the presynaptic voltage)?
5. Do instance names with indices (`c[j].v`) fit the name grammar ("letters, digits and `_`
   parts joined by dots"), the protocol, the plot's variable lists and the data columns?
6. How many instances before this needs limits (W296's bounds: a 10,000-cell network)?
7. `using` at the import vs. choosing at run time: does `using` duplicate what a `preset`
   in the importing file could do?
