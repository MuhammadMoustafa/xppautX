// Screenshots the real page at each width, as it is and with a proposed CSS injected.
import fs from 'node:fs';
import path from 'node:path';
import {findBrowser, startBrowser, startServer, stopServer, sleep} from '../../../../tools/cdp.mjs';
const [bin, work, cssFile] = process.argv.slice(2);
const CSS = fs.readFileSync(cssFile, "utf8"); const JS = process.argv[5] ? fs.readFileSync(process.argv[5], "utf8") : "1";
const SIZES = JSON.parse(process.env.SIZES || "[[390,844],[768,1024],[1024,700],[1280,800],[1440,900]]");
const server = await startServer(path.resolve(bin), work, ['lecar.odex']);
const b = await startBrowser(findBrowser(), path.join(work, 'profile'));
const cdp = b.cdp;
try {
  await cdp.send('Page.enable');
  await cdp.send('Page.navigate', {url: server.url});
  const until = async expr => { for (let i = 0; i < 600; i++) { if (await cdp.eval(`(() => { try { const s = __xpp.state(); return !!(${expr}); } catch { return false; } })()`)) return; await sleep(100); } throw new Error('timeout ' + expr); };
  await until('s.hello && !s.busy');
  await cdp.eval(`__xpp.send({cmd: 'run.initial'})`);
  await until('!s.busy && !s.ask');
  const shot = async name => { await sleep(700); const r = await cdp.send('Page.captureScreenshot', {format: 'png'}); fs.writeFileSync(path.join(work, name), Buffer.from(r.data, 'base64')); };
  const pass = async tag => { for (const dark of [false, true]) {
    await cdp.send('Emulation.setEmulatedMedia', {features: [{name: 'prefers-color-scheme', value: dark ? 'dark' : 'light'}]});
    for (const [w, h] of SIZES) {
      await cdp.send('Emulation.setDeviceMetricsOverride', {width: w, height: h, deviceScaleFactor: 1, mobile: w < 768});
      await shot(`${w}x${h}${dark ? '_dark' : ''}_${tag}.png`);
    } } };
  await pass('now');
  await cdp.eval(`(() => { const s = document.createElement('style'); s.id = 'mock'; s.textContent = ${JSON.stringify(CSS)}; document.head.append(s); return 1; })()`);
  await cdp.eval(JS);
  await pass('new');
} finally {
  b.proc.kill(); await b.waitForExit().catch(() => {}); await b.cleanup(); await stopServer(server);
}
