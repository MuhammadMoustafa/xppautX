/* Each plot window's chart, for the test hook and the export buttons: a 2D
   one (uPlot, plot/chart.ts) or, for a 3D window, plot/chart3d.ts. Both
   have an `info()` the test hook reads (testhook.ts); nothing else here
   needs to tell them apart. */
import type {Chart} from './chart';
import type {Chart3D} from './chart3d';

const charts = new Map<number, Chart | Chart3D>();

export function setChart(win: number, c: Chart | Chart3D | null): void {
  if (c) charts.set(win, c);
  else charts.delete(win);
}

export function chartOf(win: number): Chart | Chart3D | null {
  return charts.get(win) ?? null;
}
