/* Draws an animation frame (store/ani.ts) on a canvas of any size: the
   dimension box letterboxed at its own aspect (ani/frame.ts), clipped to
   it, pixel sizes scaled with the picture. The last drawing is described
   for tests (__xpp.ani(), testhook.ts), which read that, never pixels. */
import {curveColor} from '../plot/colors';
import type {AniColor} from '../protocol/types';
import type {AniFrame} from '../store/ani';
import {
  aspectOf, circleRadius, fitBox, lineWidth, penScale, textOf, textPx, toCanvas, type Box,
} from './frame';

export interface AniDrawInfo {
  /** the frame drawn (its stored row) and how many primitives */
  pos: number;
  prims: number;
  /** the canvas, and the dimension box on it, CSS pixels */
  width: number;
  height: number;
  box: Box;
  scale: number;
  /** when (performance.now()) */
  at: number;
}

let last: AniDrawInfo | null = null;

export function aniDrawInfo(): AniDrawInfo | null {
  return last;
}

/** the box a frame of `dim` takes on a cw x ch canvas */
export function frameBox(cw: number, ch: number, dim: readonly number[]): Box {
  return fitBox(cw, ch, aspectOf(dim), 8);
}

function paint(c: AniColor, dark: boolean): string {
  return typeof c === 'string' ? c : curveColor(c, dark);
}

/** draws `frame` (or only the empty box without one) on `canvas`, sized to cw x ch CSS pixels */
export function drawAniFrame(canvas: HTMLCanvasElement, cw: number, ch: number, frame: AniFrame | null, dark: boolean,
  surface: string): void {
  const dpr = window.devicePixelRatio || 1;
  const pw = Math.max(1, Math.round(cw * dpr)), ph = Math.max(1, Math.round(ch * dpr));
  if (canvas.width !== pw) canvas.width = pw;
  if (canvas.height !== ph) canvas.height = ph;
  const g = canvas.getContext('2d');
  if (!g) return;
  g.setTransform(dpr, 0, 0, dpr, 0, 0);
  g.clearRect(0, 0, cw, ch);
  if (!frame) {
    last = null;
    return;
  }
  const box = frameBox(cw, ch, frame.dim), s = penScale(box, frame.w, frame.h);
  g.fillStyle = surface;
  g.fillRect(box.x, box.y, box.w, box.h);
  g.save();
  g.beginPath();
  g.rect(box.x, box.y, box.w, box.h);
  g.clip();
  g.lineCap = 'round';
  g.lineJoin = 'round';
  for (const p of frame.prims) {
    const col = paint(p.color, dark);
    g.strokeStyle = col;
    g.fillStyle = col;
    switch (p.kind) {
      case 'line': {
        const [x1, y1] = toCanvas(box, p.u1, p.v1), [x2, y2] = toCanvas(box, p.u2, p.v2);
        g.lineWidth = lineWidth(p.width, s);
        g.beginPath();
        g.moveTo(x1, y1);
        g.lineTo(x2, y2);
        g.stroke();
        break;
      }
      case 'rect': {
        const [x1, y1] = toCanvas(box, p.u1, p.v1), [x2, y2] = toCanvas(box, p.u2, p.v2);
        const x = Math.min(x1, x2), y = Math.min(y1, y2), w = Math.abs(x2 - x1), h = Math.abs(y2 - y1);
        if (p.fill) g.fillRect(x, y, w, h);
        else {
          g.lineWidth = lineWidth(p.width, s);
          g.strokeRect(x, y, w, h);
        }
        break;
      }
      case 'circle':
      case 'ellipse': {
        const [x, y] = toCanvas(box, p.u, p.v);
        const r = circleRadius(box, p.ru, p.rv);
        const rx = p.kind === 'circle' ? r : p.ru * box.w, ry = p.kind === 'circle' ? r : p.rv * box.h;
        g.beginPath();
        g.ellipse(x, y, rx, ry, 0, 0, 2 * Math.PI);
        if (p.fill) g.fill();
        else {
          g.lineWidth = lineWidth(p.width, s);
          g.stroke();
        }
        break;
      }
      case 'dot': {
        const [x, y] = toCanvas(box, p.u, p.v);
        g.beginPath();
        g.arc(x, y, Math.max(0.5, p.r * s), 0, 2 * Math.PI);
        g.fill();
        break;
      }
      case 'text': {
        const [x, y] = toCanvas(box, p.u, p.v);
        g.font = `${textPx(p.size, s)}px Inter, system-ui, sans-serif`;
        g.fillText(textOf(p), x, y);
        break;
      }
    }
  }
  g.restore();
  last = {pos: frame.pos, prims: frame.prims.length, width: cw, height: ch, box, scale: s, at: performance.now()};
}
