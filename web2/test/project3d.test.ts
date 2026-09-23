/* 3D plot projection (docs/ui-v2.md T14): the same maths as core/graphics.c
   make_rot/scale3d/rot_3dvec/threed_proj, so a `view3d` sent from the
   client's angles draws what the core would. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {
  KEY_STEP, KEY_STEP_FINE, project, projectedBox, rotate, rotateByDrag, rotateByKey, rotationMatrix, toCube,
  type Box3,
} from '../src/plot/project3d';

const BOX: Box3 = {xmin: -1, xmax: 1, ymin: -1, ymax: 1, zmin: -1, zmax: 1};
const CLOSE = (a: number, b: number, eps = 1e-9) => assert.ok(Math.abs(a - b) < eps, `${a} !~ ${b}`);

test('rotationMatrix: theta=0, phi=0 is the identity (core/graphics.c make_rot)', () => {
  const m = rotationMatrix(0, 0);
  // -cp*st and st*sp land on -0 for st=0: same number, JSON (and deepEqual's
  // SameValue) tells it apart from 0, so compare with plain ===.
  assert.deepEqual(m.map(row => row.map(v => v + 0)), [[1, 0, 0], [0, 1, 0], [0, 0, 1]]);
});

test('rotationMatrix: theta=90 swaps x and y (right-handed)', () => {
  const m = rotationMatrix(90, 0);
  const [x, y, z] = rotate(m, 1, 0, 0);
  CLOSE(x, 0);
  CLOSE(y, -1);
  CLOSE(z, 0);
});

test('toCube: the box\'s own corners map to the unit cube\'s', () => {
  assert.deepEqual(toCube(BOX, -1, -1, -1), [-1, -1, -1]);
  assert.deepEqual(toCube(BOX, 1, 1, 1), [1, 1, 1]);
  assert.deepEqual(toCube(BOX, 0, 0, 0), [0, 0, 0]);
});

test('toCube: a degenerate axis (min === max) does not divide by zero', () => {
  const flat: Box3 = {...BOX, zmin: 5, zmax: 5};
  assert.deepEqual(toCube(flat, 0, 0, 5), [0, 0, 0]);
});

test('toCube: an asymmetric box centres and scales to [-1, 1]', () => {
  const box: Box3 = {xmin: 0, xmax: 10, ymin: -20, ymax: 0, zmin: 0, zmax: 50};
  assert.deepEqual(toCube(box, 5, -10, 25), [0, 0, 0]);
  assert.deepEqual(toCube(box, 0, -20, 0), [-1, -1, -1]);
  assert.deepEqual(toCube(box, 10, 0, 50), [1, 1, 1]);
});

test('project: theta=0, phi=0, no perspective is x, y unchanged (z dropped)', () => {
  const p = project(BOX, 0, 0, 0, -1000, 1000, 0.5, -0.25, 0.9);
  assert.deepEqual(p, {x: 0.5, y: -0.25});
});

test('project: perspective clips a point behind zplane or at/beyond zview', () => {
  // theta=90, phi=90 puts rz where x was (rotationMatrix test above): (1, 0, 0) -> rz = 1
  const behind = project(BOX, 90, 90, 1, /* zplane */ 2, /* zview */ 10, 1, 0, 0);
  assert.equal(behind, null);
  const inFront = project(BOX, 90, 90, 1, /* zplane */ -2, /* zview */ 10, 1, 0, 0);
  assert.notEqual(inFront, null);
});

test('project: perspective scales x, y by (zview - zplane) / (zview - rz)', () => {
  // theta=0, phi=0: rz is just z; z=0 at the box's centre plane
  const p = project(BOX, 0, 0, 1, /* zplane */ -2, /* zview */ 2, 0.5, 0.5, 0);
  const s = (2 - -2) / (2 - 0);
  CLOSE(p!.x, 0.5 * s);
  CLOSE(p!.y, 0.5 * s);
});

test('projectedBox: the 8 corners of a symmetric box, no perspective, project within a bounded frame', () => {
  const pts = projectedBox(BOX, 30, 40, 0, -1000, 1000);
  assert.equal(pts.length, 8);
  for (const p of pts) {
    assert.ok(p !== null);
    assert.ok(Math.abs(p.x) <= Math.sqrt(3) + 1e-9);
    assert.ok(Math.abs(p.y) <= Math.sqrt(3) + 1e-9);
  }
});

test('rotateByDrag: one pixel is one degree, subtracted (core/many_pops.c rotate3dcheck)', () => {
  assert.deepEqual(rotateByDrag(45, 45, 10, -5), {theta: 35, phi: 50});
  assert.deepEqual(rotateByDrag(45, 45, 0, 0), {theta: 45, phi: 45});
});

test('rotateByKey: left/right change theta, up/down change phi, by the fine step with Shift', () => {
  assert.deepEqual(rotateByKey(45, 45, 'ArrowLeft', false), {theta: 45 - KEY_STEP, phi: 45});
  assert.deepEqual(rotateByKey(45, 45, 'ArrowRight', false), {theta: 45 + KEY_STEP, phi: 45});
  assert.deepEqual(rotateByKey(45, 45, 'ArrowUp', false), {theta: 45, phi: 45 + KEY_STEP});
  assert.deepEqual(rotateByKey(45, 45, 'ArrowDown', true), {theta: 45, phi: 45 - KEY_STEP_FINE});
  assert.equal(rotateByKey(45, 45, 'Enter', false), null);
});
