/* Colour maps for the array plot (docs/ui-v2.md T12, plot/aplotColors.ts),
   without a browser (npm test). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {BLANK_COLOR, cellColor, legendStops, mapColor} from '../src/plot/aplotColors';

test('mapColor returns a 6-digit hex colour for every map at the ends and the middle', () => {
  for (const map of ['viridis', 'xpp'] as const) {
    for (const t of [0, 0.5, 1]) {
      assert.match(mapColor(map, t), /^#[0-9a-f]{6}$/, `${map} at ${t}`);
    }
  }
});

test('mapColor clamps out-of-range t instead of producing nonsense', () => {
  assert.equal(mapColor('viridis', -1), mapColor('viridis', 0));
  assert.equal(mapColor('viridis', 2), mapColor('viridis', 1));
  assert.equal(mapColor('xpp', -1), mapColor('xpp', 0));
  assert.equal(mapColor('xpp', 2), mapColor('xpp', 1));
});

test('mapColor is not the same colour at the two ends (it is a scale, not a solid fill)', () => {
  assert.notEqual(mapColor('viridis', 0), mapColor('viridis', 1));
  assert.notEqual(mapColor('xpp', 0), mapColor('xpp', 1));
});

test('viridis runs dark purple to yellow, low to high (matplotlib\'s table)', () => {
  const low = mapColor('viridis', 0), high = mapColor('viridis', 1);
  assert.equal(low, '#440154');
  assert.equal(high, '#fde725');
});

test('xpp\'s own map is red at the top of the scale (core/colormap.c C_NORM, x=0)', () => {
  const high = mapColor('xpp', 1);
  assert.equal(high, '#ff0000');
});

test('mapColor is deterministic: the same t always gives the same colour', () => {
  for (let i = 0; i <= 10; i++) {
    const t = i / 10;
    assert.equal(mapColor('viridis', t), mapColor('viridis', t));
    assert.equal(mapColor('xpp', t), mapColor('xpp', t));
  }
});

test('cellColor is blank for a non-finite value', () => {
  assert.equal(cellColor('viridis', NaN, 0, 1), BLANK_COLOR);
  assert.equal(cellColor('viridis', Infinity, 0, 1), BLANK_COLOR);
});

test('cellColor scales value between zmin and zmax', () => {
  assert.equal(cellColor('viridis', 0, 0, 10), mapColor('viridis', 0));
  assert.equal(cellColor('viridis', 5, 0, 10), mapColor('viridis', 0.5));
  assert.equal(cellColor('viridis', 10, 0, 10), mapColor('viridis', 1));
});

test('cellColor does not crash on a degenerate range (zmax <= zmin)', () => {
  assert.match(cellColor('viridis', 3, 5, 5), /^#[0-9a-f]{6}$/);
  assert.match(cellColor('xpp', 3, 5, 2), /^#[0-9a-f]{6}$/);
});

test('legendStops runs from zmax (index 0) to zmin (the last), n stops', () => {
  const stops = legendStops('viridis', 5);
  assert.equal(stops.length, 5);
  assert.equal(stops[0], mapColor('viridis', 1));
  assert.equal(stops[4], mapColor('viridis', 0));
});
