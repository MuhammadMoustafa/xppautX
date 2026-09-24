/* The AUTO diagram (docs/ui-v2.md T11a, docs/protocol.md "The AUTO diagram
   as data"): every point the core plots, in the diagram's axis quantities,
   built from the `diagram` events exactly (a `reset` drops the points after
   `keep`, an `add` puts points `from`.. in place), the core's axes, and the
   view's own state: whether window 101 is open, whether its panel is shown,
   the user's zoom (with undo) and the point read out. The window follows the
   core: `window` create/destroy for 101, and `state.auto`, which is there
   exactly while AUTO is open (a page that connects later learns it so).
   T11b adds the `autoinfo` event (the info strip and the stability circle
   as data), the grab (the core's `grab` ask, answered from the view) and
   the point a click stored in a two-parameter diagram (`auto point`).
   Pure: no DOM, no I/O. */
import type {Viewport} from './plots';

/** `diagram` `axes` (and `reset`): the core's view of the diagram */
export interface DiagramAxes {
  xmin: number; xmax: number; ymin: number; ymax: number;
  /** where the core draws it in window 101's pixels (for answers in pixels) */
  x0: number; y0: number; wid: number; hgt: number;
  /** Auto.plot: 0 hi, 1 norm, 2 hi and lo, 3 period, 4 two parameters, 10 frequency, 11 average */
  plot: number;
  xlabel: string;
  ylabel: string;
}

/** one run of an `add`: points sharing branch, kind and style */
export interface DiagramRun {
  br: number;
  pt: number;
  ty: number;
  d: number;
  c: number;
  lw: number;
  f2?: number;
  new?: number;
  /** the label the run that computed its first point started from */
  from?: number;
  x: (number | null)[];
  y: (number | null)[];
  y2?: (number | null)[];
  lab?: [number, number, string][];
}

export type DiagramEvent =
  | ({ev: 'diagram'; op: 'axes'} & DiagramAxes)
  | ({ev: 'diagram'; op: 'reset'; keep: number} & DiagramAxes)
  | {ev: 'diagram'; op: 'add'; from: number; runs: DiagramRun[]};

/** the points, one column per field, point i across them (NaN for null) */
export interface DiagramPoints {
  x: number[];
  y: number[];
  /** the second value (the minimum for hi and lo); y where the event had none */
  y2: number[];
  br: number[];
  pt: number[];
  /** 1 stable steady state, 2 unstable steady state, 3 stable periodic, 4 unstable periodic */
  ty: number[];
  /** 0 not drawn (the next line starts there), 1 a line back, 2 filled circles, 3 open circles */
  d: number[];
  /** palette colour (0 the foreground, 20..29 red..purple) */
  c: number[];
  lw: number[];
  /** two-parameter curve kind, 0 for one parameter */
  f2: number[];
  /** 1: no line back to the point before */
  nw: number[];
  /** the label its run started from (a run's first point only), 0 otherwise */
  fr: number[];
}

/** `autoinfo` `info`: the point AUTO's info strip shows (the grab's cursor) */
export interface AutoInfo {
  /** its index in the diagram's points, -1 when they do not have it */
  point: number;
  br: number;
  pt: number;
  /** as a run's `ty` */
  type: number;
  /** EP, LP, HB, ... or empty */
  sym: string;
  lab: number;
  f2?: number;
  /** the continuation parameter, and the second one of a two-parameter point */
  par: {name: string; value: number | null}[];
  norm: number | null;
  /** the variable of the Axes setting and its value */
  var: string;
  u: number | null;
  per: number | null;
  /** where the diagram plots it */
  x: number | null;
  y: number | null;
  y2: number | null;
}

/** `autoinfo` `stab`: what the stability circle shows */
export interface AutoStab {
  /** 1: Floquet multipliers of a periodic orbit; 0: e^λ of a steady state's eigenvalues λ */
  periodic: number;
  circle: [number | null, number | null][];
  /** a steady state's eigenvalues λ (the imaginary part modulo 2π) */
  eig?: [number | null, number | null][];
}

export interface AutoInfoEvent {
  ev: 'autoinfo';
  info: AutoInfo | null;
  stab: AutoStab | null;
}

export interface DiagramLabel {
  /** index of the point */
  point: number;
  lab: number;
  /** EP, LP, HB, BP, PD, TR, UZ, MX (empty when AUTO gives none) */
  sym: string;
}

export interface DiagramHover {
  point: number;
  /** the second value (y2) rather than y */
  low: boolean;
}

export interface DiagramState {
  /** window 101 exists */
  open: boolean;
  /** its panel is on screen (Back hides it, the core's window stays) */
  shown: boolean;
  axes: DiagramAxes | null;
  points: DiagramPoints;
  /** in point order */
  labels: DiagramLabel[];
  /** `diagram` events applied (tests wait on it) */
  events: number;
  /** an `add` did not follow on from what is held: the data must be sent again (session.ts asks) */
  outOfStep: boolean;
  viewport: Viewport;
  viewportHistory: Viewport[];
  hover: DiagramHover | null;
  /** the info strip and the stability circle (`autoinfo`) */
  info: AutoInfo | null;
  stab: AutoStab | null;
  /** `autoinfo` events applied (tests wait on it) */
  infoEvents: number;
  /** the core is grabbing a point: from its `grab` ask to the command's end */
  grabbing: boolean;
  /** the point a click stored in a two-parameter diagram (`auto point`) */
  stored: {x: number; y: number} | null;
  /** the last Run (T21: the view's status strip), null before one */
  run: AutoRun | null;
  /** Clear (T21): the points before this index are earlier branches, hidden
      unless `showEarlier` (the core keeps them; new runs draw alone) */
  earlier: number;
  showEarlier: boolean;
  /** the last Numerics and axes file saved (T21, store/autoSetup.ts), for tests */
  setupSaved: string | null;
}

/** a Run of AUTO, as the status strip tells it; what it computed so far is
    read from the points from `first` on (runStatus) */
export interface AutoRun {
  /** its command has not ended */
  active: boolean;
  /** Date.now() when it started computing (the Start menu answered), and when it ended */
  started: number;
  ended: number | null;
  /** the number of points the diagram held when it started */
  first: number;
  /** it was stopped (the `stopped` event), not finished */
  stopped: boolean;
}

export const FIELDS = ['x', 'y', 'y2', 'br', 'pt', 'ty', 'd', 'c', 'lw', 'f2', 'nw', 'fr'] as const;

function noPoints(): DiagramPoints {
  return {x: [], y: [], y2: [], br: [], pt: [], ty: [], d: [], c: [], lw: [], f2: [], nw: [], fr: []};
}

const HOME: Viewport = {x: null, y: null};
const HISTORY_KEEP = 50;

export const initialDiagram: DiagramState = {
  open: false, shown: false, axes: null, points: noPoints(), labels: [], events: 0, outOfStep: false,
  viewport: HOME, viewportHistory: [], hover: null, info: null, stab: null, infoEvents: 0, grabbing: false, stored: null,
  run: null, earlier: 0, showEarlier: false, setupSaved: null,
};

export type DiagramAction =
  | {type: 'event'; ev: DiagramEvent}
  | {type: 'window'; op: 'create' | 'destroy'}
  /** a `state` event: `auto` present while AUTO is open */
  | {type: 'core'; open: boolean}
  | {type: 'show'; shown: boolean}
  | {type: 'viewport'; viewport: Viewport; push?: boolean}
  | {type: 'undoViewport'}
  | {type: 'hover'; hover: DiagramHover | null}
  | {type: 'info'; ev: AutoInfoEvent}
  /** the core's grab: its ask came (on), or its command ended */
  | {type: 'grabbing'; on: boolean}
  | {type: 'stored'; at: {x: number; y: number} | null}
  /** Run pressed (start), its Start menu answered (clock), its command ended (end) */
  | {type: 'run'; op: 'start' | 'clock' | 'end'; at: number}
  | {type: 'runStopped'}
  /** Clear: what is drawn now becomes the earlier branches */
  | {type: 'clear'}
  | {type: 'showEarlier'; show: boolean}
  | {type: 'setupSaved'; text: string};

export function pointCount(p: DiagramPoints): number {
  return p.x.length;
}

const num = (v: number | null | undefined) => (v === null || v === undefined ? NaN : v);

/** the first `k` points */
function truncate(p: DiagramPoints, k: number): DiagramPoints {
  if (k >= pointCount(p)) return p;
  const out = {} as DiagramPoints;
  for (const f of FIELDS) out[f] = p[f].slice(0, k);
  return out;
}

/** `ev`'s points put in place from `from` on (the caller checked from <= held) */
function add(p: DiagramPoints, labels: DiagramLabel[], from: number, runs: DiagramRun[]):
  {points: DiagramPoints; labels: DiagramLabel[]} {
  const out = {} as DiagramPoints;
  for (const f of FIELDS) out[f] = p[f].slice(0, from);
  const labs = labels.filter(l => l.point < from);
  for (const r of runs) {
    const base = out.x.length;
    for (let i = 0; i < r.x.length; i++) {
      const y = num(r.y[i]);
      out.x.push(num(r.x[i]));
      out.y.push(y);
      out.y2.push(r.y2 ? num(r.y2[i]) : y);
      out.br.push(r.br);
      out.pt.push(r.pt + i);
      out.ty.push(r.ty);
      out.d.push(r.d);
      out.c.push(r.c);
      out.lw.push(r.lw);
      out.f2.push(r.f2 ?? 0);
      out.nw.push(i === 0 && r.new ? 1 : 0);
      out.fr.push(i === 0 && r.from ? r.from : 0);
    }
    for (const [i, lab, sym] of r.lab ?? []) labs.push({point: base + i, lab, sym});
  }
  return {points: out, labels: labs};
}

function axesOf(ev: DiagramAxes): DiagramAxes {
  const {xmin, xmax, ymin, ymax, x0, y0, wid, hgt, plot, xlabel, ylabel} = ev;
  return {xmin, xmax, ymin, ymax, x0, y0, wid, hgt, plot, xlabel, ylabel};
}

function sameRanges(a: DiagramAxes | null, b: DiagramAxes): boolean {
  return !!a && a.xmin === b.xmin && a.xmax === b.xmax && a.ymin === b.ymin && a.ymax === b.ymax;
}

/** the core drew the diagram at other axes (Axes, Fit, a zoom or scroll of its
    own): what it shows now is what the user asked for, so the view goes back
    to it, the user's zoom one Undo away */
function withAxes(s: DiagramState, ev: DiagramAxes): DiagramState {
  const axes = axesOf(ev);
  const moved = s.axes && !sameRanges(s.axes, axes) && (s.viewport.x !== null || s.viewport.y !== null);
  return moved ? setViewport({...s, axes}, HOME, true) : {...s, axes};
}

function setViewport(s: DiagramState, viewport: Viewport, push = false): DiagramState {
  if (JSON.stringify(viewport) === JSON.stringify(s.viewport)) return s;
  const viewportHistory = push ? [...s.viewportHistory, s.viewport].slice(-HISTORY_KEEP) : s.viewportHistory;
  return {...s, viewport, viewportHistory};
}

function onEvent(s: DiagramState, ev: DiagramEvent): DiagramState {
  const events = s.events + 1;
  switch (ev.op) {
    case 'axes':
      return {...withAxes(s, ev), events};
    case 'reset': {
      const keep = Math.max(0, ev.keep || 0);
      const points = truncate(s.points, keep);
      const hover = s.hover && s.hover.point < keep ? s.hover : null;
      return {...withAxes(s, ev), points, labels: s.labels.filter(l => l.point < keep), hover, outOfStep: false, events};
    }
    case 'add': {
      if (ev.from > pointCount(s.points)) return {...s, outOfStep: true, events};
      const {points, labels} = add(s.points, s.labels, ev.from, ev.runs);
      const hover = s.hover && s.hover.point < ev.from ? s.hover : null;
      return {...s, points, labels, hover, events, outOfStep: ev.from === 0 ? false : s.outOfStep};
    }
    default:
      return s;
  }
}

/** a new window starts empty; a second create for an open one is a resize, which keeps what it has */
function opened(s: DiagramState): DiagramState {
  return s.open ? s : {...initialDiagram, open: true, shown: true, events: s.events, infoEvents: s.infoEvents};
}

function closed(s: DiagramState): DiagramState {
  return s.open || s.axes || pointCount(s.points) ? {...initialDiagram, events: s.events, infoEvents: s.infoEvents} : s;
}

export function reduceDiagram(s: DiagramState, a: DiagramAction): DiagramState {
  switch (a.type) {
    case 'event':
      return onEvent(s, a.ev);
    case 'window':
      return a.op === 'create' ? opened(s) : closed(s);
    case 'core':
      return a.open === s.open ? s : a.open ? opened(s) : closed(s);
    case 'show':
      return a.shown === s.shown || !s.open ? s : {...s, shown: a.shown};
    case 'viewport':
      return setViewport(s, a.viewport, a.push);
    case 'undoViewport':
      if (!s.viewportHistory.length) return s;
      return {...s, viewport: s.viewportHistory[s.viewportHistory.length - 1], viewportHistory: s.viewportHistory.slice(0, -1)};
    case 'hover': {
      const h = a.hover, o = s.hover;
      if (h === o || (h && o && h.point === o.point && h.low === o.low)) return s;
      return {...s, hover: h};
    }
    case 'info':
      return {...s, info: a.ev.info ?? null, stab: a.ev.stab ?? null, infoEvents: s.infoEvents + 1};
    case 'grabbing':
      /* a grab shows the panel: the diagram is where the point is picked */
      return a.on === s.grabbing ? s : {...s, grabbing: a.on, shown: a.on && s.open ? true : s.shown};
    case 'stored':
      return {...s, stored: a.at};
    case 'run':
      return onRun(s, a.op, a.at);
    case 'runStopped':
      return s.run?.active ? {...s, run: {...s.run, stopped: true}} : s;
    case 'clear':
      return {...s, earlier: pointCount(s.points), showEarlier: false, hover: null};
    case 'showEarlier':
      return a.show === s.showEarlier ? s : {...s, showEarlier: a.show};
    case 'setupSaved':
      return {...s, setupSaved: a.text};
  }
}

function onRun(s: DiagramState, op: 'start' | 'clock' | 'end', at: number): DiagramState {
  if (op === 'start') return {...s, run: {active: true, started: at, ended: null, first: pointCount(s.points), stopped: false}};
  if (!s.run?.active) return s;
  if (op === 'clock') return {...s, run: {...s.run, started: at}};
  return {...s, run: {...s.run, active: false, ended: at}};
}

/** a command ended: no grab any more, and a diagram that holds fewer points
    than Clear hid (File/Reset diagram) has no earlier branches left (a
    redraw in other quantities sends them all again before its end) */
export function diagramSettled(s: DiagramState): DiagramState {
  const t = reduceDiagram(s, {type: 'grabbing', on: false});
  const n = pointCount(t.points);
  return t.earlier > n ? {...t, earlier: n} : t;
}

/** the points Clear hid, and whether they are shown */
export function earlierCount(s: Pick<DiagramState, 'earlier' | 'points'>): number {
  return Math.min(s.earlier, pointCount(s.points));
}

/** the number of branches among the first `n` points */
export function branchesBefore(p: DiagramPoints, n: number): number {
  const seen = new Set<number>();
  for (let i = 0; i < n && i < p.br.length; i++) seen.add(p.br[i]);
  return seen.size;
}

/** the label of point `i`, if it has one */
export function labelAt(s: Pick<DiagramState, 'labels'>, i: number): DiagramLabel | undefined {
  return s.labels.find(l => l.point === i);
}
