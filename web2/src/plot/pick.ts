/* The core's mouse, rubber and drag asks as plot modes (docs/ui-v2.md
   section 3): a crosshair that picks a point, a box (or a line) drawn from
   one corner to the other, or a drag of the plot. Pure: positions are
   fractions of the plotting area (from its top-left, as the screen counts),
   and answers are data coordinates (`xd`, `yd`, `xd2`, `yd2`,
   docs/protocol.md "Data coordinates"), so no pixel geometry of the core's
   window is needed.

   Keyboard (A3), with the plot focused:
   Arrow keys   move the crosshair (or the box's free corner) by a fiftieth,
                with Shift by a tenth; in a drag, scroll the plot by that
   Enter        pick the point; for a box or line, fix the first corner,
                then the second
   Escape       cancel (a drag: end) */
import type {AskEvent, View} from '../protocol/types';
import type {Ranges} from './viewmath';

export type PickMode = 'point' | 'box' | 'line' | 'drag';

/** a position in the plotting area: 0..1 from the left and from the top */
export interface Frac {
  fx: number;
  fy: number;
}

export interface PickState {
  mode: PickMode;
  win: number;
  /** the ask this is for */
  ask: number;
  /** the crosshair, or the box's free corner */
  cursor: Frac;
  /** a box's or a line's first corner, once fixed */
  anchor: Frac | null;
  /** answered; the command goes on (a drag asks again) */
  waiting: boolean;
}

export const CENTRE: Frac = {fx: 0.5, fy: 0.5};
export const STEP = 0.02;
export const BIG_STEP = 0.1;

/** the plot mode an ask is, when this page can show it: a 2D plot window
    (1..10) it draws, the active one (`state.view`), or the AUTO diagram
    (window 101) while the AUTO view is open (`auto`); null for anything else
    (a 3D plot, no plot yet), which the dialog then offers to cancel (A13) */
export function pickModeOf(ask: AskEvent, view: View | undefined, hasPlot: boolean, auto = false): PickMode | null {
  const win = Number(ask.win);
  const plot = hasPlot && win >= 1 && win <= 10 && !!view && view.win === win && !view.three;
  if (!plot && !(auto && win === 101)) return null;
  switch (ask.kind) {
    case 'mouse': return 'point';
    case 'rubber': return ask.flag === 1 ? 'line' : 'box';
    case 'drag': return 'drag';
    default: return null;
  }
}

/** the mode for a new ask: the crosshair stays where it was when the same
    kind is asked again (Initialconds/mIce, a drag) */
export function startPick(prev: PickState | null, ask: AskEvent, mode: PickMode): PickState {
  const same = prev && prev.mode === mode && prev.win === Number(ask.win);
  return {mode, win: Number(ask.win), ask: ask.id, cursor: same ? prev.cursor : CENTRE, anchor: null, waiting: false};
}

const clamp01 = (v: number) => Math.max(0, Math.min(1, v));

/** where an arrow key moves a position; null for any other key */
export function arrowStep(key: string, shift: boolean): {dx: number; dy: number} | null {
  const s = shift ? BIG_STEP : STEP;
  switch (key) {
    case 'ArrowLeft': return {dx: -s, dy: 0};
    case 'ArrowRight': return {dx: s, dy: 0};
    case 'ArrowUp': return {dx: 0, dy: -s};
    case 'ArrowDown': return {dx: 0, dy: s};
    default: return null;
  }
}

export function moved(f: Frac, dx: number, dy: number): Frac {
  return {fx: clamp01(f.fx + dx), fy: clamp01(f.fy + dy)};
}

/** one pointer event of a drag, at a position of the area */
export interface DragStep {
  what: 'down' | 'move' | 'up';
  at: Frac;
}

export type PickKeyResult =
  | {pick: PickState}
  | {confirm: PickState}
  | {drag: DragStep[]}
  | {cancel: true}
  | null;

/** what a key does in a plot mode; null: not a key of the mode */
export function pickKey(p: PickState, key: string, shift: boolean): PickKeyResult {
  if (key === 'Escape') return {cancel: true};
  const step = arrowStep(key, shift);
  if (p.mode === 'drag') {
    if (key === 'Enter') return {cancel: true}; /* done: any key ends XPP's scroll */
    if (!step) return null;
    /* the arrow shows more on its side, as the plot's own arrow keys do: the
       plot is dragged the other way, from the middle of the area */
    const to = moved(CENTRE, -step.dx, -step.dy);
    return {drag: [{what: 'down', at: CENTRE}, {what: 'move', at: to}, {what: 'up', at: to}]};
  }
  if (step) return {pick: {...p, cursor: moved(p.cursor, step.dx, step.dy)}};
  if (key !== 'Enter') return null;
  if (p.mode === 'point' || p.anchor) return {confirm: p};
  return {pick: {...p, anchor: p.cursor}};
}

/** a position of the area in data coordinates of `r` (what the plot shows) */
export function toData(r: Ranges, f: Frac): {x: number; y: number} {
  return {x: r.x.min + f.fx * (r.x.max - r.x.min), y: r.y.max - f.fy * (r.y.max - r.y.min)};
}

/** the answer to the ask: the point, or the anchor and the cursor, in data coordinates */
export function pickAnswer(p: PickState, r: Ranges): Record<string, number> {
  const c = toData(r, p.cursor);
  if (p.mode === 'point' || p.mode === 'drag') return {xd: c.x, yd: c.y};
  const a = toData(r, p.anchor ?? p.cursor);
  return {xd: a.x, yd: a.y, xd2: c.x, yd2: c.y};
}

/** the words of the instruction bar: one text for the whole mode, so the bar
    keeps its height and the plot does not move under a drag */
export function pickInstruction(p: PickState, touch: boolean): string {
  const tap = touch ? 'Tap' : 'Click';
  switch (p.mode) {
    case 'point':
      return `${tap} a point in the plot, or move the crosshair with the arrow keys and press Enter.`;
    case 'box':
    case 'line':
      return `Drag ${p.mode === 'box' ? 'a box' : 'a line'} in the plot, or move the crosshair with the arrow keys `
        + 'and press Enter at each corner.';
    case 'drag':
      return 'Drag the plot to scroll it, or use the arrow keys. Done (or Escape) ends.';
  }
}
