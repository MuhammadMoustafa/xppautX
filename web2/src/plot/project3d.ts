/* XPP's 3D plots (docs/ui-v2.md T14): curves in a box, seen from angles
   theta and phi, with an optional perspective. The core draws this with
   graphics.c's make_rot/scale3d/rot_3dvec/threed_proj; this module is the
   same maths in the client, in the same order, so a projection and a
   `view3d` sent from the same angles agree pixel for pixel. Pure: no DOM. */

export interface Box3 {
  xmin: number; xmax: number;
  ymin: number; ymax: number;
  zmin: number; zmax: number;
}

export interface Point2 {
  x: number;
  y: number;
}

const DEG = Math.PI / 180;

/** the box's local axes as seen from angles theta, phi (degrees):
    core/graphics.c make_rot. */
export function rotationMatrix(theta: number, phi: number): number[][] {
  const ct = Math.cos(theta * DEG), st = Math.sin(theta * DEG);
  const sp = Math.sin(phi * DEG), cp = Math.cos(phi * DEG);
  return [
    [ct, st, 0],
    [-cp * st, cp * ct, sp],
    [st * sp, -sp * ct, cp],
  ];
}

/** a data point mapped into the box's unit cube [-1, 1]^3: core/graphics.c
    scale3d (a degenerate axis, min === max, maps to its cube's centre). */
export function toCube(box: Box3, x: number, y: number, z: number): [number, number, number] {
  const dx = box.xmax > box.xmin ? 2 / (box.xmax - box.xmin) : 0;
  const dy = box.ymax > box.ymin ? 2 / (box.ymax - box.ymin) : 0;
  const dz = box.zmax > box.zmin ? 2 / (box.zmax - box.zmin) : 0;
  return [
    (x - (box.xmin + box.xmax) / 2) * dx,
    (y - (box.ymin + box.ymax) / 2) * dy,
    (z - (box.zmin + box.zmax) / 2) * dz,
  ];
}

/** `m` applied to a cube point: core/graphics.c rot_3dvec. */
export function rotate(m: number[][], x: number, y: number, z: number): [number, number, number] {
  return [
    m[0][0] * x + m[0][1] * y + m[0][2] * z,
    m[1][0] * x + m[1][1] * y + m[1][2] * z,
    m[2][0] * x + m[2][1] * y + m[2][2] * z,
  ];
}

/** a data point (x, y, z) projected to the plane at angles theta, phi:
    core/graphics.c threed_proj/threedproj. With perspective on (`persp`),
    null means the point falls outside `zplane`..`zview` (behind the
    viewer or past the clip plane) and is left out, same as the core. */
export function project(
  box: Box3, theta: number, phi: number, persp: number, zplane: number, zview: number,
  x: number, y: number, z: number,
): Point2 | null {
  const [cx, cy, cz] = toCube(box, x, y, z);
  const [rx, ry, rz] = rotate(rotationMatrix(theta, phi), cx, cy, cz);
  if (!persp) return {x: rx, y: ry};
  if (rz >= zview || rz < zplane) return null;
  const s = (zview - zplane) / (zview - rz);
  return {x: s * rx, y: s * ry};
}

/** the corner order `projectedBox` and `cubeEdges` share: x, then y, then z
    each low then high, so corner `i`'s bits are (xi, yi, zi). */
export const BOX_CORNERS: [number, number, number][] = (() => {
  const c: [number, number, number][] = [];
  for (const xi of [0, 1]) for (const yi of [0, 1]) for (const zi of [0, 1]) c.push([xi, yi, zi]);
  return c;
})();

/** the box's 8 corners projected, in `BOX_CORNERS` order (null where
    perspective clips one), to draw the wireframe and fit a view around it.
    There is no server-side equivalent to match: axes2.c's Frame_3d
    recomputes tic-aligned bounds for its own redraw; this only has to be a
    sensible, self-consistent frame for whatever angles the client has
    right now, including mid-drag ones the core has not seen yet. */
export function projectedBox(
  box: Box3, theta: number, phi: number, persp: number, zplane: number, zview: number,
): (Point2 | null)[] {
  return BOX_CORNERS.map(([xi, yi, zi]) => project(
    box, theta, phi, persp, zplane, zview,
    xi ? box.xmax : box.xmin, yi ? box.ymax : box.ymin, zi ? box.zmax : box.zmin,
  ));
}

/** the wireframe's 12 edges, as pairs of `BOX_CORNERS` indices (every pair
    one bit apart: a cube's edges) */
export const BOX_EDGES: [number, number][] = (() => {
  const e: [number, number][] = [];
  for (let i = 0; i < 8; i++)
    for (let bit = 0; bit < 3; bit++) {
      const j = i ^ (1 << bit);
      if (j > i) e.push([i, j]);
    }
  return e;
})();

/** turning by a mouse drag of (dxPixels, dyPixels) from the angles the
    drag started at: core/many_pops.c rotate3dcheck and core/ui_json.cpp
    rotate_command both use raw pixels, one degree each, so this stays in
    step with what `rotate` would have produced (docs/protocol.md). */
export function rotateByDrag(theta0: number, phi0: number, dxPixels: number, dyPixels: number): {theta: number; phi: number} {
  return {theta: theta0 - dxPixels, phi: phi0 - dyPixels};
}

/** the arrow keys' step, degrees; Shift for the coarse step */
export const KEY_STEP = 5;
export const KEY_STEP_FINE = 30;

/** turning by an arrow key (a plot with the focus, docs/ui-v2.md T14):
    left/right change theta, up/down change phi; null for any other key. */
export function rotateByKey(theta: number, phi: number, key: string, coarse: boolean): {theta: number; phi: number} | null {
  const step = coarse ? KEY_STEP_FINE : KEY_STEP;
  switch (key) {
    case 'ArrowLeft': return {theta: theta - step, phi};
    case 'ArrowRight': return {theta: theta + step, phi};
    case 'ArrowUp': return {theta, phi: phi + step};
    case 'ArrowDown': return {theta, phi: phi - step};
    default: return null;
  }
}
