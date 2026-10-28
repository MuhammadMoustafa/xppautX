# Creating animations

An animation describes a model's motion with shapes whose coordinates and
colours are formulas of time, variables, fixed quantities and parameters.
Save it as an `.anix` file beside its `.odex` model. Both use the
[model grammar](../odex.md): comments start with `#` or use `/* ... */`,
formulas use ordinary precedence, and strings use double quotes.

Run the model, open **View axes → Toon** (the Animation tab), choose
**Load**, and open the `.anix`. **Go / Pause**, **Reset**, the seek slider
and frame step buttons control playback. **Skip** selects rows per step;
**Speed** selects milliseconds per frame. **Fly** follows an integration.
The view fits the animation's bounds to its available space. GIF and frame
exports use the same frame data as the view.

## A pendulum

`pend.anix`:

```text
model: "pend.odex"
options { speed: 10, bounds: { x: [0, 1], y: [0, 1] } }
line { x1: .5, y1: .5, x2: .5+.4*sin(x), y2: .5-.4*cos(x), color: "black", width: 3 }
circle { x: .5+.4*sin(x), y: .5-.4*cos(x), radius: .05, fill: "red" }
value { x: .1, y: .9, label: "t = ", value: t, size: 2, font: roman, color: "black" }
```

Opening this file opens its named model and the animation together. A model
can also name its animation in `options { animation: "pend.anix" }`: opening the model
loads it into a hidden Animation window, which Windows (or `window.show`) brings up.
Names in either link must be base names, with no folder or `..`.
The model is not copied into `.anix` and has no fingerprint there. Every
formula is checked against the loaded model, including the case of names.
A missing name, unknown field, wrong type or missing required coordinate
refuses the whole animation with its file and line. Saving a session as
`.snapx` bundles the open animation beside the model; it opens from that
saved member, without reading another copy from disk.

## Shapes and fields

Each shape is a statement with `key: value` fields inside braces. Parentheses
are function calls. Coordinates and radii are expressions. User text and
fixed colours are quoted; choices such as `solid`, `roman` and `true` are bare.

| Statement | Required fields | Other fields |
|---|---|---|
| `line` | `x1`, `y1`, `x2`, `y2` | `relative: true` instead uses just `x1`, `y1` as offsets from the last line's end |
| `rectangle` | `x1`, `y1`, `x2`, `y2` | `fill` selects a filled shape |
| `circle` | `x`, `y`, `radius` | `fill` selects a filled shape |
| `ellipse` | `x`, `y`, `radius_x`, `radius_y` | `fill` selects a filled shape |
| `trail` | `x`, `y`, `length` | `draw: line` or `points`; points take `radius_pixels` |
| `text` | `x`, `y`, `text: "..."` | `size`, `font`, `color` |
| `value` | `x`, `y`, `label: "..."`, `value` | `size`, `font`, `color` |
| `nullcline` | `variable: "..."`, `x1`, `y1`, `x2`, `y2` | `source: current`, or `source: frozen` with `frame` |

Drawing shapes take `color`, `width` in pixels, and `dash` (`solid`,
`dashed`, `dotted`, `dash_dot`), the plot's style names. A colour is a
quoted CSS colour, `"foreground"`, `"#rrggbb"` or `"#rrggbbaa"`, or an
expression giving a value on the session's colour scale. A filled shape
uses `fill` for that colour instead of `color`. Width defaults to a thin
line (0 means one pixel). Text defaults to size 0, font `roman`, and
`"foreground"`; sizes are 0 through 4 and fonts are `roman` and `symbol`.
Each text owns its style. A trail retains 1 through 10000 points; points
default to a radius of one pixel.

A nullcline uses a variable on the current phase plane's axes and the
nullclines already computed there. Its coordinates give that phase plane's
box. `current` uses the current nullcline; `frozen` selects the saved range
member given by the `frame` formula.

Coordinates are reevaluated each frame. Group shapes whose coordinates
should be evaluated once in `permanent { ... }`. These blocks cannot nest.
The optional `options` statement sets `speed` (0 through 1000 ms, default
10) and `bounds` (default `[0,1]` on each axis). Both pairs must increase.

## Arrays

Use the model's trailing range syntax, including its optional step:

```text
line { x1: j/20, y1: u[j], x2: (j+1)/20, y2: u[j+1], color: "blue" } for j in 1..19 by 2
```

The endpoints and step are fixed values of numbers and model constants.
The index must not name a model quantity. Array references are resolved
and checked before loading. The shared model/animation range limit is
100000; a whole animation may expand to at most that many drawing commands.
Expanded formulas, compiled commands and trail histories also use the session
format's 512 MiB memory budget.

## Dragging a point

**Grab** arms dragging. A `grab` statement supplies a position, tolerance
(default .02), and assignments to execute when dragging starts and ends:

```text
grab { x: v, y: w, tolerance: .05,
       start: { iapp: 0 }, end: { iapp: mouse_x }, run: false }
```

Assignment targets must be model variables or parameters. The mouse's
position and velocity are `mouse_x`, `mouse_y`, `mouse_vx`, `mouse_vy`.
`run: true` integrates after releasing the point. Each animation has at
most 50 grabs and each task at most 20 assignments.

## Importing XPPAUT animations

Opening an XPPAUT `.ani` imports it once, writes `.anix` beside it, and
opens the native file. Existing different output requires an overwrite
answer, as model conversion does. The command line can convert explicitly:

```sh
./xppautX --convert pend.ani
./xppautX --convert model.odex --anifile drawing.ani
```

The first form uses `pend.odex` beside the animation; the second names its
model explicitly. Conversion reads XPPAUT's abbreviations, semicolon
arguments, `$` colours, array commands, `settext` and grab task lines.
It writes whole shape names and explicit text styles; filled shapes,
relative lines and comet dots become fields. Import uses the same 100000
expansion limit as native ranges. The retained `.ani` files under
`tests/animations/` exercise this import; the examples use `.anix`.
