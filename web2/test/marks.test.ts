/* Marks on the plot (docs/ui-v2.md T8): the event in the store, XPP's text
   markup as Unicode runs (its symbol font's Greek), the legend's layers,
   and the marks' geometry on the canvas. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import type {PixelFrame} from '../src/plot/decimate';
import {arrowPath, equilibriumPath, markerPath, markLayers, textPx, traceCurve} from '../src/plot/marks';
import {dashPattern} from '../src/plot/linestyle';
import type {PathSink} from '../src/plot/phase';
import {parseRichText, plainText, symbolToUnicode} from '../src/plot/richtext';
import type {MarksEvent} from '../src/protocol/types';
import {markCount, marksFromEvent} from '../src/store/marks';
import {windowOf} from '../src/store/plots';
import {reduce, type AppState} from '../src/store/state';
import {LINE, READY} from './hello';

const ev = (s: AppState, e: object) => reduce(s, {type: 'event', ev: e as never});
const f32 = (v: number[]) => Buffer.from(new Float32Array(v).buffer).toString('base64');

const MARKS: MarksEvent = {
  ev: 'plot.marks', win: 'graph_1',
  equilibria: [{x: -0.14, y: 0.03, type: 'saddle', symbol: 'triangle'}, {x: 0.3, y: 0.2, type: 'stable', symbol: 'circle'}],
  text: [{id: 4, x: 0.1, y: 0.5, text: '\\1a\\0-point', source: '\\1a\\0-point', size: 3, font: 0, style: 'bold', color: '#c8c800'}],
  arrows: [{id: 5, kind: 'pointer', x1: 0, y1: 0, x2: 1, y2: 0, size: 0.2, color: '#c8c800'}],
  markers: [{id: 6, x: 0.5, y: 0.5, shape: 'diamond', size: 2, color: '#00e100'}],
  frozen: [{slot: 0, key: 'first run', name: 'frz1', color: '#ffcd00', ...LINE, x: [0, 0.5, 1], y: [0, 1, null]},
    {slot: 1, key: '', name: 'crvb', color: '#f06400b0', ...LINE, draw: 'points', marker: 'square', marker_size: 2, x: [], y: []}],
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
  assert.deepEqual([m.text[0].id, m.text[0].source, m.text[0].style, m.text[0].color], [4, '\\1a\\0-point', 'bold', '#c8c800']);
  assert.equal(m.arrows[0].pointer, true);
  assert.equal(m.markers[0].shape, 'diamond');
  assert.equal(m.frozen[0].label, 'first run');
  assert.equal(m.frozen[1].label, 'crvb', 'no key: the name');
  assert.equal(m.frozen[0].style.draw, 'line');
  assert.deepEqual(m.frozen[1].style, {draw: 'points', dash: 'solid', width: 1, marker: 'square', marker_size: 2});
  assert.equal(m.frozen[1].color, '#f06400b0', 'a colour with its alpha is kept');
  assert.ok(Number.isNaN(m.frozen[0].ys[2]));
  const b = marksFromEvent({...MARKS, enc: 'f32', frozen: [{...MARKS.frozen[0], x: f32([0, 0.5, 1]), y: f32([0, 1, NaN])}]});
  assert.deepEqual(Array.from(b.frozen[0].xs), Array.from(m.frozen[0].xs));
  assert.equal(markCount(m), 2 + 1 + 1 + 1 + 2);
  assert.equal(markCount(null), 0);
});

test('the store keeps each window its marks; an empty event clears them', () => {
  let s = ev(READY, MARKS);
  s = ev(s, {...MARKS, win: 'graph_2', equilibria: []});
  assert.equal(windowOf(s.plots, 'graph_1')?.marks?.equilibria.length, 2);
  assert.equal(windowOf(s.plots, 'graph_2')?.marks?.equilibria.length, 0);
  s = ev(s, {ev: 'plot.marks', win: 'graph_1', equilibria: [], text: [], arrows: [], markers: [], frozen: []});
  assert.equal(markCount(windowOf(s.plots, 'graph_1')?.marks ?? null), 0);
});

test('layers: one per kind, named for the legend, and one per frozen curve by its key', () => {
  assert.deepEqual(markLayers(marksFromEvent(MARKS)).map(l => [l.key, l.label, l.color, l.count]), [
    ['equilibria', 'Equilibria', 'foreground', 2],
    ['text', 'Text', 'foreground', 1],
    ['arrows', 'Arrows', '#c8c800', 1],
    ['markers', 'Markers', '#00e100', 1],
    ['frozen-0', 'first run', '#ffcd00', 3],
    ['frozen-1', 'crvb', '#f06400b0', 0],
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

test('a curve of any style: a line through its points breaking at NaN, markers of its shape for points, both for both', () => {
  const sinks = () => ({line: recorder(), fill: recorder(), stroke: recorder()});
  let k = sinks();
  const n = traceCurve(new Float32Array([0, 0.5, 0.5, 1, 0]), new Float32Array([0, 1, NaN, 0, 1]), LINE, FRAME, 1, k.line, k.fill, k.stroke);
  assert.equal(n, 4);
  assert.deepEqual(k.line.ops.map(o => o[0]).join(''), 'MLML');
  assert.equal(k.fill.ops.length + k.stroke.ops.length, 0, 'a line has no markers');
  /* W82: a line going over the same pixels again and again (an earlier run
     round a limit cycle) is drawn once, not once per pass */
  const back = new Float32Array(1001).map((_, i) => i % 2), half = new Float32Array(1001).fill(0.5);
  k = sinks();
  assert.equal(traceCurve(back, half, LINE, FRAME, 1, k.line, k.fill, k.stroke), 2, 'the passes after the first change no pixel');
  const xy = [new Float32Array([0, 1]), new Float32Array([0, 1])] as const;
  k = sinks();
  assert.equal(traceCurve(...xy, {...LINE, draw: 'points'}, FRAME, 1, k.line, k.fill, k.stroke), 2);
  assert.equal(k.line.ops.length, 0, 'points draw no line');
  assert.equal(k.fill.ops.filter(o => o[0] === 'M').length, 2, 'a circle for each point');
  k = sinks();
  traceCurve(...xy, {...LINE, draw: 'points', marker: 'cross', marker_size: 4}, FRAME, 1, k.line, k.fill, k.stroke);
  assert.equal(k.fill.ops.length, 0);
  assert.equal(k.stroke.ops.filter(o => o[0] === 'M').length, 4, 'two strokes for each of two points');
  k = sinks();
  assert.equal(traceCurve(...xy, {...LINE, draw: 'both', marker: 'square'}, FRAME, 1, k.line, k.fill, k.stroke), 2 + 2);
  assert.ok(k.line.ops.length > 0 && k.fill.ops.length > 0, 'a line and its markers');
});

test('the dash of a style is the core\'s pattern at width 1 times the width, at least 1', () => {
  assert.deepEqual(dashPattern(LINE), []);
  assert.deepEqual(dashPattern({...LINE, dash: 'dashed'}), [6, 3]);
  assert.deepEqual(dashPattern({...LINE, dash: 'dash_dot', width: 2}), [12, 6, 2, 6]);
  assert.deepEqual(dashPattern({...LINE, dash: 'dotted', width: 0.5}), [1, 2], 'a thin line is dashed as a width of 1');
});

test('text sizes 0-4 in pixels, out of range clamped', () => {
  assert.deepEqual([0, 1, 2, 3, 4].map(textPx), [10, 12, 14, 18, 24]);
  assert.equal(textPx(9), 24);
  assert.equal(textPx(-1), 10);
});


test('W376: more than 26 frozen curves remain in the store and legend', () => {
  const frozen = Array.from({length: 32}, (_, i) => ({...MARKS.frozen[0], name: `curve${i}`, key: `key${i}`}));
  const marks = marksFromEvent({...MARKS, win: 'graph_30', frozen});
  assert.equal(marks.frozen.length, 32);
  assert.equal(marks.frozen[31].label, 'key31');
  assert.equal(marks.frozen[31].xs.length, 3);
});
