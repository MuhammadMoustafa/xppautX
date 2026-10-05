/* The point under the mouse: the plotted point nearest to it in screen
   distance, over every visible curve. Pure; a linear scan, which is fast
   enough for the sizes XPP stores (a grid index can replace it later). */
import type {CurveData} from './model';

export interface Frame {
  xmin: number; xmax: number; ymin: number; ymax: number;
  /** plotting area in CSS pixels */
  width: number; height: number;
}

export interface Nearest {
  curve: number;
  index: number;
  /** distance in CSS pixels */
  dist: number;
}

/** Squared distance from the origin to a finite line segment in screen pixels. */
export function segmentDistance2(ax: number, ay: number, bx: number, by: number): number {
  const dx = bx - ax, dy = by - ay, length = dx * dx + dy * dy;
  const t = length ? Math.max(0, Math.min(1, -(ax * dx + ay * dy) / length)) : 0;
  return (ax + t * dx) ** 2 + (ay + t * dy) ** 2;
}

export function nearestPoint(curves: CurveData[], visible: boolean[], frame: Frame, px: number, py: number,
  maxDist = Infinity, alongLines = false): Nearest | null {
  const sx = frame.width / (frame.xmax - frame.xmin), sy = frame.height / (frame.ymax - frame.ymin);
  let best: Nearest | null = null, bestD2 = maxDist * maxDist;
  curves.forEach((c, k) => {
    if (!visible[k]) return;
    for (let i = 0; i < c.xs.length; i++) {
      const dx = (c.xs[i] - frame.xmin) * sx - px, dy = (frame.ymax - c.ys[i]) * sy - py;
      let d2 = dx * dx + dy * dy;
      if (alongLines && c.line && i > 0) {
        const ax = (c.xs[i - 1] - frame.xmin) * sx - px, ay = (frame.ymax - c.ys[i - 1]) * sy - py;
        const segment = segmentDistance2(ax, ay, dx, dy);
        if (segment < d2) d2 = segment;
      }
      if (d2 < bestD2) {
        bestD2 = d2;
        best = {curve: k, index: i, dist: 0};
      }
    }
  });
  if (best) (best as Nearest).dist = Math.sqrt(bestD2);
  return best;
}
