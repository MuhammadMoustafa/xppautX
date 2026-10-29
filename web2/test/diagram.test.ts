/* The AUTO diagram (docs/ui-v2.md T11a): the store built from `diagram`
   events (full and `add` increments), the curves by branch and stability,
   and the segment from a Hopf point to its periodic branch. */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {circlePoints, complexText, infoRows, stabComputed, stabilitySummary} from '../src/plot/autoInfo';
import {
  buildDiagramModel, describePoint, fitRanges, grabStep, hopfOf, nearestVertex, stepLabel, vertexOf,
  labelShape, labelTypes, symbolHelp,
} from '../src/plot/diagramModel';
import {pointLabel} from '../src/store/pointTypes';
import {activeView, initialDiagram, pointCount, type DiagramRun} from '../src/store/diagram';
import {initialState, reduce, type AppState} from '../src/store/state';
import {READY} from './hello';

test('equilibrium and cycle codim-2 labels have names, legend entries and marks', () => {
  const kinds = ['bogdanov_takens', 'cusp', 'zero_hopf', 'generalized_hopf', 'resonance_1_1', 'resonance_1_2', 'resonance_1_3', 'resonance_1_4', 'fold_flip', 'fold_neimark_sacker', 'flip_neimark_sacker', 'double_neimark_sacker'];
  assert.deepEqual(labelTypes(kinds.map(sym => ({sym}))).sort(), [...kinds].sort());
  assert.deepEqual(kinds.map(pointLabel), ['Bogdanov\u2013Takens', 'Cusp', 'Zero-Hopf (fold\u2013Hopf)', 'Generalized Hopf (Bautin)', 'Resonance 1:1', 'Resonance 1:2', 'Resonance 1:3', 'Resonance 1:4', 'Fold\u2013flip', 'Fold\u2013Neimark\u2013Sacker', 'Flip\u2013Neimark\u2013Sacker', 'Double Neimark\u2013Sacker']);
  assert.deepEqual(kinds.map(labelShape), ['diamond', 'triangle', 'star', 'circle', 'diamond', 'triangle', 'star', 'circle', 'square', 'triangle', 'star', 'diamond']);
  assert.match(symbolHelp('generalized_hopf'), /Lyapunov coefficient/);
});

const ev = (s: AppState, e: object) => reduce(s, {type: 'event', ev: e as never});
const axes = {xmin: -0.2, xmax: 0.5, ymin: -0.5, ymax: 0.4, x0: 70, y0: 26, wid: 603, hgt: 300, plot: 'maximum_and_minimum' as const,
  xlabel: 'iapp', ylabel: 'V'};
const line = (n: number, x0: number, dx: number, y0: number, dy: number) =>
  Array.from({length: n}, (_, i) => [x0 + i * dx, y0 + i * dy]);

/* a steady-state branch like lecar's: an end point, stable, unstable between two Hopf points, stable */
const steady: DiagramRun[] = [
  {br: 1, pt: 1, ty: 2, stable: false, periodic: false, d: 1, c: 'foreground', lw: 1, jac: 'difference', new: 1, x: [0.05], y: [-0.44], lab: [[0, 1, 'end']]},
  {br: 1, pt: 2, ty: 1, stable: true, periodic: false, d: 1, c: '#ff0000', lw: 2, jac: 'difference', x: line(3, 0.07, 0.05, -0.41, 0.05).map(p => p[0]),
    y: line(3, 0.07, 0.05, -0.41, 0.05).map(p => p[1])},
  {br: 1, pt: 5, ty: 2, stable: false, periodic: false, d: 1, c: 'foreground', lw: 1, jac: 'difference', x: [0.26, 0.3, 0.45], y: [-0.2, -0.1, 0.06],
    lab: [[0, 2, 'hopf'], [2, 3, 'hopf']]},
  {br: 1, pt: 8, ty: 1, stable: true, periodic: false, d: 1, c: '#ff0000', lw: 2, jac: 'difference', x: [0.47, 0.5], y: [0.07, 0.08], lab: [[1, 4, 'end']]},
];
/* the periodic branch from the first Hopf point: unstable, then stable, max and min */
const periodic: DiagramRun[] = [
  {br: 2, pt: 1, ty: 4, stable: false, periodic: true, d: 3, c: '#0000ff', lw: 1, jac: 'difference', new: 1, x: [0.2601, 0.259, 0.255], y: [-0.2, -0.17, -0.14],
    y2: [-0.2, -0.23, -0.26]},
  {br: 2, pt: 4, ty: 3, stable: true, periodic: true, d: 2, c: '#00e100', lw: 1, jac: 'difference', x: [0.25, 0.24], y: [-0.1, -0.05], y2: [-0.3, -0.35], lab: [[1, 5, 'fold']]},
];
const add = (from: number, runs: DiagramRun[]) => ({ev: 'auto.diagram', view: 'diagram_1', op: 'add', from, runs});

function opened(): AppState {
  let s = ev(READY, {ev: 'window.created', kind: 'diagram', win: 'diagram_1', w: 687, h: 352, title: "It's AUTO man!"});
  s = ev(s, {ev: 'auto.diagram', view: 'diagram_1', op: 'axes', ...axes});
  return s;
}

function lecar(): AppState {
  return ev(ev(opened(), add(0, steady)), add(9, periodic));
}

test('window 101 opens the view, empty, with the axes the core drew', () => {
  const s = opened();
  assert.equal(s.diagram.open, true);
  assert.equal(s.diagram.shown, true);
  assert.equal(pointCount(activeView(s.diagram).points), 0);
  assert.equal(activeView(s.diagram).axes?.xlabel, 'iapp');
  assert.equal(activeView(s.diagram).axes?.plot, 'maximum_and_minimum');
});

test('add puts every point of every run in place, in order, labels included', () => {
  const s = ev(opened(), add(0, steady));
  const p = activeView(s.diagram).points;
  assert.equal(pointCount(p), 9);
  assert.deepEqual([...p.pt], [1, 2, 3, 4, 5, 6, 7, 8, 9]);
  assert.deepEqual([...p.ty], [2, 1, 1, 1, 2, 2, 2, 1, 1]);
  assert.deepEqual([...p.nw], [1, 0, 0, 0, 0, 0, 0, 0, 0]);
  assert.deepEqual([...p.c], [0, 1, 1, 1, 0, 0, 0, 1, 1]); /* indices into the palette */
  assert.deepEqual(p.palette, ['foreground', '#ff0000']);
  assert.deepEqual(p.y2, p.y, 'no y2 in the event: y2 is y');
  assert.equal(p.x[5], 0.3);
  assert.deepEqual(activeView(s.diagram).labels, [{point: 0, lab: 1, sym: 'end'}, {point: 4, lab: 2, sym: 'hopf'},
    {point: 6, lab: 3, sym: 'hopf'}, {point: 8, lab: 4, sym: 'end'}]);
  assert.equal(s.diagram.events, 2);
});

test('a later add continues from what is held; y2 and null values are kept', () => {
  let s = ev(opened(), add(0, steady));
  s = ev(s, add(9, [{...periodic[0], y: [null, -0.17, -0.14]}]));
  const p = activeView(s.diagram).points;
  assert.equal(pointCount(p), 12);
  assert.ok(Number.isNaN(p.y[9]), 'null is NaN');
  assert.deepEqual([...p.y2.slice(9)], [-0.2, -0.23, -0.26]);
  assert.deepEqual([...p.br.slice(8)], [1, 2, 2, 2]);
  assert.deepEqual([...p.nw.slice(9)], [1, 0, 0]);
});

test('an add from before the end replaces what follows; reset keeps the first k points', () => {
  let s = lecar();
  s = ev(s, add(4, [{br: 1, pt: 5, ty: 2, stable: false, periodic: false, d: 1, c: 'foreground', lw: 1, jac: 'difference', x: [0.9], y: [0.9]}]));
  assert.equal(pointCount(activeView(s.diagram).points), 5);
  assert.deepEqual(activeView(s.diagram).labels.map(l => l.point), [0], 'labels past the replaced point go');
  s = lecar();
  s = ev(s, {ev: 'auto.diagram', view: 'diagram_1', op: 'reset', keep: 3, ...axes, ymax: 0.5});
  assert.equal(pointCount(activeView(s.diagram).points), 3);
  assert.deepEqual(activeView(s.diagram).labels.map(l => l.lab), [1]);
  assert.equal(activeView(s.diagram).axes?.ymax, 0.5);
  s = ev(s, {ev: 'auto.diagram', view: 'diagram_1', op: 'reset', keep: 0, ...axes});
  assert.equal(pointCount(activeView(s.diagram).points), 0);
  assert.equal(activeView(s.diagram).labels.length, 0);
});

test('an add that does not follow on marks the data out of step, until it is sent again', () => {
  let s = ev(opened(), add(0, steady));
  s = ev(s, add(20, periodic));
  assert.equal(s.diagram.outOfStep, true);
  assert.equal(pointCount(activeView(s.diagram).points), 9, 'nothing applied');
  s = ev(s, {ev: 'auto.diagram', view: 'diagram_1', op: 'reset', keep: 0, ...axes});
  s = ev(s, add(0, steady));
  assert.equal(s.diagram.outOfStep, false);
});

test('a second create is a resize: the data stay; destroy empties the view', () => {
  let s = ev(lecar(), {ev: 'window.created', kind: 'diagram', win: 'diagram_1', w: 800, h: 400});
  assert.equal(pointCount(activeView(s.diagram).points), 14);
  s = ev(s, {ev: 'window.destroyed', kind: 'diagram', win: 'diagram_1'});
  assert.equal(s.diagram.open, false);
  assert.equal(pointCount(activeView(s.diagram).points), 0);
  assert.equal(activeView(s.diagram).axes, null);
});

test('state.auto opens the view for a page that connected later; its absence closes it', () => {
  const state = (auto?: object) => ({ev: 'session.state', sliders: [], pars: [], ics: [], bcs: [], rows: 0, menu: 0, win: 'graph_1',
    view: {win: 'graph_1', left: 0, right: 1, top: 0, bottom: 1, xlo: 0, xhi: 1, ylo: 0, yhi: 1, three: 0}, auto});
  let s = ev(READY, state({x0: 70, y0: 26, wid: 603, hgt: 300, xmin: 0, xmax: 1, ymin: 0, ymax: 1}));
  assert.equal(s.diagram.open, true);
  assert.equal(activeView(s.diagram).axes, null, 'the data come with the redraw the session asks for');
  s = ev(s, state());
  assert.equal(s.diagram.open, false);
});

test('Back hides the panel, AUTO stays open', () => {
  let s = lecar();
  s = reduce(s, {type: 'diagram', action: {type: 'show', shown: false}});
  assert.equal(s.diagram.shown, false);
  assert.equal(s.diagram.open, true);
  assert.equal(pointCount(activeView(s.diagram).points), 14);
});

test('zoom, no history (GitHub #110): the core moving its axes goes back to them', () => {
  const zoom = {x: {min: 0.2, max: 0.3}, y: {min: -0.3, max: 0}};
  let s = reduce(lecar(), {type: 'diagram', action: {type: 'viewport', view: 0, viewport: zoom}});
  assert.deepEqual(activeView(s.diagram).viewport, zoom);
  s = ev(s, {ev: 'auto.diagram', view: 'diagram_1', op: 'axes', ...axes});
  assert.deepEqual(activeView(s.diagram).viewport, zoom, 'the same axes again (reDraw): the zoom stays');
  s = ev(s, {ev: 'auto.diagram', view: 'diagram_1', op: 'axes', ...axes, xmax: 0.6});
  assert.deepEqual(activeView(s.diagram).viewport, {x: null, y: null});
});

test('one curve per branch and stability run, each line starting where the run before ended', () => {
  const d = activeView(lecar().diagram), m = buildDiagramModel(d.points, d.labels, d.axes);
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
  assert.deepEqual(m.labels.find(l => l.lab === 5), {point: 13, lab: 5, sym: 'fold', x: 0.24, y: -0.05, y2: -0.35});
});

test('a periodic branch starts at the Hopf point it bifurcates from, max and min alike', () => {
  const d = activeView(lecar().diagram), m = buildDiagramModel(d.points, d.labels, d.axes);
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
  let m = buildDiagramModel(activeView(s.diagram).points, activeView(s.diagram).labels, activeView(s.diagram).axes);
  assert.deepEqual(m.hopf, []);
  assert.equal(m.curves.find(c => c.branch === 2)?.idx[0], 9);
  /* at the Hopf parameter but its orbit nowhere near the Hopf value */
  const off: DiagramRun[] = [{...periodic[0], y: [0.3, 0.31, 0.32], y2: [0.2, 0.19, 0.18]}];
  s = ev(ev(opened(), add(0, steady)), add(9, off));
  m = buildDiagramModel(activeView(s.diagram).points, activeView(s.diagram).labels, activeView(s.diagram).axes);
  assert.deepEqual(m.hopf, []);
  /* the nearer of two Hopf points */
  const d = activeView(lecar().diagram);
  assert.equal(hopfOf(d.points, d.labels, 9, {x: 10, y: 10}), 4);
});

test('the point under the pointer: the nearest, a labelled one among equals', () => {
  const d = activeView(lecar().diagram), m = buildDiagramModel(d.points, d.labels, d.axes);
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
  const d = activeView(lecar().diagram);
  assert.equal(stepLabel(d.labels, -1, 1), 0);
  assert.equal(stepLabel(d.labels, 0, 1), 4);
  assert.equal(stepLabel(d.labels, 13, 1), 0, 'wraps');
  assert.equal(stepLabel(d.labels, -1, -1), 13);
  assert.equal(stepLabel(d.labels, 4, -1), 0);
  const r = describePoint(d.points, d.labels, d.axes, 4);
  assert.equal(r.head, 'Branch 1, point 5');
  assert.equal(r.kind, 'unstable steady state');
  assert.equal(r.label, 'Andronov\u2013Hopf label 2');
  /* T23: the point where the last run's branch ended says why */
  const e = describePoint(d.points, d.labels, d.axes, 0, {point: 0, text: 'parameter iapp reached Par Max (0.45)'});
  assert.equal(e.label, 'End of branch label 1 (parameter iapp reached Par Max (0.45))');
  assert.equal(describePoint(d.points, d.labels, d.axes, 4, {point: 0, text: 'x'}).label, 'Andronov\u2013Hopf label 2');
  assert.deepEqual(r.values, ['iapp = 0.26', 'V = -0.2']);
  const q = describePoint(d.points, d.labels, d.axes, 10);
  assert.equal(q.kind, 'unstable periodic orbit');
  assert.deepEqual(q.values, ['iapp = 0.259', 'V max = -0.17', 'V min = -0.23']);
  assert.equal(initialDiagram.open, false);
});

/* ---- T11b: autoinfo, the grab, the label a branch started from ---- */

const hbInfo = {point: 4, br: 1, pt: 5, type: 2, stable: false, periodic: false, sym: 'hopf', lab: 2, jac: 'difference', par: [{name: 'iapp', value: 0.26}, {name: 'phi', value: 0.2}],
  norm: 0.29, var: 'V', u: -0.2, per: 14.4, x: 0.26, y: -0.2, y2: -0.2};
const hbStab = {periodic: 0, circle: [[0.9, 0.42], [0.9, -0.42]], eig: [[6e-5, 0.435], [6e-5, -0.435]]};

test('autoinfo holds the strip and the circle; a new window or a closed one has neither', () => {
  let s = ev(lecar(), {ev: 'auto.info', info: hbInfo, stab: hbStab});
  assert.deepEqual(s.diagram.info, hbInfo);
  assert.deepEqual(s.diagram.stab, hbStab);
  assert.equal(s.diagram.infoEvents, 1);
  s = ev(s, {ev: 'auto.info', info: null, stab: hbStab});
  assert.equal(s.diagram.info, null);
  assert.equal(s.diagram.infoEvents, 2);
  s = ev(s, {ev: 'window.destroyed', kind: 'diagram', win: 'diagram_1'});
  assert.equal(s.diagram.stab, null);
  assert.equal(s.diagram.infoEvents, 2, 'the count goes on');
});

test('a grab ask on the diagram is the view\'s grab until the command ends, and shows the panel', () => {
  let s = lecar();
  s = reduce(s, {type: 'diagram', action: {type: 'show', shown: false}});
  s = ev(s, {ev: 'ask', id: 7, kind: 'grab', win: 'diagram_1'});
  assert.equal(s.diagram.grabbing, true);
  assert.equal(s.diagram.shown, true);
  assert.equal(s.pick, null, 'not a plot mode');
  s = reduce(s, {type: 'sent', cmd: {cmd: 'answer', id: 7, point: 3}});
  assert.equal(s.ask, null);
  assert.equal(s.diagram.grabbing, true, 'between two asks of the same grab');
  s = ev(s, {ev: 'done', status: 'ok'});
  assert.equal(s.diagram.grabbing, false);
  /* Axes/Zoom's box on the diagram is a plot mode of window 101 */
  s = ev(s, {ev: 'ask', id: 8, kind: 'rubber', win: 'diagram_1', flag: 0});
  assert.equal(s.pick?.mode, 'box');
  assert.equal(s.pick?.win, 'diagram_1');
  assert.equal(s.plots.active, initialState.plots.active, 'the plot windows are left alone');
});

test('grab keys: points one by one (the ends wrap to the first), ten, the ends, labels by Tab', () => {
  const d = activeView(lecar().diagram), n = pointCount(d.points); /* 14 points, labels at 0, 4, 6, 8, 13 */
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
  assert.deepEqual([...activeView(s.diagram).points.fr.slice(8, 12)], [0, 3, 0, 0], 'on the run\'s first point only');
  const m = buildDiagramModel(activeView(s.diagram).points, activeView(s.diagram).labels, activeView(s.diagram).axes);
  assert.deepEqual(m.hopf, [{point: 9, from: 6}], 'label 3, the second Hopf point, not the nearer first');
  /* a run from a label that is not a Hopf point is not joined, even where one lies */
  const fromEp: DiagramRun[] = [{...periodic[0], from: 1}];
  const t = ev(ev(opened(), add(0, steady)), add(9, fromEp));
  assert.deepEqual(buildDiagramModel(activeView(t.diagram).points, activeView(t.diagram).labels, activeView(t.diagram).axes).hopf, []);
});

test('the strip in words and the circle: inside is stable, the eigenvalues listed', () => {
  const rows = infoRows(hbInfo as never, axes);
  assert.deepEqual(rows.slice(0, 4), [['Branch', '1'], ['Point', '5'], ['Type', 'Unstable steady state'],
    ['Label', 'AH 2 (Andronov\u2013Hopf)']]);
  assert.deepEqual(rows.slice(4, -1), [['iapp', '0.26'], ['V', '-0.2'], ['Norm', '0.29'], ['phi', '0.2']],
    'the axes (iapp, V), then Norm (no period for a steady state), then the second parameter, last');
  assert.deepEqual(rows.at(-1), ['Jacobian', 'difference'], 'and the Jacobian its run was computed with');
  assert.deepEqual(infoRows({...hbInfo, jac: 'exact'} as never, axes).at(-1), ['Jacobian', 'exact']);
  const pts = circlePoints(hbStab as never);
  assert.deepEqual(pts.map(p => [p.inside, p.text]), [[true, '0.00006 + 0.435i'], [true, '0.00006 − 0.435i']]);
  const mult = circlePoints({periodic: 1, circle: [[1, 0], [2.5, 0], [0.1, -3]]} as never);
  assert.deepEqual(mult.map(p => [p.x, p.y, p.inside, p.text]), [[1, 0, true, '1'], [1.95, 0, false, '2.5'],
    [0.1, -1.95, false, '0.1 − 3i']]);
  assert.equal(stabilitySummary({periodic: 1, circle: [[1, 0], [2.5, 0]]} as never), '2 Floquet multipliers, 1 inside the unit circle');
  assert.equal(complexText(null, null), 'none (below the smallest number)');
});

/* T28: the strip in order of importance -- the axes' quantities, then Norm/the plotted variable/the
   period not already shown as an axis, then the remaining parameters (a second one last) */
test('infoRows orders the axes first, then Norm and the period, then the remaining parameters', () => {
  /* a one-parameter diagram, Iapp against V (maximum_and_minimum: the y axis is the variable) */
  assert.deepEqual(infoRows(hbInfo as never, axes).slice(4, -1),
    [['iapp', '0.26'], ['V', '-0.2'], ['Norm', '0.29'], ['phi', '0.2']]);
  /* a periodic orbit: the period is not an axis either, so it comes before the remaining parameter */
  assert.deepEqual(infoRows({...hbInfo, type: 4, periodic: true} as never, axes).slice(4, -1),
    [['iapp', '0.26'], ['V', '-0.2'], ['Norm', '0.29'], ['Period', '14.4'], ['phi', '0.2']]);
  /* a Norm y axis (norm): Norm is the axis, the variable falls back with the remaining parameter */
  assert.deepEqual(infoRows(hbInfo as never, {...axes, plot: 'norm'}).slice(4, -1),
    [['iapp', '0.26'], ['Norm', '0.29'], ['V', '-0.2'], ['phi', '0.2']]);
  /* a two-parameter diagram (two_parameter): both parameters in axis order, nothing left over for them */
  assert.deepEqual(infoRows(hbInfo as never, {...axes, plot: 'two_parameter', ylabel: 'phi'}).slice(4, -1),
    [['iapp', '0.26'], ['phi', '0.2'], ['Norm', '0.29'], ['V', '-0.2']]);
  /* no axes data at all: still nothing dropped, the main parameter first */
  assert.deepEqual(infoRows(hbInfo as never, null).slice(4, -1),
    [['iapp', '0.26'], ['Norm', '0.29'], ['V', '-0.2'], ['phi', '0.2']]);
});

test("a first point's circle, all zeros, is not computed: no points, no zeros listed as eigenvalues (T25)", () => {
  const zeros = {periodic: 0, circle: [[0, 0], [0, 0], [0, 0]], eig: [[null, null], [null, null], [null, null]]};
  assert.equal(stabComputed(zeros as never), false);
  assert.deepEqual(circlePoints(zeros as never), []);
  assert.equal(stabilitySummary(zeros as never), '3 eigenvalues, not computed at this point');
  assert.equal(stabComputed({periodic: 0, circle: [[0, 0], [0.5, 0]]} as never), true);
  assert.equal(stabComputed({periodic: 0, circle: [[null, null]]} as never), true, 'not finite is a value, not "not computed"');
});

/* T30: the corner Fit (and the AUTO tools' own Fit) fits the view to the
   branches shown, client-side; the earlier ones Clear hid never widen it. */
test('fitRanges: every plotted point lies in range, with a small margin', () => {
  const d = activeView(lecar().diagram), m = buildDiagramModel(d.points, d.labels, d.axes);
  const r = fitRanges(m);
  let xmin = Infinity, xmax = -Infinity, ymin = Infinity, ymax = -Infinity;
  for (const c of m.curves) {
    for (const x of c.xs) { if (x < xmin) xmin = x; if (x > xmax) xmax = x; }
    for (const y of c.ys) { if (y < ymin) ymin = y; if (y > ymax) ymax = y; }
  }
  assert.ok(r.x.min <= xmin && r.x.max >= xmax, 'every x in view');
  assert.ok(r.y.min <= ymin && r.y.max >= ymax, 'every y in view');
  /* a small margin: more than the bare data, not a wide zoomed-out view */
  const xspan = xmax - xmin, yspan = ymax - ymin;
  assert.ok(r.x.max - r.x.min > xspan && r.x.max - r.x.min < xspan * 1.2, 'x: a small margin');
  assert.ok(r.y.max - r.y.min > yspan && r.y.max - r.y.min < yspan * 1.2, 'y: a small margin');
});

test('fitRanges: earlier branches Clear hid do not widen the fit', () => {
  const d = activeView(lecar().diagram);
  const shown = buildDiagramModel(d.points, d.labels, d.axes); /* Earlier branches on: everything */
  const hidden = buildDiagramModel(d.points, d.labels, d.axes, 9); /* Clear at point 9: the steady branch hidden */
  const all = fitRanges(shown), justPeriodic = fitRanges(hidden);
  assert.ok(all.x.min < 0.05, 'the steady branch widens the fit when shown');
  assert.ok(justPeriodic.x.min > 0.2, 'hidden, the periodic branch alone is a tighter fit');
  assert.ok(justPeriodic.x.min >= all.x.min && justPeriodic.x.max <= all.x.max);
});

test('fitRanges: no branches yet is [0, 1], not NaN or an inverted range', () => {
  const r = fitRanges(buildDiagramModel(activeView(opened().diagram).points, [], null));
  assert.deepEqual(r, {x: {min: 0, max: 1}, y: {min: 0, max: 1}});
});

test("autoview (W65): the branches Clear hid, whether they are shown and the zoom are the core's", () => {
  let s = lecar();
  const n = activeView(s.diagram).points.x.length;
  s = reduce(s, {type: 'event', ev: {ev: 'auto.view', earlier: n, show: 1, active: 'diagram_1', views: [{name:'diagram_1',zoom: {x: [0, 1], y: null}}]} as never});
  assert.equal(s.diagram.earlier, n);
  assert.equal(s.diagram.showEarlier, true);
  assert.deepEqual(activeView(s.diagram).viewport, {x: {min: 0, max: 1}, y: null});
  s = reduce(s, {type: 'event', ev: {ev: 'auto.view', earlier: n, show: undefined, active: 'diagram_1', views: [{name:'diagram_1'}]} as never});
  assert.equal(s.diagram.showEarlier, true, "without show and zoom (a change of the user's on its way) they stay");
  assert.deepEqual(activeView(s.diagram).viewport, {x: {min: 0, max: 1}, y: null});
  s = reduce(s, {type: 'event', ev: {ev: 'auto.view', earlier: 0, show: 0, active: 'diagram_1', views: [{name:'diagram_1',zoom: {x: null, y: null}}]} as never});
  assert.equal(s.diagram.earlier, 0);
  assert.equal(s.diagram.showEarlier, false);
  assert.deepEqual(activeView(s.diagram).viewport, {x: null, y: null});
});

test("views (W50): each view its own axes, points and zoom; `views` resizes; the active one is the core's", () => {
  let s = lecar();
  const one = activeView(s.diagram);
  s = ev(s, {ev: 'auto.diagram', op: 'views', n: 2, names: ['diagram_1', 'diagram_2']});
  assert.equal(s.diagram.views.length, 2);
  assert.equal(s.diagram.views[0], one, 'view 0 untouched');
  assert.equal(pointCount(s.diagram.views[1].points), 0, 'a new view starts empty');
  s = ev(s, {ev: 'auto.diagram', view: 'diagram_2', op: 'reset', keep: 0, ...axes, plot: 'norm', ylabel: 'Norm'});
  s = ev(s, {ev: 'auto.diagram', view: 'diagram_2', op: 'add', from: 0, runs: [{br: 1, pt: 1, ty: 1, stable: true, periodic: false, d: 1, c: '#ff0000', lw: 2, x: [0.1, 0.2], y: [1, 2]}]});
  assert.equal(s.diagram.views[1].axes?.ylabel, 'Norm');
  assert.equal(pointCount(s.diagram.views[1].points), 2);
  assert.equal(s.diagram.views[0], one, "another view's events leave view 0 alone");
  s = reduce(s, {type: 'event', ev: {ev: 'auto.view', earlier: 0, show: 0, active: 'diagram_2',
    views: [{name:'diagram_1',zoom: {x: null, y: null}}, {name:'diagram_2',zoom: {x: [0, 0.5], y: null}}]} as never});
  assert.equal(s.diagram.active, 1);
  assert.equal(activeView(s.diagram), s.diagram.views[1]);
  assert.deepEqual(s.diagram.views[1].viewport, {x: {min: 0, max: 0.5}, y: null});
  assert.deepEqual(s.diagram.views[0].viewport, {x: null, y: null});
  s = reduce(s, {type: 'diagram', action: {type: 'activate', view: 0}});
  assert.equal(s.diagram.active, 0, 'a click makes a view active at once');
  s = ev(s, {ev: 'auto.diagram', op: 'views', n: 1, names: ['diagram_1']});
  assert.equal(s.diagram.views.length, 1);
  assert.equal(s.diagram.views[0], one);
});

test('adds after the last point fill the same buffers; an older state keeps its own points', () => {
  let s = ev(opened(), add(0, steady));
  const before = activeView(s.diagram).points;
  s = ev(s, add(9, [{...periodic[0]}]));
  const after = activeView(s.diagram).points;
  assert.equal(after.buffers, before.buffers, 'in place: no copy');
  assert.equal(pointCount(before), 9, 'the older state still holds 9');
  const again = ev(s, add(5, [{...periodic[0]}]));
  assert.notEqual(activeView(again.diagram).points.buffers, after.buffers, 'an add before the end gets new buffers');
  assert.equal(after.br[10], 2, 'and leaves the points the state before shows as they were');
});
