/* The files slice (docs/ui-v2.md section 4, store/files.ts): names the core
   takes, Keep both names, which picked file answers the ask, the replace
   decision, the command record and the missing file of an error. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {
  answerName, baseName, initialFiles, keepBothName, matchesWild, menuKeys, missingFile, onSent, safeName, uploadPlan,
  type RunRecord,
} from '../src/store/files';
import type {AskEvent} from '../src/protocol/types';
import {initialState, reduce} from '../src/store/state';

test('safeName follows the core: base names only', () => {
  for (const ok of ['lecar.set', 'a b-2.ode', 'données.dat', 'x', 'console.txt', 'n'.repeat(255)]) assert.ok(safeName(ok), ok);
  for (const bad of ['', '.', '..', '../x', 'a/b', 'a\\b', 'C:x', '.hidden', 'a..b', 'x.set.', 'x.set ', ' x', 'a\tb',
    'a|b', 'a*b', 'a?b', 'a<b', 'a"b', 'CON', 'con.txt', 'Nul', 'com1.dat', 'n'.repeat(256), 'é'.repeat(128)])
    assert.ok(!safeName(bad), JSON.stringify(bad));
});

test('baseName takes either separator', () => {
  assert.equal(baseName('/home/u/lecar.ode.set'), 'lecar.ode.set');
  assert.equal(baseName('C:\\m\\x.set'), 'x.set');
  assert.equal(baseName('x.set'), 'x.set');
});

test('Keep both picks the first free name-N.ext', () => {
  assert.equal(keepBothName('lecar.set', ['lecar.set']), 'lecar-2.set');
  assert.equal(keepBothName('lecar.set', ['lecar.set', 'lecar-2.set']), 'lecar-3.set');
  assert.equal(keepBothName('table', ['table']), 'table-2');
  assert.equal(keepBothName('a.b.dat', []), 'a.b-2.dat');
});

test('the ask is answered with the picked file its pattern matches', () => {
  assert.ok(matchesWild('x.SET', '*.set'));
  assert.ok(matchesWild('a.pars1', '*.pars*'));
  assert.ok(!matchesWild('x.tab', '*.set'));
  assert.equal(answerName(['t.tab', 'x.set'], '*.set'), 'x.set');
  assert.equal(answerName(['t.tab', 'u.tab'], '*.set'), 't.tab');
});

test('an upload is copied, skipped or confirmed against the folder', () => {
  const listing = [{name: 'a.set', size: 3, mtime: 0, sha256: 'aa'}];
  assert.equal(uploadPlan('b.set', 'aa', listing), 'copy');
  assert.equal(uploadPlan('a.set', 'aa', listing), 'same');
  assert.equal(uploadPlan('a.set', 'bb', listing), 'confirm');
});

const fileAsk = (mode: 'read' | 'write'): AskEvent => ({ev: 'ask', id: 7, kind: 'file', title: 'Load SET File', mode});

test('the run record: the command, the menu it ran in, and its answers', () => {
  let s = onSent(initialFiles, {cmd: 'key', key: 'f'}, null, 0);
  assert.deepEqual(s.run, {menu: 0, cmd: {cmd: 'key', key: 'f'}, answers: []});
  s = onSent(s, {cmd: 'key', key: 'r'}, null, 0); /* the core has not said menu 1 yet */
  assert.equal(s.run!.menu, 1, 'F switched to the File menu');
  s = onSent(s, {cmd: 'answer', id: 7, file: 'x.set'}, fileAsk('read'), 1);
  assert.deepEqual(s.run!.answers, [{kind: 'file', mode: 'read', fields: {file: 'x.set'}}]);
  assert.equal(onSent(s, {cmd: 'state'}, null, 0), s, 'a query is no command of the user');
  assert.equal(onSent(s, {cmd: 'browser', from: 0, count: 10}, null, 0), s, 'nor a table block');
});

test('missingFile names the file an error is about', () => {
  const run: RunRecord = {menu: 1, cmd: {cmd: 'key', key: 'r'}, answers: [
    {kind: 'file', mode: 'read', fields: {file: '/some/where/gone.set'}}]};
  assert.equal(missingFile('Cannot open file', run), 'gone.set');
  assert.equal(missingFile("Couldn't open file", run), 'gone.set');
  assert.equal(missingFile('Incompatible parameters', run), null);
  assert.equal(missingFile('Cannot open file', null), null);
  assert.equal(missingFile('File<tab.tab> not found in /m', null), 'tab.tab');
  const written: RunRecord = {...run, answers: [{kind: 'file', mode: 'write', fields: {file: 'out.set'}}]};
  assert.equal(missingFile('Cannot open file', written), null, 'a file being written is not missing');
});

test('menuKeys: F or U from the main menu, nothing when already there', () => {
  assert.deepEqual(menuKeys(0, 1), ['f']);
  assert.deepEqual(menuKeys(0, 2), ['u']);
  assert.deepEqual(menuKeys(1, 1), []);
  assert.equal(menuKeys(1, 0), null);
});

test('an error of a command that read a file gets "Add file…"', () => {
  let s = reduce(initialState, {type: 'sent', cmd: {cmd: 'key', key: 'r'}});
  s = reduce(s, {type: 'event', ev: fileAsk('read')});
  s = reduce(s, {type: 'sent', cmd: {cmd: 'answer', id: 7, file: 'gone.set'}});
  s = reduce(s, {type: 'event', ev: {ev: 'message', error: 'Cannot open file'}});
  const t = s.toasts[s.toasts.length - 1];
  assert.equal(t.action?.name, 'gone.set');
  assert.deepEqual(t.action?.run?.answers.map(a => a.fields.file), ['gone.set']);
  assert.ok(s.files.runFailed);
  s = reduce(s, {type: 'sent', cmd: {cmd: 'key', key: 'i'}});
  assert.ok(!s.files.runFailed, 'a new command starts clean');
});
