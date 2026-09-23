/* Pointer gestures on the plotting area.
   Mouse: drag a box to zoom (uPlot's own), wheel to zoom about the pointer,
   Shift+drag or the middle button to pan, hover names the nearest point.
   Touch: one finger pans, two fingers pinch-zoom (and pan), a tap names the
   nearest point (or clears it). The keyboard is in plotKeys.ts. */
import type {Chart} from './chart';
import {panBy, zoomAbout, type Ranges} from './viewmath';

export interface HoverSink {
  hover(curve: number, index: number): void;
  leave(): void;
}

const WHEEL_STEP = 0.85;
const WHEEL_GESTURE_MS = 400; /* wheel ticks closer than this are one gesture (one undo step) */
const HOVER_PX = 24;
const TAP_PX = 32; /* a finger is less precise */
const TAP_SLOP = 8; /* movement that still counts as a tap */

export function attachGestures(chart: Chart, area: HTMLElement, sink: HoverSink): () => void {
  const local = (e: {clientX: number; clientY: number}) => {
    const r = area.getBoundingClientRect();
    return {x: e.clientX - r.left, y: e.clientY - r.top, w: r.width, h: r.height};
  };

  let lastWheel = -Infinity;
  const onWheel = (e: WheelEvent) => {
    e.preventDefault();
    const p = local(e), now = performance.now();
    chart.setView(zoomAbout(chart.ranges(), p.x / p.w, p.y / p.h, e.deltaY < 0 ? WHEEL_STEP : 1 / WHEEL_STEP),
      now - lastWheel > WHEEL_GESTURE_MS);
    lastWheel = now;
  };

  /* mouse pan: in the capture phase, before uPlot's drag-to-zoom sees the press */
  const onMouseDown = (e: MouseEvent) => {
    if (!(e.button === 1 || (e.button === 0 && e.shiftKey))) return;
    e.preventDefault();
    e.stopImmediatePropagation(); /* capture listeners at the target run before uPlot's */
    const start = chart.ranges(), p0 = local(e);
    let first = true;
    const move = (ev: MouseEvent) => {
      const p = local(ev);
      chart.setView(panBy(start, (p.x - p0.x) / p.w, (p.y - p0.y) / p.h), first);
      first = false;
    };
    const up = () => {
      window.removeEventListener('mousemove', move);
      window.removeEventListener('mouseup', up);
    };
    window.addEventListener('mousemove', move);
    window.addEventListener('mouseup', up);
  };

  /* hover is the mouse's (and a pen's): a finger reads a point by tapping, and
     the boundary events a tap brings must not clear what it read */
  const onHoverMove = (e: PointerEvent) => {
    if (e.pointerType === 'touch' || e.buttons) return;
    const p = local(e), hit = chart.hit(p.x, p.y, HOVER_PX);
    if (hit) sink.hover(hit.curve, hit.index);
    else sink.leave();
  };
  const onHoverLeave = (e: PointerEvent) => {
    if (e.pointerType !== 'touch') sink.leave();
  };

  /* touch: the active fingers, and the view and positions when the gesture (re)started */
  const fingers = new Map<number, {x: number; y: number}>();
  let startView: Ranges | null = null, startPts: {x: number; y: number}[] = [], moved = false, pushed = false;
  const restart = () => {
    startView = chart.ranges();
    startPts = [...fingers.values()].map(p => ({...p}));
  };
  const onPointerDown = (e: PointerEvent) => {
    if (e.pointerType !== 'touch') return;
    e.preventDefault(); /* no emulated mouse events: uPlot would start a box */
    area.setPointerCapture?.(e.pointerId);
    fingers.set(e.pointerId, local(e));
    if (fingers.size === 1) {
      moved = false;
      pushed = false;
    }
    restart();
  };
  const onPointerMove = (e: PointerEvent) => {
    if (e.pointerType !== 'touch' || !fingers.has(e.pointerId) || !startView) return;
    const p = local(e);
    fingers.set(e.pointerId, p);
    const now = [...fingers.values()];
    if (now.length === 1) {
      const dx = now[0].x - startPts[0].x, dy = now[0].y - startPts[0].y;
      if (!moved && Math.hypot(dx, dy) < TAP_SLOP) return;
      moved = true;
      chart.setView(panBy(startView, dx / p.w, dy / p.h), !pushed);
    } else if (now.length >= 2 && startPts.length >= 2) {
      moved = true;
      const d0 = Math.hypot(startPts[0].x - startPts[1].x, startPts[0].y - startPts[1].y);
      const d1 = Math.hypot(now[0].x - now[1].x, now[0].y - now[1].y);
      if (d0 < 1 || d1 < 1) return;
      const m0 = {x: (startPts[0].x + startPts[1].x) / 2, y: (startPts[0].y + startPts[1].y) / 2};
      const m1 = {x: (now[0].x + now[1].x) / 2, y: (now[0].y + now[1].y) / 2};
      const zoomed = zoomAbout(startView, m0.x / p.w, m0.y / p.h, d0 / d1);
      chart.setView(panBy(zoomed, (m1.x - m0.x) / p.w, (m1.y - m0.y) / p.h), !pushed);
    }
    pushed = true;
  };
  const onPointerUp = (e: PointerEvent) => {
    if (e.pointerType !== 'touch' || !fingers.has(e.pointerId)) return;
    const p = fingers.get(e.pointerId)!;
    fingers.delete(e.pointerId);
    if (fingers.size === 0 && !moved) {
      const hit = chart.hit(p.x, p.y, TAP_PX);
      if (hit) sink.hover(hit.curve, hit.index);
      else sink.leave();
    }
    if (fingers.size) restart();
  };

  area.addEventListener('wheel', onWheel, {passive: false});
  area.addEventListener('mousedown', onMouseDown, {capture: true});
  area.addEventListener('pointermove', onHoverMove);
  area.addEventListener('pointerleave', onHoverLeave);
  area.addEventListener('pointerdown', onPointerDown);
  area.addEventListener('pointermove', onPointerMove);
  area.addEventListener('pointerup', onPointerUp);
  area.addEventListener('pointercancel', onPointerUp);
  return () => {
    area.removeEventListener('wheel', onWheel);
    area.removeEventListener('mousedown', onMouseDown, {capture: true});
    area.removeEventListener('pointermove', onHoverMove);
    area.removeEventListener('pointerleave', onHoverLeave);
    area.removeEventListener('pointerdown', onPointerDown);
    area.removeEventListener('pointermove', onPointerMove);
    area.removeEventListener('pointerup', onPointerUp);
    area.removeEventListener('pointercancel', onPointerUp);
  };
}
