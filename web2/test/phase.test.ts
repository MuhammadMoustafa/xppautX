/* Nullclines, direction field and flows (docs/ui-v2.md T7): the events in
   the store, the legend's layers, and their geometry on the canvas. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import type {PixelFrame} from '../src/plot/decimate';
import {arrowSegments, phaseLayers, tracePolyline, traceSegments, type PathSink} from '../src/plot/phase';
import type {DfieldEvent, NullclinesEvent} from '../src/protocol/types';
import {dfieldFromEvent, nullclinesFromEvent, trajectoryCount, type Dfield} from '../src/store/phase';
import {windowOf} from '../src/store/plots';
import {initialState, reduce, type AppState} from '../src/store/state';

const ev = (s: AppState, e: object) => reduce(s, {type: 'event', ev: e as never});

const f32 = (v: number[]) => Buffer.from(new Float32Array(v).buffer).toString('base64');

const NC: NullclinesEvent = {
  ev: 'nullclines', win: 1, xname: 'V', yname: 'W', xcolor: 2, ycolor: 7,
  x: [0, 0, 1, 1, 1, 1, 2, 0], y: [0, 1, 1, 0], frozen: [{x: [0, 0, 0, 1], y: []}],
};

const DF: DfieldEvent = {
  ev: 'dfield', win: 1, scaled: 1, color: 0, n: 2, du: 1, dv: 1,
  grid: [0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, -1, 0], speed: [2, 1, 0, 4],
  flows: [{color: 3, x: [0, 1, null, 2, 3, 4], y: [0, 1, null, 2, 3, 4]}],
};

/** a unit square of plot coordinates on a 100 x 100 canvas area at (10, 20) */
const FRAME: PixelFrame = {xmin: 0, xmax: 1, ymin: 0, ymax: 1, left: 10, top: 20, width: 100, height: 100};

function recorder(): PathSink & {ops: [string, number, number][]} {
  const ops: [string, number, number][] = [];
  return {ops, moveTo: (x, y) => ops.push(['M', x, y]), lineTo: (x, y) => ops.push(['L', x, y])};
}

test('nullclines: segments decoded from JSON or base64 float32 alike', () => {
  const a = nullclinesFromEvent(NC);
  const b = nullclinesFromEvent({...NC, enc: 'f32', x: f32([0, 0, 1, 1, 1, 1, 2, 0]), y: f32([0, 1, 1, 0])});
  assert.deepEqual(Array.from(a.x), Array.from(b.x));
  assert.deepEqual(Array.from(a.y), Array.from(b.y));
  assert.equal(a.frozen.length, 1);
  assert.equal(a.xName, 'V');
});

test('the store keeps each window its own nullclines and field', () => {
  let s = ev(initialState, NC);
  s = ev(s, {...DF, win: 2});
  assert.equal(windowOf(s.plots, 1)?.nullclines?.x.length, 8);
  assert.equal(windowOf(s.plots, 1)?.dfield, null);
  assert.equal(windowOf(s.plots, 2)?.dfield?.grid.length, 16);
  s = ev(s, {...NC, x: [], y: [], frozen: []});
  assert.equal(windowOf(s.plots, 1)?.nullclines?.x.length, 0, 'an empty event clears them');
});

test('layers: named for the legend, empty ones left out', () => {
  const nc = nullclinesFromEvent(NC), df = dfieldFromEvent(DF);
  assert.deepEqual(phaseLayers(nc, df).map(l => [l.key, l.label, l.color, l.count]), [
    ['xnull', 'V-nullcline', 2, 3], /* two segments and a frozen one */
    ['ynull', 'W-nullcline', 7, 1],
    ['dfield', 'Direction field', 0, 4],
    ['flow', 'Flow', 3, 2],
  ]);
  assert.deepEqual(phaseLayers(nullclinesFromEvent({...NC, x: [], y: [], frozen: []}),
    dfieldFromEvent({...DF, n: 0, grid: [], speed: [], flows: []})), []);
  assert.deepEqual(phaseLayers(null, null), []);
});

test('segments map to the canvas, y upwards', () => {
  const r = recorder();
  assert.equal(traceSegments(new Float32Array([0, 0, 1, 1]), FRAME, r), 1);
  assert.deepEqual(r.ops, [['M', 10, 120], ['L', 110, 20]]);
});

test('a flow breaks at NaN: each trajectory starts with a move', () => {
  const df = dfieldFromEvent(DF), r = recorder();
  assert.equal(trajectoryCount(df.flows[0].xs), 2);
  assert.equal(tracePolyline(df.flows[0].xs, df.flows[0].ys, FRAME, r), 5);
  assert.deepEqual(r.ops.map(o => o[0]).join(''), 'MLMLL');
});

const field = (over: Partial<Dfield>): Dfield => ({n: 2, du: 0.5, dv: 0.5, scaled: true, color: 0,
  grid: new Float32Array(0), speed: new Float32Array(0), flows: [], ...over});

test('scaled arrows: from the grid point, along the direction on screen, one length', () => {
  const df = field({grid: new Float32Array([0.5, 0.5, 1, 0, 0.5, 0.5, 0, 1]), speed: new Float32Array([1, 5])});
  const {segments: s, arrows} = arrowSegments(df, FRAME, 6);
  assert.equal(arrows, 2);
  assert.equal(s.length, 24, 'a shaft and two head strokes each');
  /* a cell is 50 px: the shaft is 0.6 of it */
  assert.deepEqual(Array.from(s.subarray(0, 4)), [60, 70, 90, 70], '+x goes right');
  assert.deepEqual(Array.from(s.subarray(12, 16)), [60, 70, 60, 40], '+y goes up');
  /* the head's strokes start at the tip and go back */
  assert.equal(s[4], 90);
  assert.ok(s[6] < 90 && Math.abs(Math.hypot(s[6] - 90, s[7] - 70) - 6) < 1e-4);
});

test('a direction follows the axes\' scales before it is normalized', () => {
  /* x spans 2 units and y 1 over a square: 45 degrees in plot units is steeper on screen */
  const f = {...FRAME, xmax: 2};
  const df = field({du: 1, dv: 0.5, grid: new Float32Array([0, 0, Math.SQRT1_2, Math.SQRT1_2]), speed: new Float32Array([1])});
  const [x0, y0, x1, y1] = arrowSegments(df, f, 6).segments;
  const want = Math.atan2(-100, 50); /* screen: 50 px per x unit, 100 per y unit, y down */
  assert.ok(Math.abs(Math.atan2(y1 - y0, x1 - x0) - want) < 1e-6);
});

test('arrows by speed: the fastest 0.9 of a cell, the others in proportion, none where the field is 0', () => {
  const df = field({scaled: false, grid: new Float32Array([0, 0, 1, 0, 0, 0.5, 1, 0, 1, 1, 0, 0]),
    speed: new Float32Array([2, 1, 0])});
  const {segments: s, arrows} = arrowSegments(df, FRAME, 6);
  assert.equal(arrows, 2);
  assert.ok(Math.abs(s[2] - s[0] - 45) < 1e-4);
  assert.ok(Math.abs(s[14] - s[12] - 22.5) < 1e-4);
});
