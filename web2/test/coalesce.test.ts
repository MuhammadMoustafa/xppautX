/* Edits not yet sent (store/values.ts, GitHub #18, #117): only the latest
   per field waits, and they go out as one `set` right before the next
   command that computes; the model's defaults by field. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {initialValues, isPending, queueSet, reduceValues, setCommand} from '../src/store/values';

test('only the latest value per field waits, in the order last changed', () => {
  let q = queueSet([], {kind: 'par', name: 'iapp', text: '0.1'});
  q = queueSet(q, {kind: 'ic', name: 'v', text: '-0.2'});
  q = queueSet(q, {kind: 'par', name: 'IAPP', text: '0.3'});
  q = queueSet(q, {kind: 'bc', index: 0, text: 'v-1'});
  assert.deepEqual(q, [{kind: 'ic', name: 'v', text: '-0.2'}, {kind: 'par', name: 'IAPP', text: '0.3'},
    {kind: 'bc', index: 0, text: 'v-1'}]);
  assert.ok(isPending(q, 'par:iapp'));
  assert.ok(!isPending(q, 'par:phi'));
});

test('the pending edits go out as one set: one value plain, several in values[]', () => {
  assert.equal(setCommand([]), null);
  assert.deepEqual(setCommand([{kind: 'par', name: 'iapp', text: '0.1'}]),
    {cmd: 'set', kind: 'par', name: 'iapp', text: '0.1'});
  assert.deepEqual(setCommand([{kind: 'par', name: 'iapp', text: '0.1'}, {kind: 'bc', index: 1, text: 'w'}]), {
    cmd: 'set', values: [{kind: 'par', name: 'iapp', text: '0.1'}, {kind: 'bc', index: 1, text: 'w'}],
  });
});

test('pending and flushed: a burst of five edits leaves one entry', () => {
  let s = initialValues;
  for (const v of ['0.1', '0.2', '0.3', '0.4', '0.5'])
    s = reduceValues(s, {type: 'edit', set: {kind: 'par', name: 'iapp', text: v}});
  assert.deepEqual(s.pending, [{kind: 'par', name: 'iapp', text: '0.5'}]);
  s = reduceValues(s, {type: 'flushed', field: 'par:iapp'});
  assert.deepEqual(s.pending, []);
  assert.equal(s.attributing, 'par:iapp');
  assert.equal(reduceValues(s, {type: 'flushed', field: 'par:iapp'}), s);
});

test('defaults by field key; ifUnset keeps the first ones', () => {
  let s = reduceValues(initialValues, {type: 'defaults', pars: [['Iapp', 0.2]], ics: [['V', -0.1]]});
  assert.deepEqual(s.defaults, {'par:iapp': 0.2, 'ic:v': -0.1});
  s = reduceValues(s, {type: 'defaults', pars: [['iapp', 9]], ics: [], ifUnset: true});
  assert.equal(s.defaults!['par:iapp'], 0.2);
});
