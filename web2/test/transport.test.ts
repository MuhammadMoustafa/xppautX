/* The wire (protocol/transport.ts): commands go out one POST at a time, in
   the order they were sent, and a failed POST does not stop the next; a
   command the server refused or that never reached it is said (W116), and
   the session shows it as an error. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {HttpTransport, type Transport} from '../src/protocol/transport';
import {Session} from '../src/session';
import type {XppEvent} from '../src/protocol/types';

/* fetch answering each POST with answer(n), n from 0; the bodies sent */
function fakeFetch(answer: (n: number) => Promise<Response>): {started: string[]; restore: () => void} {
  const started: string[] = [];
  const real = globalThis.fetch;
  globalThis.fetch = ((_url: string, init: {body: string}) => {
    started.push(init.body);
    return answer(started.length - 1);
  }) as unknown as typeof fetch;
  return {started, restore: () => { globalThis.fetch = real; }};
}

/* an EventSource that never connects: open() needs one, node has none */
function fakeEventSource(): () => void {
  const g = globalThis as unknown as {EventSource?: unknown};
  const real = g.EventSource;
  g.EventSource = class { close(): void {} };
  return () => { g.EventSource = real; };
}

const settle = () => new Promise(r => setTimeout(r, 0));

test('each command is POSTed once the one before it was answered, in order', async () => {
  const finish: (() => void)[] = [];
  const f = fakeFetch(n => new Promise<Response>((resolve, reject) => {
    finish.push(n === 1 ? () => reject(new Error('refused')) : () => resolve(new Response(null)));
  }));
  const es = fakeEventSource();
  try {
    const t = new HttpTransport('?t=x', '/');
    const failed: string[] = [];
    t.open(() => {}, () => {}, text => failed.push(text));
    t.send({cmd: 'key', key: 'a'});
    t.send({cmd: 'answer', key: 'b'});
    t.send({cmd: 'abort'});
    await settle();
    assert.deepEqual(f.started.map(b => JSON.parse(b).cmd), ['key']);
    finish[0]();
    await settle();
    assert.deepEqual(f.started.map(b => JSON.parse(b).cmd), ['key', 'answer']);
    finish[1](); /* a failed POST: said, and the next still goes */
    await settle();
    assert.deepEqual(f.started.map(b => JSON.parse(b).cmd), ['key', 'answer', 'abort']);
    assert.deepEqual(failed, ['The command answer did not reach xppautX (refused)']);
  } finally {
    f.restore();
    es();
  }
});

test('W116: a command the server refuses (403, 413, 400) is said with its status and reason', async () => {
  const f = fakeFetch(n => Promise.resolve(
    n === 0 ? new Response('bad token', {status: 403, statusText: 'Forbidden'})
      : n === 1 ? new Response('', {status: 413, statusText: 'Payload Too Large'})
        : new Response(null, {status: 200})));
  const es = fakeEventSource();
  try {
    const t = new HttpTransport('?t=x', '/');
    const failed: string[] = [];
    t.open(() => {}, () => {}, text => failed.push(text));
    t.send({cmd: 'key', key: 'a'});
    t.send({cmd: 'set', kind: 'par', name: 'iapp', value: 1});
    t.send({cmd: 'abort'});
    for (let i = 0; i < 5; i++) await settle();
    assert.equal(f.started.length, 3);
    assert.deepEqual(failed, ['The command key was refused (403: bad token)',
      'The command set was refused (413: Payload Too Large)']);
  } finally {
    f.restore();
    es();
  }
});

test('W116: the session shows a refused command as an error (Messages, the error dialog)', () => {
  let onFailed: (text: string) => void = () => {};
  const transport: Transport = {
    send: () => {},
    open: (_on: (ev: XppEvent) => void, _status: (open: boolean) => void, failed: (text: string) => void) => {
      onFailed = failed;
    },
    close: () => {},
  };
  const s = new Session(transport);
  s.start();
  onFailed('The command key was refused (403: bad token)');
  const st = s.store.getState();
  assert.deepEqual(st.toasts.map(t => [t.kind, t.text]), [['error', 'The command key was refused (403: bad token)']]);
});
