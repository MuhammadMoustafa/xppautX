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

/** the whole series of the active plot window */
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

export type XppEvent =
  | HelloEvent
  | StateEvent
  | SeriesEvent
  | SeriesAppendEvent
  | AskEvent
  | MessageEvent
  | BrowserEvent
  | {ev: 'idle'}
  | {ev: 'progress'; n: number; of: number}
  | {ev: 'title'; text: string}
  | {ev: 'menu'; which: number}
  | {ev: 'window'; op: 'create' | 'select' | 'destroy'; win: number; w: number; h: number; title?: string}
  | {ev: 'log'; text: string}
  | {ev: 'exit'; code: number}
  | {ev: 'bye'}
  /* every other event: drawing ops, AUTO, ... (not used by this UI yet) */
  | {ev: 'draw' | 'palette' | 'diagram' | 'equilibrium' | 'source' | 'equations' | 'ani' | 'aplot'
      | 'film' | 'ping'; [k: string]: unknown};

export type Command = {cmd: string; [k: string]: unknown};
