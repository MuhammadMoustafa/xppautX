/* The array plot's time scroll maths (docs/ui-v2.md T12, plot/aplotScroll.ts),
   without a browser (npm test). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {accumulateScroll, dragScroll, keyScroll, wheelScroll} from '../src/plot/aplotScroll';

test('wheelScroll inverts deltaY: scrolling down moves forward in time', () => {
  assert.equal(wheelScroll(100), -100);
  assert.equal(wheelScroll(-40), 40);
  assert.equal(wheelScroll(0), 0);
});

test('dragScroll is the pointer\'s Y delta, matching the classic page\'s convention', () => {
  assert.equal(dragScroll(100, 140), 40);
  assert.equal(dragScroll(140, 100), -40);
  assert.equal(dragScroll(100, 100), 0);
});

test('keyScroll: ArrowUp/PageUp move to earlier rows (positive dy), Down/PageDown to later (negative)', () => {
  assert.equal(keyScroll('ArrowUp', 20), 1);
  assert.equal(keyScroll('ArrowDown', 20), -1);
  assert.ok(keyScroll('PageUp', 20)! > 0);
  assert.ok(keyScroll('PageDown', 20)! < 0);
});

test('keyScroll\'s Page step is a full grid height (ny), so a page always shows fresh rows', () => {
  assert.equal(keyScroll('PageUp', 40), 40);
  assert.equal(keyScroll('PageDown', 40), -40);
});

test('keyScroll never returns a zero-row page even for an empty or tiny grid', () => {
  assert.equal(keyScroll('PageUp', 0), 1);
  assert.equal(keyScroll('PageDown', 0), -1);
});

test('keyScroll returns null for a key the array plot does not use', () => {
  assert.equal(keyScroll('Enter', 20), null);
  assert.equal(keyScroll('a', 20), null);
});

test('accumulateScroll sends the total at once when the core is idle', () => {
  assert.deepEqual(accumulateScroll(0, 5, false), {send: 5, pending: 0});
  assert.deepEqual(accumulateScroll(3, 5, false), {send: 8, pending: 0});
});

test('accumulateScroll queues while the core is busy, sending nothing yet', () => {
  assert.deepEqual(accumulateScroll(0, 5, true), {send: 0, pending: 5});
  assert.deepEqual(accumulateScroll(5, 3, true), {send: 0, pending: 8});
});

test('several busy gestures collapse into the one scroll sent once idle', () => {
  let pending = 0;
  ({pending} = accumulateScroll(pending, 10, true));
  ({pending} = accumulateScroll(pending, -3, true));
  const {send, pending: left} = accumulateScroll(pending, 2, false);
  assert.equal(send, 9);
  assert.equal(left, 0);
});
