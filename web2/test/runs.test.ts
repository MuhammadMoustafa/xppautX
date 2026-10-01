/* A plot window's earlier runs are the core's (W65: core/plot_data.cpp): the
   page holds the copy its `runs` events describe. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import type {RunsEvent} from '../src/protocol/types';
import {windowOf} from '../src/store/plots';
import {reduce, type AppState} from '../src/store/state';
import {READY} from './hello';

const ev = (s: AppState, e: object) => reduce(s, {type: 'event', ev: e as never});
const w1 = (s: AppState) => windowOf(s.plots, 1)!;

function run(v0: number, rows = 3): RunsEvent['add'][number] {
  const vs = Array.from({length: rows}, (_, i) => v0 + i);
  return {
    rows, three: 0, curves: [{x: 0, y: 2, z: 0, color: 1, line: 1}], shift: [0, 0, 0],
    columns: [{col: 0, name: 'T', data: vs.map((_, i) => i)}, {col: 2, name: 'W', data: vs}],
  };
}
const runs = (add: RunsEvent['add'], o: Partial<RunsEvent> = {}): RunsEvent =>
  ({ev: 'runs', win: 1, erased: 0, clear: 0, drop: 0, keep: 0, add, ...o});
const firstY = (s: AppState, i: number) => w1(s).history.runs[i].columns.get(2)![0];

test('the core adds earlier runs, oldest first, with their columns and names', () => {
  let s = ev(READY, runs([run(0)]));
  assert.equal(w1(s).history.runs.length, 1);
  assert.equal(firstY(s, 0), 0);
  assert.equal(w1(s).history.runs[0].names.get(2), 'W');
  s = ev(s, runs([run(10)]));
  assert.deepEqual([firstY(s, 0), firstY(s, 1)], [0, 10]);
});

test('drop forgets the oldest runs, clear all of them', () => {
  let s = ev(READY, runs([run(0), run(10), run(20)]));
  s = ev(s, runs([run(30)], {drop: 2}));
  assert.deepEqual(w1(s).history.runs.map((_, i) => firstY(s, i)), [20, 30]);
  s = ev(s, runs([], {clear: 1}));
  assert.equal(w1(s).history.runs.length, 0);
});

test('keep makes the series the window holds its newest run, without its data again', () => {
  const series = {
    ev: 'series', win: 1, rows: 2, three: 0, xlabel: 'V', ylabel: 'W', zlabel: '', version: 4,
    curves: [{x: 0, y: 2, z: 0, color: 1, line: 1}], shift: [0, 0, 0],
    columns: [{col: 0, name: 'T', data: [0, 1]}, {col: 2, name: 'W', data: [7, 8]}],
  };
  let s = ev(READY, runs([run(0)]));
  s = ev(s, series);
  const held = w1(s).series!;
  s = ev(s, runs([], {drop: 1, keep: 1}));
  const r = w1(s).history.runs;
  assert.equal(r.length, 1);
  assert.equal(r[0].columns.get(2), held.columns.get(2), 'the very same data');
  assert.equal(r[0].rows, 2);
  assert.equal(r[0].version, null);
});

test('erased is what the core says: Erase hides the current run, the next run or Redraw shows it', () => {
  let s = ev(READY, runs([run(0)]));
  s = ev(s, runs([], {clear: 1, erased: 1}));
  assert.ok(w1(s).history.erased);
  assert.equal(w1(s).history.runs.length, 0);
  s = ev(s, runs([], {clear: 1, erased: 0}));
  assert.ok(!w1(s).history.erased);
});

test('the same history again leaves the state as it was', () => {
  const s = ev(READY, runs([], {}));
  assert.equal(ev(s, runs([], {})).plots, s.plots);
});

test('the runs toggle and the zoom of a window are the core\'s (plots), except while the user\'s change is on its way', () => {
  const info = (zoom: unknown, runsOn: number) => ({
    ev: 'plots', active: 1, windows: [{win: 1, title: 'W vs V', three: 0, xlo: 0, xhi: 1, ylo: 0, yhi: 1, xlabel: '', ylabel: '',
      zlabel: '', box: {}, theta: 0, phi: 0, persp: 0, zplane: 0, zview: 0, curves: [], shift: [0, 0, 0], zoom, runs: runsOn}],
  });
  let s = ev(READY, info({x: [0, 2], y: null}, 0));
  assert.deepEqual(w1(s).viewport, {x: {min: 0, max: 2}, y: null});
  assert.equal(w1(s).showRuns, false);
  s = ev(s, info({x: null, y: null}, 1));
  assert.deepEqual(w1(s).viewport, {x: null, y: null});
  assert.equal(w1(s).showRuns, true);
  const held = {...info(undefined, 1)};
  held.windows[0] = {...held.windows[0], zoom: undefined, runs: undefined} as never;
  s = reduce(s, {type: 'viewport', viewport: {x: {min: 1, max: 2}, y: null}, win: 1});
  s = ev(s, held);
  assert.deepEqual(w1(s).viewport.x, {min: 1, max: 2}, 'an event without a zoom keeps the page\'s');
});
