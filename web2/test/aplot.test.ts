/* The array plot slice: the reducer, decoding `values`, and the pure
   readouts (docs/ui-v2.md T12, store/aplot.ts), without a browser (npm test). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import type {AplotEvent} from '../src/protocol/types';
import {aplotColumnName, initialAplot, reduceAplot, timeAt, valueAt} from '../src/store/aplot';

const ev = (over: Partial<AplotEvent> = {}): AplotEvent => ({
  ev: 'array_plot.shown', title: 'u0..3', nx: 4, ny: 3,
  values: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11],
  colormap: 'viridis', ramp: Array.from({length: 256}, (_, i) => '#' + i.toString(16).padStart(2, '0').repeat(3)),
  zmin: 0, zmax: 11, tlo: 0, thi: 20,
  ...over,
});

test('an aplot event decodes `values` and clears any hover', () => {
  let s = reduceAplot(initialAplot, {type: 'hover', hover: {row: 0, col: 0, t: 0, value: 0}});
  s = reduceAplot(s, {type: 'event', ev: ev()});
  assert.equal(s.event?.title, 'u0..3');
  assert.equal(s.values.length, 12);
  assert.equal(s.values[5], 5);
  assert.equal(s.hover, null);
});

test('values decodes a base64 f32 string the same as the JSON numbers', () => {
  const nums = [0, 1, 2, 3];
  const b64 = Buffer.from(new Float32Array(nums).buffer).toString('base64');
  const s = reduceAplot(initialAplot, {type: 'event', ev: ev({nx: 4, ny: 1, values: b64, enc: 'f32'})});
  assert.deepEqual(Array.from(s.values), nums);
});

test('window open/close tracks the core\'s array plot window (105)', () => {
  let s = reduceAplot(initialAplot, {type: 'window', open: true});
  assert.equal(s.windowOpen, true);
  s = reduceAplot(s, {type: 'event', ev: ev()});
  s = reduceAplot(s, {type: 'panel', open: true});
  assert.equal(s.open, true);
  s = reduceAplot(s, {type: 'window', open: false});
  assert.equal(s.windowOpen, false, 'the window is gone');
  assert.equal(s.open, false, 'the panel closes with it');
  assert.equal(s.event, null, 'there is nothing left to show');
});

test('the panel toggle is a no-op when already at that value (same object back)', () => {
  const t = reduceAplot(initialAplot, {type: 'panel', open: false});
  assert.equal(t, initialAplot);
});

test('valueAt reads row-major, off-grid as NaN', () => {
  const s = reduceAplot(initialAplot, {type: 'event', ev: ev()});
  assert.equal(valueAt(s, 0, 0), 0);
  assert.equal(valueAt(s, 1, 2), 6); /* row 1, col 2: index 1*4+2 = 6 */
  assert.equal(valueAt(s, 2, 3), 11);
  assert.ok(Number.isNaN(valueAt(s, -1, 0)));
  assert.ok(Number.isNaN(valueAt(s, 3, 0)));
  assert.ok(Number.isNaN(valueAt(s, 0, 4)));
  assert.ok(Number.isNaN(valueAt(initialAplot, 0, 0)), 'no event yet');
});

test('timeAt interpolates linearly between tlo (row 0) and thi (the last row)', () => {
  const e = ev({ny: 5, tlo: 0, thi: 20});
  assert.equal(timeAt(e, 0), 0);
  assert.equal(timeAt(e, 4), 20);
  assert.equal(timeAt(e, 2), 10);
});

test('timeAt is tlo for a single-row grid (no division by zero)', () => {
  assert.equal(timeAt(ev({ny: 1, tlo: 5, thi: 5}), 0), 5);
});

test('aplotColumnName reads the title\'s low bound plus the column position', () => {
  assert.equal(aplotColumnName(ev({title: 'u0..3'}), 0), 'u0');
  assert.equal(aplotColumnName(ev({title: 'u0..3'}), 3), 'u3');
  assert.equal(aplotColumnName(ev({title: 'V-2..5'}), 1), 'V-1');
});

test('aplotColumnName is empty when the title does not parse as root+range', () => {
  assert.equal(aplotColumnName(ev({title: 'Array!'}), 0), '');
});
