/* The AUTO diagram (docs/ui-v2.md T11a, docs/protocol.md "The AUTO diagram
   as data"): every point the core plots, in the diagram's axis quantities,
   built from the `diagram` events exactly (a `reset` drops the points after
   `keep`, an `add` puts points `from`.. in place), the core's axes, and the
   view's own state: whether window 101 is open, whether its panel is shown,
   the user's zoom (no history: Reset view/Fit take it back) and the point
   read out. The window follows the
   core: `window` create/destroy for 101, and `state.auto`, which is there
   exactly while AUTO is open (a page that connects later learns it so).
   T11b adds the `autoinfo` event (the info strip and the stability circle
   as data), the grab (the core's `grab` ask, answered from the view) and
   the point a click stored in a two-parameter diagram (`auto point`).
   W50: the one diagram has any number of views, each its own axes, points
   and zoom (`views`, the core's active one `active`); every view holds the
   same points in the same order, so a point index (the hover, the grab,
   Clear's `earlier`) is the same in all of them.
   Pure: no DOM, no I/O. */
import type {AutoViewEvent, HelloEvent} from '../protocol/types';
import {viewportOf, type Viewport} from './plots';

/** the AUTO diagram's window number in the protocol (hello.window_ids.auto), null before hello */
export function autoWindow(s: {hello: HelloEvent | null}): number | null {
  return s.hello?.window_ids.auto ?? null;
}

/** `diagram` `axes` (and `reset`): the core's axes of a view of the diagram */
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
  /** what the core says ty is: stable or not, a periodic orbit or a steady state */
  stable: boolean;
  periodic: boolean;
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
  | ({ev: 'diagram'; op: 'axes'; view: number} & DiagramAxes)
  | ({ev: 'diagram'; op: 'reset'; view: number; keep: number} & DiagramAxes)
  | {ev: 'diagram'; op: 'add'; view: number; from: number; runs: DiagramRun[]}
  /** the diagram has `n` views now (W50) */
  | {ev: 'diagram'; op: 'views'; n: number};

/** the points, one column per field, point i across them (NaN for null) */
export interface DiagramPoints {
  x: number[];
  y: number[];
  /** the second value (the minimum for hi and lo); y where the event had none */
  y2: number[];
  br: number[];
  pt: number[];
  /** 1 stable steady state, 2 unstable steady state, 3 stable periodic, 4 unstable periodic (the core's) */
  ty: number[];
  /** 1: stable, 0: unstable (the run's `stable`) */
  st: number[];
  /** 1: a periodic orbit, 0: a steady state (the run's `periodic`) */
  pe: number[];
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
  /** as a run's `ty`, `stable` and `periodic` */
  type: number;
  stable: boolean;
  periodic: boolean;
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

/** `autoinfo` `stop` (T23): why the run's last branch ended */
export interface AutoStop {
  /** parmin, parmax, normmin, normmax, npts, user, mark, noconv, noconv-fixed, noconv-min,
      noconv-switch-fixed, noconv-switch-min (docs/protocol.md) */
  why: string;
  /** in words, to follow "Stopped: " ("parameter iapp reached Par Max (0.45)") */
  text: string;
  /** AUTO's branch and point number of the end */
  br: number;
  pt: number;
  /** what reached the limit, and the limit (null when there is none) */
  value: number | null;
  limit: number | null;
}

export interface AutoInfoEvent {
  ev: 'autoinfo';
  info: AutoInfo | null;
  stab: AutoStab | null;
  stop?: AutoStop | null;
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
  /** the view the pointer is on (every view marks the point: the same index in each) */
  view: number;
}

/** one view of the diagram (W50): its points at its own axes, and the user's zoom of them */
export interface DiagramView {
  axes: DiagramAxes | null;
  points: DiagramPoints;
  /** in point order */
  labels: DiagramLabel[];
  viewport: Viewport;
}

export interface DiagramState {
  /** window 101 exists */
  open: boolean;
  /** its panel is on screen (Back hides it, the core's window stays) */
  shown: boolean;
  /** the views of the one diagram (W50, docs/protocol.md "Views of the diagram"), at least one: each
      holds the same points in the same order, so a point's index is the same in every view */
  views: DiagramView[];
  /** the core's active view: the one AUTO's Axes menu, zoom, the exports and a run go by */
  active: number;
  /** `diagram` events applied (tests wait on it) */
  events: number;
  /** an `add` did not follow on from what is held: the data must be sent again (session.ts asks) */
  outOfStep: boolean;
  hover: DiagramHover | null;
  /** the info strip and the stability circle (`autoinfo`) */
  info: AutoInfo | null;
  stab: AutoStab | null;
  /** why the last run's last branch ended (`autoinfo` `stop`, T23) */
  stop: AutoStop | null;
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

export const FIELDS = ['x', 'y', 'y2', 'br', 'pt', 'ty', 'st', 'pe', 'd', 'c', 'lw', 'f2', 'nw', 'fr'] as const;

function noPoints(): DiagramPoints {
  return {x: [], y: [], y2: [], br: [], pt: [], ty: [], st: [], pe: [], d: [], c: [], lw: [], f2: [], nw: [], fr: []};
}

const HOME: Viewport = {x: null, y: null};

function emptyView(): DiagramView {
  return {axes: null, points: noPoints(), labels: [], viewport: HOME};
}

/** a view with nothing yet (a selector's answer for a view that has just gone) */
export const EMPTY_VIEW: DiagramView = emptyView();

export const initialDiagram: DiagramState = {
  open: false, shown: false, views: [emptyView()], active: 0, events: 0, outOfStep: false,
  hover: null, info: null, stab: null, stop: null, infoEvents: 0, grabbing: false, stored: null,
  run: null, earlier: 0, showEarlier: false,
};

export type DiagramAction =
  | {type: 'event'; ev: DiagramEvent}
  | {type: 'window'; op: 'create' | 'destroy'}
  /** a `state` event: `auto` present while AUTO is open */
  | {type: 'core'; open: boolean}
  | {type: 'show'; shown: boolean}
  | {type: 'viewport'; view: number; viewport: Viewport}
  | {type: 'hover'; hover: DiagramHover | null}
  | {type: 'info'; ev: AutoInfoEvent}
  /** the core's grab: its ask came (on), or its command ended */
  | {type: 'grabbing'; on: boolean}
  | {type: 'stored'; at: {x: number; y: number} | null}
  /** Run pressed (start), its Start menu answered (clock), its command ended (end) */
  | {type: 'run'; op: 'start' | 'clock' | 'end'; at: number}
  | {type: 'runStopped'}
  /** Clear: what is drawn now becomes the earlier branches (until the core's `autoview` says) */
  | {type: 'clear'}
  /** the core's display of the diagram (W65): the branches Clear hid, whether they are shown, the
      active view and each view's zoom; `show` and a view's `zoom` are absent while a change of the
      user's is on its way (session.ts) */
  | {type: 'autoview'; ev: AutoViewEvent}
  | {type: 'showEarlier'; show: boolean}
  /** a view clicked: the active one at once (the core's `autoview` says so too) */
  | {type: 'activate'; view: number};

export function pointCount(p: DiagramPoints): number {
  return p.x.length;
}

/** the active view (the first while the core has not said) */
export function activeView(s: Pick<DiagramState, 'views' | 'active'>): DiagramView {
  return s.views[s.active] ?? s.views[0];
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
      out.st.push(r.stable ? 1 : 0);
      out.pe.push(r.periodic ? 1 : 0);
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

function setViewport(v: DiagramView, viewport: Viewport): DiagramView {
  if (JSON.stringify(viewport) === JSON.stringify(v.viewport)) return v;
  return {...v, viewport};
}

/** the core drew the view at other axes (Axes, Fit, a zoom or scroll of
    its own): what it shows now is what the user asked for, so the zoom
    goes back to it */
function withAxes(v: DiagramView, ev: DiagramAxes): DiagramView {
  const axes = axesOf(ev);
  const moved = v.axes && !sameRanges(v.axes, axes) && (v.viewport.x !== null || v.viewport.y !== null);
  return moved ? setViewport({...v, axes}, HOME) : {...v, axes};
}

/** `views` with view k replaced by what `f` makes of it (a view the core names before its `views`
    event made, with empty ones up to it) */
function withView(views: DiagramView[], k: number, f: (v: DiagramView) => DiagramView): DiagramView[] {
  const out = views.slice();
  while (out.length <= k) out.push(emptyView());
  out[k] = f(out[k]);
  return out;
}

/** `n` views: the ones after the first n go, new ones start empty */
function resized(views: DiagramView[], n: number): DiagramView[] {
  const out = views.slice(0, Math.max(1, n));
  while (out.length < n) out.push(emptyView());
  return out;
}

function onEvent(s: DiagramState, ev: DiagramEvent): DiagramState {
  const events = s.events + 1;
  if (ev.op === 'views') {
    const views = resized(s.views, ev.n);
    const active = Math.min(s.active, views.length - 1);
    const hover = s.hover && s.hover.view < views.length ? s.hover : null;
    return {...s, views, active, hover, events};
  }
  const k = Math.max(0, ev.view);
  const held = s.views[k]?.points;
  switch (ev.op) {
    case 'axes':
      return {...s, views: withView(s.views, k, v => withAxes(v, ev)), events};
    case 'reset': {
      const keep = Math.max(0, ev.keep || 0);
      const views = withView(s.views, k, v => ({
        ...withAxes(v, ev), points: truncate(v.points, keep), labels: v.labels.filter(l => l.point < keep),
      }));
      const hover = s.hover && (s.hover.view !== k || s.hover.point < keep) ? s.hover : null;
      return {...s, views, hover, outOfStep: false, events};
    }
    case 'add': {
      if (ev.from > (held ? pointCount(held) : 0)) return {...s, outOfStep: true, events};
      const views = withView(s.views, k, v => ({...v, ...add(v.points, v.labels, ev.from, ev.runs)}));
      const hover = s.hover && (s.hover.view !== k || s.hover.point < ev.from) ? s.hover : null;
      return {...s, views, hover, events, outOfStep: ev.from === 0 ? false : s.outOfStep};
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
  return s.open || s.views.length > 1 || s.views[0].axes || pointCount(s.views[0].points)
    ? {...initialDiagram, events: s.events, infoEvents: s.infoEvents} : s;
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
    case 'viewport': {
      const v = s.views[a.view];
      if (!v) return s;
      const w = setViewport(v, a.viewport);
      return w === v ? s : {...s, views: withView(s.views, a.view, () => w)};
    }
    case 'hover': {
      const h = a.hover, o = s.hover;
      if (h === o || (h && o && h.point === o.point && h.low === o.low && h.view === o.view)) return s;
      return {...s, hover: h};
    }
    case 'info':
      return {...s, info: a.ev.info ?? null, stab: a.ev.stab ?? null, stop: a.ev.stop ?? null, infoEvents: s.infoEvents + 1};
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
      return {...s, earlier: pointCount(activeView(s).points), showEarlier: false, hover: null};
    case 'autoview': {
      let t: DiagramState = {...s, earlier: a.ev.earlier, hover: a.ev.earlier === s.earlier ? s.hover : null};
      if (a.ev.show !== undefined) t = {...t, showEarlier: a.ev.show !== 0};
      const zooms = a.ev.views ?? [];
      let views = zooms.length > t.views.length ? resized(t.views, zooms.length) : t.views;
      zooms.forEach((z, k) => {
        if (z.zoom) views = withView(views, k, v => setViewport(v, viewportOf(z.zoom!)));
      });
      const active = a.ev.active !== undefined && a.ev.active < views.length ? a.ev.active : t.active;
      return {...t, views, active};
    }
    case 'showEarlier':
      return a.show === s.showEarlier ? s : {...s, showEarlier: a.show};
    case 'activate':
      return a.view === s.active || a.view < 0 || a.view >= s.views.length ? s : {...s, active: a.view};
  }
}

function onRun(s: DiagramState, op: 'start' | 'clock' | 'end', at: number): DiagramState {
  if (op === 'start') return {...s, run: {active: true, started: at, ended: null, first: pointCount(activeView(s).points), stopped: false}};
  if (!s.run?.active) return s;
  if (op === 'clock') return {...s, run: {...s.run, started: at}};
  return {...s, run: {...s.run, active: false, ended: at}};
}

/** a command ended: no grab any more. (A diagram that holds fewer points than Clear hid, File/Reset
    diagram, has no earlier branches left: the core says so in its `autoview`.) */
export function diagramSettled(s: DiagramState): DiagramState {
  return reduceDiagram(s, {type: 'grabbing', on: false});
}

/** the points Clear hid, and whether they are shown */
export function earlierCount(s: Pick<DiagramState, 'earlier' | 'views' | 'active'>): number {
  return Math.min(s.earlier, pointCount(activeView(s).points));
}

/** the number of branches among the first `n` points */
export function branchesBefore(p: DiagramPoints, n: number): number {
  const seen = new Set<number>();
  for (let i = 0; i < n && i < p.br.length; i++) seen.add(p.br[i]);
  return seen.size;
}

/** the label of point `i`, if it has one */
export function labelAt(s: Pick<DiagramView, 'labels'>, i: number): DiagramLabel | undefined {
  return s.labels.find(l => l.point === i);
}
