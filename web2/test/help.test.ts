/* The Help view's pure logic (docs/roadmap.md W12b): the reducer
   (store/help.ts), the manual's search index (help/search.ts) and the
   in-page link matcher (help/links.ts manualLinkTarget), without a browser
   (npm test). virtual:manual resolves through build.mjs's esbuild plugin
   even here (build.mjs --test uses the same plugin), so a real chapter's
   HTML is what search runs against. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import manual from 'virtual:manual';
import {manualLinkTarget} from '../src/help/links';
import {searchManual} from '../src/help/search';
import {HELP_HOME, initialHelp, reduceHelp} from '../src/store/help';

test('the manual has every chapter, each with a title and headings', () => {
  assert.equal(manual.length, 16);
  assert.equal(manual[0].id, '01-introduction');
  assert.ok(manual.every(c => c.title.length > 0 && c.html.length > 0));
  const auto = manual.find(c => c.id === '09-auto')!;
  assert.ok(auto.headings.some(h => h.id === 'the-auto-view'));
  assert.ok(auto.headings.some(h => h.id === 'diagram-axes'));
});

test('reduceHelp: open with a target shows it, closes, "open" with none keeps the place', () => {
  const opened = reduceHelp(initialHelp, {type: 'open', target: {chapter: '09-auto', anchor: 'diagram-axes'}});
  assert.deepEqual(opened, {open: true, chapter: '09-auto', anchor: 'diagram-axes', query: ''});
  const closed = reduceHelp(opened, {type: 'close'});
  assert.equal(closed.open, false);
  assert.equal(closed.chapter, '09-auto', 'closing does not forget where it was');
  const reopened = reduceHelp(closed, {type: 'open'});
  assert.deepEqual(reopened, {...closed, open: true});
});

test('reduceHelp: a target with no anchor goes to the chapter\'s top', () => {
  const at = reduceHelp(initialHelp, {type: 'open', target: {chapter: '09-auto', anchor: 'diagram-axes'}});
  const top = reduceHelp(at, {type: 'go', target: {chapter: '09-auto'}});
  assert.equal(top.anchor, null);
});

test('reduceHelp: the query is kept, and starts at the home chapter', () => {
  assert.equal(initialHelp.chapter, HELP_HOME);
  const q = reduceHelp(initialHelp, {type: 'query', query: 'poincare'});
  assert.equal(q.query, 'poincare');
});

test('searchManual finds a known term (Diagram axes, 09-auto.md) by heading and by body text', () => {
  const byHeading = searchManual(manual, 'diagram axes');
  assert.ok(byHeading.some(h => h.chapter === '09-auto' && h.anchor === 'diagram-axes'));
  const byText = searchManual(manual, 'Doedel'); /* AUTO's own preamble names its author */
  assert.ok(byText.some(h => h.chapter === '09-auto'));
});

test('searchManual is case-insensitive and matches nothing for a blank query', () => {
  assert.deepEqual(searchManual(manual, '   '), []);
  const hits = searchManual(manual, 'POINCARE');
  assert.ok(hits.length > 0);
});

test('manualLinkTarget reads a manual cross-reference, with or without an anchor', () => {
  assert.deepEqual(manualLinkTarget('06-numerical-parameters.md#poincare-map', '05-commands'),
    {chapter: '06-numerical-parameters', anchor: 'poincare-map'});
  assert.deepEqual(manualLinkTarget('05-commands.md', '01-introduction'), {chapter: '05-commands', anchor: undefined});
  assert.deepEqual(manualLinkTarget('#the-auto-view', '09-auto'), {chapter: '09-auto', anchor: 'the-auto-view'});
});

test('manualLinkTarget leaves an ordinary link (mailto:, an outside doc) alone', () => {
  assert.equal(manualLinkTarget('mailto:doedel@cs.concordia.edu', '09-auto'), null);
  assert.equal(manualLinkTarget('docs/protocol.md', '01-introduction'), null);
  assert.equal(manualLinkTarget('https://example.com', '01-introduction'), null);
});
