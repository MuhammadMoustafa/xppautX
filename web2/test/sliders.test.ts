/* The sliders' ranges and positions (store/sliders.ts, GitHub #18). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {defaultRange, fromPosition, presetSliders, RANGE_STEPS, sliderRange, toPosition} from '../src/store/sliders';
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
  assert.deepEqual(presetSliders([{name: 'iapp', lo: 0, hi: 0.2}], 1), [{id: 1, name: 'iapp', lo: '0', hi: '0.2'}]);
  let s = reduceValues(initialValues, {type: 'presetSliders', defs: [{name: 'iapp', lo: 0, hi: 0.2}]});
  s = reduceValues(s, {type: 'presetSliders', defs: [{name: 'phi', lo: 0, hi: 1}]});
  assert.deepEqual(s.sliders.map(d => d.name), ['iapp'], 'a reconnection keeps the list');
  s = reduceValues(s, {type: 'addSlider'});
  s = reduceValues(s, {type: 'addSlider'});
  assert.deepEqual(s.sliders.map(d => d.id), [1, 2, 3]);
  s = reduceValues(s, {type: 'setSlider', id: 2, patch: {name: 'phi', lo: '0', hi: '0.08'}});
  assert.deepEqual(s.sliders[1], {id: 2, name: 'phi', lo: '0', hi: '0.08'});
  s = reduceValues(s, {type: 'removeSlider', id: 1});
  assert.deepEqual(s.sliders.map(d => d.id), [2, 3]);
});
