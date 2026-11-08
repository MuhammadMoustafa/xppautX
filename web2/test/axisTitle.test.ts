/* The axis title's cut (plot/axisTitle.ts, W646), without a browser (npm test). The width is a fixed advance per
   character, so a title's width is its length times that advance. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {fitTitle} from '../src/plot/axisTitle';

const ADVANCE = 7;
const width = (text: string) => text.length * ADVANCE;

test('a title that fits its side is drawn whole', () => {
  assert.equal(fitTitle('membrane potential', 18 * ADVANCE, width), 'membrane potential');
  assert.equal(fitTitle('v', 1 * ADVANCE, width), 'v');
});

test('a title wider than its side ends in … and fits it', () => {
  const title = 'x'.repeat(200);
  const cut = fitTitle(title, 300, width);
  assert.ok(cut.endsWith('…'), cut);
  assert.ok(width(cut) <= 300, `${cut.length} characters`);
});

test('the cut keeps the longest start that fits, with … after it', () => {
  const cut = fitTitle('abcdefghijklmnopqrstuvwxyz', 10 * ADVANCE, width);
  assert.equal(cut, 'abcdefghi…');
  assert.equal(width(cut), 10 * ADVANCE);
});

test('a side too short for any text is the mark alone', () => {
  assert.equal(fitTitle('abc', 0, width), '…');
});
