#!/usr/bin/env node
/* State-level check of the new front end (web2/, served at /v2/): drives a
   headless Chrome or Edge through real key presses, mouse and touch events
   and asserts what the page's store and plot hold (window.__xpp), never
   pixels. A desktop session (integrate from the keyboard, the plotted
   numbers against output.dat, hover, wheel and box zoom, undo, reset, pan),
   the values panel (docs/ui-v2.md T3: edit a parameter, a slider by
   keyboard, undo, Tab reachability, the panel as a right column), keyboard-
   only use of the plot and of a prompt, and a phone-sized one (390x844,
   touch: no sideways scroll, the menu drawer, the values sheet, pinch, tap,
   pan, 44px targets).

   node tools/web2check.mjs [--bin ./xppautX] [--browser PATH] [-v]

   Needs Node 22 or later and a browser, nothing else (tools/cdp.mjs). */
import {spawnSync} from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {findBrowser, sleep, startBrowser, startServer} from './cdp.mjs';

const top = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const opt = {bin: `xppautX${process.platform === 'win32' ? '.exe' : ''}`};
for (let i = 2; i < process.argv.length; i++) {
  const a = process.argv[i];
  if (a === '-v') opt.v = true;
  else opt[a.replace(/^--/, '')] = process.argv[++i];
}
const bin = path.resolve(top, opt.bin);
const ODE = path.join(top, 'examples/ode/lecar.ode');

let failures = 0;
function check(name, ok, detail = '') {
  console.log(`${ok ? 'PASS' : 'FAIL'} ${name}${ok ? '' : '  ' + detail}`);
  if (!ok) failures++;
}

/* output.dat of the same model: the numbers the plot must show */
function outputDat() {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'xppsilent-'));
  fs.copyFileSync(ODE, path.join(dir, 'lecar.ode'));
  spawnSync(bin, ['lecar.ode', '-silent'], {cwd: dir, stdio: 'ignore', timeout: 60000});
  const rows = fs.readFileSync(path.join(dir, 'output.dat'), 'utf8').trim().split(/\r?\n/).map(l => l.trim().split(/\s+/).map(Number));
  fs.rmSync(dir, {recursive: true, force: true});
  return rows;
}

/* ---- page helpers --------------------------------------------------------- */

let cdp;
const S = expr => cdp.eval(`(() => { const s = __xpp.state(); return ${expr}; })()`);
const P = () => cdp.eval('__xpp.plot()');

async function until(expr, what, ms = 15000) {
  const t0 = Date.now();
  for (;;) {
    let v = null;
    try {
      v = await cdp.eval(`(() => { try { const s = window.__xpp && __xpp.state(); return !!(${expr}); } catch (e) { return false; } })()`);
    } catch { /* page not there yet */ }
    if (v) return true;
    if (Date.now() - t0 > ms) {
      if (opt.v) console.log(`  (timed out waiting for ${what})`);
      return false;
    }
    await sleep(40);
  }
}

const NAMED = {Escape: 27, Enter: 13, Tab: 9, Home: 36, End: 35, PageUp: 33, PageDown: 34,
  ArrowLeft: 37, ArrowUp: 38, ArrowRight: 39, ArrowDown: 40};
async function key(k, modifiers = 0) {
  if (NAMED[k]) {
    const code = NAMED[k];
    await cdp.send('Input.dispatchKeyEvent', {type: 'rawKeyDown', key: k, code: k, windowsVirtualKeyCode: code, modifiers});
    await cdp.send('Input.dispatchKeyEvent', {type: 'keyUp', key: k, code: k, windowsVirtualKeyCode: code, modifiers});
  } else if (modifiers) {
    const code = k.toUpperCase().charCodeAt(0);
    await cdp.send('Input.dispatchKeyEvent', {type: 'rawKeyDown', key: k, code: `Key${k.toUpperCase()}`, windowsVirtualKeyCode: code, modifiers});
    await cdp.send('Input.dispatchKeyEvent', {type: 'keyUp', key: k, code: `Key${k.toUpperCase()}`, windowsVirtualKeyCode: code, modifiers});
  } else {
    await cdp.send('Input.dispatchKeyEvent', {type: 'keyDown', key: k, text: k, unmodifiedText: k});
    await cdp.send('Input.dispatchKeyEvent', {type: 'keyUp', key: k});
  }
  await sleep(30);
}
const mouse = (type, x, y, extra = {}) => cdp.send('Input.dispatchMouseEvent', {type, x, y, ...extra});

/* the plotting area's box and the screen position of point i of curve c */
const area = () => cdp.eval(`(() => { const r = document.querySelector('.plot-host .u-over').getBoundingClientRect();
  return {x: r.left, y: r.top, w: r.width, h: r.height}; })()`);
async function screenOf(curve, i) {
  const [a, p, s] = [await area(), await P(), await cdp.eval(`(() => { const m = __xpp.state().series;
    const c = m.curves[${curve}]; return {x: m.columns.get(c.x)[${i}], y: m.columns.get(c.y)[${i}]}; })()`)];
  return {x: a.x + (s.x - p.x.min) / (p.x.max - p.x.min) * a.w, y: a.y + (p.y.max - s.y) / (p.y.max - p.y.min) * a.h};
}
const width = r => r.max - r.min;

/* ---- the sessions -------------------------------------------------------------- */

async function desktop(want) {
  await cdp.send('Emulation.setDeviceMetricsOverride', {width: 1280, height: 860, deviceScaleFactor: 1, mobile: false});
  check('the page connects and asks for the plot as data', await until('s.hello && s.seriesCount >= 1 && !s.busy', 'hello'));
  check('an empty plot says so and offers Integrate',
    await cdp.eval(`!!document.querySelector('.plot-empty button')`));

  /* integrate with the keyboard only: I opens the menu, G answers it */
  await key('i');
  check('I opens the Initialconds menu as a dialog', await until("s.ask && s.ask.kind === 'menu'", 'menu'),
    JSON.stringify(await S('s.ask')));
  check('the dialog has the focus', await until(`document.activeElement.closest('[role=dialog]')`, 'focus'));
  const n0 = await S('s.seriesCount');
  await key('g');
  check('G integrates: a new series of 601 rows', await until(`s.seriesCount > ${n0} && s.series.rows === 601 && !s.busy`, 'series'),
    JSON.stringify(await S('s.series && s.series.rows')));

  const cols = await cdp.eval(`(() => { const m = __xpp.state().series; const o = {};
    for (const [k, v] of m.columns) o[k] = Array.from(v); return {cols: o, names: Object.fromEntries(m.names)}; })()`);
  let bad = null;
  for (const [col, values] of Object.entries(cols.cols)) {
    values.forEach((v, r) => {
      const d = Math.abs(v - want[r][+col]), tol = 1e-7 * Math.abs(want[r][+col]) + 1e-30;
      if (d > tol && !bad) bad = `${cols.names[col]} row ${r}: ${v} vs ${want[r][+col]}`;
    });
  }
  check('the store holds the numbers of output.dat (T, V, W)',
    !bad && Object.values(cols.names).join() === 'T,V,W' && want.length === 601, bad || JSON.stringify(cols.names));
  const p = await P();
  check('the plot draws W against V in xy mode, 601 points',
    p && p.mode === 2 && p.curves.length === 1 && p.curves[0].label === 'W vs V' && p.curves[0].points === 601, JSON.stringify(p));
  const view = await S('s.core.view');
  check("the plot starts at the core's window (Viewaxes)",
    Math.abs(p.x.min - view.xlo) < 1e-9 && Math.abs(p.y.max - view.yhi) < 1e-9, JSON.stringify([p.x, p.y, view]));

  /* hover */
  const at = await screenOf(0, 200);
  await mouse('mouseMoved', at.x, at.y);
  /* points lie closer than a pixel where the trajectory is slow: any of the neighbours is right */
  check('hovering a point names it (row, T, V, W)', await until('s.hover && Math.abs(s.hover.row - 200) <= 5', 'hover'),
    JSON.stringify(await S('s.hover')));
  const row = await S('s.hover && s.hover.row');
  check('the readout shows it', (await cdp.eval(`document.querySelector('.readout').textContent`)).includes(`row ${row}`));

  /* wheel zoom about the pointer, then undo */
  const a = await area(), cx = a.x + a.w / 2, cy = a.y + a.h / 2;
  await mouse('mouseWheel', cx, cy, {deltaX: 0, deltaY: -120});
  await until('s.viewport.x', 'wheel');
  const z1 = await S('s.viewport');
  check('the wheel zooms in about the pointer', z1.x && width(z1.x) < width(p.x) * 0.9, JSON.stringify(z1));
  /* a box */
  await mouse('mouseMoved', cx - 80, cy - 60);
  await mouse('mousePressed', cx - 80, cy - 60, {button: 'left', buttons: 1, clickCount: 1});
  for (let s = 1; s <= 6; s++) await mouse('mouseMoved', cx - 80 + 25 * s, cy - 60 + 20 * s, {button: 'left', buttons: 1});
  await mouse('mouseReleased', cx + 70, cy + 60, {button: 'left', buttons: 0, clickCount: 1});
  await sleep(100);
  const z2 = await S('s.viewport');
  const depth = await S('s.viewportHistory.length');
  check('dragging a box zooms to it (one undo step)', z2.x && width(z2.x) < width(z1.x) * 0.8 && depth === 2,
    JSON.stringify({z1, z2, depth}));
  await cdp.eval(`[...document.querySelectorAll('button')].find(b => b.textContent === 'Undo zoom').click()`);
  check('Undo zoom goes back one step', await until(`Math.abs(s.viewport.x.min - ${z1.x.min}) < 1e-12`, 'undo'));
  /* pan */
  const before = await S('s.viewport.x');
  await mouse('mousePressed', cx, cy, {button: 'left', buttons: 1, clickCount: 1, modifiers: 8});
  for (let s = 1; s <= 4; s++) await mouse('mouseMoved', cx + 20 * s, cy, {button: 'left', buttons: 1, modifiers: 8});
  await mouse('mouseReleased', cx + 80, cy, {button: 'left', buttons: 0, clickCount: 1, modifiers: 8});
  await sleep(100);
  const after = await S('s.viewport.x');
  check('Shift+drag pans (same width, moved left in data)',
    after.min < before.min && Math.abs(width(after) - width(before)) < 1e-9 * width(before) + 1e-15, JSON.stringify([before, after]));
  /* double click resets */
  await mouse('mousePressed', cx, cy, {button: 'left', buttons: 1, clickCount: 1});
  await mouse('mouseReleased', cx, cy, {button: 'left', clickCount: 1});
  await mouse('mousePressed', cx, cy, {button: 'left', buttons: 1, clickCount: 2});
  await mouse('mouseReleased', cx, cy, {button: 'left', clickCount: 2});
  check("a double click goes back to the core's window", await until('s.viewport.x === null && s.viewport.y === null', 'reset'));
  await mouse('mouseMoved', 5, 5);
}

/* the values panel (docs/ui-v2.md T3): parameters, a slider, undo, layout */
async function values() {
  await cdp.send('Emulation.setDeviceMetricsOverride', {width: 1400, height: 900, deviceScaleFactor: 1, mobile: false});
  await sleep(150);
  const box = await cdp.eval(`(() => { const r = document.querySelector('.values-panel').getBoundingClientRect();
    return {left: r.left, right: r.right, top: r.top, width: r.width, winWidth: innerWidth}; })()`);
  check('at 1400px wide the values panel is a right column',
    box.width > 200 && box.right >= box.winWidth - 2 && box.top < 100, JSON.stringify(box));

  /* edit a parameter: the field, then a new value, then the next state has it */
  const field = await cdp.eval(`(() => { const l = [...document.querySelectorAll('.value-field .value-name')]
    .find(e => e.textContent.toLowerCase() === 'iapp'); return l ? l.closest('.value-field').querySelector('input').id : null; })()`);
  check('the iapp field is in the panel', !!field, String(field));
  const before = await S(`(s.core.pars.find(p => p[0].toLowerCase() === 'iapp') || [])[1]`);
  /* two round trips, not one: Preact's state update from 'input' must be flushed (a render) before blur reads it */
  await cdp.eval(`(() => { const el = document.getElementById(${JSON.stringify(field)}); el.focus();
    el.value = '0.2'; el.dispatchEvent(new Event('input', {bubbles: true})); })()`);
  await sleep(80);
  await cdp.eval(`document.getElementById(${JSON.stringify(field)}).blur()`);
  check('editing a parameter sends set: the next state has the new value',
    await until(`Math.abs((s.core.pars.find(p => p[0].toLowerCase() === "iapp") || [])[1] - 0.2) < 1e-9 && !s.busy`, 'iapp=0.2'),
    JSON.stringify(await S('s.core.pars')));

  /* Escape while editing drops the draft: nothing is sent, nothing to undo */
  const edits = await S('s.values.history.length');
  await cdp.eval(`(() => { const el = document.getElementById(${JSON.stringify(field)}); el.focus();
    el.value = '0.3'; el.dispatchEvent(new Event('input', {bubbles: true})); })()`);
  await sleep(80);
  await key('Escape');
  await sleep(300);
  check('Escape in a field cancels the edit: no set, the value stays',
    await S('s.values.history.length') === edits && !(await S('s.busy'))
    && Math.abs(await S(`(s.core.pars.find(p => p[0].toLowerCase() === "iapp") || [])[1]`) - 0.2) < 1e-9,
    JSON.stringify(await S('[s.values.history.length, s.core.pars]')));

  /* undo (Ctrl+Z with the focus still in the field): the core's state goes back */
  await cdp.eval(`document.getElementById(${JSON.stringify(field)}).focus()`);
  await key('z', 2); /* Ctrl+Z */
  check('Ctrl+Z inside the panel undoes the edit: the core state is back to the old value',
    await until(`Math.abs((s.core.pars.find(p => p[0].toLowerCase() === "iapp") || [])[1] - ${before}) < 1e-9 && !s.busy`, 'undo'),
    JSON.stringify(await S('s.core.pars')));
  check('the Undo button is disabled once the history is empty',
    await cdp.eval(`[...document.querySelectorAll('.values-header button')].find(b => b.textContent === 'Undo').disabled`));

  /* a slider by the keyboard: pick iapp for slot 0, then arrow keys move it and a new series arrives */
  await cdp.eval(`(() => { const sel = document.getElementById('slider-pick-0');
    sel.value = 'iapp'; sel.dispatchEvent(new Event('change', {bubbles: true})); })()`);
  const n0 = await S('s.seriesCount');
  await cdp.eval(`document.getElementById('slider-lo-0').value = '0'; document.getElementById('slider-lo-0')
    .dispatchEvent(new Event('input', {bubbles: true}));
    document.getElementById('slider-hi-0').value = '0.5'; document.getElementById('slider-hi-0')
    .dispatchEvent(new Event('input', {bubbles: true}));
    document.querySelector('.value-slider-range').focus(); `);
  await key('ArrowRight');
  await key('ArrowRight');
  check('a slider moved by the keyboard sends slide: a new series arrives',
    await until(`s.seriesCount > ${n0} && s.series.rows === 601 && !s.busy`, 'slide series'),
    JSON.stringify(await S('[s.seriesCount, s.series && s.series.rows]')));

  /* every control in the panel is reachable by Tab, in order, without a trap: start from the very top */
  await cdp.eval(`document.querySelector('.skip-link').focus()`);
  let steps = 0, reached = false;
  while (steps < 200) {
    await key('Tab');
    steps++;
    if (await cdp.eval(`!!document.activeElement.closest('.values-panel')`)) { reached = true; break; }
  }
  check(`Tab reaches the values panel (${steps} presses)`, reached);
}

async function keyboardOnly() {
  /* Tab from the top of the page to the plot */
  await cdp.eval('document.activeElement && document.activeElement.blur()');
  let steps = 0;
  while (steps < 60 && !(await cdp.eval(`!!document.activeElement.closest('.plot-host')`))) {
    await key('Tab');
    steps++;
  }
  check(`Tab reaches the plot (${steps} presses)`, await cdp.eval(`!!document.activeElement.closest('.plot-host')`));
  check('the focus is visible there', await cdp.eval(`getComputedStyle(document.activeElement).outlineStyle !== 'none'`));
  const w0 = (await P()).x;
  await key('+');
  const z = await S('s.viewport');
  check('+ zooms in', z.x && width(z.x) < width(w0), JSON.stringify(z));
  await key('ArrowRight');
  const z2 = await S('s.viewport');
  check('an arrow key pans', z2.x.min > z.x.min && Math.abs(width(z2.x) - width(z.x)) < 1e-12, JSON.stringify(z2));
  await key(']');
  check('] reads the first point', await until('s.hover && s.hover.row === 0', ']'));
  await key(']');
  check('] again the next one', await until('s.hover && s.hover.row === 1', ']'));
  await key('End');
  check('End the last', await until('s.hover && s.hover.row === 600', 'End'));
  await key('PageUp');
  check('PageUp ten back', await until('s.hover && s.hover.row === 590', 'PageUp'));
  check('the readout is announced (role status)', await cdp.eval(`document.querySelector('.readout').getAttribute('role') === 'status'
    && document.querySelector('.readout').textContent.includes('row 590')`));
  await key('Escape');
  check('Escape clears the readout', await until('s.hover === null', 'Escape'));
  await key('z', 2); /* Ctrl+Z */
  check('Ctrl+Z undoes the pan', await until(`Math.abs(s.viewport.x.min - ${z.x.min}) < 1e-12`, 'undo'));
  await key('0');
  check('0 resets the view', await until('s.viewport.x === null', '0'));
  /* a prompt from the plot, cancelled with Escape: focus comes back */
  await key('x');
  check('X (an XPP hotkey) still works with the plot focused', await until("s.ask && s.ask.kind === 'string'", 'x'),
    JSON.stringify(await S('s.ask')));
  check('its field has the focus', await until(`document.activeElement.tagName === 'INPUT' && document.activeElement.closest('[role=dialog]')`, 'field focus'));
  await key('Tab');
  await key('Tab');
  await key('Tab');
  check('Tab stays inside the dialog', await cdp.eval(`!!document.activeElement.closest('[role=dialog]')`));
  await key('Escape');
  check('Escape cancels it', await until('!s.ask && !s.busy', 'cancel'));
  check('and the focus is back on the plot', await until(`document.activeElement.closest('.plot-host')`, 'focus back'));
}

async function touch(type, points) {
  await cdp.send('Input.dispatchTouchEvent', {type, touchPoints: points.map((p, i) => ({x: p.x, y: p.y, id: i}))});
  await sleep(30);
}

async function phone() {
  await cdp.send('Emulation.setDeviceMetricsOverride', {width: 390, height: 844, deviceScaleFactor: 2, mobile: true});
  await cdp.send('Emulation.setTouchEmulationEnabled', {enabled: true, maxTouchPoints: 5});
  await cdp.send('Emulation.setEmulatedMedia', {features: [{name: 'pointer', value: 'coarse'}, {name: 'hover', value: 'none'}]}).catch(() => {});
  await sleep(400);
  const scroll = await cdp.eval(`({doc: document.documentElement.scrollWidth, body: document.body.scrollWidth, w: innerWidth})`);
  check('390x844: no sideways scroll', scroll.doc <= scroll.w && scroll.body <= scroll.w, JSON.stringify(scroll));
  const plot = await area();
  check('the plot fills the width', plot.w > 250 && plot.x + plot.w <= 390, JSON.stringify(plot));
  check('the menu is a closed drawer', !(await S('s.drawerOpen'))
    && await cdp.eval(`getComputedStyle(document.querySelector('.menu-panel')).visibility === 'hidden'`));
  const coarse = await cdp.eval(`matchMedia('(pointer: coarse)').matches`);
  const small = await cdp.eval(`[...document.querySelectorAll('button, .button')].filter(b => b.getClientRects().length
    && getComputedStyle(b).visibility !== 'hidden' && !b.closest('.menu-panel') && !b.closest('.values-panel'))
    .map(b => [b.textContent.trim(), b.getBoundingClientRect().height]).filter(([, h]) => h < 44)`);
  check('touch targets are at least 44px high', coarse && small.length === 0, `coarse=${coarse} ${JSON.stringify(small)}`);
  const t = await cdp.eval(`(() => { const r = document.querySelector('.menu-toggle').getBoundingClientRect();
    return {x: r.left + r.width / 2, y: r.top + r.height / 2}; })()`);
  await touch('touchStart', [t]);
  await touch('touchEnd', []);
  check('tapping Menu opens the drawer', await until('s.drawerOpen', 'drawer')
    && await cdp.eval(`getComputedStyle(document.querySelector('.menu-panel')).visibility === 'visible'`));
  check('its first command has the focus', await until(`document.activeElement.closest('.menu-panel')`, 'drawer focus'),
    await cdp.eval(`document.activeElement.outerHTML.slice(0, 120)`));
  await key('Escape');
  check('Escape closes it', await until('!s.drawerOpen', 'close drawer'));

  /* the values panel is a full-screen sheet with a Back button (R6) */
  check('the values sheet starts closed', !(await S('s.valuesOpen'))
    && await cdp.eval(`getComputedStyle(document.querySelector('.values-panel')).visibility === 'hidden'`));
  const vt = await cdp.eval(`(() => { const r = document.querySelector('.values-toggle').getBoundingClientRect();
    return {x: r.left + r.width / 2, y: r.top + r.height / 2}; })()`);
  await touch('touchStart', [vt]);
  await touch('touchEnd', []);
  check('tapping Values opens the sheet', await until('s.valuesOpen', 'values sheet')
    && await cdp.eval(`getComputedStyle(document.querySelector('.values-panel')).visibility === 'visible'`));
  check('the sheet covers the viewport', await cdp.eval(`(() => { const r = document.querySelector('.values-panel')
    .getBoundingClientRect(); return r.width >= innerWidth - 1 && r.height >= innerHeight - 1; })()`));
  check('its first control has the focus', await until(`document.activeElement.closest('.values-panel')`, 'sheet focus'));
  const sheetSmall = await cdp.eval(`[...document.querySelectorAll('.values-panel button, .values-panel select, .values-panel input')]
    .filter(b => b.getClientRects().length).map(b => [b.tagName + ':' + (b.textContent || b.id || '').trim(),
      b.getBoundingClientRect().height]).filter(([, h]) => h < 44)`);
  check('the sheet\'s own targets are at least 44px high', sheetSmall.length === 0, JSON.stringify(sheetSmall));
  await cdp.eval(`document.querySelector('.values-back').click()`);
  check('Back closes the sheet', await until('!s.valuesOpen', 'close values sheet'));
  check('the focus returns to the Values button', await until(`document.activeElement.closest('.values-toggle')`, 'values focus back'));

  /* pinch out about the middle */
  const a = await area(), cx = a.x + a.w / 2, cy = a.y + a.h / 2;
  const w0 = (await P()).x;
  await touch('touchStart', [{x: cx - 30, y: cy}, {x: cx + 30, y: cy}]);
  for (let s = 1; s <= 5; s++) await touch('touchMove', [{x: cx - 30 - 12 * s, y: cy}, {x: cx + 30 + 12 * s, y: cy}]);
  await touch('touchEnd', []);
  const z = await S('s.viewport');
  check('a pinch zooms in', z.x && width(z.x) < width(w0) * 0.6, JSON.stringify([w0, z.x]));
  if (!z.x) return;
  /* one finger pans */
  await touch('touchStart', [{x: cx, y: cy}]);
  for (let s = 1; s <= 5; s++) await touch('touchMove', [{x: cx + 10 * s, y: cy}]);
  await touch('touchEnd', []);
  const z2 = await S('s.viewport');
  check('one finger pans', z2.x.min < z.x.min && Math.abs(width(z2.x) - width(z.x)) < 1e-9 * width(z.x), JSON.stringify(z2));
  /* a tap on a point reads it */
  await cdp.eval(`document.querySelector('.plot-host').focus()`);
  await key('0');
  await until('s.viewport.x === null', 'reset');
  /* a point well inside the plot */
  const box = await area();
  let row = 100, pt = await screenOf(0, row);
  while (row < 600 && (pt.x < box.x + 30 || pt.x > box.x + box.w - 30 || pt.y < box.y + 30 || pt.y > box.y + box.h - 30)) {
    row += 10;
    pt = await screenOf(0, row);
  }
  await touch('touchStart', [pt]);
  await touch('touchEnd', []);
  check('a tap on the curve reads the point', await until(`s.hover && Math.abs(s.hover.row - ${row}) <= 5`, 'tap'),
    JSON.stringify([await S('s.hover'), pt, await area(), await cdp.eval('__xpp.actions().slice(-8)')]));
}

async function main() {
  const browser = findBrowser(opt.browser);
  if (!browser) {
    console.log('web2check: no Chrome, Chromium or Edge found (set CHROME=path); skipped');
    process.exit(0);
  }
  if (!fs.existsSync(bin)) throw new Error(`no ${bin}: build xppautX first`);
  const want = outputDat();
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'xppweb2-'));
  fs.copyFileSync(ODE, path.join(dir, 'lecar.ode'));
  const server = await startServer(bin, dir, ['lecar.ode']);
  const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'xppweb2-profile-'));
  const b = await startBrowser(browser, profile);
  cdp = b.cdp;
  try {
    await cdp.send('Page.enable');
    await cdp.send('Emulation.setDeviceMetricsOverride', {width: 1280, height: 860, deviceScaleFactor: 1, mobile: false});
    await cdp.send('Page.navigate', {url: server.url.replace('/?t=', '/v2/?t=')});
    await desktop(want);
    await values();
    await keyboardOnly();
    await phone();
    const errors = await S('s.log.filter(l => l.kind === "error").map(l => l.text)');
    check('no errors reported by the core', errors.length === 0, JSON.stringify(errors));
  } finally {
    b.proc.kill();
    server.proc.kill();
    await sleep(500);
    fs.rmSync(profile, {recursive: true, force: true, maxRetries: 5});
    fs.rmSync(dir, {recursive: true, force: true, maxRetries: 5});
  }
  console.log(`web2 checks: ${failures ? `${failures} failed` : 'all passed'}`);
  process.exit(failures ? 1 : 0);
}

main().catch(e => {
  console.error(e.stack || e);
  process.exit(2);
});
