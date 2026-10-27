# .odex: the model format without .ode's quirks

Opening a `.ode` converts it and saves `<name>.odex` beside it in every
mode: window, browser, `--server` and `--silent`, File > Open model,
Reload and a double-click (W154, #206). From then on the `.odex` is the
model. The loader reads only `.odex`; only the converter reaches the
foreign `.ode` parser. A name reserved here asks for its replacement,
with a suggestion as the default; `--silent` accepts the suggestions.
Cancel keeps the previous model and writes nothing. Equal existing text
opens unchanged (CRLF and LF are the same lines); different text asks
Replace, Open existing .odex, or Cancel, using the existing overwrite
question. `--silent` refuses it, naming both files. Failure to write names
the `.odex` path; there is no memory-only open. `.snapx` and `.recx` store
`.odex` model text and included files and refuse a `.ode` model.

W73 (#121) introduced `.odex`; W154 makes it the model format.
Foreign `.ode` input remains supported through conversion. Both readers
use the same in-memory `Model` builder (core/model.h); the converter
checks that its `.odex` compiles to the `.ode`'s programs and values.
The integrators, AUTO and front end then use the saved `.odex`.
AUTO's variable and parameter storage is sized by that model, with no fixed
400-variable or eight-selected-parameter limit; its branch-point storage
grows with the run (W298, [#356](https://github.com/MuhammadMoustafa/xppautX/issues/356)).

This document specifies the language implemented by the `.odex` parser
and `--convert`. `--check` reports foreign `.ode` quirks and load errors
without writing files (W75, [#123](https://github.com/MuhammadMoustafa/xppautX/issues/123)). docs/odex-quirks.md inventories every `.ode` quirk
`.odex` removes.

## Checking a .ode

Run `xppautX --check model.ode` to convert and validate in memory, with
no window, integration, or written `.odex`. Reserved names take the
converter's suggested replacements. `--check model.odex` uses the `.odex`
reader and reports only errors. Inspection does not open `@ logfile`,
and asks nothing: a path outside the model's folder that the preferences
do not already allow is a load error at its line (a load would ask, W286).
Stdout is one UTF-8 JSON object followed by a newline:

```json
{"file":"model.ode","diagnostics":[{"file":"model.ode","line":2,"col":10,"source":"aux z=2*3<4","id":"comparison-precedence","severity":"warning","message":"XPP comparisons bind before arithmetic or unary minus; conversion parenthesizes that meaning."}]}
```

The top-level `file` is the input path as supplied. Each diagnostic has its
own `file` (including an included file's path), `line` and
`col` (1-based; 0 when unknown), `source` (the line as written), stable
`id`, `severity` (`warning`, `info`, or `error`), and `message`.
A load error is a diagnostic with id `load-error`, even for an unreadable
file (line/col 0, empty source). Findings made before a conversion error
remain in the array. Exit status is 0 with no warnings or errors (information
alone also returns 0), 1 with warnings, 2 with an error. An empty array
means clean. Usage errors return 2 and print usage on stderr.

Model file reads stay inside the main model's folder: absolute include names,
`..` traversal, links, junctions and non-regular files are refused. Nested
plain subfolders remain usable. Model files share the session archive's
512 MiB and 4096-file limits. Formula and file tables together have a budget of
1,000,000 points (8 MB of values) per session, checked before allocation or
evaluation; replacing a table reuses its share. These restrictions also apply
when opening and converting models (W175, [#227](https://github.com/MuhammadMoustafa/xppautX/issues/227)).
Valid formulas may still demand substantial CPU time. A local writer can
race an ancestor-directory check or supply hard links; use operating-system
isolation when that is part of the threat model.

The converter owns the findings, including the notes already written as
`.odex` comments. Opening a `.ode` shows them once, with their places,
beside the converted/opened-existing message in the log panel (the console
for `--silent`). A clean input adds no diagnostic messages. Explicit
parentheses remove precedence warnings; information explains preserved
power grouping, unary-minus precedence and trailing `if` operators.
The inventory's diagnostic ids and known limits are in
[odex-quirks.md](odex-quirks.md#diagnostic-ids-w75).
`uv run --locked python tools/checkcheck.py --bin ./xppautX.exe` tests each quirk and the
JSON/no-write contract; `verify.sh` runs it too.

## Design decisions (from issue #121)

- **A `.ode` gives XPPAUT's numbers** (maintainer, 2026-09-27): every quirk
  of the `.ode` reader stays as XPPAUT has it, so the same file gives the
  same result in both programs; a fix goes into `.odex` only, and the
  converter records the quirks in comments. Conversion on open and
  `--convert` (W74) keep the `.ode`'s meaning, writing what XPPAUT computes (with a
  comment where that differs from what the line seems to say). Example:
  `y(0)=a` starts y at 0 in `.ode` (a scalar initial value is read with
  `atof`, which finds no number in `a`; measured 2026-09-27, see
  odex-quirks.md); `.odex` evaluates it after the parameters;
  `--convert` writes `y(0)=0` with a comment naming the original expression.

- Branching and functions get a formal shape, marked with curly braces,
  never indentation: models travel through email, PDFs and papers, where
  indentation breaks silently. A missing `}` is a parse error at a line
  and column; a wrong indent is never significant.
- Usual arithmetic operator precedence: arithmetic first, then
  comparisons, then `not`, then `and`, then `or`. `^` is right-
  associative. `-x^2` means `-(x^2)`, matching ordinary maths notation
  (unchanged from `.ode`'s own accidental behaviour there).
- Logical operators are the words `and`, `or`, `not` — reserved words.
  `.ode`'s `&`, `|`, and `not(...)` are not `.odex` syntax; `--convert`
  maps them over exactly (`&``->``and`, `|``->``or`, `not(x)``->``not x`).
- `!=` is a working operator (`.ode`'s is dead in the parser table).
- One-line `if` form: an ordinary expression with usual precedence,
  parentheses optional: `if c then a else b`. `else if` chains a further
  condition (no `elif` keyword): `if a then 0 else if b then 1 else 2`.
- Block functions use curly braces: `fun g(v, w) { let s = v + w  if
  s > vth { return a*s } else if s > 0 { return b } else { return 0 } }`.
  A block function is pure: it may declare locals with `let` and must
  `return`, and may never write a parameter or a state variable. It
  compiles to the same stack program with jumps the current `if/then/
  else` compiler already produces (expr_compile.cpp, `alg_to_rpn`), so there is
  no runtime cost. Locals are substituted expressions; function arguments
  are evaluated once before a call.
- Names are case-sensitive (`V` and `v` are two names) with no length
  limit (neither has `.ode` since W76).
- Every problem is an error carrying a line and column. Nothing is
  silently dropped, cut, or ignored — the opposite of `.ode`'s `atof`
  truncation on `@` options, its spacing-sensitive `@` lines, and its
  "line not recognised, skip it" behaviour.

## Statements

`.odex` keeps the same statements as `.ode`, spelled with `.odex`'s own
grammar:

- State variable: `x' = EXPR` (the only derivative spelling; `.ode`'s
  `dx/dt=`, `x(t+1)=` and `!x=` are covered by `--convert`, not by new
  `.odex` syntax, since a fresh `.odex` model always uses `x'=`).
- `par NAME = VALUE, NAME = VALUE, ...` — one or more parameters, each a
  fixed value (below: `par tau = 1/30` is fine, stored as its number; a
  quantity worked out from other parameters is `d = expr`, below).
- `const NAME = VALUE, ...` — a constant fixed at load (W80, `.ode`'s
  `number`): its value a fixed value, usable anywhere a number is
  (formulas, a range's ends and step), not in the values panel, never set.

Fixed values (W490, decision 9 of
[docs/studies/w381-choices.md](studies/w381-choices.md)): `par`, `const` and
`init` take one kind of value, read by one function (`fixed_value`,
core/odex_load.cpp), the same one that reads a range's ends and an
element's index: numbers, `+ - * / mod ^` and a sign, `pi`, consts (a const
array's element `c[2]` too), the pure built-in functions of those (`sin`,
`exp`, `max`, `atan2`, `besselj`, ...: every one the evaluator calls by
name but `ran`, `normal`, `poisson`, `delay`, `shift`, `sum`), and the
range's index. `init` alone may call `ran(x)`, drawn once when the model is
set up. A parameter, a state, an aux or fixed variable or a user function
is an error at its line, in the words `the initial value of v can't use the
parameter vrest; use a const or a number`: a value worked out from
parameters is `d = expr`. A `par` or `init` value is kept as the number it
works out to, and the model writer (File > Save values) leaves the text
you wrote (`1/30`) alone while the value is unchanged.
- `NAME = EXPR` — a quantity: always the formula's current value. When the
  formula reads only parameters, consts, pure functions (not `ran`,
  `normal`, `poisson`, nor a user function calling one) and the
  quantities before it, the loader works it out only when a parameter
  changes (a slider, an event setting a parameter, each AUTO evaluation),
  as `.ode`'s `!` does, so the numbers are identical; otherwise at every
  evaluation (question 9).
- `init NAME = VALUE, ...` — initial conditions: a fixed value, worked
  out at load (a parameter is no part of one: `init` cannot read it, the
  `.ode` quirk in docs/odex-quirks.md where a parameter read into
  `init`/`x(0)=` was measured as 0 is an error here). A state without `init` starts at 0; one warning per load
  lists those states at their equation files and lines (an array once with
  its missing range). Conversion writes explicit zero initial values for
  every state the `.ode` left unspecified (W313, [#372](https://github.com/MuhammadMoustafa/xppautX/issues/372)).
- `aux NAME = EXPR, ...` — auxiliary (recorded, non-integrated) values.
- `fun NAME(ARG, ...) = EXPR` — a one-line function, usual precedence,
  may use the one-line `if`.
- `fun NAME(ARG, ...) { ... }` — a block function (above).
- `options { group: { key: value, ... }, ... }` — the options, one block
  (W280, [#337](https://github.com/MuhammadMoustafa/xppautX/issues/337);
  replaces `@` lines, which a `.ode` keeps and `--convert` writes as this
  block). Entries are `key: value`, separated by commas, groups nested, a
  trailing comma allowed. `options` is a reserved word; the keys and values
  are read only inside the block, so none of them is reserved (`stiff`,
  `total_time` can still name a variable). `integration.total_time` is the run's
  duration, 0 or more. Negative Total is accepted only on XPPAUT `.ode` / `.set`
  import and becomes its absolute value; our files and commands reject it
  with its file and line (W347, [#408](https://github.com/MuhammadMoustafa/xppautX/issues/408)).
  A file has one block. The groups
  and their keys are the option table's (core/model_options.cpp); the JSON
  Schema generated from it is
  [odex-options.schema.json](odex-options.schema.json) (`xppautX
  --options-schema` prints it; `tools/optionschema.sh --check` fails when
  the file differs, W281):

        options {
          integration: { total_time: 100, method: stiff, jacobian: exact },
          plot:     { xp: v, yp: w, xlo: -0.6, xhi: 1.2 },
          continuation: { ntst: 50, parmax: 10 },
        }

  Groups: `integration`, `poincare`, `equilibria`, `continuation`,
  `diagram`, `plot`, `export`, `sweep`, `analysis`, `interface`, `output`.
  Integration holds solver and time settings; poincare holds section settings;
  equilibria holds `max_iterations`, `newton_tolerance`, `jacobian_step`, `search_limit` and `neartol`; continuation holds AUTO run settings;
  diagram holds its axes and colours; plot holds view and grid settings; export
  holds `plotfmt` (`pdf` or `svg`), `picture_color` and `picture_font_size`; sweep holds range settings (including `rangefiles: true` or `false`, default `false`, which writes each reset run separately in every mode); analysis holds histograms
  and spectra; interface holds sliders and buttons; output holds files and logging.
  Older group names are errors. A key is the option's one name in lower
  case, spelled exactly (`xp2`..`xp8` for the numbered ones): XPPAUT's word
  (`total`, `nout`, `meth`, ...), a prefix match (`xplot`, `state_limits`), a key
  of another group and a key twice (but for `fold` and `but`, which name one
  thing each) are errors at the key, as is anything the old `@` form wrote.
  The X11 window's settings (`bell`, `bigfont`, ...) are no options: the
  converter drops them. Every option has one name: `total_time`,
  `start_time`, `store_from` (a time, not a duration: `trans` of a `.ode`),
  `nullcline_mesh`, `max_iterations`, `newton_tolerance`, `jacobian_step`,
  `method`, `relative_tolerance`, `min_step`, `max_step`, `absolute_tolerance`,
  `storage_rows`, `torus_period`, `poincare_map`, `poincare_variable`,
  `poincare_sign`, `poincare_stop`, `poincare_plane`, where a `.ode` says
  `total`, `t0`, `trans`, `nmesh`, `newt_iter`, `newt_tol`, `jac_eps`,
  `meth`, `tol`, `dtmin`, `dtmax`, `atol`, `maxstor`, `tor_per`, `poimap`,
  `poivar`, `poisgn`, `poistop`, `poipln`; `store_every` is `nout`/`njmp`.
  A `.ode`'s `bound` is two settings of ours: `state_limit` (a variable past
  it is a blow-up, and `stop_at_limit: true`, the default, ends the run) and
  `search_limit` (the equilibria's search gives up past it), both read from
  it; the DAE solver's own cap is a constant, no setting. A `.ode`'s
  `ncdraw` is `nullclines.draw` and `nullclines.file` (`1`: draw; `2`: not
  drawn and saved to `nullclines.dat`; `nullclines.file` is a file name written
  in the working folder, in the data format its extension names: `.dat`, `.csv`,
  `.csv.gz` or `.npz`, one row a segment: `nullcline` (1 or 2), `x1`, `y1`, `x2`, `y2`). A bad value is an error naming the option and the bad text, at
  its line.

  A value is one bare word or number (letters, digits, `_ . + -`), no
  quotes, except a **path**, a **text** or a **colour**, which is a double-quoted string
  (`logfile: "out/run 1.log"`, `output: "données.dat"`, `xnc: "#ffa500"`; a colour is
  `"foreground"`, a quoted CSS colour name or `"#rrggbb"` (`#rrggbbaa` with alpha, drawn by the page,
  SVG and PDF), and every colour is double quoted, including names; a name is read and written back as hex): `\"` and `\\`
  are its only escapes, a path is written with `/` on every system, and a
  path outside the model's folder is a question the user answers (W175, W286). Only `output` and
  `logfile` are paths, and `but` (a button, `but: "quit:fq"`) a text. Every
  writer of options (the converter, Save values) writes them so.
  Continuation numbers use the same ranges as the AUTO form, protocol and session reader:
  `dsmin`, `dsmax`, `epsl`, `epsu` and `epss` are positive; `dsmin <= dsmax`,
  `dsmin <= abs(ds) <= dsmax`, `parmin < parmax` and `normmin < normmax`.
  A bad value is refused at its line, before the model is installed (W494).
  Slider presets use `interface: { sliders: [ { name: v, low: -1, high: 1, step: 0.01 }, ... ] }`.
  The list may have any length, including zero; each name must be a parameter or state,
  low and high finite with low < high, and step optional (0 chooses an automatic step).
  XPPAUT's S1-S3 slots convert to this list; they are not native option keys.
  Values that were more than one word are one now:

  | `.ode` / old `.odex` value | `.odex` value |
  |---|---|
  | `method=Runge-Kutta` (`rk`, `rk4`) | `runge_kutta` |
  | `Mod. Euler` (`modeuler`) | `modified_euler` |
  | `BackEul` (`Backward Euler`) | `backward_euler` |
  | `DoPri8(3)` | `dopri8` |
  | `DoPri5` | `dopri5` |
  | `QualRK` (`Qual RK`, `qualrk4`) | `qualrk` |
  | `Discrete`, `Euler`, `Adams`, `Gear`, `Volterra`, `Stiff`, `CVode`, `Rosenbrock`, `Symplectic` | the same in lower case |
  | `range=0/1` | `sweep: { rangefiles: false/true }` (W349, [#410](https://github.com/MuhammadMoustafa/xppautX/issues/410)) |
  | `rangereset=y...`, `rangeoldic=n...`, `poistop=1` | `true`, `false` (a yes or no is always `true` or `false`) |
  | `poincare_map=s...`, `m...`, `p...` | `section`, `max`, `period` |
  | `poisgn=-1`, `0`, `1` | `negative`, `both`, `positive` |
  | `colormap=0`..`6` | `jet`, `periodic`, `hot`, `cool`, `blue_red`, `gray`, `cubehelix` (XPPAUT's seven; ours also has `viridis`, `plasma`, `inferno`, `magma`, `cividis`, `coolwarm`, `rdbu`, `seismic`, `twilight`, `hsv`, `turbo`, `greys`, `blues`, `reds`, `greens`, `oranges` and `purples`: 256 colours each, third_party/colormaps/README.md says where each is from; a model of ours without `colormap` has `viridis`, a `.ode` without one is converted with `colormap: jet`, its own) |
  | `xnc`, `ync`, `smc`, `umc` `=0`..`10`; `sec`, `uec`, `spc`, `upc` `=0`, `20`..`29` | the colour in XPPAUT's own table: `"foreground"` for 0, else `"#rrggbb"` |
  | `lt=1`, `0`, `-n` | `plot: { line.draw: line }`, `line.draw: points`, `line.draw: points, line.marker: circle, line.marker_size: n` (the style every new curve starts with: `line.draw` is `line`, `points` or `both`, `line.dash` `solid`, `dashed`, `dotted` or `dash_dot`, `line.width`, `line.marker` `circle`, `square`, `triangle`, `diamond`, `cross`, `plus` or `star`, `line.marker_size`) |
  | `plotfmt=ps`, `svg` | `pdf`, `svg` (PostScript is gone) |
  | `ps_color=0`, `1`; `ps_fsize=n` | `picture_color: false`, `true`; `picture_font_size: n` (points) |
  | `ps_font`, `ps_lw` | none: a picture's font is Helvetica, and each curve has its own width |
  | `axes=3d` | `3` |

  A choice takes exactly one of its words (`runge_kutta`, not `Runge-Kutta`
  or `rk4`). `jacobian` (xppautX's own: XPPAUT has none) is `difference`
  (the default) or `exact`; `exact` in a model with no exact Jacobian stops
  the load at its line, with the reason (W260,
  [#317](https://github.com/MuhammadMoustafa/xppautX/issues/317); the
  manual's "Numerical parameters"). `neartol` (near's tolerance) is
  an equilibria key.
- `set "NAME" { key: value, ..., options: { group: { key: value } } }` — a named set. Its name is the user's text, so it is in double quotes even without spaces (`set "hopf" { ... }`; letters, digits and `_`, starting with a letter); a bare name is an error at its line. Its top-level keys are
  parameters and states only (`g: 9.81`, `v: -0.3`), each a number. Settings go only under `options:`, in the options
  block's shape and from the groups a set may hold: `integration`, `poincare`, `equilibria`, `continuation`, `plot`,
  `sweep` and `analysis` (each a graph's or a run's own configuration; `diagram`, `export`, `interface` and `output`
  are the model's or the session's as a whole, and so are `simplot`, `multiwin` and `colormap`). A setting at the top
  level, an unknown key or group, or a group or key a set does not hold is an error at its line and column, naming it;
  the whole set is checked when the model loads (values included), so a bad one stops the load. A parameter named like a
  setting (`par dt = 2`) is unambiguous: `set "a" { dt: 0.1, options: { integration: { dt: 0.05 } } }` is one parameter
  and one setting. A set's name is its own, apart from the model's names; two sets of one name, an imported file's
  included, are an error naming both places. `--convert` writes XPPAUT's flat set by XPPAUT's precedence (a model name
  first) and nests the rest under `options:`.
- `table NAME EXPR` / `table NAME FILE` — as `.ode`; a formula table
  may call a `fun` written after it (every function is compiled first,
  W165).
- `markov`, `wiener` kept; `event` (`.ode`'s `global`), `boundary`
  (`bdry`), `network` (`special`) renamed, same meaning, `.odex`-spelled
  arguments; no `volt` (an equation calling `volterra(...)` is a Volterra
  equation); `import` takes the declarations of another file ("Imports", below).
- No `done`: the end of the file ends the model (an include file too).
- `#` starts a line comment (as `.ode`). `/* ... */` is a block comment
  (decided, maintainer, 2026-09-27): it may span lines and nest (so a
  region that already holds one can be commented out), and a `/*` never
  closed is an error at the line where it opened.
- A trailing range makes a statement an array (below, "Arrays").

The allocation-sized whole options have inclusive maxima (W296,
[#354](https://github.com/MuhammadMoustafa/xppautX/issues/354)):

| Option | Minimum | Maximum | Reason |
|---|---:|---:|---|
| `numerics.vmaxpts`, `numerics.storage_rows` | 1 | 10000000 | Large histories and initial storage without int-sized requests |
| `numerics.nullcline_mesh` | 1 | 4096 | Bounds mesh work to about 16 million cells |
| `plot.dfgrid` | 2 | 4096 | The direction field has spacing between two or more points; bounds its work like the mesh |
| `plot.nplot` | 1 | 8 | XPPAUT's most curves in one window (`xp2` to `xp8` and the first) |
| `numerics.bandup`, `numerics.bandlo` | 0 | 100000 | Large banded models without int-sized bandwidths |
| `histogram.histbins`, `histogram.histbins2` | 2 | 4096 | About 16 million cells in two dimensions |
| `histogram.specwidth` | 1 | 1000000 | Bounds spectral workspaces |
| `auto.ntst` | 1 | 10000 | Bounds AUTO's mesh-sized matrices |
| `auto.nmax` | 1 | 1000000 | Bounds a requested continuation diagram |
| `auto.ncol` | 1 | 7 | AUTO's settings reader requires supported collocation orders 2 through 7 |

Settings that must keep an order are refused when it is broken, wherever they
come from (an `options` block, a `.setx` or session file, `set num`, the
Numerics menu, the Window and histogram dialogs), with both names and values:
`min_step` at most `max_step`, `xlo` below `xhi`, `ylo` below `yhi`, `xmin` below
`xmax`, `ymin` below `ymax`, `zmin` below `zmax`, `histlo` below `histhi`,
`histlo2` below `histhi2`, `autoxmin` below `autoxmax` and `autoymin` below
`autoymax` (the continuation's pairs are AUTO's own). `torus_period` is above 0 and
`rangestep` at least 1. The list, with each setting's range and the rule behind
it, is [docs/studies/w494-numeric-settings.md](studies/w494-numeric-settings.md).

These maxima also apply through the corresponding saved numerics and AUTO
session settings. Iteration counts, output strides, seeds, flags and indices
remain whole numbers in their existing ranges; they do not size an allocation.
The JSON schemas publish the maxima from the same option rows. Exceeding one
is a load error at the file's offending line, before applying anything.

The shared `.odex`/`.setx` lexer accepts at most 1,048,576 tokens per file,
including the end token: at most 80 MiB of token objects on 64-bit builds,
plus token text bounded by the input's length. A punctuation-only file
exceeding that cap is refused at the next token's file, line and column.
Both `.ode` and `.odex` ranges may expand to at most 100000 elements per
statement; the length is checked before any copies are made. These caps
allow large models while bounding compact hostile input's expansion.

## Settings files (.setx)

A settings file, `name.setx` (W284, [#344](https://github.com/MuhammadMoustafa/xppautX/issues/344);
design in [Discussion #341](https://github.com/MuhammadMoustafa/xppautX/discussions/341)),
holds what XPPAUT's `.set` held (the parameters, the initial values, the
numerics, the delays, the boundary conditions, the active window's graphics),
in the options block's syntax: `key: value` entries separated by commas,
groups `{ ... }` nested, a trailing comma allowed, a bare one-token value
or a value in double quotes (`\"` and `\` the only escapes), `#` and `/* */`
comments. **User text is quoted, a choice never** (W490): whatever the user
names or writes (a set's name, a window's or a curve's name, a key, a label, a
title, a formula, a path) is a string in double quotes even when it is one word,
and the reader refuses it bare (`` `name` takes text in double quotes: write "hopf" ``);
a choice, a number, `true` or `false` and a name of the model's own
(`method: runge_kutta`, `xp: v`) stay bare words, and one in quotes is
refused too (`` `method` takes a word, never in quotes ``). A reference to a
column a user named with more than letters and digits is the one place a
name may be either. The one addition is a list, `[ a, b, c ]`, of values or of groups
(a window's curves). The file is the entries of one unbraced top-level group,
in any order; it is read by the `.odex` reader's own tokenizer and block
grammar (core/odex_parse.cpp), not a second parser, and loaded all or
nothing: every key and value is checked, an error names the file, the line
and the column with the line as written, and nothing is applied before the
whole file has read. A key no group has, a key twice, a group nested deeper
than 8 and a list longer than the reader allows are errors (a `.setx` may be
a file you were sent).

        numerics: { total_time: 100, dt: 0.05, method: stiff, jacobian: exact, },
        parameters: { iapp: 0.09, gca: 1, },
        initial: { v: -0.5, w: 0.1, },
        graphics: {
          curves: [ { x: t, y: v, z: t, color: "#ff0000", draw: line, dash: solid, width: 1, marker: circle, marker_size: 3 } ],
          xlo: 0, xhi: 100,
        },

A file names what it sets: `parameters: { iapp: 0.5 }` alone changes that
parameter and nothing else (a group, or a key in it, that is absent leaves
the session as it is). The groups are `numerics`, `run` (the integrator's
position), `initial`, `ending` (the values the last run ended at),
`parameters`, `delays`, `boundary`, `graphics` (the active window: its
curves by variable name and its rotation, scales and box), `transpose`,
`coupling`, `array_plot`, `torus`, `range`, `equilibrium_range` and
`bvp_range`. The keys of `numerics` and of the table-held part of `range`
are the option table's (core/model_options.cpp, the rows marked `saved`): a
numerics setting is written under the key the Numerics menu and `set num`
use (`total_time`, `dt`, `method`, `jacobian`, `relative_tolerance`, ...), a choice
by its word (`jacobian: exact`, `poincare_map: section`) and a yes or no as `true` or `false` (`rangereset: false`), a bare number never standing for either,
so a setting added to the table is a key of the file with no other change.
The `columns` and `rows` of `array_plot` and `transpose` are whole numbers
from 1 through 4096, limiting their rectangle to about 16 million cells.
The same checks apply in a session's `model.setx` and `windows.setx` and to
an imported XPPAUT `.set`, with the offending member's file and line.

The JSON Schema generated from the file's description and the table is
[setx.schema.json](setx.schema.json) (`xppautX --setx-schema` prints it;
`tools/optionschema.sh --check` fails when it differs).

XPPAUT's `.set` (positional lines, a value and then its label, read by their
order) is import only: File > Import settings and `--setfile` of one read it,
write the `.setx` beside it (`lecar.ode.set` gives `lecar.ode.setx`; the
question before another text replaces a `.setx` that is there is the one
`.ode` to `.odex` asks), and read and apply that `.setx`. The same command
and `--setfile` take a `.setx` as it is. Session files (`.snapx`) keep their
settings in the same syntax, one member each (`model.setx`, `windows.setx`,
`marks.setx`, `sliders.setx`, `nullclines.setx`, the manifest `session.setx`),
and a recording's (`.recx`) header is a `recording` group of it: see
docs/protocol.md "Session files" and "Recordings".

A `.odex` with no state equations can be imported but cannot be opened as
a model to run. Opening it alone reports this at its file in every mode
(W324, [#384](https://github.com/MuhammadMoustafa/xppautX/issues/384)).

## Imports

W282 ([#339](https://github.com/MuhammadMoustafa/xppautX/issues/339)),
replacing `include` (a `.odex` with `include` stops at its line with the
parser's ordinary error). Two forms, each a statement of its own:

    import "f.odex"                 # everything in the file
    from "f.odex" import a, b       # a, b and what they depend on

- The file is read where the statement stands, as part of the model, and
  its name is relative to the importing file's folder: it stays inside the
  model's folder (no absolute name, no `..`, no link: the rules of every
  model file, W175). The extension is the writer's choice but never `.ode`.
- `import "f.odex"` takes every statement of `f.odex`.
  A trailing `as` prefix is a parser error (W315, [#374](https://github.com/MuhammadMoustafa/xppautX/issues/374)).
- `from "f.odex" import a, b` takes the statements that declare `a` and `b`
  (an element `x3` of an array `x` takes the array's statement) and, again
  and again, the statements that declare each name the statements taken read
  (parameters, constants, functions, tables, networks, ...). An `init` or
  `history` of a variable taken, an `event` that sets a name taken and a
  `0 = ...` constraint on an algebraic variable taken come with it, since
  the name does not work without them; a `set`, `only`, `boundary` and a
  comment do not. A name the file does not declare is an error at the
  import.
- A name that is already declared (by the importing file, or another
  import) is an error at the import line, naming the declaration.
- An imported file's options (its `options` block) are not imported:
  the options belong to the model that runs.
- No `export`: every name of a file is importable, and `from ... import`
  lets the importer choose. A file imports files in turn, to a depth of 16
  files; a file that imports itself, directly or through others, is an
  error at the import line, and one model makes at most 256 imports (a
  file imported twice is read twice).
- `from` is a word only in an import (a parameter may be called
  `from`); `import` is a reserved word.

## Arrays

W80 (question 8). One form: a statement followed, on the line it ends
on, by a range, `for INDEX in FIRST..LAST` or `for INDEX in FIRST..LAST
by STEP`:

    const n = 20
    x[1]' = -x[1]
    x[j]' = -x[j] + x[j-1]  for j in 2..n
    aux s[j] = x[j]^2  for j in 1..n by 2
    init x[j] = j/n  for j in 1..n

- The statement is made once for each value of the index, FIRST, FIRST +
  STEP, ... up to LAST, both ends included; STEP (`by`) is a whole number
  above 0, 1 when it is not written. A range that counts down is an error
  (so is `a..b..k`: which number is the step is ambiguous; and `step`,
  a likely parameter name, is not the word).
- FIRST, LAST and STEP are whole numbers worked out at load from numbers
  and the consts before them, with `+ - * / mod ^` and a sign: never a
  parameter (the number of equations is fixed at load; a slider cannot
  resize an array).
- The index is named (`for j in`, `for i in`, ...): an index named like a
  declared name is an error. In each copy it is a number, anywhere in the
  statement: `x[j]' = if j == 1 then 0 else 1`.
- An element `x[e]` is the name `x` followed by e's value (`x[3]` is the
  name `x3`, which the model may also write as `x3`); e is any integer
  expression of numbers, consts and the index, `x[2*j+1]`, whose value
  is 0 or more. Every element a copy names must exist, in both branches
  of an `if` too: the ends of a chain are statements of their own
  (`x[1]' = ...`, then `for j in 2..n`).
- No blocks and no logic: no `for ... { }` and no declaration-time `if`.
- Every statement takes a range but `options`, a comment the model
  shows, `only` and `set`. An element may name what the statement
  declares (`x[j]'`, `x[j] =`, `u[j](t) =`, the names of `par`, `init`,
  `aux`, `history`, `const`, `wiener`, an event's targets, a network and
  a network's arguments, `solv`, `markov`).
- A problem in a copy is reported at its place in the statement, naming
  the copy: `the name `y1` is not declared (where j = 1)`.

## Grammar (EBNF)

```ebnf
model        = { statement } ;
statement    = ( ode_decl | par_decl | const_decl | init_decl | aux_decl
             | fixed_decl | fun_decl | option_decl | set_decl | table_decl
             | markov_decl | wiener_decl | event_decl | boundary_decl
             | network_decl | import_decl ) , [ range ] ;
(* comments, '#' to the end of the line and nested '/* */' blocks, are
   removed by the tokenizer before this grammar *)
range        = "for" , name , "in" , expr , ".." , expr , [ "by" , expr ] ;
(* on the line the statement ends on; for and in are words only here *)
element      = name , [ "[" , expr , "]" ] ;   (* x or x[e] *)

ode_decl     = name , "'" , "=" , expr | name , "[" , expr , "]" , "'" , "=" , expr ;
fixed_decl   = element , "=" , expr ;
par_decl     = "par" , name_init_list ;      (* each value a fixed value *)
const_decl   = "const" , name_init_list ;    (* likewise *)
init_decl    = "init" , name_init_list ;     (* likewise; ran(x) too *)
aux_decl     = "aux" , name_init_list ;
name_init_list = element , "=" , expr , { "," , element , "=" , expr } ;

fun_decl     = "fun" , name , "(" , [ arg_list ] , ")" , "=" , expr
             | "fun" , name , "(" , [ arg_list ] , ")" , block ;
arg_list     = name , { "," , name } ;

block        = "{" , { block_stmt } , "}" ;
block_stmt   = let_stmt | if_stmt | return_stmt ;
let_stmt     = "let" , name , "=" , expr ;
return_stmt  = "return" , expr ;
if_stmt      = "if" , expr , block ,
               { "else" , "if" , expr , block } ,
               [ "else" , block ] ;

option_decl  = "options" , "{" , [ group , { "," , group } , [ "," ] ] , "}" ;
group        = name , ":" , "{" , [ entry , { "," , entry } , [ "," ] ] , "}" ;
entry        = name , ":" , ( bare_word | string ) ;
(* a string for a path or a text, a bare word (one token: letters, digits,
   _ . + -) for any other value; the keys and values are read here only *)

set_decl     = "set" , string , "{" , [ set_entry , { "," , set_entry } , [ "," ] ] , "}" ;
set_entry    = name , ":" , [ "-" ] , ( number | name ) ;
table_decl   = "table" , name , ( expr | filename ) ;
markov_decl  = "markov" , name , integer , { transition_row } ;
wiener_decl  = "wiener" , name , { "," , name } ;
event_decl   = "event" , sign , expr , { "," , event_stmt } ;
boundary_decl = "boundary" , name , "=" , expr ;
network_decl = "network" , name , "=" , network_call ;
import_decl  = "import" , string
             | "from" , string , "import" , name , { "," , name } ;

expr         = or_expr ;
or_expr      = and_expr , { "or" , and_expr } ;
and_expr     = not_expr , { "and" , not_expr } ;
not_expr     = [ "not" ] , cmp_expr ;
cmp_expr     = add_expr , [ cmp_op , add_expr ] ;
cmp_op       = "<" | ">" | "<=" | ">=" | "==" | "!=" ;
add_expr     = mul_expr , { ( "+" | "-" ) , mul_expr } ;
mul_expr     = unary_expr , { ( "*" | "/" | "mod" ) , unary_expr } ;
unary_expr   = [ "-" ] , pow_expr ;
pow_expr     = postfix_expr , [ "^" , unary_expr ] ;  (* right-assoc *)
postfix_expr = primary , { "(" , [ arg_exprs ] , ")" | "[" , expr , "]" } ;
primary      = number | name | "(" , expr , ")" | if_expr | call ;
if_expr      = "if" , expr , "then" , expr ,
               { "else" , "if" , expr , "then" , expr } ,
               "else" , expr ;
call         = name , "(" , [ arg_exprs ] , ")" ;
arg_exprs    = expr , { "," , ( expr | name , "=" , expr ) } ;
(* a built-in's arguments after the first are named: volterra(e, of=u, mu=0.5) *)

number       = digits , [ "." , [ digits ] ] , [ exponent ]
             | "." , digits , [ exponent ] ;
exponent     = ( "e" | "E" ) , [ "+" | "-" ] , digits ;
name         = letter , { letter | digit | "_" } ;   (* case-sensitive, no length limit *)
```

`pow_expr`'s right recursion into `unary_expr` (rather than back into
`pow_expr`) gives `^` its right-associativity while still letting a sign
appear on the right operand: `2^-3` parses as `2^(-3)`, and `-x^2`
parses as `unary_expr = "-" pow_expr` where `pow_expr = x^2`, i.e.
`-(x^2)`.

## Precedence, low to high

1. `or`
2. `and`
3. `not` (unary)
4. comparisons (`<`, `>`, `<=`, `>=`, `==`, `!=`) — non-associative
   (`a < b < c` is a grammar error: `.odex` requires `(a<b) and (b<c)`)
5. `+`, `-` (binary, left-associative)
6. `*`, `/`, `mod` (left-associative)
7. unary `-`
8. `^` (right-associative)
9. postfix (call, index)

This is the usual order every other language uses, unlike `.ode`'s table
(docs/odex-quirks.md's "Operator precedence" section) where comparisons
outrank all arithmetic.

## Reserved words

Statement keywords: `par`, `init`, `aux`, `fun`, `let`, `return`, `if`,
`then`, `else`, `set`, `table`, `markov`, `wiener`, `event`, `boundary`,
`network`, `import`, `const`, `options`; `by` (a range's step). `for` and `in`
are words only where a range starts, after a statement on its line, so
a parameter may be called `in` (W80). `!` and `number` are not `.odex`
(W80: `d = expr`, `const`).

Renamed from `.ode` (decided, maintainer, 2026-09-27): `global` is `event`,
`bdry` is `boundary`, `special` is `network`; `volt` is gone (an equation
whose right-hand side calls `volterra(...)` is a Volterra equation);
`done` and `#done` are gone (the end of the file ends it; `--convert`
moves what followed a `.ode`'s `done` into a closing comment block). `--convert` keeps the
`.ode`'s statement order, and every comment on a line of its own where it
stood (a comment after a statement goes on the line above it; W283,
[#340](https://github.com/MuhammadMoustafa/xppautX/issues/340));
`#include` has no `.odex` form (the converter reads the included text into
the `.odex`); `import` is the `.odex`'s way to share a file.

Kept from `.ode` (decided, maintainer, 2026-09-28): `aux` stays `aux`, a
quantity recorded as an output column (saved, shown, plotted), where
`d = expr` is the same computation not recorded. `watch` was considered
and refused: it reads as a watchpoint that fires or pauses; `output`
was the alternative, but `aux` is what every XPPAUT user, the book, the
examples and the extension already use, and `--convert` keeps it as is.

Logical/operator words: `and`, `or`, `not`, `mod`.

Integral operator: `volterra` (replaces `.ode`'s `int{...}`, `int[mu]{...}`;
decided, maintainer, 2026-09-27). Its first argument is the expression,
and every argument after it is named (`volterra(exp(-x), of=u, mu=0.5)`):
a rule for every built-in that takes options. The exact names are settled
against the 11 Volterra examples before W74.

Built-in functions (from `.ode`'s own reserved set, `constants.ts:31-39`,
checked against `expr_symbols.cpp`'s symbol table): `sin`, `cos`, `tan`,
`asin`, `acos`, `atan`, `atan2`, `sinh`, `cosh`, `tanh`, `exp`, `ln`,
`log`, `log10`, `sqrt`, `heav`, `sign`, `flr`, `ran`, `abs`, `delay`,
`shift`, `ishift`, `del_shft`, `sum`, `of`, `max`, `min`, `normal`,
`besselj`, `bessely`, `besseli`, `erf`, `erfc`, `hom_bcs`.

Built-in constants: `t` (time) and `pi`. `e` is not reserved (decided,
maintainer, 2026-09-27): it stays an ordinary name, as in `.ode`, and
e^x is written `exp(x)`.

### Changes from `.ode`'s reserved words

`.ode`'s reserved names (refused as a name by XPP: the built-in functions,
`not`, `set`, `if`/`then`/`else`, `arg1`..`arg9`, `t`, `pi`) against `.odex`'s:

- New in `.odex` (accepted as names in `.ode` today): the statement
  keywords `par`, `init`, `aux`, `table`, `markov`, `wiener`, `event`,
  `boundary`, `network`; `fun`, `let`, `return`; `and`, `or`;
  `volterra`; `const` and `by` (W80; `for` and `in` only where a range
  starts); `options` (W280) and `import` (W282, replacing `include`;
  `from` only in an import).
- Dropped in `.odex`: `arg1`..`arg9` (a function's arguments are named);
  `global`, `bdry`, `special` (renamed), `volt`, `done`, `number` and
  `!` (gone; W80: `const`, `d = expr`);
  `int` loses its meaning (it was never refused as a name in `.ode`, and
  `volterra` replaces the operator).
- Unchanged: the built-in functions, `not`, `mod`, `set`, `if`/`then`/`else`,
  `t`, `pi`. `e` is reserved in neither.
- Every change to this list is sent to the VS Code extension
  (MuhammadMoustafa/XPP-ODE-Extension#1) when it is made.

Every reserved word is refused as a declared name with a parse error
naming the word, the line, and the column — never the late, misleading
`.ode` errors ("Too many/few equations", "ERROR compiling X'") that
result from a reserved word slipping through as an ordinary token.

## Errors

Every problem `.odex` finds is a compile error: unmatched brackets, an
undeclared name, a chained comparison, a reserved word used as a name, a
`@` option with a non-numeric value, a block function that writes a
global, a block function with a path that does not `return`, and a
`.odex` file with no `.ode`-style silent fallback for any of them. Each
error carries the file, line, and column of the offending token (`.ode`'s
own errors, by contrast, sometimes point at a re-written, upper-cased,
space-stripped copy of the line: see the `Ref:N M` style errors in
docs/odex-quirks.md). Nothing is dropped, cut short, or ignored the way
`.ode` drops an unrecognised line or truncates a numeric `@` option with
`atof`.

## What `--convert` (W74) writes for each quirk

`--convert` reads a `.ode` file with the existing `.ode` parser (so it
sees exactly what xppautX itself understood, quirks included) and
re-emits the same `Model` as `.odex` text. Per quirk (see
docs/odex-quirks.md for the measured `.ode` behaviour each row starts
from):

- `^` chains (`a^b^c`): written with explicit parentheses matching
  `.ode`'s left grouping, `(a^b)^c`, so the `.odex` value (right-
  associative by default) still matches.
- `-x^2`: written as `-(x^2)` is unnecessary (matches `.odex`'s own
  rule by default) but `--convert` writes it anyway when the source
  used extra parentheses of its own, to stay a literal translation.
- A comparison beside arithmetic without parentheses (e.g. `2*3<4`,
  read by `.ode` as `2*(3<4)`): `--convert` writes the parentheses that
  keep the `.ode` value, i.e. `2*(3<4)`, not the naive `(2*3)<4` a
  `.odex` reader would otherwise assume.
- A chained comparison (`3<2<1`, read by `.ode` as `(3<2)<1`):
  `--convert` writes `(3<2)<1` explicitly, since `.odex` has no
  chaining of its own.
- `&`/`|`: mapped to `and`/`or` exactly (`.ode`'s `&` is priority 6, its
  own `and` is lower than `.odex`'s comparisons, so `--convert` adds
  parentheses around a comparison beside `&`/`|` only when needed to
  preserve the original grouping, following the same left-to-right
  reading `.ode` used).
- `not(x)`: mapped to `not x`.
- `!=`: since `.ode`'s `!=` never actually loads (docs/odex-quirks.md),
  there is no `.ode` file using it to convert; `--convert` maps
  `.odex`'s own `!=` back to nothing special (used only going forward).
- `if(c)then(a)else(b)`, with or without a trailing operator: written in
  the one-line form with the parentheses the `.ode` value needs, e.g.
  `if(1>0)then(10)else(20)+5` (whose `.ode` value applies `+5` to the
  whole `if`) becomes `(if 1>0 then 10 else 20)+5` in `.odex`, keeping
  the grouping explicit rather than relying on `.odex` giving `if`
  the same "whole expression" precedence by convention.
- Nested `if`/`else`-`if`: written with `.odex`'s `else if`.
- A unary sign the `.ode` file was forced to bracket (`2*(-3)`, since
  `2*-3` does not load): `--convert` may drop the now-unnecessary
  parentheses, since `.odex` allows a bare sign there, or keep them —
  an open question below.
- `@` options with spaces around `=` or an `atof`-truncated value:
  `--convert` writes the value the `.ode` file's own truncation
  produced (e.g. `@ total=2*3` becomes `@ total=2`, matching the value
  `.ode` actually ran with), not the source text, since `--convert`'s
  job is to preserve the `.ode`'s *meaning*, not its spelling. An
  ignored option (spaces around `=`) is dropped entirely (the default
  applies), also matching what `.ode` actually did.
- A long name, or two names differing only by case:
  passed through unchanged (`.odex` has no length limit, and is case-
  sensitive, so both are legal there without any rewrite) — but
  `--convert` reports a case-collision (two `.ode` names that folded to
  one) rather than silently keeping only one.
- A function argument hiding a global name: passed through unchanged
  (same lexical-scoping meaning in both formats).

## Questions the maintainer decided

1. **Division by zero and NaN** (decided, maintainer, 2026-09-27):
   `.odex` gives real IEEE results, `1/0` is inf and `0/0` is NaN, and a
   run stops with an error naming the equation and the time at the step
   where a NaN or inf first enters the state. `.ode` keeps XPP's guard
   (a zero divisor replaced by 2.23e-15, expr_program.h, `ZERO_DIVISOR`: `1/0` is
   4.5e14, `0/0` is 0), so old models give XPP's numbers; `--check`
   (W75) warns where a `.ode` formula divides by something that can be 0.
   One flag per model, chosen by the file's extension; the evaluator is
   shared. A NaN condition in `.odex` is therefore an error, never a branch.
2. **`--convert`'s bracket policy** (decided, maintainer, 2026-09-27):
   parentheses `.ode` needed only for its no-bare-sign rule (`2*(-3)`,
   `x<(-1)`) are kept, so a converted file differs from its `.ode` only
   where the meaning requires it.
3. **Spellings and renames** (decided, maintainer, 2026-09-27):
   - A name spelled two ways in a `.ode` (`par V=1`, then `v*2`: one name,
     since `.ode` folds case) is written with its declaration's spelling
     everywhere; `--convert` lists the lines it rewrote. Two declarations
     that differ only by case cannot occur: such a `.ode` does not load
     (measured: `par V=1` and `par v=2`: 'V is a name already, or one
     parameter too many').
   - A `.ode` name that `.odex` makes a keyword (`and`, `or`, `fun`, `let`,
     `return`) must be renamed: `--convert` lists each one and asks for its
     new name, one by one, with a suggestion ready (the name plus `_`, then
     a number if taken); a dialog in the program, a prompt on the command
     line. `--convert --yes` accepts every suggestion without asking. With
     no one to answer and no `--yes`, it stops and names what needs a new
     name. No `--rename` switch.
   - The converted file starts with a comment listing every rename.
4. **`e`** (decided, maintainer, 2026-09-27): not reserved in `.odex` (see
   Reserved words), so a `.ode` using `e` as a name converts unchanged.
5. **`int`** (decided, maintainer, 2026-09-27): `volterra(...)` replaces the
   integral operator, so `int` is an ordinary name in `.odex`.
6. **Extension gap**: moot, since `.odex` does not reserve `e` (question 4).
7. **Approximate equality** (decided, maintainer, 2026-09-28; W78): `==`
   and `!=` stay exact. `near(a, b)` is true when |a-b| <= tol ·
   max(1, |a|, |b|), tol being `@ neartol=` (default 1e-9) or
   `near(a, b, tol=...)` for one call. Not `~=` (MATLAB, Lua and Octave's
   "not equal"), not `eps` (a common parameter name) and not `atol` (the
   integrator's tolerance: the solver must never change the model's
   logic). `near` is reserved; `neartol` is an option name, not reserved.
8. **Arrays** (decided, maintainer, 2026-09-28; W80): a trailing range on
   each statement, `x[j]' = -x[j] + x[j-1]  for j in 1..n by 2`; the index
   named explicitly (no implicit `[j]`), an index named like a declared
   name an error; `a..b` includes both ends; `by k` a positive integer
   step (not `a..b..k`, ambiguous, nor `step`, a likely parameter name);
   bounds and step integer expressions of literals and `const`s, never
   `par`; any integer expression as an index. No blocks and no logic:
   boundaries are separate ranges or the formula's own `if`. `--convert`
   writes arrays back as arrays.
9. **Constants and derived quantities** (decided, maintainer, 2026-09-28;
   W80): `const n = 20` is fixed at load (.ode's `number`). There is no
   `!`: `d = expr` always means the
   formula's current value, and when it reads only parameters, consts
   and pure functions (not `ran`, `normal`, `poisson` or a function
   calling one) the loader computes it only when a parameter changes (a
   slider, an event setting a parameter, each AUTO evaluation), as
   .ode's `!` did, so the numbers are identical. `--convert` writes `!d =
   expr` as `d = expr` and refuses one reading t, a variable or a random
   function. No per-run snapshot (`hold`): that was a side effect of
   .ode's `!`, not a feature.
10. **One Model builder** (decided, maintainer, 2026-09-28; W79): the .ode
    and .odex readers each produce the statement list, one builder makes
    the Model; W74's translation into .ode lines goes.

## The implementation (W74, W79)

Two readers and one builder (question 10). `core/odex.h` holds the
statement list both formats are read into. The `.ode` reader,
`core/ode_read.cpp`, turns an `.ode` file's lines into statements with
every quirk of its reading kept (a backslash joining lines, `#include`,
arrays and for loops, a command told apart by its first letters,
`name=value` items read with atof, a global's events split at every `=`
and `;`, `x(0)=formula` an init of the number at the formula's front and
the formula as the history); its formulas stay `.ode` text, which the
expression engine compiles as written, `.ode`'s precedence and all. The
`.odex` reader, `odex_parse.cpp` (tokenizer and grammar) and
`odex_load.cpp`, checks a model (every name declared once, read where it
may be, called with its arguments) and readies its statements: a block
function becomes the one if/then/else expression its returns make,
`near()` gets its tol, and the model's divisions are IEEE's
(`Parsed::ieee_division`, which only this reader sets). The builder,
`form_ode.cpp`'s `build_model`, makes the Model from either list: an
`.odex` formula is written as the expression engine's text
(`odex_print.cpp`'s `engine_text`, every sign and `if` bracketed and the
parentheses the engine's precedence needs, so no `.ode` quirk is
reached), a parameter's value evaluated in order, an `.odex` initial
value once the model is set up (an `.ode`'s numbers at once), an
event's actions taken one by one (so `==` in one needs no rewriting).
`odex_convert.cpp` is `--convert`. The integrator already stops a run at
a NaN in the state (always) and at |x| > `state_limit` (an inf, unless `stop_at_limit: false`), naming the
variable and the time.

### Where this spec was open: the smallest reading taken

- **Statements `.ode` has and this spec does not list**, kept with the
  smallest spelling: a fixed variable `w = expr` (a named quantity
  computed before the right-hand sides; an `aux` may repeat its name,
  `aux ica = ica`, as `.ode` allows); a Volterra equation `u(t) = expr`
  (with or without `volterra(...)`: `.ode`'s `volt u=...` has none in
  fp.ode, and `junk = volterra(0, of=x)` is a fixed variable, fr.ode); a
  quantity worked out from parameters is `d = expr` (W80; `.ode`'s `!d =
  expr`), and `.ode`'s `number n=100` is `const n = 100`; the
  algebraic `solv y = guess` and `0 = expr`; `only x, y`; a comment the
  model shows is a string statement, `"{gk=0} text"` with its action.
  New reserved words: `solv`, `only`, `history`; the built-ins the spec's
  list lacks are reserved too (`lgamma`, `poisson`, `besselis`, the
  animator's `mouse_x` ... `mouse_vy`).
- **`history x = expr`** (new): x's values before the start, what a delay
  reads (`.ode`'s `x(0)=formula` keeps its formula for exactly that; its
  `init x=1` leaves the history at 0). `init` gives the initial value
  only, evaluated once the model is set up, where `.ode` evaluates its
  array initial values, so a random initial value draws in XPP's order.
- **The spec's other open forms:** `table w "w.tab"` and `table w
  expr, n=51, lo=-25, hi=25` (t the table's variable); `set name = a = 1,
  xp = v` (each value a number or a name); `network k = conv(even, 51,
  12, w, u0)` (arguments as written: names, numbers, `p{1-4}`);
  `boundary expr` (0 = expr, `x'` the value at the right end: the
  grammar's `boundary name = expr` has no meaning to give the name, so
  it is not implemented); `sum(expr, from=lo, to=hi)` with `i'` the
  index, as `volterra(kernel, of=u, mu=m)` names its options (`t'` the
  variable of integration in the kernel); `event 1 x - 1, x = 0` sets a
  variable, an aux quantity, a parameter, `out_put`, `arret` or
  `no_interp`.
- A list statement ends with its line: `par a=1 b=2` is an error (the
  missing comma), never a second statement `b=2`; a range may follow it
  on that line. A parameter's value is a number (W80).
- Not in `.odex`: `.ode`'s `%[a..b]` blocks (a block's lines take turns,
  copy k of each, then copy k+1; `.odex` has a range per statement and no
  blocks), `vector`, `group` and `options`.
- Until W76 and a core that keeps a name's case: names differing only by
  case, and a name that reads as a built-in in upper case (`Sin`), are
  refused with an error saying so. A name has no length limit (W76).

### What --convert writes

`xppautX --convert [--yes] model.ode` writes model.odex beside it:
the statements in the order the reader read them (Model::statements),
each formula read back from the program it compiled to. An array's
copies (the .ode reader marks each, `Statement::array`) are written back
as one statement with its range (W80): what differs between the copies'
own text is the index (a number equal to it, `j`) or an element (a name
followed by `a*j+b`, `x[j-1]`), found from the copies themselves, so the
`.odex` expands to the same programs. The copies stay written one by
one where that cannot be said (a number the index works out to, `[j+1]`
alone, which `.odex` would compute rather than read; a range that counts
down), for a `%[a..b]` block of several lines (separate ranges would
reorder the variables, the output's columns: `cgl.ode`) and for an `@`
line's `x[0..99]` (an `@` line takes no range). `number n=5` is `const
n = 5`, and `!d = expr` is `d = expr`, which the `.odex`'s builder works
out only when parameters change, as the `.ode`'s did: one that reads t,
a variable, a random function or a derived quantity after it is
refused, since `d = expr` would then be current at every evaluation. A quirk of `.ode`'s precedence becomes explicit parentheses; a
number `atof` cut, an option spaced around its `=` (dropped) or cut, a
formula in `x(0)=` (its value, and its history in a delay model) are
written as XPP read them, with a comment. A division by something that
can be 0 is written as `a/ode_divisor(b)`, with an ordinary generated
function `fun ode_divisor(value) = if value then value else 2.23e-15`.
Its argument evaluates once; the name gets underscores if already taken.
Nonzero constants, positive forms (`1+exp(-x)`, `x*x+1`), and products
of a sum's integer index when its literal lower bound is at least 1 are
left plain. Parameters remain guarded because they can change. Standalone
`exp(x)` remains guarded because it can underflow to zero; `exp` of a
proven nonnegative argument divides plainly. An outer sum
with a parameter upper bound remains guarded because it can be empty
(W165, [#217](https://github.com/MuhammadMoustafa/xppautX/issues/217)). Names are written as their
declarations spell them (the header lists the `.ode` lines that spelled
one another way); a name `.odex` reserves is renamed as question 3 says
(a question on the terminal, `--yes` for the suggestions, none asked
with no terminal: the conversion stops and names them). What followed
`done` becomes comments at the end. The `.odex` is then loaded and must
compile to the `.ode`'s own programs (IEEE divisions normalized to
`.ode` divisions and calls to the verified generated guard removed), or `--convert` fails: exact by construction,
and checked. `tools/odexcheck.sh` (verify.sh) converts every example
that loads by itself and runs the `.odex`: every output.dat's md5 is its
`.ode`'s (tests/examples.md5), and an example with arrays must have
each of its array lines written as one statement with its range.

### Arrays, consts and derived quantities in the code (W80)

The `.ode` reader expands an `.ode`'s arrays as it always did (a line's
text rewritten for each index, `[j+1]`, `[j*2]` and a bare `[3]` its
quirks) and marks each copy; the `.odex` reader (`odex_load.cpp`) works
out each const's value in order, then each range and makes the copies,
every element `x[e]` the name it is, and checks them as any statement.
The builder (`form_ode.cpp`'s `find_derived`), for an `.odex` model
(`Parsed::derived`), makes a fixed quantity that reads only parameters,
consts, pure functions and the derived quantities before it a derived
one (one an aux records under its name, one `delay`, `shift` or a
Volterra kernel needs as a variable, and one a network or vector may
read stay variables). An `.ode` follows XPPAUT (maintainer, 2026-09-29):
its fixed quantities stay fixed and only its `!` quantities are derived;
the builder marks those that read anything else (which --convert
refuses). --convert's check builds the `.ode` as the `.odex` will be
(`OdeAsOdex`), so that the two compile alike; the numbers are the same
either way. The derived quantities are the Model's
(`Model::derived`), so a model loaded after another starts without the
one before's.

Native `findext` networks name the extremum: `findext(minimum, 50, 1, d0)`,
`findext(maximum, 50, 1, d0)` or `findext(both, 50, 1, d0)`. XPPAUT's -1,
1 and 0 are read on `.ode` import only; conversion writes the name (W499).
Native AUTO session diagrams likewise write calculation and curve names
(`equilibrium`, `periodic`, `equilibrium_fold`, `periodic_fold`, `hopf`,
`neutral_saddle`, `torus`, `branch_point`, `period_doubling`, `fixed_period`,
`boundary_value`, `discrete`, `homoclinic`, or `none`), rather than XPPAUT codes.
