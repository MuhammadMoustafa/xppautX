/* Drawing a curve of a million points without holding up a frame: only what
   changes pixels goes into the canvas path. Pure (no DOM): the chart gives a
   sink that is a Path2D, the tests one that records.

   Lines (LineTrace): each segment is walked cell by cell over a grid of the
   plotting area's device pixels, clipped to it. A segment that crosses no
   cell an earlier segment has not already crossed is left out (the pen
   moves instead), so a trajectory that goes round a limit cycle a thousand
   times costs about one cycle's worth of path, and points closer than a
   pixel collapse. Segments wholly outside the area are left out too. What
   is drawn differs from drawing every segment only below a pixel.

   The walk is O(points) and a million take tens of milliseconds, so a trace
   runs a slice of points at a time (`run`): the chart runs a first slice
   while it draws and the rest in later tasks, drawing meanwhile what an
   earlier trace (another zoom, fewer rows) kept, since kept vertices are
   indices into the data and draw right in any frame. A trace carries on
   where it stopped when rows are appended.

   Points (tracePoints): one dot per cell; the grid has a margin of the
   dot's radius, so a dot just outside the area that shows is kept.

   uPlot's own line builder already keeps a min and a max per pixel column
   for a time plot (x increasing); these are for the phase plane, where x
   goes back and forth, and for points in either mode. */

/** the plotting area in canvas pixels and the data ranges it shows */
export interface PixelFrame {
  xmin: number;
  xmax: number;
  ymin: number;
  ymax: number;
  left: number;
  top: number;
  width: number;
  height: number;
}

export interface LineSink {
  moveTo(x: number, y: number): void;
  lineTo(x: number, y: number): void;
}

export function sameFrame(a: PixelFrame, b: PixelFrame): boolean {
  return a.xmin === b.xmin && a.xmax === b.xmax && a.ymin === b.ymin && a.ymax === b.ymax && a.left === b.left
    && a.top === b.top && a.width === b.width && a.height === b.height;
}

/** the vertices of a curve worth drawing in one frame, found a slice of points at a time */
export class LineTrace {
  /** the next point to look at */
  next: number;
  private readonly W: number;
  private readonly H: number;
  private readonly stride: number;
  private readonly sx: number;
  private readonly sy: number;
  /** one bit per device pixel, plus a row and a column: the clip ends on the
      far edges W and H, which then need no clamping (a clamp costs half the time) */
  private readonly grid: Uint32Array;
  /** kept vertices: point indices, -1 before one that starts a new piece */
  private out = new Int32Array(256);
  private len = 0;
  private have = false;
  private pen = false;
  private px = 0;
  private py = 0;

  constructor(readonly frame: PixelFrame, first = 0) {
    this.next = first;
    this.W = Math.max(1, Math.ceil(frame.width));
    this.H = Math.max(1, Math.ceil(frame.height));
    this.stride = this.W + 1;
    this.sx = frame.width / (frame.xmax - frame.xmin);
    this.sy = frame.height / (frame.ymax - frame.ymin);
    const cells = Number.isFinite(this.sx) && Number.isFinite(this.sy) ? this.stride * (this.H + 1) : 0;
    this.grid = new Uint32Array((cells + 31) >>> 5);
  }

  private keep(v: number): void {
    if (this.len === this.out.length) {
      const o = new Int32Array(2 * this.out.length);
      o.set(this.out);
      this.out = o;
    }
    this.out[this.len++] = v;
  }

  /** looks at points next.. up to `end` (exclusive), at most `count` of them;
      true when it got to `end`. One function on purpose: a call per segment
      would box its doubles, and the garbage costs more than the walk. */
  run(xs: ArrayLike<number>, ys: ArrayLike<number>, end: number, count = Infinity): boolean {
    const {W, H, stride, sx, sy, grid: g} = this, xmin = this.frame.xmin, ymax = this.frame.ymax;
    if (!g.length) {
      this.next = Math.max(this.next, end);
      return true;
    }
    const stop = Math.min(end, this.next + count);
    let have = this.have, pen = this.pen, px = this.px, py = this.py, i = this.next;
    for (; i < stop; i++) {
      /* area coordinates: 0..W across, 0..H down */
      const cx = (xs[i] - xmin) * sx, cy = (ymax - ys[i]) * sy;
      if (!(cx - cx === 0 && cy - cy === 0)) { /* NaN or infinite: a gap */
        have = pen = false;
        continue;
      }
      if (!have) {
        have = true;
        px = cx;
        py = cy;
        continue;
      }
      let fresh = false;
      /* both ends beyond the same edge: nothing of it shows (most of a
         curve, zoomed in) */
      if (!((px < 0 && cx < 0) || (px > W && cx > W) || (py < 0 && cy < 0) || (py > H && cy > H))) {
        /* the part of the segment inside [0,W] x [0,H] (Liang-Barsky),
           unless it is all inside */
        let ax = px, ay = py, bx = cx, by = cy, inside = true;
        if (!(px >= 0 && px <= W && py >= 0 && py <= H && cx >= 0 && cx <= W && cy >= 0 && cy <= H)) {
          const dx = cx - px, dy = cy - py;
          let t0 = 0, t1 = 1;
          for (let e = 0; e < 4 && inside; e++) {
            const p = e === 0 ? -dx : e === 1 ? dx : e === 2 ? -dy : dy;
            const q = e === 0 ? px : e === 1 ? W - px : e === 2 ? py : H - py;
            if (p === 0) {
              if (q < 0) inside = false; /* parallel to this edge, outside it */
            } else {
              const r = q / p;
              if (p < 0) {
                if (r > t1) inside = false;
                else if (r > t0) t0 = r;
              } else if (r < t0) inside = false;
              else if (r < t1) t1 = r;
            }
          }
          ax = px + t0 * dx;
          ay = py + t0 * dy;
          bx = px + t1 * dx;
          by = py + t1 * dy;
        }
        if (inside) {
          /* its cells, a pixel at a time along the longer axis, marked as
             it goes; truncation gives the cells (the coordinates are in
             [0,W] x [0,H] here, to a rounding that truncation takes to 0) */
          const dx = bx - ax, dy = by - ay, adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
          const steps = Math.ceil(adx > ady ? adx : ady) | 0;
          const scale = 1 / (steps > 0 ? steps : 1), ix = dx * scale, iy = dy * scale;
          let x = ax, y = ay;
          for (let k = 0; k <= steps; k++) {
            const cell = (y | 0) * stride + (x | 0), w = cell >>> 5, bit = 1 << (cell & 31);
            if ((g[w] & bit) === 0) {
              g[w] |= bit;
              fresh = true;
            }
            x += ix;
            y += iy;
          }
        }
      }
      if (fresh) {
        if (!pen) {
          this.keep(-1);
          this.keep(i - 1);
        }
        this.keep(i);
        pen = true;
      } else pen = false;
      px = cx;
      py = cy;
    }
    this.have = have;
    this.pen = pen;
    this.px = px;
    this.py = py;
    this.next = i;
    return i >= end;
  }

  /** draws the kept vertices of xs, ys into `sink` as frame `f` places them
      (the frame traced for, or another); returns how many */
  draw(xs: ArrayLike<number>, ys: ArrayLike<number>, f: PixelFrame, sink: LineSink): number {
    const sx = f.width / (f.xmax - f.xmin), sy = f.height / (f.ymax - f.ymin);
    const rows = Math.min(xs.length, ys.length);
    let move = true, n = 0;
    for (let k = 0; k < this.len; k++) {
      const v = this.out[k];
      if (v < 0 || v >= rows) { /* a new piece, or a row these arrays do not have (a stand-in's) */
        move = true;
        continue;
      }
      const x = f.left + (xs[v] - f.xmin) * sx, y = f.top + (f.ymax - ys[v]) * sy;
      if (move) sink.moveTo(x, y);
      else sink.lineTo(x, y);
      move = false;
      n++;
    }
    return n;
  }
}

/** draws xs[i0..i1] against ys[i0..i1] into `sink` at once; returns the vertices it emitted */
export function traceLine(xs: ArrayLike<number>, ys: ArrayLike<number>, i0: number, i1: number, f: PixelFrame,
  sink: LineSink): number {
  const t = new LineTrace(f, i0);
  t.run(xs, ys, i1 + 1);
  return t.draw(xs, ys, f, sink);
}

let scratch = new Uint32Array(0);

/** calls dot(x, y) for the first point in each device pixel, dots of radius r
    included when they reach into the area; returns how many */
export function tracePoints(xs: ArrayLike<number>, ys: ArrayLike<number>, i0: number, i1: number, f: PixelFrame,
  r: number, dot: (x: number, y: number) => void): number {
  const pad = Math.ceil(r) + 1;
  const W = Math.max(1, Math.ceil(f.width)) + 2 * pad, H = Math.max(1, Math.ceil(f.height)) + 2 * pad;
  const sx = f.width / (f.xmax - f.xmin), sy = f.height / (f.ymax - f.ymin);
  if (!(Number.isFinite(sx) && Number.isFinite(sy))) return 0;
  const words = (W * H + 31) >>> 5;
  if (scratch.length < words) scratch = new Uint32Array(words);
  else scratch.fill(0, 0, words);
  const g = scratch;
  let n = 0;
  for (let i = i0; i <= i1; i++) {
    const cx = (xs[i] - f.xmin) * sx, cy = (f.ymax - ys[i]) * sy;
    const c = Math.floor(cx) + pad, row = Math.floor(cy) + pad;
    if (!(c >= 0 && c < W && row >= 0 && row < H)) continue; /* NaN fails too */
    const cell = row * W + c, w = cell >>> 5, bit = 1 << (cell & 31);
    if (g[w] & bit) continue;
    g[w] |= bit;
    dot(f.left + cx, f.top + cy);
    n++;
  }
  return n;
}
