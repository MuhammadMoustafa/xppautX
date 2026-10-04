import {test} from 'node:test';
import assert from 'node:assert/strict';
import {steadyError} from '../src/store/steady';
import {HELLO} from './hello';
import {kindOf, mayStart} from '../src/protocol/kinds';

test('steady settings reject incomplete precision, insufficient hold and invalid limits', () => {
  const valid = {decimals: '9', hold: '1', maximum: '30'};
  assert.equal(steadyError(valid, 0.05, 15), null);
  for (const decimals of ['', '9.5', '-1', '16']) assert.ok(steadyError({...valid, decimals}, 0.05, 15));
  for (const hold of ['', '0', '0.01', 'Infinity']) assert.ok(steadyError({...valid, hold}, 0.05, 15));
  for (const maximum of ['', '0.5', 'Infinity']) assert.ok(steadyError({...valid, maximum}, 0.05, 15));
  assert.ok(steadyError(valid, -0.05, 15));
});
test('steady and direct continuation are computations guarded during another run or prompt', () => {
  for (const cmd of ['steady', 'continue']) {
    const kind = kindOf(HELLO, 1, {cmd});
    assert.equal(kind, 'computation');
    assert.equal(mayStart(kind, true, false), false);
    assert.equal(mayStart(kind, false, true), false);
    assert.equal(mayStart(kind, false, false), true);
  }
});
