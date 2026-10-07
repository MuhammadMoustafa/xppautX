/* User-reported UI regressions, checked against a real core and browser.
   Uses fixture model data; no OS picker automation is claimed. */
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {findBrowser, startBrowser, startServer, stopServer, waitFor, waitForExit, xppautSequences} from './cdp.mjs';

const bin = path.resolve(process.argv[2] || './xppautX.exe');
const root = fs.mkdtempSync(path.join(os.tmpdir(), 'xpp-ui-workflow-'));
let browser, server;
try {
  browser = await startBrowser(findBrowser(), path.join(root, 'profile'));
  server = await startServer(bin, root, [path.resolve('examples/ode/lecar.odex')]);
  const cdp = browser.cdp;
  const until = async (code, label) => assert.ok(await waitFor(() => cdp.eval(`!!(${code})`)), label);
  const state = code => cdp.eval(`(() => { const s = __xpp.state(); const w = s.plots.windows.find(w => w.win === s.plots.active); return ${code}; })()`);
  const ready = () => until('window.__xpp?.state().hello && !__xpp.state().busy', 'idle model');
  const key = async k => { await cdp.eval(`document.querySelector('.plot-host').focus(); document.activeElement.dispatchEvent(new KeyboardEvent('keydown', {key:${JSON.stringify(k)}, bubbles:true})); true`); };
  const close = async () => { await cdp.eval("document.querySelector('.dialog-close').click(); true"); await ready(); };
  const run = async () => {
    const n = await state('s.seriesCount');
    await cdp.eval("document.querySelector('[data-button=Integrate]').click(); true");
    await until(`__xpp.state().seriesCount > ${n} && !__xpp.state().busy`, 'run finished');
  };
  await cdp.send('Emulation.setDeviceMetricsOverride', {width:1280,height:860,deviceScaleFactor:1,mobile:false});
  await cdp.send('Page.navigate', {url: server.url}); await ready(); await xppautSequences(cdp);
  await key('F'); await until('__xpp.state().pendingKeys.join("") === "F"', 'uppercase F begins the File sequence (the XPPAUT preset)');
  await key('Escape'); await until('__xpp.state().pendingKeys.length === 0', 'Escape cancels the sequence');
  await key('I'); await until('__xpp.state().ask?.kind === "menu"', 'uppercase I opens initial conditions');
  await cdp.eval("document.querySelector('.dialog-backdrop').dispatchEvent(new PointerEvent('pointerdown', {bubbles:true})); true");
  await until('!__xpp.state().ask && !__xpp.state().busy', 'outside dismisses menu');
  await cdp.eval("document.querySelector('.workspace-tools summary').click(); true");
  await until('document.querySelector(".workspace-tools").open', 'Tools dropdown open');
  await cdp.eval("document.querySelector('.plot-host').dispatchEvent(new PointerEvent('pointerdown', {bubbles:true})); true");
  await until('!document.querySelector(".workspace-tools").open', 'outside dismisses dropdown');
  await cdp.eval("document.querySelector('[data-plot-axis=x]').click(); true");
  await until('__xpp.state().ask?.kind === "form"', 'axis opens variables and limits editor'); await close();
  console.log('PASS shortcuts, menu/dropdown dismissal and axis editor');
  await run(); await run();
  assert.equal(await state('w.history.runs.length'), 1, 'Freeze retains prior run');
  assert.equal(await cdp.eval('document.querySelectorAll("[data-trace-key]").length'), 2, 'each run has a legend');
  await cdp.eval("document.querySelector('.plot-tools button[title^=\"Fit\"]').click(); true"); await ready();
  assert.equal(await state('w.history.runs.length'), 1, 'Fit preserves prior run');
  const legend = 'document.querySelector("[data-trace-key=\\"current:0\\"]")';
  await cdp.eval(`${legend}.click(); true`);
  await until('__xpp.plot().curves[0].visible === false', 'legend hides current trace');
  await cdp.eval(`${legend}.click(); ${legend}.dispatchEvent(new MouseEvent('dblclick', {bubbles:true})); true`);
  await until('document.querySelector("[data-trace-dialog]")', 'double click edits legend');
  await cdp.eval(`(() => { const n = document.querySelector('[data-trace-name]'); n.value='My trajectory'; n.dispatchEvent(new Event('input',{bubbles:true}));
    const c = document.querySelector('[data-trace-color]'); c.value='#123456'; c.dispatchEvent(new Event('input',{bubbles:true})); })()`);
  await until('document.querySelector("[data-trace-name]").value === "My trajectory"', 'legend draft');
  await cdp.eval("document.querySelector('[data-trace-dialog] form').requestSubmit(); true");
  await until('__xpp.plot().curves[0].label === "My trajectory" && __xpp.plot().curves[0].color === "#123456"', 'name and colour applied');
  const point = await state(`(() => {
    const p = __xpp.plot(), r = document.querySelector('.plot-view:not([hidden]) .u-over').getBoundingClientRect();
    const c = w.series.curves[0], x = w.series.columns.get(c.x)[100], y = w.series.columns.get(c.y)[100];
    return {x:r.x+(x-p.x.min)*r.width/(p.x.max-p.x.min), y:r.y+(p.y.max-y)*r.height/(p.y.max-p.y.min)};
  })()`);
  await cdp.send('Input.dispatchMouseEvent', {type:'mouseMoved', ...point});
  await until('document.querySelector(".trace-tooltip")?.textContent.includes("My trajectory")', 'hover names trace');
  await cdp.eval("document.querySelector('[data-trace-key^=run-]').click(); true");
  await until('__xpp.plot().runs.drawn === 0', 'individual older legend hides its trace');
  await cdp.eval("document.querySelector('[data-trace-key^=run-]').click(); true");
  await until('__xpp.plot().runs.drawn > 0', 'individual older legend shows its trace');
  await cdp.eval("document.querySelector('[data-freeze-runs]').click(); true"); await ready();
  await until('__xpp.state().plots.windows[0].info.freeze === 0', 'Freeze disabled');
  await run(); assert.equal(await state('w.history.runs.length'), 0, 'unfrozen next run clears prior trajectories');
  const rows = await state('w.series.rows');
  await cdp.eval("document.querySelector('[data-run=continue]').click(); true");
  await until(`!__xpp.state().busy && __xpp.state().core.rows > ${rows}`, 'Continue appends');
  assert.equal(await state('w.history.runs.length'), 0, 'Continue extends current run without creating an older run');
  console.log('PASS Freeze, Fit, per-trace visibility and legend name/colour editing');
  await cdp.eval("document.querySelector('.table-toggle').click(); true");
  await until('__xpp.state().table.page?.rows > 0', 'data table');
  const command = async id => {
    await cdp.eval(`(() => { const l=__xpp.state().hello.windows.browser; __xpp.send({cmd:'key',win:'browser',key:l.keys[l.ids.indexOf(${JSON.stringify(id)})]}); })()`);
    await until('__xpp.state().ask?.kind === "string"', `${id} asks for name`);
  };
  const answer = async text => { await cdp.eval(`__xpp.send({cmd:'answer',id:__xpp.state().ask.id,ok:1,value:${JSON.stringify(text)}}); true`); };
  for (const [name, formula] of [['EXTRAA', 'V*2'], ['EXTRAB', 'W*3']]) {
    await command('addcol'); await answer(name);
    await until('__xpp.state().ask?.name === "Formula:"', 'formula ask'); await answer(formula); await ready();
    await until(`__xpp.state().table.page.cols.includes(${JSON.stringify(name)})`, 'column added');
  }
  const before = await state('s.table.page.data.map(r => r.slice())');
  await command('delcol'); await answer('EXTRAA'); await ready();
  await until('!__xpp.state().table.page.cols.includes("EXTRAA") && __xpp.state().table.page.cols.includes("EXTRAB")', 'middle added column deleted');
  const after = await state('s.table.page.data');
  assert.deepEqual(after.map(r => r.at(-1)), before.map(r => r.at(-1)), 'shifted column retains its values');
  await run(); await until('__xpp.state().table.page.rows > 0 && !__xpp.state().busy', 'derived column recomputed');
  assert.ok(await state('s.table.page.data.every(r => Math.abs(r.at(-1) - r[2]*3) < 1e-5)'), 'remaining derived column recomputes correctly');
  console.log('PASS Delete column shifts values safely and remaining formulas recompute');
  await cdp.eval("[...document.querySelectorAll('button')].find(b => b.textContent === 'New window').click(); true");
  await until('__xpp.state().plots.windows.length === 2 && !__xpp.state().busy', 'additional plot');
  const active = await state('s.plots.active');
  await cdp.eval("document.querySelector('.plot-view:not([hidden]) [data-plot-axis=y]').click(); true");
  await until('__xpp.state().ask?.kind === "form"', 'additional plot axis editor');
  assert.equal(await state('s.plots.active'), active, 'axis editor targets the additional plot'); await close();
  console.log('PASS additional-window axis editor');
  await stopServer(server);
  server = await startServer(bin, root, [path.resolve('examples/ode/lorenz.odex')]);
  await cdp.send('Page.navigate', {url:server.url});
  await until('window.__xpp?.state().hello?.file.endsWith("lorenz.odex") && __xpp.state().core.rows > 0 && !__xpp.state().busy', '3D model');
  await run();
  await until('__xpp.plot()?.curves.length >= 2 && document.querySelector("[data-trace-key^=run-]")', '3D retained trace and legend');
  const retained = await state('w.history.runs.length');
  await cdp.eval("document.querySelector('.plot-fit').click(); true"); await ready();
  assert.equal(await state('w.history.runs.length'), retained, '3D Fit retains trajectories');
  await cdp.eval("document.querySelector('[data-trace-key=\"current:0\"]').click(); true");
  await until('__xpp.plot().curves[0].points === 0 && __xpp.plot().curves[1].points > 0', '3D individual visibility');
  console.log('PASS 3D retained runs, Fit and individual visibility');
} finally {
  if (server) await stopServer(server);
  if (browser) { browser.cdp.ws.close(); browser.proc.kill(); await waitForExit(browser.proc); await browser.cleanup(); }
  fs.rmSync(root, {recursive:true, force:true, maxRetries:5});
}
