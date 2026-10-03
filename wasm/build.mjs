#!/usr/bin/env node
/* Assembles the WebAssembly proof of concept next to the module `make
   wasm` links (build/wasm/xppautx.js and .wasm): bundles the page and the
   client (src/, with web2's own reducer and types from ../web2/src, so
   web2's `npm ci` provides esbuild) and copies the worker, its glue, the
   page and its model. Nothing here is committed: build/ is ignored.
   Usage: node wasm/build.mjs [--typecheck] */
import {createRequire} from 'node:module';
import {spawnSync} from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.join(here, '..');
const out = path.join(root, 'build', 'wasm');
const web2 = createRequire(path.join(root, 'web2', 'package.json'));
const esbuild = web2('esbuild');

fs.mkdirSync(path.join(out, 'models'), {recursive: true});
const common = {bundle: true, logLevel: 'warning', target: 'es2022', outdir: out};
await esbuild.build({...common, entryPoints: [path.join(here, 'src/page.ts')], format: 'iife', platform: 'browser'});
await esbuild.build({...common, entryPoints: [path.join(here, 'src/client.ts')], format: 'esm', platform: 'node', outExtension: {'.js': '.mjs'}});
for (const f of ['index.html', 'core.js', 'worker.js']) fs.copyFileSync(path.join(here, f), path.join(out, f));
fs.copyFileSync(path.join(root, 'examples', 'ode', 'lecar.ode'), path.join(out, 'models', 'lecar.ode'));
if (process.argv.includes('--typecheck')) {
  const r = spawnSync(path.join(root, 'web2', 'node_modules', '.bin', 'tsc'), ['-p', here], {stdio: 'inherit'});
  process.exit(r.status ?? 1);
}
console.log('wasm: built', fs.readdirSync(out).join(', '));
