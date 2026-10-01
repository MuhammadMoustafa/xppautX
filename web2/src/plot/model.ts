/* From the store's series to what the plot draws: one x/y pair of arrays per
   curve, and whether the curves share one increasing x (a time plot, uPlot's
   aligned mode 1) or not (a phase plane, uPlot's xy mode 2). Pure. */
import {seriesColumnName, columnStats, curveLabel, type ColumnStats, type PlotSeries} from '../store/series';
import type {Range} from '../store/plots';

export interface CurveData {
  label: string;
  xName: string;
  yName: string;
  color: number;
  /** a line, or points of this radius */
  line: boolean;
  radius: number;
  xs: Float32Array;
  ys: Float32Array;
  /** storage row of xs[0] (rows before the largest shift are not plotted) */
  row0: number;
}

export interface PlotModel {
  mode: 1 | 2;
  curves: CurveData[];
  xLabel: string;
  yLabel: string;
  /** the time column, row by row, for the hover readout (null when not sent) */
  t: Float32Array | null;
  /** the least and greatest x and y the curves plot (null: none), for the chart's own axes */
  xRange: Range | null;
  yRange: Range | null;
}


const EMPTY = new Float32Array(0);

/** the stats of what a curve plots: the column's own (kept up by the store, no scan) when the
    curve plots it whole, else its rows scanned */
function statsOf(s: PlotSeries, col: number, plotted: Float32Array): ColumnStats {
  const own = s.stats.get(col);
  return own && plotted.length === s.columns.get(col)?.length ? own : columnStats(plotted, 0, plotted.length);
}

/** the range of `stats` together, null for none */
function rangeOf(stats: ColumnStats[]): Range | null {
  let min = Infinity, max = -Infinity;
  for (const t of stats) {
    if (t.min < min) min = t.min;
    if (t.max > max) max = t.max;
  }
  return min <= max ? {min, max} : null;
}

/** `erased`: the window's curves, with no points (Erase, until the next run or Redraw) */
export function buildModel(s: PlotSeries, erased = false): PlotModel {
  const [xs, ys] = s.shift;
  const start = Math.max(xs, ys, 0);
  const curves: CurveData[] = s.curves.map(c => {
    const x = s.columns.get(c.x) ?? EMPTY, y = s.columns.get(c.y) ?? EMPTY;
    const n = Math.max(0, Math.min(x.length, y.length) - start);
    return {
      label: curveLabel(s, c),
      xName: seriesColumnName(s, c.x),
      yName: seriesColumnName(s, c.y),
      color: c.color,
      line: c.line > 0,
      radius: c.line > 0 ? 0 : Math.max(1, -c.line),
      xs: erased ? EMPTY : x.subarray(start - xs, start - xs + n),
      ys: erased ? EMPTY : y.subarray(start - ys, start - ys + n),
      row0: start,
    };
  });
  const first = s.curves[0];
  const shared = !!first && start === 0 && s.curves.every(c => c.x === first.x);
  const xStats = s.curves.map((c, i) => statsOf(s, c.x, curves[i].xs));
  const firstX = erased && first ? (s.columns.get(first.x) ?? EMPTY) : null;
  const mode = shared && curves.length > 0
    && (firstX ? statsOf(s, first.x, firstX) : xStats[0]).increasing ? 1 : 2;
  const xLabel = s.labels.x || (first ? seriesColumnName(s, first.x) : '');
  const yLabel = s.labels.y || (s.curves.length === 1 && first ? seriesColumnName(s, first.y) : '');
  return {mode, curves, xLabel, yLabel, t: s.columns.get(0) ?? null,
    xRange: rangeOf(xStats), yRange: rangeOf(s.curves.map((c, i) => statsOf(s, c.y, curves[i].ys)))};
}
