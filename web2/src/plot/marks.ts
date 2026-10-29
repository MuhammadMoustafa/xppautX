/* Marks on the plot (docs/ui-v2.md T8): which layers a window's marks make,
   named as the legend shows them, and their geometry on the chart's canvas
   in canvas pixels. The chart (chart.ts) draws what these return: frozen
   curves under the curves, the rest over them. Shapes keep XPP's (its
   equilibrium symbols: a circle stable, a box unstable, a triangle a
   saddle; its six markers), at a size in pixels rather than a fraction of
   the axes, so they stay readable at any zoom. Pure: no DOM. */
import {traceLine, tracePoints, type PixelFrame} from './decimate';
import type {CoreColor} from './colors';
import {addMarker, drawsLine, drawsPoints, markerRadius} from './linestyle';
import {pixelMap, type PathSink} from './phase';
import type {LineStyle, TextStyle} from '../protocol/types';
import type {ArrowMark, EquilibriumMark, Marks, MarkerMark, MarkerShape} from '../store/marks';

export type MarkKey = 'equilibria' | 'text' | 'arrows' | 'markers' | `frozen-${number}`;

export interface MarkLayer {
  key: MarkKey;
  /** the legend's name: "Equilibria", "Text", "Arrows", "Markers", a frozen curve's key */
  label: string;
  color: CoreColor;
  /** marks, or a frozen curve's points */
  count: number;
}

/** the layers `m` has, in legend order; a kind with no marks is left out */
export function markLayers(m: Marks | null): MarkLayer[] {
  if (!m) return [];
  const out: MarkLayer[] = [];
  if (m.equilibria.length) out.push({key: 'equilibria', label: 'Equilibria', color: 'foreground', count: m.equilibria.length});
  if (m.text.length) out.push({key: 'text', label: 'Text', color: 'foreground', count: m.text.length});
  if (m.arrows.length) out.push({key: 'arrows', label: 'Arrows', color: m.arrows[0].color, count: m.arrows.length});
  if (m.markers.length) out.push({key: 'markers', label: 'Markers', color: m.markers[0].color, count: m.markers.length});
  m.frozen.forEach((f, i) => out.push({key: `frozen-${i}`, label: f.label, color: f.color, count: f.xs.length}));
  return out;
}

/** a regular polygon of n corners about (x, y), the first at angle a0 (canvas pixels, y down) */
function polygon(sink: PathSink, x: number, y: number, r: number, n: number, a0: number): void {
  for (let k = 0; k <= n; k++) {
    const a = a0 + (2 * Math.PI * k) / n, px = x + r * Math.cos(a), py = y - r * Math.sin(a);
    if (k) sink.lineTo(px, py);
    else sink.moveTo(px, py);
  }
}

/** an equilibrium's symbol, `r` pixels from its centre to its edge; the
    centre itself is a dot the caller adds */
export function equilibriumPath(e: EquilibriumMark, f: PixelFrame, r: number, sink: PathSink): boolean {
  const m = pixelMap(f), x = m.x(e.x), y = m.y(e.y);
  if (!Number.isFinite(x) || !Number.isFinite(y)) return false;
  if (e.type === 'stable') polygon(sink, x, y, r, 24, 0);
  else if (e.type === 'saddle') polygon(sink, x, y, 1.4 * r, 3, Math.PI / 2);
  else polygon(sink, x, y, r * Math.SQRT2, 4, Math.PI / 4);
  return true;
}

/** the glyphs of the annotations as the core draws them (core/grobs.h, hello's `annotation_glyphs`): each marker
    shape's polylines in glyph units, y up, `half_units` of them from the centre to the edge at size 1; an arrow
    head's strokes spread `arrow_head_spread` of their length either side */
export interface AnnotationGlyphs {
  half_units: number;
  arrow_head_spread: number;
  markers: Record<MarkerShape, [number, number][][]>;
}

let glyphs: AnnotationGlyphs | null = null;

/** the glyphs hello gave (the page sets them when hello arrives) */
export function setAnnotationGlyphs(g: AnnotationGlyphs): void {
  glyphs = g;
}

function glyphsKnown(): AnnotationGlyphs {
  if (!glyphs) throw new Error('the annotation glyphs come with hello');
  return glyphs;
}

/** pixels from a marker's centre to its edge at size 1 */
export const MARKER_PX = 4;

/** a marker's outline, `px` pixels to its edge at size 1 */
export function markerPath(k: MarkerMark, f: PixelFrame, px: number, sink: PathSink): boolean {
  const g = glyphsKnown(), m = pixelMap(f), x = m.x(k.x), y = m.y(k.y), u = (px * (k.size > 0 ? k.size : 1)) / g.half_units;
  if (!Number.isFinite(x) || !Number.isFinite(y)) return false;
  for (const stroke of g.markers[k.shape])
    stroke.forEach(([dx, dy], i) => (i ? sink.lineTo(x + dx * u, y - dy * u) : sink.moveTo(x + dx * u, y - dy * u)));
  return true;
}

/** an arrow or pointer as XPP draws it (grobs.cpp arrow_head): the head's
    tip at (x1, y1), its two strokes back to a `size` part of the way to
    (x2, y2), spread by the glyphs' `arrow_head_spread` of their length; a pointer's shaft
    too. The head is worked out on screen, so it keeps its shape whatever
    the axes' scales */
export function arrowPath(a: ArrowMark, f: PixelFrame, sink: PathSink): boolean {
  const spread = glyphsKnown().arrow_head_spread, m = pixelMap(f), x1 = m.x(a.x1), y1 = m.y(a.y1), x2 = m.x(a.x2), y2 = m.y(a.y2);
  if (![x1, y1, x2, y2].every(Number.isFinite)) return false;
  const lx = x2 - x1, ly = y2 - y1;
  const bx = x1 + a.size * lx, by = y1 + a.size * ly;
  const px = -spread * a.size * ly, py = spread * a.size * lx;
  if (a.pointer) {
    sink.moveTo(x1, y1);
    sink.lineTo(x2, y2);
  }
  sink.moveTo(bx + px, by + py);
  sink.lineTo(x1, y1);
  sink.lineTo(bx - px, by - py);
  return true;
}

/** the paths of a curve of style `style` (a frozen curve or an earlier run) on `f`: its polyline into
    `line`, its markers into `fill` (the filled shapes) and `stroke` (the stroked ones), through decimate.ts's
    tracers, which leave out what changes no pixel (W82: an earlier run of a million rows was a path of most of
    its points, stroked again at every frame of a live run); returns the vertices and markers drawn. `scale` is
    the pixel ratio of the canvas */
export function traceCurve(xs: Float32Array, ys: Float32Array, style: LineStyle, f: PixelFrame, scale: number,
  line: PathSink, fill: PathSink, stroke: PathSink): number {
  const last = Math.min(xs.length, ys.length) - 1;
  let drawn = 0;
  if (drawsLine(style)) drawn += traceLine(xs, ys, 0, last, f, line);
  if (drawsPoints(style))
    drawn += tracePoints(xs, ys, 0, last, f, markerRadius(style) * scale, (x, y) => addMarker(fill, stroke, style, x, y, scale));
  return drawn;
}

/** pixel sizes of XPP's text sizes 0-4 (its fonts are 8 to 24 points) */
export const TEXT_PX = [10, 12, 14, 18, 24];

export function textPx(size: number): number {
  return TEXT_PX[Math.max(0, Math.min(4, Math.round(size)))];
}

/** the CSS font style and weight each text style is drawn with, by its name (core/text_style.h) */
export const TEXT_CSS: Record<TextStyle, string> = {normal: 'normal', bold: 'bold', italic: 'italic', bold_italic: 'italic bold'};

/** Small annotations need a mouse target wider than the painted stroke. */
export const ANNOTATION_HIT_PX = 12;
/** The selection ring surrounds the anchor without covering the mark. */
export const ANNOTATION_SELECTION_PX = 8;
