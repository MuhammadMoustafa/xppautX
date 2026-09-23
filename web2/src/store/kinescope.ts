/* The kinescope (docs/ui-v2.md T15, docs/protocol.md `film`): the core
   decides when a frame is captured, reset or played (its Kinescope menu,
   reached with the keys k then c/r/p/a) and tells the client with a `film`
   event; the client is the one that keeps the frames (as data, the same
   shapes the plot draws from: store/plots.ts's PlotWindow) and does the
   actual playback and export. `pixels` asks from the core's own frame, GIF
   and kinescope writers are answered from this data too (session.ts,
   plot/kinescopeRender.ts), never from a screenshot. Pure: no DOM, no
   timers; the playback clock lives in session.ts. */
import type {PlotWindow, Viewport} from './plots';

/** what a plot draws (store/plots.ts PlotWindow), captured at one instant */
export interface KinescopeFrame {
  win: number;
  info: PlotWindow['info'];
  series: PlotWindow['series'];
  nullclines: PlotWindow['nullclines'];
  dfield: PlotWindow['dfield'];
  marks: PlotWindow['marks'];
  viewport: Viewport;
}

export function snapshotWindow(w: PlotWindow): KinescopeFrame {
  return {win: w.win, info: w.info, series: w.series, nullclines: w.nullclines, dfield: w.dfield, marks: w.marks,
    viewport: w.viewport};
}

export interface KinescopeState {
  frames: KinescopeFrame[];
  /** a play or autoplay is under way (session.ts's timer steps `shown`) */
  playing: boolean;
  /** the frame index on screen while playing, or last shown; null before any play */
  shown: number | null;
  /** autoplay's count and delay (docs/protocol.md `film`), for the player and the next Export GIF */
  cycles: number;
  delay: number;
}

export const initialKinescope: KinescopeState = {frames: [], playing: false, shown: null, cycles: 1, delay: 50};

export type KinescopeAction =
  | {type: 'capture'; frame: KinescopeFrame}
  | {type: 'clear'}
  | {type: 'show'; index: number}
  | {type: 'playing'; playing: boolean; cycles?: number; delay?: number};

export function reduceKinescope(state: KinescopeState, action: KinescopeAction): KinescopeState {
  switch (action.type) {
    case 'capture':
      return {...state, frames: [...state.frames, action.frame]};
    case 'clear':
      return initialKinescope;
    case 'show':
      return action.index === state.shown ? state : {...state, shown: action.index};
    case 'playing':
      return {
        ...state, playing: action.playing, cycles: action.cycles ?? state.cycles, delay: action.delay ?? state.delay,
      };
  }
}
