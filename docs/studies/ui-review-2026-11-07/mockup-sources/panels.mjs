import fs from 'node:fs';
import path from 'node:path';
import {findBrowser, startBrowser, startServer, stopServer, sleep} from '../../../../tools/cdp.mjs';
const [bin, work] = process.argv.slice(2);
const B = fs.readFileSync(path.join(work, 'b.css'), 'utf8'), BJ = fs.readFileSync(path.join(work, 'b.js'), 'utf8');
const P = fs.readFileSync(path.join(work, 'p.css'), 'utf8'), PJ = fs.readFileSync(path.join(work, 'p.js'), 'utf8');
const server = await startServer(path.resolve(bin), work, ['lecar.odex']);
const b = await startBrowser(findBrowser(), path.join(work, 'profile'));
const cdp = b.cdp;
const until = async expr => { for (let i = 0; i < 600; i++) { if (await cdp.eval(`(() => { try { const s = __xpp.state(); return !!(${expr}); } catch { return false; } })()`)) return; await sleep(100); } throw new Error(expr); };
const shot = async name => { await sleep(700); const r = await cdp.send('Page.captureScreenshot', {format: 'png'}); fs.writeFileSync(path.join(work, name), Buffer.from(r.data, 'base64')); };
try {
  await cdp.send('Page.enable'); await cdp.send('Page.navigate', {url: server.url});
  await cdp.send('Emulation.setDeviceMetricsOverride', {width: 1440, height: 900, deviceScaleFactor: 1, mobile: false});
  await until('s.hello && !s.busy'); await cdp.eval(`__xpp.send({cmd: 'run.initial'})`); await until('!s.busy && !s.ask');
  await cdp.eval(`(() => { [...document.querySelectorAll('button')].find(b => b.textContent.trim() === 'Data').click(); return 1; })()`); await sleep(600);
  await shot('panel_table_now.png');
  await cdp.eval(`(() => { [...document.querySelectorAll('.table-panel button')].find(b => b.textContent.trim() === 'Back').click(); return 1; })()`); await sleep(400);
  await cdp.eval(`(() => { const s = document.createElement('style'); s.textContent = ${JSON.stringify(B + P)}; document.head.append(s); return 1; })()`); await cdp.eval(BJ);
  for (const mode of ['graph', 'table']) {
    if (mode === 'table') await cdp.eval(`(() => { [...document.querySelectorAll('button')].find(b => b.textContent.trim() === 'Data').click(); return 1; })()`);
    await sleep(500); await cdp.eval(`history.replaceState(null, '', location.pathname + location.search + '#${mode}'); 1`); await cdp.eval(PJ);
    await shot(`panel_${mode}_new.png`);
  }
  await cdp.eval(`document.body.classList.remove('mock-table'); 1`);
  await cdp.eval(`(() => { const b = [...document.querySelectorAll('.table-panel button')].find(b => b.textContent.trim() === 'Back'); b?.click(); return 1; })()`);
  await cdp.eval(`__xpp.send({cmd: 'autox.open'})`); await until('!s.busy'); await sleep(800);
  await shot('panel_autox_now.png');
  await cdp.eval(`history.replaceState(null, '', location.pathname + location.search + '#autox'); 1`); await cdp.eval(PJ);
  await shot('panel_autox_new.png');
} finally { b.proc.kill(); await b.waitForExit().catch(() => {}); await b.cleanup(); await stopServer(server); }
