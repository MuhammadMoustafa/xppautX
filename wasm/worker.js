/* The page's Web Worker around the WebAssembly core (docs/wasm.md): the
   page posts {type: 'start', files, args, persistent} once, then {type:
   'line', line} for each command line, {type: 'save'} to keep the files
   (IDBFS); the worker posts {type: 'line', line} for each event line,
   {type: 'log', text}, {type: 'saved'} and {type: 'error', text}.
   This thread stays free while the core computes (the core runs on a
   pthread, core/xpp_wasm.cpp), so an abort line is acted on at once. */
importScripts('core.js', 'xppautx.js');

let core = null;

onmessage = e => {
  const m = e.data;
  try {
    if (m.type === 'start' && !core) {
      core = xppCore.startXpp(createXppautX, {
        files: m.files, args: m.args, persistent: m.persistent, script: 'xppautx.js',
        onLine: line => postMessage({type: 'line', line}),
        onLog: text => postMessage({type: 'log', text}),
      });
      core.ready.catch(err => postMessage({type: 'error', text: String(err)}));
    } else if (m.type === 'line' && core) core.send(m.line);
    else if (m.type === 'save' && core) core.save().then(() => postMessage({type: 'saved'}), err => postMessage({type: 'error', text: String(err)}));
  } catch (err) {
    postMessage({type: 'error', text: String(err)});
  }
};
