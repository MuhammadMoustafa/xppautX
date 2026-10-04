#!/usr/bin/env node
/* W185: verify exported exact-FMA modules against independent MPFR vectors.
   Compare every finite result bit, including signed zero; NaNs are checked
   by classification because payload propagation is not the numerical contract. */
import fs from 'node:fs';
import {checkFma} from './wasmfmafixtures.mjs';
import path from 'node:path';
import {createRequire} from 'node:module';
const [fixture, ...modules] = process.argv.slice(2);
if (!fixture || !modules.length) throw new Error('usage: wasmfmacheck.mjs VECTORS.json MODULE.js [...]');
const {vectors, mpfr} = JSON.parse(fs.readFileSync(fixture, 'utf8'));
const require = createRequire(import.meta.url);
for (const modulePath of modules) {
  const module = await require(path.resolve(modulePath))();
  const result = checkFma(module._fma, vectors);
  console.log(`${modulePath}: ${result.cases} MPFR ${mpfr} vectors, ${result.failures} failed`);
  if (result.failures) {
    console.error('first mismatch:',modulePath,result.firstMismatch);
    process.exitCode = 1;
  }
}
