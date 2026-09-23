/* Save and Load of parameters and initial conditions in XPP's own formats
   (store/valueFiles.ts, core/lunch-new.c io_parameter_file/io_ic_file). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {formatG16, formatIcFile, formatParFile, parseValuesFile} from '../src/store/valueFiles';

test('formatG16 prints what C %.16g prints', () => {
  const cases: [number, string][] = [[0, '0'], [0.2, '0.2'], [1, '1'], [-2.5, '-2.5'], [0.1 + 0.2, '0.3000000000000000'],
    [1e-5, '1e-05'], [0.0001, '0.0001'], [123456789, '123456789'], [1e16, '1e+16'], [1.5e300, '1.5e+300'],
    [-4.2e-7, '-4.2e-07'], [1 / 3, '0.3333333333333333'], [2 / 3, '0.6666666666666666'], [-0.05, '-0.05']];
  for (const [v, want] of cases) assert.equal(formatG16(v), want.replace(/(\.\d*?)0+$/, '$1').replace(/\.$/, ''), String(v));
});

test('a parameter file is XPP format: a count, "value  name" lines, the model and a date', () => {
  const text = formatParFile([['iapp', 0.2], ['phi', 0.04]], 'lecar.ode', 'Wed Sep 23 2026');
  assert.equal(text, '2   Number params\n0.2  iapp\n0.04  phi\n\n\nFile:lecar.ode\nWed Sep 23 2026\n');
  assert.deepEqual(parseValuesFile(text, ['iapp', 'phi']), {values: [['iapp', '0.2'], ['phi', '0.04']], error: null});
});

test('XPP reads a parameter file by position, and so does Load', () => {
  const text = '2   Number params\n0.5  other\n-1e-05  names\n\n\nFile:x.ode\nsome date\n';
  assert.deepEqual(parseValuesFile(text, ['iapp', 'phi']).values, [['iapp', '0.5'], ['phi', '-1e-05']]);
  assert.match(parseValuesFile(text, ['a', 'b', 'c']).error!, /2 parameters, the model 3/);
  assert.match(parseValuesFile('2   Number params\n0.5\nx\n', ['a', 'b']).error!, /not a number/);
});

test('an IC file is the values alone, one per line, read by position', () => {
  const text = formatIcFile([['v', -0.2], ['w', 0.01]]);
  assert.equal(text, '-0.2\n0.01\n');
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
