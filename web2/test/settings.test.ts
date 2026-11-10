/* Settings during a computation (W106, GitHub #155): the values panel's and
   the autoX forms' edits go to the core at once, busy or not, and the field
   shows what was sent until that set's own done -- nothing is held, no
   "pending" (npm test, through session.ts with a transport that records). */
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {Session} from '../src/session';
import type {Command, XppEvent} from '../src/protocol/types';
import type {Transport} from '../src/protocol/transport';
import {sentText} from '../src/store/values';
import {shownSettings} from '../src/store/autoxSettings';
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
  ev({ev: 'session.state', sliders: [], pars: [['iapp', 0.05]], ics: [], bcs: []});
  ev({ev: 'done', status: 'ok'}); /* the subscription's */
  ev({ev: 'done', status: 'ok', result: {due: false}}); /* W622: the opening's update question's */
  s.send({cmd: 'run.initial'});
  ev({ev: 'job.computing'});
  sent.length = 0;
  s.setValue('par', 'iapp', '0.3');
  s.setNumeric('total_time', '50');
  assert.deepEqual(sent, [{cmd: 'value.set_parameters', values: {iapp: 0.3}},
    {cmd: 'run.configure', total_time: 50}], 'sent at once, each by the command that takes it');
  const st = () => s.store.getState();
  assert.equal(sentText(st().values.inflight, 'par:iapp'), '0.3', 'the field shows what was sent');
  /* the core's state during the run still has the value the run started with: the field keeps what was sent */
  ev({ev: 'session.state', sliders: [], pars: [['iapp', 0.05]], ics: [], bcs: []});
  assert.equal(sentText(st().values.inflight, 'par:iapp'), '0.3');
  ev({ev: 'done', status: 'ok'}); /* the run ends */
  assert.equal(sentText(st().values.inflight, 'par:iapp'), '0.3');
  ev({ev: 'session.state', sliders: [], pars: [['iapp', 0.3]], ics: [], bcs: []});
  ev({ev: 'done', status: 'ok'}); /* the set's own */
  assert.equal(sentText(st().values.inflight, 'par:iapp'), null, 'the core\'s value takes over');
  ev({ev: 'done', status: 'ok'}); /* the numerics set's */
  assert.deepEqual(st().values.inflight, []);
  assert.equal(st().busy, false);
});

test('W106: an autoX settings edit during a run is sent at once, shown until its own done', () => {
  const {s, sent, ev} = session();
  ev(HELLO);
  ev({ev: 'autox.settings', numerics: {max_steps: 200}, pars: [], axes: {plot: 'norm'}, marks: []});
  ev({ev: 'done', status: 'ok'});
  s.send({cmd: 'run.initial'});
  ev({ev: 'job.computing'});
  sent.length = 0;
  s.autoxSettings({numerics: {max_steps: 25}});
  assert.deepEqual(sent, [{cmd: 'autox.configure_numerics', max_steps: 25}]);
  assert.equal(shownSettings(s.store.getState().autoxSettings)!.numerics.max_steps, 25);
  ev({ev: 'done', status: 'ok'});
  assert.equal(shownSettings(s.store.getState().autoxSettings)!.numerics.max_steps, 25);
});

test('W622: the opening asks whether an update check is due; a due one that fails tells the core, shows nothing', async () => {
  const {sent, ev, s} = session();
  ev({...HELLO, about: [[{text: 'xppautX v0.1.0'}]]});
  assert.deepEqual(sent.map(c => c.cmd), ['events.subscribe', 'update.due'], 'the question follows the subscription');
  const fetched: string[] = [];
  const original = globalThis.fetch;
  globalThis.fetch = (async (url: string) => { fetched.push(url); return new Response('unavailable', {status: 503}); }) as typeof fetch;
  try {
    ev({ev: 'done', status: 'ok', result: {due: false}});
    await new Promise(resolve => setTimeout(resolve, 0));
    assert.deepEqual(fetched, [], 'not due: no request');
    ev({ev: 'done', status: 'ok', result: {due: true}});
    for (let i = 0; i < 50 && !sent.some(c => c.cmd === 'update.report_failure'); i++) await new Promise(resolve => setTimeout(resolve, 0));
  } finally { globalThis.fetch = original; }
  assert.equal(fetched.length, 1);
  assert.ok(sent.some(c => c.cmd === 'update.report_failure' && String(c.reason).includes('HTTP 503')), 'the core is told why at INFO');
  assert.equal(s.store.getState().updateNotice, null);
  assert.equal(s.store.getState().toasts.length, 0, 'nothing is shown');
});
