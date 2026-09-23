/* Marks on the plot (docs/ui-v2.md T8): the event in the store, XPP's text
   markup as Unicode runs (its symbol font's Greek), the legend's layers,
   and the marks' geometry on the canvas. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import type {PixelFrame} from '../src/plot/decimate';
import {arrowPath, equilibriumPath, markerPath, markLayers, textPx, traceFrozen} from '../src/plot/marks';
import type {PathSink} from '../src/plot/phase';
import {parseRichText, plainText, symbolToUnicode} from '../src/plot/richtext';
import type {MarksEvent} from '../src/protocol/types';
import {markCount, marksFromEvent} from '../src/store/marks';
import {windowOf} from '../src/store/plots';
import {initialState, reduce, type AppState} from '../src/store/state';

const ev = (s: AppState, e: object) => reduce(s, {type: 'event', ev: e as never});
const f32 = (v: number[]) => Buffer.from(new Float32Array(v).buffer).toString('base64');

const MARKS: MarksEvent = {
  ev: 'marks', win: 1,
  equilibria: [{x: -0.14, y: 0.03, type: 'saddle', symbol: 'triangle'}, {x: 0.3, y: 0.2, type: 'stable', symbol: 'circle'}],
  text: [{x: 0.1, y: 0.5, text: '\\1a\\0-point', size: 3, font: 0}],
  arrows: [{kind: 'pointer', x1: 0, y1: 0, x2: 1, y2: 0, size: 0.2, color: 5}],
  markers: [{x: 0.5, y: 0.5, shape: 'diamond', size: 2, color: 7}],
  frozen: [{key: 'first run', name: 'frz1', color: 4, line: 1, x: [0, 0.5, 1], y: [0, 1, null]},
    {key: '', name: 'crvb', color: 2, line: 0, x: [], y: []}],
};

/** a unit square of plot coordinates on a 100 x 100 canvas area at (10, 20) */
const FRAME: PixelFrame = {xmin: 0, xmax: 1, ymin: 0, ymax: 1, left: 10, top: 20, width: 100, height: 100};

function recorder(): PathSink & {ops: [string, number, number][]} {
  const ops: [string, number, number][] = [];
  return {ops, moveTo: (x, y) => ops.push(['M', x, y]), lineTo: (x, y) => ops.push(['L', x, y])};
}

const near = (a: number, b: number) => Math.abs(a - b) < 1e-9;

test('the symbol font\'s Latin letters are Greek ones', () => {
  assert.equal(symbolToUnicode('abgdeqlmpstw'), 'αβγδεθλμπστω');
  assert.equal(symbolToUnicode('DGLPSW'), 'ΔΓΛΠΣΩ');
  assert.equal(symbolToUnicode('f j J V'), 'φ ϕ ϑ ς', 'the variant forms where the Symbol font has them');
  assert.equal(symbolToUnicode('a = 2.5!'), 'α = 2.5!', 'digits and punctuation stay');
});

test('XPP\'s markup: \\1 Greek, \\0 roman, \\s \\S shifts that add up, \\n back to normal', () => {
  assert.deepEqual(parseRichText('\\1a\\0-point'), [{text: 'α', rise: 0, small: false}, {text: '-point', rise: 0, small: false}]);
  assert.deepEqual(parseRichText('V\\s\\1q\\n = 2'), [
    {text: 'V', rise: 0, small: false}, {text: 'θ', rise: -0.5, small: true}, {text: ' = 2', rise: 0, small: false}]);
  assert.deepEqual(parseRichText('x\\S2\\S3').map(r => r.rise), [0, 1, 2]);
  assert.equal(plainText('\\1D\\0t\\x'), 'Δt', 'an unknown escape is dropped, as XPP drops it');
  assert.equal(plainText('ends in \\'), 'ends in ');
  assert.equal(plainText('ab', true), 'αβ', 'a label in the symbol font (font 1)');
  assert.deepEqual(parseRichText(''), []);
});

test('marks decoded: stability, text runs, arrows, markers, frozen curves from JSON or f32 alike', () => {
  const m = marksFromEvent(MARKS);
  assert.deepEqual(m.equilibria.map(e => e.type), ['saddle', 'stable']);
  assert.equal(m.text[0].plain, 'α-point');
  assert.equal(m.text[0].raw, '\\1a\\0-point');
  assert.equal(m.arrows[0].pointer, true);
  assert.equal(m.markers[0].shape, 'diamond');
  assert.equal(m.frozen[0].label, 'first run');
  assert.equal(m.frozen[1].label, 'crvb', 'no key: the name');
  assert.equal(m.frozen[0].line, true);
  assert.equal(m.frozen[1].line, false);
  assert.ok(Number.isNaN(m.frozen[0].ys[2]));
  const b = marksFromEvent({...MARKS, enc: 'f32', frozen: [{...MARKS.frozen[0], x: f32([0, 0.5, 1]), y: f32([0, 1, NaN])}]});
  assert.deepEqual(Array.from(b.frozen[0].xs), Array.from(m.frozen[0].xs));
  assert.equal(markCount(m), 2 + 1 + 1 + 1 + 2);
  assert.equal(markCount(null), 0);
  const odd = marksFromEvent({...MARKS, equilibria: [{x: 0, y: 0, type: 'what', symbol: '?'}],
    markers: [{x: 0, y: 0, shape: 'star', size: 1, color: 0}]});
  assert.equal(odd.equilibria[0].type, 'unstable');
  assert.equal(odd.markers[0].shape, 'box');
});

test('the store keeps each window its marks; an empty event clears them', () => {
  let s = ev(initialState, MARKS);
  s = ev(s, {...MARKS, win: 2, equilibria: []});
  assert.equal(windowOf(s.plots, 1)?.marks?.equilibria.length, 2);
  assert.equal(windowOf(s.plots, 2)?.marks?.equilibria.length, 0);
  s = ev(s, {ev: 'marks', win: 1, equilibria: [], text: [], arrows: [], markers: [], frozen: []});
  assert.equal(markCount(windowOf(s.plots, 1)?.marks ?? null), 0);
});

test('layers: one per kind, named for the legend, and one per frozen curve by its key', () => {
  assert.deepEqual(markLayers(marksFromEvent(MARKS)).map(l => [l.key, l.label, l.color, l.count]), [
    ['equilibria', 'Equilibria', 0, 2],
    ['text', 'Text', 0, 1],
    ['arrows', 'Arrows', 5, 1],
    ['markers', 'Markers', 7, 1],
    ['frozen-0', 'first run', 4, 3],
    ['frozen-1', 'crvb', 2, 0],
  ]);
  assert.deepEqual(markLayers(marksFromEvent({...MARKS, equilibria: [], text: [], arrows: [], markers: [], frozen: []})), []);
  assert.deepEqual(markLayers(null), []);
});

test('an arrow\'s head has its tip at the first point, a pointer a shaft as well', () => {
  const [a] = marksFromEvent(MARKS).arrows;
  let r = recorder();
  assert.ok(arrowPath(a, FRAME, r));
  /* (0,0) to (1,0) is (10,120) to (110,120); size 0.2: the head's strokes from (30, 120 -+ 10) */
  assert.deepEqual(r.ops, [['M', 10, 120], ['L', 110, 120], ['M', 30, 130], ['L', 10, 120], ['L', 30, 110]]);
  r = recorder();
  arrowPath({...a, pointer: false}, FRAME, r);
  assert.deepEqual(r.ops.map(o => o[0]).join(''), 'MLL', 'an arrow is only its head');
  assert.equal(arrowPath({...a, x2: NaN}, FRAME, recorder()), false);
});

test('markers: XPP\'s shapes about their point, scaled by their size', () => {
  const [k] = marksFromEvent(MARKS).markers;
  const r = recorder();
  assert.ok(markerPath({...k, shape: 'box'}, FRAME, 4, r));
  /* size 2, 4 px at size 1: 8 px from the centre (60, 70) */
  assert.deepEqual(r.ops[0], ['M', 52, 78]);
  assert.deepEqual(r.ops[2], ['L', 68, 62]);
  const plus = recorder();
  markerPath({...k, shape: 'plus'}, FRAME, 4, plus);
  assert.equal(plus.ops.filter(o => o[0] === 'M').length, 2, 'two strokes');
  for (const shape of ['diamond', 'triangle', 'cross', 'circle'] as const) {
    const p = recorder();
    assert.ok(markerPath({...k, shape}, FRAME, 4, p));
    assert.ok(p.ops.every(([, x, y]) => Math.hypot(x - 60, y - 70) <= 6 * Math.SQRT2 * 8 / 6 + 1e-9), shape);
  }
});

test('equilibria: a circle stable, a box unstable, a triangle a saddle, about their point', () => {
  const at = {x: 0.5, y: 0.5};
  const corners = (type: 'stable' | 'unstable' | 'saddle') => {
    const r = recorder();
    assert.ok(equilibriumPath({...at, type}, FRAME, 5, r));
    return r.ops;
  };
  assert.equal(corners('stable').length, 25);
  assert.equal(corners('unstable').length, 5);
  assert.equal(corners('saddle').length, 4);
  assert.ok(near(corners('saddle')[0][1], 60) && near(corners('saddle')[0][2], 70 - 7), 'the triangle points up');
  assert.equal(equilibriumPath({x: Infinity, y: 0, type: 'stable'}, FRAME, 5, recorder()), false);
});

test('a frozen curve: a line through its points, breaking at NaN; dots for points', () => {
  const r = recorder();
  const n = traceFrozen(new Float32Array([0, 0.5, 0.5, 1]), new Float32Array([0, 1, NaN, 0]), true, FRAME, 1.5, r);
  assert.equal(n, 3);
  assert.deepEqual(r.ops.map(o => o[0]).join(''), 'MLM');
  const same = recorder();
  assert.equal(traceFrozen(new Float32Array([0, 0.001, 0.002, 1]), new Float32Array([0, 0, 0, 0]), true, FRAME, 1.5, same), 2,
    'points on the pixel of the one before are left out, the last one kept');
  const dots = recorder();
  assert.equal(traceFrozen(new Float32Array([0, 1]), new Float32Array([0, 1]), false, FRAME, 1.5, dots), 2);
  assert.equal(dots.ops.filter(o => o[0] === 'M').length, 2);
});

test('text sizes 0-4 in pixels, out of range clamped', () => {
  assert.deepEqual([0, 1, 2, 3, 4].map(textPx), [10, 12, 14, 18, 24]);
  assert.equal(textPx(9), 24);
  assert.equal(textPx(-1), 10);
});
