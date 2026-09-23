#!/usr/bin/env node
/* Builds the new front end into web2/dist, which is committed: xppautX
   embeds those files (Makefile WEB2_FILES, tools/embed.c), so building the
   program never needs Node or npm. Anyone who edits web2/src runs this
   and commits dist with the change.

   node build.mjs           build dist/
   node build.mjs --check   exit 1 when dist/ is not what src/ builds (CI)
   node build.mjs --watch   rebuild on every change (with node web/serve.js)
   node build.mjs --test    run the unit tests in test/ */
import * as esbuild from 'esbuild';
import {spawnSync} from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const dist = path.join(here, 'dist');
const mode = process.argv[2] ?? '--build';

/* files copied as they are: dist name -> source */
const COPIED = {
  'index.html': path.join(here, 'src/index.html'),
  'inter.woff2': path.join(here, 'node_modules/@fontsource-variable/inter/files/inter-latin-wght-normal.woff2'),
  'inter-OFL.txt': path.join(here, 'node_modules/@fontsource-variable/inter/LICENSE'),
};

/* the bundled libraries' notices (both MIT) and the font's (OFL 1.1, text in inter-OFL.txt) */
const BANNER = {
  js: '/*! xppautX web2, GPL-2.0. Bundles Preact, Copyright (c) 2015-present Jason Miller, and uPlot, '
    + 'Copyright (c) 2022 Leon Sorokin, both under the MIT License. */',
  css: '/*! uPlot CSS, Copyright (c) 2022 Leon Sorokin, MIT License. Inter font, Copyright 2016 The Inter '
    + 'Project Authors, SIL Open Font License 1.1 (inter-OFL.txt). */',
};

const options = {
  entryPoints: {app: path.join(here, 'src/main.tsx')},
  bundle: true,
  format: 'iife',
  target: ['es2020', 'chrome100', 'firefox100', 'safari15'],
  minify: true,
  legalComments: 'eof',
  banner: BANNER,
  jsx: 'automatic',
  jsxImportSource: 'preact',
  external: ['*.woff2'], /* the font is copied next to app.css */
  outdir: dist,
  logLevel: 'warning',
};

function copied() {
  return Object.entries(COPIED).map(([name, src]) => ({path: path.join(dist, name), contents: fs.readFileSync(src)}));
}

async function build() {
  fs.mkdirSync(dist, {recursive: true});
  await esbuild.build(options);
  for (const f of copied()) fs.writeFileSync(f.path, f.contents);
  console.log(`web2: built ${fs.readdirSync(dist).join(', ')}`);
}

async function check() {
  const r = await esbuild.build({...options, write: false});
  const want = [...r.outputFiles.map(f => ({path: f.path, contents: Buffer.from(f.contents)})), ...copied()];
  const stale = want.filter(f => !fs.existsSync(f.path) || !fs.readFileSync(f.path).equals(f.contents));
  const names = new Set(want.map(f => path.basename(f.path)));
  const extra = fs.existsSync(dist) ? fs.readdirSync(dist).filter(n => !names.has(n)) : [];
  if (stale.length || extra.length) {
    for (const f of stale) console.log(`web2: stale ${path.relative(here, f.path)}`);
    for (const n of extra) console.log(`web2: not built by src: dist/${n}`);
    console.log('web2: dist/ is not up to date: run `npm run build` in web2 and commit dist/');
    process.exit(1);
  }
  console.log('web2: dist/ is up to date');
}

async function watch() {
  for (const f of copied()) fs.writeFileSync(f.path, f.contents);
  const ctx = await esbuild.context(options);
  await ctx.watch();
  console.log('web2: watching src/ (Ctrl+C stops)');
}

async function test() {
  const out = path.join(here, '..', 'build', 'web2-test');
  const tests = fs.readdirSync(path.join(here, 'test')).filter(n => n.endsWith('.test.ts'));
  fs.rmSync(out, {recursive: true, force: true});
  await esbuild.build({
    entryPoints: tests.map(n => path.join(here, 'test', n)),
    bundle: true, platform: 'node', format: 'esm', outdir: out, outExtension: {'.js': '.mjs'},
    jsx: 'automatic', jsxImportSource: 'preact', logLevel: 'warning',
  });
  const files = tests.map(n => path.join(out, n.replace(/\.ts$/, '.mjs')));
  const r = spawnSync(process.execPath, ['--test', ...files], {stdio: 'inherit', cwd: here}); /* tests read src/ */
  process.exit(r.status ?? 1);
}

const run = {'--build': build, '--check': check, '--watch': watch, '--test': test}[mode];
if (!run) {
  console.error(`usage: node build.mjs [--check|--watch|--test]`);
  process.exit(2);
}
await run();
