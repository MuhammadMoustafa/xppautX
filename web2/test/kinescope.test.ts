/* The kinescope (docs/ui-v2.md T15): the `film` event's capture, reset,
   play and autoplay reach store/kinescope.ts through store/state.ts,
   without a browser (npm test). The playback clock and the `pixels` ask
   are session.ts's (impure: timers, the DOM), covered by tools/web2check.mjs
   instead. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import type {PlotWindowInfo, SeriesEvent} from '../src/protocol/types';
import {initialKinescope, reduceKinescope, snapshotWindow} from '../src/store/kinescope';
import {windowOf} from '../src/store/plots';
import {initialState, reduce, type AppState} from '../src/store/state';

const ev = (s: AppState, e: object) => reduce(s, {type: 'event', ev: e as never});

const series = (win: number, v0: number): SeriesEvent => ({
  ev: 'series', win, rows: 2, three: 0, xlabel: '', ylabel: '', zlabel: '',
  curves: [{x: 0, y: 1, z: 0, color: 0, line: 1}], shift: [0, 0, 0],
  columns: [{col: 0, name: 'T', data: [0, 1]}, {col: 1, name: 'V', data: [v0, v0 + 1]}],
});

const info = (win: number): PlotWindowInfo => ({
  win, title: 'V vs T', three: 0, xlo: 0, xhi: 1, ylo: -1, yhi: 1, xlabel: '', ylabel: '', zlabel: '',
  box: {xmin: 0, xmax: 1, ymin: -1, ymax: 1, zmin: -1, zmax: 1}, theta: 45, phi: 45, persp: 0, zplane: -1000,
  zview: 1000, curves: [{x: 0, y: 1, z: 0, color: 0, line: 1}], shift: [0, 0, 0],
});

const plots = (...w: PlotWindowInfo[]) => ({ev: 'plots', active: w[0]?.win ?? 1, windows: w});
const film = (op: 'capture' | 'reset' | 'play' | 'autoplay', over: object = {}) =>
  ({ev: 'film', op, count: 0, win: 1, cycles: 1, delay: 50, ...over});

function oneCapturedFrame(v0: number): AppState {
  let s = ev(initialState, plots(info(1)));
  s = ev(s, series(1, v0));
  return ev(s, film('capture', {count: 1}));
}

test('reduceKinescope: capture appends, reset empties, show/playing update the player', () => {
  const frame = snapshotWindow({win: 1, info: null, series: null, nullclines: null, dfield: null, marks: null,
    viewport: {x: null, y: null}, viewportHistory: [], view3d: null});
  let s = reduceKinescope(initialKinescope, {type: 'capture', frame});
  assert.equal(s.frames.length, 1);
  s = reduceKinescope(s, {type: 'capture', frame});
  assert.equal(s.frames.length, 2);
  s = reduceKinescope(s, {type: 'playing', playing: true, cycles: 3, delay: 80});
  assert.deepEqual([s.playing, s.cycles, s.delay], [true, 3, 80]);
  s = reduceKinescope(s, {type: 'show', index: 1});
  assert.equal(s.shown, 1);
  s = reduceKinescope(s, {type: 'clear'});
  assert.deepEqual(s, initialKinescope);
});

test('a `film` capture snapshots the window as it is drawn now (series, marks, axes)', () => {
  const s = oneCapturedFrame(10);
  assert.equal(s.kinescope.frames.length, 1);
  const f = s.kinescope.frames[0];
  assert.equal(f.win, 1);
  assert.equal(f.info?.title, 'V vs T');
  assert.deepEqual(Array.from(f.series!.columns.get(1)!), [10, 11]);
});

test('two captures after two different integrations hold frames that differ', () => {
  let s = oneCapturedFrame(10);
  s = ev(s, series(1, 20));
  s = ev(s, film('capture', {count: 2}));
  assert.equal(s.kinescope.frames.length, 2);
  const [a, b] = s.kinescope.frames;
  assert.notDeepEqual(Array.from(a.series!.columns.get(1)!), Array.from(b.series!.columns.get(1)!));
});

test('`reset` empties the frames', () => {
  let s = oneCapturedFrame(10);
  s = ev(s, series(1, 20));
  s = ev(s, film('capture', {count: 2}));
  s = ev(s, film('reset', {count: 0}));
  assert.deepEqual(s.kinescope, initialKinescope);
});

test('`play` and `autoplay` note the timing; the frame shown is session.ts\'s clock, not this reducer\'s', () => {
  let s = oneCapturedFrame(10);
  s = ev(s, film('play', {count: 1, cycles: 1, delay: 40}));
  assert.deepEqual([s.kinescope.playing, s.kinescope.delay], [true, 40]);
  s = ev(s, film('autoplay', {count: 1, cycles: 5, delay: 90}));
  assert.deepEqual([s.kinescope.playing, s.kinescope.cycles, s.kinescope.delay], [true, 5, 90]);
});

test('a capture for a window the store does not know is left out (nothing to snapshot)', () => {
  const s = ev(initialState, film('capture', {win: 7, count: 1}));
  assert.equal(s.kinescope.frames.length, 0);
});

test('snapshotWindow carries exactly what the chart draws from, not the viewport history', () => {
  const w = {win: 3, info: info(3), series: null, nullclines: null, dfield: null, marks: null,
    viewport: {x: {min: 0, max: 1}, y: null}, viewportHistory: [{x: null, y: null}], view3d: null};
  const f = snapshotWindow(w);
  assert.deepEqual(f, {win: 3, info: w.info, series: null, nullclines: null, dfield: null, marks: null, viewport: w.viewport});
  assert.ok(!('viewportHistory' in f));
});

test('windowOf still finds the window after a capture (the reducer does not disturb plots)', () => {
  const s = oneCapturedFrame(10);
  assert.ok(windowOf(s.plots, 1));
});
