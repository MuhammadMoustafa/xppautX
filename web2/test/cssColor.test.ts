import {test} from 'node:test';
import assert from 'node:assert/strict';
import {GRAPHIC_CONTRAST, contrastRatio, cssToHex, joinColor, lightenForBackground, splitColor} from '../src/plot/colors';

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
  /* a named color goes through the page's own conversion */
  assert.equal(cssToHex('red', c => c === 'red' ? '#ff0000' : null), '#ff0000');
  assert.equal(cssToHex('nonsense', () => null), null);
  assert.equal(cssToHex('looping', c => c), null);
});

const DARK_BG = '#121212';
const hsl = (hex: string) => {
  const [r, g, b] = [1, 3, 5].map(i => parseInt(hex.slice(i, i + 2), 16) / 255);
  const max = Math.max(r, g, b), min = Math.min(r, g, b), l = (max + min) / 2;
  const s = max === min ? 0 : (max - min) / (1 - Math.abs(2 * l - 1));
  const h = max === min ? 0 : (max === r ? ((g - b) / (max - min)) % 6 : max === g ? (b - r) / (max - min) + 2 : (r - g) / (max - min) + 4) * 60;
  return {h: (h + 360) % 360, s, l};
};

test('pure blue, too dark for the dark background, is lightened to the contrast, still blue', () => {
  const out = lightenForBackground('#0000ff', DARK_BG);
  assert.match(out, /^#[0-9a-f]{6}$/);
  assert.ok(contrastRatio(out, DARK_BG) >= GRAPHIC_CONTRAST, out);
  assert.ok(contrastRatio(out, DARK_BG) < GRAPHIC_CONTRAST + 0.5, `lightened no more than needed: ${out}`);
  const {h, s} = hsl(out);
  assert.ok(Math.abs(h - 240) < 2 && s > 0.99, `hue and saturation kept: ${out}`);
});

test('black is lightened to a grey that reads; the alpha is kept', () => {
  const black = lightenForBackground('#000000', DARK_BG);
  assert.ok(contrastRatio(black, DARK_BG) >= GRAPHIC_CONTRAST, black);
  const [r, g, b] = [1, 3, 5].map(i => black.slice(i, i + 2));
  assert.ok(r === g && g === b, `a grey: ${black}`);
  assert.equal(lightenForBackground('#0000ff80', DARK_BG).slice(-2), '80');
});

test('a color that already reads, a light one, and what is not a hex color come back as they were', () => {
  assert.equal(lightenForBackground('#ff0000', DARK_BG), '#ff0000');
  assert.equal(lightenForBackground('#ffffff', DARK_BG), '#ffffff');
  assert.equal(lightenForBackground('#0000ffcc', '#ffffff'), '#0000ffcc');
  assert.equal(lightenForBackground('foreground', DARK_BG), 'foreground');
});

test('a picker gets the color as given, never the lightened one, and gives back what it was set to', () => {
  assert.deepEqual(splitColor('#0000ff'), {hex: '#0000ff', alpha: 255});
  assert.deepEqual(splitColor('#0000ff80'), {hex: '#0000ff', alpha: 128});
  assert.equal(joinColor('#0000ff', 128), '#0000ff80');
  assert.equal(joinColor('#0000FF', 255), '#0000ff');
});
