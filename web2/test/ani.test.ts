/* The animation (docs/ui-v2.md T13): decoding a frame's primitives, the
   box at the dimension's aspect on a canvas of any size, the unit-to-canvas
   mapping and its inverse, pixel sizes, and the store's slice (ani/frame.ts,
   store/ani.ts, store/state.ts), without a browser (npm test). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {
  aspectOf, circleRadius, decodePrims, fitBox, fromCanvas, lineWidth, penScale, textOf, textPx, toCanvas,
} from '../src/ani/frame';
import type {AniFrameEvent, AniStateEvent} from '../src/protocol/types';
import {initialAni, reduceAni, stepTarget} from '../src/store/ani';
import {initialState, reduce} from '../src/store/state';

const near = (a: number, b: number, eps = 1e-9) => assert.ok(Math.abs(a - b) <= eps, `${a} vs ${b}`);

/* frame 0 of tools/gui_test.ani on lecar, as the core sends it */
const frameEv = (over: Partial<AniFrameEvent> = {}): AniFrameEvent => ({
  ev: 'ani', op: 'frame', pos: 0, rows: 601, t: 0, speed: 5, skip: 1, dim: [-0.6, -0.1, 0.4, 0.6], w: 280, h: 350,
  prims: [
    ['text', 0.05, 0.928571, 'lecar  ', 9, 3, 0],
    ['line', 0.456, 0.142857, 0.456, 0.185714, 1, 3],
    ['circle', 0.456, 0.185714, 0.03, 0.0428571, 10, 0, 0],
    ['circle', 0.2, 0.185714, 0.02, 0.0285714, '#00009d', 0, 1],
    ['rect', 0.05, 0.285714, 0.1, 0.328571, 0, 1, 0],
    ['ellipse', 0.8, 0.714286, 0.05, 0.0428571, 6, 0, 1],
    ['dot', 0.456, 0.185714, 2, 1],
  ],
  ...over,
});

test('decodePrims reads every kind, in order, with its colour and style', () => {
  const p = decodePrims(frameEv().prims);
  assert.deepEqual(p.map(x => x.kind), ['text', 'line', 'circle', 'circle', 'rect', 'ellipse', 'dot']);
  assert.deepEqual(p[1], {kind: 'line', u1: 0.456, v1: 0.142857, u2: 0.456, v2: 0.185714, color: 1, width: 3});
  assert.deepEqual(p[3], {kind: 'circle', u: 0.2, v: 0.185714, ru: 0.02, rv: 0.0285714, color: '#00009d', width: 0,
    fill: true});
  assert.deepEqual(p[4], {kind: 'rect', u1: 0.05, v1: 0.285714, u2: 0.1, v2: 0.328571, color: 0, width: 1, fill: false});
  assert.deepEqual(p[6], {kind: 'dot', u: 0.456, v: 0.185714, r: 2, color: 1});
  assert.deepEqual(p[0], {kind: 'text', u: 0.05, v: 0.928571, text: 'lecar  ', color: 9, size: 3, font: 0});
});

test('decodePrims leaves out what it cannot draw: NaN (null) coordinates, unknown kinds, bad colours', () => {
  const p = decodePrims([
    ['line', null, 0, 1, 1, 0, 0], ['blob', 0, 0], 'junk', ['dot', 0.5, 0.5, 2, 'red'], ['line', 0, 0, 1, 1, 2, 0],
  ]);
  assert.equal(p.length, 1);
  assert.equal(p[0].kind, 'line');
});

test('unit coordinates outside [0,1] (the .ani leaving its box) are kept', () => {
  const p = decodePrims([['line', -0.2, 0.5, 1.3, 0.5, 0, 0]]);
  assert.equal(p.length, 1);
  assert.deepEqual([p[0].kind === 'line' && p[0].u1, p[0].kind === 'line' && p[0].u2], [-0.2, 1.3]);
});

test('the box keeps the dimension\'s aspect on any canvas, centred', () => {
  near(aspectOf([-0.6, -0.1, 0.4, 0.6]), 1 / 0.7);
  assert.equal(aspectOf([0, 0, 0, 1]), 1); /* a degenerate box: square */
  const wide = fitBox(1000, 400, 2);
  assert.deepEqual(wide, {x: 100, y: 0, w: 800, h: 400});
  const tall = fitBox(300, 900, 0.5);
  assert.deepEqual(tall, {x: 0, y: 150, w: 300, h: 600});
  const m = fitBox(100, 100, 1, 8);
  assert.deepEqual(m, {x: 8, y: 8, w: 84, h: 84});
  for (const [cw, ch] of [[390, 500], [1280, 700], [37, 1000]]) {
    const b = fitBox(cw, ch, 1 / 0.7);
    near(b.w / b.h, 1 / 0.7);
    assert.ok(b.w <= cw + 1e-9 && b.h <= ch + 1e-9);
  }
});

test('a unit point maps to the canvas with v up, and back', () => {
  const box = {x: 10, y: 20, w: 200, h: 100};
  assert.deepEqual(toCanvas(box, 0, 0), [10, 120]);
  assert.deepEqual(toCanvas(box, 1, 1), [210, 20]);
  assert.deepEqual(toCanvas(box, 0.25, 0.5), [60, 70]);
  const [u, v] = fromCanvas(box, ...toCanvas(box, 0.3, 0.8));
  near(u, 0.3);
  near(v, 0.8);
});

test('at the classic window\'s own size the mapping is the core\'s pixels (within one)', () => {
  /* core: ix = (int)(u*W), iy = (int)(H - v*H) for the 280 x 350 window */
  const box = {x: 0, y: 0, w: 280, h: 350};
  for (const [u, v] of [[0.456, 0.142857], [0.05, 0.928571], [0.999, 0.001]]) {
    const [x, y] = toCanvas(box, u, v);
    assert.ok(Math.abs(x - Math.trunc(u * 280)) <= 1 && Math.abs(y - Math.trunc(350 - v * 350)) <= 1);
  }
});

test('a circle is round at the dimension\'s aspect: its radius along u and v agree on the canvas', () => {
  const dim = [-0.6, -0.1, 0.4, 0.6], r = 0.03;
  const box = fitBox(640, 480, aspectOf(dim));
  const ru = r / (dim[2] - dim[0]), rv = r / (dim[3] - dim[1]);
  near(ru * box.w, rv * box.h, 1e-9);
  near(circleRadius(box, ru, rv), ru * box.w, 1e-9);
});

test('pixel sizes follow the picture\'s area, within a half and four times; width 0 is one pixel', () => {
  assert.equal(penScale({x: 0, y: 0, w: 280, h: 350}, 280, 350), 1);
  near(penScale({x: 0, y: 0, w: 560, h: 700}, 280, 350), 2);
  assert.equal(penScale({x: 0, y: 0, w: 28, h: 35}, 280, 350), 0.5);
  assert.equal(penScale({x: 0, y: 0, w: 5600, h: 7000}, 280, 350), 4);
  assert.equal(penScale({x: 0, y: 0, w: 100, h: 100}, 0, 0), 1);
  assert.equal(lineWidth(0, 1), 1);
  assert.equal(lineWidth(3, 2), 6);
  assert.equal(textPx(3, 1), 14);
  assert.equal(textPx(9, 1), 18);
});

test('text drops the .ani\'s trailing blanks; the symbol font is Greek', () => {
  assert.equal(textOf({text: 'lecar  ', font: 0}), 'lecar');
  assert.equal(textOf({text: 'abq=', font: 1}), 'αβθ=');
});

test('the slice: window 104 opens the panel, state and frames update it', () => {
  let s = reduceAni(initialAni, {type: 'window', exists: true});
  assert.ok(s.exists && s.open);
  const st: AniStateEvent = {ev: 'ani', pos: 3, rows: 601, fly: 0, grab: 1, skip: 1, speed: 20, loaded: 1, open: 1};
  s = reduceAni(s, {type: 'state', ev: st});
  assert.deepEqual([s.pos, s.rows, s.speed, s.grab, s.loaded], [3, 601, 20, true, true]);
  s = reduceAni(s, {type: 'frame', ev: frameEv({pos: 7})});
  assert.equal(s.frame!.pos, 7);
  assert.equal(s.frame!.prims.length, 7);
  assert.equal(s.frames, 1);
  s = reduceAni(s, {type: 'window', exists: false});
  assert.ok(!s.exists && s.open, 'a closed window leaves the panel to the user');
});

test('a step goes from the frame shown, within the stored rows', () => {
  const s = reduceAni({...initialAni, rows: 601, pos: 0}, {type: 'frame', ev: frameEv({pos: 600})});
  assert.equal(stepTarget(s, -1), 599);
  assert.equal(stepTarget(s, 1), 600);
  assert.equal(stepTarget({...s, frame: null, pos: 5}, -10), 0);
});

test('app state: ani events and window 104 reach the slice; Go plays until its idle', () => {
  let s = reduce(initialState, {type: 'event', ev: {ev: 'window', op: 'create', win: 104, w: 280, h: 350}});
  assert.ok(s.ani.exists && s.ani.open);
  assert.equal(s.plots, initialState.plots, 'not a plot window');
  s = reduce(s, {type: 'event', ev: frameEv({pos: 12})});
  assert.equal(s.ani.frame!.pos, 12);
  s = reduce(s, {type: 'sent', cmd: {cmd: 'ani', op: 'go'}});
  assert.ok(s.ani.playing && s.busy);
  s = reduce(s, {type: 'event', ev: {ev: 'ani', pos: 13, rows: 601, fly: 0, grab: 0, skip: 1, speed: 5}});
  assert.equal(s.ani.pos, 13);
  s = reduce(s, {type: 'event', ev: {ev: 'idle'}});
  assert.ok(!s.ani.playing && !s.busy);
  s = reduce(s, {type: 'event', ev: {ev: 'window', op: 'destroy', win: 104, w: 0, h: 0}});
  assert.ok(!s.ani.exists);
});
