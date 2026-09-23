/* The protocol's messages as types (docs/protocol.md). Only the fields the
   new front end reads are typed; everything else passes through untouched. */

export interface HelloEvent {
  ev: 'hello';
  protocol: number;
  features?: string[];
  title: string;
  file: string;
  menus: {
    main: string[]; main_keys: string; main_hints: string[];
    file: string[]; file_keys: string; file_hints: string[];
    num: string[]; num_keys: string; num_hints: string[];
  };
  lists: string[][];
  userbuttons: string[];
  sliders: {name: string; lo: number; hi: number}[];
}

export interface View {
  win: number;
  left: number; right: number; top: number; bottom: number;
  xlo: number; xhi: number; ylo: number; yhi: number;
  three: number;
  /** the active window's 3D angles, degrees (sent only when `three`) */
  theta?: number;
  phi?: number;
}

export interface StateEvent {
  ev: 'state';
  pars: [string, number][];
  ics: [string, number][];
  bcs: [string, string][];
  delays?: [string, string][];
  view: View;
  rows: number;
  menu: number;
  win: number;
  session?: {set: string; auto?: string};
}

/** One curve of the active plot window: storage columns (0 is T) and XPP's style. */
export interface Curve {
  x: number;
  y: number;
  z: number;
  /** XPP colour index: 0 the foreground, 1..10 red .. purple */
  color: number;
  /** > 0 a line, <= 0 points of radius -line */
  line: number;
}

/** a column's values: JSON numbers (null for NaN), or with enc "f32" base64 of little-endian float32 */
export type SeriesData = (number | null)[] | string;

export interface SeriesColumn {
  col: number;
  name: string;
  data: SeriesData;
}

/** the whole series of one plot window */
export interface SeriesEvent {
  ev: 'series';
  op?: undefined;
  enc?: 'f32';
  win: number;
  rows: number;
  three: number;
  xlabel: string;
  ylabel: string;
  zlabel: string;
  curves: Curve[];
  /** row shifts of the x, y and z columns (lag plots) */
  shift: [number, number, number];
  columns: SeriesColumn[];
}

/** rows an integration stored since what the client holds: it keeps rows
    0..from-1, then these (the columns of the last full series, in its order) */
export interface SeriesAppendEvent {
  ev: 'series';
  op: 'append';
  enc?: 'f32';
  win: number;
  from: number;
  /** rows after this append */
  rows: number;
  columns: {col: number; data: SeriesData}[];
}

/** one plot window as `plots` describes it */
export interface PlotWindowInfo {
  win: number;
  /** "W vs V" */
  title: string;
  three: number;
  /** the window's axes (Viewaxes, Window/Zoom); in 3D the projected view's */
  xlo: number; xhi: number; ylo: number; yhi: number;
  xlabel: string; ylabel: string; zlabel: string;
  /** the 3D box: the data ranges of x, y and z */
  box: {xmin: number; xmax: number; ymin: number; ymax: number; zmin: number; zmax: number};
  /** the 3D view's angles, degrees */
  theta: number;
  phi: number;
  persp: number;
  zplane: number;
  zview: number;
  curves: Curve[];
  shift: [number, number, number];
}

/** every plot window and the active one */
export interface PlotsEvent {
  ev: 'plots';
  active: number;
  windows: PlotWindowInfo[];
}

/** a plot window's nullclines (docs/protocol.md "The plot as data"): segments
    [x1,y1,x2,y2,...] in plot coordinates, empty when the window shows none */
export interface NullclinesEvent {
  ev: 'nullclines';
  win: number;
  enc?: 'f32';
  /** the variables whose derivative is 0 along the x- and y-nullcline */
  xname: string;
  yname: string;
  /** XPP colour indices, as a curve's */
  xcolor: number;
  ycolor: number;
  x: SeriesData;
  y: SeriesData;
  /** Nullcline/Freeze: earlier sets, drawn in the same colours */
  frozen: {x: SeriesData; y: SeriesData}[];
}

/** a plot window's direction field and Flow trajectories */
export interface DfieldEvent {
  ev: 'dfield';
  win: number;
  enc?: 'f32';
  /** 1: arrows of one length (Scaled Dir.Fld); 0: lengths follow the speed (Direct field) */
  scaled: number;
  color: number;
  /** grid points a side; 0 when the window shows no field */
  n: number;
  /** the grid's spacing in plot units */
  du: number;
  dv: number;
  /** x, y, ux, uy per arrow: the unit direction in plot coordinates */
  grid: SeriesData;
  /** one per arrow, plot units per unit time */
  speed: SeriesData;
  /** one per curve of the window: its trajectories, NaN (null) between two */
  flows: {color: number; x: SeriesData; y: SeriesData}[];
}

/** what a plot window shows on top of its curves (docs/protocol.md "The plot
    as data", `marks`): the equilibria Sing pts marked, Text,etc's labels,
    arrows and markers, Graphic stuff/Freeze's frozen curves, in plot
    coordinates; each list empty when the window shows none */
export interface MarksEvent {
  ev: 'marks';
  win: number;
  enc?: 'f32';
  /** the symbol XPP draws for the stability: stable circle, unstable box, saddle triangle */
  equilibria: {x: number; y: number; type: string; symbol: string}[];
  /** XPP's text: backslash escapes switch the font (\1 symbol: Greek, \0 roman), \s \S \n
      subscript, superscript, normal; size 0-4 */
  text: {x: number; y: number; text: string; size: number; font: number}[];
  /** from (x1, y1), where the head's tip is, towards (x2, y2); a pointer has a shaft, an arrow only the head */
  arrows: {kind: 'arrow' | 'pointer'; x1: number; y1: number; x2: number; y2: number; size: number; color: number}[];
  markers: {x: number; y: number; shape: string; size: number; color: number}[];
  /** line 0: drawn as points */
  frozen: {key: string; name: string; color: number; line: number; x: SeriesData; y: SeriesData}[];
}

export interface AskEvent {
  ev: 'ask';
  id: number;
  kind: 'menu' | 'choice' | 'string' | 'form' | 'checklist' | 'file' | 'alert' | 'mouse' | 'rubber' | 'grab'
    | 'drag' | 'pixels';
  title?: string;
  name?: string;
  value?: string;
  items?: string[];
  keys?: string;
  hints?: string[];
  question?: string;
  choices?: string[];
  names?: string[];
  values?: string[];
  message?: string;
  button?: string;
  /** a `file` ask: whether the command opens the file or saves one */
  mode?: 'read' | 'write';
  /** a `file` ask: the name offered (possibly a path), the pattern, and the
      core's folder listing (the "In the model's folder" tab) */
  file?: string;
  wild?: string;
  dir?: string;
  dirs?: string[];
  files?: string[];
  [k: string]: unknown;
}

export interface MessageEvent {
  ev: 'message';
  error?: string;
  bottom?: string;
  box?: string;
  xy?: string;
  auto?: string;
  calc?: string;
}

/** the data browser block the client asked for (docs/protocol.md `browser`,
    docs/ui-v2.md T10): rows `from`..`from+data.length-1` of columns
    `col`..`col+data[i].length-2` (each row is [T, that column, ...]); sent
    for a `browser` `from` request and again, with the same range, after any
    command that changed the data while the client showed the browser. */
export interface BrowserEvent {
  ev: 'browser';
  /** rows stored (my_browser.maxrow); 0 before any integration */
  rows: number;
  /** column names, T first, in storage order */
  cols: string[];
  /** the core's selected row (the X11 browser's top row): what Get, First, Last act on */
  row0: number;
  /** the First..Last range Write and Restore use */
  start: number;
  end: number;
  /** where this block starts: row `from`, column `col` (1-based, T is column 0 but always included) */
  from: number;
  col: number;
  /** data[i][0] is T of row from+i; data[i][k] for k>=1 is column col+k-1; null is NaN */
  data: (number | null)[][];
}

/** File/Prt src (docs/ui-v2.md T16): the model's source, one array entry per
    line, and the comments with a `{par=value,...}` action (`aflag` > 0 in
    core/ui_json.c j_make_txtview): [text, hasAction]. `text` already has
    core's own "* " marker prepended for an action comment (X11's own
    convention); the `{...}` block itself is not sent, only what follows it. */
export interface SourceEvent {
  ev: 'source';
  lines: string[];
  comments: [string, number][];
}

/** Text,etc/Eqns: one `dX/dT=...` line per equation (docs/protocol.md `equations`). */
export interface EquationsEvent {
  ev: 'equations';
  lines: string[];
}

/** Sing pts result (docs/protocol.md `equilibrium`, core/ui_json.c j_show_eq_box):
    `type` is "STABLE", "UNSTABLE" or "NEUTRAL" (core/xpp_util.c eq_stability);
    cplus/cminus/rplus/rminus/im are eigenvalue counts (complex/real with
    positive/negative real part, purely imaginary); `eigenvalues` the
    Jacobian's eigenvalues as [re, im] pairs, absent for a delay equation. */
export interface EquilibriumEvent {
  ev: 'equilibrium';
  type: string;
  cplus: number;
  cminus: number;
  rplus: number;
  rminus: number;
  im: number;
  values: [string, number][];
  eigenvalues?: [number, number][];
}

/** the array plot (docs/ui-v2.md T12, docs/protocol.md `aplot`, window 105):
    `ny` rows of `nx` cells, row-major like `cells` (the core's own colour
    indices, kept for the classic page). `values` is the same cells' stored
    numbers before that mapping (float32, null/NaN off the stored rows or
    columns), so this view can pick its own colour scale from them and
    zmin/zmax; `enc` "f32" (the client's last `data` `enc`) sends it as
    base64 like a series column. */
export interface AplotEvent {
  ev: 'aplot';
  title: string;
  nx: number;
  ny: number;
  cells: number[];
  values: SeriesData;
  enc?: 'f32';
  first: number;
  ncolors: number;
  zmin: number;
  zmax: number;
  tlo: number;
  thi: number;
  tag?: string;
}

/** the animation window's state (docs/protocol.md `ani`), for its slider and
    toggles: sent with every frame drawn and after every `ani` command */
export interface AniStateEvent {
  ev: 'ani';
  op?: undefined;
  /** the row the next step starts from (the core's vcr.pos) */
  pos: number;
  rows: number;
  fly: number;
  grab: number;
  skip: number;
  /** ms between two frames of Go */
  speed: number;
  /** an animation file is loaded */
  loaded?: number;
  /** the animation window exists */
  open?: number;
}

/** a primitive's colour: an XPP colour index (0 the foreground, 1..10 red ..
    purple), or a colour of the colour map as #rrggbb */
export type AniColor = number | string;

/** a frame's primitive as sent (docs/protocol.md "The animation as data"):
    unit coordinates u, v of the dimension box (y up), not clamped; widths
    and a dot's radius in pixels */
export type AniPrimWire =
  | ['line', number, number, number, number, AniColor, number]
  | ['rect' | 'circle' | 'ellipse', number, number, number, number, AniColor, number, number]
  | ['dot', number, number, number, AniColor]
  | ['text', number, number, string, AniColor, number, number];

/** one frame of the animation, for a client that asked (`data` with `ani`) */
export interface AniFrameEvent {
  ev: 'ani';
  op: 'frame';
  /** the stored row the frame shows, of `rows` */
  pos: number;
  rows: number;
  /** the frame's time; null when not finite */
  t: number | null;
  speed: number;
  skip: number;
  /** the dimension box: xlo, ylo, xhi, yhi */
  dim: [number, number, number, number];
  /** the classic window's size in pixels (what widths and dots are relative to) */
  w: number;
  h: number;
  /** numbers are null where the .ani evaluated to NaN */
  prims: AniPrimWire[];
}

export type XppEvent =
  | HelloEvent
  | StateEvent
  | SeriesEvent
  | SeriesAppendEvent
  | PlotsEvent
  | NullclinesEvent
  | DfieldEvent
  | MarksEvent
  | AskEvent
  | MessageEvent
  | BrowserEvent
  | SourceEvent
  | EquationsEvent
  | EquilibriumEvent
  | AplotEvent
  | AniStateEvent
  | AniFrameEvent
  | {ev: 'idle'}
  | {ev: 'progress'; n: number; of: number}
  | {ev: 'title'; text: string}
  | {ev: 'menu'; which: number}
  | {ev: 'window'; op: 'create' | 'select' | 'destroy'; win: number; w: number; h: number; title?: string}
  | {ev: 'log'; text: string}
  | {ev: 'exit'; code: number}
  | {ev: 'bye'}
  /* every other event: drawing ops, AUTO, ... (not used by this UI yet) */
  | {ev: 'draw' | 'palette' | 'diagram' | 'film' | 'ping'; [k: string]: unknown}
  /* AUTO's info strip and stability circle (store/diagram.ts AutoInfoEvent) */
  | {ev: 'autoinfo'; [k: string]: unknown};

export type Command = {cmd: string; [k: string]: unknown};
