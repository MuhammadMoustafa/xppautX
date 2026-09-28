/* The values slice: pending edits and error attribution, no undo (GitHub
   #110, #117; store/values.ts), without a browser (npm test). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {fieldKey, initialValues, reduceValues, sixSig, type ValueSet} from '../src/store/values';

const edit = (e: Partial<ValueSet> & Pick<ValueSet, 'kind' | 'text'>): ValueSet =>
  ({name: undefined, index: undefined, ...e});

test('fieldKey folds case for names, not for indices', () => {
  assert.equal(fieldKey('par', 'Iapp'), fieldKey('par', 'iapp'));
  assert.notEqual(fieldKey('bc', 0), fieldKey('bc', 1));
  assert.notEqual(fieldKey('par', 'v'), fieldKey('ic', 'v'), 'kind is part of the identity');
});

test('an edit is kept pending and clears its own stale error', () => {
  const e = edit({kind: 'par', name: 'Iapp', text: '0.2'});
  const s = reduceValues(initialValues, {type: 'edit', set: e});
  assert.deepEqual(s.pending, [e]);
  assert.deepEqual(s.errors, {});
});

test('a message:error while a field is attributed becomes that field\'s error', () => {
  let s = reduceValues(initialValues, {type: 'flushed', field: 'par:iapp'});
  s = reduceValues(s, {type: 'error', text: 'bad formula'});
  assert.equal(s.errors['par:iapp'], 'bad formula');
  /* idle (settled) stops attributing further errors to it */
  s = reduceValues(s, {type: 'settled'});
  assert.equal(s.attributing, null);
  const s2 = reduceValues(s, {type: 'error', text: 'unrelated later error'});
  assert.equal(s2, s, 'no field is attributed: the error is not attributed anywhere');
});

test('a new edit on a field clears its stale error', () => {
  let s = reduceValues(initialValues, {type: 'flushed', field: 'par:iapp'});
  s = reduceValues(s, {type: 'error', text: 'bad formula'});
  assert.equal(s.errors['par:iapp'], 'bad formula');
  s = reduceValues(s, {type: 'edit', set: edit({kind: 'par', name: 'iapp', text: '0.06'})});
  assert.equal(s.errors['par:iapp'], undefined);
});

test('clearError (Escape dropping a core refusal, WF-001) forgets one field\'s error and nothing else', () => {
  let s = reduceValues(initialValues, {type: 'flushed', field: 'par:iapp'});
  s = reduceValues(s, {type: 'error', text: 'Bad formula'});
  s = reduceValues(s, {type: 'flushed', field: 'ic:v'});
  s = reduceValues(s, {type: 'error', text: 'Bad formula'});
  assert.deepEqual(s.errors, {'par:iapp': 'Bad formula', 'ic:v': 'Bad formula'});
  s = reduceValues(s, {type: 'clearError', field: 'par:iapp'});
  assert.deepEqual(s.errors, {'ic:v': 'Bad formula'});
  const s2 = reduceValues(s, {type: 'clearError', field: 'par:iapp'});
  assert.equal(s2, s, 'nothing to clear: the same state');
});

test('flushed with more than one field attributes to none', () => {
  let s = reduceValues(initialValues, {type: 'edit', set: edit({kind: 'par', name: 'iapp', text: '0.2'})});
  s = reduceValues(s, {type: 'edit', set: edit({kind: 'ic', name: 'v', text: '-0.2'})});
  s = reduceValues(s, {type: 'flushed', field: null});
  assert.deepEqual(s.pending, []);
  assert.equal(s.attributing, null);
});

test('undoing bc/delay edits goes by index, not name (only the pending edit, no history)', () => {
  const s = reduceValues(initialValues, {type: 'edit', set: edit({kind: 'bc', index: 0, text: 'u(0)-u(1)'})});
  assert.deepEqual(s.pending, [edit({kind: 'bc', index: 0, text: 'u(0)-u(1)'})]);
});

test('sixSig shows six significant digits', () => {
  assert.equal(sixSig(0.144), '0.144');
  assert.equal(sixSig(1 / 3), '0.333333');
  assert.equal(sixSig(123456789), '123457000');
  assert.equal(sixSig(NaN), 'NaN');
});
