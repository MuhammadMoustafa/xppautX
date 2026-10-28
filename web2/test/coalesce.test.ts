/* Values sent (store/values.ts, GitHub #18, #155): one `set` for one
   value or several; the model's defaults by field. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {initialValues, reduceValues, valueCommands} from '../src/store/values';

const HELLO = {commands: [
  {cmd: 'run.configure', arguments: [{name: 'total_time', type: 'real'}, {name: 'method', type: 'text'}, {name: 'stop_at_limit', type: 'boolean'}]},
  {cmd: 'bvp.configure', arguments: [{name: 'max_iterates', type: 'integer'}]}]} as never;

test('values go out as the commands that take them: parameters in one set, states in ic.set, numerics in their own', () => {
  assert.deepEqual(valueCommands(HELLO, []), []);
  assert.deepEqual(valueCommands(HELLO, [{kind: 'par', name: 'iapp', text: '0.1'}]).map(c => c.command),
    [{cmd: 'set', kind: 'par', name: 'iapp', text: '0.1'}]);
  assert.deepEqual(valueCommands(HELLO, [{kind: 'par', name: 'iapp', text: '0.1'}, {kind: 'bc', index: 1, text: 'w'}]).map(c => c.command), [{
    cmd: 'set', values: [{kind: 'par', name: 'iapp', text: '0.1'}, {kind: 'bc', index: 1, text: 'w'}],
  }]);
  assert.deepEqual(valueCommands(HELLO, [
    {kind: 'ic', name: 'v', text: '-0.2'}, {kind: 'ic', name: 'w', text: '%2*0.1'}, {kind: 'num', name: 'total_time', text: '30'},
    {kind: 'num', name: 'method', text: 'euler'}, {kind: 'num', name: 'stop_at_limit', text: 'true'}, {kind: 'num', name: 'bvp_max_iterates', text: '5'},
    {kind: 'num', name: 'nullcline_mesh', text: '40'}]).map(c => c.command), [
    {cmd: 'set', kind: 'num', name: 'nullcline_mesh', text: '40'},
    {cmd: 'ic.set', values: {v: -0.2}},
    {cmd: 'ic.formula', variable: 'w', formula: '2*0.1'},
    {cmd: 'run.configure', total_time: 30, method: 'euler', stop_at_limit: true},
    {cmd: 'bvp.configure', max_iterates: 5},
  ]);
});

test('defaults by field key', () => {
  const s = reduceValues(initialValues, {type: 'defaults', pars: [['Iapp', 0.2]], ics: [['V', -0.1]]});
  assert.deepEqual(s.defaults, {'par:iapp': 0.2, 'ic:v': -0.1});
});
