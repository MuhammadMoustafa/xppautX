/* The array plot's colours (docs/ui-v2.md T12): a cell is coloured from the session's colour scale, which the core
   sends with each `aplot` event as its 256 colours (`ramp`, from the low end to the high one), by the core's own
   rule (core/colormap.h ramp_color): the fraction of zmin to zmax a value is picks the ramp's colour at
   floor(fraction * 256), clamped to the ends. Pure: no DOM. */
import type {CoreColor} from './colors';

/** a cell not on the grid, or whose value is off the stored rows/columns (NaN), or when zmax is not above zmin */
export const BLANK_COLOR = '#8888884d';

/** the ramp's colour at `fraction` of its length (clamped; NaN is the low end) */
export function rampColor(ramp: readonly CoreColor[], fraction: number): CoreColor {
  const f = Number.isNaN(fraction) ? 0 : Math.min(1, Math.max(0, fraction));
  return ramp[Math.min(ramp.length - 1, Math.floor(f * ramp.length))];
}

/** a cell's colour from its value and the event's zmin/zmax */
export function cellColor(ramp: readonly CoreColor[], value: number, zmin: number, zmax: number): CoreColor {
  if (!Number.isFinite(value) || !(zmax > zmin)) return BLANK_COLOR;
  return rampColor(ramp, (value - zmin) / (zmax - zmin));
}

/** `n` evenly spaced stops from zmax (index 0) to zmin, for a CSS `linear-gradient(to top, ...)` legend bar */
export function legendStops(ramp: readonly CoreColor[], n = 9): CoreColor[] {
  return Array.from({length: n}, (_, i) => rampColor(ramp, 1 - i / (n - 1)));
}
