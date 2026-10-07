import {test} from 'node:test';
import assert from 'node:assert/strict';
import {cssToHex} from '../src/plot/colors';

test('cssToHex gives the picker its #rrggbb for every form the page holds', () => {
  assert.equal(cssToHex('#1F2937'), '#1f2937');
  assert.equal(cssToHex('#fa0'), '#ffaa00');
  assert.equal(cssToHex('rgb(31, 41, 55)'), '#1f2937');
  assert.equal(cssToHex('rgba(255 0 128 / 0.5)'), '#ff0080');
  assert.equal(cssToHex('rgb(100%, 0%, 50%)'), '#ff0080');
  assert.equal(cssToHex('hsl(0, 100%, 50%)'), '#ff0000');
  assert.equal(cssToHex('hsl(120 100% 25%)'), '#008000');
  assert.equal(cssToHex('hsla(240, 100%, 50%, 0.3)'), '#0000ff');
  assert.equal(cssToHex('hsl(-120, 100%, 50%)'), '#0000ff');
});

test('cssToHex never answers black for what it cannot read', () => {
  assert.equal(cssToHex('rebeccapurple'), null);
  assert.equal(cssToHex('rgb(1, 2)'), null);
  assert.equal(cssToHex('rgb(a, b, c)'), null);
  assert.equal(cssToHex(''), null);
  /* a named colour goes through the page's own conversion */
  assert.equal(cssToHex('red', c => c === 'red' ? '#ff0000' : null), '#ff0000');
  assert.equal(cssToHex('nonsense', () => null), null);
  assert.equal(cssToHex('looping', c => c), null);
});
