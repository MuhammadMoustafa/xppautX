/* The values slice: pending/error attribution and the undo history
   (docs/ui-v2.md T3, store/values.ts), without a browser (npm test). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {fieldKey, initialValues, reduceValues, sixSig, type ValueEdit} from '../src/store/values';

const edit = (e: Partial<ValueEdit> & Pick<ValueEdit, 'kind' | 'previous'>): ValueEdit =>
  ({name: undefined, index: undefined, ...e});

test('fieldKey folds case for names, not for indices', () => {
  assert.equal(fieldKey('par', 'Iapp'), fieldKey('par', 'iapp'));
  assert.notEqual(fieldKey('bc', 0), fieldKey('bc', 1));
  assert.notEqual(fieldKey('par', 'v'), fieldKey('ic', 'v'), 'kind is part of the identity');
});

test('an edit is remembered for undo and marks the field pending', () => {
  const e = edit({kind: 'par', name: 'Iapp', previous: '0.05'});
  const s = reduceValues(initialValues, {type: 'edit', edit: e});
  assert.deepEqual(s.history, [e]);
  assert.equal(s.pending, 'par:iapp');
  assert.deepEqual(s.errors, {});
});

test('a message:error while a field is pending becomes that field\'s error', () => {
  let s = reduceValues(initialValues, {type: 'edit', edit: edit({kind: 'par', name: 'iapp', previous: '0.05'})});
  s = reduceValues(s, {type: 'error', text: 'bad formula'});
  assert.equal(s.errors['par:iapp'], 'bad formula');
  /* idle (settled) stops attributing further errors to it */
  s = reduceValues(s, {type: 'settled'});
  assert.equal(s.pending, null);
  const s2 = reduceValues(s, {type: 'error', text: 'unrelated later error'});
  assert.equal(s2, s, 'no field is pending: the error is not attributed anywhere');
});

test('a new edit on a field clears its stale error', () => {
  let s = reduceValues(initialValues, {type: 'edit', edit: edit({kind: 'par', name: 'iapp', previous: '0.05'})});
  s = reduceValues(s, {type: 'error', text: 'bad formula'});
  assert.equal(s.errors['par:iapp'], 'bad formula');
  s = reduceValues(s, {type: 'edit', edit: edit({kind: 'par', name: 'iapp', previous: '0.05'})});
  assert.equal(s.errors['par:iapp'], undefined);
});

test('undo pops the last edit, re-pends its field, and clears its error', () => {
  let s = reduceValues(initialValues, {type: 'edit', edit: edit({kind: 'par', name: 'iapp', previous: '0.05'})});
  s = reduceValues(s, {type: 'edit', edit: edit({kind: 'ic', name: 'v', previous: '-0.2'})});
  assert.equal(s.history.length, 2);
  s = reduceValues(s, {type: 'error', text: 'bad formula'}); /* attributed to the last edit, ic:v */
  s = reduceValues(s, {type: 'undo'});
  assert.deepEqual(s.history, [edit({kind: 'par', name: 'iapp', previous: '0.05'})]);
  assert.equal(s.pending, 'ic:v');
  assert.equal(s.errors['ic:v'], undefined);
});

test('undoing bc/delay edits goes by index, not name', () => {
  let s = reduceValues(initialValues, {type: 'edit', edit: edit({kind: 'bc', index: 0, previous: 'u(0)-u(1)'})});
  const field = fieldKey('bc', 0);
  assert.equal(s.pending, field);
  s = reduceValues(s, {type: 'undo'});
  assert.equal(s.history.length, 0);
  assert.equal(s.pending, field);
});

test('undo on an empty history does nothing', () => {
  assert.equal(reduceValues(initialValues, {type: 'undo'}), initialValues);
});

test('history keeps only the most recent 50 edits', () => {
  let s = initialValues;
  for (let i = 0; i < 55; i++) s = reduceValues(s, {type: 'edit', edit: edit({kind: 'par', name: 'p', previous: String(i)})});
  assert.equal(s.history.length, 50);
  assert.equal(s.history[0].previous, '5');
  assert.equal(s.history[49].previous, '54');
});

test('sixSig shows six significant digits', () => {
  assert.equal(sixSig(0.144), '0.144');
  assert.equal(sixSig(1 / 3), '0.333333');
  assert.equal(sixSig(123456789), '123457000');
  assert.equal(sixSig(NaN), 'NaN');
});
