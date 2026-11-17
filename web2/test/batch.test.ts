/* The batch runner's pure parts (W774, W775): the commands it sends, the messages it says, its state, and what a pin's step is run as
   and what its check says. The runner itself (session.ts batchRun) is checked in web2check (section pins and export). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {doneMessage, endLine, startLine, stepCommand, stoppedMessage} from '../src/batch';
import {initialBatch, reduceBatch, shortTitle, type BatchOutcome} from '../src/store/batch';
import {pinSteps, planStep, stepStatus, type StepRun} from '../src/pins';
import type {ExportItem} from '../src/exportSeveral';

const item = (cmd: string, args: Record<string, string>, file: string): ExportItem => ({key: cmd, cmd, args, label: '', outputKey: cmd, file});
const items = [item('plot.export', {format: 'pdf'}, 'lecar.pdf'), item('data.export', {target: 'table', format: 'csv'}, 'lecar.csv')];
const out = (status: BatchOutcome['status'], text = ''): BatchOutcome => ({step: 's', status, text});

test('a step is sent with its file, the id and the replace answer only when it writes a file', () => {
  assert.deepEqual(stepCommand({cmd: 'plot.export', format: 'pdf'}, 'a.pdf', 'b-1', 1), {cmd: 'plot.export', format: 'pdf', file: 'a.pdf', id: 'b-1', replace: 1});
  assert.deepEqual(stepCommand({cmd: 'run.initial'}, null, 'b-2', 1), {cmd: 'run.initial', id: 'b-2'});
});

test('a step that asks for its file takes the core\'s output name; one that names a file keeps it; one with no name known is a problem', () => {
  assert.deepEqual(planStep('plot.export --format pdf', {cmd: 'plot.export', format: 'pdf', file_picker: true}, items),
    {command: {cmd: 'plot.export', format: 'pdf'}, file: 'lecar.pdf', problem: null});
  assert.equal(planStep('plot.export --format pdf --file "x.pdf"', {cmd: 'plot.export', format: 'pdf', file: 'x.pdf'}, items).file, 'x.pdf');
  assert.equal(planStep('plot.export --format svg', {cmd: 'plot.export', format: 'svg', file_picker: true}, items).problem, 'it asks for its file: write --file "name"');
  assert.deepEqual(planStep('run.initial', {cmd: 'run.initial'}, items), {command: {cmd: 'run.initial'}, file: null, problem: null});
  assert.ok(planStep('x', null, items).problem);
});

test('each step shows its own check: runs, writes, exists, cannot run now, or its problem', () => {
  const runs = planStep('run.initial', {cmd: 'run.initial'}, items);
  const writes = planStep('plot.export --format pdf', {cmd: 'plot.export', format: 'pdf', file_picker: true}, items);
  assert.deepEqual(stepStatus(runs, [], false), {kind: 'ok', text: '✓ runs'});
  assert.deepEqual(stepStatus(writes, [], false), {kind: 'ok', text: '✓ writes lecar.pdf'});
  assert.equal(stepStatus(writes, [], true).kind, 'warn');
  assert.deepEqual(stepStatus(writes, [{cmd: 'plot.export', reason: 'needs a plot'}], false), {kind: 'no', text: '✕ cannot run now: needs a plot'});
  assert.equal(stepStatus({...runs, problem: 'bad'}, [], false).text, '✕ bad');
});

test('a pin\'s steps are the batch, the first problem named with its step', () => {
  const run = (s: string): StepRun => (s === 'run.initial' ? {cmd: 'run.initial'} : {cmd: 'plot.export', format: 'svg', file_picker: true});
  const ok = pinSteps({name: 'F', steps: ['run.initial']}, run, items);
  assert.deepEqual(ok.steps.map(s => s.label), ['run.initial']);
  assert.equal(ok.problem, null);
  const bad = pinSteps({name: 'F', steps: ['run.initial', 'plot.export --format svg']}, run, items);
  assert.match(bad.problem!, /^step 2 of 2, plot\.export --format svg: it asks/);
});

test('the one message of a batch that ended names the step, what was done and what was not run', () => {
  const all = ['a', 'b', 'c', 'd'];
  assert.equal(stoppedMessage('Pin "F"', all, [out('ran'), out('failed', 'no plot'), ]),
    'Pin "F" stopped at step 2 of 4, b: no plot. Done: 1 step (1). Not run: 3, 4.');
  assert.equal(stoppedMessage('Pin "F"', all, [out('ran'), out('written', 'x'), out('ran')]),
    'Pin "F" stopped at step 4 of 4, d: Stop was pressed. Done: 3 steps (1, 2, 3). Not run: none.');
  assert.equal(stoppedMessage('Pin "F"', all, [out('ran'), out('ran'), out('ran'), out('ran')]), null);
  assert.equal(doneMessage('Pin "F"', 4), 'Pin "F": 4 steps done.');
  assert.equal(startLine('Pin "F"', 1, 4, 'b'), 'Pin "F": step 2 of 4, b');
  assert.equal(endLine('Pin "F"', 1, 4, out('failed', 'no plot')), 'Pin "F": step 2 of 4 failed: no plot');
});

test('the state follows a batch: asked, running, step by step, ended with a report or cleared', () => {
  let s = reduceBatch(initialBatch, {type: 'ask', title: 'T', steps: ['a', 'b'], exists: ['a']});
  assert.deepEqual([s.phase, s.exists], ['exists', ['a']]);
  s = reduceBatch(s, {type: 'run', title: 'T', steps: ['a', 'b']});
  s = reduceBatch(s, {type: 'step', index: 1});
  s = reduceBatch(s, {type: 'outcome', outcome: out('ran')});
  assert.deepEqual([s.phase, s.index, s.outcomes.length, s.exists], ['running', 1, 1, []]);
  assert.equal(reduceBatch(s, {type: 'end', report: true}).phase, 'report');
  assert.equal(reduceBatch(s, {type: 'end', report: false}), initialBatch);
  assert.equal(reduceBatch(s, {type: 'clear'}), initialBatch);
});

test('a narrow status bar names the pin without its wrapper', () => {
  assert.equal(shortTitle('Pin "Figures"'), 'Figures');
  assert.equal(shortTitle('Export several'), 'Export several');
  assert.equal(shortTitle(null), '');
});
