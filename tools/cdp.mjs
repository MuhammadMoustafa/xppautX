/* The headless browser driver the web checks share (tools/webtest.mjs,
   tools/web2check.mjs): find a Chrome, Chromium or Edge, start it with the
   DevTools protocol on, talk to it over Node's WebSocket (Node 22 or later,
   no npm packages), and start xppautX in browser mode. */
import {spawn, spawnSync} from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';

const win = process.platform === 'win32';

export function findBrowser(explicit) {
  if (explicit) return explicit;
  if (process.env.CHROME) return process.env.CHROME;
  const candidates = win
    ? ['C:/Program Files/Google/Chrome/Application/chrome.exe', 'C:/Program Files (x86)/Google/Chrome/Application/chrome.exe',
      'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe', 'C:/Program Files/Microsoft/Edge/Application/msedge.exe']
    : process.platform === 'darwin'
      ? ['/Applications/Google Chrome.app/Contents/MacOS/Google Chrome', '/Applications/Chromium.app/Contents/MacOS/Chromium']
      : ['google-chrome', 'google-chrome-stable', 'chromium', 'chromium-browser', 'microsoft-edge'];
  for (const c of candidates) {
    if (path.isAbsolute(c) ? fs.existsSync(c) : spawnSync('which', [c]).status === 0) return c;
  }
  return null;
}

/* ---- the DevTools protocol ------------------------------------------------ */

export class Cdp {
  constructor(url) {
    this.ws = new WebSocket(url);
    this.id = 0;
    this.pending = new Map();
    this.ws.onmessage = m => {
      const d = JSON.parse(m.data);
      const p = d.id && this.pending.get(d.id);
      if (!p) return;
      this.pending.delete(d.id);
      d.error ? p.reject(new Error(`${p.method}: ${JSON.stringify(d.error)}`)) : p.resolve(d.result);
    };
  }
  open() {
    return new Promise((resolve, reject) => {
      this.ws.onopen = resolve;
      this.ws.onerror = reject;
    });
  }
  send(method, params = {}) {
    const id = ++this.id;
    this.ws.send(JSON.stringify({id, method, params}));
    return new Promise((resolve, reject) => this.pending.set(id, {resolve, reject, method}));
  }
  async eval(expression) {
    const r = await this.send('Runtime.evaluate', {expression, awaitPromise: true, returnByValue: true});
    if (r.exceptionDetails) throw new Error(`page: ${r.exceptionDetails.exception?.description || r.exceptionDetails.text}`);
    return r.result.value;
  }
}

export const sleep = ms => new Promise(r => setTimeout(r, ms));

export async function startBrowser(browser, profile) {
  const proc = spawn(browser, ['--headless=new', '--remote-debugging-port=0', `--user-data-dir=${profile}`,
    '--no-first-run', '--no-default-browser-check', '--disable-gpu', '--hide-scrollbars', '--mute-audio',
    '--force-device-scale-factor=1', '--font-render-hinting=none', '--disable-lcd-text', 'about:blank'],
  {stdio: ['ignore', 'ignore', 'pipe']});
  const wsUrl = await new Promise((resolve, reject) => {
    let text = '';
    proc.stderr.on('data', d => {
      text += d;
      const m = /DevTools listening on (ws:\/\/\S+)/.exec(text);
      if (m) resolve(m[1]);
    });
    proc.on('exit', () => reject(new Error('browser exited:\n' + text)));
    setTimeout(() => reject(new Error('browser did not start:\n' + text)), 30000);
  });
  const port = new URL(wsUrl).port;
  let page = null;
  for (let i = 0; i < 50 && !page; i++) {
    const list = await (await fetch(`http://127.0.0.1:${port}/json/list`)).json();
    page = list.find(t => t.type === 'page');
    if (!page) await sleep(100);
  }
  const cdp = new Cdp(page.webSocketDebuggerUrl);
  await cdp.open();
  return {proc, cdp};
}

/* xppautX in browser mode in `dir` with `args` (the model and its options);
   resolves with the process and the address it printed */
export function startServer(bin, dir, args) {
  const proc = spawn(bin, ['--no-open', '--port', '0', ...args],
    {cwd: dir, stdio: ['ignore', 'pipe', 'pipe']});
  return new Promise((resolve, reject) => {
    let text = '';
    const look = d => {
      text += d;
      const m = /(http:\/\/127\.0\.0\.1:\d+\/\?t=\w+)/.exec(text);
      if (m) resolve({proc, url: m[1]});
    };
    proc.stdout.on('data', look);
    proc.stderr.on('data', d => { text += d; });
    proc.on('exit', code => reject(new Error(`${bin} exited (${code}):\n${text}`)));
  });
}
