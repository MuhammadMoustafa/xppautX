/* The plot windows (docs/protocol.md "The plot as data", docs/ui-v2.md T6):
   one entry per core window, each with its own series and its own zoom, so
   switching tabs keeps what every window shows. The list follows the
   core's `plots` event (a window missing from it was destroyed); a `series`
   for a window not listed yet adds it, for a server that sends no `plots`.
   `active` is the core's active window, which is the selected tab: the
   user's choice sets it at once, the core's `plots` and `window select`
   confirm it. Pure: no DOM, no I/O. */
import type {PlotsEvent, PlotWindowInfo, SeriesAppendEvent, SeriesEvent} from '../protocol/types';
import {appendRows, seriesFromEvent, type PlotSeries} from './series';

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

export interface PlotWindow {
  win: number;
  /** from `plots`: title, axes, 3D view (null until the server sends it) */
  info: PlotWindowInfo | null;
  series: PlotSeries | null;
  viewport: Viewport;
  /** earlier viewports, for Undo zoom (newest last) */
  viewportHistory: Viewport[];
}

export interface PlotsState {
  /** by window number */
  windows: PlotWindow[];
  active: number;
}

export const initialPlots: PlotsState = {windows: [], active: 1};

const HISTORY_KEEP = 50;

function blank(win: number): PlotWindow {
  return {win, info: null, series: null, viewport: HOME, viewportHistory: []};
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

/** the windows the core has now: kept ones keep their series and zoom */
export function onPlots(p: PlotsState, ev: PlotsEvent): PlotsState {
  const windows = ev.windows.map(info => ({...(windowOf(p, info.win) ?? blank(info.win)), info}));
  return {windows, active: ev.active};
}

export function onSeries(p: PlotsState, ev: SeriesEvent): PlotsState {
  const series = seriesFromEvent(ev);
  return update(p, ev.win, w => {
    /* other curves: the user's zoom does not apply to them */
    const keep = sameCurves(w.series, series);
    return {...w, series, viewport: keep ? w.viewport : HOME, viewportHistory: keep ? w.viewportHistory : []};
  });
}

/** null when the append does not continue the window's series (the full series that ends the command puts that right) */
export function onAppend(p: PlotsState, ev: SeriesAppendEvent): PlotsState | null {
  const w = windowOf(p, ev.win);
  const series = w?.series && appendRows(w.series, ev);
  if (!w || !series) return null;
  return update(p, ev.win, x => ({...x, series}));
}

export function select(p: PlotsState, win: number): PlotsState {
  return p.active === win ? p : {...p, active: win};
}

function sameViewport(a: Viewport, b: Viewport): boolean {
  return JSON.stringify(a) === JSON.stringify(b);
}

/** push: remember the viewport it replaces (the start of a gesture), for undo */
export function setViewport(p: PlotsState, win: number, viewport: Viewport, push = false): PlotsState {
  const w = windowOf(p, win);
  if (!w || sameViewport(viewport, w.viewport)) return p;
  const history = push ? [...w.viewportHistory, w.viewport].slice(-HISTORY_KEEP) : w.viewportHistory;
  return update(p, win, x => ({...x, viewport, viewportHistory: history}));
}

export function undoViewport(p: PlotsState, win: number): PlotsState {
  const w = windowOf(p, win);
  if (!w || !w.viewportHistory.length) return p;
  return update(p, win, x => ({
    ...x,
    viewport: x.viewportHistory[x.viewportHistory.length - 1],
    viewportHistory: x.viewportHistory.slice(0, -1),
  }));
}

/** the core moved window `win`'s axes (Viewaxes, Window/Zoom, Fit, a scroll):
    what it shows now is what the user asked for, so the plot goes back to
    them (the zoom it had stays one Undo away) */
export function coreMoved(p: PlotsState, win: number): PlotsState {
  const w = windowOf(p, win);
  if (!w || (w.viewport.x === null && w.viewport.y === null)) return p;
  return setViewport(p, win, HOME, true);
}
