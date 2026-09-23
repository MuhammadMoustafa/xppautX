/* Live series: decoding, appends in place, the reducer (npm test). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {decode, valueCount} from '../src/protocol/decode';
import type {SeriesAppendEvent, SeriesEvent} from '../src/protocol/types';
import {appendRows, seriesFromEvent} from '../src/store/series';
import {initialState, reduce, type AppState} from '../src/store/state';

/** base64 of little-endian float32, as the server's enc "f32" */
function f32(values: number[]): string {
  const b = Buffer.alloc(4 * values.length);
  values.forEach((v, i) => b.writeFloatLE(v, 4 * i));
  return b.toString('base64');
}

const full: SeriesEvent = {
  ev: 'series', win: 1, rows: 2, three: 0, xlabel: '', ylabel: '', zlabel: '',
  curves: [{x: 1, y: 2, z: 1, color: 0, line: 1}], shift: [0, 0, 0],
  columns: [
    {col: 0, name: 'T', data: [0, 1]},
    {col: 1, name: 'V', data: [10, 11]},
    {col: 2, name: 'W', data: [20, 21]},
  ],
};

function append(from: number, n: number, win = 1): SeriesAppendEvent {
  const rows = Array.from({length: n}, (_, i) => from + i);
  return {ev: 'series', op: 'append', win, from, rows: from + n, columns: [
    {col: 0, data: rows}, {col: 1, data: rows.map(r => 10 + r)}, {col: 2, data: rows.map(r => 20 + r)},
  ]};
}

const ev = (s: AppState, e: object) => reduce(s, {type: 'event', ev: e as never});

test('f32 columns decode to the floats, NaN included; JSON null is NaN', () => {
  const v = [0, 0.05, -1.5e-7, 3.25, NaN, 1e30];
  const got = decode(f32(v));
  assert.equal(valueCount(f32(v)), 6);
  assert.deepEqual([...got.subarray(0, 4)], v.slice(0, 4).map(x => Math.fround(x)));
  assert.ok(Number.isNaN(got[4]));
  assert.equal(got[5], Math.fround(1e30));
  for (const n of [0, 1, 2, 3]) assert.equal(valueCount(f32(v.slice(0, n))), n, `${n} values`);
  const j = decode([1, null, 2]);
  assert.ok(Number.isNaN(j[1]) && j[2] === 2);
});

test('an f32 series equals the same series in JSON numbers', () => {
  const values = [-0.143999994, 0.0299999993, 1.20000005];
  const a = seriesFromEvent({...full, rows: 3, columns: [{col: 0, name: 'T', data: values}]});
  const b = seriesFromEvent({...full, rows: 3, enc: 'f32', columns: [{col: 0, name: 'T', data: f32(values)}]});
  assert.deepEqual([...a.columns.get(0)!], [...b.columns.get(0)!]);
  assert.deepEqual([...b.columns.get(0)!], values.map(Math.fround));
});

test('appends continue the series in place, doubling the buffers', () => {
  let s = seriesFromEvent(full);
  s = appendRows(s, append(2, 3))!;
  assert.equal(s.rows, 5);
  assert.deepEqual([...s.columns.get(1)!], [10, 11, 12, 13, 14]);
  const buf = s.buffers.get(1)!;
  assert.ok(buf.length >= 1024, 'room to grow');
  const before = s;
  s = appendRows(s, append(5, 4))!;
  assert.equal(s.buffers.get(1), buf, 'the same buffer: no copy');
  assert.deepEqual([...s.columns.get(2)!], [20, 21, 22, 23, 24, 25, 26, 27, 28]);
  assert.equal(before.columns.get(2)!.length, 5, 'the older series still ends where it did');
  /* many appends: the buffer grows by doubling, so copies stay O(n) */
  let grown = 0, last = s.buffers.get(0);
  for (let r = s.rows; r < 100000; r += 1000) {
    s = appendRows(s, append(r, 1000))!;
    if (s.buffers.get(0) !== last) grown++;
    last = s.buffers.get(0);
  }
  assert.equal(s.rows, 100009);
  assert.equal(s.columns.get(0)![99999], 99999);
  assert.ok(grown <= 7, `${grown} reallocations`);
});

test('an append from row 0 starts again without touching what older states show', () => {
  const s = appendRows(seriesFromEvent(full), append(2, 3))!;
  const again = appendRows(s, append(0, 2))!;
  assert.equal(again.rows, 2);
  assert.deepEqual([...again.columns.get(1)!], [10, 11]);
  assert.notEqual(again.buffers.get(1), s.buffers.get(1));
  assert.deepEqual([...s.columns.get(1)!], [10, 11, 12, 13, 14], 'unchanged');
  const mid = appendRows(s, append(3, 1))!;
  assert.deepEqual([...mid.columns.get(1)!], [10, 11, 12, 13]);
  assert.deepEqual([...s.columns.get(1)!], [10, 11, 12, 13, 14], 'unchanged');
});

test('an append that does not continue the series is left for the full series', () => {
  const s = seriesFromEvent(full);
  assert.equal(appendRows(s, append(3, 2)), null, 'a gap');
  assert.equal(appendRows(s, append(2, 2, 2)), null, 'another window');
  assert.equal(appendRows(s, {...append(2, 2), columns: append(2, 2).columns.slice(0, 2)}), null, 'other columns');
  assert.equal(appendRows(s, {...append(2, 2), rows: 5}), null, 'rows that do not add up');
});

test('the reducer applies appends, keeps the zoom, and counts them', () => {
  let s = ev(initialState, full);
  s = reduce(s, {type: 'viewport', viewport: {x: {min: 0, max: 1}, y: null}});
  s = reduce(s, {type: 'hover', hover: {curve: 0, row: 1, x: 11, y: 21, t: 1}});
  s = ev(s, append(2, 2));
  assert.equal(s.series!.rows, 4);
  assert.equal(s.seriesAppends, 1);
  assert.equal(s.seriesCount, 1, 'only full series count there');
  assert.deepEqual(s.viewport.x, {min: 0, max: 1});
  assert.equal(s.hover?.row, 1, 'a hovered row before the append stays');
  s = ev(s, append(0, 1));
  assert.equal(s.series!.rows, 1);
  assert.equal(s.hover, null, 'rows from 0 on were replaced');
  assert.equal(ev(initialState, append(0, 2)), initialState, 'nothing to append to');
});
