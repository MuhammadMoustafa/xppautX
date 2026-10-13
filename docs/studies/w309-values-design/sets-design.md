# W309: named sets, saving and loading values (design, 2026-10-12)

Replaces the typed named groups of [design.md](design.md) (`par NAME default { }`,
`init NAME { }`, `options NAME { }`, `preset`). The maintainer revised W309 on 2026-10-12, once
W314 (components) was parked: "either keep the old syntax or our new syntax, I think both
together will be a problem". This file is what follows from keeping XPPAUT's syntax. It
includes the outcome of [sets-debate.md](sets-debate.md) and the maintainer's answers since.
The parts of design.md not about groups still hold: dropping `.setx`, converting XPPAUT's
`.set`/`.par`/`.ic`, `.snapx`/`.recx` writing their state in `.odex` syntax, the sliders as a
list, and W313's warning for a state with no `init`.

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
  - File > "Named parameter sets" applies a set.
  - File > "Copy parameter set line" puts a set line on the clipboard to paste into the model.
  - File > "Import settings" applies a `.setx`.
  - The Values panel's Parameters and State sections each have Save and Load of XPPAUT's
    `.par` and `.ic` files, and a reset-to-default button on each field.
  - Settings are in three places: the Values panel's Numerics section, the plot's settings,
    and the AUTO window's settings.
- XPPAUT itself:
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

- The `par` and `init` lines and the `options` block are the defaults. There is one of each
  kind, with no name and no `default` word.
- A set holds any mix of the three kinds. Its top-level keys are always the model's names
  (parameters, states). Settings go only under `options:`, in the `options` block's own shape
  and group names.
- A set holds no path, as today.
- A set is checked whole at load:
  - a key that is not a model name is an error at its line;
  - so is a setting that its group does not have, and a bad value.
- Set names have a namespace of their own, apart from the model's names. Two sets of one name
  are an error naming both lines, an imported file's sets included.
- The converter turns XPPAUT's flat keys into this form by XPPAUT's precedence (model names
  first), then nests the settings.

## 2. Reserved words

- **None added, none removed.** `set` and `options` are reserved already. The settings groups
  (`numerics`, `plot`, `auto`, ...) and keys (`dt`, `method`, `total_time`, ...) are words only
  inside `options`.
- **A state or parameter named like a setting** (`par dt = 2`) is allowed and unambiguous:
  `set a { dt: 0.1, options: { numerics: { dt: 0.05 } } }` is one parameter and one setting.
- **A state or parameter named like a reserved word** (`par set = 1`) is refused at its line
  and column, naming the word, as today. A converted `.ode` with such a name asks for its
  replacement, as today.

## 3. Applying a set: a patch

- A set is checked whole, then only its entries change, over the current values, in one step.
  This is the maintainer's "dict update". Unlisted values stay; a failure changes nothing.
- Applied with a scope: **all**, or one section (parameters, initial values, numerics, plot,
  auto). A scope applies only the set's entries of that kind.
- A parameter-only apply leaves the initial values alone and does not re-evaluate `init`
  expressions.
- Reset-by-kind (each kind the set names reset to the defaults first) was rejected in
  sets-debate.md. It changes what the set does not say: one plot setting in a set would undo
  every plot setting changed by hand.

## 4. The convention: scope follows the place

The maintainer, 2026-10-12: "the menu action items have place to apply all or apply section,
and the ui section only apply on section". Each operation is one core command with a scope
(command_table.h; "One behaviour, three front ends"):
- the menu asks for the scope and defaults to All;
- a section's control passes its own scope;
- `--server` and `.recx` take the same scope argument.

| Operation | Menu (File > Sets) | A section's row |
|---|---|---|
| Apply a set | All, or a section by name | its section only |
| Save | New set or Into set, choosing what to capture | its section only |
| Load from file | All, or a section | its section only |
| Restore defaults | Restore all, or a section | its section only (each field's reset stays) |
| Export | the whole set | its section's part |

## 5. Each section's row

The sections:
- Parameters and Initial values (State) in the Values panel;
- Numerics in the Values panel;
- the plot's settings;
- the AUTO window's settings.

Each has the same row: **Sets ▾**, **Save ▾**, **Load from file…**, **Restore defaults**.

- **Sets ▾** lists the sets that hold this section's kind and applies only that part.
- **Save ▾:**
  - **New set…** asks for a name and appends `set NAME { ... }` to the model file. The set
    holds every value of this section; Initial values asks once whether to save the initial
    values or the last state reached.
  - **Into set…** picks a set and replaces that set's part of this kind, keeping its other
    parts. It shows the target and what it holds. This is how a mixed set is built from the
    window: save parameters as `hopf`, then from Initial values choose Into set → `hopf`.
  - **As defaults** rewrites this kind's defaults:
    - Parameters: the `par` lines.
    - Initial values: the `init` lines.
    - A settings section: its group in the `options` block.
  - **Export…** writes this section's part to another `.odex`, as a new file with the
    "Saved from X at T" header, or appended to an existing one.
- **Load from file…** opens any `.odex` and offers its sets that hold this kind, and its
  defaults of this kind. Only this section's part is applied, and nothing is added to the model
  file until saved. XPPAUT files are imported here too:
  - `.par` in Parameters;
  - `.ic` in Initial values;
  - `.set` in any section, which takes that section's part.
- **Restore defaults** resets this section's values to the model's defaults. For settings
  that means the built-in values with the model's `options` over them.

## 6. The menu (File > Sets)

- **Apply set…** chooses a set and a scope (All by default).
- **Save values…** chooses New set or Into set, and which kinds to capture.
- **Load from file…** chooses a file, one of its sets and a scope.
- **Restore defaults…** chooses All or a section.
- Export from the menu writes whole sets.
- "Copy parameter set line" goes: Save writes the set into the file.
- "Import settings" (`.setx`) goes with `.setx`.
- The session, `.snapx`, is still the one save and open of everything.

## 7. Mix and match without copy and paste

- Patches combine: `hopf` from Parameters, then `excited` from Initial values.
- A section applies its part of a mixed set, so two mixed sets combine without editing either.
- Save captures the combination as a new set.
- Another file's sets: once, with Load from file; always, with `import "regimes.odex"` in the
  model, whose sets are then listed with the model's own.

## 8. Writing to files

- Save keeps the rest of the file's text as written, comments included.
- It checks the file has not changed on disk since it was read, and replaces it atomically
  (W309's settled rules).
- A new set goes after the last set, or at the end.
- A set from an imported file is saved into that file only after a question naming it.
- Saving never writes an XPPAUT format: the Values panel's `.par`/`.ic` Save goes, and their
  Load stays as an import.

## 9. Not in sets

- Boundary conditions and delay initial data (also in the Values panel) are the model's
  equations, not values; XPPAUT's sets do not hold them either. They keep their own sections
  without a sets row.
- Sliders' ranges and what the windows show are the session's, not the model's.

## Open

- Is a set's position in the file worth controlling (Save into a chosen place), or is "after
  the last set" enough?
- Renaming and deleting a set from the window, or only in the file's text?
