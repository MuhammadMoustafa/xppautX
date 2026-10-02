/* One upload route (W134, GitHub #186): every file the page copies into the
   model's folder goes through session.ts `upload`, so Values > Load asks
   before it replaces a different file of the same name, as a file ask's
   pick does (npm test, through session.ts with a files API that records). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {Session} from '../src/session';
import type {Command, XppEvent} from '../src/protocol/types';
import type {Transport} from '../src/protocol/transport';
import {sha256Hex, type FilesApi} from '../src/protocol/files';
import type {FolderFile} from '../src/store/files';
import {HELLO} from './hello';

async function session(folder: Map<string, string>) {
  const sent: Command[] = [], puts: string[] = [];
  let onEvent: (ev: XppEvent) => void = () => {};
  const transport: Transport = {
    send: cmd => { sent.push(cmd); },
    open: (on) => { onEvent = on; },
    close: () => {},
  };
  const files: FilesApi = {
    list: async () => Promise.all([...folder].map(async ([name, text]): Promise<FolderFile> =>
      ({name, size: text.length, mtime: 0, sha256: await sha256Hex(new Blob([text]))}))),
    get: async () => null,
    put: async (name, data) => {
      puts.push(name);
      folder.set(name, await data.text());
      return {name, size: data.size, sha256: await sha256Hex(data)};
    },
  };
  const s = new Session(transport, files);
  s.start();
  onEvent(HELLO as XppEvent);
  onEvent({ev: 'idle'} as XppEvent);
  sent.length = 0;
  return {s, sent, puts};
}

/* the upload's steps are promises: let them run until `ok`, a bounded number of turns */
async function settle(ok: () => boolean): Promise<boolean> {
  for (let i = 0; i < 200 && !ok(); i++) await new Promise(r => setTimeout(r, 0));
  return ok();
}

const reads = (sent: Command[]) => sent.filter(c => c.cmd === 'values' && c.op === 'read');

test('W134: Values > Load over a different file of the same name asks; Cancel copies and reads nothing', async () => {
  const folder = new Map([['lecar.par', 'old']]);
  const {s, sent, puts} = await session(folder);
  const done = s.loadValues('par', new File(['new'], 'lecar.par'));
  assert.ok(await settle(() => s.store.getState().files.confirm !== null), 'the confirm opens');
  assert.deepEqual(s.store.getState().files.confirm, {ask: null, name: 'lecar.par', keepBoth: 'lecar-2.par'},
    'in a dialog of its own (no file ask), with Keep both');
  s.resolveReplace('cancel');
  await done;
  assert.equal(folder.get('lecar.par'), 'old', 'the folder\'s file is untouched');
  assert.deepEqual(puts, [], 'nothing copied');
  assert.deepEqual(reads(sent), [], 'nothing read');
});

test('W134: Keep both copies it as name-2.ext and reads that; Replace copies over it', async () => {
  const folder = new Map([['lecar.par', 'old']]);
  const {s, sent, puts} = await session(folder);
  let done = s.loadValues('par', new File(['new'], 'lecar.par'));
  await settle(() => s.store.getState().files.confirm !== null);
  s.resolveReplace('keep');
  await done;
  assert.equal(folder.get('lecar.par'), 'old');
  assert.equal(folder.get('lecar-2.par'), 'new');
  assert.deepEqual(reads(sent).map(c => c.name), ['lecar-2.par']);
  done = s.loadValues('ic', new File(['newer'], 'lecar.par'));
  await settle(() => s.store.getState().files.confirm !== null);
  s.resolveReplace('replace');
  await done;
  assert.equal(folder.get('lecar.par'), 'newer');
  assert.deepEqual(puts, ['lecar-2.par', 'lecar.par']);
  assert.deepEqual(reads(sent).map(c => c.name), ['lecar-2.par', 'lecar.par']);
});

test('W134: the same content is not copied again, and nothing asks', async () => {
  const folder = new Map([['lecar.par', 'same']]);
  const {s, sent, puts} = await session(folder);
  await s.loadValues('par', new File(['same'], 'lecar.par'));
  assert.equal(s.store.getState().files.confirm, null);
  assert.deepEqual(puts, []);
  assert.deepEqual(reads(sent).map(c => c.name), ['lecar.par']);
});
