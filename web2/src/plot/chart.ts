/* The plot widget: a uPlot chart for a PlotModel. It draws, maps between
   data and screen, and finds the point under a position; the gestures that
   change the view are in interactions.ts. It owns no application state: the
   view that owns it (ui/PlotView.tsx) feeds it the store's viewport and puts
   what it reports back into the store. A phase plane's nullclines,
   direction field and flows (phase.ts) are drawn on the same canvas, under
   the curves, each shown or hidden from the legend like a curve. */
import uPlot from 'uplot';
import {curveColor} from './colors';
import {LineTrace, sameFrame, tracePoints, type PixelFrame} from './decimate';
import type {PlotModel} from './model';
import {nearestPoint, type Nearest} from './nearest';
import {arrowSegments, phaseLayers, tracePolyline, traceSegments, type Layer, type LayerKey} from './phase';
import type {Dfield, Nullclines} from '../store/phase';
import type {Ranges} from './viewmath';
import type {Range, Viewport} from '../store/state';

export interface ChartCallbacks {
  /** push: the start of a gesture, so the view before it can be undone */
  onViewport(v: Viewport, push: boolean): void;
}

export interface ChartInfo {
  mode: 1 | 2;
  curves: {label: string; points: number; visible: boolean; color: string}[];
  x: Range;
  y: Range;
  width: number;
  height: number;
  /** milliseconds the last draws took (newest last), and how many draws there were */
  drawMs: number[];
  draws: number;
  /** a long curve's trace is still being refined in later tasks (decimate.ts) */
  tracing: boolean;
  /** milliseconds from a new view to its finished traces, the last time it took later tasks */
  traceMs: number | null;
  /** vertices the phase plane's line paths drew last, per curve (null: uPlot's own path) */
  vertices: (number | null)[];
  /** the nullclines, direction field and flow, and what the last draw drew of
      each: segments, arrows, points (0 when hidden) */
  layers: (Layer & {visible: boolean; drawn: number; css: string})[];
}

const DRAWS_KEPT = 100;
/** points of a line traced while drawing (a small curve, an append's rows);
    more go to later tasks, TRACE_MS each at most, TRACE_STEP points at a time */
const TRACE_NOW = 32768;
const TRACE_MS = 8;
const TRACE_STEP = 16384;

/** a phase-plane curve's traces: the one for the frame drawn last, and the
    last complete one, drawn in its place while the other is under way */
interface CurveTraces {
  xs: ArrayBufferLike | null;
  ys: ArrayBufferLike | null;
  row0: number;
  current: LineTrace | null;
  /** `current` has seen every row its arrays had when it last ran */
  done: boolean;
  complete: LineTrace | null;
}

function extent(arrays: Float32Array[]): Range | null {
  let min = Infinity, max = -Infinity;
  for (const a of arrays)
    for (let i = 0; i < a.length; i++) {
      const v = a[i];
      if (v < min) min = v;
      if (v > max) max = v;
    }
  if (!(min <= max)) return null;
  if (min === max) return {min: min - 1, max: max + 1};
  const pad = (max - min) * 0.05;
  return {min: min - pad, max: max + pad};
}

function cssVar(name: string): string {
  return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
}

export class Chart {
  private u: uPlot | null = null;
  private model: PlotModel | null = null;
  private visible: boolean[] = [];
  private applying = false;
  private reportPending = false;
  private dark = false;
  private base: Ranges = {x: {min: 0, max: 1}, y: {min: 0, max: 1}};
  private drawStart = 0;
  private drawMs: number[] = [];
  private draws = 0;
  private traces: CurveTraces[] = [];
  private traceTimer: ReturnType<typeof setTimeout> | null = null;
  private traceStart = 0;
  private traceMs: number | null = null;
  private vertices: (number | null)[] = [];
  private nullclines: Nullclines | null = null;
  private dfield: Dfield | null = null;
  private layers: Layer[] = [];
  private hiddenLayers = new Set<LayerKey>();
  private layerDrawn = new Map<LayerKey, number>();
  /** called with the plotting area each time uPlot makes a new one */
  onArea: (area: HTMLElement) => void = () => {};

  constructor(private readonly root: HTMLElement, private readonly cb: ChartCallbacks) {}

  /** draws `model`; `view` is the core's window (Viewaxes), `viewport` the user's zoom */
  set(model: PlotModel, view: Ranges | null, viewport: Viewport, dark: boolean): void {
    const rebuild = !this.u || !this.model || this.model.mode !== model.mode
      || this.model.curves.length !== model.curves.length || this.dark !== dark
      || this.model.curves.some((c, i) => c.color !== model.curves[i].color || c.line !== model.curves[i].line
        || c.label !== model.curves[i].label);
    if (rebuild) this.visible = model.curves.map(() => true);
    this.model = model;
    this.dark = dark;
    this.base = {
      x: view?.x ?? extent(model.curves.map(c => c.xs)) ?? {min: 0, max: 1},
      y: view?.y ?? extent(model.curves.map(c => c.ys)) ?? {min: 0, max: 1},
    };
    if (rebuild) this.create();
    else this.u!.setData(this.data(), false); /* new rows (an append): the same chart, new paths */
    this.applyViewport(viewport); /* draws */
  }

  /** the plotting area in canvas pixels and the ranges it shows */
  private frame(u: uPlot): PixelFrame {
    return {xmin: u.scales.x.min!, xmax: u.scales.x.max!, ymin: u.scales.y.min!, ymax: u.scales.y.max!,
      left: u.bbox.left, top: u.bbox.top, width: u.bbox.width, height: u.bbox.height};
  }

  /* path builders that leave out what changes no pixel (decimate.ts): for
     the phase plane's lines, and for points in either mode. A line of more
     than a slice of points (still to trace) is traced a slice per task
     (traceRest), and until that is done the last complete trace stands in,
     so no frame waits for a million points. */
  private linePath: uPlot.Series.PathBuilder = (u, si, i0, i1) => {
    const c = this.model?.curves[si - 1];
    const stroke = new Path2D();
    if (!c) return {stroke, fill: null, clip: null, band: null, flags: 1};
    const f = this.frame(u), end = Math.min(i1, c.xs.length - 1) + 1;
    const tr = this.traces[si - 1] ??= {xs: null, ys: null, row0: 0, current: null, done: false, complete: null};
    if (tr.xs !== c.xs.buffer || tr.ys !== c.ys.buffer || tr.row0 !== c.row0) {
      /* other arrays (a new series, or buffers that grew): its rows are
         traced again; the kept rows of the old trace still draw as a stand-in */
      if (tr.current && tr.done) tr.complete = tr.current;
      tr.current = null;
      tr.xs = c.xs.buffer;
      tr.ys = c.ys.buffer;
      tr.row0 = c.row0;
    }
    if (!tr.current || !sameFrame(tr.current.frame, f)) {
      if (tr.current && tr.done) tr.complete = tr.current;
      tr.current = new LineTrace(f, Math.max(0, i0));
      this.traceStart = performance.now();
    }
    /* at most a slice now (a small curve, the rows of an append), else all
       of it in later tasks: the frame shows the stand-in meanwhile */
    let n = 0;
    tr.done = end - tr.current.next <= TRACE_NOW && tr.current.run(c.xs, c.ys, end);
    if (!tr.done) {
      n += tr.complete?.draw(c.xs, c.ys, f, stroke) ?? 0;
      this.traceLater();
    }
    this.vertices[si - 1] = n + tr.current.draw(c.xs, c.ys, f, stroke);
    return {stroke, fill: null, clip: null, band: null, flags: 1};
  };

  private traceLater(): void {
    if (this.traceTimer === null) this.traceTimer = setTimeout(() => this.traceRest(), 0);
  }

  /** TRACE_MS more of the unfinished traces; a redraw when all are done */
  private traceRest(): void {
    this.traceTimer = null;
    const m = this.model, u = this.u;
    if (!m || !u) return;
    const t0 = performance.now();
    let pending: boolean;
    do {
      pending = false;
      this.traces.forEach((tr, k) => {
        const c = m.curves[k], t = tr.current;
        if (!c || !t || tr.done || tr.xs !== c.xs.buffer || tr.ys !== c.ys.buffer) return; /* new arrays: the next draw starts over */
        tr.done = t.run(c.xs, c.ys, c.xs.length, TRACE_STEP);
        if (!tr.done) pending = true;
      });
    } while (pending && performance.now() - t0 < TRACE_MS);
    if (pending) {
      this.traceTimer = setTimeout(() => this.traceRest(), 0);
      return;
    }
    this.traceMs = performance.now() - this.traceStart;
    this.applying = true;
    u.batch(() => u.setData(this.data(), false)); /* new paths from the finished traces */
    this.applying = false;
  }

  private pointPath(radius: number): uPlot.Series.PathBuilder {
    return (u, si, i0, i1) => {
      const c = this.model?.curves[si - 1], fill = new Path2D(), clip = new Path2D();
      const r = (radius + 0.5) * uPlot.pxRatio, b = u.bbox;
      clip.rect(b.left - 2 * r, b.top - 2 * r, b.width + 4 * r, b.height + 4 * r);
      if (c) {
        tracePoints(c.xs, c.ys, Math.max(0, i0), Math.min(i1, c.xs.length - 1), this.frame(u), r, (x, y) => {
          fill.moveTo(x + r, y);
          fill.arc(x, y, r, 0, 2 * Math.PI);
        });
      }
      return {stroke: null, fill, clip, flags: 3};
    };
  }

  private data(): uPlot.AlignedData {
    const m = this.model!;
    if (m.mode === 1) return [m.curves[0]?.xs ?? new Float32Array(0), ...m.curves.map(c => c.ys)] as uPlot.AlignedData;
    return [null, ...m.curves.map(c => [c.xs, c.ys])] as unknown as uPlot.AlignedData;
  }

  private create(): void {
    const m = this.model!;
    this.u?.destroy();
    this.traces = [];
    this.vertices = m.curves.map(() => null);
    /* the theme's colours when drawn: a theme switch sets the variables after the chart is made
       (useDark's effect runs after this view's), so values read now would be the old theme's */
    const fg = () => cssVar('--fg-muted') || '#666', grid = () => cssVar('--grid') || '#eee', font = cssVar('--plot-font');
    const axis = (label: string): uPlot.Axis => ({
      label, stroke: fg, font, labelFont: font, grid: {stroke: grid, width: 1}, ticks: {stroke: grid, width: 1},
    });
    const series: uPlot.Series[] = m.curves.map((c, i) => {
      const color = curveColor(c.color, this.dark);
      const s: uPlot.Series = {label: c.label, stroke: color, width: 1.5, show: this.visible[i], points: {show: false}};
      if (!c.line) {
        s.paths = this.pointPath(c.radius);
        s.fill = color;
        s.points = {show: false, size: 2 * c.radius + 1, width: 0, fill: color};
      } else if (m.mode === 2) {
        s.paths = this.linePath; /* a time plot keeps uPlot's, which already keeps a min and max per pixel column */
      }
      if (m.mode === 2) s.facets = [{scale: 'x', auto: false}, {scale: 'y', auto: false}];
      return s;
    });
    const {width, height} = this.size();
    const opts: uPlot.Options = {
      mode: m.mode,
      width, height,
      legend: {show: false},
      scales: {x: {time: false, auto: false}, y: {auto: false}},
      axes: [axis(m.xLabel), axis(m.yLabel)],
      series: [{}, ...series],
      cursor: {
        drag: {x: true, y: true, uni: 20, setScale: true},
        points: {show: false},
        bind: {dblclick: () => () => { this.reset(); return null; }},
      },
      hooks: {
        setScale: [() => this.scaleChanged()],
        drawClear: [() => { this.drawStart = performance.now(); }],
        drawAxes: [u => this.drawPhase(u)], /* after the axes, before the curves */
        draw: [() => this.drawn()],
      },
    };
    this.u = new uPlot(opts, this.data(), this.root);
    this.onArea(this.u.over);
  }

  /** the window's nullclines, direction field and flows (null: none) */
  setPhase(nullclines: Nullclines | null, dfield: Dfield | null): void {
    if (nullclines === this.nullclines && dfield === this.dfield) return;
    this.nullclines = nullclines;
    this.dfield = dfield;
    this.layers = phaseLayers(nullclines, dfield);
    this.u?.redraw(false, false);
  }

  setLayerVisible(key: LayerKey, show: boolean): void {
    if (show) this.hiddenLayers.delete(key);
    else this.hiddenLayers.add(key);
    this.u?.redraw(false, false);
  }

  isLayerVisible(key: LayerKey): boolean {
    return !this.hiddenLayers.has(key);
  }

  private drawPhase(u: uPlot): void {
    this.layerDrawn.clear();
    if (!this.layers.length) return;
    const ctx = u.ctx, f = this.frame(u), r = uPlot.pxRatio, nc = this.nullclines, df = this.dfield;
    const stroke = (key: LayerKey, color: number, width: number, dash: number[], trace: (p: Path2D) => number) => {
      if (this.hiddenLayers.has(key)) return;
      const p = new Path2D();
      const n = trace(p);
      this.layerDrawn.set(key, (this.layerDrawn.get(key) ?? 0) + n);
      ctx.strokeStyle = curveColor(color, this.dark);
      ctx.lineWidth = width * r;
      ctx.setLineDash(dash.map(d => d * r));
      ctx.stroke(p);
    };
    ctx.save();
    ctx.beginPath();
    ctx.rect(u.bbox.left, u.bbox.top, u.bbox.width, u.bbox.height);
    ctx.clip();
    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';
    if (df) {
      for (const c of df.flows) stroke('flow', c.color, 1, [], p => tracePolyline(c.xs, c.ys, f, p));
      if (df.n > 0)
        stroke('dfield', df.color, 1, [], p => {
          const {segments, arrows} = arrowSegments(df, f, 7 * r);
          for (let i = 0; i < segments.length; i += 4) {
            p.moveTo(segments[i], segments[i + 1]);
            p.lineTo(segments[i + 2], segments[i + 3]);
          }
          return arrows;
        });
    }
    if (nc) {
      /* frozen ones dashed, so they are told apart without their colour (A7) */
      for (const z of nc.frozen) {
        stroke('xnull', nc.xColor, 1.25, [5, 3], p => traceSegments(z.x, f, p));
        stroke('ynull', nc.yColor, 1.25, [5, 3], p => traceSegments(z.y, f, p));
      }
      stroke('xnull', nc.xColor, 2, [], p => traceSegments(nc.x, f, p));
      stroke('ynull', nc.yColor, 2, [], p => traceSegments(nc.y, f, p));
    }
    ctx.restore();
  }

  private drawn(): void {
    this.drawMs.push(performance.now() - this.drawStart);
    if (this.drawMs.length > DRAWS_KEPT) this.drawMs.shift();
    this.draws++;
  }

  private size(): {width: number; height: number} {
    const r = this.root.getBoundingClientRect();
    return {width: Math.max(120, Math.floor(r.width)), height: Math.max(120, Math.floor(r.height))};
  }

  resize(): void {
    if (this.u) this.u.setSize(this.size());
  }

  applyViewport(v: Viewport): void {
    const u = this.u;
    if (!u) return;
    this.applying = true;
    u.batch(() => {
      u.setScale('x', v.x ?? this.base.x);
      u.setScale('y', v.y ?? this.base.y);
    });
    this.applying = false;
  }

  /** a new view from a gesture */
  setView(r: Ranges, push: boolean): void {
    this.cb.onViewport({x: r.x, y: r.y}, push);
  }

  /** back to the core's view */
  reset(): void {
    this.cb.onViewport({x: null, y: null}, true);
  }

  /* uPlot's own drag-to-zoom (a box): it sets x, then y; one report for both */
  private scaleChanged(): void {
    if (!this.u || this.applying || this.reportPending) return;
    this.reportPending = true;
    queueMicrotask(() => {
      this.reportPending = false;
      const u = this.u;
      if (!u) return;
      const x = u.scales.x, y = u.scales.y;
      if (x.min == null || x.max == null || y.min == null || y.max == null) return;
      this.cb.onViewport({x: {min: x.min, max: x.max}, y: {min: y.min, max: y.max}}, true);
    });
  }

  ranges(): Ranges {
    const u = this.u!;
    return {x: {min: u.scales.x.min!, max: u.scales.x.max!}, y: {min: u.scales.y.min!, max: u.scales.y.max!}};
  }

  /** the point nearest to (px, py) of the plotting area, within maxDist CSS pixels */
  hit(px: number, py: number, maxDist: number): Nearest | null {
    const u = this.u, m = this.model;
    if (!u || !m) return null;
    const {x, y} = this.ranges();
    return nearestPoint(m.curves, this.visible, {xmin: x.min, xmax: x.max, ymin: y.min, ymax: y.max,
      width: u.over.clientWidth, height: u.over.clientHeight}, px, py, maxDist);
  }

  /** where a data point is, in CSS pixels from the chart's root (for the hover marker) */
  position(curve: number, index: number): {left: number; top: number} | null {
    const u = this.u, c = this.model?.curves[curve];
    if (!u || !c || index < 0 || index >= c.xs.length) return null;
    const o = u.over.getBoundingClientRect(), r = this.root.getBoundingClientRect();
    return {left: o.left - r.left + u.valToPos(c.xs[index], 'x'), top: o.top - r.top + u.valToPos(c.ys[index], 'y')};
  }

  /** the plotting area, in CSS pixels from the chart's root (for overlays) */
  areaBox(): {left: number; top: number; width: number; height: number} | null {
    const u = this.u;
    if (!u) return null;
    const o = u.over.getBoundingClientRect(), r = this.root.getBoundingClientRect();
    return {left: o.left - r.left, top: o.top - r.top, width: o.width, height: o.height};
  }

  setVisible(curve: number, show: boolean): void {
    this.visible[curve] = show;
    this.u?.setSeries(curve + 1, {show});
  }

  isVisible(curve: number): boolean {
    return this.visible[curve] ?? false;
  }

  /** a PNG of the plot (axes included) */
  png(): string | null {
    return this.u ? this.u.ctx.canvas.toDataURL('image/png') : null;
  }

  info(): ChartInfo | null {
    const u = this.u, m = this.model;
    if (!u || !m) return null;
    return {
      mode: m.mode,
      curves: m.curves.map((c, i) => ({label: c.label, points: c.xs.length, visible: this.visible[i],
        color: curveColor(c.color, this.dark)})),
      ...this.ranges(),
      width: u.over.clientWidth,
      height: u.over.clientHeight,
      drawMs: this.drawMs.slice(),
      draws: this.draws,
      tracing: this.traceTimer !== null,
      traceMs: this.traceMs,
      vertices: this.vertices.slice(),
      layers: this.layers.map(l => ({...l, visible: this.isLayerVisible(l.key), drawn: this.layerDrawn.get(l.key) ?? 0,
        css: curveColor(l.color, this.dark)})),
    };
  }

  destroy(): void {
    if (this.traceTimer !== null) clearTimeout(this.traceTimer);
    this.traceTimer = null;
    this.u?.destroy();
    this.u = null;
  }
}
