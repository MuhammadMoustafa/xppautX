#!/usr/bin/env node
/* Screenshot regression check for the web front end (the web counterpart of
   tools/guicheck.sh). Builds xppaut-web from a git ref (default HEAD), runs
   it and the working-tree binary through the same session of real key
   presses and clicks (tools/web_steps.txt) in a headless Chrome or Edge,
   and compares the screenshots and the files the session writes.

   node tools/webshots.mjs [--ref HEAD] [--base BIN] [--new BIN] [--browser PATH]
                           [--steps tools/web_steps.txt] [--out build/webshots]

   --base skips building the ref. No npm packages: the browser is driven
   through the DevTools protocol with Node's WebSocket (Node 22 or later).
   Screenshots, a side-by-side report.html and both sessions' files are
   left in --out. */
import {spawn, spawnSync} from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import zlib from 'node:zlib';
import {fileURLToPath} from 'node:url';

const win = process.platform === 'win32';
const exe = win ? '.exe' : '';
const top = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const opt = {ref: 'HEAD', steps: 'tools/web_steps.txt', out: 'build/webshots', new: `xppaut-web${exe}`};
for (let i = 2; i < process.argv.length; i++) {
  const k = process.argv[i].replace(/^--/, '');
  opt[k] = process.argv[++i];
}
const out = path.resolve(top, opt.out);

function findBrowser() {
  if (opt.browser) return opt.browser;
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

function run(cmd, args, cwd) {
  const r = spawnSync(cmd, args, {cwd, stdio: ['ignore', 'pipe', 'pipe'], shell: false, maxBuffer: 1 << 28});
  if (r.status !== 0) throw new Error(`${cmd} ${args.join(' ')} failed:\n${r.stderr || r.error}`);
  return r.stdout;
}

/* ---- the DevTools protocol ------------------------------------------------ */

class Cdp {
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

const sleep = ms => new Promise(r => setTimeout(r, ms));

async function startBrowser(browser, profile) {
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

function startServer(bin, dir) {
  const proc = spawn(bin, ['--no-open', '--port', '0', 'lecar.ode', '-anifile', 'gui_test.ani'],
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

/* ---- the session ------------------------------------------------------------ */

const NAMED = {Escape: 27, Enter: 13, Tab: 9, Backspace: 8, Delete: 46, Home: 36, End: 35, PageUp: 33, PageDown: 34,
  ArrowLeft: 37, ArrowUp: 38, ArrowRight: 39, ArrowDown: 40};

/* until the client has finished (idle, or waiting in a prompt) for a while */
const SETTLE = `(async () => {
  const ok = () => typeof client !== 'undefined' && client.menus && (!client.busy || client.pendingAsk);
  for (let n = 0, good = 0; n < 1200 && good < 12; n++) {
    await new Promise(r => setTimeout(r, 25));
    good = ok() ? good + 1 : 0;
  }
  await new Promise(r => requestAnimationFrame(() => requestAnimationFrame(r)));
  return ok();
})()`;

/* the centre of an element: css=SELECTOR, or the visible button/tab/item whose
   text is TEXT (dialogs first) */
function locate(target) {
  const css = target.startsWith('css=') ? JSON.stringify(target.slice(4)) : null;
  return `(() => {
    const seen = e => e && e.getClientRects().length && getComputedStyle(e).visibility !== 'hidden';
    let e = null;
    if (${css}) e = [...document.querySelectorAll(${css})].find(seen);
    else {
      const want = ${JSON.stringify(target)};
      const text = e => (e.firstChild && e.firstChild.nodeType === 3 ? e.firstChild.textContent : e.textContent).trim();
      const pool = [...document.querySelectorAll('.xpp-dialogs button, .xpp-dialogs .xpp-file, button, summary, .xpp-tab')];
      e = pool.find(x => seen(x) && text(x) === want);
    }
    if (!e) return null;
    e.scrollIntoView({block: 'nearest', inline: 'nearest'});
    const r = e.getBoundingClientRect();
    return {x: r.left, y: r.top, w: r.width, h: r.height};
  })()`;
}

async function mouse(cdp, type, x, y) {
  await cdp.send('Input.dispatchMouseEvent', {type, x, y, button: 'left', buttons: type === 'mouseReleased' ? 0 : 1,
    clickCount: 1});
}

async function session(cdp, url, steps, shotDir) {
  const problems = [];
  await cdp.send('Page.enable');
  await cdp.send('Emulation.setDeviceMetricsOverride', {width: 1280, height: 860, deviceScaleFactor: 1, mobile: false});
  await cdp.send('Page.navigate', {url});
  await sleep(500);
  for (const [n, raw] of steps.entries()) {
    const line = raw.trim();
    if (!line || line.startsWith('#')) continue;
    const [cmd, ...rest] = line.split(/\s+/);
    const t0 = Date.now();
    const arg = line.slice(cmd.length).trim();
    try {
      if (cmd === 'size') {
        await cdp.send('Emulation.setDeviceMetricsOverride', {width: +rest[0], height: +rest[1], deviceScaleFactor: 1, mobile: false});
        await sleep(400);
      } else if (cmd === 'key') {
        for (const k of rest) {
          if (NAMED[k]) {
            const code = NAMED[k];
            await cdp.send('Input.dispatchKeyEvent', {type: 'rawKeyDown', key: k, code: k, windowsVirtualKeyCode: code,
              ...(k === 'Enter' ? {text: '\r'} : {})});
            await cdp.send('Input.dispatchKeyEvent', {type: 'keyUp', key: k, code: k, windowsVirtualKeyCode: code});
          } else {
            await cdp.send('Input.dispatchKeyEvent', {type: 'keyDown', key: k, text: k, unmodifiedText: k});
            await cdp.send('Input.dispatchKeyEvent', {type: 'keyUp', key: k});
          }
          await cdp.eval(SETTLE);
        }
      } else if (cmd === 'type') {
        await cdp.send('Input.insertText', {text: arg});
      } else if (cmd === 'click' || cmd === 'drag' || cmd === 'clickat') {
        /* click TARGET | clickat TARGET X Y | drag TARGET X1 Y1 X2 Y2 (offsets in the element) */
        const target = cmd === 'click' ? arg : rest[0];
        const r = await cdp.eval(locate(target));
        if (!r) throw new Error(`no element ${target}`);
        const at = cmd === 'click' ? [r.x + r.w / 2, r.y + r.h / 2] : [r.x + +rest[1], r.y + +rest[2]];
        await cdp.send('Input.dispatchMouseEvent', {type: 'mouseMoved', x: at[0], y: at[1]});
        await mouse(cdp, 'mousePressed', at[0], at[1]);
        if (cmd === 'drag') {
          const to = [r.x + +rest[3], r.y + +rest[4]];
          for (let s = 1; s <= 8; s++) {
            await cdp.send('Input.dispatchMouseEvent', {type: 'mouseMoved', button: 'left', buttons: 1,
              x: at[0] + (to[0] - at[0]) * s / 8, y: at[1] + (to[1] - at[1]) * s / 8});
            await sleep(40);
          }
          await mouse(cdp, 'mouseReleased', to[0], to[1]);
        } else await mouse(cdp, 'mouseReleased', at[0], at[1]);
      } else if (cmd === 'set') {
        /* set css=SELECTOR VALUE: a field's value, as typing and leaving it would */
        const sel = rest[0].slice(4), value = line.slice(line.indexOf(rest[0]) + rest[0].length).trim();
        const ok = await cdp.eval(`(() => { const e = document.querySelector(${JSON.stringify(sel)}); if (!e) return false;
          e.value = ${JSON.stringify(value)}; e.dispatchEvent(new Event('input', {bubbles: true}));
          e.dispatchEvent(new Event('change', {bubbles: true})); return true; })()`);
        if (!ok) throw new Error(`no element ${sel}`);
      } else if (cmd === 'wait') {
        await sleep(+rest[0]);
      } else if (cmd === 'eval') {
        await cdp.eval(`(async () => { ${arg} })()`);
      } else if (cmd === 'shot') {
        await cdp.eval(SETTLE);
        await sleep(150);
        let clip = null;
        if (rest[1]) {
          clip = await cdp.eval(locate(rest[1]));
          if (!clip) throw new Error(`no element ${rest[1]} to shoot`);
        }
        const png = await cdp.send('Page.captureScreenshot', {format: 'png',
          ...(clip ? {clip: {x: clip.x, y: clip.y, width: Math.max(1, clip.w), height: Math.max(1, clip.h), scale: 1}} : {})});
        fs.writeFileSync(path.join(shotDir, rest[0] + '.png'), Buffer.from(png.data, 'base64'));
      } else throw new Error(`unknown step ${cmd}`);
      if (cmd !== 'shot' && cmd !== 'wait') await cdp.eval(SETTLE);
      if (opt.verbose) console.log(`${Date.now() - t0} ms  ${line}`);
    } catch (e) {
      problems.push(`step ${n + 1} (${line}): ${e.message.split('\n')[0]}`);
      if (cmd === 'shot') fs.writeFileSync(path.join(shotDir, rest[0] + '.png'), 'missing: ' + e.message);
    }
  }
  return problems;
}

async function runOnce(tag, bin, browser, steps) {
  const shotDir = path.join(out, tag), dir = path.join(out, 'run');
  fs.rmSync(dir, {recursive: true, force: true});
  fs.mkdirSync(shotDir, {recursive: true});
  fs.mkdirSync(dir, {recursive: true});
  fs.copyFileSync(path.join(top, 'examples/ode/lecar.ode'), path.join(dir, 'lecar.ode'));
  fs.copyFileSync(path.join(top, 'tools/gui_test.ani'), path.join(dir, 'gui_test.ani'));
  const server = await startServer(bin, dir);
  const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'xppweb-'));
  const {proc, cdp} = await startBrowser(browser, profile);
  let problems;
  try {
    problems = await session(cdp, server.url, steps, shotDir);
  } finally {
    proc.kill();
    server.proc.kill();
    await sleep(500);
    fs.rmSync(profile, {recursive: true, force: true, maxRetries: 5});
  }
  const files = path.join(out, 'files_' + tag);
  fs.cpSync(dir, files, {recursive: true});
  return problems;
}

async function main() {
  const browser = findBrowser();
  if (!browser) {
    console.log('webshots: no Chrome, Chromium or Edge found (set CHROME=path); skipped');
    process.exit(0);
  }
  /* the contents, not the folder: a viewer may hold it open */
  fs.mkdirSync(out, {recursive: true});
  for (const e of fs.readdirSync(out)) fs.rmSync(path.join(out, e), {recursive: true, force: true, maxRetries: 5});
  let base = opt.base && path.resolve(top, opt.base);
  if (!base) {
    const src = path.join(out, 'src');
    fs.mkdirSync(src, {recursive: true});
    fs.writeFileSync(path.join(out, 'src.tar'), run('git', ['archive', '--format=tar', opt.ref], top));
    run('tar', ['-xf', 'src.tar', '-C', 'src'], out); /* relative: GNU tar reads C: as a host */
    const make = process.env.MAKE || (win ? 'mingw32-make' : 'make');
    run(make, ['-j8', `xppaut-web${exe}`], src);
    base = path.join(src, `xppaut-web${exe}`);
  }
  const steps = fs.readFileSync(path.join(top, opt.steps), 'utf8').split(/\r?\n/);
  const problems = {};
  problems.base = await runOnce('base', base, browser, steps);
  problems.new = await runOnce('new', path.resolve(top, opt.new), browser, steps);

/* A PNG decoder, so two shots can be called the same when they differ only by
   Skia's antialiasing rounding (a channel off by one on a rounded corner);
   headless Chrome is not bit-exact between two runs of the same page. */
function decodePng(buf) {
  if (buf.length < 8 || buf.readUInt32BE(0) !== 0x89504e47) return null;
  let w = 0, h = 0, depth = 0, color = 0, interlace = 0;
  const idat = [];
  for (let i = 8; i + 8 <= buf.length;) {
    const len = buf.readUInt32BE(i), type = buf.toString('latin1', i + 4, i + 8);
    const body = buf.subarray(i + 8, i + 8 + len);
    if (type === 'IHDR') {
      w = body.readUInt32BE(0); h = body.readUInt32BE(4);
      depth = body[8]; color = body[9]; interlace = body[12];
    } else if (type === 'IDAT') idat.push(body);
    i += 12 + len;
  }
  if (depth !== 8 || interlace !== 0 || (color !== 2 && color !== 6)) return null;
  const bpp = color === 6 ? 4 : 3, stride = w * bpp;
  const raw = zlib.inflateSync(Buffer.concat(idat));
  const out = Buffer.alloc(h * stride);
  for (let y = 0, o = 0; y < h; y++) {
    const filter = raw[y * (stride + 1)], line = raw.subarray(y * (stride + 1) + 1, (y + 1) * (stride + 1));
    for (let x = 0; x < stride; x++, o++) {
      const a = x >= bpp ? out[o - bpp] : 0, b = y ? out[o - stride] : 0, c = x >= bpp && y ? out[o - stride - bpp] : 0;
      let v = line[x];
      if (filter === 1) v += a;
      else if (filter === 2) v += b;
      else if (filter === 3) v += (a + b) >> 1;
      else if (filter === 4) {
        const pp = a + b - c, pa = Math.abs(pp - a), pb = Math.abs(pp - b), pc = Math.abs(pp - c);
        v += pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
      }
      out[o] = v & 255;
    }
  }
  return {w, h, bpp, data: out};
}

/* same, or off by at most SLACK in a channel on a handful of pixels */
const SLACK = 2, SLACK_PIXELS = 64;
function sameShot(a, b) {
  if (a.equals(b)) return 'equal';
  const x = decodePng(a), y = decodePng(b);
  if (!x || !y || x.w !== y.w || x.h !== y.h || x.bpp !== y.bpp) return false;
  let n = 0;
  for (let i = 0; i < x.data.length; i += x.bpp) {
    let d = 0;
    for (let k = 0; k < x.bpp; k++) d = Math.max(d, Math.abs(x.data[i + k] - y.data[i + k]));
    if (d > SLACK) return false;
    if (d) n++;
  }
  return n > SLACK_PIXELS ? false : 'antialiasing (' + n + ' px)';
}

  let same = 0, total = 0, rows = '';
  for (const f of fs.readdirSync(path.join(out, 'base')).sort()) {
    total++;
    const a = fs.readFileSync(path.join(out, 'base', f)), b = fs.existsSync(path.join(out, 'new', f))
      ? fs.readFileSync(path.join(out, 'new', f)) : Buffer.alloc(0);
    const verdict = sameShot(a, b), equal = verdict !== false;
    if (equal) same++;
    if (equal && verdict !== 'equal') console.log('same but for ' + verdict + ': ' + f);
    if (!equal) console.log('DIFF: ' + f);
    rows += `<tr class="${equal ? 'same' : 'diff'}"><th>${f}${equal ? '' : ' (differs)'}</th>` +
      `<td><img src="base/${f}"></td><td>${equal ? '' : `<img src="new/${f}">`}</td></tr>\n`;
  }
  let fsame = 0, ftotal = 0;
  const walk = d => fs.readdirSync(d, {withFileTypes: true}).flatMap(e => e.isDirectory() ? walk(path.join(d, e.name)).map(x => path.join(e.name, x)) : [e.name]);
  for (const f of walk(path.join(out, 'files_base')).sort()) {
    ftotal++;
    const a = path.join(out, 'files_base', f), b = path.join(out, 'files_new', f);
    if (fs.existsSync(b) && fs.readFileSync(a).equals(fs.readFileSync(b))) fsame++;
    else console.log('DIFF file: ' + f);
  }
  fs.writeFileSync(path.join(out, 'report.html'), `<!doctype html><meta charset="utf-8"><title>webshots</title>
<style>body{font:13px sans-serif;margin:16px} img{max-width:600px;border:1px solid #ccc} tr.diff th{color:#b3261e}
th{text-align:left;vertical-align:top;padding-right:12px} td{vertical-align:top}</style>
<h1>web screenshots: ${same} / ${total} identical to ${opt.base ? opt.base : opt.ref}</h1>
<table><tr><th></th><th>base</th><th>new (when different)</th></tr>${rows}</table>`);
  for (const tag of ['base', 'new'])
    for (const p of problems[tag]) console.log(`PROBLEM (${tag}): ${p}`);
  console.log(`web screens identical to ${opt.base ? opt.base : opt.ref}: ${same} / ${total}`);
  console.log(`files written identical: ${fsame} / ${ftotal}`);
  console.log(`report: ${path.join(out, 'report.html')}`);
  const ok = total > 0 && same === total && fsame === ftotal && !problems.base.length && !problems.new.length;
  process.exit(ok ? 0 : 1);
}

main().catch(e => {
  console.error(e.stack || e);
  process.exit(2);
});
