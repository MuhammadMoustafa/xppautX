/* The settings panels' fields come from the option table's rows (store/fieldKinds.ts specOfOption, ui/OptionsPanel.tsx sectionsOf, W616). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import type {OptionGroup, OptionRow} from '../src/protocol/types';
import {fieldError, specOfOption} from '../src/store/fieldKinds';
import {sectionsOf} from '../src/ui/OptionsPanel';

const row = (key: string, extra: Partial<OptionRow> = {}): OptionRow =>
  ({key, section: key.includes('.') ? key.slice(0, key.lastIndexOf('.')) : '', section_label: '', label: key, kind: 'number', help: '', default: '0', ...extra});

test('a number takes the limits of its row, a whole number its range', () => {
  const positive = specOfOption(row('step.min', {exclusiveMinimum: 0}), []);
  assert.notEqual(fieldError(positive, '0'), null);
  assert.equal(fieldError(positive, '1e-9'), null);
  const nonzero = specOfOption(row('dt', {nonzero: true}), []);
  assert.notEqual(fieldError(nonzero, '0'), null);
  const whole = specOfOption(row('mesh', {kind: 'whole', minimum: 1, maximum: 4096}), []);
  assert.equal(fieldError(whole, '4096'), null);
  assert.notEqual(fieldError(whole, '4097'), null);
  assert.notEqual(fieldError(whole, '2.5'), null);
});

test('a choice takes its named choices, a yes/no is a boolean, a name one of the model\'s', () => {
  const choice = specOfOption(row('method', {kind: 'choice', choices: ['euler', 'runge_kutta']}), []);
  assert.equal(fieldError(choice, 'euler'), null);
  assert.notEqual(fieldError(choice, 'rk'), null);
  assert.equal(specOfOption(row('stop', {kind: 'choice', boolean: true}), []).kind, 'boolean');
  const name = specOfOption(row('variable', {kind: 'name'}), ['V', 'W']);
  assert.equal(fieldError(name, 'v'), null);
  assert.notEqual(fieldError(name, 'U'), null);
});

test('a panel keeps the table\'s order: the rows with no section first, then each section as the table has it', () => {
  const group: OptionGroup = {group: 'integration', label: 'Integration', rows: [
    row('total_time'), row('dt'), row('step.min'), row('step.max'), row('jacobian.kind', {kind: 'choice', choices: []}), row('seed', {kind: 'whole'})]};
  assert.deepEqual(sectionsOf(group).map(s => [s.section, s.rows.map(r => r.key)]),
    [['', ['total_time', 'dt']], ['step', ['step.min', 'step.max']], ['jacobian', ['jacobian.kind']], ['', ['seed']]]);
});
