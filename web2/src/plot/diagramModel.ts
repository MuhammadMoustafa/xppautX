/* From the store's diagram to what the AUTO view draws (docs/ui-v2.md T11a).
   Pure: no DOM.

   XPP's convention, kept: a steady state is a line (stable solid and thick,
   unstable thin), a periodic branch is marked at its maximum and minimum
   (the hi and lo values), a two-parameter curve is a line in its kind's
   colour. Here every kind is a line (unstable ones dashed), the periodic
   branches as two lines, max and min, so a branch reads as a curve at any
   zoom. A curve is one run of points that share branch, kind and style and
   follow on (no `new` between them), the way the core drew them: a line
   goes back from each point to the one before it, so a steady-state curve
   starts at the last point of the run before (where the stability changed)
   and a periodic one at the previous periodic point of its branch.

   XPP leaves the start of a periodic branch unjoined to the Hopf point it
   bifurcates from. Here a periodic branch that starts at a Hopf label's
   point is drawn from that point: the run that computed it says which label
   it started from (`from`, T11b), and for a diagram whose data do not say
   (loaded from a file) a periodic branch whose first point lies at a Hopf
   label's parameter (within a small part of the axes' width) and whose
   max..min spans the label's value is taken to start there. */
import type {DiagramAxes, DiagramLabel, DiagramPoints} from '../store/diagram';

export type CurveKind = 'steady' | 'periodic' | 'two-parameter';

export interface DiagramCurve {
  branch: number;
  /** the store's ty: 1 stable steady state, 2 unstable steady, 3 stable periodic, 4 unstable periodic */
  type: number;
  f2: number;
  kind: CurveKind;
  stable: boolean;
  /** the values it draws: y (the maximum for hi and lo) or y2 (the minimum) */
  which: 'y' | 'y2';
  /** palette colour of the run (0 the foreground, 20..29 red..purple) */
  color: number;
  width: number;
  dashed: boolean;
  xs: Float64Array;
  ys: Float64Array;
  /** the point each vertex is (the Hopf point first, for a branch joined to it) */
  idx: Int32Array;
  /** the Hopf label's point the curve starts from, or -1 */
  hopf: number;
}

export interface LabelMark extends DiagramLabel {
  x: number;
  y: number;
  /** the second value, when it differs (a periodic point's minimum) */
  y2: number | null;
}

export interface DiagramModel {
  curves: DiagramCurve[];
  labels: LabelMark[];
  /** Hopf joins: the periodic branch's first point, and the Hopf point it is joined to */
  hopf: {point: number; from: number}[];
  xLabel: string;
  yLabel: string;
}

const PERIODIC = (d: number) => d === 2 || d === 3;

/** point b continues a's curve */
function follows(p: DiagramPoints, a: number, b: number): boolean {
  return !p.nw[b] && p.br[a] === p.br[b] && p.ty[a] === p.ty[b] && p.d[a] === p.d[b] && p.c[a] === p.c[b]
    && p.lw[a] === p.lw[b] && p.f2[a] === p.f2[b];
}

function kindOf(p: DiagramPoints, i: number): CurveKind {
  return p.f2[i] ? 'two-parameter' : PERIODIC(p.d[i]) ? 'periodic' : 'steady';
}

/** the Hopf label's point a periodic branch starting at point `s` bifurcates
    from, or -1: the label its run started from, else one where it lies */
export function hopfOf(p: DiagramPoints, labels: DiagramLabel[], s: number, span: {x: number; y: number}): number {
  if (p.fr[s]) {
    /* the last point before it with that label: labels restart with a new diagram */
    let at = -1;
    for (const l of labels) if (l.lab === p.fr[s] && l.point < s && l.point > at) at = l.point;
    const l = at >= 0 ? labels.find(x => x.point === at) : undefined;
    return l && l.sym === 'HB' && !p.f2[at] && !PERIODIC(p.d[at]) ? at : -1;
  }
  const x = p.x[s], lo = Math.min(p.y[s], p.y2[s]), hi = Math.max(p.y[s], p.y2[s]);
  const tx = 0.05 * span.x, ty = 0.02 * span.y;
  let best = -1, bestScore = Infinity;
  for (const l of labels) {
    const h = l.point;
    if (l.sym !== 'HB' || p.f2[h] || PERIODIC(p.d[h]) || p.br[h] === p.br[s]) continue;
    const dx = Math.abs(p.x[h] - x), yh = p.y[h];
    const dy = yh < lo ? lo - yh : yh > hi ? yh - hi : 0;
    if (!(dx <= tx && dy <= ty)) continue;
    const score = dx / (tx || 1) + dy / (ty || 1);
    if (score < bestScore) {
      bestScore = score;
      best = h;
    }
  }
  return best;
}

function extent(values: number[]): number {
  let min = Infinity, max = -Infinity;
  for (const v of values) {
    if (v < min) min = v;
    if (v > max) max = v;
  }
  return max > min ? max - min : 1;
}

function curve(p: DiagramPoints, s: number, verts: number[], which: 'y' | 'y2', hopf: number): DiagramCurve {
  const kind = kindOf(p, s), ty = p.ty[s];
  const stable = kind === 'two-parameter' || ty === 1 || ty === 3;
  const col = which === 'y' ? p.y : p.y2;
  const xs = new Float64Array(verts.length), ys = new Float64Array(verts.length), idx = new Int32Array(verts.length);
  verts.forEach((v, k) => {
    xs[k] = p.x[v];
    /* the Hopf point is one value: both lines of the branch start there */
    ys[k] = v === hopf ? p.y[v] : col[v];
    idx[k] = v;
  });
  const width = kind === 'steady' ? (p.lw[s] >= 2 ? 2.25 : 1.25) : stable ? 1.75 : 1.25;
  return {branch: p.br[s], type: ty, f2: p.f2[s], kind, stable, which, color: p.c[s], width, dashed: !stable,
    xs, ys, idx, hopf};
}

export function buildDiagramModel(p: DiagramPoints, labels: DiagramLabel[], axes: DiagramAxes | null,
  hideBefore = 0): DiagramModel {
  const n = p.x.length;
  /* Clear (T21): the points before `hideBefore` are earlier branches, left out */
  const first = Math.max(0, Math.min(hideBefore, n));
  const span = axes && axes.xmax > axes.xmin && axes.ymax > axes.ymin
    ? {x: axes.xmax - axes.xmin, y: axes.ymax - axes.ymin} : {x: extent(p.x), y: extent(p.y)};
  const curves: DiagramCurve[] = [], hopf: DiagramModel['hopf'] = [];
  for (let s = first; s < n;) {
    let e = s;
    while (e + 1 < n && follows(p, e, e + 1)) e++;
    if (p.d[s] !== 0) {
      const verts: number[] = [];
      let from = -1;
      const prev = s - 1;
      if (PERIODIC(p.d[s])) {
        /* the previous periodic point of the branch (a change of stability) */
        if (prev >= first && !p.nw[s] && p.br[prev] === p.br[s] && PERIODIC(p.d[prev]) && !p.f2[prev]) verts.push(prev);
        else if (!p.f2[s]) {
          from = hopfOf(p, labels, s, span);
          if (from >= 0) {
            verts.push(from);
            hopf.push({point: s, from});
          }
        }
      } else if (prev >= first && !p.nw[s]) {
        verts.push(prev); /* the line back from the run's first point */
      }
      for (let i = s; i <= e; i++) verts.push(i);
      curves.push(curve(p, s, verts, 'y', from));
      let two = false;
      if (PERIODIC(p.d[s])) for (let i = s; i <= e && !two; i++) two = p.y2[i] !== p.y[i];
      if (two) curves.push(curve(p, s, verts, 'y2', from));
    }
    s = e + 1;
  }
  const marks = labels.filter(l => l.point >= first).map(l => ({...l, x: p.x[l.point], y: p.y[l.point],
    y2: p.y2[l.point] !== p.y[l.point] ? p.y2[l.point] : null}));
  return {curves, labels: marks, hopf, xLabel: axes?.xlabel ?? '', yLabel: axes?.ylabel ?? ''};
}

/* ---- the point under the pointer ---- */

export interface DiagramFrame {
  xmin: number; xmax: number; ymin: number; ymax: number;
  /** the plotting area in CSS pixels */
  width: number; height: number;
}

export interface DiagramHit {
  curve: number;
  /** vertex of the curve */
  index: number;
  dist: number;
}

/** the vertex nearest to (px, py) within maxDist pixels; of vertices as near
    as the nearest (within `tie` pixels more), a labelled point wins, so
    the Hopf point is named rather than the periodic point drawn on it */
export function nearestVertex(m: DiagramModel, f: DiagramFrame, px: number, py: number, maxDist = Infinity,
  tie = 2): DiagramHit | null {
  const sx = f.width / (f.xmax - f.xmin), sy = f.height / (f.ymax - f.ymin);
  const labelled = new Set(m.labels.map(l => l.point));
  const found: DiagramHit[] = [];
  let best = maxDist;
  m.curves.forEach((c, k) => {
    for (let i = 0; i < c.xs.length; i++) {
      const dx = (c.xs[i] - f.xmin) * sx - px, dy = (f.ymax - c.ys[i]) * sy - py;
      const d = Math.hypot(dx, dy);
      if (d <= best + tie) {
        found.push({curve: k, index: i, dist: d});
        if (d < best) best = d;
      }
    }
  });
  const near = found.filter(h => h.dist <= best + tie);
  if (!near.length) return null;
  near.sort((a, b) => a.dist - b.dist);
  return near.find(h => labelled.has(m.curves[h.curve].idx[h.index])) ?? near[0];
}

/** the curve and vertex that show point `point` (its y2 line when `low`), preferring curve `hint` */
export function vertexOf(m: DiagramModel, point: number, low: boolean, hint = -1): {curve: number; index: number} | null {
  const look = (k: number) => {
    const c = m.curves[k];
    if (!c || (c.which === 'y2') !== low) return -1;
    /* a point can be a curve's first vertex (the line back) and its own run's: the later one is its run's */
    for (let i = c.idx.length - 1; i >= 0; i--) if (c.idx[i] === point) return i;
    return -1;
  };
  if (hint >= 0) {
    const i = look(hint);
    if (i >= 0) return {curve: hint, index: i};
  }
  let found: {curve: number; index: number} | null = null;
  m.curves.forEach((_, k) => {
    const i = look(k);
    /* the curve the point belongs to: where it is not only the line back */
    if (i >= 0 && (!found || i > 0)) found = {curve: k, index: i};
  });
  return found;
}

/** the labelled point after (dir 1) or before (-1) point `from` (-1: none yet), wrapping around */
export function stepLabel(labels: DiagramLabel[], from: number, dir: 1 | -1): number | null {
  if (!labels.length) return null;
  const pts = labels.map(l => l.point).sort((a, b) => a - b);
  if (dir > 0) return pts.find(p => p > from) ?? pts[0];
  for (let i = pts.length - 1; i >= 0; i--) if (from < 0 || pts[i] < from) return pts[i];
  return pts[pts.length - 1];
}

/** where a key moves a grab's cursor from point `from` (-1: not on a point
    of the data) among `n` points: arrows and [ ] one point (the ends wrap
    to the first point, as XPP's grab does), Page Up and Down ten, Home and
    End the first and the last, Tab and Shift+Tab the next and the previous
    labelled point (wrapping); null for any other key */
export function grabStep(key: string, shift: boolean, from: number, n: number, labels: DiagramLabel[]): number | null {
  if (n <= 0) return null;
  switch (key) {
    case 'ArrowRight': case 'ArrowDown': case ']':
      return from + 1 < n ? from + 1 : 0;
    case 'ArrowLeft': case 'ArrowUp': case '[':
      return from > 0 ? Math.min(from - 1, n - 1) : 0;
    case 'PageDown':
      return Math.min(Math.max(from, 0) + 10, n - 1);
    case 'PageUp':
      return Math.max(Math.min(from, n - 1) - 10, 0);
    case 'Home':
      return 0;
    case 'End':
      return n - 1;
    case 'Tab':
      return stepLabel(labels.filter(l => l.point < n), from, shift ? -1 : 1);
    default:
      return null;
  }
}

/* ---- the readout ---- */

const TYPES = ['', 'stable steady state', 'unstable steady state', 'stable periodic orbit', 'unstable periodic orbit'];
const CURVES = ['', 'limit point', 'limit point of periodic orbits', 'Hopf', 'torus', 'branch point', 'period doubling',
  'fixed period'];
/** AUTO's label types in words (T23), in the key's order */
const SYMBOLS: Record<string, string> = {
  EP: 'End point', MX: 'No convergence', LP: 'Fold (limit point)', HB: 'Hopf', BP: 'Branch point',
  PD: 'Period doubling', TR: 'Torus', UZ: 'Marked value',
};

/** what each label type means, for the key's tooltips (docs/manual/09-auto.md) */
const SYMBOL_HELP: Record<string, string> = {
  EP: 'End point: where a branch starts or ends normally (a limit, Max points or Stop); the status strip says why it ended',
  MX: 'No convergence: AUTO could not compute the next point, even at the smallest step (Dsmin), and ended the branch; '
    + 'a smaller Ds or Dsmax, a larger Ntst or looser tolerances may get past it',
  LP: 'Fold (limit point): the branch turns back in the parameter; two solutions meet and disappear there',
  HB: 'Hopf bifurcation: a pair of eigenvalues crosses the imaginary axis; a branch of periodic orbits starts here',
  BP: 'Branch point: another branch of solutions crosses this one',
  PD: 'Period doubling: a Floquet multiplier crosses -1; orbits of twice the period start here',
  TR: 'Torus bifurcation: a pair of Floquet multipliers crosses the unit circle; an invariant torus starts here',
  UZ: 'Marked value: a parameter, or the period T, reached one of the Mark values',
};

export function fmt(v: number): string {
  return Number.isFinite(v) ? Number(v.toPrecision(6)).toString() : 'NaN';
}

/** what a point is, in words: its kind */
export function pointKind(p: DiagramPoints, i: number): string {
  return p.f2[i] ? `${CURVES[p.f2[i]] || 'two-parameter'} curve` : TYPES[p.ty[i]] || '';
}

export function symbolName(sym: string): string {
  return SYMBOLS[sym] ?? '';
}

export function symbolHelp(sym: string): string {
  return SYMBOL_HELP[sym] ?? '';
}

/** T29: one shape per label type, so a long run's plain numbered points no
    longer read as a row of crosses. The single source of truth for both
    the diagram (diagramChart.ts's drawLabels) and the key
    (ui/AutoView.tsx): HB a filled circle, LP a triangle, BP a diamond, PD
    a square, TR a star, UZ an inverted triangle, MX a bold cross, EP a
    bar across the branch; a plain numbered point (no type) a small,
    lighter dot; an unlisted code falls back to a cross. */
export type LabelShape = 'circle' | 'triangle' | 'diamond' | 'square' | 'star' | 'invTriangle' | 'cross' | 'bar' | 'tick';

const LABEL_SHAPES: Record<string, LabelShape> = {
  HB: 'circle', LP: 'triangle', BP: 'diamond', PD: 'square', TR: 'star', UZ: 'invTriangle', MX: 'cross', EP: 'bar',
};

export function labelShape(sym: string): LabelShape {
  return sym ? (LABEL_SHAPES[sym] ?? 'cross') : 'tick';
}

/** the key's glyph for a shape (a small Unicode mark, same shape the
    canvas draws) */
export const LABEL_GLYPH: Record<LabelShape, string> = {
  circle: '●', triangle: '△', diamond: '◇', square: '□', star: '☆', invTriangle: '▽',
  cross: '×', bar: '❙', tick: '·',
};

/** the label types the diagram has, in the key's order (EP, MX, LP, HB, BP, PD, TR, UZ) */
export function labelTypes(labels: {sym: string}[]): string[] {
  const has = new Set(labels.map(l => l.sym));
  return Object.keys(SYMBOLS).filter(k => has.has(k));
}

/** the readout's parts for point `i` (A14: six significant digits); `stop`,
    the point where the last run's branch ended and why (T23), adds the reason
    to its label: "EP label 3 (End point: parameter iapp reached Par Max (0.45))" */
export function describePoint(p: DiagramPoints, labels: DiagramLabel[], axes: DiagramAxes | null, i: number,
  stop: {point: number; text: string} | null = null) {
  const l = labels.find(x => x.point === i);
  const name = l ? symbolName(l.sym) : '';
  const why = stop && stop.point === i ? stop.text : '';
  const named = name && why ? `${name}: ${why}` : name || why;
  const xName = axes?.xlabel || 'x', yName = axes?.ylabel || 'y';
  const two = p.y2[i] !== p.y[i];
  const values = [`${xName} = ${fmt(p.x[i])}`,
    two ? `${yName} max = ${fmt(p.y[i])}` : `${yName} = ${fmt(p.y[i])}`];
  if (two) values.push(`${yName} min = ${fmt(p.y2[i])}`);
  return {
    head: `Branch ${p.br[i]}, point ${p.pt[i]}`,
    kind: pointKind(p, i),
    label: l ? `${l.sym ? l.sym + ' ' : ''}label ${l.lab}${named ? ` (${named})` : ''}` : '',
    values,
  };
}
