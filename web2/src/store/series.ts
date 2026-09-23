/* The plot's data as the store keeps it: the series event's columns as typed
   arrays (NaN where the core sent null), looked up by storage column. */
import type {Curve, SeriesEvent} from '../protocol/types';

export interface PlotSeries {
  win: number;
  rows: number;
  three: boolean;
  labels: {x: string; y: string; z: string};
  curves: Curve[];
  shift: [number, number, number];
  /** storage column -> values, one per row */
  columns: Map<number, Float64Array>;
  names: Map<number, string>;
}

export function seriesFromEvent(ev: SeriesEvent): PlotSeries {
  const columns = new Map<number, Float64Array>();
  const names = new Map<number, string>();
  for (const c of ev.columns) {
    const a = new Float64Array(c.data.length);
    for (let i = 0; i < a.length; i++) a[i] = c.data[i] ?? NaN;
    columns.set(c.col, a);
    names.set(c.col, c.name);
  }
  return {
    win: ev.win,
    rows: ev.rows,
    three: ev.three !== 0,
    labels: {x: ev.xlabel, y: ev.ylabel, z: ev.zlabel},
    curves: ev.curves,
    shift: ev.shift,
    columns,
    names,
  };
}

export function columnName(s: PlotSeries, col: number): string {
  return s.names.get(col) ?? (col === 0 ? 'T' : `#${col}`);
}

/** "W vs V": what a curve plots, as XPP titles a window */
export function curveLabel(s: PlotSeries, c: Curve): string {
  return `${columnName(s, c.y)} vs ${columnName(s, c.x)}`;
}
