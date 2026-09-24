/* The sliders under the plot (GitHub #18, T20): any number of them, each on
   a parameter or variable with its own low end, high end and step. The
   model's `@ s1=..` presets start the list; the Add slider dialog
   (SliderDialog.tsx) appends or edits one, picking the parameter or
   variable from a searchable list and setting Min, Max and Step. Pure. */

export interface SliderDef {
  id: number;
  /** the parameter or variable ('' until one is picked) */
  name: string;
  /** the ends and step as typed (a draft may not be a number yet) */
  lo: string;
  hi: string;
  step: string;
}

/** a range around v that a drag can use: [0, 2v], [2v, 0], or [-1, 1] at 0 */
export function defaultRange(v: number): {lo: number; hi: number} {
  if (!Number.isFinite(v) || v === 0) return {lo: -1, hi: 1};
  return v > 0 ? {lo: 0, hi: 2 * v} : {lo: 2 * v, hi: 0};
}

/** raw rounded up to 1, 2 or 5 times a power of ten (a "nice" step) */
export function niceStep(raw: number): number {
  if (!Number.isFinite(raw) || raw <= 0) return 1;
  const exp = Math.floor(Math.log10(raw));
  const base = raw / 10 ** exp;
  const nice = base <= 1 ? 1 : base <= 2 ? 2 : base <= 5 ? 5 : 10;
  return nice * 10 ** exp;
}

/** the default step for a range: (max - min) / 100, rounded to a nice number */
export function defaultStep(lo: number, hi: number): number {
  const span = Math.abs(hi - lo);
  return span > 0 && Number.isFinite(span) ? niceStep(span / 100) : 1;
}

/** the step as a number, or null when it does not make one (blank, not
    finite, not positive) */
export function sliderStep(d: Pick<SliderDef, 'step'>): number | null {
  const s = Number(d.step);
  return d.step.trim() !== '' && Number.isFinite(s) && s > 0 ? s : null;
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

/** the model's presets (`hello.sliders`) as the first sliders, with a
    computed step since the core does not send one */
export function presetSliders(defs: {name: string; lo: number; hi: number}[], firstId: number): SliderDef[] {
  return defs.map((d, i) => ({id: firstId + i, name: d.name, lo: String(d.lo), hi: String(d.hi),
    step: String(defaultStep(d.lo, d.hi))}));
}

/** the dialog's errors, keyed by field; empty when the range and step are valid */
export interface SliderErrors {
  lo?: string;
  hi?: string;
  step?: string;
}

/** Min < Max, Step > 0 and Step <= Max - Min (SliderDialog.tsx) */
export function validateSliderFields(lo: string, hi: string, step: string): SliderErrors {
  const errors: SliderErrors = {};
  const loN = Number(lo), hiN = Number(hi), stepN = Number(step);
  const loOk = lo.trim() !== '' && Number.isFinite(loN);
  const hiOk = hi.trim() !== '' && Number.isFinite(hiN);
  if (!loOk) errors.lo = 'A number';
  if (!hiOk) errors.hi = 'A number';
  if (loOk && hiOk && !(loN < hiN)) {
    errors.lo = errors.lo ?? 'Min must be less than Max';
    errors.hi = errors.hi ?? 'Min must be less than Max';
  }
  if (step.trim() === '' || !Number.isFinite(stepN)) errors.step = 'A number';
  else if (stepN <= 0) errors.step = 'Step must be greater than 0';
  else if (loOk && hiOk && loN < hiN && stepN > hiN - loN) errors.step = 'Step must not be more than Max − Min';
  return errors;
}

/** a parameter or variable the Add/Edit slider dialog can pick, with its
    current value (SliderDialog.tsx) */
export interface SliderCandidate {
  kind: 'par' | 'ic';
  name: string;
  value: number;
}

/** the candidates whose name contains the query, case-insensitively; blank matches all */
export function filterCandidates(candidates: SliderCandidate[], query: string): SliderCandidate[] {
  const q = query.trim().toLowerCase();
  return q === '' ? candidates : candidates.filter(c => c.name.toLowerCase().includes(q));
}
