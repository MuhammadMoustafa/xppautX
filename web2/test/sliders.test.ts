/* The sliders' ranges and positions (store/sliders.ts, GitHub #18). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {
  defaultRange, defaultStep, filterCandidates, fromPosition, niceStep, presetSliders, RANGE_STEPS, sliderRange,
  sliderStep, toPosition, validateSliderFields,
} from '../src/store/sliders';
import {initialValues, reduceValues} from '../src/store/values';

test('the default range: [0, 2v] above zero, [2v, 0] below, [-1, 1] at zero', () => {
  assert.deepEqual(defaultRange(0.05), {lo: 0, hi: 0.1});
  assert.deepEqual(defaultRange(-3), {lo: -6, hi: 0});
  assert.deepEqual(defaultRange(0), {lo: -1, hi: 1});
  assert.deepEqual(defaultRange(NaN), {lo: -1, hi: 1});
});

test('a range needs two different numbers', () => {
  assert.deepEqual(sliderRange({lo: '0', hi: '2'}), {lo: 0, hi: 2});
  assert.equal(sliderRange({lo: '1', hi: '1'}), null);
  assert.equal(sliderRange({lo: '', hi: '1'}), null);
  assert.equal(sliderRange({lo: 'x', hi: '1'}), null);
  assert.deepEqual(sliderRange({lo: '2', hi: '-2'}), {lo: 2, hi: -2}, 'a reversed range works too');
});

test('positions map to values and back, clamped to the track', () => {
  const r = {lo: -1, hi: 3};
  assert.equal(toPosition(1, r), RANGE_STEPS / 2);
  assert.equal(fromPosition(RANGE_STEPS / 2, r), 1);
  assert.equal(toPosition(10, r), RANGE_STEPS);
  assert.equal(toPosition(-10, r), 0);
});

test('the model presets start the list, once; add, set and remove', () => {
  assert.deepEqual(presetSliders([{name: 'iapp', lo: 0, hi: 0.2}], 1),
    [{id: 1, name: 'iapp', lo: '0', hi: '0.2', step: '0.002'}]);
  let s = reduceValues(initialValues, {type: 'presetSliders', defs: [{name: 'iapp', lo: 0, hi: 0.2}]});
  s = reduceValues(s, {type: 'presetSliders', defs: [{name: 'phi', lo: 0, hi: 1}]});
  assert.deepEqual(s.sliders.map(d => d.name), ['iapp'], 'a reconnection keeps the list');
  s = reduceValues(s, {type: 'addSlider'});
  s = reduceValues(s, {type: 'addSlider'});
  assert.deepEqual(s.sliders.map(d => d.id), [1, 2, 3]);
  assert.deepEqual(s.sliders[1], {id: 2, name: '', lo: '', hi: '', step: ''});
  s = reduceValues(s, {type: 'setSlider', id: 2, patch: {name: 'phi', lo: '0', hi: '0.08', step: '0.001'}});
  assert.deepEqual(s.sliders[1], {id: 2, name: 'phi', lo: '0', hi: '0.08', step: '0.001'});
  s = reduceValues(s, {type: 'removeSlider', id: 1});
  assert.deepEqual(s.sliders.map(d => d.id), [2, 3]);
});

test('niceStep rounds up to 1, 2 or 5 times a power of ten', () => {
  assert.equal(niceStep(0.002), 0.002);
  assert.equal(niceStep(0.0021), 0.005);
  assert.equal(niceStep(3), 5);
  assert.equal(niceStep(0.6), 1);
  assert.equal(niceStep(120), 200);
  assert.equal(niceStep(0), 1);
  assert.equal(niceStep(NaN), 1);
  assert.equal(niceStep(-1), 1);
});

test('the default step is (max - min) / 100, rounded nicely', () => {
  assert.equal(defaultStep(0, 0.2), 0.002);
  assert.equal(defaultStep(-1, 1), 0.02);
  assert.equal(defaultStep(5, 5), 1, 'an empty span falls back to 1');
});

test('sliderStep parses a positive, finite step', () => {
  assert.equal(sliderStep({step: '0.5'}), 0.5);
  assert.equal(sliderStep({step: ''}), null);
  assert.equal(sliderStep({step: '0'}), null);
  assert.equal(sliderStep({step: '-1'}), null);
  assert.equal(sliderStep({step: 'x'}), null);
});

test('validateSliderFields: min < max, step > 0 and no more than the span', () => {
  assert.deepEqual(validateSliderFields('0', '1', '0.1'), {});
  assert.deepEqual(validateSliderFields('1', '1', '0.1'), {lo: 'Min must be less than Max', hi: 'Min must be less than Max'});
  assert.deepEqual(validateSliderFields('2', '1', '0.1'), {lo: 'Min must be less than Max', hi: 'Min must be less than Max'});
  assert.deepEqual(validateSliderFields('', '1', '0.1'), {lo: 'A number'});
  assert.deepEqual(validateSliderFields('0', '1', '0'), {step: 'Step must be greater than 0'});
  assert.deepEqual(validateSliderFields('0', '1', '-1'), {step: 'Step must be greater than 0'});
  assert.deepEqual(validateSliderFields('0', '1', '2'), {step: 'Step must not be more than Max − Min'});
  assert.deepEqual(validateSliderFields('0', '1', ''), {step: 'A number'});
});

test('filterCandidates matches the name, case-insensitively; blank matches all', () => {
  const cs = [{kind: 'par' as const, name: 'iapp', value: 0.05}, {kind: 'par' as const, name: 'gNa', value: 120},
    {kind: 'ic' as const, name: 'V', value: -65}];
  assert.deepEqual(filterCandidates(cs, ''), cs);
  assert.deepEqual(filterCandidates(cs, 'na').map(c => c.name), ['gNa']);
  assert.deepEqual(filterCandidates(cs, 'IAPP').map(c => c.name), ['iapp']);
  assert.deepEqual(filterCandidates(cs, 'zz'), []);
});
