/* Settings during a computation (W106, GitHub #155): the values panel's and
   the AUTO forms' edits go to the core at once, busy or not, and the field
   shows what was sent until that set's own idle -- nothing is held, no
   "pending" (npm test, through session.ts with a transport that records). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {Session} from '../src/session';
import type {Command, XppEvent} from '../src/protocol/types';
import type {Transport} from '../src/protocol/transport';
import {sentText} from '../src/store/values';
import {shownSettings} from '../src/store/autoSettings';
import {HELLO} from './hello';

function session(): {s: Session; sent: Command[]; ev: (e: object) => void} {
  const sent: Command[] = [];
  let onEvent: (ev: XppEvent) => void = () => {};
  const transport: Transport = {
    send: cmd => { sent.push(cmd); },
    open: (on) => { onEvent = on; },
    close: () => {},
  };
  const s = new Session(transport);
  s.start();
  return {s, sent, ev: e => onEvent(e as XppEvent)};
}

test('W106: a parameter or numerics edit during a computation is sent at once and shown as the value', () => {
  const {s, sent, ev} = session();
  ev(HELLO);
  ev({ev: 'state', pars: [['iapp', 0.05]], ics: [], bcs: []});
  ev({ev: 'idle'});
  s.key('i');
  ev({ev: 'computing'});
  sent.length = 0;
  s.setValue('par', 'iapp', '0.3');
  s.setNumeric('total', '50');
  assert.deepEqual(sent, [{cmd: 'set', kind: 'par', name: 'iapp', text: '0.3'},
    {cmd: 'set', kind: 'num', name: 'total', text: '50'}], 'sent at once, one set each');
  const st = () => s.store.getState();
  assert.equal(sentText(st().values.inflight, 'par:iapp'), '0.3', 'the field shows what was sent');
  /* the core's state during the run still has the value the run started with: the field keeps what was sent */
  ev({ev: 'state', pars: [['iapp', 0.05]], ics: [], bcs: []});
  assert.equal(sentText(st().values.inflight, 'par:iapp'), '0.3');
  ev({ev: 'idle'}); /* the run ends */
  assert.equal(sentText(st().values.inflight, 'par:iapp'), '0.3');
  ev({ev: 'state', pars: [['iapp', 0.3]], ics: [], bcs: []});
  ev({ev: 'idle'}); /* the set's own */
  assert.equal(sentText(st().values.inflight, 'par:iapp'), null, 'the core\'s value takes over');
  ev({ev: 'idle'}); /* the numerics set's */
  assert.deepEqual(st().values.inflight, []);
  assert.equal(st().busy, false);
});

test('W106: an AUTO settings edit during a run is sent at once, shown until its own idle', () => {
  const {s, sent, ev} = session();
  ev(HELLO);
  ev({ev: 'autosettings', numerics: {nmx: 200}, pars: [], axes: {plot: 1}, marks: []});
  ev({ev: 'idle'});
  s.key('i');
  ev({ev: 'computing'});
  sent.length = 0;
  s.autoSettings({numerics: {nmx: 25}});
  assert.deepEqual(sent, [{cmd: 'auto', op: 'set', numerics: {nmx: 25}}]);
  assert.equal(shownSettings(s.store.getState().autoSettings)!.numerics.nmx, 25);
  ev({ev: 'idle'});
  assert.equal(shownSettings(s.store.getState().autoSettings)!.numerics.nmx, 25);
});
