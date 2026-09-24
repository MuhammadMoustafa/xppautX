# The xppautX manual

XPP's manual, converted from the original LaTeX (`docs/upstream/xpp_doc.tex`,
`docs/upstream/xpp_sum.tex`) to Markdown (docs/roadmap.md W12). The model
language, numerics and AUTO chapters are kept faithful to the original; the
chapters about windows, the mouse and keystrokes are rewritten for the
current front end, **web2** (the page `xppautX model.ode` opens). Where an
X11 feature has no web2 equivalent yet, the relevant chapter says so
instead of describing the old window.

A task that changes the UI updates the matching section here, the same way
it updates docs/protocol.md (see CLAUDE.md). web2's own Help view (planned,
docs/roadmap.md W12b) will search these files and link menu items and
dialogs straight to the sections below.

## Chapters

1. [Introduction](01-introduction.md) — what XPP is, environment variables, `.xpprc`
2. [ODE Files](02-ode-files.md) — the model language: equations, parameters, tables, DAEs, Markov and delay variables, options
3. [Examples](03-examples.md) — worked models (cable equations, networks, DAEs, ...)
4. [Using the interface](04-using-the-interface.md) — **web2**: starting xppautX, the page layout, the values panel, plots, dialogs, files, the log
5. [The main commands](05-commands.md) — every main-menu command and its sub-choices
6. [Numerical parameters](06-numerical-parameters.md) — the Numerics menu: integration methods, tolerances, Poincaré maps, curve fitting, averaging
7. [The Data Browser](07-data-browser.md) — the Data tab: Find, Get, Replace, Table, Write, Load, Addcol, Delcol
8. [Functional equations](08-functional-equations.md) — Volterra/integral equations, convolutions, networks (`special`)
9. [Auto interface](09-auto.md) — the AUTO view, parameters, diagram axes, numerics, running, grabbing, homoclinics
10. [Creating Animations](10-animations.md) — the DASL scripting language and the Animation tab
11. [Creating C-files for faster simulations](11-dll-libraries.md) — dynamically linked right-hand sides (`load dll`)
12. [Some comments on the numerical methods](12-numerical-methods-notes.md)
13. [Colors](13-colors.md) — the curve colour indices and their web2 palette
14. [The options file](14-options-file.md) — `option <filename>`, the `.opt` format
15. [C Files](15-generated-c-files.md) — `-m`: generating a C skeleton for a model's right-hand sides
16. [Quick reference](16-quick-reference.md) — ODE file format cheat sheet, built-in functions, the full options list, command line arguments

## Menus and dialogs → sections

Which part of web2 each section documents, for the Help view (W12b) to
link from a menu item or dialog to its section here.

| web2 | Section |
|---|---|
| Title bar, Menu button/drawer, status bar (Working…/Stop) | [Using the interface: the page layout](04-using-the-interface.md#the-page-layout), [long-running commands](04-using-the-interface.md#long-running-commands) |
| Plot tabs, zoom/pan/undo, "Use this view", legend, 3D rotation | [Using the interface: plots and axes](04-using-the-interface.md#plots-and-axes) |
| Values panel: initial conditions, parameters, checkboxes, sliders, user buttons, boundary conditions, delay data | [Using the interface: the values panel](04-using-the-interface.md#the-values-panel) |
| Slider Add/Edit dialog | [Using the interface: the values panel](04-using-the-interface.md#the-values-panel) |
| Data tab | [The Data Browser](07-data-browser.md) (commands below) |
| AUTO view (status strip, Output, axis dialog, Save/Load settings, Grab, Clear) | [Auto interface: the AUTO view](09-auto.md#the-auto-view) |
| Animation tab | [Creating Animations: the animation view](10-animations.md#the-animation-view) |
| File dialogs (Open/Save, "In the model's folder", Add file…) | [Using the interface: saving pictures and files](04-using-the-interface.md#saving-pictures-and-files) |
| Messages panel, `--verbose`/`--debug`/`-logfile` | [Using the interface: the log](04-using-the-interface.md#the-log) |
| A `%formula` field | [Using the interface: formulas as values](04-using-the-interface.md#formulas-as-values) |
| Main menu — Initialconds | [The main commands: (I)nitial conds](05-commands.md#initial-conds) |
| Main menu — Continue | [The main commands: (C)ontinue](05-commands.md#continue) |
| Main menu — Nullclines | [The main commands: (N)ullclines](05-commands.md#nullclines) |
| Main menu — Dir.field/Flow | [The main commands: (D)irection Field/Flow](05-commands.md#direction-fieldflow) |
| Main menu — Window/zoom | [The main commands: (W)indow](05-commands.md#window) |
| Main menu — Phase space | [The main commands: ph(A)se space](05-commands.md#phase-space) |
| Main menu — Kinescope | [The main commands: (K)inescope](05-commands.md#kinescope) |
| Main menu — Graphic stuff | [The main commands: (G)raphic stuff](05-commands.md#graphic-stuff) |
| Main menu — Numerics | [The main commands: n(U)merics](05-commands.md#numerics), [Numerical parameters](06-numerical-parameters.md) |
| Main menu — File | [The main commands: (F)ile](05-commands.md#file) |
| Main menu — Parameters | [The main commands: (P)arameters](05-commands.md#parameters) |
| Main menu — Erase | [The main commands: (E)rase](05-commands.md#erase) |
| Main menu — Makewindow | [The main commands: (M)ake window](05-commands.md#make-window) |
| Main menu — Text,etc | [The main commands: (T)ext, etc](05-commands.md#text-etc) |
| Main menu — Sing pts | [The main commands: (S)ing pts](05-commands.md#sing-pts) |
| Main menu — Viewaxes | [The main commands: (V)iew axes](05-commands.md#view-axes) |
| Main menu — Xi vs t | [The main commands: (X)i vs t](05-commands.md#xi-vs-t) |
| Main menu — Restore | [The main commands: (R)estore](05-commands.md#restore) |
| Main menu — 3d params | [The main commands: (3)d params](05-commands.md#3d-params) |
| Main menu — Bndry val | [The main commands: (B)ndry val](05-commands.md#bndry-val) |
| Numerics — Total/Start/Transient/Dt/Bounds/Method/Delay/... | [Numerical parameters](06-numerical-parameters.md) (one section per item, e.g. [(T)otal](06-numerical-parameters.md#total), [(M)ethod](06-numerical-parameters.md#method)) |
| Numerics — Poincaré map | [Numerical parameters: (P)oincare map](06-numerical-parameters.md#poincare-map) |
| Numerics — Averaging | [Numerical parameters: (A)veraging](06-numerical-parameters.md#averaging) |
| Data tab — Find/Get/Replace/Unreplace/First/Last/Table/Restore/Write/Load/Addcol/Delcol | [The Data Browser](07-data-browser.md) (one section per button) |
| AUTO — Parameter, Axes, Numerics, User, Run, Grab, Abort, File | [Auto interface](09-auto.md): [Choosing parameters](09-auto.md#choosing-parameters), [Diagram axes](09-auto.md#diagram-axes), [Numerical parameters](09-auto.md#numerical-parameters), [User functions](09-auto.md#user-functions), [Running](09-auto.md#running), [Grabbing](09-auto.md#grabbing), [Aborting](09-auto.md#aborting), [Saving diagrams](09-auto.md#saving-diagrams) |
