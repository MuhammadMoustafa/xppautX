import fs from 'node:fs';
import path from 'node:path';
import {findBrowser, startBrowser, startServer, stopServer, sleep} from '../../../../tools/cdp.mjs';
const [bin, work, css, js] = process.argv.slice(2); const MOCK = !!css; const CSS = MOCK ? fs.readFileSync(css,'utf8') : ''; const JS = MOCK ? fs.readFileSync(js,'utf8') : '1';
const server = await startServer(path.resolve(bin), work, ['lecar.odex']);
const b = await startBrowser(findBrowser(), path.join(work, 'profile'));
const cdp = b.cdp;
const click = text => `(() => { const e = [...document.querySelectorAll('button, summary')].find(x => x.textContent.replace(/\\s+/g,' ').trim().startsWith(${JSON.stringify(text)})); if (!e) return 'missing ' + ${JSON.stringify(text)}; e.click(); return 1; })()`;
const OPEN = {
  trace: `(() => { document.querySelector('.legend-item').dispatchEvent(new MouseEvent('dblclick', {bubbles: true})); return 1; })()`,
  palette: click('Command palette'),
  command_form: click('Settings'),
  ask_string: click('Save as new set'),
  values_file: click('Load from file'),
  shortcuts: click('Keyboard shortcuts'),
  slider: click('Add slider'),
};
try {
  await cdp.send('Page.enable'); await cdp.send('Page.navigate', {url: server.url});
  await cdp.send('Emulation.setDeviceMetricsOverride', {width: 1280, height: 800, deviceScaleFactor: 1, mobile: false});
  for (let i = 0; i < 300 && !(await cdp.eval(`(() => { try { return !!__xpp.state().hello && !__xpp.state().busy } catch { return false } })()`)); i++) await sleep(100);
  for (const [name, js] of Object.entries(OPEN)) {
    const r = await cdp.eval(js); await sleep(800);
    if (MOCK) { await cdp.eval(`(() => { if (!document.getElementById('mock')) { const s = document.createElement('style'); s.id = 'mock'; s.textContent = ${JSON.stringify(CSS)}; document.head.append(s); } return 1; })()`); await cdp.eval(JS); await sleep(300); }
    const rect = await cdp.eval(`(() => { const d = [...document.querySelectorAll('[role=dialog], [role=alertdialog], .dialog')].filter(e => e.getBoundingClientRect().height > 0).pop(); if (!d) return null; const r = d.getBoundingClientRect(); return {x: r.x, y: r.y, w: r.width, h: r.height, t: d.className}; })()`);
    console.log(name, r, JSON.stringify(rect));
    if (rect) { const m = 12; const shot = await cdp.send('Page.captureScreenshot', {format: 'png', clip: {x: Math.max(0, rect.x - m), y: Math.max(0, rect.y - m), width: rect.w + 2 * m, height: Math.min(rect.h + 2 * m, 760), scale: 1}});
      fs.writeFileSync(path.join(work, `dlg_${name}${MOCK ? '_new' : ''}.png`), Buffer.from(shot.data, 'base64')); }
    for (let k = 0; k < 2; k++) { await cdp.send('Input.dispatchKeyEvent', {type: 'keyDown', key: 'Escape', code: 'Escape', windowsVirtualKeyCode: 27}); await cdp.send('Input.dispatchKeyEvent', {type: 'keyUp', key: 'Escape', code: 'Escape', windowsVirtualKeyCode: 27}); await sleep(200); }
    await cdp.eval(`(() => { for (const c of document.querySelectorAll('.dialog-close')) c.click(); const x = [...document.querySelectorAll('.dialog button')].find(b => /^(Close|Cancel)$/.test(b.textContent.trim())); x?.click(); return 1; })()`); await sleep(300);
  }
} finally { b.proc.kill(); await b.waitForExit().catch(() => {}); await b.cleanup(); await stopServer(server); }
