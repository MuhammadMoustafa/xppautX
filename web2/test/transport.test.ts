/* The wire (protocol/transport.ts): commands go out one POST at a time, in
   the order they were sent, and a failed POST does not stop the next. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {HttpTransport} from '../src/protocol/transport';

test('each command is POSTed once the one before it was answered, in order', async () => {
  const started: string[] = [];
  const finish: (() => void)[] = [];
  const real = globalThis.fetch;
  globalThis.fetch = ((_url: string, init: {body: string}) => {
    started.push(init.body);
    return new Promise<Response>((resolve, reject) => {
      finish.push(started.length === 2 ? () => reject(new Error('refused')) : () => resolve(new Response(null)));
    });
  }) as unknown as typeof fetch;
  try {
    const t = new HttpTransport('?t=x', '/');
    t.send({cmd: 'key', key: 'a'});
    t.send({cmd: 'answer', key: 'b'});
    t.send({cmd: 'abort'});
    const settle = () => new Promise(r => setTimeout(r, 0));
    await settle();
    assert.deepEqual(started.map(b => JSON.parse(b).cmd), ['key']);
    finish[0]();
    await settle();
    assert.deepEqual(started.map(b => JSON.parse(b).cmd), ['key', 'answer']);
    finish[1](); /* a failed POST: the next still goes */
    await settle();
    assert.deepEqual(started.map(b => JSON.parse(b).cmd), ['key', 'answer', 'abort']);
  } finally {
    globalThis.fetch = real;
  }
});
