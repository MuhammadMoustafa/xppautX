#!/usr/bin/env node
/* W185: browser exact-FMA verification, using the same comparison as Node.
   Requires the experiment's fma-*.js/.wasm and vectors.json in build/wasm. */
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {findBrowser,startBrowser,waitFor} from './cdp.mjs';
import {serve} from '../wasm/serve.mjs';
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const folder = path.join(root,'build','wasm');
const profile = fs.mkdtempSync(path.join(os.tmpdir(),'w185-browser-'));
const browser = await startBrowser(findBrowser(),profile);
const server = await serve(0,true);
let failures = 0;
try {
  await browser.cdp.send('Page.enable');
  console.log(JSON.stringify(await browser.cdp.send('Browser.getVersion')));
  fs.copyFileSync(path.join(root,'tools','wasmfmafixtures.mjs'),path.join(folder,'wasmfmafixtures.mjs'));
  for (const variant of ['baseline','original','int128','wide']) {
    const page = `w185-${variant}.html`;
    fs.writeFileSync(path.join(folder,page),`<script>window.ready=${JSON.stringify(variant)}</script>`);
    await browser.cdp.send('Page.navigate',{url:`http://127.0.0.1:${server.address().port}/${page}`});
    await waitFor(async () => browser.cdp.eval(`window.ready === ${JSON.stringify(variant)}`));
    const supported = await browser.cdp.eval(`(async()=>WebAssembly.validate(await (await fetch('fma-${variant}.wasm')).arrayBuffer()))()`);
    console.log(`${variant}: runtime validation ${supported}`);
    if (!supported) {
      if (variant !== 'wide') throw new Error(`${variant}: standard module rejected`);
      continue;
    }
    const result = await browser.cdp.eval(`(async()=>{
      await new Promise((resolve,reject)=>{const script=document.createElement('script');script.src='fma-${variant}.js';script.onload=resolve;script.onerror=reject;document.head.append(script);});
      const module=await createFma();
      const {vectors}=await (await fetch('vectors.json')).json();
      const {checkFma}=await import('./wasmfmafixtures.mjs');
      return checkFma(module._fma,vectors);
    })()`);
    console.log(variant,JSON.stringify(result));
    failures += result.failures;
  }
} finally {
  server.closeAllConnections();
  await new Promise(resolve=>server.close(resolve));
  await browser.cleanup();
  browser.proc.kill();
  fs.rmSync(profile,{recursive:true,force:true,maxRetries:5});
}
if (failures) process.exitCode = 1;