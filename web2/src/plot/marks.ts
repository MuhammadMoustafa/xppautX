/* Marks on the plot (docs/ui-v2.md T8): which layers a window's marks make,
   named as the legend shows them, and their geometry on the chart's canvas
   in canvas pixels. The chart (chart.ts) draws what these return: frozen
   curves under the curves, the rest over them. Shapes keep XPP's (its
   equilibrium symbols: a circle stable, a box unstable, a triangle a
   saddle; its six markers), at a size in pixels rather than a fraction of
   the axes, so they stay readable at any zoom. Pure: no DOM. */
import type {PixelFrame} from './decimate';
import {pixelMap, type PathSink} from './phase';
import type {ArrowMark, EquilibriumMark, Marks, MarkerMark, MarkerShape} from '../store/marks';

export type MarkKey = 'equilibria' | 'text' | 'arrows' | 'markers' | `frozen-${number}`;

export interface MarkLayer {
  key: MarkKey;
  /** the legend's name: "Equilibria", "Text", "Arrows", "Markers", a frozen curve's key */
  label: string;
  /** XPP colour index */
  color: number;
  /** marks, or a frozen curve's points */
  count: number;
}

/** the layers `m` has, in legend order; a kind with no marks is left out */
export function markLayers(m: Marks | null): MarkLayer[] {
  if (!m) return [];
  const out: MarkLayer[] = [];
  if (m.equilibria.length) out.push({key: 'equilibria', label: 'Equilibria', color: 0, count: m.equilibria.length});
  if (m.text.length) out.push({key: 'text', label: 'Text', color: 0, count: m.text.length});
  if (m.arrows.length) out.push({key: 'arrows', label: 'Arrows', color: m.arrows[0].color, count: m.arrows.length});
  if (m.markers.length) out.push({key: 'markers', label: 'Markers', color: m.markers[0].color, count: m.markers.length});
  m.frozen.forEach((f, i) => out.push({key: `frozen-${i}`, label: f.label, color: f.color, count: f.xs.length}));
  return out;
}

/** a regular polygon of n corners about (x, y), the first at angle a0 (canvas pixels, y down) */
function polygon(sink: PathSink, x: number, y: number, r: number, n: number, a0: number): void {
  for (let k = 0; k <= n; k++) {
    const a = a0 + (2 * Math.PI * k) / n, px = x + r * Math.cos(a), py = y - r * Math.sin(a);
    if (k) sink.lineTo(px, py);
    else sink.moveTo(px, py);
  }
}

/** an equilibrium's symbol, `r` pixels from its centre to its edge; the
    centre itself is a dot the caller adds */
export function equilibriumPath(e: EquilibriumMark, f: PixelFrame, r: number, sink: PathSink): boolean {
  const m = pixelMap(f), x = m.x(e.x), y = m.y(e.y);
  if (!Number.isFinite(x) || !Number.isFinite(y)) return false;
  if (e.type === 'stable') polygon(sink, x, y, r, 24, 0);
  else if (e.type === 'saddle') polygon(sink, x, y, 1.4 * r, 3, Math.PI / 2);
  else polygon(sink, x, y, r * Math.SQRT2, 4, Math.PI / 4);
  return true;
}

/* XPP's markers (grobs.cpp draw_marker) in units of a sixth of their half
   size, y up: pen moves then lines */
const MARKER_STROKES: Record<MarkerShape, [number, number][][]> = {
  box: [[[-6, -6], [6, -6], [6, 6], [-6, 6], [-6, -6]]],
  diamond: [[[8, 0], [0, -8], [-8, 0], [0, 8], [8, 0]]],
  triangle: [[[-6, -6], [6, -6], [0, 6], [-6, -6]]],
  plus: [[[-6, 0], [6, 0]], [[0, -6], [0, 6]]],
  cross: [[[-6, 6], [6, -6]], [[-6, -6], [6, 6]]],
  circle: [[]],
};

/** pixels from a marker's centre to its edge at size 1 */
export const MARKER_PX = 4;

/** a marker's outline, `px` pixels to its edge at size 1 */
export function markerPath(k: MarkerMark, f: PixelFrame, px: number, sink: PathSink): boolean {
  const m = pixelMap(f), x = m.x(k.x), y = m.y(k.y), u = (px * (k.size > 0 ? k.size : 1)) / 6;
  if (!Number.isFinite(x) || !Number.isFinite(y)) return false;
  if (k.shape === 'circle') {
    polygon(sink, x, y, 6 * u, 24, 0);
    return true;
  }
  for (const stroke of MARKER_STROKES[k.shape])
    stroke.forEach(([dx, dy], i) => (i ? sink.lineTo(x + dx * u, y - dy * u) : sink.moveTo(x + dx * u, y - dy * u)));
  return true;
}

/** an arrow or pointer as XPP draws it (grobs.cpp arrow_head): the head's
    tip at (x1, y1), its two strokes back to a `size` part of the way to
    (x2, y2), spread half as wide as they are long; a pointer's shaft
    too. The head is worked out on screen, so it keeps its shape whatever
    the axes' scales */
export function arrowPath(a: ArrowMark, f: PixelFrame, sink: PathSink): boolean {
  const m = pixelMap(f), x1 = m.x(a.x1), y1 = m.y(a.y1), x2 = m.x(a.x2), y2 = m.y(a.y2);
  if (![x1, y1, x2, y2].every(Number.isFinite)) return false;
  const lx = x2 - x1, ly = y2 - y1;
  const bx = x1 + a.size * lx, by = y1 + a.size * ly;
  const px = -0.5 * a.size * ly, py = 0.5 * a.size * lx;
  if (a.pointer) {
    sink.moveTo(x1, y1);
    sink.lineTo(x2, y2);
  }
  sink.moveTo(bx + px, by + py);
  sink.lineTo(x1, y1);
  sink.lineTo(bx - px, by - py);
  return true;
}

/** a frozen curve on `f`: a polyline (line) or a dot of radius `dot` per
    point, leaving out points on the pixel of the one before; returns the
    points drawn */
export function traceFrozen(xs: Float32Array, ys: Float32Array, line: boolean, f: PixelFrame, dot: number,
  sink: PathSink): number {
  const m = pixelMap(f), n = Math.min(xs.length, ys.length);
  let pen = false, drawn = 0, lx = NaN, ly = NaN;
  for (let i = 0; i < n; i++) {
    const x = m.x(xs[i]), y = m.y(ys[i]);
    if (!Number.isFinite(x) || !Number.isFinite(y)) {
      pen = false;
      continue;
    }
    const rx = Math.round(x), ry = Math.round(y);
    if (pen && rx === lx && ry === ly && i < n - 1) continue;
    lx = rx;
    ly = ry;
    drawn++;
    if (line) {
      if (pen) sink.lineTo(x, y);
      else sink.moveTo(x, y);
      pen = true;
    } else {
      polygon(sink, x, y, dot, 8, 0);
      pen = true;
    }
  }
  return drawn;
}

/** pixel sizes of XPP's text sizes 0-4 (its fonts are 8 to 24 points) */
export const TEXT_PX = [10, 12, 14, 18, 24];

export function textPx(size: number): number {
  return TEXT_PX[Math.max(0, Math.min(4, Math.round(size)))];
}
