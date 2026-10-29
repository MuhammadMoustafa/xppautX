/* The keymap slice (store/keymap.ts, W211): it holds what hello and the `keymap` replies say, and gives
   back the user's differences for a `set`. The file, the grammar and the checks are the core's
   (tests/test_keymap.cpp, tools/servercheck.py). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import type {KeymapInfo} from '../src/protocol/types';
import {initialState, reduce} from '../src/store/state';
import {effectiveKeys, userKeymap} from '../src/store/keymap';
import {REGISTRY_HELLO as HELLO, ROWS} from './hello';

const row = (id: string) => ROWS.find(r => r.id === id)!;
const READY = reduce(initialState, {type: 'event', ev: HELLO});

test('hello gives the keymap: the table\'s keys, nothing pinned, all default', () => {
  assert.equal(initialState.keymap.info, null);
  assert.equal(READY.keymap.info, HELLO.keymap);
  assert.deepEqual(effectiveKeys(READY.keymap.info, row('session.open_model')), ['Ctrl+O']);
  assert.deepEqual(effectiveKeys(READY.keymap.info, row('run.initial')), ['Ctrl+R']);
  assert.deepEqual(userKeymap(HELLO.keymap), {pinned: [], bindings: {}});
});

test('a keymap event replaces it, and the differences go back as a set sends them', () => {
  const info: KeymapInfo = {
    ...HELLO.keymap, pinned: ['session.reload_model'],
    commands: HELLO.keymap.commands.map(c => c.id === 'session.reload_model' ? {...c, keys: ['Ctrl+B'], source: 'user'} : c.id === 'session.save' ? {...c, keys: [], source: 'user'} : c),
  };
  const state = reduce(READY, {type: 'event', ev: {ev: 'keymap', op: 'set', keymap: info}});
  assert.equal(state.keymap.info, info);
  assert.deepEqual(effectiveKeys(state.keymap.info, row('session.reload_model')), ['Ctrl+B']);
  assert.deepEqual(effectiveKeys(state.keymap.info, row('session.save')), []);
  assert.deepEqual(userKeymap(info), {pinned: ['session.reload_model'], bindings: {'session.reload_model': ['Ctrl+B'], 'session.save': []}});
  /* the same keymap again changes nothing; a new hello (File > Open model) takes the file's again */
  assert.equal(reduce(state, {type: 'event', ev: {ev: 'keymap', op: 'get', keymap: info}}), state);
  assert.equal(reduce(state, {type: 'event', ev: HELLO}).keymap.info, HELLO.keymap);
});

test('a bad file arrives as an error with its place, the commands the table\'s', () => {
  const bad: KeymapInfo = {...HELLO.keymap, ok: false, error: '"Ctrl+W" is reserved', file: '/c/keymap.json', line: 3, col: 5, source: '  "x": ["Ctrl+W"]'};
  const state = reduce(READY, {type: 'event', ev: {ev: 'keymap', op: 'get', keymap: bad}});
  assert.equal(state.keymap.info?.ok, false);
  assert.equal(state.keymap.info?.line, 3);
  assert.deepEqual(userKeymap(bad), {pinned: [], bindings: {}});
});
