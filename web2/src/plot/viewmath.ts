/* Zooming and panning a plot's ranges. Pure. */
import type {Range} from '../store/state';

export interface Ranges {
  x: Range;
  y: Range;
}

/** zooms by `factor` (< 1 in) keeping the data point at fractions (fx, fy) of the area where it is;
    fy counts from the top, as screen coordinates do */
export function zoomAbout(r: Ranges, fx: number, fy: number, factor: number): Ranges {
  const cx = r.x.min + (r.x.max - r.x.min) * fx, cy = r.y.max - (r.y.max - r.y.min) * fy;
  return {
    x: {min: cx - (cx - r.x.min) * factor, max: cx + (r.x.max - cx) * factor},
    y: {min: cy - (cy - r.y.min) * factor, max: cy + (r.y.max - cy) * factor},
  };
}

/** moves the view by fractions of its size: content follows a drag of (dx, dy) screen fractions */
export function panBy(r: Ranges, dx: number, dy: number): Ranges {
  const w = r.x.max - r.x.min, h = r.y.max - r.y.min;
  return {x: {min: r.x.min - dx * w, max: r.x.max - dx * w}, y: {min: r.y.min + dy * h, max: r.y.max + dy * h}};
}
