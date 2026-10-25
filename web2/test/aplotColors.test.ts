/* The array plot's colour rule (docs/ui-v2.md T12, plot/aplotColors.ts: core/colormap.h's ramp_color), without a
   browser (npm test). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {BLANK_COLOR, cellColor, legendStops, rampColor} from '../src/plot/aplotColors';

/** a ramp of 256 colours: the grays, so a colour says its own index */
const ramp = Array.from({length: 256}, (_, i) => '#' + i.toString(16).padStart(2, '0').repeat(3));

test('rampColor picks the colour at floor(fraction * 256), clamped to the ends', () => {
  assert.equal(rampColor(ramp, 0), '#000000');
  assert.equal(rampColor(ramp, 0.5), '#808080');
  assert.equal(rampColor(ramp, 1), '#ffffff');
  assert.equal(rampColor(ramp, -1), '#000000');
  assert.equal(rampColor(ramp, 2), '#ffffff');
  assert.equal(rampColor(ramp, NaN), '#000000');
});

test('cellColor is blank for a non-finite value', () => {
  assert.equal(cellColor(ramp, NaN, 0, 1), BLANK_COLOR);
  assert.equal(cellColor(ramp, Infinity, 0, 1), BLANK_COLOR);
});

test('cellColor scales value between zmin and zmax', () => {
  assert.equal(cellColor(ramp, 0, 0, 10), rampColor(ramp, 0));
  assert.equal(cellColor(ramp, 5, 0, 10), rampColor(ramp, 0.5));
  assert.equal(cellColor(ramp, 10, 0, 10), rampColor(ramp, 1));
});

test('cellColor is blank on a degenerate range (zmax not above zmin), as the core\'s print is', () => {
  assert.equal(cellColor(ramp, 3, 5, 5), BLANK_COLOR);
  assert.equal(cellColor(ramp, 3, 5, 2), BLANK_COLOR);
});

test('legendStops runs from zmax (index 0) to zmin (the last), n stops', () => {
  const stops = legendStops(ramp, 5);
  assert.equal(stops.length, 5);
  assert.equal(stops[0], rampColor(ramp, 1));
  assert.equal(stops[4], rampColor(ramp, 0));
});
