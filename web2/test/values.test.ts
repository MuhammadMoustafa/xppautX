/* The values slice: edits sent (settings, W106) until their set's idle,
   error attribution, no undo (GitHub #110, #155; store/values.ts), without a
   browser (npm test). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {fieldKey, initialValues, reduceValues, sentText, sixSig, type ValueSet, type ValuesState} from '../src/store/values';

const edit = (e: Partial<ValueSet> & Pick<ValueSet, 'kind' | 'text'>): ValueSet =>
  ({name: undefined, index: undefined, ...e});

test('fieldKey folds case for names, not for indices', () => {
  assert.equal(fieldKey('par', 'Iapp'), fieldKey('par', 'iapp'));
  assert.notEqual(fieldKey('bc', 0), fieldKey('bc', 1));
  assert.notEqual(fieldKey('par', 'v'), fieldKey('ic', 'v'), 'kind is part of the identity');
});

test('an edit sent shows as the field\'s value until its own idle, and clears its stale error', () => {
  const e = edit({kind: 'par', name: 'Iapp', text: '0.2'});
  let s = reduceValues({...initialValues, errors: {'par:iapp': 'old'}}, {type: 'sent', set: e, ahead: 1});
  assert.deepEqual(s.inflight, [{set: e, ahead: 1}]);
  assert.deepEqual(s.errors, {});
  assert.equal(sentText(s.inflight, 'par:iapp'), '0.2');
  /* the running computation's idle: the set's own command is next */
  s = reduceValues(s, {type: 'idle'});
  assert.equal(sentText(s.inflight, 'par:iapp'), '0.2', 'still shown: the core applies it now');
  s = reduceValues(s, {type: 'idle'});
  assert.equal(sentText(s.inflight, 'par:iapp'), null, 'its own idle: the core\'s state has it');
});

test('the latest edit sent of a field is what it shows', () => {
  let s = reduceValues(initialValues, {type: 'sent', set: edit({kind: 'par', name: 'iapp', text: '0.1'}), ahead: 1});
  s = reduceValues(s, {type: 'sent', set: edit({kind: 'par', name: 'IAPP', text: '0.3'}), ahead: 2});
  s = reduceValues(s, {type: 'sent', set: edit({kind: 'bc', index: 0, text: 'v-1'}), ahead: 3});
  assert.equal(sentText(s.inflight, 'par:iapp'), '0.3');
  assert.equal(sentText(s.inflight, 'bc:0'), 'v-1');
  assert.equal(sentText(s.inflight, 'par:phi'), null);
});

test('a message:error lands on the field whose set is the command running, no other', () => {
  let s = reduceValues(initialValues, {type: 'sent', set: edit({kind: 'par', name: 'iapp', text: '%x'}), ahead: 1});
  s = reduceValues(s, {type: 'sent', set: edit({kind: 'num', name: 'dt', text: '0'}), ahead: 2});
  const unrelated = reduceValues(s, {type: 'error', text: 'the computation\'s own error'});
  assert.deepEqual(unrelated.errors, {}, 'during the computation: no set runs');
  s = reduceValues(s, {type: 'idle'});
  s = reduceValues(s, {type: 'error', text: 'Bad formula'});
  s = reduceValues(s, {type: 'idle'});
  s = reduceValues(s, {type: 'error', text: 'Numerics: Dt must be a number other than 0'});
  s = reduceValues(s, {type: 'idle'});
  assert.deepEqual(s.errors, {'par:iapp': 'Bad formula', 'num:dt': 'Numerics: Dt must be a number other than 0'});
  assert.deepEqual(s.inflight, []);
  assert.equal(reduceValues(s, {type: 'error', text: 'later'}), s, 'nothing runs: attributed nowhere');
});

test('clearError (Escape dropping a core refusal, WF-001) forgets one field\'s error and nothing else', () => {
  let s: ValuesState = {...initialValues, errors: {'par:iapp': 'Bad formula', 'ic:v': 'Bad formula'}};
  s = reduceValues(s, {type: 'clearError', field: 'par:iapp'});
  assert.deepEqual(s.errors, {'ic:v': 'Bad formula'});
  const s2 = reduceValues(s, {type: 'clearError', field: 'par:iapp'});
  assert.equal(s2, s, 'nothing to clear: the same state');
});

test('a new connection waits for nothing sent before', () => {
  let s = reduceValues(initialValues, {type: 'sent', set: edit({kind: 'bc', index: 0, text: 'u(0)-u(1)'}), ahead: 0});
  s = reduceValues(s, {type: 'settled'});
  assert.deepEqual(s.inflight, []);
});

test('sixSig shows six significant digits', () => {
  assert.equal(sixSig(0.144), '0.144');
  assert.equal(sixSig(1 / 3), '0.333333');
  assert.equal(sixSig(123456789), '123457000');
  assert.equal(sixSig(NaN), 'NaN');
});
