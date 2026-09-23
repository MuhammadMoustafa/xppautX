/* The sliders under the plot (GitHub #18): any number of them, each on a
   parameter or variable with its own low and high end. The model's
   `@ s1=..` presets start the list; "Add slider" appends one. Pure. */

export interface SliderDef {
  id: number;
  /** the parameter or variable ('' until one is picked) */
  name: string;
  /** the ends as typed (a draft may not be a number yet) */
  lo: string;
  hi: string;
}

/** a range around v that a drag can use: [0, 2v], [2v, 0], or [-1, 1] at 0 */
export function defaultRange(v: number): {lo: number; hi: number} {
  if (!Number.isFinite(v) || v === 0) return {lo: -1, hi: 1};
  return v > 0 ? {lo: 0, hi: 2 * v} : {lo: 2 * v, hi: 0};
}

/** the ends as numbers, or null when they do not make a range */
export function sliderRange(d: Pick<SliderDef, 'lo' | 'hi'>): {lo: number; hi: number} | null {
  const lo = Number(d.lo), hi = Number(d.hi);
  return d.lo.trim() !== '' && d.hi.trim() !== '' && Number.isFinite(lo) && Number.isFinite(hi) && lo !== hi
    ? {lo, hi} : null;
}

/** RANGE_STEPS positions along the track */
export const RANGE_STEPS = 1000;

/** the track position of value v, clamped to it */
export function toPosition(v: number, r: {lo: number; hi: number}): number {
  return Math.max(0, Math.min(RANGE_STEPS, Math.round(RANGE_STEPS * (v - r.lo) / (r.hi - r.lo))));
}

export function fromPosition(pos: number, r: {lo: number; hi: number}): number {
  return r.lo + (r.hi - r.lo) * pos / RANGE_STEPS;
}

/** the model's presets (`hello.sliders`) as the first sliders */
export function presetSliders(defs: {name: string; lo: number; hi: number}[], firstId: number): SliderDef[] {
  return defs.map((d, i) => ({id: firstId + i, name: d.name, lo: String(d.lo), hi: String(d.hi)}));
}
