/* Text views (docs/ui-v2.md T16, store/text.ts): decoding the `source`
   event's comment tuples, pairing them with their source lines, and the
   reducer, without a browser (npm test). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import type {EquationsEvent, EquilibriumEvent, SourceEvent} from '../src/protocol/types';
import {attachComments, initialText, parseComments, reduceText} from '../src/store/text';

test('parseComments turns the [text, 0|1] tuples into typed flags', () => {
  const c = parseComments([['plain remark', 0], ['* set iapp=0.1', 1]]);
  assert.deepEqual(c, [{text: 'plain remark', hasAction: false}, {text: '* set iapp=0.1', hasAction: true}]);
});

test('attachComments pairs every quoted source line with the next comment, in order', () => {
  const lines = ['v\'=-v', '" plain remark', '" * set iapp=0.1', 'aux w=v'];
  const comments = parseComments([['plain remark', 0], ['* set iapp=0.1', 1]]);
  const out = attachComments(lines, comments);
  assert.equal(out.length, 4);
  assert.equal(out[0].comment, undefined, 'an equation line has no comment');
  assert.deepEqual(out[1].comment, {text: 'plain remark', hasAction: false, index: 0});
  assert.deepEqual(out[2].comment, {text: '* set iapp=0.1', hasAction: true, index: 1});
  assert.equal(out[3].comment, undefined);
});

test('attachComments never reads past the comments it was given', () => {
  const out = attachComments(['"a', '"b', '"c'], parseComments([['a', 0]]));
  assert.deepEqual(out[0].comment, {text: 'a', hasAction: false, index: 0});
  assert.equal(out[1].comment, undefined);
  assert.equal(out[2].comment, undefined);
});

test('the equations event replaces the stored lines', () => {
  const ev: EquationsEvent = {ev: 'equations', lines: ["V'=I-W", "W'=phi*(Winf-W)"]};
  const s = reduceText(initialText, {type: 'equations', ev});
  assert.deepEqual(s.equations, ["V'=I-W", "W'=phi*(Winf-W)"]);
});

test('the source event decodes lines and comments together', () => {
  const ev: SourceEvent = {
    ev: 'source',
    lines: ['v\'=-v', '" * set iapp=0.1'],
    comments: [['* set iapp=0.1', 1]],
  };
  const s = reduceText(initialText, {type: 'source', ev});
  assert.equal(s.source!.lines.length, 2);
  assert.equal(s.source!.lines[1].comment?.hasAction, true);
  assert.equal(s.source!.comments.length, 1);
});

test('the equilibrium event replaces the last result', () => {
  const ev: EquilibriumEvent = {
    ev: 'equilibrium', type: 'STABLE', cplus: 0, cminus: 2, rplus: 0, rminus: 0, im: 0,
    values: [['v', -0.144], ['w', 0.03]],
  };
  const s = reduceText(initialText, {type: 'equilibrium', ev});
  assert.deepEqual(s.equilibrium, {type: 'STABLE', cplus: 0, cminus: 2, rplus: 0, rminus: 0, im: 0, values: [['v', -0.144], ['w', 0.03]]});
});

test('open and tab are tracked, with no change returning the same object', () => {
  let s = reduceText(initialText, {type: 'open', open: true});
  assert.equal(s.open, true);
  assert.equal(reduceText(s, {type: 'open', open: true}), s);
  s = reduceText(s, {type: 'tab', tab: 'source'});
  assert.equal(s.tab, 'source');
  assert.equal(reduceText(s, {type: 'tab', tab: 'source'}), s);
});
