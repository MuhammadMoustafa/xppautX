# W309: parameter sets, states and settings — problems, decisions, rationale

Card W309 ([#368](https://github.com/MuhammadMoustafa/xppautX/issues/368)), settled with the
maintainer on 2026-10-11, before its implementation cards are written. It grew out of W306
([#365](https://github.com/MuhammadMoustafa/xppautX/issues/365), AUTO driven by a script, like
AUTO-07p's CLUI): a continuation starts from a state at some parameter values with some settings,
so a script needs a way to name "these parameters, this state, these settings". This file records
what exists, every problem found, each option discussed, what was decided and why, including the
designs that were accepted and later replaced. It is the input to a design review (debate.md).

## 1. What exists today (evidence)

- **The model file.** `.odex` (our syntax, docs/odex.md) declares parameters with their values
  (`par a = 1, b = 2`), initial values (`init x = 0.1`, any expression, evaluated after the
  parameters), settings (`options { numerics: { dt: 0.05 } }`) and XPPAUT's named sets
  (`set NAME { key: value }`, one set mixing parameters, variables and option keys). A `.ode`
  (XPPAUT's format) is converted to `.odex` on open (`--convert`).
- **Imports** (W282): `import "f.odex"`, `from "f.odex" import a, b`, `import "cell.odex" as c1`.
  A file holding only functions and parameters imports fine; opened alone it stops with XPPAUT's
  message "Must have at least one equation: probably not an ODE file" (tested 2026-10-11).
- **Settings file** `.setx` (W284, Discussion #341): `key: value` groups in the options syntax,
  holding what XPPAUT's `.set` held: `numerics`, `parameters`, `initial`, `ending`, `graphics`,
  `range`, ... A file names only what it sets. A `.snapx` (a whole session, a zip) keeps its
  state in `.setx` members (model.setx, windows.setx, sliders.setx, ...).
- **XPPAUT's value files.** `.par`: a count line, then `value  name` lines. Until W310 we read it
  by position and never checked the names: a file saved before the parameters were reordered or
  renamed, with the same count, loaded each value into the wrong parameter with no error. XPPAUT
  8.0 does the same (docs/xppaut-findings.md, finding 46). `.ic`: values only, one per
  differential equation, by position. We still *write* both formats, which breaks our own rule
  that XPPAUT formats are import-only.
- **Settings vocabulary.** The option table (core/model_options.cpp) has groups `numerics` (31
  keys: integration, plus equilibrium finding, nullcline mesh, Poincaré, torus), `auto` (24:
  continuation numerics plus the diagram's axes), `plot` (44, PostScript export included),
  `sliders` (9: exactly three slots, S1–S3), `range`, `batch` (`runnow`, output file), `histogram`
  (histograms and spectra), `log`, `buttons`. "Options", "settings" and "numerics" are all used
  for the same thing.
- **Defaults.** A parameter in `.odex` must have a value (`par a` alone is a parse error). A state
  with no `init` starts at 0 silently (tested: two states, both 0).
- **Sliders.** web2 has any number of sliders; the core's model options preset only three
  (`XPP_NSLIDERS`, XPPAUT's design).
- **runnow.** `@ runnow=1` / `--runnow` integrate once as soon as the model opens, in the window
  too. No example model uses it (the `runnow` in the `.ani` files is the animator's own).

## 2. Decisions, with rationale

### 2.1 Settings group names (decided)

Groups are named by what the user is doing, so a key's group says what it affects:

| Group | Holds | Was |
|---|---|---|
| `integration` | method, dt, time span, tolerances, storage, delay, seed | numerics |
| `poincare` | the section (`poincare_*`) | numerics |
| `equilibria` | `singpt_*`, `neartol` | numerics |
| `continuation` | AUTO's numerics (ntst, nmax, ds, parmin/parmax, eps*) | auto |
| `diagram` | AUTO's axes (`autoxmin`…, `autovar`) | auto |
| `plot` | view, axes, colours, nullcline mesh, direction-field grid | plot, numerics |
| `export` | `ps_*`, `plotfmt` | plot |
| `sweep` | Initconds > Range | range |
| `analysis` | histograms, spectra | histogram |
| `interface` | sliders (a list of any length), user buttons | sliders, buttons |
| `output` | output file, quiet, logfile | batch, log |

Rationale: "numerics" is vague (continuation and equilibrium finding are numerics too); "auto" names a
tool, not what it computes. The window keeps the name AUTO (the tool); its settings group is
`continuation` (its work). The slider presets become a list, since three slots are XPPAUT's limit, not ours.

### 2.2 runnow dropped (decided; W312, #371)

Opening a `.snapx` restores everything at once; a script (`.recx`, `.autox`) runs what it says;
`--silent` always runs; in the window the user presses Run. `--convert` drops `@ runnow` with a note.

### 2.3 A state with no initial value (decided; W313, #372)

It starts at 0, as in XPPAUT, but the load warns once, naming every such state with its file and
line; `--convert` writes the 0 explicitly so converted models load quietly. Considered and
rejected: an error (the maintainer judged a warning enough; 0 is a natural start).

### 2.4 `.par` read by name (decided and merged; W310, #369)

Each line's name must be a model parameter, each once, in any order; unknown, duplicate or missing
names are errors with file and line, nothing applied. `.ic` stays positional (it has no names), its
count checked. This is the same file format XPPAUT writes, read correctly.

### 2.5 Where parameter sets, states and settings live (decided after five revisions)

The maintainer's use cases:
- the same parameters with several initial conditions, without copying the parameters;
- several parameter regimes for one model (bursting, spiking);
- a steady state found by AUTO, reused as the start of the next run;
- mixing and matching: this settings set with that parameter set and those initial values;
- one self-contained model file that can be shared, as XPP users share `.ode` files;
- saving and loading every kind from the window and by a command, the same code in every mode.

The path, in order:

1. **Separate file kinds** (`.parx` parameters, `.icx` initial values, `.setx` settings, each in
   `.odex` syntax; `.odex` and `.autox` import them, they import nothing; XPPAUT's `.par`/`.ic`
   converted to them). Accepted, then replaced by 5 below.
2. **One `.setx` kind holding any subset of the groups** (a continuation-only `.setx` is a file
   of AUTO settings) rather than two settings kinds. Accepted: same kind of thing, one reader.
   An XPPAUT `.set` would convert to one file per kind it has content for, never an empty file.
3. **One values file (`.valx`) for parameters and initial values together.** Proposed by Claude
   by the logic of 2; rejected by the maintainer: values and settings, and parameters and states,
   are reused independently (one parameter set, many states), so they must not share a file.
   Claude agreed.
4. **Atomic files: one kind per file and one set per file.** Accepted: it removes the question
   "which set inside this file?". An import makes a set available, it does not apply it.
5. **No values or settings in `.odex`/`.autox` at all** (the model declares names; every value
   lives in imported files, one marked `default`). Raised by the maintainer, analysed by Claude,
   **rejected**: it would end XPP's single-file model (ModelDB's models and papers' supplements are
   single files), add the most overhead to the most common case (small models), undo the `.odex`
   syntax just built, and force a file for every scripted variation; its one gain (no competing
   defaults) is had more cheaply.
6. **Unnamed defaults plus named groups in `.odex`** (`par a = 1` lines are the defaults,
   `par bursting { ... }` groups are offered). Proposed by Claude; **rejected** by the maintainer
   as error-prone: an unnamed block silently acting as the default.
7. **FINAL: typed, named groups in `.odex`, one `default` per kind.**

```
par control default { gca: 1.1, gk: 2, iapp: 0.05 }   # gives every parameter
par bursting        { gca: 1.3, iapp: 0.09 }          # changes only what it names
init rest default   { v: -0.3, w: 0.01 }
init excited        { v: 0.2 }
options normal default { integration: { dt: 0.05 } }
```

   - Every group is named; one group per kind (`par`, `init`, `options`) carries `default`.
   - The `default` group is complete: a parameter it leaves out is an error naming it; a state it
     leaves out gets W313's warning and 0. Two defaults of one kind: an error naming both lines.
   - Any other group is applied over the defaults and changes only what it names. Groups are
     atomic: one kind, one set; the user mixes `bursting` with `rest` and `normal`.
   - Nothing is applied at load except the defaults; the other groups are offered.
   - The `{ key: value }` syntax is the one `options` and `.setx` already use: no new grammar.
   - XPPAUT's mixed `set` statements convert to typed groups of the same name, never an empty one.
   - XPPAUT's `.par`, `.ic` and `.set` are import-only: opening one applies it and offers to keep
     it as a named group. `.parx`/`.icx` are dropped (never built).
   - A group is a named statement, so the existing import takes it:
     `from "regimes.odex" import bursting, rest`. A large model (100 states, many states saved)
     keeps its groups in a second `.odex`.
   - **A partial `.odex` stays `.odex`** (no `.incx` kind like XPPAUT's habit of `.inc`): one
     grammar, a file may grow from library to model, and importing a complete model is legitimate
     too (`import "cell.odex" as c1`). Opened alone, a file with no equations says "it has no
     equations: it is a file to import into a model, not a model to run" (a small card).

### 2.6 Saving (decided)

One writer, three destinations, the same core operation in the window, `--server`, `--silent` and
a script (AGENTS.md "One behaviour, three front ends"):
- **Save** rewrites the group currently chosen, in place: only its own lines change, the rest of
  the file (comments, layout) stays as written.
- **Save as...** asks for a name and appends a new group at the end of the model file; an existing
  name asks before replacing.
- **Export...** writes the group to a separate `.odex` in the model's folder, headed
  `# Saved from lecar.odex, 2026-10-11 14:32`; the model imports it only if the user adds the import.

### 2.7 The window (decided)

Each section (parameters, initial conditions, settings) has one menu listing the groups the model
has or imports, plus "From file..." (an XPPAUT file or another `.odex`'s groups), with Save,
Save as and Export beside them: any set two clicks away.

## 3. Points Claude is not sure of (for the review)

1. **Declaration vs. default.** With every group named, where is a parameter *declared*? Today
   `par a = 1` declares and gives the value at once. If the `default` group declares, a model's
   parameter list lives inside a values group; if a separate `par a, b` declares, every name is
   written twice. Same question for states (declared by their equation; their default values in
   `init NAME default`).
2. **Breaking `.odex`.** `par a = 1` and `init x = 0` lines (unnamed) become illegal, so every
   `.odex` is regenerated from its `.ode` (our rule: no code for older files of ours). What does
   `--convert` name the default groups (`default`? the model's name?). The 183 examples' md5s must
   not change.
3. **Does `.setx` survive** as a user file kind (W284's Import settings) now that `options NAME {}`
   groups exist? `.snapx` uses `.setx` members internally either way.
4. **The program editing the user's model file** (Save, Save as): the user may have it open in an
   editor (VS Code with the XPP extension); the file's text and fingerprint change; a session
   (`.snapx`) saved earlier holds the older copy. Is append-and-replace-in-place safe enough?
5. **Expressions and order.** `init` values are expressions evaluated with the parameters. With
   groups, which parameters: the default group's, or the chosen one? (Proposed: apply the chosen
   `par` group first, then evaluate the chosen `init` group.)
6. **Arrays** in groups: `init rest default { x[j]: j/n for j in 1..n }`?
7. **Imported models** (`import "cell.odex" as c1`): are its groups prefixed (`c1.bursting`), and
   does its `default` stay a default inside the importer?
8. **Same name across kinds.** `par rest` and `init rest` both allowed, as a pair? Saving a
   steady state could offer "with its parameters" and write both under one name.
9. **Scripts.** An `.autox` run names its groups (`par: bursting, init: rest, options: fine`) and may
   still set a single value inline as a command's argument (AUTO-07p allows inline overrides).
10. **Many groups in one file.** The maintainer's first worry: a 100-state model with ten saved
    states makes a long model file. Export plus import is the answer; is it enough?
