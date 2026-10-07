/* The keymap editor's and the quick-access toolbar's logic (store/keymap.ts, W212): recording a key,
   conflicts, search by keys, the differences each edit sends, pinning, reordering and the overflow.
   All pure: the dialog and the toolbar are checked in web2check (section keymapeditor). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import type {KeymapInfo} from '../src/protocol/types';
import {
  conflictOf, editorRows, effectiveKeys, initialKeymap, movePinned, pinnedRows, recordKey, reduceKeymap, replaceKey,
  sourceLabel, togglePinned, userKeymap, visibleCount, withKey, withoutBinding, withoutKey, withPinned, withPreset,
} from '../src/store/keymap';
import {handleKey} from '../src/ui/hotkeys';
import {initialState, reduce, type Action, type AppState} from '../src/store/state';
import type {Session} from '../src/session';
import {HELLO} from './hello';

const info: KeymapInfo = HELLO.keymap;
const rows = HELLO.command_table;
const row = (id: string) => rows.find(r => r.id === id)!;
/** `info` with command `id` on `keys` as the user's */
const bound = (id: string, keys: string[]): KeymapInfo =>
  ({...info, commands: info.commands.map(c => c.id === id ? {...c, keys, source: 'user' as const} : c)});

test('the table has the three run commands and the editor, with the keys of the design', () => {
  assert.deepEqual(effectiveKeys(info, row('run_initial')), ['Ctrl+Enter']);
  assert.deepEqual(effectiveKeys(info, row('run_last')), ['Ctrl+Shift+Enter']);
  assert.deepEqual(effectiveKeys(info, row('steady')), ['Alt+S']);
  assert.deepEqual(effectiveKeys(info, row('keymapeditor')), []);
  assert.ok(row('keymapeditor').pinnable && row('keymapeditor').key === '');
});

test('a recorded key: Esc cancels, Backspace clears, a reserved key is refused, a taken key is a conflict', () => {
  const rec = {id: 'reload', index: 0};
  assert.deepEqual(recordKey(info, rows, rec, 'Esc'), {kind: 'cancel'});
  assert.deepEqual(recordKey(info, rows, rec, 'Backspace'), {kind: 'clear'});
  const reserved = recordKey(info, rows, rec, 'Ctrl+W');
  assert.equal(reserved.kind, 'refused');
  assert.match((reserved as {text: string}).text, /Ctrl\+W is kept by the system or the browser/);
  assert.deepEqual(recordKey(info, rows, rec, 'Ctrl+B'), {kind: 'set', key: 'Ctrl+B'});
  /* Ctrl+S is Save session's */
  assert.deepEqual(recordKey(info, rows, rec, 'Ctrl+S'), {kind: 'conflict', conflict: {id: 'reload', index: 0, key: 'Ctrl+S', other: 'savesession'}});
  /* a command's own key is no conflict */
  assert.deepEqual(recordKey(info, rows, rec, 'Ctrl+R'), {kind: 'set', key: 'Ctrl+R'});
  /* an unknown command cancels */
  assert.deepEqual(recordKey(info, rows, {id: 'nosuch', index: 0}, 'Ctrl+B'), {kind: 'cancel'});
});

test('too many keys on one command is refused with the limit', () => {
  const eight = bound('reload', ['Ctrl+B', 'Ctrl+D', 'Ctrl+E', 'Ctrl+G', 'Ctrl+H', 'Ctrl+J', 'Ctrl+M', 'Ctrl+P']);
  const result = recordKey(eight, rows, {id: 'reload', index: 8}, 'Ctrl+U');
  assert.equal(result.kind, 'refused');
  assert.match((result as {text: string}).text, /most keys it may \(8\)/);
});

test('a key that starts another command\'s chord clashes with it, as the core says', () => {
  const chord = bound('reload', ['Ctrl+K Ctrl+R']);
  assert.equal(conflictOf(chord, rows, 'savesession', 'Ctrl+K')?.id, 'reload');
  assert.equal(conflictOf(chord, rows, 'savesession', 'Ctrl+K Ctrl+R')?.id, 'reload');
  assert.equal(conflictOf(chord, rows, 'savesession', 'Ctrl+K Ctrl+S'), undefined);
  assert.equal(conflictOf(chord, rows, 'reload', 'Ctrl+K'), undefined);
});

test('an edit is the user\'s differences with that change: set, add, remove, reset, replace', () => {
  assert.deepEqual(withKey(info, row('reload'), 0, 'Ctrl+B'), {preset: 'default', pinned: [], bindings: {reload: ['Ctrl+B']}});
  /* a second binding keeps the first; a command with no keys gets one */
  assert.deepEqual(withKey(info, row('reload'), 1, 'F9').bindings, {reload: ['Ctrl+R', 'F9']});
  assert.deepEqual(withKey(info, row('keymapeditor'), 0, 'F9').bindings, {keymapeditor: ['F9']});
  /* removing the last key is a binding of [] (no key), not the default again */
  assert.deepEqual(withoutKey(info, row('reload'), 0).bindings, {reload: []});
  const changed = bound('reload', ['Ctrl+B']);
  assert.deepEqual(withoutBinding(changed, 'reload').bindings, {});
  /* the other commands' bindings, the pins and the preset are carried */
  const mixed: KeymapInfo = {...bound('help', ['F9']), pinned: ['help'], preset: 'xppaut'};
  assert.deepEqual(withKey(mixed, row('reload'), 0, 'Ctrl+B'),
    {preset: 'xppaut', pinned: ['help'], bindings: {help: ['F9'], reload: ['Ctrl+B']}});
  /* Replace gives the key to the command and takes it from the one that had it */
  assert.deepEqual(replaceKey(info, rows, row('reload'), 0, 'Ctrl+S').bindings, {reload: ['Ctrl+S'], savesession: []});
  assert.deepEqual(withPreset(info, 'xppaut').preset, 'xppaut');
  assert.deepEqual(withPinned(info, ['help']).pinned, ['help']);
});

test('where a command\'s keys come from: default, changed (instead of the table\'s) or user (it had none)', () => {
  assert.equal(sourceLabel(info, row('reload')), 'default');
  assert.equal(sourceLabel(bound('reload', ['Ctrl+B']), row('reload')), 'changed');
  assert.equal(sourceLabel(bound('keymapeditor', ['F9']), row('keymapeditor')), 'user');
  assert.deepEqual(userKeymap(bound('reload', [])).bindings, {reload: []});
});

test('the editor\'s search matches label, description, id and the keys pressed', () => {
  const ids = (query: string, from: KeymapInfo = info) => editorRows(from, rows, query).map(r => r.id);
  assert.ok(ids('reload').includes('reload'));
  assert.deepEqual(ids('Ctrl+Enter'), ['run_initial', 'run_last']);
  assert.deepEqual(ids('ctrl+shift+enter'), ['run_last']);
  assert.deepEqual(ids('alt s'), ['steady']);
  assert.ok(ids('trajectory').includes('run_initial'));
  /* the user's keys are searched, not the table's */
  assert.deepEqual(ids('f9', bound('help', ['F9'])), ['help']);
  assert.deepEqual(ids('f9'), []);
  /* the shortcut-layer switches take no keys and are not listed */
  assert.ok(!editorRows(info, rows, '').some(r => r.category === 'layer'));
  assert.equal(editorRows(info, rows, '').length, rows.filter(r => r.category !== 'layer').length);
});

test('pins: the pinned commands that may be pinned, in order; toggled, moved', () => {
  const pinned: KeymapInfo = {...info, pinned: ['help', 'reload', 'steady']};
  assert.deepEqual(pinnedRows(pinned, rows).map(r => r.id), ['help', 'reload', 'steady']);
  assert.deepEqual(pinnedRows(null, rows), []);
  assert.deepEqual(pinnedRows({...info, pinned: ['quit', 'nosuch', 'help']}, rows).map(r => r.id), ['help']);
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
  state = reduceKeymap(state, {type: 'record', recording: {id: 'reload', index: 0}});
  assert.deepEqual(state.editor.recording, {id: 'reload', index: 0});
  state = reduceKeymap(state, {type: 'conflict', conflict: {id: 'reload', index: 0, key: 'Ctrl+S', other: 'savesession'}});
  assert.equal(state.editor.recording, null);
  assert.equal(state.editor.conflict?.other, 'savesession');
  state = reduceKeymap(state, {type: 'refuse', text: 'no'});
  assert.equal(state.editor.conflict, null);
  assert.equal(state.editor.refusal, 'no');
  state = reduceKeymap(state, {type: 'confirmReset', on: true});
  const answered = reduceKeymap(state, {type: 'info', info: bound('reload', ['Ctrl+B'])});
  assert.deepEqual([answered.editor.refusal, answered.editor.confirmingReset, answered.editor.query, answered.editor.open], ['', false, 'run', true]);
  assert.equal(reduceKeymap(answered, {type: 'close'}).editor.open, false);
});

/* the dispatcher reads the effective keys */
function fake(keymap: KeymapInfo, extra: Partial<AppState> = {}) {
  let state: AppState = {...initialState, hello: HELLO, keymap: {...initialKeymap, info: keymap}, ...extra};
  const calls: string[] = [];
  const session = {
    store: {getState: () => state, dispatch: (a: Action) => { state = reduce(state, a); }},
    may: () => true,
    menuAction: (menu: string, item: string) => { calls.push(`${menu}/${item}`); },
    typeKey: () => undefined,
    awaitingMenu: () => false,
  } as unknown as Session;
  return {session, calls};
}
const press = (key: string, mods: Partial<KeyboardEvent> = {}) =>
  ({key, code: '', ctrlKey: false, metaKey: false, altKey: false, shiftKey: false, defaultPrevented: false, target: null, preventDefault() { /* the page's */ }, ...mods}) as unknown as KeyboardEvent;

test('the dispatcher runs the user\'s key, not the table\'s, and the run commands by their keys', () => {
  const user = fake(bound('reload', ['Ctrl+B']));
  handleKey(user.session, press('b', {ctrlKey: true}));
  handleKey(user.session, press('r', {ctrlKey: true}));
  assert.deepEqual(user.calls, ['file/reload']);
  const run = fake(info);
  handleKey(run.session, press('Enter', {ctrlKey: true}));
  handleKey(run.session, press('Enter', {ctrlKey: true, shiftKey: true}));
  handleKey(run.session, press('s', {altKey: true, code: 'KeyS'}));
  assert.deepEqual(run.calls, ['main/run_initial', 'main/run_last', 'main/steady']);
});

test('a key the core reserves is never run, even if a keymap bound it', () => {
  const hostile = fake(bound('reload', ['Ctrl+W']));
  handleKey(hostile.session, press('w', {ctrlKey: true}));
  assert.deepEqual(hostile.calls, []);
});
