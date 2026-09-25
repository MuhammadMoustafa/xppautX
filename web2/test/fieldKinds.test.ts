/* What each kind of box takes (store/fieldKinds.ts, T31). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {
  EXPRESSION, FILE, FORMULA, fieldError, fieldInputMode, fieldMessage, fieldsValid, NUMBER, specOfKind, TEXT,
  type FieldSpec,
} from '../src/store/fieldKinds';

const ok = (spec: FieldSpec, ...texts: string[]) => {
  for (const t of texts) assert.equal(fieldError(spec, t), null, `${spec.kind} takes ${JSON.stringify(t)}`);
};
const refused = (spec: FieldSpec, ...texts: string[]) => {
  for (const t of texts) assert.notEqual(fieldError(spec, t), null, `${spec.kind} refuses ${JSON.stringify(t)}`);
};

test('an integer: digits only, with a sign; not 2.5, letters, an exponent or nothing', () => {
  const int: FieldSpec = {kind: 'integer'};
  ok(int, '0', '42', '-7', '+3', ' 12 ');
  refused(int, '2.5', 'abc', '12a', '1e2', '', ' ', '0x10', '--1');
  assert.equal(fieldError(int, '2.5'), 'a whole number');
});

test('an integer within its range says the range', () => {
  assert.equal(fieldError({kind: 'integer', min: 2, max: 7}, '8'), 'a whole number from 2 to 7');
  assert.equal(fieldError({kind: 'integer', min: 1}, '0'), 'a whole number of at least 1');
  assert.equal(fieldError({kind: 'integer', max: 5}, '6'), 'a whole number of at most 5');
  ok({kind: 'integer', min: 2, max: 7}, '2', '7');
});

test('a number: 1e-3, -0.5, .5, 3.; not letters, a %formula, Infinity or nothing', () => {
  ok(NUMBER, '1e-3', '-0.5', '.5', '3.', '+2E+10', '0', ' 7 ');
  refused(NUMBER, 'abc', '1..2', '%2*pi', 'Infinity', 'NaN', '', '1e', '0x1f', '1,5');
  assert.equal(fieldError(NUMBER, 'x'), 'a number');
});

test('a number or %formula: the %formula too, with its brackets matched', () => {
  ok(FORMULA, '1e-3', '-0.5', '%2*pi', '%(a+b)/2', '% 3');
  refused(FORMULA, 'abc', '%', '%(2', 'pi', '');
  assert.equal(fieldError(FORMULA, 'letters'), 'a number, or %formula such as %2*pi');
});

test('a number above 0, or other than 0', () => {
  assert.equal(fieldError({kind: 'number', positive: true}, '0'), 'a number above 0');
  assert.equal(fieldError({kind: 'number', positive: true}, '-1'), 'a number above 0');
  ok({kind: 'number', positive: true}, '0.01');
  assert.equal(fieldError({kind: 'number', nonzero: true}, '0'), 'a number other than 0');
  ok({kind: 'number', nonzero: true}, '-0.01');
});

test('an expression: anything with its brackets matched, not nothing', () => {
  ok(EXPRESSION, 'x-1', 'sin(t)*a[2]', 'y(1)-y0');
  refused(EXPRESSION, '', '  ', 'sin(t', 'a)(', '[x)');
});

test('a name: one of the set, case not mattering, not another', () => {
  const name: FieldSpec = {kind: 'name', names: ['V', 'w', 'iapp'], what: 'a variable of the model'};
  ok(name, 'V', 'v', 'IAPP', ' w ');
  refused(name, 'x', '', 'V2', 'iap');
  assert.equal(fieldError(name, 'x'), 'a variable of the model, such as V, w, iapp');
});

test('a file: a base name only', () => {
  ok(FILE, 'out.dat', 'lecar.ode', 'a b.txt');
  refused(FILE, '', '../x', 'a/b', 'a\\b', '.hidden', 'con', 'x?');
});

test('text: anything', () => {
  ok(TEXT, '', 'abc', '1..2', '%(');
});

test('the input mode: digits for integers, decimals for numbers, text for the rest', () => {
  assert.equal(fieldInputMode({kind: 'integer'}), 'numeric');
  assert.equal(fieldInputMode(NUMBER), 'decimal');
  assert.equal(fieldInputMode(FORMULA), 'decimal');
  assert.equal(fieldInputMode(EXPRESSION), 'text');
});

test('the protocol kinds as specs; an unknown or missing kind is text', () => {
  const lists = [['T', 'V', 'w'], ['V', 'w'], ['iapp', 'phi']];
  assert.deepEqual(specOfKind('integer'), {kind: 'integer'});
  assert.deepEqual(specOfKind('number'), {kind: 'number'});
  assert.deepEqual(specOfKind('formula'), {kind: 'number', formula: true});
  assert.deepEqual(specOfKind('expression'), {kind: 'expression'});
  assert.deepEqual(specOfKind('file'), {kind: 'file'});
  assert.deepEqual(specOfKind('text'), {kind: 'text'});
  assert.deepEqual(specOfKind(undefined), {kind: 'text'});
  assert.deepEqual(specOfKind('colour'), {kind: 'text'});
  const par = specOfKind('name:2', lists);
  assert.equal(par.kind, 'name');
  ok(par, 'iapp', 'PHI');
  refused(par, 'V');
  assert.deepEqual(specOfKind('name:9', lists), {kind: 'text'}, 'a list the page does not have: text');
  assert.deepEqual(specOfKind('name:0', null), {kind: 'text'});
});

test('a form is valid when every field is', () => {
  assert.equal(fieldsValid([{kind: 'integer'}, NUMBER, TEXT], ['3', '0.5', '']), true);
  assert.equal(fieldsValid([{kind: 'integer'}, NUMBER], ['3', 'x']), false);
});

test('a message starts with a capital', () => {
  assert.equal(fieldMessage('a whole number'), 'A whole number');
});
