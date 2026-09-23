/* The AUTO diagram's chart: a uPlot chart in xy mode for a DiagramModel
   (docs/ui-v2.md T11a). Branches are its series (lines, the unstable ones
   dashed), the labelled points are drawn in its draw hook (a cross at the
   value, and at the minimum of a periodic point, with the label's type and
   number beside it). Like plot/chart.ts it owns no application state: the
   view (ui/AutoView.tsx) feeds it the store's viewport and puts what it
   reports back into the store; the gestures are plot/interactions.ts's. */
import uPlot from 'uplot';
import type {Range, Viewport} from '../store/plots';
import {curveColor} from './colors';
import {nearestVertex, type DiagramHit, type DiagramModel} from './diagramModel';
import type {Ranges} from './viewmath';

export interface DiagramChartInfo {
  curves: {branch: number; type: number; kind: string; stable: boolean; which: 'y' | 'y2'; points: number;
    dashed: boolean; width: number; color: string; hopf: number; first: number[]}[];
  labels: {point: number; lab: number; sym: string; x: number; y: number; y2: number | null}[];
  /** labels whose name is written beside the cross (all of them when few are in view) */
  named: number;
  x: Range;
  y: Range;
  width: number;
  height: number;
  draws: number;
}

/** labels in view up to which each one's name is written (more only get their cross) */
const NAMED_MAX = 40;
const DASH = [6, 4];

function cssVar(name: string): string {
  return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
}

/** a palette colour of the core (0 the foreground, 20..29 red..purple) as a curve colour of the theme */
export function paletteColor(c: number, dark: boolean): string {
  return curveColor(c >= 20 && c <= 29 ? c - 19 : 0, dark);
}

/** the data's extent with a margin, for a diagram the core has no axes for yet */
function extent(m: DiagramModel, axis: 'x' | 'y'): Range {
  let min = Infinity, max = -Infinity;
  for (const c of m.curves) {
    const a = axis === 'x' ? c.xs : c.ys;
    for (let i = 0; i < a.length; i++) {
      if (a[i] < min) min = a[i];
      if (a[i] > max) max = a[i];
    }
  }
  if (!(min <= max)) return {min: 0, max: 1};
  if (min === max) return {min: min - 1, max: max + 1};
  const pad = (max - min) * 0.05;
  return {min: min - pad, max: max + pad};
}

export class DiagramChart {
  private u: uPlot | null = null;
  private model: DiagramModel | null = null;
  private dark = false;
  private applying = false;
  private reportPending = false;
  private base: Ranges = {x: {min: 0, max: 1}, y: {min: 0, max: 1}};
  private draws = 0;
  private named = 0;
  private styleKey = '';
  onArea: (area: HTMLElement) => void = () => {};

  constructor(private readonly root: HTMLElement, private readonly cb: {onViewport(v: Viewport, push: boolean): void}) {}

  /** draws `model`; `axes` is the core's view of the diagram, `viewport` the user's zoom */
  set(model: DiagramModel, axes: Ranges | null, viewport: Viewport, dark: boolean): void {
    const key = JSON.stringify([dark, model.xLabel, model.yLabel,
      model.curves.map(c => [c.color, c.width, c.dashed])]);
    this.model = model;
    this.dark = dark;
    this.base = axes ?? {x: extent(model, 'x'), y: extent(model, 'y')};
    if (!this.u || key !== this.styleKey) {
      this.styleKey = key;
      this.create();
    } else this.u.setData(this.data(), false);
    this.applyViewport(viewport);
  }

  /* uPlot's xy mode needs a series: an empty one stands in before the first branch */
  private data(): uPlot.AlignedData {
    const curves = this.model!.curves;
    const series = curves.length ? curves.map(c => [c.xs, c.ys]) : [[new Float64Array(0), new Float64Array(0)]];
    return [null, ...series] as unknown as uPlot.AlignedData;
  }

  /* a polyline through every vertex, in order: the branches are short
     (thousands of points), so nothing is left out; uPlot clips it to the area */
  private linePath: uPlot.Series.PathBuilder = (u, si) => {
    const c = this.model?.curves[si - 1], stroke = new Path2D();
    if (c) {
      let pen = false;
      for (let i = 0; i < c.xs.length; i++) {
        const x = c.xs[i], y = c.ys[i];
        if (!Number.isFinite(x) || !Number.isFinite(y)) {
          pen = false;
          continue;
        }
        const px = u.valToPos(x, 'x', true), py = u.valToPos(y, 'y', true);
        if (pen) stroke.lineTo(px, py);
        else stroke.moveTo(px, py);
        pen = true;
      }
    }
    return {stroke, fill: null, clip: null, band: null, flags: 1};
  };

  private create(): void {
    const m = this.model!;
    this.u?.destroy();
    /* the theme's colours when drawn: a new theme's variables are set after this chart is made */
    const fg = () => cssVar('--fg-muted') || '#666', grid = () => cssVar('--grid') || '#eee', font = cssVar('--plot-font');
    const axis = (label: string): uPlot.Axis => ({
      label, stroke: fg, font, labelFont: font, grid: {stroke: grid, width: 1}, ticks: {stroke: grid, width: 1},
    });
    const facets: uPlot.Series.Facet[] = [{scale: 'x', auto: false}, {scale: 'y', auto: false}];
    const series: uPlot.Series[] = m.curves.map(c => ({
      label: `Branch ${c.branch}`,
      stroke: paletteColor(c.color, this.dark),
      width: c.width,
      dash: c.dashed ? DASH : undefined,
      paths: this.linePath,
      points: {show: false},
      facets,
    }));
    if (!series.length) series.push({label: '', paths: this.linePath, points: {show: false}, facets});
    const {width, height} = this.size();
    this.u = new uPlot({
      mode: 2,
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
        draw: [u => this.drawLabels(u)],
      },
    }, this.data(), this.root);
    this.onArea(this.u.over);
  }

  /* the labelled points: a cross at the value (and at the minimum), the name beside it */
  private drawLabels(u: uPlot): void {
    this.draws++;
    const m = this.model;
    if (!m) return;
    const ctx = u.ctx, b = u.bbox, r = uPlot.pxRatio, arm = 4 * r;
    const inside = (x: number, y: number) => x >= b.left && x <= b.left + b.width && y >= b.top && y <= b.top + b.height;
    const shown = m.labels.map(l => ({l, px: u.valToPos(l.x, 'x', true), py: u.valToPos(l.y, 'y', true),
      py2: l.y2 === null ? null : u.valToPos(l.y2, 'y', true)})).filter(s => inside(s.px, s.py)
      || (s.py2 !== null && inside(s.px, s.py2)));
    const fg = cssVar('--fg') || '#000';
    ctx.save();
    ctx.beginPath();
    ctx.rect(b.left, b.top, b.width, b.height);
    ctx.clip();
    ctx.strokeStyle = fg;
    ctx.fillStyle = fg;
    ctx.lineWidth = 1.25 * r;
    ctx.font = `${Math.round(11 * r)}px Inter, system-ui, sans-serif`;
    ctx.textBaseline = 'top';
    this.named = shown.length <= NAMED_MAX ? shown.length : 0;
    for (const s of shown) {
      for (const y of s.py2 === null ? [s.py] : [s.py, s.py2]) {
        ctx.beginPath();
        ctx.moveTo(s.px - arm, y);
        ctx.lineTo(s.px + arm, y);
        ctx.moveTo(s.px, y - arm);
        ctx.lineTo(s.px, y + arm);
        ctx.stroke();
      }
      if (this.named) ctx.fillText(`${s.l.sym ? s.l.sym + ' ' : ''}${s.l.lab}`, s.px + 5 * r, s.py + 3 * r);
    }
    ctx.restore();
  }

  private size(): {width: number; height: number} {
    const r = this.root.getBoundingClientRect();
    return {width: Math.max(120, Math.floor(r.width)), height: Math.max(120, Math.floor(r.height))};
  }

  resize(): void {
    this.u?.setSize(this.size());
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

  setView(r: Ranges, push: boolean): void {
    this.cb.onViewport({x: r.x, y: r.y}, push);
  }

  /** back to the core's view */
  reset(): void {
    this.cb.onViewport({x: null, y: null}, true);
  }

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

  hit(px: number, py: number, maxDist: number): DiagramHit | null {
    const u = this.u, m = this.model;
    if (!u || !m) return null;
    const {x, y} = this.ranges();
    return nearestVertex(m, {xmin: x.min, xmax: x.max, ymin: y.min, ymax: y.max,
      width: u.over.clientWidth, height: u.over.clientHeight}, px, py, maxDist);
  }

  /** where vertex `index` of curve `curve` is, in CSS pixels from the chart's root */
  position(curve: number, index: number): {left: number; top: number} | null {
    const u = this.u, c = this.model?.curves[curve];
    if (!u || !c || index < 0 || index >= c.xs.length) return null;
    const o = u.over.getBoundingClientRect(), r = this.root.getBoundingClientRect();
    return {left: o.left - r.left + u.valToPos(c.xs[index], 'x'), top: o.top - r.top + u.valToPos(c.ys[index], 'y')};
  }

  png(): string | null {
    return this.u ? this.u.ctx.canvas.toDataURL('image/png') : null;
  }

  info(): DiagramChartInfo | null {
    const u = this.u, m = this.model;
    if (!u || !m) return null;
    return {
      curves: m.curves.map(c => ({branch: c.branch, type: c.type, kind: c.kind, stable: c.stable, which: c.which,
        points: c.xs.length, dashed: c.dashed, width: c.width, color: paletteColor(c.color, this.dark), hopf: c.hopf,
        first: [c.xs[0], c.ys[0], c.idx[0]]})),
      labels: m.labels.map(l => ({point: l.point, lab: l.lab, sym: l.sym, x: l.x, y: l.y, y2: l.y2})),
      named: this.named,
      ...this.ranges(),
      width: u.over.clientWidth,
      height: u.over.clientHeight,
      draws: this.draws,
    };
  }

  destroy(): void {
    this.u?.destroy();
    this.u = null;
  }
}

/* the one diagram chart, for the test hook */
let current: DiagramChart | null = null;

export function setDiagramChart(c: DiagramChart | null): void {
  current = c;
}

export function diagramChart(): DiagramChart | null {
  return current;
}
