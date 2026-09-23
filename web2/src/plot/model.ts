/* From the store's series to what the plot draws: one x/y pair of arrays per
   curve, and whether the curves share one increasing x (a time plot, uPlot's
   aligned mode 1) or not (a phase plane, uPlot's xy mode 2). Pure. */
import {columnName, curveLabel, type PlotSeries} from '../store/series';

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
}

const EMPTY = new Float32Array(0);

function increasing(a: Float32Array): boolean {
  for (let i = 1; i < a.length; i++) if (!(a[i] >= a[i - 1])) return false;
  return true;
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
      xName: columnName(s, c.x),
      yName: columnName(s, c.y),
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
  const mode = shared && curves.length > 0 && increasing(erased ? (s.columns.get(first.x) ?? EMPTY) : curves[0].xs) ? 1 : 2;
  const xLabel = s.labels.x || (first ? columnName(s, first.x) : '');
  const yLabel = s.labels.y || (s.curves.length === 1 && first ? columnName(s, first.y) : '');
  return {mode, curves, xLabel, yLabel, t: s.columns.get(0) ?? null};
}
