#!/usr/bin/env node
/* State-level check of the front end (web2/, served at /): drives a headless Chrome or Edge through real
   key presses, mouse and touch events
   and asserts what the page's store and plot hold (window.__xpp), never
   pixels. A desktop session (integrate from the keyboard, the plotted
   numbers against output.dat, hover, wheel and box zoom, undo, reset, pan),
   the AUTO view (docs/ui-v2.md T11a: lecar's steady state and the periodic
   branch from its Hopf point, the store against the diagram events, the
   curves and labels drawn, the Hopf join, the readout by mouse and keys,
   zoom and undo, the sheet on a phone, Close),
   the values panel (docs/ui-v2.md T3: edit a parameter, a slider by
   keyboard, undo, Tab reachability, the panel as a right column), the data
   table (docs/ui-v2.md T10: scroll and keyboard navigation to row 500
   against output.dat, Get, CSV export, Tab reachability), keyboard-
   only use of the plot and of a prompt, text views (docs/ui-v2.md T16:
   equations, source with a comment action, equilibrium with Import, Tab
   reachability), and a phone-sized one (390x844,
   touch: no sideways scroll, the menu drawer, the values sheet, the table
   sheet, pinch, tap, pan, 44px targets). Prompts (T4): a form field as a
   select of variables, Window/Zoom by a box drawn with the mouse and by the
   keyboard only, Escape cancelling a plot mode, Initialconds/Mouse by a
   click, a checklist. Plot windows as tabs (T6: a second window, each tab
   its own zoom, arrow keys, close). Nullclines, direction field and flow
   (T7: in the store, drawn, toggled from the legend, cleared by Erase).
   Marks (T8: text with Greek, a pointer, a marker, a frozen curve and an
   equilibrium in the store, drawn, in the legend, toggled, cleared by
   Erase). 3D plots (T14, examples/ode/lorenz.ode: the store's own window,
   the projection drawn on a canvas, a drag and the arrow keys turning it
   locally at once, throttled `view3d` commands, and state.view settling
   to agree). Then live plotting (tools/models/live.ode: the store
   and the plot grow while 20 001 rows are computed, and end as output.dat)
   and a run of 10^6 rows (tools/models/million.ode) that must draw and zoom
   with no frame over 50 ms (draw times and long tasks, read through __xpp).
   Files (T5): Write set lands in the model's folder and is downloaded, Read
   set by upload restores the parameters, a same-content upload is not
   copied, a same-name one asks Replace / Keep both / Cancel, and "Add
   file…" adds a file the core could not open and runs the command again.
   Animation (T13): tools/gui_test.ani loaded by upload, its frames in the
   store in unit coordinates and drawn at the dimension's aspect; the
   keyboard's steps, Home/End, the seek slider, the delay, Play and Pause
   move the store's frame and the drawn one; a 390x844 sheet.
   Kinescope (T15): Capture after two different integrations holds two
   frames of different data, client-side Export GIF decodes to one frame
   per capture, Playback shows frame 1 then frame 2, Reset empties the
   frames, and the core's own Make Anigif (a `pixels` ask per frame) writes
   a real anim.gif into the model's folder.
   T21: AUTO's status strip names the run and its point count, the axis
   dialog sets the range during a run (its plot type waits), an Axes change
   brings the diagram in the new quantities without reDraw, Clear hides the
   branches so far, the Numerics and Axes settings round trip through a
   file, grabbing a labelled periodic point plots its limit cycle, Tab in a
   form replaces a field's text; `keys`: I then G typed at once integrate,
   F acts from the plot and from a button, the theme switch is an icon, a
   long menu is in columns; `busy`: during an integration the AUTO diagram
   zooms and its core buttons say why they wait.
   Help (docs/roadmap.md W12b, the manual rendered at build time into
   dist/manual.json and fetched by the page itself, not bundled in
   app.js): the fetch itself (served like app.js, no token), a "?" on the
   values panel opening Help at its section with the heading in view once
   loaded, search finding a known term, a result and a table-of-contents
   link navigating, an in-chapter cross-reference link too, and F1
   reopening it where it was left.

   node tools/web2check.mjs [--bin ./xppautX] [--browser PATH]
     [--only desktop,phase,marks,auto,keys,busy,view,three,aplot,files,live,million,ani,kinescope,runs,values,help] [-v]

   Needs Node 22 or later and a browser, nothing else (tools/cdp.mjs). */
import {spawnSync} from 'node:child_process';
import crypto from 'node:crypto';
import fs from 'node:fs';
import net from 'node:net';
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
const LIVE = path.join(top, 'tools/models/live.ode');
const MILLION = path.join(top, 'tools/models/million.ode');
const APLOT_ODE = path.join(top, 'examples/ode/wcring.ode');
const LORENZ_ODE = path.join(top, 'examples/ode/lorenz.ode');

let failures = 0;
function check(name, ok, detail = '') {
  console.log(`${ok ? 'PASS' : 'FAIL'} ${name}${ok ? '' : '  ' + detail}`);
  if (!ok) failures++;
}

/* output.dat of the same model: the numbers the plot must show */
function outputDat(ode = ODE) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'xppsilent-'));
  fs.copyFileSync(ode, path.join(dir, path.basename(ode)));
  spawnSync(bin, [path.basename(ode), '-silent'], {cwd: dir, stdio: 'ignore', timeout: 60000});
  const rows = fs.readFileSync(path.join(dir, 'output.dat'), 'utf8').trim().split(/\r?\n/).map(l => l.trim().split(/\s+/).map(Number));
  fs.rmSync(dir, {recursive: true, force: true});
  return rows;
}

/* ---- page helpers --------------------------------------------------------- */

let cdp;
/* in page expressions: `s` is the store's state, `w` its active plot window
   (store/plots.ts: series, viewport, viewportHistory) */
const ACTIVE = 's.plots.windows.find(x => x.win === s.plots.active) || {}';
const S = expr => cdp.eval(`(() => { const s = __xpp.state(), w = ${ACTIVE}; return ${expr}; })()`);
const P = () => cdp.eval('__xpp.plot()');

async function until(expr, what, ms = 15000) {
  const t0 = Date.now();
  for (;;) {
    let v = null;
    try {
      v = await cdp.eval(`(() => { try { const s = window.__xpp && __xpp.state(), w = ${ACTIVE}; return !!(${expr}); } catch (e) { return false; } })()`);
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
  ArrowLeft: 37, ArrowUp: 38, ArrowRight: 39, ArrowDown: 40, F1: 112};
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
async function click(x, y) {
  await mouse('mouseMoved', x, y);
  await mouse('mousePressed', x, y, {button: 'left', buttons: 1, clickCount: 1});
  await mouse('mouseReleased', x, y, {button: 'left', buttons: 0, clickCount: 1});
}
/* a click that edits nothing must send nothing and never make the store busy
   (T20: "clicking the plotting panel fires an integration") */
async function clickSendsNothing(name, x, y) {
  const pre = await S('__xpp.sent().length');
  await click(x, y);
  await sleep(200);
  const sent = await S(`__xpp.sent().slice(${pre})`);
  check(name, sent.length === 0 && !(await S('s.busy')), JSON.stringify(sent));
}

/* the plotting area's box and the screen position of point i of curve c */
const area = () => cdp.eval(`(() => { const r = document.querySelector('.plot-view:not([hidden]) .u-over').getBoundingClientRect();
  return {x: r.left, y: r.top, w: r.width, h: r.height}; })()`);
async function screenOf(curve, i) {
  const [a, p, s] = [await area(), await P(), await cdp.eval(`(() => { const s = __xpp.state(), m = (${ACTIVE}).series;
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

  /* T20: a click on the empty plot away from the Integrate button, nothing edited */
  const emptyHost = await cdp.eval(`(() => { const r = document.querySelector('.plot-host').getBoundingClientRect();
    return {x: r.left, y: r.top, w: r.width, h: r.height}; })()`);
  await clickSendsNothing('a click on the empty plot (nothing edited) sends nothing and starts nothing',
    emptyHost.x + 6, emptyHost.y + 6);

  /* integrate with the keyboard only: I opens the menu, G answers it */
  await key('i');
  check('I opens the Initialconds menu as a dialog', await until("s.ask && s.ask.kind === 'menu'", 'menu'),
    JSON.stringify(await S('s.ask')));
  check('the dialog has the focus', await until(`document.activeElement.closest('[role=dialog]')`, 'focus'));
  const n0 = await S('s.seriesCount');
  await key('g');
  check('G integrates: a new series of 601 rows', await until(`s.seriesCount > ${n0} && w.series.rows === 601 && !s.busy`, 'series'),
    JSON.stringify(await S('w.series && w.series.rows')));

  const cols = await cdp.eval(`(() => { const s = __xpp.state(), m = (${ACTIVE}).series; const o = {};
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
  await until('w.viewport.x', 'wheel');
  const z1 = await S('w.viewport');
  check('the wheel zooms in about the pointer', z1.x && width(z1.x) < width(p.x) * 0.9, JSON.stringify(z1));
  /* a box */
  await mouse('mouseMoved', cx - 80, cy - 60);
  await mouse('mousePressed', cx - 80, cy - 60, {button: 'left', buttons: 1, clickCount: 1});
  for (let s = 1; s <= 6; s++) await mouse('mouseMoved', cx - 80 + 25 * s, cy - 60 + 20 * s, {button: 'left', buttons: 1});
  await mouse('mouseReleased', cx + 70, cy + 60, {button: 'left', buttons: 0, clickCount: 1});
  await sleep(100);
  const z2 = await S('w.viewport');
  const depth = await S('w.viewportHistory.length');
  check('dragging a box zooms to it (one undo step)', z2.x && width(z2.x) < width(z1.x) * 0.8 && depth === 2,
    JSON.stringify({z1, z2, depth}));
  await cdp.eval(`[...document.querySelectorAll('button')].find(b => b.textContent === 'Undo zoom').click()`);
  check('Undo zoom goes back one step', await until(`Math.abs(w.viewport.x.min - ${z1.x.min}) < 1e-12`, 'undo'));
  /* pan */
  const before = await S('w.viewport.x');
  await mouse('mousePressed', cx, cy, {button: 'left', buttons: 1, clickCount: 1, modifiers: 8});
  for (let s = 1; s <= 4; s++) await mouse('mouseMoved', cx + 20 * s, cy, {button: 'left', buttons: 1, modifiers: 8});
  await mouse('mouseReleased', cx + 80, cy, {button: 'left', buttons: 0, clickCount: 1, modifiers: 8});
  await sleep(100);
  const after = await S('w.viewport.x');
  check('Shift+drag pans (same width, moved left in data)',
    after.min < before.min && Math.abs(width(after) - width(before)) < 1e-9 * width(before) + 1e-15, JSON.stringify([before, after]));
  /* double click resets */
  await mouse('mousePressed', cx, cy, {button: 'left', buttons: 1, clickCount: 1});
  await mouse('mouseReleased', cx, cy, {button: 'left', clickCount: 1});
  await mouse('mousePressed', cx, cy, {button: 'left', buttons: 1, clickCount: 2});
  await mouse('mouseReleased', cx, cy, {button: 'left', clickCount: 2});
  check("a double click goes back to the core's window", await until('w.viewport.x === null && w.viewport.y === null', 'reset'));
  await mouse('mouseMoved', 5, 5);

  /* T20: a click on the drawn canvas, on empty space and on the legend must
     never start a run by itself; last, since toggling the legend can leave
     uPlot's own scale null until the next redraw */
  await clickSendsNothing('a click on the drawn curve sends nothing and starts nothing', cx, cy);
  const corner = await cdp.eval(`(() => { const r = document.querySelector('.plot-host').getBoundingClientRect();
    return {x: r.left + 6, y: r.top + 6}; })()`);
  await clickSendsNothing('a click on empty space in the plot host sends nothing', corner.x, corner.y);
  const legendBox = await cdp.eval(`(() => { const r = document.querySelector('.legend-item').getBoundingClientRect();
    return {x: r.left, y: r.top, w: r.width, h: r.height}; })()`);
  await clickSendsNothing('a click on the legend sends nothing (it only toggles the curve)',
    legendBox.x + legendBox.w / 2, legendBox.y + legendBox.h / 2);
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

  /* "Run on change" must fire on commit only (Enter/Tab/blur), never per keystroke */
  await cdp.eval(`document.getElementById(${JSON.stringify(field)}).focus()`);
  await sleep(100);
  const preType = await S('__xpp.sent().length');
  for (const text of ['1', '16', '165']) {
    await cdp.eval(`(() => { const el = document.getElementById(${JSON.stringify(field)});
      el.value = ${JSON.stringify(text)}; el.dispatchEvent(new Event('input', {bubbles: true})); })()`);
    await sleep(150);
  }
  const whileTyping = await S(`__xpp.sent().slice(${preType})`);
  check('typing into a field sends nothing until it commits', whileTyping.length === 0, JSON.stringify(whileTyping));
  await cdp.eval(`document.getElementById(${JSON.stringify(field)}).blur()`);
  await sleep(300);
  const afterCommit = await S(`__xpp.sent().slice(${preType})`);
  check('blurring after typing sends exactly one set',
    afterCommit.length === 1 && afterCommit[0].cmd === 'set' && afterCommit[0].text === '165', JSON.stringify(afterCommit));
  /* undo this probe edit so the history below is exactly what it expects (one edit: 0.2) */
  await cdp.eval(`document.getElementById(${JSON.stringify(field)}).focus()`);
  await key('z', 2);
  await until(`Math.abs((s.core.pars.find(p => p[0].toLowerCase() === "iapp") || [])[1] - 0.2) < 1e-9 && !s.busy`, 'undo probe');

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

  /* T20 bug (the maintainer's report: "clicking the plotting panel fires
     an integration"): a field must not resend a stale draft when its
     value moves under it while it is still focused, only on a genuine
     edit. Edit iapp, refocus the field, Undo it with the keyboard (the
     field keeps the focus while the value changes under it, unlike a
     click elsewhere which would itself blur first), then click the plot,
     which was never touched: nothing must be sent. Before the fix,
     ValueField compared the draft to the *live* value at blur, so the
     untouched, now-stale draft (still the pre-undo text) differed from
     the moved-on live value and was resent as if it were a fresh edit. */
  await cdp.eval(`(() => { const el = document.getElementById(${JSON.stringify(field)}); el.focus();
    el.value = '0.3'; el.dispatchEvent(new Event('input', {bubbles: true})); })()`);
  await sleep(80);
  await cdp.eval(`document.getElementById(${JSON.stringify(field)}).blur()`);
  await until('Math.abs((s.core.pars.find(p => p[0].toLowerCase() === "iapp") || [])[1] - 0.3) < 1e-9 && !s.busy', 'iapp=0.3');
  await cdp.eval(`document.getElementById(${JSON.stringify(field)}).focus()`);
  await sleep(80);
  await key('z', 2); /* Ctrl+Z: iapp moves back to 0.05 while the field keeps the focus */
  await until('Math.abs((s.core.pars.find(p => p[0].toLowerCase() === "iapp") || [])[1] - 0.05) < 1e-9 && !s.busy', 'undo to 0.05');
  const preBlur = await S('__xpp.sent().length');
  const plotCorner = await cdp.eval(`(() => { const r = document.querySelector('.plot-host').getBoundingClientRect();
    return {x: r.left + 6, y: r.top + 6}; })()`);
  await mouse('mousePressed', plotCorner.x, plotCorner.y, {button: 'left', buttons: 1, clickCount: 1});
  await mouse('mouseReleased', plotCorner.x, plotCorner.y, {button: 'left', buttons: 0, clickCount: 1});
  await sleep(300);
  const afterBlur = await S(`__xpp.sent().slice(${preBlur})`);
  const iappNow = await S(`(s.core.pars.find(p => p[0].toLowerCase() === 'iapp') || [])[1]`);
  check('a field kept focused through an external change (Undo) does not resend its stale draft when the plot is clicked',
    afterBlur.length === 0 && Math.abs(iappNow - 0.05) < 1e-9, JSON.stringify({afterBlur, iappNow}));

  /* T20: Add slider opens a dialog; search narrows the list, picking iapp
     shows it with its current value, Min/Max/Step default from it
     (defaultRange/defaultStep), and OK is enabled once it validates */
  await cdp.eval(`document.querySelector('.slider-add').click()`);
  await until(`document.querySelector('.slider-picker-search input')`, 'slider dialog open');
  const okBtn = () => `[...document.querySelectorAll('.dialog-actions button')].find(b => b.textContent === 'OK')`;
  check('OK starts disabled: nothing is picked yet', await cdp.eval(`${okBtn()}.disabled`));
  await cdp.eval(`(() => { const el = document.querySelector('.slider-picker-search input');
    el.value = 'iapp'; el.dispatchEvent(new Event('input', {bubbles: true})); })()`);
  await until(`document.querySelectorAll('.slider-picker-item').length === 1
    && document.querySelector('.slider-picker-item').textContent.includes('iapp')`, 'search narrows to iapp');
  await cdp.eval(`document.querySelector('.slider-picker-item').click()`);
  check('picking iapp shows its current value and fills Min/Max/Step from it',
    await cdp.eval(`document.querySelector('.slider-picker-picked').textContent.includes('0.05')
      && document.querySelector('.slider-dialog-fields input').value === '0'`),
    await cdp.eval(`document.querySelector('.slider-picker-picked')?.textContent`));
  check('OK is enabled once a valid range and step are picked', !(await cdp.eval(`${okBtn()}.disabled`)));
  await setSliderDialogFields({Min: '0', Max: '0.5', Step: '0.01'});
  const n0 = await S('s.seriesCount');
  await cdp.eval(`${okBtn()}.click()`);
  await until('s.values.sliders.length > 0', 'slider added');
  const sid = await S('s.values.sliders[s.values.sliders.length - 1].id');
  check('the added slider has the dialog\'s range and step',
    await S(`(() => { const d = s.values.sliders.find(x => x.id === ${sid}); return d.lo === '0' && d.hi === '0.5' && d.step === '0.01'; })()`));
  check('the value box respects the step attribute',
    (await cdp.eval(`document.getElementById('slider-val-${sid}').step`)) === '0.01');
  const valLimits = await cdp.eval(`[document.getElementById('slider-val-${sid}').min, document.getElementById('slider-val-${sid}').max]`);
  check("the value box's min and max come from the dialog's range", JSON.stringify(valLimits) === '["0","0.5"]', JSON.stringify(valLimits));

  /* the edit icon reopens the dialog prefilled, and changes take effect */
  await cdp.eval(`document.querySelector('[data-slider="${sid}"] .slider-card-edit').click()`);
  await until(`document.querySelector('.slider-picker-picked')?.textContent.includes('iapp')`, 'edit dialog prefilled');
  const prefilled = await cdp.eval(`[...document.querySelectorAll('.slider-dialog-fields input')].map(i => i.value)`);
  check('the edit dialog is prefilled with the slider\'s own range and step',
    JSON.stringify(prefilled) === JSON.stringify(['0', '0.5', '0.01']), JSON.stringify(prefilled));
  await setSliderDialogFields({Min: '0', Max: '0.2'});
  await cdp.eval(`${okBtn()}.click()`);
  await until(`(() => { const d = s.values.sliders.find(x => x.id === ${sid}); return d && d.hi === '0.2'; })()`, 'edited');

  /* a slider moved by the keyboard: arrow keys on the range track send slide, a new series arrives */
  await cdp.eval(`document.getElementById('slider-range-${sid}').focus()`);
  await key('ArrowRight');
  await key('ArrowRight');
  check('a slider moved by the keyboard sends slide: a new series arrives',
    await until(`s.seriesCount > ${n0} && w.series.rows === 601 && !s.busy`, 'slide series'),
    JSON.stringify(await S('[s.seriesCount, w.series && w.series.rows]')));
  await cdp.eval(`document.querySelector('[data-slider="${sid}"] .slider-card-remove').click()`);
  check('a slider is removed by its button', await until(`!s.values.sliders.some(d => d.id === ${sid})`, 'remove'));

  /* T20: the slider grid is responsive (docs/ui-v2.md T20): 3 per row at
     1280px (laptop), 2 on a tablet, 1 on a phone */
  const gridIds = [await addSlider('iapp'), await addSlider('phi'), await addSlider('v1')];
  const gridRows = async () => {
    const boxes = await cdp.eval(`[...document.querySelectorAll('.slider-card')].map(e => Math.round(e.getBoundingClientRect().top))`);
    return {count: boxes.length, rows: new Set(boxes).size};
  };
  await cdp.send('Emulation.setDeviceMetricsOverride', {width: 1280, height: 860, deviceScaleFactor: 1, mobile: false});
  await sleep(150);
  let g = await gridRows();
  check('3 sliders sit in one row at 1280px wide', g.count === 3 && g.rows === 1, JSON.stringify(g));
  await cdp.send('Emulation.setDeviceMetricsOverride', {width: 800, height: 860, deviceScaleFactor: 1, mobile: false});
  await sleep(150);
  g = await gridRows();
  check('3 sliders wrap 2 per row on a tablet (48rem)', g.count === 3 && g.rows === 2, JSON.stringify(g));
  await cdp.send('Emulation.setDeviceMetricsOverride', {width: 500, height: 860, deviceScaleFactor: 1, mobile: false});
  await sleep(150);
  g = await gridRows();
  check('3 sliders stack one per row on a phone', g.count === 3 && g.rows === 3, JSON.stringify(g));
  await cdp.send('Emulation.setDeviceMetricsOverride', {width: 1400, height: 900, deviceScaleFactor: 1, mobile: false});
  await sleep(150);
  for (const id of gridIds) await cdp.eval(`document.querySelector('[data-slider="${id}"] .slider-card-remove')?.click()`);
  await until(`!s.values.sliders.some(d => ${JSON.stringify(gridIds)}.includes(d.id))`, 'grid sliders removed');

  /* T20: the dialog is keyboard-only reachable too: type to filter, Enter
     picks the highlighted candidate, Tab to OK, Enter activates it */
  await cdp.eval(`document.querySelector('.slider-add').click()`);
  await until(`document.querySelector('.slider-picker-search input') === document.activeElement`, 'search has the focus');
  await cdp.eval(`(() => { const el = document.querySelector('.slider-picker-search input');
    el.value = 'v1'; el.dispatchEvent(new Event('input', {bubbles: true})); })()`);
  await until(`document.querySelectorAll('.slider-picker-item').length === 1`, 'search narrows to v1');
  await key('Enter');
  check('Enter on the search field picks the highlighted candidate',
    await until(`document.querySelector('.slider-picker-picked')?.textContent.includes('v1')`, 'picked by Enter'));
  let tabs = 0, atOk = false;
  while (tabs < 10 && !atOk) {
    await key('Tab');
    tabs++;
    atOk = await cdp.eval(`document.activeElement.textContent === 'OK'`);
  }
  check(`Tab from the search field reaches OK (${tabs} presses)`, atOk);
  const beforeKb = await S('s.values.sliders.length');
  /* activate the focused OK button (a plain click stands in for Enter/Space
     here: CDP's synthetic key events do not trigger a browser's native
     button activation the way a real keypress does); Tab reaching it is
     what this checks for keyboard use */
  await cdp.eval('document.activeElement.click()');
  await until(`s.values.sliders.length > ${beforeKb}`, 'added by keyboard');
  check('the dialog can be completed by the keyboard alone: a v1 slider was added',
    (await S('s.values.sliders.length')) > beforeKb && (await S('s.values.sliders[s.values.sliders.length - 1].name')) === 'v1');
  const kbId = await S('s.values.sliders[s.values.sliders.length - 1].id');
  await cdp.eval(`document.querySelector('[data-slider="${kbId}"] .slider-card-remove').click()`);
  await until(`!s.values.sliders.some(d => d.id === ${kbId})`, 'keyboard slider removed');

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

/* the data table (docs/ui-v2.md T10): open it, scroll to row 500 by
   scrolling and by keyboard, check its values against output.dat, Get,
   CSV export, and Tab reachability of every button */
async function dataTable(want) {
  await cdp.send('Emulation.setDeviceMetricsOverride', {width: 1400, height: 900, deviceScaleFactor: 1, mobile: false});
  await sleep(150);
  await cdp.eval(`document.querySelector('.table-toggle').click()`);
  check('the Data button opens the table panel', await until('s.table.open', 'table open')
    && await cdp.eval(`getComputedStyle(document.querySelector('.table-panel')).visibility === 'visible'`));
  check('its first control has the focus', await until(`document.activeElement.closest('.table-panel')`, 'table focus'));
  check('the table asks for rows and gets T plus every column',
    await until('s.table.page && s.table.page.cols.length >= 2', 'table page'),
    JSON.stringify(await S('s.table.page && s.table.page.cols')));

  /* output.dat prints 8 digits, the table the exact float (9): they agree to 1e-7 */
  const tol = (v, w) => Math.abs(v - w) <= 1e-7 * Math.abs(w) + 1e-30;
  const rowOk = (row, want500) => row && row.every((v, i) => tol(v, want500[i]));

  /* scroll to row 500 with the scrollbar */
  await cdp.eval(`(() => { const el = document.querySelector('.table-grid-wrap');
    const h = document.querySelector('.table-row-head').getBoundingClientRect().height;
    el.scrollTop = 500 * h; el.dispatchEvent(new Event('scroll')); })()`);
  check('scrolling to row 500 fetches it', await until(
    '(() => { const p = s.table.page; return p && p.from <= 500 && p.from + p.data.length > 500; })()', 'row 500 scrolled'));
  let row500 = await cdp.eval(`(() => { const p = __xpp.state().table.page; return p.data[500 - p.from]; })()`);
  check("the displayed row 500 (scrolled) equals output.dat's row 500", rowOk(row500, want[500]),
    JSON.stringify({row500, want: want[500]}));

  /* the same row, reached from the keyboard only: End, then Up to 500 */
  await cdp.eval(`document.querySelector('.table-grid-wrap').focus()`);
  await key('End');
  check('End selects the last row', await until(`s.table.selected === ${want.length - 1}`, 'End'));
  for (let i = 0; i < want.length - 1 - 500; i++) await key('ArrowUp');
  check('keyboard navigation (End, Up) reaches row 500',
    await until('s.table.selected === 500', 'row 500 by keyboard'));
  row500 = await cdp.eval(`(() => { const s = __xpp.state(); const p = s.table.page;
    return p && p.from <= 500 && p.from + p.data.length > 500 ? p.data[500 - p.from] : null; })()`);
  check("the displayed row 500 (keyboard) equals output.dat's row 500", rowOk(row500, want[500]),
    JSON.stringify({row500, want: want[500]}));

  /* Get: the selected row (500) becomes the initial conditions */
  await cdp.eval(`[...document.querySelectorAll('.table-tools button')].find(b => b.textContent === 'Get').click()`);
  check("Get sets the ICs from the selected row: the next state has them", await until(
    `(() => { const ics = s.core.ics.map(p => p[1]); return Math.abs(ics[0] - ${want[500][1]}) < 1e-6 && !s.busy; })()`,
    'Get'), JSON.stringify(await S('s.core.ics')));

  /* Enter is Get too, on whatever row is selected (a step away, so the check is meaningful) */
  await cdp.eval(`document.querySelector('.table-grid-wrap').focus()`);
  await key('ArrowUp');
  await key('Enter');
  check('Enter on the grid is Get, for row 499', await until(
    `(() => { const ics = s.core.ics.map(p => p[1]); return Math.abs(ics[0] - ${want[499][1]}) < 1e-6 && !s.busy; })()`,
    'Enter=Get'), JSON.stringify(await S('s.core.ics')));

  /* CSV export: every stored row, fetched block by block (no download needed to check it) */
  await cdp.eval(`document.querySelector('.table-header .small').click()`);
  check('Export CSV records the exported text', await until('!!s.table.lastExport && !s.table.exporting', 'csv'));
  const csv = (await S('s.table.lastExport')).trim().split('\n');
  const r500 = csv[501] ? csv[501].split(',').map(Number) : [];
  check('the exported CSV has the header and all 601 rows, row 500 as in output.dat',
    csv.length === 602 && r500.length >= 3 && r500.every((v, k) => Math.abs(v - want[500][k]) <= 1e-7 * Math.abs(want[500][k]) + 1e-30),
    `${csv.length} lines, row 500: ${csv[501]} vs ${want[500]}`);
  check('the table asks for its own rows again after the export',
    await until('s.table.page && s.table.page.from <= s.table.selected && s.table.selected < s.table.page.from + s.table.page.data.length', 'refetch'));

  /* every button (and the grid) reachable by Tab */
  await cdp.eval(`document.querySelector('.skip-link').focus()`);
  let steps = 0, reached = false;
  while (steps < 200) {
    await key('Tab');
    steps++;
    if (await cdp.eval(`!!document.activeElement.closest('.table-panel')`)) { reached = true; break; }
  }
  check(`Tab reaches the table panel (${steps} presses)`, reached);
  const labels = new Set();
  for (let i = 0; i < 20 && await cdp.eval(`!!document.activeElement.closest('.table-panel')`); i++) {
    const t = await cdp.eval(`(document.activeElement && document.activeElement.textContent || '').trim()`);
    if (t) labels.add(t);
    await key('Tab');
  }
  const expect = ['Back', 'Export CSV', 'Find', 'Get', 'Replace', 'Unrepl', 'Table', 'Load', 'Write', 'First',
    'Last', 'Restore', 'Add col', 'Del col'];
  check('every button in the table panel is reachable by Tab', expect.every(l => labels.has(l)), JSON.stringify([...labels]));

  /* leave it closed for the phone session */
  await cdp.eval(`document.querySelector('.table-back').click()`);
  check('Back closes the table panel', await until('!s.table.open', 'close table'));
}

/* text views (docs/ui-v2.md T16, docs/protocol.md `equations`, `source`,
   `action`, `equilibrium`, `eqimport`): equations, source with a comment
   action, equilibrium with Import, and Tab reachability. lecar.ode's own
   tutorial ("To set parameters click on the asterisks") is the model with
   comment actions the task asks for: six `"..{name=value,...}"` lines, each
   an X11 "Action view" button. This picks the second one ({gk=0}) rather
   than the first ({total=100,iapp=.1}), so it leaves TOTAL (and so the
   601-row runs later windows() and prompts() still expect) alone. */
async function textViews() {
  await cdp.send('Emulation.setDeviceMetricsOverride', {width: 1400, height: 900, deviceScaleFactor: 1, mobile: false});
  await sleep(150);
  await cdp.eval(`document.querySelector('.text-toggle').click()`);
  check('the Text button opens the panel on Equations', await until("s.text.open && s.text.tab === 'equations'", 'text open')
    && await cdp.eval(`getComputedStyle(document.querySelector('.text-panel')).visibility === 'visible'`));
  check('its first control has the focus', await until(`document.activeElement.closest('.text-panel')`, 'text focus'));

  /* Equations: the model's own dV/dT, dW/dT lines from the `equations` event */
  check('opening it asks for the equations', await until('s.text.equations && s.text.equations.length >= 2', 'equations'),
    JSON.stringify(await S('s.text.equations')));
  check('the equations view lists them (wrapped, monospace)', await cdp.eval(
    `/d[vw]\\/dt=/i.test(document.querySelector('.text-equations').textContent)`),
    await cdp.eval(`document.querySelector('.text-equations').textContent`));

  /* Equilibrium: Sing pts/Go, then Import -- at the ODE file's own defaults,
     before the comment action below changes a parameter (gk=0 makes the
     model's dynamics degenerate along one variable and Newton's method
     does not reliably converge from the default ICs) */
  await cdp.eval(`[...document.querySelectorAll('.text-tab')].find(b => b.textContent === 'Equilibrium').click()`);
  check('no equilibrium yet', await cdp.eval(`document.querySelector('.text-equilibrium .text-empty') !== null`));
  await cdp.eval(`[...document.querySelectorAll('.text-tools button')].find(b => b.textContent === 'Find equilibrium').click()`);
  check('Find equilibrium (Sing pts/Go) computes one: type, counts and values from the equilibrium event',
    await until(`s.text.equilibrium && /STABLE|UNSTABLE|NEUTRAL/.test(s.text.equilibrium.type)
      && s.text.equilibrium.values.length === 2 && !s.busy`, 'equilibrium', 20000),
    JSON.stringify(await S('s.text.equilibrium')));
  const eq = await S('s.text.equilibrium');
  check('the view shows the type and the values (six significant digits)',
    await cdp.eval(`document.querySelector('.text-equilibrium .eq-type').textContent === ${JSON.stringify(eq.type)}
      && document.querySelectorAll('.text-equilibrium .eq-values tbody tr').length >= 2`));
  check('and its eigenvalues, one row per variable',
    eq.eigenvalues && eq.eigenvalues.length === 2 && await cdp.eval(`[...document.querySelectorAll('.text-equilibrium .eq-values')]
      .find(t => t.querySelector('caption').textContent === 'Eigenvalues').querySelectorAll('tbody tr').length === 2`),
    JSON.stringify(eq.eigenvalues));
  await cdp.eval(`[...document.querySelectorAll('.text-tools button')].find(b => b.textContent === 'Import').click()`);
  const wantIcs = eq.values.map(([, v]) => v);
  check('Import (eqimport) makes the equilibrium the initial conditions',
    await until(`(() => { const ics = s.core.ics.map(p => p[1]);
      return ics.length === ${wantIcs.length} && ics.every((v, i) => Math.abs(v - (${JSON.stringify(wantIcs)})[i]) < 1e-6); })() && !s.busy`,
      'eqimport'), JSON.stringify({ics: await S('s.core.ics'), want: wantIcs}));

  /* Source: File/Prt src, with a button on the comment actions' lines */
  await cdp.eval(`[...document.querySelectorAll('.text-tab')].find(b => b.textContent === 'Source').click()`);
  check('picking Source asks for it (File/Prt src)', await until('s.text.source && s.text.source.lines.length > 10', 'source'),
    JSON.stringify(await S('s.text.source && s.text.source.lines.length')));
  check('the source view renders its comment actions as buttons',
    await until(`document.querySelectorAll('.source-action').length >= 6`, 'source actions rendered'));
  const before = await S(`(s.core.pars.find(p => p[0].toLowerCase() === 'gk') || [])[1]`);
  check('gk starts at its ODE-file default (2)', Math.abs(before - 2) < 1e-9, String(before));
  await cdp.eval(`document.querySelectorAll('.source-action')[1].click()`); /* "{gk=0}" */
  check('choosing a comment action sends action: the next state has its parameters (gk=0)',
    await until(`Math.abs((s.core.pars.find(p => p[0].toLowerCase() === "gk") || [])[1]) < 1e-9 && !s.busy`, 'gk=0'),
    JSON.stringify(await S('s.core.pars')));

  /* every control in the panel (Back, the three tabs, and the shown view's
     own controls) is reachable by Tab, in order, without a trap */
  await cdp.eval(`document.querySelector('.skip-link').focus()`);
  let steps = 0, reached = false;
  while (steps < 200) {
    await key('Tab');
    steps++;
    if (await cdp.eval(`!!document.activeElement.closest('.text-panel')`)) { reached = true; break; }
  }
  check(`Tab reaches the text panel (${steps} presses)`, reached);
  const labels = new Set();
  let sawAction = false;
  for (let i = 0; i < 30 && await cdp.eval(`!!document.activeElement.closest('.text-panel')`); i++) {
    const t = await cdp.eval(`(document.activeElement && document.activeElement.textContent || '').trim()`);
    if (t) labels.add(t);
    if (await cdp.eval(`document.activeElement.classList.contains('source-action')`)) sawAction = true;
    await key('Tab');
  }
  check('Back and the three tabs are reachable by Tab',
    ['Back', 'Equations', 'Source', 'Equilibrium'].every(l => labels.has(l)), JSON.stringify([...labels]));
  check('a comment action button is reachable by Tab too (the shown view\'s own controls)', sawAction);

  /* leave it closed for the phone session */
  await cdp.eval(`document.querySelector('.text-back').click()`);
  check('Back closes the text panel', await until('!s.text.open', 'close text'));
}

async function keyboardOnly() {
  /* Tab from the top of the page to the plot */
  await cdp.eval('document.activeElement && document.activeElement.blur()');
  let steps = 0;
  while (steps < 100 && !(await cdp.eval(`!!document.activeElement.closest('.plot-host')`))) {
    await key('Tab');
    steps++;
  }
  check(`Tab reaches the plot (${steps} presses)`, await cdp.eval(`!!document.activeElement.closest('.plot-host')`));
  check('the focus is visible there', await cdp.eval(`getComputedStyle(document.activeElement).outlineStyle !== 'none'`));
  const w0 = (await P()).x;
  await key('+');
  const z = await S('w.viewport');
  check('+ zooms in', z.x && width(z.x) < width(w0), JSON.stringify(z));
  await key('ArrowRight');
  const z2 = await S('w.viewport');
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
  check('Ctrl+Z undoes the pan', await until(`Math.abs(w.viewport.x.min - ${z.x.min}) < 1e-12`, 'undo'));
  await key('0');
  check('0 resets the view', await until('w.viewport.x === null', '0'));
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

/* nullclines, the direction field and flows as data (docs/ui-v2.md T7): the
   store holds the events' numbers for the active window, the chart draws
   them (its own record: what the last draw drew of each layer), the legend
   shows and hides each, and Erase clears them */
async function phasePlane() {
  check('T7: the page connects and asks for nullclines and dfield', await until('s.hello && s.seriesCount >= 1 && !s.busy', 'hello')
    && await cdp.eval(`__xpp.sent().some(c => c.cmd === 'data' && c.events.includes('nullclines') && c.events.includes('dfield'))`));
  const layer = key => cdp.eval(`(__xpp.plot().layers.find(l => l.key === '${key}') || null)`);
  const legend = label => cdp.eval(`(() => { const b = [...document.querySelectorAll('.plot-view:not([hidden]) .legend-item.layer')]
    .find(b => b.textContent.trim() === ${JSON.stringify(label)}); if (b) b.click(); return b ? b.getAttribute('aria-pressed') : null; })()`);
  const answerValue = v => cdp.eval(`__xpp.send({cmd: 'answer', id: __xpp.state().ask.id, value: '${v}'})`);

  await menuKeys('n', 'n'); /* Nullcline/New */
  check('Nullcline/New: the store holds both nullclines of the active window',
    await until('!s.busy && w.nullclines && w.nullclines.x.length > 0 && w.nullclines.y.length > 0', 'nullclines')
    && await S("w.nullclines.xName === 'V' && w.nullclines.yName === 'W' && w.nullclines.x.length % 4 === 0"),
    JSON.stringify(await S('w.nullclines && [w.nullclines.xName, w.nullclines.x.length, w.nullclines.y.length]')));
  const segs = await S('[w.nullclines.x.length / 4, w.nullclines.y.length / 4]');
  let v = await layer('xnull'), wv = await layer('ynull');
  check('... the chart draws every segment of each, named V-nullcline and W-nullcline in the legend',
    v && wv && v.label === 'V-nullcline' && wv.label === 'W-nullcline' && v.drawn === segs[0] && wv.drawn === segs[1]
    && v.visible && await cdp.eval(`[...document.querySelectorAll('.legend-item.layer')].map(b => b.textContent.trim()).join()`) === 'V-nullcline,W-nullcline',
    JSON.stringify({v, wv, segs}));

  await menuKeys('d', 's'); /* Dir.field/flow, Scaled Dir.Fld */
  if (await until("s.ask && s.ask.kind === 'string'", 'grid')) await answerValue('16');
  check('Dir.field/Scaled: the store holds a 17 x 17 grid of unit directions',
    await until('!s.busy && w.dfield && w.dfield.n === 17 && w.dfield.grid.length === 4 * 289', 'dfield')
    && await S('w.dfield.scaled && w.dfield.speed.length === 289'), JSON.stringify(await S('w.dfield && [w.dfield.n, w.dfield.grid.length]')));
  v = await layer('dfield');
  check('... drawn as 289 arrows, "Direction field" in the legend', v && v.label === 'Direction field' && v.drawn === 289 && v.visible,
    JSON.stringify(v));
  check('... and the nullclines drawn again with it', (await layer('xnull'))?.drawn === segs[0]);

  let pressed = await legend('Direction field');
  v = await layer('dfield');
  check('the legend hides the direction field: not drawn, the toggle not pressed, the nullclines still drawn',
    pressed === 'true' && v && !v.visible && v.drawn === 0 && (await layer('xnull')).drawn === segs[0]
    && await cdp.eval(`[...document.querySelectorAll('.legend-item.layer')].find(b => b.textContent.trim() === 'Direction field').getAttribute('aria-pressed')`) === 'false',
    JSON.stringify(v));
  await legend('Direction field');
  await legend('V-nullcline');
  v = await layer('dfield');
  const x = await layer('xnull');
  check('... and shows it again; V-nullcline hides on its own',
    v.visible && v.drawn === 289 && !x.visible && x.drawn === 0 && (await layer('ynull')).drawn === segs[1], JSON.stringify({v, x}));
  await legend('V-nullcline');

  await menuKeys('d', 'f'); /* Dir.field/flow, Flow */
  if (await until("s.ask && s.ask.kind === 'string'", 'grid')) await answerValue('5');
  check('Dir.field/Flow: the store holds the trajectories (72 of them, NaN between two)',
    await until('!s.busy && w.dfield && w.dfield.flows.length === 1 && w.dfield.flows[0].xs.length > 72', 'flow', 30000)
    && await S('Array.from(w.dfield.flows[0].xs).filter(v => v !== v).length === 71'),
    JSON.stringify(await S('w.dfield && w.dfield.flows.map(f => f.xs.length)')));
  v = await layer('flow');
  check('... drawn, "Flow" in the legend, the field still there', v && v.label === 'Flow' && v.count === 72 && v.drawn > 72
    && (await layer('dfield'))?.drawn === 289, JSON.stringify(v));

  await key('e'); /* Erase */
  check('Erase clears them: nothing in the store, nothing drawn, no legend entries',
    await until('!s.busy && w.nullclines && w.nullclines.x.length === 0 && w.dfield && w.dfield.n === 0 && w.dfield.flows.length === 0', 'erase')
    && (await P()).layers.length === 0 && await cdp.eval(`document.querySelectorAll('.legend-item.layer').length === 0`));
}

/* marks (docs/ui-v2.md T8): Text,etc's text, pointer and marker, a frozen
   curve and a Sing pts equilibrium in the store, drawn, named in the legend
   and toggled from it; Erase clears them */
async function marks() {
  check('T8: the page connects and asks for marks', await until('s.hello && s.seriesCount >= 1 && !s.busy', 'hello')
    && await cdp.eval(`__xpp.sent().some(c => c.cmd === 'data' && c.events.includes('marks'))`));
  const layer = k => cdp.eval(`(__xpp.plot().layers.find(l => l.key === '${k}') || null)`);
  const legendLabels = () => cdp.eval(`[...document.querySelectorAll('.plot-view:not([hidden]) .legend-item.layer')].map(b => b.textContent.trim())`);
  const legend = label => cdp.eval(`(() => { const b = [...document.querySelectorAll('.plot-view:not([hidden]) .legend-item.layer')]
    .find(b => b.textContent.trim() === ${JSON.stringify(label)}); if (b) b.click(); return b ? b.getAttribute('aria-pressed') : null; })()`);
  let askId = -1;
  /* the next ask of `kind` (not the one answered last), answered with `reply` */
  const answer = async (kind, reply) => {
    if (!(await until(`s.ask && s.ask.kind === '${kind}' && s.ask.id !== ${askId}`, kind))) return false;
    askId = await S('s.ask.id');
    await cdp.eval(`__xpp.send(Object.assign({cmd: 'answer', id: ${askId}}, ${JSON.stringify(reply)}))`);
    return true;
  };
  const menu = async (...keys) => {
    for (const k of keys) await answer('menu', {key: k});
  };
  const idle = () => until('!s.busy && !s.ask', 'idle', 30000);

  await focusPlot();
  await key('i');
  await menu('g');
  await idle();
  check('... an integration: no marks yet', await until('w.series && w.series.rows > 2', 'series')
    && await S('!w.marks || (w.marks.text.length + w.marks.equilibria.length + w.marks.frozen.length === 0)'));

  await key('t');
  await menu('t');
  await answer('string', {value: '\\1a\\0-point'});
  await answer('string', {value: '3'});
  await answer('mouse', {xd: -0.3, yd: 0.5});
  await idle();
  check('Text,etc/Text: the store holds the text, Greek as Unicode', await until("w.marks && w.marks.text.length === 1", 'text')
    && await S("w.marks.text[0].plain === 'α-point' && w.marks.text[0].size === 3 && Math.abs(w.marks.text[0].x + 0.3) < 0.01"),
    JSON.stringify(await S('w.marks && w.marks.text')));
  let t = await layer('text');
  check('... drawn as text, "Text" in the legend, named in the plot\'s label', t && t.drawn === 1 && t.visible
    && (await legendLabels()).includes('Text')
    && (await cdp.eval(`document.querySelector('.plot-view:not([hidden]) .plot-host').getAttribute('aria-label')`)).includes('α-point'),
    JSON.stringify(t));

  await key('t');
  await menu('p');
  await answer('string', {value: '0.2'});
  await answer('string', {value: '5'});
  await answer('rubber', {xd: 0.1, yd: 0.2, xd2: 0.6, yd2: 0.8});
  await idle();
  await key('t');
  await menu('m');
  await answer('form', {values: ['3', '7', '2']});
  await answer('mouse', {xd: 0.9, yd: 0.1});
  await idle();
  check('Pointer and Marker: in the store, drawn, "Arrows" and "Markers" in the legend',
    await until('w.marks && w.marks.arrows.length === 1 && w.marks.markers.length === 1', 'objects')
    && await S("w.marks.arrows[0].pointer && w.marks.arrows[0].color === 5 && w.marks.markers[0].shape === 'diamond'")
    && (await layer('arrows'))?.drawn === 1 && (await layer('markers'))?.drawn === 1
    && (await legendLabels()).join() === 'Text,Arrows,Markers', JSON.stringify(await S('w.marks')));

  await key('g');
  await menu('f', 'f');
  await answer('form', {values: ['4', 'first run', 'frz1']});
  await idle();
  check('Graphic stuff/Freeze: the frozen curve in the store equals the series, named by its key',
    await until('w.marks && w.marks.frozen.length === 1', 'frozen')
    && await S(`(() => { const f = w.marks.frozen[0], c = w.series.curves[0];
      const x = w.series.columns.get(c.x), y = w.series.columns.get(c.y);
      return f.label === 'first run' && f.color === 4 && f.xs.length === x.length
        && f.xs.every((v, i) => v === x[i]) && f.ys.every((v, i) => v === y[i]); })()`),
    JSON.stringify(await S('w.marks && w.marks.frozen.map(f => [f.label, f.xs.length])')));
  const frozen = await layer('frozen-0');
  check('... drawn under the curves, its key in the legend', frozen && frozen.label === 'first run' && frozen.drawn > 2
    && (await legendLabels()).includes('first run'), JSON.stringify(frozen));

  await key('s');
  await menu('g');
  for (let k = 0; k < 6 && !(await until('!s.busy', 'sing pts', 800)); k++)
    if (await S("s.ask && s.ask.kind === 'choice'")) await answer('choice', {key: 'n'});
  await idle();
  check('Sing pts: the equilibrium in the store with its stability, drawn, "Equilibria" in the legend',
    await until('w.marks && w.marks.equilibria.length === 1', 'equilibrium')
    && await S("['stable', 'unstable', 'saddle'].includes(w.marks.equilibria[0].type)")
    && (await layer('equilibria'))?.drawn === 1 && (await legendLabels()).join() === 'Equilibria,Text,Arrows,Markers,first run',
    JSON.stringify({eq: await S('w.marks && w.marks.equilibria'), legend: await legendLabels()}));

  let pressed = await legend('Text');
  t = await layer('text');
  check('the legend hides the text: not drawn, the toggle not pressed, the other marks still drawn',
    pressed === 'true' && t && !t.visible && t.drawn === 0 && (await layer('equilibria')).drawn === 1
    && (await layer('frozen-0')).drawn > 2, JSON.stringify(t));
  await legend('Text');
  pressed = await legend('first run');
  const f0 = await layer('frozen-0');
  check('... shows it again; a frozen curve hides on its own',
    (await layer('text')).drawn === 1 && pressed === 'true' && !f0.visible && f0.drawn === 0, JSON.stringify(f0));
  await legend('first run');

  await key('e'); /* Erase */
  check('Erase clears them: no marks in the store, none drawn, no legend entries',
    await until('!s.busy && w.marks && w.marks.text.length === 0 && w.marks.equilibria.length === 0 && w.marks.frozen.length === 0', 'erase')
    && (await P()).layers.length === 0 && (await legendLabels()).length === 0);
}

/* the array plot (docs/ui-v2.md T12, docs/protocol.md `aplot`): opened from
   the title bar's Array button (the classic key path underneath it,
   Window/zoom's Axes submenu, Array: `v` `a`, which also pops the Edit
   form the first time -- both already generic, AskDialog's menu and form).
   Its cells come from `values` (not the core's own colour indices), so the
   store can pick its own colour map; the core's own buttons (Fit, Redraw,
   Close) and time scroll (wheel, keyboard) still go through the protocol.
   examples/ode/wcring.ode (a ring of 20 coupled neurons, u0..u19) is a real
   array model, so the grid actually has columns worth scrolling through. */
async function aplotView() {
  await cdp.send('Emulation.setDeviceMetricsOverride', {width: 1280, height: 860, deviceScaleFactor: 1, mobile: false});
  check('T12: the page connects', await until('s.hello && !s.busy', 'hello'));

  await key('i');
  await until("s.ask && s.ask.kind === 'menu'", 'initialconds menu');
  const n0 = await S('s.seriesCount');
  await key('g');
  check('integrating the ring gives 401 rows', await until(`s.seriesCount > ${n0} && s.core.rows === 401 && !s.busy`, 'series'),
    JSON.stringify(await S('s.core.rows')));

  check('no array plot window yet', !(await S('s.aplot.windowOpen')));
  await cdp.eval(`document.querySelector('.aplot-toggle').click()`);
  check('the Array button opens the panel and the core\'s array plot window',
    await until('s.aplot.open && s.aplot.windowOpen', 'aplot window'));
  check('... and pops the Edit form the first time (the classic key path, v then a)',
    await until("s.ask && s.ask.kind === 'form' && s.ask.title === 'Edit arrayplot'", 'edit form'),
    JSON.stringify(await S('s.ask')));

  /* Column 1 is a select of the variables (like Viewaxes' axis fields, T4);
     the rest are plain number fields, in the order the core sent them.
     Zmin/Zmax (indices 4, 5 of the inputs) are left at the core's own
     defaults, so Fit below has something to change. */
  await cdp.eval(`(() => { const s = document.querySelector('[role=dialog] select');
    s.value = 'U0'; s.dispatchEvent(new Event('change', {bubbles: true})); })()`);
  await sleep(60); /* Preact's state update must be flushed before an input field's own change reads `values` */
  const editFields = ['20', '0', '10', '1', null, null, '0', '1']; /* NCols, Row1, NRows, RowSkip, -, -, Autoplot, ColSkip */
  for (let i = 0; i < editFields.length; i++) {
    if (editFields[i] === null) continue;
    await cdp.eval(`(() => { const el = document.querySelectorAll('[role=dialog] input')[${i}];
      el.value = ${JSON.stringify(editFields[i])}; el.dispatchEvent(new Event('input', {bubbles: true})); })()`);
    await sleep(60); /* Preact's state update from 'input' must be flushed (a render) before the next field or OK reads it */
  }
  await cdp.eval(`[...document.querySelectorAll('[role=dialog] button')].find(b => b.textContent === 'OK').click()`);
  check('Edit sends the array plot: 20 columns (u0..u19), 10 rows',
    await until('!s.busy && s.aplot.event && s.aplot.event.nx === 20 && s.aplot.event.ny === 10', 'edited'),
    JSON.stringify(await S('s.aplot.event && [s.aplot.event.nx, s.aplot.event.ny, s.aplot.event.title]')));

  /* the store's cells equal the event's values (ACCEPTANCE) */
  check('the store decodes `values` into its own cells, in the event\'s order',
    await cdp.eval(`(() => { const s = __xpp.state(), ev = s.aplot.event, got = Array.from(s.aplot.values);
      if (got.length !== ev.nx * ev.ny) return false;
      if (typeof ev.values === 'string') return true; /* f32: decoded already, nothing else to compare to */
      return got.every((v, i) => v === ev.values[i] || (v !== v && ev.values[i] === null)); })()`));

  /* Fit rescales zmin/zmax to the data actually shown */
  const before = await S('[s.aplot.event.zmin, s.aplot.event.zmax]');
  await cdp.eval(`[...document.querySelectorAll('.aplot-tools button')].find(b => b.textContent === 'Fit').click()`);
  check('Fit rescales zmin/zmax to the data actually shown', await until('!s.busy', 'fit'));
  const after = await S('[s.aplot.event.zmin, s.aplot.event.zmax]');
  check('... zmax > zmin, and the range changed from the core\'s default',
    after[1] > after[0] && JSON.stringify(after) !== JSON.stringify(before), JSON.stringify({before, after}));

  /* the colour map switch is client-only: the store's map changes, nothing goes to the core */
  const sentBefore = await cdp.eval('__xpp.sent().length');
  await cdp.eval(`(() => { const s = document.querySelector('.aplot-map select');
    s.value = 'xpp'; s.dispatchEvent(new Event('change', {bubbles: true})); })()`);
  check('the colour map switch changes the store\'s map, and sends nothing to the core',
    await S("s.aplot.colorMap === 'xpp'") && await cdp.eval('__xpp.sent().length') === sentBefore);

  /* hover names a cell: its variable, row and value */
  const cellPt = await cdp.eval(`(() => { const r = document.querySelector('.aplot-canvas').getBoundingClientRect();
    return {x: r.left + r.width * 0.3, y: r.top + r.height * 0.5}; })()`);
  await mouse('mouseMoved', cellPt.x, cellPt.y);
  check('hovering a cell names it: a variable, a row, a time and a value',
    await until('s.aplot.hover', 'hover'), JSON.stringify(await S('s.aplot.hover')));
  check('... the hover text names the variable (U..)',
    await cdp.eval(`/U\\d+, row \\d+: t/.test(document.querySelector('.aplot-hover').textContent)`),
    await cdp.eval(`document.querySelector('.aplot-hover').textContent`));

  /* scrolling in time (wheel) changes the shown rows and asks the core */
  const t0 = await S('[s.aplot.event.tlo, s.aplot.event.thi]');
  const grid = await cdp.eval(`(() => { const r = document.querySelector('.aplot-grid-wrap').getBoundingClientRect();
    return {x: r.left + r.width / 2, y: r.top + r.height / 2}; })()`);
  await mouse('mouseWheel', grid.x, grid.y, {deltaX: 0, deltaY: 300});
  check('wheeling the grid asks the core to scroll (aplot op scroll)',
    await until('!s.busy', 'wheel scroll') && await cdp.eval(`__xpp.sent().some(c => c.cmd === 'aplot' && c.op === 'scroll')`));
  const t1 = await S('[s.aplot.event.tlo, s.aplot.event.thi]');
  check('... and the shown rows changed (tlo/thi moved)', JSON.stringify(t1) !== JSON.stringify(t0), JSON.stringify({t0, t1}));

  /* the same from the keyboard */
  await cdp.eval(`document.querySelector('.aplot-grid-wrap').focus()`);
  const sent1 = await cdp.eval('__xpp.sent().length');
  await key('PageUp');
  check('PageUp (the keyboard) scrolls it too, toward earlier rows',
    await until('!s.busy', 'key scroll') && await cdp.eval('__xpp.sent().length') > sent1
    && await cdp.eval(`__xpp.sent().some(c => c.cmd === 'aplot' && c.op === 'scroll')`));

  /* Redraw, then Back leaves the core's window alive */
  await cdp.eval(`[...document.querySelectorAll('.aplot-tools button')].find(b => b.textContent === 'Redraw').click()`);
  check('Redraw asks for a fresh picture', await until('!s.busy', 'redraw')
    && await cdp.eval(`__xpp.sent().some(c => c.cmd === 'aplot' && c.op === 'redraw')`));
  await cdp.eval(`document.querySelector('.aplot-back').click()`);
  check('Back closes the panel; the core\'s array plot window stays alive',
    await until('!s.aplot.open && s.aplot.windowOpen', 'close panel'));

  /* 390x844: a full-screen sheet, no sideways scroll, 44px targets (SCOPE) */
  await cdp.eval(`document.querySelector('.aplot-toggle').click()`);
  check('reopening it (the window already exists) shows the panel again, no Edit form',
    await until('s.aplot.open && !s.ask', 'reopen'));
  await cdp.send('Emulation.setDeviceMetricsOverride', {width: 390, height: 844, deviceScaleFactor: 2, mobile: true});
  await cdp.send('Emulation.setTouchEmulationEnabled', {enabled: true, maxTouchPoints: 5});
  await sleep(300);
  const scroll = await cdp.eval(`({doc: document.documentElement.scrollWidth, body: document.body.scrollWidth, w: innerWidth})`);
  check('390x844: the array plot sheet causes no sideways scroll',
    scroll.doc <= scroll.w && scroll.body <= scroll.w, JSON.stringify(scroll));
  const sheet = await cdp.eval(`(() => { const r = document.querySelector('.aplot-panel').getBoundingClientRect();
    return {w: r.width, h: r.height}; })()`);
  check('the panel covers the viewport', sheet.w >= 390 - 1 && sheet.h >= 844 - 1, JSON.stringify(sheet));
  const small = await cdp.eval(`[...document.querySelectorAll('.aplot-panel button, .aplot-panel select')]
    .filter(b => b.getClientRects().length).map(b => [(b.textContent || '').trim(), b.getBoundingClientRect().height])
    .filter(([, h]) => h < 44)`);
  check('the panel\'s targets are at least 44px high', small.length === 0, JSON.stringify(small));
  await cdp.send('Emulation.setDeviceMetricsOverride', {width: 1280, height: 860, deviceScaleFactor: 1, mobile: false});
}

/* plot windows as tabs (docs/ui-v2.md T6): Makewindow create adds a tab,
   each tab keeps its own zoom, arrow keys move between tabs, the core's
   active window follows the tab, destroy removes it */
async function windows() {
  const tabs = () => cdp.eval(`[...document.querySelectorAll('[role=tab]')].map(t => [t.id, t.getAttribute('aria-selected')])`);
  const clickButton = text => cdp.eval(`[...document.querySelectorAll('button')].find(b => b.textContent === '${text}').click()`);
  check('one window: no tab list', await S('s.plots.windows.length === 1') && (await tabs()).length === 0);
  const t1 = await S('w.info && w.info.title'); /* what window 1 plots after the sessions above */
  await focusPlot();
  await key('+');
  const z1 = await S('w.viewport');
  check('window 1 zoomed', !!z1.x, JSON.stringify(z1));

  await clickButton('New window');
  check('New window (Makewindow/Create) adds window 2, selected',
    await until('s.plots.windows.length === 2 && s.plots.active === 2 && !s.busy', 'create'),
    JSON.stringify(await S('[s.plots.windows.map(x => x.win), s.plots.active]')));
  check('the tab list follows: two tabs, the second selected',
    JSON.stringify(await tabs()) === JSON.stringify([['plot-tab-1', 'false'], ['plot-tab-2', 'true']]), JSON.stringify(await tabs()));
  check('the store holds both windows\' series', await S('s.plots.windows.every(x => x.series && x.series.rows === 601)'));
  check("window 2 starts at the core's axes", await S('w.viewport.x === null && w.viewport.y === null'));

  await focusPlot();
  await key('x');
  if (await until("s.ask && s.ask.kind === 'string'", 'x')) await cdp.eval(`__xpp.send({cmd: 'answer', id: __xpp.state().ask.id, value: 'V'})`);
  check('Xi vs t in window 2 changes its curves only',
    await until(`!s.busy && w.series.curves[0].x === 0 && w.series.curves[0].y === 1 && w.info.title === 'V vs T'
      && s.plots.windows[0].info.title === '${t1}'`, 'x vs t'),
    JSON.stringify(await S('s.plots.windows.map(x => x.info && x.info.title)')));
  check('the tabs name what each window plots', await cdp.eval(`[...document.querySelectorAll('[role=tab]')].map(t => t.textContent).join('|')`)
    === `1${t1}|2V vs T`);
  await focusPlot();
  await key('-');
  const z2 = await S('w.viewport');
  check('window 2 zoomed on its own', !!z2.x && JSON.stringify(z2) !== JSON.stringify(z1), JSON.stringify(z2));

  await cdp.eval(`document.getElementById('plot-tab-1').click()`);
  check('clicking tab 1 shows window 1 and makes it the core\'s active window',
    await until('s.plots.active === 1 && !s.busy && s.core.win === 1', 'tab 1'), JSON.stringify(await S('[s.plots.active, s.core.win]')));
  const back = await S('w.viewport'), p1 = await P();
  check("tab 1 keeps its zoom", JSON.stringify(back) === JSON.stringify(z1)
    && Math.abs(p1.x.min - z1.x.min) < 1e-9 && Math.abs(p1.x.max - z1.x.max) < 1e-9, JSON.stringify([back, p1 && p1.x]));
  check('its chart is the one shown', p1 && p1.curves[0].label === t1 && p1.width > 200, JSON.stringify(p1 && [p1.curves, p1.width]));

  await cdp.eval(`document.getElementById('plot-tab-1').focus()`);
  await key('ArrowRight');
  check('ArrowRight on the tabs moves to window 2, focus with it',
    await until(`s.plots.active === 2 && document.activeElement.id === 'plot-tab-2'`, 'arrow right'));
  check('tab 2 kept its zoom', JSON.stringify(await S('w.viewport')) === JSON.stringify(z2));
  await key('ArrowLeft');
  check('ArrowLeft back to window 1', await until(`s.plots.active === 1 && document.activeElement.id === 'plot-tab-1'`, 'arrow left'));
  await key('End');
  check('End to the last tab', await until(`s.plots.active === 2 && document.activeElement.id === 'plot-tab-2'`, 'End'));
  check('the tab key goes back to the core', await until('!s.busy && s.core.win === 2', 'core win 2'));

  /* a plot mode (T4) on the tab shown: Window/Zoom by the keyboard zooms window 2, not window 1 */
  const axes1 = await S('JSON.stringify(s.plots.windows[0].info)');
  await focusPlot();
  await menuKeys('w', 'z');
  check("Window/Zoom in window 2: its plot is in box mode, one instruction bar",
    await until("s.pick && s.pick.mode === 'box' && s.pick.win === 2 && !s.pick.waiting", 'box mode in 2')
    && await cdp.eval(`document.querySelectorAll('.pick-bar').length === 1 && !!document.querySelector('.plot-view:not([hidden]) .pick-bar')`),
    JSON.stringify(await S('s.pick')));
  const pz = await P(), vz = await S('s.core.view');
  await key('Enter');
  for (let i = 0; i < 5; i++) await key('ArrowRight');
  for (let i = 0; i < 5; i++) await key('ArrowDown');
  await key('Enter');
  const box = [dataAt(pz, 0.5, 0.5), dataAt(pz, 0.6, 0.6)];
  await until('!s.busy && !s.pick', 'zoom in 2');
  const vb = await S('s.core.view'), info2 = await S('w.info');
  check("window 2's axes are the box, in state.view and in plots; window 1's are unchanged",
    vb.win === 2 && viewIsBox(vb, box[0], box[1], corePixel(vz))
    /* state.view has 6 digits, plots the doubles */
    && Math.abs(info2.xlo - vb.xlo) <= 1e-5 * Math.abs(vb.xhi - vb.xlo) && Math.abs(info2.yhi - vb.yhi) <= 1e-5 * Math.abs(vb.yhi - vb.ylo)
    && await S('JSON.stringify(s.plots.windows[0].info)') === axes1, JSON.stringify({vz, pz: [pz.x, pz.y], vb, box, info2}));

  await cdp.send('Emulation.setDeviceMetricsOverride', {width: 390, height: 844, deviceScaleFactor: 2, mobile: true});
  await sleep(300);
  const scroll = await cdp.eval(`({doc: document.documentElement.scrollWidth, w: innerWidth,
    tabs: [...document.querySelectorAll('[role=tab]')].every(t => t.getBoundingClientRect().right <= innerWidth)})`);
  check('390 px with two tabs: no sideways scroll, the tabs fit', scroll.doc <= scroll.w && scroll.tabs, JSON.stringify(scroll));
  await cdp.send('Emulation.setDeviceMetricsOverride', {width: 1280, height: 860, deviceScaleFactor: 1, mobile: false});
  await sleep(200);

  await clickButton('Close window');
  check('Close window (Makewindow/Destroy) removes its tab',
    await until('s.plots.windows.length === 1 && s.plots.active === 1 && !s.busy', 'destroy')
    && (await tabs()).length === 0, JSON.stringify(await S('s.plots.windows.map(x => x.win)')));
  check('window 1 still has its zoom and its series', JSON.stringify(await S('w.viewport')) === JSON.stringify(z1)
    && await S('w.series.rows === 601'));
  await focusPlot();
  await key('0');
}

/* "Use this view" and Fit (docs/ui-v2.md T9, session.ts useThisView/fitView):
   zoom by wheel, press "Use this view" and check the core's axes (state.view,
   and the window's own info from "plots") equal the zoomed ranges, with the
   client viewport reset to null (the core's axes, no visible jump: the
   chart still shows the same range); then Fit (the classic page's Window/Fit
   key sequence, w then f) widens the core's axes to contain the data. */
async function viewCheck() {
  check('the page connects and asks for the plot as data', await until('s.hello && s.seriesCount >= 1 && !s.busy', 'hello'));
  await key('i');
  await until("s.ask && s.ask.kind === 'menu'", 'menu');
  await key('g');
  check('I, G integrates a series', await until('w.series && w.series.rows === 601 && !s.busy', 'series'));

  const a = await area(), cx = a.x + a.w / 2, cy = a.y + a.h / 2;
  await mouse('mouseMoved', cx, cy);
  await mouse('mouseWheel', cx, cy, {deltaX: 0, deltaY: -240});
  await until('w.viewport.x', 'wheel');
  const zoomed = await P();
  check('zoom by wheel', zoomed && zoomed.x && zoomed.y, JSON.stringify(zoomed));

  await cdp.eval(`document.querySelector('.plot-tools button[title^="Make this zoom"]').click()`);
  const close = (a2, b) => Math.abs(a2 - b) < 1e-5 * Math.max(1, Math.abs(b));
  await until('s.core.view && !s.busy', 'view');
  const view = await S('s.core.view');
  check("Use this view: the core's axes (state.view) equal the zoomed ranges",
    view && close(view.xlo, zoomed.x.min) && close(view.xhi, zoomed.x.max) && close(view.ylo, zoomed.y.min) && close(view.yhi, zoomed.y.max),
    JSON.stringify([view, zoomed.x, zoomed.y]));
  const info = await S('w.info');
  check('... "plots" agrees (the window\'s own axes)',
    close(info.xlo, zoomed.x.min) && close(info.xhi, zoomed.x.max) && close(info.ylo, zoomed.y.min) && close(info.yhi, zoomed.y.max),
    JSON.stringify([info, zoomed.x, zoomed.y]));
  check('... and the client viewport is reset', await until('w.viewport.x === null && w.viewport.y === null', 'reset'));
  const shown = await P();
  check('... with no visible jump: the chart still shows the same range',
    close(shown.x.min, zoomed.x.min) && close(shown.x.max, zoomed.x.max) && close(shown.y.min, zoomed.y.min) && close(shown.y.max, zoomed.y.max),
    JSON.stringify([shown.x, shown.y, zoomed.x, zoomed.y]));

  const extent = await cdp.eval(`(() => {
    const s = __xpp.state(), w = ${ACTIVE}, m = w.series, c = m.curves[0];
    const xs = m.columns.get(c.x), ys = m.columns.get(c.y);
    let xmin = Infinity, xmax = -Infinity, ymin = Infinity, ymax = -Infinity;
    for (let i = 0; i < xs.length; i++) {
      if (xs[i] < xmin) xmin = xs[i]; if (xs[i] > xmax) xmax = xs[i];
      if (ys[i] < ymin) ymin = ys[i]; if (ys[i] > ymax) ymax = ys[i];
    }
    return {xmin, xmax, ymin, ymax};
  })()`);
  await cdp.eval(`document.querySelector('.plot-tools button[title^="Fit the window"]').click()`);
  check("Fit changes the core's axes to the data's extent",
    await until(`s.core.view && s.core.view.xlo <= ${extent.xmin} + 1e-6 && s.core.view.xhi >= ${extent.xmax} - 1e-6
      && s.core.view.ylo <= ${extent.ymin} + 1e-6 && s.core.view.yhi >= ${extent.ymax} - 1e-6`, 'fit'),
    JSON.stringify([await S('s.core.view'), extent]));

  /* T30: the corner Fit does what the toolbar's Fit does, after a scroll
     or a zoom that has lost the data: first with the core's own axes
     moved off the extent (a plain zoom + "Use this view", as above), so
     Fit moves them, then with them already fitted. */
  check('the corner Fit sits over the plot while it has data',
    await cdp.eval(`!!document.querySelector('.plot-view:not([hidden]) .plot-host .plot-fit')`));
  await mouse('mouseWheel', cx, cy, {deltaX: 0, deltaY: -240});
  await until('w.viewport.x', 'zoom before corner fit');
  await cdp.eval(`document.querySelector('.plot-tools button[title^="Make this zoom"]').click()`);
  await until('w.viewport.x === null', 'use this view before corner fit');

  await cdp.eval(`document.querySelector('.plot-view:not([hidden]) .plot-host').focus()`);
  for (let st = 0; st < 15; st++) await key('ArrowRight'); /* pans well past the data */
  await until('w.viewport.x', 'panned away');
  const away = await P();
  check('panned far from the data', away.x.min > extent.xmax, JSON.stringify([away.x, extent]));
  await cdp.eval(`document.querySelector('.plot-view:not([hidden]) .plot-host .plot-fit').click()`);
  check('the corner Fit brings the data back into view, through the core as the toolbar\'s Fit does',
    await until(`s.core.view && s.core.view.xlo <= ${extent.xmin} + 1e-6 && s.core.view.xhi >= ${extent.xmax} - 1e-6
      && s.core.view.ylo <= ${extent.ymin} + 1e-6 && s.core.view.yhi >= ${extent.ymax} - 1e-6
      && w.viewport.x === null && w.viewport.y === null`, 'corner fit'),
    JSON.stringify([await S('s.core.view'), extent]));
  /* the user's case: the axes already fit, a scroll loses the data, Fit again. The core's
     axes do not move this time, so it is the page that must drop its own pan (session.fitView) */
  await cdp.eval(`document.querySelector('.plot-view:not([hidden]) .plot-host').focus()`);
  for (let st = 0; st < 15; st++) await key('ArrowRight');
  await until('w.viewport.x', 'panned away again');
  await cdp.eval(`document.querySelector('.plot-view:not([hidden]) .plot-host .plot-fit').click()`);
  check('Fit again with the axes already fitted still brings the data back (the page drops its pan)',
    await until('w.viewport.x === null && w.viewport.y === null && !s.busy', 'second corner fit'),
    JSON.stringify(await S('w.viewport')));
}

/* the 3D plot host's box on screen (docs/ui-v2.md T14): Plot3DView.tsx's
   canvas fills it, and drag/key events go to the host div itself */
const area3d = () => cdp.eval(`(() => { const r = document.querySelector('.plot-view:not([hidden]) .plot-host').getBoundingClientRect();
  return {x: r.left, y: r.top, w: r.width, h: r.height}; })()`);
const sentView3d = () => cdp.eval("__xpp.sent().filter(c => c.cmd === 'view3d').length");

/* 3D plots (docs/ui-v2.md T14, GitHub issue #18): lorenz.ode sets axes=3d
   and phi=60 (theta stays the default 45) and runnow=1, which the core
   runs as its own command cycle after loading (servercheck.py's
   check_view3d has the same two-cycle wait); the plot never draws with
   three.js, just a canvas (plot/render3d.ts) from the projection
   (plot/project3d.ts). */
async function threePlot() {
  check('the store holds the 3D window: three, its box, and its own angles seeded from the core\'s',
    await until("w.info && w.info.three === 1 && w.view3d && w.info.box && w.info.box.xmax === 20", '3D window', 20000));
  check('runnow=1\'s run finishes: 1601 rows (dt=.025, total=40)',
    await until('w.series && w.series.rows === 1601', 'lorenz run', 20000));

  const angles0 = await S('w.view3d');
  check("the client's angles start at the core's own (@ phi=60, theta the default 45)",
    angles0 && angles0.theta === 45 && angles0.phi === 60, JSON.stringify(angles0));
  check("state.view.theta/phi agrees too, from the start", await until(
    's.core.view && s.core.view.three === 1 && s.core.view.theta === 45 && s.core.view.phi === 60', 'state.view'));

  const before = await P();
  check('it draws a projection: the box\'s 8 corners, at least one curve with points',
    before && before.box.length === 8 && before.curves.length >= 1 && before.curves[0].points > 0, JSON.stringify(before));

  /* a drag on the focused plot turns it locally at once */
  const a = await area3d(), cx = a.x + a.w / 2, cy = a.y + a.h / 2;
  const sentBefore = await sentView3d();
  const steps = 12;
  await mouse('mousePressed', cx, cy, {button: 'left', buttons: 1, clickCount: 1});
  for (let s = 1; s <= steps; s++) await mouse('mouseMoved', cx + 3 * s, cy - 2 * s, {button: 'left', buttons: 1});
  const dragged = await S('w.view3d');
  check('drag: theta/phi changed at once, by the drag\'s pixels (core/many_pops.c rotate3dcheck: 1 pixel, 1 degree, subtracted)',
    dragged && dragged.theta === angles0.theta - 3 * steps && dragged.phi === angles0.phi + 2 * steps,
    JSON.stringify([angles0, dragged]));
  const during = await P();
  check('... and the drawn projection changed too (still dragging, no round trip needed)',
    during && JSON.stringify(during.box) !== JSON.stringify(before.box), '');
  const sentDuring = (await sentView3d()) - sentBefore;
  check(`throttled: ${steps} pointer moves sent far fewer view3d commands (${sentDuring})`,
    sentDuring > 0 && sentDuring < steps, String(sentDuring));
  await mouse('mouseReleased', cx + 3 * steps, cy - 2 * steps, {button: 'left', buttons: 0, clickCount: 1});

  check("state.view.theta/phi settles to agree with the client's angles",
    await until(`s.core.view && Math.abs(s.core.view.theta - w.view3d.theta) < 1e-6
      && Math.abs(s.core.view.phi - w.view3d.phi) < 1e-6`, 'drag settle', 5000));
  const plotsAfterDrag = await S('w.info');
  check('... "plots" agrees too', plotsAfterDrag && plotsAfterDrag.theta === dragged.theta && plotsAfterDrag.phi === dragged.phi,
    JSON.stringify([plotsAfterDrag, dragged]));

  /* the arrow keys turn it too, on the focused plot (plot/project3d.ts KEY_STEP) */
  await cdp.eval(`document.querySelector('.plot-view:not([hidden]) .plot-host').focus()`);
  const beforeKey = await S('w.view3d');
  await key('ArrowRight');
  const afterKey = await S('w.view3d');
  check('an arrow key on the focused plot turns it too (5 degrees)',
    afterKey && afterKey.theta === beforeKey.theta + 5 && afterKey.phi === beforeKey.phi,
    JSON.stringify([beforeKey, afterKey]));
  check('... and state.view settles to agree', await until(`s.core.view && Math.abs(s.core.view.theta - w.view3d.theta) < 1e-6
    && Math.abs(s.core.view.phi - w.view3d.phi) < 1e-6`, 'key settle', 5000));

  /* Shift+arrow is the coarse step (CDP modifiers: 8 is Shift) */
  const beforeShift = await S('w.view3d');
  await key('ArrowUp', 8);
  const afterShift = await S('w.view3d');
  check('Shift+arrow turns it by the coarse step (30 degrees)',
    afterShift && afterShift.phi === beforeShift.phi + 30 && afterShift.theta === beforeShift.theta,
    JSON.stringify([beforeShift, afterShift]));

  /* T30: fit_window() handles ThreeDFlag (core/graf_par.c), so 3D plots
     get the corner button too; it goes through the core like the main
     plot's, there being no client-side zoom of a 3D plot to fit locally */
  check('the corner Fit sits over the 3D plot too (Window/Fit fits its box the same way)',
    await cdp.eval(`!!document.querySelector('.plot-view:not([hidden]) .plot-host .plot-fit')`));
  const sentBeforeFit = await cdp.eval('__xpp.sent().length');
  await cdp.eval(`document.querySelector('.plot-view:not([hidden]) .plot-host .plot-fit').click()`);
  check("the corner Fit sends the core's Window/Fit (w, f) as keys, as the 2D toolbar's Fit does",
    await until('!s.busy', 'fit settled')
      && (await cdp.eval(`__xpp.sent().slice(${sentBeforeFit}).some(c => c.cmd === 'key' && (c.key === 'w' || c.key === 'f'))`)),
    JSON.stringify(await cdp.eval(`__xpp.sent().slice(${sentBeforeFit})`)));
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

  /* the data table is a full-screen sheet with a Back button (R6) */
  check('the table sheet starts closed', !(await S('s.table.open'))
    && await cdp.eval(`getComputedStyle(document.querySelector('.table-panel')).visibility === 'hidden'`));
  const dt = await cdp.eval(`(() => { const r = document.querySelector('.table-toggle').getBoundingClientRect();
    return {x: r.left + r.width / 2, y: r.top + r.height / 2}; })()`);
  await touch('touchStart', [dt]);
  await touch('touchEnd', []);
  check('tapping Data opens the table sheet', await until('s.table.open', 'table sheet')
    && await cdp.eval(`getComputedStyle(document.querySelector('.table-panel')).visibility === 'visible'`));
  check('the sheet covers the viewport', await cdp.eval(`(() => { const r = document.querySelector('.table-panel')
    .getBoundingClientRect(); return r.width >= innerWidth - 1 && r.height >= innerHeight - 1; })()`));
  check('its first control has the focus', await until(`document.activeElement.closest('.table-panel')`, 'table sheet focus'));
  const tableScroll = await cdp.eval(`({doc: document.documentElement.scrollWidth, body: document.body.scrollWidth, w: innerWidth})`);
  check('390x844: the table causes no sideways page scroll',
    tableScroll.doc <= tableScroll.w && tableScroll.body <= tableScroll.w, JSON.stringify(tableScroll));
  const tableSmall = await cdp.eval(`[...document.querySelectorAll('.table-panel button')]
    .filter(b => b.getClientRects().length).map(b => [b.textContent.trim(), b.getBoundingClientRect().height])
    .filter(([, h]) => h < 44)`);
  check('the table sheet\'s targets are at least 44px high', tableSmall.length === 0, JSON.stringify(tableSmall));
  await cdp.eval(`document.querySelector('.table-back').click()`);
  check('Back closes the table sheet', await until('!s.table.open', 'close table sheet'));
  check('the focus returns to the Data button', await until(`document.activeElement.closest('.table-toggle')`, 'table focus back'));

  /* the text views panel is a full-screen sheet with a Back button too (R6, docs/ui-v2.md T16) */
  check('the text sheet starts closed', !(await S('s.text.open'))
    && await cdp.eval(`getComputedStyle(document.querySelector('.text-panel')).visibility === 'hidden'`));
  const xt = await cdp.eval(`(() => { const r = document.querySelector('.text-toggle').getBoundingClientRect();
    return {x: r.left + r.width / 2, y: r.top + r.height / 2}; })()`);
  await touch('touchStart', [xt]);
  await touch('touchEnd', []);
  check('tapping Text opens the sheet', await until('s.text.open', 'text sheet')
    && await cdp.eval(`getComputedStyle(document.querySelector('.text-panel')).visibility === 'visible'`));
  check('the sheet covers the viewport', await cdp.eval(`(() => { const r = document.querySelector('.text-panel')
    .getBoundingClientRect(); return r.width >= innerWidth - 1 && r.height >= innerHeight - 1; })()`));
  check('its first control has the focus', await until(`document.activeElement.closest('.text-panel')`, 'text sheet focus'));
  const textScroll = await cdp.eval(`({doc: document.documentElement.scrollWidth, body: document.body.scrollWidth, w: innerWidth})`);
  check('390x844: the text panel causes no sideways page scroll',
    textScroll.doc <= textScroll.w && textScroll.body <= textScroll.w, JSON.stringify(textScroll));
  const textSmall = await cdp.eval(`[...document.querySelectorAll('.text-panel button, .text-panel input')]
    .filter(b => b.getClientRects().length).map(b => [(b.textContent || b.placeholder || '').trim(), b.getBoundingClientRect().height])
    .filter(([, h]) => h < 44)`);
  check('the text sheet\'s targets are at least 44px high', textSmall.length === 0, JSON.stringify(textSmall));
  await cdp.eval(`document.querySelector('.text-back').click()`);
  check('Back closes the text sheet', await until('!s.text.open', 'close text sheet'));
  check('the focus returns to the Text button', await until(`document.activeElement.closest('.text-toggle')`, 'text focus back'));

  /* pinch out about the middle */
  const a = await area(), cx = a.x + a.w / 2, cy = a.y + a.h / 2;
  const w0 = (await P()).x;
  await touch('touchStart', [{x: cx - 30, y: cy}, {x: cx + 30, y: cy}]);
  for (let s = 1; s <= 5; s++) await touch('touchMove', [{x: cx - 30 - 12 * s, y: cy}, {x: cx + 30 + 12 * s, y: cy}]);
  await touch('touchEnd', []);
  const z = await S('w.viewport');
  check('a pinch zooms in', z.x && width(z.x) < width(w0) * 0.6, JSON.stringify([w0, z.x]));
  if (!z.x) return;
  /* one finger pans */
  await touch('touchStart', [{x: cx, y: cy}]);
  for (let s = 1; s <= 5; s++) await touch('touchMove', [{x: cx + 10 * s, y: cy}]);
  await touch('touchEnd', []);
  const z2 = await S('w.viewport');
  check('one finger pans', z2.x.min < z.x.min && Math.abs(width(z2.x) - width(z.x)) < 1e-9 * width(z.x), JSON.stringify(z2));
  /* a tap on a point reads it */
  await cdp.eval(`document.querySelector('.plot-view:not([hidden]) .plot-host').focus()`);
  await key('0');
  await until('w.viewport.x === null', 'reset');
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

/* ---- prompts (docs/ui-v2.md T4) ------------------------------------------------- */

const lastAnswer = async () => (await cdp.eval('__xpp.sent()')).filter(c => c.cmd === 'answer').pop();
const focusPlot = () => cdp.eval(`document.querySelector('.plot-view:not([hidden]) .plot-host').focus()`);

/** a key, then the key that answers the menu it opens */
async function menuKeys(first, then) {
  await key(first);
  if (!(await until("s.ask && s.ask.kind === 'menu'", `menu of ${first}`))) return false;
  await key(then);
  return true;
}

/** the core's pixel size in data units: an answer in data coordinates lands on the nearest pixel */
const corePixel = v => ({x: Math.abs(v.xhi - v.xlo) / Math.abs(v.right - v.left), y: Math.abs(v.yhi - v.ylo) / Math.abs(v.bottom - v.top)});

/** the core's view against the box a..b (data coordinates), within a pixel of the view before */
function viewIsBox(v, a, b, px) {
  const near = (u, w, d) => Math.abs(u - w) <= d * 1.01 + 1e-12;
  return near(Math.min(v.xlo, v.xhi), Math.min(a.x, b.x), px.x) && near(Math.max(v.xlo, v.xhi), Math.max(a.x, b.x), px.x)
    && near(Math.min(v.ylo, v.yhi), Math.min(a.y, b.y), px.y) && near(Math.max(v.ylo, v.yhi), Math.max(a.y, b.y), px.y);
}

/** fractions of the plotting area in the data coordinates the plot shows */
const dataAt = (p, fx, fy) => ({x: p.x.min + fx * (p.x.max - p.x.min), y: p.y.max - fy * (p.y.max - p.y.min)});

async function prompts() {
  await desktopMetrics();
  await sleep(300);
  await until('!s.busy', 'idle');
  const lists = await S('s.hello.lists');

  /* Viewaxes/2D: the axis fields are selects of hello.lists[0]; picking another variable plots it */
  const pickX = async name => {
    await cdp.eval(`(() => { const s = document.querySelector('[role=dialog] select'); s.value = ${JSON.stringify(name)};
      s.dispatchEvent(new Event('change', {bubbles: true})); })()`);
    await sleep(80);
    await key('Enter');
  };
  await focusPlot();
  await menuKeys('v', '2');
  check('Viewaxes/2D opens its form', await until("s.ask && s.ask.kind === 'form' && document.querySelector('[role=dialog] select')", 'form'));
  const sel = await cdp.eval(`(() => { const s = document.querySelector('[role=dialog] select');
    return s && {list: s.dataset.list, options: [...s.options].map(o => o.value), value: s.value, focused: document.activeElement === s}; })()`);
  const x0 = await S('w.series.curves[0].x');
  check('its X-axis field is a select of T and the variables, at the plotted one, with the focus',
    sel && sel.list === '0' && sel.options.join() === lists[0].join() && sel.value === lists[0][x0] && sel.focused,
    JSON.stringify({sel, x0}));
  const other = lists[0][x0] === 'T' ? 'V' : 'T', n0 = await S('s.seriesCount');
  await pickX(other);
  check(`picking ${other} from the select and Enter: the core plots W against it (a new series)`,
    await until(`s.seriesCount > ${n0} && !s.busy && w.series.curves[0].x === ${lists[0].indexOf(other)}
      && w.series.curves[0].y === 2`, 'W vs other'), JSON.stringify([await S('w.series && w.series.curves'), await lastAnswer()]));
  check('the answer carries the name picked', (await lastAnswer())?.values?.[0] === other, JSON.stringify(await lastAnswer()));
  await focusPlot();
  await menuKeys('v', '2');
  await until("s.ask && s.ask.kind === 'form' && document.querySelector('[role=dialog] select')", 'form');
  await pickX('V');
  check('and V again: W against V', await until('!s.busy && w.series.curves[0].x === 1 && w.series.curves[0].y === 2', 'W vs V'));

  /* T31: the form's fields take what the core says they take (the ask's kinds): letters in a
     number field are marked with a message, OK is disabled and Enter answers nothing */
  await focusPlot();
  await menuKeys('v', '2');
  await until("s.ask && s.ask.kind === 'form' && document.querySelector('[role=dialog] input')", 'form');
  const formId = await S('s.ask.id');
  const answers = async () => (await cdp.eval('__xpp.sent()')).filter(c => c.cmd === 'answer').length;
  const answers0 = await answers();
  const numberBox = `document.querySelector('[role=dialog] input[data-kind=number]')`;
  await cdp.eval(`(() => { const i = ${numberBox}; i.focus(); i.value = 'abc'; i.dispatchEvent(new Event('input', {bubbles: true})); })()`);
  await sleep(80);
  await key('Enter');
  await sleep(200);
  const refused = await cdp.eval(`(() => { const i = ${numberBox}, m = i.closest('label').querySelector('.field-error');
    return {invalid: i.getAttribute('aria-invalid'), message: m && m.textContent, mode: i.inputMode,
      ok: document.querySelector('[role=dialog] .dialog-actions .primary').disabled}; })()`);
  check('T31: letters in a number field of a form (Viewaxes/2D Xmin) are refused: marked, "A number", OK disabled, Enter answers nothing',
    refused.invalid === 'true' && refused.message === 'A number' && refused.mode === 'decimal' && refused.ok === true
    && (await answers()) === answers0 && await S(`!!s.ask && s.ask.id === ${formId}`), JSON.stringify(refused));
  await key('Escape');
  await until('!s.ask && !s.busy', 'form cancelled');
  /* ... and a number-or-formula box (new_float: nUmerics/Total) takes a %formula, sent as typed */
  await focusPlot();
  await key('u');
  await until('!s.busy && s.core.menu === 2', 'numerics menu');
  await key('t');
  await until("s.ask && s.ask.kind === 'string'", 'total');
  const formula = `%${await S('s.ask.value')}*1`;
  const formulaBox = `document.querySelector('[role=dialog] input')`;
  const kindOf = await cdp.eval(`${formulaBox}.dataset.kind`);
  await cdp.eval(`(() => { const i = ${formulaBox}; i.value = ${JSON.stringify(formula)}; i.dispatchEvent(new Event('input', {bubbles: true})); })()`);
  await sleep(80);
  const taken = await cdp.eval(`${formulaBox}.getAttribute('aria-invalid')`);
  await key('Enter');
  await until('!s.ask && !s.busy', 'total set');
  check(`T31: a %formula (${formula}) in nUmerics/Total is taken and sent as typed`,
    kindOf === 'number' && taken === null && (await lastAnswer())?.value === formula, JSON.stringify([kindOf, taken, await lastAnswer()]));
  await key('Escape');
  await until('!s.busy && s.core.menu === 0', 'main menu');

  /* Window/Zoom by a box drawn with the mouse */
  await focusPlot();
  await menuKeys('w', 'z');
  check('Window/Zoom asks for a box: the plot is in box mode, with its instruction bar and Cancel',
    await until("s.pick && s.pick.mode === 'box' && !s.pick.waiting", 'box mode')
    && await cdp.eval(`!!document.querySelector('.pick-bar button') && !document.querySelector('[role=dialog]')`),
    JSON.stringify(await S('[s.ask, s.pick]')));
  check('the plot has the focus', await until(`document.activeElement.closest('.plot-host')`, 'plot focus'));
  let v0 = await S('s.core.view'), p = await P(), a = await area();
  const from = {x: Math.round(a.x + 0.25 * a.w), y: Math.round(a.y + 0.3 * a.h)};
  const to = {x: Math.round(a.x + 0.7 * a.w), y: Math.round(a.y + 0.8 * a.h)};
  await mouse('mouseMoved', from.x, from.y);
  await mouse('mousePressed', from.x, from.y, {button: 'left', buttons: 1, clickCount: 1});
  for (let s = 1; s <= 5; s++) {
    await mouse('mouseMoved', from.x + (to.x - from.x) * s / 5, from.y + (to.y - from.y) * s / 5, {button: 'left', buttons: 1});
  }
  await mouse('mouseReleased', to.x, to.y, {button: 'left', buttons: 0, clickCount: 1});
  let want = [dataAt(p, (from.x - a.x) / a.w, (from.y - a.y) / a.h), dataAt(p, (to.x - a.x) / a.w, (to.y - a.y) / a.h)];
  check('the box is answered in data coordinates', await until('!s.busy', 'zoom') && 'xd2' in (await lastAnswer() ?? {}),
    JSON.stringify(await lastAnswer()));
  let v1 = await S('s.core.view');
  check("the core's view is the box drawn, within a pixel", viewIsBox(v1, want[0], want[1], corePixel(v0)),
    JSON.stringify({v1, want}));
  p = await P();
  /* the window's axes: exact in `plots` (T6), 6 digits in state.view */
  const ax = await S('w.info || s.core.view');
  check('and the plot shows it', Math.abs(p.x.min - ax.xlo) < 1e-9 && Math.abs(p.y.max - ax.yhi) < 1e-9, JSON.stringify([p.x, p.y, ax]));

  /* the same by the keyboard only: arrows move the crosshair, Enter fixes a corner, Enter again zooms */
  await menuKeys('w', 'z');
  await until("s.pick && s.pick.mode === 'box' && !s.pick.waiting", 'box mode');
  check('keyboard: the plot has the focus in box mode', await until(`document.activeElement.closest('.plot-host')`, 'plot focus'));
  for (let i = 0; i < 5; i++) await key('ArrowLeft');
  for (let i = 0; i < 5; i++) await key('ArrowUp');
  await key('Enter');
  for (let i = 0; i < 10; i++) await key('ArrowRight');
  for (let i = 0; i < 10; i++) await key('ArrowDown');
  const pk = await S('s.pick');
  check('arrow keys and Enter set the corners', pk && pk.anchor && Math.abs(pk.anchor.fx - 0.4) < 1e-9
    && Math.abs(pk.anchor.fy - 0.4) < 1e-9 && Math.abs(pk.cursor.fx - 0.6) < 1e-9 && Math.abs(pk.cursor.fy - 0.6) < 1e-9, JSON.stringify(pk));
  v0 = await S('s.core.view');
  p = await P();
  want = [dataAt(p, 0.4, 0.4), dataAt(p, 0.6, 0.6)];
  await key('Enter');
  await until('!s.busy && !s.pick', 'keyboard zoom');
  v1 = await S('s.core.view');
  check("keyboard: the core's view is the box, within a pixel", viewIsBox(v1, want[0], want[1], corePixel(v0)),
    JSON.stringify({v1, want}));

  /* Escape cancels a plot mode: the ask is answered as cancelled, the view stays */
  await menuKeys('w', 'z');
  await until("s.pick && s.pick.mode === 'box'", 'box mode');
  v0 = await S('s.core.view');
  await key('ArrowRight');
  await key('Escape');
  check('Escape cancels the box: answered with ok 0, the mode is gone', await until('!s.busy && !s.ask && !s.pick', 'cancel')
    && (await lastAnswer())?.ok === 0, JSON.stringify(await lastAnswer()));
  v1 = await S('s.core.view');
  check("and the core's view is unchanged", v1.xlo === v0.xlo && v1.xhi === v0.xhi && v1.ylo === v0.ylo && v1.yhi === v0.yhi);
  await menuKeys('w', 'd'); /* Window/Default: back to the file's window */
  await until('!s.busy', 'default window');

  /* Initialconds/Mouse: a click picks the initial point */
  await focusPlot();
  await menuKeys('i', 'm');
  check('Initialconds/Mouse puts the plot in point mode', await until("s.pick && s.pick.mode === 'point'", 'point mode'));
  v0 = await S('s.core.view');
  p = await P();
  a = await area();
  const at = {x: Math.round(a.x + 0.3 * a.w), y: Math.round(a.y + 0.35 * a.h)};
  await mouse('mouseMoved', at.x, at.y);
  await mouse('mousePressed', at.x, at.y, {button: 'left', buttons: 1, clickCount: 1});
  await mouse('mouseReleased', at.x, at.y, {button: 'left', buttons: 0, clickCount: 1});
  const pt = dataAt(p, (at.x - a.x) / a.w, (at.y - a.y) / a.h), px = corePixel(v0);
  check('a click there starts from that point (the ICs), within a pixel',
    await until(`!s.busy && Math.abs(s.core.ics.find(c => c[0] === 'V')[1] - ${pt.x}) <= ${px.x * 1.01}
      && Math.abs(s.core.ics.find(c => c[0] === 'W')[1] - ${pt.y}) <= ${px.y * 1.01}`, 'mouse ICs', 30000),
    JSON.stringify([await S('s.core.ics'), pt]));

  /* Window/Scroll: a drag mode; an arrow key drags the plot (down, move, up), Enter (Done) ends it */
  await focusPlot();
  await menuKeys('w', 's');
  check('Window/Scroll puts the plot in drag mode', await until("s.pick && s.pick.mode === 'drag'", 'drag mode'));
  v0 = await S('s.core.view');
  await key('ArrowLeft');
  await sleep(300);
  await key('Enter');
  const drags = (await cdp.eval('__xpp.sent()')).filter(c => c.cmd === 'answer').slice(-4);
  check('an arrow key drags the plot in data coordinates and Enter ends the drag',
    await until('!s.busy && !s.pick', 'scroll ends') && drags.map(c => c.what ?? `ok ${c.ok}`).join() === 'down,move,up,ok 0'
    && drags[0].xd !== undefined, JSON.stringify(drags));
  v1 = await S('s.core.view');
  check("the core's view moved left (more on the left shown)", v1.xlo < v0.xlo && v1.xhi < v0.xhi, JSON.stringify([v0, v1]));
  await menuKeys('w', 'd');
  await until('!s.busy', 'default window');

  /* a checklist: phAsespace/Choose asks the period, then which variables to fold */
  await focusPlot();
  await menuKeys('a', 'c');
  if (await until("s.ask && s.ask.kind === 'string'", 'period')) {
    await until(`document.activeElement.closest('[role=dialog]')`, 'period focus');
    await key('Enter');
  }
  check('phAsespace/Choose opens a checklist of the variables', await until("s.ask && s.ask.kind === 'checklist'", 'checklist')
    && await cdp.eval(`document.querySelectorAll('[role=dialog] input[type=checkbox]').length`) === lists[0].length - 1,
    JSON.stringify(await S('s.ask')));
  check('its first box has the focus', await cdp.eval(`document.activeElement.type === 'checkbox'`));
  await cdp.eval(`document.querySelector('[role=dialog] input[type=checkbox]').click()`);
  await sleep(80);
  await cdp.eval(`[...document.querySelectorAll('[role=dialog] button')].find(b => b.textContent === 'OK').click()`);
  const ans = await lastAnswer();
  check('OK answers the checklist with its flags', await until('!s.busy && !s.ask', 'checklist answered')
    && Array.isArray(ans?.flags) && ans.flags[0] === 1 && ans.flags.slice(1).every(f => f === 0), JSON.stringify(ans));
  await focusPlot();
  await menuKeys('a', 'n'); /* phAsespace/None again */
  await until('!s.busy', 'torus off');
}

/* ---- the AUTO view (docs/ui-v2.md T11a) ------------------------------------------ */

/* the diagram from the `diagram` events on their own (docs/protocol.md), as
   tools/autocheck.py's Diagram does: what the store must equal */
function rebuildDiagram(events) {
  const pts = [];
  let labels = [];
  for (const e of events) {
    if (e.op === 'reset') {
      pts.length = Math.min(pts.length, e.keep);
      labels = labels.filter(l => l.point < e.keep);
    } else if (e.op === 'add') {
      if (e.from > pts.length) throw new Error(`diagram add from ${e.from} with ${pts.length} held`);
      pts.length = e.from;
      labels = labels.filter(l => l.point < e.from);
      for (const r of e.runs) {
        for (const [i, lab, sym] of r.lab || []) labels.push({point: pts.length + i, lab, sym});
        r.x.forEach((x, i) => pts.push({x, y: r.y[i], y2: (r.y2 || r.y)[i], br: r.br, pt: r.pt + i, ty: r.ty, d: r.d,
          c: r.c, lw: r.lw, f2: r.f2 || 0, nw: i === 0 && r.new ? 1 : 0, fr: i === 0 && r.from ? r.from : 0}));
      }
    }
  }
  return {pts, labels};
}

/* the curves the view must draw, counted from the store's points: one per
   run of points that share branch, kind and style (two, max and min, for a
   periodic run whose values differ) */
function expectedCurves(p) {
  let n = 0;
  const same = (a, b) => !p.nw[b] && ['br', 'ty', 'd', 'c', 'lw', 'f2'].every(f => p[f][a] === p[f][b]);
  for (let s = 0; s < p.x.length;) {
    let e = s;
    while (e + 1 < p.x.length && same(e, e + 1)) e++;
    if (p.d[s] !== 0) {
      n++;
      let two = false;
      for (let i = s; i <= e; i++) if ((p.d[i] === 2 || p.d[i] === 3) && p.y2[i] !== p.y[i]) two = true;
      if (two) n++;
    }
    s = e + 1;
  }
  return n;
}

const DG = () => cdp.eval('__xpp.diagram()');
const DS = expr => cdp.eval(`(() => { const d = __xpp.state().diagram; return ${expr}; })()`);
const autoButton = k => cdp.eval(`document.querySelector('.auto-tools button[aria-keyshortcuts=${k}]').click()`);
/* the diagram's plotting area and the screen position of (x, y) in it */
const autoArea = () => cdp.eval(`(() => { const r = document.querySelector('.auto-panel .u-over').getBoundingClientRect();
  return {x: r.left, y: r.top, w: r.width, h: r.height}; })()`);
async function autoScreen(x, y) {
  const [a, d] = [await autoArea(), await DG()];
  return {x: a.x + (x - d.x.min) / (d.x.max - d.x.min) * a.w, y: a.y + (d.y.max - y) / (d.y.max - d.y.min) * a.h};
}
const readout = () => cdp.eval(`document.querySelector('.auto-readout').textContent`);
const answerAsk = fields => cdp.eval(`__xpp.send(Object.assign({cmd: 'answer', id: __xpp.state().ask.id}, ${JSON.stringify(fields)}))`);
/* a menu's key, once its dialog is up (it takes its keys from then on) */
async function menuKey(k) {
  await until(`s.ask && document.activeElement.closest('[role=dialog]')`, `dialog for ${k}`);
  await key(k);
}
const center = sel => cdp.eval(`(() => { const r = document.querySelector('${sel}').getBoundingClientRect();
  return {x: r.left + r.width / 2, y: r.top + r.height / 2, h: r.height}; })()`);

const autoStatus = () => cdp.eval(`document.querySelector('[data-testid=auto-status]').textContent`);
/* the open axis dialog's minimum and maximum, typed */
async function setAxisRange(lo, hi) {
  await cdp.eval(`(() => { const d = document.querySelector('.auto-axis-dialog');
    for (const [f, v] of [['min', ${lo}], ['max', ${hi}]]) { const i = d.querySelector('input[data-field=' + f + ']');
      i.value = String(v); i.dispatchEvent(new Event('input', {bubbles: true})); } })()`);
}

async function autoView(dir) {
  await desktopMetrics();
  check('AUTO: the page connects', await until('s.hello && !s.busy', 'hello'));
  check('AUTO: no view before the core opens it', !(await cdp.eval(`!!document.querySelector('.auto-panel')`)));

  /* examples/scripts/lecar_auto.jsonl: the "hopf" parameter set, its fixed point as the IC */
  await key('f');
  await until('!s.busy', 'file menu');
  await key('g');
  await menuKey('d');
  await until('!s.busy', 'hopf set');
  await key('s');
  await menuKey('g');
  await until("s.ask && s.ask.kind === 'choice'", 'eigenvalues?');
  await menuKey('n');
  await until('!s.busy', 'fixed point', 30000);
  await cdp.eval(`__xpp.send({cmd: 'eqimport'})`);
  await until('!s.busy', 'import');
  await key('f');
  await until('!s.busy', 'file menu');
  await key('a');
  check('File/Auto opens the AUTO view', await until('s.diagram.open && s.diagram.shown && s.diagram.axes && !s.busy', 'auto open')
    && await cdp.eval(`!!document.querySelector('.auto-panel .auto-host')`), JSON.stringify(await DS('d.axes')));
  check("the diagram has the focus, so AUTO's keys work", await until(`document.activeElement.closest('.auto-host')`, 'auto focus'));
  const words = await cdp.eval(`[...document.querySelectorAll('.auto-panel button')].map(b => b.textContent.trim())`);
  check('the view has the AUTO buttons, no reDraw (the diagram is always current) and no Stop while idle (A10, T21)',
    ['Parameter', 'Axes', 'Numerics', 'Run', 'Grab', 'Mark values…', 'Clear', 'File', 'Save settings', 'Load settings', 'Close']
      .every(w => words.includes(w)) && !words.some(w => /abort|stop|redraw/i.test(w)), JSON.stringify(words));
  check('the status strip says AUTO is idle', /^Idle/.test(await autoStatus()), await autoStatus());
  check("T24: the status strip is the AUTO window's bottom line, as the main window's status bar",
    await cdp.eval(`(() => { const p = document.querySelector('.auto-panel').getBoundingClientRect(),
      t = document.querySelector('.auto-status').getBoundingClientRect(); return Math.abs(p.bottom - t.bottom) < 2; })()`));
  check('T26: at 1280px the AUTO view covers the whole viewport',
    await cdp.eval(`(() => { const r = document.querySelector('.auto-panel').getBoundingClientRect();
      return r.top === 0 && r.left === 0 && Math.abs(r.right - innerWidth) < 1 && Math.abs(r.bottom - innerHeight) < 1; })()`));
  const behind = await cdp.eval(`(() => { const at = sel => { const r = document.querySelector(sel).getBoundingClientRect();
    return !!document.elementFromPoint(r.left + r.width / 2, r.top + r.height / 2)?.closest('.auto-panel'); };
    return {menu: at('.menu-panel'), bar: at('.status-bar')}; })()`);
  check('T26: the main menu and the main status bar are behind it, not visible', behind.menu && behind.bar, JSON.stringify(behind));
  await cdp.eval(`document.querySelector('.auto-back').click()`);
  await until('!s.diagram.shown && s.diagram.open', 'auto back desktop');
  const front = await cdp.eval(`(() => { const at = sel => { const r = document.querySelector(sel).getBoundingClientRect();
    return !!document.elementFromPoint(r.left + r.width / 2, r.top + r.height / 2)?.closest(sel); };
    return {menu: at('.menu-panel'), bar: at('.status-bar'), gone: !document.querySelector('.auto-panel')}; })()`);
  check('T26: Back shows the main menu and the main status bar again', front.menu && front.bar && front.gone, JSON.stringify(front));
  await cdp.eval(`document.querySelector('.auto-show').click()`);
  await until('s.diagram.shown', 'auto show desktop');
  await until(`document.activeElement.closest('.auto-host')`, 'auto focus again');

  /* T26: a core error also sets the main status bar's `bottom` (StatusBar.tsx); with the main
     status bar hidden behind the AUTO view, its strip carries it instead. A bad %formula (not an
     `auto` command, so it does not disturb the `sent()` checks below) fails in
     json_state.cpp apply_value: "Bad formula" */
  await cdp.eval(`__xpp.send({cmd: 'set', kind: 'par', name: 'iapp', text: '%('})`);
  await until(`s.bottom === 'Bad formula'`, 'bottom message');
  check("T26: the core's last message shows in the AUTO strip",
    (await cdp.eval(`document.querySelector('.auto-message')?.textContent`)) === 'Bad formula');
  /* an error toast stays until dismissed (Toasts.tsx): clear every one so none sits over a
     later control (it floats above the AUTO view too, T26's dialog-backdrop note) */
  await cdp.eval(`document.querySelectorAll('.toast .icon').forEach(b => b.click())`);
  await until(`!s.toasts.length`, 'toasts dismissed');

  /* a dialog the view opens is on top of it, not behind (Numerics' form, T22: the page's own on the autosettings data) */
  check("T22: the store holds AUTO's settings (autosettings) before a run",
    await until('s.autoSettings.core && s.autoSettings.core.numerics.nmx > 0 && s.autoSettings.core.pars.length > 1', 'settings'),
    JSON.stringify(await S('s.autoSettings.core')).slice(0, 300));
  const sentNum = await cdp.eval('__xpp.sent().length');
  await autoButton('N');
  await until(`!!document.querySelector('.auto-settings-dialog[data-settings=auto-numerics]')`, 'numerics form');
  check("T22: Numerics opens the page's form, filled from the settings (Nmax), nothing asked of the core",
    await S(`String(s.autoSettings.core.numerics.nmx) === document.querySelector('.auto-settings-dialog input[data-field=nmx]').value
      && !s.ask && !s.busy`) && (await cdp.eval('__xpp.sent().length')) === sentNum,
    JSON.stringify(await cdp.eval('__xpp.sent().slice(-3)')));
  check("Numerics' form is above the AUTO view: its centre and its fields are the topmost elements",
    await cdp.eval(`(() => { const d = document.querySelector('.dialog'); if (!d) return false;
      const r = d.getBoundingClientRect(), f = d.querySelector('input').getBoundingClientRect();
      return d.contains(document.elementFromPoint(r.left + r.width / 2, r.top + r.height / 2))
        && d.contains(document.elementFromPoint(f.left + f.width / 2, f.top + f.height / 2)); })()`));
  await until(`!!document.activeElement.closest('.dialog')`, 'dialog focus'); /* Escape goes to the focused dialog */
  /* T21: Tab selects the next field's text, as a native Tab does, so typing replaces it */
  const second = await cdp.eval(`document.querySelectorAll('.dialog input')[1].value`);
  await key('Tab');
  await key('7');
  await key('7');
  check('Tab into a form field and typing replaces its value (not appends to it)',
    await cdp.eval(`document.activeElement === document.querySelectorAll('.dialog input')[1] && document.activeElement.value === '77'`),
    JSON.stringify([second, await cdp.eval(`document.activeElement.value`)]));
  /* T23: every field named plainly with AUTO's short name, its help a tooltip */
  const named = await cdp.eval(`[...document.querySelectorAll('.auto-settings-dialog .auto-num-group label')]
    .map(l => [l.querySelector('span').textContent, l.title, l.querySelector('input').dataset.field])`);
  check('T23: each of the 22 Numerics fields has a plain name with its short name, e.g. "Max points (NMX)", and a tooltip',
    named.length === 22 && named.every(([n, t]) => /^[A-Z][a-z].* \([A-Z0-9]+[A-Za-z]*\)$/.test(n) && t.length > 40)
      && named.some(([n, , f]) => n === 'Max points (NMX)' && f === 'nmx'), JSON.stringify(named.filter(([n, t]) => t.length <= 40 || !/\(/.test(n))));
  const typeInto = (field, text) => cdp.eval(`(() => { const i = document.querySelector('.auto-settings-dialog input[data-field=${field}]');
    i.value = ${JSON.stringify(text)}; i.dispatchEvent(new Event('input', {bubbles: true})); })()`);
  const fieldState = field => cdp.eval(`(() => { const i = document.querySelector('.auto-settings-dialog input[data-field=${field}]');
    const e = i.closest('label').querySelector('.field-error');
    return {invalid: i.getAttribute('aria-invalid'), error: e ? e.textContent : null,
      ok: document.querySelector('.auto-settings-dialog .dialog-actions .primary').disabled}; })()`);
  await typeInto('nmx', '12.5');
  const floatNmx = await fieldState('nmx');
  check('T23: a float in an integer field (Max points 12.5) is refused beside it, OK disabled',
    floatNmx.invalid === 'true' && floatNmx.error === 'Max points (NMX) must be a whole number, not 12.5' && floatNmx.ok === true,
    JSON.stringify(floatNmx));
  /* T31: Enter in it keeps the form and its message, sends nothing; the box is a whole-number box */
  await cdp.eval(`document.querySelector('.auto-settings-dialog input[data-field=nmx]').focus()`);
  await key('Enter');
  await sleep(200);
  check('T31: Enter in an integer AUTO Numerics field holding 12.5 keeps the form open and sends nothing',
    await cdp.eval(`!!document.querySelector('.auto-settings-dialog') && document.querySelector('.auto-settings-dialog input[data-field=nmx]').dataset.kind === 'integer'
      && document.querySelector('.auto-settings-dialog input[data-field=nmx]').inputMode === 'numeric'`)
    && !(await cdp.eval(`__xpp.sent().some(c => c.cmd === 'auto' && c.op === 'set')`)));
  await typeInto('nmx', '200');
  await typeInto('ds', '9');
  const bigDs = await fieldState('ds');
  check('T23: a first step larger than DSMAX is refused beside DS, OK disabled',
    bigDs.error === 'DS must be from DSMIN to DSMAX in size' && bigDs.ok === true, JSON.stringify(bigDs));
  await key('Escape');
  check('T22: Escape cancels it: nothing sent',
    await until(`!document.querySelector('.auto-settings-dialog')`, 'numerics cancelled')
    && !(await cdp.eval(`__xpp.sent().some(c => c.cmd === 'auto' && c.op === 'set')`)));

  /* Run / Steady state */
  await autoButton('R');
  check('Run asks how to start', await until("s.ask && s.ask.kind === 'menu' && s.ask.title === 'Start'", 'start menu'),
    JSON.stringify(await S('s.ask')));
  await menuKey('s');
  check('the steady-state branch arrives', await until('!s.busy && s.diagram.points.x.length > 10', 'steady state', 60000));
  const nSteady = await DS('d.points.x.length');
  const stop = await DS('d.stop');
  check('T21, T23: the status strip says why the run stopped (the core\'s reason), steady states, its branch and point, its points, its last label, the time',
    await until(`document.querySelector('.auto-status').dataset.phase === 'done'`, 'strip done')
    && stop && stop.br === 1 && /^(parameter iapp reached Par (Min|Max)|the norm reached|the branch reached Max points)/.test(stop.text)
    && new RegExp(`^Stopped: ${stop.text.replace(/[()]/g, '\\$&')} · steady states · branch 1, point ${stop.pt} · ${nSteady} points `
      + `· last label EP \\d+ \\(End point\\) at point \\d+ · [\\d.:]+ s?`).test(await autoStatus())
    && await cdp.eval(`document.querySelector('[data-testid=auto-status]').dataset.why`) === stop.why,
    JSON.stringify([stop, await autoStatus()]));
  const out = await cdp.eval(`(() => { const d = document.querySelector('.auto-output'); d.open = true;
    return {sum: d.querySelector('summary').textContent, text: d.querySelector('pre') && d.querySelector('pre').textContent}; })()`);
  check("T21: the Output panel shows AUTO's table", /^Output \(\d+\)$/.test(out.sum) && out.sum !== 'Output (0)'
    && /BR\s+PT\s+TY/.test(out.text || ''), JSON.stringify(out).slice(0, 300));
  const reasonLine = stop ? `Branch 1 stopped at point ${stop.pt}: ${stop.text}` : '?';
  check('T23: ... and the reason as a line of its own',
    await until(`document.querySelector('.auto-output pre').textContent.split('\\n').includes(${JSON.stringify(reasonLine)})`,
      'reason in output'), (await cdp.eval(`document.querySelector('.auto-output pre').textContent`)).slice(-300));
  const key23 = await cdp.eval(`[...document.querySelectorAll('.auto-legend-label')].map(l => [l.dataset.sym, l.dataset.shape,
    l.querySelector('.auto-label-mark').textContent, l.textContent.trim(), l.title])`);
  check('T23: the key spells out the label types the diagram has (EP End point, HB Hopf), each with its meaning as a tooltip',
    key23.some(([s, , , t]) => s === 'EP' && /EP End point$/.test(t)) && key23.some(([s, , , t]) => s === 'HB' && /HB Hopf$/.test(t))
      && key23.every(([, , , , h]) => h.length > 30), JSON.stringify(key23));
  /* T29: one shape per label type (HB a filled circle, LP a triangle, ... a cross for a code this
     diagram does not name), the key's glyph the same mapping the diagram itself draws with */
  const SHAPE_OF = {EP: 'bar', MX: 'cross', LP: 'triangle', HB: 'circle', BP: 'diamond', PD: 'square', TR: 'star', UZ: 'invTriangle'};
  const GLYPH_OF = {circle: '●', triangle: '△', diamond: '◇', square: '□', star: '☆',
    invTriangle: '▽', cross: '×', bar: '❙'};
  check('T29: the key\'s shape for each present code matches the type -> shape mapping, drawn with that shape\'s glyph',
    key23.length > 0 && key23.every(([s, shape, mark]) => shape === (SHAPE_OF[s] ?? 'cross') && mark === GLYPH_OF[shape]),
    JSON.stringify(key23));
  check('after the run the store holds its last point\'s stability circle (autoinfo)',
    await until('s.diagram.stab && s.diagram.stab.circle.length === 2 && s.diagram.stab.periodic === 0', 'run stab'),
    JSON.stringify(await DS('[d.info, d.stab]')));

  /* Grab from the keyboard only (T11b): G on the diagram, a step, Tab to the Hopf point, Enter */
  await cdp.eval(`document.querySelector('.auto-host').focus()`);
  await key('g');
  check('G grabs in the view: a grab mode on the diagram, no dialog, the cursor on the first point',
    await until("s.ask && s.ask.kind === 'grab' && s.diagram.grabbing && s.diagram.info && s.diagram.info.point === 0", 'grab')
    && await cdp.eval(`!document.querySelector('[role=dialog]') && !!document.querySelector('.pick-bar[data-pick=grab]')
      && !!document.activeElement.closest('.auto-host')`), JSON.stringify(await DS('[d.grabbing, d.info]')));
  const nInfo = await DS('d.infoEvents');
  await key(']');
  check('] steps the cursor: the store\'s autoinfo follows (the next point, its strip and circle)',
    await until(`s.diagram.infoEvents > ${nInfo} && s.diagram.info.point === 1 && s.diagram.info.pt === 2 && s.ask`, 'grab step')
    && (await lastAnswer())?.point === 1 && await until('s.diagram.hover && s.diagram.hover.point === 1', 'readout follows'),
    JSON.stringify([await DS('d.info'), await lastAnswer()]));
  check('the info panel shows the point, the circle its eigenvalues',
    /Branch\s*1/.test(await cdp.eval(`document.querySelector('.auto-info').textContent`))
    && (await cdp.eval(`document.querySelectorAll('.auto-stab-list li').length`)) === 2
    && /Stability circle: 2 eigenvalues/.test(await cdp.eval(`document.querySelector('.auto-stab svg').getAttribute('aria-label')`)),
    await cdp.eval(`document.querySelector('.auto-info')?.textContent`));
  await key('Tab');
  check('Tab jumps to the next label, the Hopf point: its strip and a pair of eigenvalues on the imaginary axis',
    await until("s.diagram.info.sym === 'HB' && s.ask && s.ask.kind === 'grab'", 'grab tab')
    && await DS('d.stab.eig.length === 2 && d.stab.eig.every(e => Math.abs(e[0]) < 1e-3 && Math.abs(e[1]) > 0.1)')
    && /HB/.test(await cdp.eval(`document.querySelector('.auto-info').textContent`))
    && await cdp.eval(`!!document.activeElement.closest('.auto-host')`), JSON.stringify(await DS('[d.info, d.stab]')));
  const hbLab = await DS('d.info.lab');
  await key('Enter');
  check('Enter takes it: the grab ends', await until('!s.busy && !s.diagram.grabbing', 'grabbed')
    && (await lastAnswer())?.key === 'Return' && !(await cdp.eval(`!!document.querySelector('.pick-bar')`)),
    JSON.stringify(await lastAnswer()));
  await cdp.eval(`document.querySelector('.auto-host').focus()`);
  await key('r');
  check('R, from the Hopf point, offers the periodic branch',
    await until("s.ask && s.ask.kind === 'menu' && s.ask.title === 'Hopf Pt'", 'hopf menu'), JSON.stringify(await S('s.ask')));
  await menuKey('p');
  /* T21, during the run: the strip names it and has the Stop; the axis dialog changes the range, not the variable */
  check('T21: while it runs the status strip says "Running: periodic orbits", with branch 2, its point count and a Stop',
    await until(`/^Running: periodic orbits · branch 2, point \\d+ · \\d+ points/.test(document.querySelector('[data-testid=auto-status]').textContent)
      && !!document.querySelector('.auto-status .auto-stop')`, 'strip running', 20000), await autoStatus());
  await cdp.eval(`document.querySelector('.auto-axis-name[data-axis=y]').click()`);
  const running = await until(`s.busy && document.querySelector('.auto-axis-dialog[data-axis=y]')`, 'axis dialog');
  const dlg = await cdp.eval(`(() => { const d = document.querySelector('.auto-axis-dialog');
    return {plot: d.querySelector('select[data-field=plot]').disabled, yvar: d.querySelector('select[data-field=yvar]').disabled,
      min: d.querySelector('input[data-field=min]').disabled}; })()`);
  await setAxisRange(-0.6, 0.7);
  check('T22: during the run the axis dialog\'s plot type and variable can change (they would wait), and min/max change the view at once',
    running && !dlg.plot && !dlg.yvar && !dlg.min && await until(`s.busy && s.diagram.viewport.y && s.diagram.viewport.y.min === -0.6
      && s.diagram.viewport.y.max === 0.7`, 'axis range during run', 3000)
      && !(await cdp.eval(`__xpp.sent().some(c => c.cmd === 'auto' && (c.op === 'axes' || c.op === 'set'))`)),
    JSON.stringify([running, dlg, await DS('d.viewport'), await S('s.busy')]));
  check('the periodic branch arrives', await until('!s.busy && s.diagram.points.br.includes(2)', 'periodic', 120000));
  await key('Escape');
  check('Escape closes the axis dialog, the focus back on the axis name',
    await until(`!document.querySelector('.auto-axis-dialog') && document.activeElement.matches('.auto-axis-name[data-axis=y]')`, 'axis closed'));
  await cdp.eval(`document.querySelector('.auto-panel .plot-tools button:nth-child(2)').click()`); /* Reset view */
  await until('s.diagram.viewport.y === null', 'view reset');
  check('its first point says it started from the Hopf label; the circle holds Floquet multipliers',
    await DS(`d.points.fr[d.points.br.indexOf(2)] === ${hbLab}`) && await DS('d.stab.periodic === 1 && d.stab.circle.length === 2'),
    JSON.stringify(await DS('[d.points.fr.filter(f => f), d.stab]')));

  /* the store is the events, exactly */
  const want = rebuildDiagram(await cdp.eval('__xpp.diagramEvents()'));
  const got = await DS('d.points');
  let bad = want.pts.length === got.x.length ? null : `${got.x.length} points held, ${want.pts.length} sent`;
  const same = (a, b) => (a === null ? b === null : a === b); /* NaN arrives as null through JSON */
  for (let i = 0; i < want.pts.length && !bad; i++) {
    for (const f of ['x', 'y', 'y2', 'br', 'pt', 'ty', 'd', 'c', 'lw', 'f2', 'nw'])
      if (!same(want.pts[i][f], got[f][i])) bad = `point ${i} ${f}: ${got[f][i]} vs ${want.pts[i][f]}`;
  }
  const labels = await DS('d.labels');
  const fr = await DS('d.points.fr');
  for (let i = 0; i < want.pts.length && !bad; i++) if (fr[i] !== want.pts[i].fr) bad = `point ${i} fr: ${fr[i]} vs ${want.pts[i].fr}`;
  check(`the store's diagram equals the diagram events (${want.pts.length} points, ${want.labels.length} labels)`,
    !bad && want.pts.length > 1000 && JSON.stringify(want.labels) === JSON.stringify(labels), bad || JSON.stringify(labels.slice(0, 5)));

  /* T27: AUTO's table in the Output panel: a row per label, the plain ones NPr prints (no type) too,
     each line of the core's log classified alone however its chunks arrived */
  const outRows = await cdp.eval(`__xpp.state().log.filter(l => l.kind === 'auto')
    .map(l => /^\\s*-?\\d+\\s+-?\\d+\\s+(\\S\\S)?\\s+(-?\\d+)\\s/.exec(l.text)).filter(m => m).map(m => (m[1] || '') + m[2])`);
  const labelRows = labels.map(l => l.sym + l.lab);
  check(`T27: the Output panel has AUTO's row for each of the ${labels.length} labels, the ${labels.filter(l => !l.sym).length} plain ones (NPr's) too`,
    labels.some(l => !l.sym) && labelRows.every(r => outRows.includes(r)),
    JSON.stringify([labelRows.filter(r => !outRows.includes(r)), outRows.slice(0, 10)]));

  /* the chart: a curve per branch and stability run, the label marks */
  const dg = await DG();
  const nCurves = expectedCurves(got);
  check(`the chart draws one curve per branch and stability run (${nCurves}), every label marked`,
    dg && dg.curves.length === nCurves && dg.labels.length === want.labels.length
    && dg.curves.reduce((n, c) => n + c.points, 0) > want.pts.length, JSON.stringify(dg && [dg.curves.length, nCurves, dg.labels.length]));
  const steady = dg.curves.filter(c => c.kind === 'steady');
  check('stable branches are solid, unstable ones dashed; periodic ones have max and min',
    steady.some(c => c.stable && !c.dashed) && steady.some(c => !c.stable && c.dashed)
    && dg.curves.some(c => c.kind === 'periodic' && c.which === 'y2'), JSON.stringify(dg.curves.slice(0, 6)));
  const hb = want.labels.find(l => l.sym === 'HB');
  const joined = dg.curves.filter(c => c.hopf >= 0);
  check("the periodic branch starts at its Hopf point: its max and min lines begin at the HB point",
    hb && joined.length === 2 && joined.every(c => c.hopf === hb.point && c.first[2] === hb.point && c.branch === 2
      && c.first[0] === want.pts[hb.point].x && c.first[1] === want.pts[hb.point].y), JSON.stringify({hb, joined}));

  /* T21: grabbing a labelled periodic point plots its limit cycle in the main window (File/Import orbit) */
  await cdp.eval(`document.querySelector('.auto-host').focus()`);
  await key('g');
  await until("s.ask && s.ask.kind === 'grab' && s.diagram.info", 'grab periodic');
  for (let i = 0; i < 12 && !(await DS('d.info && d.info.type >= 3 && d.info.lab > 0')); i++) {
    const n = await DS('d.infoEvents');
    await key('Tab');
    await until(`s.diagram.infoEvents > ${n} && s.ask`, 'grab tab periodic');
  }
  const grabbed = await DS('d.info');
  const sentGrab = await cdp.eval('__xpp.sent().length');
  await key('Enter');
  check('T21: grabbing a labelled periodic point imports its orbit: the main plot shows the limit cycle',
    grabbed && grabbed.type >= 3 && await until(`!s.busy && s.core.rows > 10 && w.series && w.series.rows === s.core.rows`, 'orbit', 20000)
    && JSON.stringify((await cdp.eval(`__xpp.sent().slice(${sentGrab})`)).filter(c => c.cmd === 'auto' || c.key === 'i')
      .map(c => c.op || c.key)) === JSON.stringify(['file', 'i']),
    JSON.stringify([grabbed && [grabbed.br, grabbed.pt, grabbed.type, grabbed.lab], await S('[s.core.rows, w.series && w.series.rows]'),
      await cdp.eval(`__xpp.sent().slice(${sentGrab})`)]));

  /* hover names the Hopf point */
  const hbAt = await autoScreen(want.pts[hb.point].x, want.pts[hb.point].y);
  await mouse('mouseMoved', hbAt.x, hbAt.y);
  check('hovering the Hopf point names it in the readout',
    await until(`s.diagram.hover && s.diagram.hover.point === ${hb.point}`, 'hover hb') && /HB label \d+/.test(await readout()),
    JSON.stringify([await DS('d.hover'), await readout()]));
  await mouse('mouseMoved', 5, 5);
  /* and so does stepping from label to label with the keyboard */
  await until('!s.diagram.hover', 'hover gone');
  await cdp.eval(`document.querySelector('.auto-host').focus()`);
  let steps = 0;
  while (steps < 5 && !(await DS(`!!d.hover && d.labels.some(l => l.point === d.hover.point && l.sym === 'HB')`))) {
    await key('>');
    steps++;
  }
  check('> steps from label to label to the Hopf point', steps > 0 && /HB label/.test(await readout())
    && (await DS('d.hover.point')) === hb.point, JSON.stringify([steps, await readout()]));
  await key(']');
  check('] steps to the next point of the branch', await until(`s.diagram.hover && s.diagram.hover.point === ${hb.point + 1}`, 'next point'),
    JSON.stringify(await DS('d.hover')));

  /* zoom, pan, undo */
  const a = await autoArea(), cx = a.x + a.w / 2, cy = a.y + a.h / 2;
  const axes = await DS('d.axes'), h0 = await DS('d.viewportHistory.length');
  await mouse('mouseWheel', cx, cy, {deltaX: 0, deltaY: -120});
  check('the wheel zooms the diagram', await until('s.diagram.viewport.x', 'auto wheel')
    && width((await DG()).x) < (axes.xmax - axes.xmin) * 0.9, JSON.stringify(await DS('d.viewport')));
  const z1 = await DS('d.viewport');
  await mouse('mouseMoved', cx - 60, cy - 40);
  await mouse('mousePressed', cx - 60, cy - 40, {button: 'left', buttons: 1, clickCount: 1});
  for (let st = 1; st <= 6; st++) await mouse('mouseMoved', cx - 60 + 20 * st, cy - 40 + 14 * st, {button: 'left', buttons: 1});
  await mouse('mouseReleased', cx + 60, cy + 44, {button: 'left', buttons: 0, clickCount: 1});
  check('a box zooms to it', await until(`s.diagram.viewport.x && s.diagram.viewport.x.max - s.diagram.viewport.x.min < ${width(z1.x) * 0.8}`, 'auto box')
    && (await DS('d.viewportHistory.length')) === h0 + 2, JSON.stringify(await DS('d.viewport')));
  await cdp.eval(`[...document.querySelectorAll('.auto-panel button')].find(b => b.textContent === 'Undo zoom').click()`);
  check('Undo zoom goes back one step', await until(`s.diagram.viewport.x && Math.abs(s.diagram.viewport.x.min - ${z1.x.min}) < 1e-12`, 'auto undo'));
  await cdp.eval(`document.querySelector('.auto-host').focus()`);
  await key('z', 2);
  check('Ctrl+Z on the diagram undoes the wheel', await until('s.diagram.viewport.x === null', 'auto ctrl z'));
  await key('ArrowLeft');
  check('the arrow keys pan it', await until('s.diagram.viewport.x', 'auto pan'));
  await key('0');
  check("0 goes back to AUTO's axes", await until('s.diagram.viewport.x === null', 'auto reset')
    && Math.abs((await DG()).x.min - axes.xmin) < 1e-9);

  /* T30: the corner Fit (and the AUTO tools' own Fit) fit the view to the
     branches shown, client-side, undoable like any other zoom */
  check('the corner Fit sits over the diagram while it has points',
    await cdp.eval(`!!document.querySelector('.auto-host .plot-fit')`));
  const dataExtent = await DS(`(() => {
    let xmin = Infinity, xmax = -Infinity, ymin = Infinity, ymax = -Infinity;
    for (let i = 0; i < d.points.x.length; i++) {
      if (d.points.x[i] < xmin) xmin = d.points.x[i]; if (d.points.x[i] > xmax) xmax = d.points.x[i];
      if (d.points.y[i] < ymin) ymin = d.points.y[i]; if (d.points.y[i] > ymax) ymax = d.points.y[i];
      if (d.points.y2[i] < ymin) ymin = d.points.y2[i]; if (d.points.y2[i] > ymax) ymax = d.points.y2[i];
    }
    return {xmin, xmax, ymin, ymax};
  })()`);
  await cdp.eval(`document.querySelector('.auto-host').focus()`);
  for (let st = 0; st < 15; st++) await key('ArrowRight'); /* scrolled well past the data, wrong-corner style */
  await until('s.diagram.viewport.x', 'panned away');
  const away = await DG();
  check('panned far from the data', away.x.min > dataExtent.xmax, JSON.stringify([away.x, dataExtent]));
  const fitCondition = e => `(() => { const g = __xpp.diagram(); return !!g && g.x.min <= ${e.xmin} + 1e-6 && g.x.max >= ${e.xmax} - 1e-6
    && g.y.min <= ${e.ymin} + 1e-6 && g.y.max >= ${e.ymax} - 1e-6; })()`;
  await cdp.eval(`document.querySelector('.auto-host .plot-fit').click()`);
  check('the corner Fit brings every curve back into view',
    await until(fitCondition(dataExtent), 'fit applied'), JSON.stringify([await DG(), dataExtent]));
  await cdp.eval(`[...document.querySelectorAll('.auto-panel button')].find(b => b.textContent === 'Undo zoom').click()`);
  check('the corner Fit is an undoable zoom, like any other: Undo zoom goes back to the panned-away view',
    await until(`s.diagram.viewport.x && Math.abs(s.diagram.viewport.x.min - ${away.x.min}) < 1e-6`, 'undo fit'));
  await key('0');
  await until('s.diagram.viewport.x === null', 'auto reset 2');
  for (let st = 0; st < 15; st++) await key('ArrowLeft');
  await until('s.diagram.viewport.x', 'panned away 2');
  await cdp.eval(`[...document.querySelectorAll('.auto-panel .plot-tools button')].find(b => b.textContent === 'Fit').click()`);
  check("the AUTO tools' own Fit does the same as the corner button",
    await until(fitCondition(dataExtent), 'fit2 applied'), JSON.stringify([await DG(), dataExtent]));
  await key('0');
  await until('s.diagram.viewport.x === null', 'auto reset 3');

  /* Escape cancels a grab */
  await key('g');
  await until("s.ask && s.ask.kind === 'grab'", 'grab again');
  await key('Escape');
  check('Escape cancels a grab', await until('!s.busy && !s.diagram.grabbing', 'grab cancelled')
    && (await lastAnswer())?.ok === 0 && (await DS('d.shown')), JSON.stringify(await lastAnswer()));

  /* Axes/Zoom: a box drawn on the diagram, answered in its data coordinates (T11b) */
  await cdp.eval(`document.querySelector('.auto-host').focus()`);
  await key('a');
  await until("s.ask && s.ask.kind === 'menu' && s.ask.title === 'Plot Type'", 'axes menu');
  await menuKey('z');
  check('Axes/Zoom is a box mode on the diagram, not a dialog',
    await until("s.pick && s.pick.win === 101 && s.pick.mode === 'box'", 'auto box mode')
    && await cdp.eval(`!document.querySelector('[role=dialog]') && !!document.querySelector('.auto-panel .pick-bar')`),
    JSON.stringify(await S('[s.ask, s.pick]')));
  const za = await autoArea(), zd = await DG();
  const zx = f => zd.x.min + f * (zd.x.max - zd.x.min), zy = f => zd.y.max - f * (zd.y.max - zd.y.min);
  await mouse('mouseMoved', za.x + 0.3 * za.w, za.y + 0.2 * za.h);
  await mouse('mousePressed', za.x + 0.3 * za.w, za.y + 0.2 * za.h, {button: 'left', buttons: 1, clickCount: 1});
  for (let st = 1; st <= 5; st++)
    await mouse('mouseMoved', za.x + (0.3 + 0.06 * st) * za.w, za.y + (0.2 + 0.08 * st) * za.h, {button: 'left', buttons: 1});
  await mouse('mouseReleased', za.x + 0.6 * za.w, za.y + 0.6 * za.h, {button: 'left', buttons: 0, clickCount: 1});
  const zAns = await lastAnswer();
  await until('!s.busy', 'auto zoomed');
  const zAxes = await DS('d.axes');
  const px = (zd.x.max - zd.x.min) / axes.wid * 2, py = (zd.y.max - zd.y.min) / axes.hgt * 2;
  check("the box, in the diagram's data coordinates, becomes AUTO's axes (within a pixel)",
    zAns && 'xd2' in zAns && Math.abs(zAxes.xmin - zx(0.3)) < px && Math.abs(zAxes.xmax - zx(0.6)) < px
    && Math.abs(zAxes.ymax - zy(0.2)) < py && Math.abs(zAxes.ymin - zy(0.6)) < py && await DS('d.viewport.x === null'),
    JSON.stringify([zAns, zAxes, [zx(0.3), zx(0.6), zy(0.6), zy(0.2)]]));
  /* back to AUTO's first axes for what follows */
  await key('a');
  await until("s.ask && s.ask.kind === 'menu' && s.ask.title === 'Plot Type'", 'axes menu 2');
  await menuKey('f');
  await until('!s.busy', 'auto fit');

  /* T21: an Axes change draws the diagram again in the new quantities by itself (no reDraw) */
  const nAll = await DS('d.points.x.length'), y0 = await DS('d.points.y.slice(0, 50)');
  const sentAxes = await cdp.eval('__xpp.sent().length');
  await cdp.eval(`document.querySelector('.auto-host').focus()`);
  await key('a');
  await menuKey('n');
  await until("s.ask && s.ask.kind === 'form'", 'autoplot form');
  await until(`!!document.activeElement.closest('.dialog')`, 'autoplot focus');
  await key('Enter');
  check('T21: Axes/Norm, OK: the diagram holds all its points again, in norms, without a reDraw',
    await until(`!s.busy && s.diagram.axes.plot === 1 && s.diagram.points.x.length === ${nAll}`, 'norm axes')
    && JSON.stringify(await DS('d.points.y.slice(0, 50)')) !== JSON.stringify(y0)
    && !(await cdp.eval(`__xpp.sent().slice(${sentAxes}).some(c => c.cmd === 'redraw' || c.op === 'redraw')`))
    && (await DG()).curves.length > 0, JSON.stringify(await DS('[d.axes, d.points.x.length]')));
  /* and the axis dialog does the same: hI-lo from its Plots select, then a Fit */
  await cdp.eval(`document.querySelector('.auto-axis-name[data-axis=y]').click()`);
  await until(`document.querySelector('.auto-axis-dialog select[data-field=plot]')`, 'y dialog');
  await cdp.eval(`(() => { const s = document.querySelector('.auto-axis-dialog select[data-field=plot]'); s.value = '2';
    s.dispatchEvent(new Event('change', {bubbles: true})); })()`);
  check('T22: the axis dialog\'s plot type goes to the core as an auto set with a Fit: hI-lo, every point, periodic max and min',
    await until(`!s.busy && s.diagram.axes.plot === 2 && s.diagram.points.x.length === ${nAll}
      && document.querySelector('.auto-axis-dialog select[data-field=yvar]')`, 'hilo axes', 20000)
    && (await DG()).curves.some(c => c.which === 'y2')
    && await cdp.eval(`__xpp.sent().some(c => c.cmd === 'auto' && c.op === 'set' && c.axes && c.axes.plot === 2 && c.axes.fit)`),
    JSON.stringify(await DS('d.axes')));
  const fitted = await DS('d.axes');
  await setAxisRange(fitted.ymin - 0.1, fitted.ymax + 0.1);
  check("T21: its min and max change the view at once, the core's axes stay",
    await until(`s.diagram.viewport.y && Math.abs(s.diagram.viewport.y.min - ${fitted.ymin - 0.1}) < 1e-9`, 'y range')
    && (await DS('d.axes.ymin')) === fitted.ymin, JSON.stringify(await DS('[d.viewport, d.axes]')));
  await cdp.eval(`document.querySelector('.auto-axis-dialog .dialog-actions button').click()`);
  await until('!document.querySelector(".auto-axis-dialog")', 'y dialog closed');
  await cdp.eval(`document.querySelector('.auto-panel .plot-tools button:nth-child(2)').click()`);
  await until('s.diagram.viewport.y === null', 'view reset 2');

  /* T21: Clear hides the branches so far in the view, the key shows them again; nothing goes to the core */
  const nCurvesAll = (await DG()).curves.length, sentClear = await cdp.eval('__xpp.sent().length');
  await autoButton('C');
  check('T21: Clear hides every branch so far; the key offers "Earlier branches (2)"',
    await until(`__xpp.diagram().curves.length === 0 && s.diagram.earlier === ${nAll}`, 'cleared')
    && /Earlier branches \(2\)/.test(await cdp.eval(`document.querySelector('.auto-earlier').textContent`))
    && (await cdp.eval(`__xpp.sent().length`)) === sentClear, await cdp.eval(`document.querySelector('.auto-legend').textContent`));
  await cdp.eval(`document.querySelector('.auto-earlier').click()`);
  check('T21: "Earlier branches" shows them again', await until(`__xpp.diagram().curves.length === ${nCurvesAll}`, 'earlier shown'));

  /* T21: Numerics and Axes saved to a file and loaded back (T22: from and into the settings data) */
  const sentSave = await cdp.eval('__xpp.sent().length');
  await cdp.eval(`[...document.querySelectorAll('.auto-tools button')].find(b => b.textContent === 'Save settings').click()`);
  check('T22: Save settings writes the Numerics (22 values), the Axes (plot type and 7 values), the parameters and Mark values, asking the core nothing',
    await until('!s.busy && s.diagram.setupSaved', 'settings saved', 20000)
    && await DS(`(() => { const o = JSON.parse(d.setupSaved); return o.xppautX === 'auto-settings' && o.version === 2
      && Object.keys(o.numerics).length === 22 && o.plot === 2 && Object.keys(o.axes).length === 7 && o.axes['Main Parm'] === 'iapp'
      && o.pars[0] === 'iapp' && Array.isArray(o.marks); })()`)
    && !(await S('s.ask')) && (await cdp.eval('__xpp.sent().length')) === sentSave,
    JSON.stringify([await DS('d.setupSaved'), await S('[s.busy, s.ask]'), await cdp.eval('__xpp.sent().slice(-6)')]));
  const saved = JSON.parse(await DS('d.setupSaved'));
  saved.numerics.Nmax = '321';
  saved.plot = 1;
  saved.axes.Xmin = '0.01';
  saved.axes.Xmax = '0.4';
  const setFile = path.join(dir, 'lecar-auto.json');
  fs.writeFileSync(setFile, JSON.stringify(saved));
  await pickFiles('#auto-settings-load', [setFile]);
  check('T22: Load settings sets them (one auto set): norm plot at the file\'s x range',
    await until(`!s.busy && !s.ask && s.diagram.axes.plot === 1 && s.diagram.axes.xmin === 0.01 && s.diagram.axes.xmax === 0.4
      && s.diagram.points.x.length === ${nAll}`, 'settings loaded', 20000), JSON.stringify(await DS('d.axes')));
  await autoButton('N');
  await until(`!!document.querySelector('.auto-settings-dialog[data-settings=auto-numerics]')`, 'numerics again');
  check("T22: ... and the Numerics (Nmax 321): the core's settings and the form",
    await S(`s.autoSettings.core.numerics.nmx === 321
      && document.querySelector('.auto-settings-dialog input[data-field=nmx]').value === '321'`)
    && (await cdp.eval(`__xpp.sent().filter(c => c.cmd === 'auto' && c.op === 'set').length`)) === 2,
    JSON.stringify([await S('s.autoSettings.core.numerics'), await cdp.eval(`__xpp.sent().filter(c => c.cmd === 'auto')`)]));
  await until(`!!document.activeElement.closest('.dialog')`, 'numerics focus');
  await key('Escape');
  await until(`!document.querySelector('.auto-settings-dialog')`, 'numerics closed');
  /* T22: Parameter lists AUTO's parameters; Mark values adds a user point and takes it away again */
  await autoButton('P');
  await until(`!!document.querySelector('.auto-settings-dialog[data-settings=auto-pars]')`, 'parameter form');
  check("T22: Parameter is the page's form: a select per AUTO parameter, set to the settings' names",
    await S(`(() => { const sel = [...document.querySelectorAll('.auto-settings-dialog select')];
      return sel.length === s.autoSettings.core.pars.length && sel.every((e, i) => e.value === s.autoSettings.core.pars[i]); })()`),
    JSON.stringify(await S('s.autoSettings.core.pars')));
  await key('Escape');
  await until(`!document.querySelector('.auto-settings-dialog')`, 'parameter closed');
  await autoButton('U');
  await until(`!!document.querySelector('.auto-settings-dialog[data-settings=auto-marks]')`, 'marks form');
  await cdp.eval(`document.querySelector('.auto-mark-add').click()`);
  await until(`!!document.querySelector('.auto-settings-dialog input[data-field=mark1-value]')`, 'mark row');
  await cdp.eval(`(() => { const d = document.querySelector('.auto-settings-dialog');
    const sel = d.querySelector('select[data-field=mark1-name]'); sel.value = 'iapp'; sel.dispatchEvent(new Event('change', {bubbles: true}));
    const i = d.querySelector('input[data-field=mark1-value]'); i.value = '0.125'; i.dispatchEvent(new Event('input', {bubbles: true})); })()`);
  await cdp.eval(`document.querySelector('.auto-settings-dialog .dialog-actions .primary').click()`);
  check('T22: Mark values: iapp = 0.125 added, the core has it',
    await until(`!s.busy && s.autoSettings.core.marks.length === 1 && s.autoSettings.core.marks[0][0] === 'iapp'
      && s.autoSettings.core.marks[0][1] === 0.125`, 'mark set'), JSON.stringify(await S('s.autoSettings.core.marks')));
  await autoButton('U');
  await until(`!!document.querySelector('.auto-settings-dialog input[data-field=mark1-value]')`, 'marks form again');
  await cdp.eval(`document.querySelector('.auto-settings-dialog .auto-mark-row button').click()`);
  await cdp.eval(`document.querySelector('.auto-settings-dialog .dialog-actions .primary').click()`);
  check('T22: ... and removed again', await until('!s.busy && s.autoSettings.core.marks.length === 0', 'mark removed'),
    JSON.stringify(await S('s.autoSettings.core.marks')));
  /* back to hI-lo, fitted, for what follows */
  await cdp.eval(`document.querySelector('.auto-host').focus()`);
  await key('a');
  await menuKey('i');
  await until("s.ask && s.ask.kind === 'form'", 'autoplot form 2');
  await until(`!!document.activeElement.closest('.dialog')`, 'autoplot focus 2');
  await key('Enter');
  await until('!s.busy && s.diagram.axes.plot === 2', 'hilo again');
  await key('a');
  await menuKey('f');
  await until('!s.busy', 'fit again');

  /* on a phone: a sheet with Back, 44 px targets, no sideways scroll */
  await cdp.send('Emulation.setDeviceMetricsOverride', {width: 390, height: 844, deviceScaleFactor: 2, mobile: true});
  await cdp.send('Emulation.setTouchEmulationEnabled', {enabled: true, maxTouchPoints: 5});
  await cdp.send('Emulation.setEmulatedMedia', {features: [{name: 'pointer', value: 'coarse'}, {name: 'hover', value: 'none'}]}).catch(() => {});
  await sleep(400);
  const sheet = await cdp.eval(`(() => { const r = document.querySelector('.auto-panel').getBoundingClientRect();
    return {l: r.left, t: r.top, w: r.width, bottom: r.bottom, ih: innerHeight, iw: innerWidth,
      doc: document.documentElement.scrollWidth, body: document.body.scrollWidth}; })()`);
  check('390x844: the AUTO view is a full-screen sheet, over the main status bar too (its own Stop stays in view), no sideways scroll',
    sheet.l === 0 && sheet.t === 0 && sheet.w >= sheet.iw - 1 && Math.abs(sheet.bottom - sheet.ih) <= 1
    && sheet.doc <= sheet.iw && sheet.body <= sheet.iw, JSON.stringify(sheet));
  const small = await cdp.eval(`[...document.querySelectorAll('.auto-panel button')].filter(b => b.getClientRects().length)
    .map(b => [b.textContent.trim(), b.getBoundingClientRect().height, b.getBoundingClientRect().width])
    .filter(([, h, w]) => h < 44 || w < 44)`);
  check("390x844: the sheet's targets are at least 44 px", small.length === 0, JSON.stringify(small));
  const plotW = (await autoArea()).w;
  check("390x844: the diagram fills the sheet's width", plotW > 250 && plotW <= 390, String(plotW));
  await touch('touchStart', [await center('.auto-back')]);
  await touch('touchEnd', []);
  check('Back hides the sheet, AUTO stays open', await until('!s.diagram.shown && s.diagram.open', 'auto back')
    && await cdp.eval(`!document.querySelector('.auto-panel') && !!document.querySelector('.auto-show')`));
  check('T21: the focus goes back to the main plot (Show AUTO is a Tab away)',
    await until(`document.activeElement.closest('.plot-host') && !document.activeElement.closest('.auto-panel')`, 'plot focus after back'));
  const show = await center('.auto-show');
  await touch('touchStart', [show]);
  await touch('touchEnd', []);
  check('Show AUTO (44 px) brings the sheet back with its diagram', show.h >= 44 && await until('s.diagram.shown', 'auto show')
    && await until(`__xpp.diagram() && __xpp.diagram().curves.length === ${nCurves}`, 'auto chart again'));
  await desktopMetrics();
  await sleep(200);

  /* a page that connects while AUTO is open gets the view and the whole diagram again */
  await cdp.send('Page.reload');
  check('a reloaded page opens the AUTO view and asks for the diagram again',
    await until(`s.diagram.open && s.diagram.axes && !s.busy && s.diagram.points.x.length === ${want.pts.length}`, 'reload auto', 30000)
    && JSON.stringify(await DS('d.labels')) === JSON.stringify(labels), JSON.stringify(await DS('[d.open, d.points.x.length]')));

  /* T22: Numerics edited during a run wait (pending) and apply when it ends; the run after uses them */
  const firstPeriodic = await DS('d.points.br.filter(b => b === 2).length');
  const periodicFromHopf = async what => {
    await cdp.eval(`document.querySelector('.auto-host').focus()`);
    await key('g');
    await until("s.ask && s.ask.kind === 'grab' && s.diagram.info", `grab ${what}`);
    for (let i = 0; i < 20 && !(await DS("d.info && d.info.sym === 'HB'")); i++) {
      const n = await DS('d.infoEvents');
      await key('Tab');
      await until(`s.diagram.infoEvents > ${n} && s.ask`, `grab tab ${what}`);
    }
    await key('Enter');
    await until('!s.busy && !s.diagram.grabbing', `grabbed ${what}`);
    await cdp.eval(`document.querySelector('.auto-host').focus()`);
    await key('r');
    await until("s.ask && s.ask.kind === 'menu' && s.ask.title === 'Hopf Pt'", `hopf menu ${what}`);
    await menuKey('p');
  };
  const nPre = await DS('d.points.x.length'), sentPre = await cdp.eval('__xpp.sent().length');
  await periodicFromHopf('a long run');
  const long = await until(`s.busy && !s.ask && s.diagram.points.x.length > ${nPre}`, 'long run going', 20000);
  await autoButton('N');
  await until(`!!document.querySelector('.auto-settings-dialog[data-settings=auto-numerics]')`, 'numerics during run');
  await cdp.eval(`(() => { const i = document.querySelector('.auto-settings-dialog input[data-field=nmx]');
    i.value = '15'; i.dispatchEvent(new Event('input', {bubbles: true})); })()`);
  await cdp.eval(`document.querySelector('.auto-settings-dialog .dialog-actions .primary').click()`);
  const setsSent = () => cdp.eval(`__xpp.sent().slice(${sentPre}).filter(c => c.cmd === 'auto' && c.op === 'set')`);
  check('T22: Numerics during a run: Nmax 15 waits, pending (the button dashed, the store\'s queue), nothing sent',
    long && await S(`s.busy && s.autoSettings.queued && s.autoSettings.queued.numerics.nmx === 15
      && s.autoSettings.core.numerics.nmx === 321`)
    && await cdp.eval(`document.querySelector('.auto-tools button[data-op=numerics]').classList.contains('auto-pending')`)
    && (await setsSent()).length === 0, JSON.stringify([long, await S('[s.busy, s.autoSettings]'), await setsSent()]));
  /* T25: a connection the browser opened and sent nothing on yet (a
     preconnect) held xppautX's one request thread for 30 s, and the Stop
     with it, while the run went on */
  const [host, port] = (await cdp.eval('location.host')).split(':');
  const idle = net.connect(Number(port), host);
  await new Promise(r => idle.once('connect', r));
  await sleep(200);
  const labsPre = await DS('d.labels.length'), tStop = Date.now();
  await cdp.eval(`document.querySelector('.auto-status .auto-stop').click()`);
  const stopped = await until('!s.busy', 'stopped', 5000), tookStop = Date.now() - tStop;
  const lastLab = await DS('d.labels.length > 0 && d.labels[d.labels.length - 1].sym');
  idle.destroy();
  check('T25: Stop ends the run within 1 s, with an idle connection open to xppautX, on an EP label',
    stopped && tookStop < 1000 && (await DS('d.labels.length')) > labsPre && lastLab === 'EP',
    JSON.stringify([stopped, tookStop, labsPre, lastLab]));
  check('T22: at the run\'s idle the edit goes out, one set, and applies: the core\'s Nmax is 15, nothing pending',
    await until(`!s.busy && s.autoSettings.core.numerics.nmx === 15 && !s.autoSettings.queued && !s.autoSettings.sent`,
      'applied at idle', 60000)
    && JSON.stringify(await setsSent()) === JSON.stringify([{cmd: 'auto', op: 'set', numerics: {nmx: 15}}])
    && !(await cdp.eval(`document.querySelector('.auto-tools button[data-op=numerics]').classList.contains('auto-pending')`)),
    JSON.stringify([await S('s.autoSettings'), await setsSent()]));
  check('T23: after Stop the status strip says so: "Stopped: by the user (Stop)"',
    await until(`/^Stopped: by the user \\(Stop\\) · periodic orbits/.test(document.querySelector('[data-testid=auto-status]').textContent)`,
      'strip stopped by user') && (await DS('d.stop && d.stop.why')) === 'user', JSON.stringify([await DS('d.stop'), await autoStatus()]));
  const nMid = await DS('d.points.x.length');
  await periodicFromHopf('the run after');
  const ran = await until(`!s.busy && s.diagram.points.x.length > ${nMid}`, 'the run after', 60000);
  const nNew = (await DS('d.points.x.length')) - nMid;
  check(`T22: the run after uses it: its periodic branch stops at Nmax, 15 points (the first had ${firstPeriodic})`,
    ran && nNew === 15 && firstPeriodic > 15, JSON.stringify([ran, nNew, firstPeriodic]));

  /* Close: done with AUTO */
  await cdp.eval(`document.querySelector('.auto-close').click()`);
  check("Close closes AUTO's window and the view", await until('!s.diagram.open && !s.busy', 'auto close')
    && !(await cdp.eval(`!!document.querySelector('.auto-panel, .auto-show')`))
    && (await DS('d.points.x.length')) === 0, JSON.stringify(await DS('[d.open, d.shown]')));
  check('the focus goes back to the plot', await until(`document.activeElement.closest('.plot-host')`, 'plot focus'));
  await key('i');
  check('T21: after Close, I goes to the main window: the Initialconds menu, not an AUTO command',
    await until(`s.ask && s.ask.kind === 'menu' && s.ask.name !== 'auto'`, 'main menu after close')
    && (await cdp.eval(`__xpp.sent().filter(c => c.cmd !== 'answer').pop().key`)) === 'i', JSON.stringify(await S('s.ask')));
  await key('Escape');
  await until('!s.busy && !s.ask', 'menu closed after close');
}

/* T21: while the core computes (an integration here), the AUTO view's own
   actions stay live and the buttons that need the core say why they wait */
async function busyAuto() {
  await desktopMetrics();
  check('busy: the page connects', await until('s.hello && !s.busy', 'hello'));
  /* nUmerics / Total 3000: an integration of about three seconds */
  await focusPlot();
  await key('u');
  await until('!s.busy && s.core.menu === 2', 'numerics menu');
  await key('t');
  await until("s.ask && s.ask.kind === 'string'", 'total');
  await answerAsk({ok: 1, value: '3000'});
  await until('!s.busy', 'total set');
  await key('Escape');
  await until('!s.busy && s.core.menu === 0', 'main menu');
  await key('f');
  await until('!s.busy && s.core.menu === 1', 'file menu');
  await key('a');
  check('busy: File/Auto opens the AUTO view', await until('s.diagram.open && s.diagram.axes && !s.busy', 'auto open'));
  await focusPlot();
  await key('i');
  await key('g');
  const busy = await until('s.busy && !s.ask && s.core && (s.progress || w.series)', 'integration running', 5000);
  const run = await cdp.eval(`(() => { const b = document.querySelector('.auto-tools button[aria-keyshortcuts=R]');
    return {disabled: b.disabled, title: b.title}; })()`);
  const menu = await cdp.eval(`(() => { const b = document.querySelector('.menu-panel .menu-item'); return b && [b.disabled, b.title]; })()`);
  const a = await autoArea();
  await mouse('mouseMoved', a.x + a.w / 2, a.y + a.h / 2);
  await mouse('mouseWheel', a.x + a.w / 2, a.y + a.h / 2, {deltaX: 0, deltaY: -120});
  check('busy: during an integration the AUTO diagram zooms, and Run is disabled with "Busy: available when the current run ends"',
    busy && await until('s.busy && s.diagram.viewport.x', 'zoom while busy', 3000) && run.disabled
    && run.title === 'Busy: available when the current run ends', JSON.stringify([busy, run, await DS('d.viewport')]));
  check('busy: T22: Parameter, Numerics and Mark values stay enabled (their changes wait)',
    await cdp.eval(`['P', 'N', 'U'].every(k => !document.querySelector('.auto-tools button[aria-keyshortcuts=' + k + ']').disabled)`));
  check("busy: ... and the main window's menu waits the same way", !!menu && menu[0] === true
    && menu[1] === 'Busy: available when the current run ends', JSON.stringify(menu));
  await until('!s.busy', 'integration done', 30000);
}

/* ---- T21: where keys go, the theme switch, long menus in columns ---------------- */

async function keysCheck() {
  await desktopMetrics();
  check('keys: the page connects', await until('s.hello && !s.busy', 'hello'));
  const theme = await cdp.eval(`(() => { const b = document.querySelector('.theme-toggle');
    return b && {label: b.getAttribute('aria-label'), text: b.textContent.trim(), icon: !!b.querySelector('svg')}; })()`);
  check('keys: the theme switch is an icon named "Theme: follow system", no word "Auto" beside the feature buttons',
    theme && theme.label === 'Theme: follow system' && theme.icon && !/auto/i.test(theme.text), JSON.stringify(theme));

  /* I then G typed at once on the plot: the G waits for the menu I opens, and integrates */
  await focusPlot();
  const n0 = await S('s.seriesCount');
  await key('i');
  await key('g');
  check('keys: I then G typed at once on the plot integrate',
    await until(`s.seriesCount > ${n0} && w.series.rows === 601 && !s.busy`, 'typed integrate', 20000)
    && await cdp.eval(`!!document.activeElement.closest('.plot-host')`), JSON.stringify(await S('[s.seriesCount, s.busy, s.ask]')));
  await key('f');
  check('keys: F on the plot opens the File menu and the focus stays on the plot',
    await until('s.core.menu === 1 && !s.busy', 'file menu') && await cdp.eval(`!!document.activeElement.closest('.plot-host')`),
    await cdp.eval(`document.activeElement.className`));
  await key('Escape');
  await until('s.core.menu === 0 && !s.busy', 'main menu');
  /* a button that kept the focus after a click: letters typed on it are XPP's, Enter stays the button's */
  await cdp.eval(`document.querySelector('.title-bar button.primary').focus()`);
  await key('f');
  check('keys: F typed while a button has the focus acts too (File menu), the focus stays',
    await until('s.core.menu === 1 && !s.busy', 'file menu from button')
    && await cdp.eval(`document.activeElement.matches('.title-bar button.primary')`), await cdp.eval(`document.activeElement.outerHTML.slice(0, 80)`));
  await key('Escape');
  await until('s.core.menu === 0 && !s.busy', 'main menu 2');
  /* a letter typed into a field stays there */
  const sent0 = await cdp.eval('__xpp.sent().length');
  await cdp.eval(`(() => { const i = document.querySelector('.messages-search input'); i.closest('details').open = true; i.focus(); })()`);
  await key('g');
  check('keys: a letter typed into a text field stays in it',
    await cdp.eval(`document.activeElement.value === 'g'`) && (await cdp.eval('__xpp.sent().length')) === sent0);
  await cdp.eval(`(() => { const i = document.activeElement; i.value = ''; i.dispatchEvent(new Event('input', {bubbles: true})); i.blur(); })()`);

  /* a long menu fits a laptop's screen in columns; one column on a phone */
  await focusPlot();
  await key('i');
  await until(`s.ask && s.ask.kind === 'menu' && document.querySelector('[role=dialog] .menu-list')`, 'ic menu');
  const cols = () => cdp.eval(`(() => { const d = document.querySelector('.dialog'), items = [...d.querySelectorAll('.menu-item')];
    return {n: items.length, cols: new Set(items.map(b => Math.round(b.getBoundingClientRect().left))).size,
      scroll: d.scrollHeight - d.clientHeight, sideways: document.documentElement.scrollWidth - innerWidth}; })()`);
  const wide = await cols();
  check('keys: a long menu (Initialconds) is in columns and fits without scrolling on 1280x860',
    wide.n > 8 && wide.cols >= 2 && wide.scroll <= 1, JSON.stringify(wide));
  await key('Escape');
  await until('!s.busy && !s.ask', 'menu closed');
  await cdp.send('Emulation.setDeviceMetricsOverride', {width: 390, height: 844, deviceScaleFactor: 2, mobile: true});
  await sleep(300);
  await focusPlot();
  await key('i');
  await until(`s.ask && s.ask.kind === 'menu' && document.querySelector('[role=dialog] .menu-list')`, 'ic menu phone');
  const narrow = await cols();
  check('keys: on a 390 px phone it is one column, no sideways scroll', narrow.cols === 1 && narrow.sideways <= 0,
    JSON.stringify(narrow));
  await key('Escape');
  await until('!s.busy && !s.ask', 'menu closed phone');
  await desktopMetrics();
}

/* ---- live plotting and long runs ------------------------------------------------ */

async function desktopMetrics() {
  await cdp.send('Emulation.setTouchEmulationEnabled', {enabled: false});
  await cdp.send('Emulation.setEmulatedMedia', {features: []}).catch(() => {});
  await cdp.send('Emulation.setDeviceMetricsOverride', {width: 1280, height: 860, deviceScaleFactor: 1, mobile: false});
}

/* what the store and the chart hold, sampled by the page at every frame
   while an integration runs, and the samples it took */
const startSampling = () => cdp.eval(`(() => { const seen = window.__seen = []; window.__sampling = true;
  const tick = () => { const s = __xpp.state(), w = ${ACTIVE}, p = __xpp.plot();
    const r = [w.series ? w.series.rows : -1, p && p.curves[0] ? p.curves[0].points : -1, s.busy];
    const l = seen[seen.length - 1];
    if (!l || l[0] !== r[0] || l[1] !== r[1] || l[2] !== r[2]) seen.push(r);
    if (window.__sampling) requestAnimationFrame(tick); };
  requestAnimationFrame(tick); return true; })()`);
const stopSampling = () => cdp.eval('(() => { window.__sampling = false; return window.__seen; })()');

/** integrate from the keyboard (I, G); true when the run ended with `rows` rows */
async function integrate(rows, ms) {
  const n0 = await S('s.seriesCount');
  await key('i');
  if (!(await until("s.ask && s.ask.kind === 'menu'", 'menu'))) return false;
  await key('g');
  return until(`s.seriesCount > ${n0} && w.series.rows === ${rows} && !s.busy`, `${rows} rows`, ms);
}

/* ---- runs, Erase, sliders and the values panel (GitHub #18) --------------------------- */

/** Add slider, then pick `name`: the new slider's id */
/** Add slider (T20): open the dialog, search for `name`, pick the first
    match, OK. Returns the new slider's id, with the T19-rule default range
    and step (defaultRange/defaultStep) unless `edit` overrides them
    (Min/Max/Step, as typed) before OK. */
async function addSlider(name, edit = null) {
  const before = await S('s.values.sliders.length');
  await cdp.eval(`document.querySelector('.slider-add').click()`);
  await until(`document.querySelector('.slider-picker-search input')`, 'slider dialog open');
  await cdp.eval(`(() => { const el = document.querySelector('.slider-picker-search input');
    el.value = ${JSON.stringify(name)}; el.dispatchEvent(new Event('input', {bubbles: true})); })()`);
  await until(`document.querySelector('.slider-picker-item')`, 'slider candidates filtered');
  await cdp.eval(`document.querySelector('.slider-picker-item').click()`);
  await until(`!![...document.querySelectorAll('.dialog-actions button')].find(b => b.textContent === 'OK')`, 'slider fields shown');
  if (edit) await setSliderDialogFields(edit);
  await until(
    `!([...document.querySelectorAll('.dialog-actions button')].find(b => b.textContent === 'OK')?.disabled)`, 'OK enabled');
  await cdp.eval(`[...document.querySelectorAll('.dialog-actions button')].find(b => b.textContent === 'OK').click()`);
  await until(`s.values.sliders.length > ${before}`, 'slider added');
  return S('s.values.sliders[s.values.sliders.length - 1].id');
}

/** fills the open slider dialog's Min/Max/Step fields (whichever keys are given) */
async function setSliderDialogFields(fields) {
  for (const [label, text] of Object.entries(fields)) {
    await cdp.eval(`(() => { const l = [...document.querySelectorAll('.slider-dialog-fields label')]
      .find(l => l.querySelector('span').textContent === ${JSON.stringify(label)});
      const el = l.querySelector('input'); el.value = ${JSON.stringify(text)};
      el.dispatchEvent(new Event('input', {bubbles: true})); })()`);
  }
  await sleep(50);
}

/** the input of field `name` in the values panel's section `sec` (par or ic) */
const fieldOf = (sec, name) => `[...document.querySelectorAll('[data-section="${sec}"] .value-field')]
  .find(f => f.querySelector('.value-name').textContent.toLowerCase() === ${JSON.stringify(name.toLowerCase())})`;
/** type `text` into a field and leave it (two round trips: Preact renders the draft before the blur reads it) */
async function editField(sec, name, text) {
  await cdp.eval(`(() => { const el = ${fieldOf(sec, name)}.querySelector('input'); el.focus();
    el.value = ${JSON.stringify(text)}; el.dispatchEvent(new Event('input', {bubbles: true})); })()`);
  await sleep(60);
  await cdp.eval(`${fieldOf(sec, name)}.querySelector('input').blur()`);
  await sleep(20);
}
const icsOf = 'JSON.stringify(s.core.ics.map(p => p[1]))';
const icFields = () => cdp.eval(`JSON.stringify([...document.querySelectorAll('[data-section="ic"] .value-field input')].map(i => i.value))`);
const nowCells = () => cdp.eval(`JSON.stringify([...document.querySelectorAll('.value-now')].map(o => o.textContent))`);
const close6 = (a, b) => Math.abs(a - b) <= 1e-6 * Math.max(1, Math.abs(b));

/* lecar.ode: runs accumulate, Erase, Redraw, Last and "Use current state", reset, save and load, sliders */
async function runsCheck(dir) {
  await desktopMetrics();
  check('runs: the page connects', await until('s.hello && s.seriesCount >= 1 && !s.busy && s.values.defaults', 'hello'));
  check('runs: before any run Now shows nothing', (await nowCells()) === JSON.stringify(['–', '–']), await nowCells());
  await focusPlot();
  check('runs: I, G integrates', await integrate(601, 30000));
  check('runs: one run, nothing earlier', await S('w.history.runs.length === 0 && __xpp.plot().runs.count === 0'));
  const end = await S('w.series.columns.get(1)[600]');
  check('runs: after the run Now is its last row', await until(`s.core.now && Math.abs(s.core.now[0] - ${end}) < 1e-6`, 'now')
    && (await nowCells()).includes(String(Number(end.toPrecision(6)))), await nowCells());

  /* Initialconds/Last: Now -> Initial, then a run; the first run stays under the new one */
  const now0 = await S('s.core.now.slice()');
  const n0 = await S('s.seriesCount');
  await focusPlot();
  await menuKeys('i', 'l');
  check('runs: Initialconds/Last runs again', await until(`s.seriesCount > ${n0} && !s.busy && w.series.rows === 601`, 'last'));
  check('runs: after Last the IC fields are where the previous run ended',
    (await S(`s.core.ics.every((p, i) => p[1] === ${JSON.stringify(now0)}[i])`))
    && JSON.parse(await icFields()).every((t, i) => close6(Number(t), now0[i])), `${await icFields()} vs ${JSON.stringify(now0)}`);
  check('runs: two runs keep two curves: the earlier one drawn under the current',
    await until('w.history.runs.length === 1 && __xpp.plot().runs.count === 1 && __xpp.plot().runs.drawn > 0 && __xpp.plot().curves[0].points === 601', 'two runs'),
    JSON.stringify(await cdp.eval('__xpp.plot().runs')));
  const legend = `[...document.querySelectorAll('.legend-item')].find(b => b.dataset.layer === 'runs')`;
  check('runs: the legend lists "previous runs (1)"', await cdp.eval(`(${legend} || {}).textContent === 'previous runs (1)'`));
  await cdp.eval(`${legend}.click()`);
  check('runs: the legend entry hides them', await until('!w.showRuns && __xpp.plot().runs.shown === false && __xpp.plot().runs.drawn === 0', 'hide runs'));
  await cdp.eval(`${legend}.click()`);
  await until('w.showRuns', 'show runs');
  await focusPlot();
  check('runs: a third run (Go) keeps both earlier ones', (await integrate(601, 30000)) && await until('w.history.runs.length === 2', '3 runs'));

  /* Erase blanks the picture, Redraw draws the current data again */
  await focusPlot();
  await key('e');
  check('runs: Erase clears the picture and the shown runs',
    await until('!s.busy && w.history.erased && w.history.runs.length === 0 && __xpp.plot().curves[0].points === 0 && __xpp.plot().runs.count === 0', 'erase'),
    JSON.stringify(await S('w.history')));
  await focusPlot();
  await key('r');
  check('runs: Redraw shows the current data again',
    await until('!s.busy && !w.history.erased && __xpp.plot().curves[0].points === 601', 'redraw'), JSON.stringify(await S('w.history')));

  /* "Use current state": the ICs become Now, no run */
  await cdp.eval(`[...document.querySelectorAll('[data-section="ic"] .value-tools button')].find(b => b.textContent.includes('Use current state')).click()`);
  const n1 = await S('s.seriesCount');
  check('runs: "Use current state" makes the ICs equal Now, without a run',
    await until(`!s.busy && s.core.ics.every((p, i) => p[1] === s.core.now[i])`, 'use state') && (await S('s.seriesCount')) === n1,
    JSON.stringify(await S('[s.core.ics, s.core.now]')));

  /* a parameter's reset: its title names the model value, a changed field is marked */
  await editField('par', 'phi', '0.5');
  await until('!s.busy && Math.abs(s.core.pars.find(p => p[0] === "phi")[1] - 0.5) < 1e-12', 'phi');
  const reset = `${fieldOf('par', 'phi')}.querySelector('.value-reset')`;
  check('runs: a changed parameter is marked, its reset names the default',
    await cdp.eval(`${fieldOf('par', 'phi')}.classList.contains('changed') && ${reset}.title === 'default: 0.333'`),
    await cdp.eval(`${reset}.title`));
  await cdp.eval(`${reset}.click()`);
  check('runs: reset restores the model value', await until('!s.busy && s.core.pars.find(p => p[0] === "phi")[1] === 0.333', 'reset'));

  /* Save, then Load a changed copy: one set, at most one run */
  await cdp.eval(`[...document.querySelectorAll('[data-section="par"] .value-tools button')].find(b => b.textContent === 'Save').click()`);
  const saved = await S('s.values.lastSaved && s.values.lastSaved.text');
  check('runs: Save writes XPP\'s parameter file', /^\d+   Number params\n/.test(saved || '') && /\n0\.05  iapp\n/.test(saved || ''),
    String(saved).slice(0, 80));
  const parFile = path.join(dir, 'changed.par');
  fs.writeFileSync(parFile, saved.replace('\n0.05  iapp\n', '\n0.075  iapp\n'));
  const sent0 = (await cdp.eval('__xpp.sent().length'));
  await pickFiles('#values-load-par', [parFile]);
  check('runs: Load applies the file: one set, one run',
    await until('!s.busy && s.core.pars.find(p => p[0] === "iapp")[1] === 0.075', 'load')
    && (await cdp.eval(`__xpp.sent().slice(${sent0}).filter(c => c.cmd === 'set').length`)) === 1,
    JSON.stringify(await cdp.eval(`__xpp.sent().slice(${sent0})`)));
  await cdp.eval(`[...document.querySelectorAll('[data-section="ic"] .value-tools button')].find(b => b.textContent === 'Save').click()`);
  const icText = await S('s.values.lastSaved.text');
  const icFile = path.join(dir, 'changed.ic');
  fs.writeFileSync(icFile, '-0.25\n0.1\n');
  await pickFiles('#values-load-ic', [icFile]);
  check('runs: IC Save is the values alone; Load sets them',
    icText.trim().split('\n').length === 2 && await until('!s.busy && s.core.ics[0][1] === -0.25 && s.core.ics[1][1] === 0.1', 'ic load'),
    icText);

  /* a slider: its default range is [0, 2v]; a drag sends while idle, and the release leaves its value */
  const iapp = await S('s.core.pars.find(p => p[0] === "iapp")[1]');
  const sid = await addSlider('iapp');
  const sdef = await S(`s.values.sliders.find(d => d.id === ${sid})`);
  check('runs: a picked slider ranges over [0, 2v], with a positive step',
    sdef && sdef.name === 'iapp' && sdef.lo === '0' && sdef.hi === String(2 * iapp) && Number(sdef.step) > 0,
    JSON.stringify(sdef));
  const track = await cdp.eval(`(() => { const r = document.getElementById('slider-range-${sid}').getBoundingClientRect();
    return {x: r.left, y: r.top + r.height / 2, w: r.width}; })()`);
  const sent1 = await cdp.eval('__xpp.sent().length');
  await mouse('mousePressed', track.x + track.w * 0.5, track.y, {button: 'left', clickCount: 1});
  for (let k = 1; k <= 10; k++) await mouse('mouseMoved', track.x + track.w * (0.5 + k * 0.04), track.y, {button: 'left'});
  await mouse('mouseReleased', track.x + track.w * 0.9, track.y, {button: 'left', clickCount: 1});
  await until('!s.busy && !s.values.queue.length', 'drag settles', 20000);
  await sleep(300);
  const out = await cdp.eval(`__xpp.sent().slice(${sent1}).filter(c => c.cmd === 'slide' || c.cmd === 'set')`);
  const final = await S('s.core.pars.find(p => p[0] === "iapp")[1]');
  const vals = out.map(c => c.value ?? Number(c.text));
  check(`runs: a slider drag sends as it goes (${out.length} runs for 11 positions), ends at its final value, never twice`,
    out.length >= 1 && out.length <= 11 && Math.abs(final - 0.9 * 2 * iapp) < 2 * iapp * 0.05 && !(await S('s.busy'))
    && vals.every((v, i) => i === 0 || v !== vals[i - 1]) && vals[vals.length - 1] === final,
    JSON.stringify({out, final}));

  /* folding a section is remembered by the viewer */
  await cdp.eval(`document.querySelector('[data-section="par"] .value-fold').click()`);
  check('runs: a section folds, and the page remembers it',
    await until(`document.getElementById('values-sec-par').hidden && JSON.parse(localStorage.getItem('xpp.values.folded')).includes('par')`, 'fold'));
  await cdp.eval(`document.querySelector('[data-section="par"] .value-fold').click()`);
}

/* Help (docs/roadmap.md W12b): the manual (dist/manual.json, ~270 KB) is
   fetched lazily, not bundled in app.js: this checks that fetch too, not
   just the view built from what it returns. A "?" link on a dialog opens
   Help at that section, with its heading in view once loaded; the search
   box finds a known term; a search result and a table-of-contents link
   both navigate; F1 opens it from anywhere that is not a text field. */
async function helpCheck() {
  await desktopMetrics();
  check('help: the page connects', await until('s.hello && !s.busy', 'hello'));
  check('help: starts closed', !(await S('s.help.open')));
  check('help: the manual is not fetched before Help ever opens (no chapter rendered yet)',
    !(await cdp.eval(`!!document.querySelector('.help-content')`)));

  /* manual.json itself: served like any other web2/dist file (Makefile
     WEB2_FILES, tools/embed.c), no token needed, same as app.js/app.css */
  const manualFetch = await cdp.eval(`fetch('manual.json').then(r => ({status: r.status, type: r.headers.get('content-type')}))`);
  check('help: manual.json is served with a JSON content type',
    manualFetch.status === 200 && /application\/json/.test(manualFetch.type || ''), JSON.stringify(manualFetch));

  /* a "?" link on a dialog: the values panel's Parameters section has one (always inline at 1280px) */
  check('help: a "?" link is on the values panel',
    await cdp.eval(`!!document.querySelector('.value-group-head .help-link')`));
  await cdp.eval(`document.querySelector('.value-group-head .help-link').click()`);
  check('help: it opens Help at the values panel section at once (the fetch is still pending or already done)', await until(
    `s.help.open && s.help.chapter === '04-using-the-interface' && s.help.anchor === 'the-values-panel'`, 'help open'),
    JSON.stringify(await S('s.help')));
  check('help: the panel becomes visible',
    await cdp.eval(`getComputedStyle(document.querySelector('.help-panel')).visibility === 'visible'`));
  check('help: it shows "Loading the manual…" or the chapter (a fast local fetch may beat this check)',
    await cdp.eval(`!!document.querySelector('.help-loading') || !!document.querySelector('.help-content')`));
  check('help: the section heading is scrolled into view once loaded', await until(`(() => {
    const h = document.querySelector('.help-content #the-values-panel'), c = document.querySelector('.help-content');
    if (!h || !c) return false;
    const hr = h.getBoundingClientRect(), cr = c.getBoundingClientRect();
    return hr.top >= cr.top - 5 && hr.top <= cr.bottom;
  })()`, 'heading in view'));

  /* the search box finds a known term (across chapters, not just the one shown) */
  await cdp.eval(`(() => { const el = document.querySelector('.help-search input'); el.focus();
    el.value = 'Poincare'; el.dispatchEvent(new Event('input', {bubbles: true})); })()`);
  check('help: search finds results for a known term',
    await until(`document.querySelectorAll('.help-result').length > 0`, 'search results'),
    JSON.stringify(await S('s.help.query')));

  /* picking a result opens its chapter (its anchor too, unless the match was
     above the chapter's first heading, which counts as the chapter's top) */
  const resultTarget = await S(`(() => {
    const r = document.querySelector('.help-result');
    return r ? r.querySelector('.help-result-heading').textContent : null; })()`);
  await cdp.eval(`document.querySelector('.help-result').click()`);
  check('help: picking a search result opens its chapter', await until('s.help.open && s.help.chapter', 'result nav'),
    JSON.stringify({want: resultTarget, got: await S('s.help')}));

  /* a table-of-contents (chapter) link navigates too, to a chapter with its
     own cross-reference links (04-using-the-interface.md links to others) */
  await cdp.eval(`(() => { const el = document.querySelector('.help-search input'); el.focus();
    el.value = ''; el.dispatchEvent(new Event('input', {bubbles: true})); })()`);
  await cdp.eval(`[...document.querySelectorAll('.help-toc-item')].find(b => b.textContent.includes('Using the interface')).click()`);
  check('help: a chapter link (the table of contents) navigates',
    await until(`s.help.chapter === '04-using-the-interface' && s.help.anchor === null`, 'toc nav'),
    JSON.stringify(await S('s.help')));

  /* a cross-reference inside the chapter's own text navigates too */
  const href = await cdp.eval(`(() => { const a = [...document.querySelectorAll('.help-content a[href]')]
    .find(a => /^\\d\\d-[a-z0-9-]+\\.md/.test(a.getAttribute('href'))); return a ? a.getAttribute('href') : null; })()`);
  check('help: the chapter has at least one cross-reference link to another chapter', !!href, String(href));
  if (href) {
    await cdp.eval(`[...document.querySelectorAll('.help-content a[href]')]
      .find(a => a.getAttribute('href') === ${JSON.stringify(href)}).click()`);
    const wantChapter = href.split('.md')[0].split('#')[0];
    check('help: clicking a manual cross-reference navigates to it, without a page reload',
      await until(`s.help.chapter === ${JSON.stringify(wantChapter)}`, 'link nav'), JSON.stringify(await S('s.help')));
  }

  /* Back closes it; F1 (focus away from any field) reopens it where it was left */
  await cdp.eval(`document.querySelector('.help-back').click()`);
  check('help: Back closes it', await until('!s.help.open', 'closed'));
  const chapterLeftAt = await S('s.help.chapter');
  await cdp.eval(`document.body.focus()`);
  /* headless Chrome's synthetic F1 occasionally does not land the first
     time (a browser-reserved key in many builds): press it again, a few
     times, before giving up */
  let f1ok = false;
  for (let i = 0; i < 5 && !f1ok; i++) {
    await key('F1');
    f1ok = await until(`s.help.open && s.help.chapter === ${JSON.stringify(chapterLeftAt)}`, 'F1 open', 1000);
  }
  check('help: F1 opens it again, where it was left', f1ok, JSON.stringify(await S('s.help')));
  await cdp.eval(`document.querySelector('.help-back').click()`);
  await until('!s.help.open', 'closed again');

  /* W13a: the desktop window's Help > Keyboard shortcuts and Help > Manual
     call this hook (core/xpp_window.cpp) */
  await cdp.eval(`window.__xppOpenHelp('05-commands')`);
  check('help: the desktop hook opens Help at a chapter (Help > Keyboard shortcuts)',
    await until(`s.help.open && s.help.chapter === '05-commands' && s.help.anchor === null`, 'hook open'),
    JSON.stringify(await S('s.help')));
  await cdp.eval(`document.querySelector('.help-back').click()`);
  await until('!s.help.open', 'closed by Back');
  await cdp.eval(`window.__xppOpenHelp()`);
  check('help: the desktop hook with no chapter reopens it where it was left (Help > Manual)',
    await until(`s.help.open && s.help.chapter === '05-commands'`, 'hook reopen'), JSON.stringify(await S('s.help')));
  await cdp.eval(`document.querySelector('.help-back').click()`);
  await until('!s.help.open', 'closed at the end');

  /* File/Help (M_FH, key h): the core itself sends the `help` event (docs/protocol.md) */
  await focusPlot();
  await key('f');
  await until('s.core.menu === 1 && !s.busy', 'file menu for help');
  await key('h');
  check('help: File/Help opens Help at the File menu chapter (core-sent `help` event)',
    await until(`s.help.open && s.help.chapter === '05-commands' && s.help.anchor === 'file'`, 'file help open'),
    JSON.stringify(await S('s.help')));
  await cdp.eval(`document.querySelector('.help-back').click()`);
  await until('!s.help.open', 'closed after File/Help');
  await key('Escape');
  await until('s.core.menu === 0 && !s.busy', 'main menu after help');
}

/* tools/models/live.ode (about two seconds a run): edits while busy wait and go
   out once; the IC fields do not move during a run, Now does */
async function valuesLive() {
  await desktopMetrics();
  /* the first run ends the wait: slow on a loaded machine, so the section's
     long timeout, and nothing below means anything without it */
  const connected = await until('s.hello && s.seriesCount >= 1 && !s.busy', 'hello', 60000);
  check('values: the page connects', connected);
  if (!connected) return;
  const ics0 = await S(icsOf), fields0 = await icFields();
  await cdp.eval(`window.__icSeen = []; window.__nowSeen = []; window.__sampling = true;
    (function tick() { const s = __xpp.state();
      if (s.busy) { __icSeen.push(JSON.stringify(s.core.ics.map(p => p[1])) + [...document.querySelectorAll('[data-section="ic"] .value-field input')].map(i => i.value).join());
        __nowSeen.push([...document.querySelectorAll('.value-now')].map(o => o.textContent).join()); }
      if (window.__sampling) requestAnimationFrame(tick); })(); true`);
  await focusPlot();
  await key('i');
  await until("s.ask && s.ask.kind === 'menu'", 'menu');
  await key('g');
  await until('s.busy && w.series && w.series.rows > 100', 'running');
  /* five edits of a parameter while the run goes: they wait, marked */
  for (const v of ['0.051', '0.052', '0.053', '0.054', '0.055']) await editField('par', 'iapp', v);
  const queued = await S('JSON.stringify(s.values.queue)');
  const marked = await cdp.eval(`${fieldOf('par', 'iapp')}.classList.contains('queued')`);
  const wasBusy = await S('s.busy');
  const sent0 = await cdp.eval('__xpp.sent().length');
  await until('!s.busy && w.series.rows === 20001', 'first run', 60000);
  await until('!s.busy && s.core.pars.find(p => p[0] === "iapp")[1] === 0.055 && !s.values.queue.length', 'the queued run', 60000);
  await sleep(500);
  await cdp.eval('window.__sampling = false; true');
  const after = await cdp.eval(`__xpp.sent().slice(${sent0})`);
  check('values: five edits while busy wait (the latest, marked) and go out as one set with one run',
    wasBusy && queued === JSON.stringify([{kind: 'par', name: 'iapp', text: '0.055'}]) && marked
    && after.length === 1 && after[0].cmd === 'set' && after[0].rerun === 1 && after[0].text === '0.055'
    && (await S('s.values.queue.length')) === 0,
    JSON.stringify({wasBusy, queued, marked, after}));
  const icSeen = await cdp.eval('[...new Set(__icSeen)]');
  const nowSeen = await cdp.eval('[...new Set(__nowSeen)]');
  check('values: the IC fields and the ICs do not change during a run',
    icSeen.length === 1 && icSeen[0].startsWith(ics0) && (await S(icsOf)) === ics0 && (await icFields()) === fields0,
    JSON.stringify({icSeen: icSeen.slice(0, 3), ics0}));
  check(`values: Now changes during the run (${nowSeen.length} values seen)`, nowSeen.length >= 3, JSON.stringify(nowSeen.slice(0, 5)));
  const last = await S('[w.series.columns.get(1)[w.series.rows - 1], w.series.columns.get(2)[w.series.rows - 1]]');
  check('values: after the run Now is the last row of the series',
    await S(`s.core.now && s.core.now.every((v, i) => Math.abs(v - ${JSON.stringify(last)}[i]) < 1e-6)`),
    JSON.stringify([await S('s.core.now'), last]));

  /* a slider dragged while a run goes: no position goes out until it ends, then only the last */
  const sid = await addSlider('iapp');
  await focusPlot();
  await key('i');
  await until("s.ask && s.ask.kind === 'menu'", 'menu');
  await key('g');
  await until('s.busy && w.series && w.series.rows > 100', 'running');
  const track = await cdp.eval(`(() => { const r = document.getElementById('slider-range-${sid}').getBoundingClientRect();
    return {x: r.left, y: r.top + r.height / 2, w: r.width}; })()`);
  const sent1 = await cdp.eval('__xpp.sent().length');
  await mouse('mousePressed', track.x + track.w * 0.3, track.y, {button: 'left', clickCount: 1});
  for (let k = 1; k <= 10; k++) await mouse('mouseMoved', track.x + track.w * (0.3 + k * 0.05), track.y, {button: 'left'});
  await mouse('mouseReleased', track.x + track.w * 0.8, track.y, {button: 'left', clickCount: 1});
  const during = await cdp.eval(`__xpp.sent().slice(${sent1}).length`);
  const busyThen = await S('s.busy');
  const shown = await S(`s.values.queue.length === 1 && document.querySelector('[data-slider="${sid}"]').classList.contains('queued')`);
  await until('!s.busy && !s.values.queue.length && w.series.rows === 20001', 'the drag run', 60000);
  await until('!s.busy', 'idle', 60000);
  await sleep(300);
  const out = await cdp.eval(`__xpp.sent().slice(${sent1})`);
  check('values: a slider dragged during a run sends nothing until it ends, then its last position, one run',
    busyThen && during === 0 && shown && out.length === 1 && out[0].cmd === 'set' && out[0].rerun === 1
    && Math.abs(Number(out[0].text) - (await S('s.core.pars.find(p => p[0] === "iapp")[1]'))) < 1e-12,
    JSON.stringify({busyThen, during, shown, out}));

  /* T31: a parameter box takes a number or %formula only: letters are refused with a message and
     stay in the box, marked (no silent revert); nothing is sent, Enter included; Escape drops them */
  await until('!s.busy', 'idle', 60000);
  const iapp0 = await S('s.core.pars.find(p => p[0] === "iapp")[1]');
  const sent2 = await cdp.eval('__xpp.sent().length');
  await editField('par', 'iapp', 'abc');
  await cdp.eval(`${fieldOf('par', 'iapp')}.querySelector('input').focus()`);
  await key('Enter');
  await sleep(300);
  const box = await cdp.eval(`(() => { const f = ${fieldOf('par', 'iapp')}, i = f.querySelector('input'), m = f.querySelector('.field-error');
    return {invalid: i.getAttribute('aria-invalid'), value: i.value, message: m && m.textContent, focused: document.activeElement === i}; })()`);
  check('T31: letters in a parameter box are refused: marked, "A number, or %formula such as %2*pi", kept, nothing sent (Enter neither)',
    box.invalid === 'true' && box.value === 'abc' && box.message === 'A number, or %formula such as %2*pi' && box.focused
    && (await cdp.eval(`__xpp.sent().length`)) === sent2, JSON.stringify({box, sent: await cdp.eval(`__xpp.sent().slice(${sent2})`)}));
  await key('Escape');
  await sleep(100);
  const dropped = await cdp.eval(`(() => { const i = ${fieldOf('par', 'iapp')}.querySelector('input'); return {invalid: i.getAttribute('aria-invalid'), value: i.value}; })()`);
  check('T31: Escape drops the refused text: the box shows the value again, unmarked',
    dropped.invalid === null && close6(Number(dropped.value), iapp0), JSON.stringify({dropped, iapp0}));
  /* a %formula is taken and sent as typed; the core evaluates it */
  await editField('par', 'iapp', '%0.01*6');
  check('T31: a %formula in a parameter box is sent as typed and the core evaluates it',
    await until('!s.busy && Math.abs(s.core.pars.find(p => p[0] === "iapp")[1] - 0.06) < 1e-12', 'iapp = %0.01*6', 60000)
    && (await cdp.eval(`__xpp.sent().slice(${sent2})`)).some(c => c.cmd === 'set' && c.text === '%0.01*6'),
    JSON.stringify(await cdp.eval(`__xpp.sent().slice(${sent2})`)));
}

/* tools/models/live.ode: 20 001 rows in about two seconds */
async function live(want) {
  await desktopMetrics();
  check('live: the page connects', await until('s.hello && s.seriesCount >= 1 && !s.busy', 'hello'));
  const a0 = await S('s.seriesAppends');
  await startSampling();
  const done = await integrate(20001, 60000);
  const seen = await stopSampling();
  check('live: I, G integrates 20 001 rows', done, JSON.stringify(await S('w.series && w.series.rows')));
  const appends = (await S('s.seriesAppends')) - a0;
  const busy = seen.filter(([, , b]) => b);
  const rows = busy.map(([r]) => r).filter(r => r > 0 && r < 20001);
  const points = busy.map(([, p]) => p).filter(p => p > 0 && p < 20001);
  const grows = a => a.every((v, i) => i === 0 || v >= a[i - 1]) && new Set(a).size >= 3;
  check(`live: the store grows over several appends before the idle (${appends} appends)`,
    appends >= 3 && grows(rows), JSON.stringify({appends, rows: rows.slice(0, 20)}));
  check('live: and so does the plot', grows(points), JSON.stringify(points.slice(0, 20)));
  const cols = await cdp.eval(`(() => { const s = __xpp.state(), m = (${ACTIVE}).series; const o = {};
    for (const [k, v] of m.columns) o[k] = Array.from(v); return o; })()`);
  let bad = null;
  for (const [col, values] of Object.entries(cols)) {
    values.forEach((v, r) => {
      const d = Math.abs(v - want[r][+col]), tol = 1e-7 * Math.abs(want[r][+col]) + 1e-30;
      if (d > tol && !bad) bad = `column ${col} row ${r}: ${v} vs ${want[r][+col]}`;
    });
  }
  check('live: the final store holds the numbers of output.dat', !bad && want.length === 20001
    && Object.values(cols).every(v => v.length === 20001), bad || `${want.length} rows`);
}

const pct = (a, q) => a.length ? [...a].sort((x, y) => x - y)[Math.min(a.length - 1, Math.floor(q * a.length))] : NaN;
const ms = v => `${v.toFixed(1)} ms`;

/** wheel zooms in and out about the middle of the plot; the long tasks and draw times they cost */
async function zoomFrames() {
  const a = await area(), cx = a.x + a.w * 0.55, cy = a.y + a.h * 0.45;
  await sleep(300);
  const t0 = await cdp.eval('performance.now()'), d0 = (await P()).draws;
  const v0 = await S('w.viewport');
  for (const dy of [-120, -120, -120, 120, 120, 120, 120]) {
    await mouse('mouseWheel', cx, cy, {deltaX: 0, deltaY: dy});
    await sleep(60);
  }
  await sleep(200);
  await until('!__xpp.plot().tracing', 'tracing', 5000);
  const p = await P(), draws = p.draws - d0;
  return {
    zoomed: JSON.stringify(v0) !== JSON.stringify(await S('w.viewport')),
    traceMs: p.traceMs,
    vertices: p.vertices,
    long: await cdp.eval(`__xpp.longTasks(${t0})`),
    supported: await cdp.eval('__xpp.longTasksSupported()'),
    drawMs: p.drawMs.slice(-Math.min(draws, p.drawMs.length)),
    draws,
  };
}

/* tools/models/million.ode: a phase plane of 1 000 001 points, then the
   same run against time */
async function million() {
  await desktopMetrics();
  check('10^6: the page connects', await until('s.hello && s.seriesCount >= 1 && !s.busy', 'hello'));
  const t0 = Date.now(), p0 = await cdp.eval('performance.now()'), d0 = (await P()).draws;
  const a0 = await S('s.seriesAppends');
  const done = await integrate(1000001, 300000);
  const secs = (Date.now() - t0) / 1000;
  check(`10^6: I, G stores 1 000 001 rows in the store (${secs.toFixed(1)} s, ${(await S('s.seriesAppends')) - a0} appends)`,
    done, JSON.stringify(await S('w.series && [w.series.rows, s.busy]')));
  if (!done) return;
  await until('!__xpp.plot().tracing', 'tracing', 10000);
  await sleep(300);
  const p = await P(), frames = p.drawMs.slice(-Math.min(p.draws - d0, p.drawMs.length));
  const load = await cdp.eval(`__xpp.longTasks(${p0})`);
  /* the long tasks of the run are the arrival of the data (the final full
     series is 16 MB of base64 in one event): reported, not drawing */
  console.log(`  run: ${frames.length} draws, median ${ms(pct(frames, 0.5))}, max ${ms(Math.max(...frames))}; `
    + `long tasks (data arriving) ${JSON.stringify(load.map(t => Math.round(t.duration)))}; the final trace took `
    + `${ms(p.traceMs ?? 0)} in later tasks and keeps ${p.vertices[0]} of the 1 000 001 vertices`);
  check('10^6: the phase plane draws its 1 000 001 points as they come, every draw under 50 ms',
    p.mode === 2 && p.curves[0].points === 1000001 && frames.length > 0 && Math.max(...frames) < 50
    && p.vertices[0] > 100, JSON.stringify({mode: p.mode, points: p.curves[0].points, frames, vertices: p.vertices}));
  let z = await zoomFrames();
  console.log(`  phase plane zoom: ${z.draws} draws, median ${ms(pct(z.drawMs, 0.5))}, max ${ms(Math.max(...z.drawMs))}, `
    + `long tasks ${JSON.stringify(z.long.map(t => Math.round(t.duration)))}${z.supported ? '' : ' (not supported)'}; `
    + `the last view's trace took ${ms(z.traceMs ?? 0)} (${z.vertices[0]} vertices)`);
  check('10^6: a wheel zoom of the phase plane keeps every frame under 50 ms, and ends traced',
    z.zoomed && z.draws > 0 && Math.max(...z.drawMs) < 50 && z.long.length === 0 && z.vertices[0] > 0, JSON.stringify(z));

  /* x against time: uPlot's own line with its min and max per pixel column */
  await cdp.eval(`document.querySelector('.plot-view:not([hidden]) .plot-host').focus()`);
  const n0 = await S('s.seriesCount');
  await key('x');
  if (!(await until("s.ask && s.ask.kind === 'string'", 'Xi vs t'))) return check('10^6: X asks what to plot', false);
  await cdp.eval(`__xpp.send({cmd: 'answer', id: __xpp.state().ask.id, value: 'x'})`);
  check('10^6: X plots x against T', await until(`s.seriesCount > ${n0} && !s.busy && w.series.curves[0].x === 0`, 'x vs t', 60000));
  await until('!__xpp.plot().tracing', 'tracing', 10000);
  await sleep(300);
  const q = await P();
  const render2 = q.drawMs[q.drawMs.length - 1];
  check(`10^6: the time plot draws in ${ms(render2)}`, q.mode === 1 && q.curves[0].points === 1000001 && render2 < 50,
    JSON.stringify({mode: q.mode, points: q.curves[0].points, render2}));
  z = await zoomFrames();
  console.log(`  time plot zoom: ${z.draws} draws, median ${ms(pct(z.drawMs, 0.5))}, max ${ms(Math.max(...z.drawMs))}, `
    + `long tasks ${JSON.stringify(z.long.map(t => Math.round(t.duration)))}`);
  check('10^6: a wheel zoom of the time plot keeps every frame under 50 ms',
    z.zoomed && z.draws > 0 && Math.max(...z.drawMs) < 50 && z.long.length === 0, JSON.stringify(z));
}

/* ---- files (docs/ui-v2.md section 4, T5) ------------------------------------------ */

/** the files of an <input type=file>, as a user picking them would set them */
async function pickFiles(selector, files) {
  const {root} = await cdp.send('DOM.getDocument', {depth: 0});
  const {nodeId} = await cdp.send('DOM.querySelector', {nodeId: root.nodeId, selector});
  if (!nodeId) throw new Error(`no ${selector}`);
  await cdp.send('DOM.setFileInputFiles', {nodeId, files});
}

const sha256 = data => crypto.createHash('sha256').update(data).digest('hex');
const par = name => S(`(s.core.pars.find(p => p[0] === ${JSON.stringify(name)}) || [])[1]`);
async function setPar(name, value) {
  await cdp.eval(`__xpp.send({cmd: 'set', kind: 'par', name: ${JSON.stringify(name)}, text: '${value}'})`);
  return until(`!s.busy && Math.abs(s.core.pars.find(p => p[0] === ${JSON.stringify(name)})[1] - ${value}) < 1e-12`, 'set');
}

/** File, then the File menu's key: the dialog of the file ask it opens */
async function fileMenu(k, mode) {
  await focusPlot();
  await key('f');
  await until('!s.busy && s.core.menu === 1', 'the File menu');
  await key(k);
  return until(`s.ask && s.ask.kind === 'file' && s.ask.mode === '${mode}' && document.querySelector('.file-ask')`, `file ask ${k}`);
}

async function waitFile(p, ms = 10000) {
  const t0 = Date.now();
  while (Date.now() - t0 < ms) {
    if (fs.existsSync(p) && fs.statSync(p).size > 0) return fs.readFileSync(p);
    await sleep(100);
  }
  return null;
}

async function files(dir) {
  await desktopMetrics();
  await until('!s.busy && !s.ask', 'idle');
  /* a headless browser shows no picker: the page takes its fallbacks, the
     <input type=file> and the download, which a test can drive */
  await cdp.eval('window.showOpenFilePicker = undefined; window.showSaveFilePicker = undefined; true');
  const downloads = fs.mkdtempSync(path.join(os.tmpdir(), 'xppweb2-dl-'));
  const up = fs.mkdtempSync(path.join(os.tmpdir(), 'xppweb2-up-'));
  let canDownload = true;
  await cdp.send('Browser.setDownloadBehavior', {behavior: 'allow', downloadPath: downloads})
    .catch(() => cdp.send('Page.setDownloadBehavior', {behavior: 'allow', downloadPath: downloads}))
    .catch(() => { canDownload = false; });
  try {
    const iapp0 = await par('iapp');

    /* File/Write set: the core writes into the model's folder, the page offers it */
    check('File/Write set opens a save dialog (the ask says it writes)', await fileMenu('w', 'write'),
      JSON.stringify(await S('s.ask')));
    await cdp.eval(`(() => { const i = document.querySelector('[data-file-name]'); i.value = 't5.set';
      i.dispatchEvent(new Event('input', {bubbles: true})); i.focus(); })()`);
    await sleep(50);
    await key('Enter');
    check('the ask is answered with the name', (await lastAnswer())?.file === 't5.set', JSON.stringify(await lastAnswer()));
    const offered = await until("s.files.offered && s.files.offered.name === 't5.set' && !s.busy", 'offered');
    const saved = fs.existsSync(path.join(dir, 't5.set')) ? fs.readFileSync(path.join(dir, 't5.set')) : null;
    check('Write set lands in the model\'s folder', saved && saved.length > 100, String(saved && saved.length));
    const off = await S('s.files.offered');
    check('... and is offered to the browser as a download, the same bytes',
      offered && saved && off.how === 'download' && off.size === saved.length && off.sha256 === sha256(saved), JSON.stringify(off));
    if (canDownload) {
      const got = await waitFile(path.join(downloads, 't5.set'));
      check('the browser downloaded it', got && saved && got.equals(saved), String(got && got.length));
    }

    /* File/Read set by upload restores the parameters */
    check('a new parameter value', await setPar('iapp', 0.2));
    fs.copyFileSync(path.join(dir, 't5.set'), path.join(up, 't5up.set'));
    check('File/Read set opens an open dialog (the ask says it reads)', await fileMenu('r', 'read'),
      JSON.stringify(await S('s.ask')));
    await pickFiles('[data-file-input=open]', [path.join(up, 't5up.set')]);
    check('Read set by upload restores the parameters (the next state has the file\'s values)',
      await until(`!s.ask && !s.busy && Math.abs(s.core.pars.find(p => p[0] === 'iapp')[1] - ${iapp0}) < 1e-12`, 'read set'),
      String(await par('iapp')));
    const copied = fs.existsSync(path.join(dir, 't5up.set')) && fs.readFileSync(path.join(dir, 't5up.set'));
    check('the picked file was copied into the model\'s folder and the ask answered with its name',
      copied && copied.equals(saved) && (await lastAnswer())?.file === 't5up.set'
      && (await S('s.files.uploads[0].copied')) === true, JSON.stringify(await S('s.files.uploads')));

    /* the same content again: not copied, still answered */
    await setPar('iapp', 0.2);
    await fileMenu('r', 'read');
    await pickFiles('[data-file-input=open]', [path.join(up, 't5up.set')]);
    check('a file already there with the same content is not copied again',
      await until(`!s.ask && !s.busy && s.files.uploads.length === 1 && s.files.uploads[0].copied === false`, 'same')
      && Math.abs(await par('iapp') - iapp0) < 1e-12, JSON.stringify(await S('s.files.uploads')));

    /* the same name with other content: the replace confirm */
    await setPar('iapp', 0.123);
    await fileMenu('w', 'write');
    await cdp.eval(`(() => { const i = document.querySelector('[data-file-name]'); i.value = 't5b.set';
      i.dispatchEvent(new Event('input', {bubbles: true})); i.focus(); })()`);
    await sleep(50);
    await key('Enter');
    await until("s.files.offered && s.files.offered.name === 't5b.set' && !s.busy", 'offered t5b');
    fs.mkdirSync(path.join(up, 'other'));
    const other = path.join(up, 'other', 't5up.set');
    fs.copyFileSync(path.join(dir, 't5b.set'), other);
    await setPar('iapp', 0.3);
    await fileMenu('r', 'read');
    await pickFiles('[data-file-input=open]', [other]);
    check('a same-name upload with other content asks: Replace, Keep both, Cancel',
      await until(`s.files.confirm && s.files.confirm.name === 't5up.set' && s.files.confirm.keepBoth === 't5up-2.set'
        && document.querySelectorAll('[data-confirm] [data-choice]').length === 3`, 'confirm'),
      JSON.stringify(await S('s.files.confirm')));
    await cdp.eval(`document.querySelector('[data-choice=cancel]').click()`);
    check('Cancel copies nothing and leaves the prompt open',
      await until(`!s.files.confirm && s.ask && s.ask.kind === 'file'`, 'cancel')
      && fs.readFileSync(path.join(dir, 't5up.set')).equals(saved) && !fs.existsSync(path.join(dir, 't5up-2.set')));
    await pickFiles('[data-file-input=open]', [other]);
    await until('s.files.confirm', 'confirm again');
    await cdp.eval(`document.querySelector('[data-choice=keep]').click()`);
    check('Keep both copies it as name-2.ext, keeps the old one, and reads the new one',
      await until(`!s.ask && !s.busy && Math.abs(s.core.pars.find(p => p[0] === 'iapp')[1] - 0.123) < 1e-12`, 'keep both')
      && fs.readFileSync(path.join(dir, 't5up.set')).equals(saved)
      && fs.readFileSync(path.join(dir, 't5up-2.set')).equals(fs.readFileSync(other))
      && (await lastAnswer())?.file === 't5up-2.set', String(await par('iapp')));

    /* a file the core cannot open: "Add file…" copies it under that name and runs the command again */
    await setPar('iapp', 0.4);
    await fileMenu('r', 'read');
    await cdp.eval(`document.getElementById('file-tab-folder').click()`);
    await until('document.querySelector("[data-folder-file]")', 'folder tab');
    await cdp.eval(`(() => { const i = document.querySelector('[data-folder-file]'); i.value = 'gone.set';
      i.dispatchEvent(new Event('input', {bubbles: true})); i.focus(); })()`);
    await sleep(50);
    await key('Enter');
    check('the core\'s listing answers with the name (In the model\'s folder)', (await lastAnswer())?.file === 'gone.set');
    check('a file the core cannot open: the notification offers "Add file…"',
      await until(`s.toasts.some(t => t.action && t.action.name === 'gone.set') && document.querySelector('[data-add-file="gone.set"]')`,
        'add file'), JSON.stringify(await S('s.toasts')));
    await pickFiles('[data-file-input=add]', [path.join(up, 't5up.set')]);
    check('Add file… copies it under that name and runs the command again',
      await until(`!s.busy && !s.ask && Math.abs(s.core.pars.find(p => p[0] === 'iapp')[1] - ${iapp0}) < 1e-12
        && !s.toasts.some(t => t.action)`, 'replayed')
      && fs.existsSync(path.join(dir, 'gone.set')) && fs.readFileSync(path.join(dir, 'gone.set')).equals(saved),
      JSON.stringify([await par('iapp'), await S('s.toasts'), await cdp.eval('__xpp.sent().slice(-4)')]));
    check('no file dialog is left open', await S('!s.ask'));
  } finally {
    await cdp.send('Browser.setDownloadBehavior', {behavior: 'default'}).catch(() => {});
    fs.rmSync(downloads, {recursive: true, force: true, maxRetries: 5});
    fs.rmSync(up, {recursive: true, force: true, maxRetries: 5});
  }
}

/* ---- animation (docs/ui-v2.md T13) ---------------------------------------------- */

/* the frame the store holds, the one drawn (__xpp.ani()), and the slider, once the core is idle */
const aniShown = pos => until(`!s.busy && s.ani.frame && s.ani.frame.pos === ${pos} && __xpp.ani()
  && __xpp.ani().pos === ${pos} && +document.querySelector('.ani-slider').value === ${pos}`, `frame ${pos}`);
const focusStage = () => cdp.eval(`document.querySelector('.ani-stage').focus()`);

async function animation() {
  await desktopMetrics();
  check('ani: the page connects', await until('s.hello && s.seriesCount >= 1 && !s.busy', 'hello'));
  check('ani: the server offers the ani data event and the page asks for it',
    await S(`s.hello.features.includes('ani')`)
    && (await cdp.eval('__xpp.sent()')).some(c => c.cmd === 'data' && c.events.includes('ani')));
  check('ani: integrate first (601 rows)', await integrate(601, 30000));

  /* the title bar's Animation button: Viewaxes/Toon opens the core's window, the panel shows */
  await cdp.eval(`document.querySelector('.ani-toggle').click()`);
  check('ani: the Animation button opens the core\'s animation window and the panel',
    await until(`s.ani.open && s.ani.exists && !s.busy`, 'ani window')
    && await cdp.eval(`getComputedStyle(document.querySelector('.ani-panel')).visibility === 'visible'`));
  check('ani: the picture has the focus', await until(`document.activeElement.classList.contains('ani-stage')`, 'stage focus'));

  /* Load: the file ask for reading, answered by an upload of tools/gui_test.ani */
  await cdp.eval('window.showOpenFilePicker = undefined; true');
  await cdp.eval(`[...document.querySelectorAll('.ani-header button')].find(b => b.textContent.startsWith('Load')).click()`);
  check('ani: Load asks for a file to open', await until(`s.ask && s.ask.kind === 'file' && s.ask.mode === 'read'
    && document.querySelector('[data-file-input=open]')`, 'ani file ask'), JSON.stringify(await S('s.ask')));
  await pickFiles('[data-file-input=open]', [path.join(top, 'tools/gui_test.ani')]);
  check('ani: loading it shows its first frame (store and drawing)', await aniShown(0),
    JSON.stringify(await S('s.ani.frame && {pos: s.ani.frame.pos, n: s.ani.frame.prims.length}')));
  const f0 = await S('s.ani.frame');
  check('ani: the frame\'s primitives are in the store in unit coordinates, every kind',
    ['line', 'rect', 'circle', 'ellipse', 'text', 'dot'].every(k => f0.prims.some(p => p.kind === k))
    && f0.prims.filter(p => p.kind === 'line').every(p => [p.u1, p.v1, p.u2, p.v2].every(x => x >= -0.5 && x <= 1.5)),
    JSON.stringify(f0.prims.slice(0, 4)));
  const d0 = await cdp.eval('__xpp.ani()');
  const aspect = (f0.dim[2] - f0.dim[0]) / (f0.dim[3] - f0.dim[1]);
  check('ani: drawn with every primitive, at the dimension box\'s aspect',
    d0.prims === f0.prims.length && Math.abs(d0.box.w / d0.box.h - aspect) < 1e-6 && d0.box.w > 100,
    JSON.stringify(d0));
  check('ani: nothing plays by itself (A6): no Go was sent', !(await cdp.eval('__xpp.sent()')).some(c => c.cmd === 'ani' && c.op === 'go'));

  /* keyboard on the picture: arrows step, Shift ten, End and Home */
  await focusStage();
  await key('ArrowRight');
  check('ani: Right arrow steps a frame', await aniShown(1));
  await key('ArrowRight', 8);
  check('ani: Shift+Right steps ten', await aniShown(11));
  await key('ArrowLeft');
  check('ani: Left arrow steps back', await aniShown(10));
  await key('End');
  check('ani: End goes to the last frame', await aniShown(600));
  await key('Home');
  check('ani: Home goes to the first frame', await aniShown(0));

  /* the seek slider */
  await cdp.eval(`(() => { const r = document.querySelector('.ani-slider'); r.value = '300';
    r.dispatchEvent(new Event('input', {bubbles: true})); r.dispatchEvent(new Event('change', {bubbles: true})); })()`);
  check('ani: the seek slider seeks (frame 300)', await aniShown(300));

  /* the delay between frames */
  await cdp.eval(`(() => { const s = document.querySelector('.ani-speed select'); s.value = '20';
    s.dispatchEvent(new Event('change', {bubbles: true})); })()`);
  check('ani: the delay select sets the speed', await until('!s.busy && s.ani.speed === 20', 'speed 20'));

  /* Space plays, the frames advance, Space pauses */
  await focusStage();
  const n0 = await S('s.ani.frames');
  await key(' ');
  check('ani: Space plays (Go)', await until('s.ani.playing', 'playing')
    && (await cdp.eval('__xpp.sent()')).some(c => c.cmd === 'ani' && c.op === 'go'));
  check('ani: the frames advance while it plays', await until(`s.ani.frames >= ${n0} + 3 && s.ani.frame.pos > 300`, 'advance'),
    JSON.stringify(await S('[s.ani.frames, s.ani.frame.pos]')));
  await focusStage();
  await key(' ');
  check('ani: Space pauses', await until('!s.ani.playing && !s.busy', 'paused'));
  const paused = await S('s.ani.frame.pos');
  check('ani: paused on a frame before the end, drawn', paused > 300 && paused < 600
    && await until(`__xpp.ani().pos === ${paused}`, 'paused drawn'), String(paused));
  await focusStage();
  await key('ArrowLeft');
  check('ani: after a pause a step goes from the frame shown', await aniShown(paused - 1),
    JSON.stringify(await S('[s.ani.frame.pos, s.ani.pos]')));

  /* the Play button, played to the end */
  await cdp.eval(`document.querySelector('.ani-speed select').value = '0';
    document.querySelector('.ani-speed select').dispatchEvent(new Event('change', {bubbles: true}))`);
  await until('!s.busy && s.ani.speed === 0', 'speed 0');
  await cdp.eval(`document.querySelector('.ani-play').click()`);
  check('ani: the Play button plays to the last frame', await until('!s.ani.playing && !s.busy && s.ani.frame.pos === 600', 'end', 30000)
    && await until('__xpp.ani().pos === 600', 'end drawn'), JSON.stringify(await S('[s.ani.frame.pos, s.ani.playing]')));
  await cdp.eval(`[...document.querySelectorAll('.ani-controls button')].find(b => b.getAttribute('aria-label') === 'One frame back').click()`);
  check('ani: the step back button, from the last frame shown', await aniShown(599));

  /* any size: a smaller window keeps the aspect */
  await cdp.send('Emulation.setDeviceMetricsOverride', {width: 1280, height: 600, deviceScaleFactor: 1, mobile: false});
  check('ani: drawn again at another size, the same aspect', await until(`__xpp.ani().height < ${d0.height}`, 'resized')
    && Math.abs((await cdp.eval('__xpp.ani().box.w / __xpp.ani().box.h')) - aspect) < 1e-6,
    JSON.stringify(await cdp.eval('__xpp.ani()')));

  /* a reload: the server shows the new page the animation window again, and the data
     subscription brings the last frame */
  await cdp.send('Page.reload');
  await sleep(300);
  check('ani: after a reload the panel shows the core\'s window and its last frame again',
    await until('s.hello && !s.busy && s.ani.open && s.ani.exists', 'window again') && await aniShown(599),
    JSON.stringify(await S('[s.ani.open, s.ani.exists, s.busy, s.ani.frame && s.ani.frame.pos]')));

  /* a phone: a full-screen sheet, 44px targets, no sideways scroll */
  await cdp.send('Emulation.setDeviceMetricsOverride', {width: 390, height: 844, deviceScaleFactor: 2, mobile: true});
  await cdp.send('Emulation.setTouchEmulationEnabled', {enabled: true, maxTouchPoints: 5});
  await cdp.send('Emulation.setEmulatedMedia', {features: [{name: 'pointer', value: 'coarse'}, {name: 'hover', value: 'none'}]}).catch(() => {});
  await sleep(400);
  const scroll = await cdp.eval(`({doc: document.documentElement.scrollWidth, body: document.body.scrollWidth, w: innerWidth})`);
  check('ani 390x844: no sideways scroll', scroll.doc <= scroll.w && scroll.body <= scroll.w, JSON.stringify(scroll));
  check('ani 390x844: the panel is a full-screen sheet', await cdp.eval(`(() => { const r = document.querySelector('.ani-panel')
    .getBoundingClientRect(); return r.width >= innerWidth - 1 && r.height >= innerHeight - 1; })()`));
  const small = await cdp.eval(`[...document.querySelectorAll('.ani-panel button, .ani-panel select, .ani-panel input')]
    .filter(b => b.getClientRects().length).map(b => [(b.getAttribute('aria-label') || b.textContent || b.className).trim(),
      b.getBoundingClientRect().height]).filter(([, h]) => h < 44)`);
  check('ani 390x844: the sheet\'s targets are at least 44px high', small.length === 0, JSON.stringify(small));
  check('ani 390x844: the picture fits the width at its aspect', await until(`__xpp.ani().width <= 390
    && Math.abs(__xpp.ani().box.w / __xpp.ani().box.h - ${aspect}) < 1e-6`, 'phone drawn'), JSON.stringify(await cdp.eval('__xpp.ani()')));
  await cdp.eval(`document.querySelector('.ani-back').click()`);
  check('ani: Back closes the sheet, the focus back on the Animation button',
    await until('!s.ani.open', 'close ani') && await until(`document.activeElement.closest('.ani-toggle')`, 'ani focus back'));
  await desktopMetrics();
}

/* structure only (header, each frame's size, the trailer), not the LZW
   pixels: web2/test/gif.test.ts already checks the encoder decodes right;
   this just confirms what __xpp.kinescopeGif() produced is a real GIF with
   the frames a kinescope export should have. */
function parseGifStructure(buf) {
  let p = 0;
  const u8 = () => buf[p++];
  const u16 = () => { const v = buf[p] | (buf[p + 1] << 8); p += 2; return v; };
  const header = buf.toString('ascii', 0, 6);
  if (header !== 'GIF87a' && header !== 'GIF89a') throw new Error(`not a GIF: ${header}`);
  p = 6;
  const w = u16(), h = u16();
  const packed = u8();
  u8(); u8();
  if (packed & 0x80) p += (2 << (packed & 7)) * 3;
  const frames = [];
  for (;;) {
    const block = u8();
    if (block === 0x3b || p >= buf.length) break;
    if (block === 0x21) {
      u8();
      let len;
      while ((len = u8()) !== 0) p += len;
      continue;
    }
    if (block !== 0x2c) throw new Error(`unexpected GIF block 0x${block.toString(16)} at ${p - 1}`);
    u16(); u16();
    const fw = u16(), fh = u16();
    const ipacked = u8();
    if (ipacked & 0x80) p += (2 << (ipacked & 7)) * 3;
    u8(); /* LZW minimum code size */
    let len;
    while ((len = u8()) !== 0) p += len;
    frames.push({w: fw, h: fh});
  }
  return {w, h, frames};
}

/* Kinescope (docs/ui-v2.md T15): capturing two frames of two different
   integrations, playing them, resetting, the client's own GIF export
   (plot/gif.ts, web2/test/gif.test.ts covers the encoder itself), and the
   core's own Kinescope writers (Make Anigif) getting real pixels back
   from a `pixels` ask (session.ts answerPixels) instead of the ok:0 this
   task replaces. */
async function kinescope(dir) {
  await desktopMetrics();
  check('kinescope: the page connects', await until('s.hello && s.seriesCount >= 1 && !s.busy', 'hello'));
  check('kinescope: integrate once (601 rows)', await integrate(601, 30000));

  const openKinescope = async item => {
    await key('k');
    if (!(await until("s.ask && s.ask.kind === 'menu'", 'kinescope menu'))) return false;
    await key(item);
    return true;
  };

  check('kinescope: Capture (k, c) sends a film capture', await openKinescope('c')
    && await until('s.kinescope.frames.length === 1 && !s.busy', 'frame 1'));

  check('kinescope: a different parameter, integrated again', await setPar('iapp', (await par('iapp')) + 0.05)
    && await integrate(601, 30000));
  check('kinescope: a second capture', await openKinescope('c')
    && await until('s.kinescope.frames.length === 2 && !s.busy', 'frame 2'));

  const differ = await cdp.eval(`(() => {
    const f = __xpp.state().kinescope.frames;
    const col = fr => Array.from(fr.series.columns.get(fr.series.curves[0].y) ?? []);
    return JSON.stringify(col(f[0])) !== JSON.stringify(col(f[1]));
  })()`);
  check('kinescope: the two captured frames hold different data', differ);

  const gifB64 = await cdp.eval('__xpp.kinescopeGif()');
  check('kinescope: Export GIF (client-side) produces a GIF', !!gifB64);
  if (gifB64) {
    const gif = parseGifStructure(Buffer.from(gifB64, 'base64'));
    check('kinescope: the GIF has one frame per capture, all the same size',
      gif.frames.length === 2 && gif.frames.every(f => f.w === gif.w && f.h === gif.h && f.w > 0 && f.h > 0),
      JSON.stringify(gif));
  }

  check('kinescope: Playback (k, p) shows frame 1 then frame 2', await openKinescope('p')
    && await until('s.kinescope.playing && s.kinescope.shown === 0', 'showing 0')
    && await until('s.kinescope.shown === 1', 'showing 1')
    && await until('!s.kinescope.playing && !s.busy', 'play done'));

  /* Make Anigif (k, m): no prompt, so a plain menu pick; core/json_windows.cpp's
     j_movie_make_anigif asks `pixels` for every captured frame and writes
     anim.gif in the model's folder itself */
  const animPath = path.join(dir, 'anim.gif');
  check('kinescope: Make Anigif (k, m) runs', await openKinescope('m') && await until('!s.busy', 'anigif done'));
  for (let t0 = Date.now(); !fs.existsSync(animPath) && Date.now() - t0 < 5000;) await sleep(50);
  check('kinescope: it wrote anim.gif from the pixels answer (a real GIF, not empty)',
    fs.existsSync(animPath) && fs.readFileSync(animPath).length > 20
    && fs.readFileSync(animPath).toString('ascii', 0, 3) === 'GIF',
    fs.existsSync(animPath) ? String(fs.statSync(animPath).size) : 'missing');

  check('kinescope: Reset (k, r) empties the frames', await openKinescope('r')
    && await until('s.kinescope.frames.length === 0 && s.kinescope.shown === null && !s.busy', 'reset'));
  await desktopMetrics();
}

/* xppautX in browser mode on a copy of `ode`, the page at /, then `fn`
   (given the model's folder); the server stops after it. `expected` are
   errors the session provokes on purpose. */
async function session(ode, fn, expected = []) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'xppweb2-'));
  fs.copyFileSync(ode, path.join(dir, path.basename(ode)));
  const server = await startServer(bin, dir, [path.basename(ode)]);
  try {
    await cdp.send('Page.navigate', {url: server.url});
    await fn(dir);
    const errors = (await S('s.log.filter(l => l.kind === "error").map(l => l.text)')).filter(e => !expected.includes(e));
    check(`${path.basename(ode)}: no errors reported by the core`, errors.length === 0, JSON.stringify(errors));
  } finally {
    server.proc.kill();
    await sleep(300);
    fs.rmSync(dir, {recursive: true, force: true, maxRetries: 5});
  }
}

async function main() {
  const browser = findBrowser(opt.browser);
  if (!browser) {
    console.log('web2check: no Chrome, Chromium or Edge found (set CHROME=path); skipped');
    process.exit(0);
  }
  if (!fs.existsSync(bin)) throw new Error(`no ${bin}: build xppautX first`);
  const want = outputDat(), wantLive = outputDat(LIVE);
  const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'xppweb2-profile-'));
  const b = await startBrowser(browser, profile);
  cdp = b.cdp;
  try {
    await cdp.send('Page.enable');
    await cdp.send('Emulation.setDeviceMetricsOverride', {width: 1280, height: 860, deviceScaleFactor: 1, mobile: false});
    const run = name => !opt.only || opt.only.split(',').includes(name);
    if (run('desktop')) await session(ODE, async () => {
      await desktop(want);
      /* before values(): its slider moves a parameter and reruns the
         integration (docs/protocol.md `slide`), so the stored data would no
         longer match the pristine `want` computed from the ODE file's own
         defaults */
      await dataTable(want);
      await values();
      await keyboardOnly();
      await phone();
      await prompts();
      await windows();
      await textViews();
    });
    if (run('phase')) await session(ODE, phasePlane);
    if (run('auto')) await session(ODE, autoView);
    if (run('keys')) await session(ODE, keysCheck);
    if (run('busy')) await session(LIVE, busyAuto);
    if (run('view')) await session(ODE, viewCheck);
    if (run('three')) await session(LORENZ_ODE, threePlot);
    if (run('marks')) await session(ODE, marks);
    if (run('aplot')) await session(APLOT_ODE, aplotView);
    if (run('files')) await session(ODE, files, ['Cannot open file']);
    if (run('live')) await session(LIVE, () => live(wantLive));
    if (run('million')) await session(MILLION, million);
    if (run('ani')) await session(ODE, animation);
    if (run('kinescope')) await session(ODE, kinescope);
    if (run('runs')) await session(ODE, runsCheck);
    if (run('values')) await session(LIVE, valuesLive);
    if (run('help')) await session(ODE, helpCheck);
  } finally {
    b.proc.kill();
    await sleep(500);
    fs.rmSync(profile, {recursive: true, force: true, maxRetries: 5});
  }
  console.log(`web2 checks: ${failures ? `${failures} failed` : 'all passed'}`);
  process.exit(failures ? 1 : 0);
}

main().catch(e => {
  console.error(e.stack || e);
  process.exit(2);
});
