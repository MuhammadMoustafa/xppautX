/* What a plot window shows on top of its curves, as the store keeps it
   (docs/protocol.md "The plot as data", `marks`, docs/ui-v2.md T8): the
   equilibria Sing pts marked, Text,etc's labels (with their text as
   Unicode runs, plot/richtext.ts), arrows and markers, and the frozen
   curves with their values decoded to float32. Pure: no DOM. */
import {decode} from '../protocol/decode';
import type {CoreColor, LineStyle, MarksEvent, TextStyle} from '../protocol/types';
import {parseRichText, type TextRun} from '../plot/richtext';

/** an equilibrium's stability, as the core names it (the `marks` event) */
export type Stability = MarksEvent['equilibria'][number]['type'];

export interface EquilibriumMark {
  x: number;
  y: number;
  type: Stability;
}

export interface TextMark {
  /** the core's name for it, which the `text` command takes */
  id: number;
  x: number;
  y: number;
  /** as XPP has it, escapes and all */
  raw: string;
  /** as written: what the dialog edits (raw has its \{expr}s filled in) */
  source: string;
  runs: TextRun[];
  /** the runs' text, for names and descriptions */
  plain: string;
  /** 0-4 */
  size: number;
  /** the style by its name */
  style: TextStyle;
  color: CoreColor;
}

/** what a text takes in its dialog (core/grobs.h LabelLook) */
export interface TextValues {
  text: string;
  size: number;
  style: TextStyle;
  color: CoreColor;
}

/** where a text is, in the plot's coordinates */
export interface TextPosition {x: number; y: number}

export interface ArrowMark {
  id?: number;
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
  color: CoreColor;
}

/** a marker's shape, as the core names it (core/marks_data.cpp MARKER_SHAPES) */
export type MarkerShape = MarksEvent['markers'][number]['shape'];

export interface MarkerMark {
  id?: number;
  x: number;
  y: number;
  shape: MarkerShape;
  size: number;
  color: CoreColor;
}

export interface FrozenCurve {
  /** the legend's name: the key, else the name */
  label: string;
  color: CoreColor;
  style: LineStyle;
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


export function marksFromEvent(ev: MarksEvent): Marks {
  return {
    equilibria: ev.equilibria.map(e => ({x: e.x, y: e.y, type: e.type})),
    text: ev.text.map(t => {
      const runs = parseRichText(t.text, t.font === 1);
      return {id: t.id, x: t.x, y: t.y, raw: t.text, source: t.source, runs, plain: runs.map(r => r.text).join(''), size: t.size, style: t.style,
        color: t.color};
    }),
    arrows: ev.arrows.map(a => ({id: a.id, pointer: a.kind === 'pointer', x1: a.x1, y1: a.y1, x2: a.x2, y2: a.y2, size: a.size,
      color: a.color})),
    markers: ev.markers.map(m => ({id: m.id, x: m.x, y: m.y, shape: m.shape, size: m.size, color: m.color})),
    frozen: ev.frozen.map((f, i) => ({label: f.key || f.name || `Frozen ${i + 1}`, color: f.color,
      style: {draw: f.draw, dash: f.dash, width: f.width, marker: f.marker, marker_size: f.marker_size},
      xs: decode(f.x), ys: decode(f.y)})),
  };
}

/** marks of any kind */
export function markCount(m: Marks | null): number {
  return m ? m.equilibria.length + m.text.length + m.arrows.length + m.markers.length + m.frozen.length : 0;
}

export function annotationRows(marks: Marks | null): {id: number; kind: string; text: string; x: number; y: number; color: CoreColor; values: Record<string, unknown>}[] {
  if (!marks) return [];
  return [
    ...marks.text.map(t => ({id: t.id, kind: 'text', text: t.source, x: t.x, y: t.y, color: t.color,
      values: {annotation_id: t.id, text: t.source, text_size: t.size, style: t.style, color: t.color}})),
    ...marks.arrows.filter(a => a.id !== undefined).map(a => ({id: a.id!, kind: a.pointer ? 'pointer' : 'arrow', text: '', x: a.x1, y: a.y1, color: a.color,
      values: {annotation_id: a.id, size: a.size, color: a.color}})),
    ...marks.markers.filter(m => m.id !== undefined).map(m => ({id: m.id!, kind: 'marker', text: m.shape, x: m.x, y: m.y, color: m.color,
      values: {annotation_id: m.id, size: m.size, shape: m.shape, color: m.color}})),
  ];
}
