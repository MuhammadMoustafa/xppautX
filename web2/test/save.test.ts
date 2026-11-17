import assert from 'node:assert/strict';
import {test} from 'node:test';
import {Session} from '../src/session';
import type {AskEvent, XppEvent, Command} from '../src/protocol/types';
import type {FilesApi} from '../src/protocol/files';
import type {Transport} from '../src/protocol/transport';
import {HELLO} from './hello';

/* a session that has its hello: the core's asks and events come through `emit`, its commands are in `sent` */
function saving() {
  let emit: (ev: XppEvent) => void = () => {};
  const reads: string[] = [];
  const downloads: string[] = [];
  const sent: Command[] = [];
  const files: FilesApi = {
    list: async () => [],
    get: async name => { reads.push(name); return null; },
    download: async name => { downloads.push(name); return null; },
    put: async () => { throw new Error('unexpected upload'); },
  };
  const transport: Transport = {send: cmd => { sent.push(cmd); }, close: () => {}, open: on => { emit = on; }};
  const s = new Session(transport, files);
  s.start();
  emit(HELLO);
  emit({ev: 'done', status: 'ok'});
  return {emit, reads, downloads, sent, s, files};
}

/* W786 (#866, round 3): in a browser every save the core asks for is a download under its default name: answered at once, into the
   scratch folder the core wrote (scratch: true), fetched by that name once the command is done */
test('W786: a browser save is answered at once under the default name, into the scratch folder', () => {
  const {emit, sent} = saving();
  emit({ev: 'ask', kind: 'file', id: 1, mode: 'write', wild: '*.snapx', file: 'lecar.snapx'} as XppEvent);
  assert.deepEqual(sent.at(-1), {cmd: 'answer', id: 1, file: 'lecar.snapx', scratch: true});
});

test('W786: a browser save is fetched by its name once the core has written it, and downloaded', async () => {
  const {emit, downloads} = saving();
  emit({ev: 'ask', kind: 'file', id: 2, mode: 'write', wild: '*.dat', file: 'fig1.dat'} as XppEvent);
  emit({ev: 'done', status: 'ok', result: {file: '/model/.xppautx/tmp/xppautoX-1-1/fig1.dat'}} as XppEvent);
  await new Promise(resolve => setTimeout(resolve, 0));
  assert.deepEqual(downloads, ['fig1.dat']);
});

test('W129: a declined save, failure or missing save result fetches nothing', () => {
  for (const result of [undefined, false]) {
    const {emit, reads, downloads} = saving();
    if (result !== undefined) emit({ev: 'session.saved', saved: result, file: 'old.snapx'});
    emit({ev: 'done', status: 'ok'});
    assert.deepEqual(reads, []);
    assert.deepEqual(downloads, []);
  }
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


test('W303: a denied native save retries only after idle, in the writable model folder', async () => {
  const original = Object.getOwnPropertyDescriptor(globalThis, 'window');
  const requests: {dir: string}[] = [];
  Object.defineProperty(globalThis, 'window', {configurable: true, value: {
    __xppFileDialog: async (request: {dir: string}) => { requests.push(request); return '/protected/data.csv'; },
  }});
  try {
    const {s, sent, emit} = saving();
    emit({ev: 'done', status: 'ok'});
    s.registryChoice('data.export', {format: 'csv', target: 'table'});
    const ask: AskEvent = {ev: 'ask', kind: 'file', id: 7, mode: 'write', file: '/protected/data.csv',
      dir: '/protected', save_dir: '/model', wild: '*.csv'};
    s.store.dispatch({type: 'event', ev: ask});
    await s.nativeFile(ask);
    const before = sent.length;
    emit({ev: 'session.saved', saved: false, file: '/protected/data.csv'});
    emit({ev: 'session.message', error: 'cannot open data.csv: Permission denied: the folder /protected needs administrator rights; choose another place'});
    assert.equal(sent.length, before);
    emit({ev: 'done', status: 'ok'});
    assert.deepEqual(sent.at(-1), {cmd: 'data.export', target: 'table', format: 'csv'});
    const retry = {...ask, id: 8};
    s.store.dispatch({type: 'event', ev: retry});
    await s.nativeFile(retry);
    assert.deepEqual(requests.map(r => r.dir), ['/protected/', '/model']);
  } finally {
    if (original) Object.defineProperty(globalThis, 'window', original);
    else Reflect.deleteProperty(globalThis, 'window');
  }
});
