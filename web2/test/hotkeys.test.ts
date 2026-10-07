import {test} from 'node:test';
import assert from 'node:assert/strict';
import {handleKey, keyName, resolveKeys, RESERVED_KEYS, shortcutLabel} from '../src/ui/hotkeys';
import {BUSY_TITLE} from '../src/ui/context';
import {initialState, reduce, type Action, type AppState, type KeyPreset} from '../src/store/state';
import {mayStart, kindOf, noValueEdit} from '../src/protocol/kinds';
import type {Session} from '../src/session';
import {HELLO} from './hello';

function keyEvent(key: string, mods: Partial<KeyboardEvent> = {}) {
  const e = {key, code: '', ctrlKey: false, metaKey: false, altKey: false, shiftKey: false, defaultPrevented: false, target: null,
    prevented: 0, preventDefault() { this.prevented++; }, ...mods};
  return e as unknown as KeyboardEvent & {prevented: number};
}

/** a session with the parts handleKey uses: a store, may (as Session.may), menuAction, typeKey, awaitingMenu */
function fakeSession(opts: {computing?: boolean; hello?: AppState['hello']; preset?: KeyPreset; waiting?: boolean; ask?: boolean; core?: Partial<NonNullable<AppState['core']>>} = {}) {
  let state: AppState = {...initialState, hello: opts.hello === undefined ? HELLO : opts.hello, computing: opts.computing ?? false, core: (opts.core ?? null) as AppState['core'],
    keyPreset: opts.preset ?? 'default', ask: opts.ask ? {id: 1, kind: 'menu', keys: 'gl', items: []} as never : null};
  const calls: string[] = [];
  const session = {
    store: {getState: () => state, dispatch: (a: Action) => { state = reduce(state, a); }},
    may: (cmd: Parameters<Session['may']>[0]) => !(cmd.cmd === 'key' && noValueEdit(state.core, String(cmd.item))) && mayStart(kindOf(state.hello, 0, cmd), state.computing, false),
    menuAction: (menu: string, item: string) => { calls.push(`${menu}/${item}`); },
    typeKey: (k: string) => { calls.push(`key ${k}`); },
    awaitingMenu: () => opts.waiting ?? false,
  } as unknown as Session;
  return {session, calls, state: () => state};
}

/** type `keys` one after another, as handleKey sees them; the events, to read what was prevented */
function type(s: ReturnType<typeof fakeSession>, ...events: ReturnType<typeof keyEvent>[]) {
  for (const e of events) handleKey(s.session, e);
  return events;
}

test('a key event is named as the table writes keys', () => {
  assert.equal(keyName(keyEvent('s', {ctrlKey: true})), 'Ctrl+S');
  assert.equal(keyName(keyEvent('S', {metaKey: true, shiftKey: true})), 'Ctrl+Shift+S');
  assert.equal(keyName(keyEvent('ß', {altKey: true, code: 'KeyS'})), 'Alt+S');
  assert.equal(keyName(keyEvent('Enter', {altKey: true})), 'Alt+Enter');
  assert.equal(keyName(keyEvent('Escape')), 'Esc');
  assert.equal(keyName(keyEvent('F6', {shiftKey: true})), 'Shift+F6');
  assert.equal(keyName(keyEvent('i')), 'I');
  assert.equal(keyName(keyEvent('+', {shiftKey: true})), '+');
  assert.equal(keyName(keyEvent('Shift', {shiftKey: true})), null);
  assert.equal(keyName(keyEvent('Control', {ctrlKey: true})), null);
});

test('a default key runs its command, and only that command', () => {
  const idle = fakeSession();
  const [open, save, reload] = type(idle, keyEvent('o', {ctrlKey: true}), keyEvent('S', {metaKey: true}), keyEvent('r', {ctrlKey: true}));
  assert.deepEqual(idle.calls, ['file/openmodel', 'file/savesession', 'file/reload']);
  assert.deepEqual([open.prevented, save.prevented, reload.prevented], [1, 1, 1]);
  /* a key no command has stays the browser's */
  const [other] = type(idle, keyEvent('o', {ctrlKey: true, altKey: true}));
  assert.equal(other.prevented, 0);
  assert.equal(idle.calls.length, 3);
});

test('the XPPAUT preset off: a letter alone, or F then S, does nothing', () => {
  const off = fakeSession();
  const events = type(off, keyEvent('i'), keyEvent('f'), keyEvent('s'));
  assert.deepEqual(off.calls, []);
  assert.deepEqual(events.map(e => e.prevented), [0, 0, 0]);
  assert.deepEqual(off.state().pendingKeys, []);
});

test('the XPPAUT preset on: a letter, and F then S as a chord that Esc cancels', () => {
  const on = fakeSession({preset: 'xppaut'});
  const [i] = type(on, keyEvent('i'));
  assert.deepEqual(on.calls, ['main/initialconds']);
  assert.equal(i.prevented, 1);
  const [f] = type(on, keyEvent('F', {shiftKey: true}));
  assert.equal(f.prevented, 1);
  assert.deepEqual(on.state().pendingKeys, ['F']);
  type(on, keyEvent('s'));
  assert.deepEqual(on.calls, ['main/initialconds', 'file/saveinfo']);
  assert.deepEqual(on.state().pendingKeys, []);
  /* Esc cancels a pending chord and runs nothing; the key after it is a first key again */
  type(on, keyEvent('u'));
  assert.deepEqual(on.state().pendingKeys, ['U']);
  const [esc] = type(on, keyEvent('Escape'));
  assert.equal(esc.prevented, 1);
  assert.deepEqual(on.state().pendingKeys, []);
  type(on, keyEvent('d'));
  assert.deepEqual(on.calls, ['main/initialconds', 'file/saveinfo', 'main/dirfield']);
  /* a key that completes no chord is said, never skipped */
  type(on, keyEvent('f'), keyEvent('z'));
  assert.equal(on.state().bottom, 'F Z is not a command');
  assert.deepEqual(on.state().pendingKeys, []);
  /* the layers' switches are no commands of the page */
  assert.equal(resolveKeys(HELLO, 'xppaut', ['U', 'Esc']).row, null);
  assert.equal(resolveKeys(HELLO, 'xppaut', ['U']).more, true);
});

test('a key typed while a menu is open or on its way answers it', () => {
  const waiting = fakeSession({preset: 'xppaut', waiting: true});
  type(waiting, keyEvent('g'));
  assert.deepEqual(waiting.calls, ['key g']);
  const asking = fakeSession({preset: 'xppaut', ask: true});
  type(asking, keyEvent('l'));
  assert.deepEqual(asking.calls, ['key l']);
});

test('Alt+F4, Ctrl+W, Ctrl+Q, F11, F12 and F5 are never bound or prevented', () => {
  assert.deepEqual([...RESERVED_KEYS], ['Alt+F4', 'Ctrl+W', 'Ctrl+Q', 'F11', 'F12', 'F5']);
  /* even a table that bound one of them */
  const bound = {...HELLO, command_table: HELLO.command_table.map(r => r.id === 'reload' ? {...r, default_keys: ['Ctrl+W']}
    : r.id === 'help' ? {...r, default_keys: ['F12']} : r)};
  for (const hello of [HELLO, bound]) {
    const events = [keyEvent('F4', {altKey: true}), keyEvent('w', {ctrlKey: true}), keyEvent('W', {metaKey: true}), keyEvent('q', {ctrlKey: true}),
      keyEvent('F11'), keyEvent('F12'), keyEvent('F5')];
    const s = fakeSession({hello, preset: 'xppaut'});
    type(s, ...events);
    assert.deepEqual(events.map(e => e.prevented), events.map(() => 0));
    assert.deepEqual(s.calls, []);
  }
});

test('a run forbids a data command: the key says why, in the status bar', () => {
  const busy = fakeSession({computing: true});
  const [save] = type(busy, keyEvent('s', {ctrlKey: true}));
  assert.equal(save.prevented, 1);
  assert.deepEqual(busy.calls, []);
  assert.ok(busy.state().bottom.endsWith(BUSY_TITLE));
  /* before hello the key is the browser's */
  const early = fakeSession({hello: null});
  const [open] = type(early, keyEvent('o', {ctrlKey: true}));
  assert.equal(open.prevented, 0);
  assert.deepEqual(early.calls, []);
});

test('the keys of a command are listed from the table', () => {
  const row = (id: string) => HELLO.command_table.find(r => r.id === id)!;
  assert.equal(shortcutLabel(row('savesession'), 'default'), 'Ctrl+S');
  assert.equal(shortcutLabel(row('saveinfo'), 'default'), '');
  assert.equal(shortcutLabel(row('saveinfo'), 'xppaut'), 'F S');
  assert.equal(shortcutLabel(row('initialconds'), 'xppaut'), 'I');
});

test('Ctrl+Z, Ctrl+Shift+Z and Ctrl+Y are Undo and Redo value edit; a text field keeps them, and the status bar says when there is nothing to do', () => {
  const idle = fakeSession();
  type(idle, keyEvent('z', {ctrlKey: true, code: 'KeyZ'}), keyEvent('Z', {ctrlKey: true, shiftKey: true, code: 'KeyZ'}), keyEvent('y', {ctrlKey: true, code: 'KeyY'}));
  assert.deepEqual(idle.calls, ['main/undo', 'main/redo', 'main/redo']);
  const field = {closest: (selector: string) => selector.startsWith('input:not') ? {} : null};
  const typing = fakeSession();
  const [z] = type(typing, keyEvent('z', {ctrlKey: true, code: 'KeyZ', target: field as never}));
  assert.deepEqual([typing.calls, z.prevented], [[], 0]);
  const none = fakeSession({core: {can_undo: false, can_redo: true}});
  type(none, keyEvent('z', {ctrlKey: true, code: 'KeyZ'}), keyEvent('y', {ctrlKey: true, code: 'KeyY'}));
  assert.deepEqual(none.calls, ['main/redo']);
  assert.equal(none.state().bottom, 'Undo value edit: nothing to undo');
});
