/* The plot windows (docs/protocol.md "The plot as data", docs/ui-v2.md T6):
   one entry per core window, each with its own series and its own zoom, so
   switching tabs keeps what every window shows. The list follows the
   core's `plots` event (a window missing from it was destroyed); a `series`
   for a window not listed yet adds it, for a server that sends no `plots`.
   `active` is the core's active window, which is the selected tab: the
   user's choice sets it at once, the core's `plots` and `window select`
   confirm it. A window's nullclines, direction field and flows (T7) and
   its marks (T8) come from their own events and are kept beside its
   series. Pure: no DOM, no I/O. */
import type {DfieldEvent, MarksEvent, NullclinesEvent, PlotsEvent, PlotWindowInfo, RunsEvent, SeriesAppendEvent, SeriesEndEvent, SeriesEvent} from '../protocol/types';
import {marksFromEvent, type Marks} from './marks';
import {dfieldFromEvent, nullclinesFromEvent, type Dfield, type Nullclines} from './phase';
import {emptyHistory, onRuns, type RunHistory} from './runs';
import {appendRows, endSeries, seriesFromEvent, type PlotSeries} from './series';

export interface Range {
  min: number;
  max: number;
}

/** What a plot shows: null ranges mean the core's view (Viewaxes/Window) */
export interface Viewport {
  x: Range | null;
  y: Range | null;
}

export const HOME: Viewport = {x: null, y: null};

/** a 3D plot's angles (docs/ui-v2.md T14): set from the core's `theta`,
    `phi` the first time a window is seen, then owned by the client (a
    drag or arrow keys turn it at once; the key `3` tells the core where it
    settled, throttled, see session.ts), so it does not fight the core's
    own echo of the same numbers. */
export interface View3d {
  theta: number;
  phi: number;
}

export interface PlotWindow {
  win: number;
  /** from `plots`: title, axes, 3D view (null until the server sends it) */
  info: PlotWindowInfo | null;
  series: PlotSeries | null;
  /** from `nullclines` and `dfield` (null until the server sends them) */
  nullclines: Nullclines | null;
  dfield: Dfield | null;
  /** from `marks`: equilibria, labels, arrows, markers, frozen curves (null until sent) */
  marks: Marks | null;
  viewport: Viewport;
  /** a 3D window's own angles (null: not seen yet, or not 3D) */
  view3d: View3d | null;
  /** earlier runs drawn under the current one, and whether Erase hid it (store/runs.ts) */
  history: RunHistory;
  /** the legend's "previous runs" toggle */
  showRuns: boolean;
}

export interface PlotsState {
  /** by window number */
  windows: PlotWindow[];
  active: number;
}

export const initialPlots: PlotsState = {windows: [], active: 1};

/** a window the store has not heard anything about yet (exported for tests) */
export function blank(win: number): PlotWindow {
  return {
    win, info: null, series: null, nullclines: null, dfield: null, marks: null, viewport: HOME,
    view3d: null, history: emptyHistory, showRuns: true,
  };
}

export function windowOf(p: PlotsState, win: number): PlotWindow | undefined {
  return p.windows.find(w => w.win === win);
}

export function activeWindow(p: PlotsState): PlotWindow | undefined {
  return windowOf(p, p.active);
}

/** `p` with window `win` replaced by `f` of it (made if missing) */
function update(p: PlotsState, win: number, f: (w: PlotWindow) => PlotWindow): PlotsState {
  const old = windowOf(p, win);
  const next = f(old ?? blank(win));
  if (next === old) return p;
  const windows = old ? p.windows.map(w => (w === old ? next : w)) : [...p.windows, next].sort((a, b) => a.win - b.win);
  return {...p, windows};
}

function sameCurves(a: PlotSeries | null, b: PlotSeries): boolean {
  return !!a && JSON.stringify(a.curves) === JSON.stringify(b.curves);
}

/** a zoom as the core sends it ([low, high] or null per axis) */
export function viewportOf(z: NonNullable<PlotWindowInfo['zoom']>): Viewport {
  const r = (a: [number, number] | null): Range | null => (a ? {min: a[0], max: a[1]} : null);
  return {x: r(z.x), y: r(z.y)};
}

/** the windows the core has now: kept ones keep their series and, once
    set, their own 3D angles (the client's to turn from here on). The zoom
    shown and the earlier runs' toggle are the core's (W65): a window's own
    are taken from what it says (session.ts leaves them out while a change
    of the user's is on its way) */
export function onPlots(p: PlotsState, ev: PlotsEvent): PlotsState {
  const windows = ev.windows.map(info => {
    const w = windowOf(p, info.win) ?? blank(info.win);
    const viewport = info.zoom ? viewportOf(info.zoom) : w.viewport;
    return {
      ...w, info, viewport: sameViewport(viewport, w.viewport) ? w.viewport : viewport,
      showRuns: info.runs === undefined ? w.showRuns : info.runs !== 0,
      view3d: w.view3d ?? (info.three ? {theta: info.theta, phi: info.phi} : null),
    };
  });
  return {windows, active: ev.active};
}

/** a 3D window turned, locally (a drag or arrow keys) or by the core's own
    echo of the turn it sent (session.ts): the store is what the plot
    draws from, so this is the single point a rotation changes it. */
export function rotate3d(p: PlotsState, win: number, theta: number, phi: number): PlotsState {
  return update(p, win, w => ({...w, view3d: {theta, phi}}));
}

export function onSeries(p: PlotsState, ev: SeriesEvent): PlotsState {
  const series = seriesFromEvent(ev);
  return update(p, ev.win, w => {
    /* other curves: the user's zoom does not apply to them */
    const keep = sameCurves(w.series, series);
    return {...w, series, viewport: keep ? w.viewport : HOME};
  });
}

/** the core's earlier runs of window `win` changed (Erase and Redraw included) */
export function onWindowRuns(p: PlotsState, ev: RunsEvent): PlotsState {
  return update(p, ev.win, w => {
    const history = onRuns(w.history, ev);
    return history === w.history ? w : {...w, history};
  });
}

/** the legend's "previous runs" toggle */
export function showRuns(p: PlotsState, win: number, show: boolean): PlotsState {
  const w = windowOf(p, win);
  if (!w || w.showRuns === show) return p;
  return update(p, win, x => ({...x, showRuns: show}));
}

export function onNullclines(p: PlotsState, ev: NullclinesEvent): PlotsState {
  const nullclines = nullclinesFromEvent(ev);
  return update(p, ev.win, w => ({...w, nullclines}));
}

export function onDfield(p: PlotsState, ev: DfieldEvent): PlotsState {
  const dfield = dfieldFromEvent(ev);
  return update(p, ev.win, w => ({...w, dfield}));
}

export function onMarks(p: PlotsState, ev: MarksEvent): PlotsState {
  const marks = marksFromEvent(ev);
  return update(p, ev.win, w => ({...w, marks}));
}

/** null when the append does not continue the window's series (the full series that ends the command puts that right) */
export function onAppend(p: PlotsState, ev: SeriesAppendEvent): PlotsState | null {
  const w = windowOf(p, ev.win);
  const series = w?.series && appendRows(w.series, ev);
  if (!w || !series) return null;
  return update(p, ev.win, x => ({...x, series}));
}

/** null when the end does not match the window's series (its rows): the
    appends before it did not continue it */
export function onEnd(p: PlotsState, ev: SeriesEndEvent): PlotsState | null {
  const w = windowOf(p, ev.win);
  if (!w?.series || w.series.rows !== ev.rows) return null;
  return update(p, ev.win, x => ({...x, series: endSeries(x.series!, ev.version)}));
}

export function select(p: PlotsState, win: number): PlotsState {
  return p.active === win ? p : {...p, active: win};
}

function sameViewport(a: Viewport, b: Viewport): boolean {
  return JSON.stringify(a) === JSON.stringify(b);
}

/** no history any more (GitHub #110): a new viewport just replaces the old one */
export function setViewport(p: PlotsState, win: number, viewport: Viewport): PlotsState {
  const w = windowOf(p, win);
  if (!w || sameViewport(viewport, w.viewport)) return p;
  return update(p, win, x => ({...x, viewport}));
}

/** the core moved window `win`'s axes (Viewaxes, Window/Zoom, Fit, a scroll):
    what it shows now is what the user asked for, so the plot goes back to them */
export function coreMoved(p: PlotsState, win: number): PlotsState {
  const w = windowOf(p, win);
  if (!w || (w.viewport.x === null && w.viewport.y === null)) return p;
  return setViewport(p, win, HOME);
}
