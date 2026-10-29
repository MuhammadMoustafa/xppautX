/* Actions enabled by kind (W95, protocol/kinds.ts): what may start while a
   computation runs, by the kinds hello carries (npm test). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {isWindowKey, kindOf, layerKey, mayStart, windowCommand} from '../src/protocol/kinds';
import type {Command} from '../src/protocol/types';
import {initialState, reduce, type AppState} from '../src/store/state';
import {HELLO} from './hello';

const may = (cmd: Command, computing: boolean, asking = false, menu = 0) =>
  mayStart(kindOf(HELLO, menu, cmd), computing, asking);

test('W95: each action has the kind hello gives it', () => {
  assert.equal(kindOf(HELLO, 0, {cmd: 'run.initial'}), 'computation');
  assert.equal(kindOf(HELLO, 0, {cmd: 'key', key: 'w'}), 'view');
  assert.equal(kindOf(HELLO, 1, {cmd: 'key', key: 's'}), 'data'); /* File/Save info */
  assert.equal(kindOf(HELLO, 1, {cmd: 'key', key: 'q'}), 'control');
  assert.equal(kindOf(HELLO, 0, {cmd: 'key', key: 'Escape'}), 'control');
  assert.equal(kindOf(HELLO, 0, {cmd: 'key', win: 'auto', key: 'r'}), 'computation');
  assert.equal(kindOf(HELLO, 0, {cmd: 'key', win: 'auto', key: 'a'}), 'view');
  assert.equal(kindOf(HELLO, 0, {cmd: 'key', win: 'auto', key: 'f'}), 'view'); /* AUTO's File: a menu holding a view */
  assert.equal(kindOf(HELLO, 0, {cmd: 'browser.export', file: 'x'}), 'data');
  assert.equal(kindOf(HELLO, 0, {cmd: 'browser.page', from_row: 0, count: 10}), 'view');
  assert.equal(kindOf(HELLO, 0, {cmd: 'userbut', index: 0}), 'computation');
  assert.equal(kindOf(HELLO, 0, {cmd: 'nothing'}), null);
  assert.equal(kindOf(null, 0, {cmd: 'display'}), null);
});

test('startup prompts accept answers and cancellation before hello', () => {
  for (const cmd of [{cmd: 'answer', id: 1, file: 'lecar.odex'}, {cmd: 'run.abort'}, {cmd: 'quit'}])
    assert.ok(mayStart(kindOf(null, 0, cmd), false, true), JSON.stringify(cmd));
  assert.ok(!mayStart(kindOf(null, 0, {cmd: 'run.steady'}), false, true));
});

test('W95, W106: while a computation runs only control, view and setting actions start; an open question takes control and settings', () => {
  for (const cmd of [{cmd: 'run.abort'}, {cmd: 'answer', id: 1}, {cmd: 'display', window: 'graph_1'}, {cmd: 'window.select', window: 'graph_2'},
    {cmd: 'key', key: 'w'}, {cmd: 'key', key: 'k'}, {cmd: 'key', key: 'g'}, {cmd: 'key', win: 'auto', key: 'a'}, {cmd: 'browser.page', from_row: 0, count: 1},
    {cmd: 'set', kind: 'par'}, {cmd: 'set', kind: 'num'}, {cmd: 'auto.configure_numerics'}, {cmd: 'key', key: 'p'},
    {cmd: 'key', win: 'auto', key: 'n'}, {cmd: 'value.load_file'}])
    assert.ok(may(cmd, true), JSON.stringify(cmd));
  for (const cmd of [{cmd: 'run.initial'}, {cmd: 'key', key: 's'}, {cmd: 'value.save_file'}, {cmd: 'auto.grab'},
    {cmd: 'browser.export'}, {cmd: 'key', win: 'auto', key: 'r'}, {cmd: 'key', win: 'ani', key: 'a'}, {cmd: 'unknown'}])
    assert.ok(!may(cmd, true), JSON.stringify(cmd));
  /* the Numerics menu's items that ask a value are settings: their dialog opens when the run ends */
  assert.equal(kindOf(HELLO, 2, {cmd: 'key', key: 'n'}), 'setting');
  assert.ok(may({cmd: 'key', key: 'n'}, true, false, 2));
  assert.ok(may({cmd: 'run.configure'}, true), 'run.configure is a setting');
  assert.ok(!may({cmd: 'key', key: 'h'}, true, false, 2), 'stocHast: data');
  assert.ok(may({cmd: 'set', kind: 'par'}, true, true), 'a setting goes while a question is open too');
  /* not computing (the page's own catch-up only, or idle): everything starts */
  for (const cmd of [{cmd: 'run.initial'}, {cmd: 'value.save_file'}, {cmd: 'set', kind: 'par'}])
    assert.ok(may(cmd, false), JSON.stringify(cmd));
  assert.ok(may({cmd: 'answer', id: 1}, false, true));
  assert.ok(!may({cmd: 'display', win: 'graph_1'}, false, true));
  assert.ok(!may({cmd: 'run.initial'}, false, true));
});

test('W95: the windows\' keys come from hello, not a copy in the page', () => {
  assert.equal(layerKey(HELLO, 'auto', 'run'), 'r');
  assert.equal(layerKey(HELLO, 'ani', 'grab'), 'a');
  assert.equal(layerKey(HELLO, 'aplot', 'nothing'), '');
  assert.equal(layerKey(null, 'auto', 'run'), '');
  assert.deepEqual(windowCommand(HELLO, 'auto', 'file', {row: 3}), {cmd: 'key', win: 'auto', key: 'f', row: 3});
  assert.ok(isWindowKey(HELLO, {cmd: 'key', win: 'ani', key: 'g'}, 'ani', 'go'));
  assert.ok(!isWindowKey(HELLO, {cmd: 'key', win: 'ani', key: 'g'}, 'auto', 'grab'));
});

test('W95: computing from the core\'s event to the command\'s idle; a command the page sends marks nothing', () => {
  const ev = (s: AppState, e: object) => reduce(s, {type: 'event', ev: e as never});
  let s = ev(initialState, HELLO);
  s = reduce(s, {type: 'sent', cmd: {cmd: 'display', win: 'graph_1', x: null}});
  assert.equal(s.busy, true);
  assert.equal(s.computing, false);
  s = ev(ev(s, {ev: 'idle'}), {ev: 'computing'});
  assert.equal(s.computing, true);
  assert.equal(ev(s, {ev: 'idle'}).computing, false);
  s = reduce(reduce(s, {type: 'connection', open: true}), {type: 'event', ev: {ev: 'computing'}});
  assert.equal(reduce(s, {type: 'connection', open: false}).computing, false);
  /* the AUTO view's Run is named by its key from hello */
  s = reduce(ev(s, {ev: 'idle'}), {type: 'sent', cmd: {cmd: 'key', win: 'auto', key: 'r'}});
  assert.equal(s.running, 'AUTO');
});
