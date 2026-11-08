/* The values slice: edits sent (settings, W106) until their set's done,
   error attribution, no undo (GitHub #110, #155; store/values.ts), without a
   browser (npm test). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {fieldKey, initialValues, inputText, shownNumber, isFolded, showsBcSection, reduceValues, sentText, sixSig, type ValueSet, type ValuesState} from '../src/store/values';

const edit = (e: Partial<ValueSet> & Pick<ValueSet, 'kind' | 'text'>): ValueSet =>
  ({name: undefined, index: undefined, ...e});

test('fieldKey folds case for names, not for indices', () => {
  assert.equal(fieldKey('par', 'Iapp'), fieldKey('par', 'iapp'));
  assert.notEqual(fieldKey('bc', 0), fieldKey('bc', 1));
  assert.notEqual(fieldKey('par', 'v'), fieldKey('ic', 'v'), 'kind is part of the identity');
});

test('an edit sent shows as the field\'s value until its own done, and clears its stale error', () => {
  const e = edit({kind: 'par', name: 'Iapp', text: '0.2'});
  let s = reduceValues({...initialValues, errors: {'par:iapp': 'old'}}, {type: 'sent', set: e, ahead: 1});
  assert.deepEqual(s.inflight, [{set: e, ahead: 1}]);
  assert.deepEqual(s.errors, {});
  assert.equal(sentText(s.inflight, 'par:iapp'), '0.2');
  /* the running computation's done: the set's own command is next */
  s = reduceValues(s, {type: 'ended'});
  assert.equal(sentText(s.inflight, 'par:iapp'), '0.2', 'still shown: the core applies it now');
  s = reduceValues(s, {type: 'ended'});
  assert.equal(sentText(s.inflight, 'par:iapp'), null, 'its own done: the core\'s state has it');
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
  s = reduceValues(s, {type: 'ended'});
  s = reduceValues(s, {type: 'error', text: 'Bad formula'});
  s = reduceValues(s, {type: 'ended'});
  s = reduceValues(s, {type: 'error', text: 'Numerics: Dt must be a number other than 0'});
  s = reduceValues(s, {type: 'ended'});
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
  assert.equal(sixSig(123456789), '1.23457e+8');
  assert.equal(sixSig(NaN), 'NaN');
});

test('shownNumber: six digits, an exponent outside 1e-4..1e6, and a flag for dropped digits (W640)', () => {
  const at = (n: number) => { const r = shownNumber(n); return [r.head, r.tail, r.cut, r.full]; };
  assert.deepEqual(at(0.144), ['0.144', '', false, '0.144']);
  assert.deepEqual(at(-0.3825256290227896), ['-0.382526', '', true, '-0.3825256290227896']);
  assert.deepEqual(at(1e-4), ['0.0001', '', false, '0.0001'], '1e-4 is the smallest plain number');
  assert.deepEqual(at(9.99999e-5), ['9.99999', 'e-5', false, String(9.99999e-5)]);
  assert.deepEqual(at(999999), ['999999', '', false, '999999']);
  assert.deepEqual(at(1e6), ['1', 'e+6', false, '1000000'], '1e6 is the first written with an exponent');
  assert.deepEqual(at(123456789.25), ['1.23457', 'e+8', true, '123456789.25']);
  assert.deepEqual(at(-123456789.25), ['-1.23457', 'e+8', true, '-123456789.25']);
  assert.deepEqual(at(3.2e-9), ['3.2', 'e-9', false, '3.2e-9']);
  assert.deepEqual(at(0), ['0', '', false, '0']);
  assert.deepEqual(at(-0), ['0', '', false, '0']);
  assert.deepEqual(at(NaN), ['NaN', '', false, 'NaN']);
  assert.deepEqual(at(Infinity), ['Infinity', '', false, 'Infinity']);
  assert.deepEqual(at(-Infinity), ['-Infinity', '', false, '-Infinity']);
  assert.deepEqual(at(1 / 3), ['0.333333', '', true, String(1 / 3)]);
});

test('inspection retains ten significant digits and uses compact scientific notation at extremes', () => {
  assert.equal(inputText(-0.3825256290227896), '-0.382525629');
  assert.equal(inputText(1 / 3), '0.3333333333');
  assert.equal(inputText(1.234567891234e15), '1.234567891e+15');
  assert.equal(inputText(1e-12), '1e-12');
  assert.equal(inputText(0), '0');
  assert.equal(inputText(NaN), 'NaN');
});

test('the boundary-conditions section is absent without model BCs, present and folded with them (W99)', () => {
  assert.equal(showsBcSection(undefined), false);
  assert.equal(showsBcSection([]), false, 'amari.ode: the core sends none');
  assert.equal(showsBcSection([['0=', 'x-y']]), true);
  assert.equal(isFolded([], 'bc', true), true, 'starts folded');
  assert.equal(isFolded(['+bc'], 'bc', true), false, 'opened by the person');
  assert.equal(isFolded([], 'delay'), false, 'a section that starts open');
  assert.equal(isFolded(['delay'], 'delay'), true);
});
