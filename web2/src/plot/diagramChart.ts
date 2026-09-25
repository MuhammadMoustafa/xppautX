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
import {fitRanges, labelShape, nearestVertex, type DiagramHit, type DiagramModel, type LabelShape} from './diagramModel';
import {placeLabels} from './labelPlace';
import type {Ranges} from './viewmath';

export interface DiagramChartInfo {
  curves: {branch: number; type: number; kind: string; stable: boolean; which: 'y' | 'y2'; points: number;
    dashed: boolean; width: number; color: string; hopf: number; first: number[]}[];
  labels: {point: number; lab: number; sym: string; x: number; y: number; y2: number | null}[];
  /** labels whose name is written beside the cross (all of them when few are in view) */
  named: number;
  nameTops: number[];
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

/** a labelled point's shape (T29), `r` canvas pixels from its centre; the
    caller sets `ctx`'s stroke/fill style first. Outlines except HB's
    filled circle, so a dense diagram still reads its lines through them. */
function strokeShape(ctx: CanvasRenderingContext2D, shape: LabelShape, x: number, y: number, r: number): void {
  switch (shape) {
    case 'circle':
      ctx.beginPath();
      ctx.arc(x, y, r * 0.75, 0, 2 * Math.PI);
      ctx.fill();
      return;
    case 'triangle':
    case 'invTriangle': {
      const up = shape === 'triangle' ? -1 : 1;
      ctx.beginPath();
      ctx.moveTo(x, y + up * r);
      ctx.lineTo(x + r * 0.9, y - up * r * 0.65);
      ctx.lineTo(x - r * 0.9, y - up * r * 0.65);
      ctx.closePath();
      ctx.stroke();
      return;
    }
    case 'diamond':
      ctx.beginPath();
      ctx.moveTo(x, y - r);
      ctx.lineTo(x + r, y);
      ctx.lineTo(x, y + r);
      ctx.lineTo(x - r, y);
      ctx.closePath();
      ctx.stroke();
      return;
    case 'square':
      ctx.strokeRect(x - r * 0.8, y - r * 0.8, r * 1.6, r * 1.6);
      return;
    case 'star':
      ctx.beginPath();
      for (let i = 0; i < 10; i++) {
        const rad = i % 2 === 0 ? r : r * 0.45, a = -Math.PI / 2 + (Math.PI / 5) * i;
        const px = x + rad * Math.cos(a), py = y + rad * Math.sin(a);
        if (i) ctx.lineTo(px, py); else ctx.moveTo(px, py);
      }
      ctx.closePath();
      ctx.stroke();
      return;
    case 'bar':
      ctx.beginPath();
      ctx.moveTo(x, y - r);
      ctx.lineTo(x, y + r);
      ctx.stroke();
      return;
    case 'tick':
      ctx.beginPath();
      ctx.arc(x, y, r * 0.35, 0, 2 * Math.PI);
      ctx.fill();
      return;
    case 'cross':
    default: {
      /* a bold diagonal cross, as the key's × */
      const w = ctx.lineWidth, d = r * 0.8;
      ctx.lineWidth = w * 1.8;
      ctx.beginPath();
      ctx.moveTo(x - d, y - d);
      ctx.lineTo(x + d, y + d);
      ctx.moveTo(x + d, y - d);
      ctx.lineTo(x - d, y + d);
      ctx.stroke();
      ctx.lineWidth = w;
      return;
    }
  }
}

export class DiagramChart {
  private u: uPlot | null = null;
  private model: DiagramModel | null = null;
  private dark = false;
  private applying = false;
  /* the view last asked for (setView, fit), until applyViewport draws it */
  private requested: Ranges | null = null;
  private reportPending = false;
  private base: Ranges = {x: {min: 0, max: 1}, y: {min: 0, max: 1}};
  private draws = 0;
  private named = 0;
  /** where the names were written (their tops, CSS pixels of the canvas), for the test hook */
  private nameTops: number[] = [];
  private styleKey = '';
  onArea: (area: HTMLElement) => void = () => {};

  constructor(private readonly root: HTMLElement, private readonly cb: {onViewport(v: Viewport, push: boolean): void}) {}

  /** draws `model`; `axes` is the core's view of the diagram, `viewport` the user's zoom */
  set(model: DiagramModel, axes: Ranges | null, viewport: Viewport, dark: boolean): void {
    const key = JSON.stringify([dark, model.xLabel, model.yLabel,
      model.curves.map(c => [c.color, c.width, c.dashed])]);
    this.model = model;
    this.dark = dark;
    this.base = axes ?? fitRanges(model);
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

  /* the labelled points: a shape of its type at the value (and at the
     minimum), the name beside it (T29: not every type a cross, so a long
     run's plain numbered points do not read as a row of them) */
  private drawLabels(u: uPlot): void {
    this.draws++;
    const m = this.model;
    if (!m) return;
    const ctx = u.ctx, b = u.bbox, r = uPlot.pxRatio, arm = 4 * r;
    const inside = (x: number, y: number) => x >= b.left && x <= b.left + b.width && y >= b.top && y <= b.top + b.height;
    const shown = m.labels.map(l => ({l, px: u.valToPos(l.x, 'x', true), py: u.valToPos(l.y, 'y', true),
      py2: l.y2 === null ? null : u.valToPos(l.y2, 'y', true)})).filter(s => inside(s.px, s.py)
      || (s.py2 !== null && inside(s.px, s.py2)));
    const fg = cssVar('--fg') || '#000', fgMuted = cssVar('--fg-muted') || '#666';
    ctx.save();
    ctx.beginPath();
    ctx.rect(b.left, b.top, b.width, b.height);
    ctx.clip();
    ctx.lineWidth = 1.25 * r;
    ctx.font = `${Math.round(11 * r)}px Inter, system-ui, sans-serif`;
    ctx.textBaseline = 'top';
    this.named = shown.length <= NAMED_MAX ? shown.length : 0;
    const names = shown.map(s => `${s.l.sym ? s.l.sym + ' ' : ''}${s.l.lab}`);
    /* two names at one spot are written a line apart (plot/labelPlace.ts) */
    const tops = this.named ? placeLabels(shown.map((s, i) => ({x: s.px + 5 * r, y: s.py + 3 * r,
      w: ctx.measureText(names[i]).width, h: 13 * r}))) : [];
    this.nameTops = tops.map(t => t / r);
    shown.forEach((s, i) => {
      const shape = labelShape(s.l.sym), light = shape === 'tick';
      ctx.strokeStyle = light ? fgMuted : fg;
      ctx.fillStyle = light ? fgMuted : fg;
      const rad = light ? arm * 0.55 : arm;
      for (const y of s.py2 === null ? [s.py] : [s.py, s.py2]) strokeShape(ctx, shape, s.px, y, rad);
      if (this.named) {
        ctx.fillStyle = fg;
        ctx.fillText(names[i], s.px + 5 * r, tops[i]);
      }
    });
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
    this.requested = null; /* drawn: the scales are the view now */
  }

  setView(r: Ranges, push: boolean): void {
    this.requested = r;
    this.cb.onViewport({x: r.x, y: r.y}, push);
  }

  /** back to the core's view */
  reset(): void {
    this.requested = null;
    this.cb.onViewport({x: null, y: null}, true);
  }

  /** Axes/Fit (T30): the visible curves' own extent, as a zoom (undoable,
      pushed like any other view change). A no-op with no branches yet. */
  fit(): void {
    if (!this.model || !this.model.curves.length) return;
    const r = fitRanges(this.model);
    this.requested = r;
    this.cb.onViewport({x: r.x, y: r.y}, true);
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

  /** the view: the last one asked for until it is drawn, then the drawn one. A gesture that
      follows another before the next frame (a key and the next, key repeat, a slow machine)
      starts from what the first one asked for rather than undoing it (W20) */
  ranges(): Ranges {
    if (this.requested) return this.requested;
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

  /** the plotting area in CSS pixels from the chart's root */
  areaBox(): {left: number; top: number; width: number; height: number} | null {
    const u = this.u;
    if (!u) return null;
    const o = u.over.getBoundingClientRect(), r = this.root.getBoundingClientRect();
    return {left: o.left - r.left, top: o.top - r.top, width: o.width, height: o.height};
  }

  /** where (x, y) of the diagram is, in CSS pixels from the chart's root */
  place(x: number, y: number): {left: number; top: number} | null {
    const u = this.u;
    if (!u || !Number.isFinite(x) || !Number.isFinite(y)) return null;
    const o = u.over.getBoundingClientRect(), r = this.root.getBoundingClientRect();
    return {left: o.left - r.left + u.valToPos(x, 'x'), top: o.top - r.top + u.valToPos(y, 'y')};
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
      nameTops: this.nameTops,
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
