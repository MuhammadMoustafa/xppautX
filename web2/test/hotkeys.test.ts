import {test} from 'node:test';
import assert from 'node:assert/strict';
import {commandOnKey, handleKey, keyName} from '../src/ui/hotkeys';
import {REGISTRY_HELLO as HELLO} from './hello';
import {registryRows} from '../src/ui/navigation';
import {initialState, reduce, type Action} from '../src/store/state';
import type {Session} from '../src/session';

const event = (key: string, mods: Partial<KeyboardEvent> = {}) => ({key, code:'', defaultPrevented:false, ctrlKey:false, altKey:false, shiftKey:false, metaKey:false, target:null,
  prevented:false, preventDefault() {(this as {prevented:boolean}).prevented = true;}, ...mods}) as unknown as KeyboardEvent & {prevented: boolean};
function session(computing = false) {
  const commands = registryRows(HELLO).map(row => ({id:row.id, keys:row.default_keys, source:'default' as const}));
  let state = {...initialState, hello: HELLO, computing, keymap:{...initialState.keymap, info:{...HELLO.keymap, commands}}};
  const calls: string[] = [];
  const s = {store: {getState: () => state, dispatch: (a: Action) => {state = reduce(state,a) as typeof state;}},
    registryAction: (id: string) => {calls.push(id);}, may: () => !computing, awaitingMenu: () => false} as unknown as Session;
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
  const bound = {...info, commands: info.commands.map(c => c.id === 'tool.calculator' ? {...c, keys: ['F9'], source: 'user' as const} : c)};
  assert.equal(commandOnKey(HELLO, bound, 'F9')?.id, 'tool.calculator');
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
  assert.equal(commandOnKey(HELLO,HELLO.keymap,'Ctrl+Shift+P')?.id,'tool.command_palette');
  const keys = registryRows(HELLO).flatMap(r => r.default_keys);
  assert.equal(new Set(keys).size, keys.length);
  assert.ok(keys.every(k => !HELLO.keymap.reserved.includes(k)));
});
