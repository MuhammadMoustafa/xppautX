# W309: named sets, saving and loading values (design, 2026-10-12)

Replaces the typed named groups of [design.md](design.md) (`par NAME default { }`,
`init NAME { }`, `options NAME { }`, `preset`). The maintainer revised W309 on 2026-10-12, once
W314 (components) was parked: "either keep the old syntax or our new syntax, I think both
together will be a problem". This file is what follows from keeping XPPAUT's syntax.

It was argued with Codex Astra in four rounds ([sets-debate.md](sets-debate.md)) and with the
maintainer's answers between them; Astra signs it as written. The parts of design.md not about
groups still hold: dropping `.setx`, converting XPPAUT's `.set`/`.par`/`.ic`, `.snapx`/`.recx`
writing their state in `.odex` syntax, the sliders as a list, and W313's warning for a state
with no `init`.

## What exists today

- `.odex`: `par a = 1`, `init x = 1`, one `options { group: { key: value } }` block, and
  `set NAME { key: value }`, whose keys may be variables, parameters or options' keys, flat
  (docs/odex.md "Statements"). Lecar's `set vvst { xp: t, yp: v, ..., total_time: 100,
  iapp: 0.1 }` mixes them.
- A set key that is both a model name and an option key resolves to the model name
  (core/load_eqn.cpp:295, as XPPAUT 8.0's load_eqn.c:845): with `par dt = 2`,
  `set trial { dt: 0.1 }` changes the parameter. A parameter named `method` makes
  `method: stiff` fail.
- The window:
  - File > "Named parameter sets" applies a whole set, and so does the Parameters section's
    control (ValuesPanel.tsx:169).
  - File > "Copy parameter set line" puts a set line on the clipboard to paste into the model.
  - File > "Import settings" applies a `.setx`.
  - The Values panel's Parameters and State sections each have Save and Load of XPPAUT's
    `.par` and `.ic`, and a reset-to-default button on each field.
  - Settings are in three places: the Values panel's Numerics section, the plot's settings,
    and the AUTO window's settings.
- XPPAUT:
  - "Get par set" applies a whole set: parameters, initial conditions, settings.
  - The parameter and initial-conditions windows each have a Default button for their own
    kind.

## 1. Syntax

```
par gca = 1.1, iapp = 0.05          # the defaults: what a load gives
init v = -0.3, w = 0.01
options { numerics: { total_time: 100 }, plot: { xp: v, yp: w } }

set hopf    { gca: 1.1, phi: 0.2 }
set excited { v: 0.2, w: 0.1 }
set vvst    { iapp: 0.1, options: { plot: { xp: t, yp: v }, numerics: { total_time: 100 } } }
```

- **The defaults** are every `par` line, every `init` line and the `options` block, together:
  one baseline of each kind, with no name and no `default` word.
- **A set** holds any mix:
  - its top-level keys are parameters and states only, not consts, functions, `aux` or
    quantities;
  - settings go only under `options:`, in the `options` block's shape, and only from the
    groups that sets hold (section 2);
  - it holds no path, as today.
- **A set is checked at load:** a key that is not a parameter or state is an error at its line,
  and so is a setting its group does not have, or a bad value.
- **Set names** have a namespace of their own. Two sets of one name are an error naming both
  lines, an imported file's sets included.
- **The converter** turns XPPAUT's flat keys into this form by XPPAUT's precedence (model names
  first), then nests the settings.

## 2. What a set may hold: the option groups

The nine groups of core/model_options.h:

| Group | In sets and defaults | Notes |
|---|---|---|
| numerics | yes | |
| plot | yes, per graph | a graph's configuration (section 4); window layout is the session's |
| auto | yes | the next run's settings; threads excluded (a machine preference) |
| range | yes | |
| histogram | yes | |
| buttons | model only | the model's own buttons, in the options block, never in a set |
| batch | model only | what `--silent` runs |
| log | model only | `quiet` and `logfile` stay in the options block as today, never in a set |
| sliders | model only | the presets are the options block's list; positions and ranges in use are the session's |

**All** means parameters, initial values and the five groups marked yes, each on its target.
It is not a session restore. Each plot and continuation/diagram option is classified by the same
rule (W318, #378; the groups under W317's names, in core/model_options.h: `set_groups`, and the
`wide` flag of a row), a graph's or a run's own configuration (a set holds it), or wider (not):

| Option(s) | Group | Class | In a set |
|---|---|---|---|
| xp, yp, zp, nplot, xp2..xp8, yp2.., zp2.. | plot | a graph's own axes variables | yes |
| xlo, xhi, ylo, yhi (and 2..8), xmin..zmax, phi, theta | plot | a graph's own view and 3-D angles | yes |
| axes, lt, ync, xnc, smc, umc | plot | a graph's own style and nullcline/manifold colours | yes |
| nullcline_mesh, dfgrid, dfdraw, ncdraw | plot | a graph's own nullcline and direction-field settings | yes |
| colorvia, colorize, colorlo, colorhi | plot | a graph's own colouring | yes |
| colormap | plot | the one colour map the whole window uses (`color_table`, process-wide) | no (`wide`) |
| simplot, multiwin | plot | the window layout (a Simulplot list, several windows): the session's | no (`wide`) |
| ntst, nmax, npr, ncol, dsmin, dsmax, ds, parmin, parmax, normmin, normmax, epsl, epsu, epss | continuation | the next run's own configuration | yes |
| autoxmin, autoxmax, autoymin, autoymax, autovar, autoeval, sec, uec, spc, upc | diagram | the diagram's view (axes, and the colours/styles of its curves): a finished run's, not the next run's | no (group) |
| plotfmt, ps_font, ps_lw, ps_fsize, ps_color | export | how the session writes files | no (group) |

## 3. Reserved words

- **None added, none removed.** `set` and `options` are reserved already. The groups
  (`numerics`, `plot`, ...) and keys (`dt`, `method`, `total_time`, ...) are words only inside
  `options`.
- **A state or parameter named like a setting** (`par dt = 2`) is allowed and unambiguous:
  `set a { dt: 0.1, options: { numerics: { dt: 0.05 } } }` is one parameter and one setting.
- **A state or parameter named like a reserved word** (`par set = 1`) is refused at its line
  and column, naming the word, as today. A converted `.ode` with such a name asks for its
  replacement, as today.

## 4. Applying a set: a patch, with a scope and a target

- **A patch.** A set is checked, then only its entries change, over the current values, in one
  step. This is the maintainer's "dict update". Unlisted values stay, and a failure changes
  nothing.
- **A scope:** all, or one section (parameters, initial values, numerics, plot, auto, range,
  histogram). A scope applies only the set's entries of that kind; that is a deliberate
  projection, not error recovery.
- **A target, where the kind needs one:**
  - **plot:** a graph. A graph's section passes its own. The menu shows the target, the
    active graph by default; scripts and recordings name it. The model's plot defaults set up
    each new graph, and Restore on a graph uses them.
  - **auto:** the next run's configuration. A finished run keeps the settings it used, and the
    diagram's axes name their view.
- **A parameter-only apply** leaves the initial values alone and does not re-evaluate `init`
  expressions.
- **Rejected: reset-by-kind** (each kind the set names reset to the defaults first). It changes
  what the set does not say: one plot setting would undo every plot setting changed by hand.

## 5. The convention: scope follows the place

The maintainer, 2026-10-12: "the menu action items have place to apply all or apply section,
and the ui section only apply on section". Each operation is one core command with a scope and
a target (command_table.h; "One behaviour, three front ends"):
- the menu asks for both, defaulting to All and the active graph;
- a section's control passes its own;
- `--server` and `.recx` take the same arguments.

| Operation | Menu (File > Sets) | A section's row |
|---|---|---|
| Apply a set | All, or a section by name | its section only |
| Save into a new set or an existing one | choosing what to capture | its section only |
| Save as defaults | choosing which kinds | its kind only |
| Load from file (sets or defaults) | All, or a section | its section only |
| Restore defaults | Restore all, or a section | its section only (each field's reset stays) |
| Export current values | the chosen kinds | its section only |
| Rename, Delete a set | yes | no |

## 6. Each section's row

Sections:
- Parameters and Initial values (State) in the Values panel;
- Numerics in the Values panel;
- each graph's plot settings;
- the AUTO window's settings.

Range and histogram, which have dialogs, get the same row there. Each row has **Sets ▾**,
**Save ▾**, **Load from file…** and **Restore defaults**.

- **Sets ▾** lists the sets that hold this section's kind, marking an imported set with its
  file, and applies only that part.
- **Save ▾:**
  - **New set…** asks for a name and appends `set NAME { ... }` at the end of the model file,
    holding every current value of this section. Initial values asks once whether to save the
    initial values or the last state reached.
  - **Replace parameters in set…** (named for its kind in each section) picks a local set. It
    writes every current value of this section in place of that set's part of this kind, and
    keeps its other parts exactly. It says so before writing. This is how a mixed set is built
    from the window: parameters as a new set `hopf`, then Replace initial values in set →
    `hopf`. Editing a set's chosen entries by hand is the file's text, not this command.
  - **As defaults** rewrites this kind's defaults:
    - Parameters: the `par` lines.
    - Initial values: the `init` lines.
    - A settings section: its group in the `options` block.

    Restore uses the new defaults at once.
  - **Export…** captures this section's current values into another `.odex`: a new file with
    the "Saved from X at T" header, or an existing one. It asks for the set's name; an existing
    name there gets the overwrite question, saying it replaces that set's part of this kind.
- **Load from file…** parses and checks the whole source file, offers its sets that hold this
  kind and its defaults of this kind, and checks the chosen part against this model. Nothing is
  applied unless all of it passes: an unknown parameter refuses the load, never skipped.
  Nothing runs: no equations, no logfile, no defaults applied. The model file is not changed
  until saved. XPPAUT files are imported here too:
  - `.par` in Parameters;
  - `.ic` in Initial values;
  - `.set` in any section, taking only that section's part.
- **Restore defaults** resets this section to the model's defaults. For settings, that is the
  built-in values with the model's `options` over them.

## 7. The menu (File > Sets)

- **Apply set…** chooses a set, a scope (All by default) and, for plot, a graph.
- **Save values…** chooses New set or Replace in set, and which kinds to capture.
- **Save as defaults…** chooses which kinds.
- **Load from file…** chooses a file, one of its sets or its defaults, and a scope.
- **Import XPPAUT session state…** applies a whole XPPAUT `.set`: parameters, initial values,
  settings, boundary conditions, delay initial data, graphs and the run position. It names what
  it changes, checks every field first, and applies none if one fails. What a set cannot hold
  is kept by saving the session.
- **Restore defaults…** chooses All or a section.
- **Export…**, **Rename…** and **Delete…**:
  - Rename and Delete work on local sets, through the text-preserving writer.
  - They refuse while the model (another set, an import) refers to the name.
  - A script or recording elsewhere cannot be found ahead; its step fails at the name, with
    the file and line.
- **What goes:**
  - "Copy parameter set line": Save writes the set into the file.
  - "Import settings": `.setx` goes.
  - The Values panel's Save of `.par`/`.ic`: we never write an XPPAUT format; their Load stays
    as an import.
- **The session** (`.snapx`) is still the one save and open of everything, and holds the model
  it ran.

## 8. Mix and match without copy and paste

- Patches combine: `hopf` from Parameters, then `excited` from Initial values.
- A section applies its part of a mixed set, so two mixed sets combine without editing either.
- Save captures the combination as a new set.
- Another file's sets can be used once, with Load from file, or always, with
  `import "regimes.odex"` in the model. A file of only sets is a valid partial `.odex`; its
  sets are checked against the model that imports or loads them.

## 9. Writing files

- **Order:** build the new text, parse and check it, check the file has not changed on disk
  since it was read (its fingerprint), replace it atomically, then update the live sets,
  defaults, places and fingerprint.
- **The rest of the file** keeps its text as written, comments included.
- **A new set** goes at the end of the file.
- **An imported file's sets** are read-only in the window. Saving over one asks for a new local
  name. Editing a shared library's set in place is deferred.

## 10. Not in sets

- Boundary conditions and delay initial data are the model's equations, not values; XPPAUT's
  sets do not hold them either. They keep their own Values sections without a sets row. An
  XPPAUT `.set`'s values for them come in only through Import XPPAUT session state, and are
  kept by the session.
- Window layout (how many graphs, where) and the sliders in use are the session's.
