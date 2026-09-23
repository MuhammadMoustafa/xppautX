/* The plot windows in the store (docs/ui-v2.md T6): tabs follow `plots`,
   each window keeps its own series and zoom. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import type {PlotWindowInfo, SeriesEvent} from '../src/protocol/types';
import {activeWindow, windowOf} from '../src/store/plots';
import {initialState, reduce, type AppState} from '../src/store/state';

const ev = (s: AppState, e: object) => reduce(s, {type: 'event', ev: e as never});
const wins = (s: AppState) => s.plots.windows.map(w => w.win);

const series = (win: number, x: number, y: number): SeriesEvent => ({
  ev: 'series', win, rows: 2, three: 0, xlabel: '', ylabel: '', zlabel: '',
  curves: [{x, y, z: 0, color: 0, line: 1}], shift: [0, 0, 0],
  columns: [{col: 0, name: 'T', data: [0, 1]}, {col: 1, name: 'V', data: [2, 3]}, {col: 2, name: 'W', data: [4, 5]}]
    .filter(c => c.col === 0 || c.col === x || c.col === y),
});

const info = (win: number, title: string): PlotWindowInfo => ({
  win, title, three: 0, xlo: -1, xhi: 1, ylo: -2, yhi: 2, xlabel: '', ylabel: '', zlabel: '',
  box: {xmin: -1, xmax: 1, ymin: -2, ymax: 2, zmin: -1, zmax: 1}, theta: 45, phi: 45, persp: 0, zplane: -1000,
  zview: 1000, curves: [{x: 1, y: 2, z: 1, color: 0, line: 1}], shift: [0, 0, 0],
});

const plots = (active: number, ...w: PlotWindowInfo[]) => ({ev: 'plots', active, windows: w});
const zoom = {x: {min: 0, max: 1}, y: {min: 0, max: 2}};

function twoWindows(): AppState {
  let s = ev(initialState, plots(1, info(1, 'W vs V')));
  s = ev(s, series(1, 1, 2));
  s = ev(s, plots(2, info(1, 'W vs V'), info(2, 'V vs T')));
  return ev(s, series(2, 0, 1));
}

test('plots makes one window per core window, the active one selected', () => {
  const s = twoWindows();
  assert.deepEqual(wins(s), [1, 2]);
  assert.equal(s.plots.active, 2);
  assert.equal(activeWindow(s.plots)?.info?.title, 'V vs T');
  assert.equal(windowOf(s.plots, 1)?.series?.curves[0].x, 1, "window 1 keeps its own series");
  assert.equal(windowOf(s.plots, 2)?.series?.curves[0].x, 0);
  assert.equal(s.seriesCount, 2);
});

test('each window keeps its zoom when the tabs switch', () => {
  let s = twoWindows();
  s = reduce(s, {type: 'selectWindow', win: 1});
  s = reduce(s, {type: 'viewport', viewport: zoom, push: true});
  s = reduce(s, {type: 'selectWindow', win: 2});
  assert.deepEqual(activeWindow(s.plots)?.viewport, {x: null, y: null}, 'window 2 has its own');
  s = reduce(s, {type: 'viewport', viewport: {x: {min: 5, max: 6}, y: null}, push: true, win: 2});
  s = reduce(s, {type: 'selectWindow', win: 1});
  assert.deepEqual(activeWindow(s.plots)?.viewport, zoom);
  assert.equal(activeWindow(s.plots)?.viewportHistory.length, 1);
  s = reduce(s, {type: 'undoViewport', win: 2});
  assert.deepEqual(windowOf(s.plots, 2)?.viewport, {x: null, y: null}, 'undo by window');
  assert.deepEqual(windowOf(s.plots, 1)?.viewport, zoom);
});

test('a new series for another window leaves the shown zoom and readout alone', () => {
  let s = twoWindows();
  s = reduce(s, {type: 'selectWindow', win: 1});
  s = reduce(s, {type: 'viewport', viewport: zoom});
  s = reduce(s, {type: 'hover', hover: {curve: 0, row: 1, x: 3, y: 5, t: 1}});
  s = ev(s, series(2, 0, 2));
  assert.deepEqual(activeWindow(s.plots)?.viewport, zoom);
  assert.equal(s.hover?.row, 1);
  s = ev(s, series(1, 0, 1)); /* other curves in the shown window */
  assert.deepEqual(activeWindow(s.plots)?.viewport, {x: null, y: null});
  assert.equal(s.hover, null);
});

test('appends go to their own window', () => {
  let s = twoWindows();
  s = ev(s, {ev: 'series', op: 'append', win: 1, from: 2, rows: 3, columns: [
    {col: 0, data: [2]}, {col: 1, data: [6]}, {col: 2, data: [7]}]});
  assert.equal(windowOf(s.plots, 1)?.series?.rows, 3);
  assert.equal(windowOf(s.plots, 2)?.series?.rows, 2);
  assert.equal(s.seriesAppends, 1);
});

test('the core selecting a window, or destroying one, moves the tabs', () => {
  let s = twoWindows();
  s = ev(s, {ev: 'window', op: 'select', win: 1, w: 0, h: 0});
  assert.equal(s.plots.active, 1);
  assert.equal(ev(s, {ev: 'window', op: 'select', win: 101, w: 0, h: 0}).plots.active, 1, 'not a plot window');
  s = reduce(s, {type: 'selectWindow', win: 2});
  s = reduce(s, {type: 'hover', hover: {curve: 0, row: 0, x: 0, y: 0, t: 0}});
  s = ev(s, plots(1, info(1, 'W vs V')));
  assert.deepEqual(wins(s), [1]);
  assert.equal(s.plots.active, 1);
  assert.equal(s.hover, null, 'the readout was the closed window\'s');
  assert.equal(windowOf(s.plots, 1)?.series?.rows, 2, 'the kept window keeps its series');
});

test('a series before any plots event makes its window (a server without plots)', () => {
  const s = ev(initialState, series(1, 1, 2));
  assert.deepEqual(wins(s), [1]);
  assert.equal(activeWindow(s.plots)?.info, null);
});

test('a plot mode (T4) is its window\'s, and shows that window\'s tab', () => {
  let s = reduce(twoWindows(), {type: 'selectWindow', win: 1});
  const view = {win: 2, left: 50, right: 550, top: 20, bottom: 420, xlo: 0, xhi: 1, ylo: 0, yhi: 1, three: 0};
  s = ev(s, {ev: 'state', pars: [], ics: [], bcs: [], view, rows: 2, menu: 0, win: 2});
  s = ev(s, {ev: 'ask', id: 5, kind: 'rubber', win: 2});
  assert.equal(s.pick?.win, 2);
  assert.equal(s.plots.active, 2);
  s = ev(s, {ev: 'ask', id: 6, kind: 'rubber', win: 1});
  assert.equal(s.pick, null, 'not the window state.view maps');
});

test('the core moving a window\'s axes resets that window\'s zoom only, undoably', () => {
  let s = twoWindows();
  s = reduce(s, {type: 'viewport', viewport: zoom, win: 1});
  s = reduce(s, {type: 'viewport', viewport: zoom, win: 2});
  const view = {win: 2, left: 0, right: 1, top: 0, bottom: 1, xlo: 0, xhi: 1, ylo: 0, yhi: 1, three: 0};
  s = ev(s, {ev: 'state', pars: [], ics: [], bcs: [], view, rows: 2, menu: 0, win: 2});
  s = ev(s, {ev: 'state', pars: [], ics: [], bcs: [], view: {...view, xhi: 2}, rows: 2, menu: 0, win: 2});
  assert.deepEqual(windowOf(s.plots, 2)?.viewport, {x: null, y: null});
  assert.deepEqual(windowOf(s.plots, 1)?.viewport, zoom);
  s = reduce(s, {type: 'undoViewport', win: 2});
  assert.deepEqual(windowOf(s.plots, 2)?.viewport, zoom);
});
