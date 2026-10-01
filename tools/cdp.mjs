/* The headless browser driver of tools/web2check.mjs (the browser check
   of web2): find a Chrome, Chromium or Edge, start it with the
   DevTools protocol on, talk to it over Node's WebSocket (Node 22 or later,
   no npm packages), and start xppautX in browser mode. */
import {spawn, spawnSync} from 'node:child_process';
import fs from 'node:fs';
import http from 'node:http';
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
      if (d.method && this.onEvent) this.onEvent(d);
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

/* Draw and frame timing (W58: the program itself carries no code that
   exists only to measure or slow it -- performance is for CI). Injected
   into every document the page navigates to (Page.addScriptToEvaluateOnNewDocument
   runs before any of the page's own scripts), so it needs no cooperation
   from web2/src: a PerformanceObserver for long tasks (the Long Tasks API,
   read back through Runtime.evaluate), and the timestamps of every
   animation frame, whose gaps stand in for how long each frame's drawing
   took (a frame slowed by a synchronous draw shows up as a wider gap
   before the next one). window.__xppPerf is this script's own global,
   never read by the app. */
const PERF_SCRIPT = `(() => {
  const KEEP = 2000;
  window.__xppPerf = {longTasks: [], frames: []};
  try {
    new PerformanceObserver(list => {
      for (const e of list.getEntries()) {
        window.__xppPerf.longTasks.push({start: e.startTime, duration: e.duration});
        if (window.__xppPerf.longTasks.length > KEEP) window.__xppPerf.longTasks.shift();
      }
    }).observe({type: 'longtask', buffered: true});
  } catch (e) { /* no Long Tasks API */ }
  function raf(t) {
    window.__xppPerf.frames.push(t);
    if (window.__xppPerf.frames.length > KEEP) window.__xppPerf.frames.shift();
    requestAnimationFrame(raf);
  }
  requestAnimationFrame(raf);
})();`;

/** installs PERF_SCRIPT for every document this tab navigates to from now
    on; call once, right after Page.enable, before the first Page.navigate */
export async function installPerfObserver(cdp) {
  /* The page asks "Leave site?" before a reload and tells the core it is
     leaving (W108, browser mode): a check must not hang on the prompt, nor
     let the core's 2 s exit race a slow reload, so the page's beforeunload
     listener is stopped before it runs (an accepted prompt would delay the
     reload under the check's first evaluation) and the beacon is a no-op. */
  cdp.onEvent = d => {
    if (d.method === 'Page.javascriptDialogOpening')
      cdp.send('Page.handleJavaScriptDialog', {accept: true}).catch(() => undefined);
  };
  await cdp.send('Page.addScriptToEvaluateOnNewDocument',
    {source: "navigator.sendBeacon = () => true; "
      + "window.addEventListener('beforeunload', e => e.stopImmediatePropagation(), true);"});
  return cdp.send('Page.addScriptToEvaluateOnNewDocument', {source: PERF_SCRIPT});
}

/* Wait for a process to exit (W105). Resolves when process.exitCode is not null. */
async function waitForExit(proc, timeoutMs = 5000) {
  return new Promise(resolve => {
    if (proc.exitCode !== null) { resolve(); return; }
    const done = () => { proc.off('exit', done); clearTimeout(timer); resolve(); };
    proc.on('exit', done);
    const timer = setTimeout(() => resolve(), timeoutMs);
  });
}

/* Set up downloads folder for this run: build/web2check-downloads/<pid>.
   Removes stale sibling folders whose pid is not running. */
function setupDownloadsFolder() {
  const base = path.join(process.cwd(), 'build', 'web2check-downloads');
  if (!fs.existsSync(base)) fs.mkdirSync(base, {recursive: true});

  /* Remove stale folders (pids not running) */
  const entries = fs.readdirSync(base, {withFileTypes: true});
  for (const entry of entries) {
    if (!entry.isDirectory()) continue;
    const pid = Number(entry.name);
    if (!Number.isNaN(pid)) {
      try {
        process.kill(pid, 0); /* check if process exists without sending signal */
      } catch (e) {
        /* process does not exist, remove the folder */
        try {
          fs.rmSync(path.join(base, entry.name), {recursive: true, force: true, maxRetries: 5});
        } catch (rmErr) {
          /* best effort, ignore if removal fails */
        }
      }
    }
  }

  /* Create folder for this run */
  const downloads = path.join(base, String(process.pid));
  fs.mkdirSync(downloads, {recursive: true});
  return downloads;
}

export async function startBrowser(browser, profile) {
  const downloads = setupDownloadsFolder();

  const proc = spawn(browser, ['--headless=new', '--remote-debugging-port=0', `--user-data-dir=${profile}`,
    '--no-first-run', '--no-default-browser-check', '--disable-gpu', '--hide-scrollbars', '--mute-audio',
    '--force-device-scale-factor=1', '--font-render-hinting=none', '--disable-lcd-text',
    /* a headless window is never in front: without these Chrome throttles its timers and animation frames
       (macOS treats it as occluded), and the chart draws frames behind the page's state (W20) */
    '--disable-background-timer-throttling', '--disable-backgrounding-occluded-windows', '--disable-renderer-backgrounding',
    'about:blank'],
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

  /* Set download behavior once at browser start, for all sections */
  await cdp.send('Browser.setDownloadBehavior', {behavior: 'allow', downloadPath: downloads})
    .catch(() => cdp.send('Page.setDownloadBehavior', {behavior: 'allow', downloadPath: downloads}))
    .catch(() => undefined);

  const cleanup = async () => {
    try {
      fs.rmSync(downloads, {recursive: true, force: true, maxRetries: 5});
    } catch (e) {
      /* best effort, ignore if removal fails */
    }
  };

  return {proc, cdp, downloads, waitForExit: () => waitForExit(proc), cleanup};
}

/* xppautX in browser mode (--browser, not its desktop window) in `dir` with `args` (the model and its options);
   resolves with the process and the address it printed */
export function startServer(bin, dir, args) {
  const proc = spawn(bin, ['--browser', '--no-open', '--port', '0', ...args],
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

/* Ends a startServer() session: {"cmd":"quit"} over its own /cmd (as
   File/Quit does), so it removes its AUTO scratch folder and exits on its
   own (issue #32 -- a kill leaves the folder behind); a kill is only the
   fallback when quitting does not make it exit in time, or the request
   itself fails (a wedged server). */
export function stopServer(server, timeoutMs = 2000) {
  return new Promise(resolve => {
    if (server.proc.exitCode !== null) { resolve(); return; }
    const done = () => { server.proc.off('exit', done); clearTimeout(timer); resolve(); };
    server.proc.on('exit', done);
    const timer = setTimeout(() => { if (server.proc.exitCode === null) server.proc.kill(); }, timeoutMs);
    try {
      const u = new URL(server.url);
      const req = http.request({host: u.hostname, port: u.port, path: '/cmd' + u.search, method: 'POST',
        headers: {'Content-Type': 'application/json'}}, res => res.resume());
      req.on('error', () => { if (server.proc.exitCode === null) server.proc.kill(); });
      req.end(JSON.stringify({cmd: 'quit'}));
    } catch {
      if (server.proc.exitCode === null) server.proc.kill();
    }
  });
}
