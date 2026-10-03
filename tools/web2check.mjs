#!/usr/bin/env node
/* State-level check of the front end (web2/, served at /): drives a headless Chrome or Edge through real
   key presses, mouse and touch events
   and asserts what the page's store and plot hold (window.__xpp), never
   pixels. A desktop session (integrate from the keyboard, the plotted
   numbers against output.dat, hover, wheel and box zoom, reset, pan),
   the AUTO view (docs/ui-v2.md T11a: lecar's steady state and the periodic
   branch from its Hopf point, the store against the diagram events, the
   curves and labels drawn, the Hopf join, the readout by mouse and keys,
   zoom and Fit, the sheet on a phone, Close),
   the values panel (docs/ui-v2.md T3, GitHub #117: edit a parameter, a
   slider by keyboard, edits sent at once (W106), Tab reachability,
   the panel as a right column), the data
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
   Erase). 3D plots (T14, examples/ode/lorenz.odex: the store's own window,
   the projection drawn on a canvas, a drag and the arrow keys turning it
   locally at once, throttled 3d-params turns (key 3), and state.view settling
   to agree). Then live plotting (tools/models/live.odex: the store
   and the plot grow while 20 001 rows are computed, and end as output.dat)
   and a run of 10^6 rows (tools/models/million.odex) that draws and zooms,
   its draw times and long tasks (read through __xpp; W40), and both runs'
   frame pacing and series appends per second (W82), printed as perf:
   lines, never pass/fail (W58: performance is for CI, not the program).
   Files (T5): Save session lands in the model's folder and is downloaded,
   Import XPPAUT set by upload restores the parameters, a same-content upload is not
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
   branches so far, AUTO settings are held in the session, grabbing a labelled periodic point plots its limit cycle, Tab in a
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
   reopening it where it was left. `loaderror` (W63c): a model that does not
   load shows the core's `error` event, its file, line and cause, with the
   line as written, and no hello.

   node tools/web2check.mjs [--bin ./xppautX] [--browser PATH]
     [--only desktop,layout,phase,marks,auto,autoviews,lostf,keys,record,player,leave,busy,view,three,aplot,files,live,million,ani,kinescope,runs,values,help,loaderror] [-v]
   A section a check fails in is rerun once; still failing is a FAIL,
   passing on the rerun is FLAKY. A command the page said did not reach
   the core (or was refused) is a FAIL in any attempt, never a flake (W124):
   the page sends one again itself before it says so. XPP_CHECK_SLOW=N
   scales safety timeouts
   for a slow runner (W40); it never scales a pass/fail budget (W58: draw
   times, long tasks and Stop latency are measured as perf: lines, never
   failed). XPP_NETLOG=1 prints every /cmd POST the browser failed, with
   its network error (CDP's Network domain: off by default, since it also
   carries every event of the stream).

   Needs Node 22 or later and a browser, nothing else (tools/cdp.mjs). */
import {spawnSync} from 'node:child_process';
import crypto from 'node:crypto';
import fs from 'node:fs';
import net from 'node:net';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {findBrowser, installPerfObserver, waitFor, waitForExit, startBrowser, startServer, stopServer} from './cdp.mjs';

const top = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const opt = {bin: `xppautX${process.platform === 'win32' ? '.exe' : ''}`};
for (let i = 2; i < process.argv.length; i++) {
  const a = process.argv[i];
  if (a === '-v') opt.v = true;
  else opt[a.replace(/^--/, '')] = process.argv[++i];
}
const bin = path.resolve(top, opt.bin);
const ODE = path.join(top, 'examples/ode/lecar.odex');
const LIVE = path.join(top, 'tools/models/live.odex');
const MILLION = path.join(top, 'tools/models/million.odex');
const APLOT_ODE = path.join(top, 'examples/ode/wcring.odex');
const LORENZ_ODE = path.join(top, 'examples/ode/lorenz.odex');
const HEAVY_ODE = path.join(top, 'tools/models/heavy.odex');

/* macos-ui is the slowest, least steady runner (W40, GitHub #83): a wait for
   something to settle needs slack a fast machine never sees. XPP_CHECK_SLOW
   (the Python checks' own name for this) scales every such safety timeout;
   it never scales a pass/fail budget any more (W58: frame draw times, long
   tasks and Stop latency are `perf:` lines, measured, never failed). */
const SLOW = Number(process.env.XPP_CHECK_SLOW) || 1;

let failures = 0, flaky = 0;
/* while a section (session(), below) is retrying a failed run, checks are
   collected here instead of printed at once, so a check that failed on the
   first attempt but passed on the rerun can be reported FLAKY rather than
   silently as a pass, and one that fails twice still fails (W40). */
let record = null;
/* the xppautX the section running drives (sessionAttempt), for a check that it exited */
let sessionServer = null;
function check(name, ok, detail = '') {
  if (record) { record.push({name, ok, detail}); return; }
  console.log(`${ok ? 'PASS' : 'FAIL'} ${name}${ok ? '' : '  ' + detail}`);
  if (!ok) failures++;
}

/* A measurement, never pass/fail (the maintainer: performance is for CI, not
   the program, and a check that races the clock is a design problem, not a
   speed problem -- W58). Printed even while a section's checks are being
   collected for a possible rerun (record), since a measurement is not
   something a rerun could change the truth of. */
function perf(name, value) {
  console.log(`perf: ${name} ${value}`);
}

/* output.dat of the same model: the numbers the plot must show */
function outputDat(ode = ODE) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'xppsilent-'));
  fs.copyFileSync(ode, path.join(dir, path.basename(ode)));
  spawnSync(bin, [path.basename(ode), '--silent'], {cwd: dir, stdio: 'ignore', timeout: 60000});
  const rows = fs.readFileSync(path.join(dir, 'output.dat'), 'utf8').trim().split(/\r?\n/).map(l => l.trim().split(/\s+/).map(Number));
  fs.rmSync(dir, {recursive: true, force: true});
  return rows;
}

/* ---- page helpers --------------------------------------------------------- */

let cdp;
let downloads; /* shared downloads folder for all sections (set by main) */
/* in page expressions: `s` is the store's state, `w` its active plot window
   (store/plots.ts: series, viewport) */
const ACTIVE = 's.plots.windows.find(x => x.win === s.plots.active) || {}';
/* the AUTO diagram's active view (W50: the diagram has views, each its own axes and points) */
const DV = 's && s.diagram.views[s.diagram.active]';
const S = expr => cdp.eval(`(() => { const s = __xpp.state(), w = ${ACTIVE}; return ${expr}; })()`);
/* Reads acknowledge the component callbacks, CSS transitions and the
   ResizeObserver delivery before measuring geometry or drawing data. */
const rawP = () => cdp.eval('__xpp.plot()');
const P = () => settled(rawP);
async function rendered() {
  if (!await until('window.__xpp && __xpp.rendered()', 'component renders and effects'))
    throw new Error('component renders and effects did not complete');
  await cdp.eval(`Promise.all(document.getAnimations().filter(a => a instanceof CSSTransition)
    .map(a => a.finished.catch(() => {})))`);
  await cdp.eval(`new Promise(resolve => {
    const observer = new ResizeObserver(() => { observer.disconnect(); resolve(true); });
    observer.observe(document.documentElement);
  })`);
  if (!await until('__xpp.rendered()', 'renders after resize'))
    throw new Error('component renders after resize did not complete');
}
async function settled(read) {
  await rendered();
  if (!await until('!__xpp.plot()?.tracing', 'plot tracing complete'))
    throw new Error('plot tracing did not complete');
  return read();
}
const steadyP = P;

let acceptSave = true; /* scratch saves are authorized; the W129 No check opts out */
async function until(expr, what, ms) {
  const ready = await waitFor(async () => {
    let v = null;
    try {
      v = await cdp.eval(`(() => { try { const s = window.__xpp && __xpp.state(), w = ${ACTIVE}, dv = ${DV}; return !!(${expr}); } catch (e) { return false; } })()`);
    } catch { /* page not there yet */ }
    if (v) return true;
    if (acceptSave) {
      await cdp.eval(`(() => { const a = window.__xpp && __xpp.state().ask;
        if (a && a.kind === 'choice' && a.keys === 'yn' && a.question.endsWith(' exists. Replace it?'))
          __xpp.send({cmd: 'answer', id: a.id, key: 'y'}); })()`).catch(() => {});
    }
    return false;
  }, ms);
  if (!ready && opt.v) console.log(`  (timed out waiting for ${what})`);
  return !!ready;
}

/* Page.reload returns before the old document is gone: a wait right after
   it could be met by the old page, and the next read then hit the new one
   while it loads, before __xpp exists (CI's linux-ui, GitHub #210). The old
   page is marked, as a navigation's is (W20), and the new one waited for. */
async function reloadPage() {
  await cdp.eval('window.__left = true').catch(() => {});
  await cdp.send('Page.reload');
  return until('!window.__left && s.hello', 'the reloaded page', 30000 * SLOW);
}

async function metrics(value) {
  await cdp.send('Emulation.setDeviceMetricsOverride', value);
  if (!await until(`innerWidth === ${value.width} && innerHeight === ${value.height}`, 'viewport dimensions'))
    throw new Error('viewport dimensions did not update');
  await rendered();
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
  await rendered();
}
/* an error's dialog (ErrorDialog.tsx) covers the page until OK: close it so a later click reaches its control */
async function closeErrors() {
  await cdp.eval(`document.querySelector('[data-error-ok]')?.click()`);
  await until(`!document.querySelector('.error-dialog')`, 'the error dialog closed');
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
  const pre = await S('__xpp.sentCount()');
  await click(x, y);
  await rendered();
  const sent = await S(`__xpp.sentFrom(${pre})`);
  check(name, sent.length === 0 && !(await S('s.busy')), JSON.stringify(sent));
}

/* the plotting area's box and the screen position of point i of curve c */
const rawArea = () => cdp.eval(`(() => { const r = document.querySelector('.plot-view:not([hidden]) .u-over').getBoundingClientRect();
  return {x: r.left, y: r.top, w: r.width, h: r.height}; })()`);
const area = () => settled(rawArea);
async function screenOf(curve, i) {
  const [a, p, s] = [await area(), await P(), await cdp.eval(`(() => { const s = __xpp.state(), m = (${ACTIVE}).series;
    const c = m.curves[${curve}]; return {x: m.columns.get(c.x)[${i}], y: m.columns.get(c.y)[${i}]}; })()`)];
  return {x: a.x + (s.x - p.x.min) / (p.x.max - p.x.min) * a.w, y: a.y + (p.y.max - s.y) / (p.y.max - p.y.min) * a.h};
}
const width = r => r.max - r.min;

/* ---- the sessions -------------------------------------------------------------- */

async function desktop(want) {
  await metrics({width: 1280, height: 860, deviceScaleFactor: 1, mobile: false});
  check('the page connects and asks for the plot as data', await until('s.hello && s.seriesCount >= 1 && !s.busy', 'hello'));
  check('an empty plot says so and offers Integrate',
    await cdp.eval(`!!document.querySelector('.plot-empty button')`));
  /* W85: with no data uPlot padded the window and rounded it to "nice"
     values (lecar's -0.25..1.2 read as about -0.4..1.4 until the first run) */
  const emptyPlot = await P(), emptyView = await S('s.core.view');
  check("an empty plot shows the core's window exactly, before any run (W85)",
    emptyPlot && emptyView && [[emptyPlot.x.min, emptyView.xlo], [emptyPlot.x.max, emptyView.xhi],
      [emptyPlot.y.min, emptyView.ylo], [emptyPlot.y.max, emptyView.yhi]].every(([a, b]) => Math.abs(a - b) < 1e-9),
    JSON.stringify([emptyPlot && emptyPlot.x, emptyPlot && emptyPlot.y, emptyView]));

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
  /* the DOM readout is a render behind the store's own hover field (W20):
     wait for its text rather than reading it the instant the store settled */
  check('the readout shows it',
    await until(`document.querySelector('.readout').textContent.includes('row ${row}')`, 'readout text'),
    await cdp.eval(`document.querySelector('.readout').textContent`));

  /* wheel zoom about the pointer */
  const a = await area(), cx = a.x + a.w / 2, cy = a.y + a.h / 2;
  await mouse('mouseWheel', cx, cy, {deltaX: 0, deltaY: -120});
  await until('w.viewport.x', 'wheel');
  const z1 = await S('w.viewport');
  check('the wheel zooms in about the pointer', z1.x && width(z1.x) < width(p.x) * 0.9, JSON.stringify(z1));
  /* a box: no history any more (GitHub #110), just the latest zoom */
  await mouse('mouseMoved', cx - 80, cy - 60);
  await mouse('mousePressed', cx - 80, cy - 60, {button: 'left', buttons: 1, clickCount: 1});
  for (let s = 1; s <= 6; s++) await mouse('mouseMoved', cx - 80 + 25 * s, cy - 60 + 20 * s, {button: 'left', buttons: 1});
  await mouse('mouseReleased', cx + 70, cy + 60, {button: 'left', buttons: 0, clickCount: 1});
  const boxDone = await until(`w.viewport.x && w.viewport.x.max - w.viewport.x.min < ${width(z1.x) * 0.8}`, 'box zoom settled');
  const z2 = await S('w.viewport');
  check('dragging a box zooms to it', boxDone && z2.x && width(z2.x) < width(z1.x) * 0.8, JSON.stringify({z1, z2}));
  /* pan, once the box zoom is drawn: a press before that frame pans the view drawn, the box's */
  await until(`(() => { const r = __xpp.plot(); return r && Math.abs(r.x.min - ${z2.x.min}) < 1e-9 * Math.abs(${z2.x.max} - ${z2.x.min}); })()`, 'box drawn');
  const before = await S('w.viewport.x');
  await mouse('mousePressed', cx, cy, {button: 'left', buttons: 1, clickCount: 1, modifiers: 8});
  for (let s = 1; s <= 4; s++) await mouse('mouseMoved', cx + 20 * s, cy, {button: 'left', buttons: 1, modifiers: 8});
  await mouse('mouseReleased', cx + 80, cy, {button: 'left', buttons: 0, clickCount: 1, modifiers: 8});
  await until(`w.viewport.x && w.viewport.x.min < ${before.min}`, 'pan settled');
  const after = await S('w.viewport.x');
  check('Shift+drag pans (same width, moved left in data)',
    after.min < before.min && Math.abs(width(after) - width(before)) < 1e-9 * width(before) + 1e-15, JSON.stringify([before, after]));
  /* double click resets */
  await mouse('mousePressed', cx, cy, {button: 'left', buttons: 1, clickCount: 1});
  await mouse('mouseReleased', cx, cy, {button: 'left', clickCount: 1});
  await mouse('mousePressed', cx, cy, {button: 'left', buttons: 1, clickCount: 2});
  await mouse('mouseReleased', cx, cy, {button: 'left', clickCount: 2});
  check("a double click goes back to the core's window", await until('w.viewport.x === null && w.viewport.y === null', 'reset'));
  /* W65: the zoom shown is the core's: every change went to it as a display command, the last one back to its own axes */
  check('the zoom went to the core (display commands), the last one the reset',
    await until(`(() => { const d = __xpp.sent().filter(c => c.cmd === 'display' && c.win === 1);
      return d.length >= 3 && d.some(c => c.x && c.x[1] - c.x[0] < ${width(z1.x)}) && d[d.length - 1].x === null && d[d.length - 1].y === null; })()`,
      'display commands'));
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

/* the values panel (docs/ui-v2.md T3, GitHub #155): parameters, a slider,
   edits sent at once (settings, W106), layout */
async function values() {
  await metrics({width: 1400, height: 900, deviceScaleFactor: 1, mobile: false});
  await rendered();
  const box = await cdp.eval(`(() => { const r = document.querySelector('.values-panel').getBoundingClientRect();
    return {left: r.left, right: r.right, top: r.top, width: r.width, winWidth: innerWidth}; })()`);
  check('at 1400px wide the values panel is a right column',
    box.width > 200 && box.right >= box.winWidth - 2 && box.top < 100, JSON.stringify(box));

  /* edit a parameter: a setting (W106), sent at once as one `set` when committed */
  const field = await cdp.eval(`(() => { const l = [...document.querySelectorAll('.value-field .value-name')]
    .find(e => e.textContent.toLowerCase() === 'iapp'); return l ? l.closest('.value-field').querySelector('input').id : null; })()`);
  check('the iapp field is in the panel', !!field, String(field));
  const before = await S(`(s.core.pars.find(p => p[0].toLowerCase() === 'iapp') || [])[1]`);
  const sent0 = await S('__xpp.sentCount()');
  const iappIs = v => `Math.abs((s.core.pars.find(p => p[0].toLowerCase() === "iapp") || [])[1] - ${v}) < 1e-9`;
  /* two round trips, not one: Preact's state update from 'input' must be flushed (a render) before blur reads it */
  await cdp.eval(`(() => { const el = document.getElementById(${JSON.stringify(field)}); el.focus();
    el.value = '0.2'; el.dispatchEvent(new Event('input', {bubbles: true})); })()`);
  await rendered();
  await cdp.eval(`document.getElementById(${JSON.stringify(field)}).blur()`);
  check('editing a parameter sends it at once, one set, and the core applies it (W106)',
    await until(`!s.busy && ${iappIs(0.2)} && !s.values.inflight.length`, 'iapp set'),
    JSON.stringify(await S(`[__xpp.sentFrom(${sent0}), s.values.inflight, s.core.pars]`)));
  const set0 = (await S(`__xpp.sentFrom(${sent0})`)).filter(c => c.cmd === 'set');
  check('the edit went out as exactly one set with its value',
    set0.length === 1 && set0[0].text === '0.2' && set0[0].kind === 'par', JSON.stringify(set0));
  check('no "pending" banner or dashed field: the value shown is the value',
    !(await cdp.eval(`!!document.querySelector('.values-queued, .value-field.queued')`))
    && (await cdp.eval(`document.getElementById(${JSON.stringify(field)}).value`)) === '0.2');

  /* text typed into a box whose focus event the browser never delivered (CI's Linux Chrome, a
     window without the OS focus) stays as typed: only a committed draft is dropped (W35d) */
  await rendered();
  const preNoFocus = await S('__xpp.sentCount()');
  await cdp.eval(`(() => { const el = document.getElementById(${JSON.stringify(field)});
    el.value = '0.25'; el.dispatchEvent(new Event('input', {bubbles: true})); })()`);
  await rendered();
  const noFocus = await cdp.eval(`(() => { const el = document.getElementById(${JSON.stringify(field)});
    const msg = document.getElementById(el.getAttribute('aria-describedby') || '');
    return {value: el.value, focused: document.activeElement === el, active: document.activeElement?.id || document.activeElement?.tagName,
      message: msg ? msg.textContent : null}; })()`);
  check('text typed without a focus event stays in the box, uncommitted',
    noFocus.value === '0.25' && (await S('__xpp.sentCount()')) === preNoFocus,
    JSON.stringify([noFocus, await S('[s.values.inflight, s.values.errors, s.busy, __xpp.sent().slice(-2)]')]));
  await cdp.eval(`(() => { const el = document.getElementById(${JSON.stringify(field)});
    el.focus(); el.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape', bubbles: true})); })()`);
  await rendered();

  /* an edit is sent on commit only (Enter/Tab/blur), never per keystroke */
  await cdp.eval(`document.getElementById(${JSON.stringify(field)}).focus()`);
  await rendered();
  const preType = await S('__xpp.sentCount()');
  for (const text of ['0', '0.3', '0.35']) {
    await cdp.eval(`(() => { const el = document.getElementById(${JSON.stringify(field)});
      el.value = ${JSON.stringify(text)}; el.dispatchEvent(new Event('input', {bubbles: true})); })()`);
    await rendered();
  }
  const whileTyping = await S(`__xpp.sentFrom(${preType})`);
  check('typing into a field sends nothing while it types', whileTyping.length === 0, JSON.stringify(whileTyping));
  await cdp.eval(`document.getElementById(${JSON.stringify(field)}).blur()`);
  await until(`!s.busy && ${iappIs(0.35)}`, "the blur's set");
  const typedSent = (await S(`__xpp.sentFrom(${preType})`)).filter(c => c.cmd === 'set');
  check('blurring after typing sends exactly one set, with the last text',
    typedSent.length === 1 && typedSent[0].text === '0.35', JSON.stringify(typedSent));

  /* Go sends no set of its own: the core has the value already, and the run uses it */
  const preGo = await S('__xpp.sentCount()');
  const n0go = await S('s.seriesCount');
  await key('i');
  await until("s.ask && s.ask.kind === 'menu'", 'Initialconds menu');
  await key('g');
  await until(`s.seriesCount > ${n0go} && !s.busy`, 'Go integrated');
  const goSent = await S(`__xpp.sentFrom(${preGo})`);
  check('Go sends the run only: no set rides along with it (W106)',
    !goSent.some(c => c.cmd === 'set') && (await S(iappIs(0.35))), JSON.stringify(goSent));

  /* there is no undo (GitHub #110): Reset puts a field's model-file value back, sent at once */
  check('there is no Undo button in the panel', !(await cdp.eval(`[...document.querySelectorAll('.values-header button')].some(b => b.textContent === 'Undo')`)));
  const preReset = await S('__xpp.sentCount()');
  await cdp.eval(`document.getElementById(${JSON.stringify(field)}).closest('.value-field').querySelector('.value-reset').click()`);
  check('Reset sends the model file\'s value at once, and the core has it',
    await until(`!s.busy && ${iappIs(before)}`, 'reset applied')
    && (await S(`__xpp.sentFrom(${preReset})`)).filter(c => c.cmd === 'set').length === 1,
    JSON.stringify(await S(`__xpp.sentFrom(${preReset})`)));
  /* clicking the plot (view-only, no compute) sends nothing */
  const plotCorner = await cdp.eval(`(() => { const r = document.querySelector('.plot-host').getBoundingClientRect();
    return {x: r.left + 6, y: r.top + 6}; })()`);
  const preClick = await S('__xpp.sentCount()');
  await mouse('mousePressed', plotCorner.x, plotCorner.y, {button: 'left', buttons: 1, clickCount: 1});
  await mouse('mouseReleased', plotCorner.x, plotCorner.y, {button: 'left', buttons: 0, clickCount: 1});
  await rendered();
  check('a click on the plot host sends nothing: it is view-only', (await S('__xpp.sentCount()')) === preClick);


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
    await until(`document.querySelector('.slider-picker-picked')?.textContent.includes('0.05')
      && document.querySelector('.slider-dialog-fields input')?.value === '0'`, 'picked slider and its fields'),
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

  /* a slider moved by the keyboard: arrow keys on the range track send
     each value as a set (W106), and nothing runs: no new series until Go */
  const preSlide = await S('__xpp.sentCount()');
  await cdp.eval(`document.getElementById('slider-range-${sid}').focus()`);
  await key('ArrowRight');
  await key('ArrowRight');
  await until(`!s.busy && !s.values.inflight.length && __xpp.sentCount() > ${preSlide}`, 'slider sent');
  const slid = await S(`__xpp.sentFrom(${preSlide})`);
  check('a slider moved by the keyboard sends its values as sets, and runs nothing',
    slid.length >= 1 && slid.every(c => c.cmd === 'set') && (await S('s.seriesCount')) === n0,
    JSON.stringify([slid, await S('s.seriesCount')]));
  const slidTo = Number(slid[slid.length - 1].text);
  check('the core has the slider\'s last value', await S(iappIs(slidTo)), String(slidTo));
  /* the keys go to the plot: on the focused range track a letter is not a hotkey */
  await focusPlot();
  await key('i');
  await until("s.ask && s.ask.kind === 'menu'", 'Initialconds menu (slider)');
  await key('g');
  check('Go integrates with it: a new series arrives',
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
  await metrics({width: 1280, height: 860, deviceScaleFactor: 1, mobile: false});
  await rendered();
  let g = await gridRows();
  check('3 sliders sit in one row at 1280px wide', g.count === 3 && g.rows === 1, JSON.stringify(g));
  await metrics({width: 800, height: 860, deviceScaleFactor: 1, mobile: false});
  await rendered();
  g = await gridRows();
  check('3 sliders wrap 2 per row on a tablet (48rem)', g.count === 3 && g.rows === 2, JSON.stringify(g));
  await metrics({width: 500, height: 860, deviceScaleFactor: 1, mobile: false});
  await rendered();
  g = await gridRows();
  check('3 sliders stack one per row on a phone', g.count === 3 && g.rows === 3, JSON.stringify(g));
  await metrics({width: 1400, height: 900, deviceScaleFactor: 1, mobile: false});
  await rendered();
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

/* W135, desktop/sliders: each Open takes the model's own @ slider options. */
async function sliderModelSwitch(dir) {
  for (const [model, preset, expected] of [
    ['lecar', '@ s1=iapp,slo1=-1,shi1=2', 'iapp'],
    ['vanderpol', '@ s1=x,slo1=-3,shi1=3', 'x'],
  ]) {
    const file = `w135-${model}.ode`;
    fs.writeFileSync(path.join(dir, file), preset + '\n' + fs.readFileSync(path.join(path.dirname(ODE), model + '.ode'), 'utf8'));
    await cdp.eval(`__xpp.send({cmd: 'open', file: ${JSON.stringify(file)}})`);
    await until("s.ask?.kind === 'choice'", 'save before Open');
    await cdp.eval("__xpp.send({cmd: 'answer', id: __xpp.state().ask.id, key: 'd'})");
    await until(`!s.busy && !s.ask && s.hello.file === ${JSON.stringify(file.replace('.ode', '.odex'))}`, 'model opened');
    check(`W135: Open model ${model} shows its own sliders`,
      await S(`s.values.sliders.length === 1 && s.values.sliders[0].name === ${JSON.stringify(expected)}
        && s.core.sliders.filter(d => d.name).length === 1`), JSON.stringify(await S('s.values.sliders')));
  }
}

/* the data table (docs/ui-v2.md T10): open it, scroll to row 500 by
   scrolling and by keyboard, check its values against output.dat, Get,
   CSV export, and Tab reachability of every button */
async function dataTable(want, dir) {
  await metrics({width: 1400, height: 900, deviceScaleFactor: 1, mobile: false});
  await rendered();
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

  /* CSV export: the core writes data.csv itself (browser op write, what:
     table, format: csv; core/browse_data.cpp data_write), then the page
     offers it as a download (docs/roadmap.md W66) */
  const dataCsvPath = path.join(dir, 'data.csv');
  fs.rmSync(dataCsvPath, {force: true});
  await cdp.eval(`document.querySelector('.table-header .small').click()`);
  check('Export CSV offers data.csv, written by the core',
    await until("s.files.offered && s.files.offered.name === 'data.csv' && !s.busy", 'csv offered'));
  await waitFile(dataCsvPath);
  let csv = fs.existsSync(dataCsvPath) ? fs.readFileSync(dataCsvPath, 'utf8').trim().split('\n') : [];
  if (csv[0]?.startsWith('# seed ')) csv = csv.slice(1); /* the run's seed, when it used one (docs/protocol.md) */
  const r500 = csv[501] ? csv[501].split(',').map(Number) : [];
  check('the exported CSV has the header and all 601 rows, row 500 as in output.dat',
    csv.length === 602 && r500.length >= 3 && r500.every((v, k) => Math.abs(v - want[500][k]) <= 1e-7 * Math.abs(want[500][k]) + 1e-30),
    `${csv.length} lines, row 500: ${csv[501]} vs ${want[500]}`);

  /* the plot's own CSV export (PlotView.tsx), the same core write with
     what: plot instead of table (docs/protocol.md "Saving data") */
  const curvesCsvPath = path.join(dir, 'xpp-curves.csv');
  fs.rmSync(curvesCsvPath, {force: true});
  await cdp.eval(`document.querySelector('.plot-tools button[title^="Save the plotted numbers"]').click()`);
  check('the plot\'s CSV button offers xpp-curves.csv, written by the core',
    await until("s.files.offered && s.files.offered.name === 'xpp-curves.csv' && !s.busy", 'plot csv offered'));
  await waitFile(curvesCsvPath);
  check('xpp-curves.csv is not empty',
    fs.existsSync(curvesCsvPath) && fs.statSync(curvesCsvPath).size > 0);

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
   `action`, `equilibrium`, its window's Import key): equations, source with a comment
   action, equilibrium with Import, and Tab reachability. lecar.odex's own
   tutorial ("To set parameters click on the asterisks") is the model with
   comment actions the task asks for: six `"..{name=value,...}"` lines, each
   an X11 "Action view" button. This picks the second one ({gk=0}) rather
   than the first ({total=100,iapp=.1}), so it leaves TOTAL (and so the
   601-row runs later windows() and prompts() still expect) alone. */
async function textViews() {
  await metrics({width: 1400, height: 900, deviceScaleFactor: 1, mobile: false});
  await rendered();
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
    JSON.stringify(await S(`({equilibrium: s.text.equilibrium, busy: s.busy, ask: s.ask,
      sent: __xpp.sent().slice(-4), log: __xpp.log().slice(-6)})`)));
  const eq = await S('s.text.equilibrium');
  if (!eq) return;   /* reported above; the checks below need it */
  check('the view shows the type and the values (six significant digits)',
    await cdp.eval(`document.querySelector('.text-equilibrium .eq-type').textContent === ${JSON.stringify(eq.type)}
      && document.querySelectorAll('.text-equilibrium .eq-values tbody tr').length >= 2`));
  check('and its eigenvalues, one row per variable',
    eq.eigenvalues && eq.eigenvalues.length === 2 && await cdp.eval(`[...document.querySelectorAll('.text-equilibrium .eq-values')]
      .find(t => t.querySelector('caption').textContent === 'Eigenvalues').querySelectorAll('tbody tr').length === 2`),
    JSON.stringify(eq.eigenvalues));
  await cdp.eval(`[...document.querySelectorAll('.text-tools button')].find(b => b.textContent === 'Import').click()`);
  const wantIcs = eq.values.map(([, v]) => v);
  check('Import (the equilibrium window key i) makes the equilibrium the initial conditions',
    await until(`(() => { const ics = s.core.ics.map(p => p[1]);
      return ics.length === ${wantIcs.length} && ics.every((v, i) => Math.abs(v - (${JSON.stringify(wantIcs)})[i]) < 1e-6); })() && !s.busy`,
      'import'), JSON.stringify({ics: await S('s.core.ics'), want: wantIcs}));

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

/* UX-001 (GitHub #76): at a narrow viewport the values panel is a full-screen sheet, and Escape
   also closes it (ValuesPanel.tsx) -- but a field with something of its own to drop (a
   half-typed or core-refused draft, Field.tsx) must get the key first: the first Escape belongs
   to the field, and only a second one (nothing left to drop) closes the sheet. */
async function valuesNarrowEscape() {
  await editField('par', 'iapp', '0.05');
  await metrics({width: 546, height: 900, deviceScaleFactor: 1, mobile: false});
  await rendered();
  await cdp.eval(`document.querySelector('.values-toggle').click()`);
  await until('s.valuesOpen', 'the sheet opens (546px)');
  await rendered();
  await cdp.eval(`${fieldOf('par', 'iapp')}.querySelector('input').focus()`);
  await typeIntoField('par', 'iapp', '1e');
  await key('Enter');
  await rendered();
  let box = await fieldState('par', 'iapp');
  check('UX-001: Enter on "1e" refuses it and keeps the box focused, marked',
    box.invalid === 'true' && box.value === '1e' && box.focused && box.message === '"1e" needs an exponent\'s digits',
    JSON.stringify(box));
  await key('Escape');
  await rendered();
  box = await fieldState('par', 'iapp');
  const open1 = await cdp.eval(`document.querySelector('.values-panel').classList.contains('open')`);
  check('UX-001: the first Escape belongs to the field: it drops the draft and the sheet stays open',
    box.invalid === null && close6(Number(box.value), 0.05) && open1, JSON.stringify({box, open1}));
  await key('Escape');
  await rendered();
  const open2 = await cdp.eval(`document.querySelector('.values-panel').classList.contains('open')`);
  check('UX-001: a second Escape, with nothing left to drop, closes the sheet',
    open2 === false && !(await S('s.valuesOpen')), JSON.stringify({open2}));
  await metrics({width: 1280, height: 860, deviceScaleFactor: 1, mobile: false});
  await rendered();
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
  await until(`w.viewport.x && w.viewport.x.max - w.viewport.x.min < ${width(w0)}`, '+ settled');
  const z = await S('w.viewport');
  check('+ zooms in', z.x && width(z.x) < width(w0), JSON.stringify(z));
  await key('ArrowRight');
  await until(`w.viewport.x && w.viewport.x.min > ${z.x.min}`, 'arrow pan settled');
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
   examples/ode/wcring.odex (a ring of 20 coupled neurons, u0..u19) is a real
   array model, so the grid actually has columns worth scrolling through. */
async function aplotView() {
  await metrics({width: 1280, height: 860, deviceScaleFactor: 1, mobile: false});
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
  await rendered();
  const editFields = ['20', '0', '10', '1', null, null, '0', '1']; /* NCols, Row1, NRows, RowSkip, -, -, Autoplot, ColSkip */
  for (let i = 0; i < editFields.length; i++) {
    if (editFields[i] === null) continue;
    await cdp.eval(`(() => { const el = document.querySelectorAll('[role=dialog] input')[${i}];
      el.value = ${JSON.stringify(editFields[i])}; el.dispatchEvent(new Event('input', {bubbles: true})); })()`);
    await rendered();
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
  const sentBefore = await cdp.eval('__xpp.sentCount()');
  await cdp.eval(`(() => { const s = document.querySelector('.aplot-map select');
    s.value = 'xpp'; s.dispatchEvent(new Event('change', {bubbles: true})); })()`);
  check('the colour map switch changes the store\'s map, and sends nothing to the core',
    await S("s.aplot.colorMap === 'xpp'") && await cdp.eval('__xpp.sentCount()') === sentBefore);

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
  const sent1 = await cdp.eval('__xpp.sentCount()');
  await key('PageUp');
  check('PageUp (the keyboard) scrolls it too, toward earlier rows',
    await until('!s.busy', 'key scroll') && await cdp.eval('__xpp.sentCount()') > sent1
    && await cdp.eval(`__xpp.sent().some(c => c.cmd === 'aplot' && c.op === 'scroll')`));

  /* Redraw, then Back leaves the core's window alive */
  await cdp.eval(`[...document.querySelectorAll('.aplot-tools button')].find(b => b.textContent === 'Redraw').click()`);
  check('Redraw asks for a fresh picture', await until('!s.busy', 'redraw')
    && await cdp.eval(`__xpp.sent().some(c => c.cmd === 'key' && c.win === 'aplot' && c.key === 'd')`));
  await cdp.eval(`document.querySelector('.aplot-back').click()`);
  check('Back closes the panel; the core\'s array plot window stays alive',
    await until('!s.aplot.open && s.aplot.windowOpen', 'close panel'));

  /* 390x844: a full-screen sheet, no sideways scroll, 44px targets (SCOPE) */
  await cdp.eval(`document.querySelector('.aplot-toggle').click()`);
  check('reopening it (the window already exists) shows the panel again, no Edit form',
    await until('s.aplot.open && !s.ask', 'reopen'));
  await metrics({width: 390, height: 844, deviceScaleFactor: 2, mobile: true});
  await cdp.send('Emulation.setTouchEmulationEnabled', {enabled: true, maxTouchPoints: 5});
  await rendered();
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
  await metrics({width: 1280, height: 860, deviceScaleFactor: 1, mobile: false});
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
  const shown0 = await displays();
  await key('+');
  await displayTold(shown0, 'the zoom told'); /* before New window (macos-ui, W93) */
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
  const zoom2 = await displays();
  await key('-');
  await displayTold(zoom2, 'the zoom told'); /* before tab 1 is clicked (macos-ui, W93) */
  const z2 = await S('w.viewport');
  check('window 2 zoomed on its own', !!z2.x && JSON.stringify(z2) !== JSON.stringify(z1), JSON.stringify(z2));

  await cdp.eval(`document.getElementById('plot-tab-1').click()`);
  check('clicking tab 1 shows window 1 and makes it the core\'s active window',
    await until('s.plots.active === 1 && !s.busy && s.core.win === 1', 'tab 1'), JSON.stringify(await S('[s.plots.active, s.core.win]')));
  const back = await S('w.viewport'), p1 = await P();
  check("tab 1 keeps its zoom", JSON.stringify(back) === JSON.stringify(z1)
    && Math.abs(p1.x.min - z1.x.min) < 1e-9 && Math.abs(p1.x.max - z1.x.max) < 1e-9, JSON.stringify([back, p1 && p1.x]));
  check('its chart is the one shown', p1 && p1.curves[0].label === t1 && p1.width > 200, JSON.stringify(p1 && [p1.curves, p1.width]));

  /* what a failing tab check saw: busy, the page's and the core's window, the focus, the last
     commands sent and actions taken (W93: macos-ui lost ArrowLeft here) */
  const tabState = async () => JSON.stringify(await cdp.eval(`(() => { const s = __xpp.state();
    return {busy: s.busy, active: s.plots.active, core: s.core.win, ask: s.ask && s.ask.kind, focus: document.activeElement.id,
      sent: __xpp.sent().slice(-4), actions: __xpp.actions().slice(-10)}; })()`));
  await cdp.eval(`document.getElementById('plot-tab-1').focus()`);
  await key('ArrowRight');
  check('ArrowRight on the tabs moves to window 2, focus with it',
    await until(`s.plots.active === 2 && document.activeElement.id === 'plot-tab-2'`, 'arrow right'));
  check('tab 2 kept its zoom', JSON.stringify(await S('w.viewport')) === JSON.stringify(z2));
  const beforeLeft = await tabState();
  await key('ArrowLeft');
  check('ArrowLeft back to window 1', await until(`s.plots.active === 1 && document.activeElement.id === 'plot-tab-1'`, 'arrow left'),
    `before: ${beforeLeft} after: ${await tabState()}`);
  await key('End');
  check('End to the last tab', await until(`s.plots.active === 2 && document.activeElement.id === 'plot-tab-2'`, 'End'), await tabState());
  check('the tab key goes back to the core', await until('!s.busy && s.core.win === 2', 'core win 2'), await tabState());
  /* two tab keys in one turn of the event loop: the second always arrives
     while the first one's click is still the core's (busy), so it is held
     for the idle, never dropped (a slow runner lost End this way) */
  const sentBefore = await cdp.eval('__xpp.sentCount()');
  const afterKeys = await cdp.eval(`(() => { const t = document.getElementById('plot-tab-2');
    for (const key of ['ArrowLeft', 'End']) t.dispatchEvent(new KeyboardEvent('keydown', {key, bubbles: true, cancelable: true}));
    const s = __xpp.state(); return [s.busy, s.plots.active]; })()`);
  check('two tab keys at once: the first is shown and sent, the core busy with it',
    afterKeys[0] === true && afterKeys[1] === 1, JSON.stringify(afterKeys));
  const heldOk = await until('!s.busy && s.plots.active === 2 && s.core.win === 2', 'held tab');
  const clicks = await cdp.eval(`__xpp.sentFrom(${sentBefore}).filter(c => c.cmd === 'click').map(c => c.win)`);
  check('the second, sent while the first was busy, is held and applied at its idle',
    heldOk && JSON.stringify(clicks) === '[1,2]', JSON.stringify([await S('[s.busy, s.plots.active, s.core.win]'), clicks]));
  check('... and its tab has the focus: the tab it left, disabled while the click ran, did not take it away (W93)',
    await until(`!s.busy && document.activeElement.id === 'plot-tab-2'`, 'held tab focused'), await tabState());

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

  await metrics({width: 390, height: 844, deviceScaleFactor: 2, mobile: true});
  await rendered();
  const scroll = await cdp.eval(`({doc: document.documentElement.scrollWidth, w: innerWidth,
    tabs: [...document.querySelectorAll('[role=tab]')].every(t => t.getBoundingClientRect().right <= innerWidth)})`);
  check('390 px with two tabs: no sideways scroll, the tabs fit', scroll.doc <= scroll.w && scroll.tabs, JSON.stringify(scroll));
  await metrics({width: 1280, height: 860, deviceScaleFactor: 1, mobile: false});
  await rendered();

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
  check('the page connects and asks for the plot as data', await until('s.hello && s.seriesCount >= 1 && !s.busy', 'hello'),
    JSON.stringify(await S('({hello: !!s.hello, seriesCount: s.seriesCount, busy: s.busy})')));
  await key('i');
  await until("s.ask && s.ask.kind === 'menu'", 'menu');
  await key('g');
  check('I, G integrates a series', await until('w.series && w.series.rows === 601 && !s.busy', 'series'),
    JSON.stringify(await S('({rows: w.series ? w.series.rows : null, busy: s.busy, seriesCount: s.seriesCount})')));

  const a = await area(), cx = a.x + a.w / 2, cy = a.y + a.h / 2;
  await mouse('mouseMoved', cx, cy);
  await mouse('mouseWheel', cx, cy, {deltaX: 0, deltaY: -240});
  await until('w.viewport.x', 'wheel');
  const zoomed = await P();
  check('zoom by wheel', zoomed && zoomed.x && zoomed.y, JSON.stringify(zoomed));

  await cdp.eval(`document.querySelector('.plot-tools button[title^="Make this zoom"]').click()`);
  const close = (a2, b) => Math.abs(a2 - b) < 1e-5 * Math.max(1, Math.abs(b));
  /* wait for the actual assertion (the core's axes caught up with the
     zoomed range), not the weaker "some view landed and busy dropped": a
     busy flag can clear a tick before state.view's own update arrives, and
     reading right then is the pre-W20 mistake again (W40, GitHub #83, a
     macos-ui rerun failed this exact check with the old view still there) */
  const closeExpr = (actual, want) => `Math.abs((${actual}) - (${want})) < 1e-5 * Math.max(1, Math.abs(${want}))`;
  const viewClose = `s.core.view && !s.busy
    && ${closeExpr('s.core.view.xlo', zoomed.x.min)} && ${closeExpr('s.core.view.xhi', zoomed.x.max)}
    && ${closeExpr('s.core.view.ylo', zoomed.y.min)} && ${closeExpr('s.core.view.yhi', zoomed.y.max)}`;
  const viewSettled = await until(viewClose, 'view settled to the zoom');
  const view = await S('s.core.view');
  check("Use this view: the core's axes (state.view) equal the zoomed ranges",
    viewSettled && view && close(view.xlo, zoomed.x.min) && close(view.xhi, zoomed.x.max) && close(view.ylo, zoomed.y.min) && close(view.yhi, zoomed.y.max),
    JSON.stringify([view, zoomed.x, zoomed.y, await S('({busy: s.busy})')]));
  const infoClose = `w.info
    && ${closeExpr('w.info.xlo', zoomed.x.min)} && ${closeExpr('w.info.xhi', zoomed.x.max)}
    && ${closeExpr('w.info.ylo', zoomed.y.min)} && ${closeExpr('w.info.yhi', zoomed.y.max)}`;
  const infoSettled = await until(infoClose, "plots' own axes settled to the zoom");
  const info = await S('w.info');
  check('... "plots" agrees (the window\'s own axes)',
    infoSettled && close(info.xlo, zoomed.x.min) && close(info.xhi, zoomed.x.max) && close(info.ylo, zoomed.y.min) && close(info.yhi, zoomed.y.max),
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
    await until(`!!document.querySelector('.plot-view:not([hidden]) .plot-host .plot-fit')`, 'corner fit button present'),
    JSON.stringify(await S('({busy: s.busy, rows: w.series ? w.series.rows : null})')));
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
  const pans0 = await displays();
  for (let st = 0; st < 15; st++) await key('ArrowRight');
  await until('w.viewport.x', 'panned away again');
  await displayTold(pans0, 'the pan told'); /* before Fit (macos-ui, W93) */
  await cdp.eval(`document.querySelector('.plot-view:not([hidden]) .plot-host .plot-fit').click()`);
  check('Fit again with the axes already fitted still brings the data back (the page drops its pan)',
    await until('w.viewport.x === null && w.viewport.y === null && !s.busy', 'second corner fit'),
    JSON.stringify(await S('w.viewport')));
}

/* the 3D plot host's box on screen (docs/ui-v2.md T14): Plot3DView.tsx's
   canvas fills it, and drag/key events go to the host div itself */
const rawArea3d = () => cdp.eval(`(() => { const r = document.querySelector('.plot-view:not([hidden]) .plot-host').getBoundingClientRect();
  return {x: r.left, y: r.top, w: r.width, h: r.height}; })()`);
const area3d = () => settled(rawArea3d);
/* a turn is the key 3 (3d-params) and its form's answer (W60) */
const sentView3d = mark => cdp.eval(`__xpp.sentFrom(${mark}).filter(c => c.cmd === 'key' && c.key === '3' && !c.win).length`);

/* 3D plots (docs/ui-v2.md T14, GitHub issue #18): lorenz.odex sets axes=3d
   and phi=60 (theta stays the default 45) and runnow=1, which the core
   runs as its own command cycle after loading (servercheck.py's
   check_view3d has the same two-cycle wait); the plot never draws with
   three.js, just a canvas (plot/render3d.ts) from the projection
   (plot/project3d.ts). */
/* W99: the Values panel's boundary-conditions section is there only for a model that defines
   boundary conditions, and starts collapsed */
const bcSection = (want) => async () => {
  await rendered();
  const sec = await cdp.eval(`(() => { const e = document.querySelector('[data-section="bc"]');
    return e ? {folded: e.classList.contains('folded'), fields: e.querySelectorAll('.value-field').length} : null; })()`);
  const n = await S('(s.core.bcs || []).length');
  if (want) {
    check('a model with b lines shows the boundary-conditions section, collapsed, with its own conditions',
      !!sec && sec.folded && sec.fields === want && n === want, JSON.stringify([sec, n]));
  } else {
    check('a model with no boundary conditions has no boundary-conditions section', sec === null && n === 0,
      JSON.stringify([sec, n]));
  }
};

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

  /* drawn once the run's data reached the chart (read at once, it was
     still null on macOS CI, and the drag below then threw); wait for the
     curve's own points too (W40, GitHub #83: a macos-ui rerun saw the box
     before the curve, box.length === 8 but curves[0].points still 0) */
  await until('(() => { const g = __xpp.plot(); return !!g && !!g.box && g.box.length === 8 '
    + '&& g.curves.length >= 1 && g.curves[0].points > 0; })()', 'projection drawn');
  const before = await P();
  check('it draws a projection: the box\'s 8 corners, at least one curve with points',
    before && before.box.length === 8 && before.curves.length >= 1 && before.curves[0].points > 0, JSON.stringify(before));

  /* a drag on the focused plot turns it locally at once */
  const a = await area3d(), cx = a.x + a.w / 2, cy = a.y + a.h / 2;
  const sentBefore = await S('__xpp.sentCount()');
  const steps = 12;
  await mouse('mousePressed', cx, cy, {button: 'left', buttons: 1, clickCount: 1});
  for (let s = 1; s <= steps; s++) await mouse('mouseMoved', cx + 3 * s, cy - 2 * s, {button: 'left', buttons: 1});
  const dragged = await S('w.view3d');
  check('drag: theta/phi changed at once, by the drag\'s pixels (core/many_pops.c rotate3dcheck: 1 pixel, 1 degree, subtracted)',
    dragged && dragged.theta === angles0.theta - 3 * steps && dragged.phi === angles0.phi + 2 * steps,
    JSON.stringify([angles0, dragged]));
  const during = await rawP(); /* mid-drag: at once */
  check('... and the drawn projection changed too (still dragging, no round trip needed)',
    during && JSON.stringify(during.box) !== JSON.stringify(before.box), '');
  const sentDuring = await sentView3d(sentBefore);
  check(`throttled: ${steps} pointer moves sent far fewer 3d-params turns (${sentDuring})`,
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

  /* T30: fit_window() handles ThreeDFlag (core/graf_par.cpp), so 3D plots
     get the corner button too; it goes through the core like the main
     plot's, there being no client-side zoom of a 3D plot to fit locally */
  check('the corner Fit sits over the 3D plot too (Window/Fit fits its box the same way)',
    await cdp.eval(`!!document.querySelector('.plot-view:not([hidden]) .plot-host .plot-fit')`));
  const sentBeforeFit = await cdp.eval('__xpp.sentCount()');
  await cdp.eval(`document.querySelector('.plot-view:not([hidden]) .plot-host .plot-fit').click()`);
  check("the corner Fit sends the core's Window/Fit (w, f) as keys, as the 2D toolbar's Fit does",
    await until('!s.busy', 'fit settled')
      && (await cdp.eval(`__xpp.sentFrom(${sentBeforeFit}).some(c => c.cmd === 'key' && (c.key === 'w' || c.key === 'f'))`)),
    JSON.stringify(await cdp.eval(`__xpp.sentFrom(${sentBeforeFit})`)));
}

async function touch(type, points) {
  await cdp.send('Input.dispatchTouchEvent', {type, touchPoints: points.map((p, i) => ({x: p.x, y: p.y, id: i}))});
  await rendered();
}

async function phone() {
  await metrics({width: 390, height: 844, deviceScaleFactor: 2, mobile: true});
  await cdp.send('Emulation.setTouchEmulationEnabled', {enabled: true, maxTouchPoints: 5});
  /* reduced motion too, on every platform: the sheets then close with no
     transition, which is when their focus went to <body> instead of back to
     the toggle (W18: CI's Windows Server runner has reduced motion on) */
  await cdp.send('Emulation.setEmulatedMedia', {features: [{name: 'pointer', value: 'coarse'}, {name: 'hover', value: 'none'},
    {name: 'prefers-reduced-motion', value: 'reduce'}]}).catch(() => {});
  await rendered();
  const scroll = await cdp.eval(`({doc: document.documentElement.scrollWidth, body: document.body.scrollWidth, w: innerWidth})`);
  check('390x844: no sideways scroll', scroll.doc <= scroll.w && scroll.body <= scroll.w, JSON.stringify(scroll));
  const plot = await area();
  check('the plot fills the width', plot.w > 250 && plot.x + plot.w <= 390, JSON.stringify(plot));
  /* W83: the reserved progress/Stop slot (narrower at this breakpoint, theme.css)
     still fits with no overflow, and the bar keeps its fixed height */
  const statusBar = await cdp.eval(`(() => { const b = document.querySelector('.status-bar').getBoundingClientRect(),
      run = document.querySelector('.status-run').getBoundingClientRect();
    return {barHeight: Math.round(b.height), barRight: Math.round(b.right), runRight: Math.round(run.right)}; })()`);
  check('390x844: the status bar has a fixed height and its reserved progress/Stop slot fits with no overflow',
    statusBar.barHeight > 0 && statusBar.barHeight < 40 && statusBar.barRight <= 391 && statusBar.runRight <= 391,
    JSON.stringify(statusBar));
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
  /* ... drawn: the points are placed on the view the chart shows (W20) */
  await until(`(() => { const r = __xpp.plot(); return !!r && r.x.max - r.x.min > 1.5 * ${width(z2.x)}; })()`, 'reset drawn');
  /* a point well inside the plot */
  const box = await area();
  let row = 100, pt = await screenOf(0, row);
  /* ... and not under anything drawn over the plot (the corner Fit, T30) */
  const bare = q => cdp.eval(`(() => { const e = document.elementFromPoint(${q.x}, ${q.y}); return !!e && e.classList.contains('u-over'); })()`);
  while (row < 600 && (pt.x < box.x + 30 || pt.x > box.x + box.w - 30 || pt.y < box.y + 30 || pt.y > box.y + box.h - 30 || !(await bare(pt)))) {
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
const lastFileAnswer = async () => (await cdp.eval('__xpp.sent()')).filter(c => c.cmd === 'answer' && c.file).pop();
const focusPlot = () => cdp.eval(`document.querySelector('.plot-view:not([hidden]) .plot-host').focus()`);

/** a key, then the key that answers the menu it opens */
/* The page takes no key or click while a command runs (T11). A zoom, a pan
   or the legend's runs toggle tells the core through W65's display command,
   sent a moment later (AUTO's view: auto op display); the next key or click
   must wait until it has been sent and done, else it is lost (W93: macos-ui
   and linux-ui lost them). displays() marks the commands sent before the action,
   displayTold(mark) waits for one after it. */
const displays = () => cdp.eval('__xpp.sentCount()');
const displayTold = (mark, what = 'the display told') =>
  until(`__xpp.sentFrom(${mark}).some(c => c.cmd === 'display' || (c.cmd === 'auto' && c.op === 'display')) && !s.busy`, what);

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
  await rendered();
  await until('!s.busy', 'idle');
  const lists = await S('s.hello.lists');

  /* Viewaxes/2D: the axis fields are selects of hello.lists[0]; picking another variable plots it */
  const pickX = async name => {
    await cdp.eval(`(() => { const s = document.querySelector('[role=dialog] select'); s.value = ${JSON.stringify(name)};
      s.dispatchEvent(new Event('change', {bubbles: true})); })()`);
    await rendered();
    await key('Enter');
  };
  await focusPlot();
  await menuKeys('v', '2');
  check('Viewaxes/2D opens its form', await until("s.ask && s.ask.kind === 'form' && document.querySelector('[role=dialog] select')", 'form'));
  /* the dialog can mount a frame before the form's own field takes the focus
     (a useEffect, not the same paint): wait for that too, rather than
     racing it right after the dialog itself appears (W40, GitHub #83) */
  await until(`document.activeElement === document.querySelector('[role=dialog] select')`, 'X-axis select focused');
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

  /* T35d/T31: the form's fields take what the core says they take (the ask's kinds): a keystroke
     a number field never takes (a letter) is refused outright and never lands (T35d); one left
     half-typed ("1e") is marked with a message, OK is disabled and Enter answers nothing */
  await focusPlot();
  await menuKeys('v', '2');
  await until("s.ask && s.ask.kind === 'form' && document.querySelector('[role=dialog] input')", 'form');
  const formId = await S('s.ask.id');
  const answers = async () => (await cdp.eval('__xpp.sent()')).filter(c => c.cmd === 'answer').length;
  const answers0 = await answers();
  const numberBox = `document.querySelector('[role=dialog] input[data-kind=number]')`;
  const numberBoxState = () => cdp.eval(`(() => { const i = ${numberBox}, m = i.closest('label').querySelector('.field-error');
    return {invalid: i.getAttribute('aria-invalid'), value: i.value, message: m && m.textContent, mode: i.inputMode}; })()`);
  const before = await numberBoxState();
  await cdp.eval(`(() => { const i = ${numberBox}; i.focus(); i.value = 'abc'; i.dispatchEvent(new Event('input', {bubbles: true})); })()`);
  await rendered();
  const keystroke = await numberBoxState();
  check('T35d: a letter a number field never takes is refused outright: the box keeps its own text, unmarked',
    keystroke.value === before.value && keystroke.invalid === null && !!keystroke.message, JSON.stringify(keystroke));
  await cdp.eval(`(() => { const i = ${numberBox}; i.value = '1e'; i.dispatchEvent(new Event('input', {bubbles: true})); })()`);
  await rendered();
  await key('Enter');
  await rendered();
  const refused = await cdp.eval(`(() => { const i = ${numberBox}, m = i.closest('label').querySelector('.field-error');
    return {invalid: i.getAttribute('aria-invalid'), message: m && m.textContent, mode: i.inputMode,
      ok: document.querySelector('[role=dialog] .dialog-actions .primary').disabled}; })()`);
  check('T31: "1e" left half-typed in a number field (Viewaxes/2D Xmin) is refused: marked, says what is missing, OK disabled, Enter answers nothing',
    refused.invalid === 'true' && refused.message === '"1e" needs an exponent\'s digits' && refused.mode === 'decimal'
    && refused.ok === true && (await answers()) === answers0 && await S(`!!s.ask && s.ask.id === ${formId}`), JSON.stringify(refused));
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
  await rendered();
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
  /* the area once the pick bar has settled it (read at once, it was a
     pixel off on macOS CI) */
  let v0 = await S('s.core.view'), p = await steadyP(), a = await area();
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
  a = await area();
  p = await steadyP();
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
  await until('__xpp.sent().slice(-3).filter(c => c.cmd === "answer").map(c => c.what).join() === "down,move,up"', 'arrow drag answers');
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
  check('its first box has the focus', await until(`document.activeElement.type === 'checkbox'`, 'checklist focus'));
  await cdp.eval(`document.querySelector('[role=dialog] input[type=checkbox]').click()`);
  await rendered();
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
   tools/autocheck.py's Diagram does: what the store must equal, for view `view` */
function rebuildDiagram(events, view = 0) {
  const pts = [];
  let labels = [];
  for (const e of events) {
    if (e.view !== view) continue;
    if (e.op === 'reset') {
      pts.length = Math.min(pts.length, e.keep);
      labels = labels.filter(l => l.point < e.keep);
    } else if (e.op === 'add') {
      if (e.from > pts.length) throw new Error(`diagram add from ${e.from} with ${pts.length} held`);
      pts.length = e.from;
      labels = labels.filter(l => l.point < e.from);
      for (const r of e.runs) {
        for (const [i, lab, sym] of r.lab || []) labels.push({point: pts.length + i, lab, sym});
        r.x.forEach((x, i) => pts.push({x, y: r.y[i], y2: (r.y2 || r.y)[i], br: r.br, pt: r.pt + i, ty: r.ty,
          st: r.stable ? 1 : 0, pe: r.periodic ? 1 : 0, d: r.d, c: r.c, lw: r.lw, f2: r.f2 || 0, nw: i === 0 && r.new ? 1 : 0, fr: i === 0 && r.from ? r.from : 0}));
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

const rawDG = () => cdp.eval('__xpp.diagram()');
const DG = () => settled(rawDG);
/* the diagram's state, with its active view's axes, points, labels and viewport at its top (W50) */
const DS = expr => cdp.eval(`(() => { const s = __xpp.state(), d = Object.assign({}, s.diagram, ${DV}); return ${expr}; })()`);
const autoButton = k => cdp.eval(`document.querySelector('.auto-tools button[aria-keyshortcuts=${k}]').click()`);
/* the diagram's plotting area and the screen position of (x, y) in it */
const rawAutoArea = () => cdp.eval(`(() => { const r = document.querySelector('.auto-panel .u-over').getBoundingClientRect();
  return {x: r.left, y: r.top, w: r.width, h: r.height}; })()`);
const autoArea = () => settled(rawAutoArea);
/* the screen position of diagram point (x, y), once the view has settled (a
   grab's import just before may still be moving it: the mouse then landed
   on a neighbour, macOS CI) */
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

  /* examples/recordings/lecar_auto.recx: the "hopf" parameter set, its fixed point as the IC */
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
  await cdp.eval(`__xpp.send({cmd: 'key', win: 'equilibrium', key: 'i'})`);
  await until('!s.busy', 'import');
  await key('f');
  await until('!s.busy', 'file menu');
  await key('a');
  check('File/Auto opens the AUTO view', await until('s.diagram.open && s.diagram.shown && dv.axes && !s.busy', 'auto open')
    && await cdp.eval(`!!document.querySelector('.auto-panel .auto-host')`), JSON.stringify(await DS('d.axes')));
  check("the diagram has the focus, so AUTO's keys work", await until(`document.activeElement.closest('.auto-host')`, 'auto focus'));
  const words = await cdp.eval(`[...document.querySelectorAll('.auto-panel button')].map(b => b.textContent.trim())`);
  check('the view has the AUTO buttons, no reDraw (the diagram is always current) and no Stop while idle (A10, T21)',
    ['Parameter', 'Axes', 'Numerics', 'Run', 'Grab', 'Mark values…', 'Clear', 'File', 'Close']
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
     json_state.cpp read_value: "set par iapp: Illegal formula .." */
  await cdp.eval(`__xpp.send({cmd: 'set', kind: 'par', name: 'iapp', text: '%('})`);
  await until(`s.bottom === 'set par iapp: Illegal formula ..'`, 'bottom message');
  check("T26: the core's last message shows in the AUTO strip",
    (await cdp.eval(`document.querySelector('.auto-message')?.textContent`)) === 'set par iapp: Illegal formula ..');
  /* an error opens the error dialog (ErrorDialog.tsx) until OK: close it so it does not sit over a
     later control (it is above the AUTO view too, T26's dialog-backdrop note) */
  await cdp.eval(`document.querySelector('[data-error-ok]')?.click()`);
  await until(`!s.toasts.length`, 'toasts dismissed');

  /* a dialog the view opens is on top of it, not behind (Numerics' form, T22: the page's own on the autosettings data) */
  check("T22: the store holds AUTO's settings (autosettings) before a run",
    await until('s.autoSettings.core && s.autoSettings.core.numerics.nmx > 0 && s.autoSettings.core.pars.length > 1', 'settings'),
    JSON.stringify(await S('s.autoSettings.core')).slice(0, 300));
  const sentNum = await cdp.eval('__xpp.sentCount()');
  await autoButton('N');
  await until(`!!document.querySelector('.auto-settings-dialog[data-settings=auto-numerics]')`, 'numerics form');
  check("T22: Numerics opens the page's form, filled from the settings (Nmax), nothing asked of the core",
    await S(`String(s.autoSettings.core.numerics.nmx) === document.querySelector('.auto-settings-dialog input[data-field=nmx]').value
      && !s.ask && !s.busy`) && (await cdp.eval('__xpp.sentCount()')) === sentNum,
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
  /* T35d: a decimal point never lands in an integer field: refused outright, named, nothing
     commits, so "12.5" can never actually reach the field (the point at issue for T23's message
     below, "must be a whole number", stays reachable only for a text the filter cannot see,
     such as a lone sign) */
  await typeInto('nmx', '12');
  await typeInto('nmx', '12.5');
  const dottedNmx = await fieldState('nmx');
  check('T35d: a decimal point in an integer field (Max points) is refused outright: unmarked, kept at 12',
    dottedNmx.invalid === null && dottedNmx.error === 'Pasted ".5" is not a whole number ("." at character 1)'
    && dottedNmx.ok === false, JSON.stringify(dottedNmx));
  /* T31: a lone sign is on the way to a whole number while typed (fieldIncomplete), so it lands;
     Enter on it keeps the form and its message (the core's, from the event's rules), sends nothing */
  await typeInto('nmx', '-');
  await cdp.eval(`document.querySelector('.auto-settings-dialog input[data-field=nmx]').focus()`);
  await key('Enter');
  await rendered();
  const signNmx = await fieldState('nmx');
  check('T31: Enter on a lone "-" in an integer AUTO Numerics field keeps the form open, marked, and sends nothing',
    signNmx.invalid === 'true' && signNmx.error === 'Nmax must be a whole number of at least 1' && signNmx.ok === true
    && await cdp.eval(`!!document.querySelector('.auto-settings-dialog')`)
    && !(await cdp.eval(`__xpp.sent().some(c => c.cmd === 'auto' && c.op === 'set')`)), JSON.stringify(signNmx));
  await typeInto('nmx', '200');
  await typeInto('ds', '9');
  const bigDs = await fieldState('ds');
  check('T23: a first step larger than DSMAX is refused beside DS, OK disabled',
    bigDs.error === 'Ds must be from Dsmin to Dsmax in size (its sign is the direction)' && bigDs.ok === true, JSON.stringify(bigDs));
  await key('Escape');
  check('T22: Escape cancels it: nothing sent',
    await until(`!document.querySelector('.auto-settings-dialog')`, 'numerics cancelled')
    && !(await cdp.eval(`__xpp.sent().some(c => c.cmd === 'auto' && c.op === 'set')`)));

  /* Run / Steady state */
  await autoButton('R');
  check('Run asks how to start', await until("s.ask && s.ask.kind === 'menu' && s.ask.title === 'Start'", 'start menu'),
    JSON.stringify(await S('s.ask')));
  await menuKey('s');
  check('the steady-state branch arrives', await until('!s.busy && dv.points.x.length > 10', 'steady state', 60000));
  const nSteady = await DS('d.points.x.length');
  const stop = await DS('d.stop');
  check('T21, T23: the status strip says why the run stopped (the core\'s reason), steady states, its branch and point, its points, its last label, the time',
    await until(`document.querySelector('.auto-status').dataset.phase === 'done'`, 'strip done')
    && stop && stop.br === 1 && /^(parameter iapp reached Par (Min|Max)|the norm reached|the branch reached Max points)/.test(stop.text)
    && new RegExp(`^Stopped: ${stop.text.replace(/[()]/g, '\\$&')} · steady states · branch 1, point ${stop.pt} · ${nSteady} points `
      + `· last label EP \\d+ \\(End point\\) at point \\d+ · [\\d.:]+ s?`).test(await autoStatus())
    && await cdp.eval(`document.querySelector('[data-testid=auto-status]').dataset.why`) === stop.why,
    JSON.stringify([stop, await autoStatus()]));
  /* the panel builds its text once open (W122: a long run's table is thousands of lines) */
  await cdp.eval(`document.querySelector('.auto-output').open = true`);
  await until(`!!(document.querySelector('.auto-output pre') || {}).textContent`, 'Output open');
  const out = await cdp.eval(`(() => { const d = document.querySelector('.auto-output');
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
  /* T21 ("while it runs the status strip says Running...") and T22 ("the
     axis dialog can change during the run") used to be asserted here, on
     lecar.odex's fast default mesh: on a slow runner they raced the same
     way the Stop checks did (W42, GitHub #85) and lost every time, not
     occasionally -- a slow page's render/dispatch loop, swamped by
     this run's fast stream of `diagram add` events, coalesces every
     intermediate state and paints only the final "Stopped" one, no matter
     how long the test polls for "Running" to appear. Moved to
     autoStopRace() below (tools/models/heavy.odex, whose real, expensive
     right-hand side makes every point slow by construction, not by a
     mesh size tuned to a timing window), where an intermediate frame is
     always paintable. This run still runs to completion here unassessed
     mid-flight: its data feeds the checks below (the store's diagram,
     the chart, the labels). */
  check('the periodic branch arrives', await until('!s.busy && dv.points.br.includes(2)', 'periodic', 120000));
  check('its first point says it started from the Hopf label; the circle holds Floquet multipliers',
    await DS(`d.points.fr[d.points.br.indexOf(2)] === ${hbLab}`) && await DS('d.stab.periodic === 1 && d.stab.circle.length === 2'),
    JSON.stringify(await DS('[d.points.fr.filter(f => f), d.stab]')));

  /* the store is the events, exactly */
  const want = rebuildDiagram(await cdp.eval('__xpp.diagramEvents()'));
  const got = await DS('Object.fromEntries(Object.keys(d.points).filter(k => k !== "buffers").map(k => [k, Array.from(d.points[k])]))');
  let bad = want.pts.length === got.x.length ? null : `${got.x.length} points held, ${want.pts.length} sent`;
  const same = (a, b) => (a === null ? b === null : a === b); /* NaN arrives as null through JSON */
  for (let i = 0; i < want.pts.length && !bad; i++) {
    for (const f of ['x', 'y', 'y2', 'br', 'pt', 'ty', 'st', 'pe', 'd', 'c', 'lw', 'f2', 'nw'])
      if (!same(want.pts[i][f], got[f][i])) bad = `point ${i} ${f}: ${got[f][i]} vs ${want.pts[i][f]}`;
  }
  const labels = await DS('d.labels');
  const fr = await DS('Array.from(d.points.fr)');
  for (let i = 0; i < want.pts.length && !bad; i++) if (fr[i] !== want.pts[i].fr) bad = `point ${i} fr: ${fr[i]} vs ${want.pts[i].fr}`;
  check(`the store's diagram equals the diagram events (${want.pts.length} points, ${want.labels.length} labels)`,
    !bad && want.pts.length > 1000 && JSON.stringify(want.labels) === JSON.stringify(labels), bad || JSON.stringify(labels.slice(0, 5)));

  /* T27: AUTO's table in the Output panel: a row per label, the plain ones NPr prints (no type) too,
     each line of the core's log classified alone however its chunks arrived */
  const outRows = await cdp.eval(`__xpp.log().filter(l => l.kind === 'auto')
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
  const sentGrab = await cdp.eval('__xpp.sentCount()');
  await key('Enter');
  check('T21: grabbing a labelled periodic point imports its orbit: the main plot shows the limit cycle',
    grabbed && grabbed.type >= 3 && await until(`!s.busy && s.core.rows > 10 && w.series && w.series.rows === s.core.rows`, 'orbit', 20000)
    && JSON.stringify((await cdp.eval(`__xpp.sentFrom(${sentGrab})`)).filter(c => c.win === 'auto' || c.key === 'i')
      .map(c => c.key)) === JSON.stringify(['f', 'i']),
    JSON.stringify([grabbed && [grabbed.br, grabbed.pt, grabbed.type, grabbed.lab], await S('[s.core.rows, w.series && w.series.rows]'),
      await cdp.eval(`__xpp.sentFrom(${sentGrab})`)]));

  /* hover names the Hopf point */
  /* the Hopf point's position read again on each try: the view can still
     move after the T21 import (macOS CI hovered a periodic point) */
  let hovered = false, hbAt = null;
  for (let tries = 0; tries < 3 && !hovered; tries++) {
    await mouse('mouseMoved', 5, 5);
    hbAt = await autoScreen(want.pts[hb.point].x, want.pts[hb.point].y);
    await mouse('mouseMoved', hbAt.x, hbAt.y);
    hovered = await until(`s.diagram.hover && s.diagram.hover.point === ${hb.point}`, 'hover hb', 3000);
  }
  /* the readout is a render behind the store's own hover field (W20): wait
     for its text too, rather than reading it the instant the store settled
     (W40, GitHub #83: a macos-ui rerun saw the store's hover set but the
     readout still blank) */
  const readoutOk = hovered && await until(`/HB label \\d+/.test(document.querySelector('.auto-readout').textContent)`, 'hb readout text', 3000);
  check('hovering the Hopf point names it in the readout',
    readoutOk,
    JSON.stringify([await DS('d.hover'), await readout(), hbAt, await DG(), await autoArea()]));
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

  /* zoom, pan, reset: no history any more (GitHub #110) */
  const a = await autoArea(), cx = a.x + a.w / 2, cy = a.y + a.h / 2;
  const axes = await DS('d.axes');
  await mouse('mouseWheel', cx, cy, {deltaX: 0, deltaY: -120});
  check('the wheel zooms the diagram', await until('dv.viewport.x', 'auto wheel')
    && width((await DG()).x) < (axes.xmax - axes.xmin) * 0.9, JSON.stringify(await DS('d.viewport')));
  const z1 = await DS('d.viewport');
  await mouse('mouseMoved', cx - 60, cy - 40);
  await mouse('mousePressed', cx - 60, cy - 40, {button: 'left', buttons: 1, clickCount: 1});
  for (let st = 1; st <= 6; st++) await mouse('mouseMoved', cx - 60 + 20 * st, cy - 40 + 14 * st, {button: 'left', buttons: 1});
  await mouse('mouseReleased', cx + 60, cy + 44, {button: 'left', buttons: 0, clickCount: 1});
  check('a box zooms to it', await until(`dv.viewport.x && dv.viewport.x.max - dv.viewport.x.min < ${width(z1.x) * 0.8}`, 'auto box'),
    JSON.stringify(await DS('d.viewport')));
  check('there is no Undo zoom button any more (GitHub #110)',
    !(await cdp.eval(`[...document.querySelectorAll('.auto-panel button')].some(b => b.textContent === 'Undo zoom')`)));
  await cdp.eval(`document.querySelector('.auto-host').focus()`);
  await key('ArrowLeft');
  check('the arrow keys pan it', await until('dv.viewport.x', 'auto pan'));
  await key('0');
  check("0 goes back to AUTO's axes", await until('dv.viewport.x === null', 'auto reset')
    && await until(`(() => { const d = __xpp.diagram(); return !!d && Math.abs(d.x.min - ${axes.xmin}) < 1e-9; })()`, 'auto reset drawn'));

  /* T30: the corner Fit (and the AUTO tools' own Fit) fit the view to the
     branches shown, client-side, and just replace the current zoom (no
     history: GitHub #110) */
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
  await until('dv.viewport.x', 'panned away');
  const away = await DG();
  check('panned far from the data', away.x.min > dataExtent.xmax, JSON.stringify([away.x, dataExtent]));
  const fitCondition = e => `(() => { const g = __xpp.diagram(); return !!g && g.x.min <= ${e.xmin} + 1e-6 && g.x.max >= ${e.xmax} - 1e-6
    && g.y.min <= ${e.ymin} + 1e-6 && g.y.max >= ${e.ymax} - 1e-6; })()`;
  await cdp.eval(`document.querySelector('.auto-host .plot-fit').click()`);
  check('the corner Fit brings every curve back into view',
    await until(fitCondition(dataExtent), 'fit applied'), JSON.stringify([await DG(), dataExtent]));
  await key('0');
  await until('dv.viewport.x === null', 'auto reset 2');
  for (let st = 0; st < 15; st++) await key('ArrowLeft');
  await until('dv.viewport.x', 'panned away 2');
  await DG(); /* every arrow key's pan landed before the Fit */
  await cdp.eval(`[...document.querySelectorAll('.auto-panel .plot-tools button')].find(b => b.textContent === 'Fit').click()`);
  check("the AUTO tools' own Fit does the same as the corner button",
    await until(fitCondition(dataExtent), 'fit2 applied'), JSON.stringify([await DG(), dataExtent]));
  const reset3 = await displays();
  await key('0');
  await until('dv.viewport.x === null', 'auto reset 3');
  await displayTold(reset3, 'auto reset 3 told'); /* before g (macos-ui, W93) */

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
  const sentAxes = await cdp.eval('__xpp.sentCount()');
  await cdp.eval(`document.querySelector('.auto-host').focus()`);
  await key('a');
  await menuKey('n');
  await until("s.ask && s.ask.kind === 'form'", 'autoplot form');
  await until(`!!document.activeElement.closest('.dialog')`, 'autoplot focus');
  await key('Enter');
  check('T21: Axes/Norm, OK: the diagram holds all its points again, in norms, without a reDraw',
    await until(`!s.busy && dv.axes.plot === 1 && dv.points.x.length === ${nAll}`, 'norm axes')
    && JSON.stringify(await DS('d.points.y.slice(0, 50)')) !== JSON.stringify(y0)
    && !(await cdp.eval(`__xpp.sentFrom(${sentAxes}).some(c => c.cmd === 'redraw' || (c.win === 'auto' && c.key === 'd'))`))
    && (await DG()).curves.length > 0, JSON.stringify(await DS('[d.axes, d.points.x.length]')));
  /* and the axis dialog does the same: hI-lo from its Plots select, then a Fit */
  await cdp.eval(`document.querySelector('.auto-axis-name[data-axis=y]').click()`);
  await until(`document.querySelector('.auto-axis-dialog select[data-field=plot]')`, 'y dialog');
  await cdp.eval(`(() => { const s = document.querySelector('.auto-axis-dialog select[data-field=plot]'); s.value = '2';
    s.dispatchEvent(new Event('change', {bubbles: true})); })()`);
  check('T22: the axis dialog\'s plot type goes to the core as an auto set with a Fit: hI-lo, every point, periodic max and min',
    await until(`!s.busy && dv.axes.plot === 2 && dv.points.x.length === ${nAll}
      && document.querySelector('.auto-axis-dialog select[data-field=yvar]')`, 'hilo axes', 20000)
    && (await DG()).curves.some(c => c.which === 'y2')
    && await cdp.eval(`__xpp.sent().some(c => c.cmd === 'auto' && c.op === 'set' && c.axes && c.axes.plot === 2 && c.axes.fit)`),
    JSON.stringify(await DS('d.axes')));
  const fitted = await DS('d.axes');
  await setAxisRange(fitted.ymin - 0.1, fitted.ymax + 0.1);
  check("T21: its min and max change the view at once, the core's axes stay",
    await until(`dv.viewport.y && Math.abs(dv.viewport.y.min - ${fitted.ymin - 0.1}) < 1e-9`, 'y range')
    && (await DS('d.axes.ymin')) === fitted.ymin, JSON.stringify(await DS('[d.viewport, d.axes]')));
  await cdp.eval(`document.querySelector('.auto-axis-dialog .dialog-actions button').click()`);
  await until('!document.querySelector(".auto-axis-dialog")', 'y dialog closed');
  await cdp.eval(`document.querySelector('.auto-panel .plot-tools button:nth-child(2)').click()`);
  await until('dv.viewport.y === null', 'view reset 2');

  /* T21: Clear hides the branches so far in the view, the key shows them again; the core is sent the AUTO
     window's clear key (W60) */
  const nCurvesAll = (await DG()).curves.length, sentClear = await cdp.eval('__xpp.sentCount()');
  await autoButton('C');
  check('T21: Clear hides every branch so far; the key offers "Earlier branches (2)"',
    await until(`!s.busy && __xpp.diagram().curves.length === 0 && s.diagram.earlier === ${nAll} && !!document.querySelector('.auto-earlier')`, 'cleared')
    && /Earlier branches \(2\)/.test(await cdp.eval(`document.querySelector('.auto-earlier').textContent`))
    && JSON.stringify(await cdp.eval(`__xpp.sentFrom(${sentClear})`)) === JSON.stringify([{cmd: 'key', win: 'auto', key: 'c', button: 'clear'}, {cmd: 'key', win: 'auto', key: 'd', button: 'redraw'}]),
    await cdp.eval(`document.querySelector('.auto-legend').textContent`));
  await cdp.eval(`document.querySelector('.auto-earlier').click()`);
  check('T21: "Earlier branches" shows them again', await until(`__xpp.diagram().curves.length === ${nCurvesAll}`, 'earlier shown'));

  check('W155: AUTO has no standalone settings-file control',
    await cdp.eval(`document.querySelector('#auto-settings-load') === null`));
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
  await until('!s.busy && dv.axes.plot === 2', 'hilo again');
  await key('a');
  await menuKey('f');
  await until('!s.busy', 'fit again');

  /* on a phone: a sheet with Back, 44 px targets, no sideways scroll */
  await metrics({width: 390, height: 844, deviceScaleFactor: 2, mobile: true});
  await cdp.send('Emulation.setTouchEmulationEnabled', {enabled: true, maxTouchPoints: 5});
  await cdp.send('Emulation.setEmulatedMedia', {features: [{name: 'pointer', value: 'coarse'}, {name: 'hover', value: 'none'}]}).catch(() => {});
  await rendered();
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
  await rendered();

  /* a page that connects while AUTO is open gets the view and the whole diagram again */
  await reloadPage();
  check('a reloaded page opens the AUTO view and asks for the diagram again',
    await until(`s.diagram.open && dv.axes && !s.busy && dv.points.x.length === ${want.pts.length}`, 'reload auto', 30000)
    && JSON.stringify(await DS('d.labels')) === JSON.stringify(labels), JSON.stringify(await DS('[d.open, d.points.x.length]')));

  /* T22/T23/T25 (Numerics edited during a run, Stop, the run after) moved
     to autoStopRace() below, on tools/models/heavy.odex: see that
     function's own comment for why they no longer live here. */

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

/* T21, T22, T23, T25: AUTO Numerics/Stop during a run (W42, GitHub #85).
   On lecar.odex's fast default mesh, a slow page processes its incoming
   queue of `diagram add` events slower, in order; if the run finishes
   (reaches Nmax, or a parameter/norm bound) before the page's
   render/dispatch loop gets a turn, every intermediate state is coalesced
   away and only the final "Stopped" one ever paints, no matter how large
   Nmax is or how long the test polls. A finer mesh (raising NTST) can make
   each point slower without raising Nmax, but a mesh raised far enough to
   survive a slow page's worst-case backlog (NTST 1000) made a second
   periodic continuation from the same Hopf point -- started fresh after the
   first was Stopped mid-run -- come back with zero points every time, at
   any speed (see the final report for how this reproduces; not fixed here,
   per the card).

   tools/models/heavy.odex sidesteps both problems: its right-hand side is
   deliberately expensive (a long sum), so every AUTO point costs real
   seconds of native CPU by construction, at the model's own default mesh
   (NTST 150) -- not a mesh size picked to fit a timing window. Its own
   Nmax (2000) needs no raising either: at seconds per point it cannot be
   reached within any test's patience, on any runner, at any speed --
   "cannot finish on its own" is true by construction, not by luck. This
   is the same model and technique tools/autocheck.py's section_abort
   already relies on for the raw-protocol Abort check, proven fast there
   (< 0.5 s) with no browser involved. The checks below (W58) assert Stop's
   result -- rows kept, the EP label, a second run working -- not how long
   it took: Stop latency is a perf: line, measured, never failed. */
async function autoStopRace() {
  await until('!s.busy && !s.ask && s.core.menu !== 1', 'the command before ended');
  await key('f');
  await until('!s.busy && s.core.menu === 1', 'file menu'); /* not only !busy: a sent too soon is lost (macos-ui, W93) */
  await key('a');
  check('AUTO Stop race: File/Auto opens the AUTO view',
    await until('s.diagram.open && s.diagram.shown && dv.axes && !s.busy', 'auto open')
    && await cdp.eval(`!!document.querySelector('.auto-panel .auto-host')`),
    JSON.stringify(await cdp.eval(`(() => { const s = __xpp.state(); return {busy: s.busy, menu: s.core.menu, ask: s.ask,
      diagram: [s.diagram.open, s.diagram.shown, !!s.diagram.views[s.diagram.active].axes], sent: __xpp.sent().slice(-4), actions: __xpp.actions().slice(-10)}; })()`)));
  await until(`document.activeElement.closest('.auto-host')`, 'auto focus');

  /* heavy.odex starts at a stable point (mu=-1): the steady branch finds
     its Hopf point continuing in mu, same as lecar's own first run above */
  await autoButton('R');
  check('AUTO Stop race: Run asks how to start',
    await until("s.ask && s.ask.kind === 'menu' && s.ask.title === 'Start'", 'start menu'));
  await menuKey('s');
  check('AUTO Stop race: the steady branch arrives', await until('!s.busy && dv.points.x.length > 2', 'steady', 60000 * SLOW));

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
    await until("s.ask && s.ask.kind === 'menu'", `hopf menu ${what}`);
    await menuKey('p');
  };
  const nPre = await DS('d.points.x.length');
  await periodicFromHopf('the run');
  const going = await until(`s.busy && !s.ask && dv.points.x.length > ${nPre}`, 'periodic run going', 60000 * SLOW);
  check('AUTO Stop race: the periodic run from the Hopf point is going (heavy.odex: seconds per point, by construction)', going);

  check('T21: while it runs the status strip says "Running: periodic orbits", with branch 2, its point count and a Stop',
    going && await until(`/^Running: periodic orbits · branch 2, point \\d+ · \\d+ points/.test(document.querySelector('[data-testid=auto-status]').textContent)
      && !!document.querySelector('.auto-status .auto-stop')`, 'strip running', 20000 * SLOW), await autoStatus());
  await cdp.eval(`document.querySelector('.auto-axis-name[data-axis=y]').click()`);
  const running = await until(`s.busy && document.querySelector('.auto-axis-dialog[data-axis=y]')`, 'axis dialog');
  const dlg = await cdp.eval(`(() => { const d = document.querySelector('.auto-axis-dialog');
    return {plot: d.querySelector('select[data-field=plot]').disabled, yvar: d.querySelector('select[data-field=yvar]').disabled,
      min: d.querySelector('input[data-field=min]').disabled}; })()`);
  await setAxisRange(0.2, 1.2);
  check('T22: during the run the axis dialog\'s plot type and variable can change (they would wait), and min/max change the view at once',
    running && !dlg.plot && !dlg.yvar && !dlg.min && await until(`s.busy && dv.viewport.y && dv.viewport.y.min === 0.2
      && dv.viewport.y.max === 1.2`, 'axis range during run', 5000 * SLOW)
      && !(await cdp.eval(`__xpp.sent().some(c => (c.win === 'auto' && c.key === 'a') || (c.cmd === 'auto' && c.op === 'set'))`)),
    JSON.stringify([running, dlg, await DS('d.viewport'), await S('s.busy')]));
  await key('Escape');
  await until('!document.querySelector(".auto-axis-dialog")', 'axis closed');
  await cdp.eval(`document.querySelector('.auto-panel .plot-tools button:nth-child(2)').click()`);
  await until('dv.viewport.y === null', 'view reset');

  /* T22, W106: an edit made mid-run is a setting: sent at once, the core applies it once the run ends */
  const sentPre = await cdp.eval('__xpp.sentCount()');
  await autoButton('N');
  await until(`!!document.querySelector('.auto-settings-dialog[data-settings=auto-numerics]')`, 'numerics during run');
  await cdp.eval(`(() => { const i = document.querySelector('.auto-settings-dialog input[data-field=nmx]');
    i.value = '15'; i.dispatchEvent(new Event('input', {bubbles: true})); })()`);
  await cdp.eval(`document.querySelector('.auto-settings-dialog .dialog-actions .primary').click()`);
  const setsSent = () => cdp.eval(`__xpp.sentFrom(${sentPre}).filter(c => c.cmd === 'auto' && c.op === 'set')`);
  check('W106: Numerics during a run: Nmax 15 is sent at once (one set), shown, the core\'s still 2000 (the run keeps it)',
    going && await S(`s.busy && s.autoSettings.inflight.length === 1 && s.autoSettings.inflight[0].patch.numerics.nmx === 15
      && s.autoSettings.core.numerics.nmx === 2000`)
    && await cdp.eval(`document.querySelector('.auto-tools button[data-op=numerics]').classList.contains('auto-pending')`)
    && JSON.stringify(await setsSent()) === JSON.stringify([{cmd: 'auto', op: 'set', numerics: {nmx: 15}}]),
    JSON.stringify([going, await S('[s.busy, s.autoSettings]'), await setsSent()]));

  /* T25: a connection the browser opened and sent nothing on yet (a
     preconnect) held xppautX's one request thread for 30 s, and the Stop
     with it, while the run went on */
  const [host, port] = (await cdp.eval('location.host')).split(':');
  const idle = net.connect(Number(port), host);
  idle.on('error', () => {}); /* a reset arriving around the destroy below is nothing to fail over */
  await new Promise(r => idle.once('connect', r));
  /* The accepted command below is behind this socket in the listener's accept queue. */
  const labsPre = await DS('d.labels.length'), runningAtStop = await S('s.busy'), tStop = Date.now();
  const stopBtn = runningAtStop && await until(`document.querySelector('.auto-status .auto-stop')`, 'stop button present', 2000 * SLOW);
  if (stopBtn) await cdp.eval(`document.querySelector('.auto-status .auto-stop').click()`);
  /* the wait is a safety ceiling (XPP_CHECK_SLOW-scaled), not a pass/fail
     budget: how long Stop actually took is a perf: line below (W58) --
     tools/autocheck.py's section_abort measures the core's own Abort
     latency separately, with no browser in the loop at all */
  const stopped = stopBtn && await until('!s.busy', 'stopped', 5000 * SLOW), tookStop = Date.now() - tStop;
  const lastLab = await DS('d.labels.length > 0 && d.labels[d.labels.length - 1].sym');
  idle.destroy();
  if (runningAtStop && stopBtn && stopped) perf('T25 stop latency', ms(tookStop));
  check('T25: Stop ends a running AUTO computation, with an idle connection open to xppautX, on an EP label',
    runningAtStop && stopBtn && stopped && (await DS('d.labels.length')) > labsPre && lastLab === 'EP',
    JSON.stringify([runningAtStop, stopBtn, stopped, tookStop, labsPre, lastLab]));
  check('W106: once the run ends the set applies: the core\'s Nmax is 15, nothing in flight, no second set',
    await until(`!s.busy && s.autoSettings.core.numerics.nmx === 15 && !s.autoSettings.inflight.length`,
      'applied at idle', 60000 * SLOW)
    && JSON.stringify(await setsSent()) === JSON.stringify([{cmd: 'auto', op: 'set', numerics: {nmx: 15}}])
    && !(await cdp.eval(`document.querySelector('.auto-tools button[data-op=numerics]').classList.contains('auto-pending')`)),
    JSON.stringify([await S('s.autoSettings'), await setsSent()]));
  check('T23: after Stop the status strip says so: "Stopped: by the user (Stop)"',
    await until(`/^Stopped: by the user \\(Stop\\) · periodic orbits/.test(document.querySelector('[data-testid=auto-status]').textContent)`,
      'strip stopped by user') && (await DS('d.stop && d.stop.why')) === 'user', JSON.stringify([await DS('d.stop'), await autoStatus()]));

  const nMid = await DS('d.points.x.length');
  await periodicFromHopf('the run after');
  const ran = await until(`!s.busy && dv.points.x.length > ${nMid}`, 'the run after', 120000 * SLOW);
  const nNew = (await DS('d.points.x.length')) - nMid;
  check('T22: the run after uses it: its periodic branch stops at Nmax, 15 points',
    ran && nNew === 15, JSON.stringify([ran, nNew]));

  await cdp.eval(`document.querySelector('.auto-close').click()`);
  check("AUTO Stop race: Close closes AUTO's window and the view",
    await until('!s.diagram.open && !s.busy', 'auto close') && !(await cdp.eval(`!!document.querySelector('.auto-panel, .auto-show')`)));
}

/* ---- views of the AUTO diagram (W50, docs/protocol.md "Views of the diagram") ------------ */

/* the screen position of (x, y) of view `view`'s chart, once settled */
async function viewScreen(view, x, y) {
  const a = await settled(() => cdp.eval(`(() => { const r = document.querySelector('.auto-pane[data-view="${view}"] .u-over')
    .getBoundingClientRect(); return {x: r.left, y: r.top, w: r.width, h: r.height}; })()`));
  const d = await settled(() => cdp.eval(`__xpp.diagram(${view})`));
  return {x: a.x + (x - d.x.min) / (d.x.max - d.x.min) * a.w, y: a.y + (d.y.max - y) / (d.y.max - d.y.min) * a.h};
}

/* the Axes menu's New view, each view its own axes (the state: the views and their axes, never
   pixels), a click making a view the active one, a grab taken in another view, a view closed, and
   the views back from a session file */
async function autoViews(dir) {
  check('AUTO views: the page connects', await until('s.hello && !s.busy', 'hello'));
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
  await cdp.eval(`__xpp.send({cmd: 'key', win: 'equilibrium', key: 'i'})`);
  await until('!s.busy', 'import');
  await key('f');
  await until('!s.busy', 'file menu');
  await key('a');
  await until('s.diagram.open && s.diagram.shown && dv.axes && !s.busy', 'auto open');
  check('AUTO views: one view at first, no title and no close button',
    await cdp.eval(`document.querySelectorAll('.auto-pane').length === 1 && !document.querySelector('.auto-pane-close')`)
    && (await S('s.diagram.views.length')) === 1);
  await autoButton('R');
  await menuKey('s');
  check('AUTO views: a steady-state branch', await until('!s.busy && dv.points.x.length > 10', 'steady state', 60000));
  const n = await DS('d.points.x.length');

  /* Axes / new View */
  await autoButton('A');
  await until("s.ask && s.ask.kind === 'menu' && s.ask.title === 'Plot Type'", 'axes menu');
  await menuKey('v');
  check("AUTO views: Axes' new view makes a second view, the active one, with the first one's axes and every point",
    await until(`!s.busy && s.diagram.views.length === 2 && s.diagram.active === 1 && s.diagram.views[1].points.x.length === ${n}
      && JSON.stringify(s.diagram.views[1].axes) === JSON.stringify(s.diagram.views[0].axes)
      && document.querySelectorAll('.auto-pane').length === 2
      && document.querySelector('.auto-pane.active').dataset.view === '1'
      && document.querySelectorAll('.auto-pane-close').length === 2`, 'second view'),
    JSON.stringify(await S('[s.diagram.views.map(v => [v.axes, v.points.x.length]), s.diagram.active]')));

  /* the second view's own axis dialog: its Plots, Norm */
  await cdp.eval(`document.querySelector('.auto-pane[data-view="1"] .auto-axis-name[data-axis=y]').click()`);
  await until(`document.querySelector('.auto-pane[data-view="1"] .auto-axis-dialog select[data-field=plot]')`, 'view 2 axis dialog');
  await cdp.eval(`(() => { const s = document.querySelector('.auto-pane[data-view="1"] .auto-axis-dialog select[data-field=plot]');
    s.value = '1'; s.dispatchEvent(new Event('change', {bubbles: true})); })()`);
  check("AUTO views: a view's axis dialog changes that view alone (an auto set naming it): norms in the second, the first still hI-lo, the same points in each",
    await until(`!s.busy && s.diagram.views[1].axes.plot === 1 && s.diagram.views[1].axes.ylabel === 'Norm'
      && s.diagram.views[0].axes.plot === 2 && s.diagram.views[0].axes.ylabel === 'V'
      && s.diagram.views.every(v => v.points.x.length === ${n})
      && s.diagram.views[0].points.br.every((b, i) => b === s.diagram.views[1].points.br[i])`, 'norm view', 20000)
    && await cdp.eval(`__xpp.sent().some(c => c.cmd === 'auto' && c.op === 'set' && c.axes && c.axes.view === 1 && c.axes.plot === 1)`)
    && await cdp.eval(`[...document.querySelectorAll('.auto-pane-title')].map(t => t.textContent).join('|') === 'V against iapp|Norm against iapp'`),
    JSON.stringify(await S('s.diagram.views.map(v => v.axes)')));
  check('AUTO views: each view draws its own chart', (await cdp.eval('__xpp.diagram(0).curves.length')) > 0
    && (await cdp.eval('__xpp.diagram(1).curves.length')) > 0
    && JSON.stringify(await cdp.eval('__xpp.diagram(0).y')) !== JSON.stringify(await cdp.eval('__xpp.diagram(1).y')));
  await cdp.eval(`document.querySelector('.auto-pane[data-view="1"] .auto-axis-dialog .dialog-actions button').click()`);

  /* a click in the first view makes it the active one, the core's too */
  const c0 = await center('.auto-pane[data-view="0"] .auto-host');
  await click(c0.x, c0.y);
  check('AUTO views: a click in a view makes it the active one (the core says so too)',
    await until(`s.diagram.active === 0 && document.querySelector('.auto-pane.active').dataset.view === '0'`, 'activate')
    && await cdp.eval(`__xpp.sent().some(c => c.cmd === 'auto' && c.op === 'view' && c.active === 0)`)
    && await until('!s.busy && s.diagram.active === 0', 'activated'));

  /* a grab from the other view: its cursor in both, a click in the second view takes a point */
  await autoButton('G');
  await until("s.diagram.grabbing && s.ask && s.ask.kind === 'grab' && s.diagram.info", 'grab');
  check('AUTO views: the grab cursor shows in every view', await until(`document.querySelectorAll('.auto-cursor').length === 2`, 'two cursors'));
  const target = await S(`(() => { const v = s.diagram.views[1], l = v.labels.find(l => l.sym === 'HB') || v.labels[1];
    return {point: l.point, x: v.points.x[l.point], y: v.points.y[l.point]}; })()`);
  const at = await viewScreen(1, target.x, target.y);
  await click(at.x, at.y);
  check('AUTO views: a click in the second view takes its point (the same index in every view), the active view unchanged',
    await until(`!s.busy && !s.diagram.grabbing && s.diagram.info && s.diagram.info.point === ${target.point}`, 'grabbed in view 2', 20000)
    && (await lastAnswer())?.point === target.point && (await S('s.diagram.active')) === 0,
    JSON.stringify([target, await lastAnswer(), await S('[s.diagram.info, s.diagram.active]')]));

  /* the views go into a session file; a view closed; the file opened: both views as they were */
  await cdp.eval(`__xpp.send({cmd: 'session', op: 'save', name: 'views'})`);
  await until('!s.busy', 'session saved');
  const saved = await S('JSON.stringify([s.diagram.active, s.diagram.views.map(v => [v.axes && v.axes.plot, v.axes && v.axes.ylabel, v.points.x.length])])');
  check('AUTO views: the session file is written', fs.existsSync(path.join(dir, 'views.snapx')), saved);
  await cdp.eval(`document.querySelector('.auto-pane[data-view="0"] .auto-pane-close').click()`);
  check('AUTO views: closing the first view leaves the second, now the only one (no close button), in norms',
    await until(`!s.busy && s.diagram.views.length === 1 && dv.axes.plot === 1 && s.diagram.active === 0
      && document.querySelectorAll('.auto-pane').length === 1 && !document.querySelector('.auto-pane-close')`, 'closed'),
    JSON.stringify(await S('[s.diagram.views.map(v => v.axes), s.diagram.active]')));
  await cdp.eval(`__xpp.send({cmd: 'open', file: 'views.snapx'})`);
  await until("s.ask && s.ask.kind === 'choice'", 'save first?');
  await answerAsk({key: 'd'});
  check('AUTO views: the session file brings both views back, their axes, points and the active one',
    await until(`!s.busy && s.diagram.open && JSON.stringify([s.diagram.active, s.diagram.views.map(v => [v.axes && v.axes.plot,
      v.axes && v.axes.ylabel, v.points.x.length])]) === ${JSON.stringify(saved)}`, 'views restored', 30000),
    `${saved} vs ${await S('JSON.stringify([s.diagram.active, s.diagram.views.map(v => [v.axes && v.axes.plot, v.axes && v.axes.ylabel, v.points.x.length])])')}`);
}

/* T21: while the core computes (an integration here), the AUTO view's own
   actions stay live and the buttons that need the core say why they wait */
/* W100 (GitHub #149): F was lost twice in the desktop window. (1) In AUTO, after a steady run,
   a grab of the Hopf point and a Periodic run, F did nothing; (2) after AUTO's Back, F in the
   main window did not open the File menu. The page and the core must agree on the menu at every
   step, and typed keys go to the menu the page shows. */
async function lostF() {
  await desktopMetrics();
  check('lostF: the page connects', await until('s.hello && !s.busy', 'hello'));
  await key('f');
  await until('!s.busy && s.core.menu === 1', 'file menu');
  await key('g');
  await menuKey('d');
  await until('!s.busy', 'hopf set');
  await key('s');
  await menuKey('g');
  await until("s.ask && s.ask.kind === 'choice'", 'eigenvalues?');
  await menuKey('n');
  await until('!s.busy', 'fixed point', 30000);
  await cdp.eval(`__xpp.send({cmd: 'key', win: 'equilibrium', key: 'i'})`);
  await until('!s.busy', 'import');
  await key('f');
  await until('!s.busy && s.core.menu === 1', 'file menu 2');
  await key('a');
  await until('s.diagram.open && s.diagram.shown && dv.axes && !s.busy', 'auto open');
  check('lostF: File/Auto returns the core to its main menu', await until('s.core.menu === 0', 'menu after auto'), JSON.stringify(await S('s.core')));

  /* sequence 2 first half: Back at once, F in the main window */
  const back = await center('.auto-back'); /* a real click, as the window gets: the focus is on the button that then goes */
  await click(back.x, back.y);
  await until('!s.diagram.shown && s.diagram.open && !s.busy', 'back');
  const menuShown = () => cdp.eval(`(() => { const r = document.querySelector('.menu-panel').getBoundingClientRect(); return !!document.elementFromPoint(r.left + r.width / 2, r.top + r.height / 2)?.closest('.menu-panel'); })()`);
  check('lostF: after Back the main menu is shown and the core is in its main menu', await menuShown() && (await S('s.core.menu')) === 0,
    JSON.stringify(await cdp.eval(`[document.activeElement.tagName, document.activeElement.className]`)));
  await key('f');
  check('lostF: after Back, F opens the main File menu (page and core agree)',
    await until('s.core.menu === 1 && !s.busy', 'file menu after back') && await menuShown(),
    JSON.stringify([await S('s.core.menu'), await cdp.eval(`document.activeElement.className`)]));
  await key('Escape');
  check('lostF: Escape closes it: the core is back in its main menu', await until('s.core.menu === 0 && !s.busy', 'main menu'));
  await key('u');
  check('lostF: after Back a typed key reaches the menu the page shows (U opens Numerics)',
    await until('s.core.menu === 2 && !s.busy', 'numerics after back'), JSON.stringify(await S('s.core.menu')));
  await key('Escape');
  await until('s.core.menu === 0 && !s.busy', 'main menu 2');

  /* sequence 1: steady state, grab the Hopf point, Periodic, then F in the AUTO view */
  await cdp.eval(`document.querySelector('.auto-show').click()`);
  await until('s.diagram.shown', 'auto show');
  await until(`document.activeElement.closest('.auto-host')`, 'auto focus');
  await key('r');
  await until("s.ask && s.ask.kind === 'menu' && s.ask.title === 'Start'", 'start menu');
  await menuKey('s');
  await until('!s.busy && dv.points.x.length > 10', 'steady state', 60000);
  /* Back during a grab: the grab ask stays open with the panel hidden, and every key is swallowed by it */
  await cdp.eval(`document.querySelector('.auto-host').focus()`);
  await key('g');
  await until("s.ask && s.ask.kind === 'grab'", 'grab 0');
  const back2 = await center('.auto-back');
  await click(back2.x, back2.y);
  await until('!s.diagram.shown', 'back during grab');
  check('lostF: Back during a grab cancels it: no ask stays open behind the main window',
    await until('!s.busy && !s.ask && !s.diagram.grabbing && s.core.menu === 0', 'grab cancelled'), JSON.stringify(await S('[s.ask && s.ask.kind, s.busy, s.core]')));
  await key('f');
  check('lostF: ... and F opens the main File menu',
    await until('s.core.menu === 1 && !s.busy && !s.ask', 'file menu after grab'), JSON.stringify(await S('[s.ask && s.ask.kind, s.busy, s.core]')));
  await key('Escape');
  await until('s.core.menu === 0 && !s.busy', 'main menu 3');
  await cdp.eval(`document.querySelector('.auto-show').click()`);
  await until('s.diagram.shown', 'auto show 2');
  await until(`document.activeElement.closest('.auto-host')`, 'auto focus 2');
  await cdp.eval(`document.querySelector('.auto-host').focus()`);
  await key('g');
  await until("s.ask && s.ask.kind === 'grab'", 'grab');
  await key('Tab');
  await until("s.diagram.info.sym === 'HB' && s.ask && s.ask.kind === 'grab'", 'grab tab');
  await key('Enter');
  await until('!s.busy && !s.diagram.grabbing', 'grabbed');
  await cdp.eval(`document.querySelector('.auto-host').focus()`);
  await key('r');
  await until("s.ask && s.ask.kind === 'menu' && s.ask.title === 'Hopf Pt'", 'hopf menu');
  await menuKey('p');
  /* F straight away: while the run goes or after it, AUTO's File menu opens */
  await key('f');
  const opened = await until("s.ask && s.ask.kind === 'menu' && /file/i.test(s.ask.title || '')", 'auto file menu', 120000);
  check("lostF: F in the AUTO view after the periodic run opens AUTO's File menu",
    opened, JSON.stringify(await S('[s.busy, s.ask, s.core]')));
  await key('Escape');
  await until('!s.busy && !s.ask', 'auto file menu closed', 120000);
  await key('f');
  check("lostF: ... and again once the run has ended", await until("s.ask && s.ask.kind === 'menu' && /file/i.test(s.ask.title || '')", 'auto file menu 2'),
    JSON.stringify(await S('[s.busy, s.ask]')));
  await key('Escape');
  await until('!s.busy && !s.ask', 'closed');
}

/* W100, sequence 1 on a run that is still going (heavy.odex: seconds per point): F in the AUTO view
   while the periodic run computes reaches AUTO's File menu (a view action: the run goes on, the
   menu is answered at once), not the main window's, and nothing is left hidden in the core. */
async function lostFRunning() {
  await until('!s.busy && !s.ask && s.core.menu !== 1', 'the command before ended');
  await key('f');
  await until('!s.busy && s.core.menu === 1', 'file menu');
  await key('a');
  await until('s.diagram.open && s.diagram.shown && dv.axes && !s.busy', 'auto open');
  await until(`document.activeElement.closest('.auto-host')`, 'auto focus');
  await key('r');
  await until("s.ask && s.ask.kind === 'menu' && s.ask.title === 'Start'", 'start menu');
  await menuKey('s');
  await until('!s.busy && dv.points.x.length > 2', 'steady', 60000 * SLOW);
  await cdp.eval(`document.querySelector('.auto-host').focus()`);
  await key('g');
  await until("s.ask && s.ask.kind === 'grab' && s.diagram.info", 'grab');
  for (let i = 0; i < 20 && !(await DS("d.info && d.info.sym === 'HB'")); i++) {
    const n = await DS('d.infoEvents');
    await key('Tab');
    await until(`s.diagram.infoEvents > ${n} && s.ask`, 'grab tab');
  }
  await key('Enter');
  await until('!s.busy && !s.diagram.grabbing', 'grabbed');
  await cdp.eval(`document.querySelector('.auto-host').focus()`);
  await key('r');
  await until("s.ask && s.ask.kind === 'menu'", 'hopf menu');
  const nPre = await DS('d.points.x.length');
  await menuKey('p');
  check('lostF: the periodic run is going',
    await until(`s.busy && !s.ask && dv.points.x.length > ${nPre}`, 'periodic going', 60000 * SLOW));
  const sent0 = await cdp.eval('__xpp.sentCount()');
  const menu0 = await S('s.core.menu');
  await key('f');
  const sent = await cdp.eval(`__xpp.sentFrom(${sent0})`);
  check("lostF: F in the AUTO view during the run is AUTO's File key (win auto), never the main window's",
    sent.length === 1 && sent[0].cmd === 'key' && sent[0].win === 'auto' && sent[0].key === 'f', JSON.stringify(sent));
  check("lostF: ... its menu opens when the run ends (a view action queued in the core), AUTO's File menu, not the main one",
    await until("s.ask && s.ask.kind === 'menu' && s.ask.title === 'File' && s.ask.items.includes('Save diagram')", 'auto file menu', 120000 * SLOW)
    && (await S('s.core.menu')) === menu0, JSON.stringify(await S('[s.busy, s.ask && s.ask.title, s.core]')));
  await key('Escape');
  check('lostF: Escape closes it and the core is idle in its main menu',
    await until('!s.busy && !s.ask && s.core.menu === 0', 'closed', 60000 * SLOW), JSON.stringify(await S('[s.busy, s.ask && s.ask.title, s.core]')));
}

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
  check('busy: File/Auto opens the AUTO view', await until('s.diagram.open && dv.axes && !s.busy', 'auto open'));
  await focusPlot();
  await key('i');
  await key('g');
  /* s.computing, not just s.busy: the controls are disabled by the core's
     `computing` event (W95), which comes a moment after the command starts;
     reading them on s.busy alone raced it (macos-ui, 2026-10-01) */
  const busy = await until('s.busy && s.computing && !s.ask && s.core && (s.progress || w.series)', 'integration running', 5000);
  const run = await cdp.eval(`(() => { const b = document.querySelector('.auto-tools button[aria-keyshortcuts=R]');
    return {disabled: b.disabled, title: b.title}; })()`);
  const menu = await cdp.eval(`(() => { const b = document.querySelector('.menu-panel .menu-item'); return b && [b.disabled, b.title]; })()`);
  const a = await autoArea();
  await mouse('mouseMoved', a.x + a.w / 2, a.y + a.h / 2);
  await mouse('mouseWheel', a.x + a.w / 2, a.y + a.h / 2, {deltaX: 0, deltaY: -120});
  check('busy: during an integration the AUTO diagram zooms, and Run is disabled with "Not while a computation runs"',
    busy && await until('s.busy && dv.viewport.x', 'zoom while busy', 3000) && run.disabled
    && run.title === 'Not while a computation runs: available when it ends', JSON.stringify([busy, run, await DS('d.viewport')]));
  check('busy: T22: Parameter, Numerics and Mark values stay enabled (their changes wait), and Axes and Clear (views, W95)',
    await cdp.eval(`['P', 'N', 'U', 'A', 'C'].every(k => !document.querySelector('.auto-tools button[aria-keyshortcuts=' + k + ']').disabled)`));
  check("busy: ... and the main window's menu waits the same way", !!menu && menu[0] === true
    && menu[1] === 'Not while a computation runs: available when it ends', JSON.stringify(menu));
  await until('!s.busy', 'integration done', 30000);
}

/* W68 (GitHub #116): while a run goes nothing new starts, discarded in the page: the
   controls are disabled, a key but Escape does nothing and is not kept (it used to go
   out, be dropped by the core with no reply, and hold every later key back for good),
   a slider's edit waits; Escape stops the run, and I G integrates after it. Total 1e6:
   the run does not end on its own before the Escape, whatever the machine's speed. */
async function busyKeys() {
  await desktopMetrics();
  check('busy keys: the page connects', await until('s.hello && !s.busy', 'hello', 60000));
  const sid = await addSlider('iapp');
  await focusPlot();
  await key('u');
  await until('!s.busy && s.core.menu === 2', 'numerics menu');
  await key('t');
  await until("s.ask && s.ask.kind === 'string'", 'total');
  await answerAsk({ok: 1, value: '1e6'});
  await until('!s.busy', 'total set');
  await key('Escape');
  await until('!s.busy && s.core.menu === 0', 'main menu');
  await focusPlot();
  await key('i');
  await until("s.ask && s.ask.kind === 'menu'", 'ic menu');
  const n0 = await cdp.eval('__xpp.actions().length');
  await key('g');
  const running = await until('s.busy && !s.ask && w.series && w.series.rows > 0', 'the run under way', 30000);
  /* W95: what is disabled during a computation is decided by each action's kind, from hello */
  const ui = await cdp.eval(`(() => {
    const item = k => document.querySelector('.menu-panel .menu-item[aria-keyshortcuts="' + k + '"]');
    const tool = t => [...document.querySelectorAll('button')].find(b => b.textContent.trim() === t);
    const off = [...document.querySelectorAll('.menu-panel .menu-item')].filter(b => b.disabled)
      .map(b => b.getAttribute('aria-keyshortcuts')).join('');
    return {
      status: document.querySelector('[data-testid=status]').textContent, off,
      integrate: document.querySelector('.title-bar button.primary').getAttribute('aria-disabled'),
      newWindow: tool('New window').disabled,
      save: [...document.querySelectorAll('[data-section="par"] .value-tools button')].find(b => b.textContent === 'Save').disabled,
      stop: document.querySelector('.status-bar button.danger').disabled,
      viewItems: ['w', 'v', 'x', 'r', 'e', 'f', 'n', 'd', 'k', 'g'].every(k => !item(k).disabled),
    };
  })()`);
  check('busy keys: during a run the status says what runs and that Escape stops it; Integrate, the menu\'s computations '
    + 'and data (Initialconds, Sing pts...) and Save are disabled, Parameters (a setting, W106) is not',
    running && ui.status === 'Running Go… Esc stops' && ui.integrate === 'true' && ui.save
    && ['i', 'c', 'a', 's', 'b'].every(k => ui.off.includes(k)) && !ui.off.includes('p'), JSON.stringify(ui));
  check('W95: ... while its views (Window/zoom, Viewaxes, Xi vs t, Restore, Erase, File, and the menus holding a view: Nullcline, Dir.field, Kinescope, Graphic stuff), New window and Stop stay enabled',
    ui.viewItems && !ui.newWindow && !ui.stop, JSON.stringify(ui));
  /* a view clicked during the run is sent, and the core runs it after the run (the menu it
     opens is answered then): New window makes window 2 once the run has stopped */
  const sentView = await cdp.eval('__xpp.sentCount()');
  await cdp.eval(`[...document.querySelectorAll('.plot-window-tools button')].find(b => /New window/.test(b.textContent)).click()`);
  const viewSent = await cdp.eval(`__xpp.sentFrom(${sentView})`);
  check('W95: a view button clicked during the run goes out (New window: Makewindow)',
    viewSent.length === 1 && viewSent[0].key === 'm' && await S('s.busy'), JSON.stringify(viewSent));
  const sent0 = await cdp.eval('__xpp.sentCount()');
  await focusPlot();
  await key('i');
  await key('s');
  await key('c');
  const typed = await cdp.eval(`__xpp.sentFrom(${sent0})`);
  const track = await cdp.eval(`(() => { const r = document.getElementById('slider-range-${sid}').getBoundingClientRect();
    return {x: r.left, y: r.top + r.height / 2, w: r.width}; })()`);
  await mouse('mousePressed', track.x + track.w * 0.3, track.y, {button: 'left', clickCount: 1});
  for (let k = 1; k <= 5; k++) await mouse('mouseMoved', track.x + track.w * (0.3 + k * 0.1), track.y, {button: 'left'});
  await mouse('mouseReleased', track.x + track.w * 0.8, track.y, {button: 'left', clickCount: 1});
  const dragged = await cdp.eval(`__xpp.sentFrom(${sent0})`);
  const draggedTo = dragged.length ? Number(dragged[dragged.length - 1].text) : null;
  const shownDrag = await S(`s.values.inflight.some(f => f.set.name === 'iapp')
    && !document.querySelector('[data-slider="${sid}"]').classList.contains('queued') && !document.querySelector('.values-queued')`);
  /* W106: a parameter field and a Numerics field are settings too: editable during the run, sent at once */
  await editField('par', 'phi', '0.4');
  await editField('num', 'Ncline mesh', '50');
  const edited = await cdp.eval(`__xpp.sent().slice(${sent0 + dragged.length})`);
  const stillBusy = await S('s.busy');
  check('busy keys: computation keys typed during a run (I, S, C) send nothing',
    typed.length === 0 && stillBusy, JSON.stringify({typed, stillBusy}));
  check('W106: during the run a slider, a parameter field and a Numerics field send their edits at once (sets), '
    + 'shown as the values, with no pending mark',
    dragged.length >= 1 && dragged.every(c => c.cmd === 'set' && c.name === 'iapp') && shownDrag
    && JSON.stringify(edited) === JSON.stringify([{cmd: 'set', kind: 'par', name: 'phi', text: '0.4'},
      {cmd: 'set', kind: 'num', name: 'nmesh', text: '50'}])
    && await cdp.eval(`${fieldOf('par', 'phi')}.querySelector('input').value === '0.4'
      && ${fieldOf('num', 'Ncline mesh')}.querySelector('input').value === '50'`) && stillBusy,
    JSON.stringify({dragged, edited, shownDrag, stillBusy}));
  const runPars = await S('JSON.stringify(s.core.pars)');
  await focusPlot();
  await key('Escape');
  const stopped = await until(`__xpp.actions().slice(${n0}).includes('event:idle') && !s.busy`, 'the run stopped', 30000);
  /* Stop goes straight to the transport (session.abort), so sent() has nothing: the run's `stopped` shows it */
  /* (the New window's own menu, answered by its key once the core runs it after the run, is no typed key) */
  const out = await cdp.eval(`__xpp.sentFrom(${sent0}).map(c => c.cmd).filter(c => c !== 'answer' && c !== 'set')`);
  const cancelled = await cdp.eval(`__xpp.actions().slice(${n0}).includes('event:stopped')`);
  check('busy keys: Escape during a run stops it (stopped) and sends nothing else; the run kept its values (the state '
    + 'during it had the parameters it started with)',
    stopped && cancelled && out.length === 0 && JSON.parse(runPars).find(p => p[0] === 'iapp')[1] === 0.05,
    JSON.stringify({stopped, cancelled, out, runPars}));
  check('W106: after the run the core has the edits made during it (state and numerics)',
    await until(`!s.busy && !s.values.inflight.length && Math.abs(s.core.pars.find(p => p[0] === "iapp")[1] - ${draggedTo}) < 1e-9
      && s.core.pars.find(p => p[0] === "phi")[1] === 0.4 && s.numerics.find(f => f.key === 'nmesh').value === 50`,
    'settings applied after the run', 10000),
    JSON.stringify(await S('[s.busy, s.values.inflight, s.core.pars, s.numerics]')));
  /* the New window clicked during the run ran after it: its menu answered by the click's own key */
  check('W95: the view clicked during the run ran after it: window 2 exists',
    await until('!s.busy && !s.ask && s.plots.windows.length === 2', 'new window after the run', 10000),
    JSON.stringify(await S('[s.busy, s.ask, s.plots.windows.length]')));
  await cdp.eval(`[...document.querySelectorAll('.plot-tab')].find(b => b.id === 'plot-tab-1').click()`);
  await until('!s.busy && s.plots.active === 1', 'window 1 again');
  /* the wedge case: keys work after the run (Total back to 20: 401 rows), and the next
     run uses the slider's edit */
  await focusPlot();
  await key('u');
  await until('!s.busy && s.core.menu === 2', 'numerics menu after the run');
  await key('t');
  await until("s.ask && s.ask.kind === 'string'", 'total after the run');
  await answerAsk({ok: 1, value: '20'});
  await until('!s.busy', 'total 20');
  await key('Escape');
  await until('!s.busy && s.core.menu === 0', 'main menu after the run');
  const n1 = await S('s.seriesCount');
  await focusPlot();
  await key('i');
  await key('g');
  check('busy keys: after the run I then G typed at once integrate, with the slider\'s edit applied',
    await until(`s.seriesCount > ${n1} && w.series.rows === 401 && !s.busy`, 'I G after the run', 30000)
    && await S(`Math.abs(s.core.pars.find(p => p[0] === "iapp")[1] - ${draggedTo}) < 1e-9 && s.values.inflight.length === 0`),
    JSON.stringify(await S('[s.busy, s.ask, w.series && w.series.rows, s.values.inflight]')));
}

/* W83 (GitHub #132): the status bar reserves a fixed-width slot for the
   progress bar and Stop/Stopping… button (theme.css .status-run) so a run
   starting or ending never resizes the bar or moves the plot beside it.
   Total 1e6 (as busyKeys above) makes the run long enough, by construction,
   to see progress mid-run without racing the clock. */
async function statusBarLayout() {
  const box = () => cdp.eval(`(() => { const r = s => { const b = s.getBoundingClientRect();
      return {w: Math.round(b.width), h: Math.round(b.height), top: Math.round(b.top), left: Math.round(b.left)}; };
    return {bar: r(document.querySelector('.status-bar')), plot: r(document.querySelector('.plot-host'))}; })()`);
  await desktopMetrics();
  check('status bar: the page connects', await until('s.hello && !s.busy', 'hello'));
  const idle1 = await box();
  await key('u');
  await until('!s.busy && s.core.menu === 2', 'numerics menu');
  await key('t');
  await until("s.ask && s.ask.kind === 'string'", 'total');
  await answerAsk({ok: 1, value: '1e6'});
  await until('!s.busy', 'total set');
  await key('Escape');
  await until('!s.busy && s.core.menu === 0', 'main menu');
  await focusPlot();
  await key('i');
  await until("s.ask && s.ask.kind === 'menu'", 'ic menu');
  await key('g');
  const progressShowing = await until('s.busy && s.progress', 'progress showing', 15000);
  check('status bar: a long run reports progress', progressShowing, JSON.stringify(await S('s.progress')));
  const running = await box();
  const runningUi = await cdp.eval(`(() => { const p = document.querySelector('.status-bar progress'),
      b = document.querySelector('.status-bar button.danger');
    const bar = document.querySelector('.status-bar'), slot = document.querySelector('.status-run'),
      last = [...bar.children].filter(c => getComputedStyle(c).display !== 'none').pop();
    return {progressShown: p.classList.contains('shown') && getComputedStyle(p).visibility === 'visible',
      stopShown: b.classList.contains('shown') && getComputedStyle(b).visibility === 'visible' && !b.disabled,
      slotLast: last === slot,
      slotRightGap: Math.round(bar.getBoundingClientRect().right - slot.getBoundingClientRect().right
        - parseFloat(getComputedStyle(bar).paddingRight))}; })()`);
  check('status bar: while a run shows, the progress bar and Stop button are visible in the reserved slot',
    runningUi.progressShown && runningUi.stopShown, JSON.stringify(runningUi));
  check('status bar: the progress bar and Stop sit at the right end of the bar, after the rows (W86)',
    runningUi.slotLast && Math.abs(runningUi.slotRightGap) <= 1, JSON.stringify(runningUi));
  await focusPlot();
  await key('Escape');
  await until('!s.busy', 'run stopped', 30000);
  const idle2 = await box();
  const idleUi = await cdp.eval(`(() => { const p = document.querySelector('.status-bar progress'),
      b = document.querySelector('.status-bar button.danger');
    return {progressHidden: getComputedStyle(p).visibility === 'hidden',
      stopHidden: getComputedStyle(b).visibility === 'hidden'}; })()`);
  check('status bar: idle again, the reserved slot goes back to hidden (visibility, not removed)',
    idleUi.progressHidden && idleUi.stopHidden, JSON.stringify(idleUi));
  check('status bar: idle, running and after the run have the identical bar box and plot-host box (nothing moved or resized)',
    JSON.stringify(idle1) === JSON.stringify(running) && JSON.stringify(running) === JSON.stringify(idle2),
    JSON.stringify({idle1, running, idle2}));
}

/* ---- T21: where keys go, the theme switch, long menus in columns ---------------- */

/* W59a: the recording bar. Record in the title bar starts a recording: the
   red bar shows the steps taken and a note box; a note typed there goes to
   the core when the box is left and with the next step, which empties it;
   Integrate says it is a button; Stop asks the file's name and the bar goes.
   The file holds the step with its note above it. */
async function recordCheck(dir) {
  await desktopMetrics();
  check('record: no recording bar before one starts, Record in the title bar',
    !(await cdp.eval(`!!document.querySelector('.recbar')`)) && await cdp.eval(`!!document.querySelector('.title-bar .record-toggle')`));
  await cdp.eval(`document.querySelector('.title-bar .record-toggle').click()`);
  check('record: Record starts one: the red bar with its dot and 0 steps, Record gone from the title bar',
    await until(`s.core.recording && s.core.recording.steps === 0 && !s.busy`, 'recording')
    && await cdp.eval(`!!document.querySelector('.recbar .rec-dot') && document.querySelector('.recbar .rec-count').textContent.trim() === '0 steps'
      && !document.querySelector('.title-bar .record-toggle')`));
  await cdp.eval(`(() => { const t = document.querySelector('.recbar textarea'); t.focus(); })()`);
  await key('g');
  check('record: a letter typed into the note box stays there (no hotkey)',
    await cdp.eval(`document.querySelector('.recbar textarea').value === 'g'`) && !(await S('s.ask')));
  await cdp.eval(`(() => { const t = document.querySelector('.recbar textarea');
    t.value = 'First run.\\nIt settles.'; t.dispatchEvent(new Event('input', {bubbles: true})); t.blur(); })()`);
  check('record: the note goes to the core when the box is left, and waits for the next step',
    await until(`s.core.recording.note === 'First run.\\nIt settles.' && !s.busy`, 'note'), JSON.stringify(await S('s.core.recording')));
  await cdp.eval(`document.querySelector('.title-bar button.primary').click()`);
  check('record: a step taken (Integrate): 1 step, the note gone with it, the box empty',
    await until(`s.core.recording.steps === 1 && s.core.recording.note === '' && !s.busy`, 'step', 20000)
    && await cdp.eval(`document.querySelector('.recbar textarea').value === '' && document.querySelector('.recbar .rec-count').textContent.trim() === '1 step'`),
    JSON.stringify(await S('s.core.recording')));
  check('record: Integrate sends its key as a button', await cdp.eval(`__xpp.sent().some(c => c.cmd === 'key' && c.key === 'i' && c.button === 'Integrate')`));
  await cdp.eval(`document.querySelector('.recbar .rec-stop').click()`);
  check('record: Stop asks the file\'s name, the model\'s offered',
    await until(`s.ask && s.ask.kind === 'file' && s.ask.wild === '*.recx' && s.ask.file === 'lecar.recx'`, 'file ask'), JSON.stringify(await S('s.ask')));
  await cdp.eval(`__xpp.send({cmd: 'answer', id: __xpp.state().ask.id, file: 'web'})`);
  check('record: the recording ends and the bar goes',
    await until(`!s.core.recording && !s.busy`, 'stopped') && !(await cdp.eval(`!!document.querySelector('.recbar')`)));
  const file = path.join(dir, 'web.recx');
  const lines = fs.existsSync(file) ? fs.readFileSync(file, 'utf8').split(/\r?\n/) : [];
  const at = lines.indexOf('@steps');
  check('record: web.recx holds the model and the step, its note above it, the button named',
    lines[0] === 'xppautx-recording 1' && lines.includes('@file lecar.odex') && at > 0
    && lines[at + 1] === '# First run.' && lines[at + 2] === '# It settles.'
    && lines[at + 3] === '{"step":"Initialconds → Go","button":"Integrate","keys":["i","g"]}', JSON.stringify(lines.slice(at, at + 5)));
  check('record: the recording begins with the session\'s state (a @snapshot section before the files, W59d)',
    lines.indexOf('@snapshot') > 0 && lines.indexOf('@snapshot') < lines.indexOf('@file lecar.odex'));

  /* W59d: File/Quit (F Q) asks one question, as the window's close box does:
     Save session (S), Don't save (D), Cancel; Escape keeps the session */
  await until('!s.busy && !s.ask && s.core.menu !== 1', 'the command before ended');
  await key('f');
  await until('!s.busy && s.core.menu === 1', 'file menu');
  await key('q');
  check('quit: File/Quit asks "Quit xppautX? Save this session first?", Save session (S), Don\'t save (D), Cancel',
    await until(`s.ask && s.ask.kind === 'choice' && s.ask.question === 'Quit xppautX? Save this session first?'`, 'quit ask')
    && await cdp.eval(`(() => { const b = [...document.querySelectorAll('.dialog button')].map(e => e.textContent.trim());
      return b.includes('SSave session') && b.includes("DDon't save") && b.includes('Cancel'); })()`),
    JSON.stringify(await S('s.ask')));
  await key('Escape');
  check('quit: Escape cancels: the dialog goes and the session stays',
    await until('!s.ask && !s.busy', 'quit cancelled') && await cdp.eval(`__xpp.sent().some(c => c.cmd === 'answer' && c.ok === 0)`));
}

/* W59b: the player (docs/mockups/record-play.html's player screen). A
   recording made on the page (a note, Integrate, Erase) opens in the player
   from the title bar: the caption, the step list with its note, the
   controls; played at 4x to its end, the core pressed each step's keys (the
   button for Integrate) and the page answered none of the step's
   questions; a step's note edited and saved goes into the .recx; a changed
   recording shows the banner, which Dismiss hides. State only, never
   pixels. */
async function playerCheck(dir) {
  await desktopMetrics();
  await cdp.eval(`document.querySelector('.title-bar .record-toggle').click()`);
  await until(`s.core.recording && !s.busy`, 'recording');
  await cdp.eval(`(() => { const t = document.querySelector('.recbar textarea');
    t.value = 'The cell fires once.'; t.dispatchEvent(new Event('input', {bubbles: true})); t.dispatchEvent(new FocusEvent('blur')); })()`);
  await until(`s.core.recording.note === 'The cell fires once.' && !s.busy`, 'note');
  await cdp.eval(`document.querySelector('.title-bar button.primary').click()`);
  await until(`s.core.recording.steps === 1 && !s.busy`, 'integrated', 20000);
  await key('e');
  await until(`s.core.recording.steps === 2 && !s.busy`, 'erased');
  await cdp.eval(`document.querySelector('.recbar .rec-stop').click()`);
  await until(`s.ask && s.ask.kind === 'file'`, 'file ask');
  await cdp.eval(`__xpp.send({cmd: 'answer', id: __xpp.state().ask.id, file: 'play'})`);
  await until(`!s.core.recording && !s.busy`, 'stopped');
  check('player: the title bar offers Play a recording', await cdp.eval(`!!document.querySelector('.title-bar .play-open')`));
  await cdp.eval(`document.querySelector('.title-bar .play-open').click()`);
  check('player: Play a recording asks for the .recx', await until(`s.ask && s.ask.kind === 'file' && s.ask.wild === '*.recx'`, 'recx ask'),
    JSON.stringify(await S('s.ask')));
  await cdp.eval(`__xpp.send({cmd: 'answer', id: __xpp.state().ask.id, file: 'play.recx'})`);
  await until(`s.ask && s.ask.kind === 'choice'`, 'save first?');
  await cdp.eval(`__xpp.send({cmd: 'answer', id: __xpp.state().ask.id, key: 'd'})`);
  check('player: the recording opens: its steps with their notes, paused at step 0, intact',
    await until(`s.player.open && s.player.steps.length === 2 && s.core.player && s.core.player.step === 0 && !s.core.player.playing && !s.busy`, 'player', 20000)
    && await S(`s.player.steps[0].note === 'The cell fires once.' && s.player.intact`), JSON.stringify(await S('s.player.steps')));
  check('player: the caption above the plot, the controls, the step list; no changed banner',
    await cdp.eval(`(() => { const c = document.querySelector('.player-caption');
      return !!c && /Press Play/.test(c.textContent) && !!document.querySelector('.player-stage .plots')
        && document.querySelectorAll('.player-segs i').length === 2 && document.querySelectorAll('.player-segs i.view').length === 1
        && document.querySelectorAll('.player-step').length === 2 && /The cell fires once/.test(document.querySelector('.player-step').textContent)
        && !document.querySelector('.player-changed'); })()`));
  const sentBefore = await cdp.eval(`__xpp.sentCount()`);
  await cdp.eval(`[...document.querySelectorAll('.player-speed button')].find(b => b.textContent === '4x').click()`);
  await until(`s.core.player.speed === 4 && !s.busy`, '4x');
  await cdp.eval(`document.querySelector('.player-play').click()`);
  check('player: Play plays to the end (4x)', await until(`s.core.player.step === 2 && s.core.player.running === -1 && !s.busy`, 'played', 30000),
    JSON.stringify(await S('s.core.player')));
  const presses = await cdp.eval(`__xpp.presses()`);
  check('player: each step\'s keys were pressed before they went (Integrate by its button, then G; Erase)',
    JSON.stringify(presses) === JSON.stringify([{step: 0, what: 'key', index: 0}, {step: 0, what: 'key', index: 1}, {step: 1, what: 'key', index: 0}]),
    JSON.stringify(presses));
  check('player: the page answered none of the step\'s questions (the player did)',
    await cdp.eval(`__xpp.sentFrom(${sentBefore}).every(c => c.cmd === 'play')`), JSON.stringify(await cdp.eval(`__xpp.sentFrom(${sentBefore})`)));
  check('player: at the end the caption shows the last step, Play offers Play again',
    await cdp.eval(`/Step 2: Erase/.test(document.querySelector('.player-caption').textContent) && /Play again/.test(document.querySelector('.player-play').textContent)`));
  await cdp.eval(`document.querySelectorAll('.player-step')[1].click()`);
  check('player: a step clicked opens its note editor', await until(`s.player.selected === 1 && !!document.querySelector('.player-editor textarea')`, 'editor'));
  await cdp.eval(`(() => { const t = document.querySelector('.player-editor textarea');
    t.value = 'Clear the screen.'; t.dispatchEvent(new Event('input', {bubbles: true})); })()`);
  await cdp.eval(`[...document.querySelectorAll('.player-editor button')].find(b => b.textContent === 'Save note').click()`);
  check('player: Save note writes it into the .recx, above its step, the recording still intact',
    await until(`s.player.steps[1].note === 'Clear the screen.' && s.player.intact && !s.busy`, 'saved'),
    JSON.stringify(await S('s.player.steps[1]')) + JSON.stringify(await cdp.eval(`__xpp.sent().slice(-3)`)) + JSON.stringify(await S('__xpp.log().slice(-3)')));
  const text = fs.readFileSync(path.join(dir, 'play.recx'), 'utf8');
  check('player: the note is a # line above the step in the file', /# Clear the screen\.\r?\n\{"step":"Erase"/.test(text), text.slice(text.indexOf('@steps')));
  /* a changed copy: the banner, and Dismiss */
  fs.writeFileSync(path.join(dir, 'changed.recx'), text.replace('"keys":["e"]', '"keys":["e"] '));
  await cdp.eval(`__xpp.send({cmd: 'play', op: 'open', file: ${JSON.stringify(path.join(dir, 'changed.recx'))}})`);
  await until(`s.ask && s.ask.kind === 'choice'`, 'save first? (2)');
  await cdp.eval(`__xpp.send({cmd: 'answer', id: __xpp.state().ask.id, key: 'd'})`);
  check('player: a changed recording opens with the banner "changed after it was made"',
    await until(`s.player.open && !s.player.intact && !s.busy && !!document.querySelector('.player-changed')`, 'changed', 20000)
    && /changed after it was made/.test(await cdp.eval(`document.querySelector('.player-changed').textContent`)));
  await cdp.eval(`document.querySelector('.player-changed button').click()`);
  check('player: Dismiss hides the banner', await until(`s.player.dismissed && !document.querySelector('.player-changed')`, 'dismissed'));
  /* W150: a view over the plots (AUTO) still has the player: caption, controls, step list in a dock; Close AUTO takes it back */
  await key('f');
  await until('!s.busy', 'file menu (player)');
  await key('a');
  await until('s.diagram.open && s.diagram.shown && !s.busy', 'auto open (player)', 20000);
  check('player: over the AUTO view the caption, controls and step list are still on the page (a dock), one set of each',
    await cdp.eval(`(() => { const d = document.querySelector('.player-dock');
      return !!d && !!document.querySelector('.auto-panel') && d.querySelectorAll('.player-controls').length === 1
        && document.querySelectorAll('.player-controls').length === 1 && !!d.querySelector('.player-caption')
        && d.querySelectorAll('.player-step').length === 2 && !!d.querySelector('.player-play')
        && document.documentElement.classList.contains('player-docked'); })()`));
  await cdp.eval(`document.querySelector('.auto-close').click()`);
  check('player: with AUTO closed the player is back around the plots (no dock)',
    await until(`!s.diagram.open && !document.querySelector('.player-dock') && !!document.querySelector('.player-stage .plots') && !!document.querySelector('.player-controls') && !s.busy`, 'dock gone', 20000));
  await cdp.eval(`document.querySelector('.player-close').click()`);
  check('player: Close leaves the player; the plot stays', await until(`!s.player.open && !document.querySelector('.player') && !!document.querySelector('.plots') && !s.busy`, 'closed'));
}

/* W110: the desktop window's close box and File > Quit (which call the
   page's __xppQuit: core/xpp_window.cpp; called here as they do) ask File >
   Quit's question without stopping a computation: while the core is idle
   it asks, as F Q does; while a run goes on the page asks it itself (its
   LEAVE_ASK, -1), the run's rows still growing; Cancel leaves the run
   alone; Don't save quits (the plain quit in a browser, which has no window
   to close) and the program exits. State only, never pixels. */
const QUIT_Q = 'Quit xppautX? Save this session first?';

/** an integration that goes on for minutes (lecar to t = 1e7), going */
async function longRun() {
  await until('!s.busy && !s.ask && s.core.menu === 0', 'main menu');
  await cdp.eval(`__xpp.send({cmd: 'key', key: 'u'})`);
  await until('!s.busy && s.core.menu === 2', 'numerics menu');
  await cdp.eval(`__xpp.send({cmd: 'key', key: 't'})`);
  await until(`s.ask && s.ask.kind === 'string'`, 'total');
  await cdp.eval(`__xpp.send({cmd: 'answer', id: __xpp.state().ask.id, value: '1e7'})`);
  await until('!s.busy && !s.ask && s.core.menu === 2', 'total set');
  await cdp.eval(`__xpp.send({cmd: 'key', key: 'Escape'})`);
  await until('!s.busy && s.core.menu === 0', 'main menu again');
  await cdp.eval(`__xpp.send({cmd: 'key', key: 'i'})`);
  await until(`s.ask && s.ask.kind === 'menu'`, 'initialconds');
  await cdp.eval(`__xpp.send({cmd: 'answer', id: __xpp.state().ask.id, key: 'g'})`);
  return until('s.computing && s.seriesAppends > 2', 'the run going', 30000 * SLOW);
}

/** whether the section's xppautX exited within ms */
async function exited(ms = 10000) {
  await waitForExit(sessionServer.proc, ms * SLOW);
  return sessionServer.proc.exitCode === 0;
}

const appends = () => S('s.seriesAppends');

async function leaveCheck() {
  await until('!s.busy && !s.ask', 'idle');
  await cdp.eval('__xppQuit()');
  check('leave: idle, the window\'s Quit is the core\'s question, as F Q asks it',
    await until(`s.ask && s.ask.id > 0 && s.ask.kind === 'choice' && s.ask.question === '${QUIT_Q}'`, 'core quit ask')
    && await cdp.eval(`__xpp.sent().some(c => c.cmd === 'quit' && c.ask === true)`), JSON.stringify(await S('s.ask')));
  await key('Escape');
  check('leave: idle, Escape cancels it and the session stays', await until('!s.ask && !s.busy', 'cancelled')
    && sessionServer.proc.exitCode === null);

  check('leave: a long integration is going', await longRun(), JSON.stringify(await S('[s.busy, s.computing, s.seriesAppends]')));
  const sent0 = await cdp.eval('__xpp.sentCount()');
  await cdp.eval('__xppQuit()');
  check('leave: during the run the page asks the same question itself, Save session (S), Don\'t save (D), Cancel',
    await until(`s.ask && s.ask.id === -1 && s.ask.question === '${QUIT_Q}'`, 'page quit ask')
    && await cdp.eval(`(() => { const b = [...document.querySelectorAll('.dialog button')].map(e => e.textContent.trim());
      return b.includes('SSave session') && b.includes("DDon't save") && b.includes('Cancel'); })()`),
    JSON.stringify(await S('s.ask')));
  const a0 = await appends();
  check('leave: the run goes on under the question, its rows still growing, nothing sent',
    await until(`s.seriesAppends > ${a0} + 2 && s.computing && s.ask && s.ask.id === -1`, 'rows grow', 20000 * SLOW)
    && (await cdp.eval(`__xpp.sentCount()`)) === sent0, JSON.stringify(await cdp.eval(`__xpp.sentFrom(${sent0})`)));
  await key('Escape');
  const a1 = await appends();
  check('leave: Cancel (Escape) closes it, the run untouched (still computing, rows growing, no abort sent)',
    await until(`!s.ask && s.computing && s.seriesAppends > ${a1} + 2`, 'run goes on', 20000 * SLOW)
    && (await cdp.eval(`__xpp.sentCount()`)) === sent0, JSON.stringify(await cdp.eval(`__xpp.sentFrom(${sent0})`)));
  await cdp.eval('__xppQuit()');
  await until(`s.ask && s.ask.id === -1`, 'asked again');
  await key('d');
  /* in the window Don't save closes it, which sends the plain quit (and
     ends the process even when the core does not: core/xpp_window.cpp
     window_closed); a browser has no window, so the page sends the plain
     quit itself. The core exits at once, mid-run, without a bye: browser
     mode then keeps serving the page's log (xpp_http.cpp at_exit) until
     the page goes, which the window's close does */
  check('leave: Don\'t save during the run sends the plain quit, and the core exits at once',
    await until('s.exited !== null && !s.ask', 'core exit', 10000 * SLOW)
    && await cdp.eval(`__xpp.sent().some(c => c.cmd === 'quit' && !c.ask && !c.save)`),
    JSON.stringify(await cdp.eval(`__xpp.sent().slice(-2)`)) + JSON.stringify(await S(`[s.ask, s.computing, s.exited]`)));
}

/* Save session during a run, with a recording in progress: the question
   says so; the run stops, the session and then the recording are saved
   (their names asked), and xppautX exits */
async function leaveSaveCheck(dir) {
  await until('!s.busy && !s.ask', 'idle');
  await cdp.eval(`__xpp.send({cmd: 'record', op: 'start'})`);
  await until('s.core.recording && !s.busy', 'recording');
  check('leave save: a long integration is going', await longRun());
  await cdp.eval('__xppQuit()');
  check('leave save: the page\'s question names the recording in progress',
    await until(`s.ask && s.ask.id === -1 && s.ask.question === 'Quit xppautX? Save this session, and the recording in progress, first?'`,
      'page quit ask'), JSON.stringify(await S('s.ask')));
  await key('s');
  check('leave save: Save session sends the quit that saves; the run stops and the session file is asked',
    await until(`s.ask && s.ask.kind === 'file' && s.ask.wild === '*.snapx' && !s.computing`, 'session file', 30000 * SLOW)
    && await cdp.eval(`__xpp.sent().some(c => c.cmd === 'quit' && c.save === true)`), JSON.stringify(await S('s.ask')));
  await cdp.eval(`__xpp.send({cmd: 'answer', id: __xpp.state().ask.id, file: 'left'})`);
  check('leave save: then the recording\'s name',
    await until(`s.ask && s.ask.kind === 'file' && s.ask.wild === '*.recx'`, 'recording file', 30000 * SLOW), JSON.stringify(await S('s.ask')));
  await cdp.eval(`__xpp.send({cmd: 'answer', id: __xpp.state().ask.id, file: 'left'})`);
  check('leave save: both saved, then xppautX exits',
    await exited() && fs.existsSync(path.join(dir, 'left.snapx')) && fs.existsSync(path.join(dir, 'left.recx')),
    JSON.stringify(fs.readdirSync(dir)));
}

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
  /* W67: File/cOpy set line by its menu button: the core asks the set's name, shows the line,
     and sends it; the page shows what it copies (the toast holds the line, copied or, when the
     clipboard is refused, to copy by hand) */
  await key('f');
  await until('s.core.menu === 1 && !s.busy', 'file menu for copy');
  const sentCopy0 = await cdp.eval('__xpp.sentCount()');
  await cdp.eval(`document.querySelector('.menu-panel .menu-item[aria-keyshortcuts=o]').click()`);
  check('copy set: its File menu button sends the key o',
    await cdp.eval(`__xpp.sentFrom(${sentCopy0}).some(c => c.cmd === 'key' && c.key === 'o')`));
  check('copy set: the core asks the name, pre-filled set1 or the next free one',
    await until("s.ask && s.ask.kind === 'string' && /^set[0-9]+$/.test(s.ask.value)", 'name ask'), JSON.stringify(await S('s.ask')));
  await cdp.eval(`__xpp.send({cmd: 'answer', id: __xpp.state().ask.id, value: 'mine'})`);
  check('copy set: the line is shown for confirmation before it is copied',
    await until("s.ask && s.ask.kind === 'choice' && s.ask.question.includes('set mine {') && s.ask.question.includes('iapp=')", 'line ask'),
    JSON.stringify(await S('s.ask')));
  await cdp.eval(`__xpp.send({cmd: 'answer', id: __xpp.state().ask.id, key: 'c'})`);
  check('copy set: the page receives the line (a toast holds it)',
    await until("s.toasts.some(t => /set mine \{iapp=[^}]*,V=[^}]*\}/.test(t.text))", 'copy toast') && await until('!s.busy', 'idle after copy'),
    JSON.stringify(await S('s.toasts')));
  await until('s.core.menu === 0 && !s.busy', 'main menu after copy');
  /* a letter typed into a field stays there */
  const sent0 = await cdp.eval('__xpp.sentCount()');
  await cdp.eval(`(() => { const i = document.querySelector('.messages-search input'); i.closest('details').open = true; i.focus(); })()`);
  await key('g');
  check('keys: a letter typed into a text field stays in it',
    await cdp.eval(`document.activeElement.value === 'g'`) && (await cdp.eval('__xpp.sentCount()')) === sent0);
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
  await metrics({width: 390, height: 844, deviceScaleFactor: 2, mobile: true});
  await rendered();
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
  await metrics({width: 1280, height: 860, deviceScaleFactor: 1, mobile: false});
}

/* what the store and the chart hold, sampled by the page at every frame
   while an integration runs, and the samples it took: [rows, points, busy,
   the frame's performance.now()] each time one of the first three changes */
const startSampling = () => cdp.eval(`(() => { const seen = window.__seen = []; window.__sampling = true;
  const tick = t => { const s = __xpp.state(), w = ${ACTIVE}, p = __xpp.plot();
    const r = [w.series ? w.series.rows : -1, p && p.curves[0] ? p.curves[0].points : -1, s.busy, t];
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
  await rendered();
}

/** the input of field `name` in the values panel's section `sec` (par or ic) */
const fieldOf = (sec, name) => `[...document.querySelectorAll('[data-section="${sec}"] .value-field')]
  .find(f => f.querySelector('.value-name').textContent.toLowerCase() === ${JSON.stringify(name.toLowerCase())})`;
/** type `text` into a field and leave it (two round trips: Preact renders the draft before the blur reads it) */
/** answers the open Integrate menu with Go and waits for that run's own idle: `!s.busy`
    alone is also true between a flushed set's idle and the run starting (W58: no race) */
async function goRun() {
  const n = await cdp.eval('__xpp.actions().length');
  await key('g');
  await until(`__xpp.actions().slice(${n}).includes('event:idle') && !s.busy`, 'the run after Go', 60000);
}
async function editField(sec, name, text) {
  await cdp.eval(`(() => { const el = ${fieldOf(sec, name)}.querySelector('input'); el.focus();
    el.value = ${JSON.stringify(text)}; el.dispatchEvent(new Event('input', {bubbles: true})); })()`);
  await rendered();
  const busy = await cdp.eval(`(() => { const busy = __xpp.state().busy && __xpp.state().computing;
    ${fieldOf(sec, name)}.querySelector('input').blur(); return !!busy; })()`);
  await rendered();
  return busy;
}
/** one `input` event carrying the whole new text, as a keystroke, a paste or a drop would (T35d):
    focuses the field first (unless already focused) but does not blur, so a refused edit's brief
    hint can still be read afterward */
async function typeIntoField(sec, name, text) {
  await cdp.eval(`(() => { const el = ${fieldOf(sec, name)}.querySelector('input');
    if (document.activeElement !== el) el.focus();
    el.value = ${JSON.stringify(text)}; el.dispatchEvent(new Event('input', {bubbles: true})); })()`);
  await rendered();
}
/** a field's box as the page shows it: its aria-invalid, its text, its message (if any) and
    whether it still has the focus */
const fieldState = (sec, name) => cdp.eval(`(() => { const f = ${fieldOf(sec, name)}, i = f.querySelector('input'),
    m = f.querySelector('.field-error');
  return {invalid: i.getAttribute('aria-invalid'), value: i.value, message: m ? m.textContent : null,
    focused: document.activeElement === i}; })()`);
const icsOf = 'JSON.stringify(s.core.ics.map(p => p[1]))';
const icFields = () => cdp.eval(`JSON.stringify([...document.querySelectorAll('[data-section="ic"] .value-field input')].map(i => i.value))`);
const nowCells = () => cdp.eval(`JSON.stringify([...document.querySelectorAll('.value-now')].map(o => o.textContent))`);
const close6 = (a, b) => Math.abs(a - b) <= 1e-6 * Math.max(1, Math.abs(b));

/* lecar.odex: runs accumulate, Erase, Redraw, Last and "Use current state", reset, save and load, sliders */
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
  const shown0 = await displays();
  await cdp.eval(`${legend}.click()`);
  await until('w.showRuns', 'show runs');
  await displayTold(shown0, 'show runs told'); /* before I, G: else the run never starts (linux-ui, W93) */
  await focusPlot();
  const ran3 = await integrate(601, 30000);
  check('runs: a third run (Go) keeps both earlier ones', ran3 && await until('w.history.runs.length === 2', '3 runs'),
    JSON.stringify({ran: ran3, runs: await S('w.history.runs.length'), rows: await S('w.series.rows'), busy: await S('s.busy'),
      plot: await cdp.eval('__xpp.plot().runs')}));

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

  /* "Use current state" is Initialconds/Last (i, l, W60): the ICs become the Now it was clicked at, and a run follows */
  const nowBefore = await S('s.core.now'), n1 = await S('s.seriesCount');
  const sentUse = await cdp.eval('__xpp.sentCount()');
  await cdp.eval(`[...document.querySelectorAll('[data-section="ic"] .value-tools button')].find(b => b.textContent.includes('Use current state')).click()`);
  check('runs: "Use current state" sends Initialconds/Last (keys i, l), the ICs become the Now it started from and it runs',
    await until(`!s.busy && s.seriesCount > ${n1}`, 'use state')
    && JSON.stringify((await cdp.eval(`__xpp.sentFrom(${sentUse})`)).map(c => c.key)) === JSON.stringify(['i', 'l'])
    && JSON.stringify(await S('s.core.ics.map(p => p[1])')) === JSON.stringify(nowBefore),
    JSON.stringify([await S('s.core.ics'), nowBefore]));

  /* a parameter's edit and reset (W106): sent at once, the core applies
     it; the model-value marking follows the core's own value */
  await editField('par', 'phi', '0.5');
  check('runs: an edited parameter is sent at once, the core has it, no pending mark',
    await until('!s.busy && !s.values.inflight.length && Math.abs(s.core.pars.find(p => p[0] === "phi")[1] - 0.5) < 1e-12', 'phi set')
    && !(await cdp.eval(`${fieldOf('par', 'phi')}.classList.contains('queued')`)),
    JSON.stringify(await S('[s.values.inflight, s.core.pars]')));
  const reset = `${fieldOf('par', 'phi')}.querySelector('.value-reset')`;
  check('runs: a changed parameter is marked, its reset names the default',
    await cdp.eval(`${fieldOf('par', 'phi')}.classList.contains('changed') && ${reset}.title === 'default: 0.333'`),
    await cdp.eval(`${reset}.title`));
  await cdp.eval(`${reset}.click()`);
  check('runs: reset restores the model value', await until('!s.busy && s.core.pars.find(p => p[0] === "phi")[1] === 0.333', 'reset'));

  /* Save, then Load a changed copy (W66 review, #114): both go through
     the core (docs/protocol.md "values"). Save writes into the model's
     folder, the same `pendingSave`/`deliver` path as Save session (T5,
     above) offers it as a download; Load uploads the picked file with
     the files API (session.ts loadValues, the same PUT any other upload
     uses) and sends `values` `read` -- applied at once, like File/Import XPPAUT
     set, so no Go is needed. */
  /* W95 (#143): a click while the page is busy with a command of its own (a redraw here, as its
     catch-up after an idle is) goes out and runs in its turn: Save is sent right behind it */
  const sentSave = await cdp.eval('__xpp.sentCount()');
  await cdp.eval(`(() => { __xpp.send({cmd: 'redraw'});
    [...document.querySelectorAll('[data-section="par"] .value-tools button')].find(b => b.textContent === 'Save').click(); })()`);
  const behind = await cdp.eval(`__xpp.sentFrom(${sentSave}).map(c => c.cmd + (c.op ? ' ' + c.op : ''))`);
  check('W95: a click right behind a command the page sent itself is not lost (Save behind a redraw goes out)',
    JSON.stringify(behind) === JSON.stringify(['redraw', 'values write']), JSON.stringify(behind));
  await until("s.files.offered && s.files.offered.name.endsWith('.par') && !s.busy", 'par saved');
  const parName = await S('s.files.offered.name');
  const saved = fs.readFileSync(path.join(dir, parName), 'utf8').replace(/\r\n/g, '\n');
  check('runs: Save writes XPP\'s parameter file', /^\d+   Number params\n/.test(saved) && /\n0\.05  iapp\n/.test(saved),
    saved.slice(0, 80));
  const parFile = path.join(dir, 'changed.par');
  fs.writeFileSync(parFile, saved.replace('\n0.05  iapp\n', '\n0.075  iapp\n'));
  await pickFiles('#values-load-par', [parFile]);
  check('runs: Load applies at once, no Go needed',
    await until('!s.busy && Math.abs(s.core.pars.find(p => p[0] === "iapp")[1] - 0.075) < 1e-9', 'par load applied'),
    await S('s.core.pars.find(p => p[0] === "iapp")'));
  check('runs: the loaded file was copied into the model\'s folder',
    fs.existsSync(path.join(dir, 'changed.par')), '');

  await cdp.eval(`[...document.querySelectorAll('[data-section="ic"] .value-tools button')].find(b => b.textContent === 'Save').click()`);
  await until("s.files.offered && s.files.offered.name.endsWith('.ic') && !s.busy", 'ic saved');
  const icName = await S('s.files.offered.name');
  const icText = fs.readFileSync(path.join(dir, icName), 'utf8').replace(/\r\n/g, '\n');
  check('runs: IC Save is the values alone, one per node variable (V, W: 2 lines)',
    icText.trim().split('\n').length === 2, icText);
  const icFile = path.join(dir, 'changed.ic');
  fs.writeFileSync(icFile, '-0.25\n0.1\n');
  await pickFiles('#values-load-ic', [icFile]);
  check('runs: IC Load applies at once too',
    await until('!s.busy && s.core.ics[0][1] === -0.25 && s.core.ics[1][1] === 0.1', 'ic load applied'),
    await S('s.core.ics'));

  const badPar = path.join(dir, 'bad.par');
  fs.writeFileSync(badPar, '3   Number params\n1\n2\n3\n');
  await pickFiles('#values-load-par', [badPar]);
  check('runs: a par file with the wrong count is refused with the core\'s own message',
    await until("s.bottom && /bad.par:1: it is for 3 parameters, the model has 12/.test(s.bottom)", 'bad par message'), await S('s.bottom'));
  await closeErrors();

  const badIc = path.join(dir, 'bad.ic');
  fs.writeFileSync(badIc, '-0.1\n');
  await pickFiles('#values-load-ic', [badIc]);
  check('runs: an ic file with too few values is refused with the core\'s own message',
    await until("s.bottom && /bad.ic:2: the file ends here/.test(s.bottom)", 'bad ic message'), await S('s.bottom'));
  await closeErrors();

  /* a slider: its default range is [0, 2v]; a drag sends its values as
     sets (W106), and the release leaves its final value in the core */
  const iapp = await S('s.core.pars.find(p => p[0] === "iapp")[1]');
  const sid = await addSlider('iapp');
  const sdef = await S(`s.values.sliders.find(d => d.id === ${sid})`);
  check('runs: a picked slider ranges over [0, 2v], with a positive step',
    sdef && sdef.name === 'iapp' && sdef.lo === '0' && sdef.hi === String(2 * iapp) && Number(sdef.step) > 0,
    JSON.stringify(sdef));
  const track = await cdp.eval(`(() => { const r = document.getElementById('slider-range-${sid}').getBoundingClientRect();
    return {x: r.left, y: r.top + r.height / 2, w: r.width}; })()`);
  const sent1 = await cdp.eval('__xpp.sentCount()');
  const nSlide = await S('s.seriesCount');
  await mouse('mousePressed', track.x + track.w * 0.5, track.y, {button: 'left', clickCount: 1});
  for (let k = 1; k <= 10; k++) await mouse('mouseMoved', track.x + track.w * (0.5 + k * 0.04), track.y, {button: 'left'});
  await mouse('mouseReleased', track.x + track.w * 0.9, track.y, {button: 'left', clickCount: 1});
  await until(`!s.busy && !s.values.inflight.length && __xpp.sentCount() > ${sent1}`, 'drag settles');
  const out = await cdp.eval(`__xpp.sentFrom(${sent1})`);
  const draggedTo = Number(out[out.length - 1].text);
  check('runs: a slider drag sends its values as sets, runs nothing, and the core has its final value',
    out.every(c => c.cmd === 'set') && Math.abs(draggedTo - 0.9 * 2 * iapp) < 2 * iapp * 0.05
    && (await S('s.seriesCount')) === nSlide && (await S(`Math.abs(s.core.pars.find(p => p[0] === "iapp")[1] - ${draggedTo}) < 1e-9`)),
    JSON.stringify({out, draggedTo}));
  await focusPlot();
  await key('i');
  await until("s.ask && s.ask.kind === 'menu'", 'Initialconds menu (slider)');
  const preGo = await cdp.eval('__xpp.sentCount()');
  await goRun();
  const final = await S('s.core.pars.find(p => p[0] === "iapp")[1]');
  check('runs: Go runs with the slider\'s value, no set of its own',
    Math.abs(final - draggedTo) < 1e-9 && !(await cdp.eval(`__xpp.sentFrom(${preGo}).some(c => c.cmd === 'set')`)),
    JSON.stringify({final, draggedTo}));

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

  /* Help > About: hello's about text (core/xpp_about.cpp), shown from Help's About button */
  await cdp.eval(`document.querySelector('.help-about-toggle').click()`);
  check('help: the About button shows hello.about (author, email, issues URL)', await until(`(() => {
    const t = document.querySelector('.help-about')?.textContent ?? '';
    return s.help.about && t === s.hello.about.split(String.fromCharCode(10)).join('') && t.includes('Muhammad Ahmad')
      && t.includes('muhammadmoustafa22@gmail.com') && t.includes('https://github.com/MuhammadMoustafa/xppautX/issues');
  })()`, 'about shown'), JSON.stringify(await S('s.help')));
  await cdp.eval(`[...document.querySelectorAll('.help-toc-item')][0].click()`);
  check('help: a chapter link leaves About', await until(`!s.help.about && !document.querySelector('.help-about')`, 'about left'));

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

/* W170: heavy.odex's expensive RHS with widely spaced stored rows leaves
   the integration computing through browser round trips. Stop only after
   checking every edit's commit happened while it was actually computing. */
async function valuesBusy() {
  const BUSY_TOTAL = 1e7; /* the run cannot reach its end while the page edits */
  const BUSY_JUMP = 50000; /* expensive heavy.odex steps between rows prevent filling storage during edits */
  const BUSY_TIMEOUT_MS = 60000; /* the expensive first stored interval needs the values section's safety ceiling */
  await desktopMetrics();
  /* All heavy.odex ICs are zero: RK4 keeps that equilibrium exactly, and
     unlike Stiff it honours nOutput instead of resetting it to one. */
  await cdp.eval(`__xpp.send({cmd: 'set', values: [
    {kind: 'num', name: 'method', text: 'rk4'},
    {kind: 'num', name: 'total', value: ${BUSY_TOTAL}},
    {kind: 'num', name: 'nout', value: ${BUSY_JUMP}}]})`);
  if (!await until(`!s.busy && s.numerics.find(f => f.key === 'total')?.value === ${BUSY_TOTAL}
    && s.numerics.find(f => f.key === 'nout')?.value === ${BUSY_JUMP}`, 'heavy integration settings'))
    throw new Error('heavy.odex: heavy integration settings were not applied');
  const start = async () => {
    await focusPlot();
    await key('i');
    await until("s.ask && s.ask.kind === 'menu'", 'heavy Integrate menu');
    await key('g');
    check('values: the heavy run is computing before editing',
      await until('s.busy && s.computing && w.series?.rows >= 2', 'heavy integration computing', BUSY_TIMEOUT_MS));
  };
  await start();
  const sent0 = await S('__xpp.sentCount()');
  const busyEdits = [];
  for (const v of ['-0.99', '-0.98', '-0.97', '-0.96', '-0.95']) busyEdits.push(await editField('par', 'mu', v));
  const sentDuring = await S(`__xpp.sentFrom(${sent0})`);
  const shownDuring = await cdp.eval(`${fieldOf('par', 'mu')}.querySelector('input').value`);
  const coreDuring = await S('s.core.pars.find(p => p[0] === "mu")[1]');
  const wasBusy = await S('s.busy && !!s.computing');
  const noMark = !(await cdp.eval(`${fieldOf('par', 'mu')}.classList.contains('queued') || !!document.querySelector('.values-queued')`));
  check('values: five edits while busy are sent at once (five sets), the field shows the last, no pending mark',
    busyEdits.every(Boolean) && wasBusy && sentDuring.length === 5
    && sentDuring.every(c => c.cmd === 'set' && c.name === 'mu')
    && sentDuring[4].text === '-0.95' && shownDuring === '-0.95' && noMark,
    JSON.stringify({busyEdits, wasBusy, sentDuring, shownDuring, noMark}));
  await focusPlot();
  await key('Escape');
  check('values: the run kept the value it started with; the core has the last edit once it ends',
    coreDuring === -1 && await until(`!s.busy && !s.values.inflight.length
      && s.core.pars.find(p => p[0] === 'mu')[1] === -0.95`, 'heavy edits applied after Stop'),
    JSON.stringify({coreDuring, now: await S('[s.values.inflight, s.core.pars]')}));

  const sid = await addSlider('mu');
  await start();
  const track = await cdp.eval(`(() => { const r = document.getElementById('slider-range-${sid}').getBoundingClientRect();
    return {x: r.left, y: r.top + r.height / 2, w: r.width}; })()`);
  const sent1 = await S('__xpp.sentCount()');
  const busyDrag = await S('s.busy && !!s.computing');
  await mouse('mousePressed', track.x + track.w * 0.3, track.y, {button: 'left', clickCount: 1});
  for (let k = 1; k <= 10; k++) await mouse('mouseMoved', track.x + track.w * (0.3 + k * 0.05), track.y, {button: 'left'});
  await mouse('mouseReleased', track.x + track.w * 0.8, track.y, {button: 'left', clickCount: 1});
  const busyThen = await S('s.busy && !!s.computing');
  const out = await S(`__xpp.sentFrom(${sent1})`);
  const draggedTo = out.length ? Number(out[out.length - 1].text) : null;
  await focusPlot();
  await key('Escape');
  check('values: a slider dragged during a run sends its values as sets at once; the core has the last after the run',
    busyDrag && busyThen && out.length >= 1 && out.every(c => c.cmd === 'set')
    && await until(`!s.busy && !s.values.inflight.length
      && Math.abs(s.core.pars.find(p => p[0] === 'mu')[1] - ${draggedTo}) < 1e-9`, 'heavy slider edits after Stop'),
    JSON.stringify({busyDrag, busyThen, out}));
}

/* live.odex: the IC fields do not move during a run, Now does. */
async function valuesLive() {
  await desktopMetrics();
  /* the first run ends the wait: slow on a loaded machine, so the section's
     long timeout, and nothing below means anything without it */
  const connected = await until('s.hello && s.seriesCount >= 1 && !s.busy', 'hello', 60000);
  check('values: the page connects', connected);
  if (!connected) return;
  const ics0 = await S(icsOf), fields0 = await icFields();
  /* the ICs sampled every frame (they must not change); Now recorded at
     every change of its text while the run goes, whatever the frame rate
     (macos-ui drew too few frames during the run to see it move, W93) */
  await cdp.eval(`window.__icSeen = []; window.__nowSeen = []; window.__sampling = true;
    (function tick() { const s = __xpp.state();
      if (s.busy) __icSeen.push(JSON.stringify(s.core.ics.map(p => p[1])) + [...document.querySelectorAll('[data-section="ic"] .value-field input')].map(i => i.value).join());
      if (window.__sampling) requestAnimationFrame(tick); })();
    const nowText = () => [...document.querySelectorAll('.value-now')].map(o => o.textContent).join();
    window.__nowObserver = new MutationObserver(() => { if (window.__sampling && __xpp.state().busy) __nowSeen.push(nowText()); });
    __nowObserver.observe(document.body,
      {subtree: true, childList: true, characterData: true});
    __nowSeen.push(nowText()); true`);
  await focusPlot();
  await key('i');
  await until("s.ask && s.ask.kind === 'menu'", 'menu');
  await key('g');
  await until('s.busy && w.series && w.series.rows > 100', 'running');
  await until('!s.busy && w.series.rows === 20001', 'first run', 60000);
  await rendered();
  await cdp.eval('window.__sampling = false; window.__nowObserver.disconnect(); true');
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

  /* T35d: a parameter box takes a number or %formula only, and refuses outright, while typed, a
     keystroke or a paste that would leave a text it does not take and is not on the way to one
     it does (fieldAcceptsEdit): the box's own text never changes, so it is not marked invalid,
     only a brief hint says why; nothing is sent. */
  await until('!s.busy', 'idle', 60000);
  await editField('par', 'iapp', '1');
  const iapp0 = await S('s.core.pars.find(p => p[0] === "iapp")[1]');
  const sent2 = await cdp.eval('__xpp.sentCount()');
  /* a whole paste over a selection, none of it sharing a prefix with what was there: "0.05abc"
     itself is named, since all of it is what was pasted */
  await typeIntoField('par', 'iapp', '0.05abc');
  let box = await fieldState('par', 'iapp');
  check('T35d: pasting "0.05abc" over the box\'s text lands none of it: the box reads 1, unmarked, and names it whole',
    box.value === '1' && box.invalid === null
    && box.message === 'Pasted "0.05abc" is not a number or formula ("a" at character 5)', JSON.stringify(box));
  await editField('par', 'iapp', '0.05');
  await typeIntoField('par', 'iapp', '0.05a');
  box = await fieldState('par', 'iapp');
  check('T35d: a refused keystroke ("a" after 0.05) never lands: the box still reads 0.05, unmarked, with a hint',
    box.value === '0.05' && box.invalid === null && box.message === '"a" can\'t go in a number or formula', JSON.stringify(box));
  /* the box already reads "0.05": only "abc" is new, so that is what is named (not the whole
     resulting text, which was never actually typed or pasted) */
  await typeIntoField('par', 'iapp', '0.05abc');
  box = await fieldState('par', 'iapp');
  check('T35d: a refused paste ("abc" appended at once) lands none of it: the box reads 0.05, unmarked, and names the offender',
    box.value === '0.05' && box.invalid === null
    && box.message === 'Pasted "abc" is not a number or formula ("a" at character 1)', JSON.stringify(box));
  const onlyEdit = () => cdp.eval(`(() => { const l = __xpp.sentFrom(${sent2});
    return l.length === 1 && l[0].cmd === 'set' && l[0].text === '0.05'; })()`);
  check('T35d: a refused keystroke/paste sends nothing (the one set is the 0.05 committed between them)', await onlyEdit(),
    JSON.stringify(await cdp.eval(`__xpp.sentFrom(${sent2})`)));

  /* half-typed and left ("1e-" is on the way to a number while typed, so no message while it is
     focused): a blur marks it, keeps it (never reverted, never sent) and says what is missing */
  await typeIntoField('par', 'iapp', '1e-');
  await cdp.eval(`${fieldOf('par', 'iapp')}.querySelector('input').blur()`);
  await rendered();
  box = await fieldState('par', 'iapp');
  check('T35d: "1e-" left half-typed is marked, kept, and says what is missing',
    box.value === '1e-' && box.invalid === 'true' && box.message === '"1e-" needs an exponent\'s digits', JSON.stringify(box));
  check('T35d: a half-typed number is never sent', await onlyEdit());

  /* Escape belongs to the field: it drops the half-typed text, wherever the focus went */
  await cdp.eval(`${fieldOf('par', 'iapp')}.querySelector('input').focus()`);
  await key('Escape');
  await rendered();
  let dropped = await fieldState('par', 'iapp');
  /* the value it goes back to is the one committed above (0.05: sent at once, W106), which the core has */
  const coreIapp = await S('s.core.pars.find(p => p[0] === "iapp")[1]');
  check('T35d: Escape drops the half-typed text: the box shows the value again, unmarked',
    dropped.invalid === null && close6(coreIapp, 0.05) && close6(Number(dropped.value), 0.05),
    JSON.stringify({dropped, coreIapp, iapp0}));

  /* WF-001: a %formula the box takes (its own rules say nothing against it) can still be one the
     core refuses (an unknown symbol): the box keeps showing what was sent, marked, with the
     core's own message, while it is still waiting and after it is refused -- never reverted to
     the old value behind the user's back. Escape then drops it, back to what the core has.
     W106: the edit is sent at once, so the refusal comes at once too. */
  await editField('par', 'iapp', '%0.02');
  await until('Math.abs(s.core.pars.find(p => p[0] === "iapp")[1] - 0.02) < 1e-12', 'iapp = %0.02', 60000);
  await until('!s.busy', 'idle after %0.02', 60000);
  const sent3 = await cdp.eval('__xpp.sentCount()');
  await cdp.eval(`(() => { const el = ${fieldOf('par', 'iapp')}.querySelector('input');
    el.focus(); el.value = '%bogus_symbol_zzz'; el.dispatchEvent(new Event('input', {bubbles: true}));
    /* Enter in the same task, before Preact renders the keystroke: the commit must still be of the
       text typed (a runner slow enough to run the keystroke and the Enter apart is the same case) */
    el.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter', bubbles: true, cancelable: true})); })()`);
  const refused = await until(`s.values.errors['par:iapp']`, 'the core refuses %bogus_symbol_zzz', 60000);
  box = await fieldState('par', 'iapp');
  check('WF-001: a formula the core refuses keeps its draft, marked, with the core\'s message, and takes the focus back',
    refused && box.value === '%bogus_symbol_zzz' && box.invalid === 'true' && !!box.message && box.focused
    && Math.abs((await S('s.core.pars.find(p => p[0] === "iapp")[1]')) - 0.02) < 1e-12,
    JSON.stringify({box, refused}));
  await key('Escape');
  await rendered();
  dropped = await fieldState('par', 'iapp');
  check('WF-001: Escape drops the refused formula: the field is valid again and shows what the core has',
    dropped.invalid === null && close6(Number(dropped.value), 0.02) && !(await S(`s.values.errors['par:iapp']`)),
    JSON.stringify(dropped));
  check('WF-001: only the one formula the core refused was sent, as one set',
    (await cdp.eval(`__xpp.sentFrom(${sent3}).filter(c => c.cmd === 'set')`)).length === 1,
    JSON.stringify(await cdp.eval(`__xpp.sentFrom(${sent3})`)));
  await until('!s.busy', 'idle after the refusal', 60000);

  /* a %formula the core takes is sent as typed, at once, and the core evaluates it */
  const sent4 = await cdp.eval('__xpp.sentCount()');
  await editField('par', 'iapp', '%0.01*6');
  check('a %formula in a parameter box is sent as typed and the core evaluates it',
    await until('!s.busy && Math.abs(s.core.pars.find(p => p[0] === "iapp")[1] - 0.06) < 1e-12', 'iapp = %0.01*6', 60000)
    && (await cdp.eval(`__xpp.sentFrom(${sent4})`)).some(c => c.cmd === 'set' && c.text === '%0.01*6'),
    JSON.stringify(await cdp.eval(`__xpp.sentFrom(${sent4})`)));

  /* W131: a set of several values is all or nothing, and its error shows on the field it
     belongs to (the core names it), not on the first value's */
  await until('!s.busy', 'idle before the mixed set', 60000);
  const parOf = n => S(`s.core.pars.find(p => p[0] === ${JSON.stringify(n)})[1]`);
  const iapp5 = await parOf('iapp'), gca5 = await parOf('gca');
  await cdp.eval(`__xpp.send({cmd: 'set', values: [{kind: 'par', name: 'iapp', text: '0.21'},
    {kind: 'par', name: 'gca', text: '%bogus_symbol_zzz'}, {kind: 'par', name: 'phi', text: '0.4'}]})`);
  await until('!s.busy', 'idle after the mixed set', 60000);
  check('W131: a set with one bad value applies none of the three',
    close6(await parOf('iapp'), iapp5) && close6(await parOf('gca'), gca5) && close6(await parOf('phi'), 0.333),
    JSON.stringify([await parOf('iapp'), await parOf('gca'), await parOf('phi')]));
  /* the page's own edit path: the error lands on its field */
  await editField('par', 'gca', '%bogus_symbol_zzz');
  const refusedGca = await until(`s.values.errors['par:gca']`, 'the core refuses a formula for gca', 60000);
  check('W131: a refusal shows on its own field, the core naming it, and on no other',
    !!refusedGca && /gca/.test(await S(`s.values.errors['par:gca']`)) && Object.keys(await S('s.values.errors')).join() === 'par:gca',
    JSON.stringify(await S('s.values.errors')));
  await key('Escape');
  await rendered();

  /* Reset all sends the core's default command, once, and no set */
  await editField('par', 'iapp', '0.3');
  await until(`!s.busy && Math.abs(s.core.pars.find(p => p[0] === "iapp")[1] - 0.3) < 1e-12`, 'iapp = 0.3', 60000);
  const preAll = await cdp.eval('__xpp.sentCount()');
  await cdp.eval(`[...document.querySelectorAll('[data-section="par"] button')].find(b => b.textContent.trim() === 'Reset all').click()`);
  await until('!s.busy && Math.abs(s.core.pars.find(p => p[0] === "iapp")[1] - 0.05) < 1e-12', 'Reset all applied', 60000);
  const allSent = await cdp.eval(`__xpp.sentFrom(${preAll})`);
  check('W131: Reset all sends one default command for the parameters and no set',
    allSent.filter(c => c.cmd === 'default' && c.kind === 'par').length === 1 && !allSent.some(c => c.cmd === 'set'),
    JSON.stringify(allSent));
}

/* tools/models/live.odex: 20 001 rows in about two seconds */
async function live(want) {
  await desktopMetrics();
  check('live: the page connects', await until('s.hello && s.seriesCount >= 1 && !s.busy', 'hello'));
  const a0 = await S('s.seriesAppends');
  await startSampling();
  const done = await integrate(20001, 60000);
  const seen = await stopSampling(), idleAt = await cdp.eval('performance.now()');
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
  await runPacing('live run', seen, idleAt, appends);
}

/* How smoothly the plot follows a run (W82), as perf lines named `label`:
   from the first frame the page saw the run busy (startSampling's samples)
   to the first it saw it idle again (or `idleAt`, when the idle came after
   the last sampled frame), the gaps between animation frames (p50, p90,
   max), the long tasks, and the series appends the page took per second.
   Measured, never failed (W58). */
async function runPacing(label, seen, idleAt, appends) {
  const start = seen.findIndex(([, , b]) => b);
  if (start < 0) { console.log(`  ${label}: the page never saw it busy`); return; }
  const end = seen.findIndex(([, , b], i) => i > start && !b);
  const t0 = seen[start][3], t1 = end < 0 ? idleAt : seen[end][3];
  const gaps = await cdp.eval(`(() => { const f = window.__xppPerf.frames.filter(t => t >= ${t0} && t <= ${t1});
    const g = []; for (let i = 1; i < f.length; i++) g.push(f[i] - f[i - 1]); return g; })()`);
  const long = (await longTasksSince(t0)).filter(t => t.start < t1);
  console.log(`  ${label}: ${ms(t1 - t0)} busy, ${gaps.length} frame gaps, ${appends} appends, `
    + `long tasks ${JSON.stringify(long.map(t => Math.round(t.duration)))}`);
  perf(`${label} frame gap p50`, gaps.length ? ms(pct(gaps, 0.5)) : 'n/a');
  perf(`${label} frame gap p90`, p90(gaps));
  perf(`${label} frame gap max`, ms(Math.max(0, ...gaps)));
  perf(`${label} long tasks`, `${long.length} (${ms(long.reduce((x, t) => x + t.duration, 0))} in all)`);
  perf(`${label} series appends per second`, (appends / ((t1 - t0) / 1000)).toFixed(1));
}

const pct = (a, q) => a.length ? [...a].sort((x, y) => x - y)[Math.min(a.length - 1, Math.floor(q * a.length))] : NaN;
const ms = v => `${v.toFixed(1)} ms`;
/* Frame draw times and long tasks are measurements, printed as perf: lines
   (W58: performance is for CI, not the program, and a check that races the
   clock is a design problem), read from outside through CDP: tools/cdp.mjs's
   installPerfObserver injects a PerformanceObserver (long tasks) and a
   requestAnimationFrame sampler into the page before it loads, into
   window.__xppPerf, which the app itself never reads. A frame's own draw
   time is not observable from outside without the app's cooperation, so the
   gap before the next animation frame stands in for it: a synchronous draw
   that runs long delays the next rAF callback by about as much. The 90th
   percentile is still the number reported (not the max): a slow, unsteady
   runner (macos-ui, W40, GitHub #83) drew one frame of a handful well past
   the rest, and the median alone would hide that a whole run's frames, not
   just one, had gone slow. */
const p90 = frames => frames.length ? ms(pct(frames, 0.9)) : 'n/a';

/** long tasks that started at or after t0 (performance.now() ms) */
const longTasksSince = t0 => cdp.eval(`window.__xppPerf.longTasks.filter(t => t.start >= ${t0})`);
/** the gaps (ms) between consecutive animation frames from t0 on: a proxy
    for how long each frame's drawing took (see above) */
const frameGapsSince = t0 => cdp.eval(`(() => { const f = window.__xppPerf.frames.filter(t => t >= ${t0});
  const g = []; for (let i = 1; i < f.length; i++) g.push(f[i] - f[i - 1]); return g; })()`);

/** wheel zooms in and out about the middle of the plot; the long tasks and draw times they cost */
async function zoomFrames() {
  const a = await area(), cx = a.x + a.w * 0.55, cy = a.y + a.h * 0.45;
  await rendered();
  const t0 = await cdp.eval('performance.now()'), d0 = (await P()).draws;
  const v0 = await S('w.viewport');
  for (const dy of [-120, -120, -120, 120, 120, 120, 120]) {
    const beforeWheel = await S('w.viewport');
    await mouse('mouseWheel', cx, cy, {deltaX: 0, deltaY: dy});
    await until(`JSON.stringify(w.viewport) !== ${JSON.stringify(JSON.stringify(beforeWheel))}`, 'this wheel viewport');
    await rendered();
  }
  await rendered();
  await until('!__xpp.plot().tracing', 'tracing', 5000);
  const t1 = await cdp.eval('performance.now()');
  const p = await P(), draws = p.draws - d0;
  return {
    zoomed: JSON.stringify(v0) !== JSON.stringify(await S('w.viewport')),
    traceMs: t1 - t0,
    vertices: p.vertices,
    long: await longTasksSince(t0),
    frameGaps: await frameGapsSince(t0),
    draws,
  };
}

/* tools/models/million.odex: a phase plane of 1 000 001 points, then the
   same run against time */
async function million() {
  await desktopMetrics();
  check('10^6: the page connects', await until('s.hello && s.seriesCount >= 1 && !s.busy', 'hello')
    && await until('!!__xpp.plot()', 'the chart'));
  const t0 = Date.now(), p0 = await cdp.eval('performance.now()'), d0 = (await P()).draws;
  const a0 = await S('s.seriesAppends');
  await startSampling();
  const done = await integrate(1000001, 300000);
  const seen = await stopSampling(), idleAt = await cdp.eval('performance.now()');
  const secs = (Date.now() - t0) / 1000;
  check(`10^6: I, G stores 1 000 001 rows in the store (${secs.toFixed(1)} s, ${(await S('s.seriesAppends')) - a0} appends)`,
    done, JSON.stringify(await S('w.series && [w.series.rows, s.busy]')));
  if (!done) return;
  await until('!__xpp.plot().tracing', 'tracing', 10000);
  const t1 = await cdp.eval('performance.now()');
  await rendered();
  const p = await P(), frames = await frameGapsSince(p0);
  const load = await longTasksSince(p0);
  /* the long tasks of the run are the arrival of the data (the final full
     series is 16 MB of base64 in one event): reported, not drawing */
  console.log(`  run: ${frames.length} frame gaps, median ${ms(pct(frames, 0.5))}, max ${ms(Math.max(0, ...frames))}; `
    + `long tasks (data arriving) ${JSON.stringify(load.map(t => Math.round(t.duration)))}; tracing settled `
    + `${ms(t1 - p0)} after the run started and keeps ${p.vertices[0]} of the 1 000 001 vertices`);
  check('10^6: the phase plane draws its 1 000 001 points as they come',
    p.mode === 2 && p.curves[0].points === 1000001 && p.draws - d0 > 0
    && p.vertices[0] > 100, JSON.stringify({mode: p.mode, points: p.curves[0].points, draws: p.draws - d0, vertices: p.vertices}));
  perf('10^6 phase plane draw p90', p90(frames));
  await runPacing('10^6 run', seen, idleAt, (await S('s.seriesAppends')) - a0);
  let z = await zoomFrames();
  console.log(`  phase plane zoom: ${z.draws} draws, ${z.frameGaps.length} frame gaps, median ${ms(pct(z.frameGaps, 0.5))}, `
    + `max ${ms(Math.max(0, ...z.frameGaps))}, long tasks ${JSON.stringify(z.long.map(t => Math.round(t.duration)))}; `
    + `the last view's trace took ${ms(z.traceMs)} (${z.vertices[0]} vertices)`);
  check('10^6: a wheel zoom of the phase plane ends traced',
    z.zoomed && z.draws > 0 && z.vertices[0] > 0, JSON.stringify(z));
  perf('10^6 phase plane zoom draw p90', p90(z.frameGaps));
  perf('10^6 phase plane zoom long task max', ms(z.long.length ? Math.max(...z.long.map(t => t.duration)) : 0));

  /* x against time: uPlot's own line with its min and max per pixel column */
  await cdp.eval(`document.querySelector('.plot-view:not([hidden]) .plot-host').focus()`);
  const n0 = await S('s.seriesCount');
  const t2 = await cdp.eval('performance.now()');
  await key('x');
  if (!(await until("s.ask && s.ask.kind === 'string'", 'Xi vs t'))) return check('10^6: X asks what to plot', false);
  await cdp.eval(`__xpp.send({cmd: 'answer', id: __xpp.state().ask.id, value: 'x'})`);
  check('10^6: X plots x against T', await until(`s.seriesCount > ${n0} && !s.busy && w.series.curves[0].x === 0`, 'x vs t', 60000));
  await until('!__xpp.plot().tracing', 'tracing', 10000);
  const t3 = await cdp.eval('performance.now()');
  await rendered();
  const q = await P();
  check('10^6: the time plot draws', q.mode === 1 && q.curves[0].points === 1000001,
    JSON.stringify({mode: q.mode, points: q.curves[0].points}));
  perf('10^6 time plot draw', ms(t3 - t2));
  z = await zoomFrames();
  console.log(`  time plot zoom: ${z.draws} draws, ${z.frameGaps.length} frame gaps, median ${ms(pct(z.frameGaps, 0.5))}, `
    + `max ${ms(Math.max(0, ...z.frameGaps))}, long tasks ${JSON.stringify(z.long.map(t => Math.round(t.duration)))}`);
  check('10^6: a wheel zoom of the time plot', z.zoomed && z.draws > 0, JSON.stringify(z));
  perf('10^6 time plot zoom draw p90', p90(z.frameGaps));
  perf('10^6 time plot zoom long task max', ms(z.long.length ? Math.max(...z.long.map(t => t.duration)) : 0));

  /* the same run again, drawn against time as it comes: uPlot's own line,
     every append a new draw of all the rows so far (W82) */
  await cdp.eval(`document.querySelector('.plot-view:not([hidden]) .plot-host').focus()`);
  const a1 = await S('s.seriesAppends');
  await startSampling();
  const again = await integrate(1000001, 300000);
  const seen2 = await stopSampling(), idle2 = await cdp.eval('performance.now()');
  check('10^6: I, G again draws the rows against time as they come', again
    && (await P()).mode === 1, JSON.stringify(await S('w.series && [w.series.rows, s.busy]')));
  await runPacing('10^6 time plot run', seen2, idle2, (await S('s.seriesAppends')) - a1);
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
  /* the command before has ended (the ask it answered or cancelled gone
     from the page is not that): else the File menu of the last one still
     shows, the wait below passes before `f` is read, and `k` reaches the
     main menu (macos-ui: m, File/open Model, opened Makewindow, W93) */
  await until('!s.busy && !s.ask && s.core.menu !== 1', 'the command before ended');
  await focusPlot();
  await key('f');
  await until('!s.busy && s.core.menu === 1', 'the File menu');
  await key(k);
  return until(`s.ask && s.ask.kind === 'file' && s.ask.mode === '${mode}' && document.querySelector('.file-ask')`, `file ask ${k}`);
}

async function waitFile(p, ms = 10000) {
  return waitFor(() => fs.existsSync(p) && fs.statSync(p).size > 0 && fs.readFileSync(p), ms);
}

/** the .set XPPAUT wrote for an older lecar.odex (examples/ode/lecar.ode.set: iapp 0.09, equations after
    the values), its count of equations and auxiliaries made the model's today */
function xppautLecarSet() {
  const text = fs.readFileSync(path.join(top, 'examples/ode/lecar.ode.set'), 'utf8');
  return Buffer.from(text.replace('4   Number of equations', '6   Number of equations'));
}

async function files(dir) {
  await desktopMetrics();
  await until('!s.busy && !s.ask', 'idle');
  /* a headless browser shows no picker: the page takes its fallbacks, the
     <input type=file> and the download, which a test can drive */
  await cdp.eval('window.showOpenFilePicker = undefined; window.showSaveFilePicker = undefined; true');
  const up = fs.mkdtempSync(path.join(os.tmpdir(), 'xppweb2-up-'));
  /* downloads folder is shared and set by startBrowser; downloads is a global */
  const canDownload = true;
  try {
    const iapp0 = await par('iapp');

    /* File/saVe session: the core writes into the model's folder, the page offers it */
    check('File/saVe session opens a save dialog (the ask says it writes)', await fileMenu('v', 'write'),
      JSON.stringify(await S('s.ask')));
    await cdp.eval(`(() => { const i = document.querySelector('[data-file-name]'); i.value = 't5.snapx';
      i.dispatchEvent(new Event('input', {bubbles: true})); i.focus(); })()`);
    await rendered();
    await key('Enter');
    check('the ask is answered with the name', (await lastFileAnswer())?.file === 't5.snapx', JSON.stringify(await lastFileAnswer()));
    check('W129 browser mode: the file answer carries no model-copy decision',
      !Object.hasOwn(await lastFileAnswer(), 'replace'));
    const offered = await until("s.files.offered && s.files.offered.name === 't5.snapx' && !s.busy", 'offered');
    const saved = fs.existsSync(path.join(dir, 't5.snapx')) ? fs.readFileSync(path.join(dir, 't5.snapx')) : null;
    check('Save session lands in the model\'s folder', saved && saved.length > 100, String(saved && saved.length));
    const off = await S('s.files.offered');
    check('... and is offered to the browser as a download, the same bytes',
      offered && saved && off.how === 'download' && off.size === saved.length && off.sha256 === sha256(saved), JSON.stringify(off));
    if (canDownload) {
      const got = await waitFile(path.join(downloads, 't5.snapx'));
      check('the browser downloaded it', got && saved && got.equals(saved), String(got && got.length));
    }

    /* W129: decline the core's save permission. A prior destination exists,
       but neither the offered-file action nor a second download may occur. */
    const beforeDownloads = fs.readdirSync(downloads);
    acceptSave = false;
    try {
      await fileMenu('v', 'write');
      await cdp.eval(`(() => { const i = document.querySelector('[data-file-name]'); i.value = 't5.snapx';
        i.dispatchEvent(new Event('input', {bubbles: true})); i.focus(); })()`);
      await until("document.querySelector('[data-file-name]').value === 't5.snapx'", 'repeat name');
      await key('Enter');
      check('W129 browser mode: the existing model copy asks once',
        await until("s.ask && s.ask.kind === 'choice' && s.ask.keys === 'yn' && s.ask.question === 't5.snapx exists. Replace it?'", 'save permission'));
      const beforeOffers = await cdp.eval("__xpp.actions().filter(a => a === 'files').length");
      await cdp.eval("__xpp.send({cmd: 'answer', id: __xpp.state().ask.id, key: 'n'})");
      check('W129 browser mode: No to the core question delivers nothing and preserves the model copy',
        await until('!s.busy && !s.ask', 'declined save finished')
        && fs.readFileSync(path.join(dir, 't5.snapx')).equals(saved)
        && beforeOffers === await cdp.eval("__xpp.actions().filter(a => a === 'files').length")
        && JSON.stringify(beforeDownloads) === JSON.stringify(fs.readdirSync(downloads)));
    } finally { acceptSave = true; }

    /* the .set XPPAUT wrote for lecar.odex (its equations after the values): iapp 0.09, the model's 0.05 */
    const setBytes = xppautLecarSet();
    const setIapp = 0.09;
    check('the model\'s iapp is not the set\'s', Math.abs(iapp0 - setIapp) > 1e-3, String(iapp0));
    fs.writeFileSync(path.join(up, 't5up.set'), setBytes);

    /* File/Import XPPAUT set by upload brings in its parameters */
    check('a new parameter value', await setPar('iapp', 0.2));
    check('File/Import XPPAUT set opens an open dialog (the ask says it reads)', await fileMenu('r', 'read'),
      JSON.stringify(await S('s.ask')));
    await pickFiles('[data-file-input=open]', [path.join(up, 't5up.set')]);
    check('Import XPPAUT set by upload brings in the parameters (the next state has the file\'s values)',
      await until(`!s.ask && !s.busy && Math.abs(s.core.pars.find(p => p[0] === 'iapp')[1] - ${setIapp}) < 1e-12`, 'import set'),
      String(await par('iapp')));
    const copied = fs.existsSync(path.join(dir, 't5up.set')) && fs.readFileSync(path.join(dir, 't5up.set'));
    check('the picked file was copied into the model\'s folder and the ask answered with its name',
      copied && copied.equals(setBytes) && (await lastAnswer())?.file === 't5up.set'
      && (await S('s.files.uploads[0].copied')) === true, JSON.stringify(await S('s.files.uploads')));

    /* the same content again: not copied, still answered */
    await setPar('iapp', 0.2);
    await fileMenu('r', 'read');
    await pickFiles('[data-file-input=open]', [path.join(up, 't5up.set')]);
    check('a file already there with the same content is not copied again',
      await until(`!s.ask && !s.busy && s.files.uploads.length === 1 && s.files.uploads[0].copied === false`, 'same')
      && Math.abs(await par('iapp') - setIapp) < 1e-12, JSON.stringify(await S('s.files.uploads')));

    /* the same name with other content: the replace confirm */
    fs.mkdirSync(path.join(up, 'other'));
    const other = path.join(up, 'other', 't5up.set');
    fs.writeFileSync(other, setBytes.toString().replace('0.09  iapp', '0.123  iapp'));
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
      && fs.readFileSync(path.join(dir, 't5up.set')).equals(setBytes) && !fs.existsSync(path.join(dir, 't5up-2.set')));
    await pickFiles('[data-file-input=open]', [other]);
    await until('s.files.confirm', 'confirm again');
    await cdp.eval(`document.querySelector('[data-choice=keep]').click()`);
    check('Keep both copies it as name-2.ext, keeps the old one, and reads the new one',
      await until(`!s.ask && !s.busy && Math.abs(s.core.pars.find(p => p[0] === 'iapp')[1] - 0.123) < 1e-12`, 'keep both')
      && fs.readFileSync(path.join(dir, 't5up.set')).equals(setBytes)
      && fs.readFileSync(path.join(dir, 't5up-2.set')).equals(fs.readFileSync(other))
      && (await lastAnswer())?.file === 't5up-2.set', String(await par('iapp')));

    /* a file the core cannot open: "Add file…" copies it under that name and runs the command again */
    await setPar('iapp', 0.4);
    await fileMenu('r', 'read');
    /* the core answers with a name the folder does not have (the page's picker only ever answers with a copied file) */
    await cdp.eval(`__xpp.send({cmd: 'answer', id: __xpp.state().ask.id, file: 'gone.set'})`);
    check('an answer with a name not in the folder', (await lastAnswer())?.file === 'gone.set');
    check('a file the core cannot open: the notification offers "Add file…"',
      await until(`s.toasts.some(t => t.action && t.action.name === 'gone.set') && document.querySelector('[data-add-file="gone.set"]')`,
        'add file'), JSON.stringify(await S('s.toasts')));
    await pickFiles('[data-file-input=add]', [path.join(up, 't5up.set')]);
    check('Add file… copies it under that name and runs the command again',
      await until(`!s.busy && !s.ask && Math.abs(s.core.pars.find(p => p[0] === 'iapp')[1] - ${setIapp}) < 1e-12
        && !s.toasts.some(t => t.action)`, 'replayed')
      && fs.existsSync(path.join(dir, 'gone.set')) && fs.readFileSync(path.join(dir, 'gone.set')).equals(setBytes),
      JSON.stringify([await par('iapp'), await S('s.toasts'), await cdp.eval('__xpp.sent().slice(-4)')]));
    check('no file dialog is left open', await S('!s.ask'));

    /* W134: Values > Load is an upload like the others: a same-name file with other content
       asks first, in a dialog of its own (no file ask is open), and Cancel copies and reads nothing */
    await cdp.eval(`__xpp.send({cmd: 'values', op: 'write', kind: 'par', name: 'w134.par'})`);
    await until(`!s.busy`, 'w134.par written');
    const parThere = fs.readFileSync(path.join(dir, 'w134.par'));
    const parPicked = path.join(up, 'w134.par');
    fs.writeFileSync(parPicked, parThere.toString().replace(/\n[^\n]*  iapp\r?\n/, '\n0.0777  iapp\n'));
    const sentLoad = await cdp.eval('__xpp.sentCount()');
    await pickFiles('#values-load-par', [parPicked]);
    check('W134: Values > Load over a different file of the same name asks: Replace, Keep both, Cancel',
      await until(`s.files.confirm && s.files.confirm.ask === null && s.files.confirm.name === 'w134.par'
        && document.querySelectorAll('[data-replace-dialog] [data-choice]').length === 3`, 'load confirm'),
      JSON.stringify(await S('s.files.confirm')));
    await cdp.eval(`document.querySelector('[data-replace-dialog] [data-choice=cancel]').click()`);
    check('W134: ... Cancel leaves the folder\'s file untouched and reads nothing',
      await until('!s.files.confirm && !document.querySelector("[data-replace-dialog]")', 'load cancel')
      && fs.readFileSync(path.join(dir, 'w134.par')).equals(parThere)
      && !(await cdp.eval(`__xpp.sentFrom(${sentLoad}).some(c => c.cmd === 'values')`)),
      JSON.stringify(await cdp.eval(`__xpp.sentFrom(${sentLoad})`)));
    await pickFiles('#values-load-par', [parPicked]);
    await until('s.files.confirm', 'load confirm again');
    await cdp.eval(`document.querySelector('[data-replace-dialog] [data-choice=replace]').click()`);
    check('W134: ... Replace copies it over and reads it',
      await until(`!s.busy && Math.abs(s.core.pars.find(p => p[0] === 'iapp')[1] - 0.0777) < 1e-12`, 'load replace')
      && fs.readFileSync(path.join(dir, 'w134.par')).equals(fs.readFileSync(parPicked)), String(await par('iapp')));

    /* a read ask is one prompt (no tabs, no folder listing) that opens the browser's picker filtered by wild */
    await fileMenu('r', 'read');
    check('a read ask has no tabs and no listing, one Choose file… button, focused',
      await cdp.eval(`!document.querySelector('.file-tabs, .file-list, [data-folder-file]')
        && document.querySelector('.file-ask button.primary')?.textContent === 'Choose file…'
        && document.activeElement === document.querySelector('.file-ask button.primary')`));
    check('the hidden input filters by the pattern of the ask',
      await cdp.eval(`(() => { const w = __xpp.state().ask.wild, i = document.querySelector('[data-file-input=open]');
        return !w || w === '*' ? !i.accept : i.accept === w.split('*').join('') })()`), JSON.stringify(await S('s.ask')));
    await key('Escape');
    check('Esc cancels the ask', await until('!s.ask', 'esc'));

    /* W61: File/open Model: the .ode picked is copied into the model's folder and loaded
       here, in place of this model, after the question whether to save it first */
    fs.writeFileSync(path.join(up, 'w61.ode'), "par a=2\ninit x=0.5\nx'=-a*x\n@ total=5\ndone\n");
    check('File/open Model opens an open dialog', await fileMenu('m', 'read'), JSON.stringify(await S('s.ask')));
    await pickFiles('[data-file-input=open]', [path.join(up, 'w61.ode')]);
    check('... then asks whether to save this session first',
      await until("s.ask && s.ask.kind === 'choice' && s.ask.keys === 'sd'", 'save first?'), JSON.stringify(await S('s.ask')));
    await cdp.eval("__xpp.send({cmd: 'answer', id: __xpp.state().ask.id, key: 'd'})");
    check('Don\'t save loads it here: a new hello names it, the state is its own',
      await until("!s.busy && !s.ask && s.hello.file === 'w61.odex' && s.core.pars.length === 1 && s.core.pars[0][0] === 'a'"
        + " && s.core.pars[0][1] === 2 && s.core.ics[0][0] === 'X'", 'opened'),
      JSON.stringify(await S('[s.hello && s.hello.file, s.core.pars, s.core.ics]')));
    check('... conversion names the saved .odex and links its manual section',
      await S("s.bottom.includes('saved as w61.odex') && s.bottomHelp?.chapter === '02-ode-files' && s.bottomHelp?.anchor === 'odex'"));
    check('... and the page asked for the new model\'s plot data, as on a reconnection',
      await S("__xpp.sent().slice(-3).some(c => c.cmd === 'data')"), JSON.stringify(await cdp.eval('__xpp.sent().slice(-3)')));
  } finally {
    fs.rmSync(up, {recursive: true, force: true, maxRetries: 5});
  }
}

/* ---- the desktop window's own file dialog (docs/ui-v2.md section 4, W88) --------- */

/* The window binds window.__xppFileDialog into the page (core/xpp_window.cpp);
   a headless browser has no window, so a stub stands in for the dialog: it
   records what it was asked and answers window.__nativeReply (a path, null
   for Cancel, or a rejection). The OS dialogs themselves are checked by hand. */
async function nativeFiles(dir) {
  await desktopMetrics();
  await until('!s.busy && !s.ask', 'idle');
  const elsewhere = fs.mkdtempSync(path.join(os.tmpdir(), 'xppweb2-native-'));
  const far = path.join(elsewhere, 'w88 native.snapx');
  const farSet = path.join(elsewhere, 'w88 native.set');
  fs.writeFileSync(farSet, xppautLecarSet()); /* XPPAUT's: iapp 0.09 */
  await cdp.eval(`(() => {
    window.__nativeAsked = []; window.__nativeReply = null; window.__fileDialogShown = false;
    window.__xppFileDialog = async o => {
      window.__nativeAsked.push(o);
      if (window.__nativeReply === 'reject') throw new Error('no dialog here');
      return window.__nativeReply;
    };
    new MutationObserver(() => {
      if (document.querySelector('.file-ask, [data-ask=file]')) window.__fileDialogShown = true;
    }).observe(document.body, {childList: true, subtree: true});
    return true; })()`);
  const fileKey = async (k, reply) => {
    await cdp.eval(`window.__nativeReply = ${JSON.stringify(reply)}; true`);
    await focusPlot();
    await key('f');
    await until('!s.busy && s.core.menu === 1', 'the File menu');
    await key(k);
    return until('!s.busy && !s.ask', `file ask ${k} answered`);
  };
  const asked = () => cdp.eval('window.__nativeAsked.slice(-1)[0]');
  try {
    check('a new parameter value', await setPar('iapp', 0.21));
    check('File/saVe session in the window: the ask is answered from the native dialog', await fileKey('v', far));
    const w = await asked();
    check('... asked for a save, filtered by *.snapx (wildExtensions), in the model\'s folder, the name offered',
      w && w.mode === 'write' && w.wild === '*.snapx' && JSON.stringify(w.exts) === '[".snapx"]' && w.dir
      && w.file.endsWith('.snapx') && !/[\\/]/.test(w.file), JSON.stringify(w));
    check('... answered with the full path picked', (await lastFileAnswer())?.file === far, JSON.stringify(await lastFileAnswer()));
    const saved = await waitFile(far);
    check('... and the core wrote it there, nothing in the model\'s folder, nothing offered',
      saved && saved.length > 100 && !fs.existsSync(path.join(dir, 'w88 native.snapx')) && !(await S('s.files.offered')),
      String(saved && saved.length));

    check('File/Import XPPAUT set in the window: answered from the native dialog', await fileKey('r', farSet));
    const r = await asked();
    check('... asked to open, filtered by *.set', r && r.mode === 'read' && JSON.stringify(r.exts) === '[".set"]', JSON.stringify(r));
    check('... the core read it where it is (its iapp is in), nothing copied',
      Math.abs(await par('iapp') - 0.09) < 1e-12 && (await lastFileAnswer())?.file === farSet
      && !fs.existsSync(path.join(dir, 'w88 native.set')) && !(await S('s.files.uploads.length')), String(await par('iapp')));

    check('Cancel in the native dialog cancels the ask', await fileKey('r', null)
      && (await lastAnswer())?.ok === 0 && Math.abs(await par('iapp') - 0.09) < 1e-12, JSON.stringify(await lastAnswer()));
    check('a dialog that cannot open: a notification, and the ask cancelled', await fileKey('r', 'reject')
      && (await lastAnswer())?.ok === 0
      && await until("s.toasts.some(t => t.kind === 'error' && /file dialog/.test(t.text))", 'toast'),
      JSON.stringify(await S('s.toasts')));
    check('no web2 file dialog appeared at any point', !(await cdp.eval('window.__fileDialogShown')));
  } finally {
    fs.rmSync(elsewhere, {recursive: true, force: true, maxRetries: 5});
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
  check('ani: nothing plays by itself (A6): no Go was sent', !(await cdp.eval('__xpp.sent()')).some(c => c.cmd === 'key' && c.win === 'ani' && c.key === 'g'));

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
    && (await cdp.eval('__xpp.sent()')).some(c => c.cmd === 'key' && c.win === 'ani' && c.key === 'g'));
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
  await metrics({width: 1280, height: 600, deviceScaleFactor: 1, mobile: false});
  check('ani: drawn again at another size, the same aspect', await until(`__xpp.ani().height < ${d0.height}`, 'resized')
    && Math.abs((await cdp.eval('__xpp.ani().box.w / __xpp.ani().box.h')) - aspect) < 1e-6,
    JSON.stringify(await cdp.eval('__xpp.ani()')));

  /* a reload: the server shows the new page the animation window again, and the data
     subscription brings the last frame */
  await reloadPage();
  check('ani: after a reload the panel shows the core\'s window and its last frame again',
    await until('s.hello && !s.busy && s.ani.open && s.ani.exists', 'window again') && await aniShown(599),
    JSON.stringify(await S('[s.ani.open, s.ani.exists, s.busy, s.ani.frame && s.ani.frame.pos]')));

  /* a phone: a full-screen sheet, 44px targets, no sideways scroll */
  await metrics({width: 390, height: 844, deviceScaleFactor: 2, mobile: true});
  await cdp.send('Emulation.setTouchEmulationEnabled', {enabled: true, maxTouchPoints: 5});
  await cdp.send('Emulation.setEmulatedMedia', {features: [{name: 'pointer', value: 'coarse'}, {name: 'hover', value: 'none'}]}).catch(() => {});
  await rendered();
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
   pixels: confirms anim.gif, written by the core's own GIF writer
   (core/json_windows.cpp j_movie_make_anigif), has the frames a
   kinescope export should have. */
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
   integrations, playing them, resetting, and the core's own Kinescope
   GIF writer (Make Anigif, core/json_windows.cpp) getting real pixels
   back from a `pixels` ask (session.ts answerPixels) and writing
   anim.gif itself -- both directly (k, m) and through the page's own
   Export GIF button (docs/roadmap.md W66: the page writes no files). */
async function kinescope(dir) {
  await desktopMetrics();
  check('kinescope: the page connects', await until('s.hello && s.seriesCount >= 1 && !s.busy', 'hello'));
  check('kinescope: integrate once (601 rows)', await integrate(601, 30000));

  const openKinescope = async item => {
    /* the command before ended (Make Anigif's runs on after its file is
       written): a k typed while it runs is lost (W93) */
    await until('!s.busy && !s.ask', 'the command before ended');
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

  /* Export GIF (Plots.tsx): the page's own button for Kinescope's Make
     Anigif (k, m), which core/json_windows.cpp's j_movie_make_anigif
     writes as anim.gif itself, asking `pixels` for every captured frame
     (W66: the page built the GIF itself before this task) */
  const animPath = path.join(dir, 'anim.gif');
  fs.rmSync(animPath, {force: true});
  await cdp.eval(`document.querySelector('.kinescope-bar button[title^="Kinescope/Make AniGif"]').click()`);
  check('kinescope: Export GIF offers anim.gif, written by the core',
    await until("s.files.offered && s.files.offered.name === 'anim.gif' && !s.busy", 'gif offered'));
  await waitFile(animPath);
  if (fs.existsSync(animPath)) {
    const gif = parseGifStructure(fs.readFileSync(animPath));
    check('kinescope: the GIF has one frame per capture, all the same size',
      gif.frames.length === 2 && gif.frames.every(f => f.w === gif.w && f.h === gif.h && f.w > 0 && f.h > 0),
      JSON.stringify(gif));
  } else check('kinescope: the GIF has one frame per capture, all the same size', false, 'anim.gif missing');

  /* the frames shown are read from the page's record of every change
     (__xpp.kinescopeShown), not polled: a poll on a slow runner could miss
     frame 0 (W94), and the play's end is its own state, not a clock */
  const shownFrom = (await cdp.eval('__xpp.kinescopeShown()')).length;
  const playOk = await openKinescope('p') && await until(`!s.kinescope.playing && !s.busy
    && __xpp.kinescopeShown().length >= ${shownFrom + 2}`, 'play done', 30000);
  const shownSeq = (await cdp.eval('__xpp.kinescopeShown()')).slice(shownFrom);
  check('kinescope: Playback (k, p) shows frame 1 then frame 2', playOk
    && JSON.stringify(shownSeq.filter(x => x !== null)) === '[0,1]',
    JSON.stringify({shown: shownSeq, kinescope: await S('({playing: s.kinescope.playing, shown: s.kinescope.shown, frames: s.kinescope.frames.length})'),
      busy: await S('s.busy'), sent: await cdp.eval('__xpp.sent().slice(-3)')}));

  /* Make Anigif (k, m) directly, the raw protocol path the Export GIF
     button above also drives: no prompt, so a plain menu pick */
  fs.rmSync(animPath, {force: true});
  check('kinescope: Make Anigif (k, m) runs', await openKinescope('m') && await until('!s.busy', 'anigif done'));
  await waitFile(animPath);
  check('kinescope: it wrote anim.gif from the pixels answer (a real GIF, not empty)',
    fs.existsSync(animPath) && fs.readFileSync(animPath).length > 20
    && fs.readFileSync(animPath).toString('ascii', 0, 3) === 'GIF',
    fs.existsSync(animPath) ? String(fs.statSync(animPath).size) : 'missing');

  check('kinescope: Reset (k, r) empties the frames', await openKinescope('r')
    && await until('s.kinescope.frames.length === 0 && s.kinescope.shown === null && !s.busy', 'reset'));
  await desktopMetrics();
}

/* one attempt of a session: xppautX in browser mode on a copy of `ode`, the
   page at /, then `fn` (given the model's folder). `record` collects every
   check() call made during it rather than printing them (session() below
   decides, once it knows whether a rerun is needed, what to print). */
/* A model that does not load (W63c): no session() -- there is no hello and
   the core has exited -- but the page still gets the `error` event (and the
   exit) when it connects, and shows where and why. */
async function loadErrorCheck() {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'xppweb2-bad-'));
  fs.writeFileSync(path.join(dir, 'bad.ode'), ['# a model that does not load', 'par a=1', "x'=-x+a*", 'init x=1', 'done', ''].join('\n'));
  const server = await startServer(bin, dir, ['bad.ode']);
  try {
    await cdp.eval('window.__left = true').catch(() => {});
    await cdp.send('Page.navigate', {url: server.url});
    check('load error: the page gets the error event and the exit',
      await until('!window.__left && s.loadError && s.exited !== null', 'the load error', 30000 * SLOW));
    const e = await S('s.loadError');
    check('load error: its file, line, line as written and cause',
      !!e && e.file === 'bad.ode' && e.line === 3 && e.col === 0 && e.source === "x'=-x+a*"
        && e.error.includes("ERROR compiling X'"), JSON.stringify(e));
    check('load error: no hello', await S('s.hello === null'));
    const text = await cdp.eval("(document.querySelector('.load-error') || {}).innerText || ''");
    check('load error: the page shows the place, the line and the cause',
      text.includes('bad.ode, line 3') && text.includes("x'=-x+a*") && text.includes("ERROR compiling X'"), JSON.stringify(text));
  } finally {
    await stopServer(server);

    fs.rmSync(dir, {recursive: true, force: true, maxRetries: 5});
  }
}

/* W104: an error is one dialog with OK (Enter or Escape close it), kept in Messages; a warning
   flashes the status bar and opens no dialog */
async function errorDialogCheck(dir) {
  const bad = () => cdp.eval(`__xpp.send({cmd: 'set', kind: 'par', name: 'iapp', text: '%('})`);
  const dialog = () => cdp.eval(`(() => { const d = document.querySelector('.error-dialog');
    return d && {n: d.querySelectorAll('[data-error]').length, text: d.innerText, count: document.querySelectorAll('.error-dialog').length}; })()`);
  await bad();
  check("error dialog: a refused value opens the dialog with the core's text",
    await until(`document.querySelector('.error-dialog [data-error]')`, 'dialog') && /set par iapp: Illegal formula ../.test((await dialog()).text),
    JSON.stringify(await dialog()));
  check('error dialog: focus is on OK', await cdp.eval(`document.activeElement && document.activeElement.hasAttribute('data-error-ok')`));
  await bad();
  await until(`__xpp.log().filter(l => l.kind === 'error' && l.text === 'set par iapp: Illegal formula ..').length >= 2`, 'second error');
  const two = await dialog();
  check('error dialog: two errors before OK are one dialog listing both',
    two.count === 1 && two.n >= 2 && (two.text.match(/set par iapp: Illegal formula ../g) || []).length >= 2, JSON.stringify(two));
  await key('Enter');
  check('error dialog: Enter closes it, nothing left',
    await until(`!document.querySelector('.error-dialog') && !s.toasts.some(t => t.kind === 'error')`, 'closed'));
  check('error dialog: the errors stay in the Messages list',
    (await S(`__xpp.log().filter(l => l.kind === 'error' && l.text === 'set par iapp: Illegal formula ..').length`)) >= 2);
  await bad();
  await until(`document.querySelector('.error-dialog')`, 'dialog again');
  await key('Escape');
  check('error dialog: Escape closes it too', await until(`!document.querySelector('.error-dialog')`, 'closed by Escape'));
  check('error dialog: the session is usable after it (idle, no ask)', await until('!s.busy && !s.ask', 'idle'));

  /* W140: an error the core places shows its file and line, and the line as written */
  fs.writeFileSync(path.join(dir, 'w140bad.par'), '3   Number params' + String.fromCharCode(10) + '1' + String.fromCharCode(10));
  await cdp.eval(`__xpp.send({cmd: 'values', op: 'read', kind: 'par', name: 'w140bad.par'})`);
  await until(`document.querySelector('.error-dialog [data-error-place]')`, 'a placed error');
  const placed = await S(`s.toasts.filter(t => t.kind === 'error').pop()`);
  check('error dialog: a placed error keeps its file, line and line as written (W140)',
    !!placed && placed.place && placed.place.file === 'w140bad.par' && placed.place.line === 1
      && placed.place.source === '3   Number params' && !placed.action, JSON.stringify(placed));
  const shown = await cdp.eval(`(() => { const d = document.querySelector('.error-dialog');
    return d && {place: (d.querySelector('[data-error-place]') || {}).innerText, source: (d.querySelector('[data-error-source]') || {}).innerText}; })()`);
  check('error dialog: it shows the file and line, and the line as written (W140)',
    !!shown && /w140bad\.par, line 1/.test(shown.place || '') && (shown.source || '').includes('3   Number params'), JSON.stringify(shown));
  check('error dialog: Messages read file:line: what (W140)',
    (await S(`__xpp.log().filter(l => l.kind === 'error').pop().text`)) === 'w140bad.par:1: it is for 3 parameters, the model has 12');
  await key('Enter');
  await until(`!document.querySelector('.error-dialog')`, 'closed');
}

async function warningFlashCheck() {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'xppweb2-warn-'));
  fs.copyFileSync(ODE, path.join(dir, path.basename(ODE)));
  /* Startup INFO logging exercises the same log event and status flash. */
  const server = await startServer(bin, dir, [path.basename(ODE), '--verbose']);
  try {
    await cdp.eval('window.__left = true').catch(() => {});
    await cdp.send('Page.navigate', {url: server.url});
    await until('!window.__left && s.hello && s.core && !s.busy', 'the new page', 60000);
    check('warning: the status bar flashes (state), and no dialog opens',
      await until('s.flash > 0', 'flash') && !(await cdp.eval(`!!document.querySelector('.error-dialog')`))
      && !(await S(`s.toasts.length`)), JSON.stringify(await S('[s.flash, __xpp.log().slice(-3)]')));
  } finally {
    await stopServer(server);

    fs.rmSync(dir, {recursive: true, force: true, maxRetries: 5});
  }
}

async function sessionAttempt(ode, fn, expected, attempts) {
  const rec = record = [];
  attempts.push(rec); /* its lost commands (rec.lost) count even if it throws */
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'xppweb2-'));
  fs.copyFileSync(ode, path.join(dir, path.basename(ode)));
  const server = sessionServer = await startServer(bin, dir, [path.basename(ode)]);
  try {
    /* the new page, connected and idle, before `fn`: a check run between the navigation and the
       new document's first state read the last page's store, and a key typed then was lost (W20) */
    await cdp.eval('window.__left = true').catch(() => {});
    await cdp.send('Page.navigate', {url: server.url});
    await until('!window.__left && s.hello && s.core && !s.busy', 'the new page', 60000);
    await fn(dir);
    const errors = (await S('__xpp.log().filter(l => l.kind === "error").map(l => l.text)')).filter(e => !expected.includes(e));
    check(`${path.basename(ode)}: no errors reported by the core`, errors.length === 0, JSON.stringify(errors));
  } catch (e) {
    if (e && typeof e === 'object') e.recorded = rec; /* what ran before it threw (thrown()) */
    throw e;
  } finally {
    rec.lost = await lostCommands(expected);
    await stopServer(server);

    fs.rmSync(dir, {recursive: true, force: true, maxRetries: 5});
    record = null;
  }
  return rec;
}

/* xppautX in browser mode on a copy of `ode`, the page at /, then `fn`
   (given the model's folder); the server stops after it. `expected` are
   errors the session provokes on purpose.

   A section that comes back with any check failed is run again, once, in a
   fresh page and server (W40, GitHub #83: macos-ui failed a different check
   nearly every time, always passing on a rerun -- the runner, not a
   regression). `fn` is a deterministic script of key/mouse/touch events, so
   a clean rerun makes the same sequence of check() calls; a check that
   failed the first time and passed the second is reported FLAKY (the first
   attempt's failing detail, not silently as a pass); one that fails both
   times is a real FAIL. A mismatched number of checks between the two
   attempts (fn itself branched differently, or the rerun threw) means the
   rerun cannot be matched up with the first one: the first attempt's own
   results are reported as-is instead of guessing. */
/* an attempt that threw: its message, then the checks it had recorded that
   failed and the last one that ran, which say where it went wrong */
function thrown(ode, e, what) {
  console.log(`  (${path.basename(ode)}: ${what} threw (${e.message || e})`);
  const rec = (e && e.recorded) || [];
  for (const r of rec.filter(r => !r.ok)) console.log(`    before it: FAIL ${r.name}  ${r.detail}`);
  if (rec.length) console.log(`    last check before it: ${rec[rec.length - 1].name}`);
}

/* W124: the page's own word that a command never reached the core (or was
   refused): transport.ts sends a POST that failed with no answer again
   before it says so, so this is a real loss, whatever made it */
const LOST_COMMAND = /^The command \S+ (did not reach xppautX|was refused)/;
async function lostCommands(expected) {
  try {
    return (await S('__xpp.log().filter(l => l.kind === "error").map(l => l.text)'))
      .filter(e => LOST_COMMAND.test(e) && !expected.includes(e));
  } catch {
    return []; /* no page to ask (it crashed): the attempt's own failure says so */
  }
}

/* a lost command is a FAIL in whichever attempt it happened, the rerun
   cannot make it a flake */
function checkNoLostCommand(ode, attempts) {
  const lost = attempts.flatMap(rec => rec.lost || []);
  check(`${path.basename(ode)}: no command lost on the way to the core`, lost.length === 0, JSON.stringify(lost));
}

async function session(ode, fn, expected = []) {
  const attempts = [];
  try {
    await sessionAttempts(ode, fn, expected, attempts);
  } finally {
    checkNoLostCommand(ode, attempts);
  }
}

/* the section, and its rerun when a check failed: FLAKY or FAIL as above */
async function sessionAttempts(ode, fn, expected, attempts) {
  let rec1;
  try {
    rec1 = await sessionAttempt(ode, fn, expected, attempts);
  } catch (e) {
    record = null;
    thrown(ode, e, 'the first attempt');
    rec1 = null;
  }
  if (rec1 && rec1.every(r => r.ok)) {
    for (const r of rec1) check(r.name, r.ok, r.detail);
    return;
  }
  const firstFailedCount = rec1 ? rec1.filter(r => !r.ok).length : 'the crash';
  console.log(`  (${path.basename(ode)}: ${firstFailedCount} check(s) failed; rerunning the section once)`);
  let rec2;
  try {
    rec2 = await sessionAttempt(ode, fn, expected, attempts);
  } catch (e) {
    record = null;
    thrown(ode, e, 'the rerun');
    if (!rec1) throw e; /* both attempts crashed: a real failure, not a flake */
    for (const r of rec1) check(r.name, r.ok, r.detail);
    return;
  }
  if (!rec1 || rec1.length !== rec2.length) {
    /* the crash-then-clean-rerun case, or a mismatch: trust the rerun when
       there was no first attempt to compare, else the first attempt as-is */
    for (const r of (rec1 || rec2)) check(r.name, r.ok, r.detail);
    if (!rec1) flaky++;
    return;
  }
  for (let i = 0; i < rec1.length; i++) {
    const a = rec1[i], b = rec2[i];
    if (a.ok) { check(a.name, true, a.detail); continue; }
    if (b.ok) { console.log(`FLAKY ${a.name}  ${a.detail}`); flaky++; continue; }
    check(a.name, false, a.detail); /* failed twice: a real failure */
  }
}

/* ---- layout (W98, #147): the plot at every window width, Show AUTO clear of the messages,
   and the main plot after a session with an AUTO diagram is opened and Back is pressed (W102, #151) ---- */

/** the layout's rectangles, read from the DOM (never pixels) */
const layoutRects = () => cdp.eval(`(() => {
  const r = sel => { const e = document.querySelector(sel); if (!e) return null; const b = e.getBoundingClientRect();
    return {l: b.left, t: b.top, r: b.right, b: b.bottom, w: b.width, h: b.height, d: getComputedStyle(e).display,
      v: getComputedStyle(e).visibility}; };
  return {vw: innerWidth, vh: innerHeight, sw: document.documentElement.scrollWidth,
    plot: r('.plot-view:not([hidden]) .plot-host'), ws: r('.workspace'), vals: r('.values-panel'), menu: r('.menu-panel'),
    menuToggle: r('.menu-toggle'), valuesToggle: r('.values-toggle'), show: r('.auto-show'), msgs: r('.messages'),
    status: r('.status-bar')}; })()`);
/** the part of rectangle a inside b (a plot taller than its scrolling container shows only that much) */
const rectClip = (a, b) => (!a || !b) ? a : {...a, t: Math.max(a.t, b.t), b: Math.min(a.b, b.b), l: Math.max(a.l, b.l), r: Math.min(a.r, b.r)};
const rectsCross = (a, b) => !!a && !!b && a.l < b.r - 0.5 && b.l < a.r - 0.5 && a.t < b.b - 0.5 && b.t < a.b - 0.5;

/** what is wrong with the layout at this size: a list of words, empty when it is right */
function layoutProblems(L) {
  const bad = [];
  const {plot, ws, vals, menu} = L;
  if (!plot || plot.w < 100 || plot.h < 150) bad.push(`plot ${plot ? `${Math.round(plot.w)}x${Math.round(plot.h)}` : 'missing'}`);
  else {
    if (plot.l < -0.5 || plot.r > L.vw + 0.5) bad.push('plot outside the width');
    if (ws && (plot.t < ws.t - 0.5 || plot.t > ws.b - 40)) bad.push('plot not in view at the top of its container');
  }
  if (L.sw > L.vw) bad.push(`horizontal scroll ${L.sw} > ${L.vw}`);
  if (L.vw >= 768) {
    if (!vals || vals.w < 100 || vals.h < 40 || vals.d === 'none') bad.push('values panel not shown');
    else if (vals.r > L.vw + 0.5 || vals.b > L.vh + 0.5) bad.push('values panel outside the window');
    if (!menu || menu.w < 100 || menu.d === 'none' || menu.v === 'hidden') bad.push('menu not shown');
    if (rectsCross(rectClip(plot, ws), vals)) bad.push('plot and values overlap');
    if (rectsCross(rectClip(plot, ws), menu)) bad.push('plot and menu overlap');
  } else {
    if (!L.valuesToggle || L.valuesToggle.d === 'none') bad.push('no values toggle');
    if (!L.menuToggle || L.menuToggle.d === 'none') bad.push('no menu toggle');
  }
  return bad;
}

async function layoutCheck(dir) {
  await desktopMetrics();
  check('layout: the page connects', await until('s.hello && !s.busy', 'hello'));
  check('layout: a run stores rows for the plot', await integrate(601, 30000) || await until('s.seriesCount > 0 && !s.busy', 'rows'));

  for (const h of [900, 560]) {
    const bad = [];
    for (let w = 600; w <= 2000; w += 25) {
      await metrics({width: w, height: h, deviceScaleFactor: 1, mobile: false});
      await rendered();
      const p = layoutProblems(await layoutRects());
      if (p.length) bad.push(`${w}: ${p.join(', ')}`);
    }
    check(`layout: at every width 600..2000 (step 25) x ${h} the plot, the values panel and the menu are shown and reachable`,
      bad.length === 0, bad.slice(0, 6).join(' | ') + (bad.length > 6 ? ` (+${bad.length - 6})` : ''));
  }
  /* the widths of the maintainer's report, with a tall window */
  for (const w of [1000, 1070, 1180, 1279, 1280, 1880]) {
    await metrics({width: w, height: 1000, deviceScaleFactor: 1, mobile: false});
    await rendered();
    const L = await layoutRects();
    check(`layout: ${w}x1000 shows the plot with the Values panel`, layoutProblems(L).length === 0,
      JSON.stringify([layoutProblems(L), L.plot]));
  }
  /* a phone: the Values sheet opens and closes, the plot stays */
  await metrics({width: 600, height: 900, deviceScaleFactor: 1, mobile: false});
  await rendered();
  await cdp.eval(`document.querySelector('.values-toggle').click()`);
  check('layout: 600 px: the Values sheet opens over the window', await until('s.valuesOpen', 'values open')
    && (await layoutRects()).vals.h >= 800);
  await cdp.eval(`document.querySelector('.values-back').click()`);
  check('layout: ... and closes, the plot still there', await until('!s.valuesOpen', 'values closed')
    && layoutProblems(await layoutRects()).length === 0);
  await desktopMetrics();
  await rendered();

  /* AUTO open, Back: Show AUTO in the flow, off the plot, the messages and the status bar */
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
  await cdp.eval(`__xpp.send({cmd: 'key', win: 'equilibrium', key: 'i'})`);
  await until('!s.busy', 'import');
  await key('f');
  await until('!s.busy', 'file menu');
  await key('a');
  check('layout: File/Auto opens the AUTO view', await until('s.diagram.open && s.diagram.shown && dv.axes && !s.busy', 'auto open'));
  await autoButton('R');
  await until("s.ask && s.ask.kind === 'menu' && s.ask.title === 'Start'", 'start menu');
  await menuKey('s');
  check('layout: an AUTO run stores a diagram', await until('!s.busy && dv.points.x.length > 10', 'steady state', 60000));
  const nPts = await DS('d.points.x.length');
  await cdp.eval(`document.querySelector('.auto-back').click()`);
  check('layout: Back hides the view, Show AUTO appears', await until('!s.diagram.shown && s.diagram.open', 'auto back')
    && await cdp.eval(`!!document.querySelector('.auto-show')`));
  for (const h of [900, 560]) {
    const bad = [];
    for (let w = 600; w <= 2000; w += 50) {
      await metrics({width: w, height: h, deviceScaleFactor: 1, mobile: false});
      await rendered();
      const L = await layoutRects();
      const p = layoutProblems(L);
      if (!L.show) p.push('no Show AUTO');
      else {
        const shown = rectClip(L.show, L.ws); /* the workspace scrolls: a button below its edge is clipped, not covering */
        if (rectsCross(shown, L.msgs)) p.push('Show AUTO covers the messages');
        if (rectsCross(shown, L.status)) p.push('Show AUTO covers the status bar');
        if (rectsCross(shown, rectClip(L.plot, L.ws))) p.push('Show AUTO covers the plot');
        if (L.show.l < 0 || L.show.r > L.vw + 0.5) p.push('Show AUTO outside the width');
        if (!(await cdp.eval(`(() => { const b = document.querySelector('.auto-show'); b.scrollIntoView({block: 'nearest'}); const r = b.getBoundingClientRect(), w = document.querySelector('.workspace').getBoundingClientRect(); const ok = r.top >= w.top - 0.5 && r.bottom <= w.bottom + 0.5; document.querySelector('.workspace').scrollTop = 0; return ok; })()`))) p.push('Show AUTO not reachable by scrolling');
      }
      if (p.length) bad.push(`${w}: ${p.join(', ')}`);
    }
    check(`layout: with the AUTO view hidden, at 600..2000 x ${h} Show AUTO covers no message, no status bar, no plot`,
      bad.length === 0, bad.slice(0, 6).join(' | '));
  }
  await desktopMetrics();
  await rendered();
  await cdp.eval(`__xpp.send({cmd: 'set', kind: 'par', name: 'iapp', text: '%('})`);
  await until(`s.bottom === 'set par iapp: Illegal formula ..'`, 'bottom message');
  await cdp.eval(`document.querySelector('[data-error-ok]')?.click()`);
  await until(`!s.toasts.length`, 'toasts dismissed');
  check('layout: the message strip is the topmost element at its left end (nothing over it)',
    await cdp.eval(`(() => { const e = document.querySelector('.status-message'), r = e.getBoundingClientRect();
      return e.contains(document.elementFromPoint(r.left + Math.min(20, r.width / 2), r.top + r.height / 2)); })()`));

  /* W102: a session with the run's rows and the AUTO diagram, opened again, then Back */
  await cdp.eval(`__xpp.send({cmd: 'session', op: 'save', name: 'w102', data: true})`);
  await until('!s.busy', 'session saved');
  check('layout: the session file is written', fs.existsSync(path.join(dir, 'w102.snapx')));
  await cdp.eval(`__xpp.send({cmd: 'session', op: 'load', name: 'w102'})`);
  if (await until("s.ask && s.ask.kind === 'choice'", 'save first?', 15000))
    await cdp.eval(`__xpp.send({cmd: 'answer', id: __xpp.state().ask.id, key: 'd'})`);
  check('layout: opening it brings the AUTO view back with its diagram',
    await until(`s.diagram.open && s.diagram.shown && dv.points.x.length === ${nPts} && !s.busy`, 'session opened', 30000),
    JSON.stringify(await DS('[d.open, d.shown, d.points.x.length]')));
  await cdp.eval(`document.querySelector('.auto-back').click()`);
  await until('!s.diagram.shown && s.diagram.open', 'back after open');
  await rendered();
  const L = await layoutRects();
  const pl = await cdp.eval(`__xpp.plot()`);
  check('layout: W102: after Back the main plot is in the layout, sized, and holds the rows',
    layoutProblems(L).length === 0 && !!pl && pl.curves.length > 0 && pl.curves[0].points > 0,
    JSON.stringify([layoutProblems(L), L.plot, pl && pl.curves.map(c => c.points)]));
}

/* Remove stale Chrome profiles older than an hour (W105) */
function cleanStaleProfiles() {
  const tmpDir = os.tmpdir();
  const now = Date.now();
  const oneHourMs = 60 * 60 * 1000;
  try {
    for (const entry of fs.readdirSync(tmpDir, {withFileTypes: true})) {
      if (!entry.isDirectory() || !entry.name.startsWith('xppweb2-profile-')) continue;
      try {
        const fullPath = path.join(tmpDir, entry.name);
        const stats = fs.statSync(fullPath);
        if (now - stats.mtimeMs > oneHourMs) {
          fs.rmSync(fullPath, {recursive: true, force: true});
        }
      } catch (e) {
        /* ignore: profile may be in use or already removed */
      }
    }
  } catch (e) {
    /* ignore: tmpdir may not exist or not readable */
  }
}

async function main() {
  const browser = findBrowser(opt.browser);
  if (!browser) {
    console.log('web2check: no Chrome, Chromium or Edge found (set CHROME=path); skipped');
    process.exit(0);
  }
  if (!fs.existsSync(bin)) throw new Error(`no ${bin}: build xppautX first`);

  cleanStaleProfiles();

  const want = outputDat(), wantLive = outputDat(LIVE);
  const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'xppweb2-profile-'));
  const b = await startBrowser(browser, profile);
  cdp = b.cdp;
  downloads = b.downloads;
  try {
    await cdp.send('Page.enable');
    await installPerfObserver(cdp); /* before the first Page.navigate: draw/frame timing and long tasks, W58 */
    if (process.env.XPP_NETLOG) {
      /* the renderer cancels a 204's empty body once it has the headers
         (canceled: true), which loses nothing: only real failures print */
      const sent = new Map(), t0 = Date.now(), before = cdp.onEvent;
      cdp.onEvent = d => {
        before(d);
        const p = d.params;
        if (d.method === 'Network.requestWillBeSent' && /\/cmd\?/.test(p.request.url))
          sent.set(p.requestId, {body: p.request.postData, at: Date.now() - t0});
        else if (d.method === 'Network.loadingFailed' && sent.has(p.requestId) && !p.canceled)
          console.log(`netlog: POST /cmd ${sent.get(p.requestId).body} sent at ${sent.get(p.requestId).at} ms, `
            + `failed at ${Date.now() - t0} ms: ${p.errorText}`);
        if (d.method === 'Network.loadingFinished' || d.method === 'Network.loadingFailed') sent.delete(p.requestId);
      };
      await cdp.send('Network.enable');
    }
    /* XPP_CPU_THROTTLE=N runs the page N times slower (Chrome's own CPU
       throttling), to reproduce a slow runner's timing here (W93) */
    if (Number(process.env.XPP_CPU_THROTTLE) > 1)
      await cdp.send('Emulation.setCPUThrottlingRate', {rate: Number(process.env.XPP_CPU_THROTTLE)});
    /* about:blank has no app yet: this is protocol setup before the first navigation. */
    await cdp.send('Emulation.setDeviceMetricsOverride', {width: 1280, height: 860, deviceScaleFactor: 1, mobile: false});
    const run = name => !opt.only || opt.only.split(',').includes(name);
    if (run('desktop')) await session(ODE, async (dir) => {
      await desktop(want);
      /* before values(): its edits and slider change the values, and a
         Go runs with them (docs/protocol.md `set`), so the stored data would no
         longer match the pristine `want` computed from the ODE file's own
         defaults */
      await dataTable(want, dir);
      await values();
      await valuesNarrowEscape();
      await keyboardOnly();
      await phone();
      await prompts();
      await windows();
      await textViews();
    });
    if (run('desktop')) await session(ODE, sliderModelSwitch);
    if (run('layout')) await session(ODE, layoutCheck, ['set par iapp: Illegal formula ..']);
    if (run('phase')) await session(ODE, phasePlane);
    if (run('auto')) await session(ODE, autoView);
    if (run('auto')) await session(HEAVY_ODE, autoStopRace);
    if (run('autoviews')) await session(ODE, autoViews);
    if (run('keys')) await session(ODE, keysCheck);
    if (run('record')) await session(ODE, recordCheck);
    if (run('player')) await session(ODE, playerCheck);
    if (run('leave')) await session(ODE, leaveCheck);
    if (run('leave')) await session(ODE, leaveSaveCheck);
    if (run('lostf')) await session(ODE, lostF);
    if (run('lostf')) await session(HEAVY_ODE, lostFRunning);
    if (run('busy')) await session(LIVE, busyAuto);
    if (run('busy')) await session(LIVE, busyKeys);
    if (run('busy')) await session(LIVE, statusBarLayout);
    if (run('view')) await session(ODE, viewCheck);
    if (run('three')) await session(LORENZ_ODE, threePlot);
    if (run('marks')) await session(ODE, marks);
    if (run('aplot')) await session(APLOT_ODE, aplotView);
    if (run('files')) await session(ODE, files, ['gone.set: cannot be opened']);
    if (run('native')) await session(ODE, nativeFiles);
    if (run('live')) await session(LIVE, () => live(wantLive));
    if (run('million')) await session(MILLION, million);
    if (run('ani')) await session(ODE, animation);
    if (run('kinescope')) await session(ODE, kinescope);
    if (run('runs')) await session(ODE, runsCheck, ['bad.par:1: it is for 3 parameters, the model has 12', 'bad.ic:2: the file ends here, before W']);
    /* WF-001: %bogus_symbol_zzz is refused on purpose, logging the core's own "Illegal formula
       .." (xpp_util.cpp evaluate_formula), named by the field (json_state.cpp read_value) */
    if (run('values')) await session(HEAVY_ODE, valuesBusy);
    if (run('values')) await session(LIVE, valuesLive, ['set par iapp: Illegal formula ..', 'set par gca: Illegal formula ..']);
    if (run('values')) await session(path.join(top, 'examples/ode/amari.odex'), bcSection(0));
    if (run('values')) await session(path.join(top, 'examples/ode/dumbbvp.odex'), bcSection(2));
    if (run('help')) await session(ODE, helpCheck);
    if (run('errordialog')) await session(ODE, errorDialogCheck, ['Illegal formula ..', 'set par iapp: Illegal formula ..',
      'w140bad.par:1: it is for 3 parameters, the model has 12']);
    if (run('errordialog')) await warningFlashCheck();
    if (run('loaderror')) await loadErrorCheck();
  } finally {
    b.proc.kill();
    await b.waitForExit();

    await b.cleanup();
    /* Remove profile with retries (W105): after closing, wait for the Chrome
       process to exit, then remove the profile with retries to handle Windows
       file locking delays */
    let lastError;
    const removed = await waitFor(() => {
      try { fs.rmSync(profile, {recursive: true, force: true}); return true; }
      catch (e) { lastError = e; return false; }
    });
    if (!removed) throw new Error(`removing browser profile ${profile}: ${lastError.message}`);
  }
  console.log(`web2 checks: ${failures ? `${failures} failed` : 'all passed'}`
    + (flaky ? `, ${flaky} FLAKY (passed only after a section rerun)` : ''));
  process.exit(failures ? 1 : 0);
}

main().catch(e => {
  console.error(e.stack || e);
  process.exit(2);
});
