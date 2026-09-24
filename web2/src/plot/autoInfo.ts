/* AUTO's info strip and stability circle (the `autoinfo` event,
   docs/protocol.md) as what the AUTO view shows: rows of the strip in
   words, and the circle's points placed and classified. Pure: no DOM. */
import type {AutoInfo, AutoStab, DiagramAxes} from '../store/diagram';
import {yNeeds} from './axisDialog';
import {fmt, symbolName} from './diagramModel';

const TYPES = ['', 'Stable steady state', 'Unstable steady state', 'Stable periodic orbit', 'Unstable periodic orbit'];

/** the strip's fields as [name, value] rows (A14: six significant digits), in order of importance:
    branch, point, type, label; what the diagram's axes show (the x axis's quantity, the main
    parameter, then the y axis's: the plotted variable, Norm, the period, or the second parameter of
    a two-parameter diagram, read from `axes` rather than guessed from names); then Norm, the plotted
    variable and the period (periodic orbits) that were not already shown as an axis; then the
    remaining parameters, so a second parameter not on an axis comes last */
export function infoRows(info: AutoInfo, axes: DiagramAxes | null): [string, string][] {
  const n = (v: number | null) => (v === null ? 'NaN' : fmt(v));
  const rows: [string, string][] = [
    ['Branch', String(info.br)],
    ['Point', String(info.pt)],
    ['Type', TYPES[info.type] ?? ''],
  ];
  if (info.sym || info.lab)
    rows.push(['Label', `${info.sym ? info.sym + ' ' : ''}${info.lab}${symbolName(info.sym) ? ` (${symbolName(info.sym)})` : ''}`]);

  const plot = axes?.plot;
  /* what the y axis needs besides the plot type: the variable, the second parameter, or neither
     (Norm, Period and Frequency plot the point's own quantities, not a second parameter) */
  const need = yNeeds(plot ?? -1);
  const [par1, par2] = info.par;
  const varRow: [string, string] = [info.var, n(info.u)];
  const normRow: [string, string] = ['Norm', n(info.norm)];
  const periodRow: [string, string] | null = info.type === 3 || info.type === 4 ? ['Period', n(info.per)] : null;

  if (par1) rows.push([par1.name, n(par1.value)]);
  let varShown = false, normShown = false, periodShown = false;
  if (need === 'var') {
    rows.push(varRow);
    varShown = true;
  } else if (need === 'par2') {
    if (par2) rows.push([par2.name, n(par2.value)]);
  } else if (plot === 1) {
    rows.push(normRow);
    normShown = true;
  } else if (plot === 3 && periodRow) {
    rows.push(periodRow);
    periodShown = true;
  }

  if (!normShown) rows.push(normRow);
  if (!varShown) rows.push(varRow);
  if (!periodShown && periodRow) rows.push(periodRow);

  for (const p of info.par) {
    if (p === par1) continue;
    if (need === 'par2' && p === par2) continue;
    rows.push([p.name, n(p.value)]);
  }
  return rows;
}

/** a complex number in words: `a`, `a + bi`, `a − bi` */
export function complexText(re: number | null, im: number | null): string {
  if (re === null || im === null) return 'none (below the smallest number)';
  if (im === 0) return fmt(re);
  return `${fmt(re)} ${im < 0 ? '−' : '+'} ${fmt(Math.abs(im))}i`;
}

export interface CirclePoint {
  /** where XPP draws it: the value, each part clamped to ±1.95 as its circle does */
  x: number;
  y: number;
  /** inside (or on) the unit circle: a stable direction */
  inside: boolean;
  text: string;
}

const EDGE = 1.95;
const clamp = (v: number) => Math.max(-EDGE, Math.min(EDGE, v));

/** whether AUTO computed the circle's values for the point: it computes a
    steady state's eigenvalues and an orbit's multipliers from a branch's
    second point on, and the first point's circle comes as all zeros (a run
    that stops at point 1, T25), which is no e^λ or multiplier at all */
export function stabComputed(stab: AutoStab): boolean {
  return stab.circle.some(([re, im]) => re !== 0 || im !== 0);
}

/** the circle's points: multipliers, or e^λ of a steady state's eigenvalues; none when not computed */
export function circlePoints(stab: AutoStab): CirclePoint[] {
  if (!stabComputed(stab)) return [];
  return stab.circle.map(([re, im], i) => {
    const r = re ?? 0, m = im ?? 0;
    const e = stab.eig?.[i];
    return {
      x: clamp(r), y: clamp(m), inside: Math.hypot(r, m) <= 1,
      text: stab.periodic ? complexText(re, im) : complexText(e ? e[0] : null, e ? e[1] : null),
    };
  });
}

/** the circle in a sentence, for its label (A7: not only its colours and shapes) */
export function stabilitySummary(stab: AutoStab): string {
  const pts = circlePoints(stab);
  const inside = pts.filter(p => p.inside).length;
  const what = stab.periodic ? 'Floquet multipliers' : 'eigenvalues';
  if (!stabComputed(stab)) return `${stab.circle.length} ${what}, not computed at this point`;
  const where = stab.periodic ? 'inside the unit circle' : 'with a negative real part (inside the circle)';
  return `${pts.length} ${what}, ${inside} ${where}`;
}
