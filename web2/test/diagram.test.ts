/* The AUTO diagram (docs/ui-v2.md T11a): the store built from `diagram`
   events (full and `add` increments), the curves by branch and stability,
   and the segment from a Hopf point to its periodic branch. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {circlePoints, complexText, infoRows, stabilitySummary} from '../src/plot/autoInfo';
import {buildDiagramModel, describePoint, grabStep, hopfOf, nearestVertex, stepLabel, vertexOf} from '../src/plot/diagramModel';
import {initialDiagram, pointCount, type DiagramRun, type DiagramState} from '../src/store/diagram';
import {initialState, reduce, type AppState} from '../src/store/state';

const ev = (s: AppState, e: object) => reduce(s, {type: 'event', ev: e as never});
const axes = {xmin: -0.2, xmax: 0.5, ymin: -0.5, ymax: 0.4, x0: 70, y0: 26, wid: 603, hgt: 300, plot: 2,
  xlabel: 'iapp', ylabel: 'V'};
const line = (n: number, x0: number, dx: number, y0: number, dy: number) =>
  Array.from({length: n}, (_, i) => [x0 + i * dx, y0 + i * dy]);

/* a steady-state branch like lecar's: an end point, stable, unstable between two Hopf points, stable */
const steady: DiagramRun[] = [
  {br: 1, pt: 1, ty: 2, d: 1, c: 0, lw: 1, new: 1, x: [0.05], y: [-0.44], lab: [[0, 1, 'EP']]},
  {br: 1, pt: 2, ty: 1, d: 1, c: 20, lw: 2, x: line(3, 0.07, 0.05, -0.41, 0.05).map(p => p[0]),
    y: line(3, 0.07, 0.05, -0.41, 0.05).map(p => p[1])},
  {br: 1, pt: 5, ty: 2, d: 1, c: 0, lw: 1, x: [0.26, 0.3, 0.45], y: [-0.2, -0.1, 0.06],
    lab: [[0, 2, 'HB'], [2, 3, 'HB']]},
  {br: 1, pt: 8, ty: 1, d: 1, c: 20, lw: 2, x: [0.47, 0.5], y: [0.07, 0.08], lab: [[1, 4, 'EP']]},
];
/* the periodic branch from the first Hopf point: unstable, then stable, max and min */
const periodic: DiagramRun[] = [
  {br: 2, pt: 1, ty: 4, d: 3, c: 28, lw: 1, new: 1, x: [0.2601, 0.259, 0.255], y: [-0.2, -0.17, -0.14],
    y2: [-0.2, -0.23, -0.26]},
  {br: 2, pt: 4, ty: 3, d: 2, c: 26, lw: 1, x: [0.25, 0.24], y: [-0.1, -0.05], y2: [-0.3, -0.35], lab: [[1, 5, 'LP']]},
];
const add = (from: number, runs: DiagramRun[]) => ({ev: 'diagram', op: 'add', from, runs});

function opened(): AppState {
  let s = ev(initialState, {ev: 'window', op: 'create', win: 101, w: 687, h: 352, title: "It's AUTO man!"});
  s = ev(s, {ev: 'diagram', op: 'axes', ...axes});
  return s;
}

function lecar(): AppState {
  return ev(ev(opened(), add(0, steady)), add(9, periodic));
}

test('window 101 opens the view, empty, with the axes the core drew', () => {
  const s = opened();
  assert.equal(s.diagram.open, true);
  assert.equal(s.diagram.shown, true);
  assert.equal(pointCount(s.diagram.points), 0);
  assert.equal(s.diagram.axes?.xlabel, 'iapp');
  assert.equal(s.diagram.axes?.plot, 2);
});

test('add puts every point of every run in place, in order, labels included', () => {
  const s = ev(opened(), add(0, steady));
  const p = s.diagram.points;
  assert.equal(pointCount(p), 9);
  assert.deepEqual(p.pt, [1, 2, 3, 4, 5, 6, 7, 8, 9]);
  assert.deepEqual(p.ty, [2, 1, 1, 1, 2, 2, 2, 1, 1]);
  assert.deepEqual(p.nw, [1, 0, 0, 0, 0, 0, 0, 0, 0]);
  assert.deepEqual(p.c, [0, 20, 20, 20, 0, 0, 0, 20, 20]);
  assert.deepEqual(p.y2, p.y, 'no y2 in the event: y2 is y');
  assert.equal(p.x[5], 0.3);
  assert.deepEqual(s.diagram.labels, [{point: 0, lab: 1, sym: 'EP'}, {point: 4, lab: 2, sym: 'HB'},
    {point: 6, lab: 3, sym: 'HB'}, {point: 8, lab: 4, sym: 'EP'}]);
  assert.equal(s.diagram.events, 2);
});

test('a later add continues from what is held; y2 and null values are kept', () => {
  let s = ev(opened(), add(0, steady));
  s = ev(s, add(9, [{...periodic[0], y: [null, -0.17, -0.14]}]));
  const p = s.diagram.points;
  assert.equal(pointCount(p), 12);
  assert.ok(Number.isNaN(p.y[9]), 'null is NaN');
  assert.deepEqual(p.y2.slice(9), [-0.2, -0.23, -0.26]);
  assert.deepEqual(p.br.slice(8), [1, 2, 2, 2]);
  assert.deepEqual(p.nw.slice(9), [1, 0, 0]);
});

test('an add from before the end replaces what follows; reset keeps the first k points', () => {
  let s = lecar();
  s = ev(s, add(4, [{br: 1, pt: 5, ty: 2, d: 1, c: 0, lw: 1, x: [0.9], y: [0.9]}]));
  assert.equal(pointCount(s.diagram.points), 5);
  assert.deepEqual(s.diagram.labels.map(l => l.point), [0], 'labels past the replaced point go');
  s = lecar();
  s = ev(s, {ev: 'diagram', op: 'reset', keep: 3, ...axes, ymax: 0.5});
  assert.equal(pointCount(s.diagram.points), 3);
  assert.deepEqual(s.diagram.labels.map(l => l.lab), [1]);
  assert.equal(s.diagram.axes?.ymax, 0.5);
  s = ev(s, {ev: 'diagram', op: 'reset', keep: 0, ...axes});
  assert.equal(pointCount(s.diagram.points), 0);
  assert.equal(s.diagram.labels.length, 0);
});

test('an add that does not follow on marks the data out of step, until it is sent again', () => {
  let s = ev(opened(), add(0, steady));
  s = ev(s, add(20, periodic));
  assert.equal(s.diagram.outOfStep, true);
  assert.equal(pointCount(s.diagram.points), 9, 'nothing applied');
  s = ev(s, {ev: 'diagram', op: 'reset', keep: 0, ...axes});
  s = ev(s, add(0, steady));
  assert.equal(s.diagram.outOfStep, false);
});

test('a second create is a resize: the data stay; destroy empties the view', () => {
  let s = ev(lecar(), {ev: 'window', op: 'create', win: 101, w: 800, h: 400});
  assert.equal(pointCount(s.diagram.points), 14);
  s = ev(s, {ev: 'window', op: 'destroy', win: 101});
  assert.equal(s.diagram.open, false);
  assert.equal(pointCount(s.diagram.points), 0);
  assert.equal(s.diagram.axes, null);
});

test('state.auto opens the view for a page that connected later; its absence closes it', () => {
  const state = (auto?: object) => ({ev: 'state', pars: [], ics: [], bcs: [], rows: 0, menu: 0, win: 1,
    view: {win: 1, left: 0, right: 1, top: 0, bottom: 1, xlo: 0, xhi: 1, ylo: 0, yhi: 1, three: 0}, auto});
  let s = ev(initialState, state({x0: 70, y0: 26, wid: 603, hgt: 300, xmin: 0, xmax: 1, ymin: 0, ymax: 1}));
  assert.equal(s.diagram.open, true);
  assert.equal(s.diagram.axes, null, 'the data come with the redraw the session asks for');
  s = ev(s, state());
  assert.equal(s.diagram.open, false);
});

test('Back hides the panel, AUTO stays open', () => {
  let s = lecar();
  s = reduce(s, {type: 'diagram', action: {type: 'show', shown: false}});
  assert.equal(s.diagram.shown, false);
  assert.equal(s.diagram.open, true);
  assert.equal(pointCount(s.diagram.points), 14);
});

test('zoom and undo; the core moving its axes goes back to them, the zoom one undo away', () => {
  const zoom = {x: {min: 0.2, max: 0.3}, y: {min: -0.3, max: 0}};
  let s = reduce(lecar(), {type: 'diagram', action: {type: 'viewport', viewport: zoom, push: true}});
  assert.deepEqual(s.diagram.viewport, zoom);
  s = ev(s, {ev: 'diagram', op: 'axes', ...axes});
  assert.deepEqual(s.diagram.viewport, zoom, 'the same axes again (reDraw): the zoom stays');
  s = ev(s, {ev: 'diagram', op: 'axes', ...axes, xmax: 0.6});
  assert.deepEqual(s.diagram.viewport, {x: null, y: null});
  s = reduce(s, {type: 'diagram', action: {type: 'undoViewport'}});
  assert.deepEqual(s.diagram.viewport, zoom);
  s = reduce(s, {type: 'diagram', action: {type: 'undoViewport'}});
  assert.deepEqual(s.diagram.viewport, {x: null, y: null});
});

test('one curve per branch and stability run, each line starting where the run before ended', () => {
  const d = lecar().diagram, m = buildDiagramModel(d.points, d.labels, d.axes);
  const steadyCurves = m.curves.filter(c => c.branch === 1);
  assert.deepEqual(steadyCurves.map(c => [c.type, Array.from(c.idx)]), [
    [2, [0]], [1, [0, 1, 2, 3]], [2, [3, 4, 5, 6]], [1, [6, 7, 8]]]);
  assert.deepEqual(steadyCurves.map(c => c.dashed), [true, false, true, false], 'unstable dashed');
  assert.ok(steadyCurves[1].width > steadyCurves[2].width, 'stable thicker');
  assert.ok(steadyCurves.every(c => c.kind === 'steady' && c.which === 'y'));
  /* periodic: max and min, unstable then stable, the stable run starting at the last unstable point */
  const per = m.curves.filter(c => c.branch === 2);
  assert.deepEqual(per.map(c => [c.type, c.which, c.dashed]), [[4, 'y', true], [4, 'y2', true], [3, 'y', false],
    [3, 'y2', false]]);
  assert.deepEqual(Array.from(per[2].idx), [11, 12, 13]);
  assert.deepEqual(Array.from(per[3].ys), [-0.26, -0.3, -0.35]);
  assert.equal(m.labels.length, 5);
  assert.deepEqual(m.labels.find(l => l.lab === 5), {point: 13, lab: 5, sym: 'LP', x: 0.24, y: -0.05, y2: -0.35});
});

test('a periodic branch starts at the Hopf point it bifurcates from, max and min alike', () => {
  const d = lecar().diagram, m = buildDiagramModel(d.points, d.labels, d.axes);
  assert.deepEqual(m.hopf, [{point: 9, from: 4}]);
  const [max, min] = m.curves.filter(c => c.branch === 2 && c.type === 4);
  for (const c of [max, min]) {
    assert.equal(c.hopf, 4);
    assert.deepEqual([c.idx[0], c.xs[0], c.ys[0]], [4, 0.26, -0.2], 'the first vertex is the Hopf point');
    assert.equal(c.idx[1], 9, 'then the branch\'s first point');
  }
  assert.equal(min.ys[2], -0.23);
  assert.ok(m.curves.filter(c => c.branch === 1).every(c => c.hopf === -1));
});

test('no Hopf join when no Hopf label lies at the branch start', () => {
  const far: DiagramRun[] = [{...periodic[0], x: [0.1, 0.11, 0.12]}];
  let s = ev(ev(opened(), add(0, steady)), add(9, far));
  let m = buildDiagramModel(s.diagram.points, s.diagram.labels, s.diagram.axes);
  assert.deepEqual(m.hopf, []);
  assert.equal(m.curves.find(c => c.branch === 2)?.idx[0], 9);
  /* at the Hopf parameter but its orbit nowhere near the Hopf value */
  const off: DiagramRun[] = [{...periodic[0], y: [0.3, 0.31, 0.32], y2: [0.2, 0.19, 0.18]}];
  s = ev(ev(opened(), add(0, steady)), add(9, off));
  m = buildDiagramModel(s.diagram.points, s.diagram.labels, s.diagram.axes);
  assert.deepEqual(m.hopf, []);
  /* the nearer of two Hopf points */
  const d = lecar().diagram;
  assert.equal(hopfOf(d.points, d.labels, 9, {x: 10, y: 10}), 4);
});

test('the point under the pointer: the nearest, a labelled one among equals', () => {
  const d = lecar().diagram, m = buildDiagramModel(d.points, d.labels, d.axes);
  const f = {xmin: -0.2, xmax: 0.5, ymin: -0.5, ymax: 0.4, width: 700, height: 900};
  const px = (x: number) => (x - f.xmin) * 1000, py = (y: number) => (f.ymax - y) * 1000;
  /* the Hopf point is drawn three times (branch 1, and the start of both periodic lines) and branch 2's first point lies 0.1 px away */
  const h = nearestVertex(m, f, px(0.26), py(-0.2), 24)!;
  assert.equal(m.curves[h.curve].idx[h.index], 4);
  const q = nearestVertex(m, f, px(0.24), py(-0.35), 24)!;
  assert.equal(m.curves[q.curve].which, 'y2');
  assert.equal(m.curves[q.curve].idx[q.index], 13);
  assert.equal(nearestVertex(m, f, 0, 0, 5), null);
  const v = vertexOf(m, 13, true)!;
  assert.equal(m.curves[v.curve].which, 'y2');
  assert.equal(vertexOf(m, 3, false)?.curve, 1, 'a point is found on its own run, not the next one\'s line back');
});

test('stepping from label to label, and the readout of a point', () => {
  const d: DiagramState = lecar().diagram;
  assert.equal(stepLabel(d.labels, -1, 1), 0);
  assert.equal(stepLabel(d.labels, 0, 1), 4);
  assert.equal(stepLabel(d.labels, 13, 1), 0, 'wraps');
  assert.equal(stepLabel(d.labels, -1, -1), 13);
  assert.equal(stepLabel(d.labels, 4, -1), 0);
  const r = describePoint(d.points, d.labels, d.axes, 4);
  assert.equal(r.head, 'Branch 1, point 5');
  assert.equal(r.kind, 'unstable steady state');
  assert.equal(r.label, 'HB label 2 (Hopf)');
  /* T23: the point where the last run's branch ended says why */
  const e = describePoint(d.points, d.labels, d.axes, 0, {point: 0, text: 'parameter iapp reached Par Max (0.45)'});
  assert.equal(e.label, 'EP label 1 (End point: parameter iapp reached Par Max (0.45))');
  assert.equal(describePoint(d.points, d.labels, d.axes, 4, {point: 0, text: 'x'}).label, 'HB label 2 (Hopf)');
  assert.deepEqual(r.values, ['iapp = 0.26', 'V = -0.2']);
  const q = describePoint(d.points, d.labels, d.axes, 10);
  assert.equal(q.kind, 'unstable periodic orbit');
  assert.deepEqual(q.values, ['iapp = 0.259', 'V max = -0.17', 'V min = -0.23']);
  assert.equal(initialDiagram.open, false);
});

/* ---- T11b: autoinfo, the grab, the label a branch started from ---- */

const hbInfo = {point: 4, br: 1, pt: 5, type: 2, sym: 'HB', lab: 2, par: [{name: 'iapp', value: 0.26}, {name: 'phi', value: 0.2}],
  norm: 0.29, var: 'V', u: -0.2, per: 14.4, x: 0.26, y: -0.2, y2: -0.2};
const hbStab = {periodic: 0, circle: [[0.9, 0.42], [0.9, -0.42]], eig: [[6e-5, 0.435], [6e-5, -0.435]]};

test('autoinfo holds the strip and the circle; a new window or a closed one has neither', () => {
  let s = ev(lecar(), {ev: 'autoinfo', info: hbInfo, stab: hbStab});
  assert.deepEqual(s.diagram.info, hbInfo);
  assert.deepEqual(s.diagram.stab, hbStab);
  assert.equal(s.diagram.infoEvents, 1);
  s = ev(s, {ev: 'autoinfo', info: null, stab: hbStab});
  assert.equal(s.diagram.info, null);
  assert.equal(s.diagram.infoEvents, 2);
  s = ev(s, {ev: 'window', op: 'destroy', win: 101});
  assert.equal(s.diagram.stab, null);
  assert.equal(s.diagram.infoEvents, 2, 'the count goes on');
});

test('a grab ask on the diagram is the view\'s grab until the command ends, and shows the panel', () => {
  let s = lecar();
  s = reduce(s, {type: 'diagram', action: {type: 'show', shown: false}});
  s = ev(s, {ev: 'ask', id: 7, kind: 'grab', win: 101});
  assert.equal(s.diagram.grabbing, true);
  assert.equal(s.diagram.shown, true);
  assert.equal(s.pick, null, 'not a plot mode');
  s = reduce(s, {type: 'sent', cmd: {cmd: 'answer', id: 7, point: 3}});
  assert.equal(s.ask, null);
  assert.equal(s.diagram.grabbing, true, 'between two asks of the same grab');
  s = ev(s, {ev: 'idle'});
  assert.equal(s.diagram.grabbing, false);
  /* Axes/Zoom's box on the diagram is a plot mode of window 101 */
  s = ev(s, {ev: 'ask', id: 8, kind: 'rubber', win: 101, flag: 0});
  assert.equal(s.pick?.mode, 'box');
  assert.equal(s.pick?.win, 101);
  assert.equal(s.plots.active, initialState.plots.active, 'the plot windows are left alone');
});

test('grab keys: points one by one (the ends wrap to the first), ten, the ends, labels by Tab', () => {
  const d = lecar().diagram, n = pointCount(d.points); /* 14 points, labels at 0, 4, 6, 8, 13 */
  assert.equal(grabStep('ArrowRight', false, 3, n, d.labels), 4);
  assert.equal(grabStep(']', false, n - 1, n, d.labels), 0);
  assert.equal(grabStep('ArrowLeft', false, 0, n, d.labels), 0);
  assert.equal(grabStep('[', false, 5, n, d.labels), 4);
  assert.equal(grabStep('ArrowRight', false, -1, n, d.labels), 0, 'from no point: the first');
  assert.equal(grabStep('PageDown', false, 2, n, d.labels), 12);
  assert.equal(grabStep('PageDown', false, 9, n, d.labels), n - 1);
  assert.equal(grabStep('PageUp', false, 3, n, d.labels), 0);
  assert.equal(grabStep('Home', false, 7, n, d.labels), 0);
  assert.equal(grabStep('End', false, 7, n, d.labels), n - 1);
  assert.equal(grabStep('Tab', false, 0, n, d.labels), 4, 'Tab from the first point: the Hopf point');
  assert.equal(grabStep('Tab', true, 4, n, d.labels), 0);
  assert.equal(grabStep('Tab', false, 13, n, d.labels), 0, 'Tab wraps');
  assert.equal(grabStep('x', false, 3, n, d.labels), null);
  assert.equal(grabStep('Tab', false, 0, 0, []), null);
});

test('a periodic branch whose run says it started from the Hopf label joins that label, wherever it lies', () => {
  const tagged: DiagramRun[] = [{...periodic[0], from: 3, x: [0.44, 0.43, 0.42]}, periodic[1]];
  const s = ev(ev(opened(), add(0, steady)), add(9, tagged));
  assert.deepEqual(s.diagram.points.fr.slice(8, 12), [0, 3, 0, 0], 'on the run\'s first point only');
  const m = buildDiagramModel(s.diagram.points, s.diagram.labels, s.diagram.axes);
  assert.deepEqual(m.hopf, [{point: 9, from: 6}], 'label 3, the second Hopf point, not the nearer first');
  /* a run from a label that is not a Hopf point is not joined, even where one lies */
  const fromEp: DiagramRun[] = [{...periodic[0], from: 1}];
  const t = ev(ev(opened(), add(0, steady)), add(9, fromEp));
  assert.deepEqual(buildDiagramModel(t.diagram.points, t.diagram.labels, t.diagram.axes).hopf, []);
});

test('the strip in words and the circle: inside is stable, the eigenvalues listed', () => {
  const rows = infoRows(hbInfo as never);
  assert.deepEqual(rows.slice(0, 4), [['Branch', '1'], ['Point', '5'], ['Type', 'Unstable steady state'],
    ['Label', 'HB 2 (Hopf)']]);
  assert.deepEqual(rows.slice(4), [['iapp', '0.26'], ['phi', '0.2'], ['Norm', '0.29'], ['V', '-0.2']], 'no period for a steady state');
  assert.deepEqual(infoRows({...hbInfo, type: 4, sym: '', lab: 0} as never).slice(-1), [['Period', '14.4']]);
  const pts = circlePoints(hbStab as never);
  assert.deepEqual(pts.map(p => [p.inside, p.text]), [[true, '0.00006 + 0.435i'], [true, '0.00006 − 0.435i']]);
  const mult = circlePoints({periodic: 1, circle: [[1, 0], [2.5, 0], [0.1, -3]]} as never);
  assert.deepEqual(mult.map(p => [p.x, p.y, p.inside, p.text]), [[1, 0, true, '1'], [1.95, 0, false, '2.5'],
    [0.1, -1.95, false, '0.1 − 3i']]);
  assert.equal(stabilitySummary({periodic: 1, circle: [[1, 0], [2.5, 0]]} as never), '2 Floquet multipliers, 1 inside the unit circle');
  assert.equal(complexText(null, null), 'none (below the smallest number)');
});
