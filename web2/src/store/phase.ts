/* What a phase plane shows besides its curves, as the store keeps it
   (docs/protocol.md "The plot as data", `nullclines` and `dfield`,
   docs/ui-v2.md T7): the events' value arrays decoded to float32, as the
   core computed them. Pure: no DOM. */
import {decode} from '../protocol/decode';
import type {DfieldEvent, NullclinesEvent} from '../protocol/types';

export interface Segments {
  /** x1, y1, x2, y2 per segment, plot coordinates */
  x: Float32Array;
  y: Float32Array;
}

export interface Nullclines extends Segments {
  xName: string;
  yName: string;
  xColor: number;
  yColor: number;
  frozen: Segments[];
}

export interface FlowCurve {
  color: number;
  /** the trajectories one after the other, NaN between two */
  xs: Float32Array;
  ys: Float32Array;
}

export interface Dfield {
  /** grid points a side; 0: no field */
  n: number;
  du: number;
  dv: number;
  scaled: boolean;
  color: number;
  /** x, y, ux, uy per arrow */
  grid: Float32Array;
  speed: Float32Array;
  flows: FlowCurve[];
}

export function nullclinesFromEvent(ev: NullclinesEvent): Nullclines {
  return {
    xName: ev.xname,
    yName: ev.yname,
    xColor: ev.xcolor,
    yColor: ev.ycolor,
    x: decode(ev.x),
    y: decode(ev.y),
    frozen: ev.frozen.map(f => ({x: decode(f.x), y: decode(f.y)})),
  };
}

export function dfieldFromEvent(ev: DfieldEvent): Dfield {
  return {
    n: ev.n,
    du: ev.du,
    dv: ev.dv,
    scaled: ev.scaled !== 0,
    color: ev.color,
    grid: decode(ev.grid),
    speed: decode(ev.speed),
    flows: ev.flows.map(f => ({color: f.color, xs: decode(f.x), ys: decode(f.y)})),
  };
}

/** segments in a list of 4 values each */
export const segmentCount = (a: Float32Array): number => a.length >> 2;

/** trajectories in a flow curve (NaN separates two) */
export function trajectoryCount(xs: Float32Array): number {
  if (!xs.length) return 0;
  let n = 1;
  for (let i = 0; i < xs.length; i++) if (xs[i] !== xs[i]) n++;
  return n;
}
