# The xppautX manual

**Credit.** This manual is G. Bard Ermentrout's XPPAUT documentation, in
his words: the notes, the examples and the first-person asides are his.
xppautX changed only what the program changed: the interface chapters are
rewritten for the page xppautX shows, and chapter 1 is rewritten for xppautX.
XPPAUT and its manual are copyright Bard Ermentrout.

The model language, numerics and autoX chapters are kept faithful to the
original; the chapters about windows, the mouse and keystrokes describe the
page `xppautX model.ode` opens. Where XPPAUT differs from xppautX the
differences are listed once, by area, in
[xppautx-vs-xppaut.md](../xppautx-vs-xppaut.md).

## Chapters

1. [Introduction](01-introduction.md) — what XPP is, environment variables
2. [ODE Files](02-ode-files.md) — the model language: equations, parameters, tables, DAEs, Markov and delay variables, options
3. [Examples](03-examples.md) — worked models (cable equations, networks, DAEs, ...)
4. [Using the interface](04-using-the-interface.md) — **web2**: starting xppautX, the page layout, the values panel, plots, dialogs, files, the log
5. [The main commands](05-commands.md) — every main-menu command and its sub-choices
6. [Numerical parameters](06-numerical-parameters.md) — the Numerics menu: integration methods, tolerances, Poincaré maps, curve fitting, averaging
7. [The Data Table](07-data-table.md) — the Table: Find, Get, Replace, Table, Write, Load, Addcol, Delcol
8. [Functional equations](08-functional-equations.md) — Volterra/integral equations, convolutions, networks (`special`)
9. [autoX interface](09-autox.md) — the autoX view, parameters, diagram axes, numerics, running, grabbing, homoclinics
10. [Creating Animations](10-animations.md) — the DASL scripting language and the Animation tab
12. [Some comments on the numerical methods](12-numerical-methods-notes.md)
13. [Colors](13-colors.md) — the curve color indices and their web2 palette
14. [The options file](14-options-file.md) — dropped; use `@` lines
15. [The settings panels](15-settings-panels.md) — one panel for each group of settings, generated from the option table: Integration, Poincare, Equilibria, Sweep, Continuation, Diagram, Plot, Analysis, Export, Output
16. [Quick reference](16-quick-reference.md) — ODE file format cheat sheet, built-in functions, the full options list, command line arguments

## Menus and dialogs → sections

Which part of the page each section documents; the Help view links a menu item or dialog to its section here.

| web2 | Section |
|---|---|
| Title bar, Menu button/drawer, status bar (Working…/Stop) | [Using the interface: the page layout](04-using-the-interface.md#the-page-layout), [long-running commands](04-using-the-interface.md#long-running-commands) |
| Plot tabs, zoom/pan/reset/Fit, "Use this view", legend, 3D rotation | [Using the interface: plots and axes](04-using-the-interface.md#plots-and-axes) |
| Values panel: initial conditions, parameters, checkboxes, sliders, user buttons, boundary conditions, delay data | [Using the interface: the values panel](04-using-the-interface.md#the-values-panel) |
| Slider Add/Edit dialog | [Using the interface: the values panel](04-using-the-interface.md#the-values-panel) |
| Table | [The Data Table](07-data-table.md) (commands below) |
| autoX view (status strip, Output, axis dialog, Save/Load session, Grab, Clear) | [autoX interface: the autoX view](09-autox.md#the-autox-view) |
| Animation tab | [Creating Animations: the animation view](10-animations.md#the-animation-view) |
| File dialogs (Open/Save, Add file…) | [Using the interface: saving pictures and files](04-using-the-interface.md#saving-pictures-and-files) |
| Messages panel, `--verbose`/`--debug`/`--logfile` | [Using the interface: the log](04-using-the-interface.md#the-log) |
| A `%formula` field | [Using the interface: formulas as values](04-using-the-interface.md#formulas-as-values) |
| Main menu — Initialconds | [The main commands: Initial conds](05-commands.md#initial-conds) |
| Main menu — Continue | [The main commands: Continue](05-commands.md#continue) |
| Main menu — Nullclines | [The main commands: Nullclines](05-commands.md#nullclines) |
| Main menu — Dir.field/Flow | [The main commands: Direction Field/Flow](05-commands.md#direction-fieldflow) |
| Main menu — Window/zoom | [The main commands: Window](05-commands.md#window) |
| Main menu — Phase space | [The main commands: phase space](05-commands.md#phase-space) |
| Main menu — Kinescope | [The main commands: Kinescope](05-commands.md#kinescope) |
| Main menu — Graphic stuff | [The main commands: Graphic stuff](05-commands.md#graphic-stuff) |
| Main menu — Numerics | [The main commands: numerics](05-commands.md#numerics), [Numerical parameters](06-numerical-parameters.md) |
| Main menu — File | [The main commands: File](05-commands.md#file) |
| Main menu — Parameters | [The main commands: Parameters](05-commands.md#parameters) |
| Main menu — Erase | [The main commands: Erase](05-commands.md#erase) |
| Named windows | [The main commands: named plot windows](05-commands.md#named-plot-windows) |
| Main menu — Text,etc | [The main commands: Text, etc](05-commands.md#text-etc) |
| Main menu — Sing pts | [The main commands: Sing pts](05-commands.md#sing-pts) |
| Main menu — Viewaxes | [The main commands: View axes](05-commands.md#view-axes) |
| Main menu — Xi vs t | [The main commands: Xi vs t](05-commands.md#xi-vs-t) |
| Main menu — Restore | [The main commands: Restore](05-commands.md#restore) |
| Main menu — 3d params | [The main commands: 3d params](05-commands.md#3d-params) |
| Main menu — Bndry val | [The main commands: Bndry val](05-commands.md#bndry-val) |
| Numerics — Total/Start/Transient/Dt/Bounds/Method/Delay/... | [Numerical parameters](06-numerical-parameters.md) (one section per item, e.g. [Total](06-numerical-parameters.md#total), [Method](06-numerical-parameters.md#method)) |
| Numerics — Poincaré map | [Numerical parameters: Poincare map](06-numerical-parameters.md#poincare-map) |
| Numerics — Averaging | [Numerical parameters: Averaging](06-numerical-parameters.md#averaging) |
| Table — Find/Get/Replace/Unreplace/First/Last/Table/Restore/Write/Load/Addcol/Delcol | [The Data Table](07-data-table.md) (one section per button) |
| autoX — Parameter, Axes, Numerics, User, Run, Grab, Abort, File | [autoX interface](09-autox.md): [Choosing parameters](09-autox.md#choosing-parameters), [Diagram axes](09-autox.md#diagram-axes), [Numerical parameters](09-autox.md#numerical-parameters), [User functions](09-autox.md#user-functions), [Running](09-autox.md#running), [Grabbing](09-autox.md#grabbing), [Aborting](09-autox.md#aborting), [Saving diagrams](09-autox.md#saving-diagrams) |
