/* The proof-of-concept page (docs/wasm.md): loads a model into the
   WebAssembly core running in worker.js, integrates it and draws what
   comes back, with Abort. No framework: it exists to show the core runs
   in a browser, not to replace web2. */
import {LineTransport, WasmClient} from './client';

const $ = <T extends HTMLElement>(id: string): T => document.getElementById(id) as T;
const status = $('status'), log = $('log'), canvas = $<HTMLCanvasElement>('plot');
const run = $<HTMLButtonElement>('run'), long = $<HTMLButtonElement>('long'), abort = $<HTMLButtonElement>('abort');

function say(text: string): void {
  log.textContent += text + '\n';
  log.scrollTop = log.scrollHeight;
}

/* the module shares memory between its threads: the browser allows that
   only to a cross-origin isolated page (COOP and COEP, wasm/serve.mjs) */
const isolated = window.crossOriginIsolated;
status.textContent = isolated ? 'cross-origin isolated: shared memory is available'
  : 'not cross-origin isolated: SharedArrayBuffer is off, the core cannot start (serve with COOP and COEP)';
run.disabled = long.disabled = !isolated;

let client: WasmClient | null = null;
/* what tools/wasmpagecheck.mjs reads, never pixels: the data the page holds */
const probe = {isolated, integrated: false, stopped: false, rows: () => client?.state.plots.windows[0]?.series?.rows ?? 0,
  first: () => [...(client?.state.plots.windows[0]?.series?.columns.values() ?? [])].map(c => c[0])};
(window as unknown as {__xppWasm: typeof probe}).__xppWasm = probe;

/** the active window's first curve, scaled onto the canvas */
function draw(): void {
  if (!client) return;
  const w = client.state.plots.windows.find(x => x.win === client!.state.plots.active);
  const s = w?.series, c = s?.curves[0];
  const g = canvas.getContext('2d')!;
  g.clearRect(0, 0, canvas.width, canvas.height);
  const xs = s && c && s.columns.get(c.x), ys = s && c && s.columns.get(c.y);
  if (!s || !xs || !ys || s.rows < 2) return;
  const st = (col: number) => s.stats.get(col)!;
  const [sx, sy] = [st(c.x), st(c.y)];
  g.beginPath();
  for (let i = 0; i < s.rows; i++) {
    const px = ((xs[i] - sx.min) / (sx.max - sx.min || 1)) * (canvas.width - 20) + 10;
    const py = canvas.height - 10 - ((ys[i] - sy.min) / (sy.max - sy.min || 1)) * (canvas.height - 20);
    if (i) g.lineTo(px, py);
    else g.moveTo(px, py);
  }
  g.strokeStyle = getComputedStyle(document.body).color;
  g.stroke();
  status.textContent = `${s.names.get(c.y)} vs ${s.names.get(c.x)}: ${s.rows} rows`;
}

/** starts the core on a model of the page's own folder */
async function start(name: string): Promise<WasmClient> {
  const text = await (await fetch(`models/${name}`)).text();
  const worker = new Worker('worker.js');
  const c = new WasmClient(new LineTransport(onLine => {
    worker.onmessage = e => {
      if (e.data.type === 'line') onLine(e.data.line);
      else if (e.data.type === 'log') say(e.data.text);
      else if (e.data.type === 'error') say('error: ' + e.data.text);
    };
    worker.postMessage({type: 'start', files: {[name]: text}, args: ['--server', name], persistent: false});
    return {send: line => worker.postMessage({type: 'line', line}), close: () => worker.terminate()};
  }));
  c.onChange = () => requestAnimationFrame(draw);
  c.start();
  await c.started();
  await c.run({cmd: 'data', events: ['series']});
  return c;
}

run.onclick = async () => {
  run.disabled = long.disabled = true;
  client?.close();
  client = await start('lecar.ode');
  const t0 = performance.now();
  await client.integrate();
  probe.integrated = true;
  say(`lecar.ode integrated in ${(performance.now() - t0).toFixed(0)} ms`);
  run.disabled = long.disabled = false;
};

long.onclick = async () => {
  run.disabled = long.disabled = true;
  client?.close();
  client = await start('lecar.ode');
  await client.setTotal('1e7');
  await client.beginIntegration();
  abort.disabled = false;
  say('a run of 1e7 time units started: Abort stops it');
};

abort.onclick = async () => {
  if (!client) return;
  abort.disabled = true;
  client.send({cmd: 'abort'});
  const evs = await client.collect(ev => ev.ev === 'idle');
  probe.stopped = evs.some(e => e.ev === 'stopped');
  say(`stopped: ${probe.stopped}, rows stored: ${client.state.plots.windows[0]?.series?.rows}`);
  run.disabled = long.disabled = false;
};
