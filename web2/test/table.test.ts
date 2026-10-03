/* The data table slice: paging (planRequest) and the reducer (docs/ui-v2.md
   T10, store/table.ts), without a browser (npm test). CSV export is the
   core's own write (docs/roadmap.md W66: session.writeDataFile), so it is
   not a table.ts concern any more; tools/web2check.mjs's dataTable covers
   it end to end. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import type {BrowserEvent} from '../src/protocol/types';
import {initialTable, planRequest, reduceTable, rowAt} from '../src/store/table';
import {HELLO} from './hello';

const page = (over: Partial<BrowserEvent> = {}): BrowserEvent => ({
  ev: 'browser', rows: 601, cols: ['T', 'V', 'W'], row0: 0, start: 0, end: 601, from: 0, col: 1,
  data: [[0, -0.144, 0.03], [0.05, -0.1438, 0.0301]],
  ...over,
});

test('planRequest asks for nothing already covered by the page', () => {
  const p = page({from: 0, data: [[0, -0.144, 0.03], [0.05, -0.1438, 0.0301], [0.1, -0.14, 0.031]]});
  assert.equal(planRequest(p, 0, 2, HELLO.limits), null);
  assert.equal(planRequest(p, 1, 2, HELLO.limits), null);
});

test('planRequest asks for a buffered block around what is missing', () => {
  const req = planRequest(null, 500, 20, HELLO.limits);
  assert.deepEqual(req, {from: 480, count: 60, col: 1, ncol: 500});
});

test('W170: a truncated final block covers the viewport past EOF without requesting forever', () => {
  const p = page({from: 595, data: Array.from({length: 6}, () => [0, 0, 0])});
  assert.equal(planRequest(p, 598, 20, HELLO.limits), null);
  assert.equal(planRequest(p, 601, 20, HELLO.limits), null);
  assert.equal(planRequest(p, 0, 20, HELLO.limits)?.from, 0, 'missing real rows still need a request');
  assert.ok(planRequest(p, 598, 20, HELLO.limits, 620), 'a live run growing past the cached end still needs a request');
});

test('planRequest asks again when the page is for other columns (col !== 1)', () => {
  const p = page({col: 2, data: [[0, 0.03], [0.05, 0.0301]]});
  const req = planRequest(p, 0, 2, HELLO.limits);
  assert.ok(req && req.col === 1);
});

test('planRequest clamps from at 0 and count at the core\'s cap', () => {
  const req = planRequest(null, 5, 3000, HELLO.limits);
  assert.equal(req!.from, 0);
  assert.equal(req!.count, 2000);
});

test('the request never grows below 0 rows even at the very start', () => {
  const req = planRequest(null, 0, 1, HELLO.limits);
  assert.equal(req!.from, 0);
  assert.ok(req!.count >= 1);
});

test('rowAt reads a row out of the cached page, or null off it', () => {
  const p = page({from: 10, data: [[1, 2, 3], [4, 5, 6]]});
  assert.deepEqual(rowAt(p, 10), [1, 2, 3]);
  assert.deepEqual(rowAt(p, 11), [4, 5, 6]);
  assert.equal(rowAt(p, 9), null);
  assert.equal(rowAt(p, 12), null);
  assert.equal(rowAt(null, 0), null);
});

test('a browser event replaces the page and clears the pending request', () => {
  let s = reduceTable(initialTable, {type: 'requested', req: {from: 0, count: 10, col: 1, ncol: 500}});
  assert.ok(s.pendingKey);
  s = reduceTable(s, {type: 'event', ev: page()});
  assert.equal(s.pendingKey, null);
  assert.equal(s.page!.rows, 601);
});

test('the selection follows the core when it moves row0 (Find, Get, ...)', () => {
  let s = reduceTable(initialTable, {type: 'event', ev: page({row0: 0})});
  assert.equal(s.selected, 0, 'the first page: the selection starts at the core\'s row0');
  s = reduceTable(s, {type: 'select', row: 5});
  assert.equal(s.selected, 5, 'a client-side move (arrow keys, a click) does not touch row0');
  s = reduceTable(s, {type: 'event', ev: page({row0: 0})});
  assert.equal(s.selected, 5, 'the core resending the same row0 leaves the selection alone');
  s = reduceTable(s, {type: 'event', ev: page({row0: 42})});
  assert.equal(s.selected, 42, 'Find moved row0: the view follows it');
});

test('select clamps to the known row count', () => {
  const s = reduceTable(initialTable, {type: 'event', ev: page({rows: 10})});
  assert.equal(reduceTable(s, {type: 'select', row: 999}).selected, 9);
  assert.equal(reduceTable(s, {type: 'select', row: -5}).selected, 0);
});

test('open/close is tracked', () => {
  const s = reduceTable(initialTable, {type: 'open', open: true});
  assert.equal(s.open, true);
  assert.equal(reduceTable(s, {type: 'open', open: true}), s, 'no change: the same state object');
  assert.equal(reduceTable(s, {type: 'open', open: false}).open, false);
});
