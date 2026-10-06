/* The protocol's messages as types (docs/protocol.md). Only the fields the
   new front end reads are typed; everything else passes through untouched. */

import type {CommandKind, LayerWindow, WindowLayer} from './kinds';

/** the protocol this page speaks (core/ui_json.h JSON_UI_PROTOCOL): core and
    page ship together, so a hello with another is a shown error (session.ts) */
export const PROTOCOL = 3;

/** a main-window menu's name (hello.menus.names, by state's menu number) */
export type MenuName = 'main' | 'file' | 'num';

export interface HelloEvent {
  ev: 'hello';
  protocol: number;
  /** the data events the server sends, asked for at once (session.ts) */
  features: string[];
  title: string;
  file: string;
  output_names: {par: string; ic: string; csv: string; curves: string};
  steady: {max_decimals: number; default_decimals: number; default_hold: number};
  /** Help > About's text (core/xpp_about.h), the desktop window's own box too */
  about: string;
  /** File > Quit's question as the core asks it (W59d), for the page's own while a computation runs
      (W110, store/state.ts LEAVE_ASK): the question, the one naming the recording in progress, the
      answers and their keys */
  quit: {question: string; recording: string; choices: string[]; keys: string};
  menus: {
    main: string[]; main_keys: string; main_hints: string[];
    file: string[]; file_keys: string; file_hints: string[];
    num: string[]; num_keys: string; num_hints: string[];
    /** each item's kind, one letter per key (protocol/kinds.ts) */
    main_kinds: string; file_kinds: string; num_kinds: string;
    /** each menu's name, indexed by state's menu number (0 main, 1 File, 2 nUmerics) */
    names: MenuName[];
    /** the page's name for each item, parallel to the keys (protocol/kinds.ts menuKey) */
    main_ids: string[]; file_ids: string[]; num_ids: string[];
  };
  /** the windows' key layers (protocol/kinds.ts) */
  windows: Record<LayerWindow, WindowLayer>;
  /** every command, with its kind (a key's is its menu item's: "") and whether it is a step */
  commands: CommandKind[];
  /** what the core takes: the largest upload (bytes), the most rows and columns of one browser block */
  upload_error: string;
  player_speed: {min: number; max: number};
  limits: {upload: number; browser_rows: number; browser_cols: number};
  /** the windows' numbers in `window` events: plot windows are 1 to `plots` */
  window_ids: {plots: number; auto: number; ani: number; aplot: number};
  lists: string[][];
  userbuttons: string[];
  /** the model file's own values, in the order of `state`'s pars and ics (what `default` restores) */
  defaults: {pars: number[]; ics: number[]};
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
  sliders: {name: string; lo: number; hi: number; step: number}[];
  pars: [string, number][];
  ics: [string, number][];
  /** the model's own boundary conditions; absent when it defines none */
  bcs?: [string, string][];
  delays?: [string, string][];
  /** the current state, one value per `ics` entry: where the last run ended
      (what Initialconds/Last starts from); absent before any run */
  now?: number[];
  /** Full core time, distinct from the float32 stored trajectory time. */
  time?: number;
  steady?: {status: 'settled' | 'limit' | 'stopped' | 'failed' | 'storage-limit'; decimals: number; time: number};
  view: View;
  rows: number;
  menu: number;
  win: number;
  session?: {file?: string; set?: string; auto?: string};
  /** while the core records the steps (W59a): how many so far, and the note waiting for the next one */
  recording?: {steps: number; note: string};
  /** while a recording is open in the player (W59b): the next step, the one running (-1: none),
      whether it plays, its speed, running with no pace to a `from` step, the fingerprint matching */
  player?: {step: number; running: number; playing: boolean; speed: number; fast: boolean; intact: boolean};
}

/** a recorded step, as the player event lists it (docs/protocol.md "Recordings") */
export interface PlayerStep {
  note: string;
  step: string;
  keys?: string[];
  button?: string;
  win?: string;
  cmd?: Record<string, unknown>;
  answers?: unknown[];
  view?: boolean;
  abort?: {what: string; [k: string]: unknown};
  during?: {key: string; at: {what: string; [k: string]: unknown}}[];
  files?: number[];
}

/** a recording opened in the player (W59b) */
export interface PlayerEvent {
  ev: 'player';
  file: string;
  model: string;
  intact: boolean;
  steps: PlayerStep[];
}

/** what the player sends next, `ms` before it sends it (W59b) */
export interface PressEvent {
  ev: 'press';
  step: number;
  what: 'key' | 'answer' | 'cmd' | 'alert';
  index: number;
  ms: number;
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
  /** the stored data's version: another one is other data (a new run), the
      same one is the same data sent again (docs/protocol.md "The plot as data") */
  version?: number;
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

/** the end of a command that appended to window `win`: the client holds
    all `rows` rows of its series already (the appends delivered them), and
    the data's version is `version` (docs/protocol.md "Live runs") */
export interface SeriesEndEvent {
  ev: 'series';
  op: 'end';
  win: number;
  rows: number;
  version: number;
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
  /** the zoom shown (docs/protocol.md "Display state"): [low, high] per axis, null the window's own */
  zoom?: {x: [number, number] | null; y: [number, number] | null};
  /** 1: the earlier runs are drawn */
  runs?: number;
  freeze?: number;
}

/** a plot window's earlier runs changed (docs/protocol.md "The plot as data"): forget them
    all (`clear`) or the `drop` oldest, then add these; `erased`: Erase hid the current run */
export interface RunsEvent {
  ev: 'runs';
  win: number;
  enc?: 'f32';
  erased: number;
  clear: number;
  drop: number;
  /** 1: the window's series (as the client holds it) becomes its newest earlier run, before `add` */
  keep: number;
  add: {
    rows: number;
    three: number;
    curves: Curve[];
    shift: [number, number, number];
    columns: SeriesColumn[];
  }[];
}

/** AUTO's diagram display (docs/protocol.md "Display state"): the branches before `earlier`
    are those computed before Clear, `show`n or not, the active view and each view's zoom (W50) */
export interface AutoViewEvent {
  ev: 'autoview';
  earlier: number;
  show?: number;
  active?: number;
  /** one per view; a `zoom` absent while a change of the page's is on its way (session.ts) */
  views?: {zoom?: {x: [number, number] | null; y: [number, number] | null}}[];
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
  equilibria: {x: number; y: number; type: 'stable' | 'unstable' | 'saddle'; symbol: string}[];
  /** XPP's text: backslash escapes switch the font (\1 symbol: Greek, \0 roman), \s \S \n
      subscript, superscript, normal; size 0-4 */
  text: {x: number; y: number; text: string; size: number; font: number}[];
  /** from (x1, y1), where the head's tip is, towards (x2, y2); a pointer has a shaft, an arrow only the head */
  arrows: {kind: 'arrow' | 'pointer'; x1: number; y1: number; x2: number; y2: number; size: number; color: number}[];
  markers: {x: number; y: number; shape: 'box' | 'diamond' | 'triangle' | 'plus' | 'cross' | 'circle'; size: number; color: number}[];
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
  /** a `string` or `form` ask: what each field takes (`integer`, `number`, `formula`, `expression`,
      `file`, `name:N`, `text`; one per field of every `string` and `form` ask), store/fieldKinds.ts specOfKind */
  kinds?: string[];
  message?: string;
  button?: string;
  /** a `file` ask: whether the command opens the file or saves one */
  mode?: 'read' | 'write';
  /** a `file` ask: the name offered (possibly a path), the pattern, and the
      core's folder listing (no longer shown by the page) */
  file?: string;
  wild?: string;
  dir?: string;
  dirs?: string[];
  files?: string[];
  /** the window the ask is about (mouse, rubber, drag, grab; a `pixels` ask's window to render) */
  win?: number;
  /** a `pixels` ask for a kinescope frame instead of a window */
  film?: number;
  [k: string]: unknown;
}

/** What every error event carries (docs/protocol.md "Errors", W140): what
    failed, and where: the file ("" when none), its line and column from 1
    (0 when not known) and the line as written ("" when not read). A file
    with line 0 is one the command could not read. `field` is the value field
    the error belongs to (`par:gca`, as fieldKey writes it), "" when none: a
    `set` of several values says which one it refused. */
export interface ErrorFields {
  error: string;
  file: string;
  line: number;
  col: number;
  source: string;
  field: string;
}

/** an error (`error` with its place, ErrorFields), or a status line, an alert, ... */
export interface MessageEvent extends Partial<ErrorFields> {
  ev: 'message';
  bottom?: string;
  help?: {chapter: string; anchor: string};
  box?: string;
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
    core/ui_json.cpp j_make_txtview): [text, hasAction]. `text` already has
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

/** Sing pts result (docs/protocol.md `equilibrium`, core/ui_json.cpp j_show_eq_box):
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
    indices, what its GIF writer paints). `values` is the same cells' stored
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
/** a coordinate: null where the .ani's formula has no value (NaN) */
type AniCoord = number | null;
export type AniPrimWire =
  | ['line', AniCoord, AniCoord, AniCoord, AniCoord, AniColor, number]
  | ['rect' | 'circle' | 'ellipse', AniCoord, AniCoord, AniCoord, AniCoord, AniColor, number, number]
  | ['dot', AniCoord, AniCoord, number, AniColor]
  | ['text', AniCoord, AniCoord, string, AniColor, number, number];

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
  /** the animation window's size in pixels (what widths and dots are relative to) */
  w: number;
  h: number;
  /** numbers are null where the .ani evaluated to NaN */
  prims: AniPrimWire[];
}

/** Kinescope (docs/protocol.md `film`): the core only says what happened
    (a capture, a reset, a play or autoplay to run) and gives the window,
    count and playback timing; the client keeps the frames themselves
    (store/kinescope.ts) and does the drawing and the export. */
export interface FilmEvent {
  ev: 'film';
  op: 'capture' | 'reset' | 'play' | 'autoplay';
  count: number;
  win: number;
  cycles: number;
  delay: number;
}

/** A model that does not load (docs/protocol.md "A model that does not
    load"), sent instead of hello: what is wrong (one or more lines) and
    where (ErrorFields) */
export interface LoadErrorEvent extends ErrorFields {
  ev: 'error';
}

export type XppEvent =
  | HelloEvent
  | LoadErrorEvent
  | StateEvent
  | SeriesEvent
  | SeriesAppendEvent
  | SeriesEndEvent
  | PlotsEvent
  | RunsEvent
  | AutoViewEvent
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
  | FilmEvent
  | PlayerEvent
  | PressEvent
  /** A user save completed (false for a declined or failed save). */
  | {ev: 'saved'; saved: boolean; file: string}
  | {ev: 'idle'}
  /** Erase blanked plot window `win`; Redraw drew its current data again */
  | {ev: 'erase' | 'redraw'; win: number}
  | {ev: 'progress'; n: number; of: number}
  /** Full solver doubles for all states, independently of plot columns. */
  | {ev: 'liveState'; time: number; now: number[]}
  /** the running command began computing: until its idle only view and control actions start */
  | {ev: 'computing'}
  | {ev: 'title'; text: string}
  | {ev: 'menu'; which: number}
  /** File/Help: open the manual at this chapter (and anchor) */
  | {ev: 'help'; chapter: string; anchor?: string}
  /** File/cOpy set line: text for the clipboard */
  | {ev: 'copy'; what: string; text: string}
  | {ev: 'window'; op: 'create' | 'select' | 'destroy'; win: number; w: number; h: number; title?: string}
  | {ev: 'log'; text: string}
  | {ev: 'exit'; code: number}
  | {ev: 'bye'}
  /** the command's computation was cancelled: where it got to */
  | {ev: 'stopped'; at: {what: string; [k: string]: unknown}}
  /* every other event (diagram: store/diagram.ts reads it by its op) */
  | {ev: 'diagram' | 'ping'; [k: string]: unknown}
  /* AUTO's info strip and stability circle (store/diagram.ts AutoInfoEvent) */
  | {ev: 'autoinfo'; [k: string]: unknown}
  /* AUTO's settings (store/autoSettings.ts AutoSettings) */
  | {ev: 'autosettings'; [k: string]: unknown}
  /** the main numerics (docs/protocol.md "The numerics as data") */
  | {ev: 'numerics'; fields: NumericsField[]};

/** one field of the `numerics` event: `key` names it in `set` kind `num`;
    `value` the method's number when `choices` (its names) are given;
    `unused` when the current method does not use it */
export interface NumericsField {
  key: string;
  label: string;
  value: number | null;
  integer?: boolean;
  unused?: boolean;
  choices?: string[];
}

export type Command = {cmd: string; [k: string]: unknown};
