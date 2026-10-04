#!/usr/bin/env node
/* W9: the WebAssembly core (docs/wasm.md) against the native program.
   Runs under node, which has the same threads and shared memory as a
   browser's worker (the page's headers are checked by hand, docs/wasm.md).
   Reads data only: rows and events.
     - lecar.ode integrated through the protocol in the module: its series
       equals output.dat of `xppautX lecar.ode --silent` (printed with 8
       digits, the series' stored floats: servercheck's own comparison);
     - the same session through the native --server gives the same series;
     - Abort: a long run stops through the shared memory while the core
       computes (a stopped event, then idle, far short of the run's rows);
     - names: core.js's glue refuses a file name that is not a base name.
   --bench also prints the time of kuramot100.odex's integration, module and
   native, through the same client (a perf: line, never a pass mark: W58).
   Usage: node tools/wasmcheck.mjs [--native ./xppautX] [--wasm build/wasm] [--bench [--bench-rounds N]]
   (tools/wasmcheck.sh builds everything it needs.) Every wait is for an
   event, with a generous safety limit (wasm/src/client.ts). */
import {spawn, spawnSync} from 'node:child_process';
import {createRequire} from 'node:module';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import readline from 'node:readline';
import {fileURLToPath, pathToFileURL} from 'node:url';

const root = path.join(path.dirname(fileURLToPath(import.meta.url)), '..');
const argv = process.argv.slice(2);
const option = (name, dflt) => (argv.includes(name) ? argv[argv.indexOf(name) + 1] : dflt);
const native = path.resolve(option('--native', path.join(root, 'xppautX')));
const wasmDir = path.resolve(option('--wasm', path.join(root, 'build', 'wasm')));
const bench = argv.includes('--bench');
// Three warmed samples by default; more can be requested for a timing study.
const measuredRounds = Number(option('--bench-rounds', '3'));
if (!Number.isSafeInteger(measuredRounds) || measuredRounds < 1)
  throw new Error('--bench-rounds must be a positive integer');

const {LineTransport, WasmClient} = await import(pathToFileURL(path.join(wasmDir, 'client.mjs')).href);
const require = createRequire(import.meta.url);
const xppCore = require(path.join(wasmDir, 'core.js'));
const createXppautX = require(path.join(wasmDir, 'xppautx.js'));

let failures = 0;
function check(name, ok, detail = '') {
  console.log(`${ok ? 'PASS' : 'FAIL'}: ${name}${ok || !detail ? '' : ' -- ' + detail}`);
  if (!ok) failures++;
}

const lecar = path.join(root, 'examples', 'ode', 'lecar.ode');

/** a port to the core in the module: the model's file in its folder, --server */
function wasmPort(file, text) {
  return onLine => {
    const core = xppCore.startXpp(createXppautX, {
      files: {[file]: text}, args: ['--server', file],
      onLine, onLog: () => {},
    });
    core.ready.catch(e => {
      console.error('the wasm core did not start:', e);
      process.exit(1);
    });
    return {send: line => core.send(line), close: () => core.send('{"cmd":"quit"}')};
  };
}

/** a port to the native program's --server on the same file, in a folder of its own */
function nativePort(file, text) {
  return onLine => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'xppwasm'));
    fs.writeFileSync(path.join(dir, file), text);
    const child = spawn(native, ['--server', file], {cwd: dir, stdio: ['pipe', 'pipe', 'ignore']});
    readline.createInterface({input: child.stdout}).on('line', onLine);
    return {
      send: line => child.stdin.write(line + '\n'),
      close: () => {
        child.stdin.write('{"cmd":"quit"}\n');
        child.on('exit', () => fs.rmSync(dir, {recursive: true, force: true}));
      },
    };
  };
}

async function session(port) {
  const client = new WasmClient(new LineTransport(port));
  client.start();
  await client.started();
  return client;
}

/** the active window's series as the page holds it: columns by storage column, with their names */
function seriesOf(client) {
  const w = client.state.plots.windows.find(x => x.win === client.state.plots.active);
  const s = w?.series;
  if (!s) return null;
  return {rows: s.rows, columns: [...s.columns].map(([col, values]) => ({col, name: s.names.get(col), values}))};
}

/** Compare stored values without losing negative zero through JSON serialization. */
function sameSeries(a, b) {
  return !!a && !!b && a.rows === b.rows && a.columns.length === b.columns.length
    && a.columns.every((c, i) => c.col === b.columns[i].col && c.name === b.columns[i].name
      && c.values.length === b.columns[i].values.length
      && c.values.every((v, r) => Object.is(v, b.columns[i].values[r])));
}

/** `xppautX lecar.ode --silent`'s output.dat: rows of numbers as text, or null */
function silentRows(file) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'xppsilent'));
  fs.copyFileSync(file, path.join(dir, path.basename(file)));
  const r = spawnSync(native, [path.basename(file), '--silent'], {cwd: dir, stdio: 'ignore', timeout: 120_000});
  const rows = r.status === 0
    ? fs.readFileSync(path.join(dir, 'output.dat'), 'utf8').split('\n').filter(l => l.trim()).map(l => l.trim().split(/\s+/))
    : null;
  fs.rmSync(dir, {recursive: true, force: true});
  return rows;
}

/** what differs between the series and output.dat's rows (column name and row), or null */
function differsFrom(series, rows) {
  if (series.rows !== rows.length) return `${series.rows} rows, output.dat ${rows.length}`;
  for (const c of series.columns)
    for (let r = 0; r < rows.length; r++)
      /* output.dat holds the stored float32 printed with %.8g: the same exact value, rounded to 8 digits */
      if (Number(c.values[r].toPrecision(8)) !== Number(rows[r][c.col])) return `column ${c.name} row ${r}: ${c.values[r]} vs ${rows[r][c.col]}`;
  return null;
}

async function integrateLecar(port) {
  const client = await session(port('lecar.ode', fs.readFileSync(lecar, 'utf8')));
  await client.run({cmd: 'data', events: ['series']});
  await client.integrate();
  const series = seriesOf(client);
  client.close();
  return series;
}

const rows = silentRows(lecar);
check('the native --silent run wrote output.dat: 601 rows', rows !== null && rows.length === 601, String(rows?.length));

const fromWasm = await integrateLecar(wasmPort);
check('wasm: the integration sends the series: 601 rows of T, V and W',
  fromWasm?.rows === 601 && fromWasm.columns.map(c => c.name).join() === 'T,V,W',
  JSON.stringify(fromWasm && {rows: fromWasm.rows, names: fromWasm.columns.map(c => c.name)}));
const diff = fromWasm && rows ? differsFrom(fromWasm, rows) : 'no series';
check('wasm: the series equals the native --silent output.dat', diff === null, diff ?? '');

const fromNative = await integrateLecar(nativePort);
check('the native --server gives the module\'s series, bit for bit',
  sameSeries(fromWasm, fromNative));

/* Abort: a run of 1e7 time units would store 2e8 rows; the abort goes to the
   running job through shared memory while the core's thread computes */
{
  const client = await session(wasmPort('lecar.ode', fs.readFileSync(lecar, 'utf8')));
  await client.run({cmd: 'data', events: ['series']});
  await client.setTotal('1e7');
  await client.beginIntegration();
  await client.collect(ev => ev.ev === 'progress'); /* the run is under way */
  client.send({cmd: 'abort'});
  const evs = await client.collect(ev => ev.ev === 'idle');
  const stored = seriesOf(client)?.rows ?? -1;
  check('wasm: Abort stops a running integration (stopped, then idle)', evs.some(e => e.ev === 'stopped'), evs.map(e => e.ev).join());
  check('wasm: the aborted run stored far fewer rows than the run asked for', stored > 0 && stored < 2e8, String(stored));
  client.close();
}

/* a name that is not a base name never reaches the module's folder */
for (const bad of ['../x.ode', 'a/b.ode', 'a\\b.ode', '', '..', '.']) {
  let refused = false;
  try {
    xppCore.baseName(bad);
  } catch {
    refused = true;
  }
  check(`the glue refuses the file name ${JSON.stringify(bad)}`, refused);
}

if (bench) {
  const text = fs.readFileSync(path.join(root, 'examples', 'ode', 'kuramot100.odex'), 'utf8');
  // One warm-up, alternating order to reduce thermal bias.
  const variants = [['native', nativePort], ['wasm', wasmPort]];
  const samples = new Map(variants.map(([label]) => [label, []]));
  let reference;
  for (let round = 0; round <= measuredRounds; round++) {
    for (const [label, port] of round % 2 ? [...variants].reverse() : variants) {
      const client = await session(port('kuramot100.odex', text));
      await client.run({cmd: 'data', events: ['series']});
      const t0 = performance.now();
      await client.integrate();
      const elapsed = performance.now() - t0;
      const series = seriesOf(client);
      if (reference === undefined) reference = series;
      check(`kuramot100 ${label} round ${round}: identical complete series`,
        sameSeries(series, reference));
      if (round) samples.get(label).push(elapsed);
      console.log(`perf: kuramot100 ${label} ${elapsed.toFixed(0)} ms${round ? '' : ' (warm-up)'}`);
      client.close();
    }
  }
  for (const [label, times] of samples) {
    const sorted = [...times].sort((a, b) => a - b);
    const middle = Math.floor(sorted.length / 2);
    const median = sorted.length % 2 ? sorted[middle] : (sorted[middle - 1] + sorted[middle]) / 2;
    console.log(`perf: kuramot100 ${label} median ${median.toFixed(0)} ms (alternated samples: ${times.map(t => t.toFixed(0)).join(', ')})`);
  }
}

console.log(failures ? `${failures} FAILED` : 'all passed');
process.exit(failures ? 1 : 0);
