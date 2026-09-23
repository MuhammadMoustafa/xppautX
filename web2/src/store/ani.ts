/* The animation (docs/ui-v2.md T13, docs/protocol.md "The animation as
   data"): the core's animation window (104), its player state (the `ani`
   event), the last frame it drew (the `ani` `frame` event, decoded), and
   whether the page shows the panel and has Go running. The frame index a
   view shows is the drawn frame's `pos`; `pos` here is where the core's
   next step starts (after Go ends it is back at 0 while the last frame
   stays on screen, as in XPP). Pure: no DOM, no I/O, no timers. */
import {decodePrims, type AniPrim} from '../ani/frame';
import type {AniFrameEvent, AniStateEvent} from '../protocol/types';

export interface AniFrame {
  /** the stored row the frame shows */
  pos: number;
  rows: number;
  t: number | null;
  /** the dimension box: xlo, ylo, xhi, yhi */
  dim: [number, number, number, number];
  /** the classic window's size: pixel widths are relative to it */
  w: number;
  h: number;
  prims: AniPrim[];
}

export interface AniState {
  /** the panel is shown (R6: a side panel from 48rem, a full-screen sheet under) */
  open: boolean;
  /** the core's animation window exists (Viewaxes/Toon, -anifile) */
  exists: boolean;
  /** an .ani file is loaded */
  loaded: boolean;
  /** Go is running (sent until its idle) */
  playing: boolean;
  /** where the core's next step starts, of `rows` */
  pos: number;
  rows: number;
  /** ms between frames of Go */
  speed: number;
  skip: number;
  /** Grab is waiting for the pointer */
  grab: boolean;
  fly: boolean;
  frame: AniFrame | null;
  /** frame events received: tests wait on it */
  frames: number;
}

export const initialAni: AniState = {
  open: false, exists: false, loaded: false, playing: false, pos: 0, rows: 0, speed: 10, skip: 1, grab: false,
  fly: false, frame: null, frames: 0,
};

export type AniAction =
  | {type: 'open'; open: boolean}
  | {type: 'window'; exists: boolean}
  | {type: 'state'; ev: AniStateEvent}
  | {type: 'frame'; ev: AniFrameEvent}
  | {type: 'playing'; playing: boolean};

export function reduceAni(state: AniState, action: AniAction): AniState {
  switch (action.type) {
    case 'open':
      return action.open === state.open ? state : {...state, open: action.open};
    case 'window':
      /* a window the core opens is shown; a closed one leaves the panel as it is */
      return action.exists ? {...state, exists: true, open: true} : {...state, exists: false, playing: false};
    case 'state': {
      const ev = action.ev;
      return {
        ...state, pos: ev.pos, rows: ev.rows, speed: ev.speed, skip: ev.skip, grab: !!ev.grab, fly: !!ev.fly,
        loaded: ev.loaded === undefined ? state.loaded : !!ev.loaded,
        exists: ev.open === undefined ? state.exists : !!ev.open,
      };
    }
    case 'frame': {
      const ev = action.ev;
      const frame: AniFrame = {
        pos: ev.pos, rows: ev.rows, t: ev.t, dim: ev.dim, w: ev.w, h: ev.h, prims: decodePrims(ev.prims ?? []),
      };
      return {...state, frame, rows: ev.rows, speed: ev.speed, skip: ev.skip, loaded: true, frames: state.frames + 1};
    }
    case 'playing':
      return action.playing === state.playing ? state : {...state, playing: action.playing};
  }
}

/** the frame a step of n from the shown frame lands on, within 0..rows-1
    (the core's own step goes from its next position, which after Pause or
    at the end of Go is not the frame shown) */
export function stepTarget(state: AniState, n: number): number {
  const from = state.frame?.pos ?? state.pos;
  return Math.max(0, Math.min(Math.max(0, state.rows - 1), from + n));
}
