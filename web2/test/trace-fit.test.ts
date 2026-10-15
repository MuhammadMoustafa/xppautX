import assert from 'node:assert/strict';
import {test} from 'node:test';
import {visibleRanges, type PlotModel, type CurveData} from '../src/plot/model';
import {nearest3d} from '../src/plot/render3d';
import {nearestPoint, nearestTrace} from '../src/plot/nearest';

test('Fit includes visible historical trajectories and ignores hidden and nonfinite points', () => {
  const curve = (xs: number[], ys: number[], visible = true): CurveData => ({label:'trace', xName:'T', yName:'X',
    color:0, line:true, radius:0, row0:0, xs:new Float32Array(xs), ys:new Float32Array(ys), visible});
  const model = (curves: CurveData[]): PlotModel => ({mode:1, curves, xLabel:'T', yLabel:'X', t:null, xRange:null, yRange:null});
  assert.deepEqual(visibleRanges([model([curve([0, 1], [2, 3])]),
    model([curve([-4, 5, NaN], [-10, 9, NaN]), curve([-100, 100], [-100, 100], false)])]),
  {x:{min:-4,max:5}, y:{min:-10,max:9}});
});

test('3D hover uses the rendered projection and skips hidden traces and clipped points', () => {
  const model = {box:[{x:-1,y:-1},{x:1,y:1}], labels:{x:'x',y:'y',z:'z'}, curves:[
    {label:'hidden', color:0, line:true, radius:0, row0:0, points:[{x:0,y:0}], visible:false},
    {label:'visible', color:1, line:true, radius:0, row0:0, points:[null,{x:.5,y:.5}]}]};
  assert.equal(nearest3d(model, 200, 200, 138, 62), 1);
  assert.equal(nearest3d(model, 200, 200, 100, 100), null);
});

test('trace hover finds a sparse line between samples, without crossing nonfinite gaps', () => {
  const curve: CurveData = {label:'line', xName:'X', yName:'Y', color:0, line:true, radius:0, row0:0,
    xs:new Float32Array([0, 100]), ys:new Float32Array([0, 100])};
  const frame = {xmin:0,xmax:100,ymin:0,ymax:100,width:100,height:100};
  assert.equal(nearestPoint([curve], [true], frame, 50, 50, 5), null);
  assert.equal(nearestPoint([curve], [true], frame, 50, 50, 5, true)?.curve, 0);
  curve.xs = new Float32Array([0, NaN, 100]); curve.ys = new Float32Array([0, NaN, 100]);
  assert.equal(nearestPoint([curve], [true], frame, 50, 50, 5, true), null);
});

test('hover over a trace and an identical older run names the current trace', () => {
  const curve: CurveData = {label:'line', xName:'X', yName:'Y', color:0, line:true, radius:0, row0:0,
    xs:new Float32Array([0, 1, 2]), ys:new Float32Array([0, 0.7, 0.3])};
  const frame = {xmin:0, xmax:2, ymin:0, ymax:1, width:400, height:200};
  const px = 36.1, py = 177.1;
  const current = nearestPoint([curve], [true], frame, px, py, 24, true);
  assert.ok(current);
  // the case W383 found: the current distance, squared again as the run's bound, lets the run through
  assert.ok(nearestPoint([curve], [true], frame, px, py, current.dist, true));
  assert.equal(nearestTrace([curve], [true], [[curve]], frame, px, py, 24)?.run, -1);
});

