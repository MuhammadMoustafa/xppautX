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
  return {emit, reads, sent, s, files};
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

test('W177: every committed output is captured before the next prompt', () => {
  const {emit, reads} = saving();
  emit({ev: 'saved', saved: true, file: 'other.snapx'});
  assert.deepEqual(reads, ['other.snapx']);
  emit({ev: 'saved', saved: true, file: 'old.snapx'});
  assert.deepEqual(reads, ['other.snapx', 'old.snapx']);
  emit({ev: 'idle'});
  assert.deepEqual(reads, ['other.snapx', 'old.snapx']);
});

test('W129: a committed destination with the dialog suffix delivers the core filename', () => {
  const {emit, reads} = saving('session');
  emit({ev: 'saved', saved: true, file: 'session.snapx'});
  emit({ev: 'idle'});
  assert.deepEqual(reads, ['session.snapx']);
});


test('W130: page saves use hello names, even when they differ from the model filename', () => {
  const {s, sent} = saving();
  s.saveValues('par');
  assert.equal(sent.at(-1)?.name, HELLO.output_names.par);
  s.writeDataFile('table', 'csv');
  assert.equal(sent.at(-1)?.name, HELLO.output_names.csv);
  s.writeDataFile('plot', 'csv');
  assert.equal(sent.at(-1)?.name, HELLO.output_names.curves);
});


test('W177: desktop Values and CSV buttons ask the core for a picker destination', () => {
  const original = Object.getOwnPropertyDescriptor(globalThis, 'window');
  Object.defineProperty(globalThis, 'window', {configurable: true, value: {__xppFileDialog: async () => null}});
  try {
    const {s, sent} = saving();
    s.saveValues('par');
    assert.deepEqual(sent.at(-1), {cmd: 'values', op: 'write', kind: 'par'});
    s.saveValues('ic');
    assert.deepEqual(sent.at(-1), {cmd: 'values', op: 'write', kind: 'ic'});
    for (const what of ['table', 'plot'] as const) {
      s.writeDataFile(what, 'csv');
      assert.deepEqual(sent.at(-1), {cmd: 'browser', op: 'write', what, format: 'csv'});
    }
  } finally {
    if (original) Object.defineProperty(globalThis, 'window', original);
    else Reflect.deleteProperty(globalThis, 'window');
  }
});

test('W177: same-name exports snapshot distinct bytes before answering the next file ask', async () => {
  const {s, emit, sent, files} = saving('data.dat');
  let bytes = 'orbit';
  const captured: string[] = [];
  let finish: () => void = () => {};
  Object.assign(files, {get: () => {
    const snapshot = new Blob([bytes]);
    return new Promise<Blob>(resolve => { finish = () => resolve(snapshot); });
  }});
  let done: () => void = () => {};
  const delivered = new Promise<void>(resolve => { done = resolve; });
  const handle = {name: 'data.dat', createWritable: async () => ({
    write: async (data: Blob) => { captured.push(await data.text()); if (captured.length === 2) done(); }, close: async () => {},
  })};
  const first: AskEvent = {ev: 'ask', kind: 'file', mode: 'write', id: 3};
  s.saveFile(first, 'data.dat', handle);
  emit({ev: 'saved', saved: true, file: 'data.dat'});
  const second = {...first, id: 4};
  s.saveFile(second, 'data.dat', handle);
  assert.equal(sent.at(-1)?.id, 3, 'the next answer waits for the orbit read');
  finish();
  await Promise.resolve(); await Promise.resolve(); await Promise.resolve();
  assert.equal(sent.at(-1)?.id, 4);
  bytes = 'adjoint';
  emit({ev: 'saved', saved: true, file: 'data.dat'});
  finish();
  emit({ev: 'idle'});
  await delivered;
  assert.deepEqual(captured, ['orbit', 'adjoint']);
});
