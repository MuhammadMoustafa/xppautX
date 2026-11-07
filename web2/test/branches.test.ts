/* W597: the page for several branches: `autox.jobs` gives the progress line (data, never pixels); the
   setting is one of autoX's boolean settings. The core joins the jobs and sends the diagram at the run's end. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {jobsText} from '../src/plot/autoxStatus';
import {NUM_GROUPS, NUM_KEYS} from '../src/store/autoxSettings';
import {reduce, type AppState} from '../src/store/state';
import {READY} from './hello';

const ev = (s: AppState, e: object) => reduce(s, {type: 'event', ev: e as never});
const jobs = (running: number, queued: number, done: number) => ({ev: 'autox.jobs', running, queued, done});

test('autox.jobs gives the progress line while jobs run and none once running is 0', () => {
  let s = ev(READY, jobs(2, 3, 1));
  assert.equal(s.diagram.jobs?.running, 2);
  assert.equal(jobsText(s.diagram.jobs), '2 running, 3 queued, 1 done');
  s = ev(s, jobs(0, 0, 6));
  assert.equal(jobsText(s.diagram.jobs), '');
  assert.equal(jobsText(null), '');
});

test('a new run clears the last progress line', () => {
  let s = ev(READY, jobs(1, 0, 2));
  s = reduce(s, {type: 'diagram', action: {type: 'run', op: 'start', at: 1}});
  assert.equal(s.diagram.jobs, null);
});

test("follow_all_branches is one of autoX's boolean settings, in the Solver group", () => {
  assert.ok(NUM_KEYS.includes('follow_all_branches'));
  assert.ok(NUM_GROUPS.find(g => g.title === 'Solver')!.keys.includes('follow_all_branches'));
});
