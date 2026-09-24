/* The AUTO diagram's axis dialog (docs/ui-v2.md T21), its pure part: the
   plot types the y axis can show, which name each needs (a variable, the
   second parameter or none), the names the core's axes show now, the
   spinners' step and the range typed. Pure. */
import type {DiagramAxes} from '../store/diagram';

/** Auto.plot values in the order of AUTO's Plot Type menu, with what they plot */
export const PLOT_TYPES: {plot: number; text: string}[] = [
  {plot: 0, text: 'Maximum (Hi)'},
  {plot: 2, text: 'Maximum and minimum (hI-lo)'},
  {plot: 1, text: 'Norm'},
  {plot: 11, text: 'Average'},
  {plot: 3, text: 'Period'},
  {plot: 10, text: 'Frequency'},
  {plot: 4, text: 'Two parameters'},
];

/** the name the y axis needs besides the plot type: a variable, the second parameter, or none */
export function yNeeds(plot: number): 'var' | 'par2' | null {
  return plot === 0 || plot === 2 || plot === 11 ? 'var' : plot === 4 ? 'par2' : null;
}

/** what the core's axes show now: the main parameter, and the y axis's variable or second parameter */
export function axesNames(axes: DiagramAxes | null): {par1: string; yvar: string; par2: string} {
  if (!axes) return {par1: '', yvar: '', par2: ''};
  const need = yNeeds(axes.plot);
  const y = axes.plot === 11 ? axes.ylabel.replace(/_bar$/, '') : axes.ylabel;
  return {par1: axes.xlabel, yvar: need === 'var' ? y : '', par2: need === 'par2' ? y : ''};
}

/** a spinner's step for a range of this width: a tenth of its order of magnitude */
export function spinStep(width: number): number {
  if (!(width > 0) || !Number.isFinite(width)) return 1;
  return 10 ** (Math.floor(Math.log10(width)) - 1);
}

/** the range typed, or null while it is not one (not numbers, min not below max) */
export function typedRange(minText: string, maxText: string): {min: number; max: number} | null {
  if (!minText.trim() || !maxText.trim()) return null;
  const min = Number(minText), max = Number(maxText);
  return Number.isFinite(min) && Number.isFinite(max) && min < max ? {min, max} : null;
}

/** a bound as a field shows it: short, but read back as the same number */
export function boundText(v: number): string {
  for (let p = 6; p <= 17; p++) {
    const t = String(Number(v.toPrecision(p)));
    if (Number(t) === v) return t;
  }
  return String(v);
}
