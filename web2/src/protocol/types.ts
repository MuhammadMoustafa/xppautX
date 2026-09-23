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

export interface SeriesColumn {
  col: number;
  name: string;
  data: (number | null)[];
}

export interface SeriesEvent {
  ev: 'series';
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

export type XppEvent =
  | HelloEvent
  | StateEvent
  | SeriesEvent
  | AskEvent
  | MessageEvent
  | {ev: 'idle'}
  | {ev: 'progress'; n: number; of: number}
  | {ev: 'title'; text: string}
  | {ev: 'menu'; which: number}
  | {ev: 'window'; op: 'create' | 'select' | 'destroy'; win: number; w: number; h: number; title?: string}
  | {ev: 'log'; text: string}
  | {ev: 'exit'; code: number}
  | {ev: 'bye'}
  /* every other event: drawing ops, AUTO, browser, ... (not used by this UI yet) */
  | {ev: 'draw' | 'palette' | 'diagram' | 'browser' | 'equilibrium' | 'source' | 'equations' | 'ani' | 'aplot'
      | 'film' | 'ping'; [k: string]: unknown};

export type Command = {cmd: string; [k: string]: unknown};
