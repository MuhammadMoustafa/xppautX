/* W859: the branch list's data. A branch's notes (what it found, why it stopped) are kept from every run's summary, not only the
   last one's; a reset drops those of the branches it cut; a periodic branch says where it started (the label its run started from). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {branchRows, buildDiagramModel, legendEntries, startedFrom} from '../src/plot/diagramModel';
import {activeView, type DiagramRun} from '../src/store/diagram';
import {reduce, type AppState} from '../src/store/state';
import {READY} from './hello';

const ev = (s: AppState, e: object) => reduce(s, {type: 'event', ev: e as never});
const axes = {xmin: 0, xmax: 2, ymin: 0, ymax: 2, x0: 70, y0: 26, wid: 603, hgt: 300, plot: 'maximum_and_minimum' as const, xlabel: 'p', ylabel: 'V'};
const run = (br: number, xs: number[], extra: Partial<DiagramRun> = {}): DiagramRun =>
  ({br, pt: 1, ty: 1, stable: true, periodic: false, d: 1, c: 'foreground', lw: 1, jac: 'difference', new: 1, x: xs, y: xs, ...extra});
const add = (from: number, runs: DiagramRun[]) => ({ev: 'autox.diagram', view: 'diagram_1', op: 'add', from, runs});
const summary = (br: number) => ({ev: 'autox.summary', branches: [{br, found: [{sym: 'LP', lab: br, name: 'fold', par: 'p', x: 0.5}], stop: {why: 'end', pt: 2, text: `branch ${br} ended`}}], follow: [], lines: []});

function opened(): AppState {
  const s = ev(READY, {ev: 'window.created', kind: 'diagram', win: 'diagram_1', w: 687, h: 352, title: 'autoX'});
  return ev(s, {ev: 'autox.diagram', view: 'diagram_1', op: 'axes', ...axes});
}

test('every run\'s summary adds its branches to the notes; the last summary is the last run\'s alone', () => {
  let s = ev(opened(), add(0, [run(1, [0.1, 0.2])]));
  s = ev(s, summary(1));
  s = ev(s, add(2, [run(2, [0.3, 0.4])]));
  s = reduce(s, {type: 'diagram', action: {type: 'run', op: 'start', at: 1}});
  assert.equal(s.diagram.summary, null);
  s = ev(s, summary(2));
  assert.deepEqual(Object.keys(s.diagram.branchNotes), ['1', '2']);
  assert.deepEqual(s.diagram.summary?.branches.map(b => b.br), [2]);
  assert.equal(s.diagram.branchNotes[1].stop?.text, 'branch 1 ended');
});

test('a reset that cuts a branch\'s points drops its notes', () => {
  let s = ev(opened(), add(0, [run(1, [0.1, 0.2])]));
  s = ev(s, add(2, [run(2, [0.3, 0.4])]));
  s = ev(s, summary(1));
  s = ev(s, summary(2));
  s = ev(s, {ev: 'autox.diagram', view: 'diagram_1', op: 'reset', keep: 2, ...axes});
  assert.deepEqual(Object.keys(s.diagram.branchNotes), ['1']);
  s = ev(s, {ev: 'autox.diagram', view: 'diagram_1', op: 'reset', keep: 0, ...axes});
  assert.deepEqual(Object.keys(s.diagram.branchNotes), []);
});

test('a branch row says which label its run started from, and the point there', () => {
  let s = ev(opened(), add(0, [run(1, [0.1, 0.2, 0.3], {lab: [[1, 7, 'hopf']]}), run(2, [0.15, 0.25], {from: 7, periodic: true, ty: 3, stable: true})]));
  const v = activeView(s.diagram);
  const m = buildDiagramModel(v.points, v.labels, v.axes, 0);
  const {entries} = legendEntries(m.curves, s.diagram.liveKeys);
  const rows = branchRows(entries, m.curves, v.points);
  assert.deepEqual(rows.map(r => r.from), [0, 7]);
  assert.ok(startedFrom(7, v.labels, v.points).endsWith('0.2'));
  assert.equal(startedFrom(0, v.labels, v.points), '');
  assert.equal(startedFrom(99, v.labels, v.points), '');
});
