/* Load of parameters and initial conditions from XPP's own formats,
   written by the core (store/valueFiles.ts, core/lunch-new.cpp
   io_parameter_file/io_ic_file). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {parseValuesFile} from '../src/store/valueFiles';

test('a parameter file is XPP format: a count, "value  name" lines, the model and a date', () => {
  const text = '2   Number params\n0.2  iapp\n0.04  phi\n\n\nFile:lecar.ode\nWed Sep 23 2026\n';
  assert.deepEqual(parseValuesFile(text, ['iapp', 'phi']), {values: [['iapp', '0.2'], ['phi', '0.04']], error: null});
});

test('XPP reads a parameter file by position, and so does Load', () => {
  const text = '2   Number params\n0.5  other\n-1e-05  names\n\n\nFile:x.ode\nsome date\n';
  assert.deepEqual(parseValuesFile(text, ['iapp', 'phi']).values, [['iapp', '0.5'], ['phi', '-1e-05']]);
  assert.match(parseValuesFile(text, ['a', 'b', 'c']).error!, /2 parameters, the model 3/);
  assert.match(parseValuesFile('2   Number params\n0.5\nx\n', ['a', 'b']).error!, /not a number/);
});

test('an IC file is the values alone, one per line, read by position', () => {
  const text = '-0.2\n0.01\n';
  assert.deepEqual(parseValuesFile(text, ['V', 'W']).values, [['V', '-0.2'], ['W', '0.01']]);
  assert.deepEqual(parseValuesFile('1 2\n', ['V', 'W']).values, [['V', '1'], ['W', '2']], 'whitespace separated, as fscanf');
  assert.match(parseValuesFile('1\n', ['V', 'W']).error!, /1 values, the model 2/);
});

test('name value lines, in any order, names folded; unknown names refused', () => {
  const r = parseValuesFile('# mine\nPHI = 0.1\niapp 0.3\n', ['iapp', 'phi']);
  assert.deepEqual(r, {values: [['phi', '0.1'], ['iapp', '0.3']], error: null});
  assert.match(parseValuesFile('nosuch 1\n', ['iapp']).error!, /Not in the model: nosuch/);
  assert.match(parseValuesFile('', ['iapp']).error!, /No values/);
});
