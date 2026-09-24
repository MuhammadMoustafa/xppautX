/* The reducer and the plot model, without a browser (npm test). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {buildModel} from '../src/plot/model';
import {nearestPoint} from '../src/plot/nearest';
import {plotKey} from '../src/plot/plotKeys';
import {zoomAbout} from '../src/plot/viewmath';
import type {SeriesEvent} from '../src/protocol/types';
import {activeWindow} from '../src/store/plots';
import {classifyLogText, initialState, reduce, type AppState} from '../src/store/state';

const phase: SeriesEvent = {
  ev: 'series', win: 1, rows: 3, three: 0, xlabel: '', ylabel: '', zlabel: '',
  curves: [{x: 1, y: 2, z: 1, color: 0, line: 1}], shift: [0, 0, 0],
  columns: [
    {col: 0, name: 'T', data: [0, 0.05, 0.1]},
    {col: 1, name: 'V', data: [-0.144, -0.1438, null]},
    {col: 2, name: 'W', data: [0.03, 0.0301, 0.0302]},
  ],
};

const ev = (s: AppState, e: object) => reduce(s, {type: 'event', ev: e as never});
const shown = (s: AppState) => activeWindow(s.plots)!;

test('a series event becomes typed columns', () => {
  const s = ev(initialState, phase);
  assert.equal(s.seriesCount, 1);
  assert.equal(shown(s).series!.rows, 3);
  assert.deepEqual([...shown(s).series!.columns.keys()], [0, 1, 2]);
  assert.ok(Number.isNaN(shown(s).series!.columns.get(1)![2]), 'null is NaN');
});

test('the zoom survives new data for the same curves, not other curves', () => {
  let s = ev(initialState, phase);
  s = reduce(s, {type: 'viewport', viewport: {x: {min: 0, max: 1}, y: null}});
  s = ev(s, phase);
  assert.deepEqual(shown(s).viewport.x, {min: 0, max: 1});
  s = ev(s, {...phase, curves: [{x: 0, y: 1, z: 0, color: 0, line: 1}]});
  assert.equal(shown(s).viewport.x, null);
});

test('busy from a command to its idle; an answer closes the ask', () => {
  let s = reduce(initialState, {type: 'sent', cmd: {cmd: 'key', key: 'i'}});
  assert.equal(s.busy, true);
  s = ev(s, {ev: 'ask', id: 3, kind: 'menu'});
  assert.equal(s.ask?.id, 3);
  s = reduce(s, {type: 'sent', cmd: {cmd: 'answer', id: 3, key: 'g'}});
  assert.equal(s.ask, null);
  assert.equal(s.busy, true);
  s = ev(s, {ev: 'idle'});
  assert.equal(s.busy, false);
  assert.equal(reduce(s, {type: 'sent', cmd: {cmd: 'abort'}}), s, 'abort has no idle');
});

test('a phase plane is an xy plot, a time plot an aligned one', () => {
  const s = ev(initialState, phase);
  const m = buildModel(shown(s).series!);
  assert.equal(m.mode, 2);
  assert.equal(m.curves[0].label, 'W vs V');
  assert.equal(m.xLabel, 'V');
  const t = ev(initialState, {...phase, curves: [{x: 0, y: 1, z: 0, color: 0, line: 1}]});
  assert.equal(buildModel(shown(t).series!).mode, 1);
});

test('lag plots drop the rows before the shift', () => {
  const s = ev(initialState, {...phase, shift: [1, 0, 0]});
  const c = buildModel(shown(s).series!).curves[0];
  assert.equal(c.row0, 1);
  /* V of rows 0, 1 against W of rows 1, 2 (float32, as the core stores them) */
  assert.deepEqual([...c.xs], [-0.144, -0.1438].map(Math.fround));
  assert.deepEqual([...c.ys], [0.0301, 0.0302].map(Math.fround));
});

test('the nearest point is found in screen distance', () => {
  const m = buildModel(shown(ev(initialState, {...phase, columns: [phase.columns[0],
    {col: 1, name: 'V', data: [0, 1, 2]}, {col: 2, name: 'W', data: [0, 10, 20]}]})).series!);
  const frame = {xmin: 0, xmax: 2, ymin: 0, ymax: 20, width: 200, height: 200};
  assert.deepEqual(nearestPoint(m.curves, [true], frame, 100, 100, 24), {curve: 0, index: 1, dist: 0});
  assert.equal(nearestPoint(m.curves, [true], frame, 150, 20, 24), null);
  assert.equal(nearestPoint(m.curves, [false], frame, 100, 100, 24), null);
});

test('zooms can be undone, one gesture at a time', () => {
  const a = {x: {min: 0, max: 1}, y: null}, b = {x: {min: 0, max: 0.5}, y: null};
  let s = reduce(ev(initialState, phase), {type: 'viewport', viewport: a, push: true});
  s = reduce(s, {type: 'viewport', viewport: b, push: false}); /* the same gesture going on */
  assert.equal(shown(s).viewportHistory.length, 1);
  s = reduce(s, {type: 'undoViewport'});
  assert.deepEqual(shown(s).viewport, {x: null, y: null});
  assert.equal(reduce(s, {type: 'undoViewport'}), s);
});

test('errors and alerts become notifications; Abort shows Stopping until idle', () => {
  let s = ev(initialState, {ev: 'message', error: 'bad formula'});
  assert.deepEqual(s.toasts.map(t => [t.kind, t.text]), [['error', 'bad formula']]);
  s = reduce(s, {type: 'dismiss', id: s.toasts[0].id});
  assert.equal(s.toasts.length, 0);
  s = reduce(s, {type: 'sent', cmd: {cmd: 'key', key: 'i'}});
  s = reduce(s, {type: 'aborting'});
  assert.equal(s.stopping, true);
  assert.equal(ev(s, {ev: 'idle'}).stopping, false);
});

test('the plot keys pan, zoom, reset and step through points', () => {
  const ranges = {x: {min: 0, max: 10}, y: {min: 0, max: 10}};
  const ctx = {ranges, hover: null, counts: [5, 0, 3]};
  assert.deepEqual(plotKey('ArrowRight', ctx), {view: {x: {min: 1, max: 11}, y: {min: 0, max: 10}}});
  assert.deepEqual(plotKey('+', ctx), {view: {x: {min: 1, max: 9}, y: {min: 1, max: 9}}});
  assert.deepEqual(plotKey('0', ctx), {reset: true});
  assert.deepEqual(plotKey(']', ctx), {hover: {curve: 0, index: 0}});
  assert.deepEqual(plotKey(']', {...ctx, hover: {curve: 0, index: 4}}), {hover: {curve: 0, index: 4}});
  assert.deepEqual(plotKey('}', {...ctx, hover: {curve: 0, index: 4}}), {hover: {curve: 2, index: 2}}, 'skips hidden');
  assert.deepEqual(plotKey('End', {...ctx, hover: {curve: 2, index: 0}}), {hover: {curve: 2, index: 2}});
  assert.deepEqual(plotKey('Escape', {...ctx, hover: {curve: 0, index: 1}}), {hover: null});
  assert.equal(plotKey('Escape', ctx), null, 'Escape goes on to XPP');
  assert.equal(plotKey('i', ctx), null, 'letters are XPP hotkeys');
});

test('a set that the core rejects lands on the pending value field, not only the toast (T3)', () => {
  let s = reduce(initialState, {type: 'values', action: {type: 'edit', edit: {kind: 'par', name: 'iapp', previous: '0.05'}}});
  s = ev(s, {ev: 'message', error: 'bad formula'});
  assert.equal(s.values.errors['par:iapp'], 'bad formula');
  assert.deepEqual(s.toasts.map(t => t.text), ['bad formula'], 'still a non-modal toast too (A11)');
  s = ev(s, {ev: 'idle'});
  assert.equal(s.values.pending, null);
});

test('the values panel is a sheet the store tracks for narrow screens', () => {
  let s = reduce(initialState, {type: 'valuesPanel', open: true});
  assert.equal(s.valuesOpen, true);
  assert.equal(reduce(s, {type: 'valuesPanel', open: true}), s);
  s = reduce(s, {type: 'valuesPanel', open: false});
  assert.equal(s.valuesOpen, false);
});

test('a text/source/equilibrium event lands in the text slice (T16)', () => {
  let s = ev(initialState, {ev: 'equations', lines: ["V'=I-W"]});
  assert.deepEqual(s.text.equations, ["V'=I-W"]);
  s = ev(s, {ev: 'source', lines: ['v\'=-v', '" * set iapp=0.1'], comments: [['* set iapp=0.1', 1]]});
  assert.equal(s.text.source!.lines[1].comment?.hasAction, true);
  s = ev(s, {
    ev: 'equilibrium', type: 'STABLE', cplus: 0, cminus: 2, rplus: 0, rminus: 0, im: 0,
    values: [['v', -0.144]],
  });
  assert.equal(s.text.equilibrium!.type, 'STABLE');
});

test('the text panel open/tab state is tracked like the other panels', () => {
  let s = reduce(initialState, {type: 'text', action: {type: 'open', open: true}});
  assert.equal(s.text.open, true);
  s = reduce(s, {type: 'text', action: {type: 'tab', tab: 'equilibrium'}});
  assert.equal(s.text.tab, 'equilibrium');
});

test('classifyLogText tells AUTO\'s console table apart from the rest of the log', () => {
  assert.equal(classifyLogText('  BR    PT  TY LAB '), 'auto');
  assert.equal(classifyLogText('   1     1  EP   1  1.000000E-01  2.000000E-01'), 'auto');
  assert.equal(classifyLogText('Generating starting data :'), 'auto');
  assert.equal(classifyLogText('Hopf point\n'), 'auto');
  assert.equal(classifyLogText('nvar=2 naux=4 nfix=0 nmark=0 NEQ=6 NODE=6'), 'log');
  assert.equal(classifyLogText('All formulas are valid!!\n'), 'log');
});

test('a log event is classified when it is added (Messages: AUTO output distinguishable)', () => {
  const s = ev(initialState, {ev: 'log', text: '  BR    PT  TY LAB \n'});
  assert.equal(s.log[0].kind, 'auto');
  const s2 = ev(initialState, {ev: 'log', text: 'nvar=2 naux=4\n'});
  assert.equal(s2.log[0].kind, 'log');
});

test('T27: the rows NPr prints (no type) are AUTO\'s, and a chunk of several lines is classified line by line', () => {
  assert.equal(classifyLogText('   1    50       2  1.077149E-01  3.745827E-01 -3.658905E-01  8.022678E-02\n'), 'auto');
  assert.equal(classifyLogText('   2  2250      47  3.129853E-01  1.000000E+00  2.500000E+01\n'), 'auto');
  const chunk = 'nvar=2 naux=4\n   1     5       2  1.077149E-01  3.745827E-01\n   1    10       3  1.682993E-01  3.212211E-01\n'
    + '   1    19  HB   4  2.624638E-01  2.891081E-01\n   1    2';
  let s = ev(initialState, {ev: 'log', text: chunk});
  s = ev(s, {ev: 'log', text: '0       5  2.725202E-01  2.911251E-01\nAll formulas are valid!!\n'});
  assert.deepEqual(s.log.map(l => [l.kind, l.text.trim().slice(0, 12)]), [['log', 'nvar=2 naux='], ['auto', '1     5     '],
    ['auto', '1    10     '], ['auto', '1    19  HB '], ['auto', '1    20     '], ['log', 'All formulas']]);
});

test('zoomAbout keeps the point under the pointer', () => {
  const r = zoomAbout({x: {min: 0, max: 10}, y: {min: 0, max: 10}}, 0.2, 0.5, 0.5);
  assert.deepEqual(r, {x: {min: 1, max: 6}, y: {min: 2.5, max: 7.5}});
});
