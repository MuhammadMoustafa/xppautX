/* The player (W59b, docs/protocol.md "Playing a recording"): the store's
   slice from the player, press and state events, and what the page shows
   from it (the caption, the keycaps, the lit menu item and button, a
   dialog's answer, the progress segments), without a browser (npm test). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import type {PlayerEvent, PressEvent, StateEvent} from '../src/protocol/types';
import {
  answerFill, answerTexts, caption, initialPlayer, keycaps, litAskKey, litButton, litMenuKey, reducePlayer, segments,
  type PlayerState,
} from '../src/store/player';
import {reduce} from '../src/store/state';
import {READY} from './hello';

const PLAYER: PlayerEvent = {
  ev: 'player', file: '/tmp/lecar.recx', model: 'lecar.ode', intact: true,
  steps: [
    {note: 'The cell fires once.', step: 'Initialconds → Go', keys: ['i', 'g']},
    {note: '', step: 'Set iapp = 0.08', cmd: {cmd: 'set', kind: 'par', name: 'iapp', value: 0.08}},
    {note: '', step: 'nUmerics → Total', keys: ['t'], answers: ['1e7']},
    {note: '', step: 'Initialconds → Go', keys: ['i', 'g'], button: 'Integrate'},
    {note: '', step: 'Zoom window 1', cmd: {cmd: 'display', win: 1}, view: true},
    {note: '', step: 'AUTO → Run', win: 'auto', keys: ['r', 's'], button: 'run'},
    {note: '', step: 'File → Read set', keys: ['r'], answers: [{file: 'x.set'}, null, ['a', 'b']]},
  ],
};

const press = (step: number, what: PressEvent['what'], index: number, ms = 600): PressEvent =>
  ({ev: 'press', step, what, index, ms});

const at = (p: PlayerState, ev: PressEvent) => reducePlayer(p, {type: 'press', ev});

test('the player event opens the player; a state without player closes it', () => {
  let p = reducePlayer(initialPlayer, {type: 'player', ev: PLAYER});
  assert.equal(p.open, true);
  assert.equal(p.steps.length, 7);
  assert.equal(p.running, -1);
  assert.deepEqual(caption(p), {text: 'Press Play to watch the recording.', note: false});
  p = reducePlayer(p, {type: 'core', player: undefined});
  assert.equal(p.open, false);
});

test('a press runs its step: the caption shows its note, else its label', () => {
  let p = reducePlayer(initialPlayer, {type: 'player', ev: PLAYER});
  p = at(p, press(0, 'key', 0));
  assert.equal(p.running, 0);
  assert.deepEqual(caption(p), {text: 'The cell fires once.', note: true});
  p = at(p, press(1, 'cmd', 0));
  assert.deepEqual(caption(p), {text: 'Step 2: Set iapp = 0.08 (no note)', note: false});
  /* the step's end (state.player.running -1): the caption stays, the keycaps go */
  p = reducePlayer(p, {type: 'core', player: {step: 2, running: -1, playing: true, speed: 1, fast: false, intact: true}});
  assert.equal(p.running, -1);
  assert.equal(p.shown, 1);
  assert.equal(keycaps(p), null);
  /* a restart: nothing has run */
  p = reducePlayer(p, {type: 'core', player: {step: 0, running: -1, playing: false, speed: 1, fast: false, intact: true}});
  assert.equal(caption(p).text, 'Press Play to watch the recording.');
});

test('keycaps light one by one; the main menu item for the first key; a menu answer in the open menu', () => {
  let p = at(reducePlayer(initialPlayer, {type: 'player', ev: PLAYER}), press(0, 'key', 0));
  assert.deepEqual(keycaps(p)?.caps.map(c => [c.text, c.down, c.ahead]), [['I', true, false], ['G', false, true]]);
  assert.equal(litMenuKey(p), 'i');
  assert.equal(litAskKey(p), null);
  p = at(p, press(0, 'key', 1));
  assert.deepEqual(keycaps(p)?.caps.map(c => [c.text, c.down, c.ahead]), [['I', false, false], ['G', true, false]]);
  assert.equal(litMenuKey(p), null);
  assert.equal(litAskKey(p), 'g');
});

test('a button step lights its button (a window\'s by win:id), not the menu', () => {
  let p = at(reducePlayer(initialPlayer, {type: 'player', ev: PLAYER}), press(3, 'key', 0));
  assert.deepEqual(keycaps(p)?.caps.map(c => c.text), ['Integrate']);
  assert.equal(litButton(p), 'Integrate');
  assert.equal(litMenuKey(p), null);
  p = at(p, press(5, 'key', 0));
  assert.equal(litButton(p), 'auto:run');
  p = at(p, press(5, 'key', 1));
  assert.equal(litButton(p), null);
  assert.equal(litAskKey(p), 's');
});

test('a dialog\'s answers fill in: a value, a file, a cancel, a form\'s values', () => {
  let p = at(reducePlayer(initialPlayer, {type: 'player', ev: PLAYER}), press(2, 'answer', 0));
  assert.deepEqual(answerFill(p), ['1e7']);
  assert.equal(keycaps(p)?.answering, true);
  p = at(p, press(6, 'answer', 0));
  assert.deepEqual(answerFill(p), ['x.set']);
  assert.deepEqual(answerTexts(null), ['(cancelled)']);
  assert.deepEqual(answerTexts(['a', 'b']), ['a', 'b']);
});

test('the progress: a segment per step, a view step half, done and now', () => {
  const p = at(reducePlayer(initialPlayer, {type: 'player', ev: PLAYER}), press(2, 'key', 0));
  const segs = segments(p, 2);
  assert.equal(segs.length, 7);
  assert.deepEqual(segs.map(s => (s.now ? 'now' : s.done ? 'done' : s.view ? 'view' : '-')), ['done', 'done', 'now', '-', 'view', '-', '-']);
});

test('the app state: player and press events, state.player, the banner dismissed until another file', () => {
  let s = reduce(READY, {type: 'event', ev: {...PLAYER, intact: false}});
  assert.equal(s.player.open, true);
  assert.equal(s.player.intact, false);
  s = reduce(s, {type: 'player', action: {type: 'dismiss'}});
  assert.equal(s.player.dismissed, true);
  s = reduce(s, {type: 'event', ev: {...PLAYER, intact: false}}); /* the same file again (a note saved) */
  assert.equal(s.player.dismissed, true);
  s = reduce(s, {type: 'event', ev: {...PLAYER, file: '/tmp/other.recx', intact: false}});
  assert.equal(s.player.dismissed, false);
  s = reduce(s, {type: 'event', ev: press(0, 'key', 0)});
  assert.equal(s.player.running, 0);
  const st = {ev: 'state', pars: [], ics: [], rows: 0, menu: 0, win: 1,
    player: {step: 1, running: -1, playing: true, speed: 2, fast: false, intact: false}} as unknown as StateEvent;
  s = reduce(s, {type: 'event', ev: st});
  assert.equal(s.player.running, -1);
  s = reduce(s, {type: 'player', action: {type: 'select', step: 3}});
  assert.equal(s.player.selected, 3);
});
