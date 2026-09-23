/* A plot window's earlier runs (store/runs.ts, GitHub #18): kept when a
   new run replaces the data, dropped by Erase, Redraw or other curves,
   capped in number and rows. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import type {SeriesEvent} from '../src/protocol/types';
import {windowOf} from '../src/store/plots';
import {keepRun, RUNS_KEEP, RUNS_MAX_ROWS} from '../src/store/runs';
import type {PlotSeries} from '../src/store/series';
import {initialState, reduce, type AppState} from '../src/store/state';

const ev = (s: AppState, e: object) => reduce(s, {type: 'event', ev: e as never});
const w1 = (s: AppState) => windowOf(s.plots, 1)!;

function run(v0: number, version: number, rows = 3, y = 2): SeriesEvent {
  const vs = Array.from({length: rows}, (_, i) => v0 + i);
  return {
    ev: 'series', win: 1, rows, three: 0, xlabel: '', ylabel: '', zlabel: '', version,
    curves: [{x: 0, y, z: 0, color: 1, line: 1}], shift: [0, 0, 0],
    columns: [{col: 0, name: 'T', data: vs.map((_, i) => i)}, {col: y, name: y === 2 ? 'W' : 'V', data: vs}],
  };
}
const append = (from: number, rows: number, v0: number) => ({
  ev: 'series', op: 'append', win: 1, from, rows,
  columns: [{col: 0, data: Array.from({length: rows - from}, (_, i) => from + i)},
    {col: 2, data: Array.from({length: rows - from}, (_, i) => v0 + i)}],
});
const firstY = (s: PlotSeries) => s.columns.get(2)![0];

test('a new run (another data version) keeps the one it replaces', () => {
  let s = ev(initialState, run(0, 1));
  assert.equal(w1(s).history.runs.length, 0, 'the first series has nothing before it');
  s = ev(s, run(10, 2));
  assert.equal(w1(s).history.runs.length, 1);
  assert.equal(firstY(w1(s).history.runs[0]), 0, 'the previous run is the old data');
  assert.equal(firstY(w1(s).series!), 10);
  s = ev(s, run(10, 2));
  assert.equal(w1(s).history.runs.length, 1, 'the same version again (a restyle, a resend) keeps no copy');
});

test('a run that arrives by appends is kept once: at its start, not again at its full series', () => {
  let s = ev(initialState, run(0, 1));
  s = ev(s, append(0, 2, 50));
  assert.equal(w1(s).history.runs.length, 1, 'the restart kept the old run');
  assert.equal(firstY(w1(s).history.runs[0]), 0);
  assert.ok(w1(s).history.live);
  s = ev(s, append(2, 3, 52));
  s = ev(s, run(50, 2));
  assert.equal(w1(s).history.runs.length, 1, 'the full series ends the same run');
  assert.ok(!w1(s).history.live);
  /* Continue: appends from the end are the same run */
  s = ev(s, append(3, 5, 53));
  s = ev(s, run(50, 3, 5));
  assert.equal(w1(s).history.runs.length, 1);
});

test('the history is trimmed to its rows and capped', () => {
  let s = ev(initialState, run(0, 1));
  s = ev(s, append(0, 2, 50));
  assert.equal(w1(s).history.runs[0].columns.get(2)!.length, 3);
  let runs: PlotSeries[] = [];
  const one = (i: number, rows: number): PlotSeries => ({...w1(ev(initialState, run(i, 1))).series!, rows});
  for (let i = 0; i < RUNS_KEEP + 5; i++) runs = keepRun(runs, one(i, 3));
  assert.equal(runs.length, RUNS_KEEP, 'at most RUNS_KEEP');
  assert.equal(firstY(runs[0]), 5, 'the oldest go first');
  runs = keepRun(runs, one(99, RUNS_MAX_ROWS));
  assert.equal(runs.length, 1, 'and at most RUNS_MAX_ROWS rows in all');
  assert.equal(keepRun(runs, one(1, 0)), runs, 'an empty series is not a run');
});

test('Erase hides the current data and drops the history; the next run shows alone', () => {
  let s = ev(initialState, run(0, 1));
  s = ev(s, run(10, 2));
  s = ev(s, {ev: 'erase', win: 1});
  assert.deepEqual(w1(s).history, {runs: [], erased: true, live: false});
  assert.equal(firstY(w1(s).series!), 10, 'the data stays for Redraw');
  s = ev(s, append(0, 2, 70));
  assert.equal(w1(s).history.runs.length, 0, 'the erased run is not kept');
  assert.ok(!w1(s).history.erased);
  s = ev(s, run(70, 3));
  assert.equal(w1(s).history.runs.length, 0);
});

test('Redraw shows the current data again, without the earlier runs', () => {
  let s = ev(initialState, run(0, 1));
  s = ev(s, run(10, 2));
  s = ev(s, {ev: 'redraw', win: 1});
  assert.deepEqual(w1(s).history, {runs: [], erased: false, live: false});
  s = ev(s, {ev: 'erase', win: 1});
  s = ev(s, {ev: 'redraw', win: 1});
  assert.ok(!w1(s).history.erased, 'Redraw after Erase shows the data');
});

test('other curves are another picture: no history; the legend toggle is per window', () => {
  let s = ev(initialState, run(0, 1));
  s = ev(s, run(10, 2));
  s = ev(s, run(10, 2, 3, 1));
  assert.equal(w1(s).history.runs.length, 0);
  s = reduce(s, {type: 'showRuns', win: 1, show: false});
  assert.equal(w1(s).showRuns, false);
});
