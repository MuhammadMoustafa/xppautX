/* The array plot (docs/ui-v2.md T12, docs/protocol.md `aplot`): a grid of
   cells the core computes from a range of stored columns and rows --
   `values`, the numbers before XPP maps them to a colour (`cells` stays
   the core's own mapping, for the classic page only). This slice keeps the
   latest event decoded once, the colour map the client picked (its own
   choice, never sent to the core), whether the core's array plot window
   (105) exists, the panel's own open state (R6: a section under the plot
   from 48rem, a full-screen sheet under that, like store/table.ts) and the
   hovered/picked cell. The core owns everything about what the array shows
   (columns, rows, skip, z range) through the Edit dialog and the other
   buttons (session.aplotOp): this slice never computes any of that. Pure:
   no DOM, no I/O. */
import {decode} from '../protocol/decode';
import type {AplotEvent} from '../protocol/types';
import type {AplotColorMap} from '../plot/aplotColors';

export type {AplotColorMap} from '../plot/aplotColors';

/** the cell under the pointer, or read out with the keyboard: its grid
    position, the approximate time of its row (interpolated, see `timeAt`:
    the core only sends the extremes tlo/thi) and its value */
export interface AplotHover {
  row: number;
  col: number;
  t: number;
  value: number;
}

export interface AplotState {
  event: AplotEvent | null;
  /** `event.values` decoded once (protocol/decode.ts), not on every paint */
  values: Float32Array;
  /** the core's array plot window (105) exists */
  windowOpen: boolean;
  /** the panel is shown */
  open: boolean;
  colorMap: AplotColorMap;
  hover: AplotHover | null;
}

export const initialAplot: AplotState = {
  event: null, values: new Float32Array(0), windowOpen: false, open: false, colorMap: 'viridis', hover: null,
};

export type AplotAction =
  | {type: 'event'; ev: AplotEvent}
  | {type: 'window'; open: boolean}
  | {type: 'panel'; open: boolean}
  | {type: 'colorMap'; map: AplotColorMap}
  | {type: 'hover'; hover: AplotHover | null};

export function reduceAplot(state: AplotState, action: AplotAction): AplotState {
  switch (action.type) {
    case 'event':
      return {...state, event: action.ev, values: decode(action.ev.values), hover: null};
    case 'window':
      /* the window closed (Close, or the core kills it): nothing left to show */
      return action.open === state.windowOpen ? state
        : action.open
          ? {...state, windowOpen: true}
          : {...state, windowOpen: false, open: false, event: null, values: new Float32Array(0), hover: null};
    case 'panel':
      return action.open === state.open ? state : {...state, open: action.open};
    case 'colorMap':
      return action.map === state.colorMap ? state : {...state, colorMap: action.map};
    case 'hover':
      return action.hover === state.hover ? state : {...state, hover: action.hover};
  }
}

/** the value at grid position (row, col), row-major like `cells`/`values`; NaN off the grid */
export function valueAt(state: AplotState, row: number, col: number): number {
  const ev = state.event;
  if (!ev || row < 0 || row >= ev.ny || col < 0 || col >= ev.nx) return NaN;
  return state.values[row * ev.nx + col];
}

/** the row's approximate time: the event only gives the extremes (tlo at
    row 0, thi at row ny-1); the rows between are evenly spaced in the
    stored data (core/ui_json.c send_aplot: nstart + nskip*row) */
export function timeAt(ev: AplotEvent, row: number): number {
  if (ev.ny <= 1) return ev.tlo;
  return ev.tlo + (ev.thi - ev.tlo) * (row / (ev.ny - 1));
}

/** the variable name at grid column `col` (0-based), read from the title
    the core sends ("root""lo".."hi", core/ui_json.c send_aplot/get_root):
    the same approximation the core's own label uses (unit stride; a custom
    column skip, ColSkip > 1, is not reflected in the title either). Empty
    when the title does not parse that way. */
export function columnName(ev: AplotEvent, col: number): string {
  const m = /^(.*?)(-?\d+)\.\.(-?\d+)$/.exec(ev.title);
  return m ? `${m[1]}${Number(m[2]) + col}` : '';
}
