# W314 debate: components, after the maintainer's reaction to the motoneuron sketch

On 2026-10-12 the maintainer read [motoneuron-example.md](motoneuron-example.md) and said:
it looks organized but like a completely new program with a new learning curve; XPPAUT's
users are mathematicians from every field, not only neuroscientists; the use of `=` is not
consistent; functions and expressions now look the same. Claude took the design and those
concerns to Codex Astra (gpt-6-astra, read-only, high effort), asked to push back rather than
agree, and debated three rounds. The full exchange follows the outcome.

## Outcome: what both sides sign

- **No constructor syntax.** `NaF soma_na(g: 0.06)` read like a function call or declaration
  (the maintainer's "functions and expressions look the same"). A copy is made by its own
  statement: `copy Vdp as a`. `import "file.odex"` only makes a file's definitions available;
  `copy` makes equations and state. Not "object-oriented": a system is a named set of
  equations copied with independent state.
- **`system Name { ordinary model statements }`**, in the same file or an imported one. Its
  body is written exactly like a flat model, and defining it runs nothing until it is copied.
  A system sees its own file's consts and functions, never the copier's names.
- **Inputs** (`input force = 0`) are filled by one binding written like an equation,
  `a.force = k*(b.x - a.x)`, evaluated at every step and AUTO evaluation. An input without a
  written default must be bound; instantaneous algebraic cycles are refused. `+=` is deferred:
  a sum is written out in the one binding.
- **One rule for `=` and `:`.** `=` separates a definition or a binding from its right-hand
  side (`x' =`, `q =`, `fun f(x) =`, `input u =`, `a.u =`); `:` separates a key from its
  value inside braces (groups, options, `with`). Built-in named arguments
  (`volterra(..., mu=0.5)`) keep their `=`.
- **Values have three homes, each with one job.** Run-time values (parameters, initial values)
  live in W309's groups, which may name a copy's values with dotted keys
  (`par coupling default { k: 0.1, b.mu: 1.5 }`, `par blocked { soma_h.g: 0 }`). Fixed
  per-copy values (geometry, kinetics) are the system's `const`s, changed for a copy with
  `copy Compartment as ah with { len: 20e-4 }`: numbers and consts only, fixed at load, never
  in the parameter panel or AUTO's list. Only what an experiment varies is declared `par`.
- **Composed defaults.** A copy's defaults are its system's, overridden by the copier's default
  groups; choosing a group resets its scope to that composed default and then applies the
  group.
- **Restore `par a = 1` and `init x = 1`** (reopening W309) as the shorthand for a default group
  named `base`: several shorthand lines merge, a key twice is an error, and shorthand plus an
  explicit default group of the same kind in one scope is an error. So a flat model is written
  as today, and the rejected failure (an unnamed block silently competing with a named
  default) stays impossible. Saving keeps the shorthand.
- **Ranges** on copies and in group entries: `copy Phase as osc[j] for j in 1..n`,
  `osc[j].omega: 1 + 0.01*j for j in 1..n`.
- **Deferred from the first version:** consts without a value (required), choosing a system's
  preset when copying (`using`), and `+=`.

The learning path: a flat model is unchanged. Composition starts with four things (`system`,
`copy`, `input`, a binding); fixed per-copy values (`with`) and named regimes (groups) come
later.

## Two examples in the agreed form

Astra's round 2 examples with round 3's `copy`. The motoneuron's wiring is in round 2,
with `import Name as` read as `copy Name as`.

```
system Vdp {
  par mu = 1
  init x = 1, y = 0
  input force = 0
  x' = y
  y' = mu*(1 - x^2)*y - x + force
}

copy Vdp as a
copy Vdp as b

par coupling default { k: 0.1, b.mu: 1.5 }
init start default { b.x: -1 }
par strong { k: 0.5 }

a.force = k*(b.x - a.x)
b.force = k*(a.x - b.x)
```

```
const n = 10

system Phase {
  par omega = 1
  init theta = 0
  input drive = 0
  theta' = omega + drive
}

copy Phase as osc[j] for j in 1..n

par ring default { k: 0.1, osc[j].omega: 1 + 0.01*j for j in 1..n }
init spread default { osc[j].theta: 2*pi*(j - 1)/n for j in 1..n }

osc[j].drive = k*(sin(osc[1 + ((j + n - 2) mod n)].theta - osc[j].theta)
                + sin(osc[1 + (j mod n)].theta - osc[j].theta))  for j in 1..n
```

## What the maintainer decides

1. `copy NAME as instance` (and `system NAME { }`) as the words.
2. `with { }` for per-copy constants only, so geometry and kinetics stay out of the parameter
   panel and AUTO.
3. Restoring `par a = 1` / `init x = 1` as the `base` default group, reopening W309's
   "groups only".
4. Composed defaults with dotted keys in groups.

---

## Round 1: Claude's brief

W314-DEBATE: a design review of components in .odex, with the maintainer's concerns

You are reviewing a design for xppautX (a modernized fork of XPPAUT, the ODE/bifurcation
tool). You are read-only: read the files, think, and answer in text. Be a thinker, not a
yes-man: push back where something does not feel right, say where Claude (the coordinator,
who wrote the design) is wrong, and where the maintainer's concern is right or not. Do not
agree to be agreeable. We will go several rounds until we settle; say plainly what you would
decide and why.

## Read first

- docs/odex.md: today's .odex syntax (Statements, Imports, Arrays, Grammar).
- docs/studies/w309-values-design/design.md and debate.md: settled design of named value
  groups (`par control default { gca: 1.1 }`, `init rest default { v: -0.3 }`,
  `options normal default { ... }`, `preset`), which removes the unnamed `par a = 1` form.
- docs/studies/w309-values-design/instances-design.md and instances-debate.md: the earlier
  "import as + using + input + +=" proposal, paused when the maintainer pivoted to components.
- docs/studies/w314-components/motoneuron-example.md: the component design tried on the
  maintainer's own paper model (Moustafa et al. 2023, ModelDB 267710), the original being
  paper/benchmarks/modeldb-267710/models/6cModel/ (main.ode and its .inc files).
- docs/roadmap.md row W314 (#373): component NAME { ... }, instances with arguments, inputs,
  no inheritance or methods at first, a file with no component is one implicit component.

## The maintainer's reaction to the example (2026-10-12), verbatim in substance

1. "Seems organized but this seems like a completely new program. It needs a new learning
   curve."
2. "XPPAUT users are not neuroscientists only, they are mathematicians from all fields."
3. "The use of = sign seems not consistent to me."
4. "Functions and expressions now look the same."

## What Claude thinks (argue with it)

On 1 and 2: the concern is right for the example as written, and partly the example's
fault. A model with no components must read exactly like today's .odex (x' = ..., parameters,
init), so an XPPAUT user who never writes `component` meets nothing new. The example chose a
6-compartment neuron, which put neuroscience vocabulary (channels, compartments, i_ion) where
the mechanism is generic: coupled oscillators, Kuramoto rings, predator-prey patches,
discretised PDEs. But W309 (settled) already removes `par a = 1` in favour of
`par NAME default { a: 1 }` for every model, flat ones included: that is a learning-curve
cost every user pays, which Claude did not weigh against this concern. Should W309's
"groups only" be revisited given this? (The maintainer rejected unnamed defaults plus named
groups as error-prone; say whether you think that still holds.)

On 3: today's .odex uses `=` for a parameter's value (`par a = 1`), a const, a quantity
(`y = expr`, always its formula's value), an initial value (`init x = expr`), a function
(`fun f(x) = expr`), a derivative (`x' = expr`), and `:` for keys in `options { }` and
`set { }`. W309 groups use `:`. The sketch adds `:` in instance arguments
(`Compartment soma(len: 48.8e-4)`), `=` for an input's default (`input i_ion = 0`) and `+=`
lines. Claude's candidate rule: `=` defines a name in the model (a statement); `:` gives a
value to a key inside braces or parentheses (data). Is that a real rule a user can feel, or
a rationalisation? Where does the sketch break it?

On 4: Claude is not sure which the maintainer means; consider both: (a) an instance line
`NaF soma_na(g: 0.06, v: soma.v)` reads like a function call or a function declaration;
(b) a quantity `a = 0.4*vtrap(...)` and a function `fun vtrap(x, y) = ...` look alike, and
`g_d[j] = g_axial(...)` adds brackets that read like arguments. Say which reading you think
is the stronger problem and what syntax would remove it.

## Questions for you

1. Is "components" the right answer to the maintainer's problem (imported cells cannot be
   coupled today: docs/studies/w309-values-design/instances-design.md "What exists today"),
   or is there a smaller change that keeps .odex looking like XPPAUT's language: for example
   the earlier `import "cell.odex" as c1` plus `input` slots, or a "template" that is just a
   file with parameters? Compare what each costs a mathematician who writes one coupled
   system once.
2. Propose a syntax for instances and their wiring that a mathematician who knows XPPAUT
   would read without a manual. Show the same motoneuron cell (or a part of it) and one
   non-neuro example (two coupled van der Pol oscillators, or a ring of n phase oscillators)
   in your syntax.
3. Give a consistent rule for `=` vs `:` (or another separator) across the whole language,
   W309's groups included, and say what in today's .odex and in the sketch it changes.
4. Point out anything else in the sketch that is wrong, unclear or that you would cut.

Answer in at most about 900 words, plain text with code blocks. Number your points so the
reply can answer them.

## Round 1: Astra

1. I would approve reusable equation systems, but not this constructor-style sketch.

The maintainer is right about the learning cost. Claude’s “flat models meet nothing new” claim contradicts W309. Changing the example’s vocabulary will not remove constructors, group selection, scope, inputs and wiring.

However, components are not inherently neuroscience-specific. Their useful meaning is “a named system of equations that can be copied with independent state.” Calling this object-oriented programming adds expectations that the proposed language neither needs nor satisfies.

The smallest sufficient change is the earlier `import ... as ...` plus declared inputs and continuous bindings. Existing imports already create independent copies. A parameterized file is effectively a component whose boundary happens to be a file.

For someone writing one coupled system once, flat equations have the lowest cost; imported templates add file management; explicit components add syntax but allow everything in one shareable file. I would implement one composition mechanism, with file imports first and optional named component blocks using exactly the same semantics.

2. Separate creating a copy, choosing parameter values, and defining continuous connections.

My proposed file form adds a configuration block to the existing import:

```text
import "cell.odex" as c1 with { par: { a: 2 } }
c1.drive = expression
```

If named components are included, spell their instances explicitly:

```text
instance c1 of Cell with { par: { a: 2 } }
```

Neither resembles a function call. These are proposed forms, not implemented syntax.

Here is the soma-current part of the motoneuron, retaining the sketch’s NaF, Kdr and SomaCa equations in separate files:

```text
par rest default { amp: 0 }
init rest default { v: -70 }
const cm = 0.001
area = pi*(48.8e-4)*(48.8e-4)

import "naf.odex" as na with { par: { g: 0.06 } }
import "kdr.odex" as kdr with { par: { g: 0.80048 } }
import "somaca.odex" as ca

na.v = v
kdr.v = v
ca.v = v

input i_axial = 0
i_ion = (v + 70)/225 + na.i + kdr.i + ca.i
v' = (amp*50e-6/area + i_axial/area - i_ion)/cm
```

This is a soma excerpt, not a claimed bit-identical conversion. Each channel declares `input v`; its equations remain ordinary equations.

A complete two-oscillator example:

```text
# vdp.odex
par base default { mu: 1 }
init rest default { x: 1, y: 0 }
input force = 0
x' = y
y' = mu*(1 - x^2)*y - x + force
```

```text
# pair.odex
par coupling default { k: 0.1 }
import "vdp.odex" as a
import "vdp.odex" as b with { init: { x: -1 } }

a.force = k*(b.x - a.x)
b.force = k*(a.x - b.x)
```

The import configuration supplies instance starting values; the wiring supplies formulas evaluated continuously.

3. Claude’s separator rule is partly a rationalisation.

“`=` defines a name” already fails for `init`, which supplies an existing state’s initial value. Conversely, W309’s default parameter keys declare names using `:`. Today’s grammar also has named built-in arguments such as `volterra(e, of=u, mu=0.5)`, contradicting “parentheses use colons.”

I would use this narrower, visible rule:

```text
x' = expression                 # equation
q = expression                  # derived quantity
fun f(x) = expression           # function definition
input u = expression            # default input formula
a.u = expression                # input formula binding
par base default { p: 1 }       # entries in a configuration map
preset trial { par: base }      # entries referencing groups
```

`=` separates a definition or binding from its right-hand side; `:` separates a map key from its entry. Keywords determine when expressions are evaluated. Braces alone do not imply maps: function bodies contain statements.

Keep existing named function arguments with `=`; remove constructor arguments entirely. W309’s colons need no change.

4. Reopen W309’s compulsory naming, not its deterministic selection semantics.

The maintainer’s concern about competing defaults is valid. Ordinary `par a = 1` is not inherently error-prone.

I would allow ordinary parameter/initial lines as shorthand for one named baseline, with a hard error if that scope also declares an explicit default group of the same kind. Named alternatives still resolve against that baseline. This introduces two spellings, but no competing precedence rules. It is a defensible usability tradeoff; claiming zero learning cost while banning the familiar spelling is not.

5. Constructor/function ambiguity is the stronger problem.

`NaF soma_na(...)` resembles a declaration in another programming language; `c1 = Cell(...)` resembles evaluation. Explicit `instance ... of ...` removes both ambiguities.

I would keep `fun f(x) = ...`, `q = f(v)` and indexed quantities. They distinguish a function, its current evaluated result, and an indexed family using familiar mathematical notation. Another quantity keyword would add learning without resolving a demonstrated ambiguity.

6. Cut or settle these before approval.

The bare `hillock` argument contradicts W309’s explicit presets. Matching group names must not silently select both kinds.

Start with one binding per input; defer `+=`. Later contributions need fixed summation order and explicit default handling.

Require unwired inputs unless a default is written. Reject instantaneous algebraic cycles; ordinary mutual state coupling remains valid.

Define whether instance overrides survive subsequent group selections. Keep solver options global.

Finally, the sketch is not merely reorganized source: it promotes many quantities to adjustable parameters, regroups arithmetic, and replaces the hillock Kdr’s `exp` with `expmod`. Those are scientific behavior changes requiring explicit justification, not evidence that component extraction preserves the model.

## Round 2: Claude

Claude's reply, round 2. Numbered against your points.

1. Agreed: one composition mechanism, "a named system of equations copied with independent
   state", and no "object-oriented" framing. But files-only fights what the maintainer asked
   for. The pivot to components came from "this mess with files and formats": your soma
   excerpt needs naf.odex, kdr.odex and somaca.odex as three files for three short channels.
   A mathematician's coupled system is usually one file. So I would ship both at once, with
   one semantics: a block in the same file, imported by name exactly like a file. Something
   like `system Vdp { ... }` then `import Vdp as a` (no quotes: a block of this file;
   quotes: a file). Push back if you think a block in the same file is the step too far.

2. On `with { par: { g: 0.06 } }`: why a second place for values at all? W309's groups already
   hold values, and a group in the importing file could name an instance's parameters with
   dotted keys:

   ```
   import "vdp.odex" as a
   import "vdp.odex" as b
   par coupling default { k: 0.1, a.mu: 1, b.mu: 1.5 }
   init start default { a.x: 1, b.x: -1 }
   par strong { k: 0.5 }
   ```

   Then the import line only makes a copy; every value lives in groups, so a study condition
   (the paper's HCN-blocked case, `par blocked { soma_h.g: 0, dend_h.g: 0 }`) is an ordinary
   overlay group, saved and chosen like any other. `with` duplicates that and raises your own
   question 6 (does an import's value survive a later group selection?), which disappears if
   the import carries no values. What would `with` give that dotted keys in groups do not?
   The one thing I see is choosing an imported file's own named group (its `hillock` kinetics);
   that could be `import "naf.odex" as ah_na using hillock` naming a `preset` of naf.odex,
   which also answers your objection to a bare `hillock` picking two kinds at once.

3. Your `=` / `:` rule I accept: `=` separates a definition or a binding from its right-hand
   side, `:` a map key from its entry. `a.force = k*(b.x - a.x)` is a binding with `=`, which
   reads like the equations around it; good.

4. Restoring `par a = 1` / `init x = 1` as the shorthand for the default group, an error if the
   same scope also has an explicit default group of that kind: I agree and will put it to the
   maintainer, since it answers their learning-curve concern directly and keeps the rejected
   failure (an unnamed block silently competing with a named default) impossible. Do you see
   any case where the shorthand and a named non-default group interact badly (a non-default
   group naming a parameter the shorthand declared is fine; anything else)?

5. Agreed: defer `+=`; one binding per input, a sum written out in it. Agreed: unwired inputs
   are errors unless they have a written default; instantaneous algebraic cycles rejected.

6. Your strongest catch is not the arithmetic but this: the original has one parameter
   (ampPar) and ~250 fixed quantities; the sketch turned every channel constant into a
   parameter, so the parameter panel (and AUTO's parameter list) would hold 100+ entries
   nobody tunes. Today a fixed quantity `ra = 70` is not in the panel. So a system needs
   per-copy values that are not parameters. Options: (a) `const` in a system, settable per
   copy in the importer's groups... but a group is run-time, a const is load-time; (b) a
   separate per-copy load-time setting; (c) accept that only what is declared `par` is
   tunable, and everything else is edited in the system's text. Which do you pick? This
   decides whether two copies of NaF with different kinetics (soma vs hillock) need
   parameters at all.

   On exp vs expmod: for x < -100, exp(x) < 4e-44, and every use adds it to 1 or to a term
   near e^100, so the doubles are equal; but you are right that a conversion must not argue
   that case by case: a converted model keeps its own text.

7. Arrays of copies: the maintainer prefers explicit loops to an implicit array import.
   `import Vdp as osc[j] for j in 1..n` and `par spread default { osc[j].mu: 1 + 0.1*j for j in
   1..n }`, `osc[j].force = k*(osc[j-1].x - osc[j].x)  for j in 2..n`. Any problem with the
   range on an import line, or with ranges inside group entries?

Please answer, then give your final proposal as one consolidated piece: the syntax and rules,
and three complete examples: two coupled van der Pol oscillators in one file, a ring of n
phase oscillators (Kuramoto with nearest neighbours), and the motoneuron cell's wiring part
(soma, hillock, d0..d3, their channels and links). End with the list of what you and I still
disagree on, and what the maintainer must decide. At most about 1100 words.

## Round 2: Astra

1. I accept shipping local blocks and file copies together. Your objection to files-only is right: forcing three short channels into three files defeats the maintainer’s request. `system Vdp { ... }` and `import Vdp as a` are readable, provided defining a system creates no equations in the running model until copied.

2. I withdraw runtime values from `with`. Dotted entries in groups cover parameter and initial values.

However, your example needs a new rule: `a.mu` already has an imported default. The parent’s dotted default must explicitly override that default, rather than constitute a second declaration. Compose defaults from inner scopes outward; selecting any group resets its scope’s subtree to that composed baseline, then applies its entries. Otherwise selecting `b.some_regime` could unexpectedly restore the library’s `mu`.

`using hillock` may name an explicit preset. It selects startup groups without redefining their baseline. Restrict it to parameters and initials; integration settings remain global.

3. Agreed on separators: `=` introduces definitions and continuous bindings; `:` separates map keys and entries. Keywords establish evaluation timing.

4. Shorthand works with named alternatives. Give its synthesized default a stable name, `base`; reject a conflicting `par base` or `init base`. Multiple shorthand lines merge, duplicate keys fail, and shorthand plus an explicit default of the same kind in the same scope fails. Saving must preserve the shorthand unless the user explicitly converts it.

5. Agreed on one binding, explicit optional defaults, and rejection of instantaneous algebraic cycles.

6. I choose (b): immutable configuration supplied when making a copy. Different geometry or kinetics should not require exposing parameters or editing a shared definition.

Use `with` exclusively for declared constants. Its entries accept numbers, caller constants and range indices; they never read parameters or state. Apply overrides before evaluating dependent constants and expanding the copy. Missing required constants fail. Runtime groups cannot change them.

This also exposes a real tradeoff: a conductance declared `const` cannot participate in `par blocked`. Authors must explicitly declare as `par` the quantities their experiments vary. There is no syntax that simultaneously makes a value immutable and runtime-selectable.

7. Ranges are appropriate in imports and group entries. They are expansion, not runtime loops. Preserve constant bounds and indexed naming; reject overlapping expanded keys and bindings, recursive copies and excessive cumulative expansion. Do not silently change existing empty-range behavior.

8. My consolidated proposal is:

```text
system Name { ordinary model statements }
import Name as copy
import "file.odex" as copy
import Name as copy with { constant: value } using preset
copy.input = expression
```

A definition sees its own lexical constants/functions, never undeclared caller names. Unquoted imports resolve system definitions; quoted imports copy a file’s top-level model, which may itself contain systems. Library imports bring definitions without running them.

Only `par` values enter parameter controls and AUTO. Groups retain W309’s transactional selection and initialization rules, with the composed-default rule above. Bindings evaluate at every solver trial state and AUTO evaluation. All copies, groups and connections validate before loading commits anything.

The following examples use proposed syntax.

9. Two van der Pol oscillators, one file:

```text
system Vdp {
  par mu = 1
  init x = 1, y = 0
  input force = 0
  x' = y
  y' = mu*(1 - x^2)*y - x + force
}

import Vdp as a
import Vdp as b

par coupling default { k: 0.1, b.mu: 1.5 }
init start default { b.x: -1 }
par strong { k: 0.5 }

a.force = k*(b.x - a.x)
b.force = k*(a.x - b.x)
```

The imported defaults supply unspecified initials and parameters.

10. A nearest-neighbour Kuramoto ring:

```text
const n = 10

system Phase {
  par omega = 1
  init theta = 0
  input drive = 0
  theta' = omega + drive
}

import Phase as osc[j] for j in 1..n

par ring default {
  k: 0.1,
  osc[j].omega: 1 + 0.01*j for j in 1..n
}
init spread default {
  osc[j].theta: 2*pi*(j - 1)/n for j in 1..n
}

osc[j].drive =
  k*(sin(osc[1 + ((j + n - 2) mod n)].theta - osc[j].theta)
   + sin(osc[1 + (j mod n)].theta - osc[j].theta)) for j in 1..n
```

11. The complete motoneuron wiring layer follows. `channels.odex` supplies the sketch’s five channel systems, with fixed values declared `const`, required voltage inputs and their existing initial defaults. This illustrates composition; it does not claim conversion preserves numerical results.

```text
import "channels.odex"
const cm = 0.001, ra = 70, cad_max = 0.000168

system Compartment {
  const len, diam, gpas
  const epas = -70
  init v = -70
  input i_ion = 0
  input i_axial = 0
  input i_inj = 0
  area = pi*len*diam
  cross = 0.25*pi*diam^2
  v' = ((i_inj + i_axial)/area - gpas*(v - epas) - i_ion)/cm
}

import Compartment as soma with {
  len: 48.8e-4, diam: 48.8e-4, gpas: 1/225
}
import Compartment as ah with {
  len: 20e-4, diam: 12.94999838e-4, gpas: 1/225
}
import Compartment as d[j] with {
  len: 1450.97e-4, diam: 42.2188e-4, gpas: 1/11000
} for j in 0..1
import Compartment as d2 with {
  len: 754.842*1.81e-4, diam: 36.4297e-4, gpas: 1/11000
}
import Compartment as d3 with {
  len: 1037.91*1.81e-4, diam: 10.1901e-4, gpas: 1/11000
}

import NaF as soma_na with { g: 0.06 }
import Kdr as soma_kdr with { g: 0.80048 }
import SomaCa as soma_ca
import NaF as ah_na with { g: 1.3392, ama: 18, bma: 38 }
import NaP as ah_nap with { g: 3.2971e-5 }
import Kdr as ah_kdr with { g: 0.16552 }
import CaDen as d2_ca with { g: cad_max, gk: 0.41*cad_max }
import CaDen as d3_ca with {
  g: 0.25*cad_max, gk: 0.25*0.41*cad_max
}

par rest default { amp: 0 }
par fire { amp: 0.3 }
init start default { ah_na.m: 0.0131252 }

soma_na.v = soma.v
soma_kdr.v = soma.v
soma_ca.v = soma.v
ah_na.v = ah.v
ah_nap.v = ah.v
ah_kdr.v = ah.v
d2_ca.v = d2.v
d3_ca.v = d3.v

soma.i_inj = amp*50e-6
soma.i_ion = soma_na.i + soma_kdr.i + soma_ca.i
ah.i_ion = ah_na.i + ah_nap.i + ah_kdr.i
d2.i_ion = d2_ca.i
d3.i_ion = d3_ca.i

fun axial(l1, c1, l2, c2) = 1/(ra*(0.5*l1/c1 + 0.5*l2/c2))
g_ah = axial(soma.len, soma.cross, ah.len, ah.cross)
g0 = 1/(ra*0.5*d0.len/d0.cross)
g[j] = axial(d[j-1].len, d[j-1].cross,
             d[j].len, d[j].cross) for j in 1..3

soma.i_axial = g_ah*(ah.v - soma.v) + g0*(d0.v - soma.v)
ah.i_axial = g_ah*(soma.v - ah.v)
d0.i_axial = g0*(soma.v - d0.v) + g1*(d1.v - d0.v)
d[j].i_axial = g[j]*(d[j-1].v - d[j].v)
            + g[j+1]*(d[j+1].v - d[j].v) for j in 1..2
d3.i_axial = g3*(d2.v - d3.v)
```

12. Remaining disagreement: values cannot all live in runtime groups if copies also need immutable configuration. Dotted defaults also require explicit precedence; they do not eliminate that question.

The maintainer must decide whether to restore shorthand, accept constant-only `with`, approve composed scoped defaults, and choose which channel values experiments actually need tunable.

## Round 3: Claude

Claude's reply, round 3 (closing). I accept your points 1-5, 7 and the composed-default rule in
2 (inner defaults composed outward; selecting a group resets its scope's subtree to that
baseline, then applies it). Two doubts left, both from the maintainer's first concern, the
learning curve:

A. `import Vdp as a` for a block of the same file. To a mathematician "import" means "from
   another file". You yourself proposed `instance c1 of Cell` in round 1. One word for both
   (a file copy and a block copy) is fewer words; a word that says "copy" is clearer. Which do
   you hold to, and why? (Candidates: `import Vdp as a`, `copy Vdp as a`, `use Vdp as a`.)

B. Count what a first-time user of composition must learn in your proposal: `system`,
   `import ... as`, `input`, a binding `a.x = ...`, `with { }`, `const len` with no value
   (required), dotted keys in groups, and the composed-default rule. That is eight things.
   Which of them could be cut from the first version without losing the three examples? My
   candidate: ship `with` and valueless consts later, and let the first version's copies differ
   only through `par` values in groups (the motoneuron's geometry then becomes `par` for now,
   which you rightly dislike). Is the parameter-panel cost worse than one more concept? Or is
   there a cheaper way: e.g. the panel and AUTO list only the top-level file's parameters and
   copies' parameters are reachable but folded under their copy name, so making geometry `par`
   costs nothing visible?

Answer both in at most about 400 words, then state in a few lines the final position you
would sign, so the maintainer gets one agreed proposal plus the open decisions.

## Round 3: Astra

A. I would choose `copy Vdp as a` for making an independent equation system, and reserve `import` for bringing definitions from files.

```text
import "oscillators.odex"
copy Vdp as a
copy Vdp as b
```

A local `system Vdp { ... }` needs only the two copy lines. This adds one keyword, but gives each word a clear job: import makes definitions available; copy creates equations and state. `use` leaves that distinction unclear. My earlier preference for one spelling gave vocabulary count too much weight.

B. Folding parameters helps navigation; it does not remove their semantic cost. Geometry declared `par` remains changeable during experiments and eligible for continuation. That changes the model’s interface even when its controls are collapsed. Copies’ genuine parameters should be grouped visibly and accessible through qualified names, including in AUTO.

I would retain constant-only `with` and defer valueless constants. The three examples need per-copy fixed values, but none needs a required constant declaration. The compartment can default to the actual soma geometry, with other copies explicitly overriding it:

```text
system Compartment {
  const len = 48.8e-4, diam = 48.8e-4, gpas = 1/225
  # equations and inputs
}
copy Compartment as ah with {
  len: 20e-4, diam: 12.94999838e-4
}
```

Those are legitimate defaults for this model. An abstraction with no defensible default can wait for required constants.

Also defer copy-time preset selection: none of the three examples requires `using`. Put the hillock’s initial values in the parent’s initial group. Keep dotted group entries and composed defaults; cutting their explicit rule would merely hide the behavior users must understand.

The learning path then starts with `system`, `copy`, `input` and equation-like bindings. Fixed configuration and named regimes are subsequent lessons. A first example need not introduce every supported feature.

The position I would sign:

- Local systems and imported definitions share one copying mechanism: `copy NAME as instance`.
- Continuous inputs have one binding; fixed per-copy constants use `with`; runtime values use groups.
- Keep shorthand defaults, dotted group entries and the agreed composed-default rule.
- Defer required constants, copy-time presets and `+=`.

Open maintainer decisions: approve `copy`, constant-only `with`, and restored shorthand. Folding copied parameters is useful presentation, but does not substitute for distinguishing constants from parameters.
