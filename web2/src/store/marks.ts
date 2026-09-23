/* What a plot window shows on top of its curves, as the store keeps it
   (docs/protocol.md "The plot as data", `marks`, docs/ui-v2.md T8): the
   equilibria Sing pts marked, Text,etc's labels (with their text as
   Unicode runs, plot/richtext.ts), arrows and markers, and the frozen
   curves with their values decoded to float32. Pure: no DOM. */
import {decode} from '../protocol/decode';
import type {MarksEvent} from '../protocol/types';
import {parseRichText, type TextRun} from '../plot/richtext';

export type Stability = 'stable' | 'unstable' | 'saddle';

export interface EquilibriumMark {
  x: number;
  y: number;
  type: Stability;
}

export interface TextMark {
  x: number;
  y: number;
  /** as XPP has it, escapes and all */
  raw: string;
  runs: TextRun[];
  /** the runs' text, for names and descriptions */
  plain: string;
  /** 0-4 */
  size: number;
}

export interface ArrowMark {
  /** a pointer has a shaft; an arrow is only its head */
  pointer: boolean;
  /** the head's tip */
  x1: number;
  y1: number;
  /** where the head points from */
  x2: number;
  y2: number;
  /** the head's length as a fraction of the whole */
  size: number;
  color: number;
}

export type MarkerShape = 'box' | 'diamond' | 'triangle' | 'plus' | 'cross' | 'circle';

export interface MarkerMark {
  x: number;
  y: number;
  shape: MarkerShape;
  size: number;
  color: number;
}

export interface FrozenCurve {
  /** the legend's name: the key, else the name */
  label: string;
  color: number;
  line: boolean;
  xs: Float32Array;
  ys: Float32Array;
}

export interface Marks {
  equilibria: EquilibriumMark[];
  text: TextMark[];
  arrows: ArrowMark[];
  markers: MarkerMark[];
  frozen: FrozenCurve[];
}

const STABILITY: Record<string, Stability> = {stable: 'stable', unstable: 'unstable', saddle: 'saddle'};
const SHAPES: MarkerShape[] = ['box', 'diamond', 'triangle', 'plus', 'cross', 'circle'];

export function marksFromEvent(ev: MarksEvent): Marks {
  return {
    equilibria: ev.equilibria.map(e => ({x: e.x, y: e.y, type: STABILITY[e.type] ?? 'unstable'})),
    text: ev.text.map(t => {
      const runs = parseRichText(t.text, t.font === 1);
      return {x: t.x, y: t.y, raw: t.text, runs, plain: runs.map(r => r.text).join(''), size: t.size};
    }),
    arrows: ev.arrows.map(a => ({pointer: a.kind === 'pointer', x1: a.x1, y1: a.y1, x2: a.x2, y2: a.y2, size: a.size,
      color: a.color})),
    markers: ev.markers.map(m => ({x: m.x, y: m.y, shape: SHAPES.includes(m.shape as MarkerShape)
      ? m.shape as MarkerShape : 'box', size: m.size, color: m.color})),
    frozen: ev.frozen.map((f, i) => ({label: f.key || f.name || `Frozen ${i + 1}`, color: f.color, line: f.line !== 0,
      xs: decode(f.x), ys: decode(f.y)})),
  };
}

/** marks of any kind */
export function markCount(m: Marks | null): number {
  return m ? m.equilibria.length + m.text.length + m.arrows.length + m.markers.length + m.frozen.length : 0;
}
