/* A 3D plot window's chart (docs/ui-v2.md T14): owns the canvas, redraws
   it from a Model3D and the angles it is shown at, and remembers the last
   drawing for the test hook (registry.ts, testhook.ts), the same shape
   PlotView.tsx already uses for 2D plots (plot/chart.ts) so both register
   the same way. */
import {draw3d, type Draw3DInfo} from './render3d';
import {canvasPixels} from './canvasPixels';
import type {Model3D} from './model3d';

export class Chart3D {
  private canvas: HTMLCanvasElement;
  private model: Model3D | null = null;
  private theta = 0;
  private phi = 0;
  private dark = false;
  private axisColor = '#888';
  private lastInfo: Draw3DInfo | null = null;

  constructor(canvas: HTMLCanvasElement) {
    this.canvas = canvas;
  }

  /** the model to show, and the angles it was projected at (for the info the test hook reads) */
  set(model: Model3D | null, theta: number, phi: number, dark: boolean, axisColor: string): void {
    this.model = model;
    this.theta = theta;
    this.phi = phi;
    this.dark = dark;
    this.axisColor = axisColor;
    this.redraw();
  }

  /** the canvas's CSS size changed: redraw at it (a hidden tab draws nothing until shown again) */
  resize(): void {
    this.redraw();
  }

  private redraw(): void {
    const w = this.canvas.clientWidth, h = this.canvas.clientHeight;
    if (!w || !h) return;
    this.lastInfo = draw3d(this.canvas, w, h, this.model, this.theta, this.phi, this.dark, this.axisColor);
  }

  /** the picture as drawn now, for the core's `pixels` ask (kinescope, GIF) */
  pixels(): {w: number; h: number; rgb: Uint8ClampedArray} | null {
    return canvasPixels(this.canvas);
  }

  info(): Draw3DInfo | null {
    return this.lastInfo;
  }

  destroy(): void {
    this.model = null;
  }
}
