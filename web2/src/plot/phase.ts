/* Nullclines, the direction field and flows on the plot (docs/ui-v2.md T7):
   which layers a window has, named as the legend shows them, and their
   geometry on the chart's canvas. The chart (chart.ts) strokes what these
   return in uPlot's draw cycle, under the curves. Pure: no DOM. */
import type {PixelFrame} from './decimate';
import {segmentCount, trajectoryCount, type Dfield, type Nullclines} from '../store/phase';

export type LayerKey = 'xnull' | 'ynull' | 'dfield' | 'flow';

export interface Layer {
  key: LayerKey;
  /** the legend's name: "V-nullcline", "Direction field", "Flow" */
  label: string;
  /** XPP colour index */
  color: number;
  /** segments (nullclines, frozen ones included), arrows, or trajectories */
  count: number;
}

/** the layers window data has, in legend order; a layer with nothing to draw is left out */
export function phaseLayers(nc: Nullclines | null, df: Dfield | null): Layer[] {
  const out: Layer[] = [];
  if (nc) {
    const xs = segmentCount(nc.x) + nc.frozen.reduce((n, f) => n + segmentCount(f.x), 0);
    const ys = segmentCount(nc.y) + nc.frozen.reduce((n, f) => n + segmentCount(f.y), 0);
    if (xs) out.push({key: 'xnull', label: `${nc.xName || 'x'}-nullcline`, color: nc.xColor, count: xs});
    if (ys) out.push({key: 'ynull', label: `${nc.yName || 'y'}-nullcline`, color: nc.yColor, count: ys});
  }
  if (df && df.n > 0 && df.grid.length >= 4)
    out.push({key: 'dfield', label: 'Direction field', color: df.color, count: df.grid.length >> 2});
  const flows = df?.flows.filter(f => f.xs.length > 0) ?? [];
  if (flows.length)
    out.push({key: 'flow', label: 'Flow', color: flows[0].color, count: trajectoryCount(flows[0].xs)});
  return out;
}

export interface PathSink {
  moveTo(x: number, y: number): void;
  lineTo(x: number, y: number): void;
}

/** plot coordinates to canvas pixels on `f` (y grows downwards) */
export function pixelMap(f: PixelFrame): {x: (v: number) => number; y: (v: number) => number; sx: number; sy: number} {
  const sx = f.width / (f.xmax - f.xmin), sy = f.height / (f.ymax - f.ymin);
  return {x: v => f.left + (v - f.xmin) * sx, y: v => f.top + (f.ymax - v) * sy, sx, sy};
}

/** segments [x1,y1,x2,y2,...] as moves and lines on `f`; returns how many */
export function traceSegments(a: Float32Array, f: PixelFrame, sink: PathSink): number {
  const m = pixelMap(f), n = a.length >> 2;
  for (let k = 0; k < n; k++) {
    const i = 4 * k;
    sink.moveTo(m.x(a[i]), m.y(a[i + 1]));
    sink.lineTo(m.x(a[i + 2]), m.y(a[i + 3]));
  }
  return n;
}

/** polylines one after the other, NaN between two (Flow's trajectories); returns the points drawn */
export function tracePolyline(xs: Float32Array, ys: Float32Array, f: PixelFrame, sink: PathSink): number {
  const m = pixelMap(f), n = Math.min(xs.length, ys.length);
  let pen = false, drawn = 0;
  for (let i = 0; i < n; i++) {
    const x = xs[i], y = ys[i];
    if (x !== x || y !== y) {
      pen = false;
      continue;
    }
    if (pen) sink.lineTo(m.x(x), m.y(y));
    else sink.moveTo(m.x(x), m.y(y));
    pen = true;
    drawn++;
  }
  return drawn;
}

/** of a grid cell on screen: the arrows' length (scaled), or the longest's (by speed) */
const SCALED_LENGTH = 0.6, LONGEST = 0.9;
const HEAD_ANGLE = (25 * Math.PI) / 180;

/** the field's arrows on `f` as segments [x0,y0,x1,y1,...] in canvas pixels:
    per arrow its shaft from the grid point, then the two strokes of its head
    (at most `headPx` long). A direction in plot units becomes one on screen
    through the axes' scales, so it is normalized after that. Lengths follow
    the grid's spacing on screen: every arrow SCALED_LENGTH of a cell's
    smaller side, or, by speed, the fastest LONGEST of it and the others in
    proportion to their speed on screen. An arrow where the field is 0 is
    left out. */
export function arrowSegments(df: Dfield, f: PixelFrame, headPx: number): {segments: Float32Array; arrows: number} {
  const m = pixelMap(f), n = df.grid.length >> 2;
  const cell = Math.min(Math.abs(df.du * m.sx), Math.abs(df.dv * m.sy));
  const dir = new Float64Array(3 * n); /* screen direction (unit) and speed on screen */
  let fastest = 0;
  for (let k = 0; k < n; k++) {
    const px = df.grid[4 * k + 2] * m.sx, py = -df.grid[4 * k + 3] * m.sy, len = Math.hypot(px, py);
    if (!(len > 0)) continue;
    const s = (df.speed[k] ?? 0) * len;
    dir[3 * k] = px / len;
    dir[3 * k + 1] = py / len;
    dir[3 * k + 2] = s;
    if (s > fastest) fastest = s;
  }
  const out = new Float32Array(12 * n);
  let used = 0, arrows = 0;
  for (let k = 0; k < n; k++) {
    const ux = dir[3 * k], uy = dir[3 * k + 1];
    if (ux === 0 && uy === 0) continue;
    const len = df.scaled ? SCALED_LENGTH * cell : fastest > 0 ? (LONGEST * cell * dir[3 * k + 2]) / fastest : 0;
    if (!(len >= 0.5)) continue;
    const x0 = m.x(df.grid[4 * k]), y0 = m.y(df.grid[4 * k + 1]);
    const x1 = x0 + ux * len, y1 = y0 + uy * len;
    const h = Math.min(0.35 * len, headPx), c = Math.cos(HEAD_ANGLE), s = Math.sin(HEAD_ANGLE);
    /* the head: the shaft's direction turned back by +-HEAD_ANGLE */
    const bx = -ux, by = -uy;
    out.set([x0, y0, x1, y1,
      x1, y1, x1 + h * (bx * c - by * s), y1 + h * (bx * s + by * c),
      x1, y1, x1 + h * (bx * c + by * s), y1 + h * (-bx * s + by * c)], used);
    used += 12;
    arrows++;
  }
  return {segments: out.subarray(0, used), arrows};
}
