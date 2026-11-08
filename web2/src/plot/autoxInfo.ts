/* autoX's info strip and stability circle (the `autoxinfo` event,
   docs/protocol.md) as what the autoX view shows: rows of the strip in
   words, and the circle's points placed and classified. Pure: no DOM. */
import type {AutoxInfo, AutoxStab, DiagramAxes} from '../store/diagram';
import {axesNames} from './axisDialog';
import {sixSig} from '../store/values';
import {pointLabel, pointMark} from '../store/pointTypes';

/** a strip row's value: words, or a number the page shows through ui/CutText.tsx (A14: six significant digits) */
export type InfoValue = string | number;

/** the strip's fields as [name, value] rows, in order of importance:
    branch, point, type, label; what the diagram's axes show (the x axis's quantity, the main
    parameter, then the y axis's: the plotted variable, Norm, the period, or the second parameter of
    a two-parameter diagram, read from `axes` rather than guessed from names); then Norm, the plotted
    variable and the period (periodic orbits) that were not already shown as an axis; then the
    remaining parameters, so a second parameter not on an axis comes last */
export function infoRows(info: AutoxInfo, axes: DiagramAxes | null): [string, InfoValue][] {
  const n = (v: number | null) => v ?? NaN;
  const rows: [string, InfoValue][] = [
    ['Branch', String(info.br)],
    ['Point', String(info.pt)],
    ['Type', `${info.stable ? 'Stable' : 'Unstable'} ${info.periodic ? 'periodic orbit' : 'steady state'}`],
  ];
  if (info.sym || info.lab)
    rows.push(['Label', `${info.sym ? pointMark(info.sym) + ' ' : ''}${info.lab}${pointLabel(info.sym) ? ` (${pointLabel(info.sym)})` : ''}`]);

  /* the axes' parameters by name (axesNames: the x axis's, and the y axis's in a two-parameter
     diagram), the first parameter standing in for the x axis's until the axes are known */
  const {par1, yvar, par2} = axesNames(axes);
  const xPar = info.par.find(p => p.name === par1) ?? info.par[0];
  const yPar = par2 ? info.par.find(p => p.name === par2) : undefined;
  const parRow = (p: {name: string; value: number | null}): [string, InfoValue] => [p.name, n(p.value)];
  const varRow: [string, InfoValue] = [info.var, n(info.u)];
  const normRow: [string, InfoValue] = ['Norm', n(info.norm)];
  const periodRow: [string, InfoValue] | null = info.periodic ? ['Period', n(info.per)] : null;

  /* the y axis's quantity: the plotted variable, the second parameter, Norm or the period */
  const plot = axes?.plot;
  const yRow = yvar ? varRow : yPar ? parRow(yPar) : plot === 'norm' ? normRow : plot === 'period' ? periodRow : null;

  if (xPar) rows.push(parRow(xPar));
  if (yRow) rows.push(yRow);
  for (const r of [normRow, varRow, periodRow]) if (r && r !== yRow) rows.push(r);
  for (const p of info.par) if (p !== xPar && p !== yPar) rows.push(parRow(p));
  rows.push(['Jacobian', info.jac]);
  return rows;
}

/** a complex number in words: `a`, `a + bi`, `a − bi` */
export function complexText(re: number | null, im: number | null): string {
  if (re === null || im === null) return 'none (below the smallest number)';
  if (im === 0) return sixSig(re);
  return `${sixSig(re)} ${im < 0 ? '−' : '+'} ${sixSig(Math.abs(im))}i`;
}

export interface CirclePoint {
  /** where XPP draws it: the value, each part clamped to ±1.95 as its circle does */
  x: number;
  y: number;
  /** inside (or on) the unit circle: a stable direction */
  inside: boolean;
  /** a cycle's trivial multiplier (the one nearest 1): counted as neither inside nor outside */
  trivial: boolean;
  text: string;
}

const EDGE = 1.95;
const clamp = (v: number) => Math.max(-EDGE, Math.min(EDGE, v));

/** whether autoX computed the circle's values for the point: it computes a
    steady state's eigenvalues and an orbit's multipliers from a run's
    second point on, and the first point's circle comes as all zeros (T25,
    W15: unless the run restarts from a label of the same kind, whose values
    it carries), which is no e^λ or multiplier at all */
export function stabComputed(stab: AutoxStab): boolean {
  return stab.circle.some(([re, im]) => re !== 0 || im !== 0);
}

/** the circle's points: multipliers, or e^λ of a steady state's eigenvalues; none when not computed.
    A cycle's trivial multiplier is the one nearest 1 (exactly 1 in theory, a little off when computed). */
export function circlePoints(stab: AutoxStab): CirclePoint[] {
  if (!stabComputed(stab)) return [];
  const distToOne = ([re, im]: [number | null, number | null]) => Math.hypot((re ?? 0) - 1, im ?? 0);
  let trivialAt = -1;
  if (stab.periodic) stab.circle.forEach((c, i) => {
    if (trivialAt < 0 || distToOne(c) < distToOne(stab.circle[trivialAt])) trivialAt = i;
  });
  return stab.circle.map(([re, im], i) => {
    const r = re ?? 0, m = im ?? 0;
    const e = stab.eig?.[i];
    return {
      x: clamp(r), y: clamp(m), inside: Math.hypot(r, m) <= 1, trivial: i === trivialAt,
      text: stab.periodic ? complexText(re, im) : complexText(e ? e[0] : null, e ? e[1] : null),
    };
  });
}

/** the circle in a sentence, for its label and the strip's summary line (A7: not only its colours and
    shapes): how many lie inside and outside the circle, and a cycle's trivial multiplier apart */
export function stabilitySummary(stab: AutoxStab): string {
  const pts = circlePoints(stab);
  if (!stabComputed(stab)) return `${stab.circle.length} ${stab.periodic ? 'Floquet multipliers' : 'eigenvalues'}, not computed at this point`;
  const counted = pts.filter(p => !p.trivial);
  const inside = counted.filter(p => p.inside).length;
  const outside = counted.length - inside;
  if (!stab.periodic) return `${pts.length} eigenvalues: ${inside} inside, ${outside} outside`;
  return `${pts.length} Floquet multipliers: ${inside} inside, ${outside} outside, 1 trivial`;
}
