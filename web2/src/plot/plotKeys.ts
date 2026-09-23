/* The plot from the keyboard (when the plot has the focus). Pure: a key and
   what the plot shows in, what to do out.

   Arrow keys   pan by a tenth          + or =   zoom in    -   zoom out
   0            back to the core's view  Ctrl+Z (Cmd+Z)  undo the last zoom or pan
   ] and [      next / previous point    PageDown, PageUp  ten points on / back
   Home, End    first / last point       } and {  next / previous curve
   Escape       clear the point readout

   While the plot has the focus these keys are its own; every other key
   is left to the XPP hotkeys. Escape goes on to XPP when there is no readout. */
import {panBy, zoomAbout, type Ranges} from './viewmath';

export const PLOT_KEYS_HELP =
  'Arrow keys pan, plus and minus zoom, 0 resets, Control Z undoes a zoom, square brackets and Page Up or Down '
  + 'step through the points, Home and End go to the ends, braces change the curve, Escape clears the readout.';

export interface KeyContext {
  ranges: Ranges;
  hover: {curve: number; index: number} | null;
  /** points in each curve, 0 for a hidden one */
  counts: number[];
}

export type KeyResult =
  | {view: Ranges}
  | {reset: true}
  | {undo: true}
  | {hover: {curve: number; index: number} | null}
  | null;

const PAN = 0.1, ZOOM = 0.8;

function nextCurve(counts: number[], from: number, dir: 1 | -1): number {
  for (let k = 1; k <= counts.length; k++) {
    const c = (from + dir * k + counts.length * 2) % counts.length;
    if (counts[c] > 0) return c;
  }
  return -1;
}

export function plotKey(key: string, ctx: KeyContext): KeyResult {
  const r = ctx.ranges;
  switch (key) {
    case 'ArrowLeft': return {view: panBy(r, PAN, 0)};
    case 'ArrowRight': return {view: panBy(r, -PAN, 0)};
    case 'ArrowUp': return {view: panBy(r, 0, PAN)};
    case 'ArrowDown': return {view: panBy(r, 0, -PAN)};
    case '+': case '=': return {view: zoomAbout(r, 0.5, 0.5, ZOOM)};
    case '-': case '_': return {view: zoomAbout(r, 0.5, 0.5, 1 / ZOOM)};
    case '0': return {reset: true};
    case 'Undo': return {undo: true}; /* Ctrl+Z or Cmd+Z, named so by the caller */
    case 'Escape': return ctx.hover ? {hover: null} : null;
  }
  const first = ctx.hover ? null : nextCurve(ctx.counts, -1, 1);
  const curve = ctx.hover ? ctx.hover.curve : first!;
  if (curve < 0 || !(ctx.counts[curve] > 0)) return null;
  const last = ctx.counts[curve] - 1, at = ctx.hover?.index ?? -1;
  const clamp = (i: number) => Math.max(0, Math.min(last, i));
  switch (key) {
    case ']': return {hover: {curve, index: clamp(at + 1)}};
    case '[': return {hover: {curve, index: clamp(at < 0 ? 0 : at - 1)}};
    case 'PageDown': return {hover: {curve, index: clamp(at + 10)}};
    case 'PageUp': return {hover: {curve, index: clamp(at < 0 ? 0 : at - 10)}};
    case '}': case '{': {
      const c = ctx.hover ? nextCurve(ctx.counts, curve, key === '}' ? 1 : -1) : curve;
      return c < 0 ? null : {hover: {curve: c, index: Math.min(Math.max(at, 0), ctx.counts[c] - 1)}};
    }
    case 'Home': return {hover: {curve, index: 0}};
    case 'End': return {hover: {curve, index: last}};
  }
  return null;
}
