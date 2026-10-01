/* The reducer and the plot model, without a browser (npm test). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {buildModel} from '../src/plot/model';
import {nearestPoint} from '../src/plot/nearest';
import {plotKey} from '../src/plot/plotKeys';
import {zoomAbout} from '../src/plot/viewmath';
import type {SeriesEvent} from '../src/protocol/types';
import {activeWindow} from '../src/store/plots';
import {busyText, classifyLogText, initialState, LEAVE_ASK, noIdle, reduce, type AppState} from '../src/store/state';
import {HELLO, READY} from './hello';
import {logEntries} from '../src/store/log';

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
  const s = ev(READY, phase);
  assert.equal(s.seriesCount, 1);
  assert.equal(shown(s).series!.rows, 3);
  assert.deepEqual([...shown(s).series!.columns.keys()], [0, 1, 2]);
  assert.ok(Number.isNaN(shown(s).series!.columns.get(1)![2]), 'null is NaN');
});

test('the zoom survives new data for the same curves, not other curves', () => {
  let s = ev(READY, phase);
  s = reduce(s, {type: 'viewport', viewport: {x: {min: 0, max: 1}, y: null}});
  s = ev(s, phase);
  assert.deepEqual(shown(s).viewport.x, {min: 0, max: 1});
  s = ev(s, {...phase, curves: [{x: 0, y: 1, z: 0, color: 0, line: 1}]});
  assert.equal(shown(s).viewport.x, null);
});

test('busy from a command to its idle; an answer closes the ask', () => {
  let s = reduce(READY, {type: 'sent', cmd: {cmd: 'key', key: 'i'}});
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
  const s = ev(READY, phase);
  const m = buildModel(shown(s).series!);
  assert.equal(m.mode, 2);
  assert.equal(m.curves[0].label, 'W vs V');
  assert.equal(m.xLabel, 'V');
  const t = ev(READY, {...phase, curves: [{x: 0, y: 1, z: 0, color: 0, line: 1}]});
  assert.equal(buildModel(shown(t).series!).mode, 1);
});

test('lag plots drop the rows before the shift', () => {
  const s = ev(READY, {...phase, shift: [1, 0, 0]});
  const c = buildModel(shown(s).series!).curves[0];
  assert.equal(c.row0, 1);
  /* V of rows 0, 1 against W of rows 1, 2 (float32, as the core stores them) */
  assert.deepEqual([...c.xs], [-0.144, -0.1438].map(Math.fround));
  assert.deepEqual([...c.ys], [0.0301, 0.0302].map(Math.fround));
});

test('the nearest point is found in screen distance', () => {
  const m = buildModel(shown(ev(READY, {...phase, columns: [phase.columns[0],
    {col: 1, name: 'V', data: [0, 1, 2]}, {col: 2, name: 'W', data: [0, 10, 20]}]})).series!);
  const frame = {xmin: 0, xmax: 2, ymin: 0, ymax: 20, width: 200, height: 200};
  assert.deepEqual(nearestPoint(m.curves, [true], frame, 100, 100, 24), {curve: 0, index: 1, dist: 0});
  assert.equal(nearestPoint(m.curves, [true], frame, 150, 20, 24), null);
  assert.equal(nearestPoint(m.curves, [false], frame, 100, 100, 24), null);
});

test('a zoom just replaces the viewport, no history (GitHub #110)', () => {
  const a = {x: {min: 0, max: 1}, y: null}, b = {x: {min: 0, max: 0.5}, y: null};
  let s = reduce(ev(READY, phase), {type: 'viewport', viewport: a});
  s = reduce(s, {type: 'viewport', viewport: b});
  assert.deepEqual(shown(s).viewport, b);
});

test('errors and alerts become notifications; Abort shows Stopping until idle', () => {
  let s = ev(READY, {ev: 'message', error: 'bad formula'});
  assert.deepEqual(s.toasts.map(t => [t.kind, t.text]), [['error', 'bad formula']]);
  s = reduce(s, {type: 'dismiss', id: s.toasts[0].id});
  assert.equal(s.toasts.length, 0);
  s = reduce(s, {type: 'sent', cmd: {cmd: 'key', key: 'i'}});
  s = reduce(s, {type: 'aborting'});
  assert.equal(s.stopping, true);
  assert.equal(ev(s, {ev: 'idle'}).stopping, false);
});

test('W68: the status line names what runs (the menu item answered) and that Escape stops it', () => {
  let s = reduce(READY, {type: 'sent', cmd: {cmd: 'key', key: 'i'}});
  assert.equal(s.running, null);
  s = ev(s, {ev: 'ask', id: 3, kind: 'menu', items: ['(R)ange', '(L)ast', '(G)o'], keys: 'rlg'});
  assert.equal(busyText(s.running, s.ask !== null), 'Working…');
  s = reduce(s, {type: 'sent', cmd: {cmd: 'answer', id: 3, key: 'g'}});
  assert.equal(s.running, 'Go');
  assert.equal(busyText(s.running, s.ask !== null), 'Running Go… Esc stops');
  assert.equal(busyText(null, false), 'Working… Esc stops');
  assert.equal(ev(s, {ev: 'idle'}).running, null);
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

test('a set that the core rejects lands on its value field, not only the toast (T3)', () => {
  let s = reduce(READY, {type: 'values', action: {type: 'sent', set: {kind: 'par', name: 'iapp', text: '%x'}, ahead: 1}});
  /* a computation's error, before the set's own command: not the field's */
  s = ev(s, {ev: 'message', error: 'integration failed'});
  assert.deepEqual(s.values.errors, {});
  s = ev(s, {ev: 'idle'});
  s = ev(s, {ev: 'message', error: 'bad formula'});
  assert.equal(s.values.errors['par:iapp'], 'bad formula');
  /* the field shows it and takes the focus back (WF-001): no error dialog for it, only for the
     computation's error (W104 review); both are in Messages */
  assert.deepEqual(s.toasts.map(t => t.text), ['integration failed']);
  assert.deepEqual(logEntries(s.log).filter(l => l.kind === 'error').map(l => l.text), ['integration failed', 'bad formula']);
  /* its own idle ends it */
  s = ev(s, {ev: 'idle'});
  assert.deepEqual(s.values.inflight, []);
});

test('the numerics event is the store (W106); a new hello forgets it', () => {
  const fields = [{key: 'total', label: 'Total', value: 20}, {key: 'method', label: 'Method', value: 3, choices: ['a', 'b', 'c', 'd']}];
  let s = ev(READY, {ev: 'numerics', fields});
  assert.deepEqual(s.numerics, fields);
  s = ev(s, HELLO);
  assert.equal(s.numerics, null);
});

test('the values panel is a sheet the store tracks for narrow screens', () => {
  let s = reduce(READY, {type: 'valuesPanel', open: true});
  assert.equal(s.valuesOpen, true);
  assert.equal(reduce(s, {type: 'valuesPanel', open: true}), s);
  s = reduce(s, {type: 'valuesPanel', open: false});
  assert.equal(s.valuesOpen, false);
});

test('a text/source/equilibrium event lands in the text slice (T16)', () => {
  let s = ev(READY, {ev: 'equations', lines: ["V'=I-W"]});
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
  let s = reduce(READY, {type: 'text', action: {type: 'open', open: true}});
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
  const s = ev(READY, {ev: 'log', text: '  BR    PT  TY LAB \n'});
  assert.equal(logEntries(s.log)[0].kind, 'auto');
  const s2 = ev(READY, {ev: 'log', text: 'nvar=2 naux=4\n'});
  assert.equal(logEntries(s2.log)[0].kind, 'log');
});

test('T27: the rows NPr prints (no type) are AUTO\'s, and a chunk of several lines is classified line by line', () => {
  assert.equal(classifyLogText('   1    50       2  1.077149E-01  3.745827E-01 -3.658905E-01  8.022678E-02\n'), 'auto');
  assert.equal(classifyLogText('   2  2250      47  3.129853E-01  1.000000E+00  2.500000E+01\n'), 'auto');
  const chunk = 'nvar=2 naux=4\n   1     5       2  1.077149E-01  3.745827E-01\n   1    10       3  1.682993E-01  3.212211E-01\n'
    + '   1    19  HB   4  2.624638E-01  2.891081E-01\n   1    2';
  let s = ev(READY, {ev: 'log', text: chunk});
  s = ev(s, {ev: 'log', text: '0       5  2.725202E-01  2.911251E-01\nAll formulas are valid!!\n'});
  assert.deepEqual(logEntries(s.log).map(l => [l.kind, l.text.trim().slice(0, 12)]), [['log', 'nvar=2 naux='], ['auto', '1     5     '],
    ['auto', '1    10     '], ['auto', '1    19  HB '], ['auto', '1    20     '], ['log', 'All formulas']]);
});

test('zoomAbout keeps the point under the pointer', () => {
  const r = zoomAbout({x: {min: 0, max: 10}, y: {min: 0, max: 10}}, 0.2, 0.5, 0.5);
  assert.deepEqual(r, {x: {min: 1, max: 6}, y: {min: 2.5, max: 7.5}});
});

test('a model that does not load: the error event is kept, with the exit after it (W63c)', () => {
  const e = {ev: 'error', error: "ERROR compiling X'", file: 'bad.ode', line: 3, col: 0, source: "x'=-x+a*"};
  let s = ev(initialState, e);
  assert.deepEqual(s.loadError, e);
  assert.equal(s.hello, null);
  s = ev(s, {ev: 'exit', code: 1});
  assert.equal(s.exited, 1);
  assert.deepEqual(s.loadError, e, 'the exit keeps the error');
});

test('an error the core places: Messages and the status read file:line: what, the dialog keeps the place (W140)', () => {
  const placed = {ev: 'message', error: '@ total=abc: not a number', file: 'opts.inc', line: 1, col: 0, source: '@ total=abc'};
  let s = ev(READY, placed);
  const t = s.toasts[s.toasts.length - 1];
  assert.equal(t.text, '@ total=abc: not a number');
  assert.deepEqual(t.place, {file: 'opts.inc', line: 1, col: 0, source: '@ total=abc'});
  assert.equal(t.action, undefined, 'a file read at a line is not one to add');
  assert.equal(s.bottom, 'opts.inc:1: @ total=abc: not a number');
  assert.equal(logEntries(s.log).filter(l => l.kind === 'error').pop()?.text, 'opts.inc:1: @ total=abc: not a number');
  s = ev(s, {ev: 'message', error: 'no place', file: '', line: 0, col: 0, source: ''});
  assert.equal(s.toasts[s.toasts.length - 1].place, undefined, 'no file and no line: no place');
  assert.equal(s.bottom, 'no place');
});

test('errors wait in one error dialog until OK; a warning flashes the status, no dialog (W104)', () => {
  let s = ev(READY, {ev: 'message', error: 'Empty diagram -- nothing to save'});
  const errors = (x: AppState) => x.toasts.filter(t => t.kind === 'error').map(t => t.text);
  assert.deepEqual(errors(s), ['Empty diagram -- nothing to save']);
  s = ev(s, {ev: 'message', error: 'bad formula'});
  assert.deepEqual(errors(s), ['Empty diagram -- nothing to save', 'bad formula'], 'two errors, one dialog');
  assert.deepEqual(logEntries(s.log).filter(l => l.kind === 'error').length, 2, 'both stay in Messages');
  s = reduce(s, {type: 'dismissErrors'});
  assert.deepEqual(errors(s), []);
  assert.equal(logEntries(s.log).filter(l => l.kind === 'error').length, 2, 'OK leaves Messages alone');
  const w = ev(s, {ev: 'log', text: 'Warning: something odd\n'});
  assert.equal(w.flash, s.flash + 1);
  assert.deepEqual(errors(w), []);
  assert.equal(ev(s, {ev: 'message', bottom: 'Working'}).flash, s.flash, 'progress does not flash');
  assert.equal(ev(s, {ev: 'log', text: '  1    1  EP    1   0.5E+00   0.1E+00   0.2E+00\n'}).flash, s.flash, 'AUTO output does not flash');
});

test("the page's leave question (W110): worded by hello, kept over the run's idle, replaced by a core question", () => {
  const quit = {question: 'Quit xppautX? Save this session first?',
    recording: 'Quit xppautX? Save this session, and the recording in progress, first?',
    choices: ['Save session', "Don't save"], keys: 'sd'};
  assert.equal(reduce(initialState, {type: 'leave', open: true}).ask, null, 'no hello, no question');
  let s = ev(READY, {...HELLO, quit});
  s = reduce(ev(s, {ev: 'computing'}), {type: 'leave', open: true});
  assert.deepEqual(s.ask && [s.ask.id, s.ask.kind, s.ask.question, s.ask.choices, s.ask.keys],
    [LEAVE_ASK, 'choice', quit.question, quit.choices, 'sd']);
  assert.equal(s.computing, true, 'the run goes on');
  s = ev(s, {ev: 'idle'});
  assert.equal(s.ask?.id, LEAVE_ASK, 'the run ending leaves it open');
  assert.equal(reduce(s, {type: 'leave', open: false}).ask, null);
  assert.equal(ev(s, {ev: 'ask', id: 7, kind: 'file'}).ask?.id, 7, "a question of the core's replaces it");
  const rec = ev(s, {ev: 'state', pars: [], ics: [], recording: {steps: 1, note: ''}});
  assert.equal(reduce(rec, {type: 'leave', open: true}).ask?.question, quit.recording, 'naming the recording in progress');
  assert.equal(noIdle({cmd: 'quit'}), true, 'the plain quit has no idle');
  assert.equal(noIdle({cmd: 'quit', save: true}), false, 'the quit that saves is a command of its own');
  assert.equal(noIdle({cmd: 'quit', ask: true}), false);
});
