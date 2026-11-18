/* The options panels' layout rules (W811, store/optionSections.ts): a low and a high row of one range are one field, and a section starts
   folded when all its rows are advanced or none of them applies to the method. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import type {OptionRow} from '../src/protocol/types';
import {panelItems, sectionFoldKey, sectionStartsFolded} from '../src/store/optionSections';

const row = (key: string, extra: Partial<OptionRow> = {}): OptionRow =>
  ({key, section: key.includes('.') ? key.slice(0, key.lastIndexOf('.')) : '', section_label: key.includes('.') ? key.slice(0, key.indexOf('.')).replace(/^./, c => c.toUpperCase()) : '',
    label: key, kind: 'number', help: '', default: '0', ...extra});

test('a low and a high row of one range are one pair, low first, named by the shared part', () => {
  const items = panelItems([row('limits.parameter_max'), row('limits.parameter_min'), row('limits.norm_min'), row('limits.norm_max')]);
  assert.deepEqual(items.map(i => i.kind), ['pair', 'pair']);
  const [parameter, norm] = items as {label: string; low: OptionRow; high: OptionRow; lowWord: string; highWord: string}[];
  assert.equal(parameter.label, 'Parameter');
  assert.equal(parameter.low.key, 'limits.parameter_min');
  assert.equal(parameter.high.key, 'limits.parameter_max');
  assert.equal(parameter.lowWord, 'min');
  assert.equal(parameter.highWord, 'max');
  assert.equal(norm.label, 'Norm');
});

test('the range words and stems: step.min and step.max name the section, low/high pairs too', () => {
  const step = panelItems([row('step.min'), row('step.max')]);
  assert.equal(step.length, 1);
  assert.equal(step[0].kind === 'pair' && step[0].label, 'Step');
  const range = panelItems([row('boundary_value.range_low'), row('boundary_value.range_high')]);
  assert.equal(range[0].kind === 'pair' && range[0].label, 'Range');
  assert.equal(range[0].kind === 'pair' && range[0].highWord, 'high');
});

test('a range word with no partner, or a word that is not a range word, stays one row', () => {
  const items = panelItems([row('limits.norm_min'), row('max_iterations'), row('min_step_size'), row('max_step_size'), row('xmin'), row('step.max', {section: 'step'})]);
  assert.deepEqual(items.map(i => i.kind), ['row', 'row', 'row', 'row', 'row', 'row']);
});

test('a pair takes the place of its first row, the rows around it keep the table\'s order', () => {
  const items = panelItems([row('dt'), row('step.max'), row('step.relative_tolerance'), row('step.min'), row('store_every')]);
  assert.deepEqual(items.map(i => i.kind === 'pair' ? 'pair:' + i.low.key : i.row.key),
    ['dt', 'pair:step.min', 'step.relative_tolerance', 'store_every']);
});

test('a section starts folded when all its rows are advanced or none applies, open when mixed', () => {
  const advanced = [row('step.a', {advanced: true}), row('step.b', {advanced: true})];
  const basic = [row('total', {advanced: false}), row('dt', {advanced: true})];
  const everything = () => true;
  assert.equal(sectionStartsFolded(advanced, everything), true);
  assert.equal(sectionStartsFolded(advanced.map(r => ({...r, advanced: false})), () => false), true);
  assert.equal(sectionStartsFolded(basic, everything), false);
  assert.equal(sectionStartsFolded(basic, r => r.key === 'dt'), false);
  assert.equal(sectionStartsFolded(basic, () => false), true);
  assert.equal(sectionStartsFolded([], everything), false);
});

test('a fold is kept under the group and the section', () => {
  assert.equal(sectionFoldKey('integration', 'step'), 'integration/step');
  assert.notEqual(sectionFoldKey('integration', 'step'), sectionFoldKey('continuation', 'step'));
});
