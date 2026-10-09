/* No-model startup (W232): the Open model dialog opens at once and accepts the picker answer; Cancel leaves the start screen; the window stays closable.
   Uses the shared browser driver; picker replies are fixtures, not OS dialog coverage. */
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {findBrowser, startBrowser, startServer, startWebView2, stopServer, waitFor, waitForExit} from './cdp.mjs';

const native = process.argv.includes('--webview2');
const bin = path.resolve(process.argv.slice(2).find(a => a !== '--webview2') || `./xppautX${process.platform === 'win32' ? '.exe' : ''}`);
if (native) {
  const server = await startWebView2(bin, process.cwd(), []);
  try {
    assert.ok(await waitFor(() => server.cdp.eval('!!window.__xpp?.state().ask')));
    await server.cdp.eval('window.__xppQuit(); true');
    assert.ok(await waitForExit(server.proc));
    assert.equal(server.proc.exitCode, 0);
    console.log('PASS real native startup close bridge and process exit');
  } finally { server.cdp.ws.close(); await stopServer(server); }
} else {
const root = fs.mkdtempSync(path.join(os.tmpdir(), 'xpp-startup-'));
let browser;
try {
  browser = await startBrowser(findBrowser(), path.join(root, 'profile'));
  for (const mode of ['select', 'cancel', 'close']) {
    const server = await startServer(bin, root, []);
    try {
      const reply = mode === 'select' ? JSON.stringify(path.resolve('examples/ode/lecar.odex'))
        : mode === 'cancel' ? 'null' : 'new Promise(() => {})';
      const {identifier} = await browser.cdp.send('Page.addScriptToEvaluateOnNewDocument',
        {source: `window.__xppFileDialog = async () => ${reply};`});
      await browser.cdp.send('Page.navigate', {url: server.url});
      if (mode === 'select') {
        assert.ok(await waitFor(() => browser.cdp.eval('!!window.__xpp?.state().hello && !window.__xpp.state().busy')));
        assert.equal(path.basename(await browser.cdp.eval('window.__xpp.state().hello.file')), 'lecar.odex');
        await browser.cdp.eval("document.querySelector('.run-toolbar button.primary').click(); true");
        assert.ok(await waitFor(() => browser.cdp.eval('window.__xpp.state().core.rows > 0 && !window.__xpp.state().busy')));
      } else if (mode === 'cancel') {
        /* W232: Cancel leaves the start screen; the program keeps running */
        assert.ok(await waitFor(() => browser.cdp.eval('!!window.__xpp?.state().hello?.start && !window.__xpp.state().ask && !window.__xpp.state().busy')));
        assert.equal(await browser.cdp.eval('window.__xpp.state().exited'), null);
      } else {
        assert.ok(await waitFor(() => browser.cdp.eval('!!window.__xpp?.state().ask')));
        await browser.cdp.eval('window.__xppQuit(); true');
        assert.ok(await waitForExit(server.proc));
        assert.equal(server.proc.exitCode, 0);
      }
      await browser.cdp.send('Page.removeScriptToEvaluateOnNewDocument', {identifier});
      console.log(`PASS startup ${mode}`);
    } finally { await stopServer(server); }
  }
} finally {
  if (browser) { browser.cdp.ws.close(); browser.proc.kill(); await waitForExit(browser.proc); await browser.cleanup(); }
  fs.rmSync(root, {recursive: true, force: true, maxRetries: 5});
}
}
