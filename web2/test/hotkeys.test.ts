import {test} from 'node:test';
import assert from 'node:assert/strict';
import {chordOf, commandKey, handleKey} from '../src/ui/hotkeys';
import {BUSY_TITLE} from '../src/ui/context';
import {initialState, reduce, type Action, type AppState} from '../src/store/state';
import {mayStart, kindOf} from '../src/protocol/kinds';
import type {Session} from '../src/session';
import {HELLO} from './hello';

function keyEvent(key: string, mods: Partial<KeyboardEvent> = {}) {
  const e = {key, ctrlKey: false, metaKey: false, altKey: false, shiftKey: false, defaultPrevented: false, target: null,
    prevented: 0, preventDefault() { this.prevented++; }, ...mods};
  return e as unknown as KeyboardEvent & {prevented: number};
}

/** a session with the parts handleKey uses: a store, may (as Session.may), menuAction */
function fakeSession(computing: boolean, hello = HELLO as AppState['hello']) {
  let state: AppState = {...initialState, hello, computing};
  const calls: string[] = [];
  const session = {
    store: {getState: () => state, dispatch: (a: Action) => { state = reduce(state, a); }},
    may: (cmd: Parameters<Session['may']>[0]) => mayStart(kindOf(state.hello, 0, cmd), state.computing, false),
    menuAction: (menu: string, item: string) => { calls.push(`${menu}/${item}`); },
    typeKey: (k: string) => { calls.push(`key ${k}`); },
  } as unknown as Session;
  return {session, calls, state: () => state};
}

test('only Ctrl/Cmd+O, S and K are the page\'s: Ctrl/Cmd+W and Alt+F4 stay the browser\'s and the system\'s', () => {
  assert.deepEqual(chordOf(HELLO, keyEvent('o', {ctrlKey: true})), {menu: 'file', item: 'openmodel'});
  assert.deepEqual(chordOf(HELLO, keyEvent('S', {metaKey: true})), {menu: 'file', item: 'savesession'});
  assert.equal(chordOf(HELLO, keyEvent('k', {ctrlKey: true})), 'search');
  for (const e of [keyEvent('w', {ctrlKey: true}), keyEvent('W', {metaKey: true}), keyEvent('F4', {altKey: true}),
    keyEvent('o', {ctrlKey: true, shiftKey: true}), keyEvent('o', {ctrlKey: true, altKey: true}), keyEvent('o')]) {
    assert.equal(chordOf(HELLO, e), null);
  }
  for (const e of [keyEvent('w', {ctrlKey: true}), keyEvent('w', {metaKey: true}), keyEvent('F4', {altKey: true})]) {
    const {session, calls} = fakeSession(false);
    handleKey(session, e);
    assert.equal(e.prevented, 0, 'the default is not prevented');
    assert.deepEqual(calls, [], 'the page does nothing with it');
  }
});

test('a chord is prevented only when the page acts on it or says why it cannot', () => {
  let e = keyEvent('s', {ctrlKey: true});
  const idle = fakeSession(false);
  handleKey(idle.session, e);
  assert.equal(e.prevented, 1);
  assert.deepEqual(idle.calls, ['file/savesession']);
  /* a run is going: Save is a data command, refused, and the status bar says so */
  e = keyEvent('s', {ctrlKey: true});
  const busy = fakeSession(true);
  handleKey(busy.session, e);
  assert.equal(e.prevented, 1);
  assert.deepEqual(busy.calls, []);
  assert.ok(busy.state().bottom.endsWith(BUSY_TITLE));
  /* before hello the key is the browser's */
  e = keyEvent('o', {ctrlKey: true});
  const early = fakeSession(false, null);
  handleKey(early.session, e);
  assert.equal(e.prevented, 0);
  assert.deepEqual(early.calls, []);
});

test('a capital is the lowercase command only when the capital is none', () => {
  const known = (k: string) => 'ig'.includes(k);
  assert.equal(commandKey('I', known), 'i');
  assert.equal(commandKey('i', known), 'i');
  assert.equal(commandKey('X', known), 'X');
  assert.equal(commandKey('Escape', known), 'Escape');
  assert.equal(commandKey('I', k => k === 'I' || k === 'i'), 'I');
});
