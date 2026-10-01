/* A plot window's earlier runs (docs/ui-v2.md, GitHub #18): the classic
   XPP draws every run over the ones before until Erase, so a new run keeps
   the one it replaces, drawn lighter under it. The core holds them
   (W65: core/plot_data.cpp decides when a run becomes an earlier one, caps
   them in number and rows, and forgets them at Erase) and sends `runs`
   events; this is the page's copy of what the core says. Pure: no DOM, no
   I/O.

   - `clear` forgets the runs held, `drop` the oldest ones, `keep` puts the
     window's own series last (a new run replaces it: the core does not send
     its data again), `add` puts new ones last;
   - `erased`: Erase blanked the window, the current series is not drawn
     until the core says otherwise (its next run, or Redraw). */
import type {RunsEvent} from '../protocol/types';
import {decode} from '../protocol/decode';
import {columnStats, type PlotSeries} from './series';

export interface RunHistory {
  /** earlier runs, oldest first */
  runs: PlotSeries[];
  /** Erase blanked the window: the current series is not drawn */
  erased: boolean;
}

export const emptyHistory: RunHistory = {runs: [], erased: false};

/** the earlier run an event carries, as a series of window `win` */
function runSeries(win: number, r: RunsEvent['add'][number]): PlotSeries {
  const columns = new Map<number, Float32Array>(), names = new Map<number, string>();
  for (const c of r.columns) {
    columns.set(c.col, decode(c.data));
    names.set(c.col, c.name);
  }
  return {
    win, rows: r.rows, three: r.three !== 0, labels: {x: '', y: '', z: ''}, curves: r.curves, shift: r.shift,
    columns, names, buffers: new Map(columns), version: null,
    stats: new Map([...columns].map(([c, a]) => [c, columnStats(a, 0, a.length)])),
  };
}

/** the series the window holds as an earlier run (`keep`): its own data, as the core kept it */
function heldRun(s: PlotSeries): PlotSeries {
  return {...s, labels: {x: '', y: '', z: ''}, version: null};
}

/** `h` after the core's `runs` event; `current` is the series the window
    holds, which `keep` makes the newest earlier run */
export function onRuns(h: RunHistory, ev: RunsEvent, current: PlotSeries | null): RunHistory {
  const kept = ev.clear ? [] : h.runs.slice(ev.drop);
  const held = ev.keep && current ? [heldRun(current)] : [];
  const added = [...held, ...ev.add.map(r => runSeries(ev.win, r))];
  const runs = added.length ? [...kept, ...added] : kept;
  const erased = ev.erased !== 0;
  return runs.length === h.runs.length && runs.every((r, i) => r === h.runs[i]) && erased === h.erased
    ? h : {runs, erased};
}
