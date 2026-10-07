import {test} from 'node:test';
import assert from 'node:assert/strict';
import {CONTINUE_INPUT_DEFAULT, continueDefault, continueEnd, continuePlan, numericsField, outputInterval, runInterval, steadyDefaults, steadyError} from '../src/store/steady';
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

test('the run interval is one for a map and Dt otherwise; the defaults follow Run duration and the core time', () => {
  const fields = (method: number) => [
    {key: 'total', label: 'Total', value: 20}, {key: 'dt', label: 'Dt', value: 0.05},
    {key: 'method', label: 'Method', value: method, choices: ['Discrete', 'Euler']}];
  assert.equal(runInterval(fields(0)), 1);
  assert.equal(runInterval(fields(1)), 0.05);
  assert.ok(Number.isNaN(runInterval(null)));
  assert.equal(numericsField(fields(1), 'total')?.value, 20);
  assert.equal(numericsField(null, 'total'), undefined);
  assert.deepEqual(steadyDefaults(HELLO.steady, 20, 0.05), {decimals: '9', hold: '1', maximum: '20'});
  /* a new Run duration moves the limit, and a short run caps the hold */
  assert.deepEqual(steadyDefaults(HELLO.steady, 0.5, 0.05), {decimals: '9', hold: '0.5', maximum: '0.5'});
  assert.equal(continueDefault('extra', 20, 7), '20');
  assert.equal(continueDefault('until', 20, 7), '27');
  assert.equal(continueDefault('until', 20, undefined), '20');
});

test('Continue until ends at the next interval from the core time, as the core rounds it', () => {
  const tol = HELLO.continue.grid_tolerance;
  assert.equal(continueEnd(1, 0, 0.25, tol), 1);                 // on the grid
  assert.equal(continueEnd(1 + 1e-9, 0, 0.25, tol), 1);          // within the tolerance counts as on it
  assert.ok(Math.abs(continueEnd(1.01, 0, 0.25, tol)! - 1.25) < 1e-12); // rounded up, never short
  assert.ok(Math.abs(continueEnd(5.1, 5, 0.25, tol)! - 5.25) < 1e-12);   // at least one interval
  assert.equal(continueEnd(3, 3, 0.25, tol), undefined);          // nothing to continue
  assert.equal(continueEnd(4, 3, NaN, tol), undefined);
});

test('Continue honours Store every N steps: the end shown is on the output grid, and the command is the field as typed (W213)', () => {
  const fields = (every: number) => [
    {key: 'total', label: 'Total', value: 20}, {key: 'dt', label: 'Dt', value: 0.05}, {key: 'method', label: 'Method', value: 1, choices: ['Discrete', 'Euler']},
    {key: 'store_every', label: 'Store every N steps', value: every, integer: true}];
  assert.ok(Math.abs(outputInterval(fields(7)) - 0.35) < 1e-12);
  assert.equal(outputInterval(fields(1)), 0.05);
  assert.ok(Number.isNaN(outputInterval(null)));
  const core = {time: 5, now: [1]} as unknown as Parameters<typeof continuePlan>[2];
  const none = continuePlan(CONTINUE_INPUT_DEFAULT, fields(1), core, HELLO);
  assert.deepEqual(none.command, {cmd: 'continue', extra: 20});   // the default is another Run duration
  assert.equal(none.problem, null);
  const until = continuePlan({mode: 'until', text: '6.1'}, fields(7), core, HELLO);
  assert.deepEqual(until.command, {cmd: 'continue', until: 6.1});
  assert.ok(Math.abs(until.end! - 6.4) < 1e-12);                   // 1.1 / 0.35 rounds up to 4 groups
  for (const text of ['', 'x', '0', '5.01']) {
    const bad = continuePlan({mode: 'until', text}, fields(1), core, HELLO);
    assert.equal(bad.command, null);
    assert.ok(bad.problem);
  }
  assert.equal(continuePlan(CONTINUE_INPUT_DEFAULT, fields(1), null, HELLO).command, null);   // nothing to continue yet
  assert.equal(continuePlan(CONTINUE_INPUT_DEFAULT, fields(1), null, HELLO).problem, null);
});
