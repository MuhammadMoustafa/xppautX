/* The protocol's messages as types (docs/protocol.md). Only the fields the
   new front end reads are typed; everything else passes through untouched. */

import type {CommandKind} from './kinds';
import type {LineGeometry} from '../plot/linestyle';
import type {AnnotationGlyphs} from '../plot/marks';

/** a plot text's style by its name (core/text_style.h): normal, bold, italic or bold_italic */
export type TextStyle = 'normal' | 'bold' | 'italic' | 'bold_italic';

/** the protocol this page speaks (core/ui_json.h JSON_UI_PROTOCOL): core and
    page ship together, so a hello with another is a shown error (session.ts) */
export const PROTOCOL = 4;

/** a run of the About text: a link when `url` is there */
export interface AboutPart {text: string; url?: string}
export type AboutLine = AboutPart[];

/** a point type of AUTO, as hello gives it */
export interface PointType {
  name: string;
  label: string;
  mark: string;
}

export interface AutoRunTable {
  kinds: string[];
  current: string[];
  rows: {point: string; family: string; kinds: string[]; switch: string[]}[];
}

export interface HelloEvent {
  ev: 'hello';
  protocol: number;
  /** present exactly while the session has no model (the program started with no file): the start
      screen's content (core/xpp_recent.h, docs/protocol.md "Start screen") */
  start?: StartInfo;
  /** the data events the server sends, asked for at once (session.ts) */
  features: string[];
  title: string;
  file: string;
  output_names: {par: string; ic: string; csv: string; curves: string};
  state_inspection: {tail_intervals: number};
  continue: {grid_tolerance: number};
  steady: {max_decimals: number; default_decimals: number; default_hold: number};
  /** Help > About's lines (core/xpp_about.h), the desktop window's own box too: each a list of parts,
      plain text or a link (`url`) */
  about: AboutLine[];
  /** File > Quit's question as the core asks it (W59d), for the page's own while a computation runs
      (W110, store/state.ts LEAVE_ASK): the question, the one naming the recording in progress, the
      answers and their keys */
  quit: {question: string; recording: string; choices: string[]; keys: string};
  /** the user's keymap as the core read it (W211, store/keymap.ts) */
  keymap: KeymapInfo;
  /** AUTO's Run: the kinds, the kinds a start from the current state takes, and one row for each start point type (core/auto_run.h) */
  auto_run: AutoRunTable;
  /** every command, with its kind (a key's is its menu item's: "") and whether it is a step */
  commands: CommandKind[];
  /** what the core takes: the largest upload (bytes), the most rows and columns of one browser block */
  upload_error: string;
  player_speed: {min: number; max: number};
  limits: {upload: number; browser_rows: number; browser_cols: number};
  /** what a plot text takes (core/grobs.h LABEL_*): its most characters, largest size, the size a new text starts at */
  text: {max_length: number; size_max: number; default_size: number};
  /** the names of the text styles, in the core's order (core/text_style.h) */
  text_styles: TextStyle[];
  /** the dash patterns and marker glyphs the pictures draw (core/picture.h): at width and size 1 (plot/linestyle.ts) */
  line_styles: LineGeometry;
  /** the marker glyphs and arrow head the pictures draw for texts, arrows, pointers and markers (core/grobs.h; plot/marks.ts) */
  annotation_glyphs: AnnotationGlyphs;
  /** Initial surface names; plots and diagram views subsequently carry their exact names. */
  window_names: {main: string; auto: string; ani: string; aplot: string; browser: string};
  lists: string[][];
  model_names: string[];
  /** every point type: its name (the only input), its label and its mark (core/auto_codes.h's one table) */
  point_types: PointType[];
  userbuttons: string[];
  /** the colour scales the core has, by name (the array plot's choice) */
  colormaps: string[];
  /** the model file's own values, in the order of `state`'s pars and ics (what `default` restores) */
  defaults: {pars: number[]; ics: number[]};
}

/** one model of recent_models in preferences.setx: `missing` when its file is no longer there (listed, never dropped) */
export interface RecentModel {
  path: string;
  missing: boolean;
}

/** hello.start.examples: the bundled .odex examples beside the program (base names, sorted); `reason`
    says why there are none, and `folder` where they were looked for ("" when the program's folder is unknown) */
export interface StartExamples {
  folder: string;
  names: string[];
  reason?: string;
}

/** hello.start: the models opened lately, newest first, `limit` the most kept; `error` (and the
    `error` event's other fields) when preferences.setx could not be read, with no models listed */
export interface StartInfo {
  path: string;
  limit: number;
  recent: RecentModel[];
  examples: StartExamples;
  error?: string;
  file?: string;
  line?: number;
  col?: number;
  source?: string;
  field?: string;
}

/** one command's effective keys: the user's (source `user`) or the registry's (`default`) */
export interface KeymapCommand {
  id: string;
  keys: string[];
  source: 'default' | 'user';
}

/** The `keymap` of hello and of a `keymap` event (core/xpp_keymap.h, docs/protocol.md "Keymap"). `ok`
    false is a keymap.json that could not be loaded: the error, its file, line and value (the
    `error` event's fields), and the commands are the table's defaults, nothing of the file applied. */
export interface KeymapInfo {
  ok: boolean;
  /** the settings file's path ('' when the system names no config folder) */
  path: string;
  /** the quick-access toolbar's command ids, in order */
  pinned: string[];
  /** keys no command may take (the page and the system keep them) */
  reserved: string[];
  /** the most keys of a command and bytes of a key string */
  limits: {keys: number; key_bytes: number};
  commands: KeymapCommand[];
  error?: string;
  file?: string;
  line?: number;
  col?: number;
  source?: string;
  field?: string;
}

/** the user's differences, as keymap.json holds them and `set` replaces them */
export interface UserKeymap {
  pinned: string[];
  /** command id -> its keys instead of the table's ([] = none) */
  bindings: Record<string, string[]>;
}

/** the answer to a `keymap` command (get, set or reset) */
export interface KeymapEvent {
  ev: 'keymap.changed';
  op: 'get' | 'set' | 'reset';
  keymap: KeymapInfo;
}

export interface View {
  win: string;
  left: number; right: number; top: number; bottom: number;
  xlo: number; xhi: number; ylo: number; yhi: number;
  three: number;
  /** the active window's 3D angles, degrees (sent only when `three`) */
  theta?: number;
  phi?: number;
  /** what the window plots, as text ("W vs V"): the title bar shows it */
  title?: string;
}

export interface StateEvent {
  ev: 'session.state';
  sliders: {name: string; lo: number; hi: number; step: number}[];
  pars: [string, number][];
  ics: [string, number][];
  /** the model's own boundary conditions; absent when it defines none */
  bcs?: [string, string][];
  delays?: [string, string][];
  /** the current state, one value per `ics` entry: where the last run ended
      (what Initialconds/Last starts from); absent before any run */
  now?: number[];
  /** Recent float32 storage motion for every state, independent of plot columns. */
  rates?: (number | null)[];
  /** Full core time, distinct from the float32 stored trajectory time. */
  time?: number;
  steady?: {status: 'settled' | 'limit' | 'stopped' | 'failed'; decimals: number; time: number};
  view: View;
  rows: number;
  win: string;
  session?: {file?: string; set?: string; auto?: string};
  /** the values (parameters, initial conditions, the numerics' time span, method and tolerances, the run's data) differ from
      those last saved, opened or loaded: the title's unsaved dot (docs/protocol.md "state") */
  changed: boolean;
  /** a value edit can be taken back (undo) or put back (redo): the commands of those ids are off otherwise (docs/protocol.md "state") */
  can_undo: boolean;
  can_redo: boolean;
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
  cmd?: Record<string, unknown>;
  answers?: unknown[];
  view?: boolean;
  abort?: {what: string; [k: string]: unknown};
  during?: {key: string; at: {what: string; [k: string]: unknown}}[];
  files?: number[];
}

/** a recording opened in the player (W59b) */
export interface PlayerEvent {
  ev: 'recording.player';
  file: string;
  model: string;
  intact: boolean;
  steps: PlayerStep[];
}

/** what the player sends next, `ms` before it sends it (W59b) */
export interface PressEvent {
  ev: 'recording.press';
  step: number;
  what: 'answer' | 'cmd';
  index: number;
  ms: number;
}

/** A colour as the core says it: `#rrggbb`, `#rrggbbaa` (alpha only when not opaque) or `foreground`,
    the colour the page draws dark on a light theme and light on a dark one */
export type CoreColor = string;

/** how a curve (or a frozen curve) is drawn: line, points or both, the line's dash and width,
    the points' marker and its size (pixels) */
export interface LineStyle {
  draw: 'line' | 'points' | 'both';
  dash: 'solid' | 'dashed' | 'dotted' | 'dash_dot';
  width: number;
  marker: 'circle' | 'square' | 'triangle' | 'diamond' | 'cross' | 'plus' | 'star';
  marker_size: number;
}

/** One curve of the active plot window: storage columns (0 is T), its colour and style. */
export interface Curve extends LineStyle {
  x: number;
  y: number;
  z: number;
  color: CoreColor;
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
  ev: 'plot.series';
  op?: undefined;
  enc?: 'f32';
  win: string;
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
  ev: 'plot.series';
  op: 'append';
  enc?: 'f32';
  win: string;
  from: number;
  /** rows after this append */
  rows: number;
  columns: {col: number; data: SeriesData}[];
}

/** the end of a command that appended to window `win`: the client holds
    all `rows` rows of its series already (the appends delivered them), and
    the data's version is `version` (docs/protocol.md "Live runs") */
export interface SeriesEndEvent {
  ev: 'plot.series';
  op: 'end';
  win: string;
  rows: number;
  version: number;
}

/** one window of the core as window.list and the `windows` event give it */
export interface WindowListEntry {
  name: string;
  kind: 'graph' | 'diagram' | 'browser' | 'animation' | 'array_plot';
  shown: boolean;
}

/** one plot window as `plots` describes it */
export interface PlotWindowInfo {
  redraw_mode?: string;
  win: string;
  /** The exact user-assigned window name. */
  title: string;
  three: number;
  /** the window's axes (Viewaxes, Window/Zoom); in 3D the projected view's */
  xlo: number; xhi: number; ylo: number; yhi: number;
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
  ev: 'plot.runs';
  win: string;
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
  ev: 'auto.view';
  earlier: number;
  show?: number;
  active?: string;
  /** one per view; a `zoom` absent while a change of the page's is on its way (session.ts) */
  views?: {name: string; zoom?: {x: [number, number] | null; y: [number, number] | null}}[];
}

/** every plot window and the active one */
export interface PlotsEvent {
  ev: 'plot.shown';
  active: string;
  simultaneous_plot?: boolean;
  windows: PlotWindowInfo[];
}

/** a plot window's nullclines (docs/protocol.md "The plot as data"): segments
    [x1,y1,x2,y2,...] in plot coordinates, empty when the window shows none */
export interface NullclinesEvent {
  ev: 'nullcline.curves';
  win: string;
  enc?: 'f32';
  /** the variables whose derivative is 0 along the x- and y-nullcline */
  xname: string;
  yname: string;
  /** colours, as a curve's */
  xcolor: CoreColor;
  ycolor: CoreColor;
  x: SeriesData;
  y: SeriesData;
  /** Nullcline/Freeze: earlier sets, drawn in the same colours */
  frozen: {x: SeriesData; y: SeriesData}[];
}

/** a plot window's direction field and Flow trajectories */
export interface DfieldEvent {
  ev: 'dirfield.field';
  win: string;
  enc?: 'f32';
  /** 1: arrows of one length (Scaled Dir.Fld); 0: lengths follow the speed (Direct field) */
  scaled: number;
  color: CoreColor;
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
  flows: {color: CoreColor; x: SeriesData; y: SeriesData}[];
}

/** what a plot window shows on top of its curves (docs/protocol.md "The plot
    as data", `marks`): the equilibria Sing pts marked, Text,etc's labels,
    arrows and markers, Graphic stuff/Freeze's frozen curves, in plot
    coordinates; each list empty when the window shows none */
export interface MarksEvent {
  ev: 'plot.marks';
  win: string;
  enc?: 'f32';
  /** the symbol XPP draws for the stability: stable circle, unstable box, saddle triangle */
  equilibria: {x: number; y: number; type: 'stable' | 'unstable' | 'saddle'; symbol: string}[];
  /** XPP's text: backslash escapes switch the font (\1 symbol: Greek, \0 roman), \s \S \n
      subscript, superscript, normal; size 0-4; `source` is the text as written (its \{expr}s unfilled); `id` names the text for the `annotation` command; style normal, bold,
      italic or bold_italic; color its colour */
  text: {id: number; x: number; y: number; text: string; source: string; size: number; font: number; style: TextStyle; color: CoreColor}[];
  /** from (x1, y1), where the head's tip is, towards (x2, y2); a pointer has a shaft, an arrow only the head */
  arrows: {id: number; kind: 'arrow' | 'pointer'; x1: number; y1: number; x2: number; y2: number; size: number; color: CoreColor}[];
  markers: {id: number; x: number; y: number; shape: 'box' | 'diamond' | 'triangle' | 'plus' | 'cross' | 'circle'; size: number; color: CoreColor}[];
  frozen: (LineStyle & {slot: number; key: string; name: string; color: CoreColor; x: SeriesData; y: SeriesData})[];
}

export interface AskEvent {
  ev: 'ask';
  id: number;
  kind: 'choice' | 'string' | 'form' | 'checklist' | 'file' | 'mouse' | 'rubber' | 'grab'
    | 'drag' | 'pixels' | 'text';
  title?: string;
  name?: string;
  value?: string;
  keys?: string;
  hints?: string[];
  question?: string;
  choices?: string[];
  names?: string[];
  values?: string[];
  /** a `string` or `form` ask: what each field takes (`integer`, `number`, `formula`, `expression`,
      `file`, `name:N`, `text`; one per field of every `string` and `form` ask), store/fieldKinds.ts specOfKind */
  kinds?: string[];
  /** a `form` ask with typed fields: per field the names of a `choice` (null for another kind) and the
      `range` of an `integer` or `real` (null when it has none) */
  options?: (string[] | null)[];
  ranges?: ({min?: number; max?: number} | null)[];
  /** a `form` ask laid out as a grid: its columns and its row labels; the first columns x rows fields are the
      cells, row by row, the rest follow as plain fields (the axes dialog) */
  columns?: string[];
  rows?: string[];
  /** a `file` ask: whether the command opens the file or saves one */
  mode?: 'read' | 'write';
  /** a `file` ask: the name offered (possibly a path), the pattern, and the
      core's folder listing (no longer shown by the page) */
  file?: string;
  wild?: string;
  dir?: string;
  /** Writable model folder offered for a native save retry. */
  save_dir?: string;
  dirs?: string[];
  files?: string[];
  /** the window the ask is about (mouse, rubber, drag, grab; a `pixels` ask's window to render) */
  win?: string;
  /** a `pixels` ask for a kinescope frame instead of a window */
  film?: number;
  [k: string]: unknown;
}

/** What every error event carries (docs/protocol.md "Errors", W140): what
    failed, and where: the file ("" when none), its line and column from 1
    (0 when not known) and the line as written ("" when not read). A file
    with line 0 is one the command could not read. `field` is the value field
    the error belongs to (`par:gca`, as fieldKey writes it), "" when none: a
    `set` of several values says which one it refused. `action`, when the
    core offers one, is what the user can do about it: a label and the
    commands that do it, sent in order (W260: "Switch to differences and
    continue"). */
export interface ErrorFields {
  error: string;
  file: string;
  line: number;
  col: number;
  source: string;
  field: string;
  action?: ErrorAction;
}

/** what an error offers to do (docs/protocol.md "Errors") */
export interface ErrorAction {
  label: string;
  commands: Command[];
}

/** an error (`error` with its place, ErrorFields), or a status line, an alert, ... */
export interface MessageEvent extends Partial<ErrorFields> {
  ev: 'session.message';
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
  ev: 'browser.updated';
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
  ev: 'session.source';
  lines: string[];
  comments: [string, number][];
}

/** Text,etc/Eqns: one `dX/dT=...` line per equation (docs/protocol.md `equations`). */
export interface EquationsEvent {
  ev: 'session.equations';
  lines: string[];
}

/** Sing pts result (docs/protocol.md `equilibrium`, core/ui_json.cpp j_show_eq_box):
    `type` is "STABLE", "UNSTABLE" or "NEUTRAL" (core/xpp_util.c eq_stability);
    cplus/cminus/rplus/rminus/im are eigenvalue counts (complex/real with
    positive/negative real part, purely imaginary); `eigenvalues` the
    Jacobian's eigenvalues as [re, im] pairs, absent for a delay equation. */
export interface EquilibriumEvent {
  ev: 'equilibrium.found';
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
    `ny` rows of `nx` cells, row-major. `values` is the cells' stored numbers
    (float32, null/NaN off the stored rows or columns); a cell's colour is the
    ramp's at the fraction of zmin to zmax its value is (the core's rule,
    colormap.h's ramp_color); `enc` "f32" (the client's last `data` `enc`)
    sends it as base64 like a series column. */
export interface AplotEvent {
  ev: 'array_plot.shown';
  title: string;
  nx: number;
  ny: number;
  /** the stored row the first row of cells shows (array_plot.scroll's first_row) */
  first_row?: number;
  values: SeriesData;
  enc?: 'f32';
  /** the session's colour scale: its name and its 256 colours from the low end to the high one */
  colormap: string;
  ramp: CoreColor[];
  zmin: number;
  zmax: number;
  tlo: number;
  thi: number;
  tag?: string;
}

/** the animation window's state (docs/protocol.md `ani`), for its slider and
    toggles: sent with every frame drawn and after every `ani` command */
export interface AniStateEvent {
  ev: 'animation.state';
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

/** a primitive's colour, as the core says it */
export type AniColor = CoreColor;

/** a frame's primitive as sent (docs/protocol.md "The animation as data"):
    unit coordinates u, v of the dimension box (y up), not clamped; widths
    and a dot's radius in pixels */
/** a coordinate: null where the .ani's formula has no value (NaN) */
type AniCoord = number | null;
export type AniPrimWire =
  | ['line', AniCoord, AniCoord, AniCoord, AniCoord, AniColor, number, LineStyle['dash']]
  | ['rect' | 'circle' | 'ellipse', AniCoord, AniCoord, AniCoord, AniCoord, AniColor, number, number, LineStyle['dash']]
  | ['dot', AniCoord, AniCoord, number, AniColor]
  | ['text', AniCoord, AniCoord, string, AniColor, number, number];

/** one frame of the animation, for a client that asked (`data` with `ani`) */
export interface AniFrameEvent {
  ev: 'animation.frame';
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
  ev: 'kinescope.film';
  op: 'capture' | 'reset' | 'play' | 'autoplay';
  count: number;
  win: string;
  cycles: number;
  delay: number;
}

/** A model that does not load (docs/protocol.md "A model that does not
    load"), sent instead of hello: what is wrong (one or more lines) and
    where (ErrorFields) */
export interface LoadErrorEvent extends ErrorFields {
  ev: 'session.error';
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
  | KeymapEvent
  /** A user save completed (false for a declined or failed save). */
  | {ev: 'session.saved'; saved: boolean; file: string}
  | {ev: 'done'; id?: string; status: 'ok' | 'error' | 'stopped'; result?: Record<string,unknown>; error?: string; file?: string; line?: number; col?: number; source?: string; field?: string}
  | {ev: 'window.renamed'; window: string; new_name: string}
  /** every open window and whether it is shown (window.list's result; sent again when it changed) */
  | {ev: 'window.list'; windows: WindowListEntry[]}
  /** Erase blanked plot window `win`; Redraw drew its current data again */
  | {ev: 'plot.cleared' | 'plot.redrawn'; win: string}
  | {ev: 'job.progress'; n: number; of: number}
  /** Full solver doubles for all states, independently of plot columns. */
  | {ev: 'job.live_state'; time: number; now: number[]; rates: (number | null)[]}
  /** the running command began computing: until its done only view and control actions start */
  | {ev: 'job.computing'}
  | WindowEvent
  | {ev: 'session.log'; text: string}
  | {ev: 'exit'; code: number}
  | {ev: 'session.bye'}
  /** the command's computation was cancelled: where it got to */
  | {ev: 'job.stopped'; at: {what: string; [k: string]: unknown}}
  /* every other event (auto.diagram: store/diagram.ts reads it by its op) */
  | {ev: 'auto.diagram' | 'session.ping'; [k: string]: unknown}
  /* AUTO's info strip and stability circle (store/diagram.ts AutoInfoEvent) */
  | {ev: 'auto.info'; [k: string]: unknown}
  /* AUTO's settings (store/autoSettings.ts AutoSettings) */
  | {ev: 'auto.settings'; [k: string]: unknown}
  /** the main numerics (docs/protocol.md "The numerics as data") */
  | {ev: 'run.numerics'; fields: NumericsField[]};

/** a window made, selected or destroyed (docs/protocol.md): the event's name says which */
export interface WindowEvent {
  ev: 'window.created' | 'window.selected' | 'window.destroyed';
  kind: 'graph' | 'diagram' | 'animation' | 'array_plot' | 'browser';
  win: string;
  w: number;
  h: number;
  title?: string;
}

/** one field of the `numerics` event: `key` names it in `set` kind `num`;
    `value` the choice's name when `choices` (their names: the method's,
    the Jacobian's) are given, a boolean when `boolean`, else a number;
    `unavailable` why the model cannot have a choice, by its name (W260: no exact
    Jacobian, known once exact was chosen); `unused` when the current
    method does not use it */
export interface NumericsField {
  key: string;
  label: string;
  value: number | string | boolean | null;
  integer?: boolean;
  boolean?: boolean;
  unused?: boolean;
  choices?: string[];
  unavailable?: Record<string, string>;
}

export type Command = {cmd: string; [k: string]: unknown};
