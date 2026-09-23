/* The plot widget: a uPlot chart for a PlotModel. It draws, maps between
   data and screen, and finds the point under a position; the gestures that
   change the view are in interactions.ts. It owns no application state: the
   view that owns it (ui/PlotView.tsx) feeds it the store's viewport and puts
   what it reports back into the store. */
import uPlot from 'uplot';
import {curveColor} from './colors';
import type {PlotModel} from './model';
import {nearestPoint, type Nearest} from './nearest';
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
}

function extent(arrays: Float64Array[]): Range | null {
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
    else this.u!.setData(this.data(), false);
    this.applyViewport(viewport);
  }

  private data(): uPlot.AlignedData {
    const m = this.model!;
    if (m.mode === 1) return [m.curves[0]?.xs ?? new Float64Array(0), ...m.curves.map(c => c.ys)] as uPlot.AlignedData;
    return [null, ...m.curves.map(c => [c.xs, c.ys])] as unknown as uPlot.AlignedData;
  }

  private create(): void {
    const m = this.model!;
    this.u?.destroy();
    const fg = cssVar('--fg-muted') || '#666', grid = cssVar('--grid') || '#eee', font = cssVar('--plot-font');
    const axis = (label: string): uPlot.Axis => ({
      label, stroke: fg, font, labelFont: font, grid: {stroke: grid, width: 1}, ticks: {stroke: grid, width: 1},
    });
    const series: uPlot.Series[] = m.curves.map((c, i) => {
      const color = curveColor(c.color, this.dark);
      const s: uPlot.Series = {label: c.label, stroke: color, width: 1.5, show: this.visible[i], points: {show: false}};
      if (!c.line) {
        s.paths = uPlot.paths.points!();
        s.fill = color;
        s.points = {show: false, size: 2 * c.radius + 1, width: 0, fill: color};
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
      hooks: {setScale: [() => this.scaleChanged()]},
    };
    this.u = new uPlot(opts, this.data(), this.root);
    this.onArea(this.u.over);
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
    };
  }

  destroy(): void {
    this.u?.destroy();
    this.u = null;
  }
}
