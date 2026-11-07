/* W597, W620: the page for several branches. Feeds the contract's events: the points of several running jobs arriving
   interleaved (each job its own polyline, typed but not numbered), the join's reset and the labelled points that replace
   them, `autox.jobs` (the progress line, each running job's points), and the setting. Data, never pixels. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {buildDiagramModel} from '../src/plot/diagramModel';
import {jobsText, runStatus} from '../src/plot/autoxStatus';
import {activeView, type DiagramRun} from '../src/store/diagram';
import {NUM_GROUPS, NUM_KEYS} from '../src/store/autoxSettings';
import {reduce, type AppState} from '../src/store/state';
import {READY} from './hello';

const ev = (s: AppState, e: object) => reduce(s, {type: 'event', ev: e as never});
const axes = {xmin: 0, xmax: 2, ymin: 0, ymax: 2, x0: 70, y0: 26, wid: 603, hgt: 300, plot: 'maximum_and_minimum' as const,
  xlabel: 'p', ylabel: 'V'};
const run = (live: number | undefined, pt: number, xs: number[], extra: Partial<DiagramRun> = {}): DiagramRun =>
  ({br: 1, pt, ty: 1, stable: true, periodic: false, d: 1, c: 'foreground', lw: 1, jac: 'difference', new: pt === 1 ? 1 : undefined,
    x: xs, y: xs.map(x => x * 2), ...(live === undefined ? {} : {live}), ...extra});
const add = (from: number, runs: DiagramRun[]) => ({ev: 'autox.diagram', view: 'diagram_1', op: 'add', from, runs});

function opened(): AppState {
  const s = ev(READY, {ev: 'window.created', kind: 'diagram', win: 'diagram_1', w: 687, h: 352, title: 'autoX'});
  return ev(s, {ev: 'autox.diagram', view: 'diagram_1', op: 'axes', ...axes});
}

/* two jobs' points arrive interleaved, as the threads produce them; job 1 has a limit point (typed, no number yet) */
function live(): AppState {
  let s = opened();
  s = ev(s, add(0, [run(1, 1, [0.1, 0.2]), run(2, 1, [0.9, 0.8])]));
  s = ev(s, add(4, [run(1, 3, [0.3], {lab: [[0, 0, 'fold']]})]));
  return ev(s, add(5, [run(2, 3, [0.7, 0.6, 0.5])]));
}

test('points of several jobs are kept in arrival order with their job', () => {
  const p = activeView(live().diagram).points;
  assert.deepEqual([...p.lv], [1, 1, 2, 2, 1, 2, 2, 2]);
  assert.deepEqual([...p.x], [0.1, 0.2, 0.9, 0.8, 0.3, 0.7, 0.6, 0.5]);
});

test('each job is its own polyline: no line joins two jobs, and a job is whole though others arrive between', () => {
  const v = activeView(live().diagram);
  const m = buildDiagramModel(v.points, v.labels, v.axes);
  assert.deepEqual(m.curves.map(c => [...c.idx]), [[0, 1, 4], [2, 3, 5, 6, 7]]);
});

test('a live point shows its type and no number until the join', () => {
  const v = activeView(live().diagram);
  assert.deepEqual(v.labels, [{point: 4, lab: 0, sym: 'fold'}]);
});

test('the join replaces the live points: a reset to the points before them, then the labelled points with no job', () => {
  let s = ev(opened(), add(0, [run(undefined, 1, [0.5])])); /* a point of the diagram before the run */
  s = ev(s, add(1, [run(1, 1, [0.1, 0.2]), run(2, 1, [0.9, 0.8])]));
  assert.equal(activeView(s.diagram).points.x.length, 5);
  s = ev(s, {ev: 'autox.diagram', view: 'diagram_1', op: 'reset', keep: 1, ...axes});
  assert.deepEqual([...activeView(s.diagram).points.lv], [0]);
  s = ev(s, add(1, [run(undefined, 2, [0.1, 0.2, 0.3], {lab: [[2, 1, 'fold']]}), run(undefined, 4, [0.9, 0.8, 0.7], {br: 2})]));
  const v = activeView(s.diagram);
  assert.deepEqual([...v.points.lv], [0, 0, 0, 0, 0, 0, 0]);
  assert.deepEqual(v.labels, [{point: 3, lab: 1, sym: 'fold'}]);
  assert.deepEqual(buildDiagramModel(v.points, v.labels, v.axes).curves.map(c => c.branch), [1, 2]);
});

const jobs = (running: number, queued: number, done: number, list: [number, number[], number][]) =>
  ({ev: 'autox.jobs', running, queued, done, jobs: list.map(([id, key, points]) => ({live: id, key, points}))});

test('autox.jobs gives the progress line, each running job by its key and points, and none once running is 0', () => {
  let s = ev(live(), jobs(2, 3, 1, [[1, [0], 3], [4, [0, 5], 1]]));
  assert.deepEqual(s.diagram.jobs?.jobs, [{live: 1, key: [0], points: 3}, {live: 4, key: [0, 5], points: 1}]);
  assert.deepEqual([s.diagram.jobs?.running, s.diagram.jobs?.queued, s.diagram.jobs?.done], [2, 3, 1]);
  assert.equal(jobsText(s.diagram.jobs), '2 running (job 0: 3 points, job 0.5: 1 point), 3 queued, 1 done');
  s = ev(s, jobs(0, 0, 6, []));
  assert.equal(jobsText(s.diagram.jobs), '');
  assert.equal(jobsText(null), '');
});

test('a new run clears the last progress line', () => {
  let s = ev(live(), jobs(1, 0, 2, [[3, [0, 4], 1]]));
  s = reduce(s, {type: 'diagram', action: {type: 'run', op: 'start', at: 1}});
  assert.equal(s.diagram.jobs, null);
});

test('the status of a live run counts its points and leaves the label number out', () => {
  const v = activeView(live().diagram);
  const st = runStatus({active: true, started: 0, ended: null, first: 0, stopped: false}, v.points, v.labels, 10,
    {asking: false, stopping: false});
  assert.equal(st.phase, 'running');
  assert.equal(st.points, 8);
  assert.match(st.label ?? '', /at point 3$/);
  assert.doesNotMatch(st.label ?? '', /\b0\b/);
});

test("follow_all_branches is one of autoX's boolean settings, in the Solver group", () => {
  assert.ok(NUM_KEYS.includes('follow_all_branches'));
  assert.ok(NUM_GROUPS.find(g => g.title === 'Solver')!.keys.includes('follow_all_branches'));
});
