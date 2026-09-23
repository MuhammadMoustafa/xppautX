/* Rendering a kinescope frame off screen, from data (docs/ui-v2.md T15):
   the same Chart (chart.ts) and model builder the live plot uses, drawn on
   a canvas that is never shown, so `pixels` asks and the GIF export get
   real pixels without a screenshot of the DOM. One chart is reused for
   every frame, so every rendered picture is the same size (what the core's
   AniGif writer requires: "All clips must be same size"). */
import type {KinescopeFrame} from '../store/kinescope';
import {buildModel} from './model';
import {Chart} from './chart';
import type {Ranges} from './viewmath';

/** CSS pixels of the offscreen canvas: plain enough to read, small enough
    that a GIF of a run's frames stays a reasonable download */
export const RENDER_W = 480;
export const RENDER_H = 360;

let host: HTMLDivElement | null = null;
let chart: Chart | null = null;

function offscreenChart(): Chart {
  if (chart) return chart;
  host = document.createElement('div');
  host.style.cssText = `position:fixed; left:-10000px; top:0; width:${RENDER_W}px; height:${RENDER_H}px; pointer-events:none;`;
  document.body.appendChild(host);
  chart = new Chart(host, {onViewport: () => {}});
  return chart;
}

/** the frame's own axes (its `info`, like ui/PlotView.tsx's coreView, but
    from a stored snapshot instead of the live store) */
function frameView(frame: KinescopeFrame): Ranges | null {
  const a = frame.info;
  if (!a || a.three || !(a.xlo < a.xhi && a.ylo < a.yhi)) return null;
  return {x: {min: a.xlo, max: a.xhi}, y: {min: a.ylo, max: a.yhi}};
}

/** frame's picture, RGB pixels, at RENDER_W x RENDER_H (device pixels may
    scale it further, as chart.ts's own canvas does) */
export function renderFrame(frame: KinescopeFrame, dark = false): {w: number; h: number; rgb: Uint8ClampedArray} | null {
  const model = frame.series ? buildModel(frame.series) : null;
  if (!model) return null;
  const c = offscreenChart();
  c.set(model, frameView(frame), frame.viewport, dark);
  c.setPhase(frame.nullclines, frame.dfield);
  c.setMarks(frame.marks);
  return c.pixels();
}

/** frees the offscreen chart (tests only: chart.ts owns no other global state) */
export function resetOffscreenChart(): void {
  chart?.destroy();
  chart = null;
  host?.remove();
  host = null;
}
