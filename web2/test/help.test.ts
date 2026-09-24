/* The Help view's pure logic (docs/roadmap.md W12b): the reducer
   (store/help.ts), the manual's search index (help/search.ts) and the
   in-page link matcher (help/links.ts manualLinkTarget), without a browser
   (npm test). A small fixture stands in for the real manual (fetched at
   runtime from dist/manual.json, not bundled): manualBuild.mjs's own
   output is checked by `npm run build && npm run check` instead, which
   regenerate and compare dist/manual.json against docs/manual/*.md. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {manualLinkTarget} from '../src/help/links';
import type {ManualChapter} from '../src/help/manual';
import {searchManual} from '../src/help/search';
import {HELP_HOME, initialHelp, reduceHelp} from '../src/store/help';

const MANUAL: ManualChapter[] = [
  {
    id: '01-introduction', title: 'Introduction',
    html: '<p>What XPP is; Poincare maps are mentioned here, before any heading.</p>\n'
      + '<h2 id="environment-variables">Environment variables</h2>\n<p>XPPRC and friends.</p>',
    headings: [{id: 'environment-variables', text: 'Environment variables', level: 2}],
  },
  {
    id: '09-auto', title: 'Auto interface',
    html: '<p>AUTO was written by Doedel.</p>\n'
      + '<h2 id="the-auto-view">The AUTO view</h2>\n<p>Its status strip and Output.</p>\n'
      + '<h2 id="diagram-axes">Diagram axes</h2>\n<p>Choose what each axis plots.</p>',
    headings: [{id: 'the-auto-view', text: 'The AUTO view', level: 2}, {id: 'diagram-axes', text: 'Diagram axes', level: 2}],
  },
];

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

test('reduceHelp: an empty-string anchor (a search hit above a heading) counts as none', () => {
  const at = reduceHelp(initialHelp, {type: 'open', target: {chapter: '01-introduction', anchor: ''}});
  assert.equal(at.anchor, null);
});

test('reduceHelp: the query is kept, and starts at the home chapter', () => {
  assert.equal(initialHelp.chapter, HELP_HOME);
  const q = reduceHelp(initialHelp, {type: 'query', query: 'poincare'});
  assert.equal(q.query, 'poincare');
});

test('searchManual finds a known term by heading and by body text', () => {
  const byHeading = searchManual(MANUAL, 'diagram axes');
  assert.ok(byHeading.some(h => h.chapter === '09-auto' && h.anchor === 'diagram-axes'));
  const byText = searchManual(MANUAL, 'Doedel');
  assert.ok(byText.some(h => h.chapter === '09-auto'));
});

test('searchManual: a hit above the first heading has an empty anchor (the chapter\'s top)', () => {
  const hits = searchManual(MANUAL, 'Poincare');
  assert.ok(hits.some(h => h.chapter === '01-introduction' && h.anchor === ''));
});

test('searchManual is case-insensitive and matches nothing for a blank query', () => {
  assert.deepEqual(searchManual(MANUAL, '   '), []);
  const hits = searchManual(MANUAL, 'POINCARE');
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
