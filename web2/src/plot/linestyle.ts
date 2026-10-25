/* The line styles the page draws as the core's pictures do (core/picture.h): the dash patterns and the marker
   glyphs come from the core's hello (`line_styles`), at a width and a size of 1, so a dash or a marker is
   decided once, in the core. Pure: no DOM. */
import type {LineStyle} from '../protocol/types';

/** a marker's glyph at size 1: a filled circle of radius `circle` (when above 0), a filled polygon, strokes */
export interface MarkerGlyph {
  circle: number;
  polygon: [number, number][];
  strokes: [[number, number], [number, number]][];
}

export interface LineGeometry {
  dash: Record<LineStyle['dash'], number[]>;
  markers: Record<LineStyle['marker'], MarkerGlyph>;
}

let geometry: LineGeometry | null = null;

/** the geometry hello gave (the page sets it when hello arrives) */
export function setLineGeometry(g: LineGeometry): void {
  geometry = g;
}

function known(): LineGeometry {
  if (!geometry) throw new Error('the line styles come with hello');
  return geometry;
}

/** the style of anything that carries its fields (a curve, a frozen curve of the protocol) */
export function styleOf(c: LineStyle): LineStyle {
  return {draw: c.draw, dash: c.dash, width: c.width, marker: c.marker, marker_size: c.marker_size};
}

/** a line is drawn / points are drawn for the style */
export const drawsLine = (s: LineStyle): boolean => s.draw !== 'points';
export const drawsPoints = (s: LineStyle): boolean => s.draw !== 'line';

/** the dash pattern of `s` in pixels (empty: solid): the pattern at width 1 times the width, at least 1 */
export function dashPattern(s: LineStyle): number[] {
  const u = Math.max(s.width, 1);
  return known().dash[s.dash].map(d => d * u);
}

/** the pixel radius of `s`'s markers (a size 0 is a dot) */
export function markerRadius(s: LineStyle): number {
  return Math.max(s.marker_size, 0.5);
}

/** where a path is built: Path2D, or the recording sinks the tests use */
export interface MarkerSink {
  moveTo(x: number, y: number): void;
  lineTo(x: number, y: number): void;
  closePath?(): void;
}

const CIRCLE_SIDES = 16;

/** `s`'s marker centred on (x, y), `scale` times its size in pixels: its filled shapes into `fill`, its strokes into `stroke` */
export function addMarker(fill: MarkerSink, stroke: MarkerSink, s: LineStyle, x: number, y: number, scale = 1): void {
  const g = known().markers[s.marker], r = markerRadius(s) * scale;
  if (g.circle > 0) {
    for (let k = 0; k <= CIRCLE_SIDES; k++) {
      const a = (2 * Math.PI * k) / CIRCLE_SIDES, px = x + g.circle * r * Math.cos(a), py = y + g.circle * r * Math.sin(a);
      if (k) fill.lineTo(px, py);
      else fill.moveTo(px, py);
    }
  }
  if (g.polygon.length) {
    g.polygon.forEach(([dx, dy], k) => (k ? fill.lineTo(x + dx * r, y + dy * r) : fill.moveTo(x + dx * r, y + dy * r)));
    fill.closePath?.();
  }
  for (const [[ax, ay], [bx, by]] of g.strokes) {
    stroke.moveTo(x + ax * r, y + ay * r);
    stroke.lineTo(x + bx * r, y + by * r);
  }
}
