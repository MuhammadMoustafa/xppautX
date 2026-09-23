/* A plot window's earlier runs (docs/ui-v2.md, GitHub #18): the classic
   XPP draws every run over the ones before until Erase, so a new run keeps
   the one it replaces, drawn lighter under it. The core sends only the
   current data; the history is the client's. Pure: no DOM, no I/O.

   - a full `series` with other curves (Xi vs t, Viewaxes, ...) is another
     picture: the history goes;
   - a full `series` of other data (its `version` differs) keeps the
     current one as a previous run, unless the current one is being
     extended in this command (appends: the full series ends that run) or
     was erased;
   - an append that starts again before the rows the client holds (a new
     run under way, or the next run of a range) keeps the current one first;
   - `erase` (the Erase command) forgets the history and hides the current
     data until the next run or Redraw; `redraw` shows the current data
     again, without the history (the Redraw command). */
import type {PlotSeries} from './series';

/** at most this many previous runs per window, oldest dropped */
export const RUNS_KEEP = 50;
/** and at most this many rows in all of them, so memory stays bounded */
export const RUNS_MAX_ROWS = 4_000_000;

export interface RunHistory {
  /** previous runs, oldest first, each trimmed to its own rows */
  runs: PlotSeries[];
  /** Erase blanked the window: the current series is not drawn */
  erased: boolean;
  /** the current series grew by appends in this command: its full series ends it */
  live: boolean;
}

export const emptyHistory: RunHistory = {runs: [], erased: false, live: false};

function sameCurves(a: PlotSeries, b: PlotSeries): boolean {
  return a.three === b.three && JSON.stringify(a.curves) === JSON.stringify(b.curves)
    && a.shift.every((v, i) => v === b.shift[i]);
}

/** `s` with columns of exactly its rows (an append's buffers have room to spare) */
function trimmed(s: PlotSeries): PlotSeries {
  const columns = new Map<number, Float32Array>();
  for (const [col, a] of s.columns) columns.set(col, a.buffer.byteLength === a.byteLength ? a : a.slice());
  return {...s, columns, buffers: columns};
}

/** `runs` plus `s`, within RUNS_KEEP and RUNS_MAX_ROWS */
export function keepRun(runs: PlotSeries[], s: PlotSeries): PlotSeries[] {
  if (s.rows <= 0) return runs;
  const next = [...runs, trimmed(s)];
  let rows = next.reduce((n, r) => n + r.rows, 0), drop = Math.max(0, next.length - RUNS_KEEP);
  for (let i = 0; i < drop; i++) rows -= next[i].rows;
  while (drop < next.length - 1 && rows > RUNS_MAX_ROWS) rows -= next[drop++].rows;
  return drop ? next.slice(drop) : next;
}

/** a full series `next` replaces `cur` */
export function onFull(h: RunHistory, cur: PlotSeries | null, next: PlotSeries): RunHistory {
  if (!cur || !sameCurves(cur, next)) return h.runs.length || h.erased || h.live ? emptyHistory : h;
  if (h.erased || h.live) return {runs: h.runs, erased: false, live: false};
  const other = next.version === null || cur.version === null || next.version !== cur.version;
  return other && cur.rows > 0 ? {...h, runs: keepRun(h.runs, cur)} : h;
}

/** an append from row `from` continues `cur` */
export function onAppendFrom(h: RunHistory, cur: PlotSeries, from: number): RunHistory {
  if (from < cur.rows) /* a new run (or the next of a range): the rows it replaces become a previous run */
    return {runs: h.erased ? h.runs : keepRun(h.runs, cur), erased: false, live: true};
  return h.live && !h.erased ? h : {...h, erased: false, live: true};
}

export function onErase(): RunHistory {
  return {runs: [], erased: true, live: false};
}

export function onRedraw(h: RunHistory): RunHistory {
  return h.runs.length || h.erased ? {...h, runs: [], erased: false} : h;
}
