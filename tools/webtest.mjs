#!/usr/bin/env node
/* Behavioural regression check for the web front end (the web counterpart of
   tools/guicheck.sh). Runs xppautX and drives it through a real session of
   key presses and clicks (tools/web_steps.txt) in a headless Chrome or Edge,
   and checks what each step claims about the result: a dialog with the
   right kind, a value the server computed, a canvas that got drawn to, a
   file the session wrote. No screenshots, no image comparison: tests check
   data, never pixels.

   node tools/webtest.mjs [--bin ./xppautX] [--browser PATH]
                          [--steps tools/web_steps.txt] [--out build/webtest]

   No npm packages: the browser is driven through the DevTools protocol with
   Node's WebSocket (Node 22 or later). It builds nothing: run it against the
   binary a previous step already built. */
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {findBrowser, sleep, startBrowser, startServer} from './cdp.mjs';

const win = process.platform === 'win32';
const exe = win ? '.exe' : '';
const top = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const opt = {steps: 'tools/web_steps.txt', out: 'build/webtest', bin: `xppautX${exe}`};
const FLAGS = ['verbose'];
for (let i = 2; i < process.argv.length; i++) {
  const k = process.argv[i].replace(/^--/, '');
  opt[k] = FLAGS.includes(k) ? true : process.argv[++i];
}
const out = path.resolve(top, opt.out);

/* ---- the session ------------------------------------------------------------ */

const NAMED = {Escape: 27, Enter: 13, Tab: 9, Backspace: 8, Delete: 46, Home: 36, End: 35, PageUp: 33, PageDown: 34,
  ArrowLeft: 37, ArrowUp: 38, ArrowRight: 39, ArrowDown: 40};

/* until the client has finished (idle, or waiting in a prompt) for a while */
const SETTLE = `(async () => {
  const ok = () => typeof client !== 'undefined' && client.menus && (!client.busy || client.pendingAsk);
  for (let n = 0, good = 0; n < 1200 && good < 4; n++) {
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

async function session(cdp, url, steps) {
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
      } else if (cmd === 'expect') {
        /* expect [!]css=SELECTOR | expect [!]TEXT: an element is (or is not) present and visible,
           the same way click/clickat locate one. The real check for what a dialog says, a value
           the server computed, or a canvas having been drawn to is an eval: this is for plain
           presence/absence (a dialog is open, a tab exists, a panel closed). */
        const neg = arg.startsWith('!');
        const target = neg ? arg.slice(1).trim() : arg;
        const r = await cdp.eval(locate(target));
        if (neg && r) throw new Error(`unexpected element ${target}`);
        if (!neg && !r) throw new Error(`no element ${target}`);
      } else throw new Error(`unknown step ${cmd}`);
      if (cmd !== 'wait') await cdp.eval(SETTLE);
      if (opt.verbose) console.log(`${Date.now() - t0} ms  ${line}`);
    } catch (e) {
      problems.push(`step ${n + 1} (${line}): ${e.message.split('\n')[0]}`);
    }
  }
  return problems;
}

/* ---- files the session writes ------------------------------------------------ */

/* the session's cwd starts with just these two (copied in below); anything
   else that shows up by the end is a file the session wrote and gets a
   content check; the two inputs themselves must come out byte-identical,
   since nothing in a browsing session should rewrite the model it loaded */
const SEED = ['lecar.ode', 'gui_test.ani'];

function checkWrittenFiles(dir, before) {
  const problems = [];
  for (const f of SEED) {
    const a = before.get(f), b = fs.existsSync(path.join(dir, f)) ? fs.readFileSync(path.join(dir, f)) : null;
    if (!b) problems.push(`input file ${f} disappeared during the session`);
    else if (!a.equals(b)) problems.push(`input file ${f} was modified by the session`);
  }
  for (const f of fs.readdirSync(dir)) {
    if (SEED.includes(f)) continue;
    const p = path.join(dir, f), st = fs.statSync(p);
    if (!st.isFile()) continue;
    if (st.size === 0) { problems.push(`${f}: written but empty`); continue; }
    const head = fs.readFileSync(p, {encoding: 'latin1', flag: 'r'}).slice(0, 4096);
    if (/\.dat$/.test(f)) {
      const lines = head.split(/\r?\n/).filter(Boolean);
      if (!lines.length || !/^[\s0-9.eE+-]+$/.test(lines[0]))
        problems.push(`${f}: first line does not look like numeric data: ${JSON.stringify(lines[0])}`);
    } else if (/\.ps$/.test(f) && !head.startsWith('%!')) problems.push(`${f}: not a postscript file`);
    else if (/\.gif$/i.test(f) && !head.startsWith('GIF8')) problems.push(`${f}: not a GIF`);
    else if (/\.svg$/i.test(f) && !head.includes('<svg')) problems.push(`${f}: no <svg> in it`);
  }
  return problems;
}

async function main() {
  const browser = findBrowser(opt.browser);
  if (!browser) {
    console.log('webtest: no Chrome, Chromium or Edge found (set CHROME=path); skipped');
    process.exit(0);
  }
  fs.mkdirSync(out, {recursive: true});
  for (const e of fs.readdirSync(out)) fs.rmSync(path.join(out, e), {recursive: true, force: true, maxRetries: 5});

  const steps = fs.readFileSync(path.resolve(top, opt.steps), 'utf8').split(/\r?\n/);
  const bin = path.resolve(top, opt.bin);
  const dir = path.join(out, 'run');
  fs.mkdirSync(dir, {recursive: true});
  fs.copyFileSync(path.join(top, 'examples/ode/lecar.ode'), path.join(dir, 'lecar.ode'));
  fs.copyFileSync(path.join(top, 'tools/gui_test.ani'), path.join(dir, 'gui_test.ani'));
  const before = new Map(SEED.map(f => [f, fs.readFileSync(path.join(dir, f))]));

  const server = await startServer(bin, dir, ['lecar.ode', '-anifile', 'gui_test.ani']);
  const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'xppweb-'));
  const {proc, cdp} = await startBrowser(browser, profile);
  let problems;
  try {
    problems = await session(cdp, server.url, steps);
  } finally {
    proc.kill();
    server.proc.kill();
    await sleep(500);
    fs.rmSync(profile, {recursive: true, force: true, maxRetries: 5});
  }
  problems = problems.concat(checkWrittenFiles(dir, before));

  for (const p of problems) console.log(`PROBLEM: ${p}`);
  console.log(problems.length ? `web session had ${problems.length} problem(s)` : 'web session passed');
  process.exit(problems.length ? 1 : 0);
}

main().catch(e => {
  console.error(e.stack || e);
  process.exit(2);
});
