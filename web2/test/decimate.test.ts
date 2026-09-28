/* Decimation of long curves (plot/decimate.ts), without a browser. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {ColumnTrace, LineTrace, tracePoints, traceLine, type LineSink, type PixelFrame} from '../src/plot/decimate';

/** a sink that records the path as polylines */
function recorder(): LineSink & {lines: [number, number][][]} {
  const lines: [number, number][][] = [];
  return {
    lines,
    moveTo(x, y) { lines.push([[x, y]]); },
    lineTo(x, y) { lines[lines.length - 1].push([x, y]); },
  };
}

/* data 0..100 on a 100x100 pixel area at (10, 20): one data unit per pixel */
const frame: PixelFrame = {xmin: 0, xmax: 100, ymin: 0, ymax: 100, left: 10, top: 20, width: 100, height: 100};

test('a sparse line is drawn as it is, in canvas pixels, y down', () => {
  const r = recorder();
  const n = traceLine([10, 50, 90], [10, 50, 10], 0, 2, frame, r);
  assert.equal(n, 3);
  assert.deepEqual(r.lines, [[[20, 110], [60, 70], [100, 110]]]);
});

test('a curve that goes round the same loop many times costs one loop', () => {
  const xs: number[] = [], ys: number[] = [];
  for (let k = 0; k < 200000; k++) {
    const a = (k / 500) * 2 * Math.PI; /* 400 turns, 500 points each */
    xs.push(50 + 40 * Math.cos(a));
    ys.push(50 + 40 * Math.sin(a));
  }
  const r = recorder();
  const n = traceLine(xs, ys, 0, xs.length - 1, frame, r);
  assert.ok(n > 100 && n < 2000, `${n} vertices for 200 000 points`);
  /* every pixel of the circle is still reached: the kept vertices go all the way round */
  const angles = new Set(r.lines.flat().map(([x, y]) => Math.round((Math.atan2(y - 70, x - 60) * 36) / Math.PI)));
  assert.ok(angles.size >= 70, `${angles.size} of 72 directions`);
});

test('points closer than a pixel collapse; a new pixel starts a new piece', () => {
  const r = recorder();
  const xs = [10, 10.1, 10.2, 10.3, 30], ys = [10, 10, 10, 10, 10];
  traceLine(xs, ys, 0, 4, frame, r);
  const last = r.lines[r.lines.length - 1];
  assert.deepEqual(last[last.length - 1], [40, 110], 'the far point is reached');
  assert.ok(r.lines.flat().length <= 4);
});

test('segments outside the area are left out, crossing ones kept, NaN breaks the line', () => {
  const r = recorder();
  traceLine([-50, -40, -30, 50, 60], [50, 60, 50, 50, 50], 0, 4, frame, r);
  assert.deepEqual(r.lines, [[[-20, 70], [60, 70], [70, 70]]], 'the first two are left of the area');
  const g = recorder();
  traceLine([10, 20, NaN, 30, 40], [10, 10, 5, 10, 10], 0, 4, frame, g);
  assert.equal(g.lines.length, 2);
});

test('an index range draws only those rows', () => {
  const r = recorder();
  assert.equal(traceLine([0, 10, 20, 30], [0, 10, 20, 30], 1, 2, frame, r), 2);
  assert.deepEqual(r.lines, [[[20, 110], [30, 100]]]);
});

test('points: one dot per pixel, dots just outside that still show are kept', () => {
  const dots: [number, number][] = [];
  const n = tracePoints([10, 10.2, 10.4, 50, -0.5, -20, NaN], [10, 10, 10, 50, 50, 50, 1], 0, 6, frame, 2,
    (x, y) => dots.push([x, y]));
  assert.equal(n, 3);
  assert.deepEqual(dots, [[20, 110], [60, 70], [9.5, 70]]);
});

test('a trace run a slice at a time, or as rows arrive, keeps what a trace at once keeps', () => {
  const xs: number[] = [], ys: number[] = [];
  for (let k = 0; k < 20000; k++) {
    const a = k / 97;
    xs.push(50 + (40 - k / 1000) * Math.cos(a));
    ys.push(50 + (40 - k / 1000) * Math.sin(a) + (k === 7000 ? NaN : 0));
  }
  const whole = recorder();
  traceLine(xs, ys, 0, xs.length - 1, frame, whole);
  const sliced = new LineTrace(frame);
  let tasks = 0;
  while (!sliced.run(xs, ys, xs.length, 1234)) tasks++;
  assert.ok(tasks > 10);
  const a = recorder();
  sliced.draw(xs, ys, frame, a);
  assert.deepEqual(a.lines, whole.lines);
  const grown = new LineTrace(frame);
  for (let end = 500; end <= xs.length; end += 500) grown.run(xs, ys, end); /* appends */
  const b = recorder();
  grown.draw(xs, ys, frame, b);
  assert.deepEqual(b.lines, whole.lines);
});

test('kept vertices draw in another frame, and rows the arrays lack are skipped', () => {
  const t = new LineTrace(frame);
  t.run([10, 50, 90], [10, 50, 10], 3);
  const r = recorder();
  const zoomed = {...frame, xmin: 0, xmax: 50, ymin: 0, ymax: 50}; /* two pixels per unit */
  assert.equal(t.draw([10, 50, 90], [10, 50, 10], zoomed, r), 3);
  assert.deepEqual(r.lines, [[[30, 100], [110, 20], [190, 100]]]);
  const short = recorder();
  assert.equal(t.draw([10, 50], [10, 50], frame, short), 2);
  assert.deepEqual(short.lines, [[[20, 110], [60, 70]]]);
});

test('a time plot keeps the first, lowest, highest and last point of each pixel column (W82)', () => {
  const xs: number[] = [], ys: number[] = [];
  for (let k = 0; k < 100000; k++) { /* 1000 points a pixel column, a fast oscillation in each */
    xs.push(k / 1000);
    ys.push(50 + 40 * Math.sin(k / 7));
  }
  const t = new ColumnTrace(frame);
  t.run(xs, ys, xs.length);
  const r = recorder();
  const n = t.draw(xs, ys, frame, r);
  assert.ok(n <= 4 * 101 && n >= 3 * 100, `${n} vertices for 100 000 points in 100 columns`);
  const flat = r.lines.flat(), top = Math.min(...flat.map(p => p[1])), bottom = Math.max(...flat.map(p => p[1]));
  assert.ok(top < 20 + 11 && bottom > 20 + 89, 'each column still reaches its highest and lowest point');
});

test('a time plot trace carries on over appended rows: the same path as tracing them at once', () => {
  const xs = Array.from({length: 5000}, (_, k) => k / 50), ys = xs.map(x => 50 + 30 * Math.cos(x));
  const whole = new ColumnTrace(frame), parts = new ColumnTrace(frame);
  whole.run(xs, ys, xs.length);
  for (const end of [7, 1234, 1235, 4000, 5000]) parts.run(xs, ys, end);
  const a = recorder(), b = recorder();
  assert.equal(parts.draw(xs, ys, frame, b), whole.draw(xs, ys, frame, a));
  assert.deepEqual(b.lines, a.lines);
  /* before the last rows came: what was there, the open column included */
  const early = new ColumnTrace(frame);
  early.run(xs, ys, 1235);
  const e = recorder();
  early.draw(xs, ys, frame, e);
  const end = e.lines[e.lines.length - 1];
  assert.deepEqual(end[end.length - 1], [10 + xs[1234], 20 + 100 - ys[1234]], 'the last row stored is drawn');
});

test('a time plot trace: NaN breaks the line, points off the area each side make one column', () => {
  const g = recorder(), t = new ColumnTrace(frame);
  const xs = [10, 20, 30, 40, 50], ys = [10, 10, NaN, 10, 10];
  t.run(xs, ys, 5);
  t.draw(xs, ys, frame, g);
  assert.equal(g.lines.length, 2);
  const off = new ColumnTrace(frame);
  const ox = Array.from({length: 1000}, (_, k) => -1000 + k), oy: number[] = ox.map((_, k) => (k % 2 ? 0 : 100));
  ox.push(50); oy.push(50);
  off.run(ox, oy, ox.length);
  assert.ok(off.draw(ox, oy, frame, recorder()) <= 5, 'the thousand points left of the area are one column');
});
