import assert from 'node:assert/strict';
import {test} from 'node:test';
import {Session} from '../src/session';
import {reduce, initialState, noIdle} from '../src/store/state';
import type {Command} from '../src/protocol/types';
import {HELLO} from './hello';

function connected() {
  const sent: Command[] = [];
  const session = new Session({send: command => sent.push(command), open: () => {}, close: () => {}});
  session.store.dispatch({type: 'event', ev: HELLO});
  sent.length = 0;
  return {session, sent};
}

test('the recording and the player send the recording registry commands', () => {
  const {session, sent} = connected();
  session.startRecording();
  session.recordNote('First run.');
  session.stopRecording();
  session.play('start');
  session.play('pause');
  session.play('step');
  session.playSpeed(2);
  session.playFrom(3, true);
  session.playNote(1, 'a note');
  session.playClose();
  assert.deepEqual(sent, [{cmd: 'recording.start'}, {cmd: 'recording.add_note', note: 'First run.'}, {cmd: 'recording.stop'},
    {cmd: 'recording.play'}, {cmd: 'recording.pause'}, {cmd: 'recording.step'}, {cmd: 'recording.set_speed', speed: 2},
    {cmd: 'recording.seek', step: 3, play: true}, {cmd: 'recording.set_step_note', step: 1, note: 'a note'}, {cmd: 'recording.close'}]);
});

test('opening a model, a recent model, an example and the source use the session registry commands', () => {
  const {session, sent} = connected();
  session.openModel();
  session.openRecent(2);
  session.openExample('lecar.odex');
  session.selectTextTab('source');
  session.runAction(4);
  session.userButton(1);
  assert.deepEqual(sent, [{cmd: 'key', menu: 'file', item: 'openmodel'}, {cmd: 'session.open_model', recent: 2},
    {cmd: 'session.open_model', example: 'lecar.odex'}, {cmd: 'session.get_source'}, {cmd: 'tool.run_source_action', index: 4},
    {cmd: 'tool.run_user_button', index: 1}]);
});

test('the keymap is replaced in its parts, and leaving is session.quit with its save', () => {
  const {session, sent} = connected();
  session.setKeymap({preset: 'xppaut', pinned: ['reload'], bindings: {calculator: ['Ctrl+B']}});
  session.resetKeymap();
  assert.deepEqual(sent, [{cmd: 'keymap.set', bindings: {calculator: ['Ctrl+B']}, pinned: ['reload'], preset: 'xppaut'}, {cmd: 'keymap.reset'}]);
});

test('a control answered at once settles nothing: its done is counted, the run\'s own done is the idle', () => {
  let s = reduce(initialState, {type: 'sent', cmd: {cmd: 'run.initial'}});
  assert.equal(s.busy, true);
  s = reduce(s, {type: 'sent', cmd: {cmd: 'recording.pause'}});
  assert.equal(noIdle({cmd: 'recording.pause'}), true);
  s = reduce(s, {type: 'event', ev: {ev: 'done', status: 'ok', result: {paused: true}}});
  assert.equal(s.busy, true, 'the pause is answered while the run goes on');
  s = reduce(s, {type: 'event', ev: {ev: 'done', status: 'ok', result: {}}});
  assert.equal(s.busy, false, 'the run\'s own done settles it');
});
