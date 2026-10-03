#!/usr/bin/env node
/* Serves build/wasm for the browser, with the two headers that make the
   page cross-origin isolated (COOP same-origin, COEP require-corp): the
   module's threads share memory, which a browser allows only then. Only
   127.0.0.1, only files of that folder by exact base name (no path,
   no query that matters). --no-isolation leaves the headers out, to show
   what the page says without them.
   Usage: node wasm/serve.mjs [--port N] [--no-isolation] */
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const root = path.join(path.dirname(fileURLToPath(import.meta.url)), '..', 'build', 'wasm');
const args = process.argv.slice(2);
const port = args.includes('--port') ? Number(args[args.indexOf('--port') + 1]) : 8766;
const isolate = !args.includes('--no-isolation');
const TYPES = {'.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.wasm': 'application/wasm', '.ode': 'text/plain'};

/** the file a request names, or null: only base names directly in the folder, or in models/ */
export function fileOf(url) {
  const m = /^\/(models\/)?([A-Za-z0-9_.-]+)(?:\?.*)?$/.exec(url);
  if (!m || m[2].startsWith('.')) return null;
  return path.join(root, m[1] ?? '', m[2]);
}

export function serve(listenPort, withHeaders = isolate) {
  const server = http.createServer((req, res) => {
    const file = req.method === 'GET' ? fileOf(req.url === '/' ? '/index.html' : req.url) : null;
    if (!file || !fs.existsSync(file) || !fs.statSync(file).isFile()) {
      res.writeHead(404).end('not found');
      return;
    }
    const headers = {'Content-Type': TYPES[path.extname(file)] ?? 'application/octet-stream', 'Cache-Control': 'no-store'};
    if (withHeaders) {
      headers['Cross-Origin-Opener-Policy'] = 'same-origin';
      headers['Cross-Origin-Embedder-Policy'] = 'require-corp';
    }
    res.writeHead(200, headers);
    fs.createReadStream(file).pipe(res);
  });
  return new Promise(ok => server.listen(listenPort, '127.0.0.1', () => ok(server)));
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  const s = await serve(port);
  console.log(`http://127.0.0.1:${s.address().port}/ ${isolate ? '(cross-origin isolated)' : '(no isolation headers)'}`);
}
