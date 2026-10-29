import {test} from 'node:test';
import assert from 'node:assert/strict';
import {handleKey, keyName, resolveKeys} from '../src/ui/hotkeys';
import {REGISTRY_HELLO as HELLO} from './hello';
import {registryRows} from '../src/ui/navigation';
import {initialState, reduce, type Action} from '../src/store/state';
import type {Session} from '../src/session';

const event = (key: string, mods: Partial<KeyboardEvent> = {}) => ({key, code:'', defaultPrevented:false, ctrlKey:false, altKey:false, shiftKey:false, metaKey:false, target:null,
  prevented:false, preventDefault() {(this as {prevented:boolean}).prevented = true;}, ...mods}) as unknown as KeyboardEvent & {prevented: boolean};
function session(preset = 'default', computing = false) {
  const commands = registryRows(HELLO).map(row => ({id:row.id, keys:preset === 'xppaut' ? row.legacy_keys : row.default_keys, source:'default' as const}));
  let state = {...initialState, hello: HELLO, computing, keymap:{...initialState.keymap, info:{...HELLO.keymap, preset:preset as 'default' | 'xppaut', commands}}};
  const calls: string[] = [];
  const s = {store: {getState: () => state, dispatch: (a: Action) => {state = reduce(state,a) as typeof state;}},
    registryAction: (id: string) => {calls.push(id);}, may: () => !computing, awaitingMenu: () => false} as unknown as Session;
  return {s, calls, state: () => state};
}
test('W517: logical keys run registry commands; modifier keys map Cmd to Ctrl', () => {
  const {s,calls} = session();
  for (const key of ['o','s','r']) handleKey(s,event(key,{ctrlKey:true}));
  assert.deepEqual(calls,['session.open_model','session.save','run.initial']);
  assert.equal(keyName(event('S',{metaKey:true,shiftKey:true})), 'Ctrl+Shift+S');
  assert.equal(keyName(event('Escape')), 'Escape');
});
test('W517: legacy IG resolves the same registry command; its prefix waits and Escape cancels', () => {
  const on = session('xppaut');
  handleKey(on.s,event('i'));
  assert.deepEqual(on.state().pendingKeys,['I']);
  handleKey(on.s,event('g'));
  assert.deepEqual(on.calls,['run.initial']);
  handleKey(on.s,event('i'));
  handleKey(on.s,event('Escape'));
  assert.deepEqual(on.state().pendingKeys,[]);
  const off = session();
  handleKey(off.s,event('i')); handleKey(off.s,event('g'));
  assert.deepEqual(off.calls,[]);
});
test('W517: all reserved keys remain the browser/system keys', () => {
  const on = session();
  for (const e of [event('F5'),event('w',{ctrlKey:true}),event('F4',{altKey:true}),event('F12')]) {
    handleKey(on.s,e); assert.equal(e.prevented,false);
  }
  assert.deepEqual(on.calls,[]);
});
test('W517: palette is a customizable registry key, and no default uses a reserved key', () => {
  assert.equal(resolveKeys(HELLO,HELLO.keymap,['Ctrl+Shift+P']).row?.id,'tool.command_palette');
  const keys = registryRows(HELLO).flatMap(r => r.default_keys);
  assert.equal(new Set(keys).size, keys.length);
  assert.ok(keys.every(k => !HELLO.keymap.reserved.includes(k)));
});

test('W517: XPPAUT window contexts shadow workspace letters through the same registry dispatcher', () => {
  const on = session('xppaut');
  const info = on.state().keymap.info;
  assert.equal(resolveKeys(HELLO, info, ['R']).row?.id, 'window.redraw');
  assert.equal(resolveKeys(HELLO, info, ['R'], 'auto').row?.id, 'auto.run');
  assert.equal(resolveKeys(HELLO, info, ['F','A']).row?.id, 'auto.open');
  assert.equal(resolveKeys(HELLO, info, ['F','A'], 'auto').row?.id, 'auto.export_all_info');
  assert.equal(resolveKeys(HELLO, info, ['D'], 'array_plot').row?.id, 'array_plot.redraw');
});


test('W517: AUTO A waits without running, and every Axes plot-kind sequence opens the common command', () => {
  const on = session('xppaut');
  const info = on.state().keymap.info;
  assert.deepEqual(resolveKeys(HELLO, info, ['A'], 'auto'), {row: null, more: true});
  for (const second of ['H','N','I','P','T']) assert.equal(resolveKeys(HELLO, info, ['A',second], 'auto').row?.id, 'auto.configure_plot');
  assert.equal(resolveKeys(HELLO, info, ['A','V'], 'auto').row?.id, 'auto.new_view');
});
