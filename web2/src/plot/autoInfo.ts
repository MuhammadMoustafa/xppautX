/* AUTO's info strip and stability circle (the `autoinfo` event,
   docs/protocol.md) as what the AUTO view shows: rows of the strip in
   words, and the circle's points placed and classified. Pure: no DOM. */
import type {AutoInfo, AutoStab} from '../store/diagram';
import {fmt, symbolName} from './diagramModel';

const TYPES = ['', 'Stable steady state', 'Unstable steady state', 'Stable periodic orbit', 'Unstable periodic orbit'];

/** the strip's fields as [name, value] rows (A14: six significant digits) */
export function infoRows(info: AutoInfo): [string, string][] {
  const n = (v: number | null) => (v === null ? 'NaN' : fmt(v));
  const rows: [string, string][] = [
    ['Branch', String(info.br)],
    ['Point', String(info.pt)],
    ['Type', TYPES[info.type] ?? ''],
  ];
  if (info.sym || info.lab)
    rows.push(['Label', `${info.sym ? info.sym + ' ' : ''}${info.lab}${symbolName(info.sym) ? ` (${symbolName(info.sym)})` : ''}`]);
  for (const p of info.par) rows.push([p.name, n(p.value)]);
  rows.push(['Norm', n(info.norm)], [info.var, n(info.u)]);
  if (info.type === 3 || info.type === 4) rows.push(['Period', n(info.per)]);
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
