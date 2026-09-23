/* The plot's data as the store keeps it: the series event's columns as
   float32 arrays, as the core stores them (NaN where the core sent null),
   looked up by storage column.

   A live integration sends its rows in appends (docs/protocol.md "The plot
   as data"). Each column lives in a buffer with room to spare that grows by
   doubling, and an append writes its rows in place past the rows already
   there, so a run of n rows costs O(n) copying in all, not O(n) per append.
   The series a state holds never changes: its columns are views that end at
   its own row count, and rows an older state can see are never rewritten
   (an append that starts before the end gets new buffers). */
import {decode, decodeInto, valueCount} from '../protocol/decode';
import type {Curve, SeriesAppendEvent, SeriesEvent} from '../protocol/types';

export interface PlotSeries {
  win: number;
  rows: number;
  three: boolean;
  labels: {x: string; y: string; z: string};
  curves: Curve[];
  shift: [number, number, number];
  /** storage column -> values, one per row */
  columns: Map<number, Float32Array>;
  names: Map<number, string>;
  /** the arrays behind `columns`, at least `rows` long: appends fill them in place */
  buffers: Map<number, Float32Array>;
}

export function seriesFromEvent(ev: SeriesEvent): PlotSeries {
  const columns = new Map<number, Float32Array>();
  const names = new Map<number, string>();
  for (const c of ev.columns) {
    columns.set(c.col, decode(c.data));
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
    buffers: new Map(columns),
  };
}

const MIN_CAPACITY = 1024;

/** `s` with the rows of an append, or null when the append does not continue
    `s` (another window, other columns, a gap after its rows): the full
    series that ends the command puts that right */
export function appendRows(s: PlotSeries, ev: SeriesAppendEvent): PlotSeries | null {
  const from = ev.from, rows = ev.rows;
  if (ev.win !== s.win || !(from >= 0 && from <= s.rows) || ev.columns.length !== s.columns.size) return null;
  if (!ev.columns.every(c => s.buffers.has(c.col) && valueCount(c.data) === rows - from)) return null;
  const columns = new Map<number, Float32Array>(), buffers = new Map<number, Float32Array>();
  for (const c of ev.columns) {
    let buf = s.buffers.get(c.col)!;
    if (from < s.rows || rows > buf.length) {
      /* no room, or rows `s` shows would be rewritten: a new buffer (a run
         that starts again starts small, a growing one doubles) */
      const next = new Float32Array(Math.max(MIN_CAPACITY, rows, from < s.rows ? 2 * rows : 2 * buf.length));
      next.set(buf.subarray(0, from));
      buf = next;
    }
    decodeInto(c.data, buf, from);
    buffers.set(c.col, buf);
    columns.set(c.col, buf.subarray(0, rows));
  }
  return {...s, rows, columns, buffers};
}

export function columnName(s: PlotSeries, col: number): string {
  return s.names.get(col) ?? (col === 0 ? 'T' : `#${col}`);
}

/** "W vs V": what a curve plots, as XPP titles a window */
export function curveLabel(s: PlotSeries, c: Curve): string {
  return `${columnName(s, c.y)} vs ${columnName(s, c.x)}`;
}
