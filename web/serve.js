#!/usr/bin/env node
/* Standalone XPP in a browser: runs xppautX --server and serves the web front
   end. No dependencies.

   node web/serve.js [--port 8765] [--server "command"] file.ode [xppaut options]

   --server is the command that starts xppautX; the ODE file name is
   appended and it runs in the file's folder. Default: the xppautX
   built in this repository; on Windows xppautX.exe (make xppautx with
   MinGW) or else the Linux build through `wsl -e`. Events reach the page by
   Server-Sent Events, commands come back by POST. */
'use strict';
const http = require('http');
const fs = require('fs');
const path = require('path');
const {spawn} = require('child_process');

const args = process.argv.slice(2);
let port = 8765;
let serverCmd = null;
while (args.length && args[0].startsWith('--')) {
  const opt = args.shift();
  if (opt === '--port') port = Number(args.shift());
  else if (opt === '--server') serverCmd = args.shift();
  else {
    console.error('unknown option ' + opt);
    process.exit(2);
  }
}
if (!args.length) {
  console.error('usage: node web/serve.js [--port N] [--server CMD] file.ode [xppaut options]');
  process.exit(2);
}
const odePath = path.resolve(args.shift());
const repo = path.resolve(__dirname, '..');

function wslPath(p) {
  const m = /^([A-Za-z]):[\\/](.*)$/.exec(p);
  return m ? `/mnt/${m[1].toLowerCase()}/${m[2].replace(/\\/g, '/')}` : p.replace(/\\/g, '/');
}

let command, commandArgs;
if (serverCmd) {
  const parts = serverCmd.match(/"[^"]*"|\S+/g).map(s => s.replace(/^"|"$/g, ''));
  command = parts[0];
  commandArgs = parts.slice(1);
} else if (process.platform === 'win32' && fs.existsSync(path.join(repo, 'xppautX.exe'))) {
  command = path.join(repo, 'xppautX.exe');
  commandArgs = ['--server'];
} else if (process.platform === 'win32') {
  command = 'wsl';
  commandArgs = ['-e', wslPath(path.join(repo, 'xppautX')), '--server'];
} else {
  command = path.join(repo, 'xppautX');
  commandArgs = ['--server'];
}
commandArgs.push(path.basename(odePath), ...args);

const xpp = spawn(command, commandArgs, {cwd: path.dirname(odePath)});
console.log(`xppautX: ${command} ${commandArgs.join(' ')}`);

/* the events a newly opened page needs before it can draw */
const sticky = {hello: null, palette: null, state: null, ask: null};
const windows = new Map();
const clients = new Set();
let pending = '';

xpp.stdout.setEncoding('utf8');
xpp.stdout.on('data', chunk => {
  pending += chunk;
  let nl;
  while ((nl = pending.indexOf('\n')) >= 0) {
    const line = pending.slice(0, nl);
    pending = pending.slice(nl + 1);
    if (!line.trim()) continue;
    let ev;
    try {
      ev = JSON.parse(line);
    } catch (e) {
      console.error('bad line from server: ' + line.slice(0, 200));
      continue;
    }
    if (ev.ev in sticky) sticky[ev.ev] = line;
    if (ev.ev === 'idle') sticky.ask = null;
    if (ev.ev === 'window') {
      if (ev.op === 'create') windows.set(ev.win, line);
      if (ev.op === 'destroy') windows.delete(ev.win);
    }
    for (const res of clients) res.write(`data: ${line}\n\n`);
  }
});
/* what xppaut prints (parse errors among it) goes to the page as log events */
let logText = '';
let exitLine = null;
function broadcast(obj) {
  const line = JSON.stringify(obj);
  for (const res of clients) res.write(`data: ${line}\n\n`);
  return line;
}
xpp.stderr.setEncoding('utf8');
xpp.stderr.on('data', d => {
  process.stderr.write(d);
  logText = (logText + d).slice(-100000);
  broadcast({ev: 'log', text: d});
});
function serverGone(code) {
  if (exitLine) return;
  console.log(`xppautX exited (${code}); the page stays up to show why. Ctrl+C to stop.`);
  exitLine = broadcast({ev: 'exit', code});
}
xpp.on('exit', code => serverGone(code));
xpp.on('error', err => {
  const text = `could not start ${command}: ${err.message}\n`;
  logText += text;
  broadcast({ev: 'log', text});
  serverGone(-1);
});

function sendToServer(line) {
  if (!exitLine) xpp.stdin.write(line.trim() + '\n');
}

const files = {
  '/': ['index.html', 'text/html'],
  '/xpp-client.js': ['xpp-client.js', 'text/javascript'],
  '/xpp-client.css': ['xpp-client.css', 'text/css'],
};

/* the new front end, from web2/dist (npm run watch in web2 rebuilds it) */
const web2Types = {'.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8', '.woff2': 'font/woff2', '.txt': 'text/plain; charset=utf-8'};
function web2File(url) {
  const m = /^\/v2\/([\w.-]*)$/.exec(url);
  if (!m) return null;
  const file = path.join(repo, 'web2', 'dist', m[1] || 'index.html');
  return fs.existsSync(file) ? file : null;
}

http.createServer((req, res) => {
  const url = req.url.split('?')[0];
  const v2 = req.method === 'GET' && web2File(url);
  if (req.method === 'GET' && files[url]) {
    res.writeHead(200, {'Content-Type': files[url][1] + '; charset=utf-8', 'Cache-Control': 'no-store'});
    fs.createReadStream(path.join(__dirname, files[url][0])).pipe(res);
  } else if (v2) {
    res.writeHead(200, {'Content-Type': web2Types[path.extname(v2)] || 'application/octet-stream', 'Cache-Control': 'no-store'});
    fs.createReadStream(v2).pipe(res);
  } else if (req.method === 'GET' && url === '/events') {
    res.writeHead(200, {'Content-Type': 'text/event-stream', 'Cache-Control': 'no-store', Connection: 'keep-alive'});
    const replay = [sticky.hello, sticky.palette, ...windows.values(), sticky.state, sticky.ask];
    if (logText) replay.unshift(JSON.stringify({ev: 'log', text: logText}));
    if (exitLine) replay.push(exitLine);
    for (const line of replay) {
      if (line) res.write(`data: ${line}\n\n`);
    }
    clients.add(res);
    req.on('close', () => clients.delete(res));
    /* a redraw would wait behind an open prompt; the page shows the prompt */
    if (sticky.hello && !sticky.ask) sendToServer('{"cmd":"redraw"}');
  } else if (req.method === 'POST' && url === '/cmd') {
    let body = '';
    req.on('data', d => { body += d; });
    req.on('end', () => {
      try {
        if (JSON.parse(body).cmd === 'answer') sticky.ask = null;
        sendToServer(body);
        res.writeHead(204);
      } catch (e) {
        res.writeHead(400);
      }
      res.end();
    });
  } else {
    res.writeHead(404);
    res.end();
  }
}).listen(port, '127.0.0.1', () => {
  console.log(`XPP: http://127.0.0.1:${port}/`);
  console.log(`XPP (new interface, preview): http://127.0.0.1:${port}/v2/`);
});
