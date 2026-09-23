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

export function nearestPoint(curves: CurveData[], visible: boolean[], frame: Frame, px: number, py: number,
  maxDist = Infinity): Nearest | null {
  const sx = frame.width / (frame.xmax - frame.xmin), sy = frame.height / (frame.ymax - frame.ymin);
  let best: Nearest | null = null, bestD2 = maxDist * maxDist;
  curves.forEach((c, k) => {
    if (!visible[k]) return;
    for (let i = 0; i < c.xs.length; i++) {
      const dx = (c.xs[i] - frame.xmin) * sx - px, dy = (frame.ymax - c.ys[i]) * sy - py;
      const d2 = dx * dx + dy * dy;
      if (d2 < bestD2) {
        bestD2 = d2;
        best = {curve: k, index: i, dist: 0};
      }
    }
  });
  if (best) (best as Nearest).dist = Math.sqrt(bestD2);
  return best;
}
