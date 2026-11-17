/* The keymap editor's and the quick-access toolbar's logic (store/keymap.ts, W212): recording a key,
   conflicts, search by keys, the differences each edit sends, pinning, reordering and the overflow.
   All pure: the dialog and the toolbar are checked in web2check (section keymapeditor). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import type {KeymapInfo} from '../src/protocol/types';
import {
  conflictOf, editorRows, effectiveKeys, initialKeymap, freshName, pinBindingId, pinRows, withRenamedPin, movePinned, moveStep, pinOf, pinKeys, recordKey, reduceKeymap, replaceKey,
  sourceLabel, togglePinned, userKeymap, visibleCount, withKey, withoutBinding, withoutKey, withPinned,
} from '../src/store/keymap';
import {handleKey} from '../src/ui/hotkeys';
import {initialState, reduce, type Action, type AppState} from '../src/store/state';
import type {Session} from '../src/session';
import {REGISTRY_HELLO, ROWS} from './hello';

const CALC = {name: 'Evaluate', steps: ['calculator.evaluate']};
const info: KeymapInfo = REGISTRY_HELLO.keymap;
const rows = ROWS;
const row = (id: string) => rows.find(r => r.id === id)!;
/** `info` with command `id` on `keys` as the user's */
const bound = (id: string, keys: string[]): KeymapInfo =>
  ({...info, commands: info.commands.map(c => c.id === id ? {...c, keys, source: 'user' as const} : c)});

test('the table has the frequent commands on the default keys, the rest on none', () => {
  assert.deepEqual(effectiveKeys(info, row('run.initial')), ['Ctrl+R']);
  assert.deepEqual(effectiveKeys(info, row('run.stop')), ['Escape']);
  assert.deepEqual(effectiveKeys(info, row('command_palette.open')), ['Ctrl+Shift+P']);
  assert.deepEqual(effectiveKeys(info, row('run.steady')), []);
  assert.deepEqual(effectiveKeys(info, row('keymap.edit')), []);
  assert.ok(row('keymap.edit'));
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
  assert.deepEqual(withKey(info, row('keymap.edit'), 0, 'F9').bindings, {'keymap.edit': ['F9']});
  /* removing the last key is a binding of [] (no key), not the default again */
  assert.deepEqual(withoutKey(info, row('session.open_model'), 0).bindings, {'session.open_model': []});
  const changed = bound('session.open_model', ['Ctrl+B']);
  assert.deepEqual(withoutBinding(changed, 'session.open_model').bindings, {});
  /* the other commands' bindings, the pins are carried */
  const mixed: KeymapInfo = {...bound('calculator.evaluate', ['F9']), pinned: [CALC]};
  assert.deepEqual(withKey(mixed, row('session.open_model'), 0, 'Ctrl+B'),
    {pinned: [CALC], bindings: {'calculator.evaluate': ['F9'], 'session.open_model': ['Ctrl+B']}});
  /* Replace gives the key to the command and takes it from the one that had it */
  assert.deepEqual(replaceKey(info, rows, row('session.open_model'), 0, 'Ctrl+S').bindings, {'session.open_model': ['Ctrl+S'], 'session.save': []});
  assert.deepEqual(withPinned(info, [CALC]).pinned, [CALC]);
});

test('where a command\'s keys come from: default, changed (instead of the table\'s) or user (it had none)', () => {
  assert.equal(sourceLabel(info, row('session.open_model')), 'default');
  assert.equal(sourceLabel(bound('session.open_model', ['Ctrl+B']), row('session.open_model')), 'changed');
  assert.equal(sourceLabel(bound('keymap.edit', ['F9']), row('keymap.edit')), 'user');
  assert.deepEqual(userKeymap(bound('session.open_model', [])).bindings, {'session.open_model': []});
});

test('the editor\'s search matches label, description, id and the keys pressed', () => {
  const ids = (query: string, from: KeymapInfo = info) => editorRows(from, rows, query).map(r => r.id);
  assert.ok(ids('session.open_model').includes('session.open_model'));
  assert.ok(ids('Ctrl+R').includes('run.initial'));
  /* a key is found by what it starts with: Ctrl+Shift+PageUp and PageDown (W719) begin as Ctrl+Shift+P does */
  assert.deepEqual(ids('ctrl+shift+p'), ['command_palette.open', 'window.move_earlier', 'window.move_later']);
  assert.deepEqual(ids('ctrl+shift+pagedown'), ['window.move_later']);
  assert.deepEqual(ids('escape'), ['run.stop']);
  assert.ok(ids('initial').includes('run.initial'));
  /* the user's keys are searched, not the table's */
  assert.deepEqual(ids('f9', bound('calculator.evaluate', ['F9'])), ['calculator.evaluate']);
  assert.deepEqual(ids('f9'), []);
  assert.equal(editorRows(info, rows, '').length, rows.length);
});

test('pins: a command is pinned as a pin of one step, toggled, moved, renamed apart', () => {
  const a = {name: 'A', steps: ['a']}, b = {name: 'B', steps: ['b', 'c']}, c = {name: 'C', steps: ['c']};
  assert.deepEqual(togglePinned([a, b], {binding: 'x', name: 'X'}), [a, b, {name: 'X', steps: ['x']}]);
  assert.deepEqual(togglePinned([a, b], {binding: 'a', name: 'Other'}), [b]); /* the pin of exactly that one step goes */
  assert.deepEqual(togglePinned([a], {binding: 'q', name: 'A'}).map(p => p.name), ['A', 'A (2)']); /* a name is unique */
  assert.equal(pinOf([a, b], 'b'), undefined); /* b is the first of two steps: not the pin of that command */
  assert.equal(freshName([a, c], 'D'), 'D');
  assert.deepEqual(movePinned([a, b, c], 'C', 0).map(p => p.name), ['C', 'A', 'B']);
  assert.deepEqual(movePinned([a, b, c], 'A', 2).map(p => p.name), ['B', 'C', 'A']);
  assert.deepEqual(movePinned([a, b, c], 'B', 99).map(p => p.name), ['A', 'C', 'B']);
  assert.deepEqual(movePinned([a, b], 'Z', 0), [a, b]);
  assert.deepEqual(moveStep(['x', 'y', 'z'], 2, 0), ['z', 'x', 'y']);
  assert.deepEqual(moveStep(['x', 'y', 'z'], 0, 9), ['y', 'z', 'x']);
  /* the keys of a pin of one step are its command's; a sequence has none */
  assert.deepEqual(pinKeys(info, rows, {name: 'Open', steps: ['session.open_model']}), ['Ctrl+O']);
  assert.deepEqual(pinKeys(info, rows, {name: 'Two', steps: ['session.open_model', 'run.steady']}), []);
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
  assert.equal(state.editor.query, '');
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
  assert.deepEqual([answered.editor.refusal, answered.editor.confirmingReset, answered.editor.query], ['', false, 'run']);
  assert.equal(reduceKeymap(answered, {type: 'close'}).editor.query, '');
});

/* the dispatcher reads the effective keys */
function fake(keymap: KeymapInfo, extra: Partial<AppState> = {}) {
  let state: AppState = {...initialState, hello: REGISTRY_HELLO, keymap: {...initialKeymap, info: keymap}, ...extra};
  const calls: string[] = [];
  const session = {
    store: {getState: () => state, dispatch: (a: Action) => { state = reduce(state, a); }},
    may: () => true,
    registryAction: (id: string) => { calls.push(id); },
    registryRow: (row: {id: string}) => { calls.push(row.id); },
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

const bound_ = (id: string, keys: string[]): KeymapInfo => ({...info, commands: [...info.commands, {id, keys, source: 'user'}]});
test('a pin of several steps is a row bound by `pin "Name"`: its keys go with it when it is renamed and are dropped when it goes or shrinks to one step', () => {
  const figures = {name: 'Figures', steps: ['run.initial', 'plot.export --format pdf']};
  const id = pinBindingId('Figures');
  assert.equal(id, 'pin "Figures"');
  const bound: KeymapInfo = {...bound_(id, ['Ctrl+Shift+F']), pinned: [figures, {name: 'One', steps: ['run.initial']}]};
  assert.deepEqual(pinRows(bound).map(r => [r.binding, r.pin, r.name]), [[id, 'Figures', 'Pin "Figures"']]); /* a pin of one step is its command's row */
  assert.deepEqual(effectiveKeys(bound, pinRows(bound)[0]), ['Ctrl+Shift+F']);
  assert.deepEqual(pinKeys(bound, pinRows(bound), figures), ['Ctrl+Shift+F']);
  assert.deepEqual(withRenamedPin(bound, 'Figures', {...figures, name: 'Plots'}).bindings, {'pin "Plots"': ['Ctrl+Shift+F']});
  assert.deepEqual(withPinned(bound, [{name: 'One', steps: ['run.initial']}]).bindings, {});
  assert.deepEqual(withPinned(bound, [{...figures, steps: ['run.initial']}]).bindings, {});
  assert.deepEqual(withPinned(bound, [figures]).bindings, {[id]: ['Ctrl+Shift+F']});
  /* a key another command has is the conflict the editor already asks about */
  assert.equal(conflictOf(bound, [...rows, ...pinRows(bound)], id, 'Ctrl+O')?.binding, 'session.open_model');
});
