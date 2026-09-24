/* The AUTO view's hands-on fixes (docs/ui-v2.md T21): the status strip's
   words, Clear's earlier branches, the settings file, the axis dialog's
   pure part, the label placement, the menu columns and where keys go. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {axesNames, boundText, spinStep, typedRange, yNeeds} from '../src/plot/axisDialog';
import {formatElapsed, kindOfPoint, runStatus} from '../src/plot/autoStatus';
import {buildDiagramModel} from '../src/plot/diagramModel';
import {placeLabels} from '../src/plot/labelPlace';
import {branchesBefore, earlierCount, type DiagramRun} from '../src/store/diagram';
import {initialState, reduce, type AppState} from '../src/store/state';
import {isHotkeyTarget} from '../src/ui/hotkeys';
import {menuRows} from '../src/ui/menuLayout';

const ev = (s: AppState, e: object) => reduce(s, {type: 'event', ev: e as never});
const act = (s: AppState, action: object) => reduce(s, {type: 'diagram', action: action as never});
const axes = {xmin: -0.2, xmax: 0.5, ymin: -0.5, ymax: 0.4, x0: 70, y0: 26, wid: 603, hgt: 300, plot: 2,
  xlabel: 'iapp', ylabel: 'V'};
const steady: DiagramRun[] = [
  {br: 1, pt: 1, ty: 2, d: 1, c: 0, lw: 1, new: 1, x: [0.05, 0.1, 0.2], y: [-0.44, -0.4, -0.3], lab: [[0, 1, 'EP']]},
  {br: 1, pt: 4, ty: 2, d: 1, c: 0, lw: 1, x: [0.26, 0.3], y: [-0.2, -0.1], lab: [[0, 2, 'HB']]},
];
const periodic: DiagramRun[] = [
  {br: 2, pt: 1, ty: 4, d: 3, c: 28, lw: 1, new: 1, x: [0.26, 0.25, 0.24], y: [-0.2, -0.1, 0], y2: [-0.2, -0.3, -0.4],
    lab: [[2, 3, 'LP']]},
];
const add = (from: number, runs: DiagramRun[]) => ({ev: 'diagram', op: 'add', from, runs});

function opened(): AppState {
  const s = ev(initialState, {ev: 'window', op: 'create', win: 101, w: 687, h: 352});
  return ev(s, {ev: 'diagram', op: 'axes', ...axes});
}

test('a run: its clock, what it computed, its last label, how it ended', () => {
  let s = act(opened(), {type: 'run', op: 'start', at: 1000});
  const flags = {asking: false, stopping: false};
  let st = runStatus(s.diagram.run, s.diagram.points, s.diagram.labels, 1500, {...flags, asking: true});
  assert.equal(st.phase, 'starting');
  s = act(s, {type: 'run', op: 'clock', at: 2000});
  s = ev(s, add(0, steady));
  st = runStatus(s.diagram.run, s.diagram.points, s.diagram.labels, 2500, flags);
  assert.equal(st.phase, 'running');
  assert.equal(st.text, 'Running: steady states');
  assert.deepEqual([st.branch, st.point, st.points, st.label, st.elapsed], [1, 5, 5, 'HB 2 at point 4', 500]);
  assert.equal(runStatus(s.diagram.run, s.diagram.points, s.diagram.labels, 2500, {...flags, stopping: true}).text, 'Stopping…');
  s = act(s, {type: 'run', op: 'end', at: 4000});
  st = runStatus(s.diagram.run, s.diagram.points, s.diagram.labels, 9999, flags);
  assert.deepEqual([st.phase, st.text, st.elapsed], ['done', 'Done: steady states', 2000]);
  /* the next run counts only its own points; a `stopped` event makes it "Stopped" */
  s = act(s, {type: 'run', op: 'start', at: 5000});
  s = ev(s, add(5, periodic));
  s = ev(s, {ev: 'stopped', at: {what: 'auto', branch: 2, point: 3}});
  s = act(s, {type: 'run', op: 'end', at: 6000});
  st = runStatus(s.diagram.run, s.diagram.points, s.diagram.labels, 9999, flags);
  assert.deepEqual([st.phase, st.text, st.points, st.branch, st.label], ['stopped', 'Stopped: periodic orbits', 3, 2,
    'LP 3 at point 3']);
  assert.equal(runStatus(null, s.diagram.points, s.diagram.labels, 0, flags).text, 'Idle');
});

test('the kind of a point and the elapsed time in words', () => {
  assert.deepEqual([kindOfPoint(1, 0), kindOfPoint(4, 0), kindOfPoint(2, 3)],
    ['steady states', 'periodic orbits', 'two-parameter curve']);
  assert.deepEqual([formatElapsed(420), formatElapsed(12400), formatElapsed(125000)], ['0.4 s', '12 s', '2:05']);
});

test('Clear hides the branches so far; new ones draw alone; the key shows them again', () => {
  let s = ev(opened(), add(0, steady));
  s = act(s, {type: 'clear'});
  assert.equal(earlierCount(s.diagram), 5);
  assert.equal(branchesBefore(s.diagram.points, 5), 1);
  s = ev(s, add(5, periodic));
  const hidden = buildDiagramModel(s.diagram.points, s.diagram.labels, s.diagram.axes, earlierCount(s.diagram));
  assert.ok(hidden.curves.every(c => c.branch === 2));
  assert.deepEqual(hidden.labels.map(l => l.sym), ['LP']);
  /* the periodic branch still starts at its Hopf point, which is hidden with its branch */
  assert.equal(hidden.hopf.length, 1);
  const all = buildDiagramModel(s.diagram.points, s.diagram.labels, s.diagram.axes, 0);
  assert.ok(all.curves.some(c => c.branch === 1));
  /* a redraw in other quantities keeps them hidden; a diagram emptied (Reset diagram) has none left */
  s = ev(s, {ev: 'diagram', op: 'reset', keep: 0, ...axes});
  s = ev(s, {ev: 'idle'});
  assert.equal(s.diagram.earlier, 0);
});

test('the axis dialog: the names shown, the spinner step, the range typed', () => {
  assert.deepEqual(axesNames({...axes, plot: 11, ylabel: 'V_bar'}), {par1: 'iapp', yvar: 'V', par2: ''});
  assert.deepEqual(axesNames({...axes, plot: 4, ylabel: 'phi'}), {par1: 'iapp', yvar: '', par2: 'phi'});
  assert.deepEqual([yNeeds(0), yNeeds(2), yNeeds(1), yNeeds(4)], ['var', 'var', null, 'par2']);
  assert.deepEqual([spinStep(18000), spinStep(0.7), spinStep(0)], [1000, 0.01, 1]);
  assert.deepEqual(typedRange('-1', '2'), {min: -1, max: 2});
  assert.equal(typedRange('2', '1'), null);
  assert.equal(typedRange('', '1'), null);
  assert.equal(boundText(0.1 + 0.2), '0.30000000000000004');
  assert.equal(boundText(-0.4416610002), '-0.4416610002');
});

test('two names at one spot are written a line apart', () => {
  const tops = placeLabels([{x: 10, y: 10, w: 30, h: 12}, {x: 12, y: 12, w: 30, h: 12}, {x: 200, y: 10, w: 30, h: 12}]);
  assert.equal(tops[0], 10);
  assert.equal(tops[1], 24);
  assert.equal(tops[2], 10);
});

test('a long menu is laid out in columns of at most 8, filled evenly', () => {
  assert.deepEqual([menuRows(5), menuRows(8), menuRows(14), menuRows(15), menuRows(17)], [5, 8, 7, 8, 6]);
});

test('where a key goes: letters on a button are hotkeys, nothing typed in a field is', () => {
  const el = (matches: string[]) => ({closest: (sel: string) => (matches.some(m => sel.split(', ').includes(m)) ? {} : null)}) as
    unknown as Element;
  assert.equal(isHotkeyTarget(el([]), 'f'), true);
  assert.equal(isHotkeyTarget(el(['button']), 'f'), true);
  assert.equal(isHotkeyTarget(el(['button']), 'Escape'), true);
  assert.equal(isHotkeyTarget(el(['button']), 'Enter'), false);
  assert.equal(isHotkeyTarget(el(['button']), ' '), false);
  assert.equal(isHotkeyTarget(el(['input']), 'f'), false);
  assert.equal(isHotkeyTarget(el(['[role="dialog"]']), 'g'), false);
  assert.equal(isHotkeyTarget(null, 'g'), true);
});
