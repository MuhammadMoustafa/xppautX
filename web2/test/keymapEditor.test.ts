/* The keymap editor's and the quick-access toolbar's logic (store/keymap.ts, W212): recording a key,
   conflicts, search by keys, the differences each edit sends, pinning, reordering and the overflow.
   All pure: the dialog and the toolbar are checked in web2check (section keymapeditor). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import type {KeymapInfo} from '../src/protocol/types';
import {
  conflictOf, editorRows, effectiveKeys, initialKeymap, movePinned, pinnedRows, recordKey, reduceKeymap, replaceKey,
  sourceLabel, togglePinned, userKeymap, visibleCount, withKey, withoutBinding, withoutKey, withPinned,
} from '../src/store/keymap';
import {handleKey} from '../src/ui/hotkeys';
import {initialState, reduce, type Action, type AppState} from '../src/store/state';
import type {Session} from '../src/session';
import {REGISTRY_HELLO, ROWS} from './hello';

const info: KeymapInfo = REGISTRY_HELLO.keymap;
const rows = ROWS;
const row = (id: string) => rows.find(r => r.id === id)!;
/** `info` with command `id` on `keys` as the user's */
const bound = (id: string, keys: string[]): KeymapInfo =>
  ({...info, commands: info.commands.map(c => c.id === id ? {...c, keys, source: 'user' as const} : c)});

test('the table has the frequent commands on the default keys, the rest on none', () => {
  assert.deepEqual(effectiveKeys(info, row('run.initial')), ['Ctrl+R']);
  assert.deepEqual(effectiveKeys(info, row('run.abort')), ['Escape']);
  assert.deepEqual(effectiveKeys(info, row('tool.command_palette')), ['Ctrl+Shift+P']);
  assert.deepEqual(effectiveKeys(info, row('run.steady')), []);
  assert.deepEqual(effectiveKeys(info, row('tool.keyboard_shortcuts')), []);
  assert.ok(row('tool.keyboard_shortcuts'));
});

test('a recorded key: Esc cancels, Backspace clears, a reserved key is refused, a taken key is a conflict', () => {
  const rec = {id: 'session.open_model', index: 0};
  assert.deepEqual(recordKey(info, rows, rec, 'Escape'), {kind: 'cancel'});
  assert.deepEqual(recordKey(info, rows, rec, 'Backspace'), {kind: 'clear'});
  const reserved = recordKey(info, rows, rec, 'Ctrl+W');
  assert.equal(reserved.kind, 'refused');
  assert.match((reserved as {text: string}).text, /Ctrl\+W is kept by the system or the browser/);
  assert.deepEqual(recordKey(info, rows, rec, 'Ctrl+B'), {kind: 'set', key: 'Ctrl+B'});
  /* Ctrl+S is Save session's */
  assert.deepEqual(recordKey(info, rows, rec, 'Ctrl+S'), {kind: 'conflict', conflict: {id: 'session.open_model', index: 0, key: 'Ctrl+S', other: 'session.save'}});
  /* a command's own key is no conflict */
  assert.deepEqual(recordKey(info, rows, rec, 'Ctrl+O'), {kind: 'set', key: 'Ctrl+O'});
  /* an unknown command cancels */
  assert.deepEqual(recordKey(info, rows, {id: 'nosuch', index: 0}, 'Ctrl+B'), {kind: 'cancel'});
});

test('too many keys on one command is refused with the limit', () => {
  const eight = bound('session.open_model', ['Ctrl+B', 'Ctrl+D', 'Ctrl+E', 'Ctrl+G', 'Ctrl+H', 'Ctrl+J', 'Ctrl+M', 'Ctrl+P']);
  const result = recordKey(eight, rows, {id: 'session.open_model', index: 8}, 'Ctrl+U');
  assert.equal(result.kind, 'refused');
  assert.match((result as {text: string}).text, /most keys it may \(8\)/);
});

test('a key another command has clashes, and only that exact key', () => {
  assert.equal(conflictOf(info, rows, 'session.save', 'Ctrl+O')?.id, 'session.open_model');
  assert.equal(conflictOf(info, rows, 'session.open_model', 'Ctrl+O'), undefined);
  assert.equal(conflictOf(info, rows, 'session.save', 'Ctrl+K'), undefined);
});

test('an edit is the user\'s differences with that change: set, add, remove, reset, replace', () => {
  assert.deepEqual(withKey(info, row('session.open_model'), 0, 'Ctrl+B'), {pinned: [], bindings: {'session.open_model': ['Ctrl+B']}});
  /* a second binding keeps the first; a command with no keys gets one */
  assert.deepEqual(withKey(info, row('session.open_model'), 1, 'F9').bindings, {'session.open_model': ['Ctrl+O', 'F9']});
  assert.deepEqual(withKey(info, row('tool.keyboard_shortcuts'), 0, 'F9').bindings, {'tool.keyboard_shortcuts': ['F9']});
  /* removing the last key is a binding of [] (no key), not the default again */
  assert.deepEqual(withoutKey(info, row('session.open_model'), 0).bindings, {'session.open_model': []});
  const changed = bound('session.open_model', ['Ctrl+B']);
  assert.deepEqual(withoutBinding(changed, 'session.open_model').bindings, {});
  /* the other commands' bindings, the pins are carried */
  const mixed: KeymapInfo = {...bound('tool.calculator', ['F9']), pinned: ['tool.calculator']};
  assert.deepEqual(withKey(mixed, row('session.open_model'), 0, 'Ctrl+B'),
    {pinned: ['tool.calculator'], bindings: {'tool.calculator': ['F9'], 'session.open_model': ['Ctrl+B']}});
  /* Replace gives the key to the command and takes it from the one that had it */
  assert.deepEqual(replaceKey(info, rows, row('session.open_model'), 0, 'Ctrl+S').bindings, {'session.open_model': ['Ctrl+S'], 'session.save': []});
  assert.deepEqual(withPinned(info, ['tool.calculator']).pinned, ['tool.calculator']);
});

test('where a command\'s keys come from: default, changed (instead of the table\'s) or user (it had none)', () => {
  assert.equal(sourceLabel(info, row('session.open_model')), 'default');
  assert.equal(sourceLabel(bound('session.open_model', ['Ctrl+B']), row('session.open_model')), 'changed');
  assert.equal(sourceLabel(bound('tool.keyboard_shortcuts', ['F9']), row('tool.keyboard_shortcuts')), 'user');
  assert.deepEqual(userKeymap(bound('session.open_model', [])).bindings, {'session.open_model': []});
});

test('the editor\'s search matches label, description, id and the keys pressed', () => {
  const ids = (query: string, from: KeymapInfo = info) => editorRows(from, rows, query).map(r => r.id);
  assert.ok(ids('session.open_model').includes('session.open_model'));
  assert.ok(ids('Ctrl+R').includes('run.initial'));
  assert.deepEqual(ids('ctrl+shift+p'), ['tool.command_palette']);
  assert.deepEqual(ids('escape'), ['run.abort']);
  assert.ok(ids('initial').includes('run.initial'));
  /* the user's keys are searched, not the table's */
  assert.deepEqual(ids('f9', bound('tool.calculator', ['F9'])), ['tool.calculator']);
  assert.deepEqual(ids('f9'), []);
  assert.equal(editorRows(info, rows, '').length, rows.length);
});

test('pins: the pinned commands that may be pinned, in order; toggled, moved', () => {
  const pinned: KeymapInfo = {...info, pinned: ['tool.calculator', 'session.open_model', 'run.steady']};
  assert.deepEqual(pinnedRows(pinned, rows).map(r => r.id), ['tool.calculator', 'session.open_model', 'run.steady']);
  assert.deepEqual(pinnedRows(null, rows), []);
  assert.deepEqual(pinnedRows({...info, pinned: ['nosuch', 'tool.calculator']}, rows).map(r => r.id), ['tool.calculator']);
  assert.deepEqual(togglePinned(['a', 'b'], 'c'), ['a', 'b', 'c']);
  assert.deepEqual(togglePinned(['a', 'b'], 'a'), ['b']);
  assert.deepEqual(movePinned(['a', 'b', 'c'], 'c', 0), ['c', 'a', 'b']);
  assert.deepEqual(movePinned(['a', 'b', 'c'], 'a', 2), ['b', 'c', 'a']);
  assert.deepEqual(movePinned(['a', 'b', 'c'], 'b', 99), ['a', 'c', 'b']);
  assert.deepEqual(movePinned(['a', 'b', 'c'], 'b', -3), ['b', 'a', 'c']);
  assert.deepEqual(movePinned(['a', 'b'], 'z', 0), ['a', 'b']);
});

test('the overflow: every button that fits shows, the rest go to More, none is dropped', () => {
  const widths = [100, 100, 100];
  assert.equal(visibleCount(widths, 1000, 60, 4), 3);
  /* all three need 312 with their gaps: at 300 More is needed (60 + 4), leaving room for two */
  assert.equal(visibleCount(widths, 300, 60, 4), 2);
  assert.equal(visibleCount(widths, 170, 60, 4), 1);
  assert.equal(visibleCount(widths, 50, 60, 4), 0);
  assert.equal(visibleCount([], 0, 60, 4), 0);
});

test('the editor\'s state: open, search, record, conflict, refusal; the core\'s answer ends them', () => {
  let state = reduceKeymap({...initialKeymap, info}, {type: 'open'});
  assert.equal(state.editor.open, true);
  state = reduceKeymap(state, {type: 'query', query: 'run'});
  state = reduceKeymap(state, {type: 'record', recording: {id: 'session.open_model', index: 0}});
  assert.deepEqual(state.editor.recording, {id: 'session.open_model', index: 0});
  state = reduceKeymap(state, {type: 'conflict', conflict: {id: 'session.open_model', index: 0, key: 'Ctrl+S', other: 'session.save'}});
  assert.equal(state.editor.recording, null);
  assert.equal(state.editor.conflict?.other, 'session.save');
  state = reduceKeymap(state, {type: 'refuse', text: 'no'});
  assert.equal(state.editor.conflict, null);
  assert.equal(state.editor.refusal, 'no');
  state = reduceKeymap(state, {type: 'confirmReset', on: true});
  const answered = reduceKeymap(state, {type: 'info', info: bound('session.open_model', ['Ctrl+B'])});
  assert.deepEqual([answered.editor.refusal, answered.editor.confirmingReset, answered.editor.query, answered.editor.open], ['', false, 'run', true]);
  assert.equal(reduceKeymap(answered, {type: 'close'}).editor.open, false);
});

/* the dispatcher reads the effective keys */
function fake(keymap: KeymapInfo, extra: Partial<AppState> = {}) {
  let state: AppState = {...initialState, hello: REGISTRY_HELLO, keymap: {...initialKeymap, info: keymap}, ...extra};
  const calls: string[] = [];
  const session = {
    store: {getState: () => state, dispatch: (a: Action) => { state = reduce(state, a); }},
    may: () => true,
    registryAction: (id: string) => { calls.push(id); },
    typeKey: () => undefined,
    awaitingMenu: () => false,
  } as unknown as Session;
  return {session, calls};
}
const press = (key: string, mods: Partial<KeyboardEvent> = {}) =>
  ({key, code: '', ctrlKey: false, metaKey: false, altKey: false, shiftKey: false, defaultPrevented: false, target: null, preventDefault() { /* the page's */ }, ...mods}) as unknown as KeyboardEvent;

test('the dispatcher runs a user binding of a registry command', () => {
  const info = {...REGISTRY_HELLO.keymap, commands: REGISTRY_HELLO.keymap.commands.map(c => c.id === 'session.reload_model' ? {...c, keys:['Ctrl+B'], source:'user' as const} : c)};
  const user = fake(info);
  handleKey(user.session, press('b', {ctrlKey:true}));
  handleKey(user.session, press('r', {ctrlKey:true}));
  assert.deepEqual(user.calls, ['session.reload_model', 'run.initial']);
});

test('a key the core reserves is never run, even if a keymap bound it', () => {
  const hostile = fake(bound('session.open_model', ['Ctrl+W']));
  handleKey(hostile.session, press('w', {ctrlKey: true}));
  assert.deepEqual(hostile.calls, []);
});
