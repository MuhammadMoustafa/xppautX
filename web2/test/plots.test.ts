/* The plot windows in the store (docs/ui-v2.md T6): tabs follow `plots`,
   each window keeps its own series and zoom. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import type {PlotWindowInfo, SeriesEvent} from '../src/protocol/types';
import {activeWindow, windowOf} from '../src/store/plots';
import {reduce, type AppState} from '../src/store/state';
import {READY, LINE, xppColor} from './hello';

const ev = (s: AppState, e: object) => reduce(s, {type: 'event', ev: e as never});
const wins = (s: AppState) => s.plots.windows.map(w => w.win);

const series = (win: string, x: number, y: number): SeriesEvent => ({
  ev: 'series', win, rows: 2, three: 0, xlabel: '', ylabel: '', zlabel: '',
  curves: [{x, y, z: 0, color: 'foreground', ...LINE}], shift: [0, 0, 0],
  columns: [{col: 0, name: 'T', data: [0, 1]}, {col: 1, name: 'V', data: [2, 3]}, {col: 2, name: 'W', data: [4, 5]}]
    .filter(c => c.col === 0 || c.col === x || c.col === y),
});

const info = (win: string, title: string): PlotWindowInfo => ({
  win, title, three: 0, xlo: -1, xhi: 1, ylo: -2, yhi: 2,
  box: {xmin: -1, xmax: 1, ymin: -2, ymax: 2, zmin: -1, zmax: 1}, theta: 45, phi: 45, persp: 0, zplane: -1000,
  zview: 1000, curves: [{x: 1, y: 2, z: 1, color: 'foreground', ...LINE}], shift: [0, 0, 0],
});

const plots = (active: string, ...w: PlotWindowInfo[]) => ({ev: 'plots', active, windows: w});
const zoom = {x: {min: 0, max: 1}, y: {min: 0, max: 2}};

function twoWindows(): AppState {
  let s = ev(READY, plots('graph_1', info('graph_1', 'W vs V')));
  s = ev(s, series('graph_1', 1, 2));
  s = ev(s, plots('graph_2', info('graph_1', 'W vs V'), info('graph_2', 'V vs T')));
  return ev(s, series('graph_2', 0, 1));
}

test('plots makes one window per core window, the active one selected', () => {
  const s = twoWindows();
  assert.deepEqual(wins(s), ['graph_1','graph_2']);
  assert.equal(s.plots.active, 'graph_2');
  assert.equal(activeWindow(s.plots)?.info?.title, 'V vs T');
  assert.equal(windowOf(s.plots, 'graph_1')?.series?.curves[0].x, 1, "window 1 keeps its own series");
  assert.equal(windowOf(s.plots, 'graph_2')?.series?.curves[0].x, 0);
  assert.equal(s.seriesCount, 2);
});

test('each window keeps its zoom when the tabs switch', () => {
  let s = twoWindows();
  s = reduce(s, {type: 'selectWindow', win: 'graph_1'});
  s = reduce(s, {type: 'viewport', viewport: zoom});
  s = reduce(s, {type: 'selectWindow', win: 'graph_2'});
  assert.deepEqual(activeWindow(s.plots)?.viewport, {x: null, y: null}, 'window 2 has its own');
  s = reduce(s, {type: 'viewport', viewport: {x: {min: 5, max: 6}, y: null}, win: 'graph_2'});
  s = reduce(s, {type: 'selectWindow', win: 'graph_1'});
  assert.deepEqual(activeWindow(s.plots)?.viewport, zoom);
  assert.deepEqual(windowOf(s.plots, 'graph_2')?.viewport, {x: {min: 5, max: 6}, y: null});
});

test('a new series for another window leaves the shown zoom and readout alone', () => {
  let s = twoWindows();
  s = reduce(s, {type: 'selectWindow', win: 'graph_1'});
  s = reduce(s, {type: 'viewport', viewport: zoom});
  s = reduce(s, {type: 'hover', hover: {curve: 0, row: 1, x: 3, y: 5, t: 1}});
  s = ev(s, series('graph_2', 0, 2));
  assert.deepEqual(activeWindow(s.plots)?.viewport, zoom);
  assert.equal(s.hover?.row, 1);
  s = ev(s, series('graph_1', 0, 1)); /* other curves in the shown window */
  assert.deepEqual(activeWindow(s.plots)?.viewport, {x: null, y: null});
  assert.equal(s.hover, null);
});

test('appends go to their own window', () => {
  let s = twoWindows();
  s = ev(s, {ev: 'series', op: 'append', win: 'graph_1', from: 2, rows: 3, columns: [
    {col: 0, data: [2]}, {col: 1, data: [6]}, {col: 2, data: [7]}]});
  assert.equal(windowOf(s.plots, 'graph_1')?.series?.rows, 3);
  assert.equal(windowOf(s.plots, 'graph_2')?.series?.rows, 2);
  assert.equal(s.seriesAppends, 1);
});

test('the core selecting a window, or destroying one, moves the tabs', () => {
  let s = twoWindows();
  s = ev(s, {ev: 'window', kind: 'graph', op: 'select', win: 'graph_1', w: 0, h: 0});
  assert.equal(s.plots.active, 'graph_1');
  assert.equal(ev(s, {ev: 'window', kind: 'diagram', op: 'select', win: 'diagram_1', w: 0, h: 0}).plots.active, 'graph_1', 'not a plot window');
  s = reduce(s, {type: 'selectWindow', win: 'graph_2'});
  s = reduce(s, {type: 'hover', hover: {curve: 0, row: 0, x: 0, y: 0, t: 0}});
  s = ev(s, plots('graph_1', info('graph_1', 'W vs V')));
  assert.deepEqual(wins(s), ['graph_1']);
  assert.equal(s.plots.active, 'graph_1');
  assert.equal(s.hover, null, 'the readout was the closed window\'s');
  assert.equal(windowOf(s.plots, 'graph_1')?.series?.rows, 2, 'the kept window keeps its series');
});

test('a series before any plots event makes its window (a server without plots)', () => {
  const s = ev(READY, series('graph_1', 1, 2));
  assert.deepEqual(wins(s), ['graph_1']);
  assert.equal(activeWindow(s.plots)?.info, null);
});

test('a plot mode (T4) is its window\'s, and shows that window\'s tab', () => {
  let s = reduce(twoWindows(), {type: 'selectWindow', win: 'graph_1'});
  const view = {win: 'graph_2', left: 50, right: 550, top: 20, bottom: 420, xlo: 0, xhi: 1, ylo: 0, yhi: 1, three: 0};
  s = ev(s, {ev: 'state', sliders: [], pars: [], ics: [], bcs: [], view, rows: 2, menu: 0, win: 'graph_2'});
  s = ev(s, {ev: 'ask', id: 5, kind: 'rubber', win: 'graph_2'});
  assert.equal(s.pick?.win, 'graph_2');
  assert.equal(s.plots.active, 'graph_2');
  s = ev(s, {ev: 'ask', id: 6, kind: 'rubber', win: 'graph_1'});
  assert.equal(s.pick, null, 'not the window state.view maps');
});

test('the core moving a window\'s axes resets that window\'s zoom only, no history (GitHub #110)', () => {
  let s = twoWindows();
  s = reduce(s, {type: 'viewport', viewport: zoom, win: 'graph_1'});
  s = reduce(s, {type: 'viewport', viewport: zoom, win: 'graph_2'});
  const view = {win: 'graph_2', left: 0, right: 1, top: 0, bottom: 1, xlo: 0, xhi: 1, ylo: 0, yhi: 1, three: 0};
  s = ev(s, {ev: 'state', sliders: [], pars: [], ics: [], bcs: [], view, rows: 2, menu: 0, win: 'graph_2'});
  s = ev(s, {ev: 'state', sliders: [], pars: [], ics: [], bcs: [], view: {...view, xhi: 2}, rows: 2, menu: 0, win: 'graph_2'});
  assert.deepEqual(windowOf(s.plots, 'graph_2')?.viewport, {x: null, y: null});
  assert.deepEqual(windowOf(s.plots, 'graph_1')?.viewport, zoom);
});


test('W376: many windows and curves keep their data, selection and styles', () => {
  const windows = Array.from({length: 30}, (_, i) => info(`graph_${i+1}`, `window ${i + 1}`));
  let s = ev(READY, plots('graph_30', ...windows));
  const data = series('graph_30', 0, 1);
  data.curves = Array.from({length: 13}, (_, i) => ({x: 0, y: 1, z: 0, color: xppColor(i % 11), ...LINE, draw: i % 2 ? 'points' : 'line'}));
  s = ev(s, data);
  s = ev(s, {ev: 'window', kind: 'graph', op: 'select', win: 'graph_30'});
  assert.equal(s.plots.windows.length, 30);
  assert.equal(s.plots.active, 'graph_30');
  assert.equal(activeWindow(s.plots)?.series?.curves.length, 13);
  assert.equal(activeWindow(s.plots)?.series?.curves[12].draw, 'line');
  assert.equal(activeWindow(s.plots)?.series?.curves[11].draw, 'points');
});

test('renaming preserves a named graph\'s series, history and zoom', () => {
  let s = twoWindows();
  s = reduce(s, {type:'viewport', win:'graph_2', viewport:zoom});
  const before = activeWindow(s.plots)!;
  s = ev(s, {ev:'window.renamed', window:'graph_2', new_name:'Phase "plane"'});
  const after = activeWindow(s.plots)!;
  assert.equal(after.win, 'Phase "plane"');
  assert.equal(after.series, before.series);
  assert.equal(after.history, before.history);
  assert.equal(after.viewport, before.viewport);
  assert.equal(windowOf(s.plots,'graph_2'),undefined);
});

test('window kind keeps auxiliary panels separate from user graph names', () => {
  let s = ev(READY, plots('animation',info('animation','animation')));
  s = ev(s, {ev:'window',kind:'graph',op:'create',win:'animation'});
  assert.equal(s.ani.open,false);
  s = ev(s, {ev:'window',kind:'animation',op:'create',win:'animation'});
  assert.equal(s.ani.open,true);
  assert.equal(s.plots.active,'animation');
});

test('registry done releases busy and presents its placed error once', () => {
  const s = ev({...twoWindows(),busy:true}, {ev:'done',status:'error',error:'Unknown window "missing"',file:'command',line:1,source:'window.select --window "missing"'});
  assert.equal(s.busy,false);
  assert.equal(s.log.chunks.flat().filter(e=>e.text.includes('Unknown window')).length,1);
});
