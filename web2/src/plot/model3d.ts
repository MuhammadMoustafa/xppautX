/* From a 3D window's series to what it draws: each curve's points
   projected with the window's box and angles (project3d.ts), and the box's
   own wireframe. Pure, mirrors plot/model.ts. */
import type {Box3, Point2} from './project3d';
import {project, projectedBox} from './project3d';
import {curveLabel, seriesColumnName, type PlotSeries} from '../store/series';

export interface Curve3D {
  label: string;
  color: number;
  cssColor?: string;
  visible?: boolean;
  highlighted?: boolean;
  /** a line, or points of this radius */
  line: boolean;
  radius: number;
  /** projected points, row by row; null where perspective clips a row (a gap in the line) */
  points: (Point2 | null)[];
  /** storage row of points[0] */
  row0: number;
}

export interface Model3D {
  curves: Curve3D[];
  /** the box's 8 corners projected, `project3d.BOX_CORNERS` order (null where clipped) */
  box: (Point2 | null)[];
  /** the axis titles: the window's own (Viewaxes), else the first curve's column name */
  labels: {x: string; y: string; z: string};
}

const EMPTY = new Float32Array(0);

export function buildModel3d(
  s: PlotSeries, box: Box3, theta: number, phi: number, persp: number, zplane: number, zview: number,
): Model3D {
  const [xsh, ysh, zsh] = s.shift;
  const start = Math.max(xsh, ysh, zsh, 0);
  const curves: Curve3D[] = s.curves.map(c => {
    const x = s.columns.get(c.x) ?? EMPTY, y = s.columns.get(c.y) ?? EMPTY, z = s.columns.get(c.z) ?? EMPTY;
    const n = Math.max(0, Math.min(x.length, y.length, z.length) - start);
    const points: (Point2 | null)[] = new Array(n);
    for (let i = 0; i < n; i++)
      points[i] = project(box, theta, phi, persp, zplane, zview,
        x[start - xsh + i], y[start - ysh + i], z[start - zsh + i]);
    return {
      label: curveLabel(s, c), color: c.color, line: c.line > 0, radius: c.line > 0 ? 0 : Math.max(1, -c.line),
      points, row0: start,
    };
  });
  const first = s.curves[0];
  const name = (own: string, col: number | undefined) => own || (col === undefined ? '' : seriesColumnName(s, col));
  return {curves, box: projectedBox(box, theta, phi, persp, zplane, zview),
    labels: {x: name(s.labels.x, first?.x), y: name(s.labels.y, first?.y), z: name(s.labels.z, first?.z)}};
}
