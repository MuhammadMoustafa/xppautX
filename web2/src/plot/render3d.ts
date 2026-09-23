/* Draws a 3D plot's model (model3d.ts) on a canvas of any size: the box's
   wireframe, then its curves, fit around the box's own projected corners so
   turning it never clips (project3d.ts does the projection). No library (no
   three.js): a canvas 2D context, the same approach as the animation
   (ani/render.ts). Unlike the animation there can be more than one 3D
   window at once, so the last drawing (for tests, __xpp.plot(),
   testhook.ts) is returned rather than kept in a module global; chart3d.ts
   keeps it per window. */
import {BOX_EDGES} from './project3d';
import type {Point2} from './project3d';
import {curveColor} from './colors';
import type {Model3D} from './model3d';

export interface Draw3DInfo {
  width: number;
  height: number;
  theta: number;
  phi: number;
  curves: {label: string; points: number}[];
  /** the box's own corners on the canvas, for a test to check the frame moved */
  box: ({x: number; y: number} | null)[];
  at: number;
}

interface Frame {
  cx: number;
  cy: number;
  scale: number;
}

/** the projected box's own corners, fit into `cw` x `ch` less `pad` pixels
    of margin, one scale for x and y alike so a rotation never distorts it */
function fitFrame(cw: number, ch: number, pts: (Point2 | null)[], pad: number): Frame {
  let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
  for (const p of pts) {
    if (!p) continue;
    if (p.x < minX) minX = p.x;
    if (p.x > maxX) maxX = p.x;
    if (p.y < minY) minY = p.y;
    if (p.y > maxY) maxY = p.y;
  }
  if (!(minX <= maxX) || !(minY <= maxY)) return {cx: cw / 2, cy: ch / 2, scale: 1};
  const w = Math.max(1e-9, maxX - minX), h = Math.max(1e-9, maxY - minY);
  const aw = Math.max(1, cw - 2 * pad), ah = Math.max(1, ch - 2 * pad);
  const scale = Math.min(aw / w, ah / h);
  return {cx: cw / 2 - ((minX + maxX) / 2) * scale, cy: ch / 2 + ((minY + maxY) / 2) * scale, scale};
}

/** a projected point (y up) to canvas pixels (y down) */
function toCanvas(f: Frame, p: Point2): [number, number] {
  return [f.cx + p.x * f.scale, f.cy - p.y * f.scale];
}

/** draws `model` (or clears the canvas without one) on `canvas`, sized to
    cw x ch CSS pixels; returns what it drew, for a test (null with no
    model). `axisColor` is the wireframe's stroke, a muted foreground. */
export function draw3d(
  canvas: HTMLCanvasElement, cw: number, ch: number, model: Model3D | null, theta: number, phi: number, dark: boolean,
  axisColor: string,
): Draw3DInfo | null {
  const dpr = window.devicePixelRatio || 1;
  const pw = Math.max(1, Math.round(cw * dpr)), ph = Math.max(1, Math.round(ch * dpr));
  if (canvas.width !== pw) canvas.width = pw;
  if (canvas.height !== ph) canvas.height = ph;
  const g = canvas.getContext('2d');
  if (!g) return null;
  g.setTransform(dpr, 0, 0, dpr, 0, 0);
  g.clearRect(0, 0, cw, ch);
  if (!model) return null;
  const f = fitFrame(cw, ch, model.box, 24);
  g.strokeStyle = axisColor;
  g.lineWidth = 1;
  g.beginPath();
  for (const [i, j] of BOX_EDGES) {
    const a = model.box[i], b = model.box[j];
    if (!a || !b) continue;
    const [x1, y1] = toCanvas(f, a), [x2, y2] = toCanvas(f, b);
    g.moveTo(x1, y1);
    g.lineTo(x2, y2);
  }
  g.stroke();
  g.lineCap = 'round';
  g.lineJoin = 'round';
  for (const c of model.curves) {
    g.strokeStyle = curveColor(c.color, dark);
    g.fillStyle = g.strokeStyle;
    if (c.line) {
      g.lineWidth = 1.5;
      g.beginPath();
      let open = false;
      for (const p of c.points) {
        if (!p) {
          open = false;
          continue;
        }
        const [x, y] = toCanvas(f, p);
        if (open) g.lineTo(x, y);
        else g.moveTo(x, y);
        open = true;
      }
      g.stroke();
    } else {
      for (const p of c.points) {
        if (!p) continue;
        const [x, y] = toCanvas(f, p);
        g.beginPath();
        g.arc(x, y, c.radius, 0, 2 * Math.PI);
        g.fill();
      }
    }
  }
  return {
    width: cw, height: ch, theta, phi,
    curves: model.curves.map(c => ({label: c.label, points: c.points.filter(p => p).length})),
    box: model.box.map(p => (p ? {x: toCanvas(f, p)[0], y: toCanvas(f, p)[1]} : null)),
    at: performance.now(),
  };
}
