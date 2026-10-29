/* Actions enabled by kind (W95, protocol/kinds.ts): what may start while a
   computation runs, by the kinds hello carries (npm test). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {kindOf, mayStart} from '../src/protocol/kinds';
import type {Command} from '../src/protocol/types';
import {initialState, reduce, type AppState} from '../src/store/state';
import {HELLO} from './hello';

const may = (cmd: Command, computing: boolean, asking = false) =>
  mayStart(kindOf(HELLO, cmd), computing, asking);

test('W95: each action has the kind hello gives it', () => {
  assert.equal(kindOf(HELLO, {cmd: 'run.initial'}), 'computation');
  assert.equal(kindOf(HELLO, {cmd:'window.create'}), 'view');
  assert.equal(kindOf(HELLO, {cmd:'auto.run'}), 'computation');
  assert.equal(kindOf(HELLO, {cmd:'key',key:'g'}), null);
  assert.equal(kindOf(HELLO, {cmd: 'browser.export', file: 'x'}), 'data');
  assert.equal(kindOf(HELLO, {cmd: 'browser.page', from_row: 0, count: 10}), 'view');
  assert.equal(kindOf(HELLO, {cmd: 'tool.run_user_button', index: 0}), 'computation');
  assert.equal(kindOf(HELLO, {cmd: 'nothing'}), null);
  assert.equal(kindOf(null, {cmd: 'display'}), null);
});

test('startup prompts accept answers and cancellation before hello', () => {
  for (const cmd of [{cmd: 'answer', id: 1, file: 'lecar.odex'}, {cmd: 'run.abort'}, {cmd: 'session.quit'}])
    assert.ok(mayStart(kindOf(null, cmd), false, true), JSON.stringify(cmd));
  assert.ok(!mayStart(kindOf(null, {cmd: 'run.steady'}), false, true));
});

test('W95: controls, views and settings run during a computation; questions allow controls and settings', () => {
  for (const cmd of ['run.abort','window.create','run.configure','auto.configure_numerics']) assert.ok(may({cmd},true));
  for (const cmd of ['run.initial','browser.export','auto.run','unknown']) assert.ok(!may({cmd},true));
  assert.ok(may({cmd:'run.configure'},true,true));
  assert.ok(may({cmd:'answer',id:1},false,true));
  assert.ok(!may({cmd:'window.create'},false,true));
  assert.ok(!may({cmd:'run.initial'},false,true));
});

test('W95: computing from the core\'s event to the command\'s done; a command the page sends marks nothing', () => {
  const ev = (s: AppState, e: object) => reduce(s, {type: 'event', ev: e as never});
  let s = ev(initialState, HELLO);
  s = reduce(s, {type: 'sent', cmd: {cmd: 'display', win: 'graph_1', x: null}});
  assert.equal(s.busy, true);
  assert.equal(s.computing, false);
  s = ev(ev(s, {ev: 'done', status: 'ok'}), {ev: 'job.computing'});
  assert.equal(s.computing, true);
  assert.equal(ev(s, {ev: 'done', status: 'ok'}).computing, false);
  s = reduce(reduce(s, {type: 'connection', open: true}), {type: 'event', ev: {ev: 'job.computing'}});
  assert.equal(reduce(s, {type: 'connection', open: false}).computing, false);
  /* the AUTO view's Run is named by its key from hello */
  s = reduce(ev(s, {ev: 'done', status: 'ok'}), {type: 'sent', cmd: {cmd:'auto.run'}});
  assert.equal(s.running, 'AUTO');
});
