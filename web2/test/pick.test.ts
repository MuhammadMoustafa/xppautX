/* Prompts (docs/ui-v2.md T4): form fields that pick from hello.lists, the
   plot modes' keyboard maths and answers, and how the store follows them. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {CENTRE, pickAnswer, pickKey, pickModeOf, startPick, STEP, toData, type PickState} from '../src/plot/pick';
import {fieldSpec, listOption, selectOptions} from '../src/protocol/lists';
import type {AskEvent, View} from '../src/protocol/types';
import {initialState, reduce, type AppState} from '../src/store/state';

const view: View = {win: 1, left: 50, right: 550, top: 20, bottom: 420, xlo: -0.6, xhi: 0.5, ylo: -0.1, yhi: 0.5, three: 0};
const ask = (kind: AskEvent['kind'], extra: object = {}): AskEvent => ({ev: 'ask', id: 7, kind, win: 1, ...extra});
const close = (a: number, b: number) => Math.abs(a - b) < 1e-12;

test('a *n field picks from list n; others are text', () => {
  assert.deepEqual(fieldSpec('*0X-axis'), {label: 'X-axis', list: 0});
  assert.deepEqual(fieldSpec('*4Color'), {label: 'Color', list: 4});
  assert.deepEqual(fieldSpec('Xmin'), {label: 'Xmin', list: null});
});

test('numbered items answer with their number, names with themselves', () => {
  assert.deepEqual(listOption('2 Box'), {value: '2', label: '2 Box'});
  assert.deepEqual(listOption('10 CVode'), {value: '10', label: '10 CVode'});
  assert.deepEqual(listOption('V'), {value: 'V', label: 'V'});
});

test('the select starts at the field\'s value, and keeps one the list does not have', () => {
  const vars = ['T', 'V', 'W'];
  assert.equal(selectOptions(vars, 'W').selected, 'W');
  assert.equal(selectOptions(vars, 'w').selected, 'W', 'without regard to case');
  const colours = ['0 Black/White', '1 Red', '2 RedOrange'];
  assert.equal(selectOptions(colours, '1').selected, '1');
  assert.equal(selectOptions(colours, ' 2 ').selected, '2');
  const odd = selectOptions(vars, 'X1');
  assert.equal(odd.selected, 'X1');
  assert.deepEqual(odd.options[0], {value: 'X1', label: 'X1'});
  assert.equal(odd.options.length, 4);
  assert.equal(selectOptions(vars, '').options[0].label, '(none)');
});

test('mouse, rubber and drag asks of the shown 2D window are plot modes', () => {
  assert.equal(pickModeOf(ask('mouse'), view, true), 'point');
  assert.equal(pickModeOf(ask('rubber', {flag: 0}), view, true), 'box');
  assert.equal(pickModeOf(ask('rubber', {flag: 1}), view, true), 'line');
  assert.equal(pickModeOf(ask('drag'), view, true), 'drag');
  assert.equal(pickModeOf(ask('rubber', {win: 101}), view, true), null, 'the AUTO diagram is not shown yet');
  assert.equal(pickModeOf(ask('mouse'), {...view, three: 1}, true), null, 'nor a 3D plot');
  assert.equal(pickModeOf(ask('mouse'), view, false), null, 'nor a plot before the first series');
  assert.equal(pickModeOf(ask('menu'), view, true), null);
});

const box = (): PickState => startPick(null, ask('rubber'), 'box');

test('keyboard box: arrows move the crosshair, Enter fixes a corner, then confirms', () => {
  let p = box();
  assert.deepEqual(p.cursor, CENTRE);
  for (let i = 0; i < 5; i++) p = (pickKey(p, 'ArrowLeft', false) as {pick: PickState}).pick;
  p = (pickKey(p, 'ArrowUp', true) as {pick: PickState}).pick;
  assert.ok(close(p.cursor.fx, 0.5 - 5 * STEP) && close(p.cursor.fy, 0.4), JSON.stringify(p.cursor));
  p = (pickKey(p, 'Enter', false) as {pick: PickState}).pick;
  assert.deepEqual(p.anchor, p.cursor);
  for (let i = 0; i < 3; i++) p = (pickKey(p, 'ArrowRight', true) as {pick: PickState}).pick;
  p = (pickKey(p, 'ArrowDown', true) as {pick: PickState}).pick;
  const r = pickKey(p, 'Enter', false);
  assert.ok(r && 'confirm' in r);
  assert.ok(close(p.cursor.fx, 0.7) && close(p.cursor.fy, 0.5), JSON.stringify(p.cursor));
  assert.deepEqual(pickKey(p, 'Escape', false), {cancel: true});
  assert.equal(pickKey(p, 'q', false), null, 'other keys are not the mode\'s');
});

test('the crosshair stays inside the area', () => {
  let p = box();
  for (let i = 0; i < 40; i++) p = (pickKey(p, 'ArrowRight', true) as {pick: PickState}).pick;
  assert.equal(p.cursor.fx, 1);
});

test('answers are data coordinates of what the plot shows', () => {
  const r = {x: {min: -0.6, max: 0.4}, y: {min: 0, max: 0.5}};
  assert.deepEqual(toData(r, {fx: 0, fy: 0}), {x: -0.6, y: 0.5});
  const p: PickState = {...box(), anchor: {fx: 0.25, fy: 0.2}, cursor: {fx: 0.75, fy: 0.6}};
  const a = pickAnswer(p, r);
  assert.ok(close(a.xd, -0.35) && close(a.yd, 0.4) && close(a.xd2, 0.15) && close(a.yd2, 0.2), JSON.stringify(a));
  const pt = pickAnswer({...startPick(null, ask('mouse'), 'point'), cursor: {fx: 0.5, fy: 0.5}}, r);
  assert.deepEqual(Object.keys(pt), ['xd', 'yd']);
});

test('a drag by the keyboard is down, move and up the other way from the middle', () => {
  const p = startPick(null, ask('drag'), 'drag');
  const r = pickKey(p, 'ArrowLeft', false) as {drag: {what: string; at: {fx: number; fy: number}}[]};
  assert.deepEqual(r.drag.map(d => d.what), ['down', 'move', 'up']);
  assert.ok(close(r.drag[1].at.fx, 0.5 + STEP) && close(r.drag[1].at.fy, 0.5));
  assert.deepEqual(pickKey(p, 'Enter', false), {cancel: true}, 'Enter ends the drag');
});

const ev = (s: AppState, e: object) => reduce(s, {type: 'event', ev: e as never});
const withPlot = (): AppState => {
  let s = ev(initialState, {ev: 'state', pars: [], ics: [], bcs: [], view, rows: 0, menu: 0, win: 1});
  s = ev(s, {ev: 'series', win: 1, rows: 0, three: 0, xlabel: '', ylabel: '', zlabel: '',
    curves: [{x: 1, y: 2, z: 1, color: 0, line: 1}], shift: [0, 0, 0],
    columns: [{col: 0, name: 'T', data: []}, {col: 1, name: 'V', data: []}, {col: 2, name: 'W', data: []}]});
  return s;
};

test('the store: a rubber ask starts a box, its answer ends it, idle clears it', () => {
  let s = ev(withPlot(), ask('rubber'));
  assert.equal(s.pick?.mode, 'box');
  assert.equal(s.pick?.ask, 7);
  s = reduce(s, {type: 'sent', cmd: {cmd: 'answer', id: 7, xd: 0, yd: 0, xd2: 1, yd2: 1}});
  assert.equal(s.ask, null);
  assert.equal(s.pick?.waiting, true);
  s = ev(s, {ev: 'idle'});
  assert.equal(s.pick, null);
});

test('the store: a cancelled mode is gone at once; a drag lasts over its asks', () => {
  let s = ev(withPlot(), ask('mouse'));
  s = reduce(s, {type: 'sent', cmd: {cmd: 'answer', id: 7, ok: 0}});
  assert.equal(s.pick, null);
  s = ev(s, ask('drag', {id: 8}));
  s = reduce(s, {type: 'pick', pick: {...s.pick!, cursor: {fx: 0.2, fy: 0.3}}});
  s = reduce(s, {type: 'sent', cmd: {cmd: 'answer', id: 8, what: 'down', xd: 0, yd: 0}});
  assert.equal(s.pick?.mode, 'drag');
  assert.equal(s.pick?.waiting, false);
  s = ev(s, ask('drag', {id: 9}));
  assert.equal(s.pick?.ask, 9);
  assert.deepEqual(s.pick?.cursor, {fx: 0.2, fy: 0.3}, 'the same mode keeps its crosshair');
  s = ev(s, ask('menu', {id: 10}));
  assert.equal(s.pick, null, 'any other ask ends it');
});

test('the core\'s window moving (Window/Zoom) shows it: the client zoom is undoable, not kept', () => {
  let s = withPlot();
  s = reduce(s, {type: 'viewport', viewport: {x: {min: 0, max: 0.1}, y: null}, push: true});
  s = ev(s, {ev: 'state', pars: [], ics: [], bcs: [], view, rows: 0, menu: 0, win: 1});
  assert.deepEqual(s.viewport.x, {min: 0, max: 0.1}, 'the same window: the zoom stays');
  s = ev(s, {ev: 'state', pars: [], ics: [], bcs: [], view: {...view, xlo: -0.2}, rows: 0, menu: 0, win: 1});
  assert.equal(s.viewport.x, null);
  s = reduce(s, {type: 'undoViewport'});
  assert.deepEqual(s.viewport.x, {min: 0, max: 0.1});
});

test('the core\'s hint box is kept until the command ends', () => {
  let s = ev(withPlot(), {ev: 'message', box: 'Click on initial data'});
  assert.equal(s.box, 'Click on initial data');
  s = ev(s, {ev: 'idle'});
  assert.equal(s.box, '');
});
