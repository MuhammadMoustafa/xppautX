import {test} from 'node:test';
import assert from 'node:assert/strict';
import {commandOnKey, handleKey, keyName} from '../src/ui/hotkeys';
import {REGISTRY_HELLO as HELLO} from './hello';
import {bindableRows, registryRows} from '../src/ui/navigation';
import type {HelloEvent, Pin} from '../src/protocol/types';
import {initialState, reduce, type Action} from '../src/store/state';
import type {Session} from '../src/session';

const event = (key: string, mods: Partial<KeyboardEvent> = {}) => ({key, code:'', defaultPrevented:false, ctrlKey:false, altKey:false, shiftKey:false, metaKey:false, target:null,
  prevented:false, preventDefault() {(this as {prevented:boolean}).prevented = true;}, ...mods}) as unknown as KeyboardEvent & {prevented: boolean};
function session(computing = false) {
  const commands = registryRows(HELLO).map(row => ({id:row.id, keys:row.default_keys, source:'default' as const}));
  let state = {...initialState, hello: HELLO, computing, keymap:{...initialState.keymap, info:{...HELLO.keymap, commands}}};
  const calls: string[] = [];
  const s = {store: {getState: () => state, dispatch: (a: Action) => {state = reduce(state,a) as typeof state;}},
    registryAction: (id: string) => {calls.push(id);}, registryRow: (row: {id: string; choice?: {arguments: object}}) => {calls.push(row.choice ? `${row.id} ${JSON.stringify(row.choice.arguments)}` : row.id);}, may: () => !computing, awaitingMenu: () => false} as unknown as Session;
  return {s, calls, state: () => state};
}
test('W536: a key runs the command bound to it; modifier keys map Cmd to Ctrl', () => {
  const {s,calls} = session();
  for (const key of ['o','s','r']) handleKey(s,event(key,{ctrlKey:true}));
  assert.deepEqual(calls,['session.open_model','session.save','run.initial']);
  assert.equal(keyName(event('S',{metaKey:true,shiftKey:true})), 'Ctrl+Shift+S');
  assert.equal(keyName(event('Escape')), 'Escape');
});
test('W536: a plain letter runs nothing: there are no letter sequences', () => {
  const on = session();
  for (const key of ['i','g','f','s']) { const e = event(key); handleKey(on.s, e); assert.equal(e.prevented, false); }
  assert.deepEqual(on.calls, []);
});
test('W536: a key bound by the user runs its command, and the table\'s key of another stays', () => {
  const on = session();
  const info = on.state().keymap.info!;
  const bound = {...info, commands: info.commands.map(c => c.id === 'calculator.evaluate' ? {...c, keys: ['F9'], source: 'user' as const} : c)};
  assert.equal(commandOnKey(HELLO, bound, 'F9')?.id, 'calculator.evaluate');
  assert.equal(commandOnKey(HELLO, bound, 'Ctrl+O')?.id, 'session.open_model');
  assert.equal(commandOnKey(HELLO, bound, 'Ctrl+K Ctrl+S'), null);
});
test('W517: all reserved keys remain the browser/system keys', () => {
  const on = session();
  for (const e of [event('F5'),event('w',{ctrlKey:true}),event('F4',{altKey:true}),event('F12')]) {
    handleKey(on.s,e); assert.equal(e.prevented,false);
  }
  assert.deepEqual(on.calls,[]);
});
test('W517: palette is a customizable registry key, and no default uses a reserved key', () => {
  assert.equal(commandOnKey(HELLO,HELLO.keymap,'Ctrl+Shift+P')?.id,'command_palette.open');
  const keys = registryRows(HELLO).flatMap(r => r.default_keys);
  assert.equal(new Set(keys).size, keys.length);
  assert.ok(keys.every(k => !HELLO.keymap.reserved.includes(k)));
});

/* W772: a binding and a pin name a command with its arguments (a command value), one of a menu_choice row's items or any other */
const EXPORT = {cmd: 'model.export', kind: 'd', step: true, label: 'Model as', name: 'File \u203a Export \u203a Model as', description: 'Write the model.', category: 'files',
  menu_choice: 'language', default_keys: [], arguments: [{name: 'language', type: 'choice', required: true, choices: ['python', 'python_numpy']}, {name: 'file', type: 'file', required: false}]};
const WITH_EXPORT = {...HELLO, menu_separator: ' \u203a ', commands: [...HELLO.commands.filter(c => c.cmd !== 'model.export'), EXPORT]} as unknown as HelloEvent;
function valued(keys: Record<string, string[]>, pinned: Pin[] = [], values: HelloEvent['keymap']['values'] = []) {
  const on = session();
  const commands = [...registryRows(WITH_EXPORT).map(row => ({id: row.id, keys: row.default_keys, source: 'default' as const})),
    ...Object.entries(keys).map(([id, k]) => ({id, keys: k, source: 'user' as const}))];
  on.s.store.dispatch({type: 'event', ev: WITH_EXPORT} as unknown as Action);
  const info = {...WITH_EXPORT.keymap, commands, pinned, values};
  return {...on, info, hello: WITH_EXPORT};
}
test('W772: each menu_choice item is a bindable row beside its command, named by its command value', () => {
  const rows = bindableRows(WITH_EXPORT, null).filter(r => r.id === 'model.export');
  assert.deepEqual(rows.map(r => r.binding), ['model.export', 'model.export --language python', 'model.export --language python_numpy']);
  assert.deepEqual(rows.map(r => r.name), ['File \u203a Export \u203a Model as', 'File \u203a Export \u203a Model as \u203a Python', 'File \u203a Export \u203a Model as \u203a Python numpy']);
  assert.deepEqual(rows[1].default_keys, []);
});
test('W772: a key bound to an item runs it as the submenu click does; the bare command keeps its own', () => {
  const {s, calls, info} = valued({'model.export --language python': ['Ctrl+E']});
  assert.equal(commandOnKey(WITH_EXPORT, info, 'Ctrl+E')?.binding, 'model.export --language python');
  const state = {...s.store.getState(), keymap: {...initialState.keymap, info}};
  (s as unknown as {store: {getState: () => unknown}}).store.getState = () => state;
  handleKey(s, event('e', {ctrlKey: true}));
  assert.deepEqual(calls, ['model.export {"language":"python"}']);
});
test('W772: a pin names an item, or a command value no row lists, and the second becomes a row', () => {
  const other = 'model.export --language python --file "a.py"';
  const {info} = valued({}, [{name: 'A', steps: ['model.export --language python_numpy']}, {name: 'B', steps: [other]}],
    [{value: other, run: {cmd: 'model.export', language: 'python', file: 'a.py'}}]);
  const rows = bindableRows(WITH_EXPORT, info);
  const row = rows.find(r => r.binding === other)!;
  assert.deepEqual(row.choice?.arguments, {language: 'python', file: 'a.py'});
  assert.ok(rows.some(r => r.binding === 'model.export --language python_numpy'));
});
test('W772: a bound item is one row (the item\'s own), whether or not the core lists its value', () => {
  const item = 'model.export --language python';
  const {info} = valued({[item]: ['Ctrl+E']}, [{name: 'Python', steps: [item]}], [{value: item, run: {cmd: 'model.export', language: 'python'}}]);
  const rows = bindableRows(WITH_EXPORT, info).filter(r => r.binding === item);
  assert.equal(rows.length, 1);
  assert.equal(bindableRows(WITH_EXPORT, info).length, new Set(bindableRows(WITH_EXPORT, info).map(r => r.binding)).size);
});
