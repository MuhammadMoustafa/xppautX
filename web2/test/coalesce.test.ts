/* Values sent (store/values.ts, GitHub #18, #155): one `set` for one
   value or several; the model's defaults by field. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {initialValues, reduceValues, setCommand} from '../src/store/values';

test('values go out as one set: one value plain, several in values[]', () => {
  assert.equal(setCommand([]), null);
  assert.deepEqual(setCommand([{kind: 'par', name: 'iapp', text: '0.1'}]),
    {cmd: 'set', kind: 'par', name: 'iapp', text: '0.1'});
  assert.deepEqual(setCommand([{kind: 'par', name: 'iapp', text: '0.1'}, {kind: 'bc', index: 1, text: 'w'}]), {
    cmd: 'set', values: [{kind: 'par', name: 'iapp', text: '0.1'}, {kind: 'bc', index: 1, text: 'w'}],
  });
});

test('defaults by field key', () => {
  const s = reduceValues(initialValues, {type: 'defaults', pars: [['Iapp', 0.2]], ics: [['V', -0.1]]});
  assert.deepEqual(s.defaults, {'par:iapp': 0.2, 'ic:v': -0.1});
});
