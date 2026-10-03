#!/usr/bin/env node
/* W9: the WebAssembly page (wasm/) in a headless browser, from Git Bash
   like web2check.mjs: it serves build/wasm (copy it from the WSL build)
   with and without the isolation headers, and reads the page's own data
   (window.__xppWasm), never pixels:
     - with COOP and COEP the page is cross-origin isolated, integrates
       lecar.ode in the worker (601 rows) and Abort stops a long run;
     - without them the page is not isolated, says so and starts nothing
       (SharedArrayBuffer is off: the core's threads need it).
   Usage: node tools/wasmpagecheck.mjs [--browser PATH] */
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {findBrowser, startBrowser, waitFor} from './cdp.mjs';
import {serve} from '../wasm/serve.mjs';

const root = path.join(path.dirname(fileURLToPath(import.meta.url)), '..');
const argv = process.argv.slice(2);
const browser = findBrowser(argv.includes('--browser') ? argv[argv.indexOf('--browser') + 1] : null);
if (!browser) {
  console.error('wasmpagecheck: no Chrome, Chromium or Edge found (--browser PATH)');
  process.exit(2);
}
if (!fs.existsSync(path.join(root, 'build', 'wasm', 'xppautx.wasm'))) {
  console.error('wasmpagecheck: build/wasm has no module (tools/wasmcheck.sh builds it in WSL; copy build/wasm here)');
  process.exit(2);
}

let failures = 0;
function check(name, ok, detail = '') {
  console.log(`${ok ? 'PASS' : 'FAIL'}: ${name}${ok || !detail ? '' : ' -- ' + detail}`);
  if (!ok) failures++;
}

const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'wasmpage'));
const b = await startBrowser(browser, profile);
const {cdp} = b;
await cdp.send('Page.enable');
await cdp.send('Runtime.enable');
const probe = expr => cdp.eval(`(${expr})`);
async function until(expr, what) {
  if (!(await waitFor(async () => (await probe(expr)) || null))) throw new Error(`${what}: ${cdp.notes.join('; ')}; page log: ${await probe('document.getElementById("log").textContent').catch(() => '?')}`);
}

try {
  /* isolated */
  let server = await serve(0, true);
  await cdp.send('Page.navigate', {url: `http://127.0.0.1:${server.address().port}/`});
  await until('window.__xppWasm !== undefined', 'the page did not load');
  check('with COOP and COEP the page is cross-origin isolated', await probe('window.__xppWasm.isolated && typeof SharedArrayBuffer === "function"'));
  await probe('document.getElementById("run").click()');
  await until('window.__xppWasm.integrated', 'the integration did not finish');
  check('the worker integrates lecar.ode: 601 rows', (await probe('window.__xppWasm.rows()')) === 601, String(await probe('window.__xppWasm.rows()')));
  await probe('document.getElementById("long").click()');
  await until('window.__xppWasm.rows() > 1000', 'the long run did not grow');
  await until('!document.getElementById("abort").disabled', 'Abort did not enable');
  await probe('document.getElementById("abort").click()');
  await until('window.__xppWasm.stopped', 'Abort did not stop the run');
  check('Abort stops the long run in the worker', true);
  server.close();

  /* not isolated */
  server = await serve(0, false);
  await cdp.send('Page.navigate', {url: `http://127.0.0.1:${server.address().port}/`});
  await until('window.__xppWasm !== undefined', 'the page did not load without the headers');
  check('without the headers the page is not isolated, says so and starts nothing',
    await probe('!window.__xppWasm.isolated && document.getElementById("run").disabled && /not cross-origin isolated/.test(document.getElementById("status").textContent)'));
  server.close();
} catch (e) {
  check('the page runs', false, e.message);
}
await b.cleanup();
b.proc.kill();
fs.rmSync(profile, {recursive: true, force: true, maxRetries: 5});
console.log(failures ? `${failures} FAILED` : 'all passed');
process.exit(failures ? 1 : 0);
