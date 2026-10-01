/* The files slice (docs/ui-v2.md section 4, store/files.ts): names the core
   takes, Keep both names, which picked file answers the ask, the replace
   decision, the command record and the missing file of an error. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {
  answerName, baseName, initialFiles, keepBothName, matchesWild, menuKeys, onSent, safeName, uploadPlan,
} from '../src/store/files';
import type {AskEvent} from '../src/protocol/types';
import {nativeFileRequest, wildExtensions} from '../src/pickers';
import {HELLO, READY} from './hello';
import {reduce} from '../src/store/state';

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
  let s = onSent(initialFiles, HELLO, {cmd: 'key', key: 'f'}, null, 0);
  assert.deepEqual(s.run, {menu: 0, cmd: {cmd: 'key', key: 'f'}, answers: []});
  s = onSent(s, HELLO, {cmd: 'key', key: 'r'}, null, 0); /* the core has not said menu 1 yet */
  assert.equal(s.run!.menu, 1, 'F switched to the File menu');
  s = onSent(s, HELLO, {cmd: 'answer', id: 7, file: 'x.set'}, fileAsk('read'), 1);
  assert.deepEqual(s.run!.answers, [{kind: 'file', mode: 'read', fields: {file: 'x.set'}}]);
  assert.equal(onSent(s, HELLO, {cmd: 'state'}, null, 0), s, 'a query is no command of the user');
  assert.equal(onSent(s, HELLO, {cmd: 'browser', from: 0, count: 10}, null, 0), s, 'nor a table block');
  assert.equal(onSent(s, null, {cmd: 'key', key: 'i'}, null, 0), s, 'nothing before hello');
});

test('menuKeys: F or U from the main menu, nothing when already there', () => {
  assert.deepEqual(menuKeys(HELLO, 0, 1), ['f']);
  assert.deepEqual(menuKeys(HELLO, 0, 2), ['u']);
  assert.deepEqual(menuKeys(HELLO, 1, 1), []);
  assert.equal(menuKeys(HELLO, 1, 0), null);
});

test('an error about a file the command could not read gets "Add file…"', () => {
  let s = reduce(READY, {type: 'sent', cmd: {cmd: 'key', key: 'r'}});
  s = reduce(s, {type: 'event', ev: fileAsk('read')});
  s = reduce(s, {type: 'sent', cmd: {cmd: 'answer', id: 7, file: 'gone.set'}});
  s = reduce(s, {type: 'event', ev: {ev: 'message', error: 'Cannot open file'}});
  assert.equal(s.toasts[s.toasts.length - 1].action, undefined, 'the core names no file: no offer');
  s = reduce(s, {type: 'event', ev: {ev: 'message', error: 'Cannot open file', file: 'gone.set'}});
  const t = s.toasts[s.toasts.length - 1];
  assert.equal(t.action?.name, 'gone.set');
  assert.deepEqual(t.action?.run?.answers.map(a => a.fields.file), ['gone.set']);
  assert.ok(s.files.runFailed);
  s = reduce(s, {type: 'sent', cmd: {cmd: 'key', key: 'i'}});
  assert.ok(!s.files.runFailed, 'a new command starts clean');
});

test('the picker filter comes from a pattern of the ask', () => {
  assert.deepEqual(wildExtensions('*.set'), ['.set']);
  assert.deepEqual(wildExtensions('*.DAT *.tab'), ['.dat', '.tab']);
  assert.deepEqual(wildExtensions('*.dat, *.tab'), ['.dat', '.tab']);
  for (const none of ['*', '', undefined, '*.*', '*.pars*', 'x*.set', '*.set *']) assert.deepEqual(wildExtensions(none), [], String(none));
});

test('the desktop window\'s own dialog (W88) is asked with the filter of wildExtensions, the folder and the name', () => {
  assert.deepEqual(nativeFileRequest({title: 'Load SET File', mode: 'read', file: 'lecar.ode.set', wild: '*.set', dir: '/m/'}),
    {mode: 'read', title: 'Load SET File', dir: '/m/', file: 'lecar.ode.set', wild: '*.set', exts: ['.set']});
  const save = nativeFileRequest({title: 'Save data', mode: 'write', file: 'data.dat', wild: '*.DAT *.tab', dir: 'C:\\m'});
  assert.equal(save.mode, 'write');
  assert.deepEqual(save.exts, ['.dat', '.tab']);
  assert.equal(save.dir, 'C:\\m');
  /* a pattern that is not *.ext words filters nothing (All files alone) */
  assert.deepEqual(nativeFileRequest({title: 'Open model', mode: 'read', file: '', wild: '*.ode*', dir: '/m/'}).exts, []);
  /* a full path in `file` names the folder too */
  assert.deepEqual([nativeFileRequest({file: '/home/u/models/', dir: '/m/'}).dir, nativeFileRequest({file: '/home/u/models/'}).file],
    ['/home/u/models/', '']);
  const win = nativeFileRequest({file: 'C:\\Users\\u\\x.set', dir: 'D:\\'});
  assert.deepEqual([win.dir, win.file, win.mode], ['C:\\Users\\u\\', 'x.set', 'read']);
  /* a relative path keeps the ask's folder, the base name offered */
  assert.deepEqual([nativeFileRequest({file: 'sub/x.set', dir: '/m/'}).dir, nativeFileRequest({file: 'sub/x.set', dir: '/m/'}).file],
    ['/m/', 'x.set']);
});
