/* The page driver both browser scripts share (tools/web2check.mjs's sections and tools/walkthrough.mjs): the page's state reads (S and
   the active window and view they name), the waits (until, requireUntil, rendered), keys, a real menu item (clickRegistry, paletteRegistry),
   the answer to a file ask (answerAsk) and the page's theme (setPageTheme). createDriver takes a function that returns the open Cdp of the
   page, so each script keeps its own connection. Every wait ends on the page's own state (window.__xpp), never on a time. */
import {waitFor} from './cdp.mjs';

/* the active plot window's state, and the active autoX view's (W50: the diagram has views, each its own axes and points) */
export const ACTIVE = 's.plots.windows.find(x => x.win === s.plots.active) || {}';
export const DV = 's && s.diagram.views[s.diagram.active]';

const NAMED = {Escape: 27, Enter: 13, Tab: 9, Home: 36, End: 35, PageUp: 33, PageDown: 34,
  ArrowLeft: 37, ArrowUp: 38, ArrowRight: 39, ArrowDown: 40, F1: 112, F2: 113, F6: 117, F9: 120, Backspace: 8};

export function createDriver(getCdp, {verbose = false} = {}) {
  const cdp = () => getCdp();
  const S = expr => cdp().eval(`(() => { const s = __xpp.state(), w = ${ACTIVE}; return ${expr}; })()`);

  /* waits until `expr` holds on the page (true, or a timeout: false); a pending "Replace it?" choice is answered yes, as the page's own
     copy would be */
  async function until(expr, what, ms) {
    const ready = await waitFor(async () => {
      let v = null;
      try {
        v = await cdp().eval(`(() => { try { const s = window.__xpp && __xpp.state(), w = ${ACTIVE}, dv = ${DV}; return !!(${expr}); } catch (e) { return false; } })()`);
      } catch { /* page not there yet */ }
      if (v) return true;
      await cdp().eval(`(() => { const a = window.__xpp && __xpp.state().ask;
        if (a && a.kind === 'choice' && a.keys === 'yn' && a.question.endsWith(' exists. Replace it?'))
          __xpp.send({cmd: 'answer', id: a.id, key: 'y'}); })()`).catch(() => {});
      return false;
    }, ms);
    if (!ready && verbose) console.log(`  (timed out waiting for ${what})`);
    return !!ready;
  }

  async function requireUntil(expr, what, ms) {
    if (!await until(expr, what, ms)) throw new Error(`Timed out waiting for ${what}`);
  }

  /* the page has drawn what it has to: its components, CSS transitions and the next ResizeObserver delivery */
  async function rendered() {
    if (!await until('window.__xpp && __xpp.rendered()', 'component renders and effects'))
      throw new Error('component renders and effects did not complete');
    await cdp().eval(`Promise.all(document.getAnimations().filter(a => a instanceof CSSTransition)
      .map(a => a.finished.catch(() => {})))`);
    await cdp().eval(`new Promise(resolve => {
      const observer = new ResizeObserver(() => { observer.disconnect(); resolve(true); });
      observer.observe(document.documentElement);
    })`);
    if (!await until('__xpp.rendered()', 'renders after resize'))
      throw new Error('component renders after resize did not complete');
  }

  async function key(k, modifiers = 0) {
    if (NAMED[k]) {
      const code = NAMED[k];
      await cdp().send('Input.dispatchKeyEvent', {type: 'rawKeyDown', key: k, code: k, windowsVirtualKeyCode: code, modifiers});
      await cdp().send('Input.dispatchKeyEvent', {type: 'keyUp', key: k, code: k, windowsVirtualKeyCode: code, modifiers});
    } else if (modifiers) {
      const code = k.toUpperCase().charCodeAt(0);
      await cdp().send('Input.dispatchKeyEvent', {type: 'rawKeyDown', key: k, code: `Key${k.toUpperCase()}`, windowsVirtualKeyCode: code, modifiers});
      await cdp().send('Input.dispatchKeyEvent', {type: 'keyUp', key: k, code: `Key${k.toUpperCase()}`, windowsVirtualKeyCode: code, modifiers});
    } else {
      await cdp().send('Input.dispatchKeyEvent', {type: 'keyDown', key: k, text: k, unmodifiedText: k});
      await cdp().send('Input.dispatchKeyEvent', {type: 'keyUp', key: k});
    }
    await rendered();
  }

  const exists = selector => cdp().eval(`!!document.querySelector(${JSON.stringify(selector)})`);

  /** W517: choose a real menu item; the registry supplies the entire path and command. The menu is
      built from hello, so the item is waited for (a page just loaded may not have it yet). */
  async function clickRegistry(id) {
    const selector = `.app-menu [data-command=${JSON.stringify(id)}]`;
    /* W637: a command with no menu place (a view's own control, the palette's) is run from the command palette */
    if (!(await exists(selector)) && id !== 'command_palette.open') return paletteRegistry(id);
    await requireUntil(`document.querySelector(${JSON.stringify(selector)})`, 'menu item ' + id);
    await cdp().eval(`(() => {
      const item = [...document.querySelectorAll('.app-menu [data-command]')].find(b => b.dataset.command === ${JSON.stringify(id)});
      if (!item) throw new Error('No menu item ' + ${JSON.stringify(id)});
      for (let p = item.parentElement; p; p = p.parentElement) if (p.tagName === 'DETAILS') p.open = true;
      item.click();
    })()`);
    await rendered();
  }

  /**  W637: a command with no menu place, run from the command palette (Help > Command palette) */
  async function paletteRegistry(id) {
    await clickRegistry('command_palette.open');
    await until('s.drawerOpen', 'command palette open');
    await cdp().eval(`document.querySelector(${JSON.stringify(`.command-palette [data-command="${id}"]`)}).click()`);
    await rendered();
  }

  /* answers the core's open ask with the file (the browser's picker is not driven: the answer is what it would send) */
  const answerAsk = fields => cdp().eval(`__xpp.send(Object.assign({cmd: 'answer', id: __xpp.state().ask.id}, ${JSON.stringify(fields)}))`);

  /* W874 (#956): the page's own theme toggle, clicked until the state says `want` (light, dark) */
  async function setPageTheme(want) {
    for (let i = 0; i < 3 && (await S('s.theme')) !== want; i++) await cdp().eval(`document.querySelector('.theme-toggle')?.click()`);
    return (await S('s.theme')) === want;
  }

  return {S, until, requireUntil, rendered, key, exists, clickRegistry, paletteRegistry, answerAsk, setPageTheme};
}
