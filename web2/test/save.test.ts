import assert from 'node:assert/strict';
import {test} from 'node:test';
import {Session} from '../src/session';
import type {AskEvent, XppEvent, Command} from '../src/protocol/types';
import type {FilesApi} from '../src/protocol/files';
import type {Transport} from '../src/protocol/transport';
import {HELLO} from './hello';

function saving(name = 'old.snapx') {
  let emit: (ev: XppEvent) => void = () => {};
  const reads: string[] = [];
  const sent: Command[] = [];
  const files: FilesApi = {
    list: async () => [],
    get: async name => { reads.push(name); return null; },
    put: async () => { throw new Error('unexpected upload'); },
  };
  const transport: Transport = {send: cmd => { sent.push(cmd); }, close: () => {}, open: on => { emit = on; }};
  const s = new Session(transport, files);
  s.start();
  emit(HELLO);
  emit({ev: 'idle'});
  const ask: AskEvent = {ev: 'ask', kind: 'file', id: 1, mode: 'write', wild: '*.snapx'};
  s.store.dispatch({type: 'event', ev: ask});
  s.saveFile(ask, name, null);
  return {emit, reads, sent, s};
}

test('W129: a declined save, failure or missing save result never fetches the old file', () => {
  for (const result of [undefined, false]) {
    const {emit, reads} = saving();
    if (result !== undefined) emit({ev: 'saved', saved: result, file: 'old.snapx'});
    emit({ev: 'idle'});
    assert.deepEqual(reads, []);
  }
});

test('W129: a browser file answer carries no decision for the model-folder copy', () => {
  const {sent} = saving();
  assert.deepEqual(sent.at(-1), {cmd: 'answer', id: 1, file: 'old.snapx'});
});

test('W129: native save answers authorize the picked path; open answers do not', async () => {
  const original = Object.getOwnPropertyDescriptor(globalThis, 'window');
  Object.defineProperty(globalThis, 'window', {configurable: true, value: {
    __xppFileDialog: async () => '/chosen/file.snapx',
  }});
  try {
    for (const mode of ['write', 'read'] as const) {
      const {s, sent} = saving();
      const ask: AskEvent = {ev: 'ask', kind: 'file', id: 2, mode};
      s.store.dispatch({type: 'event', ev: ask});
      await s.nativeFile(ask);
      assert.deepEqual(sent.at(-1), mode === 'write'
        ? {cmd: 'answer', id: 2, file: '/chosen/file.snapx', replace: 1}
        : {cmd: 'answer', id: 2, file: '/chosen/file.snapx'});
    }
  } finally {
    if (original) Object.defineProperty(globalThis, 'window', original);
    else Reflect.deleteProperty(globalThis, 'window');
  }
});

test('W129: only a committed save of the selected destination delivers it', () => {
  const {emit, reads} = saving();
  emit({ev: 'saved', saved: true, file: 'other.snapx'});
  assert.deepEqual(reads, []);
  emit({ev: 'saved', saved: true, file: 'old.snapx'});
  assert.deepEqual(reads, [], 'wait for the command to end');
  emit({ev: 'idle'});
  assert.deepEqual(reads, ['old.snapx']);
});

test('W129: a committed destination with the dialog suffix delivers the core filename', () => {
  const {emit, reads} = saving('session');
  emit({ev: 'saved', saved: true, file: 'session.snapx'});
  emit({ev: 'idle'});
  assert.deepEqual(reads, ['session.snapx']);
});
