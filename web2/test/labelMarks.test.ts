import assert from 'node:assert/strict';
import {test} from 'node:test';
import {labelCoded, labelGlyph, labelMark, LABEL_MARKS} from '../src/plot/diagramModel';

/* W863: the one table of label marks. A bar (a branch's end) and a two-parameter point (a filled
   square) each share their mark on purpose, within their family; every other type has a mark of its
   own, and no type outside a family uses the bar or the filled square. */
const types = Object.keys(LABEL_MARKS);
const family = (sym: string) => LABEL_MARKS[sym].shape === 'bar' ? 'bar' : LABEL_MARKS[sym].twoParameter ? 'two' : '';

test('W863: every type outside the bar and two-parameter families has a shape and fill of its own', () => {
  const seen = new Map<string, string>();
  for (const sym of types.filter(s => family(s) === '')) {
    const m = LABEL_MARKS[sym], key = `${m.shape}/${m.filled ? 'filled' : 'hollow'}`;
    assert.equal(seen.has(key), false, `${sym} shares ${key} with ${seen.get(key)}`);
    seen.set(key, sym);
  }
});

test('W863: no type outside a family uses the bar or the filled square', () => {
  for (const sym of types.filter(s => family(s) === '')) {
    const m = LABEL_MARKS[sym];
    assert.notEqual(m.shape, 'bar', `${sym} uses the bar`);
    assert.equal(m.shape === 'square' && m.filled, false, `${sym} uses the filled square`);
  }
});

test('W863: every bar and every two-parameter type draws its code beside it, and the families are whole', () => {
  const bars = types.filter(s => family(s) === 'bar'), two = types.filter(s => family(s) === 'two');
  assert.ok(bars.length >= 6 && two.length >= 12, `${bars.length} bars, ${two.length} two-parameter types`);
  for (const sym of [...bars, ...two]) assert.equal(labelCoded(sym), true, `${sym} has no code beside it`);
  for (const sym of bars) assert.equal(LABEL_MARKS[sym].filled, true, `${sym} is not filled`);
  for (const sym of two) assert.equal(LABEL_MARKS[sym].filled, true, `${sym} is not filled`);
});

test('W863: the marks the mockup gives (LP filled triangle, LPC hollow, BP/BPC diamonds, PD hollow square)', () => {
  const glyph = (sym: string) => labelGlyph(labelMark(sym));
  assert.deepEqual(['fold', 'cycle_fold', 'branch_point', 'cycle_branch_point', 'period_doubling', 'neimark_sacker', 'end', 'snic', 'cusp']
    .map(glyph), ['▲', '△', '◆', '◇', '□', '☆', '❙', '❙', '■']);
  assert.equal(labelCoded(''), false);
  assert.equal(labelGlyph(labelMark('')), '·');
});
