/* The autoX diagram's live points reach the store in batches (W676): a run sends them faster than the page can draw them, and a
   draw per event left the page minutes behind the core. */
import assert from 'node:assert/strict';
import {mock, test} from 'node:test';
import {Session} from '../src/session';
import type {XppEvent} from '../src/protocol/types';
import {activeView, pointCount, type DiagramRun} from '../src/store/diagram';
import {READY} from './hello';

const run = (x: number): DiagramRun => ({br: 1, pt: 1, ty: 1, stable: true, periodic: false, d: 1, c: 'foreground', lw: 1, jac: 'difference',
  new: 1, x: [x], y: [x]});
const add = (from: number, x: number) => ({ev: 'autox.diagram', view: 'diagram_1', op: 'add', from, runs: [run(x)]}) as unknown as XppEvent;
const jobs = (running: number) => ({ev: 'autox.jobs', running, queued: 0, done: 0, jobs: []}) as unknown as XppEvent;

function session(): {s: Session; feed: (ev: XppEvent) => void; points: () => number; notified: () => number} {
  let feed: (ev: XppEvent) => void = () => {};
  const s = new Session({send: () => {}, close: () => {}, open: on => { feed = on; }});
  s.start();
  feed({ev: 'window.created', kind: 'diagram', win: 'diagram_1', w: 687, h: 352, title: 'autoX'} as unknown as XppEvent);
  let n = 0;
  s.store.subscribe(() => n++);
  return {s, feed, points: () => pointCount(activeView(s.store.getState().diagram).points), notified: () => n};
}

test('W676: live points and job progress wait, then go in as one batch with one notification', () => {
  mock.timers.enable({apis: ['setTimeout']});
  try {
    const {feed, points, notified} = session();
    void READY;
    const before = notified();
    for (let i = 0; i < 5; i++) {
      feed(add(i, i));
      feed(jobs(2));
    }
    assert.equal(points(), 0, 'nothing is in the store before the wait is over');
    mock.timers.tick(199);
    assert.equal(points(), 0);
    mock.timers.tick(1);
    assert.equal(points(), 5, 'every point arrived, none dropped');
    assert.equal(notified() - before, 1, 'the page was told once');
  } finally {
    mock.timers.reset();
  }
});

test('W676: any other event goes in after the points before it, at once', () => {
  mock.timers.enable({apis: ['setTimeout']});
  try {
    const {s, feed, points} = session();
    feed(add(0, 0));
    feed(add(1, 1));
    assert.equal(points(), 0);
    feed({ev: 'session.ping'} as unknown as XppEvent);
    assert.equal(points(), 2, 'the points that came before it are in');
    feed(add(2, 2));
    feed({ev: 'autox.diagram', view: 'diagram_1', op: 'reset', keep: 0, xmin: 0, xmax: 1, ymin: 0, ymax: 1, x0: 0, y0: 0, wid: 1, hgt: 1,
      plot: 'maximum_and_minimum', xlabel: 'a', ylabel: 'b'} as unknown as XppEvent);
    assert.equal(points(), 0, 'a reset after a held point empties what that point was added to');
    assert.equal(s.store.getState().diagram.events > 0, true);
  } finally {
    mock.timers.reset();
  }
});
