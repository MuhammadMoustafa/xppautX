/* From the store's diagram to what the autoX view draws (docs/ui-v2.md T11a).
   Pure: no DOM.

   XPP's convention, kept: a steady state is a line (stable solid and thick,
   unstable thin), a periodic branch is marked at its maximum and minimum
   (the hi and lo values), a two-parameter curve is a line in its kind's
   color. Here every kind is a line (unstable ones dashed), the periodic
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
import {hasLivePoints, type DiagramAxes, type DiagramLabel, type DiagramPoints} from '../store/diagram';
import type {Range} from '../store/state';
import type {CoreColor} from '../protocol/types';
import type {Ranges} from './viewmath';
import {pointLabel} from '../store/pointTypes';
import {jobKeyText} from './autoxStatus';

export type CurveKind = 'steady' | 'periodic' | 'two-parameter';

export interface DiagramCurve {
  branch: number;
  /** the store's ty: 1 stable steady state, 2 unstable steady, 3 stable periodic, 4 unstable periodic (the core's) */
  type: number;
  f2: number;
  kind: CurveKind;
  stable: boolean;
  /** the values it draws: y (the maximum for hi and lo) or y2 (the minimum) */
  which: 'y' | 'y2';
  /** the color of the run as the core says it */
  color: CoreColor;
  cssColor?: string;
  visible?: boolean;
  legend?: string;
  width: number;
  dashed: boolean;
  xs: Float64Array;
  ys: Float64Array;
  /** the point each vertex is (the Hopf point first, for a branch joined to it) */
  idx: Int32Array;
  /** the Hopf label's point the curve starts from, or -1 */
  hopf: number;
  /** the running branch job (`live`, W620) whose points these are, 0 for the diagram's own */
  live: number;
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
  return !p.nw[b] && p.lv[a] === p.lv[b] && p.br[a] === p.br[b] && p.ty[a] === p.ty[b] && p.d[a] === p.d[b] && p.c[a] === p.c[b]
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
    return l && l.sym === 'hopf' && !p.f2[at] && !PERIODIC(p.d[at]) ? at : -1;
  }
  const x = p.x[s], lo = Math.min(p.y[s], p.y2[s]), hi = Math.max(p.y[s], p.y2[s]);
  const tx = 0.05 * span.x, ty = 0.02 * span.y;
  let best = -1, bestScore = Infinity;
  for (const l of labels) {
    const h = l.point;
    if (l.sym !== 'hopf' || p.f2[h] || PERIODIC(p.d[h]) || p.br[h] === p.br[s]) continue;
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

function extent(values: ArrayLike<number>): number {
  let min = Infinity, max = -Infinity;
  for (let i = 0; i < values.length; i++) {
    const v = values[i];
    if (v < min) min = v;
    if (v > max) max = v;
  }
  return max > min ? max - min : 1;
}

function curve(p: DiagramPoints, s: number, verts: number[], which: 'y' | 'y2', hopf: number): DiagramCurve {
  const kind = kindOf(p, s), ty = p.ty[s];
  const stable = kind === 'two-parameter' || p.st[s] === 1;
  const col = which === 'y' ? p.y : p.y2;
  const xs = new Float64Array(verts.length), ys = new Float64Array(verts.length), idx = new Int32Array(verts.length);
  verts.forEach((v, k) => {
    xs[k] = p.x[v];
    /* the Hopf point is one value: both lines of the branch start there */
    ys[k] = v === hopf ? p.y[v] : col[v];
    idx[k] = v;
  });
  const width = kind === 'steady' ? (p.lw[s] >= 2 ? 2.25 : 1.25) : stable ? 1.75 : 1.25;
  return {branch: p.br[s], type: ty, f2: p.f2[s], kind, stable, which, color: p.palette[p.c[s]], width, dashed: !stable,
    xs, ys, idx, hopf, live: p.lv[s]};
}

/** the points in drawing order while jobs run (W620): the diagram's own, then each running job's in the order they arrived (a
    stable sort by job), so a job's polyline never runs into another's whatever order the points came in; null when no point is a job's */
function jobOrder(p: DiagramPoints): Int32Array | null {
  if (!hasLivePoints(p)) return null;
  return Int32Array.from(p.lv.keys()).sort((a, b) => p.lv[a] - p.lv[b] || a - b);
}

export function buildDiagramModel(p: DiagramPoints, labels: DiagramLabel[], axes: DiagramAxes | null,
  hideBefore = 0): DiagramModel {
  const n = p.x.length;
  /* Clear (T21): the points before `hideBefore` are earlier branches, left out */
  const first = Math.max(0, Math.min(hideBefore, n));
  const span = axes && axes.xmax > axes.xmin && axes.ymax > axes.ymin
    ? {x: axes.xmax - axes.xmin, y: axes.ymax - axes.ymin} : {x: extent(p.x), y: extent(p.y)};
  const curves: DiagramCurve[] = [], hopf: DiagramModel['hopf'] = [];
  const order = jobOrder(p);
  /* the point at position k: while jobs run, each job's points in turn */
  const at = (k: number): number => order ? order[k] : k;
  for (let s = first; s < n;) {
    let e = s;
    while (e + 1 < n && follows(p, at(e), at(e + 1))) e++;
    const sp = at(s);
    if (p.d[sp] !== 0) {
      const verts: number[] = [];
      let from = -1;
      const prev = s - 1, pp = at(prev);
      if (PERIODIC(p.d[sp])) {
        /* the previous periodic point of the branch (a change of stability) */
        if (prev >= first && !p.nw[sp] && p.lv[pp] === p.lv[sp] && p.br[pp] === p.br[sp] && PERIODIC(p.d[pp]) && !p.f2[pp]) verts.push(pp);
        else if (!p.f2[sp]) {
          from = hopfOf(p, labels, sp, span);
          if (from >= 0) {
            verts.push(from);
            hopf.push({point: sp, from});
          }
        }
      } else if (prev >= first && !p.nw[sp] && p.lv[pp] === p.lv[sp]) {
        verts.push(pp); /* the line back from the run's first point */
      }
      for (let i = s; i <= e; i++) verts.push(at(i));
      curves.push(curve(p, sp, verts, 'y', from));
      let two = false;
      if (PERIODIC(p.d[sp])) for (let i = s; i <= e && !two; i++) two = p.y2[at(i)] !== p.y[at(i)];
      if (two) curves.push(curve(p, sp, verts, 'y2', from));
    }
    s = e + 1;
  }
  const marks = labels.filter(l => l.point >= first).map(l => ({...l, x: p.x[l.point], y: p.y[l.point],
    y2: p.y2[l.point] !== p.y[l.point] ? p.y2[l.point] : null}));
  return {curves, labels: marks, hopf, xLabel: axes?.xlabel ?? '', yLabel: axes?.ylabel ?? ''};
}

/* ---- the legend (W630): one entry per branch, stability and kind ---- */

export interface LegendEntry {
  /** what its name, color and visibility are keyed by: `job:0.5:unstable:periodic` for a running job's curves, `branch:3:stable:steady` otherwise */
  key: string;
  /** what it is called: "Job 0.5 · unstable periodic", "Branch 3 · stable steady" */
  name: string;
  /** the model's curves it stands for: the segments a stability change splits, a periodic branch's maximum and minimum */
  curves: number[];
  /** the color of its first curve, as the run says it */
  color: CoreColor;
}

/** the legend's entries of `curves`, and for each curve the index of its entry. A running job's curves are named by its key
    (`jobKeys`, the key of each running job by its live id, which the store keeps from `autox.jobs`); a curve of the diagram's
    own is named by its branch. One entry per job or branch, stability and kind. Pure. */
export function legendEntries(curves: DiagramCurve[], jobKeys: Record<number, number[]>): {entries: LegendEntry[]; of: number[]} {
  const entries: LegendEntry[] = [], index = new Map<string, number>(), of: number[] = [];
  curves.forEach((c, i) => {
    const stability = c.stable ? 'stable' : 'unstable';
    let who: string, owner: string;
    if (c.live) {
      /* a job is listed before its first point is sent (autox_jobs.cpp announces a job's start before it computes), but a page
         that connected while the jobs ran missed the events that listed them, and autox.jobs lists only the running ones */
      const key: number[] | undefined = jobKeys[c.live];
      who = key ? `job:${jobKeyText(key)}` : `live:${c.live}`;
      owner = key ? `Job ${jobKeyText(key)}` : 'Running job';
    } else {
      who = `branch:${c.branch}`;
      owner = `Branch ${c.branch}`;
    }
    const key = `${who}:${stability}:${c.kind}`;
    let e = index.get(key);
    if (e === undefined) {
      e = entries.length;
      index.set(key, e);
      entries.push({key, name: `${owner} · ${stability} ${c.kind}`, curves: [], color: c.color});
    }
    entries[e].curves.push(i);
    of.push(e);
  });
  return {entries, of};
}

/** the legend's settings (name, color, visibility; keyed by entry, see LegendEntry.key) after a join (W631): each job that
    kept its points moves its entries from `job:<key>:...` to `branch:<its branch>:...`, replacing what that key held; a job the
    join dropped (a repeat) and a `live:` entry are gone with their curves. Pure. */
export function carryAppearance<T>(appearance: Record<string, T>, joined: {key: number[]; branch: number}[]): Record<string, T> {
  const branchOf = new Map(joined.map(j => [jobKeyText(j.key), j.branch]));
  const kept: Record<string, T> = {}, moved: Record<string, T> = {};
  for (const [key, value] of Object.entries(appearance)) {
    const job = /^job:([^:]+)(:.*)$/.exec(key);
    if (!job) {
      if (!key.startsWith('live:')) kept[key] = value;
      continue;
    }
    const branch = branchOf.get(job[1]);
    if (branch !== undefined) moved[`branch:${branch}${job[2]}`] = value;
  }
  return {...kept, ...moved};
}

/* ---- Axes/Fit (T30, docs/ui-v2.md): the diagram's own fit, client-side ---- */

/** one axis' extent across `m`'s curves, with a small margin (a dot when
    they are a single point, [0, 1] when there are none yet) */
function curveExtent(m: DiagramModel, axis: 'x' | 'y'): Range {
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

/** Axes/Fit's own ranges: the visible curves' extent with a small margin,
    client-side (the core has no notion of the diagram's fit); `m`'s curves
    already leave out whatever `buildDiagramModel`'s `hideBefore` hid, so a
    hidden "earlier branches" key never widens this. DiagramChart.fit() (the
    corner button and the autoX tools' Fit) is the caller; this half is pure
    for the unit test (test/diagram.test.ts). */
export function fitRanges(m: DiagramModel): Ranges {
  return {x: curveExtent(m, 'x'), y: curveExtent(m, 'y')};
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
    if (c.visible === false) return;
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

const CURVES = ['', 'limit point', 'limit point of periodic orbits', 'Hopf', 'torus', 'branch point', 'period doubling',
  'fixed period', '', '', '', '', '', 'neutral saddle'];
/** what each label type means, for the key's tooltips (docs/manual/09-autox.md) */
const SYMBOL_HELP: Record<string, string> = {
  end: 'End point: where a branch starts or ends normally (a limit, Max points or Stop); the status strip says why it ended',
  no_convergence: 'No convergence: autoX could not compute the next point, even at the smallest step (min_step_size), and ended the branch; '
    + 'a smaller step_size or max_step_size, a larger mesh_intervals or looser tolerances may get past it',
  fold: 'Fold (limit point): the branch turns back in the parameter; two solutions meet and disappear there',
  hopf: 'Hopf bifurcation: a pair of eigenvalues crosses the imaginary axis; a branch of periodic orbits starts here',
  branch_point: 'Branch point: another branch of solutions crosses this one',
  period_doubling: 'Period doubling: a Floquet multiplier crosses -1; orbits of twice the period start here',
  neimark_sacker: 'Torus bifurcation: a pair of Floquet multipliers crosses the unit circle; an invariant torus starts here',
  user_point: 'Marked value: a parameter, or the period T, reached one of the Mark values',
  bogdanov_takens: 'Bogdanov-Takens: two zero eigenvalues on a fold or Hopf curve',
  cusp: 'Cusp: the quadratic coefficient of the fold vanishes',
  zero_hopf: 'Zero-Hopf: a zero eigenvalue and an imaginary pair meet',
  generalized_hopf: 'Generalised Hopf (Bautin): the first Lyapunov coefficient vanishes',
  cycle_fold: 'Fold of cycles: a fold of a periodic orbit, where the branch of orbits turns back in the parameter',
  cycle_branch_point: 'Branch point of cycles: another branch of periodic orbits crosses this one',
  fold_flip: 'Fold-flip: a map has multipliers +1 and -1',
  fold_neimark_sacker: 'Fold-torus: a map has a multiplier +1 and a pair on the unit circle',
  flip_neimark_sacker: 'Flip-torus: a map has a multiplier -1 and a pair on the unit circle',
  double_neimark_sacker: 'Torus-torus: a map has two pairs on the unit circle',
  resonance_1_1: '1:1 resonance: two map or cycle multipliers meet at +1',
  resonance_1_2: '1:2 resonance: two map or cycle multipliers meet at -1',
  resonance_1_3: '1:3 resonance: the Floquet angle is 2 pi / 3',
  resonance_1_4: '1:4 resonance: the Floquet angle is pi / 2',
  snic: 'SNIC (saddle-node on invariant circle): the period of the cycles grows without bound as their orbit lingers at the ghost of an equilibrium with a zero eigenvalue; the run stopped here',
  homoclinic: 'Homoclinic: the period of the cycles grows without bound as their orbit lingers at a saddle and returns to it; the run stopped here',
  heteroclinic: 'Heteroclinic: the period of the cycles grows without bound as their orbit passes two or more saddles in turn; the run stopped here',
  hopf_end: 'End at a Hopf point: the amplitude of the cycles fell back to zero at an equilibrium with a purely imaginary pair of eigenvalues; the run stopped here',
  branch_closed: 'Branch closed: the branch came back to its own start; the run stopped here',
};


/** what a point is, in words: its kind */
export function pointKind(p: DiagramPoints, i: number): string {
  return p.f2[i] ? `${CURVES[p.f2[i]] || 'two-parameter'} curve`
    : `${p.st[i] ? 'stable' : 'unstable'} ${p.pe[i] ? 'periodic orbit' : 'steady state'}`;
}

export function symbolHelp(sym: string): string {
  return SYMBOL_HELP[sym] ?? '';
}

/** T29: one shape per label type, so a long run's plain numbered points no
    longer read as a row of crosses. The single source of truth for both
    the diagram (diagramChart.ts's drawLabels) and the key
    (ui/AutoxView.tsx): HB a filled circle, LP a triangle, BP a diamond, PD
    a square, TR a star, UZ an inverted triangle, MX a bold cross, EP a
    bar across the branch; a plain numbered point (no type) a small,
    lighter dot; an unlisted code falls back to a cross. */
export type LabelShape = 'circle' | 'triangle' | 'diamond' | 'square' | 'star' | 'invTriangle' | 'cross' | 'bar' | 'tick';

const LABEL_SHAPES: Record<string, LabelShape> = {
  hopf: 'circle', fold: 'triangle', branch_point: 'diamond', period_doubling: 'square', neimark_sacker: 'star', user_point: 'invTriangle', no_convergence: 'cross', end: 'bar',
  bogdanov_takens: 'diamond', cusp: 'triangle', zero_hopf: 'star', generalized_hopf: 'circle',
  fold_flip: 'square', fold_neimark_sacker: 'triangle', flip_neimark_sacker: 'star', double_neimark_sacker: 'diamond',
  cycle_fold: 'triangle', cycle_branch_point: 'diamond',
  resonance_1_1: 'diamond', resonance_1_2: 'triangle', resonance_1_3: 'star', resonance_1_4: 'circle',
  snic: 'bar', homoclinic: 'bar', heteroclinic: 'bar', hopf_end: 'bar', branch_closed: 'bar',
};

/** the plain points (no special type, and an output point) are the small dots */
const PLAIN_TYPES = new Set(['', 'regular', 'output']);

export function labelShape(sym: string): LabelShape {
  return PLAIN_TYPES.has(sym) ? 'tick' : (LABEL_SHAPES[sym] ?? 'cross');
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
  return Object.keys(SYMBOL_HELP).filter(k => has.has(k));
}

/** the readout's parts for point `i` (its `values` are numbers: ui/AutoxView.tsx shows them through ui/CutText.tsx); `stop`,
    the point where the last run's branch ended and why (T23), adds the reason
    to its label: "EP label 3 (End point: parameter iapp reached parameter_max (0.45))" */
export function describePoint(p: DiagramPoints, labels: DiagramLabel[], axes: DiagramAxes | null, i: number,
  stop: {point: number; text: string} | null = null) {
  const l = labels.find(x => x.point === i);
  const why = stop && stop.point === i ? stop.text : '';
  const xName = axes?.xlabel || 'x', yName = axes?.ylabel || 'y';
  const two = p.y2[i] !== p.y[i];
  const values = [{name: xName, value: p.x[i]}, {name: two ? `${yName} max` : yName, value: p.y[i]}];
  if (two) values.push({name: `${yName} min`, value: p.y2[i]});
  return {
    head: `Branch ${p.br[i]}, point ${p.pt[i]}`,
    kind: pointKind(p, i),
    label: l ? [pointLabel(l.sym), l.lab ? `label ${l.lab}` : '', why ? `(${why})` : ''].filter(Boolean).join(' ') : '',
    values,
  };
}
