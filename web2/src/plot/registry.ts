/* Each plot window's chart, for the test hook and the export buttons. */
import type {Chart} from './chart';

const charts = new Map<number, Chart>();

export function setChart(win: number, c: Chart | null): void {
  if (c) charts.set(win, c);
  else charts.delete(win);
}

export function chartOf(win: number): Chart | null {
  return charts.get(win) ?? null;
}
