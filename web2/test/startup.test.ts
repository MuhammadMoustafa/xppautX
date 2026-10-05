import assert from 'node:assert/strict';
import {test} from 'node:test';
import {Session} from '../src/session';
import type {AskEvent, Command} from '../src/protocol/types';

const ask: AskEvent = {ev: 'ask', id: 1, kind: 'file', mode: 'read', title: 'Open model'};
function startup() {
  const sent: Command[] = [];
  const session = new Session({send: cmd => { sent.push(cmd); }, open: () => {}, close: () => {}});
  session.store.dispatch({type: 'event', ev: ask});
  return {session, sent};
}

test('startup model selection and cancellation reach the core before hello', async () => {
  const original = Object.getOwnPropertyDescriptor(globalThis, 'window');
  try {
    for (const path of ['C:/models/lecar.odex', null]) {
      Object.defineProperty(globalThis, 'window', {configurable: true, value: {__xppFileDialog: async () => path}});
      const {session, sent} = startup();
      await session.nativeFile(ask);
      assert.deepEqual(sent, [path ? {cmd: 'answer', id: 1, file: path} : {cmd: 'answer', id: 1, ok: 0}]);
    }
  } finally {
    if (original) Object.defineProperty(globalThis, 'window', original);
    else Reflect.deleteProperty(globalThis, 'window');
  }
});

test('startup close ends the desktop window without asking to save an unloaded model', () => {
  const original = Object.getOwnPropertyDescriptor(globalThis, 'window');
  let closed = 0;
  try {
    Object.defineProperty(globalThis, 'window', {configurable: true, value: {__xppCloseWindow: async () => { closed++; }}});
    const {session, sent} = startup();
    session.quitAsked();
    assert.equal(closed, 0, 'Quit reaches the waiting core before closing its native picker');
    assert.deepEqual(sent, [{cmd: 'quit'}]);
    Object.defineProperty(globalThis, 'window', {configurable: true, value: {}});
    session.quitAsked();
    assert.deepEqual(sent, [{cmd: 'quit'}, {cmd: 'quit'}]);
  } finally {
    if (original) Object.defineProperty(globalThis, 'window', original);
    else Reflect.deleteProperty(globalThis, 'window');
  }
});
