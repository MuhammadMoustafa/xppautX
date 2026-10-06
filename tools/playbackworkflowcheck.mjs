/* Native playback of a portable sandbox recording: launch, restart and reopening. */
import assert from 'node:assert/strict';
import path from 'node:path';
import fs from 'node:fs';
import {startWebView2, stopServer, waitFor} from './cdp.mjs';

const bin = path.resolve(process.argv[2]);
const rec = path.resolve(process.argv[3]);
const steps = fs.readFileSync(rec, 'utf8').split(/\r?\n@steps\r?\n/)[1].split(/\r?\n/).filter(line => line[0] === '{').length;
const server = await startWebView2(bin, process.cwd(), [rec]);
const cdp = server.cdp;
const state = code => cdp.eval(`(() => { const s = window.__xpp?.state(); return s && (${code}); })()`);
const done = () => state(`s.core?.player?.step === ${steps} && s.core.player.running === -1 && !s.busy`);
try {
  assert.ok(await waitFor(done, 120000), 'Automatic initial native playback must complete');
  assert.equal(await state('s.core.rows'), 40001);
  assert.equal(await state('s.core.time'), 400);
  assert.equal(await state('s.core.now.length'), 8);
  assert.equal(await state('s.core.rates.length'), 8);
  assert.ok(await state('s.core.rates.every(v => v !== null)'), 'All eight rates must be supplied');
  assert.ok(await state('s.diagram.views[s.diagram.active].points.x.length > 20'));
  assert.deepEqual(await cdp.eval('__xpp.log().filter(e => e.kind === "error")'), []);
  console.log('native player layout', await cdp.eval('JSON.stringify({steps:__xpp.state().player.steps.length, shown:__xpp.state().diagram.shown, dock:!!document.querySelector(".player-dock"), button:document.querySelector(".player-play")?.textContent})'));
  assert.ok(await cdp.eval(`(() => { const d = document.querySelector('.player-dock');
    return !!d && getComputedStyle(d).top === '0px' && d.querySelectorAll('.player-controls').length === 1
      && !document.querySelector('.run-toolbar') && /Play again/.test(d.querySelector('.player-play').textContent); })()`), 'Native player controls must show completed playback above AUTO');
  console.log('PASS native automatic playback: all steps, all states, continuation and AUTO, controls above AUTO');
  await cdp.eval('__xpp.send({cmd:"play",op:"from",step:0,play:false}); true');
  assert.ok(await waitFor(() => state('s.core?.player?.step === 0 && s.core.player.running === -1 && !s.core.player.playing && !s.busy')));
  assert.ok(await cdp.eval('/Play$/.test(document.querySelector(".player-play").textContent.trim())'));
  await cdp.eval('document.querySelector(".player-play").click(); true');
  assert.ok(await waitFor(done, 120000));
  assert.deepEqual(await cdp.eval('__xpp.log().filter(e => e.kind === "error")'), []);
  console.log('PASS native paused restart and normal playback, no step 3 errors');
} finally { cdp.ws.close(); await stopServer(server); }
