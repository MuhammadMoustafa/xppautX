/* The headless browser driver of tools/web2check.mjs (the browser check
   of web2): find a Chrome, Chromium or Edge, start it with the
   DevTools protocol on, talk to it over Node's WebSocket (Node 22 or later,
   no npm packages), and start xppautX in browser mode. */
import {spawn, spawnSync} from 'node:child_process';
import fs from 'node:fs';
import http from 'node:http';
import path from 'node:path';
import net from 'node:net';

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

const STDERR_KEPT = 4000; /* bytes of the browser's stderr a failure shows */

/* ---- the DevTools protocol ------------------------------------------------ */

const NOTES_KEPT = 30; /* page events a failure shows: a reload's whole story, not the whole run's */

export class Cdp {
  constructor(url) {
    this.ws = new WebSocket(url);
    this.id = 0;
    this.pending = new Map();
    this.notes = [];
    /* why the connection closed, once it has: a send after it fails at once, since a
       closed WebSocket drops what is sent without a word and its answer would never come
       (W168: a browser that died left every call pending, and Node exited 0, a pass) */
    this.closed = null;
    this.ws.onclose = () => {
      this.closed = `the DevTools connection closed${this.browserState ? ` (browser: ${this.browserState()})` : ''}`;
      for (const [, p] of this.pending) p.reject(new Error(`${p.method}: ${this.closed}`));
      this.pending.clear();
      if (this.openFailed) this.openFailed(new Error(this.closed));
    };
    this.ws.onmessage = m => {
      const d = JSON.parse(m.data);
      if (d.method) this.note(d);
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
      this.openFailed = reject; /* closed before it opened */
    });
  }
  /* what the page did, for a failure to name itself (W158): the last
     navigations, console errors, exceptions, the log's errors (a failed
     load among them) and a crashed renderer, oldest first, NOTES_KEPT of them */
  note(d) {
    const p = d.params;
    let text = null;
    switch (d.method) {
      case 'Page.frameNavigated': if (!p.frame.parentId) text = `navigated to ${p.frame.url}`; break;
      case 'Runtime.exceptionThrown': text = `exception: ${p.exceptionDetails.exception?.description || p.exceptionDetails.text}`; break;
      case 'Runtime.consoleAPICalled':
        if (p.type === 'error' || p.type === 'warning') text = `console.${p.type}: ${p.args.map(a => a.value ?? a.description).join(' ')}`;
        break;
      case 'Log.entryAdded': if (p.entry.level === 'error') text = `log: ${p.entry.text} ${p.entry.url || ''}`; break;
      case 'Inspector.targetCrashed': text = 'the renderer crashed'; break;
      default: break;
    }
    if (text === null) return;
    this.notes.push(text);
    if (this.notes.length > NOTES_KEPT) this.notes.shift();
  }
  /* what the page holds right now, and what it did before: for an error */
  async report() {
    let now;
    let deadline;
    try {
      const r = await Promise.race([
        this.send('Runtime.evaluate', {expression: `JSON.stringify({url: location.href, state: document.readyState,
          title: document.title, app: !!document.getElementById('app') && document.getElementById('app').childElementCount,
          text: (document.body ? document.body.innerText : '').slice(0, 200)})`, returnByValue: true}),
        new Promise(resolve => { deadline = setTimeout(() => resolve(null), REPORT_TIMEOUT_MS); })]);
      now = r && r.result ? r.result.value : 'the page did not answer within 3 s';
    } catch (e) {
      now = `the page could not be asked: ${e.message}`;
    } finally {
      clearTimeout(deadline);
    }
    return `page now: ${now}\n  page before: ${this.notes.length ? this.notes.join('\n    ') : '(nothing recorded)'}` +
      (this.browserState ? `\n  browser: ${this.browserState()}` : '');
  }
  send(method, params = {}) {
    if (this.closed) return Promise.reject(new Error(`${method}: ${this.closed}`));
    const id = ++this.id;
    this.ws.send(JSON.stringify({id, method, params}));
    return new Promise((resolve, reject) => this.pending.set(id, {resolve, reject, method}));
  }
  async eval(expression) {
    const r = await this.send('Runtime.evaluate', {expression, awaitPromise: true, returnByValue: true});
    if (r.exceptionDetails) {
      const what = r.exceptionDetails.exception?.description || r.exceptionDetails.text;
      /* the page's own error is its code's; a missing __xpp is the app never having loaded: say what did load */
      throw new Error(`page: ${what}${/__xpp is not defined/.test(what) ? `\n  ${await this.report()}` : ''}`);
    }
    return r.result.value;
  }
}

export const sleep = ms => new Promise(r => setTimeout(r, ms));
const REPORT_TIMEOUT_MS = 3000; /* diagnostics must also work when the renderer is wedged */
const WAIT_TIMEOUT_MS = 15000; /* safety ceiling for external processes and files, not a speed assertion */
const POLL_MS = 40; /* yield between observations of a condition without an event API */
const EXIT_TIMEOUT_MS = 5000; /* safety ceiling for a stopped child to release its files */
const DEVTOOLS_FETCH_TIMEOUT_MS = 1000; /* a hung local DevTools endpoint must not defeat waitFor's deadline */

/* null until the endpoint answers: a browser still starting refuses or stalls the
   request, which waitFor then asks again (macos-ui's Chrome, 2026-10-03) */
async function devtoolsPage(port, accepts) {
  let list;
  try {
    list = await (await fetch(`http://127.0.0.1:${port}/json/list`,
      {signal: AbortSignal.timeout(DEVTOOLS_FETCH_TIMEOUT_MS)})).json();
  } catch {
    return null;
  }
  return list.find(t => t.type === 'page' && (!accepts || accepts(t)));
}

async function connectPage(page) {
  const cdp = new Cdp(page.webSocketDebuggerUrl);
  await cdp.open();
  /* not every build has every domain (W13c dropped this catch; restored) */
  for (const domain of ['Page', 'Runtime', 'Log', 'Inspector']) await cdp.send(`${domain}.enable`).catch(() => undefined);
  return cdp;
}

export async function waitFor(read, timeoutMs = WAIT_TIMEOUT_MS) {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    const value = await read();
    if (value) return value;
    if (Date.now() >= deadline) return null;
    await sleep(POLL_MS);
  }
}

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
    {source: "window.__xppCheckRendering = true; navigator.sendBeacon = () => true; "
      + "window.addEventListener('beforeunload', e => e.stopImmediatePropagation(), true);"});
  return cdp.send('Page.addScriptToEvaluateOnNewDocument', {source: PERF_SCRIPT});
}

/* Wait for a process to exit (W105). Resolves when process.exitCode is not null. */
export async function waitForExit(proc, timeoutMs = EXIT_TIMEOUT_MS) {
  return new Promise(resolve => {
    if (proc.exitCode !== null || proc.signalCode !== null) { resolve(true); return; }
    const done = () => { proc.off('exit', done); clearTimeout(timer); resolve(true); };
    proc.on('exit', done);
    const timer = setTimeout(() => { proc.off('exit', done); resolve(false); }, timeoutMs);
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
    /* inside another sandbox (a Codex agent's), Chrome's own cannot start its GPU and renderer
       processes (access denied) and Chrome exits: the outer one confines it instead (W168) */
    ...(process.env.XPP_CHECK_NO_BROWSER_SANDBOX === '1' ? ['--no-sandbox'] : []),
    'about:blank'],
  {stdio: ['ignore', 'ignore', 'pipe']});
  /* the browser's stderr for its whole life (the tail: STDERR_KEPT bytes) and how it ended: a
     failure names the exit code or signal and what Chrome said (W158) */
  let text = '', ended = null;
  const state = () => (ended ? `exited (${ended})` : `running, pid ${proc.pid}`) + `; stderr:\n${text.slice(-STDERR_KEPT) || '(empty)'}`;
  proc.stderr.on('data', d => { text += d; });
  proc.on('exit', (code, signal) => { ended = signal ? `signal ${signal}` : `code ${code}`; });
  const startMs = 30000 * Number(process.env.XPP_CHECK_SLOW || 1); /* a safety timeout, never a pass/fail budget (W58) */
  const wsUrl = await new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error(`browser ${browser} did not start (no DevTools address in ${startMs / 1000} s): ${state()}`)), startMs);
    const look = () => {
      const m = /DevTools listening on (ws:\/\/\S+)/.exec(text);
      if (m) { clearTimeout(timer); resolve(m[1]); }
    };
    proc.stderr.on('data', look);
    proc.on('exit', () => { clearTimeout(timer); reject(new Error(`browser ${browser} exited before it started: ${state()}`)); });
    proc.on('error', e => { clearTimeout(timer); reject(new Error(`browser ${browser} could not be run: ${e.message}`)); });
  }).catch(e => { proc.kill(); throw e; });
  const port = new URL(wsUrl).port;
  const page = await waitFor(() => devtoolsPage(port), startMs);
  if (!page) { proc.kill(); throw new Error(`browser ${browser} has no page: ${state()}`); }
  const cdp = await connectPage(page);
  cdp.browserState = state;
  /* the events Cdp.report() tells a failure with; not Network, which would send every SSE
     message of the page over this socket (a failed load is the Log's error and the
     chrome-error page Cdp.report() shows) */

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

/* The environment of a program under test: its per-user settings folder (the keymap, XPP_CONFIG_DIR) is in
   its own `dir`, so a check that changes a key never touches the real file */
function configEnv(dir) {
  return {...process.env, XPP_CONFIG_DIR: path.join(dir, 'xpp-config')};
}

/* xppautX in browser mode (--browser, not its desktop window) in `dir` with `args` (the model and its options);
   resolves with the process and the address it printed */
export function startServer(bin, dir, args) {
  const proc = spawn(bin, ['--browser', '--no-open', '--port', '0', ...args],
    {cwd: dir, stdio: ['ignore', 'pipe', 'pipe'], env: configEnv(dir)});
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

/* W13c: own the window process and attach to its WebView2 page, Windows only. */
export async function startWebView2(bin, dir, args) {
  if (!win) throw new Error('--webview2 requires Windows');
  const port = await new Promise((resolve, reject) => {
    const listener = net.createServer();
    listener.on('error', reject);
    listener.listen(0, '127.0.0.1', () => {
      const assigned = listener.address().port;
      listener.close(() => resolve(assigned));
    });
  });
  const proc = spawn(bin, [...args, '--port', '0'], {cwd: dir, stdio: ['ignore', 'pipe', 'pipe'],
    env: {...configEnv(dir), WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS: `--remote-debugging-port=${port}`}});
  let diagnostic = '';
  proc.stdout.on('data', d => { diagnostic = (diagnostic + d).slice(-STDERR_KEPT); });
  proc.stderr.on('data', d => { diagnostic = (diagnostic + d).slice(-STDERR_KEPT); });
  let spawnError;
  proc.on('error', e => { spawnError = e; });
  try {
    const page = await waitFor(async () => {
      if (spawnError) throw spawnError;
      if (proc.exitCode !== null) throw new Error(`WebView2 window exited (${proc.exitCode}): ${diagnostic}`);
      try {
        return await devtoolsPage(port, t => /^http:\/\/127\.0\.0\.1:\d+\/\?t=/.test(t.url));
      } catch { return null; } // DevTools has not started listening yet: wait for the target.
    });
    if (!page) throw new Error(`WebView2 did not expose the xppautX page: ${diagnostic}`);
    const cdp = await connectPage(page);
    return {proc, url: page.url, cdp};
  } catch (e) {
    proc.kill(); // only the process this function started
    await waitForExit(proc);
    throw e;
  }
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
