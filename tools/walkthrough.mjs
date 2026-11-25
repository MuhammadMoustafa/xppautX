/* The flow walkthrough (W861, #942): drives the real page of `xppautX --browser` through the main tasks a user does, and photographs every
   state at 375, 768 and 1280 px wide, light and dark, into build/walkthrough/<flow>/. It writes one page per flow (build/walkthrough/<flow>.html)
   and index.html, for the maintainer to review: it is not a pass/fail check of the page. A step whose wait is not met is listed on its flow's
   page and in the terminal, with what was waited for; the steps after it are listed as not reached. Exit code 0 only when every step's wait was
   met. Run from the repository root (the browser's downloads go to build/web2check-downloads, as web2check's do):
     node tools/walkthrough.mjs [--only flow,...] [--bin xppautX] [--browser PATH] [-v]
   Published models only: examples/ode/lecar.odex (the Morris-Lecar equations, Morris and Lecar 1981) and tools/models/hodgkin_huxley_w857.autox
   (the Hodgkin-Huxley equations of 1952, whose script names tools/models/hodgkin_huxley.odex). */
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {findBrowser, installPerfObserver, navigatePage, startBrowser, startServer, stopServer, waitFor} from './cdp.mjs';
import {createDriver} from './pagedriver.mjs';

const top = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const opt = {bin: `xppautX${process.platform === 'win32' ? '.exe' : ''}`};
for (let i = 2; i < process.argv.length; i++) {
  const a = process.argv[i];
  if (a === '-v') opt.v = true;
  else opt[a.replace(/^--/, '')] = process.argv[++i];
}
const bin = path.resolve(top, opt.bin);
const OUT = path.join(top, 'build', 'walkthrough');
/* the safety ceilings of each wait, scaled as web2check's are (XPP_CHECK_SLOW): a slow machine waits longer, a wait is never a pass */
const SLOW = Number(process.env.XPP_CHECK_SLOW) || 1;
const ceiling = ms => ms * SLOW;

/* docs/ui-v2.md's widths, light and dark; the heights are the page's own */
const WIDTHS = [
  {width: 375, height: 812, label: '375 px (phone)'},
  {width: 768, height: 1024, label: '768 px (tablet)'},
  {width: 1280, height: 800, label: '1280 px (desktop)'},
];
const SCHEMES = ['light', 'dark'];
const LECAR = path.join(top, 'examples/ode/lecar.odex');
const HH_SCRIPT = path.join(top, 'tools/models/hodgkin_huxley_w857.autox');

/* a step's wait was not met: what the step waited for (named for the page and the terminal) */
class Waited extends Error {
  constructor(what) { super(`waited for ${what}`); this.waited = what; }
}

const esc = s => String(s).replace(/[&<>"']/g, c => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'})[c]);

/* the flows: each a list of steps, a step being what the user does (`does`), what they should see next (`sees`) and the action and its waits
   (`run`, given the step's context). A step's screenshots are taken after its run: every width, light and dark. */
const openFromStart = [
  {id: 'start', does: 'Starts xppautX in browser mode with no model file.',
    sees: 'The start screen: "Open a model", its Open model button and the examples list.',
    run: async w => w.wait(`!!document.querySelector('[data-start-screen]') && !!document.querySelector('[data-start-open]')`, 'the start screen')},
  {id: 'open-click', does: 'Clicks Open model on the start screen.',
    sees: 'The file picker for the model: its prompt names the kinds of file it opens, with Choose file.',
    run: async w => {
      await w.click('[data-start-open]', 'the Open model button');
      await w.wait(`s.ask?.kind === 'file' && !!document.querySelector('.file-ask')`, 'the file picker');
    }},
  {id: 'pick-lecar', does: 'Picks examples/ode/lecar.odex (the browser picker is answered with the file).',
    sees: 'The model loads: the main graph window is open and empty until the first run.',
    run: async w => {
      await w.answer({file: LECAR});
      await w.wait(`!!s.core && !s.busy && !s.ask && !document.querySelector('[data-start-screen]')`, 'lecar.odex loaded', 60000);
    }},
];

const FLOWS = [
  {id: 'open-model', title: 'Open a model from the start screen',
    steps: openFromStart},
  {id: 'integrate-change', title: 'Integrate, change a parameter, integrate again',
    steps: [...openFromStart,
      {id: 'integrate', does: 'Runs Run from initial (the Run menu).',
        sees: 'The graph draws the run: 601 rows, the variables against time.',
        run: async w => {
          await w.registry('run.initial');
          await w.wait(`!!w.series && w.series.rows === 601 && !s.busy`, 'the first run of 601 rows', 60000);
        }},
      {id: 'values', does: 'Opens Windows > Values and types 0.2 into iapp, then Enter.',
        sees: 'The iapp field shows 0.2 with no pending mark, and the core holds 0.2.',
        run: async w => {
          await w.registry('values.open');
          await w.wait(`!!document.querySelector('.value-field')`, 'the Values panel fields');
          const id = await w.eval(`(() => { const l = [...document.querySelectorAll('.value-field .value-name')]
            .find(e => e.textContent.trim().toLowerCase() === 'iapp'); return l ? l.closest('.value-field').querySelector('input').id : null; })()`);
          if (!id) throw new Error('the Values panel has no iapp field');
          await w.typeInto(id, '0.2');
          await w.wait(`Math.abs((s.core.pars.find(p => p[0].toLowerCase() === 'iapp') || [])[1] - 0.2) < 1e-9 && !s.busy`, 'iapp = 0.2 in the core');
        }},
      {id: 'integrate-again', does: 'Runs Run from initial again.',
        sees: 'A second series for iapp = 0.2 appears on the graph.',
        run: async w => {
          const before = await w.S('s.seriesCount');
          await w.registry('run.initial');
          await w.wait(`s.seriesCount > ${before} && !!w.series && w.series.rows === 601 && !s.busy`, 'the second run of 601 rows', 60000);
        }},
    ]},
  {id: 'autox-script', title: 'Open a .autox script and run it',
    steps: [...openFromStart.slice(0, 2),
      {id: 'pick-script', does: 'Picks tools/models/hodgkin_huxley_w857.autox (the script names its model, which loads with it).',
        sees: 'The autoX window opens with the script\'s runs in its Script panel; nothing is computed yet.',
        run: async w => {
          await w.answer({file: HH_SCRIPT});
          await w.wait(`!!s.autoxScript?.open && !s.busy && !s.ask`, 'the script opened', 60000);
          await w.eval(`(() => { const t = document.querySelector('[data-script-toggle]'); if (t && t.getAttribute('aria-pressed') !== 'true') t.click(); })()`);
          await w.wait(`!!document.querySelector('[data-script-run]')`, 'the script panel\'s runs');
        }},
      {id: 'run-all', does: 'Clicks Run all in the Script panel.',
        sees: 'Each run in order shows Done, Skipped or Error, and the diagram fills in.',
        run: async w => {
          await w.click('[data-script-run-all]', 'the Run all button');
          await w.wait(`!s.busy && [...document.querySelectorAll('[data-script-run]')].length > 0
            && [...document.querySelectorAll('[data-script-run]')].every(r => r.dataset.state !== '')`, 'every run of the script to end', 300000);
        }},
    ]},
  {id: 'hopf-branch', title: 'Grab a Hopf point and continue its periodic branch',
    steps: [...openFromStart,
      {id: 'open-autox', does: 'Opens autoX > Open.',
        sees: 'An autoX window with its toolbar (Run, Grab) and an empty diagram.',
        run: async w => {
          await w.registry('autox.open');
          await w.wait(`s.diagram.open && s.diagram.shown && !s.busy`, 'the autoX window');
        }},
      {id: 'run-dialog', does: 'Clicks Run in the autoX toolbar.',
        sees: 'The Run dialog: its kinds (Equilibrium, Periodic orbits, ...) and a Run button.',
        run: async w => {
          await w.registry('autox.run');
          await w.wait(`s.commandForm?.id === 'autox.run' && !!document.querySelector('.run-form .run-kinds [data-choice="equilibrium"]')`, 'the Run dialog');
        }},
      {id: 'run-equilibrium', does: 'Chooses Equilibrium and clicks Run.',
        sees: 'The equilibrium branch is drawn as the parameter moves; the run ends.',
        run: async w => {
          await w.click('.run-form .run-kinds [data-choice="equilibrium"]', 'the Equilibrium kind');
          await w.click('.run-form [data-autox-run]', 'the Run button');
          await w.wait(`!s.busy && !s.commandForm && dv.points.x.length > 10`, 'the equilibrium run to end', 120000);
        }},
      {id: 'grab', does: 'Clicks Grab in the autoX toolbar, then Grab in its dialog with the fields empty (the point is picked on the diagram).',
        sees: 'A pick bar asks which point to grab; Tab moves to a point and names its type.',
        run: async w => {
          await w.registry('autox.grab');
          await w.wait(`s.commandForm?.id === 'autox.grab' && !!document.querySelector('.registry-form button[type=submit]')`, 'the Grab dialog');
          await w.click('.registry-form button[type=submit]', 'the Grab button of the dialog');
          await w.wait(`s.ask?.kind === 'grab'`, 'the grab pick bar');
        }},
      {id: 'grab-hopf', does: 'Presses Tab until the Andronov-Hopf point is named, then Enter.',
        sees: 'The point is grabbed: the autoX info names the Hopf point, and the pick bar closes.',
        run: async w => {
          for (let i = 0; i < 12 && !(await w.S(`s.diagram.info?.sym === 'hopf'`)); i++) await w.key('Tab');
          await w.wait(`s.diagram.info?.sym === 'hopf'`, 'a Hopf point named under the pick bar');
          await w.key('Enter');
          await w.wait(`!s.busy && !s.diagram.grabbing && !s.ask`, 'the grab to end');
        }},
      {id: 'run-dialog-periodic', does: 'Clicks Run in the autoX toolbar again.',
        sees: 'The Run dialog, now with the grabbed Hopf point; Periodic orbits is among its kinds.',
        run: async w => {
          await w.registry('autox.run');
          await w.wait(`s.commandForm?.id === 'autox.run' && !!document.querySelector('.run-form .run-kinds [data-choice="periodic"]')`, 'the Run dialog');
        }},
      {id: 'run-periodic', does: 'Chooses Periodic orbits and clicks Run.',
        sees: 'A periodic branch from the Hopf point is drawn as its points come in; the run ends.',
        run: async w => {
          const before = await w.S('s.diagram.views[s.diagram.active].points.x.length');
          await w.click('.run-form .run-kinds [data-choice="periodic"]', 'the Periodic orbits kind');
          await w.click('.run-form [data-autox-run]', 'the Run button');
          await w.wait(`!s.busy && !s.commandForm && dv.points.x.length > ${before}`, 'the periodic run to end', 180000);
        }},
    ]},
  {id: 'save-reopen', title: 'Save the session and reopen it',
    steps: [...openFromStart,
      {id: 'integrate', does: 'Runs Run from initial.',
        sees: 'The graph draws the run: 601 rows.',
        run: async w => {
          await w.registry('run.initial');
          await w.wait(`!!w.series && w.series.rows === 601 && !s.busy`, 'the first run of 601 rows', 60000);
        }},
      {id: 'save', does: 'Chooses File > Save session as.',
        sees: 'The session downloads at once: a notice says the session file was downloaded.',
        run: async w => {
          const since = w.downloadMark();
          await w.registry('session.save_as');
          await w.wait(`s.toasts.some(t => t.text.endsWith(' downloaded'))`, 'the download notice');
          w.saved = await w.downloadedSince(since, '.snapx');
        }},
      {id: 'reopen-click', does: 'Chooses File > Open model.',
        sees: 'The file picker again.',
        run: async w => {
          await w.registry('session.open_model');
          await w.wait(`s.ask?.kind === 'file' && !!document.querySelector('.file-ask')`, 'the file picker');
        }},
      {id: 'reopen', does: 'Picks the saved session file.',
        sees: 'A question: Open lecar.snapx? This model\'s data and diagram go. Save its session first? (Save session, Don\'t save).',
        run: async w => {
          await w.answer({file: w.saved});
          await w.wait(`s.ask?.kind === 'choice' && /Save its session first/.test(s.ask.question)`, 'the leave question');
        }},
      {id: 'reopen-dont-save', does: 'Clicks Don\'t save (the session was just saved).',
        sees: 'The saved session opens: the graph shows the 601-row series again.',
        run: async w => {
          await w.clickButton('Don\'t save', 'the Don\'t save button');
          await w.wait(`!!w.series && w.series.rows === 601 && !s.busy && !s.ask`, 'the reopened session\'s run', 60000);
        }},
    ]},
];

/* a context of one flow: the step's actions and waits, and the pictures of the state a step reaches */
function flowContext(cdp, drv, downloads, dir, ctx) {
  const setViewport = async ({width, height}) => {
    await cdp.send('Emulation.setDeviceMetricsOverride', {width, height, deviceScaleFactor: 1, mobile: width < 500});
    await drv.requireUntil(`innerWidth === ${width} && innerHeight === ${height}`, `the ${width} px viewport`, ceiling(15000));
  };
  /* the page's own theme control (the title bar's toggle) sets the theme where it is shown; a screen without one (the start screen) keeps
     the system's, which the emulation sets: a screen where that does not reach the page is noted once, never photographed as if it did */
  const noted = new Set();
  const setScheme = async scheme => {
    await cdp.send('Emulation.setEmulatedMedia', {features: [{name: 'prefers-color-scheme', value: scheme}]});
    if (await cdp.eval(`!!document.querySelector('.theme-toggle')`)) {
      if (!(await drv.setPageTheme(scheme))) console.log(`  (the page's theme did not become ${scheme})`);
    } else if (!noted.has(scheme)) {
      noted.add(scheme);
      const page = await cdp.eval(`getComputedStyle(document.body).backgroundColor`);
      console.log(`  (no theme control on ${ctx.step}: the page keeps the system's theme, body background ${page} at ${scheme})`);
    }
    await drv.rendered();
  };
  /* every width, light and dark, of the state now on the page: JPEG files in the flow's folder */
  const shoot = async name => {
    const files = [];
    for (const wd of WIDTHS) {
      await setViewport(wd);
      for (const scheme of SCHEMES) {
        await setScheme(scheme);
        const shot = await cdp.send('Page.captureScreenshot', {format: 'jpeg', quality: 85});
        const file = `${ctx.step}-${name}-${wd.width}-${scheme}.jpg`;
        fs.writeFileSync(path.join(dir, file), Buffer.from(shot.data, 'base64'));
        files.push({width: wd.width, scheme, file});
      }
    }
    return {name, files};
  };
  const w = {
    shoot,
    S: expr => drv.S(expr),
    eval: expr => cdp.eval(expr),
    /* a wait for the page's own state: met, or the step is not reached (a Waited naming what it waited for) */
    wait: async (expr, what, ms = 30000) => {
      if (!(await drv.until(expr, what, ceiling(ms)))) throw new Waited(what);
    },
    click: async (selector, what) => {
      await w.wait(`!!document.querySelector(${JSON.stringify(selector)})`, what);
      await cdp.eval(`document.querySelector(${JSON.stringify(selector)}).click()`);
      await drv.rendered();
    },
    registry: id => drv.clickRegistry(id),
    /* a dialog's button, by its text (a button may carry its key's letter too: "D Don't save") */
    clickButton: async (text, what) => {
      const find = `[...document.querySelectorAll('[role=dialog] button')].find(b => b.textContent.includes(${JSON.stringify(text)}))`;
      await w.wait(`!!${find}`, what);
      await cdp.eval(`${find}.click()`);
      await drv.rendered();
    },
    key: k => drv.key(k),
    /* the field is focused and its text replaced by typing (real key input), then Enter applies it (Values: Enter or leave a field to apply) */
    typeInto: async (id, text) => {
      await w.wait(`!!document.getElementById(${JSON.stringify(id)})`, `the field ${id}`);
      await cdp.eval(`(() => { const el = document.getElementById(${JSON.stringify(id)}); el.focus(); el.select(); })()`);
      await cdp.send('Input.insertText', {text});
      await drv.key('Enter');
    },
    /* the core's file ask, answered with the file (the browser's own picker is not driven; the picture was taken before this) */
    answer: async fields => {
      await w.wait(`s.ask?.kind === 'file'`, 'the file picker');
      await drv.answerAsk(fields);
    },
    downloadMark: () => cdp.downloads.length,
    /* the browser's download of a file of this extension that began after `since`: its saved path, once Chrome has completed it */
    downloadedSince: async (since, ext) => {
      const rec = await waitFor(() => cdp.downloads.slice(since).find(r => r.name.endsWith(ext) && (r.state === 'completed' || r.state === 'canceled')), ceiling(30000));
      if (!rec || rec.state !== 'completed') throw new Waited(`the ${ext} download to complete`);
      return rec.filePath || path.join(downloads, rec.name);
    },
  };
  return w;
}

async function runFlow(flow, cdp, downloads) {
  const dir = path.join(OUT, flow.id);
  fs.rmSync(dir, {recursive: true, force: true});
  fs.mkdirSync(dir, {recursive: true});
  const session = fs.mkdtempSync(path.join(os.tmpdir(), `xppwalk-${flow.id}-`));
  const drv = createDriver(() => cdp, {verbose: !!opt.v});
  const ctx = {step: null};
  const w = flowContext(cdp, drv, downloads, dir, ctx);
  const result = {id: flow.id, title: flow.title, ok: true, error: null, steps: []};
  let server = null;
  console.log(`${flow.id}: ${flow.title}`);
  try {
    /* no model file: the start screen is the first state a user sees */
    server = await startServer(bin, session, []);
    await navigatePage(cdp, server.url, 'true', ceiling(60000));
    for (const [i, step] of flow.steps.entries()) {
      const rec = {id: step.id, does: step.does, sees: step.sees, shots: [], waited: null, error: null, notReached: false};
      result.steps.push(rec);
      ctx.step = step.id;
      try {
        await step.run(w);
        rec.shots.push(await w.shoot('after'));
        console.log(`  ok      ${step.id}`);
      } catch (e) {
        result.ok = false;
        if (e instanceof Waited) rec.waited = e.waited;
        else rec.error = e.message.split('\n')[0];
        /* what the page shows where the step stopped, for the maintainer to see the stuck state */
        try { rec.shots.push(await w.shoot('stuck')); } catch { /* the page is gone: the text of the failure stands */ }
        console.log(`  FAILED  ${step.id}: ${rec.waited !== null ? `waited for ${rec.waited}, not met` : rec.error}`);
        for (const later of flow.steps.slice(i + 1)) result.steps.push({id: later.id, does: later.does, sees: later.sees, shots: [], waited: null, error: null, notReached: true});
        break;
      }
    }
  } catch (e) {
    result.ok = false;
    result.error = e.message.split('\n')[0];
    console.log(`  FAILED  ${flow.id}: ${result.error}`);
  } finally {
    if (server) await stopServer(server);
    fs.rmSync(session, {recursive: true, force: true, maxRetries: 5});
  }
  fs.writeFileSync(path.join(OUT, `${flow.id}.json`), JSON.stringify(result, null, 2) + '\n');
  fs.writeFileSync(path.join(OUT, `${flow.id}.html`), pageHtml(flow.title, flowBody(result), true));
  return result.ok;
}

/* the pages: plain HTML with inline CSS, the width chosen by the radio buttons at the top (CSS only), light and dark side by side,
   each picture a link to the full JPEG; the colors are the page's variables, dark under prefers-color-scheme */
const CSS = `
:root { --bg: #ffffff; --fg: #1d1d1f; --muted: #5f6368; --line: #d0d4da; --card: #f5f6f8; --chip: #e6e9ee; --bad: #a50e0e; --badbg: #fde7e4; --ok: #1e6b34; }
@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) { --bg: #16181c; --fg: #e8eaed; --muted: #a1a7b0; --line: #3c4043; --card: #202328; --chip: #2b2f36; --bad: #ffb4a9; --badbg: #3c1f1d; --ok: #8fd19e; } }
:root[data-theme="dark"] { --bg: #16181c; --fg: #e8eaed; --muted: #a1a7b0; --line: #3c4043; --card: #202328; --chip: #2b2f36; --bad: #ffb4a9; --badbg: #3c1f1d; --ok: #8fd19e; }
* { box-sizing: border-box; }
body { margin: 0; padding: 16px; background: var(--bg); color: var(--fg); font: 16px/1.45 system-ui, -apple-system, "Segoe UI", sans-serif; }
input[type=radio] { display: none; }
header { margin-bottom: 12px; }
h1 { font-size: 1.4rem; margin: 0 0 8px; }
.widths { display: flex; flex-wrap: wrap; gap: 8px; }
.widths label { border: 1px solid var(--line); border-radius: 999px; padding: 6px 12px; cursor: pointer; background: var(--chip); }
#sel375:checked ~ header label[for="sel375"], #sel768:checked ~ header label[for="sel768"], #sel1280:checked ~ header label[for="sel1280"] { background: var(--fg); color: var(--bg); }
#sel375:checked ~ main .at:not(.w375), #sel768:checked ~ main .at:not(.w768), #sel1280:checked ~ main .at:not(.w1280) { display: none; }
.step { border: 1px solid var(--line); border-radius: 10px; background: var(--card); padding: 12px; margin: 0 0 16px; }
.step h2 { font-size: 1.05rem; margin: 0 0 6px; }
.num { display: inline-block; min-width: 1.6em; padding: 0 6px; border-radius: 999px; background: var(--chip); text-align: center; margin-right: 6px; }
.step p { margin: 4px 0; }
.problem { color: var(--bad); background: var(--badbg); border-radius: 6px; padding: 6px 8px; }
.not-reached { opacity: 0.7; }
.capture h3 { font-size: 0.95rem; color: var(--muted); margin: 12px 0 4px; font-weight: 600; }
.pair { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; align-items: start; }
figure { margin: 0; }
figcaption { font-size: 0.8rem; color: var(--muted); }
figure img { display: block; width: 100%; height: auto; border: 1px solid var(--line); border-radius: 6px; }
.ok { color: var(--ok); }
.empty { color: var(--muted); }
`;

function pageHtml(title, body, flowPage) {
  const home = flowPage ? '<p><a href="index.html">All flows</a></p>' : '';
  return `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>${esc(title)}</title>
<style>${CSS}</style>
</head>
<body>
${body.replace('<!--home-->', home)}
</body>
</html>
`;
}

/* the page's width controls, then the steps; every picture is the step's state at one width and theme, its full JPEG one click away */
function flowBody(result) {
  const radios = WIDTHS.map((wd, i) => `<input type="radio" name="width" id="sel${wd.width}"${i === 0 ? ' checked' : ''}>`).join('\n');
  const labels = WIDTHS.map(wd => `<label for="sel${wd.width}">${esc(wd.label)}</label>`).join(' ');
  const steps = result.steps.map((s, i) => stepHtml(s, i + 1, result.id)).join('\n');
  const verdict = result.ok ? '<p class="ok">Every step was reached.</p>'
    : `<p class="problem" role="alert">${result.error ? `The flow did not start: ${esc(result.error)}` : 'A step was not reached; the steps after it were not run.'}</p>`;
  return `${radios}
<header>
<!--home-->
<h1>${esc(result.title)}</h1>
<p class="empty">Flow <code>${esc(result.id)}</code>. Pictures for review, not a pass or fail check of the page.</p>
${verdict}
<nav class="widths" aria-label="Width">${labels}</nav>
</header>
<main>
${steps}
</main>`;
}

function stepHtml(s, n, flowId) {
  const head = `<h2><span class="num">${n}</span>${esc(s.id)}</h2>`;
  const does = `<p><b>You do:</b> ${esc(s.does)}</p>`;
  const sees = `<p><b>You should see next:</b> ${esc(s.sees)}</p>`;
  if (s.notReached) return `<section class="step not-reached">${head}${does}${sees}<p class="problem">Not reached: an earlier step was not met.</p></section>`;
  let problem = '';
  if (s.waited !== null) problem = `<p class="problem" role="alert">Not met: waited for ${esc(s.waited)}, and the page did not reach it.</p>`;
  else if (s.error) problem = `<p class="problem" role="alert">Error: ${esc(s.error)}</p>`;
  const captures = s.shots.map(sh => `<div class="capture"><h3>${esc(sh.name)}</h3>${WIDTHS.map(wd => {
    const figs = SCHEMES.map(sc => {
      const f = sh.files.find(x => x.width === wd.width && x.scheme === sc);
      const src = `${flowId}/${f.file}`;
      return `<figure><figcaption>${sc}</figcaption><a href="${esc(src)}"><img src="${esc(src)}" alt="${esc(s.id)}, ${esc(sh.name)}, ${wd.width} px, ${sc}" loading="lazy"></a></figure>`;
    }).join('');
    return `<div class="at w${wd.width}"><p class="empty">${esc(wd.label)}</p><div class="pair">${figs}</div></div>`;
  }).join('')}</div>`).join('');
  return `<section class="step">${head}${does}${sees}${problem}${captures}</section>`;
}

function indexPage() {
  const rows = FLOWS.map(flow => {
    const file = path.join(OUT, `${flow.id}.json`);
    if (!fs.existsSync(file)) return `<li><a href="${flow.id}.html">${esc(flow.title)}</a> <span class="empty">not run yet</span></li>`;
    const r = JSON.parse(fs.readFileSync(file, 'utf8'));
    const count = `${r.steps.filter(s => !s.notReached).length} of ${flow.steps.length} steps reached`;
    return `<li><a href="${flow.id}.html">${esc(flow.title)}</a> <span class="${r.ok ? 'ok' : 'problem'}">${r.ok ? 'all reached' : esc(count)}</span></li>`;
  }).join('\n');
  const body = `<header><h1>Flow walkthrough</h1>
<p>Each flow drives the real page and photographs every state at 375, 768 and 1280 px, light and dark. The pictures are for review (the maintainer reads them, and findings become cards); this is not a pass or fail check of the page.</p>
<p class="empty">Made by <code>node tools/walkthrough.mjs</code> from the repository root, after <code>make -j4 xppautx</code>.</p></header>
<main><ul>
${rows}
</ul></main>`;
  fs.writeFileSync(path.join(OUT, 'index.html'), pageHtml('Flow walkthrough', body, false));
}

async function main() {
  const browser = findBrowser(opt.browser);
  if (!browser) {
    console.log('walkthrough: no Chrome, Chromium or Edge found (set CHROME=path); nothing was walked through');
    process.exitCode = 1;
    return;
  }
  if (!fs.existsSync(bin)) throw new Error(`no ${bin}: build xppautX first`);
  const ids = opt.only ? opt.only.split(',') : FLOWS.map(f => f.id);
  for (const id of ids) if (!FLOWS.some(f => f.id === id)) throw new Error(`no flow "${id}": the flows are ${FLOWS.map(f => f.id).join(', ')}`);
  fs.mkdirSync(OUT, {recursive: true});
  const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'xppwalk-profile-'));
  const b = await startBrowser(browser, profile);
  let allOk = true;
  try {
    await b.cdp.send('Page.enable');
    await b.cdp.send('Emulation.setFocusEmulationEnabled', {enabled: true});
    await installPerfObserver(b.cdp); /* before the first navigation: the page's render counts (the waits' rendered()), as web2check's */
    for (const flow of FLOWS.filter(f => ids.includes(f.id))) allOk = (await runFlow(flow, b.cdp, b.downloads)) && allOk;
  } finally {
    b.proc.kill();
    await b.waitForExit().catch(() => undefined); /* the profile is removed once Chrome has let go of it */
    b.cleanup();
    /* Chrome may still be writing its profile for a moment after it exits: a folder it leaves behind is named, never fatal */
    try { fs.rmSync(profile, {recursive: true, force: true, maxRetries: 5}); } catch (e) { console.log(`walkthrough: the browser profile ${profile} was not removed: ${e.code}`); }
  }
  indexPage();
  console.log(allOk ? `walkthrough: every step reached; pages in ${path.relative(top, OUT)}/index.html` : `walkthrough: a step was not reached; see ${path.relative(top, OUT)}/index.html`);
  process.exit(allOk ? 0 : 1);
}

main().catch(e => {
  console.error(`walkthrough: ${e.message}`);
  process.exit(1);
});
