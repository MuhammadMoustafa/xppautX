import fs from 'node:fs';
import path from 'node:path';
import {findBrowser, startBrowser, startServer, stopServer, sleep} from '../../../../tools/cdp.mjs';
const [bin, work] = process.argv.slice(2);
const CSS = fs.readFileSync(path.join(work, 'f.css'), 'utf8'), JS = fs.readFileSync(path.join(work, 'f.js'), 'utf8');
const server = await startServer(path.resolve(bin), work, ['lecar.odex']);
const b = await startBrowser(findBrowser(), path.join(work, 'profile'));
const cdp = b.cdp;
try {
  await cdp.send('Page.enable'); await cdp.send('Page.navigate', {url: server.url});
  for (let i = 0; i < 300 && !(await cdp.eval(`(() => { try { return !!__xpp.state().hello && !__xpp.state().busy } catch { return false } })()`)); i++) await sleep(100);
  await cdp.eval(`(() => { const s = document.createElement('style'); s.textContent = ${JSON.stringify(CSS)}; document.head.append(s); return 1; })()`);
  for (const dark of [false, true]) {
    await cdp.send('Emulation.setEmulatedMedia', {features: [{name: 'prefers-color-scheme', value: dark ? 'dark' : 'light'}]});
    for (const [w, h] of [[1280, 800], [390, 844]]) {
      await cdp.send('Emulation.setDeviceMetricsOverride', {width: w, height: h, deviceScaleFactor: 1, mobile: w < 768});
      for (const form of ['settings', 'save_set', 'load_file']) {
        await cdp.eval(`history.replaceState(null, '', location.pathname + location.search + '#${form}'); ${JSON.stringify(1)}`);
        await cdp.eval(JS); await sleep(400);
        const r = await cdp.send('Page.captureScreenshot', {format: 'png'});
        fs.writeFileSync(path.join(work, `form_${form}_${w}${dark ? '_dark' : ''}.png`), Buffer.from(r.data, 'base64'));
      }
    }
  }
} finally { b.proc.kill(); await b.waitForExit().catch(() => {}); await b.cleanup(); await stopServer(server); }
