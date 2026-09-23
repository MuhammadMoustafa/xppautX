/* The plot view's chart, for the test hook and the export buttons. */
import type {Chart} from './chart';

let current: Chart | null = null;

export function setCurrentChart(c: Chart | null): void {
  current = c;
}

export function currentChart(): Chart | null {
  return current;
}
