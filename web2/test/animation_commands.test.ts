import assert from 'node:assert/strict';
import {test} from 'node:test';
import {Session} from '../src/session';
import type {Command} from '../src/protocol/types';
import {HELLO} from './hello';

function connected() {
  const sent: Command[] = [];
  const session = new Session({send: command => sent.push(command), open: () => {}, close: () => {}});
  session.store.dispatch({type: 'event', ev: HELLO});
  return {session, sent};
}

test('a grab drag is one animation.grab_point in the animation\'s own coordinates', () => {
  const {session, sent} = connected();
  session.aniGrabPoint([0.5, 0.25], [1, 0], [-1, 10, 3, 14]);
  assert.deepEqual(sent, [{cmd: 'animation.grab_point', x: 1, y: 11, to_x: 3, to_y: 10}]);
});

test('the kinescope buttons and the array plot scroll send registry commands', () => {
  const {session, sent} = connected();
  session.kinescopeCapture();
  session.kinescopePlay();
  session.kinescopeReset();
  session.store.dispatch({type: 'event', ev: {ev: 'aplot', title: 'V0..1', nx: 2, ny: 5, first_row: 10, values: [], colormap: 'jet', ramp: [], zmin: 0, zmax: 1, tlo: 0, thi: 1}});
  for (const dy of [3, -4, 20]) {
    session.store.dispatch({type: 'event', ev: {ev: 'idle'}});
    session.aplotScroll(dy);
  }
  assert.deepEqual(sent, [{cmd: 'kinescope.capture'}, {cmd: 'kinescope.play'}, {cmd: 'kinescope.reset'},
    {cmd: 'array_plot.scroll', first_row: 7}, {cmd: 'array_plot.scroll', first_row: 14}, {cmd: 'array_plot.scroll', first_row: 0}]);
});
