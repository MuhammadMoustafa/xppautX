import {cssVar} from './css';
/* The plot widget: a uPlot chart for a PlotModel. It draws, maps between
   data and screen, and finds the point under a position; the gestures that
   change the view are in interactions.ts. It owns no application state: the
   view that owns it (ui/PlotView.tsx) feeds it the store's viewport and puts
   what it reports back into the store. A phase plane's nullclines,
   direction field and flows (phase.ts) are drawn on the same canvas, under
   the curves, and so are its frozen curves; its other marks (marks.ts:
   equilibria, text, arrows, markers) over them. Each is shown or hidden
   from the legend like a curve. */
import uPlot from 'uplot';
import {canvasPixels} from './canvasPixels';
import {curveColor} from './colors';
import {ColumnTrace, LineTrace, sameFrame, tracePoints, type CurveTrace, type PixelFrame} from './decimate';
import type {PlotModel} from './model';
import {nearestPoint, type Nearest} from './nearest';
import {
  arrowPath, equilibriumPath, MARKER_PX, markerPath, markLayers, textPx, traceFrozen, type MarkKey, type MarkLayer,
} from './marks';
import {arrowSegments, phaseLayers, tracePolyline, traceSegments, type Layer, type LayerKey} from './phase';
import type {Marks} from '../store/marks';
import type {Dfield, Nullclines} from '../store/phase';
import type {Ranges} from './viewmath';
import type {Range, Viewport} from '../store/state';

export interface ChartCallbacks {
  onViewport(v: Viewport): void;
  onTrace?(hit: (Nearest & {run: number}) | null): void;
}

export interface ChartInfo {
  mode: 1 | 2;
  curves: {label: string; points: number; visible: boolean; color: string}[];
  x: Range;
  y: Range;
  width: number;
  height: number;
  /** how many draws there were (draw times are measured from outside, through CDP: W58) */
  draws: number;
  /** a long curve's trace is still being refined in later tasks (decimate.ts) */
  tracing: boolean;
  /** vertices the phase plane's line paths drew last, per curve (null: uPlot's own path) */
  vertices: (number | null)[];
  /** earlier runs drawn under the curves (store/runs.ts): how many, whether
      shown, and the vertices the last draw drew of them */
  runs: {count: number; shown: boolean; drawn: number};
  /** the nullclines, direction field and flow, then the marks, and what the
      last draw drew of each: segments, arrows, points, marks (0 when hidden) */
  layers: ((Layer | MarkLayer) & {visible: boolean; drawn: number; css: string})[];
}

/** points of a line traced while drawing (a small curve, an append's rows);
    more go to later tasks, TRACE_MS each at most, TRACE_STEP points at a time */
const TRACE_NOW = 32768;
const TRACE_MS = 8;
const TRACE_STEP = 16384;

/** a line's traces: the one for the frame drawn last, and the
    last complete one, drawn in its place while the other is under way */
interface CurveTraces {
  xs: ArrayBufferLike | null;
  ys: ArrayBufferLike | null;
  row0: number;
  current: CurveTrace | null;
  /** `current` has seen every row its arrays had when it last ran */
  done: boolean;
  complete: CurveTrace | null;
}

/** a scale's range function that keeps the range it is given (W85) */
const asGiven: uPlot.Range.Function = (_u, min, max) => [min ?? 0, max ?? 1];

/** the axis range the chart gives values from `r.min` to `r.max` (the model's ranges) */
export function padded(r: Range | null): Range | null {
  if (!r) return null;
  const {min, max} = r;
  if (min === max) return {min: min - 1, max: max + 1};
  const pad = (max - min) * 0.05;
  return {min: min - pad, max: max + pad};
}

/** a plot point in canvas pixels on `f`, or null off the finite plane */
function pixelOf(f: PixelFrame, x: number, y: number): {x: number; y: number} | null {
  const px = f.left + ((x - f.xmin) * f.width) / (f.xmax - f.xmin);
  const py = f.top + ((f.ymax - y) * f.height) / (f.ymax - f.ymin);
  return Number.isFinite(px) && Number.isFinite(py) ? {x: px, y: py} : null;
}


export class Chart {
  private u: uPlot | null = null;
  private model: PlotModel | null = null;
  private visible: boolean[] = [];
  private applying = false;
  /* the view last asked for (setView, fit), until applyViewport draws it */
  private requested: Ranges | null = null;
  private reportPending = false;
  private dark = false;
  private base: Ranges = {x: {min: 0, max: 1}, y: {min: 0, max: 1}};
  private draws = 0;
  private traces: CurveTraces[] = [];
  private traceTimer: ReturnType<typeof setTimeout> | null = null;
  private vertices: (number | null)[] = [];
  private nullclines: Nullclines | null = null;
  private dfield: Dfield | null = null;
  private layers: Layer[] = [];
  private marks: Marks | null = null;
  private markLayers: MarkLayer[] = [];
  private hiddenLayers = new Set<LayerKey | MarkKey>();
  private runs: PlotModel[] = [];
  private showRuns = true;
  private runsDrawn = 0;
  private highlighted: {run: number; curve: number} | null = null;
  /** each earlier run's paths for the frame they were traced for: a run does
      not change, so only a new view traces it again */
  private runPaths = new WeakMap<PlotModel,
    {frame: PixelFrame; dark: boolean; drawn: number; paths: {p: Path2D; css: string; fill: boolean}[]}>();
  /** the earlier runs drawn once into a canvas of the chart's size, for the
      frame, theme and runs it was drawn for: every other draw (each append
      of a live run) copies it instead of stroking the runs again (W82: a
      run of a million rows cost the frame about a second to stroke) */
  private runsLayer: {canvas: HTMLCanvasElement; frame: PixelFrame; dark: boolean; runs: PlotModel[]; drawn: number} | null = null;
  /** the nullclines, the direction field and the flows drawn once into a
      canvas of the chart's size, for the frame, theme, data and hidden
      layers they were drawn for: every other draw (each append of a live
      run) copies it instead of tracing them again, as runsLayer does */
  private phaseLayer: {canvas: HTMLCanvasElement; frame: PixelFrame; dark: boolean; nullclines: Nullclines | null;
    dfield: Dfield | null; hidden: string; drawn: Map<LayerKey, number>} | null = null;
  private layerDrawn = new Map<LayerKey | MarkKey, number>();
  /** called with the plotting area each time uPlot makes a new one */
  onArea: (area: HTMLElement) => void = () => {};

  constructor(private readonly root: HTMLElement, private readonly cb: ChartCallbacks) {}

  /** draws `model`; `view` is the core's window (Viewaxes), `viewport` the user's zoom */
  set(model: PlotModel, view: Ranges | null, viewport: Viewport, dark: boolean): void {
    const rebuild = !this.u || !this.model || this.model.mode !== model.mode
      || this.model.curves.length !== model.curves.length || this.dark !== dark
      || this.model.curves.some((c, i) => c.color !== model.curves[i].color || c.line !== model.curves[i].line
        || c.label !== model.curves[i].label || c.cssColor !== model.curves[i].cssColor);
    this.visible = model.curves.map(c => c.visible !== false);
    this.model = model;
    this.dark = dark;
    this.base = {
      x: view?.x ?? padded(model.xRange) ?? {min: 0, max: 1},
      y: view?.y ?? padded(model.yRange) ?? {min: 0, max: 1},
    };
    if (rebuild) this.create();
    else {
      this.u!.setData(this.data(), false); /* new rows (an append): the same chart, new paths */
      this.visible.forEach((show, i) => this.u!.setSeries(i + 1, {show}));
    }
    this.applyViewport(viewport); /* draws */
  }

  /** the plotting area in canvas pixels and the ranges it shows */
  private frame(u: uPlot): PixelFrame {
    return {xmin: u.scales.x.min!, xmax: u.scales.x.max!, ymin: u.scales.y.min!, ymax: u.scales.y.max!,
      left: u.bbox.left, top: u.bbox.top, width: u.bbox.width, height: u.bbox.height};
  }

  /* path builders that leave out what changes no pixel (decimate.ts): for
     lines (a LineTrace in the phase plane, a ColumnTrace against time), and
     for points in either mode. An append carries a line's trace on from
     where it stopped. A line of more
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
      tr.current = this.model?.mode === 1 ? new ColumnTrace(f, Math.max(0, i0)) : new LineTrace(f, Math.max(0, i0));
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
    const fg = () => cssVar('--fg-muted'), grid = () => cssVar('--grid'), font = cssVar('--plot-font');
    const axis = (label: string): uPlot.Axis => ({
      label, stroke: fg, font, labelFont: font, grid: {stroke: grid, width: 1}, ticks: {stroke: grid, width: 1},
    });
    const series: uPlot.Series[] = m.curves.map((c, i) => {
      const color = c.cssColor ?? curveColor(c.color, this.dark);
      const s: uPlot.Series = {label: c.label, stroke: color, width: 1.5, show: this.visible[i], points: {show: false}};
      if (!c.line) {
        s.paths = this.pointPath(c.radius);
        s.fill = color;
        s.points = {show: false, size: 2 * c.radius + 1, width: 0, fill: color};
      } else {
        s.paths = this.linePath; /* a time plot's too: uPlot's own walks every row at every draw (W82) */
      }
      if (m.mode === 2) s.facets = [{scale: 'x', auto: false}, {scale: 'y', auto: false}];
      return s;
    });
    const {width, height} = this.size();
    const opts: uPlot.Options = {
      mode: m.mode,
      width, height,
      legend: {show: false},
      /* the range as given: with no data yet (before a first run) uPlot's
         setScale passes it through the scale's range function, whose default
         pads it and rounds it to "nice" values (the model's -110..60 read as
         -130..80 until the run's rows came, W85) */
      scales: {x: {time: false, auto: false, range: asGiven}, y: {auto: false, range: asGiven}},
      axes: [axis(m.xLabel), axis(m.yLabel)],
      series: [{}, ...series],
      cursor: {
        drag: {x: true, y: true, uni: 20, setScale: true},
        points: {show: false},
        bind: {dblclick: () => () => { this.reset(); return null; }},
      },
      hooks: {
        setScale: [() => this.scaleChanged()],
        drawAxes: [u => this.drawPhase(u)], /* after the axes, before the curves */
        draw: [u => { this.drawMarks(u); this.drawHighlight(u); this.drawn(); }], /* over the curves */
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

  /** the window's marks (null: none) */
  setMarks(marks: Marks | null): void {
    if (marks === this.marks) return;
    this.marks = marks;
    this.markLayers = markLayers(marks);
    this.u?.redraw(false, false);
    /* Greek letters come from the font's Greek subset, which a canvas does
       not ask for itself: load it, then draw again */
    const greek = marks?.text.some(t => /[\u0370-\u03ff]/.test(t.plain));
    if (greek && typeof document !== 'undefined' && document.fonts?.load)
      document.fonts.load(`${textPx(2)}px Inter`, '\u03b1\u03b2').then(() => this.u?.redraw(false, false), () => {});
  }

  /** earlier runs of the window, oldest first, and whether they are shown */
  setRuns(runs: PlotModel[], show: boolean): void {
    if (runs === this.runs && show === this.showRuns) return;
    this.runs = runs;
    this.showRuns = show;
    this.u?.redraw(false, false);
  }

  setLayerVisible(key: LayerKey | MarkKey, show: boolean): void {
    if (show) this.hiddenLayers.delete(key);
    else this.hiddenLayers.add(key);
    this.u?.redraw(false, false);
  }

  isLayerVisible(key: LayerKey | MarkKey): boolean {
    return !this.hiddenLayers.has(key);
  }

  private drawPhase(u: uPlot): void {
    this.layerDrawn.clear();
    this.drawRuns(u);
    this.drawFrozen(u);
    if (!this.layers.length) return;
    const f = this.frame(u), {width, height} = u.ctx.canvas;
    const hidden = [...this.hiddenLayers].sort().join();
    let layer = this.phaseLayer;
    if (!layer || layer.nullclines !== this.nullclines || layer.dfield !== this.dfield || layer.dark !== this.dark
      || layer.hidden !== hidden || layer.canvas.width !== width || layer.canvas.height !== height
      || !sameFrame(layer.frame, f)) {
      const canvas = layer?.canvas ?? document.createElement('canvas');
      canvas.width = width; /* also clears it */
      canvas.height = height;
      const ctx = canvas.getContext('2d');
      if (!ctx) return;
      layer = this.phaseLayer = {canvas, frame: f, dark: this.dark, nullclines: this.nullclines, dfield: this.dfield,
        hidden, drawn: this.strokePhase(u, f, ctx)};
    }
    for (const [key, n] of layer.drawn) this.layerDrawn.set(key, (this.layerDrawn.get(key) ?? 0) + n);
    u.ctx.drawImage(layer.canvas, 0, 0);
  }

  /** the nullclines, the direction field and the flows into `ctx`, clipped to the plotting area;
      what each layer drew */
  private strokePhase(u: uPlot, f: PixelFrame, ctx: CanvasRenderingContext2D): Map<LayerKey, number> {
    const r = uPlot.pxRatio, nc = this.nullclines, df = this.dfield, drawn = new Map<LayerKey, number>();
    const stroke = (key: LayerKey, color: number, width: number, dash: number[], trace: (p: Path2D) => number) => {
      if (this.hiddenLayers.has(key)) return;
      const p = new Path2D();
      const n = trace(p);
      drawn.set(key, (drawn.get(key) ?? 0) + n);
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
    return drawn;
  }

  /** the plotting area as the clip of `ctx` (the chart's own unless another
      is given), then `draw`, then as it was */
  private clipped(u: uPlot, draw: (ctx: CanvasRenderingContext2D) => void, ctx = u.ctx): void {
    ctx.save();
    ctx.beginPath();
    ctx.rect(u.bbox.left, u.bbox.top, u.bbox.width, u.bbox.height);
    ctx.clip();
    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';
    ctx.setLineDash([]);
    draw(ctx);
    ctx.restore();
  }

  /** a mark layer's path, stroked (and filled) in `css`, counted as drawn */
  private strokeLayer(ctx: CanvasRenderingContext2D, key: MarkKey, css: string, width: number,
    trace: (p: Path2D) => number, fill = false): void {
    if (this.hiddenLayers.has(key)) return;
    const p = new Path2D(), n = trace(p);
    this.layerDrawn.set(key, (this.layerDrawn.get(key) ?? 0) + n);
    ctx.strokeStyle = css;
    ctx.lineWidth = width * uPlot.pxRatio;
    ctx.stroke(p);
    if (fill) {
      ctx.fillStyle = css;
      ctx.fill(p);
    }
  }

  /** earlier runs: under everything else, in their curves' colours, lighter */
  private drawRuns(u: uPlot): void {
    this.runsDrawn = 0;
    if (!this.runs.length || !this.showRuns) return;
    const f = this.frame(u), {width, height} = u.ctx.canvas;
    let layer = this.runsLayer;
    if (!layer || layer.runs !== this.runs || layer.dark !== this.dark || layer.canvas.width !== width
      || layer.canvas.height !== height || !sameFrame(layer.frame, f)) {
      const canvas = layer?.canvas ?? document.createElement('canvas');
      canvas.width = width; /* also clears it */
      canvas.height = height;
      const ctx = canvas.getContext('2d');
      if (!ctx) return;
      layer = this.runsLayer = {canvas, frame: f, dark: this.dark, runs: this.runs, drawn: this.strokeRuns(u, f, ctx)};
    }
    this.runsDrawn = layer.drawn;
    u.ctx.drawImage(layer.canvas, 0, 0);
  }

  /** the earlier runs into `ctx`, clipped to the plotting area; the vertices drawn */
  private strokeRuns(u: uPlot, f: PixelFrame, target: CanvasRenderingContext2D): number {
    const r = uPlot.pxRatio;
    let total = 0;
    this.clipped(u, ctx => {
      ctx.globalAlpha = 0.35;
      ctx.lineWidth = 1.25 * r;
      for (const run of this.runs) {
        let cached = this.runPaths.get(run);
        if (!cached || cached.dark !== this.dark || !sameFrame(cached.frame, f)) {
          let drawn = 0;
          const paths = run.curves.filter(c => c.visible !== false).map(c => {
            const p = new Path2D();
            drawn += traceFrozen(c.xs, c.ys, c.line, f, Math.max(1, c.radius) * r, p);
            return {p, css: c.cssColor ?? curveColor(c.color, this.dark), fill: !c.line};
          });
          cached = {frame: f, dark: this.dark, drawn, paths};
          this.runPaths.set(run, cached);
        }
        total += cached.drawn;
        for (const {p, css, fill} of cached.paths) {
          ctx.strokeStyle = css;
          ctx.stroke(p);
          if (fill) {
            ctx.fillStyle = css;
            ctx.fill(p);
          }
        }
      }
    }, target);
    return total;
  }

  /** frozen curves: under the curves, like curves */
  private drawFrozen(u: uPlot): void {
    const m = this.marks;
    if (!m?.frozen.length) return;
    const f = this.frame(u), r = uPlot.pxRatio;
    this.clipped(u, ctx => m.frozen.forEach((c, i) => {
      const css = curveColor(c.color, this.dark);
      this.strokeLayer(ctx, `frozen-${i}`, css, 1.5, p => traceFrozen(c.xs, c.ys, c.line, f, 1.5 * r, p), !c.line);
    }));
  }

  /** equilibria, arrows, markers and text: over the curves */
  private drawMarks(u: uPlot): void {
    const m = this.marks;
    if (!m) return;
    const f = this.frame(u), r = uPlot.pxRatio, fg = cssVar('--fg'), bg = cssVar('--surface');
    this.clipped(u, ctx => {
      for (const a of m.arrows)
        this.strokeLayer(ctx, 'arrows', curveColor(a.color, this.dark), 1.5, p => (arrowPath(a, f, p) ? 1 : 0));
      for (const k of m.markers)
        this.strokeLayer(ctx, 'markers', curveColor(k.color, this.dark), 1.5,
          p => (markerPath(k, f, MARKER_PX * r, p) ? 1 : 0));
      /* a stable one filled, so the three kinds differ by more than their outline */
      for (const e of m.equilibria)
        this.strokeLayer(ctx, 'equilibria', fg, 1.5, p => (equilibriumPath(e, f, 5 * r, p) ? 1 : 0), e.type === 'stable');
      if (m.text.length && !this.hiddenLayers.has('text')) {
        ctx.textAlign = 'left'; /* uPlot leaves its axes' alignment set */
        ctx.textBaseline = 'alphabetic';
        ctx.lineWidth = 3 * r;
        ctx.strokeStyle = bg; /* a halo, so the text keeps its contrast over curves */
        ctx.fillStyle = fg;
        let drawn = 0;
        for (const t of m.text) {
          const base = textPx(t.size) * r, at = pixelOf(f, t.x, t.y);
          if (!at) continue;
          let x = at.x;
          for (const run of t.runs) {
            ctx.font = `${run.small ? Math.round(base * 0.75) : base}px Inter, system-ui, sans-serif`;
            const y = at.y - run.rise * 0.45 * base;
            ctx.strokeText(run.text, x, y);
            ctx.fillText(run.text, x, y);
            x += ctx.measureText(run.text).width;
          }
          drawn++;
        }
        this.layerDrawn.set('text', drawn);
      }
    });
  }

  private drawn(): void {
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
    this.requested = null; /* drawn: the scales are the view now */
  }

  /** a new view from a gesture */
  setView(r: Ranges): void {
    this.requested = r;
    this.cb.onViewport({x: r.x, y: r.y});
  }

  /** back to the core's view */
  reset(): void {
    this.requested = null;
    this.cb.onViewport({x: null, y: null});
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
      /* no curve shown (every one hidden from the legend): uPlot's fallback range is not a zoom */
      if (!u.series.some((s, i) => i > 0 && s.show)) return;
      this.cb.onViewport({x: {min: x.min, max: x.max}, y: {min: y.min, max: y.max}});
    });
  }

  /** the view: the last one asked for until it is drawn, then the drawn one. A gesture that
      follows another before the next frame (a key and the next, key repeat, a slow machine)
      starts from what the first one asked for rather than undoing it (W20) */
  ranges(): Ranges {
    if (this.requested) return this.requested;
    const u = this.u!;
    return {x: {min: u.scales.x.min!, max: u.scales.x.max!}, y: {min: u.scales.y.min!, max: u.scales.y.max!}};
  }

  /** the point nearest to (px, py) of the plotting area, within maxDist CSS pixels */
  hit(px: number, py: number, maxDist: number): (Nearest & {run: number}) | null {
    const u = this.u, m = this.model;
    if (!u || !m) return null;
    const {x, y} = this.ranges();
    const frame = {xmin: x.min, xmax: x.max, ymin: y.min, ymax: y.max,
      width: u.over.clientWidth, height: u.over.clientHeight};
    const current = nearestPoint(m.curves, this.visible, frame, px, py, maxDist, true);
    let best = current ? {...current, run: -1} : null;
    if (this.showRuns) this.runs.forEach((r, run) => {
      const h = nearestPoint(r.curves, r.curves.map(c => c.visible !== false), frame, px, py, best?.dist ?? maxDist, true);
      if (h) best = {...h, run};
    });
    this.highlight(best);
    this.cb.onTrace?.(best);
    return best;
  }

  highlight(hit: {run: number; curve: number} | null): void {
    if (this.highlighted?.run === hit?.run && this.highlighted?.curve === hit?.curve) return;
    this.highlighted = hit;
    this.u?.redraw(false, false);
  }

  private drawHighlight(u: uPlot): void {
    const h = this.highlighted;
    const c = h && (h.run < 0 ? this.model : this.runs[h.run])?.curves[h.curve];
    if (!c || c.visible === false || (h!.run >= 0 && !this.showRuns)) return;
    this.clipped(u, ctx => {
      const p = new Path2D(), f = this.frame(u);
      if (c.line) tracePolyline(c.xs, c.ys, f, p);
      else tracePoints(c.xs, c.ys, 0, c.xs.length - 1, f, (c.radius + 1) * uPlot.pxRatio,
        (x, y) => { p.moveTo(x + 3, y); p.arc(x, y, 3 * uPlot.pxRatio, 0, Math.PI * 2); });
      ctx.strokeStyle = c.cssColor ?? curveColor(c.color, this.dark);
      ctx.lineWidth = 3.5 * uPlot.pxRatio;
      ctx.stroke(p);
    });
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

  /** the canvas's own pixels, RGB (no alpha): what a frame, GIF or kinescope
      writer's `pixels` ask wants (docs/protocol.md, plot/kinescopeRender.ts) */
  pixels(): {w: number; h: number; rgb: Uint8ClampedArray} | null {
    return this.u ? canvasPixels(this.u.ctx.canvas) : null;
  }

  info(): ChartInfo | null {
    const u = this.u, m = this.model;
    if (!u || !m) return null;
    return {
      mode: m.mode,
      curves: m.curves.map((c, i) => ({label: c.label, points: c.xs.length, visible: this.visible[i],
        color: c.cssColor ?? curveColor(c.color, this.dark)})),
      ...this.ranges(),
      width: u.over.clientWidth,
      height: u.over.clientHeight,
      draws: this.draws,
      tracing: this.traceTimer !== null,
      vertices: this.vertices.slice(),
      runs: {count: this.runs.length, shown: this.showRuns, drawn: this.runsDrawn},
      layers: [...this.layers, ...this.markLayers].map(l => ({...l, visible: this.isLayerVisible(l.key),
        drawn: this.layerDrawn.get(l.key) ?? 0, css: curveColor(l.color, this.dark)})),
    };
  }

  destroy(): void {
    if (this.traceTimer !== null) clearTimeout(this.traceTimer);
    this.traceTimer = null;
    this.runsLayer = null;
    this.phaseLayer = null;
    this.u?.destroy();
    this.u = null;
  }
}
